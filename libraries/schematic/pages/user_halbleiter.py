# The USER pages "SWITCH", "THYRISTOR", "TRANSISTOR", "Wandler", "XFORMER" and "Z Guitar". Switches after DIN EN
# 60617-7 (contacts 07-02, change-over contact with centre-off position, multi-position switch) with open circles at
# the contact points, operating devices after DIN EN 60617-2; thyristors after DIN EN 60617-5 (05-04-01 to 05-04-14,
# the diac after 05-03-09); transistors in the forms of the page "Transistoren"; transformers in core, block and helix
# form after DIN EN 60617-6. The converter modules are boxes with the pin numbers of the TRACO Power data sheets (named
# at each series), the colour-coded audio transformer follows the Jensen JT-11P-1 data sheet, the guitar parts are
# drawn as seen when wiring them. "v" vertical, "h" horizontal: a vertical form is its horizontal form mirrored and
# turned, so that what lies above in the horizontal form lies at the left.
USR = 'USER'
MINUS = '−'
NODE = 1.2      # the open circle at a contact point


def toward(a, b, d):
    """The point d from a towards b."""
    length = math.hypot(b[0] - a[0], b[1] - a[1])
    return (a[0] + (b[0] - a[0]) * d / length, a[1] + (b[1] - a[1]) * d / length)


def flipped(s):
    """The drawing mirrored at the x axis."""
    for o in s.parts:
        if 'points' in o:
            o['points'] = [pt((q[0], -q[1])) for q in o['points']]
        for key in ('centre', 'pos', 'pin'):
            if key in o:
                o[key] = pt((o[key][0], -o[key][1]))
        if o['type'] == 'ellipse' and 'start' in o:
            o['start'], o['stop'] = r(-o['stop'] % 360), r(-o['start'] % 360)
            if o['stop'] <= o['start']:
                o['stop'] = r(o['stop'] + 360)
    return s


def turned(s, quarters=1):
    """The drawing turned clockwise by quarter turns about the insertion point (what ran to the right runs down)."""
    for _ in range(quarters % 4):
        for o in s.parts:
            if 'points' in o:
                o['points'] = [pt((-q[1], q[0])) for q in o['points']]
            for key in ('centre', 'pos', 'pin'):
                if key in o:
                    o[key] = pt((-o[key][1], o[key][0]))
            if o['type'] in ('rectangle', 'ellipse'):
                o['size'] = [o['size'][1], o['size'][0]]
            if o['type'] == 'ellipse' and 'start' in o:
                o['start'], o['stop'] = r((o['start'] - 90) % 360), r((o['stop'] - 90) % 360)
                if o['stop'] <= o['start']:
                    o['stop'] = r(o['stop'] + 360)
    return hidden_texts(s)


def hidden_texts(s):
    """Hidden contact texts back beside their connection points."""
    for o in s.parts:
        if o['type'] == 'contact' and not o['visible']:
            o['pos'], o['align'] = pt((o['pin'][0] + 0.4, o['pin'][1] - 2.2)), 'left'
    return s


def upright(s):
    """The vertical form of a horizontal symbol: mirrored, then turned; what lay above lies at the left."""
    return turned(flipped(s))


def place(s, name, pos, align='left'):
    """The text of contact `name` (or of the drawn text `name`) moved to pos."""
    for o in s.parts:
        if (o['type'] == 'contact' and o['name'] == name) or (o['type'] == 'text' and o['text'] == name):
            o['pos'], o['align'] = pt(pos), align
    return s


# --- SWITCH: contact points as open circles; common 2 of a change-over contact, 1 where the blade rests
def node(s, x, y, d=NODE):
    return s.circle(x, y, d)


def rim(c, towards, d=NODE):
    """The point of the contact circle at c that faces `towards`."""
    return toward(c, towards, d / 2)


def blade(s, pivot, tip, arrow=False, d=NODE):
    """The moving contact from the rim of the pivot's circle to tip."""
    start = toward(pivot, tip, d / 2)
    return s.line(start, tip, end='arrow', size=1.0) if arrow else s.line(start, tip)


def onoff(caption, closed=False):
    """A switch 1-2: the blade turns about the left contact point and rests open above the right one (or on it)."""
    s = Symbol('SW', caption)
    a, b = (P, 0), (3 * P, 0)
    node(s, *a).line((0, 0), (P - NODE / 2, 0))
    node(s, *b).line((3 * P + NODE / 2, 0), (4 * P, 0))
    blade(s, a, (3 * P, -NODE / 2) if closed else (3 * P - 0.2, -2.4))
    s.pin('1', (0, 0)).pin('2', (4 * P, 0), label=(4 * P - 0.4, -2.2), align='right')
    return s.labels((2 * P, -5.6), (2 * P, 1.2), 'centre')


def changeover(caption, kind='plain'):
    """A change-over switch: common 2 at the left, 1 (where the blade rests) above right, 3 below right; 'angle': 1
    leaves upwards and 3 straight on; 'centre': the blade stands in the middle position, marked 0."""
    s = Symbol('SW', caption)
    pivot = (P, 0)
    node(s, *pivot).line((0, 0), (P - NODE / 2, 0)).pin('2', (0, 0), label=(0.2, -2.2), shown=True)
    if kind == 'angle':
        one, three = (3 * P, -P), (3 * P, 0)
        node(s, *one).line((3 * P, -P - NODE / 2), (3 * P, -2 * P))
        node(s, *three).line((3 * P + NODE / 2, 0), (4 * P, 0))
        s.pin('1', (3 * P, -2 * P), label=(3 * P + 0.5, -2 * P - 0.4), shown=True)
        s.pin('3', (4 * P, 0), label=(4 * P - 0.2, -2.2), align='right', shown=True)
    else:
        one, three = (3 * P, -P), (3 * P, P)
        node(s, *one).line((3 * P + NODE / 2, -P), (4 * P, -P))
        node(s, *three).line((3 * P + NODE / 2, P), (4 * P, P))
        s.pin('1', (4 * P, -P), label=(4 * P - 0.2, -P - 2.2), align='right', shown=True)
        s.pin('3', (4 * P, P), label=(4 * P - 0.2, P + 0.2), align='right', shown=True)
    if kind == 'centre':
        blade(s, pivot, (3 * P - 1.4, 0))
        s.text('0', 3 * P, -0.9, 1.8, 'centre')
    else:
        blade(s, pivot, rim(one, pivot))
    if kind == 'angle':
        return s.labels((0.2, -P - 4.4), (2 * P, P + 1.4), 'left', 'centre')
    return s.labels((2 * P, -P - 4.6), (2 * P, P + 1.4), 'centre')


def push_button(caption, side=-1):
    """A make contact 1-2 worked by pushing: the contact bar across both contact points, its plunger with the push sign
    of DIN EN 60617-2 on the side `side` (-1 above)."""
    s = Symbol('SW', caption)
    node(s, P, 0).line((0, 0), (P - NODE / 2, 0))
    node(s, 3 * P, 0).line((3 * P + NODE / 2, 0), (4 * P, 0))
    bar, top = side * 1.6, side * 4.0
    s.line((P - 0.6, bar), (3 * P + 0.6, bar), w=THICK).line((2 * P, bar), (2 * P, top))
    s.line((2 * P - 1.2, top - side * 0.8), (2 * P - 1.2, top), (2 * P + 1.2, top), (2 * P + 1.2, top - side * 0.8))
    s.pin('1', (0, 0)).pin('2', (4 * P, 0), label=(4 * P - 0.4, -2.2), align='right')
    return s.labels((2 * P + 2.0, -5.4), (2 * P, 1.2), 'left', 'centre')


LINK_Y = 2 * P - 0.5       # where the mechanical link crosses the blades


