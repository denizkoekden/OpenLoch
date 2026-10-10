# Planning symbols of the LCN bus for building automation (Issendorff KG) on the new page "Issendorff LCN". A bus module
# is a box (a circle for the flush-mounted modules) split by a double bar into the port fields T, I, P and the output
# fields, a dimmer output with the adjustment arrow; the bus terminals D, N, L at an edge, or the bus cable with one
# stroke per conductor. The form follows Issendorff's planning sheet "LCN Planung" (2007); functions and terminal names
# follow the LCN Produkthandbuch (2010), the installation guides and the Issendorff data sheets of the modules (named
# below with their article numbers).
add = page(EI, 'Issendorff LCN', 'Issendorff LCN', 'Issendorff LCN')
MINUS = '−'


class Frame:
    """Draws a symbol in local coordinates u, v and puts them on the sheet: `fu`/`fv` mirror u about fu / v about fv,
    `turn` swaps the axes, so that the top edge of the local form becomes the left edge and its bottom edge the right."""

    def __init__(self, s, turn=False, fu=None, fv=None):
        self.s, self.turn, self.fu, self.fv = s, turn, fu, fv

    def at(self, u, v):
        u = u if self.fu is None else self.fu - u
        v = v if self.fv is None else self.fv - v
        return (v, u) if self.turn else (u, v)

    def line(self, *points, **kw):
        self.s.line(*[self.at(*q) for q in points], **kw)
        return self

    def arrow(self, a, b, size=1.0):
        self.s.arrow(self.at(*a), self.at(*b), size=size)
        return self

    def rect(self, u0, v0, u1, v1, **kw):
        (x0, y0), (x1, y1) = self.at(u0, v0), self.at(u1, v1)
        self.s.rect(min(x0, x1), min(y0, y1), max(x0, x1), max(y0, y1), **kw)
        return self

    def poly(self, *points, **kw):
        self.s.poly(*[self.at(*q) for q in points], **kw)
        return self

    def dot(self, u, v):
        x, y = self.at(u, v)
        self.s.circle(x, y, 0.8, fill='#000000')
        return self

    def text(self, t, u, v, h=2.5):
        x, y = self.at(u, v)
        self.s.text(t, x, y - h / 2, h, 'centre')
        return self

    def pin(self, name, u, v, du, dv, outside=False, colour=None, text=None):
        """A contact at (u, v), its lead running (du, dv) to the edge; its name inside the edge or, `outside`, beside
        the lead."""
        (px, py), (ex, ey) = self.at(u, v), self.at(u + du, v + dv)
        self.s.line((px, py), (ex, ey), w=0.35 if colour else W)
        if colour:
            self.s.parts[-1]['pen']['color'] = colour
        if abs(ex - px) > abs(ey - py):
            right = ex < px
            if outside:
                label = (px - 0.3 if right else px + 0.3, py - 2.1)
            else:
                label = (ex - 0.5 if right else ex + 0.5, ey - 0.9)
            align = 'right' if right else 'left'
        elif outside:
            label, align = (px + 0.4, py + 0.2 if ey > py else py - 2.0), 'left'
        else:
            label, align = (ex, ey + 0.4 if ey > py else ey - 2.2), 'centre'
        self.s.pin(name, (px, py), label=label, align=align, shown=True, text=text)
        return self


def port(f, u, v, du, dv):
    """The mark of a port connector: a small black triangle on the edge at (u, v), pointing inwards along (du, dv)."""
    return f.poly((u + dv * 0.7, v + du * 0.7), (u - dv * 0.7, v - du * 0.7), (u + du * 1.1, v + dv * 1.1))


def cable(s, x, y):
    """The bus cable leaving downwards at (x, y), one oblique stroke per conductor (D, N, L)."""
    s.line((x, y), (x, y + 3 * P))
    for k in range(3):
        yc = y + 1.2 * P + 0.7 * k
        s.line((x - 1.0, yc + 0.7), (x + 1.0, yc - 0.7))
    return s


def make(f, u, a, b):
    """A make contact along v from a to b (2 P apart), the moving contact pivoting at b."""
    f.line((u, a), (u, a + 0.6 * P)).line((u, b - 0.6 * P), (u, b))
    return f.line((u, b - 0.6 * P), (u - 1.3, a + 0.6 * P + 0.2))


def coil_sign(f, u, v):
    """The relay coil (a rectangle with a diagonal) centred at (u, v), upright in every form."""
    x, y = f.at(u, v)
    f.s.rect(x - 0.9, y - 1.4, x + 0.9, y + 1.4).line((x - 0.9, y + 1.4), (x + 0.9, y - 1.4))
    return f


