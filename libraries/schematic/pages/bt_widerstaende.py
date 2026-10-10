# More symbols for the pages "Widerstände" and "Widerstände (abhängig)": the rectangle forms and adjustment marks after
# DIN EN 60617-4 (potentiometer with off position 04-01-06), the zigzag forms after IEEE 315, the marks of the rated power
# inside the box after GOST 2.728 (the Roman X for 10 W as in Soviet practice), and two historic parts of valve sets
# in a glass bulb: the iron-hydrogen resistor (coiled iron wire) and the Urdox resistor (uranium dioxide rod).
add = extend(BT, 'Widerstände')

ARROW = ((P + 0.6, 2.2), (3 * P - 0.4, -2.4))     # adjustment mark of "Einstellbarer Widerstand"


def mid(p):
    """A point of a horizontal symbol (centre (2P, 0)) moved to the same place of the vertical one (centre (0, 2P))."""
    return (p[0] - 2 * P, p[1] + 2 * P)


def upright(s, a, b, **kw):
    """An oblique mark of the horizontal form drawn on the vertical form in the same direction (pointing up right)."""
    return s.line(mid(a), mid(b), **kw)


def zigzag(s, x0=P, x1=3 * P, h=1.0, peaks=6):
    d = (x1 - x0) / peaks
    tops = [(x0 + d * (k + 0.5), -h if k % 2 == 0 else h) for k in range(peaks)]
    return s.line((0, 0), (x0, 0), *tops, (x1, 0), (4 * P, 0))


def meander(s, x0, x1, h=1.0, n=4, y=0):
    """A square-wave line from (x0, y) to (x1, y): a wound wire."""
    d = (x1 - x0) / n
    pts = [(x0, y)]
    for k in range(n):
        yy = y - h if k % 2 == 0 else y + h
        pts += [(x0 + d * k, yy), (x0 + d * (k + 1), yy)]
    return s.line(*pts, (x1, y))


def power(de, en, fr, marks):
    """A resistor with its rated power marked inside the box (lines given about the centre (0, 0))."""
    s = resistor_body(two_pin('R', C(de, en, fr), ask=True), h=2.4)
    for a, b in marks:
        s.line((2 * P + a[0], a[1]), (2 * P + b[0], b[1]), w=THICK)
    return s


s = two_pin('R', C('Widerstand (Wert im Symbol)', 'Resistor (value inside)', 'Résistance (valeur à l’intérieur)'), ask=True)
s.line((0, 0), (0.5 * P, 0)).rect(0.5 * P, -1.7, 3.5 * P, 1.7).line((3.5 * P, 0), (4 * P, 0))
add(s.labels((2 * P, -6.0), (2 * P, -1.25), 'centre'))

s = resistor_body(two_pin('R', C('Einstellbarer Widerstand (Trimmung)', 'Preset resistor', 'Résistance préréglable'), ask=True))
add(s.line(*ARROW, end='bar', size=1.6))
for de, en, fr, end in (('Einstellbarer Widerstand (senkrecht)', 'Variable resistor, vertical', 'Résistance variable verticale', 'arrow'),
                        ('Einstellbarer Widerstand (Trimmung, senkrecht)', 'Preset resistor, vertical', 'Résistance préréglable verticale', 'bar')):
    s = resistor_body(two_pin('R', C(de, en, fr), ask=True)).vertical()
    upright(s, *ARROW, end=end, size=1.2 if end == 'arrow' else 1.6)
    add(s.labels((3.4, P - 0.4), (3.4, 2 * P + 0.4)))

add(zigzag(two_pin('R', C('Widerstand (US-Form)', 'Resistor (US style)', 'Résistance (style US)'), ask=True)))
s = zigzag(two_pin('R', C('Widerstand (US-Form, senkrecht)', 'Resistor (US style), vertical', 'Résistance (style US) verticale'), ask=True))
add(s.vertical().labels((2.0, P - 0.4), (2.0, 2 * P + 0.4)))


