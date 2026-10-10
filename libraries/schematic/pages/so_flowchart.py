# More flowchart symbols for page "Flowchart", after ISO 5807 / DIN 66001 (storage, manual input and operation,
# preparation, punched tape, display, connectors) plus the usual extra shapes of flowchart editors (delay, cloud,
# summing junction, flag, block arrow, merge triangles). Like the shapes already there: no designator, not listed, no
# contacts, 8 x 4 P; "Text" is a placeholder and the short arrow at the bottom chains the symbol to the next one.
add = extend(SO, 'Flowchart')
H = 4 * P           # height of a box
X = 4 * P           # its middle
D = 1.5 * P / math.sqrt(2)      # where the diagonal cross meets the circle of a summing junction


def flow(caption, draw):
    s = Symbol('', caption, numbered=False, listed=False, shown=False)
    draw(s)
    return s


def label(s, x, y, t='Text', h=2.5):
    """A text centred on (x, y)."""
    return s.text(t, x, y - h / 2, h, 'centre')


def onward(s, y=H, x=X):
    """The flow arrow leaving the symbol at the bottom."""
    return s.arrow((x, y), (x, y + 2 * P), size=1.6)


def boxed(caption, draw, middle=(X, H / 2), arrow=H):
    """A box with the placeholder text at middle and, unless arrow is None, the flow arrow leaving at (X, arrow)."""
    def inner(s):
        draw(s)
        label(s, *middle)
        if arrow is not None:
            onward(s, arrow)
    return flow(caption, inner)


def wave(s, x0, x1, y, a):
    """An S-shaped edge from (x0, y) to (x1, y), first bulging by a downwards."""
    k = (x1 - x0) / (8 * P)
    return s.bezier((x0, y), (x0 + 2.6 * P * k, y + a), (x0 + 5.4 * P * k, y - a), (x1, y))


def document(s, x, y, w, h):
    """The document sheet of page Flowchart: straight top and sides, wavy bottom edge."""
    s.line((x, y + h), (x, y), (x + w, y), (x + w, y + h))
    return wave(s, x, x + w, y + h, 1.5 * P)


def cloud(s, cx, cy, rx, ry, bumps=9):
    """A cloud of round bumps along an ellipse; returns the lowest point of its outline (where the arrow starts)."""
    lowest = None
    turn = [0.0, 0.8, -0.5, 0.4, -0.7, 0.6, -0.3, 0.5, -0.6]
    angles = [90 + (k + 0.5 + 0.18 * turn[k % len(turn)]) * 360 / bumps for k in range(bumps)]
    points = [(cx + rx * math.cos(math.radians(a)), cy + ry * math.sin(math.radians(a))) for a in angles]
    for k in range(bumps):
        a, b = points[k], points[(k + 1) % bumps]
        mx, my = (a[0] + b[0]) / 2, (a[1] + b[1]) / 2
        nx, ny = (mx - cx) / rx ** 2, (my - cy) / ry ** 2
        n = math.hypot(nx, ny)
        bulge = 0.55 * math.hypot(b[0] - a[0], b[1] - a[1])
        c1 = (a[0] + nx / n * bulge, a[1] + ny / n * bulge)
        c2 = (b[0] + nx / n * bulge, b[1] + ny / n * bulge)
        s.bezier(a, c1, c2, b)
        mid = ((a[0] + 3 * c1[0] + 3 * c2[0] + b[0]) / 8, (a[1] + 3 * c1[1] + 3 * c2[1] + b[1]) / 8)
        if lowest is None or mid[1] > lowest[1]:
            lowest = mid
    return lowest


def wolke(s):
    low = cloud(s, X, H / 2, 3.1 * P, 1.25 * P)
    label(s, X, H / 2)
    onward(s, low[1], low[0])


