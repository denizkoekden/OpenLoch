# LochMaster-Dateiformat

Diese Beschreibung gibt den Aufbau der Dateien von LochMaster 4 (Projekte `.LM4`, Platinenvorlagen `.LMB`, Bibliotheken `.LIB`) so wieder, wie OpenLoch sie liest und schreibt. Sie dient dem Datenaustausch zwischen beiden Programmen. Gelesen wird der Reihe nach; Objektgrenzen werden nicht über Zeichenfolgensuche geraten.

## Dokument

Ein Dokument beginnt mit 41 Bytes. Byte 0 ist die Länge eines Windows-1252-Titels mit maximal 35 Zeichen; der Titel beginnt an Byte 1. Bytes 37–40 enthalten die ASCII-Versionsnummer mit Dezimalkomma. Nicht genutzte Bytes des Headers können nicht initialisierte Werte enthalten und dürfen nicht als verlässliche Metadaten interpretiert werden.

Ab Version >3.9 folgen ein getaggter Titel und die Anzahl der obersten Objekte. Jedes Objekt hat einen getaggten Klassennamen und anschließend klassenabhängige Daten. Nach der Objektliste folgen fünf getaggte Integer: Platinentyp, Breite, Höhe, X-Ursprung und Y-Ursprung. Koordinaten und Linienbreiten verwenden Hundertstel Millimeter; Bohrdurchmesser sind als Fließkommawerte in Millimeter gespeichert.

Der Rest enthält abhängig von der Version eine Metadatengruppe, Rasterweite, elf Ansichtszustände, zusätzliche Fließkommawerte, zwei rohe Punkte und längencodierte RTF-Anmerkungen. `.LM4` enthält danach ein vollständiges eingebettetes Platinendokument. Die Dateien enden mit `02 01`.

## Getaggte Werte

| Tag | Inhalt |
| --- | --- |
| `02` | Vorzeichenbehafteter 8-Bit-Integer |
| `03` | Vorzeichenbehafteter 16-Bit-Integer, little endian |
| `04` | Vorzeichenbehafteter 32-Bit-Integer, little endian |
| `05` | x87 Extended, 80 Bit, little endian |
| `06` | 1 Byte Länge, danach Windows-1252-Text |
| `08`, `09` | False, True |
| `0C` | 32-Bit-Bytelänge, danach Windows-1252-Text |
| `12` | 32-Bit-Zeichenanzahl, danach UTF-16LE |
| `14` | 32-Bit-Bytelänge, danach UTF-8 |

Die Persistenz mischt getaggte und rohe Werte. Farbangaben sind rohe 32-Bit-Delphi-TColor-Werte in BGR-Reihenfolge. Punktlisten beginnen mit der getaggten Anzahl minus eins; jeder Punkt besteht aus zwei rohen 32-Bit-Koordinaten. Eingebettete Bitmaps beginnen direkt mit einem BMP-Dateikopf. Das Original liest sie nach dem Infokopf (Kopf, Farbmasken, Farbtabelle, Bildpunkte) und schreibt bei einem 24-Bit-Bild mit Farbtabelle Dateigröße und Datenversatz um 1024 Byte zu groß. Das Ende ergibt sich deshalb aus dem Infokopf, wenn danach das nächste Feld (ein Wahrheitswert) folgt, sonst aus der Dateigröße. Zum Anzeigen setzt OpenLoch einen Datenversatz, der hinter das Ende der Bildpunkte zeigt, auf den des Infokopfs; die gespeicherten Bytes bleiben unverändert.

80-Bit-Werte werden explizit dekodiert. Ein `long double`-Cast wäre auf Apple Silicon falsch, weil dort `long double` nicht dem gespeicherten x87-Format entspricht.

## Objektklassen