def plates(f, u, v, along_v=True):
    """The two plates of a capacitor in a lead through (u, v) running along v (or along u)."""
    for d in (-0.5, 0.5):
        if along_v:
            f.line((u - 2.0, v + d), (u + 2.0, v + d), w=THICK)
        else:
            f.line((u + d, v - 2.0), (u + d, v + 2.0), w=THICK)
    return f


# --- Bus modules
# LCN-HU after the Issendorff data sheet "LCN-HU Schalt- und Dimmmodul für die Hutschiene" (art. 30003): load outputs
# A1, A2 fed from L, the electronic ballast outputs 1 to 3 each a pair of terminals + and − (0-10 V, DSI or DALI).
def din(name, caption, out='dim', bus=None, mirror=False, evg=False, load=None, big=False, pairs=False):
    """A bus module for the DIN rail: the port fields T, I, P at the left and the outputs at the right (`mirror`: the
    other way round). `out`: 'dim' the two dimmer outputs, 'none' an empty output field, 'hu' the dimmer outputs and the
    field of the 0-10 V / DSI / DALI outputs 1-3. `bus`: 'cable', or the edge with the terminals D, N, L ('top',
    'bottom'). `evg`: the outputs 1-3 as contacts, `pairs` each as the terminals + and −; `load`: the load outputs A1, A2
    as contacts ('hu'), with their feeds L1', L2' ('ld'); `big`: the fields in a larger housing (LCN-LD)."""
    s = Symbol('LCN', caption, name)
    w, h, cl = 7 * P, (13 if pairs else 10 if out == 'hu' else 6) * P, 2.5 * P
    lead = 2 * P if big else P
    right = lead + P if load else lead      # room for the names of the load contacts beside their leads
    f = Frame(s, fu=w if mirror else None)
    f.rect(0, 0, w, h, w=0.35)
    if big:
        f.rect(-P, -P, w + P, h + P, w=0.35)
    f.line((cl, 0), (cl, h)).line((cl + 0.6, 0), (cl + 0.6, h))
    for k, letter in enumerate('TIP'):
        v = (k + 0.5) * h / 3
        if k:
            f.line((0, k * h / 3), (cl, k * h / 3))
        f.text(letter, cl / 2 + 0.3, v, 3.0)
        port(f, 0, v, 1, 0)
    x0 = cl + 0.6
    if out != 'none':
        a, b = sorted((f.at(x0, 0)[0], f.at(w, 0)[0]))
        a, b = (a + 2.8, b - 1.4) if mirror else (a + 1.4, b - 2.8)     # the arrow always rising to the right
        for k in (0, 1):
            v0, v1 = 3 * P * k, 3 * P * (k + 1)
            if k:
                f.line((x0, v0), (w, v0))
            s.arrow((a, v1 - 1.6), (b, v0 + 1.4), size=1.0)
            f.text(str(k + 1), w - 1.4, v1 - 1.6, 2.5)
            if load == 'ld':
                f.pin(f"L{k + 1}'", w + right, v0 + P, -right, 0, outside=True)
            if load:
                f.pin(f'A{k + 1}', w + right, v0 + 2 * P, -right, 0, outside=True)
    if out == 'hu':
        f.line((x0, 6 * P), (w, 6 * P))
        f.text('0-10V', x0 + 3.8, 6 * P + 1.6, 1.8).text('DSI/DALI', x0 + 3.8, 6 * P + 3.6, 1.5)
        for n in (1, 2, 3):
            if evg and pairs:
                f.pin(f'{n}+', w + right, (5 + 2 * n) * P, -right, 0)
                f.pin(f'{n}-', w + right, (6 + 2 * n) * P, -right, 0, text=f'{n}{MINUS}')
            elif evg:
                f.pin(str(n), w + right, (6 + n) * P, -right, 0)
            else:
                f.text(str(n), w - 1.2, ((5.5 + 2 * n) if pairs else (6 + n)) * P, 1.8)
    if bus in ('top', 'bottom'):
        for k, n in enumerate('DNL'):
            if bus == 'top':
                f.pin(n, (k + 1) * P, -lead, 0, lead, outside=True)
            else:
                f.pin(n, (k + 1) * P, h + lead, 0, -lead, outside=True)
    elif bus == 'cable':
        cable(s, *f.at(cl + 0.3, h))
    m = P if big else 0
    if bus == 'top':
        return s.labels((-m, h + m + 0.8), (-m, h + m + 3.8))
    return s.labels((-m, -m - 6.6), (-m, -m - 3.6))


