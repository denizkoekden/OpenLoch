# More digital building blocks for the pages "ADC", "DAC", "MUX", "Register", "Zähler" and "DIL". Converters after
# IEC 60617-13 (the pointed form with the point on the analogue side, and the square with a diagonal of the converter
# general symbol); multiplexers, registers and counters after IEC 60617-12 with the common control block on top (lower
# corners notched) holding the qualifying symbol and the control inputs, the data part below; DIL packages as a box
# with leads (like DIL 8 to 40) and as the package outline seen from above.


def control_block(s, x0, x1, bottom, notch=1.0):
    """The outline of the common control block from y = 0 down to the element below it."""
    s.line((x0 + notch, bottom), (x0 + notch, bottom - notch), (x0, bottom - notch), (x0, 0), (x1, 0), (x1, bottom - notch),
           (x1 - notch, bottom - notch), (x1 - notch, bottom))


def input_at(s, x0, y, item):
    """An input (text, contact, kind) at the left side x0: kind '' plain, 'dyn' dynamic, 'neg' negated."""
    text, name, kind = item
    if kind == 'neg':
        s.circle(x0 - 0.6, y, 1.2).line((0, y), (x0 - 1.2, y))
    else:
        s.line((0, y), (x0, y))
    if kind == 'dyn':
        s.line((x0, y - 0.9), (x0 + 1.2, y), (x0, y + 0.9)).text(text, x0 + 1.5, y - 1.0, 2.0)
    else:
        s.text(text, x0 + 0.6, y - 1.0, 2.0)
    s.pin(name, (0, y))


def output_at(s, x1, y, name):
    s.line((x1, y), (x1 + P, y)).text(name, x1 - 0.6, y - 1.0, 2.0, 'right')
    s.pin(name, (x1 + P, y), label=(x1 + P - 0.4, y - 2.2), align='right')


def controlled(prefix, caption, label, control, inputs, outputs, width=4 * P):
    """An element with its common control block: the qualifying symbol in the first row of the block, the control
    inputs below it, the data inputs and outputs in the element below (None leaves a row free)."""
    s = Symbol(prefix, caption)
    x0, x1 = P, P + width
    bottom = (len(control) + 2) * P
    h = bottom + (max(len(inputs), len(outputs)) + 1) * P
    control_block(s, x0, x1, bottom)
    s.rect(x0, bottom, x1, h).text(label, (x0 + x1) / 2, 0.4, 2.5, 'centre', bold=True)
    for k, item in enumerate(control):
        input_at(s, x0, (k + 2) * P, item)
    for k, item in enumerate(inputs):
        if item:
            input_at(s, x0, bottom + (k + 1) * P, item)
    for k, name in enumerate(outputs):
        if name:
            output_at(s, x1, bottom + (k + 1) * P, name)
    return s.labels((x0, -3.4), (x0, h + 0.6))


def bits(letter, n):
    return [f'{letter}{k}' for k in range(n)]


def pointed(prefix, caption, text, digital, analogue, adc):
    """A converter whose analogue side is pointed (like an arrow head) with the single analogue lead at the point;
    the digital lines on the flat side, their names inside."""
    n = len(digital)
    h, depth, body = (4 * P, 2 * P, 2 * P) if n == 1 else ((n + 2) * P, 3 * P, 3 * P)
    mid = h / 2
    s = Symbol(prefix, caption)
    if adc:
        tip, back = P, 3 * P if n == 1 else 4 * P
        flat = back + body
        s.poly((tip, mid), (back, 0), (flat, 0), (flat, h), (back, h), fill=None)
        s.line((0, mid), (tip, mid)).pin(analogue, (0, mid))
        side, lead, align = flat, flat + P, 'right'
    else:
        flat = P
        back = flat + body
        tip = back + depth
        s.poly((flat, 0), (back, 0), (tip, mid), (back, h), (flat, h), fill=None)
        s.line((tip, mid), (tip + P, mid)).pin(analogue, (tip + P, mid), label=(tip + P - 0.4, mid - 2.2), align='right')
        side, lead, align = flat, 0, 'left'
    s.text(text, back, mid - 1.25, 2.5, 'centre')
    for k, name in enumerate(digital):
        y = mid if n == 1 else (k + 1) * P
        s.line((side, y), (lead, y))
        if n > 1:
            s.text(name, side - 0.6 if adc else side + 0.6, y - 1.0, 2.0, align)
        if adc:
            s.pin(name, (lead, y), label=(lead - 0.4, y - 2.2), align='right')
        else:
            s.pin(name, (lead, y))
    return s.labels((P, -3.4), (P, h + 0.6))


