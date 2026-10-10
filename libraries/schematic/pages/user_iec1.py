# USER pages in the manner of EN 81346 reference designations (folder IEC_EN61346): the designator starts with "-"
# and its letter code (EN 81346-2), sits left of the symbol in larger type with the value below it, and the terminal
# numbers are shown. Symbols after DIN EN 60617 (parts 2 to 8), terminal markings after EN 60445 (X1/X2 of lamps,
# A1/A2 of coils, U1 V1 W1 of motors).
IEC = 'USER/Misc. (German)/IEC_EN61346'
TAG = 3.5            # height of the reference designation
D1 = 2 * P - 0.6     # circle of a lamp, meter or small motor between two leads
D3 = 4 * P - 0.6     # circle of a motor with three leads side by side


class Tagged(Symbol):
    """A symbol whose designator is written larger than the value."""
    def json(self, entry):
        o = super().json(entry)
        for c in o['children']:
            if c.get('role') == 'designator':
                c['font']['height'] = TAG
        return o


def left_tag(s, x, mid):
    """Designator and value right-aligned at x, one below the other, as a block centred on the height mid."""
    top = mid - (TAG + 0.6 + 2.5) / 2
    return s.labels((x, top), (x, top + TAG + 0.6), 'right')


def top_tag(s, x, above, below):
    """For a lying symbol: the designator centred over the height `above`, the value under `below`."""
    return s.labels((x, above - TAG - 0.4), (x, below + 0.4), 'centre')


def styled(s, style):
    s.parts[-1]['pen']['style'] = style
    return s


def upright(prefix, caption, value='', names=('1', '2'), length=4 * P, **flags):
    """A part standing between the contacts at (0, 0) and (0, length), the terminal numbers right of the leads."""
    s = Tagged(prefix, caption, value, **flags)
    s.pin(names[0], (0, 0), label=(0.8, 0.2), shown=True).pin(names[1], (0, length), label=(0.8, length - 2.4), shown=True)
    return s


def lying(prefix, caption, value='', **flags):
    s = Tagged(prefix, caption, value, **flags)
    s.pin('1', (0, 0), label=(0.4, -2.2), shown=True).pin('2', (4 * P, 0), label=(4 * P - 0.4, -2.2), align='right', shown=True)
    return s


def leads(s, y0, y1, length=4 * P, x=0):
    return s.line((x, 0), (x, y0)).line((x, y1), (x, length))


def dot(s, x, y):
    return s.circle(x, y, 0.8, fill='#000000')


def ac(s, x, y, w=2.4):
    """The sign of alternating current, centred at (x, y)."""
    q = w / 4
    return s.arc(x - q, y, 2 * q, 0, 180).arc(x + q, y, 2 * q, 180, 360)


def dc(s, x, y, w=2.2):
    return s.line((x - w / 2, y - 0.4), (x + w / 2, y - 0.4)).line((x - w / 2, y + 0.4), (x + w / 2, y + 0.4))


# --- Signs used on several pages
def lamp(s, cx, cy, d=D1):
    """A lamp after DIN EN 60617-8: a circle with a diagonal cross."""
    h = d / 2 / math.sqrt(2)
    return s.circle(cx, cy, d).line((cx - h, cy - h), (cx + h, cy + h)).line((cx - h, cy + h), (cx + h, cy - h))


def pe_sign(s, cx, cy, d=4.0):
    """Protective earth: the earth sign in a circle."""
    s.circle(cx, cy, d).line((cx, cy - d / 2), (cx, cy - 0.5))
    for k, half in enumerate((1.3, 0.85, 0.4)):
        s.line((cx - half, cy - 0.5 + k * 0.6), (cx + half, cy - 0.5 + k * 0.6), w=0.35)
    return s


# --- Page "Diverses": illuminated push buttons and key switches, contact 13-14 and lamp X1-X2 of one device in one
# outline as in the circuit diagram of Siemens 3SU1106-0AB40-1BA0; operating signs after DIN EN 60617-2 (02-13-05
# pushing, 02-13-13 key) as reproduced in Siemens Building Technologies CM1N0113E (12.1998)
def push_sign(s, x, y):
    s.line((x + 0.8, y - 1.2), (x, y - 1.2), (x, y + 1.2), (x + 0.8, y + 1.2))
    return x


def key_sign(s, x, y):
    """The bow above, the bit below, the link at the neck."""
    a = x - 0.35
    s.circle(a, y - 0.8, 1.6).poly((a - 0.35, y), (a + 0.35, y), (a + 0.8, y + 1.8), (a - 0.8, y + 1.8), fill=None)
    return a - 0.8


def lit_switch(caption, sign, lx, frame=None, fixed=True):
    """A make contact operated by `sign` over the dashed link and the lamp of the same device `lx` to the right;
    `frame` draws the device's outline ('dashDot') or a frame ('dash') around both."""
    s = Tagged('-S', caption, numbered=not fixed)
    h = 5 * P if frame else 4 * P
    y0 = h / 2 - P                 # the fixed contact; the blade's pivot 2 P below
    y1 = y0 + 2 * P
    tip = (-1.9, y0 + 0.6)
    s.pin('13', (0, 0), label=(0.8, 0.2), shown=True).pin('14', (0, h), label=(0.8, h - 2.4), shown=True)
    leads(s, y0, y1, h).line((0, y1), tip)
    ly = (y0 + y1) / 2 - 0.5
    bx = tip[0] * (y1 - ly) / (y1 - tip[1])
    dashed(s, (bx, ly), (-4.4, ly))
    left = sign(s, -4.4, ly)
    cy = h / 2
    lamp(s.line((lx, 0), (lx, cy - D1 / 2)).line((lx, cy + D1 / 2), (lx, h)), lx, cy)
    s.pin('X1', (lx, 0), label=(lx + 0.8, 0.2), shown=True).pin('X2', (lx, h), label=(lx + 0.8, h - 2.4), shown=True)
    if frame:
        left -= 1.0
        styled(s.rect(left, y0 - 0.9, lx + D1 / 2 + 0.9, y1 + 0.9), frame)
    return left_tag(s, left - 1.0, cy)


