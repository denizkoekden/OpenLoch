// The library folders of all kinds in one overview (Fenster → Bibliotheken…, docs/suite.md "Bibliotheken").
#include "suite.h"
#include "documents/libraryfolders.h"
#include "language.h"
#include "window.h"
#include "paneleditor.h"
#include "strokefont.h"
#include "modules/schematic/editor.h"
#include "modules/schematic/library.h"
#include "modules/pcb/editor.h"
#include "pcbwindow.h"
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMainWindow>
#include <QMessageBox>
#include <QPushButton>
#include <QTreeWidget>
#include <QUrl>
#include <QVBoxLayout>
#include <functional>
namespace openloch::suite {
namespace {
// One library of a kind: how to read and write its folders, and whether further folders and the own one can change.
struct Library {
    QString name;
    std::function<LibraryFolders()> read;
    std::function<void(const LibraryFolders&)> write;
    bool ownChangeable=true,extraChangeable=true;
    QString extraNote;   // what the further folders are, when the overview cannot change them
};
QList<Library> libraries(){
    return {
        {ui("Lochraster – Bauteile"),[]{return componentLibraryFolders();},[](const LibraryFolders &f){setComponentLibraryFolders(f);},true,false,
         ui("LochMaster-Bibliotheken, eingebunden im Lochraster-Fenster")},
        {ui("Frontplatte – Symbole"),[]{return frontpanel::symbolLibraryFolders();},[](const LibraryFolders &f){frontpanel::setSymbolLibraryFolders(f);}},
        {ui("Frontplatte – Strichschriften"),[]{
             const QString own=frontpanel::ownStrokeFontFolder();QStringList extra=frontpanel::strokeFontFolders();extra.removeAll(own);return LibraryFolders{own,extra};},
         [](const LibraryFolders &f){frontpanel::setStrokeFontFolders(QStringList{f.own}+f.extra);},false,true},
        {ui("Schaltplan – Bibliothek"),[]{return schematic::libraryFolders();},[](const LibraryFolders &f){schematic::setLibraryFolders(f);}},
        {ui("Leiterplatte – Makros"),[]{return pcb::libraryFolders();},[](const LibraryFolders &f){pcb::setLibraryFolders(f);}},
    };
}
}
void Suite::showLibraries(QMainWindow *window){
    QDialog dialog(window);dialog.setObjectName("librariesDialog");dialog.setWindowTitle(ui("Bibliotheken"));
    auto *layout=new QVBoxLayout(&dialog);
    auto *about=new QLabel(ui("Jede Dokumentart hat einen eigenen Ordner, in den neue Bauteile und Symbole kommen, und auf Wunsch weitere Ordner, die nur gelesen werden."));
    about->setWordWrap(true);layout->addWidget(about);
    auto *tree=new QTreeWidget(&dialog);tree->setObjectName("libraryFolders");tree->setHeaderLabels({ui("Bibliothek"),ui("Ordner")});tree->setRootIsDecorated(true);
    layout->addWidget(tree,1);
    auto *row=new QHBoxLayout;layout->addLayout(row);
    auto *show=new QPushButton(ui("Im Dateimanager zeigen"),&dialog);show->setObjectName("libraryShow");
    auto *change=new QPushButton(ui("Ordner ändern…"),&dialog);change->setObjectName("libraryChange");
    auto *addFolder=new QPushButton(ui("Ordner hinzufügen…"),&dialog);addFolder->setObjectName("libraryAdd");
    auto *remove=new QPushButton(ui("Entfernen"),&dialog);remove->setObjectName("libraryRemove");
    for(auto *b:{show,change,addFolder,remove})row->addWidget(b);
    row->addStretch();
    auto *buttons=new QDialogButtonBox(QDialogButtonBox::Close,&dialog);layout->addWidget(buttons);
    connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    const QList<Library> all=libraries();
    // Items: a kind at the top (its index in UserRole), its folders below (index and -1 for the own one, else the
    // position among the further folders, in UserRole+1).
    auto fill=[&]{
        tree->clear();
        for(int i=0;i<all.size();i++){
            const LibraryFolders folders=all[i].read();
            auto *top=new QTreeWidgetItem(tree,{all[i].name});top->setData(0,Qt::UserRole,i);top->setData(0,Qt::UserRole+1,-2);
            auto *own=new QTreeWidgetItem(top,{ui("eigener Ordner"),QDir::toNativeSeparators(folders.own)});own->setData(0,Qt::UserRole,i);own->setData(0,Qt::UserRole+1,-1);
            for(int k=0;k<folders.extra.size();k++){
                auto *item=new QTreeWidgetItem(top,{all[i].extraNote.isEmpty()?ui("weiterer Ordner, nur lesen"):all[i].extraNote,QDir::toNativeSeparators(folders.extra[k])});
                item->setData(0,Qt::UserRole,i);item->setData(0,Qt::UserRole+1,k);
            }
            top->setExpanded(true);
        }
        tree->resizeColumnToContents(0);
    };
    auto selected=[&]{auto *item=tree->currentItem();return item?std::pair<int,int>{item->data(0,Qt::UserRole).toInt(),item->data(0,Qt::UserRole+1).toInt()}:std::pair<int,int>{-1,-2};};
    auto update=[&]{
        const auto [i,k]=selected();const bool any=i>=0;
        show->setEnabled(any&&k>=-1);change->setEnabled(any&&k==-1&&all[i].ownChangeable);
        addFolder->setEnabled(any&&all[i].extraChangeable);remove->setEnabled(any&&k>=0&&all[i].extraChangeable);
    };
    // A change reaches every open window of the suite.
    auto changed=[this]{
        for(QMainWindow *w:windows()){
            if(auto *board=dynamic_cast<Window*>(w))board->librariesChanged();
            else if(auto *panel=dynamic_cast<frontpanel::PanelEditor*>(w))panel->librariesChanged();
            else if(auto *plan=dynamic_cast<schematic::Editor*>(w))plan->librariesChanged();
            else if(auto *board=dynamic_cast<PcbWindow*>(w))board->editor()->librariesChanged();
        }
    };
    connect(tree,&QTreeWidget::currentItemChanged,&dialog,[&]{update();});
    connect(show,&QPushButton::clicked,&dialog,[&]{
        const auto [i,k]=selected();if(i<0)return;const LibraryFolders f=all[i].read();const QString folder=k==-1?f.own:f.extra.value(k);
        if(folder.isEmpty())return;
        // The own folder is made when it is missing; a further one that is gone is reported, not made anew.
        if(k==-1)QDir().mkpath(folder);
        else if(!QFileInfo(folder).isDir()){QMessageBox::warning(&dialog,ui("Bibliotheken"),ui("Diesen Ordner gibt es nicht mehr:")+u'\n'+QDir::toNativeSeparators(folder));return;}
        QDesktopServices::openUrl(QUrl::fromLocalFile(folder));
    });
    connect(change,&QPushButton::clicked,&dialog,[&]{
        const auto [i,k]=selected();if(i<0)return;LibraryFolders f=all[i].read();
        const QString folder=QFileDialog::getExistingDirectory(&dialog,ui("Ordner wählen"),f.own);if(folder.isEmpty())return;
        f.own=folder;all[i].write(f);changed();fill();update();
    });
    connect(addFolder,&QPushButton::clicked,&dialog,[&]{
        const auto [i,k]=selected();if(i<0)return;LibraryFolders f=all[i].read();
        const QString folder=QFileDialog::getExistingDirectory(&dialog,ui("Weiteren Ordner wählen"),f.extra.value(0,f.own));if(folder.isEmpty()||f.extra.contains(folder)||folder==f.own)return;
        f.extra<<folder;all[i].write(f);changed();fill();update();
    });
    connect(remove,&QPushButton::clicked,&dialog,[&]{
        const auto [i,k]=selected();if(i<0||k<0)return;LibraryFolders f=all[i].read();if(k>=f.extra.size())return;
        f.extra.removeAt(k);all[i].write(f);changed();fill();update();
    });
    fill();update();dialog.resize(820,420);dialog.exec();
}
}
