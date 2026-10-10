# Transformers, continued: the ferrite core, the block form (windings as filled rectangles), the helix form (windings
# as loops), the circle form of single-line diagrams and a PCB transformer seen from above. After DIN EN 60617-4/-6
# (06-09 transformers, 06-13-04 current transformer, form 1).
add = extend(BT, 'Transformatoren')
GAP = 1.3       # between the heavy dashes of a ferrite core: 0.8 mm left visible between the round ends


def ferrite(s, x, y0, y1):
    """A ferrite core line drawn as short heavy dashes (the pen's dash pattern shows a single dash on so short a line)."""
    n = max(2, round((y1 - y0) / 2.1))
    dash = (y1 - y0 - (n - 1) * GAP) / n
    for k in range(n):
        y = y0 + k * (dash + GAP)
        s.line((x, y), (x, y + dash), w=THICK)
    return s


def core(s, kind, y0=0.6 * P, y1=3.4 * P):
    """The core between the windings: 'iron' two heavy lines, 'ferrite' two dashed ones, None nothing."""
    for x in (2 * P - 0.6, 2 * P + 0.6):
        if kind == 'iron':
            s.line((x, y0), (x, y1), w=THICK)
        elif kind == 'ferrite':
            ferrite(s, x, y0, y1)
    return s


def winding_pins(s, x, y0, y1, pin_x, names):
    """Leads from a vertical winding at x (y0..y1) to two pins at pin_x, on the pitch just outside the winding."""
    ya, yb = math.floor(y0 / P - 1e-9) * P, math.ceil(y1 / P + 1e-9) * P
    s.line((pin_x, ya), (x, ya), (x, y0)).line((x, y1), (x, yb), (pin_x, yb))
    right = pin_x > x
    for name, y in zip(names, (ya, yb)):
        if right:
            s.pin(name, (pin_x, y), label=(pin_x - 0.4, y - 2.2), align='right')
        else:
            s.pin(name, (pin_x, y), label=(pin_x + 0.4, y - 2.2))
    return s


def block(s, x, y0, y1):
    s.rect(x - 1.0, y0, x + 1.0, y1, fill='#000000')
    return s


def helix(s, x, y0, y1, side, turns=4, half=0.9, back=0.5):
    """A winding drawn as loops (helix form) along x from y0 to y1: wide arcs towards `side` (+1 right, -1 left), small
    loops behind. A prolate trochoid, as a chain of cubic curves (a quarter of a turn each)."""
    t0, t1 = math.pi / 2, (2 * turns - 0.5) * math.pi
    a = (y1 - y0 - 2 * back) / (t1 - t0)
    at = lambda t: (x - side * half * math.cos(t), y0 + a * (t - t0) - back * (math.sin(t) - 1))
    slope = lambda t: (side * half * math.sin(t), a - back * math.cos(t))
    step = math.pi / 4
    points = [at(t0)]
    for k in range(int(round((t1 - t0) / step))):
        ta, tb = t0 + k * step, t0 + (k + 1) * step
        pa, pb, da, db = at(ta), at(tb), slope(ta), slope(tb)
        points += [(pa[0] + da[0] * step / 3, pa[1] + da[1] * step / 3), (pb[0] - db[0] * step / 3, pb[1] - db[1] * step / 3), pb]
    return s.bezier(*points)


def two_windings(caption, draw, kind):
    """A transformer like the one above in another form: primary 1-2 at the left, secondary 3-4 at the right."""
    s = Symbol('T', caption)
    winding_pins(s, P, P, 3 * P, 0, ('1', '2'))
    winding_pins(s, 3 * P, P, 3 * P, 4 * P, ('3', '4'))
    draw(s, P, P, 3 * P, 1)
    draw(s, 3 * P, P, 3 * P, -1)
    core(s, kind)
    return s.labels((4 * P + 0.8, P), (4 * P + 0.8, 2 * P + 0.4))


def three_blocks(caption, kind):
    """Block form with one primary (1-2) and two separate secondaries (3-4, 5-6)."""
    s = Symbol('T', caption)
    winding_pins(s, P, 0.5 * P, 6.5 * P, 0, ('1', '2'))
    winding_pins(s, 3 * P, 0.5 * P, 2.5 * P, 4 * P, ('3', '4'))
    winding_pins(s, 3 * P, 4.5 * P, 6.5 * P, 4 * P, ('5', '6'))
    block(s, P, 0.5 * P, 6.5 * P)
    block(s, 3 * P, 0.5 * P, 2.5 * P)
    block(s, 3 * P, 4.5 * P, 6.5 * P)
    core(s, kind, 0.1 * P, 6.9 * P)
    return s.labels((4 * P + 0.8, P), (4 * P + 0.8, 2 * P + 0.4))


