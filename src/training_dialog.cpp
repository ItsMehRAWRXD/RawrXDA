#include "training_dialog.h"
#include "model_trainer.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QCheckBox>
#include <QPushButton>
#include <QFileDialog>
#include <QMessageBox>
#include <QSettings>
#include <QFileInfo>
#include <QDebug>

TrainingDialog::TrainingDialog(ModelTrainer* trainer, QWidget* parent)
    : QDialog(parent)
    , m_trainer(trainer)
{
    setWindowTitle("Configure Model Training");
    setMinimumSize(700, 600);
    setModal(true);

    setupUI();
    setupConnections();
    loadDefaultSettings();
}

void TrainingDialog::setupUI()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    // ===== Dataset Selection Group =====
    QGroupBox* datasetGroup = new QGroupBox("Dataset Configuration", this);
    QVBoxLayout* datasetLayout = new QVBoxLayout(datasetGroup);

    QHBoxLayout* datasetPathLayout = new QHBoxLayout();
    QLabel* datasetLabel = new QLabel("Dataset Path:", this);
    m_datasetPathEdit = new QLineEdit(this);
    m_datasetPathEdit->setPlaceholderText("Path to training dataset (CSV, JSON-L, or plain text)");
    m_browseDatasetBtn = new QPushButton("Browse...", this);
    datasetPathLayout->addWidget(datasetLabel, 0);
    datasetPathLayout->addWidget(m_datasetPathEdit, 1);
    datasetPathLayout->addWidget(m_browseDatasetBtn, 0);

    QHBoxLayout* formatLayout = new QHBoxLayout();
    QLabel* formatLabel = new QLabel("Dataset Format:", this);
    m_datasetFormatCombo = new QComboBox(this);
    m_datasetFormatCombo->addItem("Auto-detect", -1);
    m_datasetFormatCombo->addItem("Plain Text", 0);
    m_datasetFormatCombo->addItem("JSON Lines", 1);
    m_datasetFormatCombo->addItem("CSV", 2);
    formatLayout->addWidget(formatLabel);
    formatLayout->addWidget(m_datasetFormatCombo);
    formatLayout->addStretch();

    m_datasetInfoLabel = new QLabel("", this);
    m_datasetInfoLabel->setStyleSheet("QLabel { color: gray; font-style: italic; }");

    datasetLayout->addLayout(datasetPathLayout);
    datasetLayout->addLayout(formatLayout);
    datasetLayout->addWidget(m_datasetInfoLabel);

    // ===== Model Selection Group =====
    QGroupBox* modelGroup = new QGroupBox("Base Model Configuration", this);
    QVBoxLayout* modelLayout = new QVBoxLayout(modelGroup);

    QHBoxLayout* modelPathLayout = new QHBoxLayout();
    QLabel* modelLabel = new QLabel("Model Path:", this);
    m_modelPathEdit = new QLineEdit(this);
    m_modelPathEdit->setPlaceholderText("Path to base GGUF model");
    m_browseModelBtn = new QPushButton("Browse...", this);
    modelPathLayout->addWidget(modelLabel, 0);
    modelPathLayout->addWidget(m_modelPathEdit, 1);
    modelPathLayout->addWidget(m_browseModelBtn, 0);

    m_modelInfoLabel = new QLabel("", this);
    m_modelInfoLabel->setStyleSheet("QLabel { color: gray; font-style: italic; }");

    modelLayout->addLayout(modelPathLayout);
    modelLayout->addWidget(m_modelInfoLabel);

    // ===== Output Configuration Group =====
    QGroupBox* outputGroup = new QGroupBox("Output Configuration", this);
    QVBoxLayout* outputLayout = new QVBoxLayout(outputGroup);

    QHBoxLayout* outputPathLayout = new QHBoxLayout();
    QLabel* outputLabel = new QLabel("Output Path:", this);
    m_outputPathEdit = new QLineEdit(this);
    m_outputPathEdit->setPlaceholderText("Path to save fine-tuned model (will create .gguf file)");
    m_browseOutputBtn = new QPushButton("Browse...", this);
    outputPathLayout->addWidget(outputLabel, 0);
    outputPathLayout->addWidget(m_outputPathEdit, 1);
    outputPathLayout->addWidget(m_browseOutputBtn, 0);

    outputLayout->addLayout(outputPathLayout);

    // ===== Hyperparameters Group =====
    QGroupBox* hyperparamsGroup = new QGroupBox("Training Hyperparameters", this);
    QGridLayout* hyperparamsLayout = new QGridLayout(hyperparamsGroup);

    // Epochs
    QLabel* epochsLabel = new QLabel("Epochs:", this);
    m_epochsSpinBox = new QSpinBox(this);
    m_epochsSpinBox->setMinimum(1);
    m_epochsSpinBox->setMaximum(1000);
    m_epochsSpinBox->setValue(3);
    m_epochsSpinBox->setToolTip("Number of complete passes through the training dataset");
    hyperparamsLayout->addWidget(epochsLabel, 0, 0);
    hyperparamsLayout->addWidget(m_epochsSpinBox, 0, 1);

    // Learning Rate
    QLabel* lrLabel = new QLabel("Learning Rate:", this);
    m_learningRateSpinBox = new QDoubleSpinBox(this);
    m_learningRateSpinBox->setMinimum(0.000001);
    m_learningRateSpinBox->setMaximum(1.0);
    m_learningRateSpinBox->setDecimals(6);
    m_learningRateSpinBox->setSingleStep(0.00001);
    m_learningRateSpinBox->setValue(0.0001);
    m_learningRateSpinBox->setToolTip("Step size for weight updates (AdamW optimizer)");
    hyperparamsLayout->addWidget(lrLabel, 0, 2);
    hyperparamsLayout->addWidget(m_learningRateSpinBox, 0, 3);

    // Batch Size
    QLabel* batchLabel = new QLabel("Batch Size:", this);
    m_batchSizeSpinBox = new QSpinBox(this);
    m_batchSizeSpinBox->setMinimum(1);
    m_batchSizeSpinBox->setMaximum(256);
    m_batchSizeSpinBox->setValue(4);
    m_batchSizeSpinBox->setToolTip("Number of sequences processed together (affects memory usage)");
    hyperparamsLayout->addWidget(batchLabel, 1, 0);
    hyperparamsLayout->addWidget(m_batchSizeSpinBox, 1, 1);

    // Sequence Length
    QLabel* seqLenLabel = new QLabel("Sequence Length:", this);
    m_sequenceLengthSpinBox = new QSpinBox(this);
    m_sequenceLengthSpinBox->setMinimum(128);
    m_sequenceLengthSpinBox->setMaximum(4096);
    m_sequenceLengthSpinBox->setSingleStep(128);
    m_sequenceLengthSpinBox->setValue(512);
    m_sequenceLengthSpinBox->setToolTip("Maximum number of tokens per sequence");
    hyperparamsLayout->addWidget(seqLenLabel, 1, 2);
    hyperparamsLayout->addWidget(m_sequenceLengthSpinBox, 1, 3);

    // Gradient Clipping
    QLabel* gradClipLabel = new QLabel("Gradient Clip:", this);
    m_gradientClipSpinBox = new QDoubleSpinBox(this);
    m_gradientClipSpinBox->setMinimum(0.1);
    m_gradientClipSpinBox->setMaximum(10.0);
    m_gradientClipSpinBox->setDecimals(2);
    m_gradientClipSpinBox->setSingleStep(0.1);
    m_gradientClipSpinBox->setValue(1.0);
    m_gradientClipSpinBox->setToolTip("Maximum gradient norm (prevents gradient explosion)");
    hyperparamsLayout->addWidget(gradClipLabel, 2, 0);
    hyperparamsLayout->addWidget(m_gradientClipSpinBox, 2, 1);

    // Weight Decay
    QLabel* weightDecayLabel = new QLabel("Weight Decay:", this);
    m_weightDecaySpinBox = new QDoubleSpinBox(this);
    m_weightDecaySpinBox->setMinimum(0.0);
    m_weightDecaySpinBox->setMaximum(1.0);
    m_weightDecaySpinBox->setDecimals(4);
    m_weightDecaySpinBox->setSingleStep(0.001);
    m_weightDecaySpinBox->setValue(0.01);
    m_weightDecaySpinBox->setToolTip("L2 regularization strength (prevents overfitting)");
    hyperparamsLayout->addWidget(weightDecayLabel, 2, 2);
    hyperparamsLayout->addWidget(m_weightDecaySpinBox, 2, 3);

    // Warmup Steps
    QLabel* warmupLabel = new QLabel("Warmup Ratio:", this);
    m_warmupStepsSpinBox = new QDoubleSpinBox(this);
    m_warmupStepsSpinBox->setMinimum(0.0);
    m_warmupStepsSpinBox->setMaximum(0.5);
    m_warmupStepsSpinBox->setDecimals(2);
    m_warmupStepsSpinBox->setSingleStep(0.01);
    m_warmupStepsSpinBox->setValue(0.1);
    m_warmupStepsSpinBox->setToolTip("Fraction of training steps for learning rate warmup");
    hyperparamsLayout->addWidget(warmupLabel, 3, 0);
    hyperparamsLayout->addWidget(m_warmupStepsSpinBox, 3, 1);

    // ===== Validation Options Group =====
    QGroupBox* validationGroup = new QGroupBox("Validation Options", this);
    QHBoxLayout* validationLayout = new QHBoxLayout(validationGroup);

    QLabel* valSplitLabel = new QLabel("Validation Split:", this);
    m_validationSplitSpinBox = new QDoubleSpinBox(this);
    m_validationSplitSpinBox->setMinimum(0.0);
    m_validationSplitSpinBox->setMaximum(0.5);
    m_validationSplitSpinBox->setDecimals(2);
    m_validationSplitSpinBox->setSingleStep(0.05);
    m_validationSplitSpinBox->setValue(0.1);
    m_validationSplitSpinBox->setToolTip("Fraction of data reserved for validation (e.g., 0.1 = 10%)");

    m_validateEveryEpochCheckBox = new QCheckBox("Validate Every Epoch", this);
    m_validateEveryEpochCheckBox->setChecked(true);
    m_validateEveryEpochCheckBox->setToolTip("Run validation after each training epoch");

    validationLayout->addWidget(valSplitLabel);
    validationLayout->addWidget(m_validationSplitSpinBox);
    validationLayout->addSpacing(20);
    validationLayout->addWidget(m_validateEveryEpochCheckBox);
    validationLayout->addStretch();

    // ===== Action Buttons =====
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();
    m_startTrainingBtn = new QPushButton("Start Training", this);
    m_startTrainingBtn->setDefault(true);
    m_startTrainingBtn->setEnabled(false); // Disabled until valid config
    m_cancelBtn = new QPushButton("Cancel", this);
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
    // Browse buttons
    connect(m_browseDatasetBtn, &QPushButton::clicked, this, &TrainingDialog::onBrowseDataset);
    connect(m_browseModelBtn, &QPushButton::clicked, this, &TrainingDialog::onBrowseModel);
    connect(m_browseOutputBtn, &QPushButton::clicked, this, &TrainingDialog::onBrowseOutputPath);

    // Dataset format combo
    connect(m_datasetFormatCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &TrainingDialog::onDatasetFormatChanged);

    // Action buttons
    connect(m_startTrainingBtn, &QPushButton::clicked, this, &TrainingDialog::onStartTraining);
    connect(m_cancelBtn, &QPushButton::clicked, this, &TrainingDialog::onCancel);

    // Validation triggers
    connect(m_datasetPathEdit, &QLineEdit::textChanged, this, &TrainingDialog::validateInputs);
    connect(m_modelPathEdit, &QLineEdit::textChanged, this, &TrainingDialog::validateInputs);
    connect(m_outputPathEdit, &QLineEdit::textChanged, this, &TrainingDialog::validateInputs);
}