add = page(IEC, 'Diverses', 'Miscellaneous', 'Divers')
add(lit_switch(C('Leuchttaster (Schließer)', 'Illuminated push button (make)', 'Bouton-poussoir lumineux (fermeture)'), push_sign, 3 * P, 'dashDot'))
add(lit_switch(C('Schlüsselschalter mit Leuchte', 'Key switch with lamp', 'Interrupteur à clé avec voyant'), key_sign, 3 * P, 'dashDot'))
add(lit_switch(C('Leuchttaster (kompakt)', 'Illuminated push button (compact)', 'Bouton-poussoir lumineux (compact)'), push_sign, 2 * P, fixed=False))
add(lit_switch(C('Leuchttaster (Rahmen)', 'Illuminated push button (framed)', 'Bouton-poussoir lumineux (cadre)'), push_sign, 2 * P, 'dash', fixed=False))
add(lit_switch(C('Schlüsselschalter mit Leuchte (kompakt)', 'Key switch with lamp (compact)', 'Interrupteur à clé avec voyant (compact)'),
               key_sign, 2 * P, fixed=False))
add(lit_switch(C('Schlüsselschalter mit Leuchte (Rahmen)', 'Key switch with lamp (framed)', 'Interrupteur à clé avec voyant (cadre)'),
               key_sign, 2 * P, 'dash', fixed=False))


# --- Page "Elektronik": standing parts after DIN EN 60617-4 and -5
def resistor(s):
    return leads(s, P, 3 * P).rect(-1.0, P, 1.0, 3 * P)


def bent(s):
    """The bent line of a non-linear dependence across the resistor, its foot at the lower left."""
    return s.line((-2.6, 3 * P + 0.4), (-1.6, 3 * P + 0.4), (1.8, P - 0.04))


def thermal(s, same):
    """ϑ with two arrows below the foot: pointing the same way (PTC) or opposite ways (NTC)."""
    s.text('ϑ', -4.7, 3 * P + 0.9, 2.2)
    s.arrow((-2.6, 4 * P + 0.4), (-2.6, 3 * P + 1.0), size=0.8)
    if same:
        s.arrow((-1.9, 4 * P + 0.4), (-1.9, 3 * P + 1.0), size=0.8)
    else:
        s.arrow((-1.9, 3 * P + 1.0), (-1.9, 4 * P + 0.4), size=0.8)
    return s


def winding(s, x, y0, y1, turns=4):
    """A standing coil of half-turns bulging to the right."""
    d = (y1 - y0) / turns
    for k in range(turns):
        s.arc(x, y0 + d * (k + 0.5), d, 270, 450)
    return s


def choke(caption, core=None):
    s = upright('-R', caption)
    winding(leads(s, P, 3 * P), 0, P, 3 * P)
    if core == 'solid':
        s.line((1.9, P), (1.9, 3 * P), w=THICK)
    elif core == 'gap':
        s.line((1.9, P), (1.9, 2 * P - 0.5), w=THICK).line((1.9, 2 * P + 0.5), (1.9, 3 * P), w=THICK)
    return left_tag(s, -1.4, 2 * P)


def diode(s, up=False, zener=False):
    """A standing diode: the triangle points down (anode above) or up."""
    a, k = (2.5 * P, 1.5 * P) if up else (1.5 * P, 2.5 * P)
    leads(s, 1.5 * P, 2.5 * P).poly((-1.5, a), (1.5, a), (0, k), fill=None).line((-1.5, k), (1.5, k), w=0.35)
    if zener:
        s.line((1.5, k), (1.5, k + (0.8 if up else -0.8)))
    return s


def diode_lying(s, zener=False):
    s.line((0, 0), (1.5 * P, 0)).line((2.5 * P, 0), (4 * P, 0))
    diode_shape(s, 1.5 * P, 2.5 * P)
    if zener:
        s.line((2.5 * P, -1.5), (2.5 * P - 0.8, -1.5))
    return top_tag(s, 2 * P, -1.5, 1.5)


def two_way(s):
    """Two bars across the leads with the triangles of both directions between them (diac, triac)."""
    y0, y1 = 1.5 * P, 2.5 * P
    leads(s, y0, y1).line((-2.6, y0), (2.6, y0), w=0.35).line((-2.6, y1), (2.6, y1), w=0.35)
    s.poly((-2.4, y0), (-0.1, y0), (-1.25, y1), fill=None).poly((0.1, y1), (2.4, y1), (1.25, y0), fill=None)
    return s


def gate(s, start, bend):
    """The gate from the cathode side to G at (2 P, 3 P); the lower terminal number moves left of its lead."""
    s.line(start, bend, (2 * P, 3 * P)).pin('G', (2 * P, 3 * P), label=(2 * P - 0.2, 3 * P - 2.2), align='right', shown=True)
    lower = [o for o in s.parts if o['type'] == 'contact' and o['name'] == '2'][0]
    lower['pos'], lower['align'] = pt((-0.6, 4 * P - 2.4)), 'right'
    return s


def led(prefix, caption, up=False, value='LED'):
    """A standing LED, contact 1 at the anode; the light leaves to the upper right."""
    s = upright(prefix, caption, value, ('2', '1') if up else ('1', '2'))
    arrows_out(diode(s, up), 1.6, 2 * P - 0.4)
    return left_tag(s, -2.4, 2 * P)


add = page(IEC, 'Elektronik', 'Electronics', 'Électronique')
add(left_tag(resistor(upright('-R', C('Widerstand', 'Resistor', 'Résistance'), '10k')), -2.0, 2 * P))
s = resistor(upright('-R', C('Fotowiderstand', 'Photoresistor', 'Photorésistance')))
for k in (0, 1):
    tip = (1.3, 2 * P - 1.4 + 1.6 * k)
    s.arrow((tip[0] + 1.7, tip[1] - 1.7), tip, size=0.9)
