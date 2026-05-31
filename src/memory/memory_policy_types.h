#pragma once
#include <cstdint>
namespace RawrXD::Memory {
    enum class MemoryAction { RETAIN, COMPRESS, TIERDOWN, EVICT, RECOMPUTE, PREFETCH };
    struct MemoryState { uint64_t vramBudget; uint64_t ramBudget; double throughput; };
    enum class MemoryMorphMode { DIRECT, HYBRID, AGENTIC };
} // namespace RawrXD::Memory
