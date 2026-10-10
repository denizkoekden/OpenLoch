# Insulated crimp connectors (cable lugs) on the page with the IDC connectors: pictures for wiring documentation, seen
# from above at 2:1, the wire entering at the insertion point. The metal is grey, the insulating sleeve has the colour of
# the wire range after DIN 46245 (red 0.5-1.5 mm², blue 1.5-2.5 mm², yellow 4-6 mm²). Receptacle lengths after the
# Cimco catalogue (DIN 46245), the yellow sleeve after Weidmüller LIF 6F638 R; the other sizes are typical values.
# No contacts: they are drawings.
add = extend(EL_E, 'Pressverbinder')

K = 2.0
METAL, BASE, EDGE = '#a0a0a0', '#808080', '#303030'
COLOURS = {'rot': ('red', 'rouge', '#d42a2a', 4.2, 10.0), 'blau': ('blue', 'bleu', '#2a5fd0', 5.0, 10.5),
           'gelb': ('yellow', 'jaune', '#f2c40c', 6.8, 12.5)}     # names, colour, sleeve diameter and length


def at(points):
    return [(K * x, K * y) for x, y in points]


def edged(s):
    s.parts[-1]['pen']['color'] = EDGE
    return s


def shape(s, *points, fill=METAL):
    return edged(s.poly(*at(points), fill=fill, w=0.18))


def thin(s, *points):
    return edged(s.line(*at(points), w=0.18))


def hidden(s, *points):
    """An edge hidden under the insulation, dashed."""
    s.parts.append({'type': 'line', 'points': [pt(p) for p in at(points)], 'pen': {'width': 0.18, 'style': 'dash', 'color': EDGE},
                    'electrical': False})
    return s


def hole(s, x, y, d, fill='#ffffff'):
    return edged(s.circle(K * x, K * y, K * d, w=0.18, fill=fill))


def arc_points(cx, cy, r, a0, a1, steps=12):
    """Points on a circle from angle a0 to a1 (degrees, counter-clockwise on the sheet, 0 to the right)."""
    angles = [math.radians(a0 + (a1 - a0) * k / steps) for k in range(steps + 1)]
    return [(cx + r * math.cos(a), cy - r * math.sin(a)) for a in angles]


def sleeve(s, colour, length):
    """The insulating sleeve over the crimp barrel, a collar at the wire entry; returns its half width."""
    fill, d = COLOURS[colour][2], COLOURS[colour][3]
    h, c = d / 2, 0.3 * length
    shape(s, (0, -h + 0.3), (0.3, -h), (c, -h), (c + 0.4, -0.9 * h), (length - 1.0, -0.9 * h), (length, -0.7 * h),
          (length, 0.7 * h), (length - 1.0, 0.9 * h), (c + 0.4, 0.9 * h), (c, h), (0.3, h), (0, h - 0.3), fill=fill)
    thin(s, (c, -0.9 * h), (c, 0.9 * h))
    return h


def neck(s, x, w):
    shape(s, (x, -w / 2), (x + 1.5, -w / 2), (x + 1.5, w / 2), (x, w / 2))
    return x + 1.5


def lug(de, en, fr, colour, size, head, length=None):
    """A crimp connector: sleeve, neck and the metal head drawn by head(s, x) from x on (real millimetres), which returns
    the head's extent along and its half width; the size is written above the head."""
    c = COLOURS[colour]
    s = Symbol('', C(f'{de} ({colour})', f'{en} ({c[0]})', f'{fr} ({c[1]})'), numbered=False)
    length = length or c[4]
    h = sleeve(s, colour, length)
    x0, x1, half = head(s, neck(s, length, getattr(head, 'neck', 2.4)))
    top = K * max(h, half)
    s.text(size, K * (x0 + x1) / 2, -top - 3.2, 2.2, 'centre')
    return s.labels((0, -top - 3.6), (0, top + 0.8))


