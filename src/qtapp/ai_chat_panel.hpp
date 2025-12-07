#pragma once

#include <QWidget>
#include <QTextEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QLabel>
#include <QScrollArea>
#include <QFrame>
#include <QList>

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
class AIChatPanel : public QWidget {
    Q_OBJECT

public:
    struct Message {
        enum Role { User, Assistant, System };
        Role role;
        QString content;
        QString timestamp;
        bool isStreaming = false;
    };

    explicit AIChatPanel(QWidget* parent = nullptr);
    
    void addUserMessage(const QString& message);
    void addAssistantMessage(const QString& message, bool streaming = false);
    void updateStreamingMessage(const QString& content);
    void finishStreaming();
    void clear();
    
    void setContext(const QString& code, const QString& filePath);
    
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
};
