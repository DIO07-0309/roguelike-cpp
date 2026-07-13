#!/usr/bin/env python
"""Replace large method bodies in game_scene.cpp with delegation calls."""
import sys

path = r'src\game\scenes\game_scene.cpp'
with open(path, 'r', encoding='utf-8-sig') as f:
    lines = f.readlines()

# Map: function_name -> replacement body (single line)
delegates = {
    '_draw_event_ui': 'void GameScene::_draw_event_ui(int sw, int sh) { _interaction.draw_event_ui(sw, sh); }\n',
    '_spawn_floor_npcs': 'void GameScene::_spawn_floor_npcs(int floor, const std::vector<std::pair<int,int>>& rooms) { _interaction.spawn_floor_npcs(floor, rooms); }\n',
    '_start_dialogue': 'void GameScene::_start_dialogue(int npc_index) { _interaction.start_dialogue(npc_index); }\n',
    '_update_dialogue': 'void GameScene::_update_dialogue(float dt) { _interaction.update_dialogue(dt); }\n',
    '_draw_dialogue': 'void GameScene::_draw_dialogue(int sw, int sh) { _interaction.draw_dialogue(sw, sh); }\n',
    '_draw_quest_log': 'void GameScene::_draw_quest_log(int sw, int sh) { _interaction.draw_quest_log(sw, sh); }\n',
}

# Also delete the static helper functions _event_option_count and _event_anim_color
# since they're now in game_scene_interaction.cpp
delete_funcs = ['_event_option_count', '_event_anim_color']

result = []
skip_until_end = 0
i = 0
while i < len(lines):
    line = lines[i]

    # Check if this line starts a function we want to replace
    replaced = False
    for func_name, repl in delegates.items():
        prefix = f'void GameScene::{func_name}'
        if line.startswith(prefix):
            result.append(repl)
            # Skip to after the closing brace
            depth = line.count('{') - line.count('}')
            i += 1
            while i < len(lines) and depth > 0:
                depth += lines[i].count('{') - lines[i].count('}')
                i += 1
            replaced = True
            break

    # Check if this line starts a static helper we want to delete
    if not replaced:
        for func in delete_funcs:
            if f'static int {func}' in line or f'static Color {func}' in line:
                depth = line.count('{') - line.count('}')
                i += 1
                while i < len(lines) and depth > 0:
                    depth += lines[i].count('{') - lines[i].count('}')
                    i += 1
                replaced = True
                break

    if not replaced:
        result.append(line)
        i += 1

with open(path, 'w', encoding='utf-8', newline='') as f:
    f.writelines(result)

print(f'Replaced {len(delegates)} methods, deleted {len(delete_funcs)} helpers')
print(f'Lines: {len(lines)} -> {len(result)}')