| Klasse | Zusätzliche Daten nach dem gemeinsamen Basisteil |
| --- | --- |
| `TDraht`, `TDrahtFest`, `TLeiterbahn` | Punktliste und optionale zweite Punktliste |
| `TTrenner`, `TTrennerFest` | Vier rohe 32-Bit-Werte |
| `TBohrung`, `TAuge` | Durchmesser und zwei getaggte Koordinaten |
| `TGruppe` | Kinderanzahl minus eins, Bauteildaten, zwei Flags, Kindobjekte |
| `TFarbcode` | Gruppe der vier Farbringe eines Widerstands; danach die E-Reihe (0 = E6 … 4 = E96, früher als Bandanzahl gelesen) und der auf die Reihe gerundete Widerstandswert |
| `TBt` | Bauteil des Objekt-Assistenten: Gruppe und ein Byte mit dem Objekttyp (11 Widerstand, 12 Elko, 13 Kondensator, 14 Diode); die Parameter werden nicht gespeichert |
| `TKreis` | Zwei Koordinatensätze, Flag und eingebettetes Drahtobjekt. Eine Füllbitmap steht zweimal in der Datei: am Kreis und an der inneren Kontur; nur die Eckpunkte der inneren Kontur werden benutzt |
| `TTextLabel` | Textpositionen, Metriken, Text, Schrift und Versionszusätze |

Die zwei Flags einer Gruppe bedeuten: erstes Flag „ist ein Bauteil“ (sonst eine einfache Gruppe mit anderem Eigenschaftendialog), zweites Flag „Erscheint in Stückliste“. Die Stückliste des Originals sammelt alle Gruppen einschließlich verschachtelter und gibt nur die aus, bei denen beide Flags gesetzt sind. Gruppen bis Version 3.9 speichern Kennung, Wert und Beschreibung in einem festen 95-Byte-Block. Dessen letzte zwei Bytes enthalten die vorzeichenbehaftete Bauteilnummer. Neuere Gruppen speichern getaggte Zeichenfolgen und danach die getaggte Nummer (`group_value`). `R#` mit Nummer 4 wird als `R4` dargestellt; der Text hinter dem ersten `#` wird dabei durch die Nummer ersetzt. Anzeige, Stückliste und Kennungsvergabe berücksichtigen diese getrennte Speicherung.

Der Basisteil erhält zusätzliche Felder bei >3.055, >3.995 und >4.005; Textobjekte bei >3.035, >4.035 und >4.065.

Die JSON-Namen einiger Basisteil-Felder stammen aus der frühen Analyse und sind irreführend; sie bleiben aus Kompatibilitätsgründen bestehen:

| Feld | Bedeutung im Original |
| --- | --- |
| `transparent` | Gefüllt. Flächen werden nur dann gefüllt gezeichnet; bei Texten liegt die Schrift auf einem Kasten in Pinselfarbe. |
| `flag2` | Eckenglättung an |
| `style` | Glättungsart: 0 Kurve durch die Kantenmitten (quadratische Bézierkurve, 20 Stützpunkte plus Endpunkt), 1 Fase, 2 abgerundete Ecke |
| `rotation` | Eckengröße der Arten 1 und 2 in 1/100 mm (Standard 200). Nur kürzere Kanten bleiben ungekappt. |
| `flag` | Setzt der Objekt-Assistent an allen erzeugten Teilen, „Gruppe aufheben“ löscht es; genaue Bedeutung noch offen |

Arten 6 und 7 zeichnet das Original immer geschlossen, auch ungefüllt: Der erste Punkt wird wieder angehängt. Kantenmitten werden zur Null hin abgeschnitten, Abstände und Kurvenpunkte gerundet. Bei offenen Linien bleiben erster und letzter Punkt erhalten, geglättet werden nur die inneren Ecken. OpenLoch bildet das in `drawnPath()` nach.

Der Winkel eines Textes (`text_height`, ebenfalls ein Altname) ist die GDI-Escapement-Richtung im Bogenmaß und dreht gegen den Uhrzeigersinn: 270° bzw. −90° läuft nach unten, der Textkasten hängt an seiner linken oberen Ecke (`text_position`) und erstreckt sich dann nach links.

