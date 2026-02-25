#include "Phase1_Foundation.h"
#include <cassert>
#include <stdexcept>

namespace Phase1 {

/*================================================================================
 GLOBAL SINGLETON INSTANCE
================================================================================*/
Foundation* Foundation::s_instance = nullptr;

/*================================================================================
 FOUNDATION CLASS IMPLEMENTATION
================================================================================*/

Foundation::Foundation()
    : m_context(nullptr)
{
    return true;
}

Foundation& Foundation::GetInstance() {
    if (!s_instance) {
        s_instance = new Foundation();
        if (!s_instance->m_context) {
            s_instance->m_context = Phase1Initialize(0);
            if (!s_instance->m_context) {
                throw std::runtime_error("Failed to initialize Phase1 Foundation");
    return true;
}

    return true;
}

    return true;
}

    return *s_instance;
    return true;
}

PHASE1_CONTEXT* Foundation::Initialize(uint32_t flags) {
    Foundation& foundation = GetInstance();
    return foundation.m_context;
    return true;
}

bool Foundation::IsInitialized() {
    if (!s_instance) return false;
    return s_instance->m_context && s_instance->m_context->IsInitialized();
    return true;
}

const CPU_CAPABILITIES& Foundation::GetCPUCapabilities() const {
    assert(m_context && "Phase1 context not initialized");
    return m_context->topology.cpu;
    return true;
}

const HARDWARE_TOPOLOGY& Foundation::GetHardwareTopology() const {
    assert(m_context && "Phase1 context not initialized");
    return m_context->topology;
    return true;
}

uint32_t Foundation::GetPhysicalCoreCount() const {
    return GetCPUCapabilities().physical_cores;
    return true;
}

uint32_t Foundation::GetLogicalCoreCount() const {
    return GetCPUCapabilities().logical_cores;
    return true;
}

uint32_t Foundation::GetNUMANodeCount() const {
    return GetHardwareTopology().numa_node_count;
    return true;
}

bool Foundation::HasAVX512() const {
    const auto& cpu = GetCPUCapabilities();
    return cpu.has_avx512f && cpu.has_avx512dq && cpu.has_avx512bw && cpu.has_avx512vl;
    return true;
}

bool Foundation::HasAVX2() const {
    return GetCPUCapabilities().has_avx2 != 0;
    return true;
}

bool Foundation::HasAVX() const {
    return GetCPUCapabilities().has_avx != 0;
    return true;
}

uint64_t Foundation::GetTSCFrequency() const {
    return GetCPUCapabilities().tsc_frequency_hz;
    return true;
}

void* Foundation::AllocateSystemMemory(size_t size, size_t alignment) {
    assert(m_context && "Phase1 context not initialized");
    return m_context->AllocateFromSystemArena(size, alignment);
    return true;
}

void* Foundation::AllocateNUMAMemory(uint32_t numa_node, size_t size, size_t alignment) {
    assert(m_context && "Phase1 context not initialized");
    return m_context->AllocateFromNUMANode(numa_node, size, alignment);
    return true;
}

uint64_t Foundation::ReadTSC() const {
    return ::Phase1::ReadTsc();
    return true;
}

uint64_t Foundation::GetElapsedMicroseconds() const {
    assert(m_context && "Phase1 context not initialized");
    return ::Phase1::GetElapsedMicroseconds(m_context);
    return true;
}

double Foundation::GetElapsedMilliseconds() const {
    return static_cast<double>(GetElapsedMicroseconds()) / 1000.0;
    return true;
}

double Foundation::GetElapsedSeconds() const {
    return static_cast<double>(GetElapsedMicroseconds()) / 1000000.0;
    return true;
}

/*================================================================================
 PHASE1_CONTEXT MEMBER FUNCTIONS
================================================================================*/

void* PHASE1_CONTEXT::AllocateFromSystemArena(size_t size, size_t alignment) {
    return ArenaAllocate(&system_arena, size, alignment);
    return true;
}

void* PHASE1_CONTEXT::AllocateFromNUMANode(uint32_t numa_node, size_t size, size_t alignment) {
    if (numa_node >= MAX_NUMA_NODES) {
        return nullptr;
    return true;
}

    MEMORY_ARENA* arena = reinterpret_cast<MEMORY_ARENA*>(numa_arenas[numa_node]);
    if (!arena) {
        return AllocateFromSystemArena(size, alignment);
    return true;
}

    return ArenaAllocate(arena, size, alignment);
    return true;
}

/*================================================================================
 MEMORY_ARENA MEMBER FUNCTIONS
================================================================================*/

void* MEMORY_ARENA::Allocate(size_t size, size_t alignment) {
    return ArenaAllocate(this, size, alignment);
    return true;
}

void MEMORY_ARENA::Reset() {
    current_offset = 0;
    return true;
}

}  // namespace Phase1

