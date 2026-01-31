#pragma once

/**
 * @file interpretability_panel_production.hpp
 * @brief Production-Grade Interpretability Panel for Model Visualization
 * 
 * Comprehensive visualization component for deep learning model interpretability:
 * - Attention weight heatmaps with multi-head visualization
 * - Gradient flow analysis with vanishing/exploding gradient detection
 * - Feature importance ranking (SHAP, LIME, Integrated Gradients)
 * - Activation distribution histograms per layer
 * - Layer contribution analysis
 * - GradCAM visualization
 * - Saliency maps
 * - Embedding space visualization (t-SNE/UMAP)
 * 
 * Production Features:
 * - Structured JSON logging for observability
 * - Performance metrics tracking
 * - Real-time update support
 * - Export capabilities (PNG, SVG, JSON)
 * - Configurable color schemes
 * - Hardware-accelerated rendering
 * 
 * @author RawrXD Team
 * @date December 2025
 * @version 2.0-production
 */


#include <vector>
#include <map>
#include <memory>
#include <chrono>
#include <functional>

// Forward declarations


namespace RawrXD {

/**
 * @brief Production-grade interpretability panel for model visualization
 * 
 * Provides comprehensive model analysis and visualization capabilities
 * with production-ready observability and performance monitoring.
 */
class InterpretabilityPanelProduction : public void {

public:
    // =========================================================================
    // ENUMS AND TYPE DEFINITIONS
    // =========================================================================
    
    /**
     * @brief Visualization types supported by the panel
     */
    enum class VisualizationType {
        AttentionHeatmap,           ///< Attention weight visualization
        AttentionHeadComparison,    ///< Compare multiple attention heads
        FeatureImportance,          ///< SHAP/LIME feature importance
        GradientFlow,               ///< Gradient magnitude per layer
        ActivationDistribution,     ///< Layer activation histograms
        LayerContribution,          ///< Layer attribution analysis
        GradCAM,                    ///< Gradient-weighted Class Activation Mapping
        IntegratedGradients,        ///< Integrated Gradients attribution
        SaliencyMap,                ///< Input saliency visualization
        EmbeddingSpace,             ///< t-SNE/UMAP embedding visualization
        TokenAttribution,           ///< Token-level attribution for NLP
        NeuronActivation            ///< Individual neuron activation patterns
    };
    
    /**
     * @brief Color scheme options for visualizations
     */
    enum class ColorScheme {
        Viridis,                    ///< Perceptually uniform (default)
        Plasma,                     ///< High contrast
        Inferno,                    ///< Dark background friendly
        Magma,                      ///< Warm tones
        CoolWarm,                   ///< Diverging (positive/negative)
        RedBlue,                    ///< Classic diverging
        Grayscale                   ///< Accessibility-friendly
    };
    
    /**
     * @brief Export format options
     */
    enum class ExportFormat {
        PNG,                        ///< Raster image
        SVG,                        ///< Vector graphics
        JSON,                       ///< Raw data export
        CSV,                        ///< Tabular data
        HTML                        ///< Interactive visualization
    };
    
    /**
     * @brief Gradient problem types for detection
     */
    enum class GradientProblem {
        None,                       ///< No issues detected
        Vanishing,                  ///< Gradient too small
        Exploding,                  ///< Gradient too large
        DeadNeurons,                ///< High dead neuron ratio
        UnstableUpdates             ///< High variance in updates
    };
    
    // =========================================================================
    // DATA STRUCTURES
    // =========================================================================
    
    /**
     * @brief Attention head data for visualization
     */
    struct AttentionData {
        int headIndex = 0;                          ///< Attention head index
        int layerIndex = 0;                         ///< Layer index
        std::vector<std::vector<float>> weights;    ///< 2D attention weights [query][key]
        std::vector<std::string> tokens;                         ///< Token labels
        float maxWeight = 1.0f;                     ///< Maximum weight for normalization
        float entropy = 0.0f;                       ///< Attention entropy (sparsity measure)
        std::chrono::system_clock::time_point timestamp;
    };
    
