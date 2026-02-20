// ============================================================================
// command_id_blueprint.h — Future-Proof Command ID Allocation Blueprint
// ============================================================================
//
// Architecture: C++20, Win32, no Qt, no exceptions
// Purpose:     Canonical ID namespace for all RawrXD subsystems.
//              Compile-time static_asserts lock IDs against drift.
//              Runtime validator catches plugin collisions at init.
//
// Design:      Each subsystem gets a 100-ID block within its zone.
//              Plugin extensions receive dynamically allocated IDs
//              from the 60000-64999 range (plugin namespace).
//
// CRITICAL:    DO NOT change locked IDs without updating all three:
//              1. This header (the blueprint)
//              2. feature_registration.cpp (_R suffixed defines)
//              3. Win32IDE_Commands.cpp (routeCommand ranges)
//
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#pragma once

#ifndef RAWRXD_COMMAND_ID_BLUEPRINT_H
#define RAWRXD_COMMAND_ID_BLUEPRINT_H

#include <cstdint>
#include <cstddef>
#include <atomic>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <string>
#include <vector>

// ============================================================================
// ZONE DEFINITIONS — Master allocation table
// ============================================================================
//
// ┌──────────────────────────────────────────────────────────────────────────┐
// │ Zone        │ Range         │ Sub-Blocks         │ Status               │
// ├─────────────┼───────────────┼────────────────────┼──────────────────────┤
// │ FILE OPS    │ 1000 – 1099   │ 1001-1035 active   │ LOCKED               │
// │ FILE (rsv)  │ 1100 – 1999   │ Future file ops    │ RESERVED             │
// │ EDIT        │ 2000 – 2099   │ 2001-2017 active   │ LOCKED               │
// │ EDIT (rsv)  │ 2100 – 2999   │ Future edit ops    │ RESERVED             │
// │ VIEW        │ 3000 – 3019   │ View panels        │ AVAILABLE            │
// │ GIT         │ 3020 – 3099   │ 3020-3024 active   │ LOCKED (partial)     │
// │ THEMES      │ 3100 – 3199   │ 3100-3116 active   │ LOCKED (partial)     │
// │ VIEW (rsv)  │ 3200 – 3999   │ Future view/SCM    │ RESERVED             │
// │ TERMINAL    │ 4000 – 4099   │ 4001-4010 active   │ LOCKED               │
// │ AGENT       │ 4100 – 4149   │ 4100-4121 active   │ LOCKED               │
// │ AUTONOMY    │ 4150 – 4199   │ 4150-4155 active   │ LOCKED               │
// │ AI MODE     │ 4200 – 4299   │ 4200-4202 active   │ LOCKED               │
// │ RE (RevEng) │ 4300 – 4399   │ 4300-4311 active   │ LOCKED               │
// │ DEBUG       │ 5000 – 5099   │ (see 5157-5184)    │ AVAILABLE            │
// │ DEBUG (Win) │ 5100 – 5199   │ 5157-5184 active   │ LOCKED               │
// │ SWARM       │ 5200 – 5299   │ 5132-5156 active   │ LOCKED (legacy pos)  │
// │ BACKEND/LLM │ 5300 – 5399   │ 5037-5081 active   │ LOCKED (legacy pos)  │
// │ TOOLS       │ 5400 – 5999   │ Future tools       │ RESERVED             │
// │ MODULES     │ 6000 – 6999   │ Future modules     │ RESERVED             │
// │ HELP        │ 7000 – 7099   │ 7001-7003 active   │ LOCKED               │
// │ HELP (rsv)  │ 7100 – 7999   │ Future help/docs   │ RESERVED             │
// │ GIT (alt)   │ 8000 – 8999   │ Alt-routing zone   │ DEPRECATED           │
// │ HOTPATCH    │ 9000 – 9099   │ 9001-9017 active   │ LOCKED               │
// │ ─DEAD ZONE─ │ 9100 – 9199   │ Monaco collision   │ FORBIDDEN            │
// │ EDITOR ENG  │ 9300 – 9399   │ 9300-9304 active   │ LOCKED               │
// │ ─DEAD ZONE─ │ 9400 – 9499   │ PDB collision      │ FORBIDDEN            │
// │ VOICE       │ 9700 – 9799   │ 9700-9707 active   │ LOCKED               │
// │ SYSTEM (rsv)│ 9800 – 9999   │ System internals   │ RESERVED             │
// ├─────────────┼───────────────┼────────────────────┼──────────────────────┤
// │ ─DEAD ZONE─ │ 10000 – 49999 │ SC_* / Windows     │ FORBIDDEN            │
// ├─────────────┼───────────────┼────────────────────┼──────────────────────┤
// │ PLUGIN BASE │ 60000 – 60999 │ Plugin slot 0      │ DYNAMIC              │
// │ PLUGIN 1    │ 61000 – 61999 │ Plugin slot 1      │ DYNAMIC              │
// │ PLUGIN 2    │ 62000 – 62999 │ Plugin slot 2      │ DYNAMIC              │
// │ PLUGIN 3    │ 63000 – 63999 │ Plugin slot 3      │ DYNAMIC              │
// │ PLUGIN 4    │ 64000 – 64999 │ Plugin slot 4      │ DYNAMIC              │
// │ PLUGIN (rsv)│ 65000 – 65534 │ Plugin overflow    │ DYNAMIC OVERFLOW     │
// └──────────────────────────────────────────────────────────────────────────┘
//
// Windows internals note:
//   SC_CLOSE = 0xF060 (61536), SC_MINIMIZE = 0xF020 (61472), etc.
//   These live in the 61000+ range when issued as WM_SYSCOMMAND.
//   WM_COMMAND only carries 16-bit wParam IDs (0-65535).
//   Plugin IDs must avoid the SC_* range (0xF000-0xF1FF = 61440-61951).
//   Plugin slot 1 (61000-61999) partially overlaps SC_* — use slot 0
//   or slots 2-4 for safety, or mask SC_* in WndProc before dispatch.