## Objekt-Assistent

Der Assistent des Originals erzeugt Konturen und Bauteile um einen Mittelpunkt. Längen gibt der Dialog in Millimetern an und rechnet sie in 1/100 mm um; gerundet wird wie in Delphi mit Halbwerten zur geraden Zahl. Konturen sind ein einzelner geschlossener Umriss (Art 7, Rechteck Art 6, Ellipse ein `TKreis` mit 16 Punkten), schwarz, 0,1 mm, ungefüllt; die Drehung wirkt gegen den Uhrzeigersinn um den Mittelpunkt mit auf ganze Einheiten gerundetem Radius. Beim Einfügen löst das Original die Gruppe auf, sodass nur der Umriss bleibt.

Bauteile sind `TBt`-Gruppen mit gesetztem Bauteil- und Stücklisten-Flag, Kennung `R#`, `C#` oder `D#`, senkrecht mit Anschluss 1 oben. Die zwei Anschlussdrähte (Art 9, Farbe C0C0C0, eingelötet am äußeren Ende) enden bei n = Round(Länge / 508 + 1) Rasterschritten über und unter der Mitte; beim Kondensator tritt das Rastermaß an die Stelle der Länge. Der Körper ist ein gefüllter, geglätteter Umriss mit Farbverlaufsbild; das Bild spannt das Original über die in `anchors` gespeicherten Ecken des umgebenden Rechtecks auf und zeichnet ohne sie nichts. Beschriftungen tragen die Platzhalter `<BauteilKennung>` und `<BauteilWertTyp>` mit 270° (nach unten laufend); der Dialog zeigt dabei feste Texte, die beim Einfügen zu Platzhaltern werden. Der Widerstand erhält eine `TFarbcode`-Gruppe mit vier Ringen in der unteren Körperhälfte: bei E6 bis E24 zwei Ziffern und Multiplikator, bei E48/E96 drei Ziffern und Multiplikator, ohne Toleranzring; Multiplikatoren 0,1 und 0,01 erscheinen gold und silber.

## Drahtarten

Die Art (`kind`) eines `TDraht` bestimmt Darstellung und elektrische Rolle:

| Art | Werkzeug im Original | Elektrische Rolle |
| --- | --- | --- |
| 1 | Drähte ziehen (Brücke, beide Enden eingelötet) | Verbindet über erstes und letztes Ende |
| 4 | Linien zeichnen | Keine |
| 6, 7 | Flächen und Umrisse (Polygone, Kreise) | Keine |
| 9 | Bauteilanschlußdraht definieren (erstes Ende eingelötet), mehrpunktig | Verbindet über den ersten Punkt |
| 11 | Anschluß-Pins setzen (z. B. IC-Pin); zwei gleiche Punkte, gezeichnet als Punkt mit der Breite als Durchmesser | Verbindet über den ersten Punkt |
| 13 | `TDrahtFest` | Keine Anschlüsse |
| 18 | Potenzial hinzufügen; Name als `label`, Farbe als Stiftfarbe | Färbt Netze, verbindet nicht |
| 19 | Lötstelle hinzufügen; Kreis, Standardbreite 150 (1,5 mm), Seite der aktuellen Bearbeitung | Siehe Lötstellenregel unten |

OpenLoch exportiert seine Werkzeuge entsprechend: Leitung → 1, Kontur → 4, Rechteck/Polygon → 6, Anschlussdraht → 9, Pin → 11, Potenzial → 18, Lötstelle → 19. Der OpenLoch-Lötpunkt bleibt ein Lötauge mit Bohrung im Dokument und leitet im Original nicht.

## Elektrisches Modell des Durchgangstesters

Der Durchgangstester des Originals arbeitet rein geometrisch, ohne gespeicherte Netzliste. OpenLoch bildet seine Regeln in `src/continuity.cpp` nach:

