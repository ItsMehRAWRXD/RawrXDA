/**
 * \file settings_dialog.h
 * \brief Settings dialog with tabbed interface
 * \author RawrXD Team
 * \date 2025-12-05
 */

#ifndef RAWRXD_SETTINGS_DIALOG_H
#define RAWRXD_SETTINGS_DIALOG_H

#include <QDialog>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLineEdit>
#include <QSpinBox>
#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QGroupBox>
#include <QTableWidget>
#include <QKeySequenceEdit>

namespace RawrXD {

/**
 * \brief General settings tab
 */
class GeneralSettingsWidget : public QWidget {
    Q_OBJECT
    
public:
    explicit GeneralSettingsWidget(QWidget* parent = nullptr);
    
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
class AppearanceSettingsWidget : public QWidget {
    Q_OBJECT
    
public:
    explicit AppearanceSettingsWidget(QWidget* parent = nullptr);
    
    void loadSettings();
    void saveSettings();
    void resetToDefaults();
    
signals:
    void themeChanged(const QString& theme);
    void fontChanged(const QString& family, int size);
    
private slots:
    void onThemeChanged(int index);
    void onFontFamilyChanged(const QString& family);
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
class EditorSettingsWidget : public QWidget {
    Q_OBJECT
    
public:
    explicit EditorSettingsWidget(QWidget* parent = nullptr);
    
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
class KeyboardSettingsWidget : public QWidget {
    Q_OBJECT
    
public:
    explicit KeyboardSettingsWidget(QWidget* parent = nullptr);
    
    void loadSettings();
    void saveSettings();
    void resetToDefaults();
    
private slots:
    void onSearchTextChanged(const QString& text);
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
class TerminalSettingsWidget : public QWidget {
    Q_OBJECT
    
public:
    explicit TerminalSettingsWidget(QWidget* parent = nullptr);
    
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
class AISettingsWidget : public QWidget {
    Q_OBJECT
    
public:
    explicit AISettingsWidget(QWidget* parent = nullptr);
    
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
class SettingsDialog : public QDialog {
    Q_OBJECT
    
public:
    explicit SettingsDialog(QWidget* parent = nullptr);
    
    /**
     * \brief Open dialog to specific tab
     */
    void openToTab(int index);
    
signals:
    void settingsApplied();
    
private slots:
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
