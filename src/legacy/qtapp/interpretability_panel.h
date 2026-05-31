#pragma once

#include <QWidget>
#include <QJsonObject>
#include <vector>
// QtCharts includes - commented out for now to avoid build issues
//#include <QtCharts/QChartView>
//#include <QtCharts/QChart>
//#include <QtCharts/QLineSeries>
//#include <QtCharts/QBarSeries>
//#include <QtCharts/QValueAxis>


class InterpretabilityPanel : public QWidget {
    Q_OBJECT

public:
    explicit InterpretabilityPanel(QWidget* parent = nullptr);
    ~InterpretabilityPanel();

    // Visualization types
    enum class VisualizationType {
        AttentionHeatmap,
        FeatureImportance,
        GradientFlow,
        ActivationDistribution,
        LayerContribution
    };

    // Update visualizations
    void updateVisualization(VisualizationType type, const QJsonObject& data);
    void clearVisualization();

    // Configuration
    void setLayerRange(int minLayer, int maxLayer);
    void setAttentionHeads(const QStringList& heads);

    // Export capabilities
    QJsonObject getCurrentVisualization() const;
    bool exportVisualization(const QString& filePath);

public slots:
    void onModelDataUpdated(const QJsonObject& data);

signals:
    void visualizationUpdated(VisualizationType type);

private:
    void setupUI();
    void updateChart();
    void updateStats();

    // QtCharts components - commented out for now
    // QtCharts::QChartView* m_chartView;
    // QtCharts::QChart* m_chart;
    VisualizationType m_currentType;
    QJsonObject m_currentData;
    int m_minLayer;
    int m_maxLayer;
    QStringList m_attentionHeads;
    
    // Data storage for visualizations
    struct FeatureImportance {
        QString name;
        double importance;
    };
    
    std::vector<FeatureImportance> m_featureImportances;
    
    struct GradientFlowData {
        int layer;
        double gradientNorm;
        double updateMagnitude;
    };
    
    std::vector<GradientFlowData> m_gradientFlowData;
};
