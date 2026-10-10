#pragma once
#include "model.h"

namespace openloch::schematic {
// An asymmetric symbol with three numbered contacts, in local coordinates around its insertion point (contact 1):
// a body with a mark next to contact 1, contact 2 on the far side and contact 3 below, off the middle.
Item exampleSymbol();
// A document with two sheets of different sizes and a simple title block; on the first sheet the example symbol
// twice ("U1" and "U2"), a conductor from contact 2 of U1 to contact 1 of U2 and a text.
Document exampleDocument();
// How "Formblatt generieren" labels the frame, as in sPlan: each side without labels ("---"), with numbers ("NUM") or
// with letters ("CHAR"), in a font and height; numbering starts at `columnStart` and `rowStart` (1 = "1" or "A").
// `field` adds OpenLoch's field with the sheet's name and number and the file.
enum class FrameLabels {None,Numbers,Letters};
struct TitleBlockStyle {
    FrameLabels top=FrameLabels::Numbers,bottom=FrameLabels::None,left=FrameLabels::Letters,right=FrameLabels::None;
    QString font=QStringLiteral("Arial");
    double textHeight=4;            // mm (sPlan asks for 1/10 mm, 40 by default)
    int columnStart=1,rowStart=1;
    bool field=true;
};
// A title block in `frame`: with columns or rows a strip inside the frame on each labelled side carries the column or
// row labels between division marks. Its grid is numbered from the style's starts.
TitleBlock generateTitleBlock(QRectF frame,int columns,int rows,const TitleBlockStyle &style);
// The same with a frame `margin` inside the sheet, numbers above and below, letters left and right, 2.5 mm text.
TitleBlock generateTitleBlock(double width,double height,double margin,int columns,int rows,bool field);
// The frame 10 mm inside the sheet and the field.
TitleBlock simpleTitleBlock(double width,double height);
}