// ============================================================================
// ZONE BOUNDARY CONSTANTS
// ============================================================================

namespace CommandZone {

    // ── Core IDE Zones (hardcoded, locked) ──
    constexpr uint32_t FILE_MIN          = 1000;
    constexpr uint32_t FILE_MAX          = 1099;
    constexpr uint32_t FILE_RESERVED_MAX = 1999;

    constexpr uint32_t EDIT_MIN          = 2000;
    constexpr uint32_t EDIT_MAX          = 2099;
    constexpr uint32_t EDIT_RESERVED_MAX = 2999;

    constexpr uint32_t VIEW_MIN          = 3000;
    constexpr uint32_t VIEW_MAX          = 3019;
    constexpr uint32_t GIT_MIN           = 3020;
    constexpr uint32_t GIT_MAX           = 3099;
    constexpr uint32_t THEME_MIN         = 3100;
    constexpr uint32_t THEME_MAX         = 3199;
    constexpr uint32_t VIEW_RESERVED_MAX = 3999;

    constexpr uint32_t TERMINAL_MIN      = 4000;
    constexpr uint32_t TERMINAL_MAX      = 4099;

    constexpr uint32_t AGENT_MIN         = 4100;
    constexpr uint32_t AGENT_MAX         = 4149;
    constexpr uint32_t AUTONOMY_MIN      = 4150;
    constexpr uint32_t AUTONOMY_MAX      = 4199;

    constexpr uint32_t AI_MODE_MIN       = 4200;
    constexpr uint32_t AI_MODE_MAX       = 4299;
    constexpr uint32_t RE_MIN            = 4300;
    constexpr uint32_t RE_MAX            = 4399;

    // ── Tools & Debug zones ──
    constexpr uint32_t DEBUG_MIN         = 5100;
    constexpr uint32_t DEBUG_MAX         = 5199;
    constexpr uint32_t SWARM_MIN         = 5132;  // Legacy position
    constexpr uint32_t SWARM_MAX         = 5156;
    constexpr uint32_t BACKEND_MIN       = 5037;  // Legacy position
    constexpr uint32_t BACKEND_MAX       = 5081;
    constexpr uint32_t TOOLS_RESERVED    = 5999;

    constexpr uint32_t MODULES_MIN       = 6000;
    constexpr uint32_t MODULES_MAX       = 6999;

    constexpr uint32_t HELP_MIN          = 7000;
    constexpr uint32_t HELP_MAX          = 7099;

    constexpr uint32_t GIT_ALT_MIN       = 8000;  // Deprecated
    constexpr uint32_t GIT_ALT_MAX       = 8999;

    // ── System zones ──
    constexpr uint32_t HOTPATCH_MIN      = 9000;
    constexpr uint32_t HOTPATCH_MAX      = 9099;
    constexpr uint32_t EDITOR_ENG_MIN    = 9300;
    constexpr uint32_t EDITOR_ENG_MAX    = 9399;
    constexpr uint32_t VOICE_MIN         = 9700;
    constexpr uint32_t VOICE_MAX         = 9799;
    constexpr uint32_t SYSTEM_MIN        = 9800;
    constexpr uint32_t SYSTEM_MAX        = 9999;

