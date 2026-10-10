# The page "USER/Diverse": a mixed collection, many of them older forms (DIN 40700 era) beside the forms of
# DIN EN 60617 (bridge rectifier, spark gap 07-22-01, gas filling as a dot after 60617-5, the fuse with a marked end
# 07-21-02), the warning sign in the colours of ISO 7010 W001 (yellow #F9A800 after ISO 3864-4), the DIN 41529
# loudspeaker connector (round contact +, flat contact −), a TO-220 transistor after the onsemi data sheet TIP31A/D
# (1 B, 2 C, 3 E), the nine-segment display with the segments a to i, and the tuning indicators (EM34, EM80, EM83, EM84)
# as their screens look. The DIN 40712 (1958) forms are drawn as the booklet "Vom Schaltzeichen zum
# Empfängerschaltbild" (Der praktische Funkamateur 10, 1959) shows them; the IEC 617 (1983) forms as reprinted in
# IS 12032. Parts carry contacts where they are wired; signs (arrows, connection marks, pictures without connections)
# are not counted in the parts list.
USR = 'USER'
add = page(USR, 'Diverse', 'Miscellaneous', 'Divers')
MINUS = '−'
GREEN, SHADE, GLASS = '#3ed16a', '#143c25', '#e0e6e3'


def part(prefix, caption, fixed=False):
    """A part: numbered when designator letters are given, a fixed designator ("Zähler", "Fs") kept as it is."""
    return Symbol(prefix, caption, numbered=bool(prefix) and not fixed)


def sign(caption):
    return Symbol('', caption, numbered=False, listed=False, shown=False)


def ends(s, length=4 * P, names=('1', '2'), texts=(None, None)):
    """Contacts at both ends of a part lying on the x axis; the texts placed as on two_pin()."""
    s.pin(names[0], (0, 0), text=texts[0]).pin(names[1], (length, 0), label=(length - 0.4, -2.2), align='right', text=texts[1])
    return s.labels((length / 2, -5.6), (length / 2, 2.8), 'centre')


def oval(s, cx, cy, w, h, pen=W, fill=None, style=None):
    o = {'type': 'ellipse', 'centre': pt((cx, cy)), 'size': [r(w), r(h)], 'pen': {'width': pen}}
    if style:
        o['pen']['style'] = style
    if fill:
        o['fill'] = {'style': 'solid', 'color': fill}
    s.parts.append(o)
    return s


def pie(s, cx, cy, d, start, stop, fill, pen=0.05):
    """A filled circular sector, the angles counter-clockwise from the x axis."""
    s.parts.append({'type': 'ellipse', 'centre': pt((cx, cy)), 'size': [r(d), r(d)], 'pen': {'width': pen},
                    'start': r(start), 'stop': r(stop), 'arc': 'pie', 'fill': {'style': 'solid', 'color': fill}})
    return s


def dots(s, x0, x1, y=0, step=0.9, d=0.45):
    n = int(round((x1 - x0) / step))
    for k in range(n + 1):
        s.circle(x0 + (x1 - x0) * k / n, y, d, w=0.05, fill='#000000')
    return s


def diode_on(s, a, b, length=2.8, half=1.6, fill=None):
    """A diode on the line from a to b, in its middle, conducting from a to b."""
    dx, dy = b[0] - a[0], b[1] - a[1]
    n = math.hypot(dx, dy)
    ux, uy, nx, ny = dx / n, dy / n, -dy / n, dx / n
    mx, my = (a[0] + b[0]) / 2, (a[1] + b[1]) / 2
    base, tip = (mx - ux * length / 2, my - uy * length / 2), (mx + ux * length / 2, my + uy * length / 2)
    s.line(a, base).line(tip, b)
    s.poly((base[0] + nx * half, base[1] + ny * half), (base[0] - nx * half, base[1] - ny * half), tip, fill=fill)
    return s.line((tip[0] + nx * half, tip[1] + ny * half), (tip[0] - nx * half, tip[1] - ny * half), w=0.35)


def zig(s, x0, x1, peaks, h, y=0, w=W, f=None):
    """A zigzag line from x0 to x1 along y (or along the axis that f maps u, v to)."""
    f = f or (lambda u, v: (u, y + v))
    d = (x1 - x0) / peaks
    pts = [(x0, 0)] + [(x0 + d * (k + 0.5), -h if k % 2 == 0 else h) for k in range(peaks)] + [(x1, 0)]
    return s.line(*[f(u, v) for u, v in pts], w=w)


def helix(s, x0, x1, turns, b, f=None, steps=24):
    """A wound wire seen from the side (a prolate trochoid): loops of height 2 b from x0 to x1."""
    f = f or (lambda u, v: (u, v))
    a = (x1 - x0) / (2 * math.pi * turns)
    ts = [k * 2 * math.pi / steps for k in range(steps * turns + 1)]
    return s.line(*[f(x0 + a * t - b * math.sin(t), -b * (1 - math.cos(t))) for t in ts])


def meander(s, x0, x1, n, h, y=0):
    d = (x1 - x0) / n
    pts = [(x0, y)]
    for k in range(n):
        yy = y - h if k % 2 == 0 else y + h
        pts += [(x0 + d * k, yy), (x0 + d * (k + 1), yy)]
    return s.line(*pts, (x1, y))


def leads(s, x0, x1, length=4 * P):
    return s.line((0, 0), (x0, 0)).line((x1, 0), (length, 0))


