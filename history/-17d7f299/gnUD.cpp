/**
 * @file version_control_widget.cpp
 * @brief Production implementation of VersionControlWidget
 * 
 * Per AI Toolkit Production Readiness Instructions:
 * - NO SIMPLIFICATIONS - all logic must remain intact
 * - Full structured logging for observability
 * - Comprehensive error handling
 */

#include "version_control_widget.h"
#include <QHeaderView>
#include <QMessageBox>
#include <QInputDialog>
#include <QDir>
#include <QFileInfo>
#include <QDateTime>
#include <QApplication>
#include <QStyle>
#include <QScrollBar>

// ==================== Structured Logging ====================
#define LOG_VCS(level, msg) \
    qDebug() << QString("[%1] [VersionControlWidget] [%2] %3") \
        .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz")) \
        .arg(level) \
        .arg(msg)

#define LOG_DEBUG(msg) LOG_VCS("DEBUG", msg)
#define LOG_INFO(msg)  LOG_VCS("INFO", msg)
#define LOG_WARN(msg)  LOG_VCS("WARN", msg)
#define LOG_ERROR(msg) LOG_VCS("ERROR", msg)

// ==================== Constructor/Destructor ====================
VersionControlWidget::VersionControlWidget(QWidget* parent)
    : QWidget(parent)
    , m_mainLayout(nullptr)
    , m_toolbar(nullptr)
    , m_branchCombo(nullptr)
    , m_refreshButton(nullptr)
    , m_pushButton(nullptr)
    , m_pullButton(nullptr)
    , m_fetchButton(nullptr)
    , m_splitter(nullptr)
    , m_stagedWidget(nullptr)
    , m_stagedLabel(nullptr)
    , m_stagedTree(nullptr)
    , m_unstageAllButton(nullptr)
    , m_unstagedWidget(nullptr)
    , m_unstagedLabel(nullptr)
    , m_unstagedTree(nullptr)
    , m_stageAllButton(nullptr)
    , m_discardAllButton(nullptr)
    , m_commitWidget(nullptr)
    , m_commitMessage(nullptr)
    , m_commitButton(nullptr)
    , m_amendButton(nullptr)
    , m_isRepo(false)
    , m_watcher(nullptr)
    , m_refreshTimer(nullptr)
    , m_autoRefresh(true)
{
    LOG_INFO("Initializing VersionControlWidget...");
    
    setupUI();
    setupToolbar();
    setupConnections();
    
    // Initialize file watcher for auto-refresh
    m_watcher = new QFileSystemWatcher(this);
    m_refreshTimer = new QTimer(this);
    m_refreshTimer->setInterval(2000);  // 2 second debounce
    m_refreshTimer->setSingleShot(true);
    
    connect(m_watcher, &QFileSystemWatcher::directoryChanged, this, [this]() {
        if (m_autoRefresh) {
            m_refreshTimer->start();
        }
    });
    
    connect(m_refreshTimer, &QTimer::timeout, this, &VersionControlWidget::refresh);
    
    LOG_INFO("VersionControlWidget initialized successfully");
}

VersionControlWidget::~VersionControlWidget()
{
    LOG_INFO("Destroying VersionControlWidget...");
}

