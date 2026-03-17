/**
 * @file monaco_settings_dialog.cpp
 * @brief MonacoSettingsDialog — pure C++20/Win32 (zero Qt).
 *
 * 5-tab settings panel: Theme, Font, Editor, Neon/ESP, Performance.
 * @copyright RawrXD IDE 2026
 */
#include "monaco_settings_dialog.h"

#include <commdlg.h>
#include <shlobj.h>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <algorithm>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "comdlg32.lib")

namespace RawrXD::UI {

// ═══════════════════════════════════════════════════════════════════════════════
// Helpers
// ═══════════════════════════════════════════════════════════════════════════════

static COLORREF u32ToCR(uint32_t c)
{
    // 0xAARRGGBB → COLORREF(0x00BBGGRR)
    return RGB((c >> 16) & 0xFF, (c >> 8) & 0xFF, c & 0xFF);
}

static uint32_t crToU32(COLORREF cr)
{
    return 0xFF000000u | (GetRValue(cr) << 16) | (GetGValue(cr) << 8) | GetBValue(cr);
}

static void setCheck(HWND h, bool v) { SendMessage(h, BM_SETCHECK, v ? BST_CHECKED : BST_UNCHECKED, 0); }
static bool getCheck(HWND h) { return SendMessage(h, BM_GETCHECK, 0, 0) == BST_CHECKED; }

static int getEditInt(HWND h)
{
    char buf[32];
    GetWindowTextA(h, buf, 32);
    return atoi(buf);
}

static void setEditInt(HWND h, int v)
{
    char buf[32];
    _snprintf_s(buf, 32, _TRUNCATE, "%d", v);
    SetWindowTextA(h, buf);
}

static std::string getEditText(HWND h)
{
    int len = GetWindowTextLengthA(h);
    if (len <= 0) return {};
    std::string s(len + 1, '\0');
    GetWindowTextA(h, s.data(), len + 1);
    s.resize(len);
    return s;
}

// ═══════════════════════════════════════════════════════════════════════════════
// Construction / Destruction
// ═══════════════════════════════════════════════════════════════════════════════

MonacoSettingsDialog::MonacoSettingsDialog(HWND parent)
    : m_hwndParent(parent) {}

MonacoSettingsDialog::~MonacoSettingsDialog()
{
    if (m_hDlg && IsWindow(m_hDlg)) DestroyWindow(m_hDlg);
}

// ═══════════════════════════════════════════════════════════════════════════════
// Modal Display
// ═══════════════════════════════════════════════════════════════════════════════

INT_PTR MonacoSettingsDialog::showModal()
{
    HINSTANCE hInst = GetModuleHandle(nullptr);
    constexpr int W = 680, H = 600;

    RECT rc{};
    if (m_hwndParent) GetWindowRect(m_hwndParent, &rc);
    else { rc.left = 120; rc.top = 80; }

    m_hDlg = CreateWindowExW(WS_EX_DLGMODALFRAME,
        L"STATIC", L"RawrXD IDE - Monaco Editor Settings",
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
        rc.left + 60, rc.top + 30, W, H,
        m_hwndParent, nullptr, hInst, nullptr);
    if (!m_hDlg) return IDCANCEL;

    SetWindowLongPtrW(m_hDlg, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
    SetWindowLongPtrW(m_hDlg, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(DlgProc));

    m_originalSettings = m_settings;
    initControls(m_hDlg);
    updateUIFromSettings();

    if (m_hwndParent) EnableWindow(m_hwndParent, FALSE);
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        if (!IsWindow(m_hDlg)) break;
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    if (m_hwndParent) EnableWindow(m_hwndParent, TRUE);
    return IDOK;
}

// ═══════════════════════════════════════════════════════════════════════════════
// Window Procedure
// ═══════════════════════════════════════════════════════════════════════════════

INT_PTR CALLBACK MonacoSettingsDialog::DlgProc(HWND h, UINT msg, WPARAM wp, LPARAM lp)
{
    auto* self = reinterpret_cast<MonacoSettingsDialog*>(GetWindowLongPtrW(h, GWLP_USERDATA));
    if (self) return self->handleMsg(h, msg, wp, lp);
    return DefWindowProcW(h, msg, wp, lp);
}

INT_PTR MonacoSettingsDialog::handleMsg(HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_NOTIFY: {
        auto* nmhdr = reinterpret_cast<NMHDR*>(lp);
        if (nmhdr->idFrom == IDC_MS_TAB && nmhdr->code == TCN_SELCHANGE) {
            int page = TabCtrl_GetCurSel(m_hwndTab);
            showTabPage(page);
        }
        return 0;
    }

    case WM_HSCROLL:
        // Glow intensity trackbar
        if (reinterpret_cast<HWND>(lp) == m_hwndGlowSlider) {
            int pos = static_cast<int>(SendMessage(m_hwndGlowSlider, TBM_GETPOS, 0, 0));
            char buf[8]; _snprintf_s(buf, 8, _TRUNCATE, "%d", pos);
            SetWindowTextA(m_hwndGlowLabel, buf);
        }
        return 0;

    case WM_COMMAND:
        switch (LOWORD(wp)) {
        // Color buttons
        case IDC_MS_CLRBG:    onColorButtonClicked(IDC_MS_CLRBG,    m_settings.backgroundColor);    return TRUE;
        case IDC_MS_CLRFG:    onColorButtonClicked(IDC_MS_CLRFG,    m_settings.foregroundColor);    return TRUE;
        case IDC_MS_CLRKW:    onColorButtonClicked(IDC_MS_CLRKW,    m_settings.keywordColor);       return TRUE;
        case IDC_MS_CLRSTR:   onColorButtonClicked(IDC_MS_CLRSTR,   m_settings.stringColor);        return TRUE;
        case IDC_MS_CLRCMT:   onColorButtonClicked(IDC_MS_CLRCMT,   m_settings.commentColor);       return TRUE;
        case IDC_MS_CLRFN:    onColorButtonClicked(IDC_MS_CLRFN,    m_settings.functionColor);      return TRUE;
        case IDC_MS_CLRTYPE:  onColorButtonClicked(IDC_MS_CLRTYPE,  m_settings.typeColor);          return TRUE;
        case IDC_MS_CLRNUM:   onColorButtonClicked(IDC_MS_CLRNUM,   m_settings.numberColor);        return TRUE;
        case IDC_MS_CLRGLOW:  onColorButtonClicked(IDC_MS_CLRGLOW,  m_settings.glowColor);          return TRUE;
        case IDC_MS_CLRGLOW2: onColorButtonClicked(IDC_MS_CLRGLOW2, m_settings.glowSecondaryColor); return TRUE;

        // Bottom buttons
        case IDC_MS_APPLY:   onApply();   return TRUE;
        case IDC_MS_RESET:   onReset();   return TRUE;
        case IDC_MS_PREVIEW: onPreview(); return TRUE;
        case IDC_MS_EXPORT:  onExport();  return TRUE;
        case IDC_MS_IMPORT:  onImport();  return TRUE;

        case IDCANCEL:
            m_settings = m_originalSettings;
            DestroyWindow(hDlg); m_hDlg = nullptr; PostQuitMessage(0);
            return TRUE;
        }
        break;

    case WM_CLOSE:
        m_settings = m_originalSettings;
        DestroyWindow(hDlg); m_hDlg = nullptr; PostQuitMessage(0);
        return TRUE;
    }
    return DefWindowProcW(hDlg, msg, wp, lp);
}

// ═══════════════════════════════════════════════════════════════════════════════
// initControls — build tab control + all pages
// ═══════════════════════════════════════════════════════════════════════════════

void MonacoSettingsDialog::initControls(HWND hDlg)
{
    HINSTANCE hInst = GetModuleHandle(nullptr);

    // Tab control
    m_hwndTab = CreateWindowExW(0, WC_TABCONTROLW, nullptr,
        WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
        6, 6, 654, 500, hDlg, reinterpret_cast<HMENU>(IDC_MS_TAB), hInst, nullptr);

    const wchar_t* tabNames[] = { L"Theme", L"Font", L"Editor", L"Neon / ESP", L"Performance" };
    for (int i = 0; i < kNumPages; ++i) {
        TCITEMW ti{}; ti.mask = TCIF_TEXT;
        ti.pszText = const_cast<wchar_t*>(tabNames[i]);
        TabCtrl_InsertItem(m_hwndTab, i, &ti);
    }

    // Determine display area inside tab
    RECT tabRC; GetClientRect(m_hwndTab, &tabRC);
    TabCtrl_AdjustRect(m_hwndTab, FALSE, &tabRC);

    for (int i = 0; i < kNumPages; ++i) {
        m_pages[i] = CreateWindowExW(0, L"STATIC", nullptr,
            WS_CHILD | WS_CLIPCHILDREN,
            tabRC.left, tabRC.top,
            tabRC.right - tabRC.left, tabRC.bottom - tabRC.top,
            m_hwndTab, nullptr, hInst, nullptr);
    }

    createThemeTab(m_pages[0], 0);
    createFontTab(m_pages[1], 1);
    createEditorTab(m_pages[2], 2);
    createNeonTab(m_pages[3], 3);
    createPerformanceTab(m_pages[4], 4);

    // Show first page
    showTabPage(0);

    // Bottom buttons row (below tab control)
    int by = 514;
    m_hwndExport  = CreateWindowExW(0, L"BUTTON", L"Export...",   WS_CHILD | WS_VISIBLE, 6,   by, 90, 28, hDlg, reinterpret_cast<HMENU>(IDC_MS_EXPORT),  hInst, nullptr);
    m_hwndImport  = CreateWindowExW(0, L"BUTTON", L"Import...",   WS_CHILD | WS_VISIBLE, 102, by, 90, 28, hDlg, reinterpret_cast<HMENU>(IDC_MS_IMPORT),  hInst, nullptr);
    m_hwndPreview = CreateWindowExW(0, L"BUTTON", L"Preview",     WS_CHILD | WS_VISIBLE, 380, by, 80, 28, hDlg, reinterpret_cast<HMENU>(IDC_MS_PREVIEW), hInst, nullptr);
    m_hwndReset   = CreateWindowExW(0, L"BUTTON", L"Reset",       WS_CHILD | WS_VISIBLE, 466, by, 80, 28, hDlg, reinterpret_cast<HMENU>(IDC_MS_RESET),   hInst, nullptr);
    m_hwndApply   = CreateWindowExW(0, L"BUTTON", L"Apply",       WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON, 552, by, 106, 28, hDlg, reinterpret_cast<HMENU>(IDC_MS_APPLY), hInst, nullptr);
}

void MonacoSettingsDialog::showTabPage(int page)
{
    for (int i = 0; i < kNumPages; ++i)
        ShowWindow(m_pages[i], (i == page) ? SW_SHOW : SW_HIDE);
    m_curPage = page;
}

// ═══════════════════════════════════════════════════════════════════════════════
// createThemeTab
// ═══════════════════════════════════════════════════════════════════════════════

void MonacoSettingsDialog::createThemeTab(HWND parent, int /*page*/)
{
    HINSTANCE hI = GetModuleHandle(nullptr);
    int y = 8;

    // Variant combo
    CreateWindowExW(0, L"STATIC", L"Editor Variant:", WS_CHILD | WS_VISIBLE, 10, y + 4, 110, 18, parent, nullptr, hI, nullptr);
    m_hwndVariant = CreateWindowExW(0, L"COMBOBOX", nullptr, WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST, 124, y, 240, 200, parent, reinterpret_cast<HMENU>(IDC_MS_VARIANT), hI, nullptr);
    const wchar_t* vars[] = { L"Core (Pure Monaco)", L"Neon Core (Cyberpunk)", L"Neon Hack (ESP)", L"Zero Dependency (Minimal)", L"Enterprise (LSP+Debug)" };
    for (auto v : vars) SendMessageW(m_hwndVariant, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(v));
    SendMessageW(m_hwndVariant, CB_SETCURSEL, m_settings.variantIndex, 0);
    y += 30;

    // Theme preset combo
    CreateWindowExW(0, L"STATIC", L"Theme Preset:", WS_CHILD | WS_VISIBLE, 10, y + 4, 110, 18, parent, nullptr, hI, nullptr);
    m_hwndTheme = CreateWindowExW(0, L"COMBOBOX", nullptr, WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST, 124, y, 240, 240, parent, reinterpret_cast<HMENU>(IDC_MS_THEME), hI, nullptr);
    const wchar_t* themes[] = { L"Default (VS Code Dark+)", L"Neon Cyberpunk", L"Matrix Green", L"Hacker Red", L"Monokai",
        L"Solarized Dark", L"Solarized Light", L"One Dark", L"Dracula", L"Gruvbox Dark", L"Nord", L"Custom" };
    for (auto t : themes) SendMessageW(m_hwndTheme, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(t));
    SendMessageW(m_hwndTheme, CB_SETCURSEL, m_settings.themePresetIndex, 0);
    y += 36;

    // Color buttons (label + colored button)
    struct ColorEntry { const wchar_t* label; UINT id; HWND* hw; };
    ColorEntry colors[] = {
        { L"Background:",   IDC_MS_CLRBG,    &m_hwndClrBg },
        { L"Foreground:",   IDC_MS_CLRFG,    &m_hwndClrFg },
        { L"Keywords:",     IDC_MS_CLRKW,    &m_hwndClrKw },
        { L"Strings:",      IDC_MS_CLRSTR,   &m_hwndClrStr },
        { L"Comments:",     IDC_MS_CLRCMT,   &m_hwndClrCmt },
        { L"Functions:",    IDC_MS_CLRFN,    &m_hwndClrFn },
        { L"Types:",        IDC_MS_CLRTYPE,  &m_hwndClrType },
        { L"Numbers:",      IDC_MS_CLRNUM,   &m_hwndClrNum },
        { L"Glow Primary:", IDC_MS_CLRGLOW,  &m_hwndClrGlow },
        { L"Glow 2nd:",     IDC_MS_CLRGLOW2, &m_hwndClrGlow2 },
    };

    // Two-column layout for color buttons
    int cx = 0;
    for (int i = 0; i < 10; ++i) {
        int col = i % 2;
        int xBase = 10 + col * 310;
        if (col == 0 && i > 0) y += 26;

        CreateWindowExW(0, L"STATIC", colors[i].label, WS_CHILD | WS_VISIBLE, xBase, y + 4, 100, 18, parent, nullptr, hI, nullptr);
        *colors[i].hw = CreateWindowExW(0, L"BUTTON", L"", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_OWNERDRAW,
            xBase + 104, y, 60, 22, parent, reinterpret_cast<HMENU>(static_cast<UINT_PTR>(colors[i].id)), hI, nullptr);
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// createFontTab
// ═══════════════════════════════════════════════════════════════════════════════

void MonacoSettingsDialog::createFontTab(HWND parent, int /*page*/)
{
    HINSTANCE hI = GetModuleHandle(nullptr);
    int y = 8;

    auto label = [&](const wchar_t* t, int x = 10) {
        CreateWindowExW(0, L"STATIC", t, WS_CHILD | WS_VISIBLE, x, y + 4, 120, 18, parent, nullptr, hI, nullptr);
    };

    label(L"Font Family:");
    m_hwndFontFamily = CreateWindowExW(0, L"COMBOBOX", nullptr, WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | CBS_SORT,
        134, y, 250, 300, parent, reinterpret_cast<HMENU>(IDC_MS_FONT_FAMILY), hI, nullptr);
    // Populate with common monospace fonts
    const wchar_t* fonts[] = { L"Consolas", L"Cascadia Code", L"Fira Code", L"JetBrains Mono",
        L"Source Code Pro", L"Courier New", L"Lucida Console", L"Monaco", L"Menlo" };
    for (auto f : fonts) SendMessageW(m_hwndFontFamily, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(f));
    SendMessageW(m_hwndFontFamily, CB_SETCURSEL, 0, 0);
    y += 30;

    label(L"Font Size:");
    m_hwndFontSize = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"14", WS_CHILD | WS_VISIBLE | ES_NUMBER,
        134, y, 60, 22, parent, reinterpret_cast<HMENU>(IDC_MS_FONT_SIZE), hI, nullptr);
    y += 30;

    label(L"Font Weight:");
    m_hwndFontWeight = CreateWindowExW(0, L"COMBOBOX", nullptr, WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST,
        134, y, 200, 200, parent, reinterpret_cast<HMENU>(IDC_MS_FONT_WEIGHT), hI, nullptr);
    const wchar_t* weights[] = { L"Thin (100)", L"Light (300)", L"Normal (400)", L"Medium (500)",
        L"SemiBold (600)", L"Bold (700)", L"ExtraBold (800)" };
    for (auto w : weights) SendMessageW(m_hwndFontWeight, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(w));
    SendMessageW(m_hwndFontWeight, CB_SETCURSEL, 2, 0); // Normal
    y += 30;

    m_hwndFontLig = CreateWindowExW(0, L"BUTTON", L"Enable Font Ligatures", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        10, y, 250, 20, parent, reinterpret_cast<HMENU>(IDC_MS_FONT_LIG), hI, nullptr);
    y += 28;

    label(L"Line Height:");
    m_hwndLineHeight = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"0", WS_CHILD | WS_VISIBLE | ES_NUMBER,
        134, y, 60, 22, parent, reinterpret_cast<HMENU>(IDC_MS_LINE_HEIGHT), hI, nullptr);
    CreateWindowExW(0, L"STATIC", L"(0 = auto)", WS_CHILD | WS_VISIBLE, 200, y + 4, 100, 18, parent, nullptr, hI, nullptr);
}

// ═══════════════════════════════════════════════════════════════════════════════
// createEditorTab
// ═══════════════════════════════════════════════════════════════════════════════

void MonacoSettingsDialog::createEditorTab(HWND parent, int /*page*/)
{
    HINSTANCE hI = GetModuleHandle(nullptr);
    int y = 8;

    auto check = [&](HWND& hw, const wchar_t* t, UINT id) {
        hw = CreateWindowExW(0, L"BUTTON", t, WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
            10, y, 280, 20, parent, reinterpret_cast<HMENU>(id), hI, nullptr);
        y += 24;
    };

    // Editor Behavior group label
    CreateWindowExW(0, L"STATIC", L"─── Editor Behavior ───", WS_CHILD | WS_VISIBLE, 10, y, 300, 18, parent, nullptr, hI, nullptr);
    y += 22;

    check(m_hwndWordWrap,  L"Word Wrap",                     IDC_MS_WORDWRAP);

    CreateWindowExW(0, L"STATIC", L"Tab Size:", WS_CHILD | WS_VISIBLE, 10, y + 4, 70, 18, parent, nullptr, hI, nullptr);
    m_hwndTabSize = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"4", WS_CHILD | WS_VISIBLE | ES_NUMBER,
        84, y, 40, 22, parent, reinterpret_cast<HMENU>(IDC_MS_TABSIZE), hI, nullptr);
    y += 28;

