#pragma once

// <QWidget> removed (Qt-free build)
// <QTextEdit> removed (Qt-free build)
// <QLineEdit> removed (Qt-free build)
// <QPushButton> removed (Qt-free build)
// <QVBoxLayout> removed (Qt-free build)
// <QLabel> removed (Qt-free build)
// Qt include removed (Qt-free build)
// Qt include removed (Qt-free build)
// <QList> removed (Qt-free build)

class AgentChatBreadcrumb;

/**
 * @brief GitHub Copilot-style AI chat panel
 * 
 * Features:
 * - Chat-style message bubbles
 * - Streaming responses
 * - Code block highlighting
 * - Quick actions (explain, fix, refactor)
 * - Context awareness (selected code)
 */
class AIChatPanel  {
    /* Q_OBJECT */

public:
    struct Message {
        enum Role { User, Assistant, System };
        Role role;
        QString content;
        QString timestamp;
        bool isStreaming = false;
    };

    explicit AIChatPanel(QWidget* parent = nullptr);
    
    /**
     * Two-phase initialization - call after QApplication is ready
     * Creates all Qt widgets and applies theme
     */
    void initialize();
    
    void addUserMessage(const QString& message);
    void addAssistantMessage(const QString& message, bool streaming = false);
    void updateStreamingMessage(const QString& content);
    void finishStreaming();
    void clear();
    
    void setContext(const QString& code, const QString& filePath);

    AgentChatBreadcrumb* getBreadcrumb() const { return m_breadcrumb; }
    
signals:
    void messageSubmitted(const QString& message);
    void quickActionTriggered(const QString& action, const QString& context);
    
private slots:
    void onSendClicked();
    void onQuickActionClicked(const QString& action);
    
private:
    void setupUI();
    void applyDarkTheme();
    QWidget* createMessageBubble(const Message& msg);
    QWidget* createQuickActions();
    void scrollToBottom();
    
    QVBoxLayout* m_messagesLayout;
    QScrollArea* m_scrollArea;
    QWidget* m_messagesContainer;
    QLineEdit* m_inputField;
    QPushButton* m_sendButton;
    QWidget* m_quickActionsWidget;
    
    QList<Message> m_messages;
    QWidget* m_streamingBubble = nullptr;
    QTextEdit* m_streamingText = nullptr;
    
    QString m_contextCode;
    QString m_contextFilePath;
    AgentChatBreadcrumb* m_breadcrumb = nullptr;
};
