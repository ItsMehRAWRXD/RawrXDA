// RawrXD Agentic IDE - Theme Configuration Panel Implementation
// Enterprise-grade theme customization with observability
#include "ThemeConfigurationPanel.h"
#include "ThemeManager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QSlider>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QPushButton>
#include <QCheckBox>
#include <QTabWidget>
#include <QColorDialog>
#include <QFileDialog>
#include <QInputDialog>
#include <QMessageBox>
#include <QScrollArea>
#include <QDebug>
#include <chrono>

namespace RawrXD {

// ============================================================
// ColorPickerButton Implementation
// ============================================================

ColorPickerButton::ColorPickerButton(const QColor& initialColor, QWidget* parent)
    : QPushButton(parent)
    , m_color(initialColor) {
    setFixedSize(50, 28);
    setCursor(Qt::PointingHandCursor);
    updateButtonStyle();
    connect(this, &QPushButton::clicked, this, &ColorPickerButton::onClicked);
}

void ColorPickerButton::setCurrentColor(const QColor& color) {
    if (m_color != color) {
        m_color = color;
        updateButtonStyle();
    }
}

void ColorPickerButton::onClicked() {
    QColorDialog dialog(m_color, this);
    dialog.setWindowTitle("Select Color");
    dialog.setOption(QColorDialog::ShowAlphaChannel, true);
    
    if (dialog.exec() == QDialog::Accepted) {
        QColor newColor = dialog.selectedColor();
        if (newColor.isValid() && newColor != m_color) {
            m_color = newColor;
            updateButtonStyle();
            emit colorChanged(m_color);
        }
    }
}

void ColorPickerButton::updateButtonStyle() {
    // Calculate contrasting border color
    int luminance = (m_color.red() * 299 + m_color.green() * 587 + m_color.blue() * 114) / 1000;
    QColor borderColor = luminance > 128 ? m_color.darker(150) : m_color.lighter(150);
    
    setStyleSheet(QString(R"(
        QPushButton {
            background-color: %1;
            border: 2px solid %2;
            border-radius: 4px;
        }
        QPushButton:hover {
            border: 2px solid %3;
        }
        QPushButton:pressed {
            border: 3px solid %3;
        }
    )")
    .arg(m_color.name(QColor::HexArgb))
    .arg(borderColor.name())
    .arg(m_color.lighter(130).name()));
}

// ============================================================
// ThemeConfigurationPanel Implementation
// ============================================================

ThemeConfigurationPanel::ThemeConfigurationPanel(QWidget* parent)
    : QWidget(parent)
    , m_previewWidget(nullptr)
    , m_previewLabel(nullptr) {
    
    qDebug() << "[ThemeConfigurationPanel] Initializing...";
    auto startTime = std::chrono::steady_clock::now();
    
    setupUI();
    loadCurrentTheme();
    connectSignals();
    
    auto endTime = std::chrono::steady_clock::now();
    auto durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
    qDebug() << "[ThemeConfigurationPanel] Initialized in" << durationMs << "ms";
}

void ThemeConfigurationPanel::setupUI() {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(8);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    
    // Theme Selection Header
    QHBoxLayout* headerLayout = new QHBoxLayout();
    headerLayout->addWidget(new QLabel("Theme:"));
    
    m_themeCombo = new QComboBox();
    m_themeCombo->setMinimumWidth(150);
    m_themeCombo->addItems(ThemeManager::instance().availableThemes());
    m_themeCombo->setCurrentText(ThemeManager::instance().currentThemeName());
    headerLayout->addWidget(m_themeCombo);
    
    headerLayout->addStretch();
    
    m_applyButton = new QPushButton("Apply");
    m_applyButton->setToolTip("Apply current theme settings");
    headerLayout->addWidget(m_applyButton);
    
    mainLayout->addLayout(headerLayout);
    
    // Tab Widget for different settings categories
    m_tabWidget = new QTabWidget();
    
    // Create each tab
    QWidget* editorColorsTab = new QWidget();
    createEditorColorsTab(editorColorsTab);
    m_tabWidget->addTab(editorColorsTab, "Editor");
    
    QWidget* syntaxColorsTab = new QWidget();
    createSyntaxColorsTab(syntaxColorsTab);
    m_tabWidget->addTab(syntaxColorsTab, "Syntax");
    
    QWidget* chatColorsTab = new QWidget();
    createChatColorsTab(chatColorsTab);
    m_tabWidget->addTab(chatColorsTab, "Chat");
    
    QWidget* uiColorsTab = new QWidget();
    createUIColorsTab(uiColorsTab);
    m_tabWidget->addTab(uiColorsTab, "UI");
    
    QWidget* opacityTab = new QWidget();
    createOpacityTab(opacityTab);
    m_tabWidget->addTab(opacityTab, "Opacity");
    
    QWidget* transparencyTab = new QWidget();
    createTransparencyTab(transparencyTab);
    m_tabWidget->addTab(transparencyTab, "Window");
    
    mainLayout->addWidget(m_tabWidget, 1);
    
    // Action Buttons
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    
    m_saveButton = new QPushButton("Save Theme");
    m_saveButton->setToolTip("Save current settings as a new theme");
    buttonLayout->addWidget(m_saveButton);
    
    m_importButton = new QPushButton("Import");
    m_importButton->setToolTip("Import theme from JSON file");
    buttonLayout->addWidget(m_importButton);
    
    m_exportButton = new QPushButton("Export");
    m_exportButton->setToolTip("Export current theme to JSON file");
    buttonLayout->addWidget(m_exportButton);
    
    buttonLayout->addStretch();
    
    m_resetButton = new QPushButton("Reset");
    m_resetButton->setToolTip("Reset to default theme");
    buttonLayout->addWidget(m_resetButton);
    
    mainLayout->addLayout(buttonLayout);
}

QWidget* ThemeConfigurationPanel::createColorRow(const QString& label, const QString& colorName, QWidget* parent) {
    QWidget* row = new QWidget(parent);
    QHBoxLayout* layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 2, 0, 2);
    
