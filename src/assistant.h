#pragma once
#include <QByteArray>
#include <QJsonObject>
#include <QList>
#include <QPointF>
#include <QStringList>
namespace openloch {
// LochMaster's "Objekt-Assistent": generated contours (styles 0-10) and lying components (11-14).
struct AssistantParameter {
    enum Type {Number,Integer,Text,Choice,Ohm};
    QString name,unit;
    Type type=Number;
    double value=0,minimum=0,maximum=0;  // Number and Integer in the shown unit (mm, degrees), Ohm in ohms
    QString text;                        // Text
    QStringList choices;int choice=0;    // Choice
};
QStringList assistantStyles();
// The parameter rows of a style with the original's defaults; `colours` replaces the list of body colours.
QList<AssistantParameter> assistantParameters(int style,const QStringList &colours={});
// The value shown in the table, and taking an edited text back like the original: out-of-range values are
// corrected to the nearer bound, an Ohm value outside its range always to the lower one; unreadable input is ignored.
enum class AssistantInput {Accepted,Corrected,Invalid};
QString assistantValueText(const AssistantParameter &parameter);
AssistantInput assistantSetValue(AssistantParameter &parameter,const QString &text);
QString formatOhm(double ohm);
double parseOhm(const QString &text);
// Resistor colour code: the nearest value of series 0-4 (E6, E12, E24, E48, E96) and the four bands as
// Delphi colours from the first band on, -1 for an invisible band.
double standardValue(double ohm,int series);
QList<int> colourBands(double ohm,int series);
// Body pictures: the colour names of a component style and OpenLoch's own gradient picture (BMP) for a name.
QStringList gradientNames(int style);
QByteArray gradientBitmap(int style,const QString &name);
// The generated object around `origin`: one outline for contours, a TBt component group otherwise.
// `bitmap` fills a component body, `back` places everything on the copper side.
QJsonObject assistantObject(int style,const QList<AssistantParameter> &parameters,const QByteArray &bitmap={},bool back=false,QPointF origin={15240,15240});
}
