/**
 * @file documentation_widget.cpp
 * @brief Implementation of DocumentationWidget - full documentation browser
 */

#include "documentation_widget.h"
#include "integration/ProdIntegration.h"
#include "integration/InitializationTracker.h"
#include <QApplication>
#include <QClipboard>
#include <QDesktopServices>
#include <QFileInfo>
#include <QDir>
#include <QDirIterator>
#include <QMessageBox>
#include <QInputDialog>
#include <QTextStream>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QStandardPaths>
#include <QUuid>
#include <QDebug>

// ============================================================================
// DocIndexer Implementation
// ============================================================================

DocIndexer::DocIndexer(QObject* parent)
    : QObject(parent)
{
}

void DocIndexer::indexSource(const DocSource& source)
{
    RawrXD::Integration::ScopedTimer timer("DocIndexer", "indexSource", source.id.toUtf8().constData());
    emit indexingStarted(source.id);
    
    QDir docDir(source.path);
    if (!docDir.exists()) {
        emit indexingError(source.id, tr("Documentation path does not exist: %1").arg(source.path));
        return;
    }
    
    QStringList filters;
    switch (source.type) {
        case DocSourceType::HTML:
        case DocSourceType::Doxygen:
        case DocSourceType::DevDocs:
            filters << "*.html" << "*.htm";
            break;
        case DocSourceType::Markdown:
            filters << "*.md" << "*.markdown";
            break;
        case DocSourceType::QtHelp:
            filters << "*.qch";
            break;
        default:
            filters << "*.html" << "*.htm" << "*.md";
            break;
    }
    
    QFileInfoList files = docDir.entryInfoList(filters, QDir::Files, QDir::Name);
    
    // Also search subdirectories
    QDirIterator it(source.path, filters, QDir::Files, QDirIterator::Subdirectories);
    QStringList allFiles;
    while (it.hasNext()) {
        allFiles << it.next();
    }
    
    int total = allFiles.size();
    int current = 0;
    
    QMutexLocker locker(&m_mutex);
    
    for (const QString& filePath : allFiles) {
        current++;
        emit indexingProgress(source.id, current, total);
        
        if (filePath.endsWith(".html") || filePath.endsWith(".htm")) {
            indexHtmlFile(filePath, source.id);
        } else if (filePath.endsWith(".md") || filePath.endsWith(".markdown")) {
            indexMarkdownFile(filePath, source.id);
        } else if (filePath.endsWith(".qch")) {
            indexQtHelpFile(filePath, source.id);
        }
    }
    
    emit indexingFinished(source.id);
}

void DocIndexer::removeSource(const QString& sourceId)
{
    QMutexLocker locker(&m_mutex);
    m_index.remove(sourceId);
}

void DocIndexer::clearIndex()
{
    QMutexLocker locker(&m_mutex);
    m_index.clear();
}

void DocIndexer::indexHtmlFile(const QString& filePath, const QString& sourceId)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return;
    }
    
    QString content = QString::fromUtf8(file.readAll());
    file.close();
    
    // Extract title
    QString title;
    QRegularExpression titleRegex("<title>([^<]+)</title>", QRegularExpression::CaseInsensitiveOption);
    QRegularExpressionMatch titleMatch = titleRegex.match(content);
    if (titleMatch.hasMatch()) {
        title = titleMatch.captured(1).trimmed();
    } else {
        title = QFileInfo(filePath).baseName();
    }
    
    // Strip HTML tags for content indexing
    QString plainContent = content;
    plainContent.remove(QRegularExpression("<script[^>]*>.*?</script>", QRegularExpression::DotMatchesEverythingOption));
    plainContent.remove(QRegularExpression("<style[^>]*>.*?</style>", QRegularExpression::DotMatchesEverythingOption));
    plainContent.remove(QRegularExpression("<[^>]+>"));
    plainContent = plainContent.simplified();
    
    // Extract keywords from headings
    QStringList keywords;
    QRegularExpression headingRegex("<h[1-6][^>]*>([^<]+)</h[1-6]>", QRegularExpression::CaseInsensitiveOption);
    QRegularExpressionMatchIterator headingIt = headingRegex.globalMatch(content);
    while (headingIt.hasNext()) {
        QRegularExpressionMatch match = headingIt.next();
        keywords << match.captured(1).trimmed().toLower();
    }
    
    IndexEntry entry;
    entry.url = QUrl::fromLocalFile(filePath).toString();
    entry.title = title;
    entry.content = plainContent;
    entry.sourceId = sourceId;
    entry.keywords = keywords;
    
    m_index[sourceId].append(entry);
}

void DocIndexer::indexMarkdownFile(const QString& filePath, const QString& sourceId)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return;
    }
    
    QString content = QString::fromUtf8(file.readAll());
    file.close();
    
    // Extract title from first heading
    QString title;
    QRegularExpression titleRegex("^#\\s+(.+)$", QRegularExpression::MultilineOption);
    QRegularExpressionMatch titleMatch = titleRegex.match(content);
    if (titleMatch.hasMatch()) {
        title = titleMatch.captured(1).trimmed();
    } else {
        title = QFileInfo(filePath).baseName();
    }
    
    // Extract keywords from headings
    QStringList keywords;
    QRegularExpression headingRegex("^#{1,6}\\s+(.+)$", QRegularExpression::MultilineOption);
    QRegularExpressionMatchIterator headingIt = headingRegex.globalMatch(content);
    while (headingIt.hasNext()) {
        QRegularExpressionMatch match = headingIt.next();
        keywords << match.captured(1).trimmed().toLower();
    }
    
    IndexEntry entry;
    entry.url = QUrl::fromLocalFile(filePath).toString();
    entry.title = title;
    entry.content = content;
    entry.sourceId = sourceId;
    entry.keywords = keywords;
    
    m_index[sourceId].append(entry);
}

void DocIndexer::indexQtHelpFile(const QString& filePath, const QString& sourceId)
{
    // Qt Help files require QHelpEngine - basic implementation
    // In production, would use QHelpEngine to properly index .qch files
    
    IndexEntry entry;
    entry.url = QString("qthelp://%1").arg(QFileInfo(filePath).baseName());
    entry.title = QFileInfo(filePath).baseName();
    entry.content = "Qt Help Collection";
    entry.sourceId = sourceId;
    
    m_index[sourceId].append(entry);
}

QList<DocSearchResult> DocIndexer::search(const QString& query, const QStringList& sourceIds)
{
    QList<DocSearchResult> results;
    QStringList queryTerms = query.toLower().split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
    
    if (queryTerms.isEmpty()) {
        return results;
    }
    
    QMutexLocker locker(&m_mutex);
    
    QStringList searchSources = sourceIds.isEmpty() ? m_index.keys() : sourceIds;
    
    for (const QString& sourceId : searchSources) {
        if (!m_index.contains(sourceId)) {
            continue;
        }
        
        for (const IndexEntry& entry : m_index[sourceId]) {
            double relevance = calculateRelevance(entry, queryTerms);
            if (relevance > 0) {
                DocSearchResult result;
                result.title = entry.title;
                result.url = entry.url;
                result.sourceId = sourceId;
                result.relevanceScore = relevance;
                
                // Generate snippet with context around first match
                int snippetStart = 0;
                for (const QString& term : queryTerms) {
                    int pos = entry.content.toLower().indexOf(term);
                    if (pos >= 0) {
                        snippetStart = qMax(0, pos - 50);
                        break;
                    }
                }
                
                result.snippet = entry.content.mid(snippetStart, 200);
                if (snippetStart > 0) {
                    result.snippet.prepend("...");
                }
                if (snippetStart + 200 < entry.content.length()) {
                    result.snippet.append("...");
                }
                
                results.append(result);
            }
        }
    }
    
    // Sort by relevance
    std::sort(results.begin(), results.end(), [](const DocSearchResult& a, const DocSearchResult& b) {
        return a.relevanceScore > b.relevanceScore;
    });
    
    return results;
}

double DocIndexer::calculateRelevance(const IndexEntry& entry, const QStringList& queryTerms)
{
    double score = 0.0;
    QString titleLower = entry.title.toLower();
    QString contentLower = entry.content.toLower();
    
    for (const QString& term : queryTerms) {
        // Title matches are worth more
        if (titleLower.contains(term)) {
            score += 10.0;
            if (titleLower.startsWith(term)) {
                score += 5.0;
            }
        }
        
        // Keyword matches
        for (const QString& keyword : entry.keywords) {
            if (keyword.contains(term)) {
                score += 3.0;
            }
        }
        
        // Content matches
        int count = 0;
        int pos = 0;
        while ((pos = contentLower.indexOf(term, pos)) != -1) {
            count++;
            pos += term.length();
        }
        score += count * 0.1;
    }
    
    return score;
}

// ============================================================================
// DocumentationWidget Implementation
// ============================================================================

DocumentationWidget::DocumentationWidget(QWidget* parent)
    : QWidget(parent)
    , m_mainLayout(nullptr)
    , m_toolbar(nullptr)
    , m_splitter(nullptr)
    , m_backAction(nullptr)
    , m_forwardAction(nullptr)
    , m_homeAction(nullptr)
    , m_refreshAction(nullptr)
    , m_bookmarkAction(nullptr)
    , m_printAction(nullptr)
    , m_zoomInAction(nullptr)
    , m_zoomOutAction(nullptr)
    , m_searchEdit(nullptr)
    , m_sourceFilterCombo(nullptr)
    , m_searchButton(nullptr)
    , m_sidebarTabs(nullptr)
    , m_contentsTree(nullptr)
    , m_searchResults(nullptr)
    , m_bookmarksList(nullptr)
    , m_historyList(nullptr)
    , m_contentTabs(nullptr)
    , m_currentBrowser(nullptr)
    , m_progressBar(nullptr)
    , m_statusLabel(nullptr)
    , m_historyIndex(-1)
    , m_indexer(std::make_unique<DocIndexer>())
    , m_indexerThread(nullptr)
    , m_fontSize(12)
    , m_theme("light")
    , m_homeUrl("about:blank")
    , m_searchTimer(nullptr)
{
    RawrXD::Integration::ScopedInitTimer init("DocumentationWidget");
    setupUI();
    setupConnections();
    loadSettings();
    setupDefaultSources();
    
    qDebug() << "DocumentationWidget initialized";
}

DocumentationWidget::~DocumentationWidget()
{
    saveSettings();
    
    if (m_indexerThread) {
        m_indexerThread->quit();
        m_indexerThread->wait();
    }
}

void DocumentationWidget::setupUI()
{
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(0, 0, 0, 0);
    m_mainLayout->setSpacing(0);
    
    setupToolbar();
    
    // Main splitter
    m_splitter = new QSplitter(Qt::Horizontal, this);
    m_mainLayout->addWidget(m_splitter);
    
    setupSidebar();
    setupContentArea();
    
    // Status bar area
    QWidget* statusWidget = new QWidget(this);
    QHBoxLayout* statusLayout = new QHBoxLayout(statusWidget);
    statusLayout->setContentsMargins(4, 2, 4, 2);
    
    m_statusLabel = new QLabel(tr("Ready"), statusWidget);
    m_progressBar = new QProgressBar(statusWidget);
    m_progressBar->setMaximumWidth(200);
    m_progressBar->setVisible(false);
    
    statusLayout->addWidget(m_statusLabel);
    statusLayout->addStretch();
    statusLayout->addWidget(m_progressBar);
    
    m_mainLayout->addWidget(statusWidget);
    
    // Search timer for delayed search
    m_searchTimer = new QTimer(this);
    m_searchTimer->setSingleShot(true);
    m_searchTimer->setInterval(SEARCH_DELAY_MS);
    
    // Set initial splitter sizes
    m_splitter->setSizes({250, 750});
}

void DocumentationWidget::setupToolbar()
{
    m_toolbar = new QToolBar(tr("Documentation"), this);
    m_toolbar->setIconSize(QSize(18, 18));
    m_mainLayout->addWidget(m_toolbar);
    
    // Navigation actions
    m_backAction = m_toolbar->addAction(QIcon::fromTheme("go-previous", QIcon(":/icons/back.png")), tr("Back"));
    m_backAction->setShortcut(QKeySequence::Back);
    m_backAction->setEnabled(false);
    
    m_forwardAction = m_toolbar->addAction(QIcon::fromTheme("go-next", QIcon(":/icons/forward.png")), tr("Forward"));
    m_forwardAction->setShortcut(QKeySequence::Forward);
    m_forwardAction->setEnabled(false);
    
    m_homeAction = m_toolbar->addAction(QIcon::fromTheme("go-home", QIcon(":/icons/home.png")), tr("Home"));
    m_homeAction->setShortcut(QKeySequence(Qt::ALT | Qt::Key_Home));
    
    m_refreshAction = m_toolbar->addAction(QIcon::fromTheme("view-refresh", QIcon(":/icons/refresh.png")), tr("Refresh"));
    m_refreshAction->setShortcut(QKeySequence::Refresh);
    
    m_toolbar->addSeparator();
    
    // Search bar
    m_sourceFilterCombo = new QComboBox(this);
    m_sourceFilterCombo->addItem(tr("All Sources"), "");
    m_sourceFilterCombo->setMinimumWidth(120);
    m_toolbar->addWidget(m_sourceFilterCombo);
    
    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText(tr("Search documentation..."));
    m_searchEdit->setMinimumWidth(200);
    m_searchEdit->setClearButtonEnabled(true);
    m_toolbar->addWidget(m_searchEdit);
    
    m_searchButton = new QPushButton(tr("Search"), this);
    m_toolbar->addWidget(m_searchButton);
    
    m_toolbar->addSeparator();
    
    // Bookmark action
    m_bookmarkAction = m_toolbar->addAction(QIcon::fromTheme("bookmark-new", QIcon(":/icons/bookmark.png")), tr("Bookmark"));
    m_bookmarkAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_D));
    
    m_toolbar->addSeparator();
    
    // Zoom actions
    m_zoomInAction = m_toolbar->addAction(QIcon::fromTheme("zoom-in", QIcon(":/icons/zoom-in.png")), tr("Zoom In"));
    m_zoomInAction->setShortcut(QKeySequence::ZoomIn);
    
    m_zoomOutAction = m_toolbar->addAction(QIcon::fromTheme("zoom-out", QIcon(":/icons/zoom-out.png")), tr("Zoom Out"));
    m_zoomOutAction->setShortcut(QKeySequence::ZoomOut);
    
    // Print action
    m_printAction = m_toolbar->addAction(QIcon::fromTheme("document-print", QIcon(":/icons/print.png")), tr("Print"));
    m_printAction->setShortcut(QKeySequence::Print);
}

void DocumentationWidget::setupSidebar()
{
    m_sidebarTabs = new QTabWidget(this);
    m_splitter->addWidget(m_sidebarTabs);
    
    // Contents tree
    m_contentsTree = new QTreeWidget(this);
    m_contentsTree->setHeaderLabel(tr("Contents"));
    m_contentsTree->setRootIsDecorated(true);
    m_contentsTree->setContextMenuPolicy(Qt::CustomContextMenu);
    m_sidebarTabs->addTab(m_contentsTree, tr("Contents"));
    
    // Search results
    m_searchResults = new QListWidget(this);
    m_searchResults->setAlternatingRowColors(true);
    m_sidebarTabs->addTab(m_searchResults, tr("Search"));
    
    // Bookmarks
    m_bookmarksList = new QListWidget(this);
    m_bookmarksList->setAlternatingRowColors(true);
    m_bookmarksList->setContextMenuPolicy(Qt::CustomContextMenu);
    m_sidebarTabs->addTab(m_bookmarksList, tr("Bookmarks"));
    
    // History
    m_historyList = new QListWidget(this);
    m_historyList->setAlternatingRowColors(true);
    m_sidebarTabs->addTab(m_historyList, tr("History"));
}

void DocumentationWidget::setupContentArea()
{
    m_contentTabs = new QTabWidget(this);
    m_contentTabs->setTabsClosable(true);
    m_contentTabs->setMovable(true);
    m_splitter->addWidget(m_contentTabs);
    
    // Create initial browser
    m_currentBrowser = new QTextBrowser(this);
    m_currentBrowser->setOpenExternalLinks(false);
    m_currentBrowser->setOpenLinks(false);
    m_currentBrowser->setContextMenuPolicy(Qt::CustomContextMenu);
    
    m_contentTabs->addTab(m_currentBrowser, tr("Welcome"));
    
    // Set welcome content
    QString welcomeHtml = R"(
        <html>
        <head>
            <style>
                body { font-family: 'Segoe UI', sans-serif; padding: 20px; background: #f5f5f5; }
                h1 { color: #333; }
                .quick-links { margin-top: 20px; }
                .quick-links a { display: inline-block; margin: 5px 10px; padding: 10px 20px; 
                    background: #007acc; color: white; text-decoration: none; border-radius: 4px; }
                .quick-links a:hover { background: #005999; }
            </style>
        </head>
        <body>
            <h1>📚 Documentation Browser</h1>
            <p>Welcome to the integrated documentation browser. You can:</p>
            <ul>
                <li>Browse documentation from multiple sources</li>
                <li>Search across all documentation</li>
                <li>Bookmark important pages</li>
                <li>Copy code examples directly to your project</li>
            </ul>
            <div class="quick-links">
                <h3>Quick Links</h3>
                <a href="doc://qt">Qt Documentation</a>
                <a href="doc://cpp">C++ Reference</a>
                <a href="doc://python">Python Docs</a>
            </div>
        </body>
        </html>
    )";
    m_currentBrowser->setHtml(welcomeHtml);
}

void DocumentationWidget::setupConnections()
{
    // Navigation
    connect(m_backAction, &QAction::triggered, this, &DocumentationWidget::navigateBack);
    connect(m_forwardAction, &QAction::triggered, this, &DocumentationWidget::navigateForward);
    connect(m_homeAction, &QAction::triggered, this, &DocumentationWidget::goHome);
    connect(m_refreshAction, &QAction::triggered, this, &DocumentationWidget::refresh);
    
    // Search
    connect(m_searchEdit, &QLineEdit::textChanged, this, &DocumentationWidget::onSearchTextChanged);
    connect(m_searchEdit, &QLineEdit::returnPressed, this, &DocumentationWidget::performSearch);
    connect(m_searchButton, &QPushButton::clicked, this, &DocumentationWidget::performSearch);
    connect(m_searchTimer, &QTimer::timeout, this, &DocumentationWidget::performSearch);
    connect(m_searchResults, &QListWidget::itemClicked, this, &DocumentationWidget::onSearchResultClicked);
    
    // Source filter
    connect(m_sourceFilterCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &DocumentationWidget::onSourceFilterChanged);
    
    // Tree navigation
    connect(m_contentsTree, &QTreeWidget::itemClicked, this, &DocumentationWidget::onTreeItemClicked);
    connect(m_contentsTree, &QTreeWidget::customContextMenuRequested, this, &DocumentationWidget::showContextMenu);
    
    // Bookmarks
    connect(m_bookmarkAction, &QAction::triggered, this, &DocumentationWidget::addCurrentPageToBookmarks);
    connect(m_bookmarksList, &QListWidget::itemClicked, this, &DocumentationWidget::onBookmarkClicked);
    connect(m_bookmarksList, &QListWidget::customContextMenuRequested, this, &DocumentationWidget::showContextMenu);
    
    // History
    connect(m_historyList, &QListWidget::itemClicked, this, &DocumentationWidget::onHistoryItemClicked);
    
    // Browser
    connect(m_currentBrowser, &QTextBrowser::anchorClicked, this, &DocumentationWidget::onLinkClicked);
    connect(m_currentBrowser, &QTextBrowser::customContextMenuRequested, this, &DocumentationWidget::showContextMenu);
    
    // Tabs
    connect(m_contentTabs, &QTabWidget::tabCloseRequested, this, &DocumentationWidget::onTabCloseRequested);
    
    // Zoom
    connect(m_zoomInAction, &QAction::triggered, this, [this]() {
        setFontSize(m_fontSize + 1);
    });
    connect(m_zoomOutAction, &QAction::triggered, this, [this]() {
        setFontSize(m_fontSize - 1);
    });
    
    // Indexer connections
    connect(m_indexer.get(), &DocIndexer::indexingStarted, this, [this](const QString& sourceId) {
        m_statusLabel->setText(tr("Indexing %1...").arg(sourceId));
        m_progressBar->setVisible(true);
        m_progressBar->setValue(0);
    });
    
    connect(m_indexer.get(), &DocIndexer::indexingProgress, this, [this](const QString&, int current, int total) {
        m_progressBar->setMaximum(total);
        m_progressBar->setValue(current);
    });
    
    connect(m_indexer.get(), &DocIndexer::indexingFinished, this, [this](const QString&) {
        m_statusLabel->setText(tr("Ready"));
        m_progressBar->setVisible(false);
    });
}

void DocumentationWidget::setupDefaultSources()
{
    // Add default documentation sources
    QString docsPath = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/Documentation";
    
    // Qt Documentation
    DocSource qtSource;
    qtSource.id = "qt";
    qtSource.name = "Qt Documentation";
    qtSource.version = "6.6";
    qtSource.path = docsPath + "/Qt";
    qtSource.type = DocSourceType::QtHelp;
    qtSource.isEnabled = true;
    qtSource.isDownloaded = QDir(qtSource.path).exists();
    qtSource.description = "Official Qt Framework documentation";
    addDocSource(qtSource);
    
    // C++ Reference
    DocSource cppSource;
    cppSource.id = "cpp";
    cppSource.name = "C++ Reference";
    cppSource.version = "C++20";
    cppSource.path = docsPath + "/CppReference";
    cppSource.type = DocSourceType::HTML;
    cppSource.isEnabled = true;
    cppSource.isDownloaded = QDir(cppSource.path).exists();
    cppSource.description = "C++ standard library and language reference";
    addDocSource(cppSource);
    
    // Python Docs
    DocSource pythonSource;
    pythonSource.id = "python";
    pythonSource.name = "Python Documentation";
    pythonSource.version = "3.12";
    pythonSource.path = docsPath + "/Python";
    pythonSource.type = DocSourceType::HTML;
    pythonSource.isEnabled = true;
    pythonSource.isDownloaded = QDir(pythonSource.path).exists();
    pythonSource.description = "Official Python documentation";
    addDocSource(pythonSource);
    
    updateSourceTree();
}

void DocumentationWidget::addDocSource(const DocSource& source)
{
    m_docSources[source.id] = source;
    
    // Add to filter combo
    m_sourceFilterCombo->addItem(source.name, source.id);
    
    // Index if enabled and downloaded
    if (source.isEnabled && source.isDownloaded) {
        m_indexer->indexSource(source);
    }
    
    updateSourceTree();
    emit docSourceAdded(source.id);
    
    qDebug() << "Added documentation source:" << source.name;
}

void DocumentationWidget::removeDocSource(const QString& sourceId)
{
    if (!m_docSources.contains(sourceId)) {
        return;
    }
    
    m_docSources.remove(sourceId);
    m_indexer->removeSource(sourceId);
    
    // Remove from filter combo
    for (int i = 0; i < m_sourceFilterCombo->count(); ++i) {
        if (m_sourceFilterCombo->itemData(i).toString() == sourceId) {
            m_sourceFilterCombo->removeItem(i);
            break;
        }
    }
    
    updateSourceTree();
    emit docSourceRemoved(sourceId);
}

void DocumentationWidget::enableDocSource(const QString& sourceId, bool enabled)
{
    if (!m_docSources.contains(sourceId)) {
        return;
    }
    
    m_docSources[sourceId].isEnabled = enabled;
    
    if (enabled && m_docSources[sourceId].isDownloaded) {
        m_indexer->indexSource(m_docSources[sourceId]);
    } else {
        m_indexer->removeSource(sourceId);
    }
    
    updateSourceTree();
}

QList<DocSource> DocumentationWidget::getDocSources() const
{
    return m_docSources.values();
}

void DocumentationWidget::navigateTo(const QString& url)
{
    loadDocPage(url);
}

void DocumentationWidget::navigateBack()
{
    if (m_historyIndex > 0) {
        m_historyIndex--;
        const DocHistoryEntry& entry = m_history[m_historyIndex];
        loadDocPage(entry.url);
        updateNavigationButtons();
    }
}

void DocumentationWidget::navigateForward()
{
    if (m_historyIndex < m_history.size() - 1) {
        m_historyIndex++;
        const DocHistoryEntry& entry = m_history[m_historyIndex];
        loadDocPage(entry.url);
        updateNavigationButtons();
    }
}

void DocumentationWidget::goHome()
{
    QString welcomeHtml = R"(
        <html>
        <head>
            <style>
                body { font-family: 'Segoe UI', sans-serif; padding: 20px; }
                h1 { color: #333; }
            </style>
        </head>
        <body>
            <h1>📚 Documentation Browser</h1>
            <p>Select a documentation source from the sidebar to get started.</p>
        </body>
        </html>
    )";
    m_currentBrowser->setHtml(welcomeHtml);
    addToHistory("about:home", tr("Home"));
}

void DocumentationWidget::search(const QString& query)
{
    m_searchEdit->setText(query);
    performSearch();
}

void DocumentationWidget::clearSearch()
{
    m_searchEdit->clear();
    m_searchResults->clear();
    m_lastSearchQuery.clear();
    m_lastSearchResults.clear();
}

void DocumentationWidget::addBookmark(const DocBookmark& bookmark)
{
    DocBookmark newBookmark = bookmark;
    if (newBookmark.id.isEmpty()) {
        newBookmark.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    }
    if (!newBookmark.created.isValid()) {
        newBookmark.created = QDateTime::currentDateTime();
    }
    
    m_bookmarks.append(newBookmark);
    updateBookmarksList();
    
    emit bookmarkAdded(newBookmark.id);
    
    qDebug() << "Added bookmark:" << newBookmark.title;
}

void DocumentationWidget::removeBookmark(const QString& bookmarkId)
{
    for (int i = 0; i < m_bookmarks.size(); ++i) {
        if (m_bookmarks[i].id == bookmarkId) {
            m_bookmarks.removeAt(i);
            updateBookmarksList();
            emit bookmarkRemoved(bookmarkId);
            break;
        }
    }
}

QList<DocBookmark> DocumentationWidget::getBookmarks() const
{
    return m_bookmarks;
}

QList<DocHistoryEntry> DocumentationWidget::getHistory() const
{
    return m_history;
}

void DocumentationWidget::clearHistory()
{
    m_history.clear();
    m_historyIndex = -1;
    updateHistoryList();
    updateNavigationButtons();
}

void DocumentationWidget::setFontSize(int size)
{
    m_fontSize = qBound(8, size, 32);
    
    QFont font = m_currentBrowser->font();
    font.setPointSize(m_fontSize);
    m_currentBrowser->setFont(font);
}

int DocumentationWidget::getFontSize() const
{
    return m_fontSize;
}

void DocumentationWidget::setTheme(const QString& theme)
{
    m_theme = theme;
    // Apply theme stylesheet
    if (theme == "dark") {
        m_currentBrowser->setStyleSheet("background-color: #1e1e1e; color: #d4d4d4;");
    } else {
        m_currentBrowser->setStyleSheet("");
    }
}

void DocumentationWidget::saveState(QSettings& settings)
{
    settings.beginGroup("DocumentationWidget");
    settings.setValue("splitterSizes", m_splitter->saveState());
    settings.setValue("fontSize", m_fontSize);
    settings.setValue("theme", m_theme);
    settings.setValue("sidebarTab", m_sidebarTabs->currentIndex());
    
    // Save bookmarks
    QJsonArray bookmarksArray;
    for (const DocBookmark& bookmark : m_bookmarks) {
        QJsonObject obj;
        obj["id"] = bookmark.id;
        obj["title"] = bookmark.title;
        obj["url"] = bookmark.url;
        obj["sourceId"] = bookmark.sourceId;
        obj["category"] = bookmark.category;
        obj["notes"] = bookmark.notes;
        obj["created"] = bookmark.created.toString(Qt::ISODate);
        obj["tags"] = QJsonArray::fromStringList(bookmark.tags);
        bookmarksArray.append(obj);
    }
    settings.setValue("bookmarks", QJsonDocument(bookmarksArray).toJson());
    
    settings.endGroup();
}

void DocumentationWidget::restoreState(QSettings& settings)
{
    settings.beginGroup("DocumentationWidget");
    
    if (settings.contains("splitterSizes")) {
        m_splitter->restoreState(settings.value("splitterSizes").toByteArray());
    }
    
    m_fontSize = settings.value("fontSize", 12).toInt();
    setFontSize(m_fontSize);
    
    m_theme = settings.value("theme", "light").toString();
    setTheme(m_theme);
    
    m_sidebarTabs->setCurrentIndex(settings.value("sidebarTab", 0).toInt());
    
    // Restore bookmarks
    QByteArray bookmarksData = settings.value("bookmarks").toByteArray();
    if (!bookmarksData.isEmpty()) {
        QJsonDocument doc = QJsonDocument::fromJson(bookmarksData);
        QJsonArray bookmarksArray = doc.array();
        
        m_bookmarks.clear();
        for (const QJsonValue& value : bookmarksArray) {
            QJsonObject obj = value.toObject();
            DocBookmark bookmark;
            bookmark.id = obj["id"].toString();
            bookmark.title = obj["title"].toString();
            bookmark.url = obj["url"].toString();
            bookmark.sourceId = obj["sourceId"].toString();
            bookmark.category = obj["category"].toString();
            bookmark.notes = obj["notes"].toString();
            bookmark.created = QDateTime::fromString(obj["created"].toString(), Qt::ISODate);
            
            QJsonArray tagsArray = obj["tags"].toArray();
            for (const QJsonValue& tag : tagsArray) {
                bookmark.tags << tag.toString();
            }
            
            m_bookmarks.append(bookmark);
        }
        updateBookmarksList();
    }
    
    settings.endGroup();
}

void DocumentationWidget::refresh()
{
    if (m_historyIndex >= 0 && m_historyIndex < m_history.size()) {
        loadDocPage(m_history[m_historyIndex].url);
    }
}

void DocumentationWidget::downloadDocSource(const QString& sourceId)
{
    if (!m_docSources.contains(sourceId)) {
        return;
    }
    
    // In production, would download from official sources
    m_statusLabel->setText(tr("Downloading %1...").arg(m_docSources[sourceId].name));
    m_progressBar->setVisible(true);
    m_progressBar->setValue(0);
    m_progressBar->setMaximum(100);
    
    // Simulate download progress
    QTimer* downloadTimer = new QTimer(this);
    int* progress = new int(0);
    
    connect(downloadTimer, &QTimer::timeout, this, [this, sourceId, downloadTimer, progress]() {
        (*progress) += 10;
        m_progressBar->setValue(*progress);
        emit downloadProgress(sourceId, *progress, 100);
        
        if (*progress >= 100) {
            downloadTimer->stop();
            downloadTimer->deleteLater();
            delete progress;
            
            m_docSources[sourceId].isDownloaded = true;
            m_statusLabel->setText(tr("Download complete"));
            m_progressBar->setVisible(false);
            
            // Create directory and index
            QDir().mkpath(m_docSources[sourceId].path);
            m_indexer->indexSource(m_docSources[sourceId]);
            
            emit downloadFinished(sourceId, true);
        }
    });
    
    downloadTimer->start(100);
}

void DocumentationWidget::cancelDownload()
{
    m_statusLabel->setText(tr("Download cancelled"));
    m_progressBar->setVisible(false);
}

void DocumentationWidget::onSearchTextChanged(const QString& text)
{
    Q_UNUSED(text)
    m_searchTimer->start();
}

void DocumentationWidget::onSearchResultClicked(QListWidgetItem* item)
{
    QString url = item->data(Qt::UserRole).toString();
    if (!url.isEmpty()) {
        navigateTo(url);
    }
}

void DocumentationWidget::onTreeItemClicked(QTreeWidgetItem* item, int column)
{
    Q_UNUSED(column)
    QString url = item->data(0, Qt::UserRole).toString();
    if (!url.isEmpty()) {
        navigateTo(url);
    }
}

void DocumentationWidget::onLinkClicked(const QUrl& url)
{
    QString urlStr = url.toString();
    
    // Handle internal documentation links
    if (urlStr.startsWith("doc://")) {
        QString sourceId = urlStr.mid(6);
        if (m_docSources.contains(sourceId)) {
            // Navigate to source index
            QString indexPath = m_docSources[sourceId].path + "/index.html";
            if (QFile::exists(indexPath)) {
                navigateTo(QUrl::fromLocalFile(indexPath).toString());
            }
        }
        return;
    }
    
    // Handle external links
    if (url.scheme() == "http" || url.scheme() == "https") {
        QDesktopServices::openUrl(url);
        return;
    }
    
    // Handle file links
    navigateTo(urlStr);
}

void DocumentationWidget::onBookmarkClicked(QListWidgetItem* item)
{
    QString url = item->data(Qt::UserRole).toString();
    if (!url.isEmpty()) {
        navigateTo(url);
    }
}

void DocumentationWidget::onHistoryItemClicked(QListWidgetItem* item)
{
    int index = item->data(Qt::UserRole).toInt();
    if (index >= 0 && index < m_history.size()) {
        m_historyIndex = index;
        loadDocPage(m_history[index].url);
        updateNavigationButtons();
    }
}

void DocumentationWidget::onTabCloseRequested(int index)
{
    if (m_contentTabs->count() > 1) {
        QWidget* widget = m_contentTabs->widget(index);
        m_contentTabs->removeTab(index);
        widget->deleteLater();
        
        // Update current browser reference
        m_currentBrowser = qobject_cast<QTextBrowser*>(m_contentTabs->currentWidget());
    }
}

void DocumentationWidget::onSourceFilterChanged(int index)
{
    Q_UNUSED(index)
    if (!m_lastSearchQuery.isEmpty()) {
        performSearch();
    }
}

void DocumentationWidget::performSearch()
{
    QString query = m_searchEdit->text().trimmed();
    if (query.isEmpty()) {
        m_searchResults->clear();
        return;
    }
    
    m_lastSearchQuery = query;
    
    // Get selected source filter
    QStringList sourceIds;
    QString selectedSource = m_sourceFilterCombo->currentData().toString();
    if (!selectedSource.isEmpty()) {
        sourceIds << selectedSource;
    }
    
    // Perform search
    m_lastSearchResults = m_indexer->search(query, sourceIds);
    
    // Update results list
    m_searchResults->clear();
    for (const DocSearchResult& result : m_lastSearchResults) {
        QListWidgetItem* item = new QListWidgetItem(m_searchResults);
        item->setText(result.title);
        item->setToolTip(result.snippet);
        item->setData(Qt::UserRole, result.url);
        
        // Add source indicator
        if (m_docSources.contains(result.sourceId)) {
            item->setIcon(QIcon::fromTheme("help-contents"));
        }
    }
    
    // Switch to search tab
    m_sidebarTabs->setCurrentWidget(m_searchResults);
    
    m_statusLabel->setText(tr("Found %1 results").arg(m_lastSearchResults.size()));
    emit searchCompleted(m_lastSearchResults.size());
}

void DocumentationWidget::showContextMenu(const QPoint& pos)
{
    QWidget* sender = qobject_cast<QWidget*>(this->sender());
    if (!sender) return;
    
    QMenu menu(this);
    
    if (sender == m_currentBrowser) {
        menu.addAction(tr("Copy"), m_currentBrowser, &QTextBrowser::copy);
        menu.addAction(tr("Select All"), m_currentBrowser, &QTextBrowser::selectAll);
        menu.addSeparator();
        menu.addAction(tr("Bookmark This Page"), this, &DocumentationWidget::addCurrentPageToBookmarks);
        
        // Check for code blocks
        if (!m_currentCodeExamples.isEmpty()) {
            menu.addSeparator();
            menu.addAction(tr("Copy Code Example"), this, &DocumentationWidget::copyCodeExample);
            menu.addAction(tr("Insert Code Example"), this, &DocumentationWidget::insertCodeExample);
        }
    } else if (sender == m_bookmarksList) {
        QListWidgetItem* item = m_bookmarksList->itemAt(pos);
        if (item) {
            QString bookmarkId = item->data(Qt::UserRole + 1).toString();
            menu.addAction(tr("Open"), this, [this, item]() {
                onBookmarkClicked(item);
            });
            menu.addAction(tr("Delete"), this, [this, bookmarkId]() {
                removeBookmark(bookmarkId);
            });
        }
    }
    
    if (!menu.isEmpty()) {
        menu.exec(sender->mapToGlobal(pos));
    }
}

void DocumentationWidget::addCurrentPageToBookmarks()
{
    if (m_historyIndex < 0 || m_historyIndex >= m_history.size()) {
        return;
    }
    
    const DocHistoryEntry& current = m_history[m_historyIndex];
    
    bool ok;
    QString title = QInputDialog::getText(this, tr("Add Bookmark"),
        tr("Bookmark title:"), QLineEdit::Normal, current.title, &ok);
    
    if (ok && !title.isEmpty()) {
        DocBookmark bookmark;
        bookmark.title = title;
        bookmark.url = current.url;
        bookmark.sourceId = current.sourceId;
        addBookmark(bookmark);
    }
}

void DocumentationWidget::copyCodeExample()
{
    if (m_currentCodeExamples.isEmpty()) {
        return;
    }
    
    // Copy first code example to clipboard
    QApplication::clipboard()->setText(m_currentCodeExamples.first().code);
    m_statusLabel->setText(tr("Code example copied to clipboard"));
}

void DocumentationWidget::insertCodeExample()
{
    if (m_currentCodeExamples.isEmpty()) {
        return;
    }
    
    emit codeExampleSelected(m_currentCodeExamples.first());
}

void DocumentationWidget::updateNavigationButtons()
{
    m_backAction->setEnabled(m_historyIndex > 0);
    m_forwardAction->setEnabled(m_historyIndex < m_history.size() - 1);
}

void DocumentationWidget::updateSourceTree()
{
    m_contentsTree->clear();
    
    for (const DocSource& source : m_docSources) {
        QTreeWidgetItem* sourceItem = new QTreeWidgetItem(m_contentsTree);
        sourceItem->setText(0, source.name);
        sourceItem->setIcon(0, QIcon::fromTheme("help-contents"));
        sourceItem->setData(0, Qt::UserRole, source.path + "/index.html");
        
        if (!source.isDownloaded) {
            sourceItem->setText(0, source.name + tr(" (Not Downloaded)"));
            sourceItem->setForeground(0, Qt::gray);
        } else if (!source.isEnabled) {
            sourceItem->setText(0, source.name + tr(" (Disabled)"));
            sourceItem->setForeground(0, Qt::gray);
        } else {
            // Add child items for downloaded docs
            QDir docDir(source.path);
            QStringList htmlFiles = docDir.entryList(QStringList() << "*.html" << "*.htm", 
                                                      QDir::Files, QDir::Name);
            
            for (const QString& file : htmlFiles.mid(0, 20)) { // Limit initial items
                QTreeWidgetItem* fileItem = new QTreeWidgetItem(sourceItem);
                fileItem->setText(0, QFileInfo(file).baseName());
                fileItem->setData(0, Qt::UserRole, docDir.absoluteFilePath(file));
            }
        }
    }
}

void DocumentationWidget::updateBookmarksList()
{
    m_bookmarksList->clear();
    
    for (const DocBookmark& bookmark : m_bookmarks) {
        QListWidgetItem* item = new QListWidgetItem(m_bookmarksList);
        item->setText(bookmark.title);
        item->setToolTip(bookmark.url);
        item->setData(Qt::UserRole, bookmark.url);
        item->setData(Qt::UserRole + 1, bookmark.id);
        item->setIcon(QIcon::fromTheme("bookmark"));
    }
}

void DocumentationWidget::updateHistoryList()
{
    m_historyList->clear();
    
    // Show history in reverse order (newest first)
    for (int i = m_history.size() - 1; i >= 0; --i) {
        const DocHistoryEntry& entry = m_history[i];
        QListWidgetItem* item = new QListWidgetItem(m_historyList);
        item->setText(entry.title);
        item->setToolTip(entry.url + "\n" + entry.timestamp.toString());
        item->setData(Qt::UserRole, i);
        
        if (i == m_historyIndex) {
            item->setIcon(QIcon::fromTheme("go-jump"));
        }
    }
}

void DocumentationWidget::highlightSearchTerms(const QString& query)
{
    if (query.isEmpty()) {
        return;
    }
    
    // Highlight search terms in the browser
    QStringList terms = query.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
    
    for (const QString& term : terms) {
        QTextDocument* doc = m_currentBrowser->document();
        QTextCursor cursor(doc);
        
        QTextCharFormat highlightFormat;
        highlightFormat.setBackground(Qt::yellow);
        
        while (!cursor.isNull() && !cursor.atEnd()) {
            cursor = doc->find(term, cursor);
            if (!cursor.isNull()) {
                cursor.mergeCharFormat(highlightFormat);
            }
        }
    }
}

void DocumentationWidget::loadDocPage(const QString& url)
{
    QUrl parsedUrl(url);
    
    if (parsedUrl.isLocalFile()) {
        QString filePath = parsedUrl.toLocalFile();
        QFile file(filePath);
        
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QString content = QString::fromUtf8(file.readAll());
            file.close();
            
            // Convert markdown if needed
            if (filePath.endsWith(".md") || filePath.endsWith(".markdown")) {
                content = convertMarkdownToHtml(content);
            }
            
            m_currentBrowser->setHtml(content);
            
            // Extract code examples
            m_currentCodeExamples = extractCodeExamples(content);
            
            // Update tab title
            QString title = QFileInfo(filePath).baseName();
            int currentIndex = m_contentTabs->currentIndex();
            m_contentTabs->setTabText(currentIndex, title);
            
            // Add to history
            addToHistory(url, title);
            
            // Highlight search terms if applicable
            if (!m_lastSearchQuery.isEmpty()) {
                highlightSearchTerms(m_lastSearchQuery);
            }
            
            emit navigationChanged(url, title);
        } else {
            m_currentBrowser->setHtml(QString("<html><body><h1>Error</h1><p>Could not open file: %1</p></body></html>").arg(filePath));
        }
    } else if (url.startsWith("about:")) {
        // Internal pages
        goHome();
    } else {
        m_currentBrowser->setSource(parsedUrl);
    }
}

void DocumentationWidget::addToHistory(const QString& url, const QString& title)
{
    // Remove forward history if navigating from middle
    while (m_history.size() > m_historyIndex + 1) {
        m_history.removeLast();
    }
    
    DocHistoryEntry entry;
    entry.url = url;
    entry.title = title;
    entry.timestamp = QDateTime::currentDateTime();
    
    m_history.append(entry);
    m_historyIndex = m_history.size() - 1;
    
    // Limit history size
    while (m_history.size() > MAX_HISTORY_SIZE) {
        m_history.removeFirst();
        m_historyIndex--;
    }
    
    updateHistoryList();
    updateNavigationButtons();
}

QString DocumentationWidget::resolveDocPath(const QString& url, const QString& sourceId)
{
    if (m_docSources.contains(sourceId)) {
        return m_docSources[sourceId].path + "/" + url;
    }
    return url;
}

QList<CodeExample> DocumentationWidget::extractCodeExamples(const QString& html)
{
    QList<CodeExample> examples;
    
    // Extract <pre><code> blocks
    QRegularExpression codeRegex(
        "<pre[^>]*><code[^>]*(?:class=\"([^\"]*)\")?[^>]*>(.*?)</code></pre>",
        QRegularExpression::DotMatchesEverythingOption
    );
    
    QRegularExpressionMatchIterator it = codeRegex.globalMatch(html);
    while (it.hasNext()) {
        QRegularExpressionMatch match = it.next();
        
        CodeExample example;
        example.language = match.captured(1);
        example.code = match.captured(2);
        
        // Decode HTML entities
        example.code.replace("&lt;", "<");
        example.code.replace("&gt;", ">");
        example.code.replace("&amp;", "&");
        example.code.replace("&quot;", "\"");
        example.code.replace("&#39;", "'");
        
        examples.append(example);
    }
    
    return examples;
}

QString DocumentationWidget::convertMarkdownToHtml(const QString& markdown)
{
    QString html = markdown;
    
    // Basic markdown to HTML conversion
    // Headers
    html.replace(QRegularExpression("^######\\s+(.+)$", QRegularExpression::MultilineOption), "<h6>\\1</h6>");
    html.replace(QRegularExpression("^#####\\s+(.+)$", QRegularExpression::MultilineOption), "<h5>\\1</h5>");
    html.replace(QRegularExpression("^####\\s+(.+)$", QRegularExpression::MultilineOption), "<h4>\\1</h4>");
    html.replace(QRegularExpression("^###\\s+(.+)$", QRegularExpression::MultilineOption), "<h3>\\1</h3>");
    html.replace(QRegularExpression("^##\\s+(.+)$", QRegularExpression::MultilineOption), "<h2>\\1</h2>");
    html.replace(QRegularExpression("^#\\s+(.+)$", QRegularExpression::MultilineOption), "<h1>\\1</h1>");
    
    // Bold and italic
    html.replace(QRegularExpression("\\*\\*\\*(.+?)\\*\\*\\*"), "<strong><em>\\1</em></strong>");
    html.replace(QRegularExpression("\\*\\*(.+?)\\*\\*"), "<strong>\\1</strong>");
    html.replace(QRegularExpression("\\*(.+?)\\*"), "<em>\\1</em>");
    
    // Code blocks
    html.replace(QRegularExpression("```(\\w*)\\n([\\s\\S]*?)```"), "<pre><code class=\"\\1\">\\2</code></pre>");
    html.replace(QRegularExpression("`([^`]+)`"), "<code>\\1</code>");
    
    // Links
    html.replace(QRegularExpression("\\[([^\\]]+)\\]\\(([^)]+)\\)"), "<a href=\"\\2\">\\1</a>");
    
    // Lists
    html.replace(QRegularExpression("^\\*\\s+(.+)$", QRegularExpression::MultilineOption), "<li>\\1</li>");
    html.replace(QRegularExpression("^-\\s+(.+)$", QRegularExpression::MultilineOption), "<li>\\1</li>");
    
    // Paragraphs
    html.replace(QRegularExpression("\\n\\n"), "</p><p>");
    
    // Wrap in HTML structure
    html = QString("<html><head><style>"
                   "body { font-family: 'Segoe UI', sans-serif; padding: 20px; line-height: 1.6; }"
                   "code { background: #f4f4f4; padding: 2px 6px; border-radius: 3px; }"
                   "pre { background: #f4f4f4; padding: 15px; border-radius: 5px; overflow-x: auto; }"
                   "a { color: #007acc; }"
                   "</style></head><body><p>%1</p></body></html>").arg(html);
    
    return html;
}

void DocumentationWidget::loadSettings()
{
    QSettings settings;
    restoreState(settings);
}

void DocumentationWidget::saveSettings()
{
    QSettings settings;
    saveState(settings);
}
