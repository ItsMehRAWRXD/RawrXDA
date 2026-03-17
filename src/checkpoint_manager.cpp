#include "checkpoint_manager.h"
#include <fstream>
#include <iostream>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <filesystem>
#include <algorithm>
#include <random>
#include <cstring>

namespace fs = std::filesystem;

CheckpointManager::CheckpointManager(void* parent) {
    (void)parent;
}

CheckpointManager::~CheckpointManager() {
}

bool CheckpointManager::initialize(const std::string& checkpointDir, int maxCheckpoints) {
    m_checkpointDir = checkpointDir;
    m_maxCheckpoints = maxCheckpoints;

    try {
        if (!fs::exists(m_checkpointDir)) {
            if (!fs::create_directories(m_checkpointDir)) {
                return false;
            }
        }
        
        // In a real implementation, we would scan the directory and populate m_checkpointIndex.
        // For Batch 1, we assume we start with an empty index or it's handled by other methods.
        m_checkpointIndex.clear();
        m_checkpointCounter = 0;
        m_bestCheckpointId = "";
        
        return true;
    } catch (const std::exception& e) {
        (void)e;
        return false;
    }
}

bool CheckpointManager::isInitialized() const {
    return !m_checkpointDir.empty() && fs::exists(m_checkpointDir);
}

std::string CheckpointManager::saveCheckpoint(const CheckpointMetadata& metadata,
                                           const CheckpointState& state,
                                           CompressionLevel compress) {
    if (!isInitialized()) return "";

    CheckpointMetadata meta = metadata;
    if (meta.checkpointId.empty()) {
        meta.checkpointId = generateCheckpointId();
    }
    
    if (meta.timestamp == 0) {
        meta.timestamp = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    }

    std::string checkpointId = meta.checkpointId;
    std::string filePath = (fs::path(m_checkpointDir) / (checkpointId + ".ckpt")).string();

    CheckpointIndex idx;
    idx.checkpointId = checkpointId;
    idx.filePath = filePath;
    idx.metadata = meta;
    idx.checkpointNumber = ++m_checkpointCounter;

    // Temporarily add to index to allow writeCheckpointToDisk to find it
    m_checkpointIndex.push_back(idx);

    if (!writeCheckpointToDisk(checkpointId, state, compress)) {
        m_checkpointIndex.pop_back();
        m_checkpointCounter--;
        return "";
    }

    if (meta.isBestModel) {
        m_bestCheckpointId = checkpointId;
    }

    if (m_savedCb) {
        m_savedCb(m_savedCtx, checkpointId.c_str(), meta.epoch, meta.step);
    }

    return checkpointId;
}

std::string CheckpointManager::quickSaveCheckpoint(const CheckpointMetadata& metadata,
                                                const CheckpointState& state) {
    return saveCheckpoint(metadata, state, CompressionLevel::None);
}

std::string CheckpointManager::saveModelWeights(const CheckpointMetadata& metadata,
                                             const std::vector<uint8_t>& modelWeights,
                                             CompressionLevel compress) {
    CheckpointState state;
    state.modelWeights = modelWeights;
    return saveCheckpoint(metadata, state, compress);
}

bool CheckpointManager::loadCheckpoint(const std::string& checkpointId, CheckpointState& state) {
    if (!isInitialized()) return false;

    if (!readCheckpointFromDisk(checkpointId, state)) {
        return false;
    }

    if (m_loadedCb) {
        m_loadedCb(m_loadedCtx, checkpointId.c_str());
    }

    return true;
}

std::string CheckpointManager::loadLatestCheckpoint(CheckpointState& state) {
    if (m_checkpointIndex.empty()) return "";

    auto it = std::max_element(m_checkpointIndex.begin(), m_checkpointIndex.end(),
                               [](const CheckpointIndex& a, const CheckpointIndex& b) {
                                   if (a.metadata.epoch != b.metadata.epoch)
                                       return a.metadata.epoch < b.metadata.epoch;
                                   if (a.metadata.step != b.metadata.step)
                                       return a.metadata.step < b.metadata.step;
                                   return a.metadata.timestamp < b.metadata.timestamp;
                               });

    if (loadCheckpoint(it->checkpointId, state)) {
        return it->checkpointId;
    }
    return "";
}

