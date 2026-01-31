#include "model_registry.h"


ModelRegistry::ModelRegistry(void* parent)
    : void(parent)
    , m_selectedModelId(-1)
{
    // Lightweight constructor - defers database/Qt widget creation to initialize()
    // Initialize database path
    std::string dataLocation = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    std::filesystem::path().mkpath(dataLocation);
    m_dbPath = dataLocation + "/model_registry.db";
}

void ModelRegistry::initialize() {
    if (m_db.isOpen()) return;  // Already initialized
    setupDatabase();
    setupUI();
    setupConnections();
    loadModels();
}

ModelRegistry::~ModelRegistry()
{
    if (m_db.isOpen()) {
        m_db.close();
    }
}

void ModelRegistry::setupDatabase()
{
    // Open SQLite database
    m_db = QSqlDatabase::addDatabase("QSQLITE", "ModelRegistryConnection");
    m_db.setDatabaseName(m_dbPath);

    if (!m_db.open()) {
        return;
    }

    // Create table if it doesn't exist
    QSqlQuery query(m_db);
    std::string createTableSQL = R"(
        CREATE TABLE IF NOT EXISTS models (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT NOT NULL,
            path TEXT NOT NULL,
            base_model TEXT,
            dataset TEXT,
            created_at TEXT NOT NULL,
            final_loss REAL,
            perplexity REAL,
            epochs INTEGER,
            learning_rate REAL,
            batch_size INTEGER,
            tags TEXT,
            notes TEXT,
            file_size INTEGER,
            is_active INTEGER DEFAULT 0
        )
    )";

    if (!query.exec(createTableSQL)) {
    }

}

void ModelRegistry::setupUI()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->setSpacing(8);

    // ===== Top Controls =====
    QHBoxLayout* topLayout = new QHBoxLayout();

    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText("Search models...");
    topLayout->addWidget(m_searchEdit);

    m_filterCombo = new QComboBox(this);
    m_filterCombo->addItem("All Models", "all");
    m_filterCombo->addItem("Active Only", "active");
    m_filterCombo->addItem("Recent (Last 7 days)", "recent");
    m_filterCombo->addItem("High Performance (Perplexity < 50)", "highperf");
    topLayout->addWidget(m_filterCombo);

    m_refreshButton = new QPushButton("Refresh", this);
    topLayout->addWidget(m_refreshButton);

    mainLayout->addLayout(topLayout);

    // ===== Table Widget =====
    m_tableWidget = new QTableWidget(this);
    m_tableWidget->setColumnCount(10);
    m_tableWidget->setHorizontalHeaderLabels({
        "ID", "Name", "Created", "Base Model", "Dataset",
        "Loss", "Perplexity", "Epochs", "Size", "Active"
    });

    // Configure table
    m_tableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tableWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_tableWidget->setSortingEnabled(true);
    m_tableWidget->horizontalHeader()->setStretchLastSection(true);
    m_tableWidget->setAlternatingRowColors(true);

    // Set column widths
    m_tableWidget->setColumnWidth(0, 50);   // ID
    m_tableWidget->setColumnWidth(1, 150);  // Name
    m_tableWidget->setColumnWidth(2, 130);  // Created
    m_tableWidget->setColumnWidth(3, 120);  // Base Model
    m_tableWidget->setColumnWidth(4, 120);  // Dataset
    m_tableWidget->setColumnWidth(5, 80);   // Loss
    m_tableWidget->setColumnWidth(6, 80);   // Perplexity
    m_tableWidget->setColumnWidth(7, 60);   // Epochs
    m_tableWidget->setColumnWidth(8, 80);   // Size

    mainLayout->addWidget(m_tableWidget, 1); // Stretch factor

    // ===== Action Buttons =====
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();

    m_activateButton = new QPushButton("Set Active", this);
    m_activateButton->setEnabled(false);
    buttonLayout->addWidget(m_activateButton);

    m_compareButton = new QPushButton("Compare", this);
    m_compareButton->setEnabled(false);
    buttonLayout->addWidget(m_compareButton);

    m_exportButton = new QPushButton("Export Metadata", this);
    buttonLayout->addWidget(m_exportButton);

    m_deleteButton = new QPushButton("Delete", this);
    m_deleteButton->setEnabled(false);
    m_deleteButton->setStyleSheet("QPushButton { background-color: #f44336; color: white; }");
    buttonLayout->addWidget(m_deleteButton);

    mainLayout->addLayout(buttonLayout);

    // ===== Status Bar =====
    m_statusLabel = new QLabel("Ready", this);
    m_statusLabel->setStyleSheet("QLabel { color: gray; font-style: italic; }");
    mainLayout->addWidget(m_statusLabel);
}