def platters(s):
    s.line((0, 0.6 * P), (0, 3.6 * P)).line((6 * P, 0.6 * P), (6 * P, 3.6 * P)).arc(3 * P, 0.6 * P, 6 * P, 0, 360, dy=1.2 * P)
    for k in (1, 2):
        s.arc(3 * P, (0.6 + 0.45 * k) * P, 6 * P, 180, 360, dy=1.2 * P)
    s.arc(3 * P, 3.6 * P, 6 * P, 180, 360, dy=1.2 * P)


def stacked(s):
    d, w, h = 0.5 * P, 7 * P, 3.2 * P
    s.line((2 * d, d), (2 * d, 0), (2 * d + w, 0), (2 * d + w, h), (d + w, h))
    s.line((d, 2 * d), (d, d), (d + w, d), (d + w, d + h), (w, d + h))
    document(s, 0, 2 * d, w, h)


def triangle_text(s, *corners):
    s.poly(*corners, fill=None)
    label(s, sum(c[0] for c in corners) / 3, sum(c[1] for c in corners) / 3, '...')


for de, en, fr, a, b in (('Ablaufpfeil nach oben', 'Flow arrow up', 'Flèche de flux vers le haut', (0, 4 * P), (0, 0)),
                         ('Ablaufpfeil nach links', 'Flow arrow to the left', 'Flèche de flux vers la gauche', (4 * P, 0), (0, 0)),
                         ('Ablaufpfeil nach rechts', 'Flow arrow to the right', 'Flèche de flux vers la droite', (0, 0), (4 * P, 0))):
    add(flow(C(de, en, fr), lambda s, a=a, b=b: s.arrow(a, b, size=1.6)))
add(flow(C('Nein', 'No', 'Non'), lambda s: s.text('Nein', 0, 0, 3.5)))
add(flow(C('Ja', 'Yes', 'Oui'), lambda s: s.text('Ja', 0, 0, 3.5)))
add(flow(C('Ende (mit Text)', 'End (with text)', 'Fin (avec texte)'), lambda s: label(s.rect(0, 0, 8 * P, 3 * P, corner=50), X, 1.5 * P, 'Ende')))
add(boxed(C('Interner Speicher', 'Internal storage', 'Mémoire interne'),
          lambda s: s.rect(0, 0, 8 * P, H).line((0, 0.6 * P), (8 * P, 0.6 * P)).line((0.6 * P, 0), (0.6 * P, H)), middle=(4.3 * P, 2.3 * P)))
add(boxed(C('Manuelle Eingabe', 'Manual input', 'Entrée manuelle'),
          lambda s: s.poly((0, 1.4 * P), (8 * P, 0), (8 * P, H), (0, H), fill=None), middle=(X, 2.4 * P)))
add(boxed(C('Verzögerung', 'Delay', 'Délai'),
          lambda s: s.line((6 * P, 0), (0, 0), (0, H), (6 * P, H)).arc(6 * P, 2 * P, 4 * P, -90, 90), middle=(3.6 * P, 2 * P)))
add(boxed(C('Manuelle Verarbeitung', 'Manual operation', 'Opération manuelle'),
          lambda s: s.poly((0, 0), (8 * P, 0), (7 * P, H), (P, H), fill=None)))
add(boxed(C('Vorbereitung', 'Preparation', 'Préparation'),
          lambda s: s.poly((P, 0), (7 * P, 0), (8 * P, 2 * P), (7 * P, H), (P, H), (0, 2 * P), fill=None)))
add(boxed(C('Mehrere Dokumente', 'Multiple documents', 'Documents multiples'), stacked, middle=(3.5 * P, 2.6 * P), arrow=None))
add(boxed(C('Lochstreifen', 'Punched tape', 'Bande perforée'),
          lambda s: wave(wave(s, 0, 8 * P, 0.5 * P, 1.2 * P), 0, 8 * P, 3.5 * P, 1.2 * P).line((0, 0.5 * P), (0, 3.5 * P)).line((8 * P, 0.5 * P), (8 * P, 3.5 * P)),
          middle=(X, 2 * P), arrow=3.5 * P))