    check(m_hwndSpaces,     L"Insert Spaces for Tabs",        IDC_MS_SPACES);
    check(m_hwndAutoIndent, L"Auto Indent",                   IDC_MS_AUTOINDENT);
    check(m_hwndBracket,    L"Bracket Matching",              IDC_MS_BRACKET);
    check(m_hwndAutoClose,  L"Auto-close Brackets",           IDC_MS_AUTOCLOSE);
    check(m_hwndFmtPaste,   L"Format on Paste",               IDC_MS_FMTPASTE);

    // Minimap group
    y += 4;
    CreateWindowExW(0, L"STATIC", L"─── Minimap ───", WS_CHILD | WS_VISIBLE, 10, y, 300, 18, parent, nullptr, hI, nullptr);
    y += 22;
    check(m_hwndMinimap,  L"Enable Minimap",                 IDC_MS_MINIMAP);
    check(m_hwndMiniChar, L"Render Characters",              IDC_MS_MINICHAR);

    CreateWindowExW(0, L"STATIC", L"Scale:", WS_CHILD | WS_VISIBLE, 10, y + 4, 50, 18, parent, nullptr, hI, nullptr);
    m_hwndMiniScale = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"1", WS_CHILD | WS_VISIBLE | ES_NUMBER,
        64, y, 40, 22, parent, reinterpret_cast<HMENU>(IDC_MS_MINISCALE), hI, nullptr);
    y += 28;

    // IntelliSense group
    CreateWindowExW(0, L"STATIC", L"─── IntelliSense (Enterprise) ───", WS_CHILD | WS_VISIBLE, 10, y, 300, 18, parent, nullptr, hI, nullptr);
    y += 22;
    check(m_hwndIntelli,   L"Enable IntelliSense",            IDC_MS_INTELLI);
    check(m_hwndQuickSug,  L"Quick Suggestions",              IDC_MS_QUICKSUG);

    CreateWindowExW(0, L"STATIC", L"Suggest Delay (ms):", WS_CHILD | WS_VISIBLE, 10, y + 4, 130, 18, parent, nullptr, hI, nullptr);
    m_hwndSugDelay = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"200", WS_CHILD | WS_VISIBLE | ES_NUMBER,
        144, y, 60, 22, parent, reinterpret_cast<HMENU>(IDC_MS_SUGDELAY), hI, nullptr);
    y += 28;

    check(m_hwndParamHint, L"Parameter Hints",                IDC_MS_PARAMHINT);
}

