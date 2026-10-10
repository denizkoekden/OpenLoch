# Electroacoustics, continued: microphones and condenser microphones in more positions, loudspeakers in more forms,
# a dome tweeter and the earphone. After DIN EN 60617-9; circle, plate and the loudspeaker's box and cone as in the
# existing "Mikrofon" and "Lautsprecher".
add = extend(BT, 'Elektroakustik')
R = 2.6     # radius of the microphone's circle


def capsule(s, cx, cy, upright=True):
    """A capacitor sign in the microphone's circle, its plates parallel to the microphone's plate."""
    f = (lambda u, v: (cx + u, cy + v)) if upright else (lambda u, v: (cx + v, cy + u))
    s.line(f(-1.8, 0), f(-0.45, 0)).line(f(0.45, 0), f(1.8, 0))
    return s.line(f(-0.45, -1.3), f(-0.45, 1.3), w=0.35).line(f(0.45, -1.3), f(0.45, 1.3), w=0.35)


def mic_left(caption, condenser=False):
    """As the existing "Mikrofon": the leads from the left to the plate."""
    s = Symbol('B', caption)
    s.pin('1', (0, 0)).pin('2', (0, 2 * P), label=(0.4, 2 * P - 2.2))
    s.line((0, 0), (P, 0), (P, 0.6)).line((0, 2 * P), (P, 2 * P), (P, 2 * P - 0.6)).circle(P + R, P, 2 * R).line((P, P - R), (P, P + R), w=THICK)
    if condenser:
        capsule(s, P + R, P)
    return s.labels((P + 6.2, -1.0), (P + 6.2, 2.0))


def mic_top(caption, condenser=False):
    """The leads go up from the circle, the plate lies at the bottom."""
    s = Symbol('B', caption)
    cx, cy = P / 2, P + R
    foot = cy - math.sqrt(R * R - cx * cx)
    s.pin('1', (0, 0), label=(-0.4, -2.2), align='right').pin('2', (P, 0), label=(P + 0.4, -2.2))
    s.line((0, 0), (0, foot)).line((P, 0), (P, foot)).circle(cx, cy, 2 * R).line((cx - R, cy + R), (cx + R, cy + R), w=THICK)
    if condenser:
        capsule(s, cx, cy, upright=False)
    return s.labels((cx + R + 1.0, P - 1.0), (cx + R + 1.0, 2 * P))


def mic_upright(caption, condenser=False):
    """Upright: one lead up and one down from the left of the circle, the plate at the right."""
    s = Symbol('B', caption)
    cx, cy = P / 2, 2 * P
    reach = math.sqrt(R * R - cx * cx)
    s.pin('1', (0, 0), label=(-0.4, -2.2), align='right').pin('2', (0, 4 * P), label=(-0.4, 4 * P - 2.2), align='right')
    s.line((0, 0), (0, cy - reach)).line((0, cy + reach), (0, 4 * P)).circle(cx, cy, 2 * R).line((cx + R, cy - R), (cx + R, cy + R), w=THICK)
    if condenser:
        capsule(s, cx, cy)
    return s.labels((cx + R + 1.0, P - 0.4), (cx + R + 1.0, 2 * P + 0.4))


add(mic_top(C('Mikrofon (Anschlüsse oben)', 'Microphone (connections at the top)', 'Microphone (connexions en haut)')))
add(mic_upright(C('Mikrofon (senkrecht)', 'Microphone, vertical', 'Microphone vertical')))
add(mic_left(C('Kondensatormikrofon', 'Condenser microphone', 'Microphone à condensateur'), True))
add(mic_top(C('Kondensatormikrofon (Anschlüsse oben)', 'Condenser microphone (connections at the top)', 'Microphone à condensateur (connexions en haut)'), True))
add(mic_upright(C('Kondensatormikrofon (senkrecht)', 'Condenser microphone, vertical', 'Microphone à condensateur vertical'), True))


def speaker(caption, value=''):
    """The leads from the left into the box of the drive; the cone is drawn by the caller."""
    s = Symbol('B', caption, value)
    s.pin('1', (0, 0)).pin('2', (0, 2 * P), label=(0.4, 2 * P - 2.2))
    return s.line((0, 0), (P, 0)).line((0, 2 * P), (P, 2 * P)).rect(P, -0.6, P + 2.0, 2 * P + 0.6)


