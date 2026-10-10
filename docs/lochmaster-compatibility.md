# Vergleich mit LochMaster 4

OpenLoch soll alles können, was LochMaster 4.0 kann, und sich dabei genauso bedienen lassen. Diese Übersicht zeigt, was vorhanden ist und was noch fehlt. Die Liste der offenen Punkte ist zugleich der Arbeitsplan für den Lochraster-Teil (siehe [Roadmap](roadmap.md)).

## Funktionen

| Bereich | Vorhanden | Offen |
| --- | --- | --- |
| Dateien | LM4, LMB und LIB der Versionen 3.08 bis 4.07 lesen und schreiben, mehrere Platinen in einem Projekt, LM4 als normales Speicherformat, `<Name>.OLD` beim Öffnen wie im Original, AutoSpeichern mit `<Name>.BAK`, Sicherungsdateien öffnen, eigenes Format `.openloch`, automatische Wiederherstellung | – |
| Bibliothek | Bauteilfenster mit Seitenauswahl, Vorschau, Klicken oder Ziehen zum Platzieren, eigene Seiten anlegen, ordnen und löschen, Seiteneigenschaften mit Name und Extrafeldern, Bauteile über den Bauteil-Dialog hinzufügen, bearbeiten und löschen, LochMaster-Bibliotheken einbinden, eigene Bibliothek mit allen Seiten des Originals | – |
| Objekt-Assistent | Alle 15 Objekte mit den Parametern, Grenzen und Standardwerten des Originals: Konturen und liegende Bauteile mit Farbringen | – |
| Bauteile | Bauteil-Dialog mit Extrafeldern, Bauteileinheit bilden und aufheben, Gruppen, Platzhalter in Texten, Kennungen wie `R#` mit laufender Nummer, Neu nummerieren in der Reihenfolge des Dokuments auch für verschachtelte Bauteile, Eigenschaften verschachtelter Bauteile per Doppelklick im Objektbaum, Gruppen mit Name, Abmessungen (skalieren) und Position | – |
| Zeichnen | Leitungen, Lötpunkte, Trennstellen, Bohrungen, Texte, Rechtecke, Ellipsen, Polygone, offene Konturen, Pins, Anschlussdrähte, Lötstellen, Potentialmarken, Linien- und Füllfarben, Linienbreiten bis „unsichtbar“, Füllungen, Konturglättung (B-Spline, Fase, Rundung), Fräskonturen, Bitmapfüllungen auch für Kreise, Lötstellen mit 3D-Bild (eigene Zeichnung) | – |
| Bearbeiten | Auswahl, Verschieben, Drehen (90°, 180°, beliebig), Spiegeln, andere Platinenseite, Ausrichten, Ebenenfolge, Duplizieren, Zwischenablage, Knotengriffe, Rückgängig und Wiederholen mit wählbarer Zahl der Schritte | – |
| Platine | Vorlagen, Layout-Editor mit Kupferflächen, Leiterbahnen, Lötaugen und Bohrungen, Platine drehen, mehrere Platinen, Eigenschaften mit Größe, Lochabstand, Fangrastern, Ursprung, Versatz und Extrafeldern, Ursprung setzen, Anmerkungen mit Formatierung, Lineal und Einzügen sowie eingefügter Stück- und Einkaufsliste, Objektbaum | – |
| Ansichten | Wenden, Durchsicht, Röntgenblick mit Kontrast, BMP-Rendering (auch für die Bauteilvorschau der Bibliothek), S/W, Zoom, Einheiten mm, inch und N mit ihren Fangrastern (Umschalt fängt nicht), Lineale, Ansicht und Einheit je Platine, Platinenfarben des Originals | – |
| Prüfen | Durchgangsprüfer, Potentialanzeige, freie Bereiche | – |
| Stückliste | Stückliste und Einkaufsliste wie im Original, „Excel erzeugen…“ öffnet die Tabelle in der Tabellenkalkulation (CSV) und legt sie in die Zwischenablage | – |
| Ausgabe | Druckvorschau mit bis zu zehn Ansichten je Platine, Ebenen, Maßstab, Lage, Kacheln, mehreren Blättern, Schnittmarken, Datenfeld, Exemplaren und Korrekturfaktoren; Export als PNG, BMP, JPG und PDF; HPGL-Dateien zum Bohren und Fräsen | Ausdrucke und HPGL-Dateien mit denen des Originals vergleichen, Maßhaltigkeit auf Papier prüfen |
| Oberfläche | Menüs, Kontextmenüs, Symbolleisten (auch „Farben“, „Breite“, „Füllen“ und „Fräsen“), Werkzeugpalette und Mauszeiger wie im Original (eigene Symbole), Werkzeuge ein- und ausblenden, Platinen als Reiter, Einheitenknopf an den Linealen, eigene Hilfe (F1), deutsch, englisch und französisch | – |
| Plattformen | macOS, Windows und Linux aus demselben Code, automatische Builds und Tests, Pakete als DMG, ZIP und AppImage | Abnahme auf Windows 10/11, Intel-Macs und Linux, Notarisierung für macOS |

