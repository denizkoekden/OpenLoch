#include "text.h"
#include <QFontMetricsF>

namespace openloch::schematic {
namespace {
constexpr int referenceSize=100;
}
QFont qtFont(const Font &font){
    QFont f(font.family);f.setPixelSize(referenceSize);f.setBold(font.bold);f.setItalic(font.italic);f.setUnderline(font.underline);f.setStrikeOut(font.strikeOut);
    // The same letter widths at every zoom.
    f.setHintingPreference(QFont::PreferNoHinting);f.setKerning(true);
    return f;
}
// The height of a line of text is that of the font's letters from ascent to descent, as the reference measures it.
double textScale(const Font &font){
    const QFontMetricsF metrics(qtFont(font));const double cell=metrics.ascent()+metrics.descent();
    return font.height/(cell>0?cell:referenceSize);
}
TextLayout layoutText(const QString &text,const Font &font){
    TextLayout out;const QFontMetricsF metrics(qtFont(font));const double scale=textScale(font);
    out.lines=text.split(u'\n');
    for(const auto &line:out.lines){const double w=metrics.horizontalAdvance(line)*scale;out.widths.append(w);out.width=std::max(out.width,w);}
    out.lineHeight=font.height;out.ascent=metrics.ascent()*scale;
    return out;
}
QTransform textFrame(const Item &text){
    QTransform t;t.translate(text.pos.x(),text.pos.y());t.rotate(-text.rotation);
    if(text.mirrored)t.scale(-1,1);
    return t;
}
QRectF textRect(const Item &text,const QString &shown){
    const auto layout=layoutText(shown.isEmpty()?QStringLiteral(" "):shown,text.font);
    const double w=layout.width,h=layout.lineHeight*layout.lines.size();
    const double left=text.align==Align::Left?0:text.align==Align::Centre?-w/2:-w;
    return QRectF(left,0,w,h);
}
QPolygonF textOutline(const Item &text,const QString &shown){
    return textFrame(text).map(QPolygonF(textRect(text,shown)));
}
}