// ==================== UI Setup ====================
void VersionControlWidget::setupUI()
{
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(0, 0, 0, 0);
    m_mainLayout->setSpacing(2);

    // Toolbar
    m_toolbar = new QToolBar(this);
    m_toolbar->setMovable(false);
    m_toolbar->setIconSize(QSize(16, 16));
    m_mainLayout->addWidget(m_toolbar);

    // Main splitter
    m_splitter = new QSplitter(Qt::Vertical, this);

    // Staged changes section
    m_stagedWidget = new QWidget(this);
    QVBoxLayout* stagedLayout = new QVBoxLayout(m_stagedWidget);
    stagedLayout->setContentsMargins(4, 4, 4, 4);
    stagedLayout->setSpacing(2);

    QHBoxLayout* stagedHeader = new QHBoxLayout();
    m_stagedLabel = new QLabel("Staged Changes (0)", this);
    m_stagedLabel->setStyleSheet("font-weight: bold;");
    stagedHeader->addWidget(m_stagedLabel);
    stagedHeader->addStretch();
    m_unstageAllButton = new QPushButton("-", this);
    m_unstageAllButton->setFixedSize(20, 20);
    m_unstageAllButton->setToolTip("Unstage All");
    stagedHeader->addWidget(m_unstageAllButton);
    stagedLayout->addLayout(stagedHeader);

    m_stagedTree = new QTreeWidget(this);
    m_stagedTree->setHeaderHidden(true);
    m_stagedTree->setRootIsDecorated(false);
    m_stagedTree->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_stagedTree->setContextMenuPolicy(Qt::CustomContextMenu);
    stagedLayout->addWidget(m_stagedTree);
    m_splitter->addWidget(m_stagedWidget);

    // Unstaged changes section
    m_unstagedWidget = new QWidget(this);
    QVBoxLayout* unstagedLayout = new QVBoxLayout(m_unstagedWidget);
    unstagedLayout->setContentsMargins(4, 4, 4, 4);
    unstagedLayout->setSpacing(2);

    QHBoxLayout* unstagedHeader = new QHBoxLayout();
    m_unstagedLabel = new QLabel("Changes (0)", this);
    m_unstagedLabel->setStyleSheet("font-weight: bold;");
    unstagedHeader->addWidget(m_unstagedLabel);
    unstagedHeader->addStretch();
    m_stageAllButton = new QPushButton("+", this);
    m_stageAllButton->setFixedSize(20, 20);
    m_stageAllButton->setToolTip("Stage All");
    unstagedHeader->addWidget(m_stageAllButton);
    m_discardAllButton = new QPushButton("↻", this);
    m_discardAllButton->setFixedSize(20, 20);
    m_discardAllButton->setToolTip("Discard All Changes");
    unstagedHeader->addWidget(m_discardAllButton);
    unstagedLayout->addLayout(unstagedHeader);

    m_unstagedTree = new QTreeWidget(this);
    m_unstagedTree->setHeaderHidden(true);
    m_unstagedTree->setRootIsDecorated(false);
    m_unstagedTree->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_unstagedTree->setContextMenuPolicy(Qt::CustomContextMenu);
    unstagedLayout->addWidget(m_unstagedTree);
    m_splitter->addWidget(m_unstagedWidget);

    // Commit section
    m_commitWidget = new QWidget(this);
    QVBoxLayout* commitLayout = new QVBoxLayout(m_commitWidget);
    commitLayout->setContentsMargins(4, 4, 4, 4);
    commitLayout->setSpacing(4);

    m_commitMessage = new QTextEdit(this);
    m_commitMessage->setPlaceholderText("Commit message...");
    m_commitMessage->setMaximumHeight(80);
    commitLayout->addWidget(m_commitMessage);

    QHBoxLayout* commitButtons = new QHBoxLayout();
    m_commitButton = new QPushButton("Commit", this);
    m_commitButton->setIcon(style()->standardIcon(QStyle::SP_DialogApplyButton));
    m_amendButton = new QPushButton("Amend", this);
    m_amendButton->setToolTip("Amend last commit");
    commitButtons->addWidget(m_commitButton);
    commitButtons->addWidget(m_amendButton);
    commitButtons->addStretch();
    commitLayout->addLayout(commitButtons);

    m_splitter->addWidget(m_commitWidget);

    m_splitter->setStretchFactor(0, 2);
    m_splitter->setStretchFactor(1, 3);
    m_splitter->setStretchFactor(2, 1);

    m_mainLayout->addWidget(m_splitter, 1);
}

void VersionControlWidget::setupToolbar()
{
    // Branch combo
    m_branchCombo = new QComboBox(this);
    m_branchCombo->setMinimumWidth(120);
    m_branchCombo->setToolTip("Current Branch");
    m_toolbar->addWidget(m_branchCombo);

    // New branch button
    QAction* newBranchAction = m_toolbar->addAction(
        style()->standardIcon(QStyle::SP_FileDialogNewFolder), "New Branch");
    connect(newBranchAction, &QAction::triggered, this, &VersionControlWidget::onCreateBranch);

    m_toolbar->addSeparator();

    // Refresh button
    m_refreshButton = new QPushButton(this);
    m_refreshButton->setIcon(style()->standardIcon(QStyle::SP_BrowserReload));
    m_refreshButton->setToolTip("Refresh");
    m_toolbar->addWidget(m_refreshButton);

    m_toolbar->addSeparator();

    // Fetch button
    m_fetchButton = new QPushButton("Fetch", this);
    m_fetchButton->setIcon(style()->standardIcon(QStyle::SP_ArrowDown));
    m_toolbar->addWidget(m_fetchButton);

    // Pull button
    m_pullButton = new QPushButton("Pull", this);
    m_pullButton->setIcon(style()->standardIcon(QStyle::SP_ArrowDown));
    m_toolbar->addWidget(m_pullButton);

    // Push button
    m_pushButton = new QPushButton("Push", this);
    m_pushButton->setIcon(style()->standardIcon(QStyle::SP_ArrowUp));
    m_toolbar->addWidget(m_pushButton);
}

void VersionControlWidget::setupConnections()
{
    connect(m_refreshButton, &QPushButton::clicked, this, &VersionControlWidget::onRefresh);
    connect(m_pushButton, &QPushButton::clicked, this, &VersionControlWidget::onPush);
    connect(m_pullButton, &QPushButton::clicked, this, &VersionControlWidget::onPull);
    connect(m_fetchButton, &QPushButton::clicked, this, &VersionControlWidget::onFetch);
    connect(m_branchCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &VersionControlWidget::onBranchChanged);

    connect(m_stageAllButton, &QPushButton::clicked, this, &VersionControlWidget::onStageAll);
    connect(m_unstageAllButton, &QPushButton::clicked, this, &VersionControlWidget::onUnstageAll);
    connect(m_commitButton, &QPushButton::clicked, this, &VersionControlWidget::onCommit);
    connect(m_amendButton, &QPushButton::clicked, this, &VersionControlWidget::onAmend);

    connect(m_stagedTree, &QTreeWidget::itemDoubleClicked, this, &VersionControlWidget::onFileDoubleClicked);
    connect(m_unstagedTree, &QTreeWidget::itemDoubleClicked, this, &VersionControlWidget::onFileDoubleClicked);
    connect(m_stagedTree, &QTreeWidget::customContextMenuRequested, this, &VersionControlWidget::onContextMenu);
    connect(m_unstagedTree, &QTreeWidget::customContextMenuRequested, this, &VersionControlWidget::onContextMenu);

    connect(m_discardAllButton, &QPushButton::clicked, this, [this]() {
        if (QMessageBox::question(this, "Discard Changes",
                "Are you sure you want to discard all changes?") == QMessageBox::Yes) {
            runGitCommand({"checkout", "--", "."});
            refresh();
        }
    });
}

// ==================== Repository Management ====================
void VersionControlWidget::setRepositoryPath(const QString& path)
{
    LOG_INFO(QString("Setting repository path: %1").arg(path));
    m_repoPath = path;

    // Check if it's a git repository
    QDir gitDir(path + "/.git");
    m_isRepo = gitDir.exists();

    if (m_isRepo) {
        // Watch .git directory for changes
        m_watcher->addPath(path + "/.git");
        m_watcher->addPath(path);
        
        refresh();
        emit repositoryChanged(path);
    } else {
        LOG_WARN("Path is not a git repository");
    }
}

QString VersionControlWidget::getRepositoryPath() const
{
    return m_repoPath;
}

bool VersionControlWidget::isGitRepository() const
{
    return m_isRepo;
}

void VersionControlWidget::initRepository()
{
    if (m_repoPath.isEmpty()) return;

    LOG_INFO("Initializing new git repository...");
    bool ok;
    QString output = runGitCommand({"init"}, &ok);

    if (ok) {
        m_isRepo = true;
        refresh();
        emit repositoryChanged(m_repoPath);
    } else {
        showError("init", output);
    }
}

void VersionControlWidget::cloneRepository(const QString& url, const QString& path)
{
    LOG_INFO(QString("Cloning repository: %1 to %2").arg(url, path));
    
    QProcess* cloneProcess = new QProcess(this);
    cloneProcess->setWorkingDirectory(QFileInfo(path).absolutePath());
    
    connect(cloneProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, cloneProcess, path](int exitCode, QProcess::ExitStatus) {
        if (exitCode == 0) {
            setRepositoryPath(path);
        } else {
            showError("clone", cloneProcess->readAllStandardError());
        }
        cloneProcess->deleteLater();
    });
    
    cloneProcess->start("git", {"clone", url, path});
}

// ==================== Status ====================
void VersionControlWidget::refresh()
{
    if (!m_isRepo) return;

    LOG_DEBUG("Refreshing repository status...");
    updateStatus();
    updateBranches();
    populateChangesTree();
    emit statusChanged();
}

void VersionControlWidget::updateStatus()
{
    m_changes.clear();

    bool ok;
    QString output = runGitCommand({"status", "--porcelain", "-uall"}, &ok);

    if (!ok) return;

    QStringList lines = output.split('\n', Qt::SkipEmptyParts);
    for (const QString& line : lines) {
        if (line.length() < 3) continue;

        FileChange change;
        QString indexStatus = line.mid(0, 1);
        QString workStatus = line.mid(1, 1);
        QString path = line.mid(3);

        // Handle renames
        if (path.contains(" -> ")) {
            QStringList parts = path.split(" -> ");
            change.oldPath = parts[0];
            change.path = parts[1];
        } else {
            change.path = path;
        }

        // Determine if staged or unstaged
        if (indexStatus != " " && indexStatus != "?") {
            // Staged change
            change.status = parseStatusCode(indexStatus);
            change.staged = true;
            m_changes.append(change);
        }

        if (workStatus != " ") {
            // Unstaged change
            FileChange unstagedChange = change;
            unstagedChange.status = parseStatusCode(workStatus);
            unstagedChange.staged = false;
            m_changes.append(unstagedChange);
        }
    }
}

void VersionControlWidget::updateBranches()
{
    m_branches.clear();
    m_branchCombo->blockSignals(true);
    m_branchCombo->clear();

    bool ok;
    QString output = runGitCommand({"branch", "-a", "-vv"}, &ok);

    if (!ok) {
        m_branchCombo->blockSignals(false);
        return;
    }

    QStringList lines = output.split('\n', Qt::SkipEmptyParts);
    int currentIndex = -1;

    for (const QString& line : lines) {
        GitBranch branch;
        branch.isCurrent = line.startsWith('*');
        
        QString trimmed = line.mid(2).trimmed();
        
        // Parse branch name
        int spaceIdx = trimmed.indexOf(' ');
        if (spaceIdx > 0) {
            branch.name = trimmed.left(spaceIdx);
        } else {
            branch.name = trimmed;
        }

        branch.isRemote = branch.name.startsWith("remotes/");
        if (branch.isRemote) {
            branch.name = branch.name.mid(8);  // Remove "remotes/"
        }

        // Parse ahead/behind if present
        QRegularExpression trackRegex(R"(\[([^\]]+)\])");
        QRegularExpressionMatch match = trackRegex.match(trimmed);
        if (match.hasMatch()) {
            QString tracking = match.captured(1);
            if (tracking.contains("ahead")) {
                QRegularExpression aheadRegex(R"(ahead (\d+))");
                QRegularExpressionMatch aheadMatch = aheadRegex.match(tracking);
                if (aheadMatch.hasMatch()) {
                    branch.ahead = aheadMatch.captured(1).toInt();
                }
            }
            if (tracking.contains("behind")) {
                QRegularExpression behindRegex(R"(behind (\d+))");
                QRegularExpressionMatch behindMatch = behindRegex.match(tracking);
                if (behindMatch.hasMatch()) {
                    branch.behind = behindMatch.captured(1).toInt();
                }
            }
        }

        m_branches.append(branch);

        // Add to combo
        QString displayName = branch.name;
        if (branch.ahead > 0 || branch.behind > 0) {
            displayName += QString(" [↑%1 ↓%2]").arg(branch.ahead).arg(branch.behind);
        }
        m_branchCombo->addItem(displayName, branch.name);

        if (branch.isCurrent) {
            currentIndex = m_branchCombo->count() - 1;
            m_currentBranch = branch.name;
        }
    }

    if (currentIndex >= 0) {
        m_branchCombo->setCurrentIndex(currentIndex);
    }

    m_branchCombo->blockSignals(false);
}

void VersionControlWidget::populateChangesTree()
{
    m_stagedTree->clear();
    m_unstagedTree->clear();

    int stagedCount = 0;
    int unstagedCount = 0;

    for (const FileChange& change : m_changes) {
        QTreeWidgetItem* item = new QTreeWidgetItem();
        QString filename = QFileInfo(change.path).fileName();
        item->setText(0, QString("%1  %2").arg(statusToString(change.status), filename));
        item->setIcon(0, statusIcon(change.status));
        item->setData(0, Qt::UserRole, change.path);
        item->setData(0, Qt::UserRole + 1, change.staged);
        item->setToolTip(0, change.path);

        if (change.staged) {
            m_stagedTree->addTopLevelItem(item);
            stagedCount++;
        } else {
            m_unstagedTree->addTopLevelItem(item);
            unstagedCount++;
        }
    }

    m_stagedLabel->setText(QString("Staged Changes (%1)").arg(stagedCount));
    m_unstagedLabel->setText(QString("Changes (%1)").arg(unstagedCount));

    m_commitButton->setEnabled(stagedCount > 0);
}

QList<VersionControlWidget::FileChange> VersionControlWidget::getChanges() const
{
    return m_changes;
}

QList<VersionControlWidget::FileChange> VersionControlWidget::getStagedChanges() const
{
    QList<FileChange> staged;
    for (const FileChange& change : m_changes) {
        if (change.staged) staged.append(change);
    }
    return staged;
}

QList<VersionControlWidget::FileChange> VersionControlWidget::getUnstagedChanges() const
{
    QList<FileChange> unstaged;
    for (const FileChange& change : m_changes) {
        if (!change.staged) unstaged.append(change);
    }
    return unstaged;
}

// ==================== Staging ====================
void VersionControlWidget::stageFile(const QString& path)
{
    LOG_INFO(QString("Staging file: %1").arg(path));
    bool ok;
    QString output = runGitCommand({"add", path}, &ok);
    if (!ok) {
        showError("stage", output);
    }
    refresh();
}

void VersionControlWidget::unstageFile(const QString& path)
{
    LOG_INFO(QString("Unstaging file: %1").arg(path));
    bool ok;
    QString output = runGitCommand({"reset", "HEAD", path}, &ok);
    if (!ok) {
        showError("unstage", output);
    }
    refresh();
}

void VersionControlWidget::stageAll()
{
    LOG_INFO("Staging all changes...");
    bool ok;
    QString output = runGitCommand({"add", "-A"}, &ok);
    if (!ok) {
        showError("stage all", output);
    }
    refresh();
}

void VersionControlWidget::unstageAll()
{
    LOG_INFO("Unstaging all changes...");
    bool ok;
    QString output = runGitCommand({"reset", "HEAD"}, &ok);
    if (!ok) {
        showError("unstage all", output);
    }
    refresh();
}

void VersionControlWidget::discardChanges(const QString& path)
{
    LOG_INFO(QString("Discarding changes: %1").arg(path));
    bool ok;
    QString output = runGitCommand({"checkout", "--", path}, &ok);
    if (!ok) {
        showError("discard", output);
    }
    refresh();
}

// ==================== Commits ====================
void VersionControlWidget::commit(const QString& message)
{
    if (message.trimmed().isEmpty()) {
        QMessageBox::warning(this, "Commit", "Please enter a commit message.");
        return;
    }

    LOG_INFO("Creating commit...");
    bool ok;
    QString output = runGitCommand({"commit", "-m", message}, &ok);

    if (ok) {
        // Extract commit hash from output
        QRegularExpression hashRegex(R"(\[[\w/]+ ([a-f0-9]+)\])");
        QRegularExpressionMatch match = hashRegex.match(output);
        QString hash = match.hasMatch() ? match.captured(1) : "";
        
        m_commitMessage->clear();
        refresh();
        emit commitCompleted(hash);
        LOG_INFO(QString("Commit created: %1").arg(hash));
    } else {
        showError("commit", output);
    }
}

void VersionControlWidget::amendCommit(const QString& message)
{
    if (message.trimmed().isEmpty()) {
        QMessageBox::warning(this, "Amend", "Please enter a commit message.");
        return;
    }

    LOG_INFO("Amending commit...");
    bool ok;
    QString output = runGitCommand({"commit", "--amend", "-m", message}, &ok);

    if (ok) {
        m_commitMessage->clear();
        refresh();
        emit commitCompleted("");
    } else {
        showError("amend", output);
    }
}

QList<VersionControlWidget::GitCommit> VersionControlWidget::getLog(int count) const
{
    QList<GitCommit> commits;

    bool ok;
    QString format = "%H|%h|%an|%ae|%ci|%s|%P";
    QString output = runGitCommand({"log", QString("-%1").arg(count), 
                                    QString("--format=%1").arg(format)}, &ok);

    if (!ok) return commits;

    QStringList lines = output.split('\n', Qt::SkipEmptyParts);
    for (const QString& line : lines) {
        QStringList parts = line.split('|');
        if (parts.size() >= 6) {
            GitCommit commit;
            commit.hash = parts[0];
            commit.shortHash = parts[1];
            commit.author = parts[2];
            commit.email = parts[3];
            commit.date = parts[4];
            commit.message = parts[5];
            if (parts.size() > 6) {
                commit.parents = parts[6].split(' ', Qt::SkipEmptyParts);
            }
            commits.append(commit);
        }
    }

    return commits;
}

// ==================== Branches ====================
QList<VersionControlWidget::GitBranch> VersionControlWidget::getBranches() const
{
    return m_branches;
}

QString VersionControlWidget::getCurrentBranch() const
{
    return m_currentBranch;
}

void VersionControlWidget::createBranch(const QString& name)
{
    LOG_INFO(QString("Creating branch: %1").arg(name));
    bool ok;
    QString output = runGitCommand({"checkout", "-b", name}, &ok);

    if (ok) {
        refresh();
        emit branchChanged(name);
    } else {
        showError("create branch", output);
    }
}

void VersionControlWidget::checkoutBranch(const QString& name)
{
    LOG_INFO(QString("Checking out branch: %1").arg(name));
    bool ok;
    QString output = runGitCommand({"checkout", name}, &ok);

    if (ok) {
        refresh();
        emit branchChanged(name);
    } else {
        showError("checkout", output);
    }
}

void VersionControlWidget::deleteBranch(const QString& name)
{
    LOG_INFO(QString("Deleting branch: %1").arg(name));
    bool ok;
    QString output = runGitCommand({"branch", "-d", name}, &ok);

    if (!ok) {
        showError("delete branch", output);
    }
    refresh();
}

void VersionControlWidget::mergeBranch(const QString& name)
{
    LOG_INFO(QString("Merging branch: %1").arg(name));
    bool ok;
    QString output = runGitCommand({"merge", name}, &ok);

    if (!ok) {
        showError("merge", output);
    }
    refresh();
}

// ==================== Remote Operations ====================
void VersionControlWidget::push()
{
    LOG_INFO("Pushing to remote...");
    m_pushButton->setEnabled(false);

    QProcess* pushProcess = new QProcess(this);
    pushProcess->setWorkingDirectory(m_repoPath);

    connect(pushProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, pushProcess](int exitCode, QProcess::ExitStatus) {
        m_pushButton->setEnabled(true);
        bool success = (exitCode == 0);
        
        if (!success) {
            showError("push", pushProcess->readAllStandardError());
        }
        
        refresh();
        emit pushCompleted(success);
        pushProcess->deleteLater();
    });

    pushProcess->start("git", {"push"});
}

void VersionControlWidget::pull()
{
    LOG_INFO("Pulling from remote...");
    m_pullButton->setEnabled(false);

    QProcess* pullProcess = new QProcess(this);
    pullProcess->setWorkingDirectory(m_repoPath);

    connect(pullProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, pullProcess](int exitCode, QProcess::ExitStatus) {
        m_pullButton->setEnabled(true);
        bool success = (exitCode == 0);
        
        if (!success) {
            showError("pull", pullProcess->readAllStandardError());
        }
        
        refresh();
        emit pullCompleted(success);
        pullProcess->deleteLater();
    });

    pullProcess->start("git", {"pull"});
}

void VersionControlWidget::fetch()
{
    LOG_INFO("Fetching from remote...");
    m_fetchButton->setEnabled(false);

    QProcess* fetchProcess = new QProcess(this);
    fetchProcess->setWorkingDirectory(m_repoPath);

    connect(fetchProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, fetchProcess](int exitCode, QProcess::ExitStatus) {
        m_fetchButton->setEnabled(true);
        
        if (exitCode != 0) {
            showError("fetch", fetchProcess->readAllStandardError());
        }
        
        refresh();
        fetchProcess->deleteLater();
    });

    fetchProcess->start("git", {"fetch", "--all"});
}

// ==================== Diff ====================
QString VersionControlWidget::getDiff(const QString& path, bool staged) const
{
    QStringList args = {"diff"};
    if (staged) {
        args << "--cached";
    }
    args << "--" << path;

    bool ok;
    return runGitCommand(args, &ok);
}

// ==================== Slots ====================
void VersionControlWidget::onRefresh()
{
    refresh();
}

void VersionControlWidget::onStageSelected()
{
    QList<QTreeWidgetItem*> selected = m_unstagedTree->selectedItems();
    for (QTreeWidgetItem* item : selected) {
        QString path = item->data(0, Qt::UserRole).toString();
        stageFile(path);
    }
}

void VersionControlWidget::onUnstageSelected()
{
    QList<QTreeWidgetItem*> selected = m_stagedTree->selectedItems();
    for (QTreeWidgetItem* item : selected) {
        QString path = item->data(0, Qt::UserRole).toString();
        unstageFile(path);
    }
}

void VersionControlWidget::onStageAll()
{
    stageAll();
}

void VersionControlWidget::onUnstageAll()
{
    unstageAll();
}

void VersionControlWidget::onCommit()
{
    commit(m_commitMessage->toPlainText());
}

void VersionControlWidget::onAmend()
{
    amendCommit(m_commitMessage->toPlainText());
}

void VersionControlWidget::onPush()
{
    push();
}

void VersionControlWidget::onPull()
{
    pull();
}

void VersionControlWidget::onFetch()
{
    fetch();
}

void VersionControlWidget::onBranchChanged(int index)
{
    if (index < 0) return;
    QString branch = m_branchCombo->itemData(index).toString();
    if (branch != m_currentBranch) {
        checkoutBranch(branch);
    }
}

void VersionControlWidget::onCreateBranch()
{
    bool ok;
    QString name = QInputDialog::getText(this, "Create Branch",
        "Branch name:", QLineEdit::Normal, "", &ok);
    if (ok && !name.isEmpty()) {
        createBranch(name);
    }
}

void VersionControlWidget::onFileDoubleClicked(QTreeWidgetItem* item, int column)
{
    Q_UNUSED(column)
    QString path = item->data(0, Qt::UserRole).toString();
    bool staged = item->data(0, Qt::UserRole + 1).toBool();
    emit fileDiffRequested(path, staged);
}

void VersionControlWidget::onContextMenu(const QPoint& pos)
{
    QTreeWidget* tree = qobject_cast<QTreeWidget*>(sender());
    if (!tree) return;

    QTreeWidgetItem* item = tree->itemAt(pos);
    if (!item) return;

    QString path = item->data(0, Qt::UserRole).toString();
    bool staged = item->data(0, Qt::UserRole + 1).toBool();

    QMenu menu(this);
    
    if (staged) {
        QAction* unstageAction = menu.addAction("Unstage");
        connect(unstageAction, &QAction::triggered, this, [this, path]() {
            unstageFile(path);
        });
    } else {
        QAction* stageAction = menu.addAction("Stage");
        connect(stageAction, &QAction::triggered, this, [this, path]() {
            stageFile(path);
        });

        menu.addSeparator();

        QAction* discardAction = menu.addAction("Discard Changes");
        connect(discardAction, &QAction::triggered, this, [this, path]() {
            if (QMessageBox::question(this, "Discard Changes",
                    QString("Discard changes to %1?").arg(path)) == QMessageBox::Yes) {
                discardChanges(path);
            }
        });
    }

    menu.addSeparator();

    QAction* diffAction = menu.addAction("Show Diff");
    connect(diffAction, &QAction::triggered, this, [this, path, staged]() {
        emit fileDiffRequested(path, staged);
    });

    menu.exec(tree->mapToGlobal(pos));
}

// ==================== Private Methods ====================
QString VersionControlWidget::runGitCommand(const QStringList& args, bool* ok) const
{
    QProcess process;
    process.setWorkingDirectory(m_repoPath);
    process.start("git", args);
    
    if (!process.waitForFinished(30000)) {
        if (ok) *ok = false;
        return "Command timed out";
    }

    if (ok) *ok = (process.exitCode() == 0);
    
    QString output = QString::fromUtf8(process.readAllStandardOutput());
    if (process.exitCode() != 0) {
        output = QString::fromUtf8(process.readAllStandardError());
    }
    
    return output;
}

VersionControlWidget::FileStatus VersionControlWidget::parseStatusCode(const QString& code) const
{
    if (code == "?" || code == "!") return FileStatus::Untracked;
    if (code == "M") return FileStatus::Modified;
    if (code == "A") return FileStatus::Added;
    if (code == "D") return FileStatus::Deleted;
    if (code == "R") return FileStatus::Renamed;
    if (code == "C") return FileStatus::Copied;
    if (code == "U") return FileStatus::Unmerged;
    return FileStatus::Modified;
}

QString VersionControlWidget::statusToString(FileStatus status) const
{
    switch (status) {
        case FileStatus::Untracked: return "U";
        case FileStatus::Modified: return "M";
        case FileStatus::Added: return "A";
        case FileStatus::Deleted: return "D";
        case FileStatus::Renamed: return "R";
        case FileStatus::Copied: return "C";
        case FileStatus::Unmerged: return "!";
        case FileStatus::Ignored: return "I";
        default: return "?";
    }
}

QIcon VersionControlWidget::statusIcon(FileStatus status) const
{
    switch (status) {
        case FileStatus::Modified:
            return style()->standardIcon(QStyle::SP_FileDialogContentsView);
        case FileStatus::Added:
            return style()->standardIcon(QStyle::SP_FileDialogNewFolder);
        case FileStatus::Deleted:
            return style()->standardIcon(QStyle::SP_TrashIcon);
        case FileStatus::Untracked:
            return style()->standardIcon(QStyle::SP_FileIcon);
        default:
            return style()->standardIcon(QStyle::SP_FileIcon);
    }
}

void VersionControlWidget::showError(const QString& operation, const QString& error)
{
    LOG_ERROR(QString("Git %1 failed: %2").arg(operation, error));
    emit operationError(operation, error);
    QMessageBox::critical(this, QString("Git %1 Failed").arg(operation), error);
}
