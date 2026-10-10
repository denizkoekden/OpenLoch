# More parts for the µP pages "PIC" and "Raspberry". PIC packages: the pins in package order after the "Pin Diagrams" of
# the Microchip data sheets (named per family below), port first and then its main other function, like mcu.py.
# Raspberry Pi boards: seen from above at 1:1 after the official mechanical drawings (Raspberry Pi Ltd,
# datasheets.raspberrypi.com: raspberry-pi-3-b-plus-mechanical-drawing.pdf, raspberry-pi-4-mechanical-drawing.pdf,
# raspberry-pi-zero-w-mechanical-drawing.pdf): outline with 3 mm corners, the M2.5 holes with their 6 mm pads, the GPIO
# header (pin 1 filled, odd pins in the inner row) and the connectors as labelled boxes. Pictures only, no contacts.
# Connector names and pads after the silkscreen in the DXF drawings (RP-008336-DS-1 for the 3B+, RP-008342-DS-1 for the
# 4B) and, for the Zero W, the board photo of the Raspberry Pi documentation (computers/raspberry-pi/images/zero-w.jpg);
# GPIO pin 1 is the square pad (DXF; Raspberry Pi 4 Model B datasheet, release 1.1, figure 3 numbers the pins).
add = extend(MP, 'PIC')

# PIC16F873A/876A: DS39582C, 28-pin PDIP
PIC16F87XA_28 = ['~MCLR', 'RA0/AN0', 'RA1/AN1', 'RA2/AN2', 'RA3/AN3', 'RA4/T0CKI', 'RA5/AN4', 'VSS', 'OSC1', 'OSC2',
                 'RC0/T1OSO', 'RC1/CCP2', 'RC2/CCP1', 'RC3/SCK', 'RC4/SDI', 'RC5/SDO', 'RC6/TX', 'RC7/RX', 'VSS', 'VDD',
                 'RB0/INT', 'RB1', 'RB2', 'RB3/PGM', 'RB4', 'RB5', 'RB6/PGC', 'RB7/PGD']
# PIC16F88: DS30487D, 18-pin PDIP
PIC16F88 = ['RA2/AN2', 'RA3/AN3', 'RA4/AN4', 'RA5/~MCLR', 'VSS', 'RB0/INT', 'RB1/SDI', 'RB2/RX', 'RB3/PGM', 'RB4/SCK',
            'RB5/TX', 'RB6/PGC', 'RB7/PGD', 'VDD', 'RA6/OSC2', 'RA7/OSC1', 'RA0/AN0', 'RA1/AN1']
# PIC16F818 and PIC16F819 (one data sheet, DS39598): pin for pin after KiCad's PIC16F818-IP (alias PIC16F819-IP)
PIC16F818 = ['RA2/AN2', 'RA3/AN3', 'RA4/AN4', 'RA5/~MCLR', 'VSS', 'RB0/INT', 'RB1/SDI', 'RB2/SDO', 'RB3/PGM', 'RB4/SCK',
             'RB5/~SS', 'RB6/PGC', 'RB7/PGD', 'VDD', 'RA6/OSC2', 'RA7/OSC1', 'RA0/AN0', 'RA1/AN1']
# PIC16F627A (same pins as 628A/648A): DS40044G, 18-pin PDIP
PIC16F627A = ['RA2/AN2', 'RA3/AN3', 'RA4/T0CKI', 'RA5/~MCLR', 'VSS', 'RB0/INT', 'RB1/RX', 'RB2/TX', 'RB3/CCP1', 'RB4/PGM',
              'RB5', 'RB6/PGC', 'RB7/PGD', 'VDD', 'RA6/OSC2', 'RA7/OSC1', 'RA0/AN0', 'RA1/AN1']
# PIC12F629: DS41190G, 8-pin PDIP
PIC12F629 = ['VDD', 'GP5/OSC1', 'GP4/OSC2', 'GP3/~MCLR', 'GP2/T0CKI', 'GP1/CIN-', 'GP0/CIN+', 'VSS']
# PIC16F630 and PIC16F676: DS40039F, 14-pin PDIP
PIC16F630 = ['VDD', 'RA5/OSC1', 'RA4/OSC2', 'RA3/~MCLR', 'RC5', 'RC4', 'RC3', 'RC2', 'RC1', 'RC0', 'RA2/T0CKI', 'RA1/CIN-',
             'RA0/CIN+', 'VSS']
