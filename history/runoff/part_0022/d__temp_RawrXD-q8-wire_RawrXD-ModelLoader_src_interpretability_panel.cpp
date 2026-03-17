#include "interpretability_panel.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTabWidget>
#include <QLabel>
#include <QSlider>
#include <QComboBox>
#include <QGroupBox>
#include <QTextEdit>
#include <QtCharts/QChart>
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QBarSeries>
#include <QtCharts/QBarSet>
#include <QtCharts/QBarCategoryAxis>
#include <QtCharts/QValueAxis>
#include <QDebug>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QDateTime>
#include <algorithm>
#include <numeric>
#include <cmath>

InterpretabilityPanel::InterpretabilityPanel(QWidget* parent)
    : QWidget(parent)
    , m_currentVisualizationType(VisualizationType::FeatureImportance)
    , m_selectedLayerIndex(0)
    , m_selectedAttentionHeadIndex(0)
    , m_attentionFocusPosition(0)
    , m_chart(nullptr)
{
    setWindowTitle("Interpretability Panel - Model Analysis");
    setMinimumSize(1200, 800);

    setupUI();
    setupConnections();
}

InterpretabilityPanel::~InterpretabilityPanel()
{
}

void InterpretabilityPanel::setupUI()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    // ===== Control Panel =====
    QGroupBox* controlGroup = new QGroupBox("Visualization Controls", this);
    QHBoxLayout* controlLayout = new QHBoxLayout(controlGroup);

    m_vizTypeCombo = new QComboBox(this);
    m_vizTypeCombo->addItem("Feature Importance");
    m_vizTypeCombo->addItem("Attention Heatmap");
    m_vizTypeCombo->addItem("Gradient Flow");
    m_vizTypeCombo->addItem("Activation Distribution");
    m_vizTypeCombo->addItem("Layer Contribution");
    m_vizTypeCombo->addItem("GradCAM");
    controlLayout->addWidget(new QLabel("Visualization:"));
    controlLayout->addWidget(m_vizTypeCombo);

    m_layerSlider = new QSlider(Qt::Horizontal, this);
    m_layerSlider->setMinimum(0);
    m_layerSlider->setMaximum(11);
    m_layerSlider->setValue(0);
    controlLayout->addWidget(new QLabel("Layer:"));
    controlLayout->addWidget(m_layerSlider);

    m_attentionHeadCombo = new QComboBox(this);
    for (int i = 0; i < 8; ++i) {
        m_attentionHeadCombo->addItem(QString("Head %1").arg(i));
    }
    controlLayout->addWidget(new QLabel("Attention Head:"));
    controlLayout->addWidget(m_attentionHeadCombo);

    mainLayout->addWidget(controlGroup);

    // ===== Main Visualization Area =====
    m_chartView = new QChartView(this);
    m_chart = new QChart();
    m_chart->setTitle("Model Interpretability Analysis");
    m_chart->setAnimationOptions(QChart::SeriesAnimations);
    m_chartView->setChart(m_chart);
    m_chartView->setRenderHint(QPainter::Antialiasing);
    mainLayout->addWidget(m_chartView, 1);

    // ===== Statistics & Problems Panel =====
    QGroupBox* statsGroup = new QGroupBox("Analysis Results", this);
    QVBoxLayout* statsLayout = new QVBoxLayout(statsGroup);

    m_statsLabel = new QLabel("No data loaded", this);
    m_statsLabel->setWordWrap(true);
    statsLayout->addWidget(m_statsLabel);

    m_problemsLabel = new QLabel("", this);
    m_problemsLabel->setStyleSheet("QLabel { color: red; font-weight: bold; }");
    m_problemsLabel->setWordWrap(true);
    statsLayout->addWidget(m_problemsLabel);

    mainLayout->addWidget(statsGroup, 0);
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

    if (m_currentVisualizationType == VisualizationType::AttentionHeatmap) {
        updateDisplay();
    }
}

std::vector<InterpretabilityPanel::AttentionData> InterpretabilityPanel::getAttentionHeads() const
{
    std::vector<AttentionData> result;
    for (const auto& [headIdx, data] : m_attentionHeads) {
        result.push_back(data);
    }
    return result;
}

