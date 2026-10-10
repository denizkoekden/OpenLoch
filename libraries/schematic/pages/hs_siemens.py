# Siemens automation devices: the page "Siemens LOGO" of the folder Elektroinstallation (designators without "-") and the
# pages "Siemens LOGO", "Siemens ET200", "Siemens NET", "Siemens S7-1200", "Siemens S7-300", "Siemens Bediengeräte" and
# "Netzteile" of "USER/Misc. (German)/sPlan - EU-Norm" (designators with the leading "-" of IEC 81346). Devices are
# front views in the style of the manufacturer's wiring diagrams: the housing, the terminals as small circles on its edge
# with their names, LEDs, type and order number as text, no logos. Terminal names after the Siemens manuals: LOGO!
# A5E03556174-01 (0BA2 devices: A5E00067780 01), LOGO!Power C98130-A7560-A999-1-7619, ET 200S A5E01638907-04 (IM151-3
# PN), A5E01119952-02 (PM-E DC24V), A5E00124881-05 (1SI), SCALANCE XB-000 A2B00077300-06, S7-1200 A5E02486680-05, CSM
# 1277 A2B00079397B, S7-300 A5E00105505-AK (signal modules) and A5E00105475-12 (CPUs), CP 343-2 C79000-G8976-C149-04,
# KTP600 Basic A5E02421799-01; the 24 V / 5 A supply after the Mean Well NDR-120-24 data sheet.
# Every wired terminal is a contact on the 2.54 mm pitch at the end of a lead P long. Protective devices and power
# supplies drawn as circuits after DIN EN 60617. Children take the parent's designator and value once linked.
U = 'USER/Misc. (German)/sPlan - EU-Norm'
CHILD_ID, CHILD_VALUE = '<PARENT_ID>', '<PARENT_VALUE>'
MINUS = '−'


def term(t):
    """A terminal as (contact name, text on the device); a plain name stands for both, its trailing "-" written as a
    minus sign."""
    if isinstance(t, tuple):
        return t
    return t, (t[:-1] + MINUS if t.endswith('-') else t)


EARTH = '⏚'           # stands for the functional earth terminal: drawn as a sign, its contact named FE


def earth(s, x, y):
    """The earth sign below (x, y), 1.6 high: a stub and three bars."""
    s.line((x, y), (x, y + 0.6))
    for k, half in enumerate((0.8, 0.5, 0.2)):
        s.line((x - half, y + 0.6 + k * 0.4), (x + half, y + 0.6 + k * 0.4))
    return s


def housing(s, x0, y0, x1, y1):
    return s.rect(x0, y0, x1, y1, w=0.35)


def row(s, x0, edge, terms, out=-1, step=2 * P, h=1.6):
    """Terminals along the housing edge y = edge, the k-th at x0 + k * step (None leaves the place free): a small circle
    on the edge and a lead P outwards (out -1: up, 1: down) to the connection point; the text inside the housing, the
    earth sign for a functional earth terminal."""
    for k, t in enumerate(terms):
        if t is None:
            continue
        name, text = term(t)
        x = x0 + k * step
        end = (x, edge + out * P)
        s.circle(x, edge, 1.4).line((x, edge + out * 0.7), end)
        if text == EARTH:
            earth(s, x, edge + 1.0 if out < 0 else edge - 2.6)
            text = 'FE'
        elif text:
            s.text(text, x, edge + 1.0 if out < 0 else edge - 1.0 - h, h, 'centre')
        s.pin(name, end, text=text or name)
    return s


def column(s, edge, y0, terms, out=1, step=P, h=1.3, gap=1.1):
    """Terminals down the housing edge x = edge, the k-th at y0 + k * step, leads P to the right (out 1) or the left
    (-1); the text inside beside the circle."""
    for k, t in enumerate(terms):
        if t is None:
            continue
        name, text = term(t)
        y = y0 + k * step
        end = (edge + out * P, y)
        s.circle(edge, y, 1.4).line((edge + out * 0.7, y), end)
        if text:
            s.text(text, edge - out * gap, y - h / 2, h, 'right' if out > 0 else 'left')
        s.pin(name, end, text=text)
    return s


def led(s, x, y, name=None, h=1.2):
    """An LED on a front: a small circle, its name at the right."""
    s.circle(x, y, 1.1, w=W)
    if name:
        s.text(name, x + 1.0, y - h / 2, h)
    return s


def rj45(s, x, y, name=None):
    """An RJ45 socket seen from the front, its upper left corner at (x, y), the name above it."""
    s.rect(x, y, x + 4.2, y + 3.6, w=0.25).rect(x + 1.3, y + 3.6 - 0.9, x + 2.9, y + 3.6, w=0.18)
    if name:
        s.text(name, x, y - 1.6, 1.2)
    return s


def dsub(s, cx, cy, name=None):
    """A small 9-pin D-sub socket (drawn only, no contacts)."""
    s.poly((cx - 3.2, cy - 1.5), (cx + 3.2, cy - 1.5), (cx + 2.5, cy + 1.5), (cx - 2.5, cy + 1.5), fill=None, w=0.25)
    for k in range(5):
        s.circle(cx - 2.0 + k, cy - 0.6, 0.4, w=0.12)
    for k in range(4):
        s.circle(cx - 1.5 + k, cy + 0.6, 0.4, w=0.12)
    if name:
        s.text(name, cx - 3.2, cy + 2.0, 1.2)
    return s


def slide(s, x, y):
    """The cover slide over a LOGO! expansion interface."""
    s.rect(x, y, x + 9.0, y + 2.6, w=0.25)
    for k in range(3):
        s.rect(x + 1.0 + k * 2.6, y + 0.5, x + 2.6 + k * 2.6, y + 2.1, w=0.18)
    return s


def strip(caption, terms, title=None, step=2 * P):
    """A parent's terminals as a child: a horizontal terminal strip, a cell per terminal with its number below and its
    function under the number, the connection point in the terminal (like the strips of the page "Klemmen").
    terms: (contact name, number, function)."""
    s = Symbol(CHILD_ID, caption, CHILD_VALUE, numbered=False)
    for k, (name, number, function) in enumerate(terms):
        x = k * step
        s.rect(x - step / 2, -P, x + step / 2, P).circle(x, 0, 1.4)
        if number:
            s.text(number, x, P + 0.4, 1.6, 'centre')
        if function:
            s.text(function, x, P + 2.4, 1.3, 'centre')
        s.pin(name, (x, 0), text=number or function)
    if title:
        s.text(title, (len(terms) - 0.5) * step + 1.0, -0.7, 1.4)
    return s.labels((-step / 2, -P - 3.4), (-step / 2, P + 4.4))


def dashed_box(s, x0, y0, x1, y1):
    s.rect(x0, y0, x1, y1)
    s.parts[-1]['pen']['style'] = 'dashDot'
    return s


def dot(s, x, y):
    return s.circle(x, y, 0.8, fill='#000000')


# --- LOGO! (manual A5E03556174-01, 04/2011: front views of chapter 1, wiring of chapter 2.3, order numbers appendix E)
def common_root(s, x, edge):
    """The two relay contacts of a LOGO! ...L output group on their common root: the root from the middle terminal at
    x + P, a make contact from it to each outer terminal (x, x + 2 P), drawn above the bottom edge."""
    top = edge - 2.6
    s.line((x + P, edge - 0.7), (x + P, top), (x + 0.5, top))
    s.line((x + P, top), (x + 2 * P - 0.5, top))
    for xs, side in ((x, 1), (x + 2 * P, -1)):
        s.line((xs, edge - 0.7), (xs, edge - 1.3), (xs + side * 0.6, edge - 1.3))
        s.line((xs + side * 0.5, top), (xs + side * 1.1, edge - 1.6))
    return s