# --- Batteries, antenna, counter, oscilloscope
s = part('Bat', C('Batterie, mehrzellig', 'Battery, several cells', 'Batterie de plusieurs éléments'))
L = 6 * P
s.line((0, 0), (P, 0)).line((P, -2.6), (P, 2.6)).line((P + 1.0, -1.3), (P + 1.0, 1.3), w=0.7)
dots(s, P + 1.9, L - P - 1.9)
s.line((L - P - 1.0, -2.6), (L - P - 1.0, 2.6)).line((L - P, -1.3), (L - P, 1.3), w=0.7).line((L - P, 0), (L, 0))
s.text('+', P - 1.2, -3.4, 2.2, 'centre')
add(ends(s, L, ('+', '-'), (None, MINUS)).labels((L / 2, -6.0), (L / 2, 2.8), 'centre'))

s = part('Ant', C('Schleifendipol', 'Folded dipole', 'Dipôle replié'))
y, h, half = -2 * P, 3.0, 3 * P
s.line((-half, y - h), (half, y - h)).line((-half, y), (-P, y)).line((P, y), (half, y))
s.arc(-half, y - h / 2, h, 90, 270).arc(half, y - h / 2, h, 270, 450)
s.line((-P, y), (-P, 0)).line((P, y), (P, 0))
add(s.pin('1', (-P, 0)).pin('2', (P, 0), label=(P + 0.4, -2.2)).labels((half + 2.4, y - h - 1.0), (half + 2.4, y - h + 2.0)))

s = part('Zähler', C('Zähler (Anzeige)', 'Counter (display)', 'Compteur (affichage)'), fixed=True)
s.rect(0, -2.6, 12.6, 2.6, w=0.35)
for k in range(3):
    x = 1.0 + k * 3.8
    s.rect(x, -2.0, x + 3.0, 2.0, w=0.18).text('0', x + 1.5, -1.5, 3.0, 'centre')
add(s.labels((6.3, -6.0), (6.3, 3.2), 'centre'))

s = sign(C('Oszilloskop (Bildschirm)', 'Oscilloscope (screen)', 'Oscilloscope (écran)'))
s.rect(-4 * P, -3 * P, 4 * P, 3 * P, w=0.35, corner=15)
for k in (-1, 1):       # Y plates above and below the beam, X plates left and right of it
    s.line((-1.8, k * 3.2), (1.8, k * 3.2), w=THICK).line((k * 3.2, -1.8), (k * 3.2, 1.8), w=THICK)
add(s.circle(0, 0, 0.8, w=0.05, fill='#000000'))

# --- Rectifiers and a diagonal diode
s = part('Gr', C('Graetzgleichrichter', 'Bridge rectifier (Graetz)', 'Pont de Graetz'))
corner = {'left': (0, 0), 'top': (3 * P, -3 * P), 'right': (6 * P, 0), 'bottom': (3 * P, 3 * P)}
for a, b in (('bottom', 'left'), ('bottom', 'right'), ('left', 'top'), ('right', 'top')):
    diode_on(s, corner[a], corner[b], 3.0, 1.7)
s.pin('~1', corner['left']).pin('~2', corner['right'], label=(6 * P - 0.4, -2.2), align='right')
s.pin('+', corner['top'], label=(3 * P + 0.6, -3 * P)).pin('-', corner['bottom'], label=(3 * P + 0.6, 3 * P - 2.2), text=MINUS)
s.text('~', -0.4, -3.4, 2.8, 'right').text('~', 6 * P + 0.4, -3.4, 2.8).text('+', 3 * P + 1.2, -3 * P - 1.6, 2.2).text(MINUS, 3 * P + 1.2, 3 * P - 0.6, 2.2)
add(s.labels((6 * P + 0.8, -3 * P), (6 * P + 0.8, -3 * P + 3.0)))

s = part('Gr', C('Diode, 45°', 'Diode, 45°', 'Diode à 45°'))
diode_on(s, (0, 0), (3 * P, -3 * P))
add(s.pin('1', (0, 0)).pin('2', (3 * P, -3 * P), label=(3 * P + 0.4, -3 * P)).labels((6.6, -3.2), (6.6, -0.2)))

s = part('', C('Graetzgleichrichter (Raute)', 'Bridge rectifier (diamond)', 'Pont de Graetz (losange)'))
s.poly((0.5 * P, 0), (2 * P, -1.5 * P), (3.5 * P, 0), (2 * P, 1.5 * P), fill=None, w=0.35)
s.line((0, 0), (0.5 * P, 0)).line((3.5 * P, 0), (4 * P, 0)).line((2 * P, -2 * P), (2 * P, -1.5 * P)).line((2 * P, 2 * P), (2 * P, 1.5 * P))
s.poly((2 * P - 1.2, 1.1), (2 * P + 1.2, 1.1), (2 * P, -0.9), fill=None).line((2 * P - 1.2, -0.9), (2 * P + 1.2, -0.9), w=0.35)
s.line((2 * P, 1.1), (2 * P, 1.9)).line((2 * P, -0.9), (2 * P, -1.7))
s.pin('~1', (0, 0)).pin('~2', (4 * P, 0), label=(4 * P - 0.4, -2.2), align='right')
s.pin('+', (2 * P, -2 * P), label=(2 * P + 0.6, -2 * P)).pin('-', (2 * P, 2 * P), label=(2 * P + 0.6, 2 * P - 2.2), text=MINUS)
s.text('~', 0.1, -2.9, 2.6).text('~', 4 * P - 0.1, -2.9, 2.6, 'right').text('+', 2 * P + 0.6, -5.9, 2.0).text(MINUS, 2 * P + 0.6, 3.6, 2.0)
add(s.labels((4 * P + 0.8, -2 * P), (4 * P + 0.8, -2 * P + 3.0)))


