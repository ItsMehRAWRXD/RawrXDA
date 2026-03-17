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
        // Handle CR/LF
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
        if (i < m_lines.size() - 1) { // Standard text file newline behavior
             file << "\n"; // Or \r\n on Windows if strictly required, but \n is generally fine
        }
    }

    m_filePath = targetPath;
    m_isModified = false;
    std::cout << "[EditorTab] Saved " << m_lines.size() << " lines to " << targetPath << "\n";
}

void EditorTab::insertText(const std::string& text, bool recordUndo) {
    if (text.empty()) return;

    // Record Undo ensure atomic
    if (recordUndo) {
        EditAction action;
        action.type = EditActionType::Insert;
        action.position = m_cursor;
        action.text = text;
        action.timestamp = std::time(nullptr);
        pushUndo(action);
    }

    // Basic insertion logic (naive)
    // Multi-line insertion supported
    std::string remaining = text;
    size_t pos = 0;
    while (true) {
        size_t newline = remaining.find('\n', pos);
        if (newline == std::string::npos) {
            // No more newlines, insert remaining
            m_lines[m_cursor.line].insert(m_cursor.column, remaining.substr(pos));
            m_cursor.column += remaining.length() - pos;
            break;
        } else {
            // Insert up to newline
            std::string segment = remaining.substr(pos, newline - pos);
            std::string& line = m_lines[m_cursor.line];
            line.insert(m_cursor.column, segment);
            
            // Split line
            std::string suffix = line.substr(m_cursor.column + segment.length());
            line.erase(m_cursor.column + segment.length());
            
            // Insert new line
            m_lines.insert(m_lines.begin() + m_cursor.line + 1, suffix);
            
            m_cursor.line++;
            m_cursor.column = 0;
            pos = newline + 1;
        }
    }
    m_isModified = true;
}

void EditorTab::deleteBack(bool recordUndo) {
    if (m_cursor.column > 0) {
        std::string& currentLine = m_lines[m_cursor.line];
        
        // Record Undo
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
        // Merge with previous line
        std::string currentLineContent = m_lines[m_cursor.line];
        m_lines.erase(m_lines.begin() + m_cursor.line);
        m_cursor.line--;
        
        size_t oldLen = m_lines[m_cursor.line].length();

         // Record Undo (Complex - simplified here for robust compilation)
        if (recordUndo) {
            EditAction action;
            action.type = EditActionType::Delete; // Treated as delete of newline
            action.position = {m_cursor.line, oldLen};
            action.text = "\n"; 
            pushUndo(action);
        }

        m_lines[m_cursor.line] += currentLineContent;
         m_cursor.column = oldLen;
        m_isModified = true;
    }
}

void EditorTab::deleteForward() {
    // Check EOF
    if (m_cursor.line >= m_lines.size()) return;
    if (m_cursor.line == m_lines.size() - 1 && m_cursor.column >= m_lines[m_cursor.line].length()) return;

    if (m_cursor.column < m_lines[m_cursor.line].length()) {
        // Delete Single Char Forward
        std::string deleted = m_lines[m_cursor.line].substr(m_cursor.column, 1);
        
        EditAction action;
        action.type = EditActionType::Delete;
        action.position = m_cursor;
        action.text = deleted;
        action.timestamp = std::time(nullptr);
        pushUndo(action);

        m_lines[m_cursor.line].erase(m_cursor.column, 1);
        m_isModified = true;
    } else {
        // Merge with next line (Delete Newline)
        if (m_cursor.line + 1 >= m_lines.size()) return;
        
        std::string nextLine = m_lines[m_cursor.line + 1];
        
        EditAction action;
        action.type = EditActionType::Delete;
        action.position = m_cursor;
        action.text = "\n"; 
        pushUndo(action);

        m_lines[m_cursor.line] += nextLine;
        m_lines.erase(m_lines.begin() + m_cursor.line + 1);
        m_isModified = true;
    }
}

void EditorTab::deleteSelection() {
    if (!m_selection.active) return;

    // Normalize selection (start must be before end)
    TextPosition start = m_selection.start;
    TextPosition end = m_selection.end;
    if (end < start) std::swap(start, end);

    // Record Undo (Transaction)
    // For now, we only push one massive composite action if we had a transaction system.
    // Simplifying to: clear selection content.

    if (start.line == end.line) {
        // Single line deletion
        std::string& line = m_lines[start.line];
        size_t len = end.column - start.column;
        std::string deletedText = line.substr(start.column, len);
        
        EditAction action;
        action.type = EditActionType::Delete;
        action.position = start;
        action.text = deletedText;
        action.timestamp = std::time(nullptr);
        pushUndo(action);

        line.erase(start.column, len);
    } else {
        // Multi-line deletion
        // 1. First line suffix
        // 2. Intermediate lines
        // 3. Last line prefix
        
        // This is complex logic often omitted. Implementing explicit steps.
        
        // Step 1: Collect deleted text for undo
        std::string deletedText;
        deletedText += m_lines[start.line].substr(start.column);
        deletedText += "\n";
        for (size_t i = start.line + 1; i < end.line; ++i) {
            deletedText += m_lines[i] + "\n";
        }
        deletedText += m_lines[end.line].substr(0, end.column);

        EditAction action;
        action.type = EditActionType::Delete;
        action.position = start;
        action.text = deletedText;
        pushUndo(action); // Assumes undo system can handle multi-line strings

        // Step 2: Modify Buffer
        m_lines[start.line].erase(start.column); // Remove suffix
        std::string suffixRemaining = m_lines[end.line].substr(end.column);
        
        // Erase intermediate lines
        m_lines.erase(m_lines.begin() + start.line + 1, m_lines.begin() + end.line + 1);
        
        // Append suffix to start line
        m_lines[start.line] += suffixRemaining;
    }

    m_cursor = start;
    m_selection.active = false;
    m_isModified = true;
}

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
        ss << m_lines[start.line].substr(start.column) << "\n";
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
    // Bounds check
    if (m_selection.start.line >= m_lines.size()) m_selection.start.line = m_lines.size() - 1;
    if (m_selection.end.line >= m_lines.size()) m_selection.end.line = m_lines.size() - 1;
}
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
    
    // Clamp to line length
    if (newCol > static_cast<long>(m_lines[m_cursor.line].length())) 
        newCol = m_lines[m_cursor.line].length();
        
    m_cursor.column = static_cast<size_t>(newCol);
}

void EditorTab::pushUndo(const EditAction& action) {
    m_undoStack.push_back(action);
    if (m_undoStack.size() > MAX_UNDO_DEPTH) {
        m_undoStack.pop_front();
    }
    m_redoStack.clear(); // innovative action clears redo
}

void EditorTab::undo() {
    if (m_undoStack.empty()) return;
    EditAction action = m_undoStack.back();
    m_undoStack.pop_back();

    // Reverse the action
    if (action.type == EditActionType::Insert) {
        // Undo Insert = Delete
        // Simplistic implementation for single line
        if (action.position.line < m_lines.size()) {
             m_lines[action.position.line].erase(action.position.column, action.text.length());
             m_cursor = action.position;
        }
    } else if (action.type == EditActionType::Delete) {
        // Undo Delete = Insert
        if (action.text == "\n") {
             // Split line logic
        } else {
             if (action.position.line < m_lines.size()) {
                  m_lines[action.position.line].insert(action.position.column, action.text);
                  m_cursor = {action.position.line, action.position.column + action.text.length()};
             }
        }
    }
    
    m_redoStack.push_back(action);
    m_isModified = true; // Techincally might be back to clean state, needs hash check
}

void EditorTab::redo() {
    if (m_redoStack.empty()) return;
    EditAction action = m_redoStack.back();
    m_redoStack.pop_back();
    
    // Re-apply action without recording Undo (avoid loops)
    if (action.type == EditActionType::Insert) {
        // We replicate insertText logic but manually to avoid pushing to undo stack
        std::string remaining = action.text;
        size_t pos = 0;
        TextPosition tempCursor = action.position;
        
        while (true) {
            size_t newline = remaining.find('\n', pos);
            if (newline == std::string::npos) {
                if (tempCursor.line < m_lines.size()) {
                    m_lines[tempCursor.line].insert(tempCursor.column, remaining.substr(pos));
                    tempCursor.column += remaining.length() - pos;
                }
                break;
            } else {
                std::string segment = remaining.substr(pos, newline - pos);
                if (tempCursor.line < m_lines.size()) {
                    std::string& line = m_lines[tempCursor.line];
                    line.insert(tempCursor.column, segment);
                    std::string suffix = line.substr(tempCursor.column + segment.length());
                    line.erase(tempCursor.column + segment.length());
                    m_lines.insert(m_lines.begin() + tempCursor.line + 1, suffix);
                    tempCursor.line++;
                    tempCursor.column = 0;
                    pos = newline + 1;
                } else break;
            }
        }
        m_cursor = tempCursor;
        m_undoStack.push_back(action); // Push back to undo!

    } else if (action.type == EditActionType::Delete) {
        // Re-delete
        // For simplicity, we can reuse delete logic if we set cursor and don't record undo?
        // But deleteBack/Forward are position relative.
        // Let's implement specific explicit delete at position
        
        TextPosition oldCv = m_cursor;
        m_cursor = action.position;
        
        if (action.text == "\n") {
             // Delete forward logic (merge lines)
             if (m_cursor.line + 1 < m_lines.size()) {
                 m_lines[m_cursor.line] += m_lines[m_cursor.line + 1];
                 m_lines.erase(m_lines.begin() + m_cursor.line + 1);
             }
        } else {
             // Basic text delete
             // Warning: Multi-line re-delete is complex, assuming single block for now or iterative
             // If action.text has newlines, we need to handle multi-line erase
             // ... Similar to deleteSelection logic ...
             // For robustness in this recursive fix, assuming single line chunk usually
             if (m_cursor.line < m_lines.size()) {
                 m_lines[m_cursor.line].erase(m_cursor.column, action.text.length());
             }
        }
        m_undoStack.push_back(action);
    }
    m_isModified = true;
}

// =========================================================================================
// MultiTabEditor Implementation (High Level Manager)
// =========================================================================================

MultiTabEditor::MultiTabEditor(void* parent) {
    // Parent ignored in pure C++ mode
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
    // Check if open
    for (size_t i = 0; i < m_tabs.size(); ++i) {
        if (m_tabs[i]->getFilePath() == filepath) {
            switchTab(i);
            return;
        }
    }

    // Reuse empty untitled tab if possible
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

// Search Logic
MultiTabEditor::FindResult MultiTabEditor::find(const std::string& query, bool forward, bool caseSensitive) {
     auto tab = getActiveTab();
     if (!tab || query.empty()) return {0,0,0,false};
     
     const auto& lines = tab->getLines();
     // Naive implementation: search line by line starting from cursor
     // TODO: Implement circular search and direction support
     
     for (size_t i = 0; i < lines.size(); ++i) {
         size_t foundPos = std::string::npos;
         if (caseSensitive) {
             foundPos = lines[i].find(query);
         } else {
             // Lowercase comparison
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
         auto result = find(query, true, true); // Forward, Case Sensitive
         if (!result.found) break;

         // Construct selection for deletion
         tab->setCursor(result.line, result.col);
         // Move to end of match
         // tab->moveCursor(0, result.length); // This just moves cursor
         
         // Direct buffer manipulation via simulate delete+insert
         // We select the range
         tab->setSelection({result.line, result.col}, {result.line, result.col + result.length});
         tab->deleteSelection();
         tab->insertText(replacement); // Handles modification
         
         count++;
         if (!all) break;
     }

     return count;
}
