#pragma once
#include <unordered_map>
#include <typeindex>
#include <typeinfo>
#include <cstdio>

// ============================================================
// D7 Step6: ServiceLocator — 编译期类型安全的全局服务定位器
// 消除跨模块直接依赖, 替代到处传递 GameScene*
// ============================================================

class ServiceLocator {
public:
    static ServiceLocator& inst();

    // ── 注册 ──
    template<typename T>
    static void provide(T* service) {
        inst()._services[std::type_index(typeid(T))] = static_cast<void*>(service);
#ifdef _DEBUG
        printf("[ServiceLocator] + %s\n", typeid(T).name());
#endif
    }

    // ── 查询 ──
    template<typename T>
    static T* get() {
        auto& map = inst()._services;
        auto it = map.find(std::type_index(typeid(T)));
        return (it != map.end()) ? static_cast<T*>(it->second) : nullptr;
    }

    template<typename T>
    static bool has() {
        return inst()._services.count(std::type_index(typeid(T))) > 0;
    }

    // ── 管理 ──
    static void remove_all() { inst()._services.clear(); }

private:
    ServiceLocator() = default;
    std::unordered_map<std::type_index, void*> _services;
};

// 单例实现 (inline in header)
inline ServiceLocator& ServiceLocator::inst() {
    static ServiceLocator sl;
    return sl;
}
