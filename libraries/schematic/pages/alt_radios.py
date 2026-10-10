# Old radios (Elektro/Elektronik/Bauteile/Alt): parts as they look in old radio sets, for repair documentation. Drawn
# from the side in colour with their leads ending on the 2.54 mm pitch and the value printed on the body; generic, no
# makes. The tuning indicators after the screens shown in the data sheets (EM34, EM11, EM84, EM83).
LEAD = '#a0a0a0'


class Pictured(Symbol):
    """A part drawn as it looks: the value text lies on the body, turned along it and in the body's print colour."""
    def __init__(self, *args, turn=0, ink=None, **kwargs):
        super().__init__(*args, **kwargs)
        self.turn, self.ink = turn, ink

    def json(self, entry):
        o = super().json(entry)
        for c in o['children']:
            if c.get('role') == 'value':
                if self.turn:
                    c['rotation'] = self.turn
                if self.ink:
                    c['font']['color'] = self.ink
        return o


def ellipse(s, cx, cy, w, h, fill=None, pen=0.18):
    o = {'type': 'ellipse', 'centre': pt((cx, cy)), 'size': [r(w), r(h)], 'pen': {'width': pen}}
    if fill:
        o['fill'] = {'style': 'solid', 'color': fill}
    s.parts.append(o)


class Side:
    """Draws a lying axial part in (u along it, v across) and places it lying (u to the right) or standing (u down);
    `flip` turns it end for end."""
    def __init__(self, s, length, standing=False, flip=False):
        self.s, self.length, self.standing, self.flip = s, length, standing, flip

    def p(self, u, v):
        if self.flip:
            u = self.length - u
        return (-v, u) if self.standing else (u, v)

    def rect(self, u0, v0, u1, v1, fill, pen=0.18, corner=0):
        (x0, y0), (x1, y1) = self.p(u0, v0), self.p(u1, v1)
        self.s.rect(min(x0, x1), min(y0, y1), max(x0, x1), max(y0, y1), w=pen, fill=fill, corner=corner)

    def ellipse(self, u, v, du, dv, fill):
        x, y = self.p(u, v)
        ellipse(self.s, x, y, dv if self.standing else du, du if self.standing else dv, fill)

    def line(self, *points, w=0.25, colour=None):
        self.s.line(*[self.p(u, v) for u, v in points], w=w)
        if colour:
            coloured(self.s, colour)


def side_part(prefix, caption, value, length, standing, flip, body, half, ink=None, pins=('1', '2')):
    """An axial part `length` pitches long: leads from both ends, body() draws the body (half its thickness `half`)
    and may return where along it the value goes; contact pins[0] at the end u = 0 (before flipping)."""
    L = length * P
    s = Pictured(prefix, caption, value, ask=True, turn=90 if standing else 0, ink=ink)
    f = Side(s, L, standing, flip)
    f.line((0, 0), (L, 0), w=0.5, colour=LEAD)
    x, y = f.p(body(f, L) or L / 2, 0)
    s.pin(pins[0], f.p(0, 0), shown=False).pin(pins[1], f.p(L, 0), shown=False)
    if standing:
        s.labels((half + 1.0, L / 2 - 1.3), (-1.25, y), 'left', 'centre')
    else:
        s.labels((L / 2, -half - 3.4), (x, -1.25), 'centre')
    return s


def elko_body(f, L, can=False):
    """Blue sleeve on the aluminium can, the plus end (u small) with its seal, bead and plus signs."""
    u0, u1 = 4.0, L - 4.0
    f.rect(u0 + 0.6, -3.2, u1, 3.2, '#3561b8', corner=15)
    f.rect(u0, -2.3, u0 + 0.8, 2.3, '#505050')
    if can:
        f.rect(u0 + 0.6, -3.2, u0 + 4.2, 3.2, '#c8c8c8')
        for u in (u0 + 1.6, u0 + 2.4):
            f.line((u, -3.2), (u, 3.2), w=0.18, colour='#808080')
        x = u0 + 5.0
    else:
        f.line((u0 + 2.2, -3.2), (u0 + 2.2, 3.2), w=0.3, colour='#1d3c78')
        x = u0 + 3.6
    for v in (-1.8, 1.8):
        f.line((x - 0.6, v), (x + 0.6, v), w=0.3, colour='#ffffff')
        f.line((x, v - 0.6), (x, v + 0.6), w=0.3, colour='#ffffff')
    return L / 2 + (2.4 if can else 1.6)


def paper(f, L):
    """Waxed paper roll, its ends sealed with wax."""
    f.ellipse(4.4, 0, 2.6, 6.0, '#d4b04a')
    f.ellipse(L - 4.4, 0, 2.6, 6.0, '#d4b04a')
    f.rect(4.4, -3.0, L - 4.4, 3.0, '#ecd77e')