- **Kupfer** sind ausschließlich die Streifen (`TLeiterbahn`, Art 16) und Lötaugen (`TAuge`, Art 15) der Platinenvorlage, auch innerhalb von Gruppen. Gleichartige Objekte im Dokument leiten nicht. Ein Streifen reicht beidseitig der Mittellinie um die gespeicherte Breite und endet flach an seinen Endpunkten; ein Lötauge ist ein Kreis mit dem gespeicherten Durchmesser. Eine generierte OpenLoch-Lochrasterplatine entspricht ihrem LM4-Export: ein isoliertes Lötauge von 1,8 mm je Rasterloch.
- **Trennen:** Trennstellen (`TTrenner`) und Bohrungen über 1,5 mm teilen waagerechte und senkrechte Streifen; eine Bohrung wirkt dabei als schmales Kreuz über ihren Durchmesser. Geteilt wird nur, wenn die Trennfläche den Streifen über seine ganze Breite überdeckt. Die Teilstücke enden eine Einheit vor der Trennfläche. Schräge Streifen und Lötaugen werden nie geteilt. Eine 2,0-mm-Bohrung trennt deshalb einen 2,0 mm breiten Streifen nicht.
- **Kupfer untereinander** verbindet sich über überlappende Flächen.
- **Drähte:** Es zählen nur die Drahtarten 1 (Brücke), 9 und 11 (Bauteilanschluss), 13 und 19 (Lötstelle). Eine Brücke verbindet über ihren ersten und letzten Punkt, ein Bauteilanschluss nur über seinen ersten, eingelöteten Punkt; der Punkt muss in einem Kupferstück liegen. Ein über ein Lötauge geführter Draht verbindet es nicht. Potentialmarken (Art 18) nehmen am Durchgang nicht teil, und durch ein Bauteil hindurch wird nie weiterverbunden.
- **Lötstellen (Art 19)** verbinden auf derselben Platinenseite alles, was ihre Kreisfläche überlappt, von der anderen Seite nur Drahtenden innerhalb dieser Fläche. Auf der Kupferseite verbinden sie außerdem das Kupfer unter ihrem Mittelpunkt. Ein weiteres internes Flag dieser Regel steht nicht in der Datei und wird als gesetzt angenommen.
- **Start:** Ein Klick in eine Bohrung über 1,5 mm startet keine Suche. Sonst beginnt sie bei jedem Kupferstück unter dem ungerundeten Klickpunkt.

**Potentiale:** Potentialmarken (Art 18, Werkzeug „Potenzial hinzufügen“) tragen einen Namen im Feld `label` und eine Farbe als Stiftfarbe. Bei eingeschalteter Potentialanzeige flutet das Original von jeder Marke aus das Netz mit den Regeln des Durchgangstesters und färbt es ein. Marken werden in Dokumentreihenfolge ausgewertet, auch innerhalb von Bauteilgruppen; schwarze Marken werden übergangen. Kupfer behält die erste Farbe, die es erreicht, und eine Marke auf bereits gefärbtem Kupfer bewirkt nichts. Da die Verbindungen getrennte Netze bilden, trägt jedes Netz die Farbe der ersten Marke darauf. Eine zweite, andersfarbige Marke im selben Netz zeigt das Original nicht an; OpenLoch meldet sie in der Statuszeile als Vorstufe der Kurzschlussanzeige. Das Original speichert den Schalter „Potenziale anzeigen“ zwar je Ansicht in der Datei, wendet ihn beim Laden aber nicht an; maßgeblich ist der Programmschalter, und eingefärbt wird erst beim Umschalten oder beim Setzen einer Marke. OpenLoch behandelt den Schalter ebenfalls als Programmeinstellung, liest ihn nicht aus der Datei und färbt sofort. Pins sind Art 11 (Werkzeug „Anschluß-Pins setzen“) und werden als Punkt mit der gespeicherten Breite als Durchmesser gezeichnet.

