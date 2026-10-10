# Bauteilbibliothek

OpenLoch bringt eine eigene Bauteilbibliothek mit. Sie enthält die 46 Seiten der LochMaster-Bibliothek mit denselben 506 Bauteilen, in derselben Reihenfolge, mit denselben Kennungen und Lochbildern. Ein Bauteil aus OpenLoch kann so ein Bauteil aus LochMaster auf derselben Platine ersetzen, ohne dass neu verdrahtet werden muss. Die Herstellerseite des Originals ist durch eine OpenLoch-Seite (Logo, Beschriftungsfeld) ersetzt. Dazu kommen acht Seiten mit modernen Bauteilen.

Die Bibliothek steht unter [CC0](../libraries/LICENSE) und darf frei verwendet werden, auch außerhalb von OpenLoch.

## Eigene Zeichnungen

Jedes Bauteil ist in [`libraries/`](../libraries) als Text beschrieben: Name, Kennung, Wert, Bauform mit Maßen, Anschlüsse und Aussehen. Das Programm zeichnet daraus Umrisse und Bilder. Nichts davon ist aus den LochMaster-Bibliotheken übernommen, abgepaust oder umgerechnet; übernommen sind nur Seitenaufteilung, Bauteilnamen, Kennungen und Lochbilder. Die Maße stammen aus Normen (JEDEC-Gehäuse wie TO-92, TO-220 und DIP, EIA-Baugrößen) und Datenblättern der Hersteller; jedes Bauteil nennt seine Quelle im Feld `source`. Wo Maße geschätzt sind, steht dort „ca.“.

Ob die vollständige Nachbildung von Seitenaufteilung und Bauteilauswahl rechtlich unbedenklich ist, wurde nicht juristisch geprüft.

## Moderne Bauteile

| Seite | Inhalt (Auswahl) |
| --- | --- |
| Mikrocontroller-Module | ESP32-DevKitC, ESP32 DevKit 30-polig, ESP32-S3/C3/C6, NodeMCU und D1 mini, ESP-01S, Raspberry Pi Pico, RP2040-Zero, Seeed XIAO, Arduino Nano/Pro Mini/Micro/Uno, STM32 Blue/Black Pill, Teensy, Digispark |
| Sensormodule | BME280/BMP280, DHT11/22, DS18B20, HC-SR04, PIR, MPU6050, INA219, VL53L0X, SHT31/AHT20, BH1750, HX711, ADS1115 |
| Anzeigemodule | OLED 0,96″ und 1,3″, LCD 1602/2004 mit I²C, TFT ST7735 und ILI9341, TM1637, MAX7219, WS2812B |
| Funk und Schnittstellen | nRF24L01, LoRa (RFM95, Ra-02), HC-05, MCP2515, MAX485, SD-Karte, DS3231, USB-Seriell, Pegelwandler |
| Stromversorgungsmodule | LM2596, MP1584, MT3608, AMS1117, TP4056, USB-C, Hi-Link-Netzteile, 18650-Halter, R-78E |
| Treiber und Relais | L298N, DRV8833, TB6612, A4988/DRV8825/TMC2208, ULN2003, Relaismodule, PCA9685 |
| Steckverbinder modern | JST-XH und -PH, Stift- und Buchsenleisten, USB-C, Klemmen 3,5/5,0/5,08 mm, DC-Buchse, XT30/XT60, Grove |
| Halbleiter modern | LDOs im TO-92 (MCP1700, HT7333), LD1117V33, Logic-Level-MOSFETs, Optokoppler, WS2812B/APA106 bedrahtet, MCP23017, PCF8574, ULN2803 |

Auf den Modulen stehen die Anschlussnamen wie Bestückungsdruck im Bild. Kennungen folgen dem Stil des Originals: `M#` für Module, `X#` für Steckverbinder, dazu `IC#`, `T#`, `LED#`, `Bu#`, `Kl#` und `Rel#`.

## Format

Eine Datei je Seite (`libraries/NN-name.json`) mit `page`, `page_en`, `position` (die Seitennummer; 1–46 wie im Original, eigene Seiten ab 101) und `parts`. Ein Bauteil hat `name` und `name_en`, optional `description` und `description_en`, `id` (Kennung wie `R#`), `value`, die Pflichtangabe `source`, eine Bauform `package` mit ihren Maßen und das Aussehen `look`. Längen sind Millimeter relativ zu Anschluss 1 (x nach rechts, y nach unten), Anschlüsse werden in Löchern zu 2,54 mm angegeben. `variants` erzeugt aus einem Eintrag mehrere Bauteile, etwa LED-Farben.

```json
{"name": "LED 3mm", "id": "D#", "package": "round", "pins": [[0, 0], [0, 2]], "diameter": 3.8,
 "flat": "down", "polarity": ["+", "-"], "look": {"form": "led", "flange": 0.18},
 "source": "LED Bauform T-1 (3 mm): Körper 3,0 mm, Kragen 3,8 mm",
 "variants": [{"value": "rot", "value_en": "red", "look": {"form": "led", "flange": 0.18, "colour": "rot"}}]}
```

**Bauformen** (`package`):

