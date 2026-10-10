#!/usr/bin/env python3
"""Package the generated icon into macOS and Windows resources (run on macOS)."""
from pathlib import Path
import struct
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = root / 'assets/app-icon-source.png'
with tempfile.TemporaryDirectory(prefix='openloch-icon-') as temporary:
    iconset = Path(temporary) / 'OpenLoch.iconset'
    iconset.mkdir()
    for logical in (16, 32, 128, 256, 512):
        for scale in (1, 2):
            name = f'icon_{logical}x{logical}' + ('@2x' if scale == 2 else '') + '.png'
            subprocess.run(['/usr/bin/sips', '-z', str(logical * scale), str(logical * scale), str(source), '--out', str(iconset / name)], check=True, capture_output=True)
    subprocess.run(['/usr/bin/iconutil', '-c', 'icns', str(iconset), '-o', str(root / 'assets/OpenLoch.icns')], check=True)
    root.joinpath('assets/openloch.png').write_bytes(iconset.joinpath('icon_512x512@2x.png').read_bytes())
    images = [(size, iconset.joinpath(f'icon_{size}x{size}.png').read_bytes()) for size in (16, 32, 128, 256)]
    offset = 6 + 16 * len(images)
    entries = []
    for size, data in images:
        entries.append(struct.pack('<BBBBHHII', size % 256, size % 256, 0, 0, 1, 32, len(data), offset))
        offset += len(data)
    root.joinpath('assets/OpenLoch.ico').write_bytes(struct.pack('<HHH', 0, 1, len(images)) + b''.join(entries) + b''.join(data for _, data in images))
print('Generated OpenLoch.icns, OpenLoch.ico and openloch.png')
