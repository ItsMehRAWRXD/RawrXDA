/**
 * \file settings_dialog.h
 * \brief Settings dialog with tabbed interface
 * \author RawrXD Team
 * \date 2025-12-05
 */

#ifndef RAWRXD_SETTINGS_DIALOG_H
#define RAWRXD_SETTINGS_DIALOG_H


namespace RawrXD {

/**
 * \brief General settings tab
 */
class GeneralSettingsWidget : public void {

public:
    explicit GeneralSettingsWidget(void* parent = nullptr);
    
    void initialize();  // Two-phase init
    
    void loadSettings();
    void saveSettings();
    void resetToDefaults();
    
private:
    QCheckBox* m_autoSaveCheckBox;
    QSpinBox* m_autoSaveIntervalSpinBox;
    QCheckBox* m_restoreSessionCheckBox;
    QCheckBox* m_checkUpdatesCheckBox;
};

/**
 * \brief Appearance settings tab
 */
class AppearanceSettingsWidget : public void {

public:
    explicit AppearanceSettingsWidget(void* parent = nullptr);
    
    void initialize();  // Two-phase init
    
    void loadSettings();
    void saveSettings();
    void resetToDefaults();
    

    void themeChanged(const std::string& theme);
    void fontChanged(const std::string& family, int size);
    
private:
    void onThemeChanged(int index);
    void onFontFamilyChanged(const std::string& family);
    void onFontSizeChanged(int size);
    
private:
    QComboBox* m_themeComboBox;
    QComboBox* m_colorSchemeComboBox;
    QComboBox* m_fontFamilyComboBox;
    QSpinBox* m_fontSizeSpinBox;
    QCheckBox* m_lineNumbersCheckBox;
    QCheckBox* m_minimapCheckBox;
    QLabel* m_previewLabel;
    
    void updatePreview();
};

/**
 * \brief Editor settings tab
 */
class EditorSettingsWidget : public void {

public:
    explicit EditorSettingsWidget(void* parent = nullptr);
    
    void initialize();  // Two-phase init
    
    void loadSettings();
    void saveSettings();
    void resetToDefaults();
    
private:
    QSpinBox* m_tabSizeSpinBox;
    QCheckBox* m_insertSpacesCheckBox;
    QCheckBox* m_trimWhitespaceCheckBox;
    QCheckBox* m_insertNewlineCheckBox;
    QCheckBox* m_formatOnSaveCheckBox;
    QComboBox* m_lineEndingsComboBox;
    QCheckBox* m_wordWrapCheckBox;
    QComboBox* m_cursorStyleComboBox;
    QCheckBox* m_bracketMatchingCheckBox;
    QCheckBox* m_autoCloseBracketsCheckBox;
    QCheckBox* m_autoIndentCheckBox;
};

/**
 * \brief Keyboard shortcuts settings tab
 */
class KeyboardSettingsWidget : public void {

public:
    explicit KeyboardSettingsWidget(void* parent = nullptr);
    
    void initialize();  // Two-phase init
    
    void loadSettings();
    void saveSettings();
    void resetToDefaults();
    
private:
    void onSearchTextChanged(const std::string& text);
    void onShortcutEdited(int row, int column);
    void onResetAllClicked();
    void onImportClicked();
    void onExportClicked();
    
private:
    void populateTable();
    void filterTable();
    
    QLineEdit* m_searchEdit;
    QTableWidget* m_shortcutsTable;
    QPushButton* m_resetAllButton;
    QPushButton* m_importButton;
    QPushButton* m_exportButton;
};

/**
 * \brief Terminal settings tab
 */
class TerminalSettingsWidget : public void {

public:
    explicit TerminalSettingsWidget(void* parent = nullptr);
    
    void initialize();  // Two-phase init
    
    void loadSettings();
    void saveSettings();
    void resetToDefaults();
    
private:
    QLineEdit* m_shellEdit;
    QSpinBox* m_fontSizeSpinBox;
    QCheckBox* m_cursorBlinkingCheckBox;
    QSpinBox* m_scrollbackLinesSpinBox;
};

/**
 * \brief AI settings tab
 */
class AISettingsWidget : public void {

public:
    explicit AISettingsWidget(void* parent = nullptr);
    
    void initialize();  // Two-phase init
    
    void loadSettings();
    void saveSettings();
    void resetToDefaults();
    
private:
    QCheckBox* m_enableSuggestionsCheckBox;
    QSpinBox* m_suggestionDelaySpinBox;
    QCheckBox* m_streamingCheckBox;
    QCheckBox* m_autoApplyFixesCheckBox;
};

/**
 * \brief Main settings dialog
 * 
 * Features:
 * - Tabbed interface for different categories
 * - Live preview of appearance changes
 * - Apply/Cancel/OK buttons
 * - Keyboard shortcut customization
 * - Import/Export settings
 */
class SettingsDialog : public void {

public:
    explicit SettingsDialog(void* parent = nullptr);
    
    void initialize();  // Two-phase init
    
    /**
     * \brief Open dialog to specific tab
     */
    void openToTab(int index);
    

    void settingsApplied();
    
private:
    void onApplyClicked();
    void onOkClicked();
    void onCancelClicked();
    void onResetClicked();
    
private:
    void setupUI();
    void loadAllSettings();
    void saveAllSettings();
    void resetAllToDefaults();
    
    QTabWidget* m_tabWidget;
    
    GeneralSettingsWidget* m_generalWidget;
    AppearanceSettingsWidget* m_appearanceWidget;
    EditorSettingsWidget* m_editorWidget;
    KeyboardSettingsWidget* m_keyboardWidget;
    TerminalSettingsWidget* m_terminalWidget;
    AISettingsWidget* m_aiWidget;
    
    QPushButton* m_applyButton;
    QPushButton* m_okButton;
    QPushButton* m_cancelButton;
    QPushButton* m_resetButton;
};

} // namespace RawrXD

#endif // RAWRXD_SETTINGS_DIALOG_H

