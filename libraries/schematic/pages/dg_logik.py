# More logic elements for the pages "Gatter", "Gatter (USA)", "Flipflops" and "Digital": rectangular forms after
# IEC 60617-12 (qualifying symbols &, ≥1, =1, 1, the hysteresis sign, C1/1D dependency notation, dynamic inputs,
# postponed outputs, the delay element) and the distinctive shapes of ANSI/IEEE Std 91. The designators name the
# function (NAND, OR, ..., STR, RS-FF, D-FF, JK-FF, ST, 7SEG).


def hysteresis(s, x, y, k=1.0, w=W):
    """The hysteresis sign of a Schmitt trigger as on the Schmitt-Trigger-Inverter: lower left corner at (x, y),
    2.0 * k wide and 1.2 * k high."""
    s.line((x, y), (x + 1.0 * k, y), (x + 1.0 * k, y - 1.2 * k), (x + 2.0 * k, y - 1.2 * k), w=w)
    s.line((x + 0.4 * k, y), (x + 0.4 * k, y - 1.2 * k), (x + 1.4 * k, y - 1.2 * k), w=w)
    return s


def negation_sign(caption):
    """The negation circle alone, to be put at a gate input or output: insertion point on the outline."""
    s = Symbol('', caption, numbered=False, listed=False, shown=False)
    return s.circle(0.6, 0, 1.2, w=THICK)


add = extend(DG, 'Gatter')
s = gate_iec('1', 1, False, C('Schmitt-Trigger', 'Schmitt trigger', 'Trigger de Schmitt'))
s.prefix = 'STR'
s.parts[1]['pos'] = pt((2 * P - 1.6, 0.8))
add(hysteresis(s, 2 * P - 0.4, 2 * P - 1.2))
# "=1" kept for three inputs as well (IEC writes "2k+1" for the odd-parity function).
for sign, neg, prefix, de, en, fr in (('&', True, 'NAND', 'NAND', 'NAND', 'NON-ET'), ('≥1', False, 'OR', 'ODER', 'OR', 'OU'),
                                      ('≥1', True, 'NOR', 'NOR', 'NOR', 'NON-OU'),
                                      ('=1', False, 'XOR', 'Exklusiv-ODER', 'XOR', 'OU exclusif'),
                                      ('=1', True, 'XNOR', 'Exklusiv-NOR', 'XNOR', 'NON-OU exclusif')):
    s = gate_iec(sign, 3, neg, C(f'{de} (3 Eingänge)', f'{en} (3 inputs)', f'{fr} (3 entrées)'))
    s.prefix = prefix
    add(s)
add(negation_sign(C('Negation', 'Negation', 'Négation')))

add = extend(DG, 'Gatter (USA)')


def gate_us3(kind, negated, prefix, caption):
    """A gate of gate_us() with a third input in the middle: inputs 1 to 3, output 4."""
    s = gate_us(kind, negated, caption)
    s.prefix = prefix
    for o in s.parts:
        if o['type'] == 'contact' and o['name'] in ('2', '3'):
            o['name'] = o['text'] = str(int(o['name']) + 1)
    back = 1.05 if kind in ('or', 'xor') else 0      # where the curved back crosses the middle row
    s.line((0, P), (P + back, P)).pin('2', (0, P))
    slots = [k for k, o in enumerate(s.parts) if o['type'] == 'contact']
    for k, o in zip(slots, sorted((s.parts[k] for k in slots), key=lambda o: int(o['name']))):
        s.parts[k] = o
    return s


for neg, de, en, fr in ((False, 'Schmitt-Trigger', 'Schmitt trigger', 'Trigger de Schmitt'),
                        (True, 'Schmitt-Trigger-Inverter', 'Schmitt trigger inverter', 'Inverseur trigger de Schmitt')):
    s = gate_us('buf', neg, C(de, en, fr))
    s.prefix = 'STR'
    add(hysteresis(s, P + 0.7, P + 0.55, k=0.9))
for kind, neg, prefix, de, en, fr in (('and', False, 'AND', 'UND', 'AND', 'ET'), ('and', True, 'NAND', 'NAND', 'NAND', 'NON-ET'),
                                      ('or', False, 'OR', 'ODER', 'OR', 'OU'), ('or', True, 'NOR', 'NOR', 'NOR', 'NON-OU'),
                                      ('xor', False, 'XOR', 'Exklusiv-ODER', 'XOR', 'OU exclusif'),
                                      ('xor', True, 'XNOR', 'Exklusiv-NOR', 'XNOR', 'NON-OU exclusif')):
    add(gate_us3(kind, neg, prefix, C(f'{de} (3 Eingänge)', f'{en} (3 inputs)', f'{fr} (3 entrées)')))
add(negation_sign(C('Negation', 'Negation', 'Négation')))

add = extend(DG, 'Flipflops')


