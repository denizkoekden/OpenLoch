#pragma once
#include "project.h"
#include <QList>

namespace openloch {
class History {
public:
    // Before a change and after it. Both first give the project's objects their identifiers (Project::assignIds), so
    // that every kept state has them and undo brings back exactly what was there.
    void begin(Project &project);
    bool commit(Project &project);
    bool undo(Project &project);
    bool redo(Project &project);
    void clear();
    // "Rückgängig-Aktionen": how many steps are kept (1 to 50 as in the original); older ones are dropped at once.
    void setLimit(int steps);
    int limit() const{return steps;}
private:
    QByteArray pending;
    QList<QByteArray> past,future;
    int steps=50;
    void trim(QList<QByteArray> &states) const;
};
}
