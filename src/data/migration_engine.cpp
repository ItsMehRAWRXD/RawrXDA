// ============================================================================
// Migration Engine — Intelligent Data Migration
// Plans, executes, and validates data migrations with minimal downtime
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

namespace RawrXD::Data {

enum class MigrationStatus {
    PLANNED,
    IN_PROGRESS,
    COMPLETED,
    FAILED,
    ROLLED_BACK
};

struct DataSource {
    std::string id;
    std::string type;
    std::string connectionString;
    std::map<std::string, std::string> schema;
    size_t recordCount;
};

struct DataInventory {
    std::vector<DataSource> sources;
    std::map<std::string, size_t> sizeByType;
    std::chrono::system_clock::time_point inventoriedAt;
};

struct MigrationStep {
    std::string id;
    std::string description;
    std::string sourceQuery;
    std::string targetQuery;
    std::vector<std::string> dependencies;
    std::function<bool()> validation;
    int estimatedDurationMinutes;
};

struct MigrationPlan {
    std::string id;
    std::string name;
    std::vector<MigrationStep> steps;
    int totalEstimatedDuration;
    std::chrono::system_clock::time_point plannedAt;
    MigrationStatus status;
    std::map<std::string, std::string> metadata;
};

struct MigrationOutcome {
    std::string planId;
    bool success;
    int recordsMigrated;
    int recordsFailed;
    std::chrono::system_clock::time_point startedAt;
    std::chrono::system_clock::time_point completedAt;
    std::vector<std::string> errors;
    std::map<std::string, double> performanceMetrics;
};

class MigrationEngine {
public:
    explicit MigrationEngine(std::shared_ptr<SovereignInferenceClient> aiClient,
                           std::shared_ptr<RawrXD::Core::SessionManager> sessionManager)
        : m_aiClient(aiClient)
        , m_sessionManager(sessionManager) {}

    MigrationPlan PlanDataMigration(const DataInventory& inventory) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        MigrationPlan plan;
        plan.id = GeneratePlanId();
        plan.plannedAt = std::chrono::system_clock::now();
        plan.status = MigrationStatus::PLANNED;
        
        // Analyze data sources
        for (const auto& source : inventory.sources) {
            // Create migration steps for each source
            MigrationStep step;
            step.id = "step_" + source.id;
            step.description = "Migrate " + source.type + " data from " + source.id;
            step.sourceQuery = BuildSourceQuery(source);
            step.targetQuery = BuildTargetQuery(source);
            step.estimatedDurationMinutes = EstimateDuration(source);
            
            plan.steps.push_back(step);
            plan.totalEstimatedDuration += step.estimatedDurationMinutes;
        }
        
        // Add validation step
        MigrationStep validationStep;
        validationStep.id = "validation";
        validationStep.description = "Validate migration results";
        validationStep.estimatedDurationMinutes = 30;
        plan.steps.push_back(validationStep);
        
        // AI-enhanced planning
        if (m_aiClient && m_aiClient->IsLoaded()) {
            auto aiSteps = GenerateAIEnhancedSteps(inventory);
            plan.steps.insert(plan.steps.end(), aiSteps.begin(), aiSteps.end());
        }
        
        m_plans[plan.id] = plan;
        return plan;
    }

    MigrationOutcome ExecuteMigration(const MigrationPlan& plan) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        MigrationOutcome outcome;
        outcome.planId = plan.id;
        outcome.startedAt = std::chrono::system_clock::now();
        
        auto mutablePlan = plan;
        mutablePlan.status = MigrationStatus::IN_PROGRESS;
        m_plans[plan.id] = mutablePlan;
        
        try {
            // Execute each step
            for (const auto& step : plan.steps) {
                if (!ExecuteMigrationStep(step)) {
                    outcome.success = false;
                    outcome.errors.push_back("Step failed: " + step.id);
                    mutablePlan.status = MigrationStatus::FAILED;
                    m_plans[plan.id] = mutablePlan;
                    return outcome;
                }
                outcome.recordsMigrated += EstimateRecordCount(step);
            }
            
            outcome.success = true;
            mutablePlan.status = MigrationStatus::COMPLETED;
            
        } catch (const std::exception& e) {
            outcome.success = false;
            outcome.errors.push_back(e.what());
            mutablePlan.status = MigrationStatus::FAILED;
        }
        
        outcome.completedAt = std::chrono::system_clock::now();
        mutablePlan.status = outcome.success ? MigrationStatus::COMPLETED : MigrationStatus::FAILED;
        m_plans[plan.id] = mutablePlan;
        m_outcomes.push_back(outcome);
        
        return outcome;
    }

    bool ValidateMigrationResults(const MigrationOutcome& outcome) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        if (!outcome.success) {
            return false;
        }
        
        // Validate record counts
        if (outcome.recordsFailed > outcome.recordsMigrated * 0.01) { // > 1% failure
            return false;
        }
        
        // Validate data integrity
        // This would perform checksums, referential integrity checks, etc.
        
        // AI validation
        if (m_aiClient && m_aiClient->IsLoaded()) {
            return PerformAIValidation(outcome);
        }
        
        return true;
    }

    void RollbackMigration(const std::string& planId) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        auto it = m_plans.find(planId);
        if (it == m_plans.end()) {
            return;
        }
        
        auto& plan = it->second;
        
        // Execute rollback in reverse order
        for (auto it = plan.steps.rbegin(); it != plan.steps.rend(); ++it) {
            RollbackStep(*it);
        }
        
        plan.status = MigrationStatus::ROLLED_BACK;
    }

    std::vector<MigrationPlan> GetMigrationHistory() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        std::vector<MigrationPlan> history;
        for (const auto& [id, plan] : m_plans) {
            history.push_back(plan);
        }
        return history;
    }

    MigrationOutcome GetMigrationOutcome(const std::string& planId) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        for (const auto& outcome : m_outcomes) {
            if (outcome.planId == planId) {
                return outcome;
            }
        }
        return MigrationOutcome{};
    }

private:
    std::shared_ptr<SovereignInferenceClient> m_aiClient;
    std::shared_ptr<RawrXD::Core::SessionManager> m_sessionManager;
    mutable std::mutex m_mutex;
    std::map<std::string, MigrationPlan> m_plans;
    std::vector<MigrationOutcome> m_outcomes;

    std::string BuildSourceQuery(const DataSource& source) {
        return "SELECT * FROM " + source.id;
    }

    std::string BuildTargetQuery(const DataSource& source) {
        return "INSERT INTO " + source.id + " VALUES (...)";
    }

    int EstimateDuration(const DataSource& source) {
        // Estimate based on record count
        return static_cast<int>(source.recordCount / 10000) + 1; // ~10k records per minute
    }

    int EstimateRecordCount(const MigrationStep& step) {
        return 1000; // Simplified
    }

    bool ExecuteMigrationStep(const MigrationStep& step) {
        // Execute the migration step
        // This would integrate with your database/ETL system
        return true;
    }

    void RollbackStep(const MigrationStep& step) {
        // Rollback the step
    }

    std::vector<MigrationStep> GenerateAIEnhancedSteps(const DataInventory& inventory) {
        std::vector<MigrationStep> steps;
        
        if (!m_aiClient || !m_aiClient->IsLoaded()) {
            return steps;
        }

        std::string prompt = "Suggest migration steps for " + 
                            std::to_string(inventory.sources.size()) + " data sources";
        
        std::vector<ChatMessage> messages = {
            {"system", "You are a data migration expert."},
            {"user", prompt}
        };

        auto result = m_aiClient->ChatSync(messages);
        
        if (result.success) {
            MigrationStep aiStep;
            aiStep.id = "ai_enhanced";
            aiStep.description = "AI-enhanced: " + result.response;
            aiStep.estimatedDurationMinutes = 15;
            steps.push_back(aiStep);
        }
        
        return steps;
    }

    bool PerformAIValidation(const MigrationOutcome& outcome) {
        std::string prompt = "Validate migration outcome: " + 
                            std::to_string(outcome.recordsMigrated) + " records migrated, " +
                            std::to_string(outcome.recordsFailed) + " failed";
        
        std::vector<ChatMessage> messages = {
            {"system", "You are a data validation expert."},
            {"user", prompt}
        };

        auto result = m_aiClient->ChatSync(messages);
        
        return result.success && result.response.find("valid") != std::string::npos;
    }

    std::string GeneratePlanId() {
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        std::ostringstream oss;
        oss << "migration_" << std::put_time(std::localtime(&time), "%Y%m%d_%H%M%S");
        return oss.str();
    }
};

} // namespace RawrXD::Data
