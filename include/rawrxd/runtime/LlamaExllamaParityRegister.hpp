#pragma once

#include <cstddef>

namespace RawrXD::Runtime::LlamaExllamaParity {

/// Canonical tensor / block roles for **llama.cpp** ↔ **ExLlamaV2** mapping (parity register).
struct RoleEntry {
    const char* canonicalId;
    const char* llamaCppName;
    const char* exllamaV2Name;
};

/// Null-terminated table (iterate until canonicalId == nullptr).
[[nodiscard]] const RoleEntry* roleTable();
[[nodiscard]] std::size_t roleTableCount();

}  // namespace RawrXD::Runtime::LlamaExllamaParity
