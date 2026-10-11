# Dateiformate und Projekte

Diese Regel gilt für alle Module von OpenLoch: Schaltplan, Lochraster, Leiterplatte und Frontplatte. Sie legt fest, in welchem Format OpenLoch arbeitet, wie die Dateien der Vorbilder behandelt werden und wie ein Projekt mehrere Dokumente mit gemeinsamen Bauteilen zusammenhält. Das genaue Schema steht in [openloch-project-format.md](openloch-project-format.md).

## Grundsätze

1. **Das eigene Format führt.** OpenLoch arbeitet mit einem offenen, dokumentierten und versionierten Format: dem OpenLoch-Projekt (`.openloch`). Ein Projekt enthält alle Dokumente eines Vorhabens, etwa den Schaltplan, die Lochrasterplatine, die Leiterplatte und die Frontplatte, dazu die gemeinsamen Bauteile und Ressourcen, in einer Datei.
2. **Die Formate der Vorbilder bleiben vollwertig.** Jedes Modul öffnet und schreibt die Dateien seines Vorbilds verlustfrei, damit Dateien weiter mit den Originalprogrammen und ihren Nutzern ausgetauscht werden können.
3. **Was über die Vorbilder hinausgeht, gibt es nur im eigenen Format:** mehrere Dokumente in einem Projekt, gemeinsame Bauteile, Netze aus dem Schaltplan, Verknüpfungen zwischen Platine und Frontplatte. Beim Schreiben in ein Originalformat entfällt das; OpenLoch sagt vorher, was verloren geht.

## Formate der Vorbilder

| Modul | Vorbild | Dokumente | Bibliotheken und weitere Dateien |
| --- | --- | --- | --- |
| Schaltplan | sPlan 7 und 8 | `.spl7`, `.spl8` | Bibliotheksseiten `.LIB`, Formblätter `.sbk` |
| Lochraster | LochMaster 4 | `.LM4`, Vorlagen `.LMB` | Bibliotheksseiten `.LIB`, Sicherungen `.BAK` und `.OLD` |
| Leiterplatte | Sprint-Layout 6 (auch die Dateiversionen 0 bis 5) | `.lay6`, `.lay` | Makros `.lmk`, Text-IO |
| Frontplatte | FrontDesigner 3 | `.FPL` | Symbolseiten `.LIB`, Strichschriften `.fhx`, Skalen `.SCL`, Ausschnitte `.CUT`, Einstellungsdateien |

Ausgaben für die Fertigung oder den Druck (HPGL, Gerber, Excellon, Bilder, PDF) sind Exporte und keine Dokumentformate.

## Öffnen und Speichern

- **Neue Dokumente** entstehen in einem OpenLoch-Projekt; „Speichern“ schreibt `.openloch`.
- **Eine Datei eines Vorbilds** öffnet sich als Dokument und merkt sich Format und Datei. „Speichern“ schreibt sie in dieses Format zurück, solange dabei nichts verloren geht.
- **Enthält das Projekt mehr**, als das Originalformat aufnehmen kann (ein weiteres Dokument, gemeinsame Bauteile, Verknüpfungen), speichert OpenLoch das Projekt als `.openloch` und fragt dafür nach dem Namen. Die Originaldatei bleibt unverändert; ins Originalformat geht es dann über „Exportieren“, mit der Liste dessen, was dabei entfällt.
- **Speichern schreibt immer das ganze Projekt**, auch aus dem Fenster eines einzelnen Dokuments heraus, mit dem Stand aller geöffneten Dokumente des Projekts.
- **Ein Projekt öffnen** zeigt seine Dokumente; das zuletzt bearbeitete öffnet sich in seinem Editor.
- **Ältere eigene Dateien** bleiben lesbar und öffnen als Projekt mit einem Dokument: `.openloch` der Version 1 als Lochraster, `.olsch` als Schaltplan, `.olpcb` als Leiterplatte, `.olfp` als Frontplatte. Beim Speichern entsteht ein Projekt. Die einfachen Schaltplanwerkzeuge des Lochrasters bleiben ein Lochraster-Dokument und werden kein Schaltplan-Dokument.

## Das OpenLoch-Projekt

