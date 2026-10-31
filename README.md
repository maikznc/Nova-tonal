# NOVA Leitstern

**Leitstern** ist das Tonal-Balance-Plugin von NOVA. Es zeigt dir, ob dein Mix spektral so aufgestellt ist wie deine Referenzen:
Referenztracks rein → Zielkurve (Median + Streubereich) → dein Signal live dagegen,
Abweichungen in Orange. Für macOS als **VST3** (Ableton, Bitwig, Reaper …) und **AU** (Logic).

Die NOVA-Mastering-Chain kommt später als eigenes Plugin in dieses Projekt.

---

## 1. Fertige Plugins herunterladen (GitHub)

Du musst nichts selbst bauen: GitHub baut die Plugins bei jeder Änderung automatisch
auf einem Mac (Apple Silicon + Intel), testet sie und packt einen Installer.

1. Im Repo oben auf **Actions** klicken.
2. Den obersten Lauf **„Mac-Plugins bauen“** mit grünem Haken öffnen.
3. Unten bei **Artifacts** auf **NOVA-Leitstern-macOS** klicken → ZIP wird geladen.
4. Entpacken und den **Installer (.pkg)** öffnen. Beim ersten Mal unter
   Systemeinstellungen → Datenschutz & Sicherheit → **„Trotzdem öffnen“** bestätigen
   (das Plugin ist nicht bei Apple registriert). Details: `INSTALLATION.txt`.

Für feste Versionen (Tag `v0.1.0` usw.) liegen dieselben Dateien zusätzlich unter **Releases**.

## 2. Selbst bauen (optional)

Nur nötig, wenn du ohne GitHub arbeiten willst.

1. Einmalig: `xcode-select --install` und CMake (`brew install cmake` oder https://cmake.org/download/).
2. Im Projektordner:
   ```
   cmake -B build -DCMAKE_BUILD_TYPE=Release
   cmake --build build --target TonalBalance_VST3 TonalBalance_AU -j8
   ```
   Beim ersten Mal wird JUCE 9.0.4 automatisch geladen. Die Plugins werden danach direkt in
   `~/Library/Audio/Plug-Ins/VST3` und `~/Library/Audio/Plug-Ins/Components` installiert.
3. Optional einen Installer daraus machen: `bash scripts/package-macos.sh` → Ordner `dist/`.

## 3. In Ableton einbinden

1. Ableton → **Einstellungen → Plug-Ins**
2. **„VST3-Plug-In-Systemordner verwenden“** einschalten
3. **„Neu scannen“** klicken
4. Im Browser unter **Plug-Ins → NOVA → NOVA Leitstern**
5. Am besten als **letztes Plugin auf den Master** legen

Nach einem Neubau: Ableton neu starten (Ableton lädt geänderte Plugins sonst nicht neu).

## 4. Benutzen

- **Referenzen laden**: 3–5 fertige Tracks aus deinem Genre ins Plugin-Fenster ziehen
  (WAV, AIFF, FLAC, MP3, M4A). Daraus wird die Zielkurve.
- **Ziel speichern**: legt die Kurve unter
  `~/Library/Application Support/NOVA/Targets` ab. Gespeicherte Ziele
  findest du oben im Auswahlfeld – in jedem Projekt.
- **Aus der Browser-Version**: eine dort exportierte `.json`-Datei einfach ins
  Plugin-Fenster ziehen – sie wird importiert und gespeichert. Beide Versionen
  rechnen identisch.
- **Kurzzeit**: die letzten 3 Sekunden. **Gesamt**: alles seit dem letzten Reset –
  Song einmal komplett durchlaufen lassen.
- **Weit / Eng**: Zielbereich 10.–90. bzw. 25.–75. Perzentil der Referenzen.
- **Neigung**: kippt nur die Anzeige, damit Höhen und Bässe besser lesbar sind.
- Die Zielkurve wird mit dem Ableton-Projekt gespeichert.
- Das Audio läuft unverändert durch – das Plugin misst nur.

## 5. Wenn etwas hakt

| Problem | Lösung |
|---|---|
| `cmake: command not found` | CMake installieren (Schritt 1) oder Terminal neu öffnen |
| Fehler zu „xcrun“ oder „SDK“ | `xcode-select --install` ausführen |
| Plugin taucht in Ableton nicht auf | VST3-Systemordner aktivieren, neu scannen, Ableton neu starten |
| AU taucht in Logic nicht auf | Terminal: `killall -9 AudioComponentRegistrar`, Logic neu starten |
| Plugin auf einen anderen Mac kopiert und blockiert | Terminal: `xattr -cr ~/Library/Audio/Plug-Ins/VST3/"NOVA Leitstern.vst3"` |

## 6. Tests (optional)

```
cmake -B build-tests -DCMAKE_BUILD_TYPE=Release -DMS_BUILD_TESTS=ON
cmake --build build-tests --target MSTests -j8
./build-tests/tests/MSTests_artefacts/Release/MSTests
```

Geprüft wird u. a.: rosa Rauschen ergibt eine flache Kurve, ein 1-kHz-Sinus misst
≈ 0 LUFS (BS.1770), Referenzen zählen unabhängig von ihrer Länge gleich viel,
Live-Messung und Datei-Messung stimmen überein, der Projektzustand wird gespeichert.

## Projektaufbau

```
NOVA-Plugins/
├── CMakeLists.txt                  Gesamtprojekt (lädt JUCE)
├── modules/ms_core/                gemeinsamer Analyse-Kern für alle Plugins
│   ├── analysis/                   Bandspektrum, Lautheit, Zielkurven, Referenz-Analyse
│   └── ui/                         Farben und Look der Suite
├── plugins/TonalBalance/           das Plugin „NOVA Leitstern“
├── tests/                          Tests + Screenshot-Werkzeug
├── web/tonal-balance.html          Browser-Prototyp (gleiche Mathematik)
├── .github/workflows/              Automatischer Mac-Build auf GitHub
├── scripts/package-macos.sh        Signieren + Installer bauen
└── packaging/INSTALLATION.txt      Installationsanleitung, liegt jedem Download bei
```

**Wie gemessen wird:** 1/6-Oktav-Bänder von 20 Hz bis 20 kHz, FFT mit ~3 Hz Auflösung,
lautheitsnormiert über die K-Gewichtung aus ITU-R BS.1770 – dadurch ist die Kurve
unabhängig davon, wie laut ein Track gemastert ist. Referenzen werden in 3-Sekunden-Fenster
zerlegt (eins pro Sekunde), leise Stellen (Intros, Breaks) fallen raus, jeder Track zählt gleich.

## Lizenz-Hinweis

Das Projekt nutzt das JUCE-Framework. JUCE steht unter AGPLv3 oder der kommerziellen
JUCE-9-Lizenz (https://juce.com/legal/juce-9-licence/). Für den eigenen Gebrauch ist das
unproblematisch; wenn du die Plugins verkaufen oder weitergeben willst, prüf vorher, ob du
eine kommerzielle JUCE-Lizenz brauchst.
