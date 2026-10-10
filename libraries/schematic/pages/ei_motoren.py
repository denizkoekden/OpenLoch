# More machines for the page "Motoren", after DIN EN 60617-6 (circle with M and the kind of current, windings drawn as
# coils), with the terminal markings of IEC 60034-8: DC armature A1 A2, series field D1 D2, shunt field E1 E2, three-phase
# stator U1 V1 W1 / U2 V2 W2, slip-ring rotor K L M. Also the thermal overload relay after DIN EN 60617-7 and the signs
# of a winding and of the three-phase connections (delta, star, zigzag) after DIN EN 60617-2.
add = extend(EI, 'Motoren')
D1 = 2 * P - 0.6     # the circle of a motor with two leads, as on the page
D3 = 4 * P - 0.6     # the circle of the three-phase motor


def edge(d, dx):
    """How far above or below its centre a vertical line dx beside the centre meets a circle of diameter d."""
    return math.sqrt((d / 2) ** 2 - dx ** 2)


def kind(s, cx, cy, sign, big=False):
    """M and the kind of current in a circle: '' (M alone), '=', '~', '3~' or 'dc' (the sign ⎓ drawn)."""
    if big:
        s.text('M', cx, cy - 3.4, 3.0, 'centre').text(sign, cx, cy + 0.2, 2.5, 'centre')
    elif not sign:
        s.text('M', cx, cy - 1.6, 3.0, 'centre')
    elif sign == 'dc':
        s.text('M', cx, cy - 2.5, 2.0, 'centre').line((cx - 1.1, cy + 0.5), (cx + 1.1, cy + 0.5))
        for k in range(3):
            s.line((cx - 1.1 + 0.85 * k, cy + 1.1), (cx - 0.6 + 0.85 * k, cy + 1.1))
    else:
        s.text('M', cx, cy - 2.5, 2.0, 'centre').text(sign, cx, cy - 0.3, 2.0, 'centre')
    return s


def winding(s, x, y0, y1, turns=4):
    """A vertical coil of half-turns bulging to the right."""
    d = (y1 - y0) / turns
    for k in range(turns):
        s.arc(x, y0 + d * (k + 0.5), d, 270, 450)
    return s


def upright(caption, sign, names):
    s = Symbol('M', caption)
    s.line((0, 0), (0, P + 0.3)).circle(0, 2 * P, D1).line((0, 3 * P - 0.3), (0, 4 * P))
    kind(s, 0, 2 * P, sign)
    s.pin(names[0], (0, 0), label=(0.6, 0.2), shown=True).pin(names[1], (0, 4 * P), label=(0.6, 4 * P - 2.4), shown=True)
    return s.labels((P + 1.2, P - 0.4), (P + 1.2, 2 * P + 0.6))


def level(caption, sign, names):
    s = Symbol('M', caption)
    s.line((0, 0), (P + 0.3, 0)).circle(2 * P, 0, D1).line((3 * P - 0.3, 0), (4 * P, 0))
    kind(s, 2 * P, 0, sign)
    s.pin(names[0], (0, 0), shown=True).pin(names[1], (4 * P, 0), label=(4 * P - 0.4, -2.2), align='right', shown=True)
    return s.labels((2 * P, -6.0), (2 * P, 3.0), 'centre')


add(level(C('Motor (waagerecht)', 'Motor, horizontal', 'Moteur horizontal'), '', ('1', '2')))
add(level(C('Gleichstrommotor (waagerecht)', 'DC motor, horizontal', 'Moteur à courant continu horizontal'), '=', ('A1', 'A2')))
add(level(C('Gleichstrommotor (Zeichen ⎓)', 'DC motor (sign ⎓)', 'Moteur à courant continu (signe ⎓)'), 'dc', ('A1', 'A2')))
add(upright(C('Gleichstrommotor (Zeichen ⎓, senkrecht)', 'DC motor (sign ⎓), vertical', 'Moteur à courant continu (signe ⎓) vertical'),
            'dc', ('A1', 'A2')))
add(level(C('Wechselstrommotor (waagerecht)', 'AC motor, horizontal', 'Moteur à courant alternatif horizontal'), '~', ('U1', 'U2')))


# Thermal overload relay: the thermal element (the lead bent into a step) in a rectangle
def thermal(s, x, k):
    s.rect(x - 1.6, P, x + 0.4, 3 * P)
    s.line((x, 0), (x, P + 1.0), (x - 1.2, P + 1.0), (x - 1.2, 3 * P - 1.0), (x, 3 * P - 1.0), (x, 4 * P))
    s.pin(str(2 * k + 1), (x, 0), label=(x + 0.6, 0.2), shown=True)
    s.pin(str(2 * k + 2), (x, 4 * P), label=(x + 0.6, 4 * P - 2.1), shown=True)
    return s


for n, caption in ((1, C('Motorschutzrelais', 'Motor protection relay', 'Relais de protection moteur')),
                   (3, C('Motorschutzrelais (dreipolig)', 'Motor protection relay, three-pole', 'Relais de protection moteur tripolaire'))):
    s = Symbol('F', caption)
    for k in range(n):
        thermal(s, 2 * P * k, k)
    x = 2 * P * (n - 1)
    if n > 1:
        dashed(s, (0.4, 2 * P), (x - 1.6, 2 * P))
    add(s.labels((x + 1.4, P), (x + 1.4, 2 * P + 0.4)))


# Three-phase motors: circle with M 3~, leads U1 V1 W1 (and more)
def three_phase(caption, cx, cy, d=D3, inner=None):
    s = Symbol('M', caption)
    s.circle(cx, cy, d)
    if inner:
        s.circle(cx, cy, inner)
    if d >= D3 and not inner:
        kind(s, cx, cy, '3~', big=True)
    else:
        kind(s, cx, cy, '3~')
    return s


def top_pin(s, name, x, y=0):
    return s.pin(name, (x, y), label=(x + 0.3, y - 2.4), shown=True)


def bottom_pin(s, name, x, y):
    return s.pin(name, (x, y), label=(x + 0.3, y + 0.6), shown=True)


def right_labels(s, cx, cy, d=D3):
    return s.labels((cx + d / 2 + 0.8, cy - 2.6), (cx + d / 2 + 0.8, cy + 0.2))


def straight(s, cx, cy, y, names, d=D3, pins=None, start=None):
    """Leads P apart from the circle (or the circle of diameter `start`) straight up (y < cy) or down to y."""
    up = y < cy
    for k, name in enumerate(names):
        x = cx + (k - 1) * P
        e = edge(start or d, x - cx)
        s.line((x, y), (x, cy - e if up else cy + e))
        (top_pin if up else bottom_pin)(s, name, x, y)
    return s


def spread(s, cx, cy, y, names, d, start=None):
    """Leads leaving the circle P apart, the outer two bent outwards to 2 * P, ending at y."""
    up = y < cy
    sg = -1 if up else 1
    for k, name in enumerate(names):
        dx = (k - 1) * P
        e = edge(start or d, dx)
        if k == 1:
            s.line((cx, cy + sg * e), (cx, y))
        else:
            s.line((cx + dx, cy + sg * e), (cx + dx, y - sg * 2 * P), (cx + 2 * dx, y - sg * P), (cx + 2 * dx, y))
        (top_pin if up else bottom_pin)(s, name, cx + 2 * dx, y)
    return s


s = three_phase(C('Drehstrommotor (Anschlüsse unten)', 'Three-phase motor (terminals at the bottom)', 'Moteur triphasé (bornes en bas)'), 2 * P, 0)
add(right_labels(straight(s, 2 * P, 0, 3 * P, ('U1', 'V1', 'W1')), 2 * P, 0))
s = three_phase(C('Drehstrommotor (Anschlüsse oben, gespreizt)', 'Three-phase motor (terminals at the top, spread)',
                  'Moteur triphasé (bornes en haut, écartées)'), 2 * P, 4 * P)
add(right_labels(spread(s, 2 * P, 4 * P, 0, ('U1', 'V1', 'W1'), D3), 2 * P, 4 * P))
s = three_phase(C('Drehstrommotor (Anschlüsse unten, gespreizt)', 'Three-phase motor (terminals at the bottom, spread)',
                  'Moteur triphasé (bornes en bas, écartées)'), 2 * P, 0)
add(right_labels(spread(s, 2 * P, 0, 4 * P, ('U1', 'V1', 'W1'), D3), 2 * P, 0))


