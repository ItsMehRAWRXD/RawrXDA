// ============================================================================
// Win32IDE_Tier3Polish.cpp — Tier 3: Polish (Quality of Life)
// ============================================================================
//
// 31. Smooth Caret Animation        — Interpolated cursor position over frames
// 32. Font Ligatures (DirectWrite)   — Enable ligature features for code fonts
// 33. High DPI Polish                — Per-monitor DPI V2 scale factor handling
// 34. Theme Toggle Animation         — Smooth color lerp over 200ms on switch
// 35. File Watcher Indicators        — ReadDirectoryChangesW → toast notification
// 36. Save Status Indicator          — Dot in tab + * prefix in title bar
// 37. Format on Save Progress        — Status bar "Formatting..." feedback
// 38. Language Mode Quick Switch     — Click status bar → language dropdown
// 39. Encoding Selector UI           — Click status bar → encoding dropdown
//
// Design: Zero simplification — every feature is fully wired.
// ============================================================================

#include "Win32IDE.h"
#include "IDELogger.h"
#include "IocpFileWatcher.h"
#include <richedit.h>
#include <commctrl.h>
#include <dwmapi.h>
#include <shellscalingapi.h>
#include <dwrite.h>
#include <cmath>
#include <algorithm>
#include <sstream>
#include <fstream>
#include <map>

#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "shcore.lib")
#pragma comment(lib, "dwrite.lib")

// ============================================================================
// 31. SMOOTH CARET ANIMATION
// ============================================================================
// Interpolate caret X/Y over 3 frames in render loop. Uses a per-frame timer
// (CARET_ANIM_TIMER_ID) that fires every 16ms (~60 FPS). Each tick lerps the
// displayed caret position toward the target, then invalidates the editor area
// so the caret redraws smoothly.
// ============================================================================

static constexpr UINT CARET_ANIM_TIMER_ID  = 9050;
static constexpr UINT CARET_ANIM_INTERVAL  = 16;   // ~60 FPS
static constexpr float CARET_LERP_SPEED    = 0.35f; // fraction per frame (3-frame convergence)
static constexpr float CARET_SNAP_EPSILON  = 0.5f;  // pixel threshold to snap

void Win32IDE::initSmoothCaret() {
    m_caretAnim.currentX   = 0.0f;
    m_caretAnim.currentY   = 0.0f;
    m_caretAnim.targetX    = 0.0f;
    m_caretAnim.targetY    = 0.0f;
    m_caretAnim.blinkOn    = true;
    m_caretAnim.animating  = false;
    m_caretAnim.blinkPhase = 0;
    m_caretAnim.enabled    = true;

    // Start the caret blink/animation timer
    if (m_hwndMain) {
        SetTimer(m_hwndMain, CARET_ANIM_TIMER_ID, CARET_ANIM_INTERVAL, nullptr);
    }
    LOG_INFO("Smooth caret animation initialized");
}

void Win32IDE::shutdownSmoothCaret() {
    if (m_hwndMain) {
        KillTimer(m_hwndMain, CARET_ANIM_TIMER_ID);
    }
    m_caretAnim.enabled = false;
}

void Win32IDE::updateCaretTarget() {
    if (!m_hwndEditor || !m_caretAnim.enabled) return;

    // Get current caret position from the editor control
    CHARRANGE range;
    SendMessage(m_hwndEditor, EM_EXGETSEL, 0, (LPARAM)&range);

    POINTL pt;
    SendMessage(m_hwndEditor, EM_POSFROMCHAR, (WPARAM)&pt, range.cpMin);

    float newTargetX = static_cast<float>(pt.x);
    float newTargetY = static_cast<float>(pt.y);

    // Only start animation if target actually moved
    if (std::abs(newTargetX - m_caretAnim.targetX) > CARET_SNAP_EPSILON ||
        std::abs(newTargetY - m_caretAnim.targetY) > CARET_SNAP_EPSILON) {
        m_caretAnim.targetX  = newTargetX;
        m_caretAnim.targetY  = newTargetY;
        m_caretAnim.animating = true;
    }
}

void Win32IDE::onCaretAnimationTick() {
    if (!m_caretAnim.enabled) return;

    // Blink toggle (every ~530ms = 33 ticks at 16ms)
    m_caretAnim.blinkPhase++;
    if (m_caretAnim.blinkPhase >= 33) {
        m_caretAnim.blinkPhase = 0;
        m_caretAnim.blinkOn = !m_caretAnim.blinkOn;
    }

    // If animating, lerp current position toward target
    if (m_caretAnim.animating) {
        float dx = m_caretAnim.targetX - m_caretAnim.currentX;
        float dy = m_caretAnim.targetY - m_caretAnim.currentY;

        m_caretAnim.currentX += dx * CARET_LERP_SPEED;
        m_caretAnim.currentY += dy * CARET_LERP_SPEED;

        // Snap when close enough
        if (std::abs(dx) < CARET_SNAP_EPSILON && std::abs(dy) < CARET_SNAP_EPSILON) {
            m_caretAnim.currentX = m_caretAnim.targetX;
            m_caretAnim.currentY = m_caretAnim.targetY;
            m_caretAnim.animating = false;
        }

        // Invalidate caret region for repaint
        if (m_hwndEditor) {
            RECT caretRect;
            caretRect.left   = (int)m_caretAnim.currentX - 2;
            caretRect.top    = (int)m_caretAnim.currentY;
            caretRect.right  = (int)m_caretAnim.currentX + 3;
            caretRect.bottom = (int)m_caretAnim.currentY + m_currentTheme.fontSize + 4;
            InvalidateRect(m_hwndEditor, &caretRect, FALSE);
        }
    }
}

void Win32IDE::renderSmoothCaret(HDC hdc) {
    if (!m_caretAnim.enabled || !m_caretAnim.blinkOn) return;

    int x = (int)m_caretAnim.currentX;
    int y = (int)m_caretAnim.currentY;
    int height = m_currentTheme.fontSize + 2;

    // Draw a smooth 2px-wide caret line in the cursor color
    HPEN pen = CreatePen(PS_SOLID, 2, m_currentTheme.cursorColor);
    HPEN oldPen = (HPEN)SelectObject(hdc, pen);

    MoveToEx(hdc, x, y, nullptr);
    LineTo(hdc, x, y + height);

    SelectObject(hdc, oldPen);
    DeleteObject(pen);
}