def co_contact(s, x, pins):
    """A change-over contact standing at x as contact('co') draws it: the blade rests on pins[0] (above), pins[1] is the
    common (below), pins[2] the make contact (left); returns where the link meets the blade."""
    s.line((x, 0), (x, P), (x + 1.6, P)).line((x, 4 * P), (x, 3 * P), (x + 2.1, P - 0.4)).line((x - P, 0), (x - P, P + 0.6))
    s.pin(pins[0], (x, 0), label=(x + 0.5, 0.2), shown=True).pin(pins[1], (x, 4 * P), label=(x + 0.5, 4 * P - 2.4), shown=True)
    s.pin(pins[2], (x - P, 0), label=(x - P - 0.4, 0.2), align='right', shown=True)
    t = (3 * P - LINK_Y) / (3 * P - (P - 0.4))
    return (x + 2.1 * t, LINK_Y)


def ganged(caption, poles, momentary):
    """One or two change-over contacts on a dashed link: a switch (hand operated, held by a detent) or a push button."""
    s = Symbol('SW', caption)
    points = [co_contact(s, 3 * P * k, [str(3 * k + 1), str(3 * k + 2), str(3 * k + 3)]) for k in range(poles)]
    x, y = points[0]
    sign = -4.4 if momentary else -6.2
    if momentary:
        dashed(s, (x, y), (sign, y))
        s.line((sign + 0.8, y - 1.2), (sign, y - 1.2), (sign, y + 1.2), (sign + 0.8, y + 1.2))
    else:
        c = (x + sign) / 2 + 0.4
        dashed(s, (x, y), (c + 0.7, y))
        s.line((c + 0.7, y), (c, y + 0.9), (c - 0.7, y))
        dashed(s, (c - 0.7, y), (sign, y))
        s.line((sign, y - 1.2), (sign, y + 1.2))
    if poles > 1:
        dashed(s, points[0], points[-1])
    last = 3 * P * (poles - 1)
    return s.labels((last + 3.0, P + 0.6), (last + 3.0, 2 * P + 1.0))


def rotary_v(caption, n, short=False):
    """A rotary switch: contacts 1..n one below the other on an arc about the wiper's pivot (or, short, in a column with
    their connection points on them); the wiper rests on 1, its common C leaves downwards."""
    s = Symbol('SW', caption)
    yc = (n - 1) * P / 2
    pivot = (P, yc)
    node(s, *pivot)
    if short:
        points = [(3 * P, k * P) for k in range(n)]
    else:
        radius = max(3 * P, yc + 1.5 * P)
        points = [(P + math.sqrt(radius ** 2 - (k * P - yc) ** 2), k * P) for k in range(n)]
        end = math.ceil((P + radius + 1.0) / P) * P
    for k, c in enumerate(points):
        node(s, *c)
        if short:
            s.pin(str(k + 1), c, label=(c[0] + 1.0, c[1] - 0.9), shown=True)
        else:
            s.line((c[0] + NODE / 2, c[1]), (end, c[1])).pin(str(k + 1), (end, c[1]), label=(end + 0.5, c[1] - 0.9), shown=True)
    blade(s, pivot, rim(points[0], pivot), arrow=True)
    s.line((P, yc + NODE / 2), (P, n * P)).pin('C', (P, n * P), label=(P + 0.5, n * P - 2.2), shown=True)
    return s.labels((0, -4.0), (0, n * P + 0.8))


def rotary_h(caption, n):
    """A rotary switch: contacts 1..n side by side on an arc above the pivot, the leads upwards; C leaves to the left."""
    s = Symbol('SW', caption)
    xc = P + (n - 1) * P / 2
    pivot = (xc, 0)
    node(s, *pivot)
    radius = max(3 * P, (n - 1) * P / 2 + 1.5 * P)
    end = -math.ceil((radius + 1.0) / P) * P
    points = [((1 + k) * P, -math.sqrt(radius ** 2 - ((1 + k) * P - xc) ** 2)) for k in range(n)]
    for k, c in enumerate(points):
        node(s, *c).line((c[0], c[1] - NODE / 2), (c[0], end))
        s.pin(str(k + 1), (c[0], end), label=(c[0], end - 2.3), align='centre', shown=True)
    blade(s, pivot, rim(points[0], pivot), arrow=True)
    s.line((0, 0), (xc - NODE / 2, 0)).pin('C', (0, 0), label=(0.2, -2.2), shown=True)
    return s.labels((n * P + 1.4, -4.4), (n * P + 1.4, -1.8))


def rotary_ring(caption, n=12):
    """A rotary switch drawn compact: the contacts on a circle about the wiper, numbered clockwise from below left, each
    led out to the nearest point of the pitch; C leaves downwards through the gap between the last and the first."""
    s = Symbol('SW', caption)
    d, rc, rp = 1.0, 2 * P, 4 * P
    node(s, 0, 0, d)
    first = None
    for k in range(n):
        a = math.radians(-165 + 330 * k / (n - 1))
        dx, dy = math.sin(a), -math.cos(a)
        c = (rc * dx, rc * dy)
        end = (round(rp * dx / P) * P, round(rp * dy / P) * P)
        node(s, *c, d).line(rim(c, end, d), end)
        s.pin(str(k + 1), end, label=(end[0] + 1.5 * dx, end[1] + 1.5 * dy - 0.9), align='centre', shown=True)
        first = first or c
    blade(s, (0, 0), rim(first, (0, 0), d), arrow=True, d=d)
    s.line((0, d / 2), (0, rp)).pin('C', (0, rp), label=(0, rp + 0.4), align='centre', shown=True)
    return s.labels((rp + 1.8, -rp), (rp + 1.8, -rp + 2.8))


