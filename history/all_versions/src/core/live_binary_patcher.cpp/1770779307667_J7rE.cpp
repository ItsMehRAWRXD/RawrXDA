// live_binary_patcher.cpp — Real-Time Binary Update Engine Implementation
// Manages function trampolines, code-page pools, RVA relocations, and
// atomic instruction patching for live binary updates without IDE restart.
//
// Uses: VirtualProtect, VirtualAlloc, FlushInstructionCache
// Pattern: PatchResult (no exceptions)
// Threading: std::lock_guard<std::mutex> (no recursive locks)
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
#include "live_binary_patcher.hpp"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstring>
#include <cstdio>

// =============================================================================
// Singleton
// =============================================================================
LiveBinaryPatcher& LiveBinaryPatcher::instance() {
    static LiveBinaryPatcher inst;
    return inst;
}

LiveBinaryPatcher::LiveBinaryPatcher()  = default;
LiveBinaryPatcher::~LiveBinaryPatcher() { shutdown(); }

// =============================================================================
// Lifecycle
// =============================================================================
PatchResult LiveBinaryPatcher::initialize(size_t initial_pool_pages) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_initialized) {
        return PatchResult::ok("Already initialized");
    }

    // Pre-allocate executable code pages
    for (size_t i = 0; i < initial_pool_pages; ++i) {
        CodePage page{};
        page.page_size = 65536; // 64KB per page
        page.base_addr = VirtualAlloc(nullptr, page.page_size,
                                       MEM_COMMIT | MEM_RESERVE,
                                       PAGE_EXECUTE_READWRITE);
        if (!page.base_addr) {
            return PatchResult::error("VirtualAlloc failed for code page pool",
                                       static_cast<int>(GetLastError()));
        }
        // Fill with INT3 (0xCC) for safety — any wild jump hits a breakpoint
        std::memset(page.base_addr, 0xCC, page.page_size);
        page.used_bytes = 0;
        page.page_id = m_next_page_id.fetch_add(1, std::memory_order_relaxed);
        page.is_sealed = false;

        m_code_pages.push_back(page);
        m_stats.code_pages_allocated.fetch_add(1, std::memory_order_relaxed);
    }

    m_initialized = true;
    return PatchResult::ok("LiveBinaryPatcher initialized");
}

PatchResult LiveBinaryPatcher::shutdown() {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_initialized) {
        return PatchResult::ok("Not initialized");
    }

    // Revert all active trampolines first
    for (auto& slot : m_slots) {
        if (slot.is_active) {
            // Restore original prologue bytes
            DWORD oldProt = 0;
            if (VirtualProtect(reinterpret_cast<void*>(slot.original_addr),
                               TRAMPOLINE_SIZE, PAGE_EXECUTE_READWRITE, &oldProt)) {
                std::memcpy(reinterpret_cast<void*>(slot.original_addr),
                            slot.original_bytes, TRAMPOLINE_SIZE);
                FlushInstructionCache(GetCurrentProcess(),
                                      reinterpret_cast<void*>(slot.original_addr),
                                      TRAMPOLINE_SIZE);
                VirtualProtect(reinterpret_cast<void*>(slot.original_addr),
                               TRAMPOLINE_SIZE, oldProt, &oldProt);
            }
            slot.is_active = false;
        }
    }

    // Unload all modules
    for (auto& mod : m_modules) {
        if (mod.hModule) {
            FreeLibrary(static_cast<HMODULE>(mod.hModule));
            mod.hModule = nullptr;
        }
    }
    m_modules.clear();

    // Free all code pages
    for (auto& page : m_code_pages) {
        if (page.base_addr) {
            VirtualFree(page.base_addr, 0, MEM_RELEASE);
            page.base_addr = nullptr;
        }
    }
    m_code_pages.clear();

    m_slots.clear();
    m_undo_stack.clear();
    m_initialized = false;

    return PatchResult::ok("LiveBinaryPatcher shut down");
}

// =============================================================================
// Function Registration
// =============================================================================
PatchResult LiveBinaryPatcher::register_function(const char* name, uintptr_t address,
                                                  uint32_t* outSlotId) {
    if (!name || !address || !outSlotId) {
        return PatchResult::error("Null parameter in register_function", 1);
    }

    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_initialized) {
        return PatchResult::error("LiveBinaryPatcher not initialized", 2);
    }

    // Check for duplicate name
    for (const auto& s : m_slots) {
        if (std::strcmp(s.name, name) == 0) {
            return PatchResult::error("Function already registered", 3);
        }
    }

    uint32_t id = m_next_slot_id.fetch_add(1, std::memory_order_relaxed);
    FunctionSlot slot = FunctionSlot::make(name, address, id);

    // Save original prologue bytes for later restoration
    std::memcpy(slot.original_bytes, reinterpret_cast<const void*>(address),
                TRAMPOLINE_SIZE);

    m_slots.push_back(slot);
    *outSlotId = id;

    return PatchResult::ok("Function registered");
}

