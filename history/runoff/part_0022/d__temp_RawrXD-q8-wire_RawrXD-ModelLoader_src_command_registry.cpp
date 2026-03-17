/**
 * @file command_registry.cpp
 * @brief Production command registry implementation
 * 
 * Provides unified command handling for CLI and GUI with real implementations
 */

#include "command_registry.h"
#include <algorithm>
#include <cctype>
#include <sstream>
#include <fstream>

namespace RawrXD::Commands {

// ============================================================================
// KEYBINDING IMPLEMENTATION
// ============================================================================

std::string KeyBinding::toString() const {
    if (key == 0) return "";
    
    std::string result;
    
    if (modifiers & FCONTROL) result += "Ctrl+";
    if (modifiers & FALT) result += "Alt+";
    if (modifiers & FSHIFT) result += "Shift+";
    
    // Convert virtual key to string
    switch (key) {
        case VK_F1: result += "F1"; break;
        case VK_F2: result += "F2"; break;
        case VK_F3: result += "F3"; break;
        case VK_F4: result += "F4"; break;
        case VK_F5: result += "F5"; break;
        case VK_F6: result += "F6"; break;
        case VK_F7: result += "F7"; break;
        case VK_F8: result += "F8"; break;
        case VK_F9: result += "F9"; break;
        case VK_F10: result += "F10"; break;
        case VK_F11: result += "F11"; break;
        case VK_F12: result += "F12"; break;
        case VK_ESCAPE: result += "Escape"; break;
        case VK_TAB: result += "Tab"; break;
        case VK_RETURN: result += "Enter"; break;
        case VK_SPACE: result += "Space"; break;
        case VK_BACK: result += "Backspace"; break;
        case VK_DELETE: result += "Delete"; break;
        case VK_INSERT: result += "Insert"; break;
        case VK_HOME: result += "Home"; break;
        case VK_END: result += "End"; break;
        case VK_PRIOR: result += "PageUp"; break;
        case VK_NEXT: result += "PageDown"; break;
        case VK_UP: result += "Up"; break;
        case VK_DOWN: result += "Down"; break;
        case VK_LEFT: result += "Left"; break;
        case VK_RIGHT: result += "Right"; break;
        case VK_OEM_3: result += "`"; break;  // Backtick
        case VK_OEM_MINUS: result += "-"; break;
        case VK_OEM_PLUS: result += "="; break;
        case VK_OEM_4: result += "["; break;
        case VK_OEM_6: result += "]"; break;
        case VK_OEM_5: result += "\\"; break;
        case VK_OEM_1: result += ";"; break;
        case VK_OEM_7: result += "'"; break;
        case VK_OEM_COMMA: result += ","; break;
        case VK_OEM_PERIOD: result += "."; break;
        case VK_OEM_2: result += "/"; break;
        default:
            if (key >= 'A' && key <= 'Z') {
                result += static_cast<char>(key);
            } else if (key >= '0' && key <= '9') {
                result += static_cast<char>(key);
            } else {
                result += "?";
            }
            break;
    }
    
    if (!chord.empty()) {
        result += " " + chord;
    }
    
    return result;
}

KeyBinding KeyBinding::fromString(const std::string& str) {
    KeyBinding kb;
    if (str.empty()) return kb;
    
    // Check for chord (space-separated)
    size_t spacePos = str.find(' ');
    std::string first = str;
    if (spacePos != std::string::npos) {
        first = str.substr(0, spacePos);
        kb.chord = str.substr(spacePos + 1);
    }
    
    // Parse modifiers and key
    std::string remaining = first;
    
    // Check for Ctrl
    if (remaining.find("Ctrl+") == 0) {
        kb.modifiers |= FCONTROL;
        remaining = remaining.substr(5);
    }
    
    // Check for Alt
    if (remaining.find("Alt+") == 0) {
        kb.modifiers |= FALT;
        remaining = remaining.substr(4);
    }
    
    // Check for Shift
    if (remaining.find("Shift+") == 0) {
        kb.modifiers |= FSHIFT;
        remaining = remaining.substr(6);
    }
    
    // Parse key
    if (remaining == "F1") kb.key = VK_F1;
    else if (remaining == "F2") kb.key = VK_F2;
    else if (remaining == "F3") kb.key = VK_F3;
    else if (remaining == "F4") kb.key = VK_F4;
    else if (remaining == "F5") kb.key = VK_F5;
    else if (remaining == "F6") kb.key = VK_F6;
    else if (remaining == "F7") kb.key = VK_F7;
    else if (remaining == "F8") kb.key = VK_F8;
    else if (remaining == "F9") kb.key = VK_F9;
    else if (remaining == "F10") kb.key = VK_F10;
    else if (remaining == "F11") kb.key = VK_F11;
    else if (remaining == "F12") kb.key = VK_F12;
    else if (remaining == "Escape" || remaining == "Esc") kb.key = VK_ESCAPE;
    else if (remaining == "Tab") kb.key = VK_TAB;
    else if (remaining == "Enter" || remaining == "Return") kb.key = VK_RETURN;
    else if (remaining == "Space") kb.key = VK_SPACE;
    else if (remaining == "Backspace") kb.key = VK_BACK;
    else if (remaining == "Delete" || remaining == "Del") kb.key = VK_DELETE;
    else if (remaining == "Insert" || remaining == "Ins") kb.key = VK_INSERT;
    else if (remaining == "Home") kb.key = VK_HOME;
    else if (remaining == "End") kb.key = VK_END;
    else if (remaining == "PageUp" || remaining == "PgUp") kb.key = VK_PRIOR;
    else if (remaining == "PageDown" || remaining == "PgDn") kb.key = VK_NEXT;
    else if (remaining == "Up") kb.key = VK_UP;
    else if (remaining == "Down") kb.key = VK_DOWN;
    else if (remaining == "Left") kb.key = VK_LEFT;
    else if (remaining == "Right") kb.key = VK_RIGHT;
    else if (remaining == "`") kb.key = VK_OEM_3;
    else if (remaining == "-") kb.key = VK_OEM_MINUS;
    else if (remaining == "=") kb.key = VK_OEM_PLUS;
    else if (remaining == "[") kb.key = VK_OEM_4;
    else if (remaining == "]") kb.key = VK_OEM_6;
    else if (remaining == "\\") kb.key = VK_OEM_5;
    else if (remaining == ";") kb.key = VK_OEM_1;
    else if (remaining == "'") kb.key = VK_OEM_7;
    else if (remaining == ",") kb.key = VK_OEM_COMMA;
    else if (remaining == ".") kb.key = VK_OEM_PERIOD;
    else if (remaining == "/") kb.key = VK_OEM_2;
    else if (remaining.length() == 1) {
        char c = std::toupper(remaining[0]);
        if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) {
            kb.key = static_cast<WORD>(c);
        }
    }
    
