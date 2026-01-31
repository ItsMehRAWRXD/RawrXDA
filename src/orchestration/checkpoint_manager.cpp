#include "checkpoint_manager.h"
#include "codec/compression.h"


// Internal helpers implemented in checkpoint_manager_impl.cpp
static void initializeCheckpointManager();
static std::string saveCheckpointReal(const void*& metadata, const std::vector<uint8_t>& stateData, int compressionLevel);
static bool loadCheckpointReal(const std::string& checkpointId, std::vector<uint8_t>& stateData);
static bool deleteCheckpointReal(const std::string& checkpointId);
static std::vector<std::string> listCheckpointsReal();
static void* getCheckpointInfoReal(const std::string& checkpointId);

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

std::vector<uint8_t> serializeState(const CheckpointManager::CheckpointState& state) {
    std::vector<uint8_t> buffer;
    QDataStream out(&buffer, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_6_5);
    out << state.modelWeights
        << state.optimizerState
        << state.schedulerState
        << state.trainingState
        << void*(state.config).toJson(void*::Compact);
    return buffer;
}

CheckpointManager::CheckpointState deserializeState(const std::vector<uint8_t>& data) {
    CheckpointManager::CheckpointState state;
    QDataStream in(data);
    in.setVersion(QDataStream::Qt_6_5);
    std::vector<uint8_t> configJson;
    in >> state.modelWeights
       >> state.optimizerState
       >> state.schedulerState
       >> state.trainingState
       >> configJson;
    state.config = void*::fromJson(configJson).object();
    return state;
}
}

CheckpointManager::CheckpointManager(void* parent) : void(parent) {
    initializeCheckpointManager();
}

CheckpointManager::~CheckpointManager() {}

std::string CheckpointManager::saveCheckpoint(const CheckpointMetadata& metadata, const CheckpointState& state, CompressionLevel compress) {
    void* metaObj;
    metaObj["checkpointId"] = metadata.checkpointId.isEmpty() ? generateCheckpointId() : metadata.checkpointId;
    metaObj["epoch"] = metadata.epoch;
    metaObj["step"] = metadata.step;
    metaObj["timestamp"] = static_cast<qint64>(metadata.timestamp > 0 ? metadata.timestamp : std::chrono::system_clock::time_point::currentSecsSinceEpoch());
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

    std::vector<uint8_t> serialized = serializeState(state);
    std::vector<uint8_t> compressedState = compressState(serialized, compress);

    const int level = toCompressionLevel(compress);
    std::string id = saveCheckpointReal(metaObj, compressedState, level);
    return id;
}

bool CheckpointManager::loadCheckpoint(const std::string& checkpointId, CheckpointState& state) {
    std::vector<uint8_t> compressed;
    if (!loadCheckpointReal(checkpointId, compressed)) {
        return false;
    }
    std::vector<uint8_t> raw = decompressState(compressed);
    state = deserializeState(raw);
    return true;
}

bool CheckpointManager::deleteCheckpoint(const std::string& checkpointId) {
    return deleteCheckpointReal(checkpointId);
}

std::vector<std::string> CheckpointManager::listCheckpoints() const {
    return listCheckpointsReal();
}

void* CheckpointManager::getCheckpointInfo(const std::string& checkpointId) const {
    return getCheckpointInfoReal(checkpointId);
}

bool CheckpointManager::rollbackToCheckpoint(const std::string& checkpointId) {
    CheckpointState state;
    return loadCheckpoint(checkpointId, state);
}

std::string CheckpointManager::generateCheckpointId() {
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

std::vector<uint8_t> CheckpointManager::compressState(const std::vector<uint8_t>& data, CompressionLevel level) {
    switch (level) {
        case CompressionLevel::None: {
            return data;
        }
        case CompressionLevel::Low:
        case CompressionLevel::Medium: {
            bool ok = false;
            std::vector<uint8_t> out = codec::deflate(data, &ok);
            return ok ? out : std::vector<uint8_t>();
        }
        case CompressionLevel::High:
        case CompressionLevel::Maximum: {
            bool ok = false;
            std::vector<uint8_t> out = codec::deflate_brutal_masm(data, &ok);
            return ok ? out : std::vector<uint8_t>();
        }
    }
    return data;
}

std::vector<uint8_t> CheckpointManager::decompressState(const std::vector<uint8_t>& data) {
    bool ok = false;
    std::vector<uint8_t> out = codec::inflate(data, &ok);
    return ok ? out : std::vector<uint8_t>();
}

bool CheckpointManager::writeCheckpointToDisk(const std::string& checkpointId, const CheckpointState& state, CompressionLevel compress) {
    void* meta;
    meta["checkpointId"] = checkpointId.isEmpty() ? generateCheckpointId() : checkpointId;
    std::vector<uint8_t> serialized = serializeState(state);
    std::vector<uint8_t> compressedState = compressState(serialized, compress);
    return !saveCheckpointReal(meta, compressedState, toCompressionLevel(compress)).isEmpty();
}

bool CheckpointManager::readCheckpointFromDisk(const std::string& checkpointId, CheckpointState& state) {
    return loadCheckpoint(checkpointId, state);
}
