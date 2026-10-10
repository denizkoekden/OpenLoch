#pragma once
#include <QList>
#include <QMap>
#include <QString>
#include <QtCore/qnamespace.h>

// The "Grundeinstellungen" of the module as kept in the user's settings (schematic/…): component texts dragged only
// with a modifier key, the library folder, the fixed folders of templates and title blocks, and how the grid shows.
// Standard folders are kept as absent entries. Sheet numbers and the designator prefix belong to the drawing (Document).
namespace openloch::schematic {
// The modifier keys offered for dragging component texts: Alt (as in sPlan), Shift+Alt, Ctrl+Alt and Meta, since a
// window manager may take Alt with a drag for itself.
QList<Qt::KeyboardModifiers> componentTextKeys();
struct GeneralSettings {
    // "Bauteiltexte nur mit gedrückter Zusatztaste verschiebbar", with the key (one of componentTextKeys()).
    bool componentTextsWithKey=false;
    Qt::KeyboardModifiers componentTextKey=Qt::AltModifier;
    QString libraryFolder,templateFolder,formFolder;
    // "Arbeitsverzeichnisse": where the file dialogs of drawings, title blocks and exports start; empty: the last used.
    QString drawingFolder,formWorkFolder,exportFolder;
    int gridContrast=100;           // percent
    int gridMarks=5;                // a cross every that many grid points; below 2 none
    bool gridLines=false,gridOverTitleBlock=false;
    bool whiteBackground=false;     // "Anzeige": the sheet pure white on screen
    // "Autospeichern": the document also into "<name>.bak" every that many minutes (0: never), and a copy
    // "Backup_of_<name>" of each file as it is opened.
    int autosaveMinutes=0;
    bool backupOnOpen=false;
    // "Neues Blatt": size, grid and title block (a sPlan title block file; empty: none) of new documents and sheets.
    double newSheetWidth=297,newSheetHeight=210,newSheetGrid=1.27;
    QString newSheetForm;
    // "Hotkeys": the keys of the drawing modes by action name where they are not the standard ones (empty: none).
    QMap<QString,QString> hotkeys;
    static GeneralSettings load();
    void store() const;
    bool operator==(const GeneralSettings &) const=default;
};
// The folder of title blocks ("schematic/formFolder"; Documents/OpenLoch/Schaltplan-Formblätter).
QString formFolder();
QString standardFormFolder();
// Sheet formats as sPlan offers them for a new sheet (A0–A5, B4, B5, C3–C5), then Letter and Legal; in landscape, in
// millimetres. Any other size is "Frei", which is no entry here.
struct PaperFormat {const char *name;double width,height;};
const QList<PaperFormat> &paperFormats();
// The format of a size in either orientation, -1 for "Frei".
int paperFormatOf(double width,double height);
}