add = page(USR, 'SWITCH', 'SWITCH', 'COMMUTATEURS')
s = Symbol('SW', C('Umschalter (Symbol, Pfeil)', 'Change-over switch (sign, arrow)', 'Inverseur (signe, flèche)'))
node(s, 0, 0).circle(3 * P, -P, NODE).circle(3 * P, P, NODE)
add(blade(s, (0, 0), rim((3 * P, -P), (0, 0)), arrow=True).labels((1.5 * P, -P - 4.4), (1.5 * P, P + 1.2), 'centre'))
add(changeover(C('Umschalter (h)', 'Change-over switch (h)', 'Inverseur (h)')))
add(upright(onoff(C('Schalter (v)', 'Switch (v)', 'Interrupteur (v)'))).labels((2.4, P - 0.4), (2.4, 2 * P + 0.4)))
add(onoff(C('Schalter (h)', 'Switch (h)', 'Interrupteur (h)')))
add(upright(onoff(C('Schalter, geschlossen (v)', 'Switch, closed (v)', 'Interrupteur, fermé (v)'), closed=True)).labels((2.4, P - 0.4), (2.4, 2 * P + 0.4)))
add(onoff(C('Schalter, geschlossen (h)', 'Switch, closed (h)', 'Interrupteur, fermé (h)'), closed=True))
add(changeover(C('Umschalter (h, Winkel)', 'Change-over switch (h, angled)', 'Inverseur (h, coudé)'), 'angle'))
s = upright(changeover(C('Umschalter (v, Winkel)', 'Change-over switch (v, angled)', 'Inverseur (v, coudé)'), 'angle'))
place(s, '2', (0.6, 0.2))
place(s, '1', (-2 * P + 0.2, 3 * P - 2.2))
place(s, '3', (0.8, 4 * P - 1.9))
add(s.labels((2.6, P - 0.4), (2.6, 2 * P + 0.4)))
add(changeover(C('Umschalter mit Mittelstellung (h)', 'Change-over switch with centre-off position (h)', 'Inverseur à position médiane (h)'), 'centre'))
s = upright(changeover(C('Umschalter mit Mittelstellung (v)', 'Change-over switch with centre-off position (v)', 'Inverseur à position médiane (v)'), 'centre'))
place(s, '2', (0.6, 0.2))
place(s, '1', (-P - 0.4, 4 * P - 1.9), 'right')
place(s, '3', (P + 0.4, 4 * P - 1.9))
place(s, '0', (0, 3 * P - 0.9), 'centre')
add(s.labels((P + 2.2, P - 0.4), (P + 2.2, 2 * P + 0.4)))
add(upright(push_button(C('Taster (v)', 'Push button (v)', 'Bouton-poussoir (v)'))).labels((2.4, P - 0.4), (2.4, 2 * P + 0.4)))
add(push_button(C('Taster (h)', 'Push button (h)', 'Bouton-poussoir (h)')))
add(ganged(C('Schalter 2 × Umschalter', 'Switch, 2 × change-over', 'Interrupteur, 2 × inverseur'), 2, False))
add(ganged(C('Schalter 2 × Umschalter, Taster', 'Switch, 2 × change-over, momentary', 'Interrupteur, 2 × inverseur, à impulsion'), 2, True))
add(ganged(C('Schalter 1 × Umschalter', 'Switch, 1 × change-over', 'Interrupteur, 1 × inverseur'), 1, False))
add(ganged(C('Schalter 1 × Umschalter, Taster', 'Switch, 1 × change-over, momentary', 'Interrupteur, 1 × inverseur, à impulsion'), 1, True))
add(rotary_ring(C('Drehschalter 1 × 12', 'Rotary switch 1 × 12', 'Commutateur rotatif 1 × 12')))
add(rotary_v(C('Drehschalter 1 × 6 (v)', 'Rotary switch 1 × 6 (v)', 'Commutateur rotatif 1 × 6 (v)'), 6))
add(rotary_v(C('Drehschalter 1 × 4 (v)', 'Rotary switch 1 × 4 (v)', 'Commutateur rotatif 1 × 4 (v)'), 4))
add(rotary_v(C('Drehschalter 1 × 4 (v, kurz)', 'Rotary switch 1 × 4 (v, short)', 'Commutateur rotatif 1 × 4 (v, court)'), 4, short=True))
add(rotary_h(C('Drehschalter 1 × 6 (h)', 'Rotary switch 1 × 6 (h)', 'Commutateur rotatif 1 × 6 (h)'), 6))
add(rotary_v(C('Drehschalter 1 × 3', 'Rotary switch 1 × 3', 'Commutateur rotatif 1 × 3'), 3))


# --- THYRISTOR: anode (or 1) at the left, cathode (or 2) at the right, gates below; outline triangles
AX, KX = 1.5 * P, 2.5 * P      # base and bar of the triangle


def thyristor(prefix, caption, gate=None, turn_off=False, reverse=False, layers=False, names=('A', 'K')):
    """A thyristor after DIN EN 60617-5: gate None (diode thyristor), 'any' (type unspecified: the gate straight down from
    the triangle), 'p' (cathode side: from the bar), 'n' (anode side: from the triangle near its base), 'pn' (tetrode:
    G1 from the bar upwards, G2 at the anode side); a turn-off thyristor has a stroke across the gate, a reverse
    conducting one a foot at the bar; the turn-off and the tetrode thyristors have no foot (IEC 617-5 1983,
    05-04-07 to 05-04-14, as reprinted in IS 12032-5:1993); `layers` the line through the triangle of the four-layer
    diode."""
    s = Symbol(prefix, caption)
    s.line((0, 0), (AX, 0)).line((KX, 0), (4 * P, 0))
    s.poly((AX, -1.5), (AX, 1.5), (KX, 0), fill=None)
    s.line((KX, -1.5), (KX, 1.5), w=0.35)
    s.pin(names[0], (0, 0)).pin(names[1], (4 * P, 0), label=(4 * P - 0.4, -2.2), align='right')
    if layers:
        s.line((2 * P, -1.5), (2 * P, 1.5))
    if reverse:
        s.line((KX, 1.5), (KX - 0.9, 1.5))
    lead = None
    if gate == 'any':
        s.line((2 * P, 0.75), (2 * P, 2 * P)).pin('G', (2 * P, 2 * P), label=(2 * P + 0.5, 2 * P - 2.2))
        lead = 2 * P
    if gate in ('p', 'pn'):
        sign, name = (-1, 'G1') if gate == 'pn' else (1, 'G')
        s.line((KX, sign * 0.7), (3 * P, sign * 1.7), (3 * P, sign * 2 * P)).pin(name, (3 * P, sign * 2 * P), label=(3 * P + 0.5, sign * 2 * P - (0.4 if sign < 0 else 2.2)))
        lead = 3 * P
    if gate in ('n', 'pn'):
        start = (AX + 0.6, 1.5 - 0.6 * 1.5 / P)
        s.line(start, (P, start[1] + 0.9), (P, 2 * P)).pin('G2' if gate == 'pn' else 'G', (P, 2 * P), label=(P - 0.5, 2 * P - 2.2), align='right')
        lead = P
    if turn_off and lead:
        s.line((lead - 0.8, 2 * P - 1.3), (lead + 0.8, 2 * P - 1.3))
    if gate == 'pn':
        return s.labels((-1.0, -4.8), (-1.0, 2.0))
    return s.labels((2 * P, -5.0), (3 * P + 0.8 if gate in ('p', 'any') else 2 * P + 2.2, 1.4), 'centre', 'left')


def bidirectional(prefix, caption, gate=False, layers=False, names=('1', '2')):
    """Two opposite triangles between two bars (DIN EN 60617-5 05-03-09 diac); with the line through both triangles the
    bidirectional thyristor diode (05-04-03), with a gate at the bar of the right terminal the triac (05-04-11)."""
    s = Symbol(prefix, caption)
    s.line((0, 0), (AX, 0)).line((KX, 0), (4 * P, 0))
    s.line((AX, -2.6), (AX, 2.6), w=0.35).line((KX, -2.6), (KX, 2.6), w=0.35)
    s.poly((AX, -2.4), (AX, -0.1), (KX, -1.25), fill=None).poly((KX, 0.1), (KX, 2.4), (AX, 1.25), fill=None)
    if layers:
        s.line((2 * P, -2.6), (2 * P, 2.6))
    s.pin(names[0], (0, 0)).pin(names[1], (4 * P, 0), label=(4 * P - 0.4, -2.2), align='right')
    if gate:
        s.line((KX, 1.8), (3 * P, 2.6), (3 * P, 2 * P)).pin('G', (3 * P, 2 * P), label=(3 * P + 0.5, 2 * P - 2.2))
    return s.labels((2 * P, -6.0), (3 * P + 0.8, 1.4) if gate else (2 * P, 3.0), 'centre', 'left' if gate else 'centre')


def vertical(s, x=-3.4):
    """The vertical form: anode at the top, the gates at the right, the texts at the left."""
    upright(s)
    return s.labels((x, P - 0.4), (x, 2 * P + 0.4), 'right')


add = page(USR, 'THYRISTOR', 'THYRISTOR', 'THYRISTORS')
for de, en, fr, flags in (('Thyristor', 'Thyristor', 'Thyristor', {'gate': 'any'}),
                          ('Thyristor, P-Gate', 'Thyristor, P-gate', 'Thyristor, gâchette P', {'gate': 'p'}),
                          ('Thyristor, N-Gate', 'Thyristor, N-gate', 'Thyristor, gâchette N', {'gate': 'n'}),
                          ('GTO-Thyristor, P-Gate', 'GTO thyristor, P-gate', 'Thyristor GTO, gâchette P', {'gate': 'p', 'turn_off': True}),
                          ('GTO-Thyristor, N-Gate', 'GTO thyristor, N-gate', 'Thyristor GTO, gâchette N', {'gate': 'n', 'turn_off': True}),
                          ('Thyristortetrode', 'Tetrode thyristor', 'Thyristor tétrode', {'gate': 'pn'}),
                          ('Thyristor, rückwärts leitend, P-Gate', 'Reverse conducting thyristor, P-gate', 'Thyristor à conduction inverse, gâchette P',
                           {'gate': 'p', 'reverse': True})):
    s = vertical(thyristor('SCR', C(de + ' (v)', en + ' (v)', fr + ' (v)'), **flags))
    if flags['gate'] == 'pn':
        s.labels((-3.0, 0.4), (-3.0, 3.0), 'right')
    add(s)
    add(thyristor('SCR', C(de + ' (h)', en + ' (h)', fr + ' (h)'), **flags))
