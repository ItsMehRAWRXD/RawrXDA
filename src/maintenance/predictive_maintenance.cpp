// ============================================================================
// Predictive Maintenance — Intelligent System Maintenance
// Predicts maintenance needs and schedules proactive interventions
// ============================================================================
#pragma once
#include "../inference/SovereignInferenceClient.h"
#include "../performance/realtime_profiler.h"
#include <memory>
#include <vector>
#include <string>
#include <map>
#include <mutex>
#include <chrono>

namespace RawrXD::Maintenance {

enum class MaintenanceType {
    PREVENTIVE,
    CORRECTIVE,
    PREDICTIVE,
    EMERGENCY
};

enum class MaintenanceStatus {
    SCHEDULED,
    IN_PROGRESS,
    COMPLETED,
    CANCELLED,
    OVERDUE
};

struct SystemHealth {
    double cpuHealth;
    double memoryHealth;
    double diskHealth;
    double networkHealth;
    std::map<std::string, double> componentHealth;
    std::chrono::system_clock::time_point measuredAt;
};

struct MaintenanceTask {
    std::string id;
    std::string description;
    MaintenanceType type;
    std::vector<std::string> affectedComponents;
    int estimatedDurationMinutes;
    std::chrono::system_clock::time_point scheduledFor;
    MaintenanceStatus status;
    std::string assignedTo;
};

struct MaintenanceSchedule {
    std::string id;
    std::vector<MaintenanceTask> tasks;
    std::chrono::system_clock::time_point createdAt;
    std::chrono::system_clock::time_point validUntil;
    double predictedReliability;
};

struct MaintenancePlan {
    std::string scheduleId;
    std::vector<MaintenanceTask> tasks;
    int totalEstimatedDuration;
    std::chrono::system_clock::time_point plannedStart;
    std::chrono::system_clock::time_point plannedEnd;
    std::map<std::string, std::string> resourceAllocations;
};

struct MaintenanceOutcomes {
    std::string planId;
    std::vector<std::string> completedTasks;
    std::vector<std::string> failedTasks;
    double actualReliability;
    std::chrono::system_clock::time_point completedAt;
    std::map<std::string, double> improvements;
};

class PredictiveMaintenance {
public:
    explicit PredictiveMaintenance(std::shared_ptr<SovereignInferenceClient> aiClient)
        : m_aiClient(aiClient) {}

    MaintenanceSchedule PredictMaintenanceNeeds(const SystemHealth& health) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        MaintenanceSchedule schedule;
        schedule.id = GenerateScheduleId();
        schedule.createdAt = std::chrono::system_clock::now();
        schedule.validUntil = schedule.createdAt + std::chrono::days(30);
        
        // Analyze health metrics
        if (health.cpuHealth < 70.0) {
            MaintenanceTask task;
            task.id = "cpu_maintenance";
            task.description = "CPU optimization and cleanup";
            task.type = MaintenanceType::PREDICTIVE;
            task.affectedComponents = {"CPU"};
            task.estimatedDurationMinutes = 60;
            task.scheduledFor = schedule.createdAt + std::chrono::days(7);
            task.status = MaintenanceStatus::SCHEDULED;
            schedule.tasks.push_back(task);
        }
        
        if (health.memoryHealth < 70.0) {
            MaintenanceTask task;
            task.id = "memory_maintenance";
            task.description = "Memory leak detection and cleanup";
            task.type = MaintenanceType::PREDICTIVE;
            task.affectedComponents = {"Memory"};
            task.estimatedDurationMinutes = 45;
            task.scheduledFor = schedule.createdAt + std::chrono::days(5);
            task.status = MaintenanceStatus::SCHEDULED;
            schedule.tasks.push_back(task);
        }
        
