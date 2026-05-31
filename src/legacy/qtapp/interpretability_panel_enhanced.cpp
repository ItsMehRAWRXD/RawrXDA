#include "interpretability_panel_enhanced.hpp"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTabWidget>
#include <QComboBox>
#include <QSlider>
#include <QLabel>
#include <QPainter>
#include <QChart>
#include <QChartView>
#include <QLineSeries>
#include <QBarSeries>
#include <QBarSet>
#include <QValueAxis>
#include <QCategoryAxis>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonArray>
#include <QFile>
#include <QTextStream>
#include <algorithm>
#include <numeric>
#include <cmath>

// ========== CONSTRUCTOR & DESTRUCTOR ==========

InterpretabilityPanelEnhanced::InterpretabilityPanelEnhanced(QWidget* parent)
    : QWidget(parent),
      m_current_visualization(VisualizationType::AttentionHeatmap),
      m_selected_layer(0),
      m_selected_attention_head(0),
      m_attention_focus_position(0),
      m_min_layer(0),
      m_max_layer(100),
      m_gradient_tracking_enabled(true),
      m_vanishing_threshold(1e-7f),
      m_exploding_threshold(10.0f),
      m_sparsity_threshold(0.5f)
{
    qDebug() << "[InterpretabilityPanelEnhanced] Constructing panel (lazy-init)";
    
    // Log initialization
    QJsonObject init_event;
    init_event["event"] = "panel_constructed";
    init_event["timestamp"] = QString::number(std::chrono::system_clock::now().time_since_epoch().count());
    logEvent("initialization", init_event);
}

InterpretabilityPanelEnhanced::~InterpretabilityPanelEnhanced()
{
    qDebug() << "[InterpretabilityPanelEnhanced] Destroying panel";
    
    QJsonObject cleanup_event;
    cleanup_event["event"] = "panel_destroyed";
    cleanup_event["total_updates"] = m_perf_metrics.total_updates;
    cleanup_event["total_exports"] = m_perf_metrics.total_exports;
    logEvent("lifecycle", cleanup_event);
}

// ========== VISUALIZATION UPDATES ==========

void InterpretabilityPanelEnhanced::updateAttentionHeads(const std::vector<AttentionHead>& attention_heads)
{
    auto start_time = std::chrono::high_resolution_clock::now();
    
    qDebug() << "[InterpretabilityPanelEnhanced] Updating" << attention_heads.size() << "attention heads";
    
    for (const auto& head : attention_heads) {
        m_attention_heads[head.layer_idx][head.head_idx] = head;
    }
    
    analyzeAttentionPatterns();
    
    auto end_time = std::chrono::high_resolution_clock::now();
    m_perf_metrics.last_update_duration = end_time - start_time;
    m_perf_metrics.total_updates++;
    
    emit visualizationUpdated(static_cast<int>(VisualizationType::AttentionHeatmap));
    
    QJsonObject event;
    event["event"] = "attention_updated";
    event["head_count"] = static_cast<int>(attention_heads.size());
    event["duration_ms"] = std::chrono::duration<double>(end_time - start_time).count() * 1000;
    logEvent("visualization", event);
}

void InterpretabilityPanelEnhanced::updateGradientFlow(const std::vector<GradientFlowMetrics>& gradient_data)
{
    auto start_time = std::chrono::high_resolution_clock::now();
    
    qDebug() << "[InterpretabilityPanelEnhanced] Updating gradient flow for"
             << gradient_data.size() << "layers";
    
    if (!m_gradient_tracking_enabled) {
        qWarning() << "[InterpretabilityPanelEnhanced] Gradient tracking disabled";
        return;
    }
    
    for (const auto& metrics : gradient_data) {
        m_gradient_flow_data[metrics.layer_idx] = metrics;
    }
    
    analyzeGradientFlow();
    
    auto end_time = std::chrono::high_resolution_clock::now();
    m_perf_metrics.last_update_duration = end_time - start_time;
    m_perf_metrics.total_updates++;
    
    emit visualizationUpdated(static_cast<int>(VisualizationType::GradientFlow));
    
    QJsonObject event;
    event["event"] = "gradient_flow_updated";
    event["layer_count"] = static_cast<int>(gradient_data.size());
    event["duration_ms"] = std::chrono::duration<double>(end_time - start_time).count() * 1000;
    logEvent("visualization", event);
}

void InterpretabilityPanelEnhanced::updateActivationStats(const std::vector<ActivationStats>& activation_stats)
{
    auto start_time = std::chrono::high_resolution_clock::now();
    
    qDebug() << "[InterpretabilityPanelEnhanced] Updating activation stats for"
             << activation_stats.size() << "layers";
    
    for (const auto& stats : activation_stats) {
        m_activation_stats[stats.layer_idx] = stats;
    }
    
    analyzeActivationPatterns();
    
    auto end_time = std::chrono::high_resolution_clock::now();
    m_perf_metrics.last_update_duration = end_time - start_time;
    m_perf_metrics.total_updates++;
    
    emit visualizationUpdated(static_cast<int>(VisualizationType::ActivationDistribution));
    
    QJsonObject event;
    event["event"] = "activation_stats_updated";
    event["layer_count"] = static_cast<int>(activation_stats.size());
    event["duration_ms"] = std::chrono::duration<double>(end_time - start_time).count() * 1000;
    logEvent("visualization", event);
}

