// ============================================================================
// Dashboard Controller — Unified Feature Dashboard
// Central dashboard for monitoring all production features
// ============================================================================
#pragma once
#include "feature_integration.cpp"
#include "../editor/minimap_renderer.h"
#include "../editor/theme_engine.h"
#include <memory>
#include <vector>
#include <string>
#include <map>
#include <mutex>
#include <chrono>

namespace RawrXD::UI {

enum class DashboardWidget {
    QUALITY_METRICS,
    PERFORMANCE_CHART,
    INCIDENT_FEED,
    RESOURCE_USAGE,
    COMPLIANCE_STATUS,
    MAINTENANCE_SCHEDULE,
    NOTIFICATION_CENTER,
    ACTIVITY_LOG
};

struct DashboardLayout {
    std::map<DashboardWidget, std::pair<int, int>> positions; // row, col
    std::map<DashboardWidget, std::pair<int, int>> sizes;    // width, height
    std::string theme;
    bool autoRefresh;
    int refreshInterval;
};

struct DashboardData {
    std::map<DashboardWidget, std::string> widgetData;
    std::chrono::system_clock::time_point lastUpdated;
    bool isStale;
};

struct Alert {
    std::string id;
    std::string title;
    std::string message;
    std::string severity;
    std::chrono::system_clock::time_point timestamp;
    bool isAcknowledged;
    std::string sourceWidget;
};

class DashboardController {
public:
    DashboardController(std::shared_ptr<FeatureUIController> featureController)
        : m_featureController(featureController)
        , m_autoRefreshEnabled(true)
        , m_refreshInterval(5000) { // 5 seconds
        InitializeDashboard();
    }

    void InitializeDashboard() {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        // Default layout
        m_layout.positions[DashboardWidget::QUALITY_METRICS] = {0, 0};
        m_layout.positions[DashboardWidget::PERFORMANCE_CHART] = {0, 1};
        m_layout.positions[DashboardWidget::INCIDENT_FEED] = {1, 0};
        m_layout.positions[DashboardWidget::RESOURCE_USAGE] = {1, 1};
        m_layout.positions[DashboardWidget::COMPLIANCE_STATUS] = {2, 0};
        m_layout.positions[DashboardWidget::MAINTENANCE_SCHEDULE] = {2, 1};
        
        m_layout.sizes[DashboardWidget::QUALITY_METRICS] = {1, 1};
        m_layout.sizes[DashboardWidget::PERFORMANCE_CHART] = {1, 1};
        m_layout.sizes[DashboardWidget::INCIDENT_FEED] = {1, 1};
        m_layout.sizes[DashboardWidget::RESOURCE_USAGE] = {1, 1};
        m_layout.sizes[DashboardWidget::COMPLIANCE_STATUS] = {1, 1};
        m_layout.sizes[DashboardWidget::MAINTENANCE_SCHEDULE] = {1, 1};
        
        m_layout.theme = "dark";
        m_layout.autoRefresh = true;
        m_layout.refreshInterval = 5000;
    }

    void RefreshDashboard() {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        // Update all widget data
        UpdateQualityMetrics();
        UpdatePerformanceChart();
        UpdateIncidentFeed();
        UpdateResourceUsage();
        UpdateComplianceStatus();
        UpdateMaintenanceSchedule();
        
        m_data.lastUpdated = std::chrono::system_clock::now();
        m_data.isStale = false;
    }

    void StartAutoRefresh() {
        m_autoRefreshEnabled = true;
        
        // Start refresh thread
        m_refreshThread = std::thread([this]() {
            while (m_autoRefreshEnabled) {
                RefreshDashboard();
                std::this_thread::sleep_for(std::chrono::milliseconds(m_refreshInterval));
            }
        });
    }

    void StopAutoRefresh() {
        m_autoRefreshEnabled = false;
        if (m_refreshThread.joinable()) {
            m_refreshThread.join();
        }
    }

    std::string GetWidgetData(DashboardWidget widget) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        auto it = m_data.widgetData.find(widget);
        if (it != m_data.widgetData.end()) {
            return it->second;
        }
        return "";
    }

