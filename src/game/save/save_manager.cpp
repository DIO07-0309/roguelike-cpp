#include "save_manager.h"
#include "player.h"
#include "skill.h"
#include "item.h"
#include "core/logger.h"
#include "config.h"
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
std::string SaveManager::_save_path() { return _save_dir() + "/save.json"; }

bool SaveManager::save_exists() {
    FILE* f = fopen(_save_path().c_str(), "r");
    if (!f) return false; fclose(f); return true;
}

// ---- 辅助: trim ----
static std::string trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    size_t b = s.find_last_not_of(" \t\r\n");
    return (a == std::string::npos) ? "" : s.substr(a, b - a + 1);
}

// ---- 序列化 ----
bool SaveManager::save_game(Player* player, int floor, int max_f) {
    mkdir_impl(_save_dir().c_str());
    FILE* f = fopen(_save_path().c_str(), "w");
    if (!f) { LOG_ERROR("存档无法写入"); return false; }
    auto& c = player->combat;
    auto& inv = player->inventory;

    fprintf(f, "v:1\n");
    fprintf(f, "floor:%d\n", floor);
    fprintf(f, "maxf:%d\n", max_f);
    fprintf(f, "lv:%d\n", player->level);
    fprintf(f, "xp:%d\n", player->xp);
    fprintf(f, "xpt:%d\n", player->xp_to_next);
    fprintf(f, "mhp:%d\n", c.max_hp);
    fprintf(f, "chp:%d\n", c.current_hp);
    fprintf(f, "atk:%d\n", c.attack);           // 基础值（每次升级+2）
    fprintf(f, "pd:%d\n", c.physical_defense);
    fprintf(f, "md:%d\n", c.magical_defense);

    // 主动技能: name,lv;name,lv;...
    fprintf(f, "act:");
    for (auto& s : player->skills.active_skills) {
        const char* nm = "?";
        if      (dynamic_cast<SlashSkill*>(s.get()))    nm = "Slash";
        else if (dynamic_cast<FireballSkill*>(s.get())) nm = "Fireball";
        else if (dynamic_cast<SelfHealSkill*>(s.get())) nm = "SelfHeal";
        else if (dynamic_cast<TheWorldSkill*>(s.get()))  nm = "TheWorld";
        fprintf(f, "%s,%d;", nm, s->level);
    }
    fprintf(f, "\n");

    // 被动技能
    fprintf(f, "pas:");
    for (auto& s : player->skills.passives) {
        const char* nm = "?";
        if      (dynamic_cast<IronSkinSkill*>(s.get())) nm = "IronSkin";
        else if (dynamic_cast<BerserkSkill*>(s.get()))  nm = "Berserk";
        fprintf(f, "%s,%d;", nm, s->level);
    }
    fprintf(f, "\n");

    // 背包物品: name,RARITY,type,val1,val2,val3;...
    fprintf(f, "inv:");
    for (auto& item : inv.items) {
        auto* eq = dynamic_cast<EquipmentItem*>(item.get());
        auto* cn = dynamic_cast<ConsumableItem*>(item.get());
        if (eq && eq->slot != "charm") {
            fprintf(f, "%s,%d,%s,%d,%d,%d;",
                eq->base_name.c_str(), (int)eq->rarity, eq->slot.c_str(),
                eq->atk_bonus, eq->pdef_bonus, eq->mdef_bonus);
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

    fprintf(f, "eqa:");
    if (inv.equipped["armor"]) {
        auto& eq = inv.equipped["armor"];
        fprintf(f, "%s,%d,%s,%d,%d,%d", eq->base_name.c_str(), (int)eq->rarity,
                eq->slot.c_str(), eq->atk_bonus, eq->pdef_bonus, eq->mdef_bonus);
    }
    fprintf(f, "\n");

    // Buff 状态 (玩家)
    fprintf(f, "buf:");
    for (auto& b : player->active_buffs) {
        fprintf(f, "%s,%d,%.2f,%.2f;",
            b.id.c_str(), b.stacks, b.remaining, b.tick_timer);
    }
    fprintf(f, "\n");

    fclose(f);
    LOG_INFO("存档: 第%d层 Lv%d HP:%d/%d %zu技能 %zu物品 %zuBuff",
        floor, player->level, c.current_hp, c.max_hp,
        player->skills.active_skills.size(), inv.items.size(),
        player->active_buffs.size());
    return true;
}

// ---- 反序列化 ----
SaveData* SaveManager::load_save() {
    if (!save_exists()) return nullptr;
    FILE* f = fopen(_save_path().c_str(), "r");
    if (!f) return nullptr;

    // 读取所有行
    std::vector<std::string> lines;
    char buf[4096];
    while (fgets(buf, sizeof(buf), f)) {
        std::string line = trim(buf);
        if (!line.empty()) lines.push_back(line);
    }
    fclose(f);

    // 解析行: "key:value"
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

    int floor = getV("floor", 1);
    int maxf  = getV("maxf", 1);
    int lv    = getV("lv", 1);
    int xp    = getV("xp", 0);
    int xpt   = getV("xpt", Player::calc_xp_for_level(lv + 1));
    int mhp   = getV("mhp", PLAYER_MAX_HP);
    int chp   = getV("chp", mhp);
    int atk   = getV("atk", PLAYER_ATTACK);
    int pd    = getV("pd", PLAYER_PDEF);
    int md    = getV("md", PLAYER_MDEF);

    auto p = std::make_unique<Player>(
        TILE_SIZE * 2, TILE_SIZE * 2, PLAYER_SPEED, mhp, atk, pd, md);
    p->combat.current_hp = chp;
    p->level = lv;
    p->xp = xp;
    p->xp_to_next = xpt;

    // 恢复主动技能
    std::string act = getS("act");
    if (!act.empty()) {
        for (size_t pos = 0; pos < act.size(); ) {
            size_t semi = act.find(';', pos);
            if (semi == std::string::npos) break;
            std::string tok = act.substr(pos, semi - pos);
            pos = semi + 1;
            size_t comma = tok.find(',');
            if (comma == std::string::npos) continue;
            std::string nm = tok.substr(0, comma);
            int lvl = atoi(tok.substr(comma + 1).c_str());
            std::unique_ptr<Skill> sk;
            if (nm == "Slash") sk = std::make_unique<SlashSkill>();
            else if (nm == "Fireball") sk = std::make_unique<FireballSkill>();
            else if (nm == "SelfHeal") sk = std::make_unique<SelfHealSkill>();
            else if (nm == "TheWorld") sk = std::make_unique<TheWorldSkill>();
            else continue;
            while (sk->level < lvl) sk->upgrade();
            p->skills.learn(std::move(sk));
        }
    }

    // 被动
    std::string pas = getS("pas");
    if (!pas.empty()) {
        for (size_t pos = 0; pos < pas.size(); ) {
            size_t semi = pas.find(';', pos);
            if (semi == std::string::npos) break;
            std::string tok = pas.substr(pos, semi - pos);
            pos = semi + 1;
            size_t comma = tok.find(',');
            if (comma == std::string::npos) continue;
            std::string nm = tok.substr(0, comma);
            int lvl = atoi(tok.substr(comma + 1).c_str());
            std::unique_ptr<Skill> sk;
            if (nm == "IronSkin") sk = std::make_unique<IronSkinSkill>();
            else if (nm == "Berserk") sk = std::make_unique<BerserkSkill>();
            else continue;
            while (sk->level < lvl) sk->upgrade();
            p->skills.learn(std::move(sk));
        }
    }
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
                p->inventory.items.push_back(
                    std::make_shared<EquipmentItem>(nm, rar, typ, a, pd2, md2));
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
            atoi(parts[3].c_str()), atoi(parts[4].c_str()), atoi(parts[5].c_str()));
    };

    auto eqw = parseEquip("eqw");
    if (eqw) { eqw->apply(p.get()); p->inventory.equipped["weapon"] = eqw; }
    auto eqa = parseEquip("eqa");
    if (eqa) { eqa->apply(p.get()); p->inventory.equipped["armor"] = eqa; }

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
                LOG_WARN("[BUF] 读档跳过坏条目: %s", tok.c_str());
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
                // 过期或无效 buff：跳过
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
    d->player = std::move(p);

    LOG_INFO("读档: 第%d层 Lv%d HP:%d/%d %zu技能 %zu物品 %zuBuff",
        floor, lv, chp, mhp,
        d->player->skills.active_skills.size(), d->player->inventory.items.size(),
        d->player->active_buffs.size());
    return d;
}

void SaveManager::delete_save() {
    remove(_save_path().c_str());
    LOG_INFO("存档已删除");
}
