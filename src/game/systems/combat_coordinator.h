#pragma once
#include <vector>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>

class Player;
class Monster;
class GameMap;
class AudioServer;

// from item.h
struct DroppedItem;

// from vfx_server.h
struct Effect;

// ============================================================
// CombatCoordinator — 战斗相关的跨系统调度
// 接入现有 CombatSystem，不重复实现伤害公式
// ============================================================
class CombatCoordinator {
public:
    // 玩家普攻 (音频 + VFX + 伤害 + 清理)
    static void player_attack(Player* player, std::vector<std::unique_ptr<Monster>>& monsters,
                              std::vector<Effect>& effects, AudioServer* audio);

    // 施加普攻伤害并播放 VFX (不处理击杀结算)
    static void apply_attack_damage(Monster* target, int dmg,
                                     std::vector<Effect>& effects, AudioServer* audio);

    // 技能释放 (含时停special-case + 伤害队列 + SFX + VFX + D2 heavy强化)
    static std::string use_skill(int index, Player* player,
                                  std::vector<std::unique_ptr<Monster>>& monsters,
                                  GameMap* map, std::vector<Effect>& effects,
                                  AudioServer* audio, double game_time,
                                  float& time_stop_remaining,
                                  std::vector<std::pair<Monster*, int>>& pending_damage,
                                  bool is_heavy = false);

    // D2 Step2: 技能命中后的 heavy 增强 VFX
    static void skill_heavy_vfx(Player* player, const std::string& skill_name,
                                 std::vector<Effect>& effects);

    // 怪物击杀结算 (XP/升级/技能习得/Boss奖励/圣物效果)
    static void on_monster_killed(Monster* m, Player* player,
                                   std::vector<std::unique_ptr<Monster>>& monsters,
                                   std::vector<DroppedItem>& ground_items,
                                   AudioServer* audio);

    // 清理死亡怪物
    static void cleanup_dead_monsters(std::vector<std::unique_ptr<Monster>>& monsters,
                                       Player* player,
                                       std::vector<DroppedItem>& ground_items,
                                       AudioServer* audio);

    // 应用时停暂存伤害
    static void apply_pending_damage(std::vector<std::pair<Monster*, int>>& pending,
                                      std::vector<std::unique_ptr<Monster>>& monsters,
                                      Player* player,
                                      std::vector<DroppedItem>& ground_items,
                                      AudioServer* audio);
};
