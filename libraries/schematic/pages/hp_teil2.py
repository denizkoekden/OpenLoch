# Fluid power, part 2: quick couplings, accumulators and reservoirs, non-return and shut-off valves, flow valves, the
# parts to build directional valves from, cylinders and conditioning units for the pages of the folder
# 'Hydraulik + Pneumatik', after ISO 1219-1 (the couplings in the form of ISO 1219-1:1991). The shut-off valves with a
# hand wheel, solenoid or diaphragm actuator and the three-way valves are the process symbols of ISO 10628-2.
# Drawn like the symbols already on these pages: valve squares of side Q, ports at the ends of their leads.


def ports(s, names):
    """Renames contacts a helper of the pages made (old name: new name)."""
    for o in s.parts:
        if o['type'] == 'contact' and o['name'] in names:
            o['name'] = o['text'] = names[o['name']]
    return s


def zigzag(s, x, y, dx, dy, w=W, scale=1.0):
    """A spring as spring() draws it, from (x, y) along the unit direction (dx, dy)."""
    pts = [(x, y)]
    for k in range(1, 6):
        a, b = k * 0.9 * scale, (-1.2 if k % 2 else 1.2) * scale
        pts.append((x + dx * a - dy * b, y + dy * a + dx * b))
    pts.append((x + dx * 5.4 * scale, y + dy * 5.4 * scale))
    return s.line(*pts, w=w)


def seat(s, apex, side, ball=False, y=0):
    """A valve seat on a line parallel to the x axis: a chevron opening to `side` (+1 right, -1 left), the ball in it."""
    s.line((apex + side * 1.6, y - 1.6), (apex, y), (apex + side * 1.6, y + 1.6))
    if ball:
        s.circle(apex + side * 1.56, y, 2.2)
    return s


def gauge(s, x, y, d=3.2):
    """A pressure gauge on a stem going up from the junction (x, y)."""
    cy = y - P - d / 2
    s.line((x, y), (x, y - P)).circle(x, cy, d).circle(x, y, 0.8, fill='#000000')
    return s.arrow((x + 0.28 * d, cy + 0.28 * d), (x - 0.28 * d, cy - 0.28 * d), size=0.8)


def outline(s, x0, y0, x1, y1):
    """The dash-dot outline around the parts of a unit."""
    s.rect(x0, y0, x1, y1)
    s.parts[-1]['pen']['style'] = 'dashDot'
    return s


# --- Quick couplings, coupled: one box, each half a seat pointing to the middle, a ball where the half closes itself
add = extend(HP, 'Schnellkupplungen')


def coupling(caption, balls):
    s = Symbol('K', caption)
    c = 2 * P
    s.rect(P / 2, -1.9, 3.5 * P, 1.9, w=0.35).line((c, -1.0), (c, 1.0))
    ends = []
    for side, ball in ((-1, balls >= 1), (1, balls >= 2)):
        apex = c + side * 1.5
        s.line((apex + side * 1.3, -1.3), (apex, 0), (apex + side * 1.3, 1.3))
        if ball:
            s.circle(apex + side * 0.92, 0, 1.3)
        ends.append(apex + side * (1.57 if ball else 0))
    s.line((0, 0), (ends[0], 0)).line((c - 1.5, 0), (c + 1.5, 0)).line((ends[1], 0), (4 * P, 0))
    s.pin('1', (0, 0)).pin('2', (4 * P, 0), label=(4 * P - 0.4, -2.2), align='right')
    return s.labels((2 * P, -5.0), (2 * P, 2.4), 'centre')


add(coupling(C('Schnellkupplung mit einem Rückschlagventil, verbunden', 'Quick coupling with one non-return valve, connected',
               'Coupleur rapide à un clapet, accouplé'), 1))
add(coupling(C('Schnellkupplung mit zwei Rückschlagventilen, verbunden', 'Quick coupling with two non-return valves, connected',
               'Coupleur rapide à deux clapets, accouplé'), 2))

# --- Accumulators: the capsule of the page, the gas side above with an open triangle; reservoirs as on the page
add = extend(HP, 'Speicher')
SEP = -2.5 * P      # height of the separating element


def accumulator(caption, inner):
    s = Symbol('P', caption)
    s.rect(-P, -4 * P, P, -P, w=0.35, corner=50)
    inner(s)
    s.line((0, -P), (0, 0)).pin('1', (0, 0), label=(0.5, 0.2), shown=True)
    return s.labels((P + 1.0, -4 * P), (P + 1.0, -4 * P + 3.0))


