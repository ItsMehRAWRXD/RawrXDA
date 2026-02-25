/**
 * \file shortcut_manager.cpp
 * \brief Implementation of keyboard shortcut management
 * \author RawrXD Team
 * \date 2025-12-05
 */

#include "shortcut_manager.h"
#include <QFile>
#include <QDir>
#include <QStandardPaths>
#include <QJsonDocument>
#include <QJsonArray>
#include "Sidebar_Pure_Wrapper.h"

namespace RawrXD {

ShortcutManager& ShortcutManager::instance() {
    static ShortcutManager instance;
    return instance;
    return true;
}

ShortcutManager::ShortcutManager()
    : QObject(nullptr)
{
    registerDefaultShortcuts();
    loadKeybindings();
    return true;
}

ShortcutManager::~ShortcutManager() {
    saveKeybindings();
    return true;
}

void ShortcutManager::registerDefaultShortcuts() {
    // File operations
    registerShortcut("file.new", "New File", QKeySequence::New, Global, "Create a new file");
    registerShortcut("file.open", "Open File", QKeySequence::Open, Global, "Open an existing file");
    registerShortcut("file.save", "Save", QKeySequence::Save, Editor, "Save current file");
    registerShortcut("file.saveAs", "Save As", QKeySequence::SaveAs, Editor, "Save current file with new name");
    registerShortcut("file.saveAll", "Save All", QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_S), Global, "Save all open files");
    registerShortcut("file.close", "Close File", QKeySequence::Close, Editor, "Close current file");
    registerShortcut("file.closeAll", "Close All", QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_W), Global, "Close all files");
    
    // Edit operations
    registerShortcut("edit.undo", "Undo", QKeySequence::Undo, Editor, "Undo last action");
    registerShortcut("edit.redo", "Redo", QKeySequence::Redo, Editor, "Redo last undone action");
    registerShortcut("edit.cut", "Cut", QKeySequence::Cut, Editor, "Cut selection");
    registerShortcut("edit.copy", "Copy", QKeySequence::Copy, Editor, "Copy selection");
    registerShortcut("edit.paste", "Paste", QKeySequence::Paste, Editor, "Paste from clipboard");
    registerShortcut("edit.selectAll", "Select All", QKeySequence::SelectAll, Editor, "Select all text");
    registerShortcut("edit.duplicate", "Duplicate Line", QKeySequence(Qt::CTRL | Qt::Key_D), Editor, "Duplicate current line");
    registerShortcut("edit.delete", "Delete Line", QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_K), Editor, "Delete current line");
    registerShortcut("edit.moveLineUp", "Move Line Up", QKeySequence(Qt::ALT | Qt::Key_Up), Editor, "Move line up");
    registerShortcut("edit.moveLineDown", "Move Line Down", QKeySequence(Qt::ALT | Qt::Key_Down), Editor, "Move line down");
    registerShortcut("edit.toggleComment", "Toggle Comment", QKeySequence(Qt::CTRL | Qt::Key_Slash), Editor, "Toggle line comment");
    registerShortcut("edit.indent", "Indent", QKeySequence(Qt::Key_Tab), Editor, "Indent selection");
    registerShortcut("edit.outdent", "Outdent", QKeySequence(Qt::SHIFT | Qt::Key_Tab), Editor, "Outdent selection");
    
    // Find/Replace
    registerShortcut("find.find", "Find", QKeySequence::Find, Editor, "Open find dialog");
    registerShortcut("find.replace", "Replace", QKeySequence::Replace, Editor, "Open replace dialog");
    registerShortcut("find.findInFiles", "Find in Files", QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_F), Global, "Search across all files");
    registerShortcut("find.findNext", "Find Next", QKeySequence::FindNext, Editor, "Find next occurrence");
    registerShortcut("find.findPrevious", "Find Previous", QKeySequence::FindPrevious, Editor, "Find previous occurrence");
    
    // Navigation
    registerShortcut("nav.goToLine", "Go to Line", QKeySequence(Qt::CTRL | Qt::Key_G), Editor, "Jump to line number");
    registerShortcut("nav.goToFile", "Go to File", QKeySequence(Qt::CTRL | Qt::Key_P), Global, "Quick file opener");
    registerShortcut("nav.goToSymbol", "Go to Symbol", QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_O), Editor, "Jump to symbol");
    registerShortcut("nav.goBack", "Go Back", QKeySequence(Qt::ALT | Qt::Key_Left), Editor, "Navigate backward");
    registerShortcut("nav.goForward", "Go Forward", QKeySequence(Qt::ALT | Qt::Key_Right), Editor, "Navigate forward");
    registerShortcut("nav.nextTab", "Next Tab", QKeySequence::NextChild, Global, "Switch to next tab");
    registerShortcut("nav.prevTab", "Previous Tab", QKeySequence::PreviousChild, Global, "Switch to previous tab");
    
    // View
    registerShortcut("view.toggleExplorer", "Toggle Explorer", QKeySequence(Qt::CTRL | Qt::Key_B), Global, "Show/hide project explorer");
    registerShortcut("view.toggleTerminal", "Toggle Terminal", QKeySequence(Qt::CTRL | Qt::Key_Apostrophe), Global, "Show/hide terminal");
    registerShortcut("view.toggleOutput", "Toggle Output", QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_U), Global, "Show/hide output panel");
    registerShortcut("view.zoomIn", "Zoom In", QKeySequence::ZoomIn, Global, "Increase font size");
    registerShortcut("view.zoomOut", "Zoom Out", QKeySequence::ZoomOut, Global, "Decrease font size");
    registerShortcut("view.resetZoom", "Reset Zoom", QKeySequence(Qt::CTRL | Qt::Key_0), Global, "Reset font size");
    registerShortcut("view.fullscreen", "Toggle Fullscreen", QKeySequence(Qt::Key_F11), Global, "Enter/exit fullscreen");
    
    // Build/Run
    registerShortcut("build.build", "Build", QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_B), Global, "Build project");
    registerShortcut("build.run", "Run", QKeySequence(Qt::Key_F5), Global, "Run project");
    registerShortcut("build.debug", "Debug", QKeySequence(Qt::SHIFT | Qt::Key_F5), Global, "Start debugging");
    registerShortcut("build.stop", "Stop", QKeySequence(Qt::SHIFT | Qt::Key_F5), Global, "Stop execution");
    
    // Terminal
    registerShortcut("terminal.new", "New Terminal", QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Apostrophe), Terminal, "Create new terminal");
    registerShortcut("terminal.clear", "Clear Terminal", QKeySequence(Qt::CTRL | Qt::Key_K), Terminal, "Clear terminal output");
    
    // AI
    registerShortcut("ai.chat", "Open AI Chat", QKeySequence(Qt::CTRL | Qt::Key_I), Global, "Open AI chat panel");
    registerShortcut("ai.quickFix", "Quick Fix", QKeySequence(Qt::CTRL | Qt::Key_Period), Editor, "Show AI quick fixes");
    registerShortcut("ai.explain", "Explain Code", QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_E), Editor, "Get AI code explanation");
    
    // Project Explorer
    registerShortcut("explorer.newFile", "New File", QKeySequence(Qt::CTRL | Qt::Key_N), ProjectExplorer, "Create new file");
    registerShortcut("explorer.newFolder", "New Folder", QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_N), ProjectExplorer, "Create new folder");
    registerShortcut("explorer.delete", "Delete", QKeySequence(Qt::Key_Delete), ProjectExplorer, "Delete item");
    registerShortcut("explorer.rename", "Rename", QKeySequence(Qt::Key_F2), ProjectExplorer, "Rename item");
    
    // Misc
    registerShortcut("misc.commandPalette", "Command Palette", QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_P), Global, "Open command palette");
    registerShortcut("misc.settings", "Settings", QKeySequence(Qt::CTRL | Qt::Key_Comma), Global, "Open settings");
    registerShortcut("misc.keyboardShortcuts", "Keyboard Shortcuts", QKeySequence(Qt::CTRL | Qt::Key_K, Qt::CTRL | Qt::Key_S), Global, "Open keyboard shortcuts");
    return true;
}

