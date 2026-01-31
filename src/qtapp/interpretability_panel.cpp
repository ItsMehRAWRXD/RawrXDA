#include "interpretability_panel.h"


InterpretabilityPanel::InterpretabilityPanel(void* parent)
    : void(parent),
      m_currentVisualizationType(VisualizationType::AttentionHeatmap),
      m_selectedLayerIndex(0),
      m_selectedAttentionHeadIndex(0),
      m_attentionFocusPosition(0),
      m_tabWidget(nullptr),
      m_vizTypeCombo(nullptr),
      m_layerSlider(nullptr),
      m_attentionHeadCombo(nullptr),
      m_chartView(nullptr),
      m_chart(nullptr),
      m_statsLabel(nullptr),
      m_problemsLabel(nullptr)
{
    // Lightweight constructor - defers Qt widget creation to initialize()
}

void InterpretabilityPanel::initialize() {
    if (m_tabWidget) return;  // Already initialized
    setupUI();
    setupConnections();
}

InterpretabilityPanel::~InterpretabilityPanel()
{
}

void InterpretabilityPanel::setVisualizationType(VisualizationType vizType)
{
    m_currentVisualizationType = vizType;
    updateDisplay();
}

InterpretabilityPanel::VisualizationType InterpretabilityPanel::getCurrentVisualizationType() const
{
    return m_currentVisualizationType;
}

void InterpretabilityPanel::updateAttentionHeatmap(const AttentionData& attentionData)
{
    m_attentionHeads[attentionData.headIndex] = attentionData;
    visualizationUpdated(static_cast<int>(VisualizationType::AttentionHeatmap));
}

std::vector<InterpretabilityPanel::AttentionData> InterpretabilityPanel::getAttentionHeads() const
{
    std::vector<AttentionData> result;
    for (const auto& pair : m_attentionHeads) {
        result.push_back(pair.second);
    }
    return result;
}

void InterpretabilityPanel::updateFeatureImportance(const std::vector<FeatureImportance>& importances)
{
    m_featureImportances = importances;
    visualizationUpdated(static_cast<int>(VisualizationType::FeatureImportance));
}

std::vector<InterpretabilityPanel::FeatureImportance> InterpretabilityPanel::getTopFeatures(int k) const
{
    std::vector<FeatureImportance> result;
    for (size_t i = 0; i < m_featureImportances.size() && i < static_cast<size_t>(k); ++i) {
        result.push_back(m_featureImportances[i]);
    }
    return result;
}

void InterpretabilityPanel::updateGradientFlow(const std::vector<GradientFlowData>& gradientData)
{
    m_gradientFlowData = gradientData;
    visualizationUpdated(static_cast<int>(VisualizationType::GradientFlow));
}

std::vector<int> InterpretabilityPanel::detectGradientProblems() const
{
    std::vector<int> problems;
    for (size_t i = 0; i < m_gradientFlowData.size(); ++i) {
        if (m_gradientFlowData[i].dead_neuron_ratio > 0.9f) {
            problems.push_back(static_cast<int>(i));
        }
    }
    return problems;
}

void InterpretabilityPanel::updateActivationStats(const std::vector<ActivationStats>& activationStats)
{
    m_activationStats = activationStats;
    visualizationUpdated(static_cast<int>(VisualizationType::ActivationDistribution));
}

std::map<int, float> InterpretabilityPanel::getSparsityReport() const
{
    std::map<int, float> result;
    for (size_t i = 0; i < m_activationStats.size(); ++i) {
        result[static_cast<int>(i)] = m_activationStats[i].sparsity;
    }
    return result;
}

void InterpretabilityPanel::updateGradCAM(int layerIndex, const std::vector<float>& gradcamData)
{
    m_gradcamData[layerIndex] = gradcamData;
    visualizationUpdated(static_cast<int>(VisualizationType::GradCAM));
}

void InterpretabilityPanel::updateLayerAttribution(const std::vector<LayerAttribution>& layerAttributions)
{
    m_layerAttributions = layerAttributions;
    visualizationUpdated(static_cast<int>(VisualizationType::LayerContribution));
}

std::vector<InterpretabilityPanel::LayerAttribution> InterpretabilityPanel::getCriticalLayers() const
{
    return m_layerAttributions;
}

void InterpretabilityPanel::updateIntegratedGradients(const std::string& inputName, const std::vector<float>& attributions)
{
    m_integratedGradients[inputName] = attributions;
    visualizationUpdated(static_cast<int>(VisualizationType::IntegratedGradients));
}

void InterpretabilityPanel::updateSaliencyMap(const std::vector<float>& saliencyData)
{
    m_saliencyMap = saliencyData;
    visualizationUpdated(static_cast<int>(VisualizationType::SaliencyMap));
}

void InterpretabilityPanel::setSelectedLayer(int layerIndex)
{
    m_selectedLayerIndex = layerIndex;
    layerSelectionChanged(layerIndex);
    updateDisplay();
}

void InterpretabilityPanel::setSelectedAttentionHead(int headIndex)
{
    m_selectedAttentionHeadIndex = headIndex;
    attentionHeadSelectionChanged(headIndex);
    updateDisplay();
}

void InterpretabilityPanel::setAttentionFocusPosition(int position)
{
    m_attentionFocusPosition = position;
    updateDisplay();
}

bool InterpretabilityPanel::exportVisualization(const std::string& filePath) const
{
    // Placeholder: in production, export chart to image
    return true;
}

