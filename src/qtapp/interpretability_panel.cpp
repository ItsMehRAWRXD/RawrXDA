#include "interpretability_panel.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTabWidget>
#include <QComboBox>
#include <QSlider>
#include <QLabel>
#include <QPainter>
#include <QtCharts/QChart>
#include <QtCharts/QChartView>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonArray>

InterpretabilityPanel::InterpretabilityPanel(QWidget* parent)
    : QWidget(parent),
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
    qDebug() << "[InterpretabilityPanel] Constructed (lazy-init)";
}

void InterpretabilityPanel::initialize() {
    if (m_tabWidget) return;  // Already initialized
    qDebug() << "[InterpretabilityPanel] Initializing interpretability panel";
    setupUI();
    setupConnections();
}

InterpretabilityPanel::~InterpretabilityPanel()
{
    qDebug() << "[InterpretabilityPanel] Destroying interpretability panel";
}

void InterpretabilityPanel::setVisualizationType(VisualizationType vizType)
{
    m_currentVisualizationType = vizType;
    qDebug() << "[InterpretabilityPanel] Visualization type set to" << static_cast<int>(vizType);
    updateDisplay();
}

InterpretabilityPanel::VisualizationType InterpretabilityPanel::getCurrentVisualizationType() const
{
    return m_currentVisualizationType;
}

void InterpretabilityPanel::updateAttentionHeatmap(const AttentionData& attentionData)
{
    qDebug() << "[InterpretabilityPanel] Updating attention heatmap for head" << attentionData.headIndex;
    m_attentionHeads[attentionData.headIndex] = attentionData;
    emit visualizationUpdated(static_cast<int>(VisualizationType::AttentionHeatmap));
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
    qDebug() << "[InterpretabilityPanel] Updating feature importance with" << importances.size() << "features";
    m_featureImportances = importances;
    emit visualizationUpdated(static_cast<int>(VisualizationType::FeatureImportance));
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
    qDebug() << "[InterpretabilityPanel] Updating gradient flow with" << gradientData.size() << "layers";
    m_gradientFlowData = gradientData;
    emit visualizationUpdated(static_cast<int>(VisualizationType::GradientFlow));
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
    qDebug() << "[InterpretabilityPanel] Updating activation stats for" << activationStats.size() << "layers";
    m_activationStats = activationStats;
    emit visualizationUpdated(static_cast<int>(VisualizationType::ActivationDistribution));
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
    qDebug() << "[InterpretabilityPanel] Updating GradCAM for layer" << layerIndex;
    m_gradcamData[layerIndex] = gradcamData;
    emit visualizationUpdated(static_cast<int>(VisualizationType::GradCAM));
}

void InterpretabilityPanel::updateLayerAttribution(const std::vector<LayerAttribution>& layerAttributions)
{
    qDebug() << "[InterpretabilityPanel] Updating layer attribution for" << layerAttributions.size() << "layers";
    m_layerAttributions = layerAttributions;
    emit visualizationUpdated(static_cast<int>(VisualizationType::LayerContribution));
}

std::vector<InterpretabilityPanel::LayerAttribution> InterpretabilityPanel::getCriticalLayers() const
{
    return m_layerAttributions;
}

void InterpretabilityPanel::updateIntegratedGradients(const QString& inputName, const std::vector<float>& attributions)
{
    qDebug() << "[InterpretabilityPanel] Updating integrated gradients for" << inputName;
    m_integratedGradients[inputName] = attributions;
    emit visualizationUpdated(static_cast<int>(VisualizationType::IntegratedGradients));
}

void InterpretabilityPanel::updateSaliencyMap(const std::vector<float>& saliencyData)
{
    qDebug() << "[InterpretabilityPanel] Updating saliency map";
    m_saliencyMap = saliencyData;
    emit visualizationUpdated(static_cast<int>(VisualizationType::SaliencyMap));
}

void InterpretabilityPanel::setSelectedLayer(int layerIndex)
{
    m_selectedLayerIndex = layerIndex;
    qDebug() << "[InterpretabilityPanel] Selected layer" << layerIndex;
    emit layerSelectionChanged(layerIndex);
    updateDisplay();
}

void InterpretabilityPanel::setSelectedAttentionHead(int headIndex)
{
    m_selectedAttentionHeadIndex = headIndex;
    qDebug() << "[InterpretabilityPanel] Selected attention head" << headIndex;
    emit attentionHeadSelectionChanged(headIndex);
    updateDisplay();
}

