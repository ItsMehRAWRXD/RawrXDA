// Win32IDE_FailureIntelligence_Handler.cpp
// Command handler: FailureIntelligence orchestrator integration into Win32IDE
// Enables: failure reporting, autonomous analysis, recovery planning and execution, pattern learning

#include "../win32app/Win32IDE.h"
#include "../agentic/failure_intelligence_orchestrator.hpp"
#include <sstream>
#include <memory>

// ============================================================================
// Static orchestrator instance (singleton per IDE session)
// ============================================================================

static std::unique_ptr<Agentic::FailureIntelligenceOrchestrator> g_failureIntelligence;

// ============================================================================
// Helper: Ensure FailureIntelligence is initialized
// ============================================================================

static void ensureFailureIntelligenceInitialized(Win32IDE* ide) {
    if (!g_failureIntelligence) {
        g_failureIntelligence = std::make_unique<Agentic::FailureIntelligenceOrchestrator>();

        // Wire output callback
        g_failureIntelligence->setAnalysisLogFn([ide](const std::string& log_entry) {
            if (ide) {
                ide->appendToOutput("[FailureIntelligence] " + log_entry + "\n",
                                  "Output", Win32IDE::OutputSeverity::Info);
            }
        });

        // Wire failure detection callback
        g_failureIntelligence->setFailureDetectedCallback(
            [ide](const Agentic::FailureSignal& signal) {
                if (ide) {
                    std::ostringstream ss;
                    ss << "🔴 Failure detected in " << signal.source_component
                       << " (step: " << signal.step_id << ") - "
                       << signal.error_message;
                    ide->appendToOutput(ss.str() + "\n", "Output",
                                      Win32IDE::OutputSeverity::Warning);
                    ide->setStatusBarText("Failure detected: " + signal.source_component);
                }
            });

        // Wire recovery initiation callback
        g_failureIntelligence->setRecoveryInitiatedCallback(
            [ide](const Agentic::RecoveryPlan& plan) {
                if (ide) {
                    std::ostringstream ss;
                    ss << "🔧 Recovery initiated: " << plan.strategy_description;
                    ide->appendToOutput(ss.str() + "\n", "Output",
                                      Win32IDE::OutputSeverity::Info);
                }
            });

        // Wire recovery completion callback
        g_failureIntelligence->setRecoveryCompletedCallback(
            [ide](const Agentic::RecoveryPlan& plan, bool success) {
                if (ide) {
                    std::ostringstream ss;
                    ss << (success ? "✅" : "❌") << " Recovery " 
                       << (success ? "succeeded" : "failed") << ": " << plan.recovery_id;
                    ide->appendToOutput(ss.str() + "\n", "Output",
                                      success ? Win32IDE::OutputSeverity::Info : 
                                              Win32IDE::OutputSeverity::Error);
                    ide->setStatusBarText("Recovery " + std::string(success ? "succeeded" : "failed"));
                }
            });
    }
}

// ============================================================================
// Command Handlers (IDM_FAILURE_*)
// ============================================================================

