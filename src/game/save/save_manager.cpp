#include "save_manager.h"
#include "player.h"
#include "skill.h"
#include "item.h"
#include "combat_stats.h"
#include "core/logger.h"
#include "config.h"
#include "combat_system.h"
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#ifdef _WIN32
#include <direct.h>
#define mkdir_impl(p) _mkdir(p)
#else
#include <sys/stat.h>
#define mkdir_impl(p) mkdir(p, 0755)
#endif

std::string SaveManager::_save_dir() { return "saves"; }
// G10.9-B2: 槽位路径 saves/slot_N.json (N = 1..3)
std::string SaveManager::_slot_path(int slot_id) {
    return _save_dir() + "/slot_" + std::to_string(slot_id) + ".json";
}
// B3 迁移专用: 旧单槽路�?
std::string SaveManager::_legacy_save_path() { return _save_dir() + "/save.json"; }

bool SaveManager::g_sim_readonly = false;  // Q3.1: --sim 只读

// �?G10.9-B2: 活跃槽位 (进程内状�? 进游戏前由菜单设�? �?
static int g_active_slot = 1;
int  SaveManager::active_slot() { return g_active_slot; }
void SaveManager::set_active_slot(int slot_id) {
    if (slot_id >= 1 && slot_id <= SAVE_SLOT_COUNT) g_active_slot = slot_id;
}

bool SaveManager::slot_exists(int slot_id) {
    FILE* f = fopen(_slot_path(slot_id).c_str(), "rb");
    if (!f) return false; fclose(f); return true;
}

// 旧接口兼�? 探测活跃�?(迁移�?save.json 不再使用)
bool SaveManager::save_exists() { return slot_exists(active_slot()); }

// ---- 辅助: trim ----
static std::string trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    size_t b = s.find_last_not_of(" \t\r\n");
    return (a == std::string::npos) ? "" : s.substr(a, b - a + 1);
}

// ---- M4e: float 列表解析 (前置声明, 定义在文件末) ----
static void _parse_float_list(const std::string& s, std::vector<float>& out);

