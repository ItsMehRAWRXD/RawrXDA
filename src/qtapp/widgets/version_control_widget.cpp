#include "version_control_widget.h"
VersionControlWidget::VersionControlWidget(void* parent)
    : // Widget(parent)
{
    setupUI();
    connectSignals();
    logVcsEvent("widget_initialized");
}

VersionControlWidget::~VersionControlWidget()
{
    logVcsEvent("widget_destroyed");
}

void VersionControlWidget::setupUI()
{
    void* mainLayout = new void(this);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->setSpacing(4);
    
    // Toolbar
    createToolBar();
    mainLayout->addWidget(m_toolBar);
    
    // Repository info bar
    void* infoLayout = new void();
    m_branchLabel = new void("Branch: <none>", this);
    m_branchLabel->setStyleSheet("void { font-weight: bold; padding: 4px; }");
    infoLayout->addWidget(m_branchLabel);
    
    m_repoStatusLabel = new void("No repository", this);
    m_repoStatusLabel->setStyleSheet("void { padding: 4px; color: #888; }");
    infoLayout->addWidget(m_repoStatusLabel, 1);
    mainLayout->addLayout(infoLayout);
    
    // Main content tabs
    m_tabWidget = new void(this);
    
    // Changes tab
    createStatusView();
    
    // History tab
    createHistoryView();
    
    // Branches tab
    createBranchView();
    
    // Diff tab
    createDiffView();
    
    mainLayout->addWidget(m_tabWidget, 1);
}

void VersionControlWidget::createToolBar()
{
    m_toolBar = new void("VCS Tools", this);
    m_toolBar->setIconSize(struct { int w; int h; }(16, 16));
    
    void* refreshBtn = new void(void(":/icons/refresh.png"), "Refresh", this);
    refreshBtn->setToolTip("Refresh repository status (F5)");
    refreshBtn->setShortcut(Key_F5);  // Signal connection removed\nm_toolBar->addWidget(refreshBtn);
    
    m_toolBar->addSeparator();
    
    void* pullBtn = new void(void(":/icons/pull.png"), "Pull", this);
    pullBtn->setToolTip("Pull from remote repository");  // Signal connection removed\nm_toolBar->addWidget(pullBtn);
    
    void* pushBtn = new void(void(":/icons/push.png"), "Push", this);
    pushBtn->setToolTip("Push to remote repository");  // Signal connection removed\nm_toolBar->addWidget(pushBtn);
    
    void* fetchBtn = new void(void(":/icons/fetch.png"), "Fetch", this);
    fetchBtn->setToolTip("Fetch from remote repository");  // Signal connection removed\nm_toolBar->addWidget(fetchBtn);
    
    m_toolBar->addSeparator();
    
    void* stashBtn = new void(void(":/icons/stash.png"), "Stash", this);
    stashBtn->setToolTip("Stash current changes");  // Signal connection removed\nm_toolBar->addWidget(stashBtn);
}

