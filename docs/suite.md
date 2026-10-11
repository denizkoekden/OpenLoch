# Ein Programm, vier Dokumentarten (`src/suite`)

OpenLoch vereint die Dokumentarten Schaltplan, Lochraster, Leiterplatte und Frontplatte in einem Programm. Jede Dokumentart ist ein eigenes Modul mit eigenem Modell, Editor und Dateiformat; die Suite in `src/suite/` verbindet sie: Startbildschirm, Zuordnung von Dateien zu Editoren, das Menü „Fenster“ und die zuletzt verwendeten Dokumente.

| Dokumentart | Kennung | Editor |
| --- | --- | --- |
| Schaltplan | `schematic` | `openloch::schematic::Editor` ([Moduldoku](modules/schematic.md)) |
| Lochraster | `perfboard` | `openloch::Window` (`src/window.*`, [Vergleich mit LochMaster](lochmaster-compatibility.md)) |
| Leiterplatte | `pcb` | `openloch::pcb::Editor` in `PcbWindow` ([Moduldoku](modules/pcb.md)) |
| Frontplatte | `frontpanel` | `openloch::frontpanel::PanelEditor` ([Moduldoku](modules/frontpanel.md)) |

## Bedienung

- **Startbildschirm** (ohne Datei beim Start): eine Kachel je Dokumentart mit ihrer Aufgabe, „Öffnen…“ für alle Dokumentarten und die zuletzt verwendeten Dokumente mit Art und Ordner (Doppelklick oder Eingabetaste öffnet, das Kontextmenü entfernt Einträge). Dateien lassen sich auf den Startbildschirm ziehen.
- **Neu:** Die Kachel öffnet den kurzen Neu-Dialog der Dokumentart und danach ihren Editor. Lochraster: Breite und Höhe (100 × 80 mm). Leiterplatte: der Neu-Dialog des Moduls wie in Sprint-Layout, nur Arbeitsfläche (160 × 100 mm) oder rechteckige bzw. runde Platine mit Kontur und Rand, dazu der Name; der Ursprung folgt den Grundeinstellungen des Moduls. Frontplatte: der Eigenschaftendialog der Frontplatte. Schaltplan: ohne Dialog ein Blatt DIN A4 quer. Abbrechen legt nichts an.
- **Fenster:** Jedes Dokument hat ein eigenes Fenster mit den Menüs seines Vorbilds. Die Suite ergänzt nur das Menü **Fenster** (vor „Hilfe“, sonst am Ende): Neues Dokument ▸, Zum Projekt hinzufügen ▸, Projektübersicht…, Bibliotheken…, Dokument öffnen…, Zuletzt verwendet ▸, Startbildschirm und die Dokumente, nach Projekten geordnet; ein Dokument ohne Fenster öffnet sich von dort.
- **Projektübersicht:** zeigt die Dokumente des Projekts mit Art, Herkunft (die Datei eines Vorbilds, aus der ein Dokument stammt) und ob es geöffnet ist. Ein Dokument lässt sich öffnen (auch per Doppelklick), umbenennen, entfernen oder hinzufügen; Umbenennen und Entfernen speichern das Projekt sofort, wie das Hinzufügen. Lochraster und Frontplatte nehmen einen neuen Namen als ihren Titel an (als Bearbeitungsschritt, wenn sie geöffnet sind), Schaltplan und Leiterplatte behalten ihn im Projekt. Entfernt werden kann weder das letzte Dokument noch das des Fensters, aus dem die Übersicht stammt; das Fenster eines entfernten Dokuments schließt sich ohne Rückfrage. Die Seite **Bauteile** zeigt die Bauteile aller Dokumente, verbunden über ihre Kennung (`suite::projectParts()` in `src/suite/projectparts.h`): Bezeichner und Wert aus dem Schaltplan, wenn er das Bauteil hat, sonst aus dem ersten Dokument, und je Dokument, ob es das Bauteil enthält. **Stückliste exportieren…** schreibt die Bauteile des Schaltplans und der Platinen (ohne die Bohrungen der Frontplatten) gruppiert nach Wert und Buchstaben des Bezeichners als CSV: Menge, Bezeichner, Wert, UTF-8 mit Byte-Order-Mark, Semikolon als Trennzeichen.
- **Projekte** ([Dateiformate und Projekte](file-formats.md)): Ein neues Dokument ist ein Projekt; „Speichern“ fragt beim ersten Mal nach einer Datei `.openloch` und schreibt danach das ganze Projekt mit dem Stand aller seiner Fenster, aus welchem Fenster auch immer. „Zum Projekt hinzufügen“ legt ein weiteres Dokument an und speichert das Projekt sofort. Eine einzelne Datei eines Vorbilds (`.LM4`, `.FPL`, …) speichert ihr Editor in diese Datei zurück; kommt ein zweites Dokument dazu, wird daraus ein Projekt, und die Originaldatei bleibt unverändert. Alle vier Dokumentarten können sich ein Projekt teilen.
- **Öffnen:** Dateien von der Kommandozeile, aus dem Finder (macOS) oder aus der Liste öffnen direkt in ihrem Editor. Eine schon offene Datei kommt nach vorn; ein unberührtes neues Dokument derselben Art überlässt sein Fenster der geöffneten Datei. „Öffnen…“ im Datei-Menü jedes Editors geht über den Dialog der Suite, in dem die eigene Art zuerst steht: eine Datei einer anderen Art öffnet in deren Editor, ein Projekt als Projekt, und im selben Fenster nur, wenn es ein unberührtes Dokument zeigt.
- **Schließen und Beenden:** Schließt das letzte Dokument, erscheint wieder der Startbildschirm. Schließen des Startbildschirms oder **Beenden** in einem Fenster beendet das Programm; jedes Fenster fragt nach ungespeicherten Änderungen, „Abbrechen“ hält das Beenden an.
- **Wiederherstellen:** Sicherungen ungespeicherter Lochraster-Projekte aus einem früheren Lauf bietet OpenLoch einmal beim Start an.
- **Kommandozeile:** `OpenLoch datei…` öffnet Dateien jeder Art, `OpenLoch --render bild.png datei` zeichnet ein Dokument ohne Fenster (Schaltplan und Frontplatte mit 300 dpi, Leiterplatte 1600 Pixel breit).

## Welche Datei öffnet welcher Editor

| Endung | Editor |
| --- | --- |
| `.openloch` | das Projekt, mit seinem zuletzt bearbeiteten Dokument |
| `.olsch` | Schaltplan, als Projekt mit einem Dokument |
| `.spl8`, `.spl7` | Schaltplan; der Editor schreibt beim Speichern in die sPlan-Datei zurück, solange nichts verloren geht |
| `.LM4`, `.LMB`, `.OLD` | Lochraster |
| `.olpcb`, `.lay6`, `.lay` | Leiterplatte (`.olpcb` als Projekt mit einem Dokument; `.lmk` ist ein Makro und wird im Editor platziert, nicht geöffnet) |
| `.olfp`, `.FPL` | Frontplatte (`.olfp` als Projekt mit einem Dokument) |
| `.LIB`, `.BAK` | nach dem Inhalt; Bibliotheksseiten von sPlan sind keine Dokumente, der Schaltplan zeigt sie in seiner Bibliothek |

