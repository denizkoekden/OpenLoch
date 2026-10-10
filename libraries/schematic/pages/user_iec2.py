# Library "IEC_EN61346" (2): relays, contactors, switches, protective devices, sensors and connections in the style of
# DIN EN 81346 reference designations: the designator ("-K1") left of the symbol, the value below it, terminal numbers
# shown. Contacts, coils and protective devices after DIN EN 60617-7, operating devices, mechanical link and detent
# after DIN EN 60617-2, delay signs as in DIN EN 60617-7 (the movement towards the centre of the arc is delayed). The
# operating signs follow the drawings of the DIN EN 60617-2 numbers given at them as reproduced in Siemens Building
# Technologies CM1N0113E "Graphic symbols for electrical circuit diagrams" (12.1998) and Rockwell Automation
# publication 100-2.10 "A Global Reference Guide for Reading Schematic Diagrams"; device forms after the manufacturer's
# circuit diagrams named at them. On the "(verlinkt)" pages coils are parents and contacts their children.
IEC = 'USER/Misc. (German)/IEC_EN61346'
LINKED = '<PARENT_ID>'
NO_TIP = (-1.9, P + 0.6)     # free end of a make contact's blade as contact() draws it, pivot at (0, 3 * P)
NC_TIP = (2.1, P - 0.4)      # the same for a break contact
LINK_Y = 2 * P - 0.4         # height of the mechanical link
FUSE_LINK_Y = 3.85           # the link above a fuse on the blade
SIGN_X = -4.4                # where the link meets the operating sign
LATCH_X = -6.6               # the same with a detent in the link


def tag(s, x, y=P):
    """Designator right-aligned at x left of the symbol, the value below it."""
    return s.labels((x, y), (x, y + 3.0), 'right')


def child(s):
    s.numbered, s.value = False, '<PARENT_VALUE>'
    return s


def dot(s, x, y):
    return s.circle(x, y, 0.8, fill='#000000')


def on_blade(kind, x, y):
    """The point at height y on the blade of the contact standing at x."""
    tip = NO_TIP if kind == 'no' else NC_TIP
    return (x + tip[0] * (3 * P - y) / (3 * P - tip[1]), y)


def pole(s, x, kind, names, mark=None, length=4 * P):
    """A make ('no') or break ('nc') contact at x with the geometry of contact(), terminal numbers shown; `mark` at the
    fixed contact: 'contactor' (semicircle), 'breaker' (cross), 'disconnector' (bar), 'switch-disconnector' (bar and
    circle)."""
    tip = NO_TIP if kind == 'no' else NC_TIP
    s.line((x, 0), (x, P)).line((x, 3 * P), (x, length)).line((x, 3 * P), (x + tip[0], tip[1]))
    if kind == 'nc':
        s.line((x, P), (x + 1.6, P))
    top = (x + 0.8, 0.2)
    if mark == 'contactor':
        if kind == 'no':
            s.arc(x, P - 0.7, 1.4, 90, 270)
        else:
            s.arc(x, P - 0.7, 1.4, 270, 450)
            top = (x - 0.4, 0.2)
    elif mark == 'breaker':
        s.line((x - 0.8, P - 0.8), (x + 0.8, P + 0.8)).line((x - 0.8, P + 0.8), (x + 0.8, P - 0.8))
        top = (x + 0.9, -0.6)
    elif mark in ('disconnector', 'switch-disconnector'):
        s.line((x - 0.9, P), (x + 0.9, P))
        if mark == 'switch-disconnector':
            s.circle(x, P + 0.55, 1.1)
    s.pin(names[0], (x, 0), label=top, align='right' if top[0] < x else 'left', shown=True)
    s.pin(names[1], (x, length), label=(x + 0.8, length - 2.4), shown=True)
    return on_blade(kind, x, LINK_Y)


def fuse_on_blade(s, x):
    pivot = (x, 3 * P)
    dx, dy = NO_TIP[0], NO_TIP[1] - 3 * P
    n = math.hypot(dx, dy)
    ux, uy = dx / n, dy / n
    cx, cy = pivot[0] + 0.45 * dx, pivot[1] + 0.45 * dy
    a, b = 1.2, 0.6
    s.poly((cx + a * ux - b * uy, cy + a * uy + b * ux), (cx + a * ux + b * uy, cy + a * uy - b * ux),
           (cx - a * ux + b * uy, cy - a * uy - b * ux), (cx - a * ux - b * uy, cy - a * uy + b * ux), fill=None)


def link(s, points, end=None, latch=False):
    """The dashed link through the blade points (left to right) and on to the operating sign at x = end, with a
    detent when latching (DIN EN 60617-2 02-12-10: a V notch in the link with the pawl above it)."""
    y = points[0][1]
    if len(points) > 1:
        dashed(s, points[0], points[-1])
    if end is None:
        return s
    x = points[0][0]
    if not latch:
        return dashed(s, (x, y), (end, y))
    c = (x + end) / 2
    dashed(s, (x, y), (c + 0.7, y))
    s.line((c + 0.7, y), (c, y + 0.9), (c - 0.7, y)).line((c, y - 0.4), (c, y - 1.6))
    return dashed(s, (c - 0.7, y), (end, y))


# Operating signs at the end of the link, the link arriving from the right at (x, y); each returns its left edge.
def manual(s, x, y):
    s.line((x, y - 1.2), (x, y + 1.2))
    return x


def push(s, x, y):
    s.line((x + 0.8, y - 1.2), (x, y - 1.2), (x, y + 1.2), (x + 0.8, y + 1.2))
    return x


def turn(s, x, y):
    s.line((x + 0.8, y - 1.2), (x, y - 1.2), (x, y + 1.2), (x - 0.8, y + 1.2))
    return x - 0.8


def key(s, x, y):
    """Operated by key (DIN EN 60617-2 02-13-13): the bow above, the bit below, the link at the neck."""
    a = x - 0.35
    s.circle(a, y - 0.8, 1.6).poly((a - 0.35, y), (a + 0.35, y), (a + 0.8, y + 1.8), (a - 0.8, y + 1.8), fill=None)
    return a - 0.8