    /**
     * @brief Feature importance data
     */
    struct FeatureImportance {
        std::string featureName;                        ///< Feature identifier
        double importance = 0.0;                    ///< Importance score [0, 1]
        double confidenceLow = 0.0;                 ///< Lower confidence bound
        double confidenceHigh = 0.0;                ///< Upper confidence bound
        std::string category;                           ///< Feature category
        bool isPositive = true;                     ///< Positive or negative contribution
    };
    
    /**
     * @brief Gradient flow data per layer
     */
    struct GradientFlowData {
        int layerIndex = 0;                         ///< Layer index
        std::string layerName;                          ///< Human-readable layer name
        double gradientNorm = 0.0;                  ///< L2 norm of gradients
        double gradientMean = 0.0;                  ///< Mean gradient value
        double gradientStd = 0.0;                   ///< Standard deviation
        double gradientMin = 0.0;                   ///< Minimum gradient
        double gradientMax = 0.0;                   ///< Maximum gradient
        double updateMagnitude = 0.0;               ///< Parameter update size
        float deadNeuronRatio = 0.0f;               ///< Ratio of neurons with zero gradient
        GradientProblem problem = GradientProblem::None;
    };
    
    /**
     * @brief Activation statistics per layer
     */
    struct ActivationStats {
        int layerIndex = 0;                         ///< Layer index
        std::string layerName;                          ///< Human-readable layer name
        double mean = 0.0;                          ///< Mean activation
        double std = 0.0;                           ///< Standard deviation
        double min = 0.0;                           ///< Minimum activation
        double max = 0.0;                           ///< Maximum activation
        double sparsity = 0.0;                      ///< Fraction of zero activations
        std::vector<int> histogram;                 ///< Histogram bins (default 50 bins)
        double histogramMin = 0.0;                  ///< Histogram range min
        double histogramMax = 0.0;                  ///< Histogram range max
    };
    
    /**
     * @brief Layer attribution/contribution data
     */
    struct LayerAttribution {
        int layerIndex = 0;                         ///< Layer index
        std::string layerName;                          ///< Human-readable layer name
        double contribution = 0.0;                  ///< Contribution score [0, 1]
        double positiveAttribution = 0.0;           ///< Positive contribution
        double negativeAttribution = 0.0;           ///< Negative contribution
        std::string layerType;                          ///< Layer type (attention, ffn, etc.)
    };
    
    /**
     * @brief Embedding point for visualization
     */
    struct EmbeddingPoint {
        double x = 0.0;                             ///< X coordinate (projected)
        double y = 0.0;                             ///< Y coordinate (projected)
        double z = 0.0;                             ///< Z coordinate (optional for 3D)
        std::string label;                              ///< Point label
        int cluster = -1;                           ///< Cluster assignment
        QColor color;                               ///< Display color
    };
    
    /**
     * @brief Token attribution data for NLP models
     */
    struct TokenAttribution {
        std::string token;                              ///< Token text
        double attribution = 0.0;                   ///< Attribution score
        double positiveAttribution = 0.0;           ///< Positive contribution
        double negativeAttribution = 0.0;           ///< Negative contribution
        int position = 0;                           ///< Position in sequence
    };
    
    /**
     * @brief Production metrics for the panel
     */
    struct PanelMetrics {
        quint64 totalUpdates = 0;                   ///< Total visualization updates
        quint64 totalExports = 0;                   ///< Total exports performed
        double avgUpdateTimeMs = 0.0;               ///< Average update latency
        double maxUpdateTimeMs = 0.0;               ///< Maximum update latency
        quint64 problemsDetected = 0;               ///< Gradient problems detected
        std::chrono::system_clock::time_point lastUpdate;
    };
    
    // =========================================================================
    // CONSTRUCTOR / DESTRUCTOR
    // =========================================================================
    
    explicit InterpretabilityPanelProduction(void* parent = nullptr);
    ~InterpretabilityPanelProduction() override;
    
    /**
     * @brief Initialize the panel (lazy initialization)
     * Call this before using any visualization features
     */
    void initialize();
    
    /**
     * @brief Check if panel is initialized
     */
    bool isInitialized() const { return m_initialized; }
    
    // =========================================================================
    // VISUALIZATION TYPE CONTROL
    // =========================================================================
    