def squared(prefix, caption, letters, digital, analogue, adc):
    """A converter as the square with a diagonal (lower left to upper right), the input quantity in the upper left
    corner, the output quantity in the lower right one. With several digital lines the box grows into a rectangle and
    the line names stand outside, above longer leads."""
    n = len(digital)
    s = Symbol(prefix, caption)
    if n == 1:
        h, x0, x1 = 4 * P, P, 5 * P
        rows = [h / 2]
    else:
        h = (n + 2) * P
        x0, x1 = (P, 5 * P) if adc else (2 * P, 6 * P)
        rows = [(k + 1) * P for k in range(n)]
    mid = h / 2
    s.rect(x0, 0, x1, h).line((x0, h), (x1, 0))
    s.text(letters[0], x0 + 0.6, 0.6, 2.5).text(letters[1], x1 - 0.6, h - 3.1, 2.5, 'right')
    lead = P if n == 1 else 2 * P
    if adc:
        s.line((0, mid), (x0, mid)).pin(analogue, (0, mid))
        for name, y in zip(digital, rows):
            s.line((x1, y), (x1 + lead, y))
            s.pin(name, (x1 + lead, y), label=(x1 + 0.4, y - 2.0), shown=n > 1)
    else:
        s.line((x1, mid), (x1 + P, mid)).pin(analogue, (x1 + P, mid), label=(x1 + P - 0.4, mid - 2.2), align='right')
        for name, y in zip(digital, rows):
            s.line((0, y), (x0, y))
            s.pin(name, (0, y), label=(x0 - 0.4, y - 2.0), align='right', shown=n > 1)
    for o in s.parts:
        if o['type'] == 'contact':
            o['font'] = {'height': 1.6}
    return s.labels((x0, -3.4), (x0, h + 0.6))


for adc, page_name, de, en, fr, text in ((True, 'ADC', 'A/D-Umsetzer', 'ADC', 'CAN', 'A/D'), (False, 'DAC', 'D/A-Umsetzer', 'DAC', 'CNA', 'D/A')):
    add = extend(DG, page_name)
    analogue, single = ('IN', 'OUT') if adc else ('OUT', 'IN')
    for form in (pointed, squared):
        box_de, box_en, box_fr = (' (Kasten)', ' (box)', ' (rectangle)') if form is squared else ('', '', '')
        add(form(page_name, C(de + box_de, en + box_en, fr + box_fr), text if form is pointed else text.split('/'), [single], analogue, adc))
        for n in (8, 12, 16):
            add(form(page_name, C(f'{de} {n} Bit{box_de}', f'{n}-bit {en}{box_en}', f'{fr} {n} bits{box_fr}'),
                     text if form is pointed else text.split('/'), bits('D', n), analogue, adc))

add = extend(DG, 'MUX')
for n, k in ((2, 1), (8, 3), (16, 4)):
    select = [(f'S{j}', f'S{j}', '') for j in range(k)]
    middle = [None] * (n // 2 - 1)
    add(controlled('MUX', C(f'Multiplexer {n}:1', f'{n}:1 multiplexer', f'Multiplexeur {n}:1'), 'MUX', select,
                   [(d, d, '') for d in bits('D', n)], middle + ['Y']))
    add(controlled('DMUX', C(f'Demultiplexer 1:{n}', f'1:{n} demultiplexer', f'Démultiplexeur 1:{n}'), 'DMUX', select,
                   middle + [('D', 'D', '')], bits('Y', n)))

add = extend(DG, 'Register')
for n in (8, 16):
    add(controlled('REG', C(f'Latch {n} Bit', f'{n}-bit latch', f'Verrou {n} bits'), 'LATCH', [('CLK', 'CLK', 'dyn'), ('R', 'R', '')],
                   [(d, d, '') for d in bits('D', n)], bits('Q', n)))
for n in (8, 16):
    add(controlled('SHIFT', C(f'Schieberegister {n} Bit', f'{n}-bit shift register', f'Registre à décalage {n} bits'), f'SRG{n}',
                   [('C1/→', 'C1', 'dyn'), ('R', 'R', '')], [('1D', '1D', '')], bits('Q', n)))

add = extend(DG, 'Zähler')
count = [('+', 'C', 'dyn'), ('R', 'R', '')]
add(controlled('CNTR', C('Binärzähler 4 Bit (QA–QD)', '4-bit binary counter (QA–QD)', 'Compteur binaire 4 bits (QA–QD)'), 'CTR4',
               count, [], ['QA', 'QB', 'QC', 'QD']))
add(controlled('CNTR', C('Binärzähler 8 Bit', '8-bit binary counter', 'Compteur binaire 8 bits'), 'CTR8', count, [], bits('Q', 8)))
add(controlled('CNTR', C('Binärzähler 8 Bit (QA–QH)', '8-bit binary counter (QA–QH)', 'Compteur binaire 8 bits (QA–QH)'), 'CTR8',
               count, [], ['Q' + c for c in 'ABCDEFGH']))

add = extend(DG, 'DIL')
add(dil(32))


def dil_package(n):
    """A DIL package seen from above: the body with the notch at the top, the pins as small rectangles on both sides
    (rows 3 or 6 pitches apart, the connection points at their outer ends), the pin numbers inside the body."""
    half = n // 2
    span = 3 * P if n <= 20 else 6 * P
    stub = P / 2
    x0, x1, h = stub, span - stub, (half + 1) * P
    s = Symbol('K', C(f'DIL {n} (Gehäuse)', f'DIL {n} (package)', f'DIL {n} (boîtier)'))
    s.rect(x0, 0, x1, h, w=0.35).arc(span / 2, 0, 2.4, 180, 360)
    for k in range(half):
        y = (k + 1) * P
        s.rect(0, y - 0.5, x0, y + 0.5).rect(x1, y - 0.5, span, y + 0.5)
        s.pin(str(k + 1), (0, y), label=(x0 + 0.3, y - 0.9), shown=True)
        s.pin(str(n - k), (span, y), label=(x1 - 0.3, y - 0.9), align='right', shown=True)
    for o in s.parts:
        if o['type'] == 'contact':
            o['font'] = {'height': 1.6}
    return s.labels((x0, -3.4), (x0, h + 0.6))


for n in (8, 14, 16, 18, 20, 24, 28, 32, 40):
    add(dil_package(n))
