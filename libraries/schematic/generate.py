#!/usr/bin/env python3
"""Draws the schematic symbols that come with OpenLoch and writes them as library pages (one JSON file per page,
format "OpenLoch Schematic Library", version 2, or 3 for a page whose parents carry their children) into this folder. The symbols follow DIN EN 60617; their connection
points lie on a 2.54 mm pitch around the insertion point. Names and captions hold German, English and French,
separated by CR. Public domain (CC0 1.0), like the pages it writes.

Run it after changing a symbol: python3 libraries/schematic/generate.py
"""
import json
import math
import os
import re
import sys

W = 0.25        # line width of symbols
THICK = 0.5     # plates, bars and the like
P = 2.54        # pitch of connection points
HERE = os.path.dirname(os.path.abspath(__file__))


def r(v):
    return round(v, 4)


def pt(p):
    return [r(p[0]), r(p[1])]


def names(de, en, fr):
    return '\r'.join([de, en, fr])


def rot(p, centre, degrees):
    """p turned counter-clockwise on the screen (y down) about centre."""
    a = math.radians(degrees)
    x, y = p[0] - centre[0], p[1] - centre[1]
    return (centre[0] + x * math.cos(a) + y * math.sin(a), centre[1] - x * math.sin(a) + y * math.cos(a))


class Symbol:
    def __init__(self, prefix, caption, value='', ask=False, numbered=True, listed=True, shown=True, value_shown=None):
        self.prefix, self.caption, self.value = prefix, caption, value
        self.ask, self.numbered, self.listed = ask, numbered, listed
        self.shown, self.value_shown = shown, shown if value_shown is None else value_shown
        self.parts = []
        self.texts = None
        self.children = []

    def attach(self, child):
        """A child kept with this parent's library entry, as in sPlan's library: placed with the parent, beside it."""
        self.children.append(child)
        return child

    # Drawing
    def line(self, *points, w=W, start='none', end='none', size=None):
        o = {'type': 'line', 'points': [pt(p) for p in points], 'pen': {'width': w}, 'electrical': False}
        if start != 'none':
            o['startEnd'] = start
        if end != 'none':
            o['endEnd'] = end
        if size:
            o['endSize'] = size
        self.parts.append(o)
        return self

    def arrow(self, a, b, size=1.2, w=W):
        return self.line(a, b, w=w, end='arrow', size=size)

    def rect(self, x0, y0, x1, y1, w=W, fill=None, corner=0):
        o = {'type': 'rectangle', 'centre': pt(((x0 + x1) / 2, (y0 + y1) / 2)), 'size': [r(abs(x1 - x0)), r(abs(y1 - y0))],
             'pen': {'width': w}}
        if fill:
            o['fill'] = {'style': 'solid', 'color': fill}
        if corner:
            o['corners'] = 'round'
            o['corner'] = corner
        self.parts.append(o)
        return self

    def circle(self, cx, cy, d, w=W, fill=None):
        o = {'type': 'ellipse', 'centre': pt((cx, cy)), 'size': [r(d), r(d)], 'pen': {'width': w}}
        if fill:
            o['fill'] = {'style': 'solid', 'color': fill}
        self.parts.append(o)
        return self

    def arc(self, cx, cy, d, start, stop, w=W, dy=None):
        o = {'type': 'ellipse', 'centre': pt((cx, cy)), 'size': [r(d), r(dy if dy else d)], 'pen': {'width': w},
             'start': r(start), 'stop': r(stop), 'arc': 'arc'}
        self.parts.append(o)
        return self

    def poly(self, *points, fill='#000000', w=W):
        o = {'type': 'polygon', 'points': [pt(p) for p in points], 'pen': {'width': w}}
        if fill:
            o['fill'] = {'style': 'solid', 'color': fill}
        self.parts.append(o)
        return self

    def bezier(self, *points, w=W):
        self.parts.append({'type': 'bezier', 'points': [pt(p) for p in points], 'pen': {'width': w}})
        return self

    def text(self, t, x, y, h=2.5, align='left', bold=False, rotation=0):
        o = {'type': 'text', 'pos': pt((x, y)), 'text': t, 'font': {'height': h}, 'align': align}
        if bold:
            o['font']['bold'] = True
        if rotation:
            o['rotation'] = rotation
        self.parts.append(o)
        return self

    def pin(self, name, at, label=None, align='left', shown=False, text=None):
        o = {'type': 'contact', 'pos': pt(label if label else (at[0] + 0.4, at[1] - 2.2)), 'name': name,
             'text': name if text is None else text, 'font': {'height': 1.8}, 'align': align, 'visible': shown,
             'pin': pt(at)}
        self.parts.append(o)
        return self

    def labels(self, designator, value=None, align='left', value_align=None):
        self.texts = (designator, value, align, value_align or align)
        return self

    def json(self, entry):
        children = list(self.parts)
        if self.texts:
            d, v, a, va = self.texts
            children.append({'type': 'text', 'role': 'designator', 'pos': pt(d), 'text': '', 'font': {'height': 2.5}, 'align': a})
            if v:
                children.append({'type': 'text', 'role': 'value', 'pos': pt(v), 'text': '', 'font': {'height': 2.5}, 'align': va})
        o = {'type': 'component', 'pos': [0, 0], 'designator': self.prefix + '?' if self.numbered else self.prefix,
             'value': self.value, 'caption': self.caption, 'libraryEntry': entry, 'children': children}
        if not self.numbered:
            o['autoNumber'] = False
        if self.ask:
            o['askValue'] = True
        if not self.listed:
            o['inPartsList'] = False
        if not self.shown:
            o['designatorVisible'] = False
        if not self.value_shown:
            o['valueVisible'] = False
        if getattr(self, 'parent', False):
            o['parent'] = True
        return o

    def vertical(self):
        """The symbol turned a quarter clockwise: what ran to the right runs down. Texts stay upright."""
        turn = lambda p: (-p[1], p[0])
        for o in self.parts:
            for key in ('points',):
                if key in o:
                    o[key] = [pt(turn(q)) for q in o[key]]
            for key in ('centre', 'pos', 'pin'):
                if key in o:
                    o[key] = pt(turn(o[key]))
            if o['type'] in ('rectangle', 'ellipse'):
                o['size'] = [o['size'][1], o['size'][0]]
            if o['type'] == 'ellipse' and 'start' in o:
                o['start'], o['stop'] = r((o['start'] - 90) % 360), r((o['stop'] - 90) % 360)
                if o['stop'] <= o['start']:
                    o['stop'] = r(o['stop'] + 360)
            if o['type'] in ('text', 'contact'):
                o['align'] = 'left'
                o['pos'] = pt((o['pos'][0] + 0.6, o['pos'][1] - 1.0))
        return self


# --- Building blocks
def two_pin(prefix, caption, value='', ask=False, length=4 * P):
    """A part between two connection points on the x axis, 1 at the insertion point."""
    s = Symbol(prefix, caption, value, ask)
    s.pin('1', (0, 0)).pin('2', (length, 0), label=(length - 0.4, -2.2), align='right')
    s.labels((length / 2, -5.6), (length / 2, 2.8), 'centre')
    return s


def resistor_body(s, x0=P, x1=3 * P, h=2.0):
    s.line((0, 0), (x0, 0)).rect(x0, -h / 2, x1, h / 2).line((x1, 0), (4 * P, 0))
    return s


def diode_shape(s, ax, cx, half=1.5, fill=None, y=0):
    """Triangle from ax to the bar at cx on a line parallel to the x axis."""
    s.poly((ax, y - half), (ax, y + half), (cx, y), fill=fill)
    s.line((cx, y - half), (cx, y + half), w=0.35)
    return s


def coil(s, x0, x1, turns=4, y=0, up=True):
    d = (x1 - x0) / turns
    for k in range(turns):
        s.arc(x0 + d * (k + 0.5), y, d, 0 if up else 180, 180 if up else 360)
    return s


def arrows_in(s, x, y, angle=225, gap=1.0):
    """Two arrows pointing at (x, y) from up right (light falling on a part)."""
    for k in (0, 1):
        tip = (x + k * gap * 1.4, y)
        a = math.radians(angle)
        tail = (tip[0] - 2.4 * math.cos(a), tip[1] + 2.4 * math.sin(a))
        s.arrow(tail, tip, size=0.9)
    return s


def arrows_out(s, x, y, angle=45, gap=1.0):
    """Two arrows leaving (x, y) up right (light sent out)."""
    for k in (0, 1):
        tail = (x + k * gap * 1.4, y)
        a = math.radians(angle)
        tip = (tail[0] + 2.4 * math.cos(a), tail[1] - 2.4 * math.sin(a))
        s.arrow(tail, tip, size=0.9)
    return s


def transistor(pnp=False, caption=None, photo=False):
    s = Symbol('K', caption)
    cx = 4.6
    s.circle(cx, 0, 7.4)
    if not photo:
        s.line((0, 0), (3.0, 0)).pin('B', (0, 0), shown=False)
    s.line((3.0, -2.0), (3.0, 2.0), w=THICK)
    s.line((3.0, -0.9), (2 * P, -3.0)).line((2 * P, -3.0), (2 * P, -2 * P))
    if pnp:
        s.line((2 * P, 3.0), (2 * P, 2 * P)).line((2 * P, 3.0), (3.0, 0.9), end='arrow', size=1.3)
    else:
        s.line((3.0, 0.9), (2 * P, 3.0), end='arrow', size=1.3).line((2 * P, 3.0), (2 * P, 2 * P))
    s.pin('C', (2 * P, -2 * P), shown=False).pin('E', (2 * P, 2 * P), shown=False)
    if photo:
        arrows_in(s, 2.0, -2.2)
    s.labels((9.2, -2.6), (9.2, 0.4))
    return s


def jfet(p=False, caption=None):
    s = Symbol('K', caption)
    s.circle(4.4, 0, 7.4)
    if p:
        s.line((0, P), (3.4, P), start='arrow', size=1.3)
    else:
        s.line((0, P), (3.4, P), end='arrow', size=1.3)
    s.line((3.4, -3.0), (3.4, 3.0), w=THICK)
    s.line((3.4, -1.6), (2 * P, -1.6)).line((2 * P, -1.6), (2 * P, -2 * P))
    s.line((3.4, 1.6), (2 * P, 1.6)).line((2 * P, 1.6), (2 * P, 2 * P))
    s.pin('G', (0, P)).pin('D', (2 * P, -2 * P)).pin('S', (2 * P, 2 * P))
    s.labels((9.2, -2.6), (9.2, 0.4))
    return s


def mosfet(p=False, depletion=False, caption=None):
    s = Symbol('K', caption)
    s.circle(4.6, 0, 7.4)
    s.line((0, P), (2.6, P)).line((2.6, -1.9), (2.6, P))
    if depletion:
        s.line((3.4, -2.2), (3.4, 2.2), w=THICK)
    else:
        for y0, y1 in ((-2.2, -1.2), (-0.5, 0.5), (1.2, 2.2)):
            s.line((3.4, y0), (3.4, y1), w=THICK)
    s.line((3.4, -1.7), (2 * P, -1.7)).line((2 * P, -1.7), (2 * P, -2 * P))
    s.line((3.4, 1.7), (2 * P, 1.7)).line((2 * P, 1.7), (2 * P, 2 * P))
    if p:
        s.line((3.4, 0), (2 * P, 0), end='arrow', size=1.3)
    else:
        s.line((2 * P, 0), (3.4, 0), end='arrow', size=1.3)
    s.line((2 * P, 0), (2 * P, 1.7))
    s.pin('G', (0, P)).pin('D', (2 * P, -2 * P)).pin('S', (2 * P, 2 * P))
    s.labels((9.2, -2.6), (9.2, 0.4))
    return s


def box(prefix, caption, inputs, outputs, width=4 * P, label='', top=None, value='', clock=()):
    """A rectangular part: pins on the left and right side, 2.54 mm apart, with their texts inside."""
    rows = max(len(inputs), len(outputs), 1)
    h = (rows + 1) * P
    s = Symbol(prefix, caption, value)
    x0, x1 = P, P + width
    s.rect(x0, 0, x1, h)
    for k, name in enumerate(inputs):
        y = (k + 1) * P
        if not name:
            continue
        neg = name.startswith('~')
        n = name.lstrip('~')
        if neg:
            s.circle(x0 - 0.6, y, 1.2).line((0, y), (x0 - 1.2, y))
        else:
            s.line((0, y), (x0, y))
        if n in clock:
            s.line((x0, y - 0.9), (x0 + 1.2, y), (x0, y + 0.9))
            s.text(n, x0 + 1.5, y - 1.0, 2.0)
        else:
            s.text(n, x0 + 0.6, y - 1.0, 2.0)
        s.pin(('/' if neg else '') + n, (0, y))
    for k, name in enumerate(outputs):
        y = (k + 1) * P
        if not name:
            continue
        neg = name.startswith('~')
        n = name.lstrip('~')
        if neg:
            s.circle(x1 + 0.6, y, 1.2).line((x1 + 1.2, y), (x1 + P, y))
        else:
            s.line((x1, y), (x1 + P, y))
        s.text(n, x1 - 0.6, y - 1.0, 2.0, 'right')
        s.pin(('/' if neg else '') + n, (x1 + P, y), label=(x1 + P - 0.4, y - 2.2), align='right')
    if label:
        s.text(label, (x0 + x1) / 2, 0.4 if top is None else top, 2.5, 'centre', bold=True)
    s.labels((x0, -3.4), (x0, h + 0.6))
    return s


def gate_iec(sign, inputs=2, negated=False, caption=None):
    """A gate after IEC 60617-12: inputs left, the output in the middle of the right side, all on the grid."""
    if inputs == 1:
        ys, h = [P], 2 * P
    elif inputs % 2:
        ys, h = [(k + 1) * P for k in range(inputs)], (inputs + 1) * P
    else:
        half = inputs // 2
        ys = [(k + 1) * P for k in range(half)] + [(k + 2 + half) * P for k in range(half)]
        h = (inputs + 2) * P
    s = Symbol('IC', caption)
    x0, x1 = P, 3 * P
    s.rect(x0, 0, x1, h, w=0.35)
    s.text(sign, (x0 + x1) / 2, 0.8, 2.5, 'centre')
    for k, y in enumerate(ys):
        s.line((0, y), (x0, y)).pin(str(k + 1), (0, y))
    yo = h / 2
    if negated:
        s.circle(x1 + 0.6, yo, 1.2).line((x1 + 1.2, yo), (x1 + P, yo))
    else:
        s.line((x1, yo), (x1 + P, yo))
    s.pin(str(inputs + 1), (x1 + P, yo), label=(x1 + P - 0.4, yo - 2.2), align='right')
    s.labels((x0, -3.4), (x0, h + 0.6))
    return s


def gate_us(kind, negated=False, caption=None):
    """Gates in the shapes of ANSI/IEEE 91, two inputs."""
    s = Symbol('IC', caption)
    x0 = P
    y0, y1 = -1.0, 2 * P + 1.0
    if kind == 'and':
        s.line((x0, y0), (x0 + 3.0, y0)).line((x0, y0), (x0, y1)).line((x0, y1), (x0 + 3.0, y1))
        s.arc(x0 + 3.0, P, y1 - y0, 270, 450)
        right = x0 + 3.0 + (y1 - y0) / 2
    elif kind == 'buf':
        s.poly((x0, y0), (x0, y1), (x0 + 5.6, P), fill=None)
        right = x0 + 5.6
    else:
        if kind == 'xor':
            s.bezier((x0 - 0.8, y0), (x0 + 0.6, y0 + 1.6), (x0 + 0.6, y1 - 1.6), (x0 - 0.8, y1))
        s.bezier((x0, y0), (x0 + 1.4, y0 + 1.6), (x0 + 1.4, y1 - 1.6), (x0, y1))
        s.bezier((x0, y0), (x0 + 3.2, y0), (x0 + 5.4, y0 + 1.2), (x0 + 6.6, P))
        s.bezier((x0, y1), (x0 + 3.2, y1), (x0 + 5.4, y1 - 1.2), (x0 + 6.6, P))
        right = x0 + 6.6
    if kind == 'buf':
        s.line((0, P), (x0, P)).pin('1', (0, P))
    else:
        inner = 1.0 if kind in ('or', 'xor') else 0
        s.line((0, 0), (x0 + inner, 0)).line((0, 2 * P), (x0 + inner, 2 * P))
        s.pin('1', (0, 0)).pin('2', (0, 2 * P))
    out = 4 * P if kind == 'buf' else 5 * P     # the connection point clear of the negation circle
    if negated:
        s.circle(right + 0.6, P, 1.2).line((right + 1.2, P), (out, P))
    else:
        s.line((right, P), (out, P))
    s.pin('3' if kind != 'buf' else '2', (out, P), label=(out - 0.4, P - 2.2), align='right')
    s.labels((x0, -4.6), (x0, y1 + 0.6))
    return s


def meter(letter, caption):
    s = two_pin('P', caption)
    s.line((0, 0), (P + 0.3, 0)).circle(2 * P, 0, 2 * P - 0.6).line((3 * P - 0.3, 0), (4 * P, 0))
    s.text(letter, 2 * P, -1.5, 3.0, 'centre')
    s.labels((2 * P, -6.0), (2 * P, 3.4), 'centre')
    return s


def ground(kind, caption):
    s = Symbol('', caption, numbered=False, listed=False, shown=False)
    s.pin('1', (0, 0)).line((0, 0), (0, P))
    if kind == 'earth':
        for k, half in enumerate((2.0, 1.3, 0.6)):
            s.line((-half, P + k * 0.8), (half, P + k * 0.8), w=0.35)
    elif kind == 'chassis':
        s.line((-2.0, P), (2.0, P), w=0.35)
        for x in (-2.0, 0, 2.0):
            s.line((x, P), (x - 1.0, P + 1.4))
    elif kind == 'protective':
        for k, half in enumerate((1.6, 1.0, 0.4)):
            s.line((-half, P + 0.4 + k * 0.7), (half, P + 0.4 + k * 0.7), w=0.35)
        s.circle(0, P + 1.1, 4.4)
    elif kind == 'signal':
        s.poly((-1.8, P), (1.8, P), (0, P + 1.8), fill=None)
    elif kind == 'noiseless':
        for k, half in enumerate((2.0, 1.3, 0.6)):
            s.line((-half, P + k * 0.8), (half, P + k * 0.8), w=0.35)
        s.arc(0, P + 0.8, 5.0, 180, 360)
    s.labels((2.2, 0), None)
    return s


def supply(text, caption, down=False):
    """A supply or ground mark: its value is the net it stands for."""
    s = Symbol('', caption, text, numbered=False, listed=False, shown=False, value_shown=True)
    s.pin('1', (0, 0))
    if down:
        s.line((0, 0), (0, P)).poly((-1.6, P), (1.6, P), (0, P + 1.6), fill=None)
        s.labels((0, P + 2.0), (0, P + 2.0), 'centre')
    else:
        s.line((0, 0), (0, -P)).line((-1.8, -P), (1.8, -P), w=0.35)
        s.labels((0, -P - 3.2), (0, -P - 3.2), 'centre')
    return s


def contact(kind, caption, prefix='S', pins=('1', '2'), operated=None, numbers=False):
    """A switch contact drawn vertically: 1 above, 2 below (a change-over contact has 3 at the right)."""
    s = Symbol(prefix, caption)
    top, bottom = (0, 0), (0, 4 * P)
    s.pin(pins[0], top, label=(0.8, 0.2), shown=numbers).pin(pins[1], bottom, label=(0.8, 4 * P - 2.4), shown=numbers)
    s.line(top, (0, P)).line((0, 3 * P), bottom)
    if kind == 'no':      # make contact
        s.line((0, 3 * P), (-1.9, P + 0.6))
        lever = (-0.95, 2 * P - 0.4)
    elif kind == 'nc':    # break contact
        s.line((0, P), (1.6, P)).line((0, 3 * P), (2.1, P - 0.4))
        lever = (1.05, 2 * P - 0.6)
    else:                 # change-over: at rest on the break contact (above), the make contact at the left
        s.line((0, P), (1.6, P)).line((0, 3 * P), (2.1, P - 0.4))
        s.line((-P, 0), (-P, P + 0.6)).pin(pins[2], (-P, 0), label=(-P - 0.4, 0.2), align='right', shown=numbers)
        lever = (1.05, 2 * P - 0.6)
    if operated == 'push':
        s.line(lever, (-4.4, lever[1]), w=W).line((-4.4, lever[1] - 1.2), (-4.4, lever[1] + 1.2))
        s.line((-4.4, lever[1] - 1.2), (-3.6, lever[1] - 1.2)).line((-4.4, lever[1] + 1.2), (-3.6, lever[1] + 1.2))
    elif operated == 'manual':
        s.line(lever, (-4.4, lever[1]), w=W).line((-4.4, lever[1] - 1.2), (-4.4, lever[1] + 1.2))
    elif operated == 'relay':
        s.parts.append({'type': 'line', 'points': [pt(lever), pt((-5.0, lever[1]))], 'pen': {'width': W, 'style': 'dash'},
                        'electrical': False})
    s.labels((2.6, P + 0.4), (2.6, 2 * P + 0.6))
    return s


