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


namespace RawrXD {

// ========== ProjectExplorerWidget Implementation ==========

ProjectExplorerWidget::ProjectExplorerWidget(void* parent,
                                           IFileWriter* fileWriter,
                                           IDirectoryManager* dirManager)
    : void(parent)
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
// Qt connect removed
    m_mainLayout->addWidget(m_filterEdit);
    
    // Tree view
    m_treeView = new QTreeView(this);
    m_treeView->setHeaderHidden(false);
    m_treeView->setContextMenuPolicy(//CustomContextMenu);
    m_treeView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_treeView->setDragEnabled(true);
    m_treeView->setAcceptDrops(true);
    m_treeView->setDropIndicatorShown(true);
    m_treeView->setDragDropMode(QAbstractItemView::InternalMove);
// Qt connect removed
// Qt connect removed
// Qt connect removed
    m_mainLayout->addWidget(m_treeView);
    
    // Project info label at bottom
    m_projectInfoLabel = new QLabel("No project open", this);
    m_projectInfoLabel->setStyleSheet("QLabel { padding: 4px; background-color: #2d2d30; color: #cccccc; }");
    m_projectInfoLabel->setWordWrap(true);
    m_mainLayout->addWidget(m_projectInfoLabel);
    
    // File system model
    m_fileSystemModel = new QFileSystemModel(this);
    m_fileSystemModel->setReadOnly(false);  // Allow renames via model
    m_fileSystemModel->setFilter(std::filesystem::path::AllEntries | std::filesystem::path::NoDotAndDotDot);
    
    m_treeView->setModel(m_fileSystemModel);
    
    // Hide size, type, date columns (can be re-enabled)
    m_treeView->setColumnHidden(1, true);  // Size
    m_treeView->setColumnHidden(2, true);  // Type
    m_treeView->setColumnHidden(3, true);  // Date Modified
}

void ProjectExplorerWidget::createToolbar() {
    m_toolbar = new QToolBar(this);
    m_toolbar->setIconSize(QSize(16, 16));
    m_toolbar->setToolButtonStyle(//ToolButtonIconOnly);
    
    // New file button
    QAction* newFileAction = m_toolbar->addAction("New File");
    newFileAction->setToolTip("Create new file (Ctrl+N)");
    newFileAction->setShortcut(QKeySequence::New);
// Qt connect removed
    // New folder button
    QAction* newFolderAction = m_toolbar->addAction("New Folder");
    newFolderAction->setToolTip("Create new folder");
// Qt connect removed
    m_toolbar->addSeparator();
    
    // Refresh button
    QAction* refreshAction = m_toolbar->addAction("Refresh");
    refreshAction->setToolTip("Refresh file tree (F5)");
    refreshAction->setShortcut(QKeySequence::Refresh);
// Qt connect removed
    // Collapse all button
    QAction* collapseAction = m_toolbar->addAction("Collapse All");
    collapseAction->setToolTip("Collapse all folders");
// Qt connect removed
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
    m_actionRename->setShortcut(QKeySequence(//Key_F2));
    
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

bool ProjectExplorerWidget::openProject(const std::string& projectPath) {
    if (projectPath.isEmpty() || !std::filesystem::path(projectPath).isDir()) {
        return false;
    }
    
    // Close existing project
    if (!m_projectPath.isEmpty()) {
        closeProject();
    }
    
    m_projectPath = std::filesystem::path(projectPath).absoluteFilePath();
    
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
    
    projectOpened(m_projectPath);
    
    
    return true;
}

std::string ProjectExplorerWidget::currentProjectPath() const {
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
        projectClosed();
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

std::string ProjectExplorerWidget::selectedFilePath() const {
    QModelIndex index = m_treeView->currentIndex();
    if (!index.isValid()) {
        return std::string();
    }
    return m_fileSystemModel->filePath(index);
}

std::vector<std::string> ProjectExplorerWidget::selectedFilePaths() const {
    std::vector<std::string> paths;
    QModelIndexList indexes = m_treeView->selectionModel()->selectedRows();
    for (const QModelIndex& index : indexes) {
        paths.append(m_fileSystemModel->filePath(index));
    }
    return paths;
}

void ProjectExplorerWidget::selectFile(const std::string& filePath) {
    QModelIndex index = m_fileSystemModel->index(filePath);
    if (index.isValid()) {
        m_treeView->setCurrentIndex(index);
        m_treeView->scrollTo(index);
    }
}

void ProjectExplorerWidget::expandDirectory(const std::string& dirPath) {
    QModelIndex index = m_fileSystemModel->index(dirPath);
    if (index.isValid()) {
        m_treeView->expand(index);
    }
}

void ProjectExplorerWidget::collapseDirectory(const std::string& dirPath) {
    QModelIndex index = m_fileSystemModel->index(dirPath);
    if (index.isValid()) {
        m_treeView->collapse(index);
    }
}

void ProjectExplorerWidget::setShowHiddenFiles(bool show) {
    m_showHiddenFiles = show;
    std::filesystem::path::Filters filter = std::filesystem::path::AllEntries | std::filesystem::path::NoDotAndDotDot;
    if (!show) {
        filter |= std::filesystem::path::Hidden;
    }
    m_fileSystemModel->setFilter(filter);
}

bool ProjectExplorerWidget::showHiddenFiles() const {
    return m_showHiddenFiles;
}

void ProjectExplorerWidget::setFileFilter(const std::string& pattern) {
    if (pattern.isEmpty()) {
        m_fileSystemModel->setNameFilters(std::vector<std::string>());
        m_fileSystemModel->setNameFilterDisables(false);
    } else {
        std::vector<std::string> filters = pattern.split(' ', //SkipEmptyParts);
        m_fileSystemModel->setNameFilters(filters);
        m_fileSystemModel->setNameFilterDisables(false);
    }
}

// ========== Slots ==========

void ProjectExplorerWidget::onTreeDoubleClicked(const QModelIndex& index) {
    if (!index.isValid()) return;
    
    std::string filePath = m_fileSystemModel->filePath(index);
    std::filesystem::path info(filePath);
    
    if (info.isFile()) {
        fileDoubleClicked(filePath);
        
        // Add to recent files
        ProjectDetector::addRecentFile(m_projectMetadata, filePath);
        saveProjectMetadata();
    }
}

void ProjectExplorerWidget::onTreeClicked(const QModelIndex& index) {
    if (!index.isValid()) return;
    
    std::string filePath = m_fileSystemModel->filePath(index);
    fileClicked(filePath);
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

void ProjectExplorerWidget::onFilterTextChanged(const std::string& text) {
    setFileFilter(text);
}

// ========== Context Menu Actions ==========

void ProjectExplorerWidget::actionNewFile() {
    std::string parentDir = m_projectPath;
    QModelIndex index = m_treeView->currentIndex();
    if (index.isValid()) {
        std::string path = m_fileSystemModel->filePath(index);
        std::filesystem::path info(path);
        parentDir = info.isDir() ? path : info.absolutePath();
    }
    
    bool ok;
    std::string fileName = QInputDialog::getText(this, "New File", 
                                             "Enter file name:", 
                                             QLineEdit::Normal,
                                             "newfile.txt", &ok);
    if (!ok || fileName.isEmpty()) {
        return;
    }
    
    std::string filePath = std::filesystem::path(parentDir).filePath(fileName);
    
    FileOperationResult result = m_fileWriter->createFile(filePath);
    if (result.success) {
        fileCreated(filePath);
        selectFile(filePath);
    } else {
        QMessageBox::warning(this, "Create File Failed", result.errorMessage);
    }
}

void ProjectExplorerWidget::actionNewFolder() {
    std::string parentDir = m_projectPath;
    QModelIndex index = m_treeView->currentIndex();
    if (index.isValid()) {
        std::string path = m_fileSystemModel->filePath(index);
        std::filesystem::path info(path);
        parentDir = info.isDir() ? path : info.absolutePath();
    }
    
    bool ok;
    std::string folderName = QInputDialog::getText(this, "New Folder", 
                                               "Enter folder name:", 
                                               QLineEdit::Normal,
                                               "newfolder", &ok);
    if (!ok || folderName.isEmpty()) {
        return;
    }
    
    std::string folderPath = std::filesystem::path(parentDir).filePath(folderName);
    
    FileOperationResult result = m_dirManager->createDirectory(folderPath);
    if (result.success) {
        selectFile(folderPath);
    } else {
        QMessageBox::warning(this, "Create Folder Failed", result.errorMessage);
    }
}

void ProjectExplorerWidget::actionRename() {
    std::string oldPath = selectedFilePath();
    if (oldPath.isEmpty()) return;
    
    std::filesystem::path info(oldPath);
    std::string oldName = info.fileName();
    
    bool ok;
    std::string newName = QInputDialog::getText(this, "Rename", 
                                           "Enter new name:", 
                                           QLineEdit::Normal,
                                           oldName, &ok);
    if (!ok || newName.isEmpty() || newName == oldName) {
        return;
    }
    
    std::string newPath = std::filesystem::path(info.absolutePath()).filePath(newName);
    
    FileOperationResult result = m_fileWriter->renameFile(oldPath, newPath);
    if (result.success) {
        fileRenamed(oldPath, newPath);
        selectFile(newPath);
    } else {
        QMessageBox::warning(this, "Rename Failed", result.errorMessage);
    }
}

void ProjectExplorerWidget::actionDelete() {
    std::vector<std::string> paths = selectedFilePaths();
    if (paths.isEmpty()) return;
    
    std::string message = paths.size() == 1 
        ? std::string("Delete '%1'?")).fileName())
        : std::string("Delete %1 items?"));
    
    QMessageBox::StandardButton reply = QMessageBox::question(
        this, "Confirm Delete", message,
        QMessageBox::Yes | QMessageBox::No);
    
    if (reply != QMessageBox::Yes) {
        return;
    }
    
    for (const std::string& path : paths) {
        FileOperationResult result;
        if (std::filesystem::path(path).isDir()) {
            result = m_dirManager->deleteDirectory(path, true);
        } else {
            result = m_fileWriter->deleteFile(path, true);
        }
        
        if (result.success) {
            fileDeleted(path);
        } else {
            QMessageBox::warning(this, "Delete Failed", 
                               std::string("Failed to delete '%1': %2")
                               .fileName())
                               );
        }
    }
}

void ProjectExplorerWidget::actionCopy() {
    m_clipboardPath = selectedFilePath();
    m_clipboardIsCut = false;
    m_actionPaste->setEnabled(!m_clipboardPath.isEmpty());
}

void ProjectExplorerWidget::actionCut() {
    m_clipboardPath = selectedFilePath();
    m_clipboardIsCut = true;
    m_actionPaste->setEnabled(!m_clipboardPath.isEmpty());
}

void ProjectExplorerWidget::actionPaste() {
    if (m_clipboardPath.isEmpty()) return;
    
    std::string destDir = m_projectPath;
    QModelIndex index = m_treeView->currentIndex();
    if (index.isValid()) {
        std::string path = m_fileSystemModel->filePath(index);
        std::filesystem::path info(path);
        destDir = info.isDir() ? path : info.absolutePath();
    }
    
    FileOperationResult result;
    if (m_clipboardIsCut) {
        // Move
        // Move operation: compute destination file path and use renameFile
        {
            std::filesystem::path srcInfo(m_clipboardPath);
            std::string destPath = std::filesystem::path(destDir).filePath(srcInfo.fileName());
            result = m_fileWriter->renameFile(m_clipboardPath, destPath);
        }
        if (result.success) {
            m_clipboardPath.clear();
            m_actionPaste->setEnabled(false);
        }
    } else {
        // Copy
        // Copy operation: compute destination file path and use copyFile
        {
            std::filesystem::path srcInfo(m_clipboardPath);
            std::string destPath = std::filesystem::path(destDir).filePath(srcInfo.fileName());
            result = m_fileWriter->copyFile(m_clipboardPath, destPath, false);
        }
    }
    
    if (!result.success) {
        QMessageBox::warning(this, "Paste Failed", result.errorMessage);
    }
}

void ProjectExplorerWidget::actionRevealInExplorer() {
    std::string path = selectedFilePath();
    if (path.isEmpty()) return;
    
    // Open file explorer at this location
    QDesktopServices::openUrl(std::string::fromLocalFile(std::filesystem::path(path).absolutePath()));
}

void ProjectExplorerWidget::actionCopyPath() {
    std::string path = selectedFilePath();
    if (path.isEmpty()) return;
    
    nullptr->setText(path);
}

void ProjectExplorerWidget::actionCopyRelativePath() {
    std::string path = selectedFilePath();
    if (path.isEmpty() || m_projectPath.isEmpty()) return;
    
    std::string relativePath = m_dirManager ? m_dirManager->toRelativePath(path, m_projectPath) : path;
    nullptr->setText(relativePath);
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
    
    m_projectMetadata.lastOpened = std::chrono::system_clock::time_point::currentDateTime();
    m_projectDetector.saveProjectMetadata(m_projectMetadata);
}

void ProjectExplorerWidget::updateProjectInfo() {
    if (m_projectPath.isEmpty()) {
        m_projectInfoLabel->setText("No project open");
    } else {
        std::string typeName = ProjectDetector::projectTypeName(m_projectMetadata.type);
        std::string info = std::string("<b>%1</b><br/>%2<br/>%3")


            ;
        
        if (!m_projectMetadata.gitBranch.isEmpty()) {
            info += std::string("<br/>Branch: %1");
        }
        
        m_projectInfoLabel->setText(info);
    }
}

bool ProjectExplorerWidget::isFileIgnored(const std::string& filePath) const {
    if (m_gitignorePatterns.isEmpty()) {
        return false;
    }
    
    std::string relativePath = m_dirManager ? m_dirManager->toRelativePath(filePath, m_projectPath) : filePath;
    
    for (const std::string& pattern : m_gitignorePatterns) {
        // Simple pattern matching (could be improved with full gitignore spec)
        std::regex regex(std::regex::wildcardToRegularExpression(pattern));
        if (regex.match(relativePath).hasMatch()) {
            return true;
        }
    }
    
    return false;
}

void ProjectExplorerWidget::loadGitignorePatterns() {
    m_gitignorePatterns.clear();
    
    if (m_projectPath.isEmpty()) return;
    
    std::string gitignorePath = std::filesystem::path(m_projectPath).filePath(".gitignore");
    if (!std::filesystem::path::exists(gitignorePath)) {
        return;
    }
    
    std::fstream file(gitignorePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return;
    }
    
    QTextStream stream(&file);
    while (!stream.atEnd()) {
        std::string line = stream.readLine().trimmed();
        
        // Skip comments and empty lines
        if (line.isEmpty() || line.startsWith('#')) {
            continue;
        }
        
        m_gitignorePatterns.insert(line);
    }
    
}

// ========== GitignoreFilter Implementation ==========

GitignoreFilter::GitignoreFilter() = default;

void GitignoreFilter::loadFromFile(const std::string& gitignorePath) {
    clear();
    
    std::fstream file(gitignorePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return;
    }
    
    QTextStream stream(&file);
    while (!stream.atEnd()) {
        std::string line = stream.readLine().trimmed();
        if (!line.isEmpty() && !line.startsWith('#')) {
            m_patterns.append(line);
        }
    }
}

void GitignoreFilter::addPattern(const std::string& pattern) {
    if (!pattern.trimmed().isEmpty()) {
        m_patterns.append(pattern.trimmed());
    }
}

bool GitignoreFilter::shouldIgnore(const std::string& filePath, const std::string& basePath) const {
    // Use std::filesystem::path for relative path calculation since FileManager is not available in GitignoreFilter
    std::string checkPath = basePath.isEmpty() ? filePath : std::filesystem::path(basePath).relativeFilePath(filePath);
    
    for (const std::string& pattern : m_patterns) {
        if (matchPattern(checkPath, pattern)) {
            return true;
        }
    }
    
    return false;
}

void GitignoreFilter::clear() {
    m_patterns.clear();
}

bool GitignoreFilter::matchPattern(const std::string& filePath, const std::string& pattern) {
    // Simple wildcard matching (full .gitignore spec is more complex)
    std::regex regex(std::regex::wildcardToRegularExpression(pattern));
    return regex.match(filePath).hasMatch();
}

} // namespace RawrXD

