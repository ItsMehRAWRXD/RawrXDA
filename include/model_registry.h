#pragma once

#include <string>
#include <vector>
#include <functional>
#include <cstdint>

/**
 * @brief Model version for registry tracking (Win32/Qt-free)
 */
struct ModelVersion
{
    int id = 0;
    std::string name;
    std::string path;
    std::string baseModel;
    std::string dataset;
    int64_t createdAt = 0;
    float finalLoss = 0.f;
    float perplexity = 0.f;
    int epochs = 0;
    float learningRate = 0.f;
    int batchSize = 0;
    std::string tags;
    std::string notes;
    int64_t fileSize = 0;
    bool isActive = false;
};

/**
 * @brief Model registry for managing trained model versions (Win32, no Qt)
 *
 * Provides:
 * - In-memory or SQLite-backed storage of model metadata
 * - Version listing and comparison
 * - Rollback to previous versions
 * - Optional callbacks for selection/deletion/update
 */
class ModelRegistry
{
public:
    using UpdatedCallback = void (*)(void* ctx);
    using DeletedCallback = void (*)(void* ctx, int id);
    using SelectedCallback = void (*)(void* ctx, const char* modelPath);

    explicit ModelRegistry(void* parent = nullptr);
    ~ModelRegistry();

    void initialize();

    bool registerModel(const ModelVersion& version);
    std::vector<ModelVersion> getAllModels() const;
    ModelVersion getModel(int id) const;
    bool deleteModel(int id);
    bool setActiveModel(int id);
    ModelVersion getActiveModel() const;

    void setUpdatedCallback(void* ctx, UpdatedCallback cb) { m_updatedCtx = ctx; m_updatedCb = cb; }
    void setDeletedCallback(void* ctx, DeletedCallback cb) { m_deletedCtx = ctx; m_deletedCb = cb; }
    void setSelectedCallback(void* ctx, SelectedCallback cb) { m_selectedCtx = ctx; m_selectedCb = cb; }

    using ShowCallback = std::function<void(void*)>;
    void setShowCallback(ShowCallback cb, void* ctx);
    void show();

private:
    void* m_parent;
    std::vector<ModelVersion> m_models;
    int m_selectedModelId = 0;
    void* m_dbHandle = nullptr;

    void* m_updatedCtx = nullptr;
    UpdatedCallback m_updatedCb = nullptr;
    void* m_deletedCtx = nullptr;
    DeletedCallback m_deletedCb = nullptr;
    void* m_selectedCtx = nullptr;
    SelectedCallback m_selectedCb = nullptr;

    ShowCallback m_showCb;
    void* m_showCtx = nullptr;
};
