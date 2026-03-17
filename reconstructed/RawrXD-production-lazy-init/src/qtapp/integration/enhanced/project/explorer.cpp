/**
 * \file enhanced_project_explorer.cpp
 * \brief Enhanced Project Explorer Implementation with Phase 1 Foundation Integration
 * \author RawrXD Team
 * \date 2026-01-13
 */

#include "enhanced_project_explorer.h"
#include "../core/file_system_manager.h"
#include "../core/logging_system.h"
#include "../core/error_handler.h"

#include <QFile>
#include <QDir>
#include <QStandardPaths>
#include <QMessageBox>
#include <QInputDialog>
#include <QFileDialog>
#include <QDesktopServices>
#include <QUrl>
#include <QApplication>
#include <QClipboard>
#include <QFont>
#include <QIcon>
#include <QStyle>

EnhancedProjectExplorer::EnhancedProjectExplorer(QWidget* parent)
    : QWidget(parent)
    , m_fileSystemManager(nullptr)
    , m_loggingSystem(nullptr)
    , m_errorHandler(nullptr)
    , m_mainLayout(nullptr)
    , m_toolbarLayout(nullptr)
    , m_treeWidget(nullptr)
    , m_filterEdit(nullptr)
    , m_pathCombo(nullptr)
    , m_refreshButton(nullptr)
    , m_newFolderButton(nullptr)
    , m_newFileButton(nullptr)
    , m_statusLabel(nullptr)
    , m_progressBar(nullptr)
    , m_currentPath("")
    , m_projectPath("")
    , m_showHiddenFiles(false)
    , m_showFileExtensions(true)
    , m_treeViewMode(false)
    , m_contextMenu(nullptr)
    , m_newFolderAction(nullptr)
    , m_newFileAction(nullptr)
    , m_deleteAction(nullptr)
    , m_renameAction(nullptr)
    , m_openInExplorerAction(nullptr)
    , m_copyPathAction(nullptr)
    , m_propertiesAction(nullptr)
{
    // Initialize foundation systems
    m_fileSystemManager = &FileSystemManager::instance();
    m_loggingSystem = &LoggingSystem::instance();
    m_errorHandler = &ErrorHandler::instance();
}

EnhancedProjectExplorer::~EnhancedProjectExplorer() {
    // Cleanup handled by Qt parent-child relationships
}

void EnhancedProjectExplorer::initialize() {
    setupUI();
    setupContextMenu();
    setupConnections();
    
    // Set initial project path
    QString defaultProjectPath = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    openProject(defaultProjectPath);
    
    m_loggingSystem->logInfo("EnhancedProjectExplorer", "Enhanced Project Explorer initialized successfully");
}

void EnhancedProjectExplorer::setupUI() {
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(2, 2, 2, 2);
    m_mainLayout->setSpacing(2);

    // Toolbar layout
    m_toolbarLayout = new QHBoxLayout();
    
    // Path combo box
    m_pathCombo = new QComboBox(this);
    m_pathCombo->setEditable(true);
    m_pathCombo->setMinimumWidth(300);
    m_toolbarLayout->addWidget(new QLabel("Path:", this));
    m_toolbarLayout->addWidget(m_pathCombo, 1);
    
    // Refresh button
    m_refreshButton = new QToolButton(this);
    m_refreshButton->setText("↻");
    m_refreshButton->setToolTip("Refresh");
    m_toolbarLayout->addWidget(m_refreshButton);
    
    // New folder button
    m_newFolderButton = new QToolButton(this);
    m_newFolderButton->setText("📁+");
    m_newFolderButton->setToolTip("New Folder");
    m_toolbarLayout->addWidget(m_newFolderButton);
    
    // New file button
    m_newFileButton = new QToolButton(this);
    m_newFileButton->setText("📄+");
    m_newFileButton->setToolTip("New File");
    m_toolbarLayout->addWidget(m_newFileButton);
    
    m_mainLayout->addLayout(m_toolbarLayout);
    
    // Filter edit
    m_filterEdit = new QLineEdit(this);
    m_filterEdit->setPlaceholderText("Filter files and folders...");
    m_filterEdit->setClearButtonEnabled(true);
    m_mainLayout->addWidget(m_filterEdit);
    
    // Tree widget
    m_treeWidget = new QTreeWidget(this);
    m_treeWidget->setHeaderLabel("Project Explorer");
    m_treeWidget->setRootIsDecorated(true);
    m_treeWidget->setAlternatingRowColors(true);
    m_treeWidget->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_treeWidget->setContextMenuPolicy(Qt::CustomContextMenu);
    m_treeWidget->setDragEnabled(true);
    m_treeWidget->setAcceptDrops(true);
    
    // Set up column widths
    m_treeWidget->setColumnWidth(0, 300);
    
    m_mainLayout->addWidget(m_treeWidget, 1);
    
    // Status bar
    QHBoxLayout* statusLayout = new QHBoxLayout();
    m_statusLabel = new QLabel("Ready", this);
    m_progressBar = new QProgressBar(this);
    m_progressBar->setVisible(false);
    m_progressBar->setMaximumHeight(16);
    
    statusLayout->addWidget(m_statusLabel);
    statusLayout->addWidget(m_progressBar);
    m_mainLayout->addLayout(statusLayout);
}

