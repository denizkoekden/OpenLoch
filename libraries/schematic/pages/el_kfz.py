# More symbols for the page "Kfz" (vehicle electrics). Components after DIN 40719 / DIN EN 60617 as vehicle wiring
# diagrams draw them, the function sign in a rectangle or circle, each terminal named and shown after DIN 72552
# (30 battery plus, 31 ground, 49 flasher, 50 starter, 53 wiper, 85/86 relay coil, 87 relay contact, B+, D+). As in such
# diagrams the designator stays empty; the parts are listed. Then telltales and indicators after ISO 2575: pictograms
# in a square frame, without contacts.
add = extend(KFZ, 'Kfz')
F = 4 * P          # side of a telltale's frame


def part(caption):
    return Symbol('', caption, numbered=False)


def terminal(s, name, body, end):
    """A lead from the body to the connection point `end`, the terminal's name beside the lead."""
    s.line(body, end)
    if end[1] == body[1]:
        left = end[0] < body[0]
        return s.pin(name, end, label=(end[0] + (0.4 if left else -0.4), end[1] - 2.2), align='left' if left else 'right', shown=True)
    up = end[1] < body[1]
    return s.pin(name, end, label=(end[0] + 0.5, end[1] + 0.2 if up else end[1] - 2.4), shown=True)


def above(s, x0, x1, y):
    """Designator and value above the body, left and right."""
    return s.labels((x0, y - 3.4), (x1, y - 3.4), 'left', 'right')


def outline(s, x0, y0, x1, y1):
    """The dash-dot outline of a device made of several elements."""
    s.rect(x0, y0, x1, y1)
    s.parts[-1]['pen']['style'] = 'dashDot'
    return s


def coil_box(s, x, y0, y1, half=1.6):
    """A relay winding: a rectangle with a diagonal."""
    return s.rect(x - half, y0, x + half, y1, w=0.35).line((x - half, y1), (x + half, y0))


def on_blade(pivot, tip, y):
    """The point at height y on a contact blade from pivot to tip."""
    t = (pivot[1] - y) / (pivot[1] - tip[1])
    return (pivot[0] + t * (tip[0] - pivot[0]), y)


def wave(s, x0, x1, y, n=None):
    """A wavy line of half waves from x0 to x1."""
    n = n or max(2, round((x1 - x0) / 1.2))
    d = (x1 - x0) / n
    for k in range(n):
        s.arc(x0 + d * (k + 0.5), y, d, 0 if k % 2 == 0 else 180, 180 if k % 2 == 0 else 360)
    return s


# --- Relays (ISO mini relay): the dash-dot outline with the terminals as small circles on it
def relay(caption, changeover):
    """The coil 86 (above) to 85 (below) at the left; the contact from 30 (below) to 87 above, a change-over contact
    resting on 87a; a make relay has a suppression resistor across its coil."""
    s = part(caption)
    xc = 3 * P if changeover else 2 * P
    left, right = (-P, xc + 2.8) if changeover else (-2 * P - 1.6, xc + 1.6)
    outline(s, left, 0, right, 4 * P)
    s.line((0, 0.5), (0, P)).line((0, 3 * P), (0, 4 * P - 0.5))
    coil_box(s, 0, P, 3 * P)
    if not changeover:
        x = -2 * P
        s.line((0, 0.5 * P), (x, 0.5 * P), (x, P + 0.2)).rect(x - 0.8, P + 0.2, x + 0.8, 3 * P - 0.2)
        s.line((x, 3 * P - 0.2), (x, 3.5 * P), (0, 3.5 * P))
        s.circle(0, 0.5 * P, 0.7, fill='#000000').circle(0, 3.5 * P, 0.7, fill='#000000')
    pivot = (xc, 3 * P)
    s.line((xc, 4 * P - 0.5), pivot)
    if changeover:
        s.line((xc, 0.5), (xc, P), (xc + 1.6, P))
        s.line((xc - P, 0.5), (xc - P, P + 0.6))
        tip = (xc + 2.1, P - 0.4)
    else:
        s.line((xc, 0.5), (xc, P))
        tip = (xc - 1.9, P + 0.6)
    s.line(pivot, tip)
    y = 2 * P - 0.4
    dashed(s, (1.6, y), on_blade(pivot, tip, y))
    pins = [('86', (0, 0)), ('85', (0, 4 * P)), ('30', (xc, 4 * P))]
    pins += [('87', (xc - P, 0)), ('87a', (xc, 0))] if changeover else [('87', (xc, 0))]
    for name, at in pins:
        s.circle(at[0], at[1], 1.0, fill='#ffffff')
        s.pin(name, at, label=(at[0] + 0.4, -2.3 if at[1] == 0 else 4 * P + 0.4), shown=True)
    return s.labels((right + 1.0, 0.4), (right + 1.0, 3.4))


