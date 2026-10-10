# More symbols for the page "Kondensatoren" after DIN EN 60617-4: the variable and preset capacitors and the polarised
# capacitor with the bent plate upright, and the older form of the electrolytic capacitor with the negative plate hatched.
add = extend(BT, 'Kondensatoren')

ARROW = ((P + 0.4, 2.6), (3 * P - 0.4, -2.8))     # adjustment mark of "Drehkondensator"


def mid(p):
    """A point of a horizontal symbol (centre (2P, 0)) moved to the same place of the vertical one (centre (0, 2P))."""
    return (p[0] - 2 * P, p[1] + 2 * P)


def plus_upright(s):
    for o in s.parts:
        if o['type'] == 'text' and o['text'] == '+':
            o['pos'], o['align'] = pt((-3.6, 2 * P - 3.6)), 'centre'
    return s


for de, en, fr, end, size in (('Drehkondensator vertikal', 'Variable capacitor, vertical', 'Condensateur variable vertical', 'arrow', 1.2),
                              ('Trimmkondensator vertikal', 'Trimmer capacitor, vertical', 'Condensateur ajustable vertical', 'bar', 1.6)):
    s = capacitor(two_pin('C', C(de, en, fr), ask=True)).vertical()
    s.line(mid(ARROW[0]), mid(ARROW[1]), end=end, size=size)
    add(s.labels((3.4, P - 0.4), (3.4, 2 * P + 0.4)))

s = two_pin('C', C('Elko (gebogene Platte) vertikal', 'Polarised capacitor, vertical', 'Condensateur polarisé vertical'), ask=True)
s.line((0, 0), (2 * P - 0.5, 0)).line((2 * P - 0.5, -2.2), (2 * P - 0.5, 2.2), w=THICK)
s.arc(2 * P + 2.6, 0, 5.4, 140, 220, w=THICK).line((2 * P + 0.55, 0), (4 * P, 0)).text('+', 2 * P - 2.2, -3.4, 2.2, 'centre')
add(plus_upright(s.vertical()).labels((3.0, P - 0.4), (3.0, 2 * P + 0.4)))


def hatched(s, x=2 * P):
    """Polarised capacitor in the older form: the positive plate hollow, the negative plate hatched."""
    s.line((0, 0), (x - 1.3, 0)).line((x + 1.3, 0), (4 * P, 0))
    s.rect(x - 1.3, -2.2, x - 0.5, 2.2).rect(x + 0.5, -2.2, x + 1.3, 2.2)
    s.parts[-1]['fill'] = {'style': 'diagonal', 'color': '#000000', 'spacing': 0.35, 'lineWidth': 0.15}
    return s.text('+', x - 2.8, -3.2, 2.2, 'centre')


add(hatched(two_pin('C', C('Elektrolytkondensator (schraffierte Platte)', 'Electrolytic capacitor (hatched plate)',
                           'Condensateur électrolytique (armature hachurée)'), ask=True)))
s = hatched(two_pin('C', C('Elektrolytkondensator (schraffierte Platte) vertikal', 'Electrolytic capacitor (hatched plate), vertical',
                           'Condensateur électrolytique (armature hachurée) vertical'), ask=True))
add(plus_upright(s.vertical()).labels((3.0, P - 0.4), (3.0, 2 * P + 0.4)))
