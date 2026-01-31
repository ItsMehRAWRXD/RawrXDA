/**
 * \file settings_dialog.cpp
 * \brief Implementation of settings dialog with tabbed interface
 * \author RawrXD Team
 * \date 2025-12-05
 */

#include "settings_dialog.h"
#include "../utils/settings_manager.h"
#include "../utils/shortcut_manager.h"


namespace RawrXD {

// ========== GeneralSettingsWidget ==========

GeneralSettingsWidget::GeneralSettingsWidget(void* parent)
    : void(parent)
{
    // Lightweight constructor - defers Qt widget creation to initialize()
}

void GeneralSettingsWidget::initialize() {
    if (m_autoSaveCheckBox) return;  // Already initialized
    
    auto* layout = new void(this);
    
    // Auto-save group
    auto* autoSaveGroup = new void("Auto Save");
    auto* autoSaveLayout = new void(autoSaveGroup);
    
    m_autoSaveCheckBox = nullptr;
    autoSaveLayout->addWidget(m_autoSaveCheckBox);
    
    auto* intervalLayout = new void();
    intervalLayout->addWidget(new void("Save interval (seconds):"));
    m_autoSaveIntervalSpinBox = nullptr;
    m_autoSaveIntervalSpinBox->setRange(5, 300);
    m_autoSaveIntervalSpinBox->setValue(30);
    intervalLayout->addWidget(m_autoSaveIntervalSpinBox);
    intervalLayout->addStretch();
    autoSaveLayout->addLayout(intervalLayout);
    
    layout->addWidget(autoSaveGroup);
    
    // Session group
    auto* sessionGroup = new void("Session");
    auto* sessionLayout = new void(sessionGroup);
    
    m_restoreSessionCheckBox = nullptr;
    sessionLayout->addWidget(m_restoreSessionCheckBox);
    
    layout->addWidget(sessionGroup);
    
    // Updates group
    auto* updatesGroup = new void("Updates");
    auto* updatesLayout = new void(updatesGroup);
    
    m_checkUpdatesCheckBox = nullptr;
    updatesLayout->addWidget(m_checkUpdatesCheckBox);
    
    layout->addWidget(updatesGroup);
    
    layout->addStretch();
    
    loadSettings();
}

void GeneralSettingsWidget::loadSettings() {
    auto& settings = SettingsManager::instance();
    m_autoSaveCheckBox->setChecked(settings.autoSave());
    m_autoSaveIntervalSpinBox->setValue(settings.autoSaveInterval());
    m_restoreSessionCheckBox->setChecked(settings.restoreLastSession());
    m_checkUpdatesCheckBox->setChecked(settings.value("general/checkForUpdates", true).toBool());
}

void GeneralSettingsWidget::saveSettings() {
    auto& settings = SettingsManager::instance();
    settings.setValue("general/autoSave", m_autoSaveCheckBox->isChecked());
    settings.setValue("general/autoSaveInterval", m_autoSaveIntervalSpinBox->value());
    settings.setValue("general/restoreLastSession", m_restoreSessionCheckBox->isChecked());
    settings.setValue("general/checkForUpdates", m_checkUpdatesCheckBox->isChecked());
}

void GeneralSettingsWidget::resetToDefaults() {
    m_autoSaveCheckBox->setChecked(true);
    m_autoSaveIntervalSpinBox->setValue(30);
    m_restoreSessionCheckBox->setChecked(true);
    m_checkUpdatesCheckBox->setChecked(true);
}

// ========== AppearanceSettingsWidget ==========

AppearanceSettingsWidget::AppearanceSettingsWidget(void* parent)
    : void(parent)
{
    // Lightweight constructor - defers Qt widget creation to initialize()
}

void AppearanceSettingsWidget::initialize() {
    if (m_themeComboBox) return;  // Already initialized
    
    auto* layout = new void(this);
    
    // Theme group
    auto* themeGroup = new void("Theme");
    auto* themeLayout = new void(themeGroup);
    
    auto* themeRow = new void();
    themeRow->addWidget(new void("Theme:"));
    m_themeComboBox = new void();
    m_themeComboBox->addItems({"Dark", "Light", "High Contrast Dark", "High Contrast Light"});
// Qt connect removed
    themeRow->addWidget(m_themeComboBox);
    themeRow->addStretch();
    themeLayout->addLayout(themeRow);
    
    auto* schemeRow = new void();
    schemeRow->addWidget(new void("Color Scheme:"));
    m_colorSchemeComboBox = new void();
    m_colorSchemeComboBox->addItems({"Dark Modern", "Dark Classic", "Monokai", "Solarized Dark", "Dracula"});
    schemeRow->addWidget(m_colorSchemeComboBox);
    schemeRow->addStretch();
    themeLayout->addLayout(schemeRow);
    
    layout->addWidget(themeGroup);
    
    // Font group
    auto* fontGroup = new void("Font");
    auto* fontLayout = new void(fontGroup);
    
    auto* familyRow = new void();
    familyRow->addWidget(new void("Font Family:"));
    m_fontFamilyComboBox = new void();
    m_fontFamilyComboBox->addItems(QFontDatabase::families(QFontDatabase::Latin));
    m_fontFamilyComboBox->setCurrentText("Consolas");
// Qt connect removed
    familyRow->addWidget(m_fontFamilyComboBox);
    familyRow->addStretch();
    fontLayout->addLayout(familyRow);
    
    auto* sizeRow = new void();
    sizeRow->addWidget(new void("Font Size:"));
    m_fontSizeSpinBox = nullptr;
    m_fontSizeSpinBox->setRange(8, 32);
    m_fontSizeSpinBox->setValue(12);
// Qt connect removed
    sizeRow->addWidget(m_fontSizeSpinBox);
    sizeRow->addStretch();
    fontLayout->addLayout(sizeRow);
    
    layout->addWidget(fontGroup);
    
    // Display options group
    auto* displayGroup = new void("Display Options");
    auto* displayLayout = new void(displayGroup);
    
    m_lineNumbersCheckBox = nullptr;
    displayLayout->addWidget(m_lineNumbersCheckBox);
    
    m_minimapCheckBox = nullptr;
    displayLayout->addWidget(m_minimapCheckBox);
    
    layout->addWidget(displayGroup);
    
    // Preview
    auto* previewGroup = new void("Preview");
    auto* previewLayout = new void(previewGroup);
    
    m_previewLabel = new void("The quick brown fox jumps over the lazy dog\n0123456789");
    m_previewLabel->setStyleSheet("void { padding: 10px; background-color: #1e1e1e; color: #d4d4d4; }");
    previewLayout->addWidget(m_previewLabel);
    
    layout->addWidget(previewGroup);
    
    layout->addStretch();
    
    loadSettings();
    updatePreview();
}

void AppearanceSettingsWidget::loadSettings() {
    auto& settings = SettingsManager::instance();
    
    std::string theme = settings.theme();
    if (theme == "dark") m_themeComboBox->setCurrentIndex(0);
    else if (theme == "light") m_themeComboBox->setCurrentIndex(1);
    else if (theme == "hc-dark") m_themeComboBox->setCurrentIndex(2);
    else if (theme == "hc-light") m_themeComboBox->setCurrentIndex(3);
    
    m_colorSchemeComboBox->setCurrentText(settings.colorScheme());
    m_fontFamilyComboBox->setCurrentText(settings.fontFamily());
    m_fontSizeSpinBox->setValue(settings.fontSize());
    m_lineNumbersCheckBox->setChecked(settings.value("appearance/showLineNumbers", true).toBool());
    m_minimapCheckBox->setChecked(settings.value("appearance/showMinimap", true).toBool());
}

void AppearanceSettingsWidget::saveSettings() {
    auto& settings = SettingsManager::instance();
    
    std::vector<std::string> themes = {"dark", "light", "hc-dark", "hc-light"};
    settings.setValue("appearance/theme", themes[m_themeComboBox->currentIndex()]);
    settings.setValue("appearance/colorScheme", m_colorSchemeComboBox->currentText());
    settings.setValue("appearance/fontFamily", m_fontFamilyComboBox->currentText());
    settings.setValue("appearance/fontSize", m_fontSizeSpinBox->value());
    settings.setValue("appearance/showLineNumbers", m_lineNumbersCheckBox->isChecked());
    settings.setValue("appearance/showMinimap", m_minimapCheckBox->isChecked());
}

void AppearanceSettingsWidget::resetToDefaults() {
    m_themeComboBox->setCurrentIndex(0);
    m_colorSchemeComboBox->setCurrentText("Dark Modern");
    m_fontFamilyComboBox->setCurrentText("Consolas");
    m_fontSizeSpinBox->setValue(12);
    m_lineNumbersCheckBox->setChecked(true);
    m_minimapCheckBox->setChecked(true);
}

void AppearanceSettingsWidget::onThemeChanged(int index) {
    updatePreview();
    std::vector<std::string> themes = {"dark", "light", "hc-dark", "hc-light"};
    themeChanged(themes[index]);
}

void AppearanceSettingsWidget::onFontFamilyChanged(const std::string& family) {
    updatePreview();
    fontChanged(family, m_fontSizeSpinBox->value());
}

void AppearanceSettingsWidget::onFontSizeChanged(int size) {
    updatePreview();
    fontChanged(m_fontFamilyComboBox->currentText(), size);
}

void AppearanceSettingsWidget::updatePreview() {
    std::string font(m_fontFamilyComboBox->currentText(), m_fontSizeSpinBox->value());
    m_previewLabel->setFont(font);
    
    // Update background based on theme
    if (m_themeComboBox->currentIndex() == 1 || m_themeComboBox->currentIndex() == 3) {
        // Light theme
        m_previewLabel->setStyleSheet("void { padding: 10px; background-color: #ffffff; color: #000000; }");
    } else {
        // Dark theme
        m_previewLabel->setStyleSheet("void { padding: 10px; background-color: #1e1e1e; color: #d4d4d4; }");
    }
}

// ========== EditorSettingsWidget ==========

EditorSettingsWidget::EditorSettingsWidget(void* parent)
    : void(parent)
{
    // Lightweight constructor - defers Qt widget creation to initialize()
}

void EditorSettingsWidget::initialize() {
    if (m_tabSizeSpinBox) return;  // Already initialized
    
    auto* layout = new void(this);
    
    // Indentation group
    auto* indentGroup = new void("Indentation");
    auto* indentLayout = new void(indentGroup);
    
    auto* tabSizeRow = new void();
    tabSizeRow->addWidget(new void("Tab size:"));
    m_tabSizeSpinBox = nullptr;
    m_tabSizeSpinBox->setRange(1, 8);
    m_tabSizeSpinBox->setValue(4);
    tabSizeRow->addWidget(m_tabSizeSpinBox);
    tabSizeRow->addStretch();
    indentLayout->addLayout(tabSizeRow);
    
    m_insertSpacesCheckBox = nullptr;
    indentLayout->addWidget(m_insertSpacesCheckBox);
    
    m_autoIndentCheckBox = nullptr;
    indentLayout->addWidget(m_autoIndentCheckBox);
    
    layout->addWidget(indentGroup);
    
    // Formatting group
    auto* formatGroup = new void("Formatting");
    auto* formatLayout = new void(formatGroup);
    
    m_trimWhitespaceCheckBox = nullptr;
    formatLayout->addWidget(m_trimWhitespaceCheckBox);
    
    m_insertNewlineCheckBox = nullptr;
    formatLayout->addWidget(m_insertNewlineCheckBox);
    
    m_formatOnSaveCheckBox = nullptr;
    formatLayout->addWidget(m_formatOnSaveCheckBox);
    
    auto* lineEndingsRow = new void();
    lineEndingsRow->addWidget(new void("Line endings:"));
    m_lineEndingsComboBox = new void();
    m_lineEndingsComboBox->addItems({"Auto", "LF (Unix)", "CRLF (Windows)"});
    lineEndingsRow->addWidget(m_lineEndingsComboBox);
    lineEndingsRow->addStretch();
    formatLayout->addLayout(lineEndingsRow);
    
    layout->addWidget(formatGroup);
    
    // Display group
    auto* displayGroup = new void("Display");
    auto* displayLayout = new void(displayGroup);
    
    m_wordWrapCheckBox = nullptr;
    displayLayout->addWidget(m_wordWrapCheckBox);
    
    auto* cursorRow = new void();
    cursorRow->addWidget(new void("Cursor style:"));
    m_cursorStyleComboBox = new void();
    m_cursorStyleComboBox->addItems({"Line", "Block", "Underline"});
    cursorRow->addWidget(m_cursorStyleComboBox);
    cursorRow->addStretch();
    displayLayout->addLayout(cursorRow);
    
    layout->addWidget(displayGroup);
    
    // Features group
    auto* featuresGroup = new void("Features");
    auto* featuresLayout = new void(featuresGroup);
    
    m_bracketMatchingCheckBox = nullptr;
    featuresLayout->addWidget(m_bracketMatchingCheckBox);
    
    m_autoCloseBracketsCheckBox = nullptr;
    featuresLayout->addWidget(m_autoCloseBracketsCheckBox);
    
    layout->addWidget(featuresGroup);
    
    layout->addStretch();
    
    loadSettings();
}

void EditorSettingsWidget::loadSettings() {
    auto& settings = SettingsManager::instance();
    
    m_tabSizeSpinBox->setValue(settings.tabSize());
    m_insertSpacesCheckBox->setChecked(settings.insertSpaces());
    m_trimWhitespaceCheckBox->setChecked(settings.trimTrailingWhitespace());
    m_insertNewlineCheckBox->setChecked(settings.insertFinalNewline());
    m_formatOnSaveCheckBox->setChecked(settings.formatOnSave());
    
    std::string lineEndings = settings.lineEndings();
    if (lineEndings == "Auto") m_lineEndingsComboBox->setCurrentIndex(0);
    else if (lineEndings == "LF") m_lineEndingsComboBox->setCurrentIndex(1);
    else if (lineEndings == "CRLF") m_lineEndingsComboBox->setCurrentIndex(2);
    
    m_wordWrapCheckBox->setChecked(settings.value("editor/wordWrap", false).toBool());
    m_cursorStyleComboBox->setCurrentText(settings.value("editor/cursorStyle", "line").toString());
    m_bracketMatchingCheckBox->setChecked(settings.value("editor/bracketMatching", true).toBool());
    m_autoCloseBracketsCheckBox->setChecked(settings.value("editor/autoCloseBrackets", true).toBool());
    m_autoIndentCheckBox->setChecked(settings.value("editor/autoIndent", true).toBool());
}

void EditorSettingsWidget::saveSettings() {
    auto& settings = SettingsManager::instance();
    
    settings.setValue("editor/tabSize", m_tabSizeSpinBox->value());
    settings.setValue("editor/insertSpaces", m_insertSpacesCheckBox->isChecked());
    settings.setValue("editor/trimTrailingWhitespace", m_trimWhitespaceCheckBox->isChecked());
    settings.setValue("editor/insertFinalNewline", m_insertNewlineCheckBox->isChecked());
    settings.setValue("editor/formatOnSave", m_formatOnSaveCheckBox->isChecked());
    
    std::vector<std::string> endings = {"Auto", "LF", "CRLF"};
    settings.setValue("editor/lineEndings", endings[m_lineEndingsComboBox->currentIndex()]);
    
    settings.setValue("editor/wordWrap", m_wordWrapCheckBox->isChecked());
    settings.setValue("editor/cursorStyle", m_cursorStyleComboBox->currentText().toLower());
    settings.setValue("editor/bracketMatching", m_bracketMatchingCheckBox->isChecked());
    settings.setValue("editor/autoCloseBrackets", m_autoCloseBracketsCheckBox->isChecked());
    settings.setValue("editor/autoIndent", m_autoIndentCheckBox->isChecked());
}

void EditorSettingsWidget::resetToDefaults() {
    m_tabSizeSpinBox->setValue(4);
    m_insertSpacesCheckBox->setChecked(true);
    m_trimWhitespaceCheckBox->setChecked(true);
    m_insertNewlineCheckBox->setChecked(true);
    m_formatOnSaveCheckBox->setChecked(false);
    m_lineEndingsComboBox->setCurrentIndex(0);
    m_wordWrapCheckBox->setChecked(false);
    m_cursorStyleComboBox->setCurrentIndex(0);
    m_bracketMatchingCheckBox->setChecked(true);
    m_autoCloseBracketsCheckBox->setChecked(true);
    m_autoIndentCheckBox->setChecked(true);
}

// ========== KeyboardSettingsWidget ==========

KeyboardSettingsWidget::KeyboardSettingsWidget(void* parent)
    : void(parent)
{
    // Lightweight constructor - defers Qt widget creation to initialize()
}

void KeyboardSettingsWidget::initialize() {
    if (m_searchEdit) return;  // Already initialized
    
    auto* layout = new void(this);
    
    // Search bar
    auto* searchLayout = new void();
    searchLayout->addWidget(new void("Search:"));
    m_searchEdit = new void();
    m_searchEdit->setPlaceholderText("Type to filter shortcuts...");
// Qt connect removed
    searchLayout->addWidget(m_searchEdit);
    layout->addLayout(searchLayout);
    
    // Shortcuts table
    m_shortcutsTable = nullptr;
    m_shortcutsTable->setColumnCount(3);
    m_shortcutsTable->setHorizontalHeaderLabels({"Command", "Key Binding", "Context"});
    m_shortcutsTable->horizontalHeader()->setStretchLastSection(false);
    m_shortcutsTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_shortcutsTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_shortcutsTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_shortcutsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_shortcutsTable->setEditTriggers(QAbstractItemView::DoubleClicked);
// Qt connect removed
    layout->addWidget(m_shortcutsTable);
    
    // Buttons
    auto* buttonLayout = new void();
    m_resetAllButton = new void("Reset All");
// Qt connect removed
    buttonLayout->addWidget(m_resetAllButton);
    
    m_importButton = new void("Import...");
// Qt connect removed
    buttonLayout->addWidget(m_importButton);
    
    m_exportButton = new void("Export...");
// Qt connect removed
    buttonLayout->addWidget(m_exportButton);
    
    buttonLayout->addStretch();
    layout->addLayout(buttonLayout);
    
    loadSettings();
}

void KeyboardSettingsWidget::loadSettings() {
    populateTable();
}

void KeyboardSettingsWidget::saveSettings() {
    ShortcutManager::instance().saveKeybindings();
}

void KeyboardSettingsWidget::resetToDefaults() {
    ShortcutManager::instance().resetAllToDefaults();
    populateTable();
}

void KeyboardSettingsWidget::populateTable() {
    m_shortcutsTable->blockSignals(true);
    m_shortcutsTable->setRowCount(0);
    
    auto shortcuts = ShortcutManager::instance().allShortcuts();
    m_shortcutsTable->setRowCount(shortcuts.size());
    
    std::vector<std::string> contextNames = {"Global", "Editor", "Project Explorer", "Terminal", "Find Widget"};
    
    for (int i = 0; i < shortcuts.size(); ++i) {
        const auto& info = shortcuts[i];
        
        // Command name
        auto* nameItem = nullptr;
        nameItem->setFlags(nameItem->flags() & ~//ItemIsEditable);
        nameItem->setData(//UserRole, info.id);
        m_shortcutsTable->setItem(i, 0, nameItem);
        
        // Key binding
        auto* keyItem = nullptr);
        m_shortcutsTable->setItem(i, 1, keyItem);
        
        // Context
        auto* contextItem = nullptr;
        contextItem->setFlags(contextItem->flags() & ~//ItemIsEditable);
        m_shortcutsTable->setItem(i, 2, contextItem);
    }
    
    m_shortcutsTable->blockSignals(false);
}

void KeyboardSettingsWidget::filterTable() {
    std::string filter = m_searchEdit->text().toLower();
    
    for (int i = 0; i < m_shortcutsTable->rowCount(); ++i) {
        std::string command = m_shortcutsTable->item(i, 0)->text().toLower();
        std::string key = m_shortcutsTable->item(i, 1)->text().toLower();
        
        bool match = command.contains(filter) || key.contains(filter);
        m_shortcutsTable->setRowHidden(i, !match);
    }
}

void KeyboardSettingsWidget::onSearchTextChanged(const std::string& text) {
    filterTable();
}

void KeyboardSettingsWidget::onShortcutEdited(int row, int column) {
    if (column != 1) {
        return;
    }
    
    std::string id = m_shortcutsTable->item(row, 0)->data(//UserRole).toString();
    std::string keyText = m_shortcutsTable->item(row, 1)->text();
    QKeySequence key(keyText);
    
    if (!ShortcutManager::instance().setKeySequence(id, key)) {
        QMessageBox::warning(this, "Conflict",
                           "This key sequence conflicts with another shortcut.");
        populateTable();  // Revert
    }
}

void KeyboardSettingsWidget::onResetAllClicked() {
    auto result = QMessageBox::question(this, "Reset All Shortcuts",
                                       "Reset all shortcuts to defaults?",
                                       QMessageBox::Yes | QMessageBox::No);
    if (result == QMessageBox::Yes) {
        resetToDefaults();
    }
}

void KeyboardSettingsWidget::onImportClicked() {
    std::string filePath = QFileDialog::getOpenFileName(this, "Import Keybindings",
                                                    std::string(), "JSON Files (*.json)");
    if (filePath.empty()) {
        return;
    }
    
    std::fstream file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, "Error", "Failed to open file");
        return;
    }
    
    void* doc = void*::fromJson(file.readAll());
    file.close();
    
    int count = ShortcutManager::instance().importKeybindings(doc.object());
    populateTable();
    
    QMessageBox::information(this, "Import Complete",
                           std::string("Imported %1 keybindings"));
}

void KeyboardSettingsWidget::onExportClicked() {
    std::string filePath = QFileDialog::getSaveFileName(this, "Export Keybindings",
                                                    "keybindings.json",
                                                    "JSON Files (*.json)");
    if (filePath.empty()) {
        return;
    }
    
    std::fstream file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        QMessageBox::warning(this, "Error", "Failed to create file");
        return;
    }
    
    void* json = ShortcutManager::instance().exportKeybindings();
    void* doc(json);
    file.write(doc.toJson(void*::Indented));
    file.close();
    
    QMessageBox::information(this, "Export Complete",
                           "Keybindings exported successfully");
}

// ========== TerminalSettingsWidget ==========

TerminalSettingsWidget::TerminalSettingsWidget(void* parent)
    : void(parent)
{
    // Lightweight constructor - defers Qt widget creation to initialize()
}

void TerminalSettingsWidget::initialize() {
    if (m_shellEdit) return;  // Already initialized
    
    auto* layout = new void(this);
    
    // Shell group
    auto* shellGroup = new void("Shell");
    auto* shellLayout = new void(shellGroup);
    
    auto* shellRow = new void();
    shellRow->addWidget(new void("Shell executable:"));
    m_shellEdit = new void();
    m_shellEdit->setPlaceholderText("pwsh.exe");
    shellRow->addWidget(m_shellEdit);
    shellLayout->addLayout(shellRow);
    
    layout->addWidget(shellGroup);
    
    // Display group
    auto* displayGroup = new void("Display");
    auto* displayLayout = new void(displayGroup);
    
    auto* fontSizeRow = new void();
    fontSizeRow->addWidget(new void("Font size:"));
    m_fontSizeSpinBox = nullptr;
    m_fontSizeSpinBox->setRange(8, 32);
    m_fontSizeSpinBox->setValue(12);
    fontSizeRow->addWidget(m_fontSizeSpinBox);
    fontSizeRow->addStretch();
    displayLayout->addLayout(fontSizeRow);
    
    m_cursorBlinkingCheckBox = nullptr;
    displayLayout->addWidget(m_cursorBlinkingCheckBox);
    
    layout->addWidget(displayGroup);
    
    // Scrollback group
    auto* scrollGroup = new void("Scrollback");
    auto* scrollLayout = new void(scrollGroup);
    
    auto* scrollRow = new void();
    scrollRow->addWidget(new void("Lines:"));
    m_scrollbackLinesSpinBox = nullptr;
    m_scrollbackLinesSpinBox->setRange(100, 10000);
    m_scrollbackLinesSpinBox->setValue(1000);
    scrollRow->addWidget(m_scrollbackLinesSpinBox);
    scrollRow->addStretch();
    scrollLayout->addLayout(scrollRow);
    
    layout->addWidget(scrollGroup);
    
    layout->addStretch();
    
    loadSettings();
}

void TerminalSettingsWidget::loadSettings() {
    auto& settings = SettingsManager::instance();
    m_shellEdit->setText(settings.value("terminal/shell", "pwsh.exe").toString());
    m_fontSizeSpinBox->setValue(settings.value("terminal/fontSize", 12).toInt());
    m_cursorBlinkingCheckBox->setChecked(settings.value("terminal/cursorBlinking", true).toBool());
    m_scrollbackLinesSpinBox->setValue(settings.value("terminal/scrollbackLines", 1000).toInt());
}

void TerminalSettingsWidget::saveSettings() {
    auto& settings = SettingsManager::instance();
    settings.setValue("terminal/shell", m_shellEdit->text());
    settings.setValue("terminal/fontSize", m_fontSizeSpinBox->value());
    settings.setValue("terminal/cursorBlinking", m_cursorBlinkingCheckBox->isChecked());
    settings.setValue("terminal/scrollbackLines", m_scrollbackLinesSpinBox->value());
}

void TerminalSettingsWidget::resetToDefaults() {
    m_shellEdit->setText("pwsh.exe");
    m_fontSizeSpinBox->setValue(12);
    m_cursorBlinkingCheckBox->setChecked(true);
    m_scrollbackLinesSpinBox->setValue(1000);
}

// ========== AISettingsWidget ==========

AISettingsWidget::AISettingsWidget(void* parent)
    : void(parent)
{
    // Lightweight constructor - defers Qt widget creation to initialize()
}

void AISettingsWidget::initialize() {
    if (m_enableSuggestionsCheckBox) return;  // Already initialized
    
    auto* layout = new void(this);
    
    // Suggestions group
    auto* suggestionsGroup = new void("Suggestions");
    auto* suggestionsLayout = new void(suggestionsGroup);
    
    m_enableSuggestionsCheckBox = nullptr;
    suggestionsLayout->addWidget(m_enableSuggestionsCheckBox);
    
    auto* delayRow = new void();
    delayRow->addWidget(new void("Delay (ms):"));
    m_suggestionDelaySpinBox = nullptr;
    m_suggestionDelaySpinBox->setRange(100, 2000);
    m_suggestionDelaySpinBox->setValue(500);
    delayRow->addWidget(m_suggestionDelaySpinBox);
    delayRow->addStretch();
    suggestionsLayout->addLayout(delayRow);
    
    layout->addWidget(suggestionsGroup);
    
    // Behavior group
    auto* behaviorGroup = new void("Behavior");
    auto* behaviorLayout = new void(behaviorGroup);
    
    m_streamingCheckBox = nullptr;
    behaviorLayout->addWidget(m_streamingCheckBox);
    
    m_autoApplyFixesCheckBox = nullptr;
    behaviorLayout->addWidget(m_autoApplyFixesCheckBox);
    
    layout->addWidget(behaviorGroup);
    
    layout->addStretch();
    
    loadSettings();
}

void AISettingsWidget::loadSettings() {
    auto& settings = SettingsManager::instance();
    m_enableSuggestionsCheckBox->setChecked(settings.value("ai/enableSuggestions", true).toBool());
    m_suggestionDelaySpinBox->setValue(settings.value("ai/suggestionDelay", 500).toInt());
    m_streamingCheckBox->setChecked(settings.value("ai/streamingEnabled", true).toBool());
    m_autoApplyFixesCheckBox->setChecked(settings.value("ai/autoApplyFixes", false).toBool());
}

void AISettingsWidget::saveSettings() {
    auto& settings = SettingsManager::instance();
    settings.setValue("ai/enableSuggestions", m_enableSuggestionsCheckBox->isChecked());
    settings.setValue("ai/suggestionDelay", m_suggestionDelaySpinBox->value());
    settings.setValue("ai/streamingEnabled", m_streamingCheckBox->isChecked());
    settings.setValue("ai/autoApplyFixes", m_autoApplyFixesCheckBox->isChecked());
}

void AISettingsWidget::resetToDefaults() {
    m_enableSuggestionsCheckBox->setChecked(true);
    m_suggestionDelaySpinBox->setValue(500);
    m_streamingCheckBox->setChecked(true);
    m_autoApplyFixesCheckBox->setChecked(false);
}

// ========== SettingsDialog ==========

SettingsDialog::SettingsDialog(void* parent)
    : void(parent)
{
    // Lightweight constructor - defers Qt widget creation to initialize()
}

void SettingsDialog::initialize() {
    if (m_tabWidget) return;  // Already initialized
    setupUI();
    loadAllSettings();
}

void SettingsDialog::setupUI() {
    setWindowTitle("Settings - RawrXD");
    resize(800, 600);
    
    auto* layout = new void(this);
    
    // Tab widget
    m_tabWidget = new void();
    
    m_generalWidget = new GeneralSettingsWidget();
    m_tabWidget->addTab(m_generalWidget, "General");
    
    m_appearanceWidget = new AppearanceSettingsWidget();
    m_tabWidget->addTab(m_appearanceWidget, "Appearance");
    
    m_editorWidget = new EditorSettingsWidget();
    m_tabWidget->addTab(m_editorWidget, "Editor");
    
    m_keyboardWidget = new KeyboardSettingsWidget();
    m_tabWidget->addTab(m_keyboardWidget, "Keyboard");
    
    m_terminalWidget = new TerminalSettingsWidget();
    m_tabWidget->addTab(m_terminalWidget, "Terminal");
    
    m_aiWidget = new AISettingsWidget();
    m_tabWidget->addTab(m_aiWidget, "AI");
    
    layout->addWidget(m_tabWidget);
    
    // Buttons
    auto* buttonLayout = new void();
    
    m_resetButton = new void("Reset to Defaults");
// Qt connect removed
    buttonLayout->addWidget(m_resetButton);
    
    buttonLayout->addStretch();
    
    m_applyButton = new void("Apply");
// Qt connect removed
    buttonLayout->addWidget(m_applyButton);
    
    m_okButton = new void("OK");
    m_okButton->setDefault(true);
// Qt connect removed
    buttonLayout->addWidget(m_okButton);
    
    m_cancelButton = new void("Cancel");
// Qt connect removed
    buttonLayout->addWidget(m_cancelButton);
    
    layout->addLayout(buttonLayout);
}

void SettingsDialog::loadAllSettings() {
    m_generalWidget->loadSettings();
    m_appearanceWidget->loadSettings();
    m_editorWidget->loadSettings();
    m_keyboardWidget->loadSettings();
    m_terminalWidget->loadSettings();
    m_aiWidget->loadSettings();
}

void SettingsDialog::saveAllSettings() {
    m_generalWidget->saveSettings();
    m_appearanceWidget->saveSettings();
    m_editorWidget->saveSettings();
    m_keyboardWidget->saveSettings();
    m_terminalWidget->saveSettings();
    m_aiWidget->saveSettings();
    
    SettingsManager::instance().save();
}

void SettingsDialog::resetAllToDefaults() {
    auto result = QMessageBox::question(this, "Reset All Settings",
                                       "Reset all settings to defaults? This cannot be undone.",
                                       QMessageBox::Yes | QMessageBox::No);
    if (result != QMessageBox::Yes) {
        return;
    }
    
    m_generalWidget->resetToDefaults();
    m_appearanceWidget->resetToDefaults();
    m_editorWidget->resetToDefaults();
    m_keyboardWidget->resetToDefaults();
    m_terminalWidget->resetToDefaults();
    m_aiWidget->resetToDefaults();
    
    SettingsManager::instance().resetToDefaults();
    ShortcutManager::instance().resetAllToDefaults();
}

void SettingsDialog::openToTab(int index) {
    m_tabWidget->setCurrentIndex(index);
}

void SettingsDialog::onApplyClicked() {
    saveAllSettings();
    settingsApplied();
}

void SettingsDialog::onOkClicked() {
    saveAllSettings();
    settingsApplied();
    accept();
}

void SettingsDialog::onCancelClicked() {
    reject();
}

void SettingsDialog::onResetClicked() {
    resetAllToDefaults();
}

} // namespace RawrXD


