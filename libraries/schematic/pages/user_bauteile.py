# The USER pages "COMP OUTL", "CON 1", "DIODE 10", "OPTO", "PWR", "PWR SUPPLY" and "RES VAR". Circuit symbols after
# DIN EN 60617-2, -3, -4, -5, -6 and -8 in the style of the pages "Bauteile" ("10" forms 4 P long, "lang" 6 P); the voltage
# reference diode after TI SNVS742E (LM385-1.2), the shunt reference after TI SLVS543S (TL431). "COMP OUTL" holds
# outlines for layout sketches seen from above, 1:1, pads on the 2.54 mm pitch with the contacts: DO-35 after Vishay
# 81857, TO-92 (BC546B: 1 C, 2 B, 3 E) after onsemi BC546/D, the multi-turn trimmer after Bourns 3006P, the 10 mm
# trimmers after Piher "PT-10 / PTC-10" (2023), the 3 mm LED after Kingbright DSAF2332 and Vishay 83001, the screw
# terminal after Würth 691102710002 (WR-TBL 102, 5.00 mm), the SMB jack after Cinch/Johnson 131-3701-261, the jumper
# after Würth 60900213421, the M3 screw head after ISO 7045, LF412 after TI SNOSBH7F, LM394 after National SNLS385A,
# LM3146 after National TL/H/7959. The optocouplers in the pin order of the 4-pin packages (Vishay 83503). Contact "-"
# is shown as "−".
USR = 'USER'
PAD, DRILL = 1.5, 0.6      # solder pad and its hole on the outlines
BODY, FINE = 0.35, 0.18    # outline of a part and the finer lines inside
BLACK = '#000000'
MINUS = '−'


def pad(s, x, y, square=False):
    if square:
        s.rect(x - PAD / 2, y - PAD / 2, x + PAD / 2, y + PAD / 2, w=FINE)
    else:
        s.circle(x, y, PAD, w=FINE)
    return s.circle(x, y, DRILL, w=FINE)


def pads(s, places, square_first=False):
    """Pads at `places`, the contacts named 1, 2, ... in their order."""
    for k, (x, y) in enumerate(places):
        pad(s, x, y, square_first and k == 0).pin(str(k + 1), (x, y))
    return s


def contacts_font(s, h):
    for o in s.parts:
        if o['type'] == 'contact':
            o['font'] = {'height': h}
    return s


# --- COMP OUTL
add = page(USR, 'COMP OUTL', 'COMP OUTL', 'Contours de composants')


def axial_outline(prefix, caption, pitches, body, thick, value='', band=False):
    """An axial part lying between pads 1 and 2, `pitches` apart: the leads, the body with rounded ends and, for a
    diode, the cathode band at contact 2."""
    s = Symbol(prefix, caption, value)
    end = pitches * P
    x0, x1 = (end - body) / 2, (end + body) / 2
    if x0 > PAD / 2:
        s.line((PAD / 2, 0), (x0, 0)).line((x1, 0), (end - PAD / 2, 0))
    s.rect(x0, -thick / 2, x1, thick / 2, w=BODY, corner=40)
    if band:
        s.rect(x1 - 0.9, -thick / 2, x1 - 0.4, thick / 2, w=FINE, fill=BLACK)
    pads(s, [(0, 0), (end, 0)])
    return s.labels((end / 2, -thick / 2 - 3.4), (end / 2, thick / 2 + 0.6), 'centre')


def radial_outline(prefix, caption, pitches, body, thick, value, corner=20):
    """A part standing on two pads `pitches` apart, its body seen from above as a rounded rectangle."""
    s = Symbol(prefix, caption, value)
    c = pitches * P / 2
    s.rect(c - body / 2, -thick / 2, c + body / 2, thick / 2, w=BODY, corner=corner)
    pads(s, [(0, 0), (pitches * P, 0)])
    return s.labels((c, -thick / 2 - 3.4), (c, thick / 2 + 0.6), 'centre')


def dil_top(prefix, caption, n, value='', outside=False):
    """A DIL package seen from above: rows 7.62 mm apart, pin 1 (square pad) at the top left, counted down the left
    side and up the right side, the notch at the top. The pin numbers inside the body, or outside when the body shows
    the circuit inside."""
    half = n // 2
    s = Symbol(prefix, caption, value, numbered=bool(prefix))
    x0, x1, y0, y1 = 1.1, 3 * P - 1.1, -1.3, (half - 1) * P + 1.3
    s.rect(x0, y0, x1, y1, w=BODY).arc(1.5 * P, y0, 1.8, 180, 360, w=BODY)
    for k in range(half):
        y = k * P
        pad(s, 0, y, k == 0)
        pad(s, 3 * P, y)
        if outside:
            s.pin(str(k + 1), (0, y), label=(-1.0, y - 0.7), align='right', shown=True)
            s.pin(str(n - k), (3 * P, y), label=(3 * P + 1.0, y - 0.7), shown=True)
        else:
            s.pin(str(k + 1), (0, y), label=(x0 + 0.4, y - 0.7), shown=True)
            s.pin(str(n - k), (3 * P, y), label=(x1 - 0.4, y - 0.7), align='right', shown=True)
    contacts_font(s, 1.4)
    right = 3 * P + (2.8 if outside else 1.4)
    return s.labels((right, y0), (right, y0 + 3.0))


add(axial_outline('R', C('Metallschichtwiderstand 0207, Raster 10 mm', 'Metal film resistor 0207, 10 mm pitch',
                         'Résistance à couche métallique 0207, pas de 10 mm'), 4, 6.3, 2.5))
add(axial_outline('R', C('Metallschichtwiderstand 0207, Raster 7,5 mm', 'Metal film resistor 0207, 7.5 mm pitch',
                         'Résistance à couche métallique 0207, pas de 7,5 mm'), 3, 6.3, 2.5))
s = Symbol('R', C('Metallschichtwiderstand 0207, stehend', 'Metal film resistor 0207, upright', 'Résistance à couche métallique 0207, verticale'))
s.circle(0, 0, 2.5, w=BODY).line((1.25, 0), (P - PAD / 2, 0), w=0.35)      # the body seen from its end, the lead bent over
add(pads(s, [(0, 0), (P, 0)]).labels((P + 1.4, -3.4), (P + 1.4, -0.6)))

