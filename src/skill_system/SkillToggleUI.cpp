// ============================================================================
// SkillToggleUI.cpp — Win32 UI Implementation
// ============================================================================

#include "SkillToggleUI.h"
#include <windows.h>
#include <windowsx.h>
#include <strsafe.h>
#include <algorithm>

namespace RawrXD {
namespace SkillSystem {

// ============================================================================
// SINGLETON
// ============================================================================
SkillToggleUI& SkillToggleUI::Instance() {
    static SkillToggleUI instance;
    return instance;
}

// ============================================================================
// RENDERING
// ============================================================================
bool SkillToggleUI::RenderTogglePanel(HWND hwndParent, HDC hdc, const RECT& panelRect) {
    auto& engine = SkillInjectionEngine::Instance();
    m_visibleSkills = engine.GetAllSkills();
    
    // Dark theme background
    FillRect(hdc, &panelRect, (HBRUSH)GetStockObject(BLACK_BRUSH));
    
    // Header
    RECT headerRect = { panelRect.left, panelRect.top, panelRect.right, panelRect.top + 32 };
    SetDCBrushColor(hdc, RGB(40, 40, 40));
    FillRect(hdc, &headerRect, (HBRUSH)GetStockObject(DC_BRUSH));
    
    HFONT hHeaderFont = CreateFontA(16, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                    DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                    DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, "Segoe UI");
    HFONT hOldFont = (HFONT)SelectObject(hdc, hHeaderFont);
    SetTextColor(hdc, RGB(255, 255, 255));
    SetBkMode(hdc, TRANSPARENT);
    TextOutA(hdc, headerRect.left + 10, headerRect.top + 8, 
             "SKILL DEFINITIONS", 17);
    SelectObject(hdc, hOldFont);
    DeleteObject(hHeaderFont);
    
    // Stats line
    RECT statsRect = { panelRect.left, panelRect.top + 32, panelRect.right, panelRect.top + 52 };
    SetDCBrushColor(hdc, RGB(30, 30, 30));
    FillRect(hdc, &statsRect, (HBRUSH)GetStockObject(DC_BRUSH));
    
    char statsText[256];
    int activeCount = 0;
    for (const auto& skill : m_visibleSkills) {
        if (engine.IsSkillEnabled(skill.name)) activeCount++;
    }
    StringCchPrintfA(statsText, 256, "%zu skills | %d active | %zu lines injected",
                     m_visibleSkills.size(), activeCount, engine.GetTotalInjectionLines());
    SetTextColor(hdc, RGB(180, 180, 180));
    TextOutA(hdc, statsRect.left + 10, statsRect.top + 4, statsText, 
             static_cast<int>(strlen(statsText)));
    
    // Skill list
    int y = panelRect.top + 58;
    int x = panelRect.left + 10;
    int width = panelRect.right - panelRect.left - 20;
    
    HFONT hFont = CreateFontA(13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                              DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                              DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, "Segoe UI");
    hOldFont = (HFONT)SelectObject(hdc, hFont);
    
    bool anyChanged = false;
    
    for (size_t i = m_scrollOffset; i < m_visibleSkills.size() && 
         y + SKILL_ITEM_HEIGHT < panelRect.bottom - 10; ++i) {
        
        const auto& skill = m_visibleSkills[i];
        bool enabled = engine.IsSkillEnabled(skill.name);
        bool focused = (skill.name == m_focusedSkill);
        
        DrawSkillItem(hdc, x, y, width, skill, focused);
        y += SKILL_ITEM_HEIGHT + 2;
    }
    
    SelectObject(hdc, hOldFont);
    DeleteObject(hFont);
    
    // Scrollbar (if needed)
    if (m_visibleSkills.size() > 0) {
        int visibleItems = (panelRect.bottom - panelRect.top - 58) / (SKILL_ITEM_HEIGHT + 2);
        if (static_cast<int>(m_visibleSkills.size()) > visibleItems) {
            RECT sbRect = { panelRect.right - 12, panelRect.top + 58, panelRect.right, panelRect.bottom - 10 };
            SetDCBrushColor(hdc, RGB(50, 50, 50));
            FillRect(hdc, &sbRect, (HBRUSH)GetStockObject(DC_BRUSH));
            
            // Thumb
            float thumbRatio = static_cast<float>(visibleItems) / m_visibleSkills.size();
            int thumbHeight = static_cast<int>((sbRect.bottom - sbRect.top) * thumbRatio);
            int thumbPos = static_cast<int>(sbRect.top + 
                (m_scrollOffset / static_cast<float>(m_visibleSkills.size())) * (sbRect.bottom - sbRect.top));
            RECT thumbRect = { sbRect.left + 2, thumbPos, sbRect.right - 2, thumbPos + thumbHeight };
            SetDCBrushColor(hdc, RGB(100, 100, 100));
            FillRect(hdc, &thumbRect, (HBRUSH)GetStockObject(DC_BRUSH));
        }
    }
    
    return anyChanged;
}

void SkillToggleUI::DrawSkillItem(HDC hdc, int x, int y, int width, 
                                   const SkillDefinition& skill, bool focused) {
    RECT itemRect = { x, y, x + width, y + SKILL_ITEM_HEIGHT };
    
    // Background
    if (focused) {
        SetDCBrushColor(hdc, RGB(60, 60, 60));
    } else {
        SetDCBrushColor(hdc, RGB(40, 40, 40));
    }
    FillRect(hdc, &itemRect, (HBRUSH)GetStockObject(DC_BRUSH));
    
    // Left border color by status
    COLORREF statusColor;
    if (skill.schemaValid) {
        statusColor = skill.alwaysInject ? RGB(0, 200, 0) : RGB(200, 200, 0);
    } else {
        statusColor = RGB(200, 0, 0);
    }
    RECT borderRect = { x, y, x + 3, y + SKILL_ITEM_HEIGHT };
    SetDCBrushColor(hdc, statusColor);
    FillRect(hdc, &borderRect, (HBRUSH)GetStockObject(DC_BRUSH));
    
    // Toggle switch
    RECT toggleRect = { x + width - TOGGLE_WIDTH - 8, 
                       y + (SKILL_ITEM_HEIGHT - TOGGLE_HEIGHT) / 2,
                       x + width - 8, 
                       y + (SKILL_ITEM_HEIGHT + TOGGLE_HEIGHT) / 2 };
    DrawToggleSwitch(hdc, toggleRect, SkillInjectionEngine::Instance().IsSkillEnabled(skill.name));
    
    // Skill name
    SetTextColor(hdc, focused ? RGB(255, 255, 255) : RGB(200, 200, 200));
    SetBkMode(hdc, TRANSPARENT);
    
    std::string displayName = skill.name;
    if (!skill.specialistAgent.empty() && skill.specialistAgent != "ANY") {
        displayName += " → " + skill.specialistAgent;
    }
    
    // Truncate if too long
    SIZE textSize;
    GetTextExtentPoint32A(hdc, displayName.c_str(), 
                          static_cast<int>(displayName.length()), &textSize);
    int maxTextWidth = width - TOGGLE_WIDTH - 30;
    if (textSize.cx > maxTextWidth && displayName.length() > 3) {
        while (displayName.length() > 3 && textSize.cx > maxTextWidth) {
            displayName = displayName.substr(0, displayName.length() - 1);
            GetTextExtentPoint32A(hdc, (displayName + "...").c_str(),
                                  static_cast<int>(displayName.length() + 3), &textSize);
        }
        displayName += "...";
    }
    
    TextOutA(hdc, x + 10, y + 6, displayName.c_str(), 
             static_cast<int>(displayName.length()));
    
    // Phase badges
    int badgeX = x + 10;
    int badgeY = y + SKILL_ITEM_HEIGHT - 14;
    for (const auto& phase : skill.phases) {
        if (badgeX > x + width - TOGGLE_WIDTH - 50) break;  // Don't overflow
        DrawPhaseBadge(hdc, badgeX, badgeY, phase);
        badgeX += static_cast<int>(phase.length()) * 6 + 10;
    }
    
    // Schema validation indicator
    if (!skill.schemaValid) {
        SetTextColor(hdc, RGB(255, 100, 100));
        TextOutA(hdc, x + width - TOGGLE_WIDTH - 60, y + 6, "⚠", 3);
    }
}

void SkillToggleUI::DrawToggleSwitch(HDC hdc, const RECT& rect, bool enabled) {
    // Track
    SetDCBrushColor(hdc, enabled ? RGB(0, 150, 0) : RGB(80, 80, 80));
    RoundRect(hdc, rect.left, rect.top, rect.right, rect.bottom, 8, 8);
    
    // Knob
    int knobSize = rect.bottom - rect.top - 4;
    int knobX = enabled ? rect.right - knobSize - 2 : rect.left + 2;
    RECT knobRect = { knobX, rect.top + 2, knobX + knobSize, rect.bottom - 2 };
    SetDCBrushColor(hdc, RGB(255, 255, 255));
    FillRect(hdc, &knobRect, (HBRUSH)GetStockObject(DC_BRUSH));
}

void SkillToggleUI::DrawPhaseBadge(HDC hdc, int x, int y, const std::string& phase) {
    RECT badgeRect = { x, y, x + static_cast<int>(phase.length()) * 6 + 6, y + 10 };
    
    // Color by phase
    COLORREF badgeColor;
    if (phase.find("phase1") != std::string::npos) badgeColor = RGB(100, 60, 60);
    else if (phase.find("phase2") != std::string::npos) badgeColor = RGB(60, 100, 60);
    else if (phase.find("phase3") != std::string::npos) badgeColor = RGB(60, 60, 100);
    else if (phase.find("phase4") != std::string::npos) badgeColor = RGB(100, 100, 60);
    else badgeColor = RGB(60, 60, 60);
    
    SetDCBrushColor(hdc, badgeColor);
    FillRect(hdc, &badgeRect, (HBRUSH)GetStockObject(DC_BRUSH));
    
    SetTextColor(hdc, RGB(200, 200, 200));
    SetBkMode(hdc, TRANSPARENT);
    TextOutA(hdc, x + 3, y + 1, phase.c_str(), static_cast<int>(phase.length()));
}

// ============================================================================
// INPUT HANDLING
// ============================================================================
bool SkillToggleUI::HandleClick(int x, int y) {
    int itemIndex = (y - 58) / (SKILL_ITEM_HEIGHT + 2) + static_cast<int>(m_scrollOffset);
    if (itemIndex < 0 || itemIndex >= static_cast<int>(m_visibleSkills.size())) {
        return false;
    }
    
    const auto& skill = m_visibleSkills[itemIndex];
    
    // Check if click is on toggle switch (right side of item)
    // Approximate toggle position based on rendering
    // This is simplified — in production, store exact toggle rects
    SkillInjectionEngine::Instance().ToggleSkill(skill.name);
    
    if (m_toggleCallback) {
        m_toggleCallback(skill.name, SkillInjectionEngine::Instance().IsSkillEnabled(skill.name));
    }
    
    return true;
}

bool SkillToggleUI::HandleKeyPress(WPARAM wParam) {
    switch (wParam) {
        case VK_UP:
            if (!m_focusedSkill.empty()) {
                auto it = std::find_if(m_visibleSkills.begin(), m_visibleSkills.end(),
                    [this](const SkillDefinition& s) { return s.name == m_focusedSkill; });
                if (it != m_visibleSkills.begin()) {
                    --it;
                    m_focusedSkill = it->name;
                    // Adjust scroll if needed
                    size_t index = std::distance(m_visibleSkills.begin(), it);
                    if (index < m_scrollOffset) {
                        m_scrollOffset = index;
                    }
                }
            } else if (!m_visibleSkills.empty()) {
                m_focusedSkill = m_visibleSkills.front().name;
            }
            return true;
            
        case VK_DOWN:
            if (!m_focusedSkill.empty()) {
                auto it = std::find_if(m_visibleSkills.begin(), m_visibleSkills.end(),
                    [this](const SkillDefinition& s) { return s.name == m_focusedSkill; });
                if (it != m_visibleSkills.end() && ++it != m_visibleSkills.end()) {
                    m_focusedSkill = it->name;
                    // Adjust scroll if needed
                    size_t index = std::distance(m_visibleSkills.begin(), it);
                    int visibleItems = 10;  // Approximate
                    if (index >= m_scrollOffset + visibleItems) {
                        m_scrollOffset = index - visibleItems + 1;
                    }
                }
            } else if (!m_visibleSkills.empty()) {
                m_focusedSkill = m_visibleSkills.front().name;
            }
            return true;
            
        case VK_SPACE:
        case VK_RETURN:
            if (!m_focusedSkill.empty()) {
                SkillInjectionEngine::Instance().ToggleSkill(m_focusedSkill);
                if (m_toggleCallback) {
                    m_toggleCallback(m_focusedSkill,
                        SkillInjectionEngine::Instance().IsSkillEnabled(m_focusedSkill));
                }
            }
            return true;
            
        case VK_HOME:
            m_scrollOffset = 0;
            if (!m_visibleSkills.empty()) {
                m_focusedSkill = m_visibleSkills.front().name;
            }
            return true;
            
        case VK_END:
            if (m_visibleSkills.size() > 0) {
                m_scrollOffset = std::max(0, static_cast<int>(m_visibleSkills.size()) - 10);
                m_focusedSkill = m_visibleSkills.back().name;
            }
            return true;
            
        case VK_PRIOR:  // Page Up
            if (m_scrollOffset >= 10) {
                m_scrollOffset -= 10;
            } else {
                m_scrollOffset = 0;
            }
            return true;
            
        case VK_NEXT:  // Page Down
            m_scrollOffset += 10;
            if (m_scrollOffset >= m_visibleSkills.size()) {
                m_scrollOffset = std::max(0, static_cast<int>(m_visibleSkills.size()) - 10);
            }
            return true;
    }
    return false;
}

void SkillToggleUI::SetFocusSkill(const std::string& skillName) {
    m_focusedSkill = skillName;
}

std::string SkillToggleUI::GetFocusedSkill() const {
    return m_focusedSkill;
}

} // namespace SkillSystem
} // namespace RawrXD
