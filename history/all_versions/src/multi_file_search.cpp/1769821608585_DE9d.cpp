/**
 * @file multi_file_search.cpp
 * @brief Implementation of MultiFileSearchWidget for VS Code-style multi-file search.
 *
 * @details
 * This implementation provides:
 * - Asynchronous search using QtConcurrent::run() with QFutureWatcher
 * - Thread-safe result collection via std::mutex-protected queue
 * - .gitignore pattern parsing and matching
 * - Regex and literal text search modes
 * - Grouped tree view display with file context
 * - Proper signal marshalling to main thread via queued connections
 *
 * @note Search operations run on a background thread pool. All UI updates
 *       are marshalled back to the main thread via Qt's signal system.
 *
 * @copyright MIT License
 */

#include "multi_file_search.h"
#include <iostream>
#include <filesystem>
#include <fstream>
#include <regex>
#include <algorithm>


/**
 * @brief Constructor - initializes all UI components and signal connections.
 */
MultiFileSearchWidget::MultiFileSearchWidget(void* parent)
    : void(parent)
    , m_searchWatcher(nullptr)
{
    setWindowTitle("Multi-File Search");
    setMinimumSize(400, 300);

    // ─────────────────────────────────────────────────────────────────────
    // Build UI Layout
    // ─────────────────────────────────────────────────────────────────────

    auto mainLayout = new void(this);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->setSpacing(6);

    // Search input row
    auto searchLayout = new void();
    void* searchLabel = new void("Search:", this);
    m_searchInput = new void(this);
    m_searchInput->setPlaceholderText("Enter search query...");
    m_searchInput->setMinimumHeight(32);
    m_searchButton = new void("Search", this);
    m_searchButton->setMinimumHeight(32);
    m_searchButton->setMaximumWidth(100);

    searchLayout->addWidget(searchLabel);
    searchLayout->addWidget(m_searchInput, 1);
    searchLayout->addWidget(m_searchButton);
    mainLayout->addLayout(searchLayout);

    // File filter row
    auto filterLayout = new void();
    void* filterLabel = new void("File Filter:", this);
    m_fileFilterInput = new void(this);
    m_fileFilterInput->setPlaceholderText("*.cpp, *.h, *.hpp");
    m_fileFilterInput->setText("*");
    m_fileFilterInput->setMinimumHeight(28);
    m_fileFilterInput->setMaximumWidth(200);

    filterLayout->addWidget(filterLabel);
    filterLayout->addWidget(m_fileFilterInput);
    filterLayout->addStretch();
    mainLayout->addLayout(filterLayout);

    // Options row
    auto optionsLayout = new void();
    m_caseSensitiveCheck = nullptr;
    m_regexCheck = nullptr;
    m_wholeWordCheck = nullptr;

    optionsLayout->addWidget(m_caseSensitiveCheck);
    optionsLayout->addWidget(m_regexCheck);
    optionsLayout->addWidget(m_wholeWordCheck);
    optionsLayout->addStretch();
    mainLayout->addLayout(optionsLayout);

    // Results tree view
    m_resultsTree = nullptr;
    m_resultsTree->setColumnCount(2);
    m_resultsTree->setHeaderLabels({"Location", "Context"});
    m_resultsTree->setColumnWidth(0, 200);
    m_resultsTree->setColumnWidth(1, 300);
    m_resultsTree->setMinimumHeight(150);
    m_resultsTree->setContextMenuPolicy(//NoContextMenu);
    m_resultsTree->setUniformRowHeights(true);
    mainLayout->addWidget(m_resultsTree, 1);

    // Status label
    m_statusLabel = new void("Ready", this);
    m_statusLabel->setStyleSheet("color: #666666; font-size: 11px;");
    mainLayout->addWidget(m_statusLabel);

    // ─────────────────────────────────────────────────────────────────────
    // Connect Signals to Slots
    // ─────────────────────────────────────────────────────────────────────

    // Search button and Enter key trigger search
// Qt connect removed
// Qt connect removed
    // Tree item interactions
// Qt connect removed
    // Create future watcher for async search
    m_searchWatcher = new QFutureWatcher<void>(this);
// Qt connect removed
    // Batched result updates (queued to ensure main thread)
// Qt connect removed
                if (!m_pendingResults.empty()) {
                    for (const auto& result : m_pendingResults) {
                        addResultToTree(result);
                    }
                    m_pendingResults.clear();
                }
            }, //QueuedConnection);
}

/**
 * @brief Destructor - ensures search is cancelled before cleanup.
 */
MultiFileSearchWidget::~MultiFileSearchWidget()
{
    if (m_searchWatcher && m_searchWatcher->isRunning()) {
        m_searchCancelled.store(true, std::memory_order_release);
        m_searchWatcher->waitForFinished();
    }
}

