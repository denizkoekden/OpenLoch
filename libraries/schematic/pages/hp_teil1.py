# Fluid power, part 1: more operating devices, pressure valves, electrical parts, measuring devices, motors and pumps
# for the pages of the folder 'Hydraulik + Pneumatik', after ISO 1219-1 (the measuring devices in the forms of
# ISO 1219-1:1991) and, for coils and contacts, DIN EN 60617-7. Drawn like the symbols already on these pages: valve
# squares of side Q, machines in a circle of 4 * P, ports as connection points at the ends of their leads.

# --- Operating devices: the sign left of the short bar that stands for the valve's edge
add = extend(HP, 'Betätigungsarten')


def push_pull(s):
    """Push-pull button: a knob rounded on both sides, divided by a line."""
    s.line((0, 0), (-2.4, 0)).arc(-3.3, 0, 1.8, 0, 360, dy=2.8).line((-3.3, -1.4), (-3.3, 1.4))


def pull(s):
    """Pull button: the flat face outwards, the rounded side towards the valve."""
    s.line((0, 0), (-2.4, 0)).arc(-3.4, 0, 2.0, 270, 450).line((-3.4, -1.0), (-3.4, 1.0))


def treadle(s):
    """Pedal working in both directions: a rocker on the stem with a foot at either end."""
    s.line((0, 0), (-2.0, 0)).line((-4.0, -1.6), (-2.6, -2.0), (-1.4, 2.0), (-2.8, 2.4))


def motor(s):
    s.line((0, -0.4), (-1.6, -0.4)).line((0, 0.4), (-1.6, 0.4)).circle(-3.2, 0, 3.2).text('M', -3.2, -1.2, 2.2, 'centre')


def roller_lever(s):
    """A roller on a lever that folds away when run over the other way (idle return): joint, link, roller."""
    u = math.sqrt(0.5)
    s.line((0, 0), (-1.5, 0)).circle(-2.0, 0, 1.0)
    s.line((-2.0 - 0.5 * u, 0.5 * u), (-3.8 + 0.8 * u, 1.8 - 0.8 * u)).circle(-3.8, 1.8, 1.6)


def pilot_stage(s):
    """Indirect (pilot-operated) control: the pilot valve as a rectangle, its triangle at the main valve."""
    s.rect(-3.6, -1.3, 0, 1.3)
    triangle(s, (0, 0), (-1.4, 0), 0.9, filled=False)


add(actuation(C('Druck-Zugknopf', 'Push-pull button', 'Bouton pousser-tirer'), push_pull))
add(actuation(C('Zugknopf', 'Pull button', 'Bouton à tirer'), pull))
add(actuation(C('Pedal, zweiseitig', 'Pedal, two directions', 'Pédale, deux sens'), treadle))
add(actuation(C('Elektromotor', 'Electric motor', 'Moteur électrique'), motor))
add(actuation(C('Wirkverbindung (Steuerleitung)', 'Control line (pilot line)', 'Ligne de commande (pilotage)'),
              lambda s: dashed(s, (0, 0), (-6.0, 0))))
add(actuation(C('Druckbetätigung hydraulisch', 'Pilot pressure, hydraulic', 'Pilotage par pression, hydraulique'),
              lambda s: (dashed(s, (0, 0), (-3.2, 0)), triangle(s, (-1.0, 0), (-2.6, 0), 0.9))[0]))
add(actuation(C('Rollenhebel mit Leerrücklauf', 'Roller lever, one direction (idle return)', 'Levier à galet escamotable'), roller_lever))
add(actuation(C('Vorsteuerung (indirekt)', 'Pilot operation, indirect', 'Pilotage indirect'), pilot_stage))

# --- Pressure valves: one square, inlet below, outlet above, the pilot at the left against the spring at the right
add = extend(HP, 'Druckventile')