def extent(symbol):
    """The smallest and largest x and y of a symbol about its insertion point: what it draws, its texts at a width
    estimated from their length, the designator and value at room for about six characters."""
    xs, ys = [0.0], [0.0]

    def text(at, width, align, height):
        x = at[0] - (width if align == 'right' else width / 2 if align == 'centre' else 0)
        xs.extend([x, x + width])
        ys.extend([at[1], at[1] + height])
    for o in symbol.parts:
        qs = list(o.get('points', [])) + [o[k] for k in ('pin',) if k in o]
        if 'centre' in o:
            c, size = o['centre'], o.get('size', [0, 0])
            qs += [(c[0] - size[0] / 2, c[1] - size[1] / 2), (c[0] + size[0] / 2, c[1] + size[1] / 2)]
        if o['type'] in ('text', 'contact') and (o['type'] == 'text' or o.get('visible')):
            h = o.get('font', {}).get('height', 2.5)
            text(o['pos'], 0.6 * h * len(o.get('text', '')), o.get('align', 'left'), h)
        for q in qs:
            xs.append(q[0])
            ys.append(q[1])
    if symbol.texts:
        d, v, a, va = symbol.texts
        text(d, 10, a, 2.5)
        if v:
            text(v, 10, va, 2.5)
    return min(xs), min(ys), max(xs), max(ys)


def child_places(parent):
    """Where the children of a parent lie relative to its insertion point: in rows to the right of it, from left to
    right with a gap of two pitches, a new row below once a row would reach beyond 120 mm; every place on the pitch,
    so the children's contacts stay on it."""
    on = lambda v: math.ceil(round(v / P, 6)) * P
    right = extent(parent)[2]
    extents = [extent(c) for c in parent.children]
    above = -min([e[1] for e in extents] + [0])
    x0 = on(right + 2 * P)
    x, y, low, places = x0, 0.0, 0.0, []
    for left, _, cr, down in extents:
        if x > x0 and x + (cr - left) > x0 + 120:
            x, y = x0, on(low + 2 * P + above)
        at = (on(x - left), y)
        places.append(at)
        x, low = at[0] + cr + 2 * P, max(low, y + down)
    return places


# --- Pages
PAGES = []


def page(folder, de, en, fr):
    entry = {'folder': folder, 'name': names(de, en, fr), 'symbols': []}
    PAGES.append(entry)
    return adder(entry)


def adder(entry):
    def add(symbol):
        entry['symbols'].append(symbol)
        return symbol
    return add


def extend(folder, de):
    """The add function of a page made before, for the symbols a page module (pages/*.py) puts on it."""
    for entry in PAGES:
        if entry['folder'] == folder and entry['name'].split('\r')[0] == de:
            return adder(entry)
    raise KeyError(folder + ' / ' + de)


def parents(folder, de, *captions):
    """Makes entries of a page made before Parents, as sPlan's library has them: devices to which contacts drawn
    elsewhere are linked as children. Without captions, every component on the page with a designator of its own."""
    for entry in PAGES:
        if entry['folder'] == folder and entry['name'].split('\r')[0] == de:
            found = {s.caption.split('\r')[0] for s in entry['symbols']}
            if set(captions) - found:
                raise KeyError(folder + ' / ' + de + ': ' + ', '.join(sorted(set(captions) - found)))
            for s in entry['symbols']:
                if (s.caption.split('\r')[0] in captions) if captions else (s.prefix and '<PARENT' not in s.prefix):
                    s.parent = True
            return
    raise KeyError(folder + ' / ' + de)


def C(de, en, fr=None):
    return names(de, en, fr or en)


BT = 'Elektro/Elektronik/Bauteile'

# Resistors
add = page(BT, 'Widerstände', 'Resistors', 'Résistances')
add(resistor_body(two_pin('R', C('Widerstand', 'Resistor', 'Résistance'), ask=True)))
s = resistor_body(two_pin('R', C('Widerstand vertikal', 'Resistor, vertical', 'Résistance verticale'), ask=True))
add(s.vertical().labels((2.0, P - 0.4), (2.0, 2 * P + 0.4)))
s = two_pin('R', C('Potentiometer', 'Potentiometer', 'Potentiomètre'), ask=True)
resistor_body(s).line((2 * P, 2 * P), (2 * P, 1.0), end='arrow', size=1.2).pin('3', (2 * P, 2 * P), label=(2 * P + 0.5, 2 * P - 2.2))
s.labels((2 * P, -5.6), (3 * P + 0.6, 1.6), 'centre', 'left')
add(s)
s = two_pin('R', C('Trimmer', 'Trimmer', 'Résistance ajustable'), ask=True)
resistor_body(s).line((2 * P, 2 * P), (2 * P, 1.7), end='bar', size=1.8).pin('3', (2 * P, 2 * P), label=(2 * P + 0.5, 2 * P - 2.2))
s.labels((2 * P, -5.6), (3 * P + 0.6, 1.6), 'centre', 'left')
add(s)
s = two_pin('R', C('Einstellbarer Widerstand', 'Variable resistor', 'Résistance variable'), ask=True)
add(resistor_body(s).arrow((P + 0.6, 2.2), (3 * P - 0.4, -2.4), size=1.2))
s = two_pin('R', C('Widerstand mit Leistungsangabe', 'Resistor with power rating', 'Résistance avec puissance'), ask=True)
add(resistor_body(s, h=2.4).line((P + 0.6, 0), (3 * P - 0.6, 0), w=0.5))
for n in (4, 8):
    s = Symbol('RN', C(f'Widerstandsnetzwerk {n}', f'Resistor network {n}', f'Réseau de résistances {n}'), ask=True)
    s.rect(P / 2, -1.0, (n + 0.5) * P, 3 * P + 1.0)
    for k in range(n):
        x = (k + 1) * P
        s.rect(x - 0.7, 0.4, x + 0.7, 2 * P - 0.6)
        s.line((x, 2 * P - 0.6), (x, 4 * P)).pin(str(k + 2), (x, 4 * P), label=(x + 0.3, 4 * P - 2.0))
        s.line((x, 0.4), (x, -0.4))
    s.line((P, -0.4), (n * P, -0.4)).line((P, -0.4), (P, -2 * P))
    s.pin('1', (P, -2 * P))
    s.labels(((n + 0.5) * P + 1.0, -1.0), ((n + 0.5) * P + 1.0, 2.0))
    add(s)

add = page(BT, 'Widerstände (abhängig)', 'Resistors (dependent)', 'Résistances (dépendantes)')
for prefix, sign, de, en, fr in (('R', '-ϑ', 'Heißleiter (NTC)', 'Thermistor (NTC)', 'Thermistance (CTN)'),
                                 ('R', '+ϑ', 'Kaltleiter (PTC)', 'Thermistor (PTC)', 'Thermistance (CTP)'),
                                 ('R', 'U', 'Varistor (VDR)', 'Varistor (VDR)', 'Varistance')):
    s = two_pin(prefix, C(de, en, fr), ask=True)
    resistor_body(s).line((P - 0.6, 2.4), (P + 0.4, 2.4), (3 * P + 0.2, -2.4))
    s.text(sign, 3 * P + 0.4, 0.6, 2.0)
    add(s)
s = two_pin('R', C('Fotowiderstand (LDR)', 'Photoresistor (LDR)', 'Photorésistance'), ask=True)
resistor_body(s)
arrows_in(s, 2 * P - 0.7, -1.3)
s.labels((2 * P, -7.0), (2 * P, 2.0), 'centre')
add(s)
s = two_pin('R', C('Magnetfeldabhängiger Widerstand', 'Magnetoresistor', 'Magnétorésistance'), ask=True)
resistor_body(s).line((P - 0.6, 2.4), (P + 0.4, 2.4), (3 * P + 0.2, -2.4)).text('B', 3 * P + 0.4, 0.6, 2.0)
add(s)

# Capacitors
add = page(BT, 'Kondensatoren', 'Capacitors', 'Condensateurs')


def capacitor(s, polar=False, x=2 * P):
    s.line((0, 0), (x - 0.5, 0)).line((x + 0.5, 0), (4 * P, 0))
    if polar:
        s.rect(x - 1.1, -2.2, x - 0.5, 2.2).rect(x + 0.5, -2.2, x + 1.1, 2.2, fill='#000000')
        s.text('+', x - 2.6, -3.2, 2.2, 'centre')
    else:
        s.line((x - 0.5, -2.2), (x - 0.5, 2.2), w=THICK).line((x + 0.5, -2.2), (x + 0.5, 2.2), w=THICK)
    return s


add(capacitor(two_pin('C', C('Kondensator', 'Capacitor', 'Condensateur'), ask=True)))
add(capacitor(two_pin('C', C('Elektrolytkondensator', 'Electrolytic capacitor', 'Condensateur électrolytique'), ask=True), polar=True))
add(capacitor(two_pin('C', C('Kondensator vertikal', 'Capacitor, vertical', 'Condensateur vertical'), ask=True)).vertical().labels((3.0, P - 0.4), (3.0, 2 * P + 0.4)))
s = capacitor(two_pin('C', C('Elektrolytkondensator vertikal', 'Electrolytic capacitor, vertical', 'Condensateur électrolytique vertical'), ask=True), polar=True).vertical()
for o in s.parts:
    if o['type'] == 'text' and o['text'] == '+':
        o['pos'], o['align'] = pt((-3.6, 2 * P - 3.6)), 'centre'
add(s.labels((3.0, P - 0.4), (3.0, 2 * P + 0.4)))
s = two_pin('C', C('Elko (gebogene Platte)', 'Polarised capacitor', 'Condensateur polarisé'), ask=True)
s.line((0, 0), (2 * P - 0.5, 0)).line((2 * P - 0.5, -2.2), (2 * P - 0.5, 2.2), w=THICK)
s.arc(2 * P + 2.6, 0, 5.4, 140, 220, w=THICK).line((2 * P + 0.55, 0), (4 * P, 0)).text('+', 2 * P - 2.2, -3.4, 2.2, 'centre')
add(s)
s = capacitor(two_pin('C', C('Drehkondensator', 'Variable capacitor', 'Condensateur variable'), ask=True))
add(s.arrow((P + 0.4, 2.6), (3 * P - 0.4, -2.8), size=1.2))
s = capacitor(two_pin('C', C('Trimmkondensator', 'Trimmer capacitor', 'Condensateur ajustable'), ask=True))
add(s.line((P + 0.4, 2.6), (3 * P - 0.4, -2.8), end='bar', size=1.6))
s = capacitor(two_pin('C', C('Durchführungskondensator', 'Feed-through capacitor', 'Condensateur de traversée'), ask=True))
add(s.arc(2 * P, 3.4, 4.0, 0, 180))

# Inductors
add = page(BT, 'Spulen', 'Inductors', 'Bobines')
s = two_pin('L', C('Spule', 'Inductor', 'Bobine'), ask=True)
add(coil(s.line((0, 0), (P, 0)).line((3 * P, 0), (4 * P, 0)), P, 3 * P))
s = two_pin('L', C('Spule vertikal', 'Inductor, vertical', 'Bobine verticale'), ask=True)
add(coil(s.line((0, 0), (P, 0)).line((3 * P, 0), (4 * P, 0)), P, 3 * P).vertical().labels((2.4, P - 0.4), (2.4, 2 * P + 0.4)))
s = two_pin('L', C('Spule mit Kern', 'Inductor with core', 'Bobine avec noyau'), ask=True)
add(coil(s.line((0, 0), (P, 0)).line((3 * P, 0), (4 * P, 0)), P, 3 * P).line((P, -1.9), (3 * P, -1.9), w=THICK))
s = two_pin('L', C('Spule mit Ferritkern', 'Inductor with ferrite core', 'Bobine à noyau de ferrite'), ask=True)
s.parts.append({'type': 'line', 'points': [pt((P, -1.9)), pt((3 * P, -1.9))], 'pen': {'width': THICK, 'style': 'dash'}, 'electrical': False})
add(coil(s.line((0, 0), (P, 0)).line((3 * P, 0), (4 * P, 0)), P, 3 * P))
s = two_pin('L', C('Einstellbare Spule', 'Variable inductor', 'Bobine variable'), ask=True)
add(coil(s.line((0, 0), (P, 0)).line((3 * P, 0), (4 * P, 0)), P, 3 * P).arrow((P + 0.2, 2.0), (3 * P - 0.2, -3.0), size=1.2))
s = two_pin('L', C('Spule mit Anzapfung', 'Tapped inductor', 'Bobine à prise'), ask=True)
coil(s.line((0, 0), (P, 0)).line((3 * P, 0), (4 * P, 0)), P, 3 * P)
add(s.line((2 * P, 0), (2 * P, 2 * P)).pin('3', (2 * P, 2 * P), label=(2 * P + 0.5, 2 * P - 2.2)))
s = two_pin('L', C('Drossel (Rechteck)', 'Choke (block)', 'Self (rectangle)'), ask=True)
add(s.line((0, 0), (P, 0)).rect(P, -1.0, 3 * P, 1.0, fill='#000000').line((3 * P, 0), (4 * P, 0)))

# Diodes
for page_name, fill in ((('Dioden', 'Diodes', 'Diodes'), None), (('Dioden (schwarz)', 'Diodes (black)', 'Diodes (noires)'), '#000000')):
    add = page(BT, *page_name)
    s = two_pin('D', C('Diode', 'Diode', 'Diode'))
    add(diode_shape(s.line((0, 0), (1.5 * P, 0)).line((2.5 * P, 0), (4 * P, 0)), 1.5 * P, 2.5 * P, fill=fill))
    s = two_pin('D', C('Z-Diode', 'Zener diode', 'Diode Zener'))
    diode_shape(s.line((0, 0), (1.5 * P, 0)).line((2.5 * P, 0), (4 * P, 0)), 1.5 * P, 2.5 * P, fill=fill)
    add(s.line((2.5 * P, -1.5), (2.5 * P - 0.8, -1.5)))
    s = two_pin('D', C('Schottky-Diode', 'Schottky diode', 'Diode Schottky'))
    diode_shape(s.line((0, 0), (1.5 * P, 0)).line((2.5 * P, 0), (4 * P, 0)), 1.5 * P, 2.5 * P, fill=fill)
    add(s.line((2.5 * P + 0.7, -1.0), (2.5 * P + 0.7, -1.5), (2.5 * P, -1.5)).line((2.5 * P, 1.5), (2.5 * P - 0.7, 1.5), (2.5 * P - 0.7, 1.0)))
    s = two_pin('CD' if fill else 'D', C('Kapazitätsdiode', 'Varicap diode', 'Diode varicap'))
    diode_shape(s.line((0, 0), (1.25 * P, 0)).line((2.25 * P + 0.8, 0), (4 * P, 0)), 1.25 * P, 2.25 * P, fill=fill)
    add(s.line((2.25 * P + 0.8, -1.5), (2.25 * P + 0.8, 1.5), w=0.35))
    s = two_pin('D', C('Tunneldiode', 'Tunnel diode', 'Diode tunnel'))
    diode_shape(s.line((0, 0), (1.5 * P, 0)).line((2.5 * P, 0), (4 * P, 0)), 1.5 * P, 2.5 * P, fill=fill)
    add(s.line((2.5 * P - 0.7, -1.5), (2.5 * P, -1.5)).line((2.5 * P - 0.7, 1.5), (2.5 * P, 1.5)))
    s = two_pin('D', C('Fotodiode', 'Photodiode', 'Photodiode'))
    diode_shape(s.line((0, 0), (1.5 * P, 0)).line((2.5 * P, 0), (4 * P, 0)), 1.5 * P, 2.5 * P, fill=fill)
    arrows_in(s, 2 * P - 0.2, -1.6)
    s.labels((2 * P, -7.4), (2 * P, 2.0), 'centre')
    add(s)
    s = two_pin('D', C('Diode vertikal', 'Diode, vertical', 'Diode verticale'))
    s.parts = []
    s.pin('1', (0, 0)).pin('2', (0, 4 * P), label=(0.4, 4 * P - 2.2))
    s.line((0, 0), (0, 1.5 * P)).line((0, 2.5 * P), (0, 4 * P)).poly((-1.5, 1.5 * P), (1.5, 1.5 * P), (0, 2.5 * P), fill=fill)
    s.line((-1.5, 2.5 * P), (1.5, 2.5 * P), w=0.35).labels((2.4, P - 0.6), (2.4, 2 * P + 0.4))
    add(s)
    s = Symbol('T', C('Brückengleichrichter', 'Bridge rectifier', 'Pont redresseur'))
    c = (2 * P, 0)
    corners = {'left': (0, 0), 'top': (2 * P, -2 * P), 'right': (4 * P, 0), 'bottom': (2 * P, 2 * P)}
    for a, b in (('bottom', 'left'), ('bottom', 'right'), ('left', 'top'), ('right', 'top')):
        pa, pb = corners[a], corners[b]
        s.line(pa, pb)
        mx, my = (pa[0] + pb[0]) / 2, (pa[1] + pb[1]) / 2
        ang = math.degrees(math.atan2(-(pb[1] - pa[1]), pb[0] - pa[0]))
        tri = [rot((mx - 0.9, my - 1.0), (mx, my), ang), rot((mx - 0.9, my + 1.0), (mx, my), ang), rot((mx + 0.9, my), (mx, my), ang)]
        s.poly(*tri, fill=fill)
        s.line(rot((mx + 0.9, my - 1.0), (mx, my), ang), rot((mx + 0.9, my + 1.0), (mx, my), ang), w=0.35)
    s.pin('~1', corners['left']).pin('~2', corners['right'], label=(4 * P - 0.4, -2.2), align='right')
    s.pin('+', corners['top'], label=(2 * P + 0.6, -2 * P)).pin('-', corners['bottom'], label=(2 * P + 0.6, 2 * P - 2.2))
    s.text('~', 0.6, 0.6, 2.2).text('~', 4 * P - 0.6, 0.6, 2.2, 'right').text('+', 2 * P + 1.0, -2 * P + 0.2, 2.2).text('−', 2 * P + 1.0, 2 * P - 2.6, 2.2)
    s.labels((4 * P + 0.6, -2 * P), (4 * P + 0.6, -2 * P + 3.0))
    add(s)

# LEDs
add = page(BT, 'LED', 'LED', 'LED')
s = two_pin('D', C('Leuchtdiode', 'Light-emitting diode', 'Diode électroluminescente'), value='LED')
diode_shape(s.line((0, 0), (1.5 * P, 0)).line((2.5 * P, 0), (4 * P, 0)), 1.5 * P, 2.5 * P)
arrows_out(s, 2 * P - 0.4, -1.8)
s.labels((2 * P, -7.4), (2 * P, 2.0), 'centre')
add(s)
s = two_pin('D', C('Leuchtdiode vertikal', 'LED, vertical', 'DEL verticale'), value='LED')
s.parts = []
s.pin('1', (0, 0)).pin('2', (0, 4 * P), label=(0.4, 4 * P - 2.2))
s.line((0, 0), (0, 1.5 * P)).line((0, 2.5 * P), (0, 4 * P)).poly((-1.5, 1.5 * P), (1.5, 1.5 * P), (0, 2.5 * P), fill=None)
s.line((-1.5, 2.5 * P), (1.5, 2.5 * P), w=0.35)
arrows_out(s, 1.6, 2 * P - 0.4)
s.labels((5.4, P - 0.6), (5.4, 2 * P + 0.4))
add(s)
s = Symbol('D', C('Zweifarbige LED', 'Bicolour LED', 'DEL bicolore'), 'LED')
for y, flip in ((0, False), (2 * P, True)):
    s.line((0, y), (1.5 * P, y)).line((2.5 * P, y), (4 * P, y))
    if flip:
        s.poly((2.5 * P, y - 1.5), (2.5 * P, y + 1.5), (1.5 * P, y), fill=None).line((1.5 * P, y - 1.5), (1.5 * P, y + 1.5), w=0.35)
    else:
        diode_shape(s, 1.5 * P, 2.5 * P)
s.line((0, 0), (0, 2 * P)).line((4 * P, 0), (4 * P, 2 * P))
arrows_out(s, 2 * P + 1.2, -1.6)
s.pin('1', (0, P)).pin('2', (4 * P, P), label=(4 * P - 0.4, P - 2.2), align='right')
s.labels((2 * P, -7.6), (2 * P, 2 * P + 2.0), 'centre')
add(s)
s = Symbol('D', C('RGB-LED (gemeinsame Kathode)', 'RGB LED (common cathode)', 'DEL RVB (cathode commune)'), 'RGB')
for k, colour in enumerate(('R', 'G', 'B')):
    y = k * P * 2
    s.line((0, y), (1.5 * P, y)).line((2.5 * P, y), (4 * P, y))
    diode_shape(s, 1.5 * P, 2.5 * P, y=y).text(colour, 0.4, y - 2.6, 1.8)
    s.pin(colour, (0, y))
s.line((4 * P, 0), (4 * P, 4 * P)).line((4 * P, 2 * P), (5 * P, 2 * P)).pin('K', (5 * P, 2 * P), label=(5 * P - 0.4, 2 * P - 2.2), align='right')
s.labels((5 * P + 0.6, -1.0), (5 * P + 0.6, 2.0))
add(s)
s = Symbol('U', C('Optokoppler', 'Optocoupler', 'Optocoupleur'))
s.rect(P, -P, 7 * P, 3 * P, w=W)
s.line((0, 0), (2 * P, 0), (2 * P, 0.5 * P)).line((2 * P, 1.5 * P), (2 * P, 2 * P), (0, 2 * P))
s.poly((2 * P - 1.4, 0.5 * P), (2 * P + 1.4, 0.5 * P), (2 * P, 1.5 * P), fill=None).line((2 * P - 1.4, 1.5 * P), (2 * P + 1.4, 1.5 * P), w=0.35)
s.arrow((3 * P - 0.6, 0.8 * P), (4 * P - 0.4, 0.8 * P), size=0.9).arrow((3 * P - 0.6, 1.3 * P), (4 * P - 0.4, 1.3 * P), size=0.9)
s.line((4.6 * P, 0.1 * P), (4.6 * P, 1.9 * P), w=THICK)
s.line((4.6 * P, 0.6 * P), (6 * P, -0.2 * P), (6 * P, 0), (8 * P, 0))
s.line((4.6 * P, 1.4 * P), (6 * P, 2.2 * P), end='arrow', size=1.2).line((6 * P, 2.2 * P), (6 * P, 2 * P), (8 * P, 2 * P))
s.pin('1', (0, 0)).pin('2', (0, 2 * P)).pin('4', (8 * P, 0), label=(8 * P - 0.4, -2.2), align='right').pin('3', (8 * P, 2 * P), label=(8 * P - 0.4, 2 * P - 2.2), align='right')
s.labels((P, -P - 3.4), (P, 3 * P + 0.6))
add(s)