        if (health.diskHealth < 60.0) {
            MaintenanceTask task;
            task.id = "disk_maintenance";
            task.description = "Disk cleanup and defragmentation";
            task.type = MaintenanceType::PREVENTIVE;
            task.affectedComponents = {"Disk"};
            task.estimatedDurationMinutes = 120;
            task.scheduledFor = schedule.createdAt + std::chrono::days(3);
            task.status = MaintenanceStatus::SCHEDULED;
            schedule.tasks.push_back(task);
        }
        
        // AI-enhanced predictions
        if (m_aiClient && m_aiClient->IsLoaded()) {
            auto aiTasks = GenerateAIMaintenanceTasks(health);
            schedule.tasks.insert(schedule.tasks.end(), aiTasks.begin(), aiTasks.end());
        }
        
        // Calculate predicted reliability
        schedule.predictedReliability = CalculatePredictedReliability(health, schedule);
        
        m_schedules[schedule.id] = schedule;
        return schedule;
    }

    MaintenancePlan ScheduleMaintenance(const MaintenanceSchedule& schedule) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        MaintenancePlan plan;
        plan.scheduleId = schedule.id;
        plan.tasks = schedule.tasks;
        
        // Calculate total duration
        for (const auto& task : schedule.tasks) {
            plan.totalEstimatedDuration += task.estimatedDurationMinutes;
        }
        
        // Schedule tasks
        auto currentTime = std::chrono::system_clock::now();
        plan.plannedStart = currentTime;
        plan.plannedEnd = currentTime + std::chrono::minutes(plan.totalEstimatedDuration);
        
        // Allocate resources
        for (const auto& task : schedule.tasks) {
            plan.resourceAllocations[task.id] = "auto-assigned";
        }
        
        m_plans[plan.scheduleId] = plan;
        return plan;
    }

    void UpdateMaintenanceModels(const MaintenanceOutcomes& outcomes) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        // Update success rates
        for (const auto& task : outcomes.completedTasks) {
            m_taskSuccessRates[task].successCount++;
        }
        
        for (const auto& task : outcomes.failedTasks) {
            m_taskSuccessRates[task].failureCount++;
        }
        
        // Update reliability predictions
        UpdateReliabilityModel(outcomes);
        
        // Adjust scheduling based on outcomes
        AdjustScheduling(outcomes);
    }

    std::vector<MaintenanceTask> GetOverdueTasks() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        std::vector<MaintenanceTask> overdue;
        auto now = std::chrono::system_clock::now();
        
        for (const auto& [id, schedule] : m_schedules) {
            for (const auto& task : schedule.tasks) {
                if (task.status == MaintenanceStatus::SCHEDULED && 
                    task.scheduledFor < now) {
                    overdue.push_back(task);
                }
            }
        }
        
        return overdue;
    }

    SystemHealth GetCurrentSystemHealth() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        // In a real implementation, this would query system metrics
        SystemHealth health;
        health.cpuHealth = 85.0;
        health.memoryHealth = 80.0;
        health.diskHealth = 90.0;
        health.networkHealth = 95.0;
        health.measuredAt = std::chrono::system_clock::now();
        
        return health;
    }

    std::string GenerateMaintenanceReport(const MaintenanceSchedule& schedule) const {
        std::ostringstream report;
        report << "# Maintenance Schedule Report\n\n";
        report << "**Schedule ID:** " << schedule.id << "\n";
        report << "**Created:** " << FormatTime(schedule.createdAt) << "\n";
        report << "**Valid Until:** " << FormatTime(schedule.validUntil) << "\n";
        report << "**Predicted Reliability:** " << std::fixed << std::setprecision(1) 
               << schedule.predictedReliability << "%\n\n";
        
        report << "## Scheduled Tasks\n";
        for (const auto& task : schedule.tasks) {
            report << "### " << task.description << "\n";
            report << "- **Type:** " << TypeToString(task.type) << "\n";
            report << "- **Scheduled:** " << FormatTime(task.scheduledFor) << "\n";
            report << "- **Duration:** " << task.estimatedDurationMinutes << " minutes\n";
            report << "- **Status:** " << StatusToString(task.status) << "\n";
            report << "\n";
        }
        
        return report.str();
    }

