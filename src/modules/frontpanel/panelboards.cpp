#include "panelboards.h"
#include "panelgeometry.h"

namespace openloch::frontpanel {
namespace {
void collect(const QList<Element> &list,QList<PanelComponent> &out){
    for(const auto &e:list){if(!e.component.isEmpty())out.append({e.component,e.id,componentPoint(e)});collect(e.children,out);}
}
// The elements from the outermost group down to the element with this id; empty when there is none.
bool pathTo(const QList<Element> &list,const QString &id,QList<const Element*> &path){
    for(const auto &e:list){path.append(&e);if(e.id==id||pathTo(e.children,id,path))return true;path.removeLast();}
    return false;
}
}
QList<PanelComponent> panelComponents(const Panel &panel){QList<PanelComponent> out;collect(panel.elements,out);return out;}
QString componentOf(const Panel &panel,const QString &element){
    QList<const Element*> path;if(!pathTo(panel.elements,element,path))return {};
    for(auto it=path.crbegin();it!=path.crend();++it)if(!(*it)->component.isEmpty())return (*it)->component;
    return {};
}
QPointF componentPoint(const Element &element){
    if(element.type==ElementType::Drill)return element.center;
    if(element.hasAnchor)return element.anchor;
    return elementBounds(element).center();
}
}
