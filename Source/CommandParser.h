#pragma once
// Befehls-Parser: getippte Saetze ("max autotune", "more reverb", "demon voice", "key of F# minor")
// werden in Parameteraenderungen uebersetzt. Englisch und einige deutsche/spanische Woerter.
#include <juce_audio_processors/juce_audio_processors.h>
#include <regex>
#include <vector>

namespace dl::cmd
{
using APVTS = juce::AudioProcessorValueTreeState;

struct State { int starter = 0; };

inline float get (APVTS& a, const char* id) { return a.getRawParameterValue (id)->load(); }

inline void set (APVTS& a, const char* id, float v)
{
    if (auto* p = a.getParameter (id))
        p->setValueNotifyingHost (p->convertTo0to1 (v));
}

struct Setting { const char* id; float v; };
struct Preset  { const char* name; std::vector<Setting> s; };

// alles auf Standard, ausser Tonart, Skala, Lautstaerke und Mix
inline void resetMix (APVTS& a)
{
    for (auto* p : a.processor.getParameters())
        if (auto* pid = dynamic_cast<juce::AudioProcessorParameterWithID*> (p))
        {
            const auto id = pid->paramID;
            if (id == "key" || id == "scale" || id == "volume" || id == "mix") continue;
            p->setValueNotifyingHost (p->getDefaultValue());
        }
}

inline void applyPreset (APVTS& a, const Preset& pr)
{
    resetMix (a);
    for (auto& s : pr.s) set (a, s.id, s.v);
}

// Eigene Einstellungen (keine Kopien echter Artist-Ketten, nur grobe Stilrichtungen)
inline const std::vector<Preset>& starters()
{
    static const std::vector<Preset> v = {
        { "Clean",        { { "gate", .30f }, { "tune", .35f }, { "speed", 40 }, { "warm", .30f }, { "presence", .40f }, { "air", .35f },
                            { "comp", .45f }, { "deess", .40f }, { "drive", .10f }, { "doubler", .15f }, { "delay", .12f }, { "reverb", .20f } } },
        { "Warm Ballad",  { { "gate", .20f }, { "tune", .40f }, { "speed", 60 }, { "warm", .60f }, { "presence", .30f }, { "air", .25f },
                            { "comp", .40f }, { "deess", .35f }, { "drive", .20f }, { "doubler", .10f }, { "delay", .10f }, { "reverb", .35f } } },
        { "Bright Pop",   { { "gate", .25f }, { "tune", .50f }, { "speed", 25 }, { "warm", .15f }, { "presence", .60f }, { "air", .60f },
                            { "comp", .50f }, { "deess", .50f }, { "drive", .10f }, { "doubler", .30f }, { "delay", .15f }, { "reverb", .25f } } },
        { "Hard Tune",    { { "gate", .30f }, { "tune", 1.0f }, { "speed", 0 }, { "warm", .30f }, { "presence", .50f }, { "air", .40f },
                            { "comp", .65f }, { "deess", .50f }, { "drive", .35f }, { "doubler", .25f }, { "delay", .25f }, { "reverb", .30f } } },
        { "Dark Melodic", { { "gate", .30f }, { "tune", .85f }, { "speed", 8 }, { "warm", .55f }, { "presence", .35f }, { "air", .45f },
                            { "comp", .60f }, { "deess", .55f }, { "drive", .25f }, { "doubler", .30f }, { "delay", .30f }, { "delayTime", 340 }, { "reverb", .40f } } },
    };
    return v;
}

inline const Preset& urbanLatin()
{
    static const Preset p { "Urban Latin", { { "gate", .30f }, { "tune", .70f }, { "speed", 15 }, { "warm", .40f }, { "presence", .55f }, { "air", .50f },
                                             { "comp", .60f }, { "deess", .50f }, { "drive", .30f }, { "doubler", .35f }, { "delay", .20f },
                                             { "delayTime", 260 }, { "reverb", .45f } } };
    return p;
}

inline const Preset& adlib()
{
    static const Preset p { "Adlib", { { "gate", .30f }, { "tune", .60f }, { "speed", 20 }, { "presence", .50f }, { "air", .50f }, { "comp", .50f },
                                       { "deess", .40f }, { "drive", .20f }, { "doubler", .50f }, { "delay", .50f }, { "delayTime", 220 }, { "reverb", .55f } } };
    return p;
}

//==============================================================================
inline juce::StringArray tokens (const juce::String& t)
{
    juce::StringArray out;
    juce::String cur;
    for (auto c : t)
    {
        if (juce::CharacterFunctions::isLetterOrDigit (c) || c == '#') cur += juce::String::charToString (c);
        else { if (cur.isNotEmpty()) out.add (cur); cur.clear(); }
    }
    if (cur.isNotEmpty()) out.add (cur);
    return out;
}

inline bool hasTok (const juce::StringArray& t, std::initializer_list<const char*> words)
{
    for (auto* w : words) if (t.contains (w)) return true;
    return false;
}

inline bool hasPrefix (const juce::StringArray& t, std::initializer_list<const char*> words)
{
    for (auto& tok : t)
        for (auto* w : words)
            if (tok.startsWith (w)) return true;
    return false;
}

struct Target
{
    const char* id;
    const char* label;
    std::vector<const char*> words; // Praefixe der Woerter
    float step, lo, hi, mid;                  // mid: Wert, wenn nur das Stichwort genannt wird (-999 = unveraendert)
};

inline const std::vector<Target>& targets()
{
    static const std::vector<Target> v = {
        { "tune",     "Autotune",   { "autotune", "tune", "tuning", "tuned", "pitch" },                0.2f,  0, 1, 0.6f },
        { "reverb",   "Reverb",     { "reverb", "hall", "space", "spacy" },                            0.2f,  0, 1, 0.6f },
        { "delay",    "Delay",      { "delay", "echo" },                                               0.2f,  0, 1, 0.5f },
        { "air",      "Air",        { "air", "bright", "crisp", "heller", "sparkle" },                 0.2f,  0, 1, 0.6f },
        { "warm",     "Warmth",     { "warm", "waerm", "wärm", "fat" },                        0.2f,  0, 1, 0.6f },
        { "presence", "Presence",   { "presence", "clarity", "clear", "forward" },                     0.2f,  0, 1, 0.6f },
        { "comp",     "Compressor", { "comp", "squash", "punch" },                                     0.2f,  0, 1, 0.6f },
        { "deess",    "De-esser",   { "deess", "ess", "sibil", "harsh", "tame" },                      0.2f,  0, 1, 0.6f },
        { "drive",    "Drive",      { "drive", "saturat", "distort", "dirty", "crunch", "grit" },      0.2f,  0, 1, 0.5f },
        { "doubler",  "Doubler",    { "doubl", "wide", "thick", "stack" },                             0.2f,  0, 1, 0.5f },
        { "gate",     "Gate",       { "gate", "noise", "rausch" },                                     0.2f,  0, 1, 0.6f },
        { "volume",   "Volume",     { "volume", "louder", "lauter", "quieter", "leiser" },             3.0f, -24, 12, -999.0f },
    };
    return v;
}

inline const char* keyNames[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

inline juce::String fmt (float v) { return juce::String (v, 2); }

//==============================================================================
// Hauptfunktion: gibt eine lesbare Zusammenfassung der Aenderungen zurueck
inline juce::String run (APVTS& a, State& st, const juce::String& raw)
{
    const juce::String text = raw.toLowerCase().trim();
    if (text.isEmpty()) return "Type something, e.g. \"make my mix good\" or \"max autotune\".";

    const auto tk = tokens (text);
    juce::StringArray log;
    bool handled = false;

    // ---- Reset
    if (hasTok (tk, { "reset", "default", "zurueck" }))
    {
        resetMix (a);
        return "Reset to default.";
    }

    // ---- Presets / Charakterstimmen
    auto usePreset = [&] (const Preset& p, const juce::String& note = {})
    {
        applyPreset (a, p);
        log.add ("Preset: " + juce::String (p.name) + note);
        handled = true;
    };

    if (text.contains ("another one") || text.contains ("next preset") || text.contains ("noch eins") || hasTok (tk, { "next", "weiter" }))
    {
        st.starter = (st.starter + 1) % (int) starters().size();
        usePreset (starters()[(size_t) st.starter]);
    }
    else if (text.contains ("make my mix good") || text.contains ("mix good") || text.contains ("make me a preset")
             || text.contains ("gute mischung") || text.contains ("mach mir einen mix"))
    {
        st.starter = 0;
        usePreset (starters()[0]);
    }
    else if (text.contains ("jhay") || text.contains ("dark melodic") || text.contains ("melodic"))
        usePreset (starters()[4], " (my own approximation, not the real chain)");
    else if (text.contains ("bunny") || text.contains ("reggaeton") || text.contains ("latin"))
        usePreset (urbanLatin(), " (my own approximation, not the real chain)");
    else if (text.contains ("adlib") || text.contains ("ad-lib") || text.contains ("ad lib"))
        usePreset (adlib());
    else if (hasPrefix (tk, { "demon" }))
    {
        resetMix (a);
        set (a, "shift", -6); set (a, "drive", .45f); set (a, "warm", .6f); set (a, "reverb", .3f); set (a, "comp", .5f);
        log.add ("Voice: demon"); handled = true;
    }
    else if (hasPrefix (tk, { "ghost" }))
    {
        resetMix (a);
        set (a, "reverb", .85f); set (a, "delay", .5f); set (a, "air", .8f); set (a, "doubler", .5f); set (a, "delayTime", 420);
        log.add ("Voice: ghost"); handled = true;
    }
    else if (hasPrefix (tk, { "telephone", "phone" }))
    {
        resetMix (a);
        set (a, "phone", 1); set (a, "drive", .3f); set (a, "comp", .6f);
        log.add ("Voice: telephone"); handled = true;
    }
    else if (hasPrefix (tk, { "baby" }))
    {
        resetMix (a);
        set (a, "shift", 7); set (a, "air", .5f); set (a, "reverb", .2f);
        log.add ("Voice: baby"); handled = true;
    }
    else if (hasTok (tk, { "deep" }))
    {
        resetMix (a);
        set (a, "shift", -4); set (a, "warm", .7f); set (a, "comp", .5f); set (a, "reverb", .2f);
        log.add ("Voice: deep"); handled = true;
    }
    else if (text.contains ("surprise"))
    {
        juce::Random r;
        resetMix (a);
        set (a, "tune", .3f + r.nextFloat() * .7f);
        set (a, "speed", r.nextFloat() * 60.0f);
        for (auto* id : { "warm", "presence", "air", "comp", "deess", "drive", "doubler", "delay", "reverb" })
            set (a, id, r.nextFloat() * .7f);
        log.add ("Surprise: random settings"); handled = true;
    }

    // ---- Tonart: "key of F# minor", "tune to a major"
    bool keyMatched = false;
    {
        static const std::regex re1 (R"(\bkey\s+(?:of\s+)?([a-g])(#|\s*sharp|\s*flat)?\s*(major|minor)?(?![a-z#]))");
        static const std::regex re2 (R"(\b([a-g])(#|\s*sharp|\s*flat)?\s+(major|minor)(?![a-z]))");
        std::smatch m;
        const std::string s = text.toStdString();
        if (std::regex_search (s, m, re1) || std::regex_search (s, m, re2))
        {
            static const int base[7] = { 9, 11, 0, 2, 4, 5, 7 }; // a b c d e f g
            int pc = base[m[1].str()[0] - 'a'];
            const std::string acc = m[2].str();
            if (acc.find ('#') != std::string::npos || acc.find ("sharp") != std::string::npos) pc += 1;
            else if (acc.find ("flat") != std::string::npos) pc -= 1;
            pc = (pc + 12) % 12;

            int scale = (int) get (a, "scale");
            const std::string sc = m[3].str();
            if (sc == "major") scale = 1;
            else if (sc == "minor") scale = 2;
            else if (scale == 0) scale = 1;

            set (a, "key", (float) pc);
            set (a, "scale", (float) scale);
            if (get (a, "tune") < 0.2f) set (a, "tune", 0.6f);
            log.add (juce::String ("Key: ") + keyNames[pc] + (scale == 2 ? " minor" : " major"));
            handled = true;
            keyMatched = true;
        }
        else if (hasTok (tk, { "chromatic" }))
        {
            set (a, "scale", 0);
            log.add ("Scale: chromatic");
            handled = true;
        }
    }

    // ---- "mehr/weniger X", "max X", "kein X"
    const bool off  = hasTok (tk, { "no", "off", "kein", "keine", "without", "sin", "disable" });
    const bool less = hasTok (tk, { "less", "weniger", "reduce", "down", "lower", "softer", "minus", "menos", "cut", "remove", "quieter", "leiser" });
    const bool more = hasTok (tk, { "more", "mehr", "increase", "up", "higher", "extra", "stronger", "mas", "boost", "add", "louder", "lauter" });
    const bool maxw = hasTok (tk, { "max", "maximum", "full", "extreme", "hard", "hardest", "robot", "robotic" });
    const bool nat  = hasTok (tk, { "natural", "subtle", "gentle", "natuerlich" });

    for (auto& t : targets())
    {
        bool hit = false;
        for (auto* w : t.words) if (hasPrefix (tk, { w })) { hit = true; break; }
        if (! hit) continue;

        const juce::String id (t.id);
        // "tune to F# minor" ist eine Tonart-Angabe, kein Autotune-Regler
        if (keyMatched && id == "tune" && ! hasPrefix (tk, { "autotune", "tuning", "pitch" })) continue;
        const float cur = get (a, t.id);
        float nv = cur;

        if (off)       nv = t.lo;
        else if (maxw) nv = t.hi;
        else if (nat && id == "tune") nv = 0.3f;
        else if (more) nv = cur + t.step;
        else if (less) nv = cur - t.step;
        else if (t.mid > -900.0f) nv = juce::jmax (cur, t.mid);

        nv = juce::jlimit (t.lo, t.hi, nv);

        if (id == "tune" && (maxw || nat))
        {
            set (a, "speed", maxw ? 0.0f : 50.0f);
            if (maxw && get (a, "scale") == 0) { /* chromatisch bleibt moeglich */ }
        }

        if (std::fabs (nv - cur) > 0.0001f || (id == "tune" && (maxw || nat)))
        {
            set (a, t.id, nv);
            log.add (juce::String (t.label) + ": " + fmt (cur) + " -> " + fmt (nv));
            handled = true;
        }
        else
        {
            log.add (juce::String (t.label) + ": already " + fmt (cur));
            handled = true;
        }
    }

    if (! handled)
        return "Didn't get that. Try: make my mix good, another one, max autotune, less autotune, more reverb, "
               "demon voice, ghost, telephone, adlib, key of F# minor, reset.";

    return log.joinIntoString ("\n");
}
} // namespace dl::cmd
