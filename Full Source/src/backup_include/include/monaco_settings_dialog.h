#pragma once

// ============================================================================
// MonacoSettingsDialog — C++20, Win32/native. No Qt. (qtapp/settings.h removed)
// ============================================================================

#include <cstdint>
#include <string>

namespace RawrXD::UI {

/** Theme preset identifiers for Monaco editor (no Qt) */
enum class MonacoThemePreset : int {
    Default = 0,
    NeonCyberpunk,
    MatrixGreen,
    HackerRed,
    Monokai,
    Custom
};

/** Monaco editor settings — pure C++ (replaces Qt-based settings) */
struct MonacoSettings {
    int variantIndex = 0;       // Core, Neon, ESP, Minimal, Enterprise
    int themePresetIndex = 0;   // maps to MonacoThemePreset
    std::string fontFamily = "Consolas";
    int fontSize = 14;
    int fontWeight = 400;
    bool fontLigatures = true;
    float lineHeight = 1.5f;
    bool wordWrap = false;
    int tabSize = 4;
    bool insertSpaces = true;
    bool autoIndent = true;
    bool bracketMatching = true;
    bool autoClosingBrackets = true;
    bool formatOnPaste = false;
    bool neonEffects = false;
    int glowIntensity = 50;
    int scanlineDensity = 2;
    float glitchProbability = 0.0f;
    bool particles = false;
    int particleCount = 30;
    bool espMode = false;
    bool espHighlightVariables = true;
    bool espHighlightFunctions = true;
    bool espWallhack = false;
    bool minimapEnabled = true;
    bool minimapRenderChars = true;
    float minimapScale = 1.0f;
    bool intellisense = true;
    bool quickSuggestions = true;
    int suggestDelay = 200;
    bool parameterHints = true;
    int renderDelay = 0;
    bool vblankSync = false;
    int predictiveFetch = 0;
    bool lazyTokenization = false;
    int lazyTokenDelay = 50;
    uint32_t backgroundColor = 0xFF1E1E1E;
    uint32_t foregroundColor = 0xFFD4D4D4;
    uint32_t keywordColor = 0xFF569CD6;
    uint32_t stringColor = 0xFFCE9178;
    uint32_t commentColor = 0xFF6A9955;
    uint32_t functionColor = 0xFFDCDCAA;
    uint32_t typeColor = 0xFF4EC9B0;
    uint32_t numberColor = 0xFFB5CEA8;
    uint32_t glowColor = 0xFF00FF00;
    uint32_t glowSecondaryColor = 0xFF008000;
};

/**
 * MonacoSettingsDialog — Win32/native settings panel for Monaco Editor
 *
 * Provides: theme selection, variant, font, neon/ESP/minimap/IntelliSense,
 * performance tuning. Persisted to file. No Qt.
 */
class MonacoSettingsDialog {
public:
    explicit MonacoSettingsDialog(void* parent = nullptr);
    ~MonacoSettingsDialog();

    MonacoSettings getSettings() const { return settings_; }
    void setSettings(const MonacoSettings& settings);

    bool loadFromFile(const std::string& path);
    bool saveToFile(const std::string& path);

    void settingsChanged(const MonacoSettings& settings);
    void themeChanged(MonacoThemePreset preset);
    void previewRequested(const MonacoSettings& settings);

private:
    void onVariantChanged(int index);
    void onThemePresetChanged(int index);
    void onFontFamilyChanged(int fontIndex);
    void onFontSizeChanged(int size);
    void onFontWeightChanged(int weight);
    void onColorButtonClicked();
    void onNeonEffectsToggled(bool enabled);
    void onGlowIntensityChanged(int value);
    void onESPModeToggled(bool enabled);
    void onMinimapToggled(bool enabled);
    void onIntelliSenseToggled(bool enabled);
    void onApplyClicked();
    void onResetToDefaultClicked();
    void onPreviewClicked();
    void onExportThemeClicked();
    void onImportThemeClicked();

    void setupUI();
    void createThemeTab(void* tabs);
    void createFontTab(void* tabs);
    void createEditorTab(void* tabs);
    void createNeonTab(void* tabs);
    void createPerformanceTab(void* tabs);
    void createColorPicker(void* group, const std::string& label, uint32_t& colorRef);
    void updateColorButton(void* button, uint32_t color);
    void updateUIFromSettings();
    void updateSettingsFromUI();
    void applyThemePreset(MonacoThemePreset preset);

    MonacoSettings settings_;
    MonacoSettings originalSettings_;

    void* variantCombo_ = nullptr;
    void* themePresetCombo_ = nullptr;
    void* customColorsGroup_ = nullptr;
    void* backgroundColorBtn_ = nullptr;
    void* foregroundColorBtn_ = nullptr;
    void* keywordColorBtn_ = nullptr;
    void* stringColorBtn_ = nullptr;
    void* commentColorBtn_ = nullptr;
    void* functionColorBtn_ = nullptr;
    void* typeColorBtn_ = nullptr;
    void* numberColorBtn_ = nullptr;
    void* glowColorBtn_ = nullptr;
    void* glowSecondaryBtn_ = nullptr;
    void* fontFamilyCombo_ = nullptr;
    void* fontSizeSpin_ = nullptr;
    void* fontWeightCombo_ = nullptr;
    void* fontLigaturesCheck_ = nullptr;
    void* lineHeightSpin_ = nullptr;
    void* wordWrapCheck_ = nullptr;
    void* tabSizeSpin_ = nullptr;
    void* insertSpacesCheck_ = nullptr;
    void* autoIndentCheck_ = nullptr;
    void* bracketMatchingCheck_ = nullptr;
    void* autoClosingBracketsCheck_ = nullptr;
    void* formatOnPasteCheck_ = nullptr;
    void* neonEffectsCheck_ = nullptr;
    void* glowIntensitySlider_ = nullptr;
    void* glowIntensityLabel_ = nullptr;
    void* scanlineDensitySpin_ = nullptr;
    void* glitchProbabilitySpin_ = nullptr;
    void* particlesCheck_ = nullptr;
    void* particleCountSpin_ = nullptr;
    void* espModeGroup_ = nullptr;
    void* espModeCheck_ = nullptr;
    void* espHighlightVariablesCheck_ = nullptr;
    void* espHighlightFunctionsCheck_ = nullptr;
    void* espWallhackCheck_ = nullptr;
    void* minimapEnabledCheck_ = nullptr;
    void* minimapRenderCharsCheck_ = nullptr;
    void* minimapScaleSpin_ = nullptr;
    void* intellisenseCheck_ = nullptr;
    void* quickSuggestionsCheck_ = nullptr;
    void* suggestDelaySpin_ = nullptr;
    void* parameterHintsCheck_ = nullptr;
    void* renderDelaySpin_ = nullptr;
    void* vblankSyncCheck_ = nullptr;
    void* predictiveFetchSpin_ = nullptr;
    void* lazyTokenizationCheck_ = nullptr;
    void* lazyTokenDelaySpin_ = nullptr;
    void* applyBtn_ = nullptr;
    void* resetBtn_ = nullptr;
    void* previewBtn_ = nullptr;
    void* exportBtn_ = nullptr;
    void* importBtn_ = nullptr;
};

} // namespace RawrXD::UI