def logo_module(d, caption, value, top, groups, texts, title, order, display=True, rows=13):
    """A LOGO! module: supply and inputs along the top edge 2 P apart, the outputs along the bottom edge in groups of
    terminals P apart, each group (terminals, [(place, name), ...]) with the output names over the terminal at `place`:
    a relay's contacts 1 and 2, a transistor output and its M, or the two relays of an ...L variant on their common
    root in the middle. Display and keys of a base module or the RUN/STOP LED of an expansion module in the middle.
    texts: supply, inputs, outputs."""
    s = Symbol(d + 'A', caption, value)
    wide = (len(top) - 1) * 2 * P
    size = max(len(terms) for terms, names in groups)
    step = (size + 3) * P if len(groups) <= 4 else (size + 1) * P
    span = (len(groups) - 1) * step + (size - 1) * P
    a0 = round((wide - span) / 2 / P) * P
    x0, x1 = min(0, a0) - 2.2, max(wide, a0 + span) + 2.2
    hgt = rows * P
    housing(s, x0, 0, x1, hgt).line((x0, 5.0), (x1, 5.0)).line((x0, hgt - 6.6), (x1, hgt - 6.6))
    row(s, 0, 0, top)
    s.text(texts[0], P, 3.0, 1.2, 'centre').text(texts[1], (4 * P + wide) / 2, 3.0, 1.2, 'centre')
    s.text(texts[2], (a0 + a0 + span) / 2, hgt - 6.2, 1.2, 'centre')
    for k, (terms, names) in enumerate(groups):
        x = a0 + k * step
        row(s, x, hgt, terms, 1, step=P, h=1.3)
        if len(terms) == 3:
            common_root(s, x, hgt)
        for place, name in names:
            s.text(name, x + place * P, hgt - 4.4, 1.4, 'centre')
    if display:
        xd = x0 + 2.4
        s.rect(xd, 7.0, xd + 20.0, 16.6, w=0.25)
        cx, cy = xd + 28.0, 10.6
        s.poly((cx - 0.9, cy - 1.2), (cx + 0.9, cy - 1.2), (cx, cy - 2.6), fill=None, w=0.18)
        s.poly((cx - 0.9, cy + 1.2), (cx + 0.9, cy + 1.2), (cx, cy + 2.6), fill=None, w=0.18)
        s.poly((cx - 1.4, cy - 0.9), (cx - 1.4, cy + 0.9), (cx - 2.8, cy), fill=None, w=0.18)
        s.poly((cx + 1.4, cy - 0.9), (cx + 1.4, cy + 0.9), (cx + 2.8, cy), fill=None, w=0.18)
        s.rect(cx - 5.4, 14.6, cx - 1.4, 16.6, w=0.25).text('ESC', cx - 3.4, 15.0, 1.2, 'centre')
        s.rect(cx + 1.4, 14.6, cx + 5.4, 16.6, w=0.25).text('OK', cx + 3.4, 15.0, 1.2, 'centre')
        s.text(title, xd, 18.0, 2.0, bold=True).text(order, xd, 20.8, 1.3)
    else:
        xd = x0 + 2.4
        slide(s, xd, 6.2)
        led(s, xd + 0.6, 11.0, 'RUN/STOP')
        s.text(title, xd, 13.2, 1.8, bold=True).text(order, xd, 15.8, 1.3)
    return s.labels((x1 + 1.0, 0.4), (x1 + 1.0, 3.4))


def relay_pairs(n):
    return [([(f'Q{k}-1', '1'), (f'Q{k}-2', '2')], [(0.5, f'Q{k}')]) for k in range(1, n + 1)]


def relay_roots(n):
    """The outputs of a LOGO! ...L variant: two relays per group of three terminals, their common root in the middle."""
    return [([(f'Q{k}', ''), (f'Q{k}/Q{k + 1}', ''), (f'Q{k + 1}', '')], [(0, f'Q{k}'), (2, f'Q{k + 1}')])
            for k in range(1, n + 1, 2)]


def logo_12_24rc(d):
    return logo_module(d, C('LOGO! 12/24RC', 'LOGO! 12/24RC', 'LOGO! 12/24RC'), 'LOGO! 12/24RC',
                       ['L+', 'M'] + [f'I{k}' for k in range(1, 9)], relay_pairs(4),
                       ('DC 12/24V', 'INPUT 8 x DC', 'OUTPUT 4 x RELAY / 10A'), 'LOGO! 12/24RC', '6ED1052-1MD00-0BA6')


def logo_24(d):
    # 6ED1052-1CC00-0BA5 is the LOGO! 24: four solid-state outputs, each with an M terminal beside it (manual, 2.3.4).
    return logo_module(d, C('LOGO! 12/24RC (Transistorausgänge)', 'LOGO! 24 (transistor outputs)', 'LOGO! 24 (sorties transistor)'),
                       'LOGO! 24', ['L+', 'M'] + [f'I{k}' for k in range(1, 9)],
                       [([f'Q{k}', (f'M/Q{k}', 'M')], []) for k in range(1, 5)],
                       ('DC 24V', 'INPUT 8 x DC', 'OUTPUT 4 x 24V / 0.3A'), 'LOGO! 24', '6ED1052-1CC00-0BA5')


def logo_24rcl(d):
    # LOGO! manual of the 0BA2 devices, A5E00067780 01 (05/2000): front view of LOGO! ...L in chapter 1, relay outputs
    # in 2.2.3: Q1/Q2, Q3/Q4, Q5/Q6 and Q7/Q8 each on three terminals, the common root of the pair in the middle.
    return logo_module(d, C('LOGO! 24RCL (0BA2)', 'LOGO! 24RCL (0BA2)', 'LOGO! 24RCL (0BA2)'), 'LOGO! 24RCL',
                       ['L+', 'M'] + [f'I{k}' for k in range(1, 13)], relay_roots(8),
                       ('DC 24V', 'INPUT 12 x DC', 'OUTPUT 8 x RELAY'), 'LOGO! 24RCL', '6ED1053-1HB00-0BA2')


def logo_dm16(d):
    return logo_module(d, C('LOGO! DM16 24R', 'LOGO! DM16 24R', 'LOGO! DM16 24R'), 'LOGO! DM16 24R',
                       ['L+', 'M'] + [f'I{k}' for k in range(1, 9)], relay_pairs(8),
                       ('DC 24V', 'INPUT 8 x DC', 'OUTPUT 8 x RELAY / 5A'), 'LOGO! DM16 24R', '6ED1055-1NB10-0BA0',
                       display=False, rows=11)


def logo_am2pt100(d):
    """The analogue module for two PT100: supply above, the PE terminal for the screens at the left, the sensor
    terminals below (manual, 2.3.3)."""
    s = Symbol(d + 'A', C('LOGO! AM2 PT100', 'LOGO! AM2 PT100', 'LOGO! AM2 PT100'), 'LOGO! AM2 PT100')
    x0, x1, hgt = -P, 11 * P, 12 * P
    housing(s, x0, 0, x1, hgt).line((x0, 5.0), (x1, 5.0)).line((x0, hgt - 6.6), (x1, hgt - 6.6))
    row(s, 0, 0, ['L+', 'M'])
    s.text('DC 12/24V', P, 3.0, 1.2, 'centre')
    row(s, 0, hgt, ['M1+', 'IC1', 'M1-', 'M2+', 'IC2', 'M2-'], 1)
    s.text('INPUT 2 x PT100', 5 * P, hgt - 6.2, 1.2, 'centre')
    slide(s, x0 + 2.4, 6.2)
    led(s, x0 + 3.0, 11.0, 'RUN/STOP')
    s.text('LOGO! AM2 PT100', x0 + 2.4, 12.8, 1.8, bold=True).text('6ED1055-1MD00-0BA0', x0 + 2.4, 15.2, 1.3)
    column(s, x0, 7 * P, ['PE'], out=-1, h=1.4)
    return s.labels((x1 + 1.0, 0.4), (x1 + 1.0, 3.4))


