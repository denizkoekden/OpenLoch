#!/usr/bin/env python3
"""Create the self-contained, script-free SVG artwork of the GitHub README from OpenLoch's own material.

The header is a program window in the style of the period: the pixel logo (assets/openloch-logo-32.png), menus, the
toolbar with the program's own symbols and one project in four document windows (schematic, perfboard, circuit board
and front panel of the two-transistor flasher), each drawn by its module through tools/readme_art.cpp (the CMake target
openloch_readme_art), beside the four modules and their models.
Run after scripts/make-logo.py from the repository root with a configured build directory `build`.
"""
from pathlib import Path
import base64
import struct
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
subprocess.run(['cmake', '--build', str(root / 'build'), '--target', 'openloch_readme_art'], check=True, capture_output=True)


def size_of(png):
    width, height = struct.unpack('>II', png[16:24])
    return width, height


def jpeg_size(jpeg):
    """Width and height from the start-of-frame segment of a JPEG."""
    at = 2
    while at < len(jpeg):
        marker, length = jpeg[at + 1], struct.unpack('>H', jpeg[at + 2:at + 4])[0]
        if marker in (0xC0, 0xC1, 0xC2):
            height, width = struct.unpack('>HH', jpeg[at + 5:at + 9])
            return width, height
        at += 2 + length
    raise ValueError('no JPEG frame')


def data(png):
    return 'data:image/png;base64,' + base64.b64encode(png).decode('ascii')


with tempfile.TemporaryDirectory(prefix='openloch-readme-') as temporary:
    out = Path(temporary)
    subprocess.run([str(root / 'build/openloch_readme_art'), str(root / 'libraries'), str(out)], check=True, capture_output=True,
                   env={'QT_QPA_PLATFORM': 'offscreen', 'PATH': '/usr/bin:/bin'})
    pictures = {name: (out / f'{name}.png').read_bytes() for name in ('toolbar', 'icon-save', 'icon-undolist', 'icon-kolben')}
    suite = {name: (out / f'{name}.png').read_bytes() for name in ('suite-schematic', 'suite-pcb', 'suite-panel', 'kind-schematic', 'kind-perfboard', 'kind-pcb', 'kind-frontpanel')}
    suite['suite-perfboard'] = (out / 'suite-perfboard.jpg').read_bytes()
    import json
    parts = [(entry, (out / entry['file']).read_bytes()) for entry in json.loads((out / 'parts.json').read_text())]
logo = (root / 'assets/openloch-logo-32.png').read_bytes()
PIXELATED = 'style="image-rendering:pixelated;image-rendering:crisp-edges"'


def bevel(x, y, width, height, face='#c0c0c0'):
    """A raised Windows 95 frame: white and grey on the light edges, black and dark grey on the shadow edges."""
    return f'''<rect x="{x}" y="{y}" width="{width}" height="{height}" fill="{face}"/>
<path d="M{x},{y+height-1}V{y}H{x+width-1}" fill="none" stroke="#fff" stroke-width="2"/>
<path d="M{x+width-1},{y}V{y+height-1}H{x}" fill="none" stroke="#000" stroke-width="2"/>
<path d="M{x+width-3},{y+2}V{y+height-3}H{x+2}" fill="none" stroke="#808080" stroke-width="2"/>'''


def sunken(x, y, width, height, face):
    """A sunken field like a Windows 95 client area or status bar field."""
    return f'''<rect x="{x}" y="{y}" width="{width}" height="{height}" fill="{face}"/>
<path d="M{x},{y+height}V{y}H{x+width}" fill="none" stroke="#808080" stroke-width="1"/>
<path d="M{x+width},{y}V{y+height}H{x}" fill="none" stroke="#fff" stroke-width="1"/>'''