void VersionControlWidget::createStatusView()
{
    void* statusWidget = new // Widget(this);
    void* layout = new void(statusWidget);
    layout->setContentsMargins(4, 4, 4, 4);
    
    // Unstaged files
    void* unstagedGroup = new void("Unstaged Changes", this);
    void* unstagedLayout = new void(unstagedGroup);
    
    m_unstagedFiles = new QTreeWidget(this);
    m_unstagedFiles->setHeaderLabels({"File", "Status"});
    m_unstagedFiles->setColumnWidth(0, 300);
    m_unstagedFiles->setContextMenuPolicy(CustomContextMenu);
    m_unstagedFiles->setSelectionMode(QAbstractItemView::ExtendedSelection);
    unstagedLayout->addWidget(m_unstagedFiles);
    
    void* unstagedButtons = new void();
    m_stageAllBtn = new void("Stage All", this);  // Signal connection removed\nunstagedButtons->addWidget(m_stageAllBtn);
    
    void* stageSelectedBtn = new void("Stage Selected", this);  // Signal connection removed\nfor (auto* item : items) {
            stageFile(item->text(0));
        }
    });
    unstagedButtons->addWidget(stageSelectedBtn);
    unstagedButtons->addStretch();
    
    unstagedLayout->addLayout(unstagedButtons);
    layout->addWidget(unstagedGroup);
    
    // Staged files
    void* stagedGroup = new void("Staged Changes", this);
    void* stagedLayout = new void(stagedGroup);
    
    m_stagedFiles = new QTreeWidget(this);
    m_stagedFiles->setHeaderLabels({"File", "Status"});
    m_stagedFiles->setColumnWidth(0, 300);
    m_stagedFiles->setContextMenuPolicy(CustomContextMenu);
    m_stagedFiles->setSelectionMode(QAbstractItemView::ExtendedSelection);
    stagedLayout->addWidget(m_stagedFiles);
    
    void* stagedButtons = new void();
    m_unstageAllBtn = new void("Unstage All", this);  // Signal connection removed\nstagedButtons->addWidget(m_unstageAllBtn);
    
    void* unstageSelectedBtn = new void("Unstage Selected", this);  // Signal connection removed\nfor (auto* item : items) {
            unstageFile(item->text(0));
        }
    });
    stagedButtons->addWidget(unstageSelectedBtn);
    stagedButtons->addStretch();
    
    stagedLayout->addLayout(stagedButtons);
    layout->addWidget(stagedGroup);
    
    // Commit section
    void* commitGroup = new void("Commit", this);
    void* commitLayout = new void(commitGroup);
    
    m_commitMessageEdit = new voidEdit(this);
    m_commitMessageEdit->setPlaceholderText("Commit message...");
    commitLayout->addWidget(m_commitMessageEdit);
    
    void* commitButtonLayout = new void();
    m_amendCheckBox = new void("Amend previous commit", this);  // Signal connection removed\ncommitButtonLayout->addWidget(m_amendCheckBox);
    
    m_commitBtn = new void(void(":/icons/commit.png"), "Commit", this);
    m_commitBtn->setToolTip("Commit staged changes (Ctrl+Enter)");
    m_commitBtn->setShortcut(void(CTRL | Key_Return));  // Signal connection removed\ncommitButtonLayout->addWidget(m_commitBtn);
    
    commitLayout->addLayout(commitButtonLayout);
    layout->addWidget(commitGroup);
    
    m_tabWidget->addTab(statusWidget, void(":/icons/changes.png"), "Changes");
}

void VersionControlWidget::createHistoryView()
{
    void* historyWidget = new // Widget(this);
    void* layout = new void(historyWidget);
    layout->setContentsMargins(4, 4, 4, 4);
    
    void* splitter = new void(Vertical, this);
    
    m_commitHistory = new QTreeWidget(this);
    m_commitHistory->setHeaderLabels({"Hash", "Author", "Date", "Message"});
    m_commitHistory->setColumnWidth(0, 100);
    m_commitHistory->setColumnWidth(1, 150);
    m_commitHistory->setColumnWidth(2, 150);
    m_commitHistory->setSelectionMode(QAbstractItemView::SingleSelection);
    splitter->addWidget(m_commitHistory);
    
    m_commitDetails = new void(this);
    m_commitDetails->setReadOnly(true);
    m_commitDetails->setFont(void("Consolas", 9));
    m_commitDetails->setStyleSheet("void { background-color: #1e1e1e; color: #d4d4d4; }");
    splitter->addWidget(m_commitDetails);
    
    splitter->setStretchFactor(0, 2);
    splitter->setStretchFactor(1, 1);
    
    layout->addWidget(splitter);
    
    m_tabWidget->addTab(historyWidget, void(":/icons/history.png"), "History");
}