def mushroom(s, x, y):
    s.arc(x, y, 3.0, 90, 270, w=0.35).line((x, y - 1.5), (x, y + 1.5))
    return x - 1.5


def boxed(text):
    def sign(s, x, y):
        s.rect(x - 3.0, y - 1.5, x, y + 1.5).text(text, x - 1.5, y - 1.3, 2.2, 'centre')
        return x - 3.0
    return sign


def thermal(s, x, y):
    """Control by thermal effect (DIN EN 60617-2 02-08-01): the line with its bump towards the link."""
    x0 = x - 1.0
    s.line((x0, y - 1.5), (x0, y - 0.7), (x0 + 0.8, y - 0.7), (x0 + 0.8, y + 0.7), (x0, y + 0.7), (x0, y + 1.5))
    return x0


def float_(s, x, y, w=3.0, h=0.7):
    """Control by fluid level (DIN EN 60617-2 02-14-01): the float, pointed at both ends, with its stem above; the link
    at its right tip."""
    c = x - w / 2
    rad = (w * w / 4 + h * h) / (2 * h)
    a = math.degrees(math.atan2(rad - h, w / 2))
    s.arc(c, y + rad - h, 2 * rad, a, 180 - a).arc(c, y - rad + h, 2 * rad, 180 + a, 360 - a)
    s.line((c, y - h), (c, y - h - 1.2))
    return x - w


def diamond(s, x, y, h=1.4):
    """Operated by proximity effect (DIN EN 60617-2 02-13-06): the diamond with the double line across it."""
    s.poly((x, y), (x - h, y - h), (x - 2 * h, y), (x - h, y + h), fill=None)
    for dx in (-0.2, 0.2):
        s.line((x - h + dx, y - h + 0.2), (x - h + dx, y + h - 0.2))
    return x - 2 * h


def proximity(s, x, y):
    return diamond(s, x, y)


def touch(s, x, y):
    left = diamond(s, x, y)
    s.line((left, y - 1.4), (left, y + 1.4))
    return left


def magnet(s, x, y):
    left = diamond(s, x, y) - 0.6
    s.rect(left - 2.8, y - 0.7, left, y + 0.7).rect(left - 2.8, y - 0.7, left - 1.4, y + 0.7, fill='#000000')
    return left - 2.8


def operated(caption, kind, pins, sign, latch=False, prefix='-S'):
    """A make or break contact (contact() with its terminal numbers) worked by `sign` over the dashed link."""
    s = contact(kind, caption, prefix, pins, numbers=True)
    end = LATCH_X if latch else SIGN_X
    link(s, [on_blade(kind, 0, LINK_Y)], end, latch)
    return tag(s, sign(s, end, LINK_Y) - 1.0, P + 0.4)


def released(caption, kind, pins, sign):
    """An emergency stop as in the circuit diagrams of Siemens 3SU1150-1HB20-1CG0 (rotate-to-unlatch) and
    3SU1150-1HA20-1CG0 (pull-to-unlatch): the mushroom head (02-13-08) with the latching device engaged (02-12-13)
    next to it, the release `sign` on a branch of the link rising between latch and contact; the break contact
    carries the sign of positive opening."""
    s = contact(kind, caption, '-S', pins, numbers=True)
    y = LINK_Y
    start = on_blade(kind, 0, y)
    xr = -4.4                      # the release branch
    xm = xr - 3.4                  # the flat side of the mushroom
    dashed(s, start, (xr - 1.0, y))
    s.line((xm, y), (xr - 1.0, y)).line((xr - 2.8, y), (xr - 2.8, y - 0.9), (xr - 1.0, y))
    left = mushroom(s, xm, y)
    dashed(s, (xr, y), (xr, y - 2.6))
    sign(s, xr, y - 2.6)
    if kind == 'nc':
        c = 3.6
        s.circle(c, y, 2.4).line((c - 0.7, y), (c + 0.7, y)).line((c + 0.2, y - 0.5), (c + 0.7, y), (c + 0.2, y + 0.5))
    return tag(s, left - 1.0, P + 0.4)


# Release signs for a link arriving from below at (x, y): the operating signs of DIN EN 60617-2 turned a quarter, as
# in the Siemens diagrams named at released()
def turn_up(s, x, y):
    s.line((x + 1.2, y + 0.8), (x + 1.2, y), (x - 1.2, y), (x - 1.2, y - 0.8))


def pull_up(s, x, y):
    s.line((x + 1.2, y - 0.8), (x + 1.2, y), (x - 1.2, y), (x - 1.2, y - 0.8))


def key_up(s, x, y):
    """The key of 02-13-13 lying: the bit to the left, the bow to the right, the branch at the neck."""
    s.circle(x + 0.8, y - 0.35, 1.6).poly((x, y), (x, y - 0.7), (x - 1.8, y - 1.15), (x - 1.8, y + 0.45), fill=None)


def position_mark(s, kind):
    tip = NO_TIP if kind == 'no' else NC_TIP
    dx, dy = tip[0], tip[1] - 3 * P
    n = math.hypot(dx, dy)
    u = (dx / n, dy / n)
    v = (u[1], -u[0])
    c = (0.45 * dx, 3 * P + 0.45 * dy)
    triangle(s, (c[0] + 1.1 * v[0], c[1] + 1.1 * v[1]), c, 0.7, filled=False)


# --- Coils
def relay_coil(prefix, caption, top='A1', parent=False):
    """The operating coil (DIN EN 60617-7) standing between its terminals, the terminal numbers left of the leads."""
    s = Symbol(prefix, caption)
    s.parent = parent
    bottom = 'A2' if top == 'A1' else 'A1'
    s.rect(-2 * P, P, 2 * P, 3 * P, w=0.35).line((0, 0), (0, P)).line((0, 3 * P), (0, 4 * P))
    s.pin(top, (0, 0), label=(-0.4, 0.2), align='right', shown=True)
    s.pin(bottom, (0, 4 * P), label=(-0.4, 4 * P - 2.4), align='right', shown=True)
    return tag(s, -2 * P - 1.0)


