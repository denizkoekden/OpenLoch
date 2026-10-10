# More fuses and protective devices for the page "Sicherungen", after DIN EN 60617-7: glass-tube fuses, numbered fuses,
# multi-pole circuit breakers, fuse-disconnectors, switch-fuses and residual current devices. The poles of a multi-pole
# device stand 2 * P apart, are drawn like contact() draws a make contact and are joined by a dashed mechanical link.
add = extend(EI, 'Sicherungen')
TIP = (-1.9, P + 0.6)      # the free end of a make contact's blade, pivot at (0, 3 * P)


def blade_at(x, y):
    """The point at height y on the blade of the pole at x."""
    return (x + TIP[0] * (3 * P - y) / (3 * P - TIP[1]), y)


def pole(s, x, names, mark=None, length=4 * P, texts=(None, None)):
    """A make contact from (x, 0) to (x, length) with its terminal numbers shown; mark is the function at the fixed
    contact: 'breaker' (cross), 'disconnector' (bar) or 'switch-disconnector' (circle)."""
    s.line((x, 0), (x, P)).line((x, 3 * P), (x, length)).line((x, 3 * P), (x + TIP[0], TIP[1]))
    if mark == 'breaker':
        s.line((x - 0.8, P - 0.8), (x + 0.8, P + 0.8)).line((x - 0.8, P + 0.8), (x + 0.8, P - 0.8))
    elif mark == 'disconnector':
        s.line((x - 0.9, P), (x + 0.9, P))
    elif mark == 'switch-disconnector':
        s.circle(x, P + 0.5, 1.0)
    s.pin(names[0], (x, 0), label=(x + 0.9, -0.2), shown=True, text=texts[0])
    s.pin(names[1], (x, length), label=(x + 0.8, length - 2.4), shown=True, text=texts[1])
    return s


def release(s, x):
    """Automatic release: an arrow from the blade towards the open position."""
    b = blade_at(x, 3.9)
    return s.arrow(b, (b[0] - 2.6, b[1]), size=0.9)


def fuse_on_blade(s, x):
    pivot = (x, 3 * P)
    dx, dy = TIP[0], TIP[1] - 3 * P
    n = math.hypot(dx, dy)
    ux, uy = dx / n, dy / n
    cx, cy = pivot[0] + 0.45 * dx, pivot[1] + 0.45 * dy
    a, b = 1.2, 0.6
    s.poly((cx + a * ux - b * uy, cy + a * uy + b * ux), (cx + a * ux + b * uy, cy + a * uy - b * ux),
           (cx - a * ux + b * uy, cy - a * uy - b * ux), (cx - a * ux - b * uy, cy - a * uy + b * ux), fill=None)
    return s


def link(s, xs, y):
    return dashed(s, blade_at(xs[0], y), blade_at(xs[-1], y))


def device(caption, n, mark=None, auto=False, fuse=False, value=''):
    s = Symbol('F', caption, value)
    xs = [2 * P * k for k in range(n)]
    for k, x in enumerate(xs):
        pole(s, x, (str(2 * k + 1), str(2 * k + 2)), mark)
        if auto:
            release(s, x)
        if fuse:
            fuse_on_blade(s, x)
    if n > 1:
        link(s, xs, 2 * P + 0.6 if auto else 3.85 if fuse else 2 * P - 0.4)
    return s.labels((xs[-1] + 2.6, P + 0.4), (xs[-1] + 2.6, 2 * P + 0.6))


POLES = {2: ('zweipolig', 'two-pole', 'bipolaire'), 3: ('dreipolig', 'three-pole', 'tripolaire'),
         4: ('vierpolig', 'four-pole', 'tétrapolaire')}


def poles(de, en, fr, n):
    if n == 1:
        return C(de, en, fr)
    d, e, f = POLES[n]
    return C(f'{de} ({d})', f'{en}, {e}', f'{fr} {f}')


# Glass-tube fuses: the fuse with its end caps
s = Symbol('F', C('Feinsicherung', 'Miniature fuse', 'Fusible miniature'), ask=True)
s.rect(-1.0, P, 1.0, 3 * P).line((0, 0), (0, 4 * P)).line((-1.0, P + 0.7), (1.0, P + 0.7)).line((-1.0, 3 * P - 0.7), (1.0, 3 * P - 0.7))
add(s.pin('1', (0, 0), label=(0.6, 0.2)).pin('2', (0, 4 * P), label=(0.6, 4 * P - 2.4)).labels((2.0, P), (2.0, 2 * P + 0.4)))
s = Symbol('F', C('Feinsicherung (waagerecht)', 'Miniature fuse, horizontal', 'Fusible miniature horizontal'), ask=True)
s.rect(P, -1.0, 3 * P, 1.0).line((0, 0), (4 * P, 0)).line((P + 0.7, -1.0), (P + 0.7, 1.0)).line((3 * P - 0.7, -1.0), (3 * P - 0.7, 1.0))
add(s.pin('1', (0, 0)).pin('2', (4 * P, 0), label=(4 * P - 0.4, -2.2), align='right').labels((2 * P, -4.6), (2 * P, 1.6), 'centre'))

