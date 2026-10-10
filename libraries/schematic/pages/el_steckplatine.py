# More breadboard pictures for the page "Steckplatine": the complete half-size board, a single hole, parts seen from
# above with their legs on the holes (TO-92, TO-220, LEDs, film and electrolytic capacitors, DIP packages), wire bridges
# and bent jumper wires. Drawn with the building blocks of the page; the bodies roughly to size, the DIP rows 0.3"
# apart, 0.4" for 22 pins and 0.6" from 24 pins on, as the common packages.
add = extend(EL_E, 'Steckplatine')
LEG = '#808080'


def moved(s, piece, dx, dy):
    """The drawing of `piece` added to `s`, shifted by (dx, dy)."""
    for o in piece.parts:
        if 'points' in o:
            o['points'] = [pt((x + dx, y + dy)) for x, y in o['points']]
        for key in ('centre', 'pos', 'pin'):
            if key in o:
                o[key] = pt((o[key][0] + dx, o[key][1] + dy))
        s.parts.append(o)
    return s


def chord(s, cx, cy, w, h, start, stop, fill, pen=0.25):
    """A filled piece of an ellipse cut off by a straight line (a half disc and the like)."""
    s.parts.append({'type': 'ellipse', 'centre': pt((cx, cy)), 'size': [r(w), r(h)], 'pen': {'width': pen},
                    'fill': {'style': 'solid', 'color': fill}, 'start': r(start), 'stop': r(stop), 'arc': 'chord'})
    return s


def leads(s, *xs):
    """Where the legs of an upright part go into the board: grey dots on the holes."""
    for x in xs:
        s.circle(x, 0, 0.6, w=0.1, fill=LEG)
    return s


# A half-size board: supply rails above and below, two fields of 5 × 30 with the channel between them.
s = Symbol('', C('Steckplatine (komplett)', 'Breadboard (complete)', "Plaque d'essai (complète)"), numbered=False, listed=False, shown=False)
s.rect(-1.2 * P, -1.2 * P, 30.2 * P, 20.2 * P, w=0.25, fill='#f6f6f6')
for row, rows, rails in ((0, 2, True), (4, 5, False), (11, 5, False), (18, 2, True)):
    moved(s, board(C('-', '-', '-'), 30, rows, rails), 0, row * P)
add(s.rect(-P / 2, 9.2 * P, 29.5 * P, 9.8 * P, w=0.1, fill='#d0d0d0'))
s = Symbol('', C('Anschlusspunkt (Steckplatine)', 'Connection point (breadboard)', "Point de connexion (plaque d'essai)"), numbered=False, listed=False, shown=False)
add(s.rect(-0.4, -0.4, 0.4, 0.4, w=0.1, fill=HOLE).pin('1', (0, 0)))


def to92_grey(s):
    # The flat side towards the viewer, the legs bent out from 1.27 mm to the holes.
    for x, foot in ((0, P - 1.27), (P, P), (2 * P, P + 1.27)):
        s.line((x, 0), (foot, -1.2), w=0.5)
        coloured(s, LEG)
    chord(s, P, -0.6, 5.0, 6.8, 0, 180, '#909090')
    s.labels((2 * P + 2.0, -3.4), (2 * P + 2.0, -0.6))


add(plugged('', C('TO-92 (Steckplatine, Draufsicht)', 'TO-92 (breadboard, top view)', "TO-92 (plaque d'essai, vue de dessus)"),
            [('1', (0, 0)), ('2', (P, 0)), ('3', (2 * P, 0))], to92_grey, numbered=False))


def to220_top(s):
    # Standing upright: the metal tab at the back, the printed face towards the viewer.
    x0, x1 = P - 5.0, P + 5.0
    s.line((x0 + 0.6, -2.2), (x1 - 0.6, -2.2), w=1.2)
    coloured(s, '#a8a8a8')
    s.rect(x0, -1.6, x1, 2.9, w=0.25, fill='#303030')
    leads(s, 0, P, 2 * P)
    s.labels((x1 + 1.0, -3.4), (x1 + 1.0, -0.6))


add(plugged('IC', C('TO-220 (Steckplatine, Draufsicht)', 'TO-220 (breadboard, top view)', "TO-220 (plaque d'essai, vue de dessus)"),
            [('1', (0, 0)), ('2', (P, 0)), ('3', (2 * P, 0))], to220_top))
for colour, de, en, fr in (('#ffd820', 'gelb', 'yellow', 'jaune'), ('#30c030', 'grün', 'green', 'verte')):
    def led(s, colour=colour):
        led5(s)
        next(o for o in s.parts if 'fill' in o)['fill']['color'] = colour
    add(plugged('LED', C(f'LED 5 mm (Steckplatine, {de})', f'LED 5 mm (breadboard, {en})', f"DEL 5 mm (plaque d'essai, {fr})"),
                [('A', (0, 0)), ('K', (P, 0))], led, de))
