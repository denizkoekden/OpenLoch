# OpenLoch-Projektformat

Eine Datei mit der Endung `.openloch` enthält UTF-8-JSON mit Zeilenende LF. **Version 2** ist das Projekt mit mehreren Dokumenten nach der Regel [Dateiformate und Projekte](file-formats.md); **Version 1** ist das bisherige Format des Lochrasters, das im Projekt als Lochraster-Dokument weiterlebt. OpenLoch liest und schreibt Version 2 für Projekte aus Schaltplan-, Lochraster-, Leiterplatten- und Frontplattendokumenten. Bauteilregister, Bibliothek und Verknüpfungen werden gelesen und unverändert erhalten, aber noch nicht bearbeitet.

## Projekt (Version 2)

| Feld | Inhalt |
| --- | --- |
| `format` | immer `"OpenLoch"` |
| `version` | `2` |
| `id` | Kennung des Projekts |
| `title` | Titel des Projekts |
| `info` | optional: `author`, `company`, `comment` |
| `active` | Kennung des zuletzt bearbeiteten Dokuments |
| `documents` | die Dokumente, mindestens eines |
| `components` | optional: das gemeinsame Bauteilregister |
| `library` | optional: Kopien der verwendeten Bibliothekseinträge |
| `links` | optional: Verknüpfungen zwischen Dokumenten |
| `resources` | optional: Originaldateien und gemeinsame Ressourcen |

Kennungen von Projekt, Dokumenten, Bauteilen und Bibliothekseinträgen sind 32 zufällige Hexadezimalziffern, wie sie das Frontplattenmodul schon für Platten und Elemente verwendet. Sie ändern sich nie; ein kopiertes Projekt behält sie.

### Dokumente

| Feld | Inhalt |
| --- | --- |
| `id` | Kennung des Dokuments |
| `kind` | `schematic`, `perfboard`, `pcb` oder `frontpanel` |
| `name` | Name in der Projektübersicht |
| `data` | die Daten des Moduls, unverändert so, wie es sie als eigene Datei schreiben würde |
| `origin` | optional, bei importierten Dokumenten: `format` (`lm4`, `lmb`, `lib`, `lay6`, `lay`, `fpl`, `spl7`, `spl8`), `file` (Dateiname beim Import) und `resource` (Schlüssel der Originaldatei in `resources`, wenn sie erhalten bleiben soll) |

