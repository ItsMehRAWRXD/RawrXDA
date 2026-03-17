#pragma once

#include "gpu_memory_pool.h"
#include "gpu_async_operations.h"
#include "gpu_ray_tracing.h"
#include "gpu_tensor_ops.h"
#include "gpu_load_balancer.h"
#include "gpu_clock_tuning.h"

#include <QWidget>
#include <QMainWindow>
#include <QString>
#include <memory>
#include <map>

namespace RawrXD {
namespace GPU {

// GPU Acceleration Service - provides unified access to all GPU features
class GPUAccelerationService {
public:
    static GPUAccelerationService& instance();

    // Initialize all GPU services
    bool initialize(uint32_t device_count = 1);

    // Shutdown all GPU services
    void shutdown();

    // ========== Memory Management ==========
    GPUMemoryPool* get_memory_pool(uint32_t device_id) {
        return GPUMemoryManager::instance().get_pool(device_id);
    }

    UnifiedMemoryPool* get_unified_memory_pool(uint32_t device_id) {
        return GPUMemoryManager::instance().get_unified_pool(device_id);
    }

    // ========== Async Operations ==========
    std::shared_ptr<GPUCommandStream> create_command_stream(uint32_t device_id) {
        return AsyncGPUExecutor::instance().create_compute_stream(device_id);
    }

    // ========== Ray Tracing ==========
    AdvancedRayTracer& get_ray_tracer() {
        return AdvancedRayTracer::instance();
    }

    // ========== Tensor Operations ==========
    GPUTensorOps& get_tensor_ops() {
        return GPUTensorOps::instance();
    }

    // ========== Load Balancing ==========
    MultiGPULoadBalancer& get_load_balancer() {
        return MultiGPULoadBalancer::instance();
    }

    GPUTaskScheduler& get_task_scheduler() {
        return GPUTaskScheduler::instance();
    }

    // ========== Clock Tuning ==========
    GPUClockGovernor& get_clock_governor() {
        return GPUClockGovernor::instance();
    }

    PowerManagementController& get_power_manager() {
        return PowerManagementController::instance();
    }

    ThermalManagementController& get_thermal_manager() {
        return ThermalManagementController::instance();
    }

    // ========== Statistics and Monitoring ==========
    struct GPUSystemStats {
        uint32_t device_count = 0;
        uint64_t total_memory_bytes = 0;
        uint64_t used_memory_bytes = 0;
        float total_utilization = 0.0f;
        float average_power_watts = 0.0f;
        float average_temperature_celsius = 0.0f;
        uint64_t total_operations = 0;
        uint64_t total_tensor_ops = 0;
        uint64_t total_ray_tracing_ops = 0;
    };

    GPUSystemStats get_system_statistics();

    // Enable/disable monitoring
    void enable_monitoring(bool enable);

    // Is system initialized
    bool is_initialized() const { return initialized_; }

private:
    GPUAccelerationService() = default;
    ~GPUAccelerationService() = default;

    bool initialized_ = false;
    uint32_t device_count_ = 0;

    std::map<uint32_t, VkDevice> vk_devices_;

    void monitor_system();
};

// ============================================================================
// Qt UI Integration
// ============================================================================

// GPU Status Dashboard Widget
class GPUStatusDashboardWidget : public QWidget {
    Q_OBJECT

public:
    explicit GPUStatusDashboardWidget(QWidget* parent = nullptr);
    ~GPUStatusDashboardWidget();

    void update_display();

protected:
    void paintEvent(QPaintEvent* event) override;

signals:
    void clock_profile_changed(int profile);
    void power_limit_changed(float watts);

private:
    void setup_ui();
    void create_status_displays();

    struct GPUStatusDisplay {
        uint32_t device_id = 0;
        QString device_name;
        float utilization = 0.0f;
        float temperature = 0.0f;
        float power_usage = 0.0f;
        float memory_usage = 0.0f;
    };

    std::vector<GPUStatusDisplay> gpu_displays_;
};

// GPU Performance Monitor Widget
class GPUPerformanceMonitorWidget : public QWidget {
    Q_OBJECT

public:
    explicit GPUPerformanceMonitorWidget(QWidget* parent = nullptr);
    ~GPUPerformanceMonitorWidget();

    void update_metrics();

signals:
    void performance_warning(const QString& message);

private:
    void setup_ui();
    void update_charts();

    struct PerformanceMetric {
        std::vector<float> gpu_utilization;
        std::vector<float> memory_bandwidth;
        std::vector<float> tensor_flops;
        std::vector<float> power_draw;
    };

    PerformanceMetric metrics_;
};

// GPU Clock Control Widget
class GPUClockControlWidget : public QWidget {
    Q_OBJECT

public:
    explicit GPUClockControlWidget(QWidget* parent = nullptr);
    ~GPUClockControlWidget();

    void refresh_state();

signals:
    void profile_selected(int profile);
    void custom_clock_set(uint32_t core_mhz, uint32_t memory_mhz);
    void power_limit_set(float watts);

private slots:
    void on_profile_changed(int index);
    void on_core_clock_changed(int value);
    void on_memory_clock_changed(int value);
    void on_power_limit_changed(int value);

private:
    void setup_ui();
    void update_ranges();
};

// GPU Memory Analysis Widget
class GPUMemoryAnalysisWidget : public QWidget {
    Q_OBJECT

public:
    explicit GPUMemoryAnalysisWidget(QWidget* parent = nullptr);
    ~GPUMemoryAnalysisWidget();

    void update_memory_info();

signals:
    void memory_warning(const QString& message);

private:
    void setup_ui();
    void visualize_memory_layout();
};

// GPU Task Monitor Widget
class GPUTaskMonitorWidget : public QWidget {
    Q_OBJECT

public:
    explicit GPUTaskMonitorWidget(QWidget* parent = nullptr);
    ~GPUTaskMonitorWidget();

    void update_tasks();

signals:
    void task_cancelled(uint32_t task_id);

private:
    void setup_ui();
    void refresh_task_list();

    struct TaskDisplay {
        uint32_t task_id = 0;
        QString name;
        QString status;
        int progress = 0;
    };

    std::vector<TaskDisplay> active_tasks_;
};

// ============================================================================
// Main GPU Control Panel (integrated into IDE)
// ============================================================================

class GPUControlPanel : public QWidget {
    Q_OBJECT

public:
    explicit GPUControlPanel(QWidget* parent = nullptr);
    ~GPUControlPanel();

    void initialize();
    void shutdown();

signals:
    void gpu_error(const QString& error);
    void gpu_warning(const QString& warning);
    void gpu_status_changed(const QString& status);

private:
    void setup_ui();
    void connect_signals();
    void start_monitoring();
    void stop_monitoring();

    std::unique_ptr<GPUStatusDashboardWidget> status_dashboard_;
    std::unique_ptr<GPUPerformanceMonitorWidget> performance_monitor_;
    std::unique_ptr<GPUClockControlWidget> clock_control_;
    std::unique_ptr<GPUMemoryAnalysisWidget> memory_analysis_;
    std::unique_ptr<GPUTaskMonitorWidget> task_monitor_;

    bool is_running_ = false;
};

} // namespace GPU
} // namespace RawrXD