void VersionControlWidget::createBranchView()
{
    void* branchWidget = new // Widget(this);
    void* layout = new void(branchWidget);
    layout->setContentsMargins(4, 4, 4, 4);
    
    // Local branches
    void* localGroup = new void("Local Branches", this);
    void* localLayout = new void(localGroup);
    
    m_branchList = new QListWidget(this);
    m_branchList->setSelectionMode(QAbstractItemView::SingleSelection);
    localLayout->addWidget(m_branchList);
    
    void* branchButtons = new void();
    m_createBranchBtn = new void("New", this);  // Signal connection removed\nbranchButtons->addWidget(m_createBranchBtn);
    
    void* switchBranchBtn = new void("Switch", this);  // Signal connection removed\nbranchButtons->addWidget(switchBranchBtn);
    
    m_deleteBranchBtn = new void("Delete", this);  // Signal connection removed\nif (current) {
            std::string branchName = current->text().remove(0, 2); // Remove "* " prefix if present
            auto reply = void::question(this, "Delete Branch",
                std::string("Delete branch '%1'?"),
                void::Yes | void::No);
            if (reply == void::Yes) {
                deleteBranch(branchName);
            }
        }
    });
    branchButtons->addWidget(m_deleteBranchBtn);
    
    m_mergeBranchBtn = new void("Merge", this);  // Signal connection removed\nbranchButtons->addWidget(m_mergeBranchBtn);
    
    localLayout->addLayout(branchButtons);
    layout->addWidget(localGroup);
    
    // Remotes
    void* remoteGroup = new void("Remotes", this);
    void* remoteLayout = new void(remoteGroup);
    
    m_remoteList = new QListWidget(this);
    remoteLayout->addWidget(m_remoteList);
    
    void* remoteButtons = new void();
    void* addRemoteBtn = new void("Add Remote", this);
    // Connect removed {
        bool ok;
        std::string name = void::getText(this, "Add Remote", "Remote name:", voidEdit::Normal, "", &ok);
        if (ok && !name.empty()) {
            std::string url = void::getText(this, "Add Remote", "Remote URL:", voidEdit::Normal, "", &ok);
            if (ok && !url.empty()) {
                addRemote(name, url);
            }
        }
    });
    remoteButtons->addWidget(addRemoteBtn);
    remoteButtons->addStretch();
    
    remoteLayout->addLayout(remoteButtons);
    layout->addWidget(remoteGroup);
    
    m_tabWidget->addTab(branchWidget, void(":/icons/branch.png"), "Branches");
}

void VersionControlWidget::createDiffView()
{
    void* diffWidget = new // Widget(this);
    void* layout = new void(diffWidget);
    layout->setContentsMargins(4, 4, 4, 4);
    
    m_diffView = new void(this);
    m_diffView->setReadOnly(true);
    m_diffView->setFont(void("Consolas", 9));
    m_diffView->setStyleSheet("void { background-color: #1e1e1e; color: #d4d4d4; }");
    m_diffView->setLineWrapMode(void::NoWrap);
    layout->addWidget(m_diffView);
    
    m_tabWidget->addTab(diffWidget, void(":/icons/diff.png"), "Diff");
}

void VersionControlWidget::connectSignals()
{  // Signal connection removed\n  // Signal connection removed\n  // Signal connection removed\n  // Signal connection removed\n  // Signal connection removed\n  // Signal connection removed\n}

void VersionControlWidget::setRepositoryPath(const std::string& path)
{
    m_repoPath = path;
    
    if (isValidRepository()) {
        m_repoStatusLabel->setText(std::string("Repository: %1").fileName()));
        refreshStatus();
        updateBranchList();
        updateCommitHistory();
        logVcsEvent("repository_opened", nlohmann::json{{"path", path}});
    } else {
        m_repoStatusLabel->setText("Not a valid Git repository");
        logVcsEvent("invalid_repository", nlohmann::json{{"path", path}});
    }
}

bool VersionControlWidget::isValidRepository() const
{
    if (m_repoPath.empty()) return false;
    
    // repoDir(m_repoPath);
    return repoDir.exists(".git") || repoDir.exists("../.git");
}