    // ── Dead Zones (Windows / Monaco / PDB collisions) ──
    constexpr uint32_t DEAD_MONACO_MIN   = 9100;
    constexpr uint32_t DEAD_MONACO_MAX   = 9199;
    constexpr uint32_t DEAD_PDB_MIN      = 9400;
    constexpr uint32_t DEAD_PDB_MAX      = 9499;
    constexpr uint32_t DEAD_WIN32_MIN    = 10000;
    constexpr uint32_t DEAD_WIN32_MAX    = 49999;
    constexpr uint32_t DEAD_SYSCMD_MIN   = 0xF000;  // 61440 — SC_* range
    constexpr uint32_t DEAD_SYSCMD_MAX   = 0xF1FF;  // 61951

    // ── Plugin Dynamic Zones ──
    constexpr uint32_t PLUGIN_BASE       = 60000;
    constexpr uint32_t PLUGIN_SLOT_SIZE  = 1000;
    constexpr uint32_t PLUGIN_MAX_SLOTS  = 5;   // Slots 0..4
    constexpr uint32_t PLUGIN_OVERFLOW   = 65000;
    constexpr uint32_t PLUGIN_HARD_MAX   = 65534;  // WM_COMMAND limit

    // ── Utility: Zone membership check ──
    constexpr bool isDeadZone(uint32_t id) {
        if (id >= DEAD_MONACO_MIN && id <= DEAD_MONACO_MAX) return true;
        if (id >= DEAD_PDB_MIN    && id <= DEAD_PDB_MAX)    return true;
        if (id >= DEAD_WIN32_MIN  && id <= DEAD_WIN32_MAX)  return true;
        if (id >= DEAD_SYSCMD_MIN && id <= DEAD_SYSCMD_MAX) return true;
        return false;
    }

    constexpr bool isPluginZone(uint32_t id) {
        return id >= PLUGIN_BASE && id <= PLUGIN_HARD_MAX;
    }

    constexpr bool isCoreZone(uint32_t id) {
        return id >= FILE_MIN && id <= SYSTEM_MAX && !isDeadZone(id);
    }

    constexpr const char* zoneNameFor(uint32_t id) {
        if (id >= FILE_MIN      && id <= FILE_RESERVED_MAX)  return "File";
        if (id >= EDIT_MIN      && id <= EDIT_RESERVED_MAX)  return "Edit";
        if (id >= GIT_MIN       && id <= GIT_MAX)            return "Git";
        if (id >= THEME_MIN     && id <= THEME_MAX)          return "Themes";
        if (id >= VIEW_MIN      && id <= VIEW_RESERVED_MAX)  return "View";
        if (id >= TERMINAL_MIN  && id <= TERMINAL_MAX)       return "Terminal";
        if (id >= AGENT_MIN     && id <= AGENT_MAX)          return "Agent";
        if (id >= AUTONOMY_MIN  && id <= AUTONOMY_MAX)       return "Autonomy";
        if (id >= AI_MODE_MIN   && id <= AI_MODE_MAX)        return "AI Mode";
        if (id >= RE_MIN        && id <= RE_MAX)             return "Reverse Engineering";
        if (id >= DEBUG_MIN     && id <= DEBUG_MAX)          return "Debug";
        if (id >= HOTPATCH_MIN  && id <= HOTPATCH_MAX)       return "Hotpatch";
        if (id >= EDITOR_ENG_MIN&& id <= EDITOR_ENG_MAX)     return "Editor Engine";
        if (id >= VOICE_MIN     && id <= VOICE_MAX)          return "Voice";
        if (id >= HELP_MIN      && id <= HELP_MAX)           return "Help";
        if (id >= MODULES_MIN   && id <= MODULES_MAX)        return "Modules";
        if (isDeadZone(id))                                   return "DEAD ZONE";
        if (isPluginZone(id))                                 return "Plugin";
        return "Unknown";
    }

} // namespace CommandZone

// ============================================================================
// STATIC ASSERTIONS — Lock canonical IDs at compile time
// ============================================================================
// If ANY of these fire, someone changed an ID without updating all three
// sources (this header, feature_registration.cpp, Win32IDE_Commands.cpp).

// ── File Ops ──
static_assert(1001 >= CommandZone::FILE_MIN && 1001 <= CommandZone::FILE_MAX,
    "IDM_FILE_NEW must be in File zone 1000-1099");
