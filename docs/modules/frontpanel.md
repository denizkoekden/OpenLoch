# Frontplatte (Modul `frontpanel`)

Das Modul gestaltet Frontplatten für Gehäuse: Konturen, Beschriftungen, Skalen, Bohrungen, Gravuren und Fräsungen. Es hat ein eigenes Modell, ein eigenes Dateiformat und einen Editor, liest und schreibt die Projekt- und Bibliotheksdateien von FrontDesigner 3 (`.FPL`, `.LIB`) und gibt Bohrungen, Fräsungen und Gravuren als HPGL-Bearbeitungsdateien aus. Die Suite öffnet den Editor als eigenes Fenster ([Suite](../suite.md)); `openloch_frontpanel_demo` startet ihn zusätzlich als eigenständiges Programm.

## Aufbau im Code

| Pfad | Inhalt |
| --- | --- |
| `src/modules/frontpanel/frontpanel.*` | Modell (`Document`, `Panel`, `Element`), natives Format |
| `src/modules/frontpanel/panelboards.*` | Bauteil-Schnittstelle: Objekte mit Bauteil, das Bauteil eines Objekts und seine Stelle |
| `src/modules/frontpanel/panelgeometry.*` | Konturglättung, Ellipsen und Bögen unter Abbildungen, Textrahmen, Umrisse, Trefferprüfung |
| `src/modules/frontpanel/panelrender.*` | Darstellung mit `QPainter` für Bildschirm, Bildexport und Druck |
| `src/modules/frontpanel/emf.*` | Wiedergabe von Vektorgrafiken im EMF-Format; der Export verwendet den gemeinsamen Schreiber `src/emfwriter.*` |
| `src/modules/frontpanel/strokefont.*`, `strokefontown.cpp` | Einlinige Strichschriften (SHX, SHP, FHX) für Gravuren und die eigene Normschrift |
| `src/modules/frontpanel/panelhistory.*` | Rückgängig/Wiederholen über Dokumentschnappschüsse |
| `src/modules/frontpanel/panelgenerators.*` | Bemaßungen, Skalen, Frontplattenausschnitte, regelmäßige Vielecke |
| `src/modules/frontpanel/panelscale.cpp` | Skalen: Parameterlisten der Stile, Aufbau, Skaleneinstellungen (`.SCL`) |
| `src/modules/frontpanel/panelmachining.*` | Bearbeitungsjobs und HPGL-Ausgabe |
| `src/modules/frontpanel/panelprint.*` | Druckvorschau und Druck |
| `src/modules/frontpanel/paneleditor.*` | Editorfenster mit Menüs, Werkzeugleisten, Plattenreitern und Objektbaum |
| `src/modules/frontpanel/panelview.*` | Arbeitsfläche mit Linealen, Raster und den Zeichen- und Bearbeitungswerkzeugen |
| `src/modules/frontpanel/panelsidebar.*` | Bibliothek rechts: Symbole, Stifte, Füllungen, Ansichten, Schriften |
| `src/modules/frontpanel/paneldialogs.*`, `panelwizards.*` | Dialoge, Skalen-Assistent, Frontplattenausschnitt |
| `src/modules/frontpanel/panelicons.*` | Eigene Symbole und Mauszeiger des Editors |
| `src/modules/frontpanel/demo.cpp` | Eigenständiges Programm |
| `src/formats/frontdesigner/delphistream.*` | Typisierter Wertestrom der FrontDesigner-Dateien |
| `src/formats/frontdesigner/fpl.*` | Leser und Schreiber für FPL und LIB |
| `src/formats/frontdesigner/fdplot.*`, `x87.*` | Umrisse der HPGL-Ausgabe wie im Original, mit dessen 80-Bit-Gleitkommarechnung |
| `src/formats/frontdesigner/fdshapes.*` | Strichschriften in Linien und Bögen umgewandelt wie im Original |
| `src/formats/frontdesigner/inifile.*` | INI-Texte der Einstellungsdateien des Originals |
| `tests/frontpanel_*.cpp` | Tests (CTest-Name `frontpanel`) |

Die Bibliothek `openloch_frontpanel` hängt nur von `openloch_core` und Qt (Gui, Widgets, PrintSupport) ab. Modell, Formate, Generatoren, Bearbeitungsjobs und Darstellung lassen sich ohne Fenster prüfen.

## Modell

Ein Dokument enthält eine oder mehrere Frontplatten (Vorder- und Rückseite eines Gehäuses sind zwei Frontplatten), gespeicherte Ansichten und Ressourcen für Bilder. Jede Frontplatte hat Name, Breite und Höhe, eine Grundfarbe mit optionalem Farbverlauf, Raster (Weite, sichtbar, Fang, Farbe), einen Benutzerursprung, eine Maßeinheit für Lineale und Koordinatenanzeige (mm oder inch; die Schaltfläche in der Ecke der Lineale schaltet sie als Bearbeitungsschritt um), eine geordnete Objektliste (zuerst gezeichnet = unten) und ihre Druckeinstellungen.

Einheiten: Millimeter. Koordinaten gehören zur Frontplatte, Ursprung oben links, x nach rechts, y nach unten. Winkel in Grad, positiv gegen den Uhrzeigersinn auf dem Bildschirm.

Objektarten:

| Art | Geometrie |
| --- | --- |
| `line` | offener Linienzug |
| `polygon` | geschlossene Fläche |
| `rectangle` | vier Ecken, bleibt beim Drehen ein Rechteck aus vier Punkten |
| `ellipse` | Mitte, zwei Radien, Drehung |
| `arc` | wie Ellipse, dazu Start- und Spannwinkel und Stil (offen, Tortenstück, Sehne) |
| `text` | Text in einem Rahmen aus drei Ecken (oben links, oben rechts, unten links); der Text füllt den Rahmen |
| `drill` | Mitte und Durchmesser |
| `image` | Rasterbild in einem Rahmen, optional mit einer ausgeblendeten Farbe |
| `picture` | Vektorgrafik (EMF) in einem Rahmen |
| `group`, `dimension`, `scale`, `cutout` | Teile und die Parameter, aus denen sie entstanden sind |

Jedes Objekt hat einen Stift (Farbe, Breite in mm, Stil durchgezogen/punktiert/gestrichelt/Strich-Punkt/unsichtbar), eine Füllung (einfarbig, ohne, sechs Schraffuren; einfarbig optional mit Verlauf zu einer zweiten Farbe in vier Richtungen) und eine Bearbeitung: nur zeichnen, fräsen oder gravieren. Bei Fräsen und Gravieren ist die Stiftbreite die Werkzeugbreite. Linienzüge, Flächen und Rechtecke haben zusätzlich eine Eckenform: unverändert, B-Spline durch die Kantenmitten, die Kanten sind Tangenten (offen wie im Original: vom ersten Punkt gerade zur Mitte der ersten Kante und von der Mitte der letzten Kante gerade zum letzten Punkt; Linienzüge aus Bögen des Originals, die beim Lesen keine Ellipse mehr bilden, behalten dessen Bogen-Spline ohne diese zwei Stücke), Fase oder Rundung mit einem Maß in mm.

Eine **Kombination** ist eine Gruppe mit `parameters.combine = true`: Ihre geschlossenen Teile bilden zusammen eine Kontur mit Stift, Füllung und Werkzeug der Gruppe; gefüllt wird nach der Gerade-Ungerade-Regel, innere Konturen sind also Löcher. Stift und Werkzeug der Kombination gelten wie im Original auch für ihre Teile; für Fräsen und Gravieren zählt jedes Teil mit seinem eigenen Werkzeug. Ein Symbol kann einen **Einfügepunkt** (`anchor`) haben; er hängt beim Platzieren am Mauszeiger und liegt auf dem Raster.

**Bauteile und Platinen dahinter.** In einem Projekt kann ein Objekt für ein Bauteil stehen, etwa die Bohrung eines Potentiometers oder ein Symbol aus Bohrung, Ausschnitt und Beschriftung als Gruppe: Es trägt dann die Kennung des Bauteils (`component`, 32 Hexadezimalziffern). Eine Kopie ist ein neues Bauteil und erhält eine neue Kennung, Objekte, die zusammen kopiert werden und sich eine Kennung teilten, wieder eine gemeinsame; ein Symbol, das in die Bibliothek kommt, verliert sie. Hinter einer Frontplatte können Platinen sitzen: Platinen eines Lochraster- oder Leiterplattendokuments desselben Projekts, genannt über die Kennung des Dokuments und die der Platine, jede höchstens einmal. Ihre Koordinaten sind Millimeter von der Bestückungsseite gesehen, Ursprung in ihrer linken oberen Ecke, x nach rechts, y nach unten. Auf der Frontplatte, von vorn gesehen, wird die Platine links und rechts gespiegelt, wenn ihre Lötseite zur Frontplatte zeigt, dann um ihren Ursprung gedreht (Grad, positiv gegen den Uhrzeigersinn), und ihr Ursprung liegt am Versatz. Die Lage gehört zur Frontplatte: Sie wird mit ihr bearbeitet, rückgängig gemacht und gespeichert.

Generierte Objekte behalten ihre Parameter: Bemaßungen die beiden Messpunkte, die Lage der Maßlinie und ihren Stil, Skalen und Ausschnitte ihre Einstellungen und die Abbildung, mit der sie platziert wurden. Nach dem Verschieben oder Strecken lassen sie sich daraus neu erzeugen; eine Bemaßung misst dabei sofort neu.

Abbildungen (Verschieben, Drehen, Spiegeln, Strecken, Scheren) wirken exakt: Ellipsen und Bögen bleiben Ellipsen und Bögen, ihre Parameter werden aus der Abbildung neu bestimmt; ein gespiegelter Bogen läuft in Gegenrichtung. Bohrungen behalten ihre Kreisform, Gruppen nehmen ihre Teile und die Ankerpunkte ihrer Generatoren mit.

## Eigenes Dateiformat