**Freie Bereiche:** Der Werkzeugleistenknopf „Freie Bereiche anzeigen“ wirkt nur, solange er gedrückt gehalten wird. Frei ist jedes Kupferstück, an dem kein Anschluss liegt. Anschlüsse sind beide Enden einer Brücke, der erste Punkt von Bauteilanschlüssen und Pins, Potentialmarken (auch schwarze) und Lötstellen auf der Kupferseite; Art 13 hat keine Anschlüsse. Belegtes Kupfer belegt alles überlappende Kupfer, über Drähte breitet sich die Belegung nicht aus. Freies Kupfer zeichnet das Original in Dunkelgrün (`#008000`); das überdeckt Potentialfarben, die Markierung des Durchgangstesters liegt darüber. Beim Loslassen werden die Potentiale neu berechnet. Bohrungen bleiben in gefärbtem Kupfer sichtbar; dafür führt das Modell einer generierten Lochrasterplatine wie ihr LM4-Export auch eine 0,9-mm-Bohrung je Rasterpunkt.

**Kurzschlussprüfung (OpenLoch-Erweiterung, nicht im Original):** Ein Netz gilt als Kurzschluss, wenn es Marken verschiedener Potentiale verbindet: unterschiedliche Namen (ohne Groß-/Kleinschreibung), bei unbenannten Marken unterschiedliche Farben. Schwarze Marken tragen wie im Original kein Potential. Je Netz wird jedes weitere Potential gegen das erste gemeldet. Als Ort dient die kürzeste Kette leitender Teile zwischen den beiden Marken, gesucht nach denselben Verbindungsregeln wie beim Durchgangstester; Bauteilanschlüsse leiten dabei nicht weiter, Lötstellen werden für die Kette nicht berücksichtigt. Die Kette zeigt, wo die Potentiale zusammenlaufen, aber nicht, welches Teil darauf der eigentliche Fehler ist. Ohne mindestens zwei verschiedene Marken gibt es keinen Befund.

Das Original rechnet mit Bildschirmregionen in Gerätepixeln; bei sehr knappen Abständen kann sein Ergebnis daher vom Zoom abhängen. OpenLoch verwendet exakte Geometrie mit einer Toleranz von 0,01 mm. Native OpenLoch-Lötpunkte werden als Lötauge im Dokument exportiert und leiten deshalb wie im Original nicht; für Lötstellen gibt es das Werkzeug „Lötstelle“ (Art 19).

## Mehrere Platinen, Dateiende und Speichern im Original

Eine LM4-Datei enthält zuerst Platine 0 (Dokument mit eingebetteter Vorlage), dann die Platinenanzahl als getaggte Ganzzahl (`02 01` bei einer Platine) und danach die Platinen 1 bis Anzahl−1, jede als vollständiges Dokument mit eigenem Namen, eigenen Anmerkungen und eigener Vorlage. Die aktive Platine wird nicht gespeichert; nach dem Laden zeigt das Original die letzte. Eine Anzahl kleiner als 2 bedeutet eine Platine, Daten nach der letzten Platine werden ignoriert. Platinenvorlagen (`.LMB`) enden ebenfalls mit `02 01`, Bibliotheksseiten (`.LIB`) enden ohne Anzahl.

Das Original überschreibt beim Speichern die Zieldatei direkt und meldet Schreibfehler nicht; OpenLoch schreibt über eine temporäre Datei und meldet Fehler. Beim Öffnen schreibt das Original eine neu gespeicherte Kopie des Geladenen als `<Name>.OLD`. OpenLoch tut das ebenfalls, außer in schreibgeschützten Ordnern und innerhalb einer eingebundenen LochMaster-Installation. Das Original speichert automatisch nach einstellbarem Intervall als `<Name>.BAK`; OpenLoch verwendet dafür seine eigene Wiederherstellungssicherung.

## Textbeschriftungen

