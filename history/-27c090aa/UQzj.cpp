#include "RawrXD_UNIFIED_ENGINE.h"
#include <iostream>
#include <algorithm>
#include <fstream>

namespace rawrxd {

// ============================================================================
// SLOT LATTICE IMPLEMENTATION
// ============================================================================

SlotLattice::SlotLattice() {
    // Allocate fixed ranges once (Fixed 2.5GB Budget)
    auto create_slots = [&](SlotType type, size_t total_bytes, int count) {
        size_t per_slot = total_bytes / count;
        for (int i = 0; i < count; ++i) {
            Slot s;
            s.base = VirtualAlloc(NULL, per_slot, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
            s.capacity = (uint32_t)per_slot;
            s.type = type;
            s.active = 0;
            slots_.push_back(s);
        }
    };

    create_slots(SlotType::ATTN, MemoryBudget::ATTN, 8);
    create_slots(SlotType::MLP, MemoryBudget::MLP, 8);
    create_slots(SlotType::KV, MemoryBudget::KV, 16);
    create_slots(SlotType::MISC, MemoryBudget::MISC, 4);
}

SlotLattice::~SlotLattice() {
    for (auto& s : slots_) {
        if (s.base) VirtualFree(s.base, 0, MEM_RELEASE);
    }
}

Slot* SlotLattice::acquire(SlotType type, uint32_t bytes) {
    for (auto& s : slots_) {
        if (s.type == type && s.active == 0 && s.capacity >= bytes) {
            s.active = bytes;
            return &s;
        }
    }
    return nullptr; // Slot overflow -> triggers recovery in real impl
}

void SlotLattice::release(Slot* slot) {
    if (slot) slot->active = 0;
}

// ============================================================================
// UNIFIED LOADER IMPLEMENTATION (Super Source)
// ============================================================================

UnifiedLoader::UnifiedLoader() : lattice_(std::make_unique<SlotLattice>()) {}

UnifiedLoader::~UnifiedLoader() {
    if (file_handle_ != INVALID_HANDLE_VALUE) CloseHandle(file_handle_);
}

bool UnifiedLoader::loadModel(const std::string& path) {
    model_path_ = path;
    file_handle_ = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED | FILE_FLAG_NO_BUFFERING, NULL);
    
    if (file_handle_ == INVALID_HANDLE_VALUE) return false;

    return buildPlan();
}

bool UnifiedLoader::buildPlan() {
    // 1. Identify Format (GGUF magic check)
    // 2. Map Tensors into Steps
    // 3. Ensure PI-partition constraints are met
    
    // Placeholder: In a real run, this would parse the actual GGUF/Ollama header
    StreamStep step1;
    step1.step_id = 0;
    step1.total_bytes = 100 * 1024 * 1024; // 100MB example
    plan_.push_back(step1);
    
    return true;
}

bool UnifiedLoader::executeNext() {
    if (current_step_ >= plan_.size()) return false;

    const auto& step = plan_[current_step_];
    for (const auto& zone : step.zones) {
        asyncLoad(zone);
    }
    
    current_step_++;
    return true;
}

void UnifiedLoader::asyncLoad(const TensorDesc& zone) {
    // Determine target slot type
    SlotType st = SlotType::MISC;
    if (zone.role <= TensorRole::ATTN_O) st = SlotType::ATTN;
    else if (zone.role <= TensorRole::MLP_DOWN) st = SlotType::MLP;
    else if (zone.role == TensorRole::KV_CACHE) st = SlotType::KV;

    Slot* slot = lattice_->acquire(st, zone.byte_length);
    if (!slot) return;

    OVERLAPPED* ov = new OVERLAPPED();
    memset(ov, 0, sizeof(OVERLAPPED));
    ov->Offset = (DWORD)(zone.file_offset & 0xFFFFFFFF);
    ov->OffsetHigh = (DWORD)(zone.file_offset >> 32);

    // High-speed direct I/O
    ReadFile(file_handle_, slot->base, zone.byte_length, NULL, ov);
}

// ============================================================================
// AGENTIC WIN32 INTEGRATION
// ============================================================================

void AgenticTools::InjectPayload(DWORD pid, const std::string& dll) {
    HANDLE h = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if (!h) return;

    void* mem = VirtualAllocEx(h, NULL, dll.size() + 1, MEM_COMMIT, PAGE_READWRITE);
    WriteProcessMemory(h, mem, dll.c_str(), dll.size() + 1, NULL);
    CreateRemoteThread(h, NULL, 0, (LPTHREAD_START_ROUTINE)GetProcAddress(GetModuleHandleA("kernel32"), "LoadLibraryA"), mem, 0, NULL);
    CloseHandle(h);
}

} // namespace rawrxd