void TrainingDialog::loadDefaultSettings()
{
    QSettings settings("RawrXD", "AgenticIDE");

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
    QSettings settings("RawrXD", "AgenticIDE");

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
    QString path = QFileDialog::getOpenFileName(
        this,
        "Select Training Dataset",
        "",
        "Dataset Files (*.csv *.jsonl *.txt);;CSV Files (*.csv);;JSON Lines (*.jsonl);;Text Files (*.txt);;All Files (*)"
    );

    if (!path.isEmpty()) {
        m_datasetPathEdit->setText(path);

        // Auto-detect format
        QString format = detectDatasetFormat(path);
        m_datasetInfoLabel->setText("Detected format: " + format);

        // Update combo box
        if (format == "Plain Text") {
            m_datasetFormatCombo->setCurrentIndex(1);
        } else if (format == "JSON Lines") {
            m_datasetFormatCombo->setCurrentIndex(2);
        } else if (format == "CSV") {
            m_datasetFormatCombo->setCurrentIndex(3);
        }

        // Show file info
        QFileInfo info(path);
        m_datasetInfoLabel->setText(QString("Format: %1 | Size: %2 MB")
            .arg(format)
            .arg(info.size() / (1024.0 * 1024.0), 0, 'f', 2));
    }
}

void TrainingDialog::onBrowseModel()
{
    QString path = QFileDialog::getOpenFileName(
        this,
        "Select Base Model",
        "",
        "GGUF Files (*.gguf);;All Files (*)"
    );

    if (!path.isEmpty()) {
        m_modelPathEdit->setText(path);

        // Show file info
        QFileInfo info(path);
        m_modelInfoLabel->setText(QString("Size: %1 MB | %2")
            .arg(info.size() / (1024.0 * 1024.0), 0, 'f', 2)
            .arg(info.fileName()));
    }
}

