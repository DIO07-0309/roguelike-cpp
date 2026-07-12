#include "floor_config.h"

// D1: 15层配置表. D2 Step4: team_coop. D2 Step5: arena_density
// arena_density = 每房间放置的ArenaObject数: 章0=0, 章1=1, 章2=2, Boss=2

static const FloorConfig FLOORS[] = {
    { 1,0,"地牢入口","你推开沉重的铁门，空气弥漫着腐败的气息。",     1.0f,1.0f,5, {80,12, 5, 0, 3, 0},2,false,false,0.2f,0,"dungeon"},
    { 2,0,"地牢入口","昏暗的走廊深处传来低沉的嘶吼。",             1.15f,1.12f,5,{80,12, 5, 0, 3, 0},2,false,false,0.2f,0,"dungeon"},
    { 3,0,"地牢入口","墙上的火把已燃尽了千年，你只能依靠直觉。",   1.30f,1.25f,6,{80,12, 5, 0, 3, 0},2,false,false,0.2f,0,"dungeon"},
    { 4,0,"地牢入口","前方出现温暖的火光——一个安全的喘息之地。",  1.50f,1.40f,5,{70,12, 8, 4, 4, 2},3,false,true, 0.2f,1,"dungeon"},
    { 5,0,"地牢入口",nullptr,                                     1.0f,1.0f,1, {70,10, 8, 6, 4, 2},0,true, false,0.2f,2,"boss"},
    { 6,1,"幽暗深渊","你顺着裂缝下坠，落入了地牢更深的黑暗中。",   1.60f,1.50f,6,{65,12,12, 6, 4, 1},2,false,false,0.6f,1,"dungeon"},
    { 7,1,"幽暗深渊","黑暗中传来施法的吟唱声——这里有萨满。",     1.80f,1.65f,6,{65,12,12, 6, 4, 1},2,false,false,0.6f,1,"dungeon"},
    { 8,1,"幽暗深渊","洞穴越来越窄，每一步都需要格外小心。",       2.00f,1.80f,7,{65,12,12, 6, 4, 1},2,false,false,0.6f,1,"dungeon"},
    { 9,1,"幽暗深渊","荧光苔藓照亮了开阔的洞窟——你可以休整。",   2.20f,2.00f,6,{70,12, 8, 4, 4, 2},3,false,true, 0.6f,2,"dungeon"},
    {10,1,"幽暗深渊",nullptr,                                     1.0f,1.0f,1, {70,10, 8, 6, 4, 2},0,true, false,0.6f,2,"boss"},
    {11,2,"熔岩炼狱","灼热的气流迎面扑来，这是地牢最深处。",      2.40f,2.20f,7,{55,10,10,10, 8, 7},3,false,false,1.0f,2,"dungeon"},
    {12,2,"熔岩炼狱","空气中弥漫着硫磺的味道，前方的敌人更加危险。",2.70f,2.40f,7,{55,10,10,10, 8, 7},3,false,false,1.0f,2,"dungeon"},
    {13,2,"熔岩炼狱","你听到无数怪物的嘶吼从四面八方涌来。",      3.00f,2.60f,8,{55,10,10,10, 8, 7},3,false,false,1.0f,3,"dungeon"},
    {14,2,"熔岩炼狱","一座宏伟的遗迹出现——远古文明的最后痕迹。", 3.30f,2.90f,7,{70,12, 8, 4, 4, 2},3,false,true, 1.0f,3,"dungeon"},
    {15,2,"熔岩炼狱",nullptr,                                     1.0f,1.0f,1, {70,10, 8, 6, 4, 2},0,true, false,1.0f,3,"boss"},
};

static const ChapterConfig CHAPTERS[] = {
    {0, "地牢入口", 1, 5}, {1, "幽暗深渊", 6, 10}, {2, "熔岩炼狱", 11, 15},
};

const FloorConfig* get_floor_config(int f) { if(f<1||f>15) return &FLOORS[0]; return &FLOORS[f-1]; }
const ChapterConfig* get_chapter_config(int ch) { if(ch<0||ch>2) return &CHAPTERS[0]; return &CHAPTERS[ch]; }
const char* get_floor_story_msg(int f) { return get_floor_config(f)->story_msg; }
int get_chapter_for_floor(int f) { return get_floor_config(f)->chapter; }
