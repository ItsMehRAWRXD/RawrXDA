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

private:
    void setupUI();

    QString m_filePath;
    QString m_content;
    ActionType m_actionType;
};

// ============ AgenticFileOperations - File Operation Handler ============

class AgenticErrorHandler;

class AgenticFileOperations : public QObject
{
    Q_OBJECT

public:
    explicit AgenticFileOperations(QObject* parent = nullptr, AgenticErrorHandler* errorHandler = nullptr);

    // Public API for file operations with approval workflow
    void createFileWithApproval(const QString& filePath, const QString& content);
    void modifyFileWithApproval(const QString& filePath, const QString& newContent);
    void modifyFileWithApproval(const QString& filePath, const QString& oldContent, const QString& newContent);  // Overload with explicit old content
    void deleteFileWithApproval(const QString& filePath);

    // Undo support
    void undoLastAction();
    bool canUndo() const { return !m_actionHistory.isEmpty(); }
    int getHistorySize() const;
    int getUndoStackSize() const { return getHistorySize(); }  // Alias for test compatibility
    
    // Metrics support (stub for test compatibility)
    struct Metrics { int operations = 0; int errors = 0; };
    const Metrics* getMetrics() const { return &m_metrics; }
    
    // Test mode - skip approval dialogs for automated testing
    void setTestMode(bool enabled) { m_testMode = enabled; }
    bool isTestMode() const { return m_testMode; }

signals:
    void fileCreated(const QString& filePath);
    void fileModified(const QString& filePath);
    void fileDeleted(const QString& filePath);
    void operationUndone(const QString& filePath);
    void operationCancelled(const QString& filePath);

private:
    // Action history tracking
    struct FileAction
    {
        QString filePath;
        AgenticActionDialog::ActionType actionType;
        QString content;
        QString oldContent;
        QDateTime timestamp;
    };

    QList<FileAction> m_actionHistory;
    int m_maxHistory = 50;  // Default, can be overridden by env var
    AgenticErrorHandler* m_errorHandler = nullptr;
    Metrics m_metrics;
    bool m_testMode = false;  // Skip approval dialogs when true
};

#endif // AGENTIC_FILE_OPERATIONS_H
