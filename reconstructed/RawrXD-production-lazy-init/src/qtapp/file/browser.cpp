/**
 * @file file_browser.cpp
 * @brief Implementation of file browser with live directory watching
 *
 * Features:
 * - Lazy-loaded directory tree
 * - Real-time file system monitoring
 * - Async directory loading
 * - Performance metrics tracking
 * - Error handling and logging
 *
 * @author RawrXD Team
 * @version 1.0.0
 */

#include "file_browser.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QDir>
#include <QFileIconProvider>
#include <QFileSystemWatcher>
#include <QThread>
#include <QtConcurrent>
#include <QDebug>
#include <QStandardPaths>
#include <QElapsedTimer>

// Forward declaration
class DirectoryLoader;

/**
 * @class FileBrowser
 * @brief File browser with real-time directory synchronization
 */
class FileBrowserPrivate {
public:
    QTreeWidget* tree = nullptr;
    QFileSystemWatcher* watcher = nullptr;
    QFileIconProvider* iconProvider = nullptr;
    QMap<QString, QTreeWidgetItem*> pathToItem;
    QMap<QString, qint64> loadTimeMetrics;
    bool initialized = false;
    
    // Performance tracking
    struct LoadMetrics {
        qint64 totalTimeMs = 0;
        int totalItems = 0;
        int successfulLoads = 0;
        int failedLoads = 0;
    } metrics;
};

/**
 * @brief Constructor
 */
FileBrowser::FileBrowser(QWidget* parent)
    : QWidget(parent)
    , tree_widget_(nullptr)
    , m_private(std::make_unique<FileBrowserPrivate>())
{
    setWindowTitle("File Browser");
    m_private->iconProvider = new QFileIconProvider();
}

FileBrowser::~FileBrowser() = default;

/**
 * @brief Initialize the UI (deferred, after QApplication)
 */
void FileBrowser::initialize()
{
    if (m_private->initialized) {
        return;
    }

    QVBoxLayout* layout = new QVBoxLayout(this);

    // Create tree widget
    m_private->tree = new QTreeWidget(this);
    m_private->tree->setColumnCount(1);
    m_private->tree->setHeaderLabel("Files");
    m_private->tree->setAnimated(true);
    m_private->tree->setIndentation(20);

    connect(m_private->tree, &QTreeWidget::itemClicked, 
            this, &FileBrowser::handleItemClicked);
    connect(m_private->tree, &QTreeWidget::itemExpanded, 
            this, &FileBrowser::handleItemExpanded);

    layout->addWidget(m_private->tree);
    setLayout(layout);

    // Create file system watcher
    m_private->watcher = new QFileSystemWatcher(this);
    connect(m_private->watcher, &QFileSystemWatcher::directoryChanged,
            this, [this](const QString &path) { onDirectoryChanged(path); });
    connect(m_private->watcher, &QFileSystemWatcher::fileChanged,
            this, [this](const QString &path) { onFileChanged(path); });

    m_private->initialized = true;

    // Load drives initially
    loadDrives();

    qDebug() << "[FileBrowser] Initialized with file system watcher";
}

/**
 * @brief Load directory structure into tree
 */
void FileBrowser::loadDirectory(const QString& dirpath)
{
    if (!m_private->initialized) {
        initialize();
    }

    QElapsedTimer timer;
    timer.start();

    m_private->tree->clear();
    m_private->pathToItem.clear();

    QDir dir(dirpath);
    if (!dir.exists()) {
        HandleDirectoryLoadError(nullptr, "Directory does not exist: " + dirpath);
        return;
    }

    // Add the root directory
    QTreeWidgetItem* rootItem = new QTreeWidgetItem(m_private->tree);
    rootItem->setText(0, dir.dirName().isEmpty() ? dirpath : dir.dirName());
    rootItem->setIcon(0, m_private->iconProvider->icon(QFileIconProvider::Folder));
    rootItem->setData(0, Qt::UserRole, dirpath);

    m_private->pathToItem[dirpath] = rootItem;

    // Start watching this directory
    if (!m_private->watcher->addPath(dirpath)) {
        qWarning() << "[FileBrowser] Failed to watch directory:" << dirpath;
    }

    // Load subdirectories and files asynchronously
    StartAsyncDirectoryLoad(rootItem, dirpath);

    qint64 elapsedMs = timer.elapsed();
    TrackLoadingMetrics(dirpath, m_private->tree->topLevelItemCount(), elapsedMs);

    LogDirectoryAccess(dirpath, true);

    qDebug() << "[FileBrowser] Loaded directory:" << dirpath << "in" << elapsedMs << "ms";
}

