// The project overview (Fenster → Projektübersicht…): the documents of a project and the parts across them.
#include "suite.h"
#include "documents.h"
#include "language.h"
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QMainWindow>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QSaveFile>
#include <QTabWidget>
#include <QToolButton>
#include <QTreeWidget>
#include <QVBoxLayout>
namespace openloch::suite {
void Suite::showProjectOverview(QMainWindow *window){
    OpenProject *project=projectOf(window);if(!project)return;
    auto projectName=[&]{const QString file=QFileInfo(project->path).completeBaseName();return !file.isEmpty()?file:!project->file.title.isEmpty()?project->file.title:ui("Neues Projekt");};
    auto kindName=[](const QString &id){for(const auto &info:documentKinds())if(info.id==id)return info.name;return id;};
    auto documentTitle=[&](const documents::ProjectDocument &d){QMainWindow *open=project->windows.value(d.id).data();return open?documentName(open):d.name.isEmpty()?kindName(d.kind):d.name;};
    QDialog dialog(window);dialog.setObjectName("projectOverview");
    auto *layout=new QVBoxLayout(&dialog);auto *tabs=new QTabWidget(&dialog);layout->addWidget(tabs);
    // The documents with what to do with them.
    auto *page=new QWidget;auto *pageLayout=new QVBoxLayout(page);
    auto *documents=new QTreeWidget(page);documents->setObjectName("projectDocuments");documents->setRootIsDecorated(false);
    documents->setHeaderLabels({ui("Dokument"),ui("Art"),ui("Herkunft"),ui("Zustand")});pageLayout->addWidget(documents);
    auto *row=new QHBoxLayout;pageLayout->addLayout(row);
    auto *openButton=new QPushButton(ui("Öffnen"),page);openButton->setObjectName("overviewOpen");
    auto *renameButton=new QPushButton(ui("Umbenennen…"),page);renameButton->setObjectName("overviewRename");
    auto *removeButton=new QPushButton(ui("Entfernen…"),page);removeButton->setObjectName("overviewRemove");
    auto *addButton=new QToolButton(page);addButton->setObjectName("overviewAdd");addButton->setText(ui("Hinzufügen"));addButton->setPopupMode(QToolButton::InstantPopup);
    auto *addMenu=new QMenu(addButton);addButton->setMenu(addMenu);
    for(auto *w:std::initializer_list<QWidget*>{openButton,renameButton,removeButton,addButton})row->addWidget(w);
    row->addStretch();tabs->addTab(page,ui("Dokumente"));
    // The parts across the documents and the parts list.
    auto *partsPage=new QWidget;auto *partsLayout=new QVBoxLayout(partsPage);
    auto *summary=new QLabel(partsPage);summary->setObjectName("partsSummary");summary->setWordWrap(true);partsLayout->addWidget(summary);
    auto *parts=new QTreeWidget(partsPage);parts->setObjectName("projectParts");parts->setRootIsDecorated(false);partsLayout->addWidget(parts);
    auto *exportButton=new QPushButton(ui("Stückliste exportieren…"),partsPage);exportButton->setObjectName("overviewExport");partsLayout->addWidget(exportButton,0,Qt::AlignLeft);
    tabs->addTab(partsPage,ui("Bauteile"));
    auto *buttons=new QDialogButtonBox(QDialogButtonBox::Close,&dialog);layout->addWidget(buttons);
    connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);

