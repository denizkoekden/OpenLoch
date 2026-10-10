# More signs for installation plans on page "Elektroinstallation" (luminaires, outlets, switches, devices, appliances,
# clocks), after DIN EN 60617-11 and DIN 40717: no connection points, the designator hidden but counted in the parts list.
add = extend(EI, 'Elektroinstallation')
SOCKET = P + 2.6    # centre of the socket's half circle below its lead, as for "Steckdose"


def sign(prefix, caption, draw, right=3.4, top=-2.0):
    s = Symbol(prefix, caption, numbered=bool(prefix), shown=False)
    draw(s)
    return s.labels((right + 0.8, top), (right + 0.8, top + 3.0))


def cross(s, x=0, y=0, r=2.0):
    d = r / math.sqrt(2)
    return s.line((x - d, y - d), (x + d, y + d)).line((x - d, y + d), (x + d, y - d))


def lamp(s):
    return cross(s.circle(0, 0, 4.0))


def lever(s, angle, ticks=1, side=1, r0=0.8, r1=4.6, centre=(0, 0), arrow=False):
    """A switch lever leaving its circle at `angle`, with `ticks` strokes at its end (on the clockwise side for side 1)."""
    a = lambda p: rot((centre[0] + p[0], centre[1] + p[1]), centre, angle)
    s.line(a((r0, 0)), a((r1, 0)), end='arrow' if arrow else 'none', size=1.0 if arrow else None)
    for k in range(ticks):
        s.line(a((r1 - 0.9 * k, 0)), a((r1 - 0.9 * k, side * 1.2)))
    return s


def switch(s, ticks=1, **kw):
    return lever(s.circle(0, 0, 1.6), 45, ticks, **kw)


def socket(s, count=1):
    s.line((0, 0), (0, P)).arc(0, SOCKET, 5.2, 0, 180).line((-2.6, SOCKET), (2.6, SOCKET))
    if count > 1:
        s.line((-1.0, P / 2 + 0.6), (1.0, P / 2 - 0.6)).text(str(count), 1.2, -1.2, 2.0)
    return s


def bracket(s, inner=False):
    """An outlet for telecommunication: an open rectangle below the lead."""
    s.line((0, 0), (0, P)).line((-2.6, SOCKET), (-2.6, P), (2.6, P), (2.6, SOCKET))
    if inner:
        s.line((-1.8, SOCKET), (-1.8, P + 0.8), (1.8, P + 0.8), (1.8, SOCKET))
    return s


def hands(s, x, y, r):
    return s.line((x, y), (x, y - 0.7 * r)).line((x, y), (x + 0.55 * r, y))


def star(s, x, y, r=0.9):
    for angle in (90, 30, 150):
        s.line(rot((x - r, y), (x, y), angle), rot((x + r, y), (x, y), angle))
    return s


def fan(s, x, y, d):
    r = d / 2
    s.circle(x, y, d)
    return s.line((x - 0.5 * r, y + 0.866 * r), (x, y - r), (x + 0.5 * r, y + 0.866 * r))


def band(s):
    """The ribbed band at the top of a hand dryer or an air conditioner."""
    s.line((-3.0, -1.8), (3.0, -1.8))
    for x in (-1.5, 0, 1.5):
        s.line((x, -3.0), (x, -1.8))
    return s


def appliance(prefix, caption, draw):
    return sign(prefix, caption, lambda s: draw(s.rect(-3.0, -3.0, 3.0, 3.0)), right=3.0, top=-3.0)


def water_lead(s):
    return s.line((2.0, 0), (3.6, 0)).line((3.6, -0.6), (3.6, 0.6))


def kitchen(s):
    s.line((-2.0, -0.8), (2.0, -0.8)).arc(0, -0.8, 4.0, 180, 360, dy=3.0)
    return s.poly((-0.5, 0.7), (0.5, 0.7), (1.0, 1.8), (-1.0, 1.8), fill=None)


def microwave(s):
    for y in (-1.5, 0, 1.5):
        s.arc(-1.0, y, 2.0, 0, 180, dy=1.2).arc(1.0, y, 2.0, 180, 360, dy=1.2)
    return s