PIC16F676 = ['VDD', 'RA5/OSC1', 'RA4/OSC2', 'RA3/~MCLR', 'RC5', 'RC4', 'RC3/AN7', 'RC2/AN6', 'RC1/AN5', 'RC0/AN4', 'RA2/AN2',
             'RA1/AN1', 'RA0/AN0', 'VSS']

for name, package, pins in (('PIC16F876A', 'DIP-28', PIC16F87XA_28), ('PIC16F873A', 'DIP-28', PIC16F87XA_28),
                            ('PIC16F88', 'DIP-18', PIC16F88), ('PIC16F818', 'DIP-18', PIC16F818),
                            ('PIC16F819', 'DIP-18', PIC16F818), ('PIC16F627A', 'DIP-18', PIC16F627A),
                            ('PIC12F629', 'DIP-8', PIC12F629), ('PIC16F630', 'DIP-14', PIC16F630),
                            ('PIC16F676', 'DIP-14', PIC16F676)):
    title = f'{name} ({package})'
    add(package_box('IC', C(title, title, title), name, pins))

add = extend(MP, 'Raspberry')
MOUNT_HOLE, PAD = 2.75, 6.0


def pcb(designator, caption, w, h, holes):
    """A bare board seen from above, its upper left corner at the insertion point."""
    s = Symbol(designator, caption, numbered=False)
    s.rect(0, 0, w, h, w=0.35, corner=300 / min(w, h))
    for x, y in holes:
        s.circle(x, y, PAD, w=0.18).circle(x, y, MOUNT_HOLE)
    s.labels((0, -3.4))
    return s


