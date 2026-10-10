# Page "AVR", part 2: the larger ATmega (40-pin DIP, TQFP-44 to TQFP-100), the AT90CAN, AT90PWM and AT90USB families and
# the 2.4 GHz transceiver AT86RF230, drawn as package boxes like mcu.py. The pins follow the "Pin Configurations" figure
# of the Atmel/Microchip data sheet named at each list, in package order; each pin is named by its port and then its
# main other function (pin-change interrupts PCINTn left out, the dots of the AT90USB figure dropped, its SCLK written
# SCK and the ICP of the ATmega324/644 ICP1, as their registers call them), "~" before an active-low name; DNC (do not
# connect) and NC as the data sheet writes them. The exposed pad of QFN/MLF is no pin.
add = extend(MP, 'AVR')


def part(name, package, pins, layout='quad'):
    caption = f'{name} ({package})'
    add(package_box('IC', C(caption, caption, caption), name, pins, layout))


def tqfp44(dip):
    """The TQFP-44/QFN-44 of a 40-pin megaAVR: DIP pins 6 to 21, a second VCC/GND pair, DIP pins 22 to 40, a third
    pair, DIP pins 1 to 5."""
    return dip[5:21] + ['VCC', 'GND'] + dip[21:] + ['VCC', 'GND'] + dip[:5]


# ATmega32: Atmel 2503Q-AVR-02/11, Figure 1 (PDIP and TQFP/MLF). The DIP is the pin list of mcu.py's ATmega16/32,
# checked again against this figure.
MEGA32 = mcu.MEGA40

# ATmega324A/PA: Microchip DS40002070B, Figure 1-1; ATmega644: Atmel 2593O-AVR-02/12, Figure 1-1 (same pins; the
# functions named are those both have).
MEGA644 = ['PB0/T0', 'PB1/T1', 'PB2/INT2', 'PB3/OC0A', 'PB4/~SS', 'PB5/MOSI', 'PB6/MISO', 'PB7/SCK', '~RESET', 'VCC',
           'GND', 'XTAL2', 'XTAL1', 'PD0/RXD0', 'PD1/TXD0', 'PD2/INT0', 'PD3/INT1', 'PD4/OC1B', 'PD5/OC1A', 'PD6/ICP1',
           'PD7/OC2A', 'PC0/SCL', 'PC1/SDA', 'PC2/TCK', 'PC3/TMS', 'PC4/TDO', 'PC5/TDI', 'PC6/TOSC1', 'PC7/TOSC2',
           'AVCC', 'GND', 'AREF', 'PA7/ADC7', 'PA6/ADC6', 'PA5/ADC5', 'PA4/ADC4', 'PA3/ADC3', 'PA2/ADC2', 'PA1/ADC1',
           'PA0/ADC0']

# ATmega325/645: Atmel 2570N-AVR-05/11, Figure 1-2. ATmega165 (2573G-AVR-07/09, Figure 1): the same, but pin 20 is
# RESET only.
MEGA325 = ['DNC', 'PE0/RXD', 'PE1/TXD', 'PE2/XCK', 'PE3/AIN1', 'PE4/USCK', 'PE5/DI', 'PE6/DO', 'PE7/CLKO', 'PB0/~SS',
           'PB1/SCK', 'PB2/MOSI', 'PB3/MISO', 'PB4/OC0A', 'PB5/OC1A', 'PB6/OC1B', 'PB7/OC2A', 'PG3/T1', 'PG4/T0',
           'PG5/~RESET', 'VCC', 'GND', 'XTAL2', 'XTAL1', 'PD0/ICP1', 'PD1/INT0', 'PD2', 'PD3', 'PD4', 'PD5', 'PD6',
           'PD7', 'PG0', 'PG1', 'PC0', 'PC1', 'PC2', 'PC3', 'PC4', 'PC5', 'PC6', 'PC7', 'PG2', 'PA7', 'PA6', 'PA5',
           'PA4', 'PA3', 'PA2', 'PA1', 'PA0', 'VCC', 'GND', 'PF7/ADC7', 'PF6/ADC6', 'PF5/ADC5', 'PF4/ADC4', 'PF3/ADC3',
           'PF2/ADC2', 'PF1/ADC1', 'PF0/ADC0', 'AREF', 'GND', 'AVCC']
