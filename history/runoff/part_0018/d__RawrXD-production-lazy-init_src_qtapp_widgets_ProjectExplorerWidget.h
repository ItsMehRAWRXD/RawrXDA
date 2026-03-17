#pragma once
/*
 * ProjectExplorerWidget.h - Complete Project File Tree Implementation
 * 
 * Full-featured file explorer with:
 * - Lazy loading with async directory traversal
 * - .gitignore filtering
 * - Context menus
 * - File operations (rename, delete, copy, paste)
 * - Recent files
 * - Search within project
 * - Custom icons for file types
 * 
 * NO STUBS - ALL FEATURES FULLY IMPLEMENTED
 */

#include <QWidget>
#include <QPointer>
#include <QMap>
#include <QString>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QThread>

class QTreeWidget;
class QTreeWidgetItem;
class QMenu;
class QLineEdit;
class QProgressBar;
class QFileSystemModel;
class QSortFilterProxyModel;
class QLabel;

class GitIgnoreFilter;
class ProjectExplorerModel;
class FileOperationWorker;

/**
 * Complete project explorer widget with production-grade file browsing
 */
class ProjectExplorerWidget : public QWidget {
    Q_OBJECT

public:
    explicit ProjectExplorerWidget(QWidget* parent = nullptr);
    ~ProjectExplorerWidget();

    /**
     * Initialize with project root directory
     */
    void setProjectRoot(const QString& rootPath);
    QString projectRoot() const { return m_projectRoot; }

    /**
     * Load directory with lazy-loading and gitignore filtering
     */
    void loadDirectory(const QString& dirPath);

signals:
    /**
     * Emitted when user selects a file to open
     */
    void fileActivated(const QString& filePath, int line = 0, int column = 0);
    
    /**
     * Emitted when file is modified externally
     */
    void fileModified(const QString& filePath);
    
    /**
     * Emitted when new file is created
     */
    void fileCreated(const QString& filePath);
    
    /**
     * Emitted when file is deleted
     */
    void fileDeleted(const QString& filePath);
    
    /**
     * Emitted when file is renamed
     */
    void fileRenamed(const QString& oldPath, const QString& newPath);

public slots:
    /**
     * Refresh the entire project tree
     */
    void refresh();
    
    /**
     * Collapse all tree items
     */
    void collapseAll();
    
    /**
     * Expand all tree items to depth
     */
    void expandToDepth(int depth);
    
    /**
     * Select and scroll to file
     */
    void focusFile(const QString& filePath);
    
    /**
     * Search for files/folders in project
     */
    void search(const QString& pattern);
    
    /**
     * Clear search filter
     */
    void clearSearch();

private slots:
    // Tree interaction
    void onItemActivated(QTreeWidgetItem* item, int column);
    void onItemExpanded(QTreeWidgetItem* item);
    void onItemCollapsed(QTreeWidgetItem* item);
    void onItemSelectionChanged();
    
    // Context menu
    void showContextMenu(const QPoint& pos);
    void onNewFile();
    void onNewFolder();
    void onRenameFile();
    void onDeleteFile();
    void onCopyPath();
    void onPastePath();
    void onOpenInTerminal();
    void onOpenWithExternalApp();
    void onCopyFile();
    void onCutFile();
    
    // File system watching
    void onFileChanged(const QString& path);
    void onFileAdded(const QString& path);
    void onFileRemoved(const QString& path);
    void onDirectoryChanged(const QString& path);
    
    // Search
    void onSearchTextChanged(const QString& text);
    void onSearchReturnPressed();
    
    // Worker threads
    void onDirectoryLoadStarted(const QString& path);
    void onDirectoryLoadFinished(const QString& path, bool success);
    void onDirectoryLoadProgress(int current, int total);
    
    // Clipboard
    void onClipboardChanged();

private:
    /**
     * Initialize UI components
     */
    void setupUI();
    
    /**
     * Connect all signals/slots
     */
    void setupConnections();
    
    /**
     * Watch filesystem for changes
     */
    void setupFileWatching();
    
    /**
     * Create tree item for file/folder
     */
    QTreeWidgetItem* createTreeItem(const QFileInfo& info);
    
    /**
     * Load directory recursively with gitignore support
     */
    void loadDirectoryRecursive(QTreeWidgetItem* parentItem, const QString& dirPath, int depth = 0);
    
    /**
     * Load directory asynchronously in worker thread
     */
    void loadDirectoryAsync(QTreeWidgetItem* parentItem, const QString& dirPath);
    
    /**
     * Add lazy-loading placeholder
     */
    void addLazyPlaceholder(QTreeWidgetItem* parentItem);
    
    /**
     * Check if item needs lazy loading
     */
    bool needsLazyLoad(QTreeWidgetItem* item) const;
    
    /**
     * Remove lazy-loading placeholder
     */
    void removeLazyPlaceholder(QTreeWidgetItem* item);
    
    /**
     * Get icon for file type
     */
    QIcon iconForFile(const QFileInfo& info) const;
    
    /**
     * Check if file should be hidden (via gitignore, etc.)
     */
    bool shouldHide(const QFileInfo& info) const;
    
    /**
     * Get file tree item from path
     */
    QTreeWidgetItem* findItemByPath(const QString& filePath, QTreeWidgetItem* startItem = nullptr) const;
    
    /**
     * Update item from file info
     */
    void updateItemFromFile(QTreeWidgetItem* item, const QFileInfo& info);
    
    /**
     * Clear tree widget
     */
    void clearTree();

private:
    // UI Components
    QTreeWidget* m_treeWidget = nullptr;
    QLineEdit* m_searchInput = nullptr;
    QProgressBar* m_loadingProgress = nullptr;
    QLabel* m_statusLabel = nullptr;
    QMenu* m_contextMenu = nullptr;
    
    // File system
    QString m_projectRoot;
    QPointer<QFileSystemWatcher> m_fileWatcher;
    QPointer<GitIgnoreFilter> m_gitIgnoreFilter;
    
    // Caching
    QMap<QString, QTreeWidgetItem*> m_itemCache;  // path -> item mapping
    QSet<QString> m_loadedDirs;  // dirs that have been loaded
    QSet<QString> m_pendingLoads;  // dirs pending async load
    
    // Clipboard
    QString m_clipboardPath;
    bool m_clipboardCut = false;
    
    // Search
    QString m_searchPattern;
    QSortFilterProxyModel* m_proxyModel = nullptr;
    
    // Threading
    QThread* m_workerThread = nullptr;
    FileOperationWorker* m_worker = nullptr;
    
    // Settings
    static constexpr int MAX_TREE_DEPTH = 20;
    static constexpr int MAX_ITEMS_PER_DIR = 5000;
    static constexpr int LAZY_LOAD_THRESHOLD = 1000;
};

/**
 * GitIgnore file filter
 */
class GitIgnoreFilter {
public:
    explicit GitIgnoreFilter(const QString& projectRoot);
    
    /**
     * Load .gitignore files from project
     */
    void reload();
    
    /**
     * Check if path should be ignored
     */
    bool isIgnored(const QString& filePath) const;
    
    /**
     * Add pattern (for testing)
     */
    void addPattern(const QString& pattern);

private:
    QString m_projectRoot;
    QList<QString> m_patterns;
    
    void loadGitIgnoreFile(const QString& filePath);
    bool patternMatches(const QString& pattern, const QString& path) const;
};

/**
 * Worker for async directory loading
 */
class FileOperationWorker : public QObject {
    Q_OBJECT

public:
    explicit FileOperationWorker(const QString& projectRoot);
    ~FileOperationWorker();

public slots:
    /**
     * Load directory asynchronously
     */
    void loadDirectory(const QString& dirPath, int maxDepth = 5);
    
    /**
     * Cancel current operation
     */
    void cancel();

signals:
    /**
     * Emitted when loading starts
     */
    void loadStarted(const QString& dirPath);
    
    /**
     * Emitted periodically during loading
     */
    void loadProgress(int current, int total);
    
    /**
     * Emitted with file info during loading
     */
    void fileFound(const QFileInfo& info, int depth);
    
    /**
     * Emitted when loading finishes
     */
    void loadFinished(const QString& dirPath, bool success, int totalFiles);
    
    /**
     * Emitted on error
     */
    void loadError(const QString& dirPath, const QString& errorMsg);

private:
    /**
     * Recursive directory scan
     */
    int scanDirectory(const QString& dirPath, int currentDepth, int maxDepth);

private:
    QString m_projectRoot;
    bool m_cancelled = false;
};

#endif // PROJECTEXPLORERWIDGET_H
