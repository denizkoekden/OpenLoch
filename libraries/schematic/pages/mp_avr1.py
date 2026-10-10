# More AVR microcontrollers for the page "AVR": the ATtiny parts and the smaller ATmega parts in their DIP/SO and quad
# packages (TQFP, QFN/MLF, PLCC), drawn as package boxes like mcu.py. The pin lists follow the "Pin Configurations"
# figures of the Atmel data sheets named at each list; the quad lists are derived from the DIP list of the same data
# sheet where the figure shows the same order. DNC (do not connect) and NC pins keep the data sheet's name.

add = extend(MP, 'AVR')


def part(caption, value, pins, layout='dil'):
    add(package_box('IC', C(caption, caption, caption), value, pins, layout))


def pins_of(name):
    return next(p.pins for p in mcu.AVR if p.name == name)


def qfn20_tiny8(d):
    """The 20-pad QFN/MLF of the 8-pin ATtiny parts from their DIP list (ATtiny13 2535J, ATtiny25/45/85 2586Q)."""
    x = 'DNC'
    return [d[0], d[1], x, x, d[2], x, x, d[3], x, x, d[4], d[5], x, d[6], d[7], x, x, x, x, x]


def quad32_mega(d):
    """The TQFP-32/MLF-32 of the 28-pin ATmega parts from their DIP list (ATmega8 2486AA, ATmega48/88/168 2545W)."""
    return d[4:6] + ['GND', 'VCC', 'GND', 'VCC'] + d[8:19] + ['AVCC', 'ADC6', 'AREF', 'GND', 'ADC7'] + d[22:28] + d[0:4]


def quad44_mega(d):
    """The TQFP-44/MLF-44 of the ATmega16 and its relatives from their DIP list: pin 1 is PB5 (DIP pin 6), the pins
    17/18 and 38/39 are a second VCC/GND pair."""
    return d[5:21] + ['VCC', 'GND'] + d[21:40] + ['VCC', 'GND'] + d[0:5]


def quad44_bus(d, extra):
    """The TQFP-44 of the parts with the external memory bus (ATmega8515, ATmega162): pin 1 is PB5 (DIP pin 6), extra
    are the pins 6, 17, 28 and 39 that the DIP does not have."""
    return d[5:10] + [extra[0]] + d[10:20] + [extra[1]] + d[20:30] + [extra[2]] + d[30:40] + [extra[3]] + d[0:5]


def plcc44(tqfp):
    """PLCC-44: the data sheet numbers pin 1 in the middle of the top side, so that PLCC pin k is TQFP pin k - 6
    (ATmega8515 2512K, ATmega8535 2502K). The box counts from the top of the left side; the list is turned so that its
    pin 1 is PLCC pin 1 and the numbers on the box are the PLCC numbers."""
    return tqfp[38:] + tqfp[:38]


# ATtiny11/12: 1006FS-AVR-06/07 (summary). The ATtiny11 has no serial programming interface.
part('ATtiny11 (DIP-8/SO-8)', 'ATtiny11',
     ['PB5/~RESET', 'PB3/XTAL1', 'PB4/XTAL2', 'GND', 'PB0/AIN0', 'PB1/INT0', 'PB2/T0', 'VCC'])
part('ATtiny12 (DIP-8/SO-8)', 'ATtiny12', mcu.TINY8)

# ATtiny13: 2535J-AVR-08/10.
part('ATtiny13 (QFN-20/MLF-20)', 'ATtiny13', qfn20_tiny8(pins_of('ATtiny13A')), 'quad')

# ATtiny15L: 1187H-AVR-09/07 (pins 2 and 3 are PB4 and PB3).
part('ATtiny15 (DIP-8/SO-8)', 'ATtiny15L',
     ['PB5/~RESET', 'PB4/ADC3', 'PB3/ADC2', 'GND', 'PB0/MOSI', 'PB1/MISO', 'PB2/SCK', 'VCC'])

# ATtiny24/44/84: 8006K-AVR-10/10.
TINY14 = pins_of('ATtiny24/44/84')
TINY14_QFN = TINY14[8:13] + ['DNC', 'DNC', 'GND', 'VCC', 'DNC'] + TINY14[1:7] + ['DNC'] * 3 + [TINY14[7]]
part('ATtiny24 (QFN-20/MLF-20)', 'ATtiny24', TINY14_QFN, 'quad')

# ATtiny25/45/85: 2586Q-AVR-08/2013.
part('ATtiny25 (MLF-20)', 'ATtiny25', qfn20_tiny8(mcu.TINY8), 'quad')

