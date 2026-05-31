#pragma once
// ============================================================================
// model_index.h — Local Model Registry (JSON Index)
// ============================================================================
// Persistent JSON-based registry that tracks downloaded models.
//   - Stored at %APPDATA%\RawrXD\models\index.json
//   - Model name → file path mapping
//   - Metadata: quantization, params, architecture, SHA256
//   - Thread-safe read/write with mutex
// ============================================================================

#ifndef RAWRXD_MODEL_INDEX_H
#define RAWRXD_MODEL_INDEX_H

#include <string>
#include <vector>
#include <mutex>
#include <cstdint>

namespace RawrXD {

// -------------------------------------------------------
// Registry entry for a downloaded model
// -------------------------------------------------------
struct ModelEntry {
    std::string id;             // short id: "qwen32b-q4km"
    std::string name;           // human name: "Qwen2.5-Coder-32B-Instruct"
    std::string quantization;   // "Q4_K_M"
    std::string path;           // relative path from models root
    std::string absolutePath;   // full path on disk
    uint64_t    sizeBytes = 0;
    std::string sha256;
    std::string source;         // "hf://bartowski/..." or "ollama://llama3.2:3b" or URL
    std::string downloadedAt;   // ISO-8601 timestamp
    std::string architecture;   // "llama", "qwen2", etc.
    std::string tags;           // comma-separated user tags
    bool        active = false; // currently loaded model
};

// -------------------------------------------------------
// ModelIndex — persistent JSON registry
// -------------------------------------------------------
class ModelIndex {
public:
    ModelIndex();
    ~ModelIndex();

    // Initialize: determines storage path, loads index if exists
    bool Initialize();
    bool Initialize(const std::string& customBasePath);

    // ---- CRUD operations ----
    bool AddModel(const ModelEntry& entry);
    bool RemoveModel(const std::string& id);
    bool UpdateModel(const ModelEntry& entry);
    bool SetActive(const std::string& id);

    // ---- Queries ----
    bool GetModel(const std::string& id, ModelEntry& out) const;
    bool GetModelByPath(const std::string& absPath, ModelEntry& out) const;
    std::vector<ModelEntry> GetAllModels() const;
    const ModelEntry* GetActiveModel() const;
    bool HasModel(const std::string& id) const;

    // ---- Search ----
    std::vector<ModelEntry> Search(const std::string& query) const;

    // ---- Persistence ----
    bool Save() const;
    bool Load();

    // ---- Paths ----
    std::string GetModelsBasePath() const { return m_basePath; }
    std::string GetIndexFilePath() const;

    // ---- Generate a unique ID from model name + quant ----
    static std::string GenerateId(const std::string& name, const std::string& quant);

    // ---- ISO-8601 timestamp ----
    static std::string NowISO8601();

private:
    mutable std::mutex m_mutex;
    std::string m_basePath;  // %APPDATA%\RawrXD\models
    std::vector<ModelEntry> m_models;
};

} // namespace RawrXD

#endif // RAWRXD_MODEL_INDEX_H