void TrainingDialog::onBrowseOutputPath()
{
    QString path = QFileDialog::getSaveFileName(
        this,
        "Save Fine-Tuned Model",
        "",
        "GGUF Files (*.gguf);;All Files (*)"
    );

    if (!path.isEmpty()) {
        // Ensure .gguf extension
        if (!path.endsWith(".gguf", Qt::CaseInsensitive)) {
            path += ".gguf";
        }
        m_outputPathEdit->setText(path);
    }
}

void TrainingDialog::onStartTraining()
{
    if (!validateConfiguration()) {
        return;
    }

    saveSettings();

    QJsonObject config = getTrainingConfig();
    emit trainingStartRequested(config);

    accept(); // Close dialog
}

void TrainingDialog::onCancel()
{
    emit trainingCancelled();
    reject();
}

void TrainingDialog::onDatasetFormatChanged(int index)
{
    Q_UNUSED(index);
    validateInputs();
}

void TrainingDialog::validateInputs()
{
    bool isValid = !m_datasetPathEdit->text().isEmpty() &&
                   !m_modelPathEdit->text().isEmpty() &&
                   !m_outputPathEdit->text().isEmpty() &&
                   QFileInfo::exists(m_datasetPathEdit->text()) &&
                   QFileInfo::exists(m_modelPathEdit->text());

    m_startTrainingBtn->setEnabled(isValid);

    // Update status
    if (isValid) {
        m_startTrainingBtn->setToolTip("Start training with current configuration");
    } else {
        if (m_datasetPathEdit->text().isEmpty()) {
            m_startTrainingBtn->setToolTip("Please select a dataset file");
        } else if (m_modelPathEdit->text().isEmpty()) {
            m_startTrainingBtn->setToolTip("Please select a base model file");
        } else if (m_outputPathEdit->text().isEmpty()) {
            m_startTrainingBtn->setToolTip("Please specify output path");
        } else if (!QFileInfo::exists(m_datasetPathEdit->text())) {
            m_startTrainingBtn->setToolTip("Dataset file does not exist");
        } else if (!QFileInfo::exists(m_modelPathEdit->text())) {
            m_startTrainingBtn->setToolTip("Model file does not exist");
        }
    }
}