for pitch, grid, length, thick, value in (('2,5', 1, 4.6, 2.6, '10n'), ('5', 2, 7.2, 3.2, '100n'), ('7,5', 3, 10.0, 4.0, '470n'),
                                          ('10', 4, 13.0, 5.0, '1µ')):
    def film(s, grid=grid, length=length, thick=thick):
        x0, x1 = (grid * P - length) / 2, (grid * P + length) / 2
        s.rect(x0, -thick / 2, x1, thick / 2, w=0.25, fill='#c83030', corner=20)
        leads(s, 0, grid * P)
        s.labels((x1 + 0.8, -3.4), (x1 + 0.8, -0.6))
    add(plugged('C', C(f'Kondensator {pitch} mm (Steckplatine)', f"Capacitor, {pitch.replace(',', '.')} mm pitch (breadboard)",
                       f"Condensateur, pas de {pitch} mm (plaque d'essai)"), [('1', (0, 0)), ('2', (grid * P, 0))], film, value))


def elko5(s):
    # An 8 mm can: the minus stripe at the - leg, a + at the + leg.
    s.circle(P, 0, 8.0, w=0.25, fill='#2050a0')
    chord(s, P, 0, 8.0, 8.0, -60, 60, '#e0e0e0', pen=0.05)
    leads(s, 0, 2 * P)
    s.text('+', P - 2.2, -1.5, 2.4, 'centre')
    s.parts[-1]['font']['color'] = '#ffffff'
    s.labels((P + 4.8, -3.4), (P + 4.8, -0.6))


add(plugged('C', C('Elko 5 mm (Steckplatine)', 'Electrolytic capacitor, 5 mm pitch (breadboard)', "Condensateur électrolytique, pas de 5 mm (plaque d'essai)"),
            [('+', (0, 0)), ('-', (2 * P, 0))], elko5, '100µ'))


def dip(n):
    """A DIP package from above, notch at the left: pin 1 at the insertion point (bottom left), counted
    counter-clockwise; the rows 0.3" apart up to 20 pins, 0.4" for 22 and 0.6" from 24 pins on."""
    half, rows = n // 2, 3 if n <= 20 else 4 if n == 22 else 6
    top, x0, x1 = -rows * P, -1.3, (half - 1) * P + 1.3
    s = Symbol('IC', C(f'IC {n} Pin (Steckplatine)', f'IC, {n} pins (breadboard)', f"CI {n} broches (plaque d'essai)"))
    for k in range(half):
        s.rect(k * P - 0.45, -1.0, k * P + 0.45, 0.4, w=0.1, fill='#c0c0c0')
        s.rect(k * P - 0.45, top - 0.4, k * P + 0.45, top + 1.0, w=0.1, fill='#c0c0c0')
    s.rect(x0, top + 1.0, x1, -1.0, w=0.25, fill='#303030')
    chord(s, x0, top / 2, 2.0, 2.0, 270, 450, '#909090', pen=0.1)
    for k in range(n):
        s.pin(str(k + 1), (k * P, 0) if k < half else ((n - 1 - k) * P, top))
    s.labels((x0, top - 3.4), (x0, 0.8))
    return s


for n in (6, 8, 12, 14, 16, 18, 20, 22, 24, 28, 32, 40):
    add(dip(n))
for colour, de, en, fr in (('#202020', 'schwarz', 'black', 'noir'), ('#7a4a00', 'braun', 'brown', 'marron')):
    def bridge(s, colour=colour):
        s.line((0, 0), (0, -1.2), (5 * P, -1.2), (5 * P, 0), w=0.6)
        coloured(s, colour)
    add(plugged('', C(f'Drahtbrücke 5 Raster ({de})', f'Wire jumper, 5 pitches ({en})', f'Pont de fil, 5 pas ({fr})'), [('1', (0, 0)), ('2', (5 * P, 0))], bridge,
                numbered=False, listed=False, shown=False))
for colour, de, en, fr in (('#d00000', 'rot', 'red', 'rouge'), ('#0040d0', 'blau', 'blue', 'bleu'), ('#202020', 'schwarz', 'black', 'noir'),
                           ('#909090', 'grau', 'grey', 'gris'), ('#008000', 'grün', 'green', 'vert'), ('#e0a000', 'gelb', 'yellow', 'jaune')):
    def wire(s, colour=colour):
        s.arc(3 * P, 0, 6 * P, 0, 180, w=0.9, dy=4 * P)
        coloured(s, colour)
    add(plugged('', C(f'Steckbrücke gebogen ({de})', f'Jumper wire, bent ({en})', f'Fil de connexion courbé ({fr})'), [('1', (0, 0)), ('2', (6 * P, 0))], wire,
                numbered=False, listed=False, shown=False))
