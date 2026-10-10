# More boards and modules for the µP page "Arduino / Shields": each a box with its pin header(s) on the 2.54 mm pitch,
# the contacts numbered 1..n, the signal names inside, drawn like package_box. Names and order of the pins after the
# makers' documentation named per module. Where the maker numbers the header, the contacts follow that numbering;
# otherwise they count down the left side, then down the right side (up with `up`, like a DIP) and then along the
# lower edge.
add = extend(MP, 'Arduino / Shields')


def header_box(prefix, caption, left, right=(), value='', *, numbered=False, up=False, width=None, bottom=()):
    """`left` and `right` hold the pin names row by row from the top (None: no pin in that row), `bottom` the names
    along the lower edge from the left; `width` is the inside width of the box."""
    assert not isinstance(right, str), caption
    rows = max(len(left), len(right), 1)
    chars = lambda pins: max([len(n.replace('~', '')) for n in pins if n] or [0])
    if width is None:
        width = max(4, math.ceil(((chars(left) + chars(right)) * 1.15 + 3.0) / P)) * P
    s = Symbol(prefix, caption, value, numbered=numbered)
    x0, x1, h = P, P + width, (rows + 1) * P
    s.rect(x0, 0, x1, h, w=0.35)
    count = [0]

    def contact(at, label, align='left'):
        count[0] += 1
        s.pin(str(count[0]), at, label=label, align=align, shown=True)

    for k, name in enumerate(left):
        if name:
            y = (k + 1) * P
            s.line((0, y), (x0, y)).text(overlined(name), x0 + 0.6, y - 1.0, 1.8)
            contact((0, y), (0.2, y - 2.2))
    for k, name in (reversed(list(enumerate(right))) if up else enumerate(right)):
        if name:
            y = (k + 1) * P
            s.line((x1, y), (x1 + P, y)).text(overlined(name), x1 - 0.6, y - 1.0, 1.8, 'right')
            contact((x1 + P, y), (x1 + P - 0.2, y - 2.2), 'right')
    for k, name in enumerate(bottom):
        x = x0 + (k + 1) * P
        s.line((x, h), (x, h + P)).text(overlined(name), x - 0.9, h - 0.6, 1.8, rotation=90)
        contact((x, h + P), (x + 0.3, h + 0.3))
    for o in s.parts:
        if o['type'] == 'contact':
            o['font'] = {'height': 1.6}
    designator = prefix + ('?' if numbered else '')
    if value and (len(designator) + len(value)) * 1.45 + 2.0 <= width:
        s.labels((x0, -3.4), (x1, -3.4), 'left', 'right')
    else:       # the value under the box, where it cannot meet a long designator (nor one typed in later)
        s.labels((x0, -3.4), (x0, h + (P + 2.2 if bottom else 0.6)))
    s.x0, s.x1, s.h = x0, x1, h
    return s


def window(s, x0, y0, x1, y1, label='', h=1.8):
    s.rect(x0, y0, x1, y1, w=0.25)
    if label:
        s.text(label, (x0 + x1) / 2, (y0 + y1) / 2 - h / 2, h, 'centre')
    return s


# WEMOS LOLIN D1 mini (ESP8266), docs www.wemos.cc: schematic V4.0.0 (J2 RST..3V3, J3 TX..5V); numbered 1..16 like the
# 16-pin header P2 in WEMOS' shield schematics (Micro SD v1.2.0, Battery v1.3.0): down the left row, up the right row.
D1_MINI = ['~RST', 'A0', 'D0', 'D5/SCK', 'D6/MISO', 'D7/MOSI', 'D8', '3V3', '5V', 'GND', 'D4', 'D3', 'D2/SDA', 'D1/SCL',
           'RX', 'TX']
s = package_box('Wemos', C('Wemos D1 mini', 'Wemos D1 mini', 'Wemos D1 mini'), 'D1 mini', D1_MINI)
s.numbered = False
add(s)

