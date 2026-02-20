// ============================================================================
// Shortcut Manager — Implementation (Phase 33 Quick-Win Port)
// ============================================================================

#include "shortcut_manager.hpp"
#include <algorithm>
#include <sstream>
#include <fstream>
#include <cstdio>

// ============================================================================
// Display String Conversion
// ============================================================================
std::string ShortcutBinding::toDisplayString() const
{
    std::string result;
    if (modifiers & MOD_CTRL_KEY)  result += "Ctrl+";
    if (modifiers & MOD_ALT_KEY)   result += "Alt+";
    if (modifiers & MOD_SHIFT_KEY) result += "Shift+";
    if (modifiers & MOD_WIN_KEY)   result += "Win+";

    // Map VK codes to names
    switch (keyCode) {
        case 'A': case 'B': case 'C': case 'D': case 'E': case 'F':
        case 'G': case 'H': case 'I': case 'J': case 'K': case 'L':
        case 'M': case 'N': case 'O': case 'P': case 'Q': case 'R':
        case 'S': case 'T': case 'U': case 'V': case 'W': case 'X':
        case 'Y': case 'Z':
            result += static_cast<char>(keyCode);
            break;
        case '0': case '1': case '2': case '3': case '4':
        case '5': case '6': case '7': case '8': case '9':
            result += static_cast<char>(keyCode);
            break;
#ifdef _WIN32
        case VK_F1:     result += "F1";    break;
        case VK_F2:     result += "F2";    break;
        case VK_F3:     result += "F3";    break;
        case VK_F4:     result += "F4";    break;
        case VK_F5:     result += "F5";    break;
        case VK_F6:     result += "F6";    break;
        case VK_F7:     result += "F7";    break;
        case VK_F8:     result += "F8";    break;
        case VK_F9:     result += "F9";    break;
        case VK_F10:    result += "F10";   break;
        case VK_F11:    result += "F11";   break;
        case VK_F12:    result += "F12";   break;
        case VK_RETURN: result += "Enter"; break;
        case VK_ESCAPE: result += "Esc";   break;
        case VK_TAB:    result += "Tab";   break;
        case VK_SPACE:  result += "Space"; break;
        case VK_BACK:   result += "Backspace"; break;
        case VK_DELETE: result += "Delete"; break;
        case VK_INSERT: result += "Insert"; break;
        case VK_HOME:   result += "Home";  break;
        case VK_END:    result += "End";   break;
        case VK_PRIOR:  result += "PageUp"; break;
        case VK_NEXT:   result += "PageDown"; break;
        case VK_UP:     result += "Up";    break;
        case VK_DOWN:   result += "Down";  break;
        case VK_LEFT:   result += "Left";  break;
        case VK_RIGHT:  result += "Right"; break;
        case VK_OEM_PLUS:   result += "+"; break;
        case VK_OEM_MINUS:  result += "-"; break;
        case VK_OEM_PERIOD: result += "."; break;
        case VK_OEM_COMMA:  result += ","; break;
        case VK_OEM_1:  result += ";";     break;
        case VK_OEM_2:  result += "/";     break;
        case VK_OEM_3:  result += "`";     break;
        case VK_OEM_4:  result += "[";     break;
        case VK_OEM_5:  result += "\\";    break;
        case VK_OEM_6:  result += "]";     break;
        case VK_OEM_7:  result += "'";     break;
#endif
        default:
            result += "0x";
            char hex[8];
            snprintf(hex, sizeof(hex), "%02X", keyCode);
            result += hex;
            break;
    }
    return result;
}

