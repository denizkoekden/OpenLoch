# The 74xx ICs of the library: for each the pins in DIL order (pin 1 first; "~" marks an active-low pin, "NC" an
# unconnected one) and how it splits into children (gates or function blocks) under a parent with the supply pins.
# One list per IC is the only source of its pin numbers: the DIL box and the children are made from it. The pin lists
# follow the manufacturers' data sheets; how each was checked is noted at the end (DATASHEET, UNVERIFIED; see
# docs/modules/schematic.md).

class Gates:
    """`count` gates of one kind: sign after IEC 60617-12, negated output, inputs, and a mark at the output
    ('oc' open collector, 'buf' buffer, 'ocbuf', 'tri' three-state, 'schmitt')."""

    def __init__(self, sign, negated, inputs, count, mark=None, de='', en='', fr=''):
        self.sign, self.negated, self.inputs, self.count, self.mark = sign, negated, inputs, count, mark
        self.de, self.en, self.fr = de, en, fr

    def units(self):
        letters = 'ABCDEFGHIJKLM'
        if self.count == 1:
            return [([letters[k] for k in range(self.inputs)], 'Y')]
        return [([f'{u}{letters[k]}' for k in range(self.inputs)], f'{u}Y') for u in range(1, self.count + 1)]


class Block:
    """A function block as a child: its inputs (left) and outputs (right) by pin name."""

    def __init__(self, de, en, fr, inputs, outputs):
        self.de, self.en, self.fr, self.inputs, self.outputs = de, en, fr, inputs, outputs


class Chip:
    def __init__(self, number, de, en, fr, pins, children):
        self.number, self.de, self.en, self.fr, self.pins, self.children = number, de, en, fr, pins, children


# Pin layouts shared by several ICs.
QUAD_A = ['1A', '1B', '1Y', '2A', '2B', '2Y', 'GND', '3Y', '3A', '3B', '4Y', '4A', '4B', 'VCC']      # 7400
QUAD_Y = ['1Y', '1A', '1B', '2Y', '2A', '2B', 'GND', '3A', '3B', '3Y', '4A', '4B', '4Y', 'VCC']      # 7402
HEX = ['1A', '1Y', '2A', '2Y', '3A', '3Y', 'GND', '4Y', '4A', '5Y', '5A', '6Y', '6A', 'VCC']         # 7404
TRIPLE = ['1A', '1B', '2A', '2B', '2C', '2Y', 'GND', '3Y', '3A', '3B', '3C', '1Y', '1C', 'VCC']      # 7410
DUAL4 = ['1A', '1B', 'NC', '1C', '1D', '1Y', 'GND', '2Y', '2A', '2B', 'NC', '2C', '2D', 'VCC']      # 7420

NAND, NOR, AND, OR, XOR, INV, BUF = '&', '≥1', '&', '≥1', '=1', '1', '1'


def g(de, en, fr, sign, negated, inputs, count, mark=None):
    return Gates(sign, negated, inputs, count, mark, de, en, fr)