def gas(s, tip):
    return triangle(s, (0, tip), (0, tip - 1.4), 0.9, filled=False)


def diaphragm(s):
    s.arc(0, SEP - 2.65, 6.98, 223.3, 316.7)
    return gas(s, SEP - 0.6)


def bladder(s):
    s.line((-1.6, -4 * P + 0.58), (-1.6, SEP - 0.6)).line((1.6, -4 * P + 0.58), (1.6, SEP - 0.6)).arc(0, SEP - 0.6, 3.2, 180, 360)
    return gas(s, SEP - 0.4)


add(accumulator(C('Hydrospeicher, gasbelastet', 'Hydraulic accumulator, gas-loaded', 'Accumulateur hydraulique à gaz'),
                lambda s: gas(s.line((-P, SEP), (P, SEP)), SEP - 0.4)))
add(accumulator(C('Kolbenspeicher', 'Piston accumulator', 'Accumulateur à piston'),
                lambda s: gas(s.rect(-P, SEP - 0.4, P, SEP + 0.4, w=0.35), SEP - 0.8)))
add(accumulator(C('Membranspeicher', 'Diaphragm accumulator', 'Accumulateur à membrane'), diaphragm))
add(accumulator(C('Blasenspeicher', 'Bladder accumulator', 'Accumulateur à vessie'), bladder))


def reservoir(caption, end=None, closed=False):
    s = Symbol('P', caption)
    s.line((-2 * P, -2 * P), (-2 * P, 0), (2 * P, 0), (2 * P, -2 * P))
    if closed:
        s.line((-2 * P, -2 * P), (2 * P, -2 * P))
    if end is not None:
        s.line((0, -3 * P), (0, end)).pin('1', (0, -3 * P), label=(0.5, -3 * P - 0.4), shown=True)
    return s.labels((2 * P + 1.0, -2 * P), (2 * P + 1.0, -2 * P + 3.0))


add(reservoir(C('Behälter, offen', 'Reservoir, open', 'Réservoir ouvert')))
add(reservoir(C('Behälter, geschlossen', 'Reservoir, closed', 'Réservoir fermé'), closed=True))
add(reservoir(C('Behälter mit Rücklaufleitung über dem Flüssigkeitsspiegel', 'Reservoir, return line above fluid level',
                'Réservoir, retour au-dessus du niveau'), end=-1.5 * P))
add(reservoir(C('Behälter mit Rücklaufleitung unter dem Flüssigkeitsspiegel', 'Reservoir, return line below fluid level',
                'Réservoir, retour sous le niveau'), end=-1.0))

# --- Non-return and shut-off valves
add = extend(HP, 'Sperrventile')

# Pilot to close: check valve of the page in a box, the pilot acting on the ball's side (B)
s = check_valve(C('Gesteuertes Rückschlagventil (Schließen gesteuert)', 'Pilot-operated non-return valve, pilot to close',
                  'Clapet piloté à la fermeture'))
s.rect(-P, P, P, 3 * P, w=0.35)
dashed(s, (1.4, 3 * P), (1.4, 3.5 * P), (2 * P, 3.5 * P), (2 * P, 4 * P))
for o in s.parts:
    if o['type'] == 'contact' and o['name'] == 'B':      # its name left of the lead, clear of the pilot line
        o['pos'], o['align'] = pt((-0.5, 4 * P - 2.2)), 'right'
add(s.pin('X', (2 * P, 4 * P), label=(2 * P + 0.5, 4 * P - 2.2), shown=True))


def body3(caption, names):
    """A horizontal valve body: inlets left and right on the axis, the outlet above the middle."""
    s = Symbol('V', caption)
    s.rect(P, -P, 5 * P, P, w=0.35).line((3 * P, -P), (3 * P, -2 * P))
    s.pin(names[0], (0, 0), label=(0.4, -2.2), shown=True).pin(names[2], (6 * P, 0), label=(6 * P - 0.4, -2.2), align='right', shown=True)
    s.pin(names[1], (3 * P, -2 * P), label=(3 * P - 0.5, -2 * P - 0.4), align='right', shown=True)
    return s.labels((5 * P, -P - 3.4), (5 * P, P + 0.6))


