#include "version_control_widget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTreeWidget>
#include <QTextEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QComboBox>
#include <QListWidget>
#include <QTabWidget>
#include <QToolBar>
#include <QLabel>
#include <QSplitter>
#include <QCheckBox>
#include <QInputDialog>
#include <QMessageBox>
#include <QFileDialog>
#include <QProcess>
#include <QRegularExpression>
#include <QMenu>
#include <QAction>
#include <QGroupBox>
#include <QFormLayout>
#include <QScrollBar>
#include <QDateTime>
#include <QDebug>
#include <QSettings>
#include <QJsonObject>
#include <QJsonDocument>
#include <QDir>
#include <QFileInfo>
#include <QApplication>
#include <QClipboard>

VersionControlWidget::VersionControlWidget(QWidget* parent)
    : QWidget(parent)
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
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->setSpacing(4);
    
    // Toolbar
    createToolBar();
    mainLayout->addWidget(m_toolBar);
    
    // Repository info bar
    QHBoxLayout* infoLayout = new QHBoxLayout();
    m_branchLabel = new QLabel("Branch: <none>", this);
    m_branchLabel->setStyleSheet("QLabel { font-weight: bold; padding: 4px; }");
    infoLayout->addWidget(m_branchLabel);
    
    m_repoStatusLabel = new QLabel("No repository", this);
    m_repoStatusLabel->setStyleSheet("QLabel { padding: 4px; color: #888; }");
    infoLayout->addWidget(m_repoStatusLabel, 1);
    mainLayout->addLayout(infoLayout);
    
    // Main content tabs
    m_tabWidget = new QTabWidget(this);
    
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
    m_toolBar = new QToolBar("VCS Tools", this);
    m_toolBar->setIconSize(QSize(16, 16));
    
    QPushButton* refreshBtn = new QPushButton(QIcon(":/icons/refresh.png"), "Refresh", this);
    refreshBtn->setToolTip("Refresh repository status (F5)");
    refreshBtn->setShortcut(Qt::Key_F5);
    connect(refreshBtn, &QPushButton::clicked, this, &VersionControlWidget::onRefreshButtonClicked);
    m_toolBar->addWidget(refreshBtn);
    
    m_toolBar->addSeparator();
    
    QPushButton* pullBtn = new QPushButton(QIcon(":/icons/pull.png"), "Pull", this);
    pullBtn->setToolTip("Pull from remote repository");
    connect(pullBtn, &QPushButton::clicked, this, &VersionControlWidget::onPullButtonClicked);
    m_toolBar->addWidget(pullBtn);
    
    QPushButton* pushBtn = new QPushButton(QIcon(":/icons/push.png"), "Push", this);
    pushBtn->setToolTip("Push to remote repository");
    connect(pushBtn, &QPushButton::clicked, this, &VersionControlWidget::onPushButtonClicked);
    m_toolBar->addWidget(pushBtn);
    
    QPushButton* fetchBtn = new QPushButton(QIcon(":/icons/fetch.png"), "Fetch", this);
    fetchBtn->setToolTip("Fetch from remote repository");
    connect(fetchBtn, &QPushButton::clicked, this, &VersionControlWidget::onFetchButtonClicked);
    m_toolBar->addWidget(fetchBtn);
    
    m_toolBar->addSeparator();
    
    QPushButton* stashBtn = new QPushButton(QIcon(":/icons/stash.png"), "Stash", this);
    stashBtn->setToolTip("Stash current changes");
    connect(stashBtn, &QPushButton::clicked, this, &VersionControlWidget::onStashButtonClicked);
    m_toolBar->addWidget(stashBtn);
}

