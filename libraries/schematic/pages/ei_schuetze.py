# Contacts and operating coils of relays and contactors: the page "Schütze + Kontakte [K]" is extended and the same set
# makes the page "Schütze + Kontakte [Q]" for power switching devices. Contact functions after DIN EN 60617-7 (07-01,
# 07-02), operating devices after DIN EN 60617-7 (07-15). Coils are parents; the contacts are placed as their children.


def pole(prefix, caption, kind, pins=('1', '2'), mark=False, raised=False):
    """A contact like contact(..., numbers=True): returns the symbol, the blade's pivot and its free end. With a mark in
    the corner of a break contact the upper terminal number moves to the left, `raised` lifts it above a cross."""
    s = Symbol(prefix, caption)
    top = dict(label=(-0.4, 0.2), align='right') if mark else dict(label=(0.9, -0.6) if raised else (0.8, 0.2))
    s.pin(pins[0], (0, 0), shown=True, **top).pin(pins[1], (0, 4 * P), label=(0.8, 4 * P - 2.4), shown=True)
    s.line((0, 0), (0, P)).line((0, 3 * P), (0, 4 * P))
    if kind == 'no':
        tip = (-1.9, P + 0.6)
    else:
        s.line((0, P), (1.6, P))
        tip = (2.1, P - 0.4)
    s.line((0, 3 * P), tip)
    s.labels((2.6, P + 0.4), (2.6, 2 * P + 0.6))
    return s, (0, 3 * P), tip


def on_blade(pivot, tip, t):
    """The point at t along the blade, the blade's direction and the normal pointing away to its left."""
    dx, dy = tip[0] - pivot[0], tip[1] - pivot[1]
    length = math.hypot(dx, dy)
    u = (dx / length, dy / length)
    return (pivot[0] + t * dx, pivot[1] + t * dy), u, (u[1], -u[0])


def tripping(s, pivot, tip):
    """The filled block of automatic tripping on the moving contact."""
    c, u, n = on_blade(pivot, tip, 0.62)
    a, b = (c[0] - 0.5 * u[0], c[1] - 0.5 * u[1]), (c[0] + 0.5 * u[0], c[1] + 0.5 * u[1])
    s.poly(a, b, (b[0] + n[0], b[1] + n[1]), (a[0] + n[0], a[1] + n[1]))


def position_mark(s, pivot, tip):
    c, u, n = on_blade(pivot, tip, 0.45)
    triangle(s, (c[0] + 1.1 * n[0], c[1] + 1.1 * n[1]), c, 0.7, filled=False)


def contactor_mark(s, kind):
    if kind == 'no':
        s.arc(0, P - 0.7, 1.4, 90, 270)
    else:
        s.arc(0, P - 0.7, 1.4, 270, 450)


def disconnector_mark(s, load=False):
    s.line((-0.8, P), (0.8, P))
    if load:
        s.circle(0, P + 0.55, 1.1)


def return_mark(s, kind):
    """The filled triangle of automatic return at the fixed contact."""
    side = -1 if kind == 'no' else 1
    s.poly((0, P - 1.2), (0, P), (side * 1.1, P - 0.6))


def changeover(prefix, caption, form):
    """Change-over contacts with 1 (common) below, 4 (make) and 2 (break) above."""
    s = Symbol(prefix, caption)
    s.pin('1', (0, 4 * P), label=(0.8, 4 * P - 2.4), shown=True)
    if form == 'centre':
        s.pin('4', (-P, 0), label=(-P - 0.4, 0.2), align='right', shown=True).pin('2', (P, 0), label=(P + 0.4, 0.2), shown=True)
        s.line((-P, 0), (-P, P + 0.6)).line((P, 0), (P, P + 0.6))
        s.line((0, 4 * P), (0, 3 * P + 0.5)).circle(0, 3 * P, 1.0).line((0, 3 * P - 0.5), (0, P + 0.6))
        return s.labels((P + 1.4, P + 1.2), (P + 1.4, 2 * P + 1.4))
    s.pin('4', (0, 0), label=(-0.4, 0.2), align='right', shown=True).pin('2', (P, 0), label=(P + 0.4, 0.2), shown=True)
    s.line((0, 4 * P), (0, 3 * P))
    if form == 'bar':        # make-before-break: blade with a cross-bar, resting on the break contact's arm
        tip = (1.5, 2.9)
        s.line((0, 0), (0, 2.4)).line((P, 0), (P, 4.1), (0.45, 4.1)).line((0, 3 * P), tip)
        c, u, n = on_blade((0, 3 * P), tip, 1.0)
        s.line((tip[0] - 0.75 * n[0], tip[1] - 0.75 * n[1]), (tip[0] + 0.75 * n[0], tip[1] + 0.75 * n[1]))
    else:                    # the same with the break contact as a bridge reaching across the make contact
        s.line((0, 0), (0, 3.0)).line((P, 0), (P, 2.0), (-0.8, 2.0)).line((0, 3 * P), (1.85, 1.7))
    return s.labels((P + 1.4, P + 1.2), (P + 1.4, 2 * P + 1.4))