s = body3(C('Wechselventil', 'Shuttle valve', 'Sélecteur de circuit'), ('1', '2', '3'))
seat(s, P + 1.0, 1, ball=True)
seat(s, 5 * P - 1.0, -1)
s.line((0, 0), (P + 1.0, 0)).line((P + 3.66, 0), (5 * P - 1.0, 0)).line((5 * P - 1.0, 0), (6 * P, 0)).circle(3 * P, 0, 0.8, fill='#000000')
add(s)
s = body3(C('Schnellentlüftungsventil', 'Quick exhaust valve', 'Soupape d’échappement rapide'), ('1', '2', '3'))
seat(s, P + 1.0, 1, ball=True)
seat(s, 5 * P - 1.0, -1)
s.line((0, 0), (P + 1.0, 0)).line((P + 3.66, 0), (5 * P - 1.0, 0)).line((5 * P - 1.0, 0), (6 * P, 0))
dashed(s, (3 * P, -1.5 * P), (4.4 * P, -1.5 * P), (4.4 * P, -P))
add(s)
s = body3(C('Zweidruckventil', 'Two-pressure valve', 'Valve à deux pressions'), ('1', '2', '3'))
s.line((0, 0), (P, 0)).line((5 * P, 0), (6 * P, 0))
for x, inner in ((P + 1.7, P + 2.6), (5 * P - 1.7, 5 * P - 2.6)):
    s.line((x, -2.0), (x, 2.0), w=THICK).line((inner, -P), (inner, -P + 1.3)).line((inner, P), (inner, P - 1.3))
add(s.line((P + 1.7, 0), (5 * P - 1.7, 0)))

s = Symbol('V', C('Absperrventil (Kasten)', 'Shut-off valve (box)', 'Vanne d’arrêt (carré)'))
s.rect(P, -P, 3 * P, P, w=0.35).line((0, 0), (P + 1.2, 0)).line((P + 1.2, -1.0), (P + 1.2, 1.0))
s.line((4 * P, 0), (3 * P - 1.2, 0)).line((3 * P - 1.2, -1.0), (3 * P - 1.2, 1.0))
add(s.pin('1', (0, 0), shown=True).pin('2', (4 * P, 0), label=(4 * P - 0.4, -2.2), align='right', shown=True)
    .labels((2 * P, -5.6), (2 * P, P + 0.6), 'centre'))


def shut_off(caption, three=False, actuator=None, coil=False):
    """Process valve: two triangles tip to tip (three meeting at a ball for the three-way valve), the actuator on a stem."""
    s = Symbol('V', caption)
    c = 2 * P
    s.line((0, 0), (P, 0)).line((3 * P, 0), (4 * P, 0))
    tip = 0.7 if three else 0
    s.poly((P, -1.6), (P, 1.6), (c - tip, 0), fill=None).poly((3 * P, -1.6), (3 * P, 1.6), (c + tip, 0), fill=None)
    s.pin('1', (0, 0), shown=True).pin('2', (4 * P, 0), label=(4 * P - 0.4, -2.2), align='right', shown=True)
    if three:
        s.circle(c, 0, 2 * tip).poly((c - 1.6, P), (c + 1.6, P), (c, tip), fill=None).line((c, P), (c, 2 * P))
        s.pin('3', (c, 2 * P), label=(c + 0.5, 2 * P - 2.2), shown=True)
    top = -tip
    if actuator == 'hand':
        s.line((c, top), (c, -2.6)).line((c - 1.6, -2.6), (c + 1.6, -2.6), w=0.35)
    elif actuator == 'diaphragm':
        s.line((c, top), (c, -P)).line((c - 2.0, -P), (c + 2.0, -P)).arc(c, -P, 4.0, 0, 180)
    elif actuator == 'solenoid':
        s.line((c, top), (c, -2 * P + 1.4)).rect(c - 1.4, -2 * P - 1.4, c + 1.4, -2 * P + 1.4).line((c - 1.4, -2 * P + 1.4), (c + 1.4, -2 * P - 1.4))
        s.line((P, -2 * P), (c - 1.4, -2 * P)).line((c + 1.4, -2 * P), (3 * P, -2 * P))
        s.pin('A1', (P, -2 * P), label=(P - 0.4, -2 * P - 2.2), align='right', shown=True)
        s.pin('A2', (3 * P, -2 * P), label=(3 * P + 0.4, -2 * P - 2.2), shown=True)
    if actuator:
        return s.labels((4 * P + 0.6, -2 * P - 1.0), (4 * P + 0.6, -2 * P + 2.0))
    return s.labels((3 * P + 0.6, -5.0), (3 * P + 0.6, -2.0))