def logo_power12(d):
    """LOGO!Power 12 V / 1.9 A: all terminals along the top edge, the line input L1, N at the left, the output +, +, -,
    - at the right; the LED "output voltage OK" on the front, the potentiometer below it (operating instructions
    LOGO!Power C98130-A7560-A999-1-7619, 02.2011, figures 1-1 and 1-2)."""
    s = Symbol(d + 'G', C('LOGO!Power 12 V', 'LOGO!Power 12 V', 'LOGO!Power 12 V'), 'LOGO!Power 12V')
    x0, x1, hgt = -2.2, 12 * P + 2.2, 10 * P
    housing(s, x0, 0, x1, hgt).line((x0, 5.0), (x1, 5.0)).line((x0, hgt - 6.6), (x1, hgt - 6.6))
    row(s, 0, 0, ['L1', 'N', None, ('+1', '+'), ('+2', '+'), ('-1', MINUS), ('-2', MINUS)])
    s.text('AC 100-240V', P, 3.0, 1.2, 'centre').text('DC 12V 1.9A', 9 * P, 3.0, 1.2, 'centre')
    s.text('LOGO!Power', x0 + 1.6, 6.4, 1.8, bold=True).text('6EP1321-1SH03', x0 + 1.6, 9.0, 1.2)
    led(s, x1 - 4.0, 13.0, None).text('DC OK', x1 - 5.2, 12.4, 1.2, 'right')
    s.circle(x1 - 4.0, hgt - 3.3, 2.6, w=0.25).line((x1 - 4.8, hgt - 2.5), (x1 - 3.2, hgt - 4.1))
    s.text('U', x1 - 6.0, hgt - 4.0, 1.3, 'right')
    return s.labels((x1 + 1.0, 0.4), (x1 + 1.0, 3.4))


# Protective devices and supplies of the LOGO! page, schematic after DIN EN 60617
def varistor(s, x, y):
    """A varistor (VDR) from (x, y) down to (x, y + 5 P), its body between 2 P and 4 P: the resistor with the bent line
    of a voltage-dependent resistance."""
    s.line((x, y), (x, y + 2 * P)).rect(x - 1.0, y + 2 * P, x + 1.0, y + 4 * P).line((x, y + 4 * P), (x, y + 5 * P))
    s.line((x - 2.4, y + 2 * P - 0.6), (x - 2.4, y + 2 * P + 0.4), (x + 2.4, y + 4 * P + 0.2))
    return s.text('U', x + 1.4, y + 4 * P - 0.8, 1.4)


def spark_gap(s, x, y0, y1):
    """A spark gap: two arrowheads facing each other between y0 and y1."""
    m = (y0 + y1) / 2
    s.line((x, y0), (x, m - 0.4), end='arrow', size=1.2).line((x, y1), (x, m + 0.4), end='arrow', size=1.2)
    return s


def arrester(d, caption, gap):
    """Surge arrester: varistors from L1, L2 and L3 to a common point, which is N with a spark gap to PE ("3+1") or
    else PE itself."""
    s = Symbol(d + 'F', caption)
    for k, name in enumerate(('L1', 'L2', 'L3')):
        varistor(s, 3 * P * k, 0)
        s.pin(name, (3 * P * k, 0), label=(3 * P * k + 0.4, -2.2), shown=True)
    if gap:
        s.line((0, 5 * P), (9 * P, 5 * P)).line((9 * P, 0), (9 * P, 6 * P))
        dot(s, 3 * P, 5 * P)
        dot(s, 9 * P, 5 * P)
        spark_gap(s, 9 * P, 6 * P, 8 * P)
        s.line((9 * P, 8 * P), (9 * P, 9 * P))
        s.pin('N', (9 * P, 0), label=(9 * P + 0.4, -2.2), shown=True).pin('PE', (9 * P, 9 * P), label=(9 * P + 0.6, 9 * P - 0.4), shown=True)
        dashed_box(s, -3.2, P, 9 * P + 1.8, 8.5 * P)
        return s.labels((9 * P + 2.8, P), (9 * P + 2.8, 2 * P + 0.4))
    s.line((0, 5 * P), (6 * P, 5 * P)).line((3 * P, 5 * P), (3 * P, 6 * P))
    dot(s, 3 * P, 5 * P)
    s.pin('PE', (3 * P, 6 * P), label=(3 * P + 0.6, 6 * P - 0.4), shown=True)
    dashed_box(s, -3.2, P, 6 * P + 3.4, 5.5 * P)
    return s.labels((6 * P + 4.4, P), (6 * P + 4.4, 2 * P + 0.4))


def arrester_31(d):
    return arrester(d, C('Überspannungsableiter 3+1', 'Surge arrester 3+1', 'Parafoudre 3+1'), True)


def arrester_3(d):
    return arrester(d, C('Überspannungsableiter 3-polig', 'Surge arrester, three-pole', 'Parafoudre tripolaire'), False)


TIP = (-1.9, P + 0.6)       # free end of a make contact's blade, pivot at (x, 3 P)


def blade_at(x, y):
    return (x + TIP[0] * (3 * P - y) / (3 * P - TIP[1]), y)


def rcd_40(d):
    """Residual current device, four poles: make contacts with automatic release, the summation current transformer
    around all conductors and the release IΔ it feeds, acting on the contacts (DIN EN 60617-7)."""
    s = Symbol(d + 'F', C('FI-Schutzschalter 40 A / 30 mA', 'Residual current device 40 A / 30 mA',
                          'Interrupteur différentiel 40 A / 30 mA'), '40A/30mA')
    xs = [0, 2 * P, 4 * P, 6 * P]
    names = (('1', '1', '2', '2'), ('3', '3', '4', '4'), ('5', '5', '6', '6'), ('N1', 'N', 'N2', 'N'))
    ring, bottom = 5 * P, 7 * P
    for x, (a, ta, b, tb) in zip(xs, names):
        s.line((x, 0), (x, P)).line((x, 3 * P), (x, bottom)).line((x, 3 * P), (x + TIP[0], TIP[1]))
        tip = blade_at(x, 3.9)
        s.arrow(tip, (tip[0] - 2.6, tip[1]), size=0.9)
        s.pin(a, (x, 0), label=(x + 0.9, -0.2), shown=True, text=ta).pin(b, (x, bottom), label=(x + 0.8, bottom - 2.4), shown=True, text=tb)
    s.parts.append({'type': 'ellipse', 'centre': pt((3 * P, ring)), 'size': [r(6 * P + 3.2), 1.8], 'pen': {'width': W}})
    s.rect(-8.4, ring - 1.8, -4.4, ring + 1.8).text('IΔ', -6.4, ring - 1.2, 2.0, 'centre').line((-4.4, ring), (-1.6, ring))
    dashed(s, (-6.4, ring - 1.8), (-6.4, 2 * P + 0.6), blade_at(6 * P, 2 * P + 0.6))
    return s.labels((6 * P + 2.6, P + 0.4), (6 * P + 2.6, 2 * P + 0.6))


def overload_3(d):
    """Thermal overload relay, three poles: the thermal releases in one box (DIN EN 60617-7)."""
    s = Symbol(d + 'F', C('Motorschutzrelais (dreipolig)', 'Motor overload relay, three-pole', 'Relais de protection moteur tripolaire'))
    s.rect(-P, P, 5 * P, 3 * P)
    for k in range(3):
        x = 2 * P * k
        s.line((x, 0), (x, P + 1.0), (x - 1.2, P + 1.0), (x - 1.2, 3 * P - 1.0), (x, 3 * P - 1.0), (x, 4 * P))
        s.pin(str(2 * k + 1), (x, 0), label=(x + 0.6, 0.2), shown=True).pin(str(2 * k + 2), (x, 4 * P), label=(x + 0.6, 4 * P - 2.1), shown=True)
    return s.labels((5 * P + 0.8, P), (5 * P + 0.8, 2 * P + 0.4))


def terminal_5(d):
    """A five-pole terminal strip: three numbered outputs, N and PE; the connection point in each terminal."""
    s = Symbol(d + 'X', C('Klemme 5-polig', 'Terminal strip, 5 poles', 'Bornier 5 pôles'))
    for k, name in enumerate(('1', '2', '3', 'N', 'PE')):
        x = 2 * k * P
        s.rect(x - P, -P, x + P, P).circle(x, 0, 1.4).text(name, x, P + 0.4, 1.6, 'centre')
        s.pin(name, (x, 0))
    return s.labels((-P, -P - 3.4), (-P, P + 2.6))


