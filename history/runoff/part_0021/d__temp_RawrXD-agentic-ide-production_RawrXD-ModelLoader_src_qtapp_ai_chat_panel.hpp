#pragma once

#include <QWidget>

QT_BEGIN_NAMESPACE
class QTextEdit;
class QLineEdit;
class QPushButton;
class QLabel;
class QCheckBox;
QT_END_NAMESPACE

class AgentChatBreadcrumb;
class AgenticExecutor;

class AIChatPanel : public QWidget {
    Q_OBJECT
public:
    explicit AIChatPanel(QWidget* parent = nullptr);
    ~AIChatPanel();

    // Two-phase init kept for compatibility
    void initialize();

    // Chat display helpers
    void addUserMessage(const QString& msg);
    void addAssistantMessage(const QString& msg, bool streaming = false);
    void addSystemMessage(const QString& msg);
    void startStreaming();
    void updateStreamingMessage(const QString& partial);
    void finishStreaming();
    void clearChat();
    void setInputEnabled(bool enabled);
    void setPlaceholderText(const QString& text);
    void setContext(const QString& code, const QString& filePath);
    void setAgenticExecutor(AgenticExecutor* executor);
    void setModelStatus(const QString& modelName, bool ready);
    AgentChatBreadcrumb* getBreadcrumb() const { return m_breadcrumb; }

signals:
    void messageSubmitted(const QString& message);
    void quickActionTriggered(const QString& action, const QString& context);
    void agentModeChanged(int mode);
    void modelSelected(const QString& modelName);
    void maxModeChanged(bool enabled);

private slots:
    void onSendClicked();
    void onQuickActionClicked(const QString& action);
    void onMaxModeToggled(bool enabled);

private:
    void setupUI();
    void appendMessage(const QString& message, bool isUser, bool isStreaming = false);

    QTextEdit* m_chatDisplay{nullptr};
    QLineEdit* m_messageInput{nullptr};
    QPushButton* m_sendButton{nullptr};
    QWidget* m_quickActions{nullptr};
    QCheckBox* m_maxMode{nullptr};
    QLabel* m_modelStatusLabel{nullptr};
    AgentChatBreadcrumb* m_breadcrumb{nullptr};
    AgenticExecutor* m_agenticExecutor{nullptr};

    QString m_contextCode;
    QString m_contextFilePath;
    bool m_inputEnabled{true};
    bool m_streamingActive{false};
    int m_streamingBlock{-1};
};