add(shut_off(C('Magnetventil (Absperrventil)', 'Solenoid shut-off valve', 'Électrovanne d’arrêt'), actuator='solenoid'))
add(shut_off(C('Absperrventil', 'Shut-off valve', 'Vanne d’arrêt')))
add(shut_off(C('Absperrventil, handbetätigt', 'Shut-off valve, manually operated', 'Vanne d’arrêt manuelle'), actuator='hand'))
add(shut_off(C('Dreiwegeventil (Absperrorgan)', 'Three-way valve', 'Vanne trois voies'), three=True))
add(shut_off(C('Absperrventil mit Membranantrieb', 'Shut-off valve with diaphragm actuator', 'Vanne d’arrêt à membrane'),
             actuator='diaphragm'))
add(shut_off(C('Dreiwegeventil mit Membranantrieb', 'Three-way valve with diaphragm actuator', 'Vanne trois voies à membrane'),
             three=True, actuator='diaphragm'))

# --- Flow valves, upright like the throttles of the page
add = extend(HP, 'Stromventile')
s = ports(throttle(C('Drosselventil, einstellbar, handbetätigt', 'Adjustable throttle, manually operated', 'Étrangleur réglable, manuel')),
          {'A': '1', 'B': '2'})
add(s.line((-0.9, 2 * P), (-3.4, 2 * P)).line((-3.4, 2 * P - 1.2), (-3.4, 2 * P + 1.2)))


def orifice(caption, box=False):
    """Sharp-edged constriction: two chevrons pointing at the line."""
    s = Symbol('V', caption)
    s.line((0, 0), (0, 4 * P))
    s.line((-2.0, 2 * P - 1.5), (-0.5, 2 * P), (-2.0, 2 * P + 1.5)).line((2.0, 2 * P - 1.5), (0.5, 2 * P), (2.0, 2 * P + 1.5))
    if box:
        s.rect(-1.2 * P, P, 1.2 * P, 3 * P, w=0.35).arrow((-2.0, 2 * P + 2.1), (2.0, 2 * P - 2.1), size=1.0)
    s.pin('1', (0, 0), label=(0.5, -0.4), shown=True).pin('2', (0, 4 * P), label=(0.5, 4 * P - 2.2), shown=True)
    return s.labels((3 * P, 0), (3 * P, 3.0))


add(orifice(C('Blende', 'Orifice', 'Diaphragme')))
add(orifice(C('Stromregelventil', 'Flow control valve', 'Régulateur de débit'), box=True))

# --- Directional valves to build: valve bodies (designator Q), single switching positions of one square and the loose
# elements (no designator, not in the parts list); ports of a square at P and 3 P from its left edge (2 P in the middle)
add = extend(HP, 'Wegeventile')


def squares(caption, n, draw=None):
    s = Symbol('Q', caption)
    for k in range(n):
        s.rect(k * Q, 0, (k + 1) * Q, Q, w=0.35)
    if draw:
        draw(s, 0)
    return s.labels((n * Q + 6.4, -0.4), (n * Q + 6.4, 2.6))


def with_ports(s, x):
    for name, dx, top in (('2', P, True), ('4', 3 * P, True), ('1', P, False), ('3', 3 * P, False)):
        y0, y1 = (0, -P) if top else (Q, Q + P)
        s.line((Q + dx, y0), (Q + dx, y1)).pin(name, (Q + dx, y1), label=(Q + dx + 0.5, y1 - (0.4 if top else 2.2)), shown=True)
    return s


def infinite(s, x):
    return s.line((0, -1.0), (2 * Q, -1.0)).line((0, Q + 1.0), (2 * Q, Q + 1.0))


def two_closed(s, x):
    blocked(s, (2 * P, Q), True)
    return blocked(s, (2 * P, 0), False)


def oblique(s, x, mirrored=False):
    a, b = (3 * P, P) if mirrored else (P, 3 * P)
    blocked(s, (a, Q), True)
    flow(s, (2 * P, Q), (a, 0))
    return flow(s, (b, 0), (b, Q))


