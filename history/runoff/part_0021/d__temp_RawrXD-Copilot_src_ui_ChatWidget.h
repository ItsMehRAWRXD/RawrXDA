#ifndef CHATWIDGET_H
#define CHATWIDGET_H

#include <QWidget>
#include <QVBoxLayout>
#include <QTextEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QListWidget>
#include <QJsonObject>
#include <QTimer>

class ChatWidget : public QWidget
{
    Q_OBJECT

public:
    struct ChatMessage {
        enum Role {
            User,
            Assistant,
            System
        };
        
        Role role;
        QString content;
        qint64 timestamp;
        QString id;
        QJsonObject metadata;
    };

    explicit ChatWidget(QWidget *parent = nullptr);
    ~ChatWidget();

    // Chat management
    void addMessage(const ChatMessage& message);
    void clearConversation();
    QList<ChatMessage> getConversationHistory() const;
    
    // UI control
    void setTypingIndicator(bool visible);
    void setEnabled(bool enabled);

public slots:
    void sendMessage();
    void onUserInput();

signals:
    void messageSent(const QString& message);
    void messageReceived(const ChatMessage& message);
    void typingStarted();
    void typingStopped();

private slots:
    void onTypingTimeout();

private:
    QTextEdit* m_chatDisplay;
    QLineEdit* m_inputField;
    QPushButton* m_sendButton;
    QPushButton* m_clearButton;
    
    QList<ChatMessage> m_conversation;
    QTimer* m_typingTimer;
    
    void setupUI();
    void updateChatDisplay();
    QString formatMessage(const ChatMessage& message);
    QString getMessageStyle(ChatMessage::Role role);
};

#endif // CHATWIDGET_H
