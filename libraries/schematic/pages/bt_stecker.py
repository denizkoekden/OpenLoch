# More plugs, sockets and terminals for "Stecker, Buchsen, Klemmen": circuit symbols after DIN EN 60617-3 in the style of
# the page; front views of connectors (mating face, socket contacts as circles, pins as dots, no connection points) after
# the USB 2.0 specification and its Mini-B ECN, DIN 41652 / IEC 60807 (HARTING and ITT Cannon D-Sub catalogues),
# IEC 60268-12 (Neutrik XX drawings), DIN 41524 / 45322 / 45326 / 45327 / 45329 (Lumberg 0131, Same Sky SD and SDS-J),
# EN 50049 (SCART) and DIN 41715 (TAE); terminals and headers seen from above with their legs on the 2.54 mm pitch.
add = extend(BT, 'Stecker, Buchsen, Klemmen')
LIGHT = '#e4e4e4'
FIELD = '#c8c8c8'
METAL = '#d0d0d0'


def smooth(points, radius, steps=5):
    """The closed polygon `points` with its corners rounded (one radius for all corners or one per corner)."""
    radii = radius if isinstance(radius, (list, tuple)) else [radius] * len(points)
    unit = lambda x, y: (x / math.hypot(x, y), y / math.hypot(x, y))
    out = []
    for k, (vx, vy) in enumerate(points):
        rr = radii[k]
        if not rr:
            out.append((vx, vy))
            continue
        ax, ay = points[k - 1]
        bx, by = points[(k + 1) % len(points)]
        u1, u2 = unit(ax - vx, ay - vy), unit(bx - vx, by - vy)
        half = math.acos(max(-1.0, min(1.0, u1[0] * u2[0] + u1[1] * u2[1]))) / 2
        d, h = rr / math.tan(half), rr / math.sin(half)
        bis = unit(u1[0] + u2[0], u1[1] + u2[1])
        c = (vx + bis[0] * h, vy + bis[1] * h)
        a1 = math.atan2(vy + u1[1] * d - c[1], vx + u1[0] * d - c[0])
        a2 = math.atan2(vy + u2[1] * d - c[1], vx + u2[0] * d - c[0])
        da = (a2 - a1 + math.pi) % (2 * math.pi) - math.pi
        out += [(c[0] + rr * math.cos(a1 + da * j / steps), c[1] + rr * math.sin(a1 + da * j / steps)) for j in range(steps + 1)]
    return out


def outline(s, points, radius=0, w=W, fill=None):
    s.poly(*(smooth(points, radius) if radius else points), fill=fill, w=w)
    return s


def slotted_screw(s, x, y, d, fill=None):
    """A screw head from above with its slot at 45 degrees."""
    s.circle(x, y, d, fill=fill)
    half = d / 2 - 0.25
    for o in (-0.3, 0.3):
        s.line((x - half * 0.707 + o * 0.707, y + half * 0.707 + o * 0.707), (x + half * 0.707 + o * 0.707, y - half * 0.707 + o * 0.707))
    return s


# DC barrel connector polarity (IEC 60417-5926): the centre pin (dot) to plus, the sleeve (C) to minus.
s = Symbol('', C('Hohlstecker-Polarität (Mitte positiv)', 'DC plug polarity (centre positive)', "Polarité de la fiche d'alimentation (centre positif)"),
           numbered=False, listed=False, shown=False)
for x, plus in ((-7.4, False), (7.4, True)):
    s.circle(x, 0, 3.4).line((x - 1.0, 0), (x + 1.0, 0))
    if plus:
        s.line((x, -1.0), (x, 1.0))
s.line((-5.7, 0), (-2.0, 0)).arc(0, 0, 4.0, 45, 315).circle(0, 0, 1.2, fill='#000000').line((0, 0), (5.7, 0))
add(s.labels((-9.0, -4.6), None))


# Jumpers: pairs of square pins on the pitch, each pair bridged by a cap; the pins numbered from 0 on the board print.
def jumper(rows):
    size = '1×2' if rows == 1 else f'2×{rows}'
    s = Symbol('X', C(f'Jumper {size}', f'Jumper {size}', f'Cavalier {size}'))
    for r in range(rows):
        y = r * P
        s.rect(-P / 2 + 0.15, y - 1.15, 1.5 * P - 0.15, y + 1.15, corner=45)
        for c in range(2):
            x, k = c * P, 2 * r + c
            s.rect(x - 0.4, y - 0.4, x + 0.4, y + 0.4, w=0.1, fill='#000000').pin(str(k + 1), (x, y))
            if rows <= 2:
                s.text(str(k), x, y - 3.1 if r == 0 else y + 1.5, 1.6, 'centre')
            else:
                s.text(str(k), -P / 2 - 0.5 if c == 0 else 1.5 * P + 0.5, y - 0.8, 1.6, 'right' if c == 0 else 'left')
    right = 1.5 * P + (2.8 if rows > 2 else 1.0)
    return s.labels((right, -1.6), (right, 1.4))


