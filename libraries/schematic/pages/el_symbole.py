# More general signs for the page "Symbole": the general warning sign (after ISO 7010 W001, outline only), lightning
# arrows and the dangerous-voltage sign (after IEC 60417), a signal flag, connection points, the frequency-range signs
# of IEC 60617-2 (stacked tildes), and pictograms of radio, batteries and a wheelchair user (after the idea of ISO 7001,
# drawn here). No parts: no designator, not in the parts list, connection points only where a wire attaches.
add = extend(EL, 'Symbole')


def sign(caption):
    return Symbol('', caption, numbered=False, listed=False, shown=False)


def arrow_outline(points, h, head=1.3, tip=2.0):
    """The outline of a stroke of half width h along `points` that ends in an arrowhead (a lightning arrow)."""
    def normal(a, b):
        dx, dy = b[0] - a[0], b[1] - a[1]
        n = math.hypot(dx, dy)
        return -dy / n, dx / n

    def side(sgn):
        out = []
        for k, p in enumerate(points):
            if k == 0:
                nx, ny = normal(p, points[1])
            elif k == len(points) - 1:
                nx, ny = normal(points[k - 1], p)
            else:
                (ax, ay), (bx, by) = normal(points[k - 1], p), normal(p, points[k + 1])
                f = 1 / (1 + ax * bx + ay * by)
                nx, ny = (ax + bx) * f, (ay + by) * f
            out.append((p[0] + sgn * h * nx, p[1] + sgn * h * ny))
        return out
    (ex, ey), (px, py) = points[-1], points[-2]
    n = math.hypot(ex - px, ey - py)
    ux, uy = (ex - px) / n, (ey - py) / n
    nx, ny = normal(points[-2], points[-1])
    return side(1) + [(ex + head * nx, ey + head * ny), (ex + tip * ux, ey + tip * uy), (ex - head * nx, ey - head * ny)] + side(-1)[::-1]


FLASH = [(2.4, -4.6), (0.6, -0.3), (2.6, -0.8), (1.2, 2.4)]


def tilde(s, y):
    """The tilde of the sign "Wechselstrom" on this page, at height y."""
    return s.arc(1.0, y, 2.0, 0, 180).arc(3.0, y, 2.0, 180, 360)


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


def battery(s, bars=0):
    """An upright battery: the body, the cap of the positive terminal and `bars` charge bars from the bottom up."""
    s.rect(-2.6, -4.2, 2.6, 4.6, w=0.5).rect(-1.2, -5.2, 1.2, -4.2, w=0.5)
    for k in range(bars):
        y = 4.0 - 1.6 * k
        s.rect(-1.8, y - 1.0, 1.8, y, w=0.1, fill='#000000')
    return s


s = sign(C('Warnung (allgemein)', 'General warning', 'Avertissement général'))
s.poly((0, -5.6), (5.5, 3.9), (-5.5, 3.9), fill=None, w=0.6)
add(s.poly((-0.55, -2.6), (0.55, -2.6), (0.3, 1.0), (-0.3, 1.0), w=0.1).circle(0, 2.2, 1.1, w=0.1, fill='#000000'))
add(sign(C('Blitzpfeil (Umriss)', 'Lightning arrow (outline)', 'Flèche éclair (contour)')).poly(*arrow_outline(FLASH, 0.55), fill=None))
s = sign(C('Gefährliche Spannung', 'Dangerous voltage', 'Tension dangereuse')).poly(*arrow_outline(FLASH, 0.55), w=0.1)
add(s.line((1.0, -5.0), (3.8, -5.0), w=THICK))
s = sign(C('Signalfahne', 'Signal flag', 'Étiquette de signal'))
add(s.poly((0, -1.8), (9.0, -1.8), (11.0, 0), (9.0, 1.8), (0, 1.8), fill=None).text('abc...', 0.8, -1.1, 2.2))
for de, en, fr, d in (('Anschlusspunkt (klein)', 'Connection point (small)', 'Point de connexion (petit)', 1.2),
                      ('Anschlusspunkt (groß)', 'Connection point (large)', 'Point de connexion (grand)', 2.2)):
    add(sign(C(de, en, fr)).line((0, 0), (P - d / 2, 0)).circle(P, 0, d).pin('1', (0, 0)))
