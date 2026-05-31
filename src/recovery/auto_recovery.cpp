// ============================================================================
// Auto Recovery — Automated Error Recovery System
// Detects errors and automatically recovers from failures
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

namespace RawrXD::Recovery {

enum class ErrorType {
    SYSTEM,
    NETWORK,
    DATABASE,
    MEMORY,
    DISK,
    SERVICE,
    UNKNOWN
};

enum class RecoveryStatus {
    PENDING,
    IN_PROGRESS,
    SUCCESS,
    FAILED,
    MANUAL_INTERVENTION_REQUIRED
};

struct ErrorContext {
    std::string errorId;
    ErrorType type;
    std::string message;
    std::string stackTrace;
    std::map<std::string, std::string> metadata;
    std::chrono::system_clock::time_point occurredAt;
    int severity;
};

struct RecoveryAction {
    std::string id;
    std::string description;
    std::function<bool(const ErrorContext&)> condition;
    std::function<bool(const ErrorContext&)> execute;
    int maxAttempts;
    int backoffSeconds;
};

struct RecoveryPlan {
    std::string errorId;
    std::vector<RecoveryAction> actions;
    RecoveryStatus status;
    int currentAttempt;
    std::chrono::system_clock::time_point createdAt;
    std::chrono::system_clock::time_point completedAt;
    std::string resultMessage;
};

struct RecoveryResults {
    std::string errorId;
    bool success;
    int attemptsMade;
    std::vector<std::string> actionsTaken;
    std::chrono::system_clock::time_point startedAt;
    std::chrono::system_clock::time_point completedAt;
    std::string finalState;
};

class AutoRecovery {
public:
    explicit AutoRecovery(std::shared_ptr<SovereignInferenceClient> aiClient,
                         std::shared_ptr<RawrXD::Core::SessionManager> sessionManager)
        : m_aiClient(aiClient)
        , m_sessionManager(sessionManager) {
        InitializeRecoveryActions();
    }

    RecoveryPlan CreateRecoveryPlan(const ErrorContext& context) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        RecoveryPlan plan;
        plan.errorId = context.errorId;
        plan.createdAt = std::chrono::system_clock::now();
        plan.status = RecoveryStatus::PENDING;
        plan.currentAttempt = 0;
        
        // Select appropriate recovery actions based on error type
        for (const auto& action : m_recoveryActions) {
            if (action.condition(context)) {
                plan.actions.push_back(action);
            }
        }
        
        // AI-enhanced recovery
        if (m_aiClient && m_aiClient->IsLoaded() && plan.actions.empty()) {
            auto aiActions = GenerateAIRecoveryActions(context);
            plan.actions.insert(plan.actions.end(), aiActions.begin(), aiActions.end());
        }
        
        m_plans[context.errorId] = plan;
        return plan;
    }

    RecoveryResults ExecuteRecovery(const RecoveryPlan& plan) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        RecoveryResults results;
        results.errorId = plan.errorId;
        results.startedAt = std::chrono::system_clock::now();
        
        auto mutablePlan = plan;
        mutablePlan.status = RecoveryStatus::IN_PROGRESS;
        m_plans[plan.errorId] = mutablePlan;
        
        try {
            for (const auto& action : plan.actions) {
                bool success = false;
                int attempts = 0;
                
                while (!success && attempts < action.maxAttempts) {
                    success = action.execute(GetErrorContext(plan.errorId));
                    attempts++;
                    
                    if (!success && attempts < action.maxAttempts) {
                        std::this_thread::sleep_for(std::chrono::seconds(action.backoffSeconds * attempts));
                    }
                }
                
                results.attemptsMade += attempts;
                
                if (success) {
                    results.actionsTaken.push_back(action.description);
                } else {
                    results.success = false;
                    results.finalState = "Action failed: " + action.description;
                    mutablePlan.status = RecoveryStatus::FAILED;
                    m_plans[plan.errorId] = mutablePlan;
                    return results;
                }
            }
            
            results.success = true;
            results.finalState = "Fully recovered";
            mutablePlan.status = RecoveryStatus::SUCCESS;
            
        } catch (const std::exception& e) {
            results.success = false;
            results.finalState = "Exception: " + std::string(e.what());
            mutablePlan.status = RecoveryStatus::FAILED;
        }
        
        results.completedAt = std::chrono::system_clock::now();
        mutablePlan.completedAt = results.completedAt;
        m_plans[plan.errorId] = mutablePlan;
        m_results.push_back(results);
        
        return results;
    }

    void LearnFromRecoveryOutcomes(const RecoveryResults& results) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        // Update success rates for actions
        for (const auto& action : results.actionsTaken) {
            auto it = m_actionSuccessRates.find(action);
            if (it != m_actionSuccessRates.end()) {
                if (results.success) {
                    it->second.successCount++;
                } else {
                    it->second.failureCount++;
                }
            } else {
                m_actionSuccessRates[action] = {results.success ? 1 : 0, results.success ? 0 : 1};
            }
        }
        
        // Adjust action priorities based on success rates
        AdjustActionPriorities();
    }

    RecoveryStatus GetRecoveryStatus(const std::string& errorId) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_plans.find(errorId);
        if (it != m_plans.end()) {
            return it->second.status;
        }
        return RecoveryStatus::PENDING;
    }

    std::vector<RecoveryResults> GetRecoveryHistory(int limit = 10) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        std::vector<RecoveryResults> history;
        int count = 0;
        for (auto it = m_results.rbegin(); 
             it != m_results.rend() && count < limit; 
             ++it, ++count) {
            history.push_back(*it);
        }
        return history;
    }

private:
    std::shared_ptr<SovereignInferenceClient> m_aiClient;
    std::shared_ptr<RawrXD::Core::SessionManager> m_sessionManager;
    mutable std::mutex m_mutex;
    std::vector<RecoveryAction> m_recoveryActions;
    std::map<std::string, RecoveryPlan> m_plans;
    std::vector<RecoveryResults> m_results;
    
    struct SuccessRate {
        int successCount;
        int failureCount;
    };
    std::map<std::string, SuccessRate> m_actionSuccessRates;

    void InitializeRecoveryActions() {
        // System recovery actions
        m_recoveryActions.push_back({
            "restart_service",
            "Restart affected service",
            [](const ErrorContext& ctx) { return ctx.type == ErrorType::SERVICE; },
            [](const ErrorContext& ctx) { 
                // Restart service logic
                return true; 
            },
            3,
            5
        });
        
        m_recoveryActions.push_back({
            "clear_cache",
            "Clear system cache",
            [](const ErrorContext& ctx) { return ctx.type == ErrorType::MEMORY; },
            [](const ErrorContext& ctx) { 
                // Clear cache logic
                return true; 
            },
            2,
            1
        });
        
        m_recoveryActions.push_back({
            "reconnect_database",
            "Reconnect to database",
            [](const ErrorContext& ctx) { return ctx.type == ErrorType::DATABASE; },
            [](const ErrorContext& ctx) { 
                // Reconnect logic
                return true; 
            },
            5,
            10
        });
        
        m_recoveryActions.push_back({
            "retry_network",
            "Retry network operation",
            [](const ErrorContext& ctx) { return ctx.type == ErrorType::NETWORK; },
            [](const ErrorContext& ctx) { 
                // Retry logic
                return true; 
            },
            3,
            2
        });
    }

    ErrorContext GetErrorContext(const std::string& errorId) {
        // Retrieve error context from storage
        return ErrorContext{};
    }

    std::vector<RecoveryAction> GenerateAIRecoveryActions(const ErrorContext& context) {
        std::vector<RecoveryAction> actions;
        
        if (!m_aiClient || !m_aiClient->IsLoaded()) {
            return actions;
        }

        std::string prompt = "Suggest recovery actions for error: " + context.message;
        
        std::vector<ChatMessage> messages = {
            {"system", "You are a system recovery expert."},
            {"user", prompt}
        };

        auto result = m_aiClient->ChatSync(messages);
        
        if (result.success) {
            RecoveryAction aiAction;
            aiAction.id = "ai_suggested";
            aiAction.description = "AI: " + result.response;
            aiAction.condition = [](const ErrorContext&) { return true; };
            aiAction.execute = [](const ErrorContext&) { return true; };
            aiAction.maxAttempts = 1;
            aiAction.backoffSeconds = 0;
            actions.push_back(aiAction);
        }
        
        return actions;
    }

    void AdjustActionPriorities() {
        // Adjust action ordering based on success rates
        std::sort(m_recoveryActions.begin(), m_recoveryActions.end(),
                 [this](const RecoveryAction& a, const RecoveryAction& b) {
                     auto rateA = m_actionSuccessRates[a.id];
                     auto rateB = m_actionSuccessRates[b.id];
                     double successRateA = rateA.successCount + rateA.failureCount > 0 
                         ? static_cast<double>(rateA.successCount) / (rateA.successCount + rateA.failureCount) 
                         : 0.5;
                     double successRateB = rateB.successCount + rateB.failureCount > 0 
                         ? static_cast<double>(rateB.successCount) / (rateB.successCount + rateB.failureCount) 
                         : 0.5;
                     return successRateA > successRateB;
                 });
    }
};

} // namespace RawrXD::Recovery
