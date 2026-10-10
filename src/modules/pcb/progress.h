#pragma once
#include <QApplication>
#include <QProgressDialog>
#include <algorithm>
#include <functional>

namespace openloch::pcb {
// How long work reports its steps: `done` of `total`.
using ProgressReport=std::function<void(int done,int total)>;
// A progress window for long work, as the reference shows one: it appears only once the work takes longer than half a
// second, counts the steps the work reports and keeps the window it belongs to from input meanwhile.
class ProgressWindow {
public:
    ProgressWindow(QWidget *parent,const QString &label):dialog(label,QString(),0,1,parent){
        dialog.setObjectName("progress");dialog.setWindowModality(Qt::WindowModal);dialog.setMinimumDuration(500);dialog.setCancelButton(nullptr);
        dialog.setAutoClose(false);dialog.setAutoReset(false);dialog.setValue(0);
    }
    ProgressReport report(){
        return [this](int done,int total){
            total=std::max(1,total);if(dialog.maximum()!=total)dialog.setMaximum(total);dialog.setValue(std::clamp(done,0,total));steps++;
            QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);};
    }
    int reported() const{return steps;}
private:
    QProgressDialog dialog;
    int steps=0;
};
}