# Bourns 3006P: 19.05 x 4.83 mm, pins 1 and 3 12.70 apart, 3 at 3.30 from the end with the screw, 2 (wiper) 5.08 from 3
# in the other row, 2.54 behind.
s = Symbol('RV', C('Trimmer RT19 (Spindel)', 'Trimmer RT19 (multi-turn)', 'Résistance ajustable RT19 (multitour)'))
right = 5 * P + 3.3
s.rect(right - 19.05, -P / 2 - 2.415, right, -P / 2 + 2.415, w=BODY)
s.rect(right, -P / 2 - 1.18, right + 1.52, -P / 2 + 1.18).line((right + 0.7, -P / 2), (right + 1.52, -P / 2), w=0.35)
add(pads(s, [(0, 0), (3 * P, -P), (5 * P, 0)]).labels((right - 19.05, -P / 2 - 5.8), (right + 1.52, -P / 2 - 5.8), 'left', 'right'))


def slot(s, x, y, length, w=0.35):
    """The slot of a screw or rotor, at 45 degrees."""
    d = length / 2 * 0.7071
    return s.line((x - d, y + d), (x + d, y - d), w=w)


# 10 mm trimmers (Piher PT-10): 1 and 3 5 mm apart, the wiper on the middle line. Lying (V10): the round body Ø10.3
# with its foot over 1 and 3, the wiper 10 mm behind them, the rotor in the middle. Standing (H01): 10.3 x 4.8, the
# wiper 2.5 mm behind 1 and 3, towards the rotor.
R10 = 10.3 / 2
s = Symbol('RV', C('Trimmer RT10 (liegend)', 'Trimmer RT10 (horizontal)', 'Résistance ajustable RT10 (horizontale)'))
cy, foot = -2 * P, 4.0
a = math.degrees(math.asin(math.sqrt(R10 ** 2 - foot ** 2) / R10))
s.arc(P, cy, 2 * R10, 360 - a, 540 + a, w=BODY)
s.line((P + foot, cy + math.sqrt(R10 ** 2 - foot ** 2)), (P + foot, 0.3), (P - foot, 0.3), (P - foot, cy + math.sqrt(R10 ** 2 - foot ** 2)), w=BODY)
s.circle(P, cy, 4.0)
slot(s, P, cy, 4.0)
add(pads(s, [(0, 0), (P, -4 * P), (2 * P, 0)]).labels((P + R10 + 1.0, cy - R10), (P + R10 + 1.0, cy - R10 + 3.0)))
s = Symbol('RV', C('Trimmer RT10 (stehend)', 'Trimmer RT10 (vertical)', 'Résistance ajustable RT10 (verticale)'))
s.rect(P - R10, 0.4, P + R10, -3.8, w=BODY)
s.rect(P - 2.0, -3.8, P + 2.0, -4.4).line((P, -3.8), (P, -4.4), w=0.35)      # the rotor at the front
add(pads(s, [(0, 0), (P, -P), (2 * P, 0)]).labels((P + R10 + 1.0, -4.4), (P + R10 + 1.0, -1.4)))

s = Symbol('', C('Stiftleiste (ein Stift)', 'Pin header (single pin)', 'Barrette (une broche)'), numbered=False)
s.rect(-P / 2, -P / 2, P / 2, P / 2, w=BODY).rect(-0.32, -0.32, 0.32, 0.32, w=0.1, fill=BLACK).pin('1', (0, 0))
add(s.labels((P / 2 + 1.0, -3.0), (P / 2 + 1.0, -0.2)))
s = Symbol('', C('Buchse (Lötauge)', 'Socket (solder pad)', 'Douille (pastille)'), numbered=False)
add(s.circle(0, 0, 2.4, w=BODY).circle(0, 0, 1.0, w=FINE).pin('1', (0, 0)).labels((2.2, -3.0), (2.2, -0.2)))

add(radial_outline('C', C('Folienkondensator, Raster 5 mm', 'Film capacitor, 5 mm pitch', 'Condensateur à film, pas de 5 mm'), 2, 7.2, 2.5, '15n'))
add(radial_outline('C', C('Keramikkondensator, Raster 2,5 mm', 'Ceramic capacitor, 2.5 mm pitch', 'Condensateur céramique, pas de 2,5 mm'),
                   1, 4.0, 2.2, '33p', corner=50))
add(radial_outline('C', C('Keramikkondensator, Raster 2,5 mm (breit)', 'Ceramic capacitor, 2.5 mm pitch (wide)',
                          'Condensateur céramique, pas de 2,5 mm (large)'), 1, 5.6, 2.6, '100n', corner=50))

s = Symbol('C', C('Elektrolytkondensator (stehend)', 'Electrolytic capacitor (radial)', 'Condensateur électrolytique (radial)'), '10/25')
s.circle(P / 2, 0, 6.3, w=BODY).arc(P / 2, 0, 5.6, 320, 400, w=0.7)      # the stripe on the side of the − lead (2)
add(pads(s, [(0, 0), (P, 0)]).labels((P / 2 + 4.0, -3.2), (P / 2 + 4.0, -0.4)))

s = Symbol('Q', C('TO-92 (Draufsicht)', 'TO-92 (top view)', 'TO-92 (vue de dessus)'), 'BC546B')
cy, radius, flat = -0.3, 2.5, 1.2     # the flat side towards the viewer
a = math.degrees(math.asin(flat / radius))
s.arc(P, cy, 2 * radius, 360 - a, 540 + a, w=BODY)
half = math.sqrt(radius ** 2 - flat ** 2)
s.line((P - half, cy + flat), (P + half, cy + flat), w=BODY)
add(pads(s, [(0, 0), (P, 0), (2 * P, 0)]).labels((2 * P + 2.0, -3.6), (2 * P + 2.0, -0.8)))

s = Symbol('D', C('LED 3 mm (Draufsicht)', 'LED 3 mm (top view)', 'DEL 3 mm (vue de dessus)'))
s.circle(P / 2, 0, 3.2).circle(P / 2, 0, 2.9, w=BODY).text('K', P, 1.75, 1.4, 'centre')     # flange, lens; 2 the cathode
add(pads(s, [(0, 0), (P, 0)]).labels((P / 2 + 2.6, -3.4), (P / 2 + 2.6, -0.6)))

add(axial_outline('D', C('Diode DO-35, Raster 7,5 mm', 'Diode DO-35, 7.5 mm pitch', 'Diode DO-35, pas de 7,5 mm'), 3, 3.4, 1.7, '1N4148', band=True))
add(axial_outline('D', C('Diode DO-35, Raster 10 mm', 'Diode DO-35, 10 mm pitch', 'Diode DO-35, pas de 10 mm'), 4, 3.4, 1.7, '1N4148', band=True))
for n in (8, 14, 16):
    add(dil_top('U', C(f'DIL {n} (Draufsicht)', f'DIL {n} (top view)', f'DIL {n} (vue de dessus)'), n))