Die macOS-App meldet diese Dateien beim Finder an (`assets/Info.plist.in`, Qts Vorlage mit Dokumenttypen): ihre eigenen Formate `.openloch`, `.olsch`, `.olpcb` und `.olfp` als deren Besitzer, die Dateien der Vorbilder (`.spl8`, `.spl7`, `.lm4`, `.lmb`, `.lay6`, `.lay`, `.fpl`) als Standardprogramm, Bibliotheksseiten (`.lib`, eine Endung, die auch andere Programme verwenden) nur als Alternative unter „Öffnen mit“. Die Typkennungen liegen im eigenen Namensraum `org.openloch.*`. Ein Doppelklick im Finder öffnet die Datei über das Öffnen-Ereignis des Systems in ihrem Editor. Der Suite-Test prüft, dass die Vorlage jede Endung der Zuordnung nennt, `tools/check_macos_bundle.py` im gebauten Bündel, dass jeder Typ erklärt ist und geöffnet wird.

LochMaster, FrontDesigner und sPlan verwenden alle `.LIB` für Bibliotheksseiten und `.BAK` für Sicherungen. sPlans Dateien beginnen mit einem eigenen Kopf (`SPLAN70`, `SPLAN80`); LochMaster und FrontDesigner schreiben denselben 41-Byte-Kopf, und LochMaster-Dateien tragen Versionen von 3 bis 4. Deshalb entscheidet der Inhalt: Eine Sicherung mit sPlans Kopf gehört zum Schaltplan, was der FrontDesigner-Leser annimmt, ist eine Frontplatte, JSON-Sicherungen von Frontplatte und Leiterplatte gehen nach ihrem Feld `format`, alles andere an den Lochraster-Editor, der ein Problem dann benennt.

## Code

| Datei | Inhalt |
| --- | --- |
| `src/suite/documents.*` | Dokumentarten, Dateizuordnung (`documentKind`), Filter des Öffnen-Dialogs, Symbole der Arten |
| `src/suite/suite.*` | `Suite`: Fenster erzeugen, finden und wiederverwenden, Neu-Dialoge, Öffnen, Fenster-Menü, zuletzt verwendete Dokumente (`QSettings` `suite/recentDocuments`, höchstens zehn), Beenden, Wiederherstellung beim Start, `renderDocument` |
| `src/suite/startscreen.*` | Startbildschirm |
| `src/suite/libraries.cpp` | Übersicht der Bibliotheksordner aller Dokumentarten (Fenster → Bibliotheken…) |
| `src/suite/schematictargets.*` | Soll-Verbindungen aus dem Schaltplan eines Projekts |
| `src/suite/projectparts.*`, `projectoverview.cpp` | Bauteile über alle Dokumente eines Projekts, Stückliste, Projektübersicht |
| `src/suite/boardsources.*` | Die Platinen eines Projekts für die Frontplatte: Lochraster und Leiterplatte mit den Mitten, Umrissen und Seiten ihrer Bauteile |
| `src/suite/pcbwindow.*` | Hauptfenster um den Leiterplatten-Editor mit der Menütabelle des Moduls und seiner Werkzeugleiste |
| `src/main.cpp` | Kommandozeile, Sprache, Öffnen- und Beenden-Ereignisse des Systems |
| `src/longlists.h` | Lange Listen in Dialogen für alle Module: `reportList`, `scrolling`, `fitToScreen`, `screenRoom`, `listInBrief` |
| `src/autoplace.*` | Bauteile anordnen und Verdrahten im Lochraster |
| `src/emfwriter.*` | `openloch::EmfDevice`: Vektorgrafik als EMF für alle Module (eine Einheit ein Hundertstel Millimeter, Titel im Dateikopf), in `openloch_core` |
| `tests/suite_tests.cpp` | CTest `suite` |

Einen abstrakten Dokumentvertrag oder Plug-ins gibt es nicht: `Suite` behandelt jede Dokumentart in einem eigenen Zweig und verwendet nur öffentliche Funktionen der Editoren (Öffnen, Dateipfad, Änderungszustand, neues Dokument). Die zuletzt verwendeten Dokumente folgen dem Fenstertitel, der sich beim Öffnen und Speichern ändert; die Editoren brauchen dafür keinen Rückruf. Das Menü „Fenster“ und die Verbindung an „Beenden“ (die Aktion mit dem Objektnamen `quit`) setzt die Suite von außen. `src/window.cpp` bindet keine Modul-Header ein.

## Hinweise für Arbeit an den Modulen

- **Menüs der Leiterplatte:** `PcbWindow` zeigt die Menütabelle des Moduls, `openloch::pcb::editorMenus()`, und hängt „Beenden“ an „Datei“. Neue Menüaktionen trägt das Modul nur dort ein; der Test `suite` prüft, dass das Fenster der Suite genau diese Tabelle zeigt.
- **Frontplatte:** `PanelEditor` wird unverändert als Fenster verwendet; Menüs und Werkzeugleisten bleiben Sache des Moduls.
- **Neue Dateiendungen** eines Moduls brauchen einen Eintrag in `documentKind` und `documentFilter` (`src/suite/documents.cpp`).
- **Tastenkürzel in Tooltips:** Ein Befehl mit Kürzel zeigt es im Tooltip in Klammern, etwa „Drehen (Strg+R)“, unter macOS mit ⌘. Ein Befehl, dessen Tasten das Modul selbst verarbeitet (Werkzeuge der Zeichenfläche), setzt die Eigenschaft `toolTipShortcut`; sie erscheint dann ebenso. Die Suite ergänzt die Tooltips beim Anzeigen, eine geänderte Taste gilt also sofort.
- **Übersetzungen:** Die Texte der Suite stehen in `scripts/translations/suite.py` und `suite_fr.py`. Qts eigene Texte der Standardknöpfe und Meldungsfenster (OK, Abbrechen, Ja, Nein, Schließen, Details einblenden …) übersetzt `installStandardTexts()` aus `src/language.h` über dieselben Tabellen in die Oberflächensprache. `suite_fr.py` übersetzt auch „Frontplatte“ („Face avant“), weil der Startbildschirm das Wort zeigt; die französische Tabelle des Frontplattenmoduls verwendet es von dort. Der Core-Test prüft die `ui()`-Texte in allen Unterordnern von `src/`.

## Ein Modul einbinden

Für ein neues Modul und für die Projekte nach [Dateiformate und Projekte](file-formats.md) und [Projektformat](openloch-project-format.md):

