// ai_digestion_panel_impl.cpp - Implementation continuation and slot implementations
#include "ai_digestion_panel.hpp"

void AIDigestionPanel::setupModelPresets() {
    if (!m_presetWidget) return;
    
    // Code Expert Preset
    QJsonObject codeExpertConfig;
    codeExpertConfig["model_size"] = "7GB (Medium)";
    codeExpertConfig["quantization"] = "Q4_1 (Balanced)";
    codeExpertConfig["extraction_mode"] = "Comprehensive";
    codeExpertConfig["extract_functions"] = true;
    codeExpertConfig["extract_classes"] = true;
    codeExpertConfig["extract_variables"] = true;
    codeExpertConfig["extract_comments"] = true;
    codeExpertConfig["learning_rate"] = 3e-5;
    codeExpertConfig["epochs"] = 15;
    codeExpertConfig["batch_size"] = 4;
    codeExpertConfig["max_tokens"] = 4096;
    codeExpertConfig["chunk_size"] = 1024;
    m_presetWidget->addPreset("Code Expert", "Optimized for C++, Python, JavaScript code analysis", codeExpertConfig);
    
    // Assembly Expert Preset
    QJsonObject asmExpertConfig;
    asmExpertConfig["model_size"] = "13GB (Large)";
    asmExpertConfig["quantization"] = "Q5_0 (Quality)";
    asmExpertConfig["extraction_mode"] = "Comprehensive";
    asmExpertConfig["extract_functions"] = true;
    asmExpertConfig["extract_variables"] = true;
    asmExpertConfig["extract_comments"] = true;
    asmExpertConfig["learning_rate"] = 1e-5;
    asmExpertConfig["epochs"] = 20;
    asmExpertConfig["batch_size"] = 2;
    asmExpertConfig["max_tokens"] = 8192;
    asmExpertConfig["chunk_size"] = 2048;
    m_presetWidget->addPreset("Assembly Expert", "Specialized for assembly language and reverse engineering", asmExpertConfig);
    
    // Security Expert Preset
    QJsonObject securityExpertConfig;
    securityExpertConfig["model_size"] = "30GB (XL)";
    securityExpertConfig["quantization"] = "Q5_1 (High Quality)";
    securityExpertConfig["extraction_mode"] = "Comprehensive";
    securityExpertConfig["extract_functions"] = true;
    securityExpertConfig["extract_classes"] = true;
    securityExpertConfig["extract_variables"] = true;
    securityExpertConfig["extract_comments"] = true;
    securityExpertConfig["learning_rate"] = 1e-5;
    securityExpertConfig["epochs"] = 25;
    securityExpertConfig["batch_size"] = 1;
    securityExpertConfig["max_tokens"] = 8192;
    securityExpertConfig["chunk_size"] = 2048;
    m_presetWidget->addPreset("Security Expert", "Focused on security analysis and malware research", securityExpertConfig);
    
    // General Purpose Preset
    QJsonObject generalConfig;
    generalConfig["model_size"] = "7GB (Medium)";
    generalConfig["quantization"] = "Q4_0 (Fast)";
    generalConfig["extraction_mode"] = "Semantic";
    generalConfig["extract_functions"] = true;
    generalConfig["extract_classes"] = true;
    generalConfig["extract_comments"] = true;
    generalConfig["learning_rate"] = 5e-5;
    generalConfig["epochs"] = 10;
    generalConfig["batch_size"] = 8;
    generalConfig["max_tokens"] = 2048;
    generalConfig["chunk_size"] = 512;
    m_presetWidget->addPreset("General Purpose", "Balanced configuration for general use", generalConfig);
}

