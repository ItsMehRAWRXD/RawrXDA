#pragma once

#include <QWidget>
#include <QString>
#include <QFileInfo>
#include <QJsonObject>
#include <memory>

class QTreeWidget;
class QTreeWidgetItem;
class FileBrowserPrivate;

class FileBrowser : public QWidget {
    Q_OBJECT
public:
    explicit FileBrowser(QWidget* parent = nullptr);
    ~FileBrowser();
    void initialize();
    
    void loadDirectory(const QString& dirpath);
    void loadDrives();
    
signals:
    void fileSelected(const QString& filepath);
    void directoryRefreshed(const QString& dirpath);
    void fileSystemError(const QString& error);
    
private slots:
    void handleItemClicked(QTreeWidgetItem* item, int column);
    void handleItemExpanded(QTreeWidgetItem* item);
    void onDirectoryChanged(const QString& path);
    void onFileChanged(const QString& filepath);
    
private:
    // Production-ready utility methods
    void ClearLazyLoadingIndicators(QTreeWidgetItem* item);
    void StartAsyncDirectoryLoad(QTreeWidgetItem* item, const QString& dirPath);
    QTreeWidgetItem* CreateFileTreeItem(const QFileInfo& info);
    void AddSmartLazyLoader(QTreeWidgetItem* parentItem, const QString& dirPath);
    
    // Error handling utilities
    void HandleDirectoryLoadError(QTreeWidgetItem* item, const QString& error);
    void LogDirectoryAccess(const QString& path, bool success);
    
    // Performance monitoring
    void TrackLoadingMetrics(const QString& path, int itemCount, qint64 loadTimeMs);
    
    // Metrics retrieval
    QJsonObject getMetrics() const;
    
private:
    QTreeWidget* tree_widget_;
    std::unique_ptr<FileBrowserPrivate> m_private;
};