void VersionControlWidget::createStatusView()
{
    QWidget* statusWidget = new QWidget(this);
    QVBoxLayout* layout = new QVBoxLayout(statusWidget);
    layout->setContentsMargins(4, 4, 4, 4);
    
    // Unstaged files
    QGroupBox* unstagedGroup = new QGroupBox("Unstaged Changes", this);
    QVBoxLayout* unstagedLayout = new QVBoxLayout(unstagedGroup);
    
    m_unstagedFiles = new QTreeWidget(this);
    m_unstagedFiles->setHeaderLabels({"File", "Status"});
    m_unstagedFiles->setColumnWidth(0, 300);
    m_unstagedFiles->setContextMenuPolicy(Qt::CustomContextMenu);
    m_unstagedFiles->setSelectionMode(QAbstractItemView::ExtendedSelection);
    unstagedLayout->addWidget(m_unstagedFiles);
    
    QHBoxLayout* unstagedButtons = new QHBoxLayout();
    m_stageAllBtn = new QPushButton("Stage All", this);
    connect(m_stageAllBtn, &QPushButton::clicked, this, &VersionControlWidget::onStageAllButtonClicked);
    unstagedButtons->addWidget(m_stageAllBtn);
    
    QPushButton* stageSelectedBtn = new QPushButton("Stage Selected", this);
    connect(stageSelectedBtn, &QPushButton::clicked, [this]() {
        auto items = m_unstagedFiles->selectedItems();
        for (auto* item : items) {
            stageFile(item->text(0));
        }
    });
    unstagedButtons->addWidget(stageSelectedBtn);
    unstagedButtons->addStretch();
    
    unstagedLayout->addLayout(unstagedButtons);
    layout->addWidget(unstagedGroup);
    
    // Staged files
    QGroupBox* stagedGroup = new QGroupBox("Staged Changes", this);
    QVBoxLayout* stagedLayout = new QVBoxLayout(stagedGroup);
    
    m_stagedFiles = new QTreeWidget(this);
    m_stagedFiles->setHeaderLabels({"File", "Status"});
    m_stagedFiles->setColumnWidth(0, 300);
    m_stagedFiles->setContextMenuPolicy(Qt::CustomContextMenu);
    m_stagedFiles->setSelectionMode(QAbstractItemView::ExtendedSelection);
    stagedLayout->addWidget(m_stagedFiles);
    
    QHBoxLayout* stagedButtons = new QHBoxLayout();
    m_unstageAllBtn = new QPushButton("Unstage All", this);
    connect(m_unstageAllBtn, &QPushButton::clicked, this, &VersionControlWidget::onUnstageAllButtonClicked);
    stagedButtons->addWidget(m_unstageAllBtn);
    
    QPushButton* unstageSelectedBtn = new QPushButton("Unstage Selected", this);
    connect(unstageSelectedBtn, &QPushButton::clicked, [this]() {
        auto items = m_stagedFiles->selectedItems();
        for (auto* item : items) {
            unstageFile(item->text(0));
        }
    });
    stagedButtons->addWidget(unstageSelectedBtn);
    stagedButtons->addStretch();
    
    stagedLayout->addLayout(stagedButtons);
    layout->addWidget(stagedGroup);
    
    // Commit section
    QGroupBox* commitGroup = new QGroupBox("Commit", this);
    QVBoxLayout* commitLayout = new QVBoxLayout(commitGroup);
    
    m_commitMessageEdit = new QLineEdit(this);
    m_commitMessageEdit->setPlaceholderText("Commit message...");
    commitLayout->addWidget(m_commitMessageEdit);
    
    QHBoxLayout* commitButtonLayout = new QHBoxLayout();
    m_amendCheckBox = new QCheckBox("Amend previous commit", this);
    connect(m_amendCheckBox, &QCheckBox::toggled, this, &VersionControlWidget::onAmendCheckBoxToggled);
    commitButtonLayout->addWidget(m_amendCheckBox);
    
    m_commitBtn = new QPushButton(QIcon(":/icons/commit.png"), "Commit", this);
    m_commitBtn->setToolTip("Commit staged changes (Ctrl+Enter)");
    m_commitBtn->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Return));
    connect(m_commitBtn, &QPushButton::clicked, this, &VersionControlWidget::onCommitButtonClicked);
    commitButtonLayout->addWidget(m_commitBtn);
    
    commitLayout->addLayout(commitButtonLayout);
    layout->addWidget(commitGroup);
    
    m_tabWidget->addTab(statusWidget, QIcon(":/icons/changes.png"), "Changes");
}