static_assert(1035 >= CommandZone::FILE_MIN && 1035 <= CommandZone::FILE_MAX,
    "IDM_FILE_QUICK_LOAD must be in File zone 1000-1099");

// ── Edit ──
static_assert(2016 >= CommandZone::EDIT_MIN && 2016 <= CommandZone::EDIT_MAX,
    "IDM_EDIT_FIND must be in Edit zone 2000-2099");
static_assert(2017 >= CommandZone::EDIT_MIN && 2017 <= CommandZone::EDIT_MAX,
    "IDM_EDIT_REPLACE must be in Edit zone 2000-2099");

// ── Git ──
static_assert(3020 >= CommandZone::GIT_MIN && 3020 <= CommandZone::GIT_MAX,
    "IDM_GIT_STATUS must be in Git zone 3020-3099");
static_assert(3024 >= CommandZone::GIT_MIN && 3024 <= CommandZone::GIT_MAX,
    "IDM_GIT_DIFF must be in Git zone 3020-3099");

// ── Terminal ──
static_assert(4001 >= CommandZone::TERMINAL_MIN && 4001 <= CommandZone::TERMINAL_MAX,
    "IDM_TERMINAL_NEW must be in Terminal zone 4000-4099");

// ── Agent ──
static_assert(4100 >= CommandZone::AGENT_MIN && 4100 <= CommandZone::AGENT_MAX,
    "IDM_AGENT_START_LOOP must be in Agent zone 4100-4149");
static_assert(4121 >= CommandZone::AGENT_MIN && 4121 <= CommandZone::AGENT_MAX,
    "IDM_AGENT_GOAL must be in Agent zone 4100-4149");

// ── Autonomy ──
static_assert(4150 >= CommandZone::AUTONOMY_MIN && 4150 <= CommandZone::AUTONOMY_MAX,
    "IDM_AUTONOMY_TOGGLE must be in Autonomy zone 4150-4199");

// ── AI Mode ──
static_assert(4200 >= CommandZone::AI_MODE_MIN && 4200 <= CommandZone::AI_MODE_MAX,
    "IDM_AI_MAX_MODE must be in AI Mode zone 4200-4299");
static_assert(4202 >= CommandZone::AI_MODE_MIN && 4202 <= CommandZone::AI_MODE_MAX,
    "IDM_AI_DEEP_RESEARCH must be in AI Mode zone 4200-4299");

// ── Reverse Engineering ──
static_assert(4300 >= CommandZone::RE_MIN && 4300 <= CommandZone::RE_MAX,
    "IDM_RE_DECISION_TREE must be in RE zone 4300-4399");
static_assert(4311 >= CommandZone::RE_MIN && 4311 <= CommandZone::RE_MAX,
    "IDM_RE_SSA_LIFT must be in RE zone 4300-4399");

// ── Debug ──
static_assert(5157 >= CommandZone::DEBUG_MIN && 5157 <= CommandZone::DEBUG_MAX,
    "IDM_DBG_LAUNCH must be in Debug zone 5100-5199");
static_assert(5184 >= CommandZone::DEBUG_MIN && 5184 <= CommandZone::DEBUG_MAX,
    "IDM_DBG max must be in Debug zone 5100-5199");

// ── Hotpatch ──
static_assert(9001 >= CommandZone::HOTPATCH_MIN && 9001 <= CommandZone::HOTPATCH_MAX,
    "IDM_HOTPATCH_STATUS must be in Hotpatch zone 9000-9099");

// ── Voice ──
static_assert(9700 >= CommandZone::VOICE_MIN && 9700 <= CommandZone::VOICE_MAX,
    "IDM_VOICE_RECORD must be in Voice zone 9700-9799");
static_assert(9707 >= CommandZone::VOICE_MIN && 9707 <= CommandZone::VOICE_MAX,
    "IDM_VOICE_MODE_PTT must be in Voice zone 9700-9799");

// ── Editor Engine ──
static_assert(9300 >= CommandZone::EDITOR_ENG_MIN && 9300 <= CommandZone::EDITOR_ENG_MAX,
    "IDM_EDITOR_ENGINE_RICHEDIT must be in Editor Engine zone 9300-9399");
static_assert(9304 >= CommandZone::EDITOR_ENG_MIN && 9304 <= CommandZone::EDITOR_ENG_MAX,
    "IDM_EDITOR_ENGINE_STATUS must be in Editor Engine zone 9300-9399");

