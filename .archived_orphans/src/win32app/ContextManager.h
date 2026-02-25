#pragma once

#include <string>
#include <vector>

// Minimal stub to satisfy build system references.
// The Win32 IDE may replace this with a full implementation.
class ContextManager {
public:
    void Clear() noexcept {}

    void AddLine(const std::wstring& /*line*/) {}

    std::vector<std::wstring> Snapshot() const { return {}; }
};
