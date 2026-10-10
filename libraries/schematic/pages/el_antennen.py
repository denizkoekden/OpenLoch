# More antennas for the page "Antennen", after DIN EN 60617-10 (the general antenna with qualifying signs for tuning,
# direction and rotation; dipoles, folded dipole, Yagi, rhombic, horn, reflector and slot antennas, balun), and the parts
# of an aerial distribution for cable TV and satellite after DIN EN 60617-11 and the usual German "BK" signs: splitters,
# taps, outlets, terminating resistors, amplifier. In the open signs the signal runs upwards (input below), in the boxes
# and at the outlets downwards (input above). At the end pictograms of radio waves, a mast and a dish (no parts).
add = extend(EL, 'Antennen')


def antenna(s, foot=0, top=-3 * P):
    """The general antenna sign of the page: the stem from (0, foot) up to (0, top), the fork above it."""
    return s.line((0, foot), (0, top)).line((-2.0, top - P), (0, top), (2.0, top - P)).line((0, top), (0, top - P))


def variable(s, x, y):
    """The variability arrow across a lead at (x, y)."""
    return s.arrow((x - 2.0, y + 2.0), (x + 2.2, y - 2.2), size=1.0)


def counterpoise(s):
    return s.line((0, -P), (2 * P, -P), (2 * P, 0)).line((2 * P - 1.6, 0), (2 * P + 1.6, 0), w=0.35)


def turn_arrow(s, cx, cy, a, b=None, start=60, stop=360):
    """A circular (with b: elliptic) arrow running counter-clockwise from start to stop, its head at stop."""
    b = a if b is None else b
    s.arc(cx, cy, 2 * a, start, stop, dy=2 * b)
    t = math.radians(stop)
    end = (cx + a * math.cos(t), cy - b * math.sin(t))
    dx, dy = -a * math.sin(t), -b * math.cos(t)
    n = math.hypot(dx, dy)
    return triangle(s, (end[0] + 1.1 * dx / n, end[1] + 1.1 * dy / n), end, 0.55)


def folded(s, y, half=3 * P, h=1.6):
    """A folded dipole from -half to half: a narrow loop whose lower side (at y) opens between the feeders at -P and P."""
    s.line((-half, y - h), (half, y - h)).line((-half, y), (-P, y)).line((P, y), (half, y))
    return s.arc(-half, y - h / 2, h, 90, 270).arc(half, y - h / 2, h, 270, 450)


def balun(s, y):
    """The balun as a half-wave loop: the balanced terminals at (-P, y) and (P, y) joined by a loop hanging below them;
    the unbalanced line leaves the left terminal and ends at (-2 P, y + 3 P)."""
    s.line((-P, y), (-P, y + 1.5 * P)).arc(0, y + 1.5 * P, 2 * P, 180, 360).line((P, y + 1.5 * P), (P, y))
    return s.line((-P, y), (-2 * P, y), (-2 * P, y + 3 * P)).circle(-P, y, 0.8, fill='#000000')


s = antenna(Symbol('T', C('Antenne, drehbar', 'Antenna, rotatable', 'Antenne rotative')))
turn_arrow(s, -4.2, -3.5 * P, 1.4)
add(s.pin('1', (0, 0)).labels((2.4, -3 * P), (2.4, -2 * P + 0.4)))
s = counterpoise(antenna(Symbol('T', C('Antenne mit Gegengewicht, abstimmbar', 'Antenna with counterpoise, tunable',
                                       'Antenne avec contrepoids, accordable'))))
variable(s, 0, -2 * P + 0.4)
add(s.pin('1', (0, 0)).labels((3.4, -4 * P), (3.4, -4 * P + 3.0)))
s = counterpoise(antenna(Symbol('T', C('Antenne mit Gegengewicht, Richtantenne', 'Antenna with counterpoise, directional',
                                       'Antenne avec contrepoids, directive'))))
