#pragma once


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
class AIChatPanel : public void {

public:
    struct Message {
        enum Role { User, Assistant, System };
        Role role;
        std::string content;
        std::string timestamp;
        bool isStreaming = false;
    };

    explicit AIChatPanel(void* parent = nullptr);
    
    /**
     * Two-phase initialization - call after QApplication is ready
     * Creates all Qt widgets and applies theme
     */
    void initialize();
    
    void addUserMessage(const std::string& message);
    void addAssistantMessage(const std::string& message, bool streaming = false);
    void updateStreamingMessage(const std::string& content);
    void finishStreaming();
    void clear();
    
    void setContext(const std::string& code, const std::string& filePath);
    
    void messageSubmitted(const std::string& message);
    void quickActionTriggered(const std::string& action, const std::string& context);
    
private:
    void onSendClicked();
    void onQuickActionClicked(const std::string& action);
    
private:
    void setupUI();
    void applyDarkTheme();
    void* createMessageBubble(const Message& msg);
    void* createQuickActions();
    void scrollToBottom();
    
    QVBoxLayout* m_messagesLayout;
    QScrollArea* m_scrollArea;
    void* m_messagesContainer;
    QLineEdit* m_inputField;
    QPushButton* m_sendButton;
    void* m_quickActionsWidget;
    
    std::vector<Message> m_messages;
    void* m_streamingBubble = nullptr;
    QTextEdit* m_streamingText = nullptr;
    
    std::string m_contextCode;
    std::string m_contextFilePath;
};