def flush(name, caption, outputs=True, bus=None):
    """A flush-mounted bus module: a circle split into the port fields T, I and the two dimmer outputs."""
    s = Symbol('LCN', caption, name)
    r = 3 * P
    e = math.sqrt(r * r - 0.09)
    s.circle(0, 0, 2 * r, w=0.35).line((-0.3, -e), (-0.3, e)).line((0.3, -e), (0.3, e)).line((-r, 0), (-0.3, 0))
    for letter, y in (('T', -3.2), ('I', 3.2)):
        x = -math.sqrt(r * r - y * y)
        s.text(letter, -r / 2 + 0.4, y - 1.5, 3.0, 'centre').poly((x, y - 0.7), (x, y + 0.7), (x + 1.1, y))
    if outputs:
        s.line((0.3, 0), (r, 0))
        s.arrow((1.4, -1.2), (4.2, -4.4)).text('1', 6.0, -3.5, 2.5, 'centre')
        s.arrow((1.4, 5.6), (4.2, 2.4)).text('2', 6.0, 1.0, 2.5, 'centre')
    for k, n in enumerate('DNL'):
        x = (k - 1) * P
        edge = math.sqrt(r * r - x * x)
        if bus == 'top':
            s.line((x, -4 * P), (x, -edge)).pin(n, (x, -4 * P), label=(x + 0.4, -4 * P + 0.2), shown=True)
        elif bus == 'bottom':
            s.line((x, 4 * P), (x, edge)).pin(n, (x, 4 * P), label=(x + 0.4, 4 * P - 2.0), shown=True)
    if bus == 'cable':
        cable(s, 0, r)
    return s.labels((r + 1.0, -r), (r + 1.0, -r + 3.0))


VARIANTS = (('Symbol', 'symbol', 'symbole', None), ('mit Leitung', 'with cable', 'avec câble', 'cable'),
            ('Anschlüsse unten', 'terminals at the bottom', 'bornes en bas', 'bottom'),
            ('Anschlüsse oben', 'terminals at the top', 'bornes en haut', 'top'))


def variants(module, draw, mirrored=False, contacts=False):
    """A bus module as bare symbol, with the bus cable, with its terminals at the bottom or the top and (`mirrored`)
    with the outputs at the left; `contacts`: its outputs are contacts where the terminals are."""
    for de, en, fr, bus in VARIANTS:
        add(draw(C(f'{module} ({de})', f'{module} ({en})', f'{module} ({fr})'), bus, False, contacts and bus in ('top', 'bottom')))
    if mirrored:
        add(draw(C(f'{module} (gespiegelt)', f'{module} (mirrored)', f'{module} (en miroir)'), 'top', True, contacts))


variants('LCN-SH', lambda c, bus, m, pins: din('LCN-SH', c, 'dim', bus, m), mirrored=True)
for module, out in (('LCN-UPP', True), ('LCN-UPS', False)):
    variants(module, lambda c, bus, m, pins, module=module, out=out: flush(module, c, out, bus))
variants('LCN-SHS', lambda c, bus, m, pins: din('LCN-SHS', c, 'none', bus, m), mirrored=True)
variants('LCN-HU', lambda c, bus, m, pins: din('LCN-HU', c, 'hu', bus, m, evg=pins, pairs=True), mirrored=True, contacts=True)
add(din('LCN-HU/LD', C('LCN-HU/LD', 'LCN-HU/LD', 'LCN-HU/LD'), 'hu', 'top', evg=True, load='hu', pairs=True))
add(din('LCN-HU/LD', C('LCN-HU/LD (gespiegelt)', 'LCN-HU/LD (mirrored)', 'LCN-HU/LD (en miroir)'), 'hu', 'top', True, evg=True, load='hu', pairs=True))
add(din('LCN-LD', C('LCN-LD (Symbol)', 'LCN-LD (symbol)', 'LCN-LD (symbole)'), 'hu', 'top', evg=True, load='ld', big=True))
add(din('LCN-LD', C('LCN-LD (gespiegelt)', 'LCN-LD (mirrored)', 'LCN-LD (en miroir)'), 'hu', 'top', True, evg=True, load='ld', big=True))