for rows in (1, 2, 3, 4, 8):
    add(jumper(rows))


# USB front views after the USB 2.0 specification (figures 6-7 to 6-10, table 6-1) and the Mini-B ECN (figures 6-11 and
# 6-15, table 6-2), drawn 2:1. Receptacle and plug faces are mirror images.
USB = ('VBUS', 'D−', 'D+', 'GND')


def usb_contact(s, x, y0, y1, female, number, h=1.6, below=True, name=None):
    s.rect(x - 1.0, y0, x + 1.0, y1, w=0.18, fill=None if female else '#000000')
    s.text(number if name is None else number + ' ' + name, x, y1 + 0.35 if below else y0 - 0.35 - h, h, 'centre')


def usb_a(female):
    s = Symbol('X', C(f'USB A {"Buchse" if female else "Stecker"} (Ansicht)', f'USB A {"receptacle" if female else "plug"} (front view)',
                      f'{"Embase" if female else "Fiche"} USB A (vue de face)'))
    if female:      # opening 12.5 x 5.12, the tongue at the top with the contacts on its underside: 1 2 3 4 from the left
        outline(s, [(-13.1, -5.72), (13.1, -5.72), (13.1, 5.72), (-13.1, 5.72)], 0.8, w=0.35)
        outline(s, [(-12.5, -5.12), (12.5, -5.12), (12.5, 5.12), (-12.5, 5.12)], 0.6)
        s.rect(-11.1, -5.12, 11.1, -1.44, fill=LIGHT)
        order, y0, y1, bottom = (1, 2, 3, 4), -1.44, -0.64, 5.72
    else:           # shell 12.0 x 4.5, the insulator below with the contacts on top: 4 3 2 1 from the left
        outline(s, [(-12.0, -4.5), (12.0, -4.5), (12.0, 4.5), (-12.0, 4.5)], 0.8, w=0.35)
        outline(s, [(-11.4, -3.9), (11.4, -3.9), (11.4, 3.9), (-11.4, 3.9)], 0.5)
        s.rect(-11.4, 0, 11.4, 3.9, fill=LIGHT)
        order, y0, y1, bottom = (4, 3, 2, 1), -0.8, 0, 4.5
    for x, n in zip((-7.0, -2.0, 2.0, 7.0), order):
        usb_contact(s, x, y0, y1, female, str(n))
        s.text(USB[n - 1], x, bottom + 0.6, 1.6, 'centre')
    return s.labels((-bottom * 2.3, -bottom - 3.4), (-bottom * 2.3 + 8.0, -bottom - 3.4))


def usb_b(female):
    s = Symbol('X', C(f'USB B {"Buchse" if female else "Stecker"} (Ansicht)', f'USB B {"receptacle" if female else "plug"} (front view)',
                      f'{"Embase" if female else "Fiche"} USB B (vue de face)'))
    if female:      # opening 8.45 x 7.78 with chamfered upper corners, the tongue in the middle: 2 1 above, 3 4 below
        hw, hh, ch, top, low = 8.45, 7.78, 3.2, (2, 1), (3, 4)
        outline(s, [(-hw - 0.6, hh + 0.6), (-hw - 0.6, -hh + ch - 0.35), (-hw + ch - 0.35, -hh - 0.6), (hw - ch + 0.35, -hh - 0.6),
                    (hw + 0.6, -hh + ch - 0.35), (hw + 0.6, hh + 0.6)], 0.6, w=0.35)
        tw, th = 5.6, 3.18
        s.rect(-tw, -th, tw, th, fill=LIGHT)
    else:           # shell 8.00 x 7.26, the cavity for the tongue with the contacts on its walls: 1 2 above, 4 3 below
        hw, hh, ch, top, low = 8.0, 7.26, 2.92, (1, 2), (4, 3)
        outline(s, [(-hw, hh), (-hw, -hh + ch), (-hw + ch, -hh), (hw - ch, -hh), (hw, -hh + ch), (hw, hh)], 0.6, w=0.35)
        i, ci = 0.6, ch - 0.25
        outline(s, [(-hw + i, hh - i), (-hw + i, -hh + i + ci), (-hw + i + ci, -hh + i), (hw - i - ci, -hh + i), (hw - i, -hh + i + ci),
                    (hw - i, hh - i)], 0.4, fill=LIGHT)
        tw, th = 5.83, 3.29
        s.rect(-tw, -th, tw, th, fill='#ffffff')
    if female:
        outline(s, [(-hw, hh), (-hw, -hh + ch), (-hw + ch, -hh), (hw - ch, -hh), (hw, -hh + ch), (hw, hh)], 0.5)
    for x, n in zip((-2.5, 2.5), top):
        usb_contact(s, x, -th, -th + 0.8, female, str(n), 1.4, True, USB[n - 1])
    for x, n in zip((-2.5, 2.5), low):
        usb_contact(s, x, th - 0.8, th, female, str(n), 1.4, False, USB[n - 1])
    return s.labels((-hw - 0.6, -hh - 4.0), (-hw + 7.4, -hh - 4.0))


