#include "keybinding_manager.h"
#include <sstream>

namespace RawrXD::UI {
    KeybindingManager& KeybindingManager::getInstance() {
        static KeybindingManager inst;
        return inst;
    }

    std::string KeybindingManager::keyEventToString(int vk, bool ctrl, bool shift, bool alt) {
        std::string result;
        if (ctrl) result += "Ctrl+";
        if (alt) result += "Alt+";
        if (shift) result += "Shift+";

        // Map common VK codes
        switch (vk) {
            case VK_F1: case VK_F2: case VK_F3: case VK_F4:
            case VK_F5: case VK_F6: case VK_F7: case VK_F8:
            case VK_F9: case VK_F10: case VK_F11: case VK_F12:
                result += "F" + std::to_string(vk - VK_F1 + 1);
                break;
            case VK_RETURN: result += "Enter"; break;
            case VK_ESCAPE: result += "Escape"; break;
            case VK_TAB: result += "Tab"; break;
            case VK_BACK: result += "Backspace"; break;
            case VK_DELETE: result += "Delete"; break;
            case VK_HOME: result += "Home"; break;
            case VK_END: result += "End"; break;
            case VK_PRIOR: result += "PageUp"; break;
            case VK_NEXT: result += "PageDown"; break;
            case VK_LEFT: result += "Left"; break;
            case VK_RIGHT: result += "Right"; break;
            case VK_UP: result += "Up"; break;
            case VK_DOWN: result += "Down"; break;
            default:
                if (vk >= 'A' && vk <= 'Z') result += static_cast<char>(vk);
                else if (vk >= '0' && vk <= '9') result += static_cast<char>(vk);
                else result += "VK" + std::to_string(vk);
        }
        return result;
    }

    void KeybindingManager::bind(const std::string& keySequence, const std::string& commandId) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_bindings[keySequence] = commandId;
    }

    void KeybindingManager::unbind(const std::string& keySequence) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_bindings.erase(keySequence);
    }

    std::string KeybindingManager::resolve(int vk, bool ctrl, bool shift, bool alt) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_bindings.find(keyEventToString(vk, ctrl, shift, alt));
        return (it != m_bindings.end()) ? it->second : "";
    }

    std::vector<std::pair<std::string, std::string>> KeybindingManager::listBindings() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return std::vector<std::pair<std::string, std::string>>(m_bindings.begin(), m_bindings.end());
    }
}