# Transistors
add = page(BT, 'Transistoren', 'Transistors', 'Transistors')
add(transistor(False, C('NPN-Transistor', 'NPN transistor', 'Transistor NPN')))
add(transistor(True, C('PNP-Transistor', 'PNP transistor', 'Transistor PNP')))
add(transistor(False, C('Fototransistor', 'Phototransistor', 'Phototransistor'), photo=True))
s = Symbol('K', C('Unijunction-Transistor', 'Unijunction transistor', 'Transistor unijonction'))
s.circle(4.6, 0, 7.4).line((3.4, -2.2), (3.4, 2.2), w=THICK).line((0, P), (1.2, P)).line((1.2, P), (3.4, -0.4), end='arrow', size=1.3)
s.line((3.4, -1.7), (2 * P, -1.7), (2 * P, -2 * P)).line((3.4, 1.7), (2 * P, 1.7), (2 * P, 2 * P))
s.pin('E', (0, P)).pin('B2', (2 * P, -2 * P)).pin('B1', (2 * P, 2 * P)).labels((9.2, -2.6), (9.2, 0.4))
add(s)

add = page(BT, 'FET', 'FET', 'FET')
add(jfet(False, C('N-Kanal-Sperrschicht-FET', 'N-channel JFET', 'JFET canal N')))
add(jfet(True, C('P-Kanal-Sperrschicht-FET', 'P-channel JFET', 'JFET canal P')))
add(mosfet(False, False, C('N-Kanal-MOSFET (Anreicherung)', 'N-channel MOSFET (enhancement)', 'MOSFET canal N (enrichissement)')))
add(mosfet(True, False, C('P-Kanal-MOSFET (Anreicherung)', 'P-channel MOSFET (enhancement)', 'MOSFET canal P (enrichissement)')))
add(mosfet(False, True, C('N-Kanal-MOSFET (Verarmung)', 'N-channel MOSFET (depletion)', 'MOSFET canal N (appauvrissement)')))
add(mosfet(True, True, C('P-Kanal-MOSFET (Verarmung)', 'P-channel MOSFET (depletion)', 'MOSFET canal P (appauvrissement)')))

add = page(BT, 'Thyristoren', 'Thyristors', 'Thyristors')
s = two_pin('Q', C('Thyristor', 'Thyristor', 'Thyristor'))
diode_shape(s.line((0, 0), (1.5 * P, 0)).line((2.5 * P, 0), (4 * P, 0)), 1.5 * P, 2.5 * P)
add(s.line((2.5 * P, 0.6), (3 * P, 2.0), (3 * P, 2 * P)).pin('G', (3 * P, 2 * P), label=(3 * P + 0.5, 2 * P - 2.2)))
s = two_pin('Q', C('Triac', 'Triac', 'Triac'))
s.line((0, 0), (1.5 * P, 0)).line((2.5 * P, 0), (4 * P, 0))
s.line((1.5 * P, -2.6), (1.5 * P, 2.6), w=0.35).line((2.5 * P, -2.6), (2.5 * P, 2.6), w=0.35)
s.poly((1.5 * P, -2.4), (1.5 * P, -0.1), (2.5 * P, -1.25), fill=None).poly((2.5 * P, 0.1), (2.5 * P, 2.4), (1.5 * P, 1.25), fill=None)
add(s.line((2.5 * P, 1.8), (3 * P, 2.6), (3 * P, 2 * P)).pin('G', (3 * P, 2 * P), label=(3 * P + 0.5, 2 * P - 2.2)))
s = two_pin('Q', C('Diac', 'Diac', 'Diac'))
s.line((0, 0), (1.5 * P, 0)).line((2.5 * P, 0), (4 * P, 0))
s.line((1.5 * P, -2.6), (1.5 * P, 2.6), w=0.35).line((2.5 * P, -2.6), (2.5 * P, 2.6), w=0.35)
add(s.poly((1.5 * P, -2.4), (1.5 * P, -0.1), (2.5 * P, -1.25), fill=None).poly((2.5 * P, 0.1), (2.5 * P, 2.4), (1.5 * P, 1.25), fill=None))
s = two_pin('Q', C('GTO-Thyristor', 'GTO thyristor', 'Thyristor GTO'))
diode_shape(s.line((0, 0), (1.5 * P, 0)).line((2.5 * P, 0), (4 * P, 0)), 1.5 * P, 2.5 * P)
s.line((2.5 * P, 0.6), (3 * P, 2.0), (3 * P, 2 * P)).pin('G', (3 * P, 2 * P), label=(3 * P + 0.5, 2 * P - 2.2))
add(s.line((2.5 * P + 0.3, 1.5), (2.5 * P + 1.3, 1.1)))

add = page(BT, "OP's", 'Op-amps', 'Amplis op')
for de, en, fr, supply_pins in (('Operationsverstärker', 'Operational amplifier', 'Amplificateur opérationnel', False),
                                ('Operationsverstärker mit Versorgung', 'Op-amp with supply', 'Ampli op avec alimentation', True),
                                ('Komparator', 'Comparator', 'Comparateur', False)):
    s = Symbol('K', C(de, en, fr))
    s.poly((P, -2 * P), (P, 2 * P), (5 * P, 0), fill=None, w=0.35)
    s.line((0, -P), (P, -P)).line((0, P), (P, P)).line((5 * P, 0), (6 * P, 0))
    s.text('−', P + 0.5, -P - 1.4, 2.5).text('+', P + 0.5, P - 1.4, 2.5)
    s.pin('2', (0, -P)).pin('3', (0, P)).pin('1', (6 * P, 0), label=(6 * P - 0.4, -2.2), align='right')
    if supply_pins:
        s.line((3 * P, -P), (3 * P, -2 * P)).line((3 * P, P), (3 * P, 2 * P))
        s.pin('7', (3 * P, -2 * P), label=(3 * P + 0.4, -2 * P)).pin('4', (3 * P, 2 * P), label=(3 * P + 0.4, 2 * P - 2.2))
    if de == 'Komparator':
        s.line((2.4 * P, -0.5), (2.9 * P, -0.5), (2.9 * P, 0.5), (3.4 * P, 0.5))
    s.labels((3.6 * P, -2 * P - 1.0), (3.6 * P, 2 * P - 1.8))
    add(s)

add = page(BT, 'Quarze', 'Crystals', 'Quartz')
s = two_pin('X', C('Quarz', 'Crystal', 'Quartz'), ask=True)
s.line((0, 0), (1.4 * P, 0)).line((1.4 * P, -2.2), (1.4 * P, 2.2), w=THICK).rect(1.65 * P, -1.8, 2.35 * P, 1.8)
add(s.line((2.6 * P, -2.2), (2.6 * P, 2.2), w=THICK).line((2.6 * P, 0), (4 * P, 0)))
s = two_pin('X', C('Keramikresonator', 'Ceramic resonator', 'Résonateur céramique'), ask=True)
s.line((0, 0), (1.4 * P, 0)).line((1.4 * P, -2.2), (1.4 * P, 2.2), w=THICK).rect(1.65 * P, -1.8, 2.35 * P, 1.8)
s.line((2.6 * P, -2.2), (2.6 * P, 2.2), w=THICK).line((2.6 * P, 0), (4 * P, 0)).line((2 * P, 1.8), (2 * P, 2 * P)).pin('3', (2 * P, 2 * P), label=(2 * P + 0.5, 2 * P - 2.2))
s.labels((2 * P, -4.6), (3 * P, 2.0), 'centre', 'left')
add(s)

add = page(BT, 'Spannungsregler', 'Voltage regulators', 'Régulateurs de tension')
for de, en, fr, value in (('Festspannungsregler', 'Fixed voltage regulator', 'Régulateur fixe', '7805'),
                          ('Einstellbarer Spannungsregler', 'Adjustable voltage regulator', 'Régulateur ajustable', 'LM317')):
    s = Symbol('T', C(de, en, fr), value)
    s.rect(P, -P, 5 * P, 2 * P)
    s.line((0, 0), (P, 0)).line((5 * P, 0), (6 * P, 0)).line((3 * P, 2 * P), (3 * P, 3 * P))
    s.text('IN', P + 0.5, -1.0, 2.0).text('OUT', 5 * P - 0.5, -1.0, 2.0, 'right').text('ADJ' if 'LM' in value else 'GND', 3 * P, 2 * P - 2.6, 2.0, 'centre')
    s.pin('IN', (0, 0)).pin('OUT', (6 * P, 0), label=(6 * P - 0.4, -2.2), align='right').pin('ADJ' if 'LM' in value else 'GND', (3 * P, 3 * P), label=(3 * P + 0.4, 3 * P - 2.2))
    s.labels((P, -P - 3.4), (P + 0.0, 3 * P + 0.4))
    add(s)

add = page(BT, 'Batterien', 'Batteries', 'Piles')
s = two_pin('G', C('Zelle', 'Cell', 'Élément'))
s.line((0, 0), (2 * P - 0.5, 0)).line((2 * P - 0.5, -2.6), (2 * P - 0.5, 2.6)).line((2 * P + 0.5, -1.3), (2 * P + 0.5, 1.3), w=0.7).line((2 * P + 0.5, 0), (4 * P, 0))
add(s.text('+', 2 * P - 2.0, -3.6, 2.2, 'centre'))
s = two_pin('G', C('Batterie', 'Battery', 'Batterie'))
s.line((0, 0), (P + 0.5, 0)).line((3 * P - 0.5, 0), (4 * P, 0))
for x in (P + 0.5, 2 * P + 0.3):
    s.line((x, -2.6), (x, 2.6)).line((x + 1.0, -1.3), (x + 1.0, 1.3), w=0.7)
s.line((P + 1.5, 0), (2 * P + 0.3, 0))
add(s.text('+', P - 1.0, -3.6, 2.2, 'centre'))
s = two_pin('G', C('Batterie (mehrere Zellen)', 'Battery (several cells)', 'Batterie (plusieurs éléments)'))
s.line((0, 0), (P + 0.5, 0)).line((3 * P - 0.5, 0), (4 * P, 0))
s.line((P + 0.5, -2.6), (P + 0.5, 2.6)).line((P + 1.5, -1.3), (P + 1.5, 1.3), w=0.7)
s.parts.append({'type': 'line', 'points': [pt((P + 1.5, 0)), pt((3 * P - 1.5, 0))], 'pen': {'width': W, 'style': 'dash'}, 'electrical': False})
s.line((3 * P - 1.5, -2.6), (3 * P - 1.5, 2.6)).line((3 * P - 0.5, -1.3), (3 * P - 0.5, 1.3), w=0.7)
add(s.text('+', P - 1.0, -3.6, 2.2, 'centre'))
s = two_pin('G', C('Akku', 'Rechargeable battery', 'Accumulateur'))
s.line((0, 0), (2 * P - 0.5, 0)).line((2 * P - 0.5, -2.6), (2 * P - 0.5, 2.6)).line((2 * P + 0.5, -1.3), (2 * P + 0.5, 1.3), w=0.7).line((2 * P + 0.5, 0), (4 * P, 0))
add(s.text('+', 2 * P - 2.0, -3.6, 2.2, 'centre').arrow((P + 1.0, 2.6), (3 * P - 1.0, -2.8), size=1.0))

add = page(BT, 'Transformatoren', 'Transformers', 'Transformateurs')


def transformer(s, secondary=1, centre_tap=False, core=True):
    s.line((0, 0), (P, 0), (P, P)).line((P, 3 * P), (P, 4 * P), (0, 4 * P))
    for k in range(4):
        s.arc(P, P + (k + 0.5) * P / 2, P / 2, 270, 450)
    s.pin('1', (0, 0)).pin('2', (0, 4 * P), label=(0.4, 4 * P - 2.2))
    if core:
        s.line((2 * P - 0.6, 0.6 * P), (2 * P - 0.6, 3.4 * P), w=THICK).line((2 * P + 0.6, 0.6 * P), (2 * P + 0.6, 3.4 * P), w=THICK)
    x = 3 * P
    s.line((4 * P, 0), (x, 0), (x, P)).line((x, 3 * P), (x, 4 * P), (4 * P, 4 * P))
    for k in range(4):
        s.arc(x, P + (k + 0.5) * P / 2, P / 2, 90, 270)
    s.pin('3', (4 * P, 0), label=(4 * P - 0.4, -2.2), align='right').pin('4', (4 * P, 4 * P), label=(4 * P - 0.4, 4 * P - 2.2), align='right')
    if centre_tap:
        s.line((x, 2 * P), (4 * P, 2 * P)).pin('5', (4 * P, 2 * P), label=(4 * P - 0.4, 2 * P - 2.2), align='right')
    s.labels((4 * P + 0.8, P), (4 * P + 0.8, 2 * P + 0.4))
    return s


add(transformer(Symbol('T', C('Transformator', 'Transformer', 'Transformateur'))))
add(transformer(Symbol('T', C('Transformator mit Mittelanzapfung', 'Centre-tapped transformer', 'Transformateur à point milieu')), centre_tap=True))
add(transformer(Symbol('T', C('Übertrager (ohne Kern)', 'Air-core transformer', 'Transformateur à air')), core=False))

add = page(BT, 'Elektroakustik', 'Electroacoustics', 'Électroacoustique')
s = two_pin('B', C('Lautsprecher', 'Loudspeaker', 'Haut-parleur'))
s.parts = []
s.pin('1', (0, 0)).pin('2', (0, 2 * P), label=(0.4, 2 * P - 2.2))
s.line((0, 0), (P, 0)).line((0, 2 * P), (P, 2 * P)).rect(P, -0.6, P + 2.0, 2 * P + 0.6).poly((P + 2.0, -0.6), (P + 5.0, -3.0), (P + 5.0, 2 * P + 3.0), (P + 2.0, 2 * P + 0.6), fill=None)
add(s.labels((P + 6.0, -1.0), (P + 6.0, 2.0)))
s = two_pin('B', C('Mikrofon', 'Microphone', 'Microphone'))
s.parts = []
s.pin('1', (0, 0)).pin('2', (0, 2 * P), label=(0.4, 2 * P - 2.2))
s.line((0, 0), (P, 0), (P, 0.6)).line((0, 2 * P), (P, 2 * P), (P, 2 * P - 0.6)).circle(P + 2.6, P, 5.2).line((P, P - 2.6), (P, P + 2.6), w=THICK)
add(s.labels((P + 6.2, -1.0), (P + 6.2, 2.0)))
s = two_pin('BZ', C('Summer', 'Buzzer', 'Ronfleur'))
s.parts = []
s.pin('1', (0, 0)).pin('2', (2 * P, 0), label=(2 * P - 0.4, -2.2), align='right')
s.line((0, 0), (0, -1.0)).line((2 * P, 0), (2 * P, -1.0)).arc(P, -1.0, 2 * P + 1.0, 0, 180).line((-0.5 - 0.0, -1.0), (2 * P + 0.5, -1.0))
add(s.labels((2 * P + 1.6, -6.0), (2 * P + 1.6, -3.2)))
s = two_pin('BZ', C('Piezo-Schallgeber', 'Piezo sounder', 'Transducteur piézo'))
s.line((0, 0), (1.4 * P, 0)).line((1.4 * P, -2.2), (1.4 * P, 2.2), w=THICK).rect(1.65 * P, -1.8, 2.35 * P, 1.8)
s.line((2.6 * P, -2.2), (2.6 * P, 2.2), w=THICK).line((2.6 * P, 0), (4 * P, 0))
add(s.arc(2 * P, -1.0, 7.0, 50, 130).labels((2 * P, -8.4), (2 * P, 2.8), 'centre'))
s = two_pin('B', C('Kopfhörer', 'Headphones', 'Casque'))
s.parts = []
s.pin('1', (0, 0)).pin('2', (2 * P, 0), label=(2 * P - 0.4, -2.2), align='right')
s.line((0, 0), (0, -1.6)).line((2 * P, 0), (2 * P, -1.6)).arc(P, -1.6, 2 * P, 0, 180).rect(-1.0, -1.6, 0.8, 0.0, fill='#000000').rect(2 * P - 0.8, -1.6, 2 * P + 1.0, 0.0, fill='#000000')
add(s.labels((2 * P + 2.0, -5.0), (2 * P + 2.0, -2.2)))

add = page(BT, 'Stecker, Buchsen, Klemmen', 'Plugs, sockets, terminals', 'Fiches, prises, bornes')
s = Symbol('X', C('Klemme', 'Terminal', 'Borne'))
add(s.circle(0, 0, 1.6).pin('1', (0, 0)).labels((1.4, -3.4), (1.4, 0.8)))
s = Symbol('X', C('Lötstützpunkt', 'Solder point', 'Point de soudure'))
add(s.circle(0, 0, 1.2, fill='#000000').pin('1', (0, 0)).labels((1.4, -3.4), (1.4, 0.8)))
s = Symbol('X', C('Buchse', 'Socket', 'Prise'))
add(s.line((0, 0), (P, 0)).arc(P + 1.6, 0, 3.2, 90, 270, w=0.35).pin('1', (0, 0)).labels((P, -4.4), (P, 2.4)))
s = Symbol('X', C('Stecker', 'Plug', 'Fiche'))
add(s.line((0, 0), (P + 1.6, 0)).rect(P + 1.6, -0.6, P + 3.2, 0.6, fill='#000000').pin('1', (0, 0)).labels((P, -3.4), (P, 1.6)))
s = Symbol('X', C('Steckverbindung', 'Plug and socket', 'Connecteur'))
s.line((0, 0), (P + 1.6, 0)).rect(P + 1.6, -0.6, P + 3.2, 0.6, fill='#000000').arc(P + 4.8, 0, 3.2, 90, 270, w=0.35).line((P + 3.2, 0), (P + 6.4, 0))
add(s.line((P + 6.4, 0), (4 * P, 0)).pin('1', (0, 0)).pin('2', (4 * P, 0), label=(4 * P - 0.4, -2.2), align='right').labels((2 * P, -4.6), (2 * P, 2.4), 'centre'))
s = Symbol('X', C('Klinkenbuchse (Stereo)', 'Jack socket (stereo)', 'Jack (stéréo)'))
s.rect(P, -P, 2 * P, 3 * P)
s.line((0, 0), (P, 0)).line((0, P), (P, P)).line((0, 2 * P), (P, 2 * P))
s.line((2 * P, -0.6 * P), (4 * P, -0.6 * P), (4.4 * P, -0.2 * P), (4.8 * P, -0.6 * P))
s.line((2 * P, 0.6 * P), (3.4 * P, 0.6 * P), (3.8 * P, 1.0 * P), (4.2 * P, 0.6 * P))
s.pin('T', (0, 0)).pin('R', (0, P)).pin('S', (0, 2 * P))
add(s.labels((P, -P - 3.4), (P, 3 * P + 0.6)))
for n in (2, 3, 4, 5, 6, 8, 10):
    s = Symbol('X', C(f'Stiftleiste 1×{n}', f'Pin header 1×{n}', f'Barrette 1×{n}'))
    s.rect(P, -P / 2, 2 * P, (n - 0.5) * P)
    for k in range(n):
        y = k * P
        s.line((0, y), (P + P / 2, y)).circle(P + P / 2, y, 0.9, fill='#000000')
        s.pin(str(k + 1), (0, y), label=(2 * P + 0.6, y - 1.0), shown=True)
    s.labels((P, -P / 2 - 3.4), (P, (n - 0.5) * P + 0.6))
    add(s)
for n in (2, 3):
    s = Symbol('X', C(f'Schraubklemme {n}-polig', f'Screw terminal, {n} poles', f'Bornier à vis, {n} pôles'))
    s.rect(P, -P / 2, 2.6 * P, (n - 0.5) * P)
    for k in range(n):
        y = k * P
        s.line((0, y), (P, y)).circle(1.8 * P, y, 1.6).line((1.8 * P - 0.6, y + 0.6), (1.8 * P + 0.6, y - 0.6))
        s.pin(str(k + 1), (0, y), label=(2.6 * P + 0.6, y - 1.0), shown=True)
    s.labels((P, -P / 2 - 3.4), (P, (n - 0.5) * P + 0.6))
    add(s)

EL = 'Elektro'
add = page(EL, 'Masse / Erde', 'Ground / Earth', 'Masse / Terre')
add(ground('chassis', C('Masse', 'Chassis', 'Masse')))
add(ground('earth', C('Erde', 'Earth', 'Terre')))
add(ground('protective', C('Schutzerde', 'Protective earth', 'Terre de protection')))
add(ground('signal', C('Signalmasse', 'Signal ground', 'Masse de signal')))
add(ground('noiseless', C('Fremdspannungsarme Erde', 'Noiseless earth', 'Terre sans bruit')))
add(supply('+5V', C('Versorgung', 'Supply', 'Alimentation')))
add(supply('GND', C('Masse (Dreieck)', 'Ground (triangle)', 'Masse (triangle)'), down=True))