RECEPTACLES = {'2,8': (3.9, 8.0, 0.7), '4,8': (6.0, 6.0, 1.0), '6,3': (7.7, 7.5, 1.2)}   # width, length, dimple
TABS = {'2,8': (2.8, 7.0, 1.2, 3.2), '4,8': (4.8, 7.5, 1.4, 3.6), '6,3': (6.3, 8.0, 1.7, 4.0)}  # length, hole, from tip
SLEEVE = {('2,8', 'rot'): 8.0, ('4,8', 'rot'): 10.5, ('6,3', 'rot'): 13.0, ('2,8', 'blau'): 8.5, ('4,8', 'blau'): 10.5,
          ('6,3', 'blau'): 12.1, ('6,3', 'gelb'): 12.0}     # catalogue length less receptacle and neck


def receptacle(size):
    """The rolled receptacle: both edges curled over the tab, the dimple seen in the slot between them."""
    w, l, dimple = RECEPTACLES[size]

    def head(s, x):
        h, e = w / 2, 0.28 * w
        shape(s, (x, -h), (x + l - 0.4, -h), (x + l, -h + 0.3), (x + l, h - 0.3), (x + l - 0.4, h), (x, h))
        shape(s, (x + 0.6, -h + e), (x + l, -h + e), (x + l, h - e), (x + 0.6, h - e), fill=BASE)
        hole(s, x + 0.55 * l, 0, dimple, fill='#606060')
        return x, x + l, h
    head.neck = 2.0
    return head


def tab(size):
    """The flat tab with chamfered tip and its detent hole."""
    w, l, d, pos = TABS[size]

    def head(s, x):
        h, c = w / 2, 0.4 if w < 4 else 0.6
        shape(s, (x, -h), (x + l - c, -h), (x + l, -h + c), (x + l, h - c), (x + l - c, h), (x, h))
        hole(s, x + l - pos, 0, d)
        return x, x + l, h
    head.neck = 2.0
    return head


def ring(d, b):
    """A ring tongue: flared from the neck to the outer diameter b around the hole d."""
    def head(s, x):
        r = b / 2
        xc = x + 1.0 + 0.9 * r
        shape(s, (x, -1.2), (x + 1.0, -r), *arc_points(xc, 0, r, 90, -90), (x + 1.0, r), (x, 1.2))
        hole(s, xc, 0, d)
        return x, xc + r, r
    return head


def fork(slot, b):
    """A fork tongue for the screw size `slot`: two prongs around the slot with its round end."""
    def head(s, x):
        r, q = b / 2, slot / 2
        xc = x + 1.6 + q
        tip = xc + 4.0
        shape(s, (x, -1.2), (x + 1.0, -r), (tip - 0.5, -r), (tip, -r + 0.5), (tip, -q),
              *arc_points(xc, 0, q, 90, 270), (tip, q), (tip, r - 0.5), (tip - 0.5, r), (x + 1.0, r), (x, 1.2))
        return x, tip, r
    return head


def bullet(d, length):
    """The pin of a bullet plug: a collar, the pin with its round nose and the detent groove."""
    def head(s, x):
        r = d / 2
        shape(s, (x, -r - 0.4), (x + 1.2, -r - 0.4), (x + 1.2, r + 0.4), (x, r + 0.4))
        x1 = x + 1.2
        shape(s, (x1, -r), *arc_points(x1 + length - r, 0, r, 90, -90), (x1, r))
        thin(s, (x1 + 0.55 * length, -r), (x1 + 0.55 * length, r))
        return x, x1 + length, r + 0.4
    return head