s.arrow((1.0, -2 * P), (5.0, -2 * P), size=1.2)
add(s.pin('1', (0, 0)).labels((2.4, -3 * P - 1.0), (2.4, -2 * P - 0.6)))
s = antenna(Symbol('T', C('Antenne, abstimmbar', 'Antenna, tunable', 'Antenne accordable')))
variable(s, 0, -1.5 * P)
add(s.pin('1', (0, 0)).labels((2.8, -4 * P), (2.8, -4 * P + 3.0)))
s = antenna(Symbol('T', C('Antenne mit Erdnetz', 'Ground-plane antenna', 'Antenne à plan de sol')))
for x, y in ((-4.6, -1.6), (-2.2, -0.8), (2.2, -0.8), (4.6, -1.6)):
    s.line((0, -1.5 * P), (x, y))
add(s.pin('1', (0, 0)).labels((2.4, -3 * P), (2.4, -2 * P + 0.4)))
s = antenna(Symbol('T', C('Peilantenne', 'Direction-finding antenna', 'Antenne radiogoniométrique')))
s.arrow((-3.4, -P), (-3.4, -3.5 * P), size=1.1)
s.circle(4.2, -1.6 * P, 3.4).text('H', 4.2, -1.6 * P - 1.0, 2.0, 'centre')
add(s.pin('1', (0, 0)).labels((2.4, -4 * P), (2.4, -4 * P + 3.0)))
s = antenna(Symbol('T', C('Antenne mit Ankopplungsspule', 'Antenna with coupling coil', 'Antenne avec bobine de couplage')), -P, -4 * P)
coil(s, 0, 2 * P, y=-P)
s.line((0, -P), (0, 0)).line((2 * P, -P), (2 * P, 0))
add(s.pin('1', (0, 0)).pin('2', (2 * P, 0), label=(2 * P + 0.4, -2.2)).labels((2.4, -4 * P), (2.4, -4 * P + 3.0)))

s = Symbol('T', C('Dipol (gespreizt)', 'Dipole (spread)', 'Dipôle (écarté)'))
s.line((-2 * P, 0), (-2 * P, -2 * P), (-4 * P, -2 * P)).line((2 * P, 0), (2 * P, -2 * P), (4 * P, -2 * P))
add(s.pin('1', (-2 * P, 0)).pin('2', (2 * P, 0), label=(2 * P + 0.4, -2.2)).labels((-P, -2 * P - 1.0), (-P, -2 * P + 2.0)))
s = folded(Symbol('T', C('Faltdipol', 'Folded dipole', 'Dipôle replié')), -2 * P)
s.line((-P, -2 * P), (-P, 0)).line((P, -2 * P), (P, 0))
add(s.pin('1', (-P, 0)).pin('2', (P, 0), label=(P + 0.4, -2.2)).labels((3 * P + 1.6, -2 * P - 2.0), (3 * P + 1.6, -2 * P + 1.0)))
s = folded(Symbol('T', C('Yagi-Antenne', 'Yagi antenna', 'Antenne Yagi')), -2 * P)
s.line((-P, -2 * P), (-P, 0)).line((P, -2 * P), (P, 0)).line((-3.4 * P, -P), (3.4 * P, -P), w=0.35)
for k, half in enumerate((2.6 * P, 2.4 * P, 2.2 * P)):
    y = -2 * P - 1.6 - 1.8 * (k + 1)
    s.line((-half, y), (half, y), w=0.35)
add(s.pin('1', (-P, 0)).pin('2', (P, 0), label=(P + 0.4, -2.2)).labels((3 * P + 1.6, -4 * P - 1.0), (3 * P + 1.6, -4 * P + 2.0)))
s = Symbol('T', C('Rhombusantenne', 'Rhombic antenna', 'Antenne en losange'))
for side in (-1, 1):
    s.line((side * P, 0), (side * P, -P), (side * 3 * P, -3 * P), (side * P, -5 * P), (side * 1.6, -5 * P))
s.rect(-1.6, -5 * P - 0.8, 1.6, -5 * P + 0.8)
add(s.pin('1', (-P, 0)).pin('2', (P, 0), label=(P + 0.4, -2.2)).labels((3 * P + 0.6, -5 * P), (3 * P + 0.6, -4 * P + 0.4)))