for n in (2, 3):
    add(device(poles('Leitungsschutzschalter', 'Circuit breaker', 'Disjoncteur', n), n, 'breaker'))
for n in (1, 2, 3):
    add(device(poles('Sicherungsautomat', 'Automatic circuit breaker', 'Disjoncteur automatique', n), n, auto=True))

s = Symbol('F', C('Thermisches Überlastrelais (dreipolig)', 'Thermal overload relay, three-pole', 'Relais thermique tripolaire'))
s.rect(-P, P, 5 * P, 3 * P)
for k in range(3):
    x = 2 * P * k
    s.line((x, 0), (x, P + 1.0), (x - 1.2, P + 1.0), (x - 1.2, 3 * P - 1.0), (x, 3 * P - 1.0), (x, 4 * P))
    s.pin(str(2 * k + 1), (x, 0), label=(x + 0.6, 0.2), shown=True).pin(str(2 * k + 2), (x, 4 * P), label=(x + 0.6, 4 * P - 2.1), shown=True)
add(s.labels((5 * P + 0.8, P), (5 * P + 0.8, 2 * P + 0.4)))


def rcd(caption, texts):
    """Residual current device: make contacts with automatic release, the summation current transformer (a ring around
    all conductors) and the release it feeds, which acts on the contacts through the dashed link."""
    s = Symbol('F', caption)
    n = len(texts) // 2
    xs = [2 * P * k for k in range(n)]
    ring = 5 * P
    for k, x in enumerate(xs):
        pole(s, x, (str(2 * k + 1), str(2 * k + 2)), length=7 * P, texts=texts[2 * k:2 * k + 2])
        release(s, x)
    s.parts.append({'type': 'ellipse', 'centre': pt(((xs[0] + xs[-1]) / 2, ring)), 'size': [r(xs[-1] - xs[0] + 3.2), 1.8],
                    'pen': {'width': W}})
    s.rect(-8.4, ring - 1.8, -4.4, ring + 1.8).text('IΔ', -6.4, ring - 1.2, 2.0, 'centre').line((-4.4, ring), (xs[0] - 1.6, ring))
    dashed(s, (-6.4, ring - 1.8), (-6.4, 2 * P + 0.6), blade_at(xs[-1], 2 * P + 0.6))
    return s.labels((xs[-1] + 2.6, P + 0.4), (xs[-1] + 2.6, 2 * P + 0.6))


add(rcd(C('FI-Schutzschalter (zweipolig)', 'Residual current device, two-pole', 'Interrupteur différentiel bipolaire'), ('1', '2', 'N', 'N')))
add(rcd(C('FI-Schutzschalter (vierpolig)', 'Residual current device, four-pole', 'Interrupteur différentiel tétrapolaire'),
        ('1', '2', '3', '4', '5', '6', 'N', 'N')))

for de, en, fr, mark in (('Sicherungstrennschalter', 'Fuse-disconnector', 'Sectionneur-fusible', 'disconnector'),
                         ('Sicherungslasttrennschalter', 'Fuse-switch-disconnector', 'Interrupteur-sectionneur-fusible', 'switch-disconnector')):
    for n in (1, 3):
        add(device(poles(de, en, fr, n), n, mark, fuse=True))

# Fuses with their terminal numbers shown
for n in (1, 2, 3):
    if n == 1:
        s = Symbol('F', C('Schmelzsicherung', 'Fuse with terminal numbers', 'Fusible avec numéros de bornes'))
    else:
        s = Symbol('F', poles('Schmelzsicherung', 'Fuse', 'Fusible', n))
    for k in range(n):
        x = 2 * P * k
        s.rect(x - 1.0, P, x + 1.0, 3 * P).line((x, 0), (x, 4 * P))
        s.pin(str(2 * k + 1), (x, 0), label=(x + 0.6, 0.2), shown=True).pin(str(2 * k + 2), (x, 4 * P), label=(x + 0.6, 4 * P - 2.1), shown=True)
    add(s.labels((2 * P * (n - 1) + 2.0, P), (2 * P * (n - 1) + 2.0, 2 * P + 0.4)))


def switch_fuse(caption, n, fused):
    """Make contacts on one manually operated link, a fuse in series below the contact of the first `fused` poles."""
    s = Symbol('F', caption)
    xs = [2 * P * k for k in range(n)]
    for k, x in enumerate(xs):
        pole(s, x, (str(2 * k + 1), str(2 * k + 2)), length=6 * P)
        if k < fused:
            s.rect(x - 1.0, 3 * P + 0.5, x + 1.0, 5 * P - 0.1)
    y = 2 * P - 0.4
    dashed(s, (-4.4, y), blade_at(xs[-1], y))
    s.line((-4.4, y - 1.2), (-4.4, y + 1.2))
    return s.labels((xs[-1] + 2.6, P + 0.4), (xs[-1] + 2.6, 2 * P + 0.6))


add(switch_fuse(C('Sicherungsschalter (zweipolig)', 'Switch-fuse, two-pole', 'Interrupteur-fusible bipolaire'), 2, 1))
add(switch_fuse(C('Sicherungsschalter (vierpolig)', 'Switch-fuse, four-pole', 'Interrupteur-fusible tétrapolaire'), 4, 3))