def usb_mini(female):
    s = Symbol('X', C(f'USB Mini-B {"Buchse" if female else "Stecker"} (Ansicht)', f'USB Mini-B {"receptacle" if female else "plug"} (front view)',
                      f'{"Embase" if female else "Fiche"} USB Mini-B (vue de face)'))
    names = ('VBUS', 'D−', 'D+', 'ID', 'GND')
    xs = (-3.2, -1.6, 0, 1.6, 3.2)
    if female:      # opening 6.9 x 3.1, lower corners bevelled; the tongue carries contacts 1 to 5 from the left on top
        hw, hh, ch = 6.9, 3.1, 1.03
        outline(s, [(-hw - 0.6, -hh - 0.6), (hw + 0.6, -hh - 0.6), (hw + 0.6, hh - ch + 0.35), (hw - ch + 0.35, hh + 0.6),
                    (-hw + ch - 0.35, hh + 0.6), (-hw - 0.6, hh - ch + 0.35)], 0.4, w=0.35)
        outline(s, [(-hw, -hh), (hw, -hh), (hw, hh - ch), (hw - ch, hh), (-hw + ch, hh), (-hw, hh - ch)], 0.3)
        outline(s, [(-4.6, -1.0), (4.6, -1.0), (4.6, 1.1), (3.51, 2.2), (-3.51, 2.2), (-4.6, 1.1)], 0, fill=LIGHT)
        for x, n in zip(xs, range(1, 6)):
            s.rect(x - 0.4, -1.0, x + 0.4, -0.2, w=0.15).text(str(n), x, 0.1, 1.2, 'centre')
    else:           # shell 6.8 x 3.0, lower corners bevelled; the insulator above, contacts 5 to 1 from the left on its underside
        hw, hh, ch = 6.8, 3.0, 1.13
        outline(s, [(-hw, -hh), (hw, -hh), (hw, hh - ch), (hw - ch, hh), (-hw + ch, hh), (-hw, hh - ch)], 0.4, w=0.35)
        outline(s, [(-hw + 0.6, -hh + 0.6), (hw - 0.6, -hh + 0.6), (hw - 0.6, -0.6), (-hw + 0.6, -0.6)], 0, fill=LIGHT)
        for x, n in zip(xs, range(5, 0, -1)):
            s.rect(x - 0.4, -0.6, x + 0.4, 0.2, w=0.15, fill='#000000').text(str(n), x, 0.55, 1.2, 'centre')
    for n, name in enumerate(names):
        s.text(f'{n + 1} {name}', hw + 2.0, -4.6 + n * 2.0, 1.6)
    return s.labels((-hw - 0.6, -hh - 4.0), (-hw + 7.4, -hh - 4.0))


add(usb_a(False))
add(usb_a(True))
add(usb_b(False))
add(usb_b(True))
add(usb_mini(False))
add(usb_mini(True))

s = Symbol('X', C('Schraubklemme (Draufsicht)', 'Screw terminal (top view)', 'Borne à vis (vue de dessus)'))
add(slotted_screw(s.rect(-P, -P, P, P), 0, 0, 3.8).labels((P + 0.8, -P), (P + 0.8, -P + 3.0)))