// ============================================================================
// 32. FONT LIGATURES (DirectWrite)
// ============================================================================
// Enable DWRITE_FONT_FEATURE_TAG_STANDARD_LIGATURES in DirectWrite so that
// fonts like Fira Code, JetBrains Mono, Cascadia Code render ligatures
// (e.g., != → ≠, => → ⇒, -> → →).
// ============================================================================

void Win32IDE::initDirectWriteLigatures() {
    HRESULT hr = S_OK;

    // Create DirectWrite factory
    hr = DWriteCreateFactory(
        DWRITE_FACTORY_TYPE_SHARED,
        __uuidof(IDWriteFactory),
        reinterpret_cast<IUnknown**>(&m_dwFactory)
    );
    if (FAILED(hr) || !m_dwFactory) {
        LOG_WARNING("DirectWrite factory creation failed — ligatures disabled");
        m_ligaturesEnabled = false;
        return;
    }

    // Create text format with current font
    std::wstring fontName(m_currentTheme.fontName.begin(), m_currentTheme.fontName.end());
    hr = m_dwFactory->CreateTextFormat(
        fontName.c_str(),
        nullptr,                            // font collection (system)
        DWRITE_FONT_WEIGHT_REGULAR,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        static_cast<float>(m_currentTheme.fontSize),
        L"en-us",
        &m_dwTextFormat
    );
    if (FAILED(hr) || !m_dwTextFormat) {
        LOG_WARNING("DirectWrite text format creation failed — ligatures disabled");
        m_ligaturesEnabled = false;
        return;
    }

    m_ligaturesEnabled = true;
    LOG_INFO("DirectWrite ligatures enabled for font: " + m_currentTheme.fontName);
}

void Win32IDE::shutdownDirectWriteLigatures() {
    if (m_dwTextFormat) {
        m_dwTextFormat->Release();
        m_dwTextFormat = nullptr;
    }
    if (m_dwFactory) {
        m_dwFactory->Release();
        m_dwFactory = nullptr;
    }
    m_ligaturesEnabled = false;
}

void Win32IDE::toggleLigatures() {
    m_ligaturesEnabled = !m_ligaturesEnabled;
    if (m_ligaturesEnabled && !m_dwFactory) {
        initDirectWriteLigatures();
    }

    std::string status = m_ligaturesEnabled ? "Font ligatures: ON" : "Font ligatures: OFF";
    if (m_hwndStatusBar) {
        SendMessageA(m_hwndStatusBar, SB_SETTEXT, 0, (LPARAM)status.c_str());
    }
    LOG_INFO(status);

    // Force editor repaint
    if (m_hwndEditor) {
        InvalidateRect(m_hwndEditor, nullptr, TRUE);
    }
}

IDWriteTextLayout* Win32IDE::createLigatureLayout(const std::wstring& text, float maxWidth) {
    if (!m_dwFactory || !m_dwTextFormat) return nullptr;

    IDWriteTextLayout* layout = nullptr;
    HRESULT hr = m_dwFactory->CreateTextLayout(
        text.c_str(),
        static_cast<UINT32>(text.length()),
        m_dwTextFormat,
        maxWidth,
        10000.0f,   // maxHeight (large)
        &layout
    );

    if (SUCCEEDED(hr) && layout) {
        // Enable standard ligatures on the entire text range
        DWRITE_TEXT_RANGE fullRange = { 0, static_cast<UINT32>(text.length()) };
        IDWriteTypography* typography = nullptr;
        hr = m_dwFactory->CreateTypography(&typography);
        if (SUCCEEDED(hr) && typography) {
            // Standard ligatures (liga)
            DWRITE_FONT_FEATURE ligaFeature = {
                DWRITE_FONT_FEATURE_TAG_STANDARD_LIGATURES, 1
            };
            typography->AddFontFeature(ligaFeature);

            // Contextual ligatures (clig)
            DWRITE_FONT_FEATURE cligFeature = {
                DWRITE_FONT_FEATURE_TAG_CONTEXTUAL_LIGATURES, 1
            };
            typography->AddFontFeature(cligFeature);

            // Contextual alternates (calt) — used by most code fonts
            DWRITE_FONT_FEATURE caltFeature = {
                DWRITE_FONT_FEATURE_TAG_CONTEXTUAL_ALTERNATES, 1
            };
            typography->AddFontFeature(caltFeature);

            layout->SetTypography(typography, fullRange);
            typography->Release();
        }
    }

    return layout;
}


// ============================================================================
// 33. HIGH DPI POLISH
// ============================================================================
// Per-monitor DPI awareness V2 is set in the manifest. Here we handle
// WM_DPICHANGED to re-scale fonts, UI dimensions, and control sizes.
// ============================================================================

void Win32IDE::onDpiChanged(UINT newDpi, const RECT* suggestedRect) {
    LOG_INFO("DPI changed: " + std::to_string(m_currentDpi) + " -> " + std::to_string(newDpi));

    UINT oldDpi = m_currentDpi;
    m_currentDpi = newDpi;

    // Apply the suggested window rect from the system
    if (suggestedRect && m_hwndMain) {
        SetWindowPos(m_hwndMain, NULL,
            suggestedRect->left,
            suggestedRect->top,
            suggestedRect->right - suggestedRect->left,
            suggestedRect->bottom - suggestedRect->top,
            SWP_NOZORDER | SWP_NOACTIVATE);
    }

    // Re-scale fonts
    recreateFonts();

    // Re-scale UI dimensions
    float scaleFactor = static_cast<float>(newDpi) / 96.0f;
    m_dpiScaleFactor = scaleFactor;

    // Re-scale sidebar width
    m_sidebarWidth = static_cast<int>(240 * scaleFactor);

    // Re-scale line number gutter
    m_lineNumberWidth = static_cast<int>(50 * scaleFactor);

    // Re-scale minimap width
    if (m_minimapVisible) {
        m_minimapWidth = static_cast<int>(80 * scaleFactor);
    }

    // Force full relayout
    RECT rc;
    GetClientRect(m_hwndMain, &rc);
    onSize(rc.right - rc.left, rc.bottom - rc.top);

    // Force repaint of all surfaces
    InvalidateRect(m_hwndMain, nullptr, TRUE);

    LOG_INFO("DPI scaling applied: factor=" + std::to_string(scaleFactor));
}

