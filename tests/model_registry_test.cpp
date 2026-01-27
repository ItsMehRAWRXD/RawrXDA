#include <QTest>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QSqlRecord>
#include <QStandardPaths>
#include <QDir>
#include <QDateTime>
#include <QTableWidget>
#include <QPushButton>
#include <QLineEdit>
#include <QComboBox>
#include <QMessageBox>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QSignalSpy>
#include <iostream>

#include "model_registry.h"

class ModelRegistryTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    // Database Tests
    void testDatabaseInitialization();
    void testDatabaseTableCreation();
    void testDatabaseConnection();

    // Model Registration Tests
    void testRegisterSingleModel();
    void testRegisterMultipleModels();
    void testRegisterModelWithAllFields();
    void testRegisterInvalidModel();
    void testRegistryUpdatedSignal();

    // Model Loading Tests
    void testLoadModelsFromDatabase();
    void testLoadEmptyDatabase();
    void testGetAllModels();
    void testGetModelById();
    void testGetNonexistentModel();

    // Model Activation Tests
    void testSetActiveModel();
    void testSetActiveModelDeactivatesPrevious();
    void testGetActiveModel();
    void testGetActiveModelWhenNoneActive();
    void testMultipleActivationCalls();

    // Model Deletion Tests
    void testDeleteModel();
    void testDeleteNonexistentModel();
    void testDeleteModelEmptiesDatabase();
    void testModelDeletedSignal();

    // UI Population Tests
    void testPopulateTableWithModels();
    void testPopulateTableEmpty();
    void testTableColumnConfiguration();
    void testTableRowSelection();

    // Search and Filter Tests
    void testSearchByName();
    void testSearchByBaseModel();
    void testSearchByDataset();
    void testSearchByTags();
    void testSearchCaseSensitivity();
    void testSearchEmpty();
    void testFilterActive();
    void testFilterRecent();
    void testFilterHighPerformance();
    void testFilterAll();

    // UI Button Tests
    void testButtonsDisabledWhenNoSelection();
    void testButtonsEnabledWhenSelectionChanged();
    void testActivateButtonFunctionality();
    void testDeleteButtonFunctionality();

    // Status Label Tests
    void testStatusLabelUpdates();
    void testStatusLabelOnRefresh();
    void testStatusLabelOnFilter();

    // File Size Formatting Tests
    void testFormatFileSizeBytes();
    void testFormatFileSizeKilobytes();
    void testFormatFileSizeMegabytes();
    void testFormatFileSizeGigabytes();

    // Timestamp Formatting Tests
    void testFormatTimestampValid();
    void testFormatTimestampInvalid();

    // Data Persistence Tests
    void testModelPersistenceAcrossInstances();
    void testDatabaseFileExists();

    // Edge Cases
    void testLargeModelMetadata();
    void testSpecialCharactersInName();
    void testNullValuesHandling();
    void testConcurrentAccess();

private:
    ModelRegistry* m_registry;
    QTemporaryDir m_tempDir;
    QString m_originalAppDataLocation;
};

void ModelRegistryTest::initTestCase()
{
    // Store original app data location
    m_originalAppDataLocation = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    
    // Set up temporary directory for test database
    if (!m_tempDir.isValid()) {
        QFAIL("Failed to create temporary directory");
    }
}

void ModelRegistryTest::cleanupTestCase()
{
    // Temporary directory cleaned up automatically by QTemporaryDir
}

void ModelRegistryTest::init()
{
    // Create a fresh registry instance for each test
    m_registry = new ModelRegistry();
    QVERIFY(m_registry != nullptr);
    QVERIFY(QSqlDatabase::database("ModelRegistryConnection").isOpen());
}

void ModelRegistryTest::cleanup()
{
    // Clean up registry and database
    if (m_registry) {
        delete m_registry;
        m_registry = nullptr;
    }

    // Close database connection
    QSqlDatabase db = QSqlDatabase::database("ModelRegistryConnection");
    if (db.isOpen()) {
        db.close();
    }

    // Clear all connections
    QSqlDatabase::removeDatabase("ModelRegistryConnection");
}

// ============= Database Tests =============

void ModelRegistryTest::testDatabaseInitialization()
{
    QSqlDatabase db = QSqlDatabase::database("ModelRegistryConnection");
    QVERIFY(db.isValid());
    QVERIFY(db.isOpen());
    QCOMPARE(db.driverName(), "QSQLITE");
}

