# Function blocks for the page "Übertragungswege" after DIN EN 60617-10 (IEC 60617-10, transmission): a square of
# 4 × 2.54 mm with the input lead at the left (1) and the output lead at the right (2), a third lead below where the
# block has one, the function sign inside. Converters are divided by the diagonal, input quantity upper left, output
# lower right; ~ signs stacked three high stand for the frequency bands (top: the highest), struck out where stopped.
add = extend(EL_E, 'Übertragungswege')
CX = 3 * P     # middle of the square


def block(caption, draw, third=False):
    s = Symbol('', caption, numbered=False)
    s.line((0, 0), (P, 0)).rect(P, -2 * P, 5 * P, 2 * P, w=0.35).line((5 * P, 0), (6 * P, 0))
    s.pin('1', (0, 0)).pin('2', (6 * P, 0), label=(6 * P - 0.4, -2.2), align='right')
    if third:
        s.line((CX, 2 * P), (CX, 3 * P)).pin('3', (CX, 3 * P), label=(CX + 0.4, 3 * P - 2.2))
    draw(s)
    s.labels((5 * P + 1.0, -2 * P - 3.4), (5 * P + 1.0, 2 * P + 0.6))
    return s


def wave(s, x, y, w=5.6, h=0.8):
    """A ~: one period of a wave from x to x + w about y, h high."""
    return s.bezier((x, y), (x + w / 3, y - 3.4 * h), (x + 2 * w / 3, y + 3.4 * h), (x + w, y))


def bands(s, stopped=(), x=CX - 2.8, w=5.6):
    for k, y in enumerate((-2.6, 0, 2.6)):
        wave(s, x, y, w)
        if k in stopped:
            s.line((x + w / 2 - 0.9, y + 1.4), (x + w / 2 + 0.9, y - 1.4))
    return s


def diagonal(s):
    return s.line((P, 2 * P), (5 * P, -2 * P))


def dc(s, x, y, w=3.6):
    return s.line((x - w / 2, y), (x + w / 2, y), w=0.35)


def modulated(s, x, y):
    """An amplitude-modulated wave, its middle at (x, y)."""
    heights = (0.6, 1.4, 2.2, 2.2, 1.4, 0.6)
    for k, h in enumerate(heights):
        up = k % 2 == 0
        s.arc(x - 2.25 + 0.375 + k * 0.75, y, 0.75, 0 if up else 180, 180 if up else 360, dy=h)
    return s


def splitter(s, y, stem):
    return s.line((CX - 2.4, y - 2.6), (CX, y), (CX + 2.4, y - 2.6)).line((CX, y), (CX, stem))


add(block(C('Netzanschlussgerät', 'Power supply unit', "Bloc d'alimentation secteur"), lambda s: dc(wave(diagonal(s), CX - 4.2, -2.6, 3.6, 0.6), CX + 2.4, 2.6)))
add(block(C('Stabilisiereinrichtung', 'Stabiliser', 'Stabilisateur'), lambda s: dc(dc(diagonal(s), CX - 2.4, -2.6), CX + 2.4, 2.6)))
for de, en, fr, stopped in (('Tiefpass', 'Low-pass filter', 'Filtre passe-bas', (0, 1)), ('Hochpass', 'High-pass filter', 'Filtre passe-haut', (1, 2)),
                            ('Bandpass', 'Band-pass filter', 'Filtre passe-bande', (0, 2)), ('Bandsperre', 'Band-stop filter', 'Filtre coupe-bande', (1,))):
    add(block(C(de, en, fr), lambda s, stopped=stopped: bands(s, stopped)))
add(block(C('Pilotgeber', 'Pilot generator', "Générateur d'onde pilote"), lambda s: bands(s.text('G', CX - 3.0, -1.5, 3.0, 'centre'), x=CX - 1.0, w=4.4)))
add(block(C('Modulator', 'Modulator', 'Modulateur'),
          lambda s: modulated(wave(wave(diagonal(s), CX - 4.2, -3.3, 3.6, 0.45), CX - 4.2, -2.0, 3.6, 0.45), CX + 2.4, 2.4)))
add(block(C('Weiche', 'Frequency splitter', 'Diplexeur'), lambda s: splitter(s, -0.4, 3.4), third=True))
add(block(C('Einspeiseweiche', 'Power inserter', "Injecteur d'alimentation"),
          lambda s: wave(splitter(s, -1.8, -0.8).arrow((CX, 4.0), (CX, -0.6), size=1.2), CX + 0.8, 2.4, 3.2, 0.6), third=True))


def attenuator(s, y=-1.4):
    # A T pad: two resistors in the line, one across.
    s.line((P + 0.8, y), (CX - 3.5, y)).rect(CX - 3.5, y - 0.5, CX - 1.1, y + 0.5).line((CX - 1.1, y), (CX + 1.1, y))
    s.rect(CX + 1.1, y - 0.5, CX + 3.5, y + 0.5).line((CX + 3.5, y), (5 * P - 0.8, y))
    s.line((CX, y), (CX, y + 0.6)).rect(CX - 0.5, y + 0.6, CX + 0.5, y + 3.0).line((CX, y + 3.0), (CX, y + 3.8))
    return s.line((CX - 1.2, y + 3.8), (CX + 1.2, y + 3.8))


def equaliser(s):
    # The loss rising over the frequency f, as the equaliser makes up for it.
    s.arrow((CX - 3.4, 2.4), (CX + 3.6, 2.4), size=1.0).arrow((CX - 3.4, 2.4), (CX - 3.4, -3.8), size=1.0)
    s.text('f', CX + 3.6, 2.6, 1.8, 'right')
    return s.bezier((CX - 2.8, 1.2), (CX + 0.4, 1.2), (CX + 1.8, 0.2), (CX + 3.0, -3.0))


add(block(C('Dämpfungsglied', 'Attenuator', 'Atténuateur'), attenuator))
add(block(C('Entzerrer', 'Equaliser', 'Égaliseur'), equaliser))
add(block(C('Trennglied', 'DC block', 'Bloqueur de courant continu'),
          lambda s: s.line((CX - 2.6, 0), (CX - 0.5, 0)).line((CX - 0.5, -2.2), (CX - 0.5, 2.2), w=THICK).line((CX + 0.5, -2.2), (CX + 0.5, 2.2), w=THICK)
          .line((CX + 0.5, 0), (CX + 2.6, 0))))
add(block(C('Übertrager (Block)', 'Transformer (block)', 'Transformateur (bloc)'),
          lambda s: s.arc(CX - 1.6, 0, 2.4, 270, 450, dy=6.0).arc(CX + 1.6, 0, 2.4, 90, 270, dy=6.0)))