Endung `.olfp`, UTF-8-JSON mit `format: "OpenLoch-Frontplatte"` und `version: 2`; Version 1 kennt keine Bauteile und keine Platinen dahinter und wird ebenso gelesen. Felder auf oberster Ebene: `id`, `title`, `revision`, `activePanel`, `panels`, optional `views`, `resources` und `foreign`.

Eine Frontplatte: `id`, `name`, `width`, `height`, `color`, `color2`, `gradient` (`none`, `horizontal`, `vertical`, `diagonal`, `crossdiagonal`), `grid` (`size`, `visible`, `snap`, `color`), `origin`, `elements`, optional `inch` (`true`: Lineale und Koordinaten in inch), optional `boards`, die Platinen dahinter (`document` und `board` als Kennungen, `offset` als `[x, y]` in mm, `rotation` in Grad von −360 bis 360, `side` `top`, wenn die Bestückungsseite zur Frontplatte zeigt, sonst `bottom`), und optional `print`, die Druckeinstellungen, wenn sie von den Vorgaben abweichen: Wahrheitswerte `mirror`, `background`, `frame`, `rulers`, `data`, `dimensions`, `machining`, `objects`, `cutMarks`, `texts`, `original`, `centred`, `onlyOne`, `landscape`, dazu `zoom` (Faktor), `sheet`, `tiles` (`[waagerecht, senkrecht]`, je 1 bis 100), `gap` (Kachelabstände in mm) und `offset` (Lage der ersten Kachel in mm, gemessen von der Ecke des bedruckbaren Bereichs). Eine Ansicht: `name`, `panel` (Index) und `area` (`[x, y, Breite, Höhe]`).

Ein Objekt: `id`, `type`, optional `name`, optional `component` (Kennung eines Bauteils), `pen` (`color`, `width`, `style`), `fill` (`style`, `color`, `color2`, `gradient`), optional `machining` (`mill`, `engrave`), optional `anchor` und je nach Art `points`, `contour` (`corners`: `sharp`, `spline`, `chamfer`, `round` oder `arcspline`; `size`), `center`/`radiusX`/`radiusY`/`rotation`, `startAngle`/`spanAngle`/`arcStyle`, `frame`, `text`, `font`, `bold`, `italic`, `underline`, `strikeOut`, `strokeFont`, `diameter`, `resource`, `transparent`/`transparentColor`, `children`, `parameters`. Punkte sind `[x, y]`-Paare, Farben `#rrggbb`.

`resources` ordnet SHA-256-Schlüssel den Bildern zu (`kind` `bmp`, `png`, `jpg` oder `emf`, `data` in Base64). Nicht mehr verwendete Ressourcen werden beim Speichern weggelassen. `foreign` enthält Werte einer importierten Datei, die im Modell keine Bedeutung haben, damit ein erneuter Export sie unverändert schreibt.

Regeln beim Lesen: Fehlende optionale Felder erhalten ihre Standardwerte, vorhandene Felder müssen gültig sein (endliche Zahlen in sinnvollen Grenzen, bekannte Aufzählungswerte, gültige Farben, passende Punktanzahl, vorhandene Ressource). Unbekannte Felder werden ignoriert. Eine Datei mit höherer `version` wird mit einer Meldung abgewiesen. Gespeichert wird atomar: Das Dokument wird vorher kodiert und erneut gelesen; ein fehlgeschlagenes Schreiben lässt eine vorhandene Datei unverändert.

## FrontDesigner-Dateien (FPL, LIB)

### Wertestrom

Die Dateien sind ein Strom aus typisierten Werten und untypisierten Blöcken. Ein typisierter Wert beginnt mit einem Kennbyte: `2`, `3`, `4` ganze Zahl mit 8, 16 oder 32 Bit (Little Endian, mit Vorzeichen, jeweils die kürzeste passende Form), `5` Gleitkommazahl mit 80 Bit (64 Bit Mantisse mit ausgeschriebenem Ganzzahlbit, 15 Bit Exponent mit Versatz 16383, Vorzeichenbit), `8`/`9` falsch/wahr, `6` Text mit Längenbyte, `12` Text mit 32-Bit-Länge, `20` UTF-8-Text mit 32-Bit-Länge, `18` UTF-16-Text mit 32-Bit-Zeichenzahl. Texte werden als kurzer Text geschrieben, wenn sie nur aus ASCII bestehen und höchstens 255 Bytes lang sind, als UTF-8, wenn das kürzer ist als UTF-16, sonst als UTF-16; ein leerer Text ist ein leerer UTF-16-Text. Untypisierte Blöcke haben eine feste Länge: Farben 4 Bytes (Rot, Grün, Blau, Merkmalbyte), Stile 1 Byte, Punktpaare 20 Bytes (zwei 80-Bit-Zahlen ohne Kennbyte), kurze Namen 41 Bytes (Längenbyte, Zeichen in Windows-1252, Rest beliebig).

Alle Längen werden in **Fünfzigstel Millimetern** gespeichert: Koordinaten, Stiftbreiten, Bohrdurchmesser, Schrifthöhen, Rasterweite und Plattengröße.

### Projekt

Ein Projekt (`.FPL`) besteht aus der ersten Frontplatte, der Liste gespeicherter Ansichten (Anzahl; je Ansicht 40 Bytes Ausschnitt, ab Version 2.0 eine 32-Bit-Plattennummer, Name), der Anzahl aller Frontplatten und den übrigen Frontplatten. Eine Bibliotheksseite (`.LIB`) enthält nur eine Frontplatte; ihr Name ist der Name der Seite. Das Format ist versioniert; jede Frontplatte trägt ihre Version im Vorspann, und ältere Versionen lassen Felder aus. Bekannt sind Dateien der Versionen 3,04, 3,05, 3,15 und 3,16; geschrieben wird 3,16.

### Frontplatte

1. Vorspann aus 41 Bytes: Längenbyte, Name (höchstens 36 Zeichen), an den Stellen 37–40 die Version als vier Zeichen, zum Beispiel `3,16`.
2. Anzahl der Objekte, danach je Objekt der Klassenname als Text und die Objektdaten.
3. Platteneigenschaften: eine ganze Zahl, die das Original nicht verwendet (neue Frontplatten: 0), Breite, Höhe, Rasterweite (vor 2.02 ganzzahlig), eine weitere unbenutzte ganze Zahl (1), Ursprung (Punktpaar), Raster sichtbar, Grundfarbe; ab 3.01 Rasterfarbe, die Maßeinheit der Frontplatte für Lineale und Koordinaten (0 mm, 1 inch) und fünf Einheitenwahlen der Eingabefelder (0 mm, 1 inch; drei für das Raster, zwei für den Ursprung, die beim Umschalten der Maßeinheit mitgehen); ab 3.02 Verlaufsfarbe und Verlaufsart (0 einfarbig, 1 waagerecht, 2 senkrecht, 3 und 4 diagonal); ab 3.05 die Druckeinstellungen: Kacheln waagerecht und senkrecht, das Blatt für „nur ein Blatt drucken“, die Kachelabstände (Punktpaar), 13 Wahrheitswerte (Bohren und Fräsen, Objekte und Symbole, Daten, Lineale, Rahmen, Hintergrund, Spiegeln, Querformat, Originalgröße, mittig ausrichten, Bemaßungen, Schnittmarken, nur ein Blatt drucken), der Maßstab und die Lage der ersten Kachel als zwei ganze Zahlen mit umgekehrtem Vorzeichen, gemessen von der Ecke des bedruckbaren Bereichs; ab 3.15 der vollständige Name; ab 3.16, ob Texte gedruckt werden (in älteren Dateien ja). Abstände und Lage sind Frontplatteneinheiten, auf dem Papier also mit dem Maßstab vervielfacht.

### Gemeinsame Objektfelder

Jedes Objekt beginnt mit: Objektart (ganze Zahl), Stiftbreite, Stiftfarbe, Stiftstil (0 durchgezogen, 1 punktiert, 2 gestrichelt, 3 Strich-Punkt, 4 unsichtbar), Füllfarbe, Füllstil (0 einfarbig, 1 ohne, 2–7 Schraffuren waagerecht, senkrecht, diagonal fallend, diagonal steigend, Gitter, Rautengitter), ein Wahrheitswert (in Dateien vor 3.04 „gefüllt“, sonst immer falsch), ein Wahrheitswert für Teile von Gruppen. Ab 2.0: gefräst, Eckenform an, Eckenform (0 B-Spline, 1 Fase, 2 Rundung, 3 B-Spline der Bögen, offen ohne die geraden Stücke zu den Endpunkten), Maß der Eckenform, kurzer Name (41 Bytes), der Einfügepunkt eines Symbols als zwei Gleitkommazahlen. Ab 3.0 der angezeigte Objektname, ab 3.02 Verlaufsfarbe und Verlaufsart, ab 3.03 graviert, ab 3.15 der lange Name.

### Objektklassen