add(left_tag(s, -2.0, 2 * P))
s = bent(resistor(upright('-R', C('Varistor', 'Varistor', 'Varistance'))))
add(left_tag(s.text('U', -2.1, 3 * P + 0.9, 2.2, 'centre'), -2.8, 2 * P - 0.8))
add(left_tag(thermal(bent(resistor(upright('-R', C('Kaltleiter', 'PTC thermistor', 'Thermistance CTP')))), True), -2.8, 2 * P - 0.8))
add(left_tag(thermal(bent(resistor(upright('-R', C('Heißleiter', 'NTC thermistor', 'Thermistance CTN')))), False), -2.8, 2 * P - 0.8))
s = resistor(upright('-R', C('Potentiometer', 'Potentiometer', 'Potentiomètre')))
s.arrow((-2 * P, 2 * P), (-1.0, 2 * P), size=1.2).pin('3', (-2 * P, 2 * P), label=(-2 * P + 0.3, 2 * P - 2.2), shown=True)
add(left_tag(s, -1.6, -0.9))
add(led('-P', C('Leuchtdiode', 'Light-emitting diode', 'Diode électroluminescente')))
add(led('-P', C('Leuchtdiode (Anode unten)', 'Light-emitting diode (anode at the bottom)', 'Diode électroluminescente (anode en bas)'), up=True))
add(choke(C('Drossel', 'Choke', 'Self')))
add(choke(C('Drossel mit Kern', 'Choke with core', 'Self à noyau'), 'solid'))
add(choke(C('Drossel mit Kern und Luftspalt', 'Choke with gapped core', 'Self à noyau avec entrefer'), 'gap'))
add(left_tag(diode(upright('-R', C('Diode', 'Diode', 'Diode'), '1N4007')), -2.4, 2 * P))
add(diode_lying(lying('-R', C('Diode (waagerecht)', 'Diode (horizontal)', 'Diode (horizontale)'), '1N4007')))
add(left_tag(diode(upright('-R', C('Z-Diode', 'Zener diode', 'Diode Zener')), zener=True), -2.4, 2 * P))
add(diode_lying(lying('-R', C('Z-Diode (waagerecht)', 'Zener diode (horizontal)', 'Diode Zener (horizontale)')), zener=True))
add(left_tag(two_way(upright('-R', C('Diac', 'Diac', 'Diac'), 'BR100-03')), -3.4, 2 * P))
s = gate(diode(upright('-Q', C('Thyristor', 'Thyristor', 'Thyristor'), 'BRY 55/200')), (0.6, 2.5 * P), (2.0, 3 * P))
add(left_tag(s, -2.4, 2 * P))
add(left_tag(gate(two_way(upright('-Q', C('Triac', 'Triac', 'Triac'), 'BT 136/600')), (1.8, 2.5 * P), (2.6, 3 * P)), -3.4, 2 * P))
s = leads(upright('-C', C('Kondensator', 'Capacitor', 'Condensateur'), '100 nF'), 2 * P - 0.5, 2 * P + 0.5)
s.line((-2.2, 2 * P - 0.5), (2.2, 2 * P - 0.5), w=THICK).line((-2.2, 2 * P + 0.5), (2.2, 2 * P + 0.5), w=THICK)
add(left_tag(s, -3.2, 2 * P))
s = leads(upright('-C', C('Elektrolytkondensator', 'Electrolytic capacitor', 'Condensateur électrolytique'), '10 µF'), 2 * P - 1.1, 2 * P + 1.1)
s.rect(-2.2, 2 * P - 1.1, 2.2, 2 * P - 0.5).rect(-2.2, 2 * P + 0.5, 2.2, 2 * P + 1.1, fill='#000000')
add(left_tag(s.text('+', 3.0, 2 * P - 3.4, 2.2, 'centre'), -3.2, 2 * P))


# --- Page "Generatoren, Umformer, Wandler": converters after DIN EN 60617-6, inputs above, outputs below
def converter_box(s, x0, x1, y0=P, y1=5 * P):
    return s.rect(x0, y0, x1, y1, w=0.35)


def terminals(s, names, xs, y, inner):
    """Contacts at height y with leads to the box edge at `inner`; '-' is shown as a minus sign."""
    for name, x in zip(names, xs):
        s.line((x, y), (x, inner))
        if y < inner:
            s.pin(name, (x, y), label=(x + 0.6, y + 0.2), shown=True, text='−' if name == '-' else None)
        else:
            s.pin(name, (x, y), label=(x + 0.6, y - 2.4), shown=True, text='−' if name == '-' else None)
    return s


def power_supply(prefix, caption, battery=False):
    """The converter: a square with a diagonal, AC in the upper left, DC in the lower right."""
    s = Tagged(prefix, caption)
    converter_box(s, -P, 3 * P).line((-P, 5 * P), (3 * P, P))
    ac(s, 0.3, 2 * P - 0.6, 3.0)
    dc(s, 2 * P - 0.3, 4 * P + 0.4, 2.8)
    terminals(s, ('L', 'N'), (0, 2 * P), 0, P)
    if battery:
        # The accumulator across the output (DIN EN 60617-6 06-15-01): its long line (+) towards the plus lead.
        terminals(s, ('+', '-'), (0, 2 * P), 7 * P, 5 * P)
        y = 5 * P + 1.8
        s.line((0, y), (P - 0.5, y)).line((P - 0.5, y - 1.4), (P - 0.5, y + 1.4)).line((P + 0.5, y - 0.7), (P + 0.5, y + 0.7), w=0.7)
        s.line((P + 0.5, y), (2 * P, y))
        dot(dot(s, 0, y), 2 * P, y)
    else:
        terminals(s, ('+', '-'), (0, 2 * P), 6 * P, 5 * P)
    return left_tag(s, -P - 1.0, 3 * P)


def small_diode(s, a, b, at=0.5, size=0.7):
    """A diode sign on the line from a to b (conducting from a to b), at the fraction `at` of its length."""
    mx, my = a[0] + (b[0] - a[0]) * at, a[1] + (b[1] - a[1]) * at
    ang = math.degrees(math.atan2(-(b[1] - a[1]), b[0] - a[0]))
    tri = [rot((mx - size, my - size), (mx, my), ang), rot((mx - size, my + size), (mx, my), ang), rot((mx + size, my), (mx, my), ang)]
    s.poly(*tri, fill=None)
    s.line(rot((mx + size, my - size), (mx, my), ang), rot((mx + size, my + size), (mx, my), ang), w=0.35)
    return s


def bridge(caption, phases):
    """A rectifier as a box with its bridge drawn inside (not wired to the terminals)."""
    s = Tagged('-T', caption)
    if phases == 1:
        converter_box(s, -P, 3 * P)
        c, h = (P, 3 * P), 3.6
        left, top, right, bottom = (c[0] - h, c[1]), (c[0], c[1] - h), (c[0] + h, c[1]), (c[0], c[1] + h)
        for a, b in ((bottom, left), (bottom, right), (left, top), (right, top)):
            small_diode(s.line(a, b), a, b, size=0.8)
        terminals(s, ('L1', 'L2'), (0, 2 * P), 0, P)
        terminals(s, ('+', '-'), (0, 2 * P), 6 * P, 5 * P)
        return left_tag(s, -P - 1.0, 3 * P)
    converter_box(s, -P, 5 * P)
    top, bottom = 3 * P - 3.4, 3 * P + 3.4
    s.line((0.5 * P, top), (3.5 * P, top)).line((0.5 * P, bottom), (3.5 * P, bottom))
    for x in (0.5 * P, 2 * P, 3.5 * P):
        s.line((x, top), (x, bottom)).line((x, 3 * P), (x - 1.6, 3 * P))
        small_diode(s, (x, 3 * P), (x, top), size=0.7)
        small_diode(s, (x, bottom), (x, 3 * P), size=0.7)
        dot(s, x, 3 * P)
    terminals(s, ('L1', 'L2', 'L3'), (0, 2 * P, 4 * P), 0, P)
    terminals(s, ('+', '-'), (P, 3 * P), 6 * P, 5 * P)
    return left_tag(s, -P - 1.0, 3 * P)


