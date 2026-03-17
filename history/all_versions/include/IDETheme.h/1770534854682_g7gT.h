// ============================================================================
// IDETheme.h — Standalone IDETheme struct definition
// ============================================================================
//
// Extracted from Win32IDE.h so that MonacoCoreEngine.cpp (and any other
// translation unit) can use the full IDETheme definition without pulling
// in the entire 4000-line Win32IDE.h header and its dependency tree.
//
// Consumers:
//   - editor_engine.h       (IEditorEngine::applyTheme)
//   - MonacoCoreEngine.cpp  (maps COLORREF → D2D1_COLOR_F)
//   - Win32IDE.h            (includes this instead of defining inline)
//   - Win32IDE_Themes.cpp   (populates theme structs)
//
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <string>
#include <map>

// ============================================================================
// IDETheme — Full theme definition for RawrXD IDE
// ============================================================================
struct IDETheme {
    std::string name;                   // Display name ("Dracula", "Nord", etc.)
    bool darkMode;

    // Core editor
    COLORREF backgroundColor;            // Editor pane background
    COLORREF textColor;                  // Default text / foreground
    COLORREF keywordColor;
    COLORREF commentColor;
    COLORREF stringColor;
    COLORREF numberColor;
    COLORREF operatorColor;
    COLORREF preprocessorColor;
    COLORREF functionColor;
    COLORREF typeColor;                  // Built-in types / classes
    COLORREF selectionColor;             // Selection highlight
    COLORREF selectionTextColor;         // Text on selected background
    COLORREF lineNumberColor;            // Gutter line numbers
    COLORREF lineNumberBg;               // Gutter background
    COLORREF currentLineBg;              // Active line highlight
    COLORREF cursorColor;

    // Sidebar / Activity bar
    COLORREF sidebarBg;
    COLORREF sidebarFg;
    COLORREF sidebarHeaderBg;
    COLORREF activityBarBg;
    COLORREF activityBarFg;
    COLORREF activityBarIndicator;       // Active-tab accent stripe
    COLORREF activityBarHoverBg;

    // Tab bar
    COLORREF tabBarBg;
    COLORREF tabActiveBg;
    COLORREF tabActiveFg;
    COLORREF tabInactiveBg;
    COLORREF tabInactiveFg;
    COLORREF tabBorder;

    // Status bar
    COLORREF statusBarBg;
    COLORREF statusBarFg;
    COLORREF statusBarAccent;            // Remote / debug indicator

    // Terminal / Output panel
    COLORREF panelBg;
    COLORREF panelFg;
    COLORREF panelBorder;
    COLORREF panelHeaderBg;

    // Title bar
    COLORREF titleBarBg;
    COLORREF titleBarFg;

    // Scrollbar
    COLORREF scrollbarBg;
    COLORREF scrollbarThumb;
    COLORREF scrollbarThumbHover;

    // Bracket matching / indent guides
    COLORREF bracketMatchBg;
    COLORREF indentGuideColor;

    // Accent / brand
    COLORREF accentColor;                // Primary brand accent
    COLORREF errorColor;                 // Squiggle / diagnostic
    COLORREF warningColor;
    COLORREF infoColor;

    // Font
    std::string fontName;
    int fontSize;

    // Transparency (0 = fully transparent, 255 = opaque)
    BYTE windowAlpha;

    // Per-language syntax palette overrides (optional)
    // When a language key is present, its non-zero fields override the
    // theme-global keyword/comment/string/etc colors in getTokenColor().
    struct LanguageTokenPalette {
        COLORREF keywordColor;       // 0 = use theme global
        COLORREF commentColor;
        COLORREF stringColor;
        COLORREF numberColor;
        COLORREF operatorColor;
        COLORREF preprocessorColor;
        COLORREF functionColor;
        COLORREF typeColor;
        COLORREF bracketColor;
        COLORREF textColor;          // Default text
    };
    std::map<int, LanguageTokenPalette> languagePalettes; // Keyed by SyntaxLanguage enum cast to int
};
