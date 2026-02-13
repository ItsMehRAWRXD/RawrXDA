// ============================================================================
// Win32IDE_Themes.cpp — Premium Theme Engine & Window Transparency
// ============================================================================
//
// Full-spectrum IDE theming:  16 built-in themes, deep apply to every
// control surface (editor, sidebar, activity bar, tab bar, status bar,
// output panel, gutter, terminal panes), plus per-window transparency
// via WS_EX_LAYERED + SetLayeredWindowAttributes for multi-monitor
// multitasking.
//
// Design: Zero simplification — every color field in IDETheme is honored.
// Themes derived from the original ASM IDE color tables and augmented
// with VS-Code-caliber palettes.
// ============================================================================

#include "Win32IDE.h"
#include "IDELogger.h"
#include <richedit.h>
#include <commctrl.h>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cmath>

#pragma comment(lib, "dwmapi.lib")
#include <dwmapi.h>

// ============================================================================
// COLOR UTILITY
// ============================================================================

COLORREF Win32IDE::blendColor(COLORREF base, COLORREF overlay, float t) const {
    int r = (int)((1.0f - t) * GetRValue(base) + t * GetRValue(overlay));
    int g = (int)((1.0f - t) * GetGValue(base) + t * GetGValue(overlay));
    int b = (int)((1.0f - t) * GetBValue(base) + t * GetBValue(overlay));
    return RGB(std::clamp(r, 0, 255), std::clamp(g, 0, 255), std::clamp(b, 0, 255));
}

// ============================================================================
// BUILT-IN THEME FACTORY
// ============================================================================

static IDETheme makeDefaultBase() {
    IDETheme t{};
    t.name = "Dark+";
    t.darkMode = true;
    t.fontName = "Cascadia Code";
    t.fontSize = 12;
    t.windowAlpha = 255;
    // Zero-init all COLORREFs already done by {} init
    return t;
}

