# Old valves and their bases (Elektro/Elektronik/Bauteile/Alt): the pages "Roehren" and "Röhrenfassungen" get
# named valve types, a split triode system and separate heaters, and further bases. Valves after DIN EN 60617-5
# (envelope, anode bar, dashed grids, cathode, heater); electrode systems after the makers' data books (Telefunken,
# Philips, RFT). Bases seen from below, pin places and numbers after the Philips/Telefunken base drawings.
ALT = BT + '/Alt'
R = 3 * P           # envelope radius


# --- Valves: contacts named by electrode, a second system primed (a', g', k').
def valve(caption, value='', prefix='RO', envelope=True):
    s = Symbol(prefix, caption, value)
    if envelope:
        s.circle(0, 0, 2 * R, w=0.35)
    s.labels((R + 1.0, 1.0), (R + 1.0, 4.0))
    return s


def anode(s, name, x=0, y=-3.6, half=1.8):
    s.line((x - half, y), (x + half, y), w=THICK).line((x, y), (x, -R))
    s.pin(name, (x, -R), label=(x + 0.6, -R - 0.4))


def grid(s, x0, x1, y):
    s.parts.append({'type': 'line', 'points': [pt((x0, y)), pt((x1, y))], 'pen': {'width': W, 'style': 'dash'}, 'electrical': False})


def grid_lead(s, name, x0, x1, y, right=False):
    """A grid from x0 to x1 at y, its lead out to the envelope's side."""
    grid(s, x0, x1, y)
    if right:
        s.line((x1, y), (R, y)).pin(name, (R, y), label=(R - 0.4, y - 2.2), align='right')
    else:
        s.line((x0, y), (-R, y)).pin(name, (-R, y), label=(-R + 0.4, y - 2.2))


def cathode(s, name, x=-P, left=None, right=1.6):
    """Indirectly heated cathode: a bar at y 1.8 with the bent end, its lead down at x."""
    s.line((x if left is None else left, 1.8), (right, 1.8), (right + 0.4, 2.3))
    s.line((x, 1.8), (x, R)).pin(name, (x, R), label=(x - 0.4, R - 2.4), align='right')


def heater(s, x0=0, x1=P, names=('f1', 'f2')):
    s.line((x0, R), (x0, 3.8), ((x0 + x1) / 2, 3.0), (x1, 3.8), (x1, R))
    s.pin(names[0], (x0, R), label=(x0 + 0.2, R + 0.2)).pin(names[1], (x1, R), label=(x1 + 0.4, R + 0.2))


def filament(s, names=('f1', 'f2')):
    """Directly heated cathode: the filament as an inverted V between f1 and f2."""
    s.line((-P, R), (-P, 4.2), (0, 2.2), (P, 4.2), (P, R))
    s.pin(names[0], (-P, R), label=(-P - 0.4, R + 0.2), align='right').pin(names[1], (P, R), label=(P + 0.4, R + 0.2))


def two_anodes(s, names=('a', "a'"), y=-3.0):
    anode(s, names[0], -P, y, 1.2)
    anode(s, names[1], P, y, 1.2)


def triode(caption, value, direct=False):
    s = valve(caption, value)
    anode(s, 'a')
    grid_lead(s, 'g', -2.0, 2.0, 0)
    if direct:
        filament(s)
    else:
        cathode(s, 'k')
        heater(s)
    return s


add = extend(ALT, 'Roehren')
add(triode(C('REN1004', 'REN1004', 'REN1004'), 'REN1004'))
add(triode(C('REN304', 'REN304', 'REN304'), 'REN304', direct=True))
add(triode(C('AC2', 'AC2', 'AC2'), 'AC2'))

s = valve(C('RGN354', 'RGN354', 'RGN354'), 'RGN354')       # half-wave rectifier, one anode, filament
anode(s, 'a', y=-2.6)
filament(s)
add(s)

