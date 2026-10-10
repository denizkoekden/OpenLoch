#pragma once
#include <QJsonObject>
#include <QList>
#include <QString>
#include <QStringList>
namespace openloch::documents {
// What a schematic asks of a board (docs/suite.md, "Soll-Verbindungen"): its components with their pins and its nets as
// pins of components, all named by the project's identifiers. It is derived from the schematic each time and never
// stored, so that there is no second truth.
struct TargetPin {
    QString component,pin;   // the component's identifier and the pin's identifier within it ("1", "E", "A")
    bool operator==(const TargetPin &) const=default;
};
struct TargetComponent {
    QString id,designator,value;   // designator as shown, e.g. "R1", or "2R1" with the sheet number
    QStringList pins;
    QString kind;                  // the kind of part ("R", "C", "IC", ...), independent of sheet numbers and prefixes
};
struct TargetNet {
    QString name;                  // empty for a net without a label
    QList<TargetPin> pins;
};
struct Targets {
    QList<TargetComponent> components;
    QList<TargetNet> nets;
    bool isEmpty() const{return components.isEmpty();}
    const TargetComponent *component(const QString &id) const;
    // {"components": [{"id", "designator", "value", "kind", "pins": ["1", "2"]}], "nets": [{"name", "pins": [["<id>", "1"]]}]}
    QJsonObject toJson() const;
    // Throws FormatError for anything malformed: unknown components or pins in a net, an identifier twice.
    static Targets fromJson(const QJsonObject &json);
};
}
