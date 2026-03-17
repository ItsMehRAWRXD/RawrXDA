#pragma once

#include <QWidget>
#include <QString>
#include <QJsonObject>
#include <vector>
#include <map>
#include <memory>

class QTabWidget;
class QLabel;
class QSlider;
class QComboBox;
class QChartView;
class QChart;
class QLineSeries;
class QBarSeries;

/**
 * @class InterpretabilityPanel
 * @brief Visualize and interpret model behavior during training
 *
 * Features:
 * - Attention weight visualization (heatmaps)
 * - Feature importance rankings (which features matter most)
 * - Gradient flow analysis (gradient magnitudes per layer)
 * - Activation distribution (neuron activation statistics)
 * - Attention head comparison (multi-head attention analysis)
 * - GradCAM visualization (gradient-based class activation mapping)
 * - Layer-wise contribution analysis
 * - Embedding space visualization (t-SNE/UMAP)
 * - Integrated gradients for input attribution
 * - Saliency maps for input importance
 *
 * Real-time updates during training with interactive exploration.
 */
class InterpretabilityPanel : public QWidget
{
    Q_OBJECT

public:
    enum class VisualizationType {
        AttentionHeatmap,           // Attention weights as heatmap
        FeatureImportance,          // Feature ranking bar chart
        GradientFlow,               // Gradient magnitude per layer
        ActivationDistribution,     // Histogram of activations
        AttentionHeadComparison,    // Multi-head attention analysis
        GradCAM,                    // Gradient-based class activation
        LayerContribution,          // Layer-wise attribution
        EmbeddingSpace,             // Embedding visualization
        IntegratedGradients,        // Input attribution
        SaliencyMap                 // Input importance map
    };
    
    explicit InterpretabilityPanel(QWidget* parent = nullptr);
    ~InterpretabilityPanel();
    
    /**
     * Two-phase initialization - call after QApplication is ready
     * Creates all Qt widgets and sets up connections
     */
    void initialize();

    struct AttentionData {
        int headIndex;
        int sequenceLength;
        std::vector<std::vector<float>> weights;  // (seq_len, seq_len)
        QString queryTokens;
        QString keyTokens;
    };

    struct FeatureImportance {
        int featureId;
        QString featureName;
        float importance;           // 0.0 to 1.0
        float stdDev;
        int rank;
    };

    struct GradientFlowData {
        int layerIndex;
        QString layerName;
        float minGradient;
        float maxGradient;
        float meanGradient;
        float stdGradient;
        float dead_neuron_ratio;    // % of neurons with ~zero gradient
    };

    struct ActivationStats {
        int layerIndex;
        QString layerName;
        float mean;
        float stdDev;
        float min;
        float max;
        float sparsity;             // % of zero/near-zero activations
        std::vector<float> histogram;  // 32 bins
    };

    struct LayerAttribution {
        int layerIndex;
        QString layerName;
        float attributionScore;     // 0.0 to 1.0
        float relativeImportance;   // Normalized importance
    };

    // Constructor & initialization
    explicit InterpretabilityPanel(QWidget* parent = nullptr);
    ~InterpretabilityPanel() override;

    /**
     * @brief Set active visualization type
     * @param vizType Type to display
     */
    void setVisualizationType(VisualizationType vizType);

    /**
     * @brief Get current visualization type
     * @return Active visualization type
     */
    VisualizationType getCurrentVisualizationType() const;

    // ===== Attention Visualization =====
    /**
     * @brief Update attention weight heatmap
     * @param attentionData Attention weights for one head
     */
    void updateAttentionHeatmap(const AttentionData& attentionData);

    /**
     * @brief Get all attention heads for current batch
     * @return List of attention data per head
     */
    std::vector<AttentionData> getAttentionHeads() const;

    // ===== Feature Importance =====
    /**
     * @brief Update feature importance ranking
     * @param importances List of features with importance scores
     */
    void updateFeatureImportance(const std::vector<FeatureImportance>& importances);

    /**
     * @brief Get top-K important features
     * @param k Number of features to return
     * @return Top K features ranked by importance
     */
    std::vector<FeatureImportance> getTopFeatures(int k = 10) const;

    // ===== Gradient Flow Analysis =====
    /**
     * @brief Update gradient flow data
     * @param gradientData Gradient statistics per layer
     */
    void updateGradientFlow(const std::vector<GradientFlowData>& gradientData);

    /**
     * @brief Detect vanishing/exploding gradient issues
     * @return List of layer indices with problems
     */
    std::vector<int> detectGradientProblems() const;