def across(s, element):
    """An element across the coil at x = 3 P, joined to both leads."""
    x = 3 * P
    s.line((0, P / 2), (x, P / 2), (x, P)).line((x, 3 * P), (x, 3.5 * P), (0, 3.5 * P))
    dot(s, 0, P / 2)
    dot(s, 0, 3.5 * P)
    element(s, x)
    return s


def diode_up(s, x):          # cathode at the top
    s.line((x, P), (x, 1.5 * P)).line((x, 2.5 * P), (x, 3 * P))
    s.poly((x - 1.5, 2.5 * P), (x + 1.5, 2.5 * P), (x, 1.5 * P), fill=None).line((x - 1.5, 1.5 * P), (x + 1.5, 1.5 * P), w=0.35)


def diode_down(s, x):        # cathode at the bottom
    s.line((x, P), (x, 1.5 * P)).line((x, 2.5 * P), (x, 3 * P))
    s.poly((x - 1.5, 1.5 * P), (x + 1.5, 1.5 * P), (x, 2.5 * P), fill=None).line((x - 1.5, 2.5 * P), (x + 1.5, 2.5 * P), w=0.35)


def rc(s, x):                # resistor and capacitor in series
    s.rect(x - 0.8, P, x + 0.8, 2 * P).line((x, 2 * P), (x, 2.5 * P - 0.5)).line((x, 2.5 * P + 0.5), (x, 3 * P))
    s.line((x - 1.4, 2.5 * P - 0.5), (x + 1.4, 2.5 * P - 0.5), w=THICK).line((x - 1.4, 2.5 * P + 0.5), (x + 1.4, 2.5 * P + 0.5), w=THICK)


def varistor(s, x):
    y0, y1 = 1.2 * P, 2.8 * P
    s.line((x, P), (x, y0)).rect(x - 1.0, y0, x + 1.0, y1).line((x, y1), (x, 3 * P))
    s.line((x - 2.0, y0 - 0.6), (x - 2.0, y0 + 0.4), (x + 2.0, y1 + 0.2))
    s.text('U', x + 1.3, y0 - 0.2, 2.0)


def field(s, k=1, fill=None):
    """The k-th added field left of the coil; returns its left edge, right edge and centre."""
    x1 = -(k + 1) * P
    s.rect(x1 - P, P, x1, 3 * P, w=0.35, fill=fill)
    return x1 - P, x1, x1 - P / 2


def impulse(s):
    return s.line((-2.4, 2 * P + 0.8), (0, 2 * P + 0.8), (0, 2 * P - 0.8), (2.4, 2 * P - 0.8))


def flasher(s):
    x0, x1, c = field(s)
    y = 2 * P + 0.6
    points = [(c - 1.1, y)]
    for k, x in enumerate((c - 0.7, c - 0.3, c + 0.3, c + 0.7)):
        hi = k % 2 == 0
        points += [(x, y - 1.2 if not hi else y), (x, y - 1.2 if hi else y)]
    s.line(*points, (c + 1.1, y), w=0.18)
    return tag(s, x0 - 1.0)


def timer(caption, off=False, on=False):
    """A time relay: the black field for off-delay, the crossed field for on-delay, control input B1 at the right."""
    s = relay_coil('-K', caption)
    k = 1
    left = -2 * P
    if off:
        left = field(s, k, fill='#000000')[0]
        k += 1
    if on:
        x0, x1, c = field(s, k)
        s.line((x0, P), (x1, 3 * P)).line((x0, 3 * P), (x1, P))
        left = x0
    s.line((2 * P, 2 * P), (3 * P, 2 * P)).pin('B1', (3 * P, 2 * P), label=(3 * P - 0.2, 2 * P - 2.2), align='right', shown=True)
    return tag(s, left - 1.0)


# --- Delayed and change-over contacts
def delay(s, pivot, tip, signs, x_end=-4.0, ym=2 * P + 0.2):
    """The delay sign: a double stem from the blade to the left ending in an arc. '(' delays the movement to the right,
    ')' the movement to the left (the movement towards the centre of the arc is delayed); both for either way."""
    for y in (ym - 0.4, ym + 0.4):
        x = pivot[0] + (tip[0] - pivot[0]) * (pivot[1] - y) / (pivot[1] - tip[1])
        s.line((x, y), (x_end, y))
    a = math.degrees(math.asin(0.8))
    if '(' in signs:
        s.arc(x_end + 1.96, ym, 4.0, 180 - a, 180 + a)
    if ')' in signs:
        s.arc(x_end - 1.96, ym, 4.0, 360 - a, 360 + a)
    return s


def delayed(caption, kind, pins, signs):
    s = contact(kind, caption, '-K', pins, numbers=True)
    delay(s, (0, 3 * P), NO_TIP if kind == 'no' else NC_TIP, signs)
    return tag(s, -5.6, P + 0.4)


def changeover(s, m=lambda x, y: (x, y)):
    """A change-over contact with the lines of contact('co') in local coordinates, each point mapped by m: the common
    contact below at (0, 4 P), the break contact above at (0, 0), the make contact at (-P, 0). Returns the three pin
    places in that order."""
    q = lambda *points: [m(*p) for p in points]
    s.line(*q((0, 4 * P), (0, 3 * P))).line(*q((0, 3 * P), NC_TIP)).line(*q((0, 0), (0, P), (1.6, P)))
    s.line(*q((-P, 0), (-P, P + 0.6)))
    return q((0, 4 * P), (0, 0), (-P, 0))


def pins3(s, names, places, labels, aligns=('left', 'left', 'left')):
    for name, at, label, align in zip(names, places, labels, aligns):
        s.pin(name, at, label=label, align=align, shown=True)
    return s


def timed_changeover(caption, signs):
    """The change-over contact of a time relay, mirrored so that it operates to the right like the single contacts."""
    s = Symbol('-K', caption)
    m = lambda x, y: (-x, y)
    places = changeover(s, m)
    pins3(s, ('15', '16', '18'), places, [(0.8, 4 * P - 2.4), (-0.4, -0.3), (P + 0.4, 0.2)], ('left', 'right', 'left'))
    delay(s, (0, 3 * P), m(*NC_TIP), signs)
    return tag(s, -5.6, P + 0.4)