| Klasse | Daten nach den gemeinsamen Feldern | Im Modell |
| --- | --- | --- |
| `TLinie` | Anzahl − 1, Punkte | Linienzug |
| `TRechteck`, `TPolygon` | wie `TLinie`, dazu „geschlossen“ | Rechteck (Objektart 4 mit vier Ecken) oder Fläche |
| `TSkaleBogen` | wie `TRechteck`, dazu ein Byte | Fläche (Teil von Skalen) |
| `TKreis` | Mittelpunkt, Radiuspunkt (beide nur beim Erzeugen gesetzt), ab 2.0 eine geschlossene `TRechteck`-Kontur ohne Klassennamen mit 16 Punkten | Ellipse |
| `TBogen` | wie `TLinie` (21 Punkte), Mittelpunkt, Radiuspunkt, Start- und Endstrahl, Stil (0 offen, 1 Tortenstück, 2 Sehne) | Bogen |
| `TTextLabel` | vier Ecken, Schriftart, Schrifthöhe, eine unbenutzte Gleitkommazahl (0), ein alter Text (ab 3.15 leer), zwei Wahrheitswerte für Spiegelungen (je einer pro Achse, siehe unten), ab 1.01 Schriftstil (Bit 0 fett, 1 kursiv, 2 unterstrichen, 3 durchgestrichen), ab 3.04 Strichschrift an und ihr Name, ab 3.15 der Text | Text |
| `TBohrung` | Mitte x, Mitte y, Durchmesser | Bohrung |
| `TBtmap` | wie `TRechteck` (vier Ecken), eine BMP-Datei (Länge aus ihrem Kopf), transparente Farbe, transparent an | Bild |
| `TMeta` | wie `TRechteck`, eine EMF-Datei (Länge aus ihrem Kopf) | Vektorgrafik |
| `TGruppe` | Anzahl − 1, Gruppenname (41 Bytes), Teile mit Klassennamen | Gruppe |
| `TKombination` | wie `TGruppe` | Kombination |
| `TSchalttafel` | wie `TGruppe` | Ausschnitt |
| `TBemassung` | wie `TGruppe` (fünf Teile), Lage der Maßlinie, zwei Messpunkte, ab 3.04 zwei Wahrheitswerte für Spiegelungen wie bei Texten | Bemaßung |
| `TScale` | wie `TGruppe`, danach Stil (ein Byte), drei Gleitkommazahlen, fünf Wahrheitswerte (die letzten beiden: waagerecht und senkrecht gespiegelt), zweimal eine ganze Zahl mit einer Gleitkommazahl, Bezugspunkt, Drehung im Bogenmaß | Skala |

Im Skalenblock stehen vor dem Bezugspunkt nur Felder eines älteren Parametersatzes; sie haben in allen bekannten Dateien dieselben Werte. Die Einstellungen des Skalen-Assistenten werden nicht gespeichert, eine Skala besteht in der Datei nur aus ihren Teilen. Der Bezugspunkt ist der Mittelpunkt einer runden Skala und die Mitte einer geraden; die Drehung zählt gegen den Uhrzeigersinn.

Kreise und Bögen werden als Stützpunkte gleichen Winkelabstands gespeichert und als B-Spline gezeichnet. Die Stützpunkte liegen um den Faktor 1/cos(halber Winkelschritt) außerhalb der Kurve, damit der Spline durch die Kantenmitten auf ihr verläuft; Bögen tragen dazu die Eckenform 3: Ihre Kurve beginnt und endet in der Mitte der ersten und letzten Kante, die Stützpunkte reichen also einen halben Schritt über Anfang und Ende hinaus. Bögen laufen auf dem Bildschirm im Uhrzeigersinn vom Start- zum Endstrahl. Der Leser bestimmt Ellipse beziehungsweise Bogen aus diesen Punkten, auch nach Drehen, Strecken oder Spiegeln; Mittel- und Radiuspunkt im Objekt sind danach oft veraltet. Bilden die Punkte keine Ellipse mehr (etwa nach Bearbeitung einzelner Knoten), übernimmt der Leser die Kontur als Linienzug oder Fläche mit B-Spline-Ecken und meldet das.

### Skaleneinstellungen (SCL)

Skaleneinstellungen sind INI-Texte in Windows-1252 mit CR LF; das Original speichert die zuletzt verwendeten Einstellungen jedes Stils im selben Format (`Skale0.INI` bis `Skale12.INI`).

- `[Parameter]`: `Style` (Nummer des Stils), `Parameter1`, `Parameter2`, … in der Reihenfolge der Parameterliste. Zahlen mit Dezimalkomma, Ja/Nein als 1/0. Ältere Dateien haben bei manchen Stilen einen Parameter weniger („Text mitdrehend“); fehlende Parameter behalten ihre Vorgabe.
- `[Text]`: `Count` und `Text0` … für die beschrifteten Stellen. Gültig sind nur die ersten `Count` Einträge; dahinter können ältere stehen.
- `[Design]`: `Count` und je Teil (Nummer als Endung, Reihenfolge der Teileliste) `PenWidth` (Fünfzigstel Millimeter), `PenColour` (Farbe als 0x00BBGGRR), `PenPattern` (wie der Stiftstil der FPL-Dateien), `PenTool` (0 Stift, 1 Fräser, 2 Gravierer); für Teile mit Füllung `BrushStyle`, `BrushColour`, `BrushGradientStyle` (wie die Verlaufsart der FPL-Dateien), `BrushGradientColour`; für die Beschriftung `FontHeight` (ohne Wirkung, die Texthöhe ist ein Parameter), `FontName`, `FontSHXName` (Strichschrift), `FontSHX` (Strichschrift an), `FontItalic`, `FontBold`.
- Ältere Dateien haben statt `[Design]` die Abschnitte `[Font]` (`Name`, `Color`, `Italic`, `Bold`, `Underline`, `Strikeout`) und `[Pen]` (`Color`, `Width`, `Style`); sie gelten für die Schrift der Beschriftung bzw. für alle Striche.

### Vorgaben für Stifte, Füllungen und Schriften

Das Original hält seine Listen in drei INI-Texten in Windows-1252; Farben sind Zahlen der Form 0x00BBGGRR, Längen Fünfzigstel Millimeter.

- `Stifte.INI`: `[Stifte]` mit `Anzahl`, je Eintrag `[Stift0]`, `[Stift1]`, … mit `Name`, `Farbe`, `Breite`, `Pattern` (wie der Stiftstil der FPL-Dateien) und `Tool` (0 Stift, 1 Fräser, 2 Gravierer).
- `FUELLUNG.INI`: `[Fuellung]` mit `Anzahl`, je Eintrag `[Fuellung0]`, … mit `Name`, `Farbe`, `Style` (wie der Füllstil), `VerlaufStyle` (wie die Verlaufsart; gilt für einfarbige Füllungen), `VerlaufFarbe` und `Verlauf`, das keine sichtbare Wirkung hat.
- `Fonts.ini`: `[Fonts]` mit `Anzahl`, je Eintrag `[Font0]`, … mit `Beschreibung` (Name der Vorgabe), `Name` (Schriftart), `Hoehe`, `Italic`, `Bold`, `Underline`, `Strikeout`, `Farbe`, `SHX` (Strichschrift an), `SHXName` sowie Stift- und Füllungswerten. Übernommen werden Name, Schriftart, Höhe, fett, kursiv und Strichschrift; Farben gehören hier nicht zur Schriftvorgabe.

### Erhalten und Schreiben

Beim Lesen bleiben alle Werte ohne Entsprechung im Modell unter `foreign.frontDesigner` erhalten. Jedes Objekt behält einen Fingerabdruck seiner Geometrie, Objekte aus Dateien der Version 3,16 zusätzlich ihre Originalbytes; ist ein Objekt beim Schreiben unverändert, werden genau diese Bytes geschrieben. Ein unverändert gelesenes und wieder geschriebenes 3,16-Projekt ist deshalb bytegleich. Außerdem bleiben die gespeicherten Werte der Stützpunkte von Kreisen und Bögen erhalten (`points`, bei Kreisen in `inner`), dazu die von Punkten und Bohrungen, wenn die Millimeter des Modells sie nicht genau wiedergeben (`points`, `drill`). Ein unverändertes Objekt einer älteren Version wird mit diesen Werten geschrieben, die HPGL-Ausgabe verwendet sie ebenfalls. Geänderte Objekte werden aus dem Modell neu erzeugt. Abgeschnittene oder widersprüchliche Dateien werden mit Angabe der Dateiposition abgewiesen; unbekannte Klassen können nicht übersprungen werden, weil die Länge ihrer Daten nicht gespeichert ist.

Gespiegelte Texte: Das Original spiegelt die vier Ecken an Ort und Stelle und kippt dabei einen der zwei Wahrheitswerte. Die Lage der Ecken zeigt die Spiegelung also schon; beim Lesen ergibt sie einen gespiegelten Textrahmen. Beim Schreiben ist für einen gespiegelten Text genau einer der Werte gesetzt; ein gelesener Text behält seine Werte, solange sie dazu passen.

Bauteilkennungen (`component`) und die Platinen hinter einer Frontplatte (`boards`) gibt es nur im eigenen Format: FPL- und LIB-Dateien enthalten sie nicht, ein Export nach FPL lässt sie weg.

Ein Rasterbild wird beim Export nach FPL als BMP geschrieben. Vektorgrafiken werden unverändert weitergegeben. Bemaßungen mit ihren fünf Teilen werden als `TBemassung` geschrieben; Skalen und Ausschnitte ohne FrontDesigner-Parameter als Gruppe, damit FrontDesigner keine fehlenden Teile erwartet.

## Editor

`PanelEditor` ist ein Hauptfenster, damit sich die Werkzeugleisten verschieben, abdocken und über „Optionen › Werkzeuge anzeigen“ ein- und ausblenden lassen. Aufteilung wie im Original: die Werkzeuge links, oben die Leisten Datei, Bearbeiten, Anordnen und Ansicht, unten Ausrichten, Drehen, Breite/Höhe, Kontur, Anzeige und Position, darüber die Reiter der Frontplatten, rechts die Bibliothek und auf Wunsch der Objektbaum. Menüs: Datei, Bearbeiten, Ansicht, Anordnen, Bibliothek, Frontplatte, Optionen, Hilfe (Hilfethemen mit F1, die Hilfeseiten `help/frontpanel/` in den drei Sprachen der Oberfläche). Neben den üblichen Tastenkürzeln gelten die des Originals (F1 Hilfe, F2 Speichern, F3 Öffnen, F4 Speichern unter, F5–F8 Frontplatte hinzufügen, duplizieren, löschen, aus Datei hinzufügen, Strg+G/U Gruppe, Strg+K/L Kombination). Strg+Y, Strg+O und Strg+S des Originals (zur Bibliothek hinzufügen, Ursprung setzen, Skalen-Assistent) bleiben Wiederholen, Öffnen und Speichern, wie auf allen Plattformen üblich; im Original liegt Strg+Y zudem doppelt.

Werkzeuge: Markieren, Drehen, Lupe, Linie, Fläche, Rechteck, Kreis, Bogen (Mittelpunkt, Start, Ende im Uhrzeigersinn), regelmäßiges Vieleck, Text, Bohrung, Skalen-Assistent, Frontplattenausschnitt, Bemaßung (zwei Messpunkte, dann die Lage der Maßlinie), Bild importieren, Ursprung setzen.