def pulse_train(s, x, y, n=2, w=1.8, h=1.4):
    for k in range(n):
        x0 = x + k * w
        s.line((x0, y), (x0 + w * 0.25, y), (x0 + w * 0.25, y - h), (x0 + w * 0.75, y - h), (x0 + w * 0.75, y), (x0 + w, y))
    return s


def current_transformer(s, x, first):
    """Form after DIN EN 60617-6: the primary conductor passes straight through, the secondary winding sits on it; its
    ends lead down to the contacts `first` (lower end) and `first` + 1 (upper end)."""
    s.line((x, 0), (x, 4 * P))
    winding(s, x, P, 3 * P)
    s.line((x, 3 * P), (x + P, 3 * P), (x + P, 4 * P)).line((x, P), (x + 2 * P, P), (x + 2 * P, 4 * P))
    for k, px in enumerate((x + P, x + 2 * P)):
        s.pin(str(first + k), (px, 4 * P), label=(px + 0.4, 4 * P - 2.4), shown=True)
    return s


add = page(IEC, 'Generatoren, Umformer, Wandler', 'Generators, converters, transformers', 'Générateurs, convertisseurs, transformateurs')
add(power_supply('-T', C('Netzteil', 'Power supply', 'Alimentation')))
add(power_supply('-C', C('Netzteil mit Akku (Notlichtgerät)', 'Power supply with battery (emergency light unit)',
                         "Alimentation avec batterie (bloc d'éclairage de sécurité)"), battery=True))
add(bridge(C('Gleichrichter (Brücke)', 'Rectifier (bridge)', 'Redresseur (pont)'), 1))
add(bridge(C('Gleichrichter (dreiphasig)', 'Rectifier (three-phase)', 'Redresseur (triphasé)'), 3))
s = Tagged('-G', C('Impulsgeber', 'Pulse generator', "Générateur d'impulsions"))
s.circle(P, 3 * P, D3).text('G', P, 3 * P - 3.8, 3.0, 'centre')
pulse_train(s, P - 1.8, 3 * P + 2.2)
for k, x in enumerate((0, P, 2 * P)):
    e = math.sqrt((D3 / 2) ** 2 - (x - P) ** 2)
    s.line((x, 0), (x, 3 * P - e)).pin(str(k + 1), (x, 0), label=(x + 0.3, -2.4), shown=True)
    s.line((x, 6 * P), (x, 3 * P + e)).pin(str(k + 4), (x, 6 * P), label=(x + 0.3, 6 * P + 0.6), shown=True)
add(left_tag(s, P - D3 / 2 - 1.0, 3 * P))
add(left_tag(current_transformer(Tagged('-T', C('Stromwandler', 'Current transformer', 'Transformateur de courant')), 0, 1), -1.0, 2 * P))
s = Tagged('-T', C('Stromwandler (dreiphasig)', 'Current transformers (three-phase)', 'Transformateurs de courant (triphasés)'))
for k in range(3):
    current_transformer(s, 3 * P * k, 2 * k + 1)
add(left_tag(s, -1.0, 2 * P))


# --- Page "Jumper": pins of a header (2.54 mm) seen from above, the jumper drawn as a rounded bridge over two pins
def jumper(caption, n, bridged=None):
    s = Tagged('J', caption, numbered=False)
    for k in range(n):
        x = k * P
        s.circle(x, -P, 1.6).line((x, -P + 0.8), (x, 0))
        s.pin(str(k + 1), (x, 0), label=(x, -P - 3.6), align='centre', shown=True)
    if bridged:
        a, b = (bridged[0] - 1) * P, (bridged[1] - 1) * P
        s.rect(a - 1.2, -P - 1.2, b + 1.2, -P + 1.2, w=THICK, corner=50)
    return left_tag(s, -2.2, -P)


add = page(IEC, 'Jumper', 'Jumpers', 'Cavaliers')
add(jumper(C('Jumper 2-polig (offen)', 'Jumper, 2 pins (open)', 'Cavalier 2 broches (ouvert)'), 2))
add(jumper(C('Jumper 2-polig (gesteckt)', 'Jumper, 2 pins (fitted)', 'Cavalier 2 broches (enfiché)'), 2, (1, 2)))
add(jumper(C('Jumper 3-polig (offen)', 'Jumper, 3 pins (open)', 'Cavalier 3 broches (ouvert)'), 3))
add(jumper(C('Jumper 3-polig (1–2 gesteckt)', 'Jumper, 3 pins (1–2 fitted)', 'Cavalier 3 broches (1–2 enfiché)'), 3, (1, 2)))
add(jumper(C('Jumper 3-polig (2–3 gesteckt)', 'Jumper, 3 pins (2–3 fitted)', 'Cavalier 3 broches (2–3 enfiché)'), 3, (2, 3)))


# --- Page "Klemmen, Stecker, Buchsen": terminals, plugs and sockets after DIN EN 60617-3 (socket a half circle, plug a
# solid bar, 03-03-05 plug and socket, as in Siemens CM1N0113E); earth signs after DIN EN 60617-2 02-15-01 and 02-15-03
def cup(s, cx, cy, opening):
    """The contact of a socket: a half circle open towards 'down', 'left' or 'right'."""
    start = {'down': 0, 'left': 270, 'right': 90}[opening]
    return s.arc(cx, cy, 3.2, start, start + 180, w=0.35)


def terminal(s, x, y, d=1.6):
    return s.circle(x, y, d)


def standing_terminal(prefix, caption, **flags):
    """A terminal: the contact above, its lead down to the terminal circle."""
    s = Tagged(prefix, caption, **flags)
    terminal(s.line((0, 0), (0, P - 0.8)), 0, P)
    return s.pin('1', (0, 0), label=(0.8, 0.2), shown=True)


