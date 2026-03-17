// ============================================================================
// Win32IDE_MonacoThemes.cpp — Theme Bridge: Win32 IDETheme → Monaco defineTheme
// ============================================================================
//
// Phase 26: WebView2 Integration — Feature #206
//
// This file exports all 16 Win32 IDETheme definitions to Monaco's
// IStandaloneThemeData format, enabling pixel-perfect reproduction
// of every RawrXD theme inside the WebView2 Monaco editor.
//
// The bridge maps:
//   IDETheme COLORREF fields → Monaco editor.* color keys (#RRGGBB hex)
//   IDETheme syntax colors   → Monaco ITokenThemeRule[] (token/foreground)
//   Per-language palettes     → Extended token rules (e.g. ASM mnemonics)
//
// The generated JavaScript is embedded in the Monaco HTML page at startup
// via MonacoThemeExporter::toAllDefineThemeJs(), which produces a block of
// monaco.editor.defineTheme() calls — one per theme.
//
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#include "Win32IDE_WebView2.h"
#include "Win32IDE.h"

#include <sstream>
#include <iomanip>
#include <cstdio>

// ============================================================================
// COLORREF → "#RRGGBB" hex string conversion
// ============================================================================
// NOTE: Win32 COLORREF is 0x00BBGGRR (little-endian BGR).
// Monaco expects standard web hex "#RRGGBB".
// ============================================================================
static std::string colorrefToHex(COLORREF c) {
    char buf[8];
    snprintf(buf, sizeof(buf), "#%02X%02X%02X",
             GetRValue(c), GetGValue(c), GetBValue(c));
    return std::string(buf);
}

// COLORREF → "RRGGBB" (no hash, for Monaco token rules)
static std::string colorrefToHexNoHash(COLORREF c) {
    char buf[8];
    snprintf(buf, sizeof(buf), "%02X%02X%02X",
             GetRValue(c), GetGValue(c), GetBValue(c));
    return std::string(buf);
}

// ============================================================================
// Theme Name Mapping
// ============================================================================
const char* MonacoThemeExporter::monacoThemeName(int themeId) {
    switch (themeId) {
        case IDM_THEME_DARK_PLUS:        return "rawrxd-dark-plus";
        case IDM_THEME_LIGHT_PLUS:       return "rawrxd-light-plus";
        case IDM_THEME_MONOKAI:          return "rawrxd-monokai";
        case IDM_THEME_DRACULA:          return "rawrxd-dracula";
        case IDM_THEME_NORD:             return "rawrxd-nord";
        case IDM_THEME_SOLARIZED_DARK:   return "rawrxd-solarized-dark";
        case IDM_THEME_SOLARIZED_LIGHT:  return "rawrxd-solarized-light";
        case IDM_THEME_CYBERPUNK_NEON:   return "rawrxd-cyberpunk-neon";
        case IDM_THEME_GRUVBOX_DARK:     return "rawrxd-gruvbox-dark";
        case IDM_THEME_CATPPUCCIN_MOCHA: return "rawrxd-catppuccin-mocha";
        case IDM_THEME_TOKYO_NIGHT:      return "rawrxd-tokyo-night";
        case IDM_THEME_RAWRXD_CRIMSON:   return "rawrxd-crimson";
        case IDM_THEME_HIGH_CONTRAST:    return "rawrxd-high-contrast";
        case IDM_THEME_ONE_DARK_PRO:     return "rawrxd-one-dark-pro";
        case IDM_THEME_SYNTHWAVE84:      return "rawrxd-synthwave84";
        case IDM_THEME_ABYSS:            return "rawrxd-abyss";
        default:                         return "rawrxd-dark-plus";
    }
}

// ============================================================================
// Export a Single Theme: IDM_THEME_xxx → MonacoThemeDef
// ============================================================================
// This creates a MonacoThemeDef by:
//   1. Getting the IDETheme from Win32IDE::getBuiltinTheme() (static call)
//   2. Setting the base theme (vs/vs-dark/hc-black) based on darkMode + HC
//   3. Building token rules from syntax color fields
//   4. Mapping all editor chrome colors to Monaco color keys
// ============================================================================

