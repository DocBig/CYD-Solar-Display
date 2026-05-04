# Web-Installer Setup

So bekommst du den Web-Installer ans Laufen — alles statisch über GitHub Pages.

## Dateien für den `docs/`-Ordner

```
docs/
├── index.html               ← Web-Installer-Seite
├── manifest.json            ← Pointer zur Firmware
├── firmware-merged.bin      ← deine kompilierte Firmware
└── cyd_solar_overview.jpg   ← Showcase-Bild im Header
```

## Schritt 1 — Firmware mergen

Per Default erzeugt PlatformIO drei separate `.bin`-Dateien (bootloader, partitions, application). Für den Web-Installer brauchen wir alle zu **einer einzigen Datei** zusammengefasst:

```bash
# Im Projekt-Root, nach erfolgreichem `pio run`
cd ~/.platformio/packages/tool-esptoolpy

python esptool.py --chip esp32 merge_bin \
  -o firmware-merged.bin \
  --flash_mode dio \
  --flash_freq 40m \
  --flash_size 4MB \
  0x1000  ~/path/to/project/.pio/build/cyd/bootloader.bin \
  0x8000  ~/path/to/project/.pio/build/cyd/partitions.bin \
  0xe000  ~/path/to/project/.pio/build/cyd/boot_app0.bin \
  0x10000 ~/path/to/project/.pio/build/cyd/firmware.bin
```

Die Offsets (`0x1000`, `0x8000`, `0xe000`, `0x10000`) sind Standard für ESP32. `boot_app0.bin` findest du im Arduino-Framework-Folder von PlatformIO.

**Alternative:** Ein Custom-Target im `platformio.ini` definieren, das `merge_bin` automatisch nach jedem Build ausführt. Sag Bescheid wenn du den Snippet brauchst.

## Schritt 2 — GitHub Pages aktivieren

Im Repo `DocBig/CYD-Solar-Display` (oder wie auch immer du es nennst):

1. Erstelle einen Ordner `docs/` im Hauptverzeichnis
2. Lege da rein:
   - `index.html`
   - `manifest.json`
   - `firmware-merged.bin`
   - `cyd_solar_overview.jpg`
3. Settings → Pages → Source: `main` Branch, Folder: `/docs`
4. Save → GitHub Pages baut die Seite automatisch
5. URL erscheint oben (typisch `https://docbig.github.io/CYD-Solar-Display/`)

## Schritt 3 — Testen

1. Öffne die GitHub-Pages-URL in **Chrome oder Edge** (nicht Firefox/Safari!)
2. ESP32-CYD per USB anschließen
3. Statusbar sollte grün leuchten ("Browser unterstützt Web Serial")
4. Auf "Install" klicken → Browser fragt nach USB-Port
5. Port wählen → Firmware wird geflasht (~30 Sekunden)

## Schritt 4 — Bei jedem Update

Nach jeder neuen Firmware-Version:

1. Neu bauen: `pio run`
2. Mergen: `esptool merge_bin ...` (siehe Schritt 1)
3. `firmware-merged.bin` im `docs/`-Ordner ersetzen
4. Optional: Version-String in `manifest.json` hochzählen
5. Commit + Push → GitHub Pages aktualisiert sich nach 1-2 Minuten automatisch

## Showcase-Bild austauschen

Das Bild `cyd_solar_overview.jpg` zeigt aktuell die vier Display-Seiten in einer Komposition. Wenn du es ersetzen willst:

- Gleichen Dateinamen behalten oder im HTML den `<img src="...">`-Pfad anpassen
- Optimal: Querformat, ca. 1400×1300 Pixel oder ähnlich
- JPEG mit ~80% Qualität, sollte unter 200 KB bleiben

## Troubleshooting

**"Failed to flash"**: Manche Boards brauchen dass die BOOT-Taste während des Flashens gehalten wird. ESP Web Tools versucht das automatisch, klappt aber nicht immer.

**"Port verschwindet nach Browser-Neustart"**: Normal — der Browser speichert die Port-Berechtigung nicht über Sessions hinweg. Einfach neu auswählen.

**Web Serial fehlt in Chrome**: In aktuellen Versionen Standard. Falls nicht: `chrome://flags/#enable-experimental-web-platform-features` aktivieren.

## Optional — eigene Domain

GitHub Pages unterstützt Custom-Domains. Wenn du z.B. `solar-display.docbig.de` willst:
1. CNAME-Eintrag bei deinem DNS-Provider auf `docbig.github.io` setzen
2. Im Repo: `docs/CNAME`-Datei erstellen mit der Domain als Inhalt
3. In GitHub-Pages-Settings die Custom-Domain eintragen
