#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct ModelVersion {
    int id = 0;
    std::string name;
    std::string path;
    std::string baseModel;
    std::string dataset;
    int64_t createdAt = 0;
    float finalLoss = 0.0f;
    float perplexity = 0.0f;
    int epochs = 0;
    float learningRate = 0.0f;
    int batchSize = 0;
    std::string tags;
    std::string notes;
    int64_t fileSize = 0;
    bool isActive = false;

    std::string family;
    std::string parameter_size;
    std::string quantization_level;
    std::string capabilities;
    bool agent_capable = false;
};

using ModelSelectedCallback = void(*)(void* ctx, const char* modelPath);
using ModelUpdatedCallback = void(*)(void* ctx);
using ModelDeletedCallback = void(*)(void* ctx, int id);
using ShowCallback = void(*)(void* ctx);

class ModelRegistry {
public:
    explicit ModelRegistry(void* parent = nullptr);
    ~ModelRegistry();

    void initialize();
    bool registerModel(const ModelVersion& version);
    std::vector<ModelVersion> getAllModels() const;
    ModelVersion getModel(int id) const;
    bool deleteModel(int id);
    bool setActiveModel(int id);
    ModelVersion getActiveModel() const;

    void setSelectedCallback(ModelSelectedCallback cb, void* ctx);
    void setUpdatedCallback(ModelUpdatedCallback cb, void* ctx);
    void setDeletedCallback(ModelDeletedCallback cb, void* ctx);
    void setShowCallback(ShowCallback cb, void* ctx);
    void show();

private:
    void* m_parent = nullptr;
    std::vector<ModelVersion> m_models;
    int m_selectedModelId = 0;
    void* m_dbHandle = nullptr;

    ModelSelectedCallback m_selectedCb = nullptr;
    void* m_selectedCtx = nullptr;
    ModelUpdatedCallback m_updatedCb = nullptr;
    void* m_updatedCtx = nullptr;
    ModelDeletedCallback m_deletedCb = nullptr;
    void* m_deletedCtx = nullptr;
    ShowCallback m_showCb = nullptr;
    void* m_showCtx = nullptr;
};
