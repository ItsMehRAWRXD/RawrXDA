// Agentic File Operations Header
// Keep/Undo workflow for autonomously created or modified files

#ifndef AGENTIC_FILE_OPERATIONS_H
#define AGENTIC_FILE_OPERATIONS_H

#include <QDialog>
#include <QPushButton>
#include <QTextEdit>
#include <QObject>
#include <QList>
#include <QDateTime>
#include <QString>

// ============ AgenticActionDialog - File Operation Approval UI ============

class AgenticActionDialog : public QDialog
{
    Q_OBJECT

public:
    enum ActionType
    {
        CREATE_FILE,
        MODIFY_FILE,
        DELETE_FILE
    };

    explicit AgenticActionDialog(const QString& filePath, ActionType type,
                                 const QString& content, QWidget* parent = nullptr);

signals:
    void actionAccepted(const QString& filePath);
    void actionUndone(const QString& filePath);

private:
    void setupUI();
    void applyDarkTheme();

    QString m_filePath;
    QString m_content;
    ActionType m_actionType;

    QTextEdit* m_previewText = nullptr;
    QPushButton* m_keepButton = nullptr;   // Accept & execute action
    QPushButton* m_undoButton = nullptr;   // Undo/reject action
};

// ============ AgenticFileOperations - File Operation Handler ============

class AgenticFileOperations : public QObject
{
    Q_OBJECT

public:
    explicit AgenticFileOperations(QObject* parent = nullptr);

    // Public API for file operations with approval workflow
    void createFileWithApproval(const QString& filePath, const QString& content);
    void modifyFileWithApproval(const QString& filePath, const QString& oldContent, const QString& newContent);
    void deleteFileWithApproval(const QString& filePath);

    // Undo support
    void undoLastAction();
    bool canUndo() const { return !m_actionHistory.isEmpty(); }
    int getUndoStackSize() const { return m_actionHistory.size(); }

signals:
    void fileCreated(const QString& filePath);
    void fileModified(const QString& filePath);
    void fileDeleted(const QString& filePath);
    void operationUndone(const QString& filePath);
    void approvalRequired(const QString& filePath, const QString& content);

private:
    // Action history tracking
    struct FileAction
    {
        QString filePath;
        AgenticActionDialog::ActionType actionType;
        QString content;        // Content created or new content for modify
        QString oldContent;     // Old content for modify or undo restore
        QDateTime timestamp;
    };

    QList<FileAction> m_actionHistory;
    static constexpr int MAX_HISTORY = 50;  // Default 50-item undo stack
    
    // Configuration
    int m_maxHistorySize;
    
    // Metrics tracking
    std::shared_ptr<Metrics> m_metrics;
    std::unique_ptr<Metrics::Counter> m_keepCounter;
    std::unique_ptr<Metrics::Counter> m_undoCounter;
    std::unique_ptr<Metrics::Counter> m_createCounter;
    std::unique_ptr<Metrics::Counter> m_modifyCounter;
    std::unique_ptr<Metrics::Counter> m_deleteCounter;
    std::unique_ptr<Metrics::Counter> m_errorCounter;
    std::unique_ptr<Metrics::Gauge> m_undoStackGauge;
    std::unique_ptr<Metrics::Histogram> m_operationDurationHistogram;
};

#endif // AGENTIC_FILE_OPERATIONS_H