add(relay(C('Wechselrelais', 'Change-over relay', 'Relais inverseur'), True))
add(relay(C('Schließerrelais', 'Make-contact relay', 'Relais à contact de travail'), False))


# --- Telltales and indicators after ISO 2575
def telltale(caption, draw):
    s = part(caption)
    s.rect(0, 0, F, F)
    draw(s)
    return above(s, 0, F, 0)


def oil_can(s):
    s.poly((2.4, 7.2), (2.4, 4.8), (3.6, 4.8), (3.6, 4.2), (4.8, 4.2), (4.8, 4.8), (6.2, 4.8), (8.6, 3.3), (8.7, 3.8),
           (6.6, 5.8), (6.6, 7.2), fill=None)
    s.arc(2.4, 6.0, 2.4, 90, 270)
    s.poly((8.7, 4.7), (8.25, 5.6), (9.15, 5.6), fill='#000000').circle(8.7, 5.75, 0.95, fill='#000000')


def coolant(s):
    c = F / 2
    s.rect(c - 0.5, 1.2, c + 0.5, 6.0).circle(c, 6.6, 1.9, fill='#000000')
    for y in (2.2, 3.2, 4.2):
        s.line((c + 0.5, y), (c + 1.7, y))
    wave(s, 1.2, c - 1.4, 6.6, 2)
    wave(s, c + 1.4, F - 1.2, 6.6, 2)
    wave(s, 1.2, F - 1.2, 8.2, 6)


def hazard(s):
    c = F / 2
    s.circle(c, c, 6.4).arc(c, c, 8.6, 135, 225).arc(c, c, 8.6, 315, 405)
    s.poly((c, c - 2.7), (c - 2.5, c + 1.6), (c + 2.5, c + 1.6), fill=None)
    s.line((c, c - 1.4), (c, c + 0.1), w=0.5).circle(c, c + 0.8, 0.5, fill='#000000')


def engine(s):
    s.poly((2.8, 3.8), (7.4, 3.8), (7.4, 4.6), (8.4, 4.6), (8.4, 3.9), (9.0, 3.9), (9.0, 7.0), (8.4, 7.0), (8.4, 6.2),
           (7.4, 6.2), (7.4, 7.6), (3.8, 7.6), (2.8, 6.6), fill=None)
    s.rect(4.0, 2.2, 6.2, 2.8).rect(4.7, 2.8, 5.5, 3.8)
    s.line((1.4, 4.4), (1.4, 6.8), w=THICK).line((1.4, 5.6), (2.8, 5.6))


SEGMENTS = {'a': ((0, 0), (1, 0)), 'b': ((1, 0), (1, 1)), 'c': ((1, 1), (1, 2)), 'd': ((0, 2), (1, 2)),
            'e': ((0, 1), (0, 2)), 'f': ((0, 0), (0, 1)), 'g': ((0, 1), (1, 1))}


def digit(s, x, y, w, h, segments='abcdefg', lw=THICK):
    """A seven-segment digit w wide and h high, its top left corner at (x, y)."""
    for k in segments:
        (ax, ay), (bx, by) = SEGMENTS[k]
        s.line((x + ax * w, y + ay * h / 2), (x + bx * w, y + by * h / 2), w=lw)
    return s


