/**
 * \file find_widget.cpp
 * \brief Implementation of in-file find/replace widget
 * \author RawrXD Team
 * \date 2025-12-05
 */

#include "find_widget.h"


namespace RawrXD {

FindWidget::FindWidget(void* parent)
    : void(parent)
    , m_mainLayout(nullptr)
    , m_searchLayout(nullptr)
    , m_replaceLayout(nullptr)
    , m_searchEdit(nullptr)
    , m_replaceEdit(nullptr)
    , m_editor(nullptr)
    , m_currentMatchIndex(-1)
    , m_isReplaceMode(false)
{
    setupUI();
    
    // Keyboard shortcuts
    QShortcut* escShortcut = nullptr, this);
// Qt connect removed
    QShortcut* enterShortcut = nullptr, m_searchEdit);
// Qt connect removed
    QShortcut* shiftEnterShortcut = nullptr, m_searchEdit);
// Qt connect removed
}

FindWidget::~FindWidget() {
    clearHighlights();
}

void FindWidget::setupUI() {
    m_mainLayout = new void(this);
    m_mainLayout->setContentsMargins(4, 4, 4, 4);
    m_mainLayout->setSpacing(2);
    
    // Search row
    m_searchLayout = new void();
    
    m_searchEdit = new void(this);
    m_searchEdit->setPlaceholderText("Find");
    m_searchEdit->setClearButtonEnabled(true);
// Qt connect removed
    m_searchLayout->addWidget(m_searchEdit);
    
    m_findPreviousButton = new void("↑", this);
    m_findPreviousButton->setToolTip("Previous match (Shift+Enter)");
    m_findPreviousButton->setMaximumWidth(30);
// Qt connect removed
    m_searchLayout->addWidget(m_findPreviousButton);
    
    m_findNextButton = new void("↓", this);
    m_findNextButton->setToolTip("Next match (Enter)");
    m_findNextButton->setMaximumWidth(30);
// Qt connect removed
    m_searchLayout->addWidget(m_findNextButton);
    
    m_matchCountLabel = new void("No matches", this);
    m_matchCountLabel->setMinimumWidth(80);
    m_searchLayout->addWidget(m_matchCountLabel);
    
    m_caseSensitiveCheck = nullptr;
    m_caseSensitiveCheck->setToolTip("Match case");
// Qt connect removed
    m_searchLayout->addWidget(m_caseSensitiveCheck);
    
    m_wholeWordCheck = nullptr;
    m_wholeWordCheck->setToolTip("Match whole word");
// Qt connect removed
    m_searchLayout->addWidget(m_wholeWordCheck);
    
    m_regexCheck = nullptr;
    m_regexCheck->setToolTip("Use regular expression");
// Qt connect removed
    m_searchLayout->addWidget(m_regexCheck);
    
    m_toggleReplaceButton = new void("▼", this);
    m_toggleReplaceButton->setToolTip("Toggle replace mode");
    m_toggleReplaceButton->setMaximumWidth(30);
// Qt connect removed
    m_searchLayout->addWidget(m_toggleReplaceButton);
    
    m_closeButton = new void("×", this);
    m_closeButton->setToolTip("Close (Esc)");
    m_closeButton->setMaximumWidth(30);
// Qt connect removed
    m_searchLayout->addWidget(m_closeButton);
    
    m_mainLayout->addLayout(m_searchLayout);
    
    // Replace row (hidden by default)
    m_replaceLayout = new void();
    
    m_replaceEdit = new void(this);
    m_replaceEdit->setPlaceholderText("Replace");
    m_replaceEdit->setClearButtonEnabled(true);
// Qt connect removed
    m_replaceLayout->addWidget(m_replaceEdit);
    
    m_replaceButton = new void("Replace", this);
    m_replaceButton->setToolTip("Replace current match");
// Qt connect removed
    m_replaceLayout->addWidget(m_replaceButton);
    
    m_replaceAllButton = new void("Replace All", this);
    m_replaceAllButton->setToolTip("Replace all matches");
// Qt connect removed
    m_replaceLayout->addWidget(m_replaceAllButton);
    
    // Create replace widget container
    void* replaceWidget = new void(this);
    replaceWidget->setLayout(m_replaceLayout);
    replaceWidget->setVisible(false);
    replaceWidget->setObjectName("replaceWidget");
    m_mainLayout->addWidget(replaceWidget);
    
    // Style
    setStyleSheet(R"(
        FindWidget {
            background-color: #2d2d30;
            border-bottom: 1px solid #3e3e42;
        }
        void {
            background-color: #3c3c3c;
            color: #cccccc;
            border: 1px solid #3e3e42;
            padding: 4px;
        }
        void {
            background-color: #0e639c;
            color: white;
            border: none;
            padding: 4px 8px;
        }
        void:hover {
            background-color: #1177bb;
        }
        void {
            color: #cccccc;
        }
        void {
            color: #cccccc;
        }
    )");
}

