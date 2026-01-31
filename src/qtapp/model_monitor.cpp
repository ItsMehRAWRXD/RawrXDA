#include "model_monitor.hpp"
#include "inference_engine.hpp"


#include <cmath>

ModelMonitor::ModelMonitor(InferenceEngine* engine, void* parent)
    : void(parent), m_engine(engine)
{
    // Lightweight constructor - defers Qt widget creation to initialize()
}

void ModelMonitor::initialize() {
    if (m_modelLabel) return;  // Already initialized
    
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    
    // Model info group
    QGroupBox* modelGroup = new QGroupBox(tr("Model Information"), this);
    QVBoxLayout* modelLayout = new QVBoxLayout(modelGroup);
    m_modelLabel = new QLabel(tr("No model loaded"), modelGroup);
    m_modelLabel->setStyleSheet("QLabel { color: #e0e0e0; }");
    modelLayout->addWidget(m_modelLabel);
    mainLayout->addWidget(modelGroup);
    
    // Performance metrics group
    QGroupBox* perfGroup = new QGroupBox(tr("Performance Metrics"), this);
    QVBoxLayout* perfLayout = new QVBoxLayout(perfGroup);
    
    m_memLabel = new QLabel(tr("Memory: --"), perfGroup);
    m_memLabel->setStyleSheet("QLabel { color: #e0e0e0; font-family: 'Consolas', monospace; }");
    perfLayout->addWidget(m_memLabel);
    
    m_tokensLabel = new QLabel(tr("Tokens/sec: --"), perfGroup);
    m_tokensLabel->setStyleSheet("QLabel { color: #0dff00; font-family: 'Consolas', monospace; font-weight: bold; }");
    perfLayout->addWidget(m_tokensLabel);
    
    m_tempLabel = new QLabel(tr("Temperature: --"), perfGroup);
    m_tempLabel->setStyleSheet("QLabel { color: #ff9900; font-family: 'Consolas', monospace; }");
    perfLayout->addWidget(m_tempLabel);
    
    mainLayout->addWidget(perfGroup);
    mainLayout->addStretch();
    
    // Setup refresh timer
    m_timer = new void*(this);
// Qt connect removed
    m_timer->start(1000);  // Update every second
    
    // Initial refresh
    refresh();
}

void ModelMonitor::refresh()
{
    // Get model info
    if (m_engine && m_engine->isModelLoaded()) {
        std::string modelPath = m_engine->modelPath();
        std::filesystem::path info(modelPath);
        m_modelLabel->setText(info.fileName());
        
        // Get real performance metrics from the engine
        qint64 memMB = m_engine->memoryUsageMB();
        double tps   = m_engine->tokensPerSecond();
        double temp  = m_engine->temperature();
        
        m_memLabel->setText(std::string("Memory: %1 MB"));
        m_tokensLabel->setText(std::string("Tokens/sec: %1"));
        m_tempLabel->setText(std::string("Temperature: %1"));
    } else {
        m_modelLabel->setText(tr("No model loaded"));
        m_memLabel->setText(tr("Memory: --"));
        m_tokensLabel->setText(tr("Tokens/sec: --"));
        m_tempLabel->setText(tr("Temperature: --"));
    }
}