def flipflop(prefix, caption, inputs, outputs, postponed=False):
    """A bistable element after IEC 60617-12 in the size of the flip-flops above. Inputs are (text, contact, kind) down
    the left side, kind '' plain, 'dyn' dynamic, 'neg' negated, 'negdyn' dynamic on the falling edge; outputs are
    contact names down the right side ('~Q' with a negation circle); postponed outputs carry the ¬ mark."""
    rows = max(len(inputs), len(outputs))
    h = (rows + 1) * P
    s = Symbol(prefix, caption)
    x0, x1 = P, 4 * P
    s.rect(x0, 0, x1, h)
    for k, item in enumerate(inputs):
        y = (k + 1) * P
        if not item:
            continue
        text, name, kind = item
        if kind.startswith('neg'):
            s.circle(x0 - 0.6, y, 1.2).line((0, y), (x0 - 1.2, y))
        else:
            s.line((0, y), (x0, y))
        if kind.endswith('dyn'):
            s.line((x0, y - 0.9), (x0 + 1.2, y), (x0, y + 0.9))
            s.text(text, x0 + 1.5, y - 1.0, 2.0)
        else:
            s.text(text, x0 + 0.6, y - 1.0, 2.0)
        s.pin(name, (0, y))
    for k, name in enumerate(outputs):
        y = (k + 1) * P
        if not name:
            continue
        if name.startswith('~'):
            s.circle(x1 + 0.6, y, 1.2).line((x1 + 1.2, y), (x1 + P, y))
        else:
            s.line((x1, y), (x1 + P, y))
        if postponed:
            s.line((x1 - 1.7, y - 0.9), (x1 - 0.6, y - 0.9), (x1 - 0.6, y + 0.2))
        s.text('Q', x1 - (2.1 if postponed else 0.6), y - 1.0, 2.0, 'right')
        s.pin(name, (x1 + P, y), label=(x1 + P - 0.4, y - 2.2), align='right')
    s.labels((x0, -3.4), (x0, h + 0.6))
    return s


rs = [('1S', '1S', ''), ('C1', 'C1', ''), ('1R', '1R', '')]
jk = [('1J', '1J', ''), ('C1', 'C1', ''), ('1K', '1K', '')]
add(flipflop('RS-FF', C('RS-Flipflop, zustandsgesteuert', 'SR flip-flop, level-triggered', 'Bascule RS, déclenchement sur niveau'),
             rs, ['Q', '', '~Q']))
add(flipflop('RS-FF', C('RS-Flipflop, flankengesteuert', 'SR flip-flop, edge-triggered', 'Bascule RS, déclenchement sur front'),
             [rs[0], ('C1', 'C1', 'dyn'), rs[2]], ['Q', '', '~Q']))
add(flipflop('D-FF', C('D-Flipflop, zustandsgesteuert', 'D flip-flop, level-triggered (D latch)', 'Bascule D, déclenchement sur niveau'),
             [('1D', '1D', ''), ('C1', 'C1', '')], ['Q', '~Q']))
add(flipflop('JK-FF', C('JK-Flipflop, Master-Slave', 'JK flip-flop, master-slave', 'Bascule JK maître-esclave'),
             jk, ['Q', '', '~Q'], postponed=True))
add(flipflop('JK-FF', C('JK-Flipflop, negative Flanke', 'JK flip-flop, negative edge', 'Bascule JK, front descendant'),
             [jk[0], ('C1', 'C1', 'negdyn'), jk[2]], ['Q', '', '~Q']))
add(flipflop('JK-FF', C('JK-Flipflop mit Setzen und Rücksetzen', 'JK flip-flop with set and reset', 'Bascule JK avec mise à 1 et à 0'),
             [('S', 'S', 'neg')] + jk + [('R', 'R', 'neg')], ['Q', '', '', '', '~Q'], postponed=True))

add = extend(DG, 'Digital')
s = Symbol('ST', C('Schmitt-Trigger (Kasten)', 'Schmitt trigger (box)', 'Trigger de Schmitt (rectangle)'))
s.rect(P, 0, 3 * P, 2 * P, w=0.35)
hysteresis(s, 2 * P - 1.6, P + 0.95, k=1.6)
s.line((0, P), (P, P)).line((3 * P, P), (4 * P, P))
add(s.pin('1', (0, P)).pin('2', (4 * P, P), label=(4 * P - 0.4, P - 2.2), align='right').labels((P, -3.4), (P, 2 * P + 0.6)))


def delay(caption, t1, t2):
    """A delay element after IEC 60617-12: the delay sign (a line with end strokes) on the signal axis, t1 (delay of
    the 0-1 transition) on its input side, t2 (delay of the 1-0 transition) on its output side."""
    s = Symbol('', caption, numbered=False)
    x0, x1, cx, cy = P, 5 * P, 3 * P, 2 * P
    s.rect(x0, 0, x1, 4 * P, w=0.35)
    s.line((cx - 1.6, cy), (cx + 1.6, cy)).line((cx - 1.6, cy - 0.9), (cx - 1.6, cy + 0.9)).line((cx + 1.6, cy - 0.9), (cx + 1.6, cy + 0.9))
    if t1:
        s.text('t1', cx - 2.2, cy - 1.0, 2.0, 'right')
    if t2:
        s.text('t2', cx + 2.2, cy - 1.0, 2.0)
    s.line((0, cy), (x0, cy)).line((x1, cy), (x1 + P, cy))
    s.pin('1', (0, cy)).pin('2', (x1 + P, cy), label=(x1 + P - 0.4, cy - 2.2), align='right')
    return s.labels((x0, -3.4), (x0, 4 * P + 0.6))


add(delay(C('Einschaltverzögerung', 'On-delay', 'Temporisation à l’enclenchement'), True, False))
add(delay(C('Ausschaltverzögerung', 'Off-delay', 'Temporisation au déclenchement'), False, True))
add(delay(C('Ein- und Ausschaltverzögerung', 'On- and off-delay', 'Temporisation à l’enclenchement et au déclenchement'), True, True))
add(box('7SEG', C('BCD-zu-7-Segment-Decoder', 'BCD to seven-segment decoder', 'Décodeur BCD 7 segments'),
        ['', 'A', 'B', 'C', 'D'], ['', 'a', 'b', 'c', 'd', 'e', 'f', 'g'], width=6 * P, label='BCD/7-Seg'))
