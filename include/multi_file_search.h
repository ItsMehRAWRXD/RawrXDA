/**
 * @file multi_file_search.h
 * @brief VS Code-style multi-file search widget for RawrXD IDE.
 *
 * Provides a complete implementation of project-wide text search with:
 * - Asynchronous file traversal and search (QtConcurrent)
 * - .gitignore-aware file filtering
 * - Regex and literal text search modes
 * - Case-sensitive/insensitive matching
 * - Real-time result streaming with thread-safe collection
 * - Cancellable long-running searches
 * - Interactive tree view with file grouping
 *
 * @par Architecture:
 * The widget uses QFutureWatcher to run searches on a background thread,
 * collecting results via thread-safe queue protected by QMutex. Results
 * are batched and emitted to the UI thread via queued signal connections.
 *
 * @par Keyboard Shortcuts:
 * - Enter: Start search / Navigate to selected result
 * - Escape: Cancel running search / Clear results
 * - Ctrl+Shift+F: Global shortcut to focus search input
 *
 * @note Thread Safety: Search operations run on background threads.
 *       UI updates are marshalled to the main thread via signals.
 *
 * @author RawrXD IDE Team
 * @version 2.0.0
 * @date 2025
 *
 * @copyright MIT License
 */
#pragma once

#include <QWidget>
#include <QLineEdit>
#include <QTreeWidget>
#include <QPushButton>
#include <QCheckBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMutex>
#include <QFutureWatcher>
#include <QRegularExpression>
#include <atomic>
#include <vector>

#include "file_manager.h"

/**
 * @class MultiFileSearchWidget
 * @brief Complete multi-file search panel with async search and result navigation.
 *
 * This widget provides a full-featured search interface similar to VS Code's
 * Ctrl+Shift+F functionality. Searches run asynchronously with cancellation
 * support, and results are displayed in a grouped tree view.
 *
 * @par Usage Example:
 * @code
 * MultiFileSearchWidget* searchWidget = new MultiFileSearchWidget(this);
 * searchWidget->setProjectRoot("/path/to/project");
 *
 * connect(searchWidget, &MultiFileSearchWidget::resultClicked,
 *         this, &MainWindow::navigateToSearchResult);
 *
 * // Optionally trigger search programmatically:
 * searchWidget->setSearchQuery("TODO:");
 * searchWidget->startSearch();
 * @endcode
 *
 * @par Signal/Slot Connections:
 * - resultClicked(MultiFileSearchResult): Emitted when user double-clicks a result
 * - searchCompleted(int): Emitted when search finishes with total result count
 *
 * @see MultiFileSearchResult for the result data structure
 */
class MultiFileSearchWidget : public QWidget {
    Q_OBJECT

public:
    /**
     * @brief Constructs the search widget with all UI components.
     * @param parent Parent widget (typically the main window or dock widget)
     *
     * Initializes:
     * - Search input field with placeholder text
     * - File filter input (glob patterns like "*.cpp, *.h")
     * - Option checkboxes (case sensitive, regex, whole word)
     * - Results tree view with custom item delegate
     * - Progress indicator and status label
     */
    explicit MultiFileSearchWidget(QWidget* parent = nullptr);

    /**
     * @brief Destructor - cancels any running search and cleans up.
     */
    ~MultiFileSearchWidget();

    /**
     * @brief Sets the root directory for project-wide searches.
     * @param path Absolute path to the project root directory
     *
     * This path is used as the base for:
     * - File traversal during search
     * - Relative path display in results
     * - .gitignore file discovery
     */
    void setProjectRoot(const QString& path);

    /**
     * @brief Gets the currently configured project root.
     * @return Absolute path to the project root, or empty if not set
     */
    QString projectRoot() const { return m_projectRoot; }

    /**
     * @brief Programmatically sets the search query.
     * @param query The search text or regex pattern
     *
     * Does not automatically start the search - call startSearch() afterward.
     */
    void setSearchQuery(const QString& query);

    /**
     * @brief Gets the current search query text.
     * @return Current contents of the search input field
     */
    QString searchQuery() const;

    /**
     * @brief Checks if a search is currently in progress.
     * @return true if an asynchronous search is running
     */
    bool isSearching() const { return m_isSearching; }

public slots:
    /**
     * @brief Initiates an asynchronous search operation.
     *
     * Cancels any existing search, clears previous results, and starts
     * a new background search using the current query and options.
     *
     * @pre Project root must be set via setProjectRoot()
     * @pre Search query must not be empty
     *
     * @note Safe to call while a search is running - will cancel and restart.
     */
    void startSearch();

    /**
     * @brief Cancels any currently running search operation.
     *
     * Sets the cancellation flag and waits for background threads to stop.
     * Partial results collected before cancellation remain in the tree view.
     */
    void cancelSearch();