// ═══════════════════════════════════════════════════════════════════════════════
// createNeonTab
// ═══════════════════════════════════════════════════════════════════════════════

void MonacoSettingsDialog::createNeonTab(HWND parent, int /*page*/)
{
    HINSTANCE hI = GetModuleHandle(nullptr);
    int y = 8;

    auto check = [&](HWND& hw, const wchar_t* t, UINT id) {
        hw = CreateWindowExW(0, L"BUTTON", t, WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
            10, y, 300, 20, parent, reinterpret_cast<HMENU>(id), hI, nullptr);
        y += 24;
    };

    CreateWindowExW(0, L"STATIC", L"─── Neon Visual Effects ───", WS_CHILD | WS_VISIBLE, 10, y, 300, 18, parent, nullptr, hI, nullptr);
    y += 22;

    check(m_hwndNeon, L"Enable Neon Effects", IDC_MS_NEON);

    // Glow Intensity trackbar
    CreateWindowExW(0, L"STATIC", L"Glow Intensity:", WS_CHILD | WS_VISIBLE, 10, y + 4, 110, 18, parent, nullptr, hI, nullptr);
    m_hwndGlowSlider = CreateWindowExW(0, TRACKBAR_CLASSW, nullptr,
        WS_CHILD | WS_VISIBLE | TBS_HORZ | TBS_AUTOTICKS,
        124, y, 200, 24, parent, reinterpret_cast<HMENU>(IDC_MS_GLOWSLIDER), hI, nullptr);
    SendMessage(m_hwndGlowSlider, TBM_SETRANGE, TRUE, MAKELONG(0, 100));
    SendMessage(m_hwndGlowSlider, TBM_SETPOS, TRUE, m_settings.glowIntensity);
    m_hwndGlowLabel = CreateWindowExW(0, L"STATIC", L"50", WS_CHILD | WS_VISIBLE, 330, y + 4, 40, 18,
        parent, reinterpret_cast<HMENU>(IDC_MS_GLOWLABEL), hI, nullptr);
    y += 30;

    CreateWindowExW(0, L"STATIC", L"Scanline Density:", WS_CHILD | WS_VISIBLE, 10, y + 4, 110, 18, parent, nullptr, hI, nullptr);
    m_hwndScanline = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"2", WS_CHILD | WS_VISIBLE | ES_NUMBER,
        124, y, 50, 22, parent, reinterpret_cast<HMENU>(IDC_MS_SCANLINE), hI, nullptr);
    y += 28;

    CreateWindowExW(0, L"STATIC", L"Glitch Probability:", WS_CHILD | WS_VISIBLE, 10, y + 4, 110, 18, parent, nullptr, hI, nullptr);
    m_hwndGlitch = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"0", WS_CHILD | WS_VISIBLE | ES_NUMBER,
        124, y, 50, 22, parent, reinterpret_cast<HMENU>(IDC_MS_GLITCH), hI, nullptr);
    CreateWindowExW(0, L"STATIC", L"/256 per frame", WS_CHILD | WS_VISIBLE, 180, y + 4, 120, 18, parent, nullptr, hI, nullptr);
    y += 28;

    check(m_hwndParticles, L"Background Particles", IDC_MS_PARTICLES);

    CreateWindowExW(0, L"STATIC", L"Particle Count:", WS_CHILD | WS_VISIBLE, 10, y + 4, 110, 18, parent, nullptr, hI, nullptr);
    m_hwndPartCount = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"30", WS_CHILD | WS_VISIBLE | ES_NUMBER,
        124, y, 70, 22, parent, reinterpret_cast<HMENU>(IDC_MS_PARTCOUNT), hI, nullptr);
    y += 34;

    // ESP group
    CreateWindowExW(0, L"STATIC", L"─── ESP Mode (NeonHack Only) ───", WS_CHILD | WS_VISIBLE, 10, y, 350, 18, parent, nullptr, hI, nullptr);
    y += 22;

    check(m_hwndEsp,     L"Enable ESP Mode",                   IDC_MS_ESP);
    check(m_hwndEspVar,  L"Highlight Variables",               IDC_MS_ESPVAR);
    check(m_hwndEspFn,   L"Highlight Functions",               IDC_MS_ESPFN);
    check(m_hwndEspWall, L"Wallhack Symbols (Through Comments)", IDC_MS_ESPWALL);
}