s = valve(C('AB1', 'AB1', 'AB1'), 'AB1')                   # two diodes on one cathode
two_anodes(s)
cathode(s, 'k', left=-3.8, right=3.2)
heater(s)
add(s)

s = valve(C('EYY13', 'EYY13', 'EYY13'), 'EYY13')           # two diode systems, each cathode tied to one end of its heater
two_anodes(s)
for side, n in ((-1, ''), (1, "'")):
    xk, xf = side * 2 * P, side * P
    s.line((xk, R), (xk, 1.8), (side * 0.9, 1.8), (side * 0.5, 2.3))
    s.line((xk, 3.8), (side * 1.5 * P, 3.0), (xf, 3.8), (xf, R))
    s.circle(xk, 3.8, 0.7, w=0.1, fill='#000000')
    s.pin('k' + n, (xk, R), label=(xk + side * 0.4, R + 0.2), align='left' if side > 0 else 'right')
    s.pin('f' + n, (xf, R), label=(xf - side * 0.4, R + 0.2), align='right' if side > 0 else 'left')
add(s)

s = valve(C('AZ11', 'AZ11', 'AZ11'), 'AZ11')               # full-wave rectifier, directly heated
two_anodes(s)
filament(s)
add(s)

s = valve(C('ABC1', 'ABC1', 'ABC1'), 'ABC1')               # triode and two diodes on one cathode
anode(s, 'a', y=-4.0)
grid_lead(s, 'g', -2.0, 2.0, -P)
s.line((-3.8, 0), (-2.0, 0), w=THICK).line((-3.8, 0), (-R, 0)).pin("a'", (-R, 0), label=(-R + 0.4, 0.4))
s.line((2.0, 0), (3.8, 0), w=THICK).line((3.8, 0), (R, 0)).pin("a''", (R, 0), label=(R - 0.4, 0.4), align='right')
cathode(s, 'k', left=-3.8, right=3.2)
heater(s)
add(s)

s = valve(C('RENS1204', 'RENS1204', 'RENS1204'), 'RENS1204')   # screen-grid valve
anode(s, 'a')
grid_lead(s, 'g1', -2.0, 2.0, 0)
grid(s, -2.0, 2.0, -1.27)
s.line((2.0, -1.27), (2.8, -1.27), (2.8, -P), (R, -P)).pin('g2', (R, -P), label=(R - 0.4, -P - 2.2), align='right')
cathode(s, 'k')
heater(s)
add(s)

s = valve(C('KDD1', 'KDD1', 'KDD1'), 'KDD1')               # two triodes on one filament (class B)
two_anodes(s, ('a', "a'"))
grid_lead(s, 'g', -3.8, -1.0, 0)
grid_lead(s, "g'", 1.0, 3.8, 0, right=True)
filament(s)
add(s)

# One system of a double triode drawn apart from the other: the envelope open towards it, the heater drawn separately.
s = valve(C('Triode (zweites System)', 'Triode (second system)', 'Triode (deuxième système)'), prefix='V', envelope=False)
s.arc(0, 0, 2 * R, 45, 315, w=0.35)
anode(s, 'a')
grid_lead(s, 'g', -2.0, 2.0, 0)
cathode(s, 'k')
add(s)

# Heaters drawn apart from their systems, the contacts showing the base pin numbers.
s = Symbol('', C('Heizfaden', 'Heater', 'Filament de chauffage'), numbered=False, listed=False, shown=False)
zig = [(P + k * P / 3, 0 if k in (0, 6) else (-1.0 if k % 2 else 1.0)) for k in range(7)]
s.line((0, 0), (P, 0)).line(*zig).line((3 * P, 0), (4 * P, 0))
s.pin('1', (0, 0), text='4', shown=True).pin('2', (4 * P, 0), label=(4 * P - 0.4, -2.2), align='right', text='5', shown=True)
add(s.labels((2 * P, -5.6), None, 'centre'))