IDETheme Win32IDE::getBuiltinTheme(int themeId) const {
    IDETheme t = makeDefaultBase();

    switch (themeId) {

    // ========================================================================
    // 1. DARK+ (VS Code default dark)
    // ========================================================================
    case IDM_THEME_DARK_PLUS:
        t.name = "Dark+";
        t.darkMode = true;
        t.backgroundColor      = RGB(30, 30, 30);
        t.textColor             = RGB(212, 212, 212);
        t.keywordColor          = RGB(86, 156, 214);
        t.commentColor          = RGB(106, 153, 85);
        t.stringColor           = RGB(206, 145, 120);
        t.numberColor           = RGB(181, 206, 168);
        t.operatorColor         = RGB(212, 212, 212);
        t.preprocessorColor     = RGB(155, 89, 182);
        t.functionColor         = RGB(220, 220, 170);
        t.typeColor             = RGB(78, 201, 176);
        t.selectionColor        = RGB(38, 79, 120);
        t.selectionTextColor    = RGB(255, 255, 255);
        t.lineNumberColor       = RGB(133, 133, 133);
        t.lineNumberBg          = RGB(30, 30, 30);
        t.currentLineBg         = RGB(40, 40, 40);
        t.cursorColor           = RGB(255, 255, 255);
        t.sidebarBg             = RGB(37, 37, 38);
        t.sidebarFg             = RGB(204, 204, 204);
        t.sidebarHeaderBg       = RGB(37, 37, 38);
        t.activityBarBg         = RGB(51, 51, 51);
        t.activityBarFg         = RGB(204, 204, 204);
        t.activityBarIndicator  = RGB(0, 122, 204);
        t.activityBarHoverBg    = RGB(90, 93, 94);
        t.tabBarBg              = RGB(37, 37, 38);
        t.tabActiveBg           = RGB(30, 30, 30);
        t.tabActiveFg           = RGB(255, 255, 255);
        t.tabInactiveBg         = RGB(45, 45, 45);
        t.tabInactiveFg         = RGB(150, 150, 150);
        t.tabBorder             = RGB(37, 37, 38);
        t.statusBarBg           = RGB(0, 122, 204);
        t.statusBarFg           = RGB(255, 255, 255);
        t.statusBarAccent       = RGB(22, 130, 93);
        t.panelBg               = RGB(30, 30, 30);
        t.panelFg               = RGB(204, 204, 204);
        t.panelBorder           = RGB(48, 48, 48);
        t.panelHeaderBg         = RGB(37, 37, 38);
        t.titleBarBg            = RGB(37, 37, 38);
        t.titleBarFg            = RGB(204, 204, 204);
        t.scrollbarBg           = RGB(37, 37, 38);
        t.scrollbarThumb        = RGB(79, 79, 79);
        t.scrollbarThumbHover   = RGB(110, 110, 110);
        t.bracketMatchBg        = RGB(0, 100, 160);
        t.indentGuideColor      = RGB(64, 64, 64);
        t.accentColor           = RGB(0, 122, 204);
        t.errorColor            = RGB(252, 127, 127);
        t.warningColor          = RGB(205, 175, 0);
        t.infoColor             = RGB(75, 175, 240);
        break;

    // ========================================================================
    // 2. LIGHT+ (VS Code default light)
    // ========================================================================
    case IDM_THEME_LIGHT_PLUS:
        t.name = "Light+";
        t.darkMode = false;
        t.backgroundColor      = RGB(255, 255, 255);
        t.textColor             = RGB(0, 0, 0);
        t.keywordColor          = RGB(0, 0, 255);
        t.commentColor          = RGB(0, 128, 0);
        t.stringColor           = RGB(163, 21, 21);
        t.numberColor           = RGB(9, 134, 88);
        t.operatorColor         = RGB(0, 0, 0);
        t.preprocessorColor     = RGB(128, 0, 128);
        t.functionColor         = RGB(121, 94, 38);
        t.typeColor             = RGB(38, 127, 153);
        t.selectionColor        = RGB(173, 214, 255);
        t.selectionTextColor    = RGB(0, 0, 0);
        t.lineNumberColor       = RGB(140, 140, 140);
        t.lineNumberBg          = RGB(255, 255, 255);
        t.currentLineBg         = RGB(242, 242, 242);
        t.cursorColor           = RGB(0, 0, 0);
        t.sidebarBg             = RGB(243, 243, 243);
        t.sidebarFg             = RGB(51, 51, 51);
        t.sidebarHeaderBg       = RGB(236, 236, 236);
        t.activityBarBg         = RGB(236, 236, 236);
        t.activityBarFg         = RGB(102, 102, 102);
        t.activityBarIndicator  = RGB(0, 122, 204);
        t.activityBarHoverBg    = RGB(215, 215, 215);
        t.tabBarBg              = RGB(236, 236, 236);
        t.tabActiveBg           = RGB(255, 255, 255);
        t.tabActiveFg           = RGB(51, 51, 51);
        t.tabInactiveBg         = RGB(236, 236, 236);
        t.tabInactiveFg         = RGB(140, 140, 140);
        t.tabBorder             = RGB(222, 222, 222);
        t.statusBarBg           = RGB(0, 122, 204);
        t.statusBarFg           = RGB(255, 255, 255);
        t.statusBarAccent       = RGB(22, 130, 93);
        t.panelBg               = RGB(255, 255, 255);
        t.panelFg               = RGB(51, 51, 51);
        t.panelBorder           = RGB(222, 222, 222);
        t.panelHeaderBg         = RGB(236, 236, 236);
        t.titleBarBg            = RGB(221, 221, 221);
        t.titleBarFg            = RGB(51, 51, 51);
        t.scrollbarBg           = RGB(243, 243, 243);
        t.scrollbarThumb        = RGB(190, 190, 190);
        t.scrollbarThumbHover   = RGB(160, 160, 160);
        t.bracketMatchBg        = RGB(200, 225, 255);
        t.indentGuideColor      = RGB(210, 210, 210);
        t.accentColor           = RGB(0, 122, 204);
        t.errorColor            = RGB(211, 47, 47);
        t.warningColor          = RGB(191, 143, 0);
        t.infoColor             = RGB(21, 101, 192);
        break;

    // ========================================================================
    // 3. MONOKAI
    // ========================================================================
    case IDM_THEME_MONOKAI:
        t.name = "Monokai";
        t.darkMode = true;
        t.backgroundColor      = RGB(39, 40, 34);
        t.textColor             = RGB(248, 248, 242);
        t.keywordColor          = RGB(249, 38, 114);
        t.commentColor          = RGB(117, 113, 94);
        t.stringColor           = RGB(230, 219, 116);
        t.numberColor           = RGB(174, 129, 255);
        t.operatorColor         = RGB(249, 38, 114);
        t.preprocessorColor     = RGB(249, 38, 114);
        t.functionColor         = RGB(166, 226, 46);
        t.typeColor             = RGB(102, 217, 239);
        t.selectionColor        = RGB(73, 72, 62);
        t.selectionTextColor    = RGB(248, 248, 242);
        t.lineNumberColor       = RGB(144, 144, 138);
        t.lineNumberBg          = RGB(39, 40, 34);
        t.currentLineBg         = RGB(60, 61, 55);
        t.cursorColor           = RGB(248, 248, 240);
        t.sidebarBg             = RGB(33, 34, 28);
        t.sidebarFg             = RGB(210, 210, 200);
        t.sidebarHeaderBg       = RGB(39, 40, 34);
        t.activityBarBg         = RGB(33, 34, 28);
        t.activityBarFg         = RGB(200, 200, 190);
        t.activityBarIndicator  = RGB(166, 226, 46);
        t.activityBarHoverBg    = RGB(55, 56, 48);
        t.tabBarBg              = RGB(33, 34, 28);
        t.tabActiveBg           = RGB(39, 40, 34);
        t.tabActiveFg           = RGB(248, 248, 242);
        t.tabInactiveBg         = RGB(30, 31, 25);
        t.tabInactiveFg         = RGB(130, 130, 125);
        t.tabBorder             = RGB(39, 40, 34);
        t.statusBarBg           = RGB(33, 34, 28);
        t.statusBarFg           = RGB(166, 226, 46);
        t.statusBarAccent       = RGB(249, 38, 114);
        t.panelBg               = RGB(39, 40, 34);
        t.panelFg               = RGB(248, 248, 242);
        t.panelBorder           = RGB(55, 56, 48);
        t.panelHeaderBg         = RGB(39, 40, 34);
        t.titleBarBg            = RGB(33, 34, 28);
        t.titleBarFg            = RGB(200, 200, 190);
        t.scrollbarBg           = RGB(39, 40, 34);
        t.scrollbarThumb        = RGB(80, 81, 75);
        t.scrollbarThumbHover   = RGB(110, 111, 105);
        t.bracketMatchBg        = RGB(73, 72, 62);
        t.indentGuideColor      = RGB(64, 65, 58);
        t.accentColor           = RGB(166, 226, 46);
        t.errorColor            = RGB(249, 38, 114);
        t.warningColor          = RGB(230, 219, 116);
        t.infoColor             = RGB(102, 217, 239);

        // Per-language palette: Python gets Monokai's iconic decorator/builtin coloring
        {
            IDETheme::LanguageTokenPalette pyPal = {};
            pyPal.keywordColor      = RGB(249, 38, 114);   // Keywords: classic Monokai pink
            pyPal.functionColor     = RGB(166, 226, 46);    // Functions: green
            pyPal.typeColor         = RGB(102, 217, 239);   // Builtins: italic cyan
            pyPal.stringColor       = RGB(230, 219, 116);   // Strings: warm yellow
            pyPal.commentColor      = RGB(117, 113, 94);    // Comments: muted
            pyPal.preprocessorColor = RGB(174, 129, 255);   // Decorators: purple
            t.languagePalettes[(int)Win32IDE::SyntaxLanguage::Python] = pyPal;
        }
        // Per-language palette: JavaScript gets blue function names
        {
            IDETheme::LanguageTokenPalette jsPal = {};
            jsPal.functionColor     = RGB(102, 217, 239);   // Functions: cyan
            jsPal.keywordColor      = RGB(249, 38, 114);    // Keywords: pink
            jsPal.typeColor         = RGB(166, 226, 46);     // Globals: green
            t.languagePalettes[(int)Win32IDE::SyntaxLanguage::JavaScript] = jsPal;
        }
        break;

    // ========================================================================
    // 4. DRACULA
    // ========================================================================
    case IDM_THEME_DRACULA:
        t.name = "Dracula";
        t.darkMode = true;
        t.backgroundColor      = RGB(40, 42, 54);
        t.textColor             = RGB(248, 248, 242);
        t.keywordColor          = RGB(255, 121, 198);
        t.commentColor          = RGB(98, 114, 164);
        t.stringColor           = RGB(241, 250, 140);
        t.numberColor           = RGB(189, 147, 249);
        t.operatorColor         = RGB(255, 121, 198);
        t.preprocessorColor     = RGB(255, 121, 198);
        t.functionColor         = RGB(80, 250, 123);
        t.typeColor             = RGB(139, 233, 253);
        t.selectionColor        = RGB(68, 71, 90);
        t.selectionTextColor    = RGB(248, 248, 242);
        t.lineNumberColor       = RGB(98, 114, 164);
        t.lineNumberBg          = RGB(40, 42, 54);
        t.currentLineBg         = RGB(68, 71, 90);
        t.cursorColor           = RGB(248, 248, 242);
        t.sidebarBg             = RGB(33, 34, 44);
        t.sidebarFg             = RGB(248, 248, 242);
        t.sidebarHeaderBg       = RGB(40, 42, 54);
        t.activityBarBg         = RGB(33, 34, 44);
        t.activityBarFg         = RGB(248, 248, 242);
        t.activityBarIndicator  = RGB(189, 147, 249);
        t.activityBarHoverBg    = RGB(55, 57, 72);
        t.tabBarBg              = RGB(33, 34, 44);
        t.tabActiveBg           = RGB(40, 42, 54);
        t.tabActiveFg           = RGB(248, 248, 242);
        t.tabInactiveBg         = RGB(33, 34, 44);
        t.tabInactiveFg         = RGB(98, 114, 164);
        t.tabBorder             = RGB(40, 42, 54);
        t.statusBarBg           = RGB(189, 147, 249);
        t.statusBarFg           = RGB(40, 42, 54);
        t.statusBarAccent       = RGB(80, 250, 123);
        t.panelBg               = RGB(40, 42, 54);
        t.panelFg               = RGB(248, 248, 242);
        t.panelBorder           = RGB(68, 71, 90);
        t.panelHeaderBg         = RGB(40, 42, 54);
        t.titleBarBg            = RGB(33, 34, 44);
        t.titleBarFg            = RGB(248, 248, 242);
        t.scrollbarBg           = RGB(40, 42, 54);
        t.scrollbarThumb        = RGB(68, 71, 90);
        t.scrollbarThumbHover   = RGB(98, 114, 164);
        t.bracketMatchBg        = RGB(68, 71, 90);
        t.indentGuideColor      = RGB(68, 71, 90);
        t.accentColor           = RGB(189, 147, 249);
        t.errorColor            = RGB(255, 85, 85);
        t.warningColor          = RGB(241, 250, 140);
        t.infoColor             = RGB(139, 233, 253);
        break;

    // ========================================================================
    // 5. NORD
    // ========================================================================
    case IDM_THEME_NORD:
        t.name = "Nord";
        t.darkMode = true;
        t.backgroundColor      = RGB(46, 52, 64);
        t.textColor             = RGB(216, 222, 233);
        t.keywordColor          = RGB(129, 161, 193);
        t.commentColor          = RGB(76, 86, 106);
        t.stringColor           = RGB(163, 190, 140);
        t.numberColor           = RGB(180, 142, 173);
        t.operatorColor         = RGB(129, 161, 193);
        t.preprocessorColor     = RGB(180, 142, 173);
        t.functionColor         = RGB(136, 192, 208);
        t.typeColor             = RGB(143, 188, 187);
        t.selectionColor        = RGB(67, 76, 94);
        t.selectionTextColor    = RGB(216, 222, 233);
        t.lineNumberColor       = RGB(76, 86, 106);
        t.lineNumberBg          = RGB(46, 52, 64);
        t.currentLineBg         = RGB(59, 66, 82);
        t.cursorColor           = RGB(216, 222, 233);
        t.sidebarBg             = RGB(46, 52, 64);
        t.sidebarFg             = RGB(216, 222, 233);
        t.sidebarHeaderBg       = RGB(46, 52, 64);
        t.activityBarBg         = RGB(46, 52, 64);
        t.activityBarFg         = RGB(216, 222, 233);
        t.activityBarIndicator  = RGB(136, 192, 208);
        t.activityBarHoverBg    = RGB(59, 66, 82);
        t.tabBarBg              = RGB(46, 52, 64);
        t.tabActiveBg           = RGB(59, 66, 82);
        t.tabActiveFg           = RGB(236, 239, 244);
        t.tabInactiveBg         = RGB(46, 52, 64);
        t.tabInactiveFg         = RGB(76, 86, 106);
        t.tabBorder             = RGB(59, 66, 82);
        t.statusBarBg           = RGB(59, 66, 82);
        t.statusBarFg           = RGB(216, 222, 233);
        t.statusBarAccent       = RGB(163, 190, 140);
        t.panelBg               = RGB(46, 52, 64);
        t.panelFg               = RGB(216, 222, 233);
        t.panelBorder           = RGB(59, 66, 82);
        t.panelHeaderBg         = RGB(59, 66, 82);
        t.titleBarBg            = RGB(46, 52, 64);
        t.titleBarFg            = RGB(216, 222, 233);
        t.scrollbarBg           = RGB(46, 52, 64);
        t.scrollbarThumb        = RGB(67, 76, 94);
        t.scrollbarThumbHover   = RGB(76, 86, 106);
        t.bracketMatchBg        = RGB(59, 66, 82);
        t.indentGuideColor      = RGB(59, 66, 82);
        t.accentColor           = RGB(136, 192, 208);
        t.errorColor            = RGB(191, 97, 106);
        t.warningColor          = RGB(235, 203, 139);
        t.infoColor             = RGB(136, 192, 208);
        break;

    // ========================================================================
    // 6. SOLARIZED DARK
    // ========================================================================
    case IDM_THEME_SOLARIZED_DARK:
        t.name = "Solarized Dark";
        t.darkMode = true;
        t.backgroundColor      = RGB(0, 43, 54);
        t.textColor             = RGB(131, 148, 150);
        t.keywordColor          = RGB(133, 153, 0);
        t.commentColor          = RGB(88, 110, 117);
        t.stringColor           = RGB(42, 161, 152);
        t.numberColor           = RGB(211, 54, 130);
        t.operatorColor         = RGB(131, 148, 150);
        t.preprocessorColor     = RGB(203, 75, 22);
        t.functionColor         = RGB(38, 139, 210);
        t.typeColor             = RGB(181, 137, 0);
        t.selectionColor        = RGB(7, 54, 66);
        t.selectionTextColor    = RGB(147, 161, 161);
        t.lineNumberColor       = RGB(88, 110, 117);
        t.lineNumberBg          = RGB(0, 43, 54);
        t.currentLineBg         = RGB(7, 54, 66);
        t.cursorColor           = RGB(147, 161, 161);
        t.sidebarBg             = RGB(0, 43, 54);
        t.sidebarFg             = RGB(131, 148, 150);
        t.sidebarHeaderBg       = RGB(7, 54, 66);
        t.activityBarBg         = RGB(0, 43, 54);
        t.activityBarFg         = RGB(131, 148, 150);
        t.activityBarIndicator  = RGB(38, 139, 210);
        t.activityBarHoverBg    = RGB(7, 54, 66);
        t.tabBarBg              = RGB(0, 43, 54);
        t.tabActiveBg           = RGB(7, 54, 66);
        t.tabActiveFg           = RGB(147, 161, 161);
        t.tabInactiveBg         = RGB(0, 43, 54);
        t.tabInactiveFg         = RGB(88, 110, 117);
        t.tabBorder             = RGB(7, 54, 66);
        t.statusBarBg           = RGB(7, 54, 66);
        t.statusBarFg           = RGB(131, 148, 150);
        t.statusBarAccent       = RGB(38, 139, 210);
        t.panelBg               = RGB(0, 43, 54);
        t.panelFg               = RGB(131, 148, 150);
        t.panelBorder           = RGB(7, 54, 66);
        t.panelHeaderBg         = RGB(7, 54, 66);
        t.titleBarBg            = RGB(0, 43, 54);
        t.titleBarFg            = RGB(131, 148, 150);
        t.scrollbarBg           = RGB(0, 43, 54);
        t.scrollbarThumb        = RGB(7, 54, 66);
        t.scrollbarThumbHover   = RGB(88, 110, 117);
        t.bracketMatchBg        = RGB(7, 54, 66);
        t.indentGuideColor      = RGB(7, 54, 66);
        t.accentColor           = RGB(38, 139, 210);
        t.errorColor            = RGB(220, 50, 47);
        t.warningColor          = RGB(181, 137, 0);
        t.infoColor             = RGB(38, 139, 210);
        break;

    // ========================================================================
    // 7. SOLARIZED LIGHT
    // ========================================================================
    case IDM_THEME_SOLARIZED_LIGHT:
        t.name = "Solarized Light";
        t.darkMode = false;
        t.backgroundColor      = RGB(253, 246, 227);
        t.textColor             = RGB(101, 123, 131);
        t.keywordColor          = RGB(133, 153, 0);
        t.commentColor          = RGB(147, 161, 161);
        t.stringColor           = RGB(42, 161, 152);
        t.numberColor           = RGB(211, 54, 130);
        t.operatorColor         = RGB(101, 123, 131);
        t.preprocessorColor     = RGB(203, 75, 22);
        t.functionColor         = RGB(38, 139, 210);
        t.typeColor             = RGB(181, 137, 0);
        t.selectionColor        = RGB(238, 232, 213);
        t.selectionTextColor    = RGB(88, 110, 117);
        t.lineNumberColor       = RGB(147, 161, 161);
        t.lineNumberBg          = RGB(253, 246, 227);
        t.currentLineBg         = RGB(238, 232, 213);
        t.cursorColor           = RGB(88, 110, 117);
        t.sidebarBg             = RGB(238, 232, 213);
        t.sidebarFg             = RGB(101, 123, 131);
        t.sidebarHeaderBg       = RGB(238, 232, 213);
        t.activityBarBg         = RGB(238, 232, 213);
        t.activityBarFg         = RGB(101, 123, 131);
        t.activityBarIndicator  = RGB(38, 139, 210);
        t.activityBarHoverBg    = RGB(253, 246, 227);
        t.tabBarBg              = RGB(238, 232, 213);
        t.tabActiveBg           = RGB(253, 246, 227);
        t.tabActiveFg           = RGB(88, 110, 117);
        t.tabInactiveBg         = RGB(238, 232, 213);
        t.tabInactiveFg         = RGB(147, 161, 161);
        t.tabBorder             = RGB(222, 216, 198);
        t.statusBarBg           = RGB(238, 232, 213);
        t.statusBarFg           = RGB(101, 123, 131);
        t.statusBarAccent       = RGB(38, 139, 210);
        t.panelBg               = RGB(253, 246, 227);
        t.panelFg               = RGB(101, 123, 131);
        t.panelBorder           = RGB(222, 216, 198);
        t.panelHeaderBg         = RGB(238, 232, 213);
        t.titleBarBg            = RGB(238, 232, 213);
        t.titleBarFg            = RGB(101, 123, 131);
        t.scrollbarBg           = RGB(238, 232, 213);
        t.scrollbarThumb        = RGB(222, 216, 198);
        t.scrollbarThumbHover   = RGB(190, 185, 168);
        t.bracketMatchBg        = RGB(238, 232, 213);
        t.indentGuideColor      = RGB(222, 216, 198);
        t.accentColor           = RGB(38, 139, 210);
        t.errorColor            = RGB(220, 50, 47);
        t.warningColor          = RGB(181, 137, 0);
        t.infoColor             = RGB(38, 139, 210);
        break;

    // ========================================================================
    // 8. CYBERPUNK NEON — signature RawrXD aggressive theme
    // ========================================================================
    case IDM_THEME_CYBERPUNK_NEON:
        t.name = "Cyberpunk Neon";
        t.darkMode = true;
        t.backgroundColor      = RGB(10, 10, 18);
        t.textColor             = RGB(230, 230, 255);
        t.keywordColor          = RGB(255, 0, 230);
        t.commentColor          = RGB(80, 80, 120);
        t.stringColor           = RGB(0, 255, 200);
        t.numberColor           = RGB(255, 200, 0);
        t.operatorColor         = RGB(255, 0, 128);
        t.preprocessorColor     = RGB(255, 100, 0);
        t.functionColor         = RGB(0, 200, 255);
        t.typeColor             = RGB(180, 0, 255);
        t.selectionColor        = RGB(60, 0, 100);
        t.selectionTextColor    = RGB(0, 255, 255);
        t.lineNumberColor       = RGB(100, 0, 200);
        t.lineNumberBg          = RGB(10, 10, 18);
        t.currentLineBg         = RGB(20, 10, 40);
        t.cursorColor           = RGB(0, 255, 200);
        t.sidebarBg             = RGB(8, 8, 14);
        t.sidebarFg             = RGB(200, 200, 255);
        t.sidebarHeaderBg       = RGB(15, 10, 30);
        t.activityBarBg         = RGB(5, 5, 10);
        t.activityBarFg         = RGB(255, 0, 230);
        t.activityBarIndicator  = RGB(0, 255, 200);
        t.activityBarHoverBg    = RGB(30, 10, 60);
        t.tabBarBg              = RGB(8, 8, 14);
        t.tabActiveBg           = RGB(10, 10, 18);
        t.tabActiveFg           = RGB(0, 255, 255);
        t.tabInactiveBg         = RGB(5, 5, 10);
        t.tabInactiveFg         = RGB(80, 80, 120);
        t.tabBorder             = RGB(60, 0, 100);
        t.statusBarBg           = RGB(255, 0, 128);
        t.statusBarFg           = RGB(255, 255, 255);
        t.statusBarAccent       = RGB(0, 255, 200);
        t.panelBg               = RGB(10, 10, 18);
        t.panelFg               = RGB(200, 200, 255);
        t.panelBorder           = RGB(60, 0, 100);
        t.panelHeaderBg         = RGB(15, 10, 30);
        t.titleBarBg            = RGB(5, 5, 10);
        t.titleBarFg            = RGB(0, 255, 255);
        t.scrollbarBg           = RGB(10, 10, 18);
        t.scrollbarThumb        = RGB(60, 0, 100);
        t.scrollbarThumbHover   = RGB(100, 0, 180);
        t.bracketMatchBg        = RGB(60, 0, 100);
        t.indentGuideColor      = RGB(30, 10, 50);
        t.accentColor           = RGB(0, 255, 200);
        t.errorColor            = RGB(255, 50, 50);
        t.warningColor          = RGB(255, 200, 0);
        t.infoColor             = RGB(0, 200, 255);

        // Per-language palette: Assembly gets neon-green registers, hot-pink mnemonics
        {
            IDETheme::LanguageTokenPalette asmPal = {};
            asmPal.keywordColor      = RGB(255, 0, 255);   // Mnemonics: magenta
            asmPal.typeColor         = RGB(0, 255, 128);    // Registers: neon green
            asmPal.commentColor      = RGB(60, 60, 100);    // Darker than global
            asmPal.numberColor       = RGB(255, 220, 0);    // Immediates: gold
            asmPal.preprocessorColor = RGB(255, 120, 0);    // Directives: orange
            asmPal.stringColor       = RGB(0, 255, 200);    // String literals: cyan
            t.languagePalettes[(int)Win32IDE::SyntaxLanguage::Assembly] = asmPal;
        }
        break;

    // ========================================================================
    // 9. GRUVBOX DARK
    // ========================================================================
    case IDM_THEME_GRUVBOX_DARK:
        t.name = "Gruvbox Dark";
        t.darkMode = true;
        t.backgroundColor      = RGB(40, 40, 40);
        t.textColor             = RGB(235, 219, 178);
        t.keywordColor          = RGB(251, 73, 52);
        t.commentColor          = RGB(146, 131, 116);
        t.stringColor           = RGB(184, 187, 38);
        t.numberColor           = RGB(211, 134, 155);
        t.operatorColor         = RGB(235, 219, 178);
        t.preprocessorColor     = RGB(142, 192, 124);
        t.functionColor         = RGB(250, 189, 47);
        t.typeColor             = RGB(131, 165, 152);
        t.selectionColor        = RGB(80, 73, 69);
        t.selectionTextColor    = RGB(235, 219, 178);
        t.lineNumberColor       = RGB(124, 111, 100);
        t.lineNumberBg          = RGB(40, 40, 40);
        t.currentLineBg         = RGB(50, 48, 47);
        t.cursorColor           = RGB(235, 219, 178);
        t.sidebarBg             = RGB(50, 48, 47);
        t.sidebarFg             = RGB(213, 196, 161);
        t.sidebarHeaderBg       = RGB(40, 40, 40);
        t.activityBarBg         = RGB(29, 32, 33);
        t.activityBarFg         = RGB(213, 196, 161);
        t.activityBarIndicator  = RGB(250, 189, 47);
        t.activityBarHoverBg    = RGB(60, 56, 54);
        t.tabBarBg              = RGB(29, 32, 33);
        t.tabActiveBg           = RGB(40, 40, 40);
        t.tabActiveFg           = RGB(235, 219, 178);
        t.tabInactiveBg         = RGB(29, 32, 33);
        t.tabInactiveFg         = RGB(124, 111, 100);
        t.tabBorder             = RGB(50, 48, 47);
        t.statusBarBg           = RGB(29, 32, 33);
        t.statusBarFg           = RGB(213, 196, 161);
        t.statusBarAccent       = RGB(142, 192, 124);
        t.panelBg               = RGB(40, 40, 40);
        t.panelFg               = RGB(235, 219, 178);
        t.panelBorder           = RGB(60, 56, 54);
        t.panelHeaderBg         = RGB(50, 48, 47);
        t.titleBarBg            = RGB(29, 32, 33);
        t.titleBarFg            = RGB(213, 196, 161);
        t.scrollbarBg           = RGB(40, 40, 40);
        t.scrollbarThumb        = RGB(80, 73, 69);
        t.scrollbarThumbHover   = RGB(102, 92, 84);
        t.bracketMatchBg        = RGB(80, 73, 69);
        t.indentGuideColor      = RGB(60, 56, 54);
        t.accentColor           = RGB(250, 189, 47);
        t.errorColor            = RGB(251, 73, 52);
        t.warningColor          = RGB(250, 189, 47);
        t.infoColor             = RGB(131, 165, 152);
        break;

    // ========================================================================
    // 10. CATPPUCCIN MOCHA
    // ========================================================================
    case IDM_THEME_CATPPUCCIN_MOCHA:
        t.name = "Catppuccin Mocha";
        t.darkMode = true;
        t.backgroundColor      = RGB(30, 30, 46);
        t.textColor             = RGB(205, 214, 244);
        t.keywordColor          = RGB(203, 166, 247);
        t.commentColor          = RGB(108, 112, 134);
        t.stringColor           = RGB(166, 227, 161);
        t.numberColor           = RGB(250, 179, 135);
        t.operatorColor         = RGB(137, 220, 235);
        t.preprocessorColor     = RGB(243, 139, 168);
        t.functionColor         = RGB(137, 180, 250);
        t.typeColor             = RGB(249, 226, 175);
        t.selectionColor        = RGB(69, 71, 90);
        t.selectionTextColor    = RGB(205, 214, 244);
        t.lineNumberColor       = RGB(108, 112, 134);
        t.lineNumberBg          = RGB(30, 30, 46);
        t.currentLineBg         = RGB(49, 50, 68);
        t.cursorColor           = RGB(245, 224, 220);
        t.sidebarBg             = RGB(24, 24, 37);
        t.sidebarFg             = RGB(205, 214, 244);
        t.sidebarHeaderBg       = RGB(30, 30, 46);
        t.activityBarBg         = RGB(17, 17, 27);
        t.activityBarFg         = RGB(205, 214, 244);
        t.activityBarIndicator  = RGB(203, 166, 247);
        t.activityBarHoverBg    = RGB(49, 50, 68);
        t.tabBarBg              = RGB(24, 24, 37);
        t.tabActiveBg           = RGB(30, 30, 46);
        t.tabActiveFg           = RGB(205, 214, 244);
        t.tabInactiveBg         = RGB(24, 24, 37);
        t.tabInactiveFg         = RGB(108, 112, 134);
        t.tabBorder             = RGB(30, 30, 46);
        t.statusBarBg           = RGB(17, 17, 27);
        t.statusBarFg           = RGB(205, 214, 244);
        t.statusBarAccent       = RGB(203, 166, 247);
        t.panelBg               = RGB(30, 30, 46);
        t.panelFg               = RGB(205, 214, 244);
        t.panelBorder           = RGB(49, 50, 68);
        t.panelHeaderBg         = RGB(30, 30, 46);
        t.titleBarBg            = RGB(17, 17, 27);
        t.titleBarFg            = RGB(205, 214, 244);
        t.scrollbarBg           = RGB(30, 30, 46);
        t.scrollbarThumb        = RGB(69, 71, 90);
        t.scrollbarThumbHover   = RGB(88, 91, 112);
        t.bracketMatchBg        = RGB(69, 71, 90);
        t.indentGuideColor      = RGB(49, 50, 68);
        t.accentColor           = RGB(203, 166, 247);
        t.errorColor            = RGB(243, 139, 168);
        t.warningColor          = RGB(250, 179, 135);
        t.infoColor             = RGB(137, 180, 250);
        break;

    // ========================================================================
    // 11. TOKYO NIGHT
    // ========================================================================
    case IDM_THEME_TOKYO_NIGHT:
        t.name = "Tokyo Night";
        t.darkMode = true;
        t.backgroundColor      = RGB(26, 27, 38);
        t.textColor             = RGB(169, 177, 214);
        t.keywordColor          = RGB(157, 124, 216);
        t.commentColor          = RGB(86, 95, 137);
        t.stringColor           = RGB(158, 206, 106);
        t.numberColor           = RGB(255, 158, 100);
        t.operatorColor         = RGB(137, 221, 255);
        t.preprocessorColor     = RGB(255, 117, 127);
        t.functionColor         = RGB(125, 207, 255);
        t.typeColor             = RGB(42, 195, 222);
        t.selectionColor        = RGB(51, 59, 88);
        t.selectionTextColor    = RGB(192, 202, 245);
        t.lineNumberColor       = RGB(63, 68, 98);
        t.lineNumberBg          = RGB(26, 27, 38);
        t.currentLineBg         = RGB(36, 40, 59);
        t.cursorColor           = RGB(192, 202, 245);
        t.sidebarBg             = RGB(26, 27, 38);
        t.sidebarFg             = RGB(169, 177, 214);
        t.sidebarHeaderBg       = RGB(26, 27, 38);
        t.activityBarBg         = RGB(26, 27, 38);
        t.activityBarFg         = RGB(169, 177, 214);
        t.activityBarIndicator  = RGB(125, 207, 255);
        t.activityBarHoverBg    = RGB(36, 40, 59);
        t.tabBarBg              = RGB(26, 27, 38);
        t.tabActiveBg           = RGB(36, 40, 59);
        t.tabActiveFg           = RGB(192, 202, 245);
        t.tabInactiveBg         = RGB(26, 27, 38);
        t.tabInactiveFg         = RGB(63, 68, 98);
        t.tabBorder             = RGB(36, 40, 59);
        t.statusBarBg           = RGB(26, 27, 38);
        t.statusBarFg           = RGB(169, 177, 214);
        t.statusBarAccent       = RGB(125, 207, 255);
        t.panelBg               = RGB(26, 27, 38);
        t.panelFg               = RGB(169, 177, 214);
        t.panelBorder           = RGB(36, 40, 59);
        t.panelHeaderBg         = RGB(36, 40, 59);
        t.titleBarBg            = RGB(26, 27, 38);
        t.titleBarFg            = RGB(169, 177, 214);
        t.scrollbarBg           = RGB(26, 27, 38);
        t.scrollbarThumb        = RGB(51, 59, 88);
        t.scrollbarThumbHover   = RGB(63, 68, 98);
        t.bracketMatchBg        = RGB(51, 59, 88);
        t.indentGuideColor      = RGB(36, 40, 59);
        t.accentColor           = RGB(125, 207, 255);
        t.errorColor            = RGB(255, 117, 127);
        t.warningColor          = RGB(224, 175, 104);
        t.infoColor             = RGB(125, 207, 255);
        break;

    // ========================================================================
    // 12. RAWRXD CRIMSON — The signature house theme
    // ========================================================================
    case IDM_THEME_RAWRXD_CRIMSON:
        t.name = "RawrXD Crimson";
        t.darkMode = true;
        t.backgroundColor      = RGB(15, 12, 14);
        t.textColor             = RGB(225, 210, 215);
        t.keywordColor          = RGB(220, 40, 60);
        t.commentColor          = RGB(100, 70, 80);
        t.stringColor           = RGB(255, 140, 100);
        t.numberColor           = RGB(255, 180, 80);
        t.operatorColor         = RGB(200, 60, 80);
        t.preprocessorColor     = RGB(255, 80, 120);
        t.functionColor         = RGB(255, 100, 60);
        t.typeColor             = RGB(200, 100, 150);
        t.selectionColor        = RGB(80, 20, 35);
        t.selectionTextColor    = RGB(255, 220, 220);
        t.lineNumberColor       = RGB(120, 60, 70);
        t.lineNumberBg          = RGB(15, 12, 14);
        t.currentLineBg         = RGB(30, 18, 22);
        t.cursorColor           = RGB(220, 40, 60);
        t.sidebarBg             = RGB(12, 10, 12);
        t.sidebarFg             = RGB(200, 180, 190);
        t.sidebarHeaderBg       = RGB(18, 14, 16);
        t.activityBarBg         = RGB(8, 6, 8);
        t.activityBarFg         = RGB(220, 40, 60);
        t.activityBarIndicator  = RGB(255, 40, 60);
        t.activityBarHoverBg    = RGB(40, 15, 25);
        t.tabBarBg              = RGB(12, 10, 12);
        t.tabActiveBg           = RGB(15, 12, 14);
        t.tabActiveFg           = RGB(255, 100, 80);
        t.tabInactiveBg         = RGB(8, 6, 8);
        t.tabInactiveFg         = RGB(100, 70, 80);
        t.tabBorder             = RGB(50, 20, 30);
        t.statusBarBg           = RGB(180, 20, 40);
        t.statusBarFg           = RGB(255, 255, 255);
        t.statusBarAccent       = RGB(255, 80, 40);
        t.panelBg               = RGB(15, 12, 14);
        t.panelFg               = RGB(200, 180, 190);
        t.panelBorder           = RGB(50, 20, 30);
        t.panelHeaderBg         = RGB(18, 14, 16);
        t.titleBarBg            = RGB(8, 6, 8);
        t.titleBarFg            = RGB(220, 40, 60);
        t.scrollbarBg           = RGB(15, 12, 14);
        t.scrollbarThumb        = RGB(80, 20, 35);
        t.scrollbarThumbHover   = RGB(120, 30, 50);
        t.bracketMatchBg        = RGB(80, 20, 35);
        t.indentGuideColor      = RGB(35, 18, 25);
        t.accentColor           = RGB(220, 40, 60);
        t.errorColor            = RGB(255, 80, 80);
        t.warningColor          = RGB(255, 180, 80);
        t.infoColor             = RGB(200, 100, 150);
        break;

    // ========================================================================
    // 13. HIGH CONTRAST
    // ========================================================================
    case IDM_THEME_HIGH_CONTRAST:
        t.name = "High Contrast";
        t.darkMode = true;
        t.backgroundColor      = RGB(0, 0, 0);
        t.textColor             = RGB(255, 255, 255);
        t.keywordColor          = RGB(86, 156, 255);
        t.commentColor          = RGB(124, 166, 104);
        t.stringColor           = RGB(255, 163, 104);
        t.numberColor           = RGB(200, 255, 200);
        t.operatorColor         = RGB(255, 255, 255);
        t.preprocessorColor     = RGB(200, 128, 255);
        t.functionColor         = RGB(255, 255, 128);
        t.typeColor             = RGB(128, 255, 255);
        t.selectionColor        = RGB(38, 79, 120);
        t.selectionTextColor    = RGB(255, 255, 255);
        t.lineNumberColor       = RGB(255, 255, 255);
        t.lineNumberBg          = RGB(0, 0, 0);
        t.currentLineBg         = RGB(20, 20, 20);
        t.cursorColor           = RGB(255, 255, 255);
        t.sidebarBg             = RGB(0, 0, 0);
        t.sidebarFg             = RGB(255, 255, 255);
        t.sidebarHeaderBg       = RGB(0, 0, 0);
        t.activityBarBg         = RGB(0, 0, 0);
        t.activityBarFg         = RGB(255, 255, 255);
        t.activityBarIndicator  = RGB(255, 255, 255);
        t.activityBarHoverBg    = RGB(30, 30, 30);
        t.tabBarBg              = RGB(0, 0, 0);
        t.tabActiveBg           = RGB(0, 0, 0);
        t.tabActiveFg           = RGB(255, 255, 255);
        t.tabInactiveBg         = RGB(0, 0, 0);
        t.tabInactiveFg         = RGB(170, 170, 170);
        t.tabBorder             = RGB(255, 255, 255);
        t.statusBarBg           = RGB(0, 0, 0);
        t.statusBarFg           = RGB(255, 255, 255);
        t.statusBarAccent       = RGB(255, 255, 255);
        t.panelBg               = RGB(0, 0, 0);
        t.panelFg               = RGB(255, 255, 255);
        t.panelBorder           = RGB(255, 255, 255);
        t.panelHeaderBg         = RGB(0, 0, 0);
        t.titleBarBg            = RGB(0, 0, 0);
        t.titleBarFg            = RGB(255, 255, 255);
        t.scrollbarBg           = RGB(0, 0, 0);
        t.scrollbarThumb        = RGB(100, 100, 100);
        t.scrollbarThumbHover   = RGB(160, 160, 160);
        t.bracketMatchBg        = RGB(38, 79, 120);
        t.indentGuideColor      = RGB(50, 50, 50);
        t.accentColor           = RGB(255, 255, 255);
        t.errorColor            = RGB(255, 0, 0);
        t.warningColor          = RGB(255, 255, 0);
        t.infoColor             = RGB(0, 200, 255);
        break;

    // ========================================================================
    // 14. ONE DARK PRO
    // ========================================================================
    case IDM_THEME_ONE_DARK_PRO:
        t.name = "One Dark Pro";
        t.darkMode = true;
        t.backgroundColor      = RGB(40, 44, 52);
        t.textColor             = RGB(171, 178, 191);
        t.keywordColor          = RGB(198, 120, 221);
        t.commentColor          = RGB(92, 99, 112);
        t.stringColor           = RGB(152, 195, 121);
        t.numberColor           = RGB(209, 154, 102);
        t.operatorColor         = RGB(86, 182, 194);
        t.preprocessorColor     = RGB(198, 120, 221);
        t.functionColor         = RGB(97, 175, 239);
        t.typeColor             = RGB(229, 192, 123);
        t.selectionColor        = RGB(62, 68, 81);
        t.selectionTextColor    = RGB(171, 178, 191);
        t.lineNumberColor       = RGB(76, 82, 99);
        t.lineNumberBg          = RGB(40, 44, 52);
        t.currentLineBg         = RGB(44, 49, 58);
        t.cursorColor           = RGB(97, 175, 239);
        t.sidebarBg             = RGB(33, 37, 43);
        t.sidebarFg             = RGB(171, 178, 191);
        t.sidebarHeaderBg       = RGB(40, 44, 52);
        t.activityBarBg         = RGB(33, 37, 43);
        t.activityBarFg         = RGB(171, 178, 191);
        t.activityBarIndicator  = RGB(97, 175, 239);
        t.activityBarHoverBg    = RGB(50, 55, 65);
        t.tabBarBg              = RGB(33, 37, 43);
        t.tabActiveBg           = RGB(40, 44, 52);
        t.tabActiveFg           = RGB(171, 178, 191);
        t.tabInactiveBg         = RGB(33, 37, 43);
        t.tabInactiveFg         = RGB(76, 82, 99);
        t.tabBorder             = RGB(40, 44, 52);
        t.statusBarBg           = RGB(33, 37, 43);
        t.statusBarFg           = RGB(171, 178, 191);
        t.statusBarAccent       = RGB(97, 175, 239);
        t.panelBg               = RGB(40, 44, 52);
        t.panelFg               = RGB(171, 178, 191);
        t.panelBorder           = RGB(52, 58, 70);
        t.panelHeaderBg         = RGB(40, 44, 52);
        t.titleBarBg            = RGB(33, 37, 43);
        t.titleBarFg            = RGB(171, 178, 191);
        t.scrollbarBg           = RGB(40, 44, 52);
        t.scrollbarThumb        = RGB(62, 68, 81);
        t.scrollbarThumbHover   = RGB(76, 82, 99);
        t.bracketMatchBg        = RGB(62, 68, 81);
        t.indentGuideColor      = RGB(52, 58, 70);
        t.accentColor           = RGB(97, 175, 239);
        t.errorColor            = RGB(224, 108, 117);
        t.warningColor          = RGB(229, 192, 123);
        t.infoColor             = RGB(97, 175, 239);
        break;

    // ========================================================================
    // 15. SYNTHWAVE '84
    // ========================================================================
    case IDM_THEME_SYNTHWAVE84:
        t.name = "SynthWave '84";
        t.darkMode = true;
        t.backgroundColor      = RGB(38, 20, 71);
        t.textColor             = RGB(255, 255, 255);
        t.keywordColor          = RGB(255, 122, 168);
        t.commentColor          = RGB(100, 80, 140);
        t.stringColor           = RGB(255, 241, 109);
        t.numberColor           = RGB(249, 155, 46);
        t.operatorColor         = RGB(255, 122, 168);
        t.preprocessorColor     = RGB(255, 155, 200);
        t.functionColor         = RGB(54, 247, 205);
        t.typeColor             = RGB(254, 78, 236);
        t.selectionColor        = RGB(65, 40, 120);
        t.selectionTextColor    = RGB(255, 255, 255);
        t.lineNumberColor       = RGB(90, 60, 130);
        t.lineNumberBg          = RGB(38, 20, 71);
        t.currentLineBg         = RGB(45, 25, 85);
        t.cursorColor           = RGB(54, 247, 205);
        t.sidebarBg             = RGB(30, 15, 58);
        t.sidebarFg             = RGB(220, 200, 255);
        t.sidebarHeaderBg       = RGB(38, 20, 71);
        t.activityBarBg         = RGB(25, 10, 48);
        t.activityBarFg         = RGB(254, 78, 236);
        t.activityBarIndicator  = RGB(54, 247, 205);
        t.activityBarHoverBg    = RGB(50, 30, 90);
        t.tabBarBg              = RGB(30, 15, 58);
        t.tabActiveBg           = RGB(38, 20, 71);
        t.tabActiveFg           = RGB(255, 255, 255);
        t.tabInactiveBg         = RGB(25, 10, 48);
        t.tabInactiveFg         = RGB(90, 60, 130);
        t.tabBorder             = RGB(50, 30, 90);
        t.statusBarBg           = RGB(254, 78, 236);
        t.statusBarFg           = RGB(255, 255, 255);
        t.statusBarAccent       = RGB(54, 247, 205);
        t.panelBg               = RGB(38, 20, 71);
        t.panelFg               = RGB(220, 200, 255);
        t.panelBorder           = RGB(60, 35, 110);
        t.panelHeaderBg         = RGB(38, 20, 71);
        t.titleBarBg            = RGB(25, 10, 48);
        t.titleBarFg            = RGB(254, 78, 236);
        t.scrollbarBg           = RGB(38, 20, 71);
        t.scrollbarThumb        = RGB(65, 40, 120);
        t.scrollbarThumbHover   = RGB(90, 60, 150);
        t.bracketMatchBg        = RGB(65, 40, 120);
        t.indentGuideColor      = RGB(50, 30, 85);
        t.accentColor           = RGB(254, 78, 236);
        t.errorColor            = RGB(255, 80, 80);
        t.warningColor          = RGB(255, 241, 109);
        t.infoColor             = RGB(54, 247, 205);
        break;

    // ========================================================================
    // 16. ABYSS
    // ========================================================================
    case IDM_THEME_ABYSS:
        t.name = "Abyss";
        t.darkMode = true;
        t.backgroundColor      = RGB(0, 4, 28);
        t.textColor             = RGB(111, 140, 189);
        t.keywordColor          = RGB(34, 93, 180);
        t.commentColor          = RGB(56, 79, 112);
        t.stringColor           = RGB(34, 180, 122);
        t.numberColor           = RGB(248, 138, 73);
        t.operatorColor         = RGB(111, 140, 189);
        t.preprocessorColor     = RGB(34, 93, 180);
        t.functionColor         = RGB(34, 180, 180);
        t.typeColor             = RGB(220, 170, 110);
        t.selectionColor        = RGB(0, 20, 65);
        t.selectionTextColor    = RGB(170, 200, 255);
        t.lineNumberColor       = RGB(56, 79, 112);
        t.lineNumberBg          = RGB(0, 4, 28);
        t.currentLineBg         = RGB(0, 10, 40);
        t.cursorColor           = RGB(170, 200, 255);
        t.sidebarBg             = RGB(0, 4, 28);
        t.sidebarFg             = RGB(111, 140, 189);
        t.sidebarHeaderBg       = RGB(0, 8, 38);
        t.activityBarBg         = RGB(0, 4, 28);
        t.activityBarFg         = RGB(111, 140, 189);
        t.activityBarIndicator  = RGB(34, 93, 180);
        t.activityBarHoverBg    = RGB(0, 12, 50);
        t.tabBarBg              = RGB(0, 4, 28);
        t.tabActiveBg           = RGB(0, 8, 38);
        t.tabActiveFg           = RGB(170, 200, 255);
        t.tabInactiveBg         = RGB(0, 4, 28);
        t.tabInactiveFg         = RGB(56, 79, 112);
        t.tabBorder             = RGB(0, 12, 50);
        t.statusBarBg           = RGB(0, 8, 38);
        t.statusBarFg           = RGB(111, 140, 189);
        t.statusBarAccent       = RGB(34, 93, 180);
        t.panelBg               = RGB(0, 4, 28);
        t.panelFg               = RGB(111, 140, 189);
        t.panelBorder           = RGB(0, 12, 50);
        t.panelHeaderBg         = RGB(0, 8, 38);
        t.titleBarBg            = RGB(0, 4, 28);
        t.titleBarFg            = RGB(111, 140, 189);
        t.scrollbarBg           = RGB(0, 4, 28);
        t.scrollbarThumb        = RGB(0, 20, 65);
        t.scrollbarThumbHover   = RGB(34, 55, 100);
        t.bracketMatchBg        = RGB(0, 20, 65);
        t.indentGuideColor      = RGB(0, 12, 50);
        t.accentColor           = RGB(34, 93, 180);
        t.errorColor            = RGB(248, 100, 100);
        t.warningColor          = RGB(248, 200, 100);
        t.infoColor             = RGB(34, 180, 180);
        break;

    // Default fallback = Dark+
    default:
        return getBuiltinTheme(IDM_THEME_DARK_PLUS);
    }

    t.fontName = "Cascadia Code";
    t.fontSize = 12;
    t.windowAlpha = 255;
    return t;
}