    /**
     * @brief Set the current visualization type
     */
    void setVisualizationType(VisualizationType type);
    
    /**
     * @brief Get the current visualization type
     */
    VisualizationType getVisualizationType() const { return m_currentType; }
    
    // =========================================================================
    // ATTENTION VISUALIZATION
    // =========================================================================
    
    /**
     * @brief Update attention heatmap data
     * @param data Attention weight data for a specific head
     */
    void updateAttentionHeatmap(const AttentionData& data);
    
    /**
     * @brief Update multiple attention heads for comparison
     */
    void updateMultiHeadAttention(const std::vector<AttentionData>& heads);
    
    /**
     * @brief Get attention data for a specific head
     */
    AttentionData getAttentionData(int layerIndex, int headIndex) const;
    
    /**
     * @brief Get all stored attention heads
     */
    std::vector<AttentionData> getAllAttentionHeads() const;
    
    /**
     * @brief Calculate attention entropy (sparsity measure)
     */
    double calculateAttentionEntropy(const AttentionData& data) const;
    
    // =========================================================================
    // FEATURE IMPORTANCE
    // =========================================================================
    
    /**
     * @brief Update feature importance rankings
     */
    void updateFeatureImportance(const std::vector<FeatureImportance>& importances);
    
    /**
     * @brief Get top-K most important features
     */
    std::vector<FeatureImportance> getTopFeatures(int k = 10) const;
    
    /**
     * @brief Get features by category
     */
    std::vector<FeatureImportance> getFeaturesByCategory(const std::string& category) const;
    
    // =========================================================================
    // GRADIENT FLOW ANALYSIS
    // =========================================================================
    
    /**
     * @brief Update gradient flow data for all layers
     */
    void updateGradientFlow(const std::vector<GradientFlowData>& gradientData);
    
    /**
     * @brief Detect gradient problems (vanishing, exploding, dead neurons)
     * @return List of layer indices with problems
     */
    std::vector<int> detectGradientProblems() const;
    
    /**
     * @brief Get gradient flow data for a specific layer
     */
    GradientFlowData getGradientFlowData(int layerIndex) const;
    
    /**
     * @brief Get summary of gradient health
     */
    void* getGradientHealthSummary() const;
    
    // =========================================================================
    // ACTIVATION ANALYSIS
    // =========================================================================
    
    /**
     * @brief Update activation statistics for all layers
     */
    void updateActivationStats(const std::vector<ActivationStats>& stats);
    
    /**
     * @brief Get sparsity report for all layers
     */
    std::map<int, float> getSparsityReport() const;
    
    /**
     * @brief Get activation statistics for a specific layer
     */
    ActivationStats getActivationStats(int layerIndex) const;
    
    // =========================================================================
    // LAYER ATTRIBUTION
    // =========================================================================
    
    /**
     * @brief Update layer contribution/attribution data
     */
    void updateLayerAttribution(const std::vector<LayerAttribution>& attributions);
    
    /**
     * @brief Get layers with highest contribution
     */
    std::vector<LayerAttribution> getCriticalLayers(int topK = 5) const;
    
    // =========================================================================
    // GRADCAM VISUALIZATION
    // =========================================================================
    
    /**
     * @brief Update GradCAM heatmap for a specific layer
     * @param layerIndex Target layer
     * @param gradcamData Flattened heatmap data
     * @param width Heatmap width
     * @param height Heatmap height
     */
    void updateGradCAM(int layerIndex, const std::vector<float>& gradcamData, 
                       int width = 0, int height = 0);
    
    /**
     * @brief Get GradCAM image for display
     */
    QImage getGradCAMImage(int layerIndex) const;
    
    // =========================================================================
    // INTEGRATED GRADIENTS
    // =========================================================================
    
    /**
     * @brief Update integrated gradients attribution
     * @param inputName Input identifier
     * @param attributions Attribution values
     */
    void updateIntegratedGradients(const std::string& inputName, 
                                   const std::vector<float>& attributions);
    
    /**
     * @brief Get integrated gradients for a specific input
     */
    std::vector<float> getIntegratedGradients(const std::string& inputName) const;
    
    // =========================================================================
    // SALIENCY MAP
    // =========================================================================
    