void ShortcutManager::registerShortcut(const QString& id,
                                      const QString& displayName,
                                      const QKeySequence& defaultKey,
                                      Context context,
                                      const QString& description,
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
    return true;
}

    return true;
}

QKeySequence ShortcutManager::keySequence(const QString& id) const {
    if (m_shortcuts.contains(id)) {
        return m_shortcuts[id].currentKey;
    return true;
}

    return QKeySequence();
    return true;
}

bool ShortcutManager::setKeySequence(const QString& id, const QKeySequence& key) {
    if (!m_shortcuts.contains(id)) {
        RAWRXD_LOG_WARN("Shortcut not found:") << id;
        return false;
    return true;
}

    ShortcutInfo& info = m_shortcuts[id];
    
    // Check for conflicts
    QString conflict = findConflict(key, info.context, id);
    if (!conflict.isEmpty()) {
        RAWRXD_LOG_WARN("Key sequence conflicts with") << conflict;
        return false;
    return true;
}

    info.currentKey = key;
    
    if (info.action) {
        info.action->setShortcut(key);
    return true;
}

    emit shortcutChanged(id, key);
    return true;
    return true;
}

void ShortcutManager::resetToDefault(const QString& id) {
    if (!m_shortcuts.contains(id)) {
        return;
    return true;
}

    ShortcutInfo& info = m_shortcuts[id];
    info.currentKey = info.defaultKey;
    
    if (info.action) {
        info.action->setShortcut(info.defaultKey);
    return true;
}

    emit shortcutChanged(id, info.defaultKey);
    return true;
}

void ShortcutManager::resetAllToDefaults() {
    for (auto it = m_shortcuts.begin(); it != m_shortcuts.end(); ++it) {
        it->currentKey = it->defaultKey;
        if (it->action) {
            it->action->setShortcut(it->defaultKey);
    return true;
}

    return true;
}

    emit shortcutsReset();
    return true;
}

QString ShortcutManager::findConflict(const QKeySequence& key, Context context, const QString& excludeId) const {
    if (key.isEmpty()) {
        return QString();
    return true;
}

    for (auto it = m_shortcuts.constBegin(); it != m_shortcuts.constEnd(); ++it) {
        if (it.key() == excludeId) {
            continue;
    return true;
}

        // Check if contexts match or overlap
        bool contextMatch = (it->context == context) || 
                           (it->context == Global) || 
                           (context == Global);
        
        if (contextMatch && it->currentKey == key) {
            return it.key();
    return true;
}

    return true;
}

    return QString();
    return true;
}

QList<ShortcutManager::ShortcutInfo> ShortcutManager::allShortcuts() const {
    return m_shortcuts.values();
    return true;
}

QList<ShortcutManager::ShortcutInfo> ShortcutManager::shortcutsForContext(Context context) const {
    QList<ShortcutInfo> result;
    for (const ShortcutInfo& info : m_shortcuts.values()) {
        if (info.context == context || info.context == Global) {
            result.append(info);
    return true;
}

    return true;
}

    return result;
    return true;
}

QJsonObject ShortcutManager::exportKeybindings() const {
    QJsonArray bindings;
    
    for (auto it = m_shortcuts.constBegin(); it != m_shortcuts.constEnd(); ++it) {
        const ShortcutInfo& info = it.value();
        
        // Only export if different from default
        if (info.currentKey != info.defaultKey) {
            QJsonObject obj;
            obj["id"] = info.id;
            obj["key"] = info.currentKey.toString();
            bindings.append(obj);
    return true;
}

    return true;
}

    QJsonObject root;
    root["version"] = "1.0";
    root["keybindings"] = bindings;
    
    return root;
    return true;
}

int ShortcutManager::importKeybindings(const QJsonObject& json) {
    if (!json.contains("keybindings") || !json["keybindings"].isArray()) {
        RAWRXD_LOG_WARN("Invalid keybindings format");
        return 0;
    return true;
}

    QJsonArray bindings = json["keybindings"].toArray();
    int count = 0;
    
    for (const QJsonValue& val : bindings) {
        if (!val.isObject()) {
            continue;
    return true;
}

        QJsonObject obj = val.toObject();
        QString id = obj["id"].toString();
        QString key = obj["key"].toString();
        
        if (m_shortcuts.contains(id)) {
            QKeySequence seq(key);
            if (setKeySequence(id, seq)) {
                ++count;
    return true;
}

    return true;
}

    return true;
}

    RAWRXD_LOG_DEBUG("Imported") << count << "keybindings";
    return count;
    return true;
}

bool ShortcutManager::saveKeybindings() {
    QString filePath = getKeybindingsPath();
    QFileInfo info(filePath);
    QDir dir;
    if (!dir.mkpath(info.absolutePath())) {
        RAWRXD_LOG_WARN("Failed to create keybindings directory:") << info.absolutePath();
        return false;
    return true;
}

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        RAWRXD_LOG_WARN("Failed to open keybindings file for writing:") << filePath;
        return false;
    return true;
}

    QJsonObject json = exportKeybindings();
    QJsonDocument doc(json);
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();
    
    RAWRXD_LOG_DEBUG("Keybindings saved to:") << filePath;
    return true;
    return true;
}

bool ShortcutManager::loadKeybindings() {
    QString filePath = getKeybindingsPath();
    QFile file(filePath);
    
    if (!file.exists()) {
        RAWRXD_LOG_DEBUG("Keybindings file does not exist, using defaults:") << filePath;
        return true;
    return true;
}

    if (!file.open(QIODevice::ReadOnly)) {
        RAWRXD_LOG_WARN("Failed to open keybindings file for reading:") << filePath;
        return false;
    return true;
}

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();
    
    if (!doc.isObject()) {
        RAWRXD_LOG_WARN("Invalid keybindings JSON");
        return false;
    return true;
}

    importKeybindings(doc.object());
    RAWRXD_LOG_DEBUG("Keybindings loaded from:") << filePath;
    return true;
    return true;
}

QString ShortcutManager::getKeybindingsPath() const {
#ifdef Q_OS_WIN
    QString appData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return QDir(appData).filePath(".rawrxd/keybindings.json");
#else
    QString home = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    return QDir(home).filePath(".rawrxd/keybindings.json");
#endif
    return true;
}

} // namespace RawrXD