    QLabel* labelWidget = new QLabel(label + ":");
    labelWidget->setMinimumWidth(120);
    layout->addWidget(labelWidget);
    
    QColor initialColor = ThemeManager::instance().getColor(colorName);
    ColorPickerButton* picker = new ColorPickerButton(initialColor, row);
    m_colorPickers[colorName] = picker;
    layout->addWidget(picker);
    
    layout->addStretch();
    
    return row;
}

void ThemeConfigurationPanel::createEditorColorsTab(QWidget* parent) {
    QVBoxLayout* layout = new QVBoxLayout(parent);
    
    QGroupBox* editorGroup = new QGroupBox("Editor Colors");
    QVBoxLayout* editorLayout = new QVBoxLayout(editorGroup);
    
    editorLayout->addWidget(createColorRow("Background", "editorBackground", editorGroup));
    editorLayout->addWidget(createColorRow("Foreground", "editorForeground", editorGroup));
    editorLayout->addWidget(createColorRow("Selection", "editorSelection", editorGroup));
    editorLayout->addWidget(createColorRow("Current Line", "editorCurrentLine", editorGroup));
    editorLayout->addWidget(createColorRow("Line Numbers", "editorLineNumbers", editorGroup));
    editorLayout->addWidget(createColorRow("Whitespace", "editorWhitespace", editorGroup));
    editorLayout->addWidget(createColorRow("Indent Guides", "editorIndentGuides", editorGroup));
    
    layout->addWidget(editorGroup);
    layout->addStretch();
}

void ThemeConfigurationPanel::createSyntaxColorsTab(QWidget* parent) {
    QVBoxLayout* layout = new QVBoxLayout(parent);
    
    QGroupBox* syntaxGroup = new QGroupBox("Syntax Highlighting");
    QVBoxLayout* syntaxLayout = new QVBoxLayout(syntaxGroup);
    
    syntaxLayout->addWidget(createColorRow("Keywords", "keywordColor", syntaxGroup));
    syntaxLayout->addWidget(createColorRow("Strings", "stringColor", syntaxGroup));
    syntaxLayout->addWidget(createColorRow("Comments", "commentColor", syntaxGroup));
    syntaxLayout->addWidget(createColorRow("Numbers", "numberColor", syntaxGroup));
    syntaxLayout->addWidget(createColorRow("Functions", "functionColor", syntaxGroup));
    syntaxLayout->addWidget(createColorRow("Classes", "classColor", syntaxGroup));
    syntaxLayout->addWidget(createColorRow("Operators", "operatorColor", syntaxGroup));
    syntaxLayout->addWidget(createColorRow("Preprocessor", "preprocessorColor", syntaxGroup));
    
    layout->addWidget(syntaxGroup);
    layout->addStretch();
}

