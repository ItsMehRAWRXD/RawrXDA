#pragma once
/**
 * @file monaco_settings_dialog.h
 * @brief MonacoSettingsDialog — pure C++20/Win32 (zero Qt).
 *
 * Tabbed settings panel: Theme, Font, Editor, Neon/ESP, Performance.
 * Color picker via ChooseColor(), file I/O via GetSaveFileName/GetOpenFileName.
 * @copyright RawrXD IDE 2026
 */

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <commctrl.h>

#include <cstdint>
#include <string>
#include <functional>
#include <optional>

namespace RawrXD::UI {

// ─── Enums ──────────────────────────────────────────────────────────────────

enum class MonacoThemePreset : int {
    Default = 0,
    NeonCyberpunk,
    MatrixGreen,
    HackerRed,
    Monokai,
    SolarizedDark,
    SolarizedLight,
    OneDark,
    Dracula,
    GruvboxDark,
    Nord,
    Custom
};

// ─── Settings POD ───────────────────────────────────────────────────────────

struct MonacoSettings {
    // Variant
    int variantIndex       = 0;   // 0=Core, 1=NeonCore, 2=NeonHack, 3=Minimal, 4=Enterprise
    int themePresetIndex   = 0;   // maps to MonacoThemePreset

    // Font
    std::string fontFamily = "Consolas";
    int    fontSize        = 14;
    int    fontWeight      = 400;
    bool   fontLigatures   = true;
    float  lineHeight      = 1.5f;

    // Editor behaviour
    bool   wordWrap              = false;
    int    tabSize               = 4;
    bool   insertSpaces          = true;
    bool   autoIndent            = true;
    bool   bracketMatching       = true;
    bool   autoClosingBrackets   = true;
    bool   formatOnPaste         = false;

    // Neon / ESP
    bool   neonEffects           = false;
    int    glowIntensity         = 50;
    int    scanlineDensity       = 2;
    int    glitchProbability     = 0;
    bool   particles             = false;
    int    particleCount         = 30;
    bool   espMode               = false;
    bool   espHighlightVariables = true;
    bool   espHighlightFunctions = true;
    bool   espWallhack           = false;

    // Minimap
    bool   minimapEnabled        = true;
    bool   minimapRenderChars    = true;
    int    minimapScale          = 1;

    // IntelliSense
    bool   intellisense          = true;
    bool   quickSuggestions      = true;
    int    suggestDelay          = 200;
    bool   parameterHints        = true;

    // Performance
    int    renderDelay           = 16;
    bool   vblankSync            = true;
    int    predictiveFetch       = 16;
    bool   lazyTokenization      = true;
    int    lazyTokenDelay        = 50;

