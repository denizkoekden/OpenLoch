# The pages of "USER/Misc. (German)/sPlan - EU-Norm": automation devices drawn as front views or terminal plans in the
# style of a wiring diagram after the manufacturers' manuals (named at each device): the housing, the terminal rows with
# the terminal names, LEDs and type numbers as text, no logos. Every wired terminal is a contact on the 2.54 mm pitch.
# Schematic parts (rails, motor starters, sensors, sockets) after DIN EN 60617. Designators carry the leading "-" of
# the reference designation (IEC 81346); children of a parent take its designator and value once linked.
U = 'USER/Misc. (German)/sPlan - EU-Norm'
CHILD_ID, CHILD_VALUE = '<PARENT_ID>', '<PARENT_VALUE>'
MINUS = '−'


def shown_name(name):
    """The name as written on the device: a minus sign for the ASCII "-" of the contact's name."""
    return name.replace('-', MINUS) if name in ('-', 'A-', '-E', '-S', 'I-', 'O-', 'E-') else name


def outline(s, x0, y0, x1, y1):
    """The dash-dot outline of a device made of several elements."""
    s.rect(x0, y0, x1, y1)
    s.parts[-1]['pen']['style'] = 'dashDot'
    return s


def dot(s, x, y):
    return s.circle(x, y, 0.8, fill='#000000')


def child(caption):
    """A part of a parent's terminal plan: designator and value come from the parent once linked."""
    return Symbol(CHILD_ID, caption, value=CHILD_VALUE, numbered=False)


def row(s, x0, edge, names, out=-1, step=P, texts=None, h=1.6):
    """A terminal row along the housing edge y = edge: the k-th terminal at x0 + k * step, a small circle on the edge
    with a lead P outwards (out -1: up, 1: down) to its connection point. The terminal's text (its name unless `texts`
    gives one) is written upwards inside the housing."""
    for k, name in enumerate(names):
        x = x0 + k * step
        end = (x, edge + out * P)
        s.line((x, edge + out * 0.7), end).circle(x, edge, 1.4)
        t = shown_name(name) if texts is None else texts[k]
        if t:
            if out < 0:
                s.text(t, x - h / 2 - 0.1, edge + 1.2, h, 'right', rotation=90)
            else:
                s.text(t, x - h / 2 - 0.1, edge - 1.4, h, 'left', rotation=90)
        s.pin(name, end, text=shown_name(name))
    return s


def led(s, x, y, name=None, d=1.4, right=True):
    """An LED on a front: a small circle, its name beside it."""
    s.circle(x, y, d, w=W)
    if name:
        s.text(name, x + d / 2 + 0.5 if right else x - d / 2 - 0.5, y - 0.8, 1.4, 'left' if right else 'right')
    return s


def dsub_face(s, x, y, n, scale=1.0):
    """The face of a D-sub socket with n contacts in two rows, centred on (x, y)."""
    top = (n + 1) // 2
    half = top * 1.385 / 2 * scale + 1.6
    h = 2.6 * scale
    s.poly((x - half, y - h), (x + half, y - h), (x + half - 1.0, y + h), (x - half + 1.0, y + h), fill=None, w=0.35)
    for k in range(n):
        upper = k < top
        j = k if upper else k - top
        cx = x - (top - 1) * 1.385 / 2 * scale + j * 1.385 * scale + (0 if upper else 1.385 / 2 * scale)
        s.circle(cx, y + (-1.0 if upper else 1.0) * scale, 0.8 * scale)
    return s


def terminal_box(caption, upper, lower, texts=None, step=2 * P, title=None, lower_step=None, rows=4):
    """A child: terminals upper (wired from above) and lower (from below, `rows` P lower), `step` (`lower_step`)
    apart, names shown at the ends."""
    s = child(caption)
    lower_step = lower_step or step
    right = max((len(upper) - 1) * step, (len(lower) - 1) * lower_step) + P
    s.rect(-P, 0, right, rows * P, w=0.35)
    for names, top in ((upper, True), (lower, False)):
        for k, name in enumerate(names):
            if not name:
                continue
            x = k * (step if top else lower_step)
            edge, end = (0, (x, -P)) if top else (rows * P, (x, (rows + 1) * P))
            s.line((x, edge + (0.7 if top else -0.7)), end).circle(x, edge, 1.4)
            short = name.split('.')[-1] if '.' in name else shown_name(name)     # a socket's contact: its pin number
            s.pin(name, end, label=(x, end[1] - 2.3 if top else end[1] + 0.3), align='centre', shown=True, text=short)
            if texts and texts.get(name):
                s.text(texts[name], x, edge + (1.0 if top else -2.4), 1.2, 'centre')
    if title:
        s.text(title, right - 0.6, 1.6 * P, 1.6, 'right')
    return s.labels((right + 1.0, -0.4), (right + 1.0, 2.6))


def m12_face(s, x, y, name):
    """An M12 socket seen from the front: four contacts on a circle and one in the middle."""
    s.circle(x, y, 5.0, w=0.35).circle(x, y, 0.6, fill='#000000')
    for dx, dy in ((1, -1), (-1, -1), (-1, 1), (1, 1)):
        s.circle(x + 1.1 * dx, y + 1.1 * dy, 0.6, fill='#000000')
    return s.text(name, x, y + 2.8, 1.4, 'centre')


# --- ATP Messtechnik: Dini Argeo weighing transmitters as sold by ATP Messtechnik. Terminal numbers and functions after
# Dini Argeo's user manuals (DGT4_08_24.04_EN_U, wiring diagrams DGT4 and DGT4AN; DGTQ_08_24.04_EN_U, DGTQ-DGTQAN and
# DGTQPB): the contacts are named after the terminal numbers, the functions written beside them. Front with the 6-digit
# display, the LEDs ->0<-, ~, NET, F, W1/SP1, W2/SP2 and the keys ZERO, TARE, MODE, PRINT, C (section "Display and
# function of the keys"). The DGT4 has its terminals on the DIN-rail housing, the panel-mounted DGTQ on its back.
add = page(U, 'ATP Messtechnik', 'ATP Messtechnik', 'ATP Messtechnik')


def numbered_row(s, x0, edge, groups, out):
    """Terminal groups (label, [(number, function), ...]) side by side, P apart with a free place between groups: the
    number beside the terminal, the function written upwards inside, the group's label beyond the functions; the
    contact is named after the number. A terminal without function (None) is not wired: no lead, no contact."""
    x = x0
    for label, terms in groups:
        for k, (number, function) in enumerate(terms):
            xk = x + k * P
            s.circle(xk, edge, 1.4).text(number, xk, edge + 1.0 if out < 0 else edge - 2.1, 1.1, 'centre')
            if function is None:
                continue
            end = (xk, edge + out * P)
            s.line((xk, edge + out * 0.7), end)
            if out < 0:
                s.text(function, xk - 0.7, edge + 2.4, 1.2, 'right', rotation=90)
            else:
                s.text(function, xk - 0.7, edge - 2.6, 1.2, 'left', rotation=90)
            s.pin(number, end, text=number)
        a, b = x - 0.5 * P, x + (len(terms) - 0.5) * P
        y = edge + (7.0 if out < 0 else -7.0)
        s.line((a, y), (b, y), w=0.18).text(label, (a + b) / 2, y + (0.3 if out < 0 else -2.0), 1.3, 'centre')
        x += (len(terms) + 1) * P
    return x


def weighing_front(s, cx, y, value):
    """Display, status LEDs and keys of a DGT front, centred on cx from y down; the type at the left."""
    s.rect(cx - 9.0, y, cx + 9.0, y + 5.4, w=0.35).text('888888', cx - 0.6, y + 1.2, 3.0, 'centre').text('kg', cx + 8.4, y + 0.4, 1.2, 'right')
    for k, name in enumerate(('→0←', '~', 'NET', 'F', 'W1', 'W2')):
        x = cx - 8.0 + k * 3.2
        led(s, x, y + 7.0, None, 0.9)
        s.text(name, x, y + 7.8, 0.9, 'centre')
    for k, name in enumerate(('ZERO', 'TARE', 'MODE', 'PRINT', 'C')):
        x = cx - 8.0 + k * 4.0
        s.rect(x - 1.7, y + 10.0, x + 1.7, y + 12.6, w=0.18, corner=20).text(name, x, y + 10.8, 0.9, 'centre')
    return s


CELL = (('EXC−', 'EXC+'), ('SIG−', 'SIG+'))


def cell(n, first):
    """Load cell n of the DGT4 (2 to 4): terminals first (EXC-) down to first - 3 (SIG+)."""
    return (f'LOAD CELL {n}', [(str(first - k), f) for k, f in enumerate(CELL[0] + CELL[1])])


def dgt4(caption, value, analog):
    s = Symbol('-K', caption, value)
    top = [cell(4, 35), cell(3, 31), cell(2, 27),
           ('LOAD CELL 1', [('23', 'EXC−'), ('22', 'EXC+'), ('21', 'REF−'), ('20', 'REF+'), ('19', 'SIG−'), ('18', 'SIG+')])]
    bottom = [('POWER', [('1', '+'), ('2', '−')]), ('INPUT', [('3', 'COM'), ('4', 'IN1'), ('5', 'IN2')]),
              ('RELAYS', [('6', 'RL1'), ('7', 'RL2'), ('8', 'COM')]),
              ('ANALOG' if analog else '', [('9', 'I+'), ('10', 'I−'), ('11', 'V+'), ('12', 'V−')] if analog
               else [(str(n), None) for n in range(9, 13)]),
              ('RS485', [('13', 'A(+)'), ('14', 'B(−)')]), ('RS232', [('15', 'TX'), ('16', 'RX'), ('17', 'GND')])]
    h = 16 * P
    s.rect(-P, 0, 22 * P, h, w=0.35)
    numbered_row(s, 0, 0, top, -1)
    numbered_row(s, 0, h, bottom, 1)
    s.text('12 / 24 Vdc', 0.5 * P, h - 10.2, 1.1, 'centre')
    weighing_front(s, 12.5 * P, 10.6, value)
    s.text(value, -P + 1.2, 13.0, 2.0, bold=True)
    return s.labels((22 * P + 1.0, -0.4), (22 * P + 1.0, 2.6))