def square_valve(s, below, above, wide=()):
    """The square with the ports below (at P and 3 P) and the one above, the adjustable spring at the right; the
    names of the ports in `wide` stand further off their lead (a sign on it)."""
    s.rect(0, 0, Q, Q, w=0.35)
    for name, x in zip(below, (P, 3 * P)):
        s.line((x, Q), (x, Q + P)).pin(name, (x, Q + P), label=(x + (1.2 if name in wide else 0.5), Q + P - 2.2), shown=True)
    s.line((P, 0), (P, -P)).pin(above, (P, -P), label=(P + 0.5, -P - 0.4), shown=True)
    spring(s, Q, Q / 2)
    s.arrow((Q + 0.6, Q / 2 + 2.0), (Q + 4.6, Q / 2 - 2.0), size=1.0)
    return s.labels((Q + 6.4, -0.4), (Q + 6.4, 2.6))


def sequence_valve(caption):
    """Normally closed, opened by the pressure at the inlet; the spring chamber drained to the tank (external drain)."""
    s = Symbol('V', caption)
    square_valve(s, ('P',), 'A')
    flow(s, (P + 1.2, Q - 0.8), (P + 1.2, 0.8))
    dashed(s, (P, Q + P / 2), (-1.6, Q + P / 2), (-1.6, Q / 2), (0, Q / 2))
    x = Q + 3.0
    dashed(s, (Q, Q - 1.0), (x, Q - 1.0), (x, Q + 2.2))
    return s.line((x - 1.2, Q + 1.8), (x - 1.2, Q + 3.0), (x + 1.2, Q + 3.0), (x + 1.2, Q + 1.8))      # the tank


def regulator(caption, names, exhaust=False):
    """Three-way pressure regulator: inlet to outlet open, the relief port closed; the outlet pressure is fed back
    against the spring and, when too high, opens the outlet to the relief port."""
    s = Symbol('V', caption)
    inlet, relief, outlet = names
    square_valve(s, (inlet, relief), outlet, wide=(relief,) if exhaust else ())
    flow(s, (P, Q), (P, 0))
    blocked(s, (3 * P, Q), True)
    dashed(s, (P, -P / 2), (-1.6, -P / 2), (-1.6, Q / 2), (0, Q / 2))
    if exhaust:
        triangle(s, (3 * P, Q + P - 0.4), (3 * P, Q + 0.5), 0.8, filled=False)     # exhaust to atmosphere
    return s


add(sequence_valve(C('Druckzuschaltventil', 'Sequence valve', 'Valve de séquence')))
add(regulator(C('Druckregelventil (3-Wege)', 'Pressure regulator, three-way', 'Régulateur de pression à trois voies'), ('1', '3', '2')))
add(regulator(C('Druckregelventil mit Entlastung', 'Pressure regulator with relief', 'Régulateur de pression à échappement'),
              ('P', 'R', 'A'), exhaust=True))

# --- Electrical: coils and contacts drawn upright like those of the pages under Elektroinstallation
add = extend(HP, 'Elektrisch')


def coil_box(s):
    s.rect(-1.6, P, 1.6, 3 * P, w=0.35).line((0, 0), (0, P)).line((0, 3 * P), (0, 4 * P)).line((-1.6, 3 * P), (1.6, P))
    return s.pin('A1', (0, 0), label=(0.6, 0.2), shown=True).pin('A2', (0, 4 * P), label=(0.6, 4 * P - 2.4), shown=True)


s = coil_box(Symbol('V', C('Magnetventil', 'Solenoid valve', 'Électrovanne')))
x = 3 * P
dashed(s, (1.6, 2 * P), (x, 2 * P))
s.poly((x - 1.4, 2 * P - 2.6), (x + 1.4, 2 * P - 2.6), (x, 2 * P), fill=None).poly((x - 1.4, 2 * P + 2.6), (x + 1.4, 2 * P + 2.6), (x, 2 * P), fill=None)
add(s.labels((x + 2.2, P - 0.4), (x + 2.2, 2 * P + 0.6)))
s = contact('nc', C('Druckschalter (Öffner)', 'Pressure switch (break contact)', 'Pressostat (contact à ouverture)'), 'S', operated='relay', numbers=True)
add(s.rect(-8.0, 2 * P - 2.1, -5.0, 2 * P + 0.9).text('p', -6.5, 2 * P - 1.9, 2.2, 'centre'))     # operated by pressure
s = coil_box(Symbol('K', C('Relaisspule', 'Relay coil', 'Bobine de relais')))
add(s.labels((2.4, P), (2.4, 2 * P + 0.4)))
add(contact('no', C('Schließer', 'Make contact', 'Contact à fermeture'), 'K', operated='relay', numbers=True))
add(contact('nc', C('Öffner', 'Break contact', 'Contact à ouverture'), 'K', operated='relay', numbers=True))