void EnhancedProjectExplorer::setupContextMenu() {
    m_contextMenu = new QMenu(this);
    
    m_newFolderAction = new QAction("📁 New Folder", this);
    m_newFileAction = new QAction("📄 New File", this);
    m_deleteAction = new QAction("🗑️ Delete", this);
    m_renameAction = new QAction("✏️ Rename", this);
    m_openInExplorerAction = new QAction("📂 Open in Explorer", this);
    m_copyPathAction = new QAction("📋 Copy Path", this);
    m_propertiesAction = new QAction("ℹ️ Properties", this);
    
    m_contextMenu->addAction(m_newFolderAction);
    m_contextMenu->addAction(m_newFileAction);
    m_contextMenu->addSeparator();
    m_contextMenu->addAction(m_renameAction);
    m_contextMenu->addAction(m_deleteAction);
    m_contextMenu->addSeparator();
    m_contextMenu->addAction(m_openInExplorerAction);
    m_contextMenu->addAction(m_copyPathAction);
    m_contextMenu->addAction(m_propertiesAction);
}

void EnhancedProjectExplorer::setupConnections() {
    // Toolbar connections
    connect(m_refreshButton, &QToolButton::clicked, this, &EnhancedProjectExplorer::onRefreshRequested);
    connect(m_newFolderButton, &QToolButton::clicked, this, &EnhancedProjectExplorer::onNewFolder);
    connect(m_newFileButton, &QToolButton::clicked, this, &EnhancedProjectExplorer::onNewFile);
    
    // Filter connections
    connect(m_filterEdit, &QLineEdit::textChanged, this, &EnhancedProjectExplorer::onFilterTextChanged);
    
    // Tree widget connections
    connect(m_treeWidget, &QTreeWidget::itemDoubleClicked, 
            this, &EnhancedProjectExplorer::onItemDoubleClicked);
    connect(m_treeWidget, &QTreeWidget::itemExpanded, 
            this, &EnhancedProjectExplorer::onItemExpanded);
    connect(m_treeWidget, &QTreeWidget::itemSelectionChanged, 
            this, &EnhancedProjectExplorer::onItemSelectionChanged);
    connect(m_treeWidget, &QTreeWidget::customContextMenuRequested,
            this, &EnhancedProjectExplorer::contextMenuEvent);
    
    // FileSystemManager connections
    connect(m_fileSystemManager, &FileSystemManager::fileChangedExternally,
            this, &EnhancedProjectExplorer::onFileSystemChanged);
    connect(m_fileSystemManager, &FileSystemManager::fileOperationComplete,
            this, &EnhancedProjectExplorer::onFileSystemChanged);
    
    // Context menu connections
    connect(m_newFolderAction, &QAction::triggered, this, &EnhancedProjectExplorer::onNewFolder);
    connect(m_newFileAction, &QAction::triggered, this, &EnhancedProjectExplorer::onNewFile);
    connect(m_deleteAction, &QAction::triggered, this, &EnhancedProjectExplorer::onDeleteSelected);
    connect(m_renameAction, &QAction::triggered, this, &EnhancedProjectExplorer::onRenameSelected);
    connect(m_openInExplorerAction, &QAction::triggered, this, &EnhancedProjectExplorer::onOpenInExplorer);
    connect(m_copyPathAction, &QAction::triggered, this, [this]() {
        QString path = getCurrentSelectionPath();
        if (!path.isEmpty()) {
            QApplication::clipboard()->setText(path);
            m_statusLabel->setText("Path copied to clipboard");
        }
    });
    connect(m_propertiesAction, &QAction::triggered, this, [this]() {
        QString path = getCurrentSelectionPath();
        if (!path.isEmpty()) {
            QFileInfo info(path);
            QString message = QString("Name: %1\nSize: %2 bytes\nModified: %3")
                             .arg(info.fileName())
                             .arg(info.size())
                             .arg(info.lastModified().toString());
            QMessageBox::information(this, "File Properties", message);
        }
    });
    
    // Refresh timer for efficient updates
    connect(&m_refreshTimer, &QTimer::timeout, this, [this]() {
        // Only refresh if not currently busy
        if (!m_progressBar->isVisible()) {
            refresh();
        }
    });
}

