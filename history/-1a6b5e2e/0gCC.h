#pragma once

#include <QObject>
#include <QString>
#include <QByteArray>
#include <QTextEdit>

/**
 * @brief StreamingTokenManager - Manages real-time token streaming with thinking UI
 * 
 * Ported from chat_stream_ui.asm MASM implementation.
 * Implements call session management, thinking buffer, and stream accumulation.
 */
class StreamingTokenManager : public QObject {
    Q_OBJECT

public:
    explicit StreamingTokenManager(QObject* parent = nullptr);
    ~StreamingTokenManager();

    // Initialize with UI components
    void initialize(QWidget* chatPanel, QTextEdit* richEdit);

    // Call session management (from MASM ChatStreamManager_StartCall/FinishCall)
    void startCall(const QString& modelName);
    void finishCall(bool success);

    // Token handling (from MASM ChatStreamManager_OnToken)
    void onToken(const QString& token);
    void onToken(const char* token, int tokenLen);

    // Thinking UI control (from MASM ChatStreamManager_ShowThinking/HideThinking)
    void showThinking(const QString& text = "Thinking...");
    void hideThinking();
    void setThinkingEnabled(bool enabled);
    bool isThinkingEnabled() const { return m_thinkingEnabled; }

    // Buffer management
    void flushBuffer();
    void finishStream();

    // Get accumulated response from current call
    QString getCurrentCallBuffer() const;

signals:
    void streamToken(const QString& token);
    void streamFinished();
    void thinkingVisibilityChanged(bool visible);

private:
    // UI components
    QWidget* m_chatPanel{nullptr};
    QTextEdit* m_richEdit{nullptr};
    QTextEdit* m_thinkingBox{nullptr};

    // Streaming buffers (from MASM)
    static constexpr int STREAM_BUFFER_SIZE = 8192;
    static constexpr int CALL_BUFFER_SIZE = 65536;

    QByteArray m_streamBuffer;
    int m_streamPos{0};
    bool m_isStreaming{false};

    // Thinking UI state
    bool m_thinkingVisible{false};
    bool m_thinkingEnabled{true};
    QByteArray m_thinkingBuffer;
    int m_thinkingPos{0};

    // Call session state (from MASM currentCallBuffer/currentCallActive)
    QByteArray m_currentCallBuffer;
    int m_currentCallPos{0};
    bool m_currentCallActive{false};

    // Helper methods
    void appendToStreamBuffer(const QString& token);
    void appendToCallBuffer(const QString& token);
    void appendToThinkingBuffer(const QString& token);
    void createThinkingBox();
    void destroyThinkingBox();
    void applyMessageStyle(QTextEdit* target);
};
