#include "relic_effect_processor.h"
#include "combat_system.h"
#include "combat_stats.h"
#include "player.h"
#include "monster.h"
#include <algorithm>
#include <cmath>

float RelicEffectProcessor::_rng_float() {
    return (float)(rng() % 1000) / 1000.0f;
}

void RelicEffectProcessor::tick(Player* player, float dt) {
    if (!_enabled || !player) return;

    for (auto& relic : player->relics) {
        auto* def = get_relic_def(relic.id);
        if (!def || def->effects.empty()) continue;
        for (auto& eff : def->effects) {
            if (eff.trigger == RelicTrigger::PASSIVE) {
                _apply_passive_stat(player, eff);
            }
        }
    }
}

void RelicEffectProcessor::on_kill(
    Player* player, Monster* monster,
    std::vector<Monster*>& all_monsters)
{
    if (!_enabled || !player) return;

    for (auto& relic : player->relics) {
        auto* def = get_relic_def(relic.id);
        if (!def || def->effects.empty()) continue;
        for (auto& eff : def->effects) {
            if (eff.trigger == RelicTrigger::ON_KILL) {
                _apply_on_kill(player, monster, all_monsters, eff);
            }
        }
    }
}

void RelicEffectProcessor::on_hit(Player* player, Monster* target) {
    if (!_enabled || !player || !target) return;

    for (auto& relic : player->relics) {
        auto* def = get_relic_def(relic.id);
        if (!def || def->effects.empty()) continue;
        for (auto& eff : def->effects) {
            if (eff.trigger == RelicTrigger::ON_HIT) {
                _apply_on_hit(player, target, eff);
            }
        }
    }
}

void RelicEffectProcessor::on_pre_damage(
    DamageContext& ctx, Player* player)
{
    if (!_enabled || !player) return;

    for (auto& relic : player->relics) {
        auto* def = get_relic_def(relic.id);
        if (!def || def->effects.empty()) continue;
        for (auto& eff : def->effects) {
            if (eff.trigger == RelicTrigger::PRE_DAMAGE) {
                _apply_pre_damage(ctx, eff);
            }
        }
    }
}

void RelicEffectProcessor::on_hurt(Player* player, int final_damage) {
    (void)player;
    (void)final_damage;
}

void RelicEffectProcessor::on_floor_enter(Player* player) {
    if (!_enabled || !player) return;

    for (auto& relic : player->relics) {
        auto* def = get_relic_def(relic.id);
        if (!def || def->effects.empty()) continue;
        for (auto& eff : def->effects) {
            if (eff.trigger == RelicTrigger::ON_FLOOR_ENTER) {
                _apply_on_floor_enter(player, eff);
            }
        }
    }
}

void RelicEffectProcessor::_apply_passive_stat(
    Player* player, const RelicEffectDef& eff)
{
    if (eff.stat == "physical_defense") {
        player->combat.physical_defense += static_cast<int>(eff.value);
    } else if (eff.stat == "magical_defense") {
        player->combat.magical_defense += static_cast<int>(eff.value);
    } else if (eff.stat == "max_hp") {
        int bonus = static_cast<int>(eff.value);
        player->combat.max_hp += bonus;
        player->combat.current_hp += bonus;
    }
}

void RelicEffectProcessor::_apply_on_kill(
    Player* player, Monster* monster,
    std::vector<Monster*>& all_monsters,
    const RelicEffectDef& eff)
{
    if (_rng_float() > eff.chance) return;

    if (eff.type == RelicEffectType::DEAL_AOE_DAMAGE) {
        int dmg = static_cast<int>(
            player->combat.get_effective_attack() * eff.value);
        for (auto& m : all_monsters) {
            if (m != monster && m->combat.is_alive) {
                m->combat.take_damage(dmg);
            }
        }
    } else if (eff.type == RelicEffectType::HEAL) {
        int heal = static_cast<int>(
            eff.value * player->combat.max_hp);
        player->combat.heal(heal);
    } else if (eff.type == RelicEffectType::ADD_BUFF) {
        apply_buff(player, eff.buff_id, eff.value2);
    }
}

void RelicEffectProcessor::_apply_on_hit(
    Player* player, Monster* target,
    const RelicEffectDef& eff)
{
    (void)player;
    if (_rng_float() > eff.chance) return;

    if (eff.type == RelicEffectType::ADD_BUFF) {
        apply_buff(target, eff.buff_id, eff.value2);
    }
}

void RelicEffectProcessor::_apply_pre_damage(
    DamageContext& ctx, const RelicEffectDef& eff)
{
    if (eff.type == RelicEffectType::DAMAGE_REDUCTION) {
        if (_rng_float() < eff.chance) {
            ctx.final_damage = static_cast<int>(
                ctx.final_damage * (1.0f - eff.value));
        }
    }
}

void RelicEffectProcessor::_apply_on_floor_enter(
    Player* player, const RelicEffectDef& eff)
{
    if (eff.type == RelicEffectType::ADD_BUFF) {
        apply_buff(player, eff.buff_id, eff.value2);
    }
}

// ── Static entry points ──

void RelicEffectProcessor::static_on_hit(Player* player, Monster* target) {
    if (!player || !target) return;
    for (auto& relic : player->relics) {
        auto* def = get_relic_def(relic.id);
        if (!def || def->effects.empty()) continue;
        for (auto& eff : def->effects) {
            if (eff.trigger == RelicTrigger::ON_HIT) {
                if ((float)(rng() % 1000) / 1000.0f > eff.chance) continue;
                if (eff.type == RelicEffectType::ADD_BUFF) {
                    apply_buff(target, eff.buff_id, eff.value2);
                }
            }
        }
    }
}

void RelicEffectProcessor::static_on_kill(Player* player, Monster* monster) {
    if (!player || !monster) return;
    for (auto& relic : player->relics) {
        auto* def = get_relic_def(relic.id);
        if (!def || def->effects.empty()) continue;
        for (auto& eff : def->effects) {
            if (eff.trigger == RelicTrigger::ON_KILL) {
                if ((float)(rng() % 1000) / 1000.0f > eff.chance) continue;
                if (eff.type == RelicEffectType::HEAL) {
                    int heal = static_cast<int>(eff.value * player->combat.max_hp);
                    player->combat.heal(heal);
                } else if (eff.type == RelicEffectType::ADD_BUFF) {
                    apply_buff(player, eff.buff_id, eff.value2);
                }
            }
        }
    }
}

void RelicEffectProcessor::static_on_pre_damage(
    DamageContext& ctx, Player* player)
{
    if (!player) return;
    for (auto& relic : player->relics) {
        auto* def = get_relic_def(relic.id);
        if (!def || def->effects.empty()) continue;
        for (auto& eff : def->effects) {
            if (eff.trigger == RelicTrigger::PRE_DAMAGE) {
                if (eff.type == RelicEffectType::DAMAGE_REDUCTION) {
                    if ((float)(rng() % 1000) / 1000.0f < eff.chance) {
                        ctx.final_damage = static_cast<int>(
                            ctx.final_damage * (1.0f - eff.value));
                    }
                }
            }
        }
    }
}
