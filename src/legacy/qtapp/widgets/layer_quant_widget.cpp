#include "layer_quant_widget.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QComboBox>
#include <QSpinBox>
#include <QPushButton>
#include <QDebug>

LayerQuantWidget::LayerQuantWidget(QWidget* parent)
    : QWidget(parent)
{
    setupUI();
}

LayerQuantWidget::~LayerQuantWidget()
{
}

void LayerQuantWidget::setupUI()
{
    auto layout = new QVBoxLayout(this);

    // Mode selection
    auto modeLayout = new QHBoxLayout();
    modeLayout->addWidget(new QLabel("Quantization Mode:"));
    
    m_quantModeCombo = new QComboBox(this);
    m_quantModeCombo->addItems({"Q4_0", "Q4_1", "Q5_0", "Q5_1", "Q6_K", "Q8_0", "F16", "F32"});
    m_quantModeCombo->setCurrentText("Q4_0");
    connect(m_quantModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &LayerQuantWidget::onModeChanged);
    modeLayout->addWidget(m_quantModeCombo);
    layout->addLayout(modeLayout);

    // Layer range selection
    auto layerLayout = new QHBoxLayout();
    layerLayout->addWidget(new QLabel("Quantize layers:"));
    
    m_layerStartSpin = new QSpinBox(this);
    m_layerStartSpin->setMinimum(0);
    m_layerStartSpin->setMaximum(1000);
    m_layerStartSpin->setValue(0);
    layerLayout->addWidget(new QLabel("From:"));
    layerLayout->addWidget(m_layerStartSpin);

    m_layerEndSpin = new QSpinBox(this);
    m_layerEndSpin->setMinimum(0);
    m_layerEndSpin->setMaximum(1000);
    m_layerEndSpin->setValue(1000);
    layerLayout->addWidget(new QLabel("To:"));
    layerLayout->addWidget(m_layerEndSpin);
    layout->addLayout(layerLayout);

    // Apply button
    m_applyButton = new QPushButton("Apply Quantization", this);
    connect(m_applyButton, &QPushButton::clicked, this, &LayerQuantWidget::onApplySettings);
    layout->addWidget(m_applyButton);

    // Status label
    m_statusLabel = new QLabel("Ready", this);
    layout->addWidget(m_statusLabel);

    layout->addStretch();
}

QString LayerQuantWidget::getCurrentQuantMode() const
{
    return m_currentMode;
}

void LayerQuantWidget::setQuantMode(const QString& mode)
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
        emit quantModeChanged(m_currentMode);
        qDebug() << "Quantization mode changed to:" << m_currentMode;
    }
}

void LayerQuantWidget::onApplySettings()
{
    qDebug() << "Applying quantization:" << m_currentMode
             << "layers" << m_layerStartSpin->value() << "-" << m_layerEndSpin->value();
    if (m_statusLabel) {
        m_statusLabel->setText("Quantization applied");
    }
    emit settingsUpdated();
}
