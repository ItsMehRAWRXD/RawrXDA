// ============================================================================
// Production Incident Responder — Automated Incident Management
// Detects, analyzes, and remediates production incidents automatically
// ============================================================================
#pragma once
#include "../inference/SovereignInferenceClient.h"
#include "../core/session_manager.h"
#include <memory>
#include <vector>
#include <string>
#include <map>
#include <mutex>
#include <chrono>
#include <functional>

namespace RawrXD::Ops {

enum class IncidentSeverity {
    INFO,
    WARNING,
    ERROR,
    CRITICAL,
    EMERGENCY
};

enum class IncidentStatus {
    DETECTED,
    ANALYZING,
    MITIGATING,
    RESOLVED,
    CLOSED
};

struct IncidentReport {
    std::string id;
    std::string title;
    std::string description;
    IncidentSeverity severity;
    IncidentStatus status;
    std::chrono::system_clock::time_point detectedAt;
    std::chrono::system_clock::time_point resolvedAt;
    std::map<std::string, std::string> metadata;
    std::vector<std::string> affectedServices;
    std::string rootCause;
};

struct IncidentResponse {
    std::string incidentId;
    std::vector<std::string> actionsTaken;
    std::vector<std::string> remediationSteps;
    bool automatedRemediation;
    double confidence;
    std::chrono::system_clock::time_point respondedAt;
};

struct IncidentAnalysis {
    std::string incidentId;
    std::string rootCause;
    std::vector<std::string> contributingFactors;
    std::vector<std::string> lessonsLearned;
    std::map<std::string, std::string> metrics;
    std::chrono::system_clock::time_point analyzedAt;
};

struct Runbook {
    std::string id;
    std::string name;
    std::vector<std::string> steps;
    std::map<std::string, std::string> parameters;
    bool isAutomated;
};

class IncidentResponder {
public:
    explicit IncidentResponder(std::shared_ptr<SovereignInferenceClient> aiClient,
                               std::shared_ptr<RawrXD::Core::SessionManager> sessionManager)
        : m_aiClient(aiClient)
        , m_sessionManager(sessionManager) {}

    IncidentResponse HandleProductionIncident(const IncidentReport& report) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        IncidentResponse response;
        response.incidentId = report.id;
        response.respondedAt = std::chrono::system_clock::now();
        
        // Store incident
        m_incidents[report.id] = report;
        
        // Analyze incident
        auto analysis = AnalyzeIncident(report);
        
        // Determine response strategy
        auto strategy = DetermineResponseStrategy(report, analysis);
        
        // Execute remediation
        if (strategy.automatedRemediation && report.severity >= IncidentSeverity::ERROR) {
            response.actionsTaken = ExecuteAutomatedRemediation(report, analysis);
            response.automatedRemediation = true;
        } else {
            response.remediationSteps = GenerateManualRemediationSteps(report, analysis);
            response.automatedRemediation = false;
        }
        
        response.confidence = analysis.confidence;
        
        // Update incident status
        auto mutableReport = report;
        mutableReport.status = IncidentStatus::MITIGATING;
        m_incidents[report.id] = mutableReport;
        
        return response;
    }

    void AutomateRemediation(const IncidentResponse& response) {
        // Execute automated remediation steps
        for (const auto& action : response.actionsTaken) {
            ExecuteRemediationAction(action);
        }
        
        // Update incident status
        auto it = m_incidents.find(response.incidentId);
        if (it != m_incidents.end()) {
            it->second.status = IncidentStatus::RESOLVED;
            it->second.resolvedAt = std::chrono::system_clock::now();
        }
    }

    void UpdateRunbooks(const IncidentAnalysis& analysis) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        // Check if runbook exists for this type of incident
        std::string runbookKey = analysis.rootCause;
        
        auto it = m_runbooks.find(runbookKey);
        if (it != m_runbooks.end()) {
            // Update existing runbook with new insights
            auto& runbook = it->second;
            for (const auto& lesson : analysis.lessonsLearned) {
                runbook.steps.push_back("// Learned: " + lesson);
            }
        } else {
            // Create new runbook
            Runbook newRunbook;
            newRunbook.id = GenerateRunbookId();
            newRunbook.name = "Auto-generated: " + analysis.rootCause;
            newRunbook.isAutomated = true;
            
            // Generate steps from analysis
            for (const auto& factor : analysis.contributingFactors) {
                newRunbook.steps.push_back("Address: " + factor);
            }
            
            m_runbooks[runbookKey] = newRunbook;
        }
    }

    std::vector<IncidentReport> GetActiveIncidents() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        std::vector<IncidentReport> active;
        for (const auto& [id, incident] : m_incidents) {
            if (incident.status != IncidentStatus::CLOSED && 
                incident.status != IncidentStatus::RESOLVED) {
                active.push_back(incident);
            }
        }
        return active;
    }

    IncidentAnalysis GetIncidentAnalysis(const std::string& incidentId) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        auto it = m_analyses.find(incidentId);
        if (it != m_analyses.end()) {
            return it->second;
        }
        return IncidentAnalysis{};
    }

    void RegisterRemediationAction(const std::string& name, 
                                   std::function<bool(const IncidentReport&)> action) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_remediationActions[name] = action;
    }

