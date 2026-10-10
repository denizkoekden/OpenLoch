#include "panelhistory.h"

namespace openloch::frontpanel {
void History::trim(QList<QByteArray> &states){
    qint64 bytes=0;for(const auto &s:states)bytes+=s.size();
    while(states.size()>200||(bytes>64*1024*1024&&states.size()>1)){bytes-=states.first().size();states.removeFirst();}
}
void History::begin(const Document &document){pending=qCompress(document.encode());}
void History::cancel(){pending.clear();}
bool History::commit(const Document &document){
    if(pending.isEmpty())return false;
    if(pending==qCompress(document.encode())){pending.clear();return false;}
    past.append(std::move(pending));pending.clear();trim(past);future.clear();return true;
}
bool History::undo(Document &document){
    pending.clear();if(past.isEmpty())return false;
    auto restored=Document::decode(qUncompress(past.last()));
    future.append(qCompress(document.encode()));trim(future);past.removeLast();document=std::move(restored);return true;
}
bool History::redo(Document &document){
    pending.clear();if(future.isEmpty())return false;
    auto restored=Document::decode(qUncompress(future.last()));
    past.append(qCompress(document.encode()));trim(past);future.removeLast();document=std::move(restored);return true;
}
void History::clear(){pending.clear();past.clear();future.clear();}
}