def connected(s, x):
    s.line((P, 0), (P, Q)).line((3 * P, 0), (3 * P, Q)).line((P, 2 * P), (3 * P, 2 * P))
    return s.circle(P, 2 * P, 1.0, fill='#000000').circle(3 * P, 2 * P, 1.0, fill='#000000')


def bypass(s, x):
    s.line((P, 0), (P, 1.6 * P), (3 * P, 1.6 * P))
    flow(s, (3 * P, 1.6 * P), (3 * P, 0))
    blocked(s, (P, Q), True)
    return blocked(s, (3 * P, Q), True)


add(squares(C('Ventilkasten, 2 Schaltstellungen', 'Valve body, 2 positions', 'Corps de distributeur, 2 positions'), 2))
add(squares(C('Ventilkasten, 3 Schaltstellungen', 'Valve body, 3 positions', 'Corps de distributeur, 3 positions'), 3))
add(squares(C('Ventilkasten, 3 Schaltstellungen mit Anschlüssen', 'Valve body, 3 positions with ports',
              'Corps de distributeur, 3 positions avec orifices'), 3, with_ports))
add(squares(C('Ventilkasten, stufenlos verstellbar', 'Valve body, infinite positioning', 'Corps de distributeur, positionnement continu'),
            2, infinite))
for de, en, fr, draw in (('Durchfluss nach oben', 'flow upwards', 'passage vers le haut', lambda s, x: flow(s, (2 * P, Q), (2 * P, 0))),
                         ('Durchfluss nach unten', 'flow downwards', 'passage vers le bas', lambda s, x: flow(s, (2 * P, 0), (2 * P, Q))),
                         ('zwei Anschlüsse gesperrt', 'two ports closed', 'deux orifices fermés', two_closed),
                         ('vier Anschlüsse gesperrt', 'four ports closed', 'quatre orifices fermés', all_closed),
                         ('zwei parallele Durchflüsse', 'two parallel flows', 'deux passages parallèles', parallel),
                         ('gekreuzte Durchflüsse', 'crossed flows', 'passages croisés', cross),
                         ('Durchfluss schräg, Anschluss gesperrt', 'oblique flow, one port closed', 'passage oblique, un orifice fermé', oblique),
                         ('Durchfluss schräg (gespiegelt)', 'oblique flow (mirrored)', 'passage oblique (inversé)',
                          lambda s, x: oblique(s, x, mirrored=True)),
                         ('alle Anschlüsse verbunden', 'all ports connected', 'tous orifices reliés', connected),
                         ('Umlauf, zwei gesperrt', 'bypass, two ports closed', 'dérivation, deux orifices fermés', bypass)):
    add(squares(C('Schaltstellung: ' + de, 'Switching position: ' + en, 'Position : ' + fr), 1, draw))


def element(caption, draw):
    s = Symbol('', caption, numbered=False, listed=False, shown=False)
    draw(s)
    return s


add(element(C('Sperre (oben)', 'Closed port (top bar)', 'Orifice fermé (barre en haut)'), lambda s: blocked(s, (0, 0), True)))
add(element(C('Sperre (unten)', 'Closed port (bottom bar)', 'Orifice fermé (barre en bas)'), lambda s: blocked(s, (0, 0), False)))
# The arrows start or end at the insertion point on the upper edge of a square and span its height.
for de, en, fr, a, b in (('nach oben', 'up', 'vers le haut', (0, Q), (0, 0)),
                         ('nach unten', 'down', 'vers le bas', (0, 0), (0, Q)),
                         ('schräg nach rechts unten', 'oblique, down right', 'oblique vers le bas à droite', (0, 0), (2 * P, Q)),
                         ('schräg nach rechts oben', 'oblique, up right', 'oblique vers le haut à droite', (-2 * P, Q), (0, 0)),
                         ('schräg nach links oben', 'oblique, up left', 'oblique vers le haut à gauche', (2 * P, Q), (0, 0)),
                         ('schräg nach links unten', 'oblique, down left', 'oblique vers le bas à gauche', (0, 0), (-2 * P, Q))):
    add(element(C('Durchflusspfeil ' + de, 'Flow arrow ' + en, 'Flèche de passage ' + fr), lambda s, a=a, b=b: flow(s, a, b)))
