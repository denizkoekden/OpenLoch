# OpenLoch: zwei Schaltungen für Punktraster

Beide Entwürfe verwenden handelsübliche **einseitige Punktrasterplatinen mit einzelnen isolierten Lötaugen und 2,54-mm-Raster**. Das Blinklicht ist für 100 x 80 mm, das Lauflicht für 160 x 100 mm vorgesehen. Die benötigten Lochbereiche sind kleiner als die Außenmaße; vor dem Bestücken das tatsächliche Lochbild und die Bauteilabmessungen mit den Plänen vergleichen.

Beide Entwürfe sind am Rechner geprüft: Rasterlage, Gehäuseabstände, Verbindungen und Kurzschlüsse an Drahtenden. Das Blinklicht ist mit generischen SPICE-Modellen simuliert, beim Lauflicht sind Pinbelegung und LED-Treiber einzeln geprüft. Aufgebaut wurden die Schaltungen noch nicht.

## Dateien öffnen

| Schaltung | Native Datei | LochMaster-Export |
| :--- | :--- | :--- |
| Blinklicht | [Projekt](01-Zweitransistor-Blinklicht.openloch) | [LM4](01-Zweitransistor-Blinklicht.LM4) |
| Zehnkanal-Lauflicht | [Projekt](02-Zehnkanal-Lauflicht.openloch) | [LM4](02-Zehnkanal-Lauflicht.LM4) |

Das [sechsseitige Bauheft](OpenLoch-Bauheft.pdf) enthält nummerierte Bestückungsansichten. Die eigenen Footprints sind in den Projekten eingebettet und zusätzlich als [LIB](OpenLoch-Praxis-Footprints.LIB) vorhanden. Zum Öffnen wird keine Herstellerinstallation benötigt.

## Pläne und Koordinaten

- `.openloch`: bearbeitbares Projekt einschließlich der selbst gezeichneten Bauteilbibliothek.
- `.LM4`: dasselbe Projekt für LochMaster.
- `-Bestueckung.png/.pdf`: Bestückungsansicht; beide Drahtseiten sind sichtbar.
- `-Loetseite.png/.pdf`: gespiegelte Ansicht von unten. Grüne isolierte Brücken sind dort nur an ihren Anschlüssen relevant.
- `-Stueckliste.csv`, `-Anschlussliste.csv`, `-Drahtliste.csv`: Bauteile, einzelne Pins und jede gerade Drahtstrecke.
- `-Aufbaupruefung.json` und `-Geometriepruefung.json`: nachvollziehbare Prüfergebnisse.

**Alle Tabellen zählen von der Bestückungsseite aus:** X nach rechts, Y nach unten. Die erste tatsächlich vorhandene Lochspalte und Lochreihe heißen jeweils 1. Randabstände handelsüblicher Platinen variieren; es zählt das Loch, nicht ein vom Außenrand gemessener Millimeterwert. Beim Blick auf die Unterseite vertauschen sich links und rechts. Die Tabellenkoordinaten bleiben unverändert.

Die PDF-Übersichten sind keine garantierten 1:1-Bohrschablonen. Für maßstäblichen Ausdruck den Druckdialog von OpenLoch verwenden und den Abstand benachbarter Löcher am Ausdruck auf 2,54 mm kontrollieren.

## Gemeinsamer Aufbau

