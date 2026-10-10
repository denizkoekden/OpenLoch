#!/usr/bin/env python3
"""Verify that all Mach-O dependencies resolve inside a bundle or to macOS, and that the bundle declares the document
types the Finder opens with OpenLoch."""
from pathlib import Path
import argparse, plistlib, re, subprocess

# The extensions OpenLoch opens (src/suite/documents.cpp), without backups.
DOCUMENTS = ['openloch', 'olsch', 'olpcb', 'olfp', 'spl7', 'spl8', 'lm4', 'lmb', 'lay6', 'lay', 'fpl', 'lib']

def document_types(bundle):
    info = plistlib.loads((bundle / 'Contents/Info.plist').read_bytes())
    declared = {}
    for key in ('UTExportedTypeDeclarations', 'UTImportedTypeDeclarations'):
        for d in info.get(key, []):
            for ext in d.get('UTTypeTagSpecification', {}).get('public.filename-extension', []):
                declared[ext] = d['UTTypeIdentifier']
    handled = {uti for t in info.get('CFBundleDocumentTypes', []) for uti in t.get('LSItemContentTypes', [])}
    return [f'Info.plist: .{ext} not declared' for ext in DOCUMENTS if ext not in declared] + \
           [f'Info.plist: .{ext} ({declared[ext]}) not opened' for ext in DOCUMENTS if ext in declared and declared[ext] not in handled]

def main():
    ap=argparse.ArgumentParser();ap.add_argument('bundle',type=Path);args=ap.parse_args()
    bundle=args.bundle.resolve();executable=bundle/'Contents/MacOS';failures=[];checked=0
    for path in bundle.rglob('*'):
        if not path.is_file() or path.is_symlink():continue
        with path.open('rb') as f:magic=f.read(4)
        if magic not in (b'\xcf\xfa\xed\xfe',b'\xce\xfa\xed\xfe',b'\xca\xfe\xba\xbe',b'\xca\xfe\xba\xbf'):continue
        checked+=1
        deps=subprocess.check_output(['otool','-L',str(path)],text=True).splitlines()[1:]
        commands=subprocess.check_output(['otool','-l',str(path)],text=True)
        rpaths=re.findall(r'cmd LC_RPATH\s+cmdsize \d+\s+path (.*?) \(offset',commands)
        own_id=re.search(r'cmd LC_ID_DYLIB\s+cmdsize \d+\s+name (.*?) \(offset',commands)
        def expand(value):return value.replace('@executable_path',str(executable)).replace('@loader_path',str(path.parent))
        for line in deps:
            dep=line.strip().split(' (compatibility')[0]
            if not dep or dep.endswith(':'):continue
            # otool lists a dylib's own install name before its dependencies.
            if own_id and dep==own_id.group(1):continue
            if dep.startswith(('/usr/lib/','/System/Library/')):continue
            if dep.startswith('@rpath/'):
                targets=[Path(expand(root))/dep[7:] for root in rpaths]
                targets+=[bundle/'Contents/Frameworks'/dep[7:]]
            else:targets=[Path(expand(dep))]
            if not any(target.exists() and target.resolve().is_relative_to(bundle) for target in targets):
                failures.append(f'{path.relative_to(bundle)}: {dep}')
    failures+=document_types(bundle)
    if failures:print('\n'.join(failures));return 1
    print(f'{checked} Mach-O files checked; all dependencies resolve inside the bundle or to macOS; {len(DOCUMENTS)} document types declared')
    return 0

if __name__=='__main__':raise SystemExit(main())