# LF412: 1 output A, 2 and 3 inputs − and + of A, 4 V−, 5 and 6 inputs + and − of B, 7 output B, 8 V+. Each amplifier
# a small triangle pointing up, its output led up, the inputs from below; the supply pins marked + and −.
s = dil_top('U', C('LF412 (Draufsicht mit Innenschaltung)', 'LF412 (top view with internal circuit)',
                   'LF412 (vue de dessus avec schéma interne)'), 8, 'LF412', outside=True)
x0, x1 = 1.1, 3 * P - 1.1
for mirror, dy, (out, minus, plus) in ((False, 0, (0, P, 2 * P)), (True, P, (P, 2 * P, 3 * P))):
    X = (lambda x: 3 * P - x) if mirror else (lambda x: x)
    edge = x1 if mirror else x0
    apex, base = 2.0 + dy, 4.0 + dy
    s.poly((X(2.7), apex), (X(1.95), base), (X(3.45), base), fill=None, w=FINE)
    s.line((X(2.7), apex), (X(2.7), out), (edge, out), w=FINE)
    s.line((edge, minus), (X(1.55), minus), (X(1.55), base + 0.5), (X(2.2), base + 0.5), (X(2.2), base), w=FINE)
    s.line((edge, plus), (X(3.2), plus), (X(3.2), base), w=FINE)
s.line((x1, 0), (x1 - 0.6, 0), w=FINE).line((x1 - 1.7, 0), (x1 - 0.9, 0), w=FINE).line((x1 - 1.3, -0.4), (x1 - 1.3, 0.4), w=FINE)
add(s.line((x0, 3 * P), (x0 + 0.6, 3 * P), w=FINE).line((x0 + 0.9, 3 * P), (x0 + 1.7, 3 * P), w=FINE))

# LM394 (DIP N08E): 1, 2, 3 collector, base, emitter of Q1, 8, 7, 6 those of Q2, 4 and 5 not connected.
s = dil_top('', C('Doppeltransistor LM394 (Draufsicht)', 'Dual transistor LM394 (top view)', 'Transistor double LM394 (vue de dessus)'),
            8, 'LM394', outside=True)
for X, edge in ((lambda x: x, x0), (lambda x: 3 * P - x, x1)):
    s.line((X(2.5), P - 1.0), (X(2.5), P + 1.0), w=0.35).line((edge, P), (X(2.5), P), w=FINE)
    s.line((X(2.5), P - 0.45), (X(3.3), P - 1.2), (X(3.3), 0), (edge, 0), w=FINE)
    s.line((X(2.5), P + 0.45), (X(3.3), P + 1.2), w=FINE, end='arrow', size=0.7).line((X(3.3), P + 1.2), (X(3.3), 2 * P), (edge, 2 * P), w=FINE)
add(s)

# One pole of a 5 mm screw terminal, 7.5 deep, the pin 4.2 from the back; the screw head (Ø3.6) over the pin, the
# opening for the wire at the front (below).
s = Symbol('CN', C('Schraubklemme PSK 5', 'Screw terminal PSK 5', 'Bornier à vis PSK 5'))
s.rect(-P, -4.2, P, 3.3, w=BODY).line((-P, 2.5), (P, 2.5), w=FINE).circle(0, 0, 3.6)
add(slot(s, 0, 0, 3.6).pin('1', (0, 0)).labels((P + 1.0, -4.2), (P + 1.0, -1.4)))

s = Symbol('J', C('SMB-Buchse (Draufsicht)', 'SMB jack (top view)', 'Embase SMB (vue de dessus)'))
s.rect(-3.0, -3.0, 3.0, 3.0, w=BODY).circle(0, 0, 3.2, w=BODY).circle(0, 0, 2.2, w=FINE)
pad(s, 0, 0).pin('1', (0, 0))
for x, y in ((-P, -P), (P, -P), (-P, P), (P, P)):
    pad(s, x, y)
add(s.pin('2', (P, P)).labels((4.0, -3.0), (4.0, -0.2)))

s = Symbol('JP', C('Jumper 2,54 mm', 'Jumper 2.54 mm', 'Cavalier 2,54 mm'))
s.rect(P / 2 - 2.49, -1.225, P / 2 + 2.49, 1.225, w=BODY)
for k in range(2):
    s.rect(k * P - 0.32, -0.32, k * P + 0.32, 0.32, w=0.1, fill=BLACK).pin(str(k + 1), (k * P, 0))
add(s.labels((P / 2, -4.6), (P / 2, 1.8), 'centre'))

s = Symbol('MP', C('Befestigungsloch M3', 'Mounting hole M3', 'Trou de fixation M3'))
s.circle(0, 0, 5.6, w=BODY).circle(0, 0, 3.2, w=FINE)     # pan head ISO 7045 (Ø5.6, recess H1) over the hole (ISO 273 fine)
add(s.line((-1.3, 0), (1.3, 0), w=0.5).line((0, -1.3), (0, 1.3), w=0.5).labels((3.6, -3.4), (3.6, -0.6)))

# LM3146 (National TL/H/7959, February 1995): Q1 C 1, B 2, E 3 with the emitter of Q2 (B 4, C 5); Q3 B 6, E 7, C 8; Q4
# B 9, E 10, C 11; Q5 B 12, E 13 (also the substrate), C 14. Q1 and Q2 drawn as on LM394, Q3 to Q5 turned with their
# bases from above or below.
s = dil_top('U', C('Transistorarray LM3146 (Draufsicht)', 'Transistor array LM3146 (top view)', 'Réseau de transistors LM3146 (vue de dessus)'),
            14, 'LM3146', outside=True)
x0, x1 = 1.1, 3 * P - 1.1
for yb, ye, yc in ((P, 2 * P, 0), (3 * P, 2 * P, 4 * P)):        # Q1, Q2: bar at x 2.5, the common emitters joined at 2P
    d = 1 if ye > yb else -1
    s.line((2.5, yb - 1.0), (2.5, yb + 1.0), w=0.35).line((x0, yb), (2.5, yb), w=FINE)
    s.line((2.5, yb - d * 0.45), (3.3, yb - d * 1.2), (3.3, yc), (x0, yc), w=FINE)
    s.line((2.5, yb + d * 0.45), (3.3, yb + d * 1.2), w=FINE, end='arrow', size=0.7).line((3.3, yb + d * 1.2), (3.3, ye), w=FINE)
