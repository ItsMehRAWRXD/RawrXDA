#include "theme_engine.h"

namespace RawrXD::UI {
    ThemeEngine& ThemeEngine::getInstance() {
        static ThemeEngine inst;
        return inst;
    }

    void ThemeEngine::loadDefaultDark() {
        m_colors["editor.background"] = {0x1E, 0x1E, 0x1E};
        m_colors["editor.foreground"] = {0xD4, 0xD4, 0xD4};
        m_colors["keyword"] = {0x56, 0x9C, 0xD6};
        m_colors["string"] = {0xCE, 0x91, 0x78};
        m_colors["comment"] = {0x6A, 0x99, 0x55};
        m_colors["number"] = {0xB5, 0xCE, 0xA8};
        m_colors["function"] = {0xDC, 0xDC, 0xAA};
        m_colors["operator"] = {0xD4, 0xD4, 0xD4};
        m_colors["identifier"] = {0x9C, 0xDC, 0xFE};
        m_colors["selection"] = {0x26, 0x4F, 0x78};
        m_colors["lineHighlight"] = {0x2A, 0x2D, 0x2E};
        m_colors["statusBar.background"] = {0x00, 0x7A, 0xCC};
        m_colors["statusBar.foreground"] = {0xFF, 0xFF, 0xFF};
    }

    void ThemeEngine::loadDefaultLight() {
        m_colors["editor.background"] = {0xFF, 0xFF, 0xFF};
        m_colors["editor.foreground"] = {0x00, 0x00, 0x00};
        m_colors["keyword"] = {0x00, 0x00, 0xFF};
        m_colors["string"] = {0xA3, 0x15, 0x15};
        m_colors["comment"] = {0x00, 0x80, 0x00};
        m_colors["number"] = {0x09, 0x86, 0x58};
        m_colors["function"] = {0x79, 0x5E, 0x26};
        m_colors["operator"] = {0x00, 0x00, 0x00};
        m_colors["identifier"] = {0x00, 0x00, 0x00};
        m_colors["selection"] = {0xAD, 0xD6, 0xFF};
        m_colors["lineHighlight"] = {0xF5, 0xF5, 0xF5};
        m_colors["statusBar.background"] = {0x00, 0x7A, 0xCC};
        m_colors["statusBar.foreground"] = {0xFF, 0xFF, 0xFF};
    }

    Color ThemeEngine::getColor(const std::string& tokenType) const {
        auto it = m_colors.find(tokenType);
        if (it != m_colors.end()) return it->second;
        return {0xFF, 0xFF, 0xFF};
    }

    void ThemeEngine::setColor(const std::string& tokenType, const Color& color) {
        m_colors[tokenType] = color;
    }

    bool ThemeEngine::loadFromJson(const nlohmann::json& j) {
        if (!j.is_object()) return false;
        for (auto& [key, val] : j.items()) {
            if (val.is_array() && val.size() >= 3) {
                m_colors[key] = {
                    static_cast<uint8_t>(val[0].get<int>()),
                    static_cast<uint8_t>(val[1].get<int>()),
                    static_cast<uint8_t>(val[2].get<int>())
                };
            }
        }
        return true;
    }
}
