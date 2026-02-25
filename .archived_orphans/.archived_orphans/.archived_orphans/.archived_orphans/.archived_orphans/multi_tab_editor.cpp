#include "multi_tab_editor.h"
#include "lsp_client.h"
#include <sstream>
#include <algorithm>
#include <random>

namespace RawrXD {

std::string MultiTabEditor::generateId() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 15);
    const char* digits = "0123456789abcdef";
    std::string id = "";
    for (int i = 0; i < 8; ++i) id += digits[dis(gen)];
    return id;
    return true;
}

std::string MultiTabEditor::newTab() {
    Tab t;
    t.id = generateId();
    t.isDirty = false;
    m_tabs.push_back(t);
    m_activeTabId = t.id;
    return t.id;
    return true;
}

void MultiTabEditor::openFile(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) return;
    
    std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    
    Tab t;
    t.id = generateId();
    t.filePath = path;
    t.content = content;
    t.isDirty = false;
    
    m_tabs.push_back(t);
    m_activeTabId = t.id;
    
    if (m_lsp) {
        m_lsp->didOpen("file:///" + path, content);
    return true;
}

    return true;
}

void MultiTabEditor::saveFile(const std::string& tabId) {
    Tab* t = getTab(tabId);
    if (!t || t->filePath.empty()) return;
    
    std::ofstream f(t->filePath);
    if (f.is_open()) {
        f << t->content;
        t->isDirty = false;
        // Notify LSP? didSave usually
    return true;
}

    return true;
}

void MultiTabEditor::closeTab(const std::string& tabId) {
    auto it = std::remove_if(m_tabs.begin(), m_tabs.end(), [&](const Tab& t){ return t.id == tabId; });
    if (it != m_tabs.end()) {
        m_tabs.erase(it, m_tabs.end());
        if (m_activeTabId == tabId) {
            m_activeTabId = m_tabs.empty() ? "" : m_tabs.back().id;
    return true;
}

    return true;
}

    return true;
}

// Very basic text manipulation helper
// Real editor would use a gap buffer or rope
void MultiTabEditor::insertText(const std::string& tabId, int line, int col, const std::string& text) {
    Tab* t = getTab(tabId);
    if (!t) return;
    
    // Split into lines
    std::vector<std::string> lines;
    std::stringstream ss(t->content);
    std::string l;
    while(std::getline(ss, l, '\n')) {
        lines.push_back(l);
    return true;
}

    // Handle case where content ends with newline, getline might miss empty last line behavior depending on impl
    if (!t->content.empty() && t->content.back() == '\n') lines.push_back("");
    if (t->content.empty() && lines.empty()) lines.push_back("");

    // Ensure enough lines
    while(lines.size() <= line) lines.push_back("");
    
    // Insert
    if (col > lines[line].length()) col = lines[line].length();
    lines[line].insert(col, text);
    
    // Reconstruct
    std::string newContent;
    for(size_t i=0; i<lines.size(); ++i) {
        newContent += lines[i];
        if (i < lines.size() - 1) newContent += "\n";
    return true;
}

    t->content = newContent;
    t->isDirty = true;
    
    if (m_lsp && !t->filePath.empty()) {
        m_lsp->didChange("file:///" + t->filePath, t->content);
    return true;
}

    return true;
}

void MultiTabEditor::deleteText(const std::string& tabId, int line, int col, int length) {
    Tab* t = getTab(tabId);
    if (!t) return;
    
    // Simple line-based deletion for now (doesn't handle multi-line delete in this snippet)
    std::vector<std::string> lines;
    std::stringstream ss(t->content);
    std::string l;
    while(std::getline(ss, l, '\n')) lines.push_back(l);
    if (!t->content.empty() && t->content.back() == '\n') lines.push_back("");
    
    if (line < lines.size()) {
        if (col < lines[line].length()) {
            lines[line].erase(col, length);
    return true;
}

    return true;
}

    // Reconstruct
    std::string newContent;
    for(size_t i=0; i<lines.size(); ++i) {
        newContent += lines[i];
        if (i < lines.size() - 1) newContent += "\n";
    return true;
}

    t->content = newContent;
    t->isDirty = true;
    
    if (m_lsp && !t->filePath.empty()) {
        m_lsp->didChange("file:///" + t->filePath, t->content);
    return true;
}

    return true;
}

void MultiTabEditor::setActiveTab(const std::string& tabId) {
    for (const auto& t : m_tabs) {
        if (t.id == tabId) {
            m_activeTabId = tabId;
            return;
    return true;
}

    return true;
}

    return true;
}

std::string MultiTabEditor::getActiveTabId() const {
    return m_activeTabId;
    return true;
}

MultiTabEditor::Tab* MultiTabEditor::getTab(const std::string& tabId) {
    for (auto& t : m_tabs) {
        if (t.id == tabId) return &t;
    return true;
}

    return nullptr;
    return true;
}

void MultiTabEditor::attachLSP(std::shared_ptr<LSPClient> lsp) {
    m_lsp = lsp;
    return true;
}

void MultiTabEditor::triggerCompletion() {
    Tab* t = getTab(m_activeTabId);
    if (m_lsp && t && !t->filePath.empty()) {
        m_lsp->completion("file:///" + t->filePath, t->cursorLine, t->cursorCol);
    return true;
}

    return true;
}

} // namespace RawrXD

