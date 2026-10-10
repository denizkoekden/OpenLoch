# Switches with their operating devices for the pages "Schalter (andere Betätigungen)" and "Schalter (drücken)": contact
# functions after DIN EN 60617-7, operating signs, detent and mechanical link after DIN EN 60617-2 (forms checked against
# the public tables of the standard). Contacts stand vertically like contact(); the terminal numbers are shown.

LINK_Y = 2 * P - 0.5     # height of the mechanical link across the blades
SIGN_X = -4.4            # where the link meets the operating sign
LATCH_X = -6.6           # the same with a detent in the link


def blade(s, kind, x=0.0, pins=('1', '2'), raised=False):
    """A make ('no') or break ('nc') contact standing at x, with the geometry of contact(); returns the point where the
    link meets the blade. `raised` lifts the upper terminal number clear of a mark at the fixed contact."""
    top, bottom = (x, 0), (x, 4 * P)
    s.pin(pins[0], top, label=(x + 0.9, -0.6) if raised else (x + 0.8, 0.2), shown=True).pin(pins[1], bottom, label=(x + 0.8, 4 * P - 2.4), shown=True)
    s.line(top, (x, P)).line((x, 3 * P), bottom)
    if kind == 'no':
        tip = (x - 1.9, P + 0.6)
    else:
        s.line((x, P), (x + 1.6, P))
        tip = (x + 2.1, P - 0.4)
    s.line((x, 3 * P), tip)
    t = (3 * P - LINK_Y) / (3 * P - tip[1])
    return (x + (tip[0] - x) * t, LINK_Y)


def link(s, start, latch=False):
    """The dashed link from the blade at `start` to the operating sign, with a detent (V notch) when latching."""
    x, y = start
    if not latch:
        return dashed(s, (x, y), (SIGN_X, y))
    c = (x + LATCH_X) / 2
    dashed(s, (x, y), (c + 0.7, y))
    s.line((c + 0.7, y), (c, y + 0.9), (c - 0.7, y))
    return dashed(s, (c - 0.7, y), (LATCH_X, y))


# Operating signs: the link arrives from the right at (x, y).
def manual(s, x, y):
    s.line((x, y - 1.2), (x, y + 1.2))


def push(s, x, y):
    s.line((x + 0.8, y - 1.2), (x, y - 1.2), (x, y + 1.2), (x + 0.8, y + 1.2))


def pull(s, x, y):
    s.line((x - 0.8, y - 1.2), (x, y - 1.2), (x, y + 1.2), (x - 0.8, y + 1.2))


def turn(s, x, y):
    s.line((x - 0.8, y - 1.2), (x, y - 1.2), (x, y + 1.2), (x + 0.8, y + 1.2))


def toggle(s, x, y):
    s.line((x - 0.8, y - 1.2), (x, y), (x - 0.8, y + 1.2))


def roller(s, x, y):
    s.circle(x - 0.9, y, 1.8)


def cam(s, x, y, r=1.3):
    s.arc(x - r, y, 2 * r, 90, 360).line((x, y), (x - r, y), (x - r, y - r))


def key(s, x, y):
    c = x - 0.47
    s.circle(c, y - 1.2, 1.2).poly((c - 0.35, y - 0.6), (c + 0.35, y - 0.6), (c + 0.75, y + 1.4), (c - 0.75, y + 1.4), fill=None)


def float_(s, x, y):
    s.parts.append({'type': 'ellipse', 'centre': pt((x - 1.2, y)), 'size': [2.4, 1.4], 'pen': {'width': W}})
    s.line((x - 1.2, y - 0.7), (x - 1.2, y - 1.7))


def mushroom(s, x, y):
    s.arc(x, y, 1.6, 90, 270, dy=3.0).line((x, y - 1.5), (x, y + 1.5))


def proximity(s, x, y, touch=False, h=1.4):
    c = x - h
    s.poly((x, y), (c, y - h), (x - 2 * h, y), (c, y + h), fill=None)
    for dx in (-0.3, 0.3):
        s.line((c + dx, y - h + 0.3), (c + dx, y + h - 0.3))
    if touch:
        s.line((x - 2 * h, y - h), (x - 2 * h, y + h))


def touch(s, x, y):
    proximity(s, x, y, touch=True)


def switch(kind, caption, sign, latch=False):
    """A single contact 1-2 operated by `sign` over a dashed link."""
    s = Symbol('S', caption)
    link(s, blade(s, kind), latch)
    sign(s, LATCH_X if latch else SIGN_X, LINK_Y)
    return s.labels((2.6, P + 0.4), (2.6, 2 * P + 0.6))


def pair(caption, first, second, sign, latch=False, gap=2 * P):
    """Two contacts on one link: `first` and `second` are (kind, pins); the second stands `gap` to the right."""
    s = Symbol('S', caption)
    a = blade(s, first[0], 0, first[1])
    b = blade(s, second[0], gap, second[1])
    link(s, a, latch)
    dashed(s, a, b)
    sign(s, LATCH_X if latch else SIGN_X, LINK_Y)
    return s.labels((gap + 2.6, P + 0.4), (gap + 2.6, 2 * P + 0.6))