/**
 * @brief Load drive letters (Windows)
 */
void FileBrowser::loadDrives()
{
    if (!m_private->initialized) {
        initialize();
    }

    m_private->tree->clear();
    m_private->pathToItem.clear();

    #ifdef Q_OS_WIN
    // Get all drives
    for (const QFileInfo& drive : QDir::drives()) {
        QTreeWidgetItem* item = CreateFileTreeItem(drive);
        m_private->tree->addTopLevelItem(item);
        m_private->pathToItem[drive.absoluteFilePath()] = item;

        // Add placeholder for lazy loading
        AddSmartLazyLoader(item, drive.absoluteFilePath());

        // Watch drive
        m_private->watcher->addPath(drive.absoluteFilePath());
    }
    #else
    // Unix: load root and home
    QString rootPath = "/";
    QString homePath = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);

    QTreeWidgetItem* rootItem = CreateFileTreeItem(QFileInfo(rootPath));
    QTreeWidgetItem* homeItem = CreateFileTreeItem(QFileInfo(homePath));

    m_private->tree->addTopLevelItem(rootItem);
    m_private->tree->addTopLevelItem(homeItem);

    m_private->pathToItem[rootPath] = rootItem;
    m_private->pathToItem[homePath] = homeItem;

    AddSmartLazyLoader(rootItem, rootPath);
    AddSmartLazyLoader(homeItem, homePath);

    m_private->watcher->addPath(rootPath);
    m_private->watcher->addPath(homePath);
    #endif

    qDebug() << "[FileBrowser] Loaded drives/roots";
}

/**
 * @brief Handle item click (file selection)
 */
void FileBrowser::handleItemClicked(QTreeWidgetItem* item, int column)
{
    Q_UNUSED(column);

    QString filepath = item->data(0, Qt::UserRole).toString();
    if (!filepath.isEmpty()) {
        QFileInfo info(filepath);
        if (info.isFile()) {
            emit fileSelected(filepath);
            qDebug() << "[FileBrowser] File selected:" << filepath;
        } else {
            qDebug() << "[FileBrowser] Directory selected:" << filepath;
        }
    }
}

/**
 * @brief Handle item expansion (lazy load subdirectories)
 */
void FileBrowser::handleItemExpanded(QTreeWidgetItem* item)
{
    QString dirpath = item->data(0, Qt::UserRole).toString();

    // Check if this is a lazy-loaded placeholder
    if (item->childCount() == 1) {
        QTreeWidgetItem* child = item->child(0);
        if (child->text(0) == "Loading...") {
            // Remove placeholder and load real children
            item->removeChild(child);
            delete child;

            StartAsyncDirectoryLoad(item, dirpath);
        }
    }

    qDebug() << "[FileBrowser] Expanded item:" << dirpath;
}

/**
 * @brief Handle directory changes (file system watcher callback)
 */
void FileBrowser::onDirectoryChanged(const QString& path)
{
    qDebug() << "[FileBrowser] Directory changed:" << path;

    QTreeWidgetItem* item = m_private->pathToItem.value(path);
    if (!item) {
        return;  // Not in tree
    }

    // Refresh this directory
    ClearLazyLoadingIndicators(item);
    StartAsyncDirectoryLoad(item, path);
}

/**
 * @brief Handle file changes (file system watcher callback)
 */
void FileBrowser::onFileChanged(const QString& filepath)
{
    qDebug() << "[FileBrowser] File changed:" << filepath;

    QFileInfo info(filepath);
    QDir parentDir = info.dir();
    QString parentPath = parentDir.absolutePath();

    // Trigger parent directory refresh
    onDirectoryChanged(parentPath);
}

/**
 * @brief Clear lazy-loading indicators
 */
void FileBrowser::ClearLazyLoadingIndicators(QTreeWidgetItem* item)
{
    for (int i = 0; i < item->childCount(); ++i) {
        QTreeWidgetItem* child = item->child(i);
        if (child->text(0) == "Loading...") {
            item->removeChild(child);
            delete child;
            break;
        }
    }
}

/**
 * @brief Start async directory loading
 */
void FileBrowser::StartAsyncDirectoryLoad(QTreeWidgetItem* item, const QString& dirPath)
{
    QElapsedTimer timer;
    timer.start();

    QFuture<QFileInfoList> future = QtConcurrent::run([dirPath]() {
        QDir dir(dirPath);
        dir.setFilter(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot);
        dir.setSorting(QDir::DirsFirst | QDir::Name);

        return dir.entryInfoList();
    });

    // Continue on result
    QtConcurrent::run([this, item, dirPath, future, timer]() mutable {
        QFileInfoList entries = future.result();

        QMetaObject::invokeMethod(this, [this, item, dirPath, entries, timer]() {
            int itemCount = 0;

            for (const QFileInfo& info : entries) {
                QTreeWidgetItem* child = CreateFileTreeItem(info);
                item->addChild(child);

                // Add lazy loader for directories
                if (info.isDir()) {
                    AddSmartLazyLoader(child, info.absoluteFilePath());

                    // Watch subdirectory
                    m_private->watcher->addPath(info.absoluteFilePath());
                }

                m_private->pathToItem[info.absoluteFilePath()] = child;
                itemCount++;
            }

            TrackLoadingMetrics(dirPath, itemCount, timer.elapsed());

            m_private->metrics.successfulLoads++;

            qDebug() << "[FileBrowser] Loaded" << itemCount << "items from" << dirPath;
        });
    });
}

/**
 * @brief Create a tree item for a file/directory
 */
QTreeWidgetItem* FileBrowser::CreateFileTreeItem(const QFileInfo& info)
{
    QTreeWidgetItem* item = new QTreeWidgetItem();
    item->setText(0, info.fileName().isEmpty() ? info.absoluteFilePath() : info.fileName());
    item->setIcon(0, m_private->iconProvider->icon(info));
    item->setData(0, Qt::UserRole, info.absoluteFilePath());

    if (info.isDir()) {
        item->setData(0, Qt::UserRole + 1, "dir");
    } else {
        item->setData(0, Qt::UserRole + 1, "file");
    }

    return item;
}

/**
 * @brief Add lazy-loading placeholder for directories
 */
void FileBrowser::AddSmartLazyLoader(QTreeWidgetItem* parentItem, const QString& dirPath)
{
    // Check if directory is accessible
    QDir dir(dirPath);
    if (!dir.isReadable()) {
        return;  // Don't add placeholder for inaccessible dirs
    }

    QTreeWidgetItem* placeholder = new QTreeWidgetItem();
    placeholder->setText(0, "Loading...");
    parentItem->addChild(placeholder);
}

/**
 * @brief Handle directory load errors
 */
void FileBrowser::HandleDirectoryLoadError(QTreeWidgetItem* item, const QString& error)
{
    qWarning() << "[FileBrowser] Error:" << error;

    if (item) {
        QTreeWidgetItem* errorItem = new QTreeWidgetItem(item);
        errorItem->setText(0, "Error: " + error);
        item->addChild(errorItem);
    }

    m_private->metrics.failedLoads++;
}

/**
 * @brief Log directory access
 */
void FileBrowser::LogDirectoryAccess(const QString& path, bool success)
{
    qDebug() << "[FileBrowser]" << (success ? "Successfully" : "Failed to") << "access" << path;
}

/**
 * @brief Track loading performance metrics
 */
void FileBrowser::TrackLoadingMetrics(const QString& path, int itemCount, qint64 loadTimeMs)
{
    m_private->loadTimeMetrics[path] = loadTimeMs;
    m_private->metrics.totalTimeMs += loadTimeMs;
    m_private->metrics.totalItems += itemCount;

    if (loadTimeMs > 100) {
        qWarning() << "[FileBrowser] Slow directory load:" << path << "took" << loadTimeMs << "ms";
    }

    qDebug() << "[FileBrowser] Load metrics - Path:" << path << "Items:" << itemCount << "Time:" << loadTimeMs << "ms";
}

/**
 * @brief Get file browser metrics for diagnostics
 */
QJsonObject FileBrowser::getMetrics() const
{
    QJsonObject metrics;
    metrics["totalTimeMs"] = static_cast<int>(m_private->metrics.totalTimeMs);
    metrics["totalItems"] = m_private->metrics.totalItems;
    metrics["successfulLoads"] = m_private->metrics.successfulLoads;
    metrics["failedLoads"] = m_private->metrics.failedLoads;
    metrics["averageLoadTimeMs"] = m_private->metrics.successfulLoads > 0
        ? static_cast<int>(m_private->metrics.totalTimeMs / m_private->metrics.successfulLoads)
        : 0;

    return metrics;
}
