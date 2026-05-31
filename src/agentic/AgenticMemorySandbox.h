// ============================================================================
// AgenticMemorySandbox.h — Phase 2: Instruction-Level Execution Guard
// ============================================================================
#pragma once

#include <vector>
#include <cstdint>
#include <string>
#include <windows.h>

namespace SovereignAssembler {

/**
 * @brief The AgenticMemorySandbox enforces execution boundaries for 
 * hot-patched AVX-512 kernels. It prevents unauthorized code from 
 * escaping the designated high-speed execution layer.
 */
class AgenticMemorySandbox {
public:
    static AgenticMemorySandbox& Instance();

    /**
     * @brief Allocates an executable page with a guard boundary.
     * Use this for hot-patched kernels (e.g., tokenizer, matmul).
     */
    void* AllocateExecutionPage(size_t size);

    /**
     * @brief Validates that a instruction pointer (EIP/RIP) is within 
     * the authorized sandbox region. 
     */
    bool IsPointerAuthorized(void* ptr);

    /**
     * @brief Seals a memory page to be read-only + executable (XOM), 
     * preventing runtime modification after the initial write.
     */
    bool SealExecutionPage(void* ptr, size_t size);

private:
    AgenticMemorySandbox() = default;
    std::vector<std::pair<void*, size_t>> m_authorizedRegions;
};

} // namespace SovereignAssembler