# BNC: the circle is the outer conductor, the dot the centre pin (the open circle the centre socket); 1 inner, 2 shield.
def bnc(plug, down):
    s = Symbol('X', C(f'BNC-{"Stecker" if plug else "Buchse"}' + (' (Abgang unten)' if down else ''),
                      f'BNC {"plug" if plug else "socket"}' + (' (shield lead down)' if down else ''),
                      f'{"Fiche" if plug else "Embase"} BNC' + (' (blindage vers le bas)' if down else '')))
    c = 2 * P
    s.circle(c, 0, 2 * P)
    if plug:
        s.circle(c, 0, 1.2, fill='#000000').line((0, 0), (c, 0))
    else:
        s.circle(c, 0, 1.2).line((0, 0), (c - 0.6, 0))
    if down:
        s.line((c, P), (c, 2 * P)).pin('2', (c, 2 * P), label=(c + 0.4, 2 * P - 2.2))
    else:
        s.line((0, P), (c, P)).pin('2', (0, P))
    return s.pin('1', (0, 0)).labels((3 * P + 0.6, -P - 1.0), (3 * P + 0.6, -P + 2.0))


for plug, down in ((True, False), (False, False), (True, True), (False, True)):
    add(bnc(plug, down))


# TAE sockets (DIN 41715) seen from the front: contacts 3 2 1 down the left wall of the slot, 4 5 6 down the right one; the
# coding bar of N between the upper two rows, that of F below the lowest. Size indicative.
def tae(coding):
    s = Symbol('X', C(f'TAE-Dose {coding}-codiert (Ansicht)', f'TAE socket, {coding}-coded (front view)', f'Prise TAE codée {coding} (vue de face)'))
    s.rect(-7.0, -12.0, 7.0, 14.4, corner=12)
    half, top, bottom = 2.5, -9.0, 10.5
    b0, b1 = (-3.8, -2.2) if coding == 'N' else (7.6, 9.2)
    outline(s, [(-half, top), (half, top), (half, b0), (half + 1.5, b0), (half + 1.5, b1), (half, b1), (half, bottom), (-half, bottom),
                (-half, b1), (-half - 1.5, b1), (-half - 1.5, b0), (-half, b0)], w=0.35)
    for y, left, right in ((-6.0, 3, 4), (0.0, 2, 5), (6.0, 1, 6)):
        s.circle(-1.6, y, 1.4).circle(1.6, y, 1.4)
        s.text(str(left), -half - 0.6, y - 0.9, 1.8, 'right').text(str(right), half + 0.6, y - 0.9, 1.8)
    s.text(coding, 0, 11.4, 2.0, 'centre')
    return s.labels((7.8, -12.0), (7.8, -9.0))


add(tae('N'))
add(tae('F'))


# D-sub after DIN 41652 / IEC 60807: shell sizes after the ITT Cannon catalogue (flange A, hole spacing C, D outside B x D,
# sides at 10 degrees), contacts 2.77 mm apart in rows 2.84 mm apart; on the male face No. 1 sits top left, on the female
# face top right (HARTING D-Sub catalogue). With flange 1:1, the shell alone 2:1.
DSUB = {9: (5, 30.81, 24.99, 16.92, 16.33), 15: (8, 39.14, 33.32, 25.25, 24.66), 25: (13, 53.04, 47.04, 38.96, 38.38)}


def dsub(n, female, flange):
    top, a, c, b_male, b_female = DSUB[n]
    kind = ('Buchse', 'female', 'femelle') if female else ('Stecker', 'male', 'mâle')
    view = ('Ansicht mit Befestigung', 'front view with flange', 'vue de face avec bride') if flange else ('Ansicht', 'front view', 'vue de face')
    s = Symbol('X', C(f'Sub-D {n} {kind[0]} ({view[0]})', f'D-sub {n} {kind[1]} ({view[1]})', f'Sub-D {n} {kind[2]} ({view[2]})'))
    k = 1.0 if flange else 2.0
    b, d = (b_female, 7.90) if female else (b_male, 8.36)
    if flange:
        s.rect(-a / 2, -6.275, a / 2, 6.275, corner=8)
        for x in (-c / 2, c / 2):
            s.circle(x, 0, 3.05)
    for inset, w, rt, rb in ((0, 0.35, 1.0, 1.6), (0.4, W, 0.7, 1.3)):
        bb, dd = b - 2 * inset, d - 2 * inset
        t = dd * math.tan(math.radians(10))
        outline(s, [(-bb / 2 * k, -dd / 2 * k), (bb / 2 * k, -dd / 2 * k), ((bb / 2 - t) * k, dd / 2 * k), (-(bb / 2 - t) * k, dd / 2 * k)],
                [rt * k, rt * k, rb * k, rb * k], w=w)
    pitch, row, dot, h = 2.77 * k, 2.84 * k, 1.0 * k, 1.2 if flange else 2.0
    number = 1
    for upper, count in ((True, top), (False, top - 1)):
        y = -row / 2 if upper else row / 2
        for i in range(count):
            x = (i - (count - 1) / 2) * pitch * (-1 if female else 1)
            s.circle(x, y, dot, w=0.18, fill=None if female else '#000000')
            s.text(str(number), x, y - dot / 2 - 0.25 * k - h if upper else y + dot / 2 + 0.25 * k, h, 'centre')
            number += 1
    x0, y0 = (-a / 2, -6.275) if flange else (-b / 2 * k, -d / 2 * k)
    return s.labels((x0, y0 - 3.4), (x0 + 8.0, y0 - 3.4))


