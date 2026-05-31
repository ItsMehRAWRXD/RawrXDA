// ============================================================================
// UI Integration Layer — Feature Integration Components
// Connects production features to Batch 3 UI components
// ============================================================================
#pragma once
#include "../src/quality/code_quality_monitor.cpp"
#include "../src/testing/ai_test_generator.cpp"
#include "../src/ops/incident_responder.cpp"
#include "../src/i18n/multilingual_engine.cpp"
#include "../src/memory/advanced_analyzer.cpp"
#include "../src/scale/predictive_scaler.cpp"
#include "../src/logs/ai_log_analyzer.cpp"
#include "../src/docs/auto_documenter.cpp"
#include "../src/performance/performance_advisor.cpp"
#include "../src/security/compliance_checker.cpp"
#include "../src/data/migration_engine.cpp"
#include "../src/recovery/auto_recovery.cpp"
#include "../src/maintenance/predictive_maintenance.cpp"
#include "../src/resources/allocator.cpp"
#include "../src/compatibility/cross_platform.cpp"
#include "../editor/status_bar.cpp"
#include "../editor/notification_manager.cpp"
#include "../editor/minimap_renderer.cpp"
#include "../core/session_manager.h"
#include <memory>
#include <functional>
#include <map>

namespace RawrXD::UI {

// ============================================================================
// Feature UI Controller — Central Integration Point
// ============================================================================
class FeatureUIController {
public:
    FeatureUIController(
        std::shared_ptr<SovereignInferenceClient> aiClient,
        std::shared_ptr<Core::SessionManager> sessionManager,
        std::shared_ptr<Editor::StatusBar> statusBar,
        std::shared_ptr<Editor::NotificationManager> notificationManager)
        : m_aiClient(aiClient)
        , m_sessionManager(sessionManager)
        , m_statusBar(statusBar)
        , m_notificationManager(notificationManager) {
        InitializeFeatures();
    }

    // Quality Panel Integration
    void ShowQualityPanel(const std::string& filePath) {
        if (!m_qualityMonitor) return;
        
        auto content = ReadFileContent(filePath);
        auto metrics = m_qualityMonitor->AnalyzeCodeQuality(content);
        
        // Update status bar with quality score
        if (m_statusBar) {
            m_statusBar->SetQualityScore(metrics.overallScore);
        }
        
        // Show notification for low quality
        if (metrics.overallScore < 6.0 && m_notificationManager) {
            m_notificationManager->ShowNotification(
                "Code Quality",
                "Low quality score detected: " + std::to_string(static_cast<int>(metrics.overallScore)) + "/10",
                NotificationType::WARNING
            );
        }
    }

    // Test Generation Panel
    void ShowTestGenerationPanel(const std::string& code, const std::string& spec) {
        if (!m_testGenerator) return;
        
        auto suite = m_testGenerator->GenerateTests(code, spec);
        
        // Store in session
        m_sessionManager->SetValue("last_test_suite", 
            m_testGenerator->ExportTestSuite(suite, "cpp"));
        
        // Show notification
        if (m_notificationManager) {
            m_notificationManager->ShowNotification(
                "Test Generation",
                "Generated " + std::to_string(suite.testCases.size()) + " test cases",
                NotificationType::INFO
            );
        }
    }

    // Incident Dashboard
    void ShowIncidentDashboard() {
        if (!m_incidentResponder) return;
        
        auto active = m_incidentResponder->GetActiveIncidents();
        
        // Update status bar
        if (m_statusBar) {
            m_statusBar->SetActiveIncidents(active.size());
        }
        
        // Show critical notifications
        for (const auto& incident : active) {
            if (incident.severity >= Ops::IncidentSeverity::CRITICAL && m_notificationManager) {
                m_notificationManager->ShowNotification(
                    "Critical Incident",
                    incident.title,
                    NotificationType::ERROR
                );
            }
        }
    }