# --- Measuring devices: a circle of 4 * P - 0.4 like the pressure gauge, the port below it
add = extend(HP, 'Messgeräte')
D = 4 * P - 0.4
R = D / 2


def instrument(caption, draw, legs=(0,)):
    """The circle about (0, -2 P) with its port(s) on the pitch below (two ports one pitch lower, so that their leads
    show); draw() adds the sign inside."""
    s = Symbol('M', caption)
    s.circle(0, -2 * P, D, w=0.35)
    y = 0 if len(legs) == 1 else P
    for k, x in enumerate(legs):
        s.line((x, y), (x, -2 * P + math.sqrt(R * R - x * x))).pin(str(k + 1), (x, y), label=(x + 0.5, y + 0.2), shown=True)
    draw(s, -2 * P)
    return s.labels((2 * P + 1.0, -4 * P), (2 * P + 1.0, -4 * P + 3.0))


def chord(s, cy, dy):
    dx = math.sqrt(R * R - dy * dy)
    s.line((-dx, cy + dy), (dx, cy + dy))


def hanging_arc(s, cy, depth=2.2, r=3.4):
    """An arc hanging from the top of the circle, its ends on the circle: the sign of flow (ISO 1219-1:1991)."""
    gap = R - depth + r                          # from the circle's centre up to the arc's centre
    u = (gap * gap + R * R - r * r) / (2 * gap)  # from the circle's centre up to the arc's ends
    a = math.degrees(math.atan2(gap - u, math.sqrt(R * R - u * u)))
    s.arc(0, cy - gap, 2 * r, 180 + a, 360 - a)


def shaft_instrument(caption, draw):
    """An instrument in a shaft (double line), the shaft's ends at the left and right on the pitch."""
    s = Symbol('M', caption)
    s.circle(0, 0, D, w=0.35)
    for side in (-1, 1):
        s.line((side * R, -0.4), (side * 3 * P, -0.4)).line((side * R, 0.4), (side * 3 * P, 0.4))
    s.pin('1', (-3 * P, 0), label=(-3 * P + 0.4, 0.8), shown=True)
    s.pin('2', (3 * P, 0), label=(3 * P - 0.4, 0.8), align='right', shown=True)
    draw(s)
    return s.labels((R + 0.6, -R - 2.0), (R + 0.6, -R + 1.0))


def rotating(s):
    r = 2.8
    s.arc(0, 0, 2 * r, 120, 400)
    end = lambda a: (r * math.cos(math.radians(a)), -r * math.sin(math.radians(a)))
    s.arrow(end(388), end(400), size=1.0)


def torsion(s):
    s.line((-2.2, 1.8), (-0.5, 1.8), (0.5, -1.8), (2.2, -1.8), start='arrow', end='arrow', size=1.0)


def cross(s, cy):
    d = R / math.sqrt(2)
    s.line((-d, cy - d), (d, cy + d)).line((-d, cy + d), (d, cy - d))


add(instrument(C('Differenzdruckmanometer', 'Differential pressure gauge', 'Manomètre différentiel'),
               lambda s, cy: s.arrow((-1.6, cy + 1.6), (1.8, cy - 1.8), size=1.0), legs=(-P, P)))
