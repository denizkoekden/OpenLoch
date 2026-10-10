# The page "Parent/Child Beispiele": examples of linked parts. A parent carries the designator (an IC part also its
# supply), its children are kept with its entry as in sPlan's library and placed with it, linked, so that they take
# the parent's designator: relay contacts show it as their own (<PARENT_ID>), gates as <PARENT_ID>-<CHILDNO> with the
# parent's value. Drawn like the 74xx parent/child
# page (IEC 60617-12) and the contactor page (DIN EN 60617-7); the 7400 pins as in ttl74.py.
add = page(EL, 'Parent/Child Beispiele', 'Parent/child examples', 'Exemples parent/enfant')

ttl7400 = next(chip for chip in ttl74.CHIPS if chip.number == '7400')
chip = add(supply_parent('7400', C('7400 (Parent)', '7400 (parent)', '7400 (parent)'), ttl7400.pins))

s = Symbol('K', C('Relaisspule (Parent)', 'Relay coil (parent)', 'Bobine de relais (parent)'))
s.parent = True
s.rect(-1.6, P, 1.6, 3 * P, w=0.35).line((0, 0), (0, P)).line((0, 3 * P), (0, 4 * P)).line((-1.6, 3 * P), (1.6, P))
coil = add(s.pin('A1', (0, 0), label=(0.6, 0.2), shown=True).pin('A2', (0, 4 * P), label=(0.6, 4 * P - 2.4), shown=True).labels((2.4, P), (2.4, 2 * P + 0.4)))

for kind, pins, de, en, fr in (('no', ('1', '2'), 'Schließer 1-2', 'Make contact 1-2', 'Contact à fermeture 1-2'),
                               ('no', ('3', '4'), 'Schließer 3-4', 'Make contact 3-4', 'Contact à fermeture 3-4'),
                               ('nc', ('5', '6'), 'Öffner 5-6', 'Break contact 5-6', 'Contact à ouverture 5-6')):
    s = contact(kind, C(f'{de} (Child)', f'{en} (child)', f'{fr} (enfant)'), '<PARENT_ID>', pins, operated='relay', numbers=True)
    s.numbered = False
    coil.attach(s)

for u, numbers in enumerate(([1, 2, 3], [4, 5, 6], [9, 10, 8], [12, 13, 11]), 1):
    s = child_gate('&', True, 2, numbers, C(f'7400 NAND {u} (Child)', f'7400 NAND {u} (child)', f'7400 NON-ET {u} (enfant)'))
    s.value = '<PARENT_VALUE>'
    chip.attach(s)