// ═══════════════════════════════════════════════════════════════════════════════
// createPerformanceTab
// ═══════════════════════════════════════════════════════════════════════════════

void MonacoSettingsDialog::createPerformanceTab(HWND parent, int /*page*/)
{
    HINSTANCE hI = GetModuleHandle(nullptr);
    int y = 8;

    auto numEdit = [&](HWND& hw, const wchar_t* lbl, const wchar_t* def, UINT id, const wchar_t* suffix = nullptr) {
        CreateWindowExW(0, L"STATIC", lbl, WS_CHILD | WS_VISIBLE, 10, y + 4, 160, 18, parent, nullptr, hI, nullptr);
        hw = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", def, WS_CHILD | WS_VISIBLE | ES_NUMBER,
            174, y, 60, 22, parent, reinterpret_cast<HMENU>(id), hI, nullptr);
        if (suffix)
            CreateWindowExW(0, L"STATIC", suffix, WS_CHILD | WS_VISIBLE, 240, y + 4, 120, 18, parent, nullptr, hI, nullptr);
        y += 28;
    };

    numEdit(m_hwndRenderDel, L"Render Delay:",         L"16", IDC_MS_RENDERDEL, L"ms (16=60fps)");

    m_hwndVSync = CreateWindowExW(0, L"BUTTON", L"VBlank Sync (VSync)", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        10, y, 250, 20, parent, reinterpret_cast<HMENU>(IDC_MS_VSYNC), hI, nullptr);
    y += 28;

    numEdit(m_hwndPredFetch, L"Predictive Fetch Lines:", L"16", IDC_MS_PREDFETCH, nullptr);

    m_hwndLazyTok = CreateWindowExW(0, L"BUTTON", L"Lazy Tokenization", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        10, y, 250, 20, parent, reinterpret_cast<HMENU>(IDC_MS_LAZYTOK), hI, nullptr);
    y += 28;

    numEdit(m_hwndLazyDelay, L"Tokenization Delay:",   L"50", IDC_MS_LAZYDELAY, L"ms");
}