def grill(s):
    s.poly((-1.4, -2.2), (1.4, -2.2), (2.4, -0.6), (-2.4, -0.6), fill=None)
    return s.line((-3.0, 0.8), (-1.6, 0.8)).rect(-1.6, 0.3, 1.6, 1.3).line((1.6, 0.8), (3.0, 0.8))


def dishwasher(s):
    s.circle(0, 0, 3.0).circle(0, 0, 1.2, fill='#000000')
    for sx in (-1, 1):
        for sy in (-1, 1):
            s.line((sx * 3.0, sy * 3.0), (sx * 1.06, sy * 1.06))
    return s


def emergency_own(s):
    """Two black triangles tip to tip between two bars."""
    s.line((-2.0, -1.6), (-2.0, 1.6), w=0.35).line((2.0, -1.6), (2.0, 1.6), w=0.35)
    return s.poly((-2.0, -1.6), (2.0, -1.6), (0, 0)).poly((-2.0, 1.6), (2.0, 1.6), (0, 0))


def door_opener(s):
    s.rect(-3.0, -1.2, 1.0, 1.2)
    coil(s, -2.4, 0.4, turns=3, y=0.5)
    return s.poly((1.0, -1.2), (1.0, 1.2), (3.4, 1.2), fill=None)


# Luminaires
add(sign('P', C('Wandleuchte', 'Luminaire outlet on wall', 'Point d’éclairage mural'), lambda s: cross(s).line((0, 0), (-2.6, 0))))
add(sign('P', C('Wandleuchte (an der Wand)', 'Wall-mounted luminaire', 'Applique murale'),
         lambda s: cross(s).line((-2.0, -2.0), (-2.0, 2.0), w=THICK)))
add(sign('P', C('Deckenleuchte (Einbau)', 'Recessed ceiling luminaire', 'Luminaire encastré au plafond'),
         lambda s: cross(s.rect(-2.0, -2.0, 2.0, 2.0))))
add(sign('P', C('Lampe', 'Lamp', 'Lampe'), lamp))
add(sign('P', C('Scheinwerfer', 'Projector', 'Projecteur'), lambda s: lamp(s).arc(0, 0, 6.0, 110, 250)))
add(sign('P', C('Punktleuchte', 'Spotlight', 'Projecteur à faisceau serré'),
         lambda s: lamp(s).arrow((2.3, -0.9), (5.4, -0.9), size=1.0).arrow((2.3, 0.9), (5.4, 0.9), size=1.0), right=5.4))
add(sign('P', C('Flutlicht', 'Floodlight', 'Projecteur à faisceau large'),
         lambda s: lamp(s).arc(0, 0, 6.0, 110, 250).arrow((2.3, -0.7), (4.9, -2.2), size=1.0).arrow((2.3, 0.7), (4.9, 2.2), size=1.0), right=4.9))
add(sign('P', C('Sicherheitsleuchte', 'Emergency luminaire', 'Luminaire de sécurité'), lambda s: cross(s).circle(0, 0, 1.0, fill='#000000')))
add(sign('P', C('Sicherheitsleuchte (Einbau)', 'Emergency luminaire, recessed', 'Luminaire de sécurité encastré'),
         lambda s: cross(s.rect(-2.0, -2.0, 2.0, 2.0)).circle(0, 0, 1.0, fill='#000000')))
add(sign('P', C('Sicherheitsleuchte mit eigener Stromversorgung', 'Self-contained emergency luminaire',
                'Bloc autonome d’éclairage de sécurité'), emergency_own))
add(sign('P', C('Leuchtstofflampe', 'Fluorescent lamp', 'Lampe fluorescente'),
         lambda s: s.line((-5.0, 0), (5.0, 0), w=THICK).line((-5.0, -1.0), (-5.0, 1.0)).line((5.0, -1.0), (5.0, 1.0)), right=5.0))

# Outlets and boxes
for n, de, en, fr in ((2, 'zweifach', 'double', 'double'), (3, 'dreifach', 'triple', 'triple'), (4, 'vierfach', 'quadruple', 'quadruple')):
    add(sign('X', C(f'Steckdose ({de})', f'Socket outlet, {en}', f'Prise de courant {fr}'), lambda s, n=n: socket(s, n), top=0))
