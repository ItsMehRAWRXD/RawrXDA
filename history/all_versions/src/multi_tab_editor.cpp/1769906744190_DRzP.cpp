#include "multi_tab_editor.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <ctime>

// =========================================================================================
// Editor Tab Implementation (Buffer Logic)
// =========================================================================================

EditorTab::EditorTab() {
    m_lines.push_back(""); // Start with one empty line
    m_cursor = {0, 0};
}

EditorTab::~EditorTab() {
    m_lines.clear();
    m_undoStack.clear();
    m_redoStack.clear();
}

void EditorTab::loadFromFile(const std::string& path) {
    std::ifstream file(path, std::ios::in | std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "[EditorTab] Failed to open file: " << path << "\n";
        return;
    }

    m_lines.clear();
    std::string line;
    while (std::getline(file, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        m_lines.push_back(line);
    }
    
    if (m_lines.empty()) m_lines.push_back("");

    m_filePath = path;
    m_isModified = false;
    m_undoStack.clear();
    m_redoStack.clear();
    m_cursor = {0, 0};
}

void EditorTab::saveToFile(const std::string& path) {
    std::string targetPath = path.empty() ? m_filePath : path;
    if (targetPath.empty()) {
        std::cerr << "[EditorTab] Cannot save - no filename.\n";
        return;
    }

    std::ofstream file(targetPath, std::ios::out | std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "[EditorTab] Failed to write file: " << targetPath << "\n";
        return;
    }

    for (size_t i = 0; i < m_lines.size(); ++i) {
        file << m_lines[i];
        if (i < m_lines.size() - 1) { 
             file << "\n";
        }
    }

    m_filePath = targetPath;
    m_isModified = false;
    std::cout << "[EditorTab] Saved " << m_lines.size() << " lines to " << targetPath << "\n";
}

void EditorTab::insertText(const std::string& text, bool recordUndo) {
    if (text.empty()) return;

    if (recordUndo) {
        EditAction action;
        action.type = EditActionType::Insert;
        action.position = m_cursor;
        action.text = text;
        action.timestamp = std::time(nullptr);
        pushUndo(action);
    }

    std::string remaining = text;
    size_t pos = 0;
    while (true) {
        size_t newline = remaining.find('\n', pos);
        if (newline == std::string::npos) {
            if (m_cursor.line < m_lines.size()) {
                m_lines[m_cursor.line].insert(m_cursor.column, remaining.substr(pos));
                m_cursor.column += remaining.length() - pos;
            }
            break;
        } else {
            std::string segment = remaining.substr(pos, newline - pos);
            if (m_cursor.line < m_lines.size()) {
                std::string& line = m_lines[m_cursor.line];
                line.insert(m_cursor.column, segment);
                
                std::string suffix = line.substr(m_cursor.column + segment.length());
                line.erase(m_cursor.column + segment.length());
                
                m_lines.insert(m_lines.begin() + m_cursor.line + 1, suffix);
                
                m_cursor.line++;
                m_cursor.column = 0;
                pos = newline + 1;
            } else {
                break;
            }
        }
    }
    m_isModified = true;
}

void EditorTab::deleteBack(bool recordUndo) {
    if (m_cursor.column > 0) {
        std::string& currentLine = m_lines[m_cursor.line];
        
        if (recordUndo) {
            EditAction action;
            action.type = EditActionType::Delete;
            action.position = {m_cursor.line, m_cursor.column - 1};
            action.text = currentLine.substr(m_cursor.column - 1, 1);
            action.timestamp = std::time(nullptr);
            pushUndo(action);
        }

        currentLine.erase(m_cursor.column - 1, 1);
        m_cursor.column--;
        m_isModified = true;
    } else if (m_cursor.line > 0) {
        std::string currentLineContent = m_lines[m_cursor.line];
        m_lines.erase(m_lines.begin() + m_cursor.line);
        m_cursor.line--;
        
        size_t oldLen = m_lines[m_cursor.line].length();

        if (recordUndo) {
            EditAction action;
            action.type = EditActionType::Delete; 
            action.position = {m_cursor.line, oldLen};
            action.text = "\n"; 
            pushUndo(action);
        }

        m_lines[m_cursor.line] += currentLineContent;
        m_cursor.column = oldLen;
        m_isModified = true;
    }
}

// void EditorTab::deleteForward() - Removed (handled by default arg in header)

void EditorTab::deleteForward(bool recordUndo) {
    if (m_cursor.line >= m_lines.size()) return;
    
    // Check if end of line
    if (m_cursor.column >= m_lines[m_cursor.line].length()) {
        if (m_cursor.line < m_lines.size() - 1) {
             // Join with next line
            if (recordUndo) {
                EditAction action;
                action.type = EditActionType::Delete;
                action.position = m_cursor; 
                action.text = "\n";
                action.timestamp = std::time(nullptr);
                pushUndo(action);
            }
             
             std::string nextLine = m_lines[m_cursor.line + 1];
             m_lines[m_cursor.line] += nextLine;
             m_lines.erase(m_lines.begin() + m_cursor.line + 1);
             m_isModified = true;
        }
    } else {
        // Delete char
        if (recordUndo) {
            EditAction action;
            action.type = EditActionType::Delete;
            action.position = m_cursor;
            action.text = m_lines[m_cursor.line].substr(m_cursor.column, 1);
            action.timestamp = std::time(nullptr);
            pushUndo(action);
        }
        m_lines[m_cursor.line].erase(m_cursor.column, 1);
        m_isModified = true;
    }
}

// void EditorTab::deleteSelection() - Removed (handled by default arg in header)

void EditorTab::deleteSelection(bool recordUndo) {
    if (!m_selection.active) return;
    deleteRange(m_selection.start, m_selection.end, recordUndo);
    m_selection.active = false;
}

void EditorTab::deleteRange(TextPosition start, TextPosition end, bool recordUndo) {
    // Normalize logic
    if (start.line > end.line || (start.line == end.line && start.column > end.column)) {
        std::swap(start, end);
    }

    if (start.line == end.line && start.column == end.column) return;
    
    std::string deletedText;
    if (recordUndo) {
         size_t currentLine = start.line;
         while(currentLine <= end.line) {
             if (currentLine >= m_lines.size()) break;
             if (currentLine == start.line && currentLine == end.line) {
                 if (start.column < m_lines[currentLine].length()) {
                     size_t len = std::min(end.column, m_lines[currentLine].length()) - start.column;
                     deletedText += m_lines[currentLine].substr(start.column, len);
                 }
             } else if (currentLine == start.line) {
                 if (start.column < m_lines[currentLine].length())
                    deletedText += m_lines[currentLine].substr(start.column);
                 deletedText += "\n";
             } else if (currentLine == end.line) {
                 size_t len = std::min(end.column, m_lines[currentLine].length());
                 deletedText += m_lines[currentLine].substr(0, len);
             } else {
                 deletedText += m_lines[currentLine];
                 deletedText += "\n";
             }
             currentLine++;
         }
    }

    m_cursor = start;
    if (start.line == end.line) {
        if (start.line < m_lines.size()) {
             size_t len = end.column - start.column;
             if (start.column < m_lines[start.line].length())
                m_lines[start.line].erase(start.column, len);
        }
    } else {
        if (start.line < m_lines.size() && end.line < m_lines.size()) {
            std::string suffix = "";
            if (end.column < m_lines[end.line].length()) suffix = m_lines[end.line].substr(end.column);
            
            if (start.column < m_lines[start.line].length())
                 m_lines[start.line].erase(start.column);
            m_lines[start.line] += suffix;
            
            if (end.line > start.line) {
                m_lines.erase(m_lines.begin() + start.line + 1, m_lines.begin() + end.line + 1);
            }
        }
    }
    
    if (recordUndo) {
        EditAction action;
        action.type = EditActionType::Delete;
        action.position = start;
        action.text = deletedText;
        action.timestamp = std::time(nullptr);
        pushUndo(action);
    }
    m_isModified = true;
    m_selection.active = false;
}

// void EditorTab::deleteSelection() - merged into deleteSelection(bool) default arg or logic
// Removed redundant function definition that was causing redefinition errors.

std::string EditorTab::getAllText() const {
    std::ostringstream oss;
    for (size_t i = 0; i < m_lines.size(); ++i) {
        oss << m_lines[i];
        if (i < m_lines.size() - 1) oss << "\n";
    }
    return oss.str();
}

std::string EditorTab::getSelectedText() const {
    if (!m_selection.active) return "";
    
    TextPosition start = m_selection.start;
    TextPosition end = m_selection.end;
    if (end < start) std::swap(start, end);

    if (start.line == end.line) {
        if (start.line >= m_lines.size()) return "";
        size_t len = end.column - start.column;
        if (start.column + len > m_lines[start.line].length()) {
            return m_lines[start.line].substr(start.column);
        }
        return m_lines[start.line].substr(start.column, len);
    }

    std::ostringstream ss;
    if (start.line < m_lines.size()) {
        if (start.column < m_lines[start.line].length())
            ss << m_lines[start.line].substr(start.column);
        ss << "\n";
    }

    for (size_t i = start.line + 1; i < end.line && i < m_lines.size(); ++i) {
        ss << m_lines[i] << "\n";
    }

    if (end.line < m_lines.size()) {
        if (end.column > m_lines[end.line].length()) {
             ss << m_lines[end.line];
        } else {
             ss << m_lines[end.line].substr(0, end.column);
        }
    }

    return ss.str();
}

void EditorTab::setSelection(const TextPosition& start, const TextPosition& end) {
    m_selection.active = true;
    m_selection.start = start;
    m_selection.end = end;
    if (m_selection.start.line >= m_lines.size()) m_selection.start.line = m_lines.size() - 1;
    if (m_selection.end.line >= m_lines.size()) m_selection.end.line = m_lines.size() - 1;
}

void EditorTab::setCursor(size_t line, size_t column) {
    if (line >= m_lines.size()) line = m_lines.size() - 1;
    if (column > m_lines[line].length()) column = m_lines[line].length();
    m_cursor = {line, column};
}

void EditorTab::moveCursor(int deltaLines, int deltaCols) {
    long newLine = static_cast<long>(m_cursor.line) + deltaLines;
    if (newLine < 0) newLine = 0;
    if (newLine >= static_cast<long>(m_lines.size())) newLine = m_lines.size() - 1;

    m_cursor.line = static_cast<size_t>(newLine);
    
    long newCol = static_cast<long>(m_cursor.column) + deltaCols;
    if (newCol < 0) newCol = 0;
    
    if (newCol > static_cast<long>(m_lines[m_cursor.line].length())) 
        newCol = m_lines[m_cursor.line].length();
        
    m_cursor.column = static_cast<size_t>(newCol);
}

void EditorTab::pushUndo(const EditAction& action) {
    m_undoStack.push_back(action);
    if (m_undoStack.size() > MAX_UNDO_DEPTH) {
        m_undoStack.pop_front();
    }
    m_redoStack.clear(); 
}

void EditorTab::undo() {
    if (m_undoStack.empty()) return;
    EditAction action = m_undoStack.back();
    m_undoStack.pop_back();

    m_cursor = action.position;

    if (action.type == EditActionType::Insert) {
        TextPosition endPos = action.position;
        for (char c : action.text) {
             if (c == '\n') { endPos.line++; endPos.column = 0; }
             else { endPos.column++; }
        }
        deleteRange(action.position, endPos, false); 
    } else if (action.type == EditActionType::Delete) {
        insertText(action.text, false);
    }
    
    m_redoStack.push_back(action);
    m_isModified = true;
}

void EditorTab::redo() {
    if (m_redoStack.empty()) return;
    EditAction action = m_redoStack.back();
    m_redoStack.pop_back();

    m_cursor = action.position;
    
    if (action.type == EditActionType::Insert) {
        insertText(action.text, false);
    } else if (action.type == EditActionType::Delete) {
        TextPosition endPos = action.position;
        for (char c : action.text) {
             if (c == '\n') { endPos.line++; endPos.column = 0; }
             else { endPos.column++; }
        }
        deleteRange(action.position, endPos, false);
    }
    
    m_undoStack.push_back(action);
    m_isModified = true;
}

// =========================================================================================
// MultiTabEditor Implementation (High Level Manager)
// =========================================================================================

MultiTabEditor::MultiTabEditor(void* parent) {
    ensureTabExists();
}

MultiTabEditor::~MultiTabEditor() {
    m_tabs.clear();
}

void MultiTabEditor::initialize() {
    std::cout << "[MultiTabEditor] Initialized pure logic engine.\n";
}

void MultiTabEditor::ensureTabExists() {
    if (m_tabs.empty()) {
        createNewTab();
    }
}

size_t MultiTabEditor::createNewTab() {
    auto tab = std::make_shared<EditorTab>();
    m_tabs.push_back(tab);
    m_activeTabIndex = m_tabs.size() - 1;
    return m_activeTabIndex;
}

void MultiTabEditor::openFile(const std::string& filepath) {
    for (size_t i = 0; i < m_tabs.size(); ++i) {
        if (m_tabs[i]->getFilePath() == filepath) {
            switchTab(i);
            return;
        }
    }

    if (m_tabs.size() == 1 && m_tabs[0]->getFilePath().empty() && !m_tabs[0]->isModified() && m_tabs[0]->getAllText().empty()) {
        m_tabs[0]->loadFromFile(filepath);
        return;
    }

    size_t idx = createNewTab();
    m_tabs[idx]->loadFromFile(filepath);
    switchTab(idx);
}

void MultiTabEditor::closeCurrentTab() {
    if (m_tabs.empty()) return;
    m_tabs.erase(m_tabs.begin() + m_activeTabIndex);
    if (m_activeTabIndex >= m_tabs.size()) {
        if (m_tabs.empty()) {
            createNewTab();
        } else {
            m_activeTabIndex = m_tabs.size() - 1;
        }
    }
}

void MultiTabEditor::switchTab(size_t index) {
    if (index < m_tabs.size()) {
        m_activeTabIndex = index;
    }
}

void MultiTabEditor::saveCurrentFile() {
    if (m_activeTabIndex < m_tabs.size()) {
        m_tabs[m_activeTabIndex]->saveToFile("");
    }
}

void MultiTabEditor::undo() {
    if (auto tab = getActiveTab()) tab->undo();
}

void MultiTabEditor::redo() {
    if (auto tab = getActiveTab()) tab->redo();
}

std::string MultiTabEditor::getCurrentText() const {
    if (m_tabs.empty()) return "";
    return m_tabs[m_activeTabIndex]->getAllText();
}

std::string MultiTabEditor::getSelectedText() const {
    if (m_tabs.empty()) return "";
    return m_tabs[m_activeTabIndex]->getSelectedText();
}

std::string MultiTabEditor::getCurrentFilePath() const {
    if (m_tabs.empty()) return "";
    return m_tabs[m_activeTabIndex]->getFilePath();
}

std::shared_ptr<EditorTab> MultiTabEditor::getActiveTab() {
    if (m_activeTabIndex < m_tabs.size()) {
        return m_tabs[m_activeTabIndex];
    }
    return nullptr;
}

void MultiTabEditor::setLSPClient(RawrXD::LSPClient* client) {
    m_lspClient = client;
}

MultiTabEditor::FindResult MultiTabEditor::find(const std::string& query, bool forward, bool caseSensitive) {
     auto tab = getActiveTab();
     if (!tab || query.empty()) return {0,0,0,false};
     
     const auto& lines = tab->getLines();
     
     for (size_t i = 0; i < lines.size(); ++i) {
         size_t foundPos = std::string::npos;
         if (caseSensitive) {
             foundPos = lines[i].find(query);
         } else {
             std::string lineLower = lines[i];
             std::string queryLower = query;
             std::transform(lineLower.begin(), lineLower.end(), lineLower.begin(), ::tolower);
             std::transform(queryLower.begin(), queryLower.end(), queryLower.begin(), ::tolower);
             foundPos = lineLower.find(queryLower);
         }

         if (foundPos != std::string::npos) {
             return {i, foundPos, query.length(), true};
         }
     }
     
     return {0,0,0,false};
}

int MultiTabEditor::replace(const std::string& query, const std::string& replacement, bool all) {
     auto tab = getActiveTab();
     if (!tab) return 0;
     
     int count = 0;
     while (true) {
         auto result = find(query, true, true);
         if (!result.found) break;
         
         // Select and replace
         tab->setSelection({result.line, result.column}, {result.line, result.column + result.length});
         tab->deleteSelection();
         tab->insertText(replacement);
         
         count++;
         if (!all) break;
     }

     return count;
}
