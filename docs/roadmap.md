# Roadmap

OpenLoch soll eine kleine, vollständige Elektronik-Werkstatt sein: Schaltplan, Lochrasterplatine, Leiterplatte und Frontplatte in einem Programm, mit der vertrauten Bedienung der ABACOM-Programme sPlan, LochMaster, Sprint-Layout und FrontDesigner. Es richtet sich an Hobbybastler, Lernende und alle, die diese Programme kennen und heute auf macOS, Linux oder einem aktuellen Windows arbeiten.

## 1. Die vier Module

Alle vier Module sind benutzbar und lesen und schreiben die Dateien ihrer Vorbilder:

- **Schaltplan (sPlan 7 und 8):** mehrseitige Pläne mit Formblatt, Symbolbibliothek, Netze und Stückliste; offene Punkte stehen unter [Schaltplan](modules/schematic.md).
- **Lochraster (LochMaster 4):** der Funktionsumfang des Originals, siehe [Vergleich mit LochMaster](lochmaster-compatibility.md).
- **Leiterplatte (Sprint-Layout 6):** Kupferlagen, Pads, Leiterbahnen, AutoMasse, Autorouter, Design Rule Check und Fertigungsdaten; offene Punkte unter [Leiterplatte](modules/pcb.md).
- **Frontplatte (FrontDesigner 3):** Konturen, Beschriftungen, Skalen, Bohrungen, Gravur und Fräsen; offene Punkte unter [Frontplatte](modules/frontpanel.md).

Jedes Modul ist systematisch mit seinem Vorbild abgeglichen, über alle Menüs, Werkzeuge, Dialoge und Eigenschaften, jeder Punkt mit Fundstelle; die gefundenen Lücken sind geschlossen, bewusste Abweichungen stehen auf den Seiten der Module. Bis zur vollständigen Übereinstimmung mit den Originalen fehlen vor allem Abnahmen, die das Original selbst brauchen: Ausdrucke und Fertigungsdateien neben die des Originals legen, die Maßhaltigkeit auf Papier und an der Maschine prüfen, mit Projekten von Nutzern testen und Windows 10/11, Intel-Macs und Linux von Hand durchgehen.

## 2. Ein Programm, ein Projekt

OpenLoch startet mit einem Startbildschirm, auf dem man wählt, was man bauen möchte, oder eine Datei öffnet. Ein Projekt (`.openloch`) hält die Dokumente einer Schaltung zusammen ([Dateiformate und Projekte](file-formats.md)). Vorhanden:

- Projekte mit beliebig vielen Dokumenten der vier Arten, die Dateien der Vorbilder bleiben nutzbar; unter macOS öffnet ein Doppelklick im Finder sie in OpenLoch.
- Vom Schaltplan zur Platine: Lochraster und Leiterplatte vergleichen sich mit den Netzen des Schaltplans, zeigen Luftlinien, übernehmen Bezeichner und Werte und legen fehlende Bauteile neben die Platine ([Soll-Verbindungen](suite.md#soll-verbindungen-aus-dem-schaltplan)).
- Projektübersicht: die Dokumente eines Projekts und die Bauteile über alle Dokumente, mit einer Stückliste für das ganze Projekt.
- Öffnen und Bibliotheken: Jedes Fenster öffnet jedes Dokument (die eigene Art zuerst im Dateidialog), eine Übersicht zeigt die Bibliotheksordner aller Dokumentarten mit festen Ordnernamen unter Dokumente/OpenLoch ([Dateidialoge und Bibliotheken](suite.md#dateidialoge-und-bibliotheken)).
- Frontplatte und Platine: Die Frontplatte zeigt die Platinen dahinter, setzt Bohrungen für LEDs, Potis, Schalter und Buchsen über ihre Bauteile, nimmt sie mit, wenn die Platine wandert, und vergleicht beides ([Frontplatte und Platine](suite.md#frontplatte-und-platine)).

Als Nächstes:

- **Gemeinsame Bauteile:** ein Bauteilregister im Projekt, das Bezeichner und Werte in allen Dokumenten gleich hält, in beide Richtungen, mit einer Stückliste für das ganze Projekt ([Entwurf](suite.md#bauteilregister-entwurf)).
- **Frontplattensymbole:** Bauteile mit ihrer Frontplattendarstellung aus der Bibliothek (Bohrung, Ausschnitt und Beschriftung als Symbol) statt einfacher Bohrungen.

## 3. Über die Vorbilder hinaus

- **Prüfen:** Kurzschlüsse und Regelverstöße sichtbar machen; die Platine gegen die Netzliste des Schaltplans prüfen ist vorhanden.
- **Simulation:** Schaltungen mit SPICE simulieren und Spannungen und Ströme in der Anwendung anzeigen.
- **Werkstatt:** Daten für Laser, CNC-Fräsen und 3D-Drucker vorbereiten, mit maßhaltiger Vorschau und Geräteprofilen.
- **Automatisierung:** ein MCP-Zugang, über den Sprachmodelle Dokumente mit denselben Aktionen wie die Oberfläche lesen, bearbeiten und prüfen können.
- **Offenes Format:** `.openloch` als vollständig dokumentiertes, versioniertes Format für alle Dokumentarten; für das Lochraster ein eigenes Modell statt der Verweise auf LM4-Daten ([Projektformat](openloch-project-format.md)).