MEGA165 = MEGA325[:19] + ['~RESET'] + MEGA325[20:]

# ATmega329/649 with LCD driver: Atmel 2552K-AVR-04/11, Figure 1-2. ATmega169 (2514P-AVR-07/06, Figure 1): the same,
# but pin 20 is RESET only.
MEGA329 = ['LCDCAP', 'PE0/RXD', 'PE1/TXD', 'PE2/XCK', 'PE3/AIN1', 'PE4/USCK', 'PE5/DI', 'PE6/DO', 'PE7/CLKO', 'PB0/~SS',
           'PB1/SCK', 'PB2/MOSI', 'PB3/MISO', 'PB4/OC0A', 'PB5/OC1A', 'PB6/OC1B', 'PB7/OC2A', 'PG3/T1', 'PG4/T0',
           'PG5/~RESET', 'VCC', 'GND', 'XTAL2', 'XTAL1', 'PD0/ICP1', 'PD1/INT0', 'PD2/SEG20', 'PD3/SEG19', 'PD4/SEG18',
           'PD5/SEG17', 'PD6/SEG16', 'PD7/SEG15', 'PG0/SEG14', 'PG1/SEG13', 'PC0/SEG12', 'PC1/SEG11', 'PC2/SEG10',
           'PC3/SEG9', 'PC4/SEG8', 'PC5/SEG7', 'PC6/SEG6', 'PC7/SEG5', 'PG2/SEG4', 'PA7/SEG3', 'PA6/SEG2', 'PA5/SEG1',
           'PA4/SEG0', 'PA3/COM3', 'PA2/COM2', 'PA1/COM1', 'PA0/COM0', 'VCC', 'GND', 'PF7/ADC7', 'PF6/ADC6', 'PF5/ADC5',
           'PF4/ADC4', 'PF3/ADC3', 'PF2/ADC2', 'PF1/ADC1', 'PF0/ADC0', 'AREF', 'GND', 'AVCC']
MEGA169 = MEGA329[:19] + ['~RESET'] + MEGA329[20:]

# ATmega3250/6450: Atmel 2570N-AVR-05/11, Figure 1-1.
MEGA3250 = ['DNC', 'PE0/RXD', 'PE1/TXD', 'PE2/XCK', 'PE3/AIN1', 'PE4/USCK', 'PE5/DI', 'PE6/DO', 'PE7/CLKO', 'VCC',
            'GND', 'DNC', 'PJ0', 'PJ1', 'DNC', 'DNC', 'DNC', 'DNC', 'PB0/~SS', 'PB1/SCK', 'PB2/MOSI', 'PB3/MISO',
            'PB4/OC0A', 'PB5/OC1A', 'PB6/OC1B', 'PB7/OC2A', 'DNC', 'PG3/T1', 'PG4/T0', 'PG5/~RESET', 'VCC', 'GND',
            'XTAL2', 'XTAL1', 'DNC', 'DNC', 'PJ2', 'PJ3', 'PJ4', 'PJ5', 'PJ6', 'DNC', 'PD0/ICP1', 'PD1/INT0', 'PD2',
            'PD3', 'PD4', 'PD5', 'PD6', 'PD7', 'PG0', 'PG1', 'PC0', 'PC1', 'PC2', 'PC3', 'PC4', 'PC5', 'DNC', 'DNC',
            'DNC', 'DNC', 'PH0', 'PH1', 'PH2', 'PH3', 'DNC', 'PC6', 'PC7', 'PG2', 'PA7', 'PA6', 'PA5', 'PA4', 'PA3',
            'PA2', 'PA1', 'PA0', 'DNC', 'VCC', 'GND', 'DNC', 'DNC', 'PH4', 'PH5', 'PH6', 'PH7', 'DNC', 'DNC',
            'PF7/ADC7', 'PF6/ADC6', 'PF5/ADC5', 'PF4/ADC4', 'PF3/ADC3', 'PF2/ADC2', 'PF1/ADC1', 'PF0/ADC0', 'AREF',
            'AGND', 'AVCC']