int Win32IDE::dpiScaleValue(int basePixels) const {
    return MulDiv(basePixels, m_currentDpi, 96);
}

float Win32IDE::getDpiScaleFactor() const {
    return m_dpiScaleFactor;
}


// ============================================================================
// 34. THEME TOGGLE ANIMATION
// ============================================================================
// Smooth COLORREF lerp over 200ms when switching themes. Instead of instant
// applyTheme(), we capture the old theme colors, compute a lerp target, and
// run a 200ms animation with ~12 frames that gradually blends each color field.
// ============================================================================

static constexpr UINT THEME_ANIM_TIMER_ID  = 9051;
static constexpr UINT THEME_ANIM_INTERVAL  = 16;    // ~60 FPS
static constexpr UINT THEME_ANIM_DURATION  = 200;   // total ms

static COLORREF lerpColor(COLORREF a, COLORREF b, float t) {
    int r = (int)((1.0f - t) * GetRValue(a) + t * GetRValue(b));
    int g = (int)((1.0f - t) * GetGValue(a) + t * GetGValue(b));
    int bl = (int)((1.0f - t) * GetBValue(a) + t * GetBValue(b));
    return RGB(std::clamp(r, 0, 255), std::clamp(g, 0, 255), std::clamp(bl, 0, 255));
}

void Win32IDE::beginThemeTransition(int targetThemeId) {
    // Capture the starting theme
    m_themeTransition.fromTheme = m_currentTheme;
    m_themeTransition.toTheme   = getBuiltinTheme(targetThemeId);
    m_themeTransition.targetThemeId = targetThemeId;
    m_themeTransition.elapsedMs = 0;
    m_themeTransition.active    = true;

    LOG_INFO("Theme transition started: \"" + m_themeTransition.fromTheme.name +
             "\" -> \"" + m_themeTransition.toTheme.name + "\" over " +
             std::to_string(THEME_ANIM_DURATION) + "ms");

    // Start the animation timer
    if (m_hwndMain) {
        SetTimer(m_hwndMain, THEME_ANIM_TIMER_ID, THEME_ANIM_INTERVAL, nullptr);
    }
}

void Win32IDE::onThemeAnimationTick() {
    if (!m_themeTransition.active) return;

    m_themeTransition.elapsedMs += THEME_ANIM_INTERVAL;
    float t = std::min(1.0f, static_cast<float>(m_themeTransition.elapsedMs) /
                              static_cast<float>(THEME_ANIM_DURATION));

    // Smooth ease-out curve: t = 1 - (1-t)^2
    float easeT = 1.0f - (1.0f - t) * (1.0f - t);

    // Lerp all color fields in the current theme
    const IDETheme& from = m_themeTransition.fromTheme;
    const IDETheme& to   = m_themeTransition.toTheme;

    m_currentTheme.backgroundColor    = lerpColor(from.backgroundColor,    to.backgroundColor,    easeT);
    m_currentTheme.textColor          = lerpColor(from.textColor,          to.textColor,          easeT);
    m_currentTheme.keywordColor       = lerpColor(from.keywordColor,       to.keywordColor,       easeT);
    m_currentTheme.commentColor       = lerpColor(from.commentColor,       to.commentColor,       easeT);
    m_currentTheme.stringColor        = lerpColor(from.stringColor,        to.stringColor,        easeT);
    m_currentTheme.numberColor        = lerpColor(from.numberColor,        to.numberColor,        easeT);
    m_currentTheme.operatorColor      = lerpColor(from.operatorColor,      to.operatorColor,      easeT);
    m_currentTheme.preprocessorColor  = lerpColor(from.preprocessorColor,  to.preprocessorColor,  easeT);
    m_currentTheme.functionColor      = lerpColor(from.functionColor,      to.functionColor,      easeT);
    m_currentTheme.typeColor          = lerpColor(from.typeColor,          to.typeColor,          easeT);
    m_currentTheme.selectionColor     = lerpColor(from.selectionColor,     to.selectionColor,     easeT);
    m_currentTheme.selectionTextColor = lerpColor(from.selectionTextColor, to.selectionTextColor, easeT);
    m_currentTheme.lineNumberColor    = lerpColor(from.lineNumberColor,    to.lineNumberColor,    easeT);
    m_currentTheme.lineNumberBg       = lerpColor(from.lineNumberBg,       to.lineNumberBg,       easeT);
    m_currentTheme.currentLineBg      = lerpColor(from.currentLineBg,      to.currentLineBg,      easeT);
    m_currentTheme.cursorColor        = lerpColor(from.cursorColor,        to.cursorColor,        easeT);

    m_currentTheme.sidebarBg          = lerpColor(from.sidebarBg,          to.sidebarBg,          easeT);
    m_currentTheme.sidebarFg          = lerpColor(from.sidebarFg,          to.sidebarFg,          easeT);
    m_currentTheme.sidebarHeaderBg    = lerpColor(from.sidebarHeaderBg,    to.sidebarHeaderBg,    easeT);
    m_currentTheme.activityBarBg      = lerpColor(from.activityBarBg,      to.activityBarBg,      easeT);
    m_currentTheme.activityBarFg      = lerpColor(from.activityBarFg,      to.activityBarFg,      easeT);
    m_currentTheme.activityBarIndicator = lerpColor(from.activityBarIndicator, to.activityBarIndicator, easeT);
    m_currentTheme.activityBarHoverBg = lerpColor(from.activityBarHoverBg, to.activityBarHoverBg, easeT);

    m_currentTheme.tabBarBg           = lerpColor(from.tabBarBg,           to.tabBarBg,           easeT);
    m_currentTheme.tabActiveBg        = lerpColor(from.tabActiveBg,        to.tabActiveBg,        easeT);
    m_currentTheme.tabActiveFg        = lerpColor(from.tabActiveFg,        to.tabActiveFg,        easeT);
    m_currentTheme.tabInactiveBg      = lerpColor(from.tabInactiveBg,      to.tabInactiveBg,      easeT);
    m_currentTheme.tabInactiveFg      = lerpColor(from.tabInactiveFg,      to.tabInactiveFg,      easeT);
    m_currentTheme.tabBorder          = lerpColor(from.tabBorder,          to.tabBorder,          easeT);

    m_currentTheme.statusBarBg        = lerpColor(from.statusBarBg,        to.statusBarBg,        easeT);
    m_currentTheme.statusBarFg        = lerpColor(from.statusBarFg,        to.statusBarFg,        easeT);
    m_currentTheme.statusBarAccent    = lerpColor(from.statusBarAccent,    to.statusBarAccent,    easeT);

    m_currentTheme.panelBg            = lerpColor(from.panelBg,            to.panelBg,            easeT);
    m_currentTheme.panelFg            = lerpColor(from.panelFg,            to.panelFg,            easeT);
    m_currentTheme.panelBorder        = lerpColor(from.panelBorder,        to.panelBorder,        easeT);
    m_currentTheme.panelHeaderBg      = lerpColor(from.panelHeaderBg,      to.panelHeaderBg,      easeT);

    m_currentTheme.titleBarBg         = lerpColor(from.titleBarBg,         to.titleBarBg,         easeT);
    m_currentTheme.titleBarFg         = lerpColor(from.titleBarFg,         to.titleBarFg,         easeT);

    m_currentTheme.scrollbarBg        = lerpColor(from.scrollbarBg,        to.scrollbarBg,        easeT);
    m_currentTheme.scrollbarThumb     = lerpColor(from.scrollbarThumb,     to.scrollbarThumb,     easeT);
    m_currentTheme.scrollbarThumbHover = lerpColor(from.scrollbarThumbHover, to.scrollbarThumbHover, easeT);

    m_currentTheme.bracketMatchBg     = lerpColor(from.bracketMatchBg,     to.bracketMatchBg,     easeT);
    m_currentTheme.indentGuideColor   = lerpColor(from.indentGuideColor,   to.indentGuideColor,   easeT);
    m_currentTheme.accentColor        = lerpColor(from.accentColor,        to.accentColor,        easeT);
    m_currentTheme.errorColor         = lerpColor(from.errorColor,         to.errorColor,         easeT);
    m_currentTheme.warningColor       = lerpColor(from.warningColor,       to.warningColor,       easeT);
    m_currentTheme.infoColor          = lerpColor(from.infoColor,          to.infoColor,          easeT);

    // Apply the interpolated theme to all surfaces
    applyTheme();
    applyThemeToAllControls();

    // Check if animation is complete
    if (t >= 1.0f) {
        m_themeTransition.active = false;
        KillTimer(m_hwndMain, THEME_ANIM_TIMER_ID);

        // Finalize: set the exact target theme
        m_currentTheme = m_themeTransition.toTheme;
        m_activeThemeId = m_themeTransition.targetThemeId;
        applyTheme();
        applyThemeToAllControls();

        // Re-trigger syntax coloring
        if (m_syntaxColoringEnabled) {
            onEditorContentChanged();
        }

        LOG_INFO("Theme transition complete: \"" + m_currentTheme.name + "\"");
    }
}

