#pragma once
#include <QDialog>
#include <QFont>
#include <QGuiApplication>
#include <QListWidget>
#include <QScreen>
#include <QScrollArea>
#include <QStringList>
#include <QWidget>
#include "language.h"
// Reports that can run long in a large project (docs/suite.md, "Lange Listen in Dialogen"): rows of the same kind under
// one heading with their count, all of them in a list that scrolls, and the dialog no higher than the screen, so that
// its buttons stay in reach. Header only, for the program and every module.
namespace openloch {
// One kind of finding: its heading ("67 Bauteile fehlen auf der Platine – …") and the rows below it.
struct ReportGroup {QString heading;QStringList rows;};
// The groups as one list: each heading bold and not selectable, its rows indented below it. Empty groups are left out.
inline QListWidget *reportList(const QList<ReportGroup> &groups,QWidget *parent=nullptr){
    auto *list=new QListWidget(parent);
    for(const auto &group:groups){
        if(group.rows.isEmpty())continue;
        if(!group.heading.isEmpty()){
            auto *heading=new QListWidgetItem(group.heading,list);QFont bold=list->font();bold.setBold(true);heading->setFont(bold);
            heading->setFlags(Qt::ItemIsEnabled);heading->setData(Qt::UserRole,QStringLiteral("heading"));
        }
        for(const auto &row:group.rows)new QListWidgetItem((group.heading.isEmpty()?QString():QStringLiteral("    "))+row,list);
    }
    return list;
}
// Anything that grows with the project (a form with a row per part or pin) in an area that scrolls.
inline QScrollArea *scrolling(QWidget *content,QWidget *parent=nullptr){
    auto *area=new QScrollArea(parent);area->setWidget(content);area->setWidgetResizable(true);area->setFrameShape(QFrame::NoFrame);
    area->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    // As high as its content wants, up to what the dialog leaves it.
    area->setMinimumHeight(qMin(content->sizeHint().height(),160));area->setSizePolicy(QSizePolicy::Preferred,QSizePolicy::Expanding);
    return area;
}
// At most this many names in the text of a message; the rest is counted there and listed in full under Details.
constexpr int briefListLength=8;
inline QString listInBrief(const QStringList &names){
    if(names.size()<=briefListLength)return names.join(", ");
    return names.mid(0,briefListLength).join(", ")+QString(ui(" … und %1 weitere")).arg(names.size()-briefListLength);
}
// The space a dialog may take: the available area of the screen of its parent (else the primary screen), less room for
// the window frame and title bar.
inline QSize screenRoom(const QWidget *dialog){
    const QWidget *parent=dialog->parentWidget();QScreen *screen=parent&&parent->screen()?parent->screen():QGuiApplication::primaryScreen();
    if(!screen)return {QWIDGETSIZE_MAX,QWIDGETSIZE_MAX};
    const QRect area=screen->availableGeometry();return {qMax(200,area.width()-40),qMax(200,area.height()-60)};
}
// Limits a dialog to the screen: at most screenRoom, its lists and scroll areas take what is left. Call before exec().
inline void fitToScreen(QDialog &dialog){
    const QSize room=screenRoom(&dialog);dialog.setMaximumSize(room);
    dialog.adjustSize();dialog.resize(dialog.size().boundedTo(room));
}
}
