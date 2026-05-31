// rawr_aperture_bridge.cpp
// C++ implementation of Sovereign Bridge Controller
// Links rawr_aperture_bypass.asm primitives into architecture-agnostic runtime

#include "rawr_aperture_bridge.h"
#include <iostream>
#include <cmath>

namespace rawr {

// ============================================================================
// PRIVILEGE MANAGER
// ============================================================================

bool PrivilegeManager::EnableLockMemoryPrivilege() {
    HANDLE hToken;
    TOKEN_PRIVILEGES tp;
    
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken)) {
        return false;
    }
    
    if (!LookupPrivilegeValue(NULL, SE_LOCK_MEMORY_NAME, &tp.Privileges[0].Luid)) {
        CloseHandle(hToken);
        return false;
    }
    
    tp.PrivilegeCount = 1;
    tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
    
    BOOL result = AdjustTokenPrivileges(hToken, FALSE, &tp, 0, NULL, NULL);
    CloseHandle(hToken);
    
    return result && (GetLastError() == ERROR_SUCCESS);
}

bool PrivilegeManager::CanUseLargePages() {
    return RawrLargePagesAvailable();
}

std::string PrivilegeManager::GetLastErrorMessage() {
    DWORD err = GetLastError();
    LPSTR msg = nullptr;
    FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
                  NULL, err, 0, (LPSTR)&msg, 0, NULL);
    std::string result = msg ? msg : "Unknown error";
    LocalFree(msg);
    return result;
}

// ============================================================================
// APERTURE ALLOCATION
// ============================================================================

void ApertureAllocation::release() {
    if (pinned) {
        unpin();
    }
    if (cpu_ptr) {
        VirtualFree(cpu_ptr, 0, MEM_RELEASE);
        cpu_ptr = nullptr;
    }
    gpu_ptr = nullptr;
    size = 0;
}

bool ApertureAllocation::pin() {
    if (!cpu_ptr || pinned) return false;
    pinned = RawrPinMemory(cpu_ptr, size);
    return pinned;
}

bool ApertureAllocation::unpin() {
    if (!cpu_ptr || !pinned) return false;
    bool ok = RawrUnpinMemory(cpu_ptr, size);
    if (ok) pinned = false;
    return ok;
}

void ApertureAllocation::prefetch() const {
    if (cpu_ptr && size > 0) {
        RawrPrefetchMemory(cpu_ptr, size);
    }
}

void ApertureAllocation::flush_cache() const {
    if (cpu_ptr && size > 0) {
        RawrFlushCacheLines(cpu_ptr, size);
    }
}

// ============================================================================
// APERTURE ALLOCATOR (static state)
// ============================================================================

size_t ApertureAllocator::s_total_aperture = 0;
size_t ApertureAllocator::s_used_aperture = 0;
bool ApertureAllocator::s_initialized = false;

bool ApertureAllocator::Initialize() {
    if (s_initialized) return true;
    s_total_aperture = 192ULL * 1024 * 1024 * 1024; // 192GB DDR5
    s_used_aperture = 0;
    s_initialized = true;
    return true;
}