# --- Page "Relais (verlinkt)"
add = page(IEC, 'Relais (verlinkt)', 'Relays (linked)', 'Relais (liés)')
for top, flipped, de, en, fr in (('A1', False, 'A1 oben', 'A1 at top', 'A1 en haut'),
                                 ('A1', True, 'A1 oben, Diode gedreht', 'A1 at top, diode reversed', 'A1 en haut, diode inversée'),
                                 ('A2', False, 'A2 oben', 'A2 at top', 'A2 en haut'),
                                 ('A2', True, 'A2 oben, Diode gedreht', 'A2 at top, diode reversed', 'A2 en haut, diode inversée')):
    s = relay_coil('-K', C(f'Relais DC ({de})', f'DC relay ({en})', f'Relais CC ({fr})'), top, parent=True)
    # The cathode at A1, the positive terminal (Siemens 3RT2916-1DG00; Finder series 99 data sheet: diode module of
    # standard polarity, + to A1); "Diode gedreht" for coils with the positive terminal A2 (Finder 99.02.9.024.79,
    # non-standard polarity).
    cathode_up = (top == 'A1') != flipped
    add(across(s, diode_up if cathode_up else diode_down))
add(impulse(relay_coil('-K', C('Stromstoßrelais', 'Impulse relay', 'Télérupteur'), parent=True)))
for n in range(1, 5):
    names = (f'{n}2', f'{n}1', f'{n}4')
    s = contact('co', C(f'Wechsler {n} ({n}2/{n}4 oben)', f'Change-over contact {n} ({n}2/{n}4 at top)',
                       f'Contact inverseur {n} ({n}2/{n}4 en haut)'), LINKED, names, numbers=True)
    add(tag(child(s), -P - 1.0, P + 0.8))
for n in range(1, 5):
    s = child(Symbol(LINKED, C(f'Wechsler {n} ({n}1 oben)', f'Change-over contact {n} ({n}1 at top)',
                               f'Contact inverseur {n} ({n}1 en haut)')))
    places = changeover(s, lambda x, y: (-x, 4 * P - y))
    pins3(s, (f'{n}1', f'{n}2', f'{n}4'), places, [(0.8, 0.2), (-0.4, 4 * P - 2.0), (P + 0.4, 4 * P - 2.0)], ('left', 'right', 'left'))
    add(tag(s, -3.0, P))

# --- Page "Relais und -kontakte"
add = page(IEC, 'Relais und -kontakte', 'Relays and relay contacts', 'Relais et contacts de relais')
add(across(relay_coil('-K', C('Relais AC', 'AC relay', 'Relais CA')), rc))
add(across(relay_coil('-K', C('Relais DC', 'DC relay', 'Relais CC')), diode_up))
add(across(relay_coil('-K', C('Relais DC (A2 oben)', 'DC relay (A2 at top)', 'Relais CC (A2 en haut)'), 'A2'), diode_down))
add(impulse(relay_coil('-K', C('Stromstoßrelais', 'Impulse relay', 'Télérupteur'))))
add(flasher(relay_coil('-K', C('Blinkrelais', 'Flasher relay', 'Relais clignoteur'))))
add(timer(C('Zeitrelais, rückfallverzögert', 'Time relay, off-delay', 'Relais temporisé au repos'), off=True))
add(timer(C('Zeitrelais, ansprechverzögert', 'Time relay, on-delay', 'Relais temporisé au travail'), on=True))
add(timer(C('Zeitrelais, ansprech- und rückfallverzögert', 'Time relay, on-delay and off-delay',
            'Relais temporisé au travail et au repos'), off=True, on=True))
# Operating moves the blades of contact() to the right, releasing to the left.
add(delayed(C('Schließer, öffnet verzögert', 'Make contact, delayed opening', 'Contact à fermeture, ouverture temporisée'), 'no', ('11', '14'), ')'))
add(delayed(C('Schließer, schließt verzögert', 'Make contact, delayed closing', 'Contact à fermeture, fermeture temporisée'), 'no', ('11', '14'), '('))
add(delayed(C('Schließer, schließt und öffnet verzögert', 'Make contact, delayed closing and opening',
              'Contact à fermeture, fermeture et ouverture temporisées'), 'no', ('11', '14'), '()'))
add(delayed(C('Öffner, schließt verzögert', 'Break contact, delayed closing', 'Contact à ouverture, fermeture temporisée'), 'nc', ('11', '12'), ')'))
add(delayed(C('Öffner, öffnet verzögert', 'Break contact, delayed opening', 'Contact à ouverture, ouverture temporisée'), 'nc', ('11', '12'), '('))
add(delayed(C('Öffner, öffnet und schließt verzögert', 'Break contact, delayed opening and closing',
              'Contact à ouverture, ouverture et fermeture temporisées'), 'nc', ('11', '12'), '()'))
add(timed_changeover(C('Wechsler, rückfallverzögert', 'Change-over contact, off-delay', 'Contact inverseur temporisé au repos'), ')'))
add(timed_changeover(C('Wechsler, ansprechverzögert', 'Change-over contact, on-delay', 'Contact inverseur temporisé au travail'), '('))
add(timed_changeover(C('Wechsler, ansprech- und rückfallverzögert', 'Change-over contact, on-delay and off-delay',
                       'Contact inverseur temporisé au travail et au repos'), '()'))

