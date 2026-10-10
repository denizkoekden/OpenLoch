#!/usr/bin/env python3
"""A small Python reader for LochMaster files (LM4, LMB, LIB), used by the example checks.

It follows the file format described in docs/lochmaster-format.md and reads the records in order; it does not search
for or guess object boundaries. The project tail is kept but not interpreted here.
"""
from pathlib import Path
import struct, math, argparse, json, collections

def dib_length(data,at):
    """The length of a bitmap as its info header gives it, -1 for unknown headers."""
    if at+30>len(data):return -1
    header=struct.unpack_from('<I',data,at+14)[0]
    if header==12:
        w,h,_,bpp=struct.unpack_from('<HHHH',data,at+18)
        if w<=0 or bpp==0 or bpp>32:return -1
        return 14+12+((1<<bpp)*3 if bpp<=8 else 0)+(w*bpp+31)//32*4*h
    if header<40 or header>1024 or at+54>len(data):return -1
    w,h,_,bpp,compression,image=struct.unpack_from('<iiHHII',data,at+18);used=struct.unpack_from('<I',data,at+46)[0]
    if w<=0 or bpp==0 or bpp>32 or used>65536:return -1
    colours=used or ((1<<bpp) if bpp<=8 else 0);masks=12 if compression==3 and header==40 else 0
    bits=image or (w*bpp+31)//32*4*abs(h)
    return 14+header+masks+colours*4+bits

