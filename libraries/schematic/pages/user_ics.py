# The pages of the folder "USER" with ICs: op-amps (ANLG), CMOS 4000 gates, flip-flops and whole ICs (CD 40xxx),
# special functions (SPC FCT), whole 74xx ICs (Texas Instruments (Digital)) and a few parts (MISC). Op-amps as the
# triangle of DIN EN 60617-13, logic elements after IEC 60617-12 (qualifying symbols, C1/1D, dynamic and negated inputs,
# the common control block, open-collector and Schmitt-trigger marks), the other ICs as boxes after the logic diagrams
# of their data sheets: inputs left, outputs right, the supply above and below, the pin number at every lead (the
# contact's name) and the pin's name inside ("~" before a name: active low, drawn with the negation circle). Pin
# assignments after the manufacturers' data sheets named at each IC; the 74xx from ttl74.py. Crystal and fuse after
# DIN EN 60617-4/-7, the heating spiral as the heating element of DIN EN 60617-4 (04-01-12), the BNC socket seen from
# the front with the two bayonet lugs of the female connector (IEC 61169-8).
USR = 'USER'
PARENT_VALUE = '<PARENT_VALUE>'
MINUS = '−'


def small_pins(s, size=1.6):
    for o in s.parts:
        if o['type'] == 'contact':
            o['font'] = {'height': size}
    return s


def as_child(s):
    """The symbol as a child of a parent IC: designator and value come from the parent once linked."""
    s.prefix, s.numbered, s.value = CHILD, False, PARENT_VALUE
    return s


# --- Op-amps: the triangle of the page "OP's", the pin numbers at the leads
def opamp(prefix, caption, value, minus, plus, out, minus_top=True, top=(), bottom=(), reach=2 * P):
    """Inputs at y = -P and P, the output at (6 P, 0); further pins (name, x) leave the upper or lower edge of the
    triangle and end at y = -reach or reach (3 P keeps the pin numbers of crowded leads clear of the triangle)."""
    s = Symbol(prefix, caption, value)
    s.poly((P, -2 * P), (P, 2 * P), (5 * P, 0), fill=None, w=0.35)
    s.line((0, -P), (P, -P)).line((0, P), (P, P)).line((5 * P, 0), (6 * P, 0))
    y_minus, y_plus = (-P, P) if minus_top else (P, -P)
    s.text(MINUS, P + 0.5, y_minus - 1.4, 2.5).text('+', P + 0.5, y_plus - 1.4, 2.5)
    s.pin(minus, (0, y_minus), label=(0.2, y_minus - 2.2), shown=True)
    s.pin(plus, (0, y_plus), label=(0.2, y_plus - 2.2), shown=True)
    s.pin(out, (6 * P, 0), label=(6 * P - 0.2, -2.2), align='right', shown=True)
    right = 0
    for pins, sign in ((top, -1), (bottom, 1)):
        for name, x in pins:
            s.line((x, sign * (2 * P - (x - P) / 2)), (x, sign * reach))
            s.pin(name, (x, sign * reach), label=(x + 0.4, -reach) if sign < 0 else (x + 0.4, reach - 2.0), shown=True)
            right = max(right, x)
    x = max(3.6 * P, right + 2.6)
    return small_pins(s).labels((x, -2 * P - 1.0), (x, 2 * P - 1.8))


def supply_leads(caption, plus, ground, text):
    """The supply of an op-amp as a block of its own, to be put on the supply pins: the positive supply with its value
    above contact `plus`, ground below contact `ground`, the two contacts 4 P apart as on the op-amps."""
    s = Symbol('', caption, numbered=False, listed=False, shown=False)
    s.line((0, 0), (0, -P)).line((-1.8, -P), (1.8, -P), w=0.35).text(text, 0, -P - 3.0, 2.5, 'centre')
    s.line((0, 4 * P), (0, 5 * P))
    for k, half in enumerate((2.0, 1.3, 0.6)):
        s.line((-half, 5 * P + k * 0.8), (half, 5 * P + k * 0.8), w=0.35)
    s.pin(plus, (0, 0), label=(0.4, -2.0), shown=True).pin(ground, (0, 4 * P), label=(0.4, 4 * P + 0.2), shown=True)
    return small_pins(s).labels((2.4, P), (2.4, 2 * P + 0.4))


# --- Boxes
def lead_in(s, x0, y, name, pin, kind=''):
    """An input lead at the left side x0: negation circle for "~", the dynamic sign for kind 'dyn'; the pin number
    above the lead, the name inside."""
    text = name.lstrip('~')
    if name.startswith('~'):
        s.circle(x0 - 0.6, y, 1.2).line((0, y), (x0 - 1.2, y))
    else:
        s.line((0, y), (x0, y))
    if kind == 'dyn':
        s.line((x0, y - 0.9), (x0 + 1.2, y), (x0, y + 0.9))
    if text:
        s.text(text, x0 + (1.5 if kind == 'dyn' else 0.6), y - 1.0, 1.8)
    s.pin(str(pin), (0, y), label=(0.2, y - 2.2), shown=True)


def lead_out(s, x1, y, name, pin, kind=''):
    """An output lead at the right side x1; kind 'tri' puts the three-state mark before the name."""
    end = x1 + P
    text = name.lstrip('~')
    if name.startswith('~'):
        s.circle(x1 + 0.6, y, 1.2).line((x1 + 1.2, y), (end, y))
    else:
        s.line((x1, y), (end, y))
    inner = x1 - 0.6
    if kind == 'tri':
        s.line((x1 - 2.2, y - 0.6), (x1 - 0.6, y - 0.6), (x1 - 1.4, y + 0.6), (x1 - 2.2, y - 0.6), w=0.18)
        inner = x1 - 2.8
    if text:
        s.text(text, inner, y - 1.0, 1.8, 'right')
    s.pin(str(pin), (end, y), label=(end - 0.2, y - 2.2), align='right', shown=True)


def item(entry):
    """(name, pin) or (name, pin, kind)."""
    return entry if len(entry) == 3 else (entry[0], entry[1], '')