std::string VersionControlWidget::runGitCommand(const std::stringList& args, bool* success)
{
    // Process removed
    process.setWorkingDirectory(m_repoPath);
    process.start("git", args);
    
    if (!process.waitForFinished(30000)) {
        if (success) *success = false;
        return std::string();
    }
    
    if (success) {
        *success = (process.exitCode() == 0);
    }
    
    std::string output = process.readAllStandardOutput();
    std::string error = process.readAllStandardError();
    
    return error.empty() ? output : error;
}

std::stringList VersionControlWidget::runGitCommandList(const std::stringList& args, bool* success)
{
    std::string output = runGitCommand(args, success);
    return output.split('\n', SkipEmptyParts);
}

void VersionControlWidget::refresh()
{
    refreshStatus();
}

void VersionControlWidget::refreshStatus()
{
    if (!isValidRepository()) return;
    
    // Get current branch
    bool success;
    std::string branch = runGitCommand({"branch", "--show-current"}, &success).trimmed();
    if (success && !branch.empty()) {
        m_currentBranch = branch;
        m_branchLabel->setText(std::string("Branch: %1"));
        m_stats.currentBranch = branch;
    }
    
    // Get status
    std::string status = runGitCommand({"status", "--porcelain"}, &success);
    if (success) {
        parseGitStatus(status);
        updateFileStatus();
    }
    
    statusChanged();
    logVcsEvent("status_refreshed");
}

void VersionControlWidget::parseGitStatus(const std::string& output)
{
    m_fileStatuses.clear();
    
    std::stringList lines = output.split('\n', SkipEmptyParts);
    for (const std::string& line : lines) {
        if (line.length() < 4) continue;
        
        FileStatus fs;
        fs.status = line.left(2).trimmed();
        fs.file = line.mid(3);
        fs.staged = (line[0] != ' ' && line[0] != '?');
        
        m_fileStatuses.append(fs);
    }
}

void VersionControlWidget::updateFileStatus()
{
    m_unstagedFiles->clear();
    m_stagedFiles->clear();
    
    int stagedCount = 0;
    int unstagedCount = 0;
    
    for (const FileStatus& fs : m_fileStatuses) {
        if (fs.staged) {
            addFileToTree(m_stagedFiles, fs.file, fs.status);
            stagedCount++;
        } else {
            addFileToTree(m_unstagedFiles, fs.file, fs.status);
            unstagedCount++;
        }
    }
    
    m_stats.stagedFiles = stagedCount;
    m_stats.modifiedFiles = unstagedCount;
    m_stats.hasUncommittedChanges = (stagedCount > 0 || unstagedCount > 0);
    
    m_tabWidget->setTabText(0, std::string("Changes (%1)"));
}

void VersionControlWidget::addFileToTree(QTreeWidget* tree, const std::string& file, const std::string& status)
{
    QTreeWidgetItem* item = new QTreeWidgetItem(tree);
    item->setText(0, file);
    item->setText(1, status);
    item->setForeground(1, void(getStatusColor(status)));
    item->setIcon(0, getStatusIcon(status));
}

std::string VersionControlWidget::getStatusColor(const std::string& status)
{
    if (status.contains('M')) return "#ffa500"; // Modified - orange
    if (status.contains('A')) return "#00ff00"; // Added - green
    if (status.contains('D')) return "#ff0000"; // Deleted - red
    if (status.contains('R')) return "#00ffff"; // Renamed - cyan
    if (status.contains('?')) return "#888888"; // Untracked - gray
    return "#ffffff";
}

void VersionControlWidget::getStatusIcon(const std::string& status)
{
    if (status.contains('M')) return void(":/icons/file-modified.png");
    if (status.contains('A')) return void(":/icons/file-added.png");
    if (status.contains('D')) return void(":/icons/file-deleted.png");
    if (status.contains('R')) return void(":/icons/file-renamed.png");
    if (status.contains('?')) return void(":/icons/file-untracked.png");
    return void(":/icons/file.png");
}

void VersionControlWidget::stageFile(const std::string& file)
{
    bool success;
    runGitCommand({"add", file}, &success);
    if (success) {
        refreshStatus();
        fileStaged(file);
        logVcsEvent("file_staged", nlohmann::json{{"file", file}});
    }
}