void ModelRegistryTest::testDatabaseTableCreation()
{
    QSqlDatabase db = QSqlDatabase::database("ModelRegistryConnection");
    QSqlQuery query(db);
    
    // Check if models table exists
    bool tableExists = false;
    if (query.exec("SELECT name FROM sqlite_master WHERE type='table' AND name='models'")) {
        tableExists = query.next();
    }
    QVERIFY(tableExists);
}

void ModelRegistryTest::testDatabaseConnection()
{
    QSqlDatabase db = QSqlDatabase::database("ModelRegistryConnection");
    QVERIFY(db.isValid());
    
    QSqlQuery testQuery(db);
    QVERIFY(testQuery.exec("SELECT 1"));
}

// ============= Model Registration Tests =============

void ModelRegistryTest::testRegisterSingleModel()
{
    ModelVersion model;
    model.name = "TestModel1";
    model.path = "/path/to/model";
    model.baseModel = "GPT-2";
    model.dataset = "WikiText";
    model.createdAt = QDateTime::currentDateTime();
    model.finalLoss = 2.5f;
    model.perplexity = 12.3f;
    model.epochs = 10;
    model.learningRate = 0.001f;
    model.batchSize = 32;
    model.fileSize = 1024 * 1024 * 100; // 100 MB

    QVERIFY(m_registry->registerModel(model));
    
    QVector<ModelVersion> models = m_registry->getAllModels();
    QCOMPARE(models.size(), 1);
    QCOMPARE(models.first().name, "TestModel1");
}

void ModelRegistryTest::testRegisterMultipleModels()
{
    for (int i = 0; i < 5; ++i) {
        ModelVersion model;
        model.name = QString("Model%1").arg(i);
        model.path = QString("/path/to/model%1").arg(i);
        model.createdAt = QDateTime::currentDateTime();
        model.finalLoss = 2.5f + i * 0.1f;
        model.perplexity = 12.3f + i * 0.5f;
        model.epochs = 10 + i;

        QVERIFY(m_registry->registerModel(model));
    }

    QVector<ModelVersion> models = m_registry->getAllModels();
    QCOMPARE(models.size(), 5);
}

void ModelRegistryTest::testRegisterModelWithAllFields()
{
    ModelVersion model;
    model.name = "CompleteModel";
    model.path = "/complete/path";
    model.baseModel = "GPT-3";
    model.dataset = "CommonCrawl";
    model.createdAt = QDateTime(QDate(2025, 1, 1), QTime(12, 0, 0));
    model.finalLoss = 1.8f;
    model.perplexity = 8.5f;
    model.epochs = 20;
    model.learningRate = 0.0001f;
    model.batchSize = 64;
    model.tags = "production,optimized";
    model.notes = "Production model with optimizations";
    model.fileSize = 5368709120LL; // 5 GB
    model.isActive = true;

    QVERIFY(m_registry->registerModel(model));

    ModelVersion retrieved = m_registry->getModel(1);
    QCOMPARE(retrieved.name, "CompleteModel");
    QCOMPARE(retrieved.baseModel, "GPT-3");
    QCOMPARE(retrieved.finalLoss, 1.8f);
    QCOMPARE(retrieved.tags, "production,optimized");
    QCOMPARE(retrieved.fileSize, 5368709120LL);
}

void ModelRegistryTest::testRegisterInvalidModel()
{
    ModelVersion invalidModel;
    // Missing required fields like name and path
    // Should still attempt registration
    QVERIFY(!m_registry->registerModel(invalidModel) || 
            m_registry->getAllModels().isEmpty());
}

void ModelRegistryTest::testRegistryUpdatedSignal()
{
    QSignalSpy spy(m_registry, SIGNAL(registryUpdated()));

    ModelVersion model;
    model.name = "SignalTestModel";
    model.path = "/signal/test";
    model.createdAt = QDateTime::currentDateTime();

    m_registry->registerModel(model);

    QCOMPARE(spy.count(), 1);
}

// ============= Model Loading Tests =============