    // Memory Analysis Panel
    void ShowMemoryPanel(int pid) {
        if (!m_memoryAnalyzer) return;
        
        Memory::ProcessInfo process;
        process.pid = pid;
        process.name = "Current Process";
        
        auto profile = m_memoryAnalyzer->AnalyzeMemoryUsage(process);
        
        // Show notification for leaks
        if (!profile.leakCandidates.empty() && m_notificationManager) {
            m_notificationManager->ShowNotification(
                "Memory Analysis",
                std::to_string(profile.leakCandidates.size()) + " potential leaks detected",
                NotificationType::WARNING
            );
        }
    }

    // Performance Dashboard
    void ShowPerformanceDashboard() {
        if (!m_performanceAdvisor) return;
        
        Performance::PerformanceData data;
        data.component = "IDE";
        data.cpuUsage = GetCurrentCPUUsage();
        data.memoryUsage = GetCurrentMemoryUsage();
        
        auto suggestions = m_performanceAdvisor->AnalyzePerformance(data);
        
        // Update status bar
        if (m_statusBar) {
            m_statusBar->SetPerformanceMetrics(data.cpuUsage, data.memoryUsage);
        }
    }

    // Compliance Report Panel
    void ShowCompliancePanel(const std::string& code, Security::ComplianceStandard standard) {
        if (!m_complianceChecker) return;
        
        auto report = m_complianceChecker->CheckCompliance(code, standard);
        
        // Show notification
        if (m_notificationManager) {
            std::string status = report.complianceScore >= 80.0 ? "PASSED" : "FAILED";
            NotificationType type = report.complianceScore >= 80.0 ? NotificationType::SUCCESS : NotificationType::WARNING;
            
            m_notificationManager->ShowNotification(
                "Compliance Check",
                status + " - Score: " + std::to_string(static_cast<int>(report.complianceScore)) + "%",
                type
            );
        }
    }

    // Language Selection Menu
    void ShowLanguageMenu() {
        if (!m_multilingualEngine) return;
        
        auto languages = m_multilingualEngine->GetSupportedLanguages();
        // This would populate a dropdown menu in the UI
    }

    // Documentation Preview
    void ShowDocumentationPreview(const std::string& code) {
        if (!m_autoDocumenter) return;
        
        Docs::DocSpec spec;
        spec.format = Docs::DocFormat::MARKDOWN;
        spec.includeExamples = true;
        
        auto docs = m_autoDocumenter->GenerateDocs(code, spec);
        
        // Store in session for preview
        m_sessionManager->SetValue("generated_documentation", docs.content);
    }

    // Cross-Platform Check
    void ShowCompatibilityReport(const std::string& code) {
        if (!m_crossPlatformEngine) return;
        
        auto platforms = m_crossPlatformEngine->GetSupportedPlatforms();
        auto report = m_crossPlatformEngine->CheckCompatibility(code, platforms);
        
        // Show notification
        if (m_notificationManager) {
            std::string status = report.overallCompatibility >= 80.0 ? "Compatible" : "Issues Found";
            NotificationType type = report.overallCompatibility >= 80.0 ? NotificationType::SUCCESS : NotificationType::WARNING;
            
            m_notificationManager->ShowNotification(
                "Cross-Platform Check",
                status + " - " + std::to_string(static_cast<int>(report.overallCompatibility)) + "% compatible",
                type
            );
        }
    }

    // Log Analysis Panel
    void ShowLogAnalysisPanel(const std::vector<Logs::LogEntry>& entries) {
        if (!m_logAnalyzer) return;
        
        auto insights = m_logAnalyzer->AnalyzeLogs(entries);
        
        // Show anomalies
        if (!insights.anomalies.empty() && m_notificationManager) {
            m_notificationManager->ShowNotification(
                "Log Analysis",
                std::to_string(insights.anomalies.size()) + " anomalies detected",
                NotificationType::WARNING
            );
        }
    }

    // Maintenance Schedule Panel
    void ShowMaintenanceSchedule() {
        if (!m_predictiveMaintenance) return;
        
        auto health = m_predictiveMaintenance->GetCurrentSystemHealth();
        auto schedule = m_predictiveMaintenance->PredictMaintenanceNeeds(health);
        
        // Show upcoming maintenance
        if (!schedule.tasks.empty() && m_notificationManager) {
            m_notificationManager->ShowNotification(
                "Maintenance Schedule",
                std::to_string(schedule.tasks.size()) + " tasks scheduled",
                NotificationType::INFO
            );
        }
    }