def plug_terminal(caption, mated):
    """Socket above (its lead to the contact), the plug below it with its lead to the next pitch."""
    s = Tagged('-X', caption)
    cup(s.line((0, 0), (0, P)), 0, P + 1.6, 'down')
    t = P + 0.9 if mated else P + 2.4
    s.rect(-0.6, t, 0.6, t + 1.6, fill='#000000').line((0, t + 1.6), (0, 3 * P))
    return left_tag(s.pin('1', (0, 0), label=(0.8, 0.2), shown=True), -2.2, P + 2.0)


def lying_connector(caption, draw):
    """A socket, plug or plug-and-socket lying to the right of its contact."""
    s = Tagged('X', caption)
    draw(s)
    s.pin('1', (0, 0), label=(0.4, -2.2), shown=True)
    return top_tag(s, 2 * P, -1.6, 1.6)


def socket(s, x=P):
    return cup(s.line((0, 0), (x, 0)), x + 1.6, 0, 'right')


def plug(s, x=P + 1.6):
    return s.line((0, 0), (x, 0)).rect(x, -0.6, x + 1.6, 0.6, fill='#000000')


def mated(s, reverse=False):
    """Plug and socket put together around x = 2 P: the plug's bar inside the socket's half circle."""
    c = 2 * P
    if reverse:
        cup(s.line((0, 0), (c - 1.6, 0)), c, 0, 'right')
        return s.rect(c - 0.8, -0.6, c + 0.8, 0.6, fill='#000000').line((c + 0.8, 0), (4 * P, 0))
    s.line((0, 0), (c - 0.8, 0)).rect(c - 0.8, -0.6, c + 0.8, 0.6, fill='#000000')
    return cup(s, c, 0, 'left').line((c + 1.6, 0), (4 * P, 0))


def device_connector(caption, female):
    """L, N and PE of a device's socket outlet or plug inside the device's dash-dot outline."""
    s = Tagged('-X', caption)
    for name, x in zip(('L', 'N', 'PE'), (0, 2 * P, 4 * P)):
        if female:
            cup(s.line((x, 0), (x, 2 * P)), x, 2 * P + 1.6, 'down')
        else:
            s.line((x, 0), (x, 2 * P + 0.2)).rect(x - 0.6, 2 * P + 0.2, x + 0.6, 2 * P + 1.8, fill='#000000')
        s.pin(name, (x, 0), label=(x + 0.6, 0.2), shown=True)
    styled(s.rect(-2.6, 2 * P - 0.9, 4 * P + 2.6, 2 * P + 2.7), 'dashDot')
    return left_tag(s, -3.6, 2 * P + 0.9)


def disconnect(caption, closed):
    """A disconnect terminal: two terminal circles, the link between them closed or swung open."""
    s = upright('-X', caption)
    terminal(terminal(leads(s, P - 0.7, 3 * P + 0.7), 0, P, 1.4), 0, 3 * P, 1.4)
    s.line((0, 3 * P - 0.7), (0, P + 0.7) if closed else (-1.7, P + 1.2), w=0.35)
    return left_tag(s, -2.6, 2 * P)


add = page(IEC, 'Klemmen, Stecker, Buchsen', 'Terminals, plugs, sockets', 'Bornes, fiches, prises')
add(left_tag(standing_terminal('-X', C('Klemme', 'Terminal', 'Borne')), -1.8, P))
add(plug_terminal(C('Steckklemme', 'Plug-in terminal', 'Borne enfichable'), False))
add(plug_terminal(C('Steckklemme (gesteckt)', 'Plug-in terminal (plugged)', 'Borne enfichable (enfichée)'), True))
s = Tagged('X', C('Geräteklemme', 'Device terminal', "Borne d'appareil"), 'Port[]')
terminal(s.line((0, 0), (0, P - 0.8)), 0, P)
add(left_tag(s.pin('1', (0, 0), label=(0.8, 0.2), shown=True, text='100'), -1.8, P))
add(lying_connector(C('Buchse', 'Socket', 'Prise'), socket))
add(lying_connector(C('Stecker', 'Plug', 'Fiche'), plug))
add(lying_connector(C('Steckverbindung', 'Plug and socket', 'Connecteur'), mated))
add(lying_connector(C('Steckverbindung (umgekehrt)', 'Plug and socket (reversed)', 'Connecteur (inversé)'), lambda s: mated(s, True)))
add(device_connector(C('Steckdose (Gerät)', 'Socket outlet (device)', 'Prise de courant (appareil)'), True))
add(device_connector(C('Stecker (Gerät)', 'Plug (device)', 'Fiche (appareil)'), False))
s = Tagged('PE', C('PE-Anschluss', 'PE connection', 'Raccordement PE'), numbered=False, listed=False)
s.line((0, 0), (0, P)).pin('1', (0, 0), label=(0.8, 0.2), shown=True)
for k, half in enumerate((2.0, 1.3, 0.6)):
    s.line((-half, P + k * 0.8), (half, P + k * 0.8), w=0.35)
add(left_tag(s, -2.6, P + 0.4))
s = Tagged('PE', C('PE-Geräteanschluss', 'PE device connection', "Raccordement PE d'appareil"), numbered=False, listed=False)
s.line((0, 0), (0, P)).rect(-2.4, P, 2.4, P + 0.8, fill='#000000').pin('1', (0, 0), label=(0.8, 0.2), shown=True)
add(left_tag(s, -3.0, P + 0.4))
s = Tagged('PE', C('PE-Klemme', 'PE terminal', 'Borne PE'), numbered=False)
terminal(s.line((0, 0), (P - 0.8, 0)), P, 0).pin('1', (0, 0), label=(0.4, -2.2), shown=True)
add(top_tag(pe_sign(s, P + 3.4, 0), P + 1.6, -2.0, 2.0))
s = pe_sign(standing_terminal('PE', C('PE-Klemme (senkrecht)', 'PE terminal (vertical)', 'Borne PE (verticale)'), numbered=False), 3.4, P)
add(left_tag(s, -2.2, P))
s = upright('-X', C('Sicherungsklemme', 'Fuse terminal', 'Borne à fusible'), '1,6 AT')
terminal(terminal(leads(s, P - 0.7, 3 * P + 0.7), 0, P, 1.4), 0, 3 * P, 1.4)
add(left_tag(s.line((0, P + 0.7), (0, 3 * P - 0.7)).rect(-0.8, P + 1.1, 0.8, 3 * P - 1.1), -2.2, 2 * P))
add(disconnect(C('Trennklemme', 'Disconnect terminal', 'Borne sectionnable'), True))
add(disconnect(C('Trennklemme (offen)', 'Disconnect terminal (open)', 'Borne sectionnable (ouverte)'), False))
add(lying_connector(C('Buchse (Variante)', 'Socket (variant)', 'Prise (variante)'),
                    lambda s: s.line((0, 0), (P, 0)).line((P + 1.6, -1.4), (P, 0), (P + 1.6, 1.4), w=0.35)))