for de, en, fr, dx, dy, w in (('waagerecht', 'horizontal', 'horizontal', 1, 0, W), ('waagerecht, fett', 'horizontal, heavy', 'horizontal, gras', 1, 0, THICK),
                              ('senkrecht', 'vertical', 'vertical', 0, 1, W), ('senkrecht, fett', 'vertical, heavy', 'vertical, gras', 0, 1, THICK)):
    add(element(C('Feder (' + de + ')', 'Spring, ' + en, 'Ressort ' + fr), lambda s, dx=dx, dy=dy, w=w: zigzag(s, 0, 0, dx, dy, w)))
add(element(C('Druckquelle (Dreieck)', 'Pressure source (triangle)', 'Source de pression (triangle)'),
            lambda s: triangle(s, (0, 0), (0, 2.0), 1.2, filled=False)))

# --- Cylinders: cylinder() of the page where it fits
add = extend(HP, 'Zylinder')
H = 2 * P


def cyl(caption, **flags):
    s = cylinder(caption, **flags)
    s.prefix = 'Z'
    return s


s = cyl(C('Doppeltwirkender Zylinder mit Magnetkolben', 'Double-acting cylinder with magnetic piston', 'Vérin double effet à piston magnétique'))
s.parts = [o for o in s.parts if o.get('points') != [pt((2 * P, -H)), pt((2 * P, 0))]]
add(s.rect(2 * P - 0.7, -H, 2 * P + 0.7, 0, w=0.35).rect(2 * P - 0.35, -H + 1.0, 2 * P + 0.35, -1.0, fill='#000000'))
s = cyl(C('Doppeltwirkender Zylinder mit Endlagendämpfung (einseitig)', 'Double-acting cylinder with cushioning on one side',
          'Vérin double effet amorti d’un côté'))
add(s.rect(2 * P - 1.4, -H + 1.0, 2 * P, -1.0))
s = cyl(C('Doppeltwirkender Zylinder mit einstellbarer Endlagendämpfung', 'Double-acting cylinder with adjustable cushioning',
          'Vérin double effet à amortissement réglable'))
add(s.rect(2 * P - 1.4, -H + 1.0, 2 * P, -1.0).rect(2 * P, -H + 1.0, 2 * P + 1.4, -1.0).arrow((4.6, 1.8), (8.0, -7.2), size=1.0))

L = 6 * P            # housing of the telescopic and plunger cylinders, open towards the rod


def housing(s):
    return s.line((L, -H + 0.8), (L, -H), (0, -H), (0, 0), (L, 0), (L, -0.8), w=0.35)


def telescopic(caption, double):
    s = Symbol('Z', caption)
    housing(s)
    s.line((L + 2 * P, -H + 1.6), (L + 2 * P, -H + 0.8), (2 * P, -H + 0.8), (2 * P, -0.8), (L + 2 * P, -0.8), (L + 2 * P, -1.6), w=0.35)
    s.rect(3.5 * P, -H + 1.6, L + 4 * P, -1.6, w=0.35)
    if double:      # the annular pistons of the stages
        s.rect(2 * P, -H, 2 * P + 1.2, -H + 0.8).rect(2 * P, -0.8, 2 * P + 1.2, 0)
        s.rect(3.5 * P, -H + 0.8, 3.5 * P + 1.2, -H + 1.6).rect(3.5 * P, -1.6, 3.5 * P + 1.2, -0.8)
        s.line((L - P, 0), (L - P, P)).pin('2', (L - P, P), label=(L - P + 0.5, P - 2.2), shown=True)
    s.line((P, 0), (P, P)).pin('1', (P, P), label=(P + 0.5, P - 2.2), shown=True)
    return s.labels((0, -H - 3.4), (3 * P, -H - 3.4))


add(telescopic(C('Teleskopzylinder, einfachwirkend', 'Telescopic cylinder, single-acting', 'Vérin télescopique simple effet'), False))
add(telescopic(C('Teleskopzylinder, doppeltwirkend', 'Telescopic cylinder, double-acting', 'Vérin télescopique double effet'), True))
s = Symbol('Z', C('Membranzylinder', 'Diaphragm cylinder', 'Vérin à membrane'))
s.rect(0, -H, 4 * P, 0, w=0.35, corner=50).arc(2 * P - 2.0, -H / 2, 6.46, -51.8, 51.8).line((-2 * P, -H / 2), (6 * P, -H / 2), w=THICK)
s.line((P, -0.3), (P, P)).pin('1', (P, P), label=(P + 0.5, P - 2.2), shown=True)
add(s.line((3 * P, -0.3), (3 * P, P)).pin('2', (3 * P, P), label=(3 * P + 0.5, P - 2.2), shown=True).labels((0, -H - 3.4), (3 * P, -H - 3.4)))
s = Symbol('Z', C('Tauchkolbenzylinder', 'Plunger cylinder', 'Vérin plongeur'))
housing(s).rect(2 * P, -H + 0.8, L + 3 * P, -0.8, w=0.35)
add(s.line((P, 0), (P, P)).pin('1', (P, P), label=(P + 0.5, P - 2.2), shown=True).labels((0, -H - 3.4), (3 * P, -H - 3.4)))