PatchResult LiveBinaryPatcher::unregister_function(uint32_t slotId) {
    std::lock_guard<std::mutex> lock(m_mutex);

    for (auto it = m_slots.begin(); it != m_slots.end(); ++it) {
        if (it->slot_id == slotId) {
            if (it->is_active) {
                // Revert trampoline first
                DWORD oldProt = 0;
                VirtualProtect(reinterpret_cast<void*>(it->original_addr),
                               TRAMPOLINE_SIZE, PAGE_EXECUTE_READWRITE, &oldProt);
                std::memcpy(reinterpret_cast<void*>(it->original_addr),
                            it->original_bytes, TRAMPOLINE_SIZE);
                FlushInstructionCache(GetCurrentProcess(),
                                      reinterpret_cast<void*>(it->original_addr),
                                      TRAMPOLINE_SIZE);
                VirtualProtect(reinterpret_cast<void*>(it->original_addr),
                               TRAMPOLINE_SIZE, oldProt, &oldProt);
                m_stats.trampolines_reverted.fetch_add(1, std::memory_order_relaxed);
            }
            m_slots.erase(it);
            return PatchResult::ok("Function unregistered");
        }
    }
    return PatchResult::error("Slot not found", 4);
}

// =============================================================================
// Trampoline Management
// =============================================================================
PatchResult LiveBinaryPatcher::install_trampoline(uint32_t slotId) {
    std::lock_guard<std::mutex> lock(m_mutex);

    FunctionSlot* slot = nullptr;
    for (auto& s : m_slots) {
        if (s.slot_id == slotId) { slot = &s; break; }
    }
    if (!slot) return PatchResult::error("Slot not found", 4);
    if (slot->is_active) return PatchResult::ok("Trampoline already installed");

    // Allocate trampoline thunk in executable pool
    // The thunk is: mov rax, <current_addr>; jmp rax + optional call counter
    uintptr_t thunk_addr = alloc_exec(TRAMPOLINE_SIZE * 2); // Extra space for instrumentation
    if (!thunk_addr) {
        return PatchResult::error("Failed to allocate trampoline thunk", 5);
    }
    slot->trampoline_addr = thunk_addr;

    // Write initial thunk: jump to original function
    uint8_t thunk[16];
    thunk[0] = 0x48; thunk[1] = 0xB8;  // mov rax, imm64
    std::memcpy(thunk + 2, &slot->current_addr, 8);
    thunk[10] = 0xFF; thunk[11] = 0xE0; // jmp rax
    // NOP sled for alignment
    thunk[12] = 0x90; thunk[13] = 0x90;
    thunk[14] = 0x90; thunk[15] = 0x90;
    std::memcpy(reinterpret_cast<void*>(thunk_addr), thunk, 16);
    FlushInstructionCache(GetCurrentProcess(),
                          reinterpret_cast<void*>(thunk_addr), 16);

    // Now install the trampoline at the original function entry point
    PatchResult r = write_absolute_jump(slot->original_addr, thunk_addr);
    if (!r.success) return r;

    slot->is_active = true;
    m_stats.trampolines_installed.fetch_add(1, std::memory_order_relaxed);
    return PatchResult::ok("Trampoline installed");
}

PatchResult LiveBinaryPatcher::revert_trampoline(uint32_t slotId) {
    std::lock_guard<std::mutex> lock(m_mutex);

    FunctionSlot* slot = nullptr;
    for (auto& s : m_slots) {
        if (s.slot_id == slotId) { slot = &s; break; }
    }
    if (!slot) return PatchResult::error("Slot not found", 4);
    if (!slot->is_active) return PatchResult::ok("Trampoline not active");

    // Restore original prologue
    DWORD oldProt = 0;
    if (!VirtualProtect(reinterpret_cast<void*>(slot->original_addr),
                        TRAMPOLINE_SIZE, PAGE_EXECUTE_READWRITE, &oldProt)) {
        return PatchResult::error("VirtualProtect failed", static_cast<int>(GetLastError()));
    }

    std::memcpy(reinterpret_cast<void*>(slot->original_addr),
                slot->original_bytes, TRAMPOLINE_SIZE);

    FlushInstructionCache(GetCurrentProcess(),
                          reinterpret_cast<void*>(slot->original_addr),
                          TRAMPOLINE_SIZE);

    VirtualProtect(reinterpret_cast<void*>(slot->original_addr),
                   TRAMPOLINE_SIZE, oldProt, &oldProt);

    slot->is_active = false;
    slot->current_addr = slot->original_addr;
    m_stats.trampolines_reverted.fetch_add(1, std::memory_order_relaxed);
    return PatchResult::ok("Trampoline reverted");
}

// =============================================================================
// Live Code Swap
// =============================================================================
PatchResult LiveBinaryPatcher::swap_implementation(uint32_t slotId,
                                                    const uint8_t* newCode, size_t codeSize,
                                                    const RVARelocation* relocs, size_t relocCount) {
    if (!newCode || codeSize == 0) {
        return PatchResult::error("Null or empty code", 1);
    }

    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_initialized) {
        return PatchResult::error("Not initialized", 2);
    }

    FunctionSlot* slot = nullptr;
    for (auto& s : m_slots) {
        if (s.slot_id == slotId) { slot = &s; break; }
    }
    if (!slot) return PatchResult::error("Slot not found", 4);

    // Allocate space for new code in executable pool
    uintptr_t new_code_addr = alloc_exec(codeSize, 16);
    if (!new_code_addr) {
        m_stats.hotswaps_failed.fetch_add(1, std::memory_order_relaxed);
        return PatchResult::error("Failed to allocate executable memory for new code", 5);
    }

    // Copy new code into executable memory
    std::memcpy(reinterpret_cast<void*>(new_code_addr), newCode, codeSize);
    m_stats.code_bytes_written.fetch_add(codeSize, std::memory_order_relaxed);

    // Apply relocations
    if (relocs && relocCount > 0) {
        PatchResult rr = apply_relocations(new_code_addr, relocs, relocCount);
        if (!rr.success) {
            m_stats.hotswaps_failed.fetch_add(1, std::memory_order_relaxed);
            return rr;
        }
    }

    // Flush instruction cache for the new code region
    FlushInstructionCache(GetCurrentProcess(),
                          reinterpret_cast<void*>(new_code_addr), codeSize);

    // Save undo entry
    m_undo_stack.push_back({slotId, slot->current_addr});

    // Atomically update the trampoline thunk to point to new code.
    // The thunk is at slot->trampoline_addr and has the form:
    //   48 B8 <8-byte addr>  FF E0
    // We only need to update the 8-byte address at offset 2.
    uintptr_t prev_addr = slot->current_addr;
    slot->current_addr = new_code_addr;
    slot->patch_count++;

    if (slot->is_active && slot->trampoline_addr) {
        // Atomic 8-byte aligned write to the trampoline's target address
        // On x64, aligned 8-byte writes are atomic
        volatile uint64_t* target = reinterpret_cast<volatile uint64_t*>(
            slot->trampoline_addr + 2);
        *target = static_cast<uint64_t>(new_code_addr);

        FlushInstructionCache(GetCurrentProcess(),
                              reinterpret_cast<void*>(slot->trampoline_addr),
                              TRAMPOLINE_SIZE);
    }

    m_stats.hotswaps_completed.fetch_add(1, std::memory_order_relaxed);
    return PatchResult::ok("Implementation swapped");
}

PatchResult LiveBinaryPatcher::apply_patch_unit(const LivePatchUnit& unit) {
    return swap_implementation(
        static_cast<uint32_t>(unit.target_slot),
        unit.code.data(), unit.code.size(),
        unit.relocations.data(), unit.relocations.size());
}

PatchResult LiveBinaryPatcher::revert_last_swap(uint32_t slotId) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Find the most recent undo entry for this slot
    for (auto it = m_undo_stack.rbegin(); it != m_undo_stack.rend(); ++it) {
        if (it->slotId == slotId) {
            FunctionSlot* slot = nullptr;
            for (auto& s : m_slots) {
                if (s.slot_id == slotId) { slot = &s; break; }
            }
            if (!slot) return PatchResult::error("Slot not found", 4);

            uintptr_t prev = it->prev_addr;
            slot->current_addr = prev;

            // Update trampoline
            if (slot->is_active && slot->trampoline_addr) {
                volatile uint64_t* target = reinterpret_cast<volatile uint64_t*>(
                    slot->trampoline_addr + 2);
                *target = static_cast<uint64_t>(prev);
                FlushInstructionCache(GetCurrentProcess(),
                                      reinterpret_cast<void*>(slot->trampoline_addr),
                                      TRAMPOLINE_SIZE);
            }

            // Remove the undo entry (convert reverse_iterator to forward)
            m_undo_stack.erase(std::next(it).base());
            m_stats.rollbacks_performed.fetch_add(1, std::memory_order_relaxed);
            return PatchResult::ok("Reverted to previous implementation");
        }
    }

    return PatchResult::error("No undo entry found for slot", 6);
}

