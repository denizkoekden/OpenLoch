# More symbols for the page "Spulen": the choke as a filled rectangle (the form DIN 40712 used for inductors), with core
# and adjustment marks after DIN EN 60617-4, the coils of the page upright, and the older helical drawing of a coil.
add = extend(BT, 'Spulen')

ARROW = ((P + 0.2, 2.0), (3 * P - 0.2, -3.0))     # adjustment mark of "Einstellbare Spule"


def mid(p):
    """A point of a horizontal symbol (centre (2P, 0)) moved to the same place of the vertical one (centre (0, 2P))."""
    return (p[0] - 2 * P, p[1] + 2 * P)


def winding(de, en, fr):
    s = two_pin('L', C(de, en, fr), ask=True)
    return coil(s.line((0, 0), (P, 0)).line((3 * P, 0), (4 * P, 0)), P, 3 * P)


def block(s, x0=P, x1=3 * P, h=1.0):
    return s.line((0, 0), (x0, 0)).rect(x0, -h, x1, h, fill='#000000').line((x1, 0), (4 * P, 0))


def choke(de, en, fr):
    return block(two_pin('L', C(de, en, fr), ask=True))


# With the value inside, the block is only outlined (heavy), so the value stays readable in black and white too.
s = two_pin('L', C('Drossel (Wert im Symbol)', 'Choke (value inside)', 'Self (valeur à l’intérieur)'), ask=True)
s.line((0, 0), (0.5 * P, 0)).rect(0.5 * P, -1.7, 3.5 * P, 1.7, w=THICK).line((3.5 * P, 0), (4 * P, 0))
add(s.labels((2 * P, -6.0), (2 * P, -1.25), 'centre'))
add(choke('Drossel (Rechteck) vertikal', 'Choke (block), vertical', 'Self (rectangle) verticale').vertical().labels((2.4, P - 0.4), (2.4, 2 * P + 0.4)))
s = choke('Drossel mit Kern', 'Choke with core', 'Self avec noyau')
add(s.line((P, -2.0), (3 * P, -2.0), w=THICK))
s = choke('Drossel mit Kern vertikal', 'Choke with core, vertical', 'Self avec noyau verticale')
add(s.line((P, -2.0), (3 * P, -2.0), w=THICK).vertical().labels((3.2, P - 0.4), (3.2, 2 * P + 0.4)))
add(choke('Einstellbare Drossel', 'Variable choke', 'Self variable').arrow(*ARROW, size=1.2))
add(choke('Drossel, voreinstellbar', 'Preset choke', 'Self préréglable').line(*ARROW, end='bar', size=1.6))
for de, en, fr, end, size in (('Einstellbare Drossel vertikal', 'Variable choke, vertical', 'Self variable verticale', 'arrow', 1.2),
                              ('Drossel, voreinstellbar, vertikal', 'Preset choke, vertical', 'Self préréglable verticale', 'bar', 1.6)):
    s = choke(de, en, fr).vertical()
    add(s.line(mid(ARROW[0]), mid(ARROW[1]), end=end, size=size).labels((3.4, P - 0.4), (3.4, 2 * P + 0.4)))

s = winding('Spule mit Kern vertikal', 'Inductor with core, vertical', 'Bobine avec noyau verticale')
add(s.line((P, -1.9), (3 * P, -1.9), w=THICK).vertical().labels((3.0, P - 0.4), (3.0, 2 * P + 0.4)))
s = winding('Spule mit Ferritkern vertikal', 'Inductor with ferrite core, vertical', 'Bobine à noyau de ferrite verticale')
s.parts.append({'type': 'line', 'points': [pt((P, -1.9)), pt((3 * P, -1.9))], 'pen': {'width': THICK, 'style': 'dash'}, 'electrical': False})
add(s.vertical().labels((3.0, P - 0.4), (3.0, 2 * P + 0.4)))
add(winding('Spule, voreinstellbar', 'Preset inductor', 'Bobine préréglable').line(*ARROW, end='bar', size=1.6))
for de, en, fr, end, size in (('Einstellbare Spule vertikal', 'Variable inductor, vertical', 'Bobine variable verticale', 'arrow', 1.2),
                              ('Spule, voreinstellbar, vertikal', 'Preset inductor, vertical', 'Bobine préréglable verticale', 'bar', 1.6)):
    s = winding(de, en, fr).vertical()
    add(s.line(mid(ARROW[0]), mid(ARROW[1]), end=end, size=size).labels((3.4, P - 0.4), (3.4, 2 * P + 0.4)))

# The helix seen from the side: a prolate trochoid, big arcs on top and small loops on the axis.
s = two_pin('L', C('Spule (Wendel)', 'Inductor (helix)', 'Bobine (hélice)'), ask=True)
turns, b = 4, 0.9
a = 2 * P / (2 * math.pi * turns)
helix = [(P + a * t - b * math.sin(t), -b * (1 - math.cos(t))) for t in (k * 2 * math.pi / 24 for k in range(24 * turns + 1))]
add(s.line((0, 0), (P, 0)).line(*helix).line((3 * P, 0), (4 * P, 0)))