    // Resource Allocation Dashboard
    void ShowResourceDashboard() {
        if (!m_resourceAllocator) return;
        
        auto utilization = m_resourceAllocator->GetCurrentUtilization();
        auto capacity = m_resourceAllocator->GetAvailableCapacity();
        
        // Update status bar
        if (m_statusBar && !utilization.currentUtilization.empty()) {
            auto it = utilization.currentUtilization.begin();
            if (it != utilization.currentUtilization.end()) {
                m_statusBar->SetResourceUsage(it->second);
            }
        }
    }

    // Recovery Status Panel
    void ShowRecoveryStatus() {
        if (!m_autoRecovery) return;
        
        auto history = m_autoRecovery->GetRecoveryHistory(5);
        
        // Show recent recoveries
        if (!history.empty() && m_notificationManager) {
            int successCount = 0;
            for (const auto& result : history) {
                if (result.success) successCount++;
            }
            
            m_notificationManager->ShowNotification(
                "Recovery Status",
                std::to_string(successCount) + "/" + std::to_string(history.size()) + " recent recoveries successful",
                NotificationType::INFO
            );
        }
    }

    // Migration Status Panel
    void ShowMigrationStatus() {
        if (!m_migrationEngine) return;
        
        auto history = m_migrationEngine->GetMigrationHistory();
        
        // Show migration count
        if (m_statusBar) {
            m_statusBar->SetMigrationCount(history.size());
        }
    }

    // Scaling Dashboard
    void ShowScalingDashboard() {
        if (!m_predictiveScaler) return;
        
        auto history = m_predictiveScaler->GetScalingHistory(5);
        
        // Update status bar
        if (m_statusBar) {
            m_statusBar->SetScalingHistory(history.size());
        }
    }

private:
    std::shared_ptr<SovereignInferenceClient> m_aiClient;
    std::shared_ptr<Core::SessionManager> m_sessionManager;
    std::shared_ptr<Editor::StatusBar> m_statusBar;
    std::shared_ptr<Editor::NotificationManager> m_notificationManager;
    
    // Feature instances
    std::unique_ptr<Quality::CodeQualityMonitor> m_qualityMonitor;
    std::unique_ptr<Testing::AITestGenerator> m_testGenerator;
    std::unique_ptr<Ops::IncidentResponder> m_incidentResponder;
    std::unique_ptr<I18n::MultiLingualEngine> m_multilingualEngine;
    std::unique_ptr<Memory::AdvancedMemoryAnalyzer> m_memoryAnalyzer;
    std::unique_ptr<Scale::PredictiveScaler> m_predictiveScaler;
    std::unique_ptr<Logs::AILogAnalyzer> m_logAnalyzer;
    std::unique_ptr<Docs::AutoDocumenter> m_autoDocumenter;
    std::unique_ptr<Performance::PerformanceAdvisor> m_performanceAdvisor;
    std::unique_ptr<Security::ComplianceChecker> m_complianceChecker;
    std::unique_ptr<Data::MigrationEngine> m_migrationEngine;
    std::unique_ptr<Recovery::AutoRecovery> m_autoRecovery;
    std::unique_ptr<Maintenance::PredictiveMaintenance> m_predictiveMaintenance;
    std::unique_ptr<Resources::IntelligentAllocator> m_resourceAllocator;
    std::unique_ptr<Compatibility::CrossPlatformEngine> m_crossPlatformEngine;