def bullet_receptacle(d, colour, size):
    """A fully insulated bullet receptacle: the barrel part like a sleeve, the front part wider around the socket, whose
    metal is hidden (dashed)."""
    c = COLOURS[colour]
    s = Symbol('', C(f'Rundsteckhülse {size} ({colour})', f'Bullet receptacle {size} ({c[0]})',
                     f'Cosse cylindrique femelle {size} ({c[1]})'), numbered=False)
    h, f, l = c[3] / 2, (d + 2.8) / 2, c[4]
    x1, x2 = l + 1.5, l + 1.5 + d + 8.0
    shape(s, (0, -h + 0.3), (0.3, -h), (l, -h), (x1, -f), (x2 - 0.6, -f), (x2, -f + 0.6),
          (x2, f - 0.6), (x2 - 0.6, f), (x1, f), (l, h), (0.3, h), (0, h - 0.3), fill=c[2])
    thin(s, (0.3 * l, -h), (0.3 * l, h))
    q = d / 2 + 0.3
    hidden(s, (x2, -q), (x1 + 0.6, -q), (x1 + 0.6, q), (x2, q))
    s.text(size, K * (x1 + x2) / 2, -K * f - 3.2, 2.2, 'centre')
    return s.labels((0, -K * f - 3.6), (0, K * f + 0.8))


def butt(colour, length=25.0):
    """An insulated butt connector: the sleeve with a collar at both ends, the wire stop in the middle, the hidden
    metal tube dashed."""
    c = COLOURS[colour]
    s = Symbol('', C(f'Stoßverbinder ({colour})', f'Butt connector ({c[0]})', f'Manchon de raccordement ({c[1]})'),
               numbered=False)
    h, e = c[3] / 2 + 0.4, 3.0
    upper = [(0, -h + 0.3), (0.3, -h), (e, -h), (e + 0.4, -h + 0.3), (length - e - 0.4, -h + 0.3), (length - e, -h),
               (length - 0.3, -h), (length, -h + 0.3)]
    shape(s, *upper, *[(x, -y) for x, y in reversed(upper)], fill=c[2])
    for x in (e, length / 2, length - e):
        thin(s, (x, -h + 0.3), (x, h - 0.3))
    hidden(s, (e, -1.5), (length - e, -1.5), (length - e, 1.5), (e, 1.5), (e, -1.5))
    s.text('1,5–2,5 mm²', K * length / 2, -K * h - 3.2, 2.2, 'centre')
    return s.labels((0, -K * h - 3.6), (0, K * h + 0.8))


HUELSE = ('Flachsteckhülse', 'Female disconnect', 'Cosse plate femelle')
STECKER = ('Flachstecker', 'Male disconnect', 'Cosse plate mâle')
for colour, sizes in (('rot', ('2,8', '4,8', '6,3')), ('blau', ('2,8', '4,8', '6,3')), ('gelb', ('6,3',))):
    for size in sizes:
        mm, en = size + ' mm', size.replace(',', '.') + ' mm'
        for names, head in ((HUELSE, receptacle(size)), (STECKER, tab(size))):
            add(lug(f'{names[0]} {mm}', f'{names[1]} {en}', f'{names[2]} {mm}', colour, mm, head, SLEEVE[size, colour]))

RINGS = {'M3': (3.2, 5.8), 'M4': (4.3, 7.2), 'M5': (5.3, 8.5)}   # hole and outer diameter
for colour, sizes in (('rot', ('M3', 'M4', 'M5')), ('blau', ('M3', 'M4', 'M5')), ('gelb', ('M5',))):
    for size in sizes:
        d, b = RINGS[size]
        add(lug(f'Ringkabelschuh {size}', f'Ring terminal {size}', f'Cosse à œillet {size}', colour, size,
                ring(d, 9.5 if colour == 'gelb' else b)))

for colour, plug, socket in (('blau', 4, 4), ('gelb', 5, 6), ('rot', 4, 4)):      # diameters as in the captions
    size = f'Ø {plug} mm'
    add(lug(f'Rundstecker {size}', f'Bullet plug {size}', f'Cosse cylindrique mâle {size}', colour, size, bullet(plug, plug + 5.0)))
    add(bullet_receptacle(socket, colour, f'Ø {socket} mm'))

for colour, b in (('rot', 6.4), ('blau', 6.8), ('gelb', 7.2)):
    add(lug('Gabelkabelschuh M4', 'Fork terminal M4', 'Cosse à fourche M4', colour, 'M4', fork(4.3, b)))
add(butt('blau'))