# --- Loudspeakers (DIN EN 60617-9: the drive's box and the cone)
def speaker(caption, magnet=False):
    s = part('Ls', caption)
    s.pin('1', (0, 0)).pin('2', (0, 2 * P), label=(0.4, 2 * P - 2.2))
    s.line((0, 0), (P, 0)).line((0, 2 * P), (P, 2 * P)).rect(P, -0.6, P + 2.0, 2 * P + 0.6)
    x = P + 2.0
    if magnet:
        s.rect(x, -1.4, x + 0.9, 2 * P + 1.4, fill='#000000')
        x += 0.9
    s.poly((x, -0.6), (x + 3.0, -3.0), (x + 3.0, 2 * P + 3.0), (x, 2 * P + 0.6), fill=None)
    return s.labels((x + 4.0, -1.0), (x + 4.0, 2.0))


add(speaker(C('Lautsprecher 1', 'Loudspeaker 1', 'Haut-parleur 1')))
add(speaker(C('Lautsprecher 2', 'Loudspeaker 2', 'Haut-parleur 2'), magnet=True))

# --- TO-220 from the front (onsemi TIP31A/D: 1 base, 2 collector and tab, 3 emitter)
s = part('', C('Transistor TO-220 (Bauform)', 'Transistor TO-220 (package)', 'Transistor TO-220 (boîtier)'))
s.rect(0, -6 * P, 4 * P, -3 * P, fill='#c8c8c8').circle(2 * P, -4.8 * P, 3.2, fill='#ffffff')
s.rect(0, -3 * P, 4 * P, 0, w=0.35, fill='#404040')
for x, name in ((P, 'B'), (2 * P, 'C'), (3 * P, 'E')):
    coloured(s.line((x, 0), (x, 2 * P), w=0.6), '#808080')
    s.pin(name, (x, 2 * P), label=(x, 2 * P + 0.4), align='centre', shown=True)
add(s.labels((4 * P + 1.0, -3 * P), (4 * P + 1.0, -3 * P + 3.0)))

# --- Capacitors
# Differential capacitor (DIN 40712 1958, IEC 617-4 04-02-11): the two stator plates (1, 2) side by side opposite the
# common rotor plate (3), one arrow across.
s = part('C', C('Differentialdrehkondensator', 'Differential variable capacitor', 'Condensateur variable différentiel'))
x = 2 * P
for y in (0, 2 * P):
    s.line((0, y), (x - 0.5, y)).line((x - 0.5, y - 1.3), (x - 0.5, y + 1.3), w=THICK)
s.line((x + 0.5, -1.3), (x + 0.5, 2 * P + 1.3), w=THICK).line((x + 0.5, P), (4 * P, P))
s.arrow((x - 1.6, 2 * P + 2.4), (x + 3.0, -1.4), size=1.2)
s.pin('1', (0, 0)).pin('2', (0, 2 * P), label=(0.4, 2 * P - 2.2)).pin('3', (4 * P, P), label=(4 * P - 0.4, P - 2.2), align='right')
add(s.labels((4 * P + 0.8, -2.6), (4 * P + 0.8, P + 0.6)))


def outer_foil(s, heavy=False):
    """A capacitor whose outer foil (2) is marked by the plate with bent ends (DIN 40712 1958; IEC 617-4 04-02-02: the
    curved element is the outside electrode); heavy: both plates as heavy bars, as the 1958 drawings show them."""
    w, gap = (1.0, 0.8) if heavy else (THICK, 0.5)
    x = 2 * P
    s.line((0, 0), (x - gap, 0)).line((x - gap, -2.2), (x - gap, 2.2), w=w)
    s.line((x + gap + 0.9, -2.9), (x + gap, -1.8), (x + gap, 1.8), (x + gap + 0.9, 2.9), w=w).line((x + gap, 0), (4 * P, 0))
    return ends(s)


add(outer_foil(part('C', C('Kondensator mit Belag', 'Capacitor with marked outer foil', 'Condensateur à armature extérieure repérée'))))

# --- Resistors in a glass bulb (iron-hydrogen, Urdox)
s = part('EW', C('Eisenwiderstand', 'Iron-hydrogen resistor (barretter)', 'Résistance fer-hydrogène'))
oval(s, 2 * P, 0, 3.4 * P, 4.8)
add(ends(zig(leads(s, 1.3 * P, 2.7 * P), 1.3 * P, 2.7 * P, 8, 1.0)))

# --- Gas-filled tubes and lamps
# Glow lamp / stabiliser after DIN 40700 as in the 1959 booklet (5.1): two equal electrodes facing each other in the
# bulb, the gas filling a dot anywhere inside.
s = part('Gl', C('Glimmröhre', 'Glow-discharge tube (voltage stabiliser)', 'Tube à décharge luminescente (stabilisateur)'))
s.circle(0, 2 * P, 3 * P, w=0.35)
for y, end in ((2 * P - 1.0, 0), (2 * P + 1.0, 4 * P)):
    s.line((0, end), (0, y)).line((-1.6, y), (1.6, y), w=THICK)
s.circle(2.2, 2 * P + 1.5, 0.7, w=0.05, fill='#000000')
add(s.pin('1', (0, 0), label=(0.6, 0.2)).pin('2', (0, 4 * P), label=(0.6, 4 * P - 2.4)).labels((4.6, P - 0.4), (4.6, 2 * P + 0.4)))

s = part('La', C('Soffitte', 'Festoon lamp', 'Lampe navette'))
L = 6 * P
leads(s, P, 5 * P, L).rect(P, -1.5, P + 1.6, 1.5, fill='#c0c0c0').rect(5 * P - 1.6, -1.5, 5 * P, 1.5, fill='#c0c0c0')
s.rect(P + 1.6, -1.8, 5 * P - 1.6, 1.8).line((P + 1.6, 0), (P + 2.6, 0)).line((5 * P - 2.6, 0), (5 * P - 1.6, 0))
helix(s, P + 2.6, 5 * P - 2.6, 7, 0.6)
add(ends(s, L).labels((L / 2, -5.4), (L / 2, 2.6), 'centre'))


