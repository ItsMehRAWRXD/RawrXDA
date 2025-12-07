/**
 * \file settings_dialog.cpp
 * \brief Implementation of settings dialog with tabbed interface
 * \author RawrXD Team
 * \date 2025-12-05
 */

#include "settings_dialog.h"
#include "../utils/settings_manager.h"
#include "../utils/shortcut_manager.h"
#include <QFontDatabase>
#include <QFileDialog>
#include <QMessageBox>
#include <QHeaderView>
#include <QJsonDocument>
#include <QFile>

namespace RawrXD {

// ========== GeneralSettingsWidget ==========

GeneralSettingsWidget::GeneralSettingsWidget(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    
    // Auto-save group
    auto* autoSaveGroup = new QGroupBox("Auto Save");
    auto* autoSaveLayout = new QVBoxLayout(autoSaveGroup);
    
    m_autoSaveCheckBox = new QCheckBox("Enable auto save");
    autoSaveLayout->addWidget(m_autoSaveCheckBox);
    
    auto* intervalLayout = new QHBoxLayout();
    intervalLayout->addWidget(new QLabel("Save interval (seconds):"));
    m_autoSaveIntervalSpinBox = new QSpinBox();
    m_autoSaveIntervalSpinBox->setRange(5, 300);
    m_autoSaveIntervalSpinBox->setValue(30);
    intervalLayout->addWidget(m_autoSaveIntervalSpinBox);
    intervalLayout->addStretch();
    autoSaveLayout->addLayout(intervalLayout);
    
    layout->addWidget(autoSaveGroup);
    
    // Session group
    auto* sessionGroup = new QGroupBox("Session");
    auto* sessionLayout = new QVBoxLayout(sessionGroup);
    
    m_restoreSessionCheckBox = new QCheckBox("Restore files and layout on startup");
    sessionLayout->addWidget(m_restoreSessionCheckBox);
    
    layout->addWidget(sessionGroup);
    
    // Updates group
    auto* updatesGroup = new QGroupBox("Updates");
    auto* updatesLayout = new QVBoxLayout(updatesGroup);
    
    m_checkUpdatesCheckBox = new QCheckBox("Automatically check for updates");
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

AppearanceSettingsWidget::AppearanceSettingsWidget(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    
    // Theme group
    auto* themeGroup = new QGroupBox("Theme");
    auto* themeLayout = new QVBoxLayout(themeGroup);
    
    auto* themeRow = new QHBoxLayout();
    themeRow->addWidget(new QLabel("Theme:"));
    m_themeComboBox = new QComboBox();
    m_themeComboBox->addItems({"Dark", "Light", "High Contrast Dark", "High Contrast Light"});
    connect(m_themeComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &AppearanceSettingsWidget::onThemeChanged);
    themeRow->addWidget(m_themeComboBox);
    themeRow->addStretch();
    themeLayout->addLayout(themeRow);
    
    auto* schemeRow = new QHBoxLayout();
    schemeRow->addWidget(new QLabel("Color Scheme:"));
    m_colorSchemeComboBox = new QComboBox();
    m_colorSchemeComboBox->addItems({"Dark Modern", "Dark Classic", "Monokai", "Solarized Dark", "Dracula"});
    schemeRow->addWidget(m_colorSchemeComboBox);
    schemeRow->addStretch();
    themeLayout->addLayout(schemeRow);
    
    layout->addWidget(themeGroup);
    
    // Font group
    auto* fontGroup = new QGroupBox("Font");
    auto* fontLayout = new QVBoxLayout(fontGroup);
    
    auto* familyRow = new QHBoxLayout();
    familyRow->addWidget(new QLabel("Font Family:"));
    m_fontFamilyComboBox = new QComboBox();
    m_fontFamilyComboBox->addItems(QFontDatabase::families(QFontDatabase::Latin));
    m_fontFamilyComboBox->setCurrentText("Consolas");
    connect(m_fontFamilyComboBox, &QComboBox::currentTextChanged,
            this, &AppearanceSettingsWidget::onFontFamilyChanged);
    familyRow->addWidget(m_fontFamilyComboBox);
    familyRow->addStretch();
    fontLayout->addLayout(familyRow);
    
    auto* sizeRow = new QHBoxLayout();
    sizeRow->addWidget(new QLabel("Font Size:"));
    m_fontSizeSpinBox = new QSpinBox();
    m_fontSizeSpinBox->setRange(8, 32);
    m_fontSizeSpinBox->setValue(12);
    connect(m_fontSizeSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &AppearanceSettingsWidget::onFontSizeChanged);
    sizeRow->addWidget(m_fontSizeSpinBox);
    sizeRow->addStretch();
    fontLayout->addLayout(sizeRow);
    
    layout->addWidget(fontGroup);
    
    // Display options group
    auto* displayGroup = new QGroupBox("Display Options");
    auto* displayLayout = new QVBoxLayout(displayGroup);
    
    m_lineNumbersCheckBox = new QCheckBox("Show line numbers");
    displayLayout->addWidget(m_lineNumbersCheckBox);
    
    m_minimapCheckBox = new QCheckBox("Show minimap");
    displayLayout->addWidget(m_minimapCheckBox);
    
    layout->addWidget(displayGroup);
    
    // Preview
    auto* previewGroup = new QGroupBox("Preview");
    auto* previewLayout = new QVBoxLayout(previewGroup);
    
    m_previewLabel = new QLabel("The quick brown fox jumps over the lazy dog\n0123456789");
    m_previewLabel->setStyleSheet("QLabel { padding: 10px; background-color: #1e1e1e; color: #d4d4d4; }");
    previewLayout->addWidget(m_previewLabel);
    
    layout->addWidget(previewGroup);
    
    layout->addStretch();
    
    loadSettings();
    updatePreview();
}

void AppearanceSettingsWidget::loadSettings() {
    auto& settings = SettingsManager::instance();
    
    QString theme = settings.theme();
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
    
    QStringList themes = {"dark", "light", "hc-dark", "hc-light"};
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
    QStringList themes = {"dark", "light", "hc-dark", "hc-light"};
    emit themeChanged(themes[index]);
}

void AppearanceSettingsWidget::onFontFamilyChanged(const QString& family) {
    updatePreview();
    emit fontChanged(family, m_fontSizeSpinBox->value());
}

void AppearanceSettingsWidget::onFontSizeChanged(int size) {
    updatePreview();
    emit fontChanged(m_fontFamilyComboBox->currentText(), size);
}

void AppearanceSettingsWidget::updatePreview() {
    QFont font(m_fontFamilyComboBox->currentText(), m_fontSizeSpinBox->value());
    m_previewLabel->setFont(font);
    
    // Update background based on theme
    if (m_themeComboBox->currentIndex() == 1 || m_themeComboBox->currentIndex() == 3) {
        // Light theme
        m_previewLabel->setStyleSheet("QLabel { padding: 10px; background-color: #ffffff; color: #000000; }");
    } else {
        // Dark theme
        m_previewLabel->setStyleSheet("QLabel { padding: 10px; background-color: #1e1e1e; color: #d4d4d4; }");
    }
}

// ========== EditorSettingsWidget ==========

EditorSettingsWidget::EditorSettingsWidget(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    
    // Indentation group
    auto* indentGroup = new QGroupBox("Indentation");
    auto* indentLayout = new QVBoxLayout(indentGroup);
    
    auto* tabSizeRow = new QHBoxLayout();
    tabSizeRow->addWidget(new QLabel("Tab size:"));
    m_tabSizeSpinBox = new QSpinBox();
    m_tabSizeSpinBox->setRange(1, 8);
    m_tabSizeSpinBox->setValue(4);
    tabSizeRow->addWidget(m_tabSizeSpinBox);
    tabSizeRow->addStretch();
    indentLayout->addLayout(tabSizeRow);
    
    m_insertSpacesCheckBox = new QCheckBox("Insert spaces instead of tabs");
    indentLayout->addWidget(m_insertSpacesCheckBox);
    
    m_autoIndentCheckBox = new QCheckBox("Auto indent");
    indentLayout->addWidget(m_autoIndentCheckBox);
    
    layout->addWidget(indentGroup);
    
    // Formatting group
    auto* formatGroup = new QGroupBox("Formatting");
    auto* formatLayout = new QVBoxLayout(formatGroup);
    
    m_trimWhitespaceCheckBox = new QCheckBox("Trim trailing whitespace on save");
    formatLayout->addWidget(m_trimWhitespaceCheckBox);
    
    m_insertNewlineCheckBox = new QCheckBox("Insert final newline");
    formatLayout->addWidget(m_insertNewlineCheckBox);
    
    m_formatOnSaveCheckBox = new QCheckBox("Format on save");
    formatLayout->addWidget(m_formatOnSaveCheckBox);
    
    auto* lineEndingsRow = new QHBoxLayout();
    lineEndingsRow->addWidget(new QLabel("Line endings:"));
    m_lineEndingsComboBox = new QComboBox();
    m_lineEndingsComboBox->addItems({"Auto", "LF (Unix)", "CRLF (Windows)"});
    lineEndingsRow->addWidget(m_lineEndingsComboBox);
    lineEndingsRow->addStretch();
    formatLayout->addLayout(lineEndingsRow);
    
    layout->addWidget(formatGroup);
    
    // Display group
    auto* displayGroup = new QGroupBox("Display");
    auto* displayLayout = new QVBoxLayout(displayGroup);
    
    m_wordWrapCheckBox = new QCheckBox("Word wrap");
    displayLayout->addWidget(m_wordWrapCheckBox);
    
    auto* cursorRow = new QHBoxLayout();
    cursorRow->addWidget(new QLabel("Cursor style:"));
    m_cursorStyleComboBox = new QComboBox();
    m_cursorStyleComboBox->addItems({"Line", "Block", "Underline"});
    cursorRow->addWidget(m_cursorStyleComboBox);
    cursorRow->addStretch();
    displayLayout->addLayout(cursorRow);
    
    layout->addWidget(displayGroup);
    
    // Features group
    auto* featuresGroup = new QGroupBox("Features");
    auto* featuresLayout = new QVBoxLayout(featuresGroup);
    
    m_bracketMatchingCheckBox = new QCheckBox("Bracket matching");
    featuresLayout->addWidget(m_bracketMatchingCheckBox);
    
    m_autoCloseBracketsCheckBox = new QCheckBox("Auto close brackets");
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
    
    QString lineEndings = settings.lineEndings();
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
    
    QStringList endings = {"Auto", "LF", "CRLF"};
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

KeyboardSettingsWidget::KeyboardSettingsWidget(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    
    // Search bar
    auto* searchLayout = new QHBoxLayout();
    searchLayout->addWidget(new QLabel("Search:"));
    m_searchEdit = new QLineEdit();
    m_searchEdit->setPlaceholderText("Type to filter shortcuts...");
    connect(m_searchEdit, &QLineEdit::textChanged,
            this, &KeyboardSettingsWidget::onSearchTextChanged);
    searchLayout->addWidget(m_searchEdit);
    layout->addLayout(searchLayout);
    
    // Shortcuts table
    m_shortcutsTable = new QTableWidget();
    m_shortcutsTable->setColumnCount(3);
    m_shortcutsTable->setHorizontalHeaderLabels({"Command", "Key Binding", "Context"});
    m_shortcutsTable->horizontalHeader()->setStretchLastSection(false);
    m_shortcutsTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_shortcutsTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_shortcutsTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_shortcutsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_shortcutsTable->setEditTriggers(QAbstractItemView::DoubleClicked);
    connect(m_shortcutsTable, &QTableWidget::cellChanged,
            this, &KeyboardSettingsWidget::onShortcutEdited);
    layout->addWidget(m_shortcutsTable);
    
    // Buttons
    auto* buttonLayout = new QHBoxLayout();
    m_resetAllButton = new QPushButton("Reset All");
    connect(m_resetAllButton, &QPushButton::clicked,
            this, &KeyboardSettingsWidget::onResetAllClicked);
    buttonLayout->addWidget(m_resetAllButton);
    
    m_importButton = new QPushButton("Import...");
    connect(m_importButton, &QPushButton::clicked,
            this, &KeyboardSettingsWidget::onImportClicked);
    buttonLayout->addWidget(m_importButton);
    
    m_exportButton = new QPushButton("Export...");
    connect(m_exportButton, &QPushButton::clicked,
            this, &KeyboardSettingsWidget::onExportClicked);
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
    
    QStringList contextNames = {"Global", "Editor", "Project Explorer", "Terminal", "Find Widget"};
    
    for (int i = 0; i < shortcuts.size(); ++i) {
        const auto& info = shortcuts[i];
        
        // Command name
        auto* nameItem = new QTableWidgetItem(info.displayName);
        nameItem->setFlags(nameItem->flags() & ~Qt::ItemIsEditable);
        nameItem->setData(Qt::UserRole, info.id);
        m_shortcutsTable->setItem(i, 0, nameItem);
        
        // Key binding
        auto* keyItem = new QTableWidgetItem(info.currentKey.toString());
        m_shortcutsTable->setItem(i, 1, keyItem);
        
        // Context
        auto* contextItem = new QTableWidgetItem(contextNames[info.context]);
        contextItem->setFlags(contextItem->flags() & ~Qt::ItemIsEditable);
        m_shortcutsTable->setItem(i, 2, contextItem);
    }
    
    m_shortcutsTable->blockSignals(false);
}

void KeyboardSettingsWidget::filterTable() {
    QString filter = m_searchEdit->text().toLower();
    
    for (int i = 0; i < m_shortcutsTable->rowCount(); ++i) {
        QString command = m_shortcutsTable->item(i, 0)->text().toLower();
        QString key = m_shortcutsTable->item(i, 1)->text().toLower();
        
        bool match = command.contains(filter) || key.contains(filter);
        m_shortcutsTable->setRowHidden(i, !match);
    }
}

void KeyboardSettingsWidget::onSearchTextChanged(const QString& text) {
    filterTable();
}

void KeyboardSettingsWidget::onShortcutEdited(int row, int column) {
    if (column != 1) {
        return;
    }
    
    QString id = m_shortcutsTable->item(row, 0)->data(Qt::UserRole).toString();
    QString keyText = m_shortcutsTable->item(row, 1)->text();
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
    QString filePath = QFileDialog::getOpenFileName(this, "Import Keybindings",
                                                    QString(), "JSON Files (*.json)");
    if (filePath.isEmpty()) {
        return;
    }
    
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, "Error", "Failed to open file");
        return;
    }
    
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();
    
    int count = ShortcutManager::instance().importKeybindings(doc.object());
    populateTable();
    
    QMessageBox::information(this, "Import Complete",
                           QString("Imported %1 keybindings").arg(count));
}

void KeyboardSettingsWidget::onExportClicked() {
    QString filePath = QFileDialog::getSaveFileName(this, "Export Keybindings",
                                                    "keybindings.json",
                                                    "JSON Files (*.json)");
    if (filePath.isEmpty()) {
        return;
    }
    
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        QMessageBox::warning(this, "Error", "Failed to create file");
        return;
    }
    
