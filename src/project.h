#pragma once
#include <QByteArray>
#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QMap>
#include <QPointF>
#include <QTransform>
#include <QList>
#include <QStringList>
#include <QSet>
namespace openloch {
struct LibrarySource {QString name,kind;QByteArray bytes;QJsonObject document;};
struct Project {
    QString title = "Neue Platine", mode = "board", sourceName, sourceKind, boardSource;
    // Notes ("Anmerkungen") as the RTF the original stores, and as plain text. Unedited notes are written back as read;
    // edited ones (notesEdited) as notesRtf, or built from the text when notesRtf is empty: clear it when setting only the text.
    QString notes;
    QByteArray notesRtf;
    bool notesEdited=false;
    QJsonArray boards;
    int activeBoard=0;
    QByteArray original;
    QJsonObject legacy;
    QJsonArray additions;
    QJsonObject moves;
    QJsonObject edits;
    QMap<QString,LibrarySource> libraries;
    double width = 10000, height = 8000;
    // User origin set with "Ursprung setzen" in board coordinates; empty means the one stored in the LochMaster file.
    QJsonArray userOrigin;
    // Print preview settings changed in OpenLoch (views, view_index, view_flags, view_integer of the board document);
    // empty means the ones stored in the LochMaster file.
    QJsonObject print;
    QPointF origin() const;
    // Board settings changed in OpenLoch ("Platine → Eigenschaften" and the main view); missing keys keep those of the
    // LochMaster file or the original's defaults: "pitch" (Lochabstand N in mm), "gridMm" and "gridInch" (the snap grids
    // of the units mm and inch, in mm), "offset" (Versatz), "extra" (the board's extra fields), "unit" (0 mm, 1 inch, 2 N)
    // and "view" (the main view's switches flip, bitmaps, xray, through, potentials).
    QJsonObject boardSettings;
    // The board offset ("Versatz Bord<->Kupfer"): where the board's corner lies in the coordinates of the document.
    QPointF offset() const;
    double pitch() const;
    double gridMm() const;
    double gridInch() const;
    int unit() const;
    QJsonArray boardExtra() const;
    // The snap grid of a unit in 1/100 mm, as the original switches it with the unit.
    double snapGrid(int unit) const{return 100*(unit==2?pitch():unit==1?gridInch():gridMm());}
    struct MainView{bool flip=false,bitmaps=true,xray=true,through=false,potentials=true;};
    MainView mainView() const;
    // "Versatz (Bord<->Kupfer)": the board moves under its content. The copper becomes a layout with the new offset and
    // keeps its place under the objects; new objects and the origin move with them in board coordinates.
    void setOffset(QPointF offset);
    // Keeps the main view and unit for saving; nothing changes when they are those already stored.
    void storeMainView(const MainView &view,int unit);
    static Project load(const QString &path);
    void save(const QString &path) const;
    QByteArray encode() const;
    static Project decode(const QByteArray &data);
    QString addLibrary(const QByteArray &data,const QString &name,const QString &kind="lib");
    QJsonObject libraryNode(const QString &key,int index) const;
    QJsonObject componentNode(const QJsonObject &placement) const;
    // An imported object with its edits; `reference` false leaves a chosen Bezugspunkt out (terminals in original order).
    QJsonObject legacyNode(int index,bool reference=true) const;
    QString nextId(const QString &pattern) const;
    // The first free number for a Kennung like "R#" among all parts, also nested ones, as LochMaster numbers parts.
    int nextNumber(const QString &pattern) const;
    // The Kennung as stored ("R#"), not as shown ("R4"): of an imported object ("legacy") or a new one ("new").
    QString rawId(const QString &kind,int index) const;
    QJsonArray billOfMaterials() const;
    void renumber();
    void switchBoard(int index);
    void addBoard(bool duplicate=false);
    void removeBoard();
    void rotateBoard();
    // The current board's copper layout (template, embedded layout or generated perfboard) as an editable template project.
    Project layoutProject() const;
    // Makes `layout` this board's copper layout, as "Layout übernehmen" does; size and origin stay the board's.
    void applyLayout(const Project &layout);
    static Project fromLegacyBytes(const QByteArray &bytes,const QString &suffix,const QString &name);
    // Moves the current board to another position in the board list.
    void moveBoard(int index);
    // Appends the boards of another project after the existing ones and shows the first of them.
    void appendBoards(const Project &other);
    // The board's objects in drawing order (bottom first) with the transform the canvas places them with; parts are
    // resolved to their groups. `kind` is "legacy" or "new" with the index in the imported objects or the additions.
    struct Placed{QString kind;int index;QJsonObject node;QTransform transform;};
    QList<Placed> placedObjects() const;
    // Identifiers of the top-level objects (docs/openloch-project-format.md, "Kennungen und Bauteile"): imported ones
    // by their index in `uids`, new ones in their "uid" field, 32 hexadecimal digits. assignIds gives missing ones and
    // renews one that comes twice (a copy); reading and every recorded change call it.
    // `taken`: the identifiers of the document's other boards; one of them is renewed too, and this board's are added.
    // The identifiers are unique in the whole document.
    QJsonObject uids;
    // The board's own identifier, unique in the document like the objects' ones: what other documents of a project name
    // the board by (a front panel in front of it). assignIds gives it, renewIds renews it.
    QString boardId;
    void assignIds(QSet<QString> *taken=nullptr);
    // A copy (a duplicated or added board): the board and every object get a new identifier.
    void renewIds();
    QString uidOf(const QString &kind,int index) const;
    // The parts of the document for projects, board by board: the component it stands for (`id`: its field "component",
    // else the part's own identifier `uid`), Kennung as shown (R4), value and its pins in the order of its connection
    // points without Bezugspunkt, named by its field "pins", else by the labels of their leads when every lead has one
    // of its own (the open libraries name them), else "1", "2", …; `kind` and `index` as in placedObjects, on board
    // `board`.
    struct Component{QString id,uid,designator,value;QStringList pins;QString kind;int index=-1,board=0;};
    QList<Component> components() const;
    // The component an object of the board shown stands for; empty when the object is no part.
    QString componentOf(const QString &kind,int index) const;
    // By the part's own identifier, on any board: the component it stands for (empty or its own identifier: none of
    // its own) and the names of its pins (empty: as the part names them). False without such a part or for invalid names.
    bool linkComponent(const QString &uid,const QString &component);
    bool setPins(const QString &uid,const QStringList &pins);
    // Kennung and value from a project: "R4" is stored as the Kennung "R#" with the number 4. False if there is no
    // part with this identifier.
    bool setComponent(const QString &id,const QString &designator,const QString &value);
};
}