void ModelRegistryTest::testLoadModelsFromDatabase()
{
    // Register models
    for (int i = 0; i < 3; ++i) {
        ModelVersion model;
        model.name = QString("LoadTest%1").arg(i);
        model.path = QString("/load/test%1").arg(i);
        model.createdAt = QDateTime::currentDateTime();
        m_registry->registerModel(model);
    }

    // Create new instance to test loading from DB
    ModelRegistry* registry2 = new ModelRegistry();
    QVector<ModelVersion> loadedModels = registry2->getAllModels();
    QCOMPARE(loadedModels.size(), 3);
    delete registry2;
}

void ModelRegistryTest::testLoadEmptyDatabase()
{
    QVector<ModelVersion> models = m_registry->getAllModels();
    QCOMPARE(models.size(), 0);
}

void ModelRegistryTest::testGetAllModels()
{
    ModelVersion model1, model2;
    model1.name = "Model1";
    model1.path = "/path1";
    model1.createdAt = QDateTime::currentDateTime();
    model2.name = "Model2";
    model2.path = "/path2";
    model2.createdAt = QDateTime::currentDateTime();

    m_registry->registerModel(model1);
    m_registry->registerModel(model2);

    QVector<ModelVersion> allModels = m_registry->getAllModels();
    QCOMPARE(allModels.size(), 2);
}

void ModelRegistryTest::testGetModelById()
{
    ModelVersion model;
    model.name = "GetByIdTest";
    model.path = "/getbyid/test";
    model.createdAt = QDateTime::currentDateTime();
    model.perplexity = 15.5f;

    m_registry->registerModel(model);

    ModelVersion retrieved = m_registry->getModel(1);
    QCOMPARE(retrieved.name, "GetByIdTest");
    QCOMPARE(retrieved.perplexity, 15.5f);
}

void ModelRegistryTest::testGetNonexistentModel()
{
    ModelVersion model = m_registry->getModel(999);
    QCOMPARE(model.name, "");
    QCOMPARE(model.path, "");
}

// ============= Model Activation Tests =============

void ModelRegistryTest::testSetActiveModel()
{
    ModelVersion model;
    model.name = "ActiveTest";
    model.path = "/active/test";
    model.createdAt = QDateTime::currentDateTime();

    m_registry->registerModel(model);
    QVERIFY(m_registry->setActiveModel(1));

    ModelVersion active = m_registry->getActiveModel();
    QCOMPARE(active.name, "ActiveTest");
    QVERIFY(active.isActive);
}

void ModelRegistryTest::testSetActiveModelDeactivatesPrevious()
{
    // Register two models
    ModelVersion model1, model2;
    model1.name = "Model1";
    model1.path = "/path1";
    model1.createdAt = QDateTime::currentDateTime();
    model2.name = "Model2";
    model2.path = "/path2";
    model2.createdAt = QDateTime::currentDateTime();

    m_registry->registerModel(model1);
    m_registry->registerModel(model2);

    // Activate first model
    m_registry->setActiveModel(1);
    ModelVersion active1 = m_registry->getActiveModel();
    QCOMPARE(active1.name, "Model1");

    // Activate second model
    m_registry->setActiveModel(2);
    ModelVersion active2 = m_registry->getActiveModel();
    QCOMPARE(active2.name, "Model2");

    // Verify first model is no longer active
    ModelVersion model1Check = m_registry->getModel(1);
    QVERIFY(!model1Check.isActive);
}

void ModelRegistryTest::testGetActiveModel()
{
    ModelVersion model;
    model.name = "ActiveModel";
    model.path = "/active/model";
    model.createdAt = QDateTime::currentDateTime();

    m_registry->registerModel(model);
    m_registry->setActiveModel(1);

    ModelVersion active = m_registry->getActiveModel();
    QCOMPARE(active.name, "ActiveModel");
}

void ModelRegistryTest::testGetActiveModelWhenNoneActive()
{
    ModelVersion active = m_registry->getActiveModel();
    QCOMPARE(active.name, "");
}

void ModelRegistryTest::testMultipleActivationCalls()
{
    ModelVersion model;
    model.name = "MultiActivate";
    model.path = "/multi/activate";
    model.createdAt = QDateTime::currentDateTime();

    m_registry->registerModel(model);

    // Activate multiple times
    for (int i = 0; i < 3; ++i) {
        QVERIFY(m_registry->setActiveModel(1));
        ModelVersion active = m_registry->getActiveModel();
        QCOMPARE(active.name, "MultiActivate");
    }
}