# --- Tuning indicators as their screens look
def bulb(s, half_w, half_h=13.0):
    return s.rect(-half_w, -half_h, half_w, half_h, w=0.25, fill=GLASS, corner=45)


s = sign(C('Magisches Auge', 'Magic eye', 'Œil magique'))
s.circle(0, 0, 16.0, w=0.25, fill=GLASS).circle(0, 0, 13.6, w=0.15, fill=GREEN)
pie(s, 0, 0, 13.6, 70, 110, SHADE)       # the two shadows of different sensitivity (EM34)
pie(s, 0, 0, 13.6, 225, 315, SHADE)
add(s.circle(0, 0, 5.0, w=0.2, fill='#101010'))

s = sign(C('Magische Waage', 'Magic balance', 'Balance magique'))
bulb(s, 6.0)
for x0, top in ((-4.0, -3.0), (0.8, 3.0)):      # two luminous bands side by side, their lengths compared (EM83)
    s.rect(x0, -10.0, x0 + 3.2, 10.0, w=0.1, fill=SHADE).rect(x0 + 0.3, top, x0 + 2.9, 9.7, w=0.05, fill=GREEN)
add(s)

s = sign(C('Magischer Fächer', 'Magic fan', 'Éventail magique'))
bulb(s, 7.0, 12.0)
pie(s, 0, 3.0, 12.0, 0, 180, GREEN)            # the half-round fan (EM80), lit in the middle
pie(s, 0, 3.0, 12.0, 0, 38, SHADE)
pie(s, 0, 3.0, 12.0, 142, 180, SHADE)
add(s.rect(-1.6, 3.0, 1.6, 4.2, w=0.1, fill='#101010'))

s = sign(C('Magischer Balken', 'Magic bar', 'Barre magique'))
bulb(s, 4.6)
s.rect(-1.6, -10.0, 1.6, 10.0, w=0.1, fill=SHADE)        # the band (EM84), dark in the middle on a weak signal
add(s.rect(-1.3, -9.7, 1.3, -2.4, w=0.05, fill=GREEN).rect(-1.3, 2.4, 1.3, 9.7, w=0.05, fill=GREEN))

# --- Fuses and spark gap
s = part('Si', C('Sicherung (Glasrohr)', 'Fuse (glass tube)', 'Fusible (tube de verre)'))
leads(s, P, 3 * P).rect(P + 1.0, -1.0, 3 * P - 1.0, 1.0).line((P + 1.0, 0), (3 * P - 1.0, 0))
add(ends(s.rect(P, -1.4, P + 1.0, 1.4, fill='#a0a0a0').rect(3 * P - 1.0, -1.4, 3 * P, 1.4, fill='#a0a0a0')))

# Drawn half filled as given for this caption; in DIN EN 60617-7 (07-21-02) a filled end marks the supply side instead.
s = part('Si', C('Sicherung (Thermosicherung)', 'Fuse (thermal fuse)', 'Fusible (fusible thermique)'))
add(ends(s.line((0, 0), (4 * P, 0)).rect(P, -1.0, 3 * P, 1.0).rect(2 * P, -1.0, 3 * P, 1.0, fill='#000000')))

s = part('Fs', C('Funkenstrecke', 'Spark gap', 'Éclateur'), fixed=True)
leads(s, 2 * P - 2.6, 2 * P + 2.6)
s.poly((2 * P - 2.6, -1.0), (2 * P - 2.6, 1.0), (2 * P - 0.5, 0)).poly((2 * P + 2.6, -1.0), (2 * P + 2.6, 1.0), (2 * P + 0.5, 0))
add(ends(s).labels((2 * P, -4.6), (2 * P, 2.0), 'centre'))

# --- Signs: arrows and connection marks
add(sign(C('Pfeil 45°', 'Arrow, 45°', 'Flèche à 45°')).arrow((0, 0), (4.24, -4.24), size=1.6))
s = sign(C('Anschlusspunkt (Abzweig)', 'Connection point (branch)', 'Point de connexion (dérivation)'))
s.line((0, 0), (P, 0)).line((P, 0), (2 * P, -P)).line((P, 0), (2 * P, P)).circle(P, 0, 0.9, w=0.05, fill='#000000')
add(s.pin('1', (0, 0)))
s = sign(C('Anschlusspunkt (Gabel)', 'Connection point (fork)', 'Point de connexion (fourche)'))
s.line((0, 0), (P, 0)).arc(P + 1.0, 0, 2.0, 90, 270).line((P + 1.0, -1.0), (2.5 * P, -1.0)).line((P + 1.0, 1.0), (2.5 * P, 1.0))
add(s.pin('1', (0, 0)))
add(sign(C('Pfeil kurz', 'Arrow, short', 'Flèche courte')).arrow((0, 0), (3.0, 0), size=1.2))

# --- Old coils
s = part('L', C('Spule (alt, 5 Windungen)', 'Inductor (old, 5 turns)', 'Bobine (ancienne, 5 spires)'))
add(ends(helix(leads(s, P, 3 * P), P, 3 * P, 5, 1.3)))
s = part('L', C('Spule (alt, Zickzack)', 'Inductor (old, zigzag)', 'Bobine (ancienne, zigzag)'))
add(ends(zig(leads(s, P, 3 * P), P, 3 * P, 12, 1.6)))


