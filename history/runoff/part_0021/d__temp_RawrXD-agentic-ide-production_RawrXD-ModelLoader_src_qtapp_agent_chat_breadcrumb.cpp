#include "agent_chat_breadcrumb.hpp"
#include <QVBoxLayout>
#include <QSettings>
#include <QDebug>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QFile>
#include <QNetworkReply>
#include <QEventLoop>
#include <QUrl>
#include <QTimer>
#include <QDateTime>
#include <QThread>
#include <QApplication>
#include <QAbstractItemView>
#include <QMouseEvent>
#include <QRegularExpression>
#include <QToolTip>

AgentChatBreadcrumb::AgentChatBreadcrumb(QWidget* parent)
    : QWidget(parent)
    , m_layout(nullptr)
    , m_currentMode(Ask)
    , m_selectedModel("Auto")
    , m_currentModelType(Auto)
    , m_networkManager(std::make_unique<QNetworkAccessManager>())
    , m_ollamaEndpoint("http://localhost:11434")
    , m_metadataCache(100) // Cache up to 100 models
    , m_cacheRefreshTimer(new QTimer(this))
{
    setupUI();
    applyDarkTheme();
    
    // Setup cache refresh timer
    m_cacheRefreshTimer->setInterval(CACHE_TIMEOUT_MS);
    connect(m_cacheRefreshTimer, &QTimer::timeout, this, &AgentChatBreadcrumb::refreshModelCache);
    m_cacheRefreshTimer->start();
}

void AgentChatBreadcrumb::setupUI()
{
    m_layout = new QHBoxLayout(this);
    m_layout->setContentsMargins(8, 4, 8, 4);
    m_layout->setSpacing(6);
    
    // Agent label
    m_agentLabel = new QLabel("🤖 Agent:", this);
    m_layout->addWidget(m_agentLabel);
    
    // Mode selector (Agent / Ask / Plan / Edit / Configure)
    m_modeSelector = new QComboBox(this);
    m_modeSelector->addItem("Agent - Autonomous Workflows");
    m_modeSelector->addItem("Ask - Simple Q&A");
    m_modeSelector->addItem("Plan - Multi-step Planning");
    m_modeSelector->addItem("Edit - Code Editing");
    m_modeSelector->addItem("Configure - Custom Agents");
    m_modeSelector->setCurrentIndex(0);  // Default to Agent mode
    m_layout->addWidget(m_modeSelector);
    
    connect(m_modeSelector, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &AgentChatBreadcrumb::onModeChanged);
    
    // Separator
    m_separator1 = new QLabel(" | ", this);
    m_layout->addWidget(m_separator1);
    
    // Model label
    m_modelLabel = new QLabel("📦 Model:", this);
    m_layout->addWidget(m_modelLabel);
    
    // Model selector dropdown with Auto at top
    m_modelSelector = new QComboBox(this);
    m_modelSelector->addItem("⚡ Auto (Smart Selection)");
    m_layout->addWidget(m_modelSelector);
    
    connect(m_modelSelector, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &AgentChatBreadcrumb::onModelChanged);
    
    // Configure button
    m_configureButton = new QPushButton("⚙️ Configure", this);
    m_layout->addWidget(m_configureButton);
    connect(m_configureButton, &QPushButton::clicked,
            this, &AgentChatBreadcrumb::onConfigureClicked);
    
    // Stretch to fill remaining space
    m_layout->addStretch();
    
    setLayout(m_layout);

    // Install event filter for rich tooltips on the dropdown items
    installEventFilterForTooltip();
}

void AgentChatBreadcrumb::applyDarkTheme()
{
    setStyleSheet(
        "AgentChatBreadcrumb {"
        "  background-color: #2d2d30;"
        "  border-bottom: 1px solid #3e3e42;"
        "  padding: 4px 8px;"
        "}"
        "QComboBox {"
        "  background-color: #3c3c3c;"
        "  color: #cccccc;"
        "  border: 1px solid #555555;"
        "  border-radius: 3px;"
        "  padding: 3px 6px;"
        "  min-width: 200px;"
        "}"
        "QComboBox::drop-down {"
        "  border: none;"
        "  background-color: #3c3c3c;"
        "}"
        "QComboBox::down-arrow {"
        "  image: url(:/icons/dropdown.png);"
        "}"
        "QComboBox QAbstractItemView {"
        "  background-color: #252526;"
        "  color: #cccccc;"
        "  selection-background-color: #094771;"
        "}"
        "QLabel {"
        "  color: #cccccc;"
        "  font-weight: bold;"
        "}"
        "QPushButton {"
        "  background-color: #0e639c;"
        "  color: white;"
        "  border: 1px solid #1177bb;"
        "  border-radius: 3px;"
        "  padding: 4px 12px;"
        "  font-weight: bold;"
        "}"
        "QPushButton:hover {"
        "  background-color: #1177bb;"
        "}"
        "QPushButton:pressed {"
        "  background-color: #094771;"
        "}"
        "QToolTip {"
        "  background: #1e1e1e;"
        "  border: 1px solid #3e3e42;"
        "  color: #cccccc;"
        "  padding: 8px;"
        "  border-radius: 4px;"
        "}"
    );
}

void AgentChatBreadcrumb::initialize()
{
    loadModelsFromConfiguration();
    // Fetch Ollama models asynchronously for better performance
    fetchOllamaModelsAsync(m_ollamaEndpoint);
    populateModelDropdown();
}

void AgentChatBreadcrumb::addModel(const ModelInfo& model)
{
    m_availableModels.append(model);
    m_modelMap[model.name] = model;
    
    // Categorize by type
    m_modelsByType[model.type].append(model.name);
}

void AgentChatBreadcrumb::addModels(const QList<ModelInfo>& models)
{
    for (const auto& model : models) {
        addModel(model);
    }
    populateModelDropdown();
}


void AgentChatBreadcrumb::setSelectedMode(AgentMode mode)
{
    m_currentMode = mode;
    m_modeSelector->setCurrentIndex(static_cast<int>(mode));
    updateAutoModelSuggestion();
}

void AgentChatBreadcrumb::setSelectedModel(const QString& modelName)
{
    m_selectedModel = modelName;
    int index = m_modelSelector->findText(modelName, Qt::MatchContains);
    if (index >= 0) {
        m_modelSelector->setCurrentIndex(index);
    }
}

QString AgentChatBreadcrumb::getSelectedModel() const
{
    return m_selectedModel;
}

AgentChatBreadcrumb::AgentMode AgentChatBreadcrumb::getSelectedMode() const
{
    return m_currentMode;
}

void AgentChatBreadcrumb::registerLocalModel(const QString& name, const QString& endpoint)
{
    ModelInfo model;
    model.name = name;
    model.displayName = QString("📁 %1 (Local)").arg(name);
    model.type = Local;
    model.endpoint = endpoint;
    model.isLarge = false;
    model.isContextModel = true;
    model.maxContextTokens = 8192;
    model.parameterCount = "Unknown";
    model.quantization = "Local";
    model.capabilities = "General";
    model.version = "Local";
    model.description = "Local model";
    addModel(model);
}

void AgentChatBreadcrumb::registerOllamaModel(const QString& name)
{
    ModelInfo model;
    model.name = name;
    model.displayName = QString("🦙 %1 (Ollama)").arg(name);
    model.type = Ollama;
    model.endpoint = "http://localhost:11434";
    model.isLarge = false;
    model.isContextModel = true;
    model.maxContextTokens = 4096;
    
    // Extract metadata from model name
    model.version = "latest";
    model.quantization = extractQuantizationFromName(name);
    model.parameterCount = extractParameterCountFromName(name);
    model.capabilities = extractCapabilitiesFromName(name);
    model.description = "Ollama model";
    
    addModel(model);
}

void AgentChatBreadcrumb::registerCloudModel(const QString& provider, const QString& modelId)
{
    ModelInfo model;
    model.name = modelId;
    model.displayName = QString("☁️ %1 (%2)").arg(modelId, provider);
    
    if (provider.toLower() == "claude" || provider.toLower().contains("anthropic")) {
        model.type = Claude;
        model.isLarge = true;
        model.maxContextTokens = 100000;
        model.parameterCount = "Unknown";
        model.quantization = "Cloud";
        model.capabilities = "Reasoning, Chat";
        model.version = "Latest";
        model.description = "Anthropic Claude model";
    } else if (provider.toLower() == "gpt" || provider.toLower().contains("openai")) {
        model.type = GPT;
        model.isLarge = true;
        model.maxContextTokens = 128000;
        model.parameterCount = "Unknown";
        model.quantization = "Cloud";
        model.capabilities = "General, Code, Reasoning";
        model.version = "Latest";
        model.description = "OpenAI GPT model";
    } else if (provider.toLower().contains("copilot")) {
        model.type = Copilot;
        model.isLarge = true;
        model.maxContextTokens = 8000;
        model.parameterCount = "Unknown";
        model.quantization = "Cloud";
        model.capabilities = "Code, Chat";
        model.version = "Latest";
        model.description = "GitHub Copilot model";
    } else if (provider.toLower().contains("hugging")) {
        model.type = HuggingFace;
        model.isLarge = false;
        model.maxContextTokens = 4096;
        model.parameterCount = "Unknown";
        model.quantization = "Cloud";
        model.capabilities = "General";
        model.version = "Latest";
        model.description = "HuggingFace model";
    } else {
        model.type = Custom;
        model.maxContextTokens = 4096;
        model.parameterCount = "Unknown";
        model.quantization = "Custom";
        model.capabilities = "General";
        model.version = "Custom";
        model.description = "Custom model";
    }
    
    model.isContextModel = true;
    model.endpoint = ""; // Cloud models use their own endpoints
    addModel(model);
}

void AgentChatBreadcrumb::loadModelsFromConfiguration()
{
    // Load from QSettings
    QSettings settings("RawrXD", "AgenticIDE");
    
    // Load local models
    settings.beginGroup("models/local");
    for (const auto& key : settings.allKeys()) {
        QString modelName = settings.value(key, "").toString();
        if (!modelName.isEmpty()) {
            registerLocalModel(modelName, "http://localhost:8000");
        }
    }
    settings.endGroup();
    
    // Load Ollama models
    settings.beginGroup("models/ollama");
    for (const auto& key : settings.allKeys()) {
        QString modelName = settings.value(key, "").toString();
        if (!modelName.isEmpty()) {
            registerOllamaModel(modelName);
        }
    }
    settings.endGroup();
    
    // Load cloud models (Anthropic, OpenAI, GitHub Copilot, HuggingFace)
    settings.beginGroup("models/cloud");
    
    // Claude/Anthropic models
    settings.beginGroup("claude");
    for (const auto& key : settings.allKeys()) {
        QString modelId = settings.value(key, "").toString();
        if (!modelId.isEmpty()) {
            registerCloudModel("Claude", modelId);
        }
    }
    settings.endGroup();
    
    // OpenAI/GPT models
    settings.beginGroup("gpt");
    for (const auto& key : settings.allKeys()) {
        QString modelId = settings.value(key, "").toString();
        if (!modelId.isEmpty()) {
            registerCloudModel("OpenAI", modelId);
        }
    }
    settings.endGroup();
    
    // GitHub Copilot models
    settings.beginGroup("copilot");
    for (const auto& key : settings.allKeys()) {
        QString modelId = settings.value(key, "").toString();
        if (!modelId.isEmpty()) {
            registerCloudModel("GitHub Copilot", modelId);
        }
    }
    settings.endGroup();
    
    // HuggingFace models
    settings.beginGroup("huggingface");
    for (const auto& key : settings.allKeys()) {
        QString modelId = settings.value(key, "").toString();
        if (!modelId.isEmpty()) {
            registerCloudModel("HuggingFace", modelId);
        }
    }
    settings.endGroup();
    
    settings.endGroup();
    
    // If no models registered, add defaults
    if (m_availableModels.isEmpty()) {
        registerOllamaModel("llama2");
        registerOllamaModel("mistral");
        registerCloudModel("Claude", "claude-3-5-sonnet");
        registerCloudModel("OpenAI", "gpt-4-turbo");
        registerCloudModel("GitHub Copilot", "gpt-4");
    }
}

void AgentChatBreadcrumb::populateModelDropdown()
{
    m_modelSelector->clear();
    m_modelSelector->addItem("⚡ Auto (Smart Selection)");
    
    // Add all registered models organized by type
    QStringList addedModels;
    
    // Order by type: Local, Ollama, Claude, GPT, Copilot, HuggingFace, Custom
    const QList<ModelType> typeOrder = {Local, Ollama, Claude, GPT, Copilot, HuggingFace, Custom};
    
    for (ModelType type : typeOrder) {
        if (m_modelsByType.contains(type)) {
            // Add separator if not the first type
            if (!addedModels.isEmpty()) {
                m_modelSelector->insertSeparator(m_modelSelector->count());
            }
            
            // Add models of this type
            for (const auto& modelName : m_modelsByType[type]) {
                if (m_modelMap.contains(modelName)) {
                    const ModelInfo& info = m_modelMap[modelName];
                    m_modelSelector->addItem(info.displayName, modelName);
                    
                    // Set tooltip with metadata
                    QString tooltip = QString("<b>%1</b><br/>"
                                            "Type: %2<br/>"
                                            "Parameters: %3<br/>"
                                            "Quantization: %4<br/>"
                                            "Capabilities: %5<br/>"
                                            "Context Window: %6 tokens<br/>"
                                            "Version: %7")
                                        .arg(info.name)
                                        .arg([type]() -> QString {
                                            switch (type) {
                                            case Local: return "Local";
                                            case Ollama: return "Ollama";
                                            case Claude: return "Claude";
                                            case GPT: return "GPT";
                                            case Copilot: return "GitHub Copilot";
                                            case HuggingFace: return "HuggingFace";
                                            case Custom: return "Custom";
                                            default: return "Unknown";
                                            }
                                        }())
                                        .arg(info.parameterCount)
                                        .arg(info.quantization)
                                        .arg(info.capabilities)
                                        .arg(info.maxContextTokens)
                                        .arg(info.version);
                    
                    if (!info.description.isEmpty()) {
                        tooltip += QString("<br/>Description: %1").arg(info.description);
                    }
                    
                    // Fallback tooltip (rich HTML will be shown via event filter during hover)
                    m_modelSelector->setItemData(m_modelSelector->count() - 1, tooltip, Qt::ToolTipRole);
                    addedModels.append(modelName);
                }
            }
        }
    }
}

QString AgentChatBreadcrumb::getAutoModelForMode(AgentMode mode) const
{
    switch (mode) {
    case Ask:
        // For simple Q&A, prefer smaller/faster models
        for (const auto& model : m_availableModels) {
            if (!model.isLarge && model.isContextModel) {
                return model.name;
            }
        }
        break;
    case Plan:
        // For planning, prefer larger models with more reasoning capability
        for (const auto& model : m_availableModels) {
            if (model.type == Claude || model.type == GPT) {
                return model.name;
            }
        }
        break;
    case Edit:
        // For code editing, prefer models optimized for code
        for (const auto& model : m_availableModels) {
            if (model.type == Copilot || model.type == GPT) {
                return model.name;
            }
        }
        break;
    case Configure:
        // Configuration mode - use first available
        if (!m_availableModels.isEmpty()) {
            return m_availableModels.first().name;
        }
        break;
    }
    
    // Fallback: return first available model
    if (!m_availableModels.isEmpty()) {
        return m_availableModels.first().name;
    }
    return "Auto";
}

QString AgentChatBreadcrumb::getSmartModelForTask(const QString& taskHint) const
{
    // Analyze task hint and select appropriate model
    QString hint = taskHint.toLower();
    
    // Code-related tasks
    if (hint.contains("code") || hint.contains("refactor") || hint.contains("debug")) {
        for (const auto& model : m_availableModels) {
            if (model.type == Copilot || model.type == GPT) {
                return model.name;
            }
        }
    }
    
    // Planning tasks
    if (hint.contains("plan") || hint.contains("design") || hint.contains("architecture")) {
        for (const auto& model : m_availableModels) {
            if (model.type == Claude || model.type == GPT) {
                return model.name;
            }
        }
    }
    
    // Context summarization - prefer smaller models
    if (hint.contains("summary") || hint.contains("summarize")) {
        for (const auto& model : m_availableModels) {
            if (model.isContextModel && !model.isLarge) {
                return model.name;
            }
        }
    }
    
    // Default: use Auto selection for current mode
    return getAutoModelForMode(m_currentMode);
}

void AgentChatBreadcrumb::updateAutoModelSuggestion()
{
    if (m_modelSelector->currentIndex() == 0) {  // Auto is selected
        QString suggestedModel = getAutoModelForMode(m_currentMode);
        m_modelSelector->setItemText(0, 
            QString("⚡ Auto (Smart Selection) → Suggested: %1").arg(suggestedModel));
    }
}

void AgentChatBreadcrumb::onModeChanged(int index)
{
    m_currentMode = static_cast<AgentMode>(index);
    emit agentModeChanged(m_currentMode);
    
    if (m_currentMode == Configure) {
        emit configureCustomAgents();
    }
    
    // Update Auto suggestion when mode changes
    updateAutoModelSuggestion();
}

void AgentChatBreadcrumb::onModelChanged(int index)
{
    if (index == 0) {
        // Auto selected
        m_selectedModel = "Auto";
        m_currentModelType = Auto;
        updateAutoModelSuggestion();
    } else {
        QVariant data = m_modelSelector->itemData(index);
        if (!data.isNull()) {
            m_selectedModel = data.toString();
        } else {
            m_selectedModel = m_modelSelector->itemText(index);
        }
        
        // Determine model type
        if (m_modelMap.contains(m_selectedModel)) {
            m_currentModelType = m_modelMap[m_selectedModel].type;
        }
    }
    
    emit modelSelected(m_selectedModel);
    emit modelTypeChanged(m_currentModelType);
}

void AgentChatBreadcrumb::onConfigureClicked()
{
    setSelectedMode(Configure);
    emit configureCustomAgents();
}

QString AgentChatBreadcrumb::buildTooltipText(const ModelInfo& info) const
{
    const QString html = QString(R"(
<html>
<head>
<style>
body { font-family: 'Segoe UI', Arial, sans-serif; margin: 0; padding: 8px; }
.header { font-size: 14px; font-weight: bold; color: #66b0ff; margin-bottom: 6px; }
.section { margin: 6px 0; }
.label { font-weight: bold; color: #ddd; display: inline-block; width: 96px; }
.value { color: #bbb; }
.chip { background: #2a2a2a; padding: 2px 6px; border-radius: 3px; margin: 2px; font-size: 11px; display: inline-block; }
.performance { background: #223322; color: #9ad29a; }
.capabilities { background: #3a3000; color: #e6c36a; }
</style>
</head>
<body>
<div class="header">%1</div>

<div class="section">
<span class="label">Parameters:</span>
<span class="value chip performance">%2</span>
</div>

<div class="section">
<span class="label">Quantization:</span>
<span class="value">%3</span>
</div>

<div class="section">
<span class="label">Context:</span>
<span class="value">%4 tokens</span>
</div>

<div class="section">
<span class="label">Version:</span>
<span class="value">%5</span>
</div>

<div class="section">
<span class="label">Capabilities:</span>
<span class="value chip capabilities">%6</span>
</div>

<div class="section">
<span class="label">Description:</span>
<div class="value" style="margin-top: 4px; font-style: italic;">%7</div>
</div>

</body>
</html>)")
        .arg(info.displayName.isEmpty() ? info.name : info.displayName)
        .arg(info.parameterCount)
        .arg(info.quantization)
        .arg(info.maxContextTokens)
        .arg(info.version)
        .arg(info.capabilities)
        .arg(info.description);
    return html;
}

void AgentChatBreadcrumb::installEventFilterForTooltip()
{
    if (!m_modelSelector) return;
    if (auto* view = m_modelSelector->view()) {
        view->viewport()->installEventFilter(this);
    }
    if (!m_tooltipTimer) {
        m_tooltipTimer = new QTimer(this);
        m_tooltipTimer->setInterval(50);
        m_tooltipTimer->setSingleShot(true);
    }
}

bool AgentChatBreadcrumb::eventFilter(QObject* obj, QEvent* event)
{
    if (m_modelSelector && obj == m_modelSelector->view()->viewport()) {
        if (event->type() == QEvent::ToolTip || event->type() == QEvent::MouseMove) {
            QMouseEvent* me = static_cast<QMouseEvent*>(event);
            m_lastMousePos = me->pos();
            QModelIndex idx = m_modelSelector->view()->indexAt(m_lastMousePos);
            if (idx.isValid()) {
                int row = idx.row();
                // Account for Auto entry at index 0
                if (row == 0) {
                    QToolTip::showText(m_modelSelector->view()->mapToGlobal(m_lastMousePos),
                                       "Smartly selects best model for the current mode.",
                                       m_modelSelector);
                    return true;
                }
                const QVariant data = m_modelSelector->itemData(row, Qt::UserRole);
                QString modelKey = data.isValid() ? data.toString() : m_modelSelector->itemText(row);
                if (m_modelMap.contains(modelKey)) {
                    const ModelInfo& info = m_modelMap[modelKey];
                    const QString tooltip = buildTooltipText(info);
                    QToolTip::showText(m_modelSelector->view()->mapToGlobal(m_lastMousePos), tooltip, m_modelSelector);
                    return true;
                }
            } else {
                QToolTip::hideText();
            }
        }
    }
    return QWidget::eventFilter(obj, event);
}

void AgentChatBreadcrumb::fetchOllamaModels(const QString& endpoint)
{
    m_ollamaEndpoint = endpoint;
    
    // Check cache first
    if (isModelCacheValid()) {
        qDebug() << "Using cached Ollama models";
        populateModelDropdown();
        return;
    }
    
    // Query Ollama API for available models
    QUrl url(endpoint + "/api/tags");
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("User-Agent", "RawrXD-AgenticIDE/1.0");
    request.setRawHeader("Connection", "close"); // Reuse connection
    
    QNetworkReply* reply = m_networkManager->get(request);
    
    // Use synchronous wait for this operation with timeout
    QEventLoop loop;
    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    connect(reply, &QNetworkReply::errorOccurred, &loop, &QEventLoop::quit);
    
    // Wait for response with timeout
    QTimer::singleShot(5000, &loop, &QEventLoop::quit);
    loop.exec();
    
    if (reply->error() == QNetworkReply::NoError) {
        QByteArray responseData = reply->readAll();
        QJsonDocument jsonDoc = QJsonDocument::fromJson(responseData);
        
        if (jsonDoc.isObject()) {
            QJsonObject jsonObj = jsonDoc.object();
            QJsonArray models = jsonObj["models"].toArray();
            
            // Clear existing Ollama models
            for (auto it = m_availableModels.begin(); it != m_availableModels.end(); ) {
                if (it->type == Ollama) {
                    QString modelName = it->name;
                    m_modelsByType[Ollama].removeAll(modelName);
                    m_modelMap.remove(modelName);
                    it = m_availableModels.erase(it);
                } else {
                    ++it;
                }
            }
            
            // Add fetched Ollama models
            for (const QJsonValue& modelValue : models) {
                QJsonObject modelObj = modelValue.toObject();
                QString modelName = modelObj["name"].toString();
                
                if (!modelName.isEmpty()) {
                    // Extract the base model name (before the tag)
                    int colonPos = modelName.indexOf(":");
                    QString tag = "latest";
                    if (colonPos > 0) {
                        tag = modelName.mid(colonPos + 1);
                        modelName = modelName.left(colonPos);
                    }
                    
                    // Check cache first
                    ModelInfo modelInfo = getCachedModelMetadata(modelName);
                    if (modelInfo.name.isEmpty()) {
                        // Not in cache, create new entry
                        modelInfo.name = modelName;
                        modelInfo.displayName = QString("🦙 %1 (Ollama)").arg(modelName);
                        modelInfo.type = Ollama;
                        modelInfo.endpoint = endpoint;
                        modelInfo.isLarge = false;
                        modelInfo.isContextModel = true;
                        modelInfo.maxContextTokens = 4096; // Default for Ollama models
                        modelInfo.lastUpdated = QDateTime::currentMSecsSinceEpoch();
                        
                        // Extract metadata from model name and tag
                        modelInfo.version = tag;
                        modelInfo.quantization = extractQuantizationFromName(modelName);
                        modelInfo.parameterCount = extractParameterCountFromName(modelName);
                        modelInfo.capabilities = extractCapabilitiesFromName(modelName);
                        modelInfo.description = QString("Ollama model (tag: %1)").arg(tag);
                        
                        // Cache the metadata
                        cacheModelMetadata(modelName, modelInfo);
                    }
                    
                    // Add to registry
                    addModel(modelInfo);
                }
            }
            
            m_lastCacheUpdate = QDateTime::currentMSecsSinceEpoch();
            qDebug() << "Ollama models fetched:" << m_modelsByType[Ollama];
        }
    } else {
        qDebug() << "Failed to fetch Ollama models:" << reply->errorString();
        qDebug() << "Using cached models as fallback";
    }
    
    reply->deleteLater();
    
    // Refresh the dropdown
    populateModelDropdown();
}

// Performance enhancement methods
void AgentChatBreadcrumb::cacheModelMetadata(const QString& modelName, const ModelInfo& metadata)
{
    QMutexLocker locker(&m_cacheMutex);
    ModelInfo* cachedInfo = new ModelInfo(metadata);
    m_metadataCache.insert(modelName, cachedInfo);
}

AgentChatBreadcrumb::ModelInfo AgentChatBreadcrumb::getCachedModelMetadata(const QString& modelName) const
{
    QMutexLocker locker(&m_cacheMutex);
    ModelInfo* cachedInfo = m_metadataCache.object(modelName);
    if (cachedInfo) {
        // Check if cache is still valid (5 minutes)
        qint64 now = QDateTime::currentMSecsSinceEpoch();
        if ((now - cachedInfo->lastUpdated) < CACHE_TIMEOUT_MS) {
            return *cachedInfo;
        } else {
            // Cache expired, remove it
            m_metadataCache.remove(modelName);
        }
    }
    return ModelInfo(); // Return empty ModelInfo
}

void AgentChatBreadcrumb::clearModelCache()
{
    QMutexLocker locker(&m_cacheMutex);
    m_metadataCache.clear();
    m_lastCacheUpdate = 0;
}

bool AgentChatBreadcrumb::isModelCacheValid() const
{
    if (m_metadataCache.isEmpty()) return false;
    
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    return (now - m_lastCacheUpdate) < CACHE_TIMEOUT_MS;
}

void AgentChatBreadcrumb::refreshModelCache()
{
    if (!m_metadataCache.isEmpty()) {
        qDebug() << "Refreshing model cache";
        // Trigger async refresh
        fetchOllamaModelsAsync(m_ollamaEndpoint);
    }
}

void AgentChatBreadcrumb::fetchOllamaModelsAsync(const QString& endpoint)
{
    if (m_asyncFetchInProgress) {
        qDebug() << "Async fetch already in progress, skipping";
        return;
    }
    
    m_ollamaEndpoint = endpoint;
    m_asyncFetchInProgress = true;
    
    // Query Ollama API for available models (async)
    QUrl url(endpoint + "/api/tags");
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("User-Agent", "RawrXD-AgenticIDE/1.0");
    
    QNetworkReply* reply = m_networkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, &AgentChatBreadcrumb::onAsyncOllamaFetchFinished);
        // connect(reply, static_cast<void(QNetworkReply::*)(QNetworkReply::NetworkError)>(&QNetworkReply::error),
        //     this, &AgentChatBreadcrumb::onAsyncOllamaFetchFinished);
}

void AgentChatBreadcrumb::onAsyncOllamaFetchFinished()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;
    
    m_asyncFetchInProgress = false;
    
    if (reply->error() == QNetworkReply::NoError) {
        QByteArray responseData = reply->readAll();
        QJsonDocument jsonDoc = QJsonDocument::fromJson(responseData);
        
        if (jsonDoc.isObject()) {
            QJsonObject jsonObj = jsonDoc.object();
            QJsonArray models = jsonObj["models"].toArray();
            
            // Clear existing Ollama models
            for (auto it = m_availableModels.begin(); it != m_availableModels.end(); ) {
                if (it->type == Ollama) {
                    QString modelName = it->name;
                    m_modelsByType[Ollama].removeAll(modelName);
                    m_modelMap.remove(modelName);
                    it = m_availableModels.erase(it);
                } else {
                    ++it;
                }
            }
            
            // Add fetched Ollama models
            for (const QJsonValue& modelValue : models) {
                QJsonObject modelObj = modelValue.toObject();
                QString modelName = modelObj["name"].toString();
                
                if (!modelName.isEmpty()) {
                    // Extract the base model name (before the tag)
                    int colonPos = modelName.indexOf(":");
                    QString tag = "latest";
                    if (colonPos > 0) {
                        tag = modelName.mid(colonPos + 1);
                        modelName = modelName.left(colonPos);
                    }
                    
                    // Check cache first
                    ModelInfo modelInfo = getCachedModelMetadata(modelName);
                    if (modelInfo.name.isEmpty()) {
                        // Not in cache, create new entry
                        modelInfo.name = modelName;
                        modelInfo.displayName = QString("🦙 %1 (Ollama)").arg(modelName);
                        modelInfo.type = Ollama;
                        modelInfo.endpoint = m_ollamaEndpoint;
                        modelInfo.isLarge = false;
                        modelInfo.isContextModel = true;
                        modelInfo.maxContextTokens = 4096;
                        modelInfo.lastUpdated = QDateTime::currentMSecsSinceEpoch();
                        
                        // Extract metadata from model name and tag
                        modelInfo.version = tag;
                        modelInfo.quantization = extractQuantizationFromName(modelName);
                        modelInfo.parameterCount = extractParameterCountFromName(modelName);
                        modelInfo.capabilities = extractCapabilitiesFromName(modelName);
                        modelInfo.description = QString("Ollama model (tag: %1)").arg(tag);
                        
                        // Cache the metadata
                        cacheModelMetadata(modelName, modelInfo);
                    }
                    
                    // Add to registry
                    addModel(modelInfo);
                }
            }
            
            m_lastCacheUpdate = QDateTime::currentMSecsSinceEpoch();
            qDebug() << "Ollama models fetched async:" << m_modelsByType[Ollama];
        }
    } else {
        qDebug() << "Failed to fetch Ollama models async:" << reply->errorString();
    }
    
    reply->deleteLater();
    
    // Refresh the dropdown
    populateModelDropdown();
    
    // Emit signal
    emit ollamaModelsUpdated();
}

// ===== Helper implementations for linker =====

QString AgentChatBreadcrumb::extractParameterCountFromName(const QString& modelName) const {
    QRegularExpression re("(\\d+[bB])");
    QRegularExpressionMatch match = re.match(modelName);
    if (match.hasMatch()) {
        return match.captured(1).toUpper();
    }
    QStringList patterns = {"7b", "13b", "30b", "34b", "40b", "65b", "70b", "180b"};
    for (const QString& pattern : patterns) {
        if (modelName.contains(pattern, Qt::CaseInsensitive)) {
            return pattern.toUpper();
        }
    }
    return "Unknown";
}

QString AgentChatBreadcrumb::extractQuantizationFromName(const QString& modelName) const {
    QRegularExpression re("Q\\d_[A-Z](?:_[A-Z])?");
    QRegularExpressionMatch match = re.match(modelName);
    if (match.hasMatch()) {
        return match.captured(0);
    }
    QStringList patterns = {"q4", "q5", "q8", "fp16", "gguf"};
    for (const QString& pattern : patterns) {
        if (modelName.contains(pattern, Qt::CaseInsensitive)) {
            return pattern.toUpper();
        }
    }
    return "Default";
}

QString AgentChatBreadcrumb::extractCapabilitiesFromName(const QString& modelName) const {
    QStringList capabilities;
    if (modelName.contains("code", Qt::CaseInsensitive) || 
        modelName.contains("copilot", Qt::CaseInsensitive) ||
        modelName.contains("deepseek", Qt::CaseInsensitive)) {
        capabilities << "Code";
    }
    if (modelName.contains("chat", Qt::CaseInsensitive) || 
        modelName.contains("vicuna", Qt::CaseInsensitive) ||
        modelName.contains("openchat", Qt::CaseInsensitive)) {
        capabilities << "Chat";
    }
    if (modelName.contains("reason", Qt::CaseInsensitive) || 
        modelName.contains("mixtral", Qt::CaseInsensitive) ||
        modelName.contains("claude", Qt::CaseInsensitive)) {
        capabilities << "Reasoning";
    }
    if (capabilities.isEmpty()) {
        capabilities << "General";
    }
    return capabilities.join(", ");
}

void AgentChatBreadcrumb::onOllamaModelsRetrieved() {
    qDebug() << "Ollama models retrieved";
    populateModelDropdown();
}

