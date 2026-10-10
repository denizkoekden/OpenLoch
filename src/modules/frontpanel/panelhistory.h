#pragma once
#include "frontpanel.h"
#include <QList>

namespace openloch::frontpanel {
// Undo and redo by snapshots of the whole document: begin() before a change, commit() after it. A change that leaves
// the document as it was adds no step; a drag with many mouse moves between begin() and commit() is one step.
class History {
public:
    void begin(const Document &document);
    bool commit(const Document &document);
    void cancel();
    bool undo(Document &document);
    bool redo(Document &document);
    bool canUndo() const{return !past.isEmpty();}
    bool canRedo() const{return !future.isEmpty();}
    bool pendingChange() const{return !pending.isEmpty();}
    void clear();
private:
    QByteArray pending;
    QList<QByteArray> past,future;
    static void trim(QList<QByteArray> &states);
};
}
