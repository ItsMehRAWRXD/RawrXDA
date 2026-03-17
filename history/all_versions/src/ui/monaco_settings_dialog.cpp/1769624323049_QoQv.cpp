#include "monaco_settings_dialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QMessageBox>
#include <QScrollArea>

namespace RawrXD::UI {

MonacoSettingsDialog::MonacoSettingsDialog(QWidget* parent)
    : QDialog(parent) {
    setWindowTitle(tr("Monaco Editor Settings"));
    setMinimumSize(600, 500);
    setupUI();
}

MonacoSettingsDialog::~MonacoSettingsDialog() = default;

void MonacoSettingsDialog::setupUI() {
    auto* mainLayout = new QVBoxLayout(this);
    
    // Create tab widget
    auto* tabs = new QTabWidget(this);
    
    createThemeTab(tabs);
    createFontTab(tabs);
    createEditorTab(tabs);
    createNeonTab(tabs);
    createPerformanceTab(tabs);
    
    mainLayout->addWidget(tabs);
    
    // Bottom buttons
    auto* buttonLayout = new QHBoxLayout();
    
    exportBtn_ = new QPushButton(tr("Export Theme..."), this);
    importBtn_ = new QPushButton(tr("Import Theme..."), this);
    previewBtn_ = new QPushButton(tr("Preview"), this);
    resetBtn_ = new QPushButton(tr("Reset to Default"), this);
    applyBtn_ = new QPushButton(tr("Apply"), this);
    
    buttonLayout->addWidget(exportBtn_);
    buttonLayout->addWidget(importBtn_);
    buttonLayout->addStretch();
    buttonLayout->addWidget(previewBtn_);
    buttonLayout->addWidget(resetBtn_);
    buttonLayout->addWidget(applyBtn_);
    
    mainLayout->addLayout(buttonLayout);
    
    // Standard dialog buttons
    auto* dialogButtons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    mainLayout->addWidget(dialogButtons);
    
    // Connections
    connect(dialogButtons, &QDialogButtonBox::accepted, this, [this]() {
        updateSettingsFromUI();
        accept();
    });
    connect(dialogButtons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    
    connect(applyBtn_, &QPushButton::clicked, this, &MonacoSettingsDialog::onApplyClicked);
    connect(resetBtn_, &QPushButton::clicked, this, &MonacoSettingsDialog::onResetToDefaultClicked);
    connect(previewBtn_, &QPushButton::clicked, this, &MonacoSettingsDialog::onPreviewClicked);
    connect(exportBtn_, &QPushButton::clicked, this, &MonacoSettingsDialog::onExportThemeClicked);
    connect(importBtn_, &QPushButton::clicked, this, &MonacoSettingsDialog::onImportThemeClicked);
}

void MonacoSettingsDialog::createThemeTab(QTabWidget* tabs) {
    auto* widget = new QWidget();
    auto* layout = new QVBoxLayout(widget);
    
    // Variant selection
    auto* variantGroup = new QGroupBox(tr("Editor Variant"), widget);
    auto* variantLayout = new QFormLayout(variantGroup);
    
    variantCombo_ = new QComboBox(variantGroup);
    variantCombo_->addItem(tr("Core (Pure Monaco)"));
    variantCombo_->addItem(tr("Neon Core (Cyberpunk Visuals)"));
    variantCombo_->addItem(tr("Neon Hack (ESP Mode)"));
    variantCombo_->addItem(tr("Zero Dependency (Minimal)"));
    variantCombo_->addItem(tr("Enterprise (LSP + Debugging)"));
    variantLayout->addRow(tr("Variant:"), variantCombo_);
    
    layout->addWidget(variantGroup);
    
    // Theme preset selection
    auto* themeGroup = new QGroupBox(tr("Theme Preset"), widget);
    auto* themeLayout = new QFormLayout(themeGroup);
    
    themePresetCombo_ = new QComboBox(themeGroup);
    themePresetCombo_->addItem(tr("Default (VS Code Dark+)"));
    themePresetCombo_->addItem(tr("Neon Cyberpunk"));
    themePresetCombo_->addItem(tr("Matrix Green"));
    themePresetCombo_->addItem(tr("Hacker Red"));
    themePresetCombo_->addItem(tr("Monokai"));
    themePresetCombo_->addItem(tr("Solarized Dark"));
    themePresetCombo_->addItem(tr("Solarized Light"));
    themePresetCombo_->addItem(tr("One Dark"));
    themePresetCombo_->addItem(tr("Dracula"));
    themePresetCombo_->addItem(tr("Gruvbox Dark"));
    themePresetCombo_->addItem(tr("Nord"));
    themePresetCombo_->addItem(tr("Custom"));
    themeLayout->addRow(tr("Theme:"), themePresetCombo_);
    
    layout->addWidget(themeGroup);
    
    // Custom colors (shown when Custom theme selected)
    customColorsGroup_ = new QGroupBox(tr("Custom Colors"), widget);
    auto* colorGrid = new QGridLayout(customColorsGroup_);
    
    auto createColorRow = [&](int row, const QString& label, QPushButton*& btn) {
        colorGrid->addWidget(new QLabel(label), row, 0);
        btn = new QPushButton();
        btn->setFixedSize(60, 25);
        btn->setFlat(true);
        colorGrid->addWidget(btn, row, 1);
        connect(btn, &QPushButton::clicked, this, &MonacoSettingsDialog::onColorButtonClicked);
    };
    
    createColorRow(0, tr("Background:"), backgroundColorBtn_);
    createColorRow(1, tr("Foreground:"), foregroundColorBtn_);
    createColorRow(2, tr("Keywords:"), keywordColorBtn_);
    createColorRow(3, tr("Strings:"), stringColorBtn_);
    createColorRow(4, tr("Comments:"), commentColorBtn_);
    createColorRow(5, tr("Functions:"), functionColorBtn_);
    createColorRow(6, tr("Types:"), typeColorBtn_);
    createColorRow(7, tr("Numbers:"), numberColorBtn_);
    createColorRow(8, tr("Glow Primary:"), glowColorBtn_);
    createColorRow(9, tr("Glow Secondary:"), glowSecondaryBtn_);
    
    layout->addWidget(customColorsGroup_);
    layout->addStretch();
    
    // Connections
    connect(variantCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MonacoSettingsDialog::onVariantChanged);
    connect(themePresetCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MonacoSettingsDialog::onThemePresetChanged);
    
    tabs->addTab(widget, tr("Theme"));
}

void MonacoSettingsDialog::createFontTab(QTabWidget* tabs) {
    auto* widget = new QWidget();
    auto* layout = new QFormLayout(widget);
    
    fontFamilyCombo_ = new QFontComboBox(widget);
    layout->addRow(tr("Font Family:"), fontFamilyCombo_);
    
    fontSizeSpin_ = new QSpinBox(widget);
    fontSizeSpin_->setRange(8, 72);
    fontSizeSpin_->setValue(14);
    layout->addRow(tr("Font Size:"), fontSizeSpin_);
    
    fontWeightCombo_ = new QComboBox(widget);
    fontWeightCombo_->addItem(tr("Thin (100)"), 100);
    fontWeightCombo_->addItem(tr("Light (300)"), 300);
    fontWeightCombo_->addItem(tr("Normal (400)"), 400);
    fontWeightCombo_->addItem(tr("Medium (500)"), 500);
    fontWeightCombo_->addItem(tr("SemiBold (600)"), 600);
    fontWeightCombo_->addItem(tr("Bold (700)"), 700);
    fontWeightCombo_->addItem(tr("ExtraBold (800)"), 800);
    fontWeightCombo_->setCurrentIndex(2); // Normal
    layout->addRow(tr("Font Weight:"), fontWeightCombo_);
    
    fontLigaturesCheck_ = new QCheckBox(tr("Enable Font Ligatures"), widget);
    fontLigaturesCheck_->setChecked(true);
    layout->addRow(fontLigaturesCheck_);
    
    lineHeightSpin_ = new QSpinBox(widget);
    lineHeightSpin_->setRange(0, 100);
    lineHeightSpin_->setValue(0);
    lineHeightSpin_->setSpecialValueText(tr("Auto"));
    layout->addRow(tr("Line Height (0=auto):"), lineHeightSpin_);
    
    connect(fontFamilyCombo_, &QFontComboBox::currentFontChanged,
            this, &MonacoSettingsDialog::onFontFamilyChanged);
    connect(fontSizeSpin_, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &MonacoSettingsDialog::onFontSizeChanged);
    
    tabs->addTab(widget, tr("Font"));
}

void MonacoSettingsDialog::createEditorTab(QTabWidget* tabs) {
    auto* widget = new QWidget();
    auto* layout = new QVBoxLayout(widget);
    
    // Editor behavior
    auto* behaviorGroup = new QGroupBox(tr("Editor Behavior"), widget);
    auto* behaviorLayout = new QFormLayout(behaviorGroup);
    
    wordWrapCheck_ = new QCheckBox(tr("Word Wrap"));
    behaviorLayout->addRow(wordWrapCheck_);
    
    tabSizeSpin_ = new QSpinBox();
    tabSizeSpin_->setRange(1, 8);
    tabSizeSpin_->setValue(4);
    behaviorLayout->addRow(tr("Tab Size:"), tabSizeSpin_);
    
    insertSpacesCheck_ = new QCheckBox(tr("Insert Spaces for Tabs"));
    insertSpacesCheck_->setChecked(true);
    behaviorLayout->addRow(insertSpacesCheck_);
    
    autoIndentCheck_ = new QCheckBox(tr("Auto Indent"));
    autoIndentCheck_->setChecked(true);
    behaviorLayout->addRow(autoIndentCheck_);
    
    bracketMatchingCheck_ = new QCheckBox(tr("Bracket Matching"));
    bracketMatchingCheck_->setChecked(true);
    behaviorLayout->addRow(bracketMatchingCheck_);
    
    autoClosingBracketsCheck_ = new QCheckBox(tr("Auto-close Brackets"));
    autoClosingBracketsCheck_->setChecked(true);
    behaviorLayout->addRow(autoClosingBracketsCheck_);
    
    formatOnPasteCheck_ = new QCheckBox(tr("Format on Paste"));
    behaviorLayout->addRow(formatOnPasteCheck_);
    
    layout->addWidget(behaviorGroup);
    
    // Minimap
    auto* minimapGroup = new QGroupBox(tr("Minimap"), widget);
    auto* minimapLayout = new QFormLayout(minimapGroup);
    
    minimapEnabledCheck_ = new QCheckBox(tr("Enable Minimap"));
    minimapEnabledCheck_->setChecked(true);
    minimapLayout->addRow(minimapEnabledCheck_);
    
    minimapRenderCharsCheck_ = new QCheckBox(tr("Render Characters"));
    minimapRenderCharsCheck_->setChecked(true);
    minimapLayout->addRow(minimapRenderCharsCheck_);
    
    minimapScaleSpin_ = new QSpinBox();
    minimapScaleSpin_->setRange(1, 3);
    minimapScaleSpin_->setValue(1);
    minimapLayout->addRow(tr("Scale:"), minimapScaleSpin_);
    
    layout->addWidget(minimapGroup);
    
    // IntelliSense (Enterprise only)
    auto* intellisenseGroup = new QGroupBox(tr("IntelliSense (Enterprise Only)"), widget);
    auto* intellisenseLayout = new QFormLayout(intellisenseGroup);
    
    intellisenseCheck_ = new QCheckBox(tr("Enable IntelliSense"));
    intellisenseCheck_->setChecked(true);
    intellisenseLayout->addRow(intellisenseCheck_);
    
    quickSuggestionsCheck_ = new QCheckBox(tr("Quick Suggestions"));
    quickSuggestionsCheck_->setChecked(true);
    intellisenseLayout->addRow(quickSuggestionsCheck_);
    
    suggestDelaySpin_ = new QSpinBox();
    suggestDelaySpin_->setRange(0, 1000);
    suggestDelaySpin_->setValue(100);
    suggestDelaySpin_->setSuffix(" ms");
    intellisenseLayout->addRow(tr("Suggest Delay:"), suggestDelaySpin_);
    
    parameterHintsCheck_ = new QCheckBox(tr("Parameter Hints"));
    parameterHintsCheck_->setChecked(true);
    intellisenseLayout->addRow(parameterHintsCheck_);
    
    layout->addWidget(intellisenseGroup);
    
    layout->addStretch();
    
    connect(minimapEnabledCheck_, &QCheckBox::toggled, this, &MonacoSettingsDialog::onMinimapToggled);
    connect(intellisenseCheck_, &QCheckBox::toggled, this, &MonacoSettingsDialog::onIntelliSenseToggled);
    
    tabs->addTab(widget, tr("Editor"));
}

void MonacoSettingsDialog::createNeonTab(QTabWidget* tabs) {
    auto* widget = new QWidget();
    auto* layout = new QVBoxLayout(widget);
    
    // Neon Visual Effects
    auto* neonGroup = new QGroupBox(tr("Neon Visual Effects"), widget);
    auto* neonLayout = new QFormLayout(neonGroup);
    
    neonEffectsCheck_ = new QCheckBox(tr("Enable Neon Effects"));
    neonEffectsCheck_->setChecked(true);
    neonLayout->addRow(neonEffectsCheck_);
    
    auto* glowLayout = new QHBoxLayout();
    glowIntensitySlider_ = new QSlider(Qt::Horizontal);
    glowIntensitySlider_->setRange(0, 15);
    glowIntensitySlider_->setValue(8);
    glowIntensityLabel_ = new QLabel("8");
    glowLayout->addWidget(glowIntensitySlider_);
    glowLayout->addWidget(glowIntensityLabel_);
    neonLayout->addRow(tr("Glow Intensity:"), glowLayout);
    
    scanlineDensitySpin_ = new QSpinBox();
    scanlineDensitySpin_->setRange(0, 8);
    scanlineDensitySpin_->setValue(2);
    neonLayout->addRow(tr("Scanline Density:"), scanlineDensitySpin_);
    
    glitchProbabilitySpin_ = new QSpinBox();
    glitchProbabilitySpin_->setRange(0, 255);
    glitchProbabilitySpin_->setValue(3);
    glitchProbabilitySpin_->setToolTip(tr("1/256 chance per frame"));
    neonLayout->addRow(tr("Glitch Probability:"), glitchProbabilitySpin_);
    
    particlesCheck_ = new QCheckBox(tr("Background Particles"));
    particlesCheck_->setChecked(true);
    neonLayout->addRow(particlesCheck_);
    
    particleCountSpin_ = new QSpinBox();
    particleCountSpin_->setRange(0, 4096);
    particleCountSpin_->setValue(1024);
    neonLayout->addRow(tr("Particle Count:"), particleCountSpin_);
    
    layout->addWidget(neonGroup);
    
    // ESP Mode (NeonHack only)
    espModeGroup_ = new QGroupBox(tr("ESP Mode (NeonHack Variant Only)"), widget);
    auto* espLayout = new QFormLayout(espModeGroup_);
    
    espModeCheck_ = new QCheckBox(tr("Enable ESP Mode"));
    espLayout->addRow(espModeCheck_);
    
    espHighlightVariablesCheck_ = new QCheckBox(tr("Highlight Variables"));
    espHighlightVariablesCheck_->setChecked(true);
    espLayout->addRow(espHighlightVariablesCheck_);
    
    espHighlightFunctionsCheck_ = new QCheckBox(tr("Highlight Functions"));
    espHighlightFunctionsCheck_->setChecked(true);
    espLayout->addRow(espHighlightFunctionsCheck_);
    
    espWallhackCheck_ = new QCheckBox(tr("Wallhack Symbols (Through Comments)"));
    espLayout->addRow(espWallhackCheck_);
    
    layout->addWidget(espModeGroup_);
    
    layout->addStretch();
    
    // Connections
    connect(neonEffectsCheck_, &QCheckBox::toggled, this, &MonacoSettingsDialog::onNeonEffectsToggled);
    connect(glowIntensitySlider_, &QSlider::valueChanged, this, &MonacoSettingsDialog::onGlowIntensityChanged);
    connect(espModeCheck_, &QCheckBox::toggled, this, &MonacoSettingsDialog::onESPModeToggled);
    
    tabs->addTab(widget, tr("Neon/ESP"));
}

void MonacoSettingsDialog::createPerformanceTab(QTabWidget* tabs) {
    auto* widget = new QWidget();
    auto* layout = new QFormLayout(widget);
    
    renderDelaySpin_ = new QSpinBox();
    renderDelaySpin_->setRange(1, 100);
    renderDelaySpin_->setValue(16);
    renderDelaySpin_->setSuffix(" ms");
    renderDelaySpin_->setToolTip(tr("16ms = 60fps, 8ms = 120fps"));
    layout->addRow(tr("Render Delay:"), renderDelaySpin_);
    
    vblankSyncCheck_ = new QCheckBox(tr("VBlank Sync (VSync)"));
    vblankSyncCheck_->setChecked(true);
    layout->addRow(vblankSyncCheck_);
    
    predictiveFetchSpin_ = new QSpinBox();
    predictiveFetchSpin_->setRange(0, 64);
    predictiveFetchSpin_->setValue(16);
    layout->addRow(tr("Predictive Fetch Lines:"), predictiveFetchSpin_);
    
    lazyTokenizationCheck_ = new QCheckBox(tr("Lazy Tokenization"));
    lazyTokenizationCheck_->setChecked(true);
    layout->addRow(lazyTokenizationCheck_);
    
    lazyTokenDelaySpuin_ = new QSpinBox();
    lazyTokenDelaySpuin_->setRange(0, 500);
    lazyTokenDelaySpuin_->setValue(50);
    lazyTokenDelaySpuin_->setSuffix(" ms");
    layout->addRow(tr("Tokenization Delay:"), lazyTokenDelaySpuin_);
    
    tabs->addTab(widget, tr("Performance"));
}

void MonacoSettingsDialog::setSettings(const MonacoSettings& settings) {
    settings_ = settings;
    originalSettings_ = settings;
    updateUIFromSettings();
}

void MonacoSettingsDialog::updateUIFromSettings() {
    // Theme tab
    variantCombo_->setCurrentIndex(static_cast<int>(settings_.variant));
    themePresetCombo_->setCurrentIndex(static_cast<int>(settings_.themePreset));
    
    // Update color buttons
    updateColorButton(backgroundColorBtn_, settings_.colors.background);
    updateColorButton(foregroundColorBtn_, settings_.colors.foreground);
    updateColorButton(keywordColorBtn_, settings_.colors.keyword);
    updateColorButton(stringColorBtn_, settings_.colors.string);
    updateColorButton(commentColorBtn_, settings_.colors.comment);
    updateColorButton(functionColorBtn_, settings_.colors.function);
    updateColorButton(typeColorBtn_, settings_.colors.type);
    updateColorButton(numberColorBtn_, settings_.colors.number);
    updateColorButton(glowColorBtn_, settings_.colors.glowColor);
    updateColorButton(glowSecondaryBtn_, settings_.colors.glowSecondary);
    
    // Show/hide custom colors based on preset
    customColorsGroup_->setVisible(settings_.themePreset == MonacoThemePreset::Custom);
    
    // Font tab
    fontFamilyCombo_->setCurrentFont(QFont(QString::fromStdString(settings_.fontFamily)));
    fontSizeSpin_->setValue(settings_.fontSize);
    fontLigaturesCheck_->setChecked(settings_.fontLigatures);
    lineHeightSpin_->setValue(settings_.lineHeight);
    
    // Find weight index
    for (int i = 0; i < fontWeightCombo_->count(); ++i) {
        if (fontWeightCombo_->itemData(i).toInt() == settings_.fontWeight) {
            fontWeightCombo_->setCurrentIndex(i);
            break;
        }
    }
    
    // Editor tab
    wordWrapCheck_->setChecked(settings_.wordWrap);
    tabSizeSpin_->setValue(settings_.tabSize);
    insertSpacesCheck_->setChecked(settings_.insertSpaces);
    autoIndentCheck_->setChecked(settings_.autoIndent);
    bracketMatchingCheck_->setChecked(settings_.bracketMatching);
    autoClosingBracketsCheck_->setChecked(settings_.autoClosingBrackets);
    formatOnPasteCheck_->setChecked(settings_.formatOnPaste);
    
    minimapEnabledCheck_->setChecked(settings_.minimapEnabled);
    minimapRenderCharsCheck_->setChecked(settings_.minimapRenderCharacters);
    minimapScaleSpin_->setValue(settings_.minimapScale);
    
    intellisenseCheck_->setChecked(settings_.enableIntelliSense);
    quickSuggestionsCheck_->setChecked(settings_.quickSuggestions);
    suggestDelaySpin_->setValue(settings_.suggestDelay);
    parameterHintsCheck_->setChecked(settings_.parameterHints);
    
    // Neon tab
    neonEffectsCheck_->setChecked(settings_.enableNeonEffects);
    glowIntensitySlider_->setValue(settings_.glowIntensity);
    glowIntensityLabel_->setText(QString::number(settings_.glowIntensity));
    scanlineDensitySpin_->setValue(settings_.scanlineDensity);
    glitchProbabilitySpin_->setValue(settings_.glitchProbability);
    particlesCheck_->setChecked(settings_.particlesEnabled);
    particleCountSpin_->setValue(settings_.particleCount);
    
    espModeCheck_->setChecked(settings_.enableESPMode);
    espHighlightVariablesCheck_->setChecked(settings_.espHighlightVariables);
    espHighlightFunctionsCheck_->setChecked(settings_.espHighlightFunctions);
    espWallhackCheck_->setChecked(settings_.espWallhackSymbols);
    
    // Enable/disable ESP mode based on variant
    bool isNeonHack = (settings_.variant == MonacoVariantType::NeonHack);
    espModeGroup_->setEnabled(isNeonHack);
    
    // Enable/disable neon effects based on variant
    bool isNeon = (settings_.variant == MonacoVariantType::NeonCore || 
                   settings_.variant == MonacoVariantType::NeonHack);
    neonEffectsCheck_->setEnabled(isNeon);
    
    // Performance tab
    renderDelaySpin_->setValue(settings_.renderDelay);
    vblankSyncCheck_->setChecked(settings_.vblankSync);
    predictiveFetchSpin_->setValue(settings_.predictiveFetchLines);
    lazyTokenizationCheck_->setChecked(settings_.lazyTokenization);
    lazyTokenDelaySpuin_->setValue(settings_.lazyTokenizationDelay);
}

void MonacoSettingsDialog::updateSettingsFromUI() {
    // Theme tab
    settings_.variant = static_cast<MonacoVariantType>(variantCombo_->currentIndex());
    settings_.themePreset = static_cast<MonacoThemePreset>(themePresetCombo_->currentIndex());
    
    // Colors are updated directly via color picker
    
    // Font tab
    settings_.fontFamily = fontFamilyCombo_->currentFont().family().toStdString();
    settings_.fontSize = fontSizeSpin_->value();
    settings_.fontWeight = fontWeightCombo_->currentData().toInt();
    settings_.fontLigatures = fontLigaturesCheck_->isChecked();
    settings_.lineHeight = lineHeightSpin_->value();
    
    // Editor tab
    settings_.wordWrap = wordWrapCheck_->isChecked();
    settings_.tabSize = tabSizeSpin_->value();
    settings_.insertSpaces = insertSpacesCheck_->isChecked();
    settings_.autoIndent = autoIndentCheck_->isChecked();
    settings_.bracketMatching = bracketMatchingCheck_->isChecked();
    settings_.autoClosingBrackets = autoClosingBracketsCheck_->isChecked();
    settings_.formatOnPaste = formatOnPasteCheck_->isChecked();
    
    settings_.minimapEnabled = minimapEnabledCheck_->isChecked();
    settings_.minimapRenderCharacters = minimapRenderCharsCheck_->isChecked();
    settings_.minimapScale = minimapScaleSpin_->value();
    
    settings_.enableIntelliSense = intellisenseCheck_->isChecked();
    settings_.quickSuggestions = quickSuggestionsCheck_->isChecked();
    settings_.suggestDelay = suggestDelaySpin_->value();
    settings_.parameterHints = parameterHintsCheck_->isChecked();
    
    // Neon tab
    settings_.enableNeonEffects = neonEffectsCheck_->isChecked();
    settings_.glowIntensity = glowIntensitySlider_->value();
    settings_.scanlineDensity = scanlineDensitySpin_->value();
    settings_.glitchProbability = glitchProbabilitySpin_->value();
    settings_.particlesEnabled = particlesCheck_->isChecked();
    settings_.particleCount = particleCountSpin_->value();
    
    settings_.enableESPMode = espModeCheck_->isChecked();
    settings_.espHighlightVariables = espHighlightVariablesCheck_->isChecked();
    settings_.espHighlightFunctions = espHighlightFunctionsCheck_->isChecked();
    settings_.espWallhackSymbols = espWallhackCheck_->isChecked();
    
    // Performance tab
    settings_.renderDelay = renderDelaySpin_->value();
    settings_.vblankSync = vblankSyncCheck_->isChecked();
    settings_.predictiveFetchLines = predictiveFetchSpin_->value();
    settings_.lazyTokenization = lazyTokenizationCheck_->isChecked();
    settings_.lazyTokenizationDelay = lazyTokenDelaySpuin_->value();
    
    settings_.dirty = true;
}

void MonacoSettingsDialog::updateColorButton(QPushButton* button, uint32_t color) {
    QColor qcolor((color >> 16) & 0xFF, (color >> 8) & 0xFF, color & 0xFF);
    QString style = QString("background-color: %1; border: 1px solid #555;").arg(qcolor.name());
    button->setStyleSheet(style);
    button->setProperty("colorValue", QVariant::fromValue(color));
}

void MonacoSettingsDialog::onVariantChanged(int index) {
    settings_.variant = static_cast<MonacoVariantType>(index);
    
    // Enable/disable ESP mode based on variant
    bool isNeonHack = (index == 2);  // NeonHack
    espModeGroup_->setEnabled(isNeonHack);
    
    // Enable/disable neon effects based on variant
    bool isNeon = (index == 1 || index == 2);  // NeonCore or NeonHack
    neonEffectsCheck_->setEnabled(isNeon);
    
    emit settingsChanged(settings_);
}

void MonacoSettingsDialog::onThemePresetChanged(int index) {
    MonacoThemePreset preset = static_cast<MonacoThemePreset>(index);
    
    if (preset != MonacoThemePreset::Custom) {
        settings_.colors = Settings::GetThemePresetColors(preset);
        
        // Update color buttons
        updateColorButton(backgroundColorBtn_, settings_.colors.background);
        updateColorButton(foregroundColorBtn_, settings_.colors.foreground);
        updateColorButton(keywordColorBtn_, settings_.colors.keyword);
        updateColorButton(stringColorBtn_, settings_.colors.string);
        updateColorButton(commentColorBtn_, settings_.colors.comment);
        updateColorButton(functionColorBtn_, settings_.colors.function);
        updateColorButton(typeColorBtn_, settings_.colors.type);
        updateColorButton(numberColorBtn_, settings_.colors.number);
        updateColorButton(glowColorBtn_, settings_.colors.glowColor);
        updateColorButton(glowSecondaryBtn_, settings_.colors.glowSecondary);
    }
    
    settings_.themePreset = preset;
    customColorsGroup_->setVisible(preset == MonacoThemePreset::Custom);
    
    emit themeChanged(preset);
    emit settingsChanged(settings_);
}

void MonacoSettingsDialog::onFontFamilyChanged(const QFont& font) {
    settings_.fontFamily = font.family().toStdString();
    emit settingsChanged(settings_);
}

void MonacoSettingsDialog::onFontSizeChanged(int size) {
    settings_.fontSize = size;
    emit settingsChanged(settings_);
}

void MonacoSettingsDialog::onFontWeightChanged(int weight) {
    settings_.fontWeight = weight;
    emit settingsChanged(settings_);
}

void MonacoSettingsDialog::onColorButtonClicked() {
    auto* button = qobject_cast<QPushButton*>(sender());
    if (!button) return;
    
    uint32_t currentColor = button->property("colorValue").toUInt();
    QColor initial((currentColor >> 16) & 0xFF, (currentColor >> 8) & 0xFF, currentColor & 0xFF);
    
    QColor newColor = QColorDialog::getColor(initial, this, tr("Select Color"));
    if (!newColor.isValid()) return;
    
    uint32_t colorValue = (newColor.red() << 16) | (newColor.green() << 8) | newColor.blue();
    updateColorButton(button, colorValue);
    
    // Update the appropriate color in settings
    settings_.themePreset = MonacoThemePreset::Custom;
    themePresetCombo_->setCurrentIndex(static_cast<int>(MonacoThemePreset::Custom));
    customColorsGroup_->setVisible(true);
    
    if (button == backgroundColorBtn_) settings_.colors.background = colorValue;
    else if (button == foregroundColorBtn_) settings_.colors.foreground = colorValue;
    else if (button == keywordColorBtn_) settings_.colors.keyword = colorValue;
    else if (button == stringColorBtn_) settings_.colors.string = colorValue;
    else if (button == commentColorBtn_) settings_.colors.comment = colorValue;
    else if (button == functionColorBtn_) settings_.colors.function = colorValue;
    else if (button == typeColorBtn_) settings_.colors.type = colorValue;
    else if (button == numberColorBtn_) settings_.colors.number = colorValue;
    else if (button == glowColorBtn_) settings_.colors.glowColor = colorValue;
    else if (button == glowSecondaryBtn_) settings_.colors.glowSecondary = colorValue;
    
    emit settingsChanged(settings_);
}

void MonacoSettingsDialog::onNeonEffectsToggled(bool enabled) {
    settings_.enableNeonEffects = enabled;
    glowIntensitySlider_->setEnabled(enabled);
    scanlineDensitySpin_->setEnabled(enabled);
    glitchProbabilitySpin_->setEnabled(enabled);
    particlesCheck_->setEnabled(enabled);
    particleCountSpin_->setEnabled(enabled && particlesCheck_->isChecked());
    
    emit settingsChanged(settings_);
}

void MonacoSettingsDialog::onGlowIntensityChanged(int value) {
    settings_.glowIntensity = value;
    glowIntensityLabel_->setText(QString::number(value));
    emit settingsChanged(settings_);
}

void MonacoSettingsDialog::onESPModeToggled(bool enabled) {
    settings_.enableESPMode = enabled;
    espHighlightVariablesCheck_->setEnabled(enabled);
    espHighlightFunctionsCheck_->setEnabled(enabled);
    espWallhackCheck_->setEnabled(enabled);
    
    emit settingsChanged(settings_);
}

void MonacoSettingsDialog::onMinimapToggled(bool enabled) {
    settings_.minimapEnabled = enabled;
    minimapRenderCharsCheck_->setEnabled(enabled);
    minimapScaleSpin_->setEnabled(enabled);
    
    emit settingsChanged(settings_);
}

void MonacoSettingsDialog::onIntelliSenseToggled(bool enabled) {
    settings_.enableIntelliSense = enabled;
    quickSuggestionsCheck_->setEnabled(enabled);
    suggestDelaySpin_->setEnabled(enabled);
    parameterHintsCheck_->setEnabled(enabled);
    
    emit settingsChanged(settings_);
}

void MonacoSettingsDialog::onApplyClicked() {
    updateSettingsFromUI();
    emit settingsChanged(settings_);
}

void MonacoSettingsDialog::onResetToDefaultClicked() {
    settings_ = MonacoSettings();  // Reset to defaults
    updateUIFromSettings();
    emit settingsChanged(settings_);
}

void MonacoSettingsDialog::onPreviewClicked() {
    updateSettingsFromUI();
    emit previewRequested(settings_);
}

void MonacoSettingsDialog::onExportThemeClicked() {
    QString path = QFileDialog::getSaveFileName(this, 
        tr("Export Monaco Theme"), 
        QString(), 
        tr("Monaco Theme (*.monaco);;All Files (*)"));
    
    if (path.isEmpty()) return;
    
    updateSettingsFromUI();
    if (Settings::SaveMonaco(settings_, path.toStdString())) {
        QMessageBox::information(this, tr("Export Successful"),
            tr("Theme exported successfully to:\n%1").arg(path));
    } else {
        QMessageBox::warning(this, tr("Export Failed"),
            tr("Failed to export theme to:\n%1").arg(path));
    }
}

void MonacoSettingsDialog::onImportThemeClicked() {
    QString path = QFileDialog::getOpenFileName(this,
        tr("Import Monaco Theme"),
        QString(),
        tr("Monaco Theme (*.monaco);;All Files (*)"));
    
    if (path.isEmpty()) return;
    
    MonacoSettings imported;
    if (Settings::LoadMonaco(imported, path.toStdString())) {
        settings_ = imported;
        updateUIFromSettings();
        emit settingsChanged(settings_);
        QMessageBox::information(this, tr("Import Successful"),
            tr("Theme imported successfully from:\n%1").arg(path));
    } else {
        QMessageBox::warning(this, tr("Import Failed"),
            tr("Failed to import theme from:\n%1").arg(path));
    }
}

bool MonacoSettingsDialog::loadFromFile(const QString& path) {
    if (Settings::LoadMonaco(settings_, path.toStdString())) {
        originalSettings_ = settings_;
        updateUIFromSettings();
        return true;
    }
    return false;
}

bool MonacoSettingsDialog::saveToFile(const QString& path) {
    updateSettingsFromUI();
    return Settings::SaveMonaco(settings_, path.toStdString());
}

} // namespace RawrXD::UI