void ThemeConfigurationPanel::createChatColorsTab(QWidget* parent) {
    QVBoxLayout* layout = new QVBoxLayout(parent);
    
    QGroupBox* userGroup = new QGroupBox("User Messages");
    QVBoxLayout* userLayout = new QVBoxLayout(userGroup);
    userLayout->addWidget(createColorRow("Background", "chatUserBackground", userGroup));
    userLayout->addWidget(createColorRow("Text", "chatUserForeground", userGroup));
    layout->addWidget(userGroup);
    
    QGroupBox* aiGroup = new QGroupBox("AI Messages");
    QVBoxLayout* aiLayout = new QVBoxLayout(aiGroup);
    aiLayout->addWidget(createColorRow("Background", "chatAIBackground", aiGroup));
    aiLayout->addWidget(createColorRow("Text", "chatAIForeground", aiGroup));
    layout->addWidget(aiGroup);
    
    QGroupBox* systemGroup = new QGroupBox("System Messages");
    QVBoxLayout* systemLayout = new QVBoxLayout(systemGroup);
    systemLayout->addWidget(createColorRow("Background", "chatSystemBackground", systemGroup));
    systemLayout->addWidget(createColorRow("Text", "chatSystemForeground", systemGroup));
    systemLayout->addWidget(createColorRow("Border", "chatBorder", systemGroup));
    layout->addWidget(systemGroup);
    
    layout->addStretch();
}

void ThemeConfigurationPanel::createUIColorsTab(QWidget* parent) {
    QScrollArea* scrollArea = new QScrollArea(parent);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    
    QWidget* scrollWidget = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(scrollWidget);
    
    QGroupBox* windowGroup = new QGroupBox("Window");
    QVBoxLayout* windowLayout = new QVBoxLayout(windowGroup);
    windowLayout->addWidget(createColorRow("Background", "windowBackground", windowGroup));
    windowLayout->addWidget(createColorRow("Foreground", "windowForeground", windowGroup));
    layout->addWidget(windowGroup);
    
    QGroupBox* dockGroup = new QGroupBox("Docks");
    QVBoxLayout* dockLayout = new QVBoxLayout(dockGroup);
    dockLayout->addWidget(createColorRow("Background", "dockBackground", dockGroup));
    dockLayout->addWidget(createColorRow("Border", "dockBorder", dockGroup));
    layout->addWidget(dockGroup);
    
    QGroupBox* menuGroup = new QGroupBox("Menus & Toolbars");
    QVBoxLayout* menuLayout = new QVBoxLayout(menuGroup);
    menuLayout->addWidget(createColorRow("Toolbar Background", "toolbarBackground", menuGroup));
    menuLayout->addWidget(createColorRow("Menu Background", "menuBackground", menuGroup));
    menuLayout->addWidget(createColorRow("Menu Text", "menuForeground", menuGroup));
    layout->addWidget(menuGroup);
    
    QGroupBox* buttonGroup = new QGroupBox("Buttons");
    QVBoxLayout* buttonLayout = new QVBoxLayout(buttonGroup);
    buttonLayout->addWidget(createColorRow("Background", "buttonBackground", buttonGroup));
    buttonLayout->addWidget(createColorRow("Text", "buttonForeground", buttonGroup));
    buttonLayout->addWidget(createColorRow("Hover", "buttonHover", buttonGroup));
    buttonLayout->addWidget(createColorRow("Pressed", "buttonPressed", buttonGroup));
    layout->addWidget(buttonGroup);
    
    layout->addStretch();
    
    scrollArea->setWidget(scrollWidget);
    
    QVBoxLayout* parentLayout = new QVBoxLayout(parent);
    parentLayout->setContentsMargins(0, 0, 0, 0);
    parentLayout->addWidget(scrollArea);
}