    return kb;
}

// ============================================================================
// COMMAND REGISTRY IMPLEMENTATION
// ============================================================================

CommandRegistry& CommandRegistry::instance() {
    static CommandRegistry s_instance;
    return s_instance;
}

void CommandRegistry::ensureInitialized() {
    if (!m_initialized) {
        InitializeCriticalSection(&m_cs);
        m_initialized = true;
    }
}

void CommandRegistry::registerCommand(const Command& cmd) {
    ensureInitialized();
    EnterCriticalSection(&m_cs);
    
    m_commands[cmd.id] = cmd;
    if (!cmd.internalId.empty()) {
        m_internalIdMap[cmd.internalId] = cmd.id;
    }
    
    LeaveCriticalSection(&m_cs);
}

void CommandRegistry::registerCommand(UINT id, const std::string& label, const std::string& shortcut,
                                       CommandHandler handler, CommandContext ctx) {
    Command cmd;
    cmd.id = id;
    cmd.label = label;
    cmd.keybinding = KeyBinding::fromString(shortcut);
    cmd.handler = handler;
    cmd.context = ctx;
    
    registerCommand(cmd);
}

void CommandRegistry::unregisterCommand(UINT id) {
    ensureInitialized();
    EnterCriticalSection(&m_cs);
    
    auto it = m_commands.find(id);
    if (it != m_commands.end()) {
        if (!it->second.internalId.empty()) {
            m_internalIdMap.erase(it->second.internalId);
        }
        m_commands.erase(it);
    }
    
    LeaveCriticalSection(&m_cs);
}

void CommandRegistry::setKeybinding(UINT cmdId, const KeyBinding& binding) {
    ensureInitialized();
    EnterCriticalSection(&m_cs);
    
    auto it = m_commands.find(cmdId);
    if (it != m_commands.end()) {
        it->second.keybinding = binding;
    }
    
    LeaveCriticalSection(&m_cs);
}