// ============================================================================
// POPULATE BUILT-IN THEMES
// ============================================================================

void Win32IDE::populateBuiltinThemes() {
    LOG_INFO("Populating 16 built-in themes");
    for (int id = IDM_THEME_DARK_PLUS; id <= IDM_THEME_ABYSS; id++) {
        IDETheme t = getBuiltinTheme(id);
        m_themes[t.name] = t;
    }
    // Apply the default (Dark+)
    m_currentTheme = getBuiltinTheme(IDM_THEME_DARK_PLUS);
    m_activeThemeId = IDM_THEME_DARK_PLUS;
}

// ============================================================================
// APPLY THEME BY ID
// ============================================================================

void Win32IDE::applyThemeById(int themeId) {
    if (themeId < IDM_THEME_DARK_PLUS || themeId > IDM_THEME_ABYSS) return;

    LOG_INFO("Applying theme ID " + std::to_string(themeId));

    m_currentTheme = getBuiltinTheme(themeId);
    m_activeThemeId = themeId;
    applyTheme();
    applyThemeToAllControls();

    // Update check state in menu
    if (m_hMenu) {
        for (int id = IDM_THEME_DARK_PLUS; id <= IDM_THEME_ABYSS; id++) {
            CheckMenuItem(m_hMenu, id, MF_BYCOMMAND |
                (id == themeId ? MF_CHECKED : MF_UNCHECKED));
        }
    }

    // Update status bar to brag about the theme
    if (m_hwndStatusBar) {
        std::string msg = "Theme: " + m_currentTheme.name + " applied";
        SendMessageA(m_hwndStatusBar, SB_SETTEXT, 0, (LPARAM)msg.c_str());
    }

    // Re-trigger syntax coloring with new theme colors
    if (m_syntaxColoringEnabled) {
        onEditorContentChanged();
    }
}