s = sign(C('Masseanschluss (Kreis)', 'Ground connection (circle)', 'Connexion de masse (cercle)'))
add(s.circle(0, 0, 5.0).line((0, -1.8), (0, 0.6)).line((-1.6, 0.6), (1.6, 0.6), w=0.35))
add(tilde(tilde(sign(C('Wechselstrom, zwei Frequenzen', 'Alternating current, two frequencies', 'Courant alternatif, deux fréquences')), -0.8), 0.8))
add(tilde(tilde(tilde(sign(C('Hochfrequenz', 'High frequency', 'Haute fréquence')), -1.6), 0), 1.6))

s = sign(C('Funk (Piktogramm)', 'Radio (pictogram)', 'Radio (pictogramme)'))
s.line((0, 0), (0, -4.0), w=0.5).line((-1.4, 0), (1.4, 0), w=0.5).circle(0, -4.4, 1.4, fill='#000000')
add(waves(waves(s, 0, -4.4, (2.4,), -45, 45), 0, -4.4, (2.4,), 135, 225))
s = sign(C('Funkwellen (Piktogramm)', 'Radio waves (pictogram)', 'Ondes radio (pictogramme)'))
add(waves(s.circle(0, 0, 1.2, fill='#000000'), 0, 0, (2.0, 3.4, 4.8), 35, 145))
s = sign(C('Funkwellen fett (Piktogramm)', 'Radio waves, bold (pictogram)', 'Ondes radio, gras (pictogramme)'))
add(waves(s.circle(0, 0, 1.8, fill='#000000'), 0, 0, (2.2, 3.8, 5.4), 35, 145, w=0.7))
add(lattice_mast(sign(C('Sendemast (Piktogramm)', 'Transmitter mast (pictogram)', 'Pylône émetteur (pictogramme)'))))
add(dish(sign(C('Satellitenschüssel (Piktogramm)', 'Satellite dish (pictogram)', 'Parabole satellite (pictogramme)'))))

s = sign(C('Akku (Piktogramm)', 'Accumulator (pictogram)', 'Accumulateur (pictogramme)'))
s.rect(-5.0, -2.6, 5.0, 3.6, w=0.5).rect(-3.8, -3.6, -2.2, -2.6, w=0.5).rect(2.2, -3.6, 3.8, -2.6, w=0.5)
add(s.text('+', -3.0, -2.0, 3.0, 'centre').text('−', 3.0, -2.0, 3.0, 'centre'))
s = battery(sign(C('Batterie laden (Piktogramm)', 'Battery charging (pictogram)', 'Batterie en charge (pictogramme)')))
add(s.poly(*arrow_outline([(0.8, -3.2), (-0.7, 0.4), (0.8, 0.0), (-0.1, 2.0)], 0.45, head=1.0, tip=1.4), w=0.1))
add(battery(sign(C('Batterie voll (Piktogramm)', 'Battery full (pictogram)', 'Batterie pleine (pictogramme)')), 5))
add(battery(sign(C('Batterie schwach (Piktogramm)', 'Battery low (pictogram)', 'Batterie faible (pictogramme)')), 2))
add(battery(sign(C('Batterie leer (Piktogramm)', 'Battery empty (pictogram)', 'Batterie vide (pictogramme)'))))
s = sign(C('Rollstuhl (Piktogramm)', 'Wheelchair (pictogram)', 'Fauteuil roulant (pictogramme)'))
s.circle(1.0, -6.6, 2.0, fill='#000000').line((0.8, -5.0), (0.2, -1.2), w=1.0).line((0.6, -3.6), (2.6, -3.0), w=0.8)
s.line((0.2, -1.2), (3.0, -1.2), w=1.0).line((3.0, -1.2), (4.0, 2.2), (4.8, 2.2), w=1.0)
add(s.arc(-0.2, 1.4, 7.0, 80, 390, w=0.9))