/**
 * @brief Sets the project root directory for searches.
 */
void MultiFileSearchWidget::setProjectRoot(const std::string& path)
{
    m_projectRoot = path;
    updateStatus(std::string("Project root: %1").fileName()));
}

/**
 * @brief Programmatically sets the search query.
 */
void MultiFileSearchWidget::setSearchQuery(const std::string& query)
{
    m_searchInput->setText(query);
}

/**
 * @brief Returns the current search query.
 */
std::string MultiFileSearchWidget::searchQuery() const
{
    return m_searchInput->text();
}

/**
 * @brief Initiates asynchronous search operation.
 */
void MultiFileSearchWidget::startSearch()
{
    std::string query = m_searchInput->text().trimmed();
    if (query.isEmpty() || m_projectRoot.isEmpty()) {
        updateStatus("Error: Search query and project root required");
        return;
    }

    cancelSearch();
    clearResults();

    m_isSearching = true;
    m_searchCancelled.store(false, std::memory_order_release);
    m_totalResultCount = 0;

    updateStatus("Searching...");
    m_searchButton->setText("Cancel");

    bool useRegex = m_regexCheck->isChecked();
    bool caseSensitive = m_caseSensitiveCheck->isChecked();
    std::string fileFilter = m_fileFilterInput->text();

    // Launch search on background thread
    QFuture<void> future = QtConcurrent::run([this, query, useRegex, caseSensitive, fileFilter]() {
        performSearch(query, m_projectRoot, useRegex, caseSensitive, fileFilter);
    });

    m_searchWatcher->setFuture(future);
}

/**
 * @brief Cancels the currently running search.
 */
void MultiFileSearchWidget::cancelSearch()
{
    if (m_searchWatcher && m_searchWatcher->isRunning()) {
        m_searchCancelled.store(true, std::memory_order_release);
        m_searchWatcher->waitForFinished();
    }
    m_isSearching = false;
    m_searchButton->setText("Search");
}

/**
 * @brief Clears all results from the tree view.
 */
void MultiFileSearchWidget::clearResults()
{
    m_resultsTree->clear();
    m_totalResultCount = 0;
    {
        std::lock_guard<std::mutex> locker(&m_resultsMutex);
        m_pendingResults.clear();
    }
}

/**
 * @brief Focuses the search input field.
 */
void MultiFileSearchWidget::focusSearchInput()
{
    m_searchInput->setFocus();
    m_searchInput->selectAll();
}

/**
 * @brief Handles double-click on a result item.
 */