for flange in (True, False):
    for n in (9, 15, 25):
        add(dsub(n, True, flange))
        add(dsub(n, False, flange))


# Jack sockets in the style of "Klinkenbuchse (Stereo)": the block is the sleeve, the springs leave it to the right; a break
# contact runs up to the hook of its spring. Contacts numbered from the top.
def jack(caption, leads, springs, numbers=False):
    s = Symbol('X', caption)
    s.rect(P, -P, 2 * P, leads * P)
    for k in range(leads):
        s.line((0, k * P), (P, k * P)).pin(str(k + 1), (0, k * P), shown=numbers)
    for points in springs:
        s.line(*[(x * P, y * P) for x, y in points])
    return s.labels((P, -P - 3.4), (P, leads * P + 0.6))


TIP = ((2, -0.6), (4, -0.6), (4.4, -0.2), (4.8, -0.6))
TIP_BREAK = ((2, 0.4), (4.0, 0.4), (4.4, -0.2))
RING = ((2, 1.6), (3.4, 1.6), (3.8, 2.0), (4.2, 1.6))
RING_BREAK = ((2, 2.6), (3.4, 2.6), (3.8, 2.0))
add(jack(C('Klinkenbuchse (Mono)', 'Jack socket (mono)', 'Jack (mono)'), 2, (TIP,)))


# SCART (EN 50049) socket seen from the front, 1:1: the corner cut at the top left, contacts 20 ... 2 in the upper row and
# 21 ... 1 in the lower one (21 is the shell contact).
s = Symbol('X', C('SCART-Buchse (Ansicht)', 'SCART socket (front view)', 'Prise péritel (vue de face)'))
hw, hh = 23.6, 7.5
outline(s, [(-hw, -hh + 6.0), (-hw + 8.2, -hh), (hw, -hh), (hw, hh), (-hw, hh)], [0, 0, 0.8, 0.8, 0.8], w=0.35)
outline(s, [(-hw + 1.0, -hh + 6.6), (-hw + 8.7, -hh + 1.0), (hw - 1.0, -hh + 1.0), (hw - 1.0, hh - 1.0), (-hw + 1.0, hh - 1.0)], [0, 0, 0.5, 0.5, 0.5])
for i in range(11):
    for x, y, number in ((-20.0 + 4 * i, 2.4, 21 - 2 * i), (-18.0 + 4 * i, -2.4, 20 - 2 * i)):
        if number < 1:
            continue
        s.rect(x - 0.6, y - 1.2, x + 0.6, y + 1.2, w=0.18)
        s.text(str(number), x, -hh - 2.1 if y < 0 else hh + 0.5, 1.6, 'centre')
add(s.labels((-hw, -hh - 6.0), (-hw + 8.0, -hh - 6.0)))
add(jack(C('Klinkenbuchse (Mono, mit Schaltkontakt)', 'Jack socket (mono, with break contact)', 'Jack (mono, avec contact de coupure)'),
         3, (TIP, TIP_BREAK), True))
add(jack(C('Klinkenbuchse (Stereo, mit Schaltkontakten)', 'Jack socket (stereo, with break contacts)', 'Jack (stéréo, avec contacts de coupure)'),
         5, (TIP, TIP_BREAK, RING, RING_BREAK), True))


# Coaxial plug and socket: the half circle is the outer conductor around the centre pin (dot) or socket (circle), the
# tangential stroke its connection; 1 inner conductor, 2 outer conductor.
def coax(plug):
    s = Symbol('X', C('Koaxialstecker' if plug else 'Koaxialbuchse', 'Coaxial plug' if plug else 'Coaxial socket',
                      'Fiche coaxiale' if plug else 'Prise coaxiale'))
    s.arc(0, 0, 2 * P, 270, 450).line((0, P), (2 * P, P))
    if plug:
        s.circle(0, 0, 1.2, fill='#000000').line((0, 0), (2 * P, 0))
    else:
        s.circle(0, 0, 1.2).line((0.6, 0), (2 * P, 0))
    s.pin('1', (2 * P, 0), label=(2 * P - 0.4, -2.2), align='right').pin('2', (2 * P, P), label=(2 * P - 0.4, P - 2.2), align='right')
    return s.labels((-1.2, -P - 3.4), (-1.2, P + 0.8))