# --- Relay blocks on the P port, their relays drawn as make contacts as on the planning sheet (the R8H and R2H have
# change-over contacts)
def relays(name, caption, outputs, turn):
    """Make contacts from a common rail fed from L to the outputs at the bottom edge of the local form (at the right when
    turned); the supply at the left edge (at the top when turned), the P port at the top (left)."""
    s = Symbol('LCN', caption, name)
    f = Frame(s, turn=turn)
    rail = 2 * P
    w, h = (len(outputs) + 2) * P, rail + 2 * P
    f.rect(0, 0, w, h, w=0.35)
    for k, n in enumerate(('L', 'N')):
        f.pin(n, -P, rail + k * P, P, 0, outside=True)
    f.line((0, rail), ((len(outputs) + 1) * P, rail))
    f.pin('P', P, -P, 0, P)
    coil_sign(f, w - 1.5 * P, rail / 2)
    for k, n in enumerate(outputs):
        u = (k + 2) * P
        make(f, u, rail, h).pin(n, u, h + P, 0, -P, outside=True)
    return s.labels((0, -P - 6.0), (0, -P - 3.2))


def shutters(caption, turn=True, below=False):
    """LCN-R8H set up for shutters: the relays in pairs, each pair fed at its terminal 1, 3, 5, 7 and switching up and
    down (K1/K2 ... K7/K8); all terminals in one row."""
    s = Symbol('LCN', caption, 'LCN-R8H')
    f = Frame(s, turn=turn)
    w, h = 17 * P, 4 * P
    f.rect(0, 0, w, h, w=0.35)
    for k, n in enumerate(('L', 'N')):
        f.pin(n, -P, (k + 1) * P, P, 0, outside=True)
    f.pin('P', P, -P, 0, P)
    coil_sign(f, w - 1.5 * P, P)
    for n in range(4):
        b = (2 + 4 * n) * P
        f.pin(str(2 * n + 1), b, h + P, 0, -P, outside=True).line((b, h), (b, 2 * P), (b + 2 * P, 2 * P))
        for k in (1, 2):
            make(f, b + k * P, 2 * P, h).pin(f'K{2 * n + k}', b + k * P, h + P, 0, -P, outside=True)
    if below:
        bottom = (w if turn else h + P) + 0.8
        return s.labels((0, bottom), (0, bottom + 3.0))
    return s.labels((0, -P - 6.0), (0, -P - 3.2))


def shutters_rows(caption, mirror=False):
    """LCN-R8H for shutters with the feeds 1, 3, 5, 7 along the top edge and the outputs K1..K8 along the bottom."""
    s = Symbol('LCN', caption, 'LCN-R8H')
    w, h = 15 * P, 5 * P
    f = Frame(s, fu=w if mirror else None)
    f.rect(0, 0, w, h, w=0.35)
    for k, n in enumerate(('L', 'N')):
        f.pin(n, (k + 1) * P, -P, 0, P, outside=True)
    f.pin('P', -P, 4 * P, P, 0)
    coil_sign(f, 1.5 * P, 2 * P)
    for n in range(4):
        b = (4 + 3 * n) * P
        f.pin(str(2 * n + 1), b, -P, 0, P, outside=True).line((b, 0), (b, 3 * P), (b + P, 3 * P))
        for k in (0, 1):
            make(f, b + k * P, 3 * P, h).pin(f'K{2 * n + k + 1}', b + k * P, h + P, 0, -P, outside=True)
    return s.labels((w + P + 1.0, 0), (w + P + 1.0, 3.0))


K8 = [f'K{k}' for k in range(8, 0, -1)]
for de, en, fr, turn, names in (('senkrecht', 'vertical', 'vertical', True, K8), ('waagerecht', 'horizontal', 'horizontal', False, K8),
                                ('senkrecht, K1 oben', 'vertical, K1 at the top', 'vertical, K1 en haut', True, K8[::-1]),
                                ('waagerecht, K1 links', 'horizontal, K1 at the left', 'horizontal, K1 à gauche', False, K8[::-1])):
    add(relays('LCN-R8H', C(f'LCN-R8H ({de})', f'LCN-R8H ({en})', f'LCN-R8H ({fr})'), names, turn))
add(shutters(C('LCN-R8H Rollladen', 'LCN-R8H shutters', 'LCN-R8H volets roulants')))
add(shutters(C('LCN-R8H Rollladen (Beschriftung unten)', 'LCN-R8H shutters (labels at the bottom)', 'LCN-R8H volets roulants (repères en bas)'), below=True))
add(shutters(C('LCN-R8H Rollladen (waagerecht)', 'LCN-R8H shutters (horizontal)', 'LCN-R8H volets roulants (horizontal)'), turn=False))
add(shutters_rows(C('LCN-R8H Rollladen (getrennte Reihen)', 'LCN-R8H shutters (separate rows)', 'LCN-R8H volets roulants (rangées séparées)')))
add(shutters_rows(C('LCN-R8H Rollladen (getrennte Reihen, gespiegelt)', 'LCN-R8H shutters (separate rows, mirrored)',
                    'LCN-R8H volets roulants (rangées séparées, en miroir)'), mirror=True))