    // ===== Activation Analysis =====
    /**
     * @brief Update activation statistics
     * @param activationStats Activation stats per layer
     */
    void updateActivationStats(const std::vector<ActivationStats>& activationStats);

    /**
     * @brief Get sparsity report (dead neurons)
     * @return Map of layer_index -> sparsity_ratio
     */
    std::map<int, float> getSparsityReport() const;

    // ===== GradCAM Visualization =====
    /**
     * @brief Update GradCAM heatmap (gradient-based activation)
     * @param layerIndex Layer to visualize
     * @param gradcamData Heatmap values
     */
    void updateGradCAM(int layerIndex, const std::vector<float>& gradcamData);

    // ===== Layer Attribution =====
    /**
     * @brief Update layer-wise contribution analysis
     * @param layerAttributions Attribution scores per layer
     */
    void updateLayerAttribution(const std::vector<LayerAttribution>& layerAttributions);

    /**
     * @brief Identify critical layers (highest attribution)
     * @return List of layers ranked by importance
     */
    std::vector<LayerAttribution> getCriticalLayers() const;

    // ===== Integrated Gradients =====
    /**
     * @brief Update integrated gradients for input attribution
     * @param inputName Input feature name
     * @param attributions Attribution value per input element
     */
    void updateIntegratedGradients(const QString& inputName, const std::vector<float>& attributions);

    // ===== Saliency Maps =====
    /**
     * @brief Update saliency map (input importance)
     * @param saliencyData Importance per input element
     */
    void updateSaliencyMap(const std::vector<float>& saliencyData);

    // ===== Interactive Controls =====
    /**
     * @brief Set layer index for layer-specific visualizations
     * @param layerIndex Layer to visualize
     */
    void setSelectedLayer(int layerIndex);

    /**
     * @brief Set attention head index for multi-head visualization
     * @param headIndex Head to display
     */
    void setSelectedAttentionHead(int headIndex);

    /**
     * @brief Set sequence position for attention focus
     * @param position Position in sequence (0-indexed)
     */
    void setAttentionFocusPosition(int position);

    // ===== Data Export =====
    /**
     * @brief Export current visualization to image file
     * @param filePath Output image path
     * @return true if successful
     */
    bool exportVisualization(const QString& filePath) const;

    /**
     * @brief Export interpretability data to JSON
     * @return Data as JSON object
     */
    QJsonObject exportInterpretabilityData() const;

    /**
     * @brief Load interpretability data from JSON
     * @param data JSON data to load
     * @return true if successful
     */
    bool loadInterpretabilityData(const QJsonObject& data);

    /**
     * @brief Clear all visualizations
     */
    void clearVisualizations();

signals:
    /// Emitted when layer selection changes
    void layerSelectionChanged(int layerIndex);

    /// Emitted when attention head selection changes
    void attentionHeadSelectionChanged(int headIndex);

    /// Emitted when gradient problem detected
    void gradientProblemDetected(const QString& problem);

    /// Emitted when critical layer identified
    void criticalLayerIdentified(int layerIndex, float attribution);

    /// Emitted when visualization updated
    void visualizationUpdated(int vizType);

public slots:
    /// Called when layer slider value changes
    void onLayerSliderChanged(int value);

    /// Called when attention head selector changes
    void onAttentionHeadChanged(int index);

    /// Called when visualization type selector changes
    void onVisualizationTypeChanged(int index);

    /// Called periodically to refresh display
    void onRefreshDisplay();

private:
    void setupUI();
    void setupConnections();
    void createCharts();
    void updateDisplay();

    // ===== Internal State =====
    VisualizationType m_currentVisualizationType;
    int m_selectedLayerIndex;
    int m_selectedAttentionHeadIndex;
    int m_attentionFocusPosition;

    // Stored data
    std::map<int, AttentionData> m_attentionHeads;
    std::vector<FeatureImportance> m_featureImportances;
    std::vector<GradientFlowData> m_gradientFlowData;
    std::vector<ActivationStats> m_activationStats;
    std::map<int, std::vector<float>> m_gradcamData;
    std::vector<LayerAttribution> m_layerAttributions;
    std::map<QString, std::vector<float>> m_integratedGradients;
    std::vector<float> m_saliencyMap;

    // ===== UI Components =====
    QTabWidget* m_tabWidget;
    QComboBox* m_vizTypeCombo;
    QSlider* m_layerSlider;
    QComboBox* m_attentionHeadCombo;
    QChartView* m_chartView;
    QChart* m_chart;
    QLabel* m_statsLabel;
    QLabel* m_problemsLabel;
};
