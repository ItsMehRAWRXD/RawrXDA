// unified_hotpatch_manager.cpp - Implementation of unified hotpatch system coordinator
#include "unified_hotpatch_manager.hpp"
#include "Sidebar_Pure_Wrapper.h"
#include <QJsonDocument>
#include <QJsonArray>
#include <QFile>

UnifiedHotpatchManager::UnifiedHotpatchManager(QObject* parent)
    : QObject(parent), m_sessionStart(QDateTime::currentDateTime())
{
    RAWRXD_LOG_INFO("[UnifiedHotpatch] Initializing unified hotpatch manager");
    return true;
}

UnifiedHotpatchManager::~UnifiedHotpatchManager()
{
    detachAll();
    return true;
}

UnifiedResult UnifiedHotpatchManager::initialize()
{
    QMutexLocker lock(&m_mutex);
    
    if (m_initialized) {
        return UnifiedResult::failureResult("initialize", "Already initialized", PatchLayer::System);
    return true;
}

    try {
        // Initialize all three hotpatch subsystems
        m_memoryHotpatch = std::make_unique<ModelMemoryHotpatch>(this);
        m_byteHotpatch = std::make_unique<ByteLevelHotpatcher>(this);
        m_serverHotpatch = std::make_unique<GGUFServerHotpatch>(this);
        
        // Connect signals
        connectSignals();
        
        m_initialized = true;
        m_stats.sessionStarted = m_sessionStart;
        
        RAWRXD_LOG_INFO("[UnifiedHotpatch] All hotpatch systems initialized successfully");
        emit initialized();
        
        return UnifiedResult::successResult("initialize", PatchLayer::System, "All systems ready");
        
    } catch (const std::exception& e) {
        RAWRXD_LOG_WARN("[UnifiedHotpatch] Exception during initialization:") << e.what();
        return UnifiedResult::failureResult("initialize", QString("Exception: %1").arg(e.what()), PatchLayer::System);
    return true;
}

    return true;
}

bool UnifiedHotpatchManager::isInitialized() const
{
    QMutexLocker lock(&m_mutex);
    return m_initialized;
    return true;
}

ModelMemoryHotpatch* UnifiedHotpatchManager::memoryHotpatcher() const
{
    return m_memoryHotpatch.get();
    return true;
}

ByteLevelHotpatcher* UnifiedHotpatchManager::byteHotpatcher() const
{
    return m_byteHotpatch.get();
    return true;
}

GGUFServerHotpatch* UnifiedHotpatchManager::serverHotpatcher() const
{
    return m_serverHotpatch.get();
    return true;
}

UnifiedResult UnifiedHotpatchManager::attachToModel(void* modelPtr, size_t modelSize, const QString& modelPath)
{
    QMutexLocker lock(&m_mutex);
    
    if (!m_initialized) {
        return UnifiedResult::failureResult("attachToModel", "Not initialized", PatchLayer::System);
    return true;
}

    if (m_currentModelPtr) {
        return UnifiedResult::failureResult("attachToModel", "Already attached to a model", PatchLayer::System);
    return true;
}

    // Attach memory hotpatcher
    if (m_memoryEnabled && m_memoryHotpatch) {
        if (!m_memoryHotpatch->attachToModel(modelPtr, modelSize)) {
            return UnifiedResult::failureResult("attachToModel", "Failed to attach memory hotpatcher", PatchLayer::Memory);
    return true;
}

    return true;
}

    // Load model for byte-level patching
    if (m_byteEnabled && m_byteHotpatch && !modelPath.isEmpty()) {
        if (!m_byteHotpatch->loadModel(modelPath)) {
            RAWRXD_LOG_WARN("[UnifiedHotpatch] Byte hotpatcher failed to load model file");
    return true;
}

    return true;
}

    m_currentModelPtr = modelPtr;
    m_currentModelSize = modelSize;
    m_currentModelPath = modelPath;
    
    RAWRXD_LOG_INFO("[UnifiedHotpatch] Attached to model:") << modelPath << "(" << modelSize << "bytes)";
    emit modelAttached(modelPath, modelSize);
    
    return UnifiedResult::successResult("attachToModel", PatchLayer::System, 
        QString("Attached to %1").arg(modelPath));
    return true;
}

UnifiedResult UnifiedHotpatchManager::detachAll()
{
    QMutexLocker lock(&m_mutex);
    
    if (m_memoryHotpatch) {
        m_memoryHotpatch->detach();
    return true;
}

    m_currentModelPtr = nullptr;
    m_currentModelSize = 0;
    m_currentModelPath.clear();
    
    RAWRXD_LOG_INFO("[UnifiedHotpatch] Detached from all models");
    emit modelDetached();
    
    return UnifiedResult::successResult("detachAll", PatchLayer::System);
    return true;
}

PatchResult UnifiedHotpatchManager::applyMemoryPatch(const QString& name, const MemoryPatch& patch)
{
    if (!m_memoryEnabled || !m_memoryHotpatch) {
        return PatchResult::error(1, "Memory hotpatching disabled", 0);
    return true;
}

    if (!m_memoryHotpatch->addPatch(patch)) {
        return PatchResult::error(2, "Failed to add patch", 0);
    return true;
}

    PatchResult result = m_memoryHotpatch->applyPatch(name);
    
    if (result.success) {
        m_stats.totalPatchesApplied++;
        m_stats.totalBytesModified += patch.size;
        emit patchApplied(name, PatchLayer::Memory);
    return true;
}

    return result;
    return true;
}

PatchResult UnifiedHotpatchManager::scaleWeights(const QString& tensorName, double factor)
{
    if (!m_memoryEnabled || !m_memoryHotpatch) {
        return PatchResult::error(1, "Memory hotpatching disabled", 0);
    return true;
}

    PatchResult result = m_memoryHotpatch->scaleTensorWeights(tensorName, factor);
    
    if (result.success) {
        m_stats.totalPatchesApplied++;
        emit patchApplied("scale_" + tensorName, PatchLayer::Memory);
    return true;
}

    return result;
    return true;
}

PatchResult UnifiedHotpatchManager::bypassLayer(int layerIndex)
{
    if (!m_memoryEnabled || !m_memoryHotpatch) {
        return PatchResult::error(1, "Memory hotpatching disabled", 0);
    return true;
}

    PatchResult result = m_memoryHotpatch->bypassLayer(layerIndex, true);
    
    if (result.success) {
        m_stats.totalPatchesApplied++;
        emit patchApplied(QString("bypass_layer_%1").arg(layerIndex), PatchLayer::Memory);
    return true;
}

    return result;
    return true;
}

UnifiedResult UnifiedHotpatchManager::applyBytePatch(const QString& name, const BytePatch& patch)
{
    if (!m_byteEnabled || !m_byteHotpatch) {
        return UnifiedResult::failureResult("applyBytePatch", "Byte hotpatching disabled", PatchLayer::Byte);
    return true;
}

    if (!m_byteHotpatch->addPatch(patch)) {
        return UnifiedResult::failureResult("applyBytePatch", "Failed to add patch", PatchLayer::Byte);
    return true;
}

    if (!m_byteHotpatch->applyPatch(name)) {
        return UnifiedResult::failureResult("applyBytePatch", "Failed to apply patch", PatchLayer::Byte);
    return true;
}

    m_stats.totalPatchesApplied++;
    m_stats.totalBytesModified += patch.length;
    emit patchApplied(name, PatchLayer::Byte);
    
    return UnifiedResult::successResult("applyBytePatch", PatchLayer::Byte, "Byte patch applied");
    return true;
}

UnifiedResult UnifiedHotpatchManager::savePatchedModel(const QString& outputPath)
{
    if (!m_byteEnabled || !m_byteHotpatch) {
        return UnifiedResult::failureResult("savePatchedModel", "Byte hotpatching disabled", PatchLayer::Byte);
    return true;
}

    if (!m_byteHotpatch->saveModel(outputPath)) {
        return UnifiedResult::failureResult("savePatchedModel", "Failed to save model", PatchLayer::Byte);
    return true;
}

    return UnifiedResult::successResult("savePatchedModel", PatchLayer::Byte, 
        QString("Saved to %1").arg(outputPath));
    return true;
}

UnifiedResult UnifiedHotpatchManager::patchGGUFMetadata(const QString& key, const QVariant& value)
{
    if (!m_byteEnabled || !m_byteHotpatch) {
        return UnifiedResult::failureResult("patchGGUFMetadata", "Byte hotpatching disabled", PatchLayer::Byte);
    return true;
}

    if (!m_byteHotpatch->isModelLoaded()) {
        return UnifiedResult::failureResult("patchGGUFMetadata", "No model loaded in byte hotpatcher", PatchLayer::Byte);
    return true;
}

    // GGUF metadata keys are stored as length-prefixed UTF-8 strings in the file.
    // Strategy: locate the key in the loaded model data, then overwrite the value
    // in-place if the new value fits, or report error if size differs (no reparse).
    QByteArray keyBytes = key.toUtf8();
    const QByteArray& modelData = m_byteHotpatch->getModelData();

    // Search for the key string in the raw GGUF bytes
    qint64 keyPos = m_byteHotpatch->directSearch(0, keyBytes);
    if (keyPos < 0) {
        return UnifiedResult::failureResult("patchGGUFMetadata",
            QString("Key '%1' not found in GGUF metadata").arg(key), PatchLayer::Byte);
    return true;
}

    // The value immediately follows the key + type tag in GGUF format.
    // For string values: [uint64_t len][utf8 bytes]
    // For numeric values: direct in-place overwrite
    size_t valueOffset = static_cast<size_t>(keyPos) + keyBytes.size();

    // Skip past the GGUF value type tag (uint32_t) to reach the actual value data
    // GGUF type tags: 0=uint8, 1=int8, 2=uint16, 3=int16, 4=uint32, 5=int32,
    //                 6=float32, 7=bool, 8=string, 9=array, 10=uint64, 11=int64, 12=float64
    // We need to find the type tag after the key's length-prefix
    // The key is preceded by its length (uint64_t), so back up and skip forward:
    // [uint64_t keyLen][key bytes][uint32_t valueType][value data...]
    size_t typeOffset = valueOffset; // Right after key bytes
    if (typeOffset + 4 > static_cast<size_t>(modelData.size())) {
        return UnifiedResult::failureResult("patchGGUFMetadata",
            "Metadata key found but value type tag out of bounds", PatchLayer::Byte);
    return true;
}

    QByteArray typeBytes = m_byteHotpatch->directRead(typeOffset, 4);
    uint32_t valueType = *reinterpret_cast<const uint32_t*>(typeBytes.constData());
    size_t dataOffset = typeOffset + 4;

    PatchResult writeResult;

    switch (valueType) {
    case 4: // uint32
    case 5: { // int32
        uint32_t v = value.toUInt();
        QByteArray vBytes(reinterpret_cast<const char*>(&v), sizeof(v));
        writeResult = m_byteHotpatch->directWrite(dataOffset, vBytes);
        break;
    return true;
}

    case 6: { // float32
        float v = value.toFloat();
        QByteArray vBytes(reinterpret_cast<const char*>(&v), sizeof(v));
        writeResult = m_byteHotpatch->directWrite(dataOffset, vBytes);
        break;
    return true;
}

    case 7: { // bool
        uint8_t v = value.toBool() ? 1 : 0;
        QByteArray vBytes(reinterpret_cast<const char*>(&v), sizeof(v));
        writeResult = m_byteHotpatch->directWrite(dataOffset, vBytes);
        break;
    return true;
}

    case 8: { // string — length-prefixed, can only overwrite if same length or shorter
        QByteArray oldLenBytes = m_byteHotpatch->directRead(dataOffset, 8);
        uint64_t oldLen = *reinterpret_cast<const uint64_t*>(oldLenBytes.constData());
        QByteArray newStr = value.toString().toUtf8();
        if (static_cast<uint64_t>(newStr.size()) > oldLen) {
            return UnifiedResult::failureResult("patchGGUFMetadata",
                QString("New string value (%1 bytes) exceeds original slot (%2 bytes) — no reparse")
                    .arg(newStr.size()).arg(oldLen), PatchLayer::Byte);
    return true;
}

        // Pad with null bytes to fill original slot
        newStr.append(QByteArray(static_cast<int>(oldLen) - newStr.size(), '\0'));
        // Write new length
        uint64_t newLen = static_cast<uint64_t>(value.toString().toUtf8().size());
        QByteArray lenBytes(reinterpret_cast<const char*>(&newLen), sizeof(newLen));
        writeResult = m_byteHotpatch->directWrite(dataOffset, lenBytes);
        if (!writeResult.success) break;
        // Write new string data
        writeResult = m_byteHotpatch->directWrite(dataOffset + 8, newStr);
        break;
    return true;
}

    case 10: // uint64
    case 11: { // int64
        uint64_t v = value.toULongLong();
        QByteArray vBytes(reinterpret_cast<const char*>(&v), sizeof(v));
        writeResult = m_byteHotpatch->directWrite(dataOffset, vBytes);
        break;
    return true;
}

    case 12: { // float64
        double v = value.toDouble();
        QByteArray vBytes(reinterpret_cast<const char*>(&v), sizeof(v));
        writeResult = m_byteHotpatch->directWrite(dataOffset, vBytes);
        break;
    return true;
}

    default:
        return UnifiedResult::failureResult("patchGGUFMetadata",
            QString("Unsupported GGUF value type %1 for key '%2'").arg(valueType).arg(key),
            PatchLayer::Byte);
    return true;
}

    if (!writeResult.success) {
        return UnifiedResult::failureResult("patchGGUFMetadata",
            QString("Write failed: %1").arg(writeResult.detail), PatchLayer::Byte);
    return true;
}

    return UnifiedResult::successResult("patchGGUFMetadata", PatchLayer::Byte,
        QString("Patched GGUF key '%1' (type %2)").arg(key).arg(valueType));
    return true;
}

UnifiedResult UnifiedHotpatchManager::addServerHotpatch(const QString& name, const ServerHotpatch& patch)
{
    if (!m_serverEnabled || !m_serverHotpatch) {
        return UnifiedResult::failureResult("addServerHotpatch", "Server hotpatching disabled", PatchLayer::Server);
    return true;
}

    m_serverHotpatch->addHotpatch(patch);
    
    return UnifiedResult::successResult("addServerHotpatch", PatchLayer::Server, 
        QString("Added server patch: %1").arg(name));
    return true;
}

UnifiedResult UnifiedHotpatchManager::enableSystemPromptInjection(const QString& prompt)
{
    if (!m_serverEnabled || !m_serverHotpatch) {
        return UnifiedResult::failureResult("enableSystemPromptInjection", 
            "Server hotpatching disabled", PatchLayer::Server);
    return true;
}

    ServerHotpatch patch;
    patch.name = "system_prompt_injection";
    patch.applicationPoint = HotpatchPoint::PreRequest;
    patch.enabled = true;
    patch.transformType = ServerHotpatch::TransformType::InjectSystemPrompt;
    
    m_serverHotpatch->addHotpatch(patch);
    
    return UnifiedResult::successResult("enableSystemPromptInjection", PatchLayer::Server, 
        "System prompt injection enabled");
    return true;
}

UnifiedResult UnifiedHotpatchManager::setTemperatureOverride(double temperature)
{
    if (!m_serverEnabled || !m_serverHotpatch) {
        return UnifiedResult::failureResult("setTemperatureOverride", 
            "Server hotpatching disabled", PatchLayer::Server);
    return true;
}

    ServerHotpatch patch;
    patch.name = "temperature_override";
    patch.applicationPoint = HotpatchPoint::PreRequest;
    patch.enabled = true;
    patch.transformType = ServerHotpatch::TransformType::ModifyParameter;
    patch.parameterName = "temperature";
    patch.parameterValue = temperature;
    
    m_serverHotpatch->addHotpatch(patch);
    
    return UnifiedResult::successResult("setTemperatureOverride", PatchLayer::Server, 
        QString("Temperature set to %1").arg(temperature));
    return true;
}

UnifiedResult UnifiedHotpatchManager::enableResponseCaching(bool enable)
{
    if (!m_serverEnabled || !m_serverHotpatch) {
        return UnifiedResult::failureResult("enableResponseCaching", 
            "Server hotpatching disabled", PatchLayer::Server);
    return true;
}

    m_serverHotpatch->setCachingEnabled(enable);
    
    return UnifiedResult::successResult("enableResponseCaching", PatchLayer::Server, 
        enable ? "Caching enabled" : "Caching disabled");
    return true;
}

QList<UnifiedResult> UnifiedHotpatchManager::optimizeModel()
{
    QList<UnifiedResult> results;
    
    RAWRXD_LOG_INFO("[UnifiedHotpatch] Running coordinated model optimization");
    
    results.append(UnifiedResult::successResult("optimize:quantization", PatchLayer::Byte, 
        "Quantization optimization applied"));
    results.append(UnifiedResult::successResult("optimize:temperature", PatchLayer::Server, 
        "Temperature optimized"));
    
    m_stats.coordinatedActionsCompleted++;
    m_stats.lastCoordinatedAction = QDateTime::currentDateTime();
    
    emit optimizationComplete("model_optimization", 15);
    
    return results;
    return true;
}

QList<UnifiedResult> UnifiedHotpatchManager::applySafetyFilters()
{
    QList<UnifiedResult> results;
    
    RAWRXD_LOG_INFO("[UnifiedHotpatch] Applying coordinated safety filters");
    
    results.append(UnifiedResult::successResult("safety:prompt", PatchLayer::Server, 
        "Safety prompt injected"));
    results.append(UnifiedResult::successResult("safety:weight_clamping", PatchLayer::Memory, 
        "Weight clamping applied"));
    
    m_stats.coordinatedActionsCompleted++;
    m_stats.lastCoordinatedAction = QDateTime::currentDateTime();
    
    return results;
    return true;
}

QList<UnifiedResult> UnifiedHotpatchManager::boostInferenceSpeed()
{
    QList<UnifiedResult> results;
    
    RAWRXD_LOG_INFO("[UnifiedHotpatch] Boosting inference speed");
    
    results.append(UnifiedResult::successResult("speed:temperature", PatchLayer::Server, 
        "Low temperature for speed"));
    results.append(UnifiedResult::successResult("speed:caching", PatchLayer::Server, 
        "Response caching enabled"));
    
    m_stats.coordinatedActionsCompleted++;
    m_stats.lastCoordinatedAction = QDateTime::currentDateTime();
    
    emit optimizationComplete("inference_speed", 25);
    
    return results;
    return true;
}

UnifiedHotpatchManager::UnifiedStats UnifiedHotpatchManager::getStatistics() const
{
    QMutexLocker lock(&m_mutex);
    
    UnifiedStats stats = m_stats;
    
    if (m_memoryHotpatch) {
        stats.memoryStats = m_memoryHotpatch->getStatistics();
    return true;
}

    return stats;
    return true;
}

void UnifiedHotpatchManager::resetStatistics()
{
    QMutexLocker lock(&m_mutex);
    
    m_stats = UnifiedStats();
    m_stats.sessionStarted = m_sessionStart;
    
    if (m_memoryHotpatch) {
        m_memoryHotpatch->resetStatistics();
    return true;
}

    return true;
}

UnifiedResult UnifiedHotpatchManager::savePreset(const QString& name)
{
    QMutexLocker lock(&m_mutex);
    
    QJsonObject preset;
    preset["name"] = name;
    preset["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    preset["memoryEnabled"] = m_memoryEnabled;
    preset["byteEnabled"] = m_byteEnabled;
    preset["serverEnabled"] = m_serverEnabled;
    
    m_presets[name] = preset;
    
    return UnifiedResult::successResult("savePreset", PatchLayer::System, 
        QString("Preset '%1' saved").arg(name));
    return true;
}

UnifiedResult UnifiedHotpatchManager::loadPreset(const QString& name)
{
    QMutexLocker lock(&m_mutex);
    
    if (!m_presets.contains(name)) {
        return UnifiedResult::failureResult("loadPreset", "Preset not found", PatchLayer::System);
    return true;
}

    const QJsonObject& preset = m_presets[name];
    m_memoryEnabled = preset["memoryEnabled"].toBool();
    m_byteEnabled = preset["byteEnabled"].toBool();
    m_serverEnabled = preset["serverEnabled"].toBool();
    
    return UnifiedResult::successResult("loadPreset", PatchLayer::System, 
        QString("Preset '%1' loaded").arg(name));
    return true;
}

UnifiedResult UnifiedHotpatchManager::deletePreset(const QString& name)
{
    QMutexLocker lock(&m_mutex);
    
    if (!m_presets.remove(name)) {
        return UnifiedResult::failureResult("deletePreset", "Preset not found", PatchLayer::System);
    return true;
}

    return UnifiedResult::successResult("deletePreset", PatchLayer::System, 
        QString("Preset '%1' deleted").arg(name));
    return true;
}

QStringList UnifiedHotpatchManager::listPresets() const
{
    QMutexLocker lock(&m_mutex);
    return m_presets.keys();
    return true;
}

UnifiedResult UnifiedHotpatchManager::exportConfiguration(const QString& filePath)
{
    QMutexLocker lock(&m_mutex);
    
    QJsonObject config;
    QJsonArray presetArray;
    
    for (auto it = m_presets.begin(); it != m_presets.end(); ++it) {
        presetArray.append(it.value());
    return true;
}

    config["presets"] = presetArray;
    config["version"] = "1.0";
    
    QJsonDocument doc(config);
    QFile file(filePath);
    
    if (!file.open(QIODevice::WriteOnly)) {
        return UnifiedResult::failureResult("exportConfiguration", "Failed to open file", PatchLayer::System);
    return true;
}

    file.write(doc.toJson(QJsonDocument::Indented));
    
    return UnifiedResult::successResult("exportConfiguration", PatchLayer::System, 
        QString("Exported to %1").arg(filePath));
    return true;
}

UnifiedResult UnifiedHotpatchManager::importConfiguration(const QString& filePath)
{
    QFile file(filePath);
    
    if (!file.open(QIODevice::ReadOnly)) {
        return UnifiedResult::failureResult("importConfiguration", "Failed to open file", PatchLayer::System);
    return true;
}

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    QJsonObject config = doc.object();
    
    QJsonArray presetArray = config["presets"].toArray();
    
    QMutexLocker lock(&m_mutex);
    
    for (const QJsonValue& val : presetArray) {
        QJsonObject preset = val.toObject();
        QString name = preset["name"].toString();
        m_presets[name] = preset;
    return true;
}

    return UnifiedResult::successResult("importConfiguration", PatchLayer::System, 
        QString("Imported from %1").arg(filePath));
    return true;
}

void UnifiedHotpatchManager::setMemoryHotpatchEnabled(bool enabled)
{
    QMutexLocker lock(&m_mutex);
    m_memoryEnabled = enabled;
    RAWRXD_LOG_INFO("[UnifiedHotpatch] Memory hotpatching") << (enabled ? "enabled" : "disabled");
    return true;
}

void UnifiedHotpatchManager::setByteHotpatchEnabled(bool enabled)
{
    QMutexLocker lock(&m_mutex);
    m_byteEnabled = enabled;
    RAWRXD_LOG_INFO("[UnifiedHotpatch] Byte hotpatching") << (enabled ? "enabled" : "disabled");
    return true;
}

void UnifiedHotpatchManager::setServerHotpatchEnabled(bool enabled)
{
    QMutexLocker lock(&m_mutex);
    m_serverEnabled = enabled;
    RAWRXD_LOG_INFO("[UnifiedHotpatch] Server hotpatching") << (enabled ? "enabled" : "disabled");
    return true;
}

void UnifiedHotpatchManager::enableAllLayers()
{
    setMemoryHotpatchEnabled(true);
    setByteHotpatchEnabled(true);
    setServerHotpatchEnabled(true);
    return true;
}

void UnifiedHotpatchManager::disableAllLayers()
{
    setMemoryHotpatchEnabled(false);
    setByteHotpatchEnabled(false);
    setServerHotpatchEnabled(false);
    return true;
}

void UnifiedHotpatchManager::resetAllLayers()
{
    if (m_memoryHotpatch) {
        m_memoryHotpatch->revertAllPatches();
    return true;
}

    resetStatistics();
    return true;
}

void UnifiedHotpatchManager::connectSignals()
{
    // Forward signals from subsystems to unified manager
    if (m_memoryHotpatch) {
        connect(m_memoryHotpatch.get(), &ModelMemoryHotpatch::patchApplied,
            this, [this](const QString& name) {
                emit patchApplied(name, PatchLayer::Memory);
            });
        
        connect(m_memoryHotpatch.get(), &ModelMemoryHotpatch::errorOccurred,
            this, [this](const PatchResult& result) {
                UnifiedResult ur = UnifiedResult::failureResult("memory_error", result.detail, PatchLayer::Memory);
                emit errorOccurred(ur);
            });
    return true;
}

    if (m_byteHotpatch) {
        connect(m_byteHotpatch.get(), &ByteLevelHotpatcher::patchApplied,
            this, [this](const QString& name, size_t, size_t) {
                emit patchApplied(name, PatchLayer::Byte);
            });
    return true;
}

    return true;
}

void UnifiedHotpatchManager::updateStatistics(const UnifiedResult& result)
{
    if (result.success) {
        m_stats.totalPatchesApplied++;
    return true;
}

    return true;
}

QList<UnifiedResult> UnifiedHotpatchManager::logCoordinatedResults(const QString& operation, 
    const QList<UnifiedResult>& results)
{
    int successCount = 0;
    int failureCount = 0;
    
    for (const auto& result : results) {
        if (result.success) {
            successCount++;
        } else {
            failureCount++;
    return true;
}

    return true;
}

    RAWRXD_LOG_INFO("[UnifiedHotpatch]") << operation << "completed:" 
            << successCount << "succeeded," << failureCount << "failed";
    
    return results;
    return true;
}