add(instrument(C('Druckanzeiger', 'Pressure indicator', 'Indicateur de pression'), lambda s, cy: s.line((0, cy + R), (0, cy - R))))
add(instrument(C('Füllstandsanzeiger', 'Level indicator', 'Indicateur de niveau'), lambda s, cy: (chord(s, cy, -0.8), chord(s, cy, 0.8))))
add(instrument(C('Durchflussanzeiger', 'Flow indicator', 'Indicateur de débit'), lambda s, cy: hanging_arc(s, cy)))
add(shaft_instrument(C('Drehzahlmessgerät', 'Tachometer', 'Tachymètre'), rotating))
add(shaft_instrument(C('Drehmomentmessgerät', 'Torque meter', 'Couplemètre'), torsion))
add(instrument(C('Optischer Anzeiger', 'Visual indicator', 'Indicateur visuel'), cross))

# --- Motors and pumps: machine() of the pages, with the drive shaft as a double line where the caption says so and
# the direction of rotation as a curved double arrow across the shaft
d2 = 2 * P          # radius of a machine's circle


def with_shaft(s, length=2 * P):
    """Replaces the short shaft of machine() by a double line to the right."""
    s.parts = [o for o in s.parts if not (o['type'] == 'line' and min(q[0] for q in o['points']) >= d2 - 1e-6)]
    s.line((d2, -0.5), (d2 + length, -0.5)).line((d2, 0.5), (d2 + length, 0.5))
    return s.labels((d2 + length + 1.0, -d2), (d2 + length + 1.0, -d2 + 3.0))


def rotation(s, x, r=3.2, half=44):
    """A curved arrow with heads at both ends across the shaft at x: both directions of rotation."""
    cx = x + r
    end = lambda a: (cx + r * math.cos(math.radians(a)), -r * math.sin(math.radians(a)))
    s.arc(cx, 0, 2 * r, 180 - half, 180 + half)
    s.arrow(end(180 - half + 12), end(180 - half), size=1.0).arrow(end(180 + half - 12), end(180 + half), size=1.0)
    return s


def unit(caption, prefix, hollow, outward, variable=False, two_way=False, shaft=False, arrow=False):
    s = machine(caption, prefix, hollow=hollow, outward=outward, variable=variable, two_way=two_way)
    if shaft or arrow:
        with_shaft(s)
    if arrow:
        rotation(s, d2 + 1.4 * P)
    return s


add = extend(HP, 'Motoren')
for hollow, de, en, fr in ((True, 'Druckluftmotor', 'Pneumatic motor', 'Moteur pneumatique'),
                           (False, 'Hydromotor', 'Hydraulic motor', 'Moteur hydraulique')):
    variants = [(f'{de} mit Welle', f'{en} with shaft', f'{fr} avec arbre', False, False, True, False)]
    if hollow:
        variants.append((f'{de}, verstellbar', f'Variable {en.lower()}', f'{fr} variable', True, False, False, False))
    variants += [
        (f'{de}, zwei Drehrichtungen', f'{en}, two directions of rotation', f'{fr}, deux sens de rotation', False, True, False, False),
        (f'{de}, zwei Drehrichtungen, mit Drehrichtungspfeil', f'{en}, two directions of rotation, with rotation arrow',
         f'{fr}, deux sens de rotation, avec flèche de rotation', False, True, True, True),
        (f'{de}, verstellbar, zwei Drehrichtungen', f'Variable {en.lower()}, two directions of rotation',
         f'{fr} variable, deux sens de rotation', True, True, False, False),
        (f'{de}, verstellbar, zwei Drehrichtungen, mit Drehrichtungspfeil',
         f'Variable {en.lower()}, two directions of rotation, with rotation arrow',
         f'{fr} variable, deux sens de rotation, avec flèche de rotation', True, True, True, True)]
    for vde, ven, vfr, variable, two_way, shaft, arrow in variants:
        add(unit(C(vde, ven, vfr), 'M', hollow, False, variable, two_way, shaft, arrow))
s = Symbol('M', C('Elektromotor (Fluidtechnik)', 'Electric motor (fluid power)', 'Moteur électrique (fluidique)'))
s.circle(0, 0, 4 * P, w=0.35).text('M', 0, -2.0, 3.6, 'centre')
add(with_shaft(s))