def film(f, L):
    f.rect(3.4, -2.3, L - 3.4, 2.3, '#874a28', corner=30)
    f.rect(3.4, -2.3, 5.0, 2.3, '#74401f', corner=30)
    f.rect(L - 5.0, -2.3, L - 3.4, 2.3, '#74401f', corner=30)
    f.rect(4.4, -2.0, L - 4.4, 2.0, '#874a28')
    coloured(f.s, '#874a28')


def composition(f, L):
    f.rect(4.2, -2.0, L - 4.2, 2.0, '#c4342a')
    f.rect(3.2, -2.4, 4.8, 2.4, '#b8b8b8', corner=20)
    f.rect(L - 4.8, -2.4, L - 3.2, 2.4, '#b8b8b8', corner=20)


def bulged(f, L):
    f.ellipse(5.0, 0, 3.8, 5.0, '#dcc898')
    f.ellipse(L - 5.0, 0, 3.8, 5.0, '#dcc898')
    f.rect(5.0, -1.9, L - 5.0, 1.9, '#e2d0a2')
    coloured(f.s, '#e2d0a2')
    f.line((5.0, -1.9), (L - 5.0, -1.9), w=0.18)
    f.line((5.0, 1.9), (L - 5.0, 1.9), w=0.18)


add = page(BT + '/Alt', 'Alte Radios', 'Old radios', 'Anciennes radios')
ELKO = '22 µF / 63 V'
add(side_part('C', C('Elektrolytkondensator, axial (senkrecht)', 'Electrolytic capacitor, axial (vertical)', 'Condensateur électrolytique axial (vertical)'),
          ELKO, 12, True, False, elko_body, 3.2, ink='#ffffff'))
add(side_part('C', C('Elektrolytkondensator, axial mit Becher (senkrecht)', 'Electrolytic capacitor, axial with metal can (vertical)',
                 'Condensateur électrolytique axial à godet métallique (vertical)'),
          ELKO, 12, True, False, lambda f, L: elko_body(f, L, can=True), 3.2, ink='#ffffff'))
add(side_part('C', C('Elektrolytkondensator, axial', 'Electrolytic capacitor, axial', 'Condensateur électrolytique axial'),
          ELKO, 12, False, True, elko_body, 3.2, ink='#ffffff'))
add(side_part('C', C('Elektrolytkondensator, axial (Pluspol links)', 'Electrolytic capacitor, axial (positive end left)',
                 'Condensateur électrolytique axial (pôle positif à gauche)'), ELKO, 12, False, False, elko_body, 3.2, ink='#ffffff'))
PAPER = '0,047 µF / 630 V'
add(side_part('C', C('Papierkondensator', 'Paper capacitor', 'Condensateur au papier'), PAPER, 12, False, False, paper, 3.0))
add(side_part('C', C('Papierkondensator (senkrecht)', 'Paper capacitor (vertical)', 'Condensateur au papier (vertical)'), PAPER, 12, True, False, paper, 3.0))
for de, en, fr, value, body, half, ink in (('Schichtwiderstand (braun', 'Carbon film resistor (brown', 'Résistance à couche (marron', '47 kΩ', film, 2.3, '#f4ead2'),
                                           ('Massewiderstand (rot', 'Composition resistor (red', 'Résistance agglomérée (rouge', '2,2 kΩ', composition, 2.4, '#ffffff'),
                                           ('Widerstand (beige', 'Resistor (beige', 'Résistance (beige', '100 kΩ', bulged, 2.5, None)):
    add(side_part('R', C(de + ')', en + ')', fr + ')'), value, 8, False, False, body, half, ink=ink))
    add(side_part('R', C(de + ', senkrecht)', en + ', vertical)', fr + ', verticale)'), value, 8, True, False, body, half, ink=ink))


def potentiometer(caption, lugs_down):
    """An old rotary potentiometer from the back: the round can, the lug strip and three solder lugs, wiper 3 in the
    middle as on the schematic potentiometer."""
    t = (lambda x, y: (4 * P - x, -y)) if lugs_down else (lambda x, y: (x, y))
    s = Symbol('P', caption, '1 MΩ log', ask=True)
    rect = lambda x0, y0, x1, y1, **k: s.rect(min(t(x0, y0)[0], t(x1, y1)[0]), min(t(x0, y0)[1], t(x1, y1)[1]),
                                               max(t(x0, y0)[0], t(x1, y1)[0]), max(t(x0, y0)[1], t(x1, y1)[1]), **k)
    cx, cy = 2 * P, 14.0
    s.circle(*t(cx, cy), 21.0, w=0.25, fill='#c0c0c0').circle(*t(cx, cy), 17.4, w=0.18)
    for a in (-60, 60, 180):
        x, y = cx + 9.8 * math.sin(math.radians(a)), cy - 9.8 * math.cos(math.radians(a))
        s.circle(*t(x, y), 1.6, w=0.15, fill='#a8a8a8')
    s.circle(*t(cx, cy), 3.4, w=0.18, fill='#a8a8a8')
    rect(-1.6, 2.4, 4 * P + 1.6, 5.2, w=0.18, fill='#7a4a20')
    for x, name in ((0, '1'), (2 * P, '3'), (4 * P, '2')):
        rect(x - 0.9, 0, x + 0.9, 4.6, w=0.15, fill='#dcdcdc')
        s.circle(*t(x, 1.2), 0.8, w=0.1, fill='#ffffff')
        s.pin(name, t(x, 0))
    y = -cy if lugs_down else cy
    s.labels((cx + 11.6, y - 9.0), (cx, y + 2.2), 'left', 'centre')
    return s