def cone(s, x, w=3.0):
    return s.poly((x, -0.6), (x + w, -3.0), (x + w, 2 * P + 3.0), (x, 2 * P + 0.6), fill=None)


s = Symbol('B', C('Lautsprecher (Anschlüsse oben)', 'Loudspeaker (connections at the top)', 'Haut-parleur (connexions en haut)'))
s.pin('1', (0, 0), label=(-0.4, -2.2), align='right').pin('2', (2 * P, 0), label=(2 * P + 0.4, -2.2))
s.line((0, 0), (0, P)).line((2 * P, 0), (2 * P, P)).rect(-0.6, P, 2 * P + 0.6, P + 2.0)
s.poly((-0.6, P + 2.0), (-3.0, P + 5.0), (2 * P + 3.0, P + 5.0), (2 * P + 0.6, P + 2.0), fill=None)
add(s.labels((2 * P + 3.6, -1.0), (2 * P + 3.6, 2.0)))
s = cone(speaker(C('Lautsprecher mit Polung', 'Loudspeaker with polarity', 'Haut-parleur avec polarité'), '8 Ω'), P + 2.0)
add(s.text('+', P / 2, -2.8, 2.2, 'centre').text('−', P / 2, 2 * P + 0.5, 2.2, 'centre').labels((P + 6.0, -1.0), (P + 6.0, 2.0)))
s = speaker(C('Lautsprecher (mit Membranrand)', 'Loudspeaker (with cone surround)', 'Haut-parleur (avec suspension de membrane)'))
s.line((P + 2.0, -0.6), (P + 5.0, -3.0)).line((P + 2.0, 2 * P + 0.6), (P + 5.0, 2 * P + 3.0))
s.parts.append({'type': 'ellipse', 'centre': pt((P + 5.0, P)), 'size': [1.6, 2 * P + 6.0], 'pen': {'width': W}})
add(s.labels((P + 6.6, -1.0), (P + 6.6, 2.0)))
s = speaker(C('Lautsprecher, dynamisch (Magnet)', 'Dynamic loudspeaker (magnet)', 'Haut-parleur dynamique (aimant)'))
s.rect(P + 2.0, -1.2, P + 2.8, 2 * P + 1.2, fill='#000000')
add(cone(s, P + 2.8).labels((P + 6.8, -1.0), (P + 6.8, 2.0)))
s = Symbol('B', C('Kalottenlautsprecher', 'Dome tweeter', 'Tweeter à dôme'), numbered=False)
s.pin('1', (0, 0)).pin('2', (0, 2 * P), label=(0.4, 2 * P - 2.2))
s.line((0, 0), (P, 0)).line((0, 2 * P), (P, 2 * P)).rect(P, -1.0, P + 2.0, 2 * P + 1.0).arc(P + 2.0, P, 4.4, 270, 450)
add(s.text('+', P / 2, -2.8, 2.2, 'centre').text('−', P / 2, 2 * P + 0.5, 2.2, 'centre').labels((P + 5.0, -1.0), (P + 5.0, 2.0)))

# The earphone (telephone receiver): a box with the heavy plate on the side the sound leaves.
s = Symbol('B', C('Hörer', 'Earphone', 'Écouteur'))
s.pin('1', (0, 0), label=(-0.4, -2.2), align='right').pin('2', (2 * P, 0), label=(2 * P + 0.4, -2.2))
s.line((0, 0), (0, P)).line((2 * P, 0), (2 * P, P)).rect(-1.0, P, 2 * P + 1.0, P + 3.0).line((-1.6, P + 3.0), (2 * P + 1.6, P + 3.0), w=THICK)
add(s.labels((2 * P + 2.4, P - 0.4), (2 * P + 2.4, 2 * P + 0.4)))
s = Symbol('B', C('Hörer (waagerecht)', 'Earphone, horizontal', 'Écouteur horizontal'))
s.pin('1', (0, 0)).pin('2', (0, 2 * P), label=(0.4, 2 * P - 2.2))
s.line((0, 0), (P, 0)).line((0, 2 * P), (P, 2 * P)).rect(P, -1.0, P + 3.0, 2 * P + 1.0).line((P + 3.0, -1.6), (P + 3.0, 2 * P + 1.6), w=THICK)
add(s.labels((P + 4.2, -1.0), (P + 4.2, 2.0)))