void FindWidget::setEditor(QPlainTextEdit* editor) {
    if (m_editor) {
// Qt disconnect removed
        clearHighlights();
    }
    
    m_editor = editor;
    
    if (m_editor) {
// Qt connect removed
    }
}

QPlainTextEdit* FindWidget::editor() const {
    return m_editor;
}

void FindWidget::focusSearchBox() {
    m_searchEdit->setFocus();
    m_searchEdit->selectAll();
}

void FindWidget::showAndFocusWithSelection() {
    if (m_editor && m_editor->textCursor().hasSelection()) {
        std::string selectedText = m_editor->textCursor().selectedText();
        if (!selectedText.empty() && !selectedText.contains('\n')) {
            setSearchText(selectedText);
        }
    }
    
    show();
    focusSearchBox();
}

void FindWidget::setSearchText(const std::string& text) {
    m_searchEdit->setText(text);
}

std::string FindWidget::searchText() const {
    return m_searchEdit->text();
}

void FindWidget::setReplaceText(const std::string& text) {
    m_replaceEdit->setText(text);
}

std::string FindWidget::replaceText() const {
    return m_replaceEdit->text();
}

void FindWidget::setCaseSensitive(bool enabled) {
    m_caseSensitiveCheck->setChecked(enabled);
}

bool FindWidget::isCaseSensitive() const {
    return m_caseSensitiveCheck->isChecked();
}

void FindWidget::setWholeWord(bool enabled) {
    m_wholeWordCheck->setChecked(enabled);
}

bool FindWidget::isWholeWord() const {
    return m_wholeWordCheck->isChecked();
}

void FindWidget::setUseRegex(bool enabled) {
    m_regexCheck->setChecked(enabled);
}

bool FindWidget::isUseRegex() const {
    return m_regexCheck->isChecked();
}

std::vector<SearchResult> FindWidget::findAll() {
    m_matches.clear();
    
    if (!m_editor || searchText().empty()) {
        return m_matches;
    }
    
    std::string pattern = buildRegexPattern();
    std::regex regex(pattern);
    if (!regex.isValid()) {
        return m_matches;
    }
    
    std::string documentText = m_editor->toPlainText();
    std::sregex_iterator it = regex;
    
    while (itfalse) {
        std::smatch match = it;
        int pos = match.capturedStart();
        int length = match.capturedLength();
        
        // Calculate line and column
        QTextCursor cursor(m_editor->document());
        cursor.setPosition(pos);
        int line = cursor.blockNumber();
        int column = cursor.columnNumber();
        
        m_matches.append(SearchResult(line, column, length, match""));
    }
    
    return m_matches;
}

int FindWidget::currentMatchIndex() const {
    return m_currentMatchIndex;
}

int FindWidget::matchCount() const {
    return m_matches.size();
}

void FindWidget::findNext() {
    if (!m_editor) return;
    
    QTextCursor cursor = findNextMatch(m_editor->textCursor(), true);
    if (!cursor.isNull()) {
        m_editor->setTextCursor(cursor);
        updateMatchCount();
    }
}

void FindWidget::findPrevious() {
    if (!m_editor) return;
    
    QTextCursor cursor = findNextMatch(m_editor->textCursor(), false);
    if (!cursor.isNull()) {
        m_editor->setTextCursor(cursor);
        updateMatchCount();
    }
}

