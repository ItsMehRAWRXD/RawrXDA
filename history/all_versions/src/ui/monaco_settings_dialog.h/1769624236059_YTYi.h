#pragma once

#include <QDialog>
#include <QComboBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QSlider>
#include <QLabel>
#include <QGroupBox>
#include <QTabWidget>
#include <QPushButton>
#include <QLineEdit>
#include <QFontComboBox>
#include <QColorDialog>
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
class MonacoSettingsDialog : public QDialog {
    Q_OBJECT

public:
    explicit MonacoSettingsDialog(QWidget* parent = nullptr);
    ~MonacoSettingsDialog() override;
    
    // Get/Set the settings being edited
    MonacoSettings getSettings() const { return settings_; }
    void setSettings(const MonacoSettings& settings);
    
    // Load/Save settings from/to file
    bool loadFromFile(const QString& path);
    bool saveToFile(const QString& path);

signals:
    void settingsChanged(const MonacoSettings& settings);
    void themeChanged(MonacoThemePreset preset);
    void previewRequested(const MonacoSettings& settings);

private slots:
    void onVariantChanged(int index);
    void onThemePresetChanged(int index);
    void onFontFamilyChanged(const QFont& font);
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
    void createThemeTab(QTabWidget* tabs);
    void createFontTab(QTabWidget* tabs);
    void createEditorTab(QTabWidget* tabs);
    void createNeonTab(QTabWidget* tabs);
    void createPerformanceTab(QTabWidget* tabs);
    void createColorPicker(QGroupBox* group, const QString& label, uint32_t& colorRef);
    void updateColorButton(QPushButton* button, uint32_t color);
    void updateUIFromSettings();
    void updateSettingsFromUI();
    void applyThemePreset(MonacoThemePreset preset);
    
    // Settings being edited
    MonacoSettings settings_;
    MonacoSettings originalSettings_;  // For reset functionality
    
    // Theme tab
    QComboBox* variantCombo_;
    QComboBox* themePresetCombo_;
    QGroupBox* customColorsGroup_;
    QPushButton* backgroundColorBtn_;
    QPushButton* foregroundColorBtn_;
    QPushButton* keywordColorBtn_;
    QPushButton* stringColorBtn_;
    QPushButton* commentColorBtn_;
    QPushButton* functionColorBtn_;
    QPushButton* typeColorBtn_;
    QPushButton* numberColorBtn_;
    QPushButton* glowColorBtn_;
    QPushButton* glowSecondaryBtn_;
    
    // Font tab
    QFontComboBox* fontFamilyCombo_;
    QSpinBox* fontSizeSpin_;
    QComboBox* fontWeightCombo_;
    QCheckBox* fontLigaturesCheck_;
    QSpinBox* lineHeightSpin_;
    
    // Editor tab
    QCheckBox* wordWrapCheck_;
    QSpinBox* tabSizeSpin_;
    QCheckBox* insertSpacesCheck_;
    QCheckBox* autoIndentCheck_;
    QCheckBox* bracketMatchingCheck_;
    QCheckBox* autoClosingBracketsCheck_;
    QCheckBox* formatOnPasteCheck_;
    
    // Neon effects tab
    QCheckBox* neonEffectsCheck_;
    QSlider* glowIntensitySlider_;
    QLabel* glowIntensityLabel_;
    QSpinBox* scanlineDensitySpin_;
    QSpinBox* glitchProbabilitySpin_;
    QCheckBox* particlesCheck_;
    QSpinBox* particleCountSpin_;
    
    // ESP mode
    QGroupBox* espModeGroup_;
    QCheckBox* espModeCheck_;
    QCheckBox* espHighlightVariablesCheck_;
    QCheckBox* espHighlightFunctionsCheck_;
    QCheckBox* espWallhackCheck_;
    
    // Minimap
    QCheckBox* minimapEnabledCheck_;
    QCheckBox* minimapRenderCharsCheck_;
    QSpinBox* minimapScaleSpin_;
    
    // IntelliSense
    QCheckBox* intellisenseCheck_;
    QCheckBox* quickSuggestionsCheck_;
    QSpinBox* suggestDelaySpin_;
    QCheckBox* parameterHintsCheck_;
    
    // Performance
    QSpinBox* renderDelaySpin_;
    QCheckBox* vblankSyncCheck_;
    QSpinBox* predictiveFetchSpin_;
    QCheckBox* lazyTokenizationCheck_;
    QSpinBox* lazyTokenDelaySpuin_;
    
    // Buttons
    QPushButton* applyBtn_;
    QPushButton* resetBtn_;
    QPushButton* previewBtn_;
    QPushButton* exportBtn_;
    QPushButton* importBtn_;
};

} // namespace RawrXD::UI