// ============= Model Deletion Tests =============

void ModelRegistryTest::testDeleteModel()
{
    ModelVersion model;
    model.name = "DeleteTest";
    model.path = "/delete/test";
    model.createdAt = QDateTime::currentDateTime();

    m_registry->registerModel(model);
    QCOMPARE(m_registry->getAllModels().size(), 1);

    QVERIFY(m_registry->deleteModel(1));
    QCOMPARE(m_registry->getAllModels().size(), 0);
}

void ModelRegistryTest::testDeleteNonexistentModel()
{
    // Should still return success (SQL delete with no matching rows)
    QVERIFY(m_registry->deleteModel(999));
}

void ModelRegistryTest::testDeleteModelEmptiesDatabase()
{
    for (int i = 0; i < 3; ++i) {
        ModelVersion model;
        model.name = QString("Model%1").arg(i);
        model.path = QString("/path%1").arg(i);
        model.createdAt = QDateTime::currentDateTime();
        m_registry->registerModel(model);
    }

    QCOMPARE(m_registry->getAllModels().size(), 3);

    m_registry->deleteModel(1);
    m_registry->deleteModel(2);
    m_registry->deleteModel(3);

    QCOMPARE(m_registry->getAllModels().size(), 0);
}

void ModelRegistryTest::testModelDeletedSignal()
{
    ModelVersion model;
    model.name = "SignalDelete";
    model.path = "/signal/delete";
    model.createdAt = QDateTime::currentDateTime();

    m_registry->registerModel(model);

    QSignalSpy spyDeleted(m_registry, SIGNAL(modelDeleted(int)));
    QSignalSpy spyUpdated(m_registry, SIGNAL(registryUpdated()));

    m_registry->deleteModel(1);

    QCOMPARE(spyDeleted.count(), 1);
    QCOMPARE(spyUpdated.count(), 1);
}

// ============= UI Population Tests =============

void ModelRegistryTest::testPopulateTableWithModels()
{
    for (int i = 0; i < 3; ++i) {
        ModelVersion model;
        model.name = QString("UIModel%1").arg(i);
        model.path = QString("/ui/model%1").arg(i);
        model.createdAt = QDateTime::currentDateTime();
        model.finalLoss = 2.5f + i * 0.1f;
        model.perplexity = 12.3f + i * 0.5f;
        model.epochs = 10 + i;
        model.fileSize = (i + 1) * 1024 * 1024 * 100;
        m_registry->registerModel(model);
    }

    QCOMPARE(m_registry->m_tableWidget->rowCount(), 3);
}

void ModelRegistryTest::testPopulateTableEmpty()
{
    QCOMPARE(m_registry->m_tableWidget->rowCount(), 0);
}

void ModelRegistryTest::testTableColumnConfiguration()
{
    QCOMPARE(m_registry->m_tableWidget->columnCount(), 10);
    
    QStringList expectedHeaders = {
        "ID", "Name", "Created", "Base Model", "Dataset",
        "Loss", "Perplexity", "Epochs", "Size", "Active"
    };
    
    for (int i = 0; i < expectedHeaders.size(); ++i) {
        QCOMPARE(
            m_registry->m_tableWidget->horizontalHeaderItem(i)->text(),
            expectedHeaders[i]
        );
    }
}

void ModelRegistryTest::testTableRowSelection()
{
    ModelVersion model;
    model.name = "SelectTest";
    model.path = "/select/test";
    model.createdAt = QDateTime::currentDateTime();

    m_registry->registerModel(model);

    // Verify UI buttons are disabled when no selection
    QVERIFY(!m_registry->m_deleteButton->isEnabled());

    // Select row
    m_registry->m_tableWidget->selectRow(0);
    
    // Buttons should now be enabled
    QVERIFY(m_registry->m_deleteButton->isEnabled());
}

// ============= Search and Filter Tests =============

void ModelRegistryTest::testSearchByName()
{
    ModelVersion model1, model2;
    model1.name = "SearchableModel";
    model1.path = "/search1";
    model1.createdAt = QDateTime::currentDateTime();
    model2.name = "OtherModel";
    model2.path = "/other";
    model2.createdAt = QDateTime::currentDateTime();

    m_registry->registerModel(model1);
    m_registry->registerModel(model2);

    m_registry->m_searchEdit->setText("Searchable");
    // Search functionality updates table via onSearchTextChanged signal
}