// ═══════════════════════════════════════════════════════════════════════════════
// Settings ↔ UI
// ═══════════════════════════════════════════════════════════════════════════════

void MonacoSettingsDialog::updateUIFromSettings()
{
    // Theme
    SendMessage(m_hwndVariant, CB_SETCURSEL, m_settings.variantIndex, 0);
    SendMessage(m_hwndTheme,   CB_SETCURSEL, m_settings.themePresetIndex, 0);

    // Font
    setEditInt(m_hwndFontSize,   m_settings.fontSize);
    setCheck(m_hwndFontLig,      m_settings.fontLigatures);
    setEditInt(m_hwndLineHeight, static_cast<int>(m_settings.lineHeight));

    // Weight index: Thin=0, Light=1, Normal=2, Medium=3, SemiBold=4, Bold=5, ExtraBold=6
    int wIdx = 2;
    const int wmap[] = { 100, 300, 400, 500, 600, 700, 800 };
    for (int i = 0; i < 7; ++i) { if (wmap[i] == m_settings.fontWeight) { wIdx = i; break; } }
    SendMessage(m_hwndFontWeight, CB_SETCURSEL, wIdx, 0);

    // Find font family in combo
    for (int i = 0; i < static_cast<int>(SendMessage(m_hwndFontFamily, CB_GETCOUNT, 0, 0)); ++i) {
        char buf[128];
        SendMessageA(m_hwndFontFamily, CB_GETLBTEXT, i, reinterpret_cast<LPARAM>(buf));
        if (m_settings.fontFamily == buf) { SendMessage(m_hwndFontFamily, CB_SETCURSEL, i, 0); break; }
    }

    // Editor
    setCheck(m_hwndWordWrap,  m_settings.wordWrap);
    setEditInt(m_hwndTabSize, m_settings.tabSize);
    setCheck(m_hwndSpaces,    m_settings.insertSpaces);
    setCheck(m_hwndAutoIndent, m_settings.autoIndent);
    setCheck(m_hwndBracket,   m_settings.bracketMatching);
    setCheck(m_hwndAutoClose, m_settings.autoClosingBrackets);
    setCheck(m_hwndFmtPaste,  m_settings.formatOnPaste);
    setCheck(m_hwndMinimap,   m_settings.minimapEnabled);
    setCheck(m_hwndMiniChar,  m_settings.minimapRenderChars);
    setEditInt(m_hwndMiniScale, m_settings.minimapScale);
    setCheck(m_hwndIntelli,   m_settings.intellisense);
    setCheck(m_hwndQuickSug,  m_settings.quickSuggestions);
    setEditInt(m_hwndSugDelay, m_settings.suggestDelay);
    setCheck(m_hwndParamHint, m_settings.parameterHints);

    // Neon
    setCheck(m_hwndNeon,      m_settings.neonEffects);
    SendMessage(m_hwndGlowSlider, TBM_SETPOS, TRUE, m_settings.glowIntensity);
    char gb[8]; _snprintf_s(gb, 8, _TRUNCATE, "%d", m_settings.glowIntensity);
    SetWindowTextA(m_hwndGlowLabel, gb);
    setEditInt(m_hwndScanline,  m_settings.scanlineDensity);
    setEditInt(m_hwndGlitch,    m_settings.glitchProbability);
    setCheck(m_hwndParticles,   m_settings.particles);
    setEditInt(m_hwndPartCount, m_settings.particleCount);
    setCheck(m_hwndEsp,         m_settings.espMode);
    setCheck(m_hwndEspVar,      m_settings.espHighlightVariables);
    setCheck(m_hwndEspFn,       m_settings.espHighlightFunctions);
    setCheck(m_hwndEspWall,     m_settings.espWallhack);

    // Performance
    setEditInt(m_hwndRenderDel, m_settings.renderDelay);
    setCheck(m_hwndVSync,       m_settings.vblankSync);
    setEditInt(m_hwndPredFetch, m_settings.predictiveFetch);
    setCheck(m_hwndLazyTok,     m_settings.lazyTokenization);
    setEditInt(m_hwndLazyDelay, m_settings.lazyTokenDelay);
}