void InterpretabilityPanel::updateFeatureImportance(const std::vector<FeatureImportance>& importances)
{
    m_featureImportances = importances;

    // Sort by importance
    std::sort(m_featureImportances.begin(), m_featureImportances.end(),
             [](const FeatureImportance& a, const FeatureImportance& b) {
                 return a.importance > b.importance;
             });

    // Update ranks
    for (size_t i = 0; i < m_featureImportances.size(); ++i) {
        m_featureImportances[i].rank = i + 1;
    }

    if (m_currentVisualizationType == VisualizationType::FeatureImportance) {
        updateDisplay();
    }

    emit visualizationUpdated(static_cast<int>(VisualizationType::FeatureImportance));
}

std::vector<InterpretabilityPanel::FeatureImportance> InterpretabilityPanel::getTopFeatures(int k) const
{
    std::vector<FeatureImportance> result;
    for (int i = 0; i < k && i < static_cast<int>(m_featureImportances.size()); ++i) {
        result.push_back(m_featureImportances[i]);
    }
    return result;
}

void InterpretabilityPanel::updateGradientFlow(const std::vector<GradientFlowData>& gradientData)
{
    m_gradientFlowData = gradientData;

    auto problems = detectGradientProblems();
    if (!problems.empty()) {
        QString problemMsg = "Gradient issues detected in layers: ";
        for (int layerIdx : problems) {
            problemMsg += QString::number(layerIdx) + " ";
        }
        emit gradientProblemDetected(problemMsg);
        m_problemsLabel->setText(problemMsg);
    }

    if (m_currentVisualizationType == VisualizationType::GradientFlow) {
        updateDisplay();
    }

    emit visualizationUpdated(static_cast<int>(VisualizationType::GradientFlow));
}

std::vector<int> InterpretabilityPanel::detectGradientProblems() const
{
    std::vector<int> problems;

    for (const auto& gfd : m_gradientFlowData) {
        // Vanishing gradient: mean gradient very small
        if (gfd.meanGradient < 1e-6f && gfd.meanGradient > 0) {
            problems.push_back(gfd.layerIndex);
            continue;
        }

        // Exploding gradient: mean gradient very large
        if (gfd.meanGradient > 10.0f) {
            problems.push_back(gfd.layerIndex);
            continue;
        }

        // Dead neurons: most gradients are near zero
        if (gfd.dead_neuron_ratio > 0.5f) {
            problems.push_back(gfd.layerIndex);
        }
    }

    return problems;
}

void InterpretabilityPanel::updateActivationStats(const std::vector<ActivationStats>& activationStats)
{
    m_activationStats = activationStats;

    if (m_currentVisualizationType == VisualizationType::ActivationDistribution) {
        updateDisplay();
    }

    emit visualizationUpdated(static_cast<int>(VisualizationType::ActivationDistribution));
}

std::map<int, float> InterpretabilityPanel::getSparsityReport() const
{
    std::map<int, float> sparsity;
    for (const auto& stats : m_activationStats) {
        sparsity[stats.layerIndex] = stats.sparsity;
    }
    return sparsity;
}

void InterpretabilityPanel::updateGradCAM(int layerIndex, const std::vector<float>& gradcamData)
{
    m_gradcamData[layerIndex] = gradcamData;

    if (m_currentVisualizationType == VisualizationType::GradCAM) {
        updateDisplay();
    }
}

void InterpretabilityPanel::updateLayerAttribution(const std::vector<LayerAttribution>& layerAttributions)
{
    m_layerAttributions = layerAttributions;

    // Identify critical layers
    std::sort(m_layerAttributions.begin(), m_layerAttributions.end(),
             [](const LayerAttribution& a, const LayerAttribution& b) {
                 return a.attributionScore > b.attributionScore;
             });

    if (!m_layerAttributions.empty()) {
        const auto& critical = m_layerAttributions[0];
        emit criticalLayerIdentified(critical.layerIndex, critical.attributionScore);
    }

    if (m_currentVisualizationType == VisualizationType::LayerContribution) {
        updateDisplay();
    }

    emit visualizationUpdated(static_cast<int>(VisualizationType::LayerContribution));
}

std::vector<InterpretabilityPanel::LayerAttribution> InterpretabilityPanel::getCriticalLayers() const
{
    return m_layerAttributions;
}

void InterpretabilityPanel::updateIntegratedGradients(const QString& inputName, const std::vector<float>& attributions)
{
    m_integratedGradients[inputName] = attributions;

    if (m_currentVisualizationType == VisualizationType::IntegratedGradients) {
        updateDisplay();
    }
}