private:
    std::shared_ptr<SovereignInferenceClient> m_aiClient;
    std::shared_ptr<RawrXD::Core::SessionManager> m_sessionManager;
    mutable std::mutex m_mutex;
    std::map<std::string, IncidentReport> m_incidents;
    std::map<std::string, IncidentAnalysis> m_analyses;
    std::map<std::string, Runbook> m_runbooks;
    std::map<std::string, std::function<bool(const IncidentReport&)>> m_remediationActions;

    IncidentAnalysis AnalyzeIncident(const IncidentReport& report) {
        IncidentAnalysis analysis;
        analysis.incidentId = report.id;
        analysis.analyzedAt = std::chrono::system_clock::now();
        
        if (!m_aiClient || !m_aiClient->IsLoaded()) {
            analysis.rootCause = "AI analysis unavailable";
            analysis.confidence = 0.5;
            return analysis;
        }

        std::string prompt = "Analyze this production incident:\n" +
                            "Title: " + report.title + "\n" +
                            "Description: " + report.description + "\n" +
                            "Severity: " + std::to_string(static_cast<int>(report.severity)) + "\n" +
                            "Affected Services: ";
        for (const auto& service : report.affectedServices) {
            prompt += service + " ";
        }
        
        std::vector<ChatMessage> messages = {
            {"system", "You are an incident response expert. Analyze incidents and identify root causes."},
            {"user", prompt}
        };

        auto result = m_aiClient->ChatSync(messages);
        
        if (result.success) {
            analysis.rootCause = ParseRootCause(result.response);
            analysis.contributingFactors = ParseContributingFactors(result.response);
            analysis.lessonsLearned = ParseLessonsLearned(result.response);
            analysis.confidence = 0.85;
        }
        
        m_analyses[report.id] = analysis;
        return analysis;
    }

    struct ResponseStrategy {
        bool automatedRemediation;
        std::vector<std::string> recommendedActions;
        double confidence;
    };

    ResponseStrategy DetermineResponseStrategy(const IncidentReport& report,
                                               const IncidentAnalysis& analysis) {
        ResponseStrategy strategy;
        
        // Determine if we can automate based on severity and confidence
        if (report.severity >= IncidentSeverity::CRITICAL) {
            strategy.automatedRemediation = false; // Require human approval for critical
            strategy.confidence = 0.7;
        } else if (analysis.confidence > 0.8) {
            strategy.automatedRemediation = true;
            strategy.confidence = analysis.confidence;
        } else {
            strategy.automatedRemediation = false;
            strategy.confidence = analysis.confidence;
        }
        
        // Get recommended actions from runbook if available
        auto it = m_runbooks.find(analysis.rootCause);
        if (it != m_runbooks.end()) {
            strategy.recommendedActions = it->second.steps;
        } else {
            // Generate generic remediation steps
            strategy.recommendedActions = {
                "Isolate affected services",
                "Check system logs",
                "Verify database connectivity",
                "Restart services if needed"
            };
        }
        
        return strategy;
    }

    std::vector<std::string> ExecuteAutomatedRemediation(const IncidentReport& report,
                                                         const IncidentAnalysis& analysis) {
        std::vector<std::string> actionsTaken;
        
        for (const auto& [name, action] : m_remediationActions) {
            try {
                if (action(report)) {
                    actionsTaken.push_back("Executed: " + name);
                }
            } catch (const std::exception& e) {
                actionsTaken.push_back("Failed: " + name + " - " + e.what());
            }
        }
        
        return actionsTaken;
    }

    std::vector<std::string> GenerateManualRemediationSteps(const IncidentReport& report,
                                                             const IncidentAnalysis& analysis) {
        std::vector<std::string> steps;
        
        steps.push_back("1. Review incident details in dashboard");
        steps.push_back("2. Verify root cause: " + analysis.rootCause);
        
        for (size_t i = 0; i < analysis.contributingFactors.size(); ++i) {
            steps.push_back(std::to_string(i + 3) + ". Address: " + analysis.contributingFactors[i]);
        }
        
        steps.push_back("Final. Update incident status and document resolution");
        
        return steps;
    }

    void ExecuteRemediationAction(const std::string& action) {
        // Execute the remediation action
        // This would integrate with your deployment/orchestration system
    }

    std::string ParseRootCause(const std::string& response) {
        // Parse AI response to extract root cause
        return "Parsed root cause from AI analysis";
    }

    std::vector<std::string> ParseContributingFactors(const std::string& response) {
        // Parse AI response to extract contributing factors
        return {"Factor 1", "Factor 2"};
    }

    std::vector<std::string> ParseLessonsLearned(const std::string& response) {
        // Parse AI response to extract lessons learned
        return {"Lesson 1", "Lesson 2"};
    }

    std::string GenerateRunbookId() {
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        std::ostringstream oss;
        oss << "runbook_" << std::put_time(std::localtime(&time), "%Y%m%d_%H%M%S");
        return oss.str();
    }
};

} // namespace RawrXD::Ops