void CommandRegistry::setKeybinding(UINT cmdId, const std::string& bindingStr) {
    setKeybinding(cmdId, KeyBinding::fromString(bindingStr));
}

std::optional<KeyBinding> CommandRegistry::getKeybinding(UINT cmdId) const {
    auto it = m_commands.find(cmdId);
    if (it != m_commands.end() && it->second.keybinding.key != 0) {
        return it->second.keybinding;
    }
    return std::nullopt;
}

Command* CommandRegistry::getCommand(UINT id) {
    auto it = m_commands.find(id);
    return (it != m_commands.end()) ? &it->second : nullptr;
}

Command* CommandRegistry::getCommandByInternalId(const std::string& internalId) {
    auto idIt = m_internalIdMap.find(internalId);
    if (idIt != m_internalIdMap.end()) {
        return getCommand(idIt->second);
    }
    return nullptr;
}

std::vector<Command*> CommandRegistry::getCommandsByCategory(const std::string& category) {
    std::vector<Command*> result;
    for (auto& [id, cmd] : m_commands) {
        if (cmd.category == category) {
            result.push_back(&cmd);
        }
    }
    return result;
}

std::vector<Command*> CommandRegistry::getAllCommands() {
    std::vector<Command*> result;
    for (auto& [id, cmd] : m_commands) {
        result.push_back(&cmd);
    }
    return result;
}

bool CommandRegistry::executeCommand(UINT id) {
    ensureInitialized();
    EnterCriticalSection(&m_cs);
    
    auto it = m_commands.find(id);
    if (it == m_commands.end() || !it->second.handler) {
        LeaveCriticalSection(&m_cs);
        return false;
    }
    
    // Check if command is enabled in current context
    if (!hasContext(m_currentContext, it->second.context)) {
        LeaveCriticalSection(&m_cs);
        return false;
    }
    
    // Check custom enabled checker
    if (it->second.enabledChecker && !it->second.enabledChecker()) {
        LeaveCriticalSection(&m_cs);
        return false;
    }
    
    auto handler = it->second.handler;
    LeaveCriticalSection(&m_cs);
    
    // Execute outside lock
    handler();
    return true;
}

bool CommandRegistry::executeCommand(const std::string& internalId) {
    auto idIt = m_internalIdMap.find(internalId);
    if (idIt != m_internalIdMap.end()) {
        return executeCommand(idIt->second);
    }
    return false;
}

bool CommandRegistry::executeCommandWithArgs(UINT id, const nlohmann::json& args) {
    ensureInitialized();
    EnterCriticalSection(&m_cs);
    
    auto it = m_commands.find(id);
    if (it == m_commands.end() || !it->second.argHandler) {
        LeaveCriticalSection(&m_cs);
        return false;
    }
    
    auto handler = it->second.argHandler;
    LeaveCriticalSection(&m_cs);
    
    handler(args);
    return true;
}

void CommandRegistry::setCurrentContext(CommandContext ctx) {
    m_currentContext = ctx;
}

void CommandRegistry::addContext(CommandContext ctx) {
    m_currentContext = m_currentContext | ctx;
}

void CommandRegistry::removeContext(CommandContext ctx) {
    m_currentContext = static_cast<CommandContext>(
        static_cast<uint32_t>(m_currentContext) & ~static_cast<uint32_t>(ctx)
    );
}

CommandContext CommandRegistry::getCurrentContext() const {
    return m_currentContext;
}

bool CommandRegistry::isCommandEnabled(UINT id) const {
    auto it = m_commands.find(id);
    if (it == m_commands.end()) return false;
    
    if (!hasContext(m_currentContext, it->second.context)) {
        return false;
    }
    
    if (it->second.enabledChecker) {
        return it->second.enabledChecker();
    }
    
    return it->second.isEnabled;
}

void CommandRegistry::updateCommandState(UINT id, bool enabled) {
    auto it = m_commands.find(id);
    if (it != m_commands.end()) {
        it->second.isEnabled = enabled;
    }
}

void CommandRegistry::updateCommandState(UINT id, bool enabled, bool checked) {
    auto it = m_commands.find(id);
    if (it != m_commands.end()) {
        it->second.isEnabled = enabled;
        it->second.isChecked = checked;
    }
}