# LCN-R4M2H after the Issendorff data sheet "LCN-R4M2H Relaismodul mit 4 Ausgängen für je 2 Motoren" (art. 30004): per
# output a mains relay from Lx and a two-pole direction relay switching the motors a and b together, each motor with
# its terminal up (↑) and down (↓).
def motors(caption, turn):
    """The four outputs along the bottom edge of the local form (the right edge when turned), per motor 1a ... 4b the
    terminals ↑ and ↓; the supply L, N, Lx at the left (top), the P port at the top (left)."""
    s = Symbol('LCN', caption, 'LCN-R4M2H')
    f = Frame(s, turn=turn)
    rail, node, w, h = 3 * P, 5 * P, 21 * P, 8 * P
    f.rect(0, 0, w, h, w=0.35)
    for k, n in enumerate(('L', 'N', 'Lx')):
        f.pin(n, -P, (k + 1) * P, P, 0, outside=True)
    f.pin('P', P, -P, 0, P)
    coil_sign(f, w - 1.5 * P, 1.5 * P)
    f.line((0, rail), (18.5 * P, rail))
    for out in range(4):
        u0 = (2 + 5 * out) * P
        make(f, u0 + 1.5 * P, rail, node)
        f.line((u0 + 0.5 * P, node), (u0 + 2.5 * P, node))
        for m, motor in enumerate('ab'):
            um = u0 + 2 * m * P
            f.line((um + 0.5 * P, node), (um + 0.5 * P, node + 0.5 * P), (um + 0.1 * P, h - 1.1 * P))
            for j, arrow in enumerate('↑↓'):
                name = f'{out + 1}{motor}{"UD"[j]}'      # ASCII names (1aU, 1aD), the arrow shown
                f.line((um + j * P, h), (um + j * P, h - P)).pin(name, um + j * P, h + P, 0, -P, outside=True, text=arrow)
            f.text(f'{out + 1}{motor}', um + 0.5 * P, h - 0.4 * P, 1.4)
        f.s.line(f.at(u0 + 0.3 * P, h - 1.9 * P), f.at(u0 + 2.3 * P, h - 1.9 * P))
        f.s.parts[-1]['pen']['style'] = 'dash'
    return s.labels((0, -P - 6.0), (0, -P - 3.2))


add(motors(C('LCN-R4M2H (senkrecht)', 'LCN-R4M2H (vertical)', 'LCN-R4M2H (vertical)'), True))
add(motors(C('LCN-R4M2H (waagerecht)', 'LCN-R4M2H (horizontal)', 'LCN-R4M2H (horizontal)'), False))
add(relays('LCN-R2H', C('LCN-R2H', 'LCN-R2H', 'LCN-R2H'), ['K1', 'K2'], True))
add(relays('LCN-R2H', C('LCN-R2H (Kontakte unten)', 'LCN-R2H (contacts at the bottom)', 'LCN-R2H (contacts en bas)'), ['K1', 'K2'], False))


# --- Tableau modules
def di12(caption, turn, bus=True):
    """LCN-DI12: the key inputs T1..T8 with their common T and the I port in the upper part, the lamp contacts 1..12 and
    the collective alarm contact Z from the common C in the lower part (turned: keys at the left, lamps at the right)."""
    s = Symbol('LCN', caption, 'LCN-DI12')
    f = Frame(s, turn=turn)
    w, h = 15 * P, 8 * P
    f.rect(0, 0, w, h, w=0.35).line((0, 4 * P), (w, 4 * P)).line((0, 4 * P + 0.6), (w, 4 * P + 0.6)).line((10 * P, 0), (10 * P, 4 * P))
    for k, n in enumerate(['T'] + [f'T{i}' for i in range(1, 9)]):
        f.pin(n, (k + 1) * P, -P, 0, P)
    f.text('T', 6 * P, 3 * P, 3.0).text('I', 12.5 * P, 2 * P, 3.0)
    port(f, 12.5 * P, 0, 0, 1)
    f.pin('C', P, h + P, 0, -P, outside=True).line((P, h), (P, 6 * P), (14 * P, 6 * P))
    for k, n in enumerate([str(i) for i in range(1, 13)] + ['Z']):
        make(f, (k + 2) * P, 6 * P, h).pin(n, (k + 2) * P, h + P, 0, -P, outside=True)
    if bus:
        for k, n in enumerate('DNL'):
            f.pin(n, w + P, (k + 1) * P, -P, 0, outside=True)
    return s.labels((0, -6.6), (0, -3.6)) if turn else s.labels((0, -P - 6.0), (0, -P - 3.2))