MonacoThemeDef MonacoThemeExporter::exportTheme(int themeId) {
    // We need a temporary Win32IDE instance just to call getBuiltinTheme.
    // Since getBuiltinTheme() is const and doesn't need window handles,
    // we create a minimal static helper that builds the theme directly
    // from the theme factory in Win32IDE_Themes.cpp.
    //
    // For the exporter, we reconstruct the same data without needing a
    // live Win32IDE instance. The getBuiltinTheme function is a const
    // member, so we use a workaround: a temporary default-constructed
    // Win32IDE is too heavy, so we directly build the theme from the
    // same switch-case data.
    //
    // Alternative approach: We reference the factory function via a
    // standalone helper declared as friend. For now, we use a simpler
    // approach — the theme data is statically reproduced here via the
    // same getBuiltinTheme mechanism through a default-init trick.

    // Build the IDETheme via the factory
    // Note: getBuiltinTheme is a const member of Win32IDE. Since we only
    // need the color data, and the function is pure (no side effects),
    // we use a dummy Win32IDE that's zero-init'd for just this call.
    // This works because getBuiltinTheme doesn't touch any window handles.

    // We can't easily default-construct Win32IDE here, so instead we
    // build MonacoThemeDef directly from the known color values.
    // The theme factory is in Win32IDE_Themes.cpp which is compiled into
    // the same binary.

    // A cleaner approach: We define a static free function wrapper.
    // For now we use the simplest working approach — a thread_local
    // temporary dummy with the barest initialization.

    // Actually the cleanest: just use a standalone helper.
    // Let's build the Monaco def from first principles based on the
    // exact same color data that getBuiltinTheme returns.

    MonacoThemeDef def;
    def.name = monacoThemeName(themeId);

    // Determine base theme
    if (themeId == IDM_THEME_HIGH_CONTRAST) {
        def.base = "hc-black";
    } else if (themeId == IDM_THEME_LIGHT_PLUS || themeId == IDM_THEME_SOLARIZED_LIGHT) {
        def.base = "vs";
    } else {
        def.base = "vs-dark";
    }

    // We'll use a helper that maps theme colors. Since we can't
    // call getBuiltinTheme without a Win32IDE instance, we use
    // the fact that makeDefaultBase() + the switch-case is what
    // getBuiltinTheme does, and the IDETheme is just data.
    //
    // The solution: call getThemeColorsForExport which we add as a
    // static method. But to avoid circular deps, the simplest
    // production approach is to have the WebView2 container call
    // this with an IDETheme reference from the Win32IDE instance.
    //
    // For now, to be self-contained, we embed the color lookup
    // directly into the exporter by theme ID:

    // Helper: add a token rule
    auto addRule = [&def](const char* token, COLORREF fg, const char* style = "") {
        MonacoThemeDef::TokenRule rule;
        rule.token = token;
        rule.foreground = colorrefToHexNoHash(fg);
        rule.fontStyle = style;
        def.rules.push_back(rule);
    };

    // Helper: add an editor color
    auto addColor = [&def](const char* key, COLORREF c) {
        def.colors[key] = colorrefToHex(c);
    };

    // ================================================================
    // Theme-specific color data
    // ================================================================
    // These are the EXACT same colors from Win32IDE_Themes.cpp
    // getBuiltinTheme(), mapped 1:1 into Monaco format.
    // ================================================================

    struct ThemeColors {
        COLORREF bg, fg, keyword, comment, string, number, op, preproc, func, type;
        COLORREF selection, selectionText, lineNum, lineNumBg, currentLine, cursor;
        COLORREF sidebarBg, sidebarFg, activityBg, activityFg, activityIndicator;
        COLORREF tabBarBg, tabActiveBg, tabActiveFg, tabInactiveBg, tabInactiveFg, tabBorder;
        COLORREF statusBg, statusFg, statusAccent;
        COLORREF panelBg, panelFg, panelBorder;
        COLORREF titleBg, titleFg;
        COLORREF scrollBg, scrollThumb;
        COLORREF bracketMatch, indentGuide, accent;
        COLORREF error, warning, info;
    };

    ThemeColors tc{};

    switch (themeId) {
    case IDM_THEME_DARK_PLUS:
        tc = { RGB(30,30,30), RGB(212,212,212), RGB(86,156,214), RGB(106,153,85),
               RGB(206,145,120), RGB(181,206,168), RGB(212,212,212), RGB(155,89,182),
               RGB(220,220,170), RGB(78,201,176),
               RGB(38,79,120), RGB(255,255,255), RGB(133,133,133), RGB(30,30,30),
               RGB(40,40,40), RGB(255,255,255),
               RGB(37,37,38), RGB(204,204,204), RGB(51,51,51), RGB(204,204,204), RGB(0,122,204),
               RGB(37,37,38), RGB(30,30,30), RGB(255,255,255), RGB(45,45,45), RGB(150,150,150), RGB(37,37,38),
               RGB(0,122,204), RGB(255,255,255), RGB(22,130,93),
               RGB(30,30,30), RGB(204,204,204), RGB(48,48,48),
               RGB(37,37,38), RGB(204,204,204),
               RGB(37,37,38), RGB(79,79,79),
               RGB(0,100,160), RGB(64,64,64), RGB(0,122,204),
               RGB(252,127,127), RGB(205,175,0), RGB(75,175,240) };
        break;

    case IDM_THEME_LIGHT_PLUS:
        tc = { RGB(255,255,255), RGB(0,0,0), RGB(0,0,255), RGB(0,128,0),
               RGB(163,21,21), RGB(9,134,88), RGB(0,0,0), RGB(128,0,128),
               RGB(121,94,38), RGB(38,127,153),
               RGB(173,214,255), RGB(0,0,0), RGB(140,140,140), RGB(255,255,255),
               RGB(242,242,242), RGB(0,0,0),
               RGB(243,243,243), RGB(51,51,51), RGB(236,236,236), RGB(102,102,102), RGB(0,122,204),
               RGB(236,236,236), RGB(255,255,255), RGB(51,51,51), RGB(236,236,236), RGB(140,140,140), RGB(222,222,222),
               RGB(0,122,204), RGB(255,255,255), RGB(22,130,93),
               RGB(255,255,255), RGB(51,51,51), RGB(222,222,222),
               RGB(221,221,221), RGB(51,51,51),
               RGB(243,243,243), RGB(190,190,190),
               RGB(200,225,255), RGB(210,210,210), RGB(0,122,204),
               RGB(211,47,47), RGB(191,143,0), RGB(21,101,192) };
        break;

    case IDM_THEME_MONOKAI:
        tc = { RGB(39,40,34), RGB(248,248,242), RGB(249,38,114), RGB(117,113,94),
               RGB(230,219,116), RGB(174,129,255), RGB(249,38,114), RGB(249,38,114),
               RGB(166,226,46), RGB(102,217,239),
               RGB(73,72,62), RGB(248,248,242), RGB(144,144,138), RGB(39,40,34),
               RGB(60,61,55), RGB(248,248,240),
               RGB(33,34,28), RGB(210,210,200), RGB(33,34,28), RGB(200,200,190), RGB(166,226,46),
               RGB(33,34,28), RGB(39,40,34), RGB(248,248,242), RGB(30,31,25), RGB(130,130,125), RGB(39,40,34),
               RGB(33,34,28), RGB(166,226,46), RGB(249,38,114),
               RGB(39,40,34), RGB(248,248,242), RGB(55,56,48),
               RGB(33,34,28), RGB(200,200,190),
               RGB(39,40,34), RGB(80,81,75),
               RGB(73,72,62), RGB(64,65,58), RGB(166,226,46),
               RGB(249,38,114), RGB(230,219,116), RGB(102,217,239) };
        break;

    case IDM_THEME_DRACULA:
        tc = { RGB(40,42,54), RGB(248,248,242), RGB(255,121,198), RGB(98,114,164),
               RGB(241,250,140), RGB(189,147,249), RGB(255,121,198), RGB(255,121,198),
               RGB(80,250,123), RGB(139,233,253),
               RGB(68,71,90), RGB(248,248,242), RGB(98,114,164), RGB(40,42,54),
               RGB(68,71,90), RGB(248,248,242),
               RGB(33,34,44), RGB(248,248,242), RGB(33,34,44), RGB(248,248,242), RGB(189,147,249),
               RGB(33,34,44), RGB(40,42,54), RGB(248,248,242), RGB(33,34,44), RGB(98,114,164), RGB(40,42,54),
               RGB(189,147,249), RGB(40,42,54), RGB(80,250,123),
               RGB(40,42,54), RGB(248,248,242), RGB(68,71,90),
               RGB(33,34,44), RGB(248,248,242),
               RGB(40,42,54), RGB(68,71,90),
               RGB(68,71,90), RGB(68,71,90), RGB(189,147,249),
               RGB(255,85,85), RGB(241,250,140), RGB(139,233,253) };
        break;

    case IDM_THEME_NORD:
        tc = { RGB(46,52,64), RGB(216,222,233), RGB(129,161,193), RGB(76,86,106),
               RGB(163,190,140), RGB(180,142,173), RGB(129,161,193), RGB(180,142,173),
               RGB(136,192,208), RGB(143,188,187),
               RGB(67,76,94), RGB(216,222,233), RGB(76,86,106), RGB(46,52,64),
               RGB(59,66,82), RGB(216,222,233),
               RGB(46,52,64), RGB(216,222,233), RGB(46,52,64), RGB(216,222,233), RGB(136,192,208),
               RGB(46,52,64), RGB(59,66,82), RGB(236,239,244), RGB(46,52,64), RGB(76,86,106), RGB(59,66,82),
               RGB(59,66,82), RGB(216,222,233), RGB(163,190,140),
               RGB(46,52,64), RGB(216,222,233), RGB(59,66,82),
               RGB(46,52,64), RGB(216,222,233),
               RGB(46,52,64), RGB(67,76,94),
               RGB(59,66,82), RGB(59,66,82), RGB(136,192,208),
               RGB(191,97,106), RGB(235,203,139), RGB(136,192,208) };
        break;

    case IDM_THEME_SOLARIZED_DARK:
        tc = { RGB(0,43,54), RGB(131,148,150), RGB(133,153,0), RGB(88,110,117),
               RGB(42,161,152), RGB(211,54,130), RGB(131,148,150), RGB(203,75,22),
               RGB(38,139,210), RGB(181,137,0),
               RGB(7,54,66), RGB(147,161,161), RGB(88,110,117), RGB(0,43,54),
               RGB(7,54,66), RGB(147,161,161),
               RGB(0,43,54), RGB(131,148,150), RGB(0,43,54), RGB(131,148,150), RGB(38,139,210),
               RGB(0,43,54), RGB(7,54,66), RGB(147,161,161), RGB(0,43,54), RGB(88,110,117), RGB(7,54,66),
               RGB(7,54,66), RGB(131,148,150), RGB(38,139,210),
               RGB(0,43,54), RGB(131,148,150), RGB(7,54,66),
               RGB(0,43,54), RGB(131,148,150),
               RGB(0,43,54), RGB(7,54,66),
               RGB(7,54,66), RGB(7,54,66), RGB(38,139,210),
               RGB(220,50,47), RGB(181,137,0), RGB(38,139,210) };
        break;

    case IDM_THEME_SOLARIZED_LIGHT:
        tc = { RGB(253,246,227), RGB(101,123,131), RGB(133,153,0), RGB(147,161,161),
               RGB(42,161,152), RGB(211,54,130), RGB(101,123,131), RGB(203,75,22),
               RGB(38,139,210), RGB(181,137,0),
               RGB(238,232,213), RGB(88,110,117), RGB(147,161,161), RGB(253,246,227),
               RGB(238,232,213), RGB(88,110,117),
               RGB(238,232,213), RGB(101,123,131), RGB(238,232,213), RGB(101,123,131), RGB(38,139,210),
               RGB(238,232,213), RGB(253,246,227), RGB(88,110,117), RGB(238,232,213), RGB(147,161,161), RGB(222,216,198),
               RGB(238,232,213), RGB(101,123,131), RGB(38,139,210),
               RGB(253,246,227), RGB(101,123,131), RGB(222,216,198),
               RGB(238,232,213), RGB(101,123,131),
               RGB(238,232,213), RGB(222,216,198),
               RGB(238,232,213), RGB(222,216,198), RGB(38,139,210),
               RGB(220,50,47), RGB(181,137,0), RGB(38,139,210) };
        break;

    case IDM_THEME_CYBERPUNK_NEON:
        tc = { RGB(10,10,18), RGB(230,230,255), RGB(255,0,230), RGB(80,80,120),
               RGB(0,255,200), RGB(255,200,0), RGB(255,0,128), RGB(255,100,0),
               RGB(0,200,255), RGB(180,0,255),
               RGB(60,0,100), RGB(0,255,255), RGB(100,0,200), RGB(10,10,18),
               RGB(20,10,40), RGB(0,255,200),
               RGB(8,8,14), RGB(200,200,255), RGB(5,5,10), RGB(255,0,230), RGB(0,255,200),
               RGB(8,8,14), RGB(10,10,18), RGB(0,255,255), RGB(5,5,10), RGB(80,80,120), RGB(60,0,100),
               RGB(255,0,128), RGB(255,255,255), RGB(0,255,200),
               RGB(10,10,18), RGB(200,200,255), RGB(60,0,100),
               RGB(5,5,10), RGB(0,255,255),
               RGB(10,10,18), RGB(60,0,100),
               RGB(60,0,100), RGB(30,10,50), RGB(0,255,200),
               RGB(255,50,50), RGB(255,200,0), RGB(0,200,255) };
        break;

    case IDM_THEME_GRUVBOX_DARK:
        tc = { RGB(40,40,40), RGB(235,219,178), RGB(251,73,52), RGB(146,131,116),
               RGB(184,187,38), RGB(211,134,155), RGB(235,219,178), RGB(142,192,124),
               RGB(250,189,47), RGB(131,165,152),
               RGB(80,73,69), RGB(235,219,178), RGB(124,111,100), RGB(40,40,40),
               RGB(50,48,47), RGB(235,219,178),
               RGB(50,48,47), RGB(213,196,161), RGB(29,32,33), RGB(213,196,161), RGB(250,189,47),
               RGB(29,32,33), RGB(40,40,40), RGB(235,219,178), RGB(29,32,33), RGB(124,111,100), RGB(50,48,47),
               RGB(29,32,33), RGB(213,196,161), RGB(142,192,124),
               RGB(40,40,40), RGB(235,219,178), RGB(60,56,54),
               RGB(29,32,33), RGB(213,196,161),
               RGB(40,40,40), RGB(80,73,69),
               RGB(80,73,69), RGB(60,56,54), RGB(250,189,47),
               RGB(251,73,52), RGB(250,189,47), RGB(131,165,152) };
        break;

    case IDM_THEME_CATPPUCCIN_MOCHA:
        tc = { RGB(30,30,46), RGB(205,214,244), RGB(203,166,247), RGB(108,112,134),
               RGB(166,227,161), RGB(250,179,135), RGB(137,220,235), RGB(243,139,168),
               RGB(137,180,250), RGB(249,226,175),
               RGB(69,71,90), RGB(205,214,244), RGB(108,112,134), RGB(30,30,46),
               RGB(49,50,68), RGB(245,224,220),
               RGB(24,24,37), RGB(205,214,244), RGB(17,17,27), RGB(205,214,244), RGB(203,166,247),
               RGB(24,24,37), RGB(30,30,46), RGB(205,214,244), RGB(24,24,37), RGB(108,112,134), RGB(30,30,46),
               RGB(17,17,27), RGB(205,214,244), RGB(203,166,247),
               RGB(30,30,46), RGB(205,214,244), RGB(49,50,68),
               RGB(17,17,27), RGB(205,214,244),
               RGB(30,30,46), RGB(69,71,90),
               RGB(69,71,90), RGB(49,50,68), RGB(203,166,247),
               RGB(243,139,168), RGB(250,179,135), RGB(137,180,250) };
        break;

    case IDM_THEME_TOKYO_NIGHT:
        tc = { RGB(26,27,38), RGB(169,177,214), RGB(157,124,216), RGB(86,95,137),
               RGB(158,206,106), RGB(255,158,100), RGB(137,221,255), RGB(255,117,127),
               RGB(125,207,255), RGB(42,195,222),
               RGB(51,59,88), RGB(192,202,245), RGB(63,68,98), RGB(26,27,38),
               RGB(36,40,59), RGB(192,202,245),
               RGB(26,27,38), RGB(169,177,214), RGB(26,27,38), RGB(169,177,214), RGB(125,207,255),
               RGB(26,27,38), RGB(36,40,59), RGB(192,202,245), RGB(26,27,38), RGB(63,68,98), RGB(36,40,59),
               RGB(26,27,38), RGB(169,177,214), RGB(125,207,255),
               RGB(26,27,38), RGB(169,177,214), RGB(36,40,59),
               RGB(26,27,38), RGB(169,177,214),
               RGB(26,27,38), RGB(51,59,88),
               RGB(51,59,88), RGB(36,40,59), RGB(125,207,255),
               RGB(255,117,127), RGB(224,175,104), RGB(125,207,255) };
        break;

    case IDM_THEME_RAWRXD_CRIMSON:
        tc = { RGB(15,12,14), RGB(225,210,215), RGB(220,40,60), RGB(100,70,80),
               RGB(255,140,100), RGB(255,180,80), RGB(200,60,80), RGB(255,80,120),
               RGB(255,100,60), RGB(200,100,150),
               RGB(80,20,35), RGB(255,220,220), RGB(120,60,70), RGB(15,12,14),
               RGB(30,18,22), RGB(220,40,60),
               RGB(12,10,12), RGB(200,180,190), RGB(8,6,8), RGB(220,40,60), RGB(255,40,60),
               RGB(12,10,12), RGB(15,12,14), RGB(255,100,80), RGB(8,6,8), RGB(100,70,80), RGB(50,20,30),
               RGB(180,20,40), RGB(255,255,255), RGB(255,80,40),
               RGB(15,12,14), RGB(200,180,190), RGB(50,20,30),
               RGB(8,6,8), RGB(220,40,60),
               RGB(15,12,14), RGB(80,20,35),
               RGB(80,20,35), RGB(35,18,25), RGB(220,40,60),
               RGB(255,80,80), RGB(255,180,80), RGB(200,100,150) };
        break;

    case IDM_THEME_HIGH_CONTRAST:
        tc = { RGB(0,0,0), RGB(255,255,255), RGB(86,156,255), RGB(124,166,104),
               RGB(255,163,104), RGB(200,255,200), RGB(255,255,255), RGB(200,128,255),
               RGB(255,255,128), RGB(128,255,255),
               RGB(38,79,120), RGB(255,255,255), RGB(255,255,255), RGB(0,0,0),
               RGB(20,20,20), RGB(255,255,255),
               RGB(0,0,0), RGB(255,255,255), RGB(0,0,0), RGB(255,255,255), RGB(255,255,255),
               RGB(0,0,0), RGB(0,0,0), RGB(255,255,255), RGB(0,0,0), RGB(170,170,170), RGB(255,255,255),
               RGB(0,0,0), RGB(255,255,255), RGB(255,255,255),
               RGB(0,0,0), RGB(255,255,255), RGB(255,255,255),
               RGB(0,0,0), RGB(255,255,255),
               RGB(0,0,0), RGB(100,100,100),
               RGB(38,79,120), RGB(50,50,50), RGB(255,255,255),
               RGB(255,0,0), RGB(255,255,0), RGB(0,200,255) };
        break;

    case IDM_THEME_ONE_DARK_PRO:
        tc = { RGB(40,44,52), RGB(171,178,191), RGB(198,120,221), RGB(92,99,112),
               RGB(152,195,121), RGB(209,154,102), RGB(86,182,194), RGB(198,120,221),
               RGB(97,175,239), RGB(229,192,123),
               RGB(62,68,81), RGB(171,178,191), RGB(76,82,99), RGB(40,44,52),
               RGB(44,49,58), RGB(97,175,239),
               RGB(33,37,43), RGB(171,178,191), RGB(33,37,43), RGB(171,178,191), RGB(97,175,239),
               RGB(33,37,43), RGB(40,44,52), RGB(171,178,191), RGB(33,37,43), RGB(76,82,99), RGB(40,44,52),
               RGB(33,37,43), RGB(171,178,191), RGB(97,175,239),
               RGB(40,44,52), RGB(171,178,191), RGB(52,58,70),
               RGB(33,37,43), RGB(171,178,191),
               RGB(40,44,52), RGB(62,68,81),
               RGB(62,68,81), RGB(52,58,70), RGB(97,175,239),
               RGB(224,108,117), RGB(229,192,123), RGB(97,175,239) };
        break;

    case IDM_THEME_SYNTHWAVE84:
        tc = { RGB(38,20,71), RGB(255,255,255), RGB(255,122,168), RGB(100,80,140),
               RGB(255,241,109), RGB(249,155,46), RGB(255,122,168), RGB(255,155,200),
               RGB(54,247,205), RGB(254,78,236),
               RGB(65,40,120), RGB(255,255,255), RGB(90,60,130), RGB(38,20,71),
               RGB(45,25,85), RGB(54,247,205),
               RGB(30,15,58), RGB(220,200,255), RGB(25,10,48), RGB(254,78,236), RGB(54,247,205),
               RGB(30,15,58), RGB(38,20,71), RGB(255,255,255), RGB(25,10,48), RGB(90,60,130), RGB(50,30,90),
               RGB(254,78,236), RGB(255,255,255), RGB(54,247,205),
               RGB(38,20,71), RGB(220,200,255), RGB(60,35,110),
               RGB(25,10,48), RGB(254,78,236),
               RGB(38,20,71), RGB(65,40,120),
               RGB(65,40,120), RGB(50,30,85), RGB(254,78,236),
               RGB(255,80,80), RGB(255,241,109), RGB(54,247,205) };
        break;

    case IDM_THEME_ABYSS:
        tc = { RGB(0,4,28), RGB(111,140,189), RGB(34,93,180), RGB(56,79,112),
               RGB(34,180,122), RGB(248,138,73), RGB(111,140,189), RGB(34,93,180),
               RGB(34,180,180), RGB(220,170,110),
               RGB(0,20,65), RGB(170,200,255), RGB(56,79,112), RGB(0,4,28),
               RGB(0,10,40), RGB(170,200,255),
               RGB(0,4,28), RGB(111,140,189), RGB(0,4,28), RGB(111,140,189), RGB(34,93,180),
               RGB(0,4,28), RGB(0,8,38), RGB(170,200,255), RGB(0,4,28), RGB(56,79,112), RGB(0,12,50),
               RGB(0,8,38), RGB(111,140,189), RGB(34,93,180),
               RGB(0,4,28), RGB(111,140,189), RGB(0,12,50),
               RGB(0,4,28), RGB(111,140,189),
               RGB(0,4,28), RGB(0,20,65),
               RGB(0,20,65), RGB(0,12,50), RGB(34,93,180),
               RGB(248,100,100), RGB(248,200,100), RGB(34,180,180) };
        break;

    default:
        // Fallback to Dark+
        return exportTheme(IDM_THEME_DARK_PLUS);
    }

    // ================================================================
    // Build Token Rules (syntax colors → Monaco token rules)
    // ================================================================
    addRule("",              tc.fg);                          // Default text
    addRule("comment",       tc.comment, "italic");
    addRule("comment.line",  tc.comment, "italic");
    addRule("comment.block", tc.comment, "italic");
    addRule("keyword",       tc.keyword);
    addRule("keyword.control", tc.keyword);
    addRule("keyword.operator", tc.op);
    addRule("string",        tc.string);
    addRule("string.quoted",  tc.string);
    addRule("string.template",tc.string);
    addRule("number",        tc.number);
    addRule("number.float",  tc.number);
    addRule("number.hex",    tc.number);
    addRule("constant.numeric", tc.number);
    addRule("constant.language", tc.keyword);
    addRule("operator",      tc.op);
    addRule("delimiter",     tc.op);
    addRule("delimiter.bracket", tc.op);
    addRule("tag",           tc.keyword);
    addRule("metatag",       tc.preproc);
    addRule("annotation",    tc.preproc);
    addRule("type",          tc.type);
    addRule("type.identifier", tc.type);
    addRule("entity.name.type", tc.type);
    addRule("entity.name.function", tc.func);
    addRule("support.function", tc.func);
    addRule("identifier",    tc.fg);
    addRule("variable",      tc.fg);
    addRule("variable.predefined", tc.type);
    addRule("predefined",    tc.func);

    // Assembly-specific tokens (for Cyberpunk Neon ASM palette)
    if (themeId == IDM_THEME_CYBERPUNK_NEON) {
        addRule("keyword.asm",          RGB(255, 0, 255));       // Mnemonics: magenta
        addRule("variable.register",    RGB(0, 255, 128));       // Registers: neon green
        addRule("constant.numeric.asm", RGB(255, 220, 0));       // Immediates: gold
        addRule("keyword.directive",    RGB(255, 120, 0));       // Directives: orange
    }

    // ================================================================
    // Build Editor Colors (IDETheme chrome → Monaco color keys)
    // ================================================================
    addColor("editor.background",                   tc.bg);
    addColor("editor.foreground",                   tc.fg);
    addColor("editor.selectionBackground",          tc.selection);
    addColor("editor.selectionForeground",          tc.selectionText);
    addColor("editor.lineHighlightBackground",      tc.currentLine);
    addColor("editorCursor.foreground",             tc.cursor);
    addColor("editorLineNumber.foreground",         tc.lineNum);
    addColor("editorLineNumber.activeForeground",   tc.fg);
    addColor("editorGutter.background",             tc.lineNumBg);

    addColor("sideBar.background",                  tc.sidebarBg);
    addColor("sideBar.foreground",                  tc.sidebarFg);
    addColor("sideBarSectionHeader.background",     tc.sidebarBg);
    addColor("activityBar.background",              tc.activityBg);
    addColor("activityBar.foreground",              tc.activityFg);
    addColor("activityBarBadge.background",         tc.activityIndicator);

    addColor("tab.activeBackground",                tc.tabActiveBg);
    addColor("tab.activeForeground",                tc.tabActiveFg);
    addColor("tab.inactiveBackground",              tc.tabInactiveBg);
    addColor("tab.inactiveForeground",              tc.tabInactiveFg);
    addColor("tab.border",                          tc.tabBorder);
    addColor("editorGroupHeader.tabsBackground",    tc.tabBarBg);

    addColor("statusBar.background",                tc.statusBg);
    addColor("statusBar.foreground",                tc.statusFg);
    addColor("statusBar.debuggingBackground",       tc.statusAccent);

    addColor("panel.background",                    tc.panelBg);
    addColor("panel.border",                        tc.panelBorder);
    addColor("panelTitle.activeForeground",         tc.panelFg);

    addColor("titleBar.activeBackground",           tc.titleBg);
    addColor("titleBar.activeForeground",           tc.titleFg);
    addColor("titleBar.inactiveBackground",         tc.titleBg);

    addColor("scrollbar.shadow",                    tc.scrollBg);
    addColor("scrollbarSlider.background",          tc.scrollThumb);
    addColor("scrollbarSlider.hoverBackground",     tc.scrollThumb);
    addColor("scrollbarSlider.activeBackground",    tc.scrollThumb);

    addColor("editorBracketMatch.background",       tc.bracketMatch);
    addColor("editorIndentGuide.background",        tc.indentGuide);
    addColor("focusBorder",                         tc.accent);

    addColor("editorError.foreground",              tc.error);
    addColor("editorWarning.foreground",            tc.warning);
    addColor("editorInfo.foreground",               tc.info);

    // Minimap
    addColor("minimap.background",                  tc.bg);
    addColor("minimapSlider.background",            tc.scrollThumb);

    // Input
    addColor("input.background",                    tc.panelBg);
    addColor("input.foreground",                    tc.panelFg);
    addColor("input.border",                        tc.panelBorder);

    // Dropdown
    addColor("dropdown.background",                 tc.panelBg);
    addColor("dropdown.foreground",                 tc.panelFg);
    addColor("dropdown.border",                     tc.panelBorder);

    return def;
}

