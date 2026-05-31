// real_time_dashboard.cpp - Implementation of live system dashboard
// Part of the Copilot-like inference pipeline.

#include "real_time_dashboard.h"
#include <iomanip>
#include <sstream>

namespace RawrXD {

// Console renderer implementation
void ConsoleDashboardRenderer::Render(const DashboardSnapshot& snapshot) {
    std::cout << "\033[2J\033[H";  // Clear screen
    std::cout << "=== RawrXD Real-Time Dashboard ===\n";
    std::cout << "Timestamp: " << std::chrono::duration_cast<std::chrono::seconds>(
        snapshot.timestamp.time_since_epoch()).count() << "\n\n";
    
    RenderGPU(snapshot);
    RenderKV(snapshot);
    RenderArbitration(snapshot);
    RenderLatency(snapshot);
    RenderPrediction(snapshot);
    RenderLoop(snapshot);
    RenderThermal(snapshot);
    RenderMemory(snapshot);
    RenderRequests(snapshot);
    RenderModels(snapshot);
    
    std::cout << "\nPress Ctrl+C to exit\n";
    std::cout.flush();
}

void ConsoleDashboardRenderer::RenderWidget(const DashboardSnapshot& snapshot, 
                                            DashboardWidget widget) {
    switch (widget) {
        case DashboardWidget::GPU_OCCUPANCY:
            RenderGPU(snapshot);
            break;
        case DashboardWidget::KV_HEAT_MAP:
            RenderKV(snapshot);
            break;
        case DashboardWidget::ARBITRATION_STATUS:
            RenderArbitration(snapshot);
            break;
        case DashboardWidget::LATENCY_TIMELINE:
            RenderLatency(snapshot);
            break;
        case DashboardWidget::PREDICTION_ACCURACY:
            RenderPrediction(snapshot);
            break;
        case DashboardWidget::LOOP_STABILITY:
            RenderLoop(snapshot);
            break;
        case DashboardWidget::THERMAL_STATUS:
            RenderThermal(snapshot);
            break;
        case DashboardWidget::MEMORY_USAGE:
            RenderMemory(snapshot);
            break;
        case DashboardWidget::REQUEST_QUEUE:
            RenderRequests(snapshot);
            break;
        case DashboardWidget::MODEL_RESIDENCY:
            RenderModels(snapshot);
            break;
    }
}

void ConsoleDashboardRenderer::SetWidgetConfig(const WidgetConfig& config) {
    // Store config for rendering
}

void ConsoleDashboardRenderer::RenderGPU(const DashboardSnapshot& snapshot) {
    std::cout << "[GPU]\n";
    std::cout << "  Utilization: " << FormatBar(snapshot.gpu.utilization) << " "
              << std::fixed << std::setprecision(1) << (snapshot.gpu.utilization * 100.0f) << "%\n";
    std::cout << "  Memory: " << snapshot.gpu.memory_used_gb << " / " 
              << snapshot.gpu.memory_total_gb << " GB\n";
    std::cout << "  Temperature: " << snapshot.gpu.temperature << "°C\n";
    std::cout << "  Clock: " << snapshot.gpu.clock_speed_mhz << " MHz\n";
    std::cout << "  Active Kernels: " << snapshot.gpu.active_kernels << "\n";
    std::cout << "  Pending Dispatches: " << snapshot.gpu.pending_dispatches << "\n\n";
}

void ConsoleDashboardRenderer::RenderKV(const DashboardSnapshot& snapshot) {
    std::cout << "[KV Cache]\n";
    std::cout << "  Hot: " << FormatBar(snapshot.kv.hot_ratio) << " "
              << snapshot.kv.hot_pages << " pages\n";
    std::cout << "  Warm: " << FormatBar(snapshot.kv.warm_ratio) << " "
              << snapshot.kv.warm_pages << " pages\n";
    std::cout << "  Cold: " << FormatBar(snapshot.kv.cold_ratio) << " "
              << snapshot.kv.cold_pages << " pages\n";
    std::cout << "  Total: " << snapshot.kv.total_pages << " pages\n";
    std::cout << "  Fault Rate: " << std::fixed << std::setprecision(2) 
              << (snapshot.kv.fault_rate * 100.0f) << "%\n";
    std::cout << "  Hit Rate: " << std::fixed << std::setprecision(2)
              << (snapshot.kv.hit_rate * 100.0f) << "%\n\n";
}

void ConsoleDashboardRenderer::RenderArbitration(const DashboardSnapshot& snapshot) {
    std::cout << "[Arbitration]\n";
    std::cout << "  Active Models: " << snapshot.arbitration.active_models << "\n";
    std::cout << "  Queued Requests: " << snapshot.arbitration.queued_requests << "\n";
    std::cout << "  Fairness: " << FormatBar(snapshot.arbitration.fairness_score) << " "
              << std::fixed << std::setprecision(1) << (snapshot.arbitration.fairness_score * 100.0f) << "%\n";
    std::cout << "  Dominant Model: " << snapshot.arbitration.dominant_model << "\n";
    std::cout << "  Starved: " << snapshot.arbitration.starved_requests << "\n\n";
}

void ConsoleDashboardRenderer::RenderLatency(const DashboardSnapshot& snapshot) {
    std::cout << "[Latency]\n";
    std::cout << "  Current: " << FormatLatency(snapshot.latency.current_ms) << "\n";
    std::cout << "  Average: " << FormatLatency(snapshot.latency.avg_ms) << "\n";
    std::cout << "  P50: " << FormatLatency(snapshot.latency.p50_ms) << "\n";
    std::cout << "  P99: " << FormatLatency(snapshot.latency.p99_ms) << "\n";
    std::cout << "  Jitter: " << snapshot.latency.jitter_ms << " ms\n";
    std::cout << "  Drift: " << std::fixed << std::setprecision(2)
              << snapshot.latency.drift_percent << "%\n\n";
}

void ConsoleDashboardRenderer::RenderPrediction(const DashboardSnapshot& snapshot) {
    std::cout << "[Prediction]\n";
    std::cout << "  Accuracy: " << FormatBar(snapshot.prediction.accuracy) << " "
              << std::fixed << std::setprecision(1) << (snapshot.prediction.accuracy * 100.0f) << "%\n";
    std::cout << "  Total: " << snapshot.prediction.total_predictions << "\n";
    std::cout << "  Correct: " << snapshot.prediction.correct_predictions << "\n";
    std::cout << "  Confidence: " << std::fixed << std::setprecision(2)
              << snapshot.prediction.confidence << "\n\n";
}

void ConsoleDashboardRenderer::RenderLoop(const DashboardSnapshot& snapshot) {
    std::cout << "[Loop Stability]\n";
    std::cout << "  Score: " << FormatBar(snapshot.loop.stability_score) << " "
              << std::fixed << std::setprecision(1) << (snapshot.loop.stability_score * 100.0f) << "%\n";
    std::cout << "  Resets: " << snapshot.loop.resets << "\n";
    std::cout << "  Degradation: " << std::fixed << std::setprecision(4)
              << snapshot.loop.degradation_rate << "\n";
    std::cout << "  Health: " << FormatHealth(snapshot.loop.is_healthy) << "\n\n";
}

void ConsoleDashboardRenderer::RenderThermal(const DashboardSnapshot& snapshot) {
    std::cout << "[Thermal]\n";
    std::cout << "  Current: " << snapshot.thermal.current_temp << "°C\n";
    std::cout << "  Max: " << snapshot.thermal.max_temp << "°C\n";
    std::cout << "  Throttling: " << (snapshot.thermal.is_throttling ? "YES" : "NO") << "\n";
    std::cout << "  Throttle: " << std::fixed << std::setprecision(1)
              << (snapshot.thermal.throttle_percent * 100.0f) << "%\n\n";
}

void ConsoleDashboardRenderer::RenderMemory(const DashboardSnapshot& snapshot) {
    std::cout << "[Memory]\n";
    std::cout << "  VRAM: " << snapshot.memory.vram_used_gb << " / "
              << snapshot.memory.vram_total_gb << " GB\n";
    std::cout << "  RAM: " << snapshot.memory.ram_used_gb << " / "
              << snapshot.memory.ram_total_gb << " GB\n";
    std::cout << "  Disk: " << snapshot.memory.disk_used_gb << " GB\n\n";
}

void ConsoleDashboardRenderer::RenderRequests(const DashboardSnapshot& snapshot) {
    std::cout << "[Requests]\n";
    std::cout << "  Active: " << snapshot.requests.active_requests << "\n";
    std::cout << "  Queued: " << snapshot.requests.queued_requests << "\n";
    std::cout << "  Completed: " << snapshot.requests.completed_requests << "\n";
    std::cout << "  Throughput: " << std::fixed << std::setprecision(1)
              << snapshot.requests.throughput_tps << " TPS\n\n";
}

void ConsoleDashboardRenderer::RenderModels(const DashboardSnapshot& snapshot) {
    std::cout << "[Models]\n";
    std::cout << "  Hot: " << snapshot.models.hot_models << "\n";
    std::cout << "  Warm: " << snapshot.models.warm_models << "\n";
    std::cout << "  Cold: " << snapshot.models.cold_models << "\n";
    std::cout << "  Loaded: ";
    for (const auto& name : snapshot.models.model_names) {
        std::cout << name << " ";
    }
    std::cout << "\n\n";
}

std::string ConsoleDashboardRenderer::FormatBar(float value, int width) {
    int filled = static_cast<int>(value * width);
    filled = std::max(0, std::min(width, filled));
    
    std::string bar = "[";
    for (int i = 0; i < width; i++) {
        if (i < filled) {
            bar += "█";
        } else {
            bar += "░";
        }
    }
    bar += "]";
    
    return bar;
}

std::string ConsoleDashboardRenderer::FormatHeat(float heat) {
    if (heat > 0.7f) return "🔥";
    if (heat > 0.3f) return "🌡️";
    return "❄️";
}

std::string ConsoleDashboardRenderer::FormatLatency(float ms) {
    if (ms < 50.0f) return std::to_string(ms) + " ms ✅";
    if (ms < 100.0f) return std::to_string(ms) + " ms ⚠️";
    return std::to_string(ms) + " ms ❌";
}

std::string ConsoleDashboardRenderer::FormatHealth(bool healthy) {
    return healthy ? "✅ HEALTHY" : "❌ DEGRADED";
}

// Dashboard implementation
RealTimeDashboard::RealTimeDashboard(FinalProductionPipeline* pipeline)
    : pipeline_(pipeline)
{
    current_snapshot_ = {};
}

RealTimeDashboard::~RealTimeDashboard() {
    Stop();
}

bool RealTimeDashboard::Initialize(std::unique_ptr<IDashboardRenderer> renderer) {
    renderer_ = std::move(renderer);
    return true;
}

void RealTimeDashboard::Start() {
    if (running_.load()) {
        return;
    }
    
    running_.store(true);
    stop_flag_.store(false);
    
    update_thread_ = std::thread(&RealTimeDashboard::UpdateThread, this);
}

void RealTimeDashboard::Stop() {
    if (!running_.load()) {
        return;
    }
    
    stop_flag_.store(true);
    running_.store(false);
    
    if (update_thread_.joinable()) {
        update_thread_.join();
    }
}

void RealTimeDashboard::ConfigureWidget(const WidgetConfig& config) {
    std::lock_guard<std::mutex> lock(config_mutex_);
    
    // Find existing config
    auto it = std::find_if(widget_configs_.begin(), widget_configs_.end(),
        [&config](const WidgetConfig& c) {
            return c.type == config.type;
        });
    
    if (it != widget_configs_.end()) {
        *it = config;
    } else {
        widget_configs_.push_back(config);
    }
}

void RealTimeDashboard::EnableWidget(DashboardWidget widget) {
    std::lock_guard<std::mutex> lock(config_mutex_);
    
    auto it = std::find_if(widget_configs_.begin(), widget_configs_.end(),
        [widget](const WidgetConfig& c) {
            return c.type == widget;
        });
    
    if (it != widget_configs_.end()) {
        it->enabled = true;
    }
}

void RealTimeDashboard::DisableWidget(DashboardWidget widget) {
    std::lock_guard<std::mutex> lock(config_mutex_);
    
    auto it = std::find_if(widget_configs_.begin(), widget_configs_.end(),
        [widget](const WidgetConfig& c) {
            return c.type == widget;
        });
    
    if (it != widget_configs_.end()) {
        it->enabled = false;
    }
}

DashboardSnapshot RealTimeDashboard::GetSnapshot() const {
    std::lock_guard<std::mutex> lock(snapshot_mutex_);
    return current_snapshot_;
}

std::vector<DashboardSnapshot> RealTimeDashboard::GetSnapshotHistory(int count) const {
    std::lock_guard<std::mutex> lock(history_mutex_);
    
    int start = std::max(0, static_cast<int>(snapshot_history_.size()) - count);
    return std::vector<DashboardSnapshot>(
        snapshot_history_.begin() + start,
        snapshot_history_.end());
}

void RealTimeDashboard::SetRefreshRate(std::chrono::milliseconds rate) {
    refresh_rate_ = rate;
}

std::string RealTimeDashboard::ExportSnapshotJSON() const {
    auto snapshot = GetSnapshot();
    
    std::ostringstream json;
    json << "{\n";
    json << "  \"timestamp\": " << std::chrono::duration_cast<std::chrono::seconds>(
        snapshot.timestamp.time_since_epoch()).count() << ",\n";
    json << "  \"gpu\": {\n";
    json << "    \"utilization\": " << snapshot.gpu.utilization << ",\n";
    json << "    \"memory_used_gb\": " << snapshot.gpu.memory_used_gb << ",\n";
    json << "    \"temperature\": " << snapshot.gpu.temperature << "\n";
    json << "  },\n";
    json << "  \"latency\": {\n";
    json << "    \"current_ms\": " << snapshot.latency.current_ms << ",\n";
    json << "    \"p50_ms\": " << snapshot.latency.p50_ms << ",\n";
    json << "    \"p99_ms\": " << snapshot.latency.p99_ms << "\n";
    json << "  },\n";
    json << "  \"loop\": {\n";
    json << "    \"stability_score\": " << snapshot.loop.stability_score << ",\n";
    json << "    \"is_healthy\": " << (snapshot.loop.is_healthy ? "true" : "false") << "\n";
    json << "  }\n";
    json << "}";
    
    return json.str();
}

std::string RealTimeDashboard::ExportHistoryJSON(int count) const {
    auto history = GetSnapshotHistory(count);
    
    std::ostringstream json;
    json << "[\n";
    
    for (size_t i = 0; i < history.size(); i++) {
        if (i > 0) json << ",\n";
        
        json << "  {\n";
        json << "    \"timestamp\": " << std::chrono::duration_cast<std::chrono::seconds>(
            history[i].timestamp.time_since_epoch()).count() << ",\n";
        json << "    \"latency_ms\": " << history[i].latency.current_ms << ",\n";
        json << "    \"gpu_utilization\": " << history[i].gpu.utilization << ",\n";
        json << "    \"kv_hit_rate\": " << history[i].kv.hit_rate << ",\n";
        json << "    \"prediction_accuracy\": " << history[i].prediction.accuracy << ",\n";
        json << "    \"loop_stability\": " << history[i].loop.stability_score << "\n";
        json << "  }";
    }
    
    json << "\n]";
    
    return json.str();
}

void RealTimeDashboard::UpdateThread() {
    while (!stop_flag_.load()) {
        // Collect snapshot
        auto snapshot = CollectSnapshot();
        
        // Store snapshot
        {
            std::lock_guard<std::mutex> lock(snapshot_mutex_);
            current_snapshot_ = snapshot;
        }
        
        // Store in history
        {
            std::lock_guard<std::mutex> lock(history_mutex_);
            snapshot_history_.push_back(snapshot);
            
            if (snapshot_history_.size() > MAX_HISTORY) {
                snapshot_history_.erase(snapshot_history_.begin());
            }
        }
        
        // Render
        if (renderer_) {
            renderer_->Render(snapshot);
        }
        
        // Wait for next refresh
        std::this_thread::sleep_for(refresh_rate_);
    }
}

DashboardSnapshot RealTimeDashboard::CollectSnapshot() {
    DashboardSnapshot snapshot;
    snapshot.timestamp = std::chrono::steady_clock::now();
    
    // TODO: Collect actual data from pipeline
    // For now, generate realistic test data
    
    // GPU
    snapshot.gpu.utilization = 0.75f + static_cast<float>(rand() % 25) / 100.0f;
    snapshot.gpu.memory_used_gb = 12.0f + static_cast<float>(rand() % 40) / 10.0f;
    snapshot.gpu.memory_total_gb = 24.0f;
    snapshot.gpu.temperature = 65.0f + static_cast<float>(rand() % 20);
    snapshot.gpu.clock_speed_mhz = 2000.0f + static_cast<float>(rand() % 500);
    snapshot.gpu.active_kernels = 3 + rand() % 5;
    snapshot.gpu.pending_dispatches = rand() % 10;
    
    // KV
    snapshot.kv.hot_ratio = 0.3f + static_cast<float>(rand() % 20) / 100.0f;
    snapshot.kv.warm_ratio = 0.4f + static_cast<float>(rand() % 20) / 100.0f;
    snapshot.kv.cold_ratio = 1.0f - snapshot.kv.hot_ratio - snapshot.kv.warm_ratio;
    snapshot.kv.total_pages = 1000 + rand() % 500;
    snapshot.kv.hot_pages = static_cast<int>(snapshot.kv.total_pages * snapshot.kv.hot_ratio);
    snapshot.kv.warm_pages = static_cast<int>(snapshot.kv.total_pages * snapshot.kv.warm_ratio);
    snapshot.kv.cold_pages = snapshot.kv.total_pages - snapshot.kv.hot_pages - snapshot.kv.warm_pages;
    snapshot.kv.fault_rate = static_cast<float>(rand() % 10) / 100.0f;
    snapshot.kv.hit_rate = 1.0f - snapshot.kv.fault_rate;
    
    // Arbitration
    snapshot.arbitration.active_models = 2 + rand() % 3;
    snapshot.arbitration.queued_requests = rand() % 20;
    snapshot.arbitration.fairness_score = 0.8f + static_cast<float>(rand() % 20) / 100.0f;
    snapshot.arbitration.dominant_model = "codestral:22b";
    snapshot.arbitration.starved_requests = rand() % 5;
    
    // Latency
    snapshot.latency.current_ms = 30.0f + static_cast<float>(rand() % 40);
    snapshot.latency.avg_ms = 35.0f + static_cast<float>(rand() % 20);
    snapshot.latency.p50_ms = snapshot.latency.avg_ms;
    snapshot.latency.p99_ms = snapshot.latency.avg_ms + static_cast<float>(rand() % 50);
    snapshot.latency.jitter_ms = static_cast<float>(rand() % 15);
    snapshot.latency.drift_percent = static_cast<float>(rand() % 20) / 10.0f - 1.0f;
    
    // Prediction
    snapshot.prediction.accuracy = 0.7f + static_cast<float>(rand() % 25) / 100.0f;
    snapshot.prediction.total_predictions = 1000 + rand() % 500;
    snapshot.prediction.correct_predictions = static_cast<int>(
        snapshot.prediction.total_predictions * snapshot.prediction.accuracy);
    snapshot.prediction.confidence = 0.6f + static_cast<float>(rand() % 30) / 100.0f;
    
    // Loop
    snapshot.loop.stability_score = 0.85f + static_cast<float>(rand() % 15) / 100.0f;
    snapshot.loop.resets = rand() % 5;
    snapshot.loop.degradation_rate = static_cast<float>(rand() % 10) / 1000.0f;
    snapshot.loop.is_healthy = snapshot.loop.stability_score > 0.8f;
    
    // Thermal
    snapshot.thermal.current_temp = 60.0f + static_cast<float>(rand() % 25);
    snapshot.thermal.max_temp = 85.0f;
    snapshot.thermal.is_throttling = snapshot.thermal.current_temp > 80.0f;
    snapshot.thermal.throttle_percent = snapshot.thermal.is_throttling ? 0.1f : 0.0f;
    
    // Memory
    snapshot.memory.vram_used_gb = snapshot.gpu.memory_used_gb;
    snapshot.memory.vram_total_gb = snapshot.gpu.memory_total_gb;
    snapshot.memory.ram_used_gb = 8.0f + static_cast<float>(rand() % 40) / 10.0f;
    snapshot.memory.ram_total_gb = 32.0f;
    snapshot.memory.disk_used_gb = 2.0f + static_cast<float>(rand() % 10);
    
    // Requests
    snapshot.requests.active_requests = 2 + rand() % 5;
    snapshot.requests.queued_requests = rand() % 10;
    snapshot.requests.completed_requests = 10000 + rand() % 5000;
    snapshot.requests.throughput_tps = 15.0f + static_cast<float>(rand() % 15);
    
    // Models
    snapshot.models.hot_models = 1 + rand() % 2;
    snapshot.models.warm_models = 1 + rand() % 2;
    snapshot.models.cold_models = 2 + rand() % 3;
    snapshot.models.model_names = {"codestral:22b", "qwen3.5-40b", "phi3mini"};
    
    return snapshot;
}

} // namespace RawrXD