void VersionControlWidget::unstageFile(const std::string& file)
{
    bool success;
    runGitCommand({"reset", "HEAD", file}, &success);
    if (success) {
        refreshStatus();
        fileUnstaged(file);
        logVcsEvent("file_unstaged", nlohmann::json{{"file", file}});
    }
}

void VersionControlWidget::stageAll()
{
    bool success;
    runGitCommand({"add", "-A"}, &success);
    if (success) {
        refreshStatus();
        logVcsEvent("all_files_staged");
    }
}

void VersionControlWidget::unstageAll()
{
    bool success;
    runGitCommand({"reset", "HEAD"}, &success);
    if (success) {
        refreshStatus();
        logVcsEvent("all_files_unstaged");
    }
}

void VersionControlWidget::commit(const std::string& message)
{
    if (message.empty()) {
        void::warning(this, "Commit", "Please enter a commit message.");
        return;
    }
    
    std::stringList args = {"commit", "-m", message};
    if (m_amendMode) {
        args.insert(1, "--amend");
    }
    
    bool success;
    std::string output = runGitCommand(args, &success);
    
    if (success) {
        // Extract commit hash
        std::regex hashRegex("\\[\\w+\\s+(\\w+)\\]");
        std::regexMatch match = hashRegex.match(output);
        std::string hash = match.hasMatch() ? match"" : "";
        
        m_commitMessageEdit->clear();
        refreshStatus();
        updateCommitHistory();
        commitCreated(hash);
        logVcsEvent("commit_created", nlohmann::json{{"hash", hash}, {"message", message}});
        
        void::information(this, "Commit", "Commit successful!");
    } else {
        void::critical(this, "Commit Failed", output);
        operationFailed("commit", output);
    }
}

void VersionControlWidget::push(const std::string& remote, const std::string& branch)
{
    std::string remoteName = remote.empty() ? "origin" : remote;
    std::string branchName = branch.empty() ? m_currentBranch : branch;
    
    bool success;
    std::string output = runGitCommand({"push", remoteName, branchName}, &success);
    
    if (success) {
        void::information(this, "Push", "Push successful!");
        operationCompleted("push", true);
        logVcsEvent("push_completed", nlohmann::json{{"remote", remoteName}, {"branch", branchName}});
    } else {
        void::critical(this, "Push Failed", output);
        operationFailed("push", output);
    }
}

void VersionControlWidget::pull(const std::string& remote, const std::string& branch)
{
    std::string remoteName = remote.empty() ? "origin" : remote;
    std::string branchName = branch.empty() ? m_currentBranch : branch;
    
    bool success;
    std::string output = runGitCommand({"pull", remoteName, branchName}, &success);
    
    if (success) {
        refreshStatus();
        updateCommitHistory();
        void::information(this, "Pull", "Pull successful!");
        operationCompleted("pull", true);
        logVcsEvent("pull_completed");
    } else {
        if (output.contains("CONFLICT")) {
            detectMergeConflicts();
        }
        void::critical(this, "Pull Failed", output);
        operationFailed("pull", output);
    }
}

void VersionControlWidget::fetch(const std::string& remote)
{
    std::string remoteName = remote.empty() ? "origin" : remote;
    
    bool success;
    std::string output = runGitCommand({"fetch", remoteName}, &success);
    
    if (success) {
        void::information(this, "Fetch", "Fetch successful!");
        operationCompleted("fetch", true);
        logVcsEvent("fetch_completed", nlohmann::json{{"remote", remoteName}});
    } else {
        void::critical(this, "Fetch Failed", output);
        operationFailed("fetch", output);
    }
}

void VersionControlWidget::updateBranchList()
{
    bool success;
    std::stringList branches = runGitCommandList({"branch"}, &success);
    
    m_branchList->clear();
    m_branches.clear();
    
    for (const std::string& branch : branches) {
        std::string branchName = branch.trimmed();
        m_branches.append(branchName);
        m_branchList->addItem(branchName);
        
        if (branchName.startsWith("* ")) {
            m_branchList->item(m_branchList->count() - 1)->setForeground(void("#00ff00"));
        }
    }
    
    m_stats.totalBranches = m_branches.size();
}