void Win32IDE::applyThemeByIdAnimated(int themeId) {
    if (themeId < IDM_THEME_DARK_PLUS || themeId > IDM_THEME_ABYSS) return;

    // If a transition is already active, finish it immediately
    if (m_themeTransition.active) {
        m_themeTransition.active = false;
        KillTimer(m_hwndMain, THEME_ANIM_TIMER_ID);
        m_currentTheme = m_themeTransition.toTheme;
        m_activeThemeId = m_themeTransition.targetThemeId;
    }

    // Begin smooth transition
    beginThemeTransition(themeId);

    // Update menu check marks
    if (m_hMenu) {
        for (int id = IDM_THEME_DARK_PLUS; id <= IDM_THEME_ABYSS; id++) {
            CheckMenuItem(m_hMenu, id, MF_BYCOMMAND |
                (id == themeId ? MF_CHECKED : MF_UNCHECKED));
        }
    }

    if (m_hwndStatusBar) {
        IDETheme target = getBuiltinTheme(themeId);
        std::string msg = "Theme: " + target.name + " (transitioning...)";
        SendMessageA(m_hwndStatusBar, SB_SETTEXT, 0, (LPARAM)msg.c_str());
    }
}


// ============================================================================
// 35. FILE WATCHER INDICATORS
// ============================================================================
// Use IocpFileWatcher (already exists) with ReadDirectoryChangesW to detect
// external file changes. When the current file is modified on disk, show a
// toast notification with reload option.
// ============================================================================

void Win32IDE::initFileWatcher() {
    m_fileWatcher = std::make_unique<IocpFileWatcher>();
    m_fileChangedExternally = false;

    m_fileWatcher->SetCallback([this](const std::string& changedFile) {
        onExternalFileChange(changedFile);
    });

    LOG_INFO("File watcher initialized");
}

void Win32IDE::shutdownFileWatcher() {
    if (m_fileWatcher) {
        m_fileWatcher->Stop();
        m_fileWatcher.reset();
    }
}

void Win32IDE::startWatchingFile(const std::string& filePath) {
    if (!m_fileWatcher) return;
    if (filePath.empty()) return;

    // Watch the directory containing the file
    size_t lastSlash = filePath.rfind('\\');
    if (lastSlash == std::string::npos) lastSlash = filePath.rfind('/');
    if (lastSlash == std::string::npos) return;

    std::string dirPath = filePath.substr(0, lastSlash);
    m_watchedFilePath = filePath;

    // Convert to wide string
    std::wstring wDir(dirPath.begin(), dirPath.end());

    // Stop any existing watch and start new one
    m_fileWatcher->Stop();
    if (m_fileWatcher->Start(wDir)) {
        LOG_INFO("Watching directory: " + dirPath + " for changes to: " + filePath);
    } else {
        LOG_WARNING("Failed to start file watcher for: " + dirPath);
    }
}

