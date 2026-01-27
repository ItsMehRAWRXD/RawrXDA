#include "InferenceSettingsDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QMessageBox>
#include <QFileDialog>

InferenceSettingsDialog::InferenceSettingsDialog(QWidget* parent)
    : QDialog(parent)
    , m_settings(InferenceSettingsManager::getInstance())
{
    setWindowTitle("Inference Settings");
    setMinimumSize(600, 500);
    
    setupUI();
    loadSettings();
    
    // Connect signals
    connect(m_presetCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &InferenceSettingsDialog::onPresetChanged);
    connect(m_temperatureSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &InferenceSettingsDialog::onTemperatureChanged);
    connect(m_topPSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &InferenceSettingsDialog::onTopPChanged);
    connect(m_topKSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &InferenceSettingsDialog::onTopKChanged);
    connect(m_maxTokensSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &InferenceSettingsDialog::onMaxTokensChanged);
    connect(m_repetitionPenaltySpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &InferenceSettingsDialog::onRepetitionPenaltyChanged);
    connect(m_useOllamaCheck, &QCheckBox::stateChanged,
            this, &InferenceSettingsDialog::onUseOllamaChanged);
    connect(m_ollamaModelEdit, &QLineEdit::textChanged,
            this, &InferenceSettingsDialog::onOllamaModelChanged);
    connect(m_recentModelsList, &QListWidget::itemClicked,
            this, &InferenceSettingsDialog::onRecentModelSelected);
    connect(m_clearRecentButton, &QPushButton::clicked,
            this, &InferenceSettingsDialog::onClearRecentModels);
    connect(m_buttonBox->button(QDialogButtonBox::Apply), &QPushButton::clicked,
            this, &InferenceSettingsDialog::onApplyClicked);
    connect(m_buttonBox->button(QDialogButtonBox::Ok), &QPushButton::clicked,
            this, &InferenceSettingsDialog::onOkClicked);
    connect(m_buttonBox->button(QDialogButtonBox::Cancel), &QPushButton::clicked,
            this, &InferenceSettingsDialog::onCancelClicked);
}

InferenceSettingsDialog::~InferenceSettingsDialog()
{
}

void InferenceSettingsDialog::setupUI()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    
    // Preset selection
    QGroupBox* presetGroup = new QGroupBox("Preset Configuration");
    QVBoxLayout* presetLayout = new QVBoxLayout(presetGroup);
    
    m_presetCombo = new QComboBox();
    m_presetCombo->addItem("Balanced", InferenceSettingsManager::Balanced);
    m_presetCombo->addItem("Performance", InferenceSettingsManager::Performance);
    m_presetCombo->addItem("Quality", InferenceSettingsManager::Quality);
    m_presetCombo->addItem("Custom", InferenceSettingsManager::Custom);
    
    m_presetIndicator = new QLabel("Current: Balanced");
    m_presetIndicator->setStyleSheet("QLabel { color: #4ec9b0; font-weight: bold; }");
    
    presetLayout->addWidget(new QLabel("Preset:"));
    presetLayout->addWidget(m_presetCombo);
    presetLayout->addWidget(m_presetIndicator);
    
    // Generation parameters
    QGroupBox* paramsGroup = new QGroupBox("Generation Parameters");
    QFormLayout* paramsLayout = new QFormLayout(paramsGroup);
    
    m_temperatureSpin = new QDoubleSpinBox();
    m_temperatureSpin->setRange(0.0, 2.0);
    m_temperatureSpin->setSingleStep(0.1);
    m_temperatureSpin->setDecimals(1);
    
    m_topPSpin = new QDoubleSpinBox();
    m_topPSpin->setRange(0.0, 1.0);
    m_topPSpin->setSingleStep(0.05);
    m_topPSpin->setDecimals(2);
    
    m_topKSpin = new QSpinBox();
    m_topKSpin->setRange(1, 200);
    
    m_maxTokensSpin = new QSpinBox();
    m_maxTokensSpin->setRange(1, 8192);
    
    m_repetitionPenaltySpin = new QDoubleSpinBox();
    m_repetitionPenaltySpin->setRange(1.0, 2.0);
    m_repetitionPenaltySpin->setSingleStep(0.05);
    m_repetitionPenaltySpin->setDecimals(2);
    
    paramsLayout->addRow("Temperature:", m_temperatureSpin);
    paramsLayout->addRow("Top P:", m_topPSpin);
    paramsLayout->addRow("Top K:", m_topKSpin);
    paramsLayout->addRow("Max Tokens:", m_maxTokensSpin);
    paramsLayout->addRow("Repetition Penalty:", m_repetitionPenaltySpin);
    
    // Ollama settings
    QGroupBox* ollamaGroup = new QGroupBox("Ollama Integration");
    QFormLayout* ollamaLayout = new QFormLayout(ollamaGroup);
    
    m_useOllamaCheck = new QCheckBox("Use Ollama for inference");
    m_ollamaModelEdit = new QLineEdit();
    
    ollamaLayout->addRow(m_useOllamaCheck);
    ollamaLayout->addRow("Model Tag:", m_ollamaModelEdit);
    
    // Recent models
    QGroupBox* recentGroup = new QGroupBox("Recent Models");
    QVBoxLayout* recentLayout = new QVBoxLayout(recentGroup);
    
    m_recentModelsList = new QListWidget();
    m_clearRecentButton = new QPushButton("Clear Recent Models");
    
    recentLayout->addWidget(new QLabel("Recently loaded models:"));
    recentLayout->addWidget(m_recentModelsList);
    recentLayout->addWidget(m_clearRecentButton);
    
    // Buttons
    m_buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Apply | QDialogButtonBox::Cancel);
    
    mainLayout->addWidget(presetGroup);
    mainLayout->addWidget(paramsGroup);
    mainLayout->addWidget(ollamaGroup);
    mainLayout->addWidget(recentGroup);
    mainLayout->addWidget(m_buttonBox);
}

