# DoseLite

Ein eigenes Vocal-Plugin mit kompletter Gesangskette, das du per Texteingabe steuerst
("max autotune", "more reverb", "demon voice" ...). VST3 für Windows (Cakewalk, FL Studio ...),
AU für Mac (GarageBand, Logic).

Das ist ein **eigenes, originales Plugin**. Es ist kein Nachbau von VocalDose und enthält
keinen Code, keine Presets und keine Grafik davon.

## Was drin ist

Gate, Low-Cut, Autotune (mit Tonart-Wahl), Pitch-Shift für Charakterstimmen, Warm/Presence/Air-EQ,
De-Esser, Kompressor, Sättigung, Doubler, Ping-Pong-Delay, Reverb, Telefon-Filter.

## Ehrliche Grenzen

- **Autotune ist "lite".** Die Tonhöhenerkennung ist genau (im Test auf wenige Cent), aber der
  Pitch-Shifter ist einfach gebaut. Kleine Korrekturen klingen gut, extreme Effekte ("max autotune")
  klingen deutlich nach Effekt. An Antares Auto-Tune oder Melodyne kommt es nicht heran.
- **Keine Sprachsteuerung**, nur Tippen. Sprache wäre ein eigenes, großes Projekt.
- **Keine Artist-Presets.** "jhayco" und "bad bunny" laden nur meine eigenen groben Näherungen
  (dunkel-melodisch bzw. urban-latin), nicht die echten Ketten.
- **Latenz:** etwa 15 ms (wird dem Host gemeldet). Beim Aufnehmen lieber ohne Plugin aufnehmen
  und es beim Mixen nutzen.
- Getestet habe ich den Code auf Linux (Build, Tonhöhenerkennung, Autotune-Schleife, Befehle,
  Stabilität). **Auf Windows und Mac habe ich ihn nicht selbst ausgeführt**, und in Cakewalk oder
  GarageBand nicht gehört.

## Plugin bekommen, ohne selbst zu kompilieren (GitHub)

1. Kostenloses Konto auf github.com anlegen und ein neues, leeres Repository erstellen.
2. Den Inhalt dieses Ordners hochladen (Add file → Upload files; der Ordner `.github` muss mit).
3. Reiter **Actions** öffnen. Der Build startet automatisch (ca. 10 bis 15 Minuten).
4. Wenn er grün ist: auf den Durchlauf klicken, unten bei **Artifacts** `DoseLite-Windows`
   oder `DoseLite-macOS` herunterladen.

## Installieren

**Windows (Cakewalk, FL Studio ...)**
1. `DoseLite.vst3` nach `C:\Program Files\Common Files\VST3\` kopieren.
2. Cakewalk: Edit → Preferences → Audio → Plug-in Manager → **Scan**.
3. Auf der Spur: FX-Bin → Audio FX → VST3 → DoseLite.

**Mac (GarageBand, Logic)**
1. `DoseLite.component` nach `/Library/Audio/Plug-Ins/Components/` kopieren.
2. Falls macOS meckert (nicht signiert): im Terminal
   `xattr -cr /Library/Audio/Plug-Ins/Components/DoseLite.component`
3. GarageBand neu starten, Spur wählen → Plug-ins → Audio Units → DoseLite.

Hinweis: `DoseLite.vst3` ist ein Ordner. Kopiere immer den ganzen Ordner.

## Selbst kompilieren

Voraussetzungen: CMake 3.22+, ein C++17-Compiler (Visual Studio 2022 oder Xcode).

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

Die Plugins liegen danach in `build/DoseLite_artefacts/Release/`. JUCE wird beim ersten Mal automatisch geladen.
JUCE steht unter AGPLv3 oder einer kommerziellen Lizenz. Für den privaten Gebrauch ist das unproblematisch.
Wenn du das Plugin veröffentlichen oder verkaufen willst, prüfe vorher die JUCE-Lizenzbedingungen.

## Befehle (oben ins Feld tippen, Enter)

| Befehl | Wirkung |
|---|---|
| `make my mix good` | Startkette "Clean" |
| `another one` | nächste von 5 Startketten (Clean, Warm Ballad, Bright Pop, Hard Tune, Dark Melodic) |
| `max autotune` / `hard autotune` | Tune 100 %, Speed 0 ms (harter Effekt) |
| `natural autotune` | Tune 30 %, weich |
| `less autotune` / `more autotune` / `no autotune` | Tune ±0,2 / aus |
| `more reverb`, `less delay`, `more air`, `warmth`, `more compression`, `more drive`, `wider` | Regler anpassen |
| `louder` / `quieter` | Lautstärke ±3 dB |
| `demon voice`, `ghost`, `telephone`, `baby`, `deep` | Charakterstimmen |
| `adlib` | Adlib-Kette (viel Delay und Doubler) |
| `jhayco`, `bad bunny` | meine Näherung: dunkel-melodisch bzw. urban-latin |
| `key of F# minor`, `tune to A major`, `key of e flat major` | Tonart für den Autotune |
| `surprise me` | Zufallseinstellung |
| `reset` | zurück auf Standard |

Auch einige deutsche Wörter gehen (`mehr hall`, `kein autotune`, `lauter`, `weniger delay`).
Unter dem Eingabefeld steht, was geändert wurde. Alle Regler kannst du auch von Hand drehen.

## Tipp zum Autotune

Stell zuerst die richtige **Tonart** ein (`key of C# minor`). Wenn du sie nicht kennst, lass die Skala auf
*Chromatic*. Dann rastet der Ton auf den nächsten Halbton, was für den typischen Effekt reicht.

## Aufbau

- `Source/Dsp.h`: Filter, Delay, Pitch-Shifter, Tonhöhenerkennung
- `Source/CommandParser.h`: Befehle, Presets
- `Source/PluginProcessor.cpp`: Signalkette und Parameter
- `Source/PluginEditor.cpp`: Oberfläche