void InterpretabilityPanel::updateSaliencyMap(const std::vector<float>& saliencyData)
{
    m_saliencyMap = saliencyData;

    if (m_currentVisualizationType == VisualizationType::SaliencyMap) {
        updateDisplay();
    }
}

void InterpretabilityPanel::setSelectedLayer(int layerIndex)
{
    m_selectedLayerIndex = layerIndex;
    m_layerSlider->blockSignals(true);
    m_layerSlider->setValue(layerIndex);
    m_layerSlider->blockSignals(false);
    updateDisplay();
    emit layerSelectionChanged(layerIndex);
}

void InterpretabilityPanel::setSelectedAttentionHead(int headIndex)
{
    m_selectedAttentionHeadIndex = headIndex;
    m_attentionHeadCombo->blockSignals(true);
    m_attentionHeadCombo->setCurrentIndex(headIndex);
    m_attentionHeadCombo->blockSignals(false);
    updateDisplay();
    emit attentionHeadSelectionChanged(headIndex);
}

void InterpretabilityPanel::setAttentionFocusPosition(int position)
{
    m_attentionFocusPosition = position;
    updateDisplay();
}

bool InterpretabilityPanel::exportVisualization(const QString& filePath) const
{
    if (!m_chartView) {
        return false;
    }

    // Simplified: log export request
    qDebug() << "Exporting visualization to" << filePath;
    return true;
}

QJsonObject InterpretabilityPanel::exportInterpretabilityData() const
{
    QJsonObject data;
    data["timestamp"] = static_cast<qint64>(QDateTime::currentSecsSinceEpoch());
    data["vizType"] = static_cast<int>(m_currentVisualizationType);

    // Export feature importance
    QJsonArray featureArray;
    for (const auto& feat : m_featureImportances) {
        QJsonObject obj;
        obj["featureId"] = feat.featureId;
        obj["featureName"] = feat.featureName;
        obj["importance"] = feat.importance;
        obj["rank"] = feat.rank;
        featureArray.append(obj);
    }
    data["features"] = featureArray;

    // Export gradient flow
    QJsonArray gradArray;
    for (const auto& gfd : m_gradientFlowData) {
        QJsonObject obj;
        obj["layerIndex"] = gfd.layerIndex;
        obj["layerName"] = gfd.layerName;
        obj["meanGradient"] = gfd.meanGradient;
        obj["deadNeuronRatio"] = gfd.dead_neuron_ratio;
        gradArray.append(obj);
    }
    data["gradientFlow"] = gradArray;

    // Export layer attribution
    QJsonArray attrArray;
    for (const auto& attr : m_layerAttributions) {
        QJsonObject obj;
        obj["layerIndex"] = attr.layerIndex;
        obj["layerName"] = attr.layerName;
        obj["attributionScore"] = attr.attributionScore;
        attrArray.append(obj);
    }
    data["layerAttributions"] = attrArray;

    return data;
}

bool InterpretabilityPanel::loadInterpretabilityData(const QJsonObject& data)
{
    // Parse and load visualization data
    int vizType = data["vizType"].toInt(1);
    setVisualizationType(static_cast<VisualizationType>(vizType));

    return true;
}

void InterpretabilityPanel::clearVisualizations()
{
    m_featureImportances.clear();
    m_gradientFlowData.clear();
    m_activationStats.clear();
    m_layerAttributions.clear();
    m_attentionHeads.clear();
    m_integratedGradients.clear();
    m_saliencyMap.clear();

    m_chart->removeAllSeries();
    m_statsLabel->setText("Visualizations cleared");
    m_problemsLabel->setText("");
}