CO = ('11', '12', '14')
# 12 and 14 2 P apart on both sides of the common contact, the blade resting on the end of 12
s = Symbol('-K', C('Wechsler (12/14 oben)', 'Change-over contact (12/14 at top)', 'Contact inverseur (12/14 en haut)'))
s.line((0, 4 * P), (0, 3 * P), (1.15 * P, 3 * P - 2.3 * P)).line((P, 0), (P, P)).line((-P, 0), (-P, P + 0.6))
pins3(s, CO, [(0, 4 * P), (P, 0), (-P, 0)], [(0.8, 4 * P - 2.4), (P + 0.4, 0.2), (-P - 0.4, 0.2)], ('left', 'left', 'right'))
add(tag(s, -P - 1.0, P + 0.8))
s = contact('co', C('Wechsler (12/14 oben, schmal)', 'Change-over contact (12/14 at top, narrow)', 'Contact inverseur (12/14 en haut, étroit)'),
            '-K', ('12', '11', '14'), numbers=True)
add(tag(s, -P - 1.0, P + 0.8))
H = 4 * P
# The other change-over forms: (caption, mapping of the local contact('co') lines, pin labels and their alignment for 11,
# 12, 14, designator place); horizontal forms carry the designator at the upper left, clear of the wires.
VARIANTS = {
    'waagerecht': (('Wechsler (waagerecht)', 'Change-over contact, horizontal', 'Contact inverseur horizontal'),
                   lambda x, y: (H - y, x), [(0.4, -2.2), (H - 0.2, -P + 0.45), (H - 0.4, -P - 2.2)], ('left', 'right', 'right'), (-0.6, -5.6)),
    'gespiegelt': (('Wechsler (waagerecht, gespiegelt)', 'Change-over contact, horizontal, mirrored', 'Contact inverseur horizontal, en miroir'),
                   lambda x, y: (y, -x), [(H - 0.4, -2.2), (0.2, 0.37), (0.4, P + 0.3)], ('right', 'left', 'left'), (-0.6, -5.6)),
    '11 rechts': (('Wechsler (waagerecht, 11 rechts)', 'Change-over contact, horizontal, 11 on the right', 'Contact inverseur horizontal, 11 à droite'),
                  lambda x, y: (y, x), [(H - 0.4, -2.2), (0.2, -P + 0.45), (0.4, -P - 2.2)], ('right', 'left', 'left'), (-0.6, -5.6)),
    '11 unten': (('Wechsler (11 unten)', 'Change-over contact (11 at bottom)', 'Contact inverseur (11 en bas)'), lambda x, y: (-x, y),
                 [(0.8, 4 * P - 2.4), (-0.4, -0.3), (P + 0.4, 0.2)], ('left', 'right', 'left'), (-3.0, P + 0.4)),
    'gedreht': (('Wechsler (gedreht)', 'Change-over contact, upside down', 'Contact inverseur retourné'), lambda x, y: (-x, 4 * P - y),
                [(0.8, 0.2), (-0.4, 4 * P - 2.0), (P + 0.4, 4 * P - 2.0)], ('left', 'right', 'left'), (-3.0, P)),
    'gedreht, gespiegelt': (('Wechsler (gedreht, gespiegelt)', 'Change-over contact, upside down, mirrored', 'Contact inverseur retourné, en miroir'),
                            lambda x, y: (x, 4 * P - y), [(-0.4, 0.2), (0.4, 4 * P - 2.0), (-P - 0.4, 4 * P - 2.0)], ('right', 'left', 'right'), (-P - 1.0, P)),
    '14 unten': (('Wechsler (waagerecht, 14 unten)', 'Change-over contact, horizontal, 14 at bottom', 'Contact inverseur horizontal, 14 en bas'),
                 lambda x, y: (H - y, -x), [(0.4, -2.2), (H - 0.2, 0.37), (H - 0.4, P + 0.3)], ('left', 'right', 'right'), (-0.6, -5.6)),
}
for name in ('waagerecht', 'gespiegelt', '11 rechts', '11 unten', 'gedreht', 'gedreht, gespiegelt', '14 unten'):
    caption, m, labels, aligns, d = VARIANTS[name]
    s = Symbol('-K', C(*caption))
    pins3(s, CO, changeover(s, m), labels, aligns)
    add(tag(s, *d))

# --- Page "Schalter und Taster"
add = page(IEC, 'Schalter und Taster', 'Switches and push buttons', 'Interrupteurs et boutons-poussoirs')
KINDS = (('no', ('13', '14'), 'Schließer', 'make', 'fermeture'), ('nc', ('11', '12'), 'Öffner', 'break', 'ouverture'))


def both(de, en, fr, sign, latch=False, prefix='-S'):
    for kind, pins, dk, ek, fk in KINDS:
        add(operated(C(f'{de} ({dk})', f'{en} ({ek})', f'{fr} ({fk})'), kind, pins, sign, latch, prefix))


s = Symbol('-Q', C('Hauptschalter (dreipolig)', 'Main switch, three-pole', 'Interrupteur principal tripolaire'))
points = [pole(s, 2 * P * k, 'no', (str(2 * k + 1), str(2 * k + 2)), 'switch-disconnector') for k in range(3)]
link(s, points, LATCH_X, latch=True)
add(tag(s, turn(s, LATCH_X, LINK_Y) - 1.0, P + 0.4))
for kind, pins, dk, ek, fk in KINDS:
    s = contact(kind, C(f'Schalter, mechanisch ({dk})', f'Mechanically operated switch ({ek})',
                        f'Interrupteur à commande mécanique ({fk})'), '-S', pins, numbers=True)
    position_mark(s, kind)
    add(tag(s, -2.8, P + 0.4))
both('Drucktaster', 'Push button', 'Bouton-poussoir', push)
both('Drehtaster', 'Rotary switch, momentary', 'Commutateur rotatif fugitif', turn)
both('Drehschalter', 'Rotary switch, latching', 'Commutateur rotatif à accrochage', turn, latch=True)
both('Schlüsseltaster', 'Key switch, momentary', 'Interrupteur à clé fugitif', key)
both('Schlüsselschalter', 'Key switch, latching', 'Interrupteur à clé à accrochage', key, latch=True)
# Operated by a counter: the square with 0, as the actuator by counter of IEC 60617-2 is reproduced in the table
# "Actuators" of electrical-symbols.com (next to the actuators by fluid level, flow and gas of 02-14).
both('Zählerkontakt', 'Counter-operated contact', 'Contact actionné par compteur', boxed('0'))
for de, en, fr, sign in (('Drehentriegelung', 'turn to release', 'déverrouillage par rotation', turn_up),
                         ('Zugentriegelung', 'pull to release', 'déverrouillage par traction', pull_up),
                         ('Schlüsselentriegelung', 'key release', 'déverrouillage par clé', key_up)):
    for kind, pins, dk, ek, fk in KINDS:
        add(released(C(f'Not-Aus, {de} ({dk})', f'Emergency stop, {en} ({ek})', f"Arrêt d'urgence, {fr} ({fk})"), kind, pins, sign))

