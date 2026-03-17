// Agentic File Operations - Keep/Undo System
// This implements the file creation/modification approval dialog for agentic operations

#pragma once

#include <QDialog>
#include <QString>
#include <QTextEdit>
#include <QPushButton>

class AgenticActionDialog : public QDialog
{
    Q_OBJECT

public:
    enum ActionType {
        CREATE_FILE,
        MODIFY_FILE,
        DELETE_FILE
    };

    explicit AgenticActionDialog(const QString& filePath, ActionType type, 
                                 const QString& content, QWidget* parent = nullptr);

    QString getFilePath() const { return m_filePath; }
    QString getContent() const { return m_content; }
    QString getOldContent() const { return m_oldContent; }
    ActionType getActionType() const { return m_actionType; }

signals:
    void actionAccepted(const QString& filePath);
    void actionUndone(const QString& filePath);

private:
    void setupUI();
    void applyDarkTheme();

    QString m_filePath;
    QString m_content;
    QString m_oldContent;
    ActionType m_actionType;

    QTextEdit *m_previewText = nullptr;
    QPushButton *m_keepButton = nullptr;
    QPushButton *m_undoButton = nullptr;
};

// Agentic File Operations Handler
class AgenticFileOperations : public QObject
{
    Q_OBJECT

public:
    explicit AgenticFileOperations(QObject* parent = nullptr);

    // Main operations with approval dialog
    void createFileWithApproval(const QString& filePath, const QString& content);
    void modifyFileWithApproval(const QString& filePath, const QString& oldContent, const QString& newContent);
    void deleteFileWithApproval(const QString& filePath);

    // Undo support
    void undoLastAction();
    bool canUndo() const { return !m_actionHistory.isEmpty(); }

signals:
    void fileCreated(const QString& filePath);
    void fileModified(const QString& filePath);
    void fileDeleted(const QString& filePath);
    void operationUndone(const QString& filePath);
    void approvalRequired(const QString& filePath, const QString& actionType, const QString& preview);

private slots:
    void onActionConfirmed(const QString& filePath);
    void onActionUndone(const QString& filePath);

private:
    struct FileAction {
        QString filePath;
        AgenticActionDialog::ActionType actionType;
        QString content;       // For CREATE/MODIFY
        QString oldContent;    // For MODIFY (to restore on undo)
        QDateTime timestamp;
    };

    QList<FileAction> m_actionHistory;
    static constexpr int MAX_HISTORY = 50;  // Keep last 50 actions for undo
};
