#pragma once
/**
 * \file project_explorer.h
 * \brief Production-grade project explorer with full file management
 * \author RawrXD Team
 * \date 2025-12-05
 * 
 * Features:
 * - Real filesystem integration with lazy loading
 * - File operations: create, delete, rename, move, copy
 * - Drag-and-drop file rearrangement
 * - Context menus with common operations
 * - .gitignore support (gray out ignored files)
 * - Project type detection and display
 * - Recent files tracking
 * - Search/filter capabilities
 */

#include <QWidget>
#include <QTreeView>
#include <QFileSystemModel>
#include <QMenu>
#include <QLineEdit>
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QToolBar>
#include <QSet>
#include "../utils/project_detector.h"
// Interfaces for file and directory operations (Dependency Inversion)
#include "../interfaces/ifile_writer.h"
#include "../interfaces/idirectory_manager.h"
// Default Qt implementations used when no custom instances are provided
#include "../utils/qt_file_writer.h"
#include "../utils/qt_directory_manager.h"
#include <memory>

namespace RawrXD {

// Forward declarations
class ProjectExplorerModel;
class GitignoreFilter;

/**
 * \class ProjectExplorerWidget
 * \brief Main project explorer widget replacing the stub
 * 
 * This is a production-grade file explorer with all features expected
 * from a modern IDE (VS Code, Cursor, etc.)
 * 
 * Usage:
 * \code
 * auto* explorer = new ProjectExplorerWidget(this);
 * explorer->openProject("/path/to/project");
 * connect(explorer, &ProjectExplorerWidget::fileDoubleClicked,
 *         this, &MainWindow::openFileInEditor);
 * \endcode
 */
class ProjectExplorerWidget : public QWidget {
    Q_OBJECT
    
public:
    /**
     * \brief Construct the widget with optional injected dependencies.
     * \param parent QWidget parent.
     * \param fileWriter Concrete IFileWriter implementation. If nullptr, a QtFileWriter is created.
     * \param dirManager Concrete IDirectoryManager implementation. If nullptr, a QtDirectoryManager is created.
     */
    explicit ProjectExplorerWidget(QWidget* parent = nullptr,
                                   IFileWriter* fileWriter = nullptr,
                                   IDirectoryManager* dirManager = nullptr);
    ~ProjectExplorerWidget() override;
    
    /**
     * \brief Open project folder and display its contents
     * \param projectPath Absolute path to project root directory
     * \return true if successful
     */
    bool openProject(const QString& projectPath);
    
    /**
     * \brief Get current project root path
     * \return Absolute path to project root, or empty if no project open
     */
    QString currentProjectPath() const;
    
    /**
     * \brief Get project metadata for current project
     * \return Current project metadata
     */
    ProjectMetadata currentProjectMetadata() const;
    
    /**
     * \brief Close current project
     */
    void closeProject();
    
    /**
     * \brief Refresh the file tree (reload from filesystem)
     */
    void refresh();
    
    /**
     * \brief Get currently selected file path
     * \return Absolute path to selected file, or empty if none selected
     */
    QString selectedFilePath() const;
    
    /**
     * \brief Get all selected file paths (multi-selection)
     * \return List of absolute paths to selected files
     */
    QStringList selectedFilePaths() const;
    
    /**
     * \brief Select and scroll to specific file
     * \param filePath Absolute path to file
     */
    void selectFile(const QString& filePath);
    
    /**
     * \brief Expand directory at path
     * \param dirPath Absolute path to directory
     */
    void expandDirectory(const QString& dirPath);
    
    /**
     * \brief Collapse directory at path
     * \param dirPath Absolute path to directory
     */
    void collapseDirectory(const QString& dirPath);
    
    /**
     * \brief Set whether to show hidden files (.dotfiles)
     * \param show If true, show hidden files
     */
    void setShowHiddenFiles(bool show);
    
    /**
     * \brief Get whether hidden files are shown
     * \return true if hidden files are visible
     */
    bool showHiddenFiles() const;
    
    /**
     * \brief Set filter pattern for visible files
     * \param pattern Wildcard pattern (e.g., "*.cpp *.h")
     */
    void setFileFilter(const QString& pattern);
    
signals:
    /**
     * \brief Emitted when file is double-clicked (should open in editor)
     * \param filePath Absolute path to file
     */
    void fileDoubleClicked(const QString& filePath);
    
    /**
     * \brief Emitted when file is single-clicked
     * \param filePath Absolute path to file
     */
    void fileClicked(const QString& filePath);
    