// ============================================================================
// ============================================================================
// THEME DIFF LOGGING — emits changed fields during live preview
//
// Compares two IDETheme structs field-by-field and logs only the fields
// that differ.  This avoids flooding the log while still giving theme
// authors instant feedback on what changed.  Called from the live-preview
// path inside ThemePickerDlgProc.
// ============================================================================

// Helper: format a COLORREF as hex string "#RRGGBB"
static std::string colorToHex(COLORREF c) {
    char buf[16];
    snprintf(buf, sizeof(buf), "#%02X%02X%02X", GetRValue(c), GetGValue(c), GetBValue(c));
    return std::string(buf);
}

void Win32IDE::logThemeDiff(const IDETheme& before, const IDETheme& candidate) const {
    std::string diff;
    int changeCount = 0;

    // Macro to compare a COLORREF field and append to diff
    #define DIFF_COLOR(field) \
        if (before.field != candidate.field) { \
            diff += "  " #field ": " + colorToHex(before.field) + " -> " + colorToHex(candidate.field) + "\n"; \
            changeCount++; \
        }

    if (before.name != candidate.name) {
        diff += "  name: \"" + before.name + "\" -> \"" + candidate.name + "\"\n";
        changeCount++;
    }
    if (before.darkMode != candidate.darkMode) {
        diff += "  darkMode: " + std::string(before.darkMode ? "true" : "false") +
                " -> " + std::string(candidate.darkMode ? "true" : "false") + "\n";
        changeCount++;
    }

    // Core editor
    DIFF_COLOR(backgroundColor);
    DIFF_COLOR(textColor);
    DIFF_COLOR(keywordColor);
    DIFF_COLOR(commentColor);
    DIFF_COLOR(stringColor);
    DIFF_COLOR(numberColor);
    DIFF_COLOR(operatorColor);
    DIFF_COLOR(preprocessorColor);
    DIFF_COLOR(functionColor);
    DIFF_COLOR(typeColor);
    DIFF_COLOR(selectionColor);
    DIFF_COLOR(selectionTextColor);
    DIFF_COLOR(lineNumberColor);
    DIFF_COLOR(lineNumberBg);
    DIFF_COLOR(currentLineBg);
    DIFF_COLOR(cursorColor);

    // Sidebar / Activity bar
    DIFF_COLOR(sidebarBg);
    DIFF_COLOR(sidebarFg);
    DIFF_COLOR(sidebarHeaderBg);
    DIFF_COLOR(activityBarBg);
    DIFF_COLOR(activityBarFg);
    DIFF_COLOR(activityBarIndicator);
    DIFF_COLOR(activityBarHoverBg);

    // Tab bar
    DIFF_COLOR(tabBarBg);
    DIFF_COLOR(tabActiveBg);
    DIFF_COLOR(tabActiveFg);
    DIFF_COLOR(tabInactiveBg);
    DIFF_COLOR(tabInactiveFg);
    DIFF_COLOR(tabBorder);

    // Status bar
    DIFF_COLOR(statusBarBg);
    DIFF_COLOR(statusBarFg);
    DIFF_COLOR(statusBarAccent);

    // Panel
    DIFF_COLOR(panelBg);
    DIFF_COLOR(panelFg);
    DIFF_COLOR(panelBorder);
    DIFF_COLOR(panelHeaderBg);

    // Title bar
    DIFF_COLOR(titleBarBg);
    DIFF_COLOR(titleBarFg);

    // Scrollbar
    DIFF_COLOR(scrollbarBg);
    DIFF_COLOR(scrollbarThumb);
    DIFF_COLOR(scrollbarThumbHover);

    // Bracket / indent
    DIFF_COLOR(bracketMatchBg);
    DIFF_COLOR(indentGuideColor);

    // Accent / diagnostic
    DIFF_COLOR(accentColor);
    DIFF_COLOR(errorColor);
    DIFF_COLOR(warningColor);
    DIFF_COLOR(infoColor);

    #undef DIFF_COLOR

    if (changeCount == 0) {
        LOG_DEBUG("Theme diff: \"" + before.name + "\" -> \"" + candidate.name + "\" — no field changes");
    } else {
        LOG_DEBUG("Theme diff: \"" + before.name + "\" -> \"" + candidate.name +
                  "\" — " + std::to_string(changeCount) + " field(s) changed:\n" + diff);
    }
}

