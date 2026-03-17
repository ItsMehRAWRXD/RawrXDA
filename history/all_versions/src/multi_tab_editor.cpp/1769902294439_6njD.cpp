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

void EditorTab::deleteSelection(bool recordUndo) {
    if (!m_selection.active) return;
    deleteRange(m_selection.start, m_selection.end, recordUndo);
    m_selection.active = false;
}

void EditorTab::deleteRange(TextPosition start, TextPosition end, bool recordUndo) {
    if (end < start) std::swap(start, end);

    // Record Undo (Transaction)
    // For now, we only push one massive composite action if we had a transaction system.
    // Simplifying to: clear selection content.

    if (start.line == end.line) {
        // Single line deletion
        std::string& line = m_lines[start.line];
        size_t len = end.column - start.column;
        std::string deletedText = line.substr(start.column, len);
        
        if (recordUndo) {
            EditAction action;
            action.type = EditActionType::Delete;
            action.position = start;
            action.text = deletedText;
            action.timestamp = std::time(nullptr);
            pushUndo(action);
        }

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

        if (recordUndo) {
            EditAction action;
            action.type = EditActionType::Delete;
            action.position = start;
            action.text = deletedText;
            pushUndo(action); // Assumes undo system can handle multi-line strings
        }

        // Step 2: Modify Buffer
        m_lines[start.line].erase(start.column); // Remove suffix
        std::string suffixRemaining = m_lines[end.line].substr(end.column);
        
        // Erase intermediate lines
        m_lines.erase(m_lines.begin() + start.line + 1, m_lines.begin() + end.line + 1);
        
        // Append suffix to start line
        m_lines[start.line] += suffixRemaining;
    }

    m_cursor = start;
    m_isModified = true;
}

void EditorTab::deleteForward(bool recordUndo) {
    deleteForward(true);
}

void EditorTab::deleteSelection() {
    deleteSelection(true);
}

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
            pushUndo(action);
        }
        m_lines[m_cursor.line].erase(m_cursor.column, 1);
        m_isModified = true;
    }
}

void EditorTab::deleteSelection(bool recordUndo) {
    if (!m_selection.active) return;
    deleteRange(m_selection.start, m_selection.end, recordUndo);
    m_selection.active = false;
}

void EditorTab::deleteRange(TextPosition start, TextPosition end, bool recordUndo) {
    if (end < start) std::swap(start, end);

    // Record Undo (Transaction)
    // For now, we only push one massive composite action if we had a transaction system.
    // Simplifying to: clear selection content.

    if (start.line == end.line) {
        // Single line deletion
        std::string& line = m_lines[start.line];
        size_t len = end.column - start.column;
        std::string deletedText = line.substr(start.column, len);
        
        if (recordUndo) {
            EditAction action;
            action.type = EditActionType::Delete;
            action.position = start;
            action.text = deletedText;
            action.timestamp = std::time(nullptr);
            pushUndo(action);
        }

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

        if (recordUndo) {
            EditAction action;
            action.type = EditActionType::Delete;
            action.position = start;
            action.text = deletedText;
            pushUndo(action); // Assumes undo system can handle multi-line strings
        }

        // Step 2: Modify Buffer
        m_lines[start.line].erase(start.column); // Remove suffix
        std::string suffixRemaining = m_lines[end.line].substr(end.column);
        
        // Erase intermediate lines
        m_lines.erase(m_lines.begin() + start.line + 1, m_lines.begin() + end.line + 1);
        
        // Append suffix to start line
        m_lines[start.line] += suffixRemaining;
    }

    m_cursor = start;
    m_isModified = true;
}