ShortcutBinding ShortcutBinding::fromDisplayString(const std::string& str, int cmdId,
                                                    const std::string& cmdName,
                                                    ShortcutContext ctx)
{
    ShortcutBinding binding = {};
    binding.commandId = cmdId;
    binding.commandName = cmdName;
    binding.context = ctx;
    binding.isDefault = false;
    binding.enabled = true;

    // Parse "Ctrl+Shift+V" style strings
    std::string remaining = str;
    while (true) {
        if (remaining.substr(0, 5) == "Ctrl+") {
            binding.modifiers |= MOD_CTRL_KEY;
            remaining = remaining.substr(5);
        } else if (remaining.substr(0, 4) == "Alt+") {
            binding.modifiers |= MOD_ALT_KEY;
            remaining = remaining.substr(4);
        } else if (remaining.substr(0, 6) == "Shift+") {
            binding.modifiers |= MOD_SHIFT_KEY;
            remaining = remaining.substr(6);
        } else if (remaining.substr(0, 4) == "Win+") {
            binding.modifiers |= MOD_WIN_KEY;
            remaining = remaining.substr(4);
        } else {
            break;
        }
    }

    // Parse key name
    if (remaining.size() == 1 && ((remaining[0] >= 'A' && remaining[0] <= 'Z') ||
                                    (remaining[0] >= '0' && remaining[0] <= '9'))) {
        binding.keyCode = static_cast<uint16_t>(remaining[0]);
    }
#ifdef _WIN32
    else if (remaining == "F1")  binding.keyCode = VK_F1;
    else if (remaining == "F2")  binding.keyCode = VK_F2;
    else if (remaining == "F3")  binding.keyCode = VK_F3;
    else if (remaining == "F4")  binding.keyCode = VK_F4;
    else if (remaining == "F5")  binding.keyCode = VK_F5;
    else if (remaining == "F6")  binding.keyCode = VK_F6;
    else if (remaining == "F7")  binding.keyCode = VK_F7;
    else if (remaining == "F8")  binding.keyCode = VK_F8;
    else if (remaining == "F9")  binding.keyCode = VK_F9;
    else if (remaining == "F10") binding.keyCode = VK_F10;
    else if (remaining == "F11") binding.keyCode = VK_F11;
    else if (remaining == "F12") binding.keyCode = VK_F12;
    else if (remaining == "Enter") binding.keyCode = VK_RETURN;
    else if (remaining == "Esc")   binding.keyCode = VK_ESCAPE;
    else if (remaining == "Tab")   binding.keyCode = VK_TAB;
    else if (remaining == "Space") binding.keyCode = VK_SPACE;
    else if (remaining == "Backspace") binding.keyCode = VK_BACK;
    else if (remaining == "Delete") binding.keyCode = VK_DELETE;
    else if (remaining == "Home")  binding.keyCode = VK_HOME;
    else if (remaining == "End")   binding.keyCode = VK_END;
    else if (remaining == "PageUp") binding.keyCode = VK_PRIOR;
    else if (remaining == "PageDown") binding.keyCode = VK_NEXT;
    else if (remaining == "Up")    binding.keyCode = VK_UP;
    else if (remaining == "Down")  binding.keyCode = VK_DOWN;
    else if (remaining == "Left")  binding.keyCode = VK_LEFT;
    else if (remaining == "Right") binding.keyCode = VK_RIGHT;
#endif

    return binding;
}

// ============================================================================
// ShortcutManager
// ============================================================================
ShortcutManager::ShortcutManager() {}

ShortcutManager::~ShortcutManager()
{
#ifdef _WIN32
    if (m_hAccel) {
        DestroyAcceleratorTable(m_hAccel);
        m_hAccel = nullptr;
    }
#endif
}

void ShortcutManager::registerDefault(int commandId, const std::string& name,
                                       uint16_t modifiers, uint16_t keyCode,
                                       ShortcutContext context)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    ShortcutBinding binding;
    binding.commandId = commandId;
    binding.commandName = name;
    binding.modifiers = modifiers;
    binding.keyCode = keyCode;
    binding.context = context;
    binding.isDefault = true;
    binding.enabled = true;

    m_defaults.push_back(binding);
    m_bindings.push_back(binding);
    m_accelDirty = true;
}