void InferenceSettingsDialog::loadSettings()
{
    m_presetCombo->setCurrentIndex(m_settings.getCurrentPreset());
    m_temperatureSpin->setValue(m_settings.getTemperature());
    m_topPSpin->setValue(m_settings.getTopP());
    m_topKSpin->setValue(m_settings.getTopK());
    m_maxTokensSpin->setValue(m_settings.getMaxTokens());
    m_repetitionPenaltySpin->setValue(m_settings.getRepetitionPenalty());
    m_useOllamaCheck->setChecked(m_settings.getUseOllama());
    m_ollamaModelEdit->setText(m_settings.getOllamaModelTag());
    
    // Load recent models
    m_recentModelsList->clear();
    QStringList recentModels = m_settings.getRecentModels();
    for (const QString& model : recentModels) {
        m_recentModelsList->addItem(model);
    }
    
    updatePresetIndicator();
}

void InferenceSettingsDialog::onPresetChanged(int index)
{
    InferenceSettingsManager::Preset preset = static_cast<InferenceSettingsManager::Preset>(index);
    m_settings.applyPreset(preset);
    loadSettings(); // Reload to show preset values
}

void InferenceSettingsDialog::onTemperatureChanged(double value)
{
    m_settings.setTemperature(value);
    updateCustomSettings();
}

void InferenceSettingsDialog::onTopPChanged(double value)
{
    m_settings.setTopP(value);
    updateCustomSettings();
}

void InferenceSettingsDialog::onTopKChanged(int value)
{
    m_settings.setTopK(value);
    updateCustomSettings();
}

void InferenceSettingsDialog::onMaxTokensChanged(int value)
{
    m_settings.setMaxTokens(value);
    updateCustomSettings();
}

void InferenceSettingsDialog::onRepetitionPenaltyChanged(double value)
{
    m_settings.setRepetitionPenalty(value);
    updateCustomSettings();
}

void InferenceSettingsDialog::onUseOllamaChanged(int state)
{
    m_settings.setUseOllama(state == Qt::Checked);
}

void InferenceSettingsDialog::onOllamaModelChanged(const QString& text)
{
    m_settings.setOllamaModelTag(text);
}

void InferenceSettingsDialog::onRecentModelSelected(QListWidgetItem* item)
{
    if (item) {
        QString modelPath = item->text();
        m_settings.setCurrentModelPath(modelPath);
        
        // Ask if user wants to load this model
        QMessageBox::StandardButton reply = QMessageBox::question(this,
            "Load Model",
            QString("Load model %1?").arg(QFileInfo(modelPath).fileName()),
            QMessageBox::Yes | QMessageBox::No);
        
        if (reply == QMessageBox::Yes) {
            emit modelSelected(modelPath);
        }
    }
}

void InferenceSettingsDialog::onClearRecentModels()
{
    m_settings.clearRecentModels();
    m_recentModelsList->clear();
}

void InferenceSettingsDialog::onApplyClicked()
{
    m_settings.save();
    QMessageBox::information(this, "Settings Applied", "Inference settings have been saved.");
}

void InferenceSettingsDialog::onOkClicked()
{
    m_settings.save();
    accept();
}

void InferenceSettingsDialog::onCancelClicked()
{
    reject();
}

void InferenceSettingsDialog::updatePresetIndicator()
{
    QString presetName = m_settings.getPresetName(m_settings.getCurrentPreset());
    m_presetIndicator->setText(QString("Current: %1").arg(presetName));
}

void InferenceSettingsDialog::updateCustomSettings()
{
    // If any parameter is changed from preset values, switch to Custom
    if (m_settings.getCurrentPreset() != InferenceSettingsManager::Custom) {
        m_presetCombo->setCurrentIndex(InferenceSettingsManager::Custom);
        updatePresetIndicator();
    }
}