1. Versorgung ausgeschaltet lassen. Platine von oben markieren: links oben, X/Y-Richtung, J1 Pin 1 = +5 V, Pin 2 = GND.
2. Alle Anschlüsse zunächst anhand der Anschlussliste markieren. Widerstände axial auf 10,16 mm Raster biegen. BC547-Beine vorsichtig auf 2,54 mm auffächern: bei der eingezeichneten flachen Gehäuseseite nach unten C-B-E von links nach rechts. Pinbelegung des tatsächlich verwendeten Herstellers prüfen.
3. Widerstände, IC-Sockel und niedrige Bauteile einsetzen. Anschließend Kondensatoren, LEDs, Transistoren und Stiftleisten bestücken. Die eingezeichneten radialen Elkos benötigen höchstens 6,3 mm Durchmesser; Widerstände höchstens etwa 7 mm Körperlänge.
4. Auf der Lötseite gerade Strecken aus verzinntem Kupferdraht (etwa 0,3 mm) auf die Lötaugen legen und verlöten. Sie müssen nicht zusätzlich durch bereits belegte Bauteillöcher gesteckt werden. Knicke, Abzweige und Bauteilanschlüsse sind echte Lötverbindungen. Lange Strecken an Zwischenaugen befestigen.
5. Grüne Strecken auf der Bestückungsseite mit dünnem **isoliertem** Kupferdraht herstellen, etwa 0,25-0,3 mm Leiterdurchmesser. Nur die Enden abisolieren. An GND-Abzweigen dürfen aneinander anschließende Teilstücke zu einem durchgehenden Draht mit kurzen, gezielt abisolierten Lötstellen zusammengefasst werden. Die Drahtliste nennt alle gemeinsamen Endpunkte. Keine blanke Brücke über fremde Netze legen.
6. Die Oberseitenbrücken bleiben gerade und folgen den eingezeichneten Reihen bzw. Spalten. Kreuzungen mit Leitungen auf der anderen Platinenseite sind durch Platine und Isolierung getrennt. Jede Verbindung zwischen den Seiten entsteht ausschließlich an einem vorgesehenen End-/Abzweigpunkt.
7. Vor dem Einschalten mit dem Durchgangsprüfer jeden Tabellen-Netznamen kontrollieren. VCC und GND dürfen nicht kurzgeschlossen sein; Kondensatoren können beim Messen kurz laden. LEDs, Elko-Polarität und Transistoranschlüsse nochmals prüfen.
8. Mit geregelten 5 V DC und zunächst etwa 20 mA Strombegrenzung testen. Betrieb nur im Bereich 4,75-5,25 V. Die Entwürfe sind nicht für eine 9-V-Batterie ausgelegt.

## 1. Zweitransistor-Blinklicht

Zwei BC547B treiben abwechselnd rote LEDs. R1/R2 = 470 Ohm, R3/R4 = 47 kOhm. C1/C2 sind **10 uF bipolare Kondensatoren**, mindestens 25 V. Normale gepolte Elkos an diesen beiden Positionen nicht ungeprüft einsetzen. C3 = 100 nF, C4 = 47 uF/16 V mit Plus links.

Die beiden geraden Versorgungsschienen liegen bei Y=3 (+5 V) und Y=24 (GND), jeweils X=3 bis X=29. Es gibt genau eine isolierte Oberseitenbrücke: **VCC von (7,21) nach (9,21)**. Sie kreuzt unten verlaufende Leitungen und muss isoliert bleiben.

Die generische SPICE-Simulation ergibt nominal eine Periode von etwa **0,463 s pro LED**, entsprechend **2,16 Hz**. Bei gleichzeitig variierten 5%-Widerständen, 20%-Zeitkondensatoren und 4,75-5,25 V ergeben die geprüften Eckfälle 0,363-0,563 s und Spitzenströme von 5,29-6,92 mA. Kurze Überlappung der LED-Ströme beim Umschalten ist bei dieser einfachen Schaltung vorhanden. Reale Blinkzeit und Helligkeit hängen von den verwendeten Bauteilen ab. Die simulierte negative Basisspannung bleibt oberhalb -2,81 V; diese Aussage ist ein Modellergebnis, kein Messwert.

## 2. Zehnkanal-Lauflicht

U1 ist ein **TLC555CP im DIP8-Gehäuse**, U2 ein **CD4017BE im DIP16-Gehäuse**. U1-Kerbe nach oben; **U2-Kerbe nach links**, weil U2 um 90 Grad gedreht eingebaut ist. Beide ICs in Sockel setzen. Einen bipolaren NE555 nicht ohne erneute Prüfung der Logikpegel einsetzen.