void Win32IDE::stopWatchingFile() {
    if (m_fileWatcher) {
        m_fileWatcher->Stop();
    }
    m_watchedFilePath.clear();
}

void Win32IDE::onExternalFileChange(const std::string& changedFile) {
    // Check if the changed file matches our watched file
    if (m_watchedFilePath.empty()) return;

    // Extract just the filename for comparison  
    size_t lastSlash = m_watchedFilePath.rfind('\\');
    if (lastSlash == std::string::npos) lastSlash = m_watchedFilePath.rfind('/');
    std::string watchedName = (lastSlash != std::string::npos) ?
        m_watchedFilePath.substr(lastSlash + 1) : m_watchedFilePath;

    if (_stricmp(changedFile.c_str(), watchedName.c_str()) != 0) return;

    m_fileChangedExternally = true;

    // Post to UI thread for toast notification (callback may be on worker thread)
    if (m_hwndMain) {
        PostMessage(m_hwndMain, WM_FILE_CHANGED_EXTERNAL, 0, 0);
    }
}

void Win32IDE::showFileChangedToast() {
    if (!m_fileChangedExternally) return;

    std::string msg = "File changed on disk: " + m_currentFile +
                      "\n\nDo you want to reload it?";

    int result = MessageBoxA(m_hwndMain, msg.c_str(),
                             "File Changed Externally",
                             MB_YESNO | MB_ICONINFORMATION);

    if (result == IDYES) {
        reloadCurrentFile();
    }

    m_fileChangedExternally = false;

    // Update status bar indicator
    if (m_hwndStatusBar) {
        SendMessageA(m_hwndStatusBar, SB_SETTEXT, 0, (LPARAM)"File reloaded from disk");
    }
}

void Win32IDE::reloadCurrentFile() {
    if (m_currentFile.empty()) return;

    std::ifstream file(m_currentFile, std::ios::binary);
    if (file) {
        file.seekg(0, std::ios::end);
        size_t fileSize = static_cast<size_t>(file.tellg());
        file.seekg(0, std::ios::beg);

        if (fileSize > 10 * 1024 * 1024) {
            LOG_WARNING("File too large to reload: " + m_currentFile);
            return;
        }

        std::string content((std::istreambuf_iterator<char>(file)),
                            std::istreambuf_iterator<char>());

        SetWindowTextA(m_hwndEditor, content.c_str());
        m_fileModified = false;
        updateSaveStatusIndicator();
        updateTitleBarText();

        LOG_INFO("File reloaded: " + m_currentFile);
    } else {
        LOG_ERROR("Failed to reload file: " + m_currentFile);
    }
}


// ============================================================================
// 36. SAVE STATUS INDICATOR
// ============================================================================
// Show a dot/bullet in the tab title and * prefix in the title bar when
// the file has unsaved changes. Wired to the isDirty (m_fileModified) flag.
// ============================================================================

void Win32IDE::updateSaveStatusIndicator() {
    // Update title bar with * prefix for unsaved changes
    if (m_hwndMain) {
        std::string title = "RawrXD IDE";
        if (!m_currentFile.empty()) {
            std::string leaf = extractLeafName(m_currentFile);
            if (m_fileModified) {
                title = "● " + leaf + " — RawrXD IDE";
            } else {
                title = leaf + " — RawrXD IDE";
            }
        }
        SetWindowTextA(m_hwndMain, title.c_str());
    }

    // Update the active tab display with modification dot
    updateTabModifiedIndicator();

    // Update status bar
    if (m_hwndStatusBar) {
        if (m_fileModified) {
            SendMessageA(m_hwndStatusBar, SB_SETTEXT, 0, (LPARAM)"● Modified");
        } else {
            SendMessageA(m_hwndStatusBar, SB_SETTEXT, 0, (LPARAM)"Saved");
        }
    }
}

void Win32IDE::updateTabModifiedIndicator() {
    if (m_activeTabIndex < 0 || m_activeTabIndex >= static_cast<int>(m_editorTabs.size())) return;

    EditorTab& tab = m_editorTabs[m_activeTabIndex];
    tab.modified = m_fileModified;

    // Rebuild tab display name with modification indicator
    std::string displayName = tab.displayName;
    if (tab.modified) {
        // Add bullet prefix for modified tabs (VS Code style)
        if (displayName.find("● ") != 0) {
            tab.displayName = "● " + displayName;
        }
    } else {
        // Remove bullet prefix if present
        if (displayName.find("● ") == 0) {
            tab.displayName = displayName.substr(4); // "● " is 4 bytes (UTF-8 bullet)
        }
    }

    // Repaint tab bar
    if (m_hwndTabBar) {
        InvalidateRect(m_hwndTabBar, nullptr, TRUE);
    }
}

void Win32IDE::markFileModified() {
    if (!m_fileModified) {
        m_fileModified = true;
        updateSaveStatusIndicator();
    }
}

void Win32IDE::markFileSaved() {
    if (m_fileModified) {
        m_fileModified = false;
        updateSaveStatusIndicator();
    }
}


// ============================================================================
// 37. FORMAT ON SAVE PROGRESS
// ============================================================================
// Show "Formatting..." in the status bar during format-on-save, with a
// progress callback for the LSP format request. After formatting completes,
// shows "Formatted" briefly, then reverts to normal status.
// ============================================================================

static constexpr UINT FORMAT_STATUS_TIMER_ID = 9052;
static constexpr UINT FORMAT_STATUS_CLEAR_MS = 2000; // clear "Formatted" after 2s

void Win32IDE::showFormatOnSaveProgress() {
    m_formatInProgress = true;

    if (m_hwndStatusBar) {
        SendMessageA(m_hwndStatusBar, SB_SETTEXT, 0, (LPARAM)"⏳ Formatting...");
    }

    LOG_INFO("Format on save started");
}

