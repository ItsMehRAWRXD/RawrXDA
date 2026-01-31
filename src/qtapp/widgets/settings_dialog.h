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
    void* m_autoSaveCheckBox;
    void* m_autoSaveIntervalSpinBox;
    void* m_restoreSessionCheckBox;
    void* m_checkUpdatesCheckBox;
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
    void* m_themeComboBox;
    void* m_colorSchemeComboBox;
    void* m_fontFamilyComboBox;
    void* m_fontSizeSpinBox;
    void* m_lineNumbersCheckBox;
    void* m_minimapCheckBox;
    void* m_previewLabel;
    
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
    void* m_tabSizeSpinBox;
    void* m_insertSpacesCheckBox;
    void* m_trimWhitespaceCheckBox;
    void* m_insertNewlineCheckBox;
    void* m_formatOnSaveCheckBox;
    void* m_lineEndingsComboBox;
    void* m_wordWrapCheckBox;
    void* m_cursorStyleComboBox;
    void* m_bracketMatchingCheckBox;
    void* m_autoCloseBracketsCheckBox;
    void* m_autoIndentCheckBox;
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
    
    void* m_searchEdit;
    QTableWidget* m_shortcutsTable;
    void* m_resetAllButton;
    void* m_importButton;
    void* m_exportButton;
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
    void* m_shellEdit;
    void* m_fontSizeSpinBox;
    void* m_cursorBlinkingCheckBox;
    void* m_scrollbackLinesSpinBox;
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
    void* m_enableSuggestionsCheckBox;
    void* m_suggestionDelaySpinBox;
    void* m_streamingCheckBox;
    void* m_autoApplyFixesCheckBox;
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
    
    void* m_tabWidget;
    
    GeneralSettingsWidget* m_generalWidget;
    AppearanceSettingsWidget* m_appearanceWidget;
    EditorSettingsWidget* m_editorWidget;
    KeyboardSettingsWidget* m_keyboardWidget;
    TerminalSettingsWidget* m_terminalWidget;
    AISettingsWidget* m_aiWidget;
    
    void* m_applyButton;
    void* m_okButton;
    void* m_cancelButton;
    void* m_resetButton;
};

} // namespace RawrXD

#endif // RAWRXD_SETTINGS_DIALOG_H