    void InitializeFeatures() {
        m_qualityMonitor = std::make_unique<Quality::CodeQualityMonitor>(m_aiClient);
        m_testGenerator = std::make_unique<Testing::AITestGenerator>(m_aiClient);
        
        auto deploymentOrchestrator = std::make_shared<Ops::DeploymentOrchestrator>();
        m_incidentResponder = std::make_unique<Ops::IncidentResponder>(m_aiClient, m_sessionManager);
        
        m_multilingualEngine = std::make_unique<I18n::MultiLingualEngine>(m_aiClient);
        m_memoryAnalyzer = std::make_unique<Memory::AdvancedMemoryAnalyzer>(m_aiClient);
        m_predictiveScaler = std::make_unique<Scale::PredictiveScaler>(m_aiClient);
        m_logAnalyzer = std::make_unique<Logs::AILogAnalyzer>(m_aiClient);
        m_autoDocumenter = std::make_unique<Docs::AutoDocumenter>(m_aiClient);
        m_performanceAdvisor = std::make_unique<Performance::PerformanceAdvisor>(m_aiClient);
        m_complianceChecker = std::make_unique<Security::ComplianceChecker>(m_aiClient);
        
        auto buildSystem = std::make_shared<Build::BuildSystem>();
        m_migrationEngine = std::make_unique<Data::MigrationEngine>(m_aiClient, m_sessionManager);
        m_autoRecovery = std::make_unique<Recovery::AutoRecovery>(m_aiClient, m_sessionManager);
        m_predictiveMaintenance = std::make_unique<Maintenance::PredictiveMaintenance>(m_aiClient);
        m_resourceAllocator = std::make_unique<Resources::IntelligentAllocator>(m_aiClient);
        m_crossPlatformEngine = std::make_unique<Compatibility::CrossPlatformEngine>(m_aiClient);
    }

    std::string ReadFileContent(const std::string& path) {
        // Implementation would read file
        return "";
    }

    double GetCurrentCPUUsage() {
        // Implementation would get CPU usage
        return 50.0;
    }

    double GetCurrentMemoryUsage() {
        // Implementation would get memory usage
        return 60.0;
    }
};

// ============================================================================
// Menu Integration — Adds feature menu items
// ============================================================================
class FeatureMenuIntegration {
public:
    static void RegisterMenuItems(FeatureUIController& controller) {
        // Quality Menu
        RegisterMenuItem("Quality", "Analyze Code Quality", [&controller]() {
            controller.ShowQualityPanel("/current/file.cpp");
        });
        
        // Testing Menu
        RegisterMenuItem("Testing", "Generate Tests", [&controller]() {
            controller.ShowTestGenerationPanel("// code", "spec");
        });
        
        // Operations Menu
        RegisterMenuItem("Operations", "Incident Dashboard", [&controller]() {
            controller.ShowIncidentDashboard();
        });
        
        // Analysis Menu
        RegisterMenuItem("Analysis", "Memory Analysis", [&controller]() {
            controller.ShowMemoryPanel(0);
        });
        
        RegisterMenuItem("Analysis", "Performance Dashboard", [&controller]() {
            controller.ShowPerformanceDashboard();
        });
        
        // Security Menu
        RegisterMenuItem("Security", "Compliance Check", [&controller]() {
            controller.ShowCompliancePanel("code", Security::ComplianceStandard::OWASP_TOP_10);
        });
        
        // Tools Menu
        RegisterMenuItem("Tools", "Language Settings", [&controller]() {
            controller.ShowLanguageMenu();
        });
        
        RegisterMenuItem("Tools", "Generate Documentation", [&controller]() {
            controller.ShowDocumentationPreview("code");
        });
        
        RegisterMenuItem("Tools", "Cross-Platform Check", [&controller]() {
            controller.ShowCompatibilityReport("code");
        });
        
        // View Menu
        RegisterMenuItem("View", "Log Analysis", [&controller]() {
            controller.ShowLogAnalysisPanel({});
        });
        
        RegisterMenuItem("View", "Maintenance Schedule", [&controller]() {
            controller.ShowMaintenanceSchedule();
        });
        
        RegisterMenuItem("View", "Resource Dashboard", [&controller]() {
            controller.ShowResourceDashboard();
        });
        
        RegisterMenuItem("View", "Recovery Status", [&controller]() {
            controller.ShowRecoveryStatus();
        });
        
        RegisterMenuItem("View", "Migration Status", [&controller]() {
            controller.ShowMigrationStatus();
        });
        
        RegisterMenuItem("View", "Scaling Dashboard", [&controller]() {
            controller.ShowScalingDashboard();
        });
    }

private:
    static void RegisterMenuItem(const std::string& menu, const std::string& item, 
                                std::function<void()> callback) {
        // This would integrate with your menu system
        // For now, just store the callback
    }
};

} // namespace RawrXD::UI
