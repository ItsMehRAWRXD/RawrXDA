/**
 * @file shortcuts_configurator.h
 * @brief Keyboard shortcuts editor with conflict detection, import/export
 * 
 * Production-ready keyboard shortcut configuration widget providing:
 * - Full keyboard shortcut listing and editing
 * - Conflict detection with resolution suggestions
 * - Category-based organization
 * - Search and filter functionality
 * - Import/export shortcuts configurations
 * - Reset to defaults per-shortcut or globally
 * - Multi-chord shortcut support (Ctrl+K, Ctrl+C)
 * - Context-aware shortcuts (when clauses)
 */

#ifndef SHORTCUTS_CONFIGURATOR_H
#define SHORTCUTS_CONFIGURATOR_H

#include <QWidget>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QLineEdit>
#include <QKeySequenceEdit>
#include <QPushButton>
#include <QComboBox>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QGroupBox>
#include <QListWidget>
#include <QMap>
#include <QSet>
#include <QKeySequence>
#include <QJsonObject>
#include <QJsonArray>
#include <QTimer>
#include <QAction>

/**
 * @struct ShortcutInfo
 * @brief Represents a single keyboard shortcut binding
 */
struct ShortcutInfo {
    QString id;                    // Unique identifier (e.g., "editor.copy")
    QString name;                  // Display name (e.g., "Copy")
    QString description;           // Detailed description
    QString category;              // Category for grouping (e.g., "Editor", "File")
    QKeySequence defaultShortcut;  // Default key binding
    QKeySequence currentShortcut;  // Current key binding (may differ from default)
    QKeySequence secondaryShortcut;// Optional secondary binding
    QString whenClause;            // Context condition (e.g., "editorTextFocus")
    bool isCustomized = false;     // Whether user has modified this shortcut
    bool isEnabled = true;         // Whether shortcut is active
    
    bool hasConflict() const;
    QString conflictWith;          // ID of conflicting shortcut if any
};

/**
 * @struct ShortcutCategory
 * @brief Represents a category of shortcuts
 */
struct ShortcutCategory {
    QString id;
    QString name;
    QString icon;
    QList<ShortcutInfo> shortcuts;
};

/**
 * @class ShortcutConflictInfo
 * @brief Information about a keyboard shortcut conflict
 */
struct ShortcutConflictInfo {
    QString shortcut1Id;
    QString shortcut2Id;
    QKeySequence conflictingSequence;
    QString resolution;  // Suggested resolution
};

/**
 * @class KeySequenceRecorder
 * @brief Custom widget for recording multi-chord key sequences
 */
class KeySequenceRecorder : public QWidget {
    Q_OBJECT

public:
    explicit KeySequenceRecorder(QWidget* parent = nullptr);
    
    QKeySequence keySequence() const { return m_sequence; }
    void setKeySequence(const QKeySequence& sequence);
    void clear();
    
    bool isRecording() const { return m_recording; }

signals:
    void keySequenceChanged(const QKeySequence& sequence);
    void recordingStarted();
    void recordingStopped();

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;
    void focusInEvent(QFocusEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;

private slots:
    void onRecordTimeout();

private:
    void startRecording();
    void stopRecording();
    void updateDisplay();
    
    QKeySequence m_sequence;
    QList<int> m_currentKeys;
    bool m_recording = false;
    QTimer* m_recordTimer;
    QLabel* m_displayLabel;
    QPushButton* m_clearButton;
};

/**
 * @class ShortcutsConfigurator
 * @brief Full keyboard shortcuts editor with conflict detection
 */
class ShortcutsConfigurator : public QWidget {
    Q_OBJECT

public:
    explicit ShortcutsConfigurator(QWidget* parent = nullptr);
    ~ShortcutsConfigurator();
    
    // Shortcut management
    void loadShortcuts();
    void saveShortcuts();
    void addShortcut(const ShortcutInfo& shortcut);
    void updateShortcut(const QString& id, const QKeySequence& sequence);
    void removeShortcut(const QString& id);
    void resetShortcut(const QString& id);
    void resetAllShortcuts();
    
    // Category management
    void addCategory(const ShortcutCategory& category);
    QStringList categories() const;
    
    // Conflict detection
    QList<ShortcutConflictInfo> detectConflicts() const;
    bool hasConflict(const QKeySequence& sequence, const QString& excludeId = QString()) const;
    QStringList getConflictingShortcuts(const QKeySequence& sequence) const;
    
    // Import/Export
    bool importShortcuts(const QString& filePath);
    bool exportShortcuts(const QString& filePath) const;
    QJsonObject toJson() const;
    void fromJson(const QJsonObject& json);
    
    // Search and filter
    void setSearchFilter(const QString& filter);
    void setCategoryFilter(const QString& category);
    void setShowCustomizedOnly(bool show);
    void setShowConflictsOnly(bool show);

signals:
    void shortcutChanged(const QString& id, const QKeySequence& oldSeq, const QKeySequence& newSeq);
    void shortcutsModified();
    void conflictDetected(const ShortcutConflictInfo& conflict);
    void shortcutsImported(int count);
    void shortcutsExported(const QString& path);

private slots:
    void onSearchTextChanged(const QString& text);
    void onCategoryChanged(int index);
    void onShortcutItemSelected(QTreeWidgetItem* item, int column);
    void onShortcutDoubleClicked(QTreeWidgetItem* item, int column);
    void onKeySequenceChanged(const QKeySequence& sequence);
    void onResetSelectedClicked();
    void onResetAllClicked();
    void onImportClicked();
    void onExportClicked();
    void onApplyClicked();
    void onShowConflictsToggled(bool checked);
    void onShowCustomizedToggled(bool checked);

private:
    void setupUI();
    void setupConnections();
    void populateDefaultShortcuts();
    void populateTree();
    void updateTreeItem(QTreeWidgetItem* item, const ShortcutInfo& shortcut);
    QTreeWidgetItem* findItemById(const QString& id) const;
    void highlightConflicts();
    void showConflictDialog(const QList<QString>& conflictingIds);
    QString formatKeySequence(const QKeySequence& sequence) const;
    ShortcutInfo* findShortcut(const QString& id);
    const ShortcutInfo* findShortcut(const QString& id) const;
    
    // UI Components
    QLineEdit* m_searchEdit;
    QComboBox* m_categoryCombo;
    QPushButton* m_showConflictsBtn;
    QPushButton* m_showCustomizedBtn;
    QTreeWidget* m_shortcutsTree;
    QGroupBox* m_editGroup;
    QLabel* m_shortcutNameLabel;
    QLabel* m_shortcutDescLabel;
    KeySequenceRecorder* m_keySequenceRecorder;
    QLabel* m_conflictWarningLabel;
    QListWidget* m_conflictList;
    QPushButton* m_resetSelectedBtn;
    QPushButton* m_resetAllBtn;
    QPushButton* m_importBtn;
    QPushButton* m_exportBtn;
    QPushButton* m_applyBtn;
    
    // Data
    QMap<QString, ShortcutCategory> m_categories;
    QMap<QString, ShortcutInfo> m_shortcuts;
    QMap<QKeySequence, QSet<QString>> m_shortcutIndex;  // For conflict detection
    QString m_currentFilter;
    QString m_currentCategory;
    QString m_selectedShortcutId;
    bool m_showCustomizedOnly = false;
    bool m_showConflictsOnly = false;
    bool m_modified = false;
};

#endif // SHORTCUTS_CONFIGURATOR_H