s = transformer(Symbol('T', C('Transformator mit Ferritkern', 'Transformer with ferrite core', 'Transformateur à noyau de ferrite')), core=False)
add(core(s, 'ferrite'))
blocks = lambda s, x, y0, y1, side: block(s, x, y0, y1)
for kind, de, en, fr in ((None, 'ohne Kern', 'air core', 'sans noyau'), ('ferrite', 'mit Ferritkern', 'with ferrite core', 'à noyau de ferrite'),
                         ('iron', 'mit Kern', 'with magnetic core', 'à noyau magnétique')):
    add(two_windings(C('Transformator (Blockdarstellung) ' + de, 'Transformer (block form), ' + en, 'Transformateur (forme bloc) ' + fr), blocks, kind))
for kind, de, en, fr in ((None, 'ohne Kern', 'air core', 'sans noyau'), ('ferrite', 'mit Ferritkern', 'with ferrite core', 'à noyau de ferrite'),
                         ('iron', 'mit Kern', 'with magnetic core', 'à noyau magnétique')):
    add(three_blocks(C('Transformator mit zwei Sekundärwicklungen (Blockdarstellung) ' + de, 'Transformer with two secondary windings (block form), ' + en,
                       'Transformateur à deux enroulements secondaires (forme bloc) ' + fr), kind))
for kind, de, en, fr in ((None, 'ohne Kern', 'air core', 'sans noyau'), ('ferrite', 'mit Ferritkern', 'with ferrite core', 'à noyau de ferrite'),
                         ('iron', 'mit Kern', 'with magnetic core', 'à noyau magnétique')):
    add(two_windings(C('Transformator (Wendeldarstellung) ' + de, 'Transformer (helix form), ' + en, 'Transformateur (forme hélicoïdale) ' + fr), helix, kind))

# The circle form (form 1 of IEC 60617-6): every winding a circle, the circles of coupled windings overlapping.
D = 5.6         # circle diameter
MID = 2.5 * P   # centre of the two-circle forms between the pins at 0 and 5 P


def two_circles(caption):
    s = Symbol('T', caption)
    s.circle(0, MID - 1.7, D).circle(0, MID + 1.7, D)
    s.pin('1', (0, 0), label=(0.4, -2.2)).pin('2', (0, 5 * P), label=(0.4, 5 * P - 2.2))
    return s.labels((D / 2 + 1.0, MID - 4.2), (D / 2 + 1.0, MID - 1.2))


s = two_circles(C('Transformator (Kreisdarstellung)', 'Transformer (circle form)', 'Transformateur (forme à cercles)'))
add(s.line((0, 0), (0, MID - 1.7 - D / 2)).line((0, MID + 1.7 + D / 2), (0, 5 * P)))
s = two_circles(C('Stelltransformator (Kreisdarstellung)', 'Adjustable transformer (circle form)', 'Transformateur réglable (forme à cercles)'))
s.line((0, 0), (0, MID - 1.7 - D / 2)).line((0, MID + 1.7 + D / 2), (0, 5 * P))
add(s.arrow((-3.6, MID + 3.6), (3.8, MID - 3.8), size=1.2).labels((D / 2 + 1.6, MID - 1.4), (D / 2 + 1.6, MID + 1.6)))
s = two_circles(C('Stromwandler mit zwei Sekundärwicklungen auf einem Kern', 'Current transformer with two secondary windings on one core',
                  'Transformateur de courant à deux enroulements secondaires sur un même noyau'))
add(s.line((0, 0), (0, 5 * P)))
s = Symbol('T', C('Dreiwicklungstransformator (Kreisdarstellung)', 'Three-winding transformer (circle form)', 'Transformateur à trois enroulements (forme à cercles)'))
top, low, side = 4.6, 4.6 + 1.8 * math.sqrt(3), 1.8
s.circle(0, top, D).circle(-side, low, D).circle(side, low, D).line((0, 0), (0, top - D / 2))
foot = low + math.sqrt((D / 2) ** 2 - (P - side) ** 2)
s.line((-P, foot), (-P, 5 * P)).line((P, foot), (P, 5 * P))
s.pin('1', (0, 0), label=(0.4, -2.2)).pin('2', (-P, 5 * P), label=(-P - 0.4, 5 * P - 2.2), align='right').pin('3', (P, 5 * P), label=(P + 0.4, 5 * P - 2.2))
add(s.labels((side + D / 2 + 1.0, top - 2.6), (side + D / 2 + 1.0, top + 0.4)))

# A small transformer for PCB mounting seen from above: the lamination stack grey, the bobbin around it.
s = Symbol('T', C('Printtransformator (Draufsicht)', 'PCB transformer (top view)', 'Transformateur pour circuit imprimé (vue de dessus)'))
s.rect(-14.0, -11.5, 14.0, 11.5, w=0.35, corner=10).rect(-13.0, -5.0, 13.0, 5.0, w=0.18, fill='#a0a0a0')
add(s.labels((15.0, -11.5), (15.0, -8.5)))