add = page(EL, 'Antennen', 'Antennas', 'Antennes')
s = Symbol('T', C('Antenne', 'Antenna', 'Antenne'))
add(s.line((0, 0), (0, -3 * P)).line((-2.0, -4 * P), (0, -3 * P), (2.0, -4 * P)).line((0, -3 * P), (0, -4 * P)).pin('1', (0, 0)).labels((2.4, -3 * P), (2.4, -2 * P + 0.4)))
s = Symbol('T', C('Dipol', 'Dipole', 'Dipôle'))
s.line((-P, 0), (-P, -2 * P), (-3 * P, -2 * P)).line((P, 0), (P, -2 * P), (3 * P, -2 * P))
add(s.pin('1', (-P, 0)).pin('2', (P, 0), label=(P + 0.4, -2.2)).labels((3 * P + 0.6, -2 * P - 1.0), (3 * P + 0.6, -2 * P + 2.0)))
s = Symbol('T', C('Rahmenantenne', 'Loop antenna', 'Antenne cadre'))
s.line((-P, 0), (-P, -P)).line((P, 0), (P, -P)).rect(-2 * P, -4 * P, 2 * P, -P)
add(s.pin('1', (-P, 0)).pin('2', (P, 0), label=(P + 0.4, -2.2)).labels((2 * P + 0.6, -4 * P), (2 * P + 0.6, -3 * P + 0.4)))
s = Symbol('T', C('Antenne mit Gegengewicht', 'Antenna with counterpoise', 'Antenne avec contrepoids'))
s.line((0, 0), (0, -3 * P)).line((-2.0, -4 * P), (0, -3 * P), (2.0, -4 * P)).line((0, -3 * P), (0, -4 * P))
add(s.line((0, -P), (2 * P, -P), (2 * P, 0)).line((2 * P - 1.6, 0), (2 * P + 1.6, 0), w=0.35).pin('1', (0, 0)).labels((2.4, -3 * P - 1.0), (2.4, -2 * P - 0.6)))

add = page(EL, 'Generatoren', 'Generators', 'Générateurs')
for sign, de, en, fr, prefix in (('=', 'Gleichspannungsquelle', 'DC voltage source', 'Source de tension continue', 'G'),
                                 ('~', 'Wechselspannungsquelle', 'AC voltage source', 'Source de tension alternative', 'G'),
                                 ('G', 'Generator', 'Generator', 'Générateur', 'G')):
    s = meter(sign, C(de, en, fr))
    s.prefix = prefix
    add(s)
s = two_pin('G', C('Spannungsquelle (ideal)', 'Voltage source (ideal)', 'Source de tension (idéale)'))
add(s.line((0, 0), (P + 0.3, 0)).circle(2 * P, 0, 2 * P - 0.6).line((P + 0.3, 0), (3 * P - 0.3, 0)).line((3 * P - 0.3, 0), (4 * P, 0)))
s = two_pin('G', C('Stromquelle (ideal)', 'Current source (ideal)', 'Source de courant (idéale)'))
add(s.line((0, 0), (P + 0.3, 0)).circle(2 * P, 0, 2 * P - 0.6).line((2 * P, -P + 0.3), (2 * P, P - 0.3)).line((3 * P - 0.3, 0), (4 * P, 0)))

add = page(EL, 'Messinstrumente', 'Measuring instruments', 'Instruments de mesure')
for letter, de, en, fr in (('V', 'Spannungsmesser', 'Voltmeter', 'Voltmètre'), ('A', 'Strommesser', 'Ammeter', 'Ampèremètre'),
                           ('Ω', 'Widerstandsmesser', 'Ohmmeter', 'Ohmmètre'), ('W', 'Leistungsmesser', 'Wattmeter', 'Wattmètre'),
                           ('Hz', 'Frequenzmesser', 'Frequency meter', 'Fréquencemètre'), ('φ', 'Phasenmesser', 'Phase meter', 'Phasemètre')):
    add(meter(letter, C(de, en, fr)))
s = meter('', C('Oszilloskop', 'Oscilloscope', 'Oscilloscope'))
add(s.arc(2 * P - 0.9, 0, 1.8, 0, 180).arc(2 * P + 0.9, 0, 1.8, 180, 360))
s = meter('', C('Galvanometer', 'Galvanometer', 'Galvanomètre'))
add(s.arrow((2 * P, 2.0), (2 * P, -2.4), size=1.0))

add = page(EL, 'Signalgeber', 'Signal devices', 'Signal appareils')
s = two_pin('P', C('Lampe', 'Lamp', 'Lampe'))
add(s.line((0, 0), (P + 0.3, 0)).circle(2 * P, 0, 2 * P - 0.6).line((3 * P - 0.3, 0), (4 * P, 0)).line(rot((2 * P - 3.3, 0), (2 * P, 0), 45), rot((2 * P + 3.3, 0), (2 * P, 0), 45)).line(rot((2 * P - 3.3, 0), (2 * P, 0), -45), rot((2 * P + 3.3, 0), (2 * P, 0), -45)))
s = two_pin('P', C('Glimmlampe', 'Neon lamp', 'Lampe au néon'))
add(s.line((0, 0), (P + 0.3, 0)).circle(2 * P, 0, 2 * P - 0.6).line((3 * P - 0.3, 0), (4 * P, 0)).line((2 * P - 0.8, -1.6), (2 * P - 0.8, 1.6), w=0.35).line((2 * P + 0.8, -1.6), (2 * P + 0.8, 1.6), w=0.35).circle(2 * P - 1.6, 1.4, 0.5, fill='#000000'))
s = two_pin('P', C('Hupe', 'Horn', 'Klaxon'))
s.line((0, 0), (P, 0)).rect(P, -1.2, P + 1.6, 1.2).poly((P + 1.6, -1.2), (3 * P, -2.6), (3 * P, 2.6), (P + 1.6, 1.2), fill=None)
add(s.line((P + 0.8, 1.2), (P + 0.8, P)).line((P + 0.8, P), (4 * P, P), (4 * P, 0)))
s = two_pin('P', C('Klingel', 'Bell', 'Sonnerie'))
add(s.line((0, 0), (P, 0)).line((3 * P, 0), (4 * P, 0)).arc(2 * P, 0, 2 * P, 0, 180).line((P, 0), (3 * P, 0), w=0.35))
s = two_pin('P', C('Sirene', 'Siren', 'Sirène'))
add(s.line((0, 0), (P, 0)).line((3 * P, 0), (4 * P, 0)).arc(2 * P, 0, 2 * P, 0, 180).line((P, 0), (3 * P, 0), w=0.35).line((2 * P - 1.2, -1.2), (2 * P, -2.2), (2 * P + 1.2, -1.2)))
s = two_pin('P', C('Meldeleuchte', 'Indicator lamp', 'Voyant'))
add(s.line((0, 0), (P + 0.3, 0)).circle(2 * P, 0, 2 * P - 0.6).line((3 * P - 0.3, 0), (4 * P, 0)).circle(2 * P, 0, 1.6, fill='#000000'))

add = page(EL, 'Symbole', 'Symbols', 'Symboles')
for de, en, fr, draw in (
        ('Gleichstrom', 'Direct current', 'Courant continu', lambda s: s.line((0, 0), (4.0, 0), w=0.35)),
        ('Wechselstrom', 'Alternating current', 'Courant alternatif', lambda s: s.arc(1.0, 0, 2.0, 0, 180).arc(3.0, 0, 2.0, 180, 360)),
        ('Pfeil', 'Arrow', 'Flèche', lambda s: s.arrow((0, 0), (6.0, 0), size=1.6)),
        ('Doppelpfeil', 'Double arrow', 'Double flèche', lambda s: s.line((0, 0), (6.0, 0), start='arrow', end='arrow', size=1.6)),
        ('Einstellbarkeit', 'Adjustability', 'Réglabilité', lambda s: s.arrow((0, 3.0), (5.0, -2.0), size=1.4)),
        ('Licht (einfallend)', 'Light (incoming)', 'Lumière (entrante)', lambda s: arrows_in(s, 3.0, 0)),
        ('Licht (ausgesandt)', 'Light (emitted)', 'Lumière (émise)', lambda s: arrows_out(s, 0, 0)),
        ('Wärme', 'Heat', 'Chaleur', lambda s: [s.bezier((x, 0), (x + 1.0, -1.2), (x - 1.0, -2.4), (x, -3.6)) for x in (0, 1.6, 3.2)]),
        ('Abschirmung', 'Screen', 'Blindage', lambda s: s.parts.append({'type': 'line', 'points': [[0, 0], [10.16, 0]], 'pen': {'width': W, 'style': 'dash'}, 'electrical': False})),
        ('Plus', 'Plus', 'Plus', lambda s: s.text('+', 0, -1.6, 3.5, 'centre')),
        ('Minus', 'Minus', 'Moins', lambda s: s.text('−', 0, -1.6, 3.5, 'centre')),
        ('Hochspannung', 'High voltage', 'Haute tension', lambda s: s.line((1.6, -3.4), (0, 0.2), (2.2, -0.4), (0.6, 3.2), end='arrow', size=1.2))):
    s = Symbol('', C(de, en, fr), numbered=False, listed=False, shown=False)
    draw(s)
    add(s)

EI = 'Elektro/Elektroinstallation'
add = page(EI, 'Schalter (drücken)', 'Switches (push)', 'Commutateurs (pousser)')
add(contact('no', C('Taster (Schließer)', 'Push button (make)', 'Bouton-poussoir (travail)'), operated='push'))
add(contact('nc', C('Taster (Öffner)', 'Push button (break)', 'Bouton-poussoir (repos)'), operated='push'))
add(contact('co', C('Taster (Wechsler)', 'Push button (change-over)', 'Bouton-poussoir (inverseur)'), pins=('2', '1', '4'), operated='push'))
add = page(EI, 'Schalter (andere Betätigungen)', 'Switches (other operation)', 'Commutateurs (autres commandes)')
add(contact('no', C('Schließer', 'Make contact', 'Contact à fermeture')))
add(contact('nc', C('Öffner', 'Break contact', 'Contact à ouverture')))
add(contact('co', C('Wechsler', 'Change-over contact', 'Contact inverseur'), pins=('2', '1', '4')))
add(contact('no', C('Schalter (handbetätigt)', 'Switch (manual)', 'Interrupteur (manuel)'), operated='manual'))
add(contact('nc', C('Öffner (handbetätigt)', 'Break contact (manual)', 'Contact à ouverture (manuel)'), operated='manual'))
add = page(EI, 'Schütze + Kontakte [K]', 'Contactors + contacts [K]', 'Contacteurs + contacts [K]')
s = Symbol('K', C('Schützspule', 'Contactor coil', 'Bobine de contacteur'))
s.parent = True
s.rect(-2 * P, P, 2 * P, 3 * P, w=0.35).line((0, 0), (0, P)).line((0, 3 * P), (0, 4 * P))
add(s.pin('A1', (0, 0), label=(0.6, 0.2), shown=True).pin('A2', (0, 4 * P), label=(0.6, 4 * P - 2.4), shown=True).labels((2 * P + 0.8, P), (2 * P + 0.8, 2 * P + 0.4)))
s = Symbol('K', C('Relaisspule', 'Relay coil', 'Bobine de relais'))
s.parent = True
s.rect(-1.6, P, 1.6, 3 * P, w=0.35).line((0, 0), (0, P)).line((0, 3 * P), (0, 4 * P)).line((-1.6, 3 * P), (1.6, P))
add(s.pin('A1', (0, 0), label=(0.6, 0.2), shown=True).pin('A2', (0, 4 * P), label=(0.6, 4 * P - 2.4), shown=True).labels((2.4, P), (2.4, 2 * P + 0.4)))
add(contact('no', C('Schließer (Schütz)', 'Make contact (contactor)', 'Contact à fermeture (contacteur)'), 'K', ('13', '14'), operated='relay', numbers=True))
add(contact('nc', C('Öffner (Schütz)', 'Break contact (contactor)', 'Contact à ouverture (contacteur)'), 'K', ('21', '22'), operated='relay', numbers=True))
add(contact('co', C('Wechsler (Schütz)', 'Change-over contact (contactor)', 'Contact inverseur (contacteur)'), 'K', ('12', '11', '14'), operated='relay', numbers=True))
add = page(EI, 'Sicherungen', 'Fuses', 'Fusibles')
s = Symbol('F', C('Sicherung', 'Fuse', 'Fusible'), ask=True)
s.rect(-1.0, P, 1.0, 3 * P).line((0, 0), (0, 4 * P))
add(s.pin('1', (0, 0), label=(0.6, 0.2)).pin('2', (0, 4 * P), label=(0.6, 4 * P - 2.4)).labels((2.0, P), (2.0, 2 * P + 0.4)))
s = Symbol('F', C('Sicherung horizontal', 'Fuse, horizontal', 'Fusible horizontal'), ask=True)
add(s.rect(P, -1.0, 3 * P, 1.0).line((0, 0), (4 * P, 0)).pin('1', (0, 0)).pin('2', (4 * P, 0), label=(4 * P - 0.4, -2.2), align='right').labels((2 * P, -4.6), (2 * P, 1.6), 'centre'))
s = contact('no', C('Leitungsschutzschalter', 'Circuit breaker', 'Disjoncteur'), 'F')
add(s.line((-0.8, P - 0.8), (0.8, P + 0.8)).line((-0.8, P + 0.8), (0.8, P - 0.8)))
s = Symbol('F', C('Thermisches Überlastrelais', 'Thermal overload relay', 'Relais thermique'))
s.rect(-2 * P, P, 2 * P, 3 * P).line((0, 0), (0, P + 1.0), (-1.2, P + 1.0), (-1.2, 3 * P - 1.0), (0, 3 * P - 1.0), (0, 4 * P))
add(s.pin('1', (0, 0), label=(0.6, 0.2)).pin('2', (0, 4 * P), label=(0.6, 4 * P - 2.4)).labels((2 * P + 0.8, P), (2 * P + 0.8, 2 * P + 0.4)))
add = page(EI, 'Grundriss', 'Floor plan', 'Plan architectural')


def plan(caption, draw):
    """A sign of an installation plan after DIN EN 60617-11, placed on a floor plan: no connection points, numbered and
    counted in the parts list, the designator hidden."""
    s = Symbol('E', caption, shown=False)
    draw(s)
    return s


def switch_sign(s, strokes=1, through=False, crossed=False, dim=False):
    s.circle(0, 0, 1.6, w=W)
    d = 4.0 / math.sqrt(2)
    tick = lambda x, y, sx, sy: s.line((x, y), (x + sx * 0.9, y + sy * 0.9), w=W)
    s.line((0.57, -0.57), (d, -d), w=W)
    for k in range(strokes):
        o = 0.9 * k
        tick(d - o * 0.7, -d + o * 0.7, 0.7, 0.7)
    if through or crossed:
        s.line((-0.57, 0.57), (-d, d), w=W)
        tick(-d, d, -0.7, -0.7)
    if crossed:
        s.line((-0.57, -0.57), (-d, -d), w=W).line((0.57, 0.57), (d, d), w=W)
        tick(-d, -d, -0.7, 0.7)
        tick(d, d, 0.7, -0.7)
    if dim:
        s.arrow((-2.2, 1.6), (2.4, -2.4), size=1.0)


def socket_sign(s, protective=False, count=1):
    s.arc(0, 0, 4.0, 0, 180, w=W).line((-2.0, 0), (2.0, 0), w=W)
    s.line((0, -2.0), (0, -3.6), w=W)
    if protective:
        s.line((-1.4, -2.0 - 0.0), (1.4, -2.0), w=W)
    if count > 1:
        s.text(str(count), 2.4, -2.4, 2.0)


add(plan(C('Leuchte', 'Luminaire', 'Luminaire'), lambda s: s.circle(0, 0, 4.0, w=W).line((-1.41, -1.41), (1.41, 1.41), w=W).line((-1.41, 1.41), (1.41, -1.41), w=W)))
add(plan(C('Leuchtstoffleuchte', 'Fluorescent luminaire', 'Luminaire fluorescent'), lambda s: s.line((-5.0, 0), (5.0, 0), w=THICK).line((-5.0, -1.0), (-5.0, 1.0), w=W).line((5.0, -1.0), (5.0, 1.0), w=W)))
add(plan(C('Ausschalter', 'One-way switch', 'Interrupteur simple'), lambda s: switch_sign(s)))
add(plan(C('Ausschalter zweipolig', 'One-way switch, two-pole', 'Interrupteur bipolaire'), lambda s: switch_sign(s, strokes=2)))
add(plan(C('Wechselschalter', 'Two-way switch', 'Interrupteur va-et-vient'), lambda s: switch_sign(s, through=True)))
add(plan(C('Kreuzschalter', 'Intermediate switch', 'Permutateur'), lambda s: switch_sign(s, crossed=True)))
add(plan(C('Taster', 'Push button', 'Bouton-poussoir'), lambda s: s.circle(0, 0, 3.0, w=W).circle(0, 0, 1.4, w=W)))
add(plan(C('Dimmer', 'Dimmer', 'Variateur'), lambda s: switch_sign(s, dim=True)))
add(plan(C('Steckdose', 'Socket outlet', 'Prise de courant'), lambda s: socket_sign(s)))
add(plan(C('Schutzkontaktsteckdose', 'Socket outlet with protective contact', 'Prise de courant avec terre'), lambda s: socket_sign(s, protective=True)))
add(plan(C('Schutzkontaktsteckdose zweifach', 'Double socket outlet with protective contact', 'Prise double avec terre'), lambda s: socket_sign(s, protective=True, count=2)))
add(plan(C('Verteiler', 'Distribution board', 'Tableau de distribution'), lambda s: s.rect(-4.0, -2.0, 4.0, 2.0, w=W).poly((-4.0, 2.0), (4.0, -2.0), (4.0, 2.0), fill='#000000', w=W)))

add = page(EI, 'Klemmen', 'Terminals', 'Bornes')
s = Symbol('X', C('Klemme', 'Terminal', 'Borne'))
add(s.circle(0, 0, 1.6).pin('1', (0, 0)).labels((1.4, -3.4), (1.4, 0.8)))
s = Symbol('X', C('Trennklemme', 'Disconnect terminal', 'Borne sectionnable'))
add(s.circle(0, 0, 1.6).circle(0, P, 1.6).line((-1.2, P / 2 + 0.6), (1.2, P / 2 - 0.6)).pin('1', (0, 0)).pin('2', (0, P), label=(0.4, P - 2.2)).labels((2.0, -3.4), (2.0, P + 0.6)))
for n in (2, 4, 6):
    s = Symbol('X', C(f'Klemmleiste {n}-polig', f'Terminal strip, {n} poles', f'Bornier, {n} pôles'))
    for k in range(n):
        x = k * P
        s.rect(x - P / 2, -P, x + P / 2, P).circle(x, 0, 1.4).text(str(k + 1), x, P + 0.4, 1.8, 'centre')
        s.pin(str(k + 1), (x, 0))
    add(s.labels((-P / 2, -P - 3.4), (-P / 2 + 0.0, P + 3.0)))
add = page(EI, 'Motoren', 'Motors', 'Moteurs')
for text, de, en, fr in (('M', 'Motor', 'Motor', 'Moteur'), ('M\n=', 'Gleichstrommotor', 'DC motor', 'Moteur à courant continu'),
                         ('M\n~', 'Wechselstrommotor', 'AC motor', 'Moteur à courant alternatif'),
                         ('M\n3~', 'Drehstrommotor', 'Three-phase motor', 'Moteur triphasé'), ('G', 'Generator', 'Generator', 'Générateur')):
    if text == 'M\n3~':
        s = Symbol('M', C(de, en, fr))
        s.circle(2 * P, 3 * P, 4 * P - 0.6)
        for k in range(3):
            x = (k + 1) * P
            s.line((x, 0), (x, 3 * P - math.sqrt(max(0.0, (2 * P - 0.3) ** 2 - (x - 2 * P) ** 2))))
            s.pin(['U', 'V', 'W'][k], (x, 0), label=(x + 0.3, -2.4), shown=True)
        s.text('M', 2 * P, 3 * P - 3.4, 3.0, 'centre').text('3~', 2 * P, 3 * P + 0.2, 2.5, 'centre')
        add(s.labels((4 * P + 0.6, 2 * P), (4 * P + 0.6, 3 * P + 0.4)))
        continue
    s = Symbol('M' if text != 'G' else 'G', C(de, en, fr))
    s.line((0, 0), (0, P)).circle(0, 2 * P, 2 * P - 0.6).line((0, 3 * P), (0, 4 * P))
    lines = text.split('\n')
    if len(lines) == 1:
        s.text(lines[0], 0, 2 * P - 1.6, 3.0, 'centre')
    else:
        s.text(lines[0], 0, 2 * P - 3.0, 2.6, 'centre').text(lines[1], 0, 2 * P - 0.2, 2.4, 'centre')
    add(s.pin('1', (0, 0), label=(0.6, 0.2)).pin('2', (0, 4 * P), label=(0.6, 4 * P - 2.4)).labels((P + 1.2, P - 0.4), (P + 1.2, 2 * P + 0.6)))
