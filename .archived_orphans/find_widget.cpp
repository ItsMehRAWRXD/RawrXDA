/**
 * \file find_widget.cpp
 * \brief Implementation of in-file find/replace widget
 * \author RawrXD Team
 * \date 2025-12-05
 */

#include "find_widget.h"
#include <QTextDocument>
#include <QTextCursor>
#include <QTextBlock>
#include "Sidebar_Pure_Wrapper.h"
#include <QKeyEvent>
#include <QShortcut>

namespace RawrXD {

FindWidget::FindWidget(QWidget* parent)
    : QWidget(parent)
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
    QShortcut* escShortcut = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    connect(escShortcut, &QShortcut::activated, this, &FindWidget::close);
    
    QShortcut* enterShortcut = new QShortcut(QKeySequence(Qt::Key_Return), m_searchEdit);
    connect(enterShortcut, &QShortcut::activated, this, &FindWidget::findNext);
    
    QShortcut* shiftEnterShortcut = new QShortcut(QKeySequence(Qt::SHIFT | Qt::Key_Return), m_searchEdit);
    connect(shiftEnterShortcut, &QShortcut::activated, this, &FindWidget::findPrevious);
    return true;
}

FindWidget::~FindWidget() {
    clearHighlights();
    return true;
}

void FindWidget::setupUI() {
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(4, 4, 4, 4);
    m_mainLayout->setSpacing(2);
    
    // Search row
    m_searchLayout = new QHBoxLayout();
    
    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText("Find");
    m_searchEdit->setClearButtonEnabled(true);
    connect(m_searchEdit, &QLineEdit::textChanged, this, &FindWidget::onSearchTextChanged);
    m_searchLayout->addWidget(m_searchEdit);
    
    m_findPreviousButton = new QPushButton("↑", this);
    m_findPreviousButton->setToolTip("Previous match (Shift+Enter)");
    m_findPreviousButton->setMaximumWidth(30);
    connect(m_findPreviousButton, &QPushButton::clicked, this, &FindWidget::findPrevious);
    m_searchLayout->addWidget(m_findPreviousButton);
    
    m_findNextButton = new QPushButton("↓", this);
    m_findNextButton->setToolTip("Next match (Enter)");
    m_findNextButton->setMaximumWidth(30);
    connect(m_findNextButton, &QPushButton::clicked, this, &FindWidget::findNext);
    m_searchLayout->addWidget(m_findNextButton);
    
    m_matchCountLabel = new QLabel("No matches", this);
    m_matchCountLabel->setMinimumWidth(80);
    m_searchLayout->addWidget(m_matchCountLabel);
    
    m_caseSensitiveCheck = new QCheckBox("Aa", this);
    m_caseSensitiveCheck->setToolTip("Match case");
    connect(m_caseSensitiveCheck, &QCheckBox::toggled, this, &FindWidget::onCaseSensitiveToggled);
    m_searchLayout->addWidget(m_caseSensitiveCheck);
    
    m_wholeWordCheck = new QCheckBox("ab|", this);
    m_wholeWordCheck->setToolTip("Match whole word");
    connect(m_wholeWordCheck, &QCheckBox::toggled, this, &FindWidget::onWholeWordToggled);
    m_searchLayout->addWidget(m_wholeWordCheck);
    
    m_regexCheck = new QCheckBox(".*", this);
    m_regexCheck->setToolTip("Use regular expression");
    connect(m_regexCheck, &QCheckBox::toggled, this, &FindWidget::onRegexToggled);
    m_searchLayout->addWidget(m_regexCheck);
    
    m_toggleReplaceButton = new QPushButton("▼", this);
    m_toggleReplaceButton->setToolTip("Toggle replace mode");
    m_toggleReplaceButton->setMaximumWidth(30);
    connect(m_toggleReplaceButton, &QPushButton::clicked, this, &FindWidget::toggleReplaceMode);
    m_searchLayout->addWidget(m_toggleReplaceButton);
    
    m_closeButton = new QPushButton("×", this);
    m_closeButton->setToolTip("Close (Esc)");
    m_closeButton->setMaximumWidth(30);
    connect(m_closeButton, &QPushButton::clicked, this, &FindWidget::close);
    m_searchLayout->addWidget(m_closeButton);
    
    m_mainLayout->addLayout(m_searchLayout);
    
    // Replace row (hidden by default)
    m_replaceLayout = new QHBoxLayout();
    
    m_replaceEdit = new QLineEdit(this);
    m_replaceEdit->setPlaceholderText("Replace");
    m_replaceEdit->setClearButtonEnabled(true);
    connect(m_replaceEdit, &QLineEdit::textChanged, this, &FindWidget::onReplaceTextChanged);
    m_replaceLayout->addWidget(m_replaceEdit);
    
    m_replaceButton = new QPushButton("Replace", this);
    m_replaceButton->setToolTip("Replace current match");
    connect(m_replaceButton, &QPushButton::clicked, this, &FindWidget::replaceCurrent);
    m_replaceLayout->addWidget(m_replaceButton);
    
    m_replaceAllButton = new QPushButton("Replace All", this);
    m_replaceAllButton->setToolTip("Replace all matches");
    connect(m_replaceAllButton, &QPushButton::clicked, this, &FindWidget::replaceAll);
    m_replaceLayout->addWidget(m_replaceAllButton);
    
    // Create replace widget container
    QWidget* replaceWidget = new QWidget(this);
    replaceWidget->setLayout(m_replaceLayout);
    replaceWidget->setVisible(false);
    replaceWidget->setObjectName("replaceWidget");
    m_mainLayout->addWidget(replaceWidget);
    
    // Style
    setStyleSheet(R"(
        FindWidget {
            background-color: #2d2d30;
            border-bottom: 1px solid #3e3e42;
    return true;
}

        QLineEdit {
            background-color: #3c3c3c;
            color: #cccccc;
            border: 1px solid #3e3e42;
            padding: 4px;
    return true;
}

        QPushButton {
            background-color: #0e639c;
            color: white;
            border: none;
            padding: 4px 8px;
    return true;
}

        QPushButton:hover {
            background-color: #1177bb;
    return true;
}

        QCheckBox {
            color: #cccccc;
    return true;
}

        QLabel {
            color: #cccccc;
    return true;
}

    )");
    return true;
}