def tl12(name, caption, turn, supply, leds, common):
    """The tableau modules on the T port: key inputs T1..T8 at the top (left), the LED outputs `leds` at the bottom
    (right), each row with its common (`common`: the printed name), the T port at the left (top), `supply` at the right
    (bottom)."""
    s = Symbol('LCN', caption, name)
    f = Frame(s, turn=turn)
    w, h = (len(leds) + 2) * P, 6 * P
    f.rect(0, 0, w, h, w=0.35).text('LED', 6.5 * P, h / 2, 2.0)
    for k in range(8):
        f.pin(f'T{k + 1}', (k + 1) * P, -P, 0, P)
    f.pin(common + '2', 9 * P, -P, 0, P, text=common)
    for k, n in enumerate(leds + [common + '1']):      # the LED outputs shown by their numbers
        f.pin(n, (k + 1) * P, h + P, 0, -P, text=common if k == len(leds) else str(k + 1))
    f.pin('T', -P, 3 * P, P, 0)
    for k, n in enumerate(supply):
        f.pin(n, w + P, (2 + k) * P, -P, 0, text=MINUS if n == '-' else None)
    return s.labels((0, -P - 6.0), (0, -P - 3.2))


for de, en, fr, turn in (('senkrecht', 'vertical', 'vertical', True), ('waagerecht', 'horizontal', 'horizontal', False)):
    add(di12(C(f'LCN-DI12 ({de})', f'LCN-DI12 ({en})', f'LCN-DI12 ({fr})'), turn))
for de, en, fr, turn in (('senkrecht', 'vertical', 'vertical', True), ('waagerecht', 'horizontal', 'horizontal', False)):
    add(di12(C(f'LCN-DI12 ({de}, ohne Busanschluss)', f'LCN-DI12 ({en}, without bus connection)', f'LCN-DI12 ({fr}, sans raccordement au bus)'), turn, False))
# After the Issendorff data sheets "LCN-TL12R" (art. 30130: flush-mounted, supplied by an LCN-NU16 with +16 V and −, the
# wires T1..T8 and L1..L12 with the commons M, all M bridged in the module), "LCN-TL12H" (art. 30164: common anode,
# terminals 1..12, C and 1..8, C) and "LCN-TLK12H" (art. 30239: common cathode, terminals 1..12, N and 1..8, N); the
# leads to keys and LEDs carry N potential. The key terminals 1..8 are named after the keys T1..T8 here.
NUMBERS = [str(i) for i in range(1, 13)]
for name, supply, leds, common in (('LCN-TL12R', ('+', '-'), [f'L{i}' for i in range(1, 13)], 'M'),
                                   ('LCN-TL12H', ('L', 'N'), NUMBERS, 'C'), ('LCN-TLK12H', ('L', 'N'), NUMBERS, 'N')):
    for de, en, fr, turn in (('senkrecht', 'vertical', 'vertical', True), ('waagerecht', 'horizontal', 'horizontal', False)):
        add(tl12(name, C(f'{name} ({de})', f'{name} ({en})', f'{name} ({fr})'), turn, supply, leds, common))


# --- Key and binary inputs
T8_WIRES = (('0', '#e00000'), ('2', '#ff70b0'), ('4', '#00a000'), ('6', '#0050e0'), None, ('+1', '#e0c000'), ('+2', '#909090'))
SIDES = (('links', 'at the left', 'à gauche', 'left'), ('rechts', 'at the right', 'à droite', 'right'),
         ('oben', 'at the top', 'en haut', 'top'), ('unten', 'at the bottom', 'en bas', 'bottom'))


def t8(caption, side):
    """LCN-T8, the cable set for conventional keys (Issendorff data sheet, art. 30041): its wires with their colours (red
    0, pink 2, green 4, blue 6, yellow +1, grey +2) at `side`, the plug for the T port opposite."""
    s = Symbol('LCN', caption, 'LCN-T8')
    w, h = 8 * P, 3 * P
    f = Frame(s, turn=side in ('left', 'right'), fv=h if side in ('top', 'left') else None)
    f.rect(0, 0, w, h, w=0.35)
    for k, wire in enumerate(T8_WIRES):
        if wire:
            f.pin(wire[0], (k + 1) * P, h + 2 * P, 0, -2 * P, outside=True, colour=wire[1])
    f.pin('T', 4 * P, -P, 0, P)
    if side in ('left', 'right'):
        return s.labels((0, -6.6), (0, -3.6))
    return s.labels((w + 1.0, 0), (w + 1.0, 3.0))


