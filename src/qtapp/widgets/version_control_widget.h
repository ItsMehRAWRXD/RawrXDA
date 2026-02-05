#pragma once

#include <QWidget>
#include <QProcess>
#include <QString>
#include <QStringList>
#include <QDateTime>
#include <QMap>
#include <QColor>
#include <QJsonObject>

QT_BEGIN_NAMESPACE
class QTreeWidget;
class QTreeWidgetItem;
class QTextEdit;
class QLineEdit;
class QPushButton;
class QComboBox;
class QListWidget;
class QTabWidget;
class QToolBar;
class QLabel;
class QSplitter;
class QCheckBox;
QT_END_NAMESPACE

/**
 * @brief Production-ready Version Control Widget with comprehensive Git integration
 * 
 * Features:
 * - Full Git operations (commit, push, pull, fetch, merge, rebase)
 * - Visual commit history with graph
 * - Branch management (create, delete, switch, merge)
 * - Staging area with file status
 * - Diff viewer (inline and side-by-side)
 * - Merge conflict resolution
 * - Stash management
 * - Tag management
 * - Remote repository management
 * - Blame/annotation view
 * - Git log filtering and search
 * - Submodule support
 * - Cherry-pick support
 * - Reflog viewer
 * - Repository statistics
 */
class VersionControlWidget : public QWidget
{
    Q_OBJECT

public:
    explicit VersionControlWidget(QWidget* parent = nullptr);
    ~VersionControlWidget() override;

    // Repository management
    void setRepositoryPath(const QString& path);
    QString repositoryPath() const { return m_repoPath; }
    bool isValidRepository() const;
    
    // Git operations
    void refreshStatus();
    void commit(const QString& message);
    void push(const QString& remote = "origin", const QString& branch = "");
    void pull(const QString& remote = "origin", const QString& branch = "");
    void fetch(const QString& remote = "origin");
    void checkout(const QString& branch);
    void createBranch(const QString& name, const QString& baseBranch = "");
    void deleteBranch(const QString& name, bool force = false);
    void mergeBranch(const QString& branch);
    void rebaseBranch(const QString& branch);
    void cherryPick(const QString& commitHash);
    void revert(const QString& commitHash);
    void reset(const QString& commitHash, const QString& mode = "mixed");
    
    // Staging operations
    void stageFile(const QString& file);
    void unstageFile(const QString& file);
    void stageAll();
    void unstageAll();
    void discardChanges(const QString& file);
    
    // Stash operations
    void stash(const QString& message = "");
    void stashPop();
    void stashApply(int index);
    void stashDrop(int index);
    
    // History and log
    void showLog(int maxCount = 100);
    void showFileHistory(const QString& file);
    void showBlame(const QString& file);
    
    // Remote operations
    void addRemote(const QString& name, const QString& url);
    void removeRemote(const QString& name);
    void listRemotes();
    
    // Tag operations
    void createTag(const QString& name, const QString& commitHash = "", const QString& message = "");
    void deleteTag(const QString& name);
    void pushTags();
    