void ThemeConfigurationPanel::createOpacityTab(QWidget* parent) {
    QVBoxLayout* layout = new QVBoxLayout(parent);
    
    auto createOpacityRow = [this](const QString& label, const QString& element,
                                    QSlider*& slider, QDoubleSpinBox*& spinBox) -> QWidget* {
        QWidget* row = new QWidget();
        QHBoxLayout* rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(0, 4, 0, 4);
        
        QLabel* labelWidget = new QLabel(label + ":");
        labelWidget->setMinimumWidth(80);
        rowLayout->addWidget(labelWidget);
        
        slider = new QSlider(Qt::Horizontal);
        slider->setRange(10, 100);
        slider->setValue(100);
        slider->setTickPosition(QSlider::TicksBelow);
        slider->setTickInterval(10);
        rowLayout->addWidget(slider, 1);
        
        spinBox = new QDoubleSpinBox();
        spinBox->setRange(0.1, 1.0);
        spinBox->setSingleStep(0.05);
        spinBox->setDecimals(2);
        spinBox->setValue(1.0);
        spinBox->setFixedWidth(70);
        rowLayout->addWidget(spinBox);
        
        QLabel* percentLabel = new QLabel("%");
        rowLayout->addWidget(percentLabel);
        
        return row;
    };
    
    QGroupBox* opacityGroup = new QGroupBox("Element Opacity");
    QVBoxLayout* opacityLayout = new QVBoxLayout(opacityGroup);
    
    opacityLayout->addWidget(createOpacityRow("Window", "window", m_windowOpacitySlider, m_windowOpacitySpin));
    opacityLayout->addWidget(createOpacityRow("Docks", "dock", m_dockOpacitySlider, m_dockOpacitySpin));
    opacityLayout->addWidget(createOpacityRow("Chat", "chat", m_chatOpacitySlider, m_chatOpacitySpin));
    opacityLayout->addWidget(createOpacityRow("Editor", "editor", m_editorOpacitySlider, m_editorOpacitySpin));
    
    layout->addWidget(opacityGroup);
    
    // Quick presets
    QGroupBox* presetGroup = new QGroupBox("Quick Presets");
    QHBoxLayout* presetLayout = new QHBoxLayout(presetGroup);
    
    QPushButton* opaqueBtn = new QPushButton("Opaque");
    connect(opaqueBtn, &QPushButton::clicked, [this]() {
        m_windowOpacitySlider->setValue(100);
        m_dockOpacitySlider->setValue(100);
        m_chatOpacitySlider->setValue(100);
        m_editorOpacitySlider->setValue(100);
    });
    presetLayout->addWidget(opaqueBtn);
    
    QPushButton* semiBtn = new QPushButton("Semi-Transparent");
    connect(semiBtn, &QPushButton::clicked, [this]() {
        m_windowOpacitySlider->setValue(90);
        m_dockOpacitySlider->setValue(85);
        m_chatOpacitySlider->setValue(92);
        m_editorOpacitySlider->setValue(88);
    });
    presetLayout->addWidget(semiBtn);
    
    QPushButton* glassBtn = new QPushButton("Glass");
    connect(glassBtn, &QPushButton::clicked, [this]() {
        m_windowOpacitySlider->setValue(95);
        m_dockOpacitySlider->setValue(90);
        m_chatOpacitySlider->setValue(92);
        m_editorOpacitySlider->setValue(88);
    });
    presetLayout->addWidget(glassBtn);
    
    QPushButton* ghostBtn = new QPushButton("Ghost");
    connect(ghostBtn, &QPushButton::clicked, [this]() {
        m_windowOpacitySlider->setValue(70);
        m_dockOpacitySlider->setValue(60);
        m_chatOpacitySlider->setValue(75);
        m_editorOpacitySlider->setValue(65);
    });
    presetLayout->addWidget(ghostBtn);
    
    layout->addWidget(presetGroup);
    
    // Warning
    QLabel* warningLabel = new QLabel(
        "⚠️ Lower opacity values may affect performance on some systems.\n"
        "Enable transparency in the Window tab to apply opacity settings.");
    warningLabel->setStyleSheet("color: #ff9800; font-size: 11px;");
    warningLabel->setWordWrap(true);
    layout->addWidget(warningLabel);
    
    layout->addStretch();
}