    void SetWidgetPosition(DashboardWidget widget, int row, int col) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_layout.positions[widget] = {row, col};
    }

    void SetWidgetSize(DashboardWidget widget, int width, int height) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_layout.sizes[widget] = {width, height};
    }

    std::vector<Alert> GetActiveAlerts() {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        std::vector<Alert> active;
        for (const auto& alert : m_alerts) {
            if (!alert.isAcknowledged) {
                active.push_back(alert);
            }
        }
        return active;
    }

    void AcknowledgeAlert(const std::string& alertId) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        for (auto& alert : m_alerts) {
            if (alert.id == alertId) {
                alert.isAcknowledged = true;
                return;
            }
        }
    }

    void ExportDashboard(const std::string& format, const std::string& outputPath) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        if (format == "html") {
            ExportToHTML(outputPath);
        } else if (format == "pdf") {
            ExportToPDF(outputPath);
        } else if (format == "json") {
            ExportToJSON(outputPath);
        }
    }

    std::string GenerateDashboardReport() {
        std::ostringstream report;
        report << "# Dashboard Report\n\n";
        report << "**Generated:** " << FormatTime(std::chrono::system_clock::now()) << "\n\n";
        
        report << "## Quality Metrics\n";
        report << GetWidgetData(DashboardWidget::QUALITY_METRICS) << "\n\n";
        
        report << "## Performance\n";
        report << GetWidgetData(DashboardWidget::PERFORMANCE_CHART) << "\n\n";
        
        report << "## Active Incidents\n";
        report << GetWidgetData(DashboardWidget::INCIDENT_FEED) << "\n\n";
        
        report << "## Resource Usage\n";
        report << GetWidgetData(DashboardWidget::RESOURCE_USAGE) << "\n\n";
        
        report << "## Compliance Status\n";
        report << GetWidgetData(DashboardWidget::COMPLIANCE_STATUS) << "\n\n";
        
        auto alerts = GetActiveAlerts();
        if (!alerts.empty()) {
            report << "## Active Alerts\n";
            for (const auto& alert : alerts) {
                report << "- [" << alert.severity << "] " << alert.title << ": " 
                       << alert.message << "\n";
            }
        }
        
        return report.str();
    }

private:
    std::shared_ptr<FeatureUIController> m_featureController;
    mutable std::mutex m_mutex;
    DashboardLayout m_layout;
    DashboardData m_data;
    std::vector<Alert> m_alerts;
    bool m_autoRefreshEnabled;
    int m_refreshInterval;
    std::thread m_refreshThread;

    void UpdateQualityMetrics() {
        // Get quality data from feature controller
        // This would populate the quality widget
        m_data.widgetData[DashboardWidget::QUALITY_METRICS] = "Quality metrics updated";
    }

    void UpdatePerformanceChart() {
        // Get performance data
        m_data.widgetData[DashboardWidget::PERFORMANCE_CHART] = "Performance data updated";
    }

    void UpdateIncidentFeed() {
        // Get incident data
        m_data.widgetData[DashboardWidget::INCIDENT_FEED] = "Incident feed updated";
    }

    void UpdateResourceUsage() {
        // Get resource data
        m_data.widgetData[DashboardWidget::RESOURCE_USAGE] = "Resource usage updated";
    }

    void UpdateComplianceStatus() {
        // Get compliance data
        m_data.widgetData[DashboardWidget::COMPLIANCE_STATUS] = "Compliance status updated";
    }

    void UpdateMaintenanceSchedule() {
        // Get maintenance data
        m_data.widgetData[DashboardWidget::MAINTENANCE_SCHEDULE] = "Maintenance schedule updated";
    }

    void ExportToHTML(const std::string& outputPath) {
        std::ofstream file(outputPath);
        if (!file.is_open()) return;
        
        file << "<!DOCTYPE html>\n";
        file << "<html>\n";
        file << "<head>\n";
        file << "<title>RawrXD Dashboard</title>\n";
        file << "</head>\n";
        file << "<body>\n";
        file << "<h1>Dashboard</h1>\n";
        file << "<pre>" << GenerateDashboardReport() << "</pre>\n";
        file << "</body>\n";
        file << "</html>\n";
    }

    void ExportToPDF(const std::string& outputPath) {
        // PDF export implementation
    }

    void ExportToJSON(const std::string& outputPath) {
        std::ofstream file(outputPath);
        if (!file.is_open()) return;
        
        file << "{\n";
        file << "  \"dashboard\": {\n";
        file << "    \"lastUpdated\": \"" << FormatTime(m_data.lastUpdated) << "\",\n";
        file << "    \"widgets\": {\n";
        
        bool first = true;
        for (const auto& [widget, data] : m_data.widgetData) {
            if (!first) file << ",\n";
            first = false;
            file << "      \"" << static_cast<int>(widget) << "\": \"" << data << "\"";
        }
        
        file << "\n    }\n";
        file << "  }\n";
        file << "}\n";
    }

    std::string FormatTime(std::chrono::system_clock::time_point time) {
        auto timeT = std::chrono::system_clock::to_time_t(time);
        std::ostringstream oss;
        oss << std::put_time(std::localtime(&timeT), "%Y-%m-%d %H:%M:%S");
        return oss.str();
    }
};

} // namespace RawrXD::UI
