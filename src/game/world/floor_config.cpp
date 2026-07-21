#include "floor_config.h"

// D1: 15层配置表. D2 Step4: team_coop. D2 Step5: arena_density
// arena_density = 每房间放置的ArenaObject数: 章0=0, 章1=1, 章2=2, Boss=2
// G5.3: enemy_weights[12] = NORMAL,Archer,Shaman,Bomber,Tank,Elite,Charger,Summoner,Sniper,Controller,Ambush,Guardian
// G5.5: special_room_count boosted for richer run variety

static const FloorConfig FLOORS[] = {
    //  D9: HP/ATK 更平滑, 怪物数量微增, team_coop 随楼层递增
    // G6.1: F1-5 Prison biome, F6-10 Volcano biome, F11-15 Abyss biome — BGM per biome
    { 1,0,"地牢入口","你推开沉重的铁门，空气弥漫着腐败的气息。",     1.0f,1.0f,4, {90, 8, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0},3,false,false,0.10f,0,"prison"},
    { 2,0,"地牢入口","昏暗的走廊深处传来低沉的嘶吼。",             1.10f,1.08f,5,{85,10, 4, 0, 2, 0, 0, 0, 0, 0, 0, 0},3,false,false,0.15f,0,"prison"},
    { 3,0,"地牢入口","墙上的火把已燃尽了千年，你只能依靠直觉。",   1.25f,1.18f,5,{80,10, 6, 2, 2, 0, 0, 0, 0, 0, 0, 0},3,false,false,0.20f,0,"prison"},
    { 4,0,"地牢入口","前方出现温暖的火光——一个安全的喘息之地。",  1.40f,1.30f,5,{70,12, 8, 4, 4, 2, 2, 0, 2, 2, 0, 2},4,false,true, 0.25f,1,"prison"},
    { 5,0,"地牢入口",nullptr,                                     1.0f,1.0f,1, {70,10, 8, 6, 4, 2, 2, 2, 0, 0, 0, 0},0,true, false,0.25f,2,"boss"},
    { 6,1,"幽暗深渊","你顺着裂缝下坠，落入了地牢更深的黑暗中。",   1.55f,1.42f,6,{65,12,10, 6, 4, 2, 3, 2, 3, 2, 2, 2},3,false,false,0.40f,1,"volcano"},
    { 7,1,"幽暗深渊","黑暗中传来施法的吟唱声——这里有萨满。",     1.70f,1.55f,6,{60,12,12, 6, 5, 2, 3, 2, 4, 3, 2, 3},4,false,false,0.50f,1,"volcano"},
    { 8,1,"幽暗深渊","洞穴越来越窄，每一步都需要格外小心。",       1.85f,1.68f,7,{58,12,12, 6, 5, 3, 4, 3, 4, 4, 3, 4},4,false,false,0.55f,1,"volcano"},
    { 9,1,"幽暗深渊","荧光苔藓照亮了开阔的洞窟——你可以休整。",   2.00f,1.80f,6,{65,12, 8, 4, 4, 3, 3, 3, 3, 3, 2, 3},5,false,true, 0.60f,2,"volcano"},
    {10,1,"幽暗深渊",nullptr,                                     1.0f,1.0f,1, {70,10, 8, 6, 4, 2, 3, 3, 0, 0, 0, 0},0,true, false,0.60f,2,"boss"},
    {11,2,"熔岩炼狱","灼热的气流迎面扑来，这是地牢最深处。",      2.15f,1.95f,7,{55,10,10, 8, 7, 5, 5, 4, 5, 5, 4, 5},4,false,false,0.80f,2,"abyss"},
    {12,2,"熔岩炼狱","空气中弥漫着硫磺的味道，前方的敌人更加危险。",2.35f,2.10f,7,{52,10,10, 8, 7, 6, 6, 4, 6, 5, 4, 6},4,false,false,0.85f,2,"abyss"},
    {13,2,"熔岩炼狱","你听到无数怪物的嘶吼从四面八方涌来。",      2.55f,2.25f,8,{50,10,10, 8, 8, 7, 6, 5, 6, 6, 5, 7},5,false,false,0.90f,3,"abyss"},
    {14,2,"熔岩炼狱","一座宏伟的遗迹出现——远古文明的最后痕迹。", 2.75f,2.40f,7,{65,12, 8, 4, 4, 3, 4, 4, 5, 4, 4, 6},5,false,true, 1.00f,3,"abyss"},
    {15,2,"熔岩炼狱",nullptr,                                     1.0f,1.0f,1, {70,10, 8, 6, 4, 2, 4, 4, 0, 0, 0, 0},0,true, false,1.00f,3,"boss"},
};

static const ChapterConfig CHAPTERS[] = {
    {0, "地牢入口", 1, 5}, {1, "幽暗深渊", 6, 10}, {2, "熔岩炼狱", 11, 15},
};

const FloorConfig* get_floor_config(int f) { if(f<1||f>15) return &FLOORS[0]; return &FLOORS[f-1]; }
const ChapterConfig* get_chapter_config(int ch) { if(ch<0||ch>2) return &CHAPTERS[0]; return &CHAPTERS[ch]; }
const char* get_floor_story_msg(int f) { return get_floor_config(f)->story_msg; }
int get_chapter_for_floor(int f) { return get_floor_config(f)->chapter; }
