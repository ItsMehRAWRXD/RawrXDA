#include "checkpoint_manager.h"
#include "codec/compression.h"
#include <QByteArray>
#include <QBuffer>
#include <QDataStream>
#include <QDateTime>
#include <QJsonDocument>
#include <QUuid>

// Internal helpers implemented in checkpoint_manager_impl.cpp
static void initializeCheckpointManager();
static QString saveCheckpointReal(const QJsonObject& metadata, const QByteArray& stateData, int compressionLevel);
static bool loadCheckpointReal(const QString& checkpointId, QByteArray& stateData);
static bool deleteCheckpointReal(const QString& checkpointId);
static QStringList listCheckpointsReal();
static QJsonObject getCheckpointInfoReal(const QString& checkpointId);

namespace {
int toCompressionLevel(CheckpointManager::CompressionLevel level) {
    switch (level) {
        case CheckpointManager::CompressionLevel::None: return 0;
        case CheckpointManager::CompressionLevel::Low: return 1;
        case CheckpointManager::CompressionLevel::Medium: return 3;
        case CheckpointManager::CompressionLevel::High: return 5;
        case CheckpointManager::CompressionLevel::Maximum: return 7;
    }
    return 0;
}

QByteArray serializeState(const CheckpointManager::CheckpointState& state) {
    QByteArray buffer;
    QDataStream out(&buffer, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_6_5);
    out << state.modelWeights
        << state.optimizerState
        << state.schedulerState
        << state.trainingState
        << QJsonDocument(state.config).toJson(QJsonDocument::Compact);
    return buffer;
}

CheckpointManager::CheckpointState deserializeState(const QByteArray& data) {
    CheckpointManager::CheckpointState state;
    QDataStream in(data);
    in.setVersion(QDataStream::Qt_6_5);
    QByteArray configJson;
    in >> state.modelWeights
       >> state.optimizerState
       >> state.schedulerState
       >> state.trainingState
       >> configJson;
    state.config = QJsonDocument::fromJson(configJson).object();
    return state;
}
}

CheckpointManager::CheckpointManager(QObject* parent) : QObject(parent) {
    initializeCheckpointManager();
}

CheckpointManager::~CheckpointManager() {}

QString CheckpointManager::saveCheckpoint(const CheckpointMetadata& metadata, const CheckpointState& state, CompressionLevel compress) {
    QJsonObject metaObj;
    metaObj["checkpointId"] = metadata.checkpointId.isEmpty() ? generateCheckpointId() : metadata.checkpointId;
    metaObj["epoch"] = metadata.epoch;
    metaObj["step"] = metadata.step;
    metaObj["timestamp"] = static_cast<qint64>(metadata.timestamp > 0 ? metadata.timestamp : QDateTime::currentSecsSinceEpoch());
    metaObj["validationLoss"] = metadata.validationLoss;
    metaObj["trainLoss"] = metadata.trainLoss;
    metaObj["accuracy"] = metadata.accuracy;
    metaObj["wallclockTime"] = metadata.wallclockTime;
    metaObj["modelSize"] = metadata.modelSize;
    metaObj["modelArchitecture"] = metadata.modelArchitecture;
    metaObj["hyperparameters"] = metadata.hyperparameters;
    metaObj["datasetVersion"] = metadata.datasetVersion;
    metaObj["isBestModel"] = metadata.isBestModel;
    metaObj["notes"] = metadata.notes;

    QByteArray serialized = serializeState(state);
    QByteArray compressedState = compressState(serialized, compress);

    const int level = toCompressionLevel(compress);
    QString id = saveCheckpointReal(metaObj, compressedState, level);
    return id;
}

bool CheckpointManager::loadCheckpoint(const QString& checkpointId, CheckpointState& state) {
    QByteArray compressed;
    if (!loadCheckpointReal(checkpointId, compressed)) {
        return false;
    }
    QByteArray raw = decompressState(compressed);
    state = deserializeState(raw);
    return true;
}

bool CheckpointManager::deleteCheckpoint(const QString& checkpointId) {
    return deleteCheckpointReal(checkpointId);
}

QStringList CheckpointManager::listCheckpoints() const {
    return listCheckpointsReal();
}

QJsonObject CheckpointManager::getCheckpointInfo(const QString& checkpointId) const {
    return getCheckpointInfoReal(checkpointId);
}

bool CheckpointManager::rollbackToCheckpoint(const QString& checkpointId) {
    CheckpointState state;
    return loadCheckpoint(checkpointId, state);
}

QString CheckpointManager::generateCheckpointId() {
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

QByteArray CheckpointManager::compressState(const QByteArray& data, CompressionLevel level) {
    switch (level) {
        case CompressionLevel::None: {
            return data;
        }
        case CompressionLevel::Low:
        case CompressionLevel::Medium: {
            bool ok = false;
            QByteArray out = codec::deflate(data, &ok);
            return ok ? out : QByteArray();
        }
        case CompressionLevel::High:
        case CompressionLevel::Maximum: {
            bool ok = false;
            QByteArray out = codec::deflate_brutal_masm(data, &ok);
            return ok ? out : QByteArray();
        }
    }
    return data;
}

QByteArray CheckpointManager::decompressState(const QByteArray& data) {
    bool ok = false;
    QByteArray out = codec::inflate(data, &ok);
    return ok ? out : QByteArray();
}

bool CheckpointManager::writeCheckpointToDisk(const QString& checkpointId, const CheckpointState& state, CompressionLevel compress) {
    QJsonObject meta;
    meta["checkpointId"] = checkpointId.isEmpty() ? generateCheckpointId() : checkpointId;
    QByteArray serialized = serializeState(state);
    QByteArray compressedState = compressState(serialized, compress);
    return !saveCheckpointReal(meta, compressedState, toCompressionLevel(compress)).isEmpty();
}

bool CheckpointManager::readCheckpointFromDisk(const QString& checkpointId, CheckpointState& state) {
    return loadCheckpoint(checkpointId, state);
}