def gpio(s, cx=32.5, cy=3.5, one=True):
    """The 40-pin header along the upper edge: pins 1 and 2 at the left, odd pins in the row towards the board's middle;
    pin 1 filled (and numbered where there is room)."""
    s.rect(cx - 10 * P, cy - P, cx + 10 * P, cy + P, w=0.18)
    for k in range(40):
        x, y = cx + (k // 2 - 9.5) * P, cy + (P / 2 if k % 2 == 0 else -P / 2)
        s.rect(x - 0.6, y - 0.6, x + 0.6, y + 0.6, w=0.12, fill='#000000' if k == 0 else None)
    if one:
        s.text('1', cx - 9.5 * P, cy + P + 0.3, 1.4, 'centre')
    s.text('GPIO', cx, cy + P + 0.3, 1.6, 'centre')


def socket(s, x0, y0, x1, y1, label='', h=1.8, turn=False, dashed=False):
    """A connector as a box with its name inside (several lines split at "\\n"; `turn`: written upwards)."""
    s.rect(x0, y0, x1, y1, w=0.25)
    if dashed:
        s.parts[-1]['pen']['style'] = 'dash'
    cx, cy = (x0 + x1) / 2, (y0 + y1) / 2
    if turn:
        s.text(label, cx - h / 2, cy, h, 'centre', rotation=90)
    else:
        lines = label.split('\n') if label else []
        for k, line in enumerate(lines):
            s.text(line, cx, cy - len(lines) * h * 0.6 + k * h * 1.2, h, 'centre')


B_HOLES = [(3.5, 3.5), (61.5, 3.5), (3.5, 52.5), (61.5, 52.5)]

s = pcb('Raspberry Pi 3B+', C('Raspberry Pi 3 Modell B+ (Platine)', 'Raspberry Pi 3 Model B+ (board)',
                              'Raspberry Pi 3 Modèle B+ (carte)'), 85, 56, B_HOLES)
gpio(s)
socket(s, 59.0, 7.12, 64.0, 12.12, 'PoE', 1.6)
socket(s, 59.0, 15.0, 64.0, 17.0)       # J2: GLOBAL_EN and RUN (silkscreen PEN, RUN; schematic RP-008339-DS-1)
s.text('Run', 58.4, 15.2, 1.6, 'right')
socket(s, 69.3, 2.45, 87.1, 15.55, '2 × USB')
socket(s, 69.3, 20.45, 87.1, 33.55, '2 × USB')
socket(s, 65.6, 38.0, 87.1, 53.5, 'LAN')
socket(s, 50.0, 43.5, 57.0, 56.0, 'Audio\n3,5 mm', 1.5)
s.rect(50.5, 56.0, 56.5, 58.5, w=0.25)
socket(s, 24.5, 45.4, 39.5, 57.4, 'HDMI')
socket(s, 6.8, 51.4, 14.4, 57.3)
s.text('Micro-USB 5 V', 10.6, 47.6, 1.6, 'centre')
socket(s, 43.6, 33.5, 48.8, 55.7, 'Kamera (CSI)', 1.6, turn=True)
socket(s, 2.5, 16.8, 5.4, 39.1, 'Display (DSI)', 1.6, turn=True)
add(s)

s = pcb('Raspberry Pi 4B', C('Raspberry Pi 4 Modell B (Platine)', 'Raspberry Pi 4 Model B (board)',
                             'Raspberry Pi 4 Modèle B (carte)'), 85, 56, B_HOLES)
gpio(s)
socket(s, 59.0, 7.14, 64.0, 12.14, 'PoE', 1.6)
socket(s, 66.6, 2.5, 88.0, 18.0, 'LAN')
socket(s, 70.5, 22.45, 88.0, 35.55, '2 × USB 3.0')     # silkscreen USB3 (blue sockets), USB2 at the edge
socket(s, 70.5, 40.45, 88.0, 53.55, '2 × USB 2.0')
socket(s, 7.0, 50.4, 15.4, 57.3)
s.text('USB-C 5 V', 11.2, 47.6, 1.6, 'centre')
socket(s, 22.9, 49.5, 29.1, 57.5)
socket(s, 36.4, 49.5, 42.6, 57.5)
s.text('2 × Micro-HDMI', 32.75, 46.4, 1.6, 'centre')
socket(s, 45.1, 33.4, 49.0, 55.7, 'Kamera', 1.6, turn=True)
socket(s, 50.5, 43.5, 57.5, 56.0, 'Audio', 1.5)
s.rect(51.0, 56.0, 57.0, 58.5, w=0.25)
socket(s, 1.5, 16.8, 5.4, 39.2, 'Display', 1.6, turn=True)
socket(s, 1.7, 21.9, 13.1, 33.9, dashed=True)         # on the underside (DXF, bottom side), the card sticking out
socket(s, -2.3, 22.5, 1.7, 33.5, dashed=True)
s.text('microSD', 9.3, 27.2, 1.4, 'centre')
add(s)

s = pcb('Raspberry Pi Zero W', C('Raspberry Pi Zero W (Platine)', 'Raspberry Pi Zero W (board)',
                                 'Raspberry Pi Zero W (carte)'), 65, 30,
        [(3.5, 3.5), (61.5, 3.5), (3.5, 26.5), (61.5, 26.5)])
gpio(s, one=False)
socket(s, 1.7, 7.1, 13.1, 19.1, 'microSD', 1.6)
s.rect(-2.3, 7.5, 1.7, 18.5, w=0.18)
socket(s, 6.9, 23.0, 17.8, 30.5, 'Mini-HDMI', 1.5)
socket(s, 37.6, 25.3, 45.1, 30.0, 'USB', 1.5)
socket(s, 50.25, 25.3, 57.7, 30.0, '5 V', 1.5)
s.text('Micro-USB', 47.65, 22.0, 1.6, 'centre')
socket(s, 61.55, 6.9, 65.0, 23.0, 'Kamera', 1.5, turn=True)
for y0, name in ((6.3, 'Run'), (8.8, 'TV')):         # RUN next to the header, TV below it
    s.rect(52.8, y0, 57.8, y0 + 1.9, w=0.18)
    for x in (54.03, 56.57):
        s.circle(x, y0 + 0.95, 1.0, w=0.12)
    s.text(name, 52.2, y0 + 0.2, 1.5, 'right')
add(s)


def gpio_label(caption, line, value=''):
    """A label for a wire to the Raspberry Pi's header: its connection point at the middle of the left side."""
    s = Symbol('', caption, value, ask=bool(value), numbered=False, listed=False, shown=False, value_shown=True)
    s.rect(0, -3.6, 9 * P, 3.6, w=0.35).pin('1', (0, 0))
    s.text('Raspberry Pi', 1.0, -3.1, 2.5)
    if line:
        s.text(line, 1.0, 0.5, 2.5)
    s.labels((1.0, -6.4), (1.0, 0.5) if value else None)
    return s


add(gpio_label(C('Raspberry Pi GPIO (Etikett)', 'Raspberry Pi GPIO (label)', 'Raspberry Pi GPIO (étiquette)'), '',
               'GPIO Port XX'))
add(gpio_label(C('Raspberry Pi 3,3 V (Etikett)', 'Raspberry Pi 3.3 V (label)', 'Raspberry Pi 3,3 V (étiquette)'),
               'GPIO 3,3V'))
add(gpio_label(C('Raspberry Pi GND (Etikett)', 'Raspberry Pi GND (label)', 'Raspberry Pi GND (étiquette)'), 'GPIO GND'))
