import json

# P1-C7-A v2 迁移验证: 汇总 5 种子 x N 局 JSON (p1c7a 目录)
import sys
prefix = sys.argv[1] if len(sys.argv) > 1 else 'reports/p1c7a/p1c7a_v2_s'
seeds = [int(s) for s in sys.argv[2].split(',')] if len(sys.argv) > 2 else [3, 7, 11, 19, 23]

tot = {'win': 0, 'n': 0, 'f1': 0, 'deep': 0, 'dmg': 0.0, 'floor': 0.0,
       'kills': 0.0, 'picks': 0.0, 'twall': 0, 'dot': 0, 'mon': 0, 'boss': 0}
runs_counted = 0
for s in seeds:
    path = f'{prefix}{s}.json'
    try:
        with open(path, encoding='utf-8') as f:
            j = json.load(f)
    except FileNotFoundError:
        print(f'  (missing: {path})')
        continue
    su = j['summary']
    n_runs = len(j['runs'])
    tot['win'] += su['total_wins']
    tot['n'] += su['total_runs']
    tot['f1'] += su['death_floor_distribution'][0]
    tot['deep'] += sum(su['death_floor_distribution'][2:])
    tot['dmg'] += su['avg_damage_dealt']
    tot['floor'] += su.get('avg_floor', 0)
    for k, o in [('twall', 'TIMEOUT_WALL'), ('dot', 'DEATH_DOT'),
                 ('mon', 'DEATH_MONSTER'), ('boss', 'DEATH_BOSS')]:
        tot[k] += su['outcome_dist'].get(o, 0)
    tot['kills'] += sum(r['kills'] for r in j['runs']) / n_runs
    tot['picks'] += sum(r['picks'] for r in j['runs']) / n_runs
    runs_counted += 1

if runs_counted == 0:
    print('no data')
else:
    n = tot['n']
    print(f"total={n} wins={tot['win']} ({100.0*tot['win']/n:.1f}%)")
    print(f"F1 deaths={tot['f1']} ({100.0*tot['f1']/n:.1f}%)  deep(F3+)={tot['deep']}")
    print(f"TWall={tot['twall']}  DOT={tot['dot']}  MON={tot['mon']}  BOSS={tot['boss']}")
    print(f"avg floor={tot['floor']/runs_counted:.2f} dmg={tot['dmg']/runs_counted:.0f} "
          f"kills={tot['kills']/runs_counted:.2f} picks={tot['picks']/runs_counted:.2f}")
