#pragma once

#include "RawrXD_Window.h"

// Forward declarations


/**
 * @brief Model version for registry tracking
 */
struct ModelVersion
{
    int id;                      // Database ID
    std::string name;                // Model name/identifier
    std::string path;                // File path to .gguf file
    std::string baseModel;           // Base model used for fine-tuning
    std::string dataset;             // Dataset used for training
    std::chrono::system_clock::time_point createdAt;         // Timestamp
    float finalLoss;             // Final training loss
    float perplexity;            // Validation perplexity
    int epochs;                  // Number of training epochs
    float learningRate;          // Learning rate used
    int batchSize;               // Batch size used
    std::string tags;                // User-defined tags (comma-separated)
    std::string notes;               // User notes
    int64_t fileSize;             // File size in bytes
    bool isActive;               // Currently selected/active model
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
class ModelRegistry : public RawrXD::Window
{

public:
    explicit ModelRegistry(void* parent = nullptr);
    ~ModelRegistry() override;
    
    /**
     * Two-phase initialization - call after void is ready
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


    /**
     * @brief Emitted when a model is selected for use
     * @param modelPath Path to the selected model file
     */
    void modelSelected(const std::string& modelPath);

    /**
     * @brief Emitted when a model is deleted
     * @param id Database ID of deleted model
     */
    void modelDeleted(int id);

    /**
     * @brief Emitted when registry is updated
     */
    void registryUpdated();

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
    std::string formatTimestamp(const std::chrono::system_clock::time_point& dt) const;

    // UI Components
    void* m_tableWidget;
    void* m_refreshButton;
    void* m_deleteButton;
    void* m_activateButton;
    void* m_compareButton;
    void* m_exportButton;
    void* m_searchEdit;
    void* m_filterCombo;
    void* m_statusLabel;

    // Database
    QSqlDatabase m_db;
    std::string m_dbPath;

    // State
    std::vector<ModelVersion> m_models;
    int m_selectedModelId;
};