void EditorTab::deleteForward(bool recordUndo) {
    // Check EOF
    if (m_cursor.line >= m_lines.size()) return;
    if (m_cursor.line == m_lines.size() - 1 && m_cursor.column >= m_lines[m_cursor.line].length()) return;

    if (m_cursor.column < m_lines[m_cursor.line].length()) {
        // Delete Single Char Forward
        std::string deleted = m_lines[m_cursor.line].substr(m_cursor.column, 1);
        
        if (recordUndo) {
            EditAction action;
            action.type = EditActionType::Delete;
            action.position = m_cursor;
            action.text = deleted;
            action.timestamp = std::time(nullptr);
            pushUndo(action);
        }

        m_lines[m_cursor.line].erase(m_cursor.column, 1);
        m_isModified = true;
    } else {
        // Merge with next line (Delete Newline)
        if (m_cursor.line + 1 >= m_lines.size()) return;
        
        std::string nextLine = m_lines[m_cursor.line + 1];
        
        if (recordUndo) {
            EditAction action;
            action.type = EditActionType::Delete;
            action.position = m_cursor;
            action.text = "\n"; 
            pushUndo(action);
        }

        m_lines[m_cursor.line] += nextLine;
        m_lines.erase(m_lines.begin() + m_cursor.line + 1);
        m_isModified = true;
    }
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

void EditorTab::setCursor(size_t line, size_t column) {
    if (line >= m_lines.size()) line = m_lines.size() - 1;
    if (column > m_lines[line].length()) column = m_lines[line].length();
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

    m_cursor = action.position;

    if (action.type == EditActionType::Insert) {
        // Undo Insert = Delete Range
        Cursor endPos = action.position;
        for (char c : action.text) {
             if (c == '\n') { endPos.line++; endPos.column = 0; }
             else { endPos.column++; }
        }
        deleteRange(action.position, endPos, false); 
    } else if (action.type == EditActionType::Delete) {
        // Undo Delete = Insert Text
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
        Cursor endPos = action.position;
        for (char c : action.text) {
             if (c == '\n') { endPos.line++; endPos.column = 0; }
             else { endPos.column++; }
        }
        deleteRange(action.position, endPos, false);
    }
    
    m_undoStack.push_back(action);
    m_isModified = true;
}
        // We replicate insertText logic but manually to avoid pushing to undo stack
        std::string remaining = action.text;
        size_t pos = 0;    size_t newline = remaining.find('\n', pos);
        TextPosition tempCursor = action.position;e == std::string::npos) {
        
        while (true) {sert(tempCursor.column, remaining.substr(pos));
            size_t newline = remaining.find('\n', pos);th() - pos;
            if (newline == std::string::npos) {
                if (tempCursor.line < m_lines.size()) {
                    m_lines[tempCursor.line].insert(tempCursor.column, remaining.substr(pos));e {
                    tempCursor.column += remaining.length() - pos;tring segment = remaining.substr(pos, newline - pos);
                }tempCursor.line < m_lines.size()) {
                break;
            } else {ent);
                std::string segment = remaining.substr(pos, newline - pos);olumn + segment.length());
                if (tempCursor.line < m_lines.size()) {ength());
                    std::string& line = m_lines[tempCursor.line];
                    line.insert(tempCursor.column, segment);
                    std::string suffix = line.substr(tempCursor.column + segment.length());
                    line.erase(tempCursor.column + segment.length());
                    m_lines.insert(m_lines.begin() + tempCursor.line + 1, suffix);
                    tempCursor.line++;
                    tempCursor.column = 0;
                    pos = newline + 1;sor = tempCursor;
                } else break;_undoStack.push_back(action); // Push back to undo!
            }
        }
        m_cursor = tempCursor;        // Re-delete
        m_undoStack.push_back(action); // Push back to undo! we set cursor and don't record undo?
eBack/Forward are position relative.
    } else if (action.type == EditActionType::Delete) {
        // Re-delete
        // For simplicity, we can reuse delete logic if we set cursor and don't record undo?
        // But deleteBack/Forward are position relative.m_cursor = action.position;
        // Let's implement specific explicit delete at position
        
        TextPosition oldCv = m_cursor;     // Delete forward logic (merge lines)
        m_cursor = action.position; < m_lines.size()) {
        [m_cursor.line + 1];
        if (action.text == "\n") {sor.line + 1);
             // Delete forward logic (merge lines)
             if (m_cursor.line + 1 < m_lines.size()) {
                 m_lines[m_cursor.line] += m_lines[m_cursor.line + 1];/ Basic text delete
                 m_lines.erase(m_lines.begin() + m_cursor.line + 1);Warning: Multi-line re-delete is complex, assuming single block for now or iterative
             }s newlines, we need to handle multi-line erase
        } else {
             // Basic text deletek usually
             // Warning: Multi-line re-delete is complex, assuming single block for now or iterative
             // If action.text has newlines, we need to handle multi-line erase
             // ... Similar to deleteSelection logic ...
             // For robustness in this recursive fix, assuming single line chunk usually
             if (m_cursor.line < m_lines.size()) {Stack.push_back(action);
                 m_lines[m_cursor.line].erase(m_cursor.column, action.text.length());
             }
        }
        m_undoStack.push_back(action);
    }/ =========================================================================================
    m_isModified = true;// MultiTabEditor Implementation (High Level Manager)
}