std::unique_ptr<ApertureAllocation> ApertureAllocator::Allocate(
    size_t size, bool use_large_pages, bool pin) {
    
    if (!s_initialized) Initialize();
    
    auto alloc = std::make_unique<ApertureAllocation>();
    alloc->size = size;
    
    if (use_large_pages && PrivilegeManager::CanUseLargePages()) {
        alloc->cpu_ptr = RawrAllocateHugePages(size);
        alloc->large_pages = (alloc->cpu_ptr != nullptr);
    }
    
    if (!alloc->cpu_ptr) {
        alloc->cpu_ptr = VirtualAlloc(NULL, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        alloc->large_pages = false;
    }
    
    if (!alloc->cpu_ptr) {
        return nullptr;
    }
    
    if (pin) {
        alloc->pin();
    }
    
    s_used_aperture += size;
    return alloc;
}

bool ApertureAllocator::MapToGPUAperture(ApertureAllocation& alloc) {
    // For now, GPU sees CPU pointer via GART (driver handles translation)
    // Real HIP mapping would go here when RAWR_ENABLE_HIP is defined
    alloc.gpu_ptr = alloc.cpu_ptr;
    alloc.location = MemoryLocation::DDR5_APerture;
    return true;
}

bool ApertureAllocator::UnmapFromGPUAperture(ApertureAllocation& alloc) {
    alloc.gpu_ptr = nullptr;
    alloc.location = MemoryLocation::SYSTEM_RAM;
    return true;
}

size_t ApertureAllocator::GetTotalApertureSize() { return s_total_aperture; }
size_t ApertureAllocator::GetUsedApertureSize() { return s_used_aperture; }
size_t ApertureAllocator::GetAvailableApertureSize() {
    return s_total_aperture > s_used_aperture ? s_total_aperture - s_used_aperture : 0;
}

// ============================================================================
// TENSOR MEMORY MANAGER (static state)
// ============================================================================

std::vector<std::unique_ptr<TensorMemory>> TensorMemoryManager::s_tensors;
size_t TensorMemoryManager::s_vram_budget = 14ULL * 1024 * 1024 * 1024; // 14GB VRAM
size_t TensorMemoryManager::s_aperture_budget = 192ULL * 1024 * 1024 * 1024;
size_t TensorMemoryManager::s_vram_used = 0;
size_t TensorMemoryManager::s_aperture_used = 0;

bool TensorMemoryManager::Initialize(size_t estimated_model_size) {
    s_tensors.clear();
    s_vram_used = 0;
    s_aperture_used = 0;
    
    // If model fits in VRAM budget, use VRAM path
    // Otherwise, overflow to aperture
    if (estimated_model_size <= s_vram_budget) {
        s_vram_used = estimated_model_size;
    } else {
        s_vram_used = s_vram_budget;
        s_aperture_used = estimated_model_size - s_vram_budget;
    }
    
    return true;
}

TensorMemory* TensorMemoryManager::AllocateTensor(
    const std::string& name, size_t size, uint32_t layer_idx, bool is_expert) {
    
    auto mem = std::make_unique<TensorMemory>();
    mem->name = name;
    mem->size = size;
    mem->layer_idx = layer_idx;
    mem->is_expert = is_expert;
    
    // Overflow logic: experts and large tensors go to aperture
    bool use_aperture = is_expert || (s_vram_used + size > s_vram_budget);
    
    auto alloc = ApertureAllocator::Allocate(size, true, true);
    if (!alloc) return nullptr;
    
    if (use_aperture) {
        ApertureAllocator::MapToGPUAperture(*alloc);
        mem->data = MakeDualPointer(alloc->gpu_ptr, MemoryLocation::DDR5_APerture);
        s_aperture_used += size;
    } else {
        mem->data = MakeDualPointer(alloc->cpu_ptr, MemoryLocation::VRAM_LOCAL);
        s_vram_used += size;
    }
    
    // Store allocation ownership
    // (In real implementation, transfer ownership to manager)
    
    TensorMemory* ptr = mem.get();
    s_tensors.push_back(std::move(mem));
    return ptr;
}

TensorMemory* TensorMemoryManager::GetTensor(const std::string& name) {
    for (auto& t : s_tensors) {
        if (t->name == name) return t.get();
    }
    return nullptr;
}

void TensorMemoryManager::PrefetchExperts(const std::vector<uint32_t>& expert_indices) {
    std::vector<void*> ptrs;
    for (auto& t : s_tensors) {
        if (t->is_expert) {
            ptrs.push_back(GetPointer(t->data));
        }
    }
    if (!ptrs.empty()) {
        RawrPreloadExpertWeights(ptrs.data(), ptrs.size(), ptrs[0] ? 64*1024*1024 : 0);
    }
}

void TensorMemoryManager::ReleaseAll() {
    s_tensors.clear();
    s_vram_used = 0;
    s_aperture_used = 0;
}

void TensorMemoryManager::GetStats(size_t& vram_used, size_t& aperture_used, size_t& total_used) {
    vram_used = s_vram_used;
    aperture_used = s_aperture_used;
    total_used = s_vram_used + s_aperture_used;
}

// ============================================================================
// DUAL-STAGE EXECUTOR
// ============================================================================

size_t DualStageExecutor::s_vram_budget = 14ULL * 1024 * 1024 * 1024;
size_t DualStageExecutor::s_vram_used = 0;

bool DualStageExecutor::Initialize() {
    s_vram_budget = 14ULL * 1024 * 1024 * 1024;
    s_vram_used = 0;
    return true;
}

void DualStageExecutor::Execute(const DualStageNode& node) {
    switch (node.stage) {
        case ExecutionStage::VRAM_COMPUTE:
            // Execute kernel directly in VRAM
            if (node.kernel) node.kernel();
            break;
            
        case ExecutionStage::APERTURE_STREAM:
            // Prefetch then execute
            RawrPrefetchMemory(GetPointer(node.tensor_ptr), node.size);
            if (node.kernel) node.kernel();
            break;
            
        case ExecutionStage::PREFETCH_HINT:
            // Just prefetch, no execution
            RawrPrefetchMemory(GetPointer(node.tensor_ptr), node.size);
            break;
    }
}

void DualStageExecutor::ExecuteBatch(const std::vector<DualStageNode>& nodes) {
    for (const auto& node : nodes) {
        Execute(node);
    }
}

void DualStageExecutor::SetVRAMBudget(size_t bytes) {
    s_vram_budget = bytes;
}

// ============================================================================
// SOVEREIGN BRIDGE (high-level API)
// ============================================================================

SovereignBridge::SovereignBridge(size_t size_gb) {
    pool_size = size_gb * 1024ULL * 1024 * 1024;
    
    if (PrivilegeManager::EnableLockMemoryPrivilege()) {
        ddr5_pool = RawrAllocateHugePages(pool_size);
        large_pages_active = (ddr5_pool != nullptr);
    }
    
    if (!ddr5_pool) {
        ddr5_pool = VirtualAlloc(NULL, pool_size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    }
    
    if (ddr5_pool) {
        RawrPinMemory(ddr5_pool, pool_size);
        RawrSetThreadAffinityToNUMA0();
    }
}

SovereignBridge::~SovereignBridge() {
    if (ddr5_pool) {
        RawrUnpinMemory(ddr5_pool, pool_size);
        VirtualFree(ddr5_pool, 0, MEM_RELEASE);
    }
}

void* SovereignBridge::ActivateAperture(void* weight_ptr, size_t tensor_size) {
    RawrPrefetchMemory(weight_ptr, tensor_size);
    RawrFlushCacheLines(weight_ptr, tensor_size);

    // For large tensors, trigger a higher-aggression prefetch pass.
    if (tensor_size >= (64ULL * 1024 * 1024)) {
        RawrStreamingPrefetch(weight_ptr, tensor_size, 3);
        RawrMemoryBarrier();
    }

    return weight_ptr;
}

float SovereignBridge::GetUtilization() const {
    if (pool_size == 0) return 0.0f;
    return static_cast<float>(used_size) / static_cast<float>(pool_size);
}

uint32_t SovereignBridge::GetOverflowTier() const {
    float util = GetUtilization();
    uint32_t bits;
    memcpy(&bits, &util, sizeof(bits));
    return RawrCheckOverflowTier(bits);
}

} // namespace rawr