void InterpretabilityPanelEnhanced::updateFeatureImportance(const std::vector<FeatureAttribution>& attributions)
{
    auto start_time = std::chrono::high_resolution_clock::now();
    
    qDebug() << "[InterpretabilityPanelEnhanced] Updating feature importance with"
             << attributions.size() << "features";
    
    m_feature_attributions = attributions;
    
    // Sort by attribution score (descending)
    std::sort(m_feature_attributions.begin(), m_feature_attributions.end(),
              [](const FeatureAttribution& a, const FeatureAttribution& b) {
                  return std::abs(a.attribution_score) > std::abs(b.attribution_score);
              });
    
    // Update ranks
    for (size_t i = 0; i < m_feature_attributions.size(); ++i) {
        m_feature_attributions[i].rank = static_cast<int>(i + 1);
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    m_perf_metrics.last_update_duration = end_time - start_time;
    m_perf_metrics.total_updates++;
    
    emit visualizationUpdated(static_cast<int>(VisualizationType::FeatureImportance));
    
    QJsonObject event;
    event["event"] = "feature_importance_updated";
    event["feature_count"] = static_cast<int>(attributions.size());
    event["top_feature"] = m_feature_attributions.empty() ? "none" : m_feature_attributions[0].feature_name;
    event["duration_ms"] = std::chrono::duration<double>(end_time - start_time).count() * 1000;
    logEvent("visualization", event);
}

void InterpretabilityPanelEnhanced::updateLayerAttribution(const std::vector<LayerAttribution>& layer_attributions)
{
    auto start_time = std::chrono::high_resolution_clock::now();
    
    qDebug() << "[InterpretabilityPanelEnhanced] Updating layer attribution for"
             << layer_attributions.size() << "layers";
    
    m_layer_attributions = layer_attributions;
    
    // Sort by contribution (descending)
    std::sort(m_layer_attributions.begin(), m_layer_attributions.end(),
              [](const LayerAttribution& a, const LayerAttribution& b) {
                  return a.contribution > b.contribution;
              });
    
    auto end_time = std::chrono::high_resolution_clock::now();
    m_perf_metrics.last_update_duration = end_time - start_time;
    m_perf_metrics.total_updates++;
    
    emit visualizationUpdated(static_cast<int>(VisualizationType::LayerContribution));
    
    QJsonObject event;
    event["event"] = "layer_attribution_updated";
    event["layer_count"] = static_cast<int>(layer_attributions.size());
    event["top_layer"] = m_layer_attributions.empty() ? -1 : m_layer_attributions[0].layer_idx;
    event["duration_ms"] = std::chrono::duration<double>(end_time - start_time).count() * 1000;
    logEvent("visualization", event);
}

void InterpretabilityPanelEnhanced::updateGradCAM(const GradCAMMap& gradcam_map)
{
    qDebug() << "[InterpretabilityPanelEnhanced] Updating GradCAM for layer"
             << gradcam_map.layer_idx << "class" << gradcam_map.target_class;
    
    m_gradcam_data[gradcam_map.layer_idx] = gradcam_map;
    emit visualizationUpdated(static_cast<int>(VisualizationType::GradCAM));
}

void InterpretabilityPanelEnhanced::updateIntegratedGradients(const QString& input_name, 
                                                             const std::vector<float>& attributions)
{
    qDebug() << "[InterpretabilityPanelEnhanced] Updating integrated gradients for"
             << input_name << "with" << attributions.size() << "values";
    
    m_integrated_gradients[input_name] = attributions;
    emit visualizationUpdated(static_cast<int>(VisualizationType::IntegratedGradients));
}

void InterpretabilityPanelEnhanced::updateSaliencyMap(const std::vector<float>& saliency_data)
{
    qDebug() << "[InterpretabilityPanelEnhanced] Updating saliency map with"
             << saliency_data.size() << "values";
    
    m_saliency_map = saliency_data;
    emit visualizationUpdated(static_cast<int>(VisualizationType::SaliencyMap));
}

void InterpretabilityPanelEnhanced::updateTokenLogits(int token_idx, const std::vector<float>& logits)
{
    qDebug() << "[InterpretabilityPanelEnhanced] Updating token" << token_idx
             << "logits with" << logits.size() << "classes";
    
    m_token_logits[token_idx] = logits;
    emit visualizationUpdated(static_cast<int>(VisualizationType::TokenLogits));
}

// ========== ANALYSIS & DIAGNOSTICS ==========

InterpretabilityPanelEnhanced::ModelDiagnostics InterpretabilityPanelEnhanced::runDiagnostics()
{
    auto start_time = std::chrono::high_resolution_clock::now();
    
    qDebug() << "[InterpretabilityPanelEnhanced] Running comprehensive diagnostics";
    
    ModelDiagnostics diagnostics;
    diagnostics.timestamp = std::chrono::system_clock::now();
    
    // Check gradient flow
    for (const auto& [layer_idx, metrics] : m_gradient_flow_data) {
        if (metrics.is_vanishing) {
            diagnostics.has_vanishing_gradients = true;
            diagnostics.critical_layer_indices.push_back(layer_idx);
            diagnostics.problematic_layers++;
        }
        if (metrics.is_exploding) {
            diagnostics.has_exploding_gradients = true;
            diagnostics.critical_layer_indices.push_back(layer_idx);
            diagnostics.problematic_layers++;
        }
    }
    
    // Check activation patterns
    float total_sparsity = 0.0f;
    int sparsity_count = 0;
    for (const auto& [layer_idx, stats] : m_activation_stats) {
        if (stats.sparsity > m_sparsity_threshold) {
            diagnostics.has_dead_neurons = true;
            diagnostics.critical_layer_indices.push_back(layer_idx);
        }
        total_sparsity += stats.sparsity;
        sparsity_count++;
    }
    
    if (sparsity_count > 0) {
        diagnostics.average_sparsity = total_sparsity / sparsity_count;
    }
    
    // Check attention patterns
    float total_entropy = 0.0f;
    int entropy_count = 0;
    for (const auto& [layer_idx, heads] : m_attention_heads) {
        for (const auto& [head_idx, head] : heads) {
            total_entropy += head.entropy;
            entropy_count++;
            
            // Low entropy indicates focus on few tokens
            if (head.entropy < 0.5f) {
                diagnostics.attention_entropy_mean = total_entropy / entropy_count;
            }
        }
    }
    
    if (entropy_count > 0) {
        diagnostics.attention_entropy_mean = total_entropy / entropy_count;
    }
    
    // Remove duplicate layer indices
    std::sort(diagnostics.critical_layer_indices.begin(), diagnostics.critical_layer_indices.end());
    diagnostics.critical_layer_indices.erase(
        std::unique(diagnostics.critical_layer_indices.begin(), 
                   diagnostics.critical_layer_indices.end()),
        diagnostics.critical_layer_indices.end()
    );
    
    auto end_time = std::chrono::high_resolution_clock::now();
    m_perf_metrics.last_diagnostics_duration = end_time - start_time;
    // Convert high_resolution_clock to system_clock for compatibility
    m_last_diagnostics_time = std::chrono::system_clock::now();
    m_cached_diagnostics = diagnostics;
    
    // Emit diagnostics signal
    QJsonObject diag_json;
    diag_json["has_vanishing_gradients"] = diagnostics.has_vanishing_gradients;
    diag_json["has_exploding_gradients"] = diagnostics.has_exploding_gradients;
    diag_json["has_dead_neurons"] = diagnostics.has_dead_neurons;
    diag_json["average_sparsity"] = diagnostics.average_sparsity;
    diag_json["attention_entropy"] = diagnostics.attention_entropy_mean;
    diag_json["problematic_layers"] = diagnostics.problematic_layers;
    diag_json["duration_ms"] = std::chrono::duration<double>(end_time - start_time).count() * 1000;
    
    QJsonArray critical_layers;
    for (int idx : diagnostics.critical_layer_indices) {
        critical_layers.append(idx);
    }
    diag_json["critical_layers"] = critical_layers;
    
    emit diagnosticsCompleted(diag_json);
    
    QJsonObject event;
    event["event"] = "diagnostics_completed";
    event["duration_ms"] = std::chrono::duration<double>(end_time - start_time).count() * 1000;
    event["has_issues"] = diagnostics.has_vanishing_gradients || diagnostics.has_exploding_gradients || 
                          diagnostics.has_dead_neurons;
    logEvent("diagnostics", event);
    
    return diagnostics;
}

std::vector<int> InterpretabilityPanelEnhanced::detectGradientProblems() const
{
    std::vector<int> problems;
    
    for (const auto& [layer_idx, metrics] : m_gradient_flow_data) {
        if (metrics.is_vanishing || metrics.is_exploding || metrics.dead_neuron_ratio > 0.9f) {
            problems.push_back(layer_idx);
        }
    }
    
    std::sort(problems.begin(), problems.end());
    problems.erase(std::unique(problems.begin(), problems.end()), problems.end());
    
    return problems;
}

std::map<int, float> InterpretabilityPanelEnhanced::getSparsityReport() const
{
    std::map<int, float> sparsity_map;
    
    for (const auto& [layer_idx, stats] : m_activation_stats) {
        sparsity_map[layer_idx] = stats.sparsity;
    }
    
    return sparsity_map;
}

std::map<std::pair<int, int>, float> InterpretabilityPanelEnhanced::getAttentionEntropy() const
{
    std::map<std::pair<int, int>, float> entropy_map;
    
    for (const auto& [layer_idx, heads] : m_attention_heads) {
        for (const auto& [head_idx, head] : heads) {
            entropy_map[{layer_idx, head_idx}] = head.entropy;
        }
    }
    
    return entropy_map;
}

std::vector<int> InterpretabilityPanelEnhanced::getCriticalLayers(int k) const
{
    auto problems = detectGradientProblems();
    
    if (static_cast<int>(problems.size()) <= k) {
        return problems;
    }
    
    std::vector<int> result(problems.begin(), problems.begin() + k);
    return result;
}

std::vector<InterpretabilityPanelEnhanced::FeatureAttribution> 
InterpretabilityPanelEnhanced::getTopFeatures(int k) const
{
    std::vector<FeatureAttribution> result;
    
    for (size_t i = 0; i < m_feature_attributions.size() && static_cast<int>(i) < k; ++i) {
        result.push_back(m_feature_attributions[i]);
    }
    
    return result;
}

// ========== CONFIGURATION ==========

void InterpretabilityPanelEnhanced::setVisualizationType(VisualizationType viz_type)
{
    qDebug() << "[InterpretabilityPanelEnhanced] Setting visualization type to"
             << static_cast<int>(viz_type);
    
    m_current_visualization = viz_type;
    updateDisplay();
    
    QJsonObject event;
    event["event"] = "visualization_type_changed";
    event["new_type"] = static_cast<int>(viz_type);
    logEvent("configuration", event);
}

InterpretabilityPanelEnhanced::VisualizationType 
InterpretabilityPanelEnhanced::getCurrentVisualizationType() const
{
    return m_current_visualization;
}

void InterpretabilityPanelEnhanced::setSelectedLayer(int layer_index)
{
    qDebug() << "[InterpretabilityPanelEnhanced] Selected layer" << layer_index;
    
    m_selected_layer = layer_index;
    emit layerSelectionChanged(layer_index);
    updateDisplay();
}

void InterpretabilityPanelEnhanced::setSelectedAttentionHead(int head_index)
{
    qDebug() << "[InterpretabilityPanelEnhanced] Selected attention head" << head_index;
    
    m_selected_attention_head = head_index;
    emit attentionHeadSelectionChanged(head_index);
    updateDisplay();
}

void InterpretabilityPanelEnhanced::setAttentionFocusPosition(int position)
{
    qDebug() << "[InterpretabilityPanelEnhanced] Set attention focus position" << position;
    m_attention_focus_position = position;
    updateDisplay();
}

void InterpretabilityPanelEnhanced::setLayerRange(int min_layer, int max_layer)
{
    qDebug() << "[InterpretabilityPanelEnhanced] Setting layer range" << min_layer << "-" << max_layer;
    m_min_layer = min_layer;
    m_max_layer = max_layer;
}

void InterpretabilityPanelEnhanced::setGradientTrackingEnabled(bool enabled)
{
    qDebug() << "[InterpretabilityPanelEnhanced] Gradient tracking"
             << (enabled ? "enabled" : "disabled");
    m_gradient_tracking_enabled = enabled;
}

void InterpretabilityPanelEnhanced::setAnomalyThresholds(float vanishing_threshold,
                                                        float exploding_threshold,
                                                        float sparsity_threshold)
{
    qDebug() << "[InterpretabilityPanelEnhanced] Setting anomaly thresholds:"
             << "vanishing=" << vanishing_threshold
             << "exploding=" << exploding_threshold
             << "sparsity=" << sparsity_threshold;
    
    m_vanishing_threshold = vanishing_threshold;
    m_exploding_threshold = exploding_threshold;
    m_sparsity_threshold = sparsity_threshold;
}

// ========== DATA EXPORT ==========

QJsonObject InterpretabilityPanelEnhanced::exportAsJSON() const
{
    auto start_time = std::chrono::high_resolution_clock::now();
    
    QJsonObject root;
    root["export_timestamp"] = QString::number(std::chrono::system_clock::now().time_since_epoch().count());
    root["visualization_type"] = static_cast<int>(m_current_visualization);
    
    // Export attention heads
    QJsonObject attention_obj;
    for (const auto& [layer_idx, heads] : m_attention_heads) {
        QJsonArray heads_array;
        for (const auto& [head_idx, head] : heads) {
            QJsonObject head_obj;
            head_obj["head_idx"] = head_idx;
            head_obj["mean_weight"] = head.mean_attn_weight;
            head_obj["max_weight"] = head.max_attn_weight;
            head_obj["entropy"] = head.entropy;
            heads_array.append(head_obj);
        }
        attention_obj[QString::number(layer_idx)] = heads_array;
    }
    root["attention_heads"] = attention_obj;
    
    // Export gradient flow
    QJsonArray gradient_array;
    for (const auto& [layer_idx, metrics] : m_gradient_flow_data) {
        QJsonObject grad_obj;
        grad_obj["layer"] = layer_idx;
        grad_obj["norm"] = metrics.norm;
        grad_obj["variance"] = metrics.variance;
        grad_obj["dead_neuron_ratio"] = metrics.dead_neuron_ratio;
        grad_obj["is_vanishing"] = metrics.is_vanishing;
        grad_obj["is_exploding"] = metrics.is_exploding;
        gradient_array.append(grad_obj);
    }
    root["gradient_flow"] = gradient_array;
    
    // Export activation stats
    QJsonArray activation_array;
    for (const auto& [layer_idx, stats] : m_activation_stats) {
        QJsonObject stats_obj;
        stats_obj["layer"] = layer_idx;
        stats_obj["mean"] = stats.mean;
        stats_obj["variance"] = stats.variance;
        stats_obj["sparsity"] = stats.sparsity;
        stats_obj["dead_neurons"] = stats.dead_neuron_count;
        activation_array.append(stats_obj);
    }
    root["activation_stats"] = activation_array;
    
    // Export feature importance
    QJsonArray features_array;
    for (const auto& feat : m_feature_attributions) {
        QJsonObject feat_obj;
        feat_obj["name"] = feat.feature_name;
        feat_obj["attribution"] = feat.attribution_score;
        feat_obj["rank"] = feat.rank;
        features_array.append(feat_obj);
    }
    root["feature_importance"] = features_array;
    
    auto end_time = std::chrono::high_resolution_clock::now();
    root["export_duration_ms"] = std::chrono::duration<double>(end_time - start_time).count() * 1000;
    
    // Note: In const context, cannot modify m_perf_metrics.total_exports - client code handles tracking
    
    return root;
}

bool InterpretabilityPanelEnhanced::exportAsCSV(const QString& file_path, VisualizationType viz_type) const
{
    qDebug() << "[InterpretabilityPanelEnhanced] Exporting to CSV:" << file_path;
    
    QFile file(file_path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "[InterpretabilityPanelEnhanced] Failed to open file for writing:" << file_path;
        return false;
    }
    
    QTextStream stream(&file);
    
    try {
        switch (viz_type) {
            case VisualizationType::GradientFlow: {
                stream << "Layer,Norm,Variance,Min,Max,DeadNeuronRatio,IsVanishing,IsExploding\n";
                for (const auto& [layer_idx, metrics] : m_gradient_flow_data) {
                    stream << layer_idx << ","
                           << metrics.norm << ","
                           << metrics.variance << ","
                           << metrics.min_value << ","
                           << metrics.max_value << ","
                           << metrics.dead_neuron_ratio << ","
                           << (metrics.is_vanishing ? "true" : "false") << ","
                           << (metrics.is_exploding ? "true" : "false") << "\n";
                }
                break;
            }
            
            case VisualizationType::ActivationDistribution: {
                stream << "Layer,Mean,Variance,Min,Max,Sparsity,DeadNeurons\n";
                for (const auto& [layer_idx, stats] : m_activation_stats) {
                    stream << layer_idx << ","
                           << stats.mean << ","
                           << stats.variance << ","
                           << stats.min_val << ","
                           << stats.max_val << ","
                           << stats.sparsity << ","
                           << stats.dead_neuron_count << "\n";
                }
                break;
            }
            
            case VisualizationType::FeatureImportance: {
                stream << "Rank,Feature,Attribution,PositiveContribution,NegativeContribution\n";
                for (const auto& feat : m_feature_attributions) {
                    stream << feat.rank << ","
                           << feat.feature_name << ","
                           << feat.attribution_score << ","
                           << feat.positive_contribution << ","
                           << feat.negative_contribution << "\n";
                }
                break;
            }
            
            default: {
                qWarning() << "[InterpretabilityPanelEnhanced] Unsupported export type";
                file.close();
                return false;
            }
        }
        
        file.close();
        
        QJsonObject event;
        event["event"] = "csv_export_completed";
        event["file_path"] = file_path;
        event["viz_type"] = static_cast<int>(viz_type);
        logEvent("export", event);
        
        return true;
    } catch (const std::exception& e) {
        qWarning() << "[InterpretabilityPanelEnhanced] CSV export failed:" << QString::fromStdString(e.what());
        file.close();
        return false;
    }
}

bool InterpretabilityPanelEnhanced::exportAsPNG(const QString& file_path) const
{
    qDebug() << "[InterpretabilityPanelEnhanced] Exporting visualization to PNG:" << file_path;
    
    if (!m_chart_view) {
        qWarning() << "[InterpretabilityPanelEnhanced] Chart view not available";
        return false;
    }
    
    try {
        // Render chart to pixmap
        QPixmap pixmap = m_chart_view->grab();
        bool success = pixmap.save(file_path, "PNG");
        
        if (success) {
            QJsonObject event;
            event["event"] = "png_export_completed";
            event["file_path"] = file_path;
            logEvent("export", event);
        } else {
            qWarning() << "[InterpretabilityPanelEnhanced] Failed to save PNG";
        }
        
        return success;
    } catch (const std::exception& e) {
        qWarning() << "[InterpretabilityPanelEnhanced] PNG export failed:"
                   << QString::fromStdString(e.what());
        return false;
    }
}

bool InterpretabilityPanelEnhanced::loadFromJSON(const QJsonObject& data)
{
    qDebug() << "[InterpretabilityPanelEnhanced] Loading from JSON";
    
    try {
        m_current_visualization = static_cast<VisualizationType>(data["visualization_type"].toInt());
        
        // Load attention heads
        QJsonObject attention_obj = data["attention_heads"].toObject();
        for (auto it = attention_obj.begin(); it != attention_obj.end(); ++it) {
            int layer_idx = it.key().toInt();
            QJsonArray heads_array = it.value().toArray();
            for (const auto& head_val : heads_array) {
                QJsonObject head_obj = head_val.toObject();
                AttentionHead head;
                head.layer_idx = layer_idx;
                head.head_idx = head_obj["head_idx"].toInt();
                head.mean_attn_weight = static_cast<float>(head_obj["mean_weight"].toDouble());
                head.entropy = static_cast<float>(head_obj["entropy"].toDouble());
                m_attention_heads[layer_idx][head.head_idx] = head;
            }
        }
        
        QJsonObject event;
        event["event"] = "loaded_from_json";
        event["data_size_bytes"] = static_cast<int>(QJsonDocument(data).toJson().size());
        logEvent("io", event);
        
        return true;
    } catch (const std::exception& e) {
        qWarning() << "[InterpretabilityPanelEnhanced] JSON load failed:"
                   << QString::fromStdString(e.what());
        return false;
    }
}

void InterpretabilityPanelEnhanced::clearVisualizations()
{
    qDebug() << "[InterpretabilityPanelEnhanced] Clearing all visualizations";
    
    m_attention_heads.clear();
    m_gradient_flow_data.clear();
    m_activation_stats.clear();
    m_feature_attributions.clear();
    m_layer_attributions.clear();
    m_gradcam_data.clear();
    m_integrated_gradients.clear();
    m_saliency_map.clear();
    m_token_logits.clear();
    
    QJsonObject event;
    event["event"] = "visualizations_cleared";
    logEvent("lifecycle", event);
}

// ========== OBSERVABILITY & MONITORING ==========

QJsonObject InterpretabilityPanelEnhanced::getHealthStatus() const
{
    QJsonObject health;
    health["status"] = "healthy";
    health["timestamp"] = QString::number(std::chrono::system_clock::now().time_since_epoch().count());
    health["total_updates"] = m_perf_metrics.total_updates;
    health["total_exports"] = m_perf_metrics.total_exports;
    health["has_data"] = !m_attention_heads.empty() || !m_gradient_flow_data.empty();
    
    if (!m_cached_diagnostics.critical_layer_indices.empty()) {
        health["status"] = "degraded";
        QJsonArray critical_layers;
        for (int idx : m_cached_diagnostics.critical_layer_indices) {
            critical_layers.append(idx);
        }
        health["critical_layers"] = critical_layers;
    }
    
    return health;
}

QString InterpretabilityPanelEnhanced::getPrometheusMetrics() const
{
    QString metrics;
    
    metrics += "# HELP interpretability_total_updates Total visualization updates\n";
    metrics += "# TYPE interpretability_total_updates counter\n";
    metrics += QString("interpretability_total_updates %1\n").arg(m_perf_metrics.total_updates);
    
    metrics += "\n# HELP interpretability_total_exports Total data exports\n";
    metrics += "# TYPE interpretability_total_exports counter\n";
    metrics += QString("interpretability_total_exports %1\n").arg(m_perf_metrics.total_exports);
    
    metrics += "\n# HELP interpretability_attention_heads_count Number of attention heads\n";
    metrics += "# TYPE interpretability_attention_heads_count gauge\n";
    int total_heads = 0;
    for (const auto& [_, heads] : m_attention_heads) {
        total_heads += static_cast<int>(heads.size());
    }
    metrics += QString("interpretability_attention_heads_count %1\n").arg(total_heads);
    
    metrics += "\n# HELP interpretability_gradient_flow_layers Number of layers with gradient data\n";
    metrics += "# TYPE interpretability_gradient_flow_layers gauge\n";
    metrics += QString("interpretability_gradient_flow_layers %1\n").arg(m_gradient_flow_data.size());
    
    metrics += "\n# HELP interpretability_activation_layers Number of layers with activation data\n";
    metrics += "# TYPE interpretability_activation_layers gauge\n";
    metrics += QString("interpretability_activation_layers %1\n").arg(m_activation_stats.size());
    
    if (!m_cached_diagnostics.critical_layer_indices.empty()) {
        metrics += "\n# HELP interpretability_critical_layers Number of critical layers detected\n";
        metrics += "# TYPE interpretability_critical_layers gauge\n";
        metrics += QString("interpretability_critical_layers %1\n")
                       .arg(m_cached_diagnostics.critical_layer_indices.size());
    }
    
    return metrics;
}

QJsonObject InterpretabilityPanelEnhanced::getPerformanceStats() const
{
    QJsonObject stats;
    stats["last_update_duration_ms"] = m_perf_metrics.last_update_duration.count() * 1000;
    stats["last_diagnostics_duration_ms"] = m_perf_metrics.last_diagnostics_duration.count() * 1000;
    stats["total_updates"] = m_perf_metrics.total_updates;
    stats["total_exports"] = m_perf_metrics.total_exports;
    stats["memory_usage_bytes"] = static_cast<int>(m_perf_metrics.current_memory_usage);
    
    return stats;
}

// ========== PRIVATE HELPER METHODS ==========

void InterpretabilityPanelEnhanced::setupUI()
{
    qDebug() << "[InterpretabilityPanelEnhanced] Setting up UI";
    
    QVBoxLayout* main_layout = new QVBoxLayout(this);
    
    // Visualization type selector
    QHBoxLayout* type_layout = new QHBoxLayout();
    QLabel* type_label = new QLabel("Visualization Type:");
    m_viz_type_combo = new QComboBox();
    
    m_viz_type_combo->addItem("Attention Heatmap", static_cast<int>(VisualizationType::AttentionHeatmap));
    m_viz_type_combo->addItem("Feature Importance", static_cast<int>(VisualizationType::FeatureImportance));
    m_viz_type_combo->addItem("Gradient Flow", static_cast<int>(VisualizationType::GradientFlow));
    m_viz_type_combo->addItem("Activation Distribution", static_cast<int>(VisualizationType::ActivationDistribution));
    m_viz_type_combo->addItem("Attention Comparison", static_cast<int>(VisualizationType::AttentionHeadComparison));
    m_viz_type_combo->addItem("GradCAM", static_cast<int>(VisualizationType::GradCAM));
    m_viz_type_combo->addItem("Layer Contribution", static_cast<int>(VisualizationType::LayerContribution));
    
    type_layout->addWidget(type_label);
    type_layout->addWidget(m_viz_type_combo);
    type_layout->addStretch();
    main_layout->addLayout(type_layout);
    
    // Layer selector
    QHBoxLayout* layer_layout = new QHBoxLayout();
    QLabel* layer_label = new QLabel("Layer:");
    m_layer_slider = new QSlider(Qt::Horizontal);
    m_layer_slider->setRange(0, 100);
    m_layer_slider->setValue(0);
    
    layer_layout->addWidget(layer_label);
    layer_layout->addWidget(m_layer_slider);
    main_layout->addLayout(layer_layout);
    
    // Attention head selector
    QHBoxLayout* attention_layout = new QHBoxLayout();
    QLabel* attention_label = new QLabel("Attention Head:");
    m_attention_head_combo = new QComboBox();
    for (int i = 0; i < 12; ++i) {
        m_attention_head_combo->addItem(QString("Head %1").arg(i), i);
    }
    
    attention_layout->addWidget(attention_label);
    attention_layout->addWidget(m_attention_head_combo);
    attention_layout->addStretch();
    main_layout->addLayout(attention_layout);
    
    // Tab widget for visualizations
    m_tab_widget = new QTabWidget();
    m_chart = new QChart();
    m_chart_view = new QChartView(m_chart);
    m_chart_view->setRenderHint(QPainter::Antialiasing);
    m_tab_widget->addTab(m_chart_view, "Chart");
    
    main_layout->addWidget(m_tab_widget);
    
    // Status labels
    m_stats_label = new QLabel("Ready");
    m_problems_label = new QLabel("No issues detected");
    m_diagnostics_label = new QLabel("Diagnostics: idle");
    
    main_layout->addWidget(m_stats_label);
    main_layout->addWidget(m_problems_label);
    main_layout->addWidget(m_diagnostics_label);
    
    setLayout(main_layout);
}

void InterpretabilityPanelEnhanced::updateDisplay()
{
    createCharts();
    
    if (m_stats_label) {
        m_stats_label->setText(QString("Layer: %1 | Head: %2 | Type: %3")
                                   .arg(m_selected_layer)
                                   .arg(m_selected_attention_head)
                                   .arg(static_cast<int>(m_current_visualization)));
    }
}

void InterpretabilityPanelEnhanced::createCharts()
{
    if (!m_chart) return;
    
    m_chart->removeAllSeries();
    // Remove all axes instead of clearing legend
    const auto axes = m_chart->axes();
    for (auto axis : axes) {
        m_chart->removeAxis(axis);
    }
    
    switch (m_current_visualization) {
        case VisualizationType::GradientFlow: {
            // Create line series for gradient norms
            auto series = new QLineSeries();
            series->setName("Gradient Norm");
            
            for (const auto& [layer_idx, metrics] : m_gradient_flow_data) {
                if (layer_idx >= m_min_layer && layer_idx <= m_max_layer) {
                    series->append(layer_idx, metrics.norm);
                }
            }
            
            m_chart->addSeries(series);
            
            // Create axes
            auto x_axis = new QValueAxis();
            x_axis->setTitleText("Layer");
            x_axis->setRange(m_min_layer, m_max_layer);
            m_chart->addAxis(x_axis, Qt::AlignBottom);
            series->attachAxis(x_axis);
            
            auto y_axis = new QValueAxis();
            y_axis->setTitleText("Gradient Norm");
            m_chart->addAxis(y_axis, Qt::AlignLeft);
            series->attachAxis(y_axis);
            
            break;
        }
        
        case VisualizationType::ActivationDistribution: {
            // Create bar series for sparsity
            auto bar_set = new QBarSet("Sparsity");
            
            for (const auto& [layer_idx, stats] : m_activation_stats) {
                if (layer_idx >= m_min_layer && layer_idx <= m_max_layer) {
                    *bar_set << stats.sparsity * 100;
                }
            }
            
            auto bar_series = new QBarSeries();
            bar_series->append(bar_set);
            m_chart->addSeries(bar_series);
            
            // Create axes
            auto x_axis = new QCategoryAxis();
            x_axis->setTitleText("Layer");
            m_chart->addAxis(x_axis, Qt::AlignBottom);
            bar_series->attachAxis(x_axis);
            
            auto y_axis = new QValueAxis();
            y_axis->setTitleText("Sparsity (%)");
            y_axis->setRange(0, 100);
            m_chart->addAxis(y_axis, Qt::AlignLeft);
            bar_series->attachAxis(y_axis);
            
            break;
        }
        
        default:
            break;
    }
}

void InterpretabilityPanelEnhanced::analyzeAttentionPatterns()
{
    for (auto& [layer_idx, heads] : m_attention_heads) {
        for (auto& [head_idx, head] : heads) {
            // Calculate entropy
            head.entropy = calculateEntropy(
                std::vector<float>(head.weights.front().begin(), head.weights.front().end())
            );
        }
    }
}

void InterpretabilityPanelEnhanced::analyzeGradientFlow()
{
    for (auto& [layer_idx, metrics] : m_gradient_flow_data) {
        metrics.is_vanishing = metrics.norm < m_vanishing_threshold;
        metrics.is_exploding = metrics.norm > m_exploding_threshold;
    }
}

void InterpretabilityPanelEnhanced::analyzeActivationPatterns()
{
    // Already processed during update
}

void InterpretabilityPanelEnhanced::detectAnomalies()
{
    runDiagnostics();
}

float InterpretabilityPanelEnhanced::calculateEntropy(const std::vector<float>& distribution)
{
    float entropy = 0.0f;
    float sum = std::accumulate(distribution.begin(), distribution.end(), 0.0f);
    
    if (sum <= 0.0f) return 0.0f;
    
    for (float val : distribution) {
        if (val > 0.0f) {
            float p = val / sum;
            entropy -= p * std::log(p);
        }
    }
    
    return entropy;
}

QJsonObject InterpretabilityPanelEnhanced::visualizationToJSON() const
{
    return exportAsJSON();
}

void InterpretabilityPanelEnhanced::logEvent(const QString& event_type, const QJsonObject& event_data) const
{
    QJsonObject log_entry;
    log_entry["timestamp"] = QString::number(std::chrono::system_clock::now().time_since_epoch().count());
    log_entry["event_type"] = event_type;
    log_entry["data"] = event_data;
    
    qDebug() << "[InterpretabilityPanelEnhanced]"
             << QJsonDocument(log_entry).toJson(QJsonDocument::Compact);
}

void InterpretabilityPanelEnhanced::onRefreshDisplay()
{
    updateDisplay();
}

// ========== NEW EXPORT OVERLOADS FOR MAINWINDOW COMPATIBILITY ==========

/**
 * Export data as JSON file (overload for MainWindow)
 * @param file_path Path to save JSON file
 * @return Success status
 */
bool InterpretabilityPanelEnhanced::exportAsJSON(const QString& file_path)
{
    qDebug() << "[InterpretabilityPanelEnhanced] Exporting JSON to file:" << file_path;
    
    try {
        QJsonObject json_data = exportAsJSON();
        QJsonDocument doc(json_data);
        
        QFile file(file_path);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            qWarning() << "[InterpretabilityPanelEnhanced] Cannot open file for writing:" << file_path;
            return false;
        }
        
        file.write(doc.toJson(QJsonDocument::Indented));
        file.close();
        
        QJsonObject event;
        event["event"] = "json_file_export_completed";
        event["file_path"] = file_path;
        logEvent("export", event);
        
        return true;
    } catch (const std::exception& e) {
        qWarning() << "[InterpretabilityPanelEnhanced] JSON file export failed:"
                   << QString::fromStdString(e.what());
        return false;
    }
}

/**
 * Export data as CSV file (overload for MainWindow - uses current viz type)
 * @param file_path Path to save CSV
 * @return Success status
 */
bool InterpretabilityPanelEnhanced::exportAsCSV(const QString& file_path)
{
    return exportAsCSV(file_path, m_current_visualization);
}
