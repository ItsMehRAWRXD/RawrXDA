// MASM Agentic Bridge - Implementation
#include "masm_agentic_bridge.hpp"
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>

Q_GLOBAL_STATIC(MASMAgenticBridge, s_masmBridge)

MASMAgenticBridge::MASMAgenticBridge(QObject* parent)
    : QObject(parent)
{
}

MASMAgenticBridge::~MASMAgenticBridge() = default;

MASMAgenticBridge* MASMAgenticBridge::instance()
{
    return s_masmBridge();
}

// IDE Master Integration
bool MASMAgenticBridge::initializeIDE()
{
    if (m_ideInitialized) {
        return true;
    }
    
    qDebug() << "Initializing MASM IDE Master...";
    int result = IDEMaster_Initialize();
    m_ideInitialized = (result == 0);
    
    if (m_ideInitialized) {
        emit ideInitialized();
        qDebug() << "✅ MASM IDE Master initialized";
    } else {
        emit errorOccurred("Failed to initialize MASM IDE Master");
    }
    
    return m_ideInitialized;
}

bool MASMAgenticBridge::loadModel(const QString& modelPath, int loadMethod)
{
    QByteArray pathBytes = modelPath.toUtf8();
    int result = IDEMaster_LoadModel(pathBytes.constData(), loadMethod);
    
    if (result == 0) {
        emit modelLoaded(modelPath);
        qDebug() << "✅ Model loaded:" << modelPath;
        return true;
    }
    
    emit errorOccurred(QString("Failed to load model: %1").arg(modelPath));
    return false;
}

bool MASMAgenticBridge::hotSwapModel(int newModelID)
{
    int result = IDEMaster_HotSwapModel(newModelID);
    
    if (result == 0) {
        m_activeModelID = newModelID;
        emit modelSwapped(newModelID);
        qDebug() << "✅ Model hot-swapped to ID:" << newModelID;
        return true;
    }
    
    emit errorOccurred(QString("Failed to hot-swap to model ID: %1").arg(newModelID));
    return false;
}

QString MASMAgenticBridge::executeAgenticTask(const QString& task)
{
    QByteArray taskBytes = task.toUtf8();
    int result = IDEMaster_ExecuteAgenticTask(taskBytes.constData());
    
    QString resultStr = (result == 0) ? "Task completed successfully" : "Task failed";
    emit agenticTaskComplete(resultStr);
    
    return resultStr;
}

bool MASMAgenticBridge::saveWorkspace(const QString& path)
{
    QByteArray pathBytes = path.toUtf8();
    int result = IDEMaster_SaveWorkspace(pathBytes.constData());
    return (result == 0);
}

bool MASMAgenticBridge::loadWorkspace(const QString& path)
{
    QByteArray pathBytes = path.toUtf8();
    int result = IDEMaster_LoadWorkspace(pathBytes.constData());
    return (result == 0);
}

// Browser Agent
bool MASMAgenticBridge::initializeBrowser()
{
    if (m_browserInitialized) {
        return true;
    }
    
    qDebug() << "Initializing MASM Browser Agent...";
    int result = BrowserAgent_Init();
    m_browserInitialized = (result == 0);
    
    if (m_browserInitialized) {
        qDebug() << "✅ MASM Browser Agent initialized";
    }
    
    return m_browserInitialized;
}

bool MASMAgenticBridge::navigateTo(const QString& url)
{
    QByteArray urlBytes = url.toUtf8();
    int result = BrowserAgent_Navigate(urlBytes.constData());
    
    if (result == 0) {
        emit browserNavigated(url);
        return true;
    }
    
    return false;
}

QString MASMAgenticBridge::getDOMContent()
{
    const char* dom = BrowserAgent_GetDOM();
    return QString::fromUtf8(dom ? dom : "");
}

QString MASMAgenticBridge::extractPageText()
{
    const char* text = BrowserAgent_ExtractText();
    return QString::fromUtf8(text ? text : "");
}

bool MASMAgenticBridge::clickElement(const QString& elementID)
{
    QByteArray idBytes = elementID.toUtf8();
    int result = BrowserAgent_ClickElement(idBytes.constData());
    return (result == 0);
}

bool MASMAgenticBridge::fillFormField(const QString& fieldID, const QString& value)
{
    QByteArray idBytes = fieldID.toUtf8();
    QByteArray valueBytes = value.toUtf8();
    int result = BrowserAgent_FillForm(idBytes.constData(), valueBytes.constData());
    return (result == 0);
}

QString MASMAgenticBridge::executeScript(const QString& script)
{
    QByteArray scriptBytes = script.toUtf8();
    int result = BrowserAgent_ExecuteScript(scriptBytes.constData());
    return (result == 0) ? "Script executed successfully" : "Script execution failed";
}

// Model Hotpatch
bool MASMAgenticBridge::initializeHotpatch()
{
    if (m_hotpatchInitialized) {
        return true;
    }
    
    qDebug() << "Initializing MASM Hotpatch Engine...";
    int result = HotPatch_Init();
    m_hotpatchInitialized = (result == 0);
    
    if (m_hotpatchInitialized) {
        qDebug() << "✅ MASM Hotpatch Engine initialized (32 model slots)";
    }
    
    return m_hotpatchInitialized;
}

int MASMAgenticBridge::registerModel(const QString& path, const QString& name, int sourceType)
{
    QByteArray pathBytes = path.toUtf8();
    QByteArray nameBytes = name.toUtf8();
    int modelID = HotPatch_RegisterModel(pathBytes.constData(), nameBytes.constData(), sourceType);
    
    if (modelID > 0) {
        qDebug() << "✅ Model registered:" << name << "ID:" << modelID;
    }
    
    return modelID;
}

bool MASMAgenticBridge::swapToModel(int modelID)
{
    int result = HotPatch_SwapModel(modelID);
    
    if (result == 0) {
        m_activeModelID = modelID;
        emit modelSwapped(modelID);
        qDebug() << "✅ Hot-swapped to model ID:" << modelID;
        return true;
    }
    
    return false;
}

bool MASMAgenticBridge::rollbackModel()
{
    int result = HotPatch_RollbackModel();
    return (result == 0);
}

bool MASMAgenticBridge::cacheModel(int modelID)
{
    int result = HotPatch_CacheModel(modelID);
    return (result == 0);
}

bool MASMAgenticBridge::warmupModel(int modelID)
{
    int result = HotPatch_WarmupModel(modelID);
    return (result == 0);
}

// Agentic Tools
bool MASMAgenticBridge::initializeTools()
{
    if (m_toolsInitialized) {
        return true;
    }
    
    qDebug() << "Initializing MASM Agentic Tools (58 total)...";
    int result = AgenticIDE_Initialize();
    m_toolsInitialized = (result == 0);
    
    if (m_toolsInitialized) {
        qDebug() << "✅ 58 MASM Agentic Tools initialized";
    }
    
    return m_toolsInitialized;
}

bool MASMAgenticBridge::executeTool(int toolID, const QVariantMap& params)
{
    QJsonDocument doc = QJsonDocument::fromVariant(params);
    QByteArray jsonBytes = doc.toJson(QJsonDocument::Compact);
    
    int result = AgenticIDE_ExecuteTool(toolID, jsonBytes.constData());
    bool success = (result == 0);
    
    emit toolExecuted(toolID, success);
    
    if (success) {
        qDebug() << "✅ Tool" << toolID << "executed successfully";
    }
    
    return success;
}

bool MASMAgenticBridge::executeToolChain(const QList<int>& tools, const QList<QVariantMap>& params)
{
    if (tools.size() != params.size()) {
        return false;
    }
    
    QVector<int> toolArray = tools.toVector();
    QVector<QByteArray> paramBytes;
    QVector<const char*> paramPtrs;
    
    for (const auto& param : params) {
        QJsonDocument doc = QJsonDocument::fromVariant(param);
        paramBytes.append(doc.toJson(QJsonDocument::Compact));
    }
    
    for (const auto& bytes : paramBytes) {
        paramPtrs.append(bytes.constData());
    }
    
    int result = AgenticIDE_ExecuteToolChain(
        toolArray.data(),
        paramPtrs.data(),
        toolArray.size()
    );
    
    return (result == 0);
}

bool MASMAgenticBridge::setToolEnabled(int toolID, bool enabled)
{
    int result = AgenticIDE_SetToolEnabled(toolID, enabled ? 1 : 0);
    return (result == 0);
}

bool MASMAgenticBridge::isToolEnabled(int toolID)
{
    int result = AgenticIDE_IsToolEnabled(toolID);
    return (result == 1);
}

QString MASMAgenticBridge::getToolName(int toolID)
{
    const char* name = AgenticIDE_GetToolName(toolID);
    return QString::fromUtf8(name ? name : "Unknown");
}

QString MASMAgenticBridge::getToolDescription(int toolID)
{
    const char* desc = AgenticIDE_GetToolDescription(toolID);
    return QString::fromUtf8(desc ? desc : "No description");
}