void EnhancedProjectExplorer::openProject(const QString& projectPath) {
    if (projectPath.isEmpty()) {
        m_errorHandler->reportError("EnhancedProjectExplorer", "Project path cannot be empty");
        return;
    }
    
    QFileInfo projectInfo(projectPath);
    if (!projectInfo.exists()) {
        m_errorHandler->reportError("EnhancedProjectExplorer", 
                                   QString("Project path does not exist: %1").arg(projectPath));
        return;
    }
    
    if (!projectInfo.isDir()) {
        m_errorHandler->reportError("EnhancedProjectExplorer", 
                                   QString("Project path is not a directory: %1").arg(projectPath));
        return;
    }
    
    m_projectPath = projectInfo.absoluteFilePath();
    m_currentPath = m_projectPath;
    
    // Add to path combo
    if (!m_pathCombo->findText(m_projectPath) >= 0) {
        m_pathCombo->addItem(m_projectPath);
    }
    
    m_pathCombo->setCurrentText(m_projectPath);
    
    // Populate tree
    populateTree(m_projectPath);
    
    // Watch the project directory
    m_fileSystemManager->watchFile(m_projectPath);
    
    m_loggingSystem->logInfo("EnhancedProjectExplorer", 
                           QString("Project opened: %1").arg(m_projectPath));
    emit projectPathChanged(m_projectPath);
}

void EnhancedProjectExplorer::populateTree(const QString& path, QTreeWidgetItem* parentItem) {
    QDir dir(path);
    if (!dir.exists()) {
        handleError("populateTree", QString("Directory does not exist: %1").arg(path));
        return;
    }
    
    m_progressBar->setVisible(true);
    m_progressBar->setRange(0, 0); // Indeterminate progress
    
    // Clear existing items if this is the root
    if (!parentItem) {
        m_treeWidget->clear();
    }
    
    // Set up filters
    QDir::Filters filters = QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot;
    if (m_showHiddenFiles) {
        filters |= QDir::Hidden;
    }
    
    QDir::SortFlags sortFlags = QDir::Name | QDir::IgnoreCase;
    
    try {
        QFileInfoList entries = dir.entryInfoList(filters, sortFlags);
        
        // Create root item if needed
        QTreeWidgetItem* rootItem = parentItem;
        if (!rootItem) {
            rootItem = new QTreeWidgetItem(m_treeWidget);
            rootItem->setText(0, QFileInfo(path).fileName());
            rootItem->setData(0, Qt::UserRole, path);
            rootItem->setIcon(0, style()->standardIcon(QStyle::SP_DirIcon));
            rootItem->setExpanded(true);
        }
        
        // Add directory items first
        for (const QFileInfo& entry : entries) {
            if (entry.isDir()) {
                QTreeWidgetItem* dirItem = createTreeItem(entry, rootItem);
                // Add a dummy child to show expand arrow for directories
                if (dirItem) {
                    new QTreeWidgetItem(dirItem); // Dummy child
                }
            }
        }
        
        // Add file items
        for (const QFileInfo& entry : entries) {
            if (entry.isFile()) {
                createTreeItem(entry, rootItem);
            }
        }
        
        m_loggingSystem->logInfo("EnhancedProjectExplorer", 
                               QString("Populated directory: %1 (%2 items)").arg(path).arg(entries.size()));
        
    } catch (const std::exception& e) {
        handleError("populateTree", QString("Exception: %1").arg(e.what()));
    }
    
    m_progressBar->setVisible(false);
}