def converter(name, caption, side, others, binary=False, mirror=False):
    """A 230 V input module (LCN-TU4H, LCN-BT4H): a square with a diagonal, the inputs 1..4 and N at `side`, the port
    contacts `others` (name, place) opposite."""
    s = Symbol('LCN', caption, name)
    q = 6 * P
    s.rect(0, 0, q, q, w=0.35).line((0, q), (q, 0))
    if binary:      # the static input sign and the DIP switches 1 (B/T) and 2 (keys 1-4/5-8)
        s.text('230V~', 3.6, 1.0, 1.8, 'centre').line((q - 6.6, q - 5.6), (q - 5.0, q - 5.6), (q - 5.0, q - 7.2), (q - 2.2, q - 7.2), w=0.18)
        for k in (0, 1):
            x = q - 6.8 + 2.4 * k
            s.rect(x, q - 4.6, x + 1.8, q - 1.9, w=0.18).rect(x + 0.4, q - 4.2, x + 1.4, q - 3.2, fill='#000000', w=0.1)
            s.text(str(k + 1), x + 0.9, q - 1.8, 1.5, 'centre')
    else:
        pulse(s, 1.0, 5.0, 3.6, 2.0)
        s.text('230V~', q - 4.0, q - 3.4, 1.8, 'centre')
    turn, fu, fv = {'left': (False, None, None), 'right': (False, q, None), 'top': (True, None, None), 'bottom': (True, q, None)}[side]
    f = Frame(s, turn, fu, q if mirror else fv)
    for k, n in enumerate(('1', '2', '3', '4', 'N')):
        f.pin(n, -P, (k + 1) * P, P, 0, outside=True)
    for n, v in others:
        f.pin(n, q + P, v, -P, 0, outside=True)
    if side == 'top':
        return s.labels((0, q + P + 0.8), (0, q + P + 3.8))
    return s.labels((0, -P - 6.6), (0, -P - 3.8))


for de, en, fr, side in SIDES:
    add(t8(C(f'LCN-T8 (Anschlüsse {de})', f'LCN-T8 (terminals {en})', f'LCN-T8 (bornes {fr})'), side))
TU4H = (('Eingänge links', 'inputs at the left', 'entrées à gauche', 'left'), ('Eingänge rechts', 'inputs at the right', 'entrées à droite', 'right'),
        ('Eingänge unten', 'inputs at the bottom', 'entrées en bas', 'bottom'), ('Eingänge oben', 'inputs at the top', 'entrées en haut', 'top'))
for de, en, fr, side in TU4H:
    add(converter('LCN-TU4H', C(f'LCN-TU4H ({de})', f'LCN-TU4H ({en})', f'LCN-TU4H ({fr})'), side, [('T', 3 * P)]))
add(converter('LCN-TU4H', C('LCN-TU4H (zwei T-Anschlüsse, Eingänge oben)', 'LCN-TU4H (two T ports, inputs at the top)', 'LCN-TU4H (deux ports T, entrées en haut)'),
              'top', [('T', 2 * P), ("T'", 4 * P)]))
add(converter('LCN-TU4H', C('LCN-TU4H (zwei T-Anschlüsse, Eingänge oben, gespiegelt)', 'LCN-TU4H (two T ports, inputs at the top, mirrored)',
                            'LCN-TU4H (deux ports T, entrées en haut, en miroir)'), 'top', [('T', 2 * P), ("T'", 4 * P)], mirror=True))
# LCN-BT4H after the Issendorff data sheet (art. 30055): inputs 1..4 and N, the I port, the DIP switches 1 and 2.
for de, en, fr, side in (TU4H[0], TU4H[3], TU4H[1], TU4H[2]):
    add(converter('LCN-BT4H', C(f'LCN-BT4H ({de})', f'LCN-BT4H ({en})', f'LCN-BT4H ({fr})'), side, [('I', 3 * P)], binary=True))