void AIDigestionPanel::resetToDefaults() {
    m_modelNameEdit->setText("CustomAI");
    m_modelSizeCombo->setCurrentText("7GB (Medium)");
    m_quantizationCombo->setCurrentText("Q4_0 (Fast)");
    m_outputDirectoryEdit->setText(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/AI Models");
    m_maxTokensSpin->setValue(2048);
    m_chunkSizeSpin->setValue(512);
    m_overlapSizeSpin->setValue(64);
    
    m_extractionModeCombo->setCurrentText("Comprehensive");
    m_extractFunctionsCheck->setChecked(true);
    m_extractClassesCheck->setChecked(true);
    m_extractVariablesCheck->setChecked(true);
    m_extractCommentsCheck->setChecked(true);
    m_preserveStructureCheck->setChecked(true);
    m_minContentLengthSpin->setValue(50);
    
    m_learningRateSpin->setValue(5e-5);
    m_epochsSpin->setValue(10);
    m_batchSizeSpin->setValue(4);
    m_weightDecaySpin->setValue(0.0);
    m_warmupRatioSpin->setValue(0.03);
    m_schedulerCombo->setCurrentText("cosine");
    m_gradientCheckpointingCheck->setChecked(true);
    m_useFp16Check->setChecked(false);
}

#if 0
void AIDigestionPanel::onAddFilesClicked() {
    QStringList files = QFileDialog::getOpenFileNames(
        this, 
        "Select Files to Digest",
        QString(),
        "All Files (*.*);;"
        "Source Code (*.cpp *.hpp *.c *.h *.py *.js *.ts *.java *.cs *.php *.rb *.go *.rs);;"
        "Assembly (*.asm *.s *.inc);;"
        "Documentation (*.md *.txt *.rst *.doc *.docx);;"
        "Data Files (*.json *.xml *.yaml *.yml)"
    );
    
    if (!files.isEmpty()) {
        onFilesDropped(files);
    }
}

void AIDigestionPanel::onAddDirectoryClicked() {
    QString dir = QFileDialog::getExistingDirectory(this, "Select Directory to Digest");
    if (!dir.isEmpty()) {
        onFilesDropped(QStringList() << dir);
    }
}

void AIDigestionPanel::onClearFilesClicked() {
    m_fileListWidget->clear();
    m_inputFiles.clear();
    m_inputDirectories.clear();
    updateFileList();
}

void AIDigestionPanel::onFileItemChanged(QListWidgetItem* item) {
    // Handle file item state changes (e.g., enable/disable files)
    updateFileList();
}

void AIDigestionPanel::onFilesDropped(const QStringList& files) {
    for (const QString& file : files) {
        QFileInfo info(file);
        if (info.isFile()) {
            if (!m_inputFiles.contains(file)) {
                m_inputFiles.append(file);
                
                QListWidgetItem* item = new QListWidgetItem();
                item->setText(QString("%1 (%2)")
                    .arg(info.fileName())
                    .arg(formatFileSize(info.size())));
                item->setData(Qt::UserRole, file);
                item->setCheckState(Qt::Checked);
                item->setIcon(style()->standardIcon(QStyle::SP_FileIcon));
                m_fileListWidget->addItem(item);
            }
        } else if (info.isDir()) {
            if (!m_inputDirectories.contains(file)) {
                m_inputDirectories.append(file);
                
                QListWidgetItem* item = new QListWidgetItem();
                item->setText(QString("%1 (Directory)")
                    .arg(info.fileName()));
                item->setData(Qt::UserRole, file);
                item->setCheckState(Qt::Checked);
                item->setIcon(style()->standardIcon(QStyle::SP_DirIcon));
                m_fileListWidget->addItem(item);
            }
        }
    }
    updateFileList();
}

void AIDigestionPanel::onStartDigestionClicked() {
    if (m_inputFiles.isEmpty() && m_inputDirectories.isEmpty()) {
        showErrorMessage("No Input Files", "Please add files or directories to digest.");
        return;
    }
    
    validateInputs();
    updateDigestionConfig();
    
    QStringList allInputs = m_inputFiles + m_inputDirectories;
    
    m_isDigesting = true;
    m_startDigestionButton->setEnabled(false);
    m_pauseDigestionButton->setEnabled(true);
    m_stopDigestionButton->setEnabled(true);
    
    m_progressUpdateTimer->start();
    
    m_digestionEngine->startDigestion(allInputs);
    
    m_logTextEdit->append(QString("[%1] Started digestion of %2 inputs")
        .arg(QDateTime::currentDateTime().toString())
        .arg(allInputs.size()));
    
    emit digestionStarted();
}

void AIDigestionPanel::onStopDigestionClicked() {
    if (m_digestionEngine) {
        m_digestionEngine->stopDigestion();
    }
    
    m_isDigesting = false;
    m_startDigestionButton->setEnabled(true);
    m_pauseDigestionButton->setEnabled(false);
    m_resumeDigestionButton->setEnabled(false);
    m_stopDigestionButton->setEnabled(false);
    
    m_progressUpdateTimer->stop();
    
    m_logTextEdit->append(QString("[%1] Digestion stopped by user")
        .arg(QDateTime::currentDateTime().toString()));
}

void AIDigestionPanel::onPauseDigestionClicked() {
    if (m_digestionEngine) {
        m_digestionEngine->pauseDigestion();
    }
    
    m_pauseDigestionButton->setEnabled(false);
    m_resumeDigestionButton->setEnabled(true);
}

void AIDigestionPanel::onResumeDigestionClicked() {
    if (m_digestionEngine) {
        m_digestionEngine->resumeDigestion();
    }
    
    m_pauseDigestionButton->setEnabled(true);
    m_resumeDigestionButton->setEnabled(false);
}

void AIDigestionPanel::onStartTrainingClicked() {
    if (m_lastDataset.totalSamples == 0) {
        showErrorMessage("No Training Data", "Please complete digestion first to generate training data.");
        return;
    }
    
    updateTrainingConfig();
    
    if (!m_trainingPipeline->prepareTraining(m_lastDataset, m_digestionEngine->getConfig())) {
        showErrorMessage("Training Preparation Failed", "Failed to prepare training environment.");
        return;
    }
    
    m_isTraining = true;
    m_startTrainingButton->setEnabled(false);
    m_pauseTrainingButton->setEnabled(true);
    m_stopTrainingButton->setEnabled(true);
    
    m_trainingPipeline->startTraining();
    
    m_logTextEdit->append(QString("[%1] Started training with %2 samples")
        .arg(QDateTime::currentDateTime().toString())
        .arg(m_lastDataset.totalSamples));
    
    emit trainingStarted();
}

void AIDigestionPanel::onStopTrainingClicked() {
    if (m_trainingPipeline) {
        m_trainingPipeline->stopTraining();
    }
    
    m_isTraining = false;
    m_startTrainingButton->setEnabled(true);
    m_pauseTrainingButton->setEnabled(false);
    m_resumeTrainingButton->setEnabled(false);
    m_stopTrainingButton->setEnabled(false);
    
    m_logTextEdit->append(QString("[%1] Training stopped by user")
        .arg(QDateTime::currentDateTime().toString()));
}

void AIDigestionPanel::onPauseTrainingClicked() {
    if (m_trainingPipeline) {
        m_trainingPipeline->pauseTraining();
    }
    
    m_pauseTrainingButton->setEnabled(false);
    m_resumeTrainingButton->setEnabled(true);
}

void AIDigestionPanel::onResumeTrainingClicked() {
    if (m_trainingPipeline) {
        m_trainingPipeline->resumeTraining();
    }
    
    m_pauseTrainingButton->setEnabled(true);
    m_resumeTrainingButton->setEnabled(false);
}

void AIDigestionPanel::onModelSizeChanged(int sizeGB) {
    // Update model size configuration
    updateDigestionConfig();
}

void AIDigestionPanel::onQuantizationChanged(const QString& quantization) {
    // Update quantization configuration
    updateDigestionConfig();
}

void AIDigestionPanel::onExtractionModeChanged(int mode) {
    // Update extraction mode
    updateDigestionConfig();
}

void AIDigestionPanel::onLearningRateChanged(double rate) {
    updateTrainingConfig();
}

void AIDigestionPanel::onEpochsChanged(int epochs) {
    updateTrainingConfig();
}

void AIDigestionPanel::onBatchSizeChanged(int size) {
    updateTrainingConfig();
}

void AIDigestionPanel::onMaxTokensChanged(int tokens) {
    updateDigestionConfig();
}
#endif

void AIDigestionPanel::onDigestionProgress(double progress) {
    m_digestionProgressValue = progress;
    m_digestionProgress->setValue(static_cast<int>(progress * 100));
}

void AIDigestionPanel::onDigestionStatusChanged(const QString& status) {
    m_digestionStatusLabel->setText(status);
}

void AIDigestionPanel::onFileProcessed(const QString& filePath, int processedCount, int totalCount) {
    m_filesProcessedLabel->setText(QString("Files processed: %1 / %2").arg(processedCount).arg(totalCount));
    
    m_logTextEdit->append(QString("[%1] Processed: %2")
        .arg(QDateTime::currentDateTime().toString("hh:mm:ss"))
        .arg(QFileInfo(filePath).fileName()));
        
    if (m_autoScrollLogCheck->isChecked()) {
        m_logTextEdit->moveCursor(QTextCursor::End);
    }
}

void AIDigestionPanel::onDigestionDatasetReady(const TrainingDataset& dataset) {
    m_lastDataset = dataset;
    
    m_isDigesting = false;
    m_startDigestionButton->setEnabled(true);
    m_pauseDigestionButton->setEnabled(false);
    m_resumeDigestionButton->setEnabled(false);
    m_stopDigestionButton->setEnabled(false);
    
    // Enable training
    m_startTrainingButton->setEnabled(true);
    
    m_progressUpdateTimer->stop();
    
    showDigestionResults(dataset);
    
    m_logTextEdit->append(QString("[%1] Digestion completed successfully")
        .arg(QDateTime::currentDateTime().toString()));
    m_logTextEdit->append(QString("  - Total samples: %1").arg(dataset.totalSamples));
    m_logTextEdit->append(QString("  - Total tokens: %1").arg(dataset.totalTokens));
    
    emit digestionCompleted(QString());
}

void AIDigestionPanel::onDigestionFailed(const QString& error) {
    m_isDigesting = false;
    m_startDigestionButton->setEnabled(true);
    m_pauseDigestionButton->setEnabled(false);
    m_resumeDigestionButton->setEnabled(false);
    m_stopDigestionButton->setEnabled(false);
    
    m_progressUpdateTimer->stop();
    
    showErrorMessage("Digestion Failed", error);
    
    m_logTextEdit->append(QString("[%1] Digestion failed: %2")
        .arg(QDateTime::currentDateTime().toString())
        .arg(error));
}

void AIDigestionPanel::onTrainingProgress(double progress, const QJsonObject& metrics) {
    m_trainingProgressValue = progress;
    m_trainingProgress->setValue(static_cast<int>(progress * 100));
    
    m_metricsWidget->updateMetrics(metrics);
    
    QString metricsText = QString("Epoch: %1/%2, Loss: %3, Step: %4/%5")
        .arg(metrics["current_epoch"].toInt())
        .arg(metrics["total_epochs"].toInt())
        .arg(metrics["current_loss"].toDouble(), 0, 'f', 4)
        .arg(metrics["current_step"].toInt())
        .arg(metrics["total_steps"].toInt());
    
    m_trainingMetricsLabel->setText(metricsText);
}

void AIDigestionPanel::onTrainingStatusChanged(const QString& status) {
    m_trainingStatusLabel->setText(status);
}

void AIDigestionPanel::onEpochCompleted(int epoch, double loss, double accuracy) {
    m_logTextEdit->append(QString("[%1] Epoch %2 completed - Loss: %3, Accuracy: %4")
        .arg(QDateTime::currentDateTime().toString("hh:mm:ss"))
        .arg(epoch)
        .arg(loss, 0, 'f', 4)
        .arg(accuracy, 0, 'f', 4));
        
    if (m_autoScrollLogCheck->isChecked()) {
        m_logTextEdit->moveCursor(QTextCursor::End);
    }
}

void AIDigestionPanel::onTrainingFinished(const QString& modelPath) {
    m_currentModelPath = modelPath;
    
    m_isTraining = false;
    m_startTrainingButton->setEnabled(true);
    m_pauseTrainingButton->setEnabled(false);
    m_resumeTrainingButton->setEnabled(false);
    m_stopTrainingButton->setEnabled(false);
    
    showTrainingResults(modelPath);
    updateModelList();
    
    m_logTextEdit->append(QString("[%1] Training completed successfully")
        .arg(QDateTime::currentDateTime().toString()));
    m_logTextEdit->append(QString("  - Model saved to: %1").arg(modelPath));
    
    emit trainingCompleted(modelPath);
    emit modelCreated(m_modelNameEdit->text(), modelPath);
}

#if 0
void AIDigestionPanel::onTrainingError(const QString& error) {
    m_isTraining = false;
    m_startTrainingButton->setEnabled(true);
    m_pauseTrainingButton->setEnabled(false);
    m_resumeTrainingButton->setEnabled(false);
    m_stopTrainingButton->setEnabled(false);
    
    showErrorMessage("Training Failed", error);
    
    m_logTextEdit->append(QString("[%1] Training failed: %2")
        .arg(QDateTime::currentDateTime().toString())
        .arg(error));
}

void AIDigestionPanel::onModelValidated(const QString& modelPath, bool isValid) {
    QString status = isValid ? "Valid" : "Invalid";
    m_logTextEdit->append(QString("[%1] Model validation: %2 - %3")
        .arg(QDateTime::currentDateTime().toString())
        .arg(QFileInfo(modelPath).fileName())
        .arg(status));
}

void AIDigestionPanel::onModelQuantized(const QString& modelPath, const QString& quantization) {
    m_logTextEdit->append(QString("[%1] Model quantized: %2 -> %3")
        .arg(QDateTime::currentDateTime().toString())
        .arg(QFileInfo(modelPath).fileName())
        .arg(quantization));
}

void AIDigestionPanel::onLoadModelClicked() {
    QString modelPath = QFileDialog::getOpenFileName(
        this, 
        "Load Model", 
        m_outputDirectoryEdit->text(),
        "GGUF Models (*.gguf);;All Files (*.*)"
    );
    
    if (!modelPath.isEmpty()) {
        // Load model into inference engine
        m_logTextEdit->append(QString("[%1] Loading model: %2")
            .arg(QDateTime::currentDateTime().toString())
            .arg(QFileInfo(modelPath).fileName()));
    }
}

void AIDigestionPanel::onTestModelClicked() {
    int currentRow = m_modelsTable->currentRow();
    if (currentRow >= 0) {
        QString modelPath = m_modelsTable->item(currentRow, 4)->text();
        
        QStringList testPrompts = {
            "What is the purpose of this code?",
            "Explain how this function works.",
            "What are the security implications of this code?"
        };
        
        if (m_trainingPipeline->testModel(modelPath, testPrompts)) {
            m_logTextEdit->append(QString("[%1] Testing model: %2")
                .arg(QDateTime::currentDateTime().toString())
                .arg(QFileInfo(modelPath).fileName()));
        }
    }
}

void AIDigestionPanel::onExportModelClicked() {
    int currentRow = m_modelsTable->currentRow();
    if (currentRow >= 0) {
        QString modelPath = m_modelsTable->item(currentRow, 4)->text();
        QString exportPath = QFileDialog::getSaveFileName(
            this,
            "Export Model",
            QFileInfo(modelPath).fileName(),
            "GGUF Models (*.gguf);;All Files (*.*)"
        );
        
        if (!exportPath.isEmpty()) {
            if (QFile::copy(modelPath, exportPath)) {
                showSuccessMessage("Export Successful", 
                    QString("Model exported to: %1").arg(exportPath));
            } else {
                showErrorMessage("Export Failed", "Failed to copy model file.");
            }
        }
    }
}

void AIDigestionPanel::onDeleteModelClicked() {
    int currentRow = m_modelsTable->currentRow();
    if (currentRow >= 0) {
        QString modelName = m_modelsTable->item(currentRow, 0)->text();
        QString modelPath = m_modelsTable->item(currentRow, 4)->text();
        
        QMessageBox::StandardButton reply = QMessageBox::question(
            this,
            "Delete Model",
            QString("Are you sure you want to delete model '%1'?").arg(modelName),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No
        );
        
        if (reply == QMessageBox::Yes) {
            if (QFile::remove(modelPath)) {
                updateModelList();
                showSuccessMessage("Model Deleted", 
                    QString("Model '%1' has been deleted.").arg(modelName));
            } else {
                showErrorMessage("Deletion Failed", "Failed to delete model file.");
            }
        }
    }
}

void AIDigestionPanel::updateFileList() {
    int totalFiles = 0;
    qint64 totalSize = 0;
    
    for (int i = 0; i < m_fileListWidget->count(); ++i) {
        QListWidgetItem* item = m_fileListWidget->item(i);
        if (item->checkState() == Qt::Checked) {
            QString path = item->data(Qt::UserRole).toString();
            QFileInfo info(path);
            if (info.isFile()) {
                totalFiles++;
                totalSize += info.size();
            } else if (info.isDir()) {
                // Count files in directory
                QDirIterator iterator(path, QDir::Files, QDirIterator::Subdirectories);
                while (iterator.hasNext()) {
                    iterator.next();
                    totalFiles++;
                    totalSize += iterator.fileInfo().size();
                }
            }
        }
    }
    
    m_fileStatsLabel->setText(QString("%1 files selected (%2)")
        .arg(totalFiles)
        .arg(formatFileSize(totalSize)));
}

void AIDigestionPanel::updateParameterWidgets() {
    // Update any dynamic parameter widgets based on current state
}

void AIDigestionPanel::updateProgressDisplays() {
    if (m_digestionEngine) {
        double progress = m_digestionEngine->getProgress();
        QString status = m_digestionEngine->getStatusMessage();
        int processed = m_digestionEngine->getProcessedFiles();
        int total = m_digestionEngine->getTotalFiles();
        
        m_digestionProgress->setValue(static_cast<int>(progress * 100));
        m_digestionStatusLabel->setText(status);
        m_filesProcessedLabel->setText(QString("Files processed: %1 / %2").arg(processed).arg(total));
    }
    
    if (m_trainingPipeline) {
        double progress = m_trainingPipeline->getTrainingProgress();
        QString status = m_trainingPipeline->getTrainingStatus();
        
        m_trainingProgress->setValue(static_cast<int>(progress * 100));
        m_trainingStatusLabel->setText(status);
    }
}

void AIDigestionPanel::updateModelList() {
    m_modelsTable->setRowCount(0);
    
    QString modelsDir = m_outputDirectoryEdit->text();
    QDir dir(modelsDir);
    
    if (dir.exists()) {
        QStringList modelFiles = dir.entryList(QStringList() << "*.gguf" << "*.bin", QDir::Files);
        
        for (const QString& fileName : modelFiles) {
            QString fullPath = dir.filePath(fileName);
            QFileInfo info(fullPath);
            
            int row = m_modelsTable->rowCount();
            m_modelsTable->insertRow(row);
            
            m_modelsTable->setItem(row, 0, new QTableWidgetItem(info.baseName()));
            m_modelsTable->setItem(row, 1, new QTableWidgetItem(formatFileSize(info.size())));
            m_modelsTable->setItem(row, 2, new QTableWidgetItem("GGUF"));
            m_modelsTable->setItem(row, 3, new QTableWidgetItem(info.lastModified().toString()));
            m_modelsTable->setItem(row, 4, new QTableWidgetItem(fullPath));
        }
    }
}

void AIDigestionPanel::applyModelPreset(const QString& presetName) {
    QJsonObject config = m_presetWidget->getPresetConfig(presetName);
    
    if (config.contains("model_size")) {
        m_modelSizeCombo->setCurrentText(config["model_size"].toString());
    }
    if (config.contains("quantization")) {
        m_quantizationCombo->setCurrentText(config["quantization"].toString());
    }
    if (config.contains("extraction_mode")) {
        m_extractionModeCombo->setCurrentText(config["extraction_mode"].toString());
    }
    if (config.contains("extract_functions")) {
        m_extractFunctionsCheck->setChecked(config["extract_functions"].toBool());
    }
    if (config.contains("extract_classes")) {
        m_extractClassesCheck->setChecked(config["extract_classes"].toBool());
    }
    if (config.contains("extract_variables")) {
        m_extractVariablesCheck->setChecked(config["extract_variables"].toBool());
    }
    if (config.contains("extract_comments")) {
        m_extractCommentsCheck->setChecked(config["extract_comments"].toBool());
    }
    if (config.contains("learning_rate")) {
        m_learningRateSpin->setValue(config["learning_rate"].toDouble());
    }
    if (config.contains("epochs")) {
        m_epochsSpin->setValue(config["epochs"].toInt());
    }
    if (config.contains("batch_size")) {
        m_batchSizeSpin->setValue(config["batch_size"].toInt());
    }
    if (config.contains("max_tokens")) {
        m_maxTokensSpin->setValue(config["max_tokens"].toInt());
    }
    if (config.contains("chunk_size")) {
        m_chunkSizeSpin->setValue(config["chunk_size"].toInt());
    }
    
    updateDigestionConfig();
    updateTrainingConfig();
    
    showSuccessMessage("Preset Applied", 
        QString("Successfully applied '%1' preset configuration.").arg(presetName));
}

void AIDigestionPanel::validateInputs() {
    // Validate output directory
    QString outputDir = m_outputDirectoryEdit->text();
    QDir dir;
    if (!dir.exists(outputDir)) {
        dir.mkpath(outputDir);
    }
    
    // Validate model name
    QString modelName = m_modelNameEdit->text().trimmed();
    if (modelName.isEmpty()) {
        m_modelNameEdit->setText("CustomAI");
    }
}

void AIDigestionPanel::updateDigestionConfig() {
    if (!m_digestionEngine) return;
    
    DigestionConfig config;
    config.modelName = m_modelNameEdit->text();
    config.outputDirectory = m_outputDirectoryEdit->text();
    config.maxTokens = m_maxTokensSpin->value();
    config.chunkSize = m_chunkSizeSpin->value();
    config.overlapSize = m_overlapSizeSpin->value();
    
    // Extract quantization from combo text
    QString quantText = m_quantizationCombo->currentText();
    config.quantization = quantText.split(' ').first(); // Get "Q4_0" from "Q4_0 (Fast)"
    
    // Extract model size
    QString sizeText = m_modelSizeCombo->currentText();
    QRegularExpression sizeRegex(R"((\d+)GB)");
    auto match = sizeRegex.match(sizeText);
    if (match.hasMatch()) {
        config.modelSizeGB = match.captured(1).toInt();
    }
    
    // Extraction mode
    config.mode = static_cast<ExtractionMode>(m_extractionModeCombo->currentIndex());
    config.extractFunctions = m_extractFunctionsCheck->isChecked();
    config.extractClasses = m_extractClassesCheck->isChecked();
    config.extractVariables = m_extractVariablesCheck->isChecked();
    config.extractComments = m_extractCommentsCheck->isChecked();
    config.preserveStructure = m_preserveStructureCheck->isChecked();
    config.minContentLength = m_minContentLengthSpin->value();
    
    // Training parameters
    config.learningRate = m_learningRateSpin->value();
    config.epochs = m_epochsSpin->value();
    
    m_digestionEngine->setConfig(config);
}

void AIDigestionPanel::updateTrainingConfig() {
    if (!m_trainingPipeline) return;
    
    TrainingHyperparameters hyperparams;
    hyperparams.learningRate = m_learningRateSpin->value();
    hyperparams.weightDecay = m_weightDecaySpin->value();
    hyperparams.batchSize = m_batchSizeSpin->value();
    hyperparams.warmupRatio = m_warmupRatioSpin->value();
    hyperparams.scheduler = m_schedulerCombo->currentText();
    hyperparams.useGradientCheckpointing = m_gradientCheckpointingCheck->isChecked();
    hyperparams.useFp16 = m_useFp16Check->isChecked();
    
    m_trainingPipeline->setHyperparameters(hyperparams);
}

void AIDigestionPanel::showDigestionResults(const TrainingDataset& dataset) {
    QMessageBox msgBox(this);
    msgBox.setWindowTitle("Digestion Completed");
    msgBox.setIcon(QMessageBox::Information);
    msgBox.setText("Content digestion completed successfully!");
    
    QString details = QString(
        "Dataset Statistics:\n"
        "• Total samples: %1\n"
        "• Total tokens: %2\n"
        "• Processing time: %3\n\n"
        "The training dataset is ready. You can now start training your custom AI model."
    ).arg(dataset.totalSamples)
     .arg(dataset.totalTokens)
     .arg(formatDuration(dataset.statistics["processing_time_seconds"].toInt()));
    
    msgBox.setDetailedText(details);
    msgBox.exec();
}

void AIDigestionPanel::showTrainingResults(const QString& modelPath) {
    QMessageBox msgBox(this);
    msgBox.setWindowTitle("Training Completed");
    msgBox.setIcon(QMessageBox::Information);
    msgBox.setText("Model training completed successfully!");
    
    QString details = QString(
        "Your custom AI model has been trained and saved:\n\n"
        "Model path: %1\n"
        "Model name: %2\n"
        "Quantization: %3\n\n"
        "You can now load and test your custom AI model."
    ).arg(modelPath)
     .arg(m_modelNameEdit->text())
     .arg(m_quantizationCombo->currentText());
    
    msgBox.setDetailedText(details);
    msgBox.exec();
}

void AIDigestionPanel::showErrorMessage(const QString& title, const QString& message) {
    QMessageBox::critical(this, title, message);
}

void AIDigestionPanel::showSuccessMessage(const QString& title, const QString& message) {
    QMessageBox::information(this, title, message);
}

QString AIDigestionPanel::formatFileSize(qint64 bytes) {
    const qint64 KB = 1024;
    const qint64 MB = KB * 1024;
    const qint64 GB = MB * 1024;
    
    if (bytes >= GB) {
        return QString::number(bytes / (double)GB, 'f', 2) + " GB";
    } else if (bytes >= MB) {
        return QString::number(bytes / (double)MB, 'f', 1) + " MB";
    } else if (bytes >= KB) {
        return QString::number(bytes / (double)KB, 'f', 1) + " KB";
    } else {
        return QString::number(bytes) + " bytes";
    }
}

QString AIDigestionPanel::formatDuration(int seconds) {
    int hours = seconds / 3600;
    int minutes = (seconds % 3600) / 60;
    int secs = seconds % 60;
    
    if (hours > 0) {
        return QString("%1h %2m %3s").arg(hours).arg(minutes).arg(secs);
    } else if (minutes > 0) {
        return QString("%1m %2s").arg(minutes).arg(secs);
    } else {
        return QString("%1s").arg(secs);
    }
}

QString AIDigestionPanel::formatProgress(double progress) {
    return QString::number(progress * 100, 'f', 1) + "%";
}

// Slot implementations for other methods would continue here...

void AIDigestionPanel::saveCurrentSettings() {
    QString fileName = QFileDialog::getSaveFileName(
        this,
        "Save Settings",
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/ai_digestion_settings.json",
        "JSON Files (*.json)"
    );
    
    if (fileName.isEmpty()) return;
    
    QJsonObject settings;
    
    // Model configuration
    QJsonObject modelConfig;
    modelConfig["modelName"] = m_modelNameEdit->text();
    modelConfig["modelSize"] = m_modelSizeCombo->currentText();
    modelConfig["quantization"] = m_quantizationCombo->currentText();
    modelConfig["outputDirectory"] = m_outputDirectoryEdit->text();
    modelConfig["maxTokens"] = m_maxTokensSpin->value();
    modelConfig["chunkSize"] = m_chunkSizeSpin->value();
    modelConfig["overlapSize"] = m_overlapSizeSpin->value();
    settings["modelConfig"] = modelConfig;
    
    // Extraction configuration
    QJsonObject extractionConfig;
    extractionConfig["mode"] = m_extractionModeCombo->currentText();
    extractionConfig["extractFunctions"] = m_extractFunctionsCheck->isChecked();
    extractionConfig["extractClasses"] = m_extractClassesCheck->isChecked();
    extractionConfig["extractVariables"] = m_extractVariablesCheck->isChecked();
    extractionConfig["extractComments"] = m_extractCommentsCheck->isChecked();
    extractionConfig["preserveStructure"] = m_preserveStructureCheck->isChecked();
    extractionConfig["minContentLength"] = m_minContentLengthSpin->value();
    settings["extractionConfig"] = extractionConfig;
    
    // Training hyperparameters
    QJsonObject trainingConfig;
    trainingConfig["learningRate"] = m_learningRateSpin->value();
    trainingConfig["epochs"] = m_epochsSpin->value();
    trainingConfig["batchSize"] = m_batchSizeSpin->value();
    trainingConfig["weightDecay"] = m_weightDecaySpin->value();
    trainingConfig["warmupRatio"] = m_warmupRatioSpin->value();
    trainingConfig["scheduler"] = m_schedulerCombo->currentText();
    trainingConfig["gradientCheckpointing"] = m_gradientCheckpointingCheck->isChecked();
    trainingConfig["useFp16"] = m_useFp16Check->isChecked();
    settings["trainingConfig"] = trainingConfig;
    
    // Metadata
    settings["version"] = "1.0";
    settings["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    
    // Save to file
    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly)) {
        showErrorMessage("Save Error", "Could not open file for writing");
        return;
    }
    
    QJsonDocument doc(settings);
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();
    
    // Structured logging: Settings saved
    qInfo() << "[CONFIG] Settings saved:"
            << "file=" << fileName
            << "timestamp=" << QDateTime::currentDateTime().toString(Qt::ISODate);
    
    showSuccessMessage("Settings Saved", QString("Settings saved to: %1").arg(fileName));
}

