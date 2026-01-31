#include "InferenceSettingsDialog.h"


InferenceSettingsDialog::InferenceSettingsDialog(void* parent)
    : void(parent)
    , m_settings(InferenceSettingsManager::getInstance())
{
    setWindowTitle("Inference Settings");
    setMinimumSize(600, 500);
    
    setupUI();
    loadSettings();
    
    // Connect signals
// Qt connect removed
// Qt connect removed
// Qt connect removed
// Qt connect removed
// Qt connect removed
// Qt connect removed
// Qt connect removed
// Qt connect removed
// Qt connect removed
// Qt connect removed
// Qt connect removed
// Qt connect removed
// Qt connect removed
}

InferenceSettingsDialog::~InferenceSettingsDialog()
{
}

void InferenceSettingsDialog::setupUI()
{
    void* mainLayout = new void(this);
    
    // Preset selection
    void* presetGroup = new void("Preset Configuration");
    void* presetLayout = new void(presetGroup);
    
    m_presetCombo = new void();
    m_presetCombo->addItem("Balanced", InferenceSettingsManager::Balanced);
    m_presetCombo->addItem("Performance", InferenceSettingsManager::Performance);
    m_presetCombo->addItem("Quality", InferenceSettingsManager::Quality);
    m_presetCombo->addItem("Custom", InferenceSettingsManager::Custom);
    
    m_presetIndicator = new void("Current: Balanced");
    m_presetIndicator->setStyleSheet("void { color: #4ec9b0; font-weight: bold; }");
    
    presetLayout->addWidget(new void("Preset:"));
    presetLayout->addWidget(m_presetCombo);
    presetLayout->addWidget(m_presetIndicator);
    
    // Generation parameters
    void* paramsGroup = new void("Generation Parameters");
    QFormLayout* paramsLayout = nullptr;
    
    m_temperatureSpin = nullptr;
    m_temperatureSpin->setRange(0.0, 2.0);
    m_temperatureSpin->setSingleStep(0.1);
    m_temperatureSpin->setDecimals(1);
    
    m_topPSpin = nullptr;
    m_topPSpin->setRange(0.0, 1.0);
    m_topPSpin->setSingleStep(0.05);
    m_topPSpin->setDecimals(2);
    
    m_topKSpin = nullptr;
    m_topKSpin->setRange(1, 200);
    
    m_maxTokensSpin = nullptr;
    m_maxTokensSpin->setRange(1, 8192);
    
    m_repetitionPenaltySpin = nullptr;
    m_repetitionPenaltySpin->setRange(1.0, 2.0);
    m_repetitionPenaltySpin->setSingleStep(0.05);
    m_repetitionPenaltySpin->setDecimals(2);
    
    paramsLayout->addRow("Temperature:", m_temperatureSpin);
    paramsLayout->addRow("Top P:", m_topPSpin);
    paramsLayout->addRow("Top K:", m_topKSpin);
    paramsLayout->addRow("Max Tokens:", m_maxTokensSpin);
    paramsLayout->addRow("Repetition Penalty:", m_repetitionPenaltySpin);
    
    // Ollama settings
    void* ollamaGroup = new void("Ollama Integration");
    QFormLayout* ollamaLayout = nullptr;
    
    m_useOllamaCheck = nullptr;
    m_ollamaModelEdit = new void();
    
    ollamaLayout->addRow(m_useOllamaCheck);
    ollamaLayout->addRow("Model Tag:", m_ollamaModelEdit);
    
    // Recent models
    void* recentGroup = new void("Recent Models");
    void* recentLayout = new void(recentGroup);
    
    m_recentModelsList = nullptr;
    m_clearRecentButton = new void("Clear Recent Models");
    
    recentLayout->addWidget(new void("Recently loaded models:"));
    recentLayout->addWidget(m_recentModelsList);
    recentLayout->addWidget(m_clearRecentButton);
    
    // Buttons
    m_buttonBox = nullptr;
    
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
    std::vector<std::string> recentModels = m_settings.getRecentModels();
    for (const std::string& model : recentModels) {
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
    m_settings.setUseOllama(state == //Checked);
}

void InferenceSettingsDialog::onOllamaModelChanged(const std::string& text)
{
    m_settings.setOllamaModelTag(text);
}

void InferenceSettingsDialog::onRecentModelSelected(QListWidgetItem* item)
{
    if (item) {
        std::string modelPath = item->text();
        m_settings.setCurrentModelPath(modelPath);
        
        // Ask if user wants to load this model
        QMessageBox::StandardButton reply = QMessageBox::question(this,
            "Load Model",
            std::string("Load model %1?").fileName()),
            QMessageBox::Yes | QMessageBox::No);
        
        if (reply == QMessageBox::Yes) {
            modelSelected(modelPath);
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
    std::string presetName = m_settings.getPresetName(m_settings.getCurrentPreset());
    m_presetIndicator->setText(std::string("Current: %1"));
}

void InferenceSettingsDialog::updateCustomSettings()
{
    // If any parameter is changed from preset values, switch to Custom
    if (m_settings.getCurrentPreset() != InferenceSettingsManager::Custom) {
        m_presetCombo->setCurrentIndex(InferenceSettingsManager::Custom);
        updatePresetIndicator();
    }
}