void MonacoSettingsDialog::updateSettingsFromUI()
{
    // Theme
    m_settings.variantIndex     = static_cast<int>(SendMessage(m_hwndVariant, CB_GETCURSEL, 0, 0));
    m_settings.themePresetIndex = static_cast<int>(SendMessage(m_hwndTheme, CB_GETCURSEL, 0, 0));

    // Font
    char buf[128];
    int idx = static_cast<int>(SendMessage(m_hwndFontFamily, CB_GETCURSEL, 0, 0));
    if (idx >= 0) { SendMessageA(m_hwndFontFamily, CB_GETLBTEXT, idx, reinterpret_cast<LPARAM>(buf)); m_settings.fontFamily = buf; }
    m_settings.fontSize     = getEditInt(m_hwndFontSize);
    const int wmap[] = { 100, 300, 400, 500, 600, 700, 800 };
    int wi = static_cast<int>(SendMessage(m_hwndFontWeight, CB_GETCURSEL, 0, 0));
    m_settings.fontWeight   = (wi >= 0 && wi < 7) ? wmap[wi] : 400;
    m_settings.fontLigatures = getCheck(m_hwndFontLig);
    m_settings.lineHeight    = static_cast<float>(getEditInt(m_hwndLineHeight));

    // Editor
    m_settings.wordWrap            = getCheck(m_hwndWordWrap);
    m_settings.tabSize             = getEditInt(m_hwndTabSize);
    m_settings.insertSpaces        = getCheck(m_hwndSpaces);
    m_settings.autoIndent          = getCheck(m_hwndAutoIndent);
    m_settings.bracketMatching     = getCheck(m_hwndBracket);
    m_settings.autoClosingBrackets = getCheck(m_hwndAutoClose);
    m_settings.formatOnPaste       = getCheck(m_hwndFmtPaste);
    m_settings.minimapEnabled      = getCheck(m_hwndMinimap);
    m_settings.minimapRenderChars  = getCheck(m_hwndMiniChar);
    m_settings.minimapScale        = getEditInt(m_hwndMiniScale);
    m_settings.intellisense        = getCheck(m_hwndIntelli);
    m_settings.quickSuggestions    = getCheck(m_hwndQuickSug);
    m_settings.suggestDelay        = getEditInt(m_hwndSugDelay);
    m_settings.parameterHints      = getCheck(m_hwndParamHint);

    // Neon/ESP
    m_settings.neonEffects           = getCheck(m_hwndNeon);
    m_settings.glowIntensity         = static_cast<int>(SendMessage(m_hwndGlowSlider, TBM_GETPOS, 0, 0));
    m_settings.scanlineDensity       = getEditInt(m_hwndScanline);
    m_settings.glitchProbability     = getEditInt(m_hwndGlitch);
    m_settings.particles             = getCheck(m_hwndParticles);
    m_settings.particleCount         = getEditInt(m_hwndPartCount);
    m_settings.espMode               = getCheck(m_hwndEsp);
    m_settings.espHighlightVariables = getCheck(m_hwndEspVar);
    m_settings.espHighlightFunctions = getCheck(m_hwndEspFn);
    m_settings.espWallhack           = getCheck(m_hwndEspWall);

    // Performance
    m_settings.renderDelay      = getEditInt(m_hwndRenderDel);
    m_settings.vblankSync       = getCheck(m_hwndVSync);
    m_settings.predictiveFetch  = getEditInt(m_hwndPredFetch);
    m_settings.lazyTokenization = getCheck(m_hwndLazyTok);
    m_settings.lazyTokenDelay   = getEditInt(m_hwndLazyDelay);
}