// ---- 序列�?----
bool SaveManager::save_game(int slot_id, Player* player, int floor, int max_f,
                              uint32_t dungeon_seed,
                              const std::vector<bool>& special_triggered,
                              const std::vector<bool>& special_discovered,
                              const std::unordered_map<std::string, int>& rule_counters,
                              const std::unordered_map<int, int>& quest_states,
                              const std::vector<float>& mirror_prior_alpha,
                              const std::vector<float>& mirror_prior_beta,
                              float play_time) {
    if (g_sim_readonly) return false;  // Q3.1: sim 模式不写玩家存档
    if (slot_id < 1 || slot_id > SAVE_SLOT_COUNT) return false;
    mkdir_impl(_save_dir().c_str());
    FILE* f = fopen(_slot_path(slot_id).c_str(), "wb");
    if (!f) { LOG_ERROR("存档无法写入 (slot %d)", slot_id); return false; }
    auto& c = player->combat;
    auto& inv = player->inventory;

    // G10.9-B2: v:5 �?首个真正被读取的版本�? 新增 time: 字段
    // (v�? 老档�?load �?getV 默认值兜�? �?_load_impl)
    fprintf(f, "v:5\n");
    fprintf(f, "floor:%d\n", floor);
    fprintf(f, "maxf:%d\n", max_f);
    fprintf(f, "lv:%d\n", player->level);
    fprintf(f, "xp:%d\n", player->xp);
    fprintf(f, "xpt:%d\n", player->xp_to_next);
    fprintf(f, "mhp:%d\n", c.max_hp);
    fprintf(f, "chp:%d\n", c.current_hp);
    fprintf(f, "atk:%d\n", c.attack);           // 基础值（每次升级+2�?
    fprintf(f, "pd:%d\n", c.physical_defense);
    fprintf(f, "md:%d\n", c.magical_defense);
    fprintf(f, "gld:%d\n", player->gold);
    fprintf(f, "key:%d\n", player->key_count);
    fprintf(f, "time:%.0f\n", play_time);       // G10.9-B2: 本档累计时长(�?

    // 主动技�? id,lv,evo,use;... (G3.2: _skill_id 替代 dynamic_cast)
    fprintf(f, "act:");
    for (auto& s : player->skills.active_skills) {
        const char* nm = s->_skill_id.empty() ? "slash" : s->_skill_id.c_str();
        fprintf(f, "%s,%d,%d,%d;", nm, s->level, s->evolution_level, s->use_count);
    }
    fprintf(f, "\n");

    // 被动技�?(G3.2: _skill_id)
    fprintf(f, "pas:");
    for (auto& s : player->skills.passives) {
        const char* nm = s->_skill_id.empty() ? "iron_skin" : s->_skill_id.c_str();
        fprintf(f, "%s,%d,%d,%d;", nm, s->level, s->evolution_level, s->use_count);
    }
    fprintf(f, "\n");

    // 背包物品: name,RARITY,type,val1,val2,val3;... G9: ,wpn_id appended for weapons
    fprintf(f, "inv:");
    for (auto& item : inv.items) {
        auto* eq = dynamic_cast<EquipmentItem*>(item.get());
        auto* cn = dynamic_cast<ConsumableItem*>(item.get());
        if (eq && eq->slot != "charm") {
            fprintf(f, "%s,%d,%s,%d,%d,%d",
                eq->base_name.c_str(), (int)eq->rarity, eq->slot.c_str(),
                eq->atk_bonus, eq->pdef_bonus, eq->mdef_bonus);
            if (!eq->weapon_def_id.empty())
                fprintf(f, ",%s", eq->weapon_def_id.c_str());
            fprintf(f, ";");
        } else if (eq) { // charm
            fprintf(f, "%s,%d,charm,0,0,0;", eq->base_name.c_str(), (int)eq->rarity);
        } else if (cn) {
            fprintf(f, "%s,%d,%s,%d,%s;",
                cn->base_name.c_str(), (int)cn->rarity, cn->effect_type.c_str(),
                cn->effect_value, cn->buff_id.c_str());
        }
    }
    fprintf(f, "\n");

    // 装备: slot:name,rarity,type,atk,pdef,mdef
    fprintf(f, "eqw:");
    if (inv.equipped["weapon"]) {
        auto& eq = inv.equipped["weapon"];
        fprintf(f, "%s,%d,%s,%d,%d,%d", eq->base_name.c_str(), (int)eq->rarity,
                eq->slot.c_str(), eq->atk_bonus, eq->pdef_bonus, eq->mdef_bonus);
    }
    fprintf(f, "\n");

    // G9: weapon_def_id for equipped weapon
    fprintf(f, "wpn:%s\n",
        inv.equipped["weapon"] && !inv.equipped["weapon"]->weapon_def_id.empty()
            ? inv.equipped["weapon"]->weapon_def_id.c_str() : "");

    fprintf(f, "eqa:");
    if (inv.equipped["armor"]) {
        auto& eq = inv.equipped["armor"];
        fprintf(f, "%s,%d,%s,%d,%d,%d", eq->base_name.c_str(), (int)eq->rarity,
                eq->slot.c_str(), eq->atk_bonus, eq->pdef_bonus, eq->mdef_bonus);
    }
    fprintf(f, "\n");

    // Buff 状�?(玩家)
    fprintf(f, "buf:");
    for (auto& b : player->active_buffs) {
        fprintf(f, "%s,%d,%.2f,%.2f;",
            b.id.c_str(), b.stacks, b.remaining, b.tick_timer);
    }
    fprintf(f, "\n");

    // B8: 特殊房间状�?
    fprintf(f, "seed:%u\n", dungeon_seed);
    fprintf(f, "spr:%s\n", _encode_spr(special_triggered).c_str());
    fprintf(f, "spd:%s\n", _encode_spr(special_discovered).c_str());
    // Batch 3A: 保存 RUN relics
    fprintf(f, "rlc:");
    for (auto& r : player->relics) {
        if (r.scope == PersistenceScope::RUN)
            fprintf(f, "%s,%d;", r.id.c_str(), static_cast<int>(r.scope));
    }
    fprintf(f, "\n");

    // ── G1 Step7: Save v2 新增 ──
    fprintf(f, "atl:%d\n", player->attack_evo.level);
    fprintf(f, "rul:");
    if (!rule_counters.empty()) {
        bool first = true;
        for (auto& kv : rule_counters) {
            if (!first) fprintf(f, ";");
            fprintf(f, "%s=%d", kv.first.c_str(), kv.second);
            first = false;
        }
    }
    fprintf(f, "\n");

    // ── G2.4: Quest state ──
    fprintf(f, "qst:");
    if (!quest_states.empty()) {
        bool first = true;
        for (auto& kv : quest_states) {
            if (!first) fprintf(f, ";");
            fprintf(f, "%d=%d", kv.first, kv.second);
            first = false;
        }
    }
    fprintf(f, "\n");

    // ── G10.1: Element Core (M4b-fix: �?int 而非名字 �?atoi("fire")=0 曾致元素类型读档丢失) ──
    fprintf(f, "elem:%d,%d,%d,%d\n",
        (int)player->element.type,
        player->element.level,
        player->element.experience,
        player->element.initialized ? 1 : 0);

    // G10.9-B2: end: 行已移除 �?unlocked_endings 迁移 meta_save.json (账号�?,
    // 解锁时由 EndingDirector �?MetaSystem::unlock_ending 立即落盘

    // ── M4e: 跨对局镜像记忆 (逗号分隔 float) ──
    if (!mirror_prior_alpha.empty() && !mirror_prior_beta.empty()) {
        fprintf(f, "mra:");
        for (size_t i = 0; i < mirror_prior_alpha.size(); i++) {
            if (i > 0) fprintf(f, ",");
            fprintf(f, "%.4f", mirror_prior_alpha[i]);
        }
        fprintf(f, "\n");
        fprintf(f, "mrb:");
        for (size_t i = 0; i < mirror_prior_beta.size(); i++) {
            if (i > 0) fprintf(f, ",");
            fprintf(f, "%.4f", mirror_prior_beta[i]);
        }
        fprintf(f, "\n");
    }

    fclose(f);
    LOG_INFO("存档: �?d�?Lv%d HP:%d/%d %zu技�?%zu物品 %zuBuff seed:%u",
        floor, player->level, c.current_hp, c.max_hp,
        player->skills.active_skills.size(), inv.items.size(),
        player->active_buffs.size(), dungeon_seed);
    return true;
}

