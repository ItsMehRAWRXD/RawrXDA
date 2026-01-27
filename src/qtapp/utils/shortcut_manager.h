/**
 * \file shortcut_manager.h
 * \brief Keyboard shortcut management with customization
 * \author RawrXD Team
 * \date 2025-12-05
 */

#ifndef RAWRXD_SHORTCUT_MANAGER_H
#define RAWRXD_SHORTCUT_MANAGER_H

#include <QObject>
#include <QKeySequence>
#include <QAction>
#include <QHash>
#include <QString>
#include <QJsonObject>

namespace RawrXD {

/**
 * \brief Manages keyboard shortcuts and customization
 * 
 * Features:
 * - Default shortcut assignments
 * - User customization
 * - Conflict detection
 * - Keymap import/export
 * - Context-aware shortcuts
 */
class ShortcutManager : public QObject {
    Q_OBJECT
    
public:
    /**
     * \brief Shortcut context (where it applies)
     */
    enum Context {
        Global,          ///< Applies everywhere
        Editor,          ///< Only in text editor
        ProjectExplorer, ///< Only in project explorer
        Terminal,        ///< Only in terminal
        FindWidget       ///< Only in find/replace
    };
    
    /**
     * \brief Shortcut information
     */
    struct ShortcutInfo {
        QString id;              ///< Unique identifier
        QString displayName;     ///< Human-readable name
        QKeySequence defaultKey; ///< Default key binding
        QKeySequence currentKey; ///< Current key binding
        Context context;         ///< Where shortcut applies
        QString description;     ///< What the shortcut does
        QAction* action;         ///< Associated QAction (if any)
        
        ShortcutInfo() : context(Global), action(nullptr) {}
    };
    
    static ShortcutManager& instance();
    
    /**
     * \brief Register a shortcut
     * \param id Unique identifier (e.g., "file.save")
     * \param displayName Human-readable name
     * \param defaultKey Default key sequence
     * \param context Where shortcut applies
     * \param description What the shortcut does
     * \param action Associated QAction (optional)
     */
    void registerShortcut(const QString& id,
                         const QString& displayName,
                         const QKeySequence& defaultKey,
                         Context context = Global,
                         const QString& description = QString(),
                         QAction* action = nullptr);
    
    /**
     * \brief Get current key sequence for a shortcut
     */
    QKeySequence keySequence(const QString& id) const;
    
    /**
     * \brief Set custom key sequence
     * \return true if successful, false if conflict exists
     */
    bool setKeySequence(const QString& id, const QKeySequence& key);
    
    /**
     * \brief Reset shortcut to default
     */
    void resetToDefault(const QString& id);
    
    /**
     * \brief Reset all shortcuts to defaults
     */
    void resetAllToDefaults();
    
    /**
     * \brief Check if key sequence conflicts with existing shortcuts
     * \param key Key sequence to check
     * \param context Context to check in
     * \param excludeId Exclude this shortcut from conflict check
     * \return ID of conflicting shortcut, or empty if no conflict
     */
    QString findConflict(const QKeySequence& key, Context context, const QString& excludeId = QString()) const;
    
    /**
     * \brief Get all registered shortcuts
     */
    QList<ShortcutInfo> allShortcuts() const;
    
    /**
     * \brief Get shortcuts for specific context
     */
    QList<ShortcutInfo> shortcutsForContext(Context context) const;
    
    /**
     * \brief Export shortcuts to JSON
     */
    QJsonObject exportKeybindings() const;
    
    /**
     * \brief Import shortcuts from JSON
     * \return Number of shortcuts imported
     */
    int importKeybindings(const QJsonObject& json);
    
    /**
     * \brief Save custom shortcuts to file
     */
    bool saveKeybindings();
    
    /**
     * \brief Load custom shortcuts from file
     */
    bool loadKeybindings();
    
signals:
    void shortcutChanged(const QString& id, const QKeySequence& newKey);
    void shortcutsReset();
    
private:
    ShortcutManager();
    ~ShortcutManager();
    ShortcutManager(const ShortcutManager&) = delete;
    ShortcutManager& operator=(const ShortcutManager&) = delete;
    
    void registerDefaultShortcuts();
    QString getKeybindingsPath() const;
    
    QHash<QString, ShortcutInfo> m_shortcuts;
};

} // namespace RawrXD

#endif // RAWRXD_SHORTCUT_MANAGER_H