# --- Page "Schutzeinrichtungen"
add = page(IEC, 'Schutzeinrichtungen', 'Protective devices', 'Dispositifs de protection')
NAMES = lambda k: (str(2 * k + 1), str(2 * k + 2))


def fuses(caption, n, value=''):
    s = Symbol('-F', caption, value)
    for k in range(n):
        x = 2 * P * k
        s.rect(x - 1.0, P, x + 1.0, 3 * P).line((x, 0), (x, 4 * P))
        s.pin(NAMES(k)[0], (x, 0), label=(x + 0.8, 0.2), shown=True).pin(NAMES(k)[1], (x, 4 * P), label=(x + 0.8, 4 * P - 2.4), shown=True)
    return tag(s, -2.0)


def releases(s, start, sign=None):
    """The overcurrent releases on the link, each in a box as in the circuit diagram of Siemens 3RV2011-1AA10:
    electromagnetic (I>) and thermal (the sign of thermal effect, DIN EN 60617-2 02-08-01); after them a turning
    handle with a detent when `sign` is given. Returns the left edge."""
    y = start[1]
    dashed(s, start, (-2.6, y))
    s.rect(-5.6, y - 1.5, -2.6, y + 1.5).text('I>', -4.1, y - 1.1, 2.2, 'centre')
    dashed(s, (-5.6, y), (-6.4, y))
    x = -8.3
    s.rect(-9.4, y - 1.5, -6.4, y + 1.5).line((x, y - 1.5), (x, y - 0.7), (x + 0.8, y - 0.7), (x + 0.8, y + 0.7), (x, y + 0.7), (x, y + 1.5))
    if not sign:
        return -9.4
    link(s, [(-9.4, y)], -14.0, latch=True)
    return sign(s, -14.0, y)


def breaker(prefix, caption, n, value='', handle=None):
    s = Symbol(prefix, caption, value)
    points = [pole(s, 2 * P * k, 'no', NAMES(k), 'breaker') for k in range(n)]
    link(s, points)
    return tag(s, releases(s, points[0], handle) - 1.0, P + 0.4)


def mains_filter(caption, n):
    """A mains (interference) filter drawn as low-pass filter (DIN EN 60617-10, as in the de.wikipedia list of circuit
    symbols, "Tiefpass"): two waves, the upper one struck out."""
    s = Symbol('-V', caption)
    xs = [2 * P * k for k in range(n)]
    c = xs[-1] / 2
    s.rect(-P, P, xs[-1] + P, 3 * P, w=0.35)
    for y in (2 * P - 0.8, 2 * P + 0.8):
        s.arc(c - 1.0, y, 2.0, 0, 180, dy=1.0).arc(c + 1.0, y, 2.0, 180, 360, dy=1.0)
    s.line((c - 0.4, 2 * P - 0.1), (c + 0.4, 2 * P - 1.5))
    for k, x in enumerate(xs):
        s.line((x, 0), (x, P)).line((x, 3 * P), (x, 4 * P))
        s.pin(NAMES(k)[0], (x, 0), label=(x + 0.5, 0.2), shown=True).pin(NAMES(k)[1], (x, 4 * P), label=(x + 0.5, 4 * P - 2.4), shown=True)
    return tag(s, -P - 1.0)


def reactor(caption, n):
    s = Symbol('-R', caption)
    for k in range(n):
        x = 2 * P * k
        s.line((x, 0), (x, P)).line((x, 3 * P), (x, 4 * P)).line((x + 1.4, P), (x + 1.4, 3 * P), w=THICK)
        for j in range(4):
            s.arc(x, P + (j + 0.5) * P / 2, P / 2, 270, 450)
        s.pin(NAMES(k)[0], (x, 0), label=(x + 0.8, 0.2), shown=True).pin(NAMES(k)[1], (x, 4 * P), label=(x + 0.8, 4 * P - 2.4), shown=True)
    return tag(s, -1.4)


def rcd(caption, n):
    """Residual current device: make contacts released over the dashed link by the IΔ release, which is fed by the
    summation current transformer (a ring around all conductors). The test circuit is inside the device as in the
    circuit diagram of Siemens 5SV3312-6: the test button T with its resistor from the last conductor before the
    ring to the one before it behind the ring; it has no terminal."""
    s = Symbol('-Q', caption)
    xs = [2 * P * k for k in range(n)]
    ring, y_a, y_b = 5 * P, 3.2 * P, 5.7 * P
    points = [pole(s, x, 'no', NAMES(k), length=7 * P) for k, x in enumerate(xs)]
    link(s, points)
    s.parts.append({'type': 'ellipse', 'centre': pt(((xs[0] + xs[-1]) / 2, ring)), 'size': [r(xs[-1] - xs[0] + 3.2), 1.8],
                    'pen': {'width': W}})
    s.rect(-8.4, ring - 1.8, -4.4, ring + 1.8).text('IΔ', -6.4, ring - 1.2, 2.0, 'centre').line((-4.4, ring), (xs[0] - 1.6, ring))
    dashed(s, (-6.4, ring - 1.8), (-6.4, LINK_Y), points[0])
    xt = xs[-1] + 2 * P
    fixed, pivot = y_a + 2.8, y_a + 5.2
    s.line((xs[-1], y_a), (xt, y_a), (xt, y_a + 0.4)).rect(xt - 0.7, y_a + 0.4, xt + 0.7, y_a + 2.4).line((xt, y_a + 2.4), (xt, fixed))
    s.line((xt, y_b), (xt, pivot), (xt - 0.95, fixed + 0.3)).line((xt, y_b), (xs[-2], y_b))
    m = (fixed + 0.3 + pivot) / 2
    dashed(s, (xt - 0.45, m), (xt + 2.6, m))
    s.line((xt + 1.8, m - 1.0), (xt + 2.6, m - 1.0), (xt + 2.6, m + 1.0), (xt + 1.8, m + 1.0)).text('T', xt + 3.0, m - 1.2, 2.0)
    dot(dot(s, xs[-1], y_a), xs[-2], y_b)
    return tag(s, -9.4)