CHIPS = [
    Chip('7400', 'Vier NAND-Gatter', 'Quad 2-input NAND gates', 'Quatre portes NON-ET à 2 entrées', QUAD_A,
         g('NAND', 'NAND', 'NON-ET', NAND, True, 2, 4)),
    Chip('7401', 'Vier NAND-Gatter, offener Kollektor', 'Quad 2-input NAND gates, open collector',
         'Quatre portes NON-ET, collecteur ouvert', QUAD_Y, g('NAND (OC)', 'NAND (OC)', 'NON-ET (CO)', NAND, True, 2, 4, 'oc')),
    Chip('7402', 'Vier NOR-Gatter', 'Quad 2-input NOR gates', 'Quatre portes NON-OU à 2 entrées', QUAD_Y,
         g('NOR', 'NOR', 'NON-OU', NOR, True, 2, 4)),
    Chip('7403', 'Vier NAND-Gatter, offener Kollektor', 'Quad 2-input NAND gates, open collector',
         'Quatre portes NON-ET, collecteur ouvert', QUAD_A, g('NAND (OC)', 'NAND (OC)', 'NON-ET (CO)', NAND, True, 2, 4, 'oc')),
    Chip('7404', 'Sechs Inverter', 'Hex inverters', 'Six inverseurs', HEX, g('Inverter', 'inverter', 'inverseur', INV, True, 1, 6)),
    Chip('7405', 'Sechs Inverter, offener Kollektor', 'Hex inverters, open collector', 'Six inverseurs, collecteur ouvert', HEX,
         g('Inverter (OC)', 'inverter (OC)', 'inverseur (CO)', INV, True, 1, 6, 'oc')),
    Chip('7406', 'Sechs Inverter-Treiber, offener Kollektor 30 V', 'Hex inverter drivers, open collector 30 V',
         'Six inverseurs de puissance, collecteur ouvert 30 V', HEX, g('Inverter-Treiber (OC)', 'inverter driver (OC)', 'inverseur de puissance (CO)', INV, True, 1, 6, 'ocbuf')),
    Chip('7407', 'Sechs Treiber, offener Kollektor 30 V', 'Hex buffer drivers, open collector 30 V',
         'Six tampons de puissance, collecteur ouvert 30 V', HEX, g('Treiber (OC)', 'buffer driver (OC)', 'tampon de puissance (CO)', BUF, False, 1, 6, 'ocbuf')),
    Chip('7408', 'Vier UND-Gatter', 'Quad 2-input AND gates', 'Quatre portes ET à 2 entrées', QUAD_A,
         g('UND', 'AND', 'ET', AND, False, 2, 4)),
    Chip('7409', 'Vier UND-Gatter, offener Kollektor', 'Quad 2-input AND gates, open collector',
         'Quatre portes ET, collecteur ouvert', QUAD_A, g('UND (OC)', 'AND (OC)', 'ET (CO)', AND, False, 2, 4, 'oc')),
    Chip('7410', 'Drei NAND-Gatter mit 3 Eingängen', 'Triple 3-input NAND gates', 'Trois portes NON-ET à 3 entrées', TRIPLE,
         g('NAND', 'NAND', 'NON-ET', NAND, True, 3, 3)),
    Chip('7411', 'Drei UND-Gatter mit 3 Eingängen', 'Triple 3-input AND gates', 'Trois portes ET à 3 entrées', TRIPLE,
         g('UND', 'AND', 'ET', AND, False, 3, 3)),
    Chip('7412', 'Drei NAND-Gatter mit 3 Eingängen, offener Kollektor', 'Triple 3-input NAND gates, open collector',
         'Trois portes NON-ET à 3 entrées, collecteur ouvert', TRIPLE, g('NAND (OC)', 'NAND (OC)', 'NON-ET (CO)', NAND, True, 3, 3, 'oc')),
    Chip('7413', 'Zwei NAND-Schmitt-Trigger mit 4 Eingängen', 'Dual 4-input NAND Schmitt triggers',
         'Deux triggers de Schmitt NON-ET à 4 entrées', DUAL4, g('NAND-Schmitt-Trigger', 'NAND Schmitt trigger', 'trigger de Schmitt NON-ET', NAND, True, 4, 2, 'schmitt')),
    Chip('7414', 'Sechs Schmitt-Trigger-Inverter', 'Hex Schmitt trigger inverters', 'Six inverseurs trigger de Schmitt', HEX,
         g('Schmitt-Trigger-Inverter', 'Schmitt trigger inverter', 'inverseur trigger de Schmitt', INV, True, 1, 6, 'schmitt')),
    Chip('7415', 'Drei UND-Gatter mit 3 Eingängen, offener Kollektor', 'Triple 3-input AND gates, open collector',
         'Trois portes ET à 3 entrées, collecteur ouvert', TRIPLE, g('UND (OC)', 'AND (OC)', 'ET (CO)', AND, False, 3, 3, 'oc')),
    Chip('7416', 'Sechs Inverter-Treiber, offener Kollektor 15 V', 'Hex inverter drivers, open collector 15 V',
         'Six inverseurs de puissance, collecteur ouvert 15 V', HEX, g('Inverter-Treiber (OC)', 'inverter driver (OC)', 'inverseur de puissance (CO)', INV, True, 1, 6, 'ocbuf')),
    Chip('7417', 'Sechs Treiber, offener Kollektor 15 V', 'Hex buffer drivers, open collector 15 V',
         'Six tampons de puissance, collecteur ouvert 15 V', HEX, g('Treiber (OC)', 'buffer driver (OC)', 'tampon de puissance (CO)', BUF, False, 1, 6, 'ocbuf')),
    Chip('7420', 'Zwei NAND-Gatter mit 4 Eingängen', 'Dual 4-input NAND gates', 'Deux portes NON-ET à 4 entrées', DUAL4,
         g('NAND', 'NAND', 'NON-ET', NAND, True, 4, 2)),
    Chip('7421', 'Zwei UND-Gatter mit 4 Eingängen', 'Dual 4-input AND gates', 'Deux portes ET à 4 entrées', DUAL4,
         g('UND', 'AND', 'ET', AND, False, 4, 2)),
    Chip('7422', 'Zwei NAND-Gatter mit 4 Eingängen, offener Kollektor', 'Dual 4-input NAND gates, open collector',
         'Deux portes NON-ET à 4 entrées, collecteur ouvert', DUAL4, g('NAND (OC)', 'NAND (OC)', 'NON-ET (CO)', NAND, True, 4, 2, 'oc')),
    Chip('7426', 'Vier NAND-Gatter, offener Kollektor 15 V', 'Quad 2-input NAND gates, open collector 15 V',
         'Quatre portes NON-ET, collecteur ouvert 15 V', QUAD_A, g('NAND (OC)', 'NAND (OC)', 'NON-ET (CO)', NAND, True, 2, 4, 'oc')),
    Chip('7427', 'Drei NOR-Gatter mit 3 Eingängen', 'Triple 3-input NOR gates', 'Trois portes NON-OU à 3 entrées', TRIPLE,
         g('NOR', 'NOR', 'NON-OU', NOR, True, 3, 3)),
    Chip('7428', 'Vier NOR-Treiber', 'Quad 2-input NOR buffers', 'Quatre tampons NON-OU', QUAD_Y,
         g('NOR-Treiber', 'NOR buffer', 'tampon NON-OU', NOR, True, 2, 4, 'buf')),
    Chip('7430', 'NAND-Gatter mit 8 Eingängen', '8-input NAND gate', 'Porte NON-ET à 8 entrées',
         ['A', 'B', 'C', 'D', 'E', 'F', 'GND', 'Y', 'NC', 'NC', 'G', 'H', 'NC', 'VCC'],
         g('NAND', 'NAND', 'NON-ET', NAND, True, 8, 1)),
    Chip('7432', 'Vier ODER-Gatter', 'Quad 2-input OR gates', 'Quatre portes OU à 2 entrées', QUAD_A,
         g('ODER', 'OR', 'OU', OR, False, 2, 4)),
    Chip('7433', 'Vier NOR-Treiber, offener Kollektor', 'Quad 2-input NOR buffers, open collector',
         'Quatre tampons NON-OU, collecteur ouvert', QUAD_Y, g('NOR-Treiber (OC)', 'NOR buffer (OC)', 'tampon NON-OU (CO)', NOR, True, 2, 4, 'ocbuf')),
    Chip('7434', 'Sechs Treiber', 'Hex buffers', 'Six tampons', HEX, g('Treiber', 'buffer', 'tampon', BUF, False, 1, 6)),
    Chip('7435', 'Sechs Treiber, offener Kollektor', 'Hex buffers, open collector', 'Six tampons, collecteur ouvert', HEX,
         g('Treiber (OC)', 'buffer (OC)', 'tampon (CO)', BUF, False, 1, 6, 'oc')),
    Chip('7437', 'Vier NAND-Treiber', 'Quad 2-input NAND buffers', 'Quatre tampons NON-ET', QUAD_A,
         g('NAND-Treiber', 'NAND buffer', 'tampon NON-ET', NAND, True, 2, 4, 'buf')),
    Chip('7438', 'Vier NAND-Treiber, offener Kollektor', 'Quad 2-input NAND buffers, open collector',
         'Quatre tampons NON-ET, collecteur ouvert', QUAD_A, g('NAND-Treiber (OC)', 'NAND buffer (OC)', 'tampon NON-ET (CO)', NAND, True, 2, 4, 'ocbuf')),
    Chip('7440', 'Zwei NAND-Treiber mit 4 Eingängen', 'Dual 4-input NAND buffers', 'Deux tampons NON-ET à 4 entrées', DUAL4,
         g('NAND-Treiber', 'NAND buffer', 'tampon NON-ET', NAND, True, 4, 2, 'buf')),
    Chip('7486', 'Vier Exklusiv-ODER-Gatter', 'Quad 2-input XOR gates', 'Quatre portes OU exclusif', QUAD_A,
         g('Exklusiv-ODER', 'XOR', 'OU exclusif', XOR, False, 2, 4)),
    Chip('74125', 'Vier Treiber mit Tristate', 'Quad tri-state buffers', 'Quatre tampons trois états',
         ['~1OE', '1A', '1Y', '~2OE', '2A', '2Y', 'GND', '3Y', '3A', '~3OE', '4Y', '4A', '~4OE', 'VCC'],
         [Block('Treiber mit Tristate', 'tri-state buffer', 'tampon trois états', [f'~{u}OE', f'{u}A'], [f'{u}Y']) for u in range(1, 5)]),
    Chip('74132', 'Vier NAND-Schmitt-Trigger', 'Quad 2-input NAND Schmitt triggers', 'Quatre triggers de Schmitt NON-ET', QUAD_A,
         g('NAND-Schmitt-Trigger', 'NAND Schmitt trigger', 'trigger de Schmitt NON-ET', NAND, True, 2, 4, 'schmitt')),
    Chip('74133', 'NAND-Gatter mit 13 Eingängen', '13-input NAND gate', 'Porte NON-ET à 13 entrées',
         ['A', 'B', 'C', 'D', 'E', 'F', 'G', 'GND', 'Y', 'H', 'I', 'J', 'K', 'L', 'M', 'VCC'],
         g('NAND', 'NAND', 'NON-ET', NAND, True, 13, 1)),
    Chip('74136', 'Vier Exklusiv-ODER-Gatter, offener Kollektor', 'Quad 2-input XOR gates, open collector',
         'Quatre portes OU exclusif, collecteur ouvert', QUAD_A, g('Exklusiv-ODER (OC)', 'XOR (OC)', 'OU exclusif (CO)', XOR, False, 2, 4, 'oc')),
    Chip('74266', 'Vier Exklusiv-NOR-Gatter, offener Kollektor', 'Quad 2-input XNOR gates, open collector',
         'Quatre portes NON-OU exclusif, collecteur ouvert',
         ['1A', '1B', '1Y', '2Y', '2A', '2B', 'GND', '3A', '3B', '3Y', '4Y', '4A', '4B', 'VCC'],
         g('Exklusiv-NOR (OC)', 'XNOR (OC)', 'NON-OU exclusif (CO)', XOR, True, 2, 4, 'oc')),
    Chip('744002', 'Zwei NOR-Gatter mit 4 Eingängen (HC4002)', 'Dual 4-input NOR gates (HC4002)', 'Deux portes NON-OU à 4 entrées (HC4002)',
         ['1Y', '1A', '1B', '1C', '1D', 'NC', 'GND', 'NC', '2D', '2C', '2B', '2A', '2Y', 'VCC'],
         g('NOR', 'NOR', 'NON-OU', NOR, True, 4, 2)),
]


def b(de, en, fr, inputs, outputs):
    return Block(de, en, fr, inputs, outputs)


DFF = ('D-Flipflop', 'D flip-flop', 'bascule D')
JKFF = ('JK-Flipflop', 'JK flip-flop', 'bascule JK')


def dual(names, kind, inputs, outputs):
    """Two equal blocks: their pin names with the unit's number in front."""
    return [b(*kind, [n.replace('#', str(u)) for n in inputs], [n.replace('#', str(u)) for n in outputs]) for u in (1, 2)]


def octal(prefix_in, prefix_out, first=1):
    return [f'{k}{prefix_in}' for k in range(first, first + 8)], [f'{k}{prefix_out}' for k in range(first, first + 8)]


