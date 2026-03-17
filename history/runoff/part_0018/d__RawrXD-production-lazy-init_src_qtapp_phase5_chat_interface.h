#pragma once

#include <QObject>
#include <QString>
#include <QJsonObject>
#include <QJsonArray>
#include <functional>
#include <memory>

class Phase5ModelRouter;
class QTimer;

/**
 * @brief Phase 5 Model-aware Chat Modes
 */
enum class Phase5ChatMode {
    Standard,       // Normal conversation
    Max,            // Maximum quality responses
    Research,       // Research-oriented responses
    DeepResearch,   // Extended research mode
    Thinking,       // Chain-of-thought reasoning
    CodeAssist,     // Code-focused assistance
    Debug           // Debug mode with verbose output
};

/**
 * @brief Chat session representing a conversation with model(s)
 */
struct ChatSession {
    QString sessionId;
    QString title;
    Phase5ChatMode mode;
    QString selectedModel;
    QString selectedModel2;  // Secondary model for comparison
    bool multiModel;         // Using multiple models?
    QJsonArray messages;     // Conversation history
    QJsonObject settings;    // Session-specific settings
    int totalTokensUsed;
    double totalCostEstimate;
    QDateTime createdAt;
    QDateTime lastUpdatedAt;
};

/**
 * @brief Phase 5 Chat Interface - Full model integration
 * 
 * Integrates with Phase5ModelRouter to provide:
 * - Multiple model support
 * - Specialized inference modes (MAX, Research, Deep Research, Thinking)
 * - Real-time token generation and streaming
 * - Session management and history
 * - Cost tracking and analytics
 * - Model comparison
 */
class Phase5ChatInterface : public QObject {
    Q_OBJECT
    
public:
    explicit Phase5ChatInterface(Phase5ModelRouter* router);
    ~Phase5ChatInterface();
    
    // ===== Session Management =====
    
    /**
     * @brief Create a new chat session
     */
    QString createSession(const QString& title, Phase5ChatMode mode);
    
    /**
     * @brief Load existing session
     */
    bool loadSession(const QString& sessionId);
    
    /**
     * @brief Save current session
     */
    bool saveSession();

    /**
     * @brief Calculate estimated cost for a token count
     */
    double calculateCost(int tokens) const;
    
    /**
     * @brief Get session list
     */
    QStringList getSessionList() const;
    
    /**
     * @brief Delete session
     */
    bool deleteSession(const QString& sessionId);
    
    /**
     * @brief Get current session
     */
    ChatSession getCurrentSession() const;
    
    // ===== Messaging =====
    
    /**
     * @brief Send user message and get response
     */
    void sendMessage(const QString& message);
    
    /**
     * @brief Send message asynchronously
     */
    void sendMessageAsync(const QString& message);
    
    /**
     * @brief Add message to history without sending
     */
    void addMessageToHistory(const QString& role, const QString& content);
    
    /**
     * @brief Get conversation history
     */
    QJsonArray getHistory() const;
    
    /**
     * @brief Clear history
     */
    void clearHistory();
    
    /**
     * @brief Get message at index
     */
    QJsonObject getMessage(int index) const;
    
    // ===== Model Management =====
    
    /**
     * @brief Set primary model for chat
     */
    void setPrimaryModel(const QString& modelName);
    
    /**
     * @brief Set secondary model (for comparison)
     */
    void setSecondaryModel(const QString& modelName);
    
    /**
     * @brief Get available models
     */
    QStringList getAvailableModels() const;
    
    /**
     * @brief Enable/disable secondary model
     */
    void setMultiModelMode(bool enabled);
    
    // ===== Chat Modes =====
    
    /**
     * @brief Set chat mode
     */
    void setChatMode(Phase5ChatMode mode);
    
    /**
     * @brief Get current chat mode
     */
    Phase5ChatMode getChatMode() const;
    
    /**
     * @brief Execute message in MAX mode
     */
    QString executeMax(const QString& prompt);
    
    /**
     * @brief Execute message in Research mode
     */
    QString executeResearch(const QString& prompt);
    
    /**
     * @brief Execute message in Deep Research mode
     */
    QString executeDeepResearch(const QString& prompt);
    
    /**
     * @brief Execute message in Thinking mode
     */
    QString executeThinking(const QString& prompt);
    
    /**
     * @brief Compare models on same prompt
     */
    QJsonObject compareModels(const QString& prompt);
    
    // ===== Configuration =====
    
    /**
     * @brief Set temperature
     */
    void setTemperature(double temp);
    
    /**
     * @brief Set max tokens
     */
    void setMaxTokens(int maxTokens);
    
    /**
     * @brief Set context window
     */
    void setContextWindow(int contextSize);
    
    /**
     * @brief Set system prompt
     */
    void setSystemPrompt(const QString& prompt);
    
    /**
     * @brief Get current settings
     */
    QJsonObject getCurrentSettings() const;
    
    // ===== Analytics =====
    
    /**
     * @brief Get session statistics
     */
    QJsonObject getSessionStats() const;
    
    /**
     * @brief Get token usage
     */
    int getTotalTokensUsed() const;
    
    /**
     * @brief Get cost estimate
     */
    double getEstimatedCost() const;
    
    /**
     * @brief Get average response quality metrics
     */
    QJsonObject getQualityMetrics() const;
    
    // ===== Streaming & Progress =====
    
    /**
     * @brief Enable streaming token generation
     */
    void enableStreaming(bool enable);
    
    /**
     * @brief Set progress callback
     */
    void setProgressCallback(std::function<void(int, int)> callback);
    
    /**
     * @brief Cancel current generation
     */
    void cancelGeneration();
    
    // ===== Export & Sharing =====
    
    /**
     * @brief Export session to markdown
     */
    QString exportToMarkdown() const;
    
    /**
     * @brief Export session to JSON
     */
    QString exportToJson() const;
    
    /**
     * @brief Load session from markdown
     */
    bool importFromMarkdown(const QString& content);

private slots:
    void onRoutingDecision(const QString& modelId, const QString& reason);
    
signals:
    void sessionCreated(const QString& sessionId);
    void sessionLoaded(const QString& sessionId);
    void messageSent(const QString& message);
    void responseReceived(const QString& response, double tokensPerSecond);
    void streamingTokenReceived(const QString& token);
    void progressUpdated(int tokensGenerated, int maxTokens);
    void generationCancelled();
    void modelChanged(const QString& modelName);
    void modeChanged(Phase5ChatMode mode);
    void errorOccurred(const QString& error);
    void costUpdated(double totalCost);
    void qualityMetricsUpdated(const QJsonObject& metrics);

private:
    Phase5ModelRouter* m_router;
    std::unique_ptr<ChatSession> m_currentSession;
    
    // Settings
    double m_temperature;
    int m_maxTokens;
    int m_contextWindow;
    QString m_systemPrompt;
    bool m_streamingEnabled;
    bool m_multiModelMode;
    
    // State tracking
    bool m_isGenerating;
    std::function<void(int, int)> m_progressCallback;
    QTimer* m_generationTimer;
    
    // Session persistence
    QString getSessionPath(const QString& sessionId) const;
    bool loadSessionFromFile(const QString& sessionId);
    bool saveSessionToFile();
};