# LOLIN Micro SD Card Shield (www.wemos.cc, v1.2.0): only the pins it uses, in the places they have on the D1 mini.
s = header_box('LOLIN', C('LOLIN Micro-SD-Shield', 'LOLIN micro SD shield', 'Shield micro SD LOLIN'),
               [None, None, None, 'D5/SCK', 'D6/MISO', 'D7/MOSI', 'D8/CS', '3V3'], [None] * 6 + ['GND', None],
               'Micro SD Shield', up=True, width=8 * P)
add(window(s, s.x0 + 2.0, 0.8, s.x1 - 2.0, 3 * P + 0.2, 'microSD'))

# LOLIN Battery Shield (www.wemos.cc, v1.3.0): 5V and GND of the header, the LiPo plug PH2.0 below (1 GND, 2 VBAT); the
# charging port drawn only.
s = header_box('LOLIN', C('LOLIN Batterie-Shield', 'LOLIN battery shield', 'Shield batterie LOLIN'), [],
               [None] * 6 + ['GND', '5V'], 'Battery Shield', up=True, width=8 * P, bottom=['BAT−', 'BAT+'])
window(s, s.x0 + 2.0, 0.8, s.x1 - 2.0, 3 * P + 0.2, 'Micro-USB')
add(s.text('LiPo', 2.5 * P, s.h - 8.4, 1.8, 'centre'))

# ATmega328P (Microchip DS40002061B, 28-pin SPDIP) with the Arduino Uno pin names (Arduino Uno Rev3 schematic,
# Arduino_Uno_Rev3-schematic.pdf: D0..D13 on PD0..PD7 and PB0..PB5, A0..A5 on PC0..PC5).
MEGA328_ARDUINO = ['PC6/~RESET', 'PD0/D0', 'PD1/D1', 'PD2/D2', 'PD3/D3', 'PD4/D4', 'VCC', 'GND', 'PB6/XTAL1', 'PB7/XTAL2',
                   'PD5/D5', 'PD6/D6', 'PD7/D7', 'PB0/D8', 'PB1/D9', 'PB2/D10', 'PB3/D11', 'PB4/D12', 'PB5/D13', 'AVCC',
                   'AREF', 'GND', 'PC0/A0', 'PC1/A1', 'PC2/A2', 'PC3/A3', 'PC4/A4', 'PC5/A5']
title = 'ATmega328P mit Arduino-Pinbelegung (DIP-28)'
add(package_box('IC', C(title, 'ATmega328P with Arduino pinout (DIP-28)', 'ATmega328P avec brochage Arduino (DIP-28)'),
                'ATmega328P', MEGA328_ARDUINO))

# Common modules without a maker's pin numbering: the pins in the order and with the names printed on the boards, after
# the vendor documents named per module (chips: Bosch BME280, NXP MFRC522, NXP PCF8574 behind an HD44780 display, Maxim
# DS3231 and MAX6675, Titan Micro TM1637).
# GY-BME280 breakout (AZ-Delivery, BME-280 Barometrischer Sensor Pinout).
add(header_box('BME280', C('BME280-Modul', 'BME280 module', 'Module BME280'), ['VIN', 'GND', 'SCL', 'SDA']))

# RFID-RC522 board (Handsontec, RC522 RFID Development Kit data specs, SKU MDU1040): SDA is the SPI select.
s = header_box('RFID-Reader', C('RFID-Leser RC522', 'RFID reader RC522', 'Lecteur RFID RC522'),
               ['SDA', 'SCK', 'MOSI', 'MISO', 'IRQ', 'GND', '~RST', '3.3V'], value='RC522', width=8 * P)
for k in range(3):          # the antenna loop on the board
    d = 1.2 * k
    s.rect(s.x0 + 7.4 + d, 1.4 + d, s.x1 - 1.4 - d, s.h - 1.4 - d, w=0.18, corner=12)
add(s)

# PCF8574 backpack on a 1602 display (Handsontec, I2C Serial Interface 1602 LCD Module user guide, SKU DSP-1182).
s = header_box('LCD', C('LCD mit I²C-Adapter', 'LCD with I²C adapter', 'LCD avec adaptateur I²C'),
               ['GND', 'VCC', 'SDA', 'SCL'], value='16×2', numbered=True, width=11 * P)
