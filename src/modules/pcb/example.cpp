#include "example.h"
#include "font.h"
#include "footprints.h"
#include "language.h"

namespace openloch::pcb {
Document exampleDocument(){
    Document d;d.title=ui("Beispielplatine");d.author="OpenLoch";
    Board b=newBoard(ui("Beispiel"),80,50);b.grid=1.27;
    // Board outline on U
    auto outline=newElement(ElementType::Track);outline.layer=Outline;outline.width=.2;outline.points={{0,0},{80,0},{80,50},{0,50},{0,0}};b.elements<<outline;
    // Two through-hole pads, 1.8 mm with 0.8 mm drills, 7.62 mm apart; pad 1 square.
    auto pad1=newElement(ElementType::Pad);pad1.pos={15.24,20.32};pad1.size=1.8;pad1.size2=.8;pad1.shape=PadShape::Square;pad1.name="1";updateOutline(pad1);
    auto pad2=pad1;pad2.pos={22.86,20.32};pad2.shape=PadShape::Round;pad2.name="2";updateOutline(pad2);
    b.elements<<pad1<<pad2;
    // SMD pad on the top copper, 1.5 × 2 mm.
    auto smd=newElement(ElementType::SmdPad);smd.pos={50.8,20.32};smd.size=1.5;smd.size2=2;smd.name="3";updateOutline(smd);b.elements<<smd;
    // Via: through-plated, 1.2 mm with a 0.6 mm drill.
    auto via=newElement(ElementType::Pad);via.pos={38.1,30.48};via.size=1.2;via.size2=.6;via.via=true;updateOutline(via);b.elements<<via;
    // Bottom copper from pad 2 to the via, 0.8 mm; top copper from the via to the SMD pad, 0.5 mm.
    auto bottom=newElement(ElementType::Track);bottom.layer=CopperBottom;bottom.width=.8;bottom.points={{22.86,20.32},{27.94,20.32},{38.1,30.48}};b.elements<<bottom;
    auto top=newElement(ElementType::Track);top.layer=CopperTop;top.width=.5;top.points={{38.1,30.48},{50.8,30.48},{50.8,20.32}};b.elements<<top;
    // An airwire still to be routed: pad 1 to pad 2.
    b.elements[1].connections={2};b.elements[2].connections={1};
    // Asymmetric silkscreen mark (an "F" of tracks) and a text, so that mirroring shows.
    auto mark=newElement(ElementType::Track);mark.layer=SilkTop;mark.width=.3;mark.points={{8,44},{8,36},{12,36}};b.elements<<mark;
    auto bar=newElement(ElementType::Track);bar.layer=SilkTop;bar.width=.3;bar.points={{8,40},{11,40}};b.elements<<bar;
    auto text=newElement(ElementType::Text);text.layer=SilkTop;text.text="OpenLoch";text.pos={15,45};text.size=2.5;updateStrokes(text);b.elements<<text;
    // A component: resistor 0805 with designator and value.
    for(const auto &f:footprints())if(f.id=="chip-0805"){auto parts=placeable(f,b);for(auto &e:parts){move(e,{63.5,40.64});if(e.role==TextRole::Value){e.text="10k";updateStrokes(e);}}b.elements<<parts;}
    d.boards={b};return d;
}
}
