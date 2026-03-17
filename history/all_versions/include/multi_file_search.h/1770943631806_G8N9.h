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
 * The widget uses std::future to run searches on a background thread,
 * collecting results via thread-safe queue protected by std::mutex. Results
 * are batched and dispatched to registered callback handlers.
 *
 * @par Keyboard Shortcuts:
 * - Enter: Start search / Navigate to selected result
 * - Escape: Cancel running search / Clear results
 * - Ctrl+Shift+F: Global shortcut to focus search input
 *
 * @note Thread Safety: Search operations run on background threads.
 *       UI updates are marshalled to the main thread via callbacks.
 *
 * @author RawrXD IDE Team
 * @version 2.0.0
 * @date 2025
 *
 * @copyright MIT License
 */
#pragma once

#include <string>
#include <vector>
#include <mutex>
#include <future>
#include <regex>
#include <atomic>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

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
 * MultiFileSearchWidget searchWidget;
 * searchWidget.setProjectRoot("/path/to/project");
 *
 * searchWidget.setResultClickedCb([](void*, const MultiFileSearchResult* r) {
 *     // Navigate to match location
 * }, nullptr);
 *
 * // Optionally trigger search programmatically:
 * searchWidget.setSearchQuery("TODO:");
 * searchWidget.startSearch();
 * @endcode
 *
 * @par Callback Connections:
 * - ResultClickedCb: Invoked when user double-clicks a result
 * - SearchCompletedCb: Invoked when search finishes with total result count
 *
 * @see MultiFileSearchResult for the result data structure
 */
class MultiFileSearchWidget {
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
    MultiFileSearchWidget() = default;

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
    void setProjectRoot(const std::string& path);

    /**
     * @brief Gets the currently configured project root.
     * @return Absolute path to the project root, or empty if not set
     */
    std::string projectRoot() const { return m_projectRoot; }

    /**
     * @brief Programmatically sets the search query.
     * @param query The search text or regex pattern
     *
     * Does not automatically start the search - call startSearch() afterward.
     */
    void setSearchQuery(const std::string& query);

    /**
     * @brief Gets the current search query text.
     * @return Current contents of the search input field
     */
    std::string searchQuery() const;

    /**
     * @brief Checks if a search is currently in progress.
     * @return true if an asynchronous search is running
     */
    bool isSearching() const { return m_isSearching; }

public:
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

    // ─────────────────────────────────────────────────────────────────────
    // Callbacks (replaces Qt signals)
    // ─────────────────────────────────────────────────────────────────────

    /**
     * @brief Callback type invoked when user double-clicks or presses Enter on a result.
     * @param ctx User-provided context pointer
     * @param result The selected search result with file/line/column info
     *
     * Register via setResultClickedCb() to implement navigation to the match location:
     * @code
     * searchWidget.setResultClickedCb([](void*, const MultiFileSearchResult* r) {
     *     openFile(r->file);
     *     goToLine(r->line, r->column);
     * }, nullptr);
     * @endcode
     */
    using ResultClickedCb = void(*)(void* ctx, const MultiFileSearchResult* result);

    /**
     * @brief Callback type invoked when a search operation completes (success or cancelled).
     * @param ctx User-provided context pointer
     * @param totalResults Total number of matches found
     */
    using SearchCompletedCb = void(*)(void* ctx, int totalResults);

    /**
     * @brief Callback type invoked periodically during search with progress updates.
     * @param ctx User-provided context pointer
     * @param filesSearched Number of files searched so far
     * @param matchesFound Number of matches found so far
     */
    using SearchProgressCb = void(*)(void* ctx, int filesSearched, int matchesFound);

    void setResultClickedCb(ResultClickedCb cb, void* ctx) { m_clickedCb = cb; m_clickedCtx = ctx; }
    void setSearchCompletedCb(SearchCompletedCb cb, void* ctx) { m_completedCb = cb; m_completedCtx = ctx; }
    void setSearchProgressCb(SearchProgressCb cb, void* ctx) { m_progressCb = cb; m_progressCtx = ctx; }

private:
    /**
     * @brief Handles double-click on a tree item.
     * @param item The clicked tree widget item
     * @param column The column that was clicked
     */
    void onResultItemDoubleClicked(void* item, int column);

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
    void performSearch(const std::string& searchText,
                       const std::string& rootPath,
                       bool useRegex,
                       bool caseSensitive,
                       const std::string& fileFilter);

    /**
     * @brief Parses .gitignore files and builds exclusion patterns.
     * @param rootPath Project root containing .gitignore
     * @return List of compiled regex patterns for ignored paths
     */
    std::vector<std::regex> loadGitignorePatterns(const std::string& rootPath);

    /**
     * @brief Checks if a file path should be excluded from search.
     * @param filePath Absolute path to check
     * @param patterns Compiled gitignore patterns
     * @return true if the file should be skipped
     */
    bool isIgnored(const std::string& filePath,
                   const std::vector<std::regex>& patterns);

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
    void updateStatus(const std::string& message);

    // ─────────────────────────────────────────────────────────────────────
    // UI Components
    // ─────────────────────────────────────────────────────────────────────

    HWND m_searchInput = nullptr;          ///< Main search query input field
    HWND m_fileFilterInput = nullptr;      ///< File pattern filter (e.g., "*.cpp, *.h")
    HWND m_caseSensitiveCheck = nullptr;   ///< Case sensitivity toggle
    HWND m_regexCheck = nullptr;           ///< Regex mode toggle
    HWND m_wholeWordCheck = nullptr;       ///< Whole word matching toggle
    HWND m_searchButton = nullptr;         ///< Search/Cancel button
    HWND m_resultsTree = nullptr;          ///< Grouped results display
    HWND m_statusLabel = nullptr;          ///< Search status and result count

    // ─────────────────────────────────────────────────────────────────────
    // Search State
    // ─────────────────────────────────────────────────────────────────────

    std::string m_projectRoot;                                ///< Project root directory
    std::atomic<bool> m_searchCancelled{false};               ///< Cancellation flag (atomic for thread safety)
    bool m_isSearching = false;                               ///< Search-in-progress flag
    std::future<void> m_searchFuture;                         ///< Async search task

    // ─────────────────────────────────────────────────────────────────────
    // Thread-Safe Result Collection
    // ─────────────────────────────────────────────────────────────────────

    mutable std::mutex m_resultsMutex;                        ///< Protects m_pendingResults
    std::vector<MultiFileSearchResult> m_pendingResults;      ///< Results waiting for UI update
    int m_totalResultCount = 0;                               ///< Running total of matches found

    // ─────────────────────────────────────────────────────────────────────
    // Callback Pointers (replaces Qt signals)
    // ─────────────────────────────────────────────────────────────────────

    ResultClickedCb m_clickedCb = nullptr;                    ///< Result clicked callback
    void* m_clickedCtx = nullptr;                             ///< Result clicked callback context
    SearchCompletedCb m_completedCb = nullptr;                ///< Search completed callback
    void* m_completedCtx = nullptr;                           ///< Search completed callback context
    SearchProgressCb m_progressCb = nullptr;                  ///< Search progress callback
    void* m_progressCtx = nullptr;                            ///< Search progress callback context
};
