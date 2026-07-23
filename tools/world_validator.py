#!/usr/bin/env python3
"""
G7.1: World Validator — automatic cross-reference checker for all JSON resources.

Checks: JSON refs, dialogue next links, effect/risk DSL, empty buff strings,
        biome coverage, colon-delimited spawn/buff/debuff/equipment references.

Usage: python tools/world_validator.py [resources_dir]
"""

import json, os, sys
from collections import defaultdict

RES_DIR = sys.argv[1] if len(sys.argv) > 1 else "resources"
errors = []
warnings = []


def load_json(path: str):
    p = os.path.join(RES_DIR, path)
    if not os.path.exists(p):
        errors.append(f"MISSING: {path}")
        return None
    with open(p, "r", encoding="utf-8") as f:
        return json.load(f)


def ids_from(data, key="id") -> set:
    if isinstance(data, list):
        return {item[key] for item in data if key in item}
    if isinstance(data, dict):
        return set(data.keys())
    return set()


def err(msg): errors.append(msg)


def check_ref(ref, valid_set, label, context=""):
    if ref and ref not in valid_set:
        err(f"BROKEN REF: {label} '{ref}' not found {context}")


# ═══ Load ═══
print(f"[Validator] Scanning {RES_DIR}/ ...")

enemies    = load_json("enemies.json") or []
bosses     = load_json("bosses.json") or {}
skills     = load_json("skills.json") or []
buffs      = load_json("buffs.json") or []
relics     = load_json("relics.json") or []
items      = load_json("items.json") or []
biomes     = load_json("biomes.json") or []
landmarks  = load_json("landmarks.json") or []
hazards    = load_json("hazards.json") or []
encounters = load_json("encounters.json") or []
bio_events = load_json("biome_events.json") or []
vfx        = load_json("vfx_recipes.json") or {}
floor_cfg  = load_json("floor_config.json") or []
floor_nar  = load_json("floor_narrative.json") or []
quests     = load_json("quests.json") or []
dialogues  = load_json("dialogues.json") or []
endings    = load_json("endings.json") or []
meta_nodes = load_json("meta_nodes.json") or []

world_files = {}
for wf in ["world/prison.json", "world/volcano.json", "world/abyss.json"]:
    data = load_json(wf)
    if data:
        world_files[wf] = data

# ID sets
enemy_ids     = ids_from(enemies)
boss_ids      = ids_from(bosses) | {"5", "10", "15"}
skill_ids     = ids_from(skills)
buff_ids      = ids_from(buffs)
relic_ids     = ids_from(relics)
item_ids      = ids_from(items)
biome_ids     = ids_from(biomes)
landmark_ids  = ids_from(landmarks)
hazard_ids    = ids_from(hazards)
encounter_ids = ids_from(encounters)
vfx_recipes   = set(vfx.get("recipes", {}).keys()) if isinstance(vfx, dict) else set()

valid_bgm        = {"title", "select", "dungeon", "boss", "prison", "volcano", "abyss"}
valid_triggers   = {"floor_enter", "room_enter", "wall_interact", "entity_interact", "boss_dead", "item_use", "talk"}
valid_enc_types  = {"event", "npc", "trader", "quest_giver"}
valid_eff_kinds  = {"buff","relic","equipment","skill_level","heal","relic_from_pool","none","trade","set_flag","set_meta_flag"}
valid_risk_kinds = {"spawn","hp_loss","debuff","confuse","none"}

print(f"  {len(enemy_ids)} enemies, {len(boss_ids)} bosses, {len(skill_ids)} skills, "
      f"{len(buff_ids)} buffs, {len(relic_ids)} relics, {len(item_ids)} items, "
      f"{len(biome_ids)} biomes, {len(landmark_ids)} landmarks, {len(hazard_ids)} hazards, "
      f"{len(encounter_ids)} encounters")


# ═══ Step 1: Basic cross-references ═══
for b in biomes:
    ctx = f"biomes.json [{b['id']}]"
    for e in b.get("enemy_pool", []):
        check_ref(e, enemy_ids, f"{ctx} enemy_pool", ctx)
    check_ref(b.get("boss_id",""), boss_ids, f"{ctx} boss_id", ctx)
    check_ref(b.get("bgm",""), valid_bgm, f"{ctx} bgm", ctx)

for lm in landmarks:
    ctx = f"landmarks.json [{lm['id']}]"
    check_ref(lm["biome"], biome_ids, f"{ctx} biome", ctx)

for hz in hazards:
    ctx = f"hazards.json [{hz['id']}]"
    check_ref(hz["biome"], biome_ids, f"{ctx} biome", ctx)
    check_ref(hz["landmark_id"], landmark_ids, f"{ctx} landmark_id", ctx)

for enc in encounters:
    ctx = f"encounters.json [{enc['id']}]"
    check_ref(enc["biome"], biome_ids, f"{ctx} biome", ctx)
    check_ref(enc["trigger"], valid_triggers, f"{ctx} trigger", ctx)
    check_ref(enc["type"], valid_enc_types, f"{ctx} type", ctx)
    if enc.get("room"):
        check_ref(enc["room"], landmark_ids | {""}, f"{ctx} room (landmark)", ctx)
    node_ids = set()
    for node in enc.get("dialogue", []):
        node_ids.add(node["id"])
        for c in node.get("choices", []):
            eff = c.get("effect","none")
            rsk = c.get("risk","none")
            for val, label in [(eff,"effect"),(rsk,"risk")]:
                if val not in ("","none") and ":" in val:
                    check_ref(val.split(":")[0], valid_eff_kinds|valid_risk_kinds,
                              f"{ctx} node[{node['id']}] {label}.kind", ctx)
    for node in enc.get("dialogue", []):
        for c in node.get("choices", []):
            nxt = c.get("next","end")
            if nxt != "end" and nxt not in node_ids:
                err(f"BROKEN NEXT: {ctx} node[{node['id']}] '{c['text'][:20]}' → '{nxt}'")