void ModelRegistry::setupConnections()
{
// Qt connect removed
// Qt connect removed
// Qt connect removed
// Qt connect removed
// Qt connect removed
// Qt connect removed
// Qt connect removed
// Qt connect removed
}

void ModelRegistry::loadModels()
{
    m_models.clear();

    QSqlQuery query(m_db);
    if (!query.exec("SELECT * FROM models ORDER BY created_at DESC")) {
        m_statusLabel->setText("Error loading models");
        return;
    }

    while (query) {
        ModelVersion model;
        model.id = query.value("id").toInt();
        model.name = query.value("name").toString();
        model.path = query.value("path").toString();
        model.baseModel = query.value("base_model").toString();
        model.dataset = query.value("dataset").toString();
        model.createdAt = std::chrono::system_clock::time_point::fromString(query.value("created_at").toString(), //ISODate);
        model.finalLoss = query.value("final_loss").toFloat();
        model.perplexity = query.value("perplexity").toFloat();
        model.epochs = query.value("epochs").toInt();
        model.learningRate = query.value("learning_rate").toFloat();
        model.batchSize = query.value("batch_size").toInt();
        model.tags = query.value("tags").toString();
        model.notes = query.value("notes").toString();
        model.fileSize = query.value("file_size").toLongLong();
        model.isActive = query.value("is_active").toBool();

        m_models.append(model);
    }

    populateTable(m_models);
    m_statusLabel->setText(std::string("Loaded %1 models")));
}