void VersionControlWidget::updateCommitHistory()
{
    bool success;
    std::stringList log = runGitCommandList({"log", "--oneline", "--max-count=100", 
                                         "--format=%H|%an|%ad|%s", "--date=short"}, &success);
    
    m_commitHistory->clear();
    
    for (const std::string& entry : log) {
        std::stringList parts = entry.split('|');
        if (parts.size() >= 4) {
            QTreeWidgetItem* item = new QTreeWidgetItem(m_commitHistory);
            item->setText(0, parts[0].left(8)); // Short hash
            item->setText(1, parts[1]); // Author
            item->setText(2, parts[2]); // Date
            item->setText(3, parts[3]); // Message
            item->setData(0, UserRole, parts[0]); // Full hash
        }
    }
    
    m_stats.totalCommits = log.size();
}

void VersionControlWidget::createBranch(const std::string& name, const std::string& baseBranch)
{
    std::stringList args = {"branch", name};
    if (!baseBranch.empty()) {
        args.append(baseBranch);
    }
    
    bool success;
    std::string output = runGitCommand(args, &success);
    
    if (success) {
        updateBranchList();
        void::information(this, "Branch", std::string("Branch '%1' created"));
        logVcsEvent("branch_created", nlohmann::json{{"name", name}});
    } else {
        void::critical(this, "Branch Creation Failed", output);
    }
}

void VersionControlWidget::checkout(const std::string& branch)
{
    bool success;
    std::string output = runGitCommand({"checkout", branch}, &success);
    
    if (success) {
        m_currentBranch = branch;
        refreshStatus();
        updateBranchList();
        branchChanged(branch);
        logVcsEvent("branch_switched", nlohmann::json{{"branch", branch}});
    } else {
        void::critical(this, "Checkout Failed", output);
    }
}

void VersionControlWidget::deleteBranch(const std::string& name, bool force)
{
    std::stringList args = {"branch", force ? "-D" : "-d", name};
    
    bool success;
    std::string output = runGitCommand(args, &success);
    
    if (success) {
        updateBranchList();
        void::information(this, "Branch", std::string("Branch '%1' deleted"));
        logVcsEvent("branch_deleted", nlohmann::json{{"name", name}});
    } else {
        void::critical(this, "Branch Deletion Failed", output);
    }
}

void VersionControlWidget::mergeBranch(const std::string& branch)
{
    bool success;
    std::string output = runGitCommand({"merge", branch}, &success);
    
    if (success) {
        refreshStatus();
        updateCommitHistory();
        void::information(this, "Merge", std::string("Branch '%1' merged successfully"));
        operationCompleted("merge", true);
        logVcsEvent("branch_merged", nlohmann::json{{"branch", branch}});
    } else {
        if (output.contains("CONFLICT")) {
            detectMergeConflicts();
        }
        void::critical(this, "Merge Failed", output);
        operationFailed("merge", output);
    }
}

void VersionControlWidget::detectMergeConflicts()
{
    std::stringList conflictFiles;
    
    for (const FileStatus& fs : m_fileStatuses) {
        if (fs.status.contains("UU") || fs.status.contains("AA") || fs.status.contains("DD")) {
            conflictFiles.append(fs.file);
        }
    }
    
    if (!conflictFiles.empty()) {
        mergeConflict(conflictFiles);
        showConflictResolutionDialog(conflictFiles);
    }
}

void VersionControlWidget::showConflictResolutionDialog(const std::stringList& files)
{
    std::string fileList = files.join("\n");
    void::warning(this, "Merge Conflicts",
        std::string("The following files have conflicts:\n\n%1\n\nPlease resolve them manually."));
}