def roof(caption, tap):
    s = Symbol('V', caption)
    s.line((0, 2 * P), (0, P), (P, 0), (2 * P, P), (2 * P, 2 * P))
    s.pin('1', (0, 2 * P), label=(-0.4, 2 * P - 2.4), align='right', text='4', shown=True)
    if tap:
        s.line((P, 0), (P, -P)).pin('2', (P, -P), label=(P + 0.5, -P - 0.2), text='9', shown=True)
    s.pin('3' if tap else '2', (2 * P, 2 * P), label=(2 * P + 0.4, 2 * P - 2.4), text='5', shown=True)
    return s.labels((2 * P + 2.0, -1.4), (2 * P + 2.0, 1.6))


add(roof(C('Heizfaden mit Mittelanzapfung', 'Heater with centre tap', 'Filament de chauffage à prise médiane'), True))
add(roof(C('Heizfaden (Dach)', 'Heater (inverted V)', 'Filament de chauffage (en V inversé)'), False))


# --- Bases seen from below. Each pin leads out over the rim to a point of the 2.54 mm pitch.
OUT = 6 * P         # rim radius


def at(angle, radius):
    """The point at `angle` degrees clockwise from the top."""
    a = math.radians(angle)
    return (radius * math.sin(a), -radius * math.cos(a))


def base(caption, pins, size=2.0, tab=False):
    """`pins`: (number, place, lead direction in degrees clockwise from the top or None for outwards). Holes are
    circles of `size`; side contacts (`tab`) are tongues on the rim."""
    s = Symbol('', caption, numbered=False)
    s.circle(0, 0, 2 * OUT, w=0.35)
    grid_pt = lambda v: round(v / P) * P
    ends = set()
    for name, place, direction in pins:
        x, y = place
        if direction is None:
            direction = math.degrees(math.atan2(x, -y))
        dx, dy = at(direction, 1)
        end = (grid_pt((OUT + P) * dx), grid_pt((OUT + P) * dy))
        assert end not in ends, (caption, name)
        ends.add(end)
        ux, uy = end[0] - x, end[1] - y
        n = math.hypot(ux, uy)
        if tab:
            a = math.radians(direction)
            c, sn = math.cos(a), math.sin(a)
            ring = lambda r, w: (r * sn + w * c, -r * c + w * sn)
            s.poly(ring(OUT - 2.8, -0.6), ring(OUT + 0.5, -0.9), ring(OUT + 0.5, 0.9), ring(OUT - 2.8, 0.6), fill='#ffffff')
            s.circle(*at(direction, OUT - 0.7), 0.6, w=0.15)
            start = at(direction, OUT + 0.5)
        else:
            s.circle(x, y, size, w=W)
            start = (x + ux / n * size / 2, y + uy / n * size / 2)
        s.line(start, end)
        s.pin(name, end, label=(end[0] + 1.6 * dx, end[1] + 1.6 * dy - 0.9), align='centre', shown=True)
    s.labels((OUT + 2.0, -OUT - 1.0), (OUT + 2.0, -OUT + 2.0))
    return s


def spigot(s, d, nose_down=True, fill=None):
    """A centre spigot of diameter d with its key at the bottom: a nose (steel valves) or a groove (Loctal)."""
    s.circle(0, 0, d, w=W, fill=fill)
    if nose_down:
        s.rect(-0.7, d / 2 - 0.1, 0.7, d / 2 + 1.1, w=W, fill='#000000')
    else:
        s.line((-0.7, d / 2), (-0.7, d / 2 - 1.2), (0.7, d / 2 - 1.2), (0.7, d / 2))