void Win32IDE::onFormatComplete(bool success) {
    m_formatInProgress = false;

    if (m_hwndStatusBar) {
        if (success) {
            SendMessageA(m_hwndStatusBar, SB_SETTEXT, 0, (LPARAM)"✅ Formatted");
        } else {
            SendMessageA(m_hwndStatusBar, SB_SETTEXT, 0, (LPARAM)"⚠ Format failed");
        }

        // Clear the format status after a delay
        if (m_hwndMain) {
            SetTimer(m_hwndMain, FORMAT_STATUS_TIMER_ID, FORMAT_STATUS_CLEAR_MS, nullptr);
        }
    }

    LOG_INFO(std::string("Format on save ") + (success ? "completed" : "failed"));
}

void Win32IDE::onFormatStatusTimerExpired() {
    KillTimer(m_hwndMain, FORMAT_STATUS_TIMER_ID);
    updateSaveStatusIndicator(); // Restore normal status
}

bool Win32IDE::formatAndSave() {
    showFormatOnSaveProgress();

    // Attempt to format via LSP
    bool formatted = false;
    if (m_lspFormatEnabled) {
        formatted = requestLSPFormat();
    }

    onFormatComplete(formatted);

    // Proceed with save regardless of format result
    return saveFile();
}

bool Win32IDE::requestLSPFormat() {
    // Send textDocument/formatting request to LSP server via Win32IDE_LSPClient.cpp
    if (m_currentFile.empty()) return false;

    // Detect language and check LSP availability
    LSPLanguage lang = detectLanguageForFile(m_currentFile);
    if (lang >= LSPLanguage::Count) {
        LOG_INFO("LSP format: no language server for " + m_currentFile);
        return false;
    }
    if (m_lspStatuses[(size_t)lang].state != LSPServerState::Running) {
        LOG_INFO("LSP format: server not running for " + m_currentFile);
        return false;
    }

    // Build textDocument/formatting params
    // LSP spec: { textDocument: { uri }, options: { tabSize, insertSpaces } }
    std::string uri = filePathToUri(m_currentFile);
    nlohmann::json params;
    params["textDocument"]["uri"] = uri;
    params["options"]["tabSize"]      = m_settings.tabWidth > 0 ? m_settings.tabWidth : 4;
    params["options"]["insertSpaces"] = !m_settings.useTabCharacter;
    params["options"]["trimTrailingWhitespace"]    = true;
    params["options"]["insertFinalNewline"]        = true;
    params["options"]["trimFinalNewlines"]         = true;

    int id = sendLSPRequest(lang, "textDocument/formatting", params);
    if (id < 0) {
        LOG_INFO("LSP format: sendLSPRequest failed for " + m_currentFile);
        return false;
    }

    // Read response (10s timeout — formatting can be slow on large files)
    nlohmann::json resp = readLSPResponse(lang, id, 10000);

    if (!resp.contains("result") || resp["result"].is_null()) {
        LOG_INFO("LSP format: null result for " + m_currentFile);
        return false;
    }

    const auto& result = resp["result"];
    if (!result.is_array() || result.empty()) {
        LOG_INFO("LSP format: empty edits for " + m_currentFile);
        return false;
    }

    // Build a workspace edit from the formatting TextEdit[] response
    LSPWorkspaceEdit edit;
    std::vector<LSPWorkspaceEdit::TextEdit> textEdits;
    for (size_t i = 0; i < result.size(); ++i) {
        const auto& ej = result[i];
        LSPWorkspaceEdit::TextEdit te;
        te.newText = ej.value("newText", "");
        if (ej.contains("range")) {
            const auto& rj = ej["range"];
            if (rj.contains("start")) {
                te.range.start.line      = rj["start"].value("line", 0);
                te.range.start.character = rj["start"].value("character", 0);
            }
            if (rj.contains("end")) {
                te.range.end.line      = rj["end"].value("line", 0);
                te.range.end.character = rj["end"].value("character", 0);
            }
        }
        textEdits.push_back(te);
    }
    edit.changes[uri] = textEdits;

    // Apply the formatting edits
    bool ok = applyWorkspaceEdit(edit);
    if (ok) {
        LOG_INFO("LSP format: applied " + std::to_string(textEdits.size()) + " edits to " + m_currentFile);
        // Reload buffer from disk into editor control
        loadFileIntoEditor(m_currentFile);
    } else {
        LOG_INFO("LSP format: applyWorkspaceEdit failed for " + m_currentFile);
    }
    return ok;
}


// ============================================================================
// 38. LANGUAGE MODE QUICK SWITCH
// ============================================================================
// Click "C++" in the status bar → dropdown list of all supported languages.
// Selecting a language immediately re-applies syntax highlighting.
// ============================================================================