s.line((3.3, 2 * P), (x0, 2 * P), w=FINE).circle(3.3, 2 * P, 0.5, fill=BLACK)
# Q3, Q4, Q5: bar centre, base row and edge, the emitter's and the collector's way to their pins.
for xc, yb, yr, edge, emitter, collector in (
        (3.6, 6 * P - 1.4, 5 * P, x0, [(2.4, 6 * P), (x0, 6 * P)], [(4.8, 6 * P), (x1, 6 * P)]),
        (5.1, 5 * P - 1.4, 5 * P, x1, [(6.2, 4 * P), (x1, 4 * P)], [(4.0, 4 * P), (4.0, 3 * P), (x1, 3 * P)]),
        (5.1, 2 * P - 1.4, 2 * P, x1, [(6.2, P), (x1, P)], [(4.0, P), (4.0, 0), (x1, 0)])):
    s.line((xc - 0.9, yb), (xc + 0.9, yb), w=0.35).line((xc, yb), (xc, yr), (edge, yr), w=FINE)
    e, c = (xc - 0.45, xc + 0.45) if emitter[0][0] < xc else (xc + 0.45, xc - 0.45)
    s.line((e, yb), emitter[0], w=FINE, end='arrow', size=0.7).line(*emitter, w=FINE)
    s.line((c, yb), *collector, w=FINE)
add(s)


# The standing PT-10 (H01) drawn with the round outline of its face over its pins, the rotor towards the front.
s = Symbol('RV', C('Trimmer RT10 (stehend, rund)', 'Trimmer RT10 (vertical, round)', 'Résistance ajustable RT10 (verticale, ronde)'), '100R')
s.circle(P, -P / 2, 2 * R10, w=BODY).circle(P, -P / 2 - 3.2, 2.0)
slot(s, P, -P / 2 - 3.2, 2.0, w=0.25)
add(pads(s, [(0, 0), (P, -P), (2 * P, 0)]).labels((P + R10 + 0.8, -P / 2 - 5.2), (P + R10 + 0.8, -P / 2 - 2.2)))

# --- CON 1
add = page(USR, 'CON 1', 'CON 1', 'Connecteurs 1')
s = Symbol('P', C('Lötstützpunkt (eckig)', 'Solder point (square)', 'Point de soudure (carré)'))
add(s.rect(-0.7, -0.7, 0.7, 0.7, fill=BLACK).pin('1', (0, 0)).labels((1.4, -3.4), (1.4, 0.8)))
s = Symbol('J', C('Buchse', 'Socket', 'Prise'))
add(s.line((0, 0), (P, 0)).arc(P + 1.6, 0, 3.2, 90, 270, w=0.35).pin('1', (0, 0)).labels((P, -4.4), (P, 2.4)))
s = Symbol('P', C('Stecker', 'Plug', 'Fiche'))
add(s.line((0, 0), (P + 1.6, 0)).rect(P + 1.6, -0.6, P + 3.2, 0.6, fill=BLACK).pin('1', (0, 0)).labels((P, -3.4), (P, 1.6)))
s = Symbol('', C('Nicht bestückt', 'Not fitted', 'Non monté'), numbered=False, listed=False, shown=False)
add(s.line((-2 * P, -2 * P), (2 * P, 2 * P), w=THICK).line((-2 * P, 2 * P), (2 * P, -2 * P), w=THICK))
s = Symbol('Jp', C('Jumper', 'Jumper', 'Cavalier'))
s.circle(0, 0, 1.6).circle(2 * P, 0, 1.6).line((0, -0.8), (0, -1.8), (2 * P, -1.8), (2 * P, -0.8), w=THICK)
add(s.pin('1', (0, 0)).pin('2', (2 * P, 0), label=(2 * P - 0.4, -2.2), align='right').labels((P, -5.6), (P, 1.2), 'centre'))
s = Symbol('CN', C('Schraubklemme', 'Screw terminal', 'Borne à vis'))
s.line((0, 0), (P, 0)).rect(P, -1.5, P + 3.0, 1.5).circle(P + 1.5, 0, 2.2)
add(slot(s, P + 1.5, 0, 2.2, w=W).pin('1', (0, 0)).labels((P, -4.6), (P, 2.0)))
s = Symbol('MP', C('Messpunkt', 'Test point', 'Point de test'))
add(s.line((0, 0), (P, 0)).rect(P, -1.2, P + 2.4, 1.2).circle(P + 1.2, 0, 1.6).pin('1', (0, 0)).labels((P, -4.4), (P, 1.8)))

# --- DIODE 10: anode (1) left or at the top, cathode (2) right or below
add = page(USR, 'DIODE 10', 'DIODE 10', 'Diodes 10')


def diode(s, ax=1.5 * P, cx=2.5 * P):
    return diode_shape(s.line((0, 0), (ax, 0)).line((cx, 0), (4 * P, 0)), ax, cx)


def upright(s, x=2.4):
    """The horizontal form turned, anode at the top, the texts at the right."""
    return s.vertical().labels((x, P - 0.6), (x, 2 * P + 0.4))


def led(s):
    return arrows_out(diode(s), 2 * P - 0.4, -1.8).labels((2 * P, -7.4), (2 * P, 2.0), 'centre')


def led_upright(s):
    upright(diode(s))
    return arrows_out(s, 1.6, 2 * P - 0.4).labels((6.0, P - 0.6), (6.0, 2 * P + 0.4))


def zener(s):
    return diode(s).line((2.5 * P, -1.5), (2.5 * P - 0.8, -1.5))


def schottky(s):
    diode(s)
    return s.line((2.5 * P + 0.7, -1.0), (2.5 * P + 0.7, -1.5), (2.5 * P, -1.5)).line((2.5 * P, 1.5), (2.5 * P - 0.7, 1.5), (2.5 * P - 0.7, 1.0))


def varicap(s):
    diode_shape(s.line((0, 0), (1.25 * P, 0)).line((2.25 * P + 0.8, 0), (4 * P, 0)), 1.25 * P, 2.25 * P)
    return s.line((2.25 * P + 0.8, -1.5), (2.25 * P + 0.8, 1.5), w=0.35)