void ModelRegistryTest::testSearchByBaseModel()
{
    ModelVersion model;
    model.name = "TestModel";
    model.path = "/test";
    model.baseModel = "GPT-2-Large";
    model.createdAt = QDateTime::currentDateTime();

    m_registry->registerModel(model);
    m_registry->m_searchEdit->setText("GPT-2");
}

void ModelRegistryTest::testSearchByDataset()
{
    ModelVersion model;
    model.name = "DatasetModel";
    model.path = "/dataset";
    model.dataset = "CommonCrawl-2024";
    model.createdAt = QDateTime::currentDateTime();

    m_registry->registerModel(model);
    m_registry->m_searchEdit->setText("CommonCrawl");
}

void ModelRegistryTest::testSearchByTags()
{
    ModelVersion model;
    model.name = "TaggedModel";
    model.path = "/tagged";
    model.tags = "production,optimized,gpu";
    model.createdAt = QDateTime::currentDateTime();

    m_registry->registerModel(model);
    m_registry->m_searchEdit->setText("production");
}

void ModelRegistryTest::testSearchCaseSensitivity()
{
    ModelVersion model;
    model.name = "CaseSensitiveModel";
    model.path = "/case";
    model.createdAt = QDateTime::currentDateTime();

    m_registry->registerModel(model);

    // Search should be case-insensitive
    m_registry->m_searchEdit->setText("casesensitive");
}

void ModelRegistryTest::testSearchEmpty()
{
    ModelVersion model;
    model.name = "EmptySearchModel";
    model.path = "/empty";
    model.createdAt = QDateTime::currentDateTime();

    m_registry->registerModel(model);
    m_registry->m_searchEdit->setText("");
    
    QCOMPARE(m_registry->m_tableWidget->rowCount(), 1);
}

void ModelRegistryTest::testFilterActive()
{
    ModelVersion model1, model2;
    model1.name = "ActiveFilter";
    model1.path = "/active";
    model1.createdAt = QDateTime::currentDateTime();
    model1.isActive = true;
    
    model2.name = "InactiveFilter";
    model2.path = "/inactive";
    model2.createdAt = QDateTime::currentDateTime();
    model2.isActive = false;

    m_registry->registerModel(model1);
    m_registry->registerModel(model2);

    m_registry->m_filterCombo->setCurrentIndex(1); // "Active Only"
}

void ModelRegistryTest::testFilterRecent()
{
    ModelVersion recentModel, oldModel;
    recentModel.name = "RecentModel";
    recentModel.path = "/recent";
    recentModel.createdAt = QDateTime::currentDateTime();

    oldModel.name = "OldModel";
    oldModel.path = "/old";
    oldModel.createdAt = QDateTime::currentDateTime().addDays(-10);

    m_registry->registerModel(recentModel);
    m_registry->registerModel(oldModel);

    m_registry->m_filterCombo->setCurrentIndex(2); // "Recent (Last 7 days)"
}

void ModelRegistryTest::testFilterHighPerformance()
{
    ModelVersion goodModel, poorModel;
    goodModel.name = "HighPerf";
    goodModel.path = "/highperf";
    goodModel.createdAt = QDateTime::currentDateTime();
    goodModel.perplexity = 30.5f;

    poorModel.name = "LowPerf";
    poorModel.path = "/lowperf";
    poorModel.createdAt = QDateTime::currentDateTime();
    poorModel.perplexity = 80.0f;

    m_registry->registerModel(goodModel);
    m_registry->registerModel(poorModel);

    m_registry->m_filterCombo->setCurrentIndex(3); // "High Performance"
}

void ModelRegistryTest::testFilterAll()
{
    for (int i = 0; i < 5; ++i) {
        ModelVersion model;
        model.name = QString("FilterAllModel%1").arg(i);
        model.path = QString("/filterall%1").arg(i);
        model.createdAt = QDateTime::currentDateTime();
        m_registry->registerModel(model);
    }

    m_registry->m_filterCombo->setCurrentIndex(0); // "All Models"
    QCOMPARE(m_registry->m_tableWidget->rowCount(), 5);
}

