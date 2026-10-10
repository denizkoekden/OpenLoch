# Braun Lectron, the magnetic building-block system (Braun from 1966, today Lectron GmbH): seven pages "Lectron ..." in
# Elektro/Elektronik/Bauteile/Braun Lectron for layout drawings of its experiments. A block is a tile seen from above:
# cells of 8 pitches with a thin outline, the circuit symbol printed on the block (DIN EN 60617; the logic blocks of
# 1969 with the shapes printed on them) and leads from it to the middles of the cell edges where the block has its
# contacts, named after the edge (N top, E, S, W), numbered along an edge of several cells. Tiles laid edge to edge
# join at these contacts. A thick bar is the contact to the metal base plate, which is the ground of the system:
# contact GND (GND1, GND2 where a block has several). Forms, sizes and contacts after the Braun Lectron
# Baustein-Katalog, the Braun price list of 1968, the Lectron Baustein-Katalog of Lectron GmbH (about 2000), the
# Lectron Baustein-Katalog of Reha Werkstatt Oberrad (Stand 1. November 2015) and the block photos on lectron.info.
from contextlib import contextmanager

T = 8 * P          # side of a cell
H = T / 2          # middle of a cell edge
ROT = {None: (1, 0, 0, 1), 'cw': (0, -1, 1, 0), 'ccw': (0, 1, -1, 0), 'flip': (0, -1, -1, 0), 'mirror': (1, 0, 0, -1),
       'hflip': (-1, 0, 0, 1)}
BLACK, RED, YELLOW, GREEN, BLUE = '#000000', '#d00000', '#e8b000', '#00a000', '#0090e0'
MINUS, BAR = '−', '̅'
FOLDER = BT + '/Braun Lectron'