def pot_us(de, en, fr, below, vertical=False):
    s = zigzag(two_pin('R', C(de, en, fr), ask=True))
    y = 2 * P if below else -2 * P
    s.line((2 * P, y), (2 * P, 1.1 if below else -1.1), end='arrow', size=1.2).pin('3', (2 * P, y), label=(2 * P + 0.5, y - 2.2))
    if vertical:
        s.vertical()
        if below:      # the wiper comes from the left
            return s.labels((2.0, P - 0.4), (2.0, 2 * P + 0.4))
        return s.labels((-2.0, P - 0.4), (-2.0, 2 * P + 0.4), 'right')
    if below:
        return s.labels((2 * P, -5.6), (3 * P + 0.6, 1.6), 'centre', 'left')
    return s.labels((2 * P + 1.0, -5.6), (2 * P, 2.8), 'left', 'centre')


add(pot_us('Potentiometer (US-Form)', 'Potentiometer (US style)', 'Potentiomètre (style US)', False))
add(pot_us('Potentiometer (US-Form, Schleifer unten)', 'Potentiometer (US style), wiper below', 'Potentiomètre (style US), curseur en bas', True))
add(pot_us('Potentiometer (US-Form, senkrecht, Schleifer rechts)', 'Potentiometer (US style), vertical, wiper right',
           'Potentiomètre (style US) vertical, curseur à droite', False, True))
add(pot_us('Potentiometer (US-Form, senkrecht, Schleifer links)', 'Potentiometer (US style), vertical, wiper left',
           'Potentiomètre (style US) vertical, curseur à gauche', True, True))

add(power('Widerstand 0,25 W', 'Resistor 0.25 W', 'Résistance 0,25 W', [((-0.5, -0.75), (0.5, 0.75))]))
add(power('Widerstand 1 W', 'Resistor 1 W', 'Résistance 1 W', [((0, -0.75), (0, 0.75))]))
add(power('Widerstand 2 W', 'Resistor 2 W', 'Résistance 2 W', [((-0.5, -0.75), (-0.5, 0.75)), ((0.5, -0.75), (0.5, 0.75))]))
add(power('Widerstand 10 W', 'Resistor 10 W', 'Résistance 10 W', [((-0.9, -0.75), (0.9, 0.75)), ((-0.9, 0.75), (0.9, -0.75))]))
s = power('Einstellbarer Widerstand 2 W (Trimmung)', 'Preset resistor 2 W', 'Résistance préréglable 2 W',
          [((-0.5, -0.75), (-0.5, 0.75)), ((0.5, -0.75), (0.5, 0.75))])
add(s.line((P + 0.6, 2.4), (3 * P - 0.4, -2.6), end='bar', size=1.6))

s = two_pin('R', C('Potentiometer mit Aus-Stellung', 'Potentiometer with off position', 'Potentiomètre avec position arrêt'), ask=True)
resistor_body(s).rect(3 * P - 0.9, -1.0, 3 * P, 1.0, fill='#000000')
s.line((4 * P, -P), (2 * P, -P), (2 * P, -1.0), end='arrow', size=1.0).pin('3', (4 * P, -P), label=(4 * P - 0.4, -P - 2.2), align='right')
add(s)


def bulb(s, x0, x1, h=2.0):
    """The glass bulb, the leads pass through its ends."""
    return s.rect(x0, -h, x1, h, corner=50)


