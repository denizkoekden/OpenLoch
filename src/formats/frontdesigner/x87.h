#pragma once
#include <QByteArray>
#include <QtGlobal>

// Floating point as the original computes: 80-bit extended values of the x87 unit (64-bit mantissa), every operation
// rounded once to the nearest value with ties to even. Done in integers, so that all platforms give the same bits;
// the outlines of the HPGL export depend on them where a coordinate lands exactly between two whole file units.
namespace openloch::frontdesigner {
class Extended {
public:
    Extended()=default;                                 // zero
    static Extended fromInt(qint64 v);
    static Extended fromDouble(double v);               // exact
    static Extended fromBytes(const char *ten);         // the ten bytes of a file value
    QByteArray bytes() const;
    double toDouble() const;                            // nearest double
    // Delphi's Round: to the nearest whole number, halves to the even one.
    qint64 rounded() const;
    bool isZero() const{return mantissa==0;}
    bool isNegative() const{return negative&&mantissa;}
    Extended operator-() const{Extended r=*this;r.negative=!r.negative;return r;}
    Extended abs() const{Extended r=*this;r.negative=false;return r;}
    friend Extended operator+(const Extended &a,const Extended &b);
    friend Extended operator-(const Extended &a,const Extended &b){return a+(-b);}
    friend Extended operator*(const Extended &a,const Extended &b);
    friend Extended operator/(const Extended &a,const Extended &b);
    Extended sqrt() const;
    // Sine, cosine, tangent and arc tangent to about one unit in the last place, like the unit's FSIN, FCOS, FPTAN and
    // FPATAN (which are not exact either).
    Extended sin() const;
    Extended cos() const;
    Extended tan() const;
    Extended atan() const;
    friend int compare(const Extended &a,const Extended &b);  // -1, 0, 1
    friend bool operator<(const Extended &a,const Extended &b){return compare(a,b)<0;}
    friend bool operator<=(const Extended &a,const Extended &b){return compare(a,b)<=0;}
    friend bool operator==(const Extended &a,const Extended &b){return compare(a,b)==0;}
private:
    // value = (-1)^negative * mantissa * 2^(exponent-63); the mantissa has its top bit set unless the value is zero.
    bool negative=false;
    int exponent=0;
    quint64 mantissa=0;
    friend struct ExtendedAccess;
};
// The unit's precision control for the current thread while the object lives: sums, differences, products, quotients
// and square roots are rounded to 64 bits of mantissa, or to 53 as with the setting Windows starts a program with.
// Loading, storing, rounding to whole numbers and the trigonometric functions do not depend on it.
class Precision {
public:
    explicit Precision(int bits);
    ~Precision();
    Precision(const Precision&)=delete;
    Precision &operator=(const Precision&)=delete;
private:
    int previous;
};
}
