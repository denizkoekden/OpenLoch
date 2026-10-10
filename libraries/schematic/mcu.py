# Microcontrollers and boards of the library: for each the pins in package order (pin 1 first; "~" marks an active-low
# pin), each named by its port first and then its main other function. Every list was compared pin by pin with a
# second source (see docs/modules/schematic.md); the packages are the through-hole ones (DIP) and the boards' headers.

class Part:
    def __init__(self, name, de, en, fr, pins, layout='dil', prefix='IC'):
        """layout 'dil': pins counter-clockwise like a DIP; 'header': two rows, odd pins left, even pins right."""
        self.name, self.de, self.en, self.fr, self.pins, self.layout, self.prefix = name, de, en, fr, pins, layout, prefix


TINY8 = ['PB5/~RESET', 'PB3/XTAL1', 'PB4/XTAL2', 'GND', 'PB0/MOSI', 'PB1/MISO', 'PB2/SCK', 'VCC']
MEGA28 = ['PC6/~RESET', 'PD0/RXD', 'PD1/TXD', 'PD2/INT0', 'PD3/INT1', 'PD4/T0', 'VCC', 'GND', 'PB6/XTAL1', 'PB7/XTAL2',
          'PD5/T1', 'PD6/AIN0', 'PD7/AIN1', 'PB0/ICP1', 'PB1/OC1A', 'PB2/~SS', 'PB3/MOSI', 'PB4/MISO', 'PB5/SCK', 'AVCC',
          'AREF', 'GND', 'PC0/ADC0', 'PC1/ADC1', 'PC2/ADC2', 'PC3/ADC3', 'PC4/SDA', 'PC5/SCL']
MEGA40 = ['PB0/T0', 'PB1/T1', 'PB2/INT2', 'PB3/OC0', 'PB4/~SS', 'PB5/MOSI', 'PB6/MISO', 'PB7/SCK', '~RESET', 'VCC',
          'GND', 'XTAL2', 'XTAL1', 'PD0/RXD', 'PD1/TXD', 'PD2/INT0', 'PD3/INT1', 'PD4/OC1B', 'PD5/OC1A', 'PD6/ICP1',
          'PD7/OC2', 'PC0/SCL', 'PC1/SDA', 'PC2/TCK', 'PC3/TMS', 'PC4/TDO', 'PC5/TDI', 'PC6/TOSC1', 'PC7/TOSC2', 'AVCC',
          'GND', 'AREF', 'PA7/ADC7', 'PA6/ADC6', 'PA5/ADC5', 'PA4/ADC4', 'PA3/ADC3', 'PA2/ADC2', 'PA1/ADC1', 'PA0/ADC0']

AVR = [
    Part('ATtiny13A', 'ATtiny13A (DIP-8)', 'ATtiny13A (DIP-8)', 'ATtiny13A (DIP-8)',
         ['PB5/~RESET', 'PB3/CLKI', 'PB4', 'GND', 'PB0/MOSI', 'PB1/MISO', 'PB2/SCK', 'VCC']),
    Part('ATtiny25/45/85', 'ATtiny25/45/85 (DIP-8)', 'ATtiny25/45/85 (DIP-8)', 'ATtiny25/45/85 (DIP-8)', TINY8),
    Part('ATtiny24/44/84', 'ATtiny24/44/84 (DIP-14)', 'ATtiny24/44/84 (DIP-14)', 'ATtiny24/44/84 (DIP-14)',
         ['VCC', 'PB0/XTAL1', 'PB1/XTAL2', 'PB3/~RESET', 'PB2/INT0', 'PA7/ADC7', 'PA6/MOSI', 'PA5/MISO', 'PA4/SCK',
          'PA3/ADC3', 'PA2/ADC2', 'PA1/ADC1', 'PA0/ADC0', 'GND']),
    Part('ATtiny2313', 'ATtiny2313 (DIP-20)', 'ATtiny2313 (DIP-20)', 'ATtiny2313 (DIP-20)',
         ['PA2/~RESET', 'PD0/RXD', 'PD1/TXD', 'PA1/XTAL2', 'PA0/XTAL1', 'PD2/INT0', 'PD3/INT1', 'PD4/T0', 'PD5/T1', 'GND',
          'PD6/ICP', 'PB0/AIN0', 'PB1/AIN1', 'PB2/OC0A', 'PB3/OC1A', 'PB4/OC1B', 'PB5/MOSI', 'PB6/MISO', 'PB7/SCK', 'VCC']),
    Part('ATmega8', 'ATmega8 (DIP-28)', 'ATmega8 (DIP-28)', 'ATmega8 (DIP-28)', MEGA28),   # pin 22 GND as in Atmel 2486AA
    Part('ATmega48/88/168/328', 'ATmega48/88/168/328 (DIP-28)', 'ATmega48/88/168/328 (DIP-28)', 'ATmega48/88/168/328 (DIP-28)', MEGA28),
    Part('ATmega16/32', 'ATmega16/32 (DIP-40)', 'ATmega16/32 (DIP-40)', 'ATmega16/32 (DIP-40)', MEGA40),
]

