#pragma once

#include <QWidget>
#include <QString>
#include <QFileInfo>

class QTreeWidget;
class QTreeWidgetItem;

class FileBrowser : public QWidget {
    Q_OBJECT
public:
    explicit FileBrowser(QWidget* parent = nullptr);
    void initialize();
    
    void loadDirectory(const QString& dirpath);
    void loadDrives();
    
signals:
    void fileSelected(const QString& filepath);
    
private slots:
    void handleItemClicked(QTreeWidgetItem* item, int column);
    void handleItemExpanded(QTreeWidgetItem* item);
    
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
    
private:
    QTreeWidget* tree_widget_;
};
