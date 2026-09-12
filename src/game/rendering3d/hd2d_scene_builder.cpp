// M6-HD2D: 场景构建器实现 — GameScene 只读状态 → HD2DDrawItem 列表
// 切片范围: 地板/墙 tile (纯色起步) + 玩家/怪 billboard (贴图) + 特效
// 红线: 只读 gs; 无 gameplay 副作用; 视觉随机只吃 visual_rng (本文件未用随机)
#include "hd2d_scene_builder.h"
#include "scenes/game_scene.h"
#include "world/game_map.h"
#include "world/challenge_room.h"              // M6-v2a: ChallengePhase
#include "entities/player.h"
#include "entities/monster.h"
#include "entities/item.h"                    // M6-v2a: item_icon_key
#include "systems/weapon_component.h"         // M6-v2b: WeaponType/range_indicator
#include "world/npc_system.h"                 // M6-v2a: npc_sprite_key
#include "entities/boss.h"                    // M6-v2b: BossAI 技能预警只读
#include "resources/resource_manager.h"
#include "rendering/sprite_renderer.h"
#include "config.h"                 // TILE_SIZE
#include <algorithm>
#include <cstring>
#include <cmath>

namespace hd2d {

// ── tile 颜色回退: 贴图缺席时的纯色地形 ──
static Color _tile_color(TileType t, bool visible_now) {
    Color base;
    switch (t) {
    case TileType::WALL:      base = {74, 78, 96, 255};  break;
    case TileType::FLOOR:     base = {40, 42, 58, 255};  break;
    case TileType::LAVA:      base = {200, 90, 30, 255}; break;
    case TileType::STAIRS_DOWN: base = {180, 160, 60, 255}; break;
    case TileType::DOOR:      base = {130, 90, 50, 255}; break;
    default:                  base = {46, 48, 64, 255};  break;
    }
    // fog: 已探索不可见 = 压暗 40%
    if (!visible_now) {
        base.r = base.r * 6 / 10; base.g = base.g * 6 / 10; base.b = base.b * 6 / 10;
    }
    return base;
}

// ── M6-v2a: 群系 tile 贴图解析 — 复用 2D 同源回退链 ──
// (群系 wall_<biome> → 通用 wall → 程序化 procedural_tile, 与 GameMap::draw 一致)
struct TileTexPair {
    Texture2D tex = {};
    SpriteDef def;
};

static TileTexPair _resolve_tile_tex(const GameMap& map, const char* kind,
                                     const Color& fallback_color, bool wall) {
    TileTexPair out;
    auto& res = ResourceManager::inst();
    char biome_key[48];
    snprintf(biome_key, sizeof(biome_key), "%s_%s", kind, map.biome_id());
    Texture2D biome_tex = res.sprite_by_key(biome_key, out.def);
    if (biome_tex.id > 0) { out.tex = biome_tex; return out; }
    out.def = SpriteDef{};
    Texture2D generic = res.sprite_by_key(kind, out.def);
    if (generic.id > 0) { out.tex = generic; return out; }
    char proc_key[40];
    snprintf(proc_key, sizeof(proc_key), "%s_%02x%02x%02x",
             kind, fallback_color.r, fallback_color.g, fallback_color.b);
    out.tex = res.procedural_tile(proc_key, fallback_color, wall);
    out.def = SpriteDef{};  // procedural_tile = 整图单帧
    return out;
}

// ── 地形: 玩家周围可见 tile → 地板/墙 item (M6-v2a: 群系贴图接线) ──
static void _build_terrain(GameScene& gs, std::vector<HD2DDrawItem>& out) {
    const GameMap* map = gs.game_map.get();
    if (!map) return;

    int cx = 0, cy = 0;
    if (gs.player) {
        cx = (int)(gs.player->entity.rect.x / TILE_SIZE);
        cy = (int)(gs.player->entity.rect.y / TILE_SIZE);
    }
    int x0 = std::max(0, cx - 16), x1 = std::min(map->width - 1, cx + 16);
    int y0 = std::max(0, cy - 12), y1 = std::min(map->height - 1, cy + 12);

    // v2a: 与 2D 同源的贴图回退链 (群系 → 通用 → 程序化)
    const auto& pal = map->palette();
    bool has_pal = map->has_palette();
    Color wall_c  = has_pal ? pal.wall_face : Color{60, 60, 80, 255};
    Color floor_c = has_pal ? pal.floor_base : Color{25, 25, 35, 255};
    TileTexPair wall_tex  = _resolve_tile_tex(*map, "wall", wall_c, true);
    TileTexPair floor_tex = _resolve_tile_tex(*map, "floor", floor_c, false);

    for (int ty = y0; ty <= y1; ty++) {
        for (int tx = x0; tx <= x1; tx++) {
            if (!map->isExplored(tx, ty)) continue;
            TileType t = map->tile_at(tx, ty);
            HD2DDrawItem item;
            item.tile_x = tx; item.tile_y = ty;
            item.world_pos = {(float)tx * TILE_SIZE + TILE_SIZE * 0.5f, 0,
                              (float)ty * TILE_SIZE + TILE_SIZE * 0.5f};
            item.size = (float)TILE_SIZE;
            item.tint = _tile_color(t, map->isVisible(tx, ty));
            if (t == TileType::WALL) {
                item.kind = HD2DDrawItem::Kind::WALL_BLOCK;
                item.height = TILE_SIZE * 1.25f;
                item.texture = wall_tex.tex;
                item.tex_src = wall_tex.def.frame_w > 0
                    ? SpriteRenderer::frame_rect(wall_tex.def, 0) : Rectangle{};
            } else {
                item.kind = HD2DDrawItem::Kind::FLOOR_TILE;
                item.texture = floor_tex.tex;
                item.tex_src = floor_tex.def.frame_w > 0
                    ? SpriteRenderer::frame_rect(floor_tex.def, 0) : Rectangle{};
                // 贴图地板不再叠 tint (贴图自带配色; fog 由 renderer 压暗)
                if (item.texture.id > 0) item.tint = WHITE;
                // M6-v2c: LAVA tile 标记 → renderer 岩浆动画 shader 分流
                // tint 编码探索压暗: 可见=白(全亮) / 已探索不可见=60%灰
                // (与 _tile_color fog 压暗 40% 同语义; shader 端乘 fragColor.rgb)
                if (t == TileType::LAVA) {
                    item.is_lava = true;
                    item.texture = {};          // 岩浆走程序化噪声, 弃贴图
                    item.tint = map->isVisible(tx, ty) ? WHITE
                                                       : Color{153, 153, 153, 255};
                }
            }
            out.push_back(item);
        }
    }
}

// ── 实体: 玩家 + 存活怪 → billboard (贴图 2D 同源 + v2a 呼吸帧动画) ──
static void _build_entities(GameScene& gs, std::vector<HD2DDrawItem>& out) {
    auto& res = ResourceManager::inst();
    // M4f.3 同款待机/呼吸 2 帧轮换 (GetTime 非随机, 不触 RNG 红线)
    int anim_frame = ((int)(GetTime() * 4)) & 1;

    if (gs.player && gs.player->combat.is_alive) {
        HD2DDrawItem item;
        item.kind = HD2DDrawItem::Kind::ENTITY_BILLBOARD;
        const auto& r = gs.player->entity.rect;
        item.world_pos = {r.x + r.width * 0.5f, 0, r.y + r.height * 0.5f};
        item.size = 36.0f;
        item.sort_y = r.y;
        SpriteDef def;
        item.texture = res.sprite_by_key("player_default", def);
        if (item.texture.id > 0)
            item.tex_src = SpriteRenderer::frame_rect(def, anim_frame);
        else item.tint = {90, 160, 255, 255};
        item.flip_x = (gs.player->direction == Direction::LEFT);
        out.push_back(item);
    }
    for (auto& m : gs.monsters) {
        if (!m || !m->combat.is_alive) continue;
        HD2DDrawItem item;
        item.kind = HD2DDrawItem::Kind::ENTITY_BILLBOARD;
        const auto& r = m->entity.rect;
        item.world_pos = {r.x + r.width * 0.5f, 0, r.y + r.height * 0.5f};
        item.size = 34.0f;
        item.sort_y = r.y;
        SpriteDef def;
        if (!m->sprite_override.empty())
            item.texture = res.sprite_by_key(m->sprite_override.c_str(), def);
        if (item.texture.id == 0)
            item.texture = res.sprite_by_key("mon_orc", def);
        if (item.texture.id > 0)
            item.tex_src = SpriteRenderer::frame_rect(def, anim_frame);
        else item.tint = {220, 80, 80, 255};
        out.push_back(item);
    }
}

// ── 特效: active_effects 存活项 → 发光片 ──
static void _build_effects(GameScene& gs, std::vector<HD2DDrawItem>& out) {
    for (const auto& e : gs.active_effects) {
        if (e.elapsed >= e.duration) continue;
        HD2DDrawItem item;
        item.kind = HD2DDrawItem::Kind::FX_QUAD;
        item.world_pos = {e.world_x, 0, e.world_y};
        item.size = e.radius;
        item.tint = e.color;
        out.push_back(item);
    }
}

// ── M6-v2a: 地面物品 → 贴地小 billboard (图标与 2D 同源) ──
static void _build_ground_items(GameScene& gs, std::vector<HD2DDrawItem>& out) {
    auto& res = ResourceManager::inst();
    for (const auto& d : gs.dropped_items()) {
        if (gs.game_map && !gs.game_map->isVisible(d.tile_x, d.tile_y)) continue;
        const char* ikey = item_icon_key(d.item.get());
        if (!ikey) continue;
        HD2DDrawItem item;
        item.kind = HD2DDrawItem::Kind::ENTITY_BILLBOARD;
        item.world_pos = {(float)d.tile_x * TILE_SIZE + TILE_SIZE * 0.5f, 0,
                          (float)d.tile_y * TILE_SIZE + TILE_SIZE * 0.5f};
        item.size = 24.0f;                     // 拾取物小一号
        item.sort_y = (float)d.tile_y * TILE_SIZE;
        SpriteDef def;
        item.texture = res.sprite_by_key(ikey, def);
        if (item.texture.id <= 0) continue;     // 图标缺素材不画 (2D 有几何回退, 3D 跳过)
        item.tex_src = SpriteRenderer::frame_rect(def, 0);
        item.tint = WHITE;
        out.push_back(item);
    }
}

// ── M6-v2a: 未完成 NPC → billboard (npc_sprite_key 楼层映射) ──
static void _build_npcs(GameScene& gs, std::vector<HD2DDrawItem>& out) {
    auto& res = ResourceManager::inst();
    const char* skey = npc_sprite_key(gs.current_floor);
    for (const auto& npc : gs.npc_views()) {
        if (gs.game_map && !gs.game_map->isVisible(npc.tile_x, npc.tile_y)) continue;
        HD2DDrawItem item;
        item.kind = HD2DDrawItem::Kind::ENTITY_BILLBOARD;
        item.world_pos = {(float)npc.tile_x * TILE_SIZE + TILE_SIZE * 0.5f, 0,
                          (float)npc.tile_y * TILE_SIZE + TILE_SIZE * 0.5f};
        item.size = 34.0f;
        item.sort_y = (float)npc.tile_y * TILE_SIZE;
        SpriteDef def;
        item.texture = res.sprite_by_key(skey, def);
        if (item.texture.id <= 0) continue;     // 缺素材回退: 2D 有绿点, 3D 跳过
        item.tex_src = SpriteRenderer::frame_rect(def, 0);
        item.tint = WHITE;
        out.push_back(item);
    }
}

// ── M6-v2a: 挑战传送门 → 竖立脉冲光环 (2D 双层圆的 3D 对应物) ──
// 与 2D 分支 (game_scene._render 2222-2237) 同条件: DUNGEON 入口 / ARENA 返回
static void _build_portals(GameScene& gs, std::vector<HD2DDrawItem>& out) {
    const auto& challenge = gs.challenge_ctrl();
    const GameMap* map = gs.game_map.get();
    if (challenge.phase() == ChallengePhase::PORTAL_ACTIVE && map) {
        for (const auto& sr : map->special_rooms) {
            if (sr.type != SpecialRoomType::CHALLENGE) continue;
            HD2DDrawItem item;
            item.kind = HD2DDrawItem::Kind::PORTAL_RING;
            item.world_pos = {(float)sr.portal_tx * TILE_SIZE + TILE_SIZE * 0.5f, 0,
                              (float)sr.portal_ty * TILE_SIZE + TILE_SIZE * 0.5f};
            item.height = 18.0f;               // 门环半径 (贴地圆心)
            item.size = 0.16f;                 // 环管粗细比例
            item.portal_entry = true;
            out.push_back(item);
            break;                             // 与 2D 同: 只画第一个挑战房
        }
    }
    if (challenge.phase() == ChallengePhase::CLEARED && gs.in_challenge_arena() &&
        challenge.return_portal_tx() >= 0) {
        HD2DDrawItem item;
        item.kind = HD2DDrawItem::Kind::PORTAL_RING;
        item.world_pos = {(float)challenge.return_portal_tx() * TILE_SIZE + TILE_SIZE * 0.5f, 0,
                          (float)challenge.return_portal_ty() * TILE_SIZE + TILE_SIZE * 0.5f};
        item.height = 18.0f;
        item.size = 0.16f;
        item.portal_entry = false;
        out.push_back(item);
    }
}

// ── M6-v2b: 投射物 — WARNING 相 (AOE圈/轨迹线) + ACTIVE 相 (发光弹体) ──
// 条件与配色逐条对齐 2D 分支 (game_scene._render 2234-2275)
static void _build_projectiles(GameScene& gs, std::vector<HD2DDrawItem>& out) {
    for (const auto& p : gs.projectiles) {
        if (!p.alive) continue;
        HD2DDrawItem item;
        item.world_pos = {p.pos.x, 12.0f, p.pos.y};   // 弹道离地 12px

        if (p.active_time < 0.0f) {
            // WARNING 相: 2D 配色 (红/橙/黄 三级) + 1-fade 递增警示
            float fade = 1.0f - (-p.active_time / p.warning_time);
            Color wc = (p.warning_level >= 2) ? Color{255,40,20,120}
                     : (p.warning_level >= 1) ? Color{255,160,30,120}
                     : Color{255,200,60,110};
            if (p.warning_radius > 0.0f) {
                // AOE 危险圈 → 贴地预警 ring
                item.kind = HD2DDrawItem::Kind::WARNING_RING;
                item.world_pos.y = 0.1f;
                item.size = p.warning_radius;
                item.tint = wc;
                item.height = fade;                    // renderer 递增脉冲
                out.push_back(item);
            } else {
                // 点弹 → 轨迹线 (终点 = 撞墙/寿命终点, 与 2D _preview 同算法)
                float speed = sqrtf(p.vel.x * p.vel.x + p.vel.y * p.vel.y);
                if (speed < 1.0f) continue;
                float ux = p.vel.x / speed, uy = p.vel.y / speed;
                float len = speed * p.lifetime;
                if (gs.game_map) {
                    for (float d = TILE_SIZE; d < len; d += TILE_SIZE) {
                        auto [tx, ty] = gs.game_map->pixel_to_tile(
                            p.pos.x + ux * d, p.pos.y + uy * d);
                        if (!gs.game_map->is_walkable(tx, ty)) { len = d; break; }
                    }
                }
                item.kind = HD2DDrawItem::Kind::TRAJECTORY_LINE;
                item.end_pos = {p.pos.x + ux * len, 12.0f, p.pos.y + uy * len};
                item.tint = wc;
                item.height = fade;
                out.push_back(item);
            }
            continue;
        }
        // ACTIVE 相: 弹体 (穿透金 / 敌元素色 / 玩家土色; 2D 同源)
        item.kind = HD2DDrawItem::Kind::PROJECTILE_BODY;
        item.piercing = p.piercing;
        item.element = p.element;
        item.tint = p.owner != 0 ? Color{255, 80, 40, 255}
                   : Color{200, 160, 100, 255};
        item.size = 6.0f;
        item.trail_dir = p.vel;                 // M6-v2d: 拖尾方向 (px/s)
        out.push_back(item);
    }
}

// ── M6-v2b: 远程武器射程指示环 (玩家 range_indicator_timer 激活时) ──
// NUNCHAKU=双环带 / SPEAR,CROSSBOW=单环 (对齐 2D 2277-2319)
static void _build_range_indicator(GameScene& gs, std::vector<HD2DDrawItem>& out) {
    if (!gs.player || gs.player->weapon.range_indicator_timer <= 0.0f) return;
    auto wt = gs.player->weapon.weapon_type();
    if (wt != WeaponType::SPEAR && wt != WeaponType::CROSSBOW
        && wt != WeaponType::NUNCHAKU) return;
    const auto& r = gs.player->entity.rect;
    HD2DDrawItem item;
    item.kind = HD2DDrawItem::Kind::WARNING_RING;
    item.world_pos = {r.x + r.width * 0.5f, 0.1f, r.y + r.height * 0.5f};
    item.tint = {235, 175, 95, 200};                 // 2D 同款暖金
    item.height = gs.player->weapon.range_indicator_timer / 0.25f;  // fade
    if (wt == WeaponType::NUNCHAKU) {
        const WeaponDef* def = gs.player->weapon.current_def();
        item.size = (def ? def->max_range : 5.0f) * TILE_SIZE;      // 外环
        item.element = (def ? def->min_range : 2.0f) * TILE_SIZE;    // 内环(复用)
    } else {
        item.size = gs.player->weapon.range_indicator_px;
        item.element = -1.0f;                        // -1 = 单环
    }
    out.push_back(item);
}

// ── M6-v2b: Boss 技能预警 — 弹幕弹道/扇形面/瞬移落点/旋风圈 (只读 BossAI) ──
// 条件对齐 2D 分支 (game_scene._render 2754-2762: is_boss && ai)
static void _build_boss_skill_warnings(GameScene& gs, std::vector<HD2DDrawItem>& out) {
    for (auto& m : gs.monsters) {
        if (!m || !m->is_boss || !m->ai || !m->combat.is_alive) continue;
        auto* bai = dynamic_cast<BossAI*>(m->ai);
        if (!bai) continue;
        const auto& r = m->entity.rect;
        Vector3 bpos = {r.x + r.width * 0.5f, 0, r.y + r.height * 0.5f};

        // 弹幕: 每颗在飞 shot → 轨迹线 (2D BarrageSkill::draw 同源)
        if (auto* sk = bai->barrage_skill()) {
            for (const auto& s : sk->shots) {
                if (s.life <= 0.0f) continue;
                float speed = sqrtf(s.vx * s.vx + s.vy * s.vy);
                if (speed < 1.0f) continue;
                HD2DDrawItem item;
                item.kind = HD2DDrawItem::Kind::TRAJECTORY_LINE;
                item.world_pos = {s.x, 10.0f, s.y};
                item.end_pos = {s.x + s.vx / speed * 60.0f, 10.0f,
                                s.y + s.vy / speed * 60.0f};
                item.tint = {255, 120, 60, 130};
                item.height = 1.0f;
                out.push_back(item);
            }
            // 蓄力期: 风扇形预警 (朝玩家; half=spread/2)
            if (sk->windup_left > 0.0f && gs.player) {
                const auto& pr = gs.player->entity.rect;
                float ang = atan2f(pr.y + pr.height*0.5f - bpos.z,
                                   pr.x + pr.width*0.5f - bpos.x);
                HD2DDrawItem item;
                item.kind = HD2DDrawItem::Kind::CONE_FAN;
                item.world_pos = bpos;
                item.size = 110.0f;
                item.fan_angle = ang;
                item.fan_half_deg = sk->spread_deg * 0.5f;
                item.tint = {255, 160, 40, 150};
                out.push_back(item);
            }
        }
        // 扇形斩: 蓄力期面预警 (对齐 2D cone_skill().draw windup)
        if (auto* sk = bai->cone_skill()) {
            if (sk->windup_left > 0.0f && gs.player) {
                const auto& pr = gs.player->entity.rect;
                float ang = atan2f(pr.y + pr.height*0.5f - bpos.z,
                                   pr.x + pr.width*0.5f - bpos.x);
                HD2DDrawItem item;
                item.kind = HD2DDrawItem::Kind::CONE_FAN;
                item.world_pos = bpos;
                item.size = sk->reach;
                item.fan_angle = ang;
                item.fan_half_deg = sk->half_angle;
                item.tint = {255, 60, 40, 160};
                out.push_back(item);
            }
        }
        // 瞬移: 蓄力期落点圈 (2D BlinkSkill::draw pending 位置)
        if (auto* sk = bai->blink_skill()) {
            if (sk->windup_left > 0.0f) {
                HD2DDrawItem item;
                item.kind = HD2DDrawItem::Kind::WARNING_RING;
                item.world_pos = {sk->pending_x, 0.1f, sk->pending_y};
                item.size = 16.0f;
                item.tint = {180, 120, 255, 170};
                item.height = 1.0f;             // fade=1
                item.element = -1.0f;
                out.push_back(item);
            }
        }
        // 旋风: 蓄力白环 / 旋转期紫圈 (半径 = 范围 2.2 tile)
        if (auto* sk = bai->whirlwind_skill()) {
            if (sk->windup_left > 0.0f || sk->spin_duration > 0.0f) {
                bool spinning = sk->spin_duration > 0.0f;
                HD2DDrawItem item;
                item.kind = HD2DDrawItem::Kind::WARNING_RING;
                item.world_pos = bpos;
                item.world_pos.y = 0.1f;
                item.size = spinning ? 80.0f : 70.0f;
                item.tint = spinning ? Color{170, 90, 255, 170}
                                     : Color{240, 240, 255, 170};
                item.height = 1.0f;
                item.element = -1.0f;
                out.push_back(item);
            }
        }
    }
}

// ── M6-v2b: Boss 战场危险区 — 岩浆/影墙/虚空 贴地危险圈 (2D arena.draw 同源) ──
static void _build_danger_zones(GameScene& gs, std::vector<HD2DDrawItem>& out) {
    for (const auto& z : gs.boss_ctrl().arena.zones()) {
        HD2DDrawItem item;
        item.kind = HD2DDrawItem::Kind::WARNING_RING;
        item.world_pos = {z.world_x, 0.1f, z.world_y};
        item.size = z.radius;
        // 警戒期=橙黄脉冲 / 激活期=红; 2D is_warning 同条件
        item.tint = z.is_warning() ? z.warn_color : z.active_color;
        item.height = z.is_warning() ? 0.7f : 1.0f;   // fade
        item.element = -1.0f;                          // 单环
        out.push_back(item);
    }
}

// ── M6-v2b: 弱点光环 (F10.2 pulse ring) + Tank 守护连线 (2D 2743-2781 同源) ──
static void _build_monster_overlays(GameScene& gs, std::vector<HD2DDrawItem>& out) {
    for (auto& m : gs.monsters) {
        if (!m || !m->combat.is_alive) continue;
        const auto& r = m->entity.rect;
        Vector3 c = {r.x + r.width * 0.5f, 0, r.y + r.height * 0.5f};
        // F10.2: 弱点脉冲环 (橙)
        if (m->is_weak_point) {
            HD2DDrawItem item;
            item.kind = HD2DDrawItem::Kind::WARNING_RING;
            item.world_pos = {c.x, 0.1f, c.z};
            item.size = 14.0f;
            item.tint = {255, 120, 30, 180};
            item.height = 1.0f;
            item.element = -1.0f;
            out.push_back(item);
        }
        // D2 Step4: Tank 守护连线 (淡蓝; 前排 → 保护目标)
        if (m->ai && m->team_role == TeamRole::FRONTLINE && m->ai->_protect_target) {
            auto* t = m->ai->_protect_target;
            HD2DDrawItem item;
            item.kind = HD2DDrawItem::Kind::ENTITY_LINK;
            item.world_pos = {c.x, 10.0f, c.z};
            const auto& tr = t->entity.rect;
            item.end_pos = {tr.x + tr.width * 0.5f, 10.0f, tr.y + tr.height * 0.5f};
            item.tint = {60, 140, 255, 100};
            out.push_back(item);
        }
    }
}

// ── M6-v2e: 氛围粒子 → 微光点 (2D _ambient.draw 同源; 只读翻译) ──
// 粒子 y 语义: 世界像素 y 直接映射 3D z; 高度 = 粒子 y 的 3D 浮动
// (rise 粒子上飘 = 视觉高度渐变; 用 life 比例近似)
static void _build_ambient(GameScene& gs, std::vector<HD2DDrawItem>& out) {
    const auto& ambient = gs.ambient_layer();
    const auto& cfg = ambient.config();
    for (const auto& p : ambient.particles()) {
        if (p.life <= 0.0f) continue;
        HD2DDrawItem item;
        item.kind = HD2DDrawItem::Kind::AMBIENT_MOTE;
        item.world_pos = {p.x, 8.0f + 40.0f * (1.0f - p.life / p.max_life),
                          p.y};
        item.size = p.size * 2.0f;               // 半径→直径感
        item.tint = cfg.color;
        item.tint.a = p.alpha;
        item.height = p.life / p.max_life;       // 渐隐比例
        out.push_back(item);
    }
}

void build_scene(GameScene& gs, std::vector<HD2DDrawItem>& out_items) {
    _build_terrain(gs, out_items);
    _build_entities(gs, out_items);
    _build_effects(gs, out_items);
    _build_ground_items(gs, out_items);   // M6-v2a
    _build_npcs(gs, out_items);           // M6-v2a
    _build_portals(gs, out_items);        // M6-v2a
    _build_projectiles(gs, out_items);    // M6-v2b
    _build_range_indicator(gs, out_items);// M6-v2b
    _build_boss_skill_warnings(gs, out_items);  // M6-v2b
    _build_danger_zones(gs, out_items);         // M6-v2b
    _build_monster_overlays(gs, out_items);     // M6-v2b
    _build_ambient(gs, out_items);              // M6-v2e
}

} // namespace hd2d