class Reader:
    def __init__(self,data):self.data=data;self.pos=0;self.version=0;self.total=0
    def raw(self,n):
        if n<0 or n>len(self.data)-self.pos:raise ValueError(f'truncated data at {self.pos:#x}, length {n}')
        b=self.data[self.pos:self.pos+n];self.pos+=n;return b
    def unpack(self,fmt):return struct.unpack('<'+fmt,self.raw(struct.calcsize('<'+fmt)))[0]
    def byte(self):return self.unpack('B')
    def integer(self):
        tag=self.byte()
        if tag not in (2,3,4):raise ValueError(f'integer tag {tag:#x} at {self.pos-1:#x}')
        return self.unpack({2:'b',3:'h',4:'i'}[tag])
    def boolean(self):
        tag=self.byte()
        if tag not in (8,9):raise ValueError(f'boolean tag {tag:#x} at {self.pos-1:#x}')
        return tag==9
    def number(self):
        tag=self.byte()
        if tag==5:
            m,e=struct.unpack('<QH',self.raw(10));exp=e&0x7fff
            if exp==0x7fff:raise ValueError('nonfinite extended number')
            return (-1 if e&0x8000 else 1)*math.ldexp(m/2**63,exp-16383 if exp else -16382) if m else 0
        if tag in (2,3,4):return self.unpack({2:'b',3:'h',4:'i'}[tag])
        if tag==15:return self.unpack('f')
        if tag==21:return self.unpack('d')
        raise ValueError(f'number tag {tag:#x} at {self.pos-1:#x}')
    def string(self):
        tag=self.byte()
        if tag==6:return self.raw(self.byte()).decode('cp1252')
        if tag in (12,20):return self.raw(self.unpack('i')).decode('cp1252' if tag==12 else 'utf-8')
        if tag==18:return self.raw(self.unpack('i')*2).decode('utf-16-le')
        raise ValueError(f'string tag {tag:#x} at {self.pos-1:#x}')
    def points(self):
        n=self.integer()+1
        if not 0<=n<=100000:raise ValueError('invalid point count')
        return [struct.unpack('<ii',self.raw(8)) for _ in range(n)]
    def base(self,kind):
        o=dict(type=kind,offset=self.pos,kind=self.integer(),width=self.integer(),pen=self.unpack('I'),brush=self.unpack('I'),transparent=self.boolean(),flag=self.boolean(),points=self.points(),back=self.boolean())
        if self.boolean():
            if self.data[self.pos:self.pos+2]!=b'BM':raise ValueError(f'unsupported bitmap at {self.pos:#x}')
            size=struct.unpack_from('<I',self.data,self.pos+2)[0]
            if size<26:raise ValueError('invalid BMP size')
            # As OpenLoch's reader: the original reads a bitmap by its info header and saves 24-bit pictures with a colour
            # table with a file size 1024 bytes too large; the info header's length counts when a truth value follows it.
            stored=dib_length(self.data,self.pos)
            if stored>=26 and stored!=size and self.pos+stored<len(self.data) and self.data[self.pos+stored] in (8,9):size=stored
            o['bitmap_offset']=self.pos;o['bitmap_size']=size;self.raw(size)
        o['flag2']=self.boolean();o['style']=self.integer();o['rotation']=self.number()
        if self.version>3.055:o['flag3']=self.boolean()
        if self.version>3.995:o['anchors']=[self.integer() for _ in range(6)]
        if self.version>4.005:
            o['label']=self.string();n=self.integer()
            if not 0<=n<=10000:raise ValueError('invalid extra field count')
            o['extra']=[(self.string(),self.boolean(),self.string()) for _ in range(n)]
        return o
    def object(self,kind=None,depth=0):
        if depth>64:raise ValueError('excessive nesting')
        self.total+=1
        if self.total>100000:raise ValueError('excessive objects')
        start=self.pos
        kind=kind or self.string();o=self.base(kind);o['start']=start
        if kind in ('TGruppe','TFarbcode','TBt'):
            n=self.integer()+1
            if not 0<=n<=100000:raise ValueError('invalid group count')
            if self.version>3.9:
                o['id']=self.string();o['value']=self.string();o['description']=self.string();o['group_value']=self.integer()
            else:
                b=self.raw(95)
                def ss(p,maxlen):return b[p+1:p+1+min(b[p],maxlen)].decode('cp1252')
                o['id']=ss(0,10);o['value']=ss(11,40);o['description']=ss(52,40);o['group_value']=struct.unpack_from('<h',b,93)[0]
            o['group_flags']=[self.boolean(),self.boolean()]
            o['children']=[self.object(depth=depth+1) for _ in range(n)]
            if kind=='TFarbcode' and self.version>=3.045:o['bands']=self.integer();o['resistance']=self.number()
            if kind=='TBt':o['component_kind']=self.byte()
        elif kind in ('TBohrung','TAuge'):
            o['diameter']=self.number();o['center']=[self.integer(),self.integer()]
        elif kind in ('TDraht','TDrahtFest','TLeiterbahn'):
            o['path']=self.points()
            if self.boolean():o['second_path']=self.points()
        elif kind in ('TTrenner','TTrennerFest'):
            o['rect']=struct.unpack('<iiii',self.raw(16))
        elif kind=='TKreis':
            o['ellipse']=[self.integer() for _ in range(4)];o['ellipse_flag']=self.boolean()
            o['ellipse2']=[self.integer() for _ in range(4)]
            if self.version>3.01:o['inner']=self.object('TDraht',depth+1)
        elif kind=='TTextLabel':
            o['text_position']=struct.unpack('<ii',self.raw(8));o['text_end']=struct.unpack('<ii',self.raw(8))
            o['text_kind']=self.integer();o['text_height']=self.number();o['text_width']=self.number();o['text']=self.string()
            o['text_role']=self.byte();o['text_flags']=[self.boolean(),self.boolean()]
            if self.version>3.035:o['font']=self.string();o['font_style']=[self.boolean() for _ in range(4)]
            if self.version>4.035:o['text_anchors']=[self.integer() for _ in range(12)];o['text_flags2']=[self.boolean(),self.boolean()]
            if self.version>4.065:o['text_metric']=self.integer()
        elif kind!='TBauteil':raise ValueError(f'unsupported class {kind} at {self.pos:#x}')
        o['end']=self.pos;return o
    def document(self,with_board=False):
        start=self.pos
        header=self.raw(41)
        if header[0]>35:raise ValueError('invalid title length')
        version=header[37:41].decode('ascii').replace(',','.')
        if version not in ('3.08','4.03','4.04','4.06','4.07'):raise ValueError(f'unverified file version {version}')
        self.version=float(version)
        doc=dict(title=header[1:1+header[0]].decode('cp1252'),version=version,start=start)
        if self.version>3.9:doc['description']=self.string()
        doc['count_offset']=self.pos
        n=self.integer()
        if not 0<=n<=100000:raise ValueError('invalid object count')
        doc['objects']=[self.object() for _ in range(n)]
        doc['list_end']=self.pos
        doc['settings']=[self.integer() for _ in range(5)]
        doc['size']=doc['settings'][1:3];doc['origin']=doc['settings'][3:5]
        doc['tail_offset']=self.pos
        if self.version>4.015:doc['metadata']=self.object('TGruppe')
        if self.version>4.025:doc['grid_mm']=self.number()
        if self.version>4.045:
            views=[]
            for _ in range(11):
                views.append(dict(flags=[self.boolean() for _ in range(7)],scale=self.number(),
                    bounds=[self.integer() for _ in range(4)],number1=self.number(),number2=self.number(),
                    flags2=[self.boolean() for _ in range(10)],number3=self.number(),integers=[self.integer() for _ in range(3)],flag=self.boolean()))
            doc['views']=views;doc['view_index']=self.integer();doc['view_flags']=[self.boolean() for _ in range(4)];doc['view_integer']=self.integer()
        if self.version>4.055:doc['numbers']=[self.number(),self.number()]
        if self.version>3.075:doc['raw_points']=self.raw(16).hex()
        if self.version>3.025:
            doc['annotations_length_offset']=self.pos;n=self.integer();doc['annotations_size']=n;doc['annotations_offset']=self.pos;self.raw(n)
        if with_board and self.version>3.065:doc['board']=self.document()
        doc['end']=self.pos;doc['tail_size']=len(self.data)-self.pos;doc['object_count']=self.total
        return doc

def main():
    ap=argparse.ArgumentParser();ap.add_argument('paths',nargs='+',type=Path);ap.add_argument('--json',type=Path);args=ap.parse_args()
    report=[]
    for p in args.paths:
        try:
            r=Reader(p.read_bytes());doc=r.document(p.suffix.upper()=='.LM4');report.append(dict(file=str(p),title=doc['title'],version=doc['version'],objects=r.total,tail_size=doc['tail_size']))
            if args.json:args.json.write_text(json.dumps(doc,indent=2,ensure_ascii=False))
        except Exception as e:report.append(dict(file=str(p),error=str(e)))
    print(json.dumps(report,indent=2,ensure_ascii=False))

if __name__=='__main__':main()
