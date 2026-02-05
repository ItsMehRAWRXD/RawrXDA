/**
 * @file multi_file_search.cpp
 * @brief Implementation of MultiFileSearchWidget for VS Code-style multi-file search.
 *
 * @details
 * This implementation provides:
 * - Asynchronous search using QtConcurrent::run() with QFutureWatcher
 * - Thread-safe result collection via QMutex-protected queue
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

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QPushButton>
#include <QCheckBox>
#include <QLabel>
#include <QDir>
#include <QFileInfo>
#include <QFile>
#include <QTextStream>
#include <QRegularExpression>
#include <QRegularExpressionMatch>
#include <QtConcurrent>
#include <QFutureWatcher>
#include <QMutexLocker>
#include <QDebug>


/**
 * @brief Constructor - initializes all UI components and signal connections.
 */
MultiFileSearchWidget::MultiFileSearchWidget(QWidget* parent)
    : QWidget(parent)
    , m_searchWatcher(nullptr)
{
    setWindowTitle("Multi-File Search");
    setMinimumSize(400, 300);

    // ─────────────────────────────────────────────────────────────────────
    // Build UI Layout
    // ─────────────────────────────────────────────────────────────────────

    auto mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->setSpacing(6);

    // Search input row
    auto searchLayout = new QHBoxLayout();
    QLabel* searchLabel = new QLabel("Search:", this);
    m_searchInput = new QLineEdit(this);
    m_searchInput->setPlaceholderText("Enter search query...");
    m_searchInput->setMinimumHeight(32);
    m_searchButton = new QPushButton("Search", this);
    m_searchButton->setMinimumHeight(32);
    m_searchButton->setMaximumWidth(100);

    searchLayout->addWidget(searchLabel);
    searchLayout->addWidget(m_searchInput, 1);
    searchLayout->addWidget(m_searchButton);
    mainLayout->addLayout(searchLayout);

    // File filter row
    auto filterLayout = new QHBoxLayout();
    QLabel* filterLabel = new QLabel("File Filter:", this);
    m_fileFilterInput = new QLineEdit(this);
    m_fileFilterInput->setPlaceholderText("*.cpp, *.h, *.hpp");
    m_fileFilterInput->setText("*");
    m_fileFilterInput->setMinimumHeight(28);
    m_fileFilterInput->setMaximumWidth(200);

    filterLayout->addWidget(filterLabel);
    filterLayout->addWidget(m_fileFilterInput);
    filterLayout->addStretch();
    mainLayout->addLayout(filterLayout);

    // Options row
    auto optionsLayout = new QHBoxLayout();
    m_caseSensitiveCheck = new QCheckBox("Case Sensitive", this);
    m_regexCheck = new QCheckBox("Regex", this);
    m_wholeWordCheck = new QCheckBox("Whole Word", this);

    optionsLayout->addWidget(m_caseSensitiveCheck);
    optionsLayout->addWidget(m_regexCheck);
    optionsLayout->addWidget(m_wholeWordCheck);
    optionsLayout->addStretch();
    mainLayout->addLayout(optionsLayout);

    // Results tree view
    m_resultsTree = new QTreeWidget(this);
    m_resultsTree->setColumnCount(2);
    m_resultsTree->setHeaderLabels({"Location", "Context"});
    m_resultsTree->setColumnWidth(0, 200);
    m_resultsTree->setColumnWidth(1, 300);
    m_resultsTree->setMinimumHeight(150);
    m_resultsTree->setContextMenuPolicy(Qt::NoContextMenu);
    m_resultsTree->setUniformRowHeights(true);
    mainLayout->addWidget(m_resultsTree, 1);

    // Status label
    m_statusLabel = new QLabel("Ready", this);
    m_statusLabel->setStyleSheet("color: #666666; font-size: 11px;");
    mainLayout->addWidget(m_statusLabel);

    // ─────────────────────────────────────────────────────────────────────
    // Connect Signals to Slots
    // ─────────────────────────────────────────────────────────────────────

    // Search button and Enter key trigger search
    connect(m_searchButton, &QPushButton::clicked, this, &MultiFileSearchWidget::startSearch);
    connect(m_searchInput, &QLineEdit::returnPressed, this, &MultiFileSearchWidget::startSearch);

    // Tree item interactions
    connect(m_resultsTree, &QTreeWidget::itemDoubleClicked,
            this, &MultiFileSearchWidget::onResultItemDoubleClicked);

    // Create future watcher for async search
    m_searchWatcher = new QFutureWatcher<void>(this);
    connect(m_searchWatcher, &QFutureWatcher<void>::finished,
            this, &MultiFileSearchWidget::onSearchFinished);

    // Batched result updates (queued to ensure main thread)
    connect(this, &MultiFileSearchWidget::searchProgress,
            this, [this](int filesSearched, int matchesFound) {
                QMutexLocker locker(&m_resultsMutex);
                if (!m_pendingResults.empty()) {
                    for (const auto& result : m_pendingResults) {
                        addResultToTree(result);
                    }
                    m_pendingResults.clear();
                }
            }, Qt::QueuedConnection);
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
void MultiFileSearchWidget::setProjectRoot(const QString& path)
{
    m_projectRoot = path;
    updateStatus(QString("Project root: %1").arg(QFileInfo(path).fileName()));
}

/**
 * @brief Programmatically sets the search query.
 */
void MultiFileSearchWidget::setSearchQuery(const QString& query)
{
    m_searchInput->setText(query);
}

/**
 * @brief Returns the current search query.
 */
QString MultiFileSearchWidget::searchQuery() const
{
    return m_searchInput->text();
}

/**
 * @brief Initiates asynchronous search operation.
 */
void MultiFileSearchWidget::startSearch()
{
    QString query = m_searchInput->text().trimmed();
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
    QString fileFilter = m_fileFilterInput->text();

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
        QMutexLocker locker(&m_resultsMutex);
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
    Q_UNUSED(column);

    // Extract result data from item
    QString file = item->data(0, Qt::UserRole).toString();
    if (file.isEmpty()) {
        return;
    }

    int line = item->data(0, Qt::UserRole + 1).toInt();
    int column_pos = item->data(0, Qt::UserRole + 2).toInt();
    QString lineText = item->data(0, Qt::UserRole + 3).toString();
    QString matchedText = item->data(0, Qt::UserRole + 4).toString();

    MultiFileSearchResult result(file, line, column_pos, lineText, matchedText);
    emit resultClicked(result);
}

/**
 * @brief Handles search completion and emits final signal.
 */
void MultiFileSearchWidget::onSearchFinished()
{
    // Flush any remaining pending results
    {
        QMutexLocker locker(&m_resultsMutex);
        for (const auto& result : m_pendingResults) {
            addResultToTree(result);
        }
        m_pendingResults.clear();
    }

    m_isSearching = false;
    m_searchButton->setText("Search");

    if (m_searchCancelled.load(std::memory_order_acquire)) {
        updateStatus(QString("Search cancelled. Found %1 results.").arg(m_totalResultCount));
    } else {
        updateStatus(QString("Search complete. Found %1 results.").arg(m_totalResultCount));
    }

    emit searchCompleted(m_totalResultCount);
}

/**
 * @brief Core search implementation - runs on background thread.
 */
void MultiFileSearchWidget::performSearch(const QString& searchText,
                                         const QString& rootPath,
                                         bool useRegex,
                                         bool caseSensitive,
                                         const QString& fileFilter)
{
    QRegularExpression searchPattern;

    // Compile search pattern (regex or literal)
    if (useRegex) {
        QRegularExpression::PatternOptions options =
            caseSensitive ? QRegularExpression::NoPatternOption
                          : QRegularExpression::CaseInsensitiveOption;
        searchPattern.setPattern(searchText);
        searchPattern.setPatternOptions(options);

        if (!searchPattern.isValid()) {
            return;
        }
    }

    // Load gitignore patterns
    auto gitignorePatterns = loadGitignorePatterns(rootPath);

    // Parse file filter patterns
    QStringList filterList = fileFilter.split(',', Qt::SkipEmptyParts);
    for (auto& filter : filterList) {
        filter = filter.trimmed();
    }

    // Recursive directory traversal
    QDir dir(rootPath);
    QFileInfoList fileList = dir.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot,
                                               QDir::DirsFirst);

    for (const QFileInfo& fileInfo : fileList) {
        if (m_searchCancelled.load(std::memory_order_acquire)) {
            return;
        }

        if (fileInfo.isDir()) {
            performSearch(searchText, fileInfo.absoluteFilePath(), useRegex, caseSensitive, fileFilter);
        } else if (fileInfo.isFile()) {
            // Check if file should be ignored
            if (isIgnored(fileInfo.absoluteFilePath(), gitignorePatterns)) {
                continue;
            }

            // Check file filter
            bool matchesFilter = false;
            for (const QString& filter : filterList) {
                QRegularExpression filterPattern(QRegularExpression::wildcardToRegularExpression(filter));
                if (filterPattern.match(fileInfo.fileName()).hasMatch()) {
                    matchesFilter = true;
                    break;
                }
            }

            if (!matchesFilter) {
                continue;
            }

            // Read and search file
            QString fileContent = FileManager::readFile(fileInfo.absoluteFilePath());
            if (fileContent.isEmpty()) {
                continue;
            }

            QStringList lines = fileContent.split('\n');
            for (int lineNum = 0; lineNum < lines.size(); ++lineNum) {
                if (m_searchCancelled.load(std::memory_order_acquire)) {
                    return;
                }

                const QString& line = lines[lineNum];

                // Search for matches in line
                if (useRegex) {
                    QRegularExpressionMatchIterator it = searchPattern.globalMatch(line);
                    while (it.hasNext()) {
                        QRegularExpressionMatch match = it.next();
                        MultiFileSearchResult result(
                            fileInfo.absoluteFilePath(),
                            lineNum + 1,
                            match.capturedStart(),
                            line,
                            match.captured()
                        );

                        {
                            QMutexLocker locker(&m_resultsMutex);
                            m_pendingResults.push_back(result);
                            m_totalResultCount++;
                        }

                        // Batch emit every 20 results for UI responsiveness
                        if (m_totalResultCount % 20 == 0) {
                            emit searchProgress(0, m_totalResultCount);
                        }
                    }
                } else {
                    // Literal string search
                    Qt::CaseSensitivity cs = caseSensitive ? Qt::CaseSensitive : Qt::CaseInsensitive;
                    int startPos = 0;

                    while (true) {
                        int pos = line.indexOf(searchText, startPos, cs);
                        if (pos == -1) {
                            break;
                        }

                        MultiFileSearchResult result(
                            fileInfo.absoluteFilePath(),
                            lineNum + 1,
                            pos,
                            line,
                            searchText
                        );

                        {
                            QMutexLocker locker(&m_resultsMutex);
                            m_pendingResults.push_back(result);
                            m_totalResultCount++;
                        }

                        // Batch emit every 20 results
                        if (m_totalResultCount % 20 == 0) {
                            emit searchProgress(0, m_totalResultCount);
                        }

                        startPos = pos + searchText.length();
                    }
                }
            }
        }
    }
}