def fused(caption, value='', mark=None, separate=False, sign=manual, latch=False):
    """Three poles with fuses: on the blade, or `separate`ly in series below the contact; operated by `sign`."""
    s = Symbol('-Q', caption, value)
    y = LINK_Y if separate else FUSE_LINK_Y
    points = []
    for k in range(3):
        x = 2 * P * k
        pole(s, x, 'no', NAMES(k), mark, length=6 * P if separate else 4 * P)
        if separate:
            s.rect(x - 1.0, 3 * P + 0.6, x + 1.0, 5 * P)
        else:
            fuse_on_blade(s, x)
        points.append(on_blade('no', x, y))
    end = LATCH_X if latch else SIGN_X
    link(s, points, end, latch)
    return tag(s, sign(s, end, y) - 1.0, P + 0.4)


def arrester(caption, n):
    """Surge arrester: the lead entering the box ends in an arrow."""
    s = Symbol('-F', caption)
    for k in range(n):
        x = 2 * P * k
        s.rect(x - 1.0, P, x + 1.0, 3 * P).line((x, 0), (x, P + 2.0), end='arrow', size=1.4).line((x, 3 * P), (x, 4 * P))
        s.pin(NAMES(k)[0], (x, 0), label=(x + 0.8, 0.2), shown=True).pin(NAMES(k)[1], (x, 4 * P), label=(x + 0.8, 4 * P - 2.4), shown=True)
    return tag(s, -2.0)


add(fuses(C('Schmelzsicherung', 'Fuse', 'Fusible'), 1, '10A'))
add(fuses(C('Sicherung (dreipolig)', 'Fuse, three-pole', 'Fusible tripolaire'), 3))
add(breaker('-F', C('Leitungsschutzschalter', 'Miniature circuit breaker', 'Disjoncteur divisionnaire'), 1, '10A'))
add(breaker('-F', C('Leitungsschutzschalter (dreipolig)', 'Miniature circuit breaker, three-pole', 'Disjoncteur divisionnaire tripolaire'), 3))
add(mains_filter(C('Netzfilter', 'Mains filter', 'Filtre secteur'), 2))
add(mains_filter(C('Netzfilter (dreiphasig)', 'Mains filter, three-phase', 'Filtre secteur triphasé'), 3))
add(reactor(C('Drossel', 'Line reactor', 'Inductance de ligne'), 1))
add(reactor(C('Drossel (dreiphasig)', 'Line reactor, three-phase', 'Inductance de ligne triphasée'), 3))
add(rcd(C('FI-Schutzschalter (zweipolig)', 'Residual current device, two-pole', 'Interrupteur différentiel bipolaire'), 2))
add(rcd(C('FI-Schutzschalter (vierpolig)', 'Residual current device, four-pole', 'Interrupteur différentiel tétrapolaire'), 4))
add(breaker('-Q', C('Motorschutzschalter (einpolig)', 'Motor protective circuit breaker, single-pole', 'Disjoncteur moteur unipolaire'), 1, '25A', turn))
add(breaker('-Q', C('Motorschutzschalter (dreipolig)', 'Motor protective circuit breaker, three-pole', 'Disjoncteur moteur tripolaire'), 3, '', turn))
add(fused(C('Hauptschalter mit Sicherungen', 'Main switch with fuses', 'Interrupteur principal avec fusibles'), '160A',
          'switch-disconnector', separate=True, sign=turn, latch=True))
add(fused(C('Sicherungsschalter (Einspeisung)', 'Fuse-switch (incoming supply)', 'Fusible-interrupteur (arrivée)'), '160A'))
add(fused(C('Sicherungslasttrennschalter (Einspeisung)', 'Fuse-switch-disconnector (incoming supply)',
            'Fusible-interrupteur-sectionneur (arrivée)'), mark='switch-disconnector'))
add(fused(C('Sicherungstrennschalter (Einspeisung)', 'Fuse-disconnector (incoming supply)', 'Fusible-sectionneur (arrivée)'),
          mark='disconnector'))
add(arrester(C('Überspannungsableiter', 'Surge arrester', 'Parafoudre'), 1))
add(arrester(C('Überspannungsableiter (dreipolig)', 'Surge arrester, three-pole', 'Parafoudre tripolaire'), 3))


# --- Pages "Schütze (verlinkt)" and "Schütze und -kontakte"
def main_contacts(prefix, caption, aux=False):
    """Three main make contacts with the contactor function, with an auxiliary break contact 21-22 when `aux`."""
    s = Symbol(prefix, caption)
    points = [pole(s, 2 * P * k, 'no', NAMES(k), 'contactor') for k in range(3)]
    if aux:
        points.append(pole(s, 6 * P, 'nc', ('21', '22')))
    link(s, points)
    return tag(s, -2.8, P + 0.4)


def main_contact(prefix, caption, kind, names):
    s = Symbol(prefix, caption)
    pole(s, 0, kind, names, 'contactor')
    return tag(s, -2.8, P + 0.4)


add = page(IEC, 'Schütze (verlinkt)', 'Contactors (linked)', 'Contacteurs (liés)')
add(across(relay_coil('-Q', C('Schütz DC (A1 oben)', 'DC contactor (A1 at top)', 'Contacteur CC (A1 en haut)'), parent=True), diode_up))
add(across(relay_coil('-Q', C('Schütz DC (A2 oben)', 'DC contactor (A2 at top)', 'Contacteur CC (A2 en haut)'), 'A2', parent=True), diode_down))
add(across(relay_coil('-Q', C('Schütz AC (A1 oben)', 'AC contactor (A1 at top)', 'Contacteur CA (A1 en haut)'), parent=True), rc))
for k in range(3):
    a, b = NAMES(k)
    add(child(main_contact(LINKED, C(f'Leistungsschließer {a}–{b}', f'Main make contact {a}–{b}', f'Contact principal à fermeture {a}–{b}'), 'no', (a, b))))