void VersionControlWidget::createHistoryView()
{
    QWidget* historyWidget = new QWidget(this);
    QVBoxLayout* layout = new QVBoxLayout(historyWidget);
    layout->setContentsMargins(4, 4, 4, 4);
    
    QSplitter* splitter = new QSplitter(Qt::Vertical, this);
    
    m_commitHistory = new QTreeWidget(this);
    m_commitHistory->setHeaderLabels({"Hash", "Author", "Date", "Message"});
    m_commitHistory->setColumnWidth(0, 100);
    m_commitHistory->setColumnWidth(1, 150);
    m_commitHistory->setColumnWidth(2, 150);
    m_commitHistory->setSelectionMode(QAbstractItemView::SingleSelection);
    splitter->addWidget(m_commitHistory);
    
    m_commitDetails = new QTextEdit(this);
    m_commitDetails->setReadOnly(true);
    m_commitDetails->setFont(QFont("Consolas", 9));
    m_commitDetails->setStyleSheet("QTextEdit { background-color: #1e1e1e; color: #d4d4d4; }");
    splitter->addWidget(m_commitDetails);
    
    splitter->setStretchFactor(0, 2);
    splitter->setStretchFactor(1, 1);
    
    layout->addWidget(splitter);
    
    m_tabWidget->addTab(historyWidget, QIcon(":/icons/history.png"), "History");
}

void VersionControlWidget::createBranchView()
{
    QWidget* branchWidget = new QWidget(this);
    QHBoxLayout* layout = new QHBoxLayout(branchWidget);
    layout->setContentsMargins(4, 4, 4, 4);
    
    // Local branches
    QGroupBox* localGroup = new QGroupBox("Local Branches", this);
    QVBoxLayout* localLayout = new QVBoxLayout(localGroup);
    
    m_branchList = new QListWidget(this);
    m_branchList->setSelectionMode(QAbstractItemView::SingleSelection);
    localLayout->addWidget(m_branchList);
    
    QHBoxLayout* branchButtons = new QHBoxLayout();
    m_createBranchBtn = new QPushButton("New", this);
    connect(m_createBranchBtn, &QPushButton::clicked, this, &VersionControlWidget::onCreateBranchButtonClicked);
    branchButtons->addWidget(m_createBranchBtn);
    
    QPushButton* switchBranchBtn = new QPushButton("Switch", this);
    connect(switchBranchBtn, &QPushButton::clicked, this, &VersionControlWidget::onSwitchBranchClicked);
    branchButtons->addWidget(switchBranchBtn);
    
    m_deleteBranchBtn = new QPushButton("Delete", this);
    connect(m_deleteBranchBtn, &QPushButton::clicked, [this]() {
        auto* current = m_branchList->currentItem();
        if (current) {
            QString branchName = current->text().remove(0, 2); // Remove "* " prefix if present
            auto reply = QMessageBox::question(this, "Delete Branch",
                QString("Delete branch '%1'?").arg(branchName),
                QMessageBox::Yes | QMessageBox::No);
            if (reply == QMessageBox::Yes) {
                deleteBranch(branchName);
            }
        }
    });
    branchButtons->addWidget(m_deleteBranchBtn);
    
    m_mergeBranchBtn = new QPushButton("Merge", this);
    connect(m_mergeBranchBtn, &QPushButton::clicked, this, &VersionControlWidget::onMergeBranchButtonClicked);
    branchButtons->addWidget(m_mergeBranchBtn);
    
    localLayout->addLayout(branchButtons);
    layout->addWidget(localGroup);
    
    // Remotes
    QGroupBox* remoteGroup = new QGroupBox("Remotes", this);
    QVBoxLayout* remoteLayout = new QVBoxLayout(remoteGroup);
    
    m_remoteList = new QListWidget(this);
    remoteLayout->addWidget(m_remoteList);
    
    QHBoxLayout* remoteButtons = new QHBoxLayout();
    QPushButton* addRemoteBtn = new QPushButton("Add Remote", this);
    connect(addRemoteBtn, &QPushButton::clicked, [this]() {
        bool ok;
        QString name = QInputDialog::getText(this, "Add Remote", "Remote name:", QLineEdit::Normal, "", &ok);
        if (ok && !name.isEmpty()) {
            QString url = QInputDialog::getText(this, "Add Remote", "Remote URL:", QLineEdit::Normal, "", &ok);
            if (ok && !url.isEmpty()) {
                addRemote(name, url);
            }
        }
    });
    remoteButtons->addWidget(addRemoteBtn);
    remoteButtons->addStretch();
    
    remoteLayout->addLayout(remoteButtons);
    layout->addWidget(remoteGroup);
    
    m_tabWidget->addTab(branchWidget, QIcon(":/icons/branch.png"), "Branches");
}

