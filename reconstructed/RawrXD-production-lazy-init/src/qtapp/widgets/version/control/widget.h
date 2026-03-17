/**
 * @file version_control_widget.h
 * @brief Production implementation of VersionControlWidget
 * 
 * Provides a fully functional Git interface including:
 * - Repository status view
 * - Staging/unstaging changes
 * - Commit creation with message
 * - Branch management
 * - Push/pull/fetch operations
 * - Diff viewing
 * - Git log/history viewing
 * 
 * Per AI Toolkit Production Readiness Instructions:
 * - NO SIMPLIFICATIONS - all logic must remain intact
 * - Full structured logging for observability
 */

#pragma once

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTreeWidget>
#include <QTextEdit>
#include <QPushButton>
#include <QComboBox>
#include <QLabel>
#include <QSplitter>
#include <QProcess>
#include <QLineEdit>
#include <QToolBar>
#include <QMenu>
#include <QAction>
#include <QTimer>
#include <QFileSystemWatcher>

class VersionControlWidget : public QWidget {
    Q_OBJECT

public:
    enum class FileStatus {
        Untracked,
        Modified,
        Added,
        Deleted,
        Renamed,
        Copied,
        Unmerged,
        Ignored
    };
    Q_ENUM(FileStatus)

    struct FileChange {
        QString path;
        QString oldPath;  // For renames
        FileStatus status;
        bool staged;
    };

    struct GitCommit {
        QString hash;
        QString shortHash;
        QString author;
        QString email;
        QString date;
        QString message;
        QStringList parents;
    };

    struct GitBranch {
        QString name;
        bool isRemote;
        bool isCurrent;
        QString upstream;
        int ahead;
        int behind;
    };

    explicit VersionControlWidget(QWidget* parent = nullptr);
    ~VersionControlWidget() override;

    // Repository management
    void setRepositoryPath(const QString& path);
    QString getRepositoryPath() const;
    bool isGitRepository() const;
    void initRepository();
    void cloneRepository(const QString& url, const QString& path);

    // Status
    void refresh();
    QList<FileChange> getChanges() const;
    QList<FileChange> getStagedChanges() const;
    QList<FileChange> getUnstagedChanges() const;

    // Staging
    void stageFile(const QString& path);
    void unstageFile(const QString& path);
    void stageAll();
    void unstageAll();
    void discardChanges(const QString& path);

    // Commits
    void commit(const QString& message);
    void amendCommit(const QString& message);
    QList<GitCommit> getLog(int count = 50) const;

    // Branches
    QList<GitBranch> getBranches() const;
    QString getCurrentBranch() const;
    void createBranch(const QString& name);
    void checkoutBranch(const QString& name);
    void deleteBranch(const QString& name);
    void mergeBranch(const QString& name);

    // Remote operations
    void push();
    void pull();
    void fetch();

    // Diff
    QString getDiff(const QString& path, bool staged = false) const;

signals:
    void repositoryChanged(const QString& path);
    void statusChanged();
    void branchChanged(const QString& branch);
    void commitCompleted(const QString& hash);
    void pushCompleted(bool success);
    void pullCompleted(bool success);
    void operationError(const QString& operation, const QString& error);
    void fileSelected(const QString& path);
    void fileDiffRequested(const QString& path, bool staged);

public slots:
    void onRefresh();
    void onStageSelected();
    void onUnstageSelected();
    void onStageAll();
    void onUnstageAll();
    void onCommit();
    void onAmend();
    void onPush();
    void onPull();
    void onFetch();
    void onBranchChanged(int index);
    void onCreateBranch();
    void onFileDoubleClicked(QTreeWidgetItem* item, int column);
    void onContextMenu(const QPoint& pos);

private:
    void setupUI();
    void setupToolbar();
    void setupConnections();
    void updateStatus();
    void updateBranches();
    void populateChangesTree();
    QString runGitCommand(const QStringList& args, bool* ok = nullptr) const;
    FileStatus parseStatusCode(const QString& code) const;
    QString statusToString(FileStatus status) const;
    QIcon statusIcon(FileStatus status) const;
    void showError(const QString& operation, const QString& error);

    // UI Components
    QVBoxLayout* m_mainLayout;
    QToolBar* m_toolbar;
    QComboBox* m_branchCombo;
    QPushButton* m_refreshButton;
    QPushButton* m_pushButton;
    QPushButton* m_pullButton;
    QPushButton* m_fetchButton;
    QSplitter* m_splitter;
    
    // Staged changes
    QWidget* m_stagedWidget;
    QLabel* m_stagedLabel;
    QTreeWidget* m_stagedTree;
    QPushButton* m_unstageAllButton;

    // Unstaged changes  
    QWidget* m_unstagedWidget;
    QLabel* m_unstagedLabel;
    QTreeWidget* m_unstagedTree;
    QPushButton* m_stageAllButton;
    QPushButton* m_discardAllButton;

    // Commit area
    QWidget* m_commitWidget;
    QTextEdit* m_commitMessage;
    QPushButton* m_commitButton;
    QPushButton* m_amendButton;

    // State
    QString m_repoPath;
    bool m_isRepo;
    QString m_currentBranch;
    QList<FileChange> m_changes;
    QList<GitBranch> m_branches;

    // Auto-refresh
    QFileSystemWatcher* m_watcher;
    QTimer* m_refreshTimer;
    bool m_autoRefresh;
};
