#include "language.h"
#include "project.h"
#include "legacy_reader.h"
#include "legacy_writer.h"
#include "geometry.h"
#include "fixtures.h"
#include "notes.h"
#include "assistant.h"
#include <QImage>
#include <QCoreApplication>
#include <QFile>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <QTextStream>
#include <QtEndian>
#include <stdexcept>
#include <cmath>
using namespace openloch;
static void require(bool b,const char *message){if(!b)throw std::runtime_error(message);}
static Project imported(const QByteArray &bytes){
    Project p;p.original=bytes;p.sourceKind="lm4";p.legacy=LegacyReader(bytes).read(true);p.title=legacyTitle(p.legacy);
    auto size=p.legacy["size"].toArray();p.width=size[0].toDouble();p.height=size[1].toDouble();return p;
}
static QByteArray record(const QByteArray &bytes,const QJsonObject &node){return bytes.mid(node["start"].toInteger(),node["end"].toInteger()-node["start"].toInteger());}
int main(int argc,char **argv){
    QCoreApplication app(argc,argv);openloch::setUiLanguage("de"); // the tests compare German texts; CI runners use English systems
    fixtures::checkOwnModel(); // the own model must write every LM4 file as the program does
    try{
        QByteArray notes="{\\rtf1\\ansi eigene Anmerkung}";
        auto p=imported(fixtures::project(notes));require(writeLegacyProject(p)==p.original,"unmodified LM4 was not byte-exact");
        { // A 4.07 file of another program: an unchanged record is copied as read, a changed one written anew
            auto upgraded=imported(fixtures::project(notes));upgraded.title="Fremd";auto bytes=writeLegacyProject(upgraded);
            const auto start=LegacyReader(bytes).read(true)["objects"].toArray()[0].toObject()["start"].toInteger();
            const QByteArray ours("\x14\x07\x00\x00\x00TGruppe",12),theirs("\x06\x07TGruppe",9);   // the class name as UTF-8 string, as ShortString
            require(bytes.mid(start,12)==ours,"the record does not start with its class name");bytes.replace(start,12,theirs);
            auto foreign=imported(bytes);require(foreign.legacy["version"]=="4.07"&&foreign.legacy["objects"].toArray().size()==1,"the file of another program was not read");
            foreign.title="Anders";require(writeLegacyProject(foreign).contains(theirs),"an unchanged record of a 4.07 file must be copied as read");
            foreign.assignIds();require(foreign.setPins(foreign.components()[0].uid,{"A"})&&writeLegacyProject(foreign).contains(theirs),"named pins must leave a record as read");
            foreign.moves["0"]=QJsonArray{254,0};require(!writeLegacyProject(foreign).contains(theirs),"a moved record must be written anew");
        }
        { // Board settings changed in OpenLoch replace only their parts of a 4.07 file: pitch, grids, extra fields, main view
            auto upgraded=imported(fixtures::project(notes));upgraded.title="Einstellungen";auto board=imported(writeLegacyProject(upgraded));
            require(board.legacy["version"]=="4.07","the settings test needs a 4.07 file");board.boardSettings=QJsonObject{{"pitch",2.54},{"unit",2}};
            require(writeLegacyProject(board)==board.original,"settings equal to the file's changed the file");
            board.userOrigin=QJsonArray{508,762};const auto plainBytes=writeLegacyProject(board);const auto plain=LegacyReader(plainBytes).read(true);
            QJsonArray extra;extra.append(QJsonArray{"URL",true,"https://example.org"}); // QJsonArray{QJsonArray{…}} copies on some compilers
            board.boardSettings=QJsonObject{{"pitch",2.0},{"gridInch",.5},{"extra",extra},{"unit",1},
                {"view",QJsonObject{{"flip",true},{"bitmaps",true},{"xray",true},{"through",false},{"potentials",false}}}};
            const auto bytes=writeLegacyProject(board);const auto d=LegacyReader(bytes).read(true);
            require(d["grid_mm"].toDouble()==2.0&&d["numbers"].toArray()[0].toDouble()==plain["numbers"].toArray()[0].toDouble()&&d["numbers"].toArray()[1].toDouble()==.5,"pitch or grids were not saved");
            require(d["metadata"].toObject()["extra"].toArray()==board.boardExtra(),"the board's extra fields were not saved");
            const auto view=d["views"].toArray()[0].toObject();require(view["integers"].toArray()[2].toInt()==1&&view["flags"].toArray()[1].toBool()&&!view["flags"].toArray()[6].toBool(),"the main view or its unit was not saved");
            for(int i=1;i<11;i++)require(d["views"].toArray()[i]==plain["views"].toArray()[i],"a print view changed with the board settings");
            require(d["raw_points"]==plain["raw_points"]&&bytes.mid(d["annotations_offset"].toInteger(),d["annotations_size"].toInt())==plainBytes.mid(plain["annotations_offset"].toInteger(),plain["annotations_size"].toInt()),"the origin or the notes moved with the board settings");
            require(d["objects"]==plain["objects"],"objects changed with the board settings");
            const auto reread=imported(bytes);require(reread.pitch()==2.0&&reread.gridInch()==.5&&reread.unit()==1&&reread.mainView().flip&&!reread.mainView().potentials&&reread.boardExtra()==board.boardExtra(),"the settings did not come back from the file");
            require(Project::decode(board.encode()).boardSettings==board.boardSettings,"board settings did not survive .openloch");
            bool rejected=false;try{auto bad=board;bad.boardSettings["pitch"]=.05;Project::decode(bad.encode());}catch(const FormatError &){rejected=true;}require(rejected,"a pitch below 0.1 mm was accepted");
        }
        { // A part of an LM4 file edited in the Bauteil dialog keeps its name, list flag, extra fields and Bezugspunkt
            auto board=imported(fixtures::project());QJsonArray extra;extra.append(QJsonArray{"Hersteller",true,"Eigenbau"});
            board.edits["0"]=QJsonObject{{"label","Vorwiderstand"},{"group_flags",QJsonArray{true,false}},{"extra",extra},{"reference",0}};
            const auto part=LegacyReader(writeLegacyProject(Project::decode(board.encode()))).read(true)["objects"].toArray()[0].toObject();
            require(part["label"]=="Vorwiderstand"&&part["group_flags"].toArray()==QJsonArray({true,false})&&part["extra"].toArray()==extra,"a part's dialog values were not saved");
        }
        { // The file keeps the raw Kennung with its number like the original: "R#" and group_value, shown as R7
            auto board=imported(fixtures::project());board.edits["0"]=QJsonObject{{"id","R#"},{"group_value",7}};
            require(board.legacyNode(0)["id"]=="R7","the shown identifier is not the Kennung with its number");
            const auto part=LegacyReader(writeLegacyProject(Project::decode(board.encode()))).read(true)["objects"].toArray()[0].toObject();
            require(part["id"]=="R#"&&part["group_value"].toInt()==7,"the raw Kennung or its number was not saved");
            auto placed=imported(fixtures::project());const auto key=placed.addLibrary(fixtures::library(),"lib.lib");
            placed.additions.append(QJsonObject{{"type","component"},{"library",key},{"index",0},{"x",2540},{"y",2540},{"id","R#"},{"group_value",3}});
            const auto out=LegacyReader(writeLegacyProject(placed)).read(true)["objects"].toArray().last().toObject();
            require(out["id"]=="R#"&&out["group_value"].toInt()==3&&placed.componentNode(placed.additions[0].toObject())["id"]=="R3","a placed part did not keep its raw Kennung and number");
        }
        { // "Versatz (Bord<->Kupfer)" as in the original: the board moves under its content; objects, copper, new objects and
          // the origin keep their places to each other and their coordinates in the file
            auto board=imported(fixtures::project());board.additions.append(QJsonObject{{"type","wire"},{"x",1270},{"y",2540},{"x2",2540},{"y2",2540},{"color","#c0c0c0"},{"width",40}});
            const auto at=[](const Project &q){const auto placed=q.placedObjects();return placed[0].transform.map(QPointF(762,1016));};
            const QPointF legacyBefore=at(board);board.setOffset({127,205});const QPointF delta(127,205);
            require(board.offset()==delta&&at(board)==legacyBefore-delta,"an imported object did not keep its place on the copper");
            require(board.additions[0].toObject()["x"].toDouble()==1270-127&&board.additions[0].toObject()["y2"].toDouble()==2540-205,"a new object did not move with the content");
            require(board.origin()==-delta,"the origin did not move with the content");
            const auto copper=board.libraries.value(board.boardSource).document;const auto copperWire=copper["objects"].toArray()[0].toObject()["path"].toArray()[0].toArray();
            require(QPointF(copperWire[0].toDouble(),copperWire[1].toDouble())-QPointF(copper["origin"].toArray()[0].toDouble(),copper["origin"].toArray()[1].toDouble())==QPointF(762,1016)-delta,"the copper did not stay under the objects");
            const auto bytes=writeLegacyProject(board);const auto d=LegacyReader(bytes).read(true);
            require(d["origin"].toArray()==QJsonArray{127,205}&&d["board"].toObject()["origin"].toArray()==QJsonArray{127,205},"the offset was not saved");
            require(d["objects"].toArray()[0].toObject()["children"].toArray()[0].toObject()["path"].toArray()[0].toArray()==QJsonArray{762,1016},"an object's file coordinates changed with the offset");
            require(d["board"].toObject()["objects"].toArray()[0].toObject()["path"].toArray()[0].toArray()==QJsonArray{762,1016},"the copper's file coordinates changed with the offset");
            require(QByteArray::fromHex(d["raw_points"].toString().toLatin1()).left(8)==QByteArray(8,0),"the origin's file coordinates changed with the offset");
            const auto reread=imported(bytes);require(at(reread)==at(board)&&reread.offset()==delta,"the offset did not come back from the file");
            auto back=Project::decode(board.encode());require(back.offset()==delta&&back.origin()==board.origin(),"the offset did not survive .openloch");
        }
        {   // A 24-bit picture with a colour table as the original saves it: file size and data offset 1024 bytes too large.
            // It is read by its info header, so that the objects after it are found.
            QByteArray info=QByteArray::fromHex("28000000010000000100000001001800000000000400000000000000000000000001000000000000");
            QByteArray picture="BM";auto u32=[](quint32 v){QByteArray b(4,0);qToLittleEndian(v,b.data());return b;};
            picture+=u32(14+40+1024+4+1024)+u32(0)+u32(14+40+2048)+info+QByteArray(1024,'\x11')+QByteArray::fromHex("33221100");
            const auto read=LegacyReader(fixtures::library(picture)).read(false);const auto wire=read["objects"].toArray().at(0).toObject()["children"].toArray().at(0).toObject();
            require(wire["bitmap_size"].toInteger()==14+40+1024+4&&!read["objects"].toArray().isEmpty(),"a bitmap saved by the original must be read by its info header");
            // Its pixels come from its bits, not from the colour table the stored offset would point into.
            const QByteArray stored=fixtures::library(picture).mid(wire["bitmap_offset"].toInteger(),wire["bitmap_size"].toInteger());
            const QImage image=QImage::fromData(legacyBitmapData(stored),"BMP");
            require(!image.isNull()&&image.pixelColor(0,0)==QColor(0x11,0x22,0x33),"a bitmap saved by the original must show its own colours");
            const QByteArray plain=QByteArray::fromHex("424d3a00000000000000360000002800000001000000010000000100180000000000040000000000000000000000000000000000000033221100");
            require(legacyBitmapData(plain)==plain,"a usual bitmap stays as it is");
        }
        auto bitmap=QByteArray::fromHex("424d3a00000000000000360000002800000001000000010000000100180000000000040000000000000000000000000000000000000033221100");
        auto key=p.addLibrary(fixtures::library(bitmap),"own.lib");
        p.edits["0"]=QJsonObject{{"id","R9"},{"value","47 kΩ"},{"description","Prüfung µ"}};p.moves["0"]=QJsonArray{254,508};
        p.additions.append(QJsonObject{{"type","component"},{"library",key},{"index",0},{"x",2540},{"y",2540},{"id","R10"},{"angle",90}});
        auto before=p.encode();auto bytes=writeLegacyProject(p);require(p.encode()==before,"export mutated project");
        auto d=LegacyReader(bytes).read(true);require(d["version"]=="4.07","edited files were not upgraded to 4.07");
        require(bytes.mid(d["annotations_offset"].toInteger(),d["annotations_size"].toInt())==notes,"RTF notes were lost during upgrade");
        auto a=d["objects"].toArray();require(a.size()==2,"export lost placed components");
        auto first=a[0].toObject();require(first["id"]=="R9"&&first["value"]=="47 kΩ"&&first["description"]=="Prüfung µ","Unicode properties changed");
        auto firstWire=first["children"].toArray()[0].toObject();require(firstWire["path"].toArray()[0].toArray()==QJsonArray{1016,1524},"imported component translation changed");
        {   // A moved label moves as in the original: its corners, not its snapshot while that is not valid; angle and flags stay.
            const auto label=first["children"].toArray()[1].toObject();
            require(label["text_position"].toArray()==QJsonArray{1016,1208}&&label["text_end"].toArray()==QJsonArray{1270,850}
                    &&label["text_anchors"].toArray()==QJsonArray{1016,1208,1270,850,1016,1208,1270,850,1016,1208,1270,850}&&label["text_height"].toDouble()==0,
                    "a moved label must move as in the original");
        }
        {   // A turned or mirrored label with stored corners turns its corners only, as in the original: angle, snapshot and
            // flags stay. Without stored corners it stays collapsed at its new top-left corner with the new angle.
            auto board=imported(fixtures::project());auto objects=board.legacy["objects"].toArray();
            const auto part=objects[0].toObject();auto children=part["children"].toArray();auto label=children[1].toObject();
            label["text_anchors"]=QJsonArray{1270,700,9,1,1270,850,9,2,762,850,9,3};   // P1, S1, P2, S2, P3, S3
            label["text_flags"]=QJsonArray{true,true};label["text_height"]=0.5;label["text_width"]=0.25;
            children[1]=label;auto stored=part;stored["children"]=children;objects[0]=stored;board.legacy["objects"]=objects;
            auto exported=[&](const QJsonObject &edit){board.edits["0"]=edit;return LegacyReader(writeLegacyProject(board)).read(true)["objects"].toArray()[0].toObject()["children"].toArray()[1].toObject();};
            for(const auto &edit:{QJsonObject{{"angle",90}},QJsonObject{{"mirrorX",true}}}){
                const auto t=objectTransform(edit,componentAnchor(part));const bool mirrored=edit["mirrorX"].toBool();
                auto at=[&](double x,double y){const auto p=t.map(QPointF(x,y));return QJsonArray{qRound(p.x()),qRound(p.y())};};
                QList<QJsonArray> c{at(762,700),at(1270,700),at(1270,850),at(762,850)};if(mirrored)c={c[1],c[0],c[3],c[2]};
                const auto out=exported(edit);const auto a=out["text_anchors"].toArray();
                require(out["text_position"].toArray()==c[0]&&QJsonArray{a[0],a[1]}==c[1]&&QJsonArray{a[4],a[5]}==c[2]&&QJsonArray{a[8],a[9]}==c[3],"the corners of a turned label are wrong");
                require(QJsonArray{a[2],a[3],a[6],a[7],a[10],a[11]}==QJsonArray{9,1,9,2,9,3}&&out["text_end"].toArray()==QJsonArray{1270,850}
                        &&out["text_height"].toDouble()==0.5&&out["text_width"].toDouble()==0.25&&out["text_flags"].toArray()==QJsonArray{true,true},
                        "a turned label must keep angle, snapshot and flags as in the original");
                require(out["text_flags2"].toArray()==QJsonArray{mirrored,false},"mirroring must toggle the label's mirror flag as in the original");
            }
            board=imported(fixtures::project());const auto collapsed=exported(QJsonObject{{"angle",90}});const auto p=collapsed["text_position"].toArray();
            require(std::abs(collapsed["text_height"].toDouble()-3*3.14159265358979323846/2)<1e-9&&collapsed["text_anchors"].toArray()==QJsonArray{p[0],p[1],p[0],p[1],p[0],p[1],p[0],p[1],p[0],p[1],p[0],p[1]},
                    "a turned label without stored corners must stay collapsed with the new angle");
        }
        {   // The anchor of a part, to which placements are saved, does not follow the stored corners of its labels; they
            // only make a label hit where it is drawn.
            const auto part=imported(fixtures::project()).legacy["objects"].toArray()[0].toObject();auto children=part["children"].toArray();
            auto label=children[1].toObject();label["text_anchors"]=QJsonArray{5080,700,9,1,5080,5080,9,2,762,5080,9,3};children[1]=label;
            auto stored=part;stored["children"]=children;
            require(componentAnchor(stored)==componentAnchor(part)&&legacyBounds(stored).bottom()>=5080&&legacyBounds(part).bottom()<5080,
                    "the anchor of a part must not follow the corners of its labels");
        }
        {   // A label of a file before 4.04 has no stored corners: written as 4.07, also unchanged, it is collapsed at P0 with
            // its snapshot at S0, as the original reads it. Corners all at the zero point count as none.
            auto board=imported(fixtures::project());board.title="Neu";
            const auto label=LegacyReader(writeLegacyProject(board)).read(true)["objects"].toArray()[0].toObject()["children"].toArray()[1].toObject();
            const auto p=label["text_position"].toArray(),s=label["text_end"].toArray();
            require(label["text_anchors"].toArray()==QJsonArray{p[0],p[1],s[0],s[1],p[0],p[1],s[0],s[1],p[0],p[1],s[0],s[1]},
                    "an old label must be written collapsed at its corner, not at the zero point");
            auto zeros=label;zeros["text_anchors"]=QJsonArray{0,0,0,0,0,0,0,0,0,0,0,0};auto bare=label;bare.remove("text_anchors");
            require(!labelStoredCorners(zeros)&&legacyBounds(zeros)==legacyBounds(bare),"corners all at the zero point are none");
        }
        auto original=p.libraryNode(key,0);auto t=objectTransform(p.additions[0].toObject(),componentAnchor(original));
        auto expected=t.map(QPointF(762,1016))+QPointF(2540,2540)-componentAnchor(original);
        auto placed=a[1].toObject()["children"].toArray()[0].toObject()["path"].toArray()[0].toArray();
        require(placed==QJsonArray{qRound(expected.x()),qRound(expected.y())},"placed component rotation changed");
        auto placedWire=a[1].toObject()["children"].toArray()[0].toObject();
        require(bytes.mid(placedWire["bitmap_offset"].toInteger(),placedWire["bitmap_size"].toInt())==bitmap,"embedded bitmap bytes changed");
        require(record(bytes,d["board"].toObject())==record(p.original,p.legacy["board"].toObject()),"embedded board changed");
        QTemporaryDir tmp;auto path=tmp.filePath("export.LM4");p.save(path);auto reopened=Project::load(path);require(writeLegacyProject(reopened)==bytes,"disk roundtrip changed exported file");
        // A second edit leaves the other record and all opaque tail data exact.
        auto secondBefore=reopened.legacy["objects"].toArray()[1].toObject();reopened.edits["0"]=QJsonObject{{"value","100 kΩ"}};
        auto secondBytes=writeLegacyProject(reopened);auto secondDoc=LegacyReader(secondBytes).read(true);
        require(record(bytes,secondBefore)==record(secondBytes,secondDoc["objects"].toArray()[1].toObject()),"unchanged record was rewritten");
        auto tail=[](const QByteArray &b,const QJsonObject &doc){return b.mid(doc["tail_offset"].toInteger(),doc["board"].toObject()["start"].toInteger()-doc["tail_offset"].toInteger());};
        require(tail(bytes,reopened.legacy)==tail(secondBytes,secondDoc),"metadata, notes or view settings changed");
        reopened.edits={};reopened.title="Renamed";auto renamedBytes=writeLegacyProject(reopened);auto renamed=LegacyReader(renamedBytes).read(true);
        require(record(bytes,reopened.legacy["objects"].toArray()[0].toObject())==record(renamedBytes,renamed["objects"].toArray()[0].toObject()),"renaming a project rewrote its objects");
        reopened.edits["0"]=QJsonObject{{"deleted",true}};require(LegacyReader(writeLegacyProject(reopened)).read(true)["objects"].toArray().size()==1,"deleted component was exported");
        Project fresh;fresh.title="Platine Ω · 日本語 mit einem sehr langen Projektnamen";fresh.width=1016;fresh.height=1016;
        for(auto type:{"wire","cut","pad","text","resistor","capacitor","ground","diode"})fresh.additions.append(QJsonObject{{"type",type},{"x",254},{"y",508},{"x2",762},{"y2",508},{"text","Ω · µ"}});
        auto freshBytes=writeLegacyProject(fresh);auto freshDoc=LegacyReader(freshBytes).read(true);
        require(freshDoc["objects"].toArray().size()==8,"new primitive was lost");
        require(freshDoc["board"].toObject()["objects"].toArray().size()==18,"default perfboard pads or holes changed");
        require(freshDoc["grid_mm"].toDouble()==2.54,"new project grid changed");
        Project pin;pin.additions={QJsonObject{{"type","pin"},{"x",762},{"y",1016}}};
        const auto pinNode=LegacyReader(writeLegacyProject(pin)).read(true)["objects"].toArray().first().toObject();
        QJsonArray pinTerminal;pinTerminal.append(QJsonArray{762,1016});
        require(pinNode["path"].toArray()==QJsonArray{QJsonArray{762,1016},QJsonArray{762,1016}}&&pinNode["points"].toArray()==pinTerminal,"pin must be exported as two equal points with a single terminal");
        require(pinNode["kind"].toInt()==11,"pin must be exported as LochMaster's pin (kind 11), not as a potential marker");
        Project marker;marker.additions={QJsonObject{{"type","potential"},{"x",1016},{"y",762},{"color","#ff8080"},{"name","Vcc"}}};
        const auto markerNode=LegacyReader(writeLegacyProject(marker)).read(true)["objects"].toArray().first().toObject();
        require(markerNode["kind"].toInt()==18&&markerNode["width"].toInt()==200&&markerNode["path"].toArray()==QJsonArray{QJsonArray{1016,762},QJsonArray{1016,762}},"potential marker must be exported as kind 18 like LochMaster");
        require(quint32(markerNode["pen"].toInteger())==0x8080ff&&markerNode["label"].toString()=="Vcc","potential marker lost its colour or name");
        // Drawing lines are LochMaster kind 4 (no electrical function), leads kind 9, solder blobs kind 19.
        Project drawn;drawn.additions={QJsonObject{{"type","polyline"},{"x",254},{"y",254},{"points",QJsonArray{QJsonArray{0,0},QJsonArray{508,0}}}},
            QJsonObject{{"type","lead"},{"x",762},{"y",254},{"points",QJsonArray{QJsonArray{0,0},QJsonArray{0,508}}},{"back",false}},
            QJsonObject{{"type","solder"},{"x",1016},{"y",254},{"width",150},{"back",true}}};
        const auto drawnObjects=LegacyReader(writeLegacyProject(drawn)).read(true)["objects"].toArray();
        require(drawnObjects[0].toObject()["kind"].toInt()==4,"a contour must not become an electrical lead");
        require(drawnObjects[1].toObject()["kind"].toInt()==9&&drawnObjects[1].toObject()["path"].toArray().first()==QJsonArray{762,254},"lead must start at its soldered end");
        const auto blob=drawnObjects[2].toObject();require(blob["kind"].toInt()==19&&blob["width"].toInt()==150&&blob["back"].toBool()&&blob["path"].toArray().size()==2,"solder blob must be exported as LochMaster's kind 19");
        Project bridges;bridges.additions={QJsonObject{{"type","wire"},{"x",254},{"y",254},{"x2",762},{"y2",254},{"back",false}},QJsonObject{{"type","wire"},{"x",254},{"y",508},{"x2",762},{"y2",508}}};
        const auto bridgeNodes=LegacyReader(writeLegacyProject(bridges)).read(true)["objects"].toArray();
        require(!bridgeNodes[0].toObject()["back"].toBool()&&bridgeNodes[1].toObject()["back"].toBool(),"LM4 export changed the side of a wire bridge or an older wire");
        require(connectionPoints(bridgeNodes[0].toObject()).size()==2,"LM4 wire bridge lost one of its solder terminals");
        // The original program saves the virtual terminal count from its cache.
        // These synthetic 3.08 records deliberately have empty caches: they
        // must be repaired at every group/lead level in the new 4.07 export.
        const auto placedGroup=a[1].toObject();
        require(placedGroup["points"].toArray().size()==1&&placedWire["points"].toArray().size()==1,"new component or lead has an incomplete terminal cache");
        require(placedGroup["points"].toArray().first()==placedWire["path"].toArray().first(),"new group cache does not use the placed terminal");
        require(placedWire["points"].toArray().first()==placedWire["path"].toArray().first(),"new lead cache does not use the placed terminal");
        Project coincident;coincident.additions={QJsonObject{{"type","wire"},{"x",254},{"y",254},{"x2",254},{"y2",254}}};
        const auto coincidentNode=LegacyReader(writeLegacyProject(coincident)).read(true)["objects"].toArray().first().toObject();
        require(coincidentNode["points"].toArray().size()==2,"coincident wire terminals were deduplicated in the save cache");
        auto unicodePath=tmp.filePath("unicode.LM4");fresh.save(unicodePath);auto unicode=Project::load(unicodePath);
        require(unicode.title==fresh.title&&writeLegacyProject(unicode)==freshBytes,"full Unicode project title was lost");
        fresh.boardSource=fresh.addLibrary(fixtures::board(),"own.lmb","lmb");
        require(LegacyReader(writeLegacyProject(fresh)).read(true)["board"].toObject()["objects"].toArray().size()==1,"selected board template was lost");
        QString note="Anmerkung Ω µ 日本語\n{Text} \\ mit\tTab";require(plainNotes(rtfNotes(note))==note,"Unicode RTF note conversion failed");
        require(plainNotes("{\\rtf1{\\fonttbl{\\f0 Arial;}}\\f0 Gr\\'fc\\'dfe\\par weiter}")==QString::fromUtf8("Grüße\nweiter"),"RTF import leaked font table or lost ANSI text");
        auto annotated=Project::load(path);annotated.notes=note;annotated.notesRtf.clear();annotated.notesEdited=true;auto annotatedBytes=writeLegacyProject(annotated);auto annotatedDoc=LegacyReader(annotatedBytes).read(true);
        require(plainNotes(annotatedBytes.mid(annotatedDoc["annotations_offset"].toInteger(),annotatedDoc["annotations_size"].toInteger()))==note,"edited notes were not exported");
        require(record(bytes,d["objects"].toArray()[0].toObject())==record(annotatedBytes,annotatedDoc["objects"].toArray()[0].toObject()),"editing notes rewrote unchanged components");
        Project rotated;rotated.width=2540;rotated.height=1270;rotated.additions={QJsonObject{{"type","wire"},{"x",254},{"y",508},{"x2",762},{"y2",508}}};auto initial=LegacyReader(writeLegacyProject(rotated)).read(true);
        rotated.rotateBoard();require(rotated.width==1270&&rotated.height==2540,"whole board dimensions were not rotated");auto turned=LegacyReader(writeLegacyProject(rotated)).read(true);auto turnedPath=turned["objects"].toArray()[0].toObject()["path"].toArray();require(turnedPath==QJsonArray{QJsonArray{762,254},QJsonArray{762,762}},"whole board rotation changed wire length or placement");
        for(int i=0;i<3;i++)rotated.rotateBoard();auto fullTurn=LegacyReader(writeLegacyProject(rotated)).read(true);require(fullTurn["objects"].toArray()[0].toObject()["path"]==initial["objects"].toArray()[0].toObject()["path"],"four board rotations did not restore geometry");
        const QJsonValue originalPad=initial["board"].toObject()["objects"].toArray()[0].toObject()["center"];require(fullTurn["board"].toObject()["objects"].toArray()[0].toObject()["center"]==originalPad,"board holes did not rotate with the design");
        { // object assistant against the original's worked example: resistor D 3 mm, L 7 mm around (cx, cy)
            const double cx=15240,cy=15240;auto at=[](const QJsonValue &v,int i){auto a=v.toArray().at(i).toArray();return QPointF(a[0].toDouble(),a[1].toDouble());};
            auto resistor=assistantObject(11,assistantParameters(11),gradientBitmap(11,"Beige"));auto parts=resistor["children"].toArray();
            require(resistor["type"]=="TBt"&&resistor["component_kind"]==11&&resistor["id"]=="R#"&&resistor["value"]=="1 KOhm"&&parts.size()==5,"assistant resistor structure differs");
            auto lead1=parts[0].toObject(),lead2=parts[1].toObject();
            require(lead1["kind"]==9&&lead1["width"]==40&&lead1["pen"]==0xC0C0C0&&at(lead1["path"],0)==QPointF(cx,cy-508)&&at(lead1["path"],1)==QPointF(cx,cy-350)&&at(lead2["path"],0)==QPointF(cx,cy+508),"assistant leads differ from the original");
            require(lead1["points"].toArray().size()==1&&at(lead1["points"],0)==QPointF(cx,cy-508),"assistant lead points must stay a list of coordinate pairs");
            auto shell=parts[2].toObject();require(shell["transparent"].toBool()&&shell["flag2"].toBool()&&shell["style"]==0&&at(shell["path"],0)==QPointF(cx-150,cy-350)&&at(shell["path"],2)==QPointF(cx-135,cy-210)&&!shell["bitmap"].toString().isEmpty()&&shell["anchors"].toArray()==QJsonArray({cx-150,cy-350,cx+150,cy-350,cx-150,cy+350}),"assistant resistor body differs");
            auto label=parts[3].toObject();require(label["text"]=="<BauteilKennung>"&&at(QJsonArray{label["text_position"]},0)==QPointF(cx+90,cy-315)&&label["text_kind"]==180,"assistant label differs");
            auto bands=parts[4].toObject()["children"].toArray();require(parts[4].toObject()["type"]=="TFarbcode"&&bands.size()==4,"colour code missing");
            const QList<double> rows{cy+131,cy+88,cy+44,cy};const QList<int> pens{0x0055BB,0x000000,0x0000FF,0xFFFF00};
            for(int i=0;i<4;i++){auto band=bands[i].toObject();require(at(band["path"],0)==QPointF(cx-124,rows[i])&&at(band["path"],1)==QPointF(cx+124,rows[i])&&band["pen"]==pens[i]&&band["width"]==(i<3?20:0),"colour band differs from the original");}
            // Written and read back: a library document with the embedded picture.
            const auto bytes=writeLegacyObjects({resistor},"Assistent");auto read=LegacyReader(bytes).read(false)["objects"].toArray()[0].toObject();
            require(read["type"]=="TBt"&&read["component_kind"]==11&&read["children"].toArray().size()==5&&read["children"].toArray()[2].toObject()["bitmap_size"].toInteger()>0,"assistant part did not survive writing");
            require(!QImage::fromData(gradientBitmap(13,"Rot"),"BMP").isNull()&&gradientNames(12).size()==9,"own body pictures missing");
            // Lead pitch: Round(R/508 + 1) grid steps, so 2.54, 5.08 and 7.62 mm all give 10.16 mm (the original's rounding).
            for(double pitch:{2.54,5.08,7.62}){auto capacitor=assistantParameters(13);capacitor[3].value=pitch;require(at(assistantObject(13,capacitor)["children"].toArray()[0].toObject()["path"],0)==QPointF(cx,cy-508),"capacitor pitch differs from the original");}
            auto diode=assistantObject(14,assistantParameters(14))["children"].toArray();require(diode.size()==5&&diode[4].toObject()["kind"]==4&&diode[4].toObject()["width"]==40&&diode[3].toObject()["type"]=="TTextLabel","assistant diode differs");
            const auto corners=label["text_anchors"].toArray();require(QPointF(corners[4].toDouble(),corners[5].toDouble())==QPointF(cx+90,cy-315)&&label["text_end"]==label["text_position"],"assistant labels must keep collapsed corners like the original's");
            // Contours: one outline; a rectangle turned by 90° moves its first corner to the bottom left.
            auto square=assistantParameters(1);square[2].value=90;auto turned=assistantObject(1,square);
            require(turned["kind"]==6&&turned["path"].toArray().size()==4&&at(turned["path"],0)==QPointF(cx-500,cy+500),"contour rotation differs");
            auto ellipse=assistantObject(2,assistantParameters(2));require(ellipse["type"]=="TKreis"&&ellipse["inner"].toObject()["path"].toArray().size()==16&&at(ellipse["inner"].toObject()["path"],0)==QPointF(cx+462,cy+191),"ellipse differs");
            for(int style=0;style<15;style++){auto o=assistantObject(style,assistantParameters(style));require(!o.isEmpty()&&LegacyReader(writeLegacyObjects({o},"Stil")).read(false)["objects"].toArray().size()==1,"an assistant style could not be written");}
            // Values: German number format, SI prefixes, standard series, range correction.
            require(formatOhm(4700)=="4,7 KOhm"&&formatOhm(470)=="470  Ohm"&&formatOhm(1e12)=="1000 GOhm"&&parseOhm(formatOhm(1e12))==1e12&&parseOhm("4,7 k")==4700&&parseOhm("2M2")==2e6,"Ohm format differs");
            require(standardValue(1234,1)==1200&&std::abs(standardValue(4.66,1)-4.7)<1e-9,"standard series snapping failed");
            require(colourBands(4.7,1)==QList<int>({0x00FFFF,0xFF00FF,0x008080,-1})&&colourBands(10000,4)==QList<int>({0x0055BB,0x000000,0x000000,0x0000FF}),"colour bands differ");
            auto rows2=assistantParameters(11);require(assistantSetValue(rows2[0],"250")==AssistantInput::Corrected&&rows2[0].value==200,"out-of-range length was not corrected");
            require(assistantSetValue(rows2[4],"5 T")==AssistantInput::Corrected&&rows2[4].value==.1,"an Ohm value out of range must become the lower bound");
            require(assistantSetValue(rows2[0],"4,5")==AssistantInput::Accepted&&assistantSetValue(rows2[0],"abc")==AssistantInput::Invalid&&rows2[0].value==4.5&&assistantValueText(rows2[0])=="4,5","decimal comma not accepted");
        }
        { // OpenLoch objects keep what the editor shows: pin and solder defaults, ellipse colour, text corners and direction
            Project board;board.mode="board";board.width=5080;board.height=5080;
            board.additions={QJsonObject{{"type","pin"},{"x",1016},{"y",1016}},QJsonObject{{"type","solder"},{"x",1524},{"y",1016}},
                QJsonObject{{"type","ellipse"},{"x",508},{"y",2032},{"x2",1524},{"y2",2540},{"color","#208020"}},
                QJsonObject{{"type","text"},{"x",3048},{"y",1016},{"text","Test"},{"textSize",100},{"angle",90}}};
            const auto objects=LegacyReader(writeLegacyProject(board)).read(true)["objects"].toArray();require(objects.size()==4,"native objects missing in the export");
            const auto pin=objects[0].toObject(),blob=objects[1].toObject(),ring=objects[2].toObject(),label=objects[3].toObject();
            require(pin["kind"]==11&&pin["width"]==45&&pin["pen"].toInteger()==0x3138b5,"pin lost its default size or colour");
            require(blob["kind"]==19&&blob["width"]==150,"solder blob lost its default size");
            require(ring["type"]=="TKreis"&&ring["pen"].toInteger()==0x208020&&ring["inner"].toObject()["pen"].toInteger()==0x208020,"ellipse lost its colour");
            // Turned 90° clockwise in OpenLoch, the text runs downwards: 270° counter-clockwise, first corner below the position.
            const auto at=label["text_position"].toArray(),corners=label["text_anchors"].toArray();
            require(std::abs(label["text_height"].toDouble()-3*M_PI/2)<1e-9&&corners[4]==at[0]&&corners[5]==at[1]&&label["text_flags"].toArray()[0].toBool(),"turned text runs the wrong way in the export");
        }
        { // several boards: board 0, the count, then the others; read back as boards with the last one shown, written back unchanged
            Project three;three.mode="board";three.title="Eins";three.width=2540;three.height=2540;three.additions={QJsonObject{{"type","pin"},{"x",508},{"y",508}}};
            three.addBoard();three.title="Zwei";three.addBoard();three.title="Drei";three.additions={QJsonObject{{"type","solder"},{"x",1016},{"y",1016}}};
            const auto bytes=writeLegacyProject(three);QTemporaryDir boardsDir;const auto file=boardsDir.filePath("Drei.LM4");three.save(file);
            const auto loaded=Project::load(file);require(loaded.boards.size()==3&&loaded.activeBoard==2&&loaded.title=="Drei"&&loaded.legacy["objects"].toArray().size()==1,"boards of a multi-board LM4 missing");
            require(Project::decode(QJsonDocument(loaded.boards[0].toObject()).toJson()).title=="Eins","first board lost its name");
            require(writeLegacyProject(loaded)==bytes,"an unchanged multi-board LM4 was not written back byte for byte");
            const int countAt=int(LegacyReader(bytes).read(true)["board_end"].toInteger());require(bytes.mid(countAt,2)==QByteArray::fromHex("0203"),"board count missing after board 0");
            // Library pages end with their document, board templates with the count 1, like the original's files.
            Project page;page.mode="board";page.title="Seite";require(!writeLegacyDocument(page).endsWith(QByteArray::fromHex("0201"))&&writeLegacyDocument(page,false,false,true).endsWith(QByteArray::fromHex("0201")),"library page or template ending differs from the original");
        }
        { // layout editing: the board's layout as a template, a track and a pad with drill added, applied back as the board's layout
            Project board;board.mode="board";board.width=2540;board.height=2540;auto layout=board.layoutProject();
            require(layout.sourceKind=="lmb"&&!layout.legacy["objects"].toArray().isEmpty(),"the generated perfboard layout is missing");
            layout.additions={QJsonObject{{"type","track"},{"x",254},{"y",508},{"x2",2286},{"y2",508},{"width",200}},QJsonObject{{"type","pad"},{"x",508},{"y",1016},{"diameter",2.0},{"drill",1.0}}};
            board.applyLayout(layout);const auto doc=LegacyReader(writeLegacyProject(board)).read(true)["board"].toObject();bool track=false,pad=false;
            for(auto v:doc["objects"].toArray()){auto o=v.toObject();if(o["type"]=="TLeiterbahn"&&o["width"]==100&&o["brush"]==0x808080)track=true;
                if(o["type"]=="TGruppe"&&o["group_flags"].toArray()==QJsonArray{false,true}&&o["children"].toArray().size()==2&&o["children"].toArray()[1].toObject()["diameter"].toDouble()==1.0)pad=true;}
            require(track&&pad,"layout objects did not reach the board's layout");
        }
        QTextStream(stdout)<<"LM4 writer tests passed\n";return 0;
    }catch(const std::exception &e){QTextStream(stderr)<<e.what()<<"\n";return 1;}
}
