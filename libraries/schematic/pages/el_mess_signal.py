# More symbols for the pages "Messinstrumente", "Generatoren", "Masse / Erde" and "Signalgeber": indicating and
# integrating instruments and signalling devices after DIN EN 60617-8, signal generators after DIN EN 60617-6 / -10
# (a square with G and the waveform), a ground after DIN EN 60617-2. The bell, gong, buzzer and siren are drawn as in
# DIN EN 60617-8: a dome with its leads from the flat side (the buzzer a bowl with its leads from below), here also turned
# so that the leads come from the left.

# --- Measuring instruments
add = extend(EL, 'Messinstrumente')


def marked(text, caption, h):
    """A meter of the page with a longer unit sign, written smaller."""
    return meter('', caption).text(text, 2 * P, -h / 2, h, 'centre')


add(marked('VAΩ', C('Vielfachmessgerät', 'Multimeter', 'Multimètre'), 1.6))
s = meter('', C('Messinstrument (allgemein)', 'Indicating instrument, general', 'Appareil indicateur, général'))
add(s.arrow((2 * P - 1.4, 1.4), (2 * P + 1.5, -1.5), size=1.0))
s = marked('0', C('Nullindikator', 'Null indicator', 'Indicateur de zéro'), 2.0)
s.parts[-1]['pos'] = pt((2 * P, -2.0))
add(s.arc(2 * P - 0.6, 0.9, 1.2, 0, 180).arc(2 * P + 0.6, 0.9, 1.2, 180, 360))
add(marked('var', C('Blindleistungsmesser', 'Varmeter', 'Varmètre'), 2.0))
s = two_pin('P', C('Elektrizitätszähler', 'Electricity meter (kWh)', 'Compteur électrique (kWh)'))
s.line((0, 0), (P - 0.6, 0)).rect(P - 0.6, -2.2, 3 * P + 0.6, 2.2).line((3 * P + 0.6, 0), (4 * P, 0))
add(s.text('kWh', 2 * P, -1.1, 2.2, 'centre').labels((2 * P, -6.0), (2 * P, 3.4), 'centre'))
s = Symbol('P', C('Oszilloskop (Bildschirm)', 'Oscilloscope (screen)', 'Oscilloscope (écran)'))
c = 2.5 * P
s.line((0, 0), (P, 0)).rect(P, -1.5 * P, 4 * P, 1.5 * P).line((4 * P, 0), (5 * P, 0)).circle(c, 0, 5.8)
for d in (-1, 1):      # the plates deflecting vertically (a pair above and below) and horizontally (a pair side by side)
    s.line((c - 2.0, d * 0.8), (c - 0.6, d * 0.8), w=0.35).line((c + 1.2 + d * 0.7, -0.7), (c + 1.2 + d * 0.7, 0.7), w=0.35)
s.pin('1', (0, 0)).pin('2', (5 * P, 0), label=(5 * P - 0.4, -2.2), align='right')
add(s.labels((c, -1.5 * P - 3.0), (c, 1.5 * P + 0.6), 'centre'))

# --- Signal generators
add = extend(EL, 'Generatoren')


X0 = -4 * P       # the left side of a generator's square


def signal_generator(caption, waveform, outputs=1):
    """The square from X0 to -P with G above the waveform; the output (the middle one of three) at the insertion point."""
    s = Symbol('G', caption)
    s.rect(X0, -1.5 * P, -P, 1.5 * P).text('G', X0 + 1.5 * P, -1.5 * P + 0.6, 2.5, 'centre')
    waveform(s)
    for k, y in enumerate([0] if outputs == 1 else [-P, 0, P]):
        s.line((-P, y), (0, y)).pin(str(k + 1), (0, y), label=(-0.4, y - 2.2), align='right')
    return s.labels((X0, -1.5 * P - 3.4), (X0, 1.5 * P + 0.6))


def sine(s, x, y, d=2.0):
    return s.arc(x - d / 2, y, d, 0, 180).arc(x + d / 2, y, d, 180, 360)


add(signal_generator(C('Pulsgenerator', 'Pulse generator', "Générateur d'impulsions"),
                     lambda s: s.line(*[(X0 + x, y) for x, y in ((1.2, 2.0), (2.4, 2.0), (2.4, 0.2), (5.2, 0.2), (5.2, 2.0), (6.4, 2.0))])))
add(signal_generator(C('Sinusgenerator', 'Sine-wave generator', 'Générateur sinusoïdal'), lambda s: sine(s, X0 + 1.5 * P, 1.2)))
add(signal_generator(C('Sinusgenerator, dreiphasig', 'Sine-wave generator, three-phase', 'Générateur sinusoïdal triphasé'),
                     lambda s: sine(s.text('3', X0 + 2.0, 0.0, 2.2, 'centre'), X0 + 1.5 * P + 0.8, 1.2, 1.6), outputs=3))
add(signal_generator(C('Sägezahngenerator', 'Sawtooth generator', 'Générateur de dents de scie'),
                     lambda s: s.line(*[(X0 + x, y) for x, y in ((1.2, 2.0), (3.8, 0.2), (3.8, 2.0), (6.4, 0.2), (6.4, 2.0))])))

