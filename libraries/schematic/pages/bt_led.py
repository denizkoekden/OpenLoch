# LEDs for the page "LED": RGB LEDs after DIN EN 60617-5 like the existing "RGB-LED (gemeinsame Kathode)", a
# seven-segment display without a common lead, and a 5 mm LED (T-1 3/4, dome 5 mm, collar 5.8 mm, flat at the
# cathode) drawn as seen from the side (legs on the 2.54 mm pitch, anode leg longer) and from above.

RGB = (('R', '#d00000'), ('G', '#008000'), ('B', '#0040d0'))


def rgb_anode(caption, coloured):
    """Three LEDs side by side, anodes joined to A at the top left, cathodes R, G, B below."""
    s = Symbol('D', caption, 'RGB')
    s.line((0, -P), (0, 0), (4 * P, 0)).pin('A', (0, -P), label=(0.4, -P - 0.4))
    for k, (name, colour) in enumerate(RGB):
        x = 2 * k * P
        s.line((x, 0), (x, 1.5 * P)).line((x, 2.5 * P), (x, 4 * P))
        s.poly((x - 1.5, 1.5 * P), (x + 1.5, 1.5 * P), (x, 2.5 * P), fill=colour if coloured else None)
        s.line((x - 1.5, 2.5 * P), (x + 1.5, 2.5 * P), w=0.35).text(name, x + 0.4, 4 * P - 2.6, 1.8)
        s.pin(name, (x, 4 * P), label=(x + 0.4, 4 * P - 2.2))
    return s.labels((4 * P + 2.4, P - 0.6), (4 * P + 2.4, 2 * P + 0.4))


add = extend(BT, 'LED')
add(rgb_anode(C('RGB-LED (gemeinsame Anode)', 'RGB LED (common anode)', 'DEL RVB (anode commune)'), False))
add(rgb_anode(C('RGB-LED farbig (gemeinsame Anode)', 'RGB LED, coloured (common anode)', 'DEL RVB colorée (anode commune)'), True))

s = Symbol('D', C('RGB-LED farbig (gemeinsame Kathode)', 'RGB LED, coloured (common cathode)', 'DEL RVB colorée (cathode commune)'), 'RGB')
for k, (name, colour) in enumerate(RGB):
    y = k * P * 2
    s.line((0, y), (1.5 * P, y)).line((2.5 * P, y), (4 * P, y))
    diode_shape(s, 1.5 * P, 2.5 * P, fill=colour, y=y).text(name, 0.4, y - 2.6, 1.8)
    s.pin(name, (0, y))
s.line((4 * P, 0), (4 * P, 4 * P)).line((4 * P, 2 * P), (5 * P, 2 * P)).pin('K', (5 * P, 2 * P), label=(5 * P - 0.4, 2 * P - 2.2), align='right')
add(s.labels((5 * P + 0.6, -1.0), (5 * P + 0.6, 2.0)))

s = box('D', C('7-Segment-Anzeige', 'Seven-segment display', 'Afficheur 7 segments'), ['a', 'b', 'c', 'd', 'e', 'f', 'g', 'dp'], [], width=6 * P)
x, y, w, h = 4 * P, 2.0 * P, 2.2 * P, 2.0 * P     # the digit "8." as on the display of the page "Digital"
for (ax, ay), (bx, by) in (((0, 0), (1, 0)), ((1, 0), (1, 1)), ((1, 1), (1, 2)), ((0, 2), (1, 2)), ((0, 1), (0, 2)), ((0, 0), (0, 1)), ((0, 1), (1, 1))):
    s.line((x + ax * w, y + ay * h), (x + bx * w, y + by * h), w=THICK)
add(s.circle(x + w + 1.0, y + 2 * h, 0.8, fill='#000000'))

COLOURS = (('klar', 'clear', 'incolore', None), ('grau', 'grey', 'grise', '#b0b0b0'), ('rot', 'red', 'rouge', '#ff4040'),
           ('gelb', 'yellow', 'jaune', '#ffd030'), ('grün', 'green', 'verte', '#40c040'), ('blau', 'blue', 'bleue', '#4080ff'))


def arc_points(cx, cy, radius, start, stop, steps):
    return [(cx + radius * math.cos(math.radians(a)), cy - radius * math.sin(math.radians(a)))
            for a in (start + (stop - start) * k / steps for k in range(steps + 1))]


def led_side(caption, fill):
    """Anode leg (1) at the insertion point, cathode leg (2) one pitch shorter at the right; the collar ends at the body
    on the cathode side (the flat)."""
    s = Symbol('D', caption)
    c, base = P / 2, -3 * P             # the body's axis and the underside of the collar
    for x, y in ((0, 0), (P, -P)):
        s.line((x, base), (x, y), w=0.5)
        coloured(s, '#808080')
    s.rect(c - 2.9, base - 1.0, c + 2.5, base, fill=fill)
    s.poly((c - 2.5, base - 1.0), *arc_points(c, base - 6.2, 2.5, 180, 0, 16), (c + 2.5, base - 1.0), fill=fill)
    s.pin('1', (0, 0)).pin('2', (P, -P), label=(P + 0.4, -P - 2.2))
    return s.labels((c + 3.6, base - 8.0), (c + 3.6, base - 5.0))


def led_top(caption, fill):
    """The collar with its flat at the cathode (right), the dome inside, a highlight at the upper left."""
    s = Symbol('D', caption)
    a = math.degrees(math.acos(2.5 / 2.9))
    s.poly(*arc_points(0, 0, 2.9, a, 360 - a, 24), fill=fill)
    s.circle(0, 0, 5.0, fill=fill)
    s.arc(0, 0, 3.4, 105, 165, w=0.35)
    if fill:
        coloured(s, '#ffffff')
    return s.labels((3.6, -3.4), (3.6, -0.6))


for de, en, fr, fill in COLOURS:
    add(led_side(C(f'LED 5 mm (Seitenansicht, {de})', f'LED 5 mm (side view, {en})', f'DEL 5 mm (vue de côté, {fr})'), fill))
for de, en, fr, fill in COLOURS:
    add(led_top(C(f'LED 5 mm (Draufsicht, {de})', f'LED 5 mm (top view, {en})', f'DEL 5 mm (vue de dessus, {fr})'), fill))
