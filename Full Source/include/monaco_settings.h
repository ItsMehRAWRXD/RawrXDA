#pragma once
/**
 * @file monaco_settings.h
 * @brief Monaco editor settings — C++20 / Win32, no Qt.
 * Replaces qtapp/settings.h for MonacoSettings, MonacoThemePreset, MonacoThemeColors.
 */

#include <cstdint>
#include <string>

namespace RawrXD {

// Theme preset enum (matches IDE theme dropdown)
enum class MonacoThemePreset : int {
    Default = 0,        // VS Code Dark+
    NeonCyberpunk = 1,
    MatrixGreen = 2,
    HackerRed = 3,
    Monokai = 4,
    SolarizedDark = 5,
    SolarizedLight = 6,
    OneDark = 7,
    Dracula = 8,
    GruvboxDark = 9,
    Nord = 10,
    Custom = 255
};

// Variant type for MonacoSettings.variant (matches MonacoVariant in MonacoIntegration.hpp)
enum class MonacoVariantType : int {
    Core = 0,
    NeonCore = 1,
    NeonHack = 2,
    ZeroDependency = 3,
    Enterprise = 4
};

// Theme colors (BGRA or ARGB as uint32_t)
struct MonacoThemeColors {
    uint32_t background = 0x001E1E1E;
    uint32_t foreground = 0x00D4D4D4;
    uint32_t lineNumber = 0x00858585;
    uint32_t cursorColor = 0x00AEAFAD;
    uint32_t selectionBg = 0x00264F78;
    uint32_t lineHighlight = 0x00282828;
    uint32_t keyword = 0x00569CD6;
    uint32_t string = 0x00CE9178;
    uint32_t number = 0x00B5CEA8;
    uint32_t comment = 0x006A9955;
    uint32_t function = 0x00DCDCAA;
    uint32_t type = 0x004EC9B0;
    uint32_t variable = 0x009CDCFE;
    uint32_t operator_ = 0x00D4D4D4;
    uint32_t preprocessor = 0x00C586C0;
    uint32_t constant = 0x004FC1FF;
    uint32_t glowColor = 0x0000FFFF;
    uint32_t glowSecondary = 0x00FF00FF;
    uint32_t scanlineColor = 0;
    uint32_t particleColor = 0;
};

// Full Monaco editor settings (no Qt types)
struct MonacoSettings {
    MonacoVariantType variant = MonacoVariantType::Core;
    MonacoThemePreset themePreset = MonacoThemePreset::Default;
    MonacoThemeColors colors;

    std::string fontFamily = "Consolas";
    int fontSize = 14;
    int lineHeight = 0;
    bool fontLigatures = true;
    int fontWeight = 400;

    bool wordWrap = false;
    int tabSize = 4;
    bool insertSpaces = true;
    bool autoIndent = true;
    bool bracketMatching = true;
    bool autoClosingBrackets = true;
    bool autoClosingQuotes = true;
    bool formatOnPaste = false;
    bool formatOnType = false;

    bool enableIntelliSense = false;
    bool quickSuggestions = true;
    int suggestDelay = 50;
    bool parameterHints = true;

    bool enableNeonEffects = false;
    int glowIntensity = 8;
    int scanlineDensity = 2;
    int glitchProbability = 3;
    bool particlesEnabled = true;
    int particleCount = 1024;

    bool enableESPMode = false;
    bool espHighlightVariables = true;
    bool espHighlightFunctions = true;
    bool espWallhackSymbols = false;

    bool minimapEnabled = true;
    bool minimapRenderCharacters = true;
    int minimapScale = 1;
    bool minimapShowSlider = true;

    bool enableDebugging = false;
    bool inlineDebugging = true;
    bool breakpointGutter = true;

    int renderDelay = 16;
    bool vblankSync = true;
    int predictiveFetchLines = 16;
    bool lazyTokenization = false;
    int lazyTokenizationDelay = 100;

