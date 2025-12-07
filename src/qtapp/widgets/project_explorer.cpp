/**
 * \file project_explorer.cpp
 * \brief Implementation of production-grade project explorer
 * \author RawrXD Team
 * \date 2025-12-05
 */

#include "project_explorer.h"
// Default implementations (used when nullptr is passed to constructor)
#include "../utils/qt_file_writer.h"
#include "../utils/qt_directory_manager.h"
#include <QHeaderView>
#include <QMessageBox>
#include <QInputDialog>
#include <QClipboard>
#include <QApplication>
#include <QDesktopServices>
#include <QUrl>
#include <QFileDialog>
#include <QDebug>
#include <QMimeData>
#include <QDrag>
#include <QRegularExpression>

namespace RawrXD {

// ========== ProjectExplorerWidget Implementation ==========

ProjectExplorerWidget::ProjectExplorerWidget(QWidget* parent,
                                           IFileWriter* fileWriter,
                                           IDirectoryManager* dirManager)
    : QWidget(parent)
    , m_mainLayout(nullptr)
    , m_toolbar(nullptr)
    , m_filterEdit(nullptr)
    , m_treeView(nullptr)
    , m_projectInfoLabel(nullptr)
    , m_fileSystemModel(nullptr)
    , m_contextMenu(nullptr)
    , m_clipboardIsCut(false)
    , m_showHiddenFiles(false)
    , m_fileWriter(fileWriter)
    , m_dirManager(dirManager)
    , m_ownsFileWriter(false)
    , m_ownsDirManager(false)
{
    // If no concrete implementations are provided, create default Qt ones
    if (!m_fileWriter) {
        m_fileWriter = new QtFileWriter(this);
        m_ownsFileWriter = true;
    }
    if (!m_dirManager) {
        m_dirManager = new QtDirectoryManager(this);
        m_ownsDirManager = true;
    }

    setupUI();
    setupContextMenu();
}

ProjectExplorerWidget::~ProjectExplorerWidget() {
    if (!m_projectPath.isEmpty()) {
        saveProjectMetadata();
    }
    // Clean up owned default implementations
    if (m_ownsFileWriter && m_fileWriter) {
        delete m_fileWriter;
    }
    if (m_ownsDirManager && m_dirManager) {
        delete m_dirManager;
    }
}

void ProjectExplorerWidget::setupUI() {
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(0, 0, 0, 0);
    m_mainLayout->setSpacing(0);
    
    // Toolbar
    createToolbar();
    m_mainLayout->addWidget(m_toolbar);
    
    // Filter/search box
    m_filterEdit = new QLineEdit(this);
    m_filterEdit->setPlaceholderText("Filter files...");
    m_filterEdit->setClearButtonEnabled(true);
    connect(m_filterEdit, &QLineEdit::textChanged, this, &ProjectExplorerWidget::onFilterTextChanged);
    m_mainLayout->addWidget(m_filterEdit);
    
    // Tree view
    m_treeView = new QTreeView(this);
    m_treeView->setHeaderHidden(false);
    m_treeView->setContextMenuPolicy(Qt::CustomContextMenu);
    m_treeView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_treeView->setDragEnabled(true);
    m_treeView->setAcceptDrops(true);
    m_treeView->setDropIndicatorShown(true);
    m_treeView->setDragDropMode(QAbstractItemView::InternalMove);
    
    connect(m_treeView, &QTreeView::doubleClicked, this, &ProjectExplorerWidget::onTreeDoubleClicked);
    connect(m_treeView, &QTreeView::clicked, this, &ProjectExplorerWidget::onTreeClicked);
    connect(m_treeView, &QTreeView::customContextMenuRequested, this, &ProjectExplorerWidget::onContextMenuRequested);
    
    m_mainLayout->addWidget(m_treeView);
    
    // Project info label at bottom
    m_projectInfoLabel = new QLabel("No project open", this);
    m_projectInfoLabel->setStyleSheet("QLabel { padding: 4px; background-color: #2d2d30; color: #cccccc; }");
    m_projectInfoLabel->setWordWrap(true);
    m_mainLayout->addWidget(m_projectInfoLabel);
    
    // File system model
    m_fileSystemModel = new QFileSystemModel(this);
    m_fileSystemModel->setReadOnly(false);  // Allow renames via model
    m_fileSystemModel->setFilter(QDir::AllEntries | QDir::NoDotAndDotDot);
    
    m_treeView->setModel(m_fileSystemModel);
    
    // Hide size, type, date columns (can be re-enabled)
    m_treeView->setColumnHidden(1, true);  // Size
    m_treeView->setColumnHidden(2, true);  // Type
    m_treeView->setColumnHidden(3, true);  // Date Modified
}

void ProjectExplorerWidget::createToolbar() {
    m_toolbar = new QToolBar(this);
    m_toolbar->setIconSize(QSize(16, 16));
    m_toolbar->setToolButtonStyle(Qt::ToolButtonIconOnly);
    
    // New file button
    QAction* newFileAction = m_toolbar->addAction("New File");
    newFileAction->setToolTip("Create new file (Ctrl+N)");
    newFileAction->setShortcut(QKeySequence::New);
    connect(newFileAction, &QAction::triggered, this, &ProjectExplorerWidget::actionNewFile);
    
    // New folder button
    QAction* newFolderAction = m_toolbar->addAction("New Folder");
    newFolderAction->setToolTip("Create new folder");
    connect(newFolderAction, &QAction::triggered, this, &ProjectExplorerWidget::actionNewFolder);
    
    m_toolbar->addSeparator();
    
    // Refresh button
    QAction* refreshAction = m_toolbar->addAction("Refresh");
    refreshAction->setToolTip("Refresh file tree (F5)");
    refreshAction->setShortcut(QKeySequence::Refresh);
    connect(refreshAction, &QAction::triggered, this, &ProjectExplorerWidget::actionRefresh);
    
    // Collapse all button
    QAction* collapseAction = m_toolbar->addAction("Collapse All");
    collapseAction->setToolTip("Collapse all folders");
    connect(collapseAction, &QAction::triggered, m_treeView, &QTreeView::collapseAll);
}

void ProjectExplorerWidget::setupContextMenu() {
    m_contextMenu = new QMenu(this);
    
    m_actionNewFile = m_contextMenu->addAction("New File...", this, &ProjectExplorerWidget::actionNewFile);
    m_actionNewFolder = m_contextMenu->addAction("New Folder...", this, &ProjectExplorerWidget::actionNewFolder);
    
    m_contextMenu->addSeparator();
    
    m_actionCut = m_contextMenu->addAction("Cut", this, &ProjectExplorerWidget::actionCut);
    m_actionCut->setShortcut(QKeySequence::Cut);
    
    m_actionCopy = m_contextMenu->addAction("Copy", this, &ProjectExplorerWidget::actionCopy);
    m_actionCopy->setShortcut(QKeySequence::Copy);
    
    m_actionPaste = m_contextMenu->addAction("Paste", this, &ProjectExplorerWidget::actionPaste);
    m_actionPaste->setShortcut(QKeySequence::Paste);
    m_actionPaste->setEnabled(false);  // Initially disabled
    
    m_contextMenu->addSeparator();
    
    m_actionRename = m_contextMenu->addAction("Rename...", this, &ProjectExplorerWidget::actionRename);
    m_actionRename->setShortcut(QKeySequence(Qt::Key_F2));
    
    m_actionDelete = m_contextMenu->addAction("Delete", this, &ProjectExplorerWidget::actionDelete);
    m_actionDelete->setShortcut(QKeySequence::Delete);
    
    m_contextMenu->addSeparator();
    
    m_actionCopyPath = m_contextMenu->addAction("Copy Path", this, &ProjectExplorerWidget::actionCopyPath);
    m_actionCopyRelativePath = m_contextMenu->addAction("Copy Relative Path", this, &ProjectExplorerWidget::actionCopyRelativePath);
    
    m_contextMenu->addSeparator();
    
    m_actionRevealInExplorer = m_contextMenu->addAction("Reveal in File Explorer", this, &ProjectExplorerWidget::actionRevealInExplorer);
    
    m_contextMenu->addSeparator();
    
    m_actionRefresh = m_contextMenu->addAction("Refresh", this, &ProjectExplorerWidget::actionRefresh);
}

bool ProjectExplorerWidget::openProject(const QString& projectPath) {
    if (projectPath.isEmpty() || !QFileInfo(projectPath).isDir()) {
        qWarning() << "Invalid project path:" << projectPath;
        return false;
    }
    
    // Close existing project
    if (!m_projectPath.isEmpty()) {
        closeProject();
    }
    
    m_projectPath = QFileInfo(projectPath).absoluteFilePath();
    
    // Detect project type and metadata
    m_projectMetadata = m_projectDetector.detectProject(m_projectPath);
    
    // Set root path in file system model
    QModelIndex rootIndex = m_fileSystemModel->setRootPath(m_projectPath);
    m_treeView->setRootIndex(rootIndex);
    
    // Load .gitignore patterns
    loadGitignorePatterns();
    
    // Update UI
    updateProjectInfo();
    
    // Expand root
    m_treeView->expand(rootIndex);
    
    emit projectOpened(m_projectPath);
    
    qDebug() << "Opened project:" << m_projectPath << "Type:" << ProjectDetector::projectTypeName(m_projectMetadata.type);
    
    return true;
}

QString ProjectExplorerWidget::currentProjectPath() const {
    return m_projectPath;
}

ProjectMetadata ProjectExplorerWidget::currentProjectMetadata() const {
    return m_projectMetadata;
}

void ProjectExplorerWidget::closeProject() {
    if (!m_projectPath.isEmpty()) {
        saveProjectMetadata();
        m_projectPath.clear();
        m_projectMetadata = ProjectMetadata();
        m_gitignorePatterns.clear();
        
        m_fileSystemModel->setRootPath("");
        m_treeView->setRootIndex(QModelIndex());
        
        updateProjectInfo();
        emit projectClosed();
    }
}

void ProjectExplorerWidget::refresh() {
    if (!m_projectPath.isEmpty()) {
        // Reload gitignore
        loadGitignorePatterns();
        
        // Force model refresh
        QModelIndex root = m_treeView->rootIndex();
        m_treeView->setRootIndex(QModelIndex());
        m_treeView->setRootIndex(root);
    }
}

QString ProjectExplorerWidget::selectedFilePath() const {
    QModelIndex index = m_treeView->currentIndex();
    if (!index.isValid()) {
        return QString();
    }
    return m_fileSystemModel->filePath(index);
}

QStringList ProjectExplorerWidget::selectedFilePaths() const {
    QStringList paths;
    QModelIndexList indexes = m_treeView->selectionModel()->selectedRows();
    for (const QModelIndex& index : indexes) {
        paths.append(m_fileSystemModel->filePath(index));
    }
    return paths;
}

void ProjectExplorerWidget::selectFile(const QString& filePath) {
    QModelIndex index = m_fileSystemModel->index(filePath);
    if (index.isValid()) {
        m_treeView->setCurrentIndex(index);
        m_treeView->scrollTo(index);
    }
}

void ProjectExplorerWidget::expandDirectory(const QString& dirPath) {
    QModelIndex index = m_fileSystemModel->index(dirPath);
    if (index.isValid()) {
        m_treeView->expand(index);
    }
}

void ProjectExplorerWidget::collapseDirectory(const QString& dirPath) {
    QModelIndex index = m_fileSystemModel->index(dirPath);
    if (index.isValid()) {
        m_treeView->collapse(index);
    }
}

void ProjectExplorerWidget::setShowHiddenFiles(bool show) {
    m_showHiddenFiles = show;
    QDir::Filters filter = QDir::AllEntries | QDir::NoDotAndDotDot;
    if (!show) {
        filter |= QDir::Hidden;
    }
    m_fileSystemModel->setFilter(filter);
}

bool ProjectExplorerWidget::showHiddenFiles() const {
    return m_showHiddenFiles;
}

void ProjectExplorerWidget::setFileFilter(const QString& pattern) {
    if (pattern.isEmpty()) {
        m_fileSystemModel->setNameFilters(QStringList());
        m_fileSystemModel->setNameFilterDisables(false);
    } else {
        QStringList filters = pattern.split(' ', Qt::SkipEmptyParts);
        m_fileSystemModel->setNameFilters(filters);
        m_fileSystemModel->setNameFilterDisables(false);
    }
}

// ========== Slots ==========

void ProjectExplorerWidget::onTreeDoubleClicked(const QModelIndex& index) {
    if (!index.isValid()) return;
    
    QString filePath = m_fileSystemModel->filePath(index);
    QFileInfo info(filePath);
    
    if (info.isFile()) {
        emit fileDoubleClicked(filePath);
        
        // Add to recent files
        ProjectDetector::addRecentFile(m_projectMetadata, filePath);
        saveProjectMetadata();
    }
}

void ProjectExplorerWidget::onTreeClicked(const QModelIndex& index) {
    if (!index.isValid()) return;
    
    QString filePath = m_fileSystemModel->filePath(index);
    emit fileClicked(filePath);
}

void ProjectExplorerWidget::onContextMenuRequested(const QPoint& pos) {
    QModelIndex index = m_treeView->indexAt(pos);
    
    // Enable/disable actions based on selection
    bool hasSelection = index.isValid();
    m_actionRename->setEnabled(hasSelection);
    m_actionDelete->setEnabled(hasSelection);
    m_actionCopy->setEnabled(hasSelection);
    m_actionCut->setEnabled(hasSelection);
    m_actionCopyPath->setEnabled(hasSelection);
    m_actionCopyRelativePath->setEnabled(hasSelection);
    m_actionRevealInExplorer->setEnabled(hasSelection);
    m_actionPaste->setEnabled(!m_clipboardPath.isEmpty());
    
    m_contextMenu->exec(m_treeView->viewport()->mapToGlobal(pos));
}

void ProjectExplorerWidget::onFilterTextChanged(const QString& text) {
    setFileFilter(text);
}

// ========== Context Menu Actions ==========

void ProjectExplorerWidget::actionNewFile() {
    QString parentDir = m_projectPath;
    QModelIndex index = m_treeView->currentIndex();
    if (index.isValid()) {
        QString path = m_fileSystemModel->filePath(index);
        QFileInfo info(path);
        parentDir = info.isDir() ? path : info.absolutePath();
    }
    
    bool ok;
    QString fileName = QInputDialog::getText(this, "New File", 
                                             "Enter file name:", 
                                             QLineEdit::Normal,
                                             "newfile.txt", &ok);
    if (!ok || fileName.isEmpty()) {
        return;
    }
    
    QString filePath = QDir(parentDir).filePath(fileName);
    
    FileOperationResult result = m_fileWriter->createFile(filePath);
    if (result.success) {
        emit fileCreated(filePath);
        selectFile(filePath);
        qDebug() << "Created file:" << filePath;
    } else {
        QMessageBox::warning(this, "Create File Failed", result.errorMessage);
    }
}

void ProjectExplorerWidget::actionNewFolder() {
    QString parentDir = m_projectPath;
    QModelIndex index = m_treeView->currentIndex();
    if (index.isValid()) {
        QString path = m_fileSystemModel->filePath(index);
        QFileInfo info(path);
        parentDir = info.isDir() ? path : info.absolutePath();
    }
    
    bool ok;
    QString folderName = QInputDialog::getText(this, "New Folder", 
                                               "Enter folder name:", 
                                               QLineEdit::Normal,
                                               "newfolder", &ok);
    if (!ok || folderName.isEmpty()) {
        return;
    }
    
    QString folderPath = QDir(parentDir).filePath(folderName);
    
    FileOperationResult result = m_dirManager->createDirectory(folderPath);
    if (result.success) {
        selectFile(folderPath);
        qDebug() << "Created folder:" << folderPath;
    } else {
        QMessageBox::warning(this, "Create Folder Failed", result.errorMessage);
    }
}

void ProjectExplorerWidget::actionRename() {
    QString oldPath = selectedFilePath();
    if (oldPath.isEmpty()) return;
    
    QFileInfo info(oldPath);
    QString oldName = info.fileName();
    
    bool ok;
    QString newName = QInputDialog::getText(this, "Rename", 
                                           "Enter new name:", 
                                           QLineEdit::Normal,
                                           oldName, &ok);
    if (!ok || newName.isEmpty() || newName == oldName) {
        return;
    }
    
    QString newPath = QDir(info.absolutePath()).filePath(newName);
    
    FileOperationResult result = m_fileWriter->renameFile(oldPath, newPath);
    if (result.success) {
        emit fileRenamed(oldPath, newPath);
        selectFile(newPath);
        qDebug() << "Renamed:" << oldPath << "→" << newPath;
    } else {
        QMessageBox::warning(this, "Rename Failed", result.errorMessage);
    }
}

void ProjectExplorerWidget::actionDelete() {
    QStringList paths = selectedFilePaths();
    if (paths.isEmpty()) return;
    
    QString message = paths.size() == 1 
        ? QString("Delete '%1'?").arg(QFileInfo(paths.first()).fileName())
        : QString("Delete %1 items?").arg(paths.size());
    
    QMessageBox::StandardButton reply = QMessageBox::question(
        this, "Confirm Delete", message,
        QMessageBox::Yes | QMessageBox::No);
    
    if (reply != QMessageBox::Yes) {
        return;
    }
    
    for (const QString& path : paths) {
        FileOperationResult result;
        if (QFileInfo(path).isDir()) {
            result = m_dirManager->deleteDirectory(path, true);
        } else {
            result = m_fileWriter->deleteFile(path, true);
        }
        
        if (result.success) {
            emit fileDeleted(path);
            qDebug() << "Deleted:" << path;
        } else {
            QMessageBox::warning(this, "Delete Failed", 
                               QString("Failed to delete '%1': %2")
                               .arg(QFileInfo(path).fileName())
                               .arg(result.errorMessage));
        }
    }
}

void ProjectExplorerWidget::actionCopy() {
    m_clipboardPath = selectedFilePath();
    m_clipboardIsCut = false;
    m_actionPaste->setEnabled(!m_clipboardPath.isEmpty());
    qDebug() << "Copied to clipboard:" << m_clipboardPath;
}

void ProjectExplorerWidget::actionCut() {
    m_clipboardPath = selectedFilePath();
    m_clipboardIsCut = true;
    m_actionPaste->setEnabled(!m_clipboardPath.isEmpty());
    qDebug() << "Cut to clipboard:" << m_clipboardPath;
}

void ProjectExplorerWidget::actionPaste() {
    if (m_clipboardPath.isEmpty()) return;
    
    QString destDir = m_projectPath;
    QModelIndex index = m_treeView->currentIndex();
    if (index.isValid()) {
        QString path = m_fileSystemModel->filePath(index);
        QFileInfo info(path);
        destDir = info.isDir() ? path : info.absolutePath();
    }
    
    FileOperationResult result;
    if (m_clipboardIsCut) {
        // Move
        // Move operation: compute destination file path and use renameFile
        {
            QFileInfo srcInfo(m_clipboardPath);
            QString destPath = QDir(destDir).filePath(srcInfo.fileName());
            result = m_fileWriter->renameFile(m_clipboardPath, destPath);
        }
        if (result.success) {
            m_clipboardPath.clear();
            m_actionPaste->setEnabled(false);
            qDebug() << "Moved (cut+paste)";
        }
    } else {
        // Copy
        // Copy operation: compute destination file path and use copyFile
        {
            QFileInfo srcInfo(m_clipboardPath);
            QString destPath = QDir(destDir).filePath(srcInfo.fileName());
            result = m_fileWriter->copyFile(m_clipboardPath, destPath, false);
        }
        qDebug() << "Copied (copy+paste)";
    }
    
    if (!result.success) {
        QMessageBox::warning(this, "Paste Failed", result.errorMessage);
    }
}

void ProjectExplorerWidget::actionRevealInExplorer() {
    QString path = selectedFilePath();
    if (path.isEmpty()) return;
    
    // Open file explorer at this location
    QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(path).absolutePath()));
}