std::string CheckpointManager::loadBestCheckpoint(CheckpointState& state) {
    if (m_bestCheckpointId.empty()) {
        for (const auto& idx : m_checkpointIndex) {
            if (idx.metadata.isBestModel) {
                m_bestCheckpointId = idx.checkpointId;
                break;
            }
        }
    }

    if (m_bestCheckpointId.empty()) return "";

    if (loadCheckpoint(m_bestCheckpointId, state)) {
        return m_bestCheckpointId;
    }
    return "";
}

std::string CheckpointManager::loadCheckpointFromEpoch(int epoch, CheckpointState& state) {
    for (const auto& idx : m_checkpointIndex) {
        if (idx.metadata.epoch == epoch) {
            if (loadCheckpoint(idx.checkpointId, state)) {
                return idx.checkpointId;
            }
        }
    }
    return "";
}

CheckpointManager::CheckpointMetadata CheckpointManager::getCheckpointMetadata(const std::string& checkpointId) const {
    for (const auto& idx : m_checkpointIndex) {
        if (idx.checkpointId == checkpointId) {
            return idx.metadata;
        }
    }
    return CheckpointMetadata();
}

// ----------------------------------------------------------------------------
// Private Helpers
// ----------------------------------------------------------------------------

std::string CheckpointManager::generateCheckpointId() {
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    struct tm buf;
    
#ifdef _WIN32
    localtime_s(&buf, &in_time_t);
#else
    localtime_r(&in_time_t, &buf);
#endif

    std::stringstream ss;
    ss << "ckpt_" << std::put_time(&buf, "%Y%m%d_%H%M%S");
    
    static std::mt19937 rng(static_cast<unsigned int>(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::uniform_int_distribution<int> dist(0, 999);
    ss << "_" << std::setw(3) << std::setfill('0') << dist(rng);
    
    return ss.str();
}

std::vector<uint8_t> CheckpointManager::compressState(const std::vector<uint8_t>& data, CompressionLevel level) {
    // Basic RLE or Zlib could be here. Prompt suggests simple copy if no lib available.
    (void)level;
    return data;
}

std::vector<uint8_t> CheckpointManager::decompressState(const std::vector<uint8_t>& data) {
    return data;
}

bool CheckpointManager::writeCheckpointToDisk(const std::string& checkpointId,
                                            const CheckpointState& state,
                                            CompressionLevel compress) {
    auto it = std::find_if(m_checkpointIndex.begin(), m_checkpointIndex.end(),
                           [&](const CheckpointIndex& idx) { return idx.checkpointId == checkpointId; });

    if (it == m_checkpointIndex.end()) return false;

    std::string path = it->filePath;
    std::ofstream out(path, std::ios::binary);
    if (!out) return false;

    // Header
    const char magic[4] = {'C', 'K', 'P', 'T'};
    out.write(magic, 4);
    uint32_t version = 1;
    out.write(reinterpret_cast<const char*>(&version), 4);

    auto writeVec = [&](const std::vector<uint8_t>& vec) {
        uint64_t size = vec.size();
        out.write(reinterpret_cast<const char*>(&size), sizeof(size));
        if (size > 0) {
            out.write(reinterpret_cast<const char*>(vec.data()), size);
        }
    };

    auto writeString = [&](const std::string& s) {
        uint64_t size = s.size();
        out.write(reinterpret_cast<const char*>(&size), sizeof(size));
        if (size > 0) {
            out.write(s.data(), size);
        }
    };

    // Serialize Metadata
    const auto& meta = it->metadata;
    writeString(meta.checkpointId);
    out.write(reinterpret_cast<const char*>(&meta.epoch), sizeof(meta.epoch));
    out.write(reinterpret_cast<const char*>(&meta.step), sizeof(meta.step));
    out.write(reinterpret_cast<const char*>(&meta.timestamp), sizeof(meta.timestamp));
    out.write(reinterpret_cast<const char*>(&meta.validationLoss), sizeof(meta.validationLoss));
    out.write(reinterpret_cast<const char*>(&meta.trainLoss), sizeof(meta.trainLoss));
    out.write(reinterpret_cast<const char*>(&meta.accuracy), sizeof(meta.accuracy));
    out.write(reinterpret_cast<const char*>(&meta.wallclockTime), sizeof(meta.wallclockTime));
    out.write(reinterpret_cast<const char*>(&meta.modelSize), sizeof(meta.modelSize));
    writeString(meta.modelArchitecture);
    writeString(meta.hyperparameters);
    writeString(meta.datasetVersion);
    uint8_t isBest = meta.isBestModel ? 1 : 0;
    out.write(reinterpret_cast<const char*>(&isBest), 1);
    writeString(meta.notes);

    // Serialize State components
    writeVec(compressState(state.modelWeights, compress));
    writeVec(compressState(state.optimizerState, compress));
    writeVec(compressState(state.schedulerState, compress));
    writeVec(compressState(state.trainingState, compress));
    writeString(state.config);

    return out.good();
}

bool CheckpointManager::readCheckpointFromDisk(const std::string& checkpointId, CheckpointState& state) {
    std::string path;
    auto it = std::find_if(m_checkpointIndex.begin(), m_checkpointIndex.end(),
                           [&](const CheckpointIndex& idx) { return idx.checkpointId == checkpointId; });

    if (it != m_checkpointIndex.end()) {
        path = it->filePath;
    } else {
        path = (fs::path(m_checkpointDir) / (checkpointId + ".ckpt")).string();
    }

    std::ifstream in(path, std::ios::binary);
    if (!in) return false;

    char magic[4];
    in.read(magic, 4);
    if (std::memcmp(magic, "CKPT", 4) != 0) return false;

    uint32_t version;
    in.read(reinterpret_cast<char*>(&version), 4);
    if (version != 1) return false;

    auto readString = [&]() -> std::string {
        uint64_t size;
        in.read(reinterpret_cast<char*>(&size), sizeof(size));
        if (size == 0) return "";
        std::string s(static_cast<size_t>(size), '\0');
        in.read(&s[0], size);
        return s;
    };

    auto readVec = [&]() -> std::vector<uint8_t> {
        uint64_t size;
        in.read(reinterpret_cast<char*>(&size), sizeof(size));
        if (size == 0) return {};
        std::vector<uint8_t> v(static_cast<size_t>(size));
        in.read(reinterpret_cast<char*>(v.data()), size);
        return v;
    };

    // Metadata reading (consume to keep file pointer correct)
    CheckpointMetadata meta;
    meta.checkpointId = readString();
    in.read(reinterpret_cast<char*>(&meta.epoch), sizeof(meta.epoch));
    in.read(reinterpret_cast<char*>(&meta.step), sizeof(meta.step));
    in.read(reinterpret_cast<char*>(&meta.timestamp), sizeof(meta.timestamp));
    in.read(reinterpret_cast<char*>(&meta.validationLoss), sizeof(meta.validationLoss));
    in.read(reinterpret_cast<char*>(&meta.trainLoss), sizeof(meta.trainLoss));
    in.read(reinterpret_cast<char*>(&meta.accuracy), sizeof(meta.accuracy));
    in.read(reinterpret_cast<char*>(&meta.wallclockTime), sizeof(meta.wallclockTime));
    in.read(reinterpret_cast<char*>(&meta.modelSize), sizeof(meta.modelSize));
    meta.modelArchitecture = readString();
    meta.hyperparameters = readString();
    meta.datasetVersion = readString();
    uint8_t isBest;
    in.read(reinterpret_cast<char*>(&isBest), 1);
    meta.isBestModel = (isBest != 0);
    meta.notes = readString();

    // Now read state
    state.modelWeights = decompressState(readVec());
    state.optimizerState = decompressState(readVec());
    state.schedulerState = decompressState(readVec());
    state.trainingState = decompressState(readVec());
    state.config = readString();

    return in.good();
}
