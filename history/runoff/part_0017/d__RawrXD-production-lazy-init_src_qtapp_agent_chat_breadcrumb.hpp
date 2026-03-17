#pragma once

#include <QWidget>
#include <QComboBox>
#include <QLabel>
#include <QHBoxLayout>
#include <QPushButton>
#include <QMenu>
#include <QString>
#include <QStringList>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QCache>
#include <QMutex>
#include <QTimer>
#include <QToolTip>
#include <QHelpEvent>
#include <QPoint>
#include <memory>

/**
 * @brief Agent chat mode breadcrumb navigation with mode selector
 * 
 * Features:
 * - Breadcrumb dropdown: Agent / Ask / Plan / Edit / Configure Custom Agents
 * - Smart model selector with Auto feature
 * - Model smart selection based on task type
 * - Integration with available models (local, cloud, Ollama, HuggingFace, etc.)
 */
class AgentChatBreadcrumb : public QWidget {
    Q_OBJECT

public:
    enum AgentMode {
        Ask = 0,           // Simple Q&A
        Plan = 1,          // Multi-step planning
        Edit = 2,          // Code editing
        Configure = 3      // Custom agent configuration
    };

    enum ModelType {
        Auto = -1,
        Local = 0,
        Ollama = 1,
        Claude = 2,
        GPT = 3,
        Copilot = 4,
        HuggingFace = 5,
        Custom = 6
    };

    struct ModelInfo {
        QString name;
        QString displayName;
        ModelType type;
        QString endpoint;
        bool isLarge;  // For Auto selection - true if model is heavy (e.g., agentic)
        bool isContextModel;  // true if good for context summarization
        int maxContextTokens;
        QString description;
        // Additional metadata for tooltips
        QString parameterCount;  // e.g., "7B", "13B", "70B"
        QString quantization;    // e.g., "Q4_K_M", "Q5_K_M"
        QString capabilities;    // e.g., "Code, Reasoning, Chat"
        QString version;         // e.g., "v2.1"
        qint64 lastUpdated;      // Timestamp for cache invalidation
    };

    explicit AgentChatBreadcrumb(QWidget* parent = nullptr);
    
    void initialize();
    void addModel(const ModelInfo& model);
    void addModels(const QList<ModelInfo>& models);
    void setSelectedMode(AgentMode mode);
    void setSelectedModel(const QString& modelName);
    QString getSelectedModel() const;
    AgentMode getSelectedMode() const;
    
    // Register available models
    void registerLocalModel(const QString& name, const QString& endpoint);
    void registerOllamaModel(const QString& name);
    void registerCloudModel(const QString& provider, const QString& modelId);
    void loadModelsFromConfiguration();
    void fetchOllamaModels(const QString& endpoint = "http://localhost:11434");
    
    // Performance enhanced methods
    void fetchOllamaModelsAsync(const QString& endpoint = "http://localhost:11434");
    void refreshModelCache();
    bool isModelCacheValid() const;

    // Accessors for external widgets to get model metadata and tooltip HTML
    ModelInfo getModelInfo(const QString& modelName) const;
    QString tooltipForModel(const QString& modelName) const;
    
signals:
    void agentModeChanged(AgentMode mode);
    void modelSelected(const QString& modelName);
    void modelTypeChanged(ModelType type);
    void configureCustomAgents();  // Signal when user selects Configure option
    void ollamaModelsUpdated();    // Signal when Ollama models are fetched
    
private slots:
    void onModeChanged(int index);
    void onModelChanged(int index);
    void onConfigureClicked();
    void populateModelDropdown();
    void updateAutoModelSuggestion();
    void onOllamaModelsRetrieved();
    void onAsyncOllamaFetchFinished();
    
private:
    void setupUI();
    void applyDarkTheme();
    QString buildTooltipText(const ModelInfo& info) const;
    void installEventFilterForTooltip();
    bool eventFilter(QObject* obj, QEvent* event) override;
    QString getAutoModelForMode(AgentMode mode) const;
    QString getSmartModelForTask(const QString& taskHint) const;
    
    // Performance enhanced helper functions
    QString extractParameterCountFromName(const QString& modelName) const;
    QString extractQuantizationFromName(const QString& modelName) const;
    QString extractCapabilitiesFromName(const QString& modelName) const;
    QString extractDetailedMetadataFromAPI(const QString& modelName);
    
    // Caching mechanisms
    void cacheModelMetadata(const QString& modelName, const ModelInfo& metadata);
    ModelInfo getCachedModelMetadata(const QString& modelName) const;
    void clearModelCache();
    
    // Async loading
    void startAsyncMetadataFetch(const QStringList& modelNames);
    void processAsyncMetadataResponse(const QString& modelName, const QJsonObject& metadata);
    
    // UI Components
    QHBoxLayout* m_layout = nullptr;
    QLabel* m_agentLabel = nullptr;
    QComboBox* m_modeSelector = nullptr;  // Agent / Ask / Plan / Edit / Configure
    QLabel* m_separator1 = nullptr;
    QLabel* m_modelLabel = nullptr;
    QComboBox* m_modelSelector = nullptr;  // Model dropdown with Auto at top
    QPushButton* m_configureButton = nullptr;
    
    // State
    AgentMode m_currentMode = Ask;
    QString m_selectedModel;
    ModelType m_currentModelType = Auto;
    QTimer* m_tooltipTimer = nullptr;
    QPoint m_lastMousePos;
    
    // Available models registry
    QList<ModelInfo> m_availableModels;
    QMap<QString, ModelInfo> m_modelMap;
    QMap<ModelType, QStringList> m_modelsByType;
    
    // Ollama API
    std::unique_ptr<QNetworkAccessManager> m_networkManager;
    QString m_ollamaEndpoint;
    
    // Performance enhancements
    mutable QCache<QString, ModelInfo> m_metadataCache;
    mutable QMutex m_cacheMutex;
    QTimer* m_cacheRefreshTimer = nullptr;
    qint64 m_lastCacheUpdate = 0;
    static const int CACHE_TIMEOUT_MS = 300000; // 5 minutes
    bool m_asyncFetchInProgress = false;
};

