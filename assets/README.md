# OpenLoch-Logo und README-Grafiken

Das Logo ist Pixel für Pixel gezeichnet, im Stil der eigenen Symbole von OpenLoch: 32 × 32 Pixel, Windows-16-Farben-Palette mit schwarzen Konturen, dazu die Platinenfarben des Programms. Es zeigt eine Lochrasterplatine mit Transistor, LED, drei Drahtbrücken und einem Widerstand mit dem gültigen Farbcode Braun–Schwarz–Rot–Gold (1 kΩ, ±5 %). Nichts davon stammt aus LochMaster.

- `scripts/make-logo.py` zeichnet das Logo und schreibt `openloch-logo-32.png` (die Pixelvorlage) und `app-icon-source.png` (ohne Glättung auf 1024 Pixel vergrößert).
- `scripts/make-icons.py` erzeugt daraus auf macOS `OpenLoch.icns`, `OpenLoch.ico` und `openloch.png`.

Die README-Grafiken sind eigenständige SVGs ohne Skripte und externe Verweise. `scripts/make-readme-art.py` setzt sie aus dem Material des Programms zusammen. Das CMake-Ziel `openloch_readme_art` (`tools/readme_art.cpp`) zeichnet dafür die Symbolleiste mit den eigenen Pixelsymbolen, einzelne Bauteile der offenen Bibliothek und ein Projekt in allen vier Dokumentarten, jedes mit seinem Modul: das Zweitransistor-Blinklicht aus `examples/perfboard` als Schaltplan (Symbole der mitgelieferten Bibliothek), als Lochrasterplatine (die Beispieldatei), als Leiterplatte (Footprints an den Lochrasterpositionen, die Drähte als Leiterbahnen) und als Frontplatte:

- `readme-header.svg`: Programmfenster mit Logo, Menüs, Symbolleiste und den vier Dokumentfenstern des Blinklichts, daneben die vier Module mit ihren Vorbildern
- `readme-parts.svg`: Bauteilfenster mit klassischen und modernen Bauteilen im gleichen Maßstab
- `readme-download.svg`, `readme-roadmap.svg`, `readme-build.svg`: Schaltflächen mit Diskette, Liste und Lötkolben
