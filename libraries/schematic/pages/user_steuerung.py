# The USER pages "Elektro(Schaltschrank)", "FET", "FLIP FLOP", "GATEs", "GRAPHICS" and "IND". Control cabinet parts
# after DIN EN 60617-2/-3/-4/-6/-7/-8 (contact numbers after EN 50005 as ABB "Aufbau von Typen- und
# Klemmenbezeichnungen", 9AKK107991A6151 Rev. A, 01/2021, gives them: x1-x2 break, x3-x4 make, x5-x6 and x7-x8 break and
# make with a special function, here the delayed contacts of a time relay; 95-96 the break contact of an overload
# relay), FETs after DIN EN 60617-5 (IEC 617-5 05-05-11 to 05-05-17), flip-flops after IEC 60617-12 with the pin
# numbers of the first section from the data sheets (TI SCLS094F 74HC74, National Semiconductor MM74HC76 TL/F/5074
# January 1988, SCHS140E 74HC109, SCHS134G 74HC73, SCHS166F 74HC221), gates in the distinctive shapes of ANSI/IEEE
# Std 91, inductors after DIN EN 60617-4. The drawing aids of "GRAPHICS" carry no designator and stay out of the parts
# list.
USR = 'USER'
TIP = (-1.9, P + 0.6)      # the free end of a make contact's blade, pivot at (0, 3 * P), as contact() draws it
NC_TIP = (2.1, P - 0.4)    # the same for a break contact


def outline(s, x0, y0, x1, y1):
    """The dash-dot outline of a device made of several elements."""
    s.rect(x0, y0, x1, y1)
    s.parts[-1]['pen']['style'] = 'dashDot'
    return s


def dashes(s, a, b, n=3, w=THICK):
    """A dashed line drawn as n separate strokes (so that a heavy dashed line stays visibly dashed)."""
    step = 1 / (n + (n - 1) * 0.5)
    for k in range(n):
        t0, t1 = k * 1.5 * step, (k * 1.5 + 1) * step
        s.line((a[0] + t0 * (b[0] - a[0]), a[1] + t0 * (b[1] - a[1])), (a[0] + t1 * (b[0] - a[0]), a[1] + t1 * (b[1] - a[1])), w=w)
    return s


def blade(x, y, tip=TIP):
    """The point at height y on the blade of the contact at x (pivot (x, 3 * P), free end x + tip)."""
    return (x + tip[0] * (3 * P - y) / (3 * P - tip[1]), y)


def unnamed(s):
    """A contact of another device: no designator of its own, not counted in the parts list."""
    s.prefix, s.numbered, s.listed = '', False, False
    return s


# --- Elektro(Schaltschrank)
add = page(USR, 'Elektro(Schaltschrank)', 'Elektro(Schaltschrank)', 'Électro (armoire de commande)')


def heating(s, x, y0, y1):
    """A heating element after DIN EN 60617-4 (04-01-12): the resistor divided into four fields, vertical."""
    s.rect(x - 1.0, y0, x + 1.0, y1)
    for k in (1, 2, 3):
        y = y0 + k * (y1 - y0) / 4
        s.line((x - 1.0, y), (x + 1.0, y))
    return s


def pole(s, x, names, mark=None, length=4 * P, top=(0.9, -0.2)):
    """A make contact from (x, 0) to (x, length) with its terminal names shown; mark is the function at the fixed
    contact: 'contactor' (half circle), 'breaker' (cross) or 'switch' (circle, switch-disconnector)."""
    s.line((x, 0), (x, P)).line((x, 3 * P), (x, length)).line((x, 3 * P), (x + TIP[0], TIP[1]))
    if mark == 'contactor':
        s.arc(x, P - 0.7, 1.4, 90, 270)
    elif mark == 'breaker':
        s.line((x - 0.8, P - 0.8), (x + 0.8, P + 0.8)).line((x - 0.8, P + 0.8), (x + 0.8, P - 0.8))
    elif mark == 'switch':
        s.circle(x, P + 0.5, 1.0)
    s.pin(names[0], (x, 0), label=(x + top[0], top[1]), shown=True)
    return s.pin(names[1], (x, length), label=(x + 0.8, length - 2.4), shown=True)


def fuse_on_blade(s, x):
    """The fuse link as the moving contact: a rectangle along the blade."""
    pivot = (x, 3 * P)
    dx, dy = TIP[0], TIP[1] - 3 * P
    n = math.hypot(dx, dy)
    ux, uy = dx / n, dy / n
    cx, cy = pivot[0] + 0.36 * dx, pivot[1] + 0.36 * dy
    a, b = 1.0, 0.6
    return s.poly((cx + a * ux - b * uy, cy + a * uy + b * ux), (cx + a * ux + b * uy, cy + a * uy - b * ux),
                  (cx - a * ux + b * uy, cy - a * uy - b * ux), (cx - a * ux - b * uy, cy - a * uy + b * ux), fill=None)


def operating_coil(prefix, caption, parent=True):
    """The operating coil of a contactor or relay (A1 above, A2 below), as on the page "Schütze + Kontakte"."""
    s = Symbol(prefix, caption)
    s.parent = parent
    s.rect(-2 * P, P, 2 * P, 3 * P, w=0.35).line((0, 0), (0, P)).line((0, 3 * P), (0, 4 * P))
    s.pin('A1', (0, 0), label=(0.6, 0.2), shown=True).pin('A2', (0, 4 * P), label=(0.6, 4 * P - 2.4), shown=True)
    return s.labels((2 * P + 0.8, P), (2 * P + 0.8, 2 * P + 0.4))


def thermal_box(s, x0, y0, x1, y1):
    """The operating device of a thermal relay: a rectangle with the conductor bent into a step."""
    xm = (x0 + x1) / 2 + 0.6
    s.rect(x0, y0, x1, y1)
    return s.line((xm, y0), (xm, y0 + 1.0), (xm - 1.6, y0 + 1.0), (xm - 1.6, y1 - 1.0), (xm, y1 - 1.0), (xm, y1))