add = page(EI, 'Elektroinstallation', 'Electrical installation', 'Installation électrique')
s = Symbol('X', C('Steckdose', 'Socket outlet', 'Prise de courant'))
add(s.line((0, 0), (0, P)).arc(0, P + 2.6, 5.2, 0, 180).line((-2.6, P + 2.6), (2.6, P + 2.6)).pin('1', (0, 0)).labels((3.4, 0), (3.4, 3.0)))
s = Symbol('X', C('Schutzkontaktsteckdose', 'Socket outlet with earth', 'Prise avec terre'))
add(s.line((0, 0), (0, P)).arc(0, P + 2.6, 5.2, 0, 180).line((-2.6, P + 2.6), (2.6, P + 2.6)).line((-2.6, P), (2.6, P)).pin('1', (0, 0)).labels((3.4, 0), (3.4, 3.0)))
s = Symbol('P', C('Leuchte', 'Luminaire', 'Luminaire'))
add(s.line((0, 0), (0, P + 2.0)).line(rot((-2.0, P + 2.0), (0, P + 2.0), 45), rot((2.0, P + 2.0), (0, P + 2.0), 45)).line(rot((-2.0, P + 2.0), (0, P + 2.0), -45), rot((2.0, P + 2.0), (0, P + 2.0), -45)).pin('1', (0, 0)).labels((2.8, 0), (2.8, 3.0)))
s = Symbol('S', C('Ausschalter', 'One-way switch', 'Interrupteur simple'))
add(s.circle(0, 0, 1.6).line(rot((0.8, 0), (0, 0), 45), rot((4.6, 0), (0, 0), 45)).line(rot((4.6, 0), (0, 0), 45), rot((4.6, 1.2), (0, 0), 45)).pin('1', (0, 0)).labels((2.4, 0.4), (2.4, 3.2)))
s = Symbol('S', C('Wechselschalter', 'Two-way switch', 'Interrupteur va-et-vient'))
add(s.circle(0, 0, 1.6).line(rot((0.8, 0), (0, 0), 45), rot((4.6, 0), (0, 0), 45)).line(rot((4.6, 0), (0, 0), 45), rot((4.6, 1.2), (0, 0), 45)).line(rot((-0.8, 0), (0, 0), 45), rot((-4.6, 0), (0, 0), 45)).line(rot((-4.6, 0), (0, 0), 45), rot((-4.6, -1.2), (0, 0), 45)).pin('1', (0, 0)).labels((2.4, 0.4), (2.4, 3.2)))
s = Symbol('S', C('Taster (Installation)', 'Push button (installation)', 'Bouton-poussoir (installation)'))
add(s.circle(0, 0, 2.4).circle(0, 0, 1.0, fill='#000000').pin('1', (0, 0)).labels((2.4, 0.4), (2.4, 3.2)))

DG = 'Elektro/Elektronik/Digital'
add = page(DG, 'Gatter', 'Gates', 'Portes')


def named(s, letters):
    """A gate of the gate pages: its function as designator letters (AND1, NOR2, ...)."""
    s.prefix = letters
    return s


for sign, neg, de, en, fr in (('&', False, 'UND', 'AND', 'ET'), ('&', True, 'NAND', 'NAND', 'NON-ET'),
                              ('≥1', False, 'ODER', 'OR', 'OU'), ('≥1', True, 'NOR', 'NOR', 'NON-OU'),
                              ('=1', False, 'Exklusiv-ODER', 'XOR', 'OU exclusif'), ('=1', True, 'Exklusiv-NOR', 'XNOR', 'NON-OU exclusif')):
    add(named(gate_iec(sign, 2, neg, C(de, en, fr)), en))
add(named(gate_iec('1', 1, True, C('Inverter', 'Inverter', 'Inverseur')), 'INV'))
add(named(gate_iec('1', 1, False, C('Treiber', 'Buffer', 'Tampon')), 'BUF'))
add(named(gate_iec('&', 3, False, C('UND (3 Eingänge)', 'AND (3 inputs)', 'ET (3 entrées)')), 'AND'))
add(named(gate_iec('&', 4, True, C('NAND (4 Eingänge)', 'NAND (4 inputs)', 'NON-ET (4 entrées)')), 'NAND'))
s = named(gate_iec('1', 1, True, C('Schmitt-Trigger-Inverter', 'Schmitt trigger inverter', 'Inverseur trigger de Schmitt')), 'STR')
s.parts[1]['pos'] = pt((2 * P - 1.6, 0.8))
add(s.line((2 * P - 0.4, 2 * P - 1.2), (2 * P + 0.6, 2 * P - 1.2), (2 * P + 0.6, 2 * P - 2.4), (2 * P + 1.6, 2 * P - 2.4)).line((2 * P, 2 * P - 1.2), (2 * P, 2 * P - 2.4), (2 * P + 1.0, 2 * P - 2.4)))
add = page(DG, 'Gatter (USA)', 'Gates (USA)', 'Portes (USA)')
for kind, neg, de, en, fr in (('and', False, 'UND', 'AND', 'ET'), ('and', True, 'NAND', 'NAND', 'NON-ET'),
                              ('or', False, 'ODER', 'OR', 'OU'), ('or', True, 'NOR', 'NOR', 'NON-OU'),
                              ('xor', False, 'Exklusiv-ODER', 'XOR', 'OU exclusif'), ('xor', True, 'Exklusiv-NOR', 'XNOR', 'NON-OU exclusif'),
                              ('buf', True, 'Inverter', 'Inverter', 'Inverseur'), ('buf', False, 'Treiber', 'Buffer', 'Tampon')):
    add(named(gate_us(kind, neg, C(de, en, fr)), {'Inverter': 'INV', 'Buffer': 'BUF'}.get(en, en)))
add = page(DG, 'Digital', 'Digital', 'Numérique')


def pulse(s, x, y, w=2.0, h=1.2):
    """The sign of a single pulse (a monoflop) or, repeated, of a clock."""
    s.line((x, y), (x + w * 0.25, y), (x + w * 0.25, y - h), (x + w * 0.75, y - h), (x + w * 0.75, y), (x + w, y), w=0.18)


mono = box('MF', C('Monoflop', 'Monostable multivibrator', 'Monostable'), ['', 'A', 'B', '~CLR'], ['', 'Q', '', '~Q'], width=4 * P)
mono.text('1', 2 * P, 0.6, 2.5)
pulse(mono, 2 * P + 1.8, 2.9)
add(mono)
clock = box('G', C('Taktgeber', 'Clock generator', "Générateur d'horloge"), ['', 'EN'], ['', 'Q'], width=4 * P)
clock.text('G', 2 * P - 1.0, 0.6, 2.5)
for k in range(2):
    pulse(clock, 2 * P + 0.8 + k * 2.0, 2.9)
add(clock)
s3 = box('IC', C('Treiber mit Tristate', 'Tri-state driver', 'Tampon trois états'), ['', 'A', 'EN'], ['', 'Y', ''], width=3 * P, label='1')
s3.poly((4 * P - 1.9, 3 * P - 0.6), (4 * P - 0.5, 3 * P - 0.6), (4 * P - 1.2, 3 * P + 0.6), fill=None, w=0.18)
add(s3)
oc = gate_iec('1', 1, True, C('Inverter mit offenem Kollektor', 'Open-collector inverter', 'Inverseur à collecteur ouvert'))
oc.poly((3 * P - 1.2, 2.6), (3 * P - 0.5, 3.3), (3 * P - 1.2, 4.0), (3 * P - 1.9, 3.3), fill=None, w=0.18).line((3 * P - 1.9, 4.3), (3 * P - 0.5, 4.3), w=0.18)
add(oc)
add(box('IC', C('Vergleicher 4 Bit', '4-bit comparator', 'Comparateur 4 bits'), ['A0', 'A1', 'A2', 'A3', 'B0', 'B1', 'B2', 'B3'],
        ['', '', 'A>B', 'A=B', 'A<B', '', '', ''], width=5 * P, label='COMP'))


def seven_segment(caption):
    """A seven-segment display with a common cathode: the segment inputs a to g and dp on the left, K below."""
    s = box('7SEG', caption, ['a', 'b', 'c', 'd', 'e', 'f', 'g', 'dp'], [], width=6 * P)
    x, y, w, h = 4 * P, 2.0 * P, 2.2 * P, 2.0 * P
    corners = {'a': ((0, 0), (1, 0)), 'b': ((1, 0), (1, 1)), 'c': ((1, 1), (1, 2)), 'd': ((0, 2), (1, 2)),
               'e': ((0, 1), (0, 2)), 'f': ((0, 0), (0, 1)), 'g': ((0, 1), (1, 1))}
    for (ax, ay), (bx, by) in corners.values():
        s.line((x + ax * w, y + ay * h), (x + bx * w, y + by * h), w=THICK)
    s.circle(x + w + 1.0, y + 2 * h, 0.8, fill='#000000')
    bottom = 9 * P
    s.line((4 * P, bottom), (4 * P, bottom + P)).pin('K', (4 * P, bottom + P), label=(4 * P + 0.5, bottom + P - 2.0), shown=True)
    return s


add(seven_segment(C('7-Segment-Anzeige (gemeinsame Kathode)', 'Seven-segment display (common cathode)', 'Afficheur 7 segments (cathode commune)')))

add = page(DG, 'Flipflops', 'Flip-flops', 'Bascules')
add(box('RS-FF', C('RS-Flipflop', 'SR flip-flop', 'Bascule RS'), ['S', 'R'], ['Q', '~Q'], width=3 * P))
add(box('D-FF', C('D-Flipflop', 'D flip-flop', 'Bascule D'), ['D', 'C'], ['Q', '~Q'], width=3 * P, clock=('C',)))
add(box('JK-FF', C('JK-Flipflop', 'JK flip-flop', 'Bascule JK'), ['J', 'C', 'K'], ['Q', '', '~Q'], width=3 * P, clock=('C',)))
add(box('T-FF', C('T-Flipflop', 'T flip-flop', 'Bascule T'), ['T', 'C'], ['Q', '~Q'], width=3 * P, clock=('C',)))
add(box('D-FF', C('D-Flipflop mit Setzen und Rücksetzen', 'D flip-flop with set and reset', 'Bascule D avec mise à 1 et à 0'), ['~S', 'D', 'C', '~R'], ['Q', '', '', '~Q'], width=3 * P, clock=('C',)))
add = page(DG, 'Zähler', 'Counters', 'Compteurs')
add(box('CNTR', C('Binärzähler 4 Bit', '4-bit binary counter', 'Compteur binaire 4 bits'), ['C', 'R', '', ''], ['Q0', 'Q1', 'Q2', 'Q3'], label='CTR4', clock=('C',)))
add(box('CNTR', C('Dezimalzähler', 'Decade counter', 'Compteur décimal'), ['C', 'R', 'EN', ''], ['Q0', 'Q1', 'Q2', 'Q3'], label='CTR10', clock=('C',)))
add = page(DG, 'Register', 'Registers', 'Registres')
add(box('SHIFT', C('Schieberegister 4 Bit', '4-bit shift register', 'Registre à décalage 4 bits'), ['SER', 'C', '~R', ''], ['Q0', 'Q1', 'Q2', 'Q3'], label='SRG4', clock=('C',)))
add(box('REG', C('Latch 4 Bit', '4-bit latch', 'Verrou 4 bits'), ['D0', 'D1', 'D2', 'D3', 'LE'], ['Q0', 'Q1', 'Q2', 'Q3', ''], label='LATCH'))
add = page(DG, 'MUX', 'MUX', 'MUX')
add(box('MUX', C('Multiplexer 4:1', '4:1 multiplexer', 'Multiplexeur 4:1'), ['D0', 'D1', 'D2', 'D3', 'S0', 'S1'], ['', '', 'Y', '', '', ''], label='MUX'))
add(box('DMUX', C('Demultiplexer 1:4', '1:4 demultiplexer', 'Démultiplexeur 1:4'), ['D', '', 'S0', 'S1'], ['Y0', 'Y1', 'Y2', 'Y3'], label='DMUX'))

SO = 'Sonstiges'
add = page(SO, 'Flowchart', 'Flowchart', 'Organigramme')


def flow(caption, draw):
    s = Symbol('', caption, numbered=False, listed=False, shown=False)
    draw(s)
    return s


add(flow(C('Start / Ende', 'Start / end', 'Début / fin'), lambda s: s.rect(0, 0, 8 * P, 3 * P, corner=50)))
add(flow(C('Verarbeitung', 'Process', 'Traitement'), lambda s: s.rect(0, 0, 8 * P, 4 * P)))
add(flow(C('Verzweigung', 'Decision', 'Décision'), lambda s: s.poly((0, 2 * P), (4 * P, 0), (8 * P, 2 * P), (4 * P, 4 * P), fill=None)))
add(flow(C('Ein- / Ausgabe', 'Input / output', 'Entrée / sortie'), lambda s: s.poly((P, 0), (8 * P, 0), (7 * P, 4 * P), (0, 4 * P), fill=None)))
add(flow(C('Unterprogramm', 'Subroutine', 'Sous-programme'), lambda s: s.rect(0, 0, 8 * P, 4 * P).line((P, 0), (P, 4 * P)).line((7 * P, 0), (7 * P, 4 * P))))
add(flow(C('Verbinder', 'Connector', 'Connecteur'), lambda s: s.circle(0, 0, 2 * P)))
add(flow(C('Dokument', 'Document', 'Document'), lambda s: s.line((0, 3.5 * P), (0, 0), (8 * P, 0), (8 * P, 3.5 * P)).bezier((0, 3.5 * P), (2.6 * P, 5.0 * P), (5.4 * P, 2.0 * P), (8 * P, 3.5 * P))))
add(flow(C('Daten', 'Data', 'Données'), lambda s: s.line((0, 0.6 * P), (0, 3.4 * P)).line((6 * P, 0.6 * P), (6 * P, 3.4 * P)).arc(3 * P, 0.6 * P, 6 * P, 0, 360, dy=1.2 * P).arc(3 * P, 3.4 * P, 6 * P, 180, 360, dy=1.2 * P)))
add(flow(C('Ablaufpfeil', 'Flow arrow', 'Flèche de flux'), lambda s: s.arrow((0, 0), (0, 4 * P), size=1.6)))


# --- More pages: packages, transmission paths, valves, converters, DIL, operating devices, vehicles, title fields
EL_E = 'Elektro/Elektronik'
add = page(EL_E, 'Transistor-Gehäuse', 'Transistor packages', 'Boîtiers de transistors')


def package(caption, draw, pins):
    s = Symbol('', caption, numbered=False)
    draw(s)
    for name, at, label in pins:
        s.pin(name, at, label=label, shown=True)
    return s


def to92(s):
    s.arc(2 * P, 0, 4 * P, 0, 180, w=0.35).line((0, 0), (4 * P, 0), w=0.35)
    for x in (P, 2 * P, 3 * P):
        s.line((x, 0), (x, 2 * P))
    s.labels((4 * P + 1.0, -2 * P), (4 * P + 1.0, -2 * P + 3.0))


def to220(s):
    s.rect(0, -6 * P, 4 * P, -3 * P).circle(2 * P, -4.8 * P, 3.2).rect(0, -3 * P, 4 * P, 0, w=0.35)
    for x in (P, 2 * P, 3 * P):
        s.line((x, 0), (x, 2 * P))
    s.labels((4 * P + 1.0, -3 * P), (4 * P + 1.0, -3 * P + 3.0))


def sot23(s):
    s.rect(-0.6 * P, -1.6, 2.6 * P, 1.6, w=0.35)
    for x, y0, y1 in ((0, 1.6, 2 * P), (2 * P, 1.6, 2 * P), (P, -1.6, -2 * P)):
        s.line((x, y0), (x, y1))
    s.labels((2.6 * P + 1.0, -2.0), (2.6 * P + 1.0, 1.0))


add(package(C('TO-92 (E B C)', 'TO-92 (E B C)', 'TO-92 (E B C)'), to92,
            [('E', (P, 2 * P), (P - 1.0, 2 * P + 0.4)), ('B', (2 * P, 2 * P), (2 * P - 1.0, 2 * P + 0.4)), ('C', (3 * P, 2 * P), (3 * P - 1.0, 2 * P + 0.4))]))
add(package(C('TO-220 (B C E)', 'TO-220 (B C E)', 'TO-220 (B C E)'), to220,
            [('B', (P, 2 * P), (P - 1.0, 2 * P + 0.4)), ('C', (2 * P, 2 * P), (2 * P - 1.0, 2 * P + 0.4)), ('E', (3 * P, 2 * P), (3 * P - 1.0, 2 * P + 0.4))]))
add(package(C('TO-220 (G D S)', 'TO-220 (G D S)', 'TO-220 (G D S)'), to220,
            [('G', (P, 2 * P), (P - 1.0, 2 * P + 0.4)), ('D', (2 * P, 2 * P), (2 * P - 1.0, 2 * P + 0.4)), ('S', (3 * P, 2 * P), (3 * P - 1.0, 2 * P + 0.4))]))
add(package(C('SOT-23', 'SOT-23', 'SOT-23'), sot23,
            [('1', (0, 2 * P), (-1.0, 2 * P + 0.4)), ('2', (2 * P, 2 * P), (2 * P - 1.0, 2 * P + 0.4)), ('3', (P, -2 * P), (P + 0.6, -2 * P - 0.6))]))

add = page(EL_E, 'Pressverbinder', 'IDC connectors', 'Connecteurs autodénudants')


