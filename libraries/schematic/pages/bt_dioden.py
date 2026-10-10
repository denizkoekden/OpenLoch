# Diodes after DIN EN 60617-5 (05-03 semiconductor diodes, 05-04-03 diac) for the pages "Dioden" (outline) and
# "Dioden (schwarz)" (filled): the LED, the vertical forms, the bidirectional suppressor diode and the diac; on the
# outline page also the bridge rectifier as a block. Anode (1) left or at the top, cathode (2) right or below.


def leads(s, ax=1.5 * P, cx=2.5 * P):
    return s.line((0, 0), (ax, 0)).line((cx, 0), (4 * P, 0))


def diode(s, fill):
    return diode_shape(leads(s), 1.5 * P, 2.5 * P, fill=fill)


def zener(s, fill):
    return diode(s, fill).line((2.5 * P, -1.5), (2.5 * P - 0.8, -1.5))


def schottky(s, fill):
    diode(s, fill)
    return s.line((2.5 * P + 0.7, -1.0), (2.5 * P + 0.7, -1.5), (2.5 * P, -1.5)).line((2.5 * P, 1.5), (2.5 * P - 0.7, 1.5), (2.5 * P - 0.7, 1.0))


def tunnel(s, fill):
    return diode(s, fill).line((2.5 * P - 0.7, -1.5), (2.5 * P, -1.5)).line((2.5 * P - 0.7, 1.5), (2.5 * P, 1.5))


def varicap(s, fill):
    diode_shape(leads(s, 1.25 * P, 2.25 * P + 0.8), 1.25 * P, 2.25 * P, fill=fill)
    return s.line((2.25 * P + 0.8, -1.5), (2.25 * P + 0.8, 1.5), w=0.35)


def suppressor(s, fill):
    """Bidirectional breakdown diode: the triangles meet at one bar, hooked at both ends."""
    leads(s, P, 3 * P)
    s.poly((P, -1.5), (P, 1.5), (2 * P, 0), fill=fill).poly((3 * P, -1.5), (3 * P, 1.5), (2 * P, 0), fill=fill)
    s.line((2 * P, -1.5), (2 * P, 1.5), w=0.35)
    return s.line((2 * P, -1.5), (2 * P - 0.8, -1.5)).line((2 * P, 1.5), (2 * P + 0.8, 1.5))


def diac(s, fill):
    """Bidirectional diode thyristor: two opposite triangles between two bars, like the diac of "Thyristoren"."""
    leads(s)
    s.line((1.5 * P, -2.6), (1.5 * P, 2.6), w=0.35).line((2.5 * P, -2.6), (2.5 * P, 2.6), w=0.35)
    return s.poly((1.5 * P, -2.4), (1.5 * P, -0.1), (2.5 * P, -1.25), fill=fill).poly((2.5 * P, 0.1), (2.5 * P, 2.4), (1.5 * P, 1.25), fill=fill)


def upright(s, x=2.4):
    """The horizontal form turned, anode at the top; the texts at the right like "Diode vertikal"."""
    return s.vertical().labels((x, P - 0.6), (x, 2 * P + 0.4))


for page_name, fill, varicap_prefix in (('Dioden', None, 'D'), ('Dioden (schwarz)', '#000000', 'CD')):
    add = extend(BT, page_name)
    s = diode(two_pin('LED', C('LED', 'LED', 'DEL')), fill)
    arrows_out(s, 2 * P - 0.4, -1.8)
    add(s.labels((2 * P, -7.4), (2 * P, 2.0), 'centre'))
    s = upright(diode(two_pin('LED', C('LED (senkrecht)', 'LED, vertical', 'DEL verticale')), fill))
    arrows_out(s, 1.6, 2 * P - 0.4)
    add(s.labels((6.0, P - 0.6), (6.0, 2 * P + 0.4)))
    add(upright(zener(two_pin('D', C('Z-Diode (senkrecht)', 'Zener diode, vertical', 'Diode Zener verticale')), fill)))
    add(upright(schottky(two_pin('D', C('Schottky-Diode (senkrecht)', 'Schottky diode, vertical', 'Diode Schottky verticale')), fill)))
    add(upright(tunnel(two_pin('D', C('Tunneldiode (senkrecht)', 'Tunnel diode, vertical', 'Diode tunnel verticale')), fill)))
    add(upright(varicap(two_pin(varicap_prefix, C('Kapazitätsdiode (senkrecht)', 'Varicap diode, vertical', 'Diode varicap verticale')), fill)))
    s = upright(diode(two_pin('D', C('Fotodiode (senkrecht)', 'Photodiode, vertical', 'Photodiode verticale')), fill))
    for k in (0, 1):      # light falling on the triangle from up right
        tip = (1.9, 1.5 * P + 0.2 + 1.4 * k)
        s.arrow((tip[0] + 1.7, tip[1] - 1.7), tip, size=0.9)
    add(s.labels((4.4, P - 0.6), (4.4, 2 * P + 0.4)))
    add(suppressor(two_pin('D', C('Suppressordiode', 'Bidirectional TVS diode', 'Diode TVS bidirectionnelle')), fill))
    add(upright(suppressor(two_pin('D', C('Suppressordiode (senkrecht)', 'Bidirectional TVS diode, vertical', 'Diode TVS bidirectionnelle verticale')), fill)))
    add(diac(two_pin('D', C('Diac', 'Diac', 'Diac')), fill))
    add(upright(diac(two_pin('D', C('Diac (senkrecht)', 'Diac, vertical', 'Diac vertical')), fill), 3.4))

add = extend(BT, 'Dioden')
s = Symbol('T', C('Brückengleichrichter (Block)', 'Bridge rectifier (block)', 'Pont redresseur (bloc)'))
x0, x1 = P, 4 * P
s.rect(x0, 0, x1, 3 * P)
for y, name in ((P, '~1'), (2 * P, '~2')):
    s.line((0, y), (x0, y)).text('~', x0 + 0.6, y - 1.2, 2.2).pin(name, (0, y))
for y, name, sign in ((P, '+', '+'), (2 * P, '-', '−')):
    s.line((x1, y), (x1 + P, y)).text(sign, x1 - 0.6, y - 1.2, 2.2, 'right')
    s.pin(name, (x1 + P, y), label=(x1 + P - 0.4, y - 2.2), align='right')
add(s.labels((x0, -3.4), (x0, 3 * P + 0.6)))