bool ShortcutManager::rebind(int commandId, uint16_t modifiers, uint16_t keyCode)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    for (auto& b : m_bindings) {
        if (b.commandId == commandId) {
            b.modifiers = modifiers;
            b.keyCode = keyCode;
            b.isDefault = false;
            m_accelDirty = true;
            return true;
        }
    }
    return false;
}

bool ShortcutManager::unbind(int commandId)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    for (auto& b : m_bindings) {
        if (b.commandId == commandId) {
            b.enabled = false;
            m_accelDirty = true;
            return true;
        }
    }
    return false;
}

void ShortcutManager::resetToDefaults()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_bindings = m_defaults;
    m_accelDirty = true;
}

void ShortcutManager::resetToDefault(int commandId)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    for (auto& def : m_defaults) {
        if (def.commandId == commandId) {
            for (auto& b : m_bindings) {
                if (b.commandId == commandId) {
                    b = def;
                    m_accelDirty = true;
                    return;
                }
            }
        }
    }
}

int ShortcutManager::findCommand(uint16_t modifiers, uint16_t keyCode,
                                  ShortcutContext context) const
{
    std::lock_guard<std::mutex> lock(m_mutex);

    for (const auto& b : m_bindings) {
        if (!b.enabled) continue;
        if (b.modifiers == modifiers && b.keyCode == keyCode) {
            if (b.context == ShortcutContext::Global || b.context == context) {
                return b.commandId;
            }
        }
    }
    return 0;
}

const ShortcutBinding* ShortcutManager::getBinding(int commandId) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    for (const auto& b : m_bindings) {
        if (b.commandId == commandId) return &b;
    }
    return nullptr;
}

std::vector<ShortcutBinding> ShortcutManager::getAllBindings() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_bindings;
}

std::vector<ShortcutBinding> ShortcutManager::getBindingsForContext(ShortcutContext context) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<ShortcutBinding> result;
    for (const auto& b : m_bindings) {
        if (b.context == context || b.context == ShortcutContext::Global) {
            result.push_back(b);
        }
    }
    return result;
}

std::vector<ShortcutConflict> ShortcutManager::detectConflicts() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<ShortcutConflict> conflicts;

    for (size_t i = 0; i < m_bindings.size(); ++i) {
        if (!m_bindings[i].enabled) continue;
        for (size_t j = i + 1; j < m_bindings.size(); ++j) {
            if (!m_bindings[j].enabled) continue;
            if (m_bindings[i].modifiers == m_bindings[j].modifiers &&
                m_bindings[i].keyCode == m_bindings[j].keyCode) {
                // Same key combo — check context overlap
                if (m_bindings[i].context == ShortcutContext::Global ||
                    m_bindings[j].context == ShortcutContext::Global ||
                    m_bindings[i].context == m_bindings[j].context) {
                    ShortcutConflict conflict;
                    conflict.existing = m_bindings[i];
                    conflict.proposed = m_bindings[j];
                    conflict.description = m_bindings[i].commandName + " conflicts with " +
                                           m_bindings[j].commandName + " (" +
                                           m_bindings[i].toDisplayString() + ")";
                    conflicts.push_back(conflict);
                }
            }
        }
    }
    return conflicts;
}

bool ShortcutManager::hasConflict(uint16_t modifiers, uint16_t keyCode,
                                   ShortcutContext context, int excludeCommandId) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    for (const auto& b : m_bindings) {
        if (!b.enabled) continue;
        if (b.commandId == excludeCommandId) continue;
        if (b.modifiers == modifiers && b.keyCode == keyCode) {
            if (b.context == ShortcutContext::Global || context == ShortcutContext::Global ||
                b.context == context) {
                return true;
            }
        }
    }
    return false;
}