QTreeWidgetItem* EnhancedProjectExplorer::createTreeItem(const QFileInfo& fileInfo, QTreeWidgetItem* parent) {
    if (!matchesFilter(fileInfo, m_filterEdit->text())) {
        return nullptr;
    }
    
    QTreeWidgetItem* item = new QTreeWidgetItem(parent);
    item->setText(0, fileInfo.fileName());
    item->setData(0, Qt::UserRole, fileInfo.absoluteFilePath());
    
    // Set icons based on file type
    if (fileInfo.isDir()) {
        item->setIcon(0, style()->standardIcon(QStyle::SP_DirIcon));
        item->setChildIndicatorPolicy(QTreeWidgetItem::ShowIndicator);
    } else {
        // TODO: Set file type icons based on extension
        QIcon fileIcon = style()->standardIcon(QStyle::SP_FileIcon);
        item->setIcon(0, fileIcon);
    }
    
    // Set tooltips
    QString tooltip = QString("%1\nSize: %2 bytes\nModified: %3")
                    .arg(fileInfo.absoluteFilePath())
                    .arg(fileInfo.size())
                    .arg(fileInfo.lastModified().toString());
    item->setToolTip(0, tooltip);
    
    return item;
}

bool EnhancedProjectExplorer::matchesFilter(const QFileInfo& fileInfo, const QString& filter) const {
    if (filter.isEmpty()) {
        return true;
    }
    
    QString fileName = fileInfo.fileName().toLower();
    QString filterLower = filter.toLower();
    
    // Simple substring matching
    return fileName.contains(filterLower);
}

void EnhancedProjectExplorer::onItemExpanded(QTreeWidgetItem* item) {
    if (!item) return;
    
    QString dirPath = item->data(0, Qt::UserRole).toString();
    if (dirPath.isEmpty()) return;
    
    // Remove dummy children
    while (item->childCount() > 0) {
        delete item->child(0);
    }
    
    // Populate with actual children
    loadDirectoryChildren(item, dirPath);
}

void EnhancedProjectExplorer::loadDirectoryChildren(QTreeWidgetItem* parentItem, const QString& dirPath) {
    if (!parentItem || dirPath.isEmpty()) return;
    
    QDir dir(dirPath);
    if (!dir.exists()) return;
    
    // Set up filters
    QDir::Filters filters = QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot;
    if (m_showHiddenFiles) {
        filters |= QDir::Hidden;
    }
    
    QDir::SortFlags sortFlags = QDir::Name | QDir::IgnoreCase;
    
    QFileInfoList entries = dir.entryInfoList(filters, sortFlags);
    
    // Add directories first
    for (const QFileInfo& entry : entries) {
        if (entry.isDir()) {
            QTreeWidgetItem* dirItem = createTreeItem(entry, parentItem);
            if (dirItem) {
                // Add dummy child for expandable directories
                new QTreeWidgetItem(dirItem);
            }
        }
    }
    
    // Add files
    for (const QFileInfo& entry : entries) {
        if (entry.isFile()) {
            createTreeItem(entry, parentItem);
        }
    }
}

void EnhancedProjectExplorer::onItemDoubleClicked(QTreeWidgetItem* item, int column) {
    if (!item) return;
    
    QString filePath = item->data(0, Qt::UserRole).toString();
    if (filePath.isEmpty()) return;
    
    QFileInfo info(filePath);
    
    if (info.isDir()) {
        // Expand/collapse directory
        if (item->isExpanded()) {
            m_treeWidget->collapseItem(item);
        } else {
            m_treeWidget->expandItem(item);
        }
    } else if (info.isFile()) {
        // Emit signal for file selection
        emit fileDoubleClicked(filePath);
        emit fileSelected(filePath);
    }
}

void EnhancedProjectExplorer::onItemSelectionChanged() {
    QStringList selectedPaths;
    QList<QTreeWidgetItem*> selectedItems = m_treeWidget->selectedItems();
    
    for (QTreeWidgetItem* item : selectedItems) {
        QString path = item->data(0, Qt::UserRole).toString();
        if (!path.isEmpty()) {
            selectedPaths.append(path);
        }
    }
    
    // Update status bar
    if (selectedPaths.size() == 0) {
        m_statusLabel->setText("No selection");
    } else if (selectedPaths.size() == 1) {
        QFileInfo info(selectedPaths.first());
        m_statusLabel->setText(QString("%1 (%2)")
                              .arg(info.fileName())
                              .arg(info.isDir() ? "Folder" : QString("%1 bytes").arg(info.size())));
    } else {
        m_statusLabel->setText(QString("%1 items selected").arg(selectedPaths.size()));
    }
}