void ProjectExplorerWidget::actionCopyPath() {
    QString path = selectedFilePath();
    if (path.isEmpty()) return;
    
    QApplication::clipboard()->setText(path);
    qDebug() << "Copied absolute path to clipboard:" << path;
}

void ProjectExplorerWidget::actionCopyRelativePath() {
    QString path = selectedFilePath();
    if (path.isEmpty() || m_projectPath.isEmpty()) return;
    
    QString relativePath = FileManager::toRelativePath(path, m_projectPath);
    QApplication::clipboard()->setText(relativePath);
    qDebug() << "Copied relative path to clipboard:" << relativePath;
}

void ProjectExplorerWidget::actionRefresh() {
    refresh();
}

// ========== Private Methods ==========

void ProjectExplorerWidget::loadProjectMetadata() {
    if (m_projectPath.isEmpty()) return;
    
    if (m_projectDetector.hasProjectMetadata(m_projectPath)) {
        m_projectMetadata = m_projectDetector.loadProjectMetadata(m_projectPath);
    }
}

void ProjectExplorerWidget::saveProjectMetadata() {
    if (m_projectPath.isEmpty()) return;
    
    m_projectMetadata.lastOpened = QDateTime::currentDateTime();
    m_projectDetector.saveProjectMetadata(m_projectMetadata);
}