def position(kind, caption, prefix='S'):
    """A position switch: the open triangle of the position-switch function on the blade."""
    s = Symbol(prefix, caption)
    blade(s, kind)
    tip = (-1.9, P + 0.6) if kind == 'no' else (2.1, P - 0.4)
    length = math.hypot(tip[0], 3 * P - tip[1])
    u = (tip[0] / length, (tip[1] - 3 * P) / length)
    n = (u[1], -u[0])
    base = (0.45 * tip[0], 3 * P + 0.45 * (tip[1] - 3 * P))
    triangle(s, (base[0] + 1.1 * n[0], base[1] + 1.1 * n[1]), base, 0.7, filled=False)
    return s.labels((2.6, P + 0.4), (2.6, 2 * P + 0.6))


add = extend(EI, 'Schalter (andere Betätigungen)')
for de, en, fr, sign, latch in (
        ('Drehschalter', 'Rotary switch', 'Commutateur rotatif', turn, False),
        ('Drehschalter mit Rastung', 'Rotary switch, latching', 'Commutateur rotatif à accrochage', turn, True),
        ('Kippschalter', 'Toggle switch', 'Interrupteur à bascule', toggle, False),
        ('Zugschalter', 'Pull switch', 'Interrupteur à tirette', pull, False)):
    add(switch('no', C(de + ' (Schließer)', en + ' (make)', fr + ' (fermeture)'), sign, latch))
    add(switch('nc', C(de + ' (Öffner)', en + ' (break)', fr + ' (ouverture)'), sign, latch))

s = Symbol('S', C('Leistungsschalter (dreipolig)', 'Circuit breaker, three-pole', 'Disjoncteur tripolaire'))
points = []
for k in range(3):
    x = 2 * k * P
    points.append(blade(s, 'no', x, (str(2 * k + 1), str(2 * k + 2)), raised=True))
    s.line((x - 0.8, P - 0.8), (x + 0.8, P + 0.8)).line((x - 0.8, P + 0.8), (x + 0.8, P - 0.8))
link(s, points[0], latch=True)
dashed(s, points[0], points[2])
turn(s, LATCH_X, LINK_Y)
add(s.labels((4 * P + 1.4, P + 1.2), (4 * P + 1.4, 2 * P + 1.4)))

for de, en, fr, sign in (
        ('Näherungsschalter', 'Proximity switch', 'Détecteur de proximité', proximity),
        ('Berührungsschalter', 'Touch switch', 'Interrupteur tactile', touch),
        ('Rollenschalter', 'Roller switch', 'Interrupteur à galet', roller),
        ('Schlüsselschalter', 'Key switch', 'Interrupteur à clé', key),
        ('Schwimmerschalter', 'Float switch', 'Interrupteur à flotteur', float_)):
    add(switch('no', C(de + ' (Schließer)', en + ' (make)', fr + ' (fermeture)'), sign))
    add(switch('nc', C(de + ' (Öffner)', en + ' (break)', fr + ' (ouverture)'), sign))
add(switch('no', C('Not-Aus-Schalter (Schließer)', 'Emergency stop switch (make)', "Arrêt d'urgence (fermeture)"), mushroom, latch=True))
add(pair(C('Not-Aus-Schalter (Öffner und Schließer)', 'Emergency stop switch (break and make)',
           "Arrêt d'urgence (ouverture et fermeture)"), ('nc', ('1', '2')), ('no', ('3', '4')), mushroom, latch=True, gap=3 * P))
add(switch('no', C('Nockenschalter (Schließer)', 'Cam switch (make)', 'Interrupteur à came (fermeture)'), cam))
add(switch('nc', C('Nockenschalter (Öffner)', 'Cam switch (break)', 'Interrupteur à came (ouverture)'), cam))
add(position('no', C('Positionsschalter (Schließer)', 'Position switch (make)', 'Interrupteur de position (fermeture)')))
add(position('nc', C('Positionsschalter (Öffner)', 'Position switch (break)', 'Interrupteur de position (ouverture)')))

add = extend(EI, 'Schalter (drücken)')
add(switch('no', C('Handschalter (Schließer)', 'Manual switch (make)', 'Interrupteur manuel (fermeture)'), manual))
add(switch('nc', C('Handschalter (Öffner)', 'Manual switch (break)', 'Interrupteur manuel (ouverture)'), manual))
add(switch('no', C('Druckschalter mit Rastung (Schließer)', 'Push switch, latching (make)',
                   'Interrupteur poussoir à accrochage (fermeture)'), push, latch=True))
add(switch('nc', C('Druckschalter mit Rastung (Öffner)', 'Push switch, latching (break)',
                   'Interrupteur poussoir à accrochage (ouverture)'), push, latch=True))
add(pair(C('Druckschalter mit Rastung (Schließer und Öffner)', 'Push switch, latching (make and break)',
           'Interrupteur poussoir à accrochage (fermeture et ouverture)'), ('no', ('1', '3')), ('nc', ('2', '4')), push, latch=True))
add(pair(C('Taster (Schließer und Öffner)', 'Push button (make and break)', 'Bouton-poussoir (travail et repos)'),
         ('no', ('1', '3')), ('nc', ('2', '4')), push))