`data` trägt die eigenen Felder `format` und `version` des Moduls; Leser wählen das Modul nach `kind`, nie nach `data.format`. Ein Lochraster-Dokument trägt darin also `format: "OpenLoch"` und `version: 1`: Das ist die Version des Lochraster-Dokuments, nicht die des Projekts. Ebenso ist die Version einer Leiterplatte die ihres Moduls. Die Schemata: Lochraster im Abschnitt [Lochraster-Dokument](#lochraster-dokument-version-1), Leiterplatte wie `.olpcb` ([Moduldoku](modules/pcb.md)), Frontplatte wie `.olfp` ([Moduldoku](modules/frontpanel.md)), Schaltplan mit seinem Modul. Ressourcen eines Moduls, etwa Bilder oder eingebettete Originaldateien, bleiben in seinen Daten.

**Unbekanntes bleibt erhalten.** Ein Dokument einer Art, die das laufende Programm nicht kennt, etwa ein Schaltplan in einer Fassung ohne Schaltplanmodul, erscheint in der Projektübersicht als nicht zu öffnen und wird beim Speichern unverändert zurückgeschrieben. Dasselbe gilt für unbekannte Felder des Projekts, seiner Dokumenteinträge und Bauteile.

### Bauteile

| Feld | Inhalt |
| --- | --- |
| `id` | Kennung des Bauteils |
| `designator` | Kennung wie angezeigt, etwa `R1` |
| `value` | Wert oder Typ |
| `description` | Beschreibung |
| `fields` | weitere Felder als Paare `[Name, Wert]` |
| `part` | optional: Kennung des Bibliothekseintrags |
| `pins` | die logischen Anschlüsse: `id` (kurzer Text, eindeutig im Bauteil, etwa `1` oder `A`) und `name` |

Was in einem Dokument zu einem Bauteil gehört, nennt dessen Kennung im Feld `component`; was für einen Anschluss steht (Symbolanschluss, Lochraster-Anschluss, Pad), nennt ihn im Feld `pin`. Wo ein Modul diese Felder ablegt, entscheidet es selbst: an jedem Element, an der Bauteilgruppe oder in einer Zuordnungstabelle seines Dokuments. Seine übrigen Feldnamen bleiben; beim Lochraster etwa ist `id` weiter die Kennung `R#`. Die Zuordnung übersteht Bearbeiten, Speichern und Rückgängig; eine Kopie innerhalb eines Dokuments ist ein neues Bauteil.

Kennung, Wert, Beschreibung und Felder gelten aus dem Register. Die Dokumente tragen Kopien für ihre Darstellung und ihre Exporte; ändert ein Editor sie, gleicht die Suite Register und übrige Dokumente an. Ein Bauteil muss nicht in jedem Dokument vorkommen, ein Potentiometer etwa in Schaltplan, Platine und Frontplatte, ein Widerstand nur in Schaltplan und Platine.

### Bibliothek, Verknüpfungen, Ressourcen

- `library`: Kopien der Bibliothekseinträge, auf die Bauteile verweisen, mit ihren Darstellungen je Dokumentart und der Zuordnung der Anschlüsse. Der genaue Aufbau folgt mit der Bauteil-Schnittstelle der Module.
- `links`: Jede Verknüpfung hat eine Art (`kind`) und nennt die beteiligten Dokumente und Objekte über ihre Kennungen; sie ist für Beziehungen gedacht, die keinem einzelnen Dokument gehören. Die Lage einer Platine hinter einer Frontplatte (Versatz, Drehung, Seite) speichert dagegen die Frontplatte selbst, im Feld `boards` ihrer Platte ([Frontplatte](modules/frontpanel.md)), weil sie dort bearbeitet und mit der Frontplatte rückgängig gemacht wird; sie nennt das Dokument und die Platine über ihre Kennungen.
- `resources`: SHA-256 des Inhalts in Kleinbuchstaben → `kind` (etwa `lm4`, `lay6`, `fpl`, `spl8`, `png`), `name` und Base64-`data`. Nicht mehr verwendete Ressourcen entfallen beim Speichern.

Nicht in die Datei gehören Fenster und ihre Lage, Netze (sie folgen aus dem Schaltplan) und abgeleitete Listen wie Stückliste oder Prüfergebnisse. Ansichtseinstellungen, die ein Modul heute in seinem Dokument speichert, bleiben dort.

### Speichern

Gespeichert wird das ganze Projekt mit dem Stand aller geöffneten Dokumente: kodieren, erneut einlesen und prüfen, dann atomar ersetzen. Eine Datei darf höchstens 256 MiB groß sein.

Ein Projekt mit genau einem Dokument, das aus einer Datei eines Vorbilds stammt (`origin`), schreibt „Speichern“ über das Modul in diese Datei zurück, solange das Modul keinen Verlust meldet. Meldet es einen, oder hat das Projekt mehr als ein Dokument, wird es als `.openloch` gespeichert; die Originaldatei bleibt dann unverändert. „Exportieren“ schreibt ein Dokument in ein Format seines Vorbilds und nennt vorher, was dabei entfällt.

### Lesen und Übernahme älterer Dateien

- `version` 2: das Projekt; jedes Dokument liest sein Modul.
- `version` 1: ein Projekt mit einem Lochraster-Dokument, dessen `data` die ganze Datei ist.
- `.olpcb` und `.olfp`: ein Projekt mit einem Dokument dieser Art.
- Fehlende Kennungen von Projekt und Dokumenten werden beim ersten Lesen vergeben. Beim nächsten Speichern entsteht eine Datei der Version 2; aus `.olpcb` und `.olfp` wird dabei eine neue Datei `.openloch`, die alte bleibt liegen.
- Eine höhere `version` wird mit einer Meldung abgewiesen, ebenso ungültige Werte, mit Angabe des Feldes.

## Lochraster-Dokument (Version 1)

`.openloch` speichert ein UTF-8-JSON-Objekt mit `format: "OpenLoch"` und `version: 1`. Die Daten werden vor dem normalen Speichern erneut eingelesen und validiert, anschließend atomar über `QSaveFile` ersetzt. Die maximale Dateigröße beträgt 128 MiB.

### Aktives Dokument

`title`, `mode` (`board` oder `schematic`), `width` und `height` beschreiben das aktive Dokument. Koordinaten und Linienbreiten verwenden Hundertstel Millimeter; `diameter` verwendet Millimeter. Positive Winkel drehen im Bildschirmkoordinatensystem im Uhrzeigersinn.

`originalBase64` enthält eine unveränderte importierte Quelldatei. `sourceKind` benennt `lm4`, `lmb` oder `lib`; `sourceName` ist der ursprüngliche Dateiname. Der Objektbaum wird beim Laden aus den Originalbytes rekonstruiert und nicht zusätzlich als zweite Wahrheit gespeichert.

`libraries` ordnet SHA-256-Schlüssel den eingebetteten Quellen zu. Jeder Eintrag enthält `name`, `kind` und Base64-`data`. `boardSource` verweist bei Bedarf auf eine LMB-Unterlage. Nicht mehr verwendete Quellen werden beim Kodieren entfernt.

### Änderungen und neue Elemente

`moves` ordnet den Indizes importierter Objekte einen Versatz `[x, y]` zu. `edits` speichert Eigenschaften einschließlich Löschmarkierung, Kennung/Wert, Darstellung, Transformation, Kontur und optionalem `z` für die Zeichenreihenfolge. Die Quelldatei selbst wird dabei nicht verändert.

`additions` enthält eigene Zeichenobjekte oder Bibliotheksplatzierungen. Unterstützte Typen sind `wire`, `cut`, `pad`, `text`, `resistor`, `capacitor`, `ground`, `diode`, `component`, `rectangle`, `ellipse`, `polygon`, `polyline`, `drill` und `pin`. Platzierungen verwenden `library`, `index`, `x` und `y`; `x` und `y` sind die Lage des Ankers des Bibliotheksteils, der Mitte seiner Grenzen auf dem 2,54-mm-Raster, in der Beschriftungen mit einem aus Zeichenzahl und Höhe geschätzten Kasten zählen (gespeicherte Ecken einer LM4-Beschriftung verschieben den Anker nicht). Neue Polygon-/Konturpunkte in `points` sind relativ zu `x`/`y`; bearbeitete Legacy-Pfade in `path` bleiben in den Objektkoordinaten der Quelle. Endpunkte von Leitungen, Rechtecken und Ellipsen stehen in `x2`/`y2`.

Kennungen werden wie im Original gespeichert: `id` mit `#` (etwa `R#`) und die laufende Nummer in `group_value`, angezeigt als `R4`. `nested` enthält Werte von Bauteilen innerhalb eines Bauteils oder einer Gruppe, adressiert über die Indizes der Kinder (`"2"` oder `"2/0"`).

`angle`, `mirrorX` und `mirrorY` beschreiben Transformationen. `reference` wählt bei Bauteilen den Anschluss, der als Bezugspunkt zum ersten Anschluss wird. `filled` steuert die Füllung, `back` die Seitenzuordnung. Diese optionalen Felder fehlen in älteren Version-1-Dateien und erhalten dann ihre Standardwerte.

### Kennungen und Bauteile

`uids` ordnet jedem importierten Objekt der obersten Ebene, über seinen Index als Text, eine Kennung zu (32 Hexadezimalziffern wie im Projekt); eigene Objekte in `additions` tragen ihre Kennung im Feld `uid`. Fehlende Kennungen werden beim Lesen vergeben. Eine Kennung ist im ganzen Dokument eindeutig, auch über mehrere Platinen (`boards`): Kommt sie doppelt vor, erhält das spätere Objekt eine neue, bei mehreren Platinen das auf der späteren Platine. Kopien erhalten neue Kennungen: eingefügte und duplizierte Objekte, eine duplizierte Platine und die Platinen, die aus einer anderen Datei hinzukommen. Kennungen überstehen Bearbeiten, Speichern und Rückgängig. LM4 kennt sie nicht: Eine als LM4 gespeicherte und wieder gelesene Platine erhält neue.

Jede Platine hat ihre eigene Kennung in `boardId`, ebenso eindeutig im Dokument (kommt sie doppelt vor, erhält die spätere Platine eine neue). Über sie nennen andere Dokumente eines Projekts eine Platine, etwa eine Frontplatte die Platine dahinter. Eine duplizierte Platine und die Platinen, die aus einer anderen Datei hinzukommen, erhalten neue.

Für Projekte ist jedes Bauteil der obersten Ebene ein Bauteil des Dokuments, auf jeder seiner Platinen: eine Gruppe, die als Bauteil markiert ist, eine Platzierung aus einer Bibliothek und die eigenen Widerstände, Kondensatoren und Dioden. Sein Bezeichner ist die angezeigte Kennung (`R4` aus `R#` und der Nummer 4). Zwei Felder gehören nur dem Projekt und ändern die LM4-Datei nicht; sie stehen bei importierten Objekten in `edits`, bei eigenen am Objekt:

- `component`: die Kennung des Bauteils im Projekt, für das es steht, etwa das Bauteil eines Schaltplans. Fehlt das Feld, steht es für sich selbst mit seiner eigenen Kennung.
- `pins`: die Namen seiner Anschlüsse in der Reihenfolge seiner Anschlusspunkte, ohne einen gewählten Bezugspunkt (er ändert die Reihenfolge nicht). Fehlt das Feld, gelten die Namen seiner Anschlussdrähte (Feld `label`, wie die [offenen Bibliotheken](open-libraries.md) sie vergeben), wenn jeder einen eigenen trägt; sonst heißen sie `1`, `2`, ….

### Anmerkungen und mehrere Platinen

`notes` enthält die Anmerkungen als einfachen Text. `notesEdited` zeigt an, ob sie die Originalanmerkungen beim Legacy-Export ersetzen; dann trägt `notesRtf` das formatierte RTF (Base64, höchstens 4 MB), das unverändert ins LM4 geschrieben wird. Ältere Dateien ohne `notesRtf` exportieren den Text als einfaches RTF. Unveränderte Anmerkungen bleiben in den Originalbytes erhalten.

Bei mehreren Platinen enthält `boards` bis zu 100 eigenständige Dokumentobjekte im selben Format, jeweils ohne weitere `boards`. `activeBoard` gibt den aktiven Index an. Die Felder auf oberster Ebene entsprechen der aktiven Platine und werden beim Kodieren auch an deren Position in `boards` geschrieben. Projekt-Historie und Wiederherstellungssicherung umfassen alle Platinen. LM4 enthält alle Platinen; LMB- und LIB-Exporte verwenden die aktive Platine.

`boardSettings` enthält in OpenLoch geänderte Einstellungen einer Platine; fehlende Schlüssel behalten die Werte der Quelldatei oder die Vorgaben des Originals. `pitch` ist der Lochabstand der Einheit N, `gridMm` und `gridInch` sind die Fangraster der Einheiten mm und inch (alle in Millimetern, Vorgaben 2,54, 0,1 und 0,254). `offset` ist der Versatz zwischen Platine und Kupfer `[x, y]` in Hundertstel Millimetern, `extra` sind die Extrafelder der Platine (`[Name, angezeigt, Wert]`), `unit` die Einheit der Hauptansicht (0 mm, 1 inch, 2 N) und `view` deren Schalter `flip`, `bitmaps`, `xray`, `through` und `potentials`.

`print` enthält geänderte Druckeinstellungen einer Platine in der Form des LM4-Dokuments (`views`, `view_index`, `view_flags`, `view_integer`); sie ersetzen beim LM4-Export die Druckansichten der Quelldatei.

Wiederherstellungsdateien verwenden denselben Container und ergänzen `recoverySource` als ursprünglichen Speicherpfad. Dieses Feld gehört zur Fenster-Wiederherstellung und verändert die Zeichnung nicht.

## Lochraster-Dokument, Version 2

Version 1 beschreibt importierte Objekte über ihre Stelle in den eingebetteten LM4-Bytes (`moves`, `edits`, `nested`). Version 2 (`format: "OpenLoch-Lochraster"`, `version` 2, damit sie nicht mit dem Projekt verwechselt wird) speichert jede Platine als vollständige Liste ihrer Objekte in Zeichenreihenfolge, das unterste zuerst:

```json
{
  "format": "OpenLoch-Lochraster",
  "version": 2,
  "active": 0,
  "boards": [
    {
      "title": "Verstärker",
      "mode": "board",
      "width": 10000,
      "height": 8000,
      "notes": "",
      "notesEdited": false,
      "source": {"name": "Verstärker.LM4", "kind": "lm4", "data": "<Base64>"},
      "objects": [
        {"type": "TGruppe", "uid": "8f0c…", "id": "R#", "group_value": 4, "start": 117, "end": 2310, "children": […]},
        {"type": "wire", "uid": "2b41…", "x": 254, "y": 508, "x2": 1016, "y2": 508}
      ]
    }
  ]
}
```

- **Gelesene Objekte** sind vollständige Knoten in der Form des LM4-Lesers, alle Änderungen angewandt (Lage, Drehung, Spiegelung, Seite, Bezugspunkt, Eigenschaften, Werte verschachtelter Bauteile); die Kennung steht roh (`R#` mit `group_value`). `start` und `end` bezeichnen den gelesenen Datensatz in `source`, Bilder verweisen mit `bitmap_offset` und `bitmap_size` dorthin.
- **Platzierungen aus Bibliotheken** sind aufgelöste Gruppen in derselben Form, ihre Bilder als Base64 im Feld `bitmap`. `part` nennt den Bibliothekseintrag (`library`: SHA-256 der Bibliotheksdatei, `index`, `name`), damit das Bauteilregister ihn wiederfindet.
- **Eigene Objekte** (Draht, Leiterbahn, Lötauge, Widerstand und die übrigen Werkzeuge) bleiben in der Form der `additions` von Version 1, ohne `z`: Die Reihenfolge der Liste ist die Zeichenreihenfolge.
- Jedes Objekt trägt seine Kennung in `uid`.

Zu jeder Platine gehören ihre Kennung `id` (in Version 1 `boardId`) und die Einstellungen wie in Version 1: `title`, `mode`, `width`, `height`, `notes`, `notesEdited` und `notesRtf`, `origin` (der gesetzte Ursprung), `print`, `settings` (die Platineneinstellungen) und `layout` (eine gewählte Platinenvorlage mit Name, Art und Daten). `source` ist die Datei, aus der die Platine gelesen wurde, unverändert als Base64 mit Name und Art. Aus ihr nimmt das Schreiben, was das Modell nicht abbildet: Ansichten, Metadaten, Anmerkungen, Lesezeichen und das eingebettete Layout.

Beim Schreiben als LM4 gilt ein gelesenes Objekt als unverändert, wenn sein Knoten dem gelesenen Datensatz an derselben Stelle gleicht. Sind alle Objekte unverändert und in der gelesenen Reihenfolge und die Einstellungen die der Datei, wird `source` geschrieben, bytegleich in jeder Version. Sonst wird jedes Objekt geschrieben: ein unverändertes Objekt einer 4.07-Datei als seine gelesenen Bytes, alle übrigen aus dem Knoten; ältere Versionen werden dabei zu 4.07.

Version-1-Dokumente werden umgewandelt: Änderungen auf die gelesenen Objekte angewandt, Bibliotheksplatzierungen aufgelöst, die Reihenfolge aus `z` übernommen, Kennungen beibehalten. Ein Objekt mit Änderungen verliert dabei `start` und `end` und wird, wie in Version 1, aus seinem Knoten geschrieben.

Stand: Das Modell und sein Format sind umgesetzt (`src/board.*`) und schreiben dieselbe LM4-Datei wie Version 1; die Tests prüfen das bei jedem LM4-Schreiben, auch nach dem Weg durch das Format. Gespeichert wird noch Version 1, bis Zeichenfläche und Fenster mit dem Modell arbeiten.

## Ziel der Weiterentwicklung

Das Ziel umfasst offen dokumentierte Bauteile, Pins, Geometrie und Zuordnungen zwischen Dokumenten, klare Einheiten sowie reproduzierbare Verweise auf Bibliotheken, Simulationsmodelle und Fertigungsdaten. Schemaänderungen benötigen eine definierte neue Version und einen geprüften Migrationsweg für vorhandene Dateien.