void InterpretabilityPanel::setAttentionFocusPosition(int position)
{
    m_attentionFocusPosition = position;
    qDebug() << "[InterpretabilityPanel] Set attention focus position" << position;
    updateDisplay();
}

bool InterpretabilityPanel::exportVisualization(const QString& filePath) const
{
    qDebug() << "[InterpretabilityPanel] Exporting visualization to" << filePath;

    if (!m_chartView || !m_chart) {
        qWarning() << "[InterpretabilityPanel] No chart to export";
        return false;
    }

    // Render the chart view to a pixmap
    QPixmap pixmap = m_chartView->grab();
    if (pixmap.isNull()) {
        qWarning() << "[InterpretabilityPanel] Failed to grab chart pixmap";
        return false;
    }

    // Determine format from extension
    QString ext = QFileInfo(filePath).suffix().toLower();
    if (ext == "svg") {
        // Export as SVG using QSvgGenerator if available, otherwise fallback to PNG
        // Simplified: save as PNG with .svg renamed (QSvgGenerator needs QtSvg module)
        return pixmap.save(filePath, "PNG");
    } else if (ext == "pdf") {
        QPrinter printer(QPrinter::HighResolution);
        printer.setOutputFormat(QPrinter::PdfFormat);
        printer.setOutputFileName(filePath);
        QPainter painter(&printer);
        m_chartView->render(&painter);
        painter.end();
        return true;
    } else {
        // Default: PNG or specified format
        const char* format = (ext == "jpg" || ext == "jpeg") ? "JPG" : "PNG";
        return pixmap.save(filePath, format);
    }
}

QJsonObject InterpretabilityPanel::exportInterpretabilityData() const
{
    QJsonObject data;
    data["currentVisualizationType"] = static_cast<int>(m_currentVisualizationType);
    data["selectedLayerIndex"] = m_selectedLayerIndex;
    data["selectedAttentionHeadIndex"] = m_selectedAttentionHeadIndex;
    data["gradientFlowCount"] = static_cast<int>(m_gradientFlowData.size());
    data["activationStatsCount"] = static_cast<int>(m_activationStats.size());
    return data;
}

bool InterpretabilityPanel::loadInterpretabilityData(const QJsonObject& data)
{
    qDebug() << "[InterpretabilityPanel] Loading interpretability data from JSON";
    m_currentVisualizationType = static_cast<VisualizationType>(data["currentVisualizationType"].toInt());
    m_selectedLayerIndex = data["selectedLayerIndex"].toInt();
    return true;
}

void InterpretabilityPanel::clearVisualizations()
{
    qDebug() << "[InterpretabilityPanel] Clearing all visualizations";
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
    m_layerSlider = new QSlider(Qt::Horizontal);
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
        m_attentionHeadCombo->addItem(QString("Head %1").arg(i), i);
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
    connect(m_vizTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &InterpretabilityPanel::onVisualizationTypeChanged);
    
    connect(m_layerSlider, &QSlider::valueChanged,
            this, &InterpretabilityPanel::onLayerSliderChanged);
    
    connect(m_attentionHeadCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &InterpretabilityPanel::onAttentionHeadChanged);
}

