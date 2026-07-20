#pragma once
#include "registry_provider.h"
#include <unordered_map>

// ============================================================
// G4.1: BuiltinProvider — 内置资源 (resources/ 目录)
// 硬编码模块→路径映射, 不扫描目录
// ============================================================

class BuiltinProvider : public IRegistryProvider {
public:
    BuiltinProvider();

    const char* name() const override { return "Builtin"; }
    int priority() const override { return 0; }
    bool enabled() const override { return true; }

    bool has_module(const std::string& module_name) const override;
    ModuleData load_module(const std::string& module_name) override;

private:
    std::unordered_map<std::string, std::string> _module_paths;
};
