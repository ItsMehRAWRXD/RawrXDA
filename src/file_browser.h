#pragma once


class FileBrowser : public void {

public:
    explicit FileBrowser(void* parent = nullptr);
    void initialize();
    
    void loadDirectory(const std::string& dirpath);
    void loadDrives();
    

    void fileSelected(const std::string& filepath);
    
private:
    void handleItemClicked(QTreeWidgetItem* item, int column);
    void handleItemExpanded(QTreeWidgetItem* item);
    
private:
    // Production-ready utility methods
    void ClearLazyLoadingIndicators(QTreeWidgetItem* item);
    void StartAsyncDirectoryLoad(QTreeWidgetItem* item, const std::string& dirPath);
    QTreeWidgetItem* CreateFileTreeItem(const std::filesystem::path& info);
    void AddSmartLazyLoader(QTreeWidgetItem* parentItem, const std::string& dirPath);
    
    // Error handling utilities
    void HandleDirectoryLoadError(QTreeWidgetItem* item, const std::string& error);
    void LogDirectoryAccess(const std::string& path, bool success);
    
    // Performance monitoring
    void TrackLoadingMetrics(const std::string& path, int itemCount, qint64 loadTimeMs);
    
private:
    QTreeWidget* tree_widget_;
};