Verhalten:

- Alle Punkte rasten auf dem Raster ein; die Umschalttaste hebt den Fang auf, beim Drehen auch den 45°-Winkelfang. Verschieben geht in ganzen Rasterschritten, die Pfeiltasten verschieben um einen Rasterschritt (mit Umschalttaste 0,1 mm).
- Die rechte Maustaste beendet Linienzug und Fläche, verlässt ein Zeichenwerkzeug, bricht das Platzieren ab und öffnet sonst das Kontextmenü (auf einem Knoten: Knoten hinzufügen oder löschen).
- Ein Klick auf ein bereits markiertes Objekt wechselt zwischen Streck- und Drehanfassern. Ecken strecken mit Strg proportional, ebenso alle Anfasser bei eingeschaltetem 1:1.
- Eingefügte, duplizierte, aus der Bibliothek gewählte, importierte und erzeugte Objekte hängen am Mauszeiger, bis ein Klick sie absetzt; der Einfügepunkt (oder die linke obere Ecke, bei Bildern die linke untere) liegt dabei auf dem Raster.
- Ausrichten bezieht sich auf das zuletzt markierte Objekt, ein einzelnes Objekt wird an der Frontplatte ausgerichtet. Verteilen (gleicher Abstand der Mittelpunkte, linken, rechten oder benachbarten Seiten; Gesamtgröße beibehalten, neu vorgeben oder Abstand vorgeben; mit oder ohne Stiftbreite) und Am Raster ausrichten arbeiten wie im Original.
- Breite und Höhe in der unteren Leiste gelten ohne Markierung für die Frontplatte. Die Position bezieht sich auf den Ursprung.
- Ist genau ein Objekt markiert, übernehmen Stift, Füllung und Schrift seine Einstellungen für die nächsten Objekte. Stift und Füllung aus der Bibliothek wirken auf die Markierung; Texte ändern sie nur, wenn ausschließlich Texte markiert sind. Teile von Gruppen lassen sich über den Objektbaum einzeln bearbeiten, aber nicht einzeln löschen.
- Kopieren legt die Objekte zusätzlich als Bild in die Zwischenablage. Die Eigenschaften einer Skala oder eines Ausschnitts öffnen den jeweiligen Assistenten; das Objekt wird an seinem Platz neu erzeugt.
- Gruppe auflösen löst nur die oberste Stufe, auch bei Bemaßungen, Skalen und Ausschnitten; danach sind es gewöhnliche Teile.
- Auf das Fenster gezogene Dateien werden geöffnet (Frontplatten, Bibliotheken, Sicherungen) oder als Bild importiert.
- AutoSpeichern schreibt in einstellbarem Abstand eine Sicherung `Name.BAK` neben das Projekt (ungespeicherte Projekte in den Programmdatenordner). Eine geöffnete `.BAK` wird als das zugehörige Projekt fortgesetzt.

**Platinen dahinter.** In einem Projekt der Suite kennt der Editor die Platinen der Lochraster- und Leiterplattendokumente (`boardSources`, die Suite liefert jede Platine mit Größe und den Mitten, Umrissen und Seiten ihrer Bauteile). Im Menü Frontplatte:

- „Platinen dahinter…“ wählt die Platinen hinter der Frontplatte und ihre Lage: Versatz der linken oberen Ecke (von der Bestückungsseite gesehen), Drehung gegen den Uhrzeigersinn um diese Ecke und die Seite, die zur Frontplatte zeigt; „Markierte Platine mittig setzen“ legt sie in die Mitte. Mit „Bohrungen und Symbole der Bauteile mitnehmen“ wandern die Objekte, die für Bauteile einer verschobenen Platine stehen, mit ihr. Eine Platine, die das Projekt nicht mehr hat, steht als fehlend in der Liste. Ein Rückgängig-Schritt.
- „Platinen anzeigen“ zeichnet die Platinen über die Frontplatte: Umriss, Umrisse und Mitten der Bauteile mit Bezeichner, Bauteile auf der abgewandten Seite blasser. Nur in der Ansicht, nie in Druck, Bildexport oder HPGL.
- „Bohrungen aus der Platine…“ bietet die Bauteile der Platinen dahinter an; vorgewählt sind LEDs (3 oder 5 mm nach ihrer Größe), Potentiometer (7 mm), Schalter und Buchsen (6 mm, BNC 9,5 mm), erkannt an Bezeichner, Wert oder Bibliotheksname, sofern sie zur Frontplatte zeigen und noch keine Bohrung haben. Jede Bohrung sitzt über der Mitte ihres Bauteils, heißt wie es und trägt seine Kennung. Ein Rückgängig-Schritt.
- „Mit Platine vergleichen“ nennt Objekte mit Bauteil, deren Bauteil nicht mehr darunter liegt (bei einer Bohrung: die Mitte des Bauteils außerhalb der Bohrung, sonst außerhalb des Objekts), und solche, deren Bauteil keine Platine dahinter hat; „An die Bauteile setzen“ schiebt sie an die Mitte ihres Bauteils. Ein Doppelklick markiert das Objekt.

Die Platinen fragt der Editor neu ab, wenn sein Fenster aktiv wird und vor jedem dieser Dialoge, da sie sich in einem anderen Fenster geändert haben können.

Die Bibliothek rechts hat fünf Seiten: **Symbole** sind FrontDesigner-Bibliotheksseiten (`.LIB`). Eigene Seiten liegen im Ordner `Dokumente/OpenLoch/Frontplattensymbole` (änderbar über die Einstellung `frontpanel/libraryFolder`); beim ersten Start entsteht dort eine Seite mit einigen Grundsymbolen. Weitere Ordner, etwa vorhandene Bibliotheken, werden nur gelesen. Symbole lassen sich aufnehmen (mit Einfügepunkt), über „Eigenschaften…“ nachträglich in Name und Einfügepunkt ändern, umbenennen, umsortieren und löschen, Seiten anlegen, umbenennen und löschen. **Stift**, **Füllung** und **Schrift** zeigen die aktuellen Einstellungen und Listen gespeicherter Vorgaben, getrennt nach Stift, Fräser und Gravierer; die Listen bleiben in den Einstellungen erhalten. „Optionen › Vorgaben aus FrontDesigner übernehmen…“ liest die Listen des Originals (`Stifte.INI`, `FUELLUNG.INI`, `Fonts.ini` aus seinem Ordner `Settings`) und ersetzt oder ergänzt damit die eigenen; ein Eintrag gleichen Namens wird ersetzt. **Ansicht** zoomt, blättert und verwaltet gespeicherte Ausschnitte des Dokuments.

## Generatoren

- **Bemaßung:** zwei Maßpfeile (je halbe Maßlinie mit Pfeilspitze aus zwei Strichen), Maßzahl lesbar in Richtung erster → zweiter Punkt, zwei Hilfslinien. Ist der Abstand zu klein für Pfeile und Zahl, zeigen die Pfeile von außen. Die Farbe kann automatisch zur Frontplatte passen.
- **Skalen** wie im Skalen-Assistenten des Originals, siehe unten.
- **Frontplattenausschnitt:** Rahmen nach DIN 43700 (Ausschnitt daraus berechnet, rechteckig, ohne Löcher) oder mit eigenen Maßen, rund oder rechteckig; der Ausschnitt wird innen mit der Fräserbreite gefräst, sodass er sein Nennmaß erhält; Montagelöcher oben/unten und links/rechts mit Abstand von Lochmitte zu Lochmitte. Einstellungen lassen sich als `.CUT` speichern und laden (INI-Text mit den Abschnitten Name, Frame, Cut und Holes).
- **Regelmäßiges Vieleck** mit Ecken, Innen- oder Außenradius und Startwinkel.

### Skalen

Der Skalen-Assistent kennt die dreizehn Stile des Originals, nummeriert wie dort. Jeder Stil hat eine feste Liste von Parametern (Seite **Konstruktion**), Texte für die beschrifteten Stellen (Seite **Beschriftung**, je Teilstrich der ersten Teilung bzw. je Segmentgrenze ein Text) und eine feste Liste von Teilen mit eigenem Stift, Werkzeug und, wo vorhanden, Füllung und Schrift (Seite **Gestaltung**; mehrere Teile lassen sich zugleich ändern). Längen in mm, Winkel in Grad.

| Nr. | Stil | Parameter in Listenreihenfolge | Teile |
| --- | --- | --- | --- |
| 0, 2 | Gerade Skala, linear bzw. logarithmisch | Länge, Lauflinie, 1. Teilung: Segmente, 1. Teilung: Länge, 2. Teilung, 2. Teilung: Segmente, 2. Teilung: Länge, Beschriftung, Texthöhe, Abstand, Textwinkel, Richtung umkehren | Beschriftung, 1. Teilung, 2. Teilung, Lauflinie |
| 1, 3 | Runde Skala, linear bzw. logarithmisch | Winkelbereich, Radius, Lauflinie, Mittelpunkt, 1. Teilung: Segmente, 1. Teilung: Länge, 2. Teilung, 2. Teilung: Segmente, 2. Teilung: Länge, Drehung, Beschriftung, Texthöhe, Abstand, Textwinkel, Richtung umkehren, „Low profile“ (%), Text mitdrehend | wie 0, dazu Mittelpunkt |
| 4 | Gerade Punktskala | wie 0, statt der Längen die Durchmesser der Punkte | wie 0 |
| 5 | Runde Punktskala | wie 1, statt der Längen die Durchmesser der Punkte | wie 1 |
| 6 | Skala aus farbigen Kreisabschnitten | Winkelbereich, Radius innen, Breite, Segmente (1 bis 10), Mittelpunkt, 1. bis 9. Segmentgrenze (% des Winkelbereichs), Grenzlinien, Länge, Drehung, Beschriftung, Texthöhe, Abstand, Textwinkel, Text mitdrehend | Beschriftung, Grenzlinien, Mittelpunkt, 1. bis 10. Segment |
| 7, 8, 9 | Zunehmender, abnehmender, zu beiden Seiten zunehmender Bogen | Winkelbereich, Radius, Breite, Mittelpunkt, Drehung | Bogen, Mittelpunkt |
| 10 | Kreisringabschnitt | Winkelbereich, Radius innen, Breite, Mittelpunkt, Drehung | Bogen, Mittelpunkt |
| 11 | Regelmäßiges Vieleck | Durchmesser außen, Anzahl der Ecken, Mittelpunkt, Drehung | Polygon, Mittelpunkt |
| 12 | Sinuskurve | Höhe, Breite, Anzahl der Schwingungen, Phasenlage, Mittellinie | Kurve, Mittellinie |