def coil(prefix, caption):
    s = Symbol(prefix, caption)
    s.parent = True
    s.rect(-2 * P, P, 2 * P, 3 * P, w=0.35).line((0, 0), (0, P)).line((0, 3 * P), (0, 4 * P))
    s.pin('A1', (0, 0), label=(0.6, 0.2), shown=True).pin('A2', (0, 4 * P), label=(0.6, 4 * P - 2.4), shown=True)
    return s.labels((2 * P + 0.8, P), (2 * P + 0.8, 2 * P + 0.4))


def field(s, k=1, fill=None):
    """The k-th added field left of the coil; returns its left edge, right edge and centre."""
    x1 = -(k + 1) * P
    s.rect(x1 - P, P, x1, 3 * P, w=0.35, fill=fill)
    return x1 - P, x1, x1 - P / 2


def coils(prefix, add, contactor):
    """The operating coils; on the [Q] page `contactor` names the plain coils "Schützspule"."""
    word = ('Schützspule', 'Contactor coil', 'Bobine de contacteur')

    def name(de, en, fr, de_q, en_q, fr_q):
        return C(de_q, en_q, fr_q) if contactor else C(de, en, fr)

    s = coil(prefix, name('Relaisspule (betätigter Zustand)', 'Relay coil (operated state)', 'Bobine de relais (état actionné)',
                          word[0] + ' (betätigter Zustand)', word[1] + ' (operated state)', word[2] + ' (état actionné)'))
    x = -2 * P - 1.6
    s.line((x - 0.9, P + 0.6), (x, P - 0.6), (x + 0.9, P + 0.6))
    s.line((x - 0.35, P - 0.13), (x - 0.35, 2 * P + 1.0), (x + 0.35, 2 * P + 1.0), (x + 0.35, P - 0.13))
    add(s)
    s = coil(prefix, C('Spule eines Thermorelais', 'Coil of a thermal relay', "Bobine d'un relais thermique"))
    add(s.line((0, P), (0, P + 1.0), (-1.6, P + 1.0), (-1.6, 3 * P - 1.0), (0, 3 * P - 1.0), (0, 3 * P)))
    s = coil(prefix, C('Spule eines Stromstoßrelais', 'Coil of an impulse relay', "Bobine d'un télérupteur"))
    add(s.line((-2.4, 2 * P + 0.8), (0, 2 * P + 0.8), (0, 2 * P - 0.8), (2.4, 2 * P - 0.8)))
    s = coil(prefix, name('Relaisspule mit zwei Wicklungen', 'Relay coil with two windings', 'Bobine de relais à deux enroulements',
                          word[0] + ' mit zwei Wicklungen', word[1] + ' with two windings', word[2] + ' à deux enroulements'))
    add(s.line((-3.6, 3 * P), (-1.6, P)).line((1.6, 3 * P), (3.6, P)))

    s = coil(prefix, name('Spule, abfallverzögert', 'Coil, slow-releasing', 'Bobine, mise au repos retardée',
                          word[0] + ', abfallverzögert', word[1] + ', slow-releasing', word[2] + ', mise au repos retardée'))
    field(s, fill='#000000')
    add(s)
    s = coil(prefix, name('Spule, ansprechverzögert', 'Coil, slow-operating', 'Bobine, mise au travail retardée',
                          word[0] + ', ansprechverzögert', word[1] + ', slow-operating', word[2] + ', mise au travail retardée'))
    x0, x1, c = field(s)
    add(s.line((x0, P), (x1, 3 * P)).line((x0, 3 * P), (x1, P)))
    s = coil(prefix, name('Spule, ansprech- und abfallverzögert', 'Coil, slow-operating and slow-releasing',
                          'Bobine, mise au travail et au repos retardées', word[0] + ', ansprech- und abfallverzögert',
                          word[1] + ', slow-operating and slow-releasing', word[2] + ', mise au travail et au repos retardées'))
    field(s, fill='#000000')
    x0, x1, c = field(s, 2)
    add(s.line((x0, P), (x1, 3 * P)).line((x0, 3 * P), (x1, P)))
    s = coil(prefix, C('Spule eines Blinkrelais', 'Coil of a flasher relay', "Bobine d'un relais clignoteur"))
    x0, x1, c = field(s)
    add(s.line((c - 1.0, 2 * P + 0.6), (c - 0.4, 2 * P + 0.6), (c - 0.4, 2 * P - 0.6), (c + 0.4, 2 * P - 0.6),
               (c + 0.4, 2 * P + 0.6), (c + 1.0, 2 * P + 0.6)))
    s = coil(prefix, C('Spule eines Wechselstromrelais', 'Coil of an AC relay', "Bobine d'un relais à courant alternatif"))
    x0, x1, c = field(s)
    add(s.arc(c - 0.5, 2 * P, 1.0, 0, 180).arc(c + 0.5, 2 * P, 1.0, 180, 360))
    s = coil(prefix, C('Spule eines schnell schaltenden Relais', 'Coil of a high-speed relay', "Bobine d'un relais rapide"))
    x0, x1, c = field(s)
    add(s.line((c - 0.4, P + 0.6), (c - 0.4, 3 * P - 0.6)).line((c + 0.4, P + 0.6), (c + 0.4, 3 * P - 0.6)))
    s = coil(prefix, C('Spule eines Remanenzrelais', 'Coil of a remanent relay', "Bobine d'un relais rémanent"))
    x0, x1, c = field(s)
    add(s.line((x0, 3 * P), (x1, P)))
    s = coil(prefix, C('Spule eines Stützrelais', 'Coil of a mechanically latched relay', "Bobine d'un relais à verrouillage mécanique"))
    x0, x1, c = field(s)
    add(s.line((x1, P), (x0, 2 * P), (x1, 3 * P)))
    s = coil(prefix, C('Spule eines wechselstromunempfindlichen Relais', 'Coil of a relay unaffected by AC',
                       "Bobine d'un relais insensible au courant alternatif"))
    add(s.rect(-2 * P, P, 2 * P, P + 0.9, fill='#000000').rect(-2 * P, 3 * P - 0.9, 2 * P, 3 * P, fill='#000000'))
    s = coil(prefix, C('Spule eines gepolten Relais', 'Coil of a polarized relay', "Bobine d'un relais polarisé"))
    x0, x1, c = field(s, fill='#000000')
    add(s.rect(c - 0.5, 2 * P - 0.5, c + 0.5, 2 * P + 0.5, w=0.1, fill='#ffffff').circle(-0.9, P - 0.9, 0.8, fill='#000000'))


