#pragma once












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
class VersionControlWidget
{

public:
    explicit VersionControlWidget(void* parent = nullptr);
    ~VersionControlWidget() override;

    // Repository management
    void setRepositoryPath(const std::string& path);
    std::string repositoryPath() const { return m_repoPath; }
    bool isValidRepository() const;
    
    // Git operations
    void refreshStatus();
    void refresh();
    void commit(const std::string& message);
    void push(const std::string& remote = "origin", const std::string& branch = "");
    void pull(const std::string& remote = "origin", const std::string& branch = "");
    void fetch(const std::string& remote = "origin");
    void checkout(const std::string& branch);
    void createBranch(const std::string& name, const std::string& baseBranch = "");
    void deleteBranch(const std::string& name, bool force = false);
    void mergeBranch(const std::string& branch);
    void rebaseBranch(const std::string& branch);
    void cherryPick(const std::string& commitHash);
    void revert(const std::string& commitHash);
    void reset(const std::string& commitHash, const std::string& mode = "mixed");
    
    // Staging operations
    void stageFile(const std::string& file);
    void unstageFile(const std::string& file);
    void stageAll();
    void unstageAll();
    void discardChanges(const std::string& file);
    
    // Stash operations
    void stash(const std::string& message = "");
    void stashPop();
    void stashApply(int index);
    void stashDrop(int index);
    
    // History and log
    void showLog(int maxCount = 100);
    void showFileHistory(const std::string& file);
    void showBlame(const std::string& file);
    
    // Remote operations
    void addRemote(const std::string& name, const std::string& url);
    void removeRemote(const std::string& name);
    void listRemotes();
    
    // Tag operations
    void createTag(const std::string& name, const std::string& commitHash = "", const std::string& message = "");
    void deleteTag(const std::string& name);
    void pushTags();
    
    struct RepositoryStats {
        int totalCommits = 0;
        int totalBranches = 0;
        int totalRemotes = 0;
        int totalTags = 0;
        int modifiedFiles = 0;
        int untrackedFiles = 0;
        int stagedFiles = 0;
        std::string currentBranch;
        bool hasUncommittedChanges = false;
    };
    RepositoryStats statistics() const { return m_stats; }
\npublic:\n    void statusChanged();
    void commitCreated(const std::string& hash);
    void branchChanged(const std::string& newBranch);
    void operationCompleted(const std::string& operation, bool success);
    void operationFailed(const std::string& operation, const std::string& error);
    void mergeConflict(const std::stringList& files);
    void fileStaged(const std::string& file);
    void fileUnstaged(const std::string& file);
\npublic:\n    void onCommitButtonClicked();
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
\nprivate:\n    void onUnstagedFileDoubleClicked(QTreeWidgetItem* item, int column);
    void onStagedFileDoubleClicked(QTreeWidgetItem* item, int column);
    void onCommitHistoryDoubleClicked(QTreeWidgetItem* item, int column);
    void onBranchSelectionChanged();
    void onContextMenuRequested(const struct { int x; int y; }& pos);
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
    std::string runGitCommand(const std::stringList& args, bool* success = nullptr);
    std::stringList runGitCommandList(const std::stringList& args, bool* success = nullptr);
    bool runGitCommandAsync(const std::stringList& args);
    
    // Status parsing
    void parseGitStatus(const std::string& output);
    void updateFileStatus();
    void updateBranchList();
    void updateCommitHistory();
    void updateStashList();
    
    // Diff operations
    std::string getFileDiff(const std::string& file, bool staged = false);
    std::string getCommitDiff(const std::string& commitHash);
    void showDiffForFile(const std::string& file);
    void showDiffForCommit(const std::string& commitHash);
    
    // UI helpers
    void addFileToTree(QTreeWidget* tree, const std::string& file, const std::string& status);
    std::string getStatusColor(const std::string& status);
    void getStatusIcon(const std::string& status);
    void updateRepositoryInfo();
    void showCommitDialog();
    void logVcsEvent(const std::string& event, const void*& data = void*());
    
    // Merge conflict handling
    void detectMergeConflicts();
    bool hasConflicts();
    void showConflictResolutionDialog(const std::stringList& files);

private:
    // UI Components
    void* m_toolBar{nullptr};
    void* m_tabWidget{nullptr};
    void* m_mainSplitter{nullptr};
    
    // Status view
    QTreeWidget* m_unstagedFiles{nullptr};
    QTreeWidget* m_stagedFiles{nullptr};
    void* m_stageAllBtn{nullptr};
    void* m_unstageAllBtn{nullptr};
    void* m_commitBtn{nullptr};
    voidEdit* m_commitMessageEdit{nullptr};
    void* m_amendCheckBox{nullptr};
    
    // History view
    QTreeWidget* m_commitHistory{nullptr};
    void* m_commitDetails{nullptr};
    
    // Branch view
    QListWidget* m_branchList{nullptr};
    QListWidget* m_remoteList{nullptr};
    void* m_createBranchBtn{nullptr};
    void* m_deleteBranchBtn{nullptr};
    void* m_mergeBranchBtn{nullptr};
    
    // Diff view
    void* m_diffView{nullptr};
    
    // Stash view
    QListWidget* m_stashList{nullptr};
    
    // Status bar
    void* m_repoStatusLabel{nullptr};
    void* m_branchLabel{nullptr};
    
    // Repository data
    std::string m_repoPath;
    std::string m_currentBranch;
    std::stringList m_branches;
    std::stringList m_remoteBranches;
    std::stringList m_remotes;
    
    // File status
    struct FileStatus {
        std::string file;
        std::string status; // M, A, D, ??, etc.
        bool staged;
    };
    std::vector<FileStatus> m_fileStatuses;
    
    // Repository statistics
    RepositoryStats m_stats;
    
    // Settings
    bool m_amendMode{false};
    std::string m_lastCommitMessage;
};