def reference(s):
    """The bar bent at both ends in opposite directions, as the LM385 data sheet draws the reference diode."""
    return diode(s).line((2.5 * P, -1.5), (2.5 * P - 0.8, -1.5)).line((2.5 * P, 1.5), (2.5 * P + 0.8, 1.5))


for draw, prefix, value, de, en, fr in (
        (diode, 'D', '1N4148', 'Diode', 'Diode', 'Diode'),
        (None, 'D', 'rd', 'LED', 'LED', 'DEL'),
        (zener, 'D', '4V7', 'Z-Diode', 'Zener diode', 'Diode Zener'),
        (schottky, 'D', '1N5158', 'Schottky-Diode', 'Schottky diode', 'Diode Schottky'),
        (varicap, 'D', '', 'Kapazitätsdiode', 'Varicap diode', 'Diode varicap'),
        (reference, 'REF', 'LM385-1.2', 'Referenzdiode', 'Voltage reference diode', 'Diode de référence')):
    h = two_pin(prefix, C(f'{de} (h)', f'{en} (horizontal)', f'{fr} (horizontale)'), value)
    v = two_pin(prefix, C(f'{de} (v)', f'{en} (vertical)', f'{fr} (verticale)'), value)
    if draw is None:
        add(led(h))
        add(led_upright(v))
    else:
        add(draw(h))
        add(upright(draw(v)))

# --- OPTO
add = page(USR, 'OPTO', 'OPTO', 'Opto')


def optocoupler(caption, numbers=False):
    """LED (1 anode, 2 cathode) and phototransistor (4 collector, 3 emitter) in a box, the order of the common 4-pin
    packages; with `numbers` the contact numbers in the corners of the box."""
    s = Symbol('OC', caption)
    s.rect(P, -P, 7 * P, 3 * P, w=W)
    s.line((0, 0), (2 * P, 0), (2 * P, 0.5 * P)).line((2 * P, 1.5 * P), (2 * P, 2 * P), (0, 2 * P))
    s.poly((2 * P - 1.4, 0.5 * P), (2 * P + 1.4, 0.5 * P), (2 * P, 1.5 * P), fill=None).line((2 * P - 1.4, 1.5 * P), (2 * P + 1.4, 1.5 * P), w=0.35)
    s.arrow((3 * P - 0.6, 0.8 * P), (4 * P - 0.4, 0.8 * P), size=0.9).arrow((3 * P - 0.6, 1.3 * P), (4 * P - 0.4, 1.3 * P), size=0.9)
    s.line((4.6 * P, 0.1 * P), (4.6 * P, 1.9 * P), w=THICK)
    s.line((4.6 * P, 0.6 * P), (6 * P, -0.2 * P), (6 * P, 0), (8 * P, 0))
    s.line((4.6 * P, 1.4 * P), (6 * P, 2.2 * P), end='arrow', size=1.2).line((6 * P, 2.2 * P), (6 * P, 2 * P), (8 * P, 2 * P))
    corner = {'1': ((P + 0.5, -P + 0.4), 'left'), '2': ((P + 0.5, 3 * P - 2.3), 'left'),
              '4': ((7 * P - 0.5, -P + 0.4), 'right'), '3': ((7 * P - 0.5, 3 * P - 2.3), 'right')}
    for name, at in (('1', (0, 0)), ('2', (0, 2 * P)), ('4', (8 * P, 0)), ('3', (8 * P, 2 * P))):
        if numbers:
            s.pin(name, at, label=corner[name][0], align=corner[name][1], shown=True)
        elif at[0]:
            s.pin(name, at, label=(at[0] - 0.4, at[1] - 2.2), align='right')
        else:
            s.pin(name, at)
    return s.labels((P, -P - 3.4), (P, 3 * P + 0.6))


def digit(s, x, y, w=1.8, h=3.6):
    """The digit "8." with its segments a to g and the decimal point, upper left corner at (x, y)."""
    for (ax, ay), (bx, by) in (((0, 0), (1, 0)), ((1, 0), (1, 1)), ((1, 1), (1, 2)), ((0, 2), (1, 2)), ((0, 1), (0, 2)), ((0, 0), (0, 1)), ((0, 1), (1, 1))):
        s.line((x + ax * w, y + ay * h / 2), (x + bx * w, y + by * h / 2), w=0.35)
    return s.circle(x + w + 0.5, y + h, 0.5, fill=BLACK)


def display(n, caption):
    """A multiplexed display of n digits: the segment lines a to dp on the left, the common cathode of each digit
    (D1 at the left) on the right."""
    pitch = 3.0
    width = math.ceil((n * pitch + 7.6) / P) * P
    s = box('X', caption, ['a', 'b', 'c', 'd', 'e', 'f', 'g', 'dp'], [f'D{k + 1}' for k in range(n)], width=width)
    x0 = P + 3.6 + (width - 7.6 - n * pitch) / 2
    y0 = 4.5 * P - 1.8
    s.rect(x0 - 0.8, y0 - 0.8, x0 + n * pitch, y0 + 4.4, w=FINE)
    for k in range(n):
        digit(s, x0 + k * pitch, y0)
    return s


add(optocoupler(C('Optokoppler', 'Optocoupler', 'Optocoupleur')))
s = seven_segment(C('7-Segment-Anzeige', 'Seven-segment display', 'Afficheur 7 segments'))
s.prefix = '7Seg'
add(s)
add(display(8, C('LED-Anzeige 8 Stellen, gemeinsame Kathode', 'LED display, 8 digits, common cathode', 'Afficheur à DEL, 8 chiffres, cathode commune')))
add(display(5, C('LED-Anzeige 5 Stellen, gemeinsame Kathode', 'LED display, 5 digits, common cathode', 'Afficheur à DEL, 5 chiffres, cathode commune')))


def lamp(caption, value):
    """Signal lamp (DIN EN 60617-8 08-10-01): a circle with a cross."""
    s = two_pin('X', caption, value)
    s.line((0, 0), (P + 0.3, 0)).circle(2 * P, 0, 2 * P - 0.6).line((3 * P - 0.3, 0), (4 * P, 0))
    for a in (45, -45):
        s.line(rot((2 * P - 2.24, 0), (2 * P, 0), a), rot((2 * P + 2.24, 0), (2 * P, 0), a))
    return s