Aufbau:

- Bezugspunkt ist der Mittelpunkt (runde Stile, Vieleck, Sinuskurve) bzw. die Mitte der Linie (gerade Stile); mit ihm wird die Skala platziert.
- Gerade Skalen liegen waagerecht, Werte steigen nach rechts, Teilstriche zeigen nach oben. Runde Skalen sind bei Drehung 0 symmetrisch zur Senkrechten über dem Mittelpunkt: Der Anfang liegt bei 90° + Winkelbereich/2 + Drehung, Werte steigen im Uhrzeigersinn. „Richtung umkehren“ vertauscht Anfang und Ende.
- Die erste Teilung teilt in gleiche Segmente, die zweite teilt jedes davon noch einmal. Logarithmisch geteilte Skalen umfassen eine Dekade; die Striche stehen für gleiche Wertschritte von 1 bis 10 an der Stelle log₁₀(1 + 9·v).
- Teilstriche beginnen auf der Lauflinie und reichen ihre Länge nach außen, negative Längen nach innen; Punkte sitzen mittig auf der Lauflinie. Texte stehen mit ihrer Mitte um die Strichlänge der ersten Teilung plus den Abstand von der Lauflinie entfernt (bei Punktskalen nur um den Abstand), mit dem Textwinkel; „Text mitdrehend“ dreht sie zusätzlich mit dem Strich.
- Das Mittelkreuz reicht ein Fünftel des Radius nach jeder Seite.
- „Low profile“ flacht runde Skalen unter 180° ab: Der Mittelpunkt rückt um (p/3)⁴ Fünfzigstel Millimeter von der Skala weg, die Lauflinie läuft durch dieselben Endpunkte, und jede Marke wird vom alten Mittelpunkt aus bestimmt und auf den Strahl vom neuen übertragen.
- Kreisabschnitte reichen von „Radius innen“ um die Breite nach außen; Grenzlinien beginnen an ihrem Außenrand und sind „Länge“ lang, Texte stehen an den Grenzen um Breite plus Abstand vom inneren Radius. Bögen werden von 0 auf die Breite breiter, umgekehrt, oder zur Mitte hin schmaler und wieder breiter; der Kreisringabschnitt hat überall die Breite. Jedes Band ist wie im Original eine Fläche mit 16 gleichen Winkelschritten auf seinem äußeren und seinem inneren Rand, gezeichnet als B-Spline; das rundet seine Ecken leicht.
- Das Vieleck hat seine erste Ecke unter dem Drehwinkel. Die Sinuskurve hat acht Stützpunkte je Schwingung und wird als B-Spline gezeichnet; die Höhe gilt von Spitze zu Spitze.

Jeder Stil merkt sich seine zuletzt verwendeten Einstellungen. Einstellungen lassen sich als Skaleneinstellungsdatei des Originals laden und speichern (siehe unten); die Werkseinstellungen entsprechen denen des Originals (schwarze, dünnste Linien).

## HPGL-Bearbeitungsdateien

Bohrungen, gefräste und gravierte Konturen werden wie im Original zu Jobs je Werkzeug zusammengefasst: zuerst Bohren je Durchmesser, dann Fräsen und Gravieren je Werkzeugbreite, jeweils aufsteigend, zuletzt das Außenrechteck; ausgefräste Bohrungen stehen als ein Job an der Stelle der Bohrjobs. Konturen mit dem Werkzeug Stift werden nicht ausgegeben. Wie im Original werden Gruppen und Kombinationen in ihre Teile zerlegt: Jedes Teil zählt mit seinem eigenen Werkzeug und seiner eigenen Breite. Stift und Werkzeug, die man einer Kombination gibt, gibt der Editor deshalb wie das Original an ihre Teile weiter, auch beim Bilden der Kombination. Im Dialog lassen sich Bohrungen, Fräsungen, Gravuren und Außenrechteck wählen und der Ausgabeordner festlegen; vorgeschlagen wird `Projektname-HPGL` neben dem Projekt. Jeder Job wird eine Datei mit seinem Namen und der Endung `.PLT`, zum Beispiel `Bohren mit 3 mm.PLT`. Die Namen bildet das Modul wie das Original aus „Bohren mit“, „Fräsen mit“, „Gravieren mit“, „Bohrungen fräsen mit“ oder „Aussenrechteck mit“ (so geschrieben), dem Werkzeugmaß als allgemeiner Zahl und „ mm“; in der englischen Oberfläche „Drilling“, „Milling“, „Engraving“, „Mill drillings with“ und „Contour rectangle“. „Ändern…“ in der Jobliste setzt nur für die Ausgabe eine andere Breite: für Konturen die Werkzeugbreite, für Bohrungen den Durchmesser (auch einzelner ausgefräster Bohrungen), für den Job der ausgefrästen Bohrungen den Fräser und für das Außenrechteck dessen Werkzeug (sonst 2 mm). Wie im Original gilt die neue Breite in ganzen Fünfzigstel Millimetern.

Dateiaufbau wie beim Original: Befehle mit `;`, jede Zeile mit CR LF. Der Kopf `IN;`, `SP1;` (mit der Option „Bohren/Fräsen auf Layer 2“ `SP2;` in Bohr- und Fräsjobs, Gravuren bleiben auf 1), `PT0;`, `PU;` steht vor dem ersten Befehl. Der Stift wird nur gehoben oder gesenkt, wenn er es nicht schon ist: Je Weg `PU;` (nur wenn der Stift unten ist), `PA x,y;` zum Anfang, `PD;`, dann die weiteren Punkte als `PA x,y;`; eine Bohrung wiederholt ihren Punkt. Am Ende `PU;` und `PA0,0;`. Mit „Gemeinsamer Ursprung“ beginnt jeder Job mit `PA0,0;`, `PD;`, `PU;` an der linken unteren Ecke, damit alle Jobs denselben Nullpunkt haben. Koordinaten in Plottereinheiten (1/40 mm) von der linken unteren Ecke der Frontplatte, y nach oben; gerundet wird zuerst auf ganze Fünfzigstel Millimeter und dann auf Plottereinheiten, jeweils zur nächsten ganzen Zahl (genau halbe zur geraden).

Gefräst wird auf der Konturlinie ohne Radiuskorrektur; der Editor zeigt die Werkzeugbreite um diese Linie. Die Wege bildet das Modul wie die Umrissbildung des Originals, in ganzen Fünfzigstel Millimetern und mit dessen Rechenweg Schritt für Schritt: Gerechnet wird wie die x87-Einheit mit 80-Bit-Gleitkommazahlen, jeder Schritt zur nächsten darstellbaren Zahl gerundet (genau halbe zur geraden), damit Punkte, die genau zwischen zwei Einheiten liegen, gleich ausfallen.

- Linienzüge und Flächen ohne Eckenform laufen von Punkt zu Punkt, jeder Punkt für sich gerundet; geschlossene kehren zum ersten Punkt zurück.
- Eine B-Spline-Ecke ist ein quadratisches Kurvenstück von der Mitte der einen Kante zur Mitte der nächsten mit der Ecke als Kontrollpunkt. Kantenmitte ist (b − a) / 2 + a, gerundet wie die Ecke. Das Stück hat 21 Punkte: t läuft von 0 in Schritten von 0,05 (als Double-Zahl), solange es höchstens 1 ist, dann folgt der Endpunkt. Offene Linienzüge beginnen an ihrem ersten und enden an ihrem letzten Punkt, geschlossene beginnen in der Mitte der ersten Kante.
- Eine Fase setzt je Ecke zwei Punkte im Abstand des Maßes von der Ecke, oder den Nachbarpunkt, wenn die Kante nicht länger ist. Geschlossene Konturen mit Fasen laufen wie im Original noch einmal bis zur ersten Ecke und dann zurück zum ersten Punkt.
- Eine Rundung ist ein Kurvenstück zwischen diesen beiden Punkten mit der Ecke als Kontrollpunkt.
- Kreise und Ellipsen sind geschlossene B-Splines durch 16 Stützpunkte auf Strahlen im Abstand von 22,5°, um 1/cos(11,25°) nach außen gerückt.
- Bögen sind B-Splines durch ihre Stützpunkte von der Mitte der ersten zur Mitte der letzten Kante. Hier gezeichnete Bögen haben 21 Stützpunkte, Bögen aus Dateien des Originals so viele, wie es angelegt hat. Ein Tortenstück läuft danach zum Mittelpunkt und zum ersten Stützpunkt, eine Sehne nur zu diesem Punkt; wie im Original kehrt der Weg nicht ganz zum Bogenanfang zurück.

Ein Objekt, das seit dem Lesen aus einer FPL-Datei unverändert ist, verwendet die dort gespeicherten Werte: die Stützpunkte von Kreisen und Bögen und alle Koordinaten, die die Millimeter des Modells nicht genau wiedergeben (siehe „Erhalten und Schreiben“). Jedes andere Objekt verwendet die Werte, die der FPL-Export für es schreibt; dieselbe Frontplatte als FPL im Original ausgegeben ergibt deshalb dieselben Wege.

