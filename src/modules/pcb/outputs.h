#pragma once
#include "fabrication.h"
#include "milling.h"
#include "gerberimport.h"
#include <QDialog>
#include <QMap>

class QCheckBox;
class QComboBox;
class QSpinBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPlainTextEdit;
class QPushButton;
class QRadioButton;

// The dialogs of the fabrication outputs (Gerber files, drill data, component data, milling files) and of the Gerber
// import.
namespace openloch::pcb {
// Gerber export: a file per chosen output, named by a common file name and the output's ending, written into one
// folder; the log below lists what was written.
class GerberDialog : public QDialog {
public:
    GerberDialog(const Board &board,const QString &name,const QString &folder,const GerberSettings &settings,QWidget *parent=nullptr);
    GerberSettings settings() const{return current;}
    QList<int> chosen() const;
    void choose(int output,bool on);
    QString fileName(int output) const;
    QString folder() const{return directory;}
    void setFolder(const QString &folder);
    // Writes the chosen outputs; returns how many files were written. Existing files are replaced.
    int writeFiles();
    QStringList log() const;
private:
    const Board &board;
    GerberSettings current;
    QString directory;
    QLineEdit *name=nullptr;
    QMap<int,QCheckBox*> outputs;
    QMap<int,QLineEdit*> suffixes;
    QMap<int,QLabel*> names;
    QLabel *folderLabel=nullptr;
    QListWidget *logList=nullptr;
    QWidget *maskBox=nullptr,*pasteBox=nullptr;
    QCheckBox *punch=nullptr;
    void changed();
};

// Excellon export of the drill data.
class DrillDialog : public QDialog {
public:
    DrillDialog(const Board &board,const QString &name,const DrillSettings &settings,QWidget *parent=nullptr);
    DrillSettings settings() const{return current;}
    QString suggestedFile() const{return file;}
private:
    DrillSettings current;
    QString file;
    QLabel *example=nullptr;
    void changed();
};

// Isolation milling: the settings, the job list they give (its order can be changed by dragging) and the options
// of the files.
class MillingDialog : public QDialog {
public:
    MillingDialog(const Board &board,const QList<int> &selection,const MillingSettings &settings,QWidget *parent=nullptr);
    MillingSettings settings() const{return current;}
    // The jobs with their paths, in the order of the list.
    QList<MillingJob> jobs(const std::function<void(int,int)> &progress={}) const;
    QStringList jobNames() const;
private:
    const Board &board;
    QList<int> selection;
    MillingSettings current;
    QListWidget *jobList=nullptr;
    void changed();
};

class GerberPreview;
// Gerber import: a Gerber file per layer and a drill file, read as soon as they are chosen and shown in the preview; the
// drill format comes from the file and can be changed.
class GerberImportDialog : public QDialog {
public:
    explicit GerberImportDialog(QWidget *parent=nullptr);
    ~GerberImportDialog() override;
    // Reads a file for a layer (empty: none); false and the reason in the dialog for a file it cannot read.
    bool setFile(int layer,const QString &path);
    bool setDrillFile(const QString &path);         // empty: none
    // A large picture of the files, as a click into the preview shows it.
    void showLarge();
    void setDrillFormat(const DrillFormat &format);
    GerberImport import() const;
    bool newBoard() const;
    // The name for a new board: the file name without its ending.
    QString boardName() const;
    QString state(int layer) const;     // what the dialog says about the file of a layer
private:
    QMap<int,QString> files;
    QMap<int,GerberData> layers;
    QMap<int,QLineEdit*> names;
    QMap<int,QLabel*> states;
    QByteArray drillBytes;
    QString drillPath;
    QList<DrillHit> hits;
    DrillFormat format;
    QLineEdit *drillName=nullptr;
    QLabel *drillState=nullptr;
    QComboBox *unit=nullptr,*zeros=nullptr;
    QSpinBox *integers=nullptr,*decimals=nullptr;
    QCheckBox *vias=nullptr,*join=nullptr;
    QRadioButton *fresh=nullptr;
    GerberPreview *preview=nullptr;
    QPushButton *importButton=nullptr;
    bool refreshing=false;
    void readDrills();
    void changed();
};

// Export of the component data: the fields in the order of the list (checked ones only), with a preview of the file.
class ComponentDataDialog : public QDialog {
public:
    ComponentDataDialog(const Board &board,const QString &name,const ComponentDataSettings &settings,QWidget *parent=nullptr);
    ComponentDataSettings settings() const{return current;}
    QString preview() const;
    QString suggestedFile() const{return file;}
private:
    const Board &board;
    ComponentDataSettings current;
    QString file;
    QListWidget *fields=nullptr;
    QPlainTextEdit *text=nullptr;
    void changed();
};
}