// ---- 反序列化 ----
// G10.9-B2: 槽位化读�?�?load_game(slot) 为新入口, load_save() 转发活跃�?
SaveData* SaveManager::load_game(int slot_id) {
    if (!slot_exists(slot_id)) return nullptr;
    FILE* f = fopen(_slot_path(slot_id).c_str(), "rb");
    if (!f) return nullptr;

    // 读取所有行
    std::vector<std::string> lines;
    char buf[4096];
    while (fgets(buf, sizeof(buf), f)) {
        std::string line = trim(buf);
        if (!line.empty()) lines.push_back(line);
    }
    fclose(f);

    // 解析�? "key:value"
    auto getV = [&](const char* key, int def = 0) -> int {
        std::string prefix = std::string(key) + ":";
        for (auto& l : lines) {
            if (l.compare(0, prefix.size(), prefix) == 0) {
                return atoi(l.c_str() + prefix.size());
            }
        }
        return def;
    };
    auto getS = [&](const char* key, const char* def = "") -> std::string {
        std::string prefix = std::string(key) + ":";
        for (auto& l : lines) {
            if (l.compare(0, prefix.size(), prefix) == 0) {
                return l.substr(prefix.size());
            }
        }
        return def;
    };

    // G10.9-B2: 版本号真正参与加�?�?v<5 老档�?time: (getV 默认 0), 其他字段本就逐键兜底
    // 此处仅记�? 后续 v6+ 迁移在此分支处理
    int save_version = getV("v", 4);
    (void)save_version;

    int floor = getV("floor", 1);
    int maxf  = getV("maxf", 1);
    int lv    = getV("lv", 1);
    int xp    = getV("xp", 0);
    int xpt   = getV("xpt", Player::calc_xp_for_level(lv + 1));
    uint32_t seed = (uint32_t)getV("seed", 0);
    std::vector<bool> spr = _decode_spr(getS("spr"));
    std::vector<bool> spd = _decode_spr(getS("spd"));
    int mhp   = getV("mhp", PLAYER_MAX_HP);
    int chp   = getV("chp", mhp);
    int atk   = getV("atk", PLAYER_ATTACK);
    int pd    = getV("pd", PLAYER_PDEF);
    int md    = getV("md", PLAYER_MDEF);
    int gld   = getV("gld", 0);
    int kcnt  = getV("key", 0);

    auto p = std::make_unique<Player>(
        TILE_SIZE * 2, TILE_SIZE * 2, PLAYER_SPEED, mhp, atk, pd, md);
    p->combat.current_hp = chp;
    p->level = lv;
    p->xp = xp;
    p->xp_to_next = xpt;
    p->gold = gld;
    p->key_count = kcnt;

    // Batch 3A: 读取 RUN relics (v4+)
    std::string rlc_str = getS("rlc");
    if (!rlc_str.empty()) {
        for (size_t pos = 0; pos < rlc_str.size(); ) {
            size_t semi = rlc_str.find(';', pos);
            std::string tok = rlc_str.substr(pos, (semi != std::string::npos ? semi - pos : std::string::npos));
            if (tok.empty()) break;
            size_t comma = tok.find(',');
            if (comma != std::string::npos) {
                std::string rid = tok.substr(0, comma);
                int scope_val = std::atoi(tok.substr(comma + 1).c_str());
                if (scope_val == static_cast<int>(PersistenceScope::RUN) && !rid.empty()) {
                    const RelicDef* def = get_relic_def(rid);
                    if (def) p->add_relic(rid, PersistenceScope::RUN);
                }
            }
            pos = (semi != std::string::npos) ? semi + 1 : std::string::npos;
        }
    }

    // 恢复主动技�?(G3.2: SkillFactory 替代 dynamic_cast, 兼容旧格�?name)
    // G3.2: �?save 名映�?("Slash"�?slash", "Fireball"�?fireball", etc.)
    static auto _map_old_name = [](const std::string& nm) -> std::string {
        if (nm == "Slash")     return "slash";
        if (nm == "Fireball")  return "fireball";
        if (nm == "SelfHeal")  return "self_heal";
        if (nm == "TheWorld")  return "the_world";
        if (nm == "IronSkin")  return "iron_skin";
        if (nm == "Berserk")   return "berserk";
        return nm; // G3.2+ new format already has correct id
    };

    std::string act = getS("act");
    if (!act.empty()) {
        for (size_t pos = 0; pos < act.size(); ) {
            size_t semi = act.find(';', pos);
            if (semi == std::string::npos) break;
            std::string tok = act.substr(pos, semi - pos);
            pos = semi + 1;
            int commas = 0;
            for (char c : tok) if (c == ',') commas++;
            size_t comma1 = tok.find(',');
            if (comma1 == std::string::npos) continue;
            std::string nm = _map_old_name(tok.substr(0, comma1));
            size_t comma2 = tok.find(',', comma1 + 1);
            std::string lvStr = (comma2 != std::string::npos)
                ? tok.substr(comma1 + 1, comma2 - comma1 - 1)
                : tok.substr(comma1 + 1);
            int lvl = atoi(lvStr.c_str());
            int evo = 0, use = 0;
            if (commas >= 3 && comma2 != std::string::npos) {
                size_t comma3 = tok.find(',', comma2 + 1);
                std::string evoStr = (comma3 != std::string::npos)
                    ? tok.substr(comma2 + 1, comma3 - comma2 - 1)
                    : tok.substr(comma2 + 1);
                evo = atoi(evoStr.c_str());
                if (comma3 != std::string::npos)
                    use = atoi(tok.substr(comma3 + 1).c_str());
            }
            // G3.2: SkillFactory::create �?替代 if-else �?
            std::unique_ptr<Skill> sk = skill_factory_create(nm);
            if (!sk) continue;
            while (sk->level < lvl) sk->upgrade();
            sk->evolution_level = evo;
            sk->use_count = use;
            p->skills.learn(std::move(sk));
        }
    }

    // 被动 (G3.2: SkillFactory)
    std::string pas = getS("pas");
    if (!pas.empty()) {
        for (size_t pos = 0; pos < pas.size(); ) {
            size_t semi = pas.find(';', pos);
            if (semi == std::string::npos) break;
            std::string tok = pas.substr(pos, semi - pos);
            pos = semi + 1;
            int commas = 0;
            for (char c : tok) if (c == ',') commas++;
            size_t comma1 = tok.find(',');
            if (comma1 == std::string::npos) continue;
            std::string nm = _map_old_name(tok.substr(0, comma1));
            size_t comma2 = tok.find(',', comma1 + 1);
            std::string lvStr = (comma2 != std::string::npos)
                ? tok.substr(comma1 + 1, comma2 - comma1 - 1)
                : tok.substr(comma1 + 1);
            int lvl = atoi(lvStr.c_str());
            int evo = 0, use = 0;
            if (commas >= 3 && comma2 != std::string::npos) {
                size_t comma3 = tok.find(',', comma2 + 1);
                evo = atoi(tok.substr(comma2 + 1,
                    (comma3 != std::string::npos ? comma3 - comma2 - 1 : std::string::npos)).c_str());
                if (comma3 != std::string::npos)
                    use = atoi(tok.substr(comma3 + 1).c_str());
            }
            std::unique_ptr<Skill> sk = skill_factory_create(nm);
            if (!sk) continue;
            while (sk->level < lvl) sk->upgrade();
            sk->evolution_level = evo;
            sk->use_count = use;
            p->skills.learn(std::move(sk));
        }
    }
    p->attack_evo.level = getV("atl", 1);  // G1 Step7
    p->skills.apply_all_passives(p.get());

    // 背包物品: name,RARITY,type,v1,v2,v3;...
    std::string inv_s = getS("inv");
    if (!inv_s.empty()) {
        for (size_t pos = 0; pos < inv_s.size(); ) {
            size_t semi = inv_s.find(';', pos);
            if (semi == std::string::npos) break;
            std::string tok = inv_s.substr(pos, semi - pos);
            pos = semi + 1;
            // Parse: name,rarity,type,val1,val2,val3
            std::vector<std::string> parts;
            for (size_t i = 0, last = 0; i <= tok.size(); i++) {
                if (i == tok.size() || tok[i] == ',') {
                    parts.push_back(tok.substr(last, i - last));
                    last = i + 1;
                }
            }
            if (parts.size() < 3) continue;
            std::string nm  = parts[0];
            Rarity rar = (Rarity)atoi(parts[1].c_str());
            std::string typ = parts[2];

            if ((typ == "heal" || typ == "buff") && parts.size() >= 4) {
                std::string buf = (parts.size() >= 5) ? parts[4] : "";
                p->inventory.items.push_back(
                    std::make_shared<ConsumableItem>(nm, rar, typ, atoi(parts[3].c_str()), buf));
            } else if (parts.size() >= 6) {
                int a = atoi(parts[3].c_str());
                int pd2 = atoi(parts[4].c_str());
                int md2 = atoi(parts[5].c_str());
                auto ei = std::make_shared<EquipmentItem>(nm, rar, typ, a, pd2, md2, false);
                // G9: restore weapon_def_id if present (7th field)
                if (parts.size() >= 7 && !parts[6].empty())
                    ei->weapon_def_id = parts[6];
                p->inventory.items.push_back(ei);
            }
        }
    }

    // 装备
    auto parseEquip = [&](const char* key) -> std::shared_ptr<EquipmentItem> {
        std::string s = getS(key);
        if (s.empty()) return nullptr;
        std::vector<std::string> parts;
        for (size_t i = 0, last = 0; i <= s.size(); i++) {
            if (i == s.size() || s[i] == ',') {
                parts.push_back(s.substr(last, i - last));
                last = i + 1;
            }
        }
        if (parts.size() < 6) return nullptr;
        Rarity rar = (Rarity)atoi(parts[1].c_str());
        return std::make_shared<EquipmentItem>(
            parts[0], rar, parts[2],
            atoi(parts[3].c_str()), atoi(parts[4].c_str()), atoi(parts[5].c_str()),
            false);
    };

    auto eqw = parseEquip("eqw");
    if (eqw) { eqw->apply(p.get()); p->inventory.equipped["weapon"] = eqw; }
    auto eqa = parseEquip("eqa");
    if (eqa) { eqa->apply(p.get()); p->inventory.equipped["armor"] = eqa; }

    // G9: restore weapon_def_id for equipped weapon
    std::string wpn_id = getS("wpn");
    if (!wpn_id.empty()) {
        if (eqw) eqw->weapon_def_id = wpn_id;
        p->weapon.equip(wpn_id);
    }

    // 恢复 Buff (buf:poison,2,3.50,0.20;attack_up,1,5.80,0.00;)
    std::string buf_line = getS("buf");
    if (!buf_line.empty()) {
        int restored = 0, skipped = 0;
        for (size_t pos = 0; pos < buf_line.size(); ) {
            size_t semi = buf_line.find(';', pos);
            if (semi == std::string::npos) break;
            std::string tok = buf_line.substr(pos, semi - pos);
            pos = semi + 1;
            if (tok.empty()) continue;

            // 解析 id,stacks,remaining,tick_timer
            std::vector<std::string> parts;
            for (size_t i = 0, last = 0; i <= tok.size(); i++) {
                if (i == tok.size() || tok[i] == ',') {
                    parts.push_back(tok.substr(last, i - last));
                    last = i + 1;
                }
            }
            if (parts.size() < 4) {
                LOG_WARN("[BUF] 读档跳过坏条�? %s", tok.c_str());
                skipped++; continue;
            }
            std::string id  = parts[0];
            if (id.empty()) { skipped++; continue; }
            BuffInstance bi;
            bi.id = id;
            bi.stacks   = atoi(parts[1].c_str());
            bi.remaining = (float)atof(parts[2].c_str());
            bi.tick_timer= (float)atof(parts[3].c_str());
            if (bi.stacks <= 0 || bi.remaining <= 0) {
                // 过期或无�?buff：跳�?
                skipped++; continue;
            }
            p->active_buffs.push_back(bi);
            restored++;
        }
        if (restored > 0 || skipped > 0)
            LOG_INFO("读档Buff: 恢复%d 跳过%d", restored, skipped);
    }

    auto* d = new SaveData;
    d->current_floor = floor;
    d->max_unlocked_floor = maxf;
    d->dungeon_seed = seed;
    d->special_triggered = spr;
    d->special_discovered = spd;

    // ── G1 Step7: Save v2 新增字段解析 ──
    // ── G2.4: Parse quest states ──
    std::string qst = getS("qst");
    if (!qst.empty()) {
        for (size_t pos = 0; pos < qst.size(); ) {
            size_t semi = qst.find(';', pos);
            std::string tok = qst.substr(pos, (semi != std::string::npos ? semi - pos : std::string::npos));
            pos = (semi != std::string::npos ? semi + 1 : qst.size());
            size_t eq = tok.find('=');
            if (eq != std::string::npos) {
                int qid = atoi(tok.substr(0, eq).c_str());
                int st  = atoi(tok.substr(eq + 1).c_str());
                d->quest_states[qid] = st;
            }
        }
    }

    // G10.9-B2: end: 行解析移�?�?endings 迁移 meta_save.json (load_game 不再返回)
    // 老档 end: 行仍可能存在, 直接忽略 (数据已在 meta 侧由 unlock_ending 接管)

    // ── G10.1: Element Core ──
    {
        std::string elem = getS("elem");
        if (!elem.empty()) {
            int parts[4] = {0, 1, 0, 0}; // type, level, exp, init
            for (int i = 0, pi = 0; i < (int)elem.size() && pi < 4; ) {
                int comma = (int)elem.find(',', i);
                std::string tok = elem.substr(i, (comma < 0 ? (int)elem.size() : comma) - i);
                if (!tok.empty()) parts[pi] = atoi(tok.c_str());
                i = (comma < 0 ? (int)elem.size() : comma + 1);
                pi++;
            }
            static const ElementType map[] = {
                ElementType::NONE, ElementType::FIRE, ElementType::ICE, ElementType::POISON
            };
            int et = (parts[0] >= 0 && parts[0] < 4) ? parts[0] : 0;
            p->element.type    = map[et];
            p->element.level   = std::max(1, parts[1]);
            p->element.experience = std::max(0, parts[2]);
            p->element.initialized = (parts[3] != 0);
            d->element_type  = parts[0];
            d->element_level = parts[1];
            d->element_exp   = parts[2];
            d->element_initialized = (parts[3] != 0);
        }
    }

    d->attack_evo_level = getV("atl", 1);
    d->play_time = (float)getV("time", 0);   // G10.9-B2: v<5 老档无此�?�?0

    // ── M4e: 跨对局镜像记忆恢复 ──
    _parse_float_list(getS("mra"), d->mirror_prior_alpha);
    _parse_float_list(getS("mrb"), d->mirror_prior_beta);

    std::string rul = getS("rul");
    if (!rul.empty()) {
        for (size_t pos = 0; pos < rul.size(); ) {
            size_t semi = rul.find(';', pos);
            std::string tok = rul.substr(pos, (semi != std::string::npos ? semi - pos : std::string::npos));
            pos = (semi != std::string::npos ? semi + 1 : rul.size());
            size_t eq = tok.find('=');
            if (eq != std::string::npos) {
                std::string key = tok.substr(0, eq);
                int val = atoi(tok.substr(eq + 1).c_str());
                if (!key.empty()) d->rule_counters[key] = val;
            }
        }
    }

    d->player = std::move(p);

    LOG_INFO("读档(slot %d v%d): �?d�?Lv%d HP:%d/%d %zu技�?%zu物品 %zuBuff",
        slot_id, save_version, floor, lv, chp, mhp,
        d->player->skills.active_skills.size(), d->player->inventory.items.size(),
        d->player->active_buffs.size());
    return d;
}

