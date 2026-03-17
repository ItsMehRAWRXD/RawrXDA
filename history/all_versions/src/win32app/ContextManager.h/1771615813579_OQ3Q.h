#pragma once
// ContextManager.h — Win32 IDE context state manager
#ifndef RAWRXD_CONTEXT_MANAGER_H
#define RAWRXD_CONTEXT_MANAGER_H

#include <string>
#include <unordered_map>

class ContextManager {
public:
    ContextManager() = default;
    ~ContextManager() = default;

    void SetContext(const std::wstring& key, const std::wstring& value) {
        m_context[key] = value;
    }

    std::wstring GetContext(const std::wstring& key) const {
        auto it = m_context.find(key);
        return (it != m_context.end()) ? it->second : std::wstring{};
    }

    bool HasContext(const std::wstring& key) const {
        return m_context.count(key) > 0;
    }

    void ClearContext() {
        m_context.clear();
    }

    void RemoveContext(const std::wstring& key) {
        m_context.erase(key);
    }

private:
    std::unordered_map<std::wstring, std::wstring> m_context;
};

#endif // RAWRXD_CONTEXT_MANAGER_H