def speedometer(s):
    w, g = 2.2, 1.0
    x0 = F / 2 - 2 * w - g
    digit(s, x0, 1.4, w, 3.8, 'bc')
    for k in (1, 2):
        digit(s, x0 + k * (w + g), 1.4, w, 3.8)
    for k in range(6):
        digit(s, 1.68 + k * 1.2, 6.8, 0.8, 1.4, lw=0.18)


def fuel_pump(s):
    s.rect(2.4, 2.0, 6.0, 8.4).rect(3.0, 2.8, 5.4, 4.6).line((1.8, 8.4), (6.6, 8.4), w=THICK)
    s.line((6.0, 3.2), (7.0, 3.2), (7.8, 4.0), (7.8, 6.6)).rect(7.4, 6.6, 8.2, 7.8, fill='#000000')


def main_beam(s):
    s.line((5.4, 2.6), (5.4, 7.6)).arc(5.4, 5.1, 6.4, 270, 450, dy=5.0)
    for k in range(5):
        s.line((1.4, 3.1 + k), (4.4, 3.1 + k), w=0.35)


def fuel_gauge(s):
    for k in range(5):
        x = (F - 7.6) / 2 + 1.6 * k
        s.rect(x, 8.6 - 1.2 * (k + 1), x + 1.2, 8.6, fill='#000000')


add(telltale(C('Öldruck-Kontrollleuchte', 'Oil pressure warning lamp', 'Témoin de pression d’huile'), oil_can))
add(telltale(C('Kühlmitteltemperatur-Anzeige', 'Coolant temperature indicator', 'Indicateur de température du liquide de refroidissement'), coolant))
add(telltale(C('Allradantrieb (4WD)', 'Four-wheel drive (4WD)', 'Transmission intégrale (4WD)'), lambda s: s.text('4WD', F / 2, F / 2 - 1.5, 3.0, 'centre', bold=True)))
add(telltale(C('Warnblinklicht', 'Hazard warning lights', 'Feux de détresse'), hazard))
add(telltale(C('Motorkontrollleuchte', 'Engine malfunction indicator', 'Témoin de défaillance moteur'), engine))
add(telltale(C('Tachometer (LCD)', 'Speedometer (LCD)', 'Compteur de vitesse (LCD)'), speedometer))
add(telltale(C('Kraftstoff-Kontrollleuchte', 'Low fuel warning lamp', 'Témoin de réserve de carburant'), fuel_pump))
add(telltale(C('Fernlicht-Kontrollleuchte', 'Main beam indicator', 'Témoin de feux de route'), main_beam))
add(telltale(C('Kraftstoffanzeige', 'Fuel gauge', 'Jauge de carburant'), fuel_gauge))


# --- Components
def boxed(caption, x1, y0=-1.5 * P, y1=1.5 * P, names=('1', '2')):
    """A component as a rectangle from (P, y0) to (x1, y1), the terminals left and right of it on the x axis."""
    s = part(caption)
    s.rect(P, y0, x1, y1)
    terminal(s, names[0], (P, 0), (0, 0))
    terminal(s, names[1], (x1, 0), (x1 + P, 0))
    return above(s, P, x1, y0)


def three_below(caption):
    """A sensor as a rectangle 4 P wide and 3 P high, its terminals 1, 2 and 3 below it, 1 at the insertion point."""
    s = part(caption)
    s.rect(-P, -4 * P, 3 * P, -P)
    for k in range(3):
        terminal(s, str(k + 1), (k * P, -P), (k * P, 0))
    return above(s, -P, 3 * P, -4 * P)


def horn(s, x, y, length=4.6, mouth=2.4):
    """A trumpet: a thin tube widening into a bell, opening to the right."""
    s.line((x, y - 0.4), (x + length * 0.45, y - 0.4)).line((x, y + 0.4), (x + length * 0.45, y + 0.4)).line((x, y - 0.4), (x, y + 0.4))
    s.bezier((x + length * 0.45, y - 0.4), (x + length * 0.8, y - 0.5), (x + length * 0.95, y - mouth * 0.7), (x + length, y - mouth))
    s.bezier((x + length * 0.45, y + 0.4), (x + length * 0.8, y + 0.5), (x + length * 0.95, y + mouth * 0.7), (x + length, y + mouth))
    return s.line((x + length, y - mouth), (x + length, y + mouth))


