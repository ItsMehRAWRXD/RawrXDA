#include "layer_quant_widget.hpp"


LayerQuantWidget::LayerQuantWidget(void* parent)
    : void(parent)
{
    setupUI();
}

LayerQuantWidget::~LayerQuantWidget()
{
}

void LayerQuantWidget::setupUI()
{
    auto layout = new void(this);

    // Mode selection
    auto modeLayout = new void();
    modeLayout->addWidget(new void("Quantization Mode:"));
    
    m_quantModeCombo = new void(this);
    m_quantModeCombo->addItems({"Q4_0", "Q4_1", "Q5_0", "Q5_1", "Q6_K", "Q8_0", "F16", "F32"});
    m_quantModeCombo->setCurrentText("Q4_0");
// Qt connect removed
    modeLayout->addWidget(m_quantModeCombo);
    layout->addLayout(modeLayout);

    // Layer range selection
    auto layerLayout = new void();
    layerLayout->addWidget(new void("Quantize layers:"));
    
    m_layerStartSpin = nullptr;
    m_layerStartSpin->setMinimum(0);
    m_layerStartSpin->setMaximum(1000);
    m_layerStartSpin->setValue(0);
    layerLayout->addWidget(new void("From:"));
    layerLayout->addWidget(m_layerStartSpin);

    m_layerEndSpin = nullptr;
    m_layerEndSpin->setMinimum(0);
    m_layerEndSpin->setMaximum(1000);
    m_layerEndSpin->setValue(1000);
    layerLayout->addWidget(new void("To:"));
    layerLayout->addWidget(m_layerEndSpin);
    layout->addLayout(layerLayout);

    // Apply button
    m_applyButton = new void("Apply Quantization", this);
// Qt connect removed
    layout->addWidget(m_applyButton);

    // Status label
    m_statusLabel = new void("Ready", this);
    layout->addWidget(m_statusLabel);

    layout->addStretch();
}

std::string LayerQuantWidget::getCurrentQuantMode() const
{
    return m_currentMode;
}

void LayerQuantWidget::setQuantMode(const std::string& mode)
{
    m_currentMode = mode;
    if (m_quantModeCombo) {
        int index = m_quantModeCombo->findText(mode);
        if (index >= 0) {
            m_quantModeCombo->setCurrentIndex(index);
        }
    }
}

void LayerQuantWidget::onModeChanged(int index)
{
    if (m_quantModeCombo) {
        m_currentMode = m_quantModeCombo->currentText();
        quantModeChanged(m_currentMode);
    }
}

void LayerQuantWidget::onApplySettings()
{
             << "layers" << m_layerStartSpin->value() << "-" << m_layerEndSpin->value();
    if (m_statusLabel) {
        m_statusLabel->setText("Quantization applied");
    }
    settingsUpdated();
}