void VersionControlWidget::stash(const std::string& message)
{
    std::stringList args = {"stash", "push"};
    if (!message.empty()) {
        args << "-m" << message;
    }
    
    bool success;
    std::string output = runGitCommand(args, &success);
    
    if (success) {
        refreshStatus();
        void::information(this, "Stash", "Changes stashed successfully");
        logVcsEvent("stash_created", nlohmann::json{{"message", message}});
    } else {
        void::critical(this, "Stash Failed", output);
    }
}

void VersionControlWidget::stashPop()
{
    bool success;
    std::string output = runGitCommand({"stash", "pop"}, &success);
    
    if (success) {
        refreshStatus();
        void::information(this, "Stash", "Stash applied and dropped");
        logVcsEvent("stash_popped");
    } else {
        void::critical(this, "Stash Pop Failed", output);
    }
}

void VersionControlWidget::showDiffForFile(const std::string& file)
{
    std::string diff = getFileDiff(file, false);
    m_diffView->setPlainText(diff);
    m_tabWidget->setCurrentIndex(3); // Switch to diff tab
}

std::string VersionControlWidget::getFileDiff(const std::string& file, bool staged)
{
    std::stringList args = {"diff"};
    if (staged) {
        args << "--cached";
    }
    args << file;
    
    return runGitCommand(args);
}

void VersionControlWidget::logVcsEvent(const std::string& event, const nlohmann::json& data)
{
    nlohmann::json logEntry;
    logEntry["timestamp"] = // DateTime::currentDateTime().toString(ISODate);
    logEntry["component"] = "VersionControlWidget";
    logEntry["event"] = event;
    logEntry["data"] = data;
    
}

// Slot implementations
void VersionControlWidget::onCommitButtonClicked()
{
    std::string message = m_commitMessageEdit->text().trimmed();
    commit(message);
}

void VersionControlWidget::onPushButtonClicked()
{
    push();
}

void VersionControlWidget::onPullButtonClicked()
{
    pull();
}

void VersionControlWidget::onFetchButtonClicked()
{
    fetch();
}

void VersionControlWidget::onRefreshButtonClicked()
{
    refreshStatus();
}

void VersionControlWidget::onStageAllButtonClicked()
{
    stageAll();
}

void VersionControlWidget::onUnstageAllButtonClicked()
{
    unstageAll();
}

void VersionControlWidget::onCreateBranchButtonClicked()
{
    bool ok;
    std::string name = void::getText(this, "Create Branch", "Branch name:", voidEdit::Normal, "", &ok);
    if (ok && !name.empty()) {
        createBranch(name);
    }
}

void VersionControlWidget::onSwitchBranchClicked()
{
    auto* current = m_branchList->currentItem();
    if (current) {
        std::string branchName = current->text().remove(0, 2).trimmed(); // Remove "* " if present
        checkout(branchName);
    }
}

void VersionControlWidget::onMergeBranchButtonClicked()
{
    auto* current = m_branchList->currentItem();
    if (current) {
        std::string branchName = current->text().remove(0, 2).trimmed();
        if (branchName != m_currentBranch) {
            auto reply = void::question(this, "Merge Branch",
                std::string("Merge branch '%1' into '%2'?"),
                void::Yes | void::No);
            if (reply == void::Yes) {
                mergeBranch(branchName);
            }
        }
    }
}

void VersionControlWidget::onStashButtonClicked()
{
    bool ok;
    std::string message = void::getText(this, "Stash Changes", "Stash message (optional):", voidEdit::Normal, "", &ok);
    if (ok) {
        stash(message);
    }
}

void VersionControlWidget::onUnstagedFileDoubleClicked(QTreeWidgetItem* item, int column)
{
    (void)(column);
    std::string file = item->text(0);
    showDiffForFile(file);
}

void VersionControlWidget::onStagedFileDoubleClicked(QTreeWidgetItem* item, int column)
{
    (void)(column);
    std::string file = item->text(0);
    showDiffForFile(file);
}

void VersionControlWidget::onCommitHistoryDoubleClicked(QTreeWidgetItem* item, int column)
{
    (void)(column);
    std::string hash = item->data(0, UserRole).toString();
    std::string diff = getCommitDiff(hash);
    m_commitDetails->setPlainText(diff);
}