Eine Datei, UTF-8-JSON mit Zeilenende LF, atomar gespeichert (schreiben, wieder einlesen, ersetzen) und beim Lesen vollständig geprüft. Das Projekt hat eine eigene Formatversion: Projekte sind Version 2, Version 1 ist das frühere Format des Lochrasters. Im Umriss:

| Teil | Inhalt |
| --- | --- |
| Projekt | Kennung, Titel, Angaben wie Autor, Firma und Kommentar, das zuletzt aktive Dokument |
| Dokumente | je Dokument eine stabile Kennung, die Art (`schematic`, `perfboard`, `pcb`, `frontpanel`), ein Name, die Schemaversion des Moduls und seine Daten; auf Wunsch die Originaldatei, aus der es stammt, für den verlustfreien Export |
| Bauteile | das gemeinsame Bauteilregister: Kennung, Wert, Beschreibung, Extrafelder, logische Pins und der Bibliothekseintrag |
| Bibliothek | Kopien der verwendeten Bibliothekseinträge, damit ein Projekt ohne die Bibliotheken des Rechners vollständig ist |
| Verknüpfungen | ausdrückliche Beziehungen zwischen Dokumenten, etwa die Lage einer Platine hinter einer Frontplatte |
| Ressourcen | Bilder, Schriften und Originaldateien, über ihren SHA-256-Wert referenziert |

Die Daten eines Dokuments sind das Dokumentschema seines Moduls. Die Formate `.olsch`, `.olpcb` und `.olfp` und das Lochraster-Dokument werden so zum Inhalt eines Projekts; nur der Behälter ändert sich. Netze werden nicht gespeichert, sondern aus dem Schaltplan abgeleitet, damit es keine zweite Wahrheit gibt. Was ein Programm nicht kennt, etwa ein Dokument einer neueren Art, bleibt beim Speichern unverändert erhalten.

## Gemeinsame Bauteile

- Ein Bauteil hat je Dokumentart eine eigene Darstellung: Schaltplansymbol, Lochraster-Bauteil, Footprint und, wenn es in der Frontplatte sitzt, ein Frontplattensymbol (die Bohrung eines Potentiometers, Schalters oder einer Buchse, der Ausschnitt einer Anzeige). Die Zuordnung der Anschlüsse ist ausdrücklich: logischer Pin, Symbolanschluss, Lochraster-Anschluss, Pad.
- Kennung und Wert stehen einmal im Projekt; jedes Dokument zeigt denselben Stand.
- Bibliothekseinträge fassen die Darstellungen zusammen. Bibliotheken der Vorbilder (LochMaster-`.LIB`, Sprint-Layout-Makros, FrontDesigner-`.LIB`, sPlan-Bibliotheken) lassen sich als Quelle einer Darstellung übernehmen; die Zuordnung wird in OpenLoch ergänzt.
- Ein Bauteil in einem Dokument zu platzieren, platziert es nicht ungefragt in den anderen; OpenLoch bietet es an.

## Arbeitsabläufe

1. Im **Schaltplan** entstehen Bauteile und Netze.
2. **Lochraster oder Leiterplatte aus dem Schaltplan:** Die Bauteile kommen mit ihrer Darstellung neben die Platine, die Netze als Soll-Verbindungen (Luftlinien). Anordnen und Verdrahten (Lochraster) bzw. Anordnen und Routen (Leiterplatte) ergeben einen ersten Entwurf; „Mit Schaltplan vergleichen“ prüft das Kupfer gegen die Netze und zeigt offene Verbindungen und verbundene Netze.
3. **Änderungen im Schaltplan** übernimmt ein Layout auf Wunsch: neue, geänderte und entfernte Bauteile. Platzierte Bauteile bleiben, wo sie sind.
4. **Rückwärts** übernimmt der Schaltplan neue Nummerierungen aus einem Layout (noch offen).
5. **Frontplatte aus der Platine:** Die Frontplattensymbole der Bauteile kommen an ihre Stelle; die Lage der Platine hinter der Frontplatte (Versatz, Drehung, Seite) ist eine Verknüpfung, sodass Bohrungen und Bauteile zusammenbleiben.
6. Eine **Stückliste** für das ganze Projekt.

Ohne Schaltplan arbeitet jedes Dokument für sich.

## Regeln für die Module