add(lamp(C('Glühlampe (v)', 'Incandescent lamp (vertical)', 'Lampe à incandescence (verticale)'), '5V/30 mA').vertical().labels((3.2, P - 0.6), (3.2, 2 * P + 0.4)))
add(lamp(C('Glühlampe (h)', 'Incandescent lamp (horizontal)', 'Lampe à incandescence (horizontale)'), '5V/30 mA'))
s = two_pin('X', C('Glimmlampe (v)', 'Neon lamp (vertical)', 'Lampe au néon (verticale)'), '60V')
s.line((0, 0), (2 * P - 0.8, 0)).circle(2 * P, 0, 2 * P - 0.6).line((2 * P + 0.8, 0), (4 * P, 0))
s.line((2 * P - 0.8, -1.4), (2 * P - 0.8, 1.4), w=0.35).line((2 * P + 0.8, -1.4), (2 * P + 0.8, 1.4), w=0.35).circle(2 * P - 1.5, 1.2, 0.5, fill=BLACK)
add(s.vertical().labels((3.2, P - 0.6), (3.2, 2 * P + 0.4)))
add(optocoupler(C('Optokoppler (Pins nummeriert)', 'Optocoupler (pins numbered)', 'Optocoupleur (broches numérotées)'), numbers=True))

# --- PWR: supply and ground signs after DIN EN 60617-2 (02-15), the value is the net
add = page(USR, 'PWR', 'PWR', "Symboles d'alimentation")


def net(caption, value, at):
    s = Symbol('', caption, value, numbered=False, listed=False, shown=False, value_shown=True)
    s.pin('1', (0, 0))
    align = 'centre' if at[0] == 0 else 'left'
    return s.labels(at, at, align)


add(supply('V1', C('Versorgung V1 (Strich)', 'Supply V1 (bar)', 'Alimentation V1 (barre)')))
s = net(C('Versorgung V1 (waagerecht)', 'Supply V1 (horizontal)', 'Alimentation V1 (horizontale)'), 'V1', (P + 0.8, -1.25))
add(s.line((0, 0), (P, 0)).line((P, -1.8), (P, 1.8), w=0.35))
s = net(C('Versorgung V2 (Pfeil)', 'Supply V2 (arrow)', 'Alimentation V2 (flèche)'), 'V2', (0, -P - 3.2))
add(s.line((0, 0), (0, -P)).line((-1.2, -P + 1.6), (0, -P), (1.2, -P + 1.6), w=0.35))
s = net(C('Versorgung V2 (Pfeil, waagerecht)', 'Supply V2 (arrow, horizontal)', 'Alimentation V2 (flèche, horizontale)'), 'V2', (P + 0.8, -1.25))
add(s.line((0, 0), (P, 0)).line((P - 1.6, -1.2), (P, 0), (P - 1.6, 1.2), w=0.35))
s = net(C('Masse (Balken)', 'Ground (bar)', 'Masse (barre)'), 'GND', (0, P + 1.4))
add(s.line((0, 0), (0, P)).rect(-2.0, P, 2.0, P + 0.7, w=0.1, fill=BLACK))
s = net(C('Masse (Gehäuse)', 'Chassis ground', 'Masse (châssis)'), 'GND', (0, P + 2.0))
s.line((0, 0), (0, P)).line((-2.0, P), (2.0, P), w=0.35)
for x in (-2.0, 0, 2.0):
    s.line((x, P), (x - 1.0, P + 1.4))
add(s)
s = net(C('Erde', 'Earth', 'Terre'), 'E', (0, P + 2.4))
s.line((0, 0), (0, P))
for k, half in enumerate((2.0, 1.3, 0.6)):
    s.line((-half, P + k * 0.8), (half, P + k * 0.8), w=0.35)
add(s)
s = net(C('Schutzerde', 'Protective earth', 'Terre de protection'), 'PE', (0, P + 3.6))
s.line((0, 0), (0, P + 0.4)).circle(0, P + 1.1, 4.4)
for k, half in enumerate((1.6, 1.0, 0.4)):
    s.line((-half, P + 0.4 + k * 0.7), (half, P + 0.4 + k * 0.7), w=0.35)
add(s)
for value, de, en, fr in (('+', 'Plus (Kreis)', 'Plus (circle)', 'Plus (cercle)'), ('-', 'Minus (Kreis)', 'Minus (circle)', 'Moins (cercle)'),
                          ('S+', 'S+ (Kreis)', 'S+ sense plus (circle)', 'S+ détection plus (cercle)')):
    s = net(C(de, en, fr), value, (0, -P - 3.4))
    add(s.line((0, 0), (0, -P + 0.8)).circle(0, -P, 1.6))

# --- PWR SUPPLY
add = page(USR, 'PWR SUPPLY', 'PWR SUPPLY', 'Alimentations')


def cell(s, x):
    """A cell across the x axis: the long line (+) at x, the short heavy line (−) 1 mm to the right."""
    return s.line((x, -2.6), (x, 2.6)).line((x + 1.0, -1.3), (x + 1.0, 1.3), w=0.7)


def battery(caption, cells, length=4 * P):
    """A battery drawn along the x axis, then stood upright: + (contact "+") at the top."""
    s = Symbol('BAT', caption, '9 V')
    s.pin('+', (0, 0)).pin('-', (length, 0), text=MINUS)
    if cells == 'dashed':
        cell(cell(s.line((0, 0), (1.6, 0)), 1.6), length - 2.6).line((length - 1.6, 0), (length, 0))
        for k in range(3):      # the cells between drawn as a dashed line
            x = 3.2 + k * 1.45
            s.line((x, 0), (x + 0.9, 0))
        plus = (-3.4, -0.8)
    elif cells == 2:
        cell(cell(s.line((0, 0), (P + 0.5, 0)), P + 0.5).line((P + 1.5, 0), (2 * P + 0.3, 0)), 2 * P + 0.3).line((2 * P + 1.3, 0), (length, 0))
        plus = (-3.4, P - 2.4)
    else:
        cell(s.line((0, 0), (P - 0.5, 0)), P - 0.5).line((P + 0.5, 0), (length, 0))
        plus = (-3.4, P - 2.9)
    s.vertical().text('+', *plus, 2.2, 'centre')
    return s.labels((3.6, length / 2 - 2.8), (3.6, length / 2 + 0.2))


add(battery(C('Batterie 9 V (mehrzellig, v)', '9 V battery (several cells, vertical)', 'Pile 9 V (plusieurs éléments, verticale)'), 'dashed'))
add(battery(C('Batterie 9 V (v)', '9 V battery (vertical)', 'Pile 9 V (verticale)'), 2))
add(battery(C('Batterie 9 V (kurz, v)', '9 V battery (short, vertical)', 'Pile 9 V (courte, verticale)'), 1, 2 * P))

