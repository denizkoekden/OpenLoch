# Op-amps after DIN EN 60617-13 and piezoelectric crystals after DIN EN 60617-4 (04-07) for the pages "OP's" and
# "Quarze": the triangle form with the non-inverting input above, the rectangular form with "▷∞", and crystals with
# three electrodes and with two pairs of electrodes. Pins as on the existing symbols (op-amp: 2 −, 3 +, 1 out, 7/4
# supply).


def opamp(caption, supply=False):
    """The existing op-amp turned over at its inputs: + (3) above, − (2) below."""
    s = Symbol('K', caption)
    s.poly((P, -2 * P), (P, 2 * P), (5 * P, 0), fill=None, w=0.35)
    s.line((0, -P), (P, -P)).line((0, P), (P, P)).line((5 * P, 0), (6 * P, 0))
    s.text('+', P + 0.5, -P - 1.4, 2.5).text('−', P + 0.5, P - 1.4, 2.5)
    s.pin('3', (0, -P)).pin('2', (0, P)).pin('1', (6 * P, 0), label=(6 * P - 0.4, -2.2), align='right')
    if supply:
        s.line((3 * P, -P), (3 * P, -2 * P)).line((3 * P, P), (3 * P, 2 * P))
        s.pin('7', (3 * P, -2 * P), label=(3 * P + 0.4, -2 * P)).pin('4', (3 * P, 2 * P), label=(3 * P + 0.4, 2 * P - 2.2))
    return s.labels((3.6 * P, -2 * P - 1.0), (3.6 * P, 2 * P - 1.8))


add = extend(BT, "OP's")
add(opamp(C('Operationsverstärker (nichtinvertierender Eingang oben)', 'Op-amp (non-inverting input at the top)', 'Ampli op (entrée non inverseuse en haut)')))
add(opamp(C('Operationsverstärker mit Versorgung (nichtinvertierender Eingang oben)', 'Op-amp with supply (non-inverting input at the top)',
            'Ampli op avec alimentation (entrée non inverseuse en haut)'), supply=True))
s = Symbol('K', C('Operationsverstärker (Kastenform)', 'Op-amp (rectangular form)', 'Ampli op (forme rectangulaire)'))
s.rect(P, -2 * P, 5 * P, 2 * P, w=0.35)
s.line((0, -P), (P, -P)).line((0, P), (P, P)).line((5 * P, 0), (6 * P, 0))
s.text('−', P + 0.5, -P - 1.4, 2.5).text('+', P + 0.5, P - 1.4, 2.5)
s.poly((3 * P - 2.2, -2 * P + 0.7), (3 * P - 2.2, -2 * P + 2.5), (3 * P - 0.6, -2 * P + 1.6), fill=None)    # amplifier, ▷
s.text('∞', 3 * P, -2 * P + 0.3, 2.5)                                                                 # very high gain
s.pin('2', (0, -P)).pin('3', (0, P)).pin('1', (6 * P, 0), label=(6 * P - 0.4, -2.2), align='right')
add(s.labels((P, -2 * P - 3.4), (P, 2 * P + 0.6)))


def plate(s, x):
    return s.line((x, -2.2), (x, 2.2), w=THICK)


def split_plate(s, x, out):
    """Two electrodes side by side at x, each half the plate; their leads leave outwards (out -1 left, +1 right) and
    step to the pitch at y = -P and y = P."""
    end = 0 if out < 0 else 4 * P
    for sign in (-1, 1):
        s.line((x, sign * 0.4), (x, sign * 2.2), w=THICK)
        s.line((x, sign * 1.3), (end - out * P, sign * 1.3), (end - out * P, sign * P), (end, sign * P))
    return s


def crystal(caption):
    s = Symbol('X', caption, ask=True)
    s.rect(1.65 * P, -1.8, 2.35 * P, 1.8)
    return s.labels((2 * P, -5.6), (2 * P, 2.8), 'centre')


add = extend(BT, 'Quarze')
s = crystal(C('Quarz mit drei Elektroden', 'Crystal with three electrodes', 'Quartz à trois électrodes'))
plate(s, 1.4 * P).line((0, 0), (1.4 * P, 0)).pin('1', (0, 0))
split_plate(s, 2.6 * P, 1).pin('2', (4 * P, -P), label=(4 * P - 0.4, -P - 2.2), align='right').pin('3', (4 * P, P), label=(4 * P - 0.4, P - 2.2), align='right')
add(s)
s = crystal(C('Quarz mit zwei Elektrodenpaaren', 'Crystal with two pairs of electrodes', 'Quartz à deux paires d’électrodes'))
split_plate(s, 1.4 * P, -1).pin('1', (0, -P)).pin('2', (0, P))
split_plate(s, 2.6 * P, 1).pin('3', (4 * P, -P), label=(4 * P - 0.4, -P - 2.2), align='right').pin('4', (4 * P, P), label=(4 * P - 0.4, P - 2.2), align='right')
add(s)