# --- DIN 41529 loudspeaker connector, mating face: the round contact (1, +) and the flat one (2, −)
def din_ls(prefix, caption, plug):
    s = part(prefix, caption)
    cy = -6.0
    s.rect(-3.0, cy - 4.0, 2 * P + 3.0, cy + 4.0, w=0.35, corner=45)
    if plug:
        s.circle(0, cy, 2.0, w=0.1, fill='#000000').rect(2 * P - 1.6, cy - 0.5, 2 * P + 1.6, cy + 0.5, w=0.1, fill='#000000')
        s.line((0, cy + 1.0), (0, 0)).line((2 * P, cy + 0.5), (2 * P, 0))
    else:
        s.circle(0, cy, 2.4).rect(2 * P - 1.8, cy - 0.6, 2 * P + 1.8, cy + 0.6)
        s.line((0, cy + 1.2), (0, 0)).line((2 * P, cy + 0.6), (2 * P, 0))
    s.text('+', 0, cy - 3.6, 2.0, 'centre').text(MINUS, 2 * P, cy - 3.6, 2.0, 'centre')
    s.pin('1', (0, 0), label=(-0.4, -2.2), align='right').pin('2', (2 * P, 0), label=(2 * P + 0.4, -2.2))
    return s.labels((2 * P + 4.0, cy - 4.0), (2 * P + 4.0, cy - 1.0))


add(din_ls('Bu', C('Lautsprecherbuchse', 'Loudspeaker socket (DIN)', 'Prise haut-parleur (DIN)'), False))
add(din_ls('St', C('Lautsprecherstecker', 'Loudspeaker plug (DIN)', 'Fiche haut-parleur (DIN)'), True))

s = part('Bu', C('Buchse mit Schirmung', 'Socket with screen', 'Prise blindée'))
s.line((0, 0), (2 * P - 1.6, 0)).arc(2 * P, 0, 3.2, 90, 270, w=0.35)
oval(s, 2 * P, 0, 7.0, 7.0, style='dash').line((2 * P, 3.5), (2 * P, 2 * P))
add(s.pin('1', (0, 0)).pin('2', (2 * P, 2 * P), label=(2 * P + 0.5, 2 * P - 2.2)).labels((2 * P + 4.2, -3.6), (2 * P + 4.2, -0.6)))


# --- Old transformers: two windings facing each other, 1 and 2 at the left, 3 and 4 at the right
def windings(caption, draw):
    s = part('', caption)
    s.line((0, 0), (P, 0), (P, P)).line((P, 3 * P), (P, 4 * P), (0, 4 * P))
    s.line((4 * P, 0), (3 * P, 0), (3 * P, P)).line((3 * P, 3 * P), (3 * P, 4 * P), (4 * P, 4 * P))
    draw(s, lambda u, v: (P - v, u), lambda u, v: (3 * P + v, u))
    s.pin('1', (0, 0)).pin('2', (0, 4 * P), label=(0.4, 4 * P - 2.2))
    s.pin('3', (4 * P, 0), label=(4 * P - 0.4, -2.2), align='right').pin('4', (4 * P, 4 * P), label=(4 * P - 0.4, 4 * P - 2.2), align='right')
    return s.labels((4 * P + 0.8, P), (4 * P + 0.8, 2 * P + 0.4))


add(windings(C('Transformator (alt, Wendel)', 'Transformer (old, helix)', 'Transformateur (ancien, hélice)'),
             lambda s, left, right: (helix(s, P, 3 * P, 4, 1.0, left), helix(s, P, 3 * P, 4, 1.0, right))))
add(windings(C('Transformator (alt, Zickzack)', 'Transformer (old, zigzag)', 'Transformateur (ancien, zigzag)'),
             lambda s, left, right: (zig(s, P, 3 * P, 8, 1.0, f=left), zig(s, P, 3 * P, 8, 1.0, f=right))))

# --- Resistors
s = part('U', C('Urdox-Widerstand', 'Urdox resistor', 'Résistance Urdox'))
oval(s, 2 * P, 0, 3.4 * P, 4.8)
add(ends(leads(s, P + 0.6, 3 * P - 0.6).rect(P + 0.6, -0.9, 3 * P - 0.6, 0.9)))
s = part('EU', C('Eisen-Urdox-Widerstand', 'Iron-hydrogen resistor with Urdox', 'Résistance fer-hydrogène avec Urdox'))
L = 6 * P
oval(s, 3 * P, 0, 5.2 * P, 4.8)
leads(s, 1.2 * P, 4.8 * P, L).line((3.0 * P, 0), (3.4 * P, 0)).rect(3.4 * P, -0.9, 4.8 * P, 0.9)
add(ends(zig(s, 1.2 * P, 3.0 * P, 8, 1.0), L))
s = part('R', C('Drahtwiderstand, lang', 'Wirewound resistor, long', 'Résistance bobinée, longue'))
L = 8 * P
leads(s, P, 7 * P, L).rect(P, -1.4, 7 * P, 1.4)
for k in range(18):
    x = P + 0.6 + k * 0.78
    s.line((x, 1.4), (x + 0.6, -1.4), w=0.18)
add(ends(s, L))
add(ends(zig(leads(part('R', C('Widerstand (Zickzack)', 'Resistor (zigzag)', 'Résistance (zigzag)')), P, 3 * P), P, 3 * P, 6, 1.0)))

