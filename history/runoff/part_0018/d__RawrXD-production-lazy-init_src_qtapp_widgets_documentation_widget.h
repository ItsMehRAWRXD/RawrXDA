/**
 * @file documentation_widget.h
 * @brief Documentation browser with offline docs, API reference, search, and bookmarks
 * 
 * Production-ready implementation providing:
 * - Multi-format documentation support (HTML, Markdown, Qt Help)
 * - Full-text search with indexing
 * - Bookmark management with sync
 * - History navigation
 * - Code example extraction and insertion
 * - Offline documentation support
 */

#ifndef DOCUMENTATION_WIDGET_H
#define DOCUMENTATION_WIDGET_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QTextBrowser>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QToolButton>
#include <QToolBar>
#include <QTabWidget>
#include <QListWidget>
#include <QListWidgetItem>
#include <QProgressBar>
#include <QLabel>
#include <QMenu>
#include <QAction>
#include <QSettings>
#include <QFileSystemWatcher>
#include <QTimer>
#include <QThread>
#include <QMutex>
#include <QHash>
#include <QMap>
#include <QSet>
#include <QUrl>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QRegularExpression>
#include <memory>
#include <vector>
#include <functional>

/**
 * @brief Documentation source types
 */
enum class DocSourceType {
    QtHelp,         // Qt .qch files
    Markdown,       // Markdown documentation
    HTML,           // Static HTML docs
    DevDocs,        // DevDocs.io format
    Doxygen,        // Doxygen generated docs
    Custom          // Custom documentation format
};

/**
 * @brief Represents a documentation source/collection
 */
struct DocSource {
    QString id;
    QString name;
    QString version;
    QString path;
    DocSourceType type;
    QString iconPath;
    bool isEnabled;
    bool isDownloaded;
    qint64 size;
    QString description;
    QDateTime lastUpdated;
};

/**
 * @brief Search result entry
 */
struct DocSearchResult {
    QString title;
    QString url;
    QString snippet;
    QString sourceId;
    QString category;
    double relevanceScore;
    int lineNumber;
};

/**
 * @brief Documentation bookmark
 */
struct DocBookmark {
    QString id;
    QString title;
    QString url;
    QString sourceId;
    QString category;
    QString notes;
    QDateTime created;
    QStringList tags;
};

/**
 * @brief History entry
 */
struct DocHistoryEntry {
    QString title;
    QString url;
    QString sourceId;
    QDateTime timestamp;
};

/**
 * @brief Code example from documentation
 */
struct CodeExample {
    QString language;
    QString code;
    QString description;
    QString sourceUrl;
    int lineStart;
    int lineEnd;
};

/**
 * @brief Background indexer for documentation search
 */
class DocIndexer : public QObject {
    Q_OBJECT
public:
    explicit DocIndexer(QObject* parent = nullptr);
    
    void indexSource(const DocSource& source);
    void removeSource(const QString& sourceId);
    void clearIndex();
    
    QList<DocSearchResult> search(const QString& query, const QStringList& sourceIds = QStringList());

signals:
    void indexingStarted(const QString& sourceId);
    void indexingProgress(const QString& sourceId, int current, int total);
    void indexingFinished(const QString& sourceId);
    void indexingError(const QString& sourceId, const QString& error);

private:
    struct IndexEntry {
        QString url;
        QString title;
        QString content;
        QString sourceId;
        QStringList keywords;
    };
    
    QHash<QString, QList<IndexEntry>> m_index;
    QMutex m_mutex;
    
    void indexHtmlFile(const QString& filePath, const QString& sourceId);
    void indexMarkdownFile(const QString& filePath, const QString& sourceId);
    void indexQtHelpFile(const QString& filePath, const QString& sourceId);
    double calculateRelevance(const IndexEntry& entry, const QStringList& queryTerms);
};

/**
 * @class DocumentationWidget
 * @brief Full-featured documentation browser widget
 * 
 * Features:
 * - Multiple documentation sources (Qt, C++, Python, etc.)
 * - Full-text search with highlighting
 * - Bookmark management
 * - Navigation history
 * - Code example extraction
 * - Offline documentation support
 * - Tab-based viewing
 */
class DocumentationWidget : public QWidget {
    Q_OBJECT
    
public:
    explicit DocumentationWidget(QWidget* parent = nullptr);
    ~DocumentationWidget() override;
    
    // Source management
    void addDocSource(const DocSource& source);
    void removeDocSource(const QString& sourceId);
    void enableDocSource(const QString& sourceId, bool enabled);
    QList<DocSource> getDocSources() const;
    
    // Navigation
    void navigateTo(const QString& url);
    void navigateBack();
    void navigateForward();
    void goHome();
    
    // Search
    void search(const QString& query);
    void clearSearch();
    
    // Bookmarks
    void addBookmark(const DocBookmark& bookmark);
    void removeBookmark(const QString& bookmarkId);
    QList<DocBookmark> getBookmarks() const;
    
    // History
    QList<DocHistoryEntry> getHistory() const;
    void clearHistory();
    
    // Settings
    void setFontSize(int size);
    int getFontSize() const;
    void setTheme(const QString& theme);
    
    // State persistence
    void saveState(QSettings& settings);
    void restoreState(QSettings& settings);

public slots:
    void refresh();
    void downloadDocSource(const QString& sourceId);
    void cancelDownload();

signals:
    void navigationChanged(const QString& url, const QString& title);
    void searchCompleted(int resultCount);
    void bookmarkAdded(const QString& bookmarkId);
    void bookmarkRemoved(const QString& bookmarkId);
    void docSourceAdded(const QString& sourceId);
    void docSourceRemoved(const QString& sourceId);
    void downloadProgress(const QString& sourceId, qint64 received, qint64 total);
    void downloadFinished(const QString& sourceId, bool success);
    void codeExampleSelected(const CodeExample& example);

private slots:
    void onSearchTextChanged(const QString& text);
    void onSearchResultClicked(QListWidgetItem* item);
    void onTreeItemClicked(QTreeWidgetItem* item, int column);
    void onLinkClicked(const QUrl& url);
    void onBookmarkClicked(QListWidgetItem* item);
    void onHistoryItemClicked(QListWidgetItem* item);
    void onTabCloseRequested(int index);
    void onSourceFilterChanged(int index);
    void performSearch();
    void showContextMenu(const QPoint& pos);
    void addCurrentPageToBookmarks();
    void copyCodeExample();
    void insertCodeExample();

private:
    void setupUI();
    void setupToolbar();
    void setupSidebar();
    void setupContentArea();
    void setupConnections();
    void setupDefaultSources();
    
    void updateNavigationButtons();
    void updateSourceTree();
    void updateBookmarksList();
    void updateHistoryList();
    void highlightSearchTerms(const QString& query);
    
    void loadDocPage(const QString& url);
    void addToHistory(const QString& url, const QString& title);
    
    QString resolveDocPath(const QString& url, const QString& sourceId);
    QList<CodeExample> extractCodeExamples(const QString& html);
    QString convertMarkdownToHtml(const QString& markdown);
    
    void loadSettings();
    void saveSettings();
    
    // UI Components
    QVBoxLayout* m_mainLayout;
    QToolBar* m_toolbar;
    QSplitter* m_splitter;
    
    // Navigation toolbar
    QAction* m_backAction;
    QAction* m_forwardAction;
    QAction* m_homeAction;
    QAction* m_refreshAction;
    QAction* m_bookmarkAction;
    QAction* m_printAction;
    QAction* m_zoomInAction;
    QAction* m_zoomOutAction;
    
    // Search bar
    QLineEdit* m_searchEdit;
    QComboBox* m_sourceFilterCombo;
    QPushButton* m_searchButton;
    
    // Sidebar
    QTabWidget* m_sidebarTabs;
    QTreeWidget* m_contentsTree;
    QListWidget* m_searchResults;
    QListWidget* m_bookmarksList;
    QListWidget* m_historyList;
    
    // Content area
    QTabWidget* m_contentTabs;
    QTextBrowser* m_currentBrowser;
    
    // Status
    QProgressBar* m_progressBar;
    QLabel* m_statusLabel;
    
    // Data
    QMap<QString, DocSource> m_docSources;
    QList<DocBookmark> m_bookmarks;
    QList<DocHistoryEntry> m_history;
    int m_historyIndex;
    QList<CodeExample> m_currentCodeExamples;
    
    // Indexer
    std::unique_ptr<DocIndexer> m_indexer;
    QThread* m_indexerThread;
    
    // Settings
    int m_fontSize;
    QString m_theme;
    QString m_homeUrl;
    
    // Search state
    QTimer* m_searchTimer;
    QString m_lastSearchQuery;
    QList<DocSearchResult> m_lastSearchResults;
    
    // Constants
    static constexpr int MAX_HISTORY_SIZE = 100;
    static constexpr int SEARCH_DELAY_MS = 300;
};

#endif // DOCUMENTATION_WIDGET_H