add(dgt4(C('Wägeelektronik DGT4-AN', 'Weighing electronics DGT4-AN', 'Électronique de pesage DGT4-AN'), 'DGT4-AN', True))
add(dgt4(C('Wägeelektronik DGT4-DB', 'Weighing electronics DGT4-DB', 'Électronique de pesage DGT4-DB'), 'DGT4-DB', False))

# DGTQ: the terminal blocks on the back, numbered from the right; the PROFIBUS socket of the DGTQPB
s = Symbol('-K', C('Wägeelektronik DGTQ-AN (Profibus)', 'Weighing electronics DGTQ-AN (Profibus)',
                   'Électronique de pesage DGTQ-AN (Profibus)'), 'DGTQ-AN')
H_Q = 18 * P
s.rect(-P, 0, 17 * P, H_Q, w=0.35)
numbered_row(s, 0, 0, [('LOAD CELL', [('26', 'EXC−'), ('25', 'EXC+'), ('24', 'SEN−'), ('23', 'SEN+'), ('22', 'SIG−'), ('21', 'SIG+')]),
                       ('OUTPUT', [('20', 'COM')] + [(str(19 - k), f'OUT{k + 1}') for k in range(6)])], -1)
numbered_row(s, 0, H_Q, [('ANALOG', [('13', 'V+'), ('12', 'COM'), ('11', 'I+')]), ('COM1', [('10', 'B(−)'), ('9', 'A(+)')]),
                         ('COM2', [('8', 'GND'), ('7', 'RX2'), ('6', 'TX2')]), ('INPUT', [('5', 'IN2'), ('4', 'IN1'), ('3', 'COM')]),
                         ('POWER', [('2', 'GND'), ('1', '+Vdc')])], 1)
weighing_front(s, 9.5 * P, 10.6, 'DGTQ-AN')
s.text('DGTQ-AN', -P + 1.2, 10.4, 2.0, bold=True)
dsub_face(s, 3.5 * P, 12.5 * P, 9, 1.0)
s.text('PROFIBUS', 3.5 * P, 12.5 * P + 3.2, 1.2, 'centre')
add(s.labels((17 * P + 1.0, -0.4), (17 * P + 1.0, 2.6)))


# --- Beckhoff: Bus Coupler and Bus Terminals as front views after the Beckhoff documentation (BK9000/BK9050 manual,
# "KL2xxx and KS2xxx" 3.2.0, data sheet KL1809/KL1819 1.0): the LEDs at the top, the terminal points of both columns led
# out to the sides with their numbers, the bus coupler's supply and power contacts (bridged pairs) to the right.
add = page(U, 'Beckhoff', 'Beckhoff', 'Beckhoff')
KL_W = 7 * P        # width of a bus terminal