- `axial`: liegend zwischen zwei Drähten, mit `pitch` (Löcher), `length`, `diameter`, `caps` für Widerstandskappen, `bands` für Farbringe aus dem Wert und `stripe` für den Kathodenring
- `standing`: stehend, von oben gesehen; `band` zeichnet den Kathodenring oben, wo der zurückgeführte Draht zu Anschluss 2 beginnt
- `round` und `box`: Körper von oben mit `pins`, `diameter` oder `width`/`height`, `centre`, `flat`, `flat_cut`, `pin_kind` und `polarity`
- `lying`: radiales Bauteil liegend, mit `polarity` wie `round`
- `inline` und `dil`: eine oder zwei Reihen mit `count`, `pitch`, `row` und `axis`
- `to220`: drei Anschlüsse, liegend oder `standing`
- `custom`: Anschlüsse, Formen, Linien und Beschriftungen ausdrücklich angegeben

`off_grid: true` erlaubt Anschlüsse außerhalb des 2,54-mm-Rasters, etwa bei Sub-D-Buchsen oder JST-Steckern.

**Anschlussnamen** (`pin_names`): die Namen der Anschlüsse in der Reihenfolge, in der das Bauteil sie angibt oder seine Bauform sie erzeugt, jeder einmal. Jeder Anschlussdraht und jeder Anschluss-Pin trägt seinen Namen im Feld `label` der LIB-Datei, ohne `pin_names` seine Nummer (`1`, `2`, … in dieser Reihenfolge). Das Feld gehört im LM4-Format zu jedem Objekt; OpenLoch ordnet über die Namen die Anschlüsse eines Bauteils denen des Schaltplans zu ([Soll-Verbindungen](suite.md#soll-verbindungen-aus-dem-schaltplan)). `pin_labels: true` schreibt jedem Anschluss seinen Namen neben das Loch, wie die Polaritätszeichen stehen; so zeigt die Zeichnung, wofür ein Anschluss steht.

Namen tragen die Bauteile, deren Zeichnung oder Quelle die Belegung zeigt:

- Dioden `A`/`K` nach dem Kathodenring, auch die stehenden (Ring oben, Anschluss 2 ist die Kathode); LEDs `A`/`K`, Anschluss 1 mit dem Pluszeichen; Elkos, Tantal, die Knopfzellenhalterung und der Clip für 9-V-Blöcke (rote Litze) `+`/`-`; Brückengleichrichter `+`, `~1`, `~2`, `-`.
- Transistoren `B`/`C`/`E` nach der Beschriftung; die Gehäuse ohne Typ nach ihrer üblichen Belegung: TO-220 liegend b-c-e, TO-126 liegend e-c-b wie BD135, das zweite TO-92 e-b-c (das erste c-b-e), Metallgehäuse (TO-5, TO-39, TO-52) mit dem Emitter an der Nase, dann Basis und Kollektor gegen den Uhrzeigersinn von oben. MOSFETs `G`/`D`/`S`, Spannungsregler `IN`/`GND`/`OUT`/`ADJ`, die modernen Halbleiter nach ihrem Datenblatt.
- DIL-ICs und -Fassungen die Nummern ihres Gehäuses: Anschluss 1 links an der Kerbe, gegen den Uhrzeigersinn von oben gezählt.
- Module, deren Bild jeden Anschluss einmal benennt (etwa HC-SR04, HC-05, DHT22), mit diesen Namen.

Nummeriert bleiben Bauteile ohne Polarität oder ohne feste Belegung (Widerstände, Steckverbinder, Schalter, Relais, Trafos), Duo-LEDs, das TO-3-Gehäuse, Anzeigen und Module, die einen Namen mehrfach tragen.

**Aussehen** (`look`): `form` (`cylinder`, `sphere`, `led`, `cap`, `box`, `elko`, `fins`, `display7`, `bars`, `pcb`), `colour` (Name oder `#rrggbb`), `gloss` und `material` (`plastic`, `epoxy`, `ceramic`, `metal`). Dazu kommen Einzelheiten wie `dot`, `notch`, `hole`, `ends`, `tab`, `stripes` und `contacts`. `marks` setzt weitere Merkmale ins Bild, jeweils mit Lage und Größe in Anteilen des Bildes: Löcher, Stifte, Schrauben, Taster, durchkontaktierte Löcher, Chips, Abschirmbleche, USB-Buchsen, SMD-LEDs, Antennen, Langlöcher und Aufdrucke (`print` mit `text`).

Die Bilder entstehen mit 12 Pixeln je Millimeter, bei großen Körpern mit höchstens 300 Pixeln auf der langen Seite, denn jedes platzierte Bauteil nimmt sein Bild in die Platinendatei mit. Module mit Aufdrucken bekommen mit `pixels` mehr Auflösung. Werte, Kennungen und Farbringe bleiben Vektorobjekte und ändern sich mit dem Wert.

## Im Programm

OpenLoch erzeugt jede Seite beim ersten Anzeigen und zeigt sie schreibgeschützt als „OpenLoch · …“ an. **Bibliothek → OpenLoch-Bibliothek als LIB-Dateien speichern…** schreibt alle Seiten als gewöhnliche LIB-Dateien, die auch LochMaster öffnet. Die automatischen Tests prüfen, dass jede Seite fehlerfrei erzeugt und wieder gelesen wird.