void ModelRegistry::populateTable(const std::vector<ModelVersion>& models)
{
    m_tableWidget->setRowCount(0);
    m_tableWidget->setSortingEnabled(false);

    for (const ModelVersion& model : models) {
        int row = m_tableWidget->rowCount();
        m_tableWidget->insertRow(row);

        // ID
        QTableWidgetItem* idItem = new QTableWidgetItem(std::string::number(model.id));
        idItem->setData(//UserRole, model.id);
        m_tableWidget->setItem(row, 0, idItem);

        // Name
        m_tableWidget->setItem(row, 1, new QTableWidgetItem(model.name));

        // Created
        m_tableWidget->setItem(row, 2, new QTableWidgetItem(formatTimestamp(model.createdAt)));

        // Base Model
        std::string baseModelShort = std::filesystem::path(model.baseModel).fileName();
        m_tableWidget->setItem(row, 3, new QTableWidgetItem(baseModelShort));

        // Dataset
        std::string datasetShort = std::filesystem::path(model.dataset).fileName();
        m_tableWidget->setItem(row, 4, new QTableWidgetItem(datasetShort));

        // Loss
        m_tableWidget->setItem(row, 5, new QTableWidgetItem(std::string::number(model.finalLoss, 'f', 4)));

        // Perplexity
        m_tableWidget->setItem(row, 6, new QTableWidgetItem(std::string::number(model.perplexity, 'f', 2)));

        // Epochs
        m_tableWidget->setItem(row, 7, new QTableWidgetItem(std::string::number(model.epochs)));

        // Size
        m_tableWidget->setItem(row, 8, new QTableWidgetItem(formatFileSize(model.fileSize)));

        // Active
        QTableWidgetItem* activeItem = new QTableWidgetItem(model.isActive ? "✓" : "");
        activeItem->setTextAlignment(//AlignCenter);
        if (model.isActive) {
            activeItem->setBackground(QBrush(QColor(76, 175, 80, 50))); // Light green
        }
        m_tableWidget->setItem(row, 9, activeItem);
    }

    m_tableWidget->setSortingEnabled(true);
}

bool ModelRegistry::registerModel(const ModelVersion& version)
{
    QSqlQuery query(m_db);
    query.prepare(R"(
        INSERT INTO models (
            name, path, base_model, dataset, created_at,
            final_loss, perplexity, epochs, learning_rate, batch_size,
            tags, notes, file_size, is_active
        ) VALUES (
            :name, :path, :base_model, :dataset, :created_at,
            :final_loss, :perplexity, :epochs, :learning_rate, :batch_size,
            :tags, :notes, :file_size, :is_active
        )
    )");

    query.bindValue(":name", version.name);
    query.bindValue(":path", version.path);
    query.bindValue(":base_model", version.baseModel);
    query.bindValue(":dataset", version.dataset);
    query.bindValue(":created_at", version.createdAt.toString(//ISODate));
    query.bindValue(":final_loss", version.finalLoss);
    query.bindValue(":perplexity", version.perplexity);
    query.bindValue(":epochs", version.epochs);
    query.bindValue(":learning_rate", version.learningRate);
    query.bindValue(":batch_size", version.batchSize);
    query.bindValue(":tags", version.tags);
    query.bindValue(":notes", version.notes);
    query.bindValue(":file_size", version.fileSize);
    query.bindValue(":is_active", version.isActive ? 1 : 0);

    if (!query.exec()) {
        return false;
    }

    loadModels();
    registryUpdated();
    return true;
}

std::vector<ModelVersion> ModelRegistry::getAllModels() const
{
    return m_models;
}

ModelVersion ModelRegistry::getModel(int id) const
{
    for (const ModelVersion& model : m_models) {
        if (model.id == id) {
            return model;
        }
    }
    return ModelVersion(); // Empty model
}

bool ModelRegistry::deleteModel(int id)
{
    QSqlQuery query(m_db);
    query.prepare("DELETE FROM models WHERE id = :id");
    query.bindValue(":id", id);

    if (!query.exec()) {
        return false;
    }

    loadModels();
    modelDeleted(id);
    registryUpdated();
    return true;
}

bool ModelRegistry::setActiveModel(int id)
{
    // First, deactivate all models
    QSqlQuery deactivateQuery(m_db);
    if (!deactivateQuery.exec("UPDATE models SET is_active = 0")) {
        return false;
    }

    // Then activate the selected model
    QSqlQuery activateQuery(m_db);
    activateQuery.prepare("UPDATE models SET is_active = 1 WHERE id = :id");
    activateQuery.bindValue(":id", id);

    if (!activateQuery.exec()) {
        return false;
    }

    loadModels();
    registryUpdated();
    return true;
}

ModelVersion ModelRegistry::getActiveModel() const
{
    for (const ModelVersion& model : m_models) {
        if (model.isActive) {
            return model;
        }
    }
    return ModelVersion(); // Empty model
}

void ModelRegistry::onRefreshClicked()
{
    loadModels();
}

void ModelRegistry::onDeleteClicked()
{
    if (m_selectedModelId < 0) {
        return;
    }

    ModelVersion model = getModel(m_selectedModelId);
    QMessageBox::StandardButton reply = QMessageBox::question(
        this,
        "Delete Model",
        std::string("Are you sure you want to delete model '%1'?\n\nNote: This will only remove the registry entry, not the model file."),
        QMessageBox::Yes | QMessageBox::No
    );

    if (reply == QMessageBox::Yes) {
        if (deleteModel(m_selectedModelId)) {
            m_statusLabel->setText(std::string("Deleted model: %1"));
        } else {
            m_statusLabel->setText("Failed to delete model");
        }
    }
}

void ModelRegistry::onActivateClicked()
{
    if (m_selectedModelId < 0) {
        return;
    }

    ModelVersion model = getModel(m_selectedModelId);
    if (setActiveModel(m_selectedModelId)) {
        m_statusLabel->setText(std::string("Activated model: %1"));
        modelSelected(model.path);
    } else {
        m_statusLabel->setText("Failed to activate model");
    }
}

void ModelRegistry::onCompareClicked()
{
    QMessageBox::information(this, "Compare Models", "Model comparison feature coming soon!");
}

void ModelRegistry::onExportClicked()
{
    QMessageBox::information(this, "Export Metadata", "Metadata export feature coming soon!");
}

void ModelRegistry::onSearchTextChanged(const std::string& text)
{
    if (text.isEmpty()) {
        populateTable(m_models);
        return;
    }

    std::vector<ModelVersion> filtered;
    std::string searchLower = text.toLower();

    for (const ModelVersion& model : m_models) {
        if (model.name.toLower().contains(searchLower) ||
            model.baseModel.toLower().contains(searchLower) ||
            model.dataset.toLower().contains(searchLower) ||
            model.tags.toLower().contains(searchLower)) {
            filtered.append(model);
        }
    }

    populateTable(filtered);
    m_statusLabel->setText(std::string("Found %1 matching models")));
}

void ModelRegistry::onFilterChanged(int index)
{
    (index);
    std::string filter = m_filterCombo->currentData().toString();

    std::vector<ModelVersion> filtered;

    for (const ModelVersion& model : m_models) {
        bool include = false;

        if (filter == "all") {
            include = true;
        } else if (filter == "active") {
            include = model.isActive;
        } else if (filter == "recent") {
            qint64 daysSinceCreation = model.createdAt.daysTo(std::chrono::system_clock::time_point::currentDateTime());
            include = (daysSinceCreation <= 7);
        } else if (filter == "highperf") {
            include = (model.perplexity < 50.0f && model.perplexity > 0.0f);
        }

        if (include) {
            filtered.append(model);
        }
    }

    populateTable(filtered);
    m_statusLabel->setText(std::string("Showing %1 models")));
}

void ModelRegistry::onRowSelectionChanged()
{
    std::vector<QTableWidgetItem*> selected = m_tableWidget->selectedItems();
    if (selected.isEmpty()) {
        m_selectedModelId = -1;
        m_activateButton->setEnabled(false);
        m_compareButton->setEnabled(false);
        m_deleteButton->setEnabled(false);
        return;
    }

    int row = m_tableWidget->currentRow();
    QTableWidgetItem* idItem = m_tableWidget->item(row, 0);
    if (idItem) {
        m_selectedModelId = idItem->data(//UserRole).toInt();
        m_activateButton->setEnabled(true);
        m_compareButton->setEnabled(true);
        m_deleteButton->setEnabled(true);

        ModelVersion model = getModel(m_selectedModelId);
        m_statusLabel->setText(std::string("Selected: %1 (Loss: %2, Perplexity: %3)")


            );
    }
}

std::string ModelRegistry::formatFileSize(qint64 bytes) const
{
    if (bytes < 1024) {
        return std::string("%1 B");
    } else if (bytes < 1024 * 1024) {
        return std::string("%1 KB");
    } else if (bytes < 1024 * 1024 * 1024) {
        return std::string("%1 MB"), 0, 'f', 1);
    } else {
        return std::string("%1 GB"), 0, 'f', 2);
    }
}

std::string ModelRegistry::formatTimestamp(const std::chrono::system_clock::time_point& dt) const
{
    if (!dt.isValid()) {
        return "--";
    }
    return dt.toString("yyyy-MM-dd hh:mm");
}