    /**
     * @brief Clears all results and resets the search state.
     *
     * Cancels any running search and removes all items from the tree view.
     * Does not clear the search query input field.
     */
    void clearResults();

    /**
     * @brief Sets focus to the search input field.
     *
     * Typically connected to a global Ctrl+Shift+F shortcut.
     * Also selects all text in the input for easy replacement.
     */
    void focusSearchInput();

signals:
    /**
     * @brief Emitted when user double-clicks or presses Enter on a result.
     * @param result The selected search result with file/line/column info
     *
     * Connect to this signal to implement navigation to the match location:
     * @code
     * connect(searchWidget, &MultiFileSearchWidget::resultClicked,
     *         [this](const MultiFileSearchResult& r) {
     *             openFile(r.file);
     *             goToLine(r.line, r.column);
     *         });
     * @endcode
     */
    void resultClicked(const MultiFileSearchResult& result);

    /**
     * @brief Emitted when a search operation completes (success or cancelled).
     * @param totalResults Total number of matches found
     */
    void searchCompleted(int totalResults);

    /**
     * @brief Emitted periodically during search with progress updates.
     * @param filesSearched Number of files searched so far
     * @param matchesFound Number of matches found so far
     */
    void searchProgress(int filesSearched, int matchesFound);

private slots:
    /**
     * @brief Handles double-click on a tree item.
     * @param item The clicked tree widget item
     * @param column The column that was clicked
     */
    void onResultItemDoubleClicked(QTreeWidgetItem* item, int column);

    /**
     * @brief Processes batched results from the background thread.
     *
     * Called via queued connection to ensure UI updates happen on main thread.
     */
    void onSearchResultsReady();

    /**
     * @brief Handles search completion from QFutureWatcher.
     */
    void onSearchFinished();

private:
    /**
     * @brief Core search implementation running on background thread.
     * @param searchText The query text or regex pattern
     * @param rootPath Project root for file traversal
     * @param useRegex Whether to interpret searchText as regex
     * @param caseSensitive Whether matching is case-sensitive
     * @param fileFilter Glob patterns for file filtering (comma-separated)
     *
     * Traverses the project directory, respects .gitignore rules, and
     * collects matches into the thread-safe results queue.
     */
    void performSearch(const QString& searchText,
                       const QString& rootPath,
                       bool useRegex,
                       bool caseSensitive,
                       const QString& fileFilter);

    /**
     * @brief Parses .gitignore files and builds exclusion patterns.
     * @param rootPath Project root containing .gitignore
     * @return List of compiled regex patterns for ignored paths
     */
    QVector<QRegularExpression> loadGitignorePatterns(const QString& rootPath);

    /**
     * @brief Checks if a file path should be excluded from search.
     * @param filePath Absolute path to check
     * @param patterns Compiled gitignore patterns
     * @return true if the file should be skipped
     */
    bool isIgnored(const QString& filePath,
                   const QVector<QRegularExpression>& patterns);

    /**
     * @brief Adds a result to the tree view, grouped by file.
     * @param result The search result to add
     *
     * Creates file group nodes as needed and adds match items underneath.
     */
    void addResultToTree(const MultiFileSearchResult& result);

    /**
     * @brief Updates the status label with current search state.
     * @param message Status message to display
     */
    void updateStatus(const QString& message);

    // ─────────────────────────────────────────────────────────────────────
    // UI Components
    // ─────────────────────────────────────────────────────────────────────

    QLineEdit* m_searchInput;          ///< Main search query input field
    QLineEdit* m_fileFilterInput;      ///< File pattern filter (e.g., "*.cpp, *.h")
    QCheckBox* m_caseSensitiveCheck;   ///< Case sensitivity toggle
    QCheckBox* m_regexCheck;           ///< Regex mode toggle
    QCheckBox* m_wholeWordCheck;       ///< Whole word matching toggle
    QPushButton* m_searchButton;       ///< Search/Cancel button
    QTreeWidget* m_resultsTree;        ///< Grouped results display
    QLabel* m_statusLabel;             ///< Search status and result count

    // ─────────────────────────────────────────────────────────────────────
    // Search State
    // ─────────────────────────────────────────────────────────────────────

    QString m_projectRoot;                                    ///< Project root directory
    std::atomic<bool> m_searchCancelled{false};               ///< Cancellation flag (atomic for thread safety)
    bool m_isSearching = false;                               ///< Search-in-progress flag
    QFutureWatcher<void>* m_searchWatcher = nullptr;          ///< Async search monitor

    // ─────────────────────────────────────────────────────────────────────
    // Thread-Safe Result Collection
    // ─────────────────────────────────────────────────────────────────────

    mutable QMutex m_resultsMutex;                            ///< Protects m_pendingResults
    std::vector<MultiFileSearchResult> m_pendingResults;      ///< Results waiting for UI update
    int m_totalResultCount = 0;                               ///< Running total of matches found
};
