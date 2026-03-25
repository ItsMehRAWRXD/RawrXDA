#include "training_dialog.h"
#include "model_trainer.h"
<<<<<<< HEAD
#include "win32_file_dialog.h"
#include <any>
#include <unordered_map>
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>


TrainingDialog::TrainingDialog(ModelTrainer* trainer, void* parent) : DialogBase(parent), m_trainer(trainer)
=======


TrainingDialog::TrainingDialog(ModelTrainer* trainer, void* parent)
    : void(parent)
    , m_trainer(trainer)
>>>>>>> origin/main
{
    setWindowTitle("Configure Model Training");
    setMinimumSize(700, 600);
    setModal(true);
}

void TrainingDialog::initialize()
{
    if (m_datasetPathEdit)
        return;  // Already initialized

    setupUI();
    setupConnections();
    loadDefaultSettings();
}

void TrainingDialog::setupUI()
{
<<<<<<< HEAD
    VerticalLayout* mainLayout = new VerticalLayout(this);

    GroupBox* datasetGroup = new GroupBox("Dataset Configuration", this);
    VerticalLayout* datasetLayout = new VerticalLayout(datasetGroup);

    HorizontalLayout* datasetPathLayout = new HorizontalLayout();
    Label* datasetLabel = new Label("Dataset Path:", this);
    m_datasetPathEdit = new LineEdit(this);
    m_datasetPathEdit->setPlaceholderText("Path to training dataset (CSV, JSON-L, or plain text)");
    m_browseDatasetBtn = new PushButton("Browse...", this);
=======
    void* mainLayout = new void(this);

    // ===== Dataset Selection Group =====
    void* datasetGroup = new void("Dataset Configuration", this);
    void* datasetLayout = new void(datasetGroup);

    void* datasetPathLayout = new void();
    void* datasetLabel = new void("Dataset Path:", this);
    m_datasetPathEdit = new void(this);
    m_datasetPathEdit->setPlaceholderText("Path to training dataset (CSV, JSON-L, or plain text)");
    m_browseDatasetBtn = new void("Browse...", this);
>>>>>>> origin/main
    datasetPathLayout->addWidget(datasetLabel, 0);
    datasetPathLayout->addWidget(m_datasetPathEdit, 1);
    datasetPathLayout->addWidget(m_browseDatasetBtn, 0);

<<<<<<< HEAD
    HorizontalLayout* formatLayout = new HorizontalLayout();
    Label* formatLabel = new Label("Dataset Format:", this);
    m_datasetFormatCombo = new ComboBox(this);
=======
    void* formatLayout = new void();
    void* formatLabel = new void("Dataset Format:", this);
    m_datasetFormatCombo = new void(this);
>>>>>>> origin/main
    m_datasetFormatCombo->addItem("Auto-detect", -1);
    m_datasetFormatCombo->addItem("Plain Text", 0);
    m_datasetFormatCombo->addItem("JSON Lines", 1);
    m_datasetFormatCombo->addItem("CSV", 2);
    formatLayout->addWidget(formatLabel);
    formatLayout->addWidget(m_datasetFormatCombo);
    formatLayout->addStretch();

<<<<<<< HEAD
    m_datasetInfoLabel = new Label("", this);
    m_datasetInfoLabel->setStyleSheet("Label { color: gray; font-style: italic; }");
=======
    m_datasetInfoLabel = new void("", this);
    m_datasetInfoLabel->setStyleSheet("void { color: gray; font-style: italic; }");
>>>>>>> origin/main

    datasetLayout->addLayout(datasetPathLayout);
    datasetLayout->addLayout(formatLayout);
    datasetLayout->addWidget(m_datasetInfoLabel);

<<<<<<< HEAD
    GroupBox* modelGroup = new GroupBox("Base Model Configuration", this);
    VerticalLayout* modelLayout = new VerticalLayout(modelGroup);

    HorizontalLayout* modelPathLayout = new HorizontalLayout();
    Label* modelLabel = new Label("Model Path:", this);
    m_modelPathEdit = new LineEdit(this);
    m_modelPathEdit->setPlaceholderText("Path to base GGUF model");
    m_browseModelBtn = new PushButton("Browse...", this);
=======
    // ===== Model Selection Group =====
    void* modelGroup = new void("Base Model Configuration", this);
    void* modelLayout = new void(modelGroup);

    void* modelPathLayout = new void();
    void* modelLabel = new void("Model Path:", this);
    m_modelPathEdit = new void(this);
    m_modelPathEdit->setPlaceholderText("Path to base GGUF model");
    m_browseModelBtn = new void("Browse...", this);
>>>>>>> origin/main
    modelPathLayout->addWidget(modelLabel, 0);
    modelPathLayout->addWidget(m_modelPathEdit, 1);
    modelPathLayout->addWidget(m_browseModelBtn, 0);

<<<<<<< HEAD
    m_modelInfoLabel = new Label("", this);
    m_modelInfoLabel->setStyleSheet("Label { color: gray; font-style: italic; }");
=======
    m_modelInfoLabel = new void("", this);
    m_modelInfoLabel->setStyleSheet("void { color: gray; font-style: italic; }");
>>>>>>> origin/main

    modelLayout->addLayout(modelPathLayout);
    modelLayout->addWidget(m_modelInfoLabel);

<<<<<<< HEAD
    GroupBox* outputGroup = new GroupBox("Output Configuration", this);
    VerticalLayout* outputLayout = new VerticalLayout(outputGroup);

    HorizontalLayout* outputPathLayout = new HorizontalLayout();
    Label* outputLabel = new Label("Output Path:", this);
    m_outputPathEdit = new LineEdit(this);
    m_outputPathEdit->setPlaceholderText("Path to save fine-tuned model (will create .gguf file)");
    m_browseOutputBtn = new PushButton("Browse...", this);
=======
    // ===== Output Configuration Group =====
    void* outputGroup = new void("Output Configuration", this);
    void* outputLayout = new void(outputGroup);

    void* outputPathLayout = new void();
    void* outputLabel = new void("Output Path:", this);
    m_outputPathEdit = new void(this);
    m_outputPathEdit->setPlaceholderText("Path to save fine-tuned model (will create .gguf file)");
    m_browseOutputBtn = new void("Browse...", this);
>>>>>>> origin/main
    outputPathLayout->addWidget(outputLabel, 0);
    outputPathLayout->addWidget(m_outputPathEdit, 1);
    outputPathLayout->addWidget(m_browseOutputBtn, 0);

    outputLayout->addLayout(outputPathLayout);

<<<<<<< HEAD
    GroupBox* hyperparamsGroup = new GroupBox("Training Hyperparameters", this);
    GridLayout* hyperparamsLayout = new GridLayout(hyperparamsGroup);

    Label* epochsLabel = new Label("Epochs:", this);
    m_epochsSpinBox = new SpinBox(this);
=======
    // ===== Hyperparameters Group =====
    void* hyperparamsGroup = new void("Training Hyperparameters", this);
    void* hyperparamsLayout = new void(hyperparamsGroup);

    // Epochs
    void* epochsLabel = new void("Epochs:", this);
    m_epochsSpinBox = nullptr;
>>>>>>> origin/main
    m_epochsSpinBox->setMinimum(1);
    m_epochsSpinBox->setMaximum(1000);
    m_epochsSpinBox->setValue(3);
    m_epochsSpinBox->setToolTip("Number of complete passes through the training dataset");
    hyperparamsLayout->addWidget(epochsLabel, 0, 0);
    hyperparamsLayout->addWidget(m_epochsSpinBox, 0, 1);

<<<<<<< HEAD
    Label* lrLabel = new Label("Learning Rate:", this);
    m_learningRateSpinBox = new DoubleSpinBox(this);
=======
    // Learning Rate
    void* lrLabel = new void("Learning Rate:", this);
    m_learningRateSpinBox = nullptr;
>>>>>>> origin/main
    m_learningRateSpinBox->setMinimum(0.000001);
    m_learningRateSpinBox->setMaximum(1.0);
    m_learningRateSpinBox->setDecimals(6);
    m_learningRateSpinBox->setSingleStep(0.00001);
    m_learningRateSpinBox->setValue(0.0001);
    m_learningRateSpinBox->setToolTip("Step size for weight updates (AdamW optimizer)");
    hyperparamsLayout->addWidget(lrLabel, 0, 2);
    hyperparamsLayout->addWidget(m_learningRateSpinBox, 0, 3);

<<<<<<< HEAD
    Label* batchLabel = new Label("Batch Size:", this);
    m_batchSizeSpinBox = new SpinBox(this);
=======
    // Batch Size
    void* batchLabel = new void("Batch Size:", this);
    m_batchSizeSpinBox = nullptr;
>>>>>>> origin/main
    m_batchSizeSpinBox->setMinimum(1);
    m_batchSizeSpinBox->setMaximum(256);
    m_batchSizeSpinBox->setValue(4);
    m_batchSizeSpinBox->setToolTip("Number of sequences processed together (affects memory usage)");
    hyperparamsLayout->addWidget(batchLabel, 1, 0);
    hyperparamsLayout->addWidget(m_batchSizeSpinBox, 1, 1);

<<<<<<< HEAD
    Label* seqLenLabel = new Label("Sequence Length:", this);
    m_sequenceLengthSpinBox = new SpinBox(this);
=======
    // Sequence Length
    void* seqLenLabel = new void("Sequence Length:", this);
    m_sequenceLengthSpinBox = nullptr;
>>>>>>> origin/main
    m_sequenceLengthSpinBox->setMinimum(128);
    m_sequenceLengthSpinBox->setMaximum(4096);
    m_sequenceLengthSpinBox->setSingleStep(128);
    m_sequenceLengthSpinBox->setValue(512);
    m_sequenceLengthSpinBox->setToolTip("Maximum number of tokens per sequence");
    hyperparamsLayout->addWidget(seqLenLabel, 1, 2);
    hyperparamsLayout->addWidget(m_sequenceLengthSpinBox, 1, 3);

<<<<<<< HEAD
    Label* gradClipLabel = new Label("Gradient Clip:", this);
    m_gradientClipSpinBox = new DoubleSpinBox(this);
=======
    // Gradient Clipping
    void* gradClipLabel = new void("Gradient Clip:", this);
    m_gradientClipSpinBox = nullptr;
>>>>>>> origin/main
    m_gradientClipSpinBox->setMinimum(0.1);
    m_gradientClipSpinBox->setMaximum(10.0);
    m_gradientClipSpinBox->setDecimals(2);
    m_gradientClipSpinBox->setSingleStep(0.1);
    m_gradientClipSpinBox->setValue(1.0);
    m_gradientClipSpinBox->setToolTip("Maximum gradient norm (prevents gradient explosion)");
    hyperparamsLayout->addWidget(gradClipLabel, 2, 0);
    hyperparamsLayout->addWidget(m_gradientClipSpinBox, 2, 1);

<<<<<<< HEAD
    Label* weightDecayLabel = new Label("Weight Decay:", this);
    m_weightDecaySpinBox = new DoubleSpinBox(this);
=======
    // Weight Decay
    void* weightDecayLabel = new void("Weight Decay:", this);
    m_weightDecaySpinBox = nullptr;
>>>>>>> origin/main
    m_weightDecaySpinBox->setMinimum(0.0);
    m_weightDecaySpinBox->setMaximum(1.0);
    m_weightDecaySpinBox->setDecimals(4);
    m_weightDecaySpinBox->setSingleStep(0.001);
    m_weightDecaySpinBox->setValue(0.01);
    m_weightDecaySpinBox->setToolTip("L2 regularization strength (prevents overfitting)");
    hyperparamsLayout->addWidget(weightDecayLabel, 2, 2);
    hyperparamsLayout->addWidget(m_weightDecaySpinBox, 2, 3);

<<<<<<< HEAD
    Label* warmupLabel = new Label("Warmup Ratio:", this);
    m_warmupStepsSpinBox = new DoubleSpinBox(this);
=======
    // Warmup Steps
    void* warmupLabel = new void("Warmup Ratio:", this);
    m_warmupStepsSpinBox = nullptr;
>>>>>>> origin/main
    m_warmupStepsSpinBox->setMinimum(0.0);
    m_warmupStepsSpinBox->setMaximum(0.5);
    m_warmupStepsSpinBox->setDecimals(2);
    m_warmupStepsSpinBox->setSingleStep(0.01);
    m_warmupStepsSpinBox->setValue(0.1);
    m_warmupStepsSpinBox->setToolTip("Fraction of training steps for learning rate warmup");
    hyperparamsLayout->addWidget(warmupLabel, 3, 0);
    hyperparamsLayout->addWidget(m_warmupStepsSpinBox, 3, 1);

<<<<<<< HEAD
    GroupBox* validationGroup = new GroupBox("Validation Options", this);
    VerticalLayout* validationLayout = new VerticalLayout(validationGroup);

    Label* valSplitLabel = new Label("Validation Split:", this);
    m_validationSplitSpinBox = new DoubleSpinBox(this);
=======
    // ===== Validation Options Group =====
    void* validationGroup = new void("Validation Options", this);
    void* validationLayout = new void(validationGroup);

    void* valSplitLabel = new void("Validation Split:", this);
    m_validationSplitSpinBox = nullptr;
>>>>>>> origin/main
    m_validationSplitSpinBox->setMinimum(0.0);
    m_validationSplitSpinBox->setMaximum(0.5);
    m_validationSplitSpinBox->setDecimals(2);
    m_validationSplitSpinBox->setSingleStep(0.05);
    m_validationSplitSpinBox->setValue(0.1);
    m_validationSplitSpinBox->setToolTip("Fraction of data reserved for validation (e.g., 0.1 = 10%)");

<<<<<<< HEAD
    m_validateEveryEpochCheckBox = new CheckBox(this);
=======
    m_validateEveryEpochCheckBox = nullptr;
>>>>>>> origin/main
    m_validateEveryEpochCheckBox->setChecked(true);
    m_validateEveryEpochCheckBox->setToolTip("Run validation after each training epoch");

    validationLayout->addWidget(valSplitLabel);
    validationLayout->addWidget(m_validationSplitSpinBox);
    validationLayout->addSpacing(20);
    validationLayout->addWidget(m_validateEveryEpochCheckBox);
    validationLayout->addStretch();

<<<<<<< HEAD
    HorizontalLayout* buttonLayout = new HorizontalLayout();
    buttonLayout->addStretch();
    m_startTrainingBtn = new PushButton("Start Training", this);
    m_startTrainingBtn->setEnabled(false);
    m_cancelBtn = new PushButton("Cancel", this);
=======
    // ===== Action Buttons =====
    void* buttonLayout = new void();
    buttonLayout->addStretch();
    m_startTrainingBtn = new void("Start Training", this);
    m_startTrainingBtn->setDefault(true);
    m_startTrainingBtn->setEnabled(false); // Disabled until valid config
    m_cancelBtn = new void("Cancel", this);
>>>>>>> origin/main
    buttonLayout->addWidget(m_startTrainingBtn);
    buttonLayout->addWidget(m_cancelBtn);

    // ===== Assemble Main Layout =====
    mainLayout->addWidget(datasetGroup);
    mainLayout->addWidget(modelGroup);
    mainLayout->addWidget(outputGroup);
    mainLayout->addWidget(hyperparamsGroup);
    mainLayout->addWidget(validationGroup);
    mainLayout->addLayout(buttonLayout);
}