// ============================================================================
// DEEP APPLY — applies theme colors to EVERY control surface
// ============================================================================

void Win32IDE::applyThemeToAllControls() {
    const IDETheme& t = m_currentTheme;

    LOG_DEBUG("applyThemeToAllControls: \"" + t.name + "\" (darkMode=" +
              std::to_string(t.darkMode) + ")");

    // -- Title bar dark mode (Windows 10/11) --
    BOOL useDarkTitleBar = t.darkMode ? TRUE : FALSE;
    if (m_hwndMain) {
        // DWMWA_USE_IMMERSIVE_DARK_MODE = 20
        DwmSetWindowAttribute(m_hwndMain, 20, &useDarkTitleBar, sizeof(useDarkTitleBar));
        // Also try the older undocumented attribute 19 for pre-20H1
        DwmSetWindowAttribute(m_hwndMain, 19, &useDarkTitleBar, sizeof(useDarkTitleBar));
    }

    // -- Activity Bar (tracked brushes — deleted before recreation) --
    if (m_hwndActivityBar) {
        if (m_actBarBackgroundBrush) DeleteObject(m_actBarBackgroundBrush);
        if (m_actBarHoverBrush) DeleteObject(m_actBarHoverBrush);
        if (m_actBarActiveBrush) DeleteObject(m_actBarActiveBrush);
        m_actBarBackgroundBrush = CreateSolidBrush(t.activityBarBg);
        m_actBarHoverBrush      = CreateSolidBrush(t.activityBarHoverBg);
        m_actBarActiveBrush     = CreateSolidBrush(t.activityBarBg);
        SetClassLongPtr(m_hwndActivityBar, GCLP_HBRBACKGROUND, (LONG_PTR)m_actBarBackgroundBrush);
        InvalidateRect(m_hwndActivityBar, nullptr, TRUE);

        for (int i = 0; i < 6; i++) {
            if (m_activityBarButtons[i]) {
                InvalidateRect(m_activityBarButtons[i], nullptr, TRUE);
            }
        }
    }

    // -- Sidebar (tracked brush — no leak) --
    if (m_hwndSidebar) {
        if (m_sidebarBrush) DeleteObject(m_sidebarBrush);
        m_sidebarBrush = CreateSolidBrush(t.sidebarBg);
        SetClassLongPtr(m_hwndSidebar, GCLP_HBRBACKGROUND, (LONG_PTR)m_sidebarBrush);
        InvalidateRect(m_hwndSidebar, nullptr, TRUE);
    }
    if (m_hwndSidebarContent) {
        if (m_sidebarContentBrush) DeleteObject(m_sidebarContentBrush);
        m_sidebarContentBrush = CreateSolidBrush(t.sidebarBg);
        SetClassLongPtr(m_hwndSidebarContent, GCLP_HBRBACKGROUND, (LONG_PTR)m_sidebarContentBrush);
        InvalidateRect(m_hwndSidebarContent, nullptr, TRUE);
    }

    // -- Explorer tree --
    if (m_hwndExplorerTree) {
        TreeView_SetBkColor(m_hwndExplorerTree, t.sidebarBg);
        TreeView_SetTextColor(m_hwndExplorerTree, t.sidebarFg);
        InvalidateRect(m_hwndExplorerTree, nullptr, TRUE);
    }

    // -- Tab bar --
    if (m_hwndTabBar) {
        InvalidateRect(m_hwndTabBar, nullptr, TRUE);
    }

    // -- Status bar --
    if (m_hwndStatusBar) {
        SendMessage(m_hwndStatusBar, SB_SETBKCOLOR, 0, (LPARAM)t.statusBarBg);
        InvalidateRect(m_hwndStatusBar, nullptr, TRUE);
    }

    // -- Editor --
    if (m_hwndEditor) {
        SendMessage(m_hwndEditor, EM_SETBKGNDCOLOR, 0, t.backgroundColor);
        CHARFORMAT2A cf = {};
        cf.cbSize = sizeof(cf);
        cf.dwMask = CFM_COLOR;
        cf.crTextColor = t.textColor;
        cf.dwEffects = 0;
        SendMessageA(m_hwndEditor, EM_SETCHARFORMAT, SCF_DEFAULT, (LPARAM)&cf);
        InvalidateRect(m_hwndEditor, nullptr, FALSE);
    }

    // -- Line number gutter --
    if (m_hwndLineNumbers) {
        InvalidateRect(m_hwndLineNumbers, nullptr, TRUE);
    }

    // -- Panel container / tabs (tracked brush) --
    if (m_hwndPanelContainer) {
        if (m_panelBrush) DeleteObject(m_panelBrush);
        m_panelBrush = CreateSolidBrush(t.panelBg);
        SetClassLongPtr(m_hwndPanelContainer, GCLP_HBRBACKGROUND, (LONG_PTR)m_panelBrush);
        InvalidateRect(m_hwndPanelContainer, nullptr, TRUE);
    }
    if (m_hwndPanelTabs) {
        InvalidateRect(m_hwndPanelTabs, nullptr, TRUE);
    }

    // -- Output tabs --
    if (m_hwndOutputTabs) {
        InvalidateRect(m_hwndOutputTabs, nullptr, TRUE);
    }

    // -- PowerShell panel --
    if (m_hwndPowerShellPanel) {
        InvalidateRect(m_hwndPowerShellPanel, nullptr, TRUE);
    }
    if (m_hwndPowerShellOutput) {
        SendMessage(m_hwndPowerShellOutput, EM_SETBKGNDCOLOR, 0, t.panelBg);
        CHARFORMAT2A pscf = {};
        pscf.cbSize = sizeof(pscf);
        pscf.dwMask = CFM_COLOR;
        pscf.crTextColor = t.panelFg;
        pscf.dwEffects = 0;
        SendMessageA(m_hwndPowerShellOutput, EM_SETCHARFORMAT, SCF_ALL, (LPARAM)&pscf);
    }
    if (m_hwndPowerShellInput) {
        SendMessage(m_hwndPowerShellInput, EM_SETBKGNDCOLOR, 0, blendColor(t.panelBg, RGB(255,255,255), 0.05f));
    }

    // -- Terminal panes --
    for (auto& pane : m_terminalPanes) {
        if (!pane.hwnd) continue;
        SendMessage(pane.hwnd, EM_SETBKGNDCOLOR, 0, t.panelBg);
        CHARFORMAT2A tcf = {};
        tcf.cbSize = sizeof(tcf);
        tcf.dwMask = CFM_COLOR;
        tcf.crTextColor = t.panelFg;
        tcf.dwEffects = 0;
        SendMessageA(pane.hwnd, EM_SETCHARFORMAT, SCF_ALL, (LPARAM)&tcf);
    }

    // -- Secondary Sidebar (tracked brush) --
    if (m_hwndSecondarySidebar) {
        if (m_secondarySidebarBrush) DeleteObject(m_secondarySidebarBrush);
        m_secondarySidebarBrush = CreateSolidBrush(t.sidebarBg);
        SetClassLongPtr(m_hwndSecondarySidebar, GCLP_HBRBACKGROUND, (LONG_PTR)m_secondarySidebarBrush);
        InvalidateRect(m_hwndSecondarySidebar, nullptr, TRUE);
    }

    // -- Main window repaint (tracked brush) --
    if (m_hwndMain) {
        if (m_mainWindowBrush) DeleteObject(m_mainWindowBrush);
        m_mainWindowBrush = CreateSolidBrush(t.backgroundColor);
        SetClassLongPtr(m_hwndMain, GCLP_HBRBACKGROUND, (LONG_PTR)m_mainWindowBrush);
        InvalidateRect(m_hwndMain, nullptr, TRUE);
    }

    LOG_INFO("Theme \"" + t.name + "\" applied to all controls (0 GDI leaks)");
}