void ProjectExplorerWidget::updateProjectInfo() {
    if (m_projectPath.isEmpty()) {
        m_projectInfoLabel->setText("No project open");
    } else {
        QString typeName = ProjectDetector::projectTypeName(m_projectMetadata.type);
        QString info = QString("<b>%1</b><br/>%2<br/>%3")
            .arg(m_projectMetadata.name)
            .arg(typeName)
            .arg(m_projectPath);
        
        if (!m_projectMetadata.gitBranch.isEmpty()) {
            info += QString("<br/>Branch: %1").arg(m_projectMetadata.gitBranch);
        }
        
        m_projectInfoLabel->setText(info);
    }
}

bool ProjectExplorerWidget::isFileIgnored(const QString& filePath) const {
    if (m_gitignorePatterns.isEmpty()) {
        return false;
    }
    
    QString relativePath = FileManager::toRelativePath(filePath, m_projectPath);
    
    for (const QString& pattern : m_gitignorePatterns) {
        // Simple pattern matching (could be improved with full gitignore spec)
        QRegularExpression regex(QRegularExpression::wildcardToRegularExpression(pattern));
        if (regex.match(relativePath).hasMatch()) {
            return true;
        }
    }
    
    return false;
}

void ProjectExplorerWidget::loadGitignorePatterns() {
    m_gitignorePatterns.clear();
    
    if (m_projectPath.isEmpty()) return;
    
    QString gitignorePath = QDir(m_projectPath).filePath(".gitignore");
    if (!QFileInfo::exists(gitignorePath)) {
        return;
    }
    
    QFile file(gitignorePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return;
    }
    
    QTextStream stream(&file);
    while (!stream.atEnd()) {
        QString line = stream.readLine().trimmed();
        
        // Skip comments and empty lines
        if (line.isEmpty() || line.startsWith('#')) {
            continue;
        }
        
        m_gitignorePatterns.insert(line);
    }
    
    qDebug() << "Loaded" << m_gitignorePatterns.size() << "gitignore patterns";
}

