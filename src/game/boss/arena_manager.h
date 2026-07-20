#pragma once
#include <vector>
#include <memory>
#include "types/boss_types.h"
#include "data/boss_defs.h"   // G2.3: ArenaEvent

class Player;
class Monster;
class BossDef;

// D5 Step4: ArenaManager — Boss战场动态元素 (G2.3 重构)
// 只维护 DangerZone 生命周期; 不参与 Boss 决策

class ArenaManager {
public:
    // ── G2.3: 统一入口 — BossSystemDirector 驱动 ──
    void execute_event(const ArenaEvent& ev, const BossArenaDef& cfg,
                       float boss_x, float boss_y, float player_x, float player_y);

    // ── 生命周期 ──
    void tick(float dt, Player* player, std::vector<std::unique_ptr<Monster>>& monsters);
    void draw(float cam_x, float cam_y) const;
    void clear();

    // ── 查询 ──
    const std::vector<DangerZone>& zones() const { return _zones; }
    int zone_count() const { return (int)_zones.size(); }

private:
    std::vector<DangerZone> _zones;

    // 辅助: 根据 danger_type 字符串创建 DangerZone
    static DangerZone _create_zone(const std::string& danger_type, float x, float y,
                                   float dur);
    // 辅助: 找一个避开玩家、偏向 Boss 前方的位置
    static void _calc_position(float bx, float by, float px, float py,
                               float radius, float& out_x, float& out_y);
};