void AIDigestionPanel::exportTrainingData() {
    if (m_lastDataset.totalSamples == 0) {
        showErrorMessage("No Data", "No training data available to export.");
        return;
    }
    
    QString fileName = QFileDialog::getSaveFileName(
        this,
        "Export Training Data",
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/training_dataset.json",
        "JSON Files (*.json)"
    );
    
    if (fileName.isEmpty()) return;
    
    if (m_digestionEngine->saveDataset(fileName)) {
        qInfo() << "[DATA] Training dataset exported:"
                << "file=" << fileName
                << "samples=" << m_lastDataset.totalSamples
                << "tokens=" << m_lastDataset.totalTokens;
        
        showSuccessMessage("Export Successful", 
            QString("Training data exported to: %1").arg(fileName));
    } else {
        showErrorMessage("Export Failed", "Failed to export training data.");
    }
}

void AIDigestionPanel::importTrainingData() {
    QString fileName = QFileDialog::getOpenFileName(
        this,
        "Import Training Data",
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation),
        "JSON Files (*.json)"
    );
    
    if (fileName.isEmpty()) return;
    
    if (m_digestionEngine->loadDataset(fileName)) {
        m_lastDataset = m_digestionEngine->getTrainingDataset();
        
        qInfo() << "[DATA] Training dataset imported:"
                << "file=" << fileName
                << "samples=" << m_lastDataset.totalSamples
                << "tokens=" << m_lastDataset.totalTokens;
        
        m_startTrainingButton->setEnabled(true);
        
        showSuccessMessage("Import Successful", 
            QString("Training data imported: %1 samples, %2 tokens")
                .arg(m_lastDataset.totalSamples)
                .arg(m_lastDataset.totalTokens));
    } else {
        showErrorMessage("Import Failed", "Failed to import training data.");
    }
}
