# Batteries, continued: upright and dashed batteries of several cells, solar cells (a cell with light arrows, and the
# box form with a chevron), two batteries drawn as they look and the signs of a charge indicator. Cells after
# DIN EN 60617-6 (long line +, short line −).
add = extend(BT, 'Batterien')


def cell(s, x):
    """A cell across the x axis: the long line (+) at x, the short heavy line (−) 1 mm to the right."""
    return s.line((x, -2.6), (x, 2.6)).line((x + 1.0, -1.3), (x + 1.0, 1.3), w=0.7)


def dashes(s, x0, x1, n=3, gap=0.9):
    """A dashed connection along the x axis, drawn dash by dash so that it starts and ends with a dash."""
    d = (x1 - x0 - (n - 1) * gap) / n
    for k in range(n):
        s.line((x0 + k * (d + gap), 0), (x0 + k * (d + gap) + d, 0))
    return s


def upright(s, plus):
    """The symbol turned to stand upright (+ at the top), its + sign moved to `plus`."""
    s.vertical()
    for o in s.parts:
        if o['type'] == 'text' and o['text'] == '+':
            o['pos'], o['align'] = pt(plus), 'centre'
    return s


def light(s, tips):
    """Arrows of incoming light from the upper right, ending at `tips`."""
    for x, y in tips:
        s.arrow((x + 1.7, y - 1.7), (x, y), size=0.9)
    return s


def poled(caption, value=''):
    """A symbol with the contacts + and − (named + and -)."""
    s = Symbol('G', caption, value)
    s.pin('+', (0, 0)).pin('-', (4 * P, 0), label=(4 * P - 0.4, -2.2), align='right')
    return s.labels((2 * P, -5.6), (2 * P, 2.8), 'centre')


A, B = 3.43, 5.73     # the cells of a two-cell battery, centred on the symbol
s = two_pin('G', C('Akku (senkrecht)', 'Rechargeable battery, vertical', 'Accumulateur vertical'))
cell(cell(s.line((0, 0), (A, 0)), A).line((A + 1.0, 0), (B, 0)), B).line((B + 1.0, 0), (4 * P, 0)).text('+', A - 1.5, -3.6, 2.2, 'centre')
add(upright(s, (-3.4, A - 2.4)).labels((3.6, P - 0.4), (3.6, 2 * P + 0.4)))


def dashed_battery(caption):
    s = two_pin('G', caption)
    cell(cell(s.line((0, 0), (1.6, 0)), 1.6), 4 * P - 2.6)
    return dashes(s, 2.6, 4 * P - 2.6).line((4 * P - 1.6, 0), (4 * P, 0)).text('+', 0.1, -3.6, 2.2, 'centre')


add(dashed_battery(C('Batterie, mehrzellig (gestrichelt)', 'Battery, several cells (dashed)', 'Batterie de plusieurs éléments (en tirets)')))
s = dashed_battery(C('Batterie, mehrzellig (gestrichelt, senkrecht)', 'Battery, several cells (dashed, vertical)', 'Batterie de plusieurs éléments (en tirets, verticale)'))
add(upright(s, (-3.4, -0.8)).labels((3.6, P - 0.4), (3.6, 2 * P + 0.4)))


def pv_box(s):
    """A solar module as a box: the chevron at the + end, like a diode pointing to the − end."""
    s.line((0, 0), (P, 0)).rect(P, -1.4, 3 * P, 1.4).line((3 * P, 0), (4 * P, 0)).line((P, -1.4), (P + 1.8, 0), (P, 1.4))
    return s.text('+', P - 1.3, -3.6, 2.2, 'centre')


add(pv_box(poled(C('Solarzelle (Kastenform)', 'Solar cell (box form)', 'Cellule solaire (forme rectangulaire)'))))
s = pv_box(poled(C('Solarzelle (Kastenform, senkrecht)', 'Solar cell (box form, vertical)', 'Cellule solaire (forme rectangulaire, verticale)')))
add(upright(s, (-2.2, P - 2.8)).labels((2.4, P + 0.4), (2.4, 2 * P + 1.2)))


def solar(s):
    return cell(s.line((0, 0), (2 * P - 0.5, 0)), 2 * P - 0.5).line((2 * P + 0.5, 0), (4 * P, 0)).text('+', 2 * P - 2.0, -3.6, 2.2, 'centre')


s = solar(poled(C('Solarzelle', 'Solar cell', 'Cellule solaire')))
add(light(s, ((2 * P + 0.1, -2.0), (2 * P + 1.5, -2.0))).labels((2 * P, -7.4), (2 * P, 2.8), 'centre'))
s = upright(solar(poled(C('Solarzelle (senkrecht)', 'Solar cell, vertical', 'Cellule solaire verticale'))), (-3.4, 2 * P - 2.9))
add(light(s, ((0.9, 2 * P - 1.2), (2.3, 2 * P - 1.2))).labels((5.4, P - 0.4), (5.4, 2 * P + 0.4)))

# Batteries as they look: a round cell from the side and a car battery from the front.
s = Symbol('G', C('Batterie 1,5 V (Bauform)', '1.5 V battery (physical shape)', 'Pile 1,5 V (forme réelle)'), '1,5 V')
s.line((0, 0), (0, 0.5 * P)).rect(-1.0, 0.5 * P, 1.0, P, fill='#c0c0c0').rect(-P, P, P, 5.5 * P).line((0, 5.5 * P), (0, 6 * P))
s.line((0.5, 4.96), (-1.1, 8.56), (1.1, 7.96), (-0.5, 11.56), end='arrow', size=1.0)
s.text('+', 1.8, -0.2, 2.2).text('−', 1.0, 5.5 * P + 0.2, 2.2)
s.pin('+', (0, 0), label=(0.4, -2.2)).pin('-', (0, 6 * P), label=(0.4, 6 * P - 2.2))
add(s.labels((P + 1.0, 2 * P), (P + 1.0, 3 * P)))
s = Symbol('G', C('Autobatterie (Bauform)', 'Car battery (physical shape)', 'Batterie de voiture (forme réelle)'), '12 V')
for x in (0, 4 * P):
    s.rect(x - 0.9, 0, x + 0.9, 1.4, fill='#c0c0c0')
s.rect(-P, 1.4, 5 * P, 2.8).rect(-P, 2.8, 5 * P, 12.0).text('+', 0, 3.4, 3.0, 'centre').text('−', 4 * P, 3.4, 3.0, 'centre')
s.pin('+', (0, 0), label=(0.4, -2.2)).pin('-', (4 * P, 0), label=(4 * P - 0.4, -2.2), align='right')
add(s.labels((5 * P + 1.0, 1.4), (5 * P + 1.0, 4.4)))

# The signs of a charge indicator: the outline of a battery with five bars.
for filled, de, en, fr in ((5, 'Ladezustand voll', 'Charge level full', 'Niveau de charge plein'),
                           (2, 'Ladezustand halb', 'Charge level half', 'Niveau de charge à moitié')):
    s = Symbol('', C(de, en, fr), numbered=False, listed=False, shown=False)
    s.rect(0, 0, 10.0, 5.0, w=0.35).rect(10.0, 1.5, 11.0, 3.5, w=0.35, fill='#000000')
    for k in range(5):
        x = 0.6 + k * 1.84
        if k < filled:
            s.rect(x, 0.6, x + 1.44, 4.4, w=0.1, fill='#000000')
        else:
            s.rect(x, 0.6, x + 1.44, 4.4, w=0.18)
    add(s)
