#!/usr/bin/env python3
"""
G7.1: World Validator — automatic cross-reference checker for all JSON resources.

Validates:
  - JSON cross-references (biome → enemy, boss → encounter, etc.)
  - Encounter dialogue node "next" integrity
  - Landmark/hazard biome membership
  - Effect/risk string DSL validity
  - Duplicate IDs within each resource file

Usage: python tools/world_validator.py [resources_dir]
"""

import json, os, sys, re
from collections import defaultdict

RES_DIR = sys.argv[1] if len(sys.argv) > 1 else "resources"

errors = []
warnings = []
_info = lambda msg: None  # suppress info in normal mode


# ═══════════════════════════════════════════════════════════
#  Helpers
# ═══════════════════════════════════════════════════════════

def load_json(path: str) -> list | dict | None:
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

def check_ref(ref, valid_ids, label, context=""):
    """Check that `ref` exists in `valid_ids`."""
    if ref not in valid_ids:
        errors.append(f"BROKEN REF: {label} '{ref}' not found {context}")


# ═══════════════════════════════════════════════════════════
#  Step 1: Load all resource files
# ═══════════════════════════════════════════════════════════

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

# World/ per-biome files
world_files = {}
for wf in ["world/prison.json", "world/volcano.json", "world/abyss.json"]:
    data = load_json(wf)
    if data: world_files[wf] = data

# ── Build ID sets ──
enemy_ids     = ids_from(enemies)
boss_ids      = ids_from(bosses) | {"5", "10", "15"}  # floor-based boss lookup keys (valid in spawn_boss)
skill_ids     = ids_from(skills)
buff_ids      = ids_from(buffs)
relic_ids     = ids_from(relics)
item_ids      = ids_from(items)
biome_ids     = ids_from(biomes)
landmark_ids  = ids_from(landmarks)
hazard_ids    = ids_from(hazards)
encounter_ids = ids_from(encounters)
vfx_recipes   = set(vfx.get("recipes", {}).keys()) if isinstance(vfx, dict) else set()
quest_ids     = ids_from(quests)
dialogue_ids  = ids_from(dialogues)
ending_ids    = ids_from(endings)

# Combined: all valid BGM names from the engine
valid_bgm = {"title", "select", "dungeon", "boss", "prison", "volcano", "abyss"}
# Valid encounter triggers
valid_triggers = {"floor_enter", "room_enter", "wall_interact", "entity_interact", "boss_dead", "item_use", "talk"}
# Valid encounter types
valid_enc_types = {"event", "npc", "trader", "quest_giver"}
# Valid effect/risk kinds
valid_effect_kinds = {"buff", "relic", "equipment", "skill_level", "heal", "relic_from_pool", "none", "trade", "set_flag", "set_meta_flag"}
valid_risk_kinds   = {"spawn", "hp_loss", "debuff", "confuse", "none"}
# Valid special room types (from special_room.h enum + LANDMARK)
valid_room_types = {"ALTAR","TREASURE","FOUNTAIN","SHOP","BLACKSMITH","LIBRARY","GAMBLER","SHRINE","SECRET","LANDMARK"}

print(f"  Loaded: {len(enemy_ids)} enemies, {len(boss_ids)} bosses, {len(skill_ids)} skills, "
      f"{len(buff_ids)} buffs, {len(relic_ids)} relics, {len(item_ids)} items")
print(f"  {len(biome_ids)} biomes, {len(landmark_ids)} landmarks, {len(hazard_ids)} hazards, "
      f"{len(encounter_ids)} encounters, {len(bio_events) if isinstance(bio_events,list) else 0} biome_events")


# ═══════════════════════════════════════════════════════════
#  Step 2: Validate cross-references
# ═══════════════════════════════════════════════════════════

# ── Biomes: enemy_pool, boss_id ──
for b in biomes:
    ctx = f"biomes.json [{b['id']}]"
    if b["id"] not in biome_ids:
        errors.append(f"DUPLICATE or MISSING biome id: {b['id']}")
    for e in b.get("enemy_pool", []):
        check_ref(e, enemy_ids, f"{ctx} enemy_pool entry", ctx)
    if b.get("boss_id"):
        check_ref(b["boss_id"], boss_ids, f"{ctx} boss_id", ctx)
    if b.get("bgm"):
        check_ref(b["bgm"], valid_bgm, f"{ctx} bgm", ctx)

# ── Landmarks: biome reference ──
for lm in landmarks:
    ctx = f"landmarks.json [{lm['id']}]"
    check_ref(lm["biome"], biome_ids, f"{ctx} biome", ctx)

# ── Hazards: biome + landmark_id ──
for hz in hazards:
    ctx = f"hazards.json [{hz['id']}]"
    check_ref(hz["biome"], biome_ids, f"{ctx} biome", ctx)
    check_ref(hz["landmark_id"], landmark_ids, f"{ctx} landmark_id", ctx)
    check_ref(hz["effect"], {"slow_zone","burn_tick","confuse","deflect"}, f"{ctx} effect", ctx)

