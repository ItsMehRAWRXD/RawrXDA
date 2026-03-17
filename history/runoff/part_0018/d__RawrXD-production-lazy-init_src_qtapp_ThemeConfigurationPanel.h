// RawrXD Agentic IDE - Theme Configuration Panel
// UI for customizing themes with color pickers and opacity controls
#pragma once

#include <QWidget>
#include <QColor>
#include <QJsonObject>
#include <QMap>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QCheckBox>
#include <QGroupBox>
#include <QTabWidget>
#include <QLabel>

namespace RawrXD {

/**
 * @brief ColorPickerButton - Custom button that shows/edits a color
 */
class ColorPickerButton : public QPushButton {
    Q_OBJECT
    
public:
    explicit ColorPickerButton(const QColor& initialColor, QWidget* parent = nullptr);
    
    QColor currentColor() const { return m_color; }
    void setCurrentColor(const QColor& color);
    
signals:
    void colorChanged(const QColor& color);
    
private slots:
    void onClicked();
    
private:
    QColor m_color;
    void updateButtonStyle();
};

/**
 * @brief ThemeConfigurationPanel - Full theme customization UI
 * 
 * Features:
 * - Theme selection dropdown
 * - Color pickers for all theme elements
 * - Opacity sliders for window/dock/chat/editor
 * - Transparency and window management controls
 * - Save/Import/Export theme functionality
 * - Real-time preview
 */
class ThemeConfigurationPanel : public QWidget {
    Q_OBJECT
    
public:
    explicit ThemeConfigurationPanel(QWidget* parent = nullptr);
    
signals:
    void themeChanged();
    void colorsUpdated();
    void opacityChanged(const QString& element, double opacity);
    void transparencySettingsChanged();
    
private slots:
    void onThemeSelected(int index);
    void onColorChanged(const QString& colorName, const QColor& color);
    void onOpacityChanged(const QString& element, double opacity);
    void onTransparencyToggled(bool enabled);
    void onAlwaysOnTopToggled(bool enabled);
    void onClickThroughToggled(bool enabled);
    
    void saveCurrentTheme();
    void importTheme();
    void exportTheme();
    void resetToDefaults();
    void applyTheme();
    
private:
    void setupUI();
    void createEditorColorsTab(QWidget* parent);
    void createSyntaxColorsTab(QWidget* parent);
    void createChatColorsTab(QWidget* parent);
    void createUIColorsTab(QWidget* parent);
    void createOpacityTab(QWidget* parent);
    void createTransparencyTab(QWidget* parent);
    void createLanguageTab(QWidget* parent);
    void connectSignals();
    void loadCurrentTheme();
    void updatePreview();
    
    // Helper to create a color picker row
    QWidget* createColorRow(const QString& label, const QString& colorName, QWidget* parent);
    
    // UI Elements
    QComboBox* m_themeCombo;
    QTabWidget* m_tabWidget;
    
    // Color Pickers (stored by color name)
    QMap<QString, ColorPickerButton*> m_colorPickers;
    
    // Opacity Controls
    QSlider* m_windowOpacitySlider;
    QSlider* m_dockOpacitySlider;
    QSlider* m_chatOpacitySlider;
    QSlider* m_editorOpacitySlider;
    
    QDoubleSpinBox* m_windowOpacitySpin;
    QDoubleSpinBox* m_dockOpacitySpin;
    QDoubleSpinBox* m_chatOpacitySpin;
    QDoubleSpinBox* m_editorOpacitySpin;
    
    // Transparency Controls
    QCheckBox* m_transparencyEnabled;
    QCheckBox* m_alwaysOnTop;
    QCheckBox* m_clickThroughEnabled;
    
    // Action Buttons
    QPushButton* m_saveButton;
    QPushButton* m_importButton;
    QPushButton* m_exportButton;
    QPushButton* m_resetButton;
    QPushButton* m_applyButton;
    
    // Preview Widget
    QWidget* m_previewWidget;
    QLabel* m_previewLabel;

    struct LanguageOpacityControl {
        QSlider* slider = nullptr;
        QDoubleSpinBox* spin = nullptr;
    };
    QMap<QString, LanguageOpacityControl> m_languageOpacity;
};

} // namespace RawrXD