# --- Ground: the short lead ending in a heavy bar
add = extend(EL, 'Masse / Erde')
s = Symbol('', C('Masse (Balken)', 'Chassis (bar)', 'Masse (barre)'), numbered=False, listed=False, shown=False)
add(s.pin('1', (0, 0)).line((0, 0), (0, P)).rect(-2.0, P, 2.0, P + 0.8, fill='#000000').labels((2.2, 0), None))

# --- Signalling devices
add = extend(EL, 'Signalgeber')
s = two_pin('P', C('Lampe (senkrecht)', 'Lamp, vertical', 'Lampe verticale'))
s.line((0, 0), (P + 0.3, 0)).circle(2 * P, 0, 2 * P - 0.6).line((3 * P - 0.3, 0), (4 * P, 0))
s.line(rot((2 * P - 3.3, 0), (2 * P, 0), 45), rot((2 * P + 3.3, 0), (2 * P, 0), 45)).line(rot((2 * P - 3.3, 0), (2 * P, 0), -45), rot((2 * P + 3.3, 0), (2 * P, 0), -45))
add(s.vertical().labels((3.0, P - 0.4), (3.0, 2 * P + 0.4)))


def device(caption, pins):
    s = Symbol('P', caption)
    for k, at in enumerate(pins):
        s.pin(str(k + 1), at)
    return s


def dome_down(caption):
    """A dome over its base, the leads from the base down to (0, 0) and (P, 0)."""
    s = device(caption, ((0, 0), (P, 0)))
    s.line((0, 0), (0, -P)).line((P, 0), (P, -P)).line((-P / 2, -P), (1.5 * P, -P), w=0.35).arc(P / 2, -P, 2 * P, 0, 180)
    return s.labels((1.5 * P + 1.0, -2 * P), (1.5 * P + 1.0, -2 * P + 3.0))


def dome_left(caption):
    """A dome turned to the right, its base upright, the leads from the left to (0, 0) and (0, P)."""
    s = device(caption, ((0, 0), (0, P)))
    s.line((0, 0), (P, 0)).line((0, P), (P, P)).line((P, -P / 2), (P, 1.5 * P), w=0.35).arc(P, P / 2, 2 * P, 270, 450)
    return s.labels((2 * P + 1.0, -P / 2 - 1.0), (2 * P + 1.0, -P / 2 + 2.0))


def chevron(s, apex, inward):
    """The siren's mark inside the dome: an angle pointing at `apex`, its arms 1.2 back towards `inward`."""
    (x, y), (dx, dy) = apex, inward
    return s.line((x + 1.0 * dx - 1.2 * dy, y + 1.0 * dy - 1.2 * dx), (x, y), (x + 1.0 * dx + 1.2 * dy, y + 1.0 * dy + 1.2 * dx))


edge = math.sqrt(P * P - (P / 2) ** 2)    # where a leg P / 2 off the centre meets a dome of radius P
add(dome_left(C('Klingel (Anschlüsse links)', 'Bell, leads at the left', 'Sonnerie, connexions à gauche')))
add(dome_down(C('Gong', 'Gong', 'Gong')).line((P / 2, -P), (P / 2, -2 * P)))
add(dome_left(C('Gong (Anschlüsse links)', 'Gong, leads at the left', 'Gong, connexions à gauche')).line((P, P / 2), (2 * P, P / 2)))
s = device(C('Schnarre', 'Buzzer', 'Ronfleur'), ((0, 0), (P, 0)))
s.line((0, 0), (0, -2 * P + edge)).line((P, 0), (P, -2 * P + edge)).line((-P / 2, -2 * P), (1.5 * P, -2 * P), w=0.35).arc(P / 2, -2 * P, 2 * P, 180, 360)
add(s.labels((1.5 * P + 1.0, -2 * P), (1.5 * P + 1.0, -2 * P + 3.0)))
s = device(C('Schnarre (Anschlüsse links)', 'Buzzer, leads at the left', 'Ronfleur, connexions à gauche'), ((0, 0), (0, P)))
s.line((0, 0), (2 * P - edge, 0)).line((0, P), (2 * P - edge, P)).line((2 * P, -P / 2), (2 * P, 1.5 * P), w=0.35).arc(2 * P, P / 2, 2 * P, 90, 270)
add(s.labels((2 * P + 1.0, -P / 2 - 1.0), (2 * P + 1.0, -P / 2 + 2.0)))
add(chevron(dome_left(C('Sirene (Anschlüsse links)', 'Siren, leads at the left', 'Sirène, connexions à gauche')), (P + 2.2, P / 2), (-1, 0)))
s = device(C('Hupe (senkrecht)', 'Horn, vertical', 'Klaxon vertical'), ((0, 0), (P, 0)))
s.line((0, 0), (0, P)).rect(-1.2, P, 1.2, P + 1.6).poly((-1.2, P + 1.6), (1.2, P + 1.6), (2.6, P + 5.0), (-2.6, P + 5.0), fill=None)
add(s.line((1.2, P + 0.8), (P, P + 0.8), (P, 0)).labels((P + 1.2, P), (P + 1.2, P + 3.0)))