add(lying_connector(C('Stecker (Variante)', 'Plug (variant)', 'Fiche (variante)'),
                    lambda s: s.line((0, 0), (P + 1.6, 0)).line((P + 0.4, -1.2), (P + 1.6, 0), (P + 0.4, 1.2), w=0.35)))
add(lying_connector(C('Steckverbindung (Variante)', 'Plug and socket (variant)', 'Connecteur (variante)'),
                    lambda s: s.line((0, 0), (2 * P, 0)).line((2 * P - 1.2, -1.2), (2 * P, 0), (2 * P - 1.2, 1.2), w=0.35)
                    .line((2 * P - 0.4, -1.2), (2 * P + 0.8, 0), (2 * P - 0.4, 1.2), w=0.35).line((2 * P + 0.8, 0), (4 * P, 0))))


# --- Page "Melder, Anzeigen, Lampen": signalling devices, meters and lamps after DIN EN 60617-8, standing
def round_part(prefix, caption, d=D1):
    """A part in a circle between the leads 1 and 2."""
    s = upright(prefix, caption)
    return leads(s, 2 * P - d / 2, 2 * P + d / 2).circle(0, 2 * P, d)


def fed(s, x, gap=0.9):
    """Lead 1 from above and lead 2 from below bent into the side of a part whose inputs are at x, `gap` either
    side of the middle (the acoustic signals, drawn as in the standard with both leads on one side)."""
    return s.line((0, 0), (0, 2 * P - gap), (x, 2 * P - gap)).line((0, 4 * P), (0, 2 * P + gap), (x, 2 * P + gap))


def integrating(caption, unit):
    """An integrating meter: a rectangle with the band of the counter at its top and the unit below it."""
    s = leads(upright('-P', caption), P, 3 * P).rect(-P, P, P, 3 * P).line((-P, P + 1.1), (P, P + 1.1))
    if unit:
        s.text(unit, 0, 2 * P - 0.6, 2.2, 'centre')
    return left_tag(s, -P - 1.0, 2 * P)


def heater(s, y0, y1, half=1.4):
    """A heating element: a rectangle divided by three bars."""
    s.rect(-half, y0, half, y1)
    for k in (1, 2, 3):
        y = y0 + (y1 - y0) * k / 4
        s.line((-half, y), (half, y))
    return s


def tube(s):
    """A fluorescent lamp: the tube with an electrode at each end, the leads entering to the electrodes."""
    s.rect(-1.6, P, 1.6, 3 * P).line((-1.0, P + 0.7), (1.0, P + 0.7), w=0.35).line((-1.0, 3 * P - 0.7), (1.0, 3 * P - 0.7), w=0.35)
    return leads(s, P + 0.7, 3 * P - 0.7)


def coil_box(s):
    """The operating coil A1-A2 (as on the page "Schütze + Kontakte [K]")."""
    s.rect(-2 * P, P, 2 * P, 3 * P, w=0.35).line((0, 0), (0, P)).line((0, 3 * P), (0, 4 * P))
    return s.pin('A1', (0, 0), label=(0.6, 0.2), shown=True).pin('A2', (0, 4 * P), label=(0.6, 4 * P - 2.4), shown=True)