/**
 * @brief Parses .gitignore file and returns compiled regex patterns.
 */
QVector<QRegularExpression> MultiFileSearchWidget::loadGitignorePatterns(const QString& rootPath)
{
    QVector<QRegularExpression> patterns;

    QString gitignorePath = rootPath + "/.gitignore";
    QFile gitignoreFile(gitignorePath);

    if (!gitignoreFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return patterns;
    }

    QTextStream stream(&gitignoreFile);
    while (!stream.atEnd()) {
        QString line = stream.readLine().trimmed();

        // Skip comments and empty lines
        if (line.isEmpty() || line.startsWith('#')) {
            continue;
        }

        // Convert gitignore glob pattern to regex
        QString regexPattern = QRegularExpression::wildcardToRegularExpression(line);
        QRegularExpression regex(regexPattern);

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
bool MultiFileSearchWidget::isIgnored(const QString& filePath,
                                      const QVector<QRegularExpression>& patterns)
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
        fileItem = new QTreeWidgetItem();
        fileItem->setText(0, result.file);
        fileItem->setData(0, Qt::UserRole, result.file);
        fileItem->setFirstColumnSpanned(true);
        m_resultsTree->addTopLevelItem(fileItem);
    }

    // Add result as child item
    QTreeWidgetItem* resultItem = new QTreeWidgetItem(fileItem);
    resultItem->setText(0, QString("Line %1:%2").arg(result.line).arg(result.column + 1));
    resultItem->setText(1, result.lineText);

    // Store result data in item for later retrieval
    resultItem->setData(0, Qt::UserRole, result.file);
    resultItem->setData(0, Qt::UserRole + 1, result.line);
    resultItem->setData(0, Qt::UserRole + 2, result.column);
    resultItem->setData(0, Qt::UserRole + 3, result.lineText);
    resultItem->setData(0, Qt::UserRole + 4, result.matchedText);

    // Highlight the matched text in the context
    QString highlightedText = result.lineText;
    int matchStart = result.lineText.indexOf(result.matchedText);
    if (matchStart != -1) {
        resultItem->setText(1, highlightedText);
    }
}

/**
 * @brief Updates the status label with current search state.
 */
void MultiFileSearchWidget::updateStatus(const QString& message)
{
    m_statusLabel->setText(message);
}
