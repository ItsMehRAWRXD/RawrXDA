#ifndef REAL_TIME_EDITOR_INTEGRATION_HPP
#define REAL_TIME_EDITOR_INTEGRATION_HPP

#include <QObject>
#include <QString>
#include <QStringList>
#include <QJsonObject>
#include <QMutex>
#include <QDateTime>
#include <memory>
#include <unordered_map>

class AgenticCopilotBridge;

class EditorSession : public QObject {
    Q_OBJECT

public:
    enum FileType {
        CPP = 0,
        Python = 1,
        Assembly = 2,
        C = 3,
        JavaScript = 4,
        Markdown = 5,
        Unknown = 6
    };

    enum ChangeType {
        ContentChange = 0,
        CursorMove = 1,
        SelectionChange = 2,
        SaveChange = 3
    };

    explicit EditorSession(
        int sessionId,
        const QString& filePath,
        QObject* parent = nullptr);

    ~EditorSession() override;

    QString getFilePath() const;
    QString getContent() const;
    bool loadFile();
    bool saveFile();
    
    void insertText(const QString& text, int position);
    void deleteText(int position, int length);
    void replaceText(int position, int length, const QString& replacement);
    
    void setCursorPosition(int position);
    int getCursorPosition() const;
    
    void setSelection(int start, int end);
    QString getSelectedText() const;
    
    bool undo();
    bool redo();
    void clearHistory();
    
    FileType getFileType() const;
    QJsonObject getStatistics() const;
    
    QString getGhostText() const;
    void setGhostText(const QString& suggestion);

signals:
    void contentChanged(const QString& newContent);
    void cursorMoved(int position);
    void selectionChanged(int start, int end);
    void fileSaved();
    void fileLoaded();
    void ghostTextChanged(const QString& suggestion);

private:
    struct EditHistoryEntry {
        QString content;
        int cursorPos;
        qint64 timestamp;
    };

    int m_sessionId;
    QString m_filePath;
    QString m_content;
    FileType m_fileType;
    mutable QMutex m_mutex;
    
    int m_cursorPosition = 0;
    int m_selectionStart = 0;
    int m_selectionEnd = 0;
    
    QStringList m_undoStack;
    QStringList m_redoStack;
    QString m_ghostText;
    
    QDateTime m_createdTime;
    QDateTime m_lastModifiedTime;
    int m_totalEdits = 0;

    FileType detectFileType(const QString& path) const;
};

class RealTimeEditorIntegration : public QObject {
    Q_OBJECT

public:
    explicit RealTimeEditorIntegration(QObject* parent = nullptr);
    ~RealTimeEditorIntegration() override;

    int openFile(const QString& filePath);
    bool closeFile(int sessionId);
    
    bool loadFileContent(int sessionId);
    bool saveFile(int sessionId);
    
    bool insertCodeAt(int sessionId, const QString& code, int position);
    bool deleteTextAt(int sessionId, int position, int length);
    bool replaceTextAt(int sessionId, int position, int length, const QString& replacement);
    
    QString getFileContent(int sessionId) const;
    QString getSelectedText(int sessionId) const;
    
    bool findAndReplace(const QString& pattern, const QString& replacement, bool regex = false);
    
    void requestCodeCompletion(int sessionId, const QString& context);
    
    int getOpenFileCount() const;
    QJsonObject getEditorStatistics() const;
    
    void setAgenticBridge(AgenticCopilotBridge* bridge);
    
    bool undo(int sessionId);
    bool redo(int sessionId);

signals:
    void fileOpened(int sessionId, const QString& filePath);
    void fileClosed(int sessionId);
    void contentChanged(int sessionId, const QString& newContent);
    void cursorPositionChanged(int sessionId, int position);
    void codeCompletionRequested(int sessionId);
    void codeCompletionReady(int sessionId, const QString& suggestions);

private:
    std::unordered_map<int, std::unique_ptr<EditorSession>> m_sessions;
    mutable QMutex m_sessionsMutex;
    
    int m_nextSessionId = 0;
    int m_maxSessions = 50;
    int m_maxEdits = 100;
    
    AgenticCopilotBridge* m_agenticBridge = nullptr;
    
    bool validateSessionId(int sessionId) const;
    EditorSession* getSession(int sessionId) const;
};

#endif // REAL_TIME_EDITOR_INTEGRATION_HPP