// ============================================================================
// Export All 16 Themes
// ============================================================================
std::vector<MonacoThemeDef> MonacoThemeExporter::exportAllThemes() {
    std::vector<MonacoThemeDef> themes;
    themes.reserve(16);

    static const int themeIds[] = {
        IDM_THEME_DARK_PLUS,
        IDM_THEME_LIGHT_PLUS,
        IDM_THEME_MONOKAI,
        IDM_THEME_DRACULA,
        IDM_THEME_NORD,
        IDM_THEME_SOLARIZED_DARK,
        IDM_THEME_SOLARIZED_LIGHT,
        IDM_THEME_CYBERPUNK_NEON,
        IDM_THEME_GRUVBOX_DARK,
        IDM_THEME_CATPPUCCIN_MOCHA,
        IDM_THEME_TOKYO_NIGHT,
        IDM_THEME_RAWRXD_CRIMSON,
        IDM_THEME_HIGH_CONTRAST,
        IDM_THEME_ONE_DARK_PRO,
        IDM_THEME_SYNTHWAVE84,
        IDM_THEME_ABYSS,
    };

    for (int id : themeIds) {
        themes.push_back(exportTheme(id));
    }

    return themes;
}

// ============================================================================
// Generate JavaScript: monaco.editor.defineTheme() for a single theme
// ============================================================================
std::string MonacoThemeExporter::toDefineThemeJs(const MonacoThemeDef& def) {
    std::ostringstream js;

    js << "    monaco.editor.defineTheme('" << def.name << "', {\n";
    js << "        base: '" << def.base << "',\n";
    js << "        inherit: true,\n";

    // Token rules
    js << "        rules: [\n";
    for (size_t i = 0; i < def.rules.size(); i++) {
        js << "            { token: '" << def.rules[i].token << "'";
        js << ", foreground: '" << def.rules[i].foreground << "'";
        if (!def.rules[i].fontStyle.empty()) {
            js << ", fontStyle: '" << def.rules[i].fontStyle << "'";
        }
        js << " }";
        if (i + 1 < def.rules.size()) js << ",";
        js << "\n";
    }
    js << "        ],\n";

    // Editor colors
    js << "        colors: {\n";
    size_t idx = 0;
    for (const auto& [key, value] : def.colors) {
        js << "            '" << key << "': '" << value << "'";
        if (idx + 1 < def.colors.size()) js << ",";
        js << "\n";
        idx++;
    }
    js << "        }\n";

    js << "    });\n";

    return js.str();
}

// ============================================================================
// Generate JavaScript for ALL 16 Themes (concatenated defineTheme calls)
// ============================================================================
std::string MonacoThemeExporter::toAllDefineThemeJs() {
    std::ostringstream js;

    js << "    // ============================================================\n";
    js << "    // RawrXD Theme Registry — 16 themes from Win32IDE bridged to Monaco\n";
    js << "    // Phase 26: WebView2 Integration — auto-generated by MonacoThemeExporter\n";
    js << "    // ============================================================\n\n";

    auto themes = exportAllThemes();
    for (const auto& theme : themes) {
        js << "    // Theme: " << theme.name << " (base: " << theme.base << ")\n";
        js << toDefineThemeJs(theme);
        js << "\n";
    }

    js << "    // ============================================================\n";
    js << "    // All 16 RawrXD themes registered successfully!\n";
    js << "    // Default: rawrxd-cyberpunk-neon (the signature theme)\n";
    js << "    // ============================================================\n";

    return js.str();
}