def delay(s, x, tip, y=2 * P - 0.4):
    """The delay mark (DIN EN 60617-7, 07-05): two lines from the blade to an arc left of it whose concave side faces
    left: the movement to the left, i.e. the return when the time relay releases, is delayed."""
    rad = 2.2
    cx = blade(x, y, tip)[0] - 2.4 - rad
    s.arc(cx, y, 2 * rad, 310, 410)
    for dy in (-0.5, 0.5):
        s.line((cx + math.sqrt(rad * rad - dy * dy), y + dy), blade(x, y + dy, tip))
    return s


def aux_contact(kind, caption, pins, delayed=False):
    s = unnamed(contact(kind, caption, pins=pins, operated=None if delayed else 'relay', numbers=True))
    if delayed:
        delay(s, 0, TIP if kind == 'no' else NC_TIP)
    return s


# 1, 2: heaters
s = Symbol('H', C('Heizung dreiphasig', 'Heater, three-phase', 'Chauffage triphasé'))
for k, name in enumerate(('L1', 'L2', 'L3')):
    x = 2 * P * k
    heating(s.line((x, 0), (x, 1.5 * P)), x, 1.5 * P, 3.5 * P).line((x, 3.5 * P), (x, 4 * P))
    s.pin(name, (x, 0), label=(x + 0.6, 0.2), shown=True)
s.line((0, 4 * P), (4 * P, 4 * P)).circle(2 * P, 4 * P, 0.8, fill='#000000')
outline(s, -P, P, 5 * P, 4.6 * P)
s.line((6 * P, 0), (6 * P, 2 * P), (5 * P, 2 * P)).pin('PE', (6 * P, 0), label=(6 * P + 0.6, 0.2), shown=True)
add(s.labels((6 * P + 0.8, 2.5 * P), (6 * P + 0.8, 3.5 * P)))
s = Symbol('H', C('Heizung einphasig', 'Heater, single-phase', 'Chauffage monophasé'))
heating(s.line((0, 0), (0, P)), 0, P, 3 * P).line((0, 3 * P), (0, 4 * P))
s.pin('L', (0, 0), label=(0.6, 0.2), shown=True).pin('N', (0, 4 * P), label=(0.6, 4 * P - 2.4), shown=True)
add(s.labels((2.0, P), (2.0, 2 * P + 0.4)))

# 3: contactor with its coil and three main contacts on one dashed link
s = operating_coil('K', C('Schütz dreipolig', 'Contactor, three-pole', 'Contacteur tripolaire'), parent=False)
xs = (4 * P, 6 * P, 8 * P)
for k, x in enumerate(xs):
    pole(s, x, (str(2 * k + 1), str(2 * k + 2)), 'contactor')
y = 2 * P - 0.4
dashed(s, (2 * P, y), blade(xs[-1], y))
add(s.labels((xs[-1] + 2.6, P + 0.4), (xs[-1] + 2.6, 2 * P + 0.6)))

# 4: the thermal overload's auxiliary break contact 95-96, worked by the thermal release
s = contact('nc', C('Motorschutzschalter (einpolig)', 'Motor protective circuit breaker (single-pole)',
                    'Disjoncteur-moteur (unipolaire)'), 'F', ('95', '96'), numbers=True)
thermal_box(s, -9.6, P, -5.6, 3 * P)
y = 2 * P - 0.6
add(dashed(s, (-5.6, y), blade(0, y, NC_TIP)))

# 5: motor protective circuit breaker: three poles with circuit-breaker function, thermal and magnetic release
s = Symbol('Q', C('Motorschutzschalter dreipolig', 'Motor protective circuit breaker, three-pole', 'Disjoncteur-moteur tripolaire'))
for k, x in enumerate((0, 2 * P, 4 * P)):
    pole(s, x, (f'L{k + 1}', f'T{k + 1}'), 'breaker')
y = 2 * P - 0.4
dashed(s, (-2.6, y), blade(4 * P, y))
s.rect(-6.2, y - 1.6, -2.6, y + 1.6).text('I>', -4.4, y - 0.9, 1.8, 'centre')
dashed(s, (-7.2, y), (-6.2, y))
s.rect(-10.8, y - 1.6, -7.2, y + 1.6).line((-8.2, y - 1.6), (-8.2, y - 0.7), (-9.6, y - 0.7), (-9.6, y + 0.7), (-8.2, y + 0.7), (-8.2, y + 1.6))
add(s.labels((4 * P + 2.6, P + 0.4), (4 * P + 2.6, 2 * P + 0.6)))

# 6: auxiliary contact block, a make and a break contact on one link
s = unnamed(Symbol('', C('Hilfskontakte 13/14, 21/22', 'Auxiliary contacts 13/14, 21/22', 'Contacts auxiliaires 13/14, 21/22')))
x = 2 * P
s.line((0, 0), (0, P)).line((0, 3 * P), (0, 4 * P)).line((0, 3 * P), TIP)
s.line((x, 0), (x, P), (x + 1.6, P)).line((x, 3 * P), (x, 4 * P)).line((x, 3 * P), (x + NC_TIP[0], NC_TIP[1]))
for name, at, label in (('13', (0, 0), (0.8, 0.2)), ('14', (0, 4 * P), (0.8, 4 * P - 2.4)),
                        ('21', (x, 0), (x + 0.8, 0.2)), ('22', (x, 4 * P), (x + 0.8, 4 * P - 2.4))):
    s.pin(name, at, label=label, shown=True)
y = 2 * P - 0.5
dashed(s, (-5.0, y), blade(x, y, NC_TIP))
add(s.labels((x + 2.6, P + 0.4), (x + 2.6, 2 * P + 0.6)))


