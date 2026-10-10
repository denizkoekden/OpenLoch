#pragma once
#include <QList>
#include <QLocale>
#include <QSet>
#include <QString>
#include <utility>
namespace openloch {
// Interface languages like the original's language dialog: German (the texts in the code are the German ones), English
// and French from tables. The language is chosen before the first window is built.
void setUiLanguage(const QString &language);  // "de", "en" or "fr"
QString uiLanguage();
bool englishUi();
// The text in the interface language, looked up by its German text. Translate a format string before .arg().
QString ui(const char *german);
QString ui(const QString &german);
// Numbers and dates the way the original writes them on a system of that language (decimal comma in German).
QLocale uiLocale();
QString uiDate(const QDate &date);
QString uiTime(const QTime &time);
// Qt's own texts of standard buttons and message boxes ("OK", "Cancel", "&Yes", "Show Details...") in the interface
// language, through the tables of ui(); dialogs opened after a change of language follow it. main() installs it once.
void installStandardTexts();
// Texts asked for without a translation in the current language, and the English table, for the completeness tests.
QSet<QString> missingTranslations();
QList<std::pair<QString,QString>> translationTable();
}
