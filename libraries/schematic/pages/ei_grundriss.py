# Floor plan elements for page "Grundriss": walls, windows, doors and furniture at a plan scale of 1:50 (1 m = 20 mm),
# drawn after DIN 1356-1 (walls cut and hatched, doors with leaf and swing arc, furniture seen from above in thin lines).
add = extend(EI, 'Grundriss')
M = 20.0           # one metre on the sheet
WALL = 0.24 * M    # 24 cm wall
THIN = 0.115 * M   # 11.5 cm partition wall
GREY = '#a0a0a0'


def element(caption, draw):
    s = Symbol('', caption, numbered=False, listed=False, shown=False)
    draw(s)
    return s


def hatched(s, x0, y0, x1, y1, step=1.2):
    """A wall cut: heavy outline, thin hatching at 45 degrees."""
    s.rect(x0, y0, x1, y1, w=THICK)
    c = x0 + y0 + step
    while c < x1 + y1:
        xa, xb = max(x0, c - y1), min(x1, c - y0)
        if xb - xa > 0.05:
            s.line((xa, c - xa), (xb, c - xb), w=0.18)
        c += step
    return s


def window(s, t, glass=False):
    """A wall piece of 2 m with a window of 1 m in its middle: the sill lines at both faces of the wall, the frame in
    the middle of the wall, the glazing as a filled band in the frame."""
    hatched(s, 0, 0, 0.5 * M, t)
    hatched(s, 1.5 * M, 0, 2 * M, t)
    s.line((0.5 * M, 0), (1.5 * M, 0)).line((0.5 * M, t), (1.5 * M, t))
    f = 0.06 * M if t > 3 else 0.05 * M
    s.rect(0.5 * M, (t - f) / 2, 1.5 * M, (t + f) / 2)
    if glass:
        s.rect(0.5 * M, t / 2 - 0.2, 1.5 * M, t / 2 + 0.2, w=0.18, fill='#000000')
    return s


def door(s, leaves, frame=False):
    """A door in a 24 cm wall, origin at the outer face of the left jamb, the leaves opening into the room (below):
    single 1 m, double 2 x 0.75 m. With frame: the frame at both jambs filled grey, the threshold dashed."""
    width = 1.0 * M if leaves == 1 else 1.5 * M
    s.line((0, 0), (0, WALL), w=THICK).line((width, 0), (width, WALL), w=THICK)
    z = 0.05 * M if frame else 0
    if frame:
        s.rect(0, 0, z, WALL, fill=GREY).rect(width - z, 0, width, WALL, fill=GREY)
        dashed(s, (z, WALL / 2), (width - z, WALL / 2))
    leaf = (width - 2 * z) / leaves
    s.line((z, WALL), (z, WALL + leaf), w=0.35).arc(z, WALL, 2 * leaf, 270, 360)
    if leaves == 2:
        s.line((width - z, WALL), (width - z, WALL + leaf), w=0.35).arc(width - z, WALL, 2 * leaf, 180, 270)
    return s


def chair(s, x, y, angle):
    """A chair 45 x 45 cm about (x, y), its backrest towards `angle` (0: to the right, counter-clockwise)."""
    h, b = 0.225 * M, 0.225 * M - 0.07 * M
    seat = [rot(p, (0, 0), angle) for p in ((-h, -h), (h, -h), (h, h), (-h, h))]
    back = [rot(p, (0, 0), angle) for p in ((b, -h), (b, h))]
    s.poly(*[(x + p[0], y + p[1]) for p in seat], fill=None)
    s.line(*[(x + p[0], y + p[1]) for p in back])
    return s


GAP = 0.04 * M      # between table edge and chair
SEAT = 0.225 * M    # half a chair


def square_table(s):
    a = 0.4 * M
    s.rect(-a, -a, a, a)
    for k in range(4):
        x, y = rot((a + GAP + SEAT, 0), (0, 0), 90 * k)
        chair(s, x, y, 90 * k)
    return s


def long_table(s):
    a, b = 0.8 * M, 0.45 * M
    s.rect(-a, -b, a, b)
    for x in (-0.4 * M, 0.4 * M):
        chair(s, x, -(b + GAP + SEAT), 90)
        chair(s, x, b + GAP + SEAT, 270)
    chair(s, a + GAP + SEAT, 0, 0)
    chair(s, -(a + GAP + SEAT), 0, 180)
    return s


def round_table(s, n, d):
    s.circle(0, 0, d)
    for k in range(n):
        x, y = rot((d / 2 + GAP + SEAT, 0), (0, 0), 90 + 360 / n * k)
        chair(s, x, y, 90 + 360 / n * k)
    return s