Ein `TTextLabel` liegt in einem Kasten aus vier Ecken: `text_position` ist die linke obere Ecke P0, `text_anchors` enthält rechts oben, rechts unten und links unten jeweils als aktuellen Punkt und als Schnappschuss. Das Original zeichnet ausschließlich aus den aktuellen Ecken: Leserichtung von P0 nach P1, Schriftgröße (Geviert) gleich der Kastenhöhe, die Grundlinie beginnt an der linken unteren Ecke um die Unterlänge angehoben, der Text wird auf die Kastenbreite gestreckt. Sind die Ecken zusammengefallen (alte Dateien, frische Assistent-Beschriftungen), legt das Original den Kasten aus dem gespeicherten Winkel (gegen den Uhrzeigersinn), der Zellhöhe `text_kind` und der gemessenen Textbreite an; das Flag „um 180° gedreht“ beginnt ihn an der gegenüberliegenden Ecke. Drehen dreht die Ecken, Spiegeln spiegelt sie und vertauscht sie (horizontal P1, P0, P3, P2; vertikal P3, P2, P1, P0), sodass die Schrift nie gespiegelt erscheint; dabei schaltet es `text_flags2` um (horizontal den ersten, vertikal den zweiten Wert), die nur gespeichert und nie gezeichnet werden. Gespeicherter Winkel und Schnappschuss bleiben dabei unverändert; der Schnappschuss dient nur dem Drehen eines Zweibeiners an einem Anschlussdraht. Verschieben verschiebt die aktuellen Ecken und, solange der Schnappschuss gilt (`text_flags[0]`), auch dessen Ecken (`text_end` und jeden zweiten Punkt in `text_anchors`); Winkel, Schnappschusswinkel und Flags bleiben. Beschriftungen aus Dateien vor 4.04 haben keine gespeicherten Ecken; das Original setzt die aktuellen auf `text_position` und die des Schnappschusses auf `text_end`. OpenLoch zeichnet und exportiert nach diesen Regeln, auch unveränderte Beschriftungen, und behandelt Ecken, die alle im Nullpunkt liegen, während die Beschriftung es nicht tut, wie fehlende; eigene Beschriftungen schreibt es mit zusammengefallenen Ecken, damit das Original die Breite selbst misst; gedreht oder gespiegelt bleiben sie zusammengefallen und tragen den neuen Winkel. Zusammengefallene Ecken legt das Original beim ersten Zeichnen an und misst die Breite dabei mit GDI auf der Zeichenfläche, also in der Auflösung der gerade eingestellten Zoomstufe (Fenster `Round(254/Zoom)` auf 16 Pixel): Bei Zoom 1 ist eine 1,5 mm hohe Schrift rund 9 Pixel hoch, und jedes Zeichen rundet um bis zu einen halben Pixel (0,08 mm). Gespeicherte Ecken misst es nicht neu. Beim Speichern schreibt es `text_metric` neu, die Textbreite bei 100 Pixel Geviert auf einem eigenen Messkontext; gelesen wird dieser Wert nie.

## Schreiber

Der Schreiber verwendet Version 4.07 für neu geschriebene Dokumente und Datensätze. Er kodiert Integer mit dem kleinsten passenden Delphi-Tag, Fließkommawerte explizit als x87 Extended und neue Zeichenfolgen als UTF-8. Der ANSI-Kompatibilitätskopf bleibt auf 35 Bytes begrenzt; der zusätzliche Unicode-Titel erhält den vollständigen Projektnamen. Beim Öffnen wird dieser Titel bevorzugt.

Ein unverändertes LM4-Projekt wird vollständig bytegenau zurückgegeben. Bei Änderungen bleiben nicht betroffene 4.07-Datensätze, eingebettete BMP-Daten und der Dokumentnachlauf einschließlich RTF und Ansichten unverändert. Alte Datensätze werden beim Neuschreiben ergänzt; unverändert eingebettete Platinenvorlagen dürfen ihre ursprüngliche Version behalten. Bibliotheksbauteile werden mit ihren Quellen serialisiert, Transformationen in ihre Geometrie übernommen.

