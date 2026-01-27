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

#include <QWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QCheckBox>
#include <QTreeWidget>
#include <QProgressBar>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QThread>
#include <QMutex>
#include <QFuture>
#include <QFutureWatcher>
#include "../utils/file_operations.h"

namespace RawrXD {

/**
 * \struct MultiFileSearchResult
 * \brief Single match in multi-file search
 */
struct MultiFileSearchResult {
    QString file;           ///< Absolute path to file
    int line;               ///< Line number (0-based)
    int column;             ///< Column number (0-based)
    QString lineText;       ///< Full line text with match
    QString matchedText;    ///< The matched portion
    
    MultiFileSearchResult() : line(-1), column(-1) {}
    MultiFileSearchResult(const QString& f, int l, int c, const QString& lt, const QString& mt)
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
class MultiFileSearchWidget : public QWidget {
    Q_OBJECT
    
public:
    explicit MultiFileSearchWidget(QWidget* parent = nullptr);
    ~MultiFileSearchWidget() override;
    
    /**
     * \brief Set project root path to search in
     * \param path Absolute path to project root
     */
    void setProjectPath(const QString& path);
    
    /**
     * \brief Get current project path
     * \return Project root path
     */
    QString projectPath() const;
    
    /**
     * \brief Set search query
     * \param query Search pattern
     */
    void setSearchQuery(const QString& query);
    
    /**
     * \brief Get search query
     * \return Search pattern
     */
    QString searchQuery() const;
    
    /**
     * \brief Set file filter pattern
     * \param pattern Wildcard pattern (e.g., "*.cpp *.h")
     */
    void setFileFilter(const QString& pattern);
    
    /**
     * \brief Get file filter pattern
     * \return Current file filter
     */
    QString fileFilter() const;
    
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
    QList<MultiFileSearchResult> results() const;
    
    /**
     * \brief Export results to text file
     * \param filePath Path to save results
     * \return true if successful
     */
    bool exportResults(const QString& filePath);
    
signals:
    /**
     * \brief Emitted when user clicks a search result
     * \param filePath Absolute path to file
     * \param line Line number (0-based)
     * \param column Column number (0-based)
     */
    void resultClicked(const QString& filePath, int line, int column);
    
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
    
public slots:
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
    
private slots:
    void onResultItemClicked(QTreeWidgetItem* item, int column);
    void onSearchCompleted();
    void onSearchProgressUpdate(int current, int total);
    
private:
    void setupUI();
    void updateResultsTree();
    void searchInFile(const QString& filePath, QList<MultiFileSearchResult>& results);
    QStringList collectFilesToSearch();
    bool shouldSkipFile(const QString& filePath) const;
    
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
    QString m_projectPath;
    QList<MultiFileSearchResult> m_results;
    QFutureWatcher<void>* m_searchWatcher;
    QMutex m_resultsMutex;
    bool m_searchCancelled;
    
    // Settings
    bool m_caseSensitive;
    bool m_wholeWord;
    bool m_useRegex;
    bool m_respectGitignore;
};

} // namespace RawrXD
