#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace juce;

//==============================================================================
AudioProcessorValueTreeState::ParameterLayout DoseLiteProcessor::createLayout()
{
    std::vector<std::unique_ptr<RangedAudioParameter>> p;

    auto f = [&] (const char* id, const char* name, float lo, float hi, float def, const char* unit = "", float skew = 1.0f)
    {
        p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name,
                                                            NormalisableRange<float> (lo, hi, 0.001f, skew), def,
                                                            AudioParameterFloatAttributes().withLabel (unit)));
    };

    f ("volume",    "Volume",     -24.0f, 12.0f, 0.0f, "dB");
    f ("tune",      "Tune",       0.0f, 1.0f, 0.0f);
    f ("speed",     "Speed",      0.0f, 200.0f, 30.0f, "ms", 0.5f);
    f ("shift",     "Shift",      -12.0f, 12.0f, 0.0f, "st");
    f ("warm",      "Warm",       0.0f, 1.0f, 0.2f);
    f ("presence",  "Presence",   0.0f, 1.0f, 0.3f);
    f ("air",       "Air",        0.0f, 1.0f, 0.3f);
    f ("gate",      "Gate",       0.0f, 1.0f, 0.2f);
    f ("comp",      "Comp",       0.0f, 1.0f, 0.4f);
    f ("deess",     "De-Ess",     0.0f, 1.0f, 0.4f);
    f ("drive",     "Drive",      0.0f, 1.0f, 0.0f);
    f ("doubler",   "Doubler",    0.0f, 1.0f, 0.0f);
    f ("delay",     "Delay",      0.0f, 1.0f, 0.1f);
    f ("delayTime", "Delay Time", 60.0f, 600.0f, 300.0f, "ms");
    f ("reverb",    "Reverb",     0.0f, 1.0f, 0.15f);
    f ("mix",       "Mix",        0.0f, 1.0f, 1.0f);

    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "key", 1 }, "Key",
                 StringArray { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }, 0));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "scale", 1 }, "Scale",
                 StringArray { "Chromatic", "Major", "Minor" }, 0));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { "phone", 1 }, "Phone", false));

    return { p.begin(), p.end() };
}

//==============================================================================
DoseLiteProcessor::DoseLiteProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", AudioChannelSet::stereo(), true)
                          .withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
    auto raw = [this] (const char* id) { return apvts.getRawParameterValue (id); };
    pVolume = raw ("volume");   pTune = raw ("tune");       pSpeed = raw ("speed");   pShift = raw ("shift");
    pWarm = raw ("warm");       pPresence = raw ("presence"); pAir = raw ("air");     pGate = raw ("gate");
    pComp = raw ("comp");       pDeess = raw ("deess");     pDrive = raw ("drive");   pDoubler = raw ("doubler");
    pDelay = raw ("delay");     pDelayTime = raw ("delayTime"); pReverb = raw ("reverb"); pMix = raw ("mix");
    pKey = raw ("key");         pScale = raw ("scale");     pPhone = raw ("phone");
}

bool DoseLiteProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in  = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();
    if (out != AudioChannelSet::mono() && out != AudioChannelSet::stereo()) return false;
    return in == AudioChannelSet::mono() || in == AudioChannelSet::stereo();
}

void DoseLiteProcessor::prepareToPlay (double sr, int block)
{
    fs = sr;
    tmpL.assign ((size_t) block, 0.0f);
    tmpR.assign ((size_t) block, 0.0f);
    tmpD.assign ((size_t) block, 0.0f);

    shifter.prepare (sr);
    latency = (int) (shifter.grain * 0.5f);
    setLatencySamples (latency);
    alignWet.prepare (latency + 8);
    alignDry.prepare (latency + 8);
    detector.prepare (sr);

    dbl.prepare ((int) (0.06 * sr));
    dlyL.prepare ((int) (0.65 * sr));
    dlyR.prepare ((int) (0.65 * sr));

    hp80.highpass (sr, 80.0);
    phoneHp.highpass (sr, 400.0);
    phoneLp.lowpass (sr, 3400.0);
    deHp.highpass (sr, 5500.0);
    for (auto* b : { &hp80, &lowShelf, &presencePk, &highShelf, &phoneHp, &phoneLp, &deHp }) b->reset();

    reverb.reset();
    reverb.setSampleRate (sr);

    gateEnv = 0; gateGain = 1; compEnv = 0; deEnv = 0;
    ratioCur = 1; targetRatio = 1; shiftMix = 0; hop = 0; lost = 100; lfoPhase = 0;

    volSmooth.reset (sr, 0.05);
    volSmooth.setCurrentAndTargetValue (Decibels::decibelsToGain (pVolume->load()));
}

// Tonhoehe -> Zielverhaeltnis der Korrektur
void DoseLiteProcessor::updateTarget (float freqHz, float tune, int key, int scale)
{
    if (freqHz > 0.0f && tune > 0.001f)
    {
        const float midi = 69.0f + 12.0f * std::log2 (freqHz / 440.0f);
        const float q = dl::quantizeMidi (midi, key, scale);
        targetRatio = std::exp2 ((q - midi) * tune / 12.0f);
        lost = 0;
    }
    else if (++lost > 6)
    {
        targetRatio = 1.0f; // kein Ton erkannt: nach kurzem Halten nicht mehr korrigieren
    }
}

//==============================================================================
void DoseLiteProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer&)
{
    ScopedNoDenormals noDenormals;

    const int n = buffer.getNumSamples();
    const int nIn = getTotalNumInputChannels();
    const int nOut = getTotalNumOutputChannels();
    if (n <= 0 || nIn <= 0) return;

    if ((int) tmpL.size() < n) { tmpL.resize ((size_t) n); tmpR.resize ((size_t) n); tmpD.resize ((size_t) n); }

    const float tune = pTune->load(), speedMs = pSpeed->load(), shiftSt = pShift->load();
    const float warm = pWarm->load(), presence = pPresence->load(), air = pAir->load();
    const float gate = pGate->load(), comp = pComp->load(), deess = pDeess->load(), drive = pDrive->load();
    const float doubler = pDoubler->load(), delayAmt = pDelay->load(), delayMs = pDelayTime->load();
    const float reverbAmt = pReverb->load(), mix = pMix->load(), volumeDb = pVolume->load();
    const bool phone = pPhone->load() > 0.5f;
    const int key = (int) pKey->load(), scale = (int) pScale->load();

    // Filter pro Block aktualisieren
    lowShelf.lowShelf (fs, 180.0, warm * 5.0);
    presencePk.peak (fs, 3500.0, 0.9, presence * 4.5);
    highShelf.highShelf (fs, std::min (10000.0, fs * 0.45), air * 8.0);

    // Koeffizienten
    const float gateThr = std::pow (10.0f, (-80.0f + gate * 50.0f) / 20.0f);
    const float envAtk = dl::coefMs (fs, 1.0), envRel = dl::coefMs (fs, 60.0);
    const float gAtk = dl::coefMs (fs, 3.0), gRel = dl::coefMs (fs, 100.0);
    const float cAtk = dl::coefMs (fs, 5.0), cRel = dl::coefMs (fs, 90.0);
    const float dAtk = dl::coefMs (fs, 0.5), dRel = dl::coefMs (fs, 30.0);
    const float tuneCoef = dl::coefMs (fs, (double) speedMs);
    const float mixCoef = dl::coefMs (fs, 20.0);

    const float compThr = -6.0f - comp * 24.0f;
    const float compRatio = 1.0f + comp * 5.0f;
    const float compMakeup = (-compThr) * (1.0f - 1.0f / compRatio) * 0.4f;
    const float shiftRatio = std::exp2 (shiftSt / 12.0f);
    const float shifterTarget = (tune > 0.001f || std::fabs (shiftSt) > 0.01f) ? 1.0f : 0.0f;
    const float driveK = 1.0f + drive * 8.0f;
    const float driveNorm = 1.0f / (driveK * 0.5f + 0.5f);
    const float driveWet = std::min (1.0f, drive * 2.0f);
    const float feedback = 0.38f;
    const float dtSamples = std::max (2.0f, delayMs * 0.001f * (float) fs);
    const double lfoInc = 2.0 * dl::kPi * 0.35 / fs;

    const float* in0 = buffer.getReadPointer (0);
    const float* in1 = nIn > 1 ? buffer.getReadPointer (1) : nullptr;

    for (int i = 0; i < n; ++i)
    {
        float x = in1 ? 0.5f * (in0[i] + in1[i]) : in0[i];
        alignDry.push (x);

        // ---- Gate + Low-Cut
        const float ax = std::fabs (x);
        gateEnv += (ax > gateEnv ? envAtk : envRel) * (ax - gateEnv);
        const float gTarget = (gate > 0.001f && gateEnv < gateThr) ? 0.0f : 1.0f;
        gateGain += (gTarget > gateGain ? gAtk : gRel) * (gTarget - gateGain);
        float y = hp80.process (x) * gateGain;

        // ---- Tonhoehenerkennung
        detector.push (y);
        if (++hop >= 256)
        {
            hop = 0;
            updateTarget (detector.detect(), tune, key, scale);
        }

        // ---- Autotune / Pitch-Shift
        ratioCur += (targetRatio - ratioCur) * tuneCoef;
        alignWet.push (y);
        const float shifted = shifter.process (y, ratioCur * shiftRatio);
        shiftMix += (shifterTarget - shiftMix) * mixCoef;
        y = alignWet.read (latency) * (1.0f - shiftMix) + shifted * shiftMix;

        // ---- Telefon-Stimme
        if (phone) y = phoneLp.process (phoneHp.process (y));

        // ---- EQ
        y = highShelf.process (presencePk.process (lowShelf.process (y)));

        // ---- De-Esser
        if (deess > 0.01f)
        {
            const float band = deHp.process (y);
            const float a = std::fabs (band);
            deEnv += (a > deEnv ? dAtk : dRel) * (a - deEnv);
            const float thr = 0.25f - 0.22f * deess;
            const float g = deEnv > thr ? thr / deEnv : 1.0f;
            y -= band * (1.0f - g);
        }

        // ---- Kompressor
        if (comp > 0.01f)
        {
            const float a = std::fabs (y);
            compEnv += (a > compEnv ? cAtk : cRel) * (a - compEnv);
            const float lvlDb = 20.0f * std::log10 (std::max (compEnv, 1.0e-5f));
            const float over = lvlDb - compThr;
            const float gr = over > 0.0f ? over * (1.0f - 1.0f / compRatio) : 0.0f;
            y *= dl::dbToGain (-gr + compMakeup);
        }

        // ---- Saettigung
        if (drive > 0.01f)
        {
            const float s = std::tanh (driveK * y) * driveNorm;
            y += (s - y) * driveWet;
        }

        // ---- Doubler (zwei modulierte Taps, links/rechts)
        dbl.push (y);
        lfoPhase += lfoInc;
        if (lfoPhase > 2.0 * dl::kPi) lfoPhase -= 2.0 * dl::kPi;
        const float m1 = (float) std::sin (lfoPhase) * 0.0015f * (float) fs;
        const float m2 = (float) std::sin (lfoPhase * 1.3 + 1.7) * 0.0015f * (float) fs;
        const float t1 = dbl.readFrac (0.017f * (float) fs + m1);
        const float t2 = dbl.readFrac (0.026f * (float) fs + m2);
        float L = y + doubler * 0.7f * t1;
        float R = y + doubler * 0.7f * t2;

        // ---- Ping-Pong-Delay
        const float oL = dlyL.readFrac (dtSamples), oR = dlyR.readFrac (dtSamples);
        dlyL.push (y + oR * feedback);
        dlyR.push (oL * feedback);
        L += oL * delayAmt * 0.8f;
        R += oR * delayAmt * 0.8f;

        tmpL[(size_t) i] = L;
        tmpR[(size_t) i] = R;
        tmpD[(size_t) i] = alignDry.read (latency);
    }

    // ---- Reverb ueber den ganzen Block
    Reverb::Parameters rp;
    rp.roomSize = 0.55f + 0.3f * reverbAmt;
    rp.damping = 0.45f;
    // juce::Reverb skaliert intern: dry x2, wet x3 -> 0.5 / 0.25 ergibt etwa Einheitspegel
    rp.wetLevel = reverbAmt * 0.25f;
    rp.dryLevel = 0.5f;
    rp.width = 1.0f;
    rp.freezeMode = 0.0f;
    reverb.setParameters (rp);
    reverb.processStereo (tmpL.data(), tmpR.data(), n);

    // ---- Mix + Lautstaerke, Ausgabe
    volSmooth.setTargetValue (Decibels::decibelsToGain (volumeDb));
    float* out0 = buffer.getWritePointer (0);
    float* out1 = nOut > 1 ? buffer.getWritePointer (1) : nullptr;

    for (int i = 0; i < n; ++i)
    {
        const float g = volSmooth.getNextValue();
        const float d = tmpD[(size_t) i] * (1.0f - mix);
        const float l = jlimit (-1.0f, 1.0f, (tmpL[(size_t) i] * mix + d) * g);
        const float r = jlimit (-1.0f, 1.0f, (tmpR[(size_t) i] * mix + d) * g);
        if (out1) { out0[i] = l; out1[i] = r; }
        else out0[i] = 0.5f * (l + r);
    }
}

//==============================================================================
String DoseLiteProcessor::runCommand (const String& text)
{
    return dl::cmd::run (apvts, cmdState, text);
}

AudioProcessorEditor* DoseLiteProcessor::createEditor() { return new DoseLiteEditor (*this); }

void DoseLiteProcessor::getStateInformation (MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary (*xml, dest);
}

void DoseLiteProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (ValueTree::fromXml (*xml));
}

AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new DoseLiteProcessor(); }
