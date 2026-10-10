# More transistor packages for the page "Transistor-Gehäuse": the metal cans TO-18, TO-39 and TO-100, the power package
# TO-3, TO-92 and TO-126. Outlines after the JEDEC registrations as the manufacturers' data sheets give them (TO-18:
# Philips SOT18/13, TO-39: Philips SOT5/11, TO-3: TT Electronics 2N3055, TO-92: MCC 2SC1815, TO-126: SOT-32, TO-100: TI
# LME0010A). Small packages are drawn 2:1, so that their legs fall on the 2.54 mm pitch; TO-3 and TO-126 1:1. Bottom
# views are seen from the lead side and only show the pin order; legs on the pitch in a side view have contacts.
add = extend(EL_E, 'Transistor-Gehäuse')


def outline(s, *points):
    return s.line(*points, w=0.35)


def can_side(s, x0, flange, cap, height, legs):
    """A metal can seen from the side, standing on its flange at y = 0: the cap with rounded top edge, the legs down to
    2P with their contacts (`legs`: name and x)."""
    rf, rc, t, k = flange / 2, cap / 2, 1.0, 1.0
    s.rect(x0 - rf, -t, x0 + rf, 0, w=0.35)
    outline(s, (x0 - rc, -t), (x0 - rc, -height + k))
    s.arc(x0 - rc + k, -height + k, 2 * k, 90, 180, w=0.35)
    outline(s, (x0 - rc + k, -height), (x0 + rc - k, -height))
    s.arc(x0 + rc - k, -height + k, 2 * k, 0, 90, w=0.35)
    outline(s, (x0 + rc, -height + k), (x0 + rc, -t))
    for name, x in legs:
        s.line((x, 0), (x, 2 * P)).pin(name, (x, 2 * P), label=(x, 2 * P + 0.4), align='centre', shown=True)
    s.labels((x0 + rf + 1.0, -height), (x0 + rf + 1.0, -height + 3.0))


def can_bottom(s, cx, cy, flange, lead, tab, pins, h=1.8):
    """A metal can seen from the lead side: the flange, its tab at the lower left (`tab`: length and width) and the pins
    on the lead circle (`pins`: name and angle, counter-clockwise from the right), each named outside."""
    rf = flange / 2
    s.circle(cx, cy, flange, w=0.35)
    u, v = (-math.sqrt(0.5), math.sqrt(0.5)), (math.sqrt(0.5), math.sqrt(0.5))
    k, j = tab
    base = math.sqrt(rf * rf - j * j / 4)
    corner = lambda r, side: (cx + r * u[0] + side * j / 2 * v[0], cy + r * u[1] + side * j / 2 * v[1])
    outline(s, corner(base, -1), corner(rf + k, -1), corner(rf + k, 1), corner(base, 1))
    for name, angle in pins:
        a = math.radians(angle)
        x, y = cx + lead / 2 * math.cos(a), cy - lead / 2 * math.sin(a)
        s.circle(x, y, 1.0, w=0.18, fill='#000000')
        d = 0.6 + h / 2
        s.text(name, x + d * math.cos(a), y - d * math.sin(a) - h / 2, h, 'centre')


EBC = [('E', 180), ('B', 90), ('C', 0)]       # Philips SOT18 and SOT5: from the tab clockwise E, B, C, seen from below


def to18(s, cx, cy):
    can_bottom(s, cx, cy, 2 * 5.38, 2 * 2.54, (2 * 1.0, 2 * 1.0), EBC)


def to39(s, cx, cy):
    can_bottom(s, cx, cy, 2 * 9.24, 2 * 5.08, (2 * 0.85, 2 * 0.8), EBC)


s = Symbol('', C('Metallgehäuse TO-18 (Ansicht und Anschlussbild)', 'Metal can TO-18 (view and pinout)',
                 'Boîtier métallique TO-18 (vue et brochage)'), numbered=False)
can_side(s, 2 * P, 2 * 5.38, 2 * 4.62, 2 * 5.0, [('E', P), ('B', 2 * P), ('C', 3 * P)])
to18(s, 2 * P, 5.5 * P)
add(s)

s = Symbol('', C('Metallgehäuse TO-39 (Ansicht und Anschlussbild)', 'Metal can TO-39 (view and pinout)',
                 'Boîtier métallique TO-39 (vue et brochage)'), numbered=False)
