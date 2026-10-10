#pragma once
#include <QByteArray>
#include <QString>
#include <QtEndian>
#include <cmath>
#include <exception>
#include "board.h"
#include "legacy_writer.h"

// Original test data, written from the documented record layout. No vendor assets.
namespace fixtures {
// Every LM4 file the program writes is written once more from the own model (board.h), also after a round through the
// model's format, and must come out byte for byte the same.
inline void checkOwnModel(){
    openloch::setLegacyWriteCheck([](const openloch::Project &project,const QByteArray &bytes){
        try{
            const auto model=openloch::BoardDocument::fromProject(project);
            if(model.writeLm4()!=bytes)qFatal("the own model wrote another LM4 file for \"%s\"",qPrintable(project.title));
            if(openloch::BoardDocument::decode(model.encode()).writeLm4()!=bytes)qFatal("the own model's format lost something of \"%s\"",qPrintable(project.title));
        }catch(const std::exception &e){qFatal("the own model failed for \"%s\": %s",qPrintable(project.title),e.what());}
    });
}
class Stream {
public:
    QByteArray bytes;
    QByteArray bitmap,notes;
    int pinY=1016;bool dualPin=false;
    void byte(quint8 v){bytes.append(char(v));}
    template<class T> void rawNumber(T v){char b[sizeof(T)];qToLittleEndian(v,b);bytes.append(b,sizeof(T));}
    void integer(qint32 v){byte(4);rawNumber(v);}
    void boolean(bool v){byte(v?9:8);}
    void number(double v){
        byte(5);int exponent=0;double fraction=std::frexp(std::abs(v),&exponent);
        rawNumber<quint64>(quint64(std::ldexp(fraction,64)));
        rawNumber<quint16>((v?exponent-1+16383:0)|(std::signbit(v)?0x8000:0));
    }
    void string(const QString &v){auto b=v.toUtf8();byte(20);rawNumber<qint32>(b.size());bytes.append(b);}
    void base(const QString &type,int kind=9,int width=40){
        string(type);integer(kind);integer(width);rawNumber<quint32>(0x00443322);rawNumber<quint32>(0);
        boolean(false);boolean(false);integer(-1);boolean(false);
        bool hasBitmap=type=="TDraht"&&!bitmap.isEmpty();boolean(hasBitmap);if(hasBitmap)bytes.append(bitmap);
        boolean(false);integer(0);number(0);boolean(false);
    }
    void wire(){base("TDraht",dualPin?1:9);integer(1);rawNumber<qint32>(762);rawNumber<qint32>(pinY);rawNumber<qint32>(1270);rawNumber<qint32>(pinY);boolean(false);}
    void text(){
        base("TTextLabel",10,1);rawNumber<qint32>(762);rawNumber<qint32>(700);rawNumber<qint32>(1270);rawNumber<qint32>(850);
        integer(150);number(0);number(0);string("<BauteilKennung>");byte(0);boolean(false);boolean(false);string("Helvetica");
        for(int i=0;i<4;i++)boolean(false);
    }
    void document(bool component){
        QByteArray header(41,0);header[0]=4;header.replace(1,4,"Test");header.replace(37,4,"3.08");bytes.append(header);
        integer(1);
        if(component){
            base("TGruppe",8,1);integer(1);QByteArray labels(95,0);
            // Shortstrings use Windows-1252; these labels are deliberately ASCII.
            labels[0]=2;labels.replace(1,2,"R#");labels[11]=3;labels.replace(12,3,"10k");
            labels[52]=10;labels.replace(53,10,"Widerstand");qToLittleEndian<qint16>(4,labels.data()+93);bytes.append(labels);
            boolean(true);boolean(true);wire();text();
        }else wire();
        for(int v:{0,10000,8000,0,0})integer(v);
        bytes.append(QByteArray(16,0));integer(notes.size());bytes.append(notes);
    }
};
inline QByteArray library(const QByteArray &bitmap={}){Stream s;s.bitmap=bitmap;s.document(true);return s.bytes;}
inline QByteArray halfPitchLibrary(){Stream s;s.pinY=1143;s.dualPin=true;s.document(true);return s.bytes;}
inline QByteArray offsetBoard(){
    Stream s;QByteArray header(41,0);header[0]=4;header.replace(1,4,"Test");header.replace(37,4,"3.08");s.bytes.append(header);s.integer(6);
    for(int x:{508,762,1016})for(auto type:{"TAuge","TBohrung"}){
        s.base(type,QString(type)=="TAuge"?15:14);s.number(QString(type)=="TAuge"?1.8:.9);s.integer(x);s.integer(635);
    }
    for(int v:{0,2540,2540,127,205})s.integer(v);
    s.bytes.append(QByteArray(16,0));s.integer(0);return s.bytes;
}
inline QByteArray board(){Stream s;s.document(false);return s.bytes;}
inline QByteArray project(const QByteArray &notes={}){Stream s;s.notes=notes;s.document(true);s.document(false);return s.bytes;}
}