def box_header(n, caption, plug=True):
    """A two-row connector for ribbon cable seen from above: pin 1 marked, odd pins in the upper row and even ones below
    (as on the cable), the key in the middle of the upper side."""
    s = Symbol('X', caption)
    columns = n // 2
    x0, x1, y0, y1 = -P / 2 - 1.0, (columns - 0.5) * P + 1.0, -P / 2 - 1.2, P + P / 2 + 1.2
    s.rect(x0, y0, x1, y1, w=0.35)
    if plug:
        s.rect(x0 + 1.0, y0 + 1.0, x1 - 1.0, y1 - 1.0, w=0.18)
        mid = (columns - 1) * P / 2
        s.line((mid - 2.0, y0), (mid - 2.0, y0 + 1.0), (mid + 2.0, y0 + 1.0), (mid + 2.0, y0), w=0.35)
    else:
        s.line(((columns - 1) * P / 2 - 2.0, y0 - 0.8), ((columns - 1) * P / 2 + 2.0, y0 - 0.8), w=0.6)
    s.poly((x0 + 0.4, y1 + 0.3), (x0 + 1.6, y1 + 0.3), (x0 + 1.0, y1 + 1.4), fill='#000000', w=0.1)
    for k in range(n):
        x, y = (k // 2) * P, (P if k % 2 else 0)
        if k == 0:
            s.rect(x - 0.6, y - 0.6, x + 0.6, y + 0.6, w=0.18, fill='#000000')
        else:
            s.circle(x, y, 1.2, w=0.18, fill='#000000')
        s.pin(str(k + 1), (x, y))
    s.text('1', x0 + 2.2, y1 + 0.1, 1.6)
    s.labels((x1 + 1.0, y0), (x1 + 1.0, y0 + 3.0))
    return s


for n in (6, 10, 14, 16, 20, 26, 34, 40, 50):
    add(box_header(n, C(f'Wannenstecker {n}-polig', f'Box header, {n} pins', f'Connecteur mâle à détrompeur, {n} broches')))
for n in (10, 14, 16, 20, 26, 34, 40):
    add(box_header(n, C(f'Pfostenbuchse {n}-polig', f'IDC socket, {n} pins', f'Connecteur femelle autodénudant, {n} broches'), plug=False))

add = page(EL_E, 'Steckplatine', 'Breadboard', "Plaque d'essai")
HOLE = '#404040'


def board(caption, columns, rows, rails=False):
    """A piece of breadboard seen from above: holes on the 2.54 mm pitch, the first at the insertion point; supply rails
    with a red and a blue line. A drawing: what is plugged in connects by lying on its holes."""
    s = Symbol('', caption, numbered=False, listed=False, shown=False)
    s.rect(-P / 2, -P / 2, (columns - 0.5) * P, (rows - 0.5) * P, w=0.18, fill='#eeeeee')
    for c in range(columns):
        for k in range(rows):
            if rails and c % 6 == 5:
                continue
            s.rect(c * P - 0.4, k * P - 0.4, c * P + 0.4, k * P + 0.4, w=0.1, fill=HOLE)
    if rails:
        s.line((-P / 2 + 0.6, -P / 2 + 0.4), ((columns - 0.5) * P - 0.6, -P / 2 + 0.4), w=0.35)
        s.parts[-1]['pen']['color'] = '#d00000'
        s.line((-P / 2 + 0.6, (rows - 0.5) * P - 0.4), ((columns - 0.5) * P - 0.6, (rows - 0.5) * P - 0.4), w=0.35)
        s.parts[-1]['pen']['color'] = '#0040d0'
    return s


def plugged(prefix, caption, pins, draw, value='', **flags):
    """A part seen from above with its legs on holes of the pitch (`pins`: name and place)."""
    s = Symbol(prefix, caption, value, **flags)
    draw(s)
    for name, at in pins:
        s.pin(name, at)
    return s


def coloured(s, colour):
    s.parts[-1]['pen']['color'] = colour
    return s


def axial(s, length, body, bands=(), colour='#e8d0a0'):
    s.line((0, 0), (length * P, 0), w=0.5)
    coloured(s, '#808080')
    x0, x1 = (length * P - body) / 2, (length * P + body) / 2
    s.rect(x0, -1.0, x1, 1.0, w=0.18, fill=colour, corner=40)
    for k, colour in enumerate(bands):
        x = x0 + 0.9 + k * 1.0
        s.rect(x, -1.0, x + 0.5, 1.0, w=0.05, fill=colour)
    s.labels((0, -3.2), (length * P / 2 + 1.0, -3.2))


add(board(C('Versorgungsschiene', 'Supply rail', "Rail d'alimentation"), 30, 2, rails=True))
add(board(C('Steckfeld 5 × 30', 'Terminal strip 5 × 30', 'Bande de contacts 5 × 30'), 30, 5))
add(board(C('Steckfeld 5 × 10', 'Terminal strip 5 × 10', 'Bande de contacts 5 × 10'), 10, 5))
add(plugged('R', C('Widerstand (Steckplatine)', 'Resistor (breadboard)', "Résistance (plaque d'essai)"), [('1', (0, 0)), ('2', (4 * P, 0))],
            lambda s: axial(s, 4, 6.0, ('#7a4a00', '#000000', '#d00000', '#c8a000')), '1k'))
def diode_top(s):
    axial(s, 4, 4.0, colour='#202020')
    s.rect(2 * P + 1.0, -1.0, 2 * P + 1.6, 1.0, w=0.05, fill='#c0c0c0')     # the ring at the cathode
add(plugged('D', C('Diode (Steckplatine)', 'Diode (breadboard)', "Diode (plaque d'essai)"), [('A', (0, 0)), ('K', (4 * P, 0))], diode_top))
def led5(s):
    s.line((0, 0), (0, -1.5), w=0.5); coloured(s, '#808080')
    s.line((P, 0), (P, -1.5), w=0.5); coloured(s, '#808080')
    s.circle(P / 2, -1.5, 5.0, w=0.25, fill='#ff4040')
    s.line((P / 2 + 2.3, -2.6), (P / 2 + 2.3, -0.4), w=0.25)
    s.labels((P / 2 + 3.2, -3.4), (P / 2 + 3.2, -0.6))
add(plugged('LED', C('LED 5 mm (Steckplatine)', 'LED 5 mm (breadboard)', "DEL 5 mm (plaque d'essai)"), [('A', (0, 0)), ('K', (P, 0))], led5, 'rot'))
def elko(s):
    s.circle(P / 2, 0, 5.0, w=0.25, fill='#2050a0')
    s.rect(P / 2 + 1.3, -2.3, P / 2 + 2.3, 2.3, w=0.05, fill='#e0e0e0')
    s.circle(0, 0, 0.6, w=0.1, fill='#808080').circle(P, 0, 0.6, w=0.1, fill='#808080')
    s.labels((P / 2 + 3.2, -3.4), (P / 2 + 3.2, -0.6))
add(plugged('C', C('Elko (Steckplatine)', 'Electrolytic capacitor (breadboard)', "Condensateur électrolytique (plaque d'essai)"), [('+', (0, 0)), ('-', (P, 0))], elko, '10µ'))
def ceramic(s):
    s.line((0, 0), (0, -1.4), w=0.5); coloured(s, '#808080')
    s.line((P, 0), (P, -1.4), w=0.5); coloured(s, '#808080')
    s.parts.append({'type': 'ellipse', 'centre': pt((P / 2, -2.4)), 'size': [4.0, 2.6], 'pen': {'width': 0.25}, 'fill': {'style': 'solid', 'color': '#e0a040'}})
    s.labels((P / 2 + 2.6, -4.4), (P / 2 + 2.6, -1.4))
add(plugged('C', C('Keramikkondensator (Steckplatine)', 'Ceramic capacitor (breadboard)', "Condensateur céramique (plaque d'essai)"), [('1', (0, 0)), ('2', (P, 0))], ceramic, '100n'))
def to92top(s):
    # The flat side towards the viewer, the round side away.
    s.poly((P - 2.6, 0.4), (P + 2.6, 0.4), (P + 2.2, -1.2), (P, -2.2), (P - 2.2, -1.2), fill='#303030', w=0.1)
    s.arc(P, 0.4, 5.2, 0, 180, w=0.25).line((P - 2.6, 0.4), (P + 2.6, 0.4), w=0.25)
    s.labels((2 * P + 2.0, -3.4), (2 * P + 2.0, -0.6))
add(plugged('T', C('Transistor TO-92 (Steckplatine)', 'Transistor TO-92 (breadboard)', "Transistor TO-92 (plaque d'essai)"), [('E', (0, 0)), ('B', (P, 0)), ('C', (2 * P, 0))], to92top))
def button(s):
    s.rect(-0.6, -0.6, 2 * P + 0.6, 2 * P + 0.6, w=0.25, fill='#202020')
    s.circle(P, P, 3.2, w=0.25, fill='#808080')
    s.labels((2 * P + 1.4, -1.0), (2 * P + 1.4, 2.0))
add(plugged('S', C('Taster (Steckplatine)', 'Push button (breadboard)', "Bouton-poussoir (plaque d'essai)"), [('1', (0, 0)), ('2', (2 * P, 0)), ('3', (0, 2 * P)), ('4', (2 * P, 2 * P))], button))
for n, colour, de, en, fr in ((2, '#d00000', 'rot', 'red', 'rouge'), (3, '#0040d0', 'blau', 'blue', 'bleu'), (4, '#008000', 'grün', 'green', 'vert'), (6, '#e0a000', 'gelb', 'yellow', 'jaune')):
    def jumper(s, n=n, colour=colour):
        s.line((0, 0), (0, -1.2), (n * P, -1.2), (n * P, 0), w=0.6)
        coloured(s, colour)
    add(plugged('', C(f'Drahtbrücke {n} Raster ({de})', f'Wire jumper, {n} pitches ({en})', f'Pont de fil, {n} pas ({fr})'), [('1', (0, 0)), ('2', (n * P, 0))], jumper,
                numbered=False, listed=False, shown=False))

add = page(EL_E, 'Übertragungswege', 'Transmission paths', 'Voies de transmission')


def path(caption, draw, prefix='W'):
    s = Symbol(prefix, caption)
    s.pin('1', (0, 0)).pin('2', (8 * P, 0), label=(8 * P - 0.4, -2.2), align='right').line((0, 0), (8 * P, 0))
    draw(s)
    s.labels((4 * P, -6.0), (4 * P, 2.8), 'centre')
    return s


add(path(C('Leitung', 'Conductor', 'Conducteur'), lambda s: None))
add(path(C('Leitung, drei Leiter', 'Three conductors', 'Trois conducteurs'), lambda s: [s.line((3 * P + k * 1.2 - 0.8, 1.4), (3 * P + k * 1.2 + 0.8, -1.4)) for k in range(3)]))
add(path(C('Abgeschirmte Leitung', 'Screened conductor', 'Conducteur blindé'),
         lambda s: s.parts.append({'type': 'ellipse', 'centre': pt((4 * P, 0)), 'size': [r(4 * P), 3.0], 'pen': {'width': W, 'style': 'dash'}})))
add(path(C('Koaxialleitung', 'Coaxial cable', 'Câble coaxial'), lambda s: s.circle(4 * P, 0, 3.4).line((4 * P - 1.7, 2.6), (4 * P + 3.0, 2.6))))
add(path(C('Verdrillte Leitung', 'Twisted pair', 'Paire torsadée'),
         lambda s: [s.bezier((x, -1.2), (x + 1.0, -1.2), (x + 1.5, 1.2), (x + 2.5, 1.2)) for x in (2 * P, 2 * P + 2.5, 2 * P + 5.0)]))
add(path(C('Lichtwellenleiter', 'Optical fibre', 'Fibre optique'), lambda s: s.circle(4 * P, 0, 2.4).arrow((4 * P - 3.0, -2.6), (4 * P - 1.2, -1.0), size=0.9)))

add = page(BT + '/Alt', 'Roehren', 'Valves', 'Tubes')


def tube(caption, grids):
    """A valve after DIN EN 60617: anode at the top, cathode and heater below, control grid G1 to the left, the further
    grids to the right and left; every lead ends on the 2.54 mm pitch."""
    s = Symbol('V', caption)
    s.circle(0, 0, 6 * P, w=0.35)
    s.line((-1.8, -3.6), (1.8, -3.6), w=THICK).line((0, -3.6), (0, -3 * P)).pin('A', (0, -3 * P), label=(0.6, -3 * P - 0.4))
    s.line((-P, 3 * P), (-P, 1.8), (1.6, 1.8), (2.0, 2.3)).pin('K', (-P, 3 * P), label=(-P - 0.4, 3 * P - 2.4), align='right')
    s.line((0, 3 * P), (0, 3.8), (1.27, 3.0), (P, 3.8), (P, 3 * P)).pin('F1', (0, 3 * P), label=(0.2, 3 * P + 0.2)).pin('F2', (P, 3 * P), label=(P + 0.4, 3 * P + 0.2))
    dashed = lambda y: s.parts.append({'type': 'line', 'points': [pt((-2.0, y)), pt((2.0, y))], 'pen': {'width': W, 'style': 'dash'}, 'electrical': False})
    if grids >= 1:
        dashed(0)
        s.line((-2.0, 0), (-3 * P, 0)).pin('G1', (-3 * P, 0), label=(-3 * P - 0.4, -2.2), align='right')
    if grids >= 2:
        dashed(-1.27)
        s.line((2.0, -1.27), (2.8, -1.27), (2.8, -P), (3 * P, -P)).pin('G2', (3 * P, -P), label=(3 * P - 0.4, -P - 2.2), align='right')
    if grids >= 3:
        dashed(-P)
        s.line((-2.0, -P), (-3 * P, -P)).pin('G3', (-3 * P, -P), label=(-3 * P - 0.4, -P - 2.2), align='right')
    s.labels((3 * P + 1.0, 1.0), (3 * P + 1.0, 4.0))
    return s


add(tube(C('Diode (Röhre)', 'Diode valve', 'Tube diode'), 0))
add(tube(C('Triode', 'Triode', 'Triode'), 1))
add(tube(C('Tetrode', 'Tetrode', 'Tétrode'), 2))
add(tube(C('Pentode', 'Pentode', 'Pentode'), 3))

add = page(BT + '/Alt', 'Röhrenfassungen', 'Valve sockets', 'Supports de tubes')


def socket(caption, positions, pins, first, key=None):
    """A valve socket seen from below: `pins` of `positions` places on a circle, numbered clockwise from `first` (in
    places from the bottom), a gap where places stay free; a key in the middle or a pip outside. Each pin leads out
    over the rim to the nearest point of the 2.54 mm pitch."""
    s = Symbol('', caption, numbered=False)
    radius, outer = 4 * P, 6 * P
    s.circle(0, 0, 2 * outer, w=0.35)
    step = 360 / positions
    grid = lambda v: round(v / P) * P
    for k in range(pins):
        a = math.radians((first + k) * step)
        dx, dy = -math.sin(a), math.cos(a)
        x, y = radius * dx, radius * dy
        end = (grid((outer + P) * dx), grid((outer + P) * dy))
        s.circle(x, y, 2.0, w=W).line((x + dx, y + dy), end)
        s.pin(str(k + 1), end, label=(end[0] + 1.6 * dx, end[1] + 1.6 * dy - 0.9), align='centre', shown=True)
    if key == 'centre':
        s.circle(0, 0, 3 * P, w=W).line((-0.8, 3 * P / 2), (-0.8, 3 * P / 2 - 1.4), (0.8, 3 * P / 2 - 1.4), (0.8, 3 * P / 2))
    elif key == 'pip':
        s.poly((-0.9, outer), (0, outer + 1.4), (0.9, outer), fill='#000000')
    s.labels((outer + 2.0, -outer - 1.0), (outer + 2.0, -outer + 2.0))
    return s


add(socket(C('Noval-Fassung (B9A)', 'Noval socket (B9A)', 'Support noval (B9A)'), 10, 9, 1))
add(socket(C('Miniatur-Fassung 7 Stifte (B7G)', 'Miniature socket, 7 pins (B7G)', 'Support miniature 7 broches (B7G)'), 8, 7, 1))
add(socket(C('Oktal-Fassung', 'Octal socket', 'Support octal'), 8, 8, 0.5, key='centre'))
add(socket(C('Rimlock-Fassung (B8A)', 'Rimlock socket (B8A)', 'Support rimlock (B8A)'), 8, 8, 0.5, key='pip'))

add = page(DG, 'ADC', 'ADC', 'CAN')
add(box('ADC', C('A/D-Wandler 8 Bit', '8-bit A/D converter', 'Convertisseur A/N 8 bits'), ['AIN', 'VREF', 'CLK', '~CS', '', '', '', ''], ['D0', 'D1', 'D2', 'D3', 'D4', 'D5', 'D6', 'D7'], label='ADC', clock=('CLK',), width=5 * P))
add(box('ADC', C('A/D-Wandler seriell', 'Serial A/D converter', 'Convertisseur A/N série'), ['AIN', 'VREF', 'SCK', '~CS'], ['DOUT', '', '', ''], label='ADC', clock=('SCK',), width=5 * P))
add = page(DG, 'DAC', 'DAC', 'CNA')
add(box('DAC', C('D/A-Wandler 8 Bit', '8-bit D/A converter', 'Convertisseur N/A 8 bits'), ['D0', 'D1', 'D2', 'D3', 'D4', 'D5', 'D6', 'D7', 'VREF'], ['', '', '', 'AOUT', '', '', '', '', ''], label='DAC', width=5 * P))
add(box('DAC', C('D/A-Wandler seriell', 'Serial D/A converter', 'Convertisseur N/A série'), ['DIN', 'SCK', '~CS', 'VREF'], ['', 'AOUT', '', ''], label='DAC', clock=('SCK',), width=5 * P))

add = page(DG, 'DIL', 'DIL', 'DIL')


def dil(n):
    """A DIL package seen from above: pins 1 to n/2 down the left side, the others up the right side."""
    half = n // 2
    s = Symbol('K', C(f'DIL {n}', f'DIL {n}', f'DIL {n}'))
    x0, x1, h = P, 6 * P, (half + 1) * P
    s.rect(x0, 0, x1, h, w=0.35).arc((x0 + x1) / 2, 0, 2.4, 180, 360)
    for k in range(half):
        y = (k + 1) * P
        s.line((0, y), (x0, y)).pin(str(k + 1), (0, y), label=(x0 + 0.6, y - 1.0), shown=True)
        right = n - k
        s.line((x1, y), (x1 + P, y)).pin(str(right), (x1 + P, y), label=(x1 - 0.6, y - 1.0), align='right', shown=True)
    for o in s.parts:
        if o['type'] == 'contact':
            o['font'] = {'height': 1.8}
    s.labels((x0, -3.4), (x0, h + 0.6))
    return s


for n in (8, 14, 16, 18, 20, 24, 28, 40):
    add(dil(n))

add = page(EI, 'Antriebsarten', 'Operating devices', 'Dispositifs de commande')


def operating(caption, draw):
    s = Symbol('', caption, numbered=False, listed=False, shown=False)
    s.parts.append({'type': 'line', 'points': [pt((0, 0)), pt((3 * P, 0))], 'pen': {'width': W, 'style': 'dash'}, 'electrical': False})
    draw(s)
    return s


add(operating(C('Handbetätigung', 'Manual operation', 'Commande manuelle'), lambda s: s.line((0, -1.2), (0, 1.2))))
add(operating(C('Drücken', 'Push', 'Pousser'), lambda s: s.line((0.8, -1.2), (0, -1.2), (0, 1.2), (0.8, 1.2))))
add(operating(C('Ziehen', 'Pull', 'Tirer'), lambda s: s.line((-0.8, -1.2), (0, -1.2), (0, 1.2), (-0.8, 1.2))))
add(operating(C('Drehen', 'Turn', 'Tourner'), lambda s: s.line((0.8, -1.2), (0, -1.2), (0, 1.2), (-0.8, 1.2))))
add(operating(C('Kippen', 'Toggle', 'Basculer'), lambda s: s.line((-0.8, -1.2), (0, 0), (-0.8, 1.2))))
add(operating(C('Fußbetätigung', 'Foot operation', 'Commande au pied'), lambda s: s.line((-0.6, -1.4), (0, -1.4), (0, 1.4), (-1.6, 1.4))))
add(operating(C('Rollenbetätigung', 'Roller', 'Galet'), lambda s: s.circle(-0.9, 0, 1.8)))
add(operating(C('Nockenbetätigung', 'Cam', 'Came'), lambda s: s.line((0, -1.4), (-1.6, 0), (0, 1.4))))
add(operating(C('Motorantrieb', 'Motor drive', 'Entraînement par moteur'), lambda s: s.circle(-1.6, 0, 3.2).text('M', -1.6, -1.2, 2.2, 'centre')))
add(operating(C('Elektromagnetisch', 'Electromagnetic', 'Électromagnétique'), lambda s: s.rect(-2.4, -1.4, 0, 1.4).line((-2.4, 1.4), (0, -1.4))))
add(operating(C('Thermisch', 'Thermal', 'Thermique'), lambda s: s.line((0, -1.2), (-0.8, -1.2), (-0.8, 1.2), (-1.6, 1.2))))
add(operating(C('Not-Aus (Pilzdrucktaster)', 'Emergency stop (mushroom)', 'Arrêt d’urgence (coup de poing)'), lambda s: s.arc(-0.2, 0, 3.0, 90, 270, w=0.35).line((-0.2, -1.5), (-0.2, 1.5))))
add(operating(C('Schlüsselbetätigung', 'Key operation', 'Commande à clé'), lambda s: s.circle(-2.2, 0, 1.6).line((-1.4, 0), (0, 0)).line((-0.6, 0), (-0.6, 0.8))))

KFZ = 'Elektro'
add = page(KFZ, 'Kfz', 'Motor vehicle', 'Véhicule automobile')
s = two_pin('G', C('Lichtmaschine', 'Alternator', 'Alternateur'))
s.line((0, 0), (P + 0.3, 0)).circle(2 * P, 0, 2 * P - 0.6).line((3 * P - 0.3, 0), (4 * P, 0)).text('G', 2 * P, -2.6, 2.6, 'centre').text('3~', 2 * P, 0.0, 2.0, 'centre')
add(s)
s = two_pin('M', C('Anlasser', 'Starter motor', 'Démarreur'))
s.line((0, 0), (P + 0.3, 0)).circle(2 * P, 0, 2 * P - 0.6).line((3 * P - 0.3, 0), (4 * P, 0)).text('M', 2 * P, -1.6, 3.0, 'centre')
add(s.rect(2 * P - 1.0, 2.6, 2 * P + 1.0, 3.4, fill='#000000'))
s = Symbol('E', C('Zündkerze', 'Spark plug', 'Bougie d’allumage'))
s.pin('1', (0, 0)).line((0, 0), (0, P)).rect(-1.2, P, 1.2, 2 * P).line((0, 2 * P), (0, 2 * P + 1.2))
s.line((-1.2, 2 * P + 2.0), (1.2, 2 * P + 2.0), w=0.35).line((1.2, 2 * P + 2.0), (1.2, 3 * P))
add(s.labels((2.0, 0), (2.0, 3.0)))
s = Symbol('', C('Zündspule', 'Ignition coil', 'Bobine d’allumage'))
transformer(s)
add(s)
s = two_pin('H', C('Glühlampe (Kfz)', 'Bulb (vehicle)', 'Ampoule (véhicule)'))
add(s.line((0, 0), (P + 0.3, 0)).circle(2 * P, 0, 2 * P - 0.6).line((3 * P - 0.3, 0), (4 * P, 0)).line(rot((2 * P - 3.3, 0), (2 * P, 0), 45), rot((2 * P + 3.3, 0), (2 * P, 0), 45)).line(rot((2 * P - 3.3, 0), (2 * P, 0), -45), rot((2 * P + 3.3, 0), (2 * P, 0), -45)))
add(ground('chassis', C('Fahrzeugmasse', 'Vehicle chassis', 'Masse du véhicule')))
s = two_pin('S', C('Zündschloss', 'Ignition switch', 'Contacteur d’allumage'))
s.line((0, 0), (P, 0)).line((3 * P, 0), (4 * P, 0)).line((P, 0), (3 * P - 0.4, -1.6)).circle(2 * P, -3.4, 1.6).line((2 * P, -2.6), (2 * P, -1.0))
add(s)

add = page(SO, 'Schriftfelder', 'Title blocks', 'Blocs de titre')


def title_field(caption, w, rows):
    s = Symbol('', caption, numbered=False, listed=False, shown=False)
    h = len(rows) * 2 * P
    s.rect(0, 0, w, h, w=0.35)
    for k, (label, value) in enumerate(rows):
        y = k * 2 * P
        if k:
            s.line((0, y), (w, y))
        s.line((22, y), (22, y + 2 * P))
        s.text(label, 1.0, y + 1.0, 2.0).text(value, 23.0, y + 0.8, 2.5)
    return s


add(title_field(C('Schriftfeld klein', 'Small title block', 'Petit cartouche'), 80,
                [('Blatt', '<PAGENAME>'), ('Datei', '<FILENAME>'), ('Seite', '<PAGENO> / <PAGECOUNT>')]))
add(title_field(C('Schriftfeld groß', 'Large title block', 'Grand cartouche'), 120,
                [('Projekt', '<FILENAME_PURE>'), ('Blatt', '<PAGENAME>'), ('Datum', '<FILEDATE>'), ('Bearbeiter', ''), ('Seite', '<PAGENO> / <PAGECOUNT>')]))
add(title_field(C('Änderungsfeld', 'Revision block', 'Bloc de révision'), 120,
                [('Änderung', ''), ('Datum', ''), ('Name', '')]))


# --- 74xx in the order of their pins
def ic74(number, function, pins):
    """A 74xx IC as a box in the order of its DIL pins: 1 to n/2 down the left side, the rest up the right side; the
    pin numbers outside, the names inside; active-low pins ("~") with a negation circle."""
    n = len(pins)
    half = n // 2
    s = Symbol('IC', function, value=number)
    x0, x1, h = P, 9 * P, (half + 1) * P
    s.rect(x0, 0, x1, h, w=0.35)
    for k in range(n):
        left = k < half
        y = (k + 1) * P if left else (n - k) * P
        name = pins[k]
        neg = name.startswith('~')
        shown = name.lstrip('~')
        if left:
            if neg:
                s.circle(x0 - 0.6, y, 1.2).line((0, y), (x0 - 1.2, y))
            else:
                s.line((0, y), (x0, y))
            s.text(shown, x0 + 0.6, y - 1.0, 1.8)
            s.pin(str(k + 1), (0, y), label=(0.2, y - 2.2), shown=True)
        else:
            if neg:
                s.circle(x1 + 0.6, y, 1.2).line((x1 + 1.2, y), (x1 + P, y))
            else:
                s.line((x1, y), (x1 + P, y))
            s.text(shown, x1 - 0.6, y - 1.0, 1.8, 'right')
            s.pin(str(k + 1), (x1 + P, y), label=(x1 + P - 0.2, y - 2.2), align='right', shown=True)
    for o in s.parts:
        if o['type'] == 'contact':
            o['font'] = {'height': 1.6}
    s.labels((x0, -3.4), (x1, -3.4), 'left', 'right')
    return s


import ttl74   # noqa: E402  the pin lists of the 74xx ICs, one per IC (libraries/schematic/ttl74.py)

add = page(DG + '/74xx', 'TTL-74xx', 'TTL 74xx', 'TTL 74xx')
for chip in ttl74.CHIPS:
    add(ic74(chip.number, C(f'{chip.number} {chip.de}', f'{chip.number} {chip.en}', f'{chip.number} {chip.fr}'), chip.pins))


# The 74xx as children of a parent that carries the supply, kept with the parent's entry as in sPlan's library and
# placed with it, linked; each gate or function block shows the pin numbers of its place in the package.
add = page(DG + '/74xx', 'TTL-74xx (Parent/Child)', 'TTL 74xx (parent/child)', 'TTL 74xx (parent/enfant)')


def supply_parent(number, caption, pins):
    s = Symbol('IC', caption, value=number)
    s.parent = True
    vcc, gnd = pins.index('VCC') + 1, pins.index('GND') + 1
    s.rect(P, 0, 5 * P, 3 * P, w=0.35).text(number, 3 * P, 0.8, 2.5, 'centre')
    s.line((3 * P, 0), (3 * P, -P)).pin(str(vcc), (3 * P, -P), label=(3 * P + 0.5, -P - 2.0), shown=True)
    s.line((3 * P, 3 * P), (3 * P, 4 * P)).pin(str(gnd), (3 * P, 4 * P), label=(3 * P + 0.5, 4 * P - 2.0), shown=True)
    s.text('VCC', 3 * P, 1.6 * P - 1.0, 1.8, 'centre').text('GND', 3 * P, 2.4 * P - 0.4, 1.8, 'centre')
    s.labels((5 * P + 1.0, 0.4), (5 * P + 1.0, 3.4))
    return s


def output_mark(s, mark, x, y):
    """The qualifying symbols of IEC 60617-12 inside a gate near its output, small: open collector (a diamond with a
    bar below), buffer (a triangle), three-state (a triangle pointing down), Schmitt trigger (the hysteresis)."""
    if mark in ('oc', 'ocbuf'):
        d = 0.7
        s.line((x, y - d), (x + d, y), (x, y + d), (x - d, y), (x, y - d), w=0.18).line((x - d, y + d + 0.3), (x + d, y + d + 0.3), w=0.18)
        x -= 2.0
    if mark in ('buf', 'ocbuf'):
        s.line((x - 0.6, y - 0.8), (x + 0.6, y), (x - 0.6, y + 0.8), (x - 0.6, y - 0.8), w=0.18)
    if mark == 'tri':
        s.line((x - 0.8, y - 0.6), (x + 0.8, y - 0.6), (x, y + 0.6), (x - 0.8, y - 0.6), w=0.18)
    if mark == 'schmitt':
        x0, y0 = 2 * P + 0.6, 3.4
        s.line((x0 - 0.2, y0), (x0 + 0.5, y0), (x0 + 0.5, y0 - 0.8), (x0 + 0.9, y0 - 0.8), w=0.18).line((x0 + 0.1, y0), (x0 + 0.1, y0 - 0.8), (x0 + 1.1, y0 - 0.8), w=0.18)
    return s


CHILD = '<PARENT_ID>-<CHILDNO>'   # as in the reference: once linked, the parent's designator and the child's number


def child_gate(sign, negated, inputs, numbers, caption, mark=None):
    s = gate_iec(sign, inputs, negated, caption)
    s.prefix, s.numbered = CHILD, False
    # The contacts named and labelled as the package's pins.
    contacts = [o for o in s.parts if o['type'] == 'contact']
    for o, n in zip(contacts, numbers):
        o['name'] = o['text'] = str(n)
        o['visible'] = True
        o['font'] = {'height': 1.6}
    if mark:
        output_mark(s, mark, 3 * P - 1.3, contacts[-1]['pin'][1])
    return s


def child_block(caption, inputs, outputs, number):
    """A function block as a child: the inputs down the left side, the outputs down the right side, each with its pin
    number outside and its name inside; active-low pins ("~") with a negation circle."""
    rows = max(len(inputs), len(outputs))
    s = Symbol(CHILD, caption, numbered=False)
    x0, x1, h = P, 9 * P, (rows + 1) * P
    s.rect(x0, 0, x1, h, w=0.35)
    for k, name in enumerate(inputs):
        y = (k + 1) * P
        shown = name.lstrip('~')
        if name.startswith('~'):
            s.circle(x0 - 0.6, y, 1.2).line((0, y), (x0 - 1.2, y))
        else:
            s.line((0, y), (x0, y))
        s.text(shown, x0 + 0.6, y - 1.0, 1.8)
        s.pin(str(number[name]), (0, y), label=(0.2, y - 2.2), shown=True)
    for k, name in enumerate(outputs):
        y = (k + 1) * P
        shown = name.lstrip('~')
        if name.startswith('~'):
            s.circle(x1 + 0.6, y, 1.2).line((x1 + 1.2, y), (x1 + P, y))
        else:
            s.line((x1, y), (x1 + P, y))
        s.text(shown, x1 - 0.6, y - 1.0, 1.8, 'right')
        s.pin(str(number[name]), (x1 + P, y), label=(x1 + P - 0.2, y - 2.2), align='right', shown=True)
    for o in s.parts:
        if o['type'] == 'contact':
            o['font'] = {'height': 1.6}
    s.labels((x0, -3.4), (x1, -3.4), 'left', 'right')
    return s


for chip in ttl74.CHIPS:
    number = {name: k + 1 for k, name in enumerate(chip.pins)}
    parent = add(supply_parent(chip.number, C(f'{chip.number} Versorgung (Parent)', f'{chip.number} supply (parent)', f'{chip.number} alimentation (parent)'), chip.pins))
    if isinstance(chip.children, ttl74.Gates):
        gates = chip.children
        units = gates.units()
        for u, (ins, out) in enumerate(units, 1):
            k = f' {u}' if len(units) > 1 else ''
            parent.attach(child_gate(gates.sign, gates.negated, gates.inputs, [number[n] for n in ins] + [number[out]],
                           C(f'{chip.number} {gates.de}{k} (Child)', f'{chip.number} {gates.en}{k} (child)', f'{chip.number} {gates.fr}{k} (enfant)'), gates.mark))
    else:
        for u, block in enumerate(chip.children, 1):
            k = f' {u}' if len(chip.children) > 1 else ''
            parent.attach(child_block(C(f'{chip.number} {block.de}{k} (Child)', f'{chip.number} {block.en}{k} (child)', f'{chip.number} {block.fr}{k} (enfant)'),
                            block.inputs, block.outputs, number))


import mcu    # noqa: E402  microcontrollers and boards (libraries/schematic/mcu.py)


def overlined(name):
    """A pin name as shown: the part after "~" with a bar over each letter."""
    if '~' not in name:
        return name
    head, tail = name.split('~', 1)
    return head + ''.join(ch + '̅' for ch in tail)


def package_box(prefix, caption, value, pins, layout='dil'):
    """A package as a box: DIP order (1 to n/2 down the left side, the rest up the right side), a two-row header
    (odd pins left, even pins right) or a quad package ('quad': TQFP, QFN, MLF, PLCC with n/4 pins per side, pin 1 at the
    top of the left side, counted counter-clockwise); the pin numbers outside, the names inside."""
    n = len(pins)
    if layout == 'quad':
        return quad_box(prefix, caption, value, pins)
    if layout == 'dil':
        left = [(k + 1, pins[k]) for k in range(n // 2)]
        right = [(n - k, pins[n - k - 1]) for k in range(n // 2)]
    else:
        left = [(k + 1, pins[k]) for k in range(0, n, 2)]
        right = [(k + 2, pins[k + 1]) for k in range(0, n, 2)]
    rows = max(len(left), len(right))
    longest = max(len(name.replace('~', '')) for name in pins)
    width = max(9, math.ceil((2 * longest * 1.15 + 4) / P) + 2)
    s = Symbol(prefix, caption, value=value)
    x0, x1, h = P, (width - 1) * P, (rows + 1) * P
    s.rect(x0, 0, x1, h, w=0.35)
    for k, (number, name) in enumerate(left):
        y = (k + 1) * P
        s.line((0, y), (x0, y)).text(overlined(name), x0 + 0.6, y - 1.0, 1.8)
        s.pin(str(number), (0, y), label=(0.2, y - 2.2), shown=True)
    for k, (number, name) in enumerate(right):
        y = (k + 1) * P
        s.line((x1, y), (x1 + P, y)).text(overlined(name), x1 - 0.6, y - 1.0, 1.8, 'right')
        s.pin(str(number), (x1 + P, y), label=(x1 + P - 0.2, y - 2.2), align='right', shown=True)
    for o in s.parts:
        if o['type'] == 'contact':
            o['font'] = {'height': 1.6}
    s.labels((x0, -3.4), (x1, -3.4), 'left', 'right')
    return s


def quad_box(prefix, caption, value, pins):
    """A quad package: n/4 pins per side, kept a name's length away from the corners so that the names of the left and
    right pins (written across) and of the top and bottom pins (written upwards) do not meet."""
    n = len(pins)
    q = n // 4
    longest = max(len(name.replace('~', '')) for name in pins)
    margin = math.ceil((longest * 1.15 + 1.2) / P)       # pitches from a corner to the first pin
    length = (q + 2 * margin + 1) * P
    x0, y0 = P, P
    x1, y1 = x0 + length, y0 + length
    at = lambda j: (margin + j + 1) * P                 # the j-th pin (from 0) along a side, from the corner
    s = Symbol(prefix, caption, value=value)
    s.rect(x0, y0, x1, y1, w=0.35)
    s.circle(x0 + 1.4, y0 + 1.4, 0.8)
    for k in range(n):
        side_no, j = divmod(k, q)
        name, number = overlined(pins[k]), str(k + 1)
        if side_no == 0:      # left, top to bottom
            y = y0 + at(j)
            s.line((0, y), (x0, y)).text(name, x0 + 0.6, y - 1.0, 1.8)
            s.pin(number, (0, y), label=(0.2, y - 2.2), shown=True)
        elif side_no == 1:    # bottom, left to right
            x = x0 + at(j)
            s.line((x, y1), (x, y1 + P)).text(name, x - 1.0, y1 - 0.6, 1.8, rotation=90)
            s.pin(number, (x, y1 + P), label=(x + 0.3, y1 + 0.3), shown=True)
        elif side_no == 2:    # right, bottom to top
            y = y1 - at(j)
            s.line((x1, y), (x1 + P, y)).text(name, x1 - 0.6, y - 1.0, 1.8, 'right')
            s.pin(number, (x1 + P, y), label=(x1 + P - 0.2, y - 2.2), align='right', shown=True)
        else:                 # top, right to left
            x = x1 - at(j)
            s.line((x, 0), (x, y0)).text(name, x - 1.0, y0 + 0.6, 1.8, 'right', rotation=90)
            s.pin(number, (x, 0), label=(x + 0.3, -2.2), shown=True)
    for o in s.parts:
        if o['type'] == 'contact':
            o['font'] = {'height': 1.6}
    s.labels((x0, -5.4), (x1, -5.4), 'left', 'right')
    return s


MP = DG + '/µP'
for title, parts in ((('AVR', 'AVR', 'AVR'), mcu.AVR), (('PIC', 'PIC', 'PIC'), mcu.PIC),
                     (('Raspberry', 'Raspberry Pi', 'Raspberry Pi'), mcu.RASPBERRY),
                     (('Arduino / Shields', 'Arduino / shields', 'Arduino / shields'), mcu.ARDUINO)):
    add = page(MP, *title)
    for part in parts:
        add(package_box(part.prefix, C(part.de, part.en, part.fr), part.name, part.pins, part.layout))


# --- Hydraulics and pneumatics after ISO 1219
HP = 'Hydraulik + Pneumatik'
Q = 4 * P          # side of a valve's square


def dashed(s, *points, w=W):
    s.parts.append({'type': 'line', 'points': [pt(p) for p in points], 'pen': {'width': w, 'style': 'dash'}, 'electrical': False})
    return s


def triangle(s, tip, base_centre, half, filled=True):
    dx, dy = tip[0] - base_centre[0], tip[1] - base_centre[1]
    length = math.hypot(dx, dy)
    nx, ny = -dy / length * half, dx / length * half
    s.poly(tip, (base_centre[0] + nx, base_centre[1] + ny), (base_centre[0] - nx, base_centre[1] - ny), fill='#000000' if filled else None)
    return s


def flow(s, a, b, size=1.4):
    """A flow path inside a valve square, with an arrow at its end."""
    return s.arrow(a, b, size=size)


def blocked(s, at, up=True):
    """A port closed inside a square: a short line with a bar across."""
    d = 1.6 if up else -1.6
    return s.line(at, (at[0], at[1] - d)).line((at[0] - 1.0, at[1] - d), (at[0] + 1.0, at[1] - d))


def spring(s, x, y, right=True):
    """A return spring at the side of a valve."""
    sign = 1 if right else -1
    pts = [(x, y)]
    for k in range(1, 6):
        pts.append((x + sign * k * 0.9, y + (-1.2 if k % 2 else 1.2)))
    pts.append((x + sign * 5.4, y))
    return s.line(*pts)


def solenoid(s, x, y, left=True):
    w = 2.6
    x0 = x - w if left else x
    return s.rect(x0, y - 1.8, x0 + w, y + 1.8).line((x0, y + 1.8), (x0 + w, y - 1.8))


def valve(caption, squares, ports_top, ports_bottom, operate_left='solenoid', operate_right='spring', rest=None):
    """A directional valve: `squares` drawn left to right, each a function drawing its paths into the square whose left
    edge is at x; the ports attach to the square of the rest position (`rest`, the right one unless given)."""
    s = Symbol('Q', caption)
    n = len(squares)
    x_rest = (n - 1 if rest is None else rest) * Q
    for k, draw in enumerate(squares):
        x = k * Q
        s.rect(x, 0, x + Q, Q, w=0.35)
        draw(s, x)
    for name, dx in ports_top:
        s.line((x_rest + dx, 0), (x_rest + dx, -P)).pin(name, (x_rest + dx, -P), label=(x_rest + dx + 0.5, -P - 0.4), shown=True)
    for name, dx in ports_bottom:
        s.line((x_rest + dx, Q), (x_rest + dx, Q + P)).pin(name, (x_rest + dx, Q + P), label=(x_rest + dx + 0.5, Q + P - 2.2), shown=True)
    if operate_left == 'solenoid':
        solenoid(s, 0, Q / 2)
    elif operate_left == 'push':
        s.line((0, Q / 2), (-2.4, Q / 2)).line((-2.4, Q / 2 - 1.2), (-2.4, Q / 2 + 1.2)).line((-2.4, Q / 2 - 1.2), (-3.2, Q / 2 - 1.2)).line((-2.4, Q / 2 + 1.2), (-3.2, Q / 2 + 1.2))
    elif operate_left == 'lever':
        s.line((0, Q / 2), (-2.6, Q / 2)).line((-2.6, Q / 2), (-4.0, Q / 2 - 2.0)).circle(-4.0, Q / 2 - 2.0, 0.8, fill='#000000')
    elif operate_left == 'pilot':
        dashed(s, (0, Q / 2), (-3.0, Q / 2))
        triangle(s, (-1.2, Q / 2), (-2.6, Q / 2), 0.9, filled=False)
    if operate_right == 'spring':
        spring(s, n * Q, Q / 2)
    elif operate_right == 'solenoid':
        solenoid(s, n * Q, Q / 2, left=False)
    s.labels((n * Q + 6.4, -0.4), (n * Q + 6.4, 2.6))
    return s


# The paths of the squares, x being the square's left edge; ports at x + P and x + 3 P.
def through(s, x):        # P to A
    return flow(s, (x + P, Q), (x + P, 0))


def closed2(s, x):
    blocked(s, (x + P, Q), True)
    return blocked(s, (x + P, 0), False)


def p_to_a_r_closed(s, x):     # 3/2 active: P to A, R closed
    flow(s, (x + P, Q), (x + P, 0))
    return blocked(s, (x + 3 * P, Q), True)


def a_to_r_p_closed(s, x):     # 3/2 rest: A to R, P closed
    flow(s, (x + P, 0), (x + 3 * P, Q))
    return blocked(s, (x + P, Q), True)


def cross(s, x):               # 4/2: P to B, A to T
    flow(s, (x + P, Q), (x + 3 * P, 0))
    return flow(s, (x + P, 0), (x + 3 * P, Q))


def parallel(s, x):            # 4/2: P to A, B to T
    flow(s, (x + P, Q), (x + P, 0))
    return flow(s, (x + 3 * P, 0), (x + 3 * P, Q))


def all_closed(s, x):          # 4/3 centre: all ports closed
    for dx in (P, 3 * P):
        blocked(s, (x + dx, Q), True)
        blocked(s, (x + dx, 0), False)
    return s


add = page(HP, 'Wegeventile', 'Directional valves', 'Distributeurs')
add(valve(C('2/2-Wegeventil', '2/2 directional valve', 'Distributeur 2/2'), [through, closed2], [('A', P)], [('P', P)]))
add(valve(C('3/2-Wegeventil', '3/2 directional valve', 'Distributeur 3/2'), [p_to_a_r_closed, a_to_r_p_closed], [('A', P)], [('P', P), ('R', 3 * P)]))
add(valve(C('3/2-Wegeventil, Handbetätigung', '3/2 valve, push button', 'Distributeur 3/2, bouton-poussoir'), [p_to_a_r_closed, a_to_r_p_closed], [('A', P)], [('P', P), ('R', 3 * P)], operate_left='push'))
add(valve(C('4/2-Wegeventil', '4/2 directional valve', 'Distributeur 4/2'), [cross, parallel], [('A', P), ('B', 3 * P)], [('P', P), ('T', 3 * P)]))
add(valve(C('4/2-Wegeventil, Hebel', '4/2 valve, lever', 'Distributeur 4/2, levier'), [cross, parallel], [('A', P), ('B', 3 * P)], [('P', P), ('T', 3 * P)], operate_left='lever'))
add(valve(C('4/2-Wegeventil, Vorsteuerung', '4/2 valve, pilot operated', 'Distributeur 4/2, pilotage'), [cross, parallel], [('A', P), ('B', 3 * P)], [('P', P), ('T', 3 * P)], operate_left='pilot'))
add(valve(C('4/3-Wegeventil, Mittelstellung gesperrt', '4/3 valve, closed centre', 'Distributeur 4/3, centre fermé'),
          [parallel, all_closed, cross], [('A', P), ('B', 3 * P)], [('P', P), ('T', 3 * P)], operate_right='solenoid', rest=1))

add = page(HP, 'Zylinder', 'Cylinders', 'Vérins')


def cylinder(caption, double=True, spring_return=False, rod_both=False):
    s = Symbol('Z', caption)
    L, H = 8 * P, 2 * P
    s.rect(0, -H, L, 0, w=0.35)
    s.line((2 * P, -H), (2 * P, 0), w=THICK)
    s.line((2 * P, -H / 2), (L + 2 * P, -H / 2), w=THICK)
    if rod_both:
        s.line((2 * P, -H / 2), (-2 * P, -H / 2), w=THICK)
    if spring_return:
        pts = [(2 * P + 0.4, -H + 0.6)]
        for k in range(1, 8):
            pts.append((2 * P + k * 1.4, -H + (0.6 if k % 2 == 0 else H - 0.6)))
        s.line(*pts)
    s.line((P, 0), (P, P)).pin('1', (P, P), label=(P + 0.5, P - 2.2), shown=True)
    if double:
        s.line((L - P, 0), (L - P, P)).pin('2', (L - P, P), label=(L - P + 0.5, P - 2.2), shown=True)
    s.labels((0, -H - 3.4), (3 * P, -H - 3.4))
    return s


add(cylinder(C('Einfachwirkender Zylinder, Federrückstellung', 'Single-acting cylinder, spring return', 'Vérin simple effet, rappel par ressort'), double=False, spring_return=True))
add(cylinder(C('Doppeltwirkender Zylinder', 'Double-acting cylinder', 'Vérin double effet')))
add(cylinder(C('Doppeltwirkender Zylinder, durchgehende Kolbenstange', 'Double-acting cylinder, through rod', 'Vérin double effet, tige traversante'), rod_both=True))
s = cylinder(C('Doppeltwirkender Zylinder mit Endlagendämpfung', 'Double-acting cylinder with cushioning', 'Vérin double effet amorti'))
add(s.rect(2 * P - 1.4, -2 * P + 1.0, 2 * P, -1.0).rect(8 * P - 2.2, -2 * P + 1.0, 8 * P - 0.8, -1.0))

add = page(HP, 'Pumpen + Kompressoren', 'Pumps + compressors', 'Pompes + compresseurs')


def machine(caption, prefix, hollow, outward=True, variable=False, two_way=False, motor_letter=None):
    s = Symbol(prefix, caption)
    d = 4 * P
    s.circle(0, 0, d, w=0.35)
    s.line((0, -d / 2), (0, -d / 2 - P)).pin('1', (0, -d / 2 - P), label=(0.5, -d / 2 - P - 0.4), shown=True)
    s.line((0, d / 2), (0, d / 2 + P)).pin('2', (0, d / 2 + P), label=(0.5, d / 2 + P - 2.2), shown=True)
    tip, base = ((0, -d / 2), (0, -d / 2 + 2.4)) if outward else ((0, -d / 2 + 2.4), (0, -d / 2))
    triangle(s, tip, base, 1.4, filled=not hollow)
    if two_way:
        tip2, base2 = ((0, d / 2), (0, d / 2 - 2.4)) if outward else ((0, d / 2 - 2.4), (0, d / 2))
        triangle(s, tip2, base2, 1.4, filled=not hollow)
    if variable:
        s.arrow((-3.6, 3.6), (3.6, -3.6), size=1.2)
    s.line((d / 2, 0), (d / 2 + 2.0, 0)).line((d / 2 + 2.0, -0.6), (d / 2 + 2.0, 0.6))  # the shaft
    s.labels((d / 2 + 3.0, -d / 2), (d / 2 + 3.0, -d / 2 + 3.0))
    return s


add(machine(C('Hydropumpe', 'Hydraulic pump', 'Pompe hydraulique'), 'P', hollow=False))
add(machine(C('Hydropumpe, verstellbar', 'Variable hydraulic pump', 'Pompe hydraulique variable'), 'P', hollow=False, variable=True))
add(machine(C('Hydropumpe, zwei Förderrichtungen', 'Hydraulic pump, two directions', 'Pompe hydraulique, deux sens'), 'P', hollow=False, two_way=True))
add(machine(C('Kompressor', 'Compressor', 'Compresseur'), 'P', hollow=True))
add(machine(C('Vakuumpumpe', 'Vacuum pump', 'Pompe à vide'), 'P', hollow=True, outward=False))

add = page(HP, 'Motoren', 'Motors', 'Moteurs')
add(machine(C('Hydromotor', 'Hydraulic motor', 'Moteur hydraulique'), 'M', hollow=False, outward=False))
add(machine(C('Hydromotor, verstellbar', 'Variable hydraulic motor', 'Moteur hydraulique variable'), 'M', hollow=False, outward=False, variable=True))
add(machine(C('Druckluftmotor', 'Pneumatic motor', 'Moteur pneumatique'), 'M', hollow=True, outward=False))
s = Symbol('M', C('Schwenkmotor', 'Rotary actuator', 'Vérin rotatif'))
s.arc(0, 0, 4 * P, 0, 180, w=0.35).line((-2 * P, 0), (2 * P, 0), w=0.35)
triangle(s, (-1.0, -2 * P + 0.4), (-1.0, -2 * P + 2.4), 1.2)
s.line((-P, 0), (-P, P)).pin('1', (-P, P), label=(-P + 0.5, P - 2.2), shown=True).line((P, 0), (P, P)).pin('2', (P, P), label=(P + 0.5, P - 2.2), shown=True)
add(s.labels((2 * P + 1.0, -2 * P), (2 * P + 1.0, -2 * P + 3.0)))

add = page(HP, 'Druckventile', 'Pressure valves', 'Valves de pression')


def pressure_valve(caption, normally_open=False, adjustable=True):
    s = Symbol('V', caption)
    s.rect(0, 0, Q, Q, w=0.35)
    if normally_open:
        flow(s, (P, Q), (P, 0))
    else:
        flow(s, (P + 1.2, Q - 0.8), (P + 1.2, 0.8))
    s.line((P, Q), (P, Q + P)).pin('P', (P, Q + P), label=(P + 0.5, Q + P - 2.2), shown=True)
    s.line((P, 0), (P, -P)).pin('T' if not normally_open else 'A', (P, -P), label=(P + 0.5, -P - 0.4), shown=True)
    dashed(s, (P, Q + P / 2), (-1.6, Q + P / 2), (-1.6, Q / 2), (0, Q / 2))
    spring(s, Q, Q / 2)
    if adjustable:
        s.arrow((Q + 0.6, Q / 2 + 2.0), (Q + 4.6, Q / 2 - 2.0), size=1.0)
    s.labels((Q + 6.4, -0.4), (Q + 6.4, 2.6))
    return s


add(pressure_valve(C('Druckbegrenzungsventil', 'Pressure relief valve', 'Limiteur de pression')))
add(pressure_valve(C('Druckregelventil', 'Pressure reducing valve', 'Réducteur de pression'), normally_open=True))
add(pressure_valve(C('Druckbegrenzungsventil, fest', 'Pressure relief valve, fixed', 'Limiteur de pression, fixe'), adjustable=False))

add = page(HP, 'Sperrventile', 'Non-return valves', 'Clapets')


def check_valve(caption, with_spring=False, piloted=False):
    s = Symbol('V', caption)
    s.line((0, 0), (0, P)).line((0, 3 * P), (0, 4 * P))
    s.line((-1.6, 3 * P - 0.6), (0, 2 * P - 0.6), (1.6, 3 * P - 0.6))
    s.circle(0, 2 * P + 0.2, 2.0)
    s.line((0, P), (0, 2 * P - 0.8))
    if with_spring:
        s.line((0, 2 * P + 1.2), (-0.8, 2 * P + 1.8), (0.8, 2 * P + 2.4), (-0.8, 2 * P + 3.0), (0, 3 * P))
    if piloted:
        dashed(s, (3.0, 2 * P), (3 * P, 2 * P))
        s.pin('X', (3 * P, 2 * P), label=(3 * P - 0.4, 2 * P - 2.2), align='right', shown=True)
    s.pin('A', (0, 0), label=(0.5, -0.4), shown=True).pin('B', (0, 4 * P), label=(0.5, 4 * P - 2.2), shown=True)
    s.labels((2.6, 0), (2.6, 3.0))
    return s


add(check_valve(C('Rückschlagventil', 'Non-return valve', 'Clapet anti-retour')))
add(check_valve(C('Rückschlagventil mit Feder', 'Non-return valve with spring', 'Clapet anti-retour à ressort'), with_spring=True))
add(check_valve(C('Entsperrbares Rückschlagventil', 'Pilot-operated non-return valve', 'Clapet piloté'), with_spring=True, piloted=True))

add = page(HP, 'Stromventile', 'Flow control valves', 'Régulateurs de débit')


def throttle(caption, adjustable=True, one_way=False):
    s = Symbol('V', caption)
    s.line((0, 0), (0, P)).line((0, 3 * P), (0, 4 * P))
    s.arc(-1.6, 2 * P, 2 * P, -50, 50).arc(1.6, 2 * P, 2 * P, 130, 230)
    s.line((0, P), (0, 3 * P))
    if adjustable:
        s.arrow((-2.6, 3 * P), (2.6, P), size=1.0)
    if one_way:
        s.line((0, P), (2 * P, P), (2 * P, 2 * P - 1.6)).line((2 * P, 2 * P + 1.6), (2 * P, 3 * P), (0, 3 * P))
        s.line((2 * P - 1.4, 2 * P + 1.0), (2 * P, 2 * P - 0.4), (2 * P + 1.4, 2 * P + 1.0)).circle(2 * P, 2 * P - 1.2, 1.6)
    s.pin('A', (0, 0), label=(0.5, -0.4), shown=True).pin('B', (0, 4 * P), label=(0.5, 4 * P - 2.2), shown=True)
    s.labels((3 * P, 0), (3 * P, 3.0))
    return s


add(throttle(C('Drosselventil', 'Throttle valve', 'Étrangleur'), adjustable=False))
add(throttle(C('Drosselventil, einstellbar', 'Adjustable throttle', 'Étrangleur réglable')))
add(throttle(C('Drosselrückschlagventil', 'One-way flow control valve', 'Réducteur de débit unidirectionnel'), one_way=True))

add = page(HP, 'Speicher', 'Accumulators', 'Accumulateurs')
s = Symbol('P', C('Hydrospeicher', 'Hydraulic accumulator', 'Accumulateur hydraulique'))
s.rect(-P, -4 * P, P, -P, w=0.35, corner=50).line((-P, -2.5 * P), (P, -2.5 * P))
add(s.line((0, -P), (0, 0)).pin('1', (0, 0), label=(0.5, 0.2), shown=True).labels((P + 1.0, -4 * P), (P + 1.0, -4 * P + 3.0)))
s = Symbol('P', C('Druckluftbehälter', 'Air receiver', 'Réservoir d’air'))
s.rect(-2 * P, -3 * P, 2 * P, -P, w=0.35, corner=50)
add(s.line((0, -P), (0, 0)).pin('1', (0, 0), label=(0.5, 0.2), shown=True).labels((2 * P + 1.0, -3 * P), (2 * P + 1.0, -3 * P + 3.0)))

add = page(HP, 'Messgeräte', 'Measuring devices', 'Appareils de mesure')
s = Symbol('M', C('Manometer', 'Pressure gauge', 'Manomètre'))
s.circle(0, -2 * P, 4 * P - 0.4, w=0.35).arrow((-1.6, -2 * P + 1.6), (1.8, -2 * P - 1.8), size=1.0)
add(s.line((0, 0), (0, -0.2)).pin('1', (0, 0), label=(0.5, 0.2), shown=True).labels((2 * P + 1.0, -4 * P), (2 * P + 1.0, -4 * P + 3.0)))
s = Symbol('M', C('Thermometer', 'Thermometer', 'Thermomètre'))
s.circle(0, -2 * P, 4 * P - 0.4, w=0.35).line((0, -2 * P + 2.0), (0, -2 * P - 2.6)).circle(0, -2 * P + 2.4, 1.2, fill='#000000')
add(s.line((0, 0), (0, -0.2)).pin('1', (0, 0), label=(0.5, 0.2), shown=True).labels((2 * P + 1.0, -4 * P), (2 * P + 1.0, -4 * P + 3.0)))
s = Symbol('M', C('Durchflussmesser', 'Flow meter', 'Débitmètre'))
s.circle(0, 0, 4 * P - 0.4, w=0.35).line((0, -2 * P), (0, -3 * P)).line((0, 2 * P), (0, 3 * P)).arrow((0, 2.4), (0, -2.4), size=1.2)
add(s.pin('1', (0, 3 * P), label=(0.5, 3 * P - 2.2), shown=True).pin('2', (0, -3 * P), label=(0.5, -3 * P - 0.4), shown=True).labels((2 * P + 1.0, -2 * P), (2 * P + 1.0, -2 * P + 3.0)))

add = page(HP, 'Übertragung + Aufbereitung', 'Transfer + conditioning', 'Transfert + conditionnement')
s = Symbol('P', C('Behälter', 'Tank', 'Réservoir'))
s.line((-2 * P, -2 * P), (-2 * P, 0), (2 * P, 0), (2 * P, -2 * P)).line((0, -3 * P), (0, -1.0))
add(s.pin('1', (0, -3 * P), label=(0.5, -3 * P - 0.4), shown=True).labels((2 * P + 1.0, -2 * P), (2 * P + 1.0, -2 * P + 3.0)))


def conditioner(caption, extra):
    s = Symbol('F', caption)
    s.poly((0, -2 * P), (2 * P, 0), (0, 2 * P), (-2 * P, 0), fill=None, w=0.35)
    s.line((-2 * P, 0), (-3 * P, 0)).line((2 * P, 0), (3 * P, 0))
    extra(s)
    s.pin('1', (-3 * P, 0)).pin('2', (3 * P, 0), label=(3 * P - 0.4, -2.2), align='right')
    s.labels((0, -2 * P - 3.4), (0, 2 * P + 0.6), 'centre')
    return s


add(conditioner(C('Filter', 'Filter', 'Filtre'), lambda s: dashed(s, (0, -2 * P + 0.6), (0, 2 * P - 0.6))))
add(conditioner(C('Wasserabscheider', 'Water separator', 'Séparateur d’eau'), lambda s: s.line((-1.6, 2 * P - 1.6), (1.6, 2 * P - 1.6)).line((0, 2 * P), (0, 3 * P))))
add(conditioner(C('Öler', 'Lubricator', 'Lubrificateur'), lambda s: s.line((-1.2, -1.2), (0, 1.2), (1.2, -1.2))))
add(conditioner(C('Kühler', 'Cooler', 'Refroidisseur'), lambda s: s.line((-2 * P, 0), (2 * P, 0)).line((-1.2, -1.4), (0, -2.6), (1.2, -1.4))))
s = Symbol('F', C('Schalldämpfer', 'Silencer', 'Silencieux'))
s.line((0, 0), (0, -P)).rect(-1.6, -P - 3.6, 1.6, -P, w=0.35)
add(s.pin('1', (0, 0), label=(0.5, 0.2), shown=True).labels((2.2, -P - 3.6), (2.2, -P - 0.6)))

add = page(HP, 'Betätigungsarten', 'Operating devices', 'Commandes')


def actuation(caption, draw):
    s = Symbol('', caption, numbered=False, listed=False, shown=False)
    s.line((0, 0), (0, -2.0)).line((0, 2.0), (0, 0))
    draw(s)
    return s


add(actuation(C('Muskelkraft allgemein', 'Manual, general', 'Manuel, général'), lambda s: s.line((0, 0), (-3.0, 0)).line((-3.0, -1.2), (-3.0, 1.2))))
add(actuation(C('Druckknopf', 'Push button', 'Bouton-poussoir'), lambda s: s.line((0, 0), (-2.4, 0)).arc(-3.4, 0, 2.0, 270, 450)))
add(actuation(C('Hebel', 'Lever', 'Levier'), lambda s: s.line((0, 0), (-3.0, -2.4)).circle(-3.0, -2.4, 0.8, fill='#000000')))
add(actuation(C('Pedal', 'Pedal', 'Pédale'), lambda s: s.line((0, 0), (-2.0, 0), (-3.4, -1.6))))
add(actuation(C('Stößel', 'Plunger', 'Poussoir'), lambda s: s.line((0, 0), (-3.0, 0))))
add(actuation(C('Rolle', 'Roller', 'Galet'), lambda s: s.line((0, 0), (-2.0, 0)).circle(-2.8, 0, 1.6)))
add(actuation(C('Feder', 'Spring', 'Ressort'), lambda s: spring(s, 0, 0, right=False)))
add(actuation(C('Elektromagnet', 'Solenoid', 'Électroaimant'), lambda s: solenoid(s, 0, 0)))
add(actuation(C('Druckbetätigung (Vorsteuerung)', 'Pilot pressure', 'Pilotage par pression'), lambda s: (dashed(s, (0, 0), (-3.2, 0)), triangle(s, (-1.0, 0), (-2.6, 0), 0.9, filled=False))[0]))

add = page(HP, 'Schnellkupplungen', 'Quick couplings', 'Coupleurs rapides')
s = Symbol('K', C('Schnellkupplung, verbunden', 'Quick coupling, connected', 'Coupleur rapide, accouplé'))
s.line((0, 0), (P, 0)).line((3 * P, 0), (4 * P, 0)).line((P, -1.2), (P, 1.2)).line((3 * P, -1.2), (3 * P, 1.2)).line((P, 0), (3 * P, 0))
s.line((P + 1.4, -1.6), (P + 1.4, 1.6)).line((3 * P - 1.4, -1.6), (3 * P - 1.4, 1.6))
add(s.pin('1', (0, 0)).pin('2', (4 * P, 0), label=(4 * P - 0.4, -2.2), align='right').labels((2 * P, -5.0), (2 * P, 2.4), 'centre'))
s = Symbol('K', C('Schnellkupplung mit Rückschlagventil, getrennt', 'Quick coupling with non-return valve, separated', 'Coupleur rapide à clapet, désaccouplé'))
s.line((0, 0), (P, 0)).line((3 * P, 0), (4 * P, 0)).line((P, 0), (P + 2.0, 0)).line((3 * P - 2.0, 0), (3 * P, 0))
s.circle(P + 2.6, 0, 1.2).line((P + 3.4, -1.2), (P + 3.4, 1.2)).circle(3 * P - 2.6, 0, 1.2).line((3 * P - 3.4, -1.2), (3 * P - 3.4, 1.2))
add(s.pin('1', (0, 0)).pin('2', (4 * P, 0), label=(4 * P - 0.4, -2.2), align='right').labels((2 * P, -5.0), (2 * P, 2.4), 'centre'))

add = page(HP, 'Elektrisch', 'Electrical', 'Électrique')
s = Symbol('S', C('Druckschalter', 'Pressure switch', 'Pressostat'))
s.line((0, 0), (0, -P)).rect(-1.6, -P - 4.0, 1.6, -P).line((0, -P - 1.4), (-1.2, -P - 2.6), (1.2, -P - 2.6), (0, -P - 1.4))
dashed(s, (1.6, -P - 2.0), (4.4, -P - 2.0))
s.line((4.4, -P - 4.0), (4.4, -P), (5.8, -P - 3.0))
add(s.pin('1', (0, 0), label=(0.5, 0.2), shown=True).labels((7.0, -P - 4.0), (7.0, -P - 1.0)))
s = Symbol('S', C('Elektrisch-pneumatischer Wandler', 'Electro-pneumatic converter', 'Convertisseur électropneumatique'))
s.rect(-2 * P, -2 * P, 2 * P, 2 * P, w=0.35).line((-2 * P, 2 * P), (2 * P, -2 * P)).line((-2 * P, 0), (-3 * P, 0)).line((2 * P, 0), (3 * P, 0))
s.text('E', -2 * P + 0.6, -2 * P + 0.6, 2.2).text('P', 2 * P - 0.6, 2 * P - 3.0, 2.2, 'right')
add(s.pin('1', (-3 * P, 0)).pin('2', (3 * P, 0), label=(3 * P - 0.4, -2.2), align='right').labels((2 * P + 1.0, -2 * P - 3.4), (2 * P + 1.0, 2 * P + 0.6)))


# --- Writing
def slug(text):
    t = text.split('\r')[0].lower()
    for a, b in (('ä', 'ae'), ('ö', 'oe'), ('ü', 'ue'), ('ß', 'ss'), ('µ', 'u'), ('≥', 'ge')):
        t = t.replace(a, b)
    return re.sub(r'[^a-z0-9]+', '-', t).strip('-')


def load_pages(only=None):
    """The page modules in pages/, by file name. Each runs in a namespace of its own on top of the building blocks above,
    so helpers of one module do not change another; page(), extend() and PAGES are shared."""
    folder = os.path.join(HERE, 'pages')
    for name in sorted(os.listdir(folder)) if os.path.isdir(folder) else []:
        if name.endswith('.py') and not name.startswith('_') and (only is None or name[:-3] in only):
            path = os.path.join(folder, name)
            with open(path, encoding='utf-8') as f:
                exec(compile(f.read(), path, 'exec'), dict(globals(), __name__='pages.' + name[:-3], __file__=path))


def on_pitch(v):
    return abs(v / P - round(v / P)) < 1e-6


def check():
    """What the module's test asks of every page, reported for all pages at once."""
    problems = []
    for p in PAGES:
        where = p['folder'] + ' / ' + p['name'].split('\r')[0]
        if p['name'].count('\r') != 2 or not all(p['name'].split('\r')) or not p['folder']:
            problems.append(where + ': name in three languages and a folder')
        if not p['symbols']:
            problems.append(where + ': no symbols')
        captions = set()
        for s in [s for parent in p['symbols'] for s in [parent] + parent.children]:
            c = s.caption
            if c.count('\r') != 2 or not all(c.split('\r')):
                problems.append(where + ': caption not in three languages: ' + repr(c))
            if c in captions:
                problems.append(where + ': caption twice: ' + c.split('\r')[0])
            captions.add(c)
            if s.children and not getattr(s, 'parent', False):
                problems.append(where + ': children on a symbol that is no parent: ' + c.split('\r')[0])
            parts = s.json('x')['children']
            if not any(o['type'] != 'contact' for o in parts):
                problems.append(where + ': nothing drawn: ' + c.split('\r')[0])
            pins = [o for o in parts if o['type'] == 'contact']
            if len({o['name'] for o in pins}) != len(pins):
                problems.append(where + ': contact names twice: ' + c.split('\r')[0])
            for o in pins:
                if not (on_pitch(o['pin'][0]) and on_pitch(o['pin'][1])):
                    problems.append(where + ': ' + c.split('\r')[0] + ': contact ' + o['name'] + ' off the 2.54 mm pitch ' + str(o['pin']))
            for o in parts:
                for q in o.get('points', []) + [o.get(k) for k in ('centre', 'pos', 'pin') if o.get(k)]:
                    if abs(q[0]) > 150 or abs(q[1]) > 150:
                        problems.append(where + ': ' + c.split('\r')[0] + ': drawn far from the insertion point ' + str(q))
                        break
    return problems


def main(out=HERE, only=None):
    load_pages(only)
    problems = check()
    if problems:
        raise SystemExit('\n'.join(problems))
    os.makedirs(out, exist_ok=True)
    written = set()
    for p in PAGES:
        symbols = []
        path = 'OpenLoch/' + p['folder'] + '/' + p['name'].split('\r')[0] + '/'
        for s in p['symbols']:
            o = {'caption': s.caption, 'item': s.json(path + s.caption.split('\r')[0])}
            if s.children:
                o['children'] = []
                for c, at in zip(s.children, child_places(s)):
                    item = c.json(path + c.caption.split('\r')[0])
                    item['pos'] = pt(at)
                    o['children'].append(item)
            symbols.append(o)
        data = {'format': 'OpenLoch Schematic Library', 'version': 3 if any(s.children for s in p['symbols']) else 2, 'unit': 'mm',
                'name': p['name'], 'folder': p['folder'], 'symbols': symbols}
        name = slug(p['folder'].replace('/', ' ')) + '--' + slug(p['name']) + '.json'
        assert name not in written, name
        written.add(name)
        with open(os.path.join(out, name), 'w', encoding='utf-8', newline='\n') as f:
            json.dump(data, f, ensure_ascii=False, indent=1, sort_keys=True)
            f.write('\n')
    for old in os.listdir(out):
        if old.endswith('.json') and old not in written:
            os.remove(os.path.join(out, old))
    print(len(written), 'pages,', sum(len(p['symbols']) for p in PAGES), 'symbols,', sum(len(s.children) for p in PAGES for s in p['symbols']), 'children with them')


if __name__ == '__main__':
    # --out DIR writes the pages there instead (a folder of their own: other JSON files in it are removed); --only a,b
    # loads only these page modules (for trying one out). The pages that come with OpenLoch are written without either.
    args, opts = sys.argv[1:], {}
    while len(args) >= 2 and args[0] in ('--out', '--only'):
        opts[args[0]], args = args[1], args[2:]
    if args:
        raise SystemExit('usage: generate.py [--out DIR] [--only module,module]')
    main(os.path.abspath(opts.get('--out', HERE)), opts['--only'].split(',') if '--only' in opts else None)
