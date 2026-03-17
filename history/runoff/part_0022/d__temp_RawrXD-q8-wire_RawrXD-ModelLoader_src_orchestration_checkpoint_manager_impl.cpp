#include "checkpoint_manager.h"
#include "codec/compression.h"
#include <QString>
#include <QJsonObject>
#include <QJsonDocument>
#include <QByteArray>
#include <QFile>
#include <QDir>
#include <QDateTime>
#include <QCryptographicHash>
#include <QStandardPaths>
#include <QMap>

// Real checkpoint manager implementation with file I/O and compression

static QString s_checkpointDir;
static QMap<QString, QJsonObject> s_checkpointRegistry;

void initializeCheckpointManager() {
    s_checkpointDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/checkpoints";
    QDir dir;
    if (!dir.exists(s_checkpointDir)) {
        dir.mkpath(s_checkpointDir);
    }
}

QString saveCheckpointReal(const QJsonObject& metadata, const QByteArray& stateData, int compressionLevel) {
    if (s_checkpointDir.isEmpty()) {
        initializeCheckpointManager();
    }
    
    // Generate unique ID
    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss");
    QString random = QString::number(QDateTime::currentMSecsSinceEpoch() % 10000);
    QString checkpointId = QString("ckpt_%1_%2").arg(timestamp, random);
    
    // Compress state data
    QByteArray compressedData;
    bool success = false;
    
    if (compressionLevel == 0) {
        compressedData = stateData;
    } else if (compressionLevel < 5) {
        compressedData = codec::deflate(stateData, &success);
    } else {
        compressedData = codec::deflate_brutal_masm(stateData, &success);
    }
    
    // Write to disk
    QString filePath = s_checkpointDir + "/" + checkpointId + ".ckpt";
    QFile file(filePath);
    
    if (file.open(QIODevice::WriteOnly)) {
        file.write(compressedData);
        file.close();
        
        // Save metadata
        QJsonObject metaWithId = metadata;
        metaWithId["id"] = checkpointId;
        metaWithId["filePath"] = filePath;
        metaWithId["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
        metaWithId["compressedSize"] = static_cast<qint64>(compressedData.size());
        metaWithId["originalSize"] = static_cast<qint64>(stateData.size());
        
        s_checkpointRegistry[checkpointId] = metaWithId;
        
        return checkpointId;
    }
    
    return QString();
}

bool loadCheckpointReal(const QString& checkpointId, QByteArray& stateData) {
    if (!s_checkpointRegistry.contains(checkpointId)) {
        return false;
    }
    
    QJsonObject metadata = s_checkpointRegistry[checkpointId];
    QString filePath = metadata["filePath"].toString();
    
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }
    
    QByteArray compressedData = file.readAll();
    file.close();
    
    // Decompress
    bool success = false;
    stateData = codec::inflate(compressedData, &success);
    
    return success;
}

bool deleteCheckpointReal(const QString& checkpointId) {
    if (!s_checkpointRegistry.contains(checkpointId)) {
        return false;
    }
    
    QJsonObject metadata = s_checkpointRegistry[checkpointId];
    QString filePath = metadata["filePath"].toString();
    
    QFile file(filePath);
    if (file.exists() && file.remove()) {
        s_checkpointRegistry.remove(checkpointId);
        return true;
    }
    
    return false;
}

QStringList listCheckpointsReal() {
    return s_checkpointRegistry.keys();
}

QJsonObject getCheckpointInfoReal(const QString& checkpointId) {
    if (s_checkpointRegistry.contains(checkpointId)) {
        return s_checkpointRegistry[checkpointId];
    }
    return QJsonObject();
}