def speaker(s, x, y, h=1.0, depth=1.2, flare=2.8, mouth=2.4):
    """The loudspeaker sign (a small rectangle and its cone) opening to the right."""
    return s.rect(x, y - h, x + depth, y + h).poly((x + depth, y - h), (x + depth + flare, y - mouth), (x + depth + flare, y + mouth),
                                                   (x + depth, y + h), fill=None)


def pulses(s, x, y, n=3, w=2.0, h=1.6):
    pts = [(x, y)]
    for k in range(n):
        x0 = x + w * k
        pts += [(x0 + 0.25 * w, y), (x0 + 0.25 * w, y - h), (x0 + 0.75 * w, y - h), (x0 + 0.75 * w, y)]
    return s.line(*pts, (x + n * w, y))


def motor(s, cx, cy, d, letter='M'):
    return s.circle(cx, cy, d).text(letter, cx, cy - 1.5, 3.0, 'centre')


# Flasher unit: 49 in, 49a out to the indicator lamps, 31 ground
s = part(C('Blinkgeber', 'Flasher unit', 'Centrale clignotante'))
s.rect(1.4 * P, -2 * P, 4.6 * P, 2 * P).text('G', 3 * P, -2 * P + 0.8, 2.5, 'centre')
pulses(s, 3 * P - 3.0, 2.0)
terminal(s, '49', (1.4 * P, 0), (0, 0))
terminal(s, '49a', (4.6 * P, 0), (6 * P, 0))
terminal(s, '31', (3 * P, 2 * P), (3 * P, 3 * P))
add(above(s, 1.4 * P, 4.6 * P, -2 * P))

# Auxiliary air valve: its heater and the thermal (bimetal) sign
s = boxed(C('Zusatzluftschieber', 'Auxiliary air valve', "Vanne d'air additionnel"), 4 * P)
s.line((P, 0), (P + 1.4, 0)).rect(P + 1.4, -0.6, 4 * P - 1.4, 0.6).line((4 * P - 1.4, 0), (4 * P, 0))
add(s.line((P + 1.6, -1.6), (2.5 * P, -1.6), (2.5 * P, -2.8), (4 * P - 1.6, -2.8)))

# Anti-theft alarm: a sensor and a horn
s = boxed(C('Diebstahlwarnanlage', 'Anti-theft alarm system', 'Alarme antivol'), 5 * P)
s.circle(5.4, 0, 1.2, fill='#000000').arc(5.4, 0, 3.0, 135, 225).arc(5.4, 0, 4.6, 140, 220)
add(speaker(s, 7.6, 0, h=0.9, depth=1.0, flare=3.0, mouth=2.4))

# Alternator with built-in regulator: generator, rectifier diode to B+, exciter output D+, ground 31
s = part(C('Drehstromgenerator mit Regler', 'Alternator with regulator', 'Alternateur avec régulateur'))
outline(s, 0, -2 * P, 5.5 * P, 2 * P)
s.circle(2 * P, 0, 6.0).text('G', 2 * P, -2.6, 2.6, 'centre').text('3~', 2 * P, 0.2, 2.0, 'centre')
xr = 2 * P + math.sqrt(9.0 - P * P)
s.line((xr, -P), (4 * P - 1.0, -P)).line((4 * P + 1.0, -P), (5.5 * P, -P)).line((xr, P), (5.5 * P, P)).line((2 * P, 3.0), (2 * P, 2 * P))
diode_shape(s, 4 * P - 1.0, 4 * P + 1.0, half=1.2, y=-P)
terminal(s, 'B+', (5.5 * P, -P), (7 * P, -P))
terminal(s, 'D+', (5.5 * P, P), (7 * P, P))
terminal(s, '31', (2 * P, 2 * P), (2 * P, 3 * P))
add(above(s, 0, 5.5 * P, -2 * P))

