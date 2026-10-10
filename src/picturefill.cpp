#include "picturefill.h"
#include <QBuffer>
#include <QHash>
#include <QLineF>
#include <QPainter>
#include <cmath>
#include <functional>
namespace openloch {
namespace {
// Douglas–Peucker on an open chain, keeping the end points.
void simplify(const QList<QPointF> &points,int from,int to,double tolerance,QList<QPointF> &out){
    double worst=0;int at=-1;const QLineF chord(points[from],points[to]);const double length=chord.length();
    for(int i=from+1;i<to;i++){
        const QPointF p=points[i];
        const double d=length<1e-9?QLineF(points[from],p).length():std::abs((chord.dx())*(points[from].y()-p.y())-(points[from].x()-p.x())*chord.dy())/length;
        if(d>worst){worst=d;at=i;}
    }
    if(at>=0&&worst>tolerance){simplify(points,from,at,tolerance,out);simplify(points,at,to,tolerance,out);}
    else out.append(points[to]);
}
double area(const QList<QPointF> &p){double a=0;for(int i=0;i<p.size();i++){const auto &u=p[i],&v=p[(i+1)%p.size()];a+=u.x()*v.y()-v.x()*u.y();}return a/2;}
}
PictureFill pictureFill(const QImage &source){
    PictureFill fill;if(source.isNull())return fill;
    const QImage argb=source.convertToFormat(QImage::Format_ARGB32);
    QImage flat(argb.size(),QImage::Format_RGB888);flat.fill(Qt::white);{QPainter p(&flat);p.drawImage(0,0,argb);}
    QBuffer buffer(&fill.bmp);buffer.open(QIODevice::WriteOnly);flat.save(&buffer,"BMP");
    // The opaque part on a mask of at most 256 pixels per side, so that tracing stays quick for large pictures.
    const double s=qMin(1.0,256.0/qMax(argb.width(),argb.height()));
    const QImage small=s<1?argb.scaled(qMax(1,int(std::lround(argb.width()*s))),qMax(1,int(std::lround(argb.height()*s))),Qt::IgnoreAspectRatio,Qt::SmoothTransformation):argb;
    const int W=small.width(),H=small.height();QList<bool> opaque(W*H);int count=0;
    for(int y=0;y<H;y++){const auto *line=reinterpret_cast<const QRgb*>(small.constScanLine(y));for(int x=0;x<W;x++){opaque[y*W+x]=qAlpha(line[x])>=128;count+=opaque[y*W+x];}}
    if(count==0||count==W*H)return fill;
    // The largest 4-connected opaque part.
    QList<int> label(W*H,-1);int best=-1,bestSize=0;
    for(int start=0;start<W*H;start++){
        if(!opaque[start]||label[start]>=0)continue;QList<int> stack{start};label[start]=start;int size=0;
        while(!stack.isEmpty()){const int i=stack.takeLast();++size;const int x=i%W,y=i/W;
            for(const auto &[dx,dy]:{std::pair{1,0},{-1,0},{0,1},{0,-1}}){const int nx=x+dx,ny=y+dy;if(nx<0||ny<0||nx>=W||ny>=H)continue;const int n=ny*W+nx;if(opaque[n]&&label[n]<0){label[n]=start;stack.append(n);}}}
        if(size>bestSize){bestSize=size;best=start;}
    }
    auto inside=[&](int x,int y){return x>=0&&y>=0&&x<W&&y<H&&label[y*W+x]==best;};
    // Its boundary as pixel edges with the part on the right (clockwise on screen), joined into loops.
    QMultiHash<qint64,QPoint> edges;auto key=[&](QPoint p){return qint64(p.y())*(W+2)+p.x();};
    for(int y=0;y<H;y++)for(int x=0;x<W;x++){if(!inside(x,y))continue;
        if(!inside(x,y-1))edges.insert(key({x,y}),{x+1,y});if(!inside(x+1,y))edges.insert(key({x+1,y}),{x+1,y+1});
        if(!inside(x,y+1))edges.insert(key({x+1,y+1}),{x,y+1});if(!inside(x-1,y))edges.insert(key({x,y+1}),{x,y});}
    QList<QPointF> outer;double outerArea=0;
    while(!edges.isEmpty()){
        auto it=edges.begin();const QPoint first(int(it.key()%(W+2)),int(it.key()/(W+2)));QPoint at=first,next=it.value();edges.erase(it);QList<QPointF> loop{first};
        while(next!=first){
            loop.append(next);const QPoint dir=next-at;at=next;auto range=edges.equal_range(key(at));if(range.first==range.second)break;
            // Where the part touches itself at a corner, turn right first, so each loop stays simple.
            auto pick=range.first;for(auto e=range.first;e!=range.second;++e){const QPoint d=e.value()-at;if(dir.x()*d.y()-dir.y()*d.x()>0){pick=e;break;}}
            next=pick.value();edges.erase(pick);
        }
        if(std::abs(area(loop))>outerArea){outerArea=std::abs(area(loop));outer=loop;}
    }
    if(outer.size()<3)return fill;
    // Straight runs and pixel steps simplified to a polygon within 0.75 mask pixels.
    QList<QPointF> chain=outer;chain.append(outer.first());QList<QPointF> simple{chain.first()};
    int far=0;double farthest=0;for(int i=1;i<chain.size()-1;i++){const double d=QLineF(chain[0],chain[i]).length();if(d>farthest){farthest=d;far=i;}}
    simplify(chain,0,far,.75,simple);simplify(chain,far,chain.size()-1,.75,simple);simple.removeLast();
    for(const auto &p:simple)fill.outline<<QPointF(p.x()/W,p.y()/H);
    return fill;
}
}