s = Symbol('T', C('Hornstrahler', 'Horn antenna', 'Antenne cornet'))
s.line((0, 0), (0, -P)).rect(-0.9, -2 * P, 0.9, -P).line((-0.9, -2 * P), (-3.4, -4 * P)).line((0.9, -2 * P), (3.4, -4 * P))
add(s.pin('1', (0, 0)).labels((3.6, -2 * P), (3.6, -P + 0.4)))
s = antenna(Symbol('T', C('Radarantenne', 'Radar antenna', 'Antenne radar')))
turn_arrow(s, 0, -2 * P, 2.6, 0.9, start=300, stop=590)
s.line((-4.6, -1.5 * P), (-4.6, -3.5 * P), start='arrow', end='arrow', size=1.0)
add(s.pin('1', (0, 0)).labels((2.4, -4 * P), (2.4, -4 * P + 3.0)))
s = Symbol('T', C('Parabolantenne', 'Parabolic antenna', 'Antenne parabolique'))
s.arc(0, -P - 7.0, 14.0, 220, 320).line((0, 0), (0, -5.0))
triangle(s, (0, -5.0), (0, -6.4), 0.9)
add(s.pin('1', (0, 0)).labels((5.8, -3 * P), (5.8, -2 * P + 0.4)))
s = Symbol('T', C('Hornparabolantenne', 'Horn-reflector antenna', 'Antenne cornet-réflecteur'))
s.line((0, 0), (0, -P)).line((-0.8, -P), (0.8, -P)).line((0.8, -P), (2.4, -3 * P)).line((-0.8, -P), (-2.4, -3 * P), (-2.4, -3.6 * P))
s.arc(2.4, -3.6 * P, 9.6, 90, 180, dy=6.0)
add(s.pin('1', (0, 0)).labels((3.6, -2 * P), (3.6, -P + 0.4)))

s = Symbol('T', C('Symmetrierglied', 'Balun', 'Symétriseur'))
balun(s, -3 * P).line((-P, -3 * P), (-P, -4 * P)).line((P, -3 * P), (P, -4 * P))
s.pin('1', (-2 * P, 0)).pin('2', (-P, -4 * P), label=(-P - 0.4, -4 * P - 0.4), align='right').pin('3', (P, -4 * P), label=(P + 0.4, -4 * P - 0.4))
add(s.labels((P + 1.6, -2 * P), (P + 1.6, -P + 0.4)))
s = folded(Symbol('T', C('Faltdipol mit Symmetrierglied', 'Folded dipole with balun', 'Dipôle replié avec symétriseur')), -5 * P)
balun(s, -3 * P).line((-P, -3 * P), (-P, -5 * P)).line((P, -3 * P), (P, -5 * P))
add(s.pin('1', (-2 * P, 0)).labels((P + 1.6, -2 * P), (P + 1.6, -P + 0.4)))
s = Symbol('T', C('Schlitzantenne', 'Slot antenna', 'Antenne à fente'))
s.line((0, 0), (0, -P)).rect(-2 * P, -2.6 * P, 2 * P, -P).rect(-1.2 * P, -2.1 * P, 1.2 * P, -1.8 * P, fill='#000000')
add(s.pin('1', (0, 0)).labels((2 * P + 0.8, -2.6 * P), (2 * P + 0.8, -2.6 * P + 3.0)))
s = Symbol('', C('Erdungsschiene', 'Earthing bar', 'Barrette de terre'), numbered=False, shown=False)
s.line((0, 0), (0, P - 0.8)).rect(-3 * P, P - 0.8, 3 * P, P + 0.8, w=0.35)
for x in (-2.4 * P, -1.2 * P, 1.2 * P, 2.4 * P):
    s.circle(x, P, 1.0)
s.line((0, P + 0.8), (0, 2 * P + 0.6))
for k, half in enumerate((2.0, 1.3, 0.6)):
    s.line((-half, 2 * P + 0.6 + k * 0.8), (half, 2 * P + 0.6 + k * 0.8), w=0.35)
add(s.pin('1', (0, 0)).labels((3 * P + 0.8, P - 1.2), None))


# Aerial distribution
def node_splitter(caption, xs):
    """A splitter: the input from below into a small circle, the outputs spreading upwards to (x, -3 P)."""
    s = Symbol('X', caption)
    s.line((0, 0), (0, -P + 0.8)).circle(0, -P, 1.6).pin('1', (0, 0))
    for k, x in enumerate(xs):
        n = math.hypot(x, 2 * P)
        s.line((0.8 * x / n, -P - 0.8 * 2 * P / n), (x, -3 * P)).pin(str(k + 2), (x, -3 * P), label=(x + 0.4, -3 * P - 0.4))
    return s.labels((max(xs) + 1.0, -2 * P), (max(xs) + 1.0, -P + 0.4))


def tap(s, y, side, end):
    """A tap output: the line from the through line (x = 0) to x = side * end, a triangle on it pointing outwards."""
    s.line((0, y), (side * 0.8, y)).poly((side * 0.8, y - 1.0), (side * 0.8, y + 1.0), (side * 2.6, y), fill=None)
    return s.line((side * 2.6, y), (side * end, y))


add(node_splitter(C('Verteiler 2-fach', 'Splitter, 2-way', 'Répartiteur 2 voies'), (-2 * P, 2 * P)))
add(node_splitter(C('Verteiler 4-fach', 'Splitter, 4-way', 'Répartiteur 4 voies'), (-3 * P, -P, P, 3 * P)))
for n, de, en, fr in ((1, 'Abzweiger 1-fach', 'Tap, 1 output', 'Dérivateur 1 sortie'), (2, 'Abzweiger 2-fach', 'Tap, 2 outputs', 'Dérivateur 2 sorties')):
    s = Symbol('X', C(de, en, fr))
    s.line((0, 0), (0, -4 * P)).pin('1', (0, 0)).pin('2', (0, -4 * P), label=(0.4, -4 * P - 0.4))
    for k, side in enumerate((1, -1)[:n]):
        tap(s, -2 * P, side, 2 * P)
        s.pin(str(k + 3), (side * 2 * P, -2 * P), label=(side * (2 * P + 0.4), -2 * P - 2.2), align='left' if side > 0 else 'right')
    add(s.labels((2.4, -4 * P), (2.4, -4 * P + 3.0)))
s = Symbol('X', C('Abschlusswiderstand', 'Terminating resistor', 'Résistance de terminaison'), '75 Ω')
s.line((0, 0), (0, P), *[((0.9 if k % 2 else -0.9), P + (k + 0.5) * P / 3) for k in range(6)], (0, 3 * P))
s.line((-1.2, 3 * P), (1.2, 3 * P), w=0.35)
add(s.pin('1', (0, 0)).labels((2.4, P - 0.4), (2.4, 2 * P + 0.4)))


def bracket(s, y):
    """An aerial outlet in the socket form: a lead from the line at x = 0 to a bracket open to the right."""
    return s.line((0, y), (P, y)).line((P + 1.6, y - 1.8), (P, y - 1.8), (P, y + 1.8), (P + 1.6, y + 1.8), w=0.35)


s = bracket(Symbol('X', C('Antennensteckdose (Stich)', 'Aerial outlet (spur)', 'Prise d’antenne (terminale)')), 2 * P)
add(s.line((0, 0), (0, 2 * P)).pin('1', (0, 0)).labels((P + 2.8, 2 * P - 2.0), (P + 2.8, 2 * P + 1.0)))
s = bracket(Symbol('X', C('Antennensteckdose (Durchgang)', 'Aerial outlet (loop-through)', 'Prise d’antenne (passante)')), 2 * P)
s.line((0, 0), (0, 4 * P)).circle(0, 2 * P, 0.8, fill='#000000')
add(s.pin('1', (0, 0)).pin('2', (0, 4 * P), label=(0.4, 4 * P - 2.2)).labels((P + 2.8, 2 * P - 2.0), (P + 2.8, 2 * P + 1.0)))


def box_x(caption, half, outputs, height=2 * P):
    """An aerial distribution part as a box from -half to half below the input (0, 0): `outputs` the ends of the
    outputs, each with a lead from the box side it leaves."""
    s = Symbol('X', caption)
    s.rect(-half, P, half, P + height, w=0.35).line((0, 0), (0, P)).pin('1', (0, 0))
    for k, (x, y) in enumerate(outputs):
        if y > P + height:
            s.line((x, P + height), (x, y))
        else:
            s.line((half if x > 0 else -half, y), (x, y))
        s.pin(str(k + 2), (x, y), label=(x + 0.4, y - 2.2) if x >= 0 else (x - 0.4, y - 2.2), align='left' if x >= 0 else 'right')
    return s.labels((half + 1.0, -1.0), (half + 1.0, 2.0))