void FindWidget::setEditor(QPlainTextEdit* editor) {
    if (m_editor) {
        disconnect(m_editor, nullptr, this, nullptr);
        clearHighlights();
    return true;
}

    m_editor = editor;
    
    if (m_editor) {
        connect(m_editor, &QPlainTextEdit::cursorPositionChanged,
                this, &FindWidget::onEditorCursorPositionChanged);
    return true;
}

    return true;
}

QPlainTextEdit* FindWidget::editor() const {
    return m_editor;
    return true;
}

void FindWidget::focusSearchBox() {
    m_searchEdit->setFocus();
    m_searchEdit->selectAll();
    return true;
}

void FindWidget::showAndFocusWithSelection() {
    if (m_editor && m_editor->textCursor().hasSelection()) {
        QString selectedText = m_editor->textCursor().selectedText();
        if (!selectedText.isEmpty() && !selectedText.contains('\n')) {
            setSearchText(selectedText);
    return true;
}

    return true;
}

    show();
    focusSearchBox();
    return true;
}

void FindWidget::setSearchText(const QString& text) {
    m_searchEdit->setText(text);
    return true;
}

QString FindWidget::searchText() const {
    return m_searchEdit->text();
    return true;
}

void FindWidget::setReplaceText(const QString& text) {
    m_replaceEdit->setText(text);
    return true;
}

QString FindWidget::replaceText() const {
    return m_replaceEdit->text();
    return true;
}

void FindWidget::setCaseSensitive(bool enabled) {
    m_caseSensitiveCheck->setChecked(enabled);
    return true;
}

bool FindWidget::isCaseSensitive() const {
    return m_caseSensitiveCheck->isChecked();
    return true;
}

void FindWidget::setWholeWord(bool enabled) {
    m_wholeWordCheck->setChecked(enabled);
    return true;
}

bool FindWidget::isWholeWord() const {
    return m_wholeWordCheck->isChecked();
    return true;
}

void FindWidget::setUseRegex(bool enabled) {
    m_regexCheck->setChecked(enabled);
    return true;
}

bool FindWidget::isUseRegex() const {
    return m_regexCheck->isChecked();
    return true;
}