add = extend(ALT, 'Röhrenfassungen')
# European pin bases (Telefunken "Sockel 5 IV", Philips base O): grid and anode pin 8 mm from the centre, the heater
# pins 8 mm to either side on a line 2 mm off the centre towards pin 4; drawn 1.27 times enlarged.
kite = [('1', (-4 * P, 0), None), ('2', (P, 4 * P), None), ('3', (P, -4 * P), None), ('4', (4 * P, 0), None)]
add(base(C('Europa-Sockel 5-polig', 'European 5-pin base', 'Culot européen à 5 broches'), kite + [('5', (0, 0), 225)], size=2.6))
add(base(C('Europa-Sockel 4-polig', 'European 4-pin base', 'Culot européen à 4 broches'), kite, size=2.6))
add(base(C('Europa-Sockel 7-polig (Hexoden)', 'European 7-pin base (hexodes)', 'Culot européen à 7 broches (hexodes)'),
         [(str(n), at(a, 4 * P), None) for n, a in ((1, 240), (2, 200), (3, 160), (4, 120), (5, 60), (6, 0), (7, 300))], size=2.6))

s = Symbol('', C('Anodenkappe', 'Anode top cap', "Capuchon d'anode"), numbered=False)
s.arc(0, 5 * P, 2 * R, 45, 135, w=0.35)
s.rect(-1.2, 2 * P - 1.5, 1.2, 2 * P + 0.1, w=W, corner=30)
s.line((0, 0), (0, 2 * P - 1.5)).pin('1', (0, 0))
add(s.labels((2.0, -1.4), (2.0, 1.6)))

s = Symbol('', C('Edison-Sockel', 'Edison screw base', 'Culot à vis Edison'), numbered=False)
s.circle(0, 0, 8 * P, w=0.35, fill='#d0d0d0').circle(0, 0, 8 * P - 2.0, w=0.18)
s.circle(0, 0, 4.6 * P, w=0.25, fill='#303030').circle(0, 0, 1.8 * P, w=0.25, fill='#d0d0d0')
s.line((0, 0.9 * P), (0, 6 * P)).pin('1', (0, 6 * P), label=(0, 6 * P + 0.7), align='centre', shown=True)
s.line((4 * P, 0), (6 * P, 0)).pin('2', (6 * P, 0), label=(6 * P + 1.4, -0.9), align='centre', shown=True)
add(s.labels((4 * P + 1.0, -4 * P - 1.0), (4 * P + 1.0, -4 * P + 2.0)))

# Side-contact base P8A after the Philips drawings: contacts 1 to 4 at 30 degrees in the top, numbered anticlockwise.
add(base(C('Außenkontaktsockel 8-polig', 'Side-contact base, 8 contacts (P8A)', 'Culot à contacts latéraux, 8 contacts (P8A)'),
         [(str(n + 1), (0, 0), a) for n, a in enumerate((45, 15, -15, -45, -99, -153, -207, -261))], tab=True))

# German steel valve base Y8A: pins 26°50' apart in groups of five and three, the guide spigot's nose towards pin 3.
s = base(C('Stahlröhrensockel', 'Steel valve base (Y8A)', 'Culot de tube en acier (Y8A)'),
         [(str(n + 1), at(a, 4 * P), None) for n, a in enumerate((126.33, 153.17, 180, 206.83, 233.67, 333.17, 0, 26.83))])
spigot(s, 3 * P)
add(s)

s = base(C('Loktal-Fassung (B8G)', 'Loctal socket (B8G)', 'Support loctal (B8G)'),
         [(str(n + 1), at(202.5 + 45 * n, 4 * P), None) for n in range(8)], size=1.4)
s.circle(0, 0, 2 * OUT - 2.0, w=W)
spigot(s, 3 * P, nose_down=False, fill='#c8c8c8')
add(s)

# Gnom base of the RFT miniature valves (B11G, the series version): ten pins 32.7 degrees apart, pin 1 inside the gap.
s = base(C('Gnom-Sockel', 'Gnom base (B11G)', 'Culot Gnom (B11G)'),
         [('1', (0, 2.4 * P), None)] + [(str(n + 2), at(212.85 + 32.7 * n, 4 * P), None) for n in range(10)], size=1.4)
s.circle(0, 0, 2 * OUT - 2.0, w=W)
add(s)