s = two_pin('R', C('Eisen-Wasserstoff-Widerstand', 'Iron-hydrogen resistor (barretter)', 'Résistance fer-hydrogène'), ask=True)
bulb(s, 0.5 * P, 3.5 * P).line((0, 0), (P, 0)).line((3 * P, 0), (4 * P, 0))
add(meander(s, P, 3 * P))
s = two_pin('R', C('Urdox-Widerstand', 'Urdox resistor', 'Résistance Urdox'), ask=True)
add(bulb(resistor_body(s), 0.5 * P, 3.5 * P))
s = two_pin('R', C('Eisen-Urdox-Widerstand', 'Iron-hydrogen resistor with Urdox', 'Résistance fer-hydrogène avec Urdox'), ask=True, length=5 * P)
bulb(s, 0.5 * P, 4.5 * P).line((0, 0), (P, 0)).line((2.6 * P, 0), (3 * P, 0)).rect(3 * P, -0.8, 4 * P, 0.8).line((4 * P, 0), (5 * P, 0))
add(meander(s, P, 2.6 * P))

add = extend(BT, 'Widerstände (abhängig)')


def dependent(de, en, fr, sign):
    """The existing NTC, PTC and varistor turned upright; the line of the non-linear dependence keeps its direction."""
    s = resistor_body(two_pin('R', C(de, en, fr), ask=True)).vertical()
    s.line(mid((P - 0.6, 2.4)), mid((P + 0.4, 2.4)), mid((3 * P + 0.2, -2.4)))
    s.text(sign, -3.6, 2 * P - 1.6, 2.0, 'right')
    return s.labels((3.6, P - 0.4), (3.6, 2 * P + 0.4))


add(dependent('Heißleiter (NTC, senkrecht)', 'Thermistor (NTC), vertical', 'Thermistance (CTN) verticale', '-ϑ'))
add(dependent('Kaltleiter (PTC, senkrecht)', 'Thermistor (PTC), vertical', 'Thermistance (CTP) verticale', '+ϑ'))
add(dependent('Varistor (senkrecht)', 'Varistor (VDR), vertical', 'Varistance verticale', 'U'))


def light(s, tips, length=2.4):
    """Light falling on a part: arrows from up right with their tips at the given points."""
    for x, y in tips:
        s.arrow((x + length * 0.7071, y - length * 0.7071), (x, y), size=0.9)
    return s


s = resistor_body(two_pin('R', C('Fotowiderstand (LDR, senkrecht)', 'Photoresistor (LDR), vertical', 'Photorésistance verticale'), ask=True))
s.vertical()
add(light(s, ((1.3, 2 * P - 1.5), (1.3, 2 * P + 0.1))).labels((4.4, P - 0.4), (4.4, 2 * P + 0.4)))

s = resistor_body(two_pin('R', C('Fotowiderstand (LDR) mit Gehäuse', 'Photoresistor (LDR) with envelope', 'Photorésistance avec enveloppe'), ask=True))
s.circle(2 * P, 0, 7.4)
add(light(s, ((2 * P - 0.9, -1.4), (2 * P + 0.5, -1.4)), 4.0).labels((2 * P - 1.6, -7.2), (2 * P, 4.4), 'centre'))
s = resistor_body(two_pin('R', C('Fotowiderstand (LDR) mit Gehäuse (senkrecht)', 'Photoresistor (LDR) with envelope, vertical',
                             'Photorésistance avec enveloppe verticale'), ask=True))
s.circle(2 * P, 0, 7.4).vertical()
add(light(s, ((1.4, 2 * P - 1.6), (1.4, 2 * P - 0.2)), 4.0).labels((5.0, 2.2), (5.0, 2 * P + 1.4)))

s = two_pin('R', C('Fotowiderstand (Bauform)', 'Photoresistor (package)', 'Photorésistance (boîtier)'), ask=True)
s.circle(2 * P, 0, 7.4).line((0, 0), (2 * P - 2.4, 0)).line((2 * P + 2.4, 0), (4 * P, 0))
meander(s, 2 * P - 2.4, 2 * P + 2.4, h=1.8, n=6)
add(light(s, ((2 * P + 1.4, -3.4), (2 * P + 2.8, -2.4)), 2.4).labels((2 * P - 1.6, -7.0), (2 * P, 4.4), 'centre'))