def contacts(prefix, add):
    s, a, b = pole(prefix, C('Schließer', 'Make contact', 'Contact à fermeture'), 'no')
    add(s)
    add(changeover(prefix, C('Wechsler ohne Unterbrechung', 'Change-over contact, make-before-break',
                             'Contact inverseur avec chevauchement'), 'bar'))
    add(changeover(prefix, C('Wechsler (Brücke)', 'Change-over contact, make-before-break (bridge form)',
                             'Contact inverseur avec chevauchement (forme à pont)'), 'bridge'))
    add(changeover(prefix, C('Wechsler mit Mittelstellung', 'Change-over contact with centre-off position',
                             "Contact inverseur avec position médiane d'ouverture"), 'centre'))
    s, a, b = pole(prefix, C('Schließer mit Leistungsschalterfunktion', 'Make contact, circuit-breaker function',
                             'Contact à fermeture, fonction disjoncteur'), 'no', raised=True)
    add(s.line((-0.8, P - 0.8), (0.8, P + 0.8)).line((-0.8, P + 0.8), (0.8, P - 0.8)))
    s, a, b = pole(prefix, C('Schließer mit Trennerfunktion', 'Make contact, disconnector function',
                             'Contact à fermeture, fonction sectionneur'), 'no')
    disconnector_mark(s)
    add(s)
    s, a, b = pole(prefix, C('Schließer mit Lasttrennerfunktion', 'Make contact, switch-disconnector function',
                             'Contact à fermeture, fonction interrupteur-sectionneur'), 'no')
    disconnector_mark(s, load=True)
    add(s)
    s, a, b = pole(prefix, C('Schließer mit Schützfunktion und selbsttätiger Auslösung',
                             'Make contact, contactor function, automatic tripping',
                             'Contact à fermeture, fonction contacteur, déclenchement automatique'), 'no')
    contactor_mark(s, 'no')
    tripping(s, a, b)
    add(s)
    s, a, b = pole(prefix, C('Schließer mit Lasttrennerfunktion und selbsttätiger Auslösung',
                             'Make contact, switch-disconnector function, automatic tripping',
                             'Contact à fermeture, fonction interrupteur-sectionneur, déclenchement automatique'), 'no')
    disconnector_mark(s, load=True)
    tripping(s, a, b)
    add(s)
    s, a, b = pole(prefix, C('Schließer mit selbsttätigem Rückgang', 'Make contact with automatic return',
                             'Contact à fermeture à retour automatique'), 'no')
    return_mark(s, 'no')
    add(s)
    s, a, b = pole(prefix, C('Positionsschalter (Schließer)', 'Position switch (make)', 'Interrupteur de position (fermeture)'), 'no')
    position_mark(s, a, b)
    add(s)
    s, a, b = pole(prefix, C('Öffner mit Schützfunktion', 'Break contact, contactor function',
                             'Contact à ouverture, fonction contacteur'), 'nc', mark=True)
    contactor_mark(s, 'nc')
    add(s)
    s, a, b = pole(prefix, C('Öffner mit selbsttätigem Rückgang', 'Break contact with automatic return',
                             'Contact à ouverture à retour automatique'), 'nc', mark=True)
    return_mark(s, 'nc')
    add(s)
    s, a, b = pole(prefix, C('Positionsschalter (Öffner)', 'Position switch (break)', 'Interrupteur de position (ouverture)'), 'nc')
    position_mark(s, a, b)
    add(s)