# --- Warning sign (ISO 7010 W001: yellow, black band and exclamation mark) and nixie tube
s = sign(C('Achtung (Warnschild)', 'Caution (warning sign)', 'Attention (panneau d’avertissement)'))
outer = [(0, -7.5), (6.5, 3.76), (-6.5, 3.76)]
s.poly(*outer, fill='#000000', w=0.1).poly(*[(0.78 * x, 0.78 * y) for x, y in outer], fill='#f9a800', w=0.05)
add(s.poly((-0.55, -3.2), (0.55, -3.2), (0.3, 0.9), (-0.3, 0.9), w=0.05).circle(0, 2.0, 1.1, w=0.05, fill='#000000'))

s = sign(C('Ziffernanzeigeröhre', 'Nixie tube', 'Tube Nixie'))
s.rect(-5.0, -11.0, 5.0, 9.0, w=0.35, fill=GLASS, corner=45).rect(-3.6, -8.6, 3.6, 6.6, w=0.1, fill='#2b2b2b')
for k in range(6):
    coloured(s.line((-3.6, -7.0 + k * 2.6), (3.6, -7.0 + k * 2.6), w=0.08), '#707070')
s.text('5', 0, -8.2, 11.0, 'centre', bold=True)
s.parts[-1]['font']['color'] = '#ff8a24'
for x in (-3.0, -1.5, 0, 1.5, 3.0):
    coloured(s.line((x, 9.0), (x, 11.0), w=0.4), '#808080')
add(s)

# --- More capacitors and coils
add(outer_foil(part('C', C('Kondensator mit Belag (dick)', 'Capacitor with marked outer foil (heavy)',
                           'Condensateur à armature extérieure repérée (épaisse)')), heavy=True))
s = part('L', C('Feldspule', 'Field coil', 'Bobine d’excitation'))
add(ends(leads(s, P, 3 * P).rect(P, -0.9, 3 * P, 0.9, fill='#000000').line((P, -2.2), (3 * P, -2.2), w=0.35)))

s = part('P', C('Widerstand, einstellbar (DIN)', 'Preset resistor (DIN)', 'Résistance ajustable (DIN)'))
resistor_body(s).line((2 * P - 1.2, -1.0), (2 * P - 1.2, 1.0)).line((2 * P - 0.5, -1.0), (2 * P - 0.5, 1.0))
add(ends(s.line((P + 1.6, 2.4), (3 * P + 0.6, -2.4), end='bar', size=1.6)))

s = part('R', C('Potentiometer (Schleifer seitlich)', 'Potentiometer (wiper at the side)', 'Potentiomètre (curseur latéral)'))
s.line((0, 0), (0, P)).rect(-1.0, P, 1.0, 3 * P).line((0, 3 * P), (0, 4 * P)).line((2 * P, 2 * P), (1.1, 2 * P), end='arrow', size=1.2)
s.pin('1', (0, 0), label=(0.6, 0.2)).pin('2', (0, 4 * P), label=(0.6, 4 * P - 2.4)).pin('3', (2 * P, 2 * P), label=(2 * P - 0.4, 2 * P - 2.2), align='right')
add(s.labels((-1.8, P - 0.2), (-1.8, 2 * P + 0.6), 'right'))

s = capacitor(part('C', C('Elektrolytkondensator, gepolt', 'Electrolytic capacitor, polarised', 'Condensateur électrolytique polarisé')), polar=True)
add(ends(s.text(MINUS, 2 * P + 2.6, -3.2, 2.2, 'centre'), names=('+', '-'), texts=(None, MINUS)))

# Jack socket with a break contact, old form: tip spring with its hook (2), the switch spring resting on it (3), the
# sleeve as a heavy bar (1); the plug enters from the right.
s = part('Bu', C('Schaltbuchse', 'Jack socket with switch contact', 'Jack avec contact de commutation'))
s.line((0, 0), (3.6 * P, 0), (4.0 * P, 0.6 * P), (4.4 * P, 0))
s.line((0, -P), (2.6 * P, -P), (2.6 * P, -0.5)).poly((2.6 * P - 0.5, -0.5), (2.6 * P + 0.5, -0.5), (2.6 * P, 0))
s.line((0, 2 * P), (P, 2 * P), (P, 1.3 * P)).line((P, 1.3 * P), (4.4 * P, 1.3 * P), w=THICK)
s.pin('3', (0, -P), label=(0.4, -P - 2.2)).pin('2', (0, 0), label=(0.4, -2.2)).pin('1', (0, 2 * P), label=(0.4, 2 * P - 2.2))
add(s.labels((P, -P - 3.6), (P, 2 * P + 0.6)))



def band(s, cx, rad, y0, y1, n=12):
    """The part of the disc about (cx, 0) between the heights y0 and y1, filled black."""
    a0, a1 = math.asin(y0 / rad), math.asin(y1 / rad)
    right = [(cx + rad * math.cos(a0 + (a1 - a0) * k / n), rad * math.sin(a0 + (a1 - a0) * k / n)) for k in range(n + 1)]
    return s.poly(*right, *[(2 * cx - x, y) for x, y in reversed(right)], w=0.05)


# Electromechanical indicator (IEC 617-8 08-10-03): the disc with black caps above and below and a black band across
s = part('Sz', C('Schauzeichen', 'Electromechanical indicator', 'Indicateur électromécanique'), fixed=True)
leads(s, 2 * P - 2.5, 2 * P + 2.5)
for y0, y1 in ((-2.5, -1.3), (-0.5, 0.5), (1.3, 2.5)):
    band(s, 2 * P, 2.5, y0, y1)