s = Symbol('BR', C('Brückengleichrichter B30C2000 (Block)', 'Bridge rectifier B30C2000 (block)', 'Pont redresseur B30C2000 (bloc)'), 'B30C2000')
x0, x1 = P, 5 * P
s.rect(x0, 0, x1, 3 * P)
for y, name in ((P, '~1'), (2 * P, '~2')):
    s.line((0, y), (x0, y)).text('~', x0 + 0.6, y - 1.2, 2.2).pin(name, (0, y))
for y, name, sign in ((P, '+', '+'), (2 * P, '-', MINUS)):
    s.line((x1, y), (x1 + P, y)).text(sign, x1 - 0.6, y - 1.2, 2.2, 'right')
    s.pin(name, (x1 + P, y), label=(x1 + P - 0.4, y - 2.2), align='right', text=sign)
diode_shape(s.line((2.4 * P, 1.5 * P), (2.7 * P, 1.5 * P)).line((3.3 * P, 1.5 * P), (3.6 * P, 1.5 * P)), 2.7 * P, 3.3 * P, half=0.9, y=1.5 * P)
add(s.labels((x0, -3.4), (x0, 3 * P + 0.6)))

s = Symbol('BR', C('Brückengleichrichter B30C2000', 'Bridge rectifier B30C2000', 'Pont redresseur B30C2000'), 'B30C2000')
corners = {'left': (0, 0), 'top': (2 * P, -2 * P), 'right': (4 * P, 0), 'bottom': (2 * P, 2 * P)}
for a, b in (('bottom', 'left'), ('bottom', 'right'), ('left', 'top'), ('right', 'top')):     # each diode conducts towards +
    pa, pb = corners[a], corners[b]
    s.line(pa, pb)
    mx, my = (pa[0] + pb[0]) / 2, (pa[1] + pb[1]) / 2
    ang = math.degrees(math.atan2(-(pb[1] - pa[1]), pb[0] - pa[0]))
    s.poly(*[rot(p, (mx, my), ang) for p in ((mx - 0.9, my - 1.0), (mx - 0.9, my + 1.0), (mx + 0.9, my))], fill=None)
    s.line(rot((mx + 0.9, my - 1.0), (mx, my), ang), rot((mx + 0.9, my + 1.0), (mx, my), ang), w=0.35)
s.pin('~1', corners['left']).pin('~2', corners['right'], label=(4 * P - 0.4, -2.2), align='right')
s.pin('+', corners['top'], label=(2 * P + 0.6, -2 * P)).pin('-', corners['bottom'], label=(2 * P + 0.6, 2 * P - 2.2), text=MINUS)
s.text('~', -0.5, -2.8, 2.2, 'right').text('~', 4 * P + 0.5, -2.8, 2.2).text('+', 2 * P + 0.7, -2 * P - 2.6, 2.2).text(MINUS, 2 * P + 0.7, 2 * P + 0.2, 2.2)
add(s.labels((4 * P + 0.8, 0.4), (4 * P + 0.8, 3.2)))

# TL431 after TI SLVS543S: cathode at the top, anode below, the reference input from the left at the triangle; the bar
# bent towards the anode at both ends.
s = Symbol('D', C('Einstellbare Referenz TL431', 'Adjustable shunt reference TL431', 'Référence ajustable TL431'), 'TL431')
s.line((0, 0), (0, 1.5 * P)).line((0, 2.5 * P), (0, 4 * P)).poly((-1.5, 2.5 * P), (1.5, 2.5 * P), (0, 1.5 * P), fill=None)
s.line((-1.5, 1.5 * P + 0.7), (-1.5, 1.5 * P), (1.5, 1.5 * P), (1.5, 1.5 * P + 0.7), w=0.35).line((-2 * P, 2 * P), (-0.75, 2 * P))
s.pin('K', (0, 0), label=(0.5, -0.2), shown=True).pin('A', (0, 4 * P), label=(0.5, 4 * P - 2.4), shown=True)
add(s.pin('R', (-2 * P, 2 * P), label=(-2 * P + 0.3, 2 * P - 2.2), shown=True).labels((2.4, P - 0.6), (2.4, 2 * P + 0.4)))

for de, en, fr, value, adj in (('Festspannungsregler 7805', 'Fixed voltage regulator 7805', 'Régulateur fixe 7805', '7805', 'GND'),
                               ('Einstellbarer Spannungsregler LM317', 'Adjustable voltage regulator LM317', 'Régulateur ajustable LM317', 'LM317', 'ADJ')):
    s = Symbol('VR', C(de, en, fr), value)
    s.rect(P, -P, 5 * P, 2 * P).line((0, 0), (P, 0)).line((5 * P, 0), (6 * P, 0)).line((3 * P, 2 * P), (3 * P, 3 * P))
    s.text('IN', P + 0.5, -1.0, 2.0).text('OUT', 5 * P - 0.5, -1.0, 2.0, 'right').text(adj, 3 * P, 2 * P - 2.6, 2.0, 'centre')
    s.pin('IN', (0, 0)).pin('OUT', (6 * P, 0), label=(6 * P - 0.4, -2.2), align='right').pin(adj, (3 * P, 3 * P), label=(3 * P + 0.4, 3 * P - 2.2))
    add(s.labels((P, -P - 3.4), (P, 3 * P + 0.4)))

# Polarity of a DC barrel connector (IEC 60417-5926) as asked: plus to the sleeve, the centre pin (dot) to minus.
s = Symbol('', C('Polaritätszeichen Hohlstecker', 'DC plug polarity sign', "Symbole de polarité de fiche d'alimentation"),
           numbered=False, listed=False, shown=False)
for x, plus in ((-7.4, True), (7.4, False)):
    s.circle(x, 0, 3.4).line((x - 1.0, 0), (x + 1.0, 0))
    if plus:
        s.line((x, -1.0), (x, 1.0))
add(s.line((-5.7, 0), (-2.0, 0)).arc(0, 0, 4.0, 45, 315).circle(0, 0, 1.2, fill=BLACK).line((0, 0), (5.7, 0)).labels((-9.0, -4.6), None))


