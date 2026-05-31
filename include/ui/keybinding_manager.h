#pragma once
/**
 * @file keybinding_manager.h
 * @brief Keyboard shortcut management and conflict resolution
 * Batch 4 - Item 47: Keybinding manager
 */

#include <string>
#include <vector>
#include <map>
#include <optional>
#include <functional>
#include <windows.h>

namespace RawrXD::UI {

struct KeyCombination {
    bool ctrl;
    bool alt;
    bool shift;
    bool win;
    uint32_t keyCode;

    bool operator==(const KeyCombination& other) const {
        return ctrl == other.ctrl && alt == other.alt &&
               shift == other.shift && win == other.win &&
               keyCode == other.keyCode;
    }

    bool operator<(const KeyCombination& other) const {
        if (ctrl != other.ctrl) return ctrl < other.ctrl;
        if (alt != other.alt) return alt < other.alt;
        if (shift != other.shift) return shift < other.shift;
        if (win != other.win) return win < other.win;
        return keyCode < other.keyCode;
    }

    std::string toString() const;
    static std::optional<KeyCombination> fromString(const std::string& str);
};

struct Keybinding {
    std::string command;
    KeyCombination key;
    std::string when; // Context condition
    int priority;
    bool isDefault;
    bool isUserDefined;
};

struct KeybindingConflict {
    Keybinding existing;
    Keybinding incoming;
    std::string description;
};

class KeybindingManager {
public:
    KeybindingManager();
    ~KeybindingManager();

    // Initialization
    bool initialize();
    void shutdown();

    // Registration
    void registerCommand(const std::string& commandId,
                         const std::string& description,
                         std::function<void()> handler);
    void registerKeybinding(const Keybinding& binding);
    void unregisterKeybinding(const std::string& commandId);

    // Default keybindings
    void registerDefaultKeybindings();
    void registerEditorKeybindings();
    void registerNavigationKeybindings();
    void registerEditingKeybindings();

    // Lookup
    std::optional<Keybinding> getBindingForKey(const KeyCombination& key) const;
    std::optional<Keybinding> getBindingForCommand(const std::string& commandId) const;
    std::vector<Keybinding> getAllBindings() const;
    std::vector<Keybinding> getBindingsForCommand(const std::string& commandId) const;

    // Execution
    bool executeCommand(const std::string& commandId);
    bool handleKeyPress(const KeyCombination& key);
    bool handleWindowsMessage(UINT msg, WPARAM wParam, LPARAM lParam);

    // Conflict resolution
    std::vector<KeybindingConflict> detectConflicts(const Keybinding& newBinding) const;
    void resolveConflict(const KeybindingConflict& conflict, bool keepExisting);

    // Customization
    void setUserKeybinding(const std::string& commandId, const KeyCombination& key);
    void resetKeybinding(const std::string& commandId);
    void resetAllKeybindings();

    // Persistence
    bool saveKeybindings(const std::string& path);
    bool loadKeybindings(const std::string& path);
    std::string exportToJson() const;
    bool importFromJson(const std::string& json);

    // Context
    void setContext(const std::string& context, bool active);
    bool isContextActive(const std::string& context) const;
    void clearContext();

    // Chords
    void startChord(const std::string& chordId);
    void endChord();
    bool isInChord() const;
    std::string getCurrentChord() const;

    // Search
    std::vector<Keybinding> searchBindings(const std::string& query) const;

private:
    std::map<KeyCombination, Keybinding> m_bindings;
    std::map<std::string, std::function<void()>> m_handlers;
    std::map<std::string, std::string> m_descriptions;
    std::map<std::string, bool> m_contexts;
    std::vector<Keybinding> m_userBindings;
    std::string m_currentChord;
    mutable std::mutex m_mutex;

    bool matchesContext(const std::string& when) const;
    void registerDefaultBinding(const std::string& command,
                                 bool ctrl, bool alt, bool shift,
                                 uint32_t keyCode,
                                 const std::string& when = "");
};

// Global instance
KeybindingManager& getKeybindingManager();

// Utility functions
std::string keyCodeToString(uint32_t keyCode);
uint32_t stringToKeyCode(const std::string& str);
std::string virtualKeyToString(uint32_t vk);

} // namespace RawrXD::UI