void ThemeConfigurationPanel::createTransparencyTab(QWidget* parent) {
    QVBoxLayout* layout = new QVBoxLayout(parent);
    
    QGroupBox* transparencyGroup = new QGroupBox("Window Management");
    QVBoxLayout* transparencyLayout = new QVBoxLayout(transparencyGroup);
    
    m_transparencyEnabled = new QCheckBox("Enable Window Transparency");
    m_transparencyEnabled->setToolTip("Apply opacity settings to the main window and docks");
    transparencyLayout->addWidget(m_transparencyEnabled);
    
    m_alwaysOnTop = new QCheckBox("Always on Top");
    m_alwaysOnTop->setToolTip("Keep the IDE window above all other windows");
    transparencyLayout->addWidget(m_alwaysOnTop);
    
    m_clickThroughEnabled = new QCheckBox("Enable Click-Through (Experimental)");
    m_clickThroughEnabled->setToolTip("Allow mouse clicks to pass through transparent areas");
    m_clickThroughEnabled->setEnabled(false); // Platform-specific, may not be available
    transparencyLayout->addWidget(m_clickThroughEnabled);
    
    layout->addWidget(transparencyGroup);
    
    // Info section
    QGroupBox* infoGroup = new QGroupBox("Information");
    QVBoxLayout* infoLayout = new QVBoxLayout(infoGroup);
    
    QLabel* infoLabel = new QLabel(
        "<b>Window Transparency:</b><br>"
        "When enabled, the opacity sliders in the Opacity tab will affect "
        "the actual window transparency.<br><br>"
        "<b>Always on Top:</b><br>"
        "Keeps the IDE visible above other windows, useful when referencing "
        "documentation or other applications.<br><br>"
        "<b>Click-Through:</b><br>"
        "An experimental feature that allows mouse clicks to pass through "
        "transparent areas to windows behind. May not work on all systems.");
    infoLabel->setWordWrap(true);
    infoLabel->setStyleSheet("font-size: 11px;");
    infoLayout->addWidget(infoLabel);
    
    layout->addWidget(infoGroup);
    
    // Preview area
    QGroupBox* previewGroup = new QGroupBox("Preview");
    QVBoxLayout* previewLayout = new QVBoxLayout(previewGroup);
    
    m_previewWidget = new QWidget();
    m_previewWidget->setMinimumHeight(100);
    m_previewWidget->setStyleSheet("background-color: #2d2d30; border: 1px solid #3e3e42; border-radius: 4px;");
    previewLayout->addWidget(m_previewWidget);
    
    m_previewLabel = new QLabel("Theme preview updates in real-time");
    m_previewLabel->setAlignment(Qt::AlignCenter);
    m_previewLabel->setStyleSheet("color: #888; font-style: italic;");
    previewLayout->addWidget(m_previewLabel);
    
    layout->addWidget(previewGroup);
    
    layout->addStretch();
}

void ThemeConfigurationPanel::connectSignals() {
    // Theme selection
    connect(m_themeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ThemeConfigurationPanel::onThemeSelected);
    
    // Color pickers
    for (auto it = m_colorPickers.begin(); it != m_colorPickers.end(); ++it) {
        QString colorName = it.key();
        connect(it.value(), &ColorPickerButton::colorChanged,
                [this, colorName](const QColor& color) {
                    onColorChanged(colorName, color);
                });
    }
    
    // Opacity sliders and spinboxes
    auto connectOpacityControls = [this](QSlider* slider, QDoubleSpinBox* spinBox, const QString& element) {
        connect(slider, &QSlider::valueChanged, [this, spinBox, element](int value) {
            double opacity = value / 100.0;
            spinBox->blockSignals(true);
            spinBox->setValue(opacity);
            spinBox->blockSignals(false);
            onOpacityChanged(element, opacity);
        });
        
        connect(spinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                [this, slider, element](double value) {
                    slider->blockSignals(true);
                    slider->setValue(static_cast<int>(value * 100));
                    slider->blockSignals(false);
                    onOpacityChanged(element, value);
                });
    };
    
    connectOpacityControls(m_windowOpacitySlider, m_windowOpacitySpin, "window");
    connectOpacityControls(m_dockOpacitySlider, m_dockOpacitySpin, "dock");
    connectOpacityControls(m_chatOpacitySlider, m_chatOpacitySpin, "chat");
    connectOpacityControls(m_editorOpacitySlider, m_editorOpacitySpin, "editor");
    
    // Transparency controls
    connect(m_transparencyEnabled, &QCheckBox::toggled,
            this, &ThemeConfigurationPanel::onTransparencyToggled);
    connect(m_alwaysOnTop, &QCheckBox::toggled,
            this, &ThemeConfigurationPanel::onAlwaysOnTopToggled);
    connect(m_clickThroughEnabled, &QCheckBox::toggled,
            this, &ThemeConfigurationPanel::onClickThroughToggled);
    
    // Buttons
    connect(m_saveButton, &QPushButton::clicked, this, &ThemeConfigurationPanel::saveCurrentTheme);
    connect(m_importButton, &QPushButton::clicked, this, &ThemeConfigurationPanel::importTheme);
    connect(m_exportButton, &QPushButton::clicked, this, &ThemeConfigurationPanel::exportTheme);
    connect(m_resetButton, &QPushButton::clicked, this, &ThemeConfigurationPanel::resetToDefaults);
    connect(m_applyButton, &QPushButton::clicked, this, &ThemeConfigurationPanel::applyTheme);
    
    // Listen to ThemeManager changes
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged,
            this, &ThemeConfigurationPanel::loadCurrentTheme);
}