add(child(main_contacts(LINKED, C('Leistungsschließer dreipolig', 'Main make contacts, three-pole', 'Contacts principaux à fermeture, tripolaires'))))
add(child(main_contacts(LINKED, C('Schützblock (3 Schließer, 1 Öffner)', 'Contactor block (3 make, 1 break)',
                                  'Bloc contacteur (3 fermetures, 1 ouverture)'), aux=True)))

add = page(IEC, 'Schütze und -kontakte', 'Contactors and contactor contacts', 'Contacteurs et contacts de contacteur')
add(across(relay_coil('-Q', C('Schütz DC', 'DC contactor', 'Contacteur CC')), diode_up))
add(across(relay_coil('-Q', C('Schütz DC (A2 oben)', 'DC contactor (A2 at top)', 'Contacteur CC (A2 en haut)'), 'A2'), diode_down))
add(across(relay_coil('-Q', C('Schütz AC', 'AC contactor', 'Contacteur CA')), rc))
add(across(relay_coil('-Q', C('Schütz AC (Varistor)', 'AC contactor (varistor)', 'Contacteur CA (varistance)')), varistor))
add(main_contact('-Q', C('Leistungsschließer', 'Main make contact', 'Contact principal à fermeture'), 'no', ('1', '2')))
add(main_contact('-Q', C('Leistungsöffner', 'Main break contact', 'Contact principal à ouverture'), 'nc', ('11', '12')))
add(tag(contact('no', C('Hilfsschließer', 'Auxiliary make contact', 'Contact auxiliaire à fermeture'), '-Q', ('13', '14'), numbers=True), -2.8, P + 0.4))
add(tag(contact('nc', C('Hilfsöffner', 'Auxiliary break contact', 'Contact auxiliaire à ouverture'), '-Q', ('11', '12'), numbers=True), -1.0, P + 0.4))
add(main_contacts('-Q', C('Leistungsschließer, dreipolig', 'Main make contacts, three-pole', 'Contacts principaux à fermeture, tripolaires')))
add(main_contacts('-Q', C('Schützblock', 'Contactor block', 'Bloc contacteur'), aux=True))

# --- Page "Sensoren"; pressure p and temperature ϑ in the box as in Rockwell 100-2.10 (German column)
add = page(IEC, 'Sensoren', 'Sensors', 'Capteurs')
for de, en, fr, sign in (('Schwimmerschalter', 'Float switch', 'Interrupteur à flotteur', float_),
                         ('Druckschalter', 'Pressure switch', 'Pressostat', boxed('p')),
                         ('Temperaturschalter', 'Temperature switch', 'Thermostat', boxed('ϑ')),
                         ('Thermoschalter', 'Thermal switch (bimetal)', 'Interrupteur thermique (bilame)', thermal),
                         ('Näherungsschalter', 'Proximity switch', 'Détecteur de proximité', proximity),
                         ('Berührungsschalter', 'Touch switch', 'Interrupteur tactile', touch),
                         ('Magnetschalter', 'Magnetic proximity switch', 'Détecteur de proximité magnétique', magnet)):
    both(de, en, fr, sign, prefix='-B')

# --- Page "Verbindungen"
add = page(IEC, 'Verbindungen', 'Connections', 'Connexions')
s = Symbol('-W', C('Hängekabel', 'Hanging cable', 'Câble suspendu'), numbered=False)
s.line((0, 0), (0, P)).arc(2 * P, P, 4 * P, 180, 360).line((4 * P, P), (4 * P, 0))
s.pin('1', (0, 0), label=(0.6, 0.2), shown=True).pin('2', (4 * P, 0), label=(4 * P - 0.6, 0.2), align='right', shown=True)
add(tag(s, -1.0))


def continuation(s):
    """A continuation mark: the wire ends in an arrow pointing to the continuation elsewhere."""
    s.line((0, 0), (P, 0)).poly((P, -1.0), (P, 1.0), (P + 1.8, 0))
    return s.pin('1', (0, 0))


s = continuation(Symbol('', C('Abbruchstelle', 'Continuation mark', 'Renvoi'), '-Abb', numbered=False, listed=False, shown=False,
                        value_shown=True))
add(s.text('/<PAGENO>', P + 2.4, 0.3, 2.0).labels((P + 2.4, -2.6), (P + 2.4, -2.6)))
s = continuation(child(Symbol(LINKED, C('Abbruchstelle mit Ziel', 'Continuation mark with target', 'Renvoi avec cible'), listed=False)))
add(s.labels((P + 2.4, -2.6), (P + 2.4, 0.3)))
s = Symbol('-A49.0', C('Gerätebezeichnung', 'Device designation', "Désignation d'appareil"), 'CUS-99', numbered=False, listed=False)
add(s.labels((0, 0), (0, 3.0)))

# sPlan's roles: these devices are Parents, so that contacts drawn elsewhere can be linked to them as children.
parents(IEC, 'Relais und -kontakte', 'Relais AC', 'Relais DC', 'Relais DC (A2 oben)', 'Stromstoßrelais', 'Blinkrelais',
        'Zeitrelais, rückfallverzögert', 'Zeitrelais, ansprechverzögert', 'Zeitrelais, ansprech- und rückfallverzögert')
parents(IEC, 'Schalter und Taster')
parents(IEC, 'Schutzeinrichtungen')
parents(IEC, 'Schütze und -kontakte', 'Schütz DC', 'Schütz DC (A2 oben)', 'Schütz AC', 'Schütz AC (Varistor)')
parents(IEC, 'Sensoren')
parents(IEC, 'Verbindungen', 'Abbruchstelle')