add(coax(True))
add(coax(False))


# XLR after IEC 60268-12 / IEC 61076-2-103, contact places after the Neutrik drawings ST-NC3FXX and ST-NC4FXX (female face,
# key at the top); the male face is the mirror image. Drawn 1.5:1.
XLR = {3: {1: (4.1, 0), 2: (-4.1, 0), 3: (0, 4.0)}, 4: {1: (4.15, 0), 2: (2.0, 3.65), 3: (-2.1, 2.85), 4: (-4.0, -0.95)}}


def xlr(n, female):
    s = Symbol('X', C(f'XLR-{n} {"Buchse" if female else "Stecker"} (Ansicht)', f'XLR {n}-pin {"female" if female else "male"} (front view)',
                      f'XLR {n} broches {"femelle" if female else "mâle"} (vue de face)'))
    k = 1.5
    if female:
        shell, r, a, depth = 15.75 * k / 2, 6.25 * k, 2.25 * k, 3.5 * k
        ang, y = math.degrees(math.acos(a / r)), -math.sqrt(r * r - a * a)
        s.circle(0, 0, 2 * shell, w=0.35).arc(0, 0, 2 * r, 180 - ang, 360 + ang)
        s.line((-a, y), (-a, -r + depth), (a, -r + depth), (a, y))
    else:
        shell = 19.0 * k / 2
        s.circle(0, 0, 2 * shell, w=0.35).circle(0, 0, 16.0 * k).circle(0, 0, 13.0 * k)
        s.rect(-2.1 * k, -8.0 * k, 2.1 * k, -6.75 * k)
    for number, (x, y) in XLR[n].items():
        x, y = x * k * (1 if female else -1), y * k
        s.circle(x, y, 1.6 * k, w=0.18, fill=None if female else '#000000')
        u = math.hypot(x, y)
        s.text(str(number), x + x / u * 2.3, y + y / u * 2.3 - 0.9, 1.8, 'centre')
    return s.labels((shell + 0.6, -shell + 1.0), (shell + 0.6, -shell + 4.0))


for n in (3, 4):
    add(xlr(n, True))
    add(xlr(n, False))


# Circular DIN connectors, 2:1: contacts on a 7.0 mm circle inside the 13.2 mm shield with its key at the top. The places
# are given for the female face (Lumberg 0131 pin configurations, Same Sky SDS-J; domino and 262 degrees after the
# Commons chart "DIN connector pinout"), as angles clockwise from the key; None is the centre contact, which on the
# 262 degree layout sits 0.9 mm towards the key. The male face is the mirror image.
DIN = {'180': {1: 90, 4: 135, 2: 180, 5: 225, 3: 270},
       '240': {1: 60, 2: 120, 3: 180, 4: 240, 5: 300},
       'domino': {1: None, 2: 315, 3: 225, 4: 135, 5: 45},
       '6': {1: 60, 2: 120, 3: 180, 4: 240, 5: 300, 6: None},
       '262': {1: 90, 4: 135, 2: 180, 5: 225, 3: 270, 7: 49, 8: 311, 6: None},
       '270': {1: 90, 4: 135, 2: 180, 5: 225, 3: 270, 6: 45, 7: 315}}


def din(layout, female, de, en, fr):
    s = Symbol('X', C(f'DIN-{"Buchse" if female else "Stecker"} {de} (Ansicht)', f'DIN {"socket" if female else "plug"}, {en} (front view)',
                      f'{"Embase" if female else "Fiche"} DIN {fr} (vue de face)'))
    k = 2.0
    big, notch = 6.6 * k, 0.9 * k
    y = notch * notch / (2 * big) - big
    x = math.sqrt(big * big - y * y)
    a_big, a_notch = math.degrees(math.atan2(-y, x)), math.degrees(math.atan2(-(y + big), x))
    s.arc(0, 0, 2 * big, 180 - a_big, 360 + a_big, w=0.35).arc(0, -big, 2 * notch, 180 - a_notch, 360 + a_notch)
    for number, angle in DIN[layout].items():
        if angle is None:
            px, py = 0, -(0.9 if layout == '262' else 0) * k
            tx, ty = px, py - 1.45 * k / 2 - 0.4 - 2.0
        else:
            a = math.radians(angle)
            px, py = 3.5 * k * math.sin(a), -3.5 * k * math.cos(a)
            tx, ty = 10.2 * math.sin(a), -10.2 * math.cos(a) - 1.0
        if not female:
            px, tx = -px, -tx
        s.circle(px, py, 1.45 * k, w=0.18, fill=None if female else '#000000')
        s.text(str(number), tx, ty, 2.0, 'centre')
    return s.labels((big + 0.6, -big + 1.0), (big + 0.6, -big + 4.0))