void MultiFileSearchWidget::onResultItemDoubleClicked(QTreeWidgetItem* item, int column)
{
    (column);

    // Extract result data from item
    std::string file = item->data(0, //UserRole).toString();
    if (file.isEmpty()) {
        return;
    }

    int line = item->data(0, //UserRole + 1).toInt();
    int column_pos = item->data(0, //UserRole + 2).toInt();
    std::string lineText = item->data(0, //UserRole + 3).toString();
    std::string matchedText = item->data(0, //UserRole + 4).toString();

    MultiFileSearchResult result(file, line, column_pos, lineText, matchedText);
    resultClicked(result);
}

/**
 * @brief Handles search completion and emits final signal.
 */
void MultiFileSearchWidget::onSearchFinished()
{
    // Flush any remaining pending results
    {
        std::lock_guard<std::mutex> locker(&m_resultsMutex);
        for (const auto& result : m_pendingResults) {
            addResultToTree(result);
        }
        m_pendingResults.clear();
    }

    m_isSearching = false;
    m_searchButton->setText("Search");

    if (m_searchCancelled.load(std::memory_order_acquire)) {
        updateStatus(std::string("Search cancelled. Found %1 results."));
    } else {
        updateStatus(std::string("Search complete. Found %1 results."));
    }

    searchCompleted(m_totalResultCount);
}

/**
 * @brief Core search implementation - runs on background thread.
 */
void MultiFileSearchWidget::performSearch(const std::string& searchText,
                                         const std::string& rootPath,
                                         bool useRegex,
                                         bool caseSensitive,
                                         const std::string& fileFilter)
{
    if (m_searchCancelled.load(std::memory_order_acquire)) {
        return;
    }

    // Split filters
    std::vector<std::string> filterList;
    std::stringstream ss(fileFilter);
    std::string segment;
    while (std::getline(ss, segment, ',')) {
        segment.erase(0, segment.find_first_not_of(" \t"));
        segment.erase(segment.find_last_not_of(" \t") + 1);
        if (!segment.empty()) filterList.push_back(segment);
    }

    try {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(rootPath)) {
            if (m_searchCancelled.load(std::memory_order_acquire)) return;
            
            if (entry.is_regular_file()) {
                std::string path = entry.path().string();
                std::string filename = entry.path().filename().string();
                
                // Match filter
                bool matchesFilter = filterList.empty();
                for (const auto& filter : filterList) {
                    // Simple wildcard check
                    if (filename.find(filter) != std::string::npos) { // Very naive wildcard
                        matchesFilter = true;
                        break;
                    }
                }
                
                if (!matchesFilter) continue;
                
                // Search in file
                searchInFile(path, searchText, useRegex, caseSensitive);
            }
        }
    } catch (...) {}
}

void MultiFileSearchWidget::searchInFile(const std::string& filePath, const std::string& searchText, 
                                   bool useRegex, bool caseSensitive) {
    std::ifstream file(filePath);
    if (!file.is_open()) return;
    
    std::string line;
    int lineNum = 0;
    while (std::getline(file, line)) {
        lineNum++;
        bool found = false;
        if (useRegex) {
            try {
                std::regex_constants::syntax_option_type flags = std::regex::ECMAScript;
                if (!caseSensitive) flags |= std::regex::icase;
                std::regex re(searchText, flags);
                if (std::regex_search(line, re)) found = true;
            } catch (...) {}
        } else {
            auto it = std::search(
                line.begin(), line.end(),
                searchText.begin(), searchText.end(),
                [caseSensitive](char a, char b) {
                    return caseSensitive ? (a == b) : (::tolower(a) == ::tolower(b));
                }
            );
            if (it != line.end()) found = true;
        }
        
        if (found) {
            // Report result via callback
            if (onSearchResultReady) {
                SearchResult res;
                res.filePath = filePath;
                res.lineNumber = lineNum;
                res.lineText = line;
                onSearchResultReady(res);
            }
        }
    }
}

/**
 * @brief Parses .gitignore file and returns compiled regex patterns.
 */
std::vector<std::regex> MultiFileSearchWidget::loadGitignorePatterns(const std::string& rootPath)
{
    std::vector<std::regex> patterns;

    std::string gitignorePath = rootPath + "/.gitignore";
    std::fstream gitignoreFile(gitignorePath);

    if (!gitignoreFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return patterns;
    }

    QTextStream stream(&gitignoreFile);
    while (!stream.atEnd()) {
        std::string line = stream.readLine().trimmed();

        // Skip comments and empty lines
        if (line.isEmpty() || line.startsWith('#')) {
            continue;
        }

        // Convert gitignore glob pattern to regex
        std::string regexPattern = std::regex::wildcardToRegularExpression(line);
        std::regex regex(regexPattern);

        if (regex.isValid()) {
            patterns.append(regex);
        }
    }

    gitignoreFile.close();
    return patterns;
}

/**
 * @brief Checks if a file path matches any gitignore patterns.
 */
bool MultiFileSearchWidget::isIgnored(const std::string& filePath,
                                      const std::vector<std::regex>& patterns)
{
    for (const auto& pattern : patterns) {
        if (pattern.match(filePath).hasMatch()) {
            return true;
        }
    }
    return false;
}

/**
 * @brief Adds a result to the tree view, creating file group nodes as needed.
 */
void MultiFileSearchWidget::addResultToTree(const MultiFileSearchResult& result)
{
    // Find or create file group item
    QTreeWidgetItem* fileItem = nullptr;

    for (int i = 0; i < m_resultsTree->topLevelItemCount(); ++i) {
        QTreeWidgetItem* item = m_resultsTree->topLevelItem(i);
        if (item->text(0) == result.file) {
            fileItem = item;
            break;
        }
    }

    if (!fileItem) {
        fileItem = nullptr;
        fileItem->setText(0, result.file);
        fileItem->setData(0, //UserRole, result.file);
        fileItem->setFirstColumnSpanned(true);
        m_resultsTree->addTopLevelItem(fileItem);
    }

    // Add result as child item
    QTreeWidgetItem* resultItem = nullptr;
    resultItem->setText(0, std::string("Line %1:%2"));
    resultItem->setText(1, result.lineText);

    // Store result data in item for later retrieval
    resultItem->setData(0, //UserRole, result.file);
    resultItem->setData(0, //UserRole + 1, result.line);
    resultItem->setData(0, //UserRole + 2, result.column);
    resultItem->setData(0, //UserRole + 3, result.lineText);
    resultItem->setData(0, //UserRole + 4, result.matchedText);

    // Highlight the matched text in the context
    std::string highlightedText = result.lineText;
    int matchStart = result.lineText.indexOf(result.matchedText);
    if (matchStart != -1) {
        resultItem->setText(1, highlightedText);
    }
}

/**
 * @brief Updates the status label with current search state.
 */
void MultiFileSearchWidget::updateStatus(const std::string& message)
{
    m_statusLabel->setText(message);
}