// G10.9-B2: 旧接�?�?活跃槽转�?
SaveData* SaveManager::load_save() { return load_game(active_slot()); }

void SaveManager::delete_save(int slot_id) {
    remove(_slot_path(slot_id).c_str());
    LOG_INFO("存档已删�?(slot %d)", slot_id);
}

// G10.9-B2: 轻量槽位摘要 �?�?fopen 扫几�? 不构�?Player/不依�?Registry
SlotSummary SaveManager::get_slot_summary(int slot_id) {
    SlotSummary s;
    s.slot_id = slot_id;
    if (!slot_exists(slot_id)) return s;
    FILE* f = fopen(_slot_path(slot_id).c_str(), "rb");
    if (!f) return s;
    s.exists = true;
    char buf[256];
    while (fgets(buf, sizeof(buf), f)) {
        if (strncmp(buf, "floor:", 6) == 0)      s.floor = atoi(buf + 6);
        else if (strncmp(buf, "maxf:", 5) == 0)  s.max_floor = atoi(buf + 5);
        else if (strncmp(buf, "lv:", 3) == 0)   s.level = atoi(buf + 3);
        else if (strncmp(buf, "time:", 5) == 0) s.play_time = (float)atof(buf + 5);
        else if (strncmp(buf, "elem:", 5) == 0) s.element_type = atoi(buf + 5);
    }
    fclose(f);
    return s;
}

