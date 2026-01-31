#pragma once

#include "../qtapp/settings.h"

namespace RawrXD::UI {

/**
 * MonacoSettingsDialog - Qt Dialog for Monaco Editor Settings
 * 
 * Provides a comprehensive settings panel for:
 * - Theme selection (12+ presets + custom)
 * - Variant selection (Core, Neon, ESP, Minimal, Enterprise)
 * - Font configuration
 * - Visual effects (glow, scanlines, particles)
 * - IntelliSense settings
 * - Performance tuning
 * 
 * All changes can be previewed live and are persisted to settings file.
 */
class MonacoSettingsDialog {public:
    explicit MonacoSettingsDialog(void* parent = nullptr);
    ~MonacoSettingsDialog() override;
    
    // Get/Set the settings being edited
    MonacoSettings getSettings() const { return settings_; }
    void setSettings(const MonacoSettings& settings);
    
    // Load/Save settings from/to file
    bool loadFromFile(const std::string& path);
    bool saveToFile(const std::string& path);
\npublic:\n    void settingsChanged(const MonacoSettings& settings);
    void themeChanged(MonacoThemePreset preset);
    void previewRequested(const MonacoSettings& settings);
\nprivate:\n    void onVariantChanged(int index);
    void onThemePresetChanged(int index);
    void onFontFamilyChanged(const void& font);
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

private:
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
    
    // Settings being edited
    MonacoSettings settings_;
    MonacoSettings originalSettings_;  // For reset functionality
    
    // Theme tab
    void* variantCombo_;
    void* themePresetCombo_;
    void* customColorsGroup_;
    void* backgroundColorBtn_;
    void* foregroundColorBtn_;
    void* keywordColorBtn_;
    void* stringColorBtn_;
    void* commentColorBtn_;
    void* functionColorBtn_;
    void* typeColorBtn_;
    void* numberColorBtn_;
    void* glowColorBtn_;
    void* glowSecondaryBtn_;
    
    // Font tab
    voidComboBox* fontFamilyCombo_;
    void* fontSizeSpin_;
    void* fontWeightCombo_;
    void* fontLigaturesCheck_;
    void* lineHeightSpin_;
    
    // Editor tab
    void* wordWrapCheck_;
    void* tabSizeSpin_;
    void* insertSpacesCheck_;
    void* autoIndentCheck_;
    void* bracketMatchingCheck_;
    void* autoClosingBracketsCheck_;
    void* formatOnPasteCheck_;
    
    // Neon effects tab
    void* neonEffectsCheck_;
    void* glowIntensitySlider_;
    void* glowIntensityLabel_;
    void* scanlineDensitySpin_;
    void* glitchProbabilitySpin_;
    void* particlesCheck_;
    void* particleCountSpin_;
    
    // ESP mode
    void* espModeGroup_;
    void* espModeCheck_;
    void* espHighlightVariablesCheck_;
    void* espHighlightFunctionsCheck_;
    void* espWallhackCheck_;
    
    // Minimap
    void* minimapEnabledCheck_;
    void* minimapRenderCharsCheck_;
    void* minimapScaleSpin_;
    
    // IntelliSense
    void* intellisenseCheck_;
    void* quickSuggestionsCheck_;
    void* suggestDelaySpin_;
    void* parameterHintsCheck_;
    
    // Performance
    void* renderDelaySpin_;
    void* vblankSyncCheck_;
    void* predictiveFetchSpin_;
    void* lazyTokenizationCheck_;
    void* lazyTokenDelaySpuin_;
    
    // Buttons
    void* applyBtn_;
    void* resetBtn_;
    void* previewBtn_;
    void* exportBtn_;
    void* importBtn_;
};

} // namespace RawrXD::UI





