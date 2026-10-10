# More terminals for the page "Klemmen", after DIN EN 60617-3 (terminal: small circle): through terminals (the lead
# passes through, connection point at the upper end), terminals at the end of a lead, and numbered terminal strips
# like the existing ones. Terminals side by side stand 2 * P apart, the designator once at the left.
add = extend(EI, 'Klemmen')


def terminals(caption, names, through):
    s = Symbol('X', caption)
    for k, name in enumerate(names):
        x = 2 * P * k
        s.line((x, 0), (x, P - 0.8)).circle(x, P, 1.6)
        if through:
            s.line((x, P + 0.8), (x, 2 * P))
        s.pin(name, (x, 0), label=(x + 1.3, P - 0.9), shown=True)
    return s.labels((-1.4, P - 1.4), (-1.4, P + 1.2), 'right')


for names in (['L1'], ['L2'], ['L3'], ['N'], ['PE'], ['L', 'N', 'PE'], ['L1', 'L2', 'L3', 'PE'], ['L1', 'L2', 'L3', 'N', 'PE']):
    t = '/'.join(names)
    if len(names) == 1:
        add(terminals(C(f'Reihenklemme {t}', f'Feed-through terminal {t}', f'Borne de passage {t}'), names, True))
    else:
        add(terminals(C(f'Reihenklemmen {t}', f'Feed-through terminals {t}', f'Bornes de passage {t}'), names, True))

for n in (3, 5, 8, 10):
    s = Symbol('X', C(f'Klemmleiste {n}-polig', f'Terminal strip, {n} poles', f'Bornier, {n} pôles'))
    for k in range(n):
        x = k * P
        s.rect(x - P / 2, -P, x + P / 2, P).circle(x, 0, 1.4).text(str(k + 1), x, P + 0.4, 1.8, 'centre')
        s.pin(str(k + 1), (x, 0))
    add(s.labels((-P / 2, -P - 3.4), (-P / 2, P + 3.0)))

for names in (['L1'], ['L', 'N', 'PE'], ['L1', 'L2', 'L3', 'PE'], ['L1', 'L2', 'L3', 'N', 'PE']):
    t = '/'.join(names)
    if len(names) == 1:
        add(terminals(C(f'Anschlussklemme {t}', f'Connection terminal {t}', f'Borne de raccordement {t}'), names, False))
    else:
        add(terminals(C(f'Anschlussklemmen {t}', f'Connection terminals {t}', f'Bornes de raccordement {t}'), names, False))
add(terminals(C('Anschlussklemme 1', 'Connection terminal 1', 'Borne de raccordement 1'), ['1'], False))
for n in (2, 3, 4, 5, 6, 8, 10):
    add(terminals(C(f'Anschlussklemmen {n}-polig', f'Connection terminals, {n} poles', f'Bornes de raccordement, {n} pôles'),
                  [str(k + 1) for k in range(n)], False))
