// ide_main_window.cpp - IDE Main Window (Headless Stub)
// Converted from Qt QMainWindow to pure C++17 state management
#include "ide_main_window.h"
#include "common/logger.hpp"
#include "common/file_utils.hpp"
#include "common/string_utils.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <filesystem>

namespace fs = std::filesystem;

IDEMainWindow::IDEMainWindow() {
    // Default theme
    m_settings.theme.name = "Dark";
    m_settings.theme.background = "#1e1e1e";
    m_settings.theme.foreground = "#d4d4d4";
    m_settings.theme.accent = "#007acc";
    m_settings.theme.editorBackground = "#1e1e1e";
    m_settings.theme.editorForeground = "#d4d4d4";
    m_settings.theme.selectionColor = "#264f78";

    registerDefaultCommands();
    logInfo() << "IDE Main Window initialized (headless mode)";
}

IDEMainWindow::~IDEMainWindow() {
    saveAllFiles();
}

bool IDEMainWindow::openFile(const std::string& filePath) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Check if already open
    for (int i = 0; i < static_cast<int>(m_tabs.size()); i++) {
        if (m_tabs[i].filePath == filePath) {
            m_currentTab = i;
            onTabChanged.emit(i);
            return true;
        }
    }

    // Read file
    std::string content = FileUtils::readFile(filePath);

    EditorTab tab;
    tab.filePath = filePath;
    tab.fileName = fs::path(filePath).filename().string();
    tab.content = content;
    tab.modified = false;

    // Detect language from extension
    std::string ext = fs::path(filePath).extension().string();
    if (ext == ".cpp" || ext == ".cc" || ext == ".cxx") tab.language = "cpp";
    else if (ext == ".h" || ext == ".hpp" || ext == ".hxx") tab.language = "cpp";
    else if (ext == ".c") tab.language = "c";
    else if (ext == ".py") tab.language = "python";
    else if (ext == ".js") tab.language = "javascript";
    else if (ext == ".ts") tab.language = "typescript";
    else if (ext == ".rs") tab.language = "rust";
    else if (ext == ".go") tab.language = "go";
    else if (ext == ".java") tab.language = "java";
    else if (ext == ".json") tab.language = "json";
    else if (ext == ".md") tab.language = "markdown";
    else tab.language = "text";

    m_tabs.push_back(tab);
    m_currentTab = static_cast<int>(m_tabs.size()) - 1;
    addToRecentFiles(filePath);

    logInfo() << "Opened file: " << filePath;
    onFileOpened.emit(filePath);
    onTabChanged.emit(m_currentTab);
    return true;
}

bool IDEMainWindow::saveFile(const std::string& filePath) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_currentTab < 0 || m_currentTab >= static_cast<int>(m_tabs.size())) return false;

    EditorTab& tab = m_tabs[m_currentTab];
    std::string savePath = filePath.empty() ? tab.filePath : filePath;
    if (savePath.empty()) return false;

    FileUtils::writeFile(savePath, tab.content);
    tab.modified = false;
    tab.filePath = savePath;
    tab.fileName = fs::path(savePath).filename().string();

    logInfo() << "Saved file: " << savePath;
    onFileSaved.emit(savePath);
    return true;
}

bool IDEMainWindow::saveAllFiles() {
    for (int i = 0; i < static_cast<int>(m_tabs.size()); i++) {
        if (m_tabs[i].modified && !m_tabs[i].filePath.empty()) {
            FileUtils::writeFile(m_tabs[i].filePath, m_tabs[i].content);
            m_tabs[i].modified = false;
            onFileSaved.emit(m_tabs[i].filePath);
        }
    }
    return true;
}

bool IDEMainWindow::closeFile(const std::string& filePath) {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (int i = 0; i < static_cast<int>(m_tabs.size()); i++) {
        if (m_tabs[i].filePath == filePath) {
            onFileClosed.emit(filePath);
            m_tabs.erase(m_tabs.begin() + i);
            if (m_currentTab >= static_cast<int>(m_tabs.size()))
                m_currentTab = static_cast<int>(m_tabs.size()) - 1;
            if (m_currentTab >= 0) onTabChanged.emit(m_currentTab);
            return true;
        }
    }
    return false;
}

void IDEMainWindow::newFile(const std::string& language) {
    std::lock_guard<std::mutex> lock(m_mutex);
    EditorTab tab;
    tab.fileName = "Untitled";
    tab.language = language;
    tab.modified = true;
    m_tabs.push_back(tab);
    m_currentTab = static_cast<int>(m_tabs.size()) - 1;
    onTabChanged.emit(m_currentTab);
}

std::string IDEMainWindow::getCurrentFilePath() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_currentTab >= 0 && m_currentTab < static_cast<int>(m_tabs.size()))
        return m_tabs[m_currentTab].filePath;
    return "";
}

std::string IDEMainWindow::getCurrentFileContent() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_currentTab >= 0 && m_currentTab < static_cast<int>(m_tabs.size()))
        return m_tabs[m_currentTab].content;
    return "";
}