    QJsonObject json = ShortcutManager::instance().exportKeybindings();
    QJsonDocument doc(json);
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();
    
    QMessageBox::information(this, "Export Complete",
                           "Keybindings exported successfully");
}

// ========== TerminalSettingsWidget ==========

TerminalSettingsWidget::TerminalSettingsWidget(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    
    // Shell group
    auto* shellGroup = new QGroupBox("Shell");
    auto* shellLayout = new QVBoxLayout(shellGroup);
    
    auto* shellRow = new QHBoxLayout();
    shellRow->addWidget(new QLabel("Shell executable:"));
    m_shellEdit = new QLineEdit();
    m_shellEdit->setPlaceholderText("pwsh.exe");
    shellRow->addWidget(m_shellEdit);
    shellLayout->addLayout(shellRow);
    
    layout->addWidget(shellGroup);
    
    // Display group
    auto* displayGroup = new QGroupBox("Display");
    auto* displayLayout = new QVBoxLayout(displayGroup);
    
    auto* fontSizeRow = new QHBoxLayout();
    fontSizeRow->addWidget(new QLabel("Font size:"));
    m_fontSizeSpinBox = new QSpinBox();
    m_fontSizeSpinBox->setRange(8, 32);
    m_fontSizeSpinBox->setValue(12);
    fontSizeRow->addWidget(m_fontSizeSpinBox);
    fontSizeRow->addStretch();
    displayLayout->addLayout(fontSizeRow);
    
    m_cursorBlinkingCheckBox = new QCheckBox("Cursor blinking");
    displayLayout->addWidget(m_cursorBlinkingCheckBox);
    
    layout->addWidget(displayGroup);
    
    // Scrollback group
    auto* scrollGroup = new QGroupBox("Scrollback");
    auto* scrollLayout = new QVBoxLayout(scrollGroup);
    
    auto* scrollRow = new QHBoxLayout();
    scrollRow->addWidget(new QLabel("Lines:"));
    m_scrollbackLinesSpinBox = new QSpinBox();
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

AISettingsWidget::AISettingsWidget(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    
    // Suggestions group
    auto* suggestionsGroup = new QGroupBox("Suggestions");
    auto* suggestionsLayout = new QVBoxLayout(suggestionsGroup);
    
    m_enableSuggestionsCheckBox = new QCheckBox("Enable AI suggestions");
    suggestionsLayout->addWidget(m_enableSuggestionsCheckBox);
    
    auto* delayRow = new QHBoxLayout();
    delayRow->addWidget(new QLabel("Delay (ms):"));
    m_suggestionDelaySpinBox = new QSpinBox();
    m_suggestionDelaySpinBox->setRange(100, 2000);
    m_suggestionDelaySpinBox->setValue(500);
    delayRow->addWidget(m_suggestionDelaySpinBox);
    delayRow->addStretch();
    suggestionsLayout->addLayout(delayRow);
    
    layout->addWidget(suggestionsGroup);
    
    // Behavior group
    auto* behaviorGroup = new QGroupBox("Behavior");
    auto* behaviorLayout = new QVBoxLayout(behaviorGroup);
    
    m_streamingCheckBox = new QCheckBox("Enable streaming responses");
    behaviorLayout->addWidget(m_streamingCheckBox);
    
    m_autoApplyFixesCheckBox = new QCheckBox("Automatically apply fixes");
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

SettingsDialog::SettingsDialog(QWidget* parent)
    : QDialog(parent)
{
    setupUI();
    loadAllSettings();
}

void SettingsDialog::setupUI() {
    setWindowTitle("Settings - RawrXD");
    resize(800, 600);
    
    auto* layout = new QVBoxLayout(this);
    
    // Tab widget
    m_tabWidget = new QTabWidget();
    
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
    auto* buttonLayout = new QHBoxLayout();
    
    m_resetButton = new QPushButton("Reset to Defaults");
    connect(m_resetButton, &QPushButton::clicked,
            this, &SettingsDialog::onResetClicked);
    buttonLayout->addWidget(m_resetButton);
    
    buttonLayout->addStretch();
    
    m_applyButton = new QPushButton("Apply");
    connect(m_applyButton, &QPushButton::clicked,
            this, &SettingsDialog::onApplyClicked);
    buttonLayout->addWidget(m_applyButton);
    
    m_okButton = new QPushButton("OK");
    m_okButton->setDefault(true);
    connect(m_okButton, &QPushButton::clicked,
            this, &SettingsDialog::onOkClicked);
    buttonLayout->addWidget(m_okButton);
    
    m_cancelButton = new QPushButton("Cancel");
    connect(m_cancelButton, &QPushButton::clicked,
            this, &SettingsDialog::onCancelClicked);
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
    emit settingsApplied();
}

void SettingsDialog::onOkClicked() {
    saveAllSettings();
    emit settingsApplied();
    accept();
}

void SettingsDialog::onCancelClicked() {
    reject();
}

void SettingsDialog::onResetClicked() {
    resetAllToDefaults();
}

} // namespace RawrXD