void MonacoSettingsDialog::applyThemePreset(MonacoThemePreset preset)
{
    // Built-in theme colour maps
    switch (preset) {
    case MonacoThemePreset::Default:
        m_settings.backgroundColor = 0xFF1E1E1E; m_settings.foregroundColor = 0xFFD4D4D4;
        m_settings.keywordColor = 0xFF569CD6; m_settings.stringColor = 0xFFCE9178;
        m_settings.commentColor = 0xFF6A9955; m_settings.functionColor = 0xFFDCDCAA;
        m_settings.typeColor = 0xFF4EC9B0; m_settings.numberColor = 0xFFB5CEA8;
        m_settings.glowColor = 0xFF00FF00; m_settings.glowSecondaryColor = 0xFF008000;
        break;
    case MonacoThemePreset::NeonCyberpunk:
        m_settings.backgroundColor = 0xFF0A0A0A; m_settings.foregroundColor = 0xFF00FF41;
        m_settings.keywordColor = 0xFFFF00FF; m_settings.stringColor = 0xFF00FFFF;
        m_settings.commentColor = 0xFF666666; m_settings.functionColor = 0xFFFFFF00;
        m_settings.typeColor = 0xFFFF6600; m_settings.numberColor = 0xFF00FF00;
        m_settings.glowColor = 0xFF00FF41; m_settings.glowSecondaryColor = 0xFF00CC33;
        break;
    case MonacoThemePreset::MatrixGreen:
        m_settings.backgroundColor = 0xFF000000; m_settings.foregroundColor = 0xFF00FF00;
        m_settings.keywordColor = 0xFF00DD00; m_settings.stringColor = 0xFF00BB00;
        m_settings.commentColor = 0xFF006600; m_settings.functionColor = 0xFF00FF00;
        m_settings.typeColor = 0xFF00CC00; m_settings.numberColor = 0xFF00AA00;
        m_settings.glowColor = 0xFF00FF00; m_settings.glowSecondaryColor = 0xFF008800;
        break;
    case MonacoThemePreset::HackerRed:
        m_settings.backgroundColor = 0xFF0A0000; m_settings.foregroundColor = 0xFFFF0000;
        m_settings.keywordColor = 0xFFFF4444; m_settings.stringColor = 0xFFFF8888;
        m_settings.commentColor = 0xFF660000; m_settings.functionColor = 0xFFFFAAAA;
        m_settings.typeColor = 0xFFFF6666; m_settings.numberColor = 0xFFFF3333;
        m_settings.glowColor = 0xFFFF0000; m_settings.glowSecondaryColor = 0xFF880000;
        break;
    case MonacoThemePreset::Monokai:
        m_settings.backgroundColor = 0xFF272822; m_settings.foregroundColor = 0xFFF8F8F2;
        m_settings.keywordColor = 0xFFF92672; m_settings.stringColor = 0xFFE6DB74;
        m_settings.commentColor = 0xFF75715E; m_settings.functionColor = 0xFFA6E22E;
        m_settings.typeColor = 0xFF66D9EF; m_settings.numberColor = 0xFFAE81FF;
        m_settings.glowColor = 0xFFA6E22E; m_settings.glowSecondaryColor = 0xFF66D9EF;
        break;
    default: break; // Custom or other — keep current
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// Actions
// ═══════════════════════════════════════════════════════════════════════════════

void MonacoSettingsDialog::onColorButtonClicked(UINT /*id*/, uint32_t& colorRef)
{
    static COLORREF customColors[16]{};
    CHOOSECOLORW cc{};
    cc.lStructSize  = sizeof(cc);
    cc.hwndOwner    = m_hDlg;
    cc.rgbResult    = u32ToCR(colorRef);
    cc.lpCustColors = customColors;
    cc.Flags        = CC_FULLOPEN | CC_RGBINIT;

    if (ChooseColorW(&cc)) {
        colorRef = crToU32(cc.rgbResult);
        // Switch to Custom theme preset
        m_settings.themePresetIndex = static_cast<int>(MonacoThemePreset::Custom);
        SendMessage(m_hwndTheme, CB_SETCURSEL, m_settings.themePresetIndex, 0);
        InvalidateRect(m_hDlg, nullptr, TRUE);
    }
}

void MonacoSettingsDialog::onApply()
{
    updateSettingsFromUI();

    auto preset = static_cast<MonacoThemePreset>(m_settings.themePresetIndex);
    if (preset != MonacoThemePreset::Custom)
        applyThemePreset(preset);

    if (m_settingsCb) m_settingsCb(m_settings);
    if (m_themeCb)    m_themeCb(preset);

    DestroyWindow(m_hDlg); m_hDlg = nullptr; PostQuitMessage(0);
}

void MonacoSettingsDialog::onReset()
{
    m_settings = MonacoSettings{};
    updateUIFromSettings();
}

void MonacoSettingsDialog::onPreview()
{
    updateSettingsFromUI();
    if (m_previewCb) m_previewCb(m_settings);
}

void MonacoSettingsDialog::onExport()
{
    char file[MAX_PATH] = {};
    OPENFILENAMEA ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner   = m_hDlg;
    ofn.lpstrFilter = "Monaco Theme (*.monaco)\0*.monaco\0All Files\0*.*\0";
    ofn.lpstrFile   = file;
    ofn.nMaxFile    = MAX_PATH;
    ofn.Flags       = OFN_OVERWRITEPROMPT;
    ofn.lpstrDefExt = "monaco";

    if (GetSaveFileNameA(&ofn)) {
        updateSettingsFromUI();
        if (saveToFile(file))
            MessageBoxA(m_hDlg, "Theme exported successfully.", "Export", MB_OK | MB_ICONINFORMATION);
        else
            MessageBoxA(m_hDlg, "Failed to export theme.", "Export", MB_OK | MB_ICONERROR);
    }
}

void MonacoSettingsDialog::onImport()
{
    char file[MAX_PATH] = {};
    OPENFILENAMEA ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner   = m_hDlg;
    ofn.lpstrFilter = "Monaco Theme (*.monaco)\0*.monaco\0All Files\0*.*\0";
    ofn.lpstrFile   = file;
    ofn.nMaxFile    = MAX_PATH;
    ofn.Flags       = OFN_FILEMUSTEXIST;

    if (GetOpenFileNameA(&ofn)) {
        if (loadFromFile(file)) {
            updateUIFromSettings();
            MessageBoxA(m_hDlg, "Theme imported successfully.", "Import", MB_OK | MB_ICONINFORMATION);
        } else {
            MessageBoxA(m_hDlg, "Failed to import theme.", "Import", MB_OK | MB_ICONERROR);
        }
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// File persistence (simple INI-style)
// ═══════════════════════════════════════════════════════════════════════════════

bool MonacoSettingsDialog::saveToFile(const std::string& path)
{
    std::ofstream f(path);
    if (!f.is_open()) return false;

    f << "[Monaco]\n";
    f << "variant="       << m_settings.variantIndex     << "\n";
    f << "theme="         << m_settings.themePresetIndex  << "\n";
    f << "fontFamily="    << m_settings.fontFamily        << "\n";
    f << "fontSize="      << m_settings.fontSize          << "\n";
    f << "fontWeight="    << m_settings.fontWeight         << "\n";
    f << "fontLigatures=" << m_settings.fontLigatures      << "\n";
    f << "lineHeight="    << m_settings.lineHeight         << "\n";
    f << "wordWrap="      << m_settings.wordWrap           << "\n";
    f << "tabSize="       << m_settings.tabSize            << "\n";
    f << "insertSpaces="  << m_settings.insertSpaces       << "\n";
    f << "autoIndent="    << m_settings.autoIndent         << "\n";
    f << "bracketMatch="  << m_settings.bracketMatching    << "\n";
    f << "autoClose="     << m_settings.autoClosingBrackets<< "\n";
    f << "formatPaste="   << m_settings.formatOnPaste      << "\n";
    f << "neon="          << m_settings.neonEffects        << "\n";
    f << "glowIntensity=" << m_settings.glowIntensity      << "\n";
    f << "scanline="      << m_settings.scanlineDensity    << "\n";
    f << "glitch="        << m_settings.glitchProbability  << "\n";
    f << "particles="     << m_settings.particles          << "\n";
    f << "particleCount=" << m_settings.particleCount      << "\n";
    f << "esp="           << m_settings.espMode            << "\n";
    f << "espVar="        << m_settings.espHighlightVariables << "\n";
    f << "espFn="         << m_settings.espHighlightFunctions << "\n";
    f << "espWall="       << m_settings.espWallhack        << "\n";
    f << "minimap="       << m_settings.minimapEnabled     << "\n";
    f << "miniChar="      << m_settings.minimapRenderChars << "\n";
    f << "miniScale="     << m_settings.minimapScale       << "\n";
    f << "intelli="       << m_settings.intellisense       << "\n";
    f << "quickSug="      << m_settings.quickSuggestions   << "\n";
    f << "sugDelay="      << m_settings.suggestDelay       << "\n";
    f << "paramHint="     << m_settings.parameterHints     << "\n";
    f << "renderDelay="   << m_settings.renderDelay        << "\n";
    f << "vblankSync="    << m_settings.vblankSync         << "\n";
    f << "predFetch="     << m_settings.predictiveFetch    << "\n";
    f << "lazyTok="       << m_settings.lazyTokenization   << "\n";
    f << "lazyDelay="     << m_settings.lazyTokenDelay     << "\n";

    char hex[12];
    auto writeClr = [&](const char* key, uint32_t c) {
        _snprintf_s(hex, 12, _TRUNCATE, "0x%08X", c);
        f << key << "=" << hex << "\n";
    };
    writeClr("bgColor",   m_settings.backgroundColor);
    writeClr("fgColor",   m_settings.foregroundColor);
    writeClr("kwColor",   m_settings.keywordColor);
    writeClr("strColor",  m_settings.stringColor);
    writeClr("cmtColor",  m_settings.commentColor);
    writeClr("fnColor",   m_settings.functionColor);
    writeClr("typeColor", m_settings.typeColor);
    writeClr("numColor",  m_settings.numberColor);
    writeClr("glowColor", m_settings.glowColor);
    writeClr("glow2Color",m_settings.glowSecondaryColor);

    return f.good();
}

bool MonacoSettingsDialog::loadFromFile(const std::string& path)
{
    std::ifstream f(path);
    if (!f.is_open()) return false;

    std::string line;
    while (std::getline(f, line)) {
        if (line.empty() || line[0] == '[' || line[0] == '#') continue;
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = line.substr(0, eq);
        std::string val = line.substr(eq + 1);

        auto i = [&]{ return std::stoi(val); };
        auto b = [&]{ return val == "1" || val == "true"; };
        auto clr = [&]{ return static_cast<uint32_t>(std::stoul(val, nullptr, 16)); };

        if      (key == "variant")       m_settings.variantIndex     = i();
        else if (key == "theme")         m_settings.themePresetIndex = i();
        else if (key == "fontFamily")    m_settings.fontFamily       = val;
        else if (key == "fontSize")      m_settings.fontSize         = i();
        else if (key == "fontWeight")    m_settings.fontWeight       = i();
        else if (key == "fontLigatures") m_settings.fontLigatures    = b();
        else if (key == "lineHeight")    m_settings.lineHeight       = std::stof(val);
        else if (key == "wordWrap")      m_settings.wordWrap         = b();
        else if (key == "tabSize")       m_settings.tabSize          = i();
        else if (key == "insertSpaces")  m_settings.insertSpaces     = b();
        else if (key == "autoIndent")    m_settings.autoIndent       = b();
        else if (key == "bracketMatch")  m_settings.bracketMatching  = b();
        else if (key == "autoClose")     m_settings.autoClosingBrackets = b();
        else if (key == "formatPaste")   m_settings.formatOnPaste    = b();
        else if (key == "neon")          m_settings.neonEffects      = b();
        else if (key == "glowIntensity") m_settings.glowIntensity    = i();
        else if (key == "scanline")      m_settings.scanlineDensity  = i();
        else if (key == "glitch")        m_settings.glitchProbability = i();
        else if (key == "particles")     m_settings.particles        = b();
        else if (key == "particleCount") m_settings.particleCount    = i();
        else if (key == "esp")           m_settings.espMode          = b();
        else if (key == "espVar")        m_settings.espHighlightVariables = b();
        else if (key == "espFn")         m_settings.espHighlightFunctions = b();
        else if (key == "espWall")       m_settings.espWallhack      = b();
        else if (key == "minimap")       m_settings.minimapEnabled   = b();
        else if (key == "miniChar")      m_settings.minimapRenderChars = b();
        else if (key == "miniScale")     m_settings.minimapScale     = i();
        else if (key == "intelli")       m_settings.intellisense     = b();
        else if (key == "quickSug")      m_settings.quickSuggestions = b();
        else if (key == "sugDelay")      m_settings.suggestDelay     = i();
        else if (key == "paramHint")     m_settings.parameterHints   = b();
        else if (key == "renderDelay")   m_settings.renderDelay      = i();
        else if (key == "vblankSync")    m_settings.vblankSync       = b();
        else if (key == "predFetch")     m_settings.predictiveFetch  = i();
        else if (key == "lazyTok")       m_settings.lazyTokenization = b();
        else if (key == "lazyDelay")     m_settings.lazyTokenDelay   = i();
        else if (key == "bgColor")       m_settings.backgroundColor  = clr();
        else if (key == "fgColor")       m_settings.foregroundColor  = clr();
        else if (key == "kwColor")       m_settings.keywordColor     = clr();
        else if (key == "strColor")      m_settings.stringColor      = clr();
        else if (key == "cmtColor")      m_settings.commentColor     = clr();
        else if (key == "fnColor")       m_settings.functionColor    = clr();
        else if (key == "typeColor")     m_settings.typeColor        = clr();
        else if (key == "numColor")      m_settings.numberColor      = clr();
        else if (key == "glowColor")     m_settings.glowColor        = clr();
        else if (key == "glow2Color")    m_settings.glowSecondaryColor = clr();
    }
    m_originalSettings = m_settings;
    return true;
}

// ═══════════════════════════════════════════════════════════════════════════════
// Public getters/setters
// ═══════════════════════════════════════════════════════════════════════════════

MonacoSettings MonacoSettingsDialog::getSettings() const { return m_settings; }

void MonacoSettingsDialog::setSettings(const MonacoSettings& s)
{
    m_settings = s;
    m_originalSettings = s;
    if (m_hDlg && IsWindow(m_hDlg)) updateUIFromSettings();
}

} // namespace RawrXD::UI