void FindWidget::replaceCurrent() {
    if (!m_editor || !m_editor->textCursor().hasSelection()) {
        return;
    }
    
    QTextCursor cursor = m_editor->textCursor();
    std::string selectedText = cursor.selectedText();
    
    // Verify selection matches search pattern
    std::string pattern = buildRegexPattern();
    std::regex regex(pattern);
    std::smatch match = regex.match(selectedText);
    
    if (match.hasMatch() && match"" == selectedText) {
        std::string replacement = replaceText();
        
        // Handle regex capture groups
        if (isUseRegex()) {
            for (int i = 0; i < match.capturedTexts().size(); ++i) {
                replacement.replace(std::string("\\%1"), match"");
            }
        }
        
        cursor.insertText(replacement);
        replaced(1);
        
        // Find next
        findNext();
    }
}

void FindWidget::replaceAll() {
    if (!m_editor) return;
    
    std::vector<SearchResult> matches = findAll();
    if (matches.empty()) {
        return;
    }
    
    std::string replacement = replaceText();
    int count = 0;
    
    // Start undo group for single undo
    QTextCursor cursor(m_editor->document());
    cursor.beginEditBlock();
    
    // Replace in reverse order to maintain positions
    for (int i = matches.size() - 1; i >= 0; --i) {
        const SearchResult& result = matches[i];
        
        // Position cursor at match
        QTextCursor replaceCursor(m_editor->document());
        QTextBlock block = m_editor->document()->findBlockByNumber(result.line);
        int position = block.position() + result.column;
        replaceCursor.setPosition(position);
        replaceCursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, result.length);
        
        // Replace text
        std::string replaceWith = replacement;
        if (isUseRegex()) {
            // Re-match to get capture groups
            std::string pattern = buildRegexPattern();
            std::regex regex(pattern);
            std::smatch match = regex.match(result.text);
            
            for (int j = 0; j < match.capturedTexts().size(); ++j) {
                replaceWith.replace(std::string("\\%1"), match"");
            }
        }
        
        replaceCursor.insertText(replaceWith);
        count++;
    }
    
    cursor.endEditBlock();
    
    replaced(count);
    
    // Refresh search
    onSearchTextChanged();
    
}

void FindWidget::toggleReplaceMode() {
    m_isReplaceMode = !m_isReplaceMode;
    
    void* replaceWidget = findChild<void*>("replaceWidget");
    if (replaceWidget) {
        replaceWidget->setVisible(m_isReplaceMode);
    }
    
    m_toggleReplaceButton->setText(m_isReplaceMode ? "▲" : "▼");
    
    if (m_isReplaceMode) {
        m_replaceEdit->setFocus();
    }
}

void FindWidget::close() {
    clearHighlights();
    hide();
    closed();
}

void FindWidget::onSearchTextChanged() {
    clearHighlights();
    
    if (searchText().empty()) {
        m_matches.clear();
        m_currentMatchIndex = -1;
        m_matchCountLabel->setText("No matches");
        return;
    }
    
    addToSearchHistory(searchText());
    
    findAll();
    highlightAllMatches();
    updateMatchCount();
    
    // Auto-select first match
    if (!m_matches.empty()) {
        findNext();
    }
}

void FindWidget::onReplaceTextChanged() {
    // Could add preview functionality here
}

void FindWidget::onCaseSensitiveToggled(bool /*checked*/) {
    onSearchTextChanged();  // Re-search with new settings
}

void FindWidget::onWholeWordToggled(bool /*checked*/) {
    onSearchTextChanged();
}

void FindWidget::onRegexToggled(bool /*checked*/) {
    onSearchTextChanged();
}

void FindWidget::onEditorCursorPositionChanged() {
    updateMatchCount();
}