CHIPS += [
    Chip('7441', 'BCD-zu-Dezimal-Decoder/Treiber für Nixie-Röhren', 'BCD to decimal decoder/driver for nixie tubes',
         'Décodeur/pilote BCD vers décimal pour tubes nixie',
         ['~8', '~9', 'A', 'D', 'VCC', 'B', 'C', '~2', '~3', '~7', '~6', 'GND', '~4', '~5', '~1', '~0'],
         [b('BCD-zu-Dezimal-Treiber', 'BCD to decimal driver', 'pilote BCD vers décimal', ['A', 'B', 'C', 'D'], [f'~{k}' for k in range(10)])]),
    Chip('7442', 'BCD-zu-Dezimal-Decoder', 'BCD to decimal decoder', 'Décodeur BCD vers décimal',
         ['~0', '~1', '~2', '~3', '~4', '~5', '~6', 'GND', '~7', '~8', '~9', 'D', 'C', 'B', 'A', 'VCC'],
         [b('BCD-zu-Dezimal-Decoder', 'BCD to decimal decoder', 'décodeur BCD vers décimal', ['A', 'B', 'C', 'D'], [f'~{k}' for k in range(10)])]),
    Chip('7443', 'Exzess-3-zu-Dezimal-Decoder', 'Excess-3 to decimal decoder', 'Décodeur excédent 3 vers décimal',
         ['~0', '~1', '~2', '~3', '~4', '~5', '~6', 'GND', '~7', '~8', '~9', 'D', 'C', 'B', 'A', 'VCC'],
         [b('Exzess-3-zu-Dezimal-Decoder', 'excess-3 to decimal decoder', 'décodeur excédent 3 vers décimal', ['A', 'B', 'C', 'D'], [f'~{k}' for k in range(10)])]),
    Chip('7444', 'Exzess-3-Gray-zu-Dezimal-Decoder', 'Excess-3-Gray to decimal decoder', 'Décodeur Gray excédent 3 vers décimal',
         ['~0', '~1', '~2', '~3', '~4', '~5', '~6', 'GND', '~7', '~8', '~9', 'D', 'C', 'B', 'A', 'VCC'],
         [b('Exzess-3-Gray-zu-Dezimal-Decoder', 'excess-3-Gray to decimal decoder', 'décodeur Gray excédent 3 vers décimal', ['A', 'B', 'C', 'D'], [f'~{k}' for k in range(10)])]),
    Chip('7445', 'BCD-zu-Dezimal-Decoder/Treiber, offener Kollektor', 'BCD to decimal decoder/driver, open collector',
         'Décodeur/pilote BCD vers décimal, collecteur ouvert',
         ['~0', '~1', '~2', '~3', '~4', '~5', '~6', 'GND', '~7', '~8', '~9', 'D', 'C', 'B', 'A', 'VCC'],
         [b('BCD-zu-Dezimal-Treiber', 'BCD to decimal driver', 'pilote BCD vers décimal', ['A', 'B', 'C', 'D'], [f'~{k}' for k in range(10)])]),
    Chip('74145', 'BCD-zu-Dezimal-Decoder/Treiber, offener Kollektor', 'BCD to decimal decoder/driver, open collector',
         'Décodeur/pilote BCD vers décimal, collecteur ouvert',
         ['~0', '~1', '~2', '~3', '~4', '~5', '~6', 'GND', '~7', '~8', '~9', 'D', 'C', 'B', 'A', 'VCC'],
         [b('BCD-zu-Dezimal-Treiber', 'BCD to decimal driver', 'pilote BCD vers décimal', ['A', 'B', 'C', 'D'], [f'~{k}' for k in range(10)])]),
    Chip('7446', 'BCD-zu-7-Segment-Decoder/Treiber, 30 V', 'BCD to 7-segment decoder/driver, 30 V', 'Décodeur/pilote BCD 7 segments, 30 V',
         ['B', 'C', '~LT', '~BI/RBO', '~RBI', 'D', 'A', 'GND', '~e', '~d', '~c', '~b', '~a', '~g', '~f', 'VCC'],
         [b('BCD-zu-7-Segment-Decoder', 'BCD to 7-segment decoder', 'décodeur BCD 7 segments', ['A', 'B', 'C', 'D', '~LT', '~RBI', '~BI/RBO'],
            ['~a', '~b', '~c', '~d', '~e', '~f', '~g'])]),
    Chip('7447', 'BCD-zu-7-Segment-Decoder/Treiber', 'BCD to 7-segment decoder/driver', 'Décodeur/pilote BCD 7 segments',
         ['B', 'C', '~LT', '~BI/RBO', '~RBI', 'D', 'A', 'GND', '~e', '~d', '~c', '~b', '~a', '~g', '~f', 'VCC'],
         [b('BCD-zu-7-Segment-Decoder', 'BCD to 7-segment decoder', 'décodeur BCD 7 segments', ['A', 'B', 'C', 'D', '~LT', '~RBI', '~BI/RBO'],
            ['~a', '~b', '~c', '~d', '~e', '~f', '~g'])]),
    Chip('7448', 'BCD-zu-7-Segment-Decoder', 'BCD to 7-segment decoder', 'Décodeur BCD 7 segments',
         ['B', 'C', '~LT', '~BI/RBO', '~RBI', 'D', 'A', 'GND', 'e', 'd', 'c', 'b', 'a', 'g', 'f', 'VCC'],
         [b('BCD-zu-7-Segment-Decoder', 'BCD to 7-segment decoder', 'décodeur BCD 7 segments', ['A', 'B', 'C', 'D', '~LT', '~RBI', '~BI/RBO'],
            ['a', 'b', 'c', 'd', 'e', 'f', 'g'])]),
    Chip('7449', 'BCD-zu-7-Segment-Decoder, offener Kollektor', 'BCD to 7-segment decoder, open collector', 'Décodeur BCD 7 segments, collecteur ouvert',
         ['B', 'C', '~BI', 'D', 'A', 'e', 'GND', 'd', 'c', 'b', 'a', 'g', 'f', 'VCC'],
         [b('BCD-zu-7-Segment-Decoder', 'BCD to 7-segment decoder', 'décodeur BCD 7 segments', ['A', 'B', 'C', 'D', '~BI'], ['a', 'b', 'c', 'd', 'e', 'f', 'g'])]),
    Chip('7473', 'Zwei JK-Flipflops mit Löschen', 'Dual JK flip-flops with clear', 'Deux bascules JK avec effacement',
         ['1CLK', '~1CLR', '1K', 'VCC', '2CLK', '~2CLR', '2J', '~2Q', '2Q', '2K', 'GND', '1Q', '~1Q', '1J'],
         dual(None, JKFF, ['#J', '#CLK', '#K', '~#CLR'], ['#Q', '~#Q'])),
    Chip('7474', 'Zwei D-Flipflops', 'Dual D flip-flops', 'Deux bascules D',
         ['~1CLR', '1D', '1CLK', '~1PRE', '1Q', '~1Q', 'GND', '~2Q', '2Q', '~2PRE', '2CLK', '2D', '~2CLR', 'VCC'],
         dual(None, DFF, ['~#PRE', '#D', '#CLK', '~#CLR'], ['#Q', '~#Q'])),
    Chip('7475', 'Vier D-Latches', '4-bit bistable latches', 'Quatre verrous D',
         ['~1Q', '1D', '2D', '3C,4C', 'VCC', '3D', '4D', '~4Q', '4Q', '3Q', '~3Q', 'GND', '1C,2C', '~2Q', '2Q', '1Q'],
         [b('Zwei D-Latches', 'two D latches', 'deux verrous D', ['1D', '2D', '1C,2C'], ['1Q', '~1Q', '2Q', '~2Q']),
          b('Zwei D-Latches', 'two D latches', 'deux verrous D', ['3D', '4D', '3C,4C'], ['3Q', '~3Q', '4Q', '~4Q'])]),
    Chip('7477', 'Vier D-Latches ohne invertierte Ausgänge', '4-bit bistable latches without inverted outputs', 'Quatre verrous D sans sorties inverses',
         ['1D', '2D', '3C,4C', 'VCC', '3D', '4D', 'NC', '4Q', '3Q', 'NC', 'GND', '1C,2C', '2Q', '1Q'],
         [b('Zwei D-Latches', 'two D latches', 'deux verrous D', ['1D', '2D', '1C,2C'], ['1Q', '2Q']),
          b('Zwei D-Latches', 'two D latches', 'deux verrous D', ['3D', '4D', '3C,4C'], ['3Q', '4Q'])]),
    Chip('7476', 'Zwei JK-Flipflops mit Setzen und Löschen', 'Dual JK flip-flops with preset and clear', 'Deux bascules JK avec mise à un et effacement',
         ['1CLK', '~1PRE', '~1CLR', '1J', 'VCC', '2CLK', '~2PRE', '~2CLR', '2J', '~2Q', '2Q', '2K', 'GND', '~1Q', '1Q', '1K'],
         dual(None, JKFF, ['~#PRE', '#J', '#CLK', '#K', '~#CLR'], ['#Q', '~#Q'])),
    Chip('7490', 'Dezimalzähler', 'Decade counter', 'Compteur décimal',
         ['CKB', 'R0(1)', 'R0(2)', 'NC', 'VCC', 'R9(1)', 'R9(2)', 'QC', 'QB', 'GND', 'QD', 'QA', 'NC', 'CKA'],
         [b('Dezimalzähler', 'decade counter', 'compteur décimal', ['CKA', 'CKB', 'R0(1)', 'R0(2)', 'R9(1)', 'R9(2)'], ['QA', 'QB', 'QC', 'QD'])]),
    Chip('7493', 'Binärzähler 4 Bit', '4-bit binary counter', 'Compteur binaire 4 bits',
         ['CKB', 'R0(1)', 'R0(2)', 'NC', 'VCC', 'NC', 'NC', 'QC', 'QB', 'GND', 'QD', 'QA', 'NC', 'CKA'],
         [b('Binärzähler', 'binary counter', 'compteur binaire', ['CKA', 'CKB', 'R0(1)', 'R0(2)'], ['QA', 'QB', 'QC', 'QD'])]),
    Chip('7495', 'Schieberegister 4 Bit, paralleler Zugriff', '4-bit parallel-access shift register', 'Registre à décalage 4 bits à accès parallèle',
         ['SER', 'A', 'B', 'C', 'D', 'MODE', 'GND', 'CLK2', 'CLK1', 'QD', 'QC', 'QB', 'QA', 'VCC'],
         [b('Schieberegister', 'shift register', 'registre à décalage', ['SER', 'A', 'B', 'C', 'D', 'MODE', 'CLK1', 'CLK2'], ['QA', 'QB', 'QC', 'QD'])]),
    Chip('74109', 'Zwei JK-Flipflops (J, nicht K)', 'Dual J-not-K flip-flops', 'Deux bascules J-K barre',
         ['~1CLR', '1J', '~1K', '1CLK', '~1PRE', '1Q', '~1Q', 'GND', '~2Q', '2Q', '~2PRE', '2CLK', '~2K', '2J', '~2CLR', 'VCC'],
         dual(None, JKFF, ['~#PRE', '#J', '#CLK', '~#K', '~#CLR'], ['#Q', '~#Q'])),
    Chip('74112', 'Zwei JK-Flipflops, negative Flanke', 'Dual JK flip-flops, negative edge', 'Deux bascules JK, front descendant',
         ['~1CLK', '1K', '1J', '~1PRE', '1Q', '~1Q', '~2Q', 'GND', '2Q', '~2PRE', '2J', '2K', '~2CLK', '~2CLR', '~1CLR', 'VCC'],
         dual(None, JKFF, ['~#PRE', '#J', '~#CLK', '#K', '~#CLR'], ['#Q', '~#Q'])),
    Chip('74113', 'Zwei JK-Flipflops mit Setzen', 'Dual JK flip-flops with preset', 'Deux bascules JK avec mise à un',
         ['~1CLK', '1K', '1J', '~1PRE', '1Q', '~1Q', 'GND', '~2Q', '2Q', '~2PRE', '2J', '2K', '~2CLK', 'VCC'],
         dual(None, JKFF, ['~#PRE', '#J', '~#CLK', '#K'], ['#Q', '~#Q'])),
    Chip('74114', 'Zwei JK-Flipflops, gemeinsamer Takt und Löschen', 'Dual JK flip-flops, common clock and clear',
         'Deux bascules JK, horloge et effacement communs',
         ['~CLR', '1K', '1J', '~1PRE', '1Q', '~1Q', 'GND', '~2Q', '2Q', '~2PRE', '2J', '2K', '~CLK', 'VCC'],
         [b('Zwei JK-Flipflops', 'two JK flip-flops', 'deux bascules JK', ['~1PRE', '1J', '1K', '~2PRE', '2J', '2K', '~CLK', '~CLR'], ['1Q', '~1Q', '2Q', '~2Q'])]),
    Chip('74121', 'Monoflop mit Schmitt-Trigger-Eingang', 'Monostable multivibrator with Schmitt trigger input',
         'Monostable avec entrée trigger de Schmitt',
         ['~Q', 'NC', 'A1', 'A2', 'B', 'Q', 'GND', 'NC', 'RINT', 'CEXT', 'REXT/CEXT', 'NC', 'NC', 'VCC'],
         [b('Monoflop', 'monostable', 'monostable', ['A1', 'A2', 'B', 'RINT', 'CEXT', 'REXT/CEXT'], ['Q', '~Q'])]),
    Chip('74123', 'Zwei nachtriggerbare Monoflops', 'Dual retriggerable monostable multivibrators', 'Deux monostables redéclenchables',
         ['1A', '1B', '~1CLR', '~1Q', '2Q', '2CEXT', '2REXT/CEXT', 'GND', '2A', '2B', '~2CLR', '~2Q', '1Q', '1CEXT', '1REXT/CEXT', 'VCC'],
         dual(None, ('Monoflop', 'monostable', 'monostable'), ['#A', '#B', '~#CLR', '#CEXT', '#REXT/CEXT'], ['#Q', '~#Q'])),
    Chip('74221', 'Zwei Monoflops mit Schmitt-Trigger-Eingang', 'Dual monostable multivibrators with Schmitt trigger inputs',
         'Deux monostables avec entrées trigger de Schmitt',
         ['1A', '1B', '~1CLR', '~1Q', '2Q', '2CEXT', '2REXT/CEXT', 'GND', '2A', '2B', '~2CLR', '~2Q', '1Q', '1CEXT', '1REXT/CEXT', 'VCC'],
         dual(None, ('Monoflop', 'monostable', 'monostable'), ['#A', '#B', '~#CLR', '#CEXT', '#REXT/CEXT'], ['#Q', '~#Q'])),
    Chip('74137', '3-zu-8-Decoder mit Adress-Latch', '3-to-8 decoder with address latches', 'Décodeur 3 vers 8 avec verrous d\'adresse',
         ['A', 'B', 'C', '~GL', '~G2', 'G1', '~Y7', 'GND', '~Y6', '~Y5', '~Y4', '~Y3', '~Y2', '~Y1', '~Y0', 'VCC'],
         [b('3-zu-8-Decoder', '3-to-8 decoder', 'décodeur 3 vers 8', ['A', 'B', 'C', '~GL', 'G1', '~G2'], [f'~Y{k}' for k in range(8)])]),
    Chip('74138', '3-zu-8-Decoder', '3-to-8 decoder', 'Décodeur 3 vers 8',
         ['A', 'B', 'C', '~G2A', '~G2B', 'G1', '~Y7', 'GND', '~Y6', '~Y5', '~Y4', '~Y3', '~Y2', '~Y1', '~Y0', 'VCC'],
         [b('3-zu-8-Decoder', '3-to-8 decoder', 'décodeur 3 vers 8', ['A', 'B', 'C', 'G1', '~G2A', '~G2B'], [f'~Y{k}' for k in range(8)])]),
    Chip('74139', 'Zwei 2-zu-4-Decoder', 'Dual 2-to-4 decoders', 'Deux décodeurs 2 vers 4',
         ['~1G', '1A', '1B', '~1Y0', '~1Y1', '~1Y2', '~1Y3', 'GND', '~2Y3', '~2Y2', '~2Y1', '~2Y0', '2B', '2A', '~2G', 'VCC'],
         dual(None, ('2-zu-4-Decoder', '2-to-4 decoder', 'décodeur 2 vers 4'), ['#A', '#B', '~#G'], ['~#Y0', '~#Y1', '~#Y2', '~#Y3'])),
    Chip('74151', '8-zu-1-Multiplexer', '8-to-1 multiplexer', 'Multiplexeur 8 vers 1',
         ['D3', 'D2', 'D1', 'D0', 'Y', '~W', '~G', 'GND', 'C', 'B', 'A', 'D7', 'D6', 'D5', 'D4', 'VCC'],
         [b('8-zu-1-Multiplexer', '8-to-1 multiplexer', 'multiplexeur 8 vers 1', [f'D{k}' for k in range(8)] + ['A', 'B', 'C', '~G'], ['Y', '~W'])]),
    Chip('74153', 'Zwei 4-zu-1-Multiplexer', 'Dual 4-to-1 multiplexers', 'Deux multiplexeurs 4 vers 1',
         ['~1G', 'B', '1C3', '1C2', '1C1', '1C0', '1Y', 'GND', '2Y', '2C0', '2C1', '2C2', '2C3', 'A', '~2G', 'VCC'],
         [b('4-zu-1-Multiplexer mit Auswahl', '4-to-1 multiplexer with select', 'multiplexeur 4 vers 1 avec sélection',
            ['1C0', '1C1', '1C2', '1C3', '~1G', 'A', 'B'], ['1Y']),
          b('4-zu-1-Multiplexer', '4-to-1 multiplexer', 'multiplexeur 4 vers 1', ['2C0', '2C1', '2C2', '2C3', '~2G'], ['2Y'])]),
    Chip('74154', '4-zu-16-Decoder', '4-to-16 decoder', 'Décodeur 4 vers 16',
         ['~0', '~1', '~2', '~3', '~4', '~5', '~6', '~7', '~8', '~9', '~10', 'GND', '~11', '~12', '~13', '~14', '~15', '~G1', '~G2', 'D', 'C', 'B', 'A', 'VCC'],
         [b('4-zu-16-Decoder', '4-to-16 decoder', 'décodeur 4 vers 16', ['A', 'B', 'C', 'D', '~G1', '~G2'], [f'~{k}' for k in range(16)])]),
    Chip('74157', 'Vier 2-zu-1-Multiplexer', 'Quad 2-to-1 multiplexers', 'Quatre multiplexeurs 2 vers 1',
         ['A/~B', '1A', '1B', '1Y', '2A', '2B', '2Y', 'GND', '3Y', '3B', '3A', '4Y', '4B', '4A', '~G', 'VCC'],
         [b('2-zu-1-Multiplexer mit Auswahl', '2-to-1 multiplexer with select', 'multiplexeur 2 vers 1 avec sélection', ['1A', '1B', 'A/~B', '~G'], ['1Y'])]
         + [b('2-zu-1-Multiplexer', '2-to-1 multiplexer', 'multiplexeur 2 vers 1', [f'{k}A', f'{k}B'], [f'{k}Y']) for k in (2, 3, 4)]),
    Chip('74158', 'Vier 2-zu-1-Multiplexer, invertierend', 'Quad 2-to-1 multiplexers, inverting', 'Quatre multiplexeurs 2 vers 1, inverseurs',
         ['A/~B', '1A', '1B', '~1Y', '2A', '2B', '~2Y', 'GND', '~3Y', '3B', '3A', '~4Y', '4B', '4A', '~G', 'VCC'],
         [b('2-zu-1-Multiplexer mit Auswahl', '2-to-1 multiplexer with select', 'multiplexeur 2 vers 1 avec sélection', ['1A', '1B', 'A/~B', '~G'], ['~1Y'])]
         + [b('2-zu-1-Multiplexer', '2-to-1 multiplexer', 'multiplexeur 2 vers 1', [f'{k}A', f'{k}B'], [f'~{k}Y']) for k in (2, 3, 4)]),
    Chip('74161', 'Synchroner Binärzähler 4 Bit', 'Synchronous 4-bit binary counter', 'Compteur binaire synchrone 4 bits',
         ['~CLR', 'CLK', 'A', 'B', 'C', 'D', 'ENP', 'GND', '~LOAD', 'ENT', 'QD', 'QC', 'QB', 'QA', 'RCO', 'VCC'],
         [b('Synchroner Binärzähler', 'synchronous binary counter', 'compteur binaire synchrone', ['A', 'B', 'C', 'D', 'ENP', 'ENT', '~LOAD', 'CLK', '~CLR'],
            ['QA', 'QB', 'QC', 'QD', 'RCO'])]),
    Chip('74164', 'Schieberegister 8 Bit', '8-bit serial-in parallel-out shift register', 'Registre à décalage 8 bits',
         ['A', 'B', 'QA', 'QB', 'QC', 'QD', 'GND', 'CLK', '~CLR', 'QE', 'QF', 'QG', 'QH', 'VCC'],
         [b('Schieberegister', 'shift register', 'registre à décalage', ['A', 'B', 'CLK', '~CLR'], ['QA', 'QB', 'QC', 'QD', 'QE', 'QF', 'QG', 'QH'])]),
    Chip('74165', 'Schieberegister 8 Bit, parallel ladbar', '8-bit parallel-load shift register', 'Registre à décalage 8 bits à chargement parallèle',
         ['SH/~LD', 'CLK', 'E', 'F', 'G', 'H', '~QH', 'GND', 'QH', 'SER', 'A', 'B', 'C', 'D', 'CLK INH', 'VCC'],
         [b('Schieberegister', 'shift register', 'registre à décalage', ['A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'SER', 'SH/~LD', 'CLK', 'CLK INH'], ['QH', '~QH'])]),
    Chip('74174', 'Sechs D-Flipflops mit Löschen', 'Hex D flip-flops with clear', 'Six bascules D avec effacement',
         ['~CLR', '1Q', '1D', '2D', '2Q', '3D', '3Q', 'GND', 'CLK', '4Q', '4D', '5Q', '5D', '6D', '6Q', 'VCC'],
         [b('Sechs D-Flipflops', 'six D flip-flops', 'six bascules D', ['1D', '2D', '3D', '4D', '5D', '6D', 'CLK', '~CLR'], ['1Q', '2Q', '3Q', '4Q', '5Q', '6Q'])]),
    Chip('74175', 'Vier D-Flipflops mit Löschen', 'Quad D flip-flops with clear', 'Quatre bascules D avec effacement',
         ['~CLR', '1Q', '~1Q', '1D', '2D', '~2Q', '2Q', 'GND', 'CLK', '3Q', '~3Q', '3D', '4D', '~4Q', '4Q', 'VCC'],
         [b('Vier D-Flipflops', 'four D flip-flops', 'quatre bascules D', ['1D', '2D', '3D', '4D', 'CLK', '~CLR'],
            ['1Q', '~1Q', '2Q', '~2Q', '3Q', '~3Q', '4Q', '~4Q'])]),
    Chip('74190', 'Synchroner Dezimalzähler vorwärts/rückwärts', 'Synchronous up/down decade counter', 'Compteur décimal synchrone réversible',
         ['B', 'QB', 'QA', '~CTEN', 'D/~U', 'QC', 'QD', 'GND', 'D', 'C', '~LOAD', 'MAX/MIN', '~RCO', 'CLK', 'A', 'VCC'],
         [b('Dezimalzähler vorwärts/rückwärts', 'up/down decade counter', 'compteur décimal réversible',
            ['A', 'B', 'C', 'D', '~LOAD', '~CTEN', 'D/~U', 'CLK'], ['QA', 'QB', 'QC', 'QD', 'MAX/MIN', '~RCO'])]),
    Chip('74191', 'Synchroner Binärzähler vorwärts/rückwärts', 'Synchronous up/down binary counter', 'Compteur binaire synchrone réversible',
         ['B', 'QB', 'QA', '~CTEN', 'D/~U', 'QC', 'QD', 'GND', 'D', 'C', '~LOAD', 'MAX/MIN', '~RCO', 'CLK', 'A', 'VCC'],
         [b('Binärzähler vorwärts/rückwärts', 'up/down binary counter', 'compteur binaire réversible',
            ['A', 'B', 'C', 'D', '~LOAD', '~CTEN', 'D/~U', 'CLK'], ['QA', 'QB', 'QC', 'QD', 'MAX/MIN', '~RCO'])]),
    Chip('74192', 'Synchroner Dezimalzähler mit zwei Takten', 'Synchronous up/down decade counter, dual clock', 'Compteur décimal synchrone, deux horloges',
         ['B', 'QB', 'QA', 'DOWN', 'UP', 'QC', 'QD', 'GND', 'D', 'C', '~LOAD', '~CO', '~BO', 'CLR', 'A', 'VCC'],
         [b('Dezimalzähler vorwärts/rückwärts', 'up/down decade counter', 'compteur décimal réversible',
            ['A', 'B', 'C', 'D', '~LOAD', 'UP', 'DOWN', 'CLR'], ['QA', 'QB', 'QC', 'QD', '~CO', '~BO'])]),
    Chip('74193', 'Synchroner Binärzähler mit zwei Takten', 'Synchronous up/down binary counter, dual clock', 'Compteur binaire synchrone, deux horloges',
         ['B', 'QB', 'QA', 'DOWN', 'UP', 'QC', 'QD', 'GND', 'D', 'C', '~LOAD', '~CO', '~BO', 'CLR', 'A', 'VCC'],
         [b('Binärzähler vorwärts/rückwärts', 'up/down binary counter', 'compteur binaire réversible',
            ['A', 'B', 'C', 'D', '~LOAD', 'UP', 'DOWN', 'CLR'], ['QA', 'QB', 'QC', 'QD', '~CO', '~BO'])]),
    Chip('74240', 'Acht invertierende Treiber mit Tristate', 'Octal inverting tri-state buffers', 'Huit tampons inverseurs trois états',
         ['~1G', '1A1', '~2Y4', '1A2', '~2Y3', '1A3', '~2Y2', '1A4', '~2Y1', 'GND', '2A1', '~1Y4', '2A2', '~1Y3', '2A3', '~1Y2', '2A4', '~1Y1', '~2G', 'VCC'],
         dual(None, ('Vier invertierende Treiber', 'four inverting buffers', 'quatre tampons inverseurs'),
              ['#A1', '#A2', '#A3', '#A4', '~#G'], ['~#Y1', '~#Y2', '~#Y3', '~#Y4'])),
    Chip('74241', 'Acht Treiber mit Tristate', 'Octal tri-state buffers', 'Huit tampons trois états',
         ['~1G', '1A1', '2Y4', '1A2', '2Y3', '1A3', '2Y2', '1A4', '2Y1', 'GND', '2A1', '1Y4', '2A2', '1Y3', '2A3', '1Y2', '2A4', '1Y1', '2G', 'VCC'],
         [b('Vier Treiber', 'four buffers', 'quatre tampons', ['1A1', '1A2', '1A3', '1A4', '~1G'], ['1Y1', '1Y2', '1Y3', '1Y4']),
          b('Vier Treiber', 'four buffers', 'quatre tampons', ['2A1', '2A2', '2A3', '2A4', '2G'], ['2Y1', '2Y2', '2Y3', '2Y4'])]),
    Chip('74244', 'Acht Treiber mit Tristate', 'Octal tri-state buffers', 'Huit tampons trois états',
         ['~1G', '1A1', '2Y4', '1A2', '2Y3', '1A3', '2Y2', '1A4', '2Y1', 'GND', '2A1', '1Y4', '2A2', '1Y3', '2A3', '1Y2', '2A4', '1Y1', '~2G', 'VCC'],
         dual(None, ('Vier Treiber', 'four buffers', 'quatre tampons'), ['#A1', '#A2', '#A3', '#A4', '~#G'], ['#Y1', '#Y2', '#Y3', '#Y4'])),
    Chip('74245', 'Bus-Transceiver 8 Bit', 'Octal bus transceiver', 'Émetteur-récepteur de bus 8 bits',
         ['DIR', 'A1', 'A2', 'A3', 'A4', 'A5', 'A6', 'A7', 'A8', 'GND', 'B8', 'B7', 'B6', 'B5', 'B4', 'B3', 'B2', 'B1', '~OE', 'VCC'],
         [b('Bus-Transceiver', 'bus transceiver', 'émetteur-récepteur de bus', [f'A{k}' for k in range(1, 9)] + ['DIR', '~OE'], [f'B{k}' for k in range(1, 9)])]),
    Chip('74253', 'Zwei 4-zu-1-Multiplexer mit Tristate', 'Dual 4-to-1 multiplexers, tri-state', 'Deux multiplexeurs 4 vers 1, trois états',
         ['~1OE', 'B', '1C3', '1C2', '1C1', '1C0', '1Y', 'GND', '2Y', '2C0', '2C1', '2C2', '2C3', 'A', '~2OE', 'VCC'],
         [b('Zwei 4-zu-1-Multiplexer', 'two 4-to-1 multiplexers', 'deux multiplexeurs 4 vers 1',
            ['1C0', '1C1', '1C2', '1C3', '~1OE', '2C0', '2C1', '2C2', '2C3', '~2OE', 'A', 'B'], ['1Y', '2Y'])]),
    Chip('74259', 'Adressierbares Latch 8 Bit', '8-bit addressable latch', 'Verrou adressable 8 bits',
         ['S0', 'S1', 'S2', 'Q0', 'Q1', 'Q2', 'Q3', 'GND', 'Q4', 'Q5', 'Q6', 'Q7', 'D', '~G', '~CLR', 'VCC'],
         [b('Adressierbares Latch', 'addressable latch', 'verrou adressable', ['D', 'S0', 'S1', 'S2', '~G', '~CLR'], [f'Q{k}' for k in range(8)])]),
    Chip('74273', 'Acht D-Flipflops mit Löschen', 'Octal D flip-flops with clear', 'Huit bascules D avec effacement',
         ['~CLR', '1Q', '1D', '2D', '2Q', '3Q', '3D', '4D', '4Q', 'GND', 'CLK', '5Q', '5D', '6D', '6Q', '7Q', '7D', '8D', '8Q', 'VCC'],
         [b('Acht D-Flipflops', 'eight D flip-flops', 'huit bascules D', [f'{k}D' for k in range(1, 9)] + ['CLK', '~CLR'], [f'{k}Q' for k in range(1, 9)])]),
    Chip('74373', 'Acht D-Latches mit Tristate', 'Octal transparent D latches, tri-state', 'Huit verrous D trois états',
         ['~OC', '1Q', '1D', '2D', '2Q', '3Q', '3D', '4D', '4Q', 'GND', 'C', '5Q', '5D', '6D', '6Q', '7Q', '7D', '8D', '8Q', 'VCC'],
         [b('Acht D-Latches', 'eight D latches', 'huit verrous D', [f'{k}D' for k in range(1, 9)] + ['C', '~OC'], [f'{k}Q' for k in range(1, 9)])]),
    Chip('74374', 'Acht D-Flipflops mit Tristate', 'Octal D flip-flops, tri-state', 'Huit bascules D trois états',
         ['~OC', '1Q', '1D', '2D', '2Q', '3Q', '3D', '4D', '4Q', 'GND', 'CLK', '5Q', '5D', '6D', '6Q', '7Q', '7D', '8D', '8Q', 'VCC'],
         [b('Acht D-Flipflops', 'eight D flip-flops', 'huit bascules D', [f'{k}D' for k in range(1, 9)] + ['CLK', '~OC'], [f'{k}Q' for k in range(1, 9)])]),
    Chip('74393', 'Zwei Binärzähler 4 Bit', 'Dual 4-bit binary counters', 'Deux compteurs binaires 4 bits',
         ['1A', '1CLR', '1QA', '1QB', '1QC', '1QD', 'GND', '2QD', '2QC', '2QB', '2QA', '2CLR', '2A', 'VCC'],
         dual(None, ('Binärzähler', 'binary counter', 'compteur binaire'), ['#A', '#CLR'], ['#QA', '#QB', '#QC', '#QD'])),
    Chip('74540', 'Acht invertierende Treiber mit Tristate', 'Octal inverting tri-state buffers', 'Huit tampons inverseurs trois états',
         ['~G1', 'A1', 'A2', 'A3', 'A4', 'A5', 'A6', 'A7', 'A8', 'GND', '~Y8', '~Y7', '~Y6', '~Y5', '~Y4', '~Y3', '~Y2', '~Y1', '~G2', 'VCC'],
         [b('Acht invertierende Treiber', 'eight inverting buffers', 'huit tampons inverseurs', [f'A{k}' for k in range(1, 9)] + ['~G1', '~G2'], [f'~Y{k}' for k in range(1, 9)])]),
    Chip('74541', 'Acht Treiber mit Tristate', 'Octal tri-state buffers', 'Huit tampons trois états',
         ['~G1', 'A1', 'A2', 'A3', 'A4', 'A5', 'A6', 'A7', 'A8', 'GND', 'Y8', 'Y7', 'Y6', 'Y5', 'Y4', 'Y3', 'Y2', 'Y1', '~G2', 'VCC'],
         [b('Acht Treiber', 'eight buffers', 'huit tampons', [f'A{k}' for k in range(1, 9)] + ['~G1', '~G2'], [f'Y{k}' for k in range(1, 9)])]),
    Chip('74573', 'Acht D-Latches mit Tristate', 'Octal transparent D latches, tri-state', 'Huit verrous D trois états',
         ['~OE', '1D', '2D', '3D', '4D', '5D', '6D', '7D', '8D', 'GND', 'LE', '8Q', '7Q', '6Q', '5Q', '4Q', '3Q', '2Q', '1Q', 'VCC'],
         [b('Acht D-Latches', 'eight D latches', 'huit verrous D', [f'{k}D' for k in range(1, 9)] + ['LE', '~OE'], [f'{k}Q' for k in range(1, 9)])]),
    Chip('74574', 'Acht D-Flipflops mit Tristate', 'Octal D flip-flops, tri-state', 'Huit bascules D trois états',
         ['~OE', '1D', '2D', '3D', '4D', '5D', '6D', '7D', '8D', 'GND', 'CLK', '8Q', '7Q', '6Q', '5Q', '4Q', '3Q', '2Q', '1Q', 'VCC'],
         [b('Acht D-Flipflops', 'eight D flip-flops', 'huit bascules D', [f'{k}D' for k in range(1, 9)] + ['CLK', '~OE'], [f'{k}Q' for k in range(1, 9)])]),
    Chip('74595', 'Schieberegister mit Ausgangsregister', '8-bit shift register with output latches', 'Registre à décalage avec verrous',
         ['QB', 'QC', 'QD', 'QE', 'QF', 'QG', 'QH', 'GND', "QH'", '~SRCLR', 'SRCLK', 'RCLK', '~OE', 'SER', 'QA', 'VCC'],
         [b('Schieberegister', 'shift register', 'registre à décalage', ['SER', 'SRCLK', '~SRCLR', 'RCLK', '~OE'],
            ['QA', 'QB', 'QC', 'QD', 'QE', 'QF', 'QG', 'QH', "QH'"])]),
    Chip('74688', 'Vergleicher 8 Bit', '8-bit identity comparator', 'Comparateur d\'identité 8 bits',
         ['~G', 'P0', 'Q0', 'P1', 'Q1', 'P2', 'Q2', 'P3', 'Q3', 'GND', 'P4', 'Q4', 'P5', 'Q5', 'P6', 'Q6', 'P7', 'Q7', '~P=Q', 'VCC'],
         [b('Vergleicher', 'comparator', 'comparateur', [f'P{k}' for k in range(8)] + [f'Q{k}' for k in range(8)] + ['~G'], ['~P=Q'])]),
    Chip('744017', 'Dezimalzähler mit 10 Ausgängen (HC4017)', 'Decade counter with 10 decoded outputs (HC4017)',
         'Compteur décimal à 10 sorties (HC4017)',
         ['Q5', 'Q1', 'Q0', 'Q2', 'Q6', 'Q7', 'Q3', 'GND', 'Q8', 'Q4', 'Q9', '~Q5-9', '~CP1', 'CP0', 'MR', 'VCC'],
         [b('Dezimalzähler', 'decade counter', 'compteur décimal', ['CP0', '~CP1', 'MR'], [f'Q{k}' for k in range(10)] + ['~Q5-9'])]),
    Chip('744020', 'Binärzähler 14 Stufen (HC4020)', '14-stage binary counter (HC4020)', 'Compteur binaire 14 étages (HC4020)',
         ['Q11', 'Q12', 'Q13', 'Q5', 'Q4', 'Q6', 'Q3', 'GND', 'Q0', '~CP', 'MR', 'Q8', 'Q7', 'Q9', 'Q10', 'VCC'],
         [b('Binärzähler', 'binary counter', 'compteur binaire', ['~CP', 'MR'], ['Q0'] + [f'Q{k}' for k in range(3, 14)])]),
    Chip('744040', 'Binärzähler 12 Stufen (HC4040)', '12-stage binary counter (HC4040)', 'Compteur binaire 12 étages (HC4040)',
         ['Q11', 'Q5', 'Q4', 'Q6', 'Q3', 'Q2', 'Q1', 'GND', 'Q0', '~CP', 'MR', 'Q8', 'Q7', 'Q9', 'Q10', 'VCC'],
         [b('Binärzähler', 'binary counter', 'compteur binaire', ['~CP', 'MR'], [f'Q{k}' for k in range(12)])]),
    Chip('744066', 'Vier Analogschalter (HC4066)', 'Quad bilateral switches (HC4066)', 'Quatre commutateurs analogiques (HC4066)',
         ['1Y', '1Z', '2Z', '2Y', '2E', '3E', 'GND', '3Y', '3Z', '4Z', '4Y', '4E', '1E', 'VCC'],
         [b('Analogschalter', 'bilateral switch', 'commutateur analogique', [f'{u}Y', f'{u}E'], [f'{u}Z']) for u in range(1, 5)]),
    Chip('744511', 'BCD-zu-7-Segment-Latch/Decoder/Treiber (HC4511)', 'BCD to 7-segment latch/decoder/driver (HC4511)',
         'Verrou/décodeur/pilote BCD 7 segments (HC4511)',
         ['D1', 'D2', '~LT', '~BL', '~LE', 'D3', 'D0', 'GND', 'e', 'd', 'c', 'b', 'a', 'g', 'f', 'VCC'],
         [b('BCD-zu-7-Segment-Decoder', 'BCD to 7-segment decoder', 'décodeur BCD 7 segments', ['D0', 'D1', 'D2', 'D3', '~LE', '~BL', '~LT'],
            ['a', 'b', 'c', 'd', 'e', 'f', 'g'])]),
]


HEX20 = ['1A', '1B', '1Y', '2A', '2B', '2Y', '3A', '3B', '3Y', 'GND', '4Y', '4A', '4B', '5Y', '5A', '5B', '6Y', '6A', '6B', 'VCC']

CHIPS += [
    Chip('74242', 'Vier Bus-Transceiver, invertierend', 'Quad inverting bus transceivers', 'Quatre émetteurs-récepteurs de bus inverseurs',
         ['~GAB', 'NC', 'A1', 'A2', 'A3', 'A4', 'GND', 'B4', 'B3', 'B2', 'B1', 'NC', 'GBA', 'VCC'],
         [b('Bus-Transceiver, invertierend', 'inverting bus transceiver', 'émetteur-récepteur inverseur', ['A1', 'A2', 'A3', 'A4', '~GAB', 'GBA'], ['B1', 'B2', 'B3', 'B4'])]),
    Chip('74243', 'Vier Bus-Transceiver', 'Quad bus transceivers', 'Quatre émetteurs-récepteurs de bus',
         ['~GAB', 'NC', 'A1', 'A2', 'A3', 'A4', 'GND', 'B4', 'B3', 'B2', 'B1', 'NC', 'GBA', 'VCC'],
         [b('Bus-Transceiver', 'bus transceiver', 'émetteur-récepteur de bus', ['A1', 'A2', 'A3', 'A4', '~GAB', 'GBA'], ['B1', 'B2', 'B3', 'B4'])]),
    Chip('74564', 'Acht D-Flipflops mit Tristate, invertierend', 'Octal D flip-flops, tri-state, inverting', 'Huit bascules D trois états, inverseuses',
         ['~OE', '1D', '2D', '3D', '4D', '5D', '6D', '7D', '8D', 'GND', 'CLK', '~8Q', '~7Q', '~6Q', '~5Q', '~4Q', '~3Q', '~2Q', '~1Q', 'VCC'],
         [b('Acht D-Flipflops', 'eight D flip-flops', 'huit bascules D', [f'{k}D' for k in range(1, 9)] + ['CLK', '~OE'], [f'~{k}Q' for k in range(1, 9)])]),
    Chip('74575', 'Acht D-Flipflops mit Tristate und Löschen', 'Octal D flip-flops with clear, tri-state', 'Huit bascules D trois états avec effacement',
         ['~CLR', '~OE', '1D', '2D', '3D', '4D', '5D', '6D', '7D', '8D', 'NC', 'GND', 'NC', 'CLK', '8Q', '7Q', '6Q', '5Q', '4Q', '3Q', '2Q', '1Q', 'NC', 'VCC'],
         [b('Acht D-Flipflops', 'eight D flip-flops', 'huit bascules D', [f'{k}D' for k in range(1, 9)] + ['CLK', '~CLR', '~OE'], [f'{k}Q' for k in range(1, 9)])]),
    Chip('74623', 'Bus-Transceiver 8 Bit, zwei Freigaben', 'Octal bus transceiver, dual enable', 'Émetteur-récepteur de bus 8 bits, deux validations',
         ['GAB', 'A1', 'A2', 'A3', 'A4', 'A5', 'A6', 'A7', 'A8', 'GND', 'B8', 'B7', 'B6', 'B5', 'B4', 'B3', 'B2', 'B1', '~GBA', 'VCC'],
         [b('Bus-Transceiver', 'bus transceiver', 'émetteur-récepteur de bus', [f'A{k}' for k in range(1, 9)] + ['GAB', '~GBA'], [f'B{k}' for k in range(1, 9)])]),
    Chip('74805', 'Sechs NOR-Treiber', 'Hex 2-input NOR drivers', 'Six pilotes NON-OU', HEX20,
         g('NOR-Treiber', 'NOR driver', 'pilote NON-OU', NOR, True, 2, 6, 'buf')),
    Chip('74808', 'Sechs UND-Treiber', 'Hex 2-input AND drivers', 'Six pilotes ET', HEX20,
         g('UND-Treiber', 'AND driver', 'pilote ET', AND, False, 2, 6, 'buf')),
    Chip('74832', 'Sechs ODER-Treiber', 'Hex 2-input OR drivers', 'Six pilotes OU', HEX20,
         g('ODER-Treiber', 'OR driver', 'pilote OU', OR, False, 2, 6, 'buf')),
    Chip('74841', 'Zehn D-Latches mit Tristate', '10-bit transparent D latches, tri-state', 'Dix verrous D trois états',
         ['~OE', '1D', '2D', '3D', '4D', '5D', '6D', '7D', '8D', '9D', '10D', 'GND', 'LE', '10Q', '9Q', '8Q', '7Q', '6Q', '5Q', '4Q', '3Q', '2Q', '1Q', 'VCC'],
         [b('Zehn D-Latches', 'ten D latches', 'dix verrous D', [f'{k}D' for k in range(1, 11)] + ['LE', '~OE'], [f'{k}Q' for k in range(1, 11)])]),
    Chip('744053', 'Drei analoge 2-zu-1-Umschalter (HC4053)', 'Triple 2-channel analog multiplexer (HC4053)', 'Trois multiplexeurs analogiques 2 voies (HC4053)',
         ['Y1', 'Y0', 'Z1', 'Z', 'Z0', '~E', 'VEE', 'GND', 'S3', 'S2', 'S1', 'X0', 'X1', 'X', 'Y', 'VCC'],
         [b('Drei Analog-Umschalter', 'three analog switches', 'trois commutateurs analogiques', ['X0', 'X1', 'Y0', 'Y1', 'Z0', 'Z1', 'S1', 'S2', 'S3', '~E', 'VEE'], ['X', 'Y', 'Z'])]),
    Chip('744060', 'Binärzähler 14 Stufen mit Oszillator (HC4060)', '14-stage binary counter with oscillator (HC4060)',
         'Compteur binaire 14 étages avec oscillateur (HC4060)',
         ['Q12', 'Q13', 'Q14', 'Q6', 'Q5', 'Q7', 'Q4', 'GND', 'φO', '~φO', 'φI', 'MR', 'Q9', 'Q8', 'Q10', 'VCC'],
         [b('Binärzähler mit Oszillator', 'binary counter with oscillator', 'compteur binaire avec oscillateur', ['φI', 'MR'],
            ['φO', '~φO'] + [f'Q{k}' for k in (4, 5, 6, 7, 8, 9, 10, 12, 13, 14)])]),
    Chip('744514', '4-zu-16-Decoder mit Latch (HC4514)', '4-to-16 line decoder with latch (HC4514)', 'Décodeur 4 vers 16 avec verrou (HC4514)',
         ['LE', 'A0', 'A1', 'Y7', 'Y6', 'Y5', 'Y4', 'Y3', 'Y1', 'Y2', 'Y0', 'GND', 'Y13', 'Y12', 'Y15', 'Y14', 'Y9', 'Y8', 'Y11', 'Y10', 'A2', 'A3', '~E', 'VCC'],
         [b('4-zu-16-Decoder', '4-to-16 decoder', 'décodeur 4 vers 16', ['A0', 'A1', 'A2', 'A3', 'LE', '~E'], [f'Y{k}' for k in range(16)])]),
]


CHIPS += [
    Chip('74131', '3-zu-8-Decoder mit Adressregister', '3-to-8 decoder with address registers', 'Décodeur 3 vers 8 avec registres d\'adresse',
         ['A', 'B', 'C', 'CLK', '~G2', 'G1', '~Y7', 'GND', '~Y6', '~Y5', '~Y4', '~Y3', '~Y2', '~Y1', '~Y0', 'VCC'],
         [b('3-zu-8-Decoder', '3-to-8 decoder', 'décodeur 3 vers 8', ['A', 'B', 'C', 'CLK', 'G1', '~G2'], [f'~Y{k}' for k in range(8)])]),
    Chip('74230', 'Acht Treiber mit Tristate, vier invertierend', 'Octal tri-state buffers, four inverting', 'Huit tampons trois états, quatre inverseurs',
         ['~1G', '1A1', '2Y4', '1A2', '2Y3', '1A3', '2Y2', '1A4', '2Y1', 'GND', '2A1', '~1Y4', '2A2', '~1Y3', '2A3', '~1Y2', '2A4', '~1Y1', '~2G', 'VCC'],
         [b('Vier invertierende Treiber', 'four inverting buffers', 'quatre tampons inverseurs', ['1A1', '1A2', '1A3', '1A4', '~1G'], ['~1Y1', '~1Y2', '~1Y3', '~1Y4']),
          b('Vier Treiber', 'four buffers', 'quatre tampons', ['2A1', '2A2', '2A3', '2A4', '~2G'], ['2Y1', '2Y2', '2Y3', '2Y4'])]),
    Chip('74231', 'Acht invertierende Treiber mit Tristate', 'Octal inverting tri-state buffers', 'Huit tampons inverseurs trois états',
         ['~1G', '1A1', '~2Y4', '1A2', '~2Y3', '1A3', '~2Y2', '1A4', '~2Y1', 'GND', '2A1', '~1Y4', '2A2', '~1Y3', '2A3', '~1Y2', '2A4', '~1Y1', '2G', 'VCC'],
         [b('Vier invertierende Treiber', 'four inverting buffers', 'quatre tampons inverseurs', ['1A1', '1A2', '1A3', '1A4', '~1G'], ['~1Y1', '~1Y2', '~1Y3', '~1Y4']),
          b('Vier invertierende Treiber', 'four inverting buffers', 'quatre tampons inverseurs', ['2A1', '2A2', '2A3', '2A4', '2G'], ['~2Y1', '~2Y2', '~2Y3', '~2Y4'])]),
    Chip('74250', '16-zu-1-Multiplexer mit Tristate', '16-to-1 multiplexer, tri-state', 'Multiplexeur 16 vers 1, trois états',
         ['E7', 'E6', 'E5', 'E4', 'E3', 'E2', 'E1', 'E0', '~OE', '~W', 'D', 'GND', 'C', 'B', 'A', 'E15', 'E14', 'E13', 'E12', 'E11', 'E10', 'E9', 'E8', 'VCC'],
         [b('16-zu-1-Multiplexer', '16-to-1 multiplexer', 'multiplexeur 16 vers 1', [f'E{k}' for k in range(16)] + ['A', 'B', 'C', 'D', '~OE'], ['~W'])]),
    Chip('74539', 'Zwei 2-zu-4-Decoder mit Tristate', 'Dual 2-to-4 decoders, tri-state', 'Deux décodeurs 2 vers 4, trois états',
         ['2Y2', '2Y1', '2Y0', '2P', '~2OE', '1A0', '1A1', '1Y3', '1Y2', 'GND', '1Y1', '1Y0', '1P', '~1OE', '~1E', '~2E', '2A0', '2A1', '2Y3', 'VCC'],
         dual(None, ('2-zu-4-Decoder', '2-to-4 decoder', 'décodeur 2 vers 4'), ['#A0', '#A1', '~#E', '#P', '~#OE'], ['#Y0', '#Y1', '#Y2', '#Y3'])),
    Chip('74580', 'Acht D-Latches mit Tristate, invertierend', 'Octal transparent D latches, tri-state, inverting', 'Huit verrous D trois états, inverseurs',
         ['~OE', '1D', '2D', '3D', '4D', '5D', '6D', '7D', '8D', 'GND', 'LE', '~8Q', '~7Q', '~6Q', '~5Q', '~4Q', '~3Q', '~2Q', '~1Q', 'VCC'],
         [b('Acht D-Latches', 'eight D latches', 'huit verrous D', [f'{k}D' for k in range(1, 9)] + ['LE', '~OE'], [f'~{k}Q' for k in range(1, 9)])]),
    Chip('7440104', 'Universal-Schieberegister 4 Bit mit Tristate (HC40104)', '4-bit universal shift register, tri-state (HC40104)',
         'Registre à décalage universel 4 bits, trois états (HC40104)',
         ['OE', 'DSR', 'D0', 'D1', 'D2', 'D3', 'DSL', 'GND', 'S0', 'S1', 'CP', 'Q3', 'Q2', 'Q1', 'Q0', 'VCC'],
         [b('Universal-Schieberegister', 'universal shift register', 'registre à décalage universel', ['D0', 'D1', 'D2', 'D3', 'DSR', 'DSL', 'S0', 'S1', 'CP', 'OE'], ['Q0', 'Q1', 'Q2', 'Q3'])]),
]


# How each pin list was checked: against KiCad's symbol libraries (pin roles and the split into units, by a script)
# or against the manufacturer's data sheet (document number). For the 74230 two manufacturers differ: National's
# DM74AS230 has four inverting and four true buffers (as here and in sPlan), TI's later SN74AS230A eight inverting ones.
DATASHEET = {'7441': 'National DM7441A (TTL Databook 1976, p. 2-1)', '7443': 'TI SN7443A (TTL Data Book 1981, p. 7-15)',
             '7444': 'TI SN7444A (TTL Data Book 1981, p. 7-15)', '7477': 'TI SN5477 (TTL Data Book 1981, p. 5-23)', '7416': 'TI SDLS031', '7417': 'TI SDLS032', '7434': 'TI SDAS058', '7435': 'TI SDAS011', '7445': 'TI SDLS110',
             '74131': 'TI SDAS060', '74230': 'National DM74AS230 (1982)', '74231': 'National DM74AS231 (1982)', '74250': 'TI SDAS137',
             '74266': 'TI SDLS151', '74539': 'Fairchild DS009552 (74F539)', '74564': 'TI SDAS164', '74575': 'TI SDAS165',
             '74580': 'TI SDAS277', '74623': 'TI SDLS185', '74805': 'TI SDAS023', '74808': 'TI SDAS018', '74832': 'TI SDAS017',
             '74841': 'TI SDAS059', '744060': 'TI SCHS207', '744511': 'TI SCHS279', '744514': 'TI SCHS280',
             '7440104': 'ST HCF40104B (4000B version)'}
UNVERIFIED = set()


def checked_by(number):
    return DATASHEET.get(number) or (None if number in UNVERIFIED else 'KiCad')