1. **Ordner und Bau:** Modell, Editor und Tests unter `src/modules/<art>/` mit einer `module.cmake` (Bibliothek `openloch_<art>`, eigenes Testprogramm als CTest `<art>`), die Formate des Vorbilds unter `src/formats/<vorbild>/`. Alle Oberflächentexte über `ui()`, die Übersetzungen in `scripts/translations/<art>.py` und `<art>_fr.py`.
2. **Editor:** ein `QMainWindow` mit eigenen Menüs wie `frontpanel::PanelEditor` oder ein Widget mit einer Menütabelle wie `pcb::editorMenus()`. Er übernimmt ein Dokument ohne Dateipfad (`setDocument`), meldet Änderungszustand und Titeländerungen, fragt mit `maybeSave()` nach ungespeicherten Änderungen und nennt eine eigene Beenden-Aktion `quit`.
3. **Dokument:** ein eigenes Modell mit Kodieren und Lesen als JSON-Objekt mit eigenem `format` und eigener `version`, vollständiger Prüfung beim Lesen und atomarem Speichern. Dieses Objekt ist später unverändert der Inhalt des Dokuments im Projekt.
4. **Kennungen:** stabile Kennungen für Bauteile und ihre Anschlüsse von Anfang an, damit Bauteile zwischen Dokumenten geteilt werden können.
5. **Formate des Vorbilds:** Leser und Schreiber mit verlustfreiem Rundlauf; der Export nennt, was entfällt.
6. **In der Suite:** Dokumentart in `src/suite/documents.cpp` freischalten und ihre Endungen eintragen, Fenster in `Suite::makeWindow`, `openIn`, `documentPath` und `create`, Neu-Dialog, Tests in `tests/suite_tests.cpp`.

Für Projekte braucht ein Modul außerdem:

- **Speichern über die Suite:** Die Suite kann dem Editor eine Funktion geben, die sein eigenes Schreiben bei „Speichern“ und „Speichern unter“ ersetzt; dann speichert jedes Fenster das ganze Projekt. Sie holt die aktuellen Dokumentdaten jederzeit ab und meldet dem Editor, dass gespeichert ist.
- **Bauteil-Schnittstelle:** Bauteile mit Kennung, Wert und Anschlüssen aufzählen, ein Bauteil aus einem Bibliothekseintrag platzieren, Kennung und Wert aus dem Projekt übernehmen und zu einem Element sein Bauteil nennen.
- **Originale im Projekt:** ein Original als Dokumentdaten einlesen und Dokumentdaten in ein Original schreiben, mit der Liste der Verluste.
- **Soll-Verbindungen:** Platinen-Module vergleichen sich mit dem Schaltplan ihres Projekts ([Soll-Verbindungen](#soll-verbindungen-aus-dem-schaltplan)).

## Soll-Verbindungen aus dem Schaltplan

Die Suite leitet aus dem ersten Schaltplan eines Projekts dessen Bauteile und Netze ab, jedes Mal neu und nie gespeichert: `documents::Targets` in `src/documents/targets.h`, gelesen von `suite::schematicTargets()` (`src/suite/schematictargets.h`). Ist der Schaltplan geöffnet, zählt sein aktueller Stand. Kommt ein Fenster zu einem Projekt oder ein Dokument hinzu, ruft die Suite `projectChanged()` der Lochraster- und Leiterplattenfenster des Projekts auf: Ihre Befehle zum Schaltplan folgen sofort, und ein angezeigter Vergleich rechnet neu.

```json
{
  "components": [{"id": "<Kennung des Bauteils>", "designator": "R1", "value": "10k", "kind": "R", "pins": ["1", "2"]}],
  "nets": [{"name": "VCC", "pins": [["<Kennung des Bauteils>", "1"], ["<…>", "2"]]}]
}
```

- Ein Bauteil hat die Kennung seines Schaltplanbauteils, den angezeigten Bezeichner und den Wert. Symbole ohne Bezeichner (Masse, Versorgung) verbinden nur Netze und fehlen in der Liste.
- Ein Child (ein Gatter eines ICs, ein Kontakt eines Relais) gehört zum Bauteil seines Parents: Seine Kontakte zählen als Anschlüsse des Parents, ein Netz an ihnen als Netz am Parent, und es erscheint nicht als eigenes Bauteil. Ein Child, dessen Parent im Schaltplan fehlt, steht für sich.
- `kind` ist die Bauteilart (R, C, L, D, T, IC, K, S, F, X, B …), unabhängig davon, ob der angezeigte Bezeichner Seitennummer und Präfix trägt („2R1“, „=A-2R1“): bei Symbolen der mitgelieferten Bibliothek die Art ihrer Seite (deren Bezeichnerbuchstaben folgen sPlans Bibliothek, ein Transistor heißt dort K1), sonst die Buchstaben des eingegebenen Bezeichners, bei einem Kennzeichen nach IEC 81346 die hinter seinem letzten Vorzeichen („-R3“, „=A1-K2“: R, K). Sie ist für die R/C/L-Regel und das Sortieren nach Bauteilart gedacht; fehlt sie (ältere Daten), gelten die Buchstaben des Bezeichners.
- Ein Anschluss heißt wie sein Kontakt im Schaltplan („1“, „A“, „E“). Fehlt ein Name oder kommt er in einem Bauteil doppelt vor, heißen alle Anschlüsse dieses Bauteils nach ihrer Stelle „1“, „2“, ….
- Ein Netz ohne Netzbezeichnung hat keinen Namen. Ein Netz mit nur einem Anschluss verlangt keine Verbindung, darf aber mit keinem anderen Netz verbunden sein.

Ein Platinen-Modul findet zu jedem Bauteil sein Gegenstück zuerst über die Kennung des Bauteils (Feld `component`), sonst über den gleichen Bezeichner ohne Rücksicht auf Groß- und Kleinschreibung; ein Bezeichner, den mehrere Bauteile tragen, wird nicht genommen. Anschlüsse werden über ihren Namen zugeordnet, ebenfalls ohne Rücksicht auf Groß- und Kleinschreibung. Der Schaltplan nummeriert die beiden Kontakte von Dioden, LEDs und Elkos: `1` ist die Anode bzw. Plus, `2` die Kathode bzw. Minus; sie gelten für Anschlüsse namens `A`/`K` bzw. `+`/`-`. Nur die Anschlüsse `1` und `2` von Widerständen, Kondensatoren und Spulen (Bauteilart `kind` `R`, `C`, `L`) dürfen andersherum stecken, wenn beide Seiten sie so nummerieren; eine Diode mit nummerierten Anschlüssen nie. Ob ein solches Bauteil gedreht steckt, wird Bauteil für Bauteil entschieden, jedes mit den Drehungen davor: Von zwei über Kreuz verbundenen Widerständen dreht sich einer, nicht beide.

Ein Bauteil, das das Dokument auf einer anderen seiner Platinen einem Bauteil des Schaltplans zugeordnet hat, gehört dorthin: Die angezeigte Platine wird ohne dieses Bauteil und seine Anschlüsse verglichen, und nichts auf ihr wird ihm zugeordnet oder für es gesetzt.

Lochraster und Leiterplatte zeigen danach offene Verbindungen als Luftlinien und listen fehlende und überzählige Bauteile, offene Netze und verbundene Netze („Mit Schaltplan vergleichen“, im Lochraster im Menü Prüfen, in der Leiterplatte im Menü Extras); „Aus Schaltplan übernehmen“ verknüpft gefundene Bauteile und übernimmt Bezeichner und Wert, erst nach einer Übersicht. „Anschlüsse zuordnen“ benennt die Anschlüsse eines Bauteils nach denen des Schaltplans; ein Anschluss, der bei dem Anschluss bleibt, für den sein eigener Name steht, behält seinen Namen (`+` und `-` eines Elkos bleiben).

„Fehlende Bauteile setzen“ schlägt für jedes fehlende Bauteil Teile vor, deren Anschlüsse passen: das Lochraster Teile der OpenLoch-Bibliothek, die es rechts neben die Platine auf das Lochraster legt, die Leiterplatte Footprints ihrer Modulbibliothek rechts neben der Arbeitsfläche ([Leiterplatte](modules/pcb.md#abgleich-mit-dem-schaltplan)). Im Lochraster stehen Teile mit allen Anschlüssen in Löchern des Rasters vor den anderen (die anderen tragen den Hinweis „Anschlüsse nicht im Lochraster“), dann Teile derselben Art (nach `kind`: R, C, L, D und LED, T und Q, IC und U …); vorausgewählt ist nur ein Teil derselben Art im Raster. Die gesetzten Teile sind benannt und dem Bauteil des Schaltplans zugeordnet, sodass ihre Luftlinien zeigen, wohin sie gehören.

Danach schlagen beide Platinen einen ersten Entwurf vor: das Lochraster mit „Bauteile anordnen…“ und „Verdrahten“ (nächster Abschnitt), die Leiterplatte mit „Bauteile automatisch anordnen…“ und „Alle Luftlinien routen“ im Menü Extras ([Abgleich mit dem Schaltplan](modules/pcb.md#abgleich-mit-dem-schaltplan)).

### Bauteile anordnen im Lochraster

„Bauteile anordnen…“ (Lochraster, Menü Prüfen, mit einem Schaltplan im Projekt) schlägt für die Bauteile einer Platine, die Bauteilen des Schaltplans zugeordnet sind, Lage und Drehung vor. Eine Übersicht nennt vorher die Zahl der Teile, die Länge der Verbindungen und die Kreuzungen, jeweils mit dem Stand vorher, und was liegen bleibt; „Anordnen“ setzt den Vorschlag in einem Rückgängig-Schritt, mit „Danach verdrahten“ samt den Drähten. „Verdrahten“ (ebenfalls im Menü Prüfen) zieht die Drähte allein, auch für eine von Hand angeordnete Platine. Beides ist ein Anfang, den der Benutzer verwirft (Rückgängig) oder von Hand weiterbearbeitet. Drähte, Leiterbahnen und andere Objekte bleiben, wo sie sind; die Anordnung gehört deshalb vor das Verdrahten, und Verdrahten verbindet jedes Netz ganz, auch wenn schon Drähte liegen (die nur ihre Löcher sperren).

Beides geht nur auf einer Lochrasterplatine mit einzelnen Lötpunkten: OpenLochs eigene Platine ohne Vorlage oder eine Vorlage, deren Löcher je einen eigenen Lötpunkt haben (wie die Beispiele). Auf Streifenraster oder größeren Kupferflächen würden Anschlüsse über das Kupfer verbunden; dort lehnen beide Befehle mit einer Meldung ab.

**Regeln, nach dem Muster einer von Hand angeordneten Kippstufe (zwei LEDs, vier Widerstände, zwei Kondensatoren, zwei Transistoren, Batterieclip, acht Netze, eine Drahtbrücke):**

1. **Schienen:** Das Netz der Versorgung (Netzname wie `VCC`, `VDD`, `V+`, `+5V`, `+UB`, sonst das Netz am Anschluss `+` einer Batterie oder Spannungsquelle, bei einer Batterie mit den Anschlüssen 1 und 2 der Anschluss 1) läuft über die Lochreihe direkt über allen Bauteilen, Masse (`GND`, `0V`, `Masse`, `VSS` oder der Anschluss `-`) über die Reihe direkt darunter. Beide Reihen liegen auf der Platine, die oberste und unterste Lochreihe der Platine bleiben frei.
2. **Höhe:** Die Plus-Anschlüsse eines Bauteils an der Versorgung liegen über allen anderen Anschlüssen, die Masse-Anschlüsse eines Bauteils an Masse unter ihnen; Bauteile ohne Schiene liegen dazwischen. Ein Bauteil an beiden Schienen (eine Batterie, ein Abblockkondensator) spannt sich dazwischen und ist von dieser Regel ausgenommen.
3. **Spiegelgleich:** Bauteile derselben Art, die im Graphen der Netze dieselbe Stellung haben (Farbverfeinerung über Bauteile und Netze), bilden Paare und liegen spiegelgleich zu einer gemeinsamen senkrechten Achse; ein Paar dreht sich so, dass seine Anschlüsse gespiegelt liegen, und wenn das nicht geht (ein Transistor, dessen Anschlussfolge sich nur durch Spiegeln umkehren ließe), liegt es gleich gedreht.
4. **Raster:** nur Drehungen um 90°, jeder Anschluss in einem Loch, die äußere Lochreihe der Platine frei. Ein Teil, dessen Anschlüsse nicht ganze Lochabstände auseinander liegen, bleibt liegen und wird umgangen, ebenso Teile ohne Bauteil im Schaltplan.
5. **Abstand:** Ein Bauteil belegt die Löcher seiner Anschlüsse und die Löcher unter seinem Körper (seine Zeichnung ohne Texte); zwischen den Löchern zweier Bauteile bleibt mindestens ein Loch frei, auch schräg. Was nicht auf die Platine passt, bleibt liegen und steht in der Übersicht, das größte Teil zuerst.
6. **Bewertung:** Länge in Millimetern: je Netz ein Spannbaum entlang Reihen und Spalten, Versorgung und Masse als Schiene über die Breite ihrer Anschlüsse mit einer senkrechten Leitung zu jedem Anschluss. Dazu je Kreuzung zweier Netze (auch eine Leitung über den Anschluss eines anderen Netzes) die Länge von vier Löchern und je Paar von Anschlüssen, die gegen Regel 2 zueinander liegen, zwei Löcher. Gesucht wird durch simuliertes Abkühlen mit einem Startwert in mehreren Durchgängen, danach schiebt und dreht jeder Durchgang jedes Teil oder Paar noch so lange um wenige Löcher, wie das hilft; der beste Durchgang gilt, sodass dasselbe Projekt immer denselben Vorschlag ergibt. Ohne Vorgabe laufen bei bis zu zwölf Teilen oder Paaren vier Durchgänge, sonst zwei, zusammen höchstens 240 000 Schritte (die Kippstufe in Bruchteilen einer Sekunde, ein Lauflicht mit 53 Teilen in wenigen Sekunden). Fertig angeordnet rückt alles in die linke obere Ecke.
7. **Verdrahten:** Drähte auf der Lötseite entlang der Reihen und Spalten. Versorgung und Masse liegen zuerst als Schiene auf ihrer Reihe über die Breite ihrer Anschlüsse, dann wächst jedes Netz von der Schiene oder seinem ersten Anschluss zum jeweils nächsten Anschluss (kürzester Weg über die Löcher, ein Knick kostet zwei Löcher). Ein Draht läuft nie durch ein Loch eines anderen Netzes, eines Anschlusses ohne Netz oder eines schon liegenden Drahts. Kommt ein Netz nicht vorbei, springt eine Drahtbrücke auf der Bestückungsseite gerade über Drähte anderer Netze, nie über einen Anschluss (sie kostet acht Löcher und ihre Länge). Netze ohne Schiene laufen kürzeste zuerst; drei Reihenfolgen werden versucht, es gilt die mit den wenigsten offenen Anschlüssen, dann Brücken, dann Länge. Ein Draht verbindet nur an seinen Enden: Drähte enden deshalb an jedem Knick, jeder Abzweigung und jedem Anschluss, über den sie laufen würden. Lötpunkte setzt Verdrahten nicht; der Durchgangsprüfer verbindet einen Lötpunkt mit jedem Draht, der über ihm liegt, also auch mit einer Brücke über die Verbindungsstelle eines anderen Netzes. Was offen bleibt (Anschlüsse von Teilen außerhalb des Rasters, Netze ohne Weg), meldet ein Hinweis mit je einem Anschluss des Netzes.

**Schnittstelle:** `src/autoplace.h`: `arrangeParts(board,targets,options)` gibt ein `Arrangement` (je Teil Kennung, Lage des Bezugspunkts und Winkel; was liegen bleibt; Länge und Kreuzungen; die Reihen von Versorgung und Masse), `applyArrangement` setzt es (neue Teile über Lage und Winkel, importierte über ihre Verschiebung und Änderung), `scoreArrangement` bewertet die Platine, wie sie ist, mit demselben Maß, `supplyNets` findet Versorgung und Masse, `routeNets` gibt die Drähte (`Routing`: Drähte, offene Anschlüsse, Brücken, Länge), `singleHoleGrid` prüft die Lötpunkte.

**Prüfung** (`openloch_tests`, `openloch_window_tests`): die Kippstufe aus den offenen Bibliotheken, von Hand angeordnet wie im Muster, wird angeordnet: alle Anschlüsse im Raster und nicht in der äußeren Lochreihe, kein Loch doppelt, keine Körper übereinander, die Reihen von Versorgung und Masse über und unter allen Anschlüssen, LEDs und Widerstände über den Transistoren, die Hälften spiegelgleich, die Länge höchstens 20 % über der Handanordnung (sie liegt deutlich darunter, weil der Batterieclip von Hand über den Rand ragt und lange Zuleitungen hat); derselbe Vorschlag bei jedem Lauf; ein Elko mit Anschlüssen außerhalb des Rasters bleibt liegen; auf einer zu kleinen Platine bleibt der Batterieclip liegen. Verdrahtet, angeordnet wie von Hand: alle acht Netze verbunden, keine zwei Netze verbunden, höchstens zwei Brücken, jeder Draht entlang der Löcher und ohne Anschluss in seiner Mitte; mit einem Elko außerhalb des Rasters bleiben genau dessen Anschlüsse offen, ohne dass Netze zusammenkommen; eine Vorlage aus Streifen gilt nicht als einzelne Lötpunkte. Das veröffentlichte Beispiel `01-Zweitransistor-Blinklicht.LM4` ohne seine Drähte wird mit den Netzen seiner Netzliste angeordnet und verdrahtet und stimmt danach mit ihr überein. Im Fenster: erst die Übersicht, Abbrechen ändert nichts, Anordnen ist ein Rückgängig-Schritt, Verdrahten auch, und Anordnen mit „Danach verdrahten“ ist ein einziger.

## Frontplatte und Platine

Hinter einer Frontplatte können Platinen des Projekts sitzen. Die Lage speichert die Frontplatte selbst (Feld `boards` ihrer Platte: Dokument und Platine über ihre Kennungen, Versatz, Drehung und Seite), damit sie mit der Frontplatte bearbeitet und rückgängig gemacht wird; Bohrungen und Symbole von Bauteilen tragen die Kennung des Bauteils (`component`). Was die Frontplatte über die Platinen weiß, liefert die Suite bei jeder Anfrage neu (`PanelEditor::boardSources`, `src/suite/boardsources.*`), aus offenen Fenstern mit ihrem aktuellen Stand:

- jede Platine jedes Lochraster- und Leiterplattendokuments mit der Kennung des Dokuments und der Platine, einem Namen (Dokument, bei mehreren Platinen „Dokument – Platine“) und ihrer Größe in Millimetern, von der Bestückungsseite gesehen, Ursprung oben links;
- ihre Bauteile mit Kennung (die Verknüpfung zum Schaltplan, sonst die eigene), Bezeichner, Wert, Bibliotheksname bzw. Gehäuse, Seite, Umriss ohne Texte und Mitte. Im Lochraster ist die Mitte die der Zeichnung ohne Texte, bei eigenen Bauteilen ohne Zeichnung (Widerstände, Dioden) die ihrer Anschlüsse; in der Leiterplatte der Pick-and-Place-Mittelpunkt.

Ein älteres Dokument ohne Kennungen erhält sie dabei einmal und behält sie im Projekt, damit eine Frontplatte seine Platinen und Bauteile dauerhaft nennen kann. Die Leiterplatte liest die Suite nur über die öffentliche Schnittstelle ihres Modells (`pcb::fromJson`, `components`, `pickPlaceCentre`, `componentOnTop`, `bounds`). Was die Frontplatte daraus macht (Platinen dahinter, Unterlage, Bohrungen, Vergleich), beschreibt [Frontplatte](modules/frontpanel.md).

## Zoom und Touchpad (alle Module)

Gilt für jede Zeichenfläche (Lochraster, Leiterplatte, Frontplatte, Schaltplan) gleich:

- **Pinch** (zwei Finger auseinander oder zusammen) zoomt um die Mausposition.
- **Zwei-Finger-Scrollen** verschiebt die Ansicht, zoomt nicht.
- **Strg (macOS ⌘) und Scrollen** zoomt um die Mausposition, auch mit einer Maus.
- **Mausrad ohne Taste** wie im jeweiligen Vorbild: in Schaltplan und Leiterplatte zoomt es um die Mausposition wie in sPlan und Sprint-Layout, in Lochraster und Frontplatte verschiebt es. Ein Touchpad, das mit Pixelschritten scrollt, verschiebt überall.
- Der Punkt unter der Maus bleibt stehen, soweit die Bildlaufleisten es zulassen (eine ganz eingepasste Platte hat am Rand keinen Platz zum Verschieben).

Ein Pinch ist ein Ereignis mit einem Zoomfaktor (`QNativeGestureEvent`, `ZoomNativeGesture`); die Ansicht rechnet Faktor und Mausposition wie beim Mausrad.

Zoomgrenzen und Schrittweite bleiben die des jeweiligen Vorbilds: Der Schaltplan folgt sPlan, das Lochraster LochMaster 4, die Frontplatte FrontDesigner 3 und die Leiterplatte Sprint-Layout. Ein Pinch verändert den Zoom stetig, ein Schritt des Mausrads die Stufe des Vorbilds.

## Lange Listen in Dialogen (alle Module)

Ein großes Projekt (ein übernommener Schaltplan mit Dutzenden Bauteilen und noch leerer Platine) darf keinen Dialog höher als den Bildschirm machen. Regeln, mit `src/longlists.h` für das Programm und jedes Modul:

- Was mit dem Projekt wächst, steht in einer Liste oder einem Bereich, der rollt (`reportList`, `scrolling`): Befunde, eine Zeile je fehlendes Bauteil, eine Zeile je Anschluss.
- Gleichartige Zeilen stehen unter einer fetten Kopfzeile mit ihrer Zahl und dem nächsten Schritt, darunter eingerückt die Bauteile: „67 Bauteil(e) fehlen auf der Platine – Prüfen → Fehlende Bauteile setzen…“.
- Ein Dialog wird höchstens so groß wie der verfügbare Bereich des Bildschirms seines Fensters, abzüglich Rahmen und Titelleiste (`fitToScreen`, `screenRoom`); die Knöpfe bleiben immer sichtbar.
- Meldungsfenster nennen höchstens acht Kennungen im Text, den Rest gezählt („… und 59 weitere“), die ganze Liste unter „Details“ (`listInBrief`).

Im Lochraster gilt das für „Aus Schaltplan übernehmen“, „Fehlende Bauteile setzen“, „Anschlüsse zuordnen“, die Übersicht von „Bauteile anordnen…“, die Meldung offener Anschlüsse nach „Verdrahten“ und die Liste „Vergleich mit Schaltplan“ (fehlende und überzählige Bauteile je unter einer Kopfzeile); in der Leiterplatte für „Aus Schaltplan übernehmen“, „Fehlende Bauteile setzen“, „Anschlüsse zuordnen“, den Bericht von „Alle Luftlinien routen“, die Hilfezeile nach „Bauteile automatisch anordnen“ (`listInBrief`) und die Liste des Schaltplanvergleichs (fehlende und überzählige Bauteile je unter einer Kopfzeile); in der Suite für die Projektübersicht. Ein Fenstertest öffnet „Aus Schaltplan übernehmen“ und „Fehlende Bauteile setzen“ mit 200 fehlenden Bauteilen auf dem Bildschirm der Testplattform (800 × 600) und verlangt, dass beide hineinpassen und ihre Knöpfe sichtbar sind.

## Dateidialoge und Bibliotheken

Jedes Modul setzt seinen Teil über die folgenden Schnittstellen um; alle vier sind angebunden.

### Öffnen in jedem Fenster

**Verhalten.** „Datei → Öffnen“ jedes Modulfensters bietet zuerst die Dateiarten des eigenen Moduls an, wie sein Vorbild, dazu „Alle Dokumente“ und die Arten der anderen Module. Die gewählte Datei öffnet die Suite nach ihrer Regel: Ein Fenster mit einem unbenannten, unveränderten Dokument derselben Art übernimmt sie, sonst öffnet sich das passende Fenster (ein Projekt mit seinem aktiven Dokument). So öffnet jedes Fenster auch Projekte der Suite (`.openloch` Version 2, ebenso eine zum Projekt gewordene `.olfp`), die sein eigener Leser nicht kennt.

**Suite.** Ein Dialog für alle: `QFileDialog` mit `documentFilter()`, der Eintrag der Art des aufrufenden Fensters zuerst. Startordner ist der vom Modul übergebene, sonst der zuletzt benutzte (`dialogs/lastDirectory`), sonst `Dokumente/OpenLoch`, sonst der Ordner des Benutzers.

**Schnittstelle je Modul.** Nach dem Muster von `saveHandler` hat jeder Editor ein `std::function<bool(const QString &folder)> openHandler`, das nur die Suite setzt. Ist es gesetzt, ruft die Aktion „Öffnen“ des Editors es mit ihrem eigenen Startordner auf, statt selbst einen Dialog zu zeigen; der Rückgabewert sagt, ob etwas geöffnet wurde. Ohne Suite (eigene Programme der Module, Tests) zeigt der Editor seinen eigenen Dialog.

| Modul | Editor | Startordner, den das Modul übergibt |
| --- | --- | --- |
| Lochraster | `openloch::Window::openHandler` | `startDirectory()` |
| Frontplatte | `frontpanel::PanelEditor::openHandler` | `startFolder()` |
| Schaltplan | `schematic::Editor::openHandler` | Zeichnungsordner der Einstellungen (`schematic/drawingFolder`), sonst leer |
| Leiterplatte | `pcb::Editor::openHandler` | Arbeitsordner „Layouts“ (`startFolder(Layouts)`, auch mit „ein Ordner für alles“) |

**Prüfung** (`tests/suite_tests.cpp`): eine Schaltplandatei, gewählt im „Öffnen“ des Lochraster-Fensters, landet in einem Schaltplanfenster; ein Projekt der Suite, gewählt im „Öffnen“ des Lochrasters und der Frontplatte, öffnet sich als Projekt; der Filter beginnt mit der Art des Fensters und enthält alle Arten.

### Ordner

Eigene Ordnereinstellungen eines Moduls, die seinem Vorbild folgen, haben Vorrang: die fünf Arbeitsordner der Leiterplatte samt „ein Ordner für alles“, Zeichnungs-, Formular- und Exportordner des Schaltplans, Skalen- und Ausschnittordner der Frontplatte. Wo ein Modul keine hat, gilt der zuletzt benutzte Ordner der Suite. Neue Vorgaben liegen unter `Dokumente/OpenLoch`. Die Dateidialoge bleiben die des Systems.

### Bibliotheken

**Verhalten.** Im Menü „Fenster“ neben der Projektübersicht öffnet „Bibliotheken…“ eine Übersicht mit einer Zeile je Dokumentart: der eigene, beschreibbare Bibliotheksordner und weitere, nur gelesene Ordner, je mit „Im Dateimanager zeigen“; der eigene Ordner lässt sich ändern, weitere Ordner hinzufügen und entfernen, wo das Modul sie kennt. Ändert sich etwas, lesen die offenen Fenster ihre Bibliotheken neu.

**Feste Ordnernamen.** Jede Art hat einen festen Ordner unter `Dokumente/OpenLoch`, unabhängig von der Sprache der Oberfläche: `Bauteile` (Lochraster), `Frontplattensymbole` und `Strichschriften` (Frontplatte), `Schaltplan-Bibliothek` (Schaltplan), `Makros` (Leiterplatte, solange kein Makroordner eingestellt ist). Ein Bauteilordner, der nach der Sprache der Oberfläche benannt ist (`Components`, `Composants`), wird weiter benutzt, solange es den festen nicht gibt.

**Schnittstelle je Modul.** Ein gemeinsamer Typ `openloch::LibraryFolders {QString own; QStringList extra;}` (in `src/documents/`) und je Modul zwei freie Funktionen, die die bestehenden Einstellungen lesen und schreiben, dazu ein Aufruf zum Neulesen im Editor:

| Modul | Lesen / Schreiben | Einstellungen dahinter | Neu lesen |
| --- | --- | --- | --- |
| Lochraster | `openloch::componentLibraryFolders()` / `setComponentLibraryFolders()` | `library/componentFolder`; weitere: die eingebundene LochMaster-Installation (`lochmaster/path`) | `Window::librariesChanged()` |
| Frontplatte | `frontpanel::symbolLibraryFolders()` / `setSymbolLibraryFolders()`, ebenso `strokeFontFolders()` / `setStrokeFontFolders()` | `frontpanel/libraryFolder`, `frontpanel/extraLibraries`, `frontpanel/strokeFontFolders` | `PanelEditor::librariesChanged()` |
| Schaltplan | `schematic::libraryFolders()` / `setLibraryFolders()` | `schematic/libraryFolder`; weitere: `schematic/extraLibraryFolders` | `schematic::Editor::librariesChanged()` |
| Leiterplatte | `pcb::libraryFolders()` / `setLibraryFolders()` | `macroFolder` in den Einstellungen des Moduls; weitere: `macroFolders/extra` | `pcb::Editor::librariesChanged()` |

**Prüfung:** Die Übersicht zeigt eine Zeile je Art mit den Ordnern aus diesen Funktionen; ein geänderter Ordner kommt in der Einstellung des Moduls und im offenen Fenster an.

## Bauteilregister (Entwurf)

Dieser Abschnitt ist ein Entwurf und legt noch nichts fest. Er beschreibt, wie die Suite Bezeichner und Werte gemeinsamer Bauteile in allen Dokumenten eines Projekts gleich hält, nach der Regel [Dateiformate und Projekte](file-formats.md) und dem Feld `components` des [Projektformats](openloch-project-format.md#bauteile), mit den Haken, die jedes Modul dafür bereitstellt.

### Verhalten

- **Was gleich gehalten wird:** zunächst Bezeichner (wie angezeigt) und Wert. Beschreibung, Felder und logische Anschlüsse stehen schon im Register, werden aber erst in einem zweiten Schritt abgeglichen.
- **Welche Bauteile:** jedes Bauteil, das ein Dokument nennt, unter der Kennung, über die das Projekt verbindet: bei einem verknüpften Platinenbauteil die Kennung des Schaltplanbauteils (Feld `component`), sonst die eigene; bei einer Bohrung der Frontplatte die des Platinenbauteils. Children eines Parents im Schaltplan sind kein eigenes Bauteil (wie bei den [Soll-Verbindungen](#soll-verbindungen-aus-dem-schaltplan)), auch nicht mit eigenem Bezeichner ohne `<PARENT_ID>`; sie werden nie abgeglichen. Bauteile in Gruppen und Clips zählen, Bauteile innerhalb eines Bauteils nicht. Doppelte Bezeichner sind im Register erlaubt (Kopieren, Duplizieren, Blatt kopieren oder laden ergeben neue Kennungen mit gleichem Bezeichner); die Stückliste zählt sie einzeln.
- **Ändern:** Ändert der Benutzer in einem Dokument Bezeichner oder Wert eines Bauteils (Bearbeiten, Rückgängig, Wiederholen), übernimmt die Suite den neuen Stand ins Register und gibt ihn sofort an die anderen Dokumente des Projekts weiter: an geöffnete über ihren Editor, an geschlossene in ihren gespeicherten Daten. Das Projekt gilt danach als geändert. Die letzte Änderung gilt; gleichzeitige Änderungen gibt es in einem Programm mit einer Oberfläche nicht.
- **Rückgängig:** Die Weitergabe ist in den anderen Dokumenten kein eigener Schritt. Damit ein Rückgängig dort keinen alten Bezeichner zurückholt, gilt der Stand aus dem Register auch in allen dort aufbewahrten Rückgängig- und Wiederholen-Schritten (alle vier Editoren bewahren ganze Stände auf). Rückgängig im Dokument, in dem geändert wurde, ist eine neue Änderung und wird ebenso weitergegeben.
- **Öffnen:** Das Register gilt. Weicht die Kopie eines Dokuments ab (etwa nach einem Import), erhält es beim Öffnen den Stand des Registers; die Projektübersicht nennt die angeglichenen Bauteile. Fehlt das Register (ältere Projekte), entsteht es beim Öffnen aus den Dokumenten, zuerst aus dem Schaltplan, dann aus den übrigen in der Reihenfolge des Projekts; wie dabei abweichende verknüpfte Bauteile behandelt werden, steht unten zur Entscheidung.
- **Verknüpfen:** Wird ein Platinenbauteil einem Schaltplanbauteil zugeordnet („Aus Schaltplan übernehmen“), ersetzt die Suite in den übrigen Dokumenten Verweise auf seine bisherige Kennung durch die des Schaltplanbauteils (etwa das Feld `component` der Bohrungen einer Frontplatte); der alte Eintrag entfällt beim Speichern. Sonst stünde eine Bohrung nach dem Verknüpfen ohne ihr Bauteil da.
- **Bezeichner wie angezeigt:** Schaltet der Schaltplan „Bauteile mit Seitennummer“ um, ändert er den Präfix, werden Blätter umsortiert, eingefügt oder gelöscht oder kommt ein Bauteil auf ein anderes Blatt, ändern sich die angezeigten Bezeichner ohne Bearbeitung des Bauteils; das ist gewollt und geht wie jede Änderung an alle Dokumente, Leiterplatte und Frontplatte zeigen dann etwa „=A-2R1“. Beim Setzen zieht der Schaltplan Präfix und Blattnummer wieder ab. Umgekehrt weist der Schaltplan einen Bezeichner ab, dessen Präfix und Blattnummer nicht zu seinem Blatt passen (eine Platine benennt „2R1“ in „3R1“ um).
- **Neue und entfernte Bauteile:** Ein Bauteil, das ein Dokument zum ersten Mal nennt, erhält einen Eintrag mit seinem Stand. Einen Eintrag, den kein Dokument mehr nennt, lässt das Speichern weg. Platzieren in einem Dokument platziert nichts in den anderen (dafür gibt es „Fehlende Bauteile setzen“).
- **Grenzen der Dokumente:** Ein Dokument, das einen Stand nicht aufnehmen kann, behält seinen und meldet es: etwa der Schaltplan einen Bezeichner, der nicht mit Präfix und Blattnummer seines Blatts beginnt, wenn er sie zeigt („Bauteile mit Seitennummer“), und einen Bezeichner oder Wert, dessen gespeicherter Text Variablen enthält (`R<PAGENO>`, `<PARENT_VALUE>`): Angezeigt wird er aufgelöst, ein eingesetzter fester Text würde die Kopplung an Blatt oder Parent lösen. Ein Bauteil auf mehreren Platinen eines Dokuments erhält den Stand überall. Ein Platinenbauteil ohne Werttext erhält einen ausgeblendeten Werttext am Bezeichner, statt den Wert abzuweisen; sonst bliebe er dauerhaft abweichend. Die Frontplatte übernimmt nur den Bezeichner (als Namen ihrer Elemente), keinen Wert.
- **Originalformate:** Die Dokumente tragen weiter ihre Kopien; Export und Speichern in eine Datei eines Vorbilds bleiben unverändert.

### Speicherung

Das Register ist das Feld `components` des Projekts mit den Feldern aus dem [Projektformat](openloch-project-format.md#bauteile) (`id`, `designator`, `value`, `description`, `fields`, `part`, `pins`); unbekannte Felder bleiben erhalten. Die Dokumentdaten ändern sich nicht im Aufbau: Sie tragen weiter ihre Kopien von Bezeichner und Wert und, wo nötig, das Feld `component`.

### Haken der Module

Gemeinsame Typen in `src/documents/componentregister.h`:

```cpp
namespace openloch::documents {
// An entry of a document's list and of a change the suite hands on: the joining identifier, designator as shown, value.
struct ComponentValues {QString id,designator,value;bool operator==(const ComponentValues &) const=default;};
// An entry of the register (the project's field "components").
struct RegisterEntry : ComponentValues {
    QString description,part;
    QList<std::pair<QString,QString>> fields;   // name, value
    QList<std::pair<QString,QString>> pins;     // logical pin: id, name
    QJsonObject unknown;                        // fields this version does not know, kept
};
QList<RegisterEntry> readRegister(const QJsonArray &components);     // throws FormatError, naming the field
QJsonArray writeRegister(const QList<RegisterEntry> &entries);
}
```

Je Modul zwei Funktionen auf seinen Dokumentdaten, ohne Fenster; sie dienen für geschlossene Dokumente, für aufbewahrte Rückgängig-Stände und als Kern der Editor-Haken:

| Modul | Auflisten | Setzen (false, wenn das Dokument den Stand nicht aufnehmen kann) |
| --- | --- | --- |
| Schaltplan | `QList<documents::ComponentValues> schematic::projectComponents(const Document &)` (Bezeichner wie angezeigt, ohne Children) | `bool schematic::setProjectComponent(Document &,const documents::ComponentValues &)` |
| Lochraster | `QList<documents::ComponentValues> Project::projectComponents() const` (aus `components()`, alle Platinen) | `bool Project::setComponent(const QString &id,const QString &designator,const QString &value)` (vorhanden; künftig für alle Teile mit der Kennung, nicht nur das erste) |
| Leiterplatte | `QList<documents::ComponentValues> pcb::projectComponents(const Document &)` (alle Platinen) | `bool pcb::setProjectComponent(Document &,const documents::ComponentValues &)` (über `setComponentText` auf jeder Platine) |
| Frontplatte | `QList<documents::ComponentValues> frontpanel::projectComponents(const Document &)` (Bezeichner aus dem Namen, Wert leer) | `bool frontpanel::setProjectComponent(Document &,const documents::ComponentValues &)` (benennt die Elemente des Bauteils) |

Je Editor (`Window`, `schematic::Editor`, `pcb::Editor`, `frontpanel::PanelEditor`) zwei Haken nach dem Muster von `saveHandler`:

```cpp
// Set by the suite: called after every change of the document by the user (an edit, undo, redo), never after
// applyComponents. The suite then lists the components and compares them with the register.
std::function<void()> componentsEdited;
// Sets designators and values in the document shown, in every kept undo and redo state and in copies the editor keeps
// for cancelling (the schematic's state before the component editor), redraws and counts the document as changed; no
// undo step. Returns the entries the document could not take.
QList<documents::ComponentValues> applyComponents(const QList<documents::ComponentValues> &values);
```

Für alle Editoren gilt: `applyComponents` löst nie `componentsEdited` aus und läuft an den eigenen Änderungswegen vorbei (etwa `touched()` der Leiterplatte), sonst entstünde eine Schleife. Vorhandene Funktionen, die Bezeichner und Wert als eigenen Rückgängig-Schritt setzen (etwa `pcb::Editor::setComponent` für „Aus Schaltplan übernehmen“), bleiben für Aufrufe aus dem Editor; die Suite gibt Stände nur über `applyComponents` weiter.

Dazu erhält jede Rückgängig-Geschichte (`openloch::History`, `frontpanel::History` und die Schritte von Schaltplan und Leiterplatte) eine Funktion, die eine Änderung auf alle aufbewahrten Stände anwendet, etwa `void History::applyToAll(const std::function<void(Project &)> &change)`.

`suite::schematicTargets` baut dann auf `schematic::projectComponents` auf, damit der angezeigte Bezeichner nur an einer Stelle entsteht.

Die Suite (`src/suite/componentregister.*`): `Suite::componentsEdited(QMainWindow *)` vergleicht die Liste des Dokuments mit dem Register, schreibt Änderungen ins Register und gibt sie an die übrigen Dokumente weiter; beim Öffnen gleicht sie Register und Dokumente ab, beim Speichern schreibt sie das Register. Die Projektübersicht und die Stückliste (`projectParts`) lesen Bezeichner und Wert dann aus dem Register.

### Zur Entscheidung

- **Ältere Projekte ohne Register:** (a) Beim Öffnen erhalten verknüpfte Bauteile, die vom Schaltplan abweichen, still dessen Stand; das Öffnen kann dann Bezeichner auf Platinen ändern. (b) Das Register entsteht aus den Dokumenten, wie sie sind; die Projektübersicht nennt die Abweichungen, und der Benutzer übernimmt sie nach der Übersicht mit „Aus Schaltplan übernehmen“. Vorschlag: (b), weil Öffnen nichts ungefragt ändern soll.

### Reihenfolge und Prüfung

1. Gemeinsame Typen, `readRegister`/`writeRegister` und das Register beim Öffnen und Speichern (Suite), mit Tests für Übernahme älterer Projekte und erhaltene unbekannte Felder.
2. In jedem Modul die beiden Datenfunktionen und `applyToAll` der Geschichte, mit Tests im Modul: Auflisten, Setzen, abgewiesene Stände, ein Rückgängig nach `applyToAll` bringt den alten Stand nicht zurück. Die Module können das unabhängig voneinander bauen.
3. In jedem Editor `componentsEdited` und `applyComponents`.
4. Abgleich in der Suite, mit Tests über alle vier Arten: ein geänderter Bezeichner im Schaltplan erscheint in Lochraster, Leiterplatte und Frontplatte, offen und geschlossen; ein Rückgängig in einem anderen Dokument holt ihn nicht zurück; ein Rückgängig im Schaltplan gibt den alten Stand weiter; ein abweichendes Dokument erhält beim Öffnen den Stand des Registers.

## Noch offen

- Gemeinsame Bauteile nach der Regel [Dateiformate und Projekte](file-formats.md): ein Bauteilregister, das Bezeichner und Werte in allen Dokumenten in beide Richtungen gleich hält ([Entwurf](#bauteilregister-entwurf)).