bool Win32IDE::handleFailureIntelligenceCommand(int commandId) {
    LOG_INFO("handleFailureIntelligenceCommand: " + std::to_string(commandId));
    
    ensureFailureIntelligenceInitialized(this);
    if (!g_failureIntelligence) {
        appendToOutput("❌ FailureIntelligence not initialized\n", "Output",
                      OutputSeverity::Error);
        return false;
    }

    switch (commandId) {
    case IDM_FAILURE_DETECT:
    {
        // Report a failure (typically called from subprocess/tool exit handler)
        char failureDesc[1024] = {0};
        if (DialogBoxParamA(m_hInstance, "AGENT_PROMPT_DLG", m_hwndMain,
            [](HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) -> INT_PTR {
                switch (msg) {
                case WM_INITDIALOG:
                    SetWindowTextA(GetDlgItem(hwnd, 101), "Report failure: error message");
                    return TRUE;
                case WM_COMMAND:
                    if (LOWORD(wp) == IDOK) {
                        GetDlgItemTextA(hwnd, 102, (char*)lp, 1024);
                        EndDialog(hwnd, IDOK);
                        return TRUE;
                    } else if (LOWORD(wp) == IDCANCEL) {
                        EndDialog(hwnd, IDCANCEL);
                        return TRUE;
                    }
                    break;
                }
                return FALSE;
            }, (LPARAM)failureDesc) == IDOK && strlen(failureDesc) > 0) {

            Agentic::FailureSignal signal;
            signal.error_message = failureDesc;
            signal.source_component = "manual_report";
            signal.severity = Agentic::SeverityLevel::Warning;
            g_failureIntelligence->reportFailure(signal);
            appendToOutput("Failure reported: " + std::string(failureDesc) + "\n",
                          "Output", OutputSeverity::Info);
        }
        return true;
    }

    case IDM_FAILURE_ANALYZE:
    {
        // Analyze most recent failure
        auto recent = g_failureIntelligence->getRecentFailures(1);
        if (recent.empty()) {
            appendToOutput("No recent failures to analyze\n", "Output",
                          OutputSeverity::Warning);
            return true;
        }

        auto rca = g_failureIntelligence->analyzeFailure(*recent[0]);
        if (rca) {
            appendToOutput("=== Root Cause Analysis ===\n"
                          "Category: " + std::to_string(static_cast<int>(rca->primary_category)) + "\n" +
                          "Confidence: " + std::to_string(static_cast<int>(rca->analysis_confidence * 100)) + "%\n" +
                          "Root Cause: " + rca->root_cause_description + "\n",
                          "Output", OutputSeverity::Info);
        }
        return true;
    }

    case IDM_FAILURE_SHOW_QUEUE:
    {
        // Display pending failures
        auto failures = g_failureIntelligence->getRecentFailures(10);
        if (failures.empty()) {
            appendToOutput("No failures in queue\n", "Output", OutputSeverity::Info);
            return true;
        }

        appendToOutput("=== Recent Failures ===\n", "Output", OutputSeverity::Info);
        for (size_t i = 0; i < failures.size(); ++i) {
            appendToOutput(std::to_string(i + 1) + ". " + failures[i]->signal_id +
                          " - " + failures[i]->error_message.substr(0, 50) + "...\n",
                          "Output", OutputSeverity::Info);
        }
        return true;
    }

    case IDM_FAILURE_SHOW_HISTORY:
    {
        // Export failure history
        auto json = g_failureIntelligence->getFailureQueueJson();
        appendToOutput("=== Failure History ===\n" + json.dump(2) + "\n",
                      "Output", OutputSeverity::Info);
        return true;
    }

    case IDM_FAILURE_GENERATE_RECOVERY:
    {
        // Generate recovery plan for recent failure
        auto recent = g_failureIntelligence->getRecentFailures(1);
        if (recent.empty()) {
            appendToOutput("No recent failures\n", "Output", OutputSeverity::Warning);
            return true;
        }

        auto plan = g_failureIntelligence->generateRecoveryPlan(*recent[0]);
        if (plan) {
            appendToOutput("=== Recovery Plan ===\n"
                          "ID: " + plan->recovery_id + "\n" +
                          "Strategy: " + plan->strategy_description + "\n" +
                          "Steps: " + std::to_string(plan->recovery_steps.size()) + "\n",
                          "Output", OutputSeverity::Info);
        }
        return true;
    }

    case IDM_FAILURE_EXECUTE_RECOVERY:
    {
        // Execute recovery plan
        auto pending = g_failureIntelligence->getPendingRecoveries();
        if (pending.empty()) {
            appendToOutput("No pending recovery plans\n", "Output", OutputSeverity::Warning);
            return true;
        }

        std::string output;
        g_failureIntelligence->executeRecovery(pending[0], output);
        appendToOutput("Recovery executed\n" + output + "\n", "Output", OutputSeverity::Info);
        return true;
    }

    case IDM_FAILURE_AUTONOMOUS_HEAL:
    {
        // Full autonomous recovery: detect → analyze → plan → execute
        auto recent = g_failureIntelligence->getRecentFailures(1);
        if (recent.empty()) {
            appendToOutput("No failures to heal\n", "Output", OutputSeverity::Warning);
            return true;
        }

        appendToOutput("🔄 Starting autonomous recovery...\n", "Output",
                      OutputSeverity::Info);

        std::string output;
        bool success = g_failureIntelligence->autonomousRecover(*recent[0], output);

        if (success) {
            appendToOutput("✅ Autonomous recovery SUCCEEDED\n" + output + "\n",
                          "Output", OutputSeverity::Info);
        } else {
            appendToOutput("⚠️ Autonomous recovery requires escalation\n" + output + "\n",
                          "Output", OutputSeverity::Warning);
        }
        return true;
    }

    case IDM_FAILURE_VIEW_PATTERNS:
    {
        // Show learned failure patterns
        auto stats = g_failureIntelligence->getFailureStatistics();
        appendToOutput("=== Failure Patterns ===\n"
                      "Total failures: " + std::to_string(stats.total_failures_seen) + "\n" +
                      "Categories: " + std::to_string(stats.category_counts.size()) + "\n",
                      "Output", OutputSeverity::Info);
        return true;
    }

    case IDM_FAILURE_LEARN_PATTERN:
    {
        // Learn from actual categorized failure (for model improvement)
        auto recent = g_failureIntelligence->getRecentFailures(1);
        if (recent.empty()) {
            appendToOutput("No recent failures to learn from\n", "Output",
                          OutputSeverity::Warning);
            return true;
        }

        // For demo: mark as Transient
        g_failureIntelligence->learnFromFailure(*recent[0], 
                                               Agentic::FailureCategory::Transient);
        appendToOutput("Pattern learned\n", "Output", OutputSeverity::Info);
        return true;
    }

    case IDM_FAILURE_STATS:
    {
        // Display comprehensive statistics
        auto stats = g_failureIntelligence->getStatisticsJson();
        appendToOutput("=== FailureIntelligence Statistics ===\n" + stats.dump(2) + "\n",
                      "Output", OutputSeverity::Info);
        return true;
    }

    case IDM_FAILURE_SET_POLICY:
    {
        // Configure recovery policies
        char policyOpt[64] = {0};
        if (DialogBoxParamA(m_hInstance, "AGENT_PROMPT_DLG", m_hwndMain,
            [](HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) -> INT_PTR {
                switch (msg) {
                case WM_INITDIALOG:
                    SetWindowTextA(GetDlgItem(hwnd, 101),
                                 "Policy (1=Conservative, 2=Standard, 3=Aggressive):");
                    return TRUE;
                case WM_COMMAND:
                    if (LOWORD(wp) == IDOK) {
                        GetDlgItemTextA(hwnd, 102, (char*)lp, 64);
                        EndDialog(hwnd, IDOK);
                        return TRUE;
                    }
                    break;
                }
                return FALSE;
            }, (LPARAM)policyOpt) == IDOK && strlen(policyOpt) > 0) {
            
            int choice = std::stoi(policyOpt);
            switch (choice) {
            case 1:
                g_failureIntelligence->setAutoRetryThreshold(
                    Agentic::SeverityLevel::Warning);
                appendToOutput("Policy set to Conservative\n", "Output",
                              OutputSeverity::Info);
                break;
            case 2:
                g_failureIntelligence->setAutoRetryThreshold(
                    Agentic::SeverityLevel::Error);
                appendToOutput("Policy set to Standard\n", "Output",
                              OutputSeverity::Info);
                break;
            case 3:
                g_failureIntelligence->setAutoRetryThreshold(
                    Agentic::SeverityLevel::Critical);
                appendToOutput("Policy set to Aggressive\n", "Output",
                              OutputSeverity::Info);
                break;
            }
        }
        return true;
    }

    case IDM_FAILURE_SHOW_HEALTH:
    {
        // Display system health assessment
        auto health = g_failureIntelligence->getSystemHealthJson();
        appendToOutput("=== System Health ===\n" + health.dump(2) + "\n",
                      "Output", OutputSeverity::Info);
        return true;
    }

    case IDM_FAILURE_EXPORT_ANALYSIS:
    {
        // Export full analysis to JSON file
        std::string exportPath = m_currentDirectory.empty() ? "." : m_currentDirectory;
        exportPath += "\\failure_analysis_export.json";

        auto json = g_failureIntelligence->getFailureQueueJson();
        std::ofstream out(exportPath);
        if (out.is_open()) {
            out << json.dump(2);
            out.close();
            appendToOutput("✅ Analysis exported to: " + exportPath + "\n",
                          "Output", OutputSeverity::Info);
        } else {
            appendToOutput("❌ Failed to export analysis\n", "Output",
                          OutputSeverity::Error);
        }
        return true;
    }

    case IDM_FAILURE_CLEAR_HISTORY:
    {
        // Clear failure history (with confirmation)
        if (MessageBoxA(m_hwndMain, "Clear all failure history?", "Confirm",
                       MB_YESNO | MB_ICONQUESTION) == IDYES) {
            // Create new instance (fresh start)
            g_failureIntelligence = std::make_unique<
                Agentic::FailureIntelligenceOrchestrator>();
            appendToOutput("🗑️ Failure history cleared\n", "Output",
                          OutputSeverity::Info);
        }
        return true;
    }

    case IDM_FAILURE_DIAGNOSTICS:
    {
        // Full system diagnostics
        std::ostringstream diag;
        diag << "=== FailureIntelligence Diagnostics ===\n";
        diag << "System initialized: " << (g_failureIntelligence ? "✅" : "❌") << "\n";
        
        auto stats = g_failureIntelligence->getFailureStatistics();
        diag << "Total failures: " << stats.total_failures_seen << "\n";
        diag << "Recovery attempts: " << stats.total_recoveries_attempted << "\n";
        diag << "Recovery successes: " << stats.total_recoveries_succeeded << "\n";
        
        if (stats.total_recoveries_attempted > 0) {
            diag << "Success rate: " 
                 << static_cast<int>(stats.overall_recovery_success_rate * 100) << "%\n";
        }

        appendToOutput(diag.str(), "Output", OutputSeverity::Info);
        return true;
    }

    default:
        return false;
    }
}