HACCEL CommandRegistry::createAcceleratorTable() {
    std::vector<ACCEL> accels;
    
    for (const auto& [id, cmd] : m_commands) {
        if (cmd.keybinding.key != 0 && !cmd.keybinding.isChord()) {
            accels.push_back(cmd.keybinding.toAccel(id));
        }
    }
    
    if (accels.empty()) {
        return nullptr;
    }
    
    return CreateAcceleratorTableA(accels.data(), static_cast<int>(accels.size()));
}

void CommandRegistry::destroyAcceleratorTable(HACCEL hAccel) {
    if (hAccel) {
        DestroyAcceleratorTable(hAccel);
    }
}

nlohmann::json CommandRegistry::exportKeybindings() const {
    nlohmann::json bindings = nlohmann::json::array();
    
    for (const auto& [id, cmd] : m_commands) {
        if (cmd.keybinding.key != 0) {
            nlohmann::json entry;
            entry["command"] = cmd.internalId.empty() ? std::to_string(id) : cmd.internalId;
            entry["key"] = cmd.keybinding.toString();
            bindings.push_back(entry);
        }
    }
    
    return bindings;
}

void CommandRegistry::importKeybindings(const nlohmann::json& bindings) {
    if (!bindings.is_array()) return;
    
    for (const auto& entry : bindings) {
        if (!entry.contains("command") || !entry.contains("key")) continue;
        
        std::string cmdStr = entry["command"].get<std::string>();
        std::string keyStr = entry["key"].get<std::string>();
        
        // Try to find command
        UINT cmdId = 0;
        try {
            cmdId = static_cast<UINT>(std::stoul(cmdStr));
        } catch (...) {
            // Try internal ID
            auto idIt = m_internalIdMap.find(cmdStr);
            if (idIt != m_internalIdMap.end()) {
                cmdId = idIt->second;
            }
        }
        
        if (cmdId != 0) {
            setKeybinding(cmdId, keyStr);
        }
    }
}

void CommandRegistry::loadKeybindingsFromFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) return;
    
    try {
        nlohmann::json bindings;
        file >> bindings;
        importKeybindings(bindings);
    } catch (...) {
        // Ignore parse errors
    }
}

void CommandRegistry::saveKeybindingsToFile(const std::string& path) const {
    std::ofstream file(path);
    if (!file.is_open()) return;
    
    file << exportKeybindings().dump(2);
}

std::vector<Command*> CommandRegistry::searchCommands(const std::string& query) {
    std::vector<Command*> results;
    std::string lowerQuery = query;
    std::transform(lowerQuery.begin(), lowerQuery.end(), lowerQuery.begin(), ::tolower);
    
    for (auto& [id, cmd] : m_commands) {
        if (!cmd.isVisible) continue;
        
        // Check label
        std::string lowerLabel = cmd.label;
        std::transform(lowerLabel.begin(), lowerLabel.end(), lowerLabel.begin(), ::tolower);
        
        if (lowerLabel.find(lowerQuery) != std::string::npos) {
            results.push_back(&cmd);
            continue;
        }
        
        // Check internal ID
        std::string lowerId = cmd.internalId;
        std::transform(lowerId.begin(), lowerId.end(), lowerId.begin(), ::tolower);
        
        if (lowerId.find(lowerQuery) != std::string::npos) {
            results.push_back(&cmd);
            continue;
        }
        
        // Check category
        std::string lowerCat = cmd.category;
        std::transform(lowerCat.begin(), lowerCat.end(), lowerCat.begin(), ::tolower);
        
        if (lowerCat.find(lowerQuery) != std::string::npos) {
            results.push_back(&cmd);
        }
    }
    
    // Sort by relevance (exact match first, then alphabetically)
    std::sort(results.begin(), results.end(), [&lowerQuery](Command* a, Command* b) {
        std::string la = a->label;
        std::transform(la.begin(), la.end(), la.begin(), ::tolower);
        std::string lb = b->label;
        std::transform(lb.begin(), lb.end(), lb.begin(), ::tolower);
        
        bool aStarts = la.find(lowerQuery) == 0;
        bool bStarts = lb.find(lowerQuery) == 0;
        
        if (aStarts != bStarts) return aStarts;
        return a->label < b->label;
    });
    
    return results;
}

} // namespace RawrXD::Commands