add(vertical(thyristor('D', C('Vierschichtdiode (v)', 'Four-layer diode (v)', 'Diode à quatre couches (v)'), layers=True), -2.4))
add(thyristor('D', C('Vierschichtdiode (h)', 'Four-layer diode (h)', 'Diode à quatre couches (h)'), layers=True))
add(vertical(bidirectional('SCR', C('Triac (v)', 'Triac (v)', 'Triac (v)'), gate=True, names=('A2', 'A1'))))
add(bidirectional('SCR', C('Triac (h)', 'Triac (h)', 'Triac (h)'), gate=True, names=('A2', 'A1')))
add(vertical(bidirectional('D', C('Diac (v)', 'Diac (v)', 'Diac (v)'))))
add(bidirectional('D', C('Diac (h)', 'Diac (h)', 'Diac (h)')))
add(vertical(bidirectional('D', C('Zweirichtungs-Thyristordiode (v)', 'Bidirectional thyristor diode (v)', 'Thyristor diode bidirectionnel (v)'), layers=True)))
add(bidirectional('D', C('Zweirichtungs-Thyristordiode (h)', 'Bidirectional thyristor diode (h)', 'Thyristor diode bidirectionnel (h)'), layers=True))


# --- TRANSISTOR: the forms of the page "Transistoren"; a horizontal form has its base (gate) below
def horizontal(s):
    """The vertical transistor turned a quarter counter-clockwise: base or gate below, the texts above."""
    turned(s, 3)
    return s.labels((0, -14.4), (0, -11.8), 'centre')


def body_diode(s, p=False, x=7.0):
    """The inverse diode of a power MOSFET between drain (above) and source (below), inside the envelope, as the
    symbols of the data sheets show it (onsemi BS170/MMBF170, MMBF170/D Rev. 5, 2017; Philips BS250, April 1995)."""
    s.line((2 * P, -1.7), (x, -1.7), (x, -0.6)).line((x, 0.6), (x, 1.7), (2 * P, 1.7))
    if p:
        return s.poly((x - 0.8, -0.6), (x + 0.8, -0.6), (x, 0.6), fill=None).line((x - 0.8, 0.6), (x + 0.8, 0.6), w=0.35)
    return s.poly((x - 0.8, 0.6), (x + 0.8, 0.6), (x, -0.6), fill=None).line((x - 0.8, -0.6), (x + 0.8, -0.6), w=0.35)


def valued(s, value):
    s.prefix, s.value = 'Q', value
    return s


def vmos(caption, value, p=False):
    return valued(body_diode(mosfet(p, False, caption), p), value)


def dual_gate_mos(caption, value):
    """N-channel depletion MOSFET with two gates: G2 (drain side) above, G1 (source side) below, substrate at the
    source (Philips data sheet BF960, December 1988: depletion type, source and substrate interconnected)."""
    s = Symbol('Q', caption, value)
    s.circle(4.6, 0, 7.4)
    s.line((0, -P), (2.6, -P)).line((2.6, -2.2), (2.6, -0.4)).line((0, P), (2.6, P)).line((2.6, 0.4), (2.6, 2.2))
    s.line((3.4, -2.2), (3.4, 2.2), w=THICK)
    s.line((3.4, -1.7), (2 * P, -1.7), (2 * P, -2 * P)).line((3.4, 1.7), (2 * P, 1.7), (2 * P, 2 * P))
    s.line((2 * P, 0), (3.4, 0), end='arrow', size=1.3).line((2 * P, 0), (2 * P, 1.7))
    s.pin('G2', (0, -P)).pin('G1', (0, P)).pin('D', (2 * P, -2 * P)).pin('S', (2 * P, 2 * P))
    return s.labels((9.2, -2.6), (9.2, 0.4))


def ujt(caption, value):
    """Unijunction transistor with N-type base: the emitter slanting onto the base bar, B2 above, B1 below."""
    s = Symbol('Q', caption, value)
    s.circle(4.6, 0, 7.4).line((3.4, -2.2), (3.4, 2.2), w=THICK)
    s.line((0, P), (1.2, P)).line((1.2, P), (3.4, -0.4), end='arrow', size=1.3)
    s.line((3.4, -1.7), (2 * P, -1.7), (2 * P, -2 * P)).line((3.4, 1.7), (2 * P, 1.7), (2 * P, 2 * P))
    s.pin('E', (0, P)).pin('B2', (2 * P, -2 * P)).pin('B1', (2 * P, 2 * P))
    return s.labels((9.2, -2.6), (9.2, 0.4))


def large_npn(caption, value):
    """The NPN transistor drawn larger: collector and emitter three pitches above and below the base."""
    s = Symbol('Q', caption, value)
    s.circle(5.8, 0, 10.4)
    s.line((0, 0), (4.0, 0)).line((4.0, -2.8), (4.0, 2.8), w=THICK)
    s.line((4.0, -1.2), (3 * P, -4.0), (3 * P, -3 * P))
    s.line((4.0, 1.2), (3 * P, 4.0), end='arrow', size=1.6).line((3 * P, 4.0), (3 * P, 3 * P))
    s.pin('B', (0, 0)).pin('C', (3 * P, -3 * P)).pin('E', (3 * P, 3 * P))
    return s.labels((11.8, -3.0), (11.8, 0.2))


def large_jfet(caption, value):
    """The N-channel JFET drawn larger: drain and source three pitches above and below, the gate opposite the source."""
    s = Symbol('Q', caption, value)
    s.circle(5.8, 0, 10.4)
    s.line((0, P), (4.4, P), end='arrow', size=1.6).line((4.4, -4.0), (4.4, 4.0), w=THICK)
    s.line((4.4, -2.4), (3 * P, -2.4), (3 * P, -3 * P)).line((4.4, P), (3 * P, P), (3 * P, 3 * P))
    s.pin('G', (0, P)).pin('D', (3 * P, -3 * P)).pin('S', (3 * P, 3 * P))
    return s.labels((11.8, -3.0), (11.8, 0.2))


add = page(USR, 'TRANSISTOR', 'TRANSISTOR', 'TRANSISTORS')
add(valued(transistor(False, C('NPN-Transistor (v)', 'NPN transistor (v)', 'Transistor NPN (v)')), 'BC546'))
add(valued(transistor(True, C('PNP-Transistor (v)', 'PNP transistor (v)', 'Transistor PNP (v)')), 'BC556'))
add(horizontal(valued(transistor(False, C('NPN-Transistor (h)', 'NPN transistor (h)', 'Transistor NPN (h)')), 'BC546')))
add(horizontal(valued(transistor(True, C('PNP-Transistor (h)', 'PNP transistor (h)', 'Transistor PNP (h)')), 'BC556')))
add(valued(jfet(False, C('J-FET N-Kanal (v)', 'N-channel JFET (v)', 'JFET canal N (v)')), 'BF245'))
add(horizontal(valued(jfet(False, C('J-FET N-Kanal (h)', 'N-channel JFET (h)', 'JFET canal N (h)')), 'BF245')))
add(vmos(C('N-Kanal-VMOS (v)', 'N-channel VMOS (v)', 'VMOS canal N (v)'), 'BS170'))
add(horizontal(vmos(C('N-Kanal-VMOS (h)', 'N-channel VMOS (h)', 'VMOS canal N (h)'), 'BS170')))
add(vmos(C('P-Kanal-VMOS (v)', 'P-channel VMOS (v)', 'VMOS canal P (v)'), 'BS250', p=True))
add(horizontal(vmos(C('P-Kanal-VMOS (h)', 'P-channel VMOS (h)', 'VMOS canal P (h)'), 'BS250', p=True)))
add(dual_gate_mos(C('Dual-Gate-MOSFET (v)', 'Dual-gate MOSFET (v)', 'MOSFET double grille (v)'), 'BF960'))
add(ujt(C('Unijunction-Transistor (v)', 'Unijunction transistor (v)', 'Transistor unijonction (v)'), '2N2646'))
add(large_npn(C('NPN-Transistor, groß (v)', 'NPN transistor, large (v)', 'Transistor NPN, grand (v)'), 'BC546C'))
add(large_jfet(C('J-FET N-Kanal, groß (v)', 'N-channel JFET, large (v)', 'JFET canal N, grand (v)'), '2SK30A'))