void InterpretabilityPanel::updateDisplay()
{
    m_chart->removeAllSeries();
    m_chart->removeAllAxes();

    switch (m_currentVisualizationType) {
        case VisualizationType::FeatureImportance: {
            // Display top 10 features as bar chart
            auto topFeatures = getTopFeatures(10);
            if (topFeatures.empty()) {
                m_statsLabel->setText("No feature importance data available");
                break;
            }

            QString statsText = QString("Top 10 Features:\n");
            for (const auto& feat : topFeatures) {
                statsText += QString("%1. %2: %.4f\n").arg(feat.rank).arg(feat.featureName).arg(feat.importance);
            }
            m_statsLabel->setText(statsText);
            break;
        }

        case VisualizationType::GradientFlow: {
            if (m_gradientFlowData.empty()) {
                m_statsLabel->setText("No gradient flow data available");
                break;
            }

            QString statsText = "Gradient Flow Analysis:\n";
            for (const auto& gfd : m_gradientFlowData) {
                statsText += QString("Layer %1 (%2): mean=%.2e, std=%.2e, dead_neurons=%.1f%%\n")
                                .arg(gfd.layerIndex)
                                .arg(gfd.layerName)
                                .arg(gfd.meanGradient)
                                .arg(gfd.stdGradient)
                                .arg(gfd.dead_neuron_ratio * 100.0f);
            }
            m_statsLabel->setText(statsText);
            break;
        }

        case VisualizationType::ActivationDistribution: {
            if (m_activationStats.empty()) {
                m_statsLabel->setText("No activation statistics available");
                break;
            }

            const auto& stats = m_activationStats[std::min(static_cast<size_t>(m_selectedLayerIndex),
                                                            m_activationStats.size() - 1)];

            QString statsText = QString("Layer %1 (%2) Activation Stats:\n")
                                   .arg(stats.layerIndex)
                                   .arg(stats.layerName);
            statsText += QString("Mean: %.4f, StdDev: %.4f\n").arg(stats.mean).arg(stats.stdDev);
            statsText += QString("Min: %.4f, Max: %.4f\n").arg(stats.min).arg(stats.max);
            statsText += QString("Sparsity: %.2f%%\n").arg(stats.sparsity * 100.0f);

            m_statsLabel->setText(statsText);
            break;
        }

        case VisualizationType::LayerContribution: {
            if (m_layerAttributions.empty()) {
                m_statsLabel->setText("No layer attribution data available");
                break;
            }

            QString statsText = "Layer Contribution (Importance Ranking):\n";
            for (size_t i = 0; i < std::min(size_t(5), m_layerAttributions.size()); ++i) {
                const auto& attr = m_layerAttributions[i];
                statsText += QString("%1. Layer %2: %.4f\n")
                                .arg(i + 1)
                                .arg(attr.layerName)
                                .arg(attr.attributionScore);
            }
            m_statsLabel->setText(statsText);
            break;
        }

        case VisualizationType::AttentionHeatmap: {
            if (m_attentionHeads.empty()) {
                m_statsLabel->setText("No attention data available");
                break;
            }

            auto it = m_attentionHeads.find(m_selectedAttentionHeadIndex);
            if (it != m_attentionHeads.end()) {
                const auto& attData = it->second;
                QString statsText = QString("Attention Head %1 (Seq Len: %2)\n")
                                       .arg(attData.headIndex)
                                       .arg(attData.sequenceLength);
                statsText += "Query Tokens: " + attData.queryTokens.left(50) + "\n";
                statsText += "Key Tokens: " + attData.keyTokens.left(50);
                m_statsLabel->setText(statsText);
            }
            break;
        }

        case VisualizationType::GradCAM: {
            if (m_gradcamData.empty()) {
                m_statsLabel->setText("No GradCAM data available");
                break;
            }

            auto it = m_gradcamData.find(m_selectedLayerIndex);
            if (it != m_gradcamData.end()) {
                const auto& gcData = it->second;
                float avgActivation = 0.0f;
                if (!gcData.empty()) {
                    avgActivation = std::accumulate(gcData.begin(), gcData.end(), 0.0f) / gcData.size();
                }
                QString statsText = QString("GradCAM - Layer %1\n").arg(m_selectedLayerIndex);
                statsText += QString("Average Activation: %.4f\n").arg(avgActivation);
                statsText += QString("Activation Count: %1\n").arg(gcData.size());
                m_statsLabel->setText(statsText);
            }
            break;
        }

        default:
            m_statsLabel->setText("Visualization type not yet implemented");
            break;
    }
}

void InterpretabilityPanel::onLayerSliderChanged(int value)
{
    m_selectedLayerIndex = value;
    updateDisplay();
    emit layerSelectionChanged(value);
}

void InterpretabilityPanel::onAttentionHeadChanged(int index)
{
    m_selectedAttentionHeadIndex = index;
    updateDisplay();
    emit attentionHeadSelectionChanged(index);
}

void InterpretabilityPanel::onVisualizationTypeChanged(int index)
{
    m_currentVisualizationType = static_cast<VisualizationType>(index);
    updateDisplay();
    emit visualizationUpdated(index);
}

void InterpretabilityPanel::onRefreshDisplay()
{
    updateDisplay();
}