# --- Conditioning: conditioner() of the page (diamond of half-diagonal 2 P, ports at -3 P and 3 P)
add = extend(HP, 'Übertragung + Aufbereitung')


def unit(caption, *marks, prefix='F'):
    def extra(s):
        for mark in marks:
            mark(s)
    s = conditioner(caption, extra)
    s.prefix = prefix
    return s


def element_line(s, bottom=2 * P - 0.6):        # the filter element
    return dashed(s, (0, -2 * P + 0.6), (0, bottom))


def separator(s):                             # water collected below the line, drained at the bottom corner
    return s.line((-1.6, 2 * P - 1.6), (1.6, 2 * P - 1.6)).line((0, 2 * P), (0, 3 * P))


def automatic(s):
    return s.line((-0.7, 2 * P - 1.33), (0, 2 * P - 0.63), (0.7, 2 * P - 1.33))


def fluid_line(s):
    return s.line((-2 * P, 0), (2 * P, 0))


def indicator(s):
    s.line((0, -2 * P), (0, -2 * P - 1.0)).circle(0, -2 * P - 1.9, 1.8)
    return s.line((-0.64, -2 * P - 2.54), (0.64, -2 * P - 1.26)).line((-0.64, -2 * P - 1.26), (0.64, -2 * P - 2.54))


s = unit(C('Filter mit Verschmutzungsanzeige', 'Filter with clogging indicator', 'Filtre avec indicateur de colmatage'), element_line, indicator)
add(s.labels((1.6, -2 * P - 3.6), (0, 2 * P + 0.6), 'left', 'centre'))
add(unit(C('Filter mit Wasserabscheider', 'Filter with water separator', 'Filtre avec séparateur d’eau'),
         lambda s: element_line(s, 2 * P - 1.6), separator))
add(unit(C('Wasserabscheider, automatisch', 'Water separator, automatic drain', 'Séparateur d’eau à purge automatique'), separator, automatic))
add(unit(C('Filter mit automatischem Wasserabscheider', 'Filter with automatic water separator', 'Filtre avec séparateur d’eau automatique'),
         lambda s: element_line(s, 2 * P - 1.6), separator, automatic))
add(unit(C('Lufttrockner (Variante)', 'Air dryer (variant)', 'Sécheur d’air (variante)'), lambda s: s.arc(0, 1.4, 4.4, 200, 340)))
add(unit(C('Aufbereitungseinheit (allgemein)', 'Conditioning unit, general', 'Appareil de conditionnement, général')))
add(unit(C('Lufttrockner', 'Air dryer', 'Sécheur d’air'), lambda s: s.line((-1.6, -2 * P + 1.6), (1.6, -2 * P + 1.6)).line((-1.6, 2 * P - 1.6), (1.6, 2 * P - 1.6))))
add(unit(C('Kühler (Variante)', 'Cooler (variant)', 'Refroidisseur (variante)'), fluid_line,
         lambda s: s.arrow((0, 0), (0, -2 * P), size=1.2).arrow((0, 0), (0, 2 * P), size=1.2)))
s = unit(C('Kühler mit Kühlmittelleitungen', 'Cooler with coolant lines', 'Refroidisseur avec circuit de refroidissement'), fluid_line,
         lambda s: s.arrow((0, 0), (0, -2 * P), size=1.2).arrow((0, 0), (0, 2 * P), size=1.2))
s.arrow((-P, P), (-P, 2.4 * P), size=1.0).line((-P, 2.4 * P), (-P, 3 * P)).arrow((P, 3 * P), (P, 1.9 * P), size=1.0).line((P, 1.9 * P), (P, P))
add(s.pin('3', (-P, 3 * P), label=(-P - 0.4, 3 * P - 2.2), align='right').pin('4', (P, 3 * P)).labels((0, -2 * P - 3.4), (2 * P, 2 * P + 0.6), 'centre'))
add(unit(C('Erhitzer', 'Heater', 'Réchauffeur'), fluid_line,
         lambda s: s.arrow((0, -2 * P), (0, -0.2), size=1.2).arrow((0, 2 * P), (0, 0.2), size=1.2)))
