#!/usr/bin/env python
"""Extract all unique Chinese/CJK characters from source and JSON files."""
import os, sys

# Force UTF-8 output
if hasattr(sys.stdout, 'reconfigure'):
    sys.stdout.reconfigure(encoding='utf-8', errors='replace')

chars = set()

for top_dir in ['src', 'resources']:
    for root, dirs, files in os.walk(top_dir):
        for fname in files:
            if not (fname.endswith('.cpp') or fname.endswith('.h') or fname.endswith('.json')):
                continue
            fpath = os.path.join(root, fname)
            try:
                with open(fpath, 'r', encoding='utf-8') as fh:
                    text = fh.read()
            except UnicodeDecodeError:
                try:
                    with open(fpath, 'r', encoding='gbk') as fh:
                        text = fh.read()
                except Exception:
                    continue
            except Exception:
                continue
            for ch in text:
                cp = ord(ch)
                if cp >= 0x2000:
                    chars.add(ch)

sorted_chars = sorted(chars)
total = len(sorted_chars)

# ASCII
ascii_cps = list(range(0x20, 0x7F))

# Essential extra symbols
extra = [
    0x00B7,  # middle dot
    0x2014,  # em dash
    0x2190, 0x2191, 0x2192, 0x2193,  # arrows
    0x2500,  # box drawing
    0x2208,  # element of
    0x2260,  # not equal
    0x3002,  # ideographic full stop
    0x3010, 0x3011,  # black lenticular brackets
    0xFF01, 0xFF08, 0xFF09, 0xFF0C, 0xFF1A,  # fullwidth punctuation
]

all_cps = set(ascii_cps)
for e in extra:
    all_cps.add(e)
for c in sorted_chars:
    all_cps.add(ord(c))

sorted_all = sorted(all_cps)

# Write output file
out_path = os.path.join(os.path.dirname(__file__) or '.', 'font_codepoints.txt')
with open(out_path, 'w', encoding='utf-8') as out:
    out.write('Total unique CJK chars: {}\n'.format(total))
    out.write('Total codepoints (with ASCII+symbols): {}\n\n'.format(len(sorted_all)))
    for i in range(0, len(sorted_all), 20):
        group = sorted_all[i:i+20]
        line = ', '.join('0x{:04X}'.format(cp) for cp in group)
        if i + 20 < len(sorted_all):
            out.write('    ' + line + ',\n')
        else:
            out.write('    ' + line + '\n')

print('OK: wrote {} codepoints to {}'.format(len(sorted_all), out_path))

# Also dump to stdout
for i in range(0, len(sorted_all), 20):
    group = sorted_all[i:i+20]
    line = ', '.join('0x{:04X}'.format(cp) for cp in group)
    print(line)