def psu_5a(d):
    """A DIN-rail power supply 90-264 V AC to 24 V DC, 5 A, after the Mean Well NDR-120-24 (data sheet NDR-120-SPEC
    2026-04-03, front view and terminal assignment): the output TB2 above (1, 2 -V, 3, 4 +V), +V ADJ and the LED DC OK
    below it, the input TB1 at the bottom (1 earth, 2 AC/N, 3 AC/L)."""
    s = Symbol(d + 'G', C('Netzteil 24 V / 5 A', 'Power supply 24 V / 5 A', 'Alimentation 24 V / 5 A'), '24V/5A')
    x0, x1, hgt = -2.2, 6 * P + 2.2, 12 * P
    housing(s, x0, 0, x1, hgt).line((x0, 5.0), (x1, 5.0)).line((x0, hgt - 6.6), (x1, hgt - 6.6))
    row(s, 0, 0, [('-1', '−V'), ('-2', '−V'), ('+1', '+V'), ('+2', '+V')])
    s.text('DC 24V 5A', 3 * P, 3.0, 1.2, 'centre')
    row(s, P, hgt, ['PE', 'N', 'L'], 1)
    s.text('AC 90-264V', 3 * P, hgt - 6.2, 1.2, 'centre')
    s.circle(x1 - 3.4, 8.2, 2.6, w=0.25).line((x1 - 4.2, 9.0), (x1 - 2.6, 7.4)).text('+V ADJ.', x1 - 5.0, 7.5, 1.2, 'right')
    led(s, x1 - 3.4, 11.8, None).text('DC OK', x1 - 5.0, 11.2, 1.2, 'right')
    s.text('NDR-120-24', x0 + 1.6, 15.6, 1.8, bold=True)
    return s.labels((x1 + 1.0, 0.4), (x1 + 1.0, 3.4))


def sr20b(d):
    """The power controller SR20b as a box: mains PE, L, N above, the output 0-24 V DC below, the control input
    0-10 V at the right (terminal names after the description; no data sheet found)."""
    s = Symbol(d + 'N', C('Regelgerät SR20b', 'Power controller SR20b', 'Régulateur de puissance SR20b'), 'SR20b')
    x0, x1, hgt = -2.2, 6 * P, 10 * P
    housing(s, x0, 0, x1, hgt)
    row(s, 0, 0, ['PE', 'L', 'N'])
    row(s, 0, hgt, ['+', '-'], 1)
    column(s, x1, 6 * P, [('0-10V', '0–10V')], out=1)
    s.text('SR20b', x0 + 1.4, 4.4, 2.0, bold=True).text('24V / 4,5A', x0 + 1.4, 7.4, 1.3)
    s.text('OUT 0–24V', x0 + 1.4, hgt - 4.8, 1.2)
    return s.labels((x1 + 1.0, 0.4), (x1 + 1.0, 3.4))


def psu_circuit(d):
    """A power supply drawn as its circuit: transformer (PE on the core), bridge rectifier and two smoothing
    capacitors; the outputs L+ twice and L- three times."""
    s = Symbol(d + 'T', C('Netzteil (Schaltung)', 'Power supply (circuit)', 'Alimentation (schéma)'))
    transformer_core(s)
    # bridge: AC at the top and bottom corner, + at the right, - at the left
    top, bottom, left, right = (6 * P, 0), (6 * P, 4 * P), (4 * P, 2 * P), (8 * P, 2 * P)
    for a, b in ((left, top), (left, bottom), (top, right), (bottom, right)):
        s.line(a, b)
        mx, my = (a[0] + b[0]) / 2, (a[1] + b[1]) / 2
        ang = math.degrees(math.atan2(-(b[1] - a[1]), b[0] - a[0]))
        s.poly(rot((mx - 0.9, my - 1.0), (mx, my), ang), rot((mx - 0.9, my + 1.0), (mx, my), ang), rot((mx + 0.9, my), (mx, my), ang), fill=None)
        s.line(rot((mx + 0.9, my - 1.0), (mx, my), ang), rot((mx + 0.9, my + 1.0), (mx, my), ang), w=0.35)
    s.line((3 * P, P), (3 * P, 0), top).line((3 * P, 3 * P), (3 * P, 4 * P), bottom)
    s.line(right, (9 * P, 2 * P), (9 * P, 0), (14 * P, 0))           # + rail
    s.line(left, (4 * P, 6 * P), (14 * P, 6 * P))                      # - rail, crossing the AC lead without a joint
    for x, polar in ((10 * P, True), (12 * P, False)):
        s.line((x, 0), (x, 3 * P - 0.5)).line((x, 3 * P + 0.5), (x, 6 * P))
        if polar:
            s.rect(x - 2.2, 3 * P - 1.1, x + 2.2, 3 * P - 0.5).rect(x - 2.2, 3 * P + 0.5, x + 2.2, 3 * P + 1.1, fill='#000000')
            s.text('+', x + 2.4, 3 * P - 3.0, 1.8)
        else:
            s.line((x - 2.2, 3 * P - 0.5), (x + 2.2, 3 * P - 0.5), w=THICK).line((x - 2.2, 3 * P + 0.5), (x + 2.2, 3 * P + 0.5), w=THICK)
        dot(s, x, 0)
        dot(s, x, 6 * P)
    s.line((13 * P, 0), (13 * P, P), (14 * P, P)).line((13 * P, 5 * P), (13 * P, 7 * P)).line((13 * P, 5 * P), (14 * P, 5 * P)).line((13 * P, 7 * P), (14 * P, 7 * P))
    dot(s, 13 * P, 0)
    dot(s, 13 * P, 6 * P)
    for name, text, y in (('L+1', 'L+', 0), ('L+2', 'L+', P), ('L-1', 'L−', 5 * P), ('L-2', 'L−', 6 * P), ('L-3', 'L−', 7 * P)):
        s.pin(name, (14 * P, y), label=(14 * P + 0.4, y - 1.0), shown=True, text=text)
    return s.labels((5 * P, -6.0), (5 * P, -3.2))


def transformer_core(s):
    """The transformer of psu_circuit: primary L1-N at the left, its core with the PE connection, secondary at 3 P."""
    s.line((0, 0), (P, 0), (P, P)).line((P, 3 * P), (P, 4 * P), (0, 4 * P))
    for k in range(4):
        s.arc(P, P + (k + 0.5) * P / 2, P / 2, 270, 450)
        s.arc(3 * P, P + (k + 0.5) * P / 2, P / 2, 90, 270)
    s.line((2 * P - 0.6, 0.6 * P), (2 * P - 0.6, 3.4 * P), w=THICK).line((2 * P + 0.6, 0.6 * P), (2 * P + 0.6, 3.4 * P), w=THICK)
    s.line((2 * P - 0.6, 3.4 * P), (2 * P + 0.6, 3.4 * P)).line((2 * P, 3.4 * P), (2 * P, 6 * P), (0, 6 * P))
    s.pin('L1', (0, 0), label=(-0.4, -2.2), align='right', shown=True).pin('N', (0, 4 * P), label=(-0.4, 4 * P - 2.2), align='right', shown=True)
    s.pin('PE', (0, 6 * P), label=(-0.4, 6 * P - 2.2), align='right', shown=True)
    return s


LOGO_PARTS = {'rc': logo_12_24rc, 'am2': logo_am2pt100, 'power': logo_power12, 'arr31': arrester_31, 'arr3': arrester_3,
              'rcl': logo_24rcl, 'rcd': rcd_40, 'psu': psu_5a, 'sr20b': sr20b, 'l24': logo_24, 'circuit': psu_circuit,
              'dm16': logo_dm16, 'x5': terminal_5, 'overload': overload_3}
add = page(EI, 'Siemens LOGO', 'Siemens LOGO', 'Siemens LOGO')
for key in ('rc', 'am2', 'power', 'arr31', 'arr3', 'rcl', 'rcd', 'psu', 'sr20b', 'l24', 'circuit', 'dm16', 'x5', 'overload'):
    add(LOGO_PARTS[key](''))
add = page(U, 'Siemens LOGO', 'Siemens LOGO', 'Siemens LOGO')
for key in ('rc', 'am2', 'power', 'arr31', 'rcl', 'rcd', 'psu', 'sr20b', 'l24', 'circuit', 'dm16', 'x5', 'overload', 'arr3'):
    add(LOGO_PARTS[key]('-'))