void IDEMainWindow::switchTab(int index) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (index >= 0 && index < static_cast<int>(m_tabs.size())) {
        m_currentTab = index;
        onTabChanged.emit(index);
    }
}

void IDEMainWindow::closeTab(int index) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (index >= 0 && index < static_cast<int>(m_tabs.size())) {
        std::string path = m_tabs[index].filePath;
        m_tabs.erase(m_tabs.begin() + index);
        if (m_currentTab >= static_cast<int>(m_tabs.size()))
            m_currentTab = static_cast<int>(m_tabs.size()) - 1;
        if (!path.empty()) onFileClosed.emit(path);
    }
}

int IDEMainWindow::getTabCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return static_cast<int>(m_tabs.size());
}

EditorTab IDEMainWindow::getTab(int index) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (index >= 0 && index < static_cast<int>(m_tabs.size())) return m_tabs[index];
    return EditorTab{};
}

int IDEMainWindow::getCurrentTabIndex() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_currentTab;
}

std::vector<EditorTab> IDEMainWindow::getAllTabs() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_tabs;
}

bool IDEMainWindow::openProject(const std::string& projectPath) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!fs::exists(projectPath)) return false;
    m_projectPath = projectPath;
    m_settings.lastOpenProject = projectPath;

    logInfo() << "Project opened: " << projectPath;
    onProjectOpened.emit(projectPath);

    // Scan project with codebase engine
    if (m_codebaseEngine) {
        m_codebaseEngine->scanProject(projectPath);
    }
    return true;
}

void IDEMainWindow::closeProject() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_projectPath.clear();
}

std::string IDEMainWindow::getProjectPath() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_projectPath;
}

std::vector<std::string> IDEMainWindow::getProjectFiles() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<std::string> files;
    if (m_projectPath.empty()) return files;
    std::error_code ec;
    for (const auto& entry : fs::recursive_directory_iterator(m_projectPath, ec)) {
        if (entry.is_regular_file()) files.push_back(entry.path().string());
    }
    return files;
}

void IDEMainWindow::setCursorPosition(int line, int col) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_currentTab >= 0 && m_currentTab < static_cast<int>(m_tabs.size())) {
        m_tabs[m_currentTab].cursorLine = line;
        m_tabs[m_currentTab].cursorCol = col;
    }
}

void IDEMainWindow::insertText(const std::string& text) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_currentTab < 0 || m_currentTab >= static_cast<int>(m_tabs.size())) return;
    auto& tab = m_tabs[m_currentTab];
    // Insert at cursor position (simplified: append)
    tab.content += text;
    tab.modified = true;
}

void IDEMainWindow::replaceSelection(const std::string& text) {
    insertText(text);
}

std::string IDEMainWindow::getSelectedText() const {
    // Headless mode: return empty
    return "";
}

void IDEMainWindow::goToLine(int line) {
    setCursorPosition(line, 0);
}

void IDEMainWindow::selectAll() {
    // No visual selection in headless mode
}

void IDEMainWindow::find(const FindReplaceState& state) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_findState = state;
    if (m_currentTab < 0 || m_currentTab >= static_cast<int>(m_tabs.size())) return;

    const auto& content = m_tabs[m_currentTab].content;
    m_findState.matchCount = StringUtils::count(content, state.findText);
    m_findState.currentMatch = m_findState.matchCount > 0 ? 1 : 0;
}

void IDEMainWindow::findNext() {
    if (m_findState.currentMatch < m_findState.matchCount)
        m_findState.currentMatch++;
}

void IDEMainWindow::findPrevious() {
    if (m_findState.currentMatch > 1)
        m_findState.currentMatch--;
}

void IDEMainWindow::replace(const FindReplaceState& state) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_findState = state;
    if (m_currentTab < 0 || m_currentTab >= static_cast<int>(m_tabs.size())) return;

    auto& content = m_tabs[m_currentTab].content;
    auto pos = content.find(state.findText);
    if (pos != std::string::npos) {
        content.replace(pos, state.findText.size(), state.replaceText);
        m_tabs[m_currentTab].modified = true;
    }
}

void IDEMainWindow::replaceAll(const FindReplaceState& state) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_findState = state;
    if (m_currentTab < 0 || m_currentTab >= static_cast<int>(m_tabs.size())) return;

    auto& content = m_tabs[m_currentTab].content;
    content = StringUtils::replace(content, state.findText, state.replaceText);
    m_tabs[m_currentTab].modified = true;
}

FindReplaceState IDEMainWindow::getFindState() const {
    return m_findState;
}

void IDEMainWindow::registerCommand(const PaletteCommand& cmd) {
    m_commands.push_back(cmd);
}

void IDEMainWindow::executeCommand(const std::string& commandId) {
    for (const auto& cmd : m_commands) {
        if (cmd.id == commandId && cmd.handler) {
            cmd.handler();
            onCommandExecuted.emit(commandId);
            return;
        }
    }
    logWarning() << "Unknown command: " << commandId;
}