void ThemeConfigurationPanel::onThemeSelected(int index) {
    Q_UNUSED(index);
    QString themeName = m_themeCombo->currentText();
    qDebug() << "[ThemeConfigurationPanel] Theme selected:" << themeName;
    
    ThemeManager::instance().loadTheme(themeName);
    emit themeChanged();
}

void ThemeConfigurationPanel::onColorChanged(const QString& colorName, const QColor& color) {
    qDebug() << "[ThemeConfigurationPanel] Color changed:" << colorName;
    ThemeManager::instance().updateColor(colorName, color);
    updatePreview();
    emit colorsUpdated();
}

void ThemeConfigurationPanel::onOpacityChanged(const QString& element, double opacity) {
    qDebug() << "[ThemeConfigurationPanel] Opacity changed:" << element << opacity;
    ThemeManager::instance().updateOpacity(element, opacity);
    emit opacityChanged(element, opacity);
}

void ThemeConfigurationPanel::onTransparencyToggled(bool enabled) {
    qDebug() << "[ThemeConfigurationPanel] Transparency toggled:" << enabled;
    ThemeManager::instance().setWindowTransparencyEnabled(enabled);
    emit transparencySettingsChanged();
}

void ThemeConfigurationPanel::onAlwaysOnTopToggled(bool enabled) {
    qDebug() << "[ThemeConfigurationPanel] Always on top toggled:" << enabled;
    ThemeManager::instance().setAlwaysOnTop(enabled);
    emit transparencySettingsChanged();
}

void ThemeConfigurationPanel::onClickThroughToggled(bool enabled) {
    qDebug() << "[ThemeConfigurationPanel] Click-through toggled:" << enabled;
    ThemeManager::instance().setClickThroughEnabled(enabled);
    emit transparencySettingsChanged();
}

void ThemeConfigurationPanel::saveCurrentTheme() {
    bool ok;
    QString themeName = QInputDialog::getText(this, "Save Theme",
        "Enter theme name:", QLineEdit::Normal, "My Custom Theme", &ok);
    
    if (ok && !themeName.isEmpty()) {
        ThemeManager::instance().saveTheme(themeName);
        
        // Update combo box
        if (m_themeCombo->findText(themeName) == -1) {
            m_themeCombo->addItem(themeName);
        }
        m_themeCombo->setCurrentText(themeName);
        
        QMessageBox::information(this, "Theme Saved",
            QString("Theme '%1' saved successfully!").arg(themeName));
    }
}

void ThemeConfigurationPanel::importTheme() {
    QString fileName = QFileDialog::getOpenFileName(this,
        "Import Theme", "", "JSON Files (*.json);;All Files (*)");
    
    if (!fileName.isEmpty()) {
        QString errorMsg;
        if (ThemeManager::instance().importTheme(fileName, errorMsg)) {
            // Refresh theme list
            m_themeCombo->clear();
            m_themeCombo->addItems(ThemeManager::instance().availableThemes());
            
            QMessageBox::information(this, "Theme Imported",
                "Theme imported successfully!");
        } else {
            QMessageBox::warning(this, "Import Failed", errorMsg);
        }
    }
}

void ThemeConfigurationPanel::exportTheme() {
    QString fileName = QFileDialog::getSaveFileName(this,
        "Export Theme", ThemeManager::instance().currentThemeName() + ".json",
        "JSON Files (*.json);;All Files (*)");
    
    if (!fileName.isEmpty()) {
        QString errorMsg;
        if (ThemeManager::instance().exportTheme(fileName, errorMsg)) {
            QMessageBox::information(this, "Theme Exported",
                QString("Theme exported to:\n%1").arg(fileName));
        } else {
            QMessageBox::warning(this, "Export Failed", errorMsg);
        }
    }
}

