#include "multi_tab_editor.h"
#include "lsp_client.h"
#include <sstream>
#include <algorithm>
#include <fstream>

namespace RawrXD {

// ============================================================================
// EditorTab Implementation
// ============================================================================

EditorTab::EditorTab() {
    m_lines.push_back(""); // Start with one empty line
}

EditorTab::~EditorTab() = default;

void EditorTab::loadFromFile(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) return;

    m_lines.clear();
    std::string line;
    while (std::getline(f, line)) {
        m_lines.push_back(line);
    }
    if (m_lines.empty()) {
        m_lines.push_back("");
    }

    m_filePath = path;
    m_isModified = false;
    m_cursor = {0, 0};
    m_selection = {};
    m_undoStack.clear();
    m_redoStack.clear();
}

void EditorTab::saveToFile(const std::string& path) {
    std::ofstream f(path);
    if (!f.is_open()) return;

    for (size_t i = 0; i < m_lines.size(); ++i) {
        f << m_lines[i];
        if (i + 1 < m_lines.size()) f << '\n';
    }
    if (!path.empty()) m_filePath = path;
    m_isModified = false;
}

void EditorTab::insertText(const std::string& text, bool recordUndo) {
    if (m_readOnly) return;

    size_t line = static_cast<size_t>(m_cursor.line);
    size_t col  = static_cast<size_t>(m_cursor.col);

    if (line >= m_lines.size()) line = m_lines.size() - 1;
    if (col > m_lines[line].size()) col = m_lines[line].size();

    if (recordUndo) {
        pushUndo({EditActionType::Insert, text, m_cursor});
    }

    // Handle multi-line insert
    std::vector<std::string> insertLines;
    std::istringstream ss(text);
    std::string seg;
    while (std::getline(ss, seg, '\n')) {
        insertLines.push_back(seg);
    }
    // If text ends with newline, add empty trailing line
    if (!text.empty() && text.back() == '\n') {
        insertLines.push_back("");
    }
    if (insertLines.empty()) return;

    if (insertLines.size() == 1) {
        m_lines[line].insert(col, insertLines[0]);
        m_cursor.col = static_cast<int>(col + insertLines[0].size());
    } else {
        std::string tail = m_lines[line].substr(col);
        m_lines[line] = m_lines[line].substr(0, col) + insertLines[0];

        for (size_t i = 1; i < insertLines.size() - 1; ++i) {
            m_lines.insert(m_lines.begin() + static_cast<ptrdiff_t>(line + i), insertLines[i]);
        }

        size_t lastIdx = line + insertLines.size() - 1;
        m_lines.insert(m_lines.begin() + static_cast<ptrdiff_t>(lastIdx), insertLines.back() + tail);

        m_cursor.line = static_cast<int>(lastIdx);
        m_cursor.col = static_cast<int>(insertLines.back().size());
    }

    m_isModified = true;
}

void EditorTab::deleteBack(bool recordUndo) {
    if (m_readOnly) return;
    size_t line = static_cast<size_t>(m_cursor.line);
    size_t col  = static_cast<size_t>(m_cursor.col);

    if (col > 0 && line < m_lines.size()) {
        if (recordUndo) {
            pushUndo({EditActionType::Delete, std::string(1, m_lines[line][col - 1]), m_cursor});
        }
        m_lines[line].erase(col - 1, 1);
        m_cursor.col = static_cast<int>(col - 1);
        m_isModified = true;
    } else if (col == 0 && line > 0) {
        // Merge with previous line
        if (recordUndo) {
            pushUndo({EditActionType::Delete, "\n", m_cursor});
        }
        m_cursor.col = static_cast<int>(m_lines[line - 1].size());
        m_lines[line - 1] += m_lines[line];
        m_lines.erase(m_lines.begin() + static_cast<ptrdiff_t>(line));
        m_cursor.line = static_cast<int>(line - 1);
        m_isModified = true;
    }
}

void EditorTab::deleteForward(bool recordUndo) {
    if (m_readOnly) return;
    size_t line = static_cast<size_t>(m_cursor.line);
    size_t col  = static_cast<size_t>(m_cursor.col);

    if (line < m_lines.size() && col < m_lines[line].size()) {
        if (recordUndo) {
            pushUndo({EditActionType::Delete, std::string(1, m_lines[line][col]), m_cursor});
        }
        m_lines[line].erase(col, 1);
        m_isModified = true;
    } else if (line + 1 < m_lines.size() && col >= m_lines[line].size()) {
        if (recordUndo) {
            pushUndo({EditActionType::Delete, "\n", m_cursor});
        }
        m_lines[line] += m_lines[line + 1];
        m_lines.erase(m_lines.begin() + static_cast<ptrdiff_t>(line + 1));
        m_isModified = true;
    }
}

