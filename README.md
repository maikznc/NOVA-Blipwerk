# NOVA · Blipwerk

Blip-Synth mit eingebautem 16-Step-Sequencer, bedient wie ein Arturia BeatStep (Original).
Gebaut mit JUCE 8 als **AU**, **VST3** und **Standalone** für macOS (Universal) und Windows.

**Installation:** siehe [INSTALLATION.md](INSTALLATION.md).

## Klang

| Sektion | Regler |
|---|---|
| OSC 1 | Wave (Sinus, Dreieck, Säge, Rechteck, Puls), Octave, FM Ratio, FM Amount |
| OSC 2 | Wave, Tune (±24 Halbtöne), Detune, Level, Mode (MIX/RING) |
| BLIP | Pitch-Sprung im Anschlag (Amount, P Decay), FM Decay, Click |
| FILTER | LP/BP/HP, Cutoff, Resonanz, Hüllkurve, F Decay |
| AMP | Decay, Drive, Crush |
| MOTION | LFO im Tempo auf Cutoff/FM/Notenlänge, Random pro Note |
| ECHO · SPACE | Ping-Pong-Echo im Tempo (1/16 bis 3/8), Feedback, Mix, Damp, Raum |

19 Presets, von „Bubble“ über „Ring Bleep“ bis „Deep Sonar“. Die DAW sieht sie als Programme.

## Sequencer

- 16 Encoder (Tonhöhe pro Step, 4 Oktaven, gerastet auf die Skala), 16 Pads (Step an/aus), großer Regler (Tempo, mit SHIFT Transpose)
- SHIFT + Pad: 8 Skalen, Richtung (FWD/REV/ALT/RND), Raster (1/4 bis 1/32)
- SHIFT + Encoder 1–3: Gate, Swing, Legato · SHIFT + CHAN + Pad: Länge
- RECALL / STORE: 16 Speicherplätze · RAND: Zufallsmuster (SHIFT + RAND nur Noten)
- Folgt Tempo und Position der DAW, läuft bei gestoppter DAW frei über PLAY
- MIDI-Eingang spielt den Synth direkt

## Bauen

```
cmake -B build -DCMAKE_BUILD_TYPE=Release            # Mac: zusätzlich -G Xcode möglich
cmake --build build --config Release
```

Optionen: `-DNOVA_COPY_AFTER_BUILD=ON` kopiert die Plugins nach dem Bauen in die Plugin-Ordner. `-DNOVA_BUILD_TESTS=ON` baut `NovaTests` (rendert alle Presets, prüft DAW-Sync, Speichern/Laden und zeichnet die Oberfläche als PNG).

## Automatische Builds

Jeder Push auf `main` baut über GitHub Actions (`.github/workflows/build.yml`):

- **macOS**: Universal (Apple Silicon + Intel), AU + VST3 + App, ad-hoc signiert, mit `auval`-Prüfung → `NovaBlipwerk-macOS.zip`
- **Windows**: VST3 + App → `NovaBlipwerk-Windows.zip`
- **Linux**: Headless-Tests

Die ZIPs liegen danach im Actions-Lauf unter „Artifacts“. Ein Tag wie `v1.0.0` erzeugt zusätzlich ein GitHub-Release mit beiden Dateien.

## Aufbau

```
Source/Params.h          Parameter, Wertebereiche, Presets, Skalen
Source/SynthEngine.*     Voices, Filter, Hüllkurven, FX-Bus (Drive, Crush, Echo, Raum, Limiter)
Source/Sequencer.h       Sequenz-Daten, Beispiel-Patterns, thread-sicherer Live-Zustand
Source/PluginProcessor.* Audio-Thread: DAW-Sync, Step-Timing mit Swing, MIDI, Zustand speichern
Source/PluginEditor.*    Oberfläche: Display, Module, Step-Feld
Source/UI.*              Farben, Schriften, Knobs, Pads, Buttons, LCD
Resources/Fonts          Michroma, Barlow Semi Condensed, IBM Plex Mono (SIL Open Font License)
packaging/macos          Installer-Script
Tests/NovaTests.cpp      Headless-Tests
```

Schriften: Michroma, Barlow Semi Condensed und IBM Plex Mono stehen unter der SIL Open Font License 1.1, die Lizenztexte liegen in `Resources/Fonts`.