1. **Eigenes Modell zuerst.** Alles, was ein Modul zeigt und bearbeitet, steht in seinem Dokumentschema. Keine Funktion gibt es nur im Originalformat oder nur in eingebetteten Originalbytes. Beim Lochraster fehlt dafür noch ein Schritt: Version 1 adressiert importierte Objekte über ihre Stelle in den LM4-Bytes; ein vollständig eigenes Modell ist Voraussetzung für gemeinsame Bauteile.
2. **Originalformate** liegen in `src/formats/<vorbild>/`, getrennt vom Modell, und werden gelesen und geschrieben. Ein Rundlauf ist verlustfrei: Unveränderte Objekte werden möglichst bytegleich geschrieben, unbekannte Daten bleiben erhalten, beschädigte Dateien werden mit der Fundstelle abgewiesen, und es wird nichts geschrieben, was der eigene Leser nicht wieder einliest. Wo es geht, wird zusätzlich im Original geöffnet und gespeichert und verglichen; wo das Original nicht speichern kann, gilt der eigene Rundlauf.
3. **Stabile Kennungen** für Dokumente, Bauteile, Pins und Pads und alles, worauf ein anderes Dokument verweist. Sie überstehen Bearbeiten, Speichern und Rückgängig; Kopien erhalten neue.
4. **Bauteil-Schnittstelle:** Jedes Modul kann seine Bauteile mit Kennung, Wert und Pins aufzählen, ein Bauteil aus einem Bibliothekseintrag platzieren, Kennung und Wert aus dem Projekt übernehmen und zu einem Element sagen, zu welchem Bauteil es gehört.
5. **Versionen:** Jede Dokumentart hat eine eigene Schemaversion. Neue Felder bekommen eine neue Version, eine Migration und einen Test; ältere Dateien bleiben lesbar, neuere werden mit einer Meldung abgewiesen.
6. **Einheiten** bleiben je Modul wie dokumentiert. Geometrische Verweise zwischen Dokumenten gehen nur über ausdrückliche Abbildungen in Millimetern.

## Stand

Umgesetzt:

- Projektversion 2 ([Projektformat](openloch-project-format.md)) mit Behälter, Dokumenten, Bauteilen, Ressourcen und der Übernahme älterer Dateien; in der Suite Projekte öffnen und speichern, Dokumente hinzufügen, umbenennen und entfernen und eine Projektübersicht mit den Bauteilen aller Dokumente und einer Stückliste ([Suite](suite.md)).
- Stabile Kennungen und Bauteil-Schnittstelle in allen Modulen, beim Lochraster auf seinem Modell der Version 1 ([Kennungen und Bauteile](openloch-project-format.md#kennungen-und-bauteile)).
- Schaltplan mit Netzen; sPlan-Dateien, -Bibliotheksseiten und -Formblätter lesen und schreiben ([Schaltplan](modules/schematic.md)).
- Übernahme vom Schaltplan in Lochraster und Leiterplatte mit Soll-Verbindungen und Prüfung: Vergleich mit dem Schaltplan, Luftlinien, Übernahme von Bezeichner und Wert, Zuordnung der Anschlüsse, fehlende Bauteile aus der Bibliothek neben die Platine setzen, anordnen, verdrahten und routen ([Soll-Verbindungen](suite.md#soll-verbindungen-aus-dem-schaltplan), [Leiterplatte](modules/pcb.md#abgleich-mit-dem-schaltplan)).
- Frontplatte und Platine verknüpft: Die Frontplatte kennt die Platinen des Projekts, ihre Lage dahinter (Versatz, Drehung, Seite) gehört zur Frontplatte; Bohrungen entstehen aus den Bauteilen der Platine, tragen deren Kennung, wandern mit der Platine und werden mit ihr verglichen ([Frontplatte und Platine](suite.md#frontplatte-und-platine), [Frontplatte](modules/frontpanel.md)).

Offen:

- Das gemeinsame Bauteilregister, das Bezeichner und Werte in allen Dokumenten gleich hält ([Entwurf](suite.md#bauteilregister-entwurf)).
- Frontplattensymbole aus Bibliothekseinträgen statt einfacher Bohrungen.
- Das eigene Modell des Lochrasters ([Version 2](openloch-project-format.md#lochraster-dokument-version-2)) steht im Speicher und schreibt LM4; Zeichenfläche und Fenster arbeiten noch mit Version 1, gespeichert wird Version 1.
- Neue Nummerierungen aus einem Layout zurück in den Schaltplan.