# --- Wandler: TRACO Power converter modules as boxes, inputs left, outputs right, the pin numbers of the data sheets
# outside, the functions inside (+E/−E input, ~ mains input, +A/−A output, COM common). The converter sign (DIN EN
# 60617-6: a square with its diagonal, input kind above left, output kind below right) in the middle.
def shown(text):
    return text.replace('-', MINUS)


def converter(caption, value, inputs, outputs, ac=False, numbered=True):
    """Rows from the top: (contact, function) or None for an empty row. Without data sheet (`numbered` False) the
    contacts are named by their function and no numbers are shown."""
    rows = max(len(inputs), len(outputs))
    h = (rows + 1) * P
    x0, x1 = P, 9 * P
    s = Symbol('G', caption, value)
    s.rect(x0, 0, x1, h, w=0.35)
    for side, items in ((0, inputs), (1, outputs)):
        for k, item in enumerate(items):
            if not item:
                continue
            name, function = item
            y = (k + 1) * P
            if side == 0:
                s.line((0, y), (x0, y)).text(shown(function), x0 + 0.6, y - 1.0, 1.8)
                s.pin(name, (0, y), label=(0.2, y - 2.2), shown=numbered, text=shown(name))
            else:
                s.line((x1, y), (x1 + P, y)).text(shown(function), x1 - 0.6, y - 1.0, 1.8, 'right')
                s.pin(name, (x1 + P, y), label=(x1 + P - 0.2, y - 2.2), align='right', shown=numbered, text=shown(name))
    a = min(5.6, h - 2.0)
    cx, cy = (x0 + x1) / 2, h / 2
    s.rect(cx - a / 2, cy - a / 2, cx + a / 2, cy + a / 2).line((cx - a / 2, cy + a / 2), (cx + a / 2, cy - a / 2))
    s.text('~' if ac else '=', cx - a / 4, cy - a / 2 + 0.2, 1.8, 'centre').text('=', cx + a / 4, cy + a / 2 - 2.0, 1.8, 'centre')
    return s.labels((x0, -3.4), (x0, h + 0.6))


SINGLE = ([('+E', '+E'), ('-E', '-E')], [('+A', '+A'), ('-A', '-A')])
DUAL = ([('+E', '+E'), None, ('-E', '-E')], [('+A', '+A'), ('COM', 'COM'), ('-A', '-A')])
TED_IN = [('1', '+E'), ('24', '+E'), None, ('12', '-E'), ('13', '-E')]
TEN3_IN = [('22', '+E'), ('23', '+E'), None, ('2', '-E'), ('3', '-E')]
TPM15_TRIPLE = [('4', '+A1'), ('2', '-A1'), ('5', '+A2'), ('3', 'COM2/3'), ('1', '-A3')]   # output 1 floating

add = page(USR, 'Wandler', 'Wandler', 'Convertisseurs')
dc = lambda de, en, fr: C('DC/DC-Wandler ' + de, 'DC/DC converter ' + en, 'Convertisseur CC/CC ' + fr)
mains = lambda de, en, fr: C('AC/DC-Wandler ' + de, 'AC/DC converter ' + en, 'Convertisseur CA/CC ' + fr)
# TME series, 1 W (data sheet "TME Series, 1 Watt", Rev. July 2, 2026)
add(converter(dc('1 W TME', '1 W TME', '1 W TME'), 'TME 1212', [('2', '+E'), ('1', '-E')], [('4', '+A'), ('3', '-A')]))
# TMA series, 1 W (data sheet "TMA Series, 1 Watt", Rev. July 2, 2026): there is no pin 3, 5 is the common of the dual
# models
add(converter(dc('1 W TMA (dual)', '1 W TMA (dual)', '1 W TMA (double)'), 'TMA 0505', [('1', '+E'), None, ('2', '-E')], [('6', '+A'), ('5', 'COM'), ('4', '-A')]))
# TEN 2: no series of that name in TRACO's lists (series overview 2005, obsolete products 09/05); the contacts named by
# function
add(converter(dc('2 W TEN 2 (single)', '2 W TEN 2 (single)', '2 W TEN 2 (simple)'), 'TEN 2-0511', *SINGLE, numbered=False))
add(converter(dc('2 W TEN 2 (dual)', '2 W TEN 2 (dual)', '2 W TEN 2 (double)'), 'TEN 2-0521', *DUAL, numbered=False))
# TED series, 2 W, DIP 24 (data sheet "DC/DC-Konverter TED Serie 2 Watt", Rev. 12/01): every function on two pins, the
# two outputs of the dual models isolated from each other
add(converter(dc('2 W TED 2 (single)', '2 W TED 2 (single)', '2 W TED 2 (simple)'), 'TED 2-0511', TED_IN,
              [('11', '+A'), ('14', '+A'), None, ('10', '-A'), ('15', '-A')]))
add(converter(dc('2 W TED 2 (dual)', '2 W TED 2 (dual)', '2 W TED 2 (double)'), 'TED 2-0521', TED_IN[:2] + [None] * 4 + TED_IN[3:],
              [('11', '+A1'), ('14', '+A1'), ('10', '-A1'), ('15', '-A1'), ('3', '+A2'), ('22', '+A2'), ('2', '-A2'), ('23', '-A2')]))
