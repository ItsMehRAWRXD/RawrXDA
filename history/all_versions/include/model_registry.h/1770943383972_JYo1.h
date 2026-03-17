#pragma once

#include <string>
#include <vector>
#include <cstdint>

#ifdef _WIN32
#include <windows.h>
#endif

/**
 * @brief Model version for registry tracking (C++20-friendly struct)
 */
struct ModelVersion
{
    int id = 0;
    std::string name;
    std::string path;
    std::string baseModel;
    std::string dataset;
    int64_t createdAt = 0;       // Unix timestamp (replaces QDateTime)
    float finalLoss = 0.0f;
    float perplexity = 0.0f;
    int epochs = 0;
    float learningRate = 0.0f;
    int batchSize = 0;
    std::string tags;
    std::string notes;
    int64_t fileSize = 0;
    bool isActive = false;
};

/**
 * @brief Model registry for managing trained model versions
 * 
 * Provides:
 * - SQLite-backed persistent storage of model metadata
 * - Version listing and comparison
 * - Rollback to previous versions
 * - Tagging and annotation
 * - Search and filtering
 */
class ModelRegistry
{

public:
    ModelRegistry() = default;
    ~ModelRegistry() = default;
    
    /**
     * Two-phase initialization - call after QApplication is ready
     * Sets up database, creates Qt widgets, and loads models
     */
    void initialize();

    /**
     * @brief Register a newly trained model
     * @param version Model metadata
     * @return true if registration successful
     */
    bool registerModel(const ModelVersion& version);

    /**
     * @brief Get all registered models
     * @return Vector of all model versions
     */
    std::vector<ModelVersion> getAllModels() const;

    /**
     * @brief Get a specific model by ID
     * @param id Database ID
     * @return Model version or empty if not found
     */
    ModelVersion getModel(int id) const;

    /**
     * @brief Delete a model from registry (does not delete file)
     * @param id Database ID
     * @return true if deletion successful
     */
    bool deleteModel(int id);

    /**
     * @brief Set a model as the active model
     * @param id Database ID
     * @return true if successful
     */
    bool setActiveModel(int id);

    /**
     * @brief Get the currently active model
     * @return Active model version or empty if none
     */
    ModelVersion getActiveModel() const;

    // --- Callbacks (replaces Qt signals) ---
    using ModelSelectedCb = void(*)(void* ctx, const char* modelPath);
    using ModelDeletedCb = void(*)(void* ctx, int id);
    using VoidCb = void(*)(void* ctx);

    void setModelSelectedCb(ModelSelectedCb cb, void* ctx) { m_selectedCb = cb; m_selectedCtx = ctx; }
    void setModelDeletedCb(ModelDeletedCb cb, void* ctx) { m_deletedCb = cb; m_deletedCtx = ctx; }
    void setRegistryUpdatedCb(VoidCb cb, void* ctx) { m_updatedCb = cb; m_updatedCtx = ctx; }

private:
    void onRefreshClicked();
    void onDeleteClicked();
    void onActivateClicked();
    void onCompareClicked();
    void onExportClicked();
    void onSearchTextChanged(const std::string& text);
    void onFilterChanged(int index);
    void onRowSelectionChanged();

private:
    void setupUI();
    void setupDatabase();
    void setupConnections();
    void loadModels();
    void populateTable(const std::vector<ModelVersion>& models);
    std::string formatFileSize(int64_t bytes) const;
    std::string formatTimestamp(int64_t timestamp) const;

    // UI Components
    HWND m_tableWidget = nullptr;
    HWND m_refreshButton = nullptr;
    HWND m_deleteButton = nullptr;
    HWND m_activateButton = nullptr;
    HWND m_compareButton = nullptr;
    HWND m_exportButton = nullptr;
    HWND m_searchEdit = nullptr;
    HWND m_filterCombo = nullptr;
    HWND m_statusLabel = nullptr;

    // Database
    void* m_dbHandle = nullptr;  // SQLite3 handle
    std::string m_dbPath;

    // State
    std::vector<ModelVersion> m_models;
    int m_selectedModelId = 0;

    // Callback members
    ModelSelectedCb m_selectedCb = nullptr;
    void* m_selectedCtx = nullptr;
    ModelDeletedCb m_deletedCb = nullptr;
    void* m_deletedCtx = nullptr;
    VoidCb m_updatedCb = nullptr;
    void* m_updatedCtx = nullptr;
};
