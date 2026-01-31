#pragma once
/**
 * \file multi_file_search.h
 * \brief Project-wide search/replace widget (like VS Code Ctrl+Shift+F)
 * \author RawrXD Team
 * \date 2025-12-05
 * 
 * Features:
 * - Search across entire project or selected folders
 * - File filter patterns (*.cpp, *.h, etc.)
 * - .gitignore support (exclude ignored files)
 * - Async search with progress bar and cancellation
 * - Results tree showing matches grouped by file
 * - Click result to jump to file+line
 * - Export results to file
 * - Replace in multiple files
 */


#include "../utils/file_operations.h"

namespace RawrXD {

/**
 * \struct MultiFileSearchResult
 * \brief Single match in multi-file search
 */
struct MultiFileSearchResult {
    std::string file;           ///< Absolute path to file
    int line;               ///< Line number (0-based)
    int column;             ///< Column number (0-based)
    std::string lineText;       ///< Full line text with match
    std::string matchedText;    ///< The matched portion
    
    MultiFileSearchResult() : line(-1), column(-1) {}
    MultiFileSearchResult(const std::string& f, int l, int c, const std::string& lt, const std::string& mt)
        : file(f), line(l), column(c), lineText(lt), matchedText(mt) {}
};

/**
 * \class MultiFileSearchWidget
 * \brief Project-wide search and replace
 * 
 * This widget provides VS Code-style global search across all files
 * in a project, with filtering, progress tracking, and result navigation.
 * 
 * Usage:
 * \code
 * auto* search = new MultiFileSearchWidget(this);
 * search->setProjectPath("/path/to/project");
 * search->show();
 * connect(search, &MultiFileSearchWidget::resultClicked,
 *         this, &MainWindow::openFileAtLine);
 * \endcode
 */
class MultiFileSearchWidget : public void {

public:
    explicit MultiFileSearchWidget(void* parent = nullptr);
    ~MultiFileSearchWidget() override;
    
    /**
     * \brief Set project root path to search in
     * \param path Absolute path to project root
     */
    void setProjectPath(const std::string& path);
    
    /**
     * \brief Get current project path
     * \return Project root path
     */
    std::string projectPath() const;
    
    /**
     * \brief Set search query
     * \param query Search pattern
     */
    void setSearchQuery(const std::string& query);
    
    /**
     * \brief Get search query
     * \return Search pattern
     */
    std::string searchQuery() const;
    
    /**
     * \brief Set file filter pattern
     * \param pattern Wildcard pattern (e.g., "*.cpp *.h")
     */
    void setFileFilter(const std::string& pattern);
    
    /**
     * \brief Get file filter pattern
     * \return Current file filter
     */
    std::string fileFilter() const;
    
    /**
     * \brief Set whether to respect .gitignore
     * \param enabled If true, skip .gitignore files
     */
    void setRespectGitignore(bool enabled);
    
    /**
     * \brief Get whether .gitignore is respected
     * \return true if .gitignore is used
     */
    bool respectsGitignore() const;
    
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
     * \brief Get all search results
     * \return List of all matches
     */
    std::vector<MultiFileSearchResult> results() const;
    
    /**
     * \brief Export results to text file
     * \param filePath Path to save results
     * \return true if successful
     */
    bool exportResults(const std::string& filePath);
    

    /**
     * \brief Emitted when user clicks a search result
     * \param filePath Absolute path to file
     * \param line Line number (0-based)
     * \param column Column number (0-based)
     */
    void resultClicked(const std::string& filePath, int line, int column);
    
    /**
     * \brief Emitted when search starts
     */
    void searchStarted();
    
    /**
     * \brief Emitted when search completes
     * \param resultCount Number of matches found
     * \param fileCount Number of files containing matches
     */
    void searchFinished(int resultCount, int fileCount);
    
    /**
     * \brief Emitted when search is cancelled
     */
    void searchCancelled();
    
    /**
     * \brief Emitted during search with progress
     * \param current Current file index
     * \param total Total files to search
     */
    void searchProgress(int current, int total);
    
public:
    /**
     * \brief Start search with current settings
     */
    void startSearch();
    
    /**
     * \brief Cancel ongoing search
     */
    void cancelSearch();
    
    /**
     * \brief Clear all results
     */
    void clearResults();
    
    /**
     * \brief Expand all file nodes in results tree
     */
    void expandAll();
    
    /**
     * \brief Collapse all file nodes in results tree
     */
    void collapseAll();
    
private:
    void onResultItemClicked(QTreeWidgetItem* item, int column);
    void onSearchCompleted();
    void onSearchProgressUpdate(int current, int total);
    
private:
    void setupUI();
    void updateResultsTree();
    void searchInFile(const std::string& filePath, std::vector<MultiFileSearchResult>& results);
    std::vector<std::string> collectFilesToSearch();
    bool shouldSkipFile(const std::string& filePath) const;
    
    // UI Components
    QVBoxLayout* m_mainLayout;
    QHBoxLayout* m_searchLayout;
    QHBoxLayout* m_filterLayout;
    QHBoxLayout* m_optionsLayout;
    
    QLineEdit* m_searchEdit;
    QLineEdit* m_filterEdit;
    QPushButton* m_searchButton;
    QPushButton* m_cancelButton;
    QCheckBox* m_caseSensitiveCheck;
    QCheckBox* m_wholeWordCheck;
    QCheckBox* m_regexCheck;
    QCheckBox* m_gitignoreCheck;
    QTreeWidget* m_resultsTree;
    QProgressBar* m_progressBar;
    QLabel* m_statusLabel;
    QPushButton* m_exportButton;
    QPushButton* m_expandAllButton;
    QPushButton* m_collapseAllButton;
    
    // State
    std::string m_projectPath;
    std::vector<MultiFileSearchResult> m_results;
    QFutureWatcher<void>* m_searchWatcher;
    std::mutex m_resultsMutex;
    bool m_searchCancelled;
    
    // Settings
    bool m_caseSensitive;
    bool m_wholeWord;
    bool m_useRegex;
    bool m_respectGitignore;
};

} // namespace RawrXD