# --- Base load module: two capacitors from the outputs 1 and 2 to N (Issendorff data sheet LCN-C2GH, art. 30048: two
# inputs and N, each a pair of terminals)
def c2gh(caption, layout):
    s = Symbol('LCN', caption, 'LCN-C2GH')
    if layout in ('top', 'bottom'):
        f = Frame(s, fv=4 * P if layout == 'bottom' else None)
        f.rect(-P, 0, 5 * P, 4 * P, w=0.35)
        for n, u in (('1', 0), ('N', 2 * P), ('2', 4 * P)):
            f.pin(n, u, -P, 0, P, outside=True)
        for u in (0, 4 * P):
            f.line((u, 0), (u, 1.6 * P - 0.5)).line((u, 1.6 * P + 0.5), (u, 3 * P))
            plates(f, u, 1.6 * P)
        f.line((0, 3 * P), (4 * P, 3 * P)).line((2 * P, 0), (2 * P, 3 * P)).dot(2 * P, 3 * P)
        if layout == 'top':
            return s.labels((-P, 4 * P + 0.8), (-P, 4 * P + 3.8))
        return s.labels((-P, -6.6), (-P, -3.6))
    if layout in ('left', 'right'):
        f = Frame(s, fu=8 * P if layout == 'right' else None)
        f.rect(0, 0, 8 * P, 4 * P, w=0.35)
        f.pin('1', -P, 2 * P, P, 0, outside=True).pin('2', 9 * P, 2 * P, -P, 0, outside=True).pin('N', 4 * P, -P, 0, P, outside=True)
        f.line((0, 2 * P), (2 * P - 0.5, 2 * P)).line((2 * P + 0.5, 2 * P), (6 * P - 0.5, 2 * P)).line((6 * P + 0.5, 2 * P), (8 * P, 2 * P))
        plates(f, 2 * P, 2 * P, False)
        plates(f, 6 * P, 2 * P, False)
        f.line((4 * P, 0), (4 * P, 2 * P)).dot(4 * P, 2 * P)
        return s.labels((0, 4 * P + 0.8), (0, 4 * P + 3.8))
    f = Frame(s, fu=5 * P if layout == 'outputs left' else None)
    f.rect(0, -1.0, 5 * P, 5 * P, w=0.35)
    for n, v in (('1', P), ('2', 4 * P)):
        f.pin(n, 6 * P, v, -P, 0, outside=True).line((5 * P, v), (3 * P + 0.5, v)).line((3 * P - 0.5, v), (P, v))
        plates(f, 3 * P, v, False)
    f.line((P, P), (P, 5 * P)).dot(P, 4 * P).pin('N', P, 6 * P, 0, -P, outside=True)
    return s.labels((0, -7.6), (0, -4.6))


for de, en, fr, layout in (('Anschlüsse oben', 'terminals at the top', 'bornes en haut', 'top'),
                           ('Anschlüsse unten', 'terminals at the bottom', 'bornes en bas', 'bottom'),
                           ('Ausgang 1 links', 'output 1 at the left', 'sortie 1 à gauche', 'left'),
                           ('Ausgang 1 rechts', 'output 1 at the right', 'sortie 1 à droite', 'right'),
                           ('N unten, Ausgänge rechts', 'N at the bottom, outputs at the right', 'N en bas, sorties à droite', 'outputs right'),
                           ('N unten, Ausgänge links', 'N at the bottom, outputs at the left', 'N en bas, sorties à gauche', 'outputs left')):
    add(c2gh(C(f'LCN-C2GH ({de})', f'LCN-C2GH ({en})', f'LCN-C2GH ({fr})'), layout))


# --- PC couplers
def coupler(name, caption, usb):
    """A PC coupler on the bus: the terminals D, N, L at the top, the PC interface at the right (a USB socket, or the
    D-sub 9 plug of RS-232). LCN-PKU after the Issendorff data sheet (art. 30172)."""
    s = Symbol('LCN', caption, name)
    f = Frame(s)
    w, h = 6 * P, 6 * P
    s.rect(0, 0, w, h, w=0.35)
    for k, n in enumerate('DNL'):
        f.pin(n, (k + 1) * P, -P, 0, P, outside=True)
    if usb:
        s.rect(w - 0.7, 2 * P, w + 0.7, 4 * P, fill='#000000').text('USB', w - 1.4, 3 * P - 1.0, 2.0, 'right')
    else:
        c = w - 4.0
        s.poly((c - 3.2, 1.6), (c + 3.2, 1.6), (c + 2.5, 4.6), (c - 2.5, 4.6), fill=None)
        for k in range(5):
            s.circle(c - 2.2 + 1.1 * k, 2.6, 0.5, fill='#000000')
        for k in range(4):
            s.circle(c - 1.65 + 1.1 * k, 3.7, 0.5, fill='#000000')
        s.text('RS-232', c, 5.6, 1.8, 'centre')
    return s.labels((0, h + 0.8), (0, h + 3.8))


add(coupler('LCN-PC', C('LCN-PC (RS-232)', 'LCN-PC (RS-232)', 'LCN-PC (RS-232)'), False))
add(coupler('LCN-PKU', C('LCN-PKU (USB)', 'LCN-PKU (USB)', 'LCN-PKU (USB)'), True))