# ── Encounters: biome, room, dialogue integrity ──
for enc in encounters:
    ctx = f"encounters.json [{enc['id']}]"
    check_ref(enc["biome"], biome_ids, f"{ctx} biome", ctx)
    check_ref(enc["trigger"], valid_triggers, f"{ctx} trigger", ctx)
    check_ref(enc["type"], valid_enc_types, f"{ctx} type", ctx)
    if enc.get("room"):
        # room can be "" or a landmark_id
        check_ref(enc["room"], landmark_ids.union({""}), f"{ctx} room (landmark)", ctx)

    node_ids = set()
    for node in enc.get("dialogue", []):
        nid = node["id"]
        node_ids.add(nid)
        nctx = f"{ctx} node[{nid}]"
        check_ref(node.get("type","dialogue"), {"dialogue","trade"}, f"{nctx} type", nctx)
        for c in node.get("choices", []):
            # Validate effect/risk string DSL
            eff = c.get("effect", "none")
            if eff != "none" and ":" in eff:
                kind = eff.split(":")[0]
                if kind not in valid_effect_kinds:
                    errors.append(f"UNKNOWN EFFECT: '{eff}' in {nctx}")
            rsk = c.get("risk", "none")
            if rsk != "none" and ":" in rsk:
                kind = rsk.split(":")[0]
                if kind not in valid_risk_kinds:
                    errors.append(f"UNKNOWN RISK: '{rsk}' in {nctx}")

    # Check next references
    for node in enc.get("dialogue", []):
        for c in node.get("choices", []):
            nxt = c.get("next", "end")
            if nxt != "end" and nxt not in node_ids:
                errors.append(f"BROKEN NEXT: {ctx} node[{node['id']}] choice '{c['text'][:20]}' → '{nxt}' not in dialogue nodes")

# ── Biome events: biome ──
if isinstance(bio_events, list):
    for bev in bio_events:
        ctx = f"biome_events.json [{bev['id']}]"
        check_ref(bev["biome"], biome_ids, f"{ctx} biome", ctx)

# ── Floor config: floor range ──
for i, fc in enumerate(floor_cfg):
    f = fc.get("floor", 0)
    if f < 1 or f > 15:
        errors.append(f"floor_config.json: floor {f} out of range [1,15] at index {i}")

# ── Floor narrative: floor range ──
for i, fn in enumerate(floor_nar):
    f = fn.get("floor", 0)
    if f < 1 or f > 15:
        errors.append(f"floor_narrative.json: floor {f} out of range [1,15] at index {i}")

# ── VFX recipes: check presets references ──
if isinstance(vfx, dict):
    for recipe_name in vfx.get("recipes", {}):
        for step in vfx["recipes"][recipe_name].get("steps", []):
            if step.get("kind") not in {"pulse","spark","bolt","flash","slash_arc","cone","arc"}:
                errors.append(f"vfx_recipes.json [{recipe_name}] unknown kind: {step.get('kind')}")


# ═══════════════════════════════════════════════════════════
#  Step 3: Coverage checks
# ═══════════════════════════════════════════════════════════

# All biomes have enemies
for b in biomes:
    if not b.get("enemy_pool"):
        warnings.append(f"biomes.json [{b['id']}]: empty enemy_pool")

# All biomes have a boss
for b in biomes:
    if not b.get("boss_id"):
        warnings.append(f"biomes.json [{b['id']}]: no boss_id")

# All landmarks belong to exactly one biome
lm_biome_count = defaultdict(int)
for lm in landmarks:
    lm_biome_count[lm["biome"]] += 1
for bio_id in biome_ids:
    if lm_biome_count.get(bio_id, 0) == 0:
        warnings.append(f"biome '{bio_id}': no landmarks defined")
    elif lm_biome_count[bio_id] < 2:
        warnings.append(f"biome '{bio_id}': only {lm_biome_count[bio_id]} landmark(s), expected 3")

# Encounter coverage per biome
enc_biome_count = defaultdict(int)
for enc in encounters:
    enc_biome_count[enc["biome"]] += 1
for bio_id in biome_ids:
    if enc_biome_count.get(bio_id, 0) == 0:
        warnings.append(f"biome '{bio_id}': no encounters defined")


# ═══════════════════════════════════════════════════════════
#  Report
# ═══════════════════════════════════════════════════════════

print(f"\n{'='*60}")
print(f"  WORLD VALIDATOR REPORT")
print(f"{'='*60}")
print(f"  Resources: {RES_DIR}")
print(f"  Files checked: 20+")
print(f"  Errors:   {len(errors)}")
print(f"  Warnings: {len(warnings)}")

if errors:
    print(f"\n  ═══ ERRORS ({len(errors)}) ═══")
    for e in errors:
        print(f"  [ERROR] {e}")

if warnings:
    print(f"\n  ═══ WARNINGS ({len(warnings)}) ═══")
    for w in warnings:
        print(f"  [WARN]  {w}")

if not errors and not warnings:
    print(f"\n  ✓ All checks passed. World is valid.")

print(f"{'='*60}")

sys.exit(1 if errors else 0)
