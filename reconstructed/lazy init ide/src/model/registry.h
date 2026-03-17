#pragma once

#include <QWidget>
#include <QString>
#include <QDateTime>
#include <QSqlDatabase>
#include <QVector>

// Forward declarations
class QTableWidget;
class QPushButton;
class QLabel;
class QLineEdit;
class QComboBox;

/**
 * @brief Model version for registry tracking
 */
struct ModelVersion
{
    int id;                      // Database ID
    QString name;                // Model name/identifier
    QString path;                // File path to .gguf file
    QString baseModel;           // Base model used for fine-tuning
    QString dataset;             // Dataset used for training
    QDateTime createdAt;         // Timestamp
    float finalLoss;             // Final training loss
    float perplexity;            // Validation perplexity
    int epochs;                  // Number of training epochs
    float learningRate;          // Learning rate used
    int batchSize;               // Batch size used
    QString tags;                // User-defined tags (comma-separated)
    QString notes;               // User notes
    qint64 fileSize;             // File size in bytes
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
class ModelRegistry : public QWidget
{
    Q_OBJECT

    // Allow test class to access private members
    friend class ModelRegistryTest;

public:
    explicit ModelRegistry(QWidget* parent = nullptr);
    ~ModelRegistry() override;
    
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
    QVector<ModelVersion> getAllModels() const;

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

signals:
    /**
     * @brief Emitted when a model is selected for use
     * @param modelPath Path to the selected model file
     */
    void modelSelected(const QString& modelPath);

    /**
     * @brief Emitted when a model is deleted
     * @param id Database ID of deleted model
     */
    void modelDeleted(int id);

    /**
     * @brief Emitted when registry is updated
     */
    void registryUpdated();

private slots:
    void onRefreshClicked();
    void onDeleteClicked();
    void onActivateClicked();
    void onCompareClicked();
    void onExportClicked();
    void onSearchTextChanged(const QString& text);
    void onFilterChanged(int index);
    void onRowSelectionChanged();

private:
    void setupUI();
    void setupDatabase();
    void setupConnections();
    void loadModels();
    void populateTable(const QVector<ModelVersion>& models);
    QString formatFileSize(qint64 bytes) const;
    QString formatTimestamp(const QDateTime& dt) const;

    // UI Components
    QTableWidget* m_tableWidget;
    QPushButton* m_refreshButton;
    QPushButton* m_deleteButton;
    QPushButton* m_activateButton;
    QPushButton* m_compareButton;
    QPushButton* m_exportButton;
    QLineEdit* m_searchEdit;
    QComboBox* m_filterCombo;
    QLabel* m_statusLabel;

    // Database
    QSqlDatabase m_db;
    QString m_dbPath;

    // State
    QVector<ModelVersion> m_models;
    int m_selectedModelId;
};