# 7, 8: three-phase motors, the protective conductor led to the housing
def motor3(caption, names_below=()):
    s = Symbol('M', caption)
    cx, cy, d = 2 * P, 3 * P, 4 * P - 0.6
    s.circle(cx, cy, d).text('M', cx, cy - 3.4, 3.0, 'centre').text('3~', cx, cy + 0.2, 2.5, 'centre')
    rest = lambda dx: math.sqrt((d / 2) ** 2 - dx ** 2)
    top = ('U', 'V', 'W') if not names_below else ('U1', 'V1', 'W1')
    for k, name in enumerate(top):
        x = (k + 1) * P
        s.line((x, 0), (x, cy - rest(x - cx))).pin(name, (x, 0), label=(x + 0.3, -2.4), shown=True)
    for k, name in enumerate(names_below):
        x = (k + 1) * P
        s.line((x, 6 * P), (x, cy + rest(x - cx))).pin(name, (x, 6 * P), label=(x + 0.3, 6 * P + 0.6), shown=True)
    s.line((5 * P, 0), (5 * P, cy), (cx + d / 2, cy)).pin('PE', (5 * P, 0), label=(5 * P + 0.3, -2.4), shown=True)
    return s.labels((5 * P + 0.8, cy + 0.4), (5 * P + 0.8, cy + 3.2))


add(motor3(C('Drehstrommotor', 'Three-phase motor', 'Moteur triphasé')))
add(motor3(C('Drehstrommotor, sechs Anschlüsse', 'Three-phase motor, six terminals', 'Moteur triphasé, six bornes'), ('U2', 'V2', 'W2')))

# 9, 10: three-pole fuse-switch-disconnector (the fuse links as moving contacts) and three fuses
s = Symbol('F', C('Sicherungen dreipolig (Schalter)', 'Fuse-switch-disconnector, three-pole', 'Interrupteur-sectionneur à fusibles tripolaire'))
for k, x in enumerate((0, 2 * P, 4 * P)):
    fuse_on_blade(pole(s, x, (str(2 * k + 1), str(2 * k + 2)), 'switch'), x)
y = 4.3
dashed(s, blade(0, y), blade(4 * P, y))
add(s.labels((4 * P + 2.6, P + 0.4), (4 * P + 2.6, 2 * P + 0.6)))


def fuses(caption, n):
    s = Symbol('F', caption)
    for k in range(n):
        x = 2 * P * k
        s.rect(x - 1.0, P, x + 1.0, 3 * P).line((x, 0), (x, 4 * P))
        s.pin(str(2 * k + 1), (x, 0), label=(x + 0.6, 0.2), shown=True).pin(str(2 * k + 2), (x, 4 * P), label=(x + 0.6, 4 * P - 2.1), shown=True)
    return s.labels((2 * P * (n - 1) + 2.0, P), (2 * P * (n - 1) + 2.0, 2 * P + 0.4))


add(fuses(C('Sicherungen dreipolig', 'Fuses, three-pole', 'Fusibles tripolaires'), 3))

# 11: contactor coil
add(operating_coil('K', C('Spule (Schütz)', 'Coil (contactor)', 'Bobine (contacteur)')))


# 12 to 14: busbars (heavy lines), no parts
def rails(caption, names):
    s = Symbol('', caption, numbered=False, listed=False, shown=False)
    for k, name in enumerate(names):
        y = k * P
        s.line((0, y), (12 * P, y), w=THICK).text(name, -0.8, y - 1.0, 2.0, 'right')
    return s


add(rails(C('Schiene dreiphasig', 'Busbars, three-phase', 'Jeu de barres triphasé'), ('L1', 'L2', 'L3')))
add(rails(C('Neutralleiterschiene', 'Neutral busbar', 'Barre de neutre'), ('N',)))
add(rails(C('Schutzleiterschiene', 'Protective earth busbar', 'Barre de terre de protection'), ('PE',)))


# 15, 16: terminal blocks
def terminal_block(caption, n):
    s = Symbol('X', caption)
    for k in range(n):
        x = k * P
        s.rect(x - P / 2, -P, x + P / 2, P).circle(x, 0, 1.4).text(str(k + 1), x, P + 0.4, 1.8, 'centre')
        s.pin(str(k + 1), (x, 0))
    return s.labels((-P / 2, -P - 3.4), (-P / 2, P + 3.0))


add(terminal_block(C('Klemme dreipolig (Leiste)', 'Terminal block, three-pole', 'Bornier tripolaire'), 3))
add(terminal_block(C('Klemme sechspolig (Leiste)', 'Terminal block, six-pole', 'Bornier à six pôles'), 6))

# 17, 18: auxiliary contacts
add(aux_contact('no', C('Hilfskontakt 13/14', 'Auxiliary contact 13/14', 'Contact auxiliaire 13/14'), ('13', '14')))
add(aux_contact('nc', C('Hilfskontakt 21/22', 'Auxiliary contact 21/22', 'Contact auxiliaire 21/22'), ('21', '22')))


# 19, 20: the terminal rows of a PLC's digital input and output module
def plc(caption, heading, names):
    """A module housing with its terminals on the lower edge, each a small circle with a lead P down to its connection
    point, the terminal names written upwards inside."""
    s = Symbol('', caption, numbered=False, listed=False)
    n = len(names)
    s.rect(-P, -10.0, n * P, 0, w=0.35).text(heading, -P + 0.8, -9.4, 2.0)
    for k, name in enumerate(names):
        x = k * P
        s.line((x, 0.7), (x, P)).circle(x, 0, 1.4).text(name, x - 0.9, -1.0, 1.6, 'left', rotation=90)
        s.pin(name, (x, P))
    return s.labels((n * P + 0.8, -10.0), (n * P + 0.8, -7.2))


add(plc(C('SPS-Eingänge (Klemmen)', 'PLC inputs (terminals)', 'Entrées API (bornes)'), 'DI', [f'I0.{k}' for k in range(8)] + ['COM']))
add(plc(C('SPS-Ausgänge (Klemmen)', 'PLC outputs (terminals)', 'Sorties API (bornes)'), 'DO', [f'Q0.{k}' for k in range(8)] + ['COM']))