# ATmega3290/6490: Atmel 2552K-AVR-04/11, Figure 1-1.
MEGA3290 = ['LCDCAP', 'PE0/RXD', 'PE1/TXD', 'PE2/XCK', 'PE3/AIN1', 'PE4/USCK', 'PE5/DI', 'PE6/DO', 'PE7/CLKO', 'VCC',
            'GND', 'DNC', 'PJ0/SEG35', 'PJ1/SEG34', 'DNC', 'DNC', 'DNC', 'DNC', 'PB0/~SS', 'PB1/SCK', 'PB2/MOSI',
            'PB3/MISO', 'PB4/OC0A', 'PB5/OC1A', 'PB6/OC1B', 'PB7/OC2A', 'DNC', 'PG3/T1', 'PG4/T0', 'PG5/~RESET', 'VCC',
            'GND', 'XTAL2', 'XTAL1', 'DNC', 'DNC', 'PJ2/SEG31', 'PJ3/SEG30', 'PJ4/SEG29', 'PJ5/SEG28', 'PJ6/SEG27',
            'DNC', 'PD0/ICP1', 'PD1/INT0', 'PD2/SEG24', 'PD3/SEG23', 'PD4/SEG22', 'PD5/SEG21', 'PD6/SEG20', 'PD7/SEG19',
            'PG0/SEG18', 'PG1/SEG17', 'PC0/SEG16', 'PC1/SEG15', 'PC2/SEG14', 'PC3/SEG13', 'PC4/SEG12', 'PC5/SEG11',
            'DNC', 'DNC', 'DNC', 'DNC', 'PH0/SEG10', 'PH1/SEG9', 'PH2/SEG8', 'PH3/SEG7', 'DNC', 'PC6/SEG6', 'PC7/SEG5',
            'PG2/SEG4', 'PA7/SEG3', 'PA6/SEG2', 'PA5/SEG1', 'PA4/SEG0', 'PA3/COM3', 'PA2/COM2', 'PA1/COM1', 'PA0/COM0',
            'DNC', 'VCC', 'GND', 'DNC', 'DNC', 'PH4/SEG39', 'PH5/SEG38', 'PH6/SEG37', 'PH7/SEG36', 'DNC', 'DNC',
            'PF7/ADC7', 'PF6/ADC6', 'PF5/ADC5', 'PF4/ADC4', 'PF3/ADC3', 'PF2/ADC2', 'PF1/ADC1', 'PF0/ADC0', 'AREF',
            'AGND', 'AVCC']

# ATmega406 battery management: Atmel 2548F-AVR-03/2013, Figure 1-1.
MEGA406 = ['SGND', 'PA0/ADC0', 'PA1/ADC1', 'PA2/ADC2', 'PA3/ADC3', 'VREG', 'VCC', 'GND', 'PA4/ADC4', 'PA5/INT1',
           'PA6/INT2', 'PA7/INT3', '~RESET', 'XTAL1', 'XTAL2', 'GND', 'PB0/TDO', 'PB1/TDI', 'PB2/TMS', 'PB3/TCK', 'PB4',
           'PB5', 'SCL', 'SDA', 'PB6/OC0A', 'PB7/OC0B', 'PD0/T0', 'PD1', 'GND', 'PC0', 'BATT', 'OPC', 'OC', 'VFET',
           'OD', 'PVT', 'GND', 'PV4', 'PV3', 'PV2', 'PV1', 'NV', 'VREF', 'VREFGND', 'PPI', 'PI', 'NI', 'NNI']

