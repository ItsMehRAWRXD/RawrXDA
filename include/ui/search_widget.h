#pragma once
/**
 * @file search_widget.h
 * @brief Find and replace widget
 * Batch 4 - Item 60: Search widget
 */

#include <string>
#include <vector>
#include <optional>
#include <functional>
#include <windows.h>

namespace RawrXD::UI {

enum class SearchMode {
    Find,
    Replace,
    FindInFiles,
    ReplaceInFiles
};

enum class SearchScope {
    CurrentDocument,
    Selection,
    OpenDocuments,
    Workspace,
    Custom
};

struct SearchOptions {
    bool caseSensitive{false};
    bool wholeWord{false};
    bool regex{false};
    bool preserveCase{false};
};

struct SearchResult {
    std::string filePath;
    int line;
    int startColumn;
    int endColumn;
    std::string lineText;
    std::string matchText;
};

struct SearchMatch {
    int start;
    int end;
    std::string text;
};

class SearchWidget {
public:
    SearchWidget();
    ~SearchWidget();

    // Initialization
    bool initialize(HWND parent);
    void shutdown();

    // Window management
    HWND getHandle() const;
    void show();
    void hide();
    void toggle();
    bool isVisible() const;
    void resize(int width, int height);

    // Mode
    void setMode(SearchMode mode);
    SearchMode getMode() const;
    void setScope(SearchScope scope);
    SearchScope getScope() const;

    // Search
    void setSearchText(const std::string& text);
    std::string getSearchText() const;
    void setReplaceText(const std::string& text);
    std::string getReplaceText() const;

    // Options
    void setOptions(const SearchOptions& options);
    SearchOptions getOptions() const;
    void toggleCaseSensitive();
    void toggleWholeWord();
    void toggleRegex();
    void togglePreserveCase();

    // Actions
    void findNext();
    void findPrevious();
    void findAll();
    void replace();
    void replaceAll();
    void replaceNext();

    // Results
    std::vector<SearchResult> getResults() const;
    int getResultCount() const;
    int getCurrentResultIndex() const;
    void clearResults();

    // History
    void addToHistory(const std::string& text);
    std::vector<std::string> getSearchHistory() const;
    std::vector<std::string> getReplaceHistory() const;
    void clearHistory();

    // Selection
    void setSelection(const std::string& text);
    std::string getSelectedText() const;

    // Events
    using SearchCallback = std::function<void(const std::string& query, const SearchOptions& options)>;
    using ReplaceCallback = std::function<void(const std::string& findText, const std::string& replaceText)>;
    using ResultCallback = std::function<void(const SearchResult& result)>;
    void onSearch(SearchCallback callback);
    void onReplace(ReplaceCallback callback);
    void onResultSelected(ResultCallback callback);

    // Configuration
    void setShowOptions(bool show);
    void setShowHistory(bool show);
    void setAutoSearch(bool enabled);
    void setSearchDelay(int milliseconds);

private:
    HWND m_hwnd{nullptr};
    HWND m_parent{nullptr};
    HWND m_searchBox{nullptr};
    HWND m_replaceBox{nullptr};
    HWND m_optionsPanel{nullptr};
    bool m_visible{false};
    SearchMode m_mode{SearchMode::Find};
    SearchScope m_scope{SearchScope::CurrentDocument};
    SearchOptions m_options;
    std::vector<SearchResult> m_results;
    int m_currentResultIndex{-1};
    std::vector<std::string> m_searchHistory;
    std::vector<std::string> m_replaceHistory;
    bool m_autoSearch{true};
    int m_searchDelay{300};

    SearchCallback m_searchCallback;
    ReplaceCallback m_replaceCallback;
    ResultCallback m_resultCallback;

    void createControls();
    void layout();
    void updateVisibility();
    void performSearch();
    void performReplace();
    std::vector<SearchMatch> findMatches(const std::string& text, const std::string& query);
    LRESULT handleMessage(UINT msg, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
};

// Global instance
SearchWidget& getSearchWidget();

} // namespace RawrXD::UI