void ThemeConfigurationPanel::resetToDefaults() {
    int result = QMessageBox::question(this, "Reset Theme",
        "Reset all colors and settings to the default Dark theme?",
        QMessageBox::Yes | QMessageBox::No);
    
    if (result == QMessageBox::Yes) {
        ThemeManager::instance().loadTheme("Dark");
        m_themeCombo->setCurrentText("Dark");
        loadCurrentTheme();
        emit themeChanged();
    }
}

void ThemeConfigurationPanel::applyTheme() {
    qDebug() << "[ThemeConfigurationPanel] Applying theme...";
    ThemeManager::instance().applyThemeToApplication();
    emit themeChanged();
}

void ThemeConfigurationPanel::loadCurrentTheme() {
    qDebug() << "[ThemeConfigurationPanel] Loading current theme into UI...";
    
    // Block signals during update
    m_themeCombo->blockSignals(true);
    m_themeCombo->setCurrentText(ThemeManager::instance().currentThemeName());
    m_themeCombo->blockSignals(false);
    
    // Update color pickers
    for (auto it = m_colorPickers.begin(); it != m_colorPickers.end(); ++it) {
        QString colorName = it.key();
        QColor color = ThemeManager::instance().getColor(colorName);
        it.value()->blockSignals(true);
        it.value()->setCurrentColor(color);
        it.value()->blockSignals(false);
    }
    
    // Update opacity controls
    const auto& colors = ThemeManager::instance().currentColors();
    
    m_windowOpacitySlider->blockSignals(true);
    m_windowOpacitySpin->blockSignals(true);
    m_windowOpacitySlider->setValue(static_cast<int>(colors.windowOpacity * 100));
    m_windowOpacitySpin->setValue(colors.windowOpacity);
    m_windowOpacitySlider->blockSignals(false);
    m_windowOpacitySpin->blockSignals(false);
    
    m_dockOpacitySlider->blockSignals(true);
    m_dockOpacitySpin->blockSignals(true);
    m_dockOpacitySlider->setValue(static_cast<int>(colors.dockOpacity * 100));
    m_dockOpacitySpin->setValue(colors.dockOpacity);
    m_dockOpacitySlider->blockSignals(false);
    m_dockOpacitySpin->blockSignals(false);
    
    m_chatOpacitySlider->blockSignals(true);
    m_chatOpacitySpin->blockSignals(true);
    m_chatOpacitySlider->setValue(static_cast<int>(colors.chatOpacity * 100));
    m_chatOpacitySpin->setValue(colors.chatOpacity);
    m_chatOpacitySlider->blockSignals(false);
    m_chatOpacitySpin->blockSignals(false);
    
    m_editorOpacitySlider->blockSignals(true);
    m_editorOpacitySpin->blockSignals(true);
    m_editorOpacitySlider->setValue(static_cast<int>(colors.editorOpacity * 100));
    m_editorOpacitySpin->setValue(colors.editorOpacity);
    m_editorOpacitySlider->blockSignals(false);
    m_editorOpacitySpin->blockSignals(false);
    
    // Update transparency controls
    m_transparencyEnabled->blockSignals(true);
    m_transparencyEnabled->setChecked(ThemeManager::instance().isWindowTransparencyEnabled());
    m_transparencyEnabled->blockSignals(false);
    
    m_alwaysOnTop->blockSignals(true);
    m_alwaysOnTop->setChecked(ThemeManager::instance().isAlwaysOnTop());
    m_alwaysOnTop->blockSignals(false);
    
    m_clickThroughEnabled->blockSignals(true);
    m_clickThroughEnabled->setChecked(ThemeManager::instance().isClickThroughEnabled());
    m_clickThroughEnabled->blockSignals(false);
    
    updatePreview();
}

void ThemeConfigurationPanel::updatePreview() {
    if (!m_previewWidget) return;
    
    const auto& colors = ThemeManager::instance().currentColors();
    
    QString previewStyle = QString(R"(
        QWidget {
            background-color: %1;
            border: 1px solid %2;
            border-radius: 4px;
        }
    )")
    .arg(colors.windowBackground.name())
    .arg(colors.dockBorder.name());
    
    m_previewWidget->setStyleSheet(previewStyle);
    
    if (m_previewLabel) {
        m_previewLabel->setStyleSheet(QString("color: %1; font-style: italic;")
            .arg(colors.windowForeground.name()));
    }
}

} // namespace RawrXD