Die Punktliste im Basisteil enthält elektrische Zwischendaten. In vorhandenen Dateien kommen sowohl Rasterindizes als auch offenbar veraltete Koordinaten vor. Vollständige vorhandene Listen bleiben erhalten; sie werden nicht als verlässlich interpretierte Anschlussgeometrie ausgegeben. Geometrische Anschlüsse sind zusätzlich aus den Drahtpfaden ableitbar.

Die **Anzahl** dieser Einträge ist beim Speichern entscheidend: Das Original verwendet die virtuelle Anschlusszahl und greift entsprechend auf die gespeicherte Liste zu. Drahtart 1 benötigt zwei Einträge, Arten 9/11/18/19 jeweils einen; Gruppen benötigen die Summe ihrer Kinder. Auch zwei geometrisch gleiche Drahtenden zählen getrennt. Neue oder unvollständige Listen werden beim Export aus den physischen Pfaden ergänzt. Native Pins werden wie die Pins des Originals geschrieben: Drahtart 11 mit zwei gleichen Punkten, denn Drähte mit weniger als zwei Punkten zeichnet das Original nicht. Art 18 ist im Original die Potentialmarke und wird nur für Potentiale verwendet. Fehlen Einträge, zeigt das Original die Datei zwar an, kann sie aber nicht zurückspeichern.

Rechteckige Trennstellen können im Legacy-Format keine beliebigen Drehwinkel darstellen; entsprechende Exporte werden abgelehnt.

`writeLegacyDocument` erzeugt LIB-/LMB-Dokumente ohne eingebettete Platine. Auswahlgruppen werden als eigene `TGruppe` mit dem Label `OpenLochSelectionGroup` gespeichert. Beim Auflösen werden Platzhaltertexte im bisherigen Bauteilkontext aufgelöst. Eine geänderte Zeichenreihenfolge wird durch die Reihenfolge der obersten Legacy-Datensätze abgebildet.

Bearbeitete Anmerkungen ersetzen ausschließlich das längencodierte RTF-Feld; unveränderte Anmerkungen bleiben erhalten.

### Anmerkungen (RTF)

Das Feld enthält RTF, wie es ein Windows-RichEdit-Feld schreibt: mit Codepage 1252, jeder Absatz einschließlich des letzten mit `\par` abgeschlossen, danach `}`, CR LF und ein NUL-Byte, das in der Länge mitzählt. Leere Anmerkungen des Originals: `{\rtf1\ansi\ansicpg1252\deff0\deflang1031{\fonttbl{\f0\fnil Tahoma;}}` CR LF `\viewkind4\uc1\pard\f0\fs16\par` CR LF `}` CR LF NUL. OpenLoch liest und schreibt davon Schriftarten (`\fonttbl`, `\f`), Größen (`\fs`), `\b`, `\i`, `\ul`/`\ulnone`, Farben (`\colortbl`, `\cf`), Ausrichtung (`\ql`, `\qc`, `\qr`, `\qj`), Einzüge (`\li`, `\fi`, `\ri`), Aufzählungen in der RichEdit-Form `{\pntext\f1\'B7\tab}{\*\pn\pnlvlblt\pnf1\pnindent0{\pntxtb\'B7}}` mit Symbol-Schrift, `\tab`, `\line`, `\'xx` und `\uN`.

