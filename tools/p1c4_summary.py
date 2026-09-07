import json

tot = {'win': 0, 'n': 0, 'f1': 0, 'twall': 0, 'dot': 0, 'mon': 0, 'boss': 0,
       'deep': 0, 'dmg': 0.0, 'kills': 0.0, 'picks': 0.0, 'tw': 0.0,
       'heal': 0.0, 'tgame': 0.0, 'stuck': 0.0, 'env': 0.0}
for s in [3, 7, 11, 19, 23]:
    with open(f'reports/p1c4/p1c4_s{s}.json', encoding='utf-8') as f:
        j = json.load(f)
    su = j['summary']
    tot['win'] += su['total_wins']
    tot['n'] += su['total_runs']
    tot['f1'] += su['death_floor_distribution'][0]
    tot['twall'] += su['outcome_dist']['TIMEOUT_WALL']
    tot['dot'] += su['outcome_dist']['DEATH_DOT']
    tot['mon'] += su['outcome_dist']['DEATH_MONSTER']
    tot['boss'] += su['outcome_dist']['DEATH_BOSS']
    tot['env'] += su['outcome_dist']['DEATH_ENVIRONMENT']
    tot['tgame'] += su['outcome_dist']['TIMEOUT_GAME']
    tot['deep'] += sum(su['death_floor_distribution'][2:])
    tot['dmg'] += su['avg_damage_dealt']
    tot['heal'] += su['avg_heal']
    tot['tw'] += su['avg_turns']
    n_runs = len(j['runs'])
    tot['kills'] += sum(r['kills'] for r in j['runs']) / n_runs
    tot['picks'] += sum(r['picks'] for r in j['runs']) / n_runs
    tot['stuck'] += sum(r['stuck_tp'] for r in j['runs']) / n_runs

n = tot['n']
print(f"total={n} wins={tot['win']} ({100.0*tot['win']/n:.1f}%)")
print(f"F1 deaths={tot['f1']} ({100.0*tot['f1']/n:.1f}%)  deep(F3+)={tot['deep']}")
print(f"TWall={tot['twall']}  DOT={tot['dot']}  MON={tot['mon']}  BOSS={tot['boss']}  ENV={tot['env']}  TGame={tot['tgame']}")
print(f"avg dmg={tot['dmg']/5:.0f} kills={tot['kills']/5:.2f} picks={tot['picks']/5:.2f} turns={tot['tw']/5:.0f} heal={tot['heal']/5:.0f} stuck_tp={tot['stuck']/5:.1f}")
