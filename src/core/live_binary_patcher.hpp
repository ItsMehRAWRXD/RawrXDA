// live_binary_patcher.hpp — Real-Time Binary Update Engine for RawrXD-Shell
// Enables live code replacement without IDE restart via function trampolines,
// RVA relocation, code-page swaps, and atomic instruction patching.
//
// Integrates with the UnifiedHotpatchManager as "Layer 5: Live Binary".
// Uses VirtualProtect/VirtualAlloc for executable page management.
//
// Pattern: PatchResult (no exceptions)
// Threading: std::mutex + std::atomic (no recursive locks)
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
#pragma once

#include "model_memory_hotpatch.hpp"
#include <cstdint>
#include <cstddef>
#include <mutex>
#include <atomic>
#include <vector>

// =============================================================================
// Trampoline — 14-byte x64 absolute jump used to redirect function calls
// =============================================================================
//   mov rax, <target_addr>     ; 48 B8 <8-byte addr>
//   jmp rax                    ; FF E0
// Total: 10 + 2 = 12 bytes (padded to 16 for alignment)
// =============================================================================
static constexpr size_t TRAMPOLINE_SIZE = 16;

// =============================================================================
// FunctionSlot — Tracks a single hotpatchable function
// =============================================================================
struct FunctionSlot {
    char                name[128];          // Human-readable function name
    uintptr_t           original_addr;      // Original function entry point
    uintptr_t           current_addr;       // Current active implementation
    uintptr_t           trampoline_addr;    // Address of the trampoline in exec pool
    uint8_t             original_bytes[TRAMPOLINE_SIZE]; // Saved prologue bytes
    uint64_t            patch_count;        // Number of times this slot was patched
    uint64_t            call_count;         // Hit counter (if instrumented)
    uint32_t            slot_id;            // Unique slot identifier
    bool                is_active;          // Whether the trampoline is installed
    bool                is_instrumented;    // Whether call counting is enabled
    uint8_t             _pad[6];            // Alignment

    static FunctionSlot make(const char* fname, uintptr_t addr, uint32_t id) {
        FunctionSlot s{};
        if (fname) {
            size_t len = 0;
            while (fname[len] && len < 127) { s.name[len] = fname[len]; ++len; }
            s.name[len] = '\0';
        }
        s.original_addr = addr;
        s.current_addr  = addr;
        s.slot_id       = id;
        return s;
    }
};

// =============================================================================
// CodePage — A VirtualAlloc'd executable page for trampoline/thunk storage
// =============================================================================
struct CodePage {
    void*       base_addr;      // VirtualAlloc'd RWX page
    size_t      page_size;      // Typically 4096 or 65536
    size_t      used_bytes;     // Current usage watermark
    uint32_t    page_id;        // Unique page identifier
    bool        is_sealed;      // If true, no more allocations (frozen for stability)

    // Allocate bytes from this page. Returns offset or SIZE_MAX on failure.
    size_t allocate(size_t bytes, size_t alignment = 16) {
        size_t aligned = (used_bytes + alignment - 1) & ~(alignment - 1);
        if (aligned + bytes > page_size || is_sealed) return SIZE_MAX;
        size_t offset = aligned;
        used_bytes = aligned + bytes;
        return offset;
    }

    uintptr_t ptr_at(size_t offset) const {
        return reinterpret_cast<uintptr_t>(base_addr) + offset;
    }
};

// =============================================================================
// RVARelocation — Describes a relative-address fixup needed after code swap
// =============================================================================
struct RVARelocation {
    uintptr_t   patch_site;     // Address of the instruction to fix up
    uintptr_t   target_symbol;  // Absolute address the site should reference
    int32_t     addend;         // Additional offset (for LEA-style relocations)
    uint8_t     reloc_type;     // 0 = REL32 (4-byte), 1 = ABS64 (8-byte)
    uint8_t     _pad[7];
};

// =============================================================================
// LivePatchUnit — A complete binary patch: new code + relocations
// =============================================================================
struct LivePatchUnit {
    char                    name[128];
    std::vector<uint8_t>    code;           // New machine code
    std::vector<RVARelocation> relocations; // Fixups to apply after copy
    uintptr_t               target_slot;    // FunctionSlot ID to patch
    uint32_t                version;        // Monotonic version counter
    bool                    is_applied;
};

// =============================================================================
// LiveBinaryPatcherStats — Atomic performance counters
// =============================================================================
struct LiveBinaryPatcherStats {
    std::atomic<uint64_t> trampolines_installed{0};
    std::atomic<uint64_t> trampolines_reverted{0};
    std::atomic<uint64_t> code_pages_allocated{0};
    std::atomic<uint64_t> code_bytes_written{0};
    std::atomic<uint64_t> relocations_applied{0};
    std::atomic<uint64_t> hotswaps_completed{0};
    std::atomic<uint64_t> hotswaps_failed{0};
    std::atomic<uint64_t> rollbacks_performed{0};
};

// =============================================================================
// LiveBinaryPatcher — Real-Time Binary Update Engine
// =============================================================================
class LiveBinaryPatcher {
public:
    static LiveBinaryPatcher& instance();

    // ---- Lifecycle ----
    PatchResult initialize(size_t initial_pool_pages = 4);
    PatchResult shutdown();

    // ---- Function Registration ----
    // Register a function for hotpatching. Must be called before any swap.
    // Returns the slot ID in *outSlotId.
    PatchResult register_function(const char* name, uintptr_t address,
                                  uint32_t* outSlotId);

    // Unregister and revert a function to its original code.
    PatchResult unregister_function(uint32_t slotId);

    // ---- Trampoline Management ----
    // Install a trampoline at the function entry point, redirecting to the
    // trampoline thunk in our executable page pool.
    PatchResult install_trampoline(uint32_t slotId);

    // Revert the trampoline, restoring original prologue bytes.
    PatchResult revert_trampoline(uint32_t slotId);

    // ---- Live Code Swap ----
    // Atomically replace the implementation behind a trampoline.
    // The new code is copied into the executable page pool, relocations are
    // applied, and the trampoline target is updated via interlocked write.
    PatchResult swap_implementation(uint32_t slotId,
                                    const uint8_t* newCode, size_t codeSize,
                                    const RVARelocation* relocs, size_t relocCount);

    // Apply a complete LivePatchUnit (code + relocations + metadata).
    PatchResult apply_patch_unit(const LivePatchUnit& unit);

    // Revert the last swap for a slot (one level of undo).
    PatchResult revert_last_swap(uint32_t slotId);

    // ---- Batch Operations ----
    // Atomically apply multiple patch units. Either all succeed or all are
    // rolled back (transactional semantics).
    PatchResult apply_batch(const LivePatchUnit* units, size_t count);

    // ---- RVA Relocation Engine ----
    // Apply relocations to a code region after it has been copied.
    PatchResult apply_relocations(uintptr_t code_base,
                                  const RVARelocation* relocs, size_t count);

    // ---- Code Page Pool ----
    // Allocate a new executable page for trampoline/thunk storage.
    PatchResult allocate_code_page(uint32_t* outPageId);

    // Seal a code page (no more allocations, improves W^X compliance).
    PatchResult seal_code_page(uint32_t pageId);

    // ---- Query ----
    const FunctionSlot* get_slot(uint32_t slotId) const;
    size_t get_slot_count() const;
    const LiveBinaryPatcherStats& get_stats() const;

    // ---- Module Loading ----
    // Load a DLL at runtime and register all exported functions as hotpatchable.
    PatchResult load_module(const char* dll_path, uint32_t* outModuleId);

    // Unload a previously loaded module and revert all its trampolines.
    PatchResult unload_module(uint32_t moduleId);

    // ---- Integrity ----
    // Verify that all installed trampolines are intact (not tampered with).
    PatchResult verify_integrity();

private:
    LiveBinaryPatcher();
    ~LiveBinaryPatcher();
    LiveBinaryPatcher(const LiveBinaryPatcher&) = delete;
    LiveBinaryPatcher& operator=(const LiveBinaryPatcher&) = delete;

    // Write a 14-byte absolute jump at `site` targeting `destination`.
    // Handles VirtualProtect internally.
    PatchResult write_absolute_jump(uintptr_t site, uintptr_t destination);

    // Allocate `size` bytes from the code page pool with execute permission.
    uintptr_t alloc_exec(size_t size, size_t alignment = 16);

    // Apply a single RVA relocation.
    PatchResult apply_one_reloc(const RVARelocation& reloc, uintptr_t code_base);

    std::mutex                          m_mutex;
    std::vector<FunctionSlot>           m_slots;
    std::vector<CodePage>               m_code_pages;
    std::atomic<uint32_t>               m_next_slot_id{0};
    std::atomic<uint32_t>               m_next_page_id{0};
    LiveBinaryPatcherStats              m_stats;
    bool                                m_initialized = false;

    // Previous implementation addresses for one-level undo
    struct UndoEntry {
        uint32_t    slotId;
        uintptr_t   prev_addr;
    };
    std::vector<UndoEntry>              m_undo_stack;

    // Loaded modules for runtime DLL management
    struct LoadedModule {
        uint32_t                moduleId;
        void*                   hModule;          // HMODULE
        char                    path[260];
        std::vector<uint32_t>   slot_ids;         // Registered function slots
    };
    std::vector<LoadedModule>           m_modules;
    std::atomic<uint32_t>               m_next_module_id{0};
};
