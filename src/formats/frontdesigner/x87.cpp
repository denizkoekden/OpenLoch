#include "x87.h"
#include <QtEndian>
#include <algorithm>
#include <cmath>
#include <utility>

namespace openloch::frontdesigner {
namespace {
// Unsigned 128-bit numbers, since not every compiler has them.
struct Wide {
    quint64 hi=0,lo=0;
    bool isZero() const{return !hi&&!lo;}
    bool bit(int i) const{return i>=64?(hi>>(i-64))&1:(lo>>i)&1;}
};
int topBit(const Wide &v){
    for(int i=63;i>=0;i--)if((v.hi>>i)&1)return i+64;
    for(int i=63;i>=0;i--)if((v.lo>>i)&1)return i;
    return -1;
}
Wide shl(Wide v,int n){
    if(n<=0)return v;
    if(n>=128)return {};
    if(n>=64)return {v.lo<<(n-64),0};
    return {(v.hi<<n)|(v.lo>>(64-n)),v.lo<<n};
}
// Shifts right; bits that fall out set `sticky`.
Wide shr(Wide v,int n,bool &sticky){
    if(n<=0)return v;
    if(n>=128){sticky|=!v.isZero();return {};}
    if(n>=64){sticky|=v.lo!=0||(n>64&&(v.hi<<(128-n))!=0);return {0,n==64?v.hi:v.hi>>(n-64)};}
    sticky|=(v.lo<<(64-n))!=0;
    return {v.hi>>n,(v.lo>>n)|(v.hi<<(64-n))};
}
bool less(const Wide &a,const Wide &b){return a.hi!=b.hi?a.hi<b.hi:a.lo<b.lo;}
Wide add(const Wide &a,const Wide &b,bool *carry=nullptr){
    Wide r{a.hi+b.hi,a.lo+b.lo};if(r.lo<a.lo)r.hi++;
    if(carry)*carry=r.hi<a.hi||(r.hi==a.hi&&r.lo<a.lo);
    return r;
}
Wide sub(const Wide &a,const Wide &b){Wide r{a.hi-b.hi,a.lo-b.lo};if(a.lo<b.lo)r.hi--;return r;}
Wide multiply(quint64 a,quint64 b){
    const quint64 a0=a&0xffffffffu,a1=a>>32,b0=b&0xffffffffu,b1=b>>32;
    const quint64 p00=a0*b0,p01=a0*b1,p10=a1*b0,p11=a1*b1;
    const quint64 middle=(p00>>32)+(p01&0xffffffffu)+(p10&0xffffffffu);
    return {p11+(p01>>32)+(p10>>32)+(middle>>32),(middle<<32)|(p00&0xffffffffu)};
}
}

struct ExtendedAccess {
    static Extended make(bool negative,int exponent,quint64 mantissa){Extended r;r.negative=negative;r.exponent=exponent;r.mantissa=mantissa;return r;}
};

namespace {
thread_local int precision=64;   // bits of mantissa that sums, products, quotients and roots are rounded to
}

Precision::Precision(int bits):previous(precision){precision=bits==53?53:64;}
Precision::~Precision(){precision=previous;}

// The nearest value of (-1)^negative * m * 2^e with `bits` bits of mantissa; `sticky` tells that nonzero bits below m
// were cut off.
static Extended nearest(bool negative,Wide m,int e,bool sticky,int bits=64){
    const int top=topBit(m);if(top<0)return {};
    quint64 mantissa;int exponent;
    if(const int s=top+1-bits;s>0){
        bool rest=sticky;
        const bool half=m.bit(s-1);
        if(s>=2){Wide low=shl(m,128-(s-1));rest|=!low.isZero();}
        bool unused=false;quint64 kept=shr(m,s,unused).lo;exponent=e+s+bits-1;
        if(half&&(rest||(kept&1))){kept++;if(bits==64?!kept:kept>>bits){kept=quint64(1)<<(bits-1);exponent++;}}
        mantissa=kept<<(64-bits);
    }else{mantissa=m.lo<<(63-top);exponent=e+top;}
    return ExtendedAccess::make(negative,exponent,mantissa);
}

Extended Extended::fromInt(qint64 v){
    if(!v)return {};
    const bool negative=v<0;const quint64 magnitude=negative?quint64(0)-quint64(v):quint64(v);
    return nearest(negative,{0,magnitude},0,false);
}
Extended Extended::fromDouble(double v){
    if(v==0||!std::isfinite(v))return {};
    int e=0;const double fraction=std::frexp(std::abs(v),&e);
    return nearest(v<0,{0,quint64(std::ldexp(fraction,64))},e-64,false);
}
Extended Extended::fromBytes(const char *ten){
    const quint64 m=qFromLittleEndian<quint64>(ten);const quint16 top=qFromLittleEndian<quint16>(ten+8);
    if((top&0x7fff)==0x7fff||!m)return {};
    return nearest(top&0x8000,{0,m},int(top&0x7fff)-16383-63,false);
}
QByteArray Extended::bytes() const{
    QByteArray out(10,0);
    if(mantissa){qToLittleEndian(mantissa,out.data());qToLittleEndian(quint16((negative?0x8000:0)|((exponent+16383)&0x7fff)),out.data()+8);}
    return out;
}
double Extended::toDouble() const{
    if(!mantissa)return 0;
    const double v=std::ldexp(double(mantissa),exponent-63);return negative?-v:v;
}
qint64 Extended::rounded() const{
    if(!mantissa)return 0;
    quint64 whole;
    if(exponent>=63)whole=mantissa<<std::min(exponent-63,62);
    else{
        const int s=63-exponent;
        if(s>64)whole=0;
        else{
            bool rest=false;bool unused=false;const Wide m{0,mantissa};
            const bool half=m.bit(s-1);if(s>=2)rest=!shl(m,128-(s-1)).isZero();
            whole=s==64?0:shr(m,s,unused).lo;
            if(half&&(rest||(whole&1)))whole++;
        }
    }
    return negative?-qint64(whole):qint64(whole);
}

int compare(const Extended &a,const Extended &b){
    const bool az=a.isZero(),bz=b.isZero();
    if(az&&bz)return 0;
    const bool an=a.isNegative(),bn=b.isNegative();
    if(az)return bn?1:-1;
    if(bz)return an?-1:1;
    if(an!=bn)return an?-1:1;
    int c=a.exponent!=b.exponent?(a.exponent<b.exponent?-1:1):a.mantissa!=b.mantissa?(a.mantissa<b.mantissa?-1:1):0;
    return an?-c:c;
}

Extended operator+(const Extended &a,const Extended &b){
    if(a.isZero())return b;
    if(b.isZero())return a;
    const Extended *x=&a,*y=&b;
    if(y->exponent>x->exponent||(y->exponent==x->exponent&&y->mantissa>x->mantissa))std::swap(x,y);
    bool sticky=false;
    const Wide big{x->mantissa,0};const Wide small=shr(Wide{y->mantissa,0},x->exponent-y->exponent,sticky);
    int e=x->exponent-127;Wide sum;
    if(x->negative==y->negative){
        bool carry=false;sum=add(big,small,&carry);
        if(carry){sum=shr(sum,1,sticky);sum.hi|=quint64(1)<<63;e++;}
    }else{
        sum=sub(big,small);
        if(sticky)sum=sub(sum,Wide{0,1});
        if(sum.isZero()&&!sticky)return {};
    }
    return nearest(x->negative,sum,e,sticky,precision);
}
Extended operator*(const Extended &a,const Extended &b){
    if(a.isZero()||b.isZero())return {};
    return nearest(a.negative!=b.negative,multiply(a.mantissa,b.mantissa),a.exponent+b.exponent-126,false,precision);
}
Extended operator/(const Extended &a,const Extended &b){
    if(a.isZero()||b.isZero())return {};
    // Long division of the mantissas, 65 bits beyond the point, the remainder only telling whether it is exact.
    constexpr int extra=65;Wide quotient,remainder;
    for(int i=63+extra;i>=0;i--){
        remainder=shl(remainder,1);if(i>=extra&&((a.mantissa>>(i-extra))&1))remainder.lo|=1;
        quotient=shl(quotient,1);
        if(!less(remainder,Wide{0,b.mantissa})){remainder=sub(remainder,Wide{0,b.mantissa});quotient.lo|=1;}
    }
    return nearest(a.negative!=b.negative,quotient,a.exponent-b.exponent-extra,!remainder.isZero(),precision);
}
Extended Extended::sqrt() const{
    if(!mantissa||negative)return {};
    // Digit by digit on the mantissa (made 66 bits with an even exponent), followed by zeros for 68 bits of root.
    int e=exponent-63;Wide n{0,mantissa};
    if(e&1){n=shl(n,1);e--;}
    constexpr int zeros=35;Wide root,remainder;
    for(int pair=32+zeros;pair>=0;pair--){
        quint64 two=0;
        if(pair>=zeros){const int at=2*(pair-zeros);two=(quint64(n.bit(at+1))<<1)|quint64(n.bit(at));}
        remainder=shl(remainder,2);remainder.lo|=two;
        const Wide trial=add(shl(root,2),Wide{0,1});
        root=shl(root,1);
        if(!less(remainder,trial)){remainder=sub(remainder,trial);root.lo|=1;}
    }
    return nearest(false,root,e/2-zeros,!remainder.isZero(),precision);
}

namespace {
Extended part(quint64 bits,int e){return nearest(false,Wide{0,bits},e,false);}
// pi/2 in three pieces of 60, 60 and 64 bits, so that small multiples of the first two are exact.
const Extended halfPi1=part(0xc90fdaa22168c23ULL,-59),halfPi2=part(0x4c4c6628b80dc1cULL,-119),halfPi3=part(0xd129024e088a67ccULL,-183);
// Sine and cosine of r in [-pi/4, pi/4] by their series, evaluated from the small end.
Extended seriesSin(const Extended &r){
    const Extended r2=r*r;Extended sum=Extended::fromInt(1);
    for(int k=12;k>=1;k--)sum=Extended::fromInt(1)-r2/Extended::fromInt(qint64(2*k)*(2*k+1))*sum;
    return r*sum;
}
Extended seriesCos(const Extended &r){
    const Extended r2=r*r;Extended sum=Extended::fromInt(1);
    for(int k=12;k>=1;k--)sum=Extended::fromInt(1)-r2/Extended::fromInt(qint64(2*k-1)*(2*k))*sum;
    return sum;
}
// x = k pi/2 + r with |r| <= pi/4 (about); returns k mod 4.
int reduce(const Extended &x,Extended &r){
    const qint64 k=std::llround(x.toDouble()/(3.14159265358979323846/2));
    const Extended kk=Extended::fromInt(k);
    r=((x-kk*halfPi1)-kk*halfPi2)-kk*halfPi3;
    return int(((k%4)+4)%4);
}
}

// The trigonometric functions of the unit work to the full 64 bits whatever the precision control says.
Extended Extended::sin() const{
    const Precision full(64);Extended r;const int k=reduce(abs(),r);
    Extended v=k==0?seriesSin(r):k==1?seriesCos(r):k==2?-seriesSin(r):-seriesCos(r);
    return isNegative()?-v:v;
}
Extended Extended::atan() const{
    // atan x = pi/2 - atan(1/x) for |x| > 1, then halving the angle with atan x = 2 atan(x / (1 + sqrt(1 + x^2))) until
    // the series is short.
    if(!mantissa)return {};
    const Precision full(64);const Extended one=Extended::fromInt(1);Extended x=abs();bool inverted=false;int halvings=0;
    if(one<x){x=one/x;inverted=true;}
    while(Extended::fromDouble(0.0625)<x){x=x/(one+(one+x*x).sqrt());halvings++;}
    const Extended x2=x*x;Extended sum=one;
    for(int k=12;k>=1;k--)sum=one-x2*Extended::fromInt(2*k-1)/Extended::fromInt(2*k+1)*sum;
    Extended v=x*sum;
    for(int i=0;i<halvings;i++)v=v*Extended::fromInt(2);
    if(inverted)v=(halfPi1+halfPi2+halfPi3)-v;
    return isNegative()?-v:v;
}
Extended Extended::cos() const{
    const Precision full(64);Extended r;const int k=reduce(abs(),r);
    return k==0?seriesCos(r):k==1?-seriesSin(r):k==2?-seriesCos(r):seriesSin(r);
}
Extended Extended::tan() const{const Precision full(64);return sin()/cos();}
}