void EnhancedProjectExplorer::contextMenuEvent(QContextMenuEvent* event) {
    QTreeWidgetItem* item = m_treeWidget->itemAt(event->pos());
    
    // Enable/disable actions based on selection
    bool hasSelection = (item != nullptr);
    m_deleteAction->setEnabled(hasSelection);
    m_renameAction->setEnabled(hasSelection);
    m_copyPathAction->setEnabled(hasSelection);
    m_propertiesAction->setEnabled(hasSelection);
    m_openInExplorerAction->setEnabled(hasSelection);
    
    m_contextMenu->exec(event->globalPos());
}

void EnhancedProjectExplorer::onRefreshRequested() {
    refresh();
}

void EnhancedProjectExplorer::refresh() {
    if (m_currentPath.isEmpty()) return;
    
    // Clear and repopulate
    m_treeWidget->clear();
    populateTree(m_currentPath);
    updateStatusBar();
    
    m_loggingSystem->logInfo("EnhancedProjectExplorer", "Project explorer refreshed");
}

void EnhancedProjectExplorer::onNewFolder() {
    QString parentPath = getCurrentSelectionPath();
    if (parentPath.isEmpty() || !QFileInfo(parentPath).isDir()) {
        parentPath = m_currentPath;
    }
    
    bool ok;
    QString folderName = QInputDialog::getText(this, "New Folder", 
                                             "Folder name:", QLineEdit::Normal, "", &ok);
    if (!ok || folderName.isEmpty()) return;
    
    QString fullPath = QDir(parentPath).filePath(folderName);
    
    if (QDir().mkdir(fullPath)) {
        m_loggingSystem->logInfo("EnhancedProjectExplorer", QString("Created folder: %1").arg(fullPath));
        refresh();
    } else {
        m_errorHandler->reportError("EnhancedProjectExplorer", 
                                   QString("Failed to create folder: %1").arg(fullPath));
    }
}

void EnhancedProjectExplorer::onNewFile() {
    QString parentPath = getCurrentSelectionPath();
    if (parentPath.isEmpty() || !QFileInfo(parentPath).isDir()) {
        parentPath = m_currentPath;
    }
    
    bool ok;
    QString fileName = QInputDialog::getText(this, "New File", 
                                           "File name:", QLineEdit::Normal, "", &ok);
    if (!ok || fileName.isEmpty()) return;
    
    QString fullPath = QDir(parentPath).filePath(fileName);
    
    QFile file(fullPath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        file.close();
        m_loggingSystem->logInfo("EnhancedProjectExplorer", QString("Created file: %1").arg(fullPath));
        refresh();
    } else {
        m_errorHandler->reportError("EnhancedProjectExplorer", 
                                   QString("Failed to create file: %1").arg(fullPath));
    }
}

void EnhancedProjectExplorer::onDeleteSelected() {
    QList<QTreeWidgetItem*> selectedItems = m_treeWidget->selectedItems();
    if (selectedItems.isEmpty()) return;
    
    QStringList paths;
    for (QTreeWidgetItem* item : selectedItems) {
        QString path = item->data(0, Qt::UserRole).toString();
        if (!path.isEmpty()) {
            paths.append(path);
        }
    }
    
    if (paths.size() == 1) {
        QFileInfo info(paths.first());
        QString message = QString("Are you sure you want to delete '%1'?").arg(info.fileName());
        if (info.isDir()) {
            message += "\n\nThis will delete the folder and all its contents.";
        }
        
        if (QMessageBox::question(this, "Confirm Delete", message) != QMessageBox::Yes) {
            return;
        }
    } else {
        if (QMessageBox::question(this, "Confirm Delete", 
                                QString("Are you sure you want to delete %1 items?").arg(paths.size())) != QMessageBox::Yes) {
            return;
        }
    }
    
    // Perform deletion
    bool allDeleted = true;
    for (const QString& path : paths) {
        bool success = false;
        QFileInfo info(path);
        
        if (info.isDir()) {
            success = QDir(path).removeRecursively();
        } else {
            success = QFile(path).remove();
        }
        
        if (!success) {
            allDeleted = false;
            m_errorHandler->reportError("EnhancedProjectExplorer", 
                                       QString("Failed to delete: %1").arg(path));
        } else {
            m_loggingSystem->logInfo("EnhancedProjectExplorer", QString("Deleted: %1").arg(path));
        }
    }
    
    if (allDeleted) {
        refresh();
    }
}

