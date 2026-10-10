# Transistors, FETs and thyristors after DIN EN 60617-5 (05-04 thyristors, 05-05 transistors) for the pages
# "Transistoren", "FET" and "Thyristoren": bipolar transistors with and without envelope, a triac in an envelope, the
# dual-gate MOSFET, thyristors with the gate at the anode side, the tetrode thyristor and the four-layer diode.


def bjt(caption, pnp=False, envelope=True, double=False, photo=False):
    """The bipolar transistor of generate.py (B left, C up, E down); without envelope the circle is left out."""
    s = transistor(pnp, caption)
    s.prefix = 'K'
    if not envelope:
        s.parts = [o for o in s.parts if o['type'] != 'ellipse']
        s.labels((6.6, -2.6), (6.6, 0.4))
    if double:      # the collector drawn twice, parallel
        s.line((3.0, -1.6), (2 * P, -3.7))
    if photo:       # light falling on the base from up left; the base lead stays
        arrows_in(s, 1.0, -0.9, angle=315)
    return s


add = extend(BT, 'Transistoren')
add(bjt(C('NPN-Transistor (Kollektor doppelt)', 'NPN transistor (double collector)', 'Transistor NPN (collecteur double)'), double=True))
add(bjt(C('PNP-Transistor (Kollektor doppelt)', 'PNP transistor (double collector)', 'Transistor PNP (collecteur double)'), pnp=True, double=True))

s = Symbol('K', C('Triac (mit Gehäuse)', 'Triac (with envelope)', 'Triac (avec enveloppe)'))
x = 2 * P       # the main terminals on one vertical, 1 above, 2 below; the gate at the bar of 2, as on the existing triac
s.circle(x, 0, 7.4)
s.line((x, -2 * P), (x, -1.27)).line((x, 1.27), (x, 2 * P))
s.line((x - 2.6, -1.27), (x + 2.6, -1.27), w=0.35).line((x - 2.6, 1.27), (x + 2.6, 1.27), w=0.35)
s.poly((x - 2.4, 1.27), (x - 0.1, 1.27), (x - 1.25, -1.27), fill=None).poly((x + 0.1, -1.27), (x + 2.4, -1.27), (x + 1.25, 1.27), fill=None)
s.line((x - 1.8, 1.27), (x - 3.1, P), (0, P))
s.pin('1', (x, -2 * P)).pin('2', (x, 2 * P)).pin('G', (0, P))
add(s.labels((9.2, -2.6), (9.2, 0.4)))

for pnp, kind in ((False, 'NPN'), (True, 'PNP')):
    add(bjt(C(f'{kind}-Transistor ohne Gehäuse', f'{kind} transistor without envelope', f'Transistor {kind} sans enveloppe'), pnp, envelope=False))
for pnp, kind in ((False, 'NPN'), (True, 'PNP')):
    add(bjt(C(f'{kind}-Transistor ohne Gehäuse (Kollektor doppelt)', f'{kind} transistor without envelope (double collector)',
              f'Transistor {kind} sans enveloppe (collecteur double)'), pnp, envelope=False, double=True))
for pnp, kind in ((False, 'NPN'), (True, 'PNP')):
    add(bjt(C(f'{kind}-Fototransistor ohne Gehäuse', f'{kind} phototransistor without envelope', f'Phototransistor {kind} sans enveloppe'), pnp, envelope=False, photo=True))


def dual_gate(p, caption):
    """Depletion-type MOSFET with two gates: G2 (drain side) above, G1 (source side) below; substrate on the source."""
    s = Symbol('K', caption)
    s.circle(4.6, 0, 7.4)
    s.line((0, -P), (2.6, -P), (2.6, -0.4)).line((0, P), (2.6, P), (2.6, 0.4))
    s.line((3.4, -2.2), (3.4, 2.2), w=THICK)
    s.line((3.4, -1.7), (2 * P, -1.7), (2 * P, -2 * P)).line((3.4, 1.7), (2 * P, 1.7), (2 * P, 2 * P))
    if p:
        s.line((3.4, 0), (2 * P, 0), end='arrow', size=1.3)
    else:
        s.line((2 * P, 0), (3.4, 0), end='arrow', size=1.3)
    s.line((2 * P, 0), (2 * P, 1.7))
    s.pin('G2', (0, -P)).pin('G1', (0, P)).pin('D', (2 * P, -2 * P)).pin('S', (2 * P, 2 * P))
    return s.labels((9.2, -2.6), (9.2, 0.4))


add = extend(BT, 'FET')
add(dual_gate(False, C('Dual-Gate-MOSFET (N-Kanal)', 'Dual-gate MOSFET (N-channel)', 'MOSFET double grille (canal N)')))
add(dual_gate(True, C('Dual-Gate-MOSFET (P-Kanal)', 'Dual-gate MOSFET (P-channel)', 'MOSFET double grille (canal P)')))

# Thyristors: anode 1 left, cathode 2 right. A P-gate leaves the cathode bar (as the existing "Thyristor"), an N-gate
# the triangle at the anode side; the turn-off thyristor has a short stroke across its gate. Outline triangles like
# the existing thyristors.
FILL = None


def thyristor(caption):
    s = two_pin('Q', caption)
    s.line((0, 0), (1.5 * P, 0)).line((2.5 * P, 0), (4 * P, 0))
    return diode_shape(s, 1.5 * P, 2.5 * P, fill=FILL)


def p_gate(s, name='G'):
    return s.line((2.5 * P, 0.6), (3 * P, 2.0), (3 * P, 2 * P)).pin(name, (3 * P, 2 * P), label=(3 * P + 0.5, 2 * P - 2.2))


def n_gate(s, name='G'):
    start = (1.5 * P + 0.5, 1.5 - 0.5 / P * 1.5)      # on the lower side of the triangle, near the anode
    bend = (P, start[1] + start[0] - P)                  # 45 degrees down to the left
    return s.line(start, bend, (P, 2 * P)).pin(name, (P, 2 * P), label=(P - 0.5, 2 * P - 2.2), align='right')


add = extend(BT, 'Thyristoren')
add(p_gate(thyristor(C('Thyristor (p-Gate)', 'Thyristor (P-gate)', 'Thyristor (gâchette P)'))))
add(n_gate(thyristor(C('Thyristor (n-Gate)', 'Thyristor (N-gate)', 'Thyristor (gâchette N)'))))
s = n_gate(thyristor(C('GTO-Thyristor (n-Gate)', 'GTO thyristor (N-gate)', 'Thyristor GTO (gâchette N)')))
add(s.line((3.92, 2.29), (2.92, 1.89)))
add(n_gate(p_gate(thyristor(C('Thyristortetrode', 'Tetrode thyristor', 'Thyristor tétrode'))), 'G2'))
add(thyristor(C('Vierschichtdiode', 'Four-layer diode', 'Diode à quatre couches')).line((2.5 * P, -0.6), (3 * P, -2.0)))