„Stückliste einfügen“ (Modus 0) und „Einkaufsliste einfügen“ (Modus 1) erzeugen: fett in Größe 20 `Stückliste für <Platine>` bzw. `Einkaufsliste für <Platine>`; in Größe 10 eine Leerzeile, `Projekt: <Datei>`, `Erstellt am <TT.MM.JJJJ> um <hh:mm:ss>` (Datums- und Zeitformat des Systems), `von <Benutzer>` (unter Windows registrierter Besitzer und Organisation), Leerzeile. Die Stückliste enthält jedes Bauteil mit „Erscheint in Stückliste“ in der Reihenfolge: Bauteile vor Bauteileinheiten, dann Kennung `R#`, `C#`, `D#`, `T#`, übrige Kennungen in der Reihenfolge ihres Auftretens, innerhalb gleicher Kennung nach Nummer; Zeile `Kennung⇥Beschreibung, Wert` (ohne Kennung `-`), Bauteileinheiten als `>>> Kennung - Beschreibung, Wert <<<` mit einer Leerzeile davor und ihren Teilen um einen Tabulator eingerückt. Die Einkaufsliste löst Einheiten auf und fasst gleiche Kennung, Wert und Beschreibung zusammen: Kennungen mit Komma getrennt (nach jeweils zehn eine neue Zeile), dann `<n>x⇥Beschreibung, Wert` und eine Leerzeile. Beide enden mit Leerzeile, fett `Drahtbrücken:`, Leerzeile und je Draht der Art 1 mit verschiedenen Endlöchern `(x/y)⇥(x/y)⇥Draht; (L=… mm Lochabstand n)`, Löcher ab dem Ursprung im Abstand 2,54 mm, gerundet zur nächsten geraden Zahl bei Gleichstand. Das eigene [OpenLoch-Projektformat](openloch-project-format.md) speichert die bearbeitbaren Eigenschaften einschließlich mehrerer Platinen getrennt von den ursprünglichen Legacy-Bytes.

## HPGL-Bearbeitungsdateien

Der Export arbeitet wie im Original auf einer Kopie der aktiven Platine, die als LM4 geschrieben und wieder gelesen wird. Gesammelt werden die Objekte der Hauptliste (nicht der Vorlage) nach Art in der Reihenfolge 3 (Trennstellen), 14 (Bohrungen), 5 und 6 (Kreise, Rechtecke), 7 (Polygone), 4 (Linien); Gruppen werden an Ort und Stelle durchlaufen. Fräsungen sind alle übrigen Objekte mit „Kontur fräsen“. Bohrungen werden nach Durchmesser, Fräsungen und Trennstellen nach Breite mit dem einfachen Vertauschungssortieren des Originals geordnet.

Jobs: `Bohren mit <d> mm` je Durchmesser oder ein Job `Bohrungen fräsen mit <T> mm`, `Fräsen mit <b> mm` je Breite, `Trennstellen` (je Breite ein Job gleichen Namens, die breitesten bleiben als Datei übrig, immer Stift 1) und `Aussenrechteck mit <b> mm`. Zahlen wie Delphis `FloatToStr` mit Dezimalkomma; das Werkzeug für ausgefräste Bohrungen wird auf den kleinsten Bohrdurchmesser begrenzt und als float32 aus dem Textfeld gelesen. Jede Datei `<Job>.PLT` enthält je Zeile einen Befehl mit CR LF: `IN;`, `SP1;` oder `SP2;` (Layer 2), `PT0;`, `PU;`, dann `PA`/`PD`/`PU` und am Ende `PU;` (falls abgesenkt) und `PA0,0;`. Koordinaten in 1/40 mm ab dem Ursprung ohne Spiegelung: `X = Round((x − Ursprung.x) · 0,4)`. Eine Bohrung ist `PAx,y;` `PD;` `PAx,y;`, eine Trennstelle ihr Rechteck, eine Fräsung ihr Pfad. Geglättete Konturen folgen dem Pfad des Originals (Mitten mit Abschneiden zur ersten Ecke, Abstand mit `Round`, quadratische Kurven mit 20 Abtastpunkten und Endpunkt) in x87-Genauigkeit mit 64-Bit-Mantisse und Rundung zur geraden Zahl. Ausgefräste Bohrungen sind 16-Ecke mit Glättung; ihr Radius entsteht wie im Original aus `y + d/2 − 50·T` mit `d` in mm, ist also etwa der Werkzeugradius. Das Aussenrechteck liegt um die halbe Werkzeugbreite außerhalb von `[0, Breite] × [0, Höhe]` und wird offen über drei Seiten gefahren. „Gemeinsamer Ursprung“ setzt am Dateianfang `PA0,0;` `PD;` `PU;`, mit Aussenrechteck `PA−p,−p;` mit `p` aus der halben Werkzeugbreite.

