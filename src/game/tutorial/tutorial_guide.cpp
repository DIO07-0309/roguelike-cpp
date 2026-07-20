#include "tutorial_guide.h"
#include "player.h"
#include "monster.h"
#include "item.h"
#include "skill.h"
#include "game_map.h"
#include "ai.h"
#include "config.h"
#include <cmath>
#include <algorithm>

void TutorialGuide::notify_skill_used() { _skill_used = true; }

// ---- stage instructions ----
std::vector<std::string> TutorialGuide::get_instructions() const {
    switch (stage) {
    case TutorialStage::WELCOME:
        return {
            "欢迎来到 Roguelike 新手教程！",
            "",
            "WASD 移动  空格 攻击  E 拾取  I 背包",
            "1-4 技能  X 装备  U 使用  D 丢弃",
            "T/Esc 退出教程",
            "",
            "按 Enter 开始练习！"
        };
    case TutorialStage::MOVE:
        return {
            "第1步：移动",
            "W 上  S 下  A 左  D 右",
            "试试走几步吧！"
        };
    case TutorialStage::ATTACK:
        return {
            "第2步：普通攻击",
            "走近绿色木桩，按 空格 攻击！",
            "(金色圆圈 = 攻击范围)"
        };
    case TutorialStage::PICKUP:
        return {
            "第3步：拾取物品",
            "走过去按 E 键捡起地面物品！"
        };
    case TutorialStage::INVENTORY:
        return {
            "第4步：背包与使用物品",
            "按 I 打开背包 → 上下选择药水",
            "→ 按 U 使用它来回复血量！"
        };
    case TutorialStage::EQUIP:
        return {
            "第5步：装备武器",
            "按 I 打开背包 → 选中武器",
            "→ 按 X 装备它！",
            "装备后左上角 ATK/DEF 会变化"
        };
    case TutorialStage::SKILL:
        return {
            "第6步：使用技能",
            "你已习得【斩击】技能",
            "走到木桩旁按 数字 1 释放！"
        };
    case TutorialStage::COMPLETE:
        return {
            "恭喜完成所有训练！",
            "WASD移动 | 空格攻击 | 1-4技能",
            "E拾取 | I背包 | X装备 | U使用",
            "",
            "按 Enter 返回标题，开始冒险！"
        };
    }
    return {};
}

// ---- check and advance ----
void TutorialGuide::check_and_advance(Player* p, bool inv_open,
                                       std::vector<Monster*>& monsters,
                                       std::vector<DroppedItem>& items) {
    switch (stage) {
    case TutorialStage::MOVE:     _check_move(p); break;
    case TutorialStage::ATTACK:    _check_attack(monsters); break;
    case TutorialStage::PICKUP:    _check_pickup(items); break;
    case TutorialStage::INVENTORY: _check_inventory(p); break;
    case TutorialStage::EQUIP:     _check_equip(p); break;
    case TutorialStage::SKILL:     _check_skill(); break;
    default: break;
    }
}

void TutorialGuide::advance_stage() {
    switch (stage) {
    case TutorialStage::WELCOME:   stage = TutorialStage::MOVE; break;
    case TutorialStage::MOVE:      stage = TutorialStage::ATTACK; attack_hits = 0; break;
    case TutorialStage::ATTACK:    stage = TutorialStage::PICKUP; break;
    case TutorialStage::PICKUP:    stage = TutorialStage::INVENTORY; break;
    case TutorialStage::INVENTORY: stage = TutorialStage::EQUIP; break;
    case TutorialStage::EQUIP:     stage = TutorialStage::SKILL; break;
    case TutorialStage::SKILL:     stage = TutorialStage::COMPLETE; break;
    default: break;
    }
}

void TutorialGuide::_check_move(Player* p) {
    float px = p->entity.position.x, py = p->entity.position.y;
    if (!_last_set) { _last_px = px; _last_py = py; _last_set = true; return; }
    move_distance += std::hypot(px - _last_px, py - _last_py);
    _last_px = px; _last_py = py;
    if (move_distance > 120) advance_stage();
}

void TutorialGuide::_check_attack(std::vector<Monster*>& monsters) {
    for (auto& m : monsters) {
        // 当前HP低于最大HP = 被攻击过
        if (m->combat.current_hp >= 0 && m->combat.current_hp < m->combat.max_hp) {
            attack_hits++;
            // 不重置HP! 这样木桩显示上也能看到红色血条变化, 玩家看到反馈
            break;
        }
    }
    if (attack_hits >= 1) advance_stage();
}

void TutorialGuide::_check_pickup(std::vector<DroppedItem>& items) {
    if (items.size() < 2) advance_stage();  // 初始2个, 捡了至少1个
}

void TutorialGuide::_check_inventory(Player* p) {
    // 教程初始HP=50, 使用药水后HP>50说明用过了
    // 注意: item_used 标记在背包面板U键操作后由engine设置
    if (item_used) advance_stage();
}

void TutorialGuide::_check_equip(Player* p) {
    if (p->inventory.equipped["weapon"]) advance_stage();
}

void TutorialGuide::_check_skill() {
    if (_skill_used) advance_stage();
}

// ---- factory ----
std::shared_ptr<GameMap> build_tutorial_map() {
    int w = 12, h = 9;
    auto gm = std::make_shared<GameMap>(w, h, TILE_SIZE);
    std::vector<std::string> tmpl;
    for (int y = 0; y < h; y++) {
        std::string line;
        for (int x = 0; x < w; x++) {
            if (y == 0 || y == h - 1 || x == 0 || x == w - 1) line += '#';
            else line += '.';
        }
        tmpl.push_back(line);
    }
    gm->load_from_template(tmpl);
    return gm;
}

class DummyAI : public MonsterAI {
public:
    DummyAI() : MonsterAI(0, 0, 999) {}
    void update(Monster*, Player*, GameMap*, double, double,
                std::vector<Monster*>*, std::vector<Effect>*) override {}
};

std::unique_ptr<Monster> create_tutorial_dummy(int tx, int ty) {
    auto m = std::make_unique<Monster>(tx * TILE_SIZE, ty * TILE_SIZE,
        "训练木桩", 9999, 0, 0, 0, Color{60, 180, 60, 255}, new DummyAI());
    return m;
}

std::vector<DroppedItem> create_tutorial_items(int tx, int ty) {
    std::vector<DroppedItem> items;
    items.push_back({std::make_shared<ConsumableItem>("初级生命药水", Rarity::COMMON, "heal", 20), tx, ty});
    items.push_back({std::make_shared<EquipmentItem>("训练用短剑", Rarity::COMMON, "weapon", 4, 1, 0), tx + 1, ty});
    return items;
}

void give_tutorial_skill(Player* p) {
    // G3.2: SkillFactory (fallback to direct SlashSkill if registry not loaded)
    auto sk = skill_factory_create("slash");
    if (!sk) sk = std::make_unique<SlashSkill>();
    p->skills.learn(std::move(sk));
}
