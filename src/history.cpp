#include "history.h"

namespace openloch {
void History::trim(QList<QByteArray> &states) const {
    qint64 bytes=0;for(const auto &state:states)bytes+=state.size();
    while(states.size()>steps||(bytes>32*1024*1024&&states.size()>1)){
        bytes-=states.first().size();states.removeFirst();
    }
}
void History::begin(Project &project){project.assignIds();pending=qCompress(project.encode());}
bool History::commit(Project &project) {
    if(pending.isEmpty())return false;
    project.assignIds();
    if(pending==qCompress(project.encode())){pending.clear();return false;}
    past.append(std::move(pending));pending.clear();trim(past);future.clear();return true;
}
bool History::undo(Project &project) {
    pending.clear();if(past.isEmpty())return false;
    auto restored=Project::decode(qUncompress(past.last()));
    future.append(qCompress(project.encode()));trim(future);past.removeLast();project=std::move(restored);return true;
}
bool History::redo(Project &project) {
    pending.clear();if(future.isEmpty())return false;
    auto restored=Project::decode(qUncompress(future.last()));
    past.append(qCompress(project.encode()));trim(past);future.removeLast();project=std::move(restored);return true;
}
void History::clear(){pending.clear();past.clear();future.clear();}
void History::setLimit(int n){steps=qBound(1,n,50);trim(past);trim(future);}
}
