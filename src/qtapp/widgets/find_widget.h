#pragma once
/**
 * \file find_widget.h
 * \brief In-file find/replace widget (like VS Code Ctrl+F)
 * \author RawrXD Team
 * \date 2025-12-05
 * 
 * Features:
 * - Find in current file with match counter
 * - Find/replace with regex support
 * - Case-sensitive and whole-word options
 * - Previous/Next navigation
 * - Replace current or replace all
 * - Search history (last 10 searches)
 * - Highlight all matches in editor
 */

#include <QWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QCheckBox>
#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPlainTextEdit>
#include <QRegularExpression>
#include <QStringList>

namespace RawrXD {

/**
 * \struct SearchResult
 * \brief Single search match result
 */
struct SearchResult {
    int line;           ///< Line number (0-based)
    int column;         ///< Column number (0-based)
    int length;         ///< Length of match
    QString text;       ///< Matched text
    
    SearchResult() : line(-1), column(-1), length(0) {}
    SearchResult(int l, int c, int len, const QString& t) 
        : line(l), column(c), length(len), text(t) {}
};

/**
 * \class FindWidget
 * \brief In-file find/replace widget
 * 
 * This widget appears at the top of the editor (like VS Code Ctrl+F)
 * and provides search/replace functionality within a single file.
 * 
 * Usage:
 * \code
 * auto* findWidget = new FindWidget(this);
 * findWidget->setEditor(myTextEdit);
 * findWidget->show();  // Shows the find bar
 * findWidget->focusSearchBox();
 * \endcode
 */
class FindWidget : public QWidget {
    Q_OBJECT
    
public:
    explicit FindWidget(QWidget* parent = nullptr);
    ~FindWidget() override;
    
    /**
     * \brief Set the editor to search in
     * \param editor QPlainTextEdit or QTextEdit to search
     */
    void setEditor(QPlainTextEdit* editor);
    
    /**
     * \brief Get current editor
     * \return Current editor widget
     */
    QPlainTextEdit* editor() const;
    
    /**
     * \brief Focus the search input box
     */
    void focusSearchBox();
    
    /**
     * \brief Show the find widget and populate with selected text
     */
    void showAndFocusWithSelection();
    
    /**
     * \brief Set search text programmatically
     * \param text Search term
     */
    void setSearchText(const QString& text);
    
    /**
     * \brief Get current search text
     * \return Search term
     */
    QString searchText() const;
    
    /**
     * \brief Set replace text
     * \param text Replacement text
     */
    void setReplaceText(const QString& text);
    
    /**
     * \brief Get replace text
     * \return Replacement text
     */
    QString replaceText() const;
    
    /**
     * \brief Set case-sensitive search
     * \param enabled If true, search is case-sensitive
     */
    void setCaseSensitive(bool enabled);
    
    /**
     * \brief Get case-sensitive state
     * \return true if case-sensitive
     */
    bool isCaseSensitive() const;
    
    /**
     * \brief Set whole-word search
     * \param enabled If true, only match whole words
     */
    void setWholeWord(bool enabled);
    
    /**
     * \brief Get whole-word state
     * \return true if whole-word matching
     */
    bool isWholeWord() const;
    
    /**
     * \brief Set regex search
     * \param enabled If true, search pattern is regex
     */
    void setUseRegex(bool enabled);
    
    /**
     * \brief Get regex state
     * \return true if using regex
     */
    bool isUseRegex() const;
    
    /**
     * \brief Get all matches in current editor
     * \return List of search results
     */
    QList<SearchResult> findAll();
    
    /**
     * \brief Get current match index (e.g., "3 of 15")
     * \return Current match number (1-based), or 0 if no matches
     */
    int currentMatchIndex() const;
    
    /**
     * \brief Get total match count
     * \return Number of matches found
     */
    int matchCount() const;
    
signals:
    /**
     * \brief Emitted when match count changes
     * \param current Current match index (1-based)
     * \param total Total matches
     */
    void matchCountChanged(int current, int total);
    
    /**
     * \brief Emitted when text is replaced
     * \param count Number of replacements made
     */
    void replaced(int count);
    
    /**
     * \brief Emitted when find widget is closed
     */
    void closed();
    
public slots:
    /**
     * \brief Find next occurrence
     */
    void findNext();
    
    /**
     * \brief Find previous occurrence
     */
    void findPrevious();
    
    /**
     * \brief Replace current match and find next
     */
    void replaceCurrent();
    
    /**
     * \brief Replace all matches
     */
    void replaceAll();
    
    /**
     * \brief Toggle replace mode (show/hide replace controls)
     */
    void toggleReplaceMode();
    
    /**
     * \brief Close the find widget
     */
    void close();
    
private slots:
    void onSearchTextChanged();
    void onReplaceTextChanged();
    void onCaseSensitiveToggled(bool checked);
    void onWholeWordToggled(bool checked);
    void onRegexToggled(bool checked);
    void onEditorCursorPositionChanged();
    
private:
    void setupUI();
    void updateMatchCount();
    void highlightAllMatches();
    void clearHighlights();
    QTextCursor findNextMatch(const QTextCursor& from, bool forward = true);
    QString buildRegexPattern() const;
    void addToSearchHistory(const QString& text);
    
    // UI Components
    QVBoxLayout* m_mainLayout;
    QHBoxLayout* m_searchLayout;
    QHBoxLayout* m_replaceLayout;
    
    QLineEdit* m_searchEdit;
    QLineEdit* m_replaceEdit;
    QPushButton* m_findPreviousButton;
    QPushButton* m_findNextButton;
    QPushButton* m_toggleReplaceButton;
    QPushButton* m_replaceButton;
    QPushButton* m_replaceAllButton;
    QPushButton* m_closeButton;
    QCheckBox* m_caseSensitiveCheck;
    QCheckBox* m_wholeWordCheck;
    QCheckBox* m_regexCheck;
    QLabel* m_matchCountLabel;
    
    // State
    QPlainTextEdit* m_editor;
    QList<SearchResult> m_matches;
    int m_currentMatchIndex;
    bool m_isReplaceMode;
    QStringList m_searchHistory;
    
    // For highlighting
    QList<QTextEdit::ExtraSelection> m_highlightSelections;
};

} // namespace RawrXD