# TEN 3 series, 3 W, DIP 24 (data sheet "TEN 3 Series, 3 Watt", Rev. July 1, 2026); pin 11 of the single models is not
# connected
add(converter(dc('3 W TEN 3 (single)', '3 W TEN 3 (single)', '3 W TEN 3 (simple)'), 'TEN 3-0510', TEN3_IN, [('14', '+A'), None, None, None, ('16', '-A')]))
add(converter(dc('3 W TEN 3 (dual)', '3 W TEN 3 (dual)', '3 W TEN 3 (double)'), 'TEN 3-0521', TEN3_IN, [('14', '+A'), None, ('9', 'COM'), ('16', 'COM'), ('11', '-A')]))
# TEN 10 series, 10 W (data sheet "TEN 10 Series, 10 Watt", Rev. July 1, 2026)
add(converter(dc('10 W TEN 10 (single)', '10 W TEN 10 (single)', '10 W TEN 10 (simple)'), 'TEN 10-1210', [('1', '+E'), ('2', '-E')], [('3', '+A'), ('5', '-A')]))
add(converter(dc('10 W TEN 10 (dual)', '10 W TEN 10 (dual)', '10 W TEN 10 (double)'), 'TEN 10-1221', [('1', '+E'), None, ('2', '-E')], [('3', '+A'), ('4', 'COM'), ('5', '-A')]))
# TEN 12: TRACO "DC/DC Converters TEN 12WI Series, 12 Watt" (Rev. 05/10); TAP 15: TRACO "DC/DC Converter TAP Series
# 15 Watt" (Rev. 01/03): 1 +Vin, 2 -Vin, 3 +Vout, 4 common (dual), 5 -Vout
add(converter(dc('12 W TEN 12 (single)', '12 W TEN 12 (single)', '12 W TEN 12 (simple)'), 'TEN 12-2410', [('1', '+E'), ('2', '-E')], [('3', '+A'), ('5', '-A')]))
add(converter(dc('12 W TEN 12 (dual)', '12 W TEN 12 (dual)', '12 W TEN 12 (double)'), 'TEN 12-2421', [('1', '+E'), None, ('2', '-E')], [('3', '+A'), ('4', 'COM'), ('5', '-A')]))
add(converter(dc('15 W TAP 15 (dual)', '15 W TAP 15 (dual)', '15 W TAP 15 (double)'), 'TAP 1222', [('1', '+E'), None, ('2', '-E')], [('3', '+A'), ('4', 'COM'), ('5', '-A')]))
# TPM series, AC/DC 5 to 40 W (data sheet "AC/DC Power Modules TPM Series, 5 to 40 Watt", Rev. October 11, 2013)
add(converter(mains('5 W TPM 05 (single)', '5 W TPM 05 (single)', '5 W TPM 05 (simple)'), 'TPM 05105', [('1', '~'), ('2', '~')], [('5', '+A'), ('3', '-A')], ac=True))
add(converter(dc('15 W TAP 15 (single)', '15 W TAP 15 (single)', '15 W TAP 15 (simple)'), 'TAP 1211', [('1', '+E'), ('2', '-E')], [('3', '+A'), ('5', '-A')]))
add(converter(mains('5 W TPM 05 (dual)', '5 W TPM 05 (dual)', '5 W TPM 05 (double)'), 'TPM 05212', [('1', '~'), None, ('2', '~')], [('5', '+A'), ('4', 'COM'), ('3', '-A')], ac=True))
add(converter(mains('10/15 W TPM 15 (dual)', '10/15 W TPM 15 (dual)', '10/15 W TPM 15 (double)'), 'TPM 15212', [('6', '~'), None, ('7', '~')], [('5', '+A'), ('3', 'COM'), ('1', '-A')], ac=True))
add(converter(mains('10/15 W TPM 15 (triple)', '10/15 W TPM 15 (triple)', '10/15 W TPM 15 (triple)'), 'TPM 15512', [('6', '~'), None, None, None, ('7', '~')], TPM15_TRIPLE, ac=True))
add(converter(mains('30 W TPM 30 (single)', '30 W TPM 30 (single)', '30 W TPM 30 (simple)'), 'TPM 30105', [('6', '~'), ('7', '~')], [('5', '+A'), ('3', '-A')], ac=True))
add(converter(mains('30 W TPM 30 (dual, symmetrisch)', '30 W TPM 30 (dual, symmetrical)', '30 W TPM 30 (double, symétrique)'), 'TPM 30212',
              [('6', '~'), None, ('7', '~')], [('5', '+A'), ('3', 'COM'), ('1', '-A')], ac=True))
add(converter(mains('30 W TPM 30 (dual, asymmetrisch)', '30 W TPM 30 (dual, asymmetrical)', '30 W TPM 30 (double, asymétrique)'), '',
              [('6', '~'), None, None, ('7', '~')], [('2', '+A1'), ('1', '-A1'), ('5', '+A2'), ('4', '-A2')], ac=True))
add(converter(mains('30 W TPM 30 (triple)', '30 W TPM 30 (triple)', '30 W TPM 30 (triple)'), 'TPM 30512', [('6', '~'), None, None, None, ('7', '~')], TPM15_TRIPLE, ac=True))
add(converter(mains('15 W TPM 15 (triple, Variante)', '15 W TPM 15 (triple, variant)', '15 W TPM 15 (triple, variante)'), 'TPM 15515',
              [('6', '~'), None, None, None, ('7', '~')], TPM15_TRIPLE, ac=True))


# --- XFORMER: core form (windings as black blocks on the legs of a core frame), block and helix form (windings above
# and below a core of two heavy lines), the colour-coded audio transformer
def labelled(s, name, at, label, align='left'):
    return s.pin(name, at, label=label, align=align, shown=True)


def frame_core(caption, value, width, secondaries=1):
    """A core frame `width` wide and 8 pitches high; the primary block on the left leg (1-2), one or two secondary blocks
    on the right leg (3-4, 5-6)."""
    s = Symbol('T', caption, value)
    top, leg = -4 * P, 2.6
    s.rect(0, top, width, -top, w=0.35).rect(leg, top + leg, width - leg, -top - leg, w=0.35)
    s.rect(-1.0, -P - 1.6, leg + 1.0, P + 1.6, fill='#000000')
    for name, y in (('1', -P), ('2', P)):
        labelled(s.line((-1.0, y), (-2 * P, y)), name, (-2 * P, y), (-2 * P + 0.2, y - 2.2))
    right = width + 2 * P
    blocks = [(-P - 1.6, P + 1.6, ('3', '4'), (-P, P))] if secondaries == 1 else \
        [(-2 * P - 1.0, -P + 1.0, ('3', '4'), (-2 * P, -P)), (P - 1.0, 2 * P + 1.0, ('5', '6'), (P, 2 * P))]
    for y0, y1, names, ys in blocks:
        s.rect(width - leg - 1.0, y0, width + 1.0, y1, fill='#000000')
        for name, y in zip(names, ys):
            labelled(s.line((width + 1.0, y), (right, y)), name, (right, y), (right - 0.2, y - 2.2), 'right')
    return s.labels((0, top - 6.0), (0, top - 3.4))


def helix(s, x0, x1, y, towards, turns, half=1.4, back=0.5):
    """A winding as loops (helix form) along x from x0 to x1 about the height y: wide arcs towards the core (`towards`
    +1 down, -1 up), small loops on the other side where both ends lie; a prolate trochoid drawn as a polyline.
    Returns its end height."""
    a = (x1 - x0) / (2 * math.pi * turns)
    b = 1.5 * a
    steps = 24 * turns
    points = []
    for k in range(steps + 1):
        t = 2 * math.pi * turns * k / steps
        points.append((x0 + a * t - b * math.sin(t), y + towards * ((half - back) / 2 - (half + back) / 2 * math.cos(t))))
    s.line(*points)
    return y - towards * back


def block_form(caption, value, tops, bottoms, form='block', tap=None):
    """Windings above (primaries) and below (secondaries) a core of two heavy lines; each winding (x0, x1, name0,
    name1) has its contacts at x0 and x1 two pitches above or below the core; a centre tap of the lower winding at
    `tap` (x, name)."""
    s = Symbol('T', caption, value)
    xs = [w[0] for w in tops + bottoms] + [w[1] for w in tops + bottoms]
    left, right = min(xs), max(xs)
    s.line((left, -0.6), (right, -0.6), w=THICK).line((left, 0.6), (right, 0.6), w=THICK)
    for side, windings in ((-1, tops), (1, bottoms)):
        yb, end = side * 2.6, side * 2 * P
        for x0, x1, a, b in windings:
            if form == 'block':
                s.rect(x0 + 0.8, yb - 0.9, x1 - 0.8, yb + 0.9, fill='#000000')
                for x, xi in ((x0, x0 + 0.8), (x1, x1 - 0.8)):
                    s.line((xi, yb), (x, yb), (x, end))
            else:
                turns = max(2, round((x1 - x0) / 4.0))
                if tap and side > 0:
                    turns += turns % 2
                y = helix(s, x0, x1, yb, -side, turns)
                s.line((x0, y), (x0, end)).line((x1, y), (x1, end))
                if tap and side > 0:
                    s.line((tap[0], y), (tap[0], end))
                    labelled(s, tap[1], (tap[0], end), (tap[0], end + 0.3), 'centre')
            for name, x in ((a, x0), (b, x1)):
                labelled(s, name, (x, end), (x, end - 2.4) if side < 0 else (x, end + 0.3), 'centre')
    return s.labels((right + 1.6, -2 * P), (right + 1.6, -2 * P + 2.8))