add(boxed(C('Speicher mit Direktzugriff', 'Direct access storage', 'Mémoire à accès direct'),
          lambda s: s.line((0.6 * P, 0), (7.4 * P, 0)).line((0.6 * P, H), (7.4 * P, H)).arc(0.6 * P, 2 * P, 1.2 * P, 90, 270, dy=H).arc(7.4 * P, 2 * P, 1.2 * P, 0, 360, dy=H),
          middle=(3.8 * P, 2 * P), arrow=None))
add(boxed(C('Gespeicherte Daten', 'Stored data', 'Données mémorisées'),
          lambda s: s.line((P, 0), (8 * P, 0)).line((P, H), (8 * P, H)).arc(P, 2 * P, 2 * P, 90, 270, dy=H).arc(8 * P, 2 * P, 2 * P, 90, 270, dy=H),
          middle=(4.2 * P, 2 * P), arrow=None))
add(boxed(C('Anzeige', 'Display', 'Affichage'),
          lambda s: s.line((6.5 * P, 0), (1.5 * P, 0), (0, 2 * P), (1.5 * P, H), (6.5 * P, H)).arc(6.5 * P, 2 * P, 3 * P, -90, 90, dy=H),
          middle=(4.2 * P, 2 * P), arrow=None))
add(flow(C('Wolke', 'Cloud', 'Nuage'), wolke))
add(boxed(C('Plattenspeicher', 'Disk storage', 'Mémoire à disques'), platters, middle=(3 * P, 2.8 * P), arrow=None))
add(flow(C('Summierstelle', 'Summing junction', 'Point de sommation'),
         lambda s: s.circle(0, 0, 3 * P).line((-D, -D), (D, D)).line((-D, D), (D, -D))))
add(flow(C('Oder-Verknüpfung', 'Logical OR', 'OU logique'),
         lambda s: s.circle(0, 0, 3 * P).line((-1.5 * P, 0), (1.5 * P, 0)).line((0, -1.5 * P), (0, 1.5 * P))))
add(flow(C('Verbinder mit Nummer', 'Numbered connector', 'Connecteur numéroté'),
         lambda s: label(s.rect(-1.2 * P, -1.2 * P, 1.2 * P, 1.2 * P).circle(0, 0, 2 * P), 0, 0, '1')))
add(flow(C('Seitenverbinder', 'Off-page connector', 'Connecteur hors page'),
         lambda s: label(s.poly((0, 0), (5 * P, 0), (6 * P, P), (5 * P, 2 * P), (0, 2 * P), fill=None), 2.6 * P, P)))
add(flow(C('Fahne', 'Flag', 'Fanion'),
         lambda s: label(s.poly((0, 0), (6 * P, 0), (5.2 * P, P), (6 * P, 2 * P), (0, 2 * P), fill=None), 2.7 * P, P)))
add(flow(C('Pfeilkasten', 'Block arrow', 'Flèche bloc'),
         lambda s: label(s.poly((0, 0.5 * P), (5.5 * P, 0.5 * P), (5.5 * P, 0), (7 * P, 1.5 * P), (5.5 * P, 3 * P), (5.5 * P, 2.5 * P), (0, 2.5 * P), fill=None), 2.75 * P, 1.5 * P)))
add(flow(C('Dreieck nach links', 'Triangle pointing left', 'Triangle vers la gauche'),
         lambda s: triangle_text(s, (0, 2 * P), (3.5 * P, 0), (3.5 * P, H))))
add(flow(C('Dreieck nach rechts', 'Triangle pointing right', 'Triangle vers la droite'),
         lambda s: triangle_text(s, (0, 0), (3.5 * P, 2 * P), (0, H))))
add(flow(C('Zusammenführen', 'Merge', 'Fusion'),
         lambda s: triangle_text(s, (0, 0), (H, 0), (2 * P, 3.5 * P))))
