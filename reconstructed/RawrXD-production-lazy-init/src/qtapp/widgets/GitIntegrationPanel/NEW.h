// GitIntegrationPanel.h - Complete Git Integration for RawrXD IDE
// Phase 4 - Full implementation with zero stubs

#ifndef GITINTEGRATIONPANEL_H
#define GITINTEGRATIONPANEL_H

#include <QDockWidget>
#include <QString>
#include <QStringList>
#include <QMap>
#include <QVector>
#include <functional>

class QTreeWidget;
class QListWidget;
class QTextEdit;
class QComboBox;
class QPushButton;
class QLabel;
class QProcess;
class QTimer;
class QTabWidget;

// ============================================================================
// Data Structures
// ============================================================================

struct GitFileInfo {
    QString path;
    QString status;
};

struct GitCommitInfo {
    QString hash;
    QString author;
    QString date;
    QString message;
};

// ============================================================================
// Main Class
// ============================================================================

class GitIntegrationPanel : public QDockWidget {
    Q_OBJECT

public:
    explicit GitIntegrationPanel(QWidget* parent = nullptr);
    ~GitIntegrationPanel() override;
    
    // Repository management
    void setRepository(const QString& path);
    void refreshStatus();
    void refreshAll();
    
    // File operations
    void stageFile(const QString& filePath);
    void unstageFile(const QString& filePath);
    void discardChanges(const QString& filePath);
    
    // Commit operations
    void commit(const QString& message);
    
    // Remote operations
    void pull();
    void push();
    void fetch();
    
    // Branch operations
    void createBranch(const QString& branchName);
    void switchBranch(const QString& branchName);
    void deleteBranch(const QString& branchName);
    void mergeBranch(const QString& branchName);
    
    // Query functions
    QString getCurrentBranch() const;
    QStringList getChangedFiles() const;
    bool hasUncommittedChanges() const;

signals:
    void statusChanged();
    void branchChanged(const QString& branch);
    void fileOpenRequested(const QString& filePath);
    void commitCompleted();
    void pullCompleted();
    void pushCompleted();

private slots:
    // UI interaction slots
    void onOpenRepository();
    void onBranchChanged(int index);
    void onCreateBranch();
    void onDeleteBranch();
    void onCommit();
    void onPull();
    void onPush();
    void onFetch();
    
    // Context menu slots
    void onStagedContextMenu(const QPoint& pos);
    void onUnstagedContextMenu(const QPoint& pos);
    void onUntrackedContextMenu(const QPoint& pos);
    void onBranchContextMenu(const QPoint& pos);
    void onRemoteBranchContextMenu(const QPoint& pos);
    void onRemoteContextMenu(const QPoint& pos);
    
    // Remote operations
    void onAddRemote();
    void onRemoveRemote();
    
    // History/Diff slots
    void onHistorySelectionChanged(int row);
    void onDiffFileChanged(int index);

private:
    // UI setup
    void setupUI();
    QWidget* createStatusTab();
    QWidget* createHistoryTab();
    QWidget* createDiffTab();
    QWidget* createBranchesTab();
    QWidget* createRemoteTab();
    void connectSignals();
    
    // Git command execution
    void executeGitCommand(
        const QStringList& args,
        std::function<void(const QString&)> onSuccess = nullptr,
        std::function<void(const QString&)> onError = nullptr
    );
    
    // Parsing functions
    void parseStatus(const QString& output);
    void parseBranches(const QString& output);
    void parseHistory(const QString& output);
    void parseRemotes(const QString& output);
    QString getStatusString(QChar statusCode) const;
    
    // UI update functions
    void updateStatusTrees();
    void refreshHistory();
    void refreshDiff();
    void refreshRemotes();
    void highlightDiff();
    void clearAll();
    
    // UI Components
    QLabel* m_repoPathLabel;
    QComboBox* m_branchCombo;
    QTabWidget* m_tabWidget;
    
    // Status tab
    QTreeWidget* m_stagedTree;
    QTreeWidget* m_unstagedTree;
    QTreeWidget* m_untrackedTree;
    
    // History tab
    QListWidget* m_historyList;
    QTextEdit* m_commitDetailsText;
    
    // Diff tab
    QComboBox* m_diffFileCombo;
    QTextEdit* m_diffText;
    
    // Branches tab
    QListWidget* m_localBranchList;
    QListWidget* m_remoteBranchList;
    
    // Remote tab
    QListWidget* m_remoteList;
    QTextEdit* m_remoteDetailsText;
    
    // Action buttons
    QPushButton* m_pullBtn;
    QPushButton* m_pushBtn;
    QPushButton* m_fetchBtn;
    QPushButton* m_commitBtn;
    
    // Data structures
    QString m_repoPath;
    QString m_currentBranch;
    QVector<GitFileInfo> m_stagedFiles;
    QVector<GitFileInfo> m_unstagedFiles;
    QStringList m_untrackedFiles;
    QVector<GitCommitInfo> m_commits;
    QStringList m_localBranches;
    QStringList m_remoteBranches;
    QMap<QString, QString> m_remotes;
    
    // Git process
    QProcess* m_gitProcess;
    QTimer* m_refreshTimer;
    
    // State
    bool m_autoRefresh;
    bool m_isGitRepository;
};

#endif // GITINTEGRATIONPANEL_H