void InterpretabilityPanel::createCharts()
{
    if (!m_chart) return;

    // Clear existing series and axes
    m_chart->removeAllSeries();
    const auto axes = m_chart->axes();
    for (auto* axis : axes) {
        m_chart->removeAxis(axis);
    }

    switch (m_currentVisualizationType) {
    case VisualizationType::AttentionHeatmap: {
        // Display attention weights as a bar chart for the selected head
        if (m_selectedAttentionHeadIndex < static_cast<int>(m_attentionHeads.size())) {
            auto* series = new QBarSeries();
            auto* set = new QBarSet(QString("Head %1").arg(m_selectedAttentionHeadIndex));
            const auto& weights = m_attentionHeads[m_selectedAttentionHeadIndex].weights;
            for (float w : weights) {
                *set << static_cast<double>(w);
            }
            series->append(set);
            m_chart->addSeries(series);
            m_chart->setTitle(QString("Attention Weights - Head %1, Layer %2")
                              .arg(m_selectedAttentionHeadIndex).arg(m_selectedLayerIndex));
            m_chart->createDefaultAxes();
        }
        break;
    }
    case VisualizationType::FeatureImportance: {
        auto* series = new QBarSeries();
        auto* set = new QBarSet("Importance");
        QStringList categories;
        for (const auto& feat : m_featureImportances) {
            *set << static_cast<double>(feat.importance);
            categories << QString::fromStdString(feat.name);
        }
        series->append(set);
        m_chart->addSeries(series);
        m_chart->setTitle("Feature Importance");
        auto* axisX = new QBarCategoryAxis();
        axisX->append(categories);
        m_chart->addAxis(axisX, Qt::AlignBottom);
        series->attachAxis(axisX);
        auto* axisY = new QValueAxis();
        axisY->setRange(0, 1.0);
        m_chart->addAxis(axisY, Qt::AlignLeft);
        series->attachAxis(axisY);
        break;
    }
    case VisualizationType::GradientFlow: {
        auto* series = new QLineSeries();
        series->setName("Gradient Magnitude");
        for (int i = 0; i < static_cast<int>(m_gradientFlowData.size()); ++i) {
            series->append(i, static_cast<double>(m_gradientFlowData[i].magnitude));
        }
        m_chart->addSeries(series);
        m_chart->setTitle("Gradient Flow Across Layers");
        m_chart->createDefaultAxes();
        break;
    }
    case VisualizationType::ActivationDistribution: {
        // Histogram of activation values for selected layer
        if (m_selectedLayerIndex < static_cast<int>(m_activationStats.size())) {
            auto* series = new QLineSeries();
            series->setName(QString("Layer %1 Activations").arg(m_selectedLayerIndex));
            const auto& stats = m_activationStats[m_selectedLayerIndex];
            // Plot mean +/- std as area
            series->append(0, stats.mean - stats.stddev);
            series->append(1, stats.mean);
            series->append(2, stats.mean + stats.stddev);
            m_chart->addSeries(series);
            m_chart->setTitle(QString("Activation Distribution - Layer %1 (mean=%.3f, std=%.3f)")
                              .arg(m_selectedLayerIndex).arg(stats.mean).arg(stats.stddev));
            m_chart->createDefaultAxes();
        }
        break;
    }
    case VisualizationType::GradCAM: {
        auto* series = new QLineSeries();
        series->setName("GradCAM");
        for (int i = 0; i < static_cast<int>(m_gradcamData.size()); ++i) {
            series->append(i, static_cast<double>(m_gradcamData[i]));
        }
        m_chart->addSeries(series);
        m_chart->setTitle("GradCAM Activation Map");
        m_chart->createDefaultAxes();
        break;
    }
    case VisualizationType::LayerContribution: {
        auto* series = new QBarSeries();
        auto* set = new QBarSet("Contribution");
        for (const auto& attr : m_layerAttributions) {
            *set << static_cast<double>(attr);
        }
        series->append(set);
        m_chart->addSeries(series);
        m_chart->setTitle("Layer Contribution Scores");
        m_chart->createDefaultAxes();
        break;
    }
    case VisualizationType::IntegratedGradients: {
        auto* series = new QLineSeries();
        series->setName("Integrated Gradients");
        for (int i = 0; i < static_cast<int>(m_integratedGradients.size()); ++i) {
            series->append(i, static_cast<double>(m_integratedGradients[i]));
        }
        m_chart->addSeries(series);
        m_chart->setTitle("Integrated Gradients Attribution");
        m_chart->createDefaultAxes();
        break;
    }
    case VisualizationType::SaliencyMap: {
        auto* series = new QLineSeries();
        series->setName("Saliency");
        for (int i = 0; i < static_cast<int>(m_saliencyMap.size()); ++i) {
            series->append(i, static_cast<double>(m_saliencyMap[i]));
        }
        m_chart->addSeries(series);
        m_chart->setTitle("Saliency Map");
        m_chart->createDefaultAxes();
        break;
    }
    default: {
        m_chart->setTitle("Select a visualization type");
        break;
    }
    }
}

void InterpretabilityPanel::updateDisplay()
{
    createCharts();
    m_statsLabel->setText(QString("Layer: %1 | Head: %2 | Type: %3")
                              .arg(m_selectedLayerIndex)
                              .arg(m_selectedAttentionHeadIndex)
                              .arg(static_cast<int>(m_currentVisualizationType)));
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