add(ends(s.circle(2 * P, 0, 5.0)))
s = sign(C('Messpunkt', 'Test point', 'Point de mesure'))
add(s.line((0, 0), (P - 1.3, 0)).poly((P - 1.3, 0), (P, -1.3), (P + 1.3, 0), (P, 1.3), fill=None).pin('1', (0, 0)))
s = part('D', C('Gleichrichter (Kasten)', 'Rectifier (box)', 'Redresseur (boîte)'))
leads(s, P, 3 * P).rect(P, -P, 3 * P, P).poly((2 * P - 0.9, -1.1), (2 * P - 0.9, 1.1), (2 * P + 0.8, 0), fill=None)
s.line((2 * P + 0.8, -1.1), (2 * P + 0.8, 1.1), w=0.35).line((2 * P - 1.8, 0), (2 * P - 0.9, 0)).line((2 * P + 0.8, 0), (2 * P + 1.7, 0))
add(ends(s).labels((2 * P, -6.0), (2 * P, 3.2), 'centre'))


# --- Tape heads: the ring core with its gap above the tape and the winding on its back; the box forms with the
# erasing cross or the double arrow of recording and playback.
def head(caption, gap):
    s = part('Kopf', caption, fixed=True)
    y0, y1 = 1.8, 5.0
    s.line((2 * P - gap / 2, y1), (P, y1), (P, y0), (3 * P, y0), (3 * P, y1), (2 * P + gap / 2, y1), w=THICK)
    s.line((P, 0), (P, 0.8)).line((3 * P, 0), (3 * P, 0.8))
    s.line(*[(P + k * P / 4, 0.8 if k % 2 == 0 else 2.8) for k in range(9)])
    s.line((P - 1.6, y1 + 0.8), (3 * P + 1.6, y1 + 0.8), w=0.35)
    s.pin('1', (P, 0), label=(P - 0.4, -2.2), align='right').pin('2', (3 * P, 0), label=(3 * P + 0.4, -2.2))
    return s.labels((3 * P + 1.6, 0.4), (3 * P + 1.6, 3.4))


add(head(C('Kombikopf', 'Record/playback head', 'Tête d’enregistrement/lecture'), 0.4))
add(head(C('Löschkopf', 'Erase head', 'Tête d’effacement'), 2.4))
s = part('Si', C('Sicherung (Rohr)', 'Fuse (tube)', 'Fusible (tube)'))
add(ends(s.line((0, 0), (4 * P, 0)).rect(P, -1.1, 3 * P, 1.1, corner=50)))

s = part('Tr', C('Transformator (Kreise)', 'Transformer (circles)', 'Transformateur (cercles)'), fixed=True)
for cx, x0, x1 in ((3 * P - 2.45, 0, 2.76), (3 * P + 2.45, 6 * P - 2.76, 6 * P)):
    s.circle(cx, P, 7.0).line((x0, 0), (x1, 0)).line((x0, 2 * P), (x1, 2 * P))
s.pin('1', (0, 0)).pin('2', (0, 2 * P), label=(0.4, 2 * P - 2.2))
s.pin('3', (6 * P, 0), label=(6 * P - 0.4, -2.2), align='right').pin('4', (6 * P, 2 * P), label=(6 * P - 0.4, 2 * P - 2.2), align='right')
add(s.labels((3 * P, -4.8), (3 * P, 2 * P + 1.4), 'centre'))

# Toggle switch from above: housing, hexagon nut, bushing and the lever thrown upwards; the three lugs (2 the common
# one) lead out below.
s = part('', C('Kippschalter (Bauform)', 'Toggle switch (package)', 'Interrupteur à levier (boîtier)'))
s.rect(-1.8, -8.0, 2 * P + 1.8, -1.0, w=0.35, fill='#e0e0e0')
s.poly(*[(P + 3.0 * math.cos(math.radians(a)), -4.5 + 3.0 * math.sin(math.radians(a))) for a in range(0, 360, 60)], fill='#c0c0c0')
s.circle(P, -4.5, 3.4, fill='#a8a8a8').rect(P - 0.7, -7.6, P + 0.7, -4.5, w=0.2, fill='#606060', corner=50)
for k, x in enumerate((0, P, 2 * P)):
    s.rect(x - 0.6, -1.0, x + 0.6, 0.6, w=0.15, fill='#d0d0d0').line((x, 0.6), (x, P))
    s.pin(str(k + 1), (x, P), label=(x, P + 0.3), align='centre', shown=True)
add(s.labels((2 * P + 2.6, -8.0), (2 * P + 2.6, -5.0)))

s = part('R', C('Widerstand, regelbar (alt)', 'Variable resistor, old form', 'Résistance variable, forme ancienne'))
zig(leads(s, P, 3 * P), P, 3 * P, 6, 1.0).line((2 * P, -2 * P), (2 * P, -0.15), end='arrow', size=1.2)
add(ends(s.pin('3', (2 * P, -2 * P), label=(2 * P + 0.5, -2 * P - 0.4))).labels((2 * P + 0.8, -5.6), (2 * P, 2.8), 'left', 'centre'))

# --- Nine-segment displays: the seven segments a to g and the diagonals h (upper half) and i (lower half), both
# running from the right up to the left down (the variant of the Soviet postcode digits, segment names as in the
# Wikipedia article "Seven-segment display", section "Nine-segment display"), common cathode K
NINE = {'a': ((0, 0), (1, 0)), 'b': ((1, 0), (1, 1)), 'c': ((1, 1), (1, 2)), 'd': ((0, 2), (1, 2)), 'e': ((0, 1), (0, 2)),
        'f': ((0, 0), (0, 1)), 'g': ((0, 1), (1, 1)), 'h': ((1, 0), (0, 1)), 'i': ((1, 1), (0, 2))}