„Bohrungen ausfräsen mit“ ersetzt die Bohrjobs durch einen Fräsjob: Wie im Original wird für jede Bohrung ein Kreis mit dem Radius (Bohrung − Fräser) / 2 und 16 Stützpunkten gebildet; Sinus und Kosinus stimmen dabei auf etwa eine Stelle im letzten Bit, wie die Befehle FSIN und FCOS der x87-Einheit. Ist der Fräser nicht kleiner als die Bohrung, wird nur angebohrt; das Original würde dann einen Kreis mit (Fräser − Bohrung) / 2 um die Mitte fräsen und die Bohrung vergrößern. Das Außenrechteck ist ein geschlossenes Rechteck um die halbe Werkzeugbreite außerhalb der Plattenkante, sodass die Frontplatte ihr Maß behält; es läuft von der linken unteren Ecke nach rechts, nach oben, nach links und zurück.

Texte in einer Strichschrift fährt das Werkzeug wie im Original Zeichen für Zeichen, entlang der Linien und Bögen ihrer FHX-Datei oder, wenn es keine gibt, entlang derer, die das Original aus der SHP- oder SHX-Datei macht (siehe unten):

- Der Maßstab der Schrift ist die Schrifthöhe der Datei geteilt durch Versalhöhe plus Unterlänge, als Double-Zahl gespeichert.
- Jedes Zeichen rückt um seinen gerundeten Vorschub weiter; ein Zeichen ohne Striche zählt als Leerzeichen.
- Die Summe der Vorschübe wird auf die Unterkante des Rahmens gestreckt. Die Grundlinie liegt die gerundete Unterlänge über der linken unteren Ecke.
- Linien laufen von Punkt zu Punkt. Ein Bogen mit dem Radius R und dem Winkel w läuft in Round(√(4·R)·w/2π + 6) gleichen Schritten.
- Schräge Rahmen scheren die Striche, gespiegelte Texte folgen den beiden Merkern der Datei, gedrehte drehen sich um den Ursprung des Zeichens.
- Ein Strich zählt erst, wenn der Stift danach gehoben wird.

Eine Strichschrift ohne FHX-Datei wandelt das Original bei der ersten Verwendung in Linien und Bögen um und speichert sie als FHX-Datei. Das Modul rechnet diese Umwandlung nach:

- Gelesen wird der Quelltext (SHP): die Zeilen nach der ersten Kopfzeile mit der Nummer des Zeichens bis zur nächsten Zeile, die mit einem Stern beginnt, ohne Trennzeichen aneinandergehängt und an den Kommas geteilt, Klammern entfernt. Eine Zahl mit führender Null gilt als zwei Hexadezimalziffern (die beiden nach der Null, bei zwei Zeichen die Null und die folgende), jede andere als Dezimalzahl mit Vorzeichen; das gilt auch für die Nummern der Kopfzeilen. Text, den das Original so nicht lesen kann (etwa Kommentare), lässt dessen Umwandlung scheitern.
- Eine kompilierte Schrift (SHX) übersetzt das Original vorher mit einem Hilfsprogramm in Quelltext; das Modul nimmt dafür die Bytes als Zahlen, mit Vorzeichen, wo der Befehl sie vorzeichenbehaftet liest (Versätze und Wölbungen im Zweierkomplement, Oktantwerte als Betrag mit Bit 7 für den Uhrzeigersinn). Liegt neben der SHX- eine gleichnamige SHP-Datei, gilt deren Text.
- Positionen sind Double-Zahlen mit y nach unten; jeder Rechenschritt läuft wie in der x87-Einheit, hier mit 53 Bit Genauigkeit, wie die mitgelieferten FHX-Dateien (von 2004) entstanden sind. Das Programm selbst stellt 64 Bit ein, mit denen auch seine HPGL-Ausgabe rechnet.
- Vektoren laufen in 16 Richtungen, die schrägen um tan 22,5° seitlich versetzt. Ein Bogen über Sehne und Wölbung erhält Radius und Mittelpunkt aus Sehne und Pfeilhöhe.
- Oktanten- und Bruchteilbögen bildet das Modul nicht wie das Original, sondern so, wie AutoCAD sie festlegt: Der Oktantwert wird gelesen wie bei AutoCAD (mit Minus und führender Null hexadezimal), sein Vorzeichen gibt die Richtung, seine Ziffern Startoktant und Zahl der Oktanten (0 für alle acht), und ein Bruchteilbogen endet in der letzten Oktante, die er erreicht. Ein Vollkreis besteht aus zwei Halbkreisen, weil ein Bogen, der endet, wo er beginnt, als Punkt geplottet würde. Das Original liest einen negativen Oktantwert dezimal, macht jeden dieser Bögen gegen den Uhrzeigersinn, einen Vollkreis zu einem Punkt und lässt Bruchteilbögen eine Oktante später enden. Die Winkel und Punkte rechnet das Modul ansonsten wie das Original.
- Unterzeichen werden an ihrer Stelle eingefügt, das Zurückholen einer gemerkten Position hebt den Stift, ein Befehl nur für senkrechten Text überspringt den folgenden Befehl (Folgen von Versätzen und Wölbungen nicht). Der Vorschub ist die gerundete Endposition.
- Danach tauschen 40 Zeichen vom DOS- an ihren Platz im Windows-Zeichensatz, etwa das ü von 129 nach 252. Mit ausgeschalteter Option „Strichschriften in DOS-Zeichenordnung“ unterbleibt der Tausch. Im Textdialog zeigt „SHX…“ wie im Original die Zeichen der gewählten Strichschrift als Tabelle; ein Klick setzt das Zeichen an der Schreibmarke in den Text. Unter „Optionen › Zeichenordnung der Strichschriften…“ lässt sich jede installierte Shape-Schrift einzeln auf DOS oder Windows stellen; die Liste zeigt dazu, ob ihre Umlaute an den Stellen von DOS oder von Windows liegen.

Aus den Quelltexten von DIN1451 und SCHREIB4 entstehen so die FHX-Dateien des Originals Bit für Bit (alle 238 Zeichen mit 3810 Linien und Bögen), und ein Text in einer dieser Schriften ergibt mit oder ohne FHX-Datei dieselbe HPGL-Datei.

Texte in Umrissschriften, in Strichschriften nach Unicode und in Bigfonts sowie in Strichschriften, deren Quelltext das Original nicht lesen kann, fährt das Werkzeug entlang ihrer Striche oder Umrisse, in Plottereinheiten fein unterteilt.

Gegen die HPGL-Datei des Originals zu seinem Gravurbeispiel sind 117 der 129 Wege in derselben Reihenfolge mit denselben Punkten und Befehlen ausgegeben, darunter alle Konturen, der Bogen mit 945 Punkten und die Beschriftung einer Skala in DIN1451. Die übrigen zwölf Wege sind die Beschriftung der anderen Skala; deren Strichschrift ARCH1 fehlt der Installation des Originals.

## Strichschriften

Für Gravuren verwendet das Original einlinige Schriften im Shape-Format von AutoCAD; ein Text mit Strichschrift trägt deren Namen in `strokeFont`. Gelesen werden kompilierte Schriften (`.shx`, Kopf `AutoCAD-86 shapes 1.0`/`1.1`, `AutoCAD-86 unifont 1.0` und `AutoCAD-86 bigfont 1.0`), Schriften als Quelltext (`.shp`) und die Schriftdateien des Originals (`.fhx`). Eine Schrift besteht aus nummerierten Zeichen; Zeichen 0 (bei Unicode-Schriften der Kopf) enthält Namen, Versalhöhe und Unterlänge. Jedes Zeichen ist eine Folge von Bytes: Vektorbytes (obere vier Bit Länge, untere vier Bit eine von 16 Richtungen), Stift ab/auf, Maßstab teilen/vervielfachen, Position merken/zurückholen (das Zurückholen bewegt den Stift, ohne zu zeichnen), Unterzeichen, Versätze, Oktantenbögen, Bruchteilbögen und Bögen über Sehne und Wölbung; Befehle nur für senkrechten Text werden übersprungen. Im Quelltext sind Zahlen mit führender Null hexadezimal. Negative Zahlen stehen in kompilierten Schriften im Zweierkomplement, Oktantwerte (Code 10 und 11) aber als Betrag mit gesetztem Bit 7 für den Uhrzeigersinn; im Quelltext steht dafür ein Minus, etwa `-043`.

Bigfonts (ostasiatische Schriften) nummerieren ihre Zeichen mit zwei Bytes, deren erstes in einem der Escape-Bereiche der Schrift liegt. Die kompilierte Datei enthält nach dem Kopf die Größe des Kopfteils, die Zahl der Zeichen und der Escape-Bereiche, die Bereiche (erstes und letztes Byte) und je Zeichen Nummer, Länge und Dateiposition; im Quelltext steht `*BIGFONT Zeichen,Bereiche,erstes,letztes,…`. Unterzeichen können dort in einen Kasten des Zeichens gezeichnet werden (Nummer mit zwei Bytes, Ecke, Höhe, bei erweiterten Bigfonts auch Breite); danach kehrt der Stift zurück. Welche Kodierung eine Bigfont verwendet, steht nicht in der Datei: Zeichen werden über Shift_JIS (bei japanischen Escape-Bereichen zuerst), GBK, Big5 und EUC-KR gesucht, soweit das System diese Kodierungen kennt, Zeichen unter 128 über ihren eigenen Code.

FrontDesigner liefert seine Strichschriften zusätzlich in einem eigenen Format (`.fhx`): derselbe typisierte Wertestrom wie in den FPL-Dateien, je Zeichencode von 1 bis 255 ein Datensatz aus Vorschub, Anzahl der Elemente und den Elementen (2 hebt den Stift, 0 ist eine Linie mit Anfang und Ende, 1 ein Bogen mit Anfang, Ende, Mittelpunkt, Radius und Drehsinn). Die Koordinaten zählen y nach unten, die Zeichen liegen in der Reihenfolge des Windows-Zeichensatzes. Versalhöhe und Unterlänge stehen nicht in der Datei; sie kommen aus einer gleichnamigen SHX- oder SHP-Datei oder sonst aus der Höhe des H und der Tiefe des p.