// ========== GitignoreFilter Implementation ==========

GitignoreFilter::GitignoreFilter() = default;

void GitignoreFilter::loadFromFile(const QString& gitignorePath) {
    clear();
    
    QFile file(gitignorePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return;
    }
    
    QTextStream stream(&file);
    while (!stream.atEnd()) {
        QString line = stream.readLine().trimmed();
        if (!line.isEmpty() && !line.startsWith('#')) {
            m_patterns.append(line);
        }
    }
}

void GitignoreFilter::addPattern(const QString& pattern) {
    if (!pattern.trimmed().isEmpty()) {
        m_patterns.append(pattern.trimmed());
    }
}

bool GitignoreFilter::shouldIgnore(const QString& filePath, const QString& basePath) const {
    QString checkPath = basePath.isEmpty() ? filePath : FileManager::toRelativePath(filePath, basePath);
    
    for (const QString& pattern : m_patterns) {
        if (matchPattern(checkPath, pattern)) {
            return true;
        }
    }
    
    return false;
}

void GitignoreFilter::clear() {
    m_patterns.clear();
}

bool GitignoreFilter::matchPattern(const QString& filePath, const QString& pattern) {
    // Simple wildcard matching (full .gitignore spec is more complex)
    QRegularExpression regex(QRegularExpression::wildcardToRegularExpression(pattern));
    return regex.match(filePath).hasMatch();
}

} // namespace RawrXD