// ── Help ──
static_assert(7001 >= CommandZone::HELP_MIN && 7001 <= CommandZone::HELP_MAX,
    "IDM_HELP_ABOUT must be in Help zone 7000-7099");

// ── Dead Zone Guards ──
static_assert(CommandZone::isDeadZone(9150), "Monaco dead zone must include 9150");
static_assert(CommandZone::isDeadZone(9450), "PDB dead zone must include 9450");
static_assert(!CommandZone::isDeadZone(9001), "Hotpatch zone must not be dead");
static_assert(!CommandZone::isDeadZone(9700), "Voice zone must not be dead");

// ============================================================================
// RUNTIME COLLISION DETECTOR — validateCommandIds()
// ============================================================================
// Integrated into UnifiedHotpatchManager init sequence.
// Crashes immediately (fail-fast) if duplicate IDs detected.

struct CommandIdCollision {
    uint32_t    id;
    const char* featureA;
    const char* featureB;
    const char* zone;
};

struct CommandIdValidation {
    bool                            valid;
    size_t                          totalChecked;
    size_t                          collisionCount;
    size_t                          deadZoneViolations;
    std::vector<CommandIdCollision> collisions;
    std::vector<uint32_t>           deadZoneIds;  // IDs found in forbidden zones
    
    static CommandIdValidation ok(size_t checked) {
        CommandIdValidation v;
        v.valid = true;
        v.totalChecked = checked;
        v.collisionCount = 0;
        v.deadZoneViolations = 0;
        return v;
    }
};

// Forward declaration — implemented in command_id_validator.cpp
CommandIdValidation validateCommandIds();

// Call this at startup (e.g. from WinMain or UnifiedHotpatchManager::instance())
// Calls validateCommandIds() and crashes if collisions found.
void enforceCommandIdIntegrity();

// ============================================================================
// PLUGIN NAMESPACE ALLOCATOR — Dynamic ID assignment for extensions
// ============================================================================
//
// Plugins request an ID block at load time. The allocator assigns them
// a contiguous block from the plugin zone (60000-65534).
// Each plugin gets its own slot (1000 IDs per slot, up to 5 slots).
// Overflow goes to 65000-65534 with per-ID allocation.
//
// Thread-safe: all allocation/deallocation is mutex-protected.
// Deterministic: slot assignment is FIFO (first plugin gets slot 0).
//
// Usage:
//   uint32_t base = PluginIdAllocator::instance().allocateSlot("MyPlugin");
//   // base = 60000 → your IDs are 60000-60999
//   // Register commands using (base + localOffset) as commandId
//
// Deallocation:
//   PluginIdAllocator::instance().releaseSlot("MyPlugin");

class PluginIdAllocator {
public:
    static PluginIdAllocator& instance() {
        static PluginIdAllocator s_instance;
        return s_instance;
    }

    struct PluginSlot {
        char        pluginName[128];
        uint32_t    baseId;
        uint32_t    maxId;
        bool        active;
        size_t      allocatedCount;  // How many IDs the plugin has actually used
    };

    // Allocate a 1000-ID slot for a plugin. Returns base ID, or 0 on failure.
    uint32_t allocateSlot(const char* pluginName) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        // Check if already allocated
        for (size_t i = 0; i < CommandZone::PLUGIN_MAX_SLOTS; ++i) {
            if (m_slots[i].active && strcmp(m_slots[i].pluginName, pluginName) == 0) {
                return m_slots[i].baseId;  // Already has a slot
            }
        }
        
        // Find first free slot
        for (size_t i = 0; i < CommandZone::PLUGIN_MAX_SLOTS; ++i) {
            if (!m_slots[i].active) {
                strncpy_s(m_slots[i].pluginName, pluginName, 127);
                m_slots[i].baseId = CommandZone::PLUGIN_BASE + 
                                    static_cast<uint32_t>(i) * CommandZone::PLUGIN_SLOT_SIZE;
                m_slots[i].maxId  = m_slots[i].baseId + CommandZone::PLUGIN_SLOT_SIZE - 1;
                m_slots[i].active = true;
                m_slots[i].allocatedCount = 0;
                m_activeSlots.fetch_add(1, std::memory_order_relaxed);
                return m_slots[i].baseId;
            }
        }
        
        // All slots full — try overflow range
        if (m_overflowNext < CommandZone::PLUGIN_HARD_MAX) {
            uint32_t base = m_overflowNext;
            m_overflowNext += 100;  // Overflow plugins get 100 IDs
            // Store in overflow map
            m_overflowAllocations[pluginName] = base;
            return base;
        }
        