def colour_coded(caption, can=False):
    """A 1:1 audio transformer after the Jensen JT-11P-1 data sheet: primary RD (start, dot) to BN, secondary YE (start,
    dot) to OR, the electrostatic shield WH (dashed) and the case BK (on the core); 10 x 20 mm, or with the can drawn
    around it 20 x 20 mm, BK then on the can."""
    s = Symbol('T', caption)
    dx = 2 * P if can else 0           # everything inside the can moves right
    xp, xs = P + dx, 3 * P + dx        # primary and secondary winding
    right, bottom = 4 * P + 2 * dx, 8 * P
    for k in range(8):
        y = P + (k + 0.5) * P / 2
        s.arc(xp, y, P / 2, 270, 450).arc(xs, y, P / 2, 90, 270)
    s.line((0, 0), (xp, 0), (xp, P)).line((xp, 5 * P), (xp, 6 * P), (0, 6 * P))
    s.line((right, 0), (xs, 0), (xs, P)).line((xs, 5 * P), (xs, 6 * P), (right, 6 * P))
    s.circle(xp - 1.2, P + 0.4, 0.7, fill='#000000').circle(xs + 1.2, P + 0.4, 0.7, fill='#000000')
    core, shield = 2 * P + dx + 0.4, 2 * P + dx - 0.8
    s.line((core, 0.7 * P), (core, 5.3 * P), w=THICK)
    dashed(s, (shield, 0.6 * P), (shield, 5.4 * P))
    s.line((shield, 5.4 * P), (shield, 7 * P), (P + dx, 7 * P), (P + dx, bottom))
    if can:
        s.rect(P, -P, right - P, 7.5 * P, w=0.35, corner=8)
        s.line((3 * P + dx, 7.5 * P), (3 * P + dx, bottom))
    else:
        s.line((core, 5.3 * P), (core, 7 * P), (3 * P, 7 * P), (3 * P, bottom))
    for name, at, label, align in (('RD', (0, 0), (0.2, -2.2), 'left'), ('BN', (0, 6 * P), (0.2, 6 * P + 0.3), 'left'),
                                   ('YE', (right, 0), (right - 0.2, -2.2), 'right'), ('OR', (right, 6 * P), (right - 0.2, 6 * P + 0.3), 'right'),
                                   ('WH', (P + dx, bottom), (P + dx, bottom + 0.3), 'centre'), ('BK', (3 * P + dx, bottom), (3 * P + dx, bottom + 0.3), 'centre')):
        if can and name in ('RD', 'BN', 'YE', 'OR'):     # beside the connection point, clear of the can
            left = at[0] == 0
            label, align = (at[0] - 0.4 if left else at[0] + 0.4, at[1] - 0.9), 'right' if left else 'left'
        labelled(s, name, at, label, align)
    return s.labels((right + (4.0 if can else 1.0), P), (right + (4.0 if can else 1.0), 2 * P + 0.4))


add = page(USR, 'XFORMER', 'XFORMER', 'TRANSFORMATEURS')
add(frame_core(C('Transformator 1:1 (Kern, 20 × 20)', 'Transformer 1:1 (core, 20 × 20)', 'Transformateur 1:1 (noyau, 20 × 20)'), '12V/2,6A', 8 * P))
add(frame_core(C('Transformator 1:1 (40 × 20)', 'Transformer 1:1 (40 × 20)', 'Transformateur 1:1 (40 × 20)'), '', 16 * P))
add(frame_core(C('Transformator 1:2 (40 × 20)', 'Transformer 1:2 (40 × 20)', 'Transformateur 1:2 (40 × 20)'), '', 16 * P, 2))
add(block_form(C('NF-Übertrager 1:1 (30 × 10)', 'Audio transformer 1:1 (30 × 10)', 'Transformateur audio 1:1 (30 × 10)'), '5093',
               [(0, 12 * P, '1', '2')], [(0, 12 * P, '3', '4')]))
add(block_form(C('Transformator 2:2 (35 × 10)', 'Transformer 2:2 (35 × 10)', 'Transformateur 2:2 (35 × 10)'), '',
               [(0, 6 * P, '1', '2'), (8 * P, 14 * P, '3', '4')], [(0, 6 * P, '5', '6'), (8 * P, 14 * P, '7', '8')]))
add(colour_coded(C('NF-Übertrager 1:1 mit Farbcode (10 × 20)', 'Audio transformer 1:1 with colour code (10 × 20)',
                   'Transformateur audio 1:1 avec code couleur (10 × 20)')))
add(colour_coded(C('NF-Übertrager 1:1 mit Farbcode (20 × 20)', 'Audio transformer 1:1 with colour code (20 × 20)',
                   'Transformateur audio 1:1 avec code couleur (20 × 20)'), can=True))
add(block_form(C('Transformator 1:2 (Wendel, 20 × 10)', 'Transformer 1:2 (helix, 20 × 10)', 'Transformateur 1:2 (hélice, 20 × 10)'), '24 VAC',
               [(0, 8 * P, '1', '2')], [(0, 3 * P, '3', '4'), (5 * P, 8 * P, '5', '6')], form='helix'))
add(block_form(C('Transformator 1:1 (Wendel, 20 × 10)', 'Transformer 1:1 (helix, 20 × 10)', 'Transformateur 1:1 (hélice, 20 × 10)'), '24 V/3A',
               [(0, 8 * P, '1', '2')], [(0, 8 * P, '3', '4')], form='helix'))
add(block_form(C('Transformator 1:2 (Wendel, Mittelanzapfung)', 'Transformer 1:2 (helix, centre tap)', 'Transformateur 1:2 (hélice, point milieu)'),
               '2 × 15 V/1A', [(0, 8 * P, '1', '2')], [(0, 8 * P, '3', '4')], form='helix', tap=(4 * P, '5')))
add(block_form(C('Transformator 2:1 (30 × 10)', 'Transformer 2:1 (30 × 10)', 'Transformateur 2:1 (30 × 10)'), '',
               [(0, 5 * P, '1', '2'), (7 * P, 12 * P, '3', '4')], [(0, 12 * P, '5', '6')]))
add(block_form(C('Transformator 1:1 (10 × 10)', 'Transformer 1:1 (10 × 10)', 'Transformateur 1:1 (10 × 10)'), '30V',
               [(0, 4 * P, '1', '2')], [(0, 4 * P, '3', '4')]))
add(block_form(C('Transformator 1:2 (20 × 10, Variante)', 'Transformer 1:2 (20 × 10, variant)', 'Transformateur 1:2 (20 × 10, variante)'), '',
               [(0, 8 * P, '1', '2')], [(0, 3 * P, '3', '4'), (5 * P, 8 * P, '5', '6')]))


# --- Z Guitar: pickups seen from above (pole pieces as dots, leads to + hot and − ground), the pot seen from the back,
# the lugs of the 5-way blade switch, the Les-Paul toggle and selector switches as multi-position switches
POLE = '#404040'


def plus_minus(s, plus, minus, below=True):
    """The pickup's contacts: + hot and − ground, their names shown beside the connection points."""
    for name, at in (('+', plus), ('-', minus)):
        s.pin(name, at, label=(at[0], at[1] + 0.3) if below else (at[0], at[1] - 2.3), align='centre', shown=True, text=shown(name))
    return s


def stadium(s, cx, cy, length, width, angle=0, w=W):
    """A rounded oblong (a pickup's bobbin from above) about (cx, cy), turned `angle` degrees counter-clockwise."""
    half, rr = length / 2 - width / 2, width / 2
    at = lambda x, y: rot((cx + x, cy + y), (cx, cy), angle)
    s.line(at(-half, -rr), at(half, -rr), w=w).line(at(-half, rr), at(half, rr), w=w)
    c1, c2 = at(-half, 0), at(half, 0)
    s.arc(c1[0], c1[1], width, 90 + angle, 270 + angle, w=w).arc(c2[0], c2[1], width, 270 + angle, 450 + angle, w=w)
    return at


