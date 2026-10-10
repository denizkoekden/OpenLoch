# More operating devices for page "Antriebsarten", after DIN EN 60617-2 (operating devices and effects) and
# DIN EN 60617-7 (relay drives): the sign at the left end of the dashed mechanical link, like the signs already there.
add = extend(EI, 'Antriebsarten')


def delayed_coil(s, black):
    """The coil of a relay with an added field at its far side: crossed (slow-operating) or black (slow-releasing)."""
    s.rect(-2.4, -1.4, 0, 1.4)
    if black:
        s.rect(-3.6, -1.4, -2.4, 1.4, fill='#000000')
    else:
        s.rect(-3.6, -1.4, -2.4, 1.4).line((-3.6, -1.4), (-2.4, 1.4)).line((-3.6, 1.4), (-2.4, -1.4))


def diamond(s):
    s.poly((-3.0, 0), (-1.5, -1.5), (0, 0), (-1.5, 1.5), fill=None)


def latch(s):
    """A latching device: a square divided into four."""
    s.rect(-2.8, -1.4, 0, 1.4).line((-1.4, -1.4), (-1.4, 1.4)).line((-2.8, 0), (0, 0))


def clock(s):
    s.circle(-1.6, 0, 3.2).line((-1.6, 0), (-1.6, -1.1)).line((-1.6, 0), (-0.75, 0.5))


def float_(s):
    s.circle(-1.2, 0, 2.4).line((-3.0, 1.2), (0.6, 1.2), w=0.35)


def cylinder(s):
    s.rect(-5.2, -1.4, -1.8, 1.4).line((-3.4, -1.4), (-3.4, 1.4), w=THICK).line((-3.4, 0), (0, 0))
    s.arrow((-4.8, -2.4), (-2.2, -2.4), size=1.0)


def overcurrent(s):
    s.line((-1.0, -1.6), (-1.0, -1.0)).arc(-1.0, 0, 2.0, 270, 450).line((-1.0, 1.0), (-1.0, 1.6))


add(operating(C('Elektromagnetischer Antrieb, ansprechverzögert', 'Electromagnetic drive, slow-operating',
                'Commande électromagnétique, temporisée à l’attraction'), lambda s: delayed_coil(s, False)))
add(operating(C('Elektromagnetischer Antrieb, rückfallverzögert', 'Electromagnetic drive, slow-releasing',
                'Commande électromagnétique, temporisée à la retombée'), lambda s: delayed_coil(s, True)))
add(operating(C('Uhrwerkantrieb', 'Clockwork drive', 'Commande par mouvement d’horlogerie'), clock))
add(operating(C('Betätigung durch Flüssigkeitspegel', 'Operated by fluid level', 'Commande par niveau de liquide'), float_))
add(operating(C('Betätigung durch Annäherung', 'Operated by proximity', 'Commande par proximité'),
              lambda s: (diamond(s), s.line((-1.5, -0.8), (-1.5, 0.8), w=THICK))))
add(operating(C('Betätigung durch Berührung', 'Operated by touch', 'Commande par effleurement'),
              lambda s: (diamond(s), s.line((-1.5, -0.8), (-1.5, 0.5)).line((-2.1, 0.5), (-0.9, 0.5)))))
add(operating(C('Kraftantrieb', 'Power drive', 'Commande par énergie auxiliaire'), lambda s: s.rect(-2.8, -1.4, 0, 1.4)))
add(operating(C('Kraftantrieb mit Handbetätigung', 'Power drive with manual operation', 'Commande par énergie auxiliaire et manuelle'),
              lambda s: s.rect(-2.8, -1.4, 0, 1.4).line((-1.4, -1.4), (-1.4, -2.8)).line((-2.4, -2.8), (-0.4, -2.8))))
add(operating(C('Schaltschloss', 'Latching device', 'Dispositif d’accrochage'), latch))
add(operating(C('Schaltschloss mit Freiauslösung', 'Latching device, trip-free', 'Dispositif d’accrochage à déclenchement libre'),
              lambda s: (latch(s), dashed(s, (-4.4, 0), (0, 0)))))
add(operating(C('Hydraulischer Antrieb', 'Hydraulic drive', 'Commande hydraulique'), cylinder))
add(operating(C('Überstromauslöser', 'Overcurrent release', 'Déclencheur à maximum de courant'), overcurrent))