void TrainingDialog::setupConnections()
{
<<<<<<< HEAD
    // Event wiring: connect browse buttons, combo, start/cancel to handlers (Win32: subclass or ID-based dispatch).
=======
    // Browse buttons
// Qt connect removed
// Qt connect removed
// Qt connect removed
    // Dataset format combo
// Qt connect removed
    // Action buttons
// Qt connect removed
// Qt connect removed
    // Validation triggers
// Qt connect removed
// Qt connect removed
// Qt connect removed
>>>>>>> origin/main
}

void TrainingDialog::loadDefaultSettings()
{
<<<<<<< HEAD
    SimpleSettings settings("RawrXD", "AgenticIDE");
=======
    void* settings("RawrXD", "AgenticIDE");
>>>>>>> origin/main

    // Load last used paths
    m_datasetPathEdit->setText(settings.value("training/lastDatasetPath", "").toString());
    m_modelPathEdit->setText(settings.value("training/lastModelPath", "").toString());
    m_outputPathEdit->setText(settings.value("training/lastOutputPath", "").toString());

    // Load hyperparameters
    m_epochsSpinBox->setValue(settings.value("training/epochs", 3).toInt());
    m_learningRateSpinBox->setValue(settings.value("training/learningRate", 0.0001).toDouble());
    m_batchSizeSpinBox->setValue(settings.value("training/batchSize", 4).toInt());
    m_sequenceLengthSpinBox->setValue(settings.value("training/sequenceLength", 512).toInt());
    m_gradientClipSpinBox->setValue(settings.value("training/gradientClip", 1.0).toDouble());
    m_weightDecaySpinBox->setValue(settings.value("training/weightDecay", 0.01).toDouble());
    m_warmupStepsSpinBox->setValue(settings.value("training/warmupSteps", 0.1).toDouble());
    m_validationSplitSpinBox->setValue(settings.value("training/validationSplit", 0.1).toDouble());
    m_validateEveryEpochCheckBox->setChecked(settings.value("training/validateEveryEpoch", true).toBool());

    validateInputs();
}

