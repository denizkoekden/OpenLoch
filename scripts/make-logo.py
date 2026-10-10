#!/usr/bin/env python3
"""Draw the OpenLoch logo pixel by pixel, in the style of the program's own toolbar symbols.

A 32 x 32 perfboard in the original's board colours (yellow board, lighter pads, black holes) with a TO-92 transistor,
a red LED, a resistor with the valid colour code brown-black-red-gold (1 kOhm, 5 %) and three wire bridges. Writes
assets/openloch-logo-32.png (the pixel master) and assets/app-icon-source.png (enlarged without smoothing to 1024 px);
scripts/make-icons.py turns the latter into the macOS and Windows icons.
"""
from pathlib import Path
import struct
import zlib

root = Path(__file__).resolve().parents[1]
# Colours: the Windows 16-colour palette of the symbols plus the board colours of the program.
P={'.':None,'K':(0,0,0),'Y':(239,204,5),'H':(254,245,207),'S':(207,172,3),'p':(255,220,21),
   'B':(226,206,160),'b':(176,150,100),'n':(122,70,22),'k':(24,24,24),'r':(220,20,20),'g':(212,170,40),
   'L':(192,192,192),'D':(128,128,128),'R':(255,0,0),'M':(128,0,0),'W':(255,255,255),'U':(0,0,255),'N':(0,0,128),
   'E':(0,160,0),'e':(0,96,0),'T':(40,40,44),'t':(96,96,104)}
W=H=32
img=[['.']*W for _ in range(H)]
def put(x,y,c):
    if 0<=x<W and 0<=y<H: img[y][x]=c
def rect(x,y,w,h,c):
    for j in range(y,y+h):
        for i in range(x,x+w): put(i,j,c)
# board 28x28 at (2,2): black frame, light bevel top/left, shadow bottom/right
rect(2,2,28,28,'K');rect(3,3,26,26,'Y')
for i in range(3,29): put(i,3,'H');put(3,i,'H')
for i in range(4,29): put(i,28,'S');put(28,i,'S')
# solder pads 3x3 with a black hole, pitch 4
holes=[6,10,14,18,22,26]
for cy in holes:
    for cx in holes:
        rect(cx-1,cy-1,3,3,'p');put(cx,cy,'K')
def pad(cx,cy):
    rect(cx-1,cy-1,3,3,'p');put(cx,cy,'K')
# transistor (TO-92 from above): round back, flat front
tr=['..KKKKK..',
    '.KTTTTTK.',
    'KTtTTTTTK',
    'KTtTTTTTK',
    'KTTTTTTTK',
    'KKKKKKKKK']
for j,row in enumerate(tr):
    for i,c in enumerate(row):
        if c!='.': put(4+i,4+j,c)
# LED top right: red dome with a highlight
led=['..KKKKK..',
     '.KRRRRRK.',
     'KRWWRRRRK',
     'KRWRRRRRK',
     'KRRRRRRMK',
     'KRRRRRMMK',
     '.KRRRMMK.',
     '..KKKKK..']
for j,row in enumerate(led):
    for i,c in enumerate(row):
        if c!='.': put(18+i,4+j,c)
# resistor across the middle: leads from hole (6,14) to (26,14), body cols 9..23
for x in range(6,27): put(x,14,'L')
pad(6,14);pad(26,14)
body=['.KKKKKKKKKKKKK.',
      'KBBnBkBrBBgBBBK',
      'KBBnBkBrBBgBBBK',
      'KbbnbkbrbbgbbbK',
      '.KKKKKKKKKKKKK.']
for j,row in enumerate(body):
    for i,c in enumerate(row):
        if c!='.': put(9+i,12+j,c)
# wire bridges: blue from (6,22) to (18,22), red from (10,26) to (22,26), green up from (26,18) to (26,26)
def wire(x1,y1,x2,y2,c,d):
    if y1==y2:
        for x in range(x1,x2+1): put(x,y1,c);put(x,y1+1,d)
    else:
        for y in range(y1,y2+1): put(x1,y,c);put(x1+1,y,d)
wire(6,21,18,21,'U','N');pad(6,22);pad(18,22);put(6,22,'K');put(18,22,'K')
wire(10,25,22,25,'R','M');pad(10,26);pad(22,26)
wire(25,18,25,26,'E','e');pad(26,18);pad(26,26)
# re-punch holes where wires end
for (x,y) in [(6,22),(18,22),(10,26),(22,26),(26,18),(26,26),(6,14),(26,14)]: put(x,y,'K')


def png(path, scale):
    raw = b''
    for row in img:
        line = b''.join((bytes(P[c]) + b'\xff' if P[c] else b'\x00\x00\x00\x00') * scale for c in row)
        raw += (b'\x00' + line) * scale
    def chunk(kind, data):
        return struct.pack('>I', len(data)) + kind + data + struct.pack('>I', zlib.crc32(kind + data) & 0xffffffff)
    header = struct.pack('>IIBBBBB', W * scale, H * scale, 8, 6, 0, 0, 0)
    path.write_bytes(b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', header) + chunk(b'IDAT', zlib.compress(raw, 9)) + chunk(b'IEND', b''))


png(root / 'assets/openloch-logo-32.png', 1)
png(root / 'assets/app-icon-source.png', 32)
print('Generated openloch-logo-32.png and app-icon-source.png')