add(sign('X', C('Steckdose, schaltbar', 'Switched socket outlet', 'Prise de courant commandée'),
         lambda s: lever(socket(s), 45, r0=2.6, r1=5.8, centre=(0, SOCKET)), right=5.0, top=0))
add(sign('X', C('Steckdose mit Trenntransformator', 'Socket outlet with isolating transformer', 'Prise de courant avec transformateur de séparation'),
         lambda s: socket(s).circle(-0.8, SOCKET - 0.9, 1.6).circle(0.8, SOCKET - 0.9, 1.6), top=0))
add(sign('X', C('Fernmeldedose', 'Telecommunication outlet', 'Prise de télécommunication'), bracket, top=0))
add(sign('X', C('Antennendose', 'Aerial outlet', 'Prise d’antenne'), lambda s: bracket(s, inner=True), top=0))
add(sign('X', C('Datendose', 'Data outlet', 'Prise informatique'),
         lambda s: bracket(s).rect(-1.6, P + 0.9, 1.6, P + 2.3).line((-1.6, P + 0.9), (1.6, P + 2.3)).line((-1.6, P + 2.3), (1.6, P + 0.9)), top=0))
add(sign('', C('Abzweigdose', 'Junction box', 'Boîte de dérivation'), lambda s: s.circle(0, 0, 3.0), right=1.5))
add(sign('', C('Anschlussdose', 'Connection box', 'Boîte de raccordement'), lambda s: s.circle(0, 0, 3.0).circle(0, 0, 1.0, fill='#000000'), right=1.5))

# Switches
add(sign('S', C('Schalter', 'Switch, general', 'Interrupteur, général'), lambda s: switch(s, 0)))
add(sign('S', C('Leuchtschalter', 'Switch with pilot lamp', 'Interrupteur à voyant'),
         lambda s: lever(cross(s.circle(0, 0, 2.4), r=1.2), 45, r0=1.2)))
add(sign('S', C('Ausschalter, zweipolig', 'One-way switch, two-pole', 'Interrupteur bipolaire'), lambda s: switch(s, 2)))
add(sign('S', C('Ausschalter, dreipolig', 'One-way switch, three-pole', 'Interrupteur tripolaire'), lambda s: switch(s, 3)))
add(sign('S', C('Zeitschalter', 'Time switch', 'Interrupteur temporisé'), lambda s: switch(s, 0, arrow=True)))
add(sign('S', C('Serienschalter', 'Series switch', 'Interrupteur double allumage'),
         lambda s: lever(lever(s.circle(0, 0, 1.6), 60), 120, side=-1)))
add(sign('S', C('Kreuzschalter', 'Intermediate switch', 'Permutateur'),
         lambda s: lever(lever(lever(lever(s.circle(0, 0, 1.6), 45), 225), 135, side=-1), 315, side=-1)))
add(sign('S', C('Dimmer', 'Dimmer', 'Variateur'),
         lambda s: switch(s).poly(*[rot(p, (0, 0), 45) for p in ((1.6, 0), (4.0, 0), (4.0, -1.0))])))
add(sign('S', C('Schlüsselschalter', 'Key-operated switch', 'Interrupteur à clé'),
         lambda s: s.rect(-1.8, -1.8, 1.8, 1.8).circle(0, -0.5, 1.0, fill='#000000').poly((0, -0.4), (0.5, 1.0), (-0.5, 1.0)), right=1.8))
add(sign('S', C('Leuchttaster', 'Push button with pilot lamp', 'Bouton-poussoir lumineux'),
         lambda s: cross(s.circle(0, 0, 3.0).circle(0, 0, 1.8), r=0.9), right=1.5))

# Devices
add(sign('VG', C('Vorschaltgerät', 'Ballast', 'Ballast'), lambda s: s.rect(-3.0, -1.5, 3.0, 1.5).rect(-2.0, -0.4, 2.0, 0.4, fill='#000000'), right=3.0))
add(sign('Tür', C('Türöffner', 'Electric door opener', 'Gâche électrique'), door_opener))
add(sign('ZR', C('Zeitrelais', 'Timer relay (staircase timer)', 'Minuterie'),
         lambda s: hands(s.rect(-3.0, -2.0, 3.0, 2.0).circle(0, 0, 2.8), 0, 0, 1.4), right=3.0))