// ============================================================================
// JSON Persistence (minimal manual serializer per project convention)
// ============================================================================
std::string ShortcutManager::exportJSON() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    std::string json = "{\n  \"keybindings\": [\n";

    for (size_t i = 0; i < m_bindings.size(); ++i) {
        const auto& b = m_bindings[i];
        json += "    {";
        json += "\"commandId\":" + std::to_string(b.commandId) + ",";
        json += "\"name\":\"" + b.commandName + "\",";
        json += "\"key\":\"" + b.toDisplayString() + "\",";
        json += "\"context\":" + std::to_string(static_cast<int>(b.context)) + ",";
        json += "\"enabled\":" + std::string(b.enabled ? "true" : "false") + ",";
        json += "\"isDefault\":" + std::string(b.isDefault ? "true" : "false");
        json += "}";
        if (i + 1 < m_bindings.size()) json += ",";
        json += "\n";
    }

    json += "  ]\n}\n";
    return json;
}

bool ShortcutManager::saveToFile(const std::string& path) const
{
    std::string json = exportJSON();
    std::ofstream out(path);
    if (!out.is_open()) return false;
    out << json;
    return out.good();
}

bool ShortcutManager::loadFromFile(const std::string& path)
{
    std::ifstream in(path);
    if (!in.is_open()) return false;

    std::string json((std::istreambuf_iterator<char>(in)),
                      std::istreambuf_iterator<char>());
    return importJSON(json);
}

bool ShortcutManager::importJSON(const std::string& json)
{
    // Simple JSON parser for keybindings array
    // Looks for "key":"..." and "commandId":N patterns
    std::lock_guard<std::mutex> lock(m_mutex);

    // Find each binding object
    size_t pos = 0;
    while ((pos = json.find("\"commandId\":", pos)) != std::string::npos) {
        // Extract commandId
        size_t numStart = pos + 12;
        size_t numEnd = json.find_first_of(",}", numStart);
        if (numEnd == std::string::npos) break;
        int cmdId = std::stoi(json.substr(numStart, numEnd - numStart));

        // Find corresponding key
        size_t keyPos = json.find("\"key\":\"", pos);
        if (keyPos != std::string::npos && keyPos < pos + 200) {
            size_t keyStart = keyPos + 7;
            size_t keyEnd = json.find('"', keyStart);
            if (keyEnd != std::string::npos) {
                std::string keyStr = json.substr(keyStart, keyEnd - keyStart);
                // Apply to existing binding
                auto parsed = ShortcutBinding::fromDisplayString(keyStr, cmdId, "");
                for (auto& b : m_bindings) {
                    if (b.commandId == cmdId) {
                        b.modifiers = parsed.modifiers;
                        b.keyCode = parsed.keyCode;
                        b.isDefault = false;
                        break;
                    }
                }
            }
        }

        pos = numEnd;
    }

    m_accelDirty = true;
    return true;
}

// ============================================================================
// Win32 Accelerator Table
// ============================================================================
#ifdef _WIN32
HACCEL ShortcutManager::buildAcceleratorTable() const
{
    std::lock_guard<std::mutex> lock(m_mutex);

    std::vector<ACCEL> accels;
    for (const auto& b : m_bindings) {
        if (!b.enabled) continue;

        ACCEL accel = {};
        accel.cmd = static_cast<WORD>(b.commandId);

        BYTE fVirt = FVIRTKEY;
        if (b.modifiers & MOD_CTRL_KEY)  fVirt |= FCONTROL;
        if (b.modifiers & MOD_ALT_KEY)   fVirt |= FALT;
        if (b.modifiers & MOD_SHIFT_KEY) fVirt |= FSHIFT;
        accel.fVirt = fVirt;
        accel.key = b.keyCode;

        accels.push_back(accel);
    }

    if (accels.empty()) return nullptr;
    return CreateAcceleratorTableA(accels.data(), static_cast<int>(accels.size()));
}

void ShortcutManager::destroyAcceleratorTable(HACCEL hAccel)
{
    if (hAccel) {
        DestroyAcceleratorTable(hAccel);
    }
}
#endif

size_t ShortcutManager::getBindingCount() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_bindings.size();
}

size_t ShortcutManager::getCustomizedCount() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    size_t count = 0;
    for (const auto& b : m_bindings) {
        if (!b.isDefault) count++;
    }
    return count;
}
