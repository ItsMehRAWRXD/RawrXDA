/**
 * \file enhanced_project_explorer.h
 * \brief Enhanced Project Explorer with Phase 1 Foundation Integration
 * \author RawrXD Team
 * \date 2026-01-13
 * 
 * Enhanced ProjectExplorer that integrates with:
 * - FileSystemManager for file I/O operations
 * - LoggingSystem for operation logging
 * - ErrorHandler for error management
 */

#pragma once

#include <QWidget>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QFileInfo>
#include <QStringList>
#include <QTimer>
#include <QProgressBar>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QToolButton>
#include <QLineEdit>
#include <QComboBox>
#include <QMenu>
#include <QAction>
#include <QContextMenuEvent>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QUrl>

// Forward declarations
class FileSystemManager;
class LoggingSystem;
class ErrorHandler;

class EnhancedProjectExplorer : public QWidget {
    Q_OBJECT

public:
    explicit EnhancedProjectExplorer(QWidget* parent = nullptr);
    ~EnhancedProjectExplorer();

    // Two-phase initialization
    void initialize();

    // File system operations
    void openProject(const QString& projectPath);
    void refresh();
    void setRootPath(const QString& path);
    QString getCurrentPath() const { return m_currentPath; }

    // View options
    void setShowHiddenFiles(bool show);
    void setShowFileExtensions(bool show);
    void setTreeViewMode(bool expanded);

signals:
    void fileSelected(const QString& filepath);
    void fileDoubleClicked(const QString& filepath);
    void folderSelected(const QString& folderpath);
    void projectPathChanged(const QString& newPath);

public slots:
    void onFileSystemChanged(const QString& path);
    void onFilterTextChanged(const QString& filter);
    void onRefreshRequested();
    void onNewFolder();
    void onNewFile();
    void onDeleteSelected();
    void onRenameSelected();
    void onOpenInExplorer();

private:
    // Foundation system integration
    FileSystemManager* m_fileSystemManager;
    LoggingSystem* m_loggingSystem;
    ErrorHandler* m_errorHandler;

    // UI Components
    QVBoxLayout* m_mainLayout;
    QHBoxLayout* m_toolbarLayout;
    QTreeWidget* m_treeWidget;
    QLineEdit* m_filterEdit;
    QComboBox* m_pathCombo;
    QToolButton* m_refreshButton;
    QToolButton* m_newFolderButton;
    QToolButton* m_newFileButton;
    QLabel* m_statusLabel;
    QProgressBar* m_progressBar;

    // State
    QString m_currentPath;
    QString m_projectPath;
    bool m_showHiddenFiles;
    bool m_showFileExtensions;
    bool m_treeViewMode;
    QTimer m_refreshTimer;

    // Context menu
    QMenu* m_contextMenu;
    QAction* m_newFolderAction;
    QAction* m_newFileAction;
    QAction* m_deleteAction;
    QAction* m_renameAction;
    QAction* m_openInExplorerAction;
    QAction* m_copyPathAction;
    QAction* m_propertiesAction;

    // Private methods
    void setupUI();
    void setupContextMenu();
    void setupConnections();
    void populateTree(const QString& path, QTreeWidgetItem* parentItem = nullptr);
    QTreeWidgetItem* createTreeItem(const QFileInfo& fileInfo, QTreeWidgetItem* parent);
    void loadDirectoryChildren(QTreeWidgetItem* parentItem, const QString& dirPath);
    QString getRelativePath(const QString& fullPath) const;
    bool matchesFilter(const QFileInfo& fileInfo, const QString& filter) const;
    void updateStatusBar();
    void handleError(const QString& operation, const QString& error);

    // Drag and drop
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;

    // Context menu
    void contextMenuEvent(QContextMenuEvent* event) override;

    // Tree widget events
    void onItemExpanded(QTreeWidgetItem* item);
    void onItemCollapsed(QTreeWidgetItem* item);
    void onItemDoubleClicked(QTreeWidgetItem* item, int column);
    void onItemSelectionChanged();

    // Utility methods
    QString getCurrentSelectionPath() const;
    QList<QTreeWidgetItem*> getSelectedItems() const;
    void expandToPath(const QString& path);
};