add = page(IEC, 'Melder, Anzeigen, Lampen', 'Signalling devices, indicators, lamps', 'Signalisation, indicateurs, lampes')
s = coil_box(Tagged('-P', C('Mechanischer Zähler', 'Electromechanical counter', 'Compteur électromécanique')))
add(left_tag(s.rect(2 * P, P, 4 * P, 3 * P, w=0.35), -2 * P - 1.0, 2 * P))
add(left_tag(lamp(round_part('-P', C('Lampe', 'Lamp', 'Lampe')), 0, 2 * P), -2.4, 2 * P))
# The flashing signal lamp of DIN EN 60617-8 08-10-02 (Siemens CM1N0113E): the lamp with a pulse beside it.
s = lamp(round_part('-P', C('Blitzleuchte', 'Flashing light', 'Feu à éclats')), 0, 2 * P)
add(left_tag(pulse_train(s, 2.8, 2 * P + 0.6, 1, 2.4, 1.2), -2.4, 2 * P))
s = lamp(round_part('-P', C('Rundumleuchte', 'Rotating beacon', 'Gyrophare')), 0, 2 * P)
R = 3.4
s.arc(0, 2 * P, 2 * R, 330, 400)
end, u = (R * math.cos(math.radians(40)), 2 * P - R * math.sin(math.radians(40))), (-math.sin(math.radians(40)), -math.cos(math.radians(40)))
add(left_tag(s.line((end[0] - 0.4 * u[0], end[1] - 0.4 * u[1]), (end[0] + 0.6 * u[0], end[1] + 0.6 * u[1]), end='arrow', size=1.0), -2.4, 2 * P))
s = upright('-P', C('Glimmlampe', 'Glow lamp', 'Lampe à lueur'))
leads(s, 2 * P - 0.7, 2 * P + 0.7).circle(0, 2 * P, D1)
s.line((-1.3, 2 * P - 0.7), (1.3, 2 * P - 0.7), w=0.35).line((-1.3, 2 * P + 0.7), (1.3, 2 * P + 0.7), w=0.35)
add(left_tag(s.circle(1.65, 2 * P, 0.5, fill='#000000'), -2.4, 2 * P))
add(led('-P', C('Leuchtdiode', 'Light-emitting diode', 'Diode électroluminescente'), value=''))
add(led('-P', C('Leuchtdiode (umgekehrt)', 'Light-emitting diode (reversed)', 'Diode électroluminescente (inversée)'), True, ''))
s = fed(upright('-P', C('Sirene', 'Siren', 'Sirène')), 1.6)
add(left_tag(s.poly((1.6, 2 * P - 2.4), (1.6, 2 * P + 2.4), (5.8, 2 * P), fill=None), -1.0, 2 * P))
s = fed(upright('-P', C('Summer', 'Buzzer', 'Ronfleur')), 4.0 - math.sqrt(2.4 ** 2 - 0.9 ** 2))
add(left_tag(s.arc(4.0, 2 * P, 4.8, 90, 270).line((4.0, 2 * P - 2.4), (4.0, 2 * P + 2.4)), -1.0, 2 * P))
s = fed(upright('-P', C('Horn, Hupe', 'Horn', 'Avertisseur sonore')), 1.6, 0.6)
s.rect(1.6, 2 * P - 1.3, 4.2, 2 * P + 1.3).poly((4.2, 2 * P - 0.5), (8.0, 2 * P - 2.0), (7.4, 2 * P + 1.8), (4.2, 2 * P + 0.5), fill=None)
add(left_tag(s, -1.0, 2 * P))
add(integrating(C('Amperestundenzähler', 'Ampere-hour meter', 'Ampère-heuremètre'), 'Ah'))
add(integrating(C('Betriebsstundenzähler', 'Hour meter', 'Compteur horaire'), 'h'))
add(integrating(C('Zähler (allgemein)', 'Meter (integrating, general)', 'Compteur (général)'), ''))
add(integrating(C('Wattstundenzähler', 'Watt-hour meter', 'Wattheuremètre'), 'Wh'))
add(left_tag(tube(upright('-E', C('Leuchtstofflampe', 'Fluorescent lamp', 'Lampe fluorescente'))), -2.6, 2 * P))
s = tube(Tagged('-E', C('Leuchtstofflampe mit Starter', 'Fluorescent lamp with starter', 'Lampe fluorescente avec starter')))
s.pin('1', (0, 0), label=(0.8, 0.2), shown=True).pin('2', (0, 4 * P), label=(0.8, 4 * P - 2.4), shown=True)
x = 2 * P
s.line((1.0, P + 0.7), (x, P + 0.7), (x, 2 * P - 1.2)).line((x, 2 * P + 1.2), (x, 3 * P - 0.7), (1.0, 3 * P - 0.7))
s.circle(x, 2 * P, 2.4).line((x - 0.7, 2 * P - 0.4), (x + 0.7, 2 * P - 0.4), w=0.35).line((x - 0.7, 2 * P + 0.4), (x + 0.7, 2 * P + 0.4), (x + 0.7, 2 * P - 0.1), w=0.35)
s.line((x, 2 * P - 1.2), (x, 2 * P - 0.4)).line((x, 2 * P + 1.2), (x, 2 * P + 0.4))
dot(s.line((x, 0), (x, P + 0.7)), x, P + 0.7).pin('3', (x, 0), label=(x + 0.6, 0.2), shown=True)
add(left_tag(s, -2.6, 2 * P))
add(left_tag(heater(leads(upright('-E', C('Heizung', 'Heater', 'Chauffage')), P, 3 * P), P, 3 * P), -2.4, 2 * P))
s = upright('-E', C('Heizung mit Thermostat', 'Heater with thermostat', 'Chauffage avec thermostat'), length=8 * P)
s.line((0, 0), (0, P)).line((0, P), (1.6, P)).line((0, 3 * P), (2.1, P - 0.4)).line((0, 3 * P), (0, 5 * P))
ly = 2 * P - 0.5
bx = 2.1 * (3 * P - ly) / (3 * P - (P - 0.4))
dashed(s, (bx, ly), (-3.0, ly)).text('ϑ', -3.2, ly - 1.3, 2.4, 'right')
dot(s.line((0, 4 * P), (2 * P, 4 * P)), 0, 4 * P).pin('3', (2 * P, 4 * P), label=(2 * P - 0.2, 4 * P - 2.2), align='right', shown=True)
add(left_tag(heater(s.line((0, 7 * P), (0, 8 * P)), 5 * P, 7 * P), -2.4, 5.5 * P))
add(left_tag(round_part('-P', C('Spannungsmesser', 'Voltmeter', 'Voltmètre')).text('V', 0, 2 * P - 1.5, 3.0, 'centre'), -2.4, 2 * P))
add(left_tag(round_part('-P', C('Strommesser', 'Ammeter', 'Ampèremètre')).text('A', 0, 2 * P - 1.5, 3.0, 'centre'), -2.4, 2 * P))
s = round_part('-P', C('Uhr', 'Clock', 'Horloge'))
add(left_tag(s.line((0, 2 * P - 1.5), (0, 2 * P), (1.2, 2 * P), w=0.35), -2.4, 2 * P))


# --- Page "Motoren, Ventile, Magnete": machines after DIN EN 60617-6, coils with their function after DIN EN 60617-7
def machine_text(s, cx, cy, sign, d):
    """M and the kind of current; 'dc' draws the sign ⎓."""
    big = d > D1
    s.text('M', cx, cy - (3.6 if big else 2.5), 3.0 if big else 2.0, 'centre')
    y = cy + (0.6 if big else 0.4)
    if sign == 'dc':
        s.line((cx - 1.1, y), (cx + 1.1, y))
        for k in range(3):
            s.line((cx - 1.1 + 0.85 * k, y + 0.6), (cx - 0.6 + 0.85 * k, y + 0.6))
    else:
        s.text(sign, cx, y - (0.4 if big else 0.6), 2.5 if big else 2.0, 'centre')
    return s


def edge(d, dx):
    return math.sqrt((d / 2) ** 2 - dx ** 2)


def inline_motor(caption, sign, pe):
    s = upright('-M', caption)
    machine_text(leads(s, 2 * P - D1 / 2, 2 * P + D1 / 2).circle(0, 2 * P, D1), 0, 2 * P, sign, D1)
    if pe:
        s.line((D1 / 2, 2 * P), (2 * P, 2 * P)).pin('PE', (2 * P, 2 * P), label=(2 * P - 0.2, 2 * P - 2.2), align='right', shown=True)
    return left_tag(s, -D1 / 2 - 1.0, 2 * P)


def row(s, names, xs, cx, cy, y, d=D3):
    """Leads from the circle straight up (y above it) or down to the contacts, the names beyond their ends."""
    up = y < cy
    for name, x in zip(names, xs):
        e = edge(d, x - cx)
        s.line((x, y), (x, cy - e if up else cy + e))
        s.pin(name, (x, y), label=(x + 0.3, y - 2.4) if up else (x + 0.3, y + 0.6), shown=True)
    return s


def pe_beside(s, x, cx, cy, y, d=D3):
    """PE from the right of the circle to x and up or down to y."""
    s.line((cx + d / 2, cy), (x, cy), (x, y))
    up = y < cy
    return s.pin('PE', (x, y), label=(x + 0.3, y - 2.4) if up else (x + 0.3, y + 0.6), shown=True)