// ============================================================================
// TRANSPARENCY
// ============================================================================

void Win32IDE::setWindowTransparency(BYTE alpha) {
    if (!m_hwndMain) return;

    m_windowAlpha = alpha;
    m_currentTheme.windowAlpha = alpha;

    if (alpha < 255) {
        // Enable layered window if not already
        LONG_PTR exStyle = GetWindowLongPtr(m_hwndMain, GWL_EXSTYLE);
        if (!(exStyle & WS_EX_LAYERED)) {
            SetWindowLongPtr(m_hwndMain, GWL_EXSTYLE, exStyle | WS_EX_LAYERED);
        }
        SetLayeredWindowAttributes(m_hwndMain, 0, alpha, LWA_ALPHA);
        m_transparencyEnabled = true;
        LOG_INFO("Window transparency set to " + std::to_string(alpha) + "/255 (" +
                 std::to_string((int)(alpha * 100 / 255)) + "%)");
    } else {
        // Disable layered window for 100% opaque (better performance)
        LONG_PTR exStyle = GetWindowLongPtr(m_hwndMain, GWL_EXSTYLE);
        if (exStyle & WS_EX_LAYERED) {
            SetWindowLongPtr(m_hwndMain, GWL_EXSTYLE, exStyle & ~WS_EX_LAYERED);
        }
        m_transparencyEnabled = false;
        LOG_INFO("Window transparency disabled (100% opaque)");
    }

    // Update transparency menu check states
    if (m_hMenu) {
        int checks[] = { IDM_TRANSPARENCY_100, IDM_TRANSPARENCY_90, IDM_TRANSPARENCY_80,
                         IDM_TRANSPARENCY_70, IDM_TRANSPARENCY_60, IDM_TRANSPARENCY_50,
                         IDM_TRANSPARENCY_40 };
        BYTE values[] = { 255, 230, 204, 178, 153, 128, 102 };
        for (int i = 0; i < 7; i++) {
            CheckMenuItem(m_hMenu, checks[i], MF_BYCOMMAND |
                (alpha == values[i] ? MF_CHECKED : MF_UNCHECKED));
        }
    }

    // Status bar feedback
    if (m_hwndStatusBar) {
        int pct = (int)(alpha * 100 / 255);
        std::string msg = "Transparency: " + std::to_string(pct) + "%";
        if (pct == 100) msg = "Transparency: Off (100% opaque)";
        SendMessageA(m_hwndStatusBar, SB_SETTEXT, 0, (LPARAM)msg.c_str());
    }
}

