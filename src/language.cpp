#include "language.h"
#include <QCoreApplication>
#include <QDate>
#include <QHash>
#include <QTime>
#include <QTranslator>
namespace openloch {
extern const char *const englishTexts[][2];
extern const int englishTextCount;
extern const char *const frenchTexts[][2];
extern const int frenchTextCount;
namespace {
QString language="de";
QSet<QString> &missing(){static QSet<QString> set;return set;}
QHash<QString,QString> load(const char *const texts[][2],int count){QHash<QString,QString> h;for(int i=0;i<count;i++)h.insert(QString::fromUtf8(texts[i][0]),QString::fromUtf8(texts[i][1]));return h;}
const QHash<QString,QString> &table(){static const QHash<QString,QString> texts=load(englishTexts,englishTextCount);return texts;}
const QHash<QString,QString> &frenchTable(){static const QHash<QString,QString> texts=load(frenchTexts,frenchTextCount);return texts;}
}
void setUiLanguage(const QString &chosen){language=chosen.startsWith("en")?"en":chosen.startsWith("fr")?"fr":"de";}
QString uiLanguage(){return language;}
bool englishUi(){return language=="en";}
QString ui(const char *german){return ui(QString::fromUtf8(german));}
// French texts come from their own table; texts it does not have yet are shown in English.
QString ui(const QString &german){
    if(language=="de"||german.isEmpty())return german;
    if(language=="fr"){const auto it=frenchTable().constFind(german);if(it!=frenchTable().constEnd())return *it;missing().insert(german);}
    const auto it=table().constFind(german);if(it!=table().constEnd())return *it;
    missing().insert(german);return german;
}
QLocale uiLocale(){return language=="en"?QLocale(QLocale::English,QLocale::UnitedStates):language=="fr"?QLocale(QLocale::French,QLocale::France):QLocale(QLocale::German,QLocale::Germany);}
// German Windows writes 08.10.2026 and 16:29:41 (the original's sample file shows it); for English the US format, for
// French the French one.
QString uiDate(const QDate &date){return date.toString(language=="en"?"M/d/yyyy":language=="fr"?"dd/MM/yyyy":"dd.MM.yyyy");}
QString uiTime(const QTime &time){return time.toString(language=="en"?"h:mm:ss AP":"HH:mm:ss");}
QSet<QString> missingTranslations(){return missing();}
QList<std::pair<QString,QString>> translationTable(){QList<std::pair<QString,QString>> out;for(int i=0;i<englishTextCount;i++)out.append({QString::fromUtf8(englishTexts[i][0]),QString::fromUtf8(englishTexts[i][1])});return out;}
namespace {
// The texts Qt asks for in its own contexts (its platform theme names the standard buttons), by their English source.
class StandardTexts : public QTranslator {
public:
    bool isEmpty() const override{return false;}
    QString translate(const char *context,const char *source,const char *,int) const override{
        const QByteArray from(context),text(source);
        if(from=="QMessageBox"){if(text=="Show Details...")return ui("Details einblenden…");if(text=="Hide Details...")return ui("Details ausblenden…");return {};}
        if(from!="QPlatformTheme"&&from!="QDialogButtonBox"&&from!="QCocoaTheme")return {};
        if(text=="OK")return ui("OK");
        if(text=="Save")return ui("Speichern");
        if(text=="Save All")return ui("Alle speichern");
        if(text=="Open")return ui("Öffnen");
        if(text=="&Yes")return ui("&Ja");
        if(text=="Yes to &All")return ui("Ja, &alle");
        if(text=="&No")return ui("&Nein");
        if(text=="N&o to All")return ui("N&ein, keine");
        if(text=="Abort"||text=="Cancel")return ui("Abbrechen");
        if(text=="Retry")return ui("Erneut versuchen");
        if(text=="Ignore")return ui("Ignorieren");
        if(text=="Close")return ui("Schließen");
        if(text=="Discard")return ui("Verwerfen");
        if(text=="Don't Save")return ui("Nicht speichern");
        if(text=="Help")return ui("Hilfe");
        if(text=="Apply")return ui("Übernehmen");
        if(text=="Reset")return ui("Zurücksetzen");
        if(text=="Restore Defaults")return ui("Standardwerte wiederherstellen");
        return {};
    }
};
}
void installStandardTexts(){static StandardTexts texts;QCoreApplication::installTranslator(&texts);}
}
