#pragma once
#include <QMainWindow>
#include <functional>
namespace openloch::pcb {
class Editor;
}
namespace openloch::suite {
// The PCB editor (module "Leiterplatte", openloch::pcb::Editor) in a window of its own, with the module's menu table
// (pcb::editorMenus, shared with the module's own program) and its toolbar.
class PcbWindow : public QMainWindow {
public:
    explicit PcbWindow(QWidget *parent=nullptr);
    pcb::Editor *editor() const{return pcb;}
    // The file this window was opened from. Sprint-Layout files keep no path in the editor (saving writes the own
    // format), so the suite remembers it here to find the window again.
    QString source;
    // The file shown: the editor's own-format file, else the opened Sprint-Layout file while the editor still shows it.
    QString documentPath() const;
    // Called after the window has set its title, so that a host can show its own.
    std::function<void()> titleChanged;
protected:
    void closeEvent(QCloseEvent *event) override;
private:
    pcb::Editor *pcb=nullptr;
};
}
