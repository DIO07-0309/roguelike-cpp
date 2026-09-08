import json

ws = json.load(open('resources/weapons.json', encoding='utf-8'))
if isinstance(ws, dict):
    items = ws.get('weapons', list(ws.values()))
else:
    items = ws
for w in items:
    wid = w.get('id', '?')
    wtype = w.get('type', '?')
    ranges = [(s.get('range'), s.get('recovery'), s.get('hit_shape'))
              for s in w.get('stages', [])]
    print(f"{wid:20s} {wtype:9s} stages={ranges}")