    // Colors (0xAARRGGBB — alpha always 0xFF)
    uint32_t backgroundColor     = 0xFF1E1E1E;
    uint32_t foregroundColor     = 0xFFD4D4D4;
    uint32_t keywordColor        = 0xFF569CD6;
    uint32_t stringColor         = 0xFFCE9178;
    uint32_t commentColor        = 0xFF6A9955;
    uint32_t functionColor       = 0xFFDCDCAA;
    uint32_t typeColor           = 0xFF4EC9B0;
    uint32_t numberColor         = 0xFFB5CEA8;
    uint32_t glowColor           = 0xFF00FF00;
    uint32_t glowSecondaryColor  = 0xFF008000;
};

// ─── Control IDs ────────────────────────────────────────────────────────────

constexpr UINT IDC_MS_TAB           = 3000;

// Theme tab
constexpr UINT IDC_MS_VARIANT       = 3010;
constexpr UINT IDC_MS_THEME         = 3011;
constexpr UINT IDC_MS_CLRBG         = 3020;
constexpr UINT IDC_MS_CLRFG         = 3021;
constexpr UINT IDC_MS_CLRKW         = 3022;
constexpr UINT IDC_MS_CLRSTR        = 3023;
constexpr UINT IDC_MS_CLRCMT        = 3024;
constexpr UINT IDC_MS_CLRFN         = 3025;
constexpr UINT IDC_MS_CLRTYPE       = 3026;
constexpr UINT IDC_MS_CLRNUM        = 3027;
constexpr UINT IDC_MS_CLRGLOW       = 3028;
constexpr UINT IDC_MS_CLRGLOW2      = 3029;

// Font tab
constexpr UINT IDC_MS_FONT_FAMILY   = 3040;
constexpr UINT IDC_MS_FONT_SIZE     = 3041;
constexpr UINT IDC_MS_FONT_WEIGHT   = 3042;
constexpr UINT IDC_MS_FONT_LIG      = 3043;
constexpr UINT IDC_MS_LINE_HEIGHT   = 3044;

// Editor tab
constexpr UINT IDC_MS_WORDWRAP      = 3050;
constexpr UINT IDC_MS_TABSIZE       = 3051;
constexpr UINT IDC_MS_SPACES        = 3052;
constexpr UINT IDC_MS_AUTOINDENT    = 3053;
constexpr UINT IDC_MS_BRACKET       = 3054;
constexpr UINT IDC_MS_AUTOCLOSE     = 3055;
constexpr UINT IDC_MS_FMTPASTE      = 3056;
constexpr UINT IDC_MS_MINIMAP       = 3057;
constexpr UINT IDC_MS_MINICHAR      = 3058;
constexpr UINT IDC_MS_MINISCALE     = 3059;
constexpr UINT IDC_MS_INTELLI       = 3060;
constexpr UINT IDC_MS_QUICKSUG      = 3061;
constexpr UINT IDC_MS_SUGDELAY      = 3062;
constexpr UINT IDC_MS_PARAMHINT     = 3063;

// Neon / ESP tab
constexpr UINT IDC_MS_NEON          = 3070;
constexpr UINT IDC_MS_GLOWSLIDER    = 3071;
constexpr UINT IDC_MS_GLOWLABEL     = 3072;
constexpr UINT IDC_MS_SCANLINE      = 3073;
constexpr UINT IDC_MS_GLITCH        = 3074;
constexpr UINT IDC_MS_PARTICLES     = 3075;
constexpr UINT IDC_MS_PARTCOUNT     = 3076;
constexpr UINT IDC_MS_ESP           = 3077;
constexpr UINT IDC_MS_ESPVAR        = 3078;
constexpr UINT IDC_MS_ESPFN         = 3079;
constexpr UINT IDC_MS_ESPWALL       = 3080;

// Performance tab
constexpr UINT IDC_MS_RENDERDEL     = 3090;
constexpr UINT IDC_MS_VSYNC         = 3091;
constexpr UINT IDC_MS_PREDFETCH     = 3092;
constexpr UINT IDC_MS_LAZYTOK       = 3093;
constexpr UINT IDC_MS_LAZYDELAY     = 3094;

// Buttons
constexpr UINT IDC_MS_APPLY         = 3100;
constexpr UINT IDC_MS_RESET         = 3101;
constexpr UINT IDC_MS_PREVIEW       = 3102;
constexpr UINT IDC_MS_EXPORT        = 3103;
constexpr UINT IDC_MS_IMPORT        = 3104;

// ─── Dialog class ───────────────────────────────────────────────────────────

class MonacoSettingsDialog {
public:
    explicit MonacoSettingsDialog(HWND parent = nullptr);
    ~MonacoSettingsDialog();

    /** Show as modal dialog; returns IDOK on Apply/OK, IDCANCEL otherwise. */
    INT_PTR showModal();

    /** Get / set the current settings snapshot. */
    MonacoSettings getSettings() const;
    void setSettings(const MonacoSettings& s);

    /** File persistence. */
    bool loadFromFile(const std::string& path);
    bool saveToFile(const std::string& path);

    /** Callbacks (replace Qt signals). */
    using SettingsCb = std::function<void(const MonacoSettings&)>;
    using ThemeCb    = std::function<void(MonacoThemePreset)>;
    void onSettingsChanged(SettingsCb cb)    { m_settingsCb = std::move(cb); }
    void onThemeChanged(ThemeCb cb)          { m_themeCb    = std::move(cb); }
    void onPreviewRequested(SettingsCb cb)   { m_previewCb  = std::move(cb); }

private:
    // ── Window procedure ────────────────────────────────────────────────
    static INT_PTR CALLBACK DlgProc(HWND, UINT, WPARAM, LPARAM);
    INT_PTR handleMsg(HWND, UINT, WPARAM, LPARAM);

    // ── Tab construction helpers ────────────────────────────────────────
    void initControls(HWND hDlg);
    void createThemeTab(HWND hTab, int page);
    void createFontTab(HWND hTab, int page);
    void createEditorTab(HWND hTab, int page);
    void createNeonTab(HWND hTab, int page);
    void createPerformanceTab(HWND hTab, int page);
    void showTabPage(int page);

    // ── Settings ↔ UI ───────────────────────────────────────────────────
    void updateUIFromSettings();
    void updateSettingsFromUI();
    void applyThemePreset(MonacoThemePreset preset);

    // ── Actions ─────────────────────────────────────────────────────────
    void onColorButtonClicked(UINT id, uint32_t& colorRef);
    void onApply();
    void onReset();
    void onPreview();
    void onExport();
    void onImport();

    // ── Data ────────────────────────────────────────────────────────────
    HWND m_hwndParent = nullptr;
    HWND m_hDlg       = nullptr;
    int  m_curPage    = 0;

    MonacoSettings m_settings;
    MonacoSettings m_originalSettings;

    SettingsCb m_settingsCb;
    ThemeCb    m_themeCb;
    SettingsCb m_previewCb;

    // Tab control
    HWND m_hwndTab    = nullptr;
    static constexpr int kNumPages = 5;
    HWND m_pages[kNumPages]{};   // container panels per tab

    // --- Theme controls ---
    HWND m_hwndVariant    = nullptr;
    HWND m_hwndTheme      = nullptr;
    HWND m_hwndClrBg      = nullptr;
    HWND m_hwndClrFg      = nullptr;
    HWND m_hwndClrKw      = nullptr;
    HWND m_hwndClrStr     = nullptr;
    HWND m_hwndClrCmt     = nullptr;
    HWND m_hwndClrFn      = nullptr;
    HWND m_hwndClrType    = nullptr;
    HWND m_hwndClrNum     = nullptr;
    HWND m_hwndClrGlow    = nullptr;
    HWND m_hwndClrGlow2   = nullptr;

    // --- Font controls ---
    HWND m_hwndFontFamily = nullptr;
    HWND m_hwndFontSize   = nullptr;
    HWND m_hwndFontWeight = nullptr;
    HWND m_hwndFontLig    = nullptr;
    HWND m_hwndLineHeight = nullptr;

    // --- Editor controls ---
    HWND m_hwndWordWrap       = nullptr;
    HWND m_hwndTabSize        = nullptr;
    HWND m_hwndSpaces         = nullptr;
    HWND m_hwndAutoIndent     = nullptr;
    HWND m_hwndBracket        = nullptr;
    HWND m_hwndAutoClose      = nullptr;
    HWND m_hwndFmtPaste       = nullptr;
    HWND m_hwndMinimap        = nullptr;
    HWND m_hwndMiniChar       = nullptr;
    HWND m_hwndMiniScale      = nullptr;
    HWND m_hwndIntelli        = nullptr;
    HWND m_hwndQuickSug       = nullptr;
    HWND m_hwndSugDelay       = nullptr;
    HWND m_hwndParamHint      = nullptr;

    // --- Neon/ESP controls ---
    HWND m_hwndNeon           = nullptr;
    HWND m_hwndGlowSlider    = nullptr;
    HWND m_hwndGlowLabel     = nullptr;
    HWND m_hwndScanline      = nullptr;
    HWND m_hwndGlitch        = nullptr;
    HWND m_hwndParticles     = nullptr;
    HWND m_hwndPartCount     = nullptr;
    HWND m_hwndEsp           = nullptr;
    HWND m_hwndEspVar        = nullptr;
    HWND m_hwndEspFn         = nullptr;
    HWND m_hwndEspWall       = nullptr;

    // --- Performance controls ---
    HWND m_hwndRenderDel     = nullptr;
    HWND m_hwndVSync         = nullptr;
    HWND m_hwndPredFetch     = nullptr;
    HWND m_hwndLazyTok       = nullptr;
    HWND m_hwndLazyDelay     = nullptr;

    // --- Bottom buttons ---
    HWND m_hwndApply         = nullptr;
    HWND m_hwndReset         = nullptr;
    HWND m_hwndPreview       = nullptr;
    HWND m_hwndExport        = nullptr;
    HWND m_hwndImport        = nullptr;
};

} // namespace RawrXD::UI
