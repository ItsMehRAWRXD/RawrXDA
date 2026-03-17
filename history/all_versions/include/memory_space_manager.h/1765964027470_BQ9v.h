#pragma once

#include <QObject>
#include <QMap>
#include <QVariant>
#include <QStringList>
#include <QJsonObject>

// Manages persistent memory space for agent conversations/preferences.
class MemorySpaceManager : public QObject
{
    Q_OBJECT
public:
    static MemorySpaceManager& instance();

    bool isEnabled() const;
    void setEnabled(bool enabled);

    qint64 limitBytes() const;
    void setLimitBytes(qint64 bytes);

    // Persist the in-memory map to disk, enforcing size limits.
    void persist(const QMap<QString, QVariant>& memoryMap);

    // Load the stored memory into a simple string map.
    QMap<QString, QString> loadMemory() const;

    // Utility helpers for UI/ops.
    QStringList listKeys() const;
    bool deleteKey(const QString& key);
    void clearAll();
    qint64 currentSizeBytes() const;

private:
    MemorySpaceManager();
    QString memoryFilePath() const;
    QString settingsFilePath() const;
    QJsonObject readJson() const;
    bool writeJson(const QJsonObject& obj) const;
    void ensureConfig();

    bool m_enabled = false;
    qint64 m_limitBytes = 134217728; // Default 128 MB
};