void Win32IDE::showLanguageModeSelector() {
    if (!m_hwndMain) return;

    // Build the language list
    static const struct { const char* name; const char* extensions; } languages[] = {
        {"Assembly",           "*.asm;*.s"},
        {"Batch",              "*.bat;*.cmd"},
        {"C",                  "*.c"},
        {"C#",                 "*.cs"},
        {"C++",                "*.cpp;*.cc;*.cxx"},
        {"C/C++ Header",       "*.h;*.hpp;*.hxx"},
        {"Config",             "*.cfg;*.ini"},
        {"CSS",                "*.css"},
        {"F#",                 "*.fs"},
        {"Go",                 "*.go"},
        {"HTML",               "*.html;*.htm"},
        {"INI",                "*.ini"},
        {"Java",               "*.java"},
        {"JavaScript",         "*.js"},
        {"JavaScript React",   "*.jsx"},
        {"JSON",               "*.json"},
        {"Kotlin",             "*.kt"},
        {"Less",               "*.less"},
        {"Lua",                "*.lua"},
        {"Markdown",           "*.md"},
        {"PHP",                "*.php"},
        {"Plain Text",         "*.txt"},
        {"PowerShell",         "*.ps1;*.psm1;*.psd1"},
        {"Python",             "*.py"},
        {"R",                  "*.r"},
        {"Ruby",               "*.rb"},
        {"Rust",               "*.rs"},
        {"Scala",              "*.scala"},
        {"SCSS",               "*.scss"},
        {"Shell Script",       "*.sh;*.bash;*.zsh"},
        {"SQL",                "*.sql"},
        {"Swift",              "*.swift"},
        {"TOML",               "*.toml"},
        {"TypeScript",         "*.ts"},
        {"TypeScript React",   "*.tsx"},
        {"Visual Basic",       "*.vb"},
        {"XML",                "*.xml"},
        {"YAML",               "*.yaml;*.yml"},
    };
    constexpr int langCount = sizeof(languages) / sizeof(languages[0]);

    // Get status bar position for the language segment to anchor popup
    RECT statusRect;
    GetWindowRect(m_hwndStatusBar, &statusRect);

    // Create popup menu
    HMENU hMenu = CreatePopupMenu();
    for (int i = 0; i < langCount; i++) {
        UINT flags = MF_STRING;
        if (m_statusBarInfo.languageMode == languages[i].name) {
            flags |= MF_CHECKED;
        }
        AppendMenuA(hMenu, flags, IDM_LANGMODE_FIRST + i, languages[i].name);
    }

    // Show popup near the language segment in the status bar
    int x = statusRect.right - 200;
    int y = statusRect.top;
    int choice = TrackPopupMenuEx(hMenu, TPM_RETURNCMD | TPM_NONOTIFY | TPM_BOTTOMALIGN,
                                  x, y, m_hwndMain, nullptr);

    if (choice >= IDM_LANGMODE_FIRST && choice < IDM_LANGMODE_FIRST + langCount) {
        int index = choice - IDM_LANGMODE_FIRST;
        m_statusBarInfo.languageMode = languages[index].name;
        updateEnhancedStatusBar();

        // Re-trigger syntax highlighting with new language
        if (m_syntaxColoringEnabled) {
            onEditorContentChanged();
        }

        LOG_INFO("Language mode changed to: " + m_statusBarInfo.languageMode);
    }

    DestroyMenu(hMenu);
}


// ============================================================================
// 39. ENCODING SELECTOR UI
// ============================================================================
// Click "UTF-8" in the status bar → dropdown list of encodings.
// Selecting an encoding re-opens/re-interprets the file with that encoding.
// ============================================================================

void Win32IDE::showEncodingSelector() {
    if (!m_hwndMain) return;

    static const struct { const char* name; int codePage; } encodings[] = {
        {"UTF-8",                  CP_UTF8},
        {"UTF-8 with BOM",         CP_UTF8},    // BOM flag handled separately
        {"UTF-16 LE",              1200},
        {"UTF-16 BE",              1201},
        {"Windows-1252 (Western)", 1252},
        {"ISO 8859-1 (Latin-1)",   28591},
        {"ISO 8859-15 (Latin-9)",  28605},
        {"Windows-1250 (Central)", 1250},
        {"Windows-1251 (Cyrillic)",1251},
        {"Windows-1253 (Greek)",   1253},
        {"Windows-1254 (Turkish)", 1254},
        {"Windows-1256 (Arabic)",  1256},
        {"Shift JIS",              932},
        {"GB2312",                 936},
        {"EUC-KR",                 949},
        {"Big5",                   950},
        {"ASCII",                  20127},
    };
    constexpr int encCount = sizeof(encodings) / sizeof(encodings[0]);

    // Get status bar position for encoding segment
    RECT statusRect;
    GetWindowRect(m_hwndStatusBar, &statusRect);

    // Create submenu structure: "Reopen with Encoding" and "Save with Encoding"
    HMENU hMenu = CreatePopupMenu();

    HMENU hReopenMenu = CreatePopupMenu();
    HMENU hSaveMenu   = CreatePopupMenu();

    for (int i = 0; i < encCount; i++) {
        UINT flags = MF_STRING;
        if (m_statusBarInfo.encoding == encodings[i].name) {
            flags |= MF_CHECKED;
        }
        AppendMenuA(hReopenMenu, flags, IDM_ENCODING_REOPEN_FIRST + i, encodings[i].name);
        AppendMenuA(hSaveMenu,   flags, IDM_ENCODING_SAVE_FIRST + i,   encodings[i].name);
    }

    AppendMenuA(hMenu, MF_POPUP, (UINT_PTR)hReopenMenu, "Reopen with Encoding");
    AppendMenuA(hMenu, MF_POPUP, (UINT_PTR)hSaveMenu,   "Save with Encoding");

    // Show popup near the encoding segment
    int x = statusRect.right - 300;
    int y = statusRect.top;
    int choice = TrackPopupMenuEx(hMenu, TPM_RETURNCMD | TPM_NONOTIFY | TPM_BOTTOMALIGN,
                                  x, y, m_hwndMain, nullptr);

    if (choice >= IDM_ENCODING_REOPEN_FIRST && choice < IDM_ENCODING_REOPEN_FIRST + encCount) {
        int index = choice - IDM_ENCODING_REOPEN_FIRST;
        reopenWithEncoding(encodings[index].name, encodings[index].codePage);
    } else if (choice >= IDM_ENCODING_SAVE_FIRST && choice < IDM_ENCODING_SAVE_FIRST + encCount) {
        int index = choice - IDM_ENCODING_SAVE_FIRST;
        saveWithEncoding(encodings[index].name, encodings[index].codePage);
    }

    DestroyMenu(hMenu);
}