    bool dirty = false;
};

/** Returns theme colors for a preset. No Qt, no Settings class. */
inline MonacoThemeColors GetMonacoThemePresetColors(MonacoThemePreset preset) {
    MonacoThemeColors colors;
    switch (preset) {
        case MonacoThemePreset::Default:
            colors.background = 0x001E1E1E;
            colors.foreground = 0x00D4D4D4;
            colors.lineNumber = 0x00858585;
            colors.cursorColor = 0x00AEAFAD;
            colors.selectionBg = 0x00264F78;
            colors.lineHighlight = 0x00282828;
            colors.keyword = 0x00569CD6;
            colors.string = 0x00CE9178;
            colors.number = 0x00B5CEA8;
            colors.comment = 0x006A9955;
            colors.function = 0x00DCDCAA;
            colors.type = 0x004EC9B0;
            colors.variable = 0x009CDCFE;
            colors.operator_ = 0x00D4D4D4;
            colors.preprocessor = 0x00C586C0;
            colors.constant = 0x004FC1FF;
            colors.glowColor = 0x0000FFFF;
            colors.glowSecondary = 0x00FF00FF;
            break;
        case MonacoThemePreset::NeonCyberpunk:
            colors.background = 0x00080808;
            colors.foreground = 0x0000FF99;
            colors.lineNumber = 0x00FF00FF;
            colors.cursorColor = 0x0000FFFF;
            colors.selectionBg = 0x00330066;
            colors.lineHighlight = 0x00111122;
            colors.keyword = 0x00FF00FF;
            colors.string = 0x0000FF99;
            colors.number = 0x00FFFF00;
            colors.comment = 0x00666699;
            colors.function = 0x0000FFFF;
            colors.type = 0x00FF6699;
            colors.variable = 0x0099FFFF;
            colors.operator_ = 0x00FFFFFF;
            colors.preprocessor = 0x00FF9900;
            colors.constant = 0x00FFFF00;
            colors.glowColor = 0x0000FFFF;
            colors.glowSecondary = 0x00FF00FF;
            colors.scanlineColor = 0x00101010;
            colors.particleColor = 0x0000FF66;
            break;
        case MonacoThemePreset::MatrixGreen:
            colors.background = 0x00000000;
            colors.foreground = 0x0000FF00;
            colors.lineNumber = 0x00006600;
            colors.cursorColor = 0x0000FF00;
            colors.selectionBg = 0x00003300;
            colors.lineHighlight = 0x00001100;
            colors.keyword = 0x0066FF66;
            colors.string = 0x0000CC00;
            colors.number = 0x0033FF33;
            colors.comment = 0x00336633;
            colors.function = 0x0099FF99;
            colors.type = 0x0000FF66;
            colors.variable = 0x0000DD00;
            colors.operator_ = 0x0000FF00;
            colors.preprocessor = 0x0066FF00;
            colors.constant = 0x0033FF99;
            colors.glowColor = 0x0000FF00;
            colors.glowSecondary = 0x0033FF33;
            colors.scanlineColor = 0x00050505;
            colors.particleColor = 0x0000FF00;
            break;
        case MonacoThemePreset::HackerRed:
            colors.background = 0x00100000;
            colors.foreground = 0x00FF3333;
            colors.lineNumber = 0x00993333;
            colors.cursorColor = 0x00FF6600;
            colors.selectionBg = 0x00330000;
            colors.lineHighlight = 0x00200000;
            colors.keyword = 0x00FF6666;
            colors.string = 0x00FF9966;
            colors.number = 0x00FFCC00;
            colors.comment = 0x00663333;
            colors.function = 0x00FF9999;
            colors.type = 0x00FF6600;
            colors.variable = 0x00FF4444;
            colors.operator_ = 0x00FF3333;
            colors.preprocessor = 0x00FFAA00;
            colors.constant = 0x00FFDD00;
            colors.glowColor = 0x00FF0000;
            colors.glowSecondary = 0x00FF6600;
            break;
        case MonacoThemePreset::Monokai:
            colors.background = 0x00272822;
            colors.foreground = 0x00F8F8F2;
            colors.lineNumber = 0x0090908A;
            colors.cursorColor = 0x00F8F8F0;
            colors.selectionBg = 0x0049483E;
            colors.lineHighlight = 0x003E3D32;
            colors.keyword = 0x00F92672;
            colors.string = 0x00E6DB74;
            colors.number = 0x00AE81FF;
            colors.comment = 0x0075715E;
            colors.function = 0x00A6E22E;
            colors.type = 0x0066D9EF;
            colors.variable = 0x00F8F8F2;
            colors.operator_ = 0x00F92672;
            colors.preprocessor = 0x00AE81FF;
            colors.constant = 0x00AE81FF;
            break;
        case MonacoThemePreset::SolarizedDark:
            colors.background = 0x00002B36;
            colors.foreground = 0x00839496;
            colors.lineNumber = 0x00586E75;
            colors.cursorColor = 0x00819090;
            colors.selectionBg = 0x00073642;
            colors.lineHighlight = 0x00073642;
            colors.keyword = 0x00859900;
            colors.string = 0x002AA198;
            colors.number = 0x00D33682;
            colors.comment = 0x00586E75;
            colors.function = 0x00268BD2;
            colors.type = 0x00B58900;
            colors.variable = 0x00839496;
            colors.operator_ = 0x00859900;
            colors.preprocessor = 0x00CB4B16;
            colors.constant = 0x00D33682;
            break;
        case MonacoThemePreset::SolarizedLight:
            colors.background = 0x00FDF6E3;
            colors.foreground = 0x00657B83;
            colors.lineNumber = 0x0093A1A1;
            colors.cursorColor = 0x00586E75;
            colors.selectionBg = 0x00EEE8D5;
            colors.lineHighlight = 0x00EEE8D5;
            colors.keyword = 0x00859900;
            colors.string = 0x002AA198;
            colors.number = 0x00D33682;
            colors.comment = 0x0093A1A1;
            colors.function = 0x00268BD2;
            colors.type = 0x00B58900;
            colors.variable = 0x00657B83;
            colors.operator_ = 0x00859900;
            colors.preprocessor = 0x00CB4B16;
            colors.constant = 0x00D33682;
            break;
        case MonacoThemePreset::OneDark:
            colors.background = 0x00282C34;
            colors.foreground = 0x00ABB2BF;
            colors.lineNumber = 0x00636D83;
            colors.cursorColor = 0x00528BFF;
            colors.selectionBg = 0x003E4451;
            colors.lineHighlight = 0x002C313C;
            colors.keyword = 0x00C678DD;
            colors.string = 0x0098C379;
            colors.number = 0x00D19A66;
            colors.comment = 0x005C6370;
            colors.function = 0x0061AFEF;
            colors.type = 0x00E5C07B;
            colors.variable = 0x00E06C75;
            colors.operator_ = 0x0056B6C2;
            colors.preprocessor = 0x00C678DD;
            colors.constant = 0x00D19A66;
            break;
        case MonacoThemePreset::Dracula:
            colors.background = 0x00282A36;
            colors.foreground = 0x00F8F8F2;
            colors.lineNumber = 0x006272A4;
            colors.cursorColor = 0x00F8F8F2;
            colors.selectionBg = 0x0044475A;
            colors.lineHighlight = 0x0044475A;
            colors.keyword = 0x00FF79C6;
            colors.string = 0x00F1FA8C;
            colors.number = 0x00BD93F9;
            colors.comment = 0x006272A4;
            colors.function = 0x0050FA7B;
            colors.type = 0x008BE9FD;
            colors.variable = 0x00F8F8F2;
            colors.operator_ = 0x00FF79C6;
            colors.preprocessor = 0x00BD93F9;
            colors.constant = 0x00BD93F9;
            break;
        case MonacoThemePreset::GruvboxDark:
            colors.background = 0x00282828;
            colors.foreground = 0x00EBDBB2;
            colors.lineNumber = 0x00928374;
            colors.cursorColor = 0x00EBDBB2;
            colors.selectionBg = 0x00504945;
            colors.lineHighlight = 0x003C3836;
            colors.keyword = 0x00FB4934;
            colors.string = 0x00B8BB26;
            colors.number = 0x00D3869B;
            colors.comment = 0x00928374;
            colors.function = 0x00FABD2F;
            colors.type = 0x0083A598;
            colors.variable = 0x00EBDBB2;
            colors.operator_ = 0x00EBDBB2;
            colors.preprocessor = 0x00FE8019;
            colors.constant = 0x00D3869B;
            break;
        case MonacoThemePreset::Nord:
            colors.background = 0x002E3440;
            colors.foreground = 0x00D8DEE9;
            colors.lineNumber = 0x004C566A;
            colors.cursorColor = 0x00D8DEE9;
            colors.selectionBg = 0x00434C5E;
            colors.lineHighlight = 0x003B4252;
            colors.keyword = 0x0081A1C1;
            colors.string = 0x00A3BE8C;
            colors.number = 0x00B48EAD;
            colors.comment = 0x00616E88;
            colors.function = 0x0088C0D0;
            colors.type = 0x008FBCBB;
            colors.variable = 0x00D8DEE9;
            colors.operator_ = 0x0081A1C1;
            colors.preprocessor = 0x00D08770;
            colors.constant = 0x00B48EAD;
            break;
        default: // Custom — leave defaults
            break;
    }
    return colors;
}

} // namespace RawrXD
