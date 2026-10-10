#include "milling.h"
#include "copper.h"
#include "font.h"
#include "language.h"
#include <QByteArrayList>
#include <QPainterPath>
#include <QPainterPathStroker>
#include <QTransform>
#include <QtMath>
#include <algorithm>
#include <cmath>
#include <functional>
#include <map>
#include <vector>

namespace openloch::pcb {
namespace {
// Boolean operations run in hundredths of a millimetre, where Qt keeps curves round to a few micrometres.
constexpr double fine=100;
QPainterPath scaled(const QPainterPath &path,double factor){return path.isEmpty()?path:QTransform::fromScale(factor,factor).map(path);}
QPainterPath grown(const QPainterPath &shape,double distance){
    if(distance<=0)return shape;
    QPainterPathStroker s;s.setWidth(2*distance);s.setJoinStyle(Qt::RoundJoin);s.setCapStyle(Qt::RoundCap);
    return shape.united(s.createStroke(shape));
}
// The copper of a hatched area as made, in fine units, grown by `grow` (fine units): the lines of its grid with round
// ends and its border. A stroke with round ends and joins grown by a distance is the same stroke that much wider on
// each side, so the grid is never grown with path booleans. While the gaps are open, a cell between four whole lines
// with nothing else near (the outline, the boxes `near` of other copper in fine units) has a rectangle as its path:
// such paths come as `gaps` in millimetres, and in `shape` these cells are filled and the lines between them left out,
// so that path booleans only work along the outline and round other copper.
struct GridCopper {QPainterPath shape;QList<QPolygonF> gaps;};
GridCopper gridCopper(const Element &e,double grow,const QList<QRectF> &near){
    // The grid in the whole units hatchLines() lays it out in: lines at multiples of the pitch, from unit to unit.
    constexpr double unit=1e4;const qint64 p=std::llround(hatchSpacing(e)*unit);const double pitch=double(p)/unit,lw=hatchLineWidth(e),r=lw/2+grow/fine;
    struct Line {qint64 at,from,to;};QList<Line> lines[2];     // columns, rows
    for(const auto &l:hatchLines(e)){
        const bool column=l.x1()==l.x2();
        lines[column?0:1].append({std::llround((column?l.x1():l.y1())*unit/double(p)),std::llround((column?l.y1():l.x1())*unit),std::llround((column?l.y2():l.x2())*unit)});
    }
    // Cell (i, j) lies between columns i0+i and i0+i+1 and rows j0+j and j0+j+1; its gap is the cell less r on each side.
    qint64 i0=1,i1=0,j0=1,j1=0;
    for(const auto &l:lines[0]){if(i0>i1)i0=i1=l.at;i0=std::min(i0,l.at);i1=std::max(i1,l.at);}
    for(const auto &l:lines[1]){if(j0>j1)j0=j1=l.at;j0=std::min(j0,l.at);j1=std::max(j1,l.at);}
    const qint64 nx=i1-i0,ny=j1-j0;const bool openGaps=pitch-2*r>1e-6&&nx>0&&ny>0;
    auto floorDiv=[](qint64 a,qint64 b){return a>=0?a/b:-((b-1-a)/b);};
    std::vector<char> clean;
    if(openGaps){
        // Cells whose four sides lie on lines.
        std::vector<char> down(size_t((nx+1)*ny),0),across(size_t((ny+1)*nx),0);
        for(const auto &l:lines[0])for(qint64 j=std::max(j0,-floorDiv(-l.from,p));j<std::min(j1,floorDiv(l.to,p));j++)down[size_t((l.at-i0)*ny+j-j0)]=1;
        for(const auto &l:lines[1])for(qint64 i=std::max(i0,-floorDiv(-l.from,p));i<std::min(i1,floorDiv(l.to,p));i++)across[size_t((l.at-j0)*nx+i-i0)]=1;
        clean.assign(size_t(nx*ny),0);
        for(qint64 j=0;j<ny;j++)for(qint64 i=0;i<nx;i++)
            clean[size_t(j*nx+i)]=down[size_t(i*ny+j)]&&down[size_t((i+1)*ny+j)]&&across[size_t(j*nx+i)]&&across[size_t((j+1)*nx+i)];
        // The cells (from `base` on, `cells` of them) whose gaps reach into [lo, hi] along one axis, first and last.
        auto reaching=[&](double lo,double hi,qint64 base,qint64 cells){
            const double a=std::ceil((lo+r)/pitch-1-double(base)),b=std::floor((hi-r)/pitch-double(base));
            return std::pair<qint64,qint64>(!(a>0)?0:a>=double(cells)?cells:qint64(a),!(b<double(cells-1))?cells-1:b<0?-1:qint64(b));
        };
        // Not those near the outline, where the border grown and a plane cut by the area reach in, ...
        const double reach=std::max(0.0,e.width)/2+grow/fine+.001;const qsizetype m=e.points.size();
        for(qsizetype k=0;k<m;k++){
            const QPointF a=e.points[k],b=e.points[(k+1)%m];const auto rows=reaching(std::min(a.y(),b.y())-reach,std::max(a.y(),b.y())+reach,j0,ny);
            for(qint64 j=rows.first;j<=rows.second;j++){
                // The part of the edge level with the gaps of this row of cells, and the cells beside it.
                double t0=0,t1=1;
                if(a.y()!=b.y()){
                    t0=(double((j0+j)*p)/unit+r-reach-a.y())/(b.y()-a.y());t1=(double((j0+j+1)*p)/unit-r+reach-a.y())/(b.y()-a.y());
                    if(t0>t1)std::swap(t0,t1);t0=std::max(t0,0.0);t1=std::min(t1,1.0);if(t0>t1)continue;
                }
                const double x0=a.x()+t0*(b.x()-a.x()),x1=a.x()+t1*(b.x()-a.x());const auto cells=reaching(std::min(x0,x1)-reach,std::max(x0,x1)+reach,i0,nx);
                for(qint64 i=cells.first;i<=cells.second;i++)clean[size_t(j*nx+i)]=0;
            }
        }
        // ... nor those near other copper.
        for(const auto &q:near){
            const double g=grow/fine+.001;const auto cells=reaching(q.left()/fine-g,q.right()/fine+g,i0,nx),rows=reaching(q.top()/fine-g,q.bottom()/fine+g,j0,ny);
            for(qint64 j=rows.first;j<=rows.second;j++)for(qint64 i=cells.first;i<=cells.second;i++)clean[size_t(j*nx+i)]=0;
        }
    }
    auto isClean=[&](qint64 i,qint64 j){return i>=0&&j>=0&&i<nx&&j<ny&&clean[size_t(j*nx+i)];};
    // The lines, less the pieces between two clean cells where the line goes on beyond both ends of the piece.
    QPainterPath kept[2];
    for(int row=0;row<2;row++)for(const auto &l:lines[row]){
        const qint64 base=row?i0:j0,cells=row?nx:ny,line=l.at-(row?j0:i0);const double c=double(l.at*p)/unit*fine;
        auto piece=[&](qint64 from,qint64 to){
            const double a=double(from)/unit*fine,b=double(to)/unit*fine;if(row){kept[1].moveTo(a,c);kept[1].lineTo(b,c);}else{kept[0].moveTo(c,a);kept[0].lineTo(c,b);}
        };
        qint64 start=l.from;
        if(openGaps)for(qint64 k=std::max(base,-floorDiv(-l.from,p));k<std::min(base+cells,floorDiv(l.to,p));k++){
            const qint64 a=k*p,b=(k+1)*p;
            if(l.from<a&&b<l.to&&(row?isClean(k-base,line-1)&&isClean(k-base,line):isClean(line-1,k-base)&&isClean(line,k-base))){if(a>start)piece(start,a);start=b;}
        }
        if(l.to>start)piece(start,l.to);
    }
    GridCopper out;QPainterPathStroker s;s.setWidth(lw*fine+2*grow);s.setCapStyle(Qt::RoundCap);s.setJoinStyle(Qt::RoundJoin);
    out.shape=s.createStroke(kept[0]).simplified().united(s.createStroke(kept[1]).simplified());
    if(e.width>0){
        QPainterPath border;border.addPolygon(e.points);border.closeSubpath();s.setWidth(e.width*fine+2*grow);out.shape=out.shape.united(s.createStroke(scaled(border,fine)));
    }
    if(!openGaps)return out;
    // The clean cells, row by row in runs, each run reaching into the lines round it (which cover that); neighbouring
    // rows reach in by different distances, so that no edges of runs lie on each other.
    QPainterPath filled;filled.setFillRule(Qt::WindingFill);
    for(qint64 j=0;j<ny;j++){
        const double reachIn=j%2?r/4:r/2,y0=double((j0+j)*p)/unit,y1=double((j0+j+1)*p)/unit;
        for(qint64 i=0;i<nx;){
            if(!clean[size_t(j*nx+i)]){i++;continue;}
            qint64 k=i;
            for(;k<nx&&clean[size_t(j*nx+k)];k++){
                const double x0=double((i0+k)*p)/unit,x1=double((i0+k+1)*p)/unit;out.gaps.append(QPolygonF({{x0+r,y1-r},{x1-r,y1-r},{x1-r,y0+r},{x0+r,y0+r},{x0+r,y1-r}}));
            }
            filled.addRect(QRectF(QPointF(double((i0+i)*p)/unit-reachIn,y0-reachIn)*fine,QPointF(double((i0+k)*p)/unit+reachIn,y1+reachIn)*fine));i=k;
        }
    }
    if(!filled.isEmpty())out.shape=out.shape.united(filled);
    return out;
}
QString mm(double v){return uiLocale().toString(v,'f',2);}
QString from(int side){return side==CopperTop?ui("von oben"):ui("von unten");}
// Points and closed paths in a short order: always the nearest next, closed paths entered at their nearest corner.
template<typename T> QList<T> nearestFirst(QList<T> items,QPointF start,const std::function<QPointF(const T&)> &at){
    QList<T> out;QPointF here=start;
    while(!items.isEmpty()){
        qsizetype best=0;double d=1e300;
        for(qsizetype i=0;i<items.size();i++){const QPointF p=at(items[i]);const double e=std::hypot(p.x()-here.x(),p.y()-here.y());if(e<d){d=e;best=i;}}
        out.append(items.takeAt(best));here=at(out.last());
    }
    return out;
}
// Paths in the same order: open ones entered at either end (and then reversed), closed ones at their nearest corner;
// at equal distances the path listed first and its first corner (the start of an open path before its end) win. A grid
// of buckets over the places to enter finds the nearest without looking at every path each time.
QList<QPolygonF> orderedPaths(const QList<QPolygonF> &paths,QPointF start){
    const qsizetype n=paths.size();QList<QPolygonF> out;out.reserve(n);
    // The places to enter, path by path: the corners of a closed path, or the start and the end (corner −1) of an open one.
    struct Place {QPointF at;qsizetype path,corner;};std::vector<Place> places;std::vector<qsizetype> placesOf(n+1);
    for(qsizetype i=0;i<n;i++){
        const auto &p=paths[i];placesOf[i]=qsizetype(places.size());if(p.isEmpty())continue;
        if(p.size()>2&&p.first()==p.last())for(qsizetype k=0;k+1<p.size();k++)places.push_back({p[k],i,k});
        else{places.push_back({p.first(),i,0});places.push_back({p.last(),i,-1});}
    }
    placesOf[n]=qsizetype(places.size());
    auto finite=[](QPointF q){return std::isfinite(q.x())&&std::isfinite(q.y());};
    // Square buckets of about two places each over the places with finite coordinates; the others are never the nearest.
    double left=0,top=0,right=-1,bottom=-1;qsizetype count=0;
    for(const auto &q:places)if(finite(q.at)){if(!count++){left=right=q.at.x();top=bottom=q.at.y();}
        left=std::min(left,q.at.x());right=std::max(right,q.at.x());top=std::min(top,q.at.y());bottom=std::max(bottom,q.at.y());}
    const double w=right-left,h=bottom-top;double size=std::max(std::sqrt(2*w*h/std::max<qsizetype>(1,count)),2*std::max(w,h)/std::max<qsizetype>(1,count));
    if(!(size>0&&std::isfinite(size)))size=1;
    const qint64 nx=std::min<qint64>(qint64(w/size)+1,count+1),ny=std::min<qint64>(qint64(h/size)+1,count+1);
    auto cellOf=[&](double v,double low,qint64 cells){const double c=std::floor((v-low)/size);return c<=0?qint64(0):c>=double(cells-1)?cells-1:qint64(c);};
    // Each bucket a run of entries; the live places of a bucket lead its run, so that a place leaves in constant time.
    std::vector<qint64> cell(places.size(),-1);std::vector<qsizetype> run(size_t(nx*ny)+1,0),live(size_t(nx*ny),0),entries(places.size()),slot(places.size());
    for(size_t k=0;k<places.size();k++)if(finite(places[k].at)){cell[k]=cellOf(places[k].at.y(),top,ny)*nx+cellOf(places[k].at.x(),left,nx);run[size_t(cell[k])+1]++;}
    for(size_t c=0;c<size_t(nx*ny);c++)run[c+1]+=run[c];
    for(size_t k=0;k<places.size();k++)if(cell[k]>=0){const size_t c=size_t(cell[k]);entries[size_t(run[c]+live[c])]=qsizetype(k);slot[k]=run[c]+live[c];live[c]++;}
    auto leave=[&](qsizetype k){
        const qint64 c=cell[size_t(k)];if(c<0)return;const qsizetype last=run[size_t(c)]+--live[size_t(c)],other=entries[size_t(last)];
        entries[size_t(slot[size_t(k)])]=other;slot[size_t(other)]=slot[size_t(k)];entries[size_t(last)]=k;slot[size_t(k)]=last;
    };
    std::vector<char> taken(size_t(n),0);qsizetype lowest=0;QPointF here=start;
    for(qsizetype done=0;done<n;done++){
        // The nearest place, ring by ring of buckets round the one of `here`, until no further ring can hold one as near.
        qsizetype best=-1;double d=1e300;
        auto better=[&](qsizetype k,double e){
            if(e<d)return true;if(e!=d||best<0)return false;const auto &p=places[size_t(k)],&q=places[size_t(best)];
            return p.path<q.path||(p.path==q.path&&(p.corner<0?n:p.corner)<(q.corner<0?n:q.corner));
        };
        if(finite(here)&&count){
            const qint64 cx=cellOf(here.x(),left,nx),cy=cellOf(here.y(),top,ny);
            for(qint64 r=0;;r++){
                if(cx-r<0&&cy-r<0&&cx+r>=nx&&cy+r>=ny)break;
                for(qint64 y=std::max<qint64>(0,cy-r);y<=std::min(ny-1,cy+r);y++){
                    const bool edge=y==cy-r||y==cy+r;
                    for(qint64 x=edge?std::max<qint64>(0,cx-r):cx-r;x<=(edge?std::min(nx-1,cx+r):cx+r);x+=edge||r==0?1:2*r){
                        if(x<0||x>=nx)continue;const size_t c=size_t(y*nx+x);
                        for(qsizetype s=run[c];s<run[c]+live[c];s++){
                            const auto k=entries[size_t(s)];const QPointF p=places[size_t(k)].at;const double e=std::hypot(p.x()-here.x(),p.y()-here.y());
                            if(better(k,e)){d=e;best=k;}
                        }
                    }
                }
                if(best>=0&&d+1e-6+1e-9*d<double(r)*size)break;
            }
        }
        // Nothing nearer than 1e300 (no finite places left): the first path left, as it is.
        while(lowest<n&&taken[size_t(lowest)])lowest++;
        const qsizetype i=best>=0?places[size_t(best)].path:lowest,corner=best>=0?places[size_t(best)].corner:0;
        taken[size_t(i)]=1;for(qsizetype k=placesOf[i];k<placesOf[i+1];k++)leave(k);
        QPolygonF p=paths[i];
        if(corner<0)std::reverse(p.begin(),p.end());
        else if(corner>0){QPolygonF turned;for(qsizetype k=0;k<p.size()-1;k++)turned.append(p[(corner+k)%(p.size()-1)]);turned.append(turned.first());p=turned;}
        out.append(p);if(!p.isEmpty())here=p.last();
    }
    return out;
}
// The outline of a circle element, as its centre line.
QPolygonF circleLine(const Element &e){
    const double span=[&]{double s=std::fmod(e.stop-e.start,360.0);if(s<0)s+=360;return s<1e-9?360.0:s;}();
    const int steps=std::max(8,int(std::ceil(span/2)));QPolygonF out;
    for(int k=0;k<=steps;k++){const double a=qDegreesToRadians(e.start+span*k/steps);out.append(e.pos+QPointF(e.size*std::cos(a),-e.size*std::sin(a)));}
    if(span>=360)out.last()=out.first();
    return out;
}
QList<QPointF> holes(const Board &b,const std::function<bool(int)> &chosen){
    QList<QPointF> out;for(int i=0;i<b.elements.size();i++){const auto &e=b.elements[i];if(e.type==ElementType::Pad&&e.size2>0&&chosen(i))out.append(e.pos);}return out;
}
}

QList<MillingJob> millingJobs(const Board &b,const MillingSettings &s,const QList<int> &selection,bool withPaths,const std::function<void(int,int)> &progress){
    QList<MillingJob> jobs;
    // The paths of both sides are the long part of the work: each counts as a step.
    const int steps=std::max(1,(s.top+s.bottom)*std::max(1,s.passes));int done=0;
    auto chosen=[&](int i){return !s.onlySelected||selection.contains(i);};
    auto at=[](const QPointF &p){return p;};
    bool first=true;
    for(int side:{CopperTop,CopperBottom}){
        const bool top=side==CopperTop,mill=top?s.top:s.bottom,drill=s.drillSide==(top?1:2),contour=s.contourSide==(top?1:2);
        if(!mill&&!drill&&!contour)continue;
        // The paths and holes of the side in an order that starts where the machine's zero lies.
        const QPointF start=outputFrame(b,top?s.topMirror:s.bottomMirror,s.fromOrigin).zero();
        // Registration holes once, from the side milled first.
        if(first&&s.registrationCorners){
            MillingJob j;j.kind=MillingJobKind::Registration;j.side=side;j.tool=s.registrationDiameter;
            j.name=ui("Passbohrungen %1, Ø %2 mm").arg(from(side),mm(s.registrationDiameter));
            const double d=s.registrationDistance;
            const QList<QPointF> places{{-d,-d},{b.width+d,-d},{-d,b.height+d},{b.width+d,b.height+d},{b.width/2,-d},{b.width+d,b.height/2},{b.width/2,b.height+d},{-d,b.height/2}};
            for(int k=0;k<8;k++)if(s.registrationCorners&(1<<k))j.plunges.append(places[k]);
            jobs.append(j);
        }
        first=false;
        if(mill){
            MillingJob j;j.kind=MillingJobKind::Isolation;j.side=side;j.tool=s.toolWidth;
            j.name=ui("Isolationsfräsen %1, Fräser %2 mm").arg(layerName(side),mm(s.toolWidth));
            if(s.passes>1)j.name+=ui(", %1 Spuren").arg(s.passes);
            if(withPaths){
                // The copper of the side as one shape (texts milled as single lines left out), grown by the cutter's radius
                // for the first path and by a cutter width less the overlap for each further one. Hatched areas join it
                // per path as made, their lines and border grown the same way: the cutter goes round their lines into
                // the gaps.
                QPainterPath copper;copper.setFillRule(Qt::WindingFill);QList<QPolygonF> lines;QList<const Element*> hatched;
                // A box round the copper of each element, in which a hatched area has other copper near its cells: grown by
                // the clearance, as far as thermal spokes of a plane reach, and for hatched areas by the ends of their lines.
                QList<QRectF> boxes,hatchedBoxes;
                for(int i=0;i<b.elements.size();i++){
                    const auto &e=b.elements[i];if(!chosen(i))continue;const auto shape=copperOn(e,side,b);if(shape.isEmpty())continue;
                    if(e.type==ElementType::Text&&(selection.contains(i)?s.selectedTexts:s.texts)==1){
                        const auto strokes=e.strokes.isEmpty()?textStrokes(e.text,e.pos,e.size,e.style,e.thickness,e.rotation,e.mirrored):e.strokes;
                        for(const auto &stroke:strokes)if(!stroke.isEmpty())lines.append(stroke);
                        continue;
                    }
                    const auto fineShape=scaled(shape,fine);
                    if(e.type==ElementType::Area&&e.hatched){
                        const double l=hatchLineWidth(e)/2*fine;hatched.append(&e);hatchedBoxes.append(fineShape.boundingRect().adjusted(-l,-l,l,l));continue;
                    }
                    const double c=std::max(0.0,e.clearance)*fine;copper.addPath(fineShape);boxes.append(fineShape.boundingRect().adjusted(-c,-c,c,c));
                }
                copper=copper.simplified();
                if(b.groundPlane[side]&&copperLayers(b).contains(side)&&!s.onlySelected)copper=copper.united(scaled(groundPlane(b,side),fine));
                QList<QPolygonF> paths;
                const double step=std::max(.001,s.toolWidth*(1-std::clamp(s.overlap,0.0,99.0)/100));
                for(int k=0;k<std::max(1,s.passes)&&(!copper.isEmpty()||!hatched.isEmpty());k++){
                    if(progress)progress(done++,steps);
                    const double grow=(s.toolWidth/2+k*step)*fine;QList<QPainterPath> shapes{grown(copper,grow)};QList<QPolygonF> gaps;
                    for(qsizetype h=0;h<hatched.size();h++){
                        QList<QRectF> near=boxes;for(qsizetype o=0;o<hatched.size();o++)if(o!=h)near.append(hatchedBoxes[o]);
                        const auto grid=gridCopper(*hatched[h],grow,near);shapes.append(grid.shape);gaps+=grid.gaps;
                    }
                    // United in pairs, so that each union works on shapes of about the same size.
                    while(shapes.size()>1){
                        QList<QPainterPath> next;for(qsizetype n=0;n+1<shapes.size();n+=2)next.append(shapes[n].united(shapes[n+1]));
                        if(shapes.size()%2)next.append(shapes.last());shapes=next;
                    }
                    for(auto p:shapes.first().toSubpathPolygons()){
                        // Slivers the boolean operations leave (well below a hundredth of a millimetre) are nothing to mill.
                        const QRectF r=p.boundingRect();if(p.size()<2||std::max(r.width(),r.height())<1)continue;
                        if(p.first()!=p.last())p.append(p.first());
                        paths.append(QTransform::fromScale(1/fine,1/fine).map(p));
                    }
                    paths+=gaps;
                }
                j.paths=orderedPaths(paths+lines,start);
                if(top?s.punchTop:s.punchBottom)j.plunges=nearestFirst<QPointF>(holes(b,chosen),j.paths.isEmpty()?start:j.paths.last().last(),at);
            }
            jobs.append(j);
        }
        if(drill){
            QList<std::pair<QPointF,double>> all;
            for(int i=0;i<b.elements.size();i++){const auto &e=b.elements[i];if(e.type==ElementType::Pad&&e.size2>0&&chosen(i))all.append({e.pos,e.size2});}
            if(s.drillMode==0){
                MillingJob j;j.kind=MillingJobKind::Drill;j.side=side;j.tool=s.drillToolWidth;
                j.name=ui("Bohrungen %1 fräsen, Fräser %2 mm").arg(from(side),mm(s.drillToolWidth));
                QList<QPointF> plunges;QList<std::pair<QPointF,double>> circles;
                for(const auto &[p,d]:all){
                    const double r=(d-s.drillToolWidth)/2;
                    if(r<-1e-9)j.skipped++;else if(r<1e-6)plunges.append(p);else circles.append({p,r});
                }
                j.plunges=nearestFirst<QPointF>(plunges,start,at);
                j.circles=nearestFirst<std::pair<QPointF,double>>(circles,start,[](const std::pair<QPointF,double> &c){return c.first;});
                jobs.append(j);
            }else if(s.drillMode==1){
                MillingJob j;j.kind=MillingJobKind::Drill;j.side=side;j.name=ui("Bohrungen %1").arg(from(side));
                QList<QPointF> plunges;for(const auto &h:all)plunges.append(h.first);j.plunges=nearestFirst<QPointF>(plunges,start,at);
                jobs.append(j);
            }else{
                std::map<qint64,QList<QPointF>> sizes;for(const auto &[p,d]:all)sizes[std::llround(d*1000)].append(p);
                for(const auto &[size,points]:sizes){
                    MillingJob j;j.kind=MillingJobKind::Drill;j.side=side;j.tool=size/1000.0;
                    j.name=ui("Bohrungen %1, Ø %2 mm").arg(from(side),mm(j.tool));j.plunges=nearestFirst<QPointF>(points,start,at);jobs.append(j);
                }
            }
        }
        if(contour){
            MillingJob j;j.kind=MillingJobKind::Contour;j.side=side;j.name=ui("Platinenkontur %1").arg(from(side));
            QList<QPolygonF> paths;
            for(int i=0;i<b.elements.size();i++){
                const auto &e=b.elements[i];if(e.layer!=Outline||!chosen(i))continue;
                if(e.type==ElementType::Track&&e.points.size()>1)paths.append(e.points);
                else if(e.type==ElementType::Circle)paths.append(circleLine(e));
            }
            j.paths=orderedPaths(paths,start);jobs.append(j);
        }
    }
    return jobs;
}
namespace {
QByteArray hpglWith(const Board &b,const QList<std::pair<MillingJob,int>> &jobs,const MillingSettings &s){
    const double perMm=1/(s.roundedScale?.025:.0254);
    QByteArray out="IN;\nPA;\n";
    for(const auto &[job,pen]:jobs){
        const OutputFrame frame=outputFrame(b,job.side==CopperTop?s.topMirror:s.bottomMirror,s.fromOrigin);
        auto units=[&](QPointF p){const QPointF q=frame.map(p)*perMm;return std::pair<qint64,qint64>(std::llround(q.x()),std::llround(q.y()));};
        auto xy=[&](QPointF p){const auto [x,y]=units(p);return QByteArray::number(x)+","+QByteArray::number(y);};
        out+="SP"+QByteArray::number(pen)+";\n";
        for(const auto &path:job.paths){
            if(path.isEmpty())continue;out+="PU"+xy(path[0])+";\n";
            // A few points per line, so that every program reads them.
            for(qsizetype k=1;k<path.size();k+=8){
                QByteArrayList points;for(qsizetype n=k;n<std::min(path.size(),k+8);n++)points.append(xy(path[n]));
                out+="PD"+points.join(',')+";\n";
            }
            if(path.size()==1)out+="PD;\n";
            out+="PU;\n";
        }
        for(const auto &p:job.plunges){
            out+="PU"+xy(p)+";\nPD;\n";
            if(s.minimalFeed){const auto [x,y]=units(p);out+="PA"+QByteArray::number(x+1)+","+QByteArray::number(y)+";\n";}
            out+="PU;\n";
        }
        for(const auto &[centre,radius]:job.circles)out+="PU"+xy(centre)+";\nCI"+QByteArray::number(std::llround(radius*perMm))+";\n";
    }
    return out+"PU;\nSP0;\n";
}
}
QByteArray hpgl(const Board &b,const QList<MillingJob> &jobs,const MillingSettings &s,int firstPen){
    QList<std::pair<MillingJob,int>> withPens;int pen=firstPen;for(const auto &j:jobs)withPens.append({j,pen++});return hpglWith(b,withPens,s);
}
QList<std::pair<QString,QByteArray>> millingFiles(const Board &b,const QList<MillingJob> &jobs,const MillingSettings &s,const QString &base,const QString &suffix){
    if(!s.separateFiles)return {{base+QStringLiteral(".")+suffix,hpgl(b,jobs,s)}};
    // A file per job with its pen number; the registration holes first in every other file as its zero point, if wanted.
    int registration=-1;for(int k=0;k<jobs.size();k++)if(jobs[k].kind==MillingJobKind::Registration){registration=k;break;}
    QList<std::pair<QString,QByteArray>> out;
    for(int k=0;k<jobs.size();k++){
        QList<std::pair<MillingJob,int>> these;
        if(s.registrationEveryFile&&registration>=0&&k!=registration)these.append({jobs[registration],registration+1});
        these.append({jobs[k],k+1});out.append({QStringLiteral("%1_%2.%3").arg(base).arg(k+1).arg(suffix),hpglWith(b,these,s)});
    }
    return out;
}
QString millingJobList(const QList<MillingJob> &jobs){
    QStringList lines;
    for(int k=0;k<jobs.size();k++){
        const auto &j=jobs[k];QString line=QStringLiteral("SP%1  %2").arg(k+1).arg(j.name);
        const int count=int(j.plunges.size()+j.circles.size());
        if(j.kind==MillingJobKind::Drill||j.kind==MillingJobKind::Registration)line+=ui(" (%1 Bohrungen)").arg(count);
        if(j.skipped)line+=ui(", %1 kleiner als der Fräser ausgelassen").arg(j.skipped);
        lines.append(line);
    }
    return lines.join('\n')+'\n';
}
}