def armchair(s):
    a, back, arm = 0.4 * M, 0.15 * M, 0.13 * M
    s.rect(-a, -a, a, a, corner=10)
    s.line((-a, -a + back), (a, -a + back))
    s.line((-a + arm, -a + back), (-a + arm, a)).line((a - arm, -a + back), (a - arm, a))
    return s


def sofa(s, seats):
    a, d, back, arm = (0.75 if seats == 2 else 1.05) * M, 0.425 * M, 0.17 * M, 0.15 * M
    s.rect(-a, -d, a, d, corner=8)
    s.line((-a, -d + back), (a, -d + back))
    s.line((-a + arm, -d + back), (-a + arm, d)).line((a - arm, -d + back), (a - arm, d))
    cushion = (2 * a - 2 * arm) / seats
    for k in range(1, seats):
        x = -a + arm + k * cushion
        s.line((x, -d + back), (x, d))
    return s


def bed(s, width):
    a, d = width / 2, 1.0 * M
    s.rect(-a, -d, a, d)
    pillows = [0] if width < 1.2 * M else [-a / 2, a / 2]
    pw = min(0.7 * M, width / len(pillows) - 0.1 * M) / 2
    for x in pillows:
        s.rect(x - pw, -d + 0.08 * M, x + pw, -d + 0.48 * M, corner=30)
    s.line((-a, -d + 0.6 * M), (a, -d + 0.6 * M))
    s.line((a - 0.3 * M, -d + 0.6 * M), (a, -d + 0.9 * M))
    return s


add(element(C('Wand (senkrecht)', 'Wall (vertical)', 'Mur (vertical)'), lambda s: hatched(s, 0, 0, WALL, 2 * M)))
add(element(C('Wand (waagerecht)', 'Wall (horizontal)', 'Mur (horizontal)'), lambda s: hatched(s, 0, 0, 2 * M, WALL)))
add(element(C('Fenster', 'Window', 'Fenêtre'), lambda s: window(s, WALL)))
add(element(C('Fenster mit Glasscheibe', 'Window with glazing', 'Fenêtre avec vitrage'), lambda s: window(s, WALL, glass=True)))
add(element(C('Wand, dünn (senkrecht)', 'Partition wall (vertical)', 'Cloison (verticale)'), lambda s: hatched(s, 0, 0, THIN, 2 * M)))
add(element(C('Wand, dünn (waagerecht)', 'Partition wall (horizontal)', 'Cloison (horizontale)'), lambda s: hatched(s, 0, 0, 2 * M, THIN)))
add(element(C('Fenster (dünne Wand)', 'Window (partition wall)', 'Fenêtre (cloison)'), lambda s: window(s, THIN)))
add(element(C('Fenster mit Glasscheibe (dünne Wand)', 'Window with glazing (partition wall)', 'Fenêtre avec vitrage (cloison)'),
            lambda s: window(s, THIN, glass=True)))
add(element(C('Tür (einflügelig)', 'Door (single leaf)', 'Porte (un vantail)'), lambda s: door(s, 1)))
add(element(C('Tür (zweiflügelig)', 'Door (double leaf)', 'Porte (deux vantaux)'), lambda s: door(s, 2)))
add(element(C('Tür mit Zarge (einflügelig)', 'Door with frame (single leaf)', 'Porte avec huisserie (un vantail)'), lambda s: door(s, 1, frame=True)))
add(element(C('Tür mit Zarge (zweiflügelig)', 'Door with frame (double leaf)', 'Porte avec huisserie (deux vantaux)'), lambda s: door(s, 2, frame=True)))
add(element(C('Tisch quadratisch mit 4 Stühlen', 'Square table with 4 chairs', 'Table carrée avec 4 chaises'), square_table))
add(element(C('Tisch rechteckig mit 6 Stühlen', 'Rectangular table with 6 chairs', 'Table rectangulaire avec 6 chaises'), long_table))
add(element(C('Tisch rund mit 4 Stühlen', 'Round table with 4 chairs', 'Table ronde avec 4 chaises'), lambda s: round_table(s, 4, 1.0 * M)))
add(element(C('Tisch rund mit 6 Stühlen', 'Round table with 6 chairs', 'Table ronde avec 6 chaises'), lambda s: round_table(s, 6, 1.2 * M)))
add(element(C('Sessel', 'Armchair', 'Fauteuil'), armchair))
add(element(C('Sofa (zweisitzig)', 'Sofa (two-seater)', 'Canapé (deux places)'), lambda s: sofa(s, 2)))
add(element(C('Sofa (dreisitzig)', 'Sofa (three-seater)', 'Canapé (trois places)'), lambda s: sofa(s, 3)))
add(element(C('Bett', 'Single bed', 'Lit simple'), lambda s: bed(s, 0.9 * M)))
add(element(C('Doppelbett', 'Double bed', 'Lit double'), lambda s: bed(s, 1.8 * M)))
