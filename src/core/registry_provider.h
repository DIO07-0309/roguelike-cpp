#pragma once
#include <string>
#include <vector>

// ============================================================
// G4.1: RegistryProvider — 数据源抽象层
// Provider 提供数据, 不解析业务。
// Registry 保存数据, 不关心来源。
// RegistryBuilder 负责协调, 不参与具体数据模型。
// ============================================================

// ── Merge 模式 ──
enum class MergeMode {
    Skip,       // 同 id 跳过 (不覆盖)
    Replace,    // 同 id 覆盖
    MergePatch  // G4.3: 同 id → 字段级合并 (__patch 标记)
};

// ── Provider 返回的结构 ──
struct ModuleData {
    std::string text;         // JSON 文本
    std::string source;       // "resources/enemies.json" | "mods/hard_mode/enemies.json"
    std::string provider;     // "Builtin" | "HardMode"
};

// ── Build 日志记录 ──
struct BuildRecord {
    std::string module;       // "enemy" | "boss" | ...
    std::string provider;     // "Builtin" | "HardMode"
    std::string source;       // 文件路径
    bool is_merge = false;    // true=覆盖模式, false=首次加载
    bool is_patch = false;    // G4.3: true=MergePatch, false=Replace
    int entries_loaded = 0;   // 本次加载条数
    int entries_merged = 0;   // G4.3: 字段合并条数 (patch only)
};

// ── G4.1.5: Validation ──
enum class ValSeverity { Warning, Error };

struct ValFinding {
    ValSeverity severity;
    std::string module;        // "skill" | "enemy" | ...
    std::string entry_id;      // "slash" | "shadow_knight" | ...
    std::string message;       // "references unknown buff 'megafire'"
};

// ── Provider 接口 ──
// 只提供数据文本, 不解析 JSON
class IRegistryProvider {
public:
    virtual ~IRegistryProvider() = default;

    virtual const char* name() const = 0;
    virtual int priority() const = 0;           // 数字越低越先加载
    virtual bool enabled() const = 0;           // G4.1: 预留 Mod 启用/禁用

    // 查询该 provider 是否为某模块提供数据
    virtual bool has_module(const std::string& module_name) const = 0;

    // 读取模块的 JSON 文本 (含来源信息)
    // 返回 ModuleData{} (text 为空) 表示无数据
    virtual ModuleData load_module(const std::string& module_name) = 0;
};