def mains(prefix, caption, leads, value='', female=False):
    """A mains connector with its leads from the left (`leads`: contact name and the text at the lead), plug pins
    (DIN EN 60617-3 male contacts) or socket contacts (female) at the right in an insulating body."""
    s = Symbol(prefix, caption, value, numbered=bool(prefix))
    bottom = (len(leads) - 1) * P
    if female:
        s.poly((2 * P, -1.6), (3.6 * P, -1.6), (4 * P, -0.6), (4 * P, bottom + 0.6), (3.6 * P, bottom + 1.6), (2 * P, bottom + 1.6), fill=None, w=0.35)
    else:
        s.rect(2 * P, -1.2, 3 * P, bottom + 1.2, w=0.35)
    for k, (name, text) in enumerate(leads):
        y = k * P
        s.text(text, 0.6, y - 2.3, 1.8).pin(name, (0, y))
        if female:
            s.line((0, y), (3 * P - 1.0, y)).arc(3 * P, y, 2.0, 90, 270, w=0.35)
        else:
            longer = 0.4 * P if name == 'PE' else 0     # the earth pin leads
            s.line((0, y), (3 * P, y)).rect(3 * P, y - 0.45, 4 * P + longer, y + 0.45, fill=BLACK)
    return s.labels((2 * P, -4.8), (4.6 * P, bottom / 2 - 1.25))


add(mains('P', C('Netzstecker (h)', 'Mains plug (horizontal)', 'Fiche secteur (horizontale)'), [('L', 'L'), ('N', 'N')], '230 VAC'))
s = Symbol('F', C('Sicherung 1 A (h)', 'Fuse 1 A (horizontal)', 'Fusible 1 A (horizontal)'), '1 A')
add(s.rect(P, -1.0, 3 * P, 1.0).line((0, 0), (4 * P, 0)).pin('1', (0, 0)).pin('2', (4 * P, 0), label=(4 * P - 0.4, -2.2), align='right').labels((2 * P, -4.6), (2 * P, 1.6), 'centre'))
add(mains('', C('Netzstecker mit Schutzleiter', 'Mains plug with protective earth', 'Fiche secteur avec terre'), [('L', 'L'), ('PE', 'E'), ('N', 'N')]))
add(mains('J', C('Netzausgang (Kaltgerätebuchse)', 'Mains outlet (IEC appliance socket)', 'Sortie secteur (embase CEI femelle)'),
          [('L', 'L'), ('PE', 'E'), ('N', 'N')], '230 VAC', female=True))

# --- RES VAR: the wiper is contact 3 as on the page "Widerstände"
add = page(USR, 'RES VAR', 'RES VAR', 'Résistances variables')


def pot(prefix, caption, value, preset=False, length=4 * P, vertical=False):
    """A potentiometer (wiper arrow) or a preset one (wiper with a bar): horizontal with the wiper below, vertical with
    the wiper at the right; the body 2 P long in the middle."""
    s = two_pin(prefix, caption, value, length=length)
    c = length / 2
    s.line((0, 0), (c - P, 0)).rect(c - P, -1.0, c + P, 1.0).line((c + P, 0), (length, 0))
    y = -2 * P if vertical else 2 * P
    tip = (-1.7 if vertical else 1.7) if preset else (-1.0 if vertical else 1.0)
    if preset:
        s.line((c, y), (c, tip), end='bar', size=1.8)
    else:
        s.line((c, y), (c, tip), end='arrow', size=1.2)
    if vertical:
        s.pin('3', (c, y)).vertical()
        return s.labels((-2.0, c - P - 0.4), (-2.0, c + 0.4), 'right')
    s.pin('3', (c, y), label=(c + 0.5, y - 2.2))
    return s.labels((c, -5.6), (c + P + 0.6, 1.6), 'centre', 'left')


add(pot('P', C('Potentiometer (v)', 'Potentiometer (vertical)', 'Potentiomètre (vertical)'), '1M log', vertical=True))
add(pot('P', C('Potentiometer (h)', 'Potentiometer (horizontal)', 'Potentiomètre (horizontal)'), '500k'))
add(pot('VR', C('Trimmpotentiometer (v)', 'Trimmer potentiometer (vertical)', 'Potentiomètre ajustable (vertical)'), '100k', True, vertical=True))
add(pot('VR', C('Trimmpotentiometer (h)', 'Trimmer potentiometer (horizontal)', 'Potentiomètre ajustable (horizontal)'), '10k', True))
add(pot('P', C('Potentiometer, lang (h)', 'Potentiometer, long (horizontal)', 'Potentiomètre, long (horizontal)'), '100k lin', length=6 * P))
add(pot('VR', C('Trimmpotentiometer, lang (v)', 'Trimmer potentiometer, long (vertical)', 'Potentiomètre ajustable, long (vertical)'), '100k lin',
        True, 6 * P, vertical=True))


def mid(p):
    """A point of the horizontal symbol (centre (2P, 0)) moved to the same place of the vertical one (centre (0, 2P))."""
    return (p[0] - 2 * P, p[1] + 2 * P)


s = resistor_body(two_pin('R', C('Fotowiderstand (v)', 'Photoresistor (vertical)', 'Photorésistance (verticale)'), 'LDR 04')).vertical()
for x, y in ((1.3, 2 * P - 1.5), (1.3, 2 * P + 0.1)):     # light falling on it from the upper right
    s.arrow((x + 1.7, y - 1.7), (x, y), size=0.9)
add(s.labels((4.4, P - 0.4), (4.4, 2 * P + 0.4)))
s = resistor_body(two_pin('R', C('Fotowiderstand (h)', 'Photoresistor (horizontal)', 'Photorésistance (horizontale)'), 'LDR 04'))
arrows_in(s, 2 * P - 0.7, -1.3)
add(s.labels((2 * P, -7.0), (2 * P, 2.0), 'centre'))
NTC = ((P - 0.6, 2.4), (P + 0.4, 2.4), (3 * P + 0.2, -2.4))     # the line of the non-linear dependence
s = resistor_body(two_pin('R', C('Heißleiter (v)', 'NTC thermistor (vertical)', 'Thermistance CTN (verticale)'), '−0.3%/°C')).vertical()
add(s.line(*[mid(p) for p in NTC]).text('-ϑ', -3.6, 2 * P - 1.6, 2.0, 'right').labels((3.6, P - 0.4), (3.6, 2 * P + 0.4)))
s = resistor_body(two_pin('R', C('Heißleiter (h)', 'NTC thermistor (horizontal)', 'Thermistance CTN (horizontale)'), '−0.3%/°C'))
add(s.line(*NTC).text('-ϑ', 3 * P + 0.4, 0.6, 2.0).labels((2 * P, -5.6), (2 * P, 3.4), 'centre'))