// ============================================================================
// TRANSPARENCY SLIDER DIALOG (Custom slider via TrackBar control)
// ============================================================================

static Win32IDE* s_sliderInstance = nullptr;

INT_PTR CALLBACK TransparencyDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_INITDIALOG: {
        // Create slider control
        HWND hSlider = CreateWindowExA(0, TRACKBAR_CLASSA, "",
            WS_CHILD | WS_VISIBLE | TBS_HORZ | TBS_AUTOTICKS,
            20, 50, 350, 35, hwndDlg, (HMENU)1001, nullptr, nullptr);
        SendMessage(hSlider, TBM_SETRANGE, TRUE, MAKELPARAM(30, 100));
        int currentPct = s_sliderInstance ? (int)(s_sliderInstance->m_windowAlpha * 100 / 255) : 100;
        SendMessage(hSlider, TBM_SETPOS, TRUE, currentPct);
        SendMessage(hSlider, TBM_SETTICFREQ, 10, 0);

        // Label
        HWND hLabel = CreateWindowExA(0, "STATIC", "Window Opacity:",
            WS_CHILD | WS_VISIBLE, 20, 20, 200, 20, hwndDlg, (HMENU)1002, nullptr, nullptr);
        (void)hLabel;

        // Value label
        char buf[32]; snprintf(buf, sizeof(buf), "%d%%", currentPct);
        CreateWindowExA(0, "STATIC", buf,
            WS_CHILD | WS_VISIBLE | SS_CENTER, 160, 85, 60, 20, hwndDlg, (HMENU)1003, nullptr, nullptr);

        // "See-through mode lets you multitask with content behind the IDE"
        CreateWindowExA(0, "STATIC",
            "Drag the slider to adjust. Lower values = more see-through.\n"
            "Perfect for referencing docs or videos behind the IDE.",
            WS_CHILD | WS_VISIBLE, 20, 110, 350, 40, hwndDlg, (HMENU)1004, nullptr, nullptr);

        SetFocus(hSlider);
        return TRUE;
    }
    case WM_HSCROLL: {
        HWND hSlider = GetDlgItem(hwndDlg, 1001);
        int pos = (int)SendMessage(hSlider, TBM_GETPOS, 0, 0);
        BYTE alpha = (BYTE)(pos * 255 / 100);
        if (s_sliderInstance) {
            s_sliderInstance->setWindowTransparency(alpha);
        }
        // Update value label
        HWND hVal = GetDlgItem(hwndDlg, 1003);
        if (hVal) {
            char buf[32]; snprintf(buf, sizeof(buf), "%d%%", pos);
            SetWindowTextA(hVal, buf);
        }
        return TRUE;
    }
    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) {
            EndDialog(hwndDlg, LOWORD(wParam));
            return TRUE;
        }
        break;
    case WM_CLOSE:
        EndDialog(hwndDlg, IDCANCEL);
        return TRUE;
    }
    return FALSE;
}

void Win32IDE::showTransparencySlider() {
    // Build a dialog template in memory (no .rc file needed)
    // Using DLGTEMPLATE struct
    struct {
        DLGTEMPLATE dlg;
        WORD menuArray;
        WORD classArray;
        WCHAR titleArray[32];
    } dlgTemplate = {};

    dlgTemplate.dlg.style = DS_MODALFRAME | DS_CENTER | WS_POPUP | WS_CAPTION | WS_SYSMENU;
    dlgTemplate.dlg.dwExtendedStyle = 0;
    dlgTemplate.dlg.cdit = 0; // We create controls in WM_INITDIALOG
    dlgTemplate.dlg.x = 0;
    dlgTemplate.dlg.y = 0;
    dlgTemplate.dlg.cx = 200;
    dlgTemplate.dlg.cy = 100;
    dlgTemplate.menuArray = 0;
    dlgTemplate.classArray = 0;
    wcscpy(dlgTemplate.titleArray, L"Window Transparency");

    s_sliderInstance = this;
    DialogBoxIndirectA(m_hInstance, &dlgTemplate.dlg, m_hwndMain, TransparencyDlgProc);
    s_sliderInstance = nullptr;
}

// ============================================================================
// BUILD APPEARANCE MENU (Themes + Transparency submenus)
// ============================================================================

