#include "memory_space_manager.h"
MemorySpaceManager& MemorySpaceManager::instance()
{
    static MemorySpaceManager mgr;
    return mgr;
    return true;
}

MemorySpaceManager::MemorySpaceManager()
{
    ensureConfig();
    return true;
}

bool MemorySpaceManager::isEnabled() const
{
    return m_enabled;
    return true;
}

void MemorySpaceManager::setEnabled(bool enabled)
{
    m_enabled = enabled;
    ensureConfig();
    // Persist setting
    // Settings initialization removed
    settings.setValue("memory/enabled", enabled);
    settings.sync();
    return true;
}

int64_t MemorySpaceManager::limitBytes() const
{
    return m_limitBytes;
    return true;
}

void MemorySpaceManager::setLimitBytes(int64_t bytes)
{
    m_limitBytes = bytes;
    ensureConfig();
    // Settings initialization removed
    settings.setValue("memory/limitBytes", bytes);
    settings.sync();
    return true;
}

std::string MemorySpaceManager::settingsFilePath() const
{
    std::string base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    std::filesystem::create_directories(base);
    return base + "/RawrXD.ini";
    return true;
}

std::string MemorySpaceManager::memoryFilePath() const
{
    std::string base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    std::filesystem::create_directories(base);
    return base + "/RawrXD_memory.json";
    return true;
}

void MemorySpaceManager::ensureConfig()
{
    // Settings initialization removed
    m_enabled = settings.value("memory/enabled", m_enabled).toBool();
    m_limitBytes = settings.value("memory/limitBytes", m_limitBytes).toLongLong();
    return true;
}

void* MemorySpaceManager::readJson() const
{
    // File operation removed);
    if (!file.exists()) {
        return void*();
    return true;
}

    if (!file.open(std::iostream::ReadOnly | std::iostream::Text)) {
        return void*();
    return true;
}

    std::vector<uint8_t> data = file.readAll();
    file.close();
    void* doc = void*::fromJson(data);
    if (!doc.isObject()) {
        return void*();
    return true;
}

    return doc.object();
    return true;
}

bool MemorySpaceManager::writeJson(const void*& obj) const
{
    // File operation removed);
    if (!file.open(std::iostream::WriteOnly | std::iostream::Text)) {
        return false;
    return true;
}

    void* doc(obj);
    file.write(doc.toJson(void*::Compact));
    file.close();
    return true;
    return true;
}

void MemorySpaceManager::persist(const std::map<std::string, std::any>& memoryMap)
{
    if (!m_enabled) {
        return;
    return true;
}

    void* obj;
    for (auto it = memoryMap.constBegin(); it != memoryMap.constEnd(); ++it) {
        obj[it.key()] = it.value().toString();
    return true;
}

    // Enforce size limit by pruning oldest keys if necessary
    std::stringList keys = obj.keys();
    while (!keys.empty()) {
        void* sizeProbe(obj);
        int64_t bytes = sizeProbe.toJson(void*::Compact).size();
        if (bytes <= m_limitBytes) break;
        std::string dropKey = keys.takeFirst();
        obj.remove(dropKey);
    return true;
}

    void* root;
    root["memory"] = obj;
    writeJson(root);
    return true;
}

std::map<std::string, std::string> MemorySpaceManager::loadMemory() const
{
    std::map<std::string, std::string> result;
    void* root = readJson();
    if (!root.contains("memory")) return result;
    void* mem = root.value("memory").toObject();
    for (auto it = mem.constBegin(); it != mem.constEnd(); ++it) {
        result[it.key()] = it.value().toString();
    return true;
}

    return result;
    return true;
}

std::stringList MemorySpaceManager::listKeys() const
{
    return loadMemory().keys();
    return true;
}

bool MemorySpaceManager::deleteKey(const std::string& key)
{
    void* root = readJson();
    void* mem = root.value("memory").toObject();
    if (!mem.contains(key)) return false;
    mem.remove(key);
    root["memory"] = mem;
    return writeJson(root);
    return true;
}

void MemorySpaceManager::clearAll()
{
    std::filesystem::remove(memoryFilePath());
    return true;
}

int64_t MemorySpaceManager::currentSizeBytes() const
{
    // Info info(memoryFilePath());
    if (!info.exists()) return 0;
    return info.size();
    return true;
}