def sideways(caption, up):
    """The middle lead straight, the outer two leaving the circle 120 degrees from it and running up (or down) beside it."""
    cx, cy = 3 * P, (3 * P if up else 0)
    y = 0 if up else 3 * P
    sg = 1 if up else -1
    s = three_phase(caption, cx, cy)
    rr = D3 / 2
    s.line((cx, y), (cx, cy - sg * rr))
    for side, name in ((-1, 'U1'), (1, 'W1')):
        a = (cx + side * rr * math.cos(math.radians(30)), cy + sg * rr * 0.5)
        k = (3 * P - rr * math.cos(math.radians(30))) / math.cos(math.radians(30))
        b = (cx + side * 3 * P, a[1] + sg * k * 0.5)
        s.line(a, b, (b[0], y))
        (top_pin if up else bottom_pin)(s, name, b[0], y)
    (top_pin if up else bottom_pin)(s, 'V1', cx, y)
    return s.labels((6 * P + 0.8, cy - 2.6), (6 * P + 0.8, cy + 0.2))


add(sideways(C('Drehstrommotor (Anschlüsse oben, seitlich)', 'Three-phase motor (terminals at the top, at the sides)',
               'Moteur triphasé (bornes en haut, latérales)'), True))
add(sideways(C('Drehstrommotor (Anschlüsse unten, seitlich)', 'Three-phase motor (terminals at the bottom, at the sides)',
               'Moteur triphasé (bornes en bas, latérales)'), False))

STATOR, ENDS, ROTOR = ('U1', 'V1', 'W1'), ('U2', 'V2', 'W2'), ('K', 'L', 'M')
RING = 7.0           # the rotor circle of a slip-ring motor
s = three_phase(C('Drehstrommotor mit sechs Anschlüssen', 'Three-phase motor with six terminals', 'Moteur triphasé à six bornes'), 2 * P, 3 * P)
straight(s, 2 * P, 3 * P, 0, STATOR)
add(right_labels(straight(s, 2 * P, 3 * P, 6 * P, ENDS), 2 * P, 3 * P))
s = three_phase(C('Drehstrom-Schleifringläufermotor', 'Three-phase slip-ring motor', 'Moteur triphasé à bagues'), 2 * P, 3 * P, inner=RING)
straight(s, 2 * P, 3 * P, 0, STATOR)
add(right_labels(straight(s, 2 * P, 3 * P, 6 * P, ROTOR, start=RING), 2 * P, 3 * P))


def side_leads(s, cx, x, names, start=D3):
    """Leads P apart from the circle straight left (x < cx) or right to x."""
    for k, name in enumerate(names):
        y = (k - 1) * P
        e = edge(start, y)
        s.line((x, y), (cx - e if x < cx else cx + e, y))
        if x < cx:
            s.pin(name, (x, y), shown=True)
        else:
            s.pin(name, (x, y), label=(x - 0.4, y - 2.2), align='right', shown=True)
    return s


s = three_phase(C('Drehstrommotor mit sechs Anschlüssen (seitlich)', 'Three-phase motor with six terminals (at the sides)',
                  'Moteur triphasé à six bornes (latérales)'), 4 * P, 0)
side_leads(side_leads(s, 4 * P, 0, STATOR), 4 * P, 8 * P, ENDS)
add(s.labels((4 * P, -D3 / 2 - 3.6), (4 * P, D3 / 2 + 0.6), 'centre'))
s = three_phase(C('Drehstrom-Schleifringläufermotor (seitlich)', 'Three-phase slip-ring motor (at the sides)',
                  'Moteur triphasé à bagues (latérales)'), 4 * P, 0, inner=RING)
side_leads(side_leads(s, 4 * P, 0, STATOR), 4 * P, 8 * P, ROTOR, start=RING)
add(s.labels((4 * P, -D3 / 2 - 3.6), (4 * P, D3 / 2 + 0.6), 'centre'))

SMALL, SMALL_RING = 8.0, 6.0     # the circles of the motors with spread leads
s = three_phase(C('Drehstrommotor mit sechs Anschlüssen (gespreizt)', 'Three-phase motor with six terminals (spread)',
                  'Moteur triphasé à six bornes (écartées)'), 2 * P, 3.5 * P, d=SMALL)
spread(s, 2 * P, 3.5 * P, 0, STATOR, SMALL)
add(right_labels(spread(s, 2 * P, 3.5 * P, 7 * P, ENDS, SMALL), 2 * P, 3.5 * P, SMALL))
s = three_phase(C('Drehstrom-Schleifringläufermotor (gespreizt)', 'Three-phase slip-ring motor (spread)', 'Moteur triphasé à bagues (écartées)'),
                2 * P, 3.5 * P, d=SMALL, inner=SMALL_RING)