for layout, de, en, fr in (('180', '5-polig 180°', '5-pin 180°', '5 broches 180°'), ('240', '5-polig 240°', '5-pin 240°', '5 broches 240°'),
                           ('domino', '5-polig Domino', '5-pin domino', '5 broches domino'), ('6', '6-polig', '6-pin', '6 broches')):
    add(din(layout, True, de, en, fr))
    add(din(layout, False, de, en, fr))
add(din('262', True, '8-polig 262°', '8-pin 262°', '8 broches 262°'))
add(din('270', True, '7-polig 270°', '7-pin 270°', '7 broches 270°'))


# Jack plugs seen from the side (about 2:1 of a 3.5 mm plug): tip, insulating rings, sleeve, the body and the cable.
def plug_side(stereo):
    s = Symbol('X', C(f'Klinkenstecker {"Stereo" if stereo else "Mono"} (Ansicht)', f'Jack plug, {"stereo" if stereo else "mono"} (side view)',
                      f'Fiche jack {"stéréo" if stereo else "mono"} (vue de côté)'))
    r = 2.8
    zones = ((0, 6.0), (7.2, 12.4), (13.6, 24.0)) if stereo else ((0, 7.0), (8.4, 24.0))
    t1 = zones[0][1]
    s.poly((0, 0), (0.5, -1.6), (1.6, -2.5), (2.6, -r), (t1, -r), (t1, r), (2.6, r), (1.6, 2.5), (0.5, 1.6), fill=METAL)
    for (x0, x1), (n0, n1) in zip(zones, zones[1:]):
        s.rect(x1, -r, n0, r, fill='#000000').rect(n0, -r, n1, r, fill=METAL)
    s.rect(24.0, -4.6, 25.6, 4.6)
    outline(s, [(25.6, -4.1), (44.0, -4.1), (44.0, 4.1), (25.6, 4.1)], [0, 1.2, 1.2, 0])
    outline(s, [(44.0, -3.2), (50.0, -2.1), (50.0, 2.1), (44.0, 3.2)])
    s.line((50.0, -1.6), (57.0, -1.6)).line((50.0, 1.6), (57.0, 1.6))
    if stereo:
        for x, mark in ((3.4, 'L'), (9.8, 'R')):
            s.text(mark, x, -r - 2.6, 2.0, 'centre')
        s.line((18.8, -r - 2.6), (18.8, -r - 0.9)).line((17.8, -r - 0.9), (19.8, -r - 0.9))
    return s.labels((0, -r - 6.4), (8.0, -r - 6.4))


add(plug_side(False))
add(plug_side(True))

# Stereo jack plug: tip, ring and sleeve as plug contacts of falling length in one body.
s = Symbol('X', C('Klinkenstecker Stereo', 'Jack plug (stereo)', 'Fiche jack (stéréo)'))
s.rect(P, -0.5 * P, 1.5 * P, 2.5 * P)
for k, (name, end) in enumerate((('T', 4.0), ('R', 3.0), ('S', 2.0))):
    y = k * P
    s.line((0, y), (end * P, y)).rect(end * P, y - 0.6, end * P + 1.6, y + 0.6, fill='#000000').text(name, end * P + 2.2, y - 0.9, 1.8)
    s.pin(str(k + 1), (0, y))
add(s.labels((P, -0.5 * P - 3.4), (P, 2.5 * P + 0.6)))

# Schuko socket outlet (CEE 7/3) from the front, 1:2: frame, central plate, the round recess with the two holes and the
# earthing clips at the top and bottom.
s = Symbol('X', C('Schutzkontaktsteckdose (Ansicht)', 'Schuko socket outlet (front view)', 'Prise de courant Schuko (vue de face)'))
s.rect(-20.0, -20.0, 20.0, 20.0, corner=12, w=0.35).rect(-14.0, -14.0, 14.0, 14.0, corner=18).circle(0, 0, 19.5, w=0.35)
for x in (-4.75, 4.75):
    s.circle(x, 0, 2.4, fill='#404040')
for y0, y1 in ((-9.5, -8.3), (8.3, 9.5)):
    s.rect(-2.0, y0, 2.0, y1, w=0.18, fill='#909090')
