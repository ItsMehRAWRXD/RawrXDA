#pragma once


#include <vector>
// QtCharts includes - commented out for now to avoid build issues


class InterpretabilityPanel : public void {

public:
    explicit InterpretabilityPanel(void* parent = nullptr);
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
    void updateVisualization(VisualizationType type, const void*& data);
    void clearVisualization();

    // Configuration
    void setLayerRange(int minLayer, int maxLayer);
    void setAttentionHeads(const std::vector<std::string>& heads);

    // Export capabilities
    void* getCurrentVisualization() const;
    bool exportVisualization(const std::string& filePath);

public:
    void onModelDataUpdated(const void*& data);


    void visualizationUpdated(VisualizationType type);

private:
    void setupUI();
    void updateChart();
    void updateStats();

    // QtCharts components - commented out for now
    // QtCharts::QChartView* m_chartView;
    // QtCharts::QChart* m_chart;
    VisualizationType m_currentType;
    void* m_currentData;
    int m_minLayer;
    int m_maxLayer;
    std::vector<std::string> m_attentionHeads;
    
    // Data storage for visualizations
    struct FeatureImportance {
        std::string name;
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