std::vector<SlotSummary> SaveManager::get_all_slots() {
    std::vector<SlotSummary> v;
    v.reserve(SAVE_SLOT_COUNT);
    for (int i = 1; i <= SAVE_SLOT_COUNT; i++) v.push_back(get_slot_summary(i));
    return v;
}

// �?G10.9-B3: 旧档安全迁移 �?save.json �?slot_1.json �?
// 条件: 旧档存在 && slot_1 �?(不覆盖任何已有槽!)
// 流程: 复制旧档 �?验证新档可读 �?旧档改名 .bak (不删�? 迁移失败可手工恢�?
bool SaveManager::migrate_legacy_save() {
    FILE* legacy = fopen(_legacy_save_path().c_str(), "rb");
    if (!legacy) { fclose(legacy); return false; }          // 无旧�? 无事可做
    // 读完旧档内容到内�?(旧档单文�?< 10KB)
    std::string content;
    char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), legacy)) > 0) content.append(buf, n);
    fclose(legacy);

    if (content.empty()) return false;
    if (slot_exists(1)) {
        LOG_INFO("[迁移] slot_1 已存�? 旧档保留不动");
        return false;                                       // 安全第一: 不覆�?
    }
    // 写入 slot_1
    mkdir_impl(_save_dir().c_str());
    FILE* out = fopen(_slot_path(1).c_str(), "wb");
    if (!out) { LOG_ERROR("[迁移] slot_1 写入失败"); return false; }
    fwrite(content.c_str(), 1, content.size(), out);
    fclose(out);
    // 验证: 新档存在且非�?
    FILE* verify = fopen(_slot_path(1).c_str(), "rb");
    if (!verify) { LOG_ERROR("[迁移] slot_1 验证失败(不可�?"); return false; }
    fseek(verify, 0, SEEK_END);
    bool ok = ftell(verify) == (long)content.size();
    fclose(verify);
    if (!ok) { remove(_slot_path(1).c_str()); LOG_ERROR("[迁移] slot_1 验证失败(大小不符)"); return false; }
    // 验证通过 �?旧档改名备份 (rename 同目录原子性足�? 失败也不丢数�?
    std::string bak = _legacy_save_path() + ".bak";
    if (rename(_legacy_save_path().c_str(), bak.c_str()) != 0)
        LOG_WARN("[迁移] 旧档改名失败 (数据已复制到 slot_1, 手动处理 save.json)");
    LOG_INFO("[迁移] 旧档 save.json �?slot_1.json 完成 (备份: save.json.bak)");
    return true;
}