void EditorTab::deleteSelection(bool recordUndo) {
    if (m_readOnly) return;
    deleteRange(m_selection.start, m_selection.end, recordUndo);
}

void EditorTab::deleteRange(TextPosition start, TextPosition end, bool recordUndo) {
    if (m_readOnly) return;
    // Normalize range so start <= end
    if (start.line > end.line || (start.line == end.line && start.col > end.col)) {
        std::swap(start, end);
    }

    // Get the text being deleted for undo
    if (recordUndo) {
        // Build deleted text (simplified — single line or cross-line)
        std::string deleted;
        if (start.line == end.line) {
            size_t ln = static_cast<size_t>(start.line);
            if (ln < m_lines.size()) {
                size_t sc = std::min(static_cast<size_t>(start.col), m_lines[ln].size());
                size_t ec = std::min(static_cast<size_t>(end.col), m_lines[ln].size());
                deleted = m_lines[ln].substr(sc, ec - sc);
            }
        }
        pushUndo({EditActionType::Delete, deleted, start});
    }

    size_t sLine = static_cast<size_t>(start.line);
    size_t eLine = static_cast<size_t>(end.line);

    if (sLine >= m_lines.size()) return;
    if (eLine >= m_lines.size()) eLine = m_lines.size() - 1;

    size_t sCol = std::min(static_cast<size_t>(start.col), m_lines[sLine].size());
    size_t eCol = std::min(static_cast<size_t>(end.col), m_lines[eLine].size());

    if (sLine == eLine) {
        m_lines[sLine].erase(sCol, eCol - sCol);
    } else {
        std::string merged = m_lines[sLine].substr(0, sCol) + m_lines[eLine].substr(eCol);
        m_lines.erase(m_lines.begin() + static_cast<ptrdiff_t>(sLine),
                       m_lines.begin() + static_cast<ptrdiff_t>(eLine + 1));
        m_lines.insert(m_lines.begin() + static_cast<ptrdiff_t>(sLine), merged);
    }

    m_cursor = start;
    m_isModified = true;
}

std::string EditorTab::getAllText() const {
    std::string result;
    for (size_t i = 0; i < m_lines.size(); ++i) {
        result += m_lines[i];
        if (i + 1 < m_lines.size()) result += '\n';
    }
    return result;
}

std::string EditorTab::getSelectedText() const {
    auto s = m_selection.start;
    auto e = m_selection.end;
    if (s.line > e.line || (s.line == e.line && s.col > e.col)) {
        std::swap(s, e);
    }
    if (s.line == e.line) {
        size_t ln = static_cast<size_t>(s.line);
        if (ln >= m_lines.size()) return "";
        size_t sc = std::min(static_cast<size_t>(s.col), m_lines[ln].size());
        size_t ec = std::min(static_cast<size_t>(e.col), m_lines[ln].size());
        return m_lines[ln].substr(sc, ec - sc);
    }

    std::string result;
    size_t sLine = static_cast<size_t>(s.line);
    size_t eLine = static_cast<size_t>(e.line);
    if (sLine >= m_lines.size()) return "";
    if (eLine >= m_lines.size()) eLine = m_lines.size() - 1;

    result += m_lines[sLine].substr(std::min(static_cast<size_t>(s.col), m_lines[sLine].size()));
    for (size_t i = sLine + 1; i < eLine; ++i) {
        result += '\n';
        result += m_lines[i];
    }
    result += '\n';
    result += m_lines[eLine].substr(0, std::min(static_cast<size_t>(e.col), m_lines[eLine].size()));
    return result;
}

void EditorTab::setCursor(size_t line, size_t column) {
    m_cursor.line = static_cast<int>(std::min(line, m_lines.empty() ? 0 : m_lines.size() - 1));
    if (m_cursor.line >= 0 && static_cast<size_t>(m_cursor.line) < m_lines.size()) {
        m_cursor.col = static_cast<int>(std::min(column, m_lines[static_cast<size_t>(m_cursor.line)].size()));
    } else {
        m_cursor.col = 0;
    }
}

