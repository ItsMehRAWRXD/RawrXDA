#include "tool_abi.h"

#include <algorithm>
#include <map>
#include <mutex>
#include <unordered_map>

namespace RawrXD {
namespace DAE {

struct ToolRegistry::Impl {
    std::map<std::string, ToolDescriptor> tools;
    std::unordered_map<std::string, ToolResponse> idempotencyCache;
    mutable std::mutex mu;
};

ToolRegistry::ToolRegistry() : m_impl(std::make_unique<Impl>()) {}
ToolRegistry::~ToolRegistry() = default;

ToolRegistry& ToolRegistry::Instance() {
    static ToolRegistry g;
    return g;
}

void ToolRegistry::Register(ToolDescriptor desc) {
    std::lock_guard<std::mutex> lock(m_impl->mu);
    m_impl->tools[desc.name] = std::move(desc);
}

const ToolDescriptor* ToolRegistry::Find(std::string_view name) const {
    std::lock_guard<std::mutex> lock(m_impl->mu);
    auto it = m_impl->tools.find(std::string(name));
    if (it == m_impl->tools.end()) return nullptr;
    return &it->second;
}

ToolResult_v ToolRegistry::Invoke(const ToolInvocation& inv,
                                  const ToolPolicy& policy) {
    if (inv.idempotencyKey.empty()) {
        return std::unexpected(ToolError::InvalidArgs);
    }

    std::lock_guard<std::mutex> lock(m_impl->mu);

    auto it = m_impl->tools.find(inv.toolName);
    if (it == m_impl->tools.end()) {
        return std::unexpected(ToolError::NotFound);
    }

    const ToolDescriptor& d = it->second;

    if ((d.sideEffects == SideEffectClass::Network && !policy.allowNetwork) ||
        (d.sideEffects == SideEffectClass::Process && !policy.allowProcess) ||
        (d.sideEffects == SideEffectClass::WriteReal && !policy.allowRealWrite)) {
        return std::unexpected(ToolError::SideEffectDenied);
    }

    std::string cacheKey = inv.toolName + ":" + inv.idempotencyKey;
    auto cit = m_impl->idempotencyCache.find(cacheKey);
    if (cit != m_impl->idempotencyCache.end()) {
        ToolResponse cached = cit->second;
        cached.fromCache = true;
        return cached;
    }

    if (inv.dryRun) {
        ToolResponse dry{};
        dry.status = ToolError::None;
        dry.resultJson = "{}";
        dry.diagnostics = "dry-run";
        m_impl->idempotencyCache[cacheKey] = dry;
        return dry;
    }

    auto start = std::chrono::steady_clock::now();
    auto res = d.fn(inv);
    auto end = std::chrono::steady_clock::now();

    if (!res.has_value()) {
        return std::unexpected(res.error());
    }

    ToolResponse out = *res;
    if (out.elapsed.count() == 0) {
        out.elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    }
    m_impl->idempotencyCache[cacheKey] = out;
    return out;
}

void ToolRegistry::ClearCache() {
    std::lock_guard<std::mutex> lock(m_impl->mu);
    m_impl->idempotencyCache.clear();
}

std::vector<std::string> ToolRegistry::RegisteredNames() const {
    std::lock_guard<std::mutex> lock(m_impl->mu);
    std::vector<std::string> names;
    names.reserve(m_impl->tools.size());
    for (const auto& [k, _] : m_impl->tools) names.push_back(k);
    return names;
}

} // namespace DAE
} // namespace RawrXD