void Win32IDE::buildThemeMenu(HMENU hParentMenu) {
    // -- Themes Submenu --
    HMENU hThemeMenu = CreatePopupMenu();
    AppendMenuA(hThemeMenu, MF_STRING, IDM_THEME_DARK_PLUS,        "Dark+ (Default)");
    AppendMenuA(hThemeMenu, MF_STRING, IDM_THEME_LIGHT_PLUS,       "Light+");
    AppendMenuA(hThemeMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuA(hThemeMenu, MF_STRING, IDM_THEME_MONOKAI,          "Monokai");
    AppendMenuA(hThemeMenu, MF_STRING, IDM_THEME_DRACULA,          "Dracula");
    AppendMenuA(hThemeMenu, MF_STRING, IDM_THEME_NORD,             "Nord");
    AppendMenuA(hThemeMenu, MF_STRING, IDM_THEME_ONE_DARK_PRO,     "One Dark Pro");
    AppendMenuA(hThemeMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuA(hThemeMenu, MF_STRING, IDM_THEME_SOLARIZED_DARK,   "Solarized Dark");
    AppendMenuA(hThemeMenu, MF_STRING, IDM_THEME_SOLARIZED_LIGHT,  "Solarized Light");
    AppendMenuA(hThemeMenu, MF_STRING, IDM_THEME_GRUVBOX_DARK,     "Gruvbox Dark");
    AppendMenuA(hThemeMenu, MF_STRING, IDM_THEME_CATPPUCCIN_MOCHA, "Catppuccin Mocha");
    AppendMenuA(hThemeMenu, MF_STRING, IDM_THEME_TOKYO_NIGHT,      "Tokyo Night");
    AppendMenuA(hThemeMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuA(hThemeMenu, MF_STRING, IDM_THEME_CYBERPUNK_NEON,   "\xE2\x9A\xA1 Cyberpunk Neon");
    AppendMenuA(hThemeMenu, MF_STRING, IDM_THEME_SYNTHWAVE84,      "\xF0\x9F\x8C\x8A SynthWave '84");
    AppendMenuA(hThemeMenu, MF_STRING, IDM_THEME_RAWRXD_CRIMSON,   "\xF0\x9F\x94\xA5 RawrXD Crimson");
    AppendMenuA(hThemeMenu, MF_STRING, IDM_THEME_ABYSS,            "Abyss");
    AppendMenuA(hThemeMenu, MF_STRING, IDM_THEME_HIGH_CONTRAST,    "High Contrast");

    // Mark current theme
    CheckMenuItem(hThemeMenu, m_activeThemeId, MF_BYCOMMAND | MF_CHECKED);

    // -- Transparency Submenu --
    HMENU hTransMenu = CreatePopupMenu();
    AppendMenuA(hTransMenu, MF_STRING, IDM_TRANSPARENCY_100,    "100% (Opaque)");
    AppendMenuA(hTransMenu, MF_STRING, IDM_TRANSPARENCY_90,     "90%");
    AppendMenuA(hTransMenu, MF_STRING, IDM_TRANSPARENCY_80,     "80%");
    AppendMenuA(hTransMenu, MF_STRING, IDM_TRANSPARENCY_70,     "70%");
    AppendMenuA(hTransMenu, MF_STRING, IDM_TRANSPARENCY_60,     "60%");
    AppendMenuA(hTransMenu, MF_STRING, IDM_TRANSPARENCY_50,     "50%");
    AppendMenuA(hTransMenu, MF_STRING, IDM_TRANSPARENCY_40,     "40%");
    AppendMenuA(hTransMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuA(hTransMenu, MF_STRING, IDM_TRANSPARENCY_CUSTOM, "Custom Slider...");
    AppendMenuA(hTransMenu, MF_STRING, IDM_TRANSPARENCY_TOGGLE, "Toggle On/Off");

    // Mark current transparency level
    {
        int checks[] = { IDM_TRANSPARENCY_100, IDM_TRANSPARENCY_90, IDM_TRANSPARENCY_80,
                         IDM_TRANSPARENCY_70, IDM_TRANSPARENCY_60, IDM_TRANSPARENCY_50,
                         IDM_TRANSPARENCY_40 };
        BYTE values[] = { 255, 230, 204, 178, 153, 128, 102 };
        for (int i = 0; i < 7; i++) {
            CheckMenuItem(hTransMenu, checks[i], MF_BYCOMMAND |
                (m_windowAlpha == values[i] ? MF_CHECKED : MF_UNCHECKED));
        }
    }

    // Append to parent menu
    AppendMenuA(hParentMenu, MF_POPUP, (UINT_PTR)hThemeMenu, "Color &Theme");
    AppendMenuA(hParentMenu, MF_POPUP, (UINT_PTR)hTransMenu, "&Transparency");
}

// ============================================================================
// SHOW THEME PICKER — Modal dialog with LISTBOX, live preview, OK / Cancel
// ============================================================================

static Win32IDE* s_pickerInstance = nullptr;

// Theme entries for the picker (index → command ID mapping)
struct ThemePickerEntry {
    int commandId;
    const char* displayName;
};

static const ThemePickerEntry g_themePickerEntries[] = {
    { IDM_THEME_DARK_PLUS,        "Dark+ (Default)" },
    { IDM_THEME_LIGHT_PLUS,       "Light+" },
    { IDM_THEME_MONOKAI,          "Monokai" },
    { IDM_THEME_DRACULA,          "Dracula" },
    { IDM_THEME_NORD,             "Nord" },
    { IDM_THEME_SOLARIZED_DARK,   "Solarized Dark" },
    { IDM_THEME_SOLARIZED_LIGHT,  "Solarized Light" },
    { IDM_THEME_CYBERPUNK_NEON,   "\xE2\x9A\xA1 Cyberpunk Neon" },
    { IDM_THEME_GRUVBOX_DARK,     "Gruvbox Dark" },
    { IDM_THEME_CATPPUCCIN_MOCHA, "Catppuccin Mocha" },
    { IDM_THEME_TOKYO_NIGHT,      "Tokyo Night" },
    { IDM_THEME_RAWRXD_CRIMSON,   "\xF0\x9F\x94\xA5 RawrXD Crimson" },
    { IDM_THEME_HIGH_CONTRAST,    "High Contrast" },
    { IDM_THEME_ONE_DARK_PRO,     "One Dark Pro" },
    { IDM_THEME_SYNTHWAVE84,      "\xF0\x9F\x8C\x8A SynthWave '84" },
    { IDM_THEME_ABYSS,            "Abyss" },
};
static const int g_themePickerCount = sizeof(g_themePickerEntries) / sizeof(g_themePickerEntries[0]);

// Control IDs for the theme picker dialog
#define IDP_THEME_LIST      2001
#define IDP_TRANSPARENCY    2002
#define IDP_TRANSPARENCY_LBL 2003
#define IDP_BTN_OK          IDOK
#define IDP_BTN_CANCEL      IDCANCEL

INT_PTR CALLBACK ThemePickerDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_INITDIALOG: {
        if (!s_pickerInstance) { EndDialog(hwndDlg, IDCANCEL); return TRUE; }

        // -- Theme listbox --
        CreateWindowExA(0, "STATIC", "Color Theme:",
            WS_CHILD | WS_VISIBLE, 14, 10, 200, 18, hwndDlg, nullptr, nullptr, nullptr);

        HWND hList = CreateWindowExA(WS_EX_CLIENTEDGE, "LISTBOX", "",
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT,
            14, 30, 290, 260, hwndDlg, (HMENU)(UINT_PTR)IDP_THEME_LIST, nullptr, nullptr);

        // Populate + select current
        int selectIdx = 0;
        for (int i = 0; i < g_themePickerCount; i++) {
            SendMessageA(hList, LB_ADDSTRING, 0, (LPARAM)g_themePickerEntries[i].displayName);
            if (g_themePickerEntries[i].commandId == s_pickerInstance->m_activeThemeId)
                selectIdx = i;
        }
        SendMessage(hList, LB_SETCURSEL, selectIdx, 0);

        // -- Transparency slider --
        CreateWindowExA(0, "STATIC", "Window Opacity:",
            WS_CHILD | WS_VISIBLE, 14, 300, 120, 18, hwndDlg, nullptr, nullptr, nullptr);

        HWND hSlider = CreateWindowExA(0, TRACKBAR_CLASSA, "",
            WS_CHILD | WS_VISIBLE | TBS_HORZ | TBS_AUTOTICKS,
            14, 320, 230, 28, hwndDlg, (HMENU)(UINT_PTR)IDP_TRANSPARENCY, nullptr, nullptr);
        SendMessage(hSlider, TBM_SETRANGE, TRUE, MAKELPARAM(30, 100));
        int pct = (int)(s_pickerInstance->m_windowAlpha * 100 / 255);
        SendMessage(hSlider, TBM_SETPOS, TRUE, pct);
        SendMessage(hSlider, TBM_SETTICFREQ, 10, 0);

        char pctBuf[16]; snprintf(pctBuf, sizeof(pctBuf), "%d%%", pct);
        CreateWindowExA(0, "STATIC", pctBuf,
            WS_CHILD | WS_VISIBLE | SS_CENTER,
            250, 324, 50, 18, hwndDlg, (HMENU)(UINT_PTR)IDP_TRANSPARENCY_LBL, nullptr, nullptr);

        // -- OK / Cancel buttons --
        CreateWindowExA(0, "BUTTON", "OK",
            WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
            120, 360, 80, 28, hwndDlg, (HMENU)(UINT_PTR)IDP_BTN_OK, nullptr, nullptr);
        CreateWindowExA(0, "BUTTON", "Cancel",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            210, 360, 80, 28, hwndDlg, (HMENU)(UINT_PTR)IDP_BTN_CANCEL, nullptr, nullptr);

        SetFocus(hList);
        return FALSE; // We set focus manually
    }

    case WM_COMMAND: {
        int ctrl = LOWORD(wParam);
        int notif = HIWORD(wParam);

        // -- Live preview on listbox selection change --
        if (ctrl == IDP_THEME_LIST && notif == LBN_SELCHANGE) {
            HWND hList = GetDlgItem(hwndDlg, IDP_THEME_LIST);
            int idx = (int)SendMessage(hList, LB_GETCURSEL, 0, 0);
            if (idx >= 0 && idx < g_themePickerCount && s_pickerInstance) {
                int themeId = g_themePickerEntries[idx].commandId;
                // Log theme diff before applying preview
                IDETheme candidate = s_pickerInstance->getBuiltinTheme(themeId);
                s_pickerInstance->logThemeDiff(s_pickerInstance->m_currentTheme, candidate);
                // Apply live preview (don't persist until OK)
                s_pickerInstance->m_currentTheme = candidate;
                s_pickerInstance->m_activeThemeId = themeId;
                s_pickerInstance->applyTheme();
                s_pickerInstance->applyThemeToAllControls();
                if (s_pickerInstance->m_syntaxColoringEnabled) {
                    s_pickerInstance->onEditorContentChanged();
                }
                LOG_DEBUG("Theme picker: live preview of \"" +
                          s_pickerInstance->m_currentTheme.name + "\"");
            }
            return TRUE;
        }

        // -- OK: accept current selection --
        if (ctrl == IDP_BTN_OK) {
            // The live preview already applied the theme — just persist the choice
            if (s_pickerInstance) {
                // Update menu check marks
                if (s_pickerInstance->m_hMenu) {
                    for (int id = IDM_THEME_DARK_PLUS; id <= IDM_THEME_ABYSS; id++) {
                        CheckMenuItem(s_pickerInstance->m_hMenu, id, MF_BYCOMMAND |
                            (id == s_pickerInstance->m_activeThemeId ? MF_CHECKED : MF_UNCHECKED));
                    }
                }
                if (s_pickerInstance->m_hwndStatusBar) {
                    std::string msg = "Theme: " + s_pickerInstance->m_currentTheme.name + " applied";
                    SendMessageA(s_pickerInstance->m_hwndStatusBar, SB_SETTEXT, 0, (LPARAM)msg.c_str());
                }
                LOG_INFO("Theme picker: confirmed \"" + s_pickerInstance->m_currentTheme.name + "\"");
            }
            EndDialog(hwndDlg, IDOK);
            return TRUE;
        }

        // -- Cancel: restore previous theme --
        if (ctrl == IDP_BTN_CANCEL) {
            if (s_pickerInstance) {
                s_pickerInstance->m_currentTheme = s_pickerInstance->m_themeBeforePreview;
                s_pickerInstance->m_activeThemeId = s_pickerInstance->m_themeIdBeforePreview;
                s_pickerInstance->applyTheme();
                s_pickerInstance->applyThemeToAllControls();
                if (s_pickerInstance->m_syntaxColoringEnabled) {
                    s_pickerInstance->onEditorContentChanged();
                }
                LOG_INFO("Theme picker: cancelled — restored \"" +
                         s_pickerInstance->m_currentTheme.name + "\"");
            }
            EndDialog(hwndDlg, IDCANCEL);
            return TRUE;
        }
        break;
    }

    case WM_HSCROLL: {
        // Transparency slider
        HWND hSlider = GetDlgItem(hwndDlg, IDP_TRANSPARENCY);
        if ((HWND)lParam == hSlider && s_pickerInstance) {
            int pos = (int)SendMessage(hSlider, TBM_GETPOS, 0, 0);
            BYTE alpha = (BYTE)(pos * 255 / 100);
            s_pickerInstance->setWindowTransparency(alpha);
            HWND hLbl = GetDlgItem(hwndDlg, IDP_TRANSPARENCY_LBL);
            if (hLbl) {
                char buf[16]; snprintf(buf, sizeof(buf), "%d%%", pos);
                SetWindowTextA(hLbl, buf);
            }
        }
        return TRUE;
    }

    case WM_CLOSE:
        // Treat close as Cancel
        SendMessage(hwndDlg, WM_COMMAND, MAKEWPARAM(IDP_BTN_CANCEL, BN_CLICKED), 0);
        return TRUE;
    }
    return FALSE;
}

void Win32IDE::showThemePicker() {
    // Save state for rollback on Cancel
    m_themeBeforePreview = m_currentTheme;
    m_themeIdBeforePreview = m_activeThemeId;

    // Build in-memory DLGTEMPLATE (no .rc file needed)
    struct {
        DLGTEMPLATE dlg;
        WORD menuArray;
        WORD classArray;
        WCHAR titleArray[32];
    } dlgTemplate = {};

    dlgTemplate.dlg.style = DS_MODALFRAME | DS_CENTER | WS_POPUP | WS_CAPTION | WS_SYSMENU;
    dlgTemplate.dlg.dwExtendedStyle = 0;
    dlgTemplate.dlg.cdit = 0; // Controls created in WM_INITDIALOG
    dlgTemplate.dlg.x = 0;
    dlgTemplate.dlg.y = 0;
    dlgTemplate.dlg.cx = 170; // dialog units
    dlgTemplate.dlg.cy = 210;
    dlgTemplate.menuArray = 0;
    dlgTemplate.classArray = 0;
    wcscpy(dlgTemplate.titleArray, L"RawrXD Theme Picker");

    s_pickerInstance = this;
    DialogBoxIndirectA(m_hInstance, &dlgTemplate.dlg, m_hwndMain, ThemePickerDlgProc);
    s_pickerInstance = nullptr;
}