# ATtiny26: 1477K-AVR-08/10.
part('ATtiny26 (DIP-20/SO-20)', 'ATtiny26',
     ['PB0/MOSI', 'PB1/MISO', 'PB2/SCK', 'PB3/OC1B', 'VCC', 'GND', 'PB4/XTAL1', 'PB5/XTAL2', 'PB6/INT0', 'PB7/~RESET',
      'PA7/ADC6', 'PA6/ADC5', 'PA5/ADC4', 'PA4/ADC3', 'AVCC', 'GND', 'PA3/AREF', 'PA2/ADC2', 'PA1/ADC1', 'PA0/ADC0'])
part('ATtiny26 (MLF-32)', 'ATtiny26',
     ['NC', 'PB3/OC1B', 'NC', 'VCC', 'GND', 'NC', 'PB4/XTAL1', 'PB5/XTAL2', 'NC', 'PB6/INT0', 'PB7/~RESET', 'NC',
      'PA7/ADC6', 'PA6/ADC5', 'PA5/ADC4', 'NC', 'PA4/ADC3', 'AVCC', 'NC', 'NC', 'GND', 'PA3/AREF', 'PA2/ADC2', 'NC',
      'PA1/ADC1', 'PA0/ADC0', 'NC', 'NC', 'NC', 'PB0/MOSI', 'PB1/MISO', 'PB2/SCK'], 'quad')

# ATtiny28L/V: 1062F-AVR-07/06 (port A in the order PA2, PA3, PA1, PA0).
part('ATtiny28 (DIP-28)', 'ATtiny28',
     ['~RESET', 'PD0', 'PD1', 'PD2', 'PD3', 'PD4', 'VCC', 'GND', 'XTAL1', 'XTAL2', 'PD5', 'PD6', 'PD7', 'PB0/AIN0',
      'PB1/AIN1', 'PB2/T0', 'PB3/INT0', 'PB4/INT1', 'PB5', 'VCC', 'NC', 'GND', 'PB6', 'PB7', 'PA2/IR', 'PA3', 'PA1',
      'PA0'])
part('ATtiny28 (TQFP-32/QFN-32/MLF-32)', 'ATtiny28',
     ['PD3', 'PD4', 'NC', 'VCC', 'GND', 'NC', 'XTAL1', 'XTAL2', 'PD5', 'PD6', 'PD7', 'PB0/AIN0', 'PB1/AIN1', 'PB2/T0',
      'PB3/INT0', 'PB4/INT1', 'PB5', 'VCC', 'NC', 'NC', 'GND', 'NC', 'PB6', 'PB7', 'PA2/IR', 'PA3', 'PA1', 'PA0',
      '~RESET', 'PD0', 'PD1', 'PD2'], 'quad')

# ATtiny2313: 2543M-AVR-10/16; the MLF starts at DIP pin 3.
TINY20 = pins_of('ATtiny2313')
part('ATtiny2313 (MLF-20)', 'ATtiny2313', TINY20[2:] + TINY20[:2], 'quad')

# ATtiny44/84 (8006K) and ATtiny45/85 (2586Q): the packages of the ATtiny24 and ATtiny25 above.
part('ATtiny44 (DIP-14/SO-14)', 'ATtiny44', TINY14)
part('ATtiny44 (QFN-20/MLF-20)', 'ATtiny44', TINY14_QFN, 'quad')
part('ATtiny45 (DIP-8/SO-8)', 'ATtiny45', mcu.TINY8)
part('ATtiny45 (MLF-20)', 'ATtiny45', qfn20_tiny8(mcu.TINY8), 'quad')
part('ATtiny84 (DIP-14/SO-14)', 'ATtiny84', TINY14)
part('ATtiny84 (QFN-20/MLF-20)', 'ATtiny84', TINY14_QFN, 'quad')
part('ATtiny85 (DIP-8/SO-8)', 'ATtiny85', mcu.TINY8)
part('ATtiny85 (MLF-20)', 'ATtiny85', qfn20_tiny8(mcu.TINY8), 'quad')

# ATmega48/88/168: 2545W (11/2016); ATmega8: 2486AA-AVR-02/2013, TQFP and MLF alike.
MEGA32 = quad32_mega(mcu.MEGA28)
part('ATmega48 (TQFP-32/MLF-32)', 'ATmega48', MEGA32, 'quad')
part('ATmega8 (TQFP-32/MLF-32)', 'ATmega8', MEGA32, 'quad')
part('ATmega88 (DIP-28)', 'ATmega88', mcu.MEGA28)
part('ATmega88 (TQFP-32)', 'ATmega88', MEGA32, 'quad')