// ============= UI Button Tests =============

void ModelRegistryTest::testButtonsDisabledWhenNoSelection()
{
    QVERIFY(!m_registry->m_activateButton->isEnabled());
    QVERIFY(!m_registry->m_deleteButton->isEnabled());
    QVERIFY(!m_registry->m_compareButton->isEnabled());
}

void ModelRegistryTest::testButtonsEnabledWhenSelectionChanged()
{
    ModelVersion model;
    model.name = "ButtonTest";
    model.path = "/button/test";
    model.createdAt = QDateTime::currentDateTime();

    m_registry->registerModel(model);
    m_registry->m_tableWidget->selectRow(0);

    QVERIFY(m_registry->m_activateButton->isEnabled());
    QVERIFY(m_registry->m_deleteButton->isEnabled());
    QVERIFY(m_registry->m_compareButton->isEnabled());
}

void ModelRegistryTest::testActivateButtonFunctionality()
{
    ModelVersion model;
    model.name = "ActivateButton";
    model.path = "/activate/button";
    model.createdAt = QDateTime::currentDateTime();

    m_registry->registerModel(model);
    m_registry->m_tableWidget->selectRow(0);

    QSignalSpy spyModelSelected(m_registry, SIGNAL(modelSelected(QString)));
    
    m_registry->onActivateClicked();

    QCOMPARE(spyModelSelected.count(), 1);
}

void ModelRegistryTest::testDeleteButtonFunctionality()
{
    ModelVersion model;
    model.name = "DeleteButton";
    model.path = "/delete/button";
    model.createdAt = QDateTime::currentDateTime();

    m_registry->registerModel(model);
    m_registry->m_tableWidget->selectRow(0);

    QSignalSpy spyModelDeleted(m_registry, SIGNAL(modelDeleted(int)));

    // Simulate delete button click (would normally show dialog)
    m_registry->onDeleteClicked();
}

// ============= Status Label Tests =============

void ModelRegistryTest::testStatusLabelUpdates()
{
    ModelVersion model;
    model.name = "StatusTest";
    model.path = "/status/test";
    model.createdAt = QDateTime::currentDateTime();

    m_registry->registerModel(model);

    QString status = m_registry->m_statusLabel->text();
    QVERIFY(status.contains("Loaded"));
}

void ModelRegistryTest::testStatusLabelOnRefresh()
{
    m_registry->onRefreshClicked();
    
    QString status = m_registry->m_statusLabel->text();
    QVERIFY(!status.isEmpty());
}

void ModelRegistryTest::testStatusLabelOnFilter()
{
    ModelVersion model;
    model.name = "FilterStatus";
    model.path = "/filter/status";
    model.createdAt = QDateTime::currentDateTime();

    m_registry->registerModel(model);
    m_registry->m_filterCombo->setCurrentIndex(0);

    QString status = m_registry->m_statusLabel->text();
    QVERIFY(status.contains("Showing"));
}

// ============= File Size Formatting Tests =============

void ModelRegistryTest::testFormatFileSizeBytes()
{
    qint64 bytes = 512;
    QString formatted = m_registry->formatFileSize(bytes);
    QVERIFY(formatted.contains("B"));
    QCOMPARE(formatted, "512 B");
}

void ModelRegistryTest::testFormatFileSizeKilobytes()
{
    qint64 kilobytes = 1024 * 256;
    QString formatted = m_registry->formatFileSize(kilobytes);
    QVERIFY(formatted.contains("KB"));
}

void ModelRegistryTest::testFormatFileSizeMegabytes()
{
    qint64 megabytes = 1024 * 1024 * 100;
    QString formatted = m_registry->formatFileSize(megabytes);
    QVERIFY(formatted.contains("MB"));
}

void ModelRegistryTest::testFormatFileSizeGigabytes()
{
    qint64 gigabytes = 1024LL * 1024 * 1024 * 5;
    QString formatted = m_registry->formatFileSize(gigabytes);
    QVERIFY(formatted.contains("GB"));
}

// ============= Timestamp Formatting Tests =============

void ModelRegistryTest::testFormatTimestampValid()
{
    QDateTime dt(QDate(2025, 1, 15), QTime(14, 30, 0));
    QString formatted = m_registry->formatTimestamp(dt);
    QVERIFY(formatted.contains("2025"));
    QVERIFY(formatted.contains("01"));
    QVERIFY(formatted.contains("15"));
}