wx0, wx1 = s.x0 + 6.0, s.x1 - 1.2
window(s, wx0, 1.4, wx1, s.h - 1.4)
pitch = (wx1 - wx0 - 1.2) / 16
for row in (0, 1):
    for k in range(16):
        x, y = wx0 + 0.6 + k * pitch, s.h / 2 - 2.3 + row * 2.6
        s.rect(x + 0.1, y, x + pitch - 0.1, y + 2.0, w=0.08)
add(s)

# ZS-042 board (AZ-Delivery, DS3231 Real Time Clock Datenblatt, pinout): the six pins at one end, the I²C pins led
# through to the other end.
add(header_box('IC', C('RTC-Modul DS3231', 'RTC module DS3231', 'Module RTC DS3231'),
               ['32K', 'SQW', 'SCL', 'SDA', 'VCC', 'GND'], [None, None, 'SCL', 'SDA', 'VCC', 'GND'], 'DS3231', numbered=True))

# MAX6675 board (AZ-Delivery e-book AZ175, MAX6675 temperature sensor with probe): SO, CS, SCK, VCC, GND; the screw
# terminals + and − for the thermocouple.
add(header_box('IC', C('Thermoelement-Modul MAX6675', 'Thermocouple module MAX6675', 'Module thermocouple MAX6675'),
               ['SO', '~CS', 'SCK', 'VCC', 'GND'], [None, 'T+', 'T−'], 'MAX6675', numbered=True))

# KY-040 (Joy-IT data sheet): CLK, DT, SW, +, GND.
s = header_box('Rotary encoder', C('Drehgeber-Modul', 'Rotary encoder module', 'Module codeur rotatif'),
               ['CLK', 'DT', 'SW', '+', 'GND'], value='KY-040', width=6 * P)
cx, cy = s.x1 - 4.4, s.h / 2
add(s.circle(cx, cy, 6.4, w=0.25).circle(cx, cy, 4.0, w=0.18).line((cx, cy), (cx, cy - 2.0), w=0.35))

# Capacitive soil moisture sensor v1.2 (AZ-Delivery, Hygrometer Modul V1.2 Pinout): AOUT, VCC, GND, the probe beyond.
s = header_box('Capacitive Soil', C('Bodenfeuchtesensor (kapazitiv)', 'Soil moisture sensor (capacitive)',
                                    "Capteur d'humidité du sol (capacitif)"), ['AOUT', 'VCC', 'GND'])
add(s.line((s.x1, 1.0), (s.x1 + 16.0, 1.0), (s.x1 + 20.0, s.h / 2), (s.x1 + 16.0, s.h - 1.0), (s.x1, s.h - 1.0), w=0.35))

# FT232RL adapter header DTR RX TX VCC CTS GND (the same row as SparkFun's FTDI Basic, which counts it from GND).
s = header_box('USB-Seriell-Wandler', C('USB-Seriell-Wandler (FTDI)', 'USB to serial converter (FTDI)',
                                        'Convertisseur USB-série (FTDI)'),
               ['~DTR', 'RX', 'TX', 'VCC', '~CTS', 'GND'], value='FT232RL', width=6 * P)
add(window(s, s.x1 - 5.4, s.h / 2 - 3.4, s.x1 + 1.6, s.h / 2 + 3.4).text('USB', s.x1 - 0.7, s.h / 2 - 0.8, 1.6, 'right'))

# TP4056 charger with protection (AZ-Delivery, TP4056 Type C datasheet, board picture): IN+ and IN− on either side of
# the USB socket, OUT+, B+, B−, OUT− at the other end.
s = header_box('Laderegler', C('Lademodul TP4056', 'Charger module TP4056', 'Module de charge TP4056'),
               ['IN+', None, None, 'IN−'], ['OUT+', 'B+', 'B−', 'OUT−'], 'TP4056', width=6 * P)
add(window(s, s.x0 - 1.0, 1.6 * P, s.x0 + 5.4, 3.3 * P).text('USB', s.x0 + 0.7, 2.45 * P - 0.8, 1.6))

# 18650 holder: a cell (IEC 60617) in the holder's outline, + at contact 1.
s = Symbol('Akkuhalterung', C('18650-Akkuhalter', '18650 battery holder', "Support d'accumulateur 18650"), '18650')
x0, x1, cx = P, 11 * P, 6 * P
s.rect(x0, -3.0, x1, 3.0, w=0.35)
s.line((0, 0), (cx - 0.6, 0)).line((cx - 0.6, -2.4), (cx - 0.6, 2.4))
s.line((cx + 0.6, -1.2), (cx + 0.6, 1.2), w=THICK).line((cx + 0.6, 0), (x1 + P, 0))
s.text('+', x0 + 0.8, -2.8, 2.5).text('−', x1 - 0.8, -2.8, 2.5, 'right')
s.pin('1', (0, 0), label=(0.2, -2.2), shown=True).pin('2', (x1 + P, 0), label=(x1 + P - 0.2, -2.2), align='right', shown=True)
for o in s.parts:
    if o['type'] == 'contact':
        o['font'] = {'height': 1.6}
add(s.labels((x0, -6.4), (x0, 3.6)))


def converter(s):
    """The sign of a DC/DC converter (IEC 60617) in the middle of the box."""
    cx, cy, a = (s.x0 + s.x1) / 2, s.h / 2, 2.6
    s.rect(cx - a, cy - a, cx + a, cy + a, w=0.25).line((cx - a, cy + a), (cx + a, cy - a))
    return s.text('=', cx - a + 0.4, cy - a + 0.1, 2.0).text('=', cx + a - 0.4, cy + 0.4, 2.0, 'right')


# LM2596S and MT3608 boards (AZ-Delivery, LM2596S DC-DC Step down Modul Pinout and MT3608 Step-Up Modul Pinout): IN+
# over IN− at one end, OUT+ over OUT− at the other.
add(converter(header_box('Step-Down-Modul', C('Step-Down-Modul', 'Step-down module', 'Module abaisseur (step-down)'),
                         ['IN+', 'IN−'], ['OUT+', 'OUT−'], width=8 * P)))
add(converter(header_box('Step-Up-Modul', C('Step-Up-Modul', 'Step-up module', 'Module élévateur (step-up)'),
                         ['IN+', 'IN−'], ['OUT+', 'OUT−'], width=8 * P)))

# FS1000A (Berrybase, Datenblatt zu 433Mhz Sender & Empfänger FS1000A XY-FST XY-MK-5V): Data, VCC, GND from the left,
# the ANT pad in the corner.
s = header_box('FS1000A', C('433-MHz-Sender FS1000A', '433 MHz transmitter FS1000A', 'Émetteur 433 MHz FS1000A'),
               ['DATA', 'VCC', 'GND'], width=5 * P)
add(s.circle(s.x1 - 1.8, 1.8, 1.6, w=0.25).text('ANT', s.x1 - 3.0, 1.0, 1.6, 'right'))

# CHJ-RXB6 technical data (manufacturer's sheet): 1 GND, 2 DATA, 3 DER, 4 VCC | 5 VCC, 6 GND, 7 GND, 8 ANT in one row.
add(header_box('RXB6', C('433-MHz-Empfänger RXB6', '433 MHz receiver RXB6', 'Récepteur 433 MHz RXB6'),
               ['GND', 'DATA', 'DER', 'VCC'], ['ANT', 'GND', 'GND', 'VCC'], up=True))

# Adafruit ADS1115 breakout (learn.adafruit.com, pinouts): one row VDD..A3.
add(header_box('Analog-Digital-Wandler', C('A/D-Wandler-Modul ADS1115', 'A/D converter module ADS1115',
                                           'Module convertisseur A/N ADS1115'),
               ['VDD', 'GND', 'SCL', 'SDA', 'ADDR', 'ALRT'], ['A0', 'A1', 'A2', 'A3'], 'ADS1115'))

# DS18B20 in TO-92 (Maxim data sheet, pin description): 1 GND, 2 DQ, 3 VDD.
add(header_box('Temperatursensor', C('Temperatursensor DS18B20', 'Temperature sensor DS18B20',
                                     'Capteur de température DS18B20'), ['GND', 'DQ', 'VDD'], value='DS18B20'))

# Adafruit TSL2591 breakout, product 1980 (Eagle files, adafruit/Adafruit-TSL2591-Breakout-PCB, original board).
add(header_box('Luxsensor', C('Lichtsensor TSL2591', 'Light sensor TSL2591', 'Capteur de lumière TSL2591'),
               ['VIN', 'GND', '3Vo', 'INT', 'SDA', 'SCL'], value='TSL2591'))

# TM1637 board (AZ-Delivery, 4 Bit 7-Segmentanzeige LED Display Datenblatt): CLK, DIO, VCC, GND in one row.
s = header_box('4-Digit-Display', C('4-Digit-Anzeige TM1637', '4-digit display TM1637', 'Afficheur 4 chiffres TM1637'),
               ['CLK', 'DIO', 'VCC', 'GND'], value='TM1637', width=9 * P)
add(window(s, s.x0 + 5.6, 1.4, s.x1 - 1.2, s.h - 1.4, '88:88', 5.0))

# Micro SD card module (AZ-Delivery, SPI Reader Micro Speicherkartenmodul Pinout): numbered 1 GND, 2 VCC, 3 MISO,
# 4 MOSI, 5 SCK, 6 CS.
s = header_box('MicroSD Card', C('Micro-SD-Kartenmodul', 'Micro SD card module', 'Module carte micro SD'),
               ['GND', 'VCC', 'MISO', 'MOSI', 'SCK', '~CS'], width=8 * P)
add(window(s, s.x1 - 10.0, 2.4, s.x1 + 1.6, s.h - 2.4, 'microSD', 1.6))

# Espressif ESP32-DevKitC V4 user guide: header J2 (pins 1..19) on the left, J3 (contacts 20..38) on the right, both
# counted from the module's end; the first row stays free for the header names.
J2 = ['3V3', 'EN', 'VP', 'VN', 'IO34', 'IO35', 'IO32', 'IO33', 'IO25', 'IO26', 'IO27', 'IO14', 'IO12', 'GND', 'IO13', 'D2',
      'D3', 'CMD', '5V']
J3 = ['GND', 'IO23', 'IO22', 'TX', 'RX', 'IO21', 'GND', 'IO19', 'IO18', 'IO5', 'IO17', 'IO16', 'IO4', 'IO0', 'IO2', 'IO15',
      'D1', 'D0', 'CLK']
s = header_box('Devkit v2', C('ESP32-DevKitC', 'ESP32-DevKitC', 'ESP32-DevKitC'), [None] + J2, [None] + J3, 'ESP32-DevKitC',
               width=8 * P)
s.text('J2', s.x0 + 0.6, P - 1.0, 1.8, bold=True).text('J3', s.x1 - 0.6, P - 1.0, 1.8, 'right', bold=True)
s.labels((s.x0, -3.4), (s.x0, s.h + 2.0))
add(window(s, (s.x0 + s.x1) / 2 - 3.8, s.h - 3.2, (s.x0 + s.x1) / 2 + 3.8, s.h + 1.2, 'USB', 1.6))

# HC-SR04 (ElecFreaks data sheet): Vcc, Trig, Echo, GND.
s = header_box('Ultraschallsensor', C('Ultraschallsensor HC-SR04', 'Ultrasonic sensor HC-SR04',
                                      'Capteur à ultrasons HC-SR04'), ['VCC', 'Trig', 'Echo', 'GND'], value='HC-SR04', width=11 * P)
for k, letter in enumerate(('T', 'R')):
    x = s.x0 + 11.0 + k * 10.6
    s.circle(x, s.h / 2, 9.0, w=0.25).circle(x, s.h / 2, 6.0, w=0.18).text(letter, x, s.h / 2 - 1.0, 2.0, 'centre')
add(s)

# SparkFun Bi-Directional Logic Level Converter (BOB-12009, Eagle files): HV1 HV2 HV GND HV3 HV4 opposite LV1 LV2 LV GND
# LV3 LV4.
add(header_box('Level Converter', C('Pegelwandler 4 Kanäle', 'Level shifter, 4 channels', 'Convertisseur de niveau 4 canaux'),
               ['HV1', 'HV2', 'HV', 'GND', 'HV3', 'HV4'], ['LV1', 'LV2', 'LV', 'GND', 'LV3', 'LV4'], width=6 * P))