# ATmega64: Atmel 2490R-AVR-02/2013, Figure 1; ATmega128: 2467X-AVR-06/11, Figure 1 (the same pins).
MEGA128 = ['~PEN', 'PE0/RXD0', 'PE1/TXD0', 'PE2/XCK0', 'PE3/OC3A', 'PE4/OC3B', 'PE5/OC3C', 'PE6/T3', 'PE7/ICP3',
           'PB0/~SS', 'PB1/SCK', 'PB2/MOSI', 'PB3/MISO', 'PB4/OC0', 'PB5/OC1A', 'PB6/OC1B', 'PB7/OC2', 'PG3/TOSC2',
           'PG4/TOSC1', '~RESET', 'VCC', 'GND', 'XTAL2', 'XTAL1', 'PD0/SCL', 'PD1/SDA', 'PD2/RXD1', 'PD3/TXD1',
           'PD4/ICP1', 'PD5/XCK1', 'PD6/T1', 'PD7/T2', 'PG0/~WR', 'PG1/~RD', 'PC0/A8', 'PC1/A9', 'PC2/A10', 'PC3/A11',
           'PC4/A12', 'PC5/A13', 'PC6/A14', 'PC7/A15', 'PG2/ALE', 'PA7/AD7', 'PA6/AD6', 'PA5/AD5', 'PA4/AD4', 'PA3/AD3',
           'PA2/AD2', 'PA1/AD1', 'PA0/AD0', 'VCC', 'GND', 'PF7/ADC7', 'PF6/ADC6', 'PF5/ADC5', 'PF4/ADC4', 'PF3/ADC3',
           'PF2/ADC2', 'PF1/ADC1', 'PF0/ADC0', 'AREF', 'GND', 'AVCC']

# ATmega640/1280/2560: Atmel 2549Q-AVR-02/2014, Figure 1-1.
MEGA2560 = ['PG5/OC0B', 'PE0/RXD0', 'PE1/TXD0', 'PE2/XCK0', 'PE3/OC3A', 'PE4/OC3B', 'PE5/OC3C', 'PE6/T3', 'PE7/ICP3',
            'VCC', 'GND', 'PH0/RXD2', 'PH1/TXD2', 'PH2/XCK2', 'PH3/OC4A', 'PH4/OC4B', 'PH5/OC4C', 'PH6/OC2B', 'PB0/~SS',
            'PB1/SCK', 'PB2/MOSI', 'PB3/MISO', 'PB4/OC2A', 'PB5/OC1A', 'PB6/OC1B', 'PB7/OC0A', 'PH7/T4', 'PG3/TOSC2',
            'PG4/TOSC1', '~RESET', 'VCC', 'GND', 'XTAL2', 'XTAL1', 'PL0/ICP4', 'PL1/ICP5', 'PL2/T5', 'PL3/OC5A',
            'PL4/OC5B', 'PL5/OC5C', 'PL6', 'PL7', 'PD0/SCL', 'PD1/SDA', 'PD2/RXD1', 'PD3/TXD1', 'PD4/ICP1', 'PD5/XCK1',
            'PD6/T1', 'PD7/T0', 'PG0/~WR', 'PG1/~RD', 'PC0/A8', 'PC1/A9', 'PC2/A10', 'PC3/A11', 'PC4/A12', 'PC5/A13',
            'PC6/A14', 'PC7/A15', 'VCC', 'GND', 'PJ0/RXD3', 'PJ1/TXD3', 'PJ2/XCK3', 'PJ3', 'PJ4', 'PJ5', 'PJ6',
            'PG2/ALE', 'PA7/AD7', 'PA6/AD6', 'PA5/AD5', 'PA4/AD4', 'PA3/AD3', 'PA2/AD2', 'PA1/AD1', 'PA0/AD0', 'PJ7',
            'VCC', 'GND', 'PK7/ADC15', 'PK6/ADC14', 'PK5/ADC13', 'PK4/ADC12', 'PK3/ADC11', 'PK2/ADC10', 'PK1/ADC9',
            'PK0/ADC8', 'PF7/ADC7', 'PF6/ADC6', 'PF5/ADC5', 'PF4/ADC4', 'PF3/ADC3', 'PF2/ADC2', 'PF1/ADC1', 'PF0/ADC0',
            'AREF', 'GND', 'AVCC']