add(unit(C('Temperaturregler', 'Temperature controller', 'Régulateur de température'), fluid_line,
         lambda s: s.arrow((0, -2 * P), (0, -0.2), size=1.2).arrow((0, 0), (0, 2 * P), size=1.2)))

s = Symbol('P', C('Druckluftbehälter (liegend)', 'Air receiver, horizontal', 'Réservoir d’air horizontal'))
s.rect(-2 * P, -P, 2 * P, P, w=0.35, corner=50).line((-3 * P, 0), (-2 * P, 0)).line((2 * P, 0), (3 * P, 0))
add(s.pin('1', (-3 * P, 0)).pin('2', (3 * P, 0), label=(3 * P - 0.4, -2.2), align='right').labels((0, -P - 3.4), (0, P + 0.6), 'centre'))


def regulator(s, x0, x1, relief=None):
    """Pressure regulator in its simplified form: the box with the flow both ways (relieving), the adjustable spring
    on top, the outlet pressure fed back below the box."""
    s.rect(x0, -P, x1, P, w=0.35).line((x0 + 0.5, 0), (x1 - 0.5, 0), start='arrow', end='arrow', size=1.0)
    xs = (x0 + x1) / 2
    zigzag(s, xs, -P, 0, -1, scale=0.6).arrow((xs - 1.6, -P - 0.4), (xs + 1.8, -P - 4.0), size=0.9)
    xf, yf = x1 + 0.5 * P, P + 1.2
    dashed(s, (xf, 0), (xf, yf))
    dashed(s, (xf, yf), (x1 - 0.8 * P, yf))
    dashed(s, (x1 - 0.8 * P, yf), (x1 - 0.8 * P, P))
    return s


s = Symbol('P', C('Druckregler mit Manometer', 'Pressure regulator with gauge', 'Régulateur de pression avec manomètre'))
regulator(s, P, 4 * P).line((0, 0), (P, 0)).line((4 * P, 0), (7 * P, 0))
gauge(s, 5.5 * P, 0)
s.arrow((2 * P, P), (2 * P, 1.75 * P), size=1.0).line((2 * P, 1.75 * P), (2 * P, 2 * P))
s.pin('1', (0, 0)).pin('2', (7 * P, 0), label=(7 * P - 0.4, -2.2), align='right').pin('3', (2 * P, 2 * P), label=(2 * P + 0.4, 2 * P - 2.2))
add(s.labels((6.5 * P, -2 * P - 2.6), (6.5 * P, -2 * P + 0.4)))

# Service unit in detail: filter with water separator, regulator with gauge, lubricator, in a dash-dot outline
s = Symbol('', C('Wartungseinheit (ausführlich)', 'Air service unit, detailed', 'Unité de conditionnement, détaillée'), numbered=False)
d = 1.2 * P
for cx in (2 * P, 8.6 * P):
    s.poly((cx, -d), (cx + d, 0), (cx, d), (cx - d, 0), fill=None, w=0.35)
dashed(s, (2 * P, -d + 0.5), (2 * P, d - 1.2))
s.line((2 * P - 1.2, d - 1.2), (2 * P + 1.2, d - 1.2)).line((2 * P, d), (2 * P, d + 2.6))
s.line((8.6 * P, -d), (8.6 * P, -d + 1.2))
regulator(s, 4 * P, 6 * P)
triangle(s, (4 * P - 1.2, -0.55 * P), (4 * P, -0.55 * P), 0.6, filled=False)
gauge(s, 7 * P, 0)
s.line((0, 0), (2 * P - d, 0)).line((2 * P + d, 0), (4 * P, 0)).line((6 * P, 0), (8.6 * P - d, 0)).line((8.6 * P + d, 0), (11 * P, 0))
outline(s, 0.5 * P, -7.4, 10.5 * P, 1.7 * P)
add(s.pin('1', (0, 0)).pin('2', (11 * P, 0), label=(11 * P - 0.4, -2.2), align='right').labels((0.5 * P, -10.8), (4 * P, -10.8)))