def bus_terminal(caption, value, left, right, texts, leds, pitch):
    """A Bus Terminal: left[k] and right[k] are the terminal points of row k, texts their functions (left, right)."""
    s = Symbol('-A', caption, value)
    rows = len(left)
    y0 = 5 * P if leds > 4 else 3 * P
    h = y0 + rows * pitch + P
    s.rect(0, 0, KL_W, h, w=0.35)
    for k in range(leds):
        if leds <= 4:      # LED 1 and 2 in the first row, 3 and 4 below
            column, place = k % 2, k // 2
            cy = 0.9 * P + place * 1.4 * P
        else:              # LEDs 1 to n/2 in the left column, the rest in the right one
            column, place = divmod(k, leds // 2)
            cy = 0.8 * P + place * (y0 - 1.8 * P) / (leds // 2 - 1)
        cx = 2.6 * P if column == 0 else 4.4 * P
        s.rect(cx - 0.5, cy - 0.35, cx + 0.5, cy + 0.35, w=0.18)
        if leds <= 4 or place in (0, leds // 2 - 1):
            s.text(str(k + 1), cx + (0.9 if column else -0.9), cy - 0.7, 1.2, 'left' if column else 'right')
    for k in range(rows):
        y = y0 + k * pitch
        for name, text, x, end in ((left[k], texts[0][k], P, -P), (right[k], texts[1][k], KL_W - P, KL_W + P)):
            s.circle(x, y, 1.4)
            if end < 0:
                s.line((x - 0.7, y), (end, y)).text(text, x + 1.1, y - 0.8, 1.4)
                s.pin(name, (end, y), label=(end + 0.3, y - 2.2), shown=True)
            else:
                s.line((x + 0.7, y), (end, y)).text(text, x - 1.1, y - 0.8, 1.4, 'right')
                s.pin(name, (end, y), label=(end - 0.3, y - 2.2), align='right', shown=True)
    s.text(value, KL_W / 2, h - 2.4, 1.6, 'centre', bold=True)
    return s.labels((KL_W + P + 0.6, -3.4), (KL_W + P + 0.6, -0.6))


s = Symbol('-A', C('Buskoppler BK9000/BK9050', 'Bus Coupler BK9000/BK9050', 'Coupleur de bus BK9000/BK9050'), 'BK9000')
s.parent = True
W_BK, H_BK = 16 * P, 12 * P
TB = (12 * P, 14 * P)               # the two columns of the terminal block
s.rect(0, 0, W_BK, H_BK, w=0.35).line((TB[0] - P, 0), (TB[0] - P, H_BK))
s.text('Ethernet TCP/IP', 0.8, 0.8, 1.6)
s.rect(0.8, 2 * P, 0.8 + 3.6, 2 * P + 3.6).rect(2.0, 2 * P + 2.6, 3.2, 2 * P + 3.6)
for k, name in enumerate(('LINK', 'ACT', 'ERROR', 'WDG', 'I/O RUN', 'I/O ERR')):
    led(s, 2.6 * P, 1.6 * P + k * 1.6 + (1.2 if k > 3 else 0), name)
s.rect(0.8, 8 * P, 0.8 + 10 * 1.2, 8 * P + 2.4)
for k in range(10):
    s.rect(1.0 + k * 1.2, 8 * P + 0.4, 1.8 + k * 1.2, 8 * P + 1.4, w=0.18, fill='#000000')
s.text('1 … 10', 0.8, 8 * P + 2.6, 1.3).text('BK9000', 7 * P, 10.5 * P, 1.8, bold=True)
for x in TB:
    led(s, x, 1.0 * P, None, 1.2)
s.text('Us', TB[0], 1.0 * P + 0.8, 1.2, 'centre').text('Up', TB[1], 1.0 * P + 0.8, 1.2, 'centre')
for k, (a, b) in enumerate((('24V', '0V'), ('+', '+'), ('-', '-'), ('PE', 'PE'))):
    y = 3 * P + 2 * k * P
    s.circle(TB[0], y, 1.4).circle(TB[1], y, 1.4).text(shown_name(a), TB[0] - 1.0, y - 0.8, 1.4, 'right')
    s.line((TB[1] + 0.7, y), (W_BK + P, y))
    if k == 0:
        s.line((TB[0], y + 0.7), (TB[0], y + P), (W_BK + P, y + P))
        s.pin('24V', (W_BK + P, y + P), label=(W_BK + P - 0.3, y + P - 2.2), align='right', shown=True)
        s.pin('0V', (W_BK + P, y), label=(W_BK + P - 0.3, y - 2.2), align='right', shown=True)
    else:
        s.line((TB[0] + 0.7, y), (TB[1] - 0.7, y))
        s.pin(b, (W_BK + P, y), label=(W_BK + P - 0.3, y - 2.2), align='right', shown=True, text=shown_name(b))
add(s.labels((W_BK + 2 * P, -3.4), (W_BK + 2 * P, -0.6)))

out = ('OUT1', '0V', '0V', 'OUT3'), ('OUT2', '0V', '0V', 'OUT4')
add(bus_terminal(C('Digitalausgang KL2404/KL2424', 'Digital output KL2404/KL2424', 'Sortie numérique KL2404/KL2424'), 'KL2404',
                 ['1', '2', '3', '4'], ['5', '6', '7', '8'], out, 4, 2 * P))
add(bus_terminal(C('Digitaleingang KL1809', 'Digital input KL1809', 'Entrée numérique KL1809'), 'KL1809',
                 [str(k) for k in range(1, 9)], [str(k) for k in range(9, 17)],
                 ([f'IN{k}' for k in range(1, 9)], [f'IN{k}' for k in range(9, 17)]), 16, P))
s = Symbol('-A', C('Endklemme KL9010', 'End terminal KL9010', 'Borne finale KL9010'), 'KL9010')
s.rect(0, 0, 3 * P, 12 * P, w=0.35).rect(0.8, 0.8, 3 * P - 0.8, 12 * P - 0.8, w=0.18)
add(s.text('KL9010', 1.5 * P + 0.9, 6 * P, 1.6, 'centre', rotation=90).labels((3 * P + 0.8, -0.4), (3 * P + 0.8, 2.6)))


def strip(caption, title, texts):
    """A row of terminal points numbered 1..n as a strip, wired from above."""
    s = Symbol('', caption, numbered=False, listed=False, shown=False)
    n = len(texts)
    s.rect(-P / 2, 0, (n - 0.5) * P, 3 * P, w=0.35)
    for k, t in enumerate(texts):
        x = k * P
        if k:
            s.line((x - P / 2, 0), (x - P / 2, 3 * P))
        s.line((x, 0.7), (x, -P)).circle(x, 1.4, 1.4).text(str(k + 1), x, 2.4, 1.3, 'centre')
        s.text(t, x - 0.6, 3 * P + 0.4, 1.3, 'right', rotation=90)
        s.pin(str(k + 1), (x, -P))
    return s.text(title, -P / 2, -P - 2.6, 1.8)


add(strip(C('Digitaleingang KL1809 (Klemmen)', 'Digital input KL1809 (terminals)', 'Entrée numérique KL1809 (bornes)'),
          'Digitaleingang', [f'IN{k}' for k in range(1, 9)]))
add(strip(C('Digitalausgang KL2404 (Klemmen)', 'Digital output KL2404 (terminals)', 'Sortie numérique KL2404 (bornes)'),
          'Digitalausgabe', ['OUT1', '0V', '0V', 'OUT3', 'OUT2', '0V', '0V', 'OUT4']))
s = child(C('Buskoppler BK9050 (Versorgung)', 'Bus Coupler BK9050 (supply)', 'Coupleur de bus BK9050 (alimentation)'))
names = ['24V', '0V', '+', '-', 'PE']
s.rect(-P, 0, 9 * P, 3 * P, w=0.35)
for k, name in enumerate(names):
    x = 2 * P * k
    s.line((x, 0.7), (x, -P)).circle(x, 0, 1.4)
    s.pin(name, (x, -P), label=(x, -P - 2.3), align='centre', shown=True, text=shown_name(name))
s.line((-0.4 * P, 1.6 * P), (2.4 * P, 1.6 * P)).text('Us', P, 1.6 * P + 0.3, 1.4, 'centre')
s.line((3.6 * P, 1.6 * P), (6.4 * P, 1.6 * P)).text('Up', 5 * P, 1.6 * P + 0.3, 1.4, 'centre')
add(s.labels((9 * P + 1.0, -0.4), (9 * P + 1.0, 2.6)))


# --- Bluhm System: the interface box between the line control and a label printer applicator: the terminal strip X1
# below, the D-sub 25 socket for the printer's cable on the front (no terminal plan of the manufacturer published)
add = page(U, 'Bluhm System', 'Bluhm System', 'Bluhm System')


s = Symbol('-A', C('Druckerbox (Etikettierer-Schnittstelle)', 'Printer box (labeller interface)',
                   'Boîtier imprimante (interface étiqueteuse)'))
n = 27
H_BOX = 9 * P
right = (n + 1) * P
s.rect(-P, 0, right, H_BOX, w=0.35)
for k in range(n):
    x = k * P
    s.line((x, H_BOX - 0.7), (x, H_BOX + P)).circle(x, H_BOX, 1.4).text(str(k + 1), x, H_BOX - 2.9, 1.3, 'centre')
    s.pin(f'X1.{k + 1}', (x, H_BOX + P))
x = n * P
s.line((x, H_BOX - 0.7), (x, H_BOX + P)).circle(x, H_BOX, 1.4).text('PE', x, H_BOX - 2.9, 1.3, 'centre').pin('PE', (x, H_BOX + P))
s.rect(-0.6 * P, H_BOX - 3.3, (n + 0.6) * P, H_BOX - 0.9, w=W).text('X1', -0.8 * P, H_BOX - 5.4, 1.8)
dsub_face(s, (n - 1) * P / 2, 3.4 * P, 25, 1.4)
s.text('X2  D-Sub 25', (n - 1) * P / 2, 1.0, 1.8, 'centre').text('Druckerbox', -0.4 * P, 1.0, 2.0, bold=True)
add(s.labels((right + 1.0, 0), (right + 1.0, 3.0)))


# --- Festo: CPV10 valve terminals with AS-Interface node after Festo's brief descriptions (CPV...-GE-ASI-4E4A-Z-CE:
# 8107493, 2019-02a; CPV-ASI-8E8A-Z-CE: 8101727, 2018-11b): the flat-cable connections "Bus" (pin 2 AS-i +, pin 1 AS-i
# -) and "24 V DC" for the valves (pin 2 +24 V, pin 1 0 V), the M8 sensor sockets numbered from 0 (pin 1 US+, 3 US-,
# 4 input Ix), the outputs O0 ... to the solenoid coils; the 8E8A holds two slaves (I0-I3/O0-O3 and I4-I7/O4-O7).
# The parent is the terminal seen from above: the connections, PWR and Fault LEDs at the left end plate, per valve
# position the LED of its input, its sensor socket and the LEDs of the coils 14 and 12.
add = page(U, 'Festo', 'Festo', 'Festo')


def cpv_inputs(caption, inputs):
    upper, texts = [], {}
    for k in range(inputs):
        for pin, t in zip(('1', '3', '4'), ('+', '−', 'I')):
            upper.append(f'I{k}.{pin}')
            texts[f'I{k}.{pin}'] = t
        upper.append('')
    lower = ['A+', 'A-', '24V', '0V'] + [f'O{k}' for k in range(inputs)]
    s = terminal_box(caption, upper[:-1], lower, texts, step=P, lower_step=2 * P, rows=5)
    for k in range(inputs):
        x = (4 * k + 1) * P
        s.text(f'I{k}', x, 2.4 * P, 1.4, 'centre').line((x - 1.4 * P, 2.0 * P), (x + 1.4 * P, 2.0 * P), w=0.18)
    for a, b, name in ((0, 2 * P, 'BUS'), (4 * P, 6 * P, '24 VDC'), (8 * P, (2 * inputs + 6) * P, 'VALVES')):
        s.line((a - 0.6 * P, 3.3 * P), (b + 0.6 * P, 3.3 * P), w=0.18).text(name, (a + b) / 2, 3.3 * P + 0.3, 1.2, 'centre')
    return s


def cpv(caption, value, valves):
    s = Symbol('-A', caption, value)
    s.parent = True
    strip, pitch, h = 3 * P, 2.4 * P, 13 * P
    right = strip + 0.4 * P + valves * pitch + (2.6 * P if valves > 4 else 1.6 * P)
    s.rect(0, 0, right, h, w=0.35).line((strip, 0), (strip, h))
    for y, name in ((1.0 * P, 'BUS'), (h - 3.0 * P, '24 VDC')):
        s.rect(0.6, y, strip - 0.6, y + 2.0 * P, w=0.25).text(name, strip / 2, y - 1.4, 1.0, 'centre')
        for dy in (0.5 * P, 1.5 * P):
            s.circle(strip / 2, y + dy, 1.4).line((strip / 2 - 0.5, y + dy + 0.5), (strip / 2 + 0.5, y + dy - 0.5))
    x = strip / 2
    s.line((x, 3.6 * P), (x, 4.2 * P))
    for k, half in enumerate((0.9, 0.6, 0.3)):
        s.line((x - half, 4.2 * P + k * 0.45), (x + half, 4.2 * P + k * 0.45))
    for k, name in enumerate(('PWR', 'Fault')):
        y = 6.0 * P + k * 1.1 * P
        s.rect(x - 1.1, y - 0.4, x - 0.1, y + 0.4, w=0.18).text(name, x + 0.2, y - 0.5, 0.9)
    for k in range(valves):
        cx = strip + 0.4 * P + (k + 0.5) * pitch
        s.text(str(k), cx - 1.2, 0.5, 1.2).rect(cx, 0.7, cx + 1.4, 1.5, w=0.18)
        s.rect(cx - 2.2, 2.2 * P, cx + 2.2, 3.6 * P, w=0.18)
        s.circle(cx, 5.2 * P, 4.4)
        for dx, dy in ((0, -0.9), (-0.8, 0.6), (0.8, 0.6)):
            s.circle(cx + dx, 5.2 * P + dy, 0.5, fill='#000000')
        s.rect(cx - 2.2, 7.0 * P, cx + 2.2, 9.6 * P, w=0.18)
        for y in (10.4 * P, 11.4 * P):
            s.rect(cx - 0.8, y - 0.4, cx + 0.8, y + 0.4, w=0.18)
    x14 = strip + 0.4 * P + 0.5 * pitch - 1.4
    s.text('14', x14, 10.4 * P - 0.6, 1.0, 'right').text('12', x14, 11.4 * P - 0.6, 1.0, 'right')
    if valves > 4:
        s.circle(right - 1.3 * P, 1.6 * P, 2.6).circle(right - 1.3 * P - 2.0, 1.6 * P, 0.6, fill='#000000')
        s.text('Addr.', right - 1.3 * P, 2.5 * P, 1.0, 'centre')
        for half, name in ((0, 'slave 1'), (4, 'slave 2')):
            a = strip + 0.4 * P + half * pitch
            s.line((a + 0.6, h - 0.9 * P), (a + 4 * pitch - 0.6, h - 0.9 * P), w=0.18).text(name, a + 2 * pitch, h - 0.8 * P, 1.0, 'centre')
    s.text('AS-i', right - 0.6, h - 2.0 * P, 1.4, 'right', bold=True)
    return s.labels((right + 1.0, -0.4), (right + 1.0, 2.6))


add(cpv_inputs(C('Ventilinsel CPV10 AS-i 8E/8A (Eingänge)', 'Valve terminal CPV10 AS-i 8I/8O (inputs)',
                 'Terminal de distributeurs CPV10 AS-i 8E/8S (entrées)'), 8))
add(cpv_inputs(C('Ventilinsel CPV10 AS-i 4E/4A (Eingänge)', 'Valve terminal CPV10 AS-i 4I/4O (inputs)',
                 'Terminal de distributeurs CPV10 AS-i 4E/4S (entrées)'), 4))
add(cpv(C('Ventilinsel CPV10 mit 4E/4A', 'Valve terminal CPV10 with 4I/4O', 'Terminal de distributeurs CPV10 avec 4E/4S'),
        'CPV10-GE-ASI-4E4A-Z-M8-CE', 4))
add(cpv(C('Ventilinsel CPV10 mit 8E/8A', 'Valve terminal CPV10 with 8I/8O', 'Terminal de distributeurs CPV10 avec 8E/8S'),
        'CPV10-GE-ASI-8E8A-Z-M8-CE', 8))


# --- HBM: a load cell as its strain-gauge full bridge (DIN EN 60617-4 resistors in a Wheatstone bridge), excitation
# above and below, signal at the sides, the wire colours of the 4-wire cable at the leads (HBM data sheet PW6C,
# B01994 09 E00 04, wiring code: blue excitation +, black excitation -, white signal +, red signal -)
add = page(U, 'HBM', 'HBM', 'HBM')


def resistor_on(s, a, b, length=2.8, width=1.1):
    """A resistor's rectangle in the middle of the line from a to b."""
    cx, cy = (a[0] + b[0]) / 2, (a[1] + b[1]) / 2
    d = math.hypot(b[0] - a[0], b[1] - a[1])
    ux, uy = (b[0] - a[0]) / d, (b[1] - a[1]) / d
    l, w = length / 2, width / 2
    corners = [(cx + sl * l * ux - sw * w * uy, cy + sl * l * uy + sw * w * ux) for sl, sw in ((-1, -1), (1, -1), (1, 1), (-1, 1))]
    s.line(a, (cx - l * ux, cy - l * uy)).line((cx + l * ux, cy + l * uy), b)
    return s.poly(*corners, fill='#ffffff')


s = Symbol('-B', C('Wägezelle PW6C (5 kg)', 'Load cell PW6C (5 kg)', 'Cellule de pesée PW6C (5 kg)'), value='PW6C/5kg')
nodes = {'+E': (0, -2 * P), '-E': (0, 2 * P), '-S': (-2 * P, 0), '+S': (2 * P, 0)}
order = ['+E', '+S', '-E', '-S']
for k in range(4):
    resistor_on(s, nodes[order[k]], nodes[order[(k + 1) % 4]])
BODY = 2 * P + 1.2
s.rect(-BODY, -BODY, BODY, BODY, w=0.35)
ends = {'+E': (0, -4 * P), '-E': (0, 4 * P), '-S': (-4 * P, 0), '+S': (4 * P, 0)}
colours = {'+E': 'bl', '-E': 'sw', '+S': 'ws', '-S': 'rt'}
for name, end in ends.items():
    a = nodes[name]
    dot(s.line(a, end), *a)
    t = shown_name(name)
    mid = (BODY + 4 * P) / 2
    if end[0] == 0:
        s.pin(name, end, label=(0.6, end[1] - 2.0 if end[1] < 0 else end[1] + 0.2), shown=True, text=t)
        s.text(colours[name], 0.6, (mid if end[1] > 0 else -mid) - 0.9, 1.6)
    else:
        s.pin(name, end, label=(end[0], end[1] - 2.3), align='centre', shown=True, text=t)
        s.text(colours[name], mid if end[0] > 0 else -mid, 0.4, 1.6, 'centre')
add(s.labels((BODY + 1.0, -4 * P), (BODY + 1.0, -4 * P + 3.0)))


# --- IFM: AS-i modules after ifm's data sheets and operating instructions (AC2251: data sheet AC2251-01, operating
# instructions 11406488/00; AC5205: data sheet AC5205-00, operating instructions 80226775/00). The parents show the front,
# the children the terminals as wired: terminal names as printed (A = AS-i, E = external AUX supply, I+/I- sensor
# supply, O- actuator 0 V), the M12 sockets of the ClassicLine module with their pins.
add = page(U, 'IFM', 'IFM', 'IFM')
add(terminal_box(C('AS-i-Modul AC2251 (Klemmen)', 'AS-i module AC2251 (terminals)', 'Module AS-i AC2251 (bornes)'),
                 ['I+', 'I1', 'I2', 'I3', 'I4', 'I-'], ['A+', 'A-', 'E+', 'E-', 'O1', 'O2', 'O3', 'O4', 'O-'], title='AC2251'))

s = Symbol('-A', C('AS-i-Modul AC5205', 'AS-i module AC5205', 'Module AS-i AC5205'), 'AC5205')
s.parent = True
s.rect(0, 0, 9 * P, 13 * P, w=0.35, corner=10)
for k, (cx, cy) in enumerate(((2.5 * P, 4 * P), (2.5 * P, 9 * P), (6.5 * P, 4 * P), (6.5 * P, 9 * P))):
    m12_face(s, cx, cy, f'I-{k + 1}')
    led(s, cx - 3.2, cy - 2.6, None, 0.9)
s.circle(7.4 * P, 1.2 * P, 2.0).text('ADDR', 7.4 * P - 1.6, 1.2 * P - 0.8, 1.2, 'right')
led(s, 1.0 * P, 0.8 * P, 'PWR', 0.9)
led(s, 1.0 * P, 1.6 * P, 'FAULT', 0.9)
s.text('AC5205', 4.5 * P, 11.6 * P, 1.6, 'centre', bold=True).text('AS-i 4DI', 4.5 * P, 12.3 * P, 1.2, 'centre')
add(s.labels((9 * P + 1.0, -0.4), (9 * P + 1.0, 2.6)))

texts = {}
upper = ['A+', '', 'A-', '']
for k in range(1, 5):
    for pin, t in zip(range(1, 6), ('L+', 'IN', 'L−', 'IN', 'n.c.')):
        upper.append(f'I{k}.{pin}')
        texts[f'I{k}.{pin}'] = t
    upper.append('')
s = terminal_box(C('AS-i-Modul AC5205 DI4 (Klemmen)', 'AS-i module AC5205 DI4 (terminals)', 'Module AS-i AC5205 DI4 (bornes)'),
                 upper[:-1], [], texts, step=P)
for k in range(4):
    x = (4 + 6 * k + 2) * P
    s.text(f'I-{k + 1}', x, 2.4 * P, 1.4, 'centre').line((x - 2.4 * P, 2.0 * P), (x + 2.4 * P, 2.0 * P), w=0.18)
add(s)
# The AC5205 has inputs only; the ClassicLine module with four outputs is the AC5213 (4DO-Y, data sheet AC5213-00,
# 24.01.2024): AS-i and AUX (24 V) by flat cable, output n on pin 4 of its socket (O-1/2, O-2, O-3/4, O-4), AUX - on pin 3.
add(terminal_box(C('AS-i-Modul AC5205 DO4 (Klemmen)', 'AS-i module AC5205 DO4 (terminals)', 'Module AS-i AC5205 DO4 (bornes)'),
                 ['A+', 'A-', 'O1', 'O2', 'O3', 'O4', '+24V', '0V'], [],
                 {'A+': 'AS-i', 'A-': 'AS-i', 'O1': 'O-1/2', 'O2': 'O-2', 'O3': 'O-3/4', 'O4': 'O-4', '+24V': 'AUX', '0V': 'AUX'},
                 title='AC5213 4DO-Y'))

s = Symbol('-A', C('AS-i-Modul AC2251', 'AS-i module AC2251', 'Module AS-i AC2251'), 'AC2251')
s.parent = True
W_IFM, H_IFM = 10 * P, 16 * P
s.rect(0, 0, W_IFM, H_IFM, w=0.35)
blocks = ((0.4 * P, (('I−',) * 4, ('I1', 'I2', 'I3', 'I4'), ('I+',) * 4)),
          (12.0 * P, (('A+', 'A−', 'E+', 'E−'), ('O1', 'O2', 'O3', 'O4'), ('O−',) * 4)))
for y0, rows in blocks:
    for j, names in enumerate(rows):
        for k, name in enumerate(names):
            x = (1.6 + 2 * k) * P + (j - 1) * 0.6
            y = y0 + j * 1.2 * P
            s.rect(x - 0.9, y, x + 0.9, y + 0.9 * P, w=0.18).text(name, x, y + 0.2, 1.2, 'centre')
for k in range(4):
    led(s, (1.6 + 2 * k) * P, 4.6 * P, None, 1.0).text(str(k + 1), (1.6 + 2 * k) * P, 4.6 * P + 0.7, 1.2, 'centre')
    led(s, (1.6 + 2 * k) * P, 10.6 * P, None, 1.0).text(f'O{k + 1}', (1.6 + 2 * k) * P, 10.6 * P - 2.3, 1.2, 'centre')
for k, name in enumerate(('AS-i', 'FAULT', 'AUX')):
    led(s, (1.6 + 2.6 * k) * P, 6.4 * P, None, 1.0).text(name, (1.6 + 2.6 * k) * P, 6.4 * P + 0.7, 1.2, 'centre')
s.circle(W_IFM / 2, 8.6 * P, 2.6).text('AC2251', W_IFM - 0.6, 8.2 * P, 1.6, 'right', bold=True)
add(s.labels((W_IFM + 1.0, -0.4), (W_IFM + 1.0, 2.6)))


# --- Leitungen: potential rails and a junction, no parts
add = page(U, 'Leitungen', 'Leitungen', 'Conducteurs')
RAIL = 20 * P


def rails(caption, names, value):
    s = Symbol('', caption, value=value, ask=True, numbered=False, listed=False, shown=False, value_shown=True)
    for k, name in enumerate(names):
        y = k * P
        s.line((0, y), (RAIL, y)).text(name, -0.8, y - 1.0, 2.0, 'right')
    return s.labels((RAIL, -3.4), (RAIL, -3.4), 'right', 'right')


add(rails(C('Schiene 24 V und 0 V', 'Rails 24 V and 0 V', 'Barres 24 V et 0 V'), ['24V', '0V'], '1,5 mm²'))
add(rails(C('Schiene L1 L2 L3 N PE', 'Rails L1 L2 L3 N PE', 'Barres L1 L2 L3 N PE'), ['L1', 'L2', 'L3', 'N', 'PE'], '2,5 mm²'))
add(rails(C('Schiene N PE', 'Rails N PE', 'Barres N PE'), ['N', 'PE'], '2,5 mm²'))
add(rails(C('Schiene L1 L2 L3', 'Rails L1 L2 L3', 'Barres L1 L2 L3'), ['L1', 'L2', 'L3'], '2,5 mm²'))
s = Symbol('', C('Abzweig', 'Branch', 'Dérivation'), numbered=False, listed=False, shown=False)
add(dot(s.line((0, 0), (4 * P, 0)).line((2 * P, 0), (2 * P, 2 * P)), 2 * P, 0))


# --- Motorschutzschalter: manual motor starters after DIN EN 60617-7 and -2 as the Siemens 3RV2 data sheets draw them
# (3RV2011-1FA10, data sheet of 11/18/2024, circuit diagram): make contacts with the circuit-breaker function, operated
# by hand (the general sign of manual operation) through the switching mechanism (a square with a cross on the
# mechanical link); in each current path the thermal overload release (the conductor's bend in a box) and the magnetic
# overcurrent release (I>) acting on the mechanism; the auxiliary contact block as a child.
add = page(U, 'Motorschutzschalter', 'Motorschutzschalter', 'Disjoncteurs-moteurs')
TIP = (-1.9, P + 0.6)    # free end of a make contact's blade, pivot at (x, 3 P)


def on_blade(x, y, tip=TIP):
    return (x + tip[0] * (3 * P - y) / (3 * P - tip[1]), y)


def breaker_pole(s, x, names):
    """Terminal names[0] above, the make contact with the circuit-breaker function (DIN EN 60617-7 07-13-05), the
    thermal and the magnetic release, terminal names[1] below."""
    s.line((x, 0), (x, P)).line((x, 3 * P), (x, 3.5 * P)).line((x, 3 * P), (x + TIP[0], TIP[1]))
    s.line((x - 0.8, P - 0.8), (x + 0.8, P + 0.8)).line((x - 0.8, P + 0.8), (x + 0.8, P - 0.8))
    s.rect(x - P, 3.5 * P, x + P, 5 * P)
    s.line((x, 3.5 * P), (x, 3.5 * P + 0.8), (x - 0.7, 3.5 * P + 0.8), (x - 0.7, 5 * P - 0.8), (x, 5 * P - 0.8), (x, 5 * P))
    s.rect(x - P, 5 * P, x + P, 6.5 * P).text('I>', x, 5 * P + 0.9, 1.6, 'centre')
    s.line((x, 6.5 * P), (x, 8 * P))
    s.pin(names[0], (x, 0), label=(x + 0.9, -0.2), shown=True).pin(names[1], (x, 8 * P), label=(x + 0.8, 8 * P - 2.4), shown=True)
    return s


def hand(s, x, y):
    """Manual operation, general (DIN EN 60617-2), at the end of the mechanical link."""
    return s.line((x, y - 1.2), (x, y + 1.2))


def mechanism(s, x, y, d=2.4):
    """The switching mechanism (latch) on the mechanical link: a square with a cross."""
    s.rect(x - d / 2, y - d / 2, x + d / 2, y + d / 2, fill='#ffffff')
    return s.line((x - d / 2, y), (x + d / 2, y)).line((x, y - d / 2), (x, y + d / 2))


def motor_starter(caption, n, value):
    s = Symbol('-Q', caption, value)
    xs = [2 * P * k for k in range(n)]
    y = 2 * P - 0.4
    hx, mx = -8.0, -4.6
    for yr in (4.25 * P, 5.75 * P):
        dashed(s, (mx, yr), (-P, yr))
    dashed(s, (mx, 5.75 * P), (mx, y))
    dashed(s, (hx, y), on_blade(xs[-1], y))
    for k, x in enumerate(xs):
        breaker_pole(s, x, (str(2 * k + 1), str(2 * k + 2)))
    hand(s, hx, y)
    mechanism(s, mx, y)
    return s.labels((xs[-1] + 2.6, P + 0.4), (xs[-1] + 2.6, 2 * P + 0.6))


add(motor_starter(C('Motorschutzschalter einpolig', 'Motor protection switch, single-pole', 'Disjoncteur-moteur unipolaire'), 1, '10–30 A / 10 A'))
s = motor_starter(C('Motorschutzschalter dreipolig', 'Motor protection switch, three-pole', 'Disjoncteur-moteur tripolaire'), 3, '30–50 A / 40 A')
s.parent = True
add(s)
s = child(C('Hilfskontakte 13/14, 21/22', 'Auxiliary contacts 13/14, 21/22', 'Contacts auxiliaires 13/14, 21/22'))
s.line((0, 0), (0, P)).line((0, 3 * P), (0, 4 * P)).line((0, 3 * P), TIP)
s.line((2 * P, 0), (2 * P, P), (2 * P + 1.6, P)).line((2 * P, 3 * P), (2 * P, 4 * P)).line((2 * P, 3 * P), (2 * P + 2.1, P - 0.4))
y = 2 * P - 0.4
dashed(s, (-4.4, y), on_blade(2 * P, y, (2.1, P - 0.4)))
for name, at in (('13', (0, 0)), ('14', (0, 4 * P)), ('21', (2 * P, 0)), ('22', (2 * P, 4 * P))):
    s.pin(name, at, label=(at[0] + 0.8, 0.2 if at[1] == 0 else 4 * P - 2.4), shown=True)
add(s.labels((2 * P + 3.0, P + 0.4), (2 * P + 3.0, 2 * P + 0.6)))


# --- Möller easy (now Eaton): front views after Eaton's leaflets and manuals (IL05013015Z and IL05013012Z 02/18,
# MN04902001Z easy800, MN05013012Z EASY209-SE): supply and inputs in the upper row, the relay outputs (terminals 1 and
# 2 of each contact) in the lower row, a second 0V terminal bridged to the first.
add = page(U, 'Möller easy', 'Möller easy', 'Möller easy')


def bridged(s, x, edge, text, to_x):
    """A second terminal of the same potential: its circle, its text and the bridge to the terminal at to_x."""
    s.circle(x, edge, 1.4).line((x, edge + 0.7), (x, edge + 0.6 * P), (to_x, edge + 0.6 * P), (to_x, edge + 0.7))
    return s.text(text, x - 0.9, edge + 0.6 * P + 0.4, 1.6, 'right', rotation=90)


def relay_outputs(s, x0, edge, prefix, n):
    """n relay contacts along the lower edge, each the terminals 1 and 2 with the output's name above them; contacts
    named <output>.1 and <output>.2."""
    for k in range(n):
        x = x0 + 3 * k * P
        name = f'{prefix}{k + 1}'
        for j in range(2):
            xj = x + j * P
            s.line((xj, edge + 0.7), (xj, edge + P)).circle(xj, edge, 1.4).text(str(j + 1), xj, edge - 2.6, 1.4, 'centre')
            s.pin(f'{name}.{j + 1}', (xj, edge + P), text=str(j + 1))
        s.line((x - 0.6, edge - 3.0), (x + P + 0.6, edge - 3.0), w=0.18)
        s.text(name, x + P / 2, edge - 5.4, 1.6, 'centre')
    return s


def keys(s, x, y):
    """The cursor ring with DEL and ALT above, ESC and OK below."""
    s.circle(x, y, 5.0)
    for a in (0, 90, 180, 270):
        tip = rot((x + 1.9, y), (x, y), a)
        triangle(s, tip, rot((x + 1.2, y), (x, y), a), 0.5)
    for dx, dy, name in ((-4.6, -3.4, 'DEL'), (4.6, -3.4, 'ALT'), (-4.6, 3.4, 'ESC'), (4.6, 3.4, 'OK')):
        s.rect(x + dx - 1.7, y + dy - 1.0, x + dx + 1.7, y + dy + 1.0, w=0.18).text(name, x + dx, y + dy - 0.7, 1.2, 'centre')
    return s


# easy618-DC-RE: an expansion (no display): E+/E- for the remote expansion, inputs R1..R12, supply at the right
s = Symbol('-NU', C('Steuerrelais easy618-DC-RE', 'Control relay easy618-DC-RE', 'Relais de commande easy618-DC-RE'), 'EASY618-DC-RE')
W_E, H_E = 21 * P, 15 * P
s.rect(-P, 0, W_E, H_E, w=0.35).rect(2 * P, 5.5 * P, 17 * P, 10 * P, w=0.18)
row(s, 0, 0, ['E+', 'E-'], texts=['E+', 'E−'])
row(s, 3 * P, 0, [f'R{k}' for k in range(1, 13)])
row(s, 16 * P, 0, ['+24V', '0V'])
bridged(s, 18 * P, 0, '0V', 17 * P)
s.text('24 V DC', 9 * P, 4.2 * P, 1.6, 'centre').text('Input', 3 * P, 4.2 * P, 1.6)
led(s, 3 * P, 7 * P, 'POW', 1.6)
s.text('EASY618-DC-RE', 16.4 * P, 8.6 * P, 2.0, 'right', bold=True)
relay_outputs(s, 0, H_E, 'S', 6)
add(s.labels((W_E + 1.0, -0.4), (W_E + 1.0, 2.6)))

# easy800 base unit with 12 inputs and 6 relays (EASY819-DC-RC)
s = Symbol('-NU', C('Steuerrelais easy800', 'Control relay easy800', 'Relais de commande easy800'), 'EASY819-DC-RC')
s.rect(-P, 0, W_E, H_E, w=0.35)
row(s, 0, 0, ['24V', '0V'])
bridged(s, 2 * P, 0, '0V', P)
row(s, 4 * P, 0, [f'I{k}' for k in range(1, 13)])
s.rect(17 * P, 0.6, W_E - 0.6, 3.4 * P, w=0.18).text('NET', 17.2 * P, 0.8, 1.3)
for k in range(2):
    x = 17.6 * P + k * 2.6 * P
    s.rect(x, 1.6 * P, x + 2.0 * P, 3.0 * P, w=0.18).text(str(k + 1), x + P, 1.6 * P + 0.3, 1.2, 'centre')
s.rect(0, 5.4 * P, 7 * P, 9.6 * P, w=0.35).rect(0.8, 5.4 * P + 0.8, 7 * P - 0.8, 9.6 * P - 0.8, w=0.18)
keys(s, 11 * P, 7.5 * P)
led(s, 15 * P, 6.2 * P, 'POW/RUN', 1.4)
led(s, 15 * P, 7.4 * P, 'NET', 1.4)
s.text('EASY819-DC-RC', 19.8 * P, 9.4 * P, 1.8, 'right', bold=True)
relay_outputs(s, 0, H_E, 'Q', 6)
add(s.labels((W_E + 1.0, -0.4), (W_E + 1.0, 2.6)))

# EASY209-SE: Ethernet gateway, RJ45 on top, the RS232 terminal COM (1..5, led out to the left) and RESET on the front,
# the supply below
s = Symbol('-NU', C('Erweiterung easy209-SE', 'Expansion easy209-SE', 'Extension easy209-SE'), 'EASY209-SE')
W_G, H_G = 6 * P, 13 * P
s.rect(-P, 0, W_G, H_G, w=0.35)
s.rect(0.4, 0.6, 4.8, 4.0, w=0.18).rect(1.8, 3.0, 3.4, 4.0, w=0.18).text('ETH', 2.6, 4.4, 1.2, 'centre')
led(s, 6.4, 1.4, '1', 1.0)
led(s, 6.4, 3.0, '2', 1.0)
led(s, 0.6, 3.0 * P, 'POW/RUN', 1.4)
s.rect(-P + 0.5, 3.7 * P, 1.2 * P, 9.3 * P, w=0.18).text('COM', 0.5, 3.8 * P, 1.3, 'centre')
for k in range(5):
    y = (5 + k) * P
    s.circle(-P, y, 1.4).line((-P - 0.7, y), (-2 * P, y))
    s.pin(str(k + 1), (-2 * P, y), label=(-2 * P + 0.3, y - 2.2), shown=True)
s.circle(3.6 * P, 7 * P, 2.0).text('RESET', 3.6 * P, 7 * P + 1.4, 1.2, 'centre')
s.text('EASY209-SE', 2.5 * P, 10.4 * P, 1.6, 'centre', bold=True)
row(s, 0, H_G, ['+24V', '0V'], out=1, step=2 * P)
add(s.labels((W_G + 1.0, -0.4), (W_G + 1.0, 2.6)))


# --- Pilz safety relays as the block diagram / terminal configuration of their operating manuals (PNOZ XV3: 20185-EN-14,
# PNOZ X3.2: 21147-EN-13, PNOZ XV3.1: 20515-EN-12): supply block, input and start circuits, the positive-guided relays
# K1/K2 (instantaneous) and K3/K4 (delayed on de-energisation, DIN EN 60617-7 07-05-02) with their contacts in series,
# the auxiliary break contacts in parallel. Terminals 2 P apart, top and bottom row as on the device.
add = page(U, 'Pilz Relais', 'Pilz Relais', 'Relais Pilz')
COL = 2 * P
H_PNOZ = 10 * P
LEVEL = {'K3': 1.0 * P, 'K1': 3.0 * P, 'K4': 5.0 * P, 'K2': 7.0 * P}    # upper end of each relay's contacts


def make_contact(s, x, y, bottom):
    """A make contact at x from its fixed contact at y to its pivot at y + 2 P, the lead on to `bottom`."""
    s.line((x, y + 2 * P), (x - 1.9, y + 0.6)).line((x, y + 2 * P), (x, bottom))
    return (x - 1.9 * P / (2 * P - 0.6), y + P)


def break_contact(s, x, y, bottom):
    s.line((x, y), (x + 1.6, y)).line((x, y + 2 * P), (x + 2.1, y - 0.4)).line((x, y + 2 * P), (x, bottom))
    return (x + 2.1 * P / (2 * P + 0.4), y + P)


def terminal(s, name, x, top):
    edge = 0 if top else H_PNOZ
    end = (x, -P if top else H_PNOZ + P)
    s.line((x, edge + (-0.7 if top else 0.7)), end).circle(x, edge, 1.4)
    s.pin(name, end, label=(x, end[1] - 2.3 if top else end[1] + 0.3), align='centre', shown=True)


def pnoz(caption, value, top, bottom, power, blocks, outputs, semiconductor=None,
         leds=('POWER', 'START', 'CH.1', 'CH.2', 'CH.1 [t]', 'CH.2 [t]')):
    """top/bottom: the terminal names of the left part by column ('' for none); power: the number of columns of the supply
    block; blocks: (text, columns) of the input/start circuits under the top row; outputs: (kind, upper, lower) with kind
    'i' (K1, K2), 'd' (K3, K4) or 'nc' (break contacts of K1 and K2 in parallel)."""
    s = Symbol('-KN', caption, value)
    n = len(top)
    xo = (n + 0.5) * COL           # where the relay labels stand
    xs = [(n + 1 + k) * COL for k in range(len(outputs))]
    for k, kind in enumerate(o[0] for o in outputs):
        if k and kind != outputs[k - 1][0]:
            xs[k:] = [x + COL for x in xs[k:]]
    right = xs[-1] + (1.5 * COL if outputs[-1][0] == 'nc' else COL - 0.4)
    s.rect(-COL / 2, 0, right, H_PNOZ, w=0.35)
    for k, (a, b) in enumerate(zip(top, bottom)):
        x = k * COL
        if a:
            terminal(s, a, x, True)
            s.line((x, 0.7), (x, P))
        if b:
            terminal(s, b, x, False)
            s.line((x, H_PNOZ - 0.7), (x, H_PNOZ - P))
    # supply block and the logic with its input and start circuits
    x1 = (power - 0.5) * COL - 0.6
    s.rect(-COL / 2 + 0.6, P, x1, H_PNOZ - P).text('Power', (x1 - COL / 2 + 0.6) / 2, H_PNOZ / 2 - 1.0, 1.8, 'centre')
    s.line((x1, H_PNOZ / 2 - 0.6), (x1 + 1.2, H_PNOZ / 2 - 0.6)).line((x1, H_PNOZ / 2 + 0.6), (x1 + 1.2, H_PNOZ / 2 + 0.6))
    x0, x2 = x1 + 1.2, (n - 0.5) * COL + 0.6
    s.rect(x0, P, x2, H_PNOZ - P)
    c = power
    for text, cols in blocks:
        a, b = (c - 0.5) * COL, (c + cols - 0.5) * COL
        s.rect(max(a, x0), P, min(b, x2), 3 * P).text(text, (max(a, x0) + min(b, x2)) / 2, 2 * P - 1.0, 1.8, 'centre')
        c += cols
    xm = (power - 0.5) * COL + 2.0
    s.text(value, xm, 3.6 * P, 2.2, bold=True)
    for k, name in enumerate(leds):
        led(s, xm + 0.7 + (k // 2) * 9.0, 5.2 * P + (k % 2) * 2.4, name)
    if semiconductor:
        a, b = semiconductor
        xa, xb = [top.index(a) if a in top else bottom.index(a), bottom.index(b)]
        xa, xb = xa * COL, xb * COL
        s.rect(xa - 2.0, H_PNOZ - 3.8 * P, xb + 2.0, H_PNOZ - P).text('24 V', xa - 1.4, H_PNOZ - 2.0 * P - 0.2, 1.6)
        bx, by = xb - 1.2, H_PNOZ - 3.0 * P
        s.line((bx - 1.6, by), (bx, by)).line((bx, by - 1.0), (bx, by + 1.0)).line((bx, by - 0.5), (bx + 1.2, by - 1.3), end='arrow', size=0.7)
        s.line((bx, by + 0.5), (xb, by + 1.3), (xb, H_PNOZ - P))
        s.line((xa, H_PNOZ - P), (xa, H_PNOZ - 1.6 * P), (bx - 1.6, H_PNOZ - 1.6 * P), (bx - 1.6, by))
    # the relays' contacts
    links = {k: [] for k in LEVEL}
    for (kind, a, b), x in zip(outputs, xs):
        terminal(s, a, x, True)
        terminal(s, b, x, False)
        if kind in ('i', 'd'):
            first, second = ('K1', 'K2') if kind == 'i' else ('K3', 'K4')
            s.line((x, 0.7), (x, LEVEL[first]))
            links[first].append(make_contact(s, x, LEVEL[first], LEVEL[second]))
            links[second].append(make_contact(s, x, LEVEL[second], H_PNOZ - 0.7))
        else:
            xr = x + 0.5 * COL
            top_node, bottom_node = LEVEL['K1'] - 0.6 * P, LEVEL['K2'] + 2.6 * P
            s.line((x, 0.7), (x, LEVEL['K2'])).line((x, top_node), (xr, top_node), (xr, LEVEL['K1']))
            links['K1'].append(break_contact(s, xr, LEVEL['K1'], bottom_node))
            links['K2'].append(break_contact(s, x, LEVEL['K2'], H_PNOZ - 0.7))
            s.line((xr, bottom_node), (x, bottom_node))
            dot(dot(s, x, top_node), x, bottom_node)
    for name, points in links.items():
        if not points:
            continue
        y = points[0][1]
        s.text(name, xo - 0.4, y - 1.0, 1.8, 'right')
        first = points[0][0]
        if name in ('K3', 'K4'):
            dashed(s, (xo, y), (first - 2.0, y))
            s.arc(first - 3.2, y, 2.4, -60, 60)
            s.line((first - 2.0, y), (first, y))
        else:
            dashed(s, (xo, y), (first, y))
        for p in points:
            dot(s, *p)
        s.line((first, y - 0.3), (points[-1][0], y - 0.3)).line((first, y + 0.3), (points[-1][0], y + 0.3))
    return s.labels((right + 1.0, 0), (right + 1.0, 3.0))


add(pnoz(C('Sicherheitsschaltgerät PNOZ XV3', 'Safety relay PNOZ XV3', 'Relais de sécurité PNOZ XV3'), 'PNOZ XV3',
         ['A1', 'A2', 'S11', 'S12', 'S21', 'S22', 'S31', 'S32'], ['', '', 'S13', 'S14', 'S33', 'S34', 'Y39', 'Y40'], 2,
         [('Input', 2), ('Input', 2), ('Input', 2)],
         [('i', '13', '14'), ('i', '23', '24'), ('i', '33', '34'), ('d', '47', '48'), ('d', '57', '58')]))
add(pnoz(C('Sicherheitsschaltgerät PNOZ X3.2', 'Safety relay PNOZ X3.2', 'Relais de sécurité PNOZ X3.2'), 'PNOZ X3.2',
         ['A1', 'A2', 'S11', 'S12', 'S21', 'S22', 'S31', 'S32', 'S13', 'S14', 'S33', 'S34'],
         ['B1', 'B2', '', '', 'Y31', 'Y32', '', '', '', '', '', ''], 2,
         [('Input', 2), ('Input', 2), ('Input', 2), ('Start', 2), ('Start', 2)],
         [('i', '13', '14'), ('i', '23', '24'), ('i', '33', '34'), ('nc', '41', '42')], semiconductor=('Y31', 'Y32'),
         leds=('POWER', 'CH.1', 'CH.2')))
add(pnoz(C('Sicherheitsschaltgerät PNOZ XV3.1', 'Safety relay PNOZ XV3.1', 'Relais de sécurité PNOZ XV3.1'), 'PNOZ XV3.1',
         ['A1', 'A2', 'S11', 'S12', 'S21', 'S22', 'S31', 'S32'], ['', '', 'S13', 'S14', 'S33', 'S34', 'Y39', 'Y40'], 2,
         [('Input', 2), ('Input', 2), ('Input', 2)],
         [('i', '13', '14'), ('i', '23', '24'), ('i', '33', '34'), ('d', '57', '58'), ('d', '67', '68'), ('nc', '41', '42')]))


# --- Sensoren: 3-wire DC sensors drawn as the data sheets after EN 60947-5-2 draw them (Pepperl+Fuchs 240154, ifm
# IFC240-01): a box with the sensing element at the left (the proximity rhombus) and the switching element at the
# right, the M12 pins 1 (L+), 4 (output) and 3 (L-) led out to the right. Pepperl+Fuchs draws its capacitive sensors
# with the same proximity sign as the inductive ones (data sheets 240154 NBB4-12GM50-E2-V1-M and 038158
# CJ10-30GM-E2-V1, "Connection"); the photoelectric sensor carries the arrows of optical radiation instead. Pressure
# switches after SUCO (operating instructions 1-1-84-628-006, change-over contact, DIN EN 175301-803 socket with PE) and
# ifm (PN7297-02, two switching outputs); the thermostat after the Eberle RTR-E 6724 (catalogue "Temperaturregler",
# RTR-E 6000).
add = page(U, 'Sensoren', 'Sensoren', 'Capteurs')


def m12_pin(s, y, x0, x1, name):
    """A lead from the box at x0 out to the connection point x1, with the plug's pin (a short bar) and its number."""
    s.line((x0, y), (x1, y)).rect(x0 + 1.0, y - 0.3, x0 + 2.2, y + 0.3, w=0.1, fill='#000000')
    return s.pin(name, (x1, y), label=(x0 + 1.6, y - 2.2), align='centre', shown=True)


def make_across(s, x, y):
    """A make contact drawn across, from x to x + 1.8 P on the line y."""
    return s.line((x, y), (x + 0.4 * P, y)).line((x + 0.4 * P, y), (x + 1.5 * P, y - 1.2)).line((x + 1.6 * P, y), (x + 1.6 * P, y - 0.5))


def rhombus(s, x, y, r=1.6):
    return s.poly((x - r, y), (x, y - r), (x + r, y), (x, y + r), fill=None)


def sensor(caption, element, outputs=(('4', 2 * P),), supply=(('1', 0), ('3', None)), prefix='-B', value='PNP'):
    """The box from y = -P down, the element drawn into the left part by `element`, L+ on top, make contacts from the
    L+ bus to the outputs, L- at the bottom."""
    s = Symbol(prefix, caption, value)
    bottom = outputs[-1][1] + 2 * P
    right = 6 * P
    s.rect(0, -P, right, bottom + P, w=0.35).line((2 * P, -P), (2 * P, bottom + P))
    element(s, P, (bottom) / 2)
    xb = 3 * P
    s.line((xb, 0), (right, 0))
    m12_pin(s, 0, right, 8 * P, supply[0][0])
    s.line((xb, 0), (xb, outputs[-1][1]))
    for name, y in outputs:
        make_across(s, xb, y).line((xb + 1.6 * P, y), (right, y))
        m12_pin(s, y, right, 8 * P, name)
        if y != outputs[-1][1]:
            dot(s, xb, y)
    dot(s, xb, 0)
    s.line((2 * P, bottom), (right, bottom))
    m12_pin(s, bottom, right, 8 * P, supply[1][0])
    s.text('+', right - 0.6, -2.0, 1.6, 'right').text(MINUS, right - 0.6, bottom - 2.0, 1.6, 'right')
    return s.labels((8 * P + 1.0, -P - 0.4), (8 * P + 1.0, -P + 2.6))


def proximity(s, x, y):
    rhombus(s, x, y).line((x - 0.35, y - 0.9), (x - 0.35, y + 0.9)).line((x + 0.35, y - 0.9), (x + 0.35, y + 0.9))


def optical(s, x, y):
    rhombus(s, x, y + 0.6)
    arrows_in(s, x + 0.2, y - 1.6, gap=0.6)


add(sensor(C('Induktiver Näherungsschalter (M12, PNP)', 'Inductive proximity switch (M12, PNP)', 'Détecteur de proximité inductif (M12, PNP)'), proximity))
add(sensor(C('Kapazitiver Näherungsschalter (M12, PNP)', 'Capacitive proximity switch (M12, PNP)', 'Détecteur de proximité capacitif (M12, PNP)'), proximity))
add(sensor(C('Optischer Sensor (M12, PNP)', 'Photoelectric sensor (M12, PNP)', 'Détecteur photoélectrique (M12, PNP)'), optical))

# pressure switch with one switching point: change-over contact 1 (common) - 2 (break) / 4 (make), PE on the housing
s = Symbol('-S', C('Druckschalter, ein Schaltpunkt', 'Pressure switch, one switching point', 'Pressostat, un point de commutation'))
outline(s, -5.4 * P, -0.4 * P, 2 * P, 5 * P)
s.pin('4', (-P, -P), label=(-P - 0.4, -P - 2.2), align='right', shown=True).pin('2', (0, -P), label=(0.4, -P - 2.2), shown=True)
s.line((-P, -P), (-P, P + 0.6)).line((0, -P), (0, P), (1.6, P)).line((0, 3 * P), (2.1, P - 0.4)).line((0, 3 * P), (0, 6 * P))
s.pin('1', (0, 6 * P), label=(0.4, 6 * P + 0.2), shown=True)
blade = (2.1 * P / (2 * P + 0.4), 2 * P)
s.rect(-5 * P, 2 * P - 1.3, -3.6 * P, 2 * P + 1.3).text('p', -4.3 * P, 2 * P - 1.6, 2.2, 'centre')
dashed(s, (-3.6 * P, 2 * P), blade)
x = -3 * P
s.line((x, 6 * P), (x, 4 * P)).pin('PE', (x, 6 * P), label=(x + 0.4, 6 * P + 0.2), shown=True).circle(x, 4 * P - 0.7, 3.4)
for k, half in enumerate((1.1, 0.7, 0.3)):
    s.line((x - half, 4 * P - k * 0.6), (x + half, 4 * P - k * 0.6), w=0.35)
add(s.labels((2 * P + 1.0, -0.4), (2 * P + 1.0, 2.6)))


def pressure(s, x, y):
    s.text('p', x, y - 1.6, 2.2, 'centre')


add(sensor(C('Druckschalter, zwei Schaltpunkte', 'Pressure switch, two switching points', 'Pressostat, deux points de commutation'),
           pressure, outputs=(('4', 2 * P), ('2', 4 * P)), prefix='-S', value=''))

# thermostat: change-over contact operated by the temperature (1 common, 2 heating at rest, 3 cooling), thermal feedback
# resistor RF from 2 to N, set-back heater TA from 5 to N, all in the dash-dot outline; terminals in the device's order
s = Symbol('-S', C('Thermostat', 'Thermostat', 'Thermostat'), 'RTR-E 6724')
outline(s, 2 * P, -P, 12 * P, 9 * P)
for name, y in (('5', 0), ('3', 2 * P), ('2', 4 * P), ('1', 6 * P), ('N', 8 * P)):
    s.line((0, y), (P, y)).rect(P, y - 0.9 * P, 2 * P, y + 0.9 * P).text(name, 1.5 * P, y - 0.9, 1.6, 'centre')
    s.pin(name, (0, y), label=(-0.4, y - 2.2), align='right')
x0 = 6 * P
s.line((2 * P, 6 * P), (x0, 6 * P)).line((x0, 6 * P), (x0 - 2.1, 4 * P - 0.4))            # common and blade
s.line((x0, 3 * P), (x0, 4 * P), (x0 - 1.6, 4 * P))                                       # break contact (heating)
s.line((x0 + P, 3 * P), (x0 + P, 4.6 * P))                                                # make contact (cooling)
s.line((x0, 3 * P), (4 * P, 3 * P), (4 * P, 4 * P)).line((2 * P, 4 * P), (4 * P, 4 * P))
s.line((x0 + P, 3 * P), (x0 + P, 2 * P), (2 * P, 2 * P))
blade = (x0 - 2.1 * P / (2 * P + 0.4), 5 * P)
s.rect(8.4 * P, 5 * P - 1.3, 10 * P, 5 * P + 1.3).text('ϑ', 9.2 * P, 5 * P - 1.4, 2.2, 'centre')
dashed(s, (8.4 * P, 5 * P), blade)
dot(s, 4 * P, 4 * P).line((4 * P, 4 * P), (4 * P, 6.4 * P)).rect(4 * P - 0.8, 6.4 * P, 4 * P + 0.8, 7.6 * P).line((4 * P, 7.6 * P), (4 * P, 8 * P))
s.text('RF', 4 * P + 1.2, 6.6 * P, 1.4)
s.line((2 * P, 0), (11 * P, 0), (11 * P, 1.2 * P)).rect(11 * P - 0.8, 1.2 * P, 11 * P + 0.8, 2.4 * P).line((11 * P, 2.4 * P), (11 * P, 8 * P))
s.text('TA', 11 * P - 1.2, 1.4 * P, 1.4, 'right')
dot(s.line((2 * P, 8 * P), (11 * P, 8 * P)), 4 * P, 8 * P)
add(s.labels((12 * P + 1.0, -P), (12 * P + 1.0, -P + 3.0)))


# --- Sicherheitsschalter: safety switch with guard locking, the connector assignment of the EUCHNER TZ...BHA12 (operating
# instructions "Safety Switch TZ..." 2088062, fig. 6): each contact SK (actuated by the actuator) in series with the
# matching contact ÜK (monitoring the guard locking solenoid), break contacts 11-12 ... 41-42 drawn closed (guard closed
# and locked), the solenoid with the red LED across it, the green LED on the circuit 21-22, PE on pin 9.
add = page(U, 'Sicherheitsschalter', 'Sicherheitsschalter', 'Interrupteurs de sécurité')


def nc_contact(s, x, y, number):
    """A break contact from y to y + 2 P (pivot below), its terminal numbers number1/number2 beside it."""
    s.line((x, y), (x + 1.6, y)).line((x, y + 2 * P), (x + 2.1, y - 0.4))
    s.text(f'{number}1', x - 0.5, y - 1.6, 1.3, 'right').text(f'{number}2', x - 0.5, y + 2 * P - 1.0, 1.3, 'right')
    return (x + 2.1 * P / (2 * P + 0.4), y + P)


def led_up(s, x, y0, y1, colour):
    """An LED between y0 (anode, below) and y1 (cathode, above), its colour beside it."""
    m = (y0 + y1) / 2
    s.line((x, y0), (x, m + 0.75)).line((x, m - 0.75), (x, y1))
    s.poly((x - 1.1, m + 0.75), (x + 1.1, m + 0.75), (x, m - 0.75), fill=None)
    s.line((x - 1.1, m - 0.75), (x + 1.1, m - 0.75), w=0.35)
    arrows_out(s, x + 1.2, m - 0.4, gap=0.7)
    return s.text(colour, x - 1.4, m - 0.8, 1.2, 'right')


s = Symbol('-S', C('Sicherheitsschalter mit Zuhaltung', 'Safety switch with guard locking', 'Interrupteur de sécurité avec interverrouillage'), 'TZ…BHA12')
W_S, H_S = 16 * P, 10 * P
outline(s, -1.6 * P, 0, W_S, H_S)
columns = ((0, '1', '11', '12'), (2 * P, '3', '5', '6'), (4 * P, '4', '3', '4'), (6 * P, '2', '1', '2'))
links = {'SK': [], 'ÜK': []}
for x, number, top, bottom in columns:
    s.line((x, -P), (x, P)).line((x, 3 * P), (x, 5 * P)).line((x, 7 * P), (x, H_S + P))
    links['SK'].append(nc_contact(s, x, P, number))
    links['ÜK'].append(nc_contact(s, x, 5 * P, number))
    s.pin(top, (x, -P), label=(x, -P - 2.3), align='centre', shown=True)
    s.pin(bottom, (x, H_S + P), label=(x, H_S + P + 0.3), align='centre', shown=True)
for name, points in links.items():
    y = points[0][1]
    dashed(s, (-1.2 * P, y), points[-1])
    s.text(name, -1.4 * P, y - 2.4, 1.4)
for p in links['SK'] + links['ÜK']:
    dot(s, *p)
dot(s, 6 * P, 4 * P).line((6 * P, 4 * P), (W_S + P, 4 * P)).pin('8', (W_S + P, 4 * P), label=(W_S + P - 0.3, 4 * P - 2.2), align='right', shown=True)
# green LED from the circuit 21-22 (pin 2) to pin 7; solenoid and red LED between pin 10 and pin 7
top = 5.4 * P
dot(s, 6 * P, 8.4 * P).line((6 * P, 8.4 * P), (8 * P, 8.4 * P))
led_up(s, 8 * P, 8.4 * P, top, 'GN')
s.line((8 * P, top), (14 * P, top), (14 * P, H_S + P))
s.line((10 * P, H_S + P), (10 * P, 8.0 * P)).rect(10 * P - 1.3, 6.2 * P, 10 * P + 1.3, 8.0 * P, w=0.35)
s.line((10 * P - 1.3, 8.0 * P), (10 * P + 1.3, 6.2 * P)).line((10 * P, 6.2 * P), (10 * P, top))
dot(dot(s, 10 * P, top), 12 * P, top)
dot(s, 10 * P, 8.7 * P).line((10 * P, 8.7 * P), (12 * P, 8.7 * P), (12 * P, 8.2 * P))
led_up(s, 12 * P, 8.2 * P, 6.0 * P, 'RD')
s.line((12 * P, 6.0 * P), (12 * P, top))
s.line((15 * P, H_S + P), (15 * P, 7.7 * P)).circle(15 * P, 8.0 * P, 2.4)
for k, half in enumerate((0.9, 0.55, 0.2)):
    s.line((15 * P - half, 7.7 * P + k * 0.5), (15 * P + half, 7.7 * P + k * 0.5), w=0.25)
for name, x in (('10', 10 * P), ('7', 14 * P), ('9', 15 * P)):
    s.pin(name, (x, H_S + P), label=(x - (0.4 if name == '9' else 0), H_S + P + 0.3), align='left' if name == '9' else 'centre',
          shown=True, text='PE/9' if name == '9' else name)
add(s.labels((W_S + 1.0, -0.4), (W_S + 1.0, 2.6)))


# --- Steckdosen: a device of three socket outlets with protective contact (DIN EN 60617-3 female contacts) on common
# L, N, PE terminals
add = page(U, 'Steckdosen', 'Steckdosen', 'Prises de courant')
s = Symbol('-XS', C('Schutzkontaktsteckdose dreifach (Gerät)', 'Triple socket outlet with protective contact (device)',
                    'Prise triple avec terre (appareil)'))
right = 13 * P
outline(s, P, -P, right, 5 * P + 1.0)
for k, name in enumerate(('L', 'N', 'PE')):
    y = k * P
    s.line((0, y), (P - 0.7, y)).circle(P, y, 1.4).line((P + 0.7, y), (right - P, y))
    s.pin(name, (0, y), label=(-0.4, y - 1.0), align='right', shown=True)
for k in range(3):
    x0 = 3 * P + 4 * k * P
    for j, name in enumerate(('L', 'N', 'PE')):
        x = x0 + j * P
        dot(s, x, j * P).line((x, j * P), (x, 4 * P)).arc(x, 4 * P + 0.9, 1.8, 0, 180, w=0.35)
        s.text(name, x, 4 * P + 1.2, 1.2, 'centre')
add(s.labels((right + 1.0, -P), (right + 1.0, 2.0)))

# sPlan's roles: these devices are Parents, so that contacts drawn elsewhere can be linked to them as children.
parents(U, 'ATP Messtechnik', 'Wägeelektronik DGT4-AN', 'Wägeelektronik DGTQ-AN (Profibus)')
parents(U, 'Beckhoff', 'Digitalausgang KL2404/KL2424', 'Endklemme KL9010')
parents(U, 'Bluhm System')
parents(U, 'Motorschutzschalter')
parents(U, 'Pilz Relais')
parents(U, 'Sensoren', 'Thermostat')