# ATmega1281/2561: Atmel 2549Q-AVR-02/2014, Figure 1-3.
MEGA2561 = ['PG5/OC0B', 'PE0/RXD0', 'PE1/TXD0', 'PE2/XCK0', 'PE3/OC3A', 'PE4/OC3B', 'PE5/OC3C', 'PE6/T3', 'PE7/ICP3',
            'PB0/~SS', 'PB1/SCK', 'PB2/MOSI', 'PB3/MISO', 'PB4/OC2A', 'PB5/OC1A', 'PB6/OC1B', 'PB7/OC0A', 'PG3/TOSC2',
            'PG4/TOSC1', '~RESET', 'VCC', 'GND', 'XTAL2', 'XTAL1', 'PD0/SCL', 'PD1/SDA', 'PD2/RXD1', 'PD3/TXD1',
            'PD4/ICP1', 'PD5/XCK1', 'PD6/T1', 'PD7/T0', 'PG0/~WR', 'PG1/~RD', 'PC0/A8', 'PC1/A9', 'PC2/A10', 'PC3/A11',
            'PC4/A12', 'PC5/A13', 'PC6/A14', 'PC7/A15', 'PG2/ALE', 'PA7/AD7', 'PA6/AD6', 'PA5/AD5', 'PA4/AD4',
            'PA3/AD3', 'PA2/AD2', 'PA1/AD1', 'PA0/AD0', 'VCC', 'GND', 'PF7/ADC7', 'PF6/ADC6', 'PF5/ADC5', 'PF4/ADC4',
            'PF3/ADC3', 'PF2/ADC2', 'PF1/ADC1', 'PF0/ADC0', 'AREF', 'GND', 'AVCC']

# AT86RF230: Atmel 5131E-MCU Wireless-02/09, Figure 1-1 (the exposed paddle is AVSS).
RF230 = ['AVSS', 'AVSS', 'AVSS', 'RFP', 'RFN', 'AVSS', 'TST', '~RST', 'DVSS', 'DVSS', 'SLP_TR', 'DVSS', 'DVDD', 'DVDD',
         'DEVDD', 'DVSS', 'CLKM', 'DVSS', 'SCLK', 'MISO', 'DVSS', 'MOSI', '~SEL', 'IRQ', 'XTAL1', 'XTAL2', 'AVSS',
         'EVDD', 'AVDD', 'AVSS', 'AVSS', 'AVSS']

# AT90CAN32/64/128: Atmel 7679H-CAN-08/08, Figures 1-2 (TQFP) and 1-3 (QFN, the same pins).
CAN128 = ['NC', 'PE0/RXD0', 'PE1/TXD0', 'PE2/XCK0', 'PE3/OC3A', 'PE4/OC3B', 'PE5/OC3C', 'PE6/T3', 'PE7/ICP3', 'PB0/~SS',
          'PB1/SCK', 'PB2/MOSI', 'PB3/MISO', 'PB4/OC2A', 'PB5/OC1A', 'PB6/OC1B', 'PB7/OC0A', 'PG3/TOSC2', 'PG4/TOSC1',
          '~RESET', 'VCC', 'GND', 'XTAL2', 'XTAL1', 'PD0/SCL', 'PD1/SDA', 'PD2/RXD1', 'PD3/TXD1', 'PD4/ICP1',
          'PD5/TXCAN', 'PD6/RXCAN', 'PD7/T0', 'PG0/~WR', 'PG1/~RD', 'PC0/A8', 'PC1/A9', 'PC2/A10', 'PC3/A11', 'PC4/A12',
          'PC5/A13', 'PC6/A14', 'PC7/A15', 'PG2/ALE', 'PA7/AD7', 'PA6/AD6', 'PA5/AD5', 'PA4/AD4', 'PA3/AD3', 'PA2/AD2',
          'PA1/AD1', 'PA0/AD0', 'VCC', 'GND', 'PF7/ADC7', 'PF6/ADC6', 'PF5/ADC5', 'PF4/ADC4', 'PF3/ADC3', 'PF2/ADC2',
          'PF1/ADC1', 'PF0/ADC0', 'AREF', 'GND', 'AVCC']