def ic_box(prefix, caption, value, left, right, top=(), bottom=(), width=None, dividers=(), gap=2):
    """An IC after the logic diagram of its data sheet: `left` and `right` hold the pins down the sides row by row
    (None leaves a row free), `top` and `bottom` the supply pins, `gap` pitches apart in the middle of the edges.
    `dividers` draws a line across the box half a row before these rows (4.5: through the free row 4 between the
    sections of an IC)."""
    rows = max(len(left), len(right))
    first = 2 if top else 1
    h = (first + rows + (1 if bottom else 0)) * P
    if width is None:
        longest = lambda side: max([len(item(e)[0].lstrip('~')) + (1 if item(e)[2] in ('dyn', 'tri') else 0) for e in side if e] or [0])
        width = max(4, math.ceil(((longest(left) + longest(right)) * 1.15 + 3.0) / P), 2 * max(len(top), len(bottom)) + 2)
    s = Symbol(prefix, caption, value)
    x0, x1 = P, (width + 1) * P
    s.rect(x0, 0, x1, h, w=0.35)
    for k, e in enumerate(left):
        if e:
            lead_in(s, x0, (first + k) * P, *item(e))
    for k, e in enumerate(right):
        if e:
            name, pin, kind = item(e)
            lead_out(s, x1, (first + k) * P, name, pin, kind)
    for k in dividers:
        y = (first + k - 0.5) * P
        s.line((x0, y), (x1, y))
    centre = (width // 2 + 1) * P
    for pins, edge, out in ((top, 0, -P), (bottom, h, P)):
        for k, (name, pin) in enumerate(pins):
            x = centre + ((len(pins) - 1) * gap // -2 + k * gap) * P
            s.line((x, edge), (x, edge + out))
            s.text(name, x, 0.4 if out < 0 else h - 2.4, 1.8, 'centre')
            s.pin(str(pin), (x, edge + out), label=(x + 0.4, -2.3) if out < 0 else (x + 0.4, h + 0.3), shown=True)
    s.labels((x0, -3.4 - (P if top else 0)), (x0, h + 0.6 + (P if bottom else 0)))
    return small_pins(s)


def gate(sign, negated, ins, out, caption, value, mark=None):
    """One gate of an IC after IEC 60617-12 (gate_iec) with the pin numbers of its place in the package."""
    s = gate_iec(sign, len(ins), negated, caption)
    s.value = value
    contacts = [o for o in s.parts if o['type'] == 'contact']
    for o, n in zip(contacts, list(ins) + [out]):
        o['name'] = o['text'] = str(n)
        o['visible'] = True
    if mark == 'buf':
        y = contacts[-1]['pin'][1]
        s.parts[1]['pos'] = pt((2 * P - 1.6, 0.8))
        s.line((3 * P - 1.9, y - 0.8), (3 * P - 0.7, y), (3 * P - 1.9, y + 0.8), (3 * P - 1.9, y - 0.8), w=0.18)
    return small_pins(s)


def iec_ff(caption, value, inputs, outputs):
    """A flip-flop of a CMOS IC after IEC 60617-12 (S, 1D or 1J/1K, C1 dynamic, R; Q and the negated Q)."""
    return ic_box('IC', caption, value, inputs, outputs, width=4)


def monoflop(caption, value, pins):
    """A retriggerable monostable of the CD4098B: the single-pulse sign in the first row, the timing pins CX and
    RXCX, the triggers +TR (rising edge) and -TR (falling edge), the reset; outputs Q and Q negated. `pins` maps the
    names to the pin numbers."""
    s = ic_box('IC', caption, value,
               [None, ('CX', pins['CX']), ('RXCX', pins['RXCX']), ('+TR', pins['+TR'], 'dyn'), ('~' + MINUS + 'TR', pins['-TR'], 'dyn'),
                ('~R', pins['R'])],
               [None, None, None, ('Q', pins['Q']), None, ('~Q', pins['~Q'])], width=5)
    pulse(s, 3.5 * P - 1.0, P + 0.6)
    return s


# --- Logic diagrams of the CD4000 ICs (TI data sheets, document numbers at each)
def cmos(caption, value, left, right, n=16, dividers=(), width=None):
    """A whole CMOS IC of n pins: VDD (pin n) above, VSS (pin n/2) below."""
    return ic_box('IC', caption, value, left, right, [('VDD', n)], [('VSS', n // 2)], width=width, dividers=dividers)


add = page(USR, 'ANLG', 'ANLG', 'ANLG')
# TI SLOS094H (µA741), SNOSBH0D (LF355), SLOS058C (LM348), SNOSBS0D (LM301A), SLOS090D (TLC271); EL2001: Elantec
# EL2001C, Rev G, December 1995 (DIP: V+ 1, IN 2, V− 4, OUT 7).
INPUTS = ((True, '(− oben)', '(inverting input at the top)', '(entrée inverseuse en haut)'),
          (False, '(+ oben)', '(non-inverting input at the top)', '(entrée non inverseuse en haut)'))
for top_minus, de, en, fr in INPUTS:
    add(opamp('A', C(f'Operationsverstärker µA741 {de}', f'Op-amp µA741 {en}', f'Ampli op µA741 {fr}'), 'µA741', '2', '3', '6', top_minus))
for top_minus, de, en, fr in INPUTS:
    add(opamp('A', C(f'Operationsverstärker LF355 mit Versorgung {de}', f'Op-amp LF355 with supply {en}', f'Ampli op LF355 avec alimentation {fr}'),
              'LF355', '2', '3', '6', top_minus, top=[('7', 3 * P)], bottom=[('4', 3 * P)]))
for top_minus, de, en, fr in INPUTS:
    add(opamp('A', C(f'Operationsverstärker LM348 {de}', f'Op-amp LM348 {en}', f'Ampli op LM348 {fr}'), 'LM348', '2', '3', '1', top_minus))
add(opamp('A', C('Operationsverstärker LM301 mit Kompensation', 'Op-amp LM301 with compensation', 'Ampli op LM301 avec compensation'),
          'LM301', '2', '3', '6', top=[('7', 2 * P), ('1', 3 * P), ('8', 4 * P)], bottom=[('4', 3 * P)], reach=3 * P))
add(opamp('A', C('Operationsverstärker TLC271 (programmierbar)', 'Op-amp TLC271 (programmable)', 'Ampli op TLC271 (programmable)'),
          'TLC271', '2', '3', '6', top=[('7', 3 * P), ('8', 4 * P)], bottom=[('1', 2 * P), ('4', 3 * P), ('5', 4 * P)], reach=3 * P))
s = Symbol('IC', C('Pufferverstärker EL2001', 'Buffer amplifier EL2001', 'Amplificateur tampon EL2001'), 'EL2001')
s.poly((P, -2 * P), (P, 2 * P), (5 * P, 0), fill=None, w=0.35).line((0, 0), (P, 0)).line((5 * P, 0), (6 * P, 0))
s.text('+1', 2 * P - 0.6, -1.25, 2.5)
s.line((3 * P, -P), (3 * P, -2 * P)).line((3 * P, P), (3 * P, 2 * P))
s.pin('2', (0, 0), label=(0.2, -2.2), shown=True).pin('7', (6 * P, 0), label=(6 * P - 0.2, -2.2), align='right', shown=True)
s.pin('1', (3 * P, -2 * P), label=(3 * P + 0.4, -2 * P), shown=True).pin('4', (3 * P, 2 * P), label=(3 * P + 0.4, 2 * P - 2.0), shown=True)
add(small_pins(s).labels((3.6 * P + 0.6, -2 * P - 1.0), (3.6 * P + 0.6, 2 * P - 1.8)))
add(supply_leads(C('Versorgung Operationsverstärker', 'Op-amp supply', 'Alimentation ampli op'), '8', '4', '+9 V'))

add = page(USR, 'CD 40xxx', 'CD 40xxx', 'CD 40xxx')
# CD4009UB: TI SCHS020C. Hex inverting buffer; the buffer sign (amplification) of IEC 60617-12 at the output.
inverters = [(3, 2), (5, 4), (7, 6), (9, 10), (11, 12), (14, 15)]
for k, (i, o) in enumerate(inverters, 1):
    add(gate('1', True, [i], o, C(f'4009 Inverter {k}', f'4009 inverter {k}', f'4009 inverseur {k}'), '4009', 'buf'))
# CD4013B: TI SCHS023E.
ff13 = [{'S': 6, 'D': 5, 'C': 3, 'R': 4, 'Q': 1, '~Q': 2}, {'S': 8, 'D': 9, 'C': 11, 'R': 10, 'Q': 13, '~Q': 12}]


def dff(caption, value, p):
    return iec_ff(caption, value, [('S', p['S']), ('1D', p['D']), ('C1', p['C'], 'dyn'), ('R', p['R'])], [('Q', p['Q']), None, None, ('~Q', p['~Q'])])


for p, letter in zip(ff13, 'ab'):
    add(dff(C(f'4013 D-Flipflop {letter}', f'4013 D flip-flop {letter}', f'4013 bascule D {letter}'), '4013', p))
# CD4017B: TI SCHS027C.
q4017 = [3, 2, 4, 7, 10, 1, 5, 6, 9, 11]
add(cmos(C('4017 Dezimalzähler', '4017 decade counter', '4017 compteur décimal'), '4017',
         [('CLOCK', 14, 'dyn'), ('CLK INH', 13), ('RESET', 15)], [(f'Q{k}', n) for k, n in enumerate(q4017)] + [('CO', 12)]))
# CD4023B: TI SCHS021D.
for letter, ins, out in (('a', (1, 2, 8), 9), ('b', (3, 4, 5), 6), ('c', (11, 12, 13), 10)):
    add(gate('&', True, ins, out, C(f'4023 NAND {letter}', f'4023 NAND {letter}', f'4023 NON-ET {letter}'), '4023'))
# CD4098B: TI SCHS065C.
mono98 = [{'CX': 1, 'RXCX': 2, 'R': 3, '+TR': 4, '-TR': 5, 'Q': 6, '~Q': 7}, {'CX': 15, 'RXCX': 14, 'R': 13, '+TR': 12, '-TR': 11, 'Q': 10, '~Q': 9}]
for p, letter in zip(mono98, 'ab'):
    add(monoflop(C(f'4098 Monoflop {letter}', f'4098 monostable {letter}', f'4098 monostable {letter}'), '4098', p))
add(as_child(gate('1', True, [3], 2, C('4009 Inverter (Child)', '4009 inverter (child)', '4009 inverseur (enfant)'), '4009', 'buf')))
# CD4012B: TI SCHS021D.
add(gate('&', True, (2, 3, 4, 5), 1, C('4012 NAND (4 Eingänge)', '4012 NAND (4 inputs)', '4012 NON-ET (4 entrées)'), '4012'))
p1, p2 = ff13
add(cmos(C('4013 (komplett)', '4013 (complete)', '4013 (complet)'), '4013',
         [('S', p1['S']), ('1D', p1['D']), ('C1', p1['C'], 'dyn'), ('R', p1['R']), None, ('S', p2['S']), ('2D', p2['D']), ('C2', p2['C'], 'dyn'), ('R', p2['R'])],
         [('Q', p1['Q']), None, None, ('~Q', p1['~Q']), None, ('Q', p2['Q']), None, None, ('~Q', p2['~Q'])], 14, dividers=(4.5,), width=4))
add(as_child(dff(C('4013 D-Flipflop (Child)', '4013 D flip-flop (child)', '4013 bascule D (enfant)'), '4013', ff13[0])))
# CD4516B: TI SCHS071B.
add(cmos(C('4516 Vorwärts-/Rückwärtszähler', '4516 up/down counter', '4516 compteur-décompteur'), '4516',
         [('PE', 1), ('P1', 4), ('P2', 12), ('P3', 13), ('P4', 3), ('~CI', 5), ('U/D', 10), ('CLK', 15, 'dyn'), ('RESET', 9)],
         [None, ('Q1', 6), ('Q2', 11), ('Q3', 14), ('Q4', 2), ('~CO', 7)]))
# The CD4017B after IEC 60617-12 as the IEC logic symbol of the same counter in Nexperia 74HC_HCT4017 Rev. 8 (Fig. 3):
# CTRDIV10/DEC, the embedded & of CLOCK (14) and CLOCK INHIBIT (13, active low) as count input, CT=0 at RESET (15), the
# decoded outputs 0 to 9 and the carry, low for CT≥5 (12).
s = Symbol('IC', C('4017 (Kasten)', '4017 (box)', '4017 (rectangle)'), '4017')
x0, x1 = P, 8 * P
outputs = [(str(k), n) for k, n in enumerate(q4017)] + [('~CT≥5', 12)]
h = (len(outputs) + 2) * P
s.rect(x0, 0, x1, h, w=0.35).text('CTRDIV10/DEC', (x0 + x1) / 2, 0.4, 2.0, 'centre', bold=True)
s.rect(x0, 1.5 * P, x0 + 2 * P, 3.5 * P).text('&', x0 + 0.6, 1.5 * P + 0.3, 1.8)
for name, pin, y in (('', 14, 2 * P), ('~', 13, 3 * P), ('CT=0', 15, 4 * P)):
    lead_in(s, x0, y, name, pin)
for k, (name, pin) in enumerate(outputs):
    lead_out(s, x1, (k + 2) * P, name, pin)
for pin, y0, y1, label, text in ((16, 0, -P, (4 * P + 0.4, -2.3), 'VDD'), (8, h, h + P, (4 * P + 0.4, h + 0.3), 'VSS')):
    s.line((4 * P, y0), (4 * P, y1)).text(text, 4 * P - 0.4, label[1] + 0.1, 1.6, 'right')
    s.pin(str(pin), (4 * P, y1), label=label, shown=True)
add(small_pins(s).labels((x1 + 1.0, -P - 0.6), (x1 + 1.0, -P + 2.4)))
# CD4020B: TI SCHS030D.
add(cmos(C('4020 Binärzähler 14 Stufen', '4020 14-stage binary counter', '4020 compteur binaire 14 étages'), '4020',
         [('~CLK', 10, 'dyn'), ('RESET', 11)],
         [('Q1', 9), ('Q4', 7), ('Q5', 5), ('Q6', 4), ('Q7', 6), ('Q8', 13), ('Q9', 12), ('Q10', 14), ('Q11', 15), ('Q12', 1), ('Q13', 2), ('Q14', 3)]))
add(as_child(gate('&', True, (3, 4, 5), 6, C('4023 NAND (Child)', '4023 NAND (child)', '4023 NON-ET (enfant)'), '4023')))
# CD4027B: TI SCHS032D.
add(cmos(C('4027 JK-Flipflop (doppelt)', '4027 dual JK flip-flop', '4027 double bascule JK'), '4027',
         [('S', 9), ('1J', 10), ('C1', 13, 'dyn'), ('1K', 11), ('R', 12), None, ('S', 7), ('2J', 6), ('C2', 3, 'dyn'), ('2K', 5), ('R', 4)],
         [('Q', 15), None, None, None, ('~Q', 14), None, ('Q', 1), None, None, None, ('~Q', 2)], dividers=(5.5,), width=4))
# CD4028B: TI SCHS033C.
add(cmos(C('4028 BCD-zu-Dezimal-Decoder', '4028 BCD to decimal decoder', '4028 décodeur BCD vers décimal'), '4028',
         [('A', 10), ('B', 13), ('C', 12), ('D', 11)], [(f'Q{k}', n) for k, n in enumerate([3, 14, 2, 15, 1, 6, 7, 4, 9, 5])]))
# CD4046B: TI SCHS043B.
add(cmos(C('4046 PLL', '4046 PLL', '4046 PLL'), '4046',
         [('SIG IN', 14), ('COMP IN', 3), ('VCO IN', 9), ('INH', 5), ('C1A', 6), ('C1B', 7), ('R1', 11), ('R2', 12)],
         [('PC1 OUT', 2), ('PC2 OUT', 13), ('PULSES', 1), ('VCO OUT', 4), ('DEM OUT', 10), ('ZENER', 15)]))
# CD4051B: TI SCHS047O.
s = ic_box('IC', C('4051 Analogmultiplexer 8:1', '4051 8:1 analog multiplexer', '4051 multiplexeur analogique 8:1'), '4051',
           [(f'CH{k}', n) for k, n in enumerate([13, 14, 15, 12, 1, 5, 2, 4])] + [('A', 11), ('B', 10), ('C', 9), ('INH', 6)],
           [None, None, None, ('COM', 3)], [('VDD', 16)], [('VSS', 8), ('VEE', 7)])
add(s)
# CD4060B: TI SCHS049C.
add(cmos(C('4060 Binärzähler mit Oszillator', '4060 binary counter with oscillator', '4060 compteur binaire avec oscillateur'), '4060',
         [('~φI', 11, 'dyn'), ('RESET', 12)],
         [('φO', 9), ('~φO', 10)] + [(f'Q{k}', n) for k, n in ((4, 7), (5, 5), (6, 4), (7, 6), (8, 14), (9, 13), (10, 15), (12, 1), (13, 2), (14, 3))]))
# CD4066B: TI SCHS051J. Each switch drawn as a make contact operated by its control input.
switches = (('A', 1, 2, 13), ('B', 4, 3, 5), ('C', 8, 9, 6), ('D', 11, 10, 12))
s = cmos(C('4066 Analogschalter (vierfach)', '4066 quad analog switch', '4066 quadruple commutateur analogique'), '4066',
         [e for letter, a, b, c in switches for e in ((letter, a), ('CTL ' + letter, c))],
         [e for letter, a, b, c in switches for e in ((letter, b), None)], 14, width=7)
for k in range(4):
    y = (2 + 2 * k) * P
    xm = 5 * P
    s.line((P + 2.2, y), (xm - 1.6, y), (xm + 1.4, y - 1.6)).line((xm + 1.6, y), (8 * P - 2.2, y))
    s.parts.append({'type': 'line', 'points': [pt((P + 6.4, y + P)), pt((xm, y + P)), pt((xm, y - 0.9))], 'pen': {'width': W, 'style': 'dash'},
                    'electrical': False})
add(s)
# CD4076B: TI SCHS058C. Data input disable G1, G2 and output disable M, N.
add(cmos(C('4076 Register 4 Bit (Tristate)', '4076 4-bit register (tri-state)', '4076 registre 4 bits (trois états)'), '4076',
         [('D1', 14), ('D2', 13), ('D3', 12), ('D4', 11), ('G1', 9), ('G2', 10), ('CLK', 7, 'dyn'), ('RESET', 15), ('M', 1), ('N', 2)],
         [('Q1', 3, 'tri'), ('Q2', 4, 'tri'), ('Q3', 5, 'tri'), ('Q4', 6, 'tri')]))
# CD4094B: TI SCHS063B.
add(cmos(C('4094 Schieberegister 8 Bit', '4094 8-bit shift register', '4094 registre à décalage 8 bits'), '4094',
         [('DATA', 2), ('CLK', 3, 'dyn'), ('STROBE', 1), ('OE', 15)],
         [(f'Q{k}', n, 'tri') for k, n in ((1, 4), (2, 5), (3, 6), (4, 7), (5, 14), (6, 13), (7, 12), (8, 11))] + [('QS', 9), ("Q'S", 10)]))
add(as_child(monoflop(C('4098 Monoflop (Child)', '4098 monostable (child)', '4098 monostable (enfant)'), '4098', mono98[1])))
# CD40163B: TI SCHS103C.
add(cmos(C('40163 Synchronzähler 4 Bit', '40163 4-bit synchronous counter', '40163 compteur synchrone 4 bits'), '40163',
         [('P1', 3), ('P2', 4), ('P3', 5), ('P4', 6), ('PE', 7), ('TE', 10), ('~LOAD', 9), ('CLK', 2, 'dyn'), ('~CLEAR', 1)],
         [('Q1', 14), ('Q2', 13), ('Q3', 12), ('Q4', 11), ('CO', 15)]))

add = page(USR, 'SPC FCT', 'SPC FCT', 'FCT SPÉC')
# XR-2206: Exar data sheet Rev. 1.04, February 2008.
add(ic_box('IC', C('Funktionsgenerator XR2206', 'Function generator XR2206', 'Générateur de fonctions XR2206'), 'XR2206',
           [('TC1', 5), ('TC2', 6), ('TR1', 7), ('TR2', 8), ('FSKI', 9), ('AMSI', 1), ('WAVEA1', 13), ('WAVEA2', 14), ('SYMA1', 15), ('SYMA2', 16)],
           [('STO', 2), ('MO', 3), ('SYNCO', 11), ('BIAS', 10)], [('VCC', 4)], [('GND', 12)]))
# NE555: TI SLFS022K.
add(ic_box('IC', C('Timer NE555', 'Timer NE555', 'Temporisateur NE555'), 'NE555', [('TRIG', 2), ('THRS', 6), ('DISCH', 7)], [None, ('OUT', 3)],
           [('V+', 8), ('RES', 4)], [('GND', 1), ('VCTRL', 5)], width=6, gap=3))
# AD637 (14-lead SBDIP/CERDIP): Analog Devices data sheet Rev. L; pins 2 and 12 not connected inside (NIC).
add(ic_box('IC', C('Effektivwertwandler AD637', 'RMS-to-DC converter AD637', 'Convertisseur efficace AD637'), 'AD637',
           [('VIN', 13), ('BUFF IN', 1), ('CS', 5), ('DEN IN', 6), ('OFFSET', 4), ('CAV', 8)],
           [('RMS OUT', 9), ('BUFF OUT', 14), ('dB OUT', 7), None, ('NIC', 2), ('NIC', 12)], [('+VS', 11)], [('−VS', 10), ('COMMON', 3)], gap=3))

# The NE555 with its inner function (TI SLFS022K, functional block diagram): divider of three equal resistors, threshold
# and trigger comparators, the RS flip-flop with its reset, the output stage and the discharge transistor.
s = Symbol('U', C('Timer NE555 (Innenschaltung)', 'Timer NE555 (internal circuit)', 'Temporisateur NE555 (schéma interne)'), 'NE555')
s.rect(P, 0, 19 * P, 16 * P, w=0.35)
dot = lambda x, y: s.circle(x, y, 0.8, fill='#000000')
xd = 4 * P
s.line((xd, -P), (xd, 0.8 * P)).rect(xd - 0.8, 0.8 * P, xd + 0.8, 2.4 * P).line((xd, 2.4 * P), (xd, 6 * P))
s.rect(xd - 0.8, 6 * P, xd + 0.8, 8 * P).line((xd, 8 * P), (xd, 12 * P)).rect(xd - 0.8, 12 * P, xd + 0.8, 14 * P)
s.line((xd, 14 * P), (xd, 17 * P))
for top in (2 * P, 8 * P):
    s.poly((6 * P, top), (6 * P, top + 4 * P), (9 * P, top + 2 * P), fill=None, w=0.35)
s.text('+', 6 * P + 0.4, 3 * P - 1.2, 2.0).text(MINUS, 6 * P + 0.4, 5 * P - 1.2, 2.0)
s.text('+', 6 * P + 0.4, 9 * P - 1.2, 2.0).text(MINUS, 6 * P + 0.4, 11 * P - 1.2, 2.0)
s.line((0, 3 * P), (6 * P, 3 * P)).line((0, 5 * P), (6 * P, 5 * P)).line((xd, 9 * P), (6 * P, 9 * P)).line((0, 11 * P), (6 * P, 11 * P))
dot(xd, 5 * P)
dot(xd, 9 * P)
s.line((9 * P, 4 * P), (10 * P, 4 * P), (10 * P, 6 * P), (11 * P, 6 * P)).line((9 * P, 10 * P), (11 * P, 10 * P))
s.rect(11 * P, 5 * P, 15 * P, 11 * P, w=0.35).text('R', 11 * P + 0.6, 6 * P - 1.0, 1.8).text('S', 11 * P + 0.6, 10 * P - 1.0, 1.8)
s.text('R', 14 * P, 5 * P + 0.4, 1.8, 'centre').text('Q̅', 15 * P - 0.6, 8 * P - 1.0, 1.8, 'right')
s.line((14 * P, -P), (14 * P, 5 * P - 1.2)).circle(14 * P, 5 * P - 0.6, 1.2)
s.line((15 * P, 8 * P), (16.4 * P, 8 * P)).poly((16.4 * P, 7 * P), (16.4 * P, 9 * P), (17.6 * P, 8 * P), fill=None)
s.circle(17.6 * P + 0.5, 8 * P, 1.0).line((17.6 * P + 1.0, 8 * P), (20 * P, 8 * P))
dot(15.7 * P, 8 * P)
s.line((15.7 * P, 8 * P), (15.7 * P, 12.5 * P), (16.5 * P, 12.5 * P)).line((16.5 * P, 11.9 * P), (16.5 * P, 13.1 * P), w=THICK)
s.line((16.5 * P, 12.2 * P), (17.3 * P, 11.6 * P), (17.3 * P, 11 * P), (20 * P, 11 * P))
s.line((16.5 * P, 12.8 * P), (17.3 * P, 13.4 * P), end='arrow', size=1.0).line((17.3 * P, 13.4 * P), (17.3 * P, 15 * P), (xd, 15 * P))
dot(xd, 15 * P)
for text, x, y, align in (('THRS', P + 0.4, 3 * P - 1.7, 'left'), ('VCTRL', P + 0.4, 5 * P - 1.7, 'left'), ('TRIG', P + 0.4, 11 * P - 1.7, 'left'),
                          ('OUT', 19 * P - 0.4, 8 * P - 2.3, 'right'), ('DISCH', 19 * P - 0.4, 11 * P - 1.7, 'right'),
                          ('V+', xd + 0.4, 0.3, 'left'), ('RES', 14 * P + 0.4, 0.3, 'left'), ('GND', xd + 0.4, 16 * P - 1.7, 'left')):
    s.text(text, x, y, 1.4, align)
s.pin('8', (xd, -P), label=(xd + 0.4, -2.3), shown=True).pin('4', (14 * P, -P), label=(14 * P + 0.4, -2.3), shown=True)
s.pin('1', (xd, 17 * P), label=(xd + 0.4, 16 * P + 0.3), shown=True)
s.pin('6', (0, 3 * P), label=(0.2, 3 * P - 2.2), shown=True).pin('5', (0, 5 * P), label=(0.2, 5 * P - 2.2), shown=True)
s.pin('2', (0, 11 * P), label=(0.2, 11 * P - 2.2), shown=True)
s.pin('3', (20 * P, 8 * P), label=(20 * P - 0.2, 8 * P - 2.2), align='right', shown=True)
s.pin('7', (20 * P, 11 * P), label=(20 * P - 0.2, 11 * P - 2.2), align='right', shown=True)
add(small_pins(s).labels((P, -P - 3.0), (P, 17 * P + 0.6)))

# ICM7216B (universal counter, common-cathode LED drive, 28-pin PDIP): Intersil/Renesas FN3166.
add(ic_box('IC', C('Frequenzzähler 8 Stellen ICM7216', '8-digit frequency counter ICM7216', 'Fréquencemètre 8 chiffres ICM7216'), 'ICM7216B',
           [('INPUT A', 28), ('INPUT B', 2), ('FUNCTION', 3), ('RANGE', 14), ('CONTROL', 1), ('HOLD', 27), ('RESET', 13), None,
            ('OSC IN', 25), ('OSC OUT', 26), ('EXT OSC', 24)],
           [(f'D{k}', n) for k, n in ((1, 4), (2, 6), (3, 5), (4, 7), (5, 9), (6, 10), (7, 11), (8, 12))]
           + [(f'SEG {c}', n) for c, n in (('a', 20), ('b', 17), ('c', 16), ('d', 19), ('e', 21), ('f', 15), ('g', 22))] + [('DP', 23)],
           [('VDD', 18)], [('VSS', 8)]))
# 74HC4046A: TI SCHS204J.
add(ic_box('IC', C('PLL 74HC4046', 'PLL 74HC4046', 'PLL 74HC4046'), '74HC4046',
           [('SIG IN', 14), ('COMP IN', 3), ('VCO IN', 9), ('INH', 5), ('C1A', 6), ('C1B', 7), ('R1', 11), ('R2', 12)],
           [('PC1 OUT', 2), ('PC2 OUT', 13), ('PC3 OUT', 15), ('PCP OUT', 1), ('VCO OUT', 4), ('DEM OUT', 10)], [('VCC', 16)], [('GND', 8)]))
# AD636 in the 10-pin TO-100 can (AD636JH): Analog Devices data sheet Rev. E, the TO-100 pin numbers.
add(ic_box('IC', C('Effektivwertwandler AD636', 'RMS-to-DC converter AD636', 'Convertisseur efficace AD636'), 'AD636JH',
           [('VIN', 4), ('BUF IN', 9), ('CAV', 6), ('RL', 1)], [('IOUT', 10), ('BUF OUT', 8), ('dB', 7)], [('+VS', 3)], [('−VS', 5), ('COM', 2)]))
# A 3½-digit LED panel meter built around the ICL7107 (Renesas FN3082 Rev 10.00): its analogue terminals and supply,
# named as the converter's pins; the display drawn as four seven-segment digits.
s = ic_box('U', C('3½-stelliges Digitalvoltmeter-Modul (LED)', '3½-digit digital voltmeter module (LED)', 'Module voltmètre numérique 3½ chiffres (DEL)'), 'DVM',
           [('IN HI', 'IN HI'), ('IN LO', 'IN LO'), ('REF HI', 'REF HI'), ('REF LO', 'REF LO'), ('COMMON', 'COMMON')], [], [('V+', 'V+')],
           [('GND', 'GND'), ('V−', 'V-')], width=13, gap=3)
for o in s.parts:
    if o['type'] == 'contact':
        o['visible'] = False
x, y, w, hd = 6 * P, 2.6 * P, 1.6, 2.4
s.rect(x - 1.0, y - 1.0, x + 4 * 3.0 + 0.4, y + 2 * hd + 1.0, w=0.18)
s.line((x + 1.0, y), (x + 1.0, y + 2 * hd), w=THICK)
for k in range(1, 4):
    cx = x + k * 3.0
    for (ax, ay), (bx, by) in (((0, 0), (1, 0)), ((1, 0), (1, 1)), ((1, 1), (1, 2)), ((0, 2), (1, 2)), ((0, 1), (0, 2)), ((0, 0), (0, 1)), ((0, 1), (1, 1))):
        s.line((cx + ax * w, y + ay * hd), (cx + bx * w, y + by * hd), w=THICK)
add(s.circle(x + 4 * 3.0 - 0.6, y + 2 * hd, 0.5, fill='#000000'))
# ICL7106 (40-pin PDIP): Renesas FN3082.
add(ic_box('U', C('3½-stelliges Digitalvoltmeter (LCD) ICL7106', '3½-digit digital voltmeter (LCD) ICL7106', 'Voltmètre numérique 3½ chiffres (LCD) ICL7106'), 'ICL7106',
           [('IN HI', 31), ('IN LO', 30), ('REF HI', 36), ('REF LO', 35), ('CREF+', 34), ('CREF−', 33), ('COMMON', 32), ('BUFF', 28), ('A-Z', 29),
            ('INT', 27), None, ('OSC 1', 40), ('OSC 2', 39), ('OSC 3', 38), ('TEST', 37)],
           [(f'{c}1', n) for c, n in zip('ABCDEFG', (5, 4, 3, 2, 8, 6, 7))] + [(f'{c}2', n) for c, n in zip('ABCDEFG', (12, 11, 10, 9, 14, 13, 25))]
           + [(f'{c}3', n) for c, n in zip('ABCDEFG', (23, 16, 24, 15, 18, 17, 22))] + [('AB4', 19), ('POL', 20), ('BP', 21)],
           [('V+', 1)], [('V−', 26)]))
# TDA1308 (headphone driver, two amplifiers): NXP product data sheet Rev. 5, 14 March 2011.
s = Symbol('U', C('Kopfhörerverstärker', 'Headphone amplifier', 'Amplificateur pour casque'), 'TDA1308')
s.rect(P, 0, 8 * P, 12 * P, w=0.35)
for letter, top, minus, plus, out in (('A', P, '2', '3', '1'), ('B', 6 * P, '6', '5', '7')):
    s.poly((3 * P, top), (3 * P, top + 4 * P), (6 * P, top + 2 * P), fill=None, w=0.35)
    s.text(MINUS, 3 * P + 0.4, top + P - 1.2, 2.0).text('+', 3 * P + 0.4, top + 3 * P - 1.2, 2.0).text(letter, 4 * P, top + 2 * P - 0.9, 1.8)
    for name, y in ((minus, top + P), (plus, top + 3 * P)):
        s.line((0, y), (3 * P, y)).pin(name, (0, y), label=(0.2, y - 2.2), shown=True)
    s.line((6 * P, top + 2 * P), (9 * P, top + 2 * P)).pin(out, (9 * P, top + 2 * P), label=(9 * P - 0.2, top + 2 * P - 2.2), align='right', shown=True)
s.line((5 * P, 0), (5 * P, -P)).text('VDD', 5 * P, 0.4, 1.8, 'centre').pin('8', (5 * P, -P), label=(5 * P + 0.4, -2.3), shown=True)
s.line((5 * P, 12 * P), (5 * P, 13 * P)).text('VSS', 5 * P, 12 * P - 2.4, 1.8, 'centre').pin('4', (5 * P, 13 * P), label=(5 * P + 0.4, 12 * P + 0.3), shown=True)
add(small_pins(s).labels((P, -P - 3.4), (P, 13 * P + 0.6)))
# SAD-1024 (two sections A and B of 512 stages): EG&G Reticon data sheet "SAD-1024 Dual Analog Delay Line", Figure 1.
add(ic_box('IC', C('Analoge Verzögerungsleitung SAD1024', 'Analog delay line SAD1024', 'Ligne à retard analogique SAD1024'), 'SAD1024',
           [('IN A', 2), ('φ1A', 8), ('φ2A', 3), None, ('IN B', 15), ('φ1B', 10), ('φ2B', 14)],
           [('OUT A', 5), ("OUT A'", 6), None, None, ('OUT B', 12), ("OUT B'", 11), None, None, ('NC', 4), ('NC', 13), ('NC', 16)],
           [('VDD', 7), ('VBB', 9)], [('GND', 1)], dividers=(3.5, 7.5)))
# TDA1022 (bucket brigade of 512 stages): Philips data sheet, June 1976. VDD (9) is the negative supply (−15 V), drawn
# below; ground and substrate (16) above.
add(ic_box('IC', C('CCD-Verzögerungsleitung TDA1022', 'Bucket-brigade delay line TDA1022', 'Ligne à retard à transfert de charges TDA1022'), 'TDA1022',
           [('IN', 5), ('φ1', 1), ('φ2', 4), ('GATE', 13), None, ('NC', 2), ('NC', 3), ('NC', 6), ('NC', 7)],
           [('OUT 512', 12), ('OUT 513', 8), None, None, None, ('NC', 10), ('NC', 11), ('NC', 14), ('NC', 15)],
           [('GND', 16)], [('VDD', 9)], dividers=(4.5,)))
# NE570: onsemi NE570/D Rev. 4.
ne570 = lambda r, g, i, r3, c, t: [('RECT IN', r), ('ΔG IN', g), ('INV IN', i), ('R3', r3), ('RECT CAP', c), ('THD TRIM', t)]
add(ic_box('IC', C('Kompander NE570', 'Compander NE570', 'Compresseur-extenseur NE570'), 'NE570',
           ne570(2, 3, 5, 6, 1, 8) + [None] + ne570(15, 14, 12, 11, 16, 9), [None, None, ('OUT', 7), None, None, None, None, None, None, ('OUT', 10)],
           [('VCC', 13)], [('GND', 4)], dividers=(6.5,)))

add = page(USR, 'Texas Instruments (Digital)', 'Texas Instruments (Digital)', 'Texas Instruments (numérique)')
# Whole 74xx ICs after the logic symbols of IEC 60617-12 in the TI data sheets: one element per gate, the qualifying
# symbol in each, pin numbers in brackets; the supply (and unused pins) in an element of its own below. The pins come
# from ttl74.py. "SN74xx06/07" is drawn as the 7407 (TI SDLS032H: open-collector buffer, output not negated; the 7406
# of TI SDLS031A negates), which its English and French captions and its value say.
SHOWN = {'7406/07': ('SN74xx06/07 (drawn as SN74xx07, non-inverting)', 'SN74xx06/07 (représenté en SN74xx07, non inverseur)', 'SN74xx07')}


def oc_mark(s, x, y, buffer=False):
    d = 0.7
    s.line((x, y - d), (x + d, y), (x, y + d), (x - d, y), (x, y - d), w=0.18).line((x - d, y + d + 0.3), (x + d, y + d + 0.3), w=0.18)
    if buffer:
        s.line((x - 2.6, y - 0.8), (x - 1.4, y), (x - 2.6, y + 0.8), (x - 2.6, y - 0.8), w=0.18)


def ti_ic(number, sign, negated, mark=None):
    chip = next(c for c in ttl74.CHIPS if c.number == number[:4])
    pin_of = {name: k + 1 for k, name in enumerate(chip.pins)}
    name = f'SN74xx{number[2:]}'
    en, fr, value = SHOWN.get(number, (name, name, name))
    s = Symbol('U', C(name, en, fr), value)
    units = chip.children.units()
    n = len(units[0][0])
    x0 = 2 * P
    x1 = x0 + (3 * P if mark and n == 1 else 2 * P)
    y0 = 0
    for ins, out in units:
        if n == 1:
            ys, h = [P], 2 * P
        elif n % 2:
            ys, h = [(k + 1) * P for k in range(n)], (n + 1) * P
        else:
            ys, h = [(k + 1) * P for k in range(n // 2)] + [(k + 2 + n // 2) * P for k in range(n // 2)], (n + 2) * P
        s.rect(x0, y0, x1, y0 + h, w=0.35)
        if mark and n == 1:
            s.text(sign, x0 + 0.6, y0 + 0.4, 2.5)
        else:
            s.text(sign, (x0 + x1) / 2, y0 + 0.4, 2.5, 'centre')
        for pin, y in zip(ins, ys):
            k = pin_of[pin]
            s.line((0, y0 + y), (x0, y0 + y)).pin(str(k), (0, y0 + y), label=(0.2, y0 + y - 2.2), shown=True, text=f'({k})')
        yo = y0 + h / 2
        if negated:
            s.circle(x1 + 0.6, yo, 1.2).line((x1 + 1.2, yo), (x1 + 2 * P, yo))
        else:
            s.line((x1, yo), (x1 + 2 * P, yo))
        k = pin_of[out]
        s.pin(str(k), (x1 + 2 * P, yo), label=(x1 + 2 * P - 0.2, yo - 2.2), align='right', shown=True, text=f'({k})')
        if mark in ('oc', 'ocbuf'):
            oc_mark(s, x1 - 1.1, yo, mark == 'ocbuf')
        if mark == 'schmitt':
            xs, ys0 = (x0 + x1) / 2 - 1.0, y0 + 4.4
            s.line((xs, ys0), (xs + 1.0, ys0), (xs + 1.0, ys0 - 1.2), (xs + 2.0, ys0 - 1.2), w=0.18)
            s.line((xs + 0.4, ys0), (xs + 0.4, ys0 - 1.2), (xs + 1.4, ys0 - 1.2), w=0.18)
        y0 += h
    rest = [('VCC', 'VCC'), ('GND', 'GND')] + [('NC', k + 1) for k, p in enumerate(chip.pins) if p == 'NC']
    y0 += P
    s.rect(x0, y0, x0 + 2 * P, y0 + (len(rest) + 1) * P, w=0.35)
    for j, (text, key) in enumerate(rest):
        y = y0 + (j + 1) * P
        k = pin_of[key] if key in pin_of else key
        s.line((0, y), (x0, y)).text(text, x0 + P, y - 0.8, 1.6, 'centre')
        s.pin(str(k), (0, y), label=(0.2, y - 2.2), shown=True, text=f'({k})')
    return small_pins(s).labels((x0, -3.4), (x0, y0 + (len(rest) + 1) * P + 0.6))


for number, sign, negated, mark in (('7400', '&', True, None), ('7401', '&', True, 'oc'), ('7402', '≥1', True, None), ('7403', '&', True, 'oc'),
                                    ('7404', '1', True, None), ('7405', '1', True, 'oc'), ('7406/07', '1', False, 'ocbuf'), ('7408', '&', False, None),
                                    ('7409', '&', False, 'oc'), ('7410', '&', True, None), ('7411', '&', False, None), ('7412', '&', True, 'oc'),
                                    ('7413', '&', True, 'schmitt')):
    add(ti_ic(number, sign, negated, mark))

add = page(USR, 'MISC', 'MISC', 'DIVERS')


def crystal(caption, value):
    s = two_pin('XT', caption, value, ask=True)
    s.line((0, 0), (1.4 * P, 0)).line((1.4 * P, -2.2), (1.4 * P, 2.2), w=THICK).rect(1.65 * P, -1.8, 2.35 * P, 1.8)
    return s.line((2.6 * P, -2.2), (2.6 * P, 2.2), w=THICK).line((2.6 * P, 0), (4 * P, 0))


add(crystal(C('Quarz (senkrecht)', 'Crystal (vertical)', 'Quartz (vertical)'), '4,3 MHz').vertical().labels((3.4, P - 0.4), (3.4, 2 * P + 0.4)))
add(crystal(C('Quarz', 'Crystal', 'Quartz'), '4,3 MHz'))
s = Symbol('F', C('Sicherung (senkrecht)', 'Fuse (vertical)', 'Fusible (vertical)'), '100mA', ask=True)
s.rect(-1.0, P, 1.0, 3 * P).line((0, 0), (0, 4 * P))
add(s.pin('1', (0, 0), label=(0.6, 0.2)).pin('2', (0, 4 * P), label=(0.6, 4 * P - 2.4)).labels((2.0, P), (2.0, 2 * P + 0.4)))
s = two_pin('F', C('Sicherung', 'Fuse', 'Fusible'), '100mA', ask=True)
add(s.rect(P, -1.0, 3 * P, 1.0).line((0, 0), (4 * P, 0)))
s = resistor_body(two_pin('X', C('Heizspirale', 'Heating coil', 'Spirale chauffante')))      # the heating element, 04-01-12
for k in (1, 2, 3):
    s.line((P + k * P / 2, -1.0), (P + k * P / 2, 1.0))
add(s)
# LM386: TI SNAS545D.
add(opamp('IC', C('Audioverstärker LM386', 'Audio amplifier LM386', 'Amplificateur audio LM386'), 'LM386', '2', '3', '5',
          top=[('6', 3 * P)], bottom=[('4', 3 * P)]))
# MAX186: Maxim data sheet 19-0123, Rev 5 (1/12).
add(ic_box('IC', C('A/D-Umsetzer MAX186', 'A/D converter MAX186', 'Convertisseur A/N MAX186'), 'MAX186',
           [(f'CH{k}', k + 1) for k in range(8)] + [None, ('VREF', 11), ('REFADJ', 12), ('~SHDN', 10)],
           [('~CS', 18), ('SCLK', 19), ('DIN', 17), ('DOUT', 15), ('SSTRB', 16)], [('VDD', 20)], [('VSS', 9), ('AGND', 13), ('DGND', 14)]))
# TLC549: TI SLAS067C, September 1996.
add(ic_box('IC', C('A/D-Umsetzer TLC549', 'A/D converter TLC549', 'Convertisseur A/N TLC549'), 'TLC549',
           [('ANALOG IN', 2), ('REF+', 1), ('REF−', 3)], [('~CS', 5), ('I/O CLOCK', 7), ('DATA OUT', 6)], [('VCC', 8)], [('GND', 4)]))
s = Symbol('X', C('BNC-Buchse 50 Ω (Bauteil)', 'BNC socket 50 Ω (component)', 'Embase BNC 50 Ω (composant)'), '50 Ω')
s.circle(2 * P, 0, 2 * P, w=0.35).circle(2 * P, 0, 3.0).circle(2 * P, 0, 1.0, fill='#000000')
for a in (45, 225):     # the bayonet lugs
    s.poly(*[rot(p, (2 * P, 0), a) for p in ((3 * P - 0.1, -0.45), (3 * P + 0.6, -0.45), (3 * P + 0.6, 0.45), (3 * P - 0.1, 0.45))])
s.line((0, 0), (2 * P - 0.5, 0)).line((2 * P, P), (2 * P, 2 * P))
add(s.pin('1', (0, 0)).pin('2', (2 * P, 2 * P), label=(2 * P + 0.4, 2 * P - 2.2)).labels((3 * P + 0.6, -P - 1.6), (3 * P + 0.6, -P + 1.2)))
