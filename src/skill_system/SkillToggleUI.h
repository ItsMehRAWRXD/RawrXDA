// ============================================================================
// SkillToggleUI.h — Win32 UI Toggle Panel for Skill System
// ============================================================================
// Embeddable in Settings dialog, Sidebar, or standalone floating panel
// Provides: skill list, toggle switches, phase badges, priority sorting
//
// Usage:
//   SkillToggleUI::Instance().RenderTogglePanel(hwnd, hdc, rect);
//   SkillToggleUI::Instance().HandleClick(x, y);  // in WM_LBUTTONDOWN
//   SkillToggleUI::Instance().HandleKeyPress(wParam);  // in WM_KEYDOWN
// ============================================================================

#pragma once

#include "SkillInjectionEngine.h"
#include <windows.h>
#include <string>
#include <vector>

namespace RawrXD {
namespace SkillSystem {

class SkillToggleUI {
public:
    static SkillToggleUI& Instance();
    
    // Panel rendering
    bool RenderTogglePanel(HWND hwndParent, HDC hdc, const RECT& panelRect);
    
    // Input handling
    bool HandleClick(int x, int y);
    bool HandleKeyPress(WPARAM wParam);
    
    // Focus management
    void SetFocusSkill(const std::string& skillName);
    std::string GetFocusedSkill() const;
    
    // Notification
    using ToggleCallback = std::function<void(const std::string& skillName, bool enabled)>;
    void SetToggleCallback(ToggleCallback cb);
    
    // Scroll state
    void SetScrollOffset(size_t offset) { m_scrollOffset = offset; }
    size_t GetScrollOffset() const { return m_scrollOffset; }
    
private:
    SkillToggleUI() = default;
    ~SkillToggleUI() = default;
    
    ToggleCallback m_toggleCallback;
    std::string m_focusedSkill;
    std::vector<SkillDefinition> m_visibleSkills;
    size_t m_scrollOffset = 0;
    
    static constexpr int SKILL_ITEM_HEIGHT = 28;
    static constexpr int TOGGLE_WIDTH = 40;
    static constexpr int TOGGLE_HEIGHT = 18;
    
    void DrawToggleSwitch(HDC hdc, const RECT& rect, bool enabled);
    void DrawPhaseBadge(HDC hdc, int x, int y, const std::string& phase);
    void DrawSkillItem(HDC hdc, int x, int y, int width, const SkillDefinition& skill, bool focused);
    
    SkillToggleUI(const SkillToggleUI&) = delete;
    SkillToggleUI& operator=(const SkillToggleUI&) = delete;
};

// ============================================================================
// WIN32 MESSAGE HANDLER MACROS
// ============================================================================
#define SKILL_TOGGLE_HANDLE_CLICK(x, y) \
    RawrXD::SkillSystem::SkillToggleUI::Instance().HandleClick((x), (y))

#define SKILL_TOGGLE_HANDLE_KEY(wParam) \
    RawrXD::SkillSystem::SkillToggleUI::Instance().HandleKeyPress((wParam))

#define SKILL_TOGGLE_RENDER_PANEL(hwnd, hdc, rect) \
    RawrXD::SkillSystem::SkillToggleUI::Instance().RenderTogglePanel((hwnd), (hdc), (rect))

} // namespace SkillSystem
} // namespace RawrXD
