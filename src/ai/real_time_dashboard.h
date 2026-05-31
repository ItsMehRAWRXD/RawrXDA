// real_time_dashboard.h - Live system dashboard for GPU, KV heat, and arbitration
// Exposes hidden latency and system state in real time
//
// This is what turns the backend into something usable and provable.
//
// Part of the Copilot-like inference pipeline.

#pragma once

#include "final_production_pipeline.h"
#include "stabilization_test_harness.h"
#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace RawrXD {

// Dashboard widget types
enum class DashboardWidget : uint8_t {
    GPU_OCCUPANCY = 0,
    KV_HEAT_MAP = 1,
    ARBITRATION_STATUS = 2,
    LATENCY_TIMELINE = 3,
    PREDICTION_ACCURACY = 4,
    LOOP_STABILITY = 5,
    THERMAL_STATUS = 6,
    MEMORY_USAGE = 7,
    REQUEST_QUEUE = 8,
    MODEL_RESIDENCY = 9,
};

// Widget configuration
struct WidgetConfig {
    DashboardWidget type;
    int x, y, width, height;
    int refresh_rate_ms;
    bool enabled;
    std::string title;
};

// Dashboard data snapshot
struct DashboardSnapshot {
    std::chrono::steady_clock::time_point timestamp;
    
    // GPU state
    struct {
        float utilization;
        float memory_used_gb;
        float memory_total_gb;
        float temperature;
        float clock_speed_mhz;
        int active_kernels;
        int pending_dispatches;
    } gpu;
    
    // KV state
    struct {
        float hot_ratio;
        float warm_ratio;
        float cold_ratio;
        int total_pages;
        int hot_pages;
        int warm_pages;
        int cold_pages;
        float fault_rate;
        float hit_rate;
    } kv;
    
    // Arbitration state
    struct {
        int active_models;
        int queued_requests;
        float fairness_score;
        std::string dominant_model;
        int starved_requests;
    } arbitration;
    
    // Latency state
    struct {
        float current_ms;
        float avg_ms;
        float p50_ms;
        float p99_ms;
        float jitter_ms;
        float drift_percent;
    } latency;
    
    // Prediction state
    struct {
        float accuracy;
        int total_predictions;
        int correct_predictions;
        float confidence;
    } prediction;
    
    // Loop state
    struct {
        float stability_score;
        int resets;
        float degradation_rate;
        bool is_healthy;
    } loop;
    
    // Thermal state
    struct {
        float current_temp;
        float max_temp;
        bool is_throttling;
        float throttle_percent;
    } thermal;
    
    // Memory state
    struct {
        float vram_used_gb;
        float vram_total_gb;
        float ram_used_gb;
        float ram_total_gb;
        float disk_used_gb;
    } memory;
    
    // Request state
    struct {
        int active_requests;
        int queued_requests;
        int completed_requests;
        float throughput_tps;
    } requests;
    
    // Model state
    struct {
        int hot_models;
        int warm_models;
        int cold_models;
        std::vector<std::string> model_names;
    } models;
};

// Dashboard renderer interface
class IDashboardRenderer {
public:
    virtual ~IDashboardRenderer() = default;
    virtual void Render(const DashboardSnapshot& snapshot) = 0;
    virtual void RenderWidget(const DashboardSnapshot& snapshot, DashboardWidget widget) = 0;
    virtual void SetWidgetConfig(const WidgetConfig& config) = 0;
};

// Console renderer (text-based)
class ConsoleDashboardRenderer : public IDashboardRenderer {
public:
    void Render(const DashboardSnapshot& snapshot) override;
    void RenderWidget(const DashboardSnapshot& snapshot, DashboardWidget widget) override;
    void SetWidgetConfig(const WidgetConfig& config) override;
    
private:
    void RenderGPU(const DashboardSnapshot& snapshot);
    void RenderKV(const DashboardSnapshot& snapshot);
    void RenderArbitration(const DashboardSnapshot& snapshot);
    void RenderLatency(const DashboardSnapshot& snapshot);
    void RenderPrediction(const DashboardSnapshot& snapshot);
    void RenderLoop(const DashboardSnapshot& snapshot);
    void RenderThermal(const DashboardSnapshot& snapshot);
    void RenderMemory(const DashboardSnapshot& snapshot);
    void RenderRequests(const DashboardSnapshot& snapshot);
    void RenderModels(const DashboardSnapshot& snapshot);
    
    std::string FormatBar(float value, int width = 20);
    std::string FormatHeat(float heat);
    std::string FormatLatency(float ms);
    std::string FormatHealth(bool healthy);
};

// Real-time dashboard
class RealTimeDashboard {
public:
    RealTimeDashboard(FinalProductionPipeline* pipeline);
    ~RealTimeDashboard();
    
    // Initialize dashboard
    bool Initialize(std::unique_ptr<IDashboardRenderer> renderer);
    
    // Start dashboard
    void Start();
    
    // Stop dashboard
    void Stop();
    
    // Configure widgets
    void ConfigureWidget(const WidgetConfig& config);
    void EnableWidget(DashboardWidget widget);
    void DisableWidget(DashboardWidget widget);
    
    // Get current snapshot
    DashboardSnapshot GetSnapshot() const;
    
    // Get snapshot history
    std::vector<DashboardSnapshot> GetSnapshotHistory(int count) const;
    
    // Set refresh rate
    void SetRefreshRate(std::chrono::milliseconds rate);
    
    // Check if running
    bool IsRunning() const { return running_.load(); }
    
    // Export snapshot to JSON
    std::string ExportSnapshotJSON() const;
    
    // Export history to JSON
    std::string ExportHistoryJSON(int count) const;
    
private:
    // Update thread
    void UpdateThread();
    
    // Collect snapshot from pipeline
    DashboardSnapshot CollectSnapshot();
    
    // Members
    FinalProductionPipeline* pipeline_;
    std::unique_ptr<IDashboardRenderer> renderer_;
    
    // Threading
    std::thread update_thread_;
    std::atomic<bool> running_{false};
    std::atomic<bool> stop_flag_{false};
    
    // Configuration
    std::chrono::milliseconds refresh_rate_{1000};
    std::vector<WidgetConfig> widget_configs_;
    mutable std::mutex config_mutex_;
    
    // Snapshot history
    std::vector<DashboardSnapshot> snapshot_history_;
    mutable std::mutex history_mutex_;
    static constexpr int MAX_HISTORY = 3600;  // 1 hour at 1 sample/sec
    
    // Current snapshot
    mutable std::mutex snapshot_mutex_;
    DashboardSnapshot current_snapshot_;
};

} // namespace RawrXD