void EnhancedProjectExplorer::onRenameSelected() {
    QTreeWidgetItem* item = m_treeWidget->currentItem();
    if (!item) return;
    
    QString oldPath = item->data(0, Qt::UserRole).toString();
    if (oldPath.isEmpty()) return;
    
    QFileInfo info(oldPath);
    QString oldName = info.fileName();
    
    bool ok;
    QString newName = QInputDialog::getText(this, "Rename", 
                                          "New name:", QLineEdit::Normal, oldName, &ok);
    if (!ok || newName.isEmpty() || newName == oldName) return;
    
    QString newPath = QDir(info.dir()).filePath(newName);
    
    if (QFile(oldPath).rename(newPath)) {
        m_loggingSystem->logInfo("EnhancedProjectExplorer", 
                                QString("Renamed: %1 -> %2").arg(oldPath).arg(newPath));
        refresh();
    } else {
        m_errorHandler->reportError("EnhancedProjectExplorer", 
                                  QString("Failed to rename: %1 -> %2").arg(oldPath).arg(newPath));
    }
}

void EnhancedProjectExplorer::onOpenInExplorer() {
    QString path = getCurrentSelectionPath();
    if (!path.isEmpty()) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(path));
    }
}

void EnhancedProjectExplorer::onFilterTextChanged(const QString& filter) {
    // Debounce filter changes to avoid excessive repainting
    m_refreshTimer.stop();
    m_refreshTimer.start(300); // 300ms delay
}

void EnhancedProjectExplorer::onFileSystemChanged(const QString& path) {
    // Only refresh if the changed file/folder is within our current project
    if (path.startsWith(m_projectPath)) {
        m_refreshTimer.stop();
        m_refreshTimer.start(500); // 500ms delay for file system changes
    }
}

QString EnhancedProjectExplorer::getCurrentSelectionPath() const {
    QTreeWidgetItem* currentItem = m_treeWidget->currentItem();
    if (currentItem) {
        return currentItem->data(0, Qt::UserRole).toString();
    }
    return QString();
}

QList<QTreeWidgetItem*> EnhancedProjectExplorer::getSelectedItems() const {
    return m_treeWidget->selectedItems();
}

void EnhancedProjectExplorer::updateStatusBar() {
    // Update status bar with current path and item count
    int totalItems = 0;
    int fileCount = 0;
    int dirCount = 0;
    
    // Count items recursively (simplified)
    QDir dir(m_currentPath);
    QFileInfoList entries = dir.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
    totalItems = entries.size();
    
    for (const QFileInfo& entry : entries) {
        if (entry.isFile()) fileCount++;
        else if (entry.isDir()) dirCount++;
    }
    
    m_statusLabel->setText(QString("Path: %1 | %2 files, %3 folders")
                          .arg(m_currentPath)
                          .arg(fileCount)
                          .arg(dirCount));
}

void EnhancedProjectExplorer::handleError(const QString& operation, const QString& error) {
    m_errorHandler->reportError("EnhancedProjectExplorer", 
                              QString("%1 failed: %2").arg(operation).arg(error));
    m_statusLabel->setText(QString("Error: %1").arg(error));
    m_progressBar->setVisible(false);
}

void EnhancedProjectExplorer::setShowHiddenFiles(bool show) {
    m_showHiddenFiles = show;
    refresh();
}

void EnhancedProjectExplorer::setShowFileExtensions(bool show) {
    m_showFileExtensions = show;
    refresh();
}

void EnhancedProjectExplorer::setTreeViewMode(bool expanded) {
    m_treeViewMode = expanded;
    refresh();
}

void EnhancedProjectExplorer::dragEnterEvent(QDragEnterEvent* event) {
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void EnhancedProjectExplorer::dropEvent(QDropEvent* event) {
    if (!event->mimeData()->hasUrls()) return;
    
    QString dropPath = getCurrentSelectionPath();
    if (dropPath.isEmpty() || !QFileInfo(dropPath).isDir()) {
        dropPath = m_currentPath;
    }
    
    for (const QUrl& url : event->mimeData()->urls()) {
        QString filePath = url.toLocalFile();
        QFileInfo srcInfo(filePath);
        QString dstPath = QDir(dropPath).filePath(srcInfo.fileName());
        
        if (srcInfo.isFile()) {
            QFile::copy(filePath, dstPath);
        } else if (srcInfo.isDir()) {
            QDir(srcInfo.filePath()).mkpath(dstPath);
        }
    }
    
    refresh();
}

void EnhancedProjectExplorer::setRootPath(const QString& path) {
    openProject(path);
}