can_side(s, 3 * P, 2 * 9.24, 2 * 8.25, 2 * 6.5, [('E', P), ('B', 3 * P), ('C', 5 * P)])
to39(s, 3 * P, 7 * P)
add(s)


# TO-92 after MCC 2SC1815: seen from the flat side E, C, B; from below with the flat side down therefore B, C, E.
def to92_bottom(s, cx, flat, names):
    outline(s, (cx - 4.5, flat - 2.5), (cx - 4.5, flat), (cx + 4.5, flat), (cx + 4.5, flat - 2.5))
    s.arc(cx, flat - 2.5, 9.0, 0, 180, w=0.35)
    for k, name in enumerate(names):
        x, y = cx + (k - 1) * P, flat - 2.4
        s.circle(x, y, 1.0, w=0.18, fill='#000000').text(name, x, y - 2.9, 1.8, 'centre')


s = Symbol('', C('TO-92 (Ansicht und Anschlussbild)', 'TO-92 (view and pinout)', 'TO-92 (vue et brochage)'), numbered=False)
s.rect(2 * P - 4.5, -9.0, 2 * P + 4.5, 0, w=0.35)
for name, x in (('E', P), ('C', 2 * P), ('B', 3 * P)):
    s.line((x, 0), (x, 2 * P)).pin(name, (x, 2 * P), label=(x, 2 * P + 0.4), align='centre', shown=True)
to92_bottom(s, 2 * P, 6.5 * P, 'BCE')
s.labels((2 * P + 5.5, -9.0), (2 * P + 5.5, -6.0))
add(s)


# TO-3 (TO-204AA) 1:1 after the 2N3055 data sheet of TT Electronics: holes 30.15 apart, pins 10.92 apart and 16.89 from
# the lower hole; seen from below pin 1 (B) on the left, pin 2 (E) on the right, the case is the collector.
def to3_bottom(s, cy, h=2.5):
    R, r, d = 13.1, 4.2, 30.15 / 2
    b = math.degrees(math.asin((R - r) / d))
    s.arc(0, cy, 2 * R, 360 - b, 360 + b, w=0.35).arc(0, cy, 2 * R, 180 - b, 180 + b, w=0.35)
    s.arc(0, cy - d, 2 * r, b, 180 - b, w=0.35).arc(0, cy + d, 2 * r, 180 + b, 360 - b, w=0.35)
    c, n = math.cos(math.radians(b)), math.sin(math.radians(b))
    for sx in (-1, 1):
        for sy in (-1, 1):
            outline(s, (sx * R * c, cy + sy * R * n), (sx * r * c, cy + sy * (d + r * n)))
    for sy in (-1, 1):
        s.circle(0, cy + sy * d, 4.0, w=W)
    y = cy + d - 16.89
    for name, x, align in (('B', -5.46, 'right'), ('E', 5.46, 'left')):
        s.circle(x, y, 2.6, w=0.18).circle(x, y, 1.0, w=0.18, fill='#000000')
        s.text(name, x + (-2.0 if align == 'right' else 2.0), y - h / 2, h, align)
    s.text('C', 13.4, cy + 6.6, h)
    s.line((13.3, cy + 7.6), (11.8, cy + 6.0), w=0.18)


s = Symbol('', C('TO-3 (Seitenansicht und Anschlussbild)', 'TO-3 (side view and pinout)', 'TO-3 (vue de côté et brochage)'),
           numbered=False)
s.rect(-13.1, -1.6, 13.1, 0, w=0.35)
outline(s, (-10.5, -1.6), (-10.5, -5.1))
s.arc(-8.0, -5.1, 5.0, 90, 180, w=0.35)
outline(s, (-8.0, -7.6), (8.0, -7.6))
s.arc(8.0, -5.1, 5.0, 0, 90, w=0.35)
outline(s, (10.5, -5.1), (10.5, -1.6))
for name, x in (('B', -5.46), ('E', 5.46)):
    s.rect(x - 0.5, 0, x + 0.5, 11.7, w=W).text(name, x, 12.1, 2.5, 'centre')
to3_bottom(s, 36.0)
s.labels((14.6, -7.6), (14.6, -4.6))
add(s)

s = Symbol('Q', C('TO-3 (Ansicht von unten)', 'TO-3 (bottom view)', 'TO-3 (vue de dessous)'), '2N3055')
to3_bottom(s, 0)
add(s.labels((15.0, -8.0), (15.0, -5.0)))

s = Symbol('Q', C('TO-18 (Ansicht von unten)', 'TO-18 (bottom view)', 'TO-18 (vue de dessous)'), 'BCY58')
to18(s, 0, 0)
add(s.labels((6.6, -5.0), (6.6, -2.0)))


# TO-92 in oblique view, seen from the front, above and right: a millimetre of depth runs 0.35 to the right and 0.5 up.
# The leads leave the bottom 1.2 mm behind the flat side, so the body sits left of them by that depth.
DX, DY = 0.35, 0.5


def oblique(u, z, y, x0):
    return (x0 + u + DX * z, y - DY * z)


def plan(steps=12):
    """The TO-92 seen from above (2:1), clockwise from the left end of the flat side: u across, z the depth behind it."""
    pts = [(-4.5, 0), (4.5, 0), (4.5, 2.5)]
    pts += [(4.5 * math.cos(math.pi * k / steps), 2.5 + 4.5 * math.sin(math.pi * k / steps)) for k in range(1, steps)]
    return pts + [(-4.5, 2.5)]


s = Symbol('', C('TO-92 (Schrägansicht)', 'TO-92 (oblique view)', 'TO-92 (vue en perspective)'), numbered=False)
x0, top = 2 * P - DX * 2.4, -9.0
t = math.atan(DX)       # where the round back turns away: the outline on the right
side = [(4.5, 0), (4.5, 2.5)] + [(4.5 * math.cos(t * k / 4), 2.5 + 4.5 * math.sin(t * k / 4)) for k in range(1, 5)]
s.poly(*[oblique(u, z, top, x0) for u, z in plan()], fill='#e0e0e0', w=0.35)
s.poly(*[oblique(u, z, top, x0) for u, z in side], *[oblique(u, z, 0, x0) for u, z in reversed(side)], fill='#b8b8b8', w=0.35)
s.rect(x0 - 4.5, top, x0 + 4.5, 0, w=0.35)
for k in range(3):
    x = (k + 1) * P
    s.line((x, 0), (x, 2 * P)).pin(str(k + 1), (x, 2 * P), label=(x, 2 * P + 0.4), align='centre', shown=True)
add(s.labels((x0 + 7.0, top - 2.0), (x0 + 7.0, top + 1.0)))

# TO-126 (SOT-32) 1:1: body 7.6 x 11, mounting hole 3.1 mm; the leads (2.28 mm apart) drawn on the pitch.
s = Symbol('', C('TO-126', 'TO-126', 'TO-126'), numbered=False)
s.rect(0.5 * P, -11.0, 3.5 * P, 0, w=0.35).circle(2 * P, -7.2, 3.1)
for k in range(3):
    x = (k + 1) * P
    s.line((x, 0), (x, 2 * P)).pin(str(k + 1), (x, 2 * P), label=(x, 2 * P + 0.4), align='centre', shown=True)
add(s.labels((3.5 * P + 1.0, -11.0), (3.5 * P + 1.0, -8.0)))

# TO-100 (MO-006) 2:1 seen from above after TI LME0010A: ten leads on a 5.84 mm circle, 36 degrees apart, the tab in
# line with lead 10, the leads counted counter-clockwise from it.
s = Symbol('', C('TO-100 (Draufsicht)', 'TO-100 (top view)', 'TO-100 (vue de dessus)'), numbered=False)
s.circle(0, 0, 18.0, w=0.35).circle(0, 0, 16.2, w=0.35)
outline(s, (-0.8, -8.96), (-0.8, -10.9), (0.8, -10.9), (0.8, -8.96))
for k in range(10):
    a = math.radians(90 + 36 * (k + 1))
    x, y = 5.84 * math.cos(a), -5.84 * math.sin(a)
    s.circle(x, y, 2.6, w=0.18).text(str(k + 1), x, y - 0.75, 1.5, 'centre')
add(s.labels((10.5, -9.0), (10.5, -6.0)))