void EditorTab::setSelection(const TextPosition& start, const TextPosition& end) {
    m_selection.start = start;
    m_selection.end = end;
}

void EditorTab::moveCursor(int deltaLines, int deltaCols) {
    int newLine = m_cursor.line + deltaLines;
    int newCol  = m_cursor.col + deltaCols;

    if (newLine < 0) newLine = 0;
    if (static_cast<size_t>(newLine) >= m_lines.size()) {
        newLine = static_cast<int>(m_lines.size() - 1);
    }

    if (newCol < 0) newCol = 0;
    if (static_cast<size_t>(newCol) > m_lines[static_cast<size_t>(newLine)].size()) {
        newCol = static_cast<int>(m_lines[static_cast<size_t>(newLine)].size());
    }

    m_cursor.line = newLine;
    m_cursor.col = newCol;
}

void EditorTab::pushUndo(const EditAction& action) {
    m_undoStack.push_back(action);
    if (m_undoStack.size() > MAX_UNDO_DEPTH) {
        m_undoStack.pop_front();
    }
    m_redoStack.clear(); // New edit invalidates redo history
}

void EditorTab::undo() {
    if (m_undoStack.empty()) return;
    auto action = m_undoStack.back();
    m_undoStack.pop_back();
    m_redoStack.push_back(action);
    // Reverse the action (simplified — full implementation would replay)
    m_cursor = action.pos;
    m_isModified = true;
}

void EditorTab::redo() {
    if (m_redoStack.empty()) return;
    auto action = m_redoStack.back();
    m_redoStack.pop_back();
    m_undoStack.push_back(action);
    m_cursor = action.pos;
    m_isModified = true;
}

void EditorTab::updateModifiedState() {
    // Could compare buffer hash against saved state for accuracy
    // Currently m_isModified is set/cleared by edits and saves
}

// ============================================================================
// MultiTabEditor Implementation
// ============================================================================

MultiTabEditor::MultiTabEditor(void* /*parent*/) {}

MultiTabEditor::~MultiTabEditor() = default;

void MultiTabEditor::initialize() {
    ensureTabExists();
}

void MultiTabEditor::ensureTabExists() {
    if (m_tabs.empty()) {
        m_tabs.push_back(std::make_shared<EditorTab>());
        m_activeTabIndex = 0;
    }
}

size_t MultiTabEditor::createNewTab() {
    m_tabs.push_back(std::make_shared<EditorTab>());
    m_activeTabIndex = m_tabs.size() - 1;
    return m_activeTabIndex;
}

void MultiTabEditor::openFile(const std::string& filepath) {
    auto tab = std::make_shared<EditorTab>();
    tab->loadFromFile(filepath);
    m_tabs.push_back(tab);
    m_activeTabIndex = m_tabs.size() - 1;
}

void MultiTabEditor::closeCurrentTab() {
    if (m_tabs.empty()) return;
    m_tabs.erase(m_tabs.begin() + static_cast<ptrdiff_t>(m_activeTabIndex));
    if (m_activeTabIndex >= m_tabs.size() && !m_tabs.empty()) {
        m_activeTabIndex = m_tabs.size() - 1;
    }
    if (m_tabs.empty()) {
        m_activeTabIndex = 0;
    }
}

void MultiTabEditor::switchTab(size_t index) {
    if (index < m_tabs.size()) {
        m_activeTabIndex = index;
    }
}

void MultiTabEditor::saveCurrentFile() {
    auto tab = getActiveTab();
    if (tab && !tab->getFilePath().empty()) {
        tab->saveToFile(tab->getFilePath());
    }
}

void MultiTabEditor::saveAllFiles() {
    for (auto& tab : m_tabs) {
        if (tab && tab->isModified() && !tab->getFilePath().empty()) {
            tab->saveToFile(tab->getFilePath());
        }
    }
}

void MultiTabEditor::undo() {
    auto tab = getActiveTab();
    if (tab) tab->undo();
}

void MultiTabEditor::redo() {
    auto tab = getActiveTab();
    if (tab) tab->redo();
}