// =========================================================================================
// MultiTabEditor Implementation (High Level Manager)    // Parent ignored in pure C++ mode
// =========================================================================================

MultiTabEditor::MultiTabEditor(void* parent) {
    // Parent ignored in pure C++ modeultiTabEditor::~MultiTabEditor() {
    ensureTabExists();    m_tabs.clear();
}

MultiTabEditor::~MultiTabEditor() {oid MultiTabEditor::initialize() {
    m_tabs.clear();    std::cout << "[MultiTabEditor] Initialized pure logic engine.\n";
}

void MultiTabEditor::initialize() {oid MultiTabEditor::ensureTabExists() {
    std::cout << "[MultiTabEditor] Initialized pure logic engine.\n";    if (m_tabs.empty()) {
}

void MultiTabEditor::ensureTabExists() {
    if (m_tabs.empty()) {
        createNewTab();ize_t MultiTabEditor::createNewTab() {
    }    auto tab = std::make_shared<EditorTab>();
}

size_t MultiTabEditor::createNewTab() {x;
    auto tab = std::make_shared<EditorTab>();
    m_tabs.push_back(tab);
    m_activeTabIndex = m_tabs.size() - 1;oid MultiTabEditor::openFile(const std::string& filepath) {
    return m_activeTabIndex;    // Check if open
}
]->getFilePath() == filepath) {
void MultiTabEditor::openFile(const std::string& filepath) {
    // Check if open
    for (size_t i = 0; i < m_tabs.size(); ++i) {
        if (m_tabs[i]->getFilePath() == filepath) {
            switchTab(i);
            return;/ Reuse empty untitled tab if possible
        }    if (m_tabs.size() == 1 && m_tabs[0]->getFilePath().empty() && !m_tabs[0]->isModified() && m_tabs[0]->getAllText().empty()) {
    }

    // Reuse empty untitled tab if possible
    if (m_tabs.size() == 1 && m_tabs[0]->getFilePath().empty() && !m_tabs[0]->isModified() && m_tabs[0]->getAllText().empty()) {
        m_tabs[0]->loadFromFile(filepath);ize_t idx = createNewTab();
        return;    m_tabs[idx]->loadFromFile(filepath);
    }

    size_t idx = createNewTab();
    m_tabs[idx]->loadFromFile(filepath);oid MultiTabEditor::closeCurrentTab() {
    switchTab(idx);    if (m_tabs.empty()) return;
}veTabIndex);
abs.size()) {
void MultiTabEditor::closeCurrentTab() {
    if (m_tabs.empty()) return;
    m_tabs.erase(m_tabs.begin() + m_activeTabIndex);
    if (m_activeTabIndex >= m_tabs.size()) {x = m_tabs.size() - 1;
        if (m_tabs.empty()) {
            createNewTab();
        } else {
            m_activeTabIndex = m_tabs.size() - 1;
        }oid MultiTabEditor::switchTab(size_t index) {
    }    if (index < m_tabs.size()) {
}

void MultiTabEditor::switchTab(size_t index) {
    if (index < m_tabs.size()) {
        m_activeTabIndex = index;oid MultiTabEditor::saveCurrentFile() {
    }    if (m_activeTabIndex < m_tabs.size()) {
}File("");

void MultiTabEditor::saveCurrentFile() {
    if (m_activeTabIndex < m_tabs.size()) {
        m_tabs[m_activeTabIndex]->saveToFile("");oid MultiTabEditor::undo() {
    }    if (auto tab = getActiveTab()) tab->undo();
}

void MultiTabEditor::undo() {oid MultiTabEditor::redo() {
    if (auto tab = getActiveTab()) tab->undo();    if (auto tab = getActiveTab()) tab->redo();
}

void MultiTabEditor::redo() {td::string MultiTabEditor::getCurrentText() const {
    if (auto tab = getActiveTab()) tab->redo();    if (m_tabs.empty()) return "";
}

std::string MultiTabEditor::getCurrentText() const {
    if (m_tabs.empty()) return "";td::string MultiTabEditor::getSelectedText() const {
    return m_tabs[m_activeTabIndex]->getAllText();    if (m_tabs.empty()) return "";
});

std::string MultiTabEditor::getSelectedText() const {
    if (m_tabs.empty()) return "";td::string MultiTabEditor::getCurrentFilePath() const {
    return m_tabs[m_activeTabIndex]->getSelectedText();    if (m_tabs.empty()) return "";
}

std::string MultiTabEditor::getCurrentFilePath() const {
    if (m_tabs.empty()) return "";td::shared_ptr<EditorTab> MultiTabEditor::getActiveTab() {
    return m_tabs[m_activeTabIndex]->getFilePath();    if (m_activeTabIndex < m_tabs.size()) {
}

std::shared_ptr<EditorTab> MultiTabEditor::getActiveTab() {
    if (m_activeTabIndex < m_tabs.size()) {
        return m_tabs[m_activeTabIndex];
    }oid MultiTabEditor::setLSPClient(RawrXD::LSPClient* client) {
    return nullptr;    m_lspClient = client;
}

void MultiTabEditor::setLSPClient(RawrXD::LSPClient* client) {/ Search Logic
    m_lspClient = client;MultiTabEditor::FindResult MultiTabEditor::find(const std::string& query, bool forward, bool caseSensitive) {
} getActiveTab();

// Search Logic
MultiTabEditor::FindResult MultiTabEditor::find(const std::string& query, bool forward, bool caseSensitive) {
     auto tab = getActiveTab();// Naive implementation: search line by line starting from cursor
     if (!tab || query.empty()) return {0,0,0,false};nd direction support
     
     const auto& lines = tab->getLines();
     // Naive implementation: search line by line starting from cursor    size_t foundPos = std::string::npos;
     // TODO: Implement circular search and direction support
     
     for (size_t i = 0; i < lines.size(); ++i) {
         size_t foundPos = std::string::npos;
         if (caseSensitive) {:string lineLower = lines[i];
             foundPos = lines[i].find(query);= query;
         } else { lineLower.end(), lineLower.begin(), ::tolower);
             // Lowercase comparison(), queryLower.end(), queryLower.begin(), ::tolower);
             std::string lineLower = lines[i];
             std::string queryLower = query;
             std::transform(lineLower.begin(), lineLower.end(), lineLower.begin(), ::tolower);
             std::transform(queryLower.begin(), queryLower.end(), queryLower.begin(), ::tolower);f (foundPos != std::string::npos) {
             foundPos = lineLower.find(queryLower);             return {i, foundPos, query.length(), true};
         }

         if (foundPos != std::string::npos) {
             return {i, foundPos, query.length(), true};return {0,0,0,false};
         }
     }
     nt MultiTabEditor::replace(const std::string& query, const std::string& replacement, bool all) {
     return {0,0,0,false};     auto tab = getActiveTab();
}

int MultiTabEditor::replace(const std::string& query, const std::string& replacement, bool all) {
     auto tab = getActiveTab();while (true) {
     if (!tab) return 0;t = find(query, true, true); // Forward, Case Sensitive
     t.found) break;
     int count = 0;
     while (true) {r deletion
         auto result = find(query, true, true); // Forward, Case Sensitive         tab->setCursor(result.line, result.col);
         if (!result.found) break;
 This just moves cursor
         // Construct selection for deletion
         tab->setCursor(result.line, result.col);
         // Move to end of match// We select the range
         // tab->moveCursor(0, result.length); // This just moves cursore, result.col + result.length});
         ;
         // Direct buffer manipulation via simulate delete+insert
         // We select the range
         tab->setSelection({result.line, result.col}, {result.line, result.col + result.length});
         tab->deleteSelection();if (!all) break;
         tab->insertText(replacement); // Handles modification
         
         count++;return count;
         if (!all) break;}
     }     return count;
}