// B8: spr 序列�?�?vector<bool> �?"1,0,1"
std::string SaveManager::_encode_spr(const std::vector<bool>& v) {
    std::string out;
    for (size_t i = 0; i < v.size(); i++) {
        if (i > 0) out += ",";
        out += v[i] ? "1" : "0";
    }
    return out;
}

// B8: spr 反序列化 �?"1,0,1" �?vector<bool>
std::vector<bool> SaveManager::_decode_spr(const std::string& s) {
    std::vector<bool> out;
    if (s.empty()) return out;
    for (size_t pos = 0; pos < s.size(); ) {
        size_t comma = s.find(',', pos);
        std::string tok = s.substr(pos, (comma == std::string::npos) ? std::string::npos : (comma - pos));
        out.push_back(tok == "1");  // 宽容: �?1"一律当 false
        if (comma == std::string::npos) break;
        pos = comma + 1;
    }
    return out;
}

// M4e: "1.0,2.0,..." �?vector<float> (空串忽略)
static void _parse_float_list(const std::string& s, std::vector<float>& out) {
    out.clear();
    if (s.empty()) return;
    for (size_t pos = 0; pos < s.size(); ) {
        size_t comma = s.find(',', pos);
        std::string tok = s.substr(pos, (comma == std::string::npos) ? std::string::npos : (comma - pos));
        if (!tok.empty()) out.push_back((float)atof(tok.c_str()));
        if (comma == std::string::npos) break;
        pos = comma + 1;
    }
}