Gibt es zu einem Namen eine FHX-Datei, liefert sie die Zeichen, weil sie zeigt, wie das Original die Schrift zeichnet; sonst gilt eine SHX- vor einer SHP-Datei. Die Linien und Bögen einer FHX-Datei bleiben dazu wie gespeichert erhalten, damit die HPGL-Ausgabe sie wie das Original unterteilen kann. Gesucht wird eine Schrift nach dem Dateinamen ohne Endung im Ordner `Dokumente/OpenLoch/Strichschriften` und in weiteren Ordnern aus der Einstellung `frontpanel/strokeFontFolders`; „Optionen › Strichschrift installieren…“ kopiert gewählte Dateien in den eigenen Ordner, wie das Original sie in sein Programmverzeichnis übernimmt. Zeichen werden wie im Original über ihren Windows-1252-Code gesucht (Zeichen außerhalb davon als Fragezeichen), Zeichen ohne Striche gelten als Leerzeichen; Unicode-Schriften und Bigfonts nehmen den Unicode-Wert. Shape-Schriften nummerieren ihre Zeichen meist nach der DOS-Codetabelle; das Original nimmt das für jede Shape-Schrift an und tauscht 40 Zeichen an ihren Platz in Windows-1252 (siehe HPGL), sodass Zeichen, die dabei nicht vorkommen, wie µ bei vielen Schriften fehlen. Mit „Optionen › Strichschriften in DOS-Zeichenordnung (wie FrontDesigner)“, voreingestellt an, gilt das auch hier; ausgeschaltet gelten die Nummern der Zeichen als Windows-1252-Codes, für Schriften, die so geordnet sind. Bildschirm und Gravur folgen dabei immer derselben Zuordnung. Der Textrahmen fasst Versalhöhe plus Unterlänge, die Grundlinie liegt in Versalhöhe unter der Oberkante. Strichtexte werden mit dem Stift gezeichnet und nicht gefüllt (ohne sichtbaren Stift als dünne Linie in der Füllfarbe), graviert und gefräst wird entlang der Striche. Ein Strichtext ohne Schriftnamen verwendet wie im Original die Schrift DIN1451. Ist die Schrift nicht installiert, zeichnet das Modul den Text mit seiner eigenen Strichschrift, damit er einlinig bleibt und sich gravieren lässt.

Die eigene Schrift „Normschrift“ ist nach den Regeln technischer Beschriftung entworfen: Versalhöhe 10, Mittellänge 7, Unterlänge 3, zwei Einheiten Zeichenabstand, nur Linien sowie Kreis- und Ellipsenbögen; runde Buchstaben bestehen aus zwei Halbkreisen mit geraden Seiten. Sie enthält alle druckbaren ASCII- und Windows-1252-Zeichen mit Umlauten, Akzenten und Ligaturen, Hochzahlen und Brüche, die Zeichen Ω, µ, Δ, π, ⌀, ±, ×, ÷, ≤, ≥, ≈, ∞, √ und Pfeile. Sie erscheint in der Schriftliste, solange keine Datei gleichen Namens installiert ist.

Verwendet ein geöffnetes Dokument Schriften, die nicht installiert sind (Konturschriften des Systems oder Strichschriften), öffnet sich wie im Original von selbst die Übersicht „Verwendete Schriftarten“ (sonst unter „Optionen“), ohne die Arbeit zu blockieren; die betroffenen Texte erscheinen bis dahin in einer Ersatzschrift.

## Bildexport und Druck

Grafik exportieren schreibt PNG, JPG, BMP (Auflösung wählbar; der Dialog nennt Bildgröße und Speicherbedarf bei 24 Bit je Pixel wie das Original), EMF oder PDF (beide als Vektoren in der Größe der Frontplatte), wahlweise alle oder nur die markierten Objekte, mit oder ohne Hintergrund.

EMF-Dateien schreibt der gemeinsame Schreiber (`openloch::EmfDevice` in `src/emfwriter.h`, auch für die anderen Module); sie zählen in Hundertstel Millimetern; der Kopf beschreibt ein Bezugsgerät mit 100 Einheiten je Millimeter. Formen werden als Pfade mit Stift und Pinsel geschrieben, Texte als ihre Umrisse, Bilder als geräteunabhängige Bitmaps (mit Transparenz als Alpha-Überblendung, gedreht über eine Welttransformation). Damit jedes Programm die Datei gleich zeigt, enthält sie keine Beschneidung und keine Verlaufsdatensätze: Flächen werden auf den Beschneidungsbereich zugeschnitten, Linien an seinem Rand zerteilt, Verläufe als Streifen einfarbiger Flächen geschrieben (bis zu 128, etwa einer je zwei Farbstufen). Nur Bitmaps unter einer Beschneidung bekommen ein Beschneidungsrechteck oder einen Beschneidungspfad. Die Frontplatte schaltet beim Export `setSmoothImages` ein, damit Bilder auch in Programmen geglättet erscheinen, die Bitmaps wie GDI zeigen: deckende Bilder erhalten den Halbtonmodus (`SETSTRETCHBLTMODE`), Bilder mit Transparenz werden vorher auf zehn Pixel je Millimeter vergrößert, weil GDI eine Alpha-Überblendung nie glättet.

Vektorgrafiken (EMF) spielt das Modul ab wie GDI, über das auch das Original sie zeigt: mit Abbildungsmodus, Fenster und Viewport, Welttransformation, Stiften (auch erweiterten), Pinseln (auch Musterpinseln), Polygonen, Linienzügen, Bézierkurven, Rechtecken, abgerundeten Rechtecken, Ellipsen, Bögen, Sehnen, Tortenstücken, Pfaden (auch mit Text), Text (`EXTTEXTOUT`, `SMALLTEXTOUT`, logische Schriften), Farbverläufen (`GRADIENTFILL` mit Rechtecken und Dreiecken), Regionen (`FILLRGN`, `PAINTRGN`, `FRAMERGN`, `INVERTRGN`), Bitmaps (`STRETCHDIBITS`, `SETDIBITSTODEVICE` auch in Streifen, `BITBLT`, `STRETCHBLT`, `MASKBLT`, `PLGBLT`, `TRANSPARENTBLT`, `ALPHABLEND`, auch gedreht) und Beschneidung (Rechtecke, Regionen, Pfade). Bitmaps und Pinsel verbinden sich mit dem Untergrund nach ihrer Rasteroperation, also nach jeder der 16 Verknüpfungen von Bild oder Pinsel mit dem Ziel; Operationen, die Bild und Pinsel zugleich brauchen, kopieren das Bild. Vergrößerte Bitmaps zeigt das Modul wie GDI mit sichtbaren Pixeln; geglättet werden sie nur im Halbtonmodus (`SETSTRETCHBLTMODE` 4), bei `ALPHABLEND` nie. Wie GDI überspringt das Modul EMF+-Datensätze: Dateien mit EMF+ und EMF-Ersatz erscheinen über ihren EMF-Teil, reine EMF+-Dateien bleiben leer.

Die Druckvorschau bietet Normal oder Spiegeln, die Optionen Hintergrund, Rahmen, Lineale, Daten (Zeile mit Projekt, Frontplatte, Größe, Maßstab und Datum), Bemaßungen, Bohren und Fräsen, Objekte und Symbole, Schnittmarken und Texte, Originalgröße 1:1 oder Vergrößern, mittig ausrichten oder Abstand vom Papierrand (auch durch Ziehen in der Vorschau), Hoch- oder Querformat, Kacheln (Anzahl und Abstand), mehrere Exemplare, die gewählte oder alle Frontplatten und das Drucken nur eines Blattes, wenn der Ausdruck mehrere Blätter braucht. Korrekturfaktoren für die waagerechte und senkrechte Richtung gleichen Drucker aus, die Längen etwas verzerren.

Die Druckeinstellungen gehören wie im Original zur Frontplatte und werden mit ihr gespeichert, auch in FPL-Dateien; nur die Korrekturfaktoren gelten für alle. Die Vorschau zeigt die Einstellungen der gewählten Frontplatte, ein Wechsel der Frontplatte bringt deren Einstellungen und Papierlage mit; „von allen Frontplatten“ druckt jede mit ihren eigenen. Änderungen übernimmt der Editor beim Schließen der Vorschau, auch nach Abbrechen, als einen Bearbeitungsschritt. Vorgaben einer neuen Frontplatte: alle Optionen außer Spiegeln, nur ein Blatt und Querformat, Originalgröße, mittig ausrichten, eine Kachel ohne Abstand, die Frontplatte 20 mm vom Rand des bedruckbaren Bereichs. Die Lage der Frontplatte und die Kachelabstände sind Frontplattenmillimeter, gemessen von der Ecke des bedruckbaren Bereichs; gedruckt werden sie mit dem Maßstab vervielfacht. Die Abstandsfelder zeigen, wie weit die Frontplatte vom Papierrand liegt. Die Lage darf auch außerhalb des bedruckbaren Bereichs liegen, etwa für Etikettenbögen mit schmalem Rand; was der Drucker nicht erreicht, fehlt dann im Ausdruck.

## Prüfung