    /**
     * \brief Emitted when file is created
     * \param filePath Absolute path to new file
     */
    void fileCreated(const QString& filePath);
    
    /**
     * \brief Emitted when file is deleted
     * \param filePath Absolute path to deleted file
     */
    void fileDeleted(const QString& filePath);
    
    /**
     * \brief Emitted when file is renamed
     * \param oldPath Previous absolute path
     * \param newPath New absolute path
     */
    void fileRenamed(const QString& oldPath, const QString& newPath);
    
    /**
     * \brief Emitted when project is opened
     * \param projectPath Absolute path to project root
     */
    void projectOpened(const QString& projectPath);
    
    /**
     * \brief Emitted when project is closed
     */
    void projectClosed();
    
private slots:
    void onTreeDoubleClicked(const QModelIndex& index);
    void onTreeClicked(const QModelIndex& index);
    void onContextMenuRequested(const QPoint& pos);
    void onFilterTextChanged(const QString& text);
    
    // Context menu actions
    void actionNewFile();
    void actionNewFolder();
    void actionRename();
    void actionDelete();
    void actionCopy();
    void actionCut();
    void actionPaste();
    void actionRevealInExplorer();
    void actionCopyPath();
    void actionCopyRelativePath();
    void actionRefresh();
    
private:
    void setupUI();
    void setupContextMenu();
    void createToolbar();
    void loadProjectMetadata();
    void saveProjectMetadata();
    void updateProjectInfo();
    
    /**
     * \brief Check if file should be ignored based on .gitignore
     * \param filePath Absolute path to file
     * \return true if file should be ignored
     */
    bool isFileIgnored(const QString& filePath) const;
    
    /**
     * \brief Load .gitignore patterns from project
     */
    void loadGitignorePatterns();
    
    // UI Components
    QVBoxLayout* m_mainLayout;
    QToolBar* m_toolbar;
    QLineEdit* m_filterEdit;
    QTreeView* m_treeView;
    QLabel* m_projectInfoLabel;
    
    // Data model
    QFileSystemModel* m_fileSystemModel;
    
    // Project state
    QString m_projectPath;
    ProjectMetadata m_projectMetadata;
    ProjectDetector m_projectDetector;
    // Abstracted file and directory managers injected via constructor
    IFileWriter *m_fileWriter = nullptr;
    IDirectoryManager *m_dirManager = nullptr;
    // Ownership flags to delete default implementations in destructor
    bool m_ownsFileWriter = false;
    bool m_ownsDirManager = false;
    
    // Context menu
    QMenu* m_contextMenu;
    QAction* m_actionNewFile;
    QAction* m_actionNewFolder;
    QAction* m_actionRename;
    QAction* m_actionDelete;
    QAction* m_actionCopy;
    QAction* m_actionCut;
    QAction* m_actionPaste;
    QAction* m_actionRevealInExplorer;
    QAction* m_actionCopyPath;
    QAction* m_actionCopyRelativePath;
    QAction* m_actionRefresh;
    
    // Clipboard state
    QString m_clipboardPath;
    bool m_clipboardIsCut;  // true = cut, false = copy
    
    // .gitignore patterns
    QSet<QString> m_gitignorePatterns;
    
    // Settings
    bool m_showHiddenFiles;
};

/**
 * \class GitignoreFilter
 * \brief Helper class for .gitignore pattern matching
 */
class GitignoreFilter {
public:
    GitignoreFilter();
    
    /**
     * \brief Load patterns from .gitignore file
     * \param gitignorePath Absolute path to .gitignore
     */
    void loadFromFile(const QString& gitignorePath);
    
    /**
     * \brief Add pattern manually
     * \param pattern .gitignore style pattern
     */
    void addPattern(const QString& pattern);
    
    /**
     * \brief Check if file matches any ignore pattern
     * \param filePath Absolute or relative path to file
     * \param basePath Base directory for relative paths
     * \return true if file should be ignored
     */
    bool shouldIgnore(const QString& filePath, const QString& basePath = QString()) const;
    
    /**
     * \brief Clear all patterns
     */
    void clear();
    
private:
    QStringList m_patterns;
    
    /**
     * \brief Match file against single .gitignore pattern
     * \param filePath File path to check
     * \param pattern .gitignore pattern
     * \return true if matches
     */
    static bool matchPattern(const QString& filePath, const QString& pattern);
};

} // namespace RawrXD
