# Nova Blipwerk installieren

Nova Blipwerk ist ein Blip-Synth mit eingebautem 16-Step-Sequencer.
Er läuft als **Audio Unit** (Logic, GarageBand, Ableton auf dem Mac), als **VST3** (Ableton, Bitwig, Cubase, Reaper, FL Studio und andere) und als eigenständige App.

Im Paket für den Mac liegen:

| Datei | Wofür |
|---|---|
| `Nova Blipwerk.component` | Audio Unit (AU) |
| `Nova Blipwerk.vst3` | VST3 |
| `Nova Blipwerk.app` | Standalone-App, ohne DAW |
| `Installieren.command` | Script, das alles installiert |

Das Mac-Paket ist **Universal**: es läuft nativ auf Apple Silicon (M1–M4) und auf Intel-Macs. Mindestens macOS 11 Big Sur.

---

## macOS: schnell mit einem Terminal-Befehl

1. **ZIP entpacken** (Doppelklick). Wenn du die Datei von GitHub Actions geladen hast, steckt darin noch einmal `NovaBlipwerk-macOS.zip`. Die auch per Doppelklick entpacken. Danach hast du einen Ordner **Nova Blipwerk**.
2. **Terminal öffnen**: `Cmd` + `Leertaste`, „Terminal“ tippen, Enter.
3. Folgendes tippen, **mit einem Leerzeichen am Ende**, aber noch nicht Enter drücken:

   ```
   bash 
   ```

4. Die Datei **Installieren.command** aus dem Finder ins Terminal-Fenster ziehen. Der Pfad wird automatisch eingefügt. Jetzt **Enter** drücken.

   Liegt der Ordner in „Downloads“, geht auch direkt:

   ```
   bash ~/Downloads/"Nova Blipwerk"/Installieren.command
   ```

5. Das Script kopiert die Plugins, entfernt die Quarantäne-Markierung von macOS und prüft die Audio Unit. Am Ende steht **„Audio Unit geprüft: OK“**.
6. **DAW neu starten** (oder Plugins neu scannen). Das Plugin heißt **Nova Blipwerk**, Hersteller **Nova**.

> **Warum über das Terminal?** Das Plugin ist nicht bei Apple notarisiert, dafür bräuchte es einen kostenpflichtigen Apple-Developer-Account. macOS markiert heruntergeladene Dateien mit einem Quarantäne-Flag und blockiert nicht notarisierte Plugins dann beim Laden. Die DAW zeigt das Plugin nicht an oder meldet, es sei „beschädigt“. Ein Doppelklick auf das Script wird aus demselben Grund blockiert, deshalb der Start mit `bash`. Das Script ändert nichts am System, es kopiert nur die Dateien und entfernt dieses eine Flag.

---

## macOS: manuell, Schritt für Schritt

Alle Befehle im Terminal ausführen, und zwar **im entpackten Ordner**. Dorthin wechselst du mit `cd ` (Leerzeichen) und dem Ordner aus dem Finder ins Terminal gezogen, dann Enter.

**1. Plugins kopieren**

```
mkdir -p ~/Library/Audio/Plug-Ins/Components ~/Library/Audio/Plug-Ins/VST3
cp -R "Nova Blipwerk.component" ~/Library/Audio/Plug-Ins/Components/
cp -R "Nova Blipwerk.vst3" ~/Library/Audio/Plug-Ins/VST3/
```

Ohne Terminal geht das auch im Finder: `Gehe zu` → `Gehe zum Ordner …` (`Cmd` + `Shift` + `G`), `~/Library/Audio/Plug-Ins/Components` einfügen und die `.component`-Datei hineinziehen, genauso mit `~/Library/Audio/Plug-Ins/VST3` und der `.vst3`-Datei.

**2. Quarantäne-Flag entfernen** (das ist der wichtige Schritt)

```
xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/Components/"Nova Blipwerk.component"
xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/"Nova Blipwerk.vst3"
```

**3. Audio-Unit-Cache neu laden und prüfen** (für Logic, GarageBand und andere AU-Hosts)

```
killall -9 AudioComponentRegistrar
auval -v aumu Nblw Nova
```

Ganz unten muss **`AU VALIDATION SUCCEEDED`** stehen. Die Fehlermeldung von `killall` („No matching processes“) kannst du ignorieren.

**4. Standalone-App (optional)**

`Nova Blipwerk.app` in den Programme-Ordner ziehen, dann:

```
xattr -dr com.apple.quarantine "/Applications/Nova Blipwerk.app"
```

---

## In der DAW finden

- **Logic Pro**: Neue Software-Instrument-Spur → Instrument-Slot → `AU-Instrumente` → `Nova` → `Nova Blipwerk`.
  Wird es nicht angezeigt: `Logic Pro` → `Einstellungen` → `Plug-in-Manager`, Nova Blipwerk auswählen → `Zurücksetzen & Neu scannen`.
- **Ableton Live**: `Einstellungen` → `Plug-ins` → „Audio Units verwenden“ bzw. „VST3-Plug-in-Systemordner verwenden“ einschalten → `Neu scannen`. Danach im Browser unter `Plug-ins` → `Nova`.
- **Bitwig, Reaper, Cubase**: Plugin-Ordner neu scannen; VST3 liegt im Standardordner.

---

## Wenn es nicht klappt

| Problem | Lösung |
|---|---|
| „… ist beschädigt und kann nicht geöffnet werden“ | Schritt 2 (Quarantäne-Flag) ausführen. |
| Plugin taucht in Logic nicht auf | Schritt 3 ausführen, Logic neu starten, im Plug-in-Manager neu scannen. |
| `auval` findet nichts | Prüfen, ob die Datei wirklich in `~/Library/Audio/Plug-Ins/Components` liegt (die Tilde `~` ist dein Benutzerordner, nicht `/Library`). |
| „bash: … No such file or directory“ | Pfad stimmt nicht. Datei ins Terminal ziehen statt tippen; Leerzeichen im Namen brauchen Anführungszeichen. |
| Sequencer läuft nicht | In der DAW Play drücken, der Sequencer folgt dem Transport. Ist die DAW gestoppt, startet PLAY im Plugin ihn frei. Steht im Display „AUS“, einmal PLAY drücken. |

**Deinstallieren**: `Nova Blipwerk.component` und `Nova Blipwerk.vst3` aus den beiden Ordnern löschen.

---

## Windows

1. ZIP entpacken.
2. Den Ordner **`Nova Blipwerk.vst3`** nach `C:\Program Files\Common Files\VST3\` kopieren und die Admin-Abfrage bestätigen.
3. In der DAW die Plugins neu scannen.

Die Standalone-App `Nova Blipwerk.exe` ist nicht signiert. Meldet Windows SmartScreen „Der Computer wurde durch Windows geschützt“, auf `Weitere Informationen` → `Trotzdem ausführen` klicken.

---

## Bedienung in der DAW

- Der Sequencer folgt **Tempo und Position der DAW**. Er startet automatisch, sobald die DAW läuft, und springt bei Loops sauber auf Step 1.
- Bei gestoppter DAW startet **PLAY** im Plugin den Sequencer frei, praktisch zum Sounds-Bauen.
- **STOP** schaltet den Sequencer aus. MIDI-Noten aus der DAW spielen den Synth jederzeit direkt, so kannst du ihn auch ohne Sequencer mit eigenen Clips nutzen.
- **Presets**: im Display auf den Namen klicken oder die Pfeile daneben nutzen. Die DAW sieht sie auch als Programme.
- **Größe**: unten rechts an der Fensterecke ziehen.
- Alle Einstellungen, das aktuelle Pattern und die 16 Sequenz-Speicher werden mit dem Projekt gespeichert.
- Die Bedienung des Step-Felds (SHIFT, CHAN, RECALL, STORE, RAND) zeigt das Display über den Encodern jeweils an.

---

## Selbst bauen (optional)

**macOS** – braucht Xcode (App Store) und CMake (`brew install cmake`):

```
cd NovaBlipwerk
cmake -B build -G Xcode -DNOVA_COPY_AFTER_BUILD=ON
cmake --build build --config Release
```

Mit `NOVA_COPY_AFTER_BUILD=ON` landen AU und VST3 automatisch in `~/Library/Audio/Plug-Ins`. Selbst gebaute Plugins haben kein Quarantäne-Flag. Den Audio-Unit-Cache (Schritt 3 oben) danach trotzdem einmal neu laden.

**Windows** – braucht Visual Studio 2022 mit „Desktopentwicklung mit C++“ und CMake:

```
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

Die fertigen Dateien liegen danach in `build/NovaBlipwerk_artefacts/Release/`.