def box_splitter(caption, half, outputs):
    """A splitter as a box: inside, a small circle as the node with lines from the input to the outputs."""
    s = box_x(caption, half, outputs)
    node = (0, 2 * P)
    s.line((0, P), (0, 2 * P - 0.6)).circle(*node, 1.2)
    for x, y in outputs:
        edge = (x, 3 * P) if y > 3 * P else (half if x > 0 else -half, y)
        dx, dy = edge[0] - node[0], edge[1] - node[1]
        n = math.hypot(dx, dy)
        s.line((node[0] + 0.6 * dx / n, node[1] + 0.6 * dy / n), edge)
    return s


add(box_splitter(C('Verteiler 2-fach (Kasten)', 'Splitter, 2-way (box)', 'Répartiteur 2 voies (boîtier)'), 2 * P, [(-P, 4 * P), (P, 4 * P)]))
add(box_splitter(C('Verteiler 2-fach (Kasten, seitlich)', 'Splitter, 2-way (box, outputs at the sides)', 'Répartiteur 2 voies (boîtier, sorties latérales)'),
                 2 * P, [(-3 * P, 2 * P), (3 * P, 2 * P)]))
add(box_splitter(C('Verteiler 4-fach (Kasten)', 'Splitter, 4-way (box)', 'Répartiteur 4 voies (boîtier)'), 2 * P,
                 [(-3 * P, 2 * P), (3 * P, 2 * P), (-P, 4 * P), (P, 4 * P)]))
add(box_splitter(C('Verteiler 4-fach (Kasten, unten)', 'Splitter, 4-way (box, outputs below)', 'Répartiteur 4 voies (boîtier, sorties en bas)'), 3.6 * P,
                 [(-3 * P, 4 * P), (-P, 4 * P), (P, 4 * P), (3 * P, 4 * P)]))
for de, en, fr, ys, sides in (('Abzweiger 1-fach (Kasten)', 'Tap, 1 output (box)', 'Dérivateur 1 sortie (boîtier)', (2 * P,), (1,)),
                              ('Abzweiger 2-fach (Kasten)', 'Tap, 2 outputs (box)', 'Dérivateur 2 sorties (boîtier)', (2 * P, 2 * P), (1, -1)),
                              ('Abzweiger 2-fach (Kasten, rechts)', 'Tap, 2 outputs (box, both right)', 'Dérivateur 2 sorties (boîtier, à droite)', (2 * P, 3 * P), (1, 1))):
    height = max(ys)
    s = box_x(C(de, en, fr), 2 * P, [(0, P + height + P)] + [(side * 3 * P, y) for y, side in zip(ys, sides)], height)
    s.line((0, P), (0, P + height))
    for y, side in zip(ys, sides):
        tap(s, y, side, 2 * P)
    add(s)
s = Symbol('T', C('Verstärker (Antennenanlage)', 'Amplifier (aerial system)', 'Amplificateur (installation d’antenne)'))
s.rect(P, -1.5 * P, 4 * P, 1.5 * P, w=0.35).poly((P + 1.4, -P), (P + 1.4, P), (4 * P - 1.4, 0), fill=None)
s.line((0, 0), (P, 0)).line((4 * P, 0), (5 * P, 0))
add(s.pin('1', (0, 0)).pin('2', (5 * P, 0), label=(5 * P - 0.4, -2.2), align='right').labels((P, -1.5 * P - 3.4), (P, 1.5 * P + 0.6)))
s = Symbol('X', C('Abschlusswiderstand (Kasten)', 'Terminating resistor (box)', 'Résistance de terminaison (boîtier)'), '75 Ω')
s.line((0, 0), (0, P)).rect(-1.0, P, 1.0, 3 * P)
add(s.pin('1', (0, 0)).labels((2.0, P - 0.4), (2.0, 2 * P + 0.4)))


def outlet(caption, through, openings=False):
    """An aerial outlet in the circle form, the line from the input above; a loop-through outlet passes it on below,
    an end outlet ends it on a bar under the circle. With openings: a filled and an open socket (radio and TV) inside."""
    s = Symbol('X', caption)
    c = 2 * P
    s.circle(0, c, 2 * P).pin('1', (0, 0))
    if openings:
        s.circle(-1.3, c, 1.0, fill='#000000').circle(1.3, c, 1.0)
    if through:
        s.line((0, 0), (0, 4 * P)).pin('2', (0, 4 * P), label=(0.4, 4 * P - 2.2))
    else:
        s.line((0, 0), (0, 3 * P + 0.6)).line((-1.6, 3 * P + 0.6), (1.6, 3 * P + 0.6), w=THICK)
    return s.labels((P + 1.0, P - 0.4), (P + 1.0, 2 * P + 0.4))