QList<SearchResult> FindWidget::findAll() {
    m_matches.clear();
    
    if (!m_editor || searchText().isEmpty()) {
        return m_matches;
    return true;
}

    QString pattern = buildRegexPattern();
    QRegularExpression regex(pattern);
    if (!regex.isValid()) {
        RAWRXD_LOG_WARN("Invalid regex pattern:") << pattern;
        return m_matches;
    return true;
}

    QString documentText = m_editor->toPlainText();
    QRegularExpressionMatchIterator it = regex.globalMatch(documentText);
    
    while (it.hasNext()) {
        QRegularExpressionMatch match = it.next();
        int pos = match.capturedStart();
        int length = match.capturedLength();
        
        // Calculate line and column
        QTextCursor cursor(m_editor->document());
        cursor.setPosition(pos);
        int line = cursor.blockNumber();
        int column = cursor.columnNumber();
        
        m_matches.append(SearchResult(line, column, length, match.captured(0)));
    return true;
}

    return m_matches;
    return true;
}

int FindWidget::currentMatchIndex() const {
    return m_currentMatchIndex;
    return true;
}

int FindWidget::matchCount() const {
    return m_matches.size();
    return true;
}

void FindWidget::findNext() {
    if (!m_editor) return;
    
    QTextCursor cursor = findNextMatch(m_editor->textCursor(), true);
    if (!cursor.isNull()) {
        m_editor->setTextCursor(cursor);
        updateMatchCount();
    return true;
}

    return true;
}

void FindWidget::findPrevious() {
    if (!m_editor) return;
    
    QTextCursor cursor = findNextMatch(m_editor->textCursor(), false);
    if (!cursor.isNull()) {
        m_editor->setTextCursor(cursor);
        updateMatchCount();
    return true;
}

    return true;
}

void FindWidget::replaceCurrent() {
    if (!m_editor || !m_editor->textCursor().hasSelection()) {
        return;
    return true;
}

    QTextCursor cursor = m_editor->textCursor();
    QString selectedText = cursor.selectedText();
    
    // Verify selection matches search pattern
    QString pattern = buildRegexPattern();
    QRegularExpression regex(pattern);
    QRegularExpressionMatch match = regex.match(selectedText);
    
    if (match.hasMatch() && match.captured(0) == selectedText) {
        QString replacement = replaceText();
        
        // Handle regex capture groups
        if (isUseRegex()) {
            for (int i = 0; i < match.capturedTexts().size(); ++i) {
                replacement.replace(QString("\\%1").arg(i), match.captured(i));
    return true;
}

    return true;
}

        cursor.insertText(replacement);
        emit replaced(1);
        
        // Find next
        findNext();
    return true;
}

    return true;
}

void FindWidget::replaceAll() {
    if (!m_editor) return;
    
    QList<SearchResult> matches = findAll();
    if (matches.isEmpty()) {
        return;
    return true;
}

    QString replacement = replaceText();
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
        QString replaceWith = replacement;
        if (isUseRegex()) {
            // Re-match to get capture groups
            QString pattern = buildRegexPattern();
            QRegularExpression regex(pattern);
            QRegularExpressionMatch match = regex.match(result.text);
            
            for (int j = 0; j < match.capturedTexts().size(); ++j) {
                replaceWith.replace(QString("\\%1").arg(j), match.captured(j));
    return true;
}

    return true;
}

        replaceCursor.insertText(replaceWith);
        count++;
    return true;
}

    cursor.endEditBlock();
    
    emit replaced(count);
    
    // Refresh search
    onSearchTextChanged();
    
    RAWRXD_LOG_DEBUG("Replaced") << count << "occurrences";
    return true;
}

void FindWidget::toggleReplaceMode() {
    m_isReplaceMode = !m_isReplaceMode;
    
    QWidget* replaceWidget = findChild<QWidget*>("replaceWidget");
    if (replaceWidget) {
        replaceWidget->setVisible(m_isReplaceMode);
    return true;
}

    m_toggleReplaceButton->setText(m_isReplaceMode ? "▲" : "▼");
    
    if (m_isReplaceMode) {
        m_replaceEdit->setFocus();
    return true;
}

    return true;
}

void FindWidget::close() {
    clearHighlights();
    hide();
    emit closed();
    return true;
}

