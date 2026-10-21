#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "CommandParser.h"
#include "Dsp.h"

class DoseLiteProcessor : public juce::AudioProcessor
{
public:
    DoseLiteProcessor();
    ~DoseLiteProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "DoseLite"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 3.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // Textbefehl ausfuehren, Rueckgabe = Zusammenfassung der Aenderungen
    juce::String runCommand (const juce::String& text);

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    juce::AudioProcessorValueTreeState apvts;

private:
    void updateTarget (float freqHz, float tune, int key, int scale);

    dl::cmd::State cmdState;

    // Parameter-Zeiger
    std::atomic<float> *pVolume, *pTune, *pSpeed, *pShift, *pWarm, *pPresence, *pAir, *pGate, *pComp, *pDeess,
        *pDrive, *pDoubler, *pDelay, *pDelayTime, *pReverb, *pMix, *pKey, *pScale, *pPhone;

    double fs = 44100.0;
    int latency = 0;

    // Bausteine
    dl::Biquad hp80, lowShelf, presencePk, highShelf, phoneHp, phoneLp, deHp;
    dl::DelayLine alignWet, alignDry, dbl, dlyL, dlyR;
    dl::PitchShifter shifter;
    dl::PitchDetector detector;
    juce::Reverb reverb;
    juce::SmoothedValue<float> volSmooth;

    // Zustaende
    float gateEnv = 0, gateGain = 1, compEnv = 0, deEnv = 0;
    float ratioCur = 1, targetRatio = 1, shiftMix = 0;
    int hop = 0, lost = 100;
    double lfoPhase = 0;

    std::vector<float> tmpL, tmpR, tmpD;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DoseLiteProcessor)
};