add(outlet(C('Antennendurchgangsdose', 'Aerial loop-through outlet', 'Prise d’antenne passante'), True))
add(outlet(C('Antennenenddose', 'Aerial end outlet', 'Prise d’antenne terminale'), False))
add(outlet(C('Antennendurchgangsdose (Radio/TV)', 'Aerial loop-through outlet (radio/TV)', 'Prise d’antenne passante (radio/TV)'), True, True))
add(outlet(C('Antennenenddose (Radio/TV)', 'Aerial end outlet (radio/TV)', 'Prise d’antenne terminale (radio/TV)'), False, True))


# Pictograms: no parts, no connection points
def sign(caption):
    return Symbol('', caption, numbered=False, listed=False, shown=False)


def waves(s, x, y, radii, start, stop, w=W):
    for rad in radii:
        s.arc(x, y, 2 * rad, start, stop, w=w)
    return s


def lattice_mast(s, h=12.0, foot=2.4, bays=4):
    """A lattice mast standing on (0, 0), cross-braced, the tip at (0, -h) with waves on both sides."""
    top = 0.4
    xl = lambda y: -foot + (foot - top) * (-y) / h
    levels = [-h * k / bays for k in range(bays + 1)]
    s.line((-foot, 0), (-top, -h), w=0.35).line((foot, 0), (top, -h), w=0.35)
    for y0, y1 in zip(levels, levels[1:]):
        s.line((xl(y0), y0), (-xl(y1), y1)).line((-xl(y0), y0), (xl(y1), y1)).line((xl(y1), y1), (-xl(y1), y1))
    s.circle(0, -h - 0.8, 1.2, fill='#000000')
    waves(s, 0, -h - 0.8, (2.0, 3.4), -40, 40)
    return waves(s, 0, -h - 0.8, (2.0, 3.4), 140, 220)


def dish(s):
    """A satellite dish seen obliquely: the filled reflector facing up to the right, the feed arm and the foot."""
    c, a, b, tilt = (0, -6.0), 4.4, 1.8, math.radians(45)
    rim = []
    for k in range(24):
        t = 2 * math.pi * k / 24
        x, y = a * math.cos(t), b * math.sin(t)
        rim.append((c[0] + x * math.cos(tilt) + y * math.sin(tilt), c[1] + x * math.sin(tilt) - y * math.cos(tilt)))
    s.poly(*rim, fill='#000000')
    s.line(c, (c[0] + 3.4, c[1] - 3.4), w=0.35).circle(c[0] + 3.6, c[1] - 3.6, 1.2, fill='#000000')
    return s.line((-0.6, -5.4), (-0.6, 0), w=0.5).line((-3.0, 0), (1.8, 0), w=0.5)


s = sign(C('Funkwellen', 'Radio waves', 'Ondes radio'))
add(waves(s.circle(0, 0, 1.2, fill='#000000'), 0, 0, (2.0, 3.4, 4.8), 35, 145))
s = sign(C('Funkwellen (fett)', 'Radio waves (bold)', 'Ondes radio (gras)'))
add(waves(s.circle(0, 0, 1.8, fill='#000000'), 0, 0, (2.2, 3.8, 5.4), 35, 145, w=0.7))
s = sign(C('Funkantenne (Piktogramm)', 'Radio antenna (pictogram)', 'Antenne radio (pictogramme)'))
s.line((0, 0), (0, -4.0), w=0.5).line((-1.4, 0), (1.4, 0), w=0.5).circle(0, -4.4, 1.4, fill='#000000')
add(waves(s, 0, -4.4, (2.2, 3.6), 40, 140))
add(lattice_mast(sign(C('Sendemast (Piktogramm)', 'Transmitter mast (pictogram)', 'Pylône émetteur (pictogramme)'))))
add(dish(sign(C('Satellitenschüssel (Piktogramm)', 'Satellite dish (pictogram)', 'Parabole satellite (pictogramme)'))))