# Throttle valve switch: a make contact closed by a cam on the throttle shaft through a roller
s = part(C('Drosselklappenschalter', 'Throttle valve switch', 'Contacteur de papillon'))
pivot, tip = (0, 3 * P), (-1.9, P + 0.6)
s.line(pivot, tip)
terminal(s, '1', (0, P), (0, 0))
terminal(s, '2', pivot, (0, 4 * P))
y = 2 * P - 0.4
dashed(s, on_blade(pivot, tip, y), (-4.0, y))
s.circle(-4.9, y, 1.8).circle(-7.6, y, 3.6).circle(-8.2, y, 0.6, fill='#000000')
add(s.labels((2.6, P + 0.4), (2.6, 2 * P + 0.6)))

# Pressure control valve: the solenoid on a valve block
s = part(C('Drucksteuerventil', 'Pressure control valve', 'Valve de régulation de pression'))
coil_box(s, 2 * P, -1.2, 1.2, half=P)
terminal(s, '1', (P, 0), (0, 0))
terminal(s, '2', (3 * P, 0), (4 * P, 0))
s.line((2 * P, 1.2), (2 * P, 2.2)).rect(P, 2.2, 3 * P, 2.2 + 2 * P, w=0.35)
s.arrow((P + 1.3, 2.2 + 2 * P - 0.6), (P + 1.3, 2.8), size=1.0).arrow((3 * P - 1.3, 2.8), (3 * P - 1.3, 2.2 + 2 * P - 0.6), size=1.0)
add(above(s, P, 3 * P, -1.2))

# Electric fuel pump: the motor with the pump's triangle
s = part(C('Elektrokraftstoffpumpe', 'Electric fuel pump', 'Pompe à carburant électrique'))
s.circle(2 * P, 0, 6.0).text('M', 2 * P, -0.7, 2.6, 'centre')
triangle(s, (2 * P, -3.0), (2 * P, -1.6), 0.9)
terminal(s, '1', (2 * P - 3.0, 0), (0, 0))
terminal(s, '2', (2 * P + 3.0, 0), (4 * P, 0))
add(s.labels((2 * P, -6.6), (2 * P, 3.4), 'centre'))

# Tachograph: clock faces
s = boxed(C('Fahrtschreiber', 'Tachograph', 'Chronotachygraphe'), 5 * P, -P - 0.6, P + 0.6)
for cx in (P + 2.8, 5 * P - 2.8):
    s.circle(cx, 0, 4.0).line((cx, 0), (cx, -1.4)).line((cx, 0), (cx + 1.0, 0.6))
add(s)

add(horn(boxed(C('Fanfare', 'Air horn', 'Avertisseur à trompe'), 4 * P), P + 1.0, 0, length=5.2, mouth=2.4))

# Hall-effect sender: the Hall element and the vane rotor
s = three_below(C('Hallgeber', 'Hall-effect sender', 'Capteur à effet Hall'))
hx, hy = -1.4, -2.5 * P - 1.3
s.rect(hx, hy, hx + 3.0, hy + 2.6).line((hx, hy), (hx + 3.0, hy + 2.6)).line((hx, hy + 2.6), (hx + 3.0, hy))
for a in (10, 100, 190, 280):
    s.arc(2 * P + 0.2, -2.5 * P, 3.6, a, a + 70, w=THICK)
add(s.circle(2 * P + 0.2, -2.5 * P, 0.8, fill='#000000'))

# Inductive sender: the winding on a magnet in front of a toothed wheel
s = boxed(C('Induktionsgeber', 'Inductive sender', 'Capteur inductif'), 5 * P)
for k in range(3):
    s.arc(5.0, -1.5 + k + 0.5, 1.0, 90, 270)
s.line((5.0, -1.5), (5.0, -2.4)).line((5.0, 1.5), (5.0, 2.4)).line((5.8, -2.2), (5.8, 2.2), w=THICK)
s.circle(9.8, 0, 3.6)
for k in range(8):
    a = math.radians(22.5 + 45 * k)
    s.line((9.8 + 1.8 * math.cos(a), -1.8 * math.sin(a)), (9.8 + 2.5 * math.cos(a), -2.5 * math.sin(a)))