# AT90PWM1: Atmel 4378C-AVR-09/08, Figure 3-1. AT90PWM2/3: 4317K-AVR-03/2013, Figures 3-1 (SO-24), 3-2 (SO-32) and
# 3-3 (QFN-32).
PWM1 = ['PD0/PSCOUT00', 'PE0/~RESET', 'PD1/PSCIN0', 'PD2/PSCIN2', 'PD3/OC0A', 'VCC', 'GND', 'PB0/MISO', 'PB1/MOSI',
        'PE1/XTAL1', 'PE2/XTAL2', 'PD4/ADC1', 'PD5/ADC2', 'PD6/ADC3', 'PD7/ACMP0', 'PB2/ADC5', 'AVCC', 'AGND', 'AREF',
        'PB3/AMP0-', 'PB4/AMP0+', 'PB5/ADC6', 'PB6/ADC7', 'PB7/ADC4']
PWM2 = ['PD0/PSCOUT00', 'PE0/~RESET', 'PD1/PSCIN0', 'PD2/PSCIN2', 'PD3/TXD', 'VCC', 'GND', 'PB0/MISO', 'PB1/MOSI',
        'PE1/XTAL1', 'PE2/XTAL2', 'PD4/RXD', 'PD5/ADC2', 'PD6/ADC3', 'PD7/ACMP0', 'PB2/ADC5', 'AVCC', 'AGND', 'AREF',
        'PB3/AMP0-', 'PB4/AMP0+', 'PB5/ADC6', 'PB6/ADC7', 'PB7/ADC4']
PWM3 = ['PD0/PSCOUT00', 'PC0/INT3', 'PE0/~RESET', 'PD1/PSCIN0', 'PD2/PSCIN2', 'PD3/TXD', 'PC1/PSCIN1', 'VCC', 'GND',
        'PC2/T0', 'PC3/T1', 'PB0/MISO', 'PB1/MOSI', 'PE1/XTAL1', 'PE2/XTAL2', 'PD4/RXD', 'PD5/ADC2', 'PD6/ADC3',
        'PD7/ACMP0', 'PB2/ADC5', 'PC4/ADC8', 'PC5/ADC9', 'AVCC', 'AGND', 'AREF', 'PC6/ADC10', 'PB3/AMP0-', 'PB4/AMP0+',
        'PC7/D2A', 'PB5/ADC6', 'PB6/ADC7', 'PB7/ADC4']
PWM3_QFN = ['PD2/PSCIN2', 'PD3/TXD', 'PC1/PSCIN1', 'VCC', 'GND', 'PC2/T0', 'PC3/T1', 'PB0/MISO', 'PB1/MOSI',
            'PE1/XTAL1', 'PE2/XTAL2', 'PD4/RXD', 'PD5/ADC2', 'PD6/ADC3', 'PD7/ACMP0', 'PB2/ADC5', 'PC4/ADC8',
            'PC5/ADC9', 'AVCC', 'AGND', 'AREF', 'PC6/ADC10', 'PB3/AMP0-', 'PB4/AMP0+', 'PC7/D2A', 'PB5/ADC6',
            'PB6/ADC7', 'PB7/ADC4', 'PD0/PSCOUT00', 'PC0/INT3', 'PE0/~RESET', 'PD1/PSCIN0']