toolbar_w, toolbar_h = (v // 2 for v in size_of(pictures['toolbar']))
menus = ['Datei', 'Bearbeiten', 'Ansicht', 'Projekt', 'Fenster', 'Hilfe']
menu_x, menu_items = 20, []
for name in menus:
    menu_items.append(f'<text x="{menu_x}" y="61" font-size="14">{name}</text>')
    menu_x += 8 * len(name) + 20

# One project in four document windows, each drawn by its module, and the modules beside them.
body_top, gap = 108, 8
info_w = 262
info_x = 950 - info_w
tile_w = (info_x - 10 - 10 - gap) // 2
tile_h = 214
title_h = 22
documents = [('schematic', 'Schaltplan', 'suite-schematic', '#ffffff'), ('perfboard', 'Lochraster', 'suite-perfboard', '#ece9da'),
             ('pcb', 'Leiterplatte', 'suite-pcb', '#000000'), ('frontpanel', 'Frontplatte', 'suite-panel', '#ece9da')]


def tile(x, y, kind, name, picture, face):
    png = suite[picture]
    width, height = size_of(png) if png[:8] == b'\x89PNG\r\n\x1a\n' else jpeg_size(png)
    area_w, area_h = tile_w - 12, tile_h - title_h - 14
    scale = min(area_w / width, area_h / height)
    w, h = width * scale, height * scale
    kind_png = suite[f'kind-{kind}']
    mime = 'image/png' if png[:8] == b'\x89PNG\r\n\x1a\n' else 'image/jpeg'
    return f'''{bevel(x, y, tile_w, tile_h)}
<rect x="{x+4}" y="{y+4}" width="{tile_w-8}" height="{title_h}" fill="#000080"/>
<image x="{x+8}" y="{y+7}" width="16" height="16" {PIXELATED} href="{data(kind_png)}"/>
<text x="{x+30}" y="{y+20}" font-size="13" font-weight="bold" fill="#fff">Blinklicht – {name}</text>
{sunken(x+6, y+title_h+8, tile_w-12, tile_h-title_h-14, face)}
<image x="{x+6+(area_w-w)/2:.1f}" y="{y+title_h+8+(area_h-h)/2:.1f}" width="{w:.1f}" height="{h:.1f}" href="data:{mime};base64,{base64.b64encode(png).decode('ascii')}"/>'''


tiles = ''.join(tile(10 + (i % 2) * (tile_w + gap), body_top + (i // 2) * (tile_h + gap), *document) for i, document in enumerate(documents))
body_h = 2 * tile_h + gap
status_y = body_top + body_h + 8
height = status_y + 30
modules = [('schematic', 'Schaltplan', 'wie sPlan'), ('perfboard', 'Lochraster', 'wie LochMaster 4'), ('pcb', 'Leiterplatte', 'wie Sprint-Layout 6'),
           ('frontpanel', 'Frontplatte', 'wie FrontDesigner 3')]
module_items = ''.join(
    f'<image x="{info_x+18}" y="{body_top+150+40*i}" width="32" height="32" {PIXELATED} href="{data(suite[f"kind-{kind}"])}"/>'
    f'<text x="{info_x+60}" y="{body_top+164+40*i}" font-size="14" font-weight="bold">{name}</text>'
    f'<text x="{info_x+60}" y="{body_top+180+40*i}" font-size="12" fill="#404040">{model}</text>'
    for i, (kind, name, model) in enumerate(modules))
features = ['Öffnet die Originaldateien', 'Gemeinsame Bauteile', 'Vom Schaltplan zur Platine', '693 eigene Bauteile (CC0)']
feature_items = ''.join(
    f'<rect x="{info_x+20}" y="{body_top+322+21*i}" width="7" height="7" fill="#000080"/><text x="{info_x+34}" y="{body_top+330+21*i}" font-size="12">{text}</text>'
    for i, text in enumerate(features))

header = f'''<svg xmlns="http://www.w3.org/2000/svg" width="960" height="{height}" viewBox="0 0 960 {height}" role="img" aria-labelledby="title description">
<title id="title">OpenLoch — Elektronik-Werkstatt</title>
<desc id="description">Programmfenster im Stil der 90er-Jahre mit dem OpenLoch-Pixellogo und einem Projekt in vier Dokumenten, jedes von seinem Modul gezeichnet: Schaltplan, Lochrasterplatine, Leiterplatte und Frontplatte eines Zweitransistor-Blinklichts. Daneben die vier Module mit ihren Vorbildern sPlan, LochMaster, Sprint-Layout und FrontDesigner. Native Werkzeuge für Elektronikprojekte, C++20 und Qt 6.</desc>
<g font-family="Tahoma, Verdana, sans-serif" fill="#000">
{bevel(1, 1, 958, height - 2)}
<rect x="8" y="8" width="944" height="30" fill="#000080"/>
<image x="14" y="15" width="16" height="16" href="{data(logo)}"/>
<text x="38" y="29" font-size="15" font-weight="bold" fill="#fff">OpenLoch — Elektronik-Werkstatt</text>
{bevel(872, 12, 24, 22)}{bevel(898, 12, 24, 22)}{bevel(924, 12, 24, 22)}
<path d="M878 28h11" stroke="#000" stroke-width="2"/>
<path d="M904 17h11v11h-11z M904 19h11 M930 17l11 11 M941 17l-11 11" fill="none" stroke="#000" stroke-width="2"/>
{''.join(menu_items)}
<path d="M9 70h942" stroke="#808080"/><path d="M9 71h942" stroke="#fff"/>
<path d="M12 76v20" stroke="#fff" stroke-width="2"/><path d="M14 76v20" stroke="#808080"/>
<image x="20" y="75" width="{toolbar_w}" height="{toolbar_h}" {PIXELATED} href="{data(pictures['toolbar'])}"/>
<path d="M9 99h942" stroke="#808080"/><path d="M9 100h942" stroke="#fff"/>
{sunken(9, body_top - 4, info_x - 15, body_h + 8, '#808080')}
{tiles}
{sunken(info_x, body_top - 4, info_w - 2, body_h + 8, '#fff')}
<image x="{info_x+18}" y="{body_top+12}" width="80" height="80" {PIXELATED} href="{data(logo)}"/>
<text x="{info_x+108}" y="{body_top+50}" font-size="26" font-weight="bold">OpenLoch</text>
<text x="{info_x+109}" y="{body_top+74}" font-size="13" fill="#404040">Elektronik-Werkstatt</text>
<rect x="{info_x+18}" y="{body_top+106}" width="{info_w-38}" height="24" fill="#000080"/>
<text x="{info_x+28}" y="{body_top+123}" font-size="11" font-weight="bold" fill="#fff" letter-spacing="0.5">VIER MODULE, EIN PROJEKT</text>
{module_items}
{feature_items}
{sunken(10, status_y, 600, 20, '#c0c0c0')}{sunken(616, status_y, 334, 20, '#c0c0c0')}
<text x="18" y="{status_y+15}" font-size="12">Vertraute Werkzeuge. Native Technik. Eigene Bauteile.</text>
<text x="624" y="{status_y+15}" font-size="12">C++20 / Qt 6 · macOS · Windows · Linux</text>
</g></svg>
'''
root.joinpath('assets/readme-header.svg').write_text(header, encoding='utf-8')

# The parts panel: classic and modern parts of the open library at the same scale, drawn by the program.
gap, x, items = 22, 0, []
baseline = 292
for entry, png in parts:
    w, h = entry['width'] // 2, entry['height'] // 2
    cell = max(w, int(len(entry['caption']) * 7.4))
    items.append((x, cell, w, h, entry['caption'], png))
    x += cell + gap
x -= gap
offset = (960 - x) // 2
cells = ''.join(
    f'<image x="{offset+px+(cell-w)/2:.0f}" y="{baseline-h}" width="{w}" height="{h}" href="{data(png)}"/>'
    f'<text x="{offset+px+cell/2:.0f}" y="{baseline+24}" text-anchor="middle" font-size="12">{caption}</text>'
    for px, cell, w, h, caption, png in items)
divider = offset + items[5][0] - gap // 2
panel = f'''<svg xmlns="http://www.w3.org/2000/svg" width="960" height="{baseline+86}" viewBox="0 0 960 {baseline+86}" role="img" aria-labelledby="ptitle pdescription">
<title id="ptitle">Bauteile der OpenLoch-Bibliothek</title>
<desc id="pdescription">Widerstand, LED, Elko, Transistor und DIL-IC wie in LochMaster, daneben ESP32-DevKitC, Raspberry Pi Pico, OLED-Anzeige, BME280-Sensor und A4988-Treiber aus den eigenen Seiten, maßstabsgetreu von OpenLoch gezeichnet.</desc>
<g font-family="Tahoma, Verdana, sans-serif" fill="#000">
{bevel(1, 1, 958, baseline+84)}
<rect x="8" y="8" width="944" height="26" fill="#000080"/>
<image x="13" y="13" width="16" height="16" href="{data(logo)}"/>
<text x="36" y="26" font-size="13" font-weight="bold" fill="#fff">Bauteile — OpenLoch-Bibliothek</text>
{sunken(10, 42, 940, baseline-6, '#fef5cf')}
<path d="M{divider} 60V{baseline+30}" stroke="#c8b98a" stroke-dasharray="4 4"/>
<text x="{offset}" y="66" font-size="12" fill="#806a30">wie im Original</text>
<text x="{divider+16}" y="66" font-size="12" fill="#806a30">eigene Seiten mit modernen Bauteilen</text>
{cells}
{sunken(10, baseline+44, 940, 22, '#c0c0c0')}
<text x="18" y="{baseline+60}" font-size="12">46 Seiten mit 506 Bauteilen wie im Original · 8 eigene Seiten mit 187 modernen Bauteilen · frei verwendbar (CC0)</text>
</g></svg>
'''
root.joinpath('assets/readme-parts.svg').write_text(panel, encoding='utf-8')

buttons = [('download', 'Downloads', 'icon-save'), ('roadmap', 'Projektplan', 'icon-undolist'), ('build', 'Selbst bauen', 'icon-kolben')]
for name, label, icon in buttons:
    button = f'''<svg xmlns="http://www.w3.org/2000/svg" width="160" height="38" viewBox="0 0 160 38" role="img" aria-label="{label}">
<title>{label}</title>{bevel(1, 1, 158, 36)}
<image x="{10 if icon == 'icon-kolben' else 14}" y="{3 if icon == 'icon-kolben' else 10}" width="{32 if icon == 'icon-kolben' else 16}" height="{32 if icon == 'icon-kolben' else 16}" {PIXELATED} href="{data(pictures[icon])}"/>
<text x="90" y="24" text-anchor="middle" font-family="Tahoma, Verdana, sans-serif" font-size="14" fill="#000">{label}</text>
</svg>'''
    root.joinpath(f'assets/readme-{name}.svg').write_text(button, encoding='utf-8')
print('Generated README header, parts panel and navigation buttons')