    /**
     * @brief Update saliency map
     * @param saliencyData Flattened saliency values
     * @param width Map width
     * @param height Map height
     */
    void updateSaliencyMap(const std::vector<float>& saliencyData,
                           int width = 0, int height = 0);
    
    /**
     * @brief Get saliency map as image
     */
    QImage getSaliencyImage() const;
    
    // =========================================================================
    // EMBEDDING SPACE VISUALIZATION
    // =========================================================================
    
    /**
     * @brief Update embedding space points
     */
    void updateEmbeddingSpace(const std::vector<EmbeddingPoint>& points);
    
    /**
     * @brief Get embedding points by cluster
     */
    std::vector<EmbeddingPoint> getEmbeddingsByCluster(int cluster) const;
    
    // =========================================================================
    // TOKEN ATTRIBUTION (NLP)
    // =========================================================================
    
    /**
     * @brief Update token-level attribution
     */
    void updateTokenAttribution(const std::vector<TokenAttribution>& tokens);
    
    /**
     * @brief Get tokens with highest attribution
     */
    std::vector<TokenAttribution> getTopAttributedTokens(int topK = 10) const;
    
    // =========================================================================
    // SELECTION AND NAVIGATION
    // =========================================================================
    
    /**
     * @brief Set selected layer for detailed view
     */
    void setSelectedLayer(int layerIndex);
    
    /**
     * @brief Get currently selected layer
     */
    int getSelectedLayer() const { return m_selectedLayerIndex; }
    
    /**
     * @brief Set selected attention head
     */
    void setSelectedAttentionHead(int headIndex);
    
    /**
     * @brief Get currently selected attention head
     */
    int getSelectedAttentionHead() const { return m_selectedHeadIndex; }
    
    /**
     * @brief Set layer range for visualization
     */
    void setLayerRange(int minLayer, int maxLayer);
    
    // =========================================================================
    // APPEARANCE AND STYLING
    // =========================================================================
    
    /**
     * @brief Set color scheme for visualizations
     */
    void setColorScheme(ColorScheme scheme);
    
    /**
     * @brief Get current color scheme
     */
    ColorScheme getColorScheme() const { return m_colorScheme; }
    
    /**
     * @brief Enable/disable dark mode
     */
    void setDarkMode(bool enabled);
    
    /**
     * @brief Set visualization opacity
     */
    void setOpacity(float opacity);
    
    // =========================================================================
    // REAL-TIME UPDATES
    // =========================================================================
    
    /**
     * @brief Enable/disable auto-refresh
     */
    void setAutoRefresh(bool enabled, int intervalMs = 1000);
    
    /**
     * @brief Set update callback for real-time data
     */
    void setUpdateCallback(std::function<void()> callback);
    
    /**
     * @brief Force immediate refresh
     */
    void refresh();
    
    // =========================================================================
    // EXPORT CAPABILITIES
    // =========================================================================
    
    /**
     * @brief Export current visualization
     * @param filePath Output file path
     * @param format Export format
     * @return Success status
     */
    bool exportVisualization(const std::string& filePath, ExportFormat format = ExportFormat::PNG);
    
    /**
     * @brief Export all data as JSON
     */
    void* exportAllData() const;
    
    /**
     * @brief Import data from JSON
     */
    bool importData(const void*& data);
    
    // =========================================================================
    // METRICS AND OBSERVABILITY
    // =========================================================================
    
    /**
     * @brief Get panel performance metrics
     */
    PanelMetrics getMetrics() const { return m_metrics; }
    
    /**
     * @brief Clear all visualizations and reset state
     */
    void clearAll();
    
    // =========================================================================
    // SIGNALS
    // =========================================================================
    
    /**
     * @brief Emitted when visualization is updated
     */
    void visualizationUpdated(VisualizationType type);
    
    /**
     * @brief Emitted when layer selection changes
     */
    void layerSelectionChanged(int layerIndex);
    
    /**
     * @brief Emitted when attention head selection changes
     */
    void attentionHeadSelectionChanged(int headIndex);
    
    /**
     * @brief Emitted when gradient problem is detected
     */
    void gradientProblemDetected(int layerIndex, GradientProblem problem);
    