class Tile(Symbol):
    """A block of cols × rows cells, its top left corner at the insertion point. Drawing goes through the matrix M
    (a, b, c, d, e, f): x' = a x + b y + e, y' = c x + d y + f. `turn` draws a one-cell block given from W to E turned:
    W to N ('cw'), W to S ('ccw'), W to S and N to E ('flip'), or N to S ('mirror')."""
    def __init__(self, prefix, caption, cols=1, rows=1, turn=None, **kw):
        super().__init__(prefix, caption, **kw)
        self.cols, self.rows = cols, rows
        self.M = (1, 0, 0, 1, 0, 0)
        super().rect(0, 0, cols * T, rows * T, w=0.18)
        if turn:
            a, b, c, d = ROT[turn]
            self.M = (a, b, c, d, H - a * H - b * H, H - c * H - d * H)

    def m(self, p):
        a, b, c, d, e, f = self.M
        return (a * p[0] + b * p[1] + e, c * p[0] + d * p[1] + f)

    @contextmanager
    def local(self, x, y, turn=None):
        """Draw about (x, y), the x axis turned as for `turn` ('cw': running down)."""
        old = self.M
        A, B, Cc, D, E, F = old
        a, b, c, d = ROT[turn]
        self.M = (A * a + B * c, A * b + B * d, Cc * a + D * c, Cc * b + D * d, A * x + B * y + E, Cc * x + D * y + F)
        try:
            yield self
        finally:
            self.M = old

    @contextmanager
    def plain(self):
        """Draw at the places given on the finished block."""
        old, self.M = self.M, (1, 0, 0, 1, 0, 0)
        try:
            yield self
        finally:
            self.M = old

    def line(self, *points, **kw):
        return super().line(*[self.m(p) for p in points], **kw)

    def dashed(self, *points, w=W):
        self.parts.append({'type': 'line', 'points': [pt(self.m(p)) for p in points], 'pen': {'width': w, 'style': 'dash'},
                           'electrical': False})
        return self

    def rect(self, x0, y0, x1, y1, **kw):
        (u0, v0), (u1, v1) = self.m((x0, y0)), self.m((x1, y1))
        return super().rect(min(u0, u1), min(v0, v1), max(u0, u1), max(v0, v1), **kw)

    def circle(self, cx, cy, d, **kw):
        x, y = self.m((cx, cy))
        return super().circle(x, y, d, **kw)

    def arc(self, cx, cy, d, start, stop, w=W, dy=None, dash=False):
        a, b, c, dd = self.M[:4]
        sx, sy = d, dy or d
        if b:
            sx, sy = sy, sx

        def turned(t):
            u, v = math.cos(math.radians(t)), -math.sin(math.radians(t))
            return math.degrees(math.atan2(-(c * u + dd * v), a * u + b * v)) % 360
        t0, t1 = turned(start), turned(stop)
        if a * dd - b * c < 0:
            t0, t1 = t1, t0
        if t1 <= t0 + 1e-9:
            t1 += 360
        x, y = self.m((cx, cy))
        super().arc(x, y, sx, t0, t1, w=w, dy=sy)
        if dash:
            self.parts[-1]['pen']['style'] = 'dash'
        return self

    def poly(self, *points, **kw):
        return super().poly(*[self.m(p) for p in points], **kw)

    def bezier(self, *points, **kw):
        return super().bezier(*[self.m(p) for p in points], **kw)

    def label(self, t, x, y, h=1.8, align='centre', bold=False):
        """A text centred on y; centred on x, or beginning or ending there. Its place goes through M."""
        u, v = self.m((x, y))
        return super().text(t, u, v - h / 2, h, align, bold)

    def note(self, t, x, y, h=1.8, align='left', bold=False):
        """A text centred on y at the place given on the finished block."""
        return super().text(t, x, y - h / 2, h, align, bold)

    def side(self, x, y):
        wd, ht = self.cols * T, self.rows * T
        near = lambda a, b: abs(a - b) < 1e-6
        if near(y, 0) or near(y, ht):
            edge, along, n = ('N' if near(y, 0) else 'S'), x, self.cols
        elif near(x, 0) or near(x, wd):
            edge, along, n = ('W' if near(x, 0) else 'E'), y, self.rows
        else:
            raise ValueError('contact not on the outline: ' + str((x, y)))
        k = int(along // T)
        assert near(along, k * T + H), 'contact not at the middle of a cell edge: ' + str((x, y))
        return edge + (str(k + 1) if n > 1 else '')

    def con(self, x, y, name=None):
        """A contact at (x, y), named after its edge unless `name` is given."""
        X, Y = self.m((x, y))
        X, Y = round(X, 6) + 0.0, round(Y, 6) + 0.0
        wd, ht = self.cols * T, self.rows * T
        name = name or self.side(X, Y)
        if abs(Y) < 1e-6:
            lab, al = (X + 0.4, 0.3), 'left'
        elif abs(Y - ht) < 1e-6:
            lab, al = (X + 0.4, ht - 2.2), 'left'
        elif abs(X - wd) < 1e-6:
            lab, al = (X - 0.4, Y - 2.2), 'right'
        else:
            lab, al = (X + 0.4, Y - 2.2), 'left'
        super().pin(name, (X, Y), label=lab, align=al)
        return self

    def lead(self, *points, name=None):
        """A lead from the contact at points[0] through the other points."""
        self.line(*points)
        return self.con(*points[0], name=name)

    def bar(self, x, y, name='GND', half=1.5):
        """The contact to the base plate: a thick bar across at (x, y)."""
        self.line((x - half, y), (x + half, y), w=THICK)
        return self.con(x, y, name=name)

    def dot(self, x, y, d=0.9, fill=BLACK):
        return self.circle(x, y, d, fill=fill)


# --- What is printed on the blocks, drawn about (0, 0) with the main axis along x
DL, DH = 3.4, 2.0      # length and half height of a diode


def r_body(s, l=2 * P, h=2.0):
    s.rect(-l / 2, -h / 2, l / 2, h / 2)


def d_body(s, fill=None, half=DH, l=DL):
    s.poly((-l / 2, -half), (-l / 2, half), (l / 2, 0), fill=fill)
    s.line((l / 2, -half), (l / 2, half), w=0.35)


def zener_hook(s):
    s.line((DL / 2, -DH), (DL / 2 - 1.0, -DH))


def schottky_hooks(s):
    s.line((DL / 2 + 0.8, -DH + 0.6), (DL / 2 + 0.8, -DH), (DL / 2, -DH)).line((DL / 2, DH), (DL / 2 - 0.8, DH), (DL / 2 - 0.8, DH - 0.6))


def c_body(s, polar=False, h=2.4):
    if polar:
        s.rect(-1.1, -h, -0.5, h).rect(0.5, -h, 1.1, h, fill=BLACK)
    else:
        s.line((-0.5, -h), (-0.5, h), w=THICK).line((0.5, -h), (0.5, h), w=THICK)


def l_body(s, l=2 * P, turns=4):
    d = l / turns
    for k in range(turns):
        s.arc(-l / 2 + d * (k + 0.5), 0, d, 0, 180)


def lamp_body(s, d=6.0):
    s.circle(0, 0, d)
    k = d / 2 * math.sqrt(0.5)
    s.line((-k, -k), (k, k)).line((-k, k), (k, -k))


def meter_body(s, d=6.4):
    s.circle(0, 0, d).arrow((-1.7, 1.7), (1.8, -1.8), size=1.0)


def light_out(s, x, y, gap=1.4, length=2.6):
    """Two arrows leaving downwards: the light of an LED printed beside it."""
    for k in (0, 1):
        s.arrow((x + k * gap, y), (x + k * gap, y + length), size=0.9)


def light_in(s, x, y):
    """Two arrows falling from up right onto (x, y)."""
    for k in (0, 1):
        s.arrow((x + 2.0 + k * 1.4, y - 2.6), (x + k * 1.4, y - 0.4), size=0.9)


def flash(s, x, y):
    """The flash of a high voltage, its tip at (x, y)."""
    s.line((x + 0.8, y - 3.2), (x - 0.4, y - 1.4), (x + 0.8, y - 1.6), (x, y), end='arrow', size=0.8)


def two_terminal(s, gap):
    """The leads of a part in the middle of a cell: from W to it and from it to E."""
    s.lead((0, H), (H - gap, H)).lead((T, H), (H + gap, H))


def socket_cup(s, x, y):
    """A plug socket of the blocks: a cup opening upwards at (x, y) with the hole in it."""
    s.arc(x, y, 2.8, 180, 360, w=THICK)
    s.circle(x, y, 0.8)


def rca(s, x, y):
    s.circle(x, y, 3.6, w=0.35).circle(x, y, 1.2, w=0.35)


def din_socket(s, x, y, d=6.0):
    """A DIN socket seen from the front: the ring with its key at the top and five pins on an arc."""
    s.arc(x, y, d, 100, 440, w=0.35)
    s.line((x - 0.53, y - 2.95), (x - 0.53, y - 2.2), (x + 0.53, y - 2.2), (x + 0.53, y - 2.95), w=0.35)
    for a in (180, 225, 270, 315, 360):
        s.circle(x + 1.7 * math.cos(math.radians(a)), y - 1.7 * math.sin(math.radians(a)), 0.8)


def bjt(s, pnp=False, top='C', d=7.6):
    """A bipolar transistor: the base bar at x = -2, the base coming from the left at y = 0; collector and emitter end at
    (0, -2.6) and (0, 2.6), the one named `top` above."""
    s.circle(-0.8, 0, d)
    s.line((-2.0, -2.0), (-2.0, 2.0), w=THICK)
    for sign, name in ((-1, top), (1, 'E' if top == 'C' else 'C')):
        a, b = (-2.0, sign * 0.9), (0, sign * 2.6)
        if name == 'C':
            s.line(a, b)
        elif pnp:
            s.line(b, a, end='arrow', size=1.3)
        else:
            s.line(a, b, end='arrow', size=1.3)


def mos(s, p=False):
    """An enhancement MOSFET: the gate plate at x = -2.6 (its lead from the left at y = 0), drain and source ending at
    (0, -1.7) and (0, 1.7): drain above for the N channel, source above for the P channel."""
    s.circle(-0.6, 0, 7.6).line((-2.6, -2.0), (-2.6, 2.0))
    for y0, y1 in ((-2.2, -1.2), (-0.5, 0.5), (1.2, 2.2)):
        s.line((-1.8, y0), (-1.8, y1), w=THICK)
    s.line((-1.8, -1.7), (0, -1.7)).line((-1.8, 1.7), (0, 1.7))
    if p:
        s.line((-1.8, 0), (0, 0), end='arrow', size=1.3).line((0, 0), (0, -1.7))
    else:
        s.line((0, 0), (-1.8, 0), end='arrow', size=1.3).line((0, 0), (0, 1.7))


def plus(s, x, y):
    return s.note('+', x, y, 3.0, 'centre')


def blk(prefix, caption, cols=1, rows=1, turn=None, **kw):
    return Tile(prefix, caption, cols, rows, turn, **kw)


def diode_tile(prefix, caption, turn=None, fill=None, extra=None, value=''):
    s = blk(prefix, caption, turn=turn, value=value)
    two_terminal(s, DL / 2)
    with s.local(H, H):
        d_body(s, fill)
        if extra:
            extra(s)
    return s


# ======================================================================================================================
add = page(FOLDER, 'Lectron Active', 'Lectron Active', 'Lectron composants actifs')


def transistor_tile(caption, pnp, top='C', value=''):
    """A transistor block: base W, the lead named `top` to N, the other one to S."""
    s = blk('T', caption, value=value)
    with s.local(H, H):
        bjt(s, pnp, top)
    return s.lead((0, H), (H - 2.0, H)).lead((H, 0), (H, H - 2.6))


def base_resistor(s):
    """The base resistor of the transistor blocks, from the collector (N) to the base."""
    s.dot(2.0, H).line((2.0, H), (2.0, 2.6), (3.0, 2.6)).rect(3.0, 1.8, 7.2, 3.4).line((7.2, 2.6), (H, 2.6)).dot(H, 2.6)


add(transistor_tile(C('NPN-Transistor', 'NPN transistor', 'Transistor NPN'), False).lead((H, T), (H, H + 2.6)).labels((0.6, 0.6), None))
add(transistor_tile(C('PNP-Transistor', 'PNP transistor', 'Transistor PNP'), True, 'E').lead((H, T), (H, H + 2.6)).labels((0.6, 0.6), None))
for ohm in ('100', '330'):
    s = transistor_tile(C(f'PNP-Transistor mit Basiswiderstand {ohm} kΩ', f'PNP transistor with {ohm} kΩ base resistor',
                          f'Transistor PNP avec résistance de base {ohm} kΩ'), True, value=ohm + ' kΩ')
    base_resistor(s)
    add(s.lead((H, T), (H, H + 2.6)).labels((0.6, T - 3.2), (H + 0.6, 0.4)))
s = transistor_tile(C('PNP-Transistor mit Basiswiderstand 330 kΩ, Emitter an Masse', 'PNP transistor with 330 kΩ base resistor, emitter to ground',
                      'Transistor PNP avec résistance de base 330 kΩ, émetteur à la masse'), True, value='330 kΩ')
base_resistor(s)
add(s.line((H, H + 2.6), (H, 7 * P)).bar(H, 7 * P).labels((0.6, T - 3.2), (H + 0.6, 0.4)))
# The 680 kΩ block carries an AF 126, an HF transistor whose screen (dashed) goes to the base plate.
s = transistor_tile(C('PNP-Transistor mit Basiswiderstand 680 kΩ, Abschirmung an Masse', 'PNP transistor with 680 kΩ base resistor, screen to ground',
                      'Transistor PNP avec résistance de base 680 kΩ, blindage à la masse'), True, value='680 kΩ')
base_resistor(s)
s.lead((H, T), (H, H + 2.6)).arc(H - 0.8, H, 9.6, 200, 276, dash=True)
add(s.line((3 * P, H + 4.48), (3 * P, 7 * P)).bar(3 * P, 7 * P).labels((0.6, T - 3.2), (H + 0.6, 0.4)))

s = blk('T', C('Feldeffekttransistor', 'Field-effect transistor', 'Transistor à effet de champ'))
s.circle(H - 0.6, H, 7.6).line((H - 1.8, H - 3.0), (H - 1.8, H + 3.0), w=THICK)
s.line((0, H), (H - 1.8, H), end='arrow', size=1.3).con(0, H)
s.lead((H, 0), (H, H - 1.8), (H - 1.8, H - 1.8)).lead((H, T), (H, H + 1.8), (H - 1.8, H + 1.8))
s.note('G', 2.4, H - 1.6, 2.0, 'centre').note('D', H + 1.4, 2.0, 2.0).note('S', H + 1.4, T - 2.0, 2.0)
add(s.labels((0.6, 0.6), None))

s = blk('T', C('Unijunction-Transistor', 'Unijunction transistor', 'Transistor unijonction'))
s.circle(H - 0.6, H, 7.6).line((H - 1.6, H - 2.2), (H - 1.6, H + 2.2), w=THICK)
s.lead((0, H), (H - 4.8, H)).line((H - 4.8, H), (H - 1.6, H - 1.0), end='arrow', size=1.3)
s.lead((H, 0), (H, H - 1.7), (H - 1.6, H - 1.7)).lead((H, T), (H, H + 1.7), (H - 1.6, H + 1.7))
s.note('E', 2.4, H - 1.6, 2.0, 'centre').note('B2', H + 1.4, 2.0, 2.0).note('B1', H + 1.4, T - 2.0, 2.0)
add(s.labels((0.6, 0.6), None))

for p, de, en, fr in ((False, 'MOSFET N-Kanal', 'MOSFET, N-channel', 'MOSFET canal N'), (True, 'MOSFET P-Kanal', 'MOSFET, P-channel', 'MOSFET canal P')):
    s = blk('T', C(de, en, fr))
    with s.local(H, H):
        mos(s, p)
    s.lead((0, H), (H - 2.6, H)).lead((H, 0), (H, H - 1.7)).lead((H, T), (H, H + 1.7))
    top, bottom = ('S', 'D') if p else ('D', 'S')
    s.note('G', 2.4, H - 1.6, 2.0, 'centre').note(top, H + 1.4, 2.0, 2.0).note(bottom, H + 1.4, T - 2.0, 2.0)
    add(s.labels((0.6, 0.6), None))

VALUE_BELOW = ((0.6, 0.6), (H, H + 3.0), 'left', 'centre')
VALUE_RIGHT = ((0.6, 0.6), (H + 2.8, H - 1.25))
s = diode_tile('D', C('Germaniumdiode', 'Germanium diode', 'Diode au germanium'))
add(s.note('Ge', H + 3.4, H - 3.4, 2.2).labels(*VALUE_BELOW))
s = diode_tile('D', C('Germaniumdiode (senkrecht)', 'Germanium diode, vertical', 'Diode au germanium verticale'), turn='ccw')
add(s.note('Ge', H + 2.8, H - 3.4, 2.2).labels((0.6, 0.6), (H + 2.8, H + 0.6)))
add(diode_tile('D', C('Siliziumdiode', 'Silicon diode', 'Diode au silicium')).labels(*VALUE_BELOW))


def led_arrows(s):
    light_out(s, -1.4, DH + 0.8)


s = diode_tile('D', C('Leuchtdiode (senkrecht)', 'LED, vertical', 'DEL verticale'), turn='cw', extra=led_arrows)
add(plus(s, H + 2.4, 3.4).labels(*VALUE_RIGHT))
add(diode_tile('D', C('Leistungsdiode', 'Power diode', 'Diode de puissance'), fill=BLACK).labels(*VALUE_BELOW))
add(diode_tile('D', C('Schottky-Diode', 'Schottky diode', 'Diode Schottky'), extra=schottky_hooks).labels(*VALUE_BELOW))

s = blk('D', C('Doppeldiode', 'Dual diode', 'Double diode'))
for y in (H - 4.6, H + 4.6):
    with s.local(H, y, 'cw'):
        d_body(s, half=1.7, l=2.8)
s.lead((H, 0), (H, H - 6.0)).line((H, H - 3.2), (H, H + 3.2)).lead((H, T), (H, H + 6.0))
add(s.lead((0, H), (T, H)).con(T, H).dot(H, H).labels((0.6, 0.6), None))


def thyristor_tile(caption, turn=None):
    """Anode W, cathode E, the gate leaving the cathode and ending at S (all turned with the block)."""
    s = diode_tile('TH', caption, turn=turn)
    return s.lead((H, T), (H, H + 3.4), (H + DL / 2, H + 0.6))


add(thyristor_tile(C('Thyristor', 'Thyristor', 'Thyristor')).labels((0.6, 0.6), (H + 3.0, H - 4.6)))
add(thyristor_tile(C('Thyristor (senkrecht)', 'Thyristor, vertical', 'Thyristor vertical'), 'flip').labels((0.6, 0.6), (H + 2.8, H + 3.0)))
s = diode_tile('D', C('Z-Diode 5,1 V', 'Zener diode 5.1 V', 'Diode Zener 5,1 V'), turn='cw', extra=zener_hook, value='C5V1')
add(s.labels(*VALUE_RIGHT))
s = diode_tile('D', C('Diode (senkrecht, gefüllt)', 'Diode, vertical (filled)', 'Diode verticale (pleine)'), turn='cw', fill=BLACK)
add(plus(s, H + 2.6, H + 3.6).labels(*VALUE_RIGHT))
s = diode_tile('D', C('Z-Diode 5,1 V (gefüllt)', 'Zener diode 5.1 V (filled)', 'Diode Zener 5,1 V (pleine)'), turn='cw', fill=BLACK, extra=zener_hook, value='C5V1')
add(s.labels(*VALUE_RIGHT))
s = diode_tile('D', C('Diode (gefüllt)', 'Diode (filled)', 'Diode (pleine)'), fill=BLACK)
add(plus(s, H + 3.6, H - 3.2).labels(*VALUE_BELOW))
s = diode_tile('D', C('Leuchtdiode', 'LED', 'DEL'), fill=BLACK, extra=led_arrows)
add(plus(s, H - 3.6, H - 3.0).labels((0.6, 0.6), (H + 2.4, H + 3.0)))


# ======================================================================================================================
add = page(FOLDER, 'Lectron Connections', 'Lectron Connections', 'Lectron connexions')


def wire(caption, cols=1, rows=1):
    return blk('', caption, cols, rows, numbered=False, listed=False, shown=False)


add(wire(C('Verbindung gerade', 'Straight connection', 'Liaison droite')).lead((0, H), (T, H)).con(T, H))
add(wire(C('Verbindung T-Stück', 'T connection', 'Liaison en T')).lead((0, H), (T, H)).con(T, H).lead((H, T), (H, H)).dot(H, H))
add(wire(C('Verbindung Winkel', 'Angle connection', 'Liaison coudée')).lead((0, H), (H, H), (H, T)).con(H, T))
add(wire(C('Verbindung Masse', 'Ground connection', 'Liaison à la masse')).lead((H, 0), (H, H)).bar(H, H))
add(wire(C('Kreuzung isoliert', 'Crossing, insulated', 'Croisement isolé')).lead((0, H), (T, H)).con(T, H).lead((H, 0), (H, T)).con(H, T))
s = wire(C('Kreuzung verbunden', 'Crossing, connected', 'Croisement relié'))
add(s.lead((0, H), (T, H)).con(T, H).lead((H, 0), (H, T)).con(H, T).dot(H, H))
add(wire(C('Verbindung gerade, dreifach', 'Straight connection, triple length', 'Liaison droite, triple longueur'), 3, 1).lead((0, H), (3 * T, H)).con(3 * T, H))
s = wire(C('Kreuzung vierfach', 'Crossing, four cells', 'Croisement, quatre cases'), 2, 2)
add(s.lead((0, H), (2 * T, 3 * H)).con(2 * T, 3 * H).lead((0, 3 * H), (2 * T, H)).con(2 * T, H))
s = wire(C('Kreuzung zweifach', 'Crossing, two cells', 'Croisement, deux cases'), 2, 1)
add(s.lead((H, 0), (3 * H, T)).con(3 * H, T).lead((3 * H, 0), (H, T)).con(H, T))
# The measuring block (Messbaustein, no. 2114 of the 2015 catalogue, 8058 of Braun): the straight connection W-E with
# two sockets on it for the plugs of the measuring leads.
s = wire(C('Verbindung Messleitung', 'Measuring lead connection', 'Liaison pour cordon de mesure'))
s.lead((0, H), (T, H)).con(T, H)
for x in (2.5 * P, 5.5 * P):
    socket_cup(s, x, 1.6 * P)
    s.line((x, 1.6 * P + 1.4), (x, H)).dot(x, H)
add(s)
s = wire(C('Anschluss zweifach', 'Two sockets', 'Deux douilles'))
s.lead((0, H), (2 * P, H), (2 * P, 5 * P - 0.5)).lead((T, H), (6 * P, H), (6 * P, 5 * P - 0.5))
socket_cup(s, 2 * P, 5 * P + 0.9)
socket_cup(s, 6 * P, 5 * P + 0.9)
add(s)
# The connection block (2113 of the 2015 catalogue, 8 100 406): the socket on the base plate left, the one on the edge
# contact E right.
s = wire(C('Anschlussbaustein', 'Socket block', 'Bloc de douilles'))
s.lead((T, H), (6 * P, H), (6 * P, 5 * P - 0.5)).bar(2 * P, 2 * P).line((2 * P, 2 * P), (2 * P, 5 * P - 0.5))
socket_cup(s, 2 * P, 5 * P + 0.9)
socket_cup(s, 6 * P, 5 * P + 0.9)
add(s)
s = wire(C('DIN-Buchse', 'DIN socket', 'Embase DIN'), 1, 3)
din_socket(s, H, H + 0.4)
s.lead((0, H), (H - 3.2, H)).lead((T, H), (H + 3.2, H)).line((H, H + 2.1), (H, 22 * P))
add(s.bar(H, 22 * P))
s = wire(C('Cinch-Buchse', 'RCA sockets', 'Embases RCA'), 2, 1)
for x, edge in ((H, 0), (3 * H, 2 * T)):
    rca(s, x, 3 * P)
    s.lead((edge, H), (x, H), (x, 3 * P + 0.6))
s.line((H + 1.8, 3 * P), (3 * H - 1.8, 3 * P)).dot(T, 3 * P)
add(s.line((T, 3 * P), (T, 6 * P)).bar(T, 6 * P))


# ======================================================================================================================
add = page(FOLDER, 'Lectron E-Mechanics', 'Lectron E-Mechanics', 'Lectron électromécanique')


def push_head(s, x, y):
    """The push button above a contact: the dashed link from (x, y) up to the knob."""
    s.dashed((x, y), (x, y - 3.6))
    s.line((x - 1.3, y - 2.8), (x - 1.3, y - 3.6), (x + 1.3, y - 3.6), (x + 1.3, y - 2.8))


def button_tile(caption, make, turn=None):
    """A push button W-E: the fixed contact at W, the blade hinged at E."""
    s = blk('S', caption, turn=turn)
    s.lead((0, H), (H - 3.0, H)).dot(H - 3.0, H).lead((T, H), (H + 3.0, H)).dot(H + 3.0, H)
    if make:
        s.line((H + 3.0, H), (H - 2.4, H - 2.4))
        push_head(s, H + 0.3, H - 1.2)
    else:
        s.line((H - 3.0, H), (H - 3.0, H - 2.17)).line((H + 3.0, H), (H - 3.6, H - 2.4))
        push_head(s, H - 0.3, H - 1.2)
    return s.labels((0.6, 0.6), None)


add(button_tile(C('Taster (Schließer)', 'Push button (make)', 'Bouton-poussoir (travail)'), True))
add(button_tile(C('Taster (Schließer, senkrecht)', 'Push button (make), vertical', 'Bouton-poussoir (travail) vertical'), True, 'cw'))
add(button_tile(C('Taster (Öffner)', 'Push button (break)', 'Bouton-poussoir (repos)'), False))
add(button_tile(C('Taster (Öffner, senkrecht)', 'Push button (break), vertical', 'Bouton-poussoir (repos) vertical'), False, 'cw'))


def toggle_tile(caption, turn=None):
    """The change-over toggle switch: common at W, at rest on the contact at N, the other one at S."""
    s = blk('S', caption, turn=turn)
    s.lead((0, H), (H - 3.4, H)).dot(H - 3.4, H)
    s.lead((H, 0), (H, H - 2.8)).dot(H, H - 2.8).lead((H, T), (H, H + 2.8)).dot(H, H + 2.8)
    s.line((H - 3.4, H), (H - 0.3, H - 2.6))
    s.dashed((H - 1.85, H - 1.3), (H - 1.85, H - 4.8)).line((H - 3.15, H - 4.8), (H - 0.55, H - 4.8))
    return s.labels((0.6, 0.6), None)


add(toggle_tile(C('Schalter', 'Toggle switch', 'Interrupteur à levier')))
add(toggle_tile(C('Schalter (senkrecht)', 'Toggle switch, vertical', 'Interrupteur à levier vertical'), 'cw'))


def coil_box(s, x, y, vertical=False):
    """A relay coil: the rectangle with the slant line."""
    a, b = (1.4, 2.4) if vertical else (2.4, 1.4)
    s.rect(x - a, y - b, x + a, y + b).line((x - a, y + b), (x + a, y - b))


# The relay of the 1960s: three cells high, the coil across the middle one, a change-over contact in each of the others
# hinged at the end of the block.
s = blk('K', C('Relais', 'Relay', 'Relais'), 1, 3)
coil_box(s, H, 3 * H)
s.lead((0, 3 * H), (H - 2.4, 3 * H)).lead((T, 3 * H), (H + 2.4, 3 * H))
for y, end, dy in ((H, 0, -3.4), (5 * H, 3 * T, 3.4)):
    s.lead((H, end), (H, y + dy)).dot(H, y + dy)
    s.lead((0, y), (H - 3.0, y)).dot(H - 3.0, y).lead((T, y), (H + 3.0, y)).dot(H + 3.0, y)
    s.line((H, y + dy), (H - 2.8, y + (0.4 if dy < 0 else -0.4)))
    s.dashed((H - 1.4, y + dy / 2), (H - 1.4, 3 * H - (1.4 if dy < 0 else -1.4)))
add(s.labels((0.6, T + 0.6), None))

# The relay of today: three cells long, the coil with its free-wheeling diode across the middle one (+ at S2), a
# change-over contact at each end, hinged at W and at E.
s = blk('K', C('Relais Typ 2', 'Relay, type 2', 'Relais, type 2'), 3, 1)
coil_box(s, 3 * H, H, vertical=True)
s.lead((3 * H, 0), (3 * H, H - 2.4)).lead((3 * H, T), (3 * H, H + 2.4))
s.dot(3 * H, H - 4.0).dot(3 * H, H + 4.0).line((3 * H, H - 4.0), (3 * H - 4.0, H - 4.0), (3 * H - 4.0, H - 1.7))
s.line((3 * H - 4.0, H + 1.7), (3 * H - 4.0, H + 4.0), (3 * H, H + 4.0))
with s.local(3 * H - 4.0, H, 'cw'):
    d_body(s)
plus(s, 3 * H + 2.0, T - 2.0)
for x0, sign in ((0, 1), (3 * T, -1)):
    px = x0 + sign * 3.0
    s.lead((x0, H), (px, H)).dot(px, H)
    s.lead((x0 + sign * H, 0), (x0 + sign * H, H - 2.8)).dot(x0 + sign * H, H - 2.8)
    s.lead((x0 + sign * H, T), (x0 + sign * H, H + 2.8)).dot(x0 + sign * H, H + 2.8)
    s.line((px, H), (x0 + sign * (H - 0.3), H + 2.6))
    s.dashed((x0 + sign * 6.6, H + 1.4), (3 * H - sign * 1.4, H + 1.4))
add(s.labels((T + 0.6, 0.6), None))


def speaker(s, x, y, k=1.0):
    """A loudspeaker after DIN EN 60617: the magnet (its leads at top and bottom) and the cone to the right."""
    s.rect(x - k, y - 1.6 * k, x + k, y + 1.6 * k)
    s.poly((x + k, y - 1.6 * k), (x + 3.4 * k, y - 3.6 * k), (x + 3.4 * k, y + 3.6 * k), (x + k, y + 1.6 * k), fill=None)


def output_transformer(s, x, y, k=1.0, flip=False):
    """The primary winding on the vertical at x from y - 2.6 k to y + 2.6 k, the iron core and the secondary feeding a
    loudspeaker, all to the right of the primary (to the left with `flip`)."""
    with s.local(x, y, 'hflip' if flip else None):
        with s.local(0, 0, 'cw'):
            l_body(s, 5.2 * k, 4)
        for cx in (1.6 * k, 2.2 * k):
            s.line((cx, -2.8 * k), (cx, 2.8 * k), w=THICK)
        with s.local(3.8 * k, 0, 'ccw'):
            l_body(s, 5.2 * k, 4)
        sx = 6.4 * k
        s.line((3.8 * k, -2.6 * k), (3.8 * k, -3.4 * k), (sx, -3.4 * k), (sx, -1.6 * k))
        s.line((3.8 * k, 2.6 * k), (3.8 * k, 3.4 * k), (sx, 3.4 * k), (sx, 1.6 * k))
        speaker(s, sx, 0, k)


# Today's block: two cells square, the grille above, the loudspeaker with its transformer between N2 and S2.
s = blk('B', C('Lautsprecher 0,2 W', 'Loudspeaker 0.2 W', 'Haut-parleur 0,2 W'), 2, 2, value='0,2 W')
for k in range(6):
    y = 1.2 * P + k * 0.9 * P
    half = math.sqrt(max(0.0, (3.4 * P) ** 2 - (y - 3.6 * P) ** 2)) * 0.8
    s.line((T - 1.0 * P - half, y), (T - 1.0 * P + half, y), w=0.6)
s.lead((3 * H, 0), (3 * H, 11.5 * P - 3.9)).lead((3 * H, 2 * T), (3 * H, 11.5 * P + 3.9))
output_transformer(s, 3 * H, 11.5 * P, 1.5, flip=True)
add(s.labels((0.6, 2 * T - 6.4), (0.6, 2 * T - 3.4)))

# The block of the 1960s: three cells square, the line W2-E2 across it, the loudspeaker from its middle to S2.
s = blk('B', C('Lautsprecher (ältere Ausführung)', 'Loudspeaker (older version)', 'Haut-parleur (ancienne version)'), 3, 3)
s.lead((0, 3 * H), (3 * T / 2, 3 * H)).lead((3 * T, 3 * H), (3 * T / 2, 3 * H)).dot(3 * T / 2, 3 * H)
s.line((3 * T / 2, 3 * H), (3 * T / 2, 18 * P - 5.2)).lead((3 * T / 2, 3 * T), (3 * T / 2, 18 * P + 5.2))
output_transformer(s, 3 * T / 2, 18 * P, 2.0)
add(s.labels((0.6, 0.6), None))

# The audio transformer (Übertrager 10:1): primary between N and S, secondary to E1 and E2.
s = blk('T', C('Transformator', 'Transformer', 'Transformateur'), 1, 2, value='10:1')
with s.local(H - 1.0, T, 'cw'):
    l_body(s, 6.0, 5)
s.lead((H, 0), (H, T - 4.0), (H - 1.0, T - 4.0), (H - 1.0, T - 3.0)).lead((H, 2 * T), (H, T + 4.0), (H - 1.0, T + 4.0), (H - 1.0, T + 3.0))
s.line((H + 0.6, T - 3.4), (H + 0.6, T + 3.4), w=THICK).line((H + 1.2, T - 3.4), (H + 1.2, T + 3.4), w=THICK)
with s.local(H + 2.8, T, 'ccw'):
    l_body(s, 4.2, 3)
s.lead((T, H), (H + 2.8, H), (H + 2.8, T - 2.1)).lead((T, 3 * H), (H + 2.8, 3 * H), (H + 2.8, T + 2.1))
add(s.labels((0.6, 0.6), (0.6, T - 1.2)))

s = blk('B', C('Hörer', 'Earphone', 'Écouteur'))
s.lead((H, 0), (H, H - 1.4)).lead((H, T), (H, H + 1.4)).rect(H - 1.6, H - 1.4, H + 0.4, H + 1.4)
add(s.line((H + 1.2, H - 2.6), (H + 1.2, H + 2.6), w=0.8).labels((0.6, 0.6), None))
# The magnets laid beside the motor block (Lectron-Motor 8 100 600 of the Lectron catalogue of about 2000): blocks of
# 1 × 2 cells, the magnet a thick bar along the edge towards the motor (the south pole at its left, the north pole at
# its right), the pole letter in the middle.
for letter, de, en, fr in (('S', 'Magnet Südpol', 'Magnet, south pole', 'Aimant, pôle sud'), ('N', 'Magnet Nordpol', 'Magnet, north pole', 'Aimant, pôle nord')):
    s = blk('', C(de, en, fr), 1, 2, numbered=False)
    x = T - 1.8 if letter == 'S' else 1.8
    s.rect(x - 0.8, T - 11.4, x + 0.8, T + 11.4, fill=BLACK)
    add(s.note(letter, H, T, 5.0, 'centre', bold=True))

s = blk('M', C('Motor', 'Motor', 'Moteur'), 2, 2)
s.circle(T, T, 2 * P * 2).note('M', T, T, 5.0, 'centre').note('=', T, T + 3.6, 3.0, 'centre')
s.lead((0, H), (3 * P, H), (3 * P, T), (T - 2 * P, T)).lead((2 * T, H), (2 * T - 3 * P, H), (2 * T - 3 * P, T), (T + 2 * P, T))
add(plus(s, 1.4 * P, H - 2.4).labels((0.6, 2 * T - 3.4), None))

# The stepper motor: one centre-tapped winding at each side, its ends at W1, W2 and E1, E2 (numbered 2, 4 and 1, 3 as
# on the block), the centre taps to the base plate.
s = blk('M', C('Schrittmotor', 'Stepper motor', 'Moteur pas à pas'), 2, 2)
s.circle(T, T, 4 * P).circle(T, T, 1.6 * P)
for x, edge, sign, names in ((2 * P, 0, 1, ('2', '4')), (14 * P, 2 * T, -1, ('1', '3'))):
    s.lead((edge, H), (x, H), (x, H + 0.6 * P), name=names[0]).lead((edge, 3 * H), (x, 3 * H), (x, 3 * H - 0.6 * P), name=names[1])
    s.rect(x - 0.6, H + 0.6 * P, x + 0.6, 7.4 * P).rect(x - 0.6, 8.6 * P, x + 0.6, 3 * H - 0.6 * P)
    s.line((x, 7.4 * P), (x, 8.6 * P)).dot(x, T).line((x, T), (x + sign * P, T))
    with s.local(x + sign * P, T, 'cw'):
        s.bar(0, 0, 'GND1' if sign > 0 else 'GND2', 1.2)
    s.note(names[0], x + sign * 1.8, H - 2.6, 2.5, 'centre', bold=True).note(names[1], x + sign * 1.8, 3 * H + 2.6, 2.5, 'centre', bold=True)
add(s.labels((T - 1.6, 0.6), None))


def tuned_coil(caption, turns):
    """The radio coil blocks, one print for the medium and short wave coils (nos. 2707 to 2709 of the 2015 catalogue,
    the radio sets on lectron.info): N and W1 at the head of the coil, 100 kΩ and 330 pF in parallel from there to E1
    (the grid leak of an audion), the coil with its ferrite core, trimmed by the core, its tap to E2 and its foot to the
    base plate; the coupling turn from W2 to the base plate."""
    s = blk('L', caption, 1, 2)
    s.lead((H, 0), (H, H)).lead((0, H), (H, H)).dot(H, H).dot(H, 1.6 * P).dot(7 * P, H).lead((T, H), (7 * P, H))
    s.line((H, H), (5.5 * P - 0.5, H)).line((5.5 * P + 0.5, H), (7 * P, H))
    s.line((5.5 * P - 0.5, H - 1.6), (5.5 * P - 0.5, H + 1.6), w=THICK).line((5.5 * P + 0.5, H - 1.6), (5.5 * P + 0.5, H + 1.6), w=THICK)
    s.line((H, 1.6 * P), (4.8 * P, 1.6 * P)).rect(4.8 * P, 1.6 * P - 0.7, 6.4 * P, 1.6 * P + 0.7).line((6.4 * P, 1.6 * P), (7 * P, 1.6 * P), (7 * P, H))
    s.note('100 kΩ', 0.4, 0.9 * P, 1.6).note('330 pF', 0.4, 2.3 * P, 1.6)
    top, tap = H + 2.0, 3 * H
    s.line((H, H), (H, top))
    with s.local(H, (top + tap) / 2, 'cw'):
        l_body(s, tap - top, turns)
    with s.local(H, tap + 1.2, 'cw'):
        l_body(s, 2.4, 1)
    s.dot(H, tap).lead((T, tap), (H, tap)).line((H, tap + 2.4), (H, 14 * P)).bar(H, 14 * P, 'GND2')
    s.lead((0, tap), (2 * P, tap))
    with s.local(2 * P, tap + 1.2, 'cw'):
        l_body(s, 2.4, 1)
    s.line((2 * P, tap + 2.4), (2 * P, 14 * P)).bar(2 * P, 14 * P, 'GND1')
    s.dashed((3 * P, top + 0.4), (3 * P, tap + 3.2), w=THICK)
    s.line((1.2 * P, 11.0 * P), (6.0 * P, 5.6 * P))
    s.line((6.0 * P - 0.75, 5.6 * P - 0.67), (6.0 * P + 0.75, 5.6 * P + 0.67))
    return s.labels((5.4 * P, T + 0.6), None)


add(tuned_coil(C('Spule Mittelwelle', 'Coil, medium wave', 'Bobine petites ondes'), 5))

s = blk('C', C('Drehkondensator', 'Tuning capacitor', 'Condensateur variable'), 2, 2, value='47 pF')
s.lead((0, H), (3 * P - 0.5, H)).line((3 * P + 0.5, H), (T, H)).dot(T, H).lead((2 * T, H), (T, H))
with s.local(3 * P, H):
    c_body(s)
s.line((T, H), (T, 9 * P - 0.5)).line((T, 9 * P + 0.5), (T, 13 * P)).bar(T, 13 * P)
with s.local(T, 9 * P, 'cw'):
    c_body(s)
s.arrow((T - 3.0, 9 * P + 3.0), (T + 3.2, 9 * P - 3.2), size=1.0)
s.circle(4 * P, 11 * P, 3.2 * P, w=0.35).line((4 * P, 11 * P), (4 * P - 2.6, 11 * P - 2.6), w=0.35)
s.note('220 pF', T + 3.4, 9 * P + 2.4, 1.8)
add(s.labels((0.6, 0.6), (3 * P, H + 3.6), 'left', 'centre'))

add(tuned_coil(C('Spule Langwelle', 'Coil, long wave', 'Bobine grandes ondes'), 8))


# ======================================================================================================================
add = page(FOLDER, 'Lectron Indicators', 'Lectron Indicators', 'Lectron indicateurs')

for turn, de, en, fr in (('cw', 'Glühlampe', 'Incandescent lamp', 'Lampe à incandescence'),
                         (None, 'Glühlampe (waagerecht)', 'Incandescent lamp, horizontal', 'Lampe à incandescence horizontale')):
    s = blk('LA', C(de, en, fr), turn=turn)
    two_terminal(s, 3.0)
    with s.local(H, H):
        lamp_body(s)
    add(s.labels((0.6, 0.6), (H + 4.0, H + 3.4) if turn else (H, H + 4.4), 'left', 'left' if turn else 'centre'))


def led_dome(s, fill=None):
    """The LED of the block beside its symbol, up right."""
    return s.circle(H + 4.4, H - 4.6, 3.6, w=0.35, fill=fill)


s = diode_tile('D', C('Leuchtdiode', 'LED', 'DEL'), extra=led_arrows)
add(led_dome(s).labels((0.6, 0.6), None))
s = diode_tile('D', C('Leuchtdiode sehr hell', 'LED, very bright', 'DEL très lumineuse'), extra=led_arrows)
led_dome(s)
s.poly((H + 2.6, T - 1.8), (H + 7.6, T - 1.8), (H + 5.1, T - 6.1), fill=None, w=0.35)
for a in range(0, 180, 45):
    dx, dy = 1.2 * math.cos(math.radians(a)), 1.2 * math.sin(math.radians(a))
    s.line((H + 5.1 - dx, T - 3.3 - dy), (H + 5.1 + dx, T - 3.3 + dy), w=0.18)
add(s.circle(H + 5.1, T - 3.3, 0.9, fill=BLACK).labels((0.6, 0.6), None))
for colour, de, en, fr in ((RED, 'rot', 'red', 'rouge'), (YELLOW, 'gelb', 'yellow', 'jaune'), (GREEN, 'grün', 'green', 'verte')):
    s = blk('D', C(f'Leuchtdiode mit Vorwiderstand, {de}', f'LED with series resistor, {en}', f'DEL avec résistance série, {fr}'))
    s.lead((0, H), (H - 6.0, H)).rect(H - 6.0, H - 0.9, H - 2.0, H + 0.9).line((H - 2.0, H), (H + 1.0, H))
    s.lead((T, H), (H + 1.0 + DL, H))
    with s.local(H + 1.0 + DL / 2, H):
        d_body(s)
    light_out(s, H + 0.6, H + DH + 0.8)
    add(s.circle(H + 4.4, H - 4.8, 3.0, w=0.25, fill=colour).labels((0.6, 0.6), None))

s = blk('LA', C('Glimmlampe', 'Neon lamp', 'Lampe au néon'))
two_terminal(s, 1.0)
s.circle(H, H, 6.4).line((H - 1.0, H - 2.0), (H - 1.0, H + 2.0), w=0.35).line((H + 1.0, H - 2.0), (H + 1.0, H + 2.0), w=0.35)
add(s.dot(H + 1.6, H + 2.0, 0.7).labels((0.6, 0.6), None))


def scale(s, cx, cy, radius, left, right, marks):
    """A meter scale: an arc about (cx, cy) from the angle `left` to `right`, ten divisions, `marks` the figures at the
    divisions given (index, text)."""
    s.arc(cx, cy, 2 * radius, right, left, w=0.18)
    for k in range(11):
        a = math.radians(left + (right - left) * k / 10)
        long_mark = 1.0 if k % 5 == 0 else 0.6
        s.line((cx + radius * math.cos(a), cy - radius * math.sin(a)), (cx + (radius + long_mark) * math.cos(a), cy - (radius + long_mark) * math.sin(a)), w=0.18)
    for k, text in marks:
        a = math.radians(left + (right - left) * k / 10)
        s.note(text, cx + (radius + 2.2) * math.cos(a), cy - (radius + 2.2) * math.sin(a), 1.6, 'centre')


def needle(s, cx, cy, radius, angle):
    a = math.radians(angle)
    s.line((cx + (radius - 2.5) * math.cos(a), cy - (radius - 2.5) * math.sin(a)), (cx + (radius + 0.6) * math.cos(a), cy - (radius + 0.6) * math.sin(a)), w=0.35)


s = blk('', C('Messinstrument', 'Measuring instrument', 'Appareil de mesure'), 2, 2, numbered=False, value='100 µA')
s.rect(1.0 * P, 1.0 * P, 10.6 * P, 7.0 * P, w=0.35)
scale(s, 5.8 * P, 11.5 * P, 6 * P, 128, 52, ((0, '0'), (5, '5'), (10, '10')))
needle(s, 5.8 * P, 11.5 * P, 6 * P, 110)
s.lead((3 * H, 0), (3 * H, 11 * P - 3.2)).lead((3 * H, 2 * T), (3 * H, 11 * P + 3.2))
with s.local(3 * H, 11 * P):
    meter_body(s)
add(s.labels((0.6, 2 * T - 3.4), (0.6, 2 * T - 6.6)))

s = blk('', C('Messwerk Typ 1', 'Meter movement, type 1', 'Galvanomètre, type 1'), numbered=False)
two_terminal(s, 3.2)
with s.local(H, H):
    meter_body(s)
add(s.labels((0.6, 0.6), None))

s = blk('', C('Messwerk Typ 2', 'Meter movement, type 2', 'Galvanomètre, type 2'), 2, 2, numbered=False, value='100 µA')
s.rect(1.0 * P, 1.0 * P, 15.0 * P, 7.0 * P, w=0.35)
scale(s, T, 15.0 * P, 10.0 * P, 125, 55, ((0, '10'), (5, '0'), (10, '10')))
needle(s, T, 15.0 * P, 10.0 * P, 90)
s.lead((0, 3 * H), (H, 3 * H)).dot(H, 3 * H).line((H, 3 * H), (T - 3.2, 3 * H)).line((T + 3.2, 3 * H), (3 * H, 3 * H)).dot(3 * H, 3 * H)
s.lead((2 * T, 3 * H), (3 * H, 3 * H)).lead((H, 2 * T), (H, 3 * H)).lead((3 * H, 2 * T), (3 * H, 3 * H))
with s.local(T, 3 * H):
    meter_body(s)
add(s.labels((0.6, T + 0.4), (0.6, T + 3.4)))


# ======================================================================================================================
add = page(FOLDER, 'Lectron Modules', 'Lectron Modules', 'Lectron modules')


def texts(s, items, h=1.8):
    for t, x, y, align in items:
        s.note(t, x, y, h, align)
    return s


def amplifier(s, x, y, w=6 * P, h=5 * P, sign='FM'):
    """An amplifier triangle pointing right, its tip at (x + w, y)."""
    s.poly((x, y - h / 2), (x, y + h / 2), (x + w, y), fill=None, w=0.35)
    s.note(sign, x + w * 0.36, y, 2.5, 'centre', bold=True)


s = blk('U', C('UKW-Baustein', 'FM receiver module', 'Module récepteur FM'), 2, 2)
amplifier(s, 4 * P, H)
s.lead((0, H), (4 * P, H)).arrow((0.8 * P, H), (2.4 * P, H), size=1.2).lead((2 * T, H), (10 * P, H))
s.lead((3 * H, 0), (3 * H, 1.0 * P), (7 * P, 1.0 * P), (7 * P, 2.75 * P))
s.line((5 * P, 6.08 * P), (5 * P, 8 * P)).bar(5 * P, 8 * P)
s.lead((H, 2 * T), (H, 12 * P), (7 * P, 12 * P), (7 * P, 5.25 * P))
s.lead((3 * H, 2 * T), (3 * H, 13 * P + 0.5)).line((3 * H, 13 * P - 0.5), (3 * H, 10 * P), (8.5 * P, 10 * P), (8.5 * P, 4.62 * P))
with s.local(3 * H, 13 * P, 'cw'):
    c_body(s)
s.arrow((3 * H - 3.0, 13 * P + 3.0), (3 * H + 3.0, 13 * P - 3.0), size=1.0)
texts(s, (('Ant', 0.4, H - 1.6, 'left'), ('NF', 2 * T - 0.4, H - 1.6, 'right'), ('+', 3 * H + 0.8, 0.6 * P, 'left'),
          ('Mute', H + 0.6, 2 * T - 1.4, 'left'), ('Tuning', 3 * H - 0.6, 2 * T - 1.4, 'right')))
add(s.labels((10.6 * P, 7.2 * P), None))

# The FM stereo receiver (no. 2488 of the 2015 catalogue): three cells square, the antenna at N1, + at N2, a tuning
# capacitor (dashed, from outside) at W2, the loudspeaker outputs L* and R* at E2 and E3, the headphone outputs R and L
# at S1 and S2, S3 to the base plate.
s = blk('U', C('UKW-Stereomodul', 'FM stereo module', 'Module stéréo FM'), 3, 3)
fm = ((9.2 * P, 8.4 * P), (9.2 * P, 15.6 * P), (16.8 * P, 3 * H))
s.poly(*fm, fill=None, w=0.35).note('FM', 12.2 * P, 3 * H, 3.5, 'centre')
s.lead((H, 0), (H, 9 * P), (9.2 * P, 9 * P)).line((H - 1.0, 0.5 * P), (H, 1.4 * P), (H + 1.0, 0.5 * P))
s.lead((3 * H, 0), (3 * H, 9.73 * P)).dot(3 * H, 5.6 * P, 1.6, BLUE)
s.dot(6 * P, 5.6 * P, 1.6, RED).dot(18 * P, 5.6 * P, 1.6, YELLOW)
s.lead((0, 3 * H), (2 * P - 0.5, 3 * H)).dashed((2 * P + 0.5, 3 * H), (9.2 * P, 3 * H))
s.line((2 * P - 0.5, 3 * H - 1.6), (2 * P - 0.5, 3 * H + 1.6), w=THICK).line((2 * P + 0.5, 3 * H - 1.6), (2 * P + 0.5, 3 * H + 1.6), w=THICK)
s.arrow((2 * P - 2.4, 3 * H + 2.4), (2 * P + 2.4, 3 * H - 2.4), size=1.0)
s.circle(6 * P, 16.6 * P, 4 * P, fill=BLACK).arc(6 * P, 16.6 * P, 5 * P, -45, 225)
s.poly((17.2 * P, 11 * P), (17.2 * P, 13 * P), (19.2 * P, 3 * H), fill=None).line((16.8 * P, 3 * H), (17.2 * P, 3 * H))
s.lead((3 * T, 3 * H), (19.2 * P, 3 * H)).dot(22 * P, 3 * H).line((22 * P, 3 * H), (22 * P, 16 * P - 1.6))
speaker(s, 22 * P, 16 * P)
s.lead((3 * T, 5 * H), (22 * P, 5 * H), (22 * P, 16 * P + 1.6))
s.line((18 * P, 12.5 * P), (18 * P, 14.3 * P)).circle(18 * P, 16.3 * P, 4 * P, fill=BLACK).arc(18 * P, 16.3 * P, 5 * P, 20, 150, w=0.6)
s.line((15.4 * P, 12.66 * P), (15.4 * P, 21.6 * P), (3 * H, 21.6 * P)).dot(3 * H, 21.6 * P).lead((3 * H, 3 * T), (3 * H, 21.6 * P))
s.line((3 * H, 21.6 * P), (10 * P, 21.6 * P)).arc(8.5 * P, 21.6 * P, 3.0, 0, 180)
s.rect(7.9 * P - 0.6, 21.6 * P, 7.9 * P + 0.2, 22.8 * P, fill=BLACK).rect(9.1 * P - 0.2, 21.6 * P, 9.1 * P + 0.6, 22.8 * P, fill=BLACK)
s.lead((H, 3 * T), (H, 21.6 * P), (7 * P, 21.6 * P))
s.line((11 * P, 14.75 * P), (11 * P, 18 * P)).bar(11 * P, 18 * P, 'GND1')
s.lead((5 * H, 3 * T), (5 * H, 22.2 * P), (18 * P, 22.2 * P), (18 * P, 23 * P)).bar(18 * P, 23 * P, 'GND2')
texts(s, (('Ant', H + 1.2, 1.0 * P, 'left'), ('+', 3 * H + 1.0, 1.0 * P, 'left'), ('Tune', 6 * P, 7.0 * P, 'centre'),
          ('Stereo', 18 * P, 7.0 * P, 'centre'), ('Tuning', 6 * P, 13.6 * P, 'centre'), ('Volume', 18 * P, 19.4 * P, 'centre'),
          ('L*', 3 * T - 0.4, 3 * H - 1.4, 'right'), ('R*', 3 * T - 0.4, 5 * H - 1.4, 'right'),
          ('R', H + 0.6, 23.2 * P, 'left'), ('L', 3 * H + 0.6, 23.2 * P, 'left')), 2.0)
add(s.labels((0.6, 0.6), None))

# The transistor amplifier 2 × AC 173 of 1968: two stages, input W2, output E1, the supply at N1.
s = blk('U', C('Verstärker', 'Amplifier', 'Amplificateur'), 2, 2)
s.lead((H, 0), (H, 1.5 * P), (10 * P, 1.5 * P))
for x, y in ((4 * P, 4.5 * P), (10 * P, 2.75 * P), (8 * P, 4 * P), (3 * P, 8 * P), (10 * P, 11.5 * P)):
    with s.local(x, y, None if (x, y) in ((8 * P, 4 * P), (3 * P, 8 * P)) else 'cw'):
        r_body(s, 3.6, 1.6)
s.line((4 * P, 1.5 * P), (4 * P, 4.5 * P - 1.8)).line((4 * P, 4.5 * P + 1.8), (4 * P, 11 * P - 2.6)).dot(4 * P, 8 * P)
s.line((4 * P, 8 * P), (3 * P + 1.8, 8 * P)).line((3 * P - 1.8, 8 * P), (2 * P, 8 * P), (2 * P, 11 * P)).dot(2 * P, 11 * P)
s.lead((0, 3 * H), (1 * P - 0.5, 3 * H)).line((1 * P + 0.5, 3 * H), (2 * P, 3 * H), (2 * P, 11 * P), (4 * P - 2.0, 11 * P))
with s.local(1 * P, 3 * H):
    c_body(s, h=1.8)
with s.local(4 * P, 11 * P):
    bjt(s, True, d=6.4)
s.line((4 * P, 11 * P + 2.6), (4 * P, 15 * P)).bar(4 * P, 15 * P, 'GND1')
s.line((4 * P, 8 * P), (6 * P - 0.5, 8 * P)).line((6 * P + 0.5, 8 * P), (10 * P - 2.0, 8 * P)).dot(7 * P, 8 * P)
with s.local(6 * P, 8 * P):
    c_body(s, h=1.8)
with s.local(10 * P, 8 * P):
    bjt(s, True, d=6.4)
s.line((10 * P, 1.5 * P), (10 * P, 2.75 * P - 1.8)).line((10 * P, 2.75 * P + 1.8), (10 * P, 8 * P - 2.6)).dot(10 * P, H)
s.line((10 * P, H), (8 * P + 1.8, H)).line((8 * P - 1.8, H), (7 * P, H), (7 * P, 8 * P))
s.line((10 * P, 8 * P + 2.6), (10 * P, 11.5 * P - 1.8)).line((10 * P, 11.5 * P + 1.8), (10 * P, 15 * P)).bar(10 * P, 15 * P, 'GND2')
s.line((10 * P, H), (13 * P - 0.5, H)).lead((2 * T, H), (13 * P + 0.5, H))
with s.local(13 * P, H):
    c_body(s, h=1.8)
add(texts(s, (('E', 0.4, 3 * H - 3.2, 'left'), ('A', 2 * T - 0.4, H - 1.6, 'right'), (MINUS, H + 0.8, 0.5 * P, 'left'))).labels((12 * P, 13.2 * P), None))


def braun_flipflop(caption, dynamic):
    """The flip-flop of 1969: inputs E1, E2 (dynamic), outputs A1, A2, reset R at N2, the supply (−) at N1, the base
    plate (+) below."""
    s = blk('U', caption, 2, 2)
    s.rect(3 * P, 2 * P, 12 * P, 14 * P, w=0.35).dashed((3 * P, T), (12 * P, T), w=0.35).line((12 * P - 0.35, 2 * P), (12 * P - 0.35, T), w=0.8)
    for y in (H, 3 * H):
        s.lead((0, y), (3 * P, y)).lead((2 * T, y), (12 * P, y))
        if dynamic:
            s.line((3 * P, y - 0.9), (3 * P + 1.4, y), (3 * P, y + 0.9))
    s.lead((H, 0), (H, 1.0 * P)).lead((3 * H, 0), (3 * H, 2 * P))
    s.line((T, 14 * P), (T, 15 * P)).bar(T, 15 * P)
    texts(s, (('E1', 0.4, H - 1.6, 'left'), ('E2', 0.4, 3 * H - 1.6, 'left'), ('A1', 2 * T - 0.4, H - 1.6, 'right'),
              ('A2', 2 * T - 0.4, 3 * H - 1.6, 'right'), (MINUS, H + 0.8, 0.5 * P, 'left'), ('R', 3 * H + 0.8, 0.6 * P, 'left'),
              ('+', T + 2.4, 15 * P, 'left')), 2.0)
    return s.labels((3 * P + 0.6, 2 * P + 0.6), None)


add(braun_flipflop(C('Flipflop', 'Flip-flop', 'Bascule'), True))

s = blk('U', C('MOSFET-Modul', 'MOSFET module', 'Module MOSFET'), 2, 2)
for k, (x, y) in enumerate(((H, H), (3 * H, 3 * H)), 1):
    with s.local(x, y):
        mos(s)
    s.lead((0, y), (x - 2.6, y)).lead((x, 0), (x, y - 1.7)).line((x, y + 1.7), (x, y + 3 * P)).bar(x, y + 3 * P, f'GND{k}')
    texts(s, ((f'G{k}', 0.4, y - 1.6, 'left'), (f'D{k}', x + 0.8, 1.0 * P if k == 1 else 5 * P, 'left')))
add(s.labels((10 * P, 0.6), None))

add(braun_flipflop(C('Flipflop (Variante)', 'Flip-flop (variant)', 'Bascule (variante)'), False))

s = blk('U', C('Schmitt-Trigger', 'Schmitt trigger', 'Trigger de Schmitt'), 2, 2)
s.rect(3 * P, 2 * P, 13 * P, 14 * P, w=0.35)
s.line((4.5 * P, 11.5 * P), (6.5 * P, 11.5 * P), (9.5 * P, 4.5 * P), (11.5 * P, 4.5 * P), w=0.35)
s.lead((0, 3 * H), (3 * P, 3 * H)).lead((2 * T, H), (13 * P, H)).lead((H, 0), (H, 1.0 * P))
s.line((T, 14 * P), (T, 15 * P)).bar(T, 15 * P)
texts(s, (('E', 0.4, 3 * H - 1.6, 'left'), ('A', 2 * T - 0.4, H - 1.6, 'right'), (MINUS, H + 0.8, 0.5 * P, 'left'),
          ('+', T + 2.4, 15 * P, 'left')), 2.0)
add(s.labels((3 * P + 0.6, 2 * P + 0.6), None))


def braun_gate(caption, sign, negated):
    """The gates of 1969 with three inputs, as printed on them (the flat side the inputs, the round one the output),
    turned upright: E1 to E3 at W1 to W3, the output A at E2, the supply (−) at N, the base plate (+) below."""
    s = blk('U', caption, 1, 3)
    x0, x1, y0, y1 = 2 * P, 5.5 * P, 1.6 * P, 22.4 * P
    s.line((x0, y0), (x0, y1), w=0.35)
    s.bezier((x0, y0), (4.6 * P, y0), (x1, 2.6 * P), (x1, 4.6 * P), w=0.35).line((x1, 4.6 * P), (x1, 19.4 * P), w=0.35)
    s.bezier((x1, 19.4 * P), (x1, 21.4 * P), (4.6 * P, y1), (x0, y1), w=0.35)
    for k, y in enumerate((H, 3 * H, 5 * H), 1):
        s.lead((0, y), (x0, y)).note(f'E{k}', 0.4, y - 1.6, 1.8)
    if negated:
        s.circle(x1 + 0.6, 3 * H, 1.2).lead((T, 3 * H), (x1 + 1.2, 3 * H))
    else:
        s.lead((T, 3 * H), (x1, 3 * H))
    s.note('A', T - 0.4, 3 * H - 1.6, 1.8, 'right').note(sign, 3.6 * P, 3 * H, 2.5, 'centre')
    s.lead((H, 0), (H, 1.0 * P)).note(MINUS, H + 0.8, 0.5 * P, 2.0)
    s.line((H, 23 * P), (H, 23 * P - 1.0)).bar(H, 23 * P).note('+', H + 2.4, 23 * P, 2.0)
    return s.labels((0.4, 0.4), None)


add(braun_gate(C('ODER-Gatter, drei Eingänge', 'OR gate, three inputs', 'Porte OU, trois entrées'), '≥1', False))
add(braun_gate(C('NAND-Gatter, drei Eingänge', 'NAND gate, three inputs', 'Porte NON-ET, trois entrées'), '&', True))

s = blk('U', C('Impedanzwandler', 'Impedance converter', "Adaptateur d'impédance"), 1, 2)
s.poly((H - 3.0, 5 * P), (H + 3.0, 5 * P), (H, 9.4 * P), fill=None, w=0.35)
s.lead((0, H), (H, H), (H, 5 * P)).lead((T, 3 * H), (H, 3 * H), (H, 9.4 * P)).lead((H, 0), (H, 0.9 * P))
add(texts(s, (('E', 0.4, H - 1.6, 'left'), ('A', T - 0.4, 3 * H - 1.6, 'right'), (MINUS, H + 0.8, 0.5 * P, 'left')), 2.0).labels((0.4, 0.4), None))


def module_box(s, x0, y0, x1, y1):
    s.rect(x0, y0, x1, y1, w=0.35)


def ground_below(s, x, y, name='GND'):
    """The base plate under the box bottom at y: a short stem and the bar one pitch lower, on the grid."""
    s.line((x, y), (x, 23 * P)).bar(x, 23 * P, name)


# The binary counter (no. 2468 of the 2015 catalogue, CD4520): EN and the negated CLK into an AND gate clocking the
# counter, the LEDs of Q0 to Q3; EN, CLK, Q0 at W1 to W3, Q3, Q2, Q1 at E1 to E3, + at N, the reset R at S.
s = blk('U', C('Zähler', 'Counter', 'Compteur'), 1, 3)
module_box(s, 1 * P, 1.6 * P, 7 * P, 22 * P)
s.rect(2.2 * P, 2.6 * P, 4.8 * P, 6.2 * P, w=0.35).note('&', 3.5 * P, 4.4 * P, 2.5, 'centre')
s.poly((4.8 * P, 4.4 * P - 0.7), (4.8 * P, 4.4 * P + 0.7), (4.8 * P + 1.1, 4.4 * P))
s.line((1 * P, H), (2.2 * P, H)).line((1 * P, 3 * H), (1.6 * P, 3 * H), (1.6 * P, 5.4 * P), (2.2 * P - 1.2, 5.4 * P)).circle(2.2 * P - 0.6, 5.4 * P, 1.2)
for k in range(4):
    y0 = 7.0 * P + k * 3.6 * P
    s.rect(2.6 * P, y0, 5.4 * P, y0 + 3.2 * P, w=0.18).dot(H, y0 + 1.6 * P, 1.6, RED)
for k, (inp, out) in enumerate((('EN', 'Q3'), ('CLK', 'Q2'), ('Q0', 'Q1'))):
    y = H + k * T
    s.lead((0, y), (1 * P, y)).lead((T, y), (7 * P, y))
    s.note(inp, 1 * P + 0.3, y + (1.4 if inp == 'CLK' else -1.4), 1.5).note(out, 7 * P - 0.3, y - 1.4, 1.5, 'right')
s.lead((H, 0), (H, 1.6 * P)).lead((H, 3 * T), (H, 22 * P))
s.line((6 * P, 22 * P), (6 * P, 23 * P)).bar(6 * P, 23 * P)
add(s.note('+', H + 0.8, 0.6 * P, 2.2).note('R', H - 0.6, 23.2 * P, 1.6, 'right').labels((0.4, 0.2), None))

s = blk('U', C('Hex-Anzeige', 'Hexadecimal display', 'Afficheur hexadécimal'), 2, 2)
module_box(s, 2 * P, 1.6 * P, 14 * P, 14.4 * P)
sx, sy, sw, sh = 8.6 * P, 3.6 * P, 3.2 * P, 3.4 * P
for (ax, ay), (bx, by) in (((0, 0), (1, 0)), ((1, 0), (1, 1)), ((1, 1), (1, 2)), ((0, 2), (1, 2)), ((0, 1), (0, 2)), ((0, 0), (0, 1)), ((0, 1), (1, 1))):
    gx, gy = 0.5 * (bx - ax), 0.5 * (by - ay)
    s.line((sx + ax * sw + gx, sy + ay * sh + gy), (sx + bx * sw - gx, sy + by * sh - gy), w=0.8)
s.lead((0, H), (2 * P, H)).lead((0, 3 * H), (2 * P, 3 * H)).lead((H, 2 * T), (H, 14.4 * P)).lead((3 * H, 2 * T), (3 * H, 14.4 * P))
s.lead((H, 0), (H, 1.6 * P)).line((T, 14.4 * P), (T, 15 * P)).bar(T, 15 * P)
texts(s, (('1', 2 * P + 0.5, H, 'left'), ('2', 2 * P + 0.5, 3 * H, 'left'), ('4', H, 13.2 * P, 'centre'), ('8', 3 * H, 13.2 * P, 'centre'),
          ('+', H + 0.8, 0.6 * P, 'left'), ('0…F', sx + sw / 2, 11.6 * P, 'centre')), 2.0)
add(s.labels((10 * P, 0.4), None))

s = blk('U', C('Inverter', 'Inverter', 'Inverseur'))
module_box(s, 2 * P, 2 * P, 6 * P, 6 * P)
s.note('1', H, H - 1.2, 2.5, 'centre').dot(H, H + 1.4, 1.4, RED)
s.lead((0, H), (2 * P, H)).circle(6 * P + 0.6, H, 1.2).lead((T, H), (6 * P + 1.2, H)).lead((H, 0), (H, 2 * P))
s.line((H, 6 * P), (H, 7 * P)).bar(H, 7 * P)
add(texts(s, (('E', 0.4, H - 1.6, 'left'), ('A', T - 0.4, H - 1.6, 'right'), ('+', H + 0.8, 1.0 * P, 'left')), 2.0).labels((0.4, 0.2), None))


def gate3(caption, sign, inputs=('E1', 'E2', 'E3')):
    """The logic blocks of today, three cells long and turned upright: inputs at W1 to W3, A at E1, its negation at E3,
    + at N, the base plate below; the LED shows the state of A."""
    s = blk('U', caption, 1, 3)
    module_box(s, 1 * P, 1.6 * P, 7 * P, 22 * P)
    s.note(sign, H, 3 * H, 3.5, 'centre', bold=True).dot(H, 7 * P, 1.6, RED)
    for k, name in enumerate(inputs):
        y = H + k * T
        s.lead((0, y), (1 * P, y)).note(name, 1 * P + 0.4, y - 1.4, 1.6)
    s.lead((T, H), (7 * P, H)).circle(7 * P + 0.6, 5 * H, 1.2).lead((T, 5 * H), (7 * P + 1.2, 5 * H))
    s.note('A', 7 * P - 0.4, H - 1.4, 1.6, 'right').note('A' + BAR, 7 * P - 0.4, 5 * H - 1.4, 1.6, 'right')
    s.lead((H, 0), (H, 1.6 * P)).note('+', H + 0.8, 0.6 * P, 2.2)
    s.line((H, 22 * P), (H, 23 * P)).bar(H, 23 * P)
    return s.labels((0.4, 0.2), None)


add(gate3(C('UND/NAND', 'AND/NAND', 'ET/NON-ET'), '&'))
add(gate3(C('ODER/NOR', 'OR/NOR', 'OU/NON-OU'), '≥1'))
add(gate3(C('EXOR/EXNOR', 'XOR/XNOR', 'OU exclusif/NON-OU exclusif'), '2k+1'))

# The debounced key (no. 2505 of the 2015 catalogue, CD4011): the red key on the line from E at W, the key contact
# in the box of the debouncing flip-flop, A at E, its negation at S2, the LED of Q at N2, + at N1 and S1.
s = blk('U', C('Prellfreie Taste', 'Debounced push button', 'Touche antirebond'), 2, 1)
module_box(s, 6 * P, 1.7 * P, 11 * P, 6 * P)
s.lead((0, H), (H - 2.8, H)).circle(H, H, 5.6, fill=RED).line((H + 2.8, H), (6 * P, H))
s.dot(7.2 * P, 5.0 * P).dot(9.6 * P, 5.0 * P).line((7.2 * P, 5.0 * P), (9.4 * P, 3.5 * P))
s.line((7.2 * P, 5.0 * P), (6.8 * P, 5.0 * P)).line((9.6 * P, 5.0 * P), (10.2 * P, 5.0 * P))
push_head(s, 8.3 * P, 4.25 * P)
s.lead((2 * T, H), (14.6 * P, H), (14.6 * P, 2.6 * P), (11 * P, 2.6 * P))
s.lead((3 * H, T), (3 * H, 5.2 * P), (11 * P, 5.2 * P)).dot(11 * P, 5.2 * P)
s.dashed((3 * H, 0), (3 * H, 4.0 * P - 0.8)).con(3 * H, 0)
s.dashed((9.0 * P, 4.0 * P), (3 * H - 0.8, 4.0 * P)).dot(3 * H, 4.0 * P, 1.6, RED)
s.lead((H, 0), (H, 1.4 * P)).lead((H, T), (H, 6.6 * P))
s.line((2 * P, 6.2 * P), (2 * P, 7 * P)).bar(2 * P, 7 * P)
texts(s, (('E', 0.4, H - 1.6, 'left'), ('Q', 3 * H + 0.8, 0.6 * P, 'left'), ('A', 2 * T - 0.4, H - 1.6, 'right'),
          ('A' + BAR, 3 * H + 0.8, T - 1.0, 'left')), 2.0)
add(plus(plus(s, H, 2.1 * P), H, 5.9 * P).labels((0.4, 0.2), None))

# The JK master-slave flip-flop (no. 2435 of the 2015 catalogue, CD4027): CLK through from W1 to E1, J and Q in the
# middle cell, K and Q̄ in the lower one, + at N, the reset R at S, the LED of Q above the flip-flop.
s = blk('U', C('JK-Master-Slave-Flipflop', 'JK master-slave flip-flop', 'Bascule JK maître-esclave'), 1, 3)
s.lead((0, H), (T, H)).con(T, H).dot(1.4 * P, H).line((1.4 * P, H), (1.4 * P, 16 * P), (2.6 * P, 16 * P))
s.rect(2.6 * P, 10.6 * P, 6.6 * P, 21.4 * P, w=0.35).line((2.6 * P, 16 * P), (6.6 * P, 16 * P), w=0.35)
s.line((6.6 * P - 0.4, 16 * P), (6.6 * P - 0.4, 21.4 * P), w=0.8).line((2.6 * P, 16 * P - 0.8), (2.6 * P + 1.2, 16 * P), (2.6 * P, 16 * P + 0.8))
s.dot(H, 7 * P, 1.6, RED)
for k, (inp, out) in enumerate((('J', 'Q'), ('K', 'Q' + BAR)), 1):
    y = H + k * T
    s.lead((0, y), (2.6 * P, y)).lead((T, y), (6.6 * P, y))
    s.note(inp, 0.4, y - 1.4, 1.6).note(out, 6.6 * P - 1.2, y + (1.4 if k == 1 else -1.4), 1.6, 'right')
s.lead((H, 0), (H, 1.2 * P)).note('+', H + 0.8, 0.6 * P, 2.2).note('CLK', T - 0.4, H - 1.4, 1.6, 'right')
s.lead((H, 3 * T), (H, 21.4 * P)).note('R', H - 0.6, 23.2 * P, 1.6, 'right')
s.line((6 * P, 22.2 * P), (6 * P, 23 * P)).bar(6 * P, 23 * P)
add(s.labels((0.4, 0.2), None))

add(gate3(C('Majoritätsgatter, drei Eingänge', 'Majority gate, three inputs', 'Porte majoritaire, trois entrées'), 'm'))


# ======================================================================================================================
add = page(FOLDER, 'Lectron Passive', 'Lectron Passive', 'Lectron composants passifs')


def ohm_value(s, x, y):
    """The value as the user gives it, followed by the printed Ω."""
    s.note('Ω', x + 0.3, y + 1.25, 2.5)
    return s


s = blk('R', C('Widerstand', 'Resistor', 'Résistance'), ask=True)
two_terminal(s, P)
with s.local(H, H):
    r_body(s)
add(ohm_value(s, H + 0.6, H + 2.0).labels((0.6, 0.6), (H + 0.6, H + 2.0), 'left', 'right'))
s = blk('R', C('Widerstand (senkrecht)', 'Resistor, vertical', 'Résistance verticale'), turn='cw', ask=True)
two_terminal(s, P)
with s.local(H, H):
    r_body(s)
add(ohm_value(s, T - 2.4, H - 1.25).labels((0.6, 0.6), (T - 2.4, H - 1.25), 'left', 'right'))


def pot_tile(caption, turn=None, value=''):
    """A potentiometer W-E with the wiper from N (turned with the block)."""
    s = blk('P', caption, turn=turn, value=value, ask=not value)
    two_terminal(s, P)
    with s.local(H, H):
        r_body(s)
    return s.lead((H, 0), (H, H - 1.0)).arrow((H, H - 3.0), (H, H - 1.0), size=1.2)


add(pot_tile(C('Potentiometer (senkrecht)', 'Potentiometer, vertical', 'Potentiomètre vertical'), 'cw').labels((0.6, 0.6), (H + 1.8, T - 3.6)))
add(pot_tile(C('Potentiometer', 'Potentiometer', 'Potentiomètre'), 'mirror').labels((0.6, 0.6), (H, H - 4.6), 'left', 'centre'))
s = blk('P', C('Einstellbarer Widerstand', 'Variable resistor', 'Résistance variable'), value='10 kΩ')
two_terminal(s, P)
with s.local(H, H):
    r_body(s)
add(s.arrow((H - 3.4, H + 3.0), (H + 3.6, H - 3.2), size=1.2).labels((0.6, 0.6), (H, T - 4.2), 'left', 'centre'))
s = blk('R', C('Fotowiderstand', 'Photoresistor', 'Photorésistance'))
two_terminal(s, P)
with s.local(H, H):
    r_body(s, h=2.6)
    r_body(s, 2 * P - 1.6, 1.0)
light_in(s, H - 1.0, H - 1.6)
add(s.labels((0.6, 0.6), None))
# 15 kΩ with the double arrow across: the thermistor (Heißleiter) of the 1960s.
for turn, de, en, fr in ((None, 'Heißleiter 15 kΩ', 'Thermistor (NTC) 15 kΩ', 'Thermistance (CTN) 15 kΩ'),
                         ('cw', 'Heißleiter 15 kΩ (senkrecht)', 'Thermistor (NTC) 15 kΩ, vertical', 'Thermistance (CTN) 15 kΩ verticale')):
    s = blk('R', C(de, en, fr), turn=turn, value='15 kΩ')
    two_terminal(s, P)
    with s.local(H, H):
        r_body(s)
    s.line((H - 3.4, H + 3.0), (H + 3.6, H - 3.2), start='arrow', end='arrow', size=1.2)
    add(s.labels((0.6, 0.6), (H, T - 4.2) if not turn else (H + 1.6, T - 3.4), 'left', 'centre' if not turn else 'left'))


def cap_tile(caption, polar=False, turn=None, value=''):
    s = blk('C', caption, turn=turn, value=value, ask=not value)
    two_terminal(s, 0.5)
    with s.local(H, H):
        c_body(s, polar)
    return s


s = cap_tile(C('Elektrolytkondensator', 'Electrolytic capacitor', 'Condensateur électrolytique'), True)
add(plus(s, H - 2.6, H - 3.6).labels((0.6, 0.6), (H, H + 3.4), 'left', 'centre'))
s = cap_tile(C('Elektrolytkondensator (senkrecht)', 'Electrolytic capacitor, vertical', 'Condensateur électrolytique vertical'), True, 'cw')
add(plus(s, H + 3.6, H - 2.6).labels((0.6, 0.6), (H + 3.4, H + 0.6)))
add(cap_tile(C('Kondensator', 'Capacitor', 'Condensateur')).labels((0.6, 0.6), (H, H + 3.4), 'left', 'centre'))
add(cap_tile(C('Kondensator (senkrecht)', 'Capacitor, vertical', 'Condensateur vertical'), turn='cw').labels((0.6, 0.6), (H + 3.4, H - 1.25)))
# The flash marks the 250 V of this block.
s = cap_tile(C('Kondensator 0,47 µF (senkrecht)', 'Capacitor 0.47 µF, vertical', 'Condensateur 0,47 µF vertical'), turn='cw', value='0,47 µF')
with s.plain():
    flash(s, H + 3.0, H - 3.4)
add(s.labels((0.6, 0.6), (T - 0.4, H + 3.4), 'left', 'right'))
s = cap_tile(C('Kondensator 0,47 µF', 'Capacitor 0.47 µF', 'Condensateur 0,47 µF'), value='0,47 µF')
flash(s, H + 3.4, H - 2.8)
add(s.labels((0.6, 0.6), (H, H + 3.4), 'left', 'centre'))
for turn, de, en, fr in ((None, 'HF-Spule', 'RF choke', 'Bobine HF'), ('cw', 'HF-Spule (senkrecht)', 'RF choke, vertical', 'Bobine HF verticale')):
    s = blk('L', C(de, en, fr), turn=turn, ask=True)
    two_terminal(s, P)
    with s.local(H, H):
        l_body(s)
        for x in (-P, -P / 3, P / 3):
            s.line((x + 0.3, 1.1), (x + 2 * P / 3 - 0.3, 1.1), w=THICK)
    add(s.labels((0.6, 0.6), (H, H + 3.4) if not turn else (H + 2.8, H - 1.25), 'left', 'centre' if not turn else 'left'))

# The decoupling block: 220 Ω between W and E, from each end an electrolytic capacitor of 100 µF to the base plate
# (its + at the plate, the ground of the system).
s = blk('RC', C('RC-Siebglied', 'RC filter', 'Filtre RC'), value='220 Ω')
s.lead((0, H), (H - 2.5, H)).lead((T, H), (H + 2.5, H)).rect(H - 2.5, H - 0.8, H + 2.5, H + 0.8)
for x, name in ((2 * P, 'GND1'), (6 * P, 'GND2')):
    s.dot(x, H)
    with s.local(x, 5.5 * P, 'ccw'):
        c_body(s, True, 1.6)
    s.line((x, H), (x, 5.5 * P - 1.1)).line((x, 5.5 * P + 1.1), (x, 7 * P)).bar(x, 7 * P, name)
    s.note('+', x + 1.4, 5.5 * P + 1.8, 2.0)
add(s.note('2 × 100 µF', H, 1.4, 1.5, 'centre').labels((H, T - 3.2), (H, H - 4.4), 'centre'))


# ======================================================================================================================
add = page(FOLDER, 'Lectron Power supply', 'Lectron Power supply', 'Lectron alimentation')


def cells(s, y0, y1, minus_first):
    """A battery on the vertical through the middle: two cells from y0 to y1 with the dashed line between them."""
    for y in (y0, y1):
        long_y, short_y = (y + 1.0, y) if minus_first else (y, y + 1.0)
        s.line((H - 2.8, long_y), (H + 2.8, long_y)).line((H - 1.4, short_y), (H + 1.4, short_y), w=0.8)
    s.dashed((H, y0 + 1.0), (H, y1))


s = blk('', C('Batterie 9 V', 'Battery 9 V', 'Pile 9 V'), 1, 3, numbered=False, value='9 V')
cells(s, 9 * P, 14 * P, False)
s.lead((H, 0), (H, 9 * P)).lead((H, 3 * T), (H, 14 * P + 1.0))
add(s.note('+', H + 3.2, 8 * P, 3.0).note(MINUS, H + 3.2, 16.4 * P, 3.0).labels((0.6, 0.6), (H + 3.6, 11.5 * P)))

# The mains unit: 12 V~ in at W and E, the rectifier bridge, the adjustable 1.25 V to 12 V out at N1 (+) and S3 (−).
s = blk('', C('Netzteil', 'Power supply unit', "Bloc d'alimentation"), 3, 1, numbered=False)
cx, d = 3 * H, 3.6
corners = {'l': (cx - d, H), 'r': (cx + d, H), 't': (cx, H - d), 'b': (cx, H + d)}
for a, b in (('l', 't'), ('r', 't'), ('b', 'l'), ('b', 'r')):
    pa, pb = corners[a], corners[b]
    s.line(pa, pb)
    mx, my = (pa[0] + pb[0]) / 2, (pa[1] + pb[1]) / 2
    ang = math.degrees(math.atan2(-(pb[1] - pa[1]), pb[0] - pa[0]))
    s.poly(*[rot(q, (mx, my), ang) for q in ((mx - 0.7, my - 0.9), (mx - 0.7, my + 0.9), (mx + 0.7, my))], fill=BLACK)
    s.line(rot((mx + 0.7, my - 0.9), (mx, my), ang), rot((mx + 0.7, my + 0.9), (mx, my), ang), w=0.35)
s.lead((0, H), corners['l']).lead((3 * T, H), corners['r'])
s.lead((H, 0), (H, 1.2), (cx, 1.2), corners['t']).lead((5 * H, T), (5 * H, T - 1.2), (cx, T - 1.2), corners['b'])
s.circle(17 * P, 2.4 * P, 2.0 * P, w=0.35).arrow((17 * P - 2.0, 2.4 * P + 2.0), (17 * P + 2.2, 2.4 * P - 2.2), size=1.0)
texts(s, (('12 V~', 0.4, H - 1.6, 'left'), ('12 V~', 3 * T - 0.4, H - 1.6, 'right'), ('+', H + 1.0, 2.2, 'left'),
          (MINUS, 5 * H + 1.0, T - 2.2, 'left'), ('1,25…12 V', 5 * H, 5.8 * P, 'centre')), 2.0)
add(s)

s = blk('', C('Batterie 9 V (ältere Ausführung)', 'Battery 9 V (older version)', 'Pile 9 V (ancienne version)'), 1, 3, numbered=False, value='9 V')
cells(s, 9 * P, 14 * P, True)
s.lead((H, 0), (H, 9 * P)).line((H, 14 * P + 1.0), (H, 22 * P)).bar(H, 22 * P)
add(s.note(MINUS, H + 3.2, 8 * P, 3.0).note('+', H + 3.2, 16.4 * P, 3.0).labels((0.6, 0.6), (H + 3.6, 11.5 * P)))