        return 0;  // No space
    }

    // Release a plugin's slot
    bool releaseSlot(const char* pluginName) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        for (size_t i = 0; i < CommandZone::PLUGIN_MAX_SLOTS; ++i) {
            if (m_slots[i].active && strcmp(m_slots[i].pluginName, pluginName) == 0) {
                m_slots[i].active = false;
                m_slots[i].pluginName[0] = '\0';
                m_slots[i].allocatedCount = 0;
                m_activeSlots.fetch_sub(1, std::memory_order_relaxed);
                return true;
            }
        }
        
        // Check overflow
        auto it = m_overflowAllocations.find(pluginName);
        if (it != m_overflowAllocations.end()) {
            m_overflowAllocations.erase(it);
            return true;
        }
        
        return false;
    }

    // Register a specific ID within a plugin's allocated slot
    bool registerPluginCommandId(const char* pluginName, uint32_t localOffset,
                                  const char* commandName) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        for (size_t i = 0; i < CommandZone::PLUGIN_MAX_SLOTS; ++i) {
            if (m_slots[i].active && strcmp(m_slots[i].pluginName, pluginName) == 0) {
                uint32_t globalId = m_slots[i].baseId + localOffset;
                if (globalId > m_slots[i].maxId) return false;  // Overflow
                
                // Check for collision within plugin
                if (m_usedIds.count(globalId)) return false;
                m_usedIds.insert(globalId);
                m_slots[i].allocatedCount++;
                return true;
            }
        }
        return false;
    }

    // Query
    const PluginSlot* getSlot(const char* pluginName) const {
        for (size_t i = 0; i < CommandZone::PLUGIN_MAX_SLOTS; ++i) {
            if (m_slots[i].active && strcmp(m_slots[i].pluginName, pluginName) == 0) {
                return &m_slots[i];
            }
        }
        return nullptr;
    }

    size_t activeSlotCount() const {
        return m_activeSlots.load(std::memory_order_relaxed);
    }

    bool isPluginId(uint32_t id) const {
        return CommandZone::isPluginZone(id);
    }

    // Validate: no plugin IDs overlap with core or dead zones
    bool validatePluginIds() const {
        for (uint32_t id : m_usedIds) {
            if (CommandZone::isCoreZone(id))  return false;
            if (CommandZone::isDeadZone(id))  return false;
        }
        return true;
    }

    // Generate allocation report
    std::string generateReport() const {
        std::string report;
        report += "=== Plugin ID Allocation Report ===\n";
        report += "Active slots: " + std::to_string(m_activeSlots.load()) + "/" +
                  std::to_string(CommandZone::PLUGIN_MAX_SLOTS) + "\n\n";
        
        for (size_t i = 0; i < CommandZone::PLUGIN_MAX_SLOTS; ++i) {
            if (m_slots[i].active) {
                report += "  Slot " + std::to_string(i) + ": " + m_slots[i].pluginName + "\n";
                report += "    Range: " + std::to_string(m_slots[i].baseId) + "-" +
                          std::to_string(m_slots[i].maxId) + "\n";
                report += "    Used:  " + std::to_string(m_slots[i].allocatedCount) + "/" +
                          std::to_string(CommandZone::PLUGIN_SLOT_SIZE) + "\n\n";
            }
        }
        
        if (!m_overflowAllocations.empty()) {
            report += "  Overflow allocations:\n";
            for (const auto& [name, base] : m_overflowAllocations) {
                report += "    " + name + " → " + std::to_string(base) + "\n";
            }
        }
        
        return report;
    }

private:
    PluginIdAllocator() {
        for (size_t i = 0; i < CommandZone::PLUGIN_MAX_SLOTS; ++i) {
            m_slots[i] = {};
        }
        m_overflowNext = CommandZone::PLUGIN_OVERFLOW;
    }
    
    PluginIdAllocator(const PluginIdAllocator&) = delete;
    PluginIdAllocator& operator=(const PluginIdAllocator&) = delete;

    PluginSlot                                  m_slots[CommandZone::PLUGIN_MAX_SLOTS];
    std::unordered_set<uint32_t>                m_usedIds;
    std::unordered_map<std::string, uint32_t>   m_overflowAllocations;
    uint32_t                                    m_overflowNext;
    std::mutex                                  m_mutex;
    std::atomic<size_t>                         m_activeSlots{0};
};

#endif // RAWRXD_COMMAND_ID_BLUEPRINT_H