    struct RepositoryStats {
        int totalCommits = 0;
        int totalBranches = 0;
        int totalRemotes = 0;
        int totalTags = 0;
        int modifiedFiles = 0;
        int untrackedFiles = 0;
        int stagedFiles = 0;
        QString currentBranch;
        bool hasUncommittedChanges = false;
    };
    RepositoryStats statistics() const { return m_stats; }

signals:
    void statusChanged();
    void commitCreated(const QString& hash);
    void branchChanged(const QString& newBranch);
    void operationCompleted(const QString& operation, bool success);
    void operationFailed(const QString& operation, const QString& error);
    void mergeConflict(const QStringList& files);
    void fileStaged(const QString& file);
    void fileUnstaged(const QString& file);

public slots:
    void onCommitButtonClicked();
    void onPushButtonClicked();
    void onPullButtonClicked();
    void onFetchButtonClicked();
    void onRefreshButtonClicked();
    void onStageAllButtonClicked();
    void onUnstageAllButtonClicked();
    void onCreateBranchButtonClicked();
    void onSwitchBranchClicked();
    void onMergeBranchButtonClicked();
    void onStashButtonClicked();

private slots:
    void onUnstagedFileDoubleClicked(QTreeWidgetItem* item, int column);
    void onStagedFileDoubleClicked(QTreeWidgetItem* item, int column);
    void onCommitHistoryDoubleClicked(QTreeWidgetItem* item, int column);
    void onBranchSelectionChanged();
    void onContextMenuRequested(const QPoint& pos);
    void onAmendCheckBoxToggled(bool checked);

private:
    void setupUI();
    void createToolBar();
    void createStatusView();
    void createHistoryView();
    void createBranchView();
    void createDiffView();
    void connectSignals();
    
    // Git command execution
    QString runGitCommand(const QStringList& args, bool* success = nullptr);
    QStringList runGitCommandList(const QStringList& args, bool* success = nullptr);
    bool runGitCommandAsync(const QStringList& args);
    
    // Status parsing
    void parseGitStatus(const QString& output);
    void updateFileStatus();
    void updateBranchList();
    void updateCommitHistory();
    void updateStashList();
    
    // Diff operations
    QString getFileDiff(const QString& file, bool staged = false);
    QString getCommitDiff(const QString& commitHash);
    void showDiffForFile(const QString& file);
    void showDiffForCommit(const QString& commitHash);
    
    // UI helpers
    void addFileToTree(QTreeWidget* tree, const QString& file, const QString& status);
    QString getStatusColor(const QString& status);
    QIcon getStatusIcon(const QString& status);
    void updateRepositoryInfo();
    void showCommitDialog();
    void logVcsEvent(const QString& event, const QJsonObject& data = QJsonObject());
    
    // Merge conflict handling
    void detectMergeConflicts();
    bool hasConflicts();
    void showConflictResolutionDialog(const QStringList& files);

private:
    // UI Components
    QToolBar* m_toolBar{nullptr};
    QTabWidget* m_tabWidget{nullptr};
    QSplitter* m_mainSplitter{nullptr};
    
    // Status view
    QTreeWidget* m_unstagedFiles{nullptr};
    QTreeWidget* m_stagedFiles{nullptr};
    QPushButton* m_stageAllBtn{nullptr};
    QPushButton* m_unstageAllBtn{nullptr};
    QPushButton* m_commitBtn{nullptr};
    QLineEdit* m_commitMessageEdit{nullptr};
    QCheckBox* m_amendCheckBox{nullptr};
    
    // History view
    QTreeWidget* m_commitHistory{nullptr};
    QTextEdit* m_commitDetails{nullptr};
    
    // Branch view
    QListWidget* m_branchList{nullptr};
    QListWidget* m_remoteList{nullptr};
    QPushButton* m_createBranchBtn{nullptr};
    QPushButton* m_deleteBranchBtn{nullptr};
    QPushButton* m_mergeBranchBtn{nullptr};
    
    // Diff view
    QTextEdit* m_diffView{nullptr};
    
    // Stash view
    QListWidget* m_stashList{nullptr};
    
    // Status bar
    QLabel* m_repoStatusLabel{nullptr};
    QLabel* m_branchLabel{nullptr};
    
    // Repository data
    QString m_repoPath;
    QString m_currentBranch;
    QStringList m_branches;
    QStringList m_remoteBranches;
    QStringList m_remotes;
    
    // File status
    struct FileStatus {
        QString file;
        QString status; // M, A, D, ??, etc.
        bool staged;
    };
    QList<FileStatus> m_fileStatuses;
    
    // Repository statistics
    RepositoryStats m_stats;
    
    // Settings
    bool m_amendMode{false};
    QString m_lastCommitMessage;
};