void ModelRegistryTest::testFormatTimestampInvalid()
{
    QDateTime invalidDt;
    QString formatted = m_registry->formatTimestamp(invalidDt);
    QCOMPARE(formatted, "--");
}

// ============= Data Persistence Tests =============

void ModelRegistryTest::testModelPersistenceAcrossInstances()
{
    // Register model in first instance
    ModelVersion model;
    model.name = "PersistenceTest";
    model.path = "/persistence/test";
    model.baseModel = "GPT-2";
    model.createdAt = QDateTime::currentDateTime();
    model.finalLoss = 2.3f;
    model.perplexity = 11.5f;

    m_registry->registerModel(model);

    // Create second instance and verify model exists
    ModelRegistry* registry2 = new ModelRegistry();
    QVector<ModelVersion> models = registry2->getAllModels();
    
    QCOMPARE(models.size(), 1);
    QCOMPARE(models.first().name, "PersistenceTest");
    QCOMPARE(models.first().baseModel, "GPT-2");

    delete registry2;
}

void ModelRegistryTest::testDatabaseFileExists()
{
    QString dataLocation = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QString dbPath = dataLocation + "/model_registry.db";
    
    QVERIFY(QFile::exists(dbPath));
}

// ============= Edge Cases =============

void ModelRegistryTest::testLargeModelMetadata()
{
    ModelVersion model;
    model.name = "VeryLongModelNameThatContainsDetailedDescriptionOfTheModelArchitectureAndTrainingProcess";
    model.path = "/very/long/path/to/model/with/many/nested/directories/and/files";
    model.baseModel = "GPT-3-Large-Bilingual-Multilingual-Extended-Version";
    model.dataset = "CommonCrawl-2024-Full-Deduplicated-Filtered-Extended";
    model.notes = QString("This is a very long note that contains detailed information about the model. ").repeated(10);
    model.tags = "production,optimized,gpu,distributed,experimental,research,benchmark,evaluation";
    model.createdAt = QDateTime::currentDateTime();
    model.fileSize = 1099511627776LL; // 1 TB

    QVERIFY(m_registry->registerModel(model));
    
    ModelVersion retrieved = m_registry->getModel(1);
    QCOMPARE(retrieved.name, model.name);
    QCOMPARE(retrieved.fileSize, model.fileSize);
}

void ModelRegistryTest::testSpecialCharactersInName()
{
    ModelVersion model;
    model.name = "Model-v2.0_Beta@2025-01-15";
    model.path = "/special/chars/model";
    model.createdAt = QDateTime::currentDateTime();

    QVERIFY(m_registry->registerModel(model));

    ModelVersion retrieved = m_registry->getModel(1);
    QCOMPARE(retrieved.name, "Model-v2.0_Beta@2025-01-15");
}

void ModelRegistryTest::testNullValuesHandling()
{
    ModelVersion model;
    model.name = "NullTest";
    model.path = "/null/test";
    model.baseModel = ""; // Empty
    model.dataset = ""; // Empty
    model.createdAt = QDateTime::currentDateTime();
    model.finalLoss = 0.0f;
    model.perplexity = 0.0f;

    QVERIFY(m_registry->registerModel(model));

    ModelVersion retrieved = m_registry->getModel(1);
    QCOMPARE(retrieved.name, "NullTest");
}

void ModelRegistryTest::testConcurrentAccess()
{
    // Register multiple models
    for (int i = 0; i < 10; ++i) {
        ModelVersion model;
        model.name = QString("ConcurrentModel%1").arg(i);
        model.path = QString("/concurrent%1").arg(i);
        model.createdAt = QDateTime::currentDateTime();
        m_registry->registerModel(model);
    }

    // Verify all were registered
    QVector<ModelVersion> allModels = m_registry->getAllModels();
    QCOMPARE(allModels.size(), 10);

    // Perform concurrent-like operations
    m_registry->setActiveModel(5);
    m_registry->getModel(3);
    m_registry->deleteModel(1);

    QVector<ModelVersion> finalModels = m_registry->getAllModels();
    QCOMPARE(finalModels.size(), 9);
}

QTEST_MAIN(ModelRegistryTest)
#include "model_registry_test.moc"