bool TrainingDialog::validateConfiguration() const
{
    // Check required fields
    if (m_datasetPathEdit->text().isEmpty()) {
        QMessageBox::warning(const_cast<TrainingDialog*>(this), "Missing Dataset",
            "Please select a training dataset file.");
        return false;
    }

    if (m_modelPathEdit->text().isEmpty()) {
        QMessageBox::warning(const_cast<TrainingDialog*>(this), "Missing Model",
            "Please select a base model file.");
        return false;
    }

    if (m_outputPathEdit->text().isEmpty()) {
        QMessageBox::warning(const_cast<TrainingDialog*>(this), "Missing Output Path",
            "Please specify where to save the fine-tuned model.");
        return false;
    }

    // Check file existence
    if (!QFileInfo::exists(m_datasetPathEdit->text())) {
        QMessageBox::warning(const_cast<TrainingDialog*>(this), "Dataset Not Found",
            QString("Dataset file does not exist:\n%1").arg(m_datasetPathEdit->text()));
        return false;
    }

    if (!QFileInfo::exists(m_modelPathEdit->text())) {
        QMessageBox::warning(const_cast<TrainingDialog*>(this), "Model Not Found",
            QString("Model file does not exist:\n%1").arg(m_modelPathEdit->text()));
        return false;
    }

    // Validate hyperparameters
    if (m_epochsSpinBox->value() < 1) {
        QMessageBox::warning(const_cast<TrainingDialog*>(this), "Invalid Epochs",
            "Number of epochs must be at least 1.");
        return false;
    }

    if (m_learningRateSpinBox->value() <= 0.0) {
        QMessageBox::warning(const_cast<TrainingDialog*>(this), "Invalid Learning Rate",
            "Learning rate must be greater than 0.");
        return false;
    }

    if (m_batchSizeSpinBox->value() < 1) {
        QMessageBox::warning(const_cast<TrainingDialog*>(this), "Invalid Batch Size",
            "Batch size must be at least 1.");
        return false;
    }

    return true;
}