# --- ET 200S: interface module IM151-3 PN (A5E01638907-04, table 1-1), power module PM-E DC24V (A5E01119952-02) and the
# serial interface module 1SI (A5E00124881-05, table 2-3, RS 232C). An electronic module stands on its terminal module:
# terminals 1 to 4 down the left column, 5 to 8 down the right one.
def et200s_module(caption, title, order, leds, left, right):
    """A 15 mm module of the ET 200S on its terminal module: LEDs, type and order number written upwards; the terminal
    module's columns with the numbers inside and the functions at the leads. left, right: (contact name, number,
    function) for the rows 1 to 4, None where a terminal is missing."""
    s = Symbol('-A', caption, title)
    s.parent = True
    x1, hgt = 6 * P, 21 * P
    housing(s, 0, 0, x1, hgt).line((0, 12.5 * P), (x1, 12.5 * P))
    for k, name in enumerate(leds):
        led(s, 1.4, 1.6 + k * 1.9, name)
    s.text(title, 2.4 * P, 12 * P, 1.8, bold=True, rotation=90).text(order, 4.0 * P, 12 * P, 1.2, rotation=90)
    for side, items in ((-1, left), (1, right)):
        edge = 0 if side < 0 else x1
        for k, item in enumerate(items):
            if item is None:
                continue
            name, number, function = item
            y = (14 + 2 * k) * P
            column(s, edge, y, [(name, number)], out=side, h=1.4)
            if function:
                s.text(function, edge + side * 0.9, y - 2.0, 1.2, 'right' if side < 0 else 'left')
    return s.labels((x1 + P + 1.0, 0.4), (x1 + P + 1.0, 3.4))


add = page(U, 'Siemens ET200', 'Siemens ET200', 'Siemens ET200')
s = Symbol('-A', C('ET 200S IM151-3 PN', 'ET 200S IM151-3 PN', 'ET 200S IM151-3 PN'), 'IM151-3 PN')
s.parent = True
housing(s, 0, 0, 12 * P, 15 * P).line((0, 10.4 * P), (12 * P, 10.4 * P))
for k, name in enumerate(('SF', 'BF', 'MAINT', 'ON')):
    led(s, 1.6, 1.8 + k * 2.0, name)
s.text('IM151-3 PN', 5.4 * P, 1.0, 1.8, bold=True).text('6ES7151-3AA23-0AB0', 5.4 * P, 3.6, 1.2)
s.rect(5.4 * P, 6.0, 11 * P, 7.2, w=0.18).text('MMC', 5.4 * P, 7.6, 1.1)
for k, port in enumerate(('X1 P1', 'X1 P2')):
    y = 5 * P + k * 3 * P
    rj45(s, 1.4, y, port)
    led(s, 1.4 + 6.0, y + 1.0, 'LINK')
s.text('DC 24V', 6 * P, 11 * P, 1.2, 'centre')
column(s, 0, 12 * P, ['1L+', None, '2L+'], out=-1, h=1.4)
column(s, 12 * P, 12 * P, ['1M', None, '2M'], out=1, h=1.4)
add(s.labels((13 * P + 1.0, 0.4), (13 * P + 1.0, 3.4)))
add(et200s_module(C('ET 200S PM-E DC24V', 'ET 200S PM-E DC24V', 'ET 200S PM-E DC24V'), 'PM-E DC24V', '6ES7138-4CA01-0AA0',
                  ('SF', 'PWR'), [None, ('2', '2', 'L+'), ('3', '3', 'M'), ('A4', 'A4', 'AUX1')],
                  [None, ('6', '6', 'L+'), ('7', '7', 'M'), ('A8', 'A8', 'AUX1')]))
SI = [('1', 'TXD'), ('2', 'RTS'), ('3', 'DTR'), ('4', 'DCD'), ('5', 'RXD'), ('6', 'CTS'), ('7', 'DSR'), ('8', 'PE')]
add(et200s_module(C('ET 200S Modbus/USS-Modul (ISI 3964/ASCII)', 'ET 200S Modbus/USS module (1SI 3964/ASCII)',
                    'ET 200S module Modbus/USS (1SI 3964/ASCII)'), '1SI 3964/ASCII', '6ES7138-4DF01-0AB0', ('SF', 'TX', 'RX'),
                  [(n, n, f) for n, f in SI[:4]], [(n, n, f) for n, f in SI[4:]]))
add(strip(C('IM151 (Klemmen)', 'IM151 (terminals)', 'IM151 (bornes)'), [(n, n, '') for n in ('1L+', '2L+', '1M', '2M')], 'IM151-3 PN'))
add(strip(C('PM-E (Klemmen)', 'PM-E (terminals)', 'PM-E (bornes)'),
          [('2', '2', 'L+'), ('3', '3', 'M'), ('A4', 'A4', 'AUX1'), ('6', '6', 'L+'), ('7', '7', 'M'), ('A8', 'A8', 'AUX1')], 'PM-E DC24V'))
add(strip(C('ISI (Klemmen)', '1SI (terminals)', '1SI (bornes)'), [(n, n, f) for n, f in SI], '1SI'))