// =============================================================================
// Batch Operations (Transactional)
// =============================================================================
PatchResult LiveBinaryPatcher::apply_batch(const LivePatchUnit* units, size_t count) {
    if (!units || count == 0) {
        return PatchResult::error("Null or empty batch", 1);
    }

    // Record undo stack size for rollback
    size_t undo_checkpoint = m_undo_stack.size();

    for (size_t i = 0; i < count; ++i) {
        PatchResult r = apply_patch_unit(units[i]);
        if (!r.success) {
            // Rollback all patches applied in this batch
            while (m_undo_stack.size() > undo_checkpoint) {
                auto& entry = m_undo_stack.back();
                // Find slot and revert
                for (auto& s : m_slots) {
                    if (s.slot_id == entry.slotId) {
                        s.current_addr = entry.prev_addr;
                        if (s.is_active && s.trampoline_addr) {
                            volatile uint64_t* target = reinterpret_cast<volatile uint64_t*>(
                                s.trampoline_addr + 2);
                            *target = static_cast<uint64_t>(entry.prev_addr);
                            FlushInstructionCache(GetCurrentProcess(),
                                                  reinterpret_cast<void*>(s.trampoline_addr),
                                                  TRAMPOLINE_SIZE);
                        }
                        break;
                    }
                }
                m_undo_stack.pop_back();
            }
            m_stats.rollbacks_performed.fetch_add(1, std::memory_order_relaxed);
            return PatchResult::error("Batch failed — all patches rolled back", 7);
        }
    }

    return PatchResult::ok("Batch applied successfully");
}

// =============================================================================
// RVA Relocation Engine
// =============================================================================
PatchResult LiveBinaryPatcher::apply_relocations(uintptr_t code_base,
                                                  const RVARelocation* relocs, size_t count) {
    for (size_t i = 0; i < count; ++i) {
        PatchResult r = apply_one_reloc(relocs[i], code_base);
        if (!r.success) return r;
    }
    return PatchResult::ok("All relocations applied");
}

PatchResult LiveBinaryPatcher::apply_one_reloc(const RVARelocation& reloc, uintptr_t code_base) {
    uintptr_t site = code_base + reloc.patch_site;

    if (reloc.reloc_type == 0) {
        // REL32: 4-byte PC-relative fixup
        // value = target - (site + 4) + addend
        int64_t delta = static_cast<int64_t>(reloc.target_symbol) -
                        static_cast<int64_t>(site + 4) +
                        reloc.addend;
        if (delta > INT32_MAX || delta < INT32_MIN) {
            return PatchResult::error("REL32 overflow — target too far", 8);
        }

        DWORD oldProt = 0;
        if (!VirtualProtect(reinterpret_cast<void*>(site), 4,
                            PAGE_EXECUTE_READWRITE, &oldProt)) {
            return PatchResult::error("VirtualProtect failed for REL32",
                                       static_cast<int>(GetLastError()));
        }
        *reinterpret_cast<int32_t*>(site) = static_cast<int32_t>(delta);
        VirtualProtect(reinterpret_cast<void*>(site), 4, oldProt, &oldProt);
    }
    else if (reloc.reloc_type == 1) {
        // ABS64: 8-byte absolute address
        DWORD oldProt = 0;
        if (!VirtualProtect(reinterpret_cast<void*>(site), 8,
                            PAGE_EXECUTE_READWRITE, &oldProt)) {
            return PatchResult::error("VirtualProtect failed for ABS64",
                                       static_cast<int>(GetLastError()));
        }
        *reinterpret_cast<uint64_t*>(site) = static_cast<uint64_t>(
            reloc.target_symbol + reloc.addend);
        VirtualProtect(reinterpret_cast<void*>(site), 8, oldProt, &oldProt);
    }
    else {
        return PatchResult::error("Unknown relocation type", 9);
    }

    m_stats.relocations_applied.fetch_add(1, std::memory_order_relaxed);
    return PatchResult::ok("Relocation applied");
}

// =============================================================================
// Code Page Pool
// =============================================================================
PatchResult LiveBinaryPatcher::allocate_code_page(uint32_t* outPageId) {
    if (!outPageId) return PatchResult::error("Null outPageId", 1);

    std::lock_guard<std::mutex> lock(m_mutex);

    CodePage page{};
    page.page_size = 65536;
    page.base_addr = VirtualAlloc(nullptr, page.page_size,
                                   MEM_COMMIT | MEM_RESERVE,
                                   PAGE_EXECUTE_READWRITE);
    if (!page.base_addr) {
        return PatchResult::error("VirtualAlloc failed",
                                   static_cast<int>(GetLastError()));
    }
    std::memset(page.base_addr, 0xCC, page.page_size); // INT3 fill
    page.used_bytes = 0;
    page.page_id = m_next_page_id.fetch_add(1, std::memory_order_relaxed);
    page.is_sealed = false;

    m_code_pages.push_back(page);
    *outPageId = page.page_id;
    m_stats.code_pages_allocated.fetch_add(1, std::memory_order_relaxed);

    return PatchResult::ok("Code page allocated");
}

PatchResult LiveBinaryPatcher::seal_code_page(uint32_t pageId) {
    std::lock_guard<std::mutex> lock(m_mutex);

    for (auto& page : m_code_pages) {
        if (page.page_id == pageId) {
            if (page.is_sealed) return PatchResult::ok("Already sealed");

            // Transition from RWX to RX (W^X compliance)
            DWORD oldProt = 0;
            if (!VirtualProtect(page.base_addr, page.page_size,
                                PAGE_EXECUTE_READ, &oldProt)) {
                return PatchResult::error("VirtualProtect to RX failed",
                                           static_cast<int>(GetLastError()));
            }
            page.is_sealed = true;
            return PatchResult::ok("Code page sealed (RX only)");
        }
    }
    return PatchResult::error("Page not found", 10);
}

// =============================================================================
// Query
// =============================================================================
const FunctionSlot* LiveBinaryPatcher::get_slot(uint32_t slotId) const {
    for (const auto& s : m_slots) {
        if (s.slot_id == slotId) return &s;
    }
    return nullptr;
}

size_t LiveBinaryPatcher::get_slot_count() const {
    return m_slots.size();
}

const LiveBinaryPatcherStats& LiveBinaryPatcher::get_stats() const {
    return m_stats;
}

// =============================================================================
// Module Loading
// =============================================================================
PatchResult LiveBinaryPatcher::load_module(const char* dll_path, uint32_t* outModuleId) {
    if (!dll_path || !outModuleId) {
        return PatchResult::error("Null parameter", 1);
    }

    std::lock_guard<std::mutex> lock(m_mutex);

    HMODULE hMod = LoadLibraryA(dll_path);
    if (!hMod) {
        return PatchResult::error("LoadLibraryA failed",
                                   static_cast<int>(GetLastError()));
    }

    LoadedModule mod{};
    mod.moduleId = m_next_module_id.fetch_add(1, std::memory_order_relaxed);
    mod.hModule = static_cast<void*>(hMod);
    std::strncpy(mod.path, dll_path, sizeof(mod.path) - 1);
    mod.path[sizeof(mod.path) - 1] = '\0';

    m_modules.push_back(mod);
    *outModuleId = mod.moduleId;

    return PatchResult::ok("Module loaded");
}

PatchResult LiveBinaryPatcher::unload_module(uint32_t moduleId) {
    std::lock_guard<std::mutex> lock(m_mutex);

    for (auto it = m_modules.begin(); it != m_modules.end(); ++it) {
        if (it->moduleId == moduleId) {
            // Revert all trampolines for this module's slots
            for (uint32_t sid : it->slot_ids) {
                for (auto& s : m_slots) {
                    if (s.slot_id == sid && s.is_active) {
                        DWORD oldProt = 0;
                        VirtualProtect(reinterpret_cast<void*>(s.original_addr),
                                       TRAMPOLINE_SIZE, PAGE_EXECUTE_READWRITE, &oldProt);
                        std::memcpy(reinterpret_cast<void*>(s.original_addr),
                                    s.original_bytes, TRAMPOLINE_SIZE);
                        FlushInstructionCache(GetCurrentProcess(),
                                              reinterpret_cast<void*>(s.original_addr),
                                              TRAMPOLINE_SIZE);
                        VirtualProtect(reinterpret_cast<void*>(s.original_addr),
                                       TRAMPOLINE_SIZE, oldProt, &oldProt);
                        s.is_active = false;
                    }
                }
            }

            if (it->hModule) {
                FreeLibrary(static_cast<HMODULE>(it->hModule));
            }
            m_modules.erase(it);
            return PatchResult::ok("Module unloaded");
        }
    }
    return PatchResult::error("Module not found", 11);
}

// =============================================================================
// Integrity Verification
// =============================================================================
PatchResult LiveBinaryPatcher::verify_integrity() {
    std::lock_guard<std::mutex> lock(m_mutex);

    uint32_t corrupted = 0;
    for (const auto& slot : m_slots) {
        if (!slot.is_active) continue;
        if (!slot.trampoline_addr) { ++corrupted; continue; }

        // Verify the trampoline thunk is intact:
        // Byte 0-1 should be 48 B8 (mov rax, imm64)
        // Byte 10-11 should be FF E0 (jmp rax)
        const uint8_t* thunk = reinterpret_cast<const uint8_t*>(slot.trampoline_addr);
        if (thunk[0] != 0x48 || thunk[1] != 0xB8 ||
            thunk[10] != 0xFF || thunk[11] != 0xE0) {
            ++corrupted;
            continue;
        }

        // Verify the embedded address matches slot->current_addr
        uint64_t embedded_addr = 0;
        std::memcpy(&embedded_addr, thunk + 2, 8);
        if (embedded_addr != static_cast<uint64_t>(slot.current_addr)) {
            ++corrupted;
        }
    }

    if (corrupted > 0) {
        char msg[128];
        std::snprintf(msg, sizeof(msg), "%u trampoline(s) corrupted", corrupted);
        return PatchResult::error(msg, static_cast<int>(corrupted));
    }
    return PatchResult::ok("All trampolines intact");
}

// =============================================================================
// Private Helpers
// =============================================================================
PatchResult LiveBinaryPatcher::write_absolute_jump(uintptr_t site, uintptr_t destination) {
    DWORD oldProt = 0;
    if (!VirtualProtect(reinterpret_cast<void*>(site), TRAMPOLINE_SIZE,
                        PAGE_EXECUTE_READWRITE, &oldProt)) {
        return PatchResult::error("VirtualProtect failed for trampoline write",
                                   static_cast<int>(GetLastError()));
    }

    uint8_t jump[16];
    jump[0] = 0x48; jump[1] = 0xB8;       // mov rax, imm64
    std::memcpy(jump + 2, &destination, 8);
    jump[10] = 0xFF; jump[11] = 0xE0;     // jmp rax
    jump[12] = 0x90; jump[13] = 0x90;     // NOP padding
    jump[14] = 0x90; jump[15] = 0x90;

    std::memcpy(reinterpret_cast<void*>(site), jump, TRAMPOLINE_SIZE);

    FlushInstructionCache(GetCurrentProcess(),
                          reinterpret_cast<void*>(site), TRAMPOLINE_SIZE);

    DWORD dummy = 0;
    VirtualProtect(reinterpret_cast<void*>(site), TRAMPOLINE_SIZE, oldProt, &dummy);

    return PatchResult::ok("Absolute jump written");
}

uintptr_t LiveBinaryPatcher::alloc_exec(size_t size, size_t alignment) {
    // Try existing pages first
    for (auto& page : m_code_pages) {
        size_t off = page.allocate(size, alignment);
        if (off != SIZE_MAX) {
            return page.ptr_at(off);
        }
    }

    // Allocate a new page
    CodePage page{};
    page.page_size = (size > 65536) ? ((size + 4095) & ~4095) : 65536;
    page.base_addr = VirtualAlloc(nullptr, page.page_size,
                                   MEM_COMMIT | MEM_RESERVE,
                                   PAGE_EXECUTE_READWRITE);
    if (!page.base_addr) return 0;

    std::memset(page.base_addr, 0xCC, page.page_size);
    page.used_bytes = 0;
    page.page_id = m_next_page_id.fetch_add(1, std::memory_order_relaxed);
    page.is_sealed = false;

    size_t off = page.allocate(size, alignment);
    if (off == SIZE_MAX) return 0; // Should not happen on a fresh page

    uintptr_t result = page.ptr_at(off);
    m_code_pages.push_back(page);
    m_stats.code_pages_allocated.fetch_add(1, std::memory_order_relaxed);

    return result;
}