add(potentiometer(C('Potentiometer (Lötösen oben)', 'Potentiometer (solder lugs at the top)', 'Potentiomètre (cosses en haut)'), False))
add(potentiometer(C('Potentiometer (Lötösen unten)', 'Potentiometer (solder lugs at the bottom)', 'Potentiomètre (cosses en bas)'), True))

GLOW, SHADE, GLASS = '#46d96e', '#17402a', '#dfe6e2'


def sector(s, angle, width, r0, r1, fill=SHADE):
    """A ring sector centred on `angle` (degrees clockwise from the top) between the radii r0 and r1."""
    a0, a1 = angle - width / 2, angle + width / 2
    steps = [a0 + (a1 - a0) * k / 8 for k in range(9)]
    pts = [(r1 * math.sin(math.radians(a)), -r1 * math.cos(math.radians(a))) for a in steps]
    pts += [(r0 * math.sin(math.radians(a)), -r0 * math.cos(math.radians(a))) for a in reversed(steps)]
    s.poly(*pts, fill=fill, w=0.05)


def indicator(caption, value, draw, numbered=True):
    s = Symbol('La', caption, value, numbered=numbered)
    draw(s)
    return s


def magic_eye(s):
    """EM34 from the front: the green screen, the narrow and the wide shadow and the deflector bars, the black cap."""
    s.circle(0, 0, 17.0, w=0.25, fill=GLASS).circle(0, 0, 14.6, w=0.15, fill=GLOW)
    sector(s, 0, 46, 2.6, 7.3)
    sector(s, 180, 86, 2.6, 7.3)
    s.rect(-7.2, -0.35, -2.6, 0.35, w=0.05, fill=SHADE).rect(2.6, -0.35, 7.2, 0.35, w=0.05, fill=SHADE)
    s.circle(0, 0, 5.4, w=0.2, fill='#101010')
    s.labels((9.6, -6.0), (9.6, -3.0))


def magic_fan(s):
    """EM11 from above through its glass dome: the screen with two pairs of shadows (two ranges)."""
    s.circle(0, 0, 18.0, w=0.25, fill=GLASS).circle(0, 0, 15.0, w=0.15, fill=GLOW)
    for angle, width in ((0, 30), (180, 30), (90, 70), (270, 70)):
        sector(s, angle, width, 2.4, 7.5)
    s.circle(0, 0, 4.8, w=0.2, fill='#101010')
    s.arc(0, 0, 16.4, 110, 160, w=0.6)
    coloured(s, '#ffffff')
    s.labels((10.0, -6.0), (10.0, -3.0))


def magic_bar(s):
    """EM84 from the side: the glass bulb and the luminous band, brightest in the middle where the two bars meet."""
    s.rect(-4.6, -13.0, 4.6, 13.0, w=0.25, fill=GLASS, corner=45)
    s.rect(-1.6, -10.0, 1.6, 10.0, w=0.1, fill=SHADE)
    s.rect(-1.3, -9.6, 1.3, 9.6, w=0.05, fill=GLOW)
    s.rect(-1.3, -2.0, 1.3, 2.0, w=0.05, fill='#b4ffc4')
    s.labels((6.0, -6.0), (6.0, -3.0))


def magic_balance(s):
    """EM83 from the side: two luminous bands side by side, their lengths compared."""
    s.rect(-5.6, -13.0, 5.6, 13.0, w=0.25, fill=GLASS, corner=45)
    for x0, top in ((-3.4, -4.0), (0.6, 1.5)):
        s.rect(x0, -9.6, x0 + 2.8, 9.6, w=0.1, fill=SHADE)
        s.rect(x0 + 0.3, top, x0 + 2.5, 9.3, w=0.05, fill=GLOW)
    s.labels((7.0, -6.0), (7.0, -3.0))


add(indicator(C('Magisches Auge', 'Magic eye', 'Œil magique'), 'EM34', magic_eye))
add(indicator(C('Magischer Fächer', 'Magic fan', 'Éventail magique'), 'EM11', magic_fan, numbered=False))
add(indicator(C('Magischer Balken', 'Magic bar', 'Barre magique'), 'EM84', magic_bar))
add(indicator(C('Magische Waage', 'Magic balance', 'Balance magique'), 'EM83', magic_balance))