# --- SCALANCE XB005 (operating instructions A2B00077300-06: terminal block pin 1 functional ground, 2 M, 3 L+)
add = page(U, 'Siemens NET', 'Siemens NET', 'Siemens NET')
s = Symbol('-A', C('SCALANCE XB005', 'SCALANCE XB005', 'SCALANCE XB005'), 'XB005')
s.parent = True
housing(s, 0, 0, 10 * P, 16 * P)
s.text('SCALANCE XB005', 0.8, 0.8, 1.6, bold=True).text('6GK5005-0BA00-1AB2', 0.8, 3.0, 1.2)
led(s, 8 * P, 3.6, 'L')
for k, port in enumerate(('P1', 'P2', 'P3', 'P4', 'P5')):
    rj45(s, 1.4 + (k % 2) * 5 * P, 3.6 * P + (k // 2) * 3.4 * P, port)
row(s, 3 * P, 16 * P, [('FE', EARTH), 'M', 'L+'], 1)
s.text('24 V DC', 5 * P, 16 * P - 4.6, 1.2, 'centre')
add(s.labels((10 * P + 1.0, 0.4), (10 * P + 1.0, 3.4)))
add(strip(C('SCALANCE XB005 (Versorgung)', 'SCALANCE XB005 (supply)', 'SCALANCE XB005 (alimentation)'),
          [('FE', '1', 'FE'), ('M', '2', 'M'), ('L+', '3', 'L+')], 'XB005'))


# --- S7-1200 (system manual A5E02486680-05: wiring diagrams of appendix A, connector pins of the CM 1241 in A.10.4).
# The terminals of a block stand P apart; the device's two terminal rows of a signal module are drawn as one row.
def s71200(caption, value, title, lines, order, top, bottom, bottom_x=0, hgt=10 * P, groups=(), port=False, leds=()):
    s = Symbol('-NU', caption, value)
    wide = max((len(top) - 1) * P, bottom_x + (len(bottom) - 1) * P)
    x0, x1 = -2.2, wide + 2.2
    housing(s, x0, 0, x1, hgt)
    if top:
        s.line((x0, 4.6), (x1, 4.6))
    if bottom:
        s.line((x0, hgt - 4.6), (x1, hgt - 4.6))
    row(s, 0, 0, top, step=P, h=1.3)
    row(s, bottom_x, hgt, bottom, 1, step=P, h=1.3)
    for text, x, below in groups:
        s.text(text, x, 2.9 if not below else hgt - 4.2, 1.1, 'centre')
    for k, name in enumerate(leds):
        led(s, x0 + 2.0, 6.6 + k * 1.9, name)
    tx = x0 + (12.0 if leds else 1.8)
    s.text(title, tx, 6.0, 2.0, bold=True)
    for k, line in enumerate(lines):
        s.text(line, tx, 8.8 + k * 2.0, 1.3)
    s.text(order, tx, 8.8 + len(lines) * 2.0, 1.2)
    if port:
        rj45(s, x0 + 2.0, hgt - 10.6, 'X1 PN')
    return s.labels((x1 + 1.0, 0.4), (x1 + 1.0, 3.4))


def io(prefix, group, bits, offset=0):
    """Terminals named after their channel, e.g. ('DI a.0', '.0')."""
    return [(f'{prefix} {group}.{k}', f'.{k}') for k in range(offset, offset + bits)]


add = page(U, 'Siemens S7-1200', 'Siemens S7-1200', 'Siemens S7-1200')
add(s71200(C('S7-1200 CPU (DC/DC/Relais)', 'S7-1200 CPU (DC/DC/relay)', 'S7-1200 CPU (DC/DC/relais)'), 'CPU 1214C',
           'CPU 1214C', ['DC/DC/Relay'], '6ES7 214-1HE30-0XB0',
           ['L+', 'M', ('FE', EARTH), None, ('L+ OUT', 'L+'), ('M OUT', 'M'), None, '1M'] + io('DI', 'a', 8) + io('DI', 'b', 6)
           + [None, '2M', ('AI 0', '0'), ('AI 1', '1')],
           ['1L'] + io('DQ', 'a', 5) + [None, '2L'] + io('DQ', 'a', 3, 5) + io('DQ', 'b', 2), bottom_x=12 * P, hgt=12 * P,
           groups=(('24VDC', P, False), ('SENSOR', 4.5 * P, False), ('DI a', 11.5 * P, False), ('DI b', 18.5 * P, False),
                   ('AI', 24.5 * P, False), ('RELAY  DQ a', 15 * P, True), ('DQ a', 21 * P, True), ('DQ b', 23.5 * P, True)),
           port=True, leds=('RUN/STOP', 'ERROR', 'MAINT')))
add(s71200(C('S7-1200 SM1223 DI16/DQ16 Relais', 'S7-1200 SM1223 DI16/DQ16 relay', 'S7-1200 SM1223 DI16/DQ16 relais'),
           'SM 1223', 'SM 1223', ['DI16 x 24VDC / DQ16 x Relay'], '6ES7 223-1PL30-0XB0',
           ['L+', 'M', '1M'] + io('DI', 'a', 8) + [None, ('FE', EARTH), '2M'] + io('DI', 'b', 8),
           ['1L'] + io('DQ', 'a', 4) + ['2L'] + io('DQ', 'a', 4, 4) + [None, '3L'] + io('DQ', 'b', 4) + ['4L'] + io('DQ', 'b', 4, 4),
           groups=(('24VDC', 0.5 * P, False), ('DI a', 6.5 * P, False), ('DI b', 17.5 * P, False),
                   ('DQ a', 4.5 * P, True), ('DQ b', 15.5 * P, True))))
add(s71200(C('S7-1200 SM1223 DI8/DQ8 Relais', 'S7-1200 SM1223 DI8/DQ8 relay', 'S7-1200 SM1223 DI8/DQ8 relais'),
           'SM 1223', 'SM 1223', ['DI8 x 24VDC / DQ8 x Relay'], '6ES7 223-1PH30-0XB0',
           ['L+', 'M', '1M'] + io('DI', 'a', 4) + [None, ('FE', EARTH), '2M'] + io('DI', 'a', 4, 4),
           ['1L'] + io('DQ', 'a', 4) + [None, '2L'] + io('DQ', 'a', 4, 4),
           groups=(('24VDC', 0.5 * P, False), ('DI a', 8 * P, False), ('DQ a', 5 * P, True))))
s = s71200(C('S7-1200 Signalboard SB1223', 'S7-1200 signal board SB1223', 'S7-1200 carte de signaux SB1223'), 'SB 1223',
           'SB 1223', ['DI2/DQ2 24VDC'], '6ES7 223-0BD30-0XB0', [],
           ['L+', 'M'] + io('DI', 'e', 2) + io('DQ', 'e', 2), hgt=8 * P,
           groups=(('24VDC', 0.5 * P, True), ('DI e', 2.5 * P, True), ('DQ e', 4.5 * P, True)))
add(s)
add(s71200(C('S7-1200 SM1231 AI4', 'S7-1200 SM1231 AI4', 'S7-1200 SM1231 AI4'), 'SM 1231', 'SM 1231', ['AI4 x 13 bit'],
           '6ES7 231-4HD30-0XB0', ['L+', 'M', ('FE', EARTH), None, '0+', '0-', '1+', '1-'], ['2+', '2-', '3+', '3-'],
           bottom_x=4 * P, groups=(('24VDC', P, False), ('AI', 5.5 * P, False), ('AI', 5.5 * P, True))))
# The 24 V hub of the reference: the compact switch module CSM 1277 (A.15.2: four RJ45 sockets, a three-pole terminal
# strip for the 24 V DC supply on top; operating instructions CSM 1277 V1.20, A2B00079397B, 08/2010, table 4-1: pin 1
# L+, pin 2 M, pin 3 functional ground).
s = Symbol('-NU', C('S7-1200 Stromversorgung (Hub)', 'S7-1200 supply (hub)', 'S7-1200 alimentation (hub)'), 'CSM 1277')
housing(s, -2.2, 0, 10 * P + 2.2, 8 * P).line((-2.2, 4.6), (10 * P + 2.2, 4.6))
row(s, 0, 0, ['L+', 'M', ('FE', EARTH)])
s.text('24VDC', 2 * P, 2.9, 1.1, 'centre')
s.text('CSM 1277', -0.4, 6.0, 2.0, bold=True).text('6GK7 277-1AA10-0AA0', -0.4, 8.8, 1.2)
for k, port in enumerate(('X1 P1', 'X1 P2', 'X1 P3', 'X1 P4')):
    rj45(s, -0.4 + k * 6.6, 5 * P, port)
add(s.labels((10 * P + 3.2, 0.4), (10 * P + 3.2, 3.4)))
# CM 1241: the 9-pin D-sub, RS232 male (table A-164) or RS485 female (table A-160); upper row 1 to 5, lower row 6 to 9
s = Symbol('-NU', C('S7-1200 CM1241 / CM1221', 'S7-1200 CM1241 / CM1221', 'S7-1200 CM1241 / CM1221'), 'CM 1241')
housing(s, -3.4, 0, 8 * P + 3.4, 11 * P)
s.text('CM 1241', -2.4, 1.0, 2.0, bold=True).text('RS232: 6ES7 241-1AH30-0XB0', -2.4, 3.8, 1.1)
s.text('RS485: 6ES7 241-1CH30-0XB0', -2.4, 5.6, 1.1)
for k, line in enumerate(('RS232: 1 DCD  2 RxD  3 TxD  4 DTR  5 GND', '6 DSR  7 RTS  8 CTS  9 RI',
                          'RS485: 3 TxD+ (B)  4 RTS  5 GND', '6 PWR  8 TxD− (A)')):
    s.text(line, -2.4, 8.0 + k * 1.6, 1.0)
s.poly((-2.4, 8 * P - 2.2), (8 * P + 2.4, 8 * P - 2.2), (8 * P + 1.2, 9 * P + 1.4), (-1.2, 9 * P + 1.4), fill=None, w=0.35)
for k in range(9):
    x, y = (2 * k * P, 8 * P) if k < 5 else ((2 * (k - 5) + 1) * P, 9 * P)
    s.circle(x, y, 1.0, w=0.18).line((x, y + 0.5), (x, 12 * P))
    s.text(str(k + 1), x - 0.6, y - 1.6, 1.0, 'right')
    s.pin(str(k + 1), (x, 12 * P), text=str(k + 1))
add(s.labels((8 * P + 4.4, 0.4), (8 * P + 4.4, 3.4)))


# --- S7-300: signal modules (module data A5E00105505-AK, wiring and block diagrams), CPUs (A5E00105475-12: operator
# controls, pin assignment of X11 of the CPU 312C), the AS-i master CP 343-2 P (C79000-G8976-C149-04).
def front_connector(s, edge, y0, texts, out, first=1):
    """The terminals of a front connector down the edge x = edge, P apart from y0: the number at the terminal, the
    address or function beside it. The contact is named after the number, or after the function where the entry is a
    tuple (name, text); None is a free pin (no lead, no contact)."""
    for k, t in enumerate(texts):
        n, y = str(first + k), y0 + k * P
        if t is None:
            s.circle(edge, y, 1.4).text(n, edge - out * 1.1, y - 0.65, 1.3, 'right' if out > 0 else 'left')
            continue
        name, text = t if isinstance(t, tuple) else (n, t)
        column(s, edge, y, [(name, n)], out=out, h=1.3)
        if text:
            s.text(text, edge - out * 3.6, y - 0.65, 1.3, 'right' if out > 0 else 'left')
    return s


def channels(prefix, byte):
    return [f'{prefix}{byte}.{k}' for k in range(8)]


def sm300(caption, value, title, line, order, left, right=None):
    """An S7-300 signal module: type and order number at the top, the front connector down the right edge (20 pins) or
    down both edges (40 pins: 1 to 20 left, 21 to 40 right), a green status LED with its channel number beside each
    input or output."""
    s = Symbol('-A', caption, value)
    s.parent = True
    two = right is not None
    x1, top = (14 if two else 8) * P, 3 * P
    housing(s, 0, 0, x1, top + 21 * P).line((0, top), (x1, top))
    s.text(title, 0.8, 0.6, 1.8, bold=True).text(line, 0.8, 3.0, 1.3).text(order, 0.8, 5.0, 1.2)
    for edge, out, texts, first in ([(0, -1, left, 1), (x1, 1, right, 21)] if two else [(x1, 1, left, 1)]):
        front_connector(s, edge, top + P, texts, out, first)
        lx = (5.4 * P if out < 0 else x1 - 5.4 * P) if two else 1.2 * P
        for k, text in enumerate(texts):
            if text and text[0] in 'EA' and '.' in text:
                y = top + (k + 1) * P
                s.rect(lx - 0.5, y - 0.4, lx + 0.5, y + 0.4, w=0.18)
                s.text(text[-1], lx + 0.9 if out < 0 or not two else lx - 0.9, y - 0.6, 1.2, 'left' if out < 0 or not two else 'right')
    return s.labels((x1 + (P if two else 2 * P) + 0.6, 0.4), (x1 + (P if two else 2 * P) + 0.6, 3.4))


add = page(U, 'Siemens S7-300', 'Siemens S7-300', 'Siemens S7-300')
s = Symbol('-A', C('S7-300 CPU 312C', 'S7-300 CPU 312C', 'S7-300 CPU 312C'), 'CPU 312C')
s.parent = True
housing(s, 0, 0, 12 * P, 23 * P)
for k, name in enumerate(('SF', 'MAINT', 'DC5V', 'FRCE', 'RUN', 'STOP')):
    led(s, 1.6, 1.8 + k * 1.9, name)
s.rect(4.6 * P, 1.0, 5.0 * P, 8.6, w=0.18)
s.rect(4.6 * P, 10.0, 5.0 * P, 14.0, w=0.18).rect(4.6 * P, 11.4, 5.0 * P, 12.6, w=0.18, fill='#000000')
for k, name in enumerate(('RUN', 'STOP', 'MRES')):
    s.text(name, 5.3 * P, 9.8 + k * 1.5, 1.1)
dsub(s, 3.2 * P, 13.5 * P, 'X1 MPI')
s.text('X20 DC 24V', 1.0, 17.4 * P, 1.2)
column(s, 0, 19 * P, ['L+', 'M'], out=-1, h=1.4)
s.text('CPU 312C', 12 * P - 1.0, 0.6, 1.8, 'right', bold=True).text('6ES7312-5BF04-0AB0', 1.0, 21.6 * P, 1.1)
s.text('X11', 12 * P - 1.0, 2.6 * P - 2.6, 1.2, 'right')
X11 = [f'DI 124.{k}' for k in range(8)] + ['DI 125.0', 'DI 125.1', '2M', '1L+'] + [f'DO 124.{k}' for k in range(6)] + ['1M']
front_connector(s, 12 * P, 3 * P, [None] + [(t, t) for t in X11], 1)     # contacts named after the default addresses
add(s.labels((13 * P + 0.6, 0.4), (13 * P + 0.6, 3.4)))
add(sm300(C('S7-300 SM321 DI16', 'S7-300 SM321 DI16', 'S7-300 SM321 DI16'), 'SM 321 DI16', 'SM 321', 'DI 16 x DC 24V',
          '6ES7321-1BH02-0AA0', [''] + channels('E', 0) + ['', ''] + channels('E', 1) + ['M']))
add(sm300(C('S7-300 SM322 DO16', 'S7-300 SM322 DO16', 'S7-300 SM322 DO16'), 'SM 322 DO16', 'SM 322', 'DO 16 x DC 24V/0.5A',
          '6ES7322-1BH01-0AA0', ['1L+'] + channels('A', 0) + ['1M', '2L+'] + channels('A', 1) + ['2M']))
add(sm300(C('S7-300 SM323 DI8/DO8', 'S7-300 SM323 DI8/DO8', 'S7-300 SM323 DI8/DO8'), 'SM 323 DI8/DO8', 'SM 323',
          'DI 8/DO 8 x DC 24V/0.5A', '6ES7323-1BH01-0AA0', [''] + channels('E', 0) + ['1M', '2L+'] + channels('A', 0) + ['2M']))
add(sm300(C('S7-300 SM321 DI32', 'S7-300 SM321 DI32', 'S7-300 SM321 DI32'), 'SM 321 DI32', 'SM 321', 'DI 32 x DC 24V',
          '6ES7321-1BL00-0AA0', [''] + channels('E', 0) + ['', ''] + channels('E', 1) + ['M'],
          [''] + channels('E', 2) + ['', ''] + channels('E', 3) + ['M']))
add(sm300(C('S7-300 SM322 DO32', 'S7-300 SM322 DO32', 'S7-300 SM322 DO32'), 'SM 322 DO32', 'SM 322', 'DO 32 x DC 24V/0.5A',
          '6ES7322-1BL00-0AA0', ['1L+'] + channels('A', 0) + ['1M', '2L+'] + channels('A', 1) + ['2M'],
          ['3L+'] + channels('A', 2) + ['3M', '4L+'] + channels('A', 3) + ['4M']))
add(strip(C('S7-300 Eingänge (Klemmen)', 'S7-300 inputs (terminals)', 'S7-300 entrées (bornes)'),
          [(str(k + 2), str(k + 2), f'E0.{k}') for k in range(8)] + [('1M', '1M', '')], 'Digitaleingang'))
add(strip(C('S7-300 Ausgänge (Klemmen)', 'S7-300 outputs (terminals)', 'S7-300 sorties (bornes)'),
          [('1L+', '1', '1L+')] + [(str(k + 2), str(k + 2), f'A0.{k}') for k in range(8)] + [('1M', '10', '1M')], 'Digitalausgabe'))
add(strip(C('S7-300 CPU (Versorgung)', 'S7-300 CPU (supply)', 'S7-300 CPU (alimentation)'), [('L+', 'L+', ''), ('M', 'M', '')], 'X20 DC 24V'))


def rail(caption, pe):
    """The mounting rail seen from the front, with the PE screw at its left end if `pe`."""
    s = Symbol('', caption, numbered=False, listed=False, shown=False)
    s.rect(0, -2.6, 40 * P, 2.6, w=0.35)
    for x in (7 * P, 33 * P):
        s.rect(x - 2.0, -0.6, x + 2.0, 0.6, w=0.18, corner=50)
    if pe:
        s.circle(2 * P - 0.0, 0, 2.2).line((2 * P - 0.7, 0.7), (2 * P + 0.7, -0.7)).text('PE', 2 * P + 1.6, -0.7, 1.4)
        s.line((2 * P, 1.1), (2 * P, 2 * P)).pin('PE', (2 * P, 2 * P), label=(2 * P + 0.5, 2 * P - 1.8))
    return s


add(rail(C('Profilschiene', 'Mounting rail', 'Rail de montage'), False))
add(rail(C('Profilschiene mit PE', 'Mounting rail with PE', 'Rail de montage avec PE'), True))
# DO 8 x Rel. AC 230V: two outputs share the root of their group (terminals 4, 8, 13, 17), relay supply L+ / M
add(sm300(C('S7-300 SM322 DO8 Relais AC230V', 'S7-300 SM322 DO8 relay AC 230 V', 'S7-300 SM322 DO8 relais AC 230 V'),
          'SM 322 DO8 Rel', 'SM 322', 'DO 8 x Rel. AC 230V', '6ES7322-1HF01-0AA0',
          ['L+', '', 'A0.0', '1L', 'A0.1', '', 'A0.2', '2L', 'A0.3', '', '', 'A0.4', '3L', 'A0.5', '', 'A0.6', '4L', 'A0.7', '', 'M']))
# CPU 315F-2 PN/DP 6ES7315-2FJ14-0AB0 (Siemens data sheet): the front of the CPU 315-2 PN/DP it is built on
# (A5E00105475-12, 2.2.3): LEDs SF, BF1, BF2, MAINT, DC5V, FRCE, RUN, STOP, the MMC slot and mode selector, X2 with the
# ports P1 and P2, X1 MPI/DP, the supply X20 below.
s = Symbol('-A', C('S7-300 CPU 315F-2 PN/DP', 'S7-300 CPU 315F-2 PN/DP', 'S7-300 CPU 315F-2 PN/DP'), 'CPU 315F-2 PN/DP')
s.parent = True
housing(s, 0, 0, 10 * P, 21 * P)
for k, name in enumerate(('SF', 'BF1', 'BF2', 'MAINT', 'DC5V', 'FRCE', 'RUN', 'STOP')):
    led(s, 1.6, 1.8 + k * 1.9 + (1.0 if k > 2 else 0), name)
s.rect(5.4 * P, 1.0, 5.8 * P, 8.6, w=0.18)
s.rect(5.4 * P, 10.0, 5.8 * P, 14.0, w=0.18).rect(5.4 * P, 11.4, 5.8 * P, 12.6, w=0.18, fill='#000000')
for k, name in enumerate(('RUN', 'STOP', 'MRES')):
    s.text(name, 6.1 * P, 9.8 + k * 1.5, 1.1)
s.text('CPU 315F-2 PN/DP', 0.8, 7.4 * P, 1.6, bold=True).text('6ES7315-2FJ14-0AB0', 0.8, 8.4 * P, 1.1)
for k, port in enumerate(('X2 P1', 'X2 P2')):
    rj45(s, 1.4, 9.6 * P + k * 3 * P, port)
dsub(s, 7.4 * P, 11.4 * P, 'X1 MPI/DP')
s.text('X20 DC 24V', 1.0, 17.4 * P, 1.2)
column(s, 0, 19 * P, ['L+', 'M'], out=-1, h=1.4)
add(s.labels((10 * P + 1.0, 0.4), (10 * P + 1.0, 3.4)))
# CP 343-2 P (manual CP 343-2 / CP 343-2 P AS-Interface Master C79000-G8976-C149-04, 08/2008, figure 1-3): the status
# LEDs, the SET button and the slave display 0 to 9; on the 20-pin front connector only 17/19 (AS-i +) and 18/20 (AS-i -)
# are used, jumpered inside the CP so that it can be looped into the AS-i cable.
s = Symbol('-A', C('S7-300 CP 343-2P', 'S7-300 CP 343-2P', 'S7-300 CP 343-2P'), 'CP 343-2 P')
housing(s, 0, 0, 8 * P, 24 * P).line((0, 3 * P), (8 * P, 3 * P))
s.text('CP 343-2 P', 0.8, 0.6, 1.8, bold=True).text('AS-Interface', 0.8, 3.0, 1.3).text('6GK7 343-2AH11-0XA0', 0.8, 4.8, 1.0)
for k, name in enumerate(('SF', 'RUN', 'APF', 'CER', 'AUP', 'CM', None, 'B', '20+', '10+')):
    if name:
        led(s, 1.6, 3 * P + 1.6 + k * 1.7, name)
s.rect(1.0, 3 * P + 17.6, 5.4, 3 * P + 19.8, w=0.25).text('SET', 3.2, 3 * P + 18.1, 1.2, 'centre')
for k in range(10):
    led(s, 1.6, 3 * P + 22.0 + k * 1.7, str(9 - k))
front_connector(s, 8 * P, 4 * P, [None] * 16 + [('17', ''), ('18', ''), ('19', ''), ('20', '')], 1)
xn, xa, xb = 8 * P - 3.0, 8 * P - 3.8, 8 * P - 4.8      # the jumpers 17-19 and 18-20 inside, beside the numbers
s.line((xn, 20 * P), (xa, 20 * P), (xa, 22 * P), (xn, 22 * P))
s.line((xn, 21 * P), (xb, 21 * P), (xb, 23 * P), (xn, 23 * P))
s.text('AS-i+', xb - 0.6, 20 * P - 0.65, 1.2, 'right').text('AS-i' + MINUS, xb - 0.6, 21 * P - 0.65, 1.2, 'right')
add(s.labels((10 * P + 0.6, 0.4), (10 * P + 0.6, 3.4)))


# --- Operator panel KTP600 Basic (operating instructions A5E02421799-01, 01/2009, 1.3 and 3.3.3: the six function keys
# F1 to F6 in a row under the display, the supply connector L+, M "DC 24V" on the underside)
add = page(U, 'Siemens Bediengeräte', 'Siemens operator panels', 'Siemens pupitres opérateur')
s = Symbol('-NT', C('Bedienpanel KTP600', 'Operator panel KTP600', 'Pupitre opérateur KTP600'), 'KTP600')
housing(s, 0, 0, 24 * P, 17 * P)
s.rect(2.4, 2.4, 24 * P - 2.4, 11 * P, w=0.25)
kw = (24 * P - 4.8 - 5 * 2.0) / 6
for k in range(6):
    kx = 2.4 + k * (kw + 2.0)
    s.rect(kx, 11.6 * P, kx + kw, 11.6 * P + 3.0, w=0.25, corner=30).text(f'F{k + 1}', kx + kw / 2, 11.6 * P + 0.8, 1.3, 'centre')
s.text('KTP600 Basic', 2.4, 14.4 * P, 1.6, bold=True)
row(s, 20 * P, 17 * P, ['L+', 'M'], 1)
s.text('DC 24V', 21 * P, 17 * P - 5.2, 1.2, 'centre')
add(s.labels((24 * P + 1.0, 0.4), (24 * P + 1.0, 3.4)))


# --- Power supplies as signs of a converter (DIN EN 60617-6): a box with the diagonal, input and output marked
def psu_sign(caption, inputs, outputs, text_in, text_out):
    s = Symbol('-G', caption)
    n = len(inputs)
    side = 2 * n * P
    bx0, bx1, by0, by1 = 2 * P, 2 * P + side, -P, -P + side
    s.rect(bx0, by0, bx1, by1, w=0.35).line((bx0, by1), (bx1, by0))
    s.text(text_in, bx0 + 0.8, by0 + 0.8, 1.6).text(text_out, bx1 - 0.8, by1 - 2.6, 1.6, 'right')
    for k, name in enumerate(inputs):
        y = 2 * k * P
        s.line((0, y), (bx0, y)).pin(name, (0, y), label=(0.4, y - 2.2), shown=True)
    first = (n - len(outputs)) * P
    for k, (name, text) in enumerate(outputs):
        y = first + 2 * k * P
        s.line((bx1, y), (bx1 + 2 * P, y)).pin(name, (bx1 + 2 * P, y), label=(bx1 + 2 * P - 0.4, y - 2.2), align='right', shown=True, text=text)
    return s.labels((bx0, by0 - 3.4))


add = page(U, 'Netzteile', 'Power supplies', 'Alimentations')
add(psu_sign(C('Netzteil einphasig 24 V', 'Power supply, single-phase, 24 V', 'Alimentation monophasée 24 V'),
             ['L1', 'N', 'PE'], [('+', '+'), ('-', MINUS)], '230 V ~', '24 V ='))
add(psu_sign(C('Netzteil dreiphasig 24 V', 'Power supply, three-phase, 24 V', 'Alimentation triphasée 24 V'),
             ['L1', 'L2', 'L3', 'PE'], [('L+1', 'L+'), ('L+2', 'L+'), ('L-1', 'L−'), ('L-2', 'L−')], '400 V 3~', '24 V ='))

# sPlan's roles: the basic module is a Parent on the USER page, so that contacts drawn elsewhere can be linked to it.
parents(U, 'Siemens LOGO', 'LOGO! 12/24RC')