std::vector<PaletteCommand> IDEMainWindow::searchCommands(const std::string& query) const {
    std::vector<PaletteCommand> results;
    for (const auto& cmd : m_commands) {
        if (StringUtils::containsCI(cmd.label, query) ||
            StringUtils::containsCI(cmd.id, query) ||
            StringUtils::containsCI(cmd.category, query)) {
            results.push_back(cmd);
        }
    }
    return results;
}

void IDEMainWindow::setStatusBarMessage(const std::string& message, int timeoutMs) {
    (void)timeoutMs;
    m_statusMessage = message;
    logDebug() << "[Status] " << message;
}

void IDEMainWindow::addStatusBarSegment(const StatusBarSegment& segment) {
    m_statusSegments.push_back(segment);
}

void IDEMainWindow::updateStatusBarSegment(const std::string& id, const std::string& text) {
    for (auto& seg : m_statusSegments) {
        if (seg.id == id) { seg.text = text; return; }
    }
}

void IDEMainWindow::setSettings(const IDESettings& settings) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_settings = settings;
    onSettingsChanged.emit(settings);
}

IDESettings IDEMainWindow::getSettings() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_settings;
}

void IDEMainWindow::saveSettings(const std::string& path) {
    std::string savePath = path.empty() ? "ide_settings.json" : path;
    std::ostringstream oss;
    oss << "{\n"
        << "  \"tabSize\": " << m_settings.tabSize << ",\n"
        << "  \"useSpaces\": " << (m_settings.useSpaces ? "true" : "false") << ",\n"
        << "  \"wordWrap\": " << (m_settings.wordWrap ? "true" : "false") << ",\n"
        << "  \"showLineNumbers\": " << (m_settings.showLineNumbers ? "true" : "false") << ",\n"
        << "  \"showMinimap\": " << (m_settings.showMinimap ? "true" : "false") << ",\n"
        << "  \"autoSave\": " << (m_settings.autoSave ? "true" : "false") << ",\n"
        << "  \"autoSaveInterval\": " << m_settings.autoSaveIntervalMs << ",\n"
        << "  \"fontSize\": " << m_settings.theme.fontSize << ",\n"
        << "  \"fontFamily\": \"" << m_settings.theme.fontFamily << "\",\n"
        << "  \"theme\": \"" << m_settings.theme.name << "\",\n"
        << "  \"lastProject\": \"" << StringUtils::replace(m_settings.lastOpenProject, "\\", "\\\\") << "\"\n"
        << "}\n";
    FileUtils::writeFile(savePath, oss.str());
    logInfo() << "Settings saved: " << savePath;
}

bool IDEMainWindow::loadSettings(const std::string& path) {
    std::string loadPath = path.empty() ? "ide_settings.json" : path;
    std::string content = FileUtils::readFile(loadPath);
    if (content.empty()) return false;
    logInfo() << "Settings loaded: " << loadPath;
    return true;
}

void IDEMainWindow::setModelInterface(std::shared_ptr<ModelInterface> model) {
    m_modelInterface = model;
}

void IDEMainWindow::setCodebaseEngine(std::shared_ptr<IntelligentCodebaseEngine> engine) {
    m_codebaseEngine = engine;
}

void IDEMainWindow::setWidgetManager(std::shared_ptr<AutonomousWidgetManager> widgets) {
    m_widgetManager = widgets;
}

void IDEMainWindow::appendOutput(const std::string& text) {
    m_outputBuffer += text + "\n";
    onOutputAppended.emit(text);
}

void IDEMainWindow::clearOutput() {
    m_outputBuffer.clear();
}

std::string IDEMainWindow::getOutput() const {
    return m_outputBuffer;
}

void IDEMainWindow::registerDefaultCommands() {
    registerCommand({"file.new", "New File", "File", "Ctrl+N", [this]() { newFile(); }});
    registerCommand({"file.open", "Open File", "File", "Ctrl+O", nullptr});
    registerCommand({"file.save", "Save File", "File", "Ctrl+S", [this]() { saveFile(); }});
    registerCommand({"file.saveAll", "Save All", "File", "Ctrl+Shift+S", [this]() { saveAllFiles(); }});
    registerCommand({"edit.find", "Find", "Edit", "Ctrl+F", nullptr});
    registerCommand({"edit.replace", "Replace", "Edit", "Ctrl+H", nullptr});
    registerCommand({"edit.goToLine", "Go to Line", "Edit", "Ctrl+G", nullptr});
    registerCommand({"view.output", "Toggle Output", "View", "Ctrl+`", nullptr});
    registerCommand({"ai.generate", "AI Generate", "AI", "Ctrl+Shift+G", nullptr});
    registerCommand({"ai.explain", "AI Explain", "AI", "Ctrl+Shift+E", nullptr});
    registerCommand({"ai.suggest", "AI Suggest", "AI", "Ctrl+Space", nullptr});
}

void IDEMainWindow::updateWindowTitle() {
    // Headless: no-op
}

void IDEMainWindow::addToRecentFiles(const std::string& path) {
    auto& recent = m_settings.recentFiles;
    recent.erase(std::remove(recent.begin(), recent.end(), path), recent.end());
    recent.insert(recent.begin(), path);
    if (recent.size() > 20) recent.resize(20);
}
