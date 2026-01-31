/**
 * \file shortcut_manager.cpp
 * \brief Implementation of keyboard shortcut management
 * \author RawrXD Team
 * \date 2025-12-05
 */

#include "shortcut_manager.h"


namespace RawrXD {

ShortcutManager& ShortcutManager::instance() {
    static ShortcutManager instance;
    return instance;
}

ShortcutManager::ShortcutManager()
    : void(nullptr)
{
    registerDefaultShortcuts();
    loadKeybindings();
}

ShortcutManager::~ShortcutManager() {
    saveKeybindings();
}

void ShortcutManager::registerDefaultShortcuts() {
    // File operations
    registerShortcut("file.new", "New File", QKeySequence::New, Global, "Create a new file");
    registerShortcut("file.open", "Open File", QKeySequence::Open, Global, "Open an existing file");
    registerShortcut("file.save", "Save", QKeySequence::Save, Editor, "Save current file");
    registerShortcut("file.saveAs", "Save As", QKeySequence::SaveAs, Editor, "Save current file with new name");
    registerShortcut("file.saveAll", "Save All", QKeySequence(//CTRL | //SHIFT | //Key_S), Global, "Save all open files");
    registerShortcut("file.close", "Close File", QKeySequence::Close, Editor, "Close current file");
    registerShortcut("file.closeAll", "Close All", QKeySequence(//CTRL | //SHIFT | //Key_W), Global, "Close all files");
    
    // Edit operations
    registerShortcut("edit.undo", "Undo", QKeySequence::Undo, Editor, "Undo last action");
    registerShortcut("edit.redo", "Redo", QKeySequence::Redo, Editor, "Redo last undone action");
    registerShortcut("edit.cut", "Cut", QKeySequence::Cut, Editor, "Cut selection");
    registerShortcut("edit.copy", "Copy", QKeySequence::Copy, Editor, "Copy selection");
    registerShortcut("edit.paste", "Paste", QKeySequence::Paste, Editor, "Paste from clipboard");
    registerShortcut("edit.selectAll", "Select All", QKeySequence::SelectAll, Editor, "Select all text");
    registerShortcut("edit.duplicate", "Duplicate Line", QKeySequence(//CTRL | //Key_D), Editor, "Duplicate current line");
    registerShortcut("edit.delete", "Delete Line", QKeySequence(//CTRL | //SHIFT | //Key_K), Editor, "Delete current line");
    registerShortcut("edit.moveLineUp", "Move Line Up", QKeySequence(//ALT | //Key_Up), Editor, "Move line up");
    registerShortcut("edit.moveLineDown", "Move Line Down", QKeySequence(//ALT | //Key_Down), Editor, "Move line down");
    registerShortcut("edit.toggleComment", "Toggle Comment", QKeySequence(//CTRL | //Key_Slash), Editor, "Toggle line comment");
    registerShortcut("edit.indent", "Indent", QKeySequence(//Key_Tab), Editor, "Indent selection");
    registerShortcut("edit.outdent", "Outdent", QKeySequence(//SHIFT | //Key_Tab), Editor, "Outdent selection");
    
    // Find/Replace
    registerShortcut("find.find", "Find", QKeySequence::Find, Editor, "Open find dialog");
    registerShortcut("find.replace", "Replace", QKeySequence::Replace, Editor, "Open replace dialog");
    registerShortcut("find.findInFiles", "Find in Files", QKeySequence(//CTRL | //SHIFT | //Key_F), Global, "Search across all files");
    registerShortcut("find.findNext", "Find Next", QKeySequence::FindNext, Editor, "Find next occurrence");
    registerShortcut("find.findPrevious", "Find Previous", QKeySequence::FindPrevious, Editor, "Find previous occurrence");
    
    // Navigation
    registerShortcut("nav.goToLine", "Go to Line", QKeySequence(//CTRL | //Key_G), Editor, "Jump to line number");
    registerShortcut("nav.goToFile", "Go to File", QKeySequence(//CTRL | //Key_P), Global, "Quick file opener");
    registerShortcut("nav.goToSymbol", "Go to Symbol", QKeySequence(//CTRL | //SHIFT | //Key_O), Editor, "Jump to symbol");
    registerShortcut("nav.goBack", "Go Back", QKeySequence(//ALT | //Key_Left), Editor, "Navigate backward");
    registerShortcut("nav.goForward", "Go Forward", QKeySequence(//ALT | //Key_Right), Editor, "Navigate forward");
    registerShortcut("nav.nextTab", "Next Tab", QKeySequence::NextChild, Global, "Switch to next tab");
    registerShortcut("nav.prevTab", "Previous Tab", QKeySequence::PreviousChild, Global, "Switch to previous tab");
    
    // View
    registerShortcut("view.toggleExplorer", "Toggle Explorer", QKeySequence(//CTRL | //Key_B), Global, "Show/hide project explorer");
    registerShortcut("view.toggleTerminal", "Toggle Terminal", QKeySequence(//CTRL | //Key_Apostrophe), Global, "Show/hide terminal");
    registerShortcut("view.toggleOutput", "Toggle Output", QKeySequence(//CTRL | //SHIFT | //Key_U), Global, "Show/hide output panel");
    registerShortcut("view.zoomIn", "Zoom In", QKeySequence::ZoomIn, Global, "Increase font size");
    registerShortcut("view.zoomOut", "Zoom Out", QKeySequence::ZoomOut, Global, "Decrease font size");
    registerShortcut("view.resetZoom", "Reset Zoom", QKeySequence(//CTRL | //Key_0), Global, "Reset font size");
    registerShortcut("view.fullscreen", "Toggle Fullscreen", QKeySequence(//Key_F11), Global, "Enter/exit fullscreen");
    
    // Build/Run
    registerShortcut("build.build", "Build", QKeySequence(//CTRL | //SHIFT | //Key_B), Global, "Build project");
    registerShortcut("build.run", "Run", QKeySequence(//Key_F5), Global, "Run project");
    registerShortcut("build.debug", "Debug", QKeySequence(//SHIFT | //Key_F5), Global, "Start debugging");
    registerShortcut("build.stop", "Stop", QKeySequence(//SHIFT | //Key_F5), Global, "Stop execution");
    
    // Terminal
    registerShortcut("terminal.new", "New Terminal", QKeySequence(//CTRL | //SHIFT | //Key_Apostrophe), Terminal, "Create new terminal");
    registerShortcut("terminal.clear", "Clear Terminal", QKeySequence(//CTRL | //Key_K), Terminal, "Clear terminal output");
    
    // AI
    registerShortcut("ai.chat", "Open AI Chat", QKeySequence(//CTRL | //Key_I), Global, "Open AI chat panel");
    registerShortcut("ai.quickFix", "Quick Fix", QKeySequence(//CTRL | //Key_Period), Editor, "Show AI quick fixes");
    registerShortcut("ai.explain", "Explain Code", QKeySequence(//CTRL | //SHIFT | //Key_E), Editor, "Get AI code explanation");
    
    // Project Explorer
    registerShortcut("explorer.newFile", "New File", QKeySequence(//CTRL | //Key_N), ProjectExplorer, "Create new file");
    registerShortcut("explorer.newFolder", "New Folder", QKeySequence(//CTRL | //SHIFT | //Key_N), ProjectExplorer, "Create new folder");
    registerShortcut("explorer.delete", "Delete", QKeySequence(//Key_Delete), ProjectExplorer, "Delete item");
    registerShortcut("explorer.rename", "Rename", QKeySequence(//Key_F2), ProjectExplorer, "Rename item");
    
    // Misc
    registerShortcut("misc.commandPalette", "Command Palette", QKeySequence(//CTRL | //SHIFT | //Key_P), Global, "Open command palette");
    registerShortcut("misc.settings", "Settings", QKeySequence(//CTRL | //Key_Comma), Global, "Open settings");
    registerShortcut("misc.keyboardShortcuts", "Keyboard Shortcuts", QKeySequence(//CTRL | //Key_K, //CTRL | //Key_S), Global, "Open keyboard shortcuts");
}

void ShortcutManager::registerShortcut(const std::string& id,
                                      const std::string& displayName,
                                      const QKeySequence& defaultKey,
                                      Context context,
                                      const std::string& description,
                                      QAction* action)
{
    ShortcutInfo info;
    info.id = id;
    info.displayName = displayName;
    info.defaultKey = defaultKey;
    info.currentKey = defaultKey;
    info.context = context;
    info.description = description;
    info.action = action;
    
    m_shortcuts[id] = info;
    
    if (action) {
        action->setShortcut(defaultKey);
    }
}

QKeySequence ShortcutManager::keySequence(const std::string& id) const {
    if (m_shortcuts.contains(id)) {
        return m_shortcuts[id].currentKey;
    }
    return QKeySequence();
}

bool ShortcutManager::setKeySequence(const std::string& id, const QKeySequence& key) {
    if (!m_shortcuts.contains(id)) {
        return false;
    }
    
    ShortcutInfo& info = m_shortcuts[id];
    
    // Check for conflicts
    std::string conflict = findConflict(key, info.context, id);
    if (!conflict.isEmpty()) {
        return false;
    }
    
    info.currentKey = key;
    
    if (info.action) {
        info.action->setShortcut(key);
    }
    
    shortcutChanged(id, key);
    return true;
}

void ShortcutManager::resetToDefault(const std::string& id) {
    if (!m_shortcuts.contains(id)) {
        return;
    }
    
    ShortcutInfo& info = m_shortcuts[id];
    info.currentKey = info.defaultKey;
    
    if (info.action) {
        info.action->setShortcut(info.defaultKey);
    }
    
    shortcutChanged(id, info.defaultKey);
}

void ShortcutManager::resetAllToDefaults() {
    for (auto it = m_shortcuts.begin(); it != m_shortcuts.end(); ++it) {
        it->currentKey = it->defaultKey;
        if (it->action) {
            it->action->setShortcut(it->defaultKey);
        }
    }
    
    shortcutsReset();
}

std::string ShortcutManager::findConflict(const QKeySequence& key, Context context, const std::string& excludeId) const {
    if (key.isEmpty()) {
        return std::string();
    }
    
    for (auto it = m_shortcuts.constBegin(); it != m_shortcuts.constEnd(); ++it) {
        if (it.key() == excludeId) {
            continue;
        }
        
        // Check if contexts match or overlap
        bool contextMatch = (it->context == context) || 
                           (it->context == Global) || 
                           (context == Global);
        
        if (contextMatch && it->currentKey == key) {
            return it.key();
        }
    }
    
    return std::string();
}

std::vector<ShortcutManager::ShortcutInfo> ShortcutManager::allShortcuts() const {
    return m_shortcuts.values();
}

std::vector<ShortcutManager::ShortcutInfo> ShortcutManager::shortcutsForContext(Context context) const {
    std::vector<ShortcutInfo> result;
    for (const ShortcutInfo& info : m_shortcuts.values()) {
        if (info.context == context || info.context == Global) {
            result.append(info);
        }
    }
    return result;
}

void* ShortcutManager::exportKeybindings() const {
    void* bindings;
    
    for (auto it = m_shortcuts.constBegin(); it != m_shortcuts.constEnd(); ++it) {
        const ShortcutInfo& info = it.value();
        
        // Only export if different from default
        if (info.currentKey != info.defaultKey) {
            void* obj;
            obj["id"] = info.id;
            obj["key"] = info.currentKey.toString();
            bindings.append(obj);
        }
    }
    
    void* root;
    root["version"] = "1.0";
    root["keybindings"] = bindings;
    
    return root;
}

int ShortcutManager::importKeybindings(const void*& json) {
    if (!json.contains("keybindings") || !json["keybindings"].isArray()) {
        return 0;
    }
    
    void* bindings = json["keybindings"].toArray();
    int count = 0;
    
    for (const void*& val : bindings) {
        if (!val.isObject()) {
            continue;
        }
        
        void* obj = val.toObject();
        std::string id = obj["id"].toString();
        std::string key = obj["key"].toString();
        
        if (m_shortcuts.contains(id)) {
            QKeySequence seq(key);
            if (setKeySequence(id, seq)) {
                ++count;
            }
        }
    }
    
    return count;
}

bool ShortcutManager::saveKeybindings() {
    std::string filePath = getKeybindingsPath();
    std::filesystem::path info(filePath);
    std::filesystem::path dir;
    if (!dir.mkpath(info.absolutePath())) {
        return false;
    }
    
    std::fstream file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    
    void* json = exportKeybindings();
    void* doc(json);
    file.write(doc.toJson(void*::Indented));
    file.close();
    
    return true;
}

bool ShortcutManager::loadKeybindings() {
    std::string filePath = getKeybindingsPath();
    std::fstream file(filePath);
    
    if (!file.exists()) {
        return true;
    }
    
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }
    
    void* doc = void*::fromJson(file.readAll());
    file.close();
    
    if (!doc.isObject()) {
        return false;
    }
    
    importKeybindings(doc.object());
    return true;
}

std::string ShortcutManager::getKeybindingsPath() const {
#ifdef 
    std::string appData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return std::filesystem::path(appData).filePath(".rawrxd/keybindings.json");
#else
    std::string home = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    return std::filesystem::path(home).filePath(".rawrxd/keybindings.json");
#endif
}

} // namespace RawrXD