`openloch_frontpanel_tests` prüft Modell, natives Format (Rundlauf und Ablehnung ungültiger Eingaben), Geometrie unter Abbildungen, Darstellung, Rückgängig, Generatoren (darunter die Maßregeln aller Skalenstile und Skaleneinstellungsdateien in beiden Richtungen), EMF-Export im Rundlauf gegen die direkte Darstellung und die Wiedergabe von Verläufen, Regionen, Alpha-Bitmaps, Rasteroperationen, Masken, Streifen und Parallelogrammen bei Bitmaps, Pfaden aus mehreren Figuren und Text in Pfaden, den Wertestrom, Byte für Byte erzeugte Dateien der Versionen 1,00 bis 3,04 und 3,16, abgeschnittene Dateien, den Rundlauf aller Objektarten durch FPL und LIB, den bytegleichen Wiederexport, Bearbeitungsjobs und HPGL-Dateien (auch wieder eingelesen und mit den Bearbeitungswegen verglichen), die 80-Bit-Rechnung und die Umrissbildung des Originals gegen unabhängig mit exakten Brüchen berechnete Werte, die Wege von Objekten und ihren FPL-Kopien, eine eigene kleine Strichschrift als SHP, SHX und im FHX-Format, ihre Umwandlung in Linien und Bögen wie im Original (Lesen der Zahlen, Vektoren, Versätze, Wölbungen, Oktanten- und Bruchteilbögen in beiden Richtungen und als Vollkreis, Unterzeichen, gemerkte Positionen, Maßstab, Vorschub, Umsortierung nach DOS- oder Windows-Ordnung, unlesbarer Text), Bigfonts, die mitgelieferte Normschrift (Zeichenvorrat, Zellen, Ersatz und Gravur), das Drucklayout, Druckeinstellungen je Frontplatte (in FPL, im eigenen Format und in der Vorschau) und den Editor in einem Fenster ohne Bildschirm (Zeichnen mit der Maus, Verschieben im Raster, Anordnen, Gruppen und Kombinationen, Zwischenablage, Frontplatten, Speichern und Öffnen, Sicherung, Bibliotheksseite). Dazu kommen die englischen Texte aller `ui()`-Aufrufe in `src/modules/frontpanel` und `src/formats/frontdesigner`. Alle Testdaten entstehen im Test selbst; Einstellungen gehen in einen temporären Ordner.

Ist `OPENLOCH_FRONTDESIGNER_CORPUS` auf einen lokalen Ordner mit FPL- und LIB-Dateien gesetzt, liest der Test jede Datei, schreibt sie wieder, vergleicht 3,16-Dateien Byte für Byte und alle anderen inhaltlich. Mit `OPENLOCH_FRONTDESIGNER_WRITE` schreibt er jede Datei zusätzlich vollständig aus dem Modell neu und eine Beispieldatei mit hier erzeugten Objekten in den angegebenen Ordner, damit andere Leser sie prüfen können. Solche Dateien gehören nicht ins Repository. Mit `OPENLOCH_FRONTPANEL_SCREENSHOT` schreibt der Test ein Bild des Editorfensters an den angegebenen Pfad, mit `OPENLOCH_FRONTPANEL_BOARDS_SCREENSHOT` eines mit einer Platine dahinter und Bohrungen aus ihr. Mit `OPENLOCH_STROKE_TABLE_SCREENSHOT` legt der Test ein Bild der Zeichentabelle ab. Entsprechend liest `OPENLOCH_STROKE_FONT_CORPUS` alle Strichschriften eines lokalen Ordners und vergleicht bei einer FHX-Datei mit gleichnamiger SHP-Datei die Umwandlung Bit für Bit mit der FHX-Datei; mit `OPENLOCH_STROKE_FONT_SAMPLES` zeichnet der Test von jeder eine Probezeile in den angegebenen Ordner.

## Bewusste Abweichungen

- Einige Felder erlauben mehr als im Original: der Ursprung ±10 000 mm statt 0 bis 600 mm, Texthöhe der Bemaßung und Schrifthöhe ab 0,5 statt 1 mm, Kacheln bis 100 statt 20, AutoSpeichern alle 1 bis 240 statt 1 bis 60 Minuten.
- Fortschrittsfenster beim Öffnen und beim Installieren von Strichschriften entfallen: OpenLoch öffnet ohne merkliche Wartezeit und liest kompilierte Strichschriften (SHX) direkt, ohne sie umzuwandeln.
- Die Sprache (Deutsch, Englisch, Französisch) steht unter „Optionen › Sprache“ statt in einem eigenen Dialog und gilt ab dem nächsten Start für alle Fenster.

## Noch offen

- Drucker und PDF kennen keine Rasteroperationen: Dort werden UND, ODER und Exklusiv-ODER durch Mischmodi angenähert, ein invertiertes Bild wird vorher invertiert, die übrigen Operationen kopieren.
- Fehlt eine Strichschrift, steht die eigene Normschrift für sie ein; ihre Buchstaben werden in den Textrahmen gestreckt und sehen anders aus als die der fehlenden Schrift. Zweibyte-Zeichen von Bigfonts erscheinen nur, wenn das System die passende Kodierung kennt.
- Skalen aus FrontDesigner-Dateien bleiben Gruppen ihrer Teile, weil die Dateien die Einstellungen des Assistenten nicht enthalten.
- Textrahmen der Beschriftung von Skalen können um Bruchteile eines Millimeters von denen des Originals abweichen.
- Dateien vor Version 3,04 sind nur aus der Beschreibung des Formats abgeleitet: Die Tests lesen Byte für Byte gebaute Dateien aller Versionszweige von 1,00 bis 3,03, Dateien des Originals in diesen Versionen lagen nicht vor.
- HPGL: Texte in Umrissschriften weichen ab, weil das Original dafür die Glyphenumrisse von Windows verwendet. Für kompilierte Strichschriften (SHX) fehlt der Demoversion das Hilfsprogramm, das sie in Quelltext übersetzt; dass es die Zahlen so schreibt, wie das Modul sie annimmt, ist nicht belegt. Ob das Original eine neue Strichschrift mit 53 oder mit 64 Bit Genauigkeit umwandelt, zeigen nur seine FHX-Dateien von 2004; der Unterschied betrifft höchstens die letzte Stelle einzelner Mittelpunkte und Radien. Die vorliegende Fassung des Originals (Demoversion) enthält Jobliste, Ausgabe der Wege und Plotdatei, aber nicht den abschließenden Aufruf, der für jeden Job die Datei anlegt. Nicht belegt sind daher die Stelle der Markierung „Gemeinsamer Ursprung“ im Befehlsstrom (hier vor dem ersten Weg; nach dem Handbuch ein Senken und Heben bei (0;0)) und der Bezug der y-Achse bei Plattenhöhen, die keine ganze Zahl von Fünfzigstel Millimetern sind. Dass sich die Koordinaten auch ohne diese Markierung auf die linke untere Plattenecke beziehen, zeigt die Beispieldatei des Originals.
- HPGL-Dateien und Ausdrucke sind als Dateien und Bildschirmvorschau geprüft, nicht an einer Maschine und nicht mit einem nachgemessenen Ausdruck.

## Anbindung an gemeinsamen Code

Das Modul ändert keinen gemeinsamen Code; die Suite verwendet `PanelEditor` als Fenster und ordnet Dateien über `readFrontDesigner()` und `Document::decode()` zu. Für die weitere Anbindung:

- **Fenster:** `PanelEditor` ist ein `QMainWindow` mit eigener Menüleiste und eigenen Werkzeugleisten; `titleChanged` meldet Dateiname und Änderungsstand. Soll er einmal in ein anderes Hauptfenster eingebettet werden, braucht es eine Regel für die Menüleiste (unter macOS gibt es nur eine).
- **Einstellungen:** Alle Werte liegen in `QSettings` unter `frontpanel/…` (Fensteraufteilung, Stile, Bibliotheksordner, Vorgaben für Stifte, Füllungen und Schriften, Skaleneinstellungen je Stil, Bemaßung, Korrekturfaktoren des Drucks, HPGL). Sie setzen Organisations- und Programmnamen der Anwendung voraus.
- **Übersetzungen:** Die Texte des Moduls stehen englisch in `scripts/translations/frontpanel.py` und französisch in `frontpanel_fr.py`. Texte, die schon eine andere Tabelle übersetzt (Lochraster, Suite, Leiterplatte, Schaltplan), stehen nur dort, weil `generate.py` zwei verschiedene Übersetzungen desselben Texts ablehnt.
- **Projektbehälter:** Das Dokument gibt es auch als JSON-Objekt: `Document::toJson()` und, mit derselben Prüfung wie beim Lesen einer Datei, `Document::fromJson()`. Dieses Objekt ist unverändert der Inhalt einer `.olfp`-Datei und die `data` des Dokuments (Art `frontpanel`) im Projekt.
- **Speichern über die Suite:** Ist `PanelEditor::saveHandler` gesetzt, ruft der Editor bei „Speichern“ und „Speichern unter“ (auch beim Schließen mit ungesicherten Änderungen) diese Funktion statt selbst zu schreiben, mit `asNew` für „Speichern unter“, und gibt ihr Ergebnis zurück. Die Suite holt die aktuellen Daten jederzeit mit `documentData()` und meldet mit `markSaved()`, dass gespeichert ist; erst dann gilt das Dokument als unverändert. Die Sicherungsdateien (`.BAK`) schreibt der Editor weiter selbst.
- **Kennungen:** Dokument, Frontplatten und Objekte haben stabile Kennungen aus 32 Hexadezimalziffern (`id`). Sie überstehen Bearbeiten, Speichern und Rückgängig; Kopien erhalten neue. Eine Verknüpfung wie „Platine hinter Frontplatte“ kann eine Frontplatte über ihre `id` nennen.
- **Bauteil-Schnittstelle (`panelboards.h`):** Ein Bauteil erscheint in der Frontplatte als Symbol, meist eine Gruppe aus Bohrung, Ausschnitt und Beschriftung (etwa ein Potentiometer), sonst ein einzelnes Objekt. Diese Gruppe oder dieses Objekt trägt das Feld `component` mit der Kennung des Bauteils; Anschlüsse (`pin`) braucht die Frontplatte nicht. `panelComponents()` zählt die Objekte einer Frontplatte mit Bauteil auf, mit der Stelle, an der sie sitzen (`componentPoint()`: Mitte einer Bohrung, sonst Einfügepunkt eines Symbols, sonst Mitte seiner Umgrenzung); `componentOf()` nennt zu einem Objekt sein Bauteil (das des nächsten Objekts mit `component` auf dem Weg von ihm zur obersten Gruppe). Eine Kopie innerhalb des Dokuments ist ein neues Bauteil und erhält eine neue Bauteilkennung. Noch eingeplant: ein Bauteil aus einem Bibliothekseintrag platzieren (seine Frontplattendarstellung ist ein Symbol mit Einfügepunkt) und Kennung und Wert aus dem Projekt in dafür gekennzeichnete Texte des Symbols übernehmen.