# 21, 22: push buttons, the illuminated one with its lamp X-Y in one housing
add(contact('no', C('Taster (Schließer)', 'Push button (make)', 'Bouton-poussoir (travail)'), pins=('1', '3'), operated='push', numbers=True))
s = contact('no', C('Leuchttaster', 'Illuminated push button', 'Bouton-poussoir lumineux'), pins=('1', '3'), operated='push', numbers=True)
x, d = 3 * P, 2 * P - 0.6
s.line((x, 0), (x, 2 * P - d / 2)).circle(x, 2 * P, d).line((x, 2 * P + d / 2), (x, 4 * P))
s.line(rot((x - d / 2, 2 * P), (x, 2 * P), 45), rot((x + d / 2, 2 * P), (x, 2 * P), 45))
s.line(rot((x - d / 2, 2 * P), (x, 2 * P), -45), rot((x + d / 2, 2 * P), (x, 2 * P), -45))
s.pin('X', (x, 0), label=(x + 0.6, 0.2), shown=True).pin('Y', (x, 4 * P), label=(x + 0.6, 4 * P - 1.9), shown=True)
for o in s.parts:
    if o['type'] == 'contact' and o['name'] == '3':
        o['pos'] = pt((0.8, 4 * P - 1.9))
outline(s, -5.6, 0.85 * P, x + 3.2, 3.15 * P)
add(s.labels((x + 4.0, P + 0.4), (x + 4.0, 2 * P + 0.6)))

# 23: solenoid valve: the coil with a field holding the valve sign
s = operating_coil('YQ', C('Magnetventil', 'Solenoid valve', 'Électrovanne'), parent=False)
x0, x1, cy = -2 * P - 3.6, -2 * P, 2 * P
s.rect(x0, P, x1, 3 * P, w=0.35)
s.poly((x0 + 0.3, cy - 1.0), (x0 + 0.3, cy + 1.0), ((x0 + x1) / 2, cy), fill=None).poly((x1 - 0.3, cy - 1.0), (x1 - 0.3, cy + 1.0), ((x0 + x1) / 2, cy), fill=None)
add(s)

# 24: indicator lamp
s = Symbol('H', C('Meldeleuchte', 'Indicator lamp', 'Voyant'))
d = 2 * P - 0.6
s.line((0, 0), (0, 2 * P - d / 2)).circle(0, 2 * P, d).line((0, 2 * P + d / 2), (0, 4 * P))
s.line(rot((-d / 2, 2 * P), (0, 2 * P), 45), rot((d / 2, 2 * P), (0, 2 * P), 45)).line(rot((-d / 2, 2 * P), (0, 2 * P), -45), rot((d / 2, 2 * P), (0, 2 * P), -45))
s.pin('X', (0, 0), label=(0.6, 0.2), shown=True).pin('Y', (0, 4 * P), label=(0.6, 4 * P - 2.4), shown=True)
add(s.labels((P + 1.2, P - 0.4), (P + 1.2, 2 * P + 0.6)))

# 25: thermostat: a change-over contact (1 common, 2 break, 4 make) worked by temperature, in the housing; drawn like
# contact('co') with the leads above one pitch longer, so that the housing passes between terminals and contacts; the
# temperature sign beside the blade as on the temperature sensitive switches of IEC 617-7 (07-09-01, 07-09-02)
s = Symbol('S', C('Thermostat', 'Thermostat', 'Thermostat'))
s.line((0, 0), (0, 2 * P), (1.6, 2 * P)).line((-P, 0), (-P, 2 * P + 0.6)).line((0, 4 * P), (0, 5 * P)).line((0, 4 * P), (NC_TIP[0], NC_TIP[1] + P))
s.pin('2', (0, 0), label=(0.8, 0.2), shown=True).pin('4', (-P, 0), label=(-P - 0.4, 0.2), align='right', shown=True)
s.pin('1', (0, 5 * P), label=(0.8, 5 * P - 1.8), shown=True)
s.text('ϑ', -1.7, 3 * P - 0.9, 3.0, 'centre')
outline(s, -4.4, 1.5 * P, 3.6, 4.2 * P)
add(s.labels((4.4, 2 * P), (4.4, 3 * P + 0.2)))

# 26: fan: the motor circle with three blades
s = Symbol('M', C('Lüfter', 'Fan', 'Ventilateur'))
d, cy = 3 * P - 0.6, 2.5 * P
s.line((0, 0), (0, cy - d / 2)).circle(0, cy, d).line((0, cy + d / 2), (0, 5 * P))
for a in (90, 210, 330):
    leaf = [(0.5 + 2.3 * (1 - math.cos(t)) / 2, 0.85 * math.sin(t)) for t in (math.pi * k / 12 for k in range(25))]
    s.poly(*[rot((px, cy + py), (0, cy), a + 12 * math.sin(math.pi * px / 2.8)) for px, py in leaf])
s.circle(0, cy, 1.0, fill='#ffffff')
s.pin('L', (0, 0), label=(0.6, 0.2), shown=True).pin('N', (0, 5 * P), label=(0.6, 5 * P - 2.4), shown=True)
add(s.labels((d / 2 + 1.0, P), (d / 2 + 1.0, 2 * P + 1.0)))

# 27: time relay, off-delay (slow-releasing coil: the filled field)
s = operating_coil('K', C('Zeitrelais', 'Time-delay relay', 'Relais temporisé'))
add(s.rect(-3 * P, P, -2 * P, 3 * P, w=0.35, fill='#000000'))

# 28: fuse
add(fuses(C('Sicherung', 'Fuse', 'Fusible'), 1))
# 29: push button, break contact
add(contact('nc', C('Taster (Öffner)', 'Push button (break)', 'Bouton-poussoir (repos)'), pins=('21', '22'), operated='push', numbers=True))
# 30: the delayed make contact of the time relay
add(aux_contact('no', C('Hilfskontakt 17/18', 'Auxiliary contact 17/18', 'Contact auxiliaire 17/18'), ('17', '18'), delayed=True))

# 31: current transformer: the primary conductor through the ring core, the secondary terminals k and l
s = Symbol('', C('Stromwandler', 'Current transformer', 'Transformateur de courant'), numbered=False)
d = 3 * P
s.line((-2.5 * P, 0), (2.5 * P, 0)).circle(0, 0, d)
edge = math.sqrt((d / 2) ** 2 - P ** 2)
s.line((-P, edge), (-P, 2 * P)).line((P, edge), (P, 2 * P))
s.pin('k', (-P, 2 * P), label=(-P - 0.4, 2 * P - 2.2), align='right', shown=True).pin('l', (P, 2 * P), label=(P + 0.4, 2 * P - 2.2), shown=True)
add(s.labels((d / 2 + 1.0, -d / 2 - 1.4), (d / 2 + 1.0, -d / 2 + 1.4)))