PIC = [
    Part('PIC12F675', 'PIC12F675 (DIP-8)', 'PIC12F675 (DIP-8)', 'PIC12F675 (DIP-8)',
         ['VDD', 'GP5/OSC1', 'GP4/OSC2', 'GP3/~MCLR', 'GP2/AN2', 'GP1/AN1', 'GP0/AN0', 'VSS']),
    Part('PIC16F84A', 'PIC16F84A (DIP-18)', 'PIC16F84A (DIP-18)', 'PIC16F84A (DIP-18)',
         ['RA2', 'RA3', 'RA4/T0CKI', '~MCLR', 'VSS', 'RB0/INT', 'RB1', 'RB2', 'RB3', 'RB4', 'RB5', 'RB6', 'RB7', 'VDD',
          'OSC2', 'OSC1', 'RA0', 'RA1']),
    Part('PIC16F628A', 'PIC16F628A (DIP-18)', 'PIC16F628A (DIP-18)', 'PIC16F628A (DIP-18)',
         ['RA2/AN2', 'RA3/AN3', 'RA4/T0CKI', 'RA5/~MCLR', 'VSS', 'RB0/INT', 'RB1/RX', 'RB2/TX', 'RB3/CCP1', 'RB4/PGM',
          'RB5', 'RB6/PGC', 'RB7/PGD', 'VDD', 'RA6/OSC2', 'RA7/OSC1', 'RA0/AN0', 'RA1/AN1']),
    Part('PIC16F877A', 'PIC16F877A (DIP-40)', 'PIC16F877A (DIP-40)', 'PIC16F877A (DIP-40)',
         ['~MCLR', 'RA0/AN0', 'RA1/AN1', 'RA2/AN2', 'RA3/AN3', 'RA4/T0CKI', 'RA5/AN4', 'RE0/AN5', 'RE1/AN6', 'RE2/AN7',
          'VDD', 'VSS', 'OSC1', 'OSC2', 'RC0/T1OSO', 'RC1/CCP2', 'RC2/CCP1', 'RC3/SCK', 'RD0', 'RD1', 'RD2', 'RD3',
          'RC4/SDI', 'RC5/SDO', 'RC6/TX', 'RC7/RX', 'RD4', 'RD5', 'RD6', 'RD7', 'VSS', 'VDD', 'RB0/INT', 'RB1', 'RB2',
          'RB3/PGM', 'RB4', 'RB5', 'RB6/PGC', 'RB7/PGD']),
]

RASPBERRY = [
    Part('Raspberry Pi GPIO', 'Raspberry Pi GPIO-Leiste (40 Pins)', 'Raspberry Pi GPIO header (40 pins)',
         'Connecteur GPIO Raspberry Pi (40 broches)',
         ['3V3', '5V', 'GPIO2/SDA', '5V', 'GPIO3/SCL', 'GND', 'GPIO4', 'GPIO14/TXD', 'GND', 'GPIO15/RXD', 'GPIO17',
          'GPIO18', 'GPIO27', 'GND', 'GPIO22', 'GPIO23', '3V3', 'GPIO24', 'GPIO10/MOSI', 'GND', 'GPIO9/MISO', 'GPIO25',
          'GPIO11/SCLK', 'GPIO8/CE0', 'GND', 'GPIO7/CE1', 'ID_SD', 'ID_SC', 'GPIO5', 'GND', 'GPIO6', 'GPIO12', 'GPIO13',
          'GND', 'GPIO19', 'GPIO16', 'GPIO26', 'GPIO20', 'GND', 'GPIO21'],
         layout='header', prefix='J'),
]

ARDUINO = [
    Part('Arduino Nano', 'Arduino Nano', 'Arduino Nano', 'Arduino Nano',
         ['D1/TX', 'D0/RX', '~RESET', 'GND', 'D2', 'D3', 'D4', 'D5', 'D6', 'D7', 'D8', 'D9', 'D10', 'D11/MOSI',
          'D12/MISO', 'D13/SCK', '3V3', 'AREF', 'A0', 'A1', 'A2', 'A3', 'A4/SDA', 'A5/SCL', 'A6', 'A7', '5V', '~RESET',
          'GND', 'VIN'], prefix='A'),
]
