#pragma once
#include "example.h"
#include "model.h"
#include "partslist.h"
#include "shapes.h"
#include "settings.h"
#include "render.h"
#include <QKeySequence>
#include <functional>
#include <QDialog>

class QCheckBox;
class QComboBox;
class QLabel;
class QDoubleSpinBox;
class QFontComboBox;
class QPageLayout;
class QLineEdit;
class QListWidget;
class QMenu;
class QPlainTextEdit;
class QPushButton;
class QRadioButton;
class QSlider;
class QSpinBox;
class QTabWidget;
class QTableView;
class QTableWidget;
class QToolButton;
class QTreeWidget;

// The dialogs of the schematic editor, after the reference's.
namespace openloch::schematic {
// "Blatt einfügen" and "Blatt kopieren": where the new sheets go and how many.
class SheetInsertDialog : public QDialog {
public:
    SheetInsertDialog(const QString &title,const QStringList &sheets,int current,QWidget *parent=nullptr);
    // The index the first new sheet gets.
    int position() const;
    int count() const;
    QComboBox *sheetBox=nullptr;
    QRadioButton *before=nullptr,*after=nullptr,*first=nullptr,*last=nullptr;
    QSpinBox *countBox=nullptr;
private:
    int sheetCount=0;
};
// "Blätter sortieren": the sheets in a list that can be reordered by dragging or with the buttons.
class SheetSortDialog : public QDialog {
public:
    SheetSortDialog(const QStringList &sheets,QWidget *parent=nullptr);
    // The old indexes in the new order.
    QList<int> order() const;
    QListWidget *list=nullptr;
};
// "Erweiterte Texteingabe" as in the reference: a text over several lines; buttons with menus for the fixed variables
// (grouped as in sPlan), the user variables and the text constants, each of the last two with "Definieren…"; "Ω" and
// Strg+Einfg for the special characters of the text's font.
class TextDialog : public QDialog {
public:
    TextDialog(const QString &text,const QStringList &variables,QWidget *parent=nullptr,const QStringList &constants={},const QString &family={});
    QString text() const;
    // The menu of the fixed variables, "<NAME>" in each action's data.
    static QMenu *fixedVariables(QWidget *parent);
    // "Definieren…": show the dialog of the user variables or the text constants and give the list afterwards.
    std::function<QStringList()> defineVariables,defineConstants;
    QPlainTextEdit *edit=nullptr;
    QPushButton *fixed=nullptr,*user=nullptr,*constant=nullptr,*defineUser=nullptr,*defineConstant=nullptr,*special=nullptr;
private:
    QStringList userNames,constantTexts;
    void put(const QString &text);
    void refresh();
};
// "Anwender-Variablen": names and values, rows added and deleted with buttons; a name twice (in any case) is refused.
class VariablesDialog : public QDialog {
public:
    VariablesDialog(const QList<Variable> &variables,QWidget *parent=nullptr);
    QList<Variable> variables() const;
    void accept() override;
    QTableWidget *table=nullptr;
};
// "Sonderzeichen": the characters of a font (as in the reference those up to U+FFFF) twenty in a row; "Indizes",
// "Griechisch" and "Mathematisch" jump to U+2080 (else U+2070), U+0391 and U+2202; the last twenty characters taken in
// this session below, a click shows one in the table. A double click takes a character.
class CharacterDialog : public QDialog {
public:
    explicit CharacterDialog(const QString &family,QWidget *parent=nullptr);
    QString chosen() const{return taken;}
    // Selects the character in the table; false when the font does not have it.
    bool jumpTo(char32_t code);
    // Takes the character and closes; refused with a message when the font does not have it.
    bool take(char32_t code);
    // The characters taken last in this session, newest first.
    static QList<char32_t> &recent();
    // Asks for a character of `family` ("Arial" when empty); empty when cancelled.
    static QString ask(const QString &family,QWidget *parent);
    // Called with the dialog before it is shown (for tests).
    static inline std::function<void(CharacterDialog*)> shown;
    QTableView *grid=nullptr;
    QTableWidget *last=nullptr;
private:
    QList<char32_t> codes;
    QString taken;
    void showRecent();
};
// Strg+Einfg in `field` (a QLineEdit or QPlainTextEdit) asks for a special character of the font `family` gives and puts
// it in at the cursor.
void offerSpecialCharacters(QWidget *field,std::function<QString()> family);
// "Bauteileigenschaften": designator and value with their switches, the parts list, the caption in the library and
// the contacts. For a whole library page ("Eigenschaften (Alle)") only what was changed is taken over.
class ComponentDialog : public QDialog {
public:
    ComponentDialog(const Item &component,const QString &caption,QWidget *parent=nullptr);
    // The component and caption with the dialog's values; with `changedOnly`, only the fields changed in the dialog.
    void apply(Item &component,QString &caption,bool changedOnly=false) const;
    QLineEdit *designator=nullptr,*value=nullptr,*extra=nullptr,*caption=nullptr;
    QCheckBox *designatorVisible=nullptr,*autoNumber=nullptr,*valueVisible=nullptr,*askValue=nullptr,*partsList=nullptr;
private:
    Item before;
    QString captionBefore;
};
// "Blätter auswählen": the sheets a function works on, all chosen at first.
class SheetChoiceDialog : public QDialog {
public:
    SheetChoiceDialog(const QString &title,const QStringList &sheets,QWidget *parent=nullptr);
    QList<int> sheets() const;
    QListWidget *list=nullptr;
};
// "Bauteilnummerierung": the order in which components are numbered and which of them.
class NumberingDialog : public QDialog {
public:
    NumberingDialog(bool selection,QWidget *parent=nullptr);
    QRadioButton *none=nullptr,*columns=nullptr,*rows=nullptr;
    QDoubleSpinBox *raster=nullptr;
    QCheckBox *selectedOnly=nullptr,*lettersOnly=nullptr;
    QLineEdit *letters=nullptr;
    QSpinBox *start=nullptr;
};
// "Stückliste": the parts list of the chosen sheets in an editable table, made anew when its options change; printed,
// copied, exported as text or put onto the sheet. As sPlan's window: the buttons above the list, at the right the font
// (of the list, the printout and RTF files), the options, the sorting and printing with margins and orientation.
class PartsListDialog : public QDialog {
public:
    PartsListDialog(const Document &document,const QList<int> &sheets,const QString &fileName,QWidget *parent=nullptr);
    // The table as shown, with the changes made in it; the lines with the file's data above it (if chosen).
    PartsTable table() const;
    QStringList fileLines() const;
    QList<int> groupStarts() const;
    std::function<void(const PartsTable&,const PartsDrawing&)> insert;
    // Lets tests reach the options dialog of "Stückliste auf Blatt einfügen" before it is shown.
    std::function<void(QDialog*)> placementOptions;
    QTableWidget *grid=nullptr;
    QCheckBox *merge=nullptr,*fileData=nullptr,*verticalLines=nullptr,*horizontalLines=nullptr;
    QList<QCheckBox*> extras;
    QRadioButton *alphabetical=nullptr,*frequency=nullptr;
    QFontComboBox *font=nullptr;
    QSpinBox *fontSize=nullptr,*topMargin=nullptr,*leftMargin=nullptr;     // points; millimetres
    QRadioButton *portrait=nullptr,*landscape=nullptr;
    // "?": the help on lists (set by the editor).
    std::function<void()> help;
    // Lets tests reach the export dialog with its preview before it is shown.
    std::function<void(QDialog*)> exportOptions;
    // The page of the printout from a printer's: the orientation and the top and left margins chosen.
    QPageLayout printLayout(QPageLayout layout) const;
    // "Exportieren": separator, field names and empty lines between the groups, with a preview of the lines.
    void exportText();
    void rebuild();
    void copyToClipboard() const;
    // "Auf Blatt einfügen": asks for the options and hands the table to `insert`.
    void insertOnSheet();
    // "Speichern" and "Öffnen": the list as a Rich Text Format file.
    bool saveRtf(const QString &file,QString *error=nullptr) const;
    bool openRtf(const QString &file,QString *error=nullptr);
private:
    const Document &document;
    QList<int> sheets;
    QString fileName;
    QList<PartsRow> rows;
    void print();
    void applyFont();
};
// "Verknüpfe mit PARENT": the parents of the document with designator, value and sheet.
class ParentChoiceDialog : public QDialog {
public:
    ParentChoiceDialog(const Document &document,const QString &fileName,QWidget *parent=nullptr);
    QString parentId() const;       // empty if none is chosen
    QTreeWidget *list=nullptr;
};
// "Parent-Child-Liste": every parent with its children, over all sheets or the current one; a double click goes to the
// component (`chosen`).
class ParentChildListDialog : public QDialog {
public:
    ParentChildListDialog(const Document &document,const QString &fileName,QWidget *parent=nullptr);
    void fill();
    QString text() const;           // the list as text, for the clipboard
    QString chosen;
    QTreeWidget *tree=nullptr;
    QCheckBox *currentOnly=nullptr,*showContacts=nullptr,*expanded=nullptr;
private:
    const Document &document;
    QString fileName;
};
// "Kontaktliste": every contact of the components with sheet, component, contact text and name; a double click goes to
// the component (`chosen`).
class ContactListDialog : public QDialog {
public:
    ContactListDialog(const Document &document,const QString &fileName,QWidget *parent=nullptr);
    QString text() const;
    QString chosen;
    QTreeWidget *list=nullptr;
};
// "Inhaltsverzeichnis einfügen": text height, frame and lines of the table of the chosen sheets.
class ContentsDialog : public QDialog {
public:
    explicit ContentsDialog(QWidget *parent=nullptr);
    PartsDrawing drawing() const;
    QDoubleSpinBox *height=nullptr;
    QCheckBox *frame=nullptr,*verticalLines=nullptr,*horizontalLines=nullptr;
};
// "Ziel auswählen": the texts released as link targets, with their sheet.
class TargetChoiceDialog : public QDialog {
public:
    TargetChoiceDialog(const Document &document,const QString &except,QWidget *parent=nullptr);
    QString targetId() const;
    QTreeWidget *list=nullptr;
};
// "Linkliste": every linked text with its sheet and target; a double click goes to the text (`chosen`).
class LinkListDialog : public QDialog {
public:
    LinkListDialog(const Document &document,QWidget *parent=nullptr);
    QString text() const;
    QString chosen;
    QTreeWidget *list=nullptr;
};
// "Textkonstanten": texts kept for quick insertion, one per row.
class TextConstantsDialog : public QDialog {
public:
    TextConstantsDialog(const QStringList &constants,QWidget *parent=nullptr);
    QStringList constants() const;
    QTableWidget *table=nullptr;
};
// "Grundeinstellungen", as the reference a list of pages: "Grundeinstellungen" (sheet numbers in the tabs, component
// texts only with Alt), "Verzeichnisse" (the fixed folders of templates and title blocks), "Bibliothek" (its root
// folder: changed, reset, shown in the file manager), "Raster" (contrast, marks, lines or dots, over the title block),
// "Autospeichern", "Neues Blatt" (size, orientation, grid, title block) and "Hotkeys" (the keys of the drawing modes,
// given as action name, label and standard key).
struct HotkeyMode {QString action,label;QKeySequence standard;};
// What the page "Grundeinstellungen" shows of the drawing itself (kept in its file, as in sPlan): sheet tabs with
// numbers, designators with sheet number and prefix.
struct DrawingSettings {
    bool sheetNumbers=true,designatorPageNumbers=false;
    QString designatorPrefix;
    bool operator==(const DrawingSettings &) const=default;
};
class SettingsDialog : public QDialog {
public:
    SettingsDialog(const GeneralSettings &settings,QWidget *parent=nullptr,const QList<HotkeyMode> &modes={},const DrawingSettings &drawing={});
    GeneralSettings settings() const;
    DrawingSettings drawing() const;
    QListWidget *pages=nullptr;
    QLineEdit *folder=nullptr,*templates=nullptr,*forms=nullptr,*drawingFolder=nullptr,*formWorkFolder=nullptr,*exportFolder=nullptr;
    QComboBox *gridContrast=nullptr;
    QSpinBox *autosaveMinutes=nullptr;
    QCheckBox *backupOnOpen=nullptr;
    QDoubleSpinBox *newSheetWidth=nullptr,*newSheetHeight=nullptr,*newSheetGrid=nullptr;
    QRadioButton *landscape=nullptr,*portrait=nullptr;
    QLineEdit *newSheetForm=nullptr;
    QTableWidget *hotkeys=nullptr;
    QSpinBox *gridMarks=nullptr;
    QRadioButton *gridDots=nullptr,*gridLines=nullptr;
    QCheckBox *gridOverTitleBlock=nullptr,*sheetNumbers=nullptr,*designatorPageNumbers=nullptr,*componentTextsWithKey=nullptr;
    QCheckBox *whiteBackground=nullptr;   // "Anzeige"
    QLineEdit *designatorPrefix=nullptr;
    QComboBox *componentTextKey=nullptr;
private:
    QString libraryFolder() const;
    QList<HotkeyMode> modes;
};
// "Bauteilbeschriftung": the lettering of designators, values and contacts, and what it is applied to.
class LetteringDialog : public QDialog {
public:
    enum Scope {Project,Sheet,Selection,LibraryPage};
    LetteringDialog(bool selection,bool libraryPage,QWidget *parent=nullptr);
    Lettering lettering(int part) const;   // 0 designator, 1 value, 2 contacts
    Scope scope() const;
    QList<QCheckBox*> use;
    QList<QFontComboBox*> family;
    QList<QDoubleSpinBox*> height;
    QList<QCheckBox*> bold,italic;
    QList<QToolButton*> colour;
    QLabel *preview=nullptr;    // a component with "R1", "4k7" and the contacts "1" and "2" in the chosen lettering
    QRadioButton *project=nullptr,*sheet=nullptr,*selection=nullptr,*libraryPage=nullptr;
    QList<QColor> colours;      // chosen with the colour buttons
    void showPreview();
    // The values a group starts with (part 0 designator, 1 value, 2 contacts).
    void start(int part,const Font &font);
};
// "Childliste (Kontaktspiegel)": font, frame and lines, the columns (checked, their order by dragging), the format of
// rows and columns of the grid, a preview; "Einfügen..." puts the table on the sheet.
class ChildListDialog : public QDialog {
public:
    ChildListDialog(const Document &document,const QString &parentId,const QString &fileName,QWidget *parent=nullptr);
    ChildListOptions options() const;
    PartsDrawing drawing() const;
    PartsTable table() const;
    QFontComboBox *family=nullptr;
    QSpinBox *size=nullptr;                 // in tenths of a millimetre
    QCheckBox *frame=nullptr,*shadow=nullptr,*verticalLines=nullptr,*alternate=nullptr,*slash=nullptr;
    QList<QCheckBox*> columns;              // by ChildColumn
    QListWidget *order=nullptr;
    QComboBox *rowFormat=nullptr,*columnFormat=nullptr;
    QPlainTextEdit *preview=nullptr;
private:
    const Document &document;
    QString parentId,fileName;
    void updatePreview();
};
// "Bitmap-Explorer": the pictures of the document by sheet, memory or resolution; pictures above a maximum resolution
// are reduced to it, the chosen one or all. Changes go through `change` as undo steps.
class BitmapExplorerDialog : public QDialog {
public:
    explicit BitmapExplorerDialog(QWidget *parent=nullptr);
    std::function<const Document*()> document;
    std::function<void(const std::function<void(Document&)>&)> change;
    std::function<void(int,const QString&)> showImage;    // sheet, element id
    void refresh();
    // Reduces the chosen picture or all above the maximum; returns how many changed.
    int applyMaximum(bool all);
    QTreeWidget *list=nullptr;
    QRadioButton *bySheet=nullptr,*byMemory=nullptr,*byDpi=nullptr;
    QComboBox *maximum=nullptr;
    QLabel *count=nullptr,*memory=nullptr;
};
// "Spezialformen": the settings of polygon, star, grid and wave on one page each; `page` is the one shown first.
class SpecialShapeDialog : public QDialog {
public:
    SpecialShapeDialog(const SpecialShapeOptions &options,int page,QWidget *parent=nullptr);
    SpecialShapeOptions options() const;
    QTabWidget *pages=nullptr;
    QSpinBox *corners=nullptr,*polygonOffset=nullptr,*spikes=nullptr,*spikeDepth=nullptr,*starOffset=nullptr,*columns=nullptr,*rows=nullptr,*textHeight=nullptr,*waves=nullptr;
    QRadioButton *polygonAsLine=nullptr,*polygonAsPolygon=nullptr,*starAsLine=nullptr,*starAsPolygon=nullptr;
    QCheckBox *frame=nullptr,*textFields=nullptr;
    QComboBox *wave=nullptr;
};
// "Formblatt generieren" as in sPlan: the labels of each side of the frame ("---", "NUM", "CHAR") and their font and
// height; frame, columns, rows, start numbers and grid are the sheet's (properties, section "Formblatt"), a sheet without
// a frame gets one 10 mm inside it. OpenLoch adds the field with the sheet's name and number.
class TitleBlockDialog : public QDialog {
public:
    TitleBlockDialog(const TitleBlock &current,double width,double height,QWidget *parent=nullptr);
    TitleBlock titleBlock() const;
    TitleBlockStyle style() const;
    QComboBox *top=nullptr,*bottom=nullptr,*left=nullptr,*right=nullptr;
    QFontComboBox *font=nullptr;
    QSpinBox *size=nullptr;     // 1/10 mm
    QCheckBox *field=nullptr;
private:
    TitleBlock current;
    double sheetWidth,sheetHeight;
};
// "Exportieren": file type by suffix, resolution, colour or black and white, the current sheet or all.
class ExportDialog : public QDialog {
public:
    // The sizes in millimetres of the current sheet, of the frame of all its elements and of the selection's (empty
    // when nothing is selected), and the number of sheets ("Alle Blätter" only with more than one).
    ExportDialog(QSizeF sheet,QSizeF elements,QSizeF selection,int sheets,QWidget *parent=nullptr);
    enum Format {Jpg,Png,Bmp,Emf,Svg,Pdf};
    Format format() const;
    QString suffix() const;     // "jpg", "png", …
    ExportArea area() const;
    bool raster() const{return format()<=Bmp;}
    QRadioButton *formats[6]{};
    QSlider *resolution=nullptr;
    QSpinBox *dpi=nullptr;
    QLabel *original=nullptr,*pixels=nullptr;
    QRadioButton *colour=nullptr,*blackAndWhite=nullptr;
    QCheckBox *transparent=nullptr;
    QRadioButton *currentSheet=nullptr,*allElements=nullptr,*selectedElements=nullptr,*allSheets=nullptr;
private:
    QSizeF sizes[3];
    void update();
};
// The reference's problem dialog: elements wholly outside the sheet, deleted or moved onto its edge, and pictures of
// more than 300 dpi, for the Bitmap-Explorer.
class ProblemDialog : public QDialog {
public:
    enum Choice {None,Delete,Move,Pictures};
    ProblemDialog(int outside,int pictures,QWidget *parent=nullptr);
    Choice choice=None;
    QPushButton *deleteButton=nullptr,*moveButton=nullptr,*picturesButton=nullptr;
};
// "Kopieren in die Zwischenablage": the resolution (75, 150 or 300 dpi) and all elements or only the selected ones
// (chosen when there is a selection); the resolution is kept for the session.
class ClipboardDialog : public QDialog {
public:
    ClipboardDialog(bool selection,QWidget *parent=nullptr);
    int resolution() const;
    bool selectionOnly() const;
    void accept() override;
    QRadioButton *dpi75=nullptr,*dpi150=nullptr,*dpi300=nullptr,*all=nullptr,*selected=nullptr;
};
}