def single_coil(caption, angle=0):
    """A single-coil pickup from above with six pole pieces; both leads leave the lower side to the right."""
    s = Symbol('PU', caption)
    cx, length, width = 5 * P, 27.0, 7.0
    at = stadium(s, cx, 0, length, width, angle, w=0.35)
    for k in range(6):
        p = at((k - 2.5) * 4.1, 0)
        s.circle(p[0], p[1], 2.0, fill=POLE)
    for name, x in (('+', 8 * P), ('-', 9 * P)):
        start = at(x - cx, width / 2)
        end = (round(start[0] / P) * P, math.ceil((start[1] + 1.2) / P) * P)
        s.line(start, end)
        s.pin(name, end, label=(end[0], end[1] + 0.3), align='centre', shown=True, text=shown(name))
    return s.labels((cx + length / 2 + 1.4, -4.4), (cx + length / 2 + 1.4, -1.8))


add = page(USR, 'Z Guitar', 'Z Guitar', 'Z Guitare')
s = Symbol('H', C('Tonabnehmer', 'Pickup', 'Micro (guitare)'), '6k2')
coil(s.line((0, 0), (P, 0)).line((3 * P, 0), (4 * P, 0)), P, 3 * P)
s.rect(P, -3.4, 3 * P, -2.0, fill='#000000')
s.text('rd', 0.2, 0.4, 1.8).text('wh', 4 * P - 0.2, 0.4, 1.8, 'right')
plus_minus(s, (0, 0), (4 * P, 0), below=False)
add(s.labels((2 * P, -7.0), (2 * P, 0.8), 'centre'))

s = Symbol('VOL', C('Potentiometer (Bauform)', 'Potentiometer (body)', 'Potentiomètre (boîtier)'), '500 kB')
s.circle(0, 0, 6 * P, w=0.35).circle(0, 0, 4 * P)
for k, x in enumerate((-2 * P, 0, 2 * P)):
    edge = -math.sqrt((3 * P) ** 2 - x ** 2) + 0.6
    s.rect(x - 0.9, -3.6 * P, x + 0.9, edge, fill='#d0d0d0').circle(x, -3.2 * P, 0.7)
    s.line((x, -3.6 * P), (x, -4 * P)).pin(str(k + 1), (x, -4 * P), label=(x, -4 * P - 2.3), align='centre', shown=True)
add(s.labels((3 * P + 1.0, -1.0), (3 * P + 1.0, 1.6)))

s = Symbol('SW', C('Pickup-Wahlschalter 5 Positionen', 'Pickup selector, 5 positions', 'Sélecteur de micros, 5 positions'))
s.rect(-1.8, -2.0, 10 * P + 1.8, 2.0, w=0.35).rect(4 * P, -0.5, 6 * P, 0.5, fill='#000000')
for row, side in ((0, -1), (1, 1)):
    for k in range(6):
        x, name = 2 * k * P, str(6 * row + k + 1)
        s.rect(x - 0.6, min(side * 2.0, side * 4.0), x + 0.6, max(side * 2.0, side * 4.0), fill='#d0d0d0')
        s.line((x, side * 4.0), (x, side * 2 * P))
        s.pin(name, (x, side * 2 * P), label=(x, side * 2 * P - 2.3) if side < 0 else (x, side * 2 * P + 0.3), align='centre', shown=True)
    s.text('C', 0.9, side * 1.0 - 0.7, 1.4)
add(s.labels((10 * P + 2.6, -4.4), (10 * P + 2.6, -1.8)))

add(single_coil(C('Single-Coil-Tonabnehmer (6 Pole)', 'Single-coil pickup (6 poles)', 'Micro simple bobinage (6 plots)')))
add(single_coil(C('Single-Coil-Tonabnehmer (schräg)', 'Single-coil pickup (slanted)', 'Micro simple bobinage (incliné)'), angle=-10))

s = Symbol('PU', C('Humbucker (6 Pole, zwei Spulen)', 'Humbucker (6 poles, two coils)', 'Humbucker (6 plots, deux bobines)'))
s.rect(-13.0, -7.0, 13.0, 7.0, w=0.35, corner=12)
for y in (-3.3, 3.3):
    stadium(s, 0, y, 24.0, 5.4)
    for k in range(6):
        s.circle((k - 2.5) * 4.1, y, 1.8, fill=POLE)
for name, x in (('+', 3 * P), ('-', 4 * P)):
    s.line((x, 7.0), (x, 4 * P))
plus_minus(s, (3 * P, 4 * P), (4 * P, 4 * P))
add(s.labels((14.4, -7.0), (14.4, -4.4)))


def toggle_switch(caption):
    """The three-way toggle: two leaves from the common 2 to the contacts 1 and 3, both closed in the middle position;
    the toggle sign of DIN EN 60617-2 above, linked to both leaves."""
    s = Symbol('', caption, numbered=False)
    one, three, common = (P, 0), (3 * P, 0), (2 * P, 2 * P)
    node(s, *one)
    node(s, *three)
    node(s, *common)
    s.line((0, 0), (P - NODE / 2, 0)).line((3 * P + NODE / 2, 0), (4 * P, 0)).line((2 * P, 2 * P + NODE / 2), (2 * P, 3 * P))
    blade(s, common, rim(one, common))
    blade(s, common, rim(three, common))
    dashed(s, (2 * P, 2 * P - NODE / 2), (2 * P, -2.6))
    s.line((2 * P - 1.2, -3.4), (2 * P, -2.6), (2 * P + 1.2, -3.4))
    s.pin('1', (0, 0), label=(0.2, -2.2), shown=True).pin('3', (4 * P, 0), label=(4 * P - 0.2, -2.2), align='right', shown=True)
    s.pin('2', (2 * P, 3 * P), label=(2 * P + 0.5, 3 * P - 2.2), shown=True)
    return s.labels((4 * P + 1.0, 0.6), (4 * P + 1.0, 3.2))


def selector(caption, n):
    """A multi-position switch (DIN EN 60617-7): the contacts 1..n side by side, leads upwards; the moving contact on
    the bar of the common C rests on 1."""
    s = Symbol('SW', caption)
    for k in range(1, n + 1):
        s.line((k * P, -P - 0.6), (k * P, -3 * P)).pin(str(k), (k * P, -3 * P), label=(k * P, -3 * P - 2.3), align='centre', shown=True)
    s.line((0, 0), (n * P + 0.8, 0)).line((P, 0), (P, -P - 1.4), w=THICK)
    s.pin('C', (0, 0), label=(0.2, -2.2), shown=True)
    return s.labels(((n + 1) * P + 0.4, -3 * P), ((n + 1) * P + 0.4, -3 * P + 2.6))


add(toggle_switch(C('Dreiwegeschalter (Toggle)', 'Three-way toggle switch', 'Sélecteur trois positions (levier)')))
add(selector(C('Schalter 1 × 5', 'Switch 1 × 5', 'Commutateur 1 × 5'), 5))
add(selector(C('Schalter 1 × 3', 'Switch 1 × 3', 'Commutateur 1 × 3'), 3))
s = turned(toggle_switch(C('Dreiwegeschalter (Toggle, Variante)', 'Three-way toggle switch (variant)', 'Sélecteur trois positions (levier, variante)')))
place(s, '1', (0.6, 0.2))
place(s, '3', (0.6, 4 * P - 2.4))
place(s, '2', (-3 * P + 0.2, 2 * P - 2.2))
add(s.labels((4.6, P - 0.4), (4.6, 2 * P + 0.4)))