void TrainingDialog::saveSettings()
{
<<<<<<< HEAD
    SimpleSettings settings("RawrXD", "AgenticIDE");
=======
    void* settings("RawrXD", "AgenticIDE");
>>>>>>> origin/main

    // Save paths
    settings.setValue("training/lastDatasetPath", m_datasetPathEdit->text());
    settings.setValue("training/lastModelPath", m_modelPathEdit->text());
    settings.setValue("training/lastOutputPath", m_outputPathEdit->text());

    // Save hyperparameters
    settings.setValue("training/epochs", m_epochsSpinBox->value());
    settings.setValue("training/learningRate", m_learningRateSpinBox->value());
    settings.setValue("training/batchSize", m_batchSizeSpinBox->value());
    settings.setValue("training/sequenceLength", m_sequenceLengthSpinBox->value());
    settings.setValue("training/gradientClip", m_gradientClipSpinBox->value());
    settings.setValue("training/weightDecay", m_weightDecaySpinBox->value());
    settings.setValue("training/warmupSteps", m_warmupStepsSpinBox->value());
    settings.setValue("training/validationSplit", m_validationSplitSpinBox->value());
    settings.setValue("training/validateEveryEpoch", m_validateEveryEpochCheckBox->isChecked());
}

void TrainingDialog::onBrowseDataset()
{
<<<<<<< HEAD
    const char* filter = "Dataset (*.csv;*.jsonl;*.txt)\0*.csv;*.jsonl;*.txt\0CSV (*.csv)\0*.csv\0JSON Lines "
                         "(*.jsonl)\0*.jsonl\0Text (*.txt)\0*.txt\0All (*.*)\0*.*\0";
    std::string path = RawrXD::getOpenFileName(this, "Select Training Dataset", filter);

    if (!path.empty())
    {
=======
    std::string path = QFileDialog::getOpenFileName(
        this,
        "Select Training Dataset",
        "",
        "Dataset Files (*.csv *.jsonl *.txt);;CSV Files (*.csv);;JSON Lines (*.jsonl);;Text Files (*.txt);;All Files (*)"
    );

    if (!path.empty()) {
>>>>>>> origin/main
        m_datasetPathEdit->setText(path);

        // Auto-detect format
        std::string format = detectDatasetFormat(path);
        m_datasetInfoLabel->setText("Detected format: " + format);

        // Update combo box
        if (format == "Plain Text")
        {
            m_datasetFormatCombo->setCurrentIndex(1);
        }
        else if (format == "JSON Lines")
        {
            m_datasetFormatCombo->setCurrentIndex(2);
        }
        else if (format == "CSV")
        {
            m_datasetFormatCombo->setCurrentIndex(3);
        }

        // Show file info
        std::filesystem::path info(path);
<<<<<<< HEAD
        double sizeMb = std::filesystem::file_size(info) / (1024.0 * 1024.0);
        m_datasetInfoLabel->setText("Format: " + format + " | Size: " + std::to_string(sizeMb) + " MB");
=======
        m_datasetInfoLabel->setText(std::string("Format: %1 | Size: %2 MB")
            
             / (1024.0 * 1024.0), 0, 'f', 2));
>>>>>>> origin/main
    }
}

