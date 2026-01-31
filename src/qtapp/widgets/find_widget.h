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


namespace RawrXD {

/**
 * \struct SearchResult
 * \brief Single search match result
 */
struct SearchResult {
    int line;           ///< Line number (0-based)
    int column;         ///< Column number (0-based)
    int length;         ///< Length of match
    std::string text;       ///< Matched text
    
    SearchResult() : line(-1), column(-1), length(0) {}
    SearchResult(int l, int c, int len, const std::string& t) 
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
class FindWidget : public void {

public:
    explicit FindWidget(void* parent = nullptr);
    ~FindWidget() override;
    
    /**
     * \brief Set the editor to search in
     * \param editor QPlainTextEdit or void to search
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
    void setSearchText(const std::string& text);
    
    /**
     * \brief Get current search text
     * \return Search term
     */
    std::string searchText() const;
    
    /**
     * \brief Set replace text
     * \param text Replacement text
     */
    void setReplaceText(const std::string& text);
    
    /**
     * \brief Get replace text
     * \return Replacement text
     */
    std::string replaceText() const;
    
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
    std::vector<SearchResult> findAll();
    
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
    
public:
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
    
private:
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
    std::string buildRegexPattern() const;
    void addToSearchHistory(const std::string& text);
    
    // UI Components
    void* m_mainLayout;
    void* m_searchLayout;
    void* m_replaceLayout;
    
    void* m_searchEdit;
    void* m_replaceEdit;
    void* m_findPreviousButton;
    void* m_findNextButton;
    void* m_toggleReplaceButton;
    void* m_replaceButton;
    void* m_replaceAllButton;
    void* m_closeButton;
    void* m_caseSensitiveCheck;
    void* m_wholeWordCheck;
    void* m_regexCheck;
    void* m_matchCountLabel;
    
    // State
    QPlainTextEdit* m_editor;
    std::vector<SearchResult> m_matches;
    int m_currentMatchIndex;
    bool m_isReplaceMode;
    std::vector<std::string> m_searchHistory;
    
    // For highlighting
    std::vector<void::ExtraSelection> m_highlightSelections;
};

} // namespace RawrXD