add = extend(HP, 'Pumpen + Kompressoren')
for vde, ven, vfr, variable, two_way, shaft, arrow in (
        ('Kompressor mit Welle', 'Compressor with shaft', 'Compresseur avec arbre', False, False, True, False),
        ('Kompressor, verstellbar', 'Variable compressor', 'Compresseur variable', True, False, False, False),
        ('Kompressor, zwei Förderrichtungen', 'Compressor, two directions', 'Compresseur, deux sens', False, True, False, False),
        ('Kompressor, zwei Förderrichtungen, mit Drehrichtungspfeil', 'Compressor, two directions, with rotation arrow',
         'Compresseur, deux sens, avec flèche de rotation', False, True, True, True),
        ('Kompressor, verstellbar, zwei Förderrichtungen', 'Variable compressor, two directions', 'Compresseur variable, deux sens',
         True, True, False, False),
        ('Kompressor, verstellbar, zwei Förderrichtungen, mit Drehrichtungspfeil', 'Variable compressor, two directions, with rotation arrow',
         'Compresseur variable, deux sens, avec flèche de rotation', True, True, True, True)):
    add(unit(C(vde, ven, vfr), 'P', True, True, variable, two_way, shaft, arrow))
add(unit(C('Hydropumpe mit Welle', 'Hydraulic pump with shaft', 'Pompe hydraulique avec arbre'), 'P', False, True, shaft=True))
s = with_shaft(machine(C('Handpumpe', 'Hand pump', 'Pompe à main'), 'P', hollow=False), length=P)
s.line((d2 + P, 0), (d2 + P + 3.0, -2.4)).circle(d2 + P + 3.0, -2.4, 0.8, fill='#000000')
add(s.labels((d2 + P + 3.0, -d2 - 3.0), (d2 + P + 3.0, -d2 - 0.2)))
for vde, ven, vfr, variable, arrow in (
        ('Hydropumpe, zwei Förderrichtungen, mit Drehrichtungspfeil', 'Hydraulic pump, two directions, with rotation arrow',
         'Pompe hydraulique, deux sens, avec flèche de rotation', False, True),
        ('Hydropumpe, verstellbar, zwei Förderrichtungen', 'Variable hydraulic pump, two directions', 'Pompe hydraulique variable, deux sens',
         True, False),
        ('Hydropumpe, verstellbar, zwei Förderrichtungen, mit Drehrichtungspfeil', 'Variable hydraulic pump, two directions, with rotation arrow',
         'Pompe hydraulique variable, deux sens, avec flèche de rotation', True, True)):
    add(unit(C(vde, ven, vfr), 'P', False, True, variable, True, arrow, arrow))
s = Symbol('P', C('Schwenkantrieb', 'Semi-rotary actuator', 'Actionneur oscillant'))
s.arc(0, 0, 4 * P, 0, 180, w=0.35).line((-2 * P, 0), (2 * P, 0), w=0.35)
s.line((-0.5, -2 * P), (-0.5, -2 * P - 2.0)).line((0.5, -2 * P), (0.5, -2 * P - 2.0))     # the shaft
end = lambda a: (3.0 * math.cos(math.radians(a)), -3.0 * math.sin(math.radians(a)))
s.arc(0, 0, 6.0, 35, 145).arrow(end(47), end(35), size=1.0).arrow(end(133), end(145), size=1.0)
s.line((-P, 0), (-P, P)).pin('1', (-P, P), label=(-P + 0.5, P - 2.2), shown=True).line((P, 0), (P, P)).pin('2', (P, P), label=(P + 0.5, P - 2.2), shown=True)
add(s.labels((2 * P + 1.0, -2 * P), (2 * P + 1.0, -2 * P + 3.0)))
s = with_shaft(machine(C('Motor-Pumpen-Einheit', 'Motor-pump unit', 'Groupe motopompe'), 'P', hollow=False), length=P)
s.circle(2 * d2 + P, 0, 4 * P, w=0.35).text('M', 2 * d2 + P, -2.0, 3.6, 'centre')
add(s.labels((3 * d2 + P + 1.0, -d2), (3 * d2 + P + 1.0, -d2 + 3.0)))
