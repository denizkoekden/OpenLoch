#pragma once
#include <optional>
#include <QByteArray>
#include <QHash>
#include <QPainterPath>
#include <QString>
#include <QStringList>
#include <memory>
#include <utility>

// Single-line fonts for engraving: AutoCAD shape fonts, compiled (.shx, "shapes", "unifont" and "bigfont") or as
// source text (.shp), and the ready-drawn fonts of FrontDesigner (.fhx). Their glyphs are strokes, not outlines, so that
// an engraver cuts each line once.
namespace openloch::frontpanel {
// An element of a glyph in FrontDesigner's own font format, as stored (font units, y downwards): a line, an arc
// (start, end, centre, radius, direction) or the pen lifted.
struct StrokeElement {
    enum Kind {Line=0,Arc=1,PenUp=2};
    int kind=Line;
    double x1=0,y1=0,x2=0,y2=0,cx=0,cy=0,radius=0;
    bool counterClockwise=false;
};
struct StrokeGlyph {
    QPainterPath path;
    double advance=0;
    QList<StrokeElement> elements;  // fonts in FrontDesigner's own format only
};
struct StrokeFont {
    QString name;
    int above=0,below=0;            // capital height above and descender depth below the base line, in font units
    bool unicode=false;
    // Big fonts (East Asian): characters numbered by two bytes, the first one in one of the escape ranges; extended big
    // fonts draw parts of characters into boxes.
    bool bigfont=false,extended=false;
    QList<std::pair<int,int>> escapes;
    QHash<int,QByteArray> shapes;   // shape number → its specification bytes (without the name)
    QHash<int,StrokeGlyph> glyphs;  // character code → strokes (fonts in FrontDesigner's own format)
    QByteArray source;              // the source text of a shape font (.shp), also kept with a compiled one
    bool dosOrder=true;             // shape fonts: letters numbered in the DOS code page (see shapeNumberFor)
    // FrontDesigner's letters made of the shapes, once they are plotted (fdshapes.h).
    mutable std::shared_ptr<const QHash<int,StrokeGlyph>> letters;
    bool isNull() const{return shapes.isEmpty()&&glyphs.isEmpty();}
    // The strokes of a text in font units, y upwards, base line at 0; `advance` receives the width.
    QPainterPath text(const QString &text,double *advance=nullptr) const;
    // Throw FormatError for anything that is not a shape font.
    static StrokeFont fromShx(const QByteArray &data);
    static StrokeFont fromShp(const QByteArray &text);
    static StrokeFont fromFhx(const QByteArray &data);
    static StrokeFont load(const QString &path);
};
// The shape that stands for a Windows-1252 code in a shape font: FrontDesigner takes the font to be numbered in the DOS
// code page and moves 40 of its letters to their places in Windows-1252; without `dosOrder` the code is the number.
int shapeNumberFor(int code,bool dosOrder);
// Whether installed shape fonts count as numbered in the DOS code page, as FrontDesigner takes them (the default).
bool strokeFontsInDosOrder();
void setStrokeFontsInDosOrder(bool on);
// A shape font may be numbered otherwise than the others: its own order by its name (any case), nullopt where it
// follows the setting for all.
std::optional<bool> ownStrokeFontOrder(const QString &name);
bool strokeFontInDosOrder(const QString &name);
void setStrokeFontInDosOrder(const QString &name,std::optional<bool> dos);
// The installed shape fonts, compiled or as source (FrontDesigner's own files hold their letters in Windows order).
QStringList shapeFontNames();
// Where an installed shape font keeps the German umlauts: at their numbers in the DOS code page (true), at their
// Windows-1252 codes (false), or not clearly at either (nullopt). A hint for choosing its order.
std::optional<bool> umlautsInDosPlaces(const QString &name);
// The module's own single-line font ("Normschrift"), drawn after the rules of technical lettering. It is listed with the
// installed fonts and stands in for stroke fonts that are not installed, so that such texts stay single lines.
const StrokeFont &ownStrokeFont();
// Folders searched for stroke fonts: the own folder (Dokumente/OpenLoch/Strichschriften) and those in the settings.
QString ownStrokeFontFolder();
QStringList strokeFontFolders();
void setStrokeFontFolders(const QStringList &folders);
// The font with this name (file name without ending, any case), read once; nullptr when there is none.
const StrokeFont *findStrokeFont(const QString &name);
// The font a stroke text is drawn with: the font of that name, else the own one; nullptr for an empty name.
const StrokeFont *strokeFontFor(const QString &name);
QStringList strokeFontNames();
// Forget the fonts read so far, after files or folders changed.
void forgetStrokeFonts();
}