def motor(caption, sign, names, at_top, d=D3):
    """Terminals in a row above the circle (at_top) or below it, PE at the end of the row."""
    s = Tagged('-M', caption)
    cy = 3 * P if at_top else 0
    y = 0 if at_top else 3 * P
    s.circle(P, cy, d)
    machine_text(s, P, cy, sign, d)
    row(s, names, (0, P, 2 * P)[:len(names)], P, cy, y, d)
    if len(names) == 3:
        pe_beside(s, 4 * P, P, cy, y, d)
    else:
        row(s, ('PE',), (2 * P,), P, cy, y, d)
    return left_tag(s, P - d / 2 - 1.0, cy)


def field_coil(prefix, caption, sign):
    """The operating coil with a field to its right showing what it works on."""
    s = coil_box(Tagged(prefix, caption)).rect(2 * P, P, 4 * P, 3 * P, w=0.35)
    sign(s, 3 * P, 2 * P)
    return left_tag(s, -2 * P - 1.0, 2 * P)


def valve_sign(s, cx, cy):
    return s.poly((cx - 1.8, cy - 1.2), (cx - 1.8, cy + 1.2), (cx, cy), fill=None).poly((cx + 1.8, cy - 1.2), (cx + 1.8, cy + 1.2), (cx, cy), fill=None)


def brake_sign(s, cx, cy):
    """A drum with the brake shoe pressed on it."""
    return s.circle(cx, cy + 0.5, 2.4).rect(cx - 1.0, cy - 1.6, cx + 1.0, cy - 0.7, fill='#000000')


def clutch_sign(s, cx, cy):
    """The two halves of a coupling on the shaft, disengaged."""
    s.line((cx - 2.0, cy), (cx - 0.4, cy)).line((cx + 0.4, cy), (cx + 2.0, cy))
    return s.line((cx - 0.4, cy - 1.4), (cx - 0.4, cy + 1.4), w=THICK).line((cx + 0.4, cy - 1.4), (cx + 0.4, cy + 1.4), w=THICK)


def latch_sign(s, cx, cy):
    """The latching device, engaged (DIN EN 60617-2 02-12-13, as in Siemens CM1N0113E): the tooth on the link with the
    pawl above it."""
    y, x = cy + 0.7, cx - 0.6
    dashed(s, (cx - 2.2, y), (cx + 2.2, y))
    return s.line((x, y), (x, y - 1.0), (x + 1.8, y)).line((x - 0.3, y - 1.1), (x - 0.3, y - 2.4))


add = page(IEC, 'Motoren, Ventile, Magnete', 'Motors, valves, magnets', 'Moteurs, vannes, électroaimants')
add(inline_motor(C('Gleichstrommotor', 'DC motor', 'Moteur à courant continu'), 'dc', False))
add(inline_motor(C('Gleichstrommotor mit PE', 'DC motor with PE', 'Moteur à courant continu avec PE'), 'dc', True))
add(inline_motor(C('Wechselstrommotor mit PE', 'AC motor with PE', 'Moteur monophasé avec PE'), '1~', True))
add(motor(C('Wechselstrommotor mit PE (Anschlüsse oben)', 'AC motor with PE (terminals at the top)', 'Moteur monophasé avec PE (bornes en haut)'),
          '1~', ('1', '2'), True))
add(motor(C('Drehstrommotor mit PE', 'Three-phase motor with PE', 'Moteur triphasé avec PE'), '3~', ('U1', 'V1', 'W1'), False))
add(motor(C('Drehstrommotor mit PE (Anschlüsse oben)', 'Three-phase motor with PE (terminals at the top)', 'Moteur triphasé avec PE (bornes en haut)'),
          '3~', ('U1', 'V1', 'W1'), True))
s = Tagged('-M', C('Drehstrommotor, sechs Anschlüsse, mit PE', 'Three-phase motor, six terminals, with PE', 'Moteur triphasé, six bornes, avec PE'))
machine_text(s.circle(P, 3 * P, D3), P, 3 * P, '3~', D3)
row(row(s, ('U1', 'V1', 'W1'), (0, P, 2 * P), P, 3 * P, 0), ('U2', 'V2', 'W2'), (0, P, 2 * P), P, 3 * P, 6 * P)
add(left_tag(pe_beside(s, 4 * P, P, 3 * P, 6 * P), P - D3 / 2 - 1.0, 3 * P))
DD = 6 * P - 0.6     # the circle of the pole-changing motor with six terminals in a row
# Dahlander winding Δ/YY; terminals of a pole-changing motor with the number of the speed first, after EN 60034-8 as
# given in the de.wikipedia article "Dahlanderschaltung".
s = Tagged('-M', C('Drehstrommotor, polumschaltbar', 'Three-phase motor, pole-changing', 'Moteur triphasé à changement de polarité'))
machine_text(s.circle(2.5 * P, 4 * P, DD), 2.5 * P, 4 * P, '3~', DD)
s.text('Δ/YY', 2.5 * P, 4 * P + 3.6, 2.0, 'centre')
row(s, ('1U', '1V', '1W', '2U', '2V', '2W'), [k * P for k in range(6)], 2.5 * P, 4 * P, 0, DD)
pe_beside(s, 7 * P, 2.5 * P, 4 * P, 0, DD)
for o in s.parts:
    if o['type'] == 'contact':
        o['font'] = {'height': 1.4}
add(left_tag(s, 2.5 * P - DD / 2 - 1.0, 4 * P))
add(field_coil('-K', C('Magnetventil', 'Solenoid valve', 'Électrovanne'), valve_sign))
add(field_coil('-Q', C('Magnetbremse', 'Electromagnetic brake', 'Frein électromagnétique'), brake_sign))
add(field_coil('-Q', C('Magnetkupplung', 'Electromagnetic clutch', 'Embrayage électromagnétique'), clutch_sign))
add(field_coil('-M', C('Auslösemagnet', 'Trip magnet', 'Électroaimant de déclenchement'), latch_sign))

# sPlan's roles: these devices are Parents, so that contacts drawn elsewhere can be linked to them as children.
parents(IEC, 'Diverses', 'Leuchttaster (kompakt)', 'Leuchttaster (Rahmen)', 'Schlüsselschalter mit Leuchte (kompakt)',
        'Schlüsselschalter mit Leuchte (Rahmen)')
parents(IEC, 'Elektronik', 'Drossel', 'Drossel mit Kern', 'Drossel mit Kern und Luftspalt')
parents(IEC, 'Generatoren, Umformer, Wandler')
parents(IEC, 'Melder, Anzeigen, Lampen', 'Mechanischer Zähler', 'Uhr')
parents(IEC, 'Motoren, Ventile, Magnete')