QString TrainingDialog::detectDatasetFormat(const QString& path) const
{
    if (path.endsWith(".csv", Qt::CaseInsensitive)) {
        return "CSV";
    } else if (path.endsWith(".jsonl", Qt::CaseInsensitive) || path.endsWith(".json", Qt::CaseInsensitive)) {
        return "JSON Lines";
    } else if (path.endsWith(".txt", Qt::CaseInsensitive)) {
        return "Plain Text";
    }
    return "Unknown";
}

QJsonObject TrainingDialog::getTrainingConfig() const
{
    QJsonObject config;

    // Paths
    config["datasetPath"] = m_datasetPathEdit->text();
    config["modelPath"] = m_modelPathEdit->text();
    config["outputPath"] = m_outputPathEdit->text();

    // Dataset format
    int formatIndex = m_datasetFormatCombo->currentData().toInt();
    if (formatIndex >= 0) {
        config["datasetFormat"] = formatIndex;
    } else {
        // Auto-detect
        QString format = detectDatasetFormat(m_datasetPathEdit->text());
        if (format == "Plain Text") config["datasetFormat"] = 0;
        else if (format == "JSON Lines") config["datasetFormat"] = 1;
        else if (format == "CSV") config["datasetFormat"] = 2;
        else config["datasetFormat"] = 0; // Default to plain text
    }

    // Hyperparameters
    config["epochs"] = m_epochsSpinBox->value();
    config["learningRate"] = m_learningRateSpinBox->value();
    config["batchSize"] = m_batchSizeSpinBox->value();
    config["sequenceLength"] = m_sequenceLengthSpinBox->value();
    config["gradientClip"] = m_gradientClipSpinBox->value();
    config["weightDecay"] = m_weightDecaySpinBox->value();
    config["warmupSteps"] = m_warmupStepsSpinBox->value();

    // Validation options
    config["validationSplit"] = m_validationSplitSpinBox->value();
    config["validateEveryEpoch"] = m_validateEveryEpochCheckBox->isChecked();

    return config;
}