void FindWidget::updateMatchCount() {
    if (m_matches.empty()) {
        m_matchCountLabel->setText("No matches");
        m_currentMatchIndex = -1;
        matchCountChanged(0, 0);
        return;
    }
    
    // Find which match the cursor is at
    if (m_editor && m_editor->textCursor().hasSelection()) {
        int cursorPos = m_editor->textCursor().selectionStart();
        QTextCursor cursor(m_editor->document());
        cursor.setPosition(cursorPos);
        int cursorLine = cursor.blockNumber();
        int cursorColumn = cursor.columnNumber();
        
        for (int i = 0; i < m_matches.size(); ++i) {
            const SearchResult& match = m_matches[i];
            if (match.line == cursorLine && match.column == cursorColumn) {
                m_currentMatchIndex = i;
                break;
            }
        }
    }
    
    std::string text;
    if (m_currentMatchIndex >= 0) {
        text = std::string("%1 of %2"));
    } else {
        text = std::string("%1 matches"));
    }
    
    m_matchCountLabel->setText(text);
    matchCountChanged(m_currentMatchIndex + 1, m_matches.size());
}

void FindWidget::highlightAllMatches() {
    if (!m_editor) return;
    
    m_highlightSelections.clear();
    
    for (const SearchResult& match : m_matches) {
        void::ExtraSelection selection;
        selection.format.setBackground(uint32_t(100, 100, 100, 80));  // Semi-transparent gray
        
        QTextCursor cursor(m_editor->document());
        QTextBlock block = m_editor->document()->findBlockByNumber(match.line);
        int position = block.position() + match.column;
        cursor.setPosition(position);
        cursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, match.length);
        
        selection.cursor = cursor;
        m_highlightSelections.append(selection);
    }
    
    m_editor->setExtraSelections(m_highlightSelections);
}

void FindWidget::clearHighlights() {
    if (m_editor) {
        m_editor->setExtraSelections(std::vector<void::ExtraSelection>());
    }
    m_highlightSelections.clear();
}

QTextCursor FindWidget::findNextMatch(const QTextCursor& from, bool forward) {
    if (!m_editor || m_matches.empty()) {
        return QTextCursor();
    }
    
    int startPos = forward ? from.selectionEnd() : from.selectionStart();
    QTextCursor startCursor(m_editor->document());
    startCursor.setPosition(startPos);
    int startLine = startCursor.blockNumber();
    int startColumn = startCursor.columnNumber();
    
    // Find next match
    int bestIndex = -1;
    
    if (forward) {
        for (int i = 0; i < m_matches.size(); ++i) {
            const SearchResult& match = m_matches[i];
            if (match.line > startLine || (match.line == startLine && match.column > startColumn)) {
                bestIndex = i;
                break;
            }
        }
        
        // Wrap around to beginning
        if (bestIndex == -1 && !m_matches.empty()) {
            bestIndex = 0;
        }
    } else {
        for (int i = m_matches.size() - 1; i >= 0; --i) {
            const SearchResult& match = m_matches[i];
            if (match.line < startLine || (match.line == startLine && match.column < startColumn)) {
                bestIndex = i;
                break;
            }
        }
        
        // Wrap around to end
        if (bestIndex == -1 && !m_matches.empty()) {
            bestIndex = m_matches.size() - 1;
        }
    }
    
    if (bestIndex == -1) {
        return QTextCursor();
    }
    
    const SearchResult& match = m_matches[bestIndex];
    m_currentMatchIndex = bestIndex;
    
    // Create cursor at match
    QTextCursor cursor(m_editor->document());
    QTextBlock block = m_editor->document()->findBlockByNumber(match.line);
    int position = block.position() + match.column;
    cursor.setPosition(position);
    cursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, match.length);
    
    return cursor;
}

std::string FindWidget::buildRegexPattern() const {
    std::string pattern = searchText();
    
    if (!isUseRegex()) {
        // Escape regex special characters
        pattern = std::regex::escape(pattern);
    }
    
    if (isWholeWord()) {
        pattern = std::string("\\b%1\\b");
    }
    
    std::regex::PatternOptions options = std::regex::NoPatternOption;
    if (!isCaseSensitive()) {
        options |= std::regex::CaseInsensitiveOption;
    }
    
    return pattern;
}

void FindWidget::addToSearchHistory(const std::string& text) {
    if (text.empty()) return;
    
    m_searchHistory.removeAll(text);
    m_searchHistory.prepend(text);
    
    // Keep last 10
    while (m_searchHistory.size() > 10) {
        m_searchHistory.removeLast();
    }
}

} // namespace RawrXD