# AT90USB646/647/1286/1287: Atmel 7593L-AVR-09/12, Figures 1-1 (TQFP) and 1-2 (QFN, the same pins).
USB1287 = ['PE6/INT6', 'PE7/INT7', 'UVCC', 'D-', 'D+', 'UGND', 'UCAP', 'VBUS', 'PE3/UID', 'PB0/~SS', 'PB1/SCK',
           'PB2/MOSI', 'PB3/MISO', 'PB4/OC2A', 'PB5/OC1A', 'PB6/OC1B', 'PB7/OC0A', 'PE4/INT4', 'PE5/INT5', '~RESET',
           'VCC', 'GND', 'XTAL2', 'XTAL1', 'PD0/SCL', 'PD1/SDA', 'PD2/RXD1', 'PD3/TXD1', 'PD4/ICP1', 'PD5/XCK1',
           'PD6/T1', 'PD7/T0', 'PE0/~WR', 'PE1/~RD', 'PC0/A8', 'PC1/A9', 'PC2/A10', 'PC3/A11', 'PC4/A12', 'PC5/A13',
           'PC6/A14', 'PC7/A15', 'PE2/ALE', 'PA7/AD7', 'PA6/AD6', 'PA5/AD5', 'PA4/AD4', 'PA3/AD3', 'PA2/AD2', 'PA1/AD1',
           'PA0/AD0', 'VCC', 'GND', 'PF7/ADC7', 'PF6/ADC6', 'PF5/ADC5', 'PF4/ADC4', 'PF3/ADC3', 'PF2/ADC2', 'PF1/ADC1',
           'PF0/ADC0', 'AREF', 'GND', 'AVCC']

part('ATmega165', 'TQFP-64', MEGA165)
part('ATmega169', 'TQFP-64', MEGA169)
part('ATmega32', 'TQFP-44/QFN-44/MLF-44', tqfp44(MEGA32))
part('ATmega32', 'DIP-40', MEGA32, 'dil')
part('ATmega324', 'DIP-40', MEGA644, 'dil')
part('ATmega324', 'TQFP-44', tqfp44(MEGA644))
part('ATmega325', 'TQFP-64', MEGA325)
part('ATmega3250', 'TQFP-100', MEGA3250)
part('ATmega329', 'TQFP-64', MEGA329)
part('ATmega3290', 'TQFP-100', MEGA3290)
part('ATmega406', 'TQFP-48', MEGA406)
part('ATmega64', 'TQFP-64', MEGA128)
part('ATmega640', 'TQFP-100', MEGA2560)
part('ATmega644', 'DIP-40', MEGA644, 'dil')
part('ATmega644', 'TQFP-44', tqfp44(MEGA644))
part('ATmega645', 'TQFP-64', MEGA325)
part('ATmega6450', 'TQFP-100', MEGA3250)
part('ATmega649', 'TQFP-64', MEGA329)
part('ATmega6490', 'TQFP-100', MEGA3290)
part('ATmega128', 'TQFP-64', MEGA128)
part('ATmega1280', 'TQFP-100', MEGA2560)
part('ATmega1281', 'TQFP-64', MEGA2561)
part('ATmega2560', 'TQFP-100', MEGA2560)
part('ATmega2561', 'TQFP-64', MEGA2561)
part('AT86RF230', 'QFN-32', RF230)
part('AT90CAN32', 'TQFP-64/QFN-64', CAN128)
part('AT90CAN64', 'TQFP-64/QFN-64', CAN128)
part('AT90CAN128', 'TQFP-64/QFN-64', CAN128)
part('AT90PWM1', 'SO-24', PWM1, 'dil')
part('AT90PWM2', 'SO-24', PWM2, 'dil')
part('AT90PWM3', 'SO-32', PWM3, 'dil')
part('AT90PWM3', 'QFN-32', PWM3_QFN)
part('AT90USB646', 'QFN-64', USB1287)
part('AT90USB647', 'TQFP-64/QFN-64', USB1287)
part('AT90USB1286', 'QFN-64', USB1287)
part('AT90USB1287', 'TQFP-64/QFN-64', USB1287)