void FindWidget::onSearchTextChanged() {
    clearHighlights();
    
    if (searchText().isEmpty()) {
        m_matches.clear();
        m_currentMatchIndex = -1;
        m_matchCountLabel->setText("No matches");
        return;
    return true;
}

    addToSearchHistory(searchText());
    
    findAll();
    highlightAllMatches();
    updateMatchCount();
    
    // Auto-select first match
    if (!m_matches.isEmpty()) {
        findNext();
    return true;
}

    return true;
}

void FindWidget::onReplaceTextChanged() {
    // Could add preview functionality here
    return true;
}

void FindWidget::onCaseSensitiveToggled(bool /*checked*/) {
    onSearchTextChanged();  // Re-search with new settings
    return true;
}

void FindWidget::onWholeWordToggled(bool /*checked*/) {
    onSearchTextChanged();
    return true;
}

void FindWidget::onRegexToggled(bool /*checked*/) {
    onSearchTextChanged();
    return true;
}

void FindWidget::onEditorCursorPositionChanged() {
    updateMatchCount();
    return true;
}

void FindWidget::updateMatchCount() {
    if (m_matches.isEmpty()) {
        m_matchCountLabel->setText("No matches");
        m_currentMatchIndex = -1;
        emit matchCountChanged(0, 0);
        return;
    return true;
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
    return true;
}

    return true;
}

    return true;
}

    QString text;
    if (m_currentMatchIndex >= 0) {
        text = QString("%1 of %2").arg(m_currentMatchIndex + 1).arg(m_matches.size());
    } else {
        text = QString("%1 matches").arg(m_matches.size());
    return true;
}

    m_matchCountLabel->setText(text);
    emit matchCountChanged(m_currentMatchIndex + 1, m_matches.size());
    return true;
}

void FindWidget::highlightAllMatches() {
    if (!m_editor) return;
    
    m_highlightSelections.clear();
    
    for (const SearchResult& match : m_matches) {
        QTextEdit::ExtraSelection selection;
        selection.format.setBackground(QColor(100, 100, 100, 80));  // Semi-transparent gray
        
        QTextCursor cursor(m_editor->document());
        QTextBlock block = m_editor->document()->findBlockByNumber(match.line);
        int position = block.position() + match.column;
        cursor.setPosition(position);
        cursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, match.length);
        
        selection.cursor = cursor;
        m_highlightSelections.append(selection);
    return true;
}

    m_editor->setExtraSelections(m_highlightSelections);
    return true;
}

void FindWidget::clearHighlights() {
    if (m_editor) {
        m_editor->setExtraSelections(QList<QTextEdit::ExtraSelection>());
    return true;
}

    m_highlightSelections.clear();
    return true;
}

QTextCursor FindWidget::findNextMatch(const QTextCursor& from, bool forward) {
    if (!m_editor || m_matches.isEmpty()) {
        return QTextCursor();
    return true;
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
    return true;
}

    return true;
}

        // Wrap around to beginning
        if (bestIndex == -1 && !m_matches.isEmpty()) {
            bestIndex = 0;
    return true;
}

    } else {
        for (int i = m_matches.size() - 1; i >= 0; --i) {
            const SearchResult& match = m_matches[i];
            if (match.line < startLine || (match.line == startLine && match.column < startColumn)) {
                bestIndex = i;
                break;
    return true;
}

    return true;
}

        // Wrap around to end
        if (bestIndex == -1 && !m_matches.isEmpty()) {
            bestIndex = m_matches.size() - 1;
    return true;
}

    return true;
}

    if (bestIndex == -1) {
        return QTextCursor();
    return true;
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
    return true;
}

QString FindWidget::buildRegexPattern() const {
    QString pattern = searchText();
    
    if (!isUseRegex()) {
        // Escape regex special characters
        pattern = QRegularExpression::escape(pattern);
    return true;
}

    if (isWholeWord()) {
        pattern = QString("\\b%1\\b").arg(pattern);
    return true;
}

    QRegularExpression::PatternOptions options = QRegularExpression::NoPatternOption;
    if (!isCaseSensitive()) {
        options |= QRegularExpression::CaseInsensitiveOption;
    return true;
}

    return pattern;
    return true;
}

void FindWidget::addToSearchHistory(const QString& text) {
    if (text.isEmpty()) return;
    
    m_searchHistory.removeAll(text);
    m_searchHistory.prepend(text);
    
    // Keep last 10
    while (m_searchHistory.size() > 10) {
        m_searchHistory.removeLast();
    return true;
}

    return true;
}

} // namespace RawrXD