add(s)

# Air conditioning: switch, relay and fan
s = boxed(C('Klimaanlage', 'Air conditioning', 'Climatisation'), 7 * P)
s.line((3.6, 0.8), (4.6, 0.8)).line((4.6, 0.8), (6.8, -0.6)).line((6.6, 0.8), (7.6, 0.8))
coil_box(s, 10.2, -1.6, 1.6, half=1.2)
s.circle(14.6, 0, 4.4)
for a in (45, 135, 225, 315):
    s.poly((14.6, 0), *[(14.6 + 1.8 * math.cos(math.radians(a + d)), -1.8 * math.sin(math.radians(a + d))) for d in (-14, 14)])
add(s)

s = part(C('Lambdasonde', 'Lambda sensor', 'Sonde lambda'))
s.circle(2 * P, 0, 2 * P - 0.6).text('λ', 2 * P, -1.5, 3.0, 'centre')
terminal(s, '1', (P + 0.3, 0), (0, 0))
terminal(s, '2', (3 * P - 0.3, 0), (4 * P, 0))
add(s.labels((2 * P, -6.0), (2 * P, 3.4), 'centre'))

add(speaker(boxed(C('Lautsprecher (Kfz)', 'Loudspeaker (vehicle)', 'Haut-parleur (véhicule)'), 4 * P), 4.45, 0))

# Air mass meter: a resistor depending on the air mass and temperature, with a third terminal
s = part(C('Luftmassenmesser', 'Air mass meter', "Débitmètre massique d'air"))
s.rect(P, -1.0, 3 * P, 1.0).line((P - 0.6, 2.4), (P + 0.4, 2.4), (3 * P + 0.2, -2.4)).text('m, t°', 3 * P + 0.4, 0.6, 2.0)
terminal(s, '1', (P, 0), (0, 0))
terminal(s, '2', (3 * P, 0), (4 * P, 0))
terminal(s, '3', (2 * P, 1.0), (2 * P, 2 * P))
add(above(s, 0, 4 * P, -2.2))

# Air flow meter: a potentiometer moved by the air flow Q_L
s = part(C('Luftmengenmesser', 'Air flow meter', "Débitmètre d'air volumétrique"))
s.rect(P, -1.0, 3 * P, 1.0).arrow((2 * P, 2 * P), (2 * P, 1.0), size=1.2)
s.text('Q', 3 * P + 0.4, 0.6, 2.2).text('L', 3 * P + 2.0, 1.8, 1.5)
terminal(s, '1', (P, 0), (0, 0))
terminal(s, '2', (3 * P, 0), (4 * P, 0))
s.pin('3', (2 * P, 2 * P), label=(2 * P + 0.5, 2 * P - 2.4), shown=True)
add(above(s, 0, 4 * P, -2.2))

# Cold start valve: a solenoid on a valve
s = part(C('Kaltstartventil', 'Cold start valve', 'Injecteur de départ à froid'))
coil_box(s, 2 * P, -1.2, 1.2, half=P)
terminal(s, '1', (P, 0), (0, 0))
terminal(s, '2', (3 * P, 0), (4 * P, 0))
s.line((2 * P, 1.2), (2 * P, 3.6))
s.poly((2 * P - 2.2, 2.4), (2 * P - 2.2, 4.8), (2 * P, 3.6), fill=None).poly((2 * P + 2.2, 2.4), (2 * P + 2.2, 4.8), (2 * P, 3.6), fill=None)
add(above(s, P, 3 * P, -1.2))