private:
    std::shared_ptr<SovereignInferenceClient> m_aiClient;
    mutable std::mutex m_mutex;
    std::map<std::string, MaintenanceSchedule> m_schedules;
    std::map<std::string, MaintenancePlan> m_plans;
    
    struct SuccessRate {
        int successCount = 0;
        int failureCount = 0;
    };
    std::map<std::string, SuccessRate> m_taskSuccessRates;

    std::vector<MaintenanceTask> GenerateAIMaintenanceTasks(const SystemHealth& health) {
        std::vector<MaintenanceTask> tasks;
        
        if (!m_aiClient || !m_aiClient->IsLoaded()) {
            return tasks;
        }

        std::string prompt = "Predict maintenance needs based on system health:\n" +
                            "CPU: " + std::to_string(static_cast<int>(health.cpuHealth)) + "%\n" +
                            "Memory: " + std::to_string(static_cast<int>(health.memoryHealth)) + "%\n" +
                            "Disk: " + std::to_string(static_cast<int>(health.diskHealth)) + "%\n";
        
        std::vector<ChatMessage> messages = {
            {"system", "You are a predictive maintenance expert."},
            {"user", prompt}
        };

        auto result = m_aiClient->ChatSync(messages);
        
        if (result.success) {
            MaintenanceTask aiTask;
            aiTask.id = "ai_predicted";
            aiTask.description = "AI: " + result.response;
            aiTask.type = MaintenanceType::PREDICTIVE;
            aiTask.estimatedDurationMinutes = 30;
            aiTask.scheduledFor = std::chrono::system_clock::now() + std::chrono::days(14);
            aiTask.status = MaintenanceStatus::SCHEDULED;
            tasks.push_back(aiTask);
        }
        
        return tasks;
    }

    double CalculatePredictedReliability(const SystemHealth& health, 
                                        const MaintenanceSchedule& schedule) {
        double baseReliability = (health.cpuHealth + health.memoryHealth + 
                                 health.diskHealth + health.networkHealth) / 4.0;
        
        // Adjust based on scheduled maintenance
        double maintenanceBonus = schedule.tasks.size() * 2.0;
        
        return std::min(100.0, baseReliability + maintenanceBonus);
    }

    void UpdateReliabilityModel(const MaintenanceOutcomes& outcomes) {
        // Update reliability prediction model based on actual outcomes
    }

    void AdjustScheduling(const MaintenanceOutcomes& outcomes) {
        // Adjust future scheduling based on outcomes
    }

    std::string GenerateScheduleId() {
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        std::ostringstream oss;
        oss << "schedule_" << std::put_time(std::localtime(&time), "%Y%m%d_%H%M%S");
        return oss.str();
    }

    std::string TypeToString(MaintenanceType type) {
        switch (type) {
            case MaintenanceType::PREVENTIVE: return "Preventive";
            case MaintenanceType::CORRECTIVE: return "Corrective";
            case MaintenanceType::PREDICTIVE: return "Predictive";
            case MaintenanceType::EMERGENCY: return "Emergency";
            default: return "Unknown";
        }
    }

    std::string StatusToString(MaintenanceStatus status) {
        switch (status) {
            case MaintenanceStatus::SCHEDULED: return "Scheduled";
            case MaintenanceStatus::IN_PROGRESS: return "In Progress";
            case MaintenanceStatus::COMPLETED: return "Completed";
            case MaintenanceStatus::CANCELLED: return "Cancelled";
            case MaintenanceStatus::OVERDUE: return "Overdue";
            default: return "Unknown";
        }
    }

    std::string FormatTime(std::chrono::system_clock::time_point time) {
        auto timeT = std::chrono::system_clock::to_time_t(time);
        std::ostringstream oss;
        oss << std::put_time(std::localtime(&timeT), "%Y-%m-%d %H:%M:%S");
        return oss.str();
    }
};

} // namespace RawrXD::Maintenance