    auto current=[&]{return documents->currentItem()?documents->currentItem()->data(0,Qt::UserRole).toString():QString();};
    auto updateButtons=[&]{
        const QString id=current();const bool any=!id.isEmpty();
        openButton->setEnabled(any);renameButton->setEnabled(any);
        removeButton->setEnabled(any&&!project->transparent&&project->file.documents.size()>1&&project->windows.value(id).data()!=window);
    };
    auto refresh=[&]{
        dialog.setWindowTitle(ui("Projekt „%1“").arg(projectName()));
        const QString selected=current();documents->clear();
        for(const auto &d:project->file.documents){
            const QString origin=d.origin.value("file").toString();
            auto *item=new QTreeWidgetItem(documents,{documentTitle(d),kindName(d.kind),origin.isEmpty()?QStringLiteral("–"):origin,project->windows.value(d.id)?ui("geöffnet"):QString()});
            item->setData(0,Qt::UserRole,d.id);if(d.id==selected)documents->setCurrentItem(item);
        }
        if(!documents->currentItem()&&documents->topLevelItemCount())documents->setCurrentItem(documents->topLevelItem(0));
        for(int c=0;c<documents->columnCount();c++)documents->resizeColumnToContents(c);
        const auto list=projectPartsOf(window);
        QStringList headers{ui("Bezeichner"),ui("Wert")};for(const auto &d:project->file.documents)headers<<documentTitle(d);
        parts->clear();parts->setColumnCount(int(headers.size()));parts->setHeaderLabels(headers);
        for(const auto &p:list){
            QStringList cells{p.designator.isEmpty()?ui("ohne Kennung"):p.designator,p.value};
            for(const auto &d:project->file.documents)cells<<(p.documents.contains(d.id)?QStringLiteral("✓"):QString());
            new QTreeWidgetItem(parts,cells);
        }
        for(int c=0;c<parts->columnCount();c++)parts->resizeColumnToContents(c);
        const auto rows=partsList(list,project->file);int count=0;for(const auto &r:rows)count+=r.count;
        summary->setText(list.isEmpty()?ui("Die Dokumente des Projekts enthalten keine Bauteile."):ui("%1 Bauteil(e) in den Dokumenten, davon %2 in der Stückliste (ohne Bohrungen der Frontplatten).").arg(list.size()).arg(count));
        exportButton->setEnabled(!rows.isEmpty());updateButtons();
    };
    connect(documents,&QTreeWidget::currentItemChanged,&dialog,[&]{updateButtons();});
    connect(openButton,&QPushButton::clicked,&dialog,[&]{const QString id=current();dialog.accept();openDocumentWindow(project,id);});
    connect(documents,&QTreeWidget::itemDoubleClicked,&dialog,[&]{openButton->click();});
    connect(renameButton,&QPushButton::clicked,&dialog,[&]{
        const QString id=current();bool ok=false;
        const QString name=QInputDialog::getText(&dialog,ui("Dokument umbenennen"),ui("Name:"),QLineEdit::Normal,documents->currentItem()->text(0),&ok);
        if(ok&&!name.trimmed().isEmpty())renameDocument(window,id,name);
        refresh();
    });
    connect(removeButton,&QPushButton::clicked,&dialog,[&]{
        const QString id=current();
        if(QMessageBox::question(&dialog,ui("Dokument entfernen"),ui("„%1“ aus dem Projekt entfernen? Sein Fenster schließt sich, ungesicherte Änderungen daran gehen verloren, und das Projekt wird gespeichert.").arg(documents->currentItem()->text(0)))!=QMessageBox::Yes)return;
        removeDocument(window,id);refresh();
    });
    for(const auto &info:documentKinds()){
        auto *a=addMenu->addAction(info.name+QStringLiteral("…"));a->setObjectName("overviewAdd-"+info.id);a->setEnabled(info.available&&projectCapable(info.kind));
        connect(a,&QAction::triggered,&dialog,[&,kind=info.kind]{addToProject(window,kind);refresh();});
    }
    connect(exportButton,&QPushButton::clicked,&dialog,[&]{
        const QString folder=project->path.isEmpty()?QDir::homePath():QFileInfo(project->path).absolutePath();
        const QString file=QFileDialog::getSaveFileName(&dialog,ui("Stückliste exportieren"),QDir(folder).filePath(projectName()+ui(" Stückliste")+".csv"),ui("CSV-Datei (*.csv)"));
        if(file.isEmpty())return;
        QSaveFile out(file);
        if(!out.open(QIODevice::WriteOnly)||out.write(partsListCsv(partsList(projectPartsOf(window),project->file)))<0||!out.commit())
            QMessageBox::warning(&dialog,ui("Stückliste exportieren"),ui("%1 kann nicht gespeichert werden:\n%2").arg(QFileInfo(file).fileName(),out.errorString()));
    });
    refresh();dialog.resize(760,480);dialog.exec();
}
}