# Starter with solenoid switch: the switch (box) closes 30 to the motor; its winding (50) pulls in through the motor
s = part(C('Starter mit Einrückrelais', 'Starter with solenoid switch', 'Démarreur avec contacteur à solénoïde'))
s.rect(0, 0, 4 * P, 2 * P)
pivot, tip = (P, 4.2), (P - 1.5, 1.9)
s.line((P, 0), (P, 1.4)).line(pivot, tip).line(pivot, (P, 2 * P))
coil_box(s, 3 * P, 0.8, 3.2, half=1.0)
s.line((3 * P, 0), (3 * P, 0.8)).line((3 * P, 3.2), (3 * P, 4.2), pivot).circle(P, 4.2, 0.7, fill='#000000')
dashed(s, (3 * P - 1.0, 2.6), on_blade(pivot, tip, 2.6))
motor(s, P, 3.5 * P, 2 * P - 0.6)
s.line((P, 2 * P), (P, 3.5 * P - P + 0.3))
terminal(s, '30', (P, 0), (P, -P))
terminal(s, '50', (3 * P, 0), (3 * P, -P))
terminal(s, '31', (P, 4.5 * P - 0.3), (P, 6 * P))
add(s.labels((4 * P + 1.0, 0.4), (4 * P + 1.0, 3.4)))

# Electronic control unit
s = boxed(C('Steuergerät', 'Electronic control unit', 'Calculateur électronique'), 4 * P)
s.line((4.8, 2.4), (4.8, -0.4)).line((3.4, -2.4), (4.8, -0.4), (6.2, -2.4))
add(s.arrow((6.2, 0.8), (9.0, 0.8), size=1.0))

s = boxed(C('Temperaturfühler (NTC)', 'Temperature sensor (NTC)', 'Sonde de température (CTN)'), 5 * P)
s.line((P, 0), (4.2, 0)).rect(4.2, -0.8, 8.6, 0.8).line((8.6, 0), (5 * P, 0))
add(s.line((3.6, 2.2), (4.6, 2.2), (9.0, -2.2)).text('t°', 9.4, 0.8, 2.0))

# Wiper motor: 53 and 53b the brushes for the two speeds, 53a the park switch supply, 31 ground; the wiper sweeping
s = part(C('Wischermotor', 'Wiper motor', "Moteur d'essuie-glace"))
motor(s, 2 * P, 0, 6.0)
dy = math.sqrt(9.0 - P * P)
s.line((P, -dy), (P, -2 * P)).line((2 * P, -3.0), (2 * P, -2 * P)).line((3 * P, -dy), (3 * P, -2 * P))
s.pin('53a', (P, -2 * P), label=(P - 0.5, -2 * P + 0.2), align='right', shown=True)
s.pin('53', (2 * P, -2 * P), label=(2 * P, -2 * P - 2.2), align='centre', shown=True)
s.pin('53b', (3 * P, -2 * P), label=(3 * P + 0.5, -2 * P + 0.2), shown=True)
terminal(s, '31', (2 * P, 3.0), (2 * P, 2 * P))
x, y = 6 * P, 1.6
dashed(s, (2 * P + 3.0, 0), (x - 4.0, 0))
s.line((x - 4.4, y - 4.0), (x - 3.8, y), (x + 3.8, y), (x + 4.4, y - 4.0))
s.bezier((x - 4.4, y - 4.0), (x - 1.6, y - 4.9), (x + 1.6, y - 4.9), (x + 4.4, y - 4.0))
for px in (x - 2.0, x + 1.8):
    s.line((px, y - 0.5), (px - 1.5, y - 3.2)).line((px - 0.6, y - 1.6), (px - 1.5, y - 3.2), w=0.7).circle(px, y - 0.5, 0.6, fill='#000000')
add(s.labels((-1.0, -1.4), (-1.0, 1.4), 'right'))

# Contact breaker: a break contact opened by the cam of the distributor shaft
s = part(C('Zündunterbrecher', 'Contact breaker', 'Rupteur'))
pivot, tip = (P, 0), (3 * P + 0.4, 2.1)
s.line((3 * P, 0), (3 * P, 1.6)).line(pivot, tip)
terminal(s, '1', pivot, (0, 0))
terminal(s, '2', (3 * P, 0), (4 * P, 0))
dashed(s, (2 * P, -2.8), (2 * P, tip[1] * P / (tip[0] - P)))
s.rect(2 * P - 1.4, -5.6, 2 * P + 1.4, -2.8, corner=40).circle(2 * P, -4.2, 0.5, fill='#000000')
add(s.labels((2 * P, 2.6), (2 * P, 5.2), 'centre'))