void VersionControlWidget::createDiffView()
{
    QWidget* diffWidget = new QWidget(this);
    QVBoxLayout* layout = new QVBoxLayout(diffWidget);
    layout->setContentsMargins(4, 4, 4, 4);
    
    m_diffView = new QTextEdit(this);
    m_diffView->setReadOnly(true);
    m_diffView->setFont(QFont("Consolas", 9));
    m_diffView->setStyleSheet("QTextEdit { background-color: #1e1e1e; color: #d4d4d4; }");
    m_diffView->setLineWrapMode(QTextEdit::NoWrap);
    layout->addWidget(m_diffView);
    
    m_tabWidget->addTab(diffWidget, QIcon(":/icons/diff.png"), "Diff");
}

void VersionControlWidget::connectSignals()
{
    connect(m_unstagedFiles, &QTreeWidget::itemDoubleClicked,
            this, &VersionControlWidget::onUnstagedFileDoubleClicked);
    connect(m_stagedFiles, &QTreeWidget::itemDoubleClicked,
            this, &VersionControlWidget::onStagedFileDoubleClicked);
    connect(m_commitHistory, &QTreeWidget::itemDoubleClicked,
            this, &VersionControlWidget::onCommitHistoryDoubleClicked);
    connect(m_branchList, &QListWidget::currentItemChanged,
            this, &VersionControlWidget::onBranchSelectionChanged);
    connect(m_unstagedFiles, &QTreeWidget::customContextMenuRequested,
            this, &VersionControlWidget::onContextMenuRequested);
    connect(m_stagedFiles, &QTreeWidget::customContextMenuRequested,
            this, &VersionControlWidget::onContextMenuRequested);
}

void VersionControlWidget::setRepositoryPath(const QString& path)
{
    m_repoPath = path;
    
    if (isValidRepository()) {
        m_repoStatusLabel->setText(QString("Repository: %1").arg(QFileInfo(path).fileName()));
        refreshStatus();
        updateBranchList();
        updateCommitHistory();
        logVcsEvent("repository_opened", QJsonObject{{"path", path}});
    } else {
        m_repoStatusLabel->setText("Not a valid Git repository");
        logVcsEvent("invalid_repository", QJsonObject{{"path", path}});
    }
}

bool VersionControlWidget::isValidRepository() const
{
    if (m_repoPath.isEmpty()) return false;
    
    QDir repoDir(m_repoPath);
    return repoDir.exists(".git") || repoDir.exists("../.git");
}

QString VersionControlWidget::runGitCommand(const QStringList& args, bool* success)
{
    QProcess process;
    process.setWorkingDirectory(m_repoPath);
    process.start("git", args);
    
    if (!process.waitForFinished(30000)) {
        if (success) *success = false;
        return QString();
    }
    
    if (success) {
        *success = (process.exitCode() == 0);
    }
    
    QString output = process.readAllStandardOutput();
    QString error = process.readAllStandardError();
    
    return error.isEmpty() ? output : error;
}

QStringList VersionControlWidget::runGitCommandList(const QStringList& args, bool* success)
{
    QString output = runGitCommand(args, success);
    return output.split('\n', Qt::SkipEmptyParts);
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
    QString branch = runGitCommand({"branch", "--show-current"}, &success).trimmed();
    if (success && !branch.isEmpty()) {
        m_currentBranch = branch;
        m_branchLabel->setText(QString("Branch: %1").arg(branch));
        m_stats.currentBranch = branch;
    }
    
    // Get status
    QString status = runGitCommand({"status", "--porcelain"}, &success);
    if (success) {
        parseGitStatus(status);
        updateFileStatus();
    }
    
    emit statusChanged();
    logVcsEvent("status_refreshed");
}

void VersionControlWidget::parseGitStatus(const QString& output)
{
    m_fileStatuses.clear();
    
    QStringList lines = output.split('\n', Qt::SkipEmptyParts);
    for (const QString& line : lines) {
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
    
    m_tabWidget->setTabText(0, QString("Changes (%1)").arg(stagedCount + unstagedCount));
}

void VersionControlWidget::addFileToTree(QTreeWidget* tree, const QString& file, const QString& status)
{
    QTreeWidgetItem* item = new QTreeWidgetItem(tree);
    item->setText(0, file);
    item->setText(1, status);
    item->setForeground(1, QColor(getStatusColor(status)));
    item->setIcon(0, getStatusIcon(status));
}

QString VersionControlWidget::getStatusColor(const QString& status)
{
    if (status.contains('M')) return "#ffa500"; // Modified - orange
    if (status.contains('A')) return "#00ff00"; // Added - green
    if (status.contains('D')) return "#ff0000"; // Deleted - red
    if (status.contains('R')) return "#00ffff"; // Renamed - cyan
    if (status.contains('?')) return "#888888"; // Untracked - gray
    return "#ffffff";
}

QIcon VersionControlWidget::getStatusIcon(const QString& status)
{
    if (status.contains('M')) return QIcon(":/icons/file-modified.png");
    if (status.contains('A')) return QIcon(":/icons/file-added.png");
    if (status.contains('D')) return QIcon(":/icons/file-deleted.png");
    if (status.contains('R')) return QIcon(":/icons/file-renamed.png");
    if (status.contains('?')) return QIcon(":/icons/file-untracked.png");
    return QIcon(":/icons/file.png");
}

void VersionControlWidget::stageFile(const QString& file)
{
    bool success;
    runGitCommand({"add", file}, &success);
    if (success) {
        refreshStatus();
        emit fileStaged(file);
        logVcsEvent("file_staged", QJsonObject{{"file", file}});
    }
}

void VersionControlWidget::unstageFile(const QString& file)
{
    bool success;
    runGitCommand({"reset", "HEAD", file}, &success);
    if (success) {
        refreshStatus();
        emit fileUnstaged(file);
        logVcsEvent("file_unstaged", QJsonObject{{"file", file}});
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

void VersionControlWidget::commit(const QString& message)
{
    if (message.isEmpty()) {
        QMessageBox::warning(this, "Commit", "Please enter a commit message.");
        return;
    }
    
    QStringList args = {"commit", "-m", message};
    if (m_amendMode) {
        args.insert(1, "--amend");
    }
    
    bool success;
    QString output = runGitCommand(args, &success);
    
    if (success) {
        // Extract commit hash
        QRegularExpression hashRegex("\\[\\w+\\s+(\\w+)\\]");
        QRegularExpressionMatch match = hashRegex.match(output);
        QString hash = match.hasMatch() ? match.captured(1) : "";
        
        m_commitMessageEdit->clear();
        refreshStatus();
        updateCommitHistory();
        emit commitCreated(hash);
        logVcsEvent("commit_created", QJsonObject{{"hash", hash}, {"message", message}});
        
        QMessageBox::information(this, "Commit", "Commit successful!");
    } else {
        QMessageBox::critical(this, "Commit Failed", output);
        emit operationFailed("commit", output);
    }
}

void VersionControlWidget::push(const QString& remote, const QString& branch)
{
    QString remoteName = remote.isEmpty() ? "origin" : remote;
    QString branchName = branch.isEmpty() ? m_currentBranch : branch;
    
    bool success;
    QString output = runGitCommand({"push", remoteName, branchName}, &success);
    
    if (success) {
        QMessageBox::information(this, "Push", "Push successful!");
        emit operationCompleted("push", true);
        logVcsEvent("push_completed", QJsonObject{{"remote", remoteName}, {"branch", branchName}});
    } else {
        QMessageBox::critical(this, "Push Failed", output);
        emit operationFailed("push", output);
    }
}

void VersionControlWidget::pull(const QString& remote, const QString& branch)
{
    QString remoteName = remote.isEmpty() ? "origin" : remote;
    QString branchName = branch.isEmpty() ? m_currentBranch : branch;
    
    bool success;
    QString output = runGitCommand({"pull", remoteName, branchName}, &success);
    
    if (success) {
        refreshStatus();
        updateCommitHistory();
        QMessageBox::information(this, "Pull", "Pull successful!");
        emit operationCompleted("pull", true);
        logVcsEvent("pull_completed");
    } else {
        if (output.contains("CONFLICT")) {
            detectMergeConflicts();
        }
        QMessageBox::critical(this, "Pull Failed", output);
        emit operationFailed("pull", output);
    }
}

void VersionControlWidget::fetch(const QString& remote)
{
    QString remoteName = remote.isEmpty() ? "origin" : remote;
    
    bool success;
    QString output = runGitCommand({"fetch", remoteName}, &success);
    
    if (success) {
        QMessageBox::information(this, "Fetch", "Fetch successful!");
        emit operationCompleted("fetch", true);
        logVcsEvent("fetch_completed", QJsonObject{{"remote", remoteName}});
    } else {
        QMessageBox::critical(this, "Fetch Failed", output);
        emit operationFailed("fetch", output);
    }
}

void VersionControlWidget::updateBranchList()
{
    bool success;
    QStringList branches = runGitCommandList({"branch"}, &success);
    
    m_branchList->clear();
    m_branches.clear();
    
    for (const QString& branch : branches) {
        QString branchName = branch.trimmed();
        m_branches.append(branchName);
        m_branchList->addItem(branchName);
        
        if (branchName.startsWith("* ")) {
            m_branchList->item(m_branchList->count() - 1)->setForeground(QColor("#00ff00"));
        }
    }
    
    m_stats.totalBranches = m_branches.size();
}

void VersionControlWidget::updateCommitHistory()
{
    bool success;
    QStringList log = runGitCommandList({"log", "--oneline", "--max-count=100", 
                                         "--format=%H|%an|%ad|%s", "--date=short"}, &success);
    
    m_commitHistory->clear();
    
    for (const QString& entry : log) {
        QStringList parts = entry.split('|');
        if (parts.size() >= 4) {
            QTreeWidgetItem* item = new QTreeWidgetItem(m_commitHistory);
            item->setText(0, parts[0].left(8)); // Short hash
            item->setText(1, parts[1]); // Author
            item->setText(2, parts[2]); // Date
            item->setText(3, parts[3]); // Message
            item->setData(0, Qt::UserRole, parts[0]); // Full hash
        }
    }
    
    m_stats.totalCommits = log.size();
}

void VersionControlWidget::createBranch(const QString& name, const QString& baseBranch)
{
    QStringList args = {"branch", name};
    if (!baseBranch.isEmpty()) {
        args.append(baseBranch);
    }
    
    bool success;
    QString output = runGitCommand(args, &success);
    
    if (success) {
        updateBranchList();
        QMessageBox::information(this, "Branch", QString("Branch '%1' created").arg(name));
        logVcsEvent("branch_created", QJsonObject{{"name", name}});
    } else {
        QMessageBox::critical(this, "Branch Creation Failed", output);
    }
}

void VersionControlWidget::checkout(const QString& branch)
{
    bool success;
    QString output = runGitCommand({"checkout", branch}, &success);
    
    if (success) {
        m_currentBranch = branch;
        refreshStatus();
        updateBranchList();
        emit branchChanged(branch);
        logVcsEvent("branch_switched", QJsonObject{{"branch", branch}});
    } else {
        QMessageBox::critical(this, "Checkout Failed", output);
    }
}

void VersionControlWidget::deleteBranch(const QString& name, bool force)
{
    QStringList args = {"branch", force ? "-D" : "-d", name};
    
    bool success;
    QString output = runGitCommand(args, &success);
    
    if (success) {
        updateBranchList();
        QMessageBox::information(this, "Branch", QString("Branch '%1' deleted").arg(name));
        logVcsEvent("branch_deleted", QJsonObject{{"name", name}});
    } else {
        QMessageBox::critical(this, "Branch Deletion Failed", output);
    }
}

void VersionControlWidget::mergeBranch(const QString& branch)
{
    bool success;
    QString output = runGitCommand({"merge", branch}, &success);
    
    if (success) {
        refreshStatus();
        updateCommitHistory();
        QMessageBox::information(this, "Merge", QString("Branch '%1' merged successfully").arg(branch));
        emit operationCompleted("merge", true);
        logVcsEvent("branch_merged", QJsonObject{{"branch", branch}});
    } else {
        if (output.contains("CONFLICT")) {
            detectMergeConflicts();
        }
        QMessageBox::critical(this, "Merge Failed", output);
        emit operationFailed("merge", output);
    }
}

void VersionControlWidget::detectMergeConflicts()
{
    QStringList conflictFiles;
    
    for (const FileStatus& fs : m_fileStatuses) {
        if (fs.status.contains("UU") || fs.status.contains("AA") || fs.status.contains("DD")) {
            conflictFiles.append(fs.file);
        }
    }
    
    if (!conflictFiles.isEmpty()) {
        emit mergeConflict(conflictFiles);
        showConflictResolutionDialog(conflictFiles);
    }
}

void VersionControlWidget::showConflictResolutionDialog(const QStringList& files)
{
    QString fileList = files.join("\n");
    QMessageBox::warning(this, "Merge Conflicts",
        QString("The following files have conflicts:\n\n%1\n\nPlease resolve them manually.").arg(fileList));
}

void VersionControlWidget::stash(const QString& message)
{
    QStringList args = {"stash", "push"};
    if (!message.isEmpty()) {
        args << "-m" << message;
    }
    
    bool success;
    QString output = runGitCommand(args, &success);
    
    if (success) {
        refreshStatus();
        QMessageBox::information(this, "Stash", "Changes stashed successfully");
        logVcsEvent("stash_created", QJsonObject{{"message", message}});
    } else {
        QMessageBox::critical(this, "Stash Failed", output);
    }
}

void VersionControlWidget::stashPop()
{
    bool success;
    QString output = runGitCommand({"stash", "pop"}, &success);
    
    if (success) {
        refreshStatus();
        QMessageBox::information(this, "Stash", "Stash applied and dropped");
        logVcsEvent("stash_popped");
    } else {
        QMessageBox::critical(this, "Stash Pop Failed", output);
    }
}

void VersionControlWidget::showDiffForFile(const QString& file)
{
    QString diff = getFileDiff(file, false);
    m_diffView->setPlainText(diff);
    m_tabWidget->setCurrentIndex(3); // Switch to diff tab
}

QString VersionControlWidget::getFileDiff(const QString& file, bool staged)
{
    QStringList args = {"diff"};
    if (staged) {
        args << "--cached";
    }
    args << file;
    
    return runGitCommand(args);
}

void VersionControlWidget::logVcsEvent(const QString& event, const QJsonObject& data)
{
    QJsonObject logEntry;
    logEntry["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    logEntry["component"] = "VersionControlWidget";
    logEntry["event"] = event;
    logEntry["data"] = data;
    
    qDebug().noquote() << "VCS_EVENT:" << QJsonDocument(logEntry).toJson(QJsonDocument::Compact);
}

// Slot implementations
void VersionControlWidget::onCommitButtonClicked()
{
    QString message = m_commitMessageEdit->text().trimmed();
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
    QString name = QInputDialog::getText(this, "Create Branch", "Branch name:", QLineEdit::Normal, "", &ok);
    if (ok && !name.isEmpty()) {
        createBranch(name);
    }
}

void VersionControlWidget::onSwitchBranchClicked()
{
    auto* current = m_branchList->currentItem();
    if (current) {
        QString branchName = current->text().remove(0, 2).trimmed(); // Remove "* " if present
        checkout(branchName);
    }
}

void VersionControlWidget::onMergeBranchButtonClicked()
{
    auto* current = m_branchList->currentItem();
    if (current) {
        QString branchName = current->text().remove(0, 2).trimmed();
        if (branchName != m_currentBranch) {
            auto reply = QMessageBox::question(this, "Merge Branch",
                QString("Merge branch '%1' into '%2'?").arg(branchName, m_currentBranch),
                QMessageBox::Yes | QMessageBox::No);
            if (reply == QMessageBox::Yes) {
                mergeBranch(branchName);
            }
        }
    }
}

void VersionControlWidget::onStashButtonClicked()
{
    bool ok;
    QString message = QInputDialog::getText(this, "Stash Changes", "Stash message (optional):", QLineEdit::Normal, "", &ok);
    if (ok) {
        stash(message);
    }
}

void VersionControlWidget::onUnstagedFileDoubleClicked(QTreeWidgetItem* item, int column)
{
    Q_UNUSED(column);
    QString file = item->text(0);
    showDiffForFile(file);
}

void VersionControlWidget::onStagedFileDoubleClicked(QTreeWidgetItem* item, int column)
{
    Q_UNUSED(column);
    QString file = item->text(0);
    showDiffForFile(file);
}

void VersionControlWidget::onCommitHistoryDoubleClicked(QTreeWidgetItem* item, int column)
{
    Q_UNUSED(column);
    QString hash = item->data(0, Qt::UserRole).toString();
    QString diff = getCommitDiff(hash);
    m_commitDetails->setPlainText(diff);
}

QString VersionControlWidget::getCommitDiff(const QString& commitHash)
{
    return runGitCommand({"show", commitHash});
}

void VersionControlWidget::onBranchSelectionChanged()
{
    // Could show branch details here
}

void VersionControlWidget::onContextMenuRequested(const QPoint& pos)
{
    QTreeWidget* tree = qobject_cast<QTreeWidget*>(sender());
    if (!tree) return;
    
    QTreeWidgetItem* item = tree->itemAt(pos);
    if (!item) return;
    
    QMenu menu(this);
    QString file = item->text(0);
    
    if (tree == m_unstagedFiles) {
        menu.addAction("Stage", [this, file]() { stageFile(file); });
        menu.addAction("Discard Changes", [this, file]() { discardChanges(file); });
    } else if (tree == m_stagedFiles) {
        menu.addAction("Unstage", [this, file]() { unstageFile(file); });
    }
    
    menu.addAction("Show Diff", [this, file]() { showDiffForFile(file); });
    menu.addAction("Copy Filename", [file]() { 
        QApplication::clipboard()->setText(file);
    });
    
    menu.exec(tree->mapToGlobal(pos));
}

void VersionControlWidget::onAmendCheckBoxToggled(bool checked)
{
    m_amendMode = checked;
    if (checked) {
        // Load previous commit message
        bool success;
        QString lastMsg = runGitCommand({"log", "-1", "--pretty=%B"}, &success).trimmed();
        if (success) {
            m_commitMessageEdit->setText(lastMsg);
        }
    } else {
        m_commitMessageEdit->clear();
    }
}

void VersionControlWidget::discardChanges(const QString& file)
{
    auto reply = QMessageBox::warning(this, "Discard Changes",
        QString("Discard all changes in '%1'?\nThis cannot be undone.").arg(file),
        QMessageBox::Yes | QMessageBox::No);
    
    if (reply == QMessageBox::Yes) {
        bool success;
        runGitCommand({"checkout", "--", file}, &success);
        if (success) {
            refreshStatus();
            logVcsEvent("changes_discarded", QJsonObject{{"file", file}});
        }
    }
}

// Stub implementations for remaining methods
void VersionControlWidget::rebaseBranch(const QString& branch) { Q_UNUSED(branch); }
void VersionControlWidget::cherryPick(const QString& commitHash) { Q_UNUSED(commitHash); }
void VersionControlWidget::revert(const QString& commitHash) { Q_UNUSED(commitHash); }
void VersionControlWidget::reset(const QString& commitHash, const QString& mode) { Q_UNUSED(commitHash); Q_UNUSED(mode); }
void VersionControlWidget::stashApply(int index) { Q_UNUSED(index); }
void VersionControlWidget::stashDrop(int index) { Q_UNUSED(index); }
void VersionControlWidget::showLog(int maxCount) { Q_UNUSED(maxCount); updateCommitHistory(); }
void VersionControlWidget::showFileHistory(const QString& file) { Q_UNUSED(file); }
void VersionControlWidget::showBlame(const QString& file) { Q_UNUSED(file); }
void VersionControlWidget::addRemote(const QString& name, const QString& url) { runGitCommand({"remote", "add", name, url}); listRemotes(); }
void VersionControlWidget::removeRemote(const QString& name) { runGitCommand({"remote", "remove", name}); listRemotes(); }
void VersionControlWidget::listRemotes() { m_remotes = runGitCommandList({"remote", "-v"}); m_remoteList->clear(); m_remoteList->addItems(m_remotes); m_stats.totalRemotes = m_remotes.size(); }
void VersionControlWidget::createTag(const QString& name, const QString& commitHash, const QString& message) { Q_UNUSED(name); Q_UNUSED(commitHash); Q_UNUSED(message); }
void VersionControlWidget::deleteTag(const QString& name) { Q_UNUSED(name); }
void VersionControlWidget::pushTags() { runGitCommand({"push", "--tags"}); }
bool VersionControlWidget::hasConflicts() { return false; }
void VersionControlWidget::updateRepositoryInfo() {}
void VersionControlWidget::showCommitDialog() {}
void VersionControlWidget::updateStashList() {}