void TrainingDialog::onBrowseModel()
{
<<<<<<< HEAD
    const char* filter = "GGUF (*.gguf)\0*.gguf\0All (*.*)\0*.*\0";
    std::string path = RawrXD::getOpenFileName(this, "Select Base Model", filter);

    if (!path.empty())
    {
=======
    std::string path = QFileDialog::getOpenFileName(
        this,
        "Select Base Model",
        "",
        "GGUF Files (*.gguf);;All Files (*)"
    );

    if (!path.empty()) {
>>>>>>> origin/main
        m_modelPathEdit->setText(path);

        // Show file info
        std::filesystem::path info(path);
<<<<<<< HEAD
        double sizeMb = std::filesystem::file_size(info) / (1024.0 * 1024.0);
        m_modelInfoLabel->setText("Size: " + std::to_string(sizeMb) + " MB");
=======
        m_modelInfoLabel->setText(std::string("Size: %1 MB | %2")
             / (1024.0 * 1024.0), 0, 'f', 2)
            ));
>>>>>>> origin/main
    }
}

void TrainingDialog::onBrowseOutputPath()
{
<<<<<<< HEAD
    const char* filter = "GGUF (*.gguf)\0*.gguf\0All (*.*)\0*.*\0";
    std::string path = RawrXD::getSaveFileName(this, "Save Fine-Tuned Model", filter, "gguf");

    if (!path.empty())
    {
        // Ensure .gguf extension
        if (!path.ends_with(".gguf"))
        {
=======
    std::string path = QFileDialog::getSaveFileName(
        this,
        "Save Fine-Tuned Model",
        "",
        "GGUF Files (*.gguf);;All Files (*)"
    );

    if (!path.empty()) {
        // Ensure .gguf extension
        if (!path.endsWith(".gguf", //CaseInsensitive)) {
>>>>>>> origin/main
            path += ".gguf";
        }
        m_outputPathEdit->setText(path);
    }
}

void TrainingDialog::onStartTraining()
{
    if (!validateConfiguration())
    {
        return;
    }

    saveSettings();

    void* config = getTrainingConfig();
    trainingStartRequested(config);

    accept();  // Close dialog
}

void TrainingDialog::onCancel()
{
    trainingCancelled();
    reject();
}

void TrainingDialog::onDatasetFormatChanged(int index)
{
    (index);
    validateInputs();
}

void TrainingDialog::validateInputs()
{
<<<<<<< HEAD
    bool isValid = !m_datasetPathEdit->text().empty() && !m_modelPathEdit->text().empty() &&
                   !m_outputPathEdit->text().empty() && std::filesystem::exists(m_datasetPathEdit->text()) &&
                   std::filesystem::exists(m_modelPathEdit->text());
=======
    bool isValid = !m_datasetPathEdit->text().empty() &&
                   !m_modelPathEdit->text().empty() &&
                   !m_outputPathEdit->text().empty() &&
                   std::filesystem::path::exists(m_datasetPathEdit->text()) &&
                   std::filesystem::path::exists(m_modelPathEdit->text());
>>>>>>> origin/main

    m_startTrainingBtn->setEnabled(isValid);

    // Update status
    if (isValid)
    {
        m_startTrainingBtn->setToolTip("Start training with current configuration");
<<<<<<< HEAD
    }
    else
    {
        if (m_datasetPathEdit->text().empty())
        {
            m_startTrainingBtn->setToolTip("Please select a dataset file");
        }
        else if (m_modelPathEdit->text().empty())
        {
            m_startTrainingBtn->setToolTip("Please select a base model file");
        }
        else if (m_outputPathEdit->text().empty())
        {
            m_startTrainingBtn->setToolTip("Please specify output path");
        }
        else if (!std::filesystem::exists(m_datasetPathEdit->text()))
        {
            m_startTrainingBtn->setToolTip("Dataset file does not exist");
        }
        else if (!std::filesystem::exists(m_modelPathEdit->text()))
        {
=======
    } else {
        if (m_datasetPathEdit->text().empty()) {
            m_startTrainingBtn->setToolTip("Please select a dataset file");
        } else if (m_modelPathEdit->text().empty()) {
            m_startTrainingBtn->setToolTip("Please select a base model file");
        } else if (m_outputPathEdit->text().empty()) {
            m_startTrainingBtn->setToolTip("Please specify output path");
        } else if (!std::filesystem::path::exists(m_datasetPathEdit->text())) {
            m_startTrainingBtn->setToolTip("Dataset file does not exist");
        } else if (!std::filesystem::path::exists(m_modelPathEdit->text())) {
>>>>>>> origin/main
            m_startTrainingBtn->setToolTip("Model file does not exist");
        }
    }
}

bool TrainingDialog::validateConfiguration() const
{
    // Check required fields
<<<<<<< HEAD
    if (m_datasetPathEdit->text().empty())
    {
        MessageBoxA(nullptr, "Please select a training dataset file.", "Missing Dataset", MB_OK | MB_ICONWARNING);
        return false;
    }

    if (m_modelPathEdit->text().empty())
    {
        MessageBoxA(nullptr, "Please select a base model file.", "Missing Model", MB_OK | MB_ICONWARNING);
        return false;
    }

    if (m_outputPathEdit->text().empty())
    {
        MessageBoxA(nullptr, "Please specify where to save the fine-tuned model.", "Missing Output Path",
                    MB_OK | MB_ICONWARNING);
        return false;
    }

    if (!std::filesystem::exists(m_datasetPathEdit->text()))
    {
        std::string msg = "Dataset file does not exist:\n" + m_datasetPathEdit->text();
        MessageBoxA(nullptr, msg.c_str(), "Dataset Not Found", MB_OK | MB_ICONWARNING);
        return false;
    }

    if (!std::filesystem::exists(m_modelPathEdit->text()))
    {
        std::string msg = "Model file does not exist:\n" + m_modelPathEdit->text();
        MessageBoxA(nullptr, msg.c_str(), "Model Not Found", MB_OK | MB_ICONWARNING);
=======
    if (m_datasetPathEdit->text().empty()) {
        QMessageBox::warning(const_cast<TrainingDialog*>(this), "Missing Dataset",
            "Please select a training dataset file.");
        return false;
    }

    if (m_modelPathEdit->text().empty()) {
        QMessageBox::warning(const_cast<TrainingDialog*>(this), "Missing Model",
            "Please select a base model file.");
        return false;
    }

    if (m_outputPathEdit->text().empty()) {
        QMessageBox::warning(const_cast<TrainingDialog*>(this), "Missing Output Path",
            "Please specify where to save the fine-tuned model.");
        return false;
    }

    // Check file existence
    if (!std::filesystem::path::exists(m_datasetPathEdit->text())) {
        QMessageBox::warning(const_cast<TrainingDialog*>(this), "Dataset Not Found",
            std::string("Dataset file does not exist:\n%1")));
        return false;
    }

    if (!std::filesystem::path::exists(m_modelPathEdit->text())) {
        QMessageBox::warning(const_cast<TrainingDialog*>(this), "Model Not Found",
            std::string("Model file does not exist:\n%1")));
>>>>>>> origin/main
        return false;
    }

    if (m_epochsSpinBox->value() < 1)
    {
        MessageBoxA(nullptr, "Number of epochs must be at least 1.", "Invalid Epochs", MB_OK | MB_ICONWARNING);
        return false;
    }

    if (m_learningRateSpinBox->value() <= 0.0)
    {
        MessageBoxA(nullptr, "Learning rate must be greater than 0.", "Invalid Learning Rate", MB_OK | MB_ICONWARNING);
        return false;
    }

    if (m_batchSizeSpinBox->value() < 1)
    {
        MessageBoxA(nullptr, "Batch size must be at least 1.", "Invalid Batch Size", MB_OK | MB_ICONWARNING);
        return false;
    }

    return true;
}

std::string TrainingDialog::detectDatasetFormat(const std::string& path) const
{
<<<<<<< HEAD
    if (path.ends_with(".csv"))
    {
        return "CSV";
    }
    else if (path.ends_with(".jsonl") || path.ends_with(".json"))
    {
        return "JSON Lines";
    }
    else if (path.ends_with(".txt"))
    {
=======
    if (path.endsWith(".csv", //CaseInsensitive)) {
        return "CSV";
    } else if (path.endsWith(".jsonl", //CaseInsensitive) || path.endsWith(".json", //CaseInsensitive)) {
        return "JSON Lines";
    } else if (path.endsWith(".txt", //CaseInsensitive)) {
>>>>>>> origin/main
        return "Plain Text";
    }
    return "Unknown";
}

void* TrainingDialog::getTrainingConfig() const
{
<<<<<<< HEAD
    auto* config = new std::unordered_map<std::string, std::any>();
=======
    void* config;
>>>>>>> origin/main

    // Paths
    (*config)["datasetPath"] = m_datasetPathEdit->text();
    (*config)["modelPath"] = m_modelPathEdit->text();
    (*config)["outputPath"] = m_outputPathEdit->text();

    // Dataset format
    int formatIndex = m_datasetFormatCombo->currentData().toInt();
<<<<<<< HEAD
    if (formatIndex >= 0)
    {
        (*config)["datasetFormat"] = formatIndex;
    }
    else
    {
        std::string format = detectDatasetFormat(m_datasetPathEdit->text());
        if (format == "Plain Text")
            (*config)["datasetFormat"] = 0;
        else if (format == "JSON Lines")
            (*config)["datasetFormat"] = 1;
        else if (format == "CSV")
            (*config)["datasetFormat"] = 2;
        else
            (*config)["datasetFormat"] = 0;
=======
    if (formatIndex >= 0) {
        config["datasetFormat"] = formatIndex;
    } else {
        // Auto-detect
        std::string format = detectDatasetFormat(m_datasetPathEdit->text());
        if (format == "Plain Text") config["datasetFormat"] = 0;
        else if (format == "JSON Lines") config["datasetFormat"] = 1;
        else if (format == "CSV") config["datasetFormat"] = 2;
        else config["datasetFormat"] = 0; // Default to plain text
>>>>>>> origin/main
    }

    // Hyperparameters
    (*config)["epochs"] = m_epochsSpinBox->value();
    (*config)["learningRate"] = m_learningRateSpinBox->value();
    (*config)["batchSize"] = m_batchSizeSpinBox->value();
    (*config)["sequenceLength"] = m_sequenceLengthSpinBox->value();
    (*config)["gradientClip"] = m_gradientClipSpinBox->value();
    (*config)["weightDecay"] = m_weightDecaySpinBox->value();
    (*config)["warmupSteps"] = m_warmupStepsSpinBox->value();

    (*config)["validationSplit"] = m_validationSplitSpinBox->value();
    (*config)["validateEveryEpoch"] = m_validateEveryEpochCheckBox->isChecked();

    return static_cast<void*>(config);
}


