#include "printing.h"
#include "language.h"
#include <QJsonArray>
#include <QSettings>
#include <QSysInfo>
#include <cmath>
namespace openloch {
namespace {
// Delphi's Round: to the nearest integer, halves to the even one (the default rounding mode).
int round(double v){return int(std::nearbyint(v));}
}
PrintView defaultPrintView(int index){
    PrintView v;v.centred=index==0;v.posX=v.posY=-1000*index;return v;
}
PrintSetup defaultPrintSetup(){PrintSetup s;for(int i=0;i<10;i++)s.views[i]=defaultPrintView(i);return s;}
PrintSetup printSetupOf(const QJsonObject &document){
    PrintSetup s=defaultPrintSetup();
    if(document.contains("view_index"))s.count=qBound(1,document["view_index"].toInt(1),10);
    const auto flags=document["view_flags"].toArray();
    if(flags.size()==4){s.onlyOne=flags[0].toBool();s.cutMarks=flags[1].toBool();s.landscape=flags[2].toBool();s.dataField=flags[3].toBool();}
    if(document.contains("view_integer"))s.sheet=qMax(1,document["view_integer"].toInt(1));
    const auto views=document["views"].toArray();
    for(int i=0;i<10&&i+1<views.size();i++){
        const auto o=views[i+1].toObject();auto &v=s.views[i];
        const auto f=o["flags"].toArray();if(f.size()==7){v.mono=f[0].toBool();v.flip=f[1].toBool();v.bitmaps=f[2].toBool();v.xray=f[3].toBool();v.through=f[4].toBool();v.free=f[5].toBool();v.potentials=f[6].toBool();}
        const auto g=o["flags2"].toArray();
        if(g.size()==10){v.holes=g[0].toBool();v.cuts=g[1].toBool();v.objects=g[2].toBool();v.rulers=g[3].toBool();v.spare=g[4].toBool();v.background=g[5].toBool();v.original=g[6].toBool();v.centred=g[7].toBool();v.copper=g[8].toBool();v.solderMarks=g[9].toBool();}
        const auto b=o["bounds"].toArray();if(b.size()==4){v.tilesX=qBound(1,b[2].toInt(1),10);v.tilesY=qBound(1,b[3].toInt(1),10);}
        v.gapX=o["number1"].toDouble();v.gapY=o["number2"].toDouble();v.scale=o["number3"].toDouble(1);if(!(v.scale>0))v.scale=1;v.zoom=o["scale"].toDouble(1);
        const auto n=o["integers"].toArray();if(n.size()==3){v.posX=n[0].toInt();v.posY=n[1].toInt();v.unit=qBound(0,n[2].toInt(),2);}
        v.texts=o["flag"].toBool(true);
    }
    return s;
}
void storePrintSetup(QJsonObject &document,const PrintSetup &s){
    document["view_index"]=s.count;document["view_flags"]=QJsonArray{s.onlyOne,s.cutMarks,s.landscape,s.dataField};document["view_integer"]=s.sheet;
    auto views=document["views"].toArray();
    // Record 0 is the main window's view; unknown fields of the print records (bounds 0 and 1) stay as they are.
    while(views.size()<11)views.append(QJsonObject{{"flags",QJsonArray{false,false,true,true,false,false,true}},{"scale",1},{"bounds",QJsonArray{0,0,1,1}},{"number1",0},{"number2",0},
        {"flags2",QJsonArray{true,true,true,true,true,true,true,true,true,true}},{"number3",1},{"integers",QJsonArray{0,0,2}},{"flag",true}});
    for(int i=0;i<10;i++){
        auto o=views[i+1].toObject();const auto &v=s.views[i];
        o["flags"]=QJsonArray{v.mono,v.flip,v.bitmaps,v.xray,v.through,v.free,v.potentials};
        o["flags2"]=QJsonArray{v.holes,v.cuts,v.objects,v.rulers,v.spare,v.background,v.original,v.centred,v.copper,v.solderMarks};
        auto b=o["bounds"].toArray();if(b.size()!=4)b={0,0,1,1};b[2]=v.tilesX;b[3]=v.tilesY;o["bounds"]=b;
        o["number1"]=v.gapX;o["number2"]=v.gapY;o["number3"]=v.scale;o["scale"]=v.zoom;o["integers"]=QJsonArray{v.posX,v.posY,v.unit};o["flag"]=v.texts;
        views[i+1]=o;
    }
    document["views"]=views;
}
PrintSetup printSetupOf(const Project &project){
    QJsonObject document;for(const auto key:{"views","view_index","view_flags","view_integer"})if(project.legacy.contains(key))document[key]=project.legacy[key];
    for(auto it=project.print.begin();it!=project.print.end();++it)document[it.key()]=it.value();
    return printSetupOf(document);
}
void storePrintSetup(Project &project,const PrintSetup &setup){
    QJsonObject document{{"views",project.print.contains("views")?project.print["views"]:project.legacy["views"]}};storePrintSetup(document,setup);project.print=document;
}
Paper paperOf(const QPageLayout &layout){
    Paper p;const QRectF full=layout.fullRect(QPageLayout::Millimeter),paint=layout.paintRect(QPageLayout::Millimeter);
    p.width=int(std::floor(paint.width()+1e-6));p.height=int(std::floor(paint.height()+1e-6));
    p.left=paint.left()-full.left();p.top=paint.top()-full.top();p.fullWidth=full.width();p.fullHeight=full.height();return p;
}
int usableHeight(const Paper &paper,const PrintSetup &setup){return paper.height-(setup.dataField?dataFieldHeight:0);}
QSize sheetCount(const PrintSetup &setup,QSizeF board,const Paper &paper){
    const double W=board.width(),H=board.height();const int PH=usableHeight(paper,setup);int nx=0,ny=0;
    if(paper.width<=0||PH<=0)return {1,1};
    for(int i=0;i<qBound(1,setup.count,10);i++){
        const auto &v=setup.views[i];
        double ex=W/100;if(!v.centred)ex-=v.posX/100.0;if(v.tilesX>1)ex+=(v.tilesX-1)*W/100+(v.tilesX-1)*v.gapX/100;
        double ey=H/100;if(!v.centred)ey-=v.posY/100.0;if(v.drawsRulers())ey+=5;if(v.tilesY>1)ey+=(v.tilesY-1)*(H+v.gapY)/100;
        nx=qMax(nx,int(std::trunc(ex*v.scale/paper.width))+1);ny=qMax(ny,int(std::trunc(ey*v.scale/PH))+1);
    }
    return {qMax(1,nx),qMax(1,ny)};
}
void centreView(PrintView &view,const PrintSetup &setup,QSizeF board,const Paper &paper){
    const QSize n=sheetCount(setup,board,paper);const double W=board.width(),H=board.height();
    const int totalW=round((W+view.gapX)*(view.tilesX-1)+W),totalH=round((H+view.gapY)*(view.tilesY-1)+H);
    view.posX=-round(paper.width*n.width()*100/view.scale/2)+totalW/2;
    view.posY=-round(usableHeight(paper,setup)*n.height()*100/view.scale/2)+totalH/2;
}
QTransform printTransform(const PrintView &v,QSizeF board,int tx,int ty){
    const double s=v.scale/100,dx=-v.posX+tx*(board.width()+v.gapX),dy=-v.posY+ty*(board.height()+v.gapY);
    if(v.flip)return QTransform(s,0,0,-s,dx*s,(board.height()+dy)*s);
    return QTransform(s,0,0,s,dx*s,dy*s);
}
QPointF viewPosition(const PrintView &v,const Paper &paper){return {paper.left-v.posX/100.0*v.scale,paper.top-v.posY/100.0*v.scale};}
void setViewPosition(PrintView &v,const Paper &paper,QPointF mm){v.posX=-round((mm.x()-paper.left)/v.scale*100);v.posY=-round((mm.y()-paper.top)/v.scale*100);}
QPoint sheetCell(int number,QSize sheets){const int n=qBound(1,number,qMax(1,sheets.width()*sheets.height()))-1;return {n%qMax(1,sheets.width()),n/qMax(1,sheets.width())};}
DataField dataField(const QString &project,const QString &board,QSizeF size,int unit,const QString &editor,const QDateTime &when,int sheet,int sheets){
    // The size in the main window's unit: inch, otherwise mm (also for N).
    const bool inch=unit==1;const double f=inch?2540:100;const auto locale=uiLocale();
    DataField d;
    d.left={ui("Projekt: ")+project+" ["+board+"]",ui("Abmessungen: ")+locale.toString(size.width()/f,'f',2)+" x "+locale.toString(size.height()/f,'f',2)+(inch?" inch":" mm"),ui("Bearbeiter: ")+editor};
    d.right={uiDate(when.date()),uiTime(when.time()),ui("Blatt ")+QString::number(sheet)+" / "+QString::number(sheets)};
    return d;
}
QString printEditor(){
#ifdef Q_OS_WIN
    const QSettings registry("HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion",QSettings::NativeFormat);
    return registry.value("RegisteredOwner").toString()+" ["+registry.value("RegisteredOrganization").toString()+"]";
#else
    QString user=qEnvironmentVariable("USER");if(user.isEmpty())user=qEnvironmentVariable("USERNAME");
    return user+" ["+QSysInfo::machineHostName()+"]";
#endif
}
std::optional<double> correctionFactor(const QString &text){
    bool ok=false;const double v=QString(text).replace(',','.').trimmed().toDouble(&ok);
    if(!ok||v<.8||v>1.2)return std::nullopt;return v;
}
}