def digit(s, x, y, w, h, wid=0.8, gap=0.5):
    for (ax, ay), (bx, by) in NINE.values():
        p, q = (x + ax * w, y + ay * h / 2), (x + bx * w, y + by * h / 2)
        n = math.hypot(q[0] - p[0], q[1] - p[1])
        ux, uy = (q[0] - p[0]) / n * gap, (q[1] - p[1]) / n * gap
        s.line((p[0] + ux, p[1] + uy), (q[0] - ux, q[1] - uy), w=wid)
    return s


s = part('9Seg', C('9-Segment-Anzeige', 'Nine-segment display', 'Afficheur 9 segments'))
s.rect(-1.6, -16.0, 9 * P + 1.6, -P, w=0.35)
digit(s, 4.5 * P - 3.0, -14.0, 6.0, 10.0)
for k, name in enumerate('abcdefghi' + 'K'):
    s.line((k * P, -P), (k * P, 0)).pin(name, (k * P, 0), label=(k * P, 0.3), align='centre', shown=True)
add(s.labels((9 * P + 2.4, -16.0), (9 * P + 2.4, -13.0)))

s = part('', C('Kabelschleife', 'Cable loop', 'Boucle de câble'))
leads(s, 2 * P - 1.2, 2 * P + 1.2).arc(2 * P, 0, 2.4, 0, 180).line((2 * P, -P), (2 * P, P))
add(ends(s))
s = part('Rö', C('Röhrenheizung', 'Valve heater', 'Chauffage de tube'))
s.line((0, 0), (0, -P)).line((2 * P, 0), (2 * P, -P)).arc(P, -P, 2 * P, 0, 180)
add(s.pin('1', (0, 0)).pin('2', (2 * P, 0), label=(2 * P + 0.4, -2.2)).labels((2 * P + 1.4, -2 * P - 0.4), (2 * P + 1.4, -P + 0.4)))


def head_box(caption, erase):
    """The transducer head of IEC 617-9 (09-09-09, the box with a pointed end) with the qualifying symbol inside:
    erasing, a cross (09-08-10, the erasing head 09-09-12), or recording and reproducing, a double arrow (09-08-09)."""
    s = part('', caption)
    s.line((0, 0), (P, 0)).line((0, 2 * P), (P, 2 * P))
    s.poly((P, -0.5 * P), (4 * P, -0.5 * P), (4 * P + 2.2, P), (4 * P, 2.5 * P), (P, 2.5 * P), fill=None)
    if erase:
        s.line((2.5 * P - 1.2, P - 1.2), (2.5 * P + 1.2, P + 1.2)).line((2.5 * P - 1.2, P + 1.2), (2.5 * P + 1.2, P - 1.2))
    else:
        s.line((P + 1.4, P), (4 * P - 1.0, P), start='arrow', end='arrow', size=1.2)
    s.pin('1', (0, 0)).pin('2', (0, 2 * P), label=(0.4, 2 * P - 2.2))
    return s.labels((P, -0.5 * P - 3.4), (P, 2.5 * P + 0.6))


add(head_box(C('Löschkopf (Kasten)', 'Erase head (box)', 'Tête d’effacement (boîte)'), True))
add(head_box(C('Kombikopf (Kasten)', 'Record/playback head (box)', 'Tête d’enregistrement/lecture (boîte)'), False))

s = part('', C('Transformator (Kasten mit Hinweis)', 'Transformer (box with label)', 'Transformateur (boîte avec inscription)'))
s.rect(P, -P, 4 * P, 3 * P).text('Trafo', 2.5 * P, P - 1.25, 2.5, 'centre')
for y in (0, 2 * P):
    s.line((0, y), (P, y)).line((4 * P, y), (5 * P, y))
s.pin('1', (0, 0)).pin('2', (0, 2 * P), label=(0.4, 2 * P - 2.2))
s.pin('3', (5 * P, 0), label=(5 * P - 0.4, -2.2), align='right').pin('4', (5 * P, 2 * P), label=(5 * P - 0.4, 2 * P - 2.2), align='right')
add(s.labels((P, -P - 3.4), (P, 3 * P + 0.6)))

s = resistor_body(part('R', C('Widerstand, regelbar', 'Variable resistor', 'Résistance variable'), fixed=True))
add(ends(s.arrow((P + 0.6, 2.2), (3 * P - 0.4, -2.4), size=1.2)))
add(ends(zig(leads(part('', C('Heizleiter (Zickzack)', 'Heating conductor (zigzag)', 'Conducteur chauffant (zigzag)')), P, 3 * P), P, 3 * P, 4, 1.6, w=0.35)))
add(ends(meander(leads(part('R', C('Heizleiter (Mäander)', 'Heating conductor (meander)', 'Conducteur chauffant (méandre)')), P, 3 * P), P, 3 * P, 6, 1.2)))
s = part('', C('Zeigerinstrument', 'Pointer instrument', 'Instrument à aiguille'))
leads(s, P, 3 * P).rect(P, -3.2, 3 * P, 3.2).arc(2 * P, 1.6, 5.0, 40, 140, w=0.18)
s.line((2 * P, 1.6), (2 * P + 1.3, -1.8)).circle(2 * P, 1.6, 0.8, w=0.05, fill='#000000')
add(ends(s).labels((2 * P, -6.4), (2 * P, 3.6), 'centre'))

s = box('9Seg', C('9-Segment-Anzeige (Kasten)', 'Nine-segment display (box)', 'Afficheur 9 segments (boîte)'), list('abcdefghi'), [], width=6 * P)
s.numbered = False
digit(s, 3.8 * P, 2.5 * P, 2.2 * P, 5.0 * P, wid=THICK, gap=0.4)
add(s.line((4 * P, 10 * P), (4 * P, 11 * P)).pin('K', (4 * P, 11 * P), label=(4 * P + 0.5, 11 * P - 2.0), shown=True))