Registrierung, Hersteller-Updates und der Info-Dialog des Herstellers werden nicht nachgebaut.

## Erweiterungen gegenüber dem Original

- Eigene Bauteilbibliothek, auch ohne LochMaster-Installation, mit zusätzlichen Seiten für moderne Bauteile wie ESP32, Raspberry Pi Pico und Sensormodule
- Bauteilordner mit einer LIB-Datei je Bauteil; Unterordner erscheinen als Seiten, Änderungen im Dateimanager ohne Neustart
- Bitmapfüllungen aus PNG, GIF und JPEG; durchsichtige Stellen werden zum Umriss der Fläche, damit das Bild weder Kupfer noch Bohrungen verdeckt
- Ein beliebiger Anschluss als Bezugspunkt eines Bauteils; er wird dessen erster Anschluss und bleibt so auch in LM4-Dateien erhalten
- Kurzschlussprüfung zwischen Netzen mit verschiedenen Potentialen
- Englische Oberfläche, dunkler Modus, PDF-Ausgabe

## Bewusste Abweichungen

- Tastenkürzel folgen der Plattform: Rückgängig liegt auf Strg+Z (macOS: ⌘Z), „Nach vorne setzen“ und „Nach hinten setzen“ auf Strg+] und Strg+[. Im Original legt Strg+Z ein Objekt nach vorne; Strg+H und Strg+M sind unter macOS belegt. Strg+W und Strg+Q (im Original: Bauteil in der Bibliothek eins nach oben bzw. unten setzen) bleiben Fenster schließen und Beenden; die beiden Befehle stehen ohne Kürzel im Menü. F1 öffnet die Hilfe auf allen Systemen.
- Wird die Zahl der Rückgängig-Schritte verkleinert, fallen nur die ältesten Schritte weg; das Original leert die Liste.
- Die Druckvorschau öffnet auch ohne installierten Drucker und druckt dann in eine PDF-Datei. „Abbrechen“ beendet den Druck nach dem laufenden Blatt.
- Gespeichert wird zusätzlich automatisch für die Wiederherstellung nach einem Absturz.
- AutoSpeichern legt die `.BAK`-Datei nur an, wenn sich seit dem letzten Speichern etwas geändert hat, und gilt nicht als Speichern: Beim Schließen fragt OpenLoch weiter nach ungespeicherten Änderungen.
- Die Einkaufsliste löst Bauteileinheiten auch in den letzten Einträgen auf.
- Beschriftungen ohne gespeicherte Ecken (neue Texte, Dateien vor 4.04) legt das Original beim ersten Zeichnen an und misst ihre Breite in der Bildschirmauflösung der gerade eingestellten Zoomstufe; dieselbe Datei erhält so je nach Zoom um einige Zehntelmillimeter verschiedene Breiten. OpenLoch rechnet mit der Breite der Schrift ohne Rasterung, so wie das Original bei starker Vergrößerung misst. Eigene Beschriftungen schreibt OpenLoch mit zusammengefallenen Ecken, damit das Original sie selbst anlegt.