std::string VersionControlWidget::getCommitDiff(const std::string& commitHash)
{
    return runGitCommand({"show", commitHash});
}

void VersionControlWidget::onBranchSelectionChanged()
{
    // Could show branch details here
}

void VersionControlWidget::onContextMenuRequested(const struct { int x; int y; }& pos)
{
    QTreeWidget* tree = qobject_cast<QTreeWidget*>(sender());
    if (!tree) return;
    
    QTreeWidgetItem* item = tree->itemAt(pos);
    if (!item) return;
    
    void menu(this);
    std::string file = item->text(0);
    
    if (tree == m_unstagedFiles) {
        menu.addAction("Stage", [this, file]() { stageFile(file); });
        menu.addAction("Discard Changes", [this, file]() { discardChanges(file); });
    } else if (tree == m_stagedFiles) {
        menu.addAction("Unstage", [this, file]() { unstageFile(file); });
    }
    
    menu.addAction("Show Diff", [this, file]() { showDiffForFile(file); });
    menu.addAction("Copy Filename", [file]() { 
        nullptr->setText(file);
    });
    
    menu.exec(tree->mapToGlobal(pos));
}

void VersionControlWidget::onAmendCheckBoxToggled(bool checked)
{
    m_amendMode = checked;
    if (checked) {
        // Load previous commit message
        bool success;
        std::string lastMsg = runGitCommand({"log", "-1", "--pretty=%B"}, &success).trimmed();
        if (success) {
            m_commitMessageEdit->setText(lastMsg);
        }
    } else {
        m_commitMessageEdit->clear();
    }
}

void VersionControlWidget::discardChanges(const std::string& file)
{
    auto reply = void::warning(this, "Discard Changes",
        std::string("Discard all changes in '%1'?\nThis cannot be undone."),
        void::Yes | void::No);
    
    if (reply == void::Yes) {
        bool success;
        runGitCommand({"checkout", "--", file}, &success);
        if (success) {
            refreshStatus();
            logVcsEvent("changes_discarded", nlohmann::json{{"file", file}});
        }
    }
}

// Stub implementations for remaining methods
void VersionControlWidget::rebaseBranch(const std::string& branch) { (void)(branch); }
void VersionControlWidget::cherryPick(const std::string& commitHash) { (void)(commitHash); }
void VersionControlWidget::revert(const std::string& commitHash) { (void)(commitHash); }
void VersionControlWidget::reset(const std::string& commitHash, const std::string& mode) { (void)(commitHash); (void)(mode); }
void VersionControlWidget::stashApply(int index) { (void)(index); }
void VersionControlWidget::stashDrop(int index) { (void)(index); }
void VersionControlWidget::showLog(int maxCount) { (void)(maxCount); updateCommitHistory(); }
void VersionControlWidget::showFileHistory(const std::string& file) { (void)(file); }
void VersionControlWidget::showBlame(const std::string& file) { (void)(file); }
void VersionControlWidget::addRemote(const std::string& name, const std::string& url) { runGitCommand({"remote", "add", name, url}); listRemotes(); }
void VersionControlWidget::removeRemote(const std::string& name) { runGitCommand({"remote", "remove", name}); listRemotes(); }
void VersionControlWidget::listRemotes() { m_remotes = runGitCommandList({"remote", "-v"}); m_remoteList->clear(); m_remoteList->addItems(m_remotes); m_stats.totalRemotes = m_remotes.size(); }
void VersionControlWidget::createTag(const std::string& name, const std::string& commitHash, const std::string& message) { (void)(name); (void)(commitHash); (void)(message); }
void VersionControlWidget::deleteTag(const std::string& name) { (void)(name); }
void VersionControlWidget::pushTags() { runGitCommand({"push", "--tags"}); }
bool VersionControlWidget::hasConflicts() { return false; }
void VersionControlWidget::updateRepositoryInfo() {}
void VersionControlWidget::showCommitDialog() {}
void VersionControlWidget::updateStashList() {}

