#include "memory_space_manager.h"
#include <QStandardPaths>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QDebug>

MemorySpaceManager& MemorySpaceManager::instance()
{
    static MemorySpaceManager mgr;
    return mgr;
}

MemorySpaceManager::MemorySpaceManager()
{
    ensureConfig();
}

bool MemorySpaceManager::isEnabled() const
{
    return m_enabled;
}

void MemorySpaceManager::setEnabled(bool enabled)
{
    m_enabled = enabled;
    ensureConfig();
    // Persist setting
    QSettings settings(settingsFilePath(), QSettings::IniFormat);
    settings.setValue("memory/enabled", enabled);
    settings.sync();
}

qint64 MemorySpaceManager::limitBytes() const
{
    return m_limitBytes;
}

void MemorySpaceManager::setLimitBytes(qint64 bytes)
{
    m_limitBytes = bytes;
    ensureConfig();
    QSettings settings(settingsFilePath(), QSettings::IniFormat);
    settings.setValue("memory/limitBytes", bytes);
    settings.sync();
}

QString MemorySpaceManager::settingsFilePath() const
{
    QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(base);
    return base + "/RawrXD.ini";
}

QString MemorySpaceManager::memoryFilePath() const
{
    QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(base);
    return base + "/RawrXD_memory.json";
}

void MemorySpaceManager::ensureConfig()
{
    QSettings settings(settingsFilePath(), QSettings::IniFormat);
    m_enabled = settings.value("memory/enabled", m_enabled).toBool();
    m_limitBytes = settings.value("memory/limitBytes", m_limitBytes).toLongLong();
}

QJsonObject MemorySpaceManager::readJson() const
{
    QFile file(memoryFilePath());
    if (!file.exists()) {
        return QJsonObject();
    }
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "[MemorySpaceManager] Failed to open memory file" << memoryFilePath();
        return QJsonObject();
    }
    QByteArray data = file.readAll();
    file.close();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isObject()) {
        return QJsonObject();
    }
    return doc.object();
}

bool MemorySpaceManager::writeJson(const QJsonObject& obj) const
{
    QFile file(memoryFilePath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "[MemorySpaceManager] Failed to write memory file" << memoryFilePath();
        return false;
    }
    QJsonDocument doc(obj);
    file.write(doc.toJson(QJsonDocument::Compact));
    file.close();
    return true;
}

void MemorySpaceManager::persist(const QMap<QString, QVariant>& memoryMap)
{
    if (!m_enabled) {
        return;
    }

    QJsonObject obj;
    for (auto it = memoryMap.constBegin(); it != memoryMap.constEnd(); ++it) {
        obj[it.key()] = it.value().toString();
    }

    // Enforce size limit by pruning oldest keys if necessary
    QStringList keys = obj.keys();
    while (!keys.isEmpty()) {
        QJsonDocument sizeProbe(obj);
        qint64 bytes = sizeProbe.toJson(QJsonDocument::Compact).size();
        if (bytes <= m_limitBytes) break;
        QString dropKey = keys.takeFirst();
        obj.remove(dropKey);
        qWarning() << "[MemorySpaceManager] Pruned memory key to enforce limit:" << dropKey;
    }

    QJsonObject root;
    root["memory"] = obj;
    writeJson(root);
}

QMap<QString, QString> MemorySpaceManager::loadMemory() const
{
    QMap<QString, QString> result;
    QJsonObject root = readJson();
    if (!root.contains("memory")) return result;
    QJsonObject mem = root.value("memory").toObject();
    for (auto it = mem.constBegin(); it != mem.constEnd(); ++it) {
        result[it.key()] = it.value().toString();
    }
    return result;
}

QStringList MemorySpaceManager::listKeys() const
{
    return loadMemory().keys();
}

bool MemorySpaceManager::deleteKey(const QString& key)
{
    QJsonObject root = readJson();
    QJsonObject mem = root.value("memory").toObject();
    if (!mem.contains(key)) return false;
    mem.remove(key);
    root["memory"] = mem;
    return writeJson(root);
}

void MemorySpaceManager::clearAll()
{
    QFile::remove(memoryFilePath());
}

qint64 MemorySpaceManager::currentSizeBytes() const
{
    QFileInfo info(memoryFilePath());
    if (!info.exists()) return 0;
    return info.size();
}
