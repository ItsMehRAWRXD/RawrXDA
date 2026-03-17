#pragma once

// <QObject> removed (Qt-free build)
// <QString> removed (Qt-free build)
// <QJsonObject> removed (Qt-free build)
// Qt include removed (Qt-free build)
// <QFile> removed (Qt-free build)
// <QDir> removed (Qt-free build)
#include <vector>
#include <map>

class AdvancedCheckpointManager  {
    /* Q_OBJECT */

public:
    explicit AdvancedCheckpointManager(QObject* parent = nullptr);
    ~AdvancedCheckpointManager();

    // Checkpoint operations
    QString createCheckpoint(const QJsonObject& state, const QString& description = "");
    bool restoreCheckpoint(const QString& checkpointId);
    bool deleteCheckpoint(const QString& checkpointId);
    QJsonObject getCheckpointInfo(const QString& checkpointId) const;

    // Checkpoint management
    QJsonArray listCheckpoints() const;
    bool pruneOldCheckpoints(int keepCount = 10);
    QJsonObject getCheckpointStats() const;

    // Compression and encryption
    void setCompressionEnabled(bool enabled);
    void setEncryptionEnabled(bool enabled, const QString& key = "");

    // Distributed checkpointing
    bool syncWithRemote(const QString& remoteUrl);
    bool backupToCloud(const QString& cloudConfig);

signals:
    void checkpointCreated(const QString& checkpointId);
    void checkpointRestored(const QString& checkpointId);
    void checkpointDeleted(const QString& checkpointId);
    void syncCompleted(bool success);

private:
    QString m_checkpointDir;
    bool m_compressionEnabled;
    bool m_encryptionEnabled;
    QString m_encryptionKey;
    std::map<QString, QJsonObject> m_checkpoints;
};
