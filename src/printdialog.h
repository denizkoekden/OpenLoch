#pragma once
#include "printing.h"
#include "project.h"
#include <QDialog>
class QPrinter;class QSpinBox;class QComboBox;class QCheckBox;class QRadioButton;class QScrollBar;class QLabel;class QLineEdit;class QTabBar;class QToolButton;class QPainter;
namespace openloch {
// LochMaster's "Druckvorschau": up to ten views of each board placed on the sheets, each with its own switches, layers,
// scale and position; paper, copies, Kacheln and Korrekturfaktoren; a preview of all sheets in which views are dragged.
// The settings are kept per board in the project and saved with it, without marking it changed (like the original).
class PrintDialog : public QDialog {
public:
    // `projectName` and `unit` (the main window's unit) are used by the data field.
    PrintDialog(Project &project,QPrinter &printer,const QString &projectName,int unit,QWidget *parent=nullptr);
    // The print run: all sheets of the selected board or of all boards, copies collated. Returns the pages printed.
    int print(QPrinter &target);
    QSize sheets() const;
    int sheetsToPrint() const;
    // Korrekturfaktoren for this session (1.0 at start, 0.8 to 1.2), applied only when printing.
    static double correctionX,correctionY;
protected:
    void done(int result) override;
private:
    class Preview;friend class Preview;
    Project &project;QPrinter &printer;QString projectName;int mainUnit;
    QList<Project> pages;QList<PrintSetup> setups;int board=0,active=0;bool syncing=false,cancelled=false;
    QSpinBox *count=nullptr,*sheetNumber=nullptr,*copies=nullptr;QComboBox *activeView=nullptr,*range=nullptr;
    QList<QToolButton*> switches;QToolButton *unitButton=nullptr;
    QCheckBox *objects=nullptr,*texts=nullptr,*copper=nullptr,*solderMarks=nullptr,*cuts=nullptr,*holes=nullptr,*background=nullptr,*rulers=nullptr;
    QRadioButton *original=nullptr,*zoom=nullptr,*portrait=nullptr,*landscape=nullptr;QScrollBar *scale=nullptr;QLabel *scaleLabel=nullptr,*sheetLabel=nullptr;
    QCheckBox *centred=nullptr,*onlyOne=nullptr,*cutMarks=nullptr,*dataField=nullptr;QLineEdit *left=nullptr,*top=nullptr;
    QTabBar *tabs=nullptr;Preview *preview=nullptr;QList<QLabel*> status;
    PrintSetup &setup(){return setups[board];}
    PrintView &view(){return setups[board].views[active];}
    Paper paper() const;
    QSizeF boardSize(int index) const{return {pages[index].width,pages[index].height};}
    void syncUi();
    void refresh();
    void setCount(int n);
    // The views of a board on one sheet (col, row): device pixels per mm, origin at the printable corner.
    void paintSheet(QPainter &p,int index,const Paper &paper,QPoint sheet,double pxPerMm,bool printing,QPointF correction) const;
    void paintDataField(QPainter &p,int index,const Paper &paper,double pxPerMm,int sheet,int sheets) const;
};
}