spread(s, 2 * P, 3.5 * P, 0, STATOR, SMALL)
add(right_labels(spread(s, 2 * P, 3.5 * P, 7 * P, ROTOR, SMALL, start=SMALL_RING), 2 * P, 3.5 * P, SMALL))


# Commutator motors: the armature with its field winding
def series(caption, sign, names):
    s = Symbol('M', caption)
    s.line((0, 0), (0, P + 0.3)).circle(0, 2 * P, D1).line((0, 3 * P - 0.3), (0, 3.5 * P))
    winding(kind(s, 0, 2 * P, sign), 0, 3.5 * P, 5.5 * P).line((0, 5.5 * P), (0, 6 * P))
    s.pin(names[0], (0, 0), label=(0.6, 0.2), shown=True).pin(names[1], (0, 6 * P), label=(1.0, 6 * P - 2.4), shown=True)
    return s.labels((P + 1.2, P - 0.4), (P + 1.2, 2 * P + 0.6))


def shunt(caption, sign, names):
    s = Symbol('M', caption)
    s.line((0, 0), (0, P + 0.3)).circle(0, 2 * P, D1).line((0, 3 * P - 0.3), (0, 4 * P))
    kind(s, 0, 2 * P, sign)
    x = 2 * P
    winding(s.line((x, 0), (x, P)), x, P, 3 * P).line((x, 3 * P), (x, 4 * P))
    for k, (px, py) in enumerate(((0, 0), (0, 4 * P), (x, 0), (x, 4 * P))):
        s.pin(names[k], (px, py), label=(px + 0.6, 0.2 if py == 0 else 4 * P - 2.4), shown=True)
    return s.labels((x + 1.6, P + 0.4), (x + 1.6, 2 * P + 0.8))


add(series(C('Gleichstrom-Reihenschlussmotor', 'DC series motor', 'Moteur à courant continu à excitation série'), '=', ('A1', 'D2')))
add(series(C('Wechselstrom-Reihenschlussmotor', 'AC series motor (universal motor)', 'Moteur série à courant alternatif (universel)'), '~', ('U1', 'U2')))
add(shunt(C('Gleichstrom-Nebenschlussmotor', 'DC shunt motor', 'Moteur à courant continu à excitation shunt'), '=', ('A1', 'A2', 'E1', 'E2')))
add(shunt(C('Wechselstrom-Nebenschlussmotor', 'AC shunt motor', 'Moteur shunt à courant alternatif'), '~', ('U1', 'U2', 'E1', 'E2')))


# Signs: no part, no contacts
def sign(caption, draw):
    s = Symbol('', caption, numbered=False, listed=False, shown=False)
    draw(s)
    return s


def zigzag(s):
    for a in (270, 30, 150):
        p1 = (1.6 * math.cos(math.radians(a)), -1.6 * math.sin(math.radians(a)))
        p2 = (p1[0] + 1.4 * math.cos(math.radians(a + 60)), p1[1] - 1.4 * math.sin(math.radians(a + 60)))
        s.line((0, 0), p1, p2)


add(sign(C('Wicklung', 'Winding', 'Enroulement'), lambda s: coil(s, 0, 2 * P)))
add(sign(C('Wicklung (senkrecht)', 'Winding, vertical', 'Enroulement vertical'), lambda s: winding(s, 0, 0, 2 * P)))
add(sign(C('Dreieckschaltung', 'Delta connection', 'Couplage triangle'), lambda s: s.line((-2.0, 1.15), (2.0, 1.15), (0, -2.31), (-2.0, 1.15))))
add(sign(C('Wechselstrom (Zeichen)', 'Alternating current (sign)', 'Courant alternatif (signe)'),
         lambda s: s.arc(1.0, 0, 2.0, 0, 180).arc(3.0, 0, 2.0, 180, 360)))
add(sign(C('Sternschaltung', 'Star connection', 'Couplage étoile'),
         lambda s: s.line((0, 0), (0, 2.2)).line((0, 0), (1.905, -1.1)).line((0, 0), (-1.905, -1.1))))
add(sign(C('Zickzackschaltung', 'Zigzag connection', 'Couplage zigzag'), zigzag))
