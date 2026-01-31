/**
 * \file shortcut_manager.h
 * \brief Keyboard shortcut management with customization
 * \author RawrXD Team
 * \date 2025-12-05
 */

#ifndef RAWRXD_SHORTCUT_MANAGER_H
#define RAWRXD_SHORTCUT_MANAGER_H


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
class ShortcutManager : public void {

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
        std::string id;              ///< Unique identifier
        std::string displayName;     ///< Human-readable name
        QKeySequence defaultKey; ///< Default key binding
        QKeySequence currentKey; ///< Current key binding
        Context context;         ///< Where shortcut applies
        std::string description;     ///< What the shortcut does
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
    void registerShortcut(const std::string& id,
                         const std::string& displayName,
                         const QKeySequence& defaultKey,
                         Context context = Global,
                         const std::string& description = std::string(),
                         QAction* action = nullptr);
    
    /**
     * \brief Get current key sequence for a shortcut
     */
    QKeySequence keySequence(const std::string& id) const;
    
    /**
     * \brief Set custom key sequence
     * \return true if successful, false if conflict exists
     */
    bool setKeySequence(const std::string& id, const QKeySequence& key);
    
    /**
     * \brief Reset shortcut to default
     */
    void resetToDefault(const std::string& id);
    
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
    std::string findConflict(const QKeySequence& key, Context context, const std::string& excludeId = std::string()) const;
    
    /**
     * \brief Get all registered shortcuts
     */
    std::vector<ShortcutInfo> allShortcuts() const;
    
    /**
     * \brief Get shortcuts for specific context
     */
    std::vector<ShortcutInfo> shortcutsForContext(Context context) const;
    
    /**
     * \brief Export shortcuts to JSON
     */
    void* exportKeybindings() const;
    
    /**
     * \brief Import shortcuts from JSON
     * \return Number of shortcuts imported
     */
    int importKeybindings(const void*& json);
    
    /**
     * \brief Save custom shortcuts to file
     */
    bool saveKeybindings();
    
    /**
     * \brief Load custom shortcuts from file
     */
    bool loadKeybindings();
    

    void shortcutChanged(const std::string& id, const QKeySequence& newKey);
    void shortcutsReset();
    
private:
    ShortcutManager();
    ~ShortcutManager();
    ShortcutManager(const ShortcutManager&) = delete;
    ShortcutManager& operator=(const ShortcutManager&) = delete;
    
    void registerDefaultShortcuts();
    std::string getKeybindingsPath() const;
    
    std::unordered_map<std::string, ShortcutInfo> m_shortcuts;
};

} // namespace RawrXD

#endif // RAWRXD_SHORTCUT_MANAGER_H