Die zehn LED-Stufen sind gleich aufgebaut und stehen von D1 bis D10 nebeneinander. R4-R13 = 1,5 kOhm als LED-Vorwiderstände; R14-R23 = 22 kOhm als Basiswiderstände; Q1-Q10 = BC547B. Hocheffiziente rote LEDs verwenden. Die unabhängige Treibersimulation ergibt in den geprüften Varianten etwa **1,67-2,18 mA** LED-Strom und weniger als 0,25 mA Basisstrom.

Die Taktfrequenz folgt näherungsweise f = 1,44 / ((RA + 2 RB) C): RA=R1=10 kOhm, RB=R2=100 kOhm, C=C1=1 uF. Nominal **6,86 Hz**, also etwa **1,46 s für einen Umlauf**. Allein die geprüfte RC-Toleranz ergibt etwa 5,44-9,02 Hz; IC-Toleranzen und Elko-Leckstrom sind darin nicht enthalten.

Die CD4017-Ausgänge 0 bis 9 liegen an den Pins **3,2,4,7,10,1,5,6,9,11**. Sie treiben D1 bis D10 in dieser Reihenfolge. Pin 13 liegt fest an GND, Pin 12 (Carry Out) bleibt offen. Zehn parallele Signalstrecken liegen unten; die jeweiligen senkrechten Verbindungen zu den Basiswiderständen liegen isoliert oben. Die obere Masseschiene bei Y=16 ist ebenfalls isoliert und hat vorgesehene Löt-Abzweige.

Reset: R3=100 kOhm nach GND, C6=100 nF nach VCC. Ein externer Schließer-Taster verbindet J2 Pin 1 (+5 V) mit J2 Pin 2 (RESET). Beim ersten Einschalten und bei langsam ansteigender Versorgung den Reset-Taster kurz drücken. Ein RC-Reset allein garantiert bei jeder Versorgungskurve keinen definierten Start. Danach muss D1 beginnen; bei jedem steigenden Takt folgt die nächste LED.

Zusätzlich zur Bauteilliste werden ein DIP8-Sockel, ein DIP16-Sockel, ein externer Reset-Taster, Anschlussleitung und die geregelte 5-V-Versorgung benötigt. Die SPICE-Prüfung der Treiber ersetzt keine vollständige Simulation von TLC555 und CD4017; ein realer Funktionstest steht aus.

## Herstellerunterlagen

- [Texas Instruments: TLC555, Pinbelegung, Logikpegel und astabiler Betrieb](https://www.ti.com/lit/ds/symlink/tlc555.pdf)
- [Texas Instruments: CD4017B, Ausgangsfolge und Strombelastbarkeit](https://www.ti.com/lit/ds/symlink/cd4017b.pdf)
- [onsemi: BC547, TO-92 Style 17 und elektrische Kenndaten](https://www.onsemi.com/pdf/datasheet/bc546-d.pdf)

Pläne, Footprints und Prüfdateien sind eigene OpenLoch-Arbeit. Herstellerbibliotheken und Originalprogrammdateien sind nicht enthalten.

## Prüfung reproduzieren

Im Repository `python3 tools/validate_perfboard_examples.py` ausführen. Das Werkzeug liest die tatsächlichen LM4-Dateien mit dem unabhängigen Python-Leser, vergleicht Pins und Drahtseiten, prüft die Netzgraphen und kontrolliert die Prüfsummen der nativen Geometrieberichte. Ein bearbeitetes Projekt benötigt einen neuen Geometriebericht; alte Berichte werden nicht als Nachweis akzeptiert. Die Prüfung ist auf diese beiden Bauentwürfe zugeschnitten und noch kein allgemeiner Design-Regelprüfer in OpenLoch.

Die `.cir`-Dateien lassen sich mit ngspice ausführen. Ausgabedateien werden relativ zum Arbeitsverzeichnis geschrieben. Das Blinklicht-Netz enthält die tatsächliche Verdrahtung; die Lauflicht-Prüfung betrachtet die zehn LED-Treiber mit vorgegebenen Ausgangspegeln, ohne vollständige IC-Simulation. Die verwendeten Modelle sind generisch und keine qualifizierten Hersteller-SPICE-Modelle.