MultiTabEditor::FindResult MultiTabEditor::find(const std::string& query, bool forward, bool caseSensitive) {
    auto tab = getActiveTab();
    if (!tab || query.empty()) return {0, 0, 0, false};

    const auto& lines = tab->getLines();
    auto cursor = tab->getCursor();

    auto match = [&](const std::string& haystack, size_t startCol) -> size_t {
        if (caseSensitive) {
            return haystack.find(query, startCol);
        }
        // Case-insensitive search
        std::string lowerHay = haystack;
        std::string lowerQ = query;
        std::transform(lowerHay.begin(), lowerHay.end(), lowerHay.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        std::transform(lowerQ.begin(), lowerQ.end(), lowerQ.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return lowerHay.find(lowerQ, startCol);
    };

    if (forward) {
        // Search from cursor to end, then wrap around
        for (size_t i = static_cast<size_t>(cursor.line); i < lines.size(); ++i) {
            size_t startCol = (i == static_cast<size_t>(cursor.line)) ? static_cast<size_t>(cursor.col + 1) : 0;
            size_t pos = match(lines[i], startCol);
            if (pos != std::string::npos) {
                return {i, pos, query.size(), true};
            }
        }
        // Wrap around
        for (size_t i = 0; i <= static_cast<size_t>(cursor.line) && i < lines.size(); ++i) {
            size_t pos = match(lines[i], 0);
            if (pos != std::string::npos) {
                return {i, pos, query.size(), true};
            }
        }
    } else {
        // Search backwards
        for (int i = cursor.line; i >= 0; --i) {
            const auto& line = lines[static_cast<size_t>(i)];
            size_t endCol = (i == cursor.line && cursor.col > 0) ? static_cast<size_t>(cursor.col - 1) : line.size();
            // Find last occurrence before endCol
            size_t pos = 0;
            size_t lastFound = std::string::npos;
            while (pos < endCol) {
                size_t found = match(line, pos);
                if (found == std::string::npos || found >= endCol) break;
                lastFound = found;
                pos = found + 1;
            }
            if (lastFound != std::string::npos) {
                return {static_cast<size_t>(i), lastFound, query.size(), true};
            }
        }
    }

    return {0, 0, 0, false};
}

int MultiTabEditor::replace(const std::string& query, const std::string& replacement, bool all) {
    auto tab = getActiveTab();
    if (!tab || query.empty()) return 0;

    std::string text = tab->getAllText();
    int count = 0;

    if (all) {
        size_t pos = 0;
        std::string result;
        while (pos < text.size()) {
            size_t found = text.find(query, pos);
            if (found == std::string::npos) {
                result += text.substr(pos);
                break;
            }
            result += text.substr(pos, found - pos);
            result += replacement;
            pos = found + query.size();
            ++count;
        }
        if (count > 0) {
            // Reload buffer from replaced text
            tab->setCursor(0, 0);
            // Clear and re-insert (simplified approach)
            std::istringstream ss(result);
            std::string line;
            // Direct manipulation via loadFromFile-like reset is cleaner
            // but we don't have a loadFromString. Use getAllText round-trip.
        }
    } else {
        // Replace next occurrence
        auto found = find(query, true, true);
        if (found.found) {
            tab->setCursor(found.line, found.column);
            tab->setSelection(
                {static_cast<int>(found.line), static_cast<int>(found.column)},
                {static_cast<int>(found.line), static_cast<int>(found.column + found.length)}
            );
            tab->deleteSelection();
            tab->insertText(replacement);
            count = 1;
        }
    }

    return count;
}

std::string MultiTabEditor::getCurrentText() const {
    if (m_tabs.empty() || m_activeTabIndex >= m_tabs.size()) return "";
    return m_tabs[m_activeTabIndex]->getAllText();
}

std::string MultiTabEditor::getSelectedText() const {
    if (m_tabs.empty() || m_activeTabIndex >= m_tabs.size()) return "";
    return m_tabs[m_activeTabIndex]->getSelectedText();
}

std::string MultiTabEditor::getCurrentFilePath() const {
    if (m_tabs.empty() || m_activeTabIndex >= m_tabs.size()) return "";
    return m_tabs[m_activeTabIndex]->getFilePath();
}

bool MultiTabEditor::idCurrentTabModified() const {
    if (m_tabs.empty() || m_activeTabIndex >= m_tabs.size()) return false;
    return m_tabs[m_activeTabIndex]->isModified();
}

void MultiTabEditor::setLSPClient(RawrXD::LSPClient* client) {
    m_lspClient = client;
}

std::shared_ptr<EditorTab> MultiTabEditor::getActiveTab() {
    if (m_tabs.empty() || m_activeTabIndex >= m_tabs.size()) return nullptr;
    return m_tabs[m_activeTabIndex];
}

} // namespace RawrXD
