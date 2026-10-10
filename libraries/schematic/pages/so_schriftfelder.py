# More title blocks for page "Schriftfelder", in German, English and French: after DIN EN ISO 7200 (the compact
# arrangement of its figure 1, 180 mm wide, the scale added in the strip at the bottom right) and in the classic German
# arrangement after DIN 6771-1 (change rows, signature rows, title, drawing number, sheet). Centred on the insertion
# point, the frame heavier than the field lines, field names small at the top left of each field.
add = extend(SO, 'Schriftfelder')
FRAME = 0.35
NAME = 1.8      # height of the field names


def fields(s, x0, cells, w, h):
    """The frame and the fields of a title block. A field is (x0, y0, x1, y1, name, value); its right and bottom edges
    are drawn unless they lie on the frame, so every line is drawn once."""
    s.rect(x0, 0, x0 + w, h, w=FRAME)
    for a, b, c, d, name, value in cells:
        if c < w - 1e-6:
            s.line((x0 + c, b), (x0 + c, d))
        if d < h - 1e-6:
            s.line((x0 + a, d), (x0 + c, d))
        if name:
            s.text(name, x0 + a + 0.8, b + 0.7, NAME)
        if value:
            s.text(value, x0 + a + 1.0, b + 4.2, 2.5)
    return s


def iso7200(caption, names, right=50.0):
    """ISO 7200, figure 1: legal owner at the left over two rows, below the responsible department; creator and
    approval person beside it; the title in the middle; identification number, scale, date of issue and sheet at
    the right. right: width of the right-hand column (50 mm for 180 mm in all)."""
    company, department, title, created, approved, number, kind, date, scale, sheet = names
    s = Symbol('', caption, numbered=False, listed=False, shown=False)
    a, b, c, h = 32.0, 74.0, 130.0, 9.0
    w = c + right
    d, e = c + 0.26 * right, w - 0.36 * right
    cells = [(0, 0, b, h, department, ''), (b, 0, w, h, kind, ''),
             (0, h, a, 3 * h, company, ''), (a, h, b, 2 * h, created, ''), (a, 2 * h, b, 3 * h, approved, ''),
             (b, h, c, 3 * h, title, ''), (c, h, w, 2 * h, number, ''),
             (c, 2 * h, d, 3 * h, scale, ''), (d, 2 * h, e, 3 * h, date, ''), (e, 2 * h, w, 3 * h, sheet, '<PAGENO>')]
    return fields(s, -w / 2, cells, w, 3 * h)


def revision(caption, names):
    """The classic German arrangement: change rows at the left (their heading in the bottom row), the drawn and checked
    rows with date and name beside them, the title and drawing number, sheet number and number of sheets at the right."""
    state, change, date, name, drawn, checked, title, number, count, sheet = names
    s = Symbol('', caption, numbered=False, listed=False, shown=False)
    w, h = 180.0, 6.0
    columns = (0, 10, 42, 58, 74, 86, 102, 120)
    cells = [(a, y, b, y + h, '', '') for a, b in zip(columns, columns[1:]) for y in (0, h, 2 * h)]
    cells += [(120, 0, 158, 2 * h, title, ''), (120, 2 * h, 158, 3 * h, number, ''),
              (158, 0, w, 1.5 * h, sheet, ''), (158, 1.5 * h, w, 3 * h, count, '')]
    fields(s, -w / 2, cells, w, 3 * h)
    x0, mid = -w / 2, 2.5 * h - NAME / 2
    for x, t in ((5, state), (26, change), (50, date), (66, name)):
        s.text(t, x0 + x, mid, NAME, 'centre')
    for x, t in ((94, date), (111, name)):
        s.text(t, x0 + x, h / 2 - NAME / 2, NAME, 'centre')
    for y, t in ((1.5 * h, drawn), (2.5 * h, checked)):
        s.text(t, x0 + 74.8, y - NAME / 2, NAME)
    return s


ISO = {'de': ('Firma', 'Abteilung', 'Titel', 'Erstellt durch', 'Genehmigt von', 'Sachnummer', 'Dokumentenart',
              'Ausgabedatum', 'Maßstab', 'Blatt'),
       'en': ('Company', 'Department', 'Title', 'Created by', 'Approved by', 'Number', 'Type', 'Date', 'Scale', 'Page'),
       'fr': ('Société', 'Département', 'Titre', 'Créé par', 'Approuvé par', 'Numéro', 'Type', 'Date', 'Échelle', 'Page')}
CHANGES = {'de': ('Zustand', 'Änderung', 'Datum', 'Name', 'gez.', 'gepr.', 'Bezeichnung', 'Zeichnungs-Nr.', 'Blattzahl', 'Blatt-Nr.'),
           'en': ('Rev.', 'Changes', 'Date', 'Name', 'drawn', 'checked', 'Title', 'Drawing No.', 'Pages total', 'Page'),
           'fr': ('Ind.', 'Modifications', 'Date', 'Nom', 'dessiné', 'vérifié', 'Titre', 'N° de plan', 'Nombre de pages', 'Page')}
LANGUAGES = (('de', 'Deutsch', 'German', 'allemand'), ('en', 'Englisch', 'English', 'anglais'), ('fr', 'Französisch', 'French', 'français'))

for key, de, en, fr in LANGUAGES:
    add(iso7200(C(f'Schriftfeld DIN EN ISO 7200 ({de})', f'Title block ISO 7200 ({en})', f'Cartouche ISO 7200 ({fr})'), ISO[key]))
for key, de, en, fr in LANGUAGES:
    add(revision(C(f'Schriftfeld mit Änderungsfeld ({de})', f'Title block with revisions ({en})',
                   f'Cartouche avec modifications ({fr})'), CHANGES[key]))
for key, de, en, fr in LANGUAGES:
    add(iso7200(C(f'Schriftfeld DIN EN ISO 7200, breit ({de})', f'Title block ISO 7200, wide ({en})',
                  f'Cartouche ISO 7200 large ({fr})'), ISO[key], right=68.0))