if isinstance(bio_events, list):
    for bev in bio_events:
        ctx = f"biome_events.json [{bev['id']}]"
        check_ref(bev["biome"], biome_ids, f"{ctx} biome", ctx)

for fc in floor_cfg:
    f = fc.get("floor",0)
    if f < 1 or f > 15:
        err(f"floor_config.json: floor {f} out of range")

for fn in floor_nar:
    f = fn.get("floor",0)
    if f < 1 or f > 15:
        err(f"floor_narrative.json: floor {f} out of range")


# ═══ Step 2: Colon-delimited effect/risk refs ═══
def check_colon(val, idx, kind, valid_set, ctx):
    if val in ("","none"): return
    parts = val.split(":")
    if kind == "spawn":
        if len(parts) >= 2: check_ref(parts[1], valid_set, f"{ctx} spawn target", ctx)
    elif kind in ("buff","debuff"):
        if len(parts) >= 2: check_ref(parts[1], valid_set, f"{ctx} {kind} target", ctx)
    elif kind == "equipment":
        if len(parts) >= 2 and parts[1] not in ("rare","epic","legendary","common"):
            err(f"INVALID RARITY: '{val}' in {ctx}")
    elif kind not in ("potion","skill_level","relic","hp_loss","confuse","heal",
                      "relic_from_pool","set_flag","set_meta_flag","trade","none"):
        err(f"UNKNOWN TOKEN: '{kind}' in '{val}' at {ctx}")

for src_list in [encounters, bio_events]:
    if not isinstance(src_list, list): continue
    for item in src_list:
        ctx = f"[{item['id']}]"
        for node in item.get("dialogue", []):
            for c in node.get("choices", []):
                for fld in ("effect","risk"):
                    val = c.get(fld,"none")
                    if val not in ("","none") and ":" in val:
                        check_colon(val,0,val.split(":")[0],
                                    enemy_ids|buff_ids,
                                    f"{item['id']} node[{node['id']}] {fld}")
        for c in item.get("choices", []):
            for fld in ("effect","risk"):
                val = c.get(fld,"none")
                if val not in ("","none") and ":" in val:
                    check_colon(val,0,val.split(":")[0],
                                enemy_ids|buff_ids,
                                f"{item['id']} choice {fld}")


# ═══ Step 3: Enemy/skill/item internal refs ═══
for enemy in enemies:
    for eb in enemy.get("elite_buffs", []):
        bv = eb.get("buff","")
        if bv == "":
            err(f"EMPTY BUFF: enemies.json [{enemy['id']}] elite_buffs")
        elif bv not in buff_ids:
            err(f"BROKEN REF: enemies.json [{enemy['id']}] elite_buffs buff '{bv}'")
    for oh in enemy.get("on_hit", []):
        bv = oh.get("buff","")
        if bv and bv not in buff_ids:
            err(f"BROKEN REF: enemies.json [{enemy['id']}] on_hit buff '{bv}'")

for sk in skills:
    for tr in sk.get("triggers", []):
        bv = tr.get("buff","")
        if bv and bv not in buff_ids:
            err(f"BROKEN REF: skills.json [{sk['id']}] trigger buff '{bv}'")

for it in items:
    for ref_field, id_set, label in [("buff_id", buff_ids, "buff"), ("skill_id", skill_ids, "skill")]:
        if ref_field in it and it[ref_field]:
            check_ref(it[ref_field], id_set, f"items.json [{it['id']}] {ref_field}")

for wf_path, wf in world_files.items():
    b = wf.get("biome",{})
    if b.get("id","") not in biome_ids:
        err(f"BROKEN REF: {wf_path} biome.id '{b.get('id','')}'")
    for hz in wf.get("hazards", []):
        lm = hz.get("landmark", hz.get("landmark_id", ""))
        if lm: check_ref(lm, landmark_ids, f"{wf_path} hazard landmark", wf_path)


# ═══ Step 4: Coverage warnings ═══
for b in biomes:
    if not b.get("enemy_pool"): warnings.append(f"biomes.json [{b['id']}]: empty enemy_pool")
    if not b.get("boss_id"):    warnings.append(f"biomes.json [{b['id']}]: no boss_id")

lm_count = defaultdict(int)
for lm in landmarks: lm_count[lm["biome"]] += 1
for bid in biome_ids:
    if lm_count.get(bid,0) == 0:
        warnings.append(f"biome '{bid}': no landmarks")
    elif lm_count[bid] < 2:
        warnings.append(f"biome '{bid}': only {lm_count[bid]} landmark(s)")

enc_count = defaultdict(int)
for enc in encounters: enc_count[enc["biome"]] += 1
for bid in biome_ids:
    if enc_count.get(bid,0) == 0:
        warnings.append(f"biome '{bid}': no encounters")


# ═══ Report ═══
print(f"\n{'='*60}")
print(f"  WORLD VALIDATOR REPORT")
print(f"{'='*60}")
print(f"  Resources: {RES_DIR}")
print(f"  Files:    20+ JSON")
print(f"  Errors:   {len(errors)}")
print(f"  Warnings: {len(warnings)}")

if errors:
    print(f"\n  ═══ ERRORS ═══")
    for e in errors:
        print(f"  [ERR] {e}")

if warnings:
    print(f"\n  ═══ WARNINGS ═══")
    for w in warnings:
        print(f"  [WRN] {w}")

if not errors and not warnings:
    print(f"\n  All checks passed. World is valid.")

print(f"{'='*60}")
exit(0 if not errors else 1)