void Win32IDE::reopenWithEncoding(const char* encodingName, int codePage) {
    if (m_currentFile.empty()) return;

    LOG_INFO("Reopening file with encoding: " + std::string(encodingName));

    // Read raw bytes from disk
    std::ifstream file(m_currentFile, std::ios::binary);
    if (!file) {
        LOG_ERROR("Failed to open file for re-encoding: " + m_currentFile);
        return;
    }

    std::string rawBytes((std::istreambuf_iterator<char>(file)),
                          std::istreambuf_iterator<char>());
    file.close();

    // Convert from the specified codepage to UTF-16, then to UTF-8 for display
    if (codePage == CP_UTF8 || codePage == 20127) {
        // Already UTF-8 or ASCII — use directly
        SetWindowTextA(m_hwndEditor, rawBytes.c_str());
    } else {
        // Convert from codePage → UTF-16 → UTF-8
        int wideLen = MultiByteToWideChar(codePage, 0, rawBytes.c_str(),
                                          static_cast<int>(rawBytes.size()), nullptr, 0);
        if (wideLen > 0) {
            std::wstring wide(wideLen, L'\0');
            MultiByteToWideChar(codePage, 0, rawBytes.c_str(),
                               static_cast<int>(rawBytes.size()), &wide[0], wideLen);

            int utf8Len = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), wideLen,
                                              nullptr, 0, nullptr, nullptr);
            if (utf8Len > 0) {
                std::string utf8(utf8Len, '\0');
                WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), wideLen,
                                   &utf8[0], utf8Len, nullptr, nullptr);
                SetWindowTextA(m_hwndEditor, utf8.c_str());
            }
        }
    }

    // Update encoding display
    m_statusBarInfo.encoding = encodingName;
    m_currentEncoding = codePage;
    m_fileModified = false;
    updateEnhancedStatusBar();
    updateSaveStatusIndicator();

    LOG_INFO("File reopened with encoding: " + std::string(encodingName));
}

void Win32IDE::saveWithEncoding(const char* encodingName, int codePage) {
    if (m_currentFile.empty()) return;

    LOG_INFO("Saving file with encoding: " + std::string(encodingName));

    // Get current editor content (UTF-8) 
    int textLen = GetWindowTextLengthA(m_hwndEditor);
    if (textLen <= 0) return;

    std::string utf8Content(textLen + 1, '\0');
    GetWindowTextA(m_hwndEditor, &utf8Content[0], textLen + 1);
    utf8Content.resize(textLen);

    std::ofstream outFile(m_currentFile, std::ios::binary);
    if (!outFile) {
        LOG_ERROR("Failed to open file for saving: " + m_currentFile);
        return;
    }

    if (codePage == CP_UTF8) {
        // Check if BOM is requested
        if (std::string(encodingName) == "UTF-8 with BOM") {
            outFile.write("\xEF\xBB\xBF", 3); // UTF-8 BOM
        }
        outFile.write(utf8Content.c_str(), utf8Content.size());
    } else {
        // Convert UTF-8 → UTF-16 → target codepage
        int wideLen = MultiByteToWideChar(CP_UTF8, 0, utf8Content.c_str(),
                                          static_cast<int>(utf8Content.size()), nullptr, 0);
        if (wideLen > 0) {
            std::wstring wide(wideLen, L'\0');
            MultiByteToWideChar(CP_UTF8, 0, utf8Content.c_str(),
                               static_cast<int>(utf8Content.size()), &wide[0], wideLen);

            int mbLen = WideCharToMultiByte(codePage, 0, wide.c_str(), wideLen,
                                           nullptr, 0, nullptr, nullptr);
            if (mbLen > 0) {
                std::string encoded(mbLen, '\0');
                WideCharToMultiByte(codePage, 0, wide.c_str(), wideLen,
                                   &encoded[0], mbLen, nullptr, nullptr);
                outFile.write(encoded.c_str(), encoded.size());
            }
        }
    }

    outFile.close();

    // Update encoding display
    m_statusBarInfo.encoding = encodingName;
    m_currentEncoding = codePage;
    m_fileModified = false;
    updateEnhancedStatusBar();
    updateSaveStatusIndicator();

    LOG_INFO("File saved with encoding: " + std::string(encodingName));
}


// ============================================================================
// TIER 3 TIMER DISPATCH
// ============================================================================
// Routes WM_TIMER messages for all Tier 3 features from the main WndProc.
// Called by handleMessage() when it detects a Tier 3 timer ID.
// ============================================================================

bool Win32IDE::handleTier3Timer(UINT_PTR timerId) {
    switch (timerId) {
    case CARET_ANIM_TIMER_ID:
        onCaretAnimationTick();
        return true;
    case THEME_ANIM_TIMER_ID:
        onThemeAnimationTick();
        return true;
    case FORMAT_STATUS_TIMER_ID:
        onFormatStatusTimerExpired();
        return true;
    default:
        return false;
    }
}


// ============================================================================
// TIER 3 STATUS BAR CLICK HANDLER
// ============================================================================
// Detects clicks on the Language Mode (part 10) or Encoding (part 8) segments
// of the status bar and shows the appropriate popup selector.
// ============================================================================

void Win32IDE::handleStatusBarClick(int x, int y) {
    if (!m_hwndStatusBar) return;

    // Get the status bar part information
    RECT partRect;

    // Check Language Mode segment (part 10)
    if (SendMessage(m_hwndStatusBar, SB_GETRECT, 10, (LPARAM)&partRect)) {
        POINT pt = { x, y };
        if (PtInRect(&partRect, pt)) {
            showLanguageModeSelector();
            return;
        }
    }

    // Check Encoding segment (part 8)
    if (SendMessage(m_hwndStatusBar, SB_GETRECT, 8, (LPARAM)&partRect)) {
        POINT pt = { x, y };
        if (PtInRect(&partRect, pt)) {
            showEncodingSelector();
            return;
        }
    }
}


// ============================================================================
// TIER 3 INITIALIZATION & SHUTDOWN
// ============================================================================

void Win32IDE::initTier3Polish() {
    LOG_INFO("Initializing Tier 3: Polish features");

    initSmoothCaret();
    initDirectWriteLigatures();
    initFileWatcher();

    // Start watching the current file if one is open
    if (!m_currentFile.empty()) {
        startWatchingFile(m_currentFile);
    }

    // Initialize state
    m_formatInProgress = false;
    m_lspFormatEnabled = false;
    m_currentEncoding  = CP_UTF8;
    m_dpiScaleFactor   = static_cast<float>(m_currentDpi) / 96.0f;
    m_themeTransition.active = false;

    // Update save status for current file
    updateSaveStatusIndicator();

    LOG_INFO("Tier 3: Polish features initialized (9 features)");
}

void Win32IDE::shutdownTier3Polish() {
    LOG_INFO("Shutting down Tier 3: Polish features");

    shutdownSmoothCaret();
    shutdownDirectWriteLigatures();
    shutdownFileWatcher();

    LOG_INFO("Tier 3: Polish features shut down");
}
