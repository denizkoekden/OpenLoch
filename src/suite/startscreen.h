#pragma once
#include <QWidget>
#include <functional>
class QStackedWidget;
class QTreeWidget;
namespace openloch::suite {
class Suite;
// The start screen: what one can make (one tile per kind of document), Öffnen… and the recently used documents.
// Files dropped on it open in their editor. It stands for the program while no document is open.
class StartScreen : public QWidget {
public:
    explicit StartScreen(Suite *suite);
    // Fills the list of recently used documents again.
    void refresh();
    // Called after the user closed the start screen.
    std::function<void()> closed;
protected:
    void closeEvent(QCloseEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;
private:
    Suite *suite;
    QTreeWidget *recent=nullptr;
    QStackedWidget *recentPages=nullptr;
    void openRecent(const QString &path);
    bool askToForget(const QString &path);
};
}