# Appliances
add(appliance('V', C('Elektrogerät', 'Electrical appliance', 'Appareil électrique'), lambda s: s.text('E', 0, -1.5, 3.0, 'centre')))
add(appliance('V', C('Küchenmaschine', 'Food processor', 'Robot de cuisine'), kitchen))
add(appliance('V', C('Elektroherd', 'Electric cooker', 'Cuisinière électrique'),
              lambda s: [s.circle(x, y, 1.2, fill='#000000') for x, y in ((0, -1.3), (-1.3, 1.1), (1.3, 1.1))]))
add(appliance('V', C('Mikrowellengerät', 'Microwave oven', 'Four à micro-ondes'), microwave))
add(appliance('V', C('Backofen', 'Oven', 'Four'), lambda s: s.line((-3.0, -1.6), (3.0, -1.6)).circle(0, 0.8, 1.4, fill='#000000')))
add(appliance('V', C('Wärmeplatte', 'Warming plate', 'Chauffe-plat'), lambda s: s.rect(-1.7, -1.7, 1.7, 1.7).circle(0, 0, 1.2, fill='#000000')))
add(appliance('V', C('Infrarotgrill', 'Infrared grill', 'Gril infrarouge'), grill))
add(appliance('V', C('Waschmaschine', 'Washing machine', 'Lave-linge'), lambda s: s.circle(0, 0, 4.0).circle(0, 0, 1.4, fill='#000000')))
add(appliance('V', C('Geschirrspüler', 'Dishwasher', 'Lave-vaisselle'), dishwasher))
add(appliance('V', C('Wäschetrockner', 'Tumble dryer', 'Sèche-linge'), lambda s: fan(s, 0, -1.0, 2.6).circle(0, 1.7, 1.2, fill='#000000')))
add(appliance('V', C('Händetrockner', 'Hand dryer', 'Sèche-mains'), lambda s: fan(band(s), 0, 0.6, 3.0)))
add(appliance('V', C('Klimagerät', 'Air conditioner', 'Climatiseur'), lambda s: star(band(s), 0, 0.6, 1.2)))
add(appliance('V', C('Kühlgerät', 'Refrigerator', 'Réfrigérateur'), lambda s: star(s, 0, 0, 1.0)))
add(appliance('V', C('Tiefkühlgerät', 'Freezer compartment', 'Conservateur'), lambda s: star(star(s, -1.25, 0), 1.25, 0)))
add(appliance('V', C('Gefriergerät', 'Freezer', 'Congélateur'), lambda s: star(star(star(s, -1.9, 0, 0.75), 0, 0, 0.75), 1.9, 0, 0.75)))
add(sign('V', C('Heißwasserspeicher', 'Hot water storage heater', 'Chauffe-eau à accumulation'),
         lambda s: water_lead(s.circle(0, 0, 4.0).circle(0, 0, 1.2, fill='#000000')), right=3.6))
add(sign('V', C('Durchlauferhitzer', 'Instantaneous water heater', 'Chauffe-eau instantané'),
         lambda s: water_lead(s.circle(0, 0, 4.0).circle(0, -0.7, 1.0, fill='#000000')).arrow((-1.3, 0.8), (1.3, 0.8), size=1.0), right=3.6))

# Clocks
add(sign('Uhr', C('Uhr', 'Clock', 'Horloge'), lambda s: hands(s.circle(0, 0, 4.0), 0, 0, 2.0), right=2.0))
add(sign('Uhr', C('Hauptuhr', 'Master clock', 'Horloge mère'), lambda s: hands(s.circle(0, 0, 5.0).circle(0, 0, 4.0), 0, 0, 2.0), right=2.5))
add(sign('V', C('Kartenleser', 'Card reader (time recorder)', 'Lecteur de cartes (pointeuse)'),
         lambda s: hands(s.rect(-2.2, -3.2, 2.2, 3.2).line((-1.2, -2.2), (1.2, -2.2), w=THICK).circle(0, 0.9, 2.8), 0, 0.9, 1.4), right=2.2, top=-3.2))
