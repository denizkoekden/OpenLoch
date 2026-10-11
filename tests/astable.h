#pragma once
#include "documents/projectfile.h"
#include "documents/targets.h"
#include "openlibrary.h"
#include "project.h"
#include <QFile>
#include <QHash>
#include <QJsonDocument>
#include <QRegularExpression>
#include <stdexcept>
#include <utility>

// The astable multivibrator of "Bauteile anordnen" (docs/suite.md): its schematic as target connections and its parts
// from the open libraries, as arranged by hand: two mirrored halves between the supply on top and ground below, the
// battery clip at the left edge, hanging over it. No wires yet.
namespace fixtures {
inline std::pair<openloch::Project,openloch::documents::Targets> astableByHand(){
    using namespace openloch::documents;
    auto page=[](const QString &name){
        QFile f(QString(OPENLOCH_SOURCE_DIR)+"/libraries/"+name);if(!f.open(QIODevice::ReadOnly))throw std::runtime_error("a library page is missing");
        return openloch::openLibraryPage(QJsonDocument::fromJson(f.readAll()).object());
    };
    openloch::Project board;board.width=10000;board.height=8000;
    const auto leds=board.addLibrary(page("06-leuchtdioden.json"),"LIB6.LIB"),resistors=board.addLibrary(page("09-widerstaende.json"),"LIB9.LIB");
    const auto caps=board.addLibrary(page("46-kondensatoren-elko-rm.json"),"LIB46.LIB"),transistors=board.addLibrary(page("08-transistoren.json"),"LIB8.LIB");
    const auto batteries=board.addLibrary(page("18-batterien.json"),"LIB18.LIB");
    Targets astable;QHash<QString,QString> id;
    auto component=[&](const QString &designator,const QString &value,const QStringList &pins,const QString &kind){id[designator]=newId();astable.components<<TargetComponent{id[designator],designator,value,pins,kind};};
    for(const QString d:{"LED1","LED2"})component(d,"",{"1","2"},"D");
    for(const QString d:{"R1","R4"})component(d,"470",{"1","2"},"R");
    for(const QString d:{"R2","R3"})component(d,"47k",{"1","2"},"R");
    for(const QString d:{"C1","C2"})component(d,"10uF",{"1","2"},"C");
    for(const QString d:{"T1","T2"})component(d,"BC548",{"B","C","E"},"T");
    component("BAT1","9V",{"1","2"},"BAT");
    auto net=[&](const QList<std::pair<QString,QString>> &pins){TargetNet n;for(const auto &[d,p]:pins)n.pins<<TargetPin{id[d],p};astable.nets<<n;};
    net({{"LED1","1"},{"LED2","1"},{"R2","1"},{"R3","1"},{"BAT1","1"}});net({{"LED1","2"},{"R1","1"}});net({{"LED2","2"},{"R4","1"}});
    net({{"R1","2"},{"C1","1"},{"T1","C"}});net({{"R4","2"},{"C2","1"},{"T2","C"}});net({{"R2","2"},{"C1","2"},{"T2","B"}});net({{"R3","2"},{"C2","2"},{"T1","B"}});
    net({{"T1","E"},{"T2","E"},{"BAT1","2"}});
    struct Hand {QString designator,library;int index;double x,y,angle;};
    const QList<Hand> hand{{"LED1",leds,3,2032,2032,0},{"LED2",leds,5,6096,2032,0},{"R1",resistors,2,2032,3556,90},{"R4",resistors,2,6096,3556,90},
        {"R2",resistors,2,3048,2286,90},{"R3",resistors,2,5080,2286,90},{"C1",caps,12,2540,4318,-90},{"C2",caps,12,5588,4826,90},
        {"T1",transistors,11,2032,5588,90},{"T2",transistors,11,6096,5588,90},{"BAT1",batteries,9,-1270,3048,180}};
    for(const auto &h:hand){
        static const QRegularExpression numbered(QStringLiteral("^(.*\\D)(\\d+)$"));const auto match=numbered.match(h.designator);
        board.additions<<QJsonObject{{"type","component"},{"library",h.library},{"index",h.index},{"x",h.x},{"y",h.y},{"angle",h.angle},{"component",id[h.designator]},{"id",match.captured(1)+'#'},{"group_value",match.captured(2).toInt()}};
    }
    board.assignIds();
    return {board,astable};
}
}
