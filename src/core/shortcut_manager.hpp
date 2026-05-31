#pragma once
// ============================================================================
// Shortcut Manager — Customizable Keybinding System (Phase 33 Quick-Win Port)
// Pure Win32 — replaces Qt shortcut_manager with native ACCEL tables
// Features:
//   - JSON-based keybinding configuration
//   - Context-aware shortcuts (Editor, Terminal, Global, Agent)
//   - Conflict detection
//   - Export/Import keybindings
//   - Reset to defaults
// ============================================================================

#ifndef RAWRXD_SHORTCUT_MANAGER_HPP
#define RAWRXD_SHORTCUT_MANAGER_HPP

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <cstdint>

// ============================================================================
// Shortcut Context
// ============================================================================
enum class ShortcutContext : int {
    Global   = 0,  // Always active
    Editor   = 1,  // Only when editor is focused
    Terminal = 2,  // Only when terminal is focused
    Agent    = 3,  // Only when agent panel is focused
    Sidebar  = 4,  // Only when sidebar is focused
    Dialog   = 5   // Only in dialogs
};

// ============================================================================
// Shortcut Modifier Flags
// ============================================================================
enum ShortcutModifiers : uint16_t {
    MOD_NONE_KEY  = 0x0000,
    MOD_CTRL_KEY  = 0x0001,
    MOD_SHIFT_KEY = 0x0002,
    MOD_ALT_KEY   = 0x0004,
    MOD_WIN_KEY   = 0x0008
};

// ============================================================================
// Shortcut Binding
// ============================================================================
struct ShortcutBinding {
    int commandId;               // IDM_* command ID
    std::string commandName;     // Human-readable name
    uint16_t modifiers;          // ShortcutModifiers bitmask
    uint16_t keyCode;            // Virtual key code (VK_*)
    ShortcutContext context;     // When this shortcut is active
    bool isDefault;              // Whether this is the default binding
    bool enabled;                // Whether this shortcut is currently active

    // Get display string like "Ctrl+Shift+V"
    std::string toDisplayString() const;

    // Parse from display string
    static ShortcutBinding fromDisplayString(const std::string& str, int cmdId,
                                              const std::string& cmdName,
                                              ShortcutContext ctx = ShortcutContext::Global);
};

// ============================================================================
// Shortcut Conflict
// ============================================================================
struct ShortcutConflict {
    ShortcutBinding existing;
    ShortcutBinding proposed;
    std::string description;
};

// ============================================================================
// Shortcut Manager
// ============================================================================
class ShortcutManager {
public:
    ShortcutManager();
    ~ShortcutManager();

    // ---- Registration ----
    void registerDefault(int commandId, const std::string& name,
                          uint16_t modifiers, uint16_t keyCode,
                          ShortcutContext context = ShortcutContext::Global);

    // ---- Customization ----
    bool rebind(int commandId, uint16_t modifiers, uint16_t keyCode);
    bool unbind(int commandId);
    void resetToDefaults();
    void resetToDefault(int commandId);

    // ---- Lookup ----
    int findCommand(uint16_t modifiers, uint16_t keyCode, ShortcutContext context) const;
    const ShortcutBinding* getBinding(int commandId) const;
    std::vector<ShortcutBinding> getAllBindings() const;
    std::vector<ShortcutBinding> getBindingsForContext(ShortcutContext context) const;

    // ---- Conflict detection ----
    std::vector<ShortcutConflict> detectConflicts() const;
    bool hasConflict(uint16_t modifiers, uint16_t keyCode, ShortcutContext context,
                     int excludeCommandId = -1) const;

    // ---- Persistence (JSON) ----
    bool saveToFile(const std::string& path) const;
    bool loadFromFile(const std::string& path);
    std::string exportJSON() const;
    bool importJSON(const std::string& json);

    // ---- Win32 Accelerator Table ----
#ifdef _WIN32
    HACCEL buildAcceleratorTable() const;
    void destroyAcceleratorTable(HACCEL hAccel);
#endif

    // ---- Stats ----
    size_t getBindingCount() const;
    size_t getCustomizedCount() const;

private:
    std::vector<ShortcutBinding> m_bindings;
    std::vector<ShortcutBinding> m_defaults;
    mutable std::mutex m_mutex;

    // Accelerator table cache
#ifdef _WIN32
    HACCEL m_hAccel = nullptr;
#endif
    bool m_accelDirty = true;
};

#endif // RAWRXD_SHORTCUT_MANAGER_HPP