    /**
     * @brief Emitted when export completes
     */
    void exportCompleted(const std::string& filePath, bool success);
    
    /**
     * @brief Emitted on error
     */
    void errorOccurred(const std::string& error);
    
    // =========================================================================
    // PRIVATE SLOTS
    // =========================================================================
    
private:
    void onVisualizationTypeChanged(int index);
    void onLayerSliderChanged(int value);
    void onHeadComboChanged(int index);
    void onColorSchemeChanged(int index);
    void onExportButtonClicked();
    void onRefreshTimerTimeout();
    void onZoomIn();
    void onZoomOut();
    void onResetZoom();
    
    // =========================================================================
    // PRIVATE METHODS
    // =========================================================================
    
private:
    void setupUI();
    void setupConnections();
    void createToolbar();
    void createVisualizationArea();
    void createControlPanel();
    void createStatusBar();
    
    void updateDisplay();
    void renderAttentionHeatmap();
    void renderFeatureImportance();
    void renderGradientFlow();
    void renderActivationDistribution();
    void renderLayerContribution();
    void renderGradCAM();
    void renderIntegratedGradients();
    void renderSaliencyMap();
    void renderEmbeddingSpace();
    void renderTokenAttribution();
    
    QColor getColorForValue(float value, float min = 0.0f, float max = 1.0f) const;
    QImage createHeatmapImage(const std::vector<std::vector<float>>& data) const;
    QImage createBarChartImage(const std::vector<double>& values, 
                               const std::vector<std::string>& labels) const;
    
    void logStructured(const std::string& level, const std::string& event, 
                       const void*& data = void*());
    void updateMetrics(double updateTimeMs);
    
    // =========================================================================
    // MEMBER VARIABLES
    // =========================================================================
    
    bool m_initialized = false;
    VisualizationType m_currentType = VisualizationType::AttentionHeatmap;
    ColorScheme m_colorScheme = ColorScheme::Viridis;
    
    int m_selectedLayerIndex = 0;
    int m_selectedHeadIndex = 0;
    int m_minLayer = 0;
    int m_maxLayer = 100;
    float m_opacity = 1.0f;
    float m_zoomLevel = 1.0f;
    bool m_darkMode = false;
    
    // Data storage
    std::map<std::pair<int, int>, AttentionData> m_attentionData;  // (layer, head) -> data
    std::vector<FeatureImportance> m_featureImportances;
    std::vector<GradientFlowData> m_gradientFlowData;
    std::vector<ActivationStats> m_activationStats;
    std::vector<LayerAttribution> m_layerAttributions;
    std::map<int, std::vector<float>> m_gradcamData;
    std::map<int, std::pair<int, int>> m_gradcamDimensions;  // layer -> (width, height)
    std::map<std::string, std::vector<float>> m_integratedGradients;
    std::vector<float> m_saliencyMap;
    std::pair<int, int> m_saliencyDimensions;
    std::vector<EmbeddingPoint> m_embeddingPoints;
    std::vector<TokenAttribution> m_tokenAttributions;
    
    // Metrics
    PanelMetrics m_metrics;
    QElapsedTimer m_updateTimer;
    
    // UI Components
    QVBoxLayout* m_mainLayout = nullptr;
    QTabWidget* m_tabWidget = nullptr;
    QComboBox* m_vizTypeCombo = nullptr;
    QComboBox* m_colorSchemeCombo = nullptr;
    QSlider* m_layerSlider = nullptr;
    QComboBox* m_headCombo = nullptr;
    QLabel* m_statsLabel = nullptr;
    QLabel* m_problemsLabel = nullptr;
    QProgressBar* m_updateProgress = nullptr;
    QPushButton* m_exportButton = nullptr;
    QPushButton* m_refreshButton = nullptr;
    QScrollArea* m_visualizationArea = nullptr;
    void* m_visualizationCanvas = nullptr;
    QTableWidget* m_dataTable = nullptr;
    
    // Timer for auto-refresh
    void** m_refreshTimer = nullptr;
    std::function<void()> m_updateCallback;
    
    // Rendered images
    QPixmap m_currentVisualization;
};

} // namespace RawrXD