# ATmega8515: 2512K-AVR-01/10 (with the external memory bus; the figure's "TDX" is TXD).
MEGA8515 = ['PB0/T0', 'PB1/T1', 'PB2/AIN0', 'PB3/AIN1', 'PB4/~SS', 'PB5/MOSI', 'PB6/MISO', 'PB7/SCK', '~RESET',
            'PD0/RXD', 'PD1/TXD', 'PD2/INT0', 'PD3/INT1', 'PD4/XCK', 'PD5/OC1A', 'PD6/~WR', 'PD7/~RD', 'XTAL2', 'XTAL1',
            'GND', 'PC0/A8', 'PC1/A9', 'PC2/A10', 'PC3/A11', 'PC4/A12', 'PC5/A13', 'PC6/A14', 'PC7/A15', 'PE2/OC1B',
            'PE1/ALE', 'PE0/ICP', 'PA7/AD7', 'PA6/AD6', 'PA5/AD5', 'PA4/AD4', 'PA3/AD3', 'PA2/AD2', 'PA1/AD1',
            'PA0/AD0', 'VCC']
MEGA8515_44 = quad44_bus(MEGA8515, ['NC'] * 4)
part('ATmega8515 (DIP-40)', 'ATmega8515', MEGA8515)
part('ATmega8515 (TQFP-44/MLF-44)', 'ATmega8515', MEGA8515_44, 'quad')
part('ATmega8515 (PLCC-44)', 'ATmega8515', plcc44(MEGA8515_44), 'quad')

# ATmega8535: 2502K-AVR-10/06; the pins of the ATmega16 without JTAG on PC2..PC5.
MEGA8535 = mcu.MEGA40[:23] + ['PC2', 'PC3', 'PC4', 'PC5'] + mcu.MEGA40[27:]
MEGA8535_44 = quad44_mega(MEGA8535)
part('ATmega8535 (DIP-40)', 'ATmega8535', MEGA8535)
part('ATmega8535 (TQFP-44/MLF-44)', 'ATmega8535', MEGA8535_44, 'quad')
part('ATmega8535 (PLCC-44)', 'ATmega8535', plcc44(MEGA8535_44), 'quad')

# ATmega16: 2466T-AVR-07/10.
part('ATmega16 (TQFP-44/QFN-44/MLF-44)', 'ATmega16', quad44_mega(mcu.MEGA40), 'quad')

# ATmega162: 2513L-AVR-03/2013.
MEGA162 = ['PB0/T0', 'PB1/T1', 'PB2/RXD1', 'PB3/TXD1', 'PB4/~SS', 'PB5/MOSI', 'PB6/MISO', 'PB7/SCK', '~RESET',
           'PD0/RXD0', 'PD1/TXD0', 'PD2/INT0', 'PD3/INT1', 'PD4/TOSC1', 'PD5/TOSC2', 'PD6/~WR', 'PD7/~RD', 'XTAL2',
           'XTAL1', 'GND', 'PC0/A8', 'PC1/A9', 'PC2/A10', 'PC3/A11', 'PC4/A12', 'PC5/A13', 'PC6/A14', 'PC7/A15',
           'PE2/OC1B', 'PE1/ALE', 'PE0/ICP1', 'PA7/AD7', 'PA6/AD6', 'PA5/AD5', 'PA4/AD4', 'PA3/AD3', 'PA2/AD2',
           'PA1/AD1', 'PA0/AD0', 'VCC']
part('ATmega162 (DIP-40)', 'ATmega162', MEGA162)
part('ATmega162 (TQFP-44)', 'ATmega162', quad44_bus(MEGA162, ['VCC', 'VCC', 'GND', 'GND']), 'quad')

# ATmega164P/PA: 8272G-AVR-01/2015 (ATmega164A/PA/324A/PA/644A/PA/1284/P, the same pins as the ATmega164P).
MEGA164 = ['PB0/T0', 'PB1/T1', 'PB2/INT2', 'PB3/OC0A', 'PB4/~SS', 'PB5/MOSI', 'PB6/MISO', 'PB7/SCK', '~RESET', 'VCC',
           'GND', 'XTAL2', 'XTAL1', 'PD0/RXD0', 'PD1/TXD0', 'PD2/INT0', 'PD3/INT1', 'PD4/OC1B', 'PD5/OC1A', 'PD6/ICP1',
           'PD7/OC2A', 'PC0/SCL', 'PC1/SDA', 'PC2/TCK', 'PC3/TMS', 'PC4/TDO', 'PC5/TDI', 'PC6/TOSC1', 'PC7/TOSC2',
           'AVCC', 'GND', 'AREF', 'PA7/ADC7', 'PA6/ADC6', 'PA5/ADC5', 'PA4/ADC4', 'PA3/ADC3', 'PA2/ADC2', 'PA1/ADC1',
           'PA0/ADC0']
part('ATmega164 (DIP-40)', 'ATmega164P/PA', MEGA164)
part('ATmega164 (TQFP-44)', 'ATmega164P/PA', quad44_mega(MEGA164), 'quad')

# ATmega168: 2545W, as the ATmega88.
part('ATmega168 (DIP-28)', 'ATmega168', mcu.MEGA28)
part('ATmega168 (TQFP-32)', 'ATmega168', MEGA32, 'quad')