add(s.labels((20.6, -20.0), (20.6, -17.0)))

# Schuko plug (CEE 7/4) from the side, 1:2, a generic body in dark grey.
DARK = '#404040'
s = Symbol('X', C('Schutzkontaktstecker (Ansicht)', 'Schuko plug (side view)', 'Fiche Schuko (vue de côté)'))
for y in (-4.75, 4.75):
    s.rect(-9.5, y - 1.2, 0.2, y + 1.2, corner=40, w=0.18, fill=METAL)
outline(s, [(0, -9.25), (2.6, -9.25), (2.6, 9.25), (0, 9.25)], 0.8, fill=DARK)
outline(s, [(2.6, -8.6), (10.0, -8.6), (20.0, -5.2), (20.0, 5.2), (10.0, 8.6), (2.6, 8.6)], [0, 3.0, 1.5, 1.5, 3.0, 0], fill=DARK)
outline(s, [(20.0, -4.0), (27.0, -2.4), (27.0, 2.4), (20.0, 4.0)], fill=DARK)
s.rect(27.0, -1.6, 38.0, 1.6, fill=DARK).rect(0.5, -1.8, 2.1, 1.8, w=0.1, fill=METAL)
add(s.labels((-9.5, -12.8), (-1.5, -12.8)))

s = Symbol('X', C('Schraubklemme mit Anschlüssen', 'Screw terminal with leads', 'Borne à vis avec connexions'))
slotted_screw(s.rect(-P, P, P, 3 * P), 0, 2 * P, 3.8).line((0, 0), (0, P)).line((0, 3 * P), (0, 4 * P))
add(s.pin('1', (0, 0)).pin('2', (0, 4 * P), label=(0.4, 4 * P - 2.2)).labels((P + 0.8, P), (P + 0.8, P + 3.0)))


# PCB screw terminal blocks from above, 5.08 mm pitch: a screw per pole, the grey number field below, the lead up.
def screw_block(n, numbered):
    poles = 's' if n > 1 else ''
    s = Symbol('X', C(f'Schraubklemme {n}-polig' + (' (mit Nummern)' if numbered else ''), f'Screw terminal, {n} pole{poles}' + (' (numbered)' if numbered else ''),
                      f'Bornier à vis, {n} pôle{poles}' + (' (numérotés)' if numbered else '')))
    right = (n - 1) * 2 * P + P
    s.rect(-P, -3.0, right, 5.6, w=0.35)
    for k in range(n):
        x = k * 2 * P
        if k:
            s.line((x - P, -3.0), (x - P, 5.6))
        slotted_screw(s, x, 0, 3.8)
        s.rect(x - P + 0.5, 2.5, x + P - 0.5, 5.0, w=0.1, fill=FIELD).text(str(k + 1), x, 2.85, 1.8, 'centre')
        s.line((x, -3.0), (x, -2 * P)).pin(str(k + 1), (x, -2 * P))
    return s.labels((right + 1.0, -3.0), (right + 1.0, 0.0))


for n, numbered in ((1, False), (2, True), (3, True), (4, False), (8, False)):
    add(screw_block(n, numbered))


# Terminal for a DIN rail from above: the body with two screws, the leads up and down; outline only or in a colour.
for fill, de, en, fr in ((None, '', '', ''), ('#d8d8d8', 'hellgrau', 'light grey', 'gris clair'), ('#a0a0a0', 'grau', 'grey', 'gris'),
                         ('#d03030', 'rot', 'red', 'rouge'), ('#30a040', 'grün', 'green', 'vert'), ('#3a7cc8', 'blau', 'blue', 'bleu'),
                         ('#f0d020', 'gelb', 'yellow', 'jaune')):
    s = Symbol('X', C('Reihenklemme (Draufsicht' + (', ' + de if de else '') + ')', 'Rail-mounted terminal (top view' + (', ' + en if en else '') + ')',
                      'Borne sur rail (vue de dessus' + (', ' + fr if fr else '') + ')'))
    outline(s, [(-2.6, P), (2.6, P), (2.6, 7 * P), (-2.6, 7 * P)], 0.5, fill=fill)
    for y in (2 * P, 6 * P):
        slotted_screw(s, 0, y, 3.4, fill='#ffffff' if fill else None)
    s.line((0, 0), (0, P)).line((0, 7 * P), (0, 8 * P)).pin('1', (0, 0)).pin('2', (0, 8 * P), label=(0.4, 8 * P - 2.2))
    add(s.labels((3.4, P), (3.4, P + 3.0)))
