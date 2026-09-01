#include "first_hint.h"
#include "meta_progression.h"
#include "scenes/game_scene.h"
#include "director/presentation_system_director.h"

bool first_hint(GameScene& gs, const std::string& hint_id,
                const std::string& line1, const std::string& line2) {
    if (MetaSystem::hint_already_shown(hint_id)) return false;
    MetaSystem::mark_hint_shown(hint_id);
    std::string msg = line2.empty() ? line1 : (line1 + "  " + line2);
    // GameScene 提供公开转发 (game_scene.h: show_hint)
    gs.show_hint(msg.c_str(), 4.5f);
    return true;
}