void* InterpretabilityPanel::exportInterpretabilityData() const
{
    void* data;
    data["currentVisualizationType"] = static_cast<int>(m_currentVisualizationType);
    data["selectedLayerIndex"] = m_selectedLayerIndex;
    data["selectedAttentionHeadIndex"] = m_selectedAttentionHeadIndex;
    data["gradientFlowCount"] = static_cast<int>(m_gradientFlowData.size());
    data["activationStatsCount"] = static_cast<int>(m_activationStats.size());
    return data;
}

bool InterpretabilityPanel::loadInterpretabilityData(const void*& data)
{
    m_currentVisualizationType = static_cast<VisualizationType>(data["currentVisualizationType"].toInt());
    m_selectedLayerIndex = data["selectedLayerIndex"].toInt();
    return true;
}

void InterpretabilityPanel::clearVisualizations()
{
    m_attentionHeads.clear();
    m_featureImportances.clear();
    m_gradientFlowData.clear();
    m_activationStats.clear();
    m_gradcamData.clear();
    m_layerAttributions.clear();
    m_integratedGradients.clear();
    m_saliencyMap.clear();
}

void InterpretabilityPanel::setupUI()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    
    // Visualization type selector
    QHBoxLayout* typeLayout = new QHBoxLayout();
    QLabel* typeLabel = new QLabel("Visualization Type:");
    m_vizTypeCombo = new QComboBox();
    m_vizTypeCombo->addItem("Attention Heatmap", static_cast<int>(VisualizationType::AttentionHeatmap));
    m_vizTypeCombo->addItem("Feature Importance", static_cast<int>(VisualizationType::FeatureImportance));
    m_vizTypeCombo->addItem("Gradient Flow", static_cast<int>(VisualizationType::GradientFlow));
    m_vizTypeCombo->addItem("Activation Distribution", static_cast<int>(VisualizationType::ActivationDistribution));
    m_vizTypeCombo->addItem("Attention Head Comparison", static_cast<int>(VisualizationType::AttentionHeadComparison));
    m_vizTypeCombo->addItem("GradCAM", static_cast<int>(VisualizationType::GradCAM));
    m_vizTypeCombo->addItem("Layer Contribution", static_cast<int>(VisualizationType::LayerContribution));
    m_vizTypeCombo->addItem("Embedding Space", static_cast<int>(VisualizationType::EmbeddingSpace));
    m_vizTypeCombo->addItem("Integrated Gradients", static_cast<int>(VisualizationType::IntegratedGradients));
    m_vizTypeCombo->addItem("Saliency Map", static_cast<int>(VisualizationType::SaliencyMap));
    
    typeLayout->addWidget(typeLabel);
    typeLayout->addWidget(m_vizTypeCombo);
    typeLayout->addStretch();
    mainLayout->addLayout(typeLayout);
    
    // Layer selector
    QHBoxLayout* layerLayout = new QHBoxLayout();
    QLabel* layerLabel = new QLabel("Layer:");
    m_layerSlider = new QSlider(//Horizontal);
    m_layerSlider->setRange(0, 50);
    m_layerSlider->setValue(0);
    
    layerLayout->addWidget(layerLabel);
    layerLayout->addWidget(m_layerSlider);
    mainLayout->addLayout(layerLayout);
    
    // Attention head selector
    QHBoxLayout* attentionLayout = new QHBoxLayout();
    QLabel* attentionLabel = new QLabel("Attention Head:");
    m_attentionHeadCombo = new QComboBox();
    for (int i = 0; i < 12; ++i) {
        m_attentionHeadCombo->addItem(std::string("Head %1"), i);
    }
    
    attentionLayout->addWidget(attentionLabel);
    attentionLayout->addWidget(m_attentionHeadCombo);
    attentionLayout->addStretch();
    mainLayout->addLayout(attentionLayout);
    
    // Tab widget for different visualizations
    m_tabWidget = new QTabWidget();
    m_chart = new QChart();
    m_chartView = new QChartView(m_chart);
    m_chartView->setRenderHint(QPainter::Antialiasing);
    m_tabWidget->addTab(m_chartView, "Chart");
    
    mainLayout->addWidget(m_tabWidget);
    
    // Stats label
    m_statsLabel = new QLabel("Ready");
    mainLayout->addWidget(m_statsLabel);
    
    // Problems label
    m_problemsLabel = new QLabel("No issues detected");
    mainLayout->addWidget(m_problemsLabel);
    
    setLayout(mainLayout);
}

void InterpretabilityPanel::setupConnections()
{
// Qt connect removed
// Qt connect removed
// Qt connect removed
}

void InterpretabilityPanel::createCharts()
{
    // Placeholder: create charts based on current visualization type
}

void InterpretabilityPanel::updateDisplay()
{
    createCharts();
    m_statsLabel->setText(std::string("Layer: %1 | Head: %2 | Type: %3")


                              ));
}

void InterpretabilityPanel::onLayerSliderChanged(int value)
{
    setSelectedLayer(value);
}

void InterpretabilityPanel::onAttentionHeadChanged(int index)
{
    setSelectedAttentionHead(index);
}

void InterpretabilityPanel::onVisualizationTypeChanged(int index)
{
    VisualizationType vizType = static_cast<VisualizationType>(m_vizTypeCombo->itemData(index).toInt());
    setVisualizationType(vizType);
}

void InterpretabilityPanel::onRefreshDisplay()
{
    updateDisplay();
}