# 32, 33: break contact 11/12 and the delayed break contact 15/16 of the time relay
add(aux_contact('nc', C('Hilfskontakt 11/12', 'Auxiliary contact 11/12', 'Contact auxiliaire 11/12'), ('11', '12')))
add(aux_contact('nc', C('Hilfskontakt 15/16', 'Auxiliary contact 15/16', 'Contact auxiliaire 15/16'), ('15', '16'), delayed=True))

# --- FET: (v) as on the page "FET" (gate left, drain up, source down), (h) the same turned a quarter clockwise
# (gate up, drain right, source left). "Mit Substrat": the substrate joined to the source (IEC 617-5 05-05-14);
# without, the substrate arrow stays unconnected (05-05-11, 05-05-12). The BF960 is a depletion type with source and
# substrate interconnected (Philips data sheet BF960, December 1988), drawn after 05-05-17.
add = page(USR, 'FET', 'FET', 'FET')


def fet(s, value='', substrate=True, h=False):
    s.prefix, s.value = 'Q', value
    if not substrate:
        s.parts = [o for o in s.parts if not (o['type'] == 'line' and o['points'] == [pt((2 * P, 0)), pt((2 * P, 1.7))])]
    if h:
        s.vertical().labels((4.6, -2.6), (4.6, -0.2))
    return s


def dual_gate(caption):
    """Depletion-type N-channel MOSFET with two gates: G2 (drain side) above, G1 (source side) below, the substrate
    joined to the source."""
    s = Symbol('Q', caption)
    s.circle(4.6, 0, 7.4)
    s.line((0, -P), (2.6, -P), (2.6, -0.4)).line((0, P), (2.6, P), (2.6, 0.4))
    s.line((3.4, -2.2), (3.4, 2.2), w=THICK)
    s.line((3.4, -1.7), (2 * P, -1.7), (2 * P, -2 * P)).line((3.4, 1.7), (2 * P, 1.7), (2 * P, 2 * P))
    s.line((2 * P, 0), (3.4, 0), end='arrow', size=1.3).line((2 * P, 0), (2 * P, 1.7))
    s.pin('G2', (0, -P)).pin('G1', (0, P)).pin('D', (2 * P, -2 * P)).pin('S', (2 * P, 2 * P))
    return s.labels((9.2, -2.6), (9.2, 0.4))


ORIENT = {False: ('v', 'vertical', 'vertical'), True: ('h', 'horizontal', 'horizontal')}
for h in (False, True):
    m, en, fr = ORIENT[h]
    sfx = lambda d, e, f: C(f'{d} ({m})', f'{e}, {en}', f'{f}, {fr}')
    add(fet(jfet(False, sfx('J-FET N-Kanal', 'N-channel JFET', 'JFET canal N')), '' if h else 'BF245', h=h))
    add(fet(jfet(True, sfx('J-FET P-Kanal', 'P-channel JFET', 'JFET canal P')), '' if h else '2N3820', h=h))
    for p, k in ((False, 'N'), (True, 'P')):
        add(fet(mosfet(p, False, sfx(f'MOSFET {k}-Kanal, Anreicherung', f'{k}-channel MOSFET, enhancement',
                                     f'MOSFET canal {k}, enrichissement')), substrate=False, h=h))
    for p, k in ((False, 'N'), (True, 'P')):
        kind = ('Anreicherung, ', 'enhancement, ', 'enrichissement, ') if not h else ('', '', '')
        add(fet(mosfet(p, False, sfx(f'MOSFET {k}-Kanal, {kind[0]}mit Substrat', f'{k}-channel MOSFET, {kind[1]}substrate to source',
                                     f'MOSFET canal {k}, {kind[2]}substrat relié à la source')), h=h))
    add(fet(dual_gate(sfx('Dual-Gate-MOSFET N-Kanal', 'N-channel dual-gate MOSFET', 'MOSFET double grille canal N')), '' if h else 'BF960', h=h))

# --- FLIP FLOP: boxes after IEC 60617-12 like box(), the inputs on the left, Q and /Q on the right; the contacts named
# by function, showing the pin numbers of the first section in the data sheet.
add = page(USR, 'FLIP FLOP', 'FLIP FLOP', 'BASCULES')


def chip(caption, value, inputs, outputs, mark=None):
    """Inputs and outputs are (name, pin number, kind) per row, None for an empty row; kind '' plain, 'neg' with a
    negation circle, 'dyn' dynamic (edge-triggered), 'negdyn' both. The text inside is the name without "/"."""
    rows = max(len(inputs), len(outputs))
    h = (rows + 1) * P
    s = Symbol('IC', caption, value)
    x0, x1 = P, 5 * P
    s.rect(x0, 0, x1, h, w=0.35)
    for k, item in enumerate(inputs):
        if not item:
            continue
        name, number, kind = item
        y = (k + 1) * P
        if kind.startswith('neg'):
            s.circle(x0 - 0.6, y, 1.2).line((0, y), (x0 - 1.2, y))
        else:
            s.line((0, y), (x0, y))
        if kind.endswith('dyn'):
            s.line((x0, y - 0.9), (x0 + 1.2, y), (x0, y + 0.9))
        s.text(name.lstrip('/'), x0 + (1.5 if kind.endswith('dyn') else 0.6), y - 1.0, 2.0)
        s.pin(name, (0, y), label=(0.2, y - 2.2), shown=True, text=number)
    for k, item in enumerate(outputs):
        if not item:
            continue
        name, number = item
        y = (k + 1) * P
        if name.startswith('/'):
            s.circle(x1 + 0.6, y, 1.2).line((x1 + 1.2, y), (x1 + P, y))
        else:
            s.line((x1, y), (x1 + P, y))
        s.text(name.lstrip('/'), x1 - 0.6, y - 1.0, 2.0, 'right')
        s.pin(name, (x1 + P, y), label=(x1 + P - 0.2, y - 2.2), align='right', shown=True, text=number)
    for o in s.parts:
        if o['type'] == 'contact':
            o['font'] = {'height': 1.6}
    if mark:
        mark(s, x0, x1)
    return s.labels((x0, -3.4), (x0, h + 0.6))


def monostable(s, x0, x1):
    """The qualifying symbol of a non-retriggerable monostable: 1 and a single pulse."""
    x, y = x1 - 4.6, 2.2
    s.text('1', x, y - 1.8, 2.0)
    return s.line((x + 1.4, y), (x + 1.9, y), (x + 1.9, y - 1.4), (x + 3.1, y - 1.4), (x + 3.1, y), (x + 3.6, y), w=0.18)


add(chip(C('D-Flipflop mit Setzen und Rücksetzen (½ 74HC74)', 'D flip-flop with set and reset (½ 74HC74)',
           'Bascule D avec mise à 1 et à 0 (½ 74HC74)'), '½ 74HC74',
         [('/S', '4', 'neg'), ('D', '2', ''), ('CK', '3', 'dyn'), ('/R', '1', 'neg')], [('Q', '5'), None, None, ('/Q', '6')]))
add(chip(C('JK-Flipflop mit Setzen und Rücksetzen (½ 74HC76)', 'JK flip-flop with set and reset (½ 74HC76)',
           'Bascule JK avec mise à 1 et à 0 (½ 74HC76)'), '½ 74HC76',
         [('/S', '2', 'neg'), ('J', '4', ''), ('CK', '1', 'negdyn'), ('K', '16', ''), ('/R', '3', 'neg')],
         [('Q', '15'), None, None, None, ('/Q', '14')]))
add(chip(C('J/K-Flipflop mit Setzen und Rücksetzen (½ 74HC109)', 'J/K flip-flop with set and reset (½ 74HC109)',
           'Bascule J/K avec mise à 1 et à 0 (½ 74HC109)'), '½ 74HC109',
         [('/S', '5', 'neg'), ('J', '2', ''), ('CK', '4', 'dyn'), ('/K', '3', 'neg'), ('/R', '1', 'neg')],
         [('Q', '6'), None, None, None, ('/Q', '7')]))
add(chip(C('JK-Flipflop mit Rücksetzen (½ 74HC73)', 'JK flip-flop with reset (½ 74HC73)', 'Bascule JK avec mise à 0 (½ 74HC73)'),
         '½ 74HC73', [('J', '14', ''), ('/CK', '1', 'negdyn'), ('K', '3', ''), ('/R', '2', 'neg')], [('Q', '12'), None, None, ('/Q', '13')]))
add(chip(C('Monoflop mit Rücksetzen (½ 74HC221)', 'Monostable with reset (½ 74HC221)', 'Monostable avec remise à zéro (½ 74HC221)'),
         '½ 74HC221', [('R-C', '15', ''), ('C', '14', ''), ('/A', '1', 'neg'), ('B', '2', ''), ('/R', '3', 'neg')],
         [None, None, ('Q', '13'), ('/Q', '4')], mark=monostable))

# --- GATEs: the distinctive shapes of gate_us(), the value the 74HC type. Contacts 1, 2, 3 are the pins of the first
# gate in the data sheet and shown (TI SCLS181H HC00, SCLS078H HC04, SCLS081J HC08, SCLS085L HC14, SCLS200F HC32,
# SCLS100F HC86, SCLS135G HC266 with open-drain outputs; the HC02 has its output on pin 1, TI SCLS076G); the
# three-input gates have the given 1 to 3 (inputs) and 4 (output), not the package's pins (HC10/11/27: 1, 2, 13 to 12;
# HC4075: 3, 4, 5 to 6), so their contact names stay hidden.
add = page(USR, 'GATEs', 'GATEs', 'PORTES')


def gate(kind, negated, caption, value, numbers=None):
    """numbers: the contact names of input 1, input 2 and the output (default 1, 2, 3)."""
    s = gate_us(kind, negated, caption)
    s.prefix, s.value = 'N', value
    if value:
        names = dict(zip(('1', '2', '3'), numbers or ('1', '2', '3')))
        for o in s.parts:
            if o['type'] == 'contact':
                o['name'] = o['text'] = names.get(o['name'], o['name'])
                o['visible'] = True
    return s


def gate3(kind, negated, caption, value):
    """A gate of gate() with a third input in the middle: inputs 1 to 3, output 4."""
    s = gate(kind, negated, caption, value)
    out = 5 * P
    s.parts = [o for o in s.parts if o['type'] != 'contact']
    back = 1.05 if kind in ('or', 'xor') else 0      # where the curved back crosses the middle row
    s.line((0, P), (P + back, P))
    for name, at in (('1', (0, 0)), ('2', (0, P)), ('3', (0, 2 * P))):
        s.pin(name, at)
    return s.pin('4', (out, P), label=(out - 0.4, P - 2.2), align='right')


def hysteresis(s, x, y, k=0.9, w=W):
    s.line((x, y), (x + 1.0 * k, y), (x + 1.0 * k, y - 1.2 * k), (x + 2.0 * k, y - 1.2 * k), w=w)
    return s.line((x + 0.4 * k, y), (x + 0.4 * k, y - 1.2 * k), (x + 1.4 * k, y - 1.2 * k), w=w)


add(gate('buf', True, C('Inverter (HC04)', 'Inverter (HC04)', 'Inverseur (HC04)'), '74HC04'))
add(hysteresis(gate('buf', True, C('Schmitt-Trigger-Inverter (HC14)', 'Schmitt trigger inverter (HC14)', 'Inverseur trigger de Schmitt (HC14)'),
                    '74HC14'), P + 0.7, P + 0.55))
add(gate('and', True, C('NAND 2 (¼ HC00)', 'NAND 2 (¼ HC00)', 'NON-ET 2 (¼ HC00)'), '74HC00'))
add(gate('or', True, C('NOR 2 (¼ HC02)', 'NOR 2 (¼ HC02)', 'NON-OU 2 (¼ HC02)'), '74HC02', ('2', '3', '1')))
add(gate('and', False, C('UND 2 (¼ HC08)', 'AND 2 (¼ HC08)', 'ET 2 (¼ HC08)'), '74HC08'))
add(gate('or', False, C('ODER 2 (¼ HC32)', 'OR 2 (¼ HC32)', 'OU 2 (¼ HC32)'), '74HC32'))
s = gate('xor', True, C('XNOR 2 (¼ HC266)', 'XNOR 2 (¼ HC266)', 'NON-OU exclusif 2 (¼ HC266)'), '74HC266')
add(output_mark(s, 'oc', 2 * P + 0.6, P - 0.3))      # open-drain output
add(gate('xor', False, C('XOR 2 (¼ HC86)', 'XOR 2 (¼ HC86)', 'OU exclusif 2 (¼ HC86)'), '74HC86'))
add(gate3('and', True, C('NAND 3 (HC10)', 'NAND 3 (HC10)', 'NON-ET 3 (HC10)'), '74HC10'))
add(gate3('or', True, C('NOR 3 (HC27)', 'NOR 3 (HC27)', 'NON-OU 3 (HC27)'), '74HC27'))
add(gate3('and', False, C('UND 3 (HC11)', 'AND 3 (HC11)', 'ET 3 (HC11)'), '74HC11'))
add(gate3('or', False, C('ODER 3 (HC4075)', 'OR 3 (HC4075)', 'OU 3 (HC4075)'), '74HC4075'))
add(gate('buf', False, C('Treiber', 'Buffer', 'Tampon'), ''))

# --- GRAPHICS: drawing aids. Where a text names something (a signal, a current, a voltage), it is the value, shown and
# editable; the designator stays empty and hidden.
add = page(USR, 'GRAPHICS', 'GRAPHICS', 'GRAPHIQUES')


def aid(caption, value=None):
    return Symbol('', caption, value or '', numbered=False, listed=False, shown=False, value_shown=value is not None)


def polyline_arc(cx, cy, rad, a0, a1, n=16):
    return [(cx + rad * math.cos(math.radians(a0 + (a1 - a0) * k / n)), cy - rad * math.sin(math.radians(a0 + (a1 - a0) * k / n)))
            for k in range(n + 1)]


s = aid(C('Ein-/Ausgangsbeschriftung', 'Input/output label', 'Étiquette d’entrée/sortie'), 'IN')
add(s.line((0, -1.27), (0, 1.27), w=THICK).labels((0.8, -1.25), (0.8, -1.25)))
s = aid(C('Anschluss (Kreis)', 'Terminal (circle)', 'Borne (cercle)'), 'Vout')
add(s.circle(0, 0, 1.6).pin('1', (0, 0)).labels((0, -4.0), (0, -4.0), 'centre'))
s = aid(C('Anschluss (Quadrat)', 'Terminal (square)', 'Borne (carré)'), '')
add(s.rect(-0.8, -0.8, 0.8, 0.8).pin('1', (0, 0)).labels((0, -4.0), (0, -4.0), 'centre'))
s = aid(C('Strompfeil (h)', 'Current arrow, horizontal', 'Flèche de courant, horizontale'), 'I1')
add(s.arrow((0, 0), (2 * P, 0), size=1.2).labels((P, -3.6), (P, -3.6), 'centre'))
s = aid(C('Strompfeil (v)', 'Current arrow, vertical', 'Flèche de courant, verticale'), 'I1')
add(s.arrow((0, 0), (0, 2 * P), size=1.2).labels((1.0, P - 1.25), (1.0, P - 1.25)))
s = aid(C('Spannungspfeil', 'Voltage arrow', 'Flèche de tension'), 'V1')
add(s.arrow((0, 0), (0, 4 * P), size=1.6).labels((1.0, 2 * P - 1.25), (1.0, 2 * P - 1.25)))
s = aid(C('Markierung', 'Marker', 'Repère'), 'A')
add(s.rect(-2.0, -2.0, 2.0, 2.0, w=0.35).labels((0, -1.25), (0, -1.25), 'centre'))
s = aid(C('Zeichnungsnummer', 'Drawing number', 'Numéro de dessin'), '0001')
s.rect(0, 0, 36.0, 6.0, w=0.35).line((17.0, 0), (17.0, 6.0)).text('Zeichnungs-Nr.', 1.0, 2.0, 2.0)
add(s.labels((18.0, 1.75), (18.0, 1.75)))
s = aid(C('Spannungsangabe', 'Voltage annotation', 'Indication de tension'), '1V DC')
add(s.line((2 * P + 3.0, -2 * P), (2 * P, -2 * P), (0.3, -0.3), end='arrow', size=1.2).labels((2 * P + 3.4, -2 * P - 1.25), (2 * P + 3.4, -2 * P - 1.25)))
# the screen as a broken ring around the conductors (IEC 617-3 03-01-07), its lead below
s = aid(C('Kabelschirm', 'Cable screen', 'Blindage de câble'))
s.parts.append({'type': 'ellipse', 'centre': [0, 0], 'size': [3.0, r(4 * P)], 'pen': {'width': W, 'style': 'dash'}})
add(s.line((0, 2 * P), (0, 3 * P)))
add(aid(C('Nicht angeschlossen', 'Not connected', 'Non connecté')).line((-1.0, -1.0), (1.0, 1.0)).line((-1.0, 1.0), (1.0, -1.0)))
add(aid(C('Plus (Kreis)', 'Plus (circle)', 'Plus (cercle)')).circle(0, 0, 3.4).line((-1.0, 0), (1.0, 0)).line((0, -1.0), (0, 1.0)))
add(aid(C('Null (Kreis)', 'Zero (circle)', 'Zéro (cercle)')).circle(0, 0, 3.4).text('0', 0, -1.1, 2.2, 'centre'))
add(aid(C('Minus (Kreis)', 'Minus (circle)', 'Moins (cercle)')).circle(0, 0, 3.4).line((-1.0, 0), (1.0, 0)))
add(aid(C('Masse (Zeichen)', 'Ground (sign)', 'Masse (signe)')).line((0, 0), (0, P), w=THICK).line((-2.0, P), (2.0, P), w=0.7))
add(aid(C('Phase (Sinus)', 'Phase (sine)', 'Phase (sinus)')).line(*[(x / 4, -1.6 * math.sin(math.pi * x / 12)) for x in range(25)]))
for letter in ('v', 't'):
    s = aid(C(f'Pfeil {letter} (Text)', f'Arrow {letter} (text)', f'Flèche {letter} (texte)'))
    add(s.poly((0, -0.45), (0, 0.45), (8.0, 0)).text(letter, 4.0, -3.4, 2.5, 'centre'))
s = aid(C('Pfeil N senkrecht', 'North arrow, vertical', 'Flèche nord, verticale'))
add(s.line((0, 2 * P), (0, -2 * P), end='arrow', size=1.8).text('N', 0, -2 * P - 3.4, 2.5, 'centre', bold=True))
s = aid(C('Pfeil N waagerecht', 'North arrow, horizontal', 'Flèche nord, horizontale'))
add(s.line((-2 * P, 0), (2 * P, 0), end='arrow', size=1.8).text('N', 2 * P + 1.0, -1.25, 2.5, bold=True))
s = aid(C('Pfeil N schräg', 'North arrow, diagonal', 'Flèche nord, oblique'))
add(s.line((-1.5 * P, 1.5 * P), (1.5 * P, -1.5 * P), end='arrow', size=1.8).text('N', 1.5 * P + 0.8, -1.5 * P - 2.6, 2.5, bold=True))
s = aid(C('Pfeilfeld (INPUT)', 'Arrow box (INPUT)', 'Flèche à texte (INPUT)'), 'INPUT')
add(s.poly((0, -2.0), (13.0, -2.0), (13.0, -3.4), (17.0, 0), (13.0, 3.4), (13.0, 2.0), (0, 2.0), fill=None, w=0.35).labels((6.5, -1.25), (6.5, -1.25), 'centre'))


def source(prefix, caption, value, draw):
    s = Symbol(prefix, caption, value)
    draw(s)
    s.pin('1', (0, 0), label=(0.6, 0.2)).pin('2', (0, 4 * P), label=(0.6, 4 * P - 2.4))
    return s.labels((P + 1.2, P - 0.4), (P + 1.2, 2 * P + 0.6))


D = 2 * P - 0.6
add(source('V', C('Spannungsquelle', 'Voltage source', 'Source de tension'), '1V',
           lambda s: s.line((0, 0), (0, 2 * P - D / 2)).circle(0, 2 * P, D).line((0, 2 * P + D / 2), (0, 4 * P)).arrow((0, 2 * P - 1.6), (0, 2 * P + 1.6), size=1.0)))
add(source('I', C('Stromquelle', 'Current source', 'Source de courant'), '1mA',
           lambda s: s.line((0, 0), (0, 2 * P - D / 2)).circle(0, 2 * P, D).line((0, 2 * P + D / 2), (0, 4 * P)).line((-D / 2, 2 * P), (D / 2, 2 * P))))
add(source('I', C('Stromquelle (zwei Kreise)', 'Current source (two circles)', 'Source de courant (deux cercles)'), '50 µA',
           lambda s: s.line((0, 0), (0, 2 * P - 2.6)).circle(0, 2 * P - 1.0, 3.2).circle(0, 2 * P + 1.0, 3.2).line((0, 2 * P + 2.6), (0, 4 * P))))
s = aid(C('Pinnummer und Name', 'Pin number and name', 'Numéro et nom de broche'))
add(s.line((0, 0), (P, 0)).pin('1', (0, 0), label=(0.2, -2.2), shown=True, text='0').text('VDD', P + 0.6, -1.0, 1.8))
s = aid(C('Drehrichtung (h)', 'Direction of rotation, horizontal', 'Sens de rotation, horizontal'), 'CW')
add(s.line(*polyline_arc(0, 0, 3.0, 150, 30), end='arrow', size=1.2).labels((0, -6.4), (0, -6.4), 'centre'))
s = aid(C('Drehrichtung (v)', 'Direction of rotation, vertical', 'Sens de rotation, vertical'), 'CW')
add(s.line(*polyline_arc(0, 0, 3.0, 60, -60), end='arrow', size=1.2).labels((4.0, -1.25), (4.0, -1.25)))
s = aid(C('Funktionsbeschriftung', 'Function label', 'Étiquette de fonction'))
add(s.line((0, 0), (4 * P, 0), start='arrow', end='arrow', size=1.2).text('BYPASS', -1.0, -1.0, 2.0, 'right').text('EFF ON', 4 * P + 1.0, -1.0, 2.0))

# --- IND: inductors after DIN EN 60617-4 (half-turn coil, core a heavy line, ferrite core a dashed heavy line, the
# choke a filled block), each standing and lying; the long ones 6 * P.
add = page(USR, 'IND', 'IND', 'IND')


def inductor(caption, length=4 * P, kind='coil', upright=False):
    s = two_pin('L', caption, '100µH', ask=True, length=length)
    end = length - P
    s.line((0, 0), (P, 0)).line((end, 0), (length, 0))
    if kind == 'block':
        s.rect(P, -1.0, end, 1.0, fill='#000000')
    else:
        coil(s, P, end, turns=round((end - P) / (P / 2)))
    if kind == 'core':
        s.line((P, -1.9), (end, -1.9), w=THICK)
    elif kind == 'ferrite':
        dashes(s, (P, -1.9), (end, -1.9))
    if upright:
        x = 3.2 if kind in ('core', 'ferrite') else 2.4
        s.vertical().labels((x, length / 2 - P - 0.4), (x, length / 2 + 0.4))
    return s


for de, en, fr, length, kind in (('Spule', 'Inductor', 'Bobine', 4 * P, 'coil'),
                                 ('Spule mit Kern', 'Inductor with core', 'Bobine avec noyau', 4 * P, 'core'),
                                 ('Spule mit Ferritkern', 'Inductor with ferrite core', 'Bobine à noyau de ferrite', 4 * P, 'ferrite'),
                                 ('Drossel', 'Choke', 'Self', 4 * P, 'block'),
                                 ('Drossel, lang', 'Choke, long', 'Self, longue', 6 * P, 'block'),
                                 ('Spule, lang', 'Inductor, long', 'Bobine, longue', 6 * P, 'coil')):
    for upright in (True, False):
        m, e, f = ORIENT[not upright]
        add(inductor(C(f'{de} ({m})', f'{en}, {e}', f'{fr}, {f}e'), length, kind, upright))