add = extend(EI, 'Schütze + Kontakte [K]')
contacts('K', add)
coils('K', add, False)

add = page(EI, 'Schütze + Kontakte [Q]', 'Contactors + contacts [Q]', 'Contacteurs + contacts [Q]')
s = coil('Q', C('Schützspule', 'Contactor coil', 'Bobine de contacteur'))
add(s)
s = Symbol('Q', C('Relaisspule', 'Relay coil', 'Bobine de relais'))
s.parent = True
s.rect(-1.6, P, 1.6, 3 * P, w=0.35).line((0, 0), (0, P)).line((0, 3 * P), (0, 4 * P)).line((-1.6, 3 * P), (1.6, P))
add(s.pin('A1', (0, 0), label=(0.6, 0.2), shown=True).pin('A2', (0, 4 * P), label=(0.6, 4 * P - 2.4), shown=True).labels((2.4, P), (2.4, 2 * P + 0.4)))
add(contact('no', C('Schließer (Schütz)', 'Make contact (contactor)', 'Contact à fermeture (contacteur)'), 'Q', ('13', '14'), operated='relay', numbers=True))
add(contact('nc', C('Öffner (Schütz)', 'Break contact (contactor)', 'Contact à ouverture (contacteur)'), 'Q', ('21', '22'), operated='relay', numbers=True))
add(contact('co', C('Wechsler (Schütz)', 'Change-over contact (contactor)', 'Contact inverseur (contacteur)'), 'Q', ('12', '11', '14'), operated='relay', numbers=True))
contacts('Q', add)
coils('Q', add, True)
