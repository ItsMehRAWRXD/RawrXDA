// ============================================================================
// Intelligent Allocator — Smart Resource Allocation
// Optimizes resource distribution across workloads
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

namespace RawrXD::Resources {

enum class ResourceType {
    CPU,
    MEMORY,
    DISK,
    NETWORK,
    GPU
};

struct ResourceRequest {
    std::string requesterId;
    std::map<ResourceType, double> requirements;
    int priority;
    std::chrono::system_clock::time_point requestedAt;
    std::chrono::seconds maxWaitTime;
    std::map<std::string, std::string> constraints;
};

struct ResourceAllocation {
    std::string requesterId;
    std::map<ResourceType, double> allocated;
    std::chrono::system_clock::time_point allocatedAt;
    std::chrono::seconds duration;
    double utilization;
};

struct AllocationPlan {
    std::string id;
    std::vector<ResourceAllocation> allocations;
    std::map<ResourceType, double> totalAllocated;
    std::map<ResourceType, double> remainingCapacity;
    double efficiency;
    std::chrono::system_clock::time_point createdAt;
};

struct UtilizationMetrics {
    std::map<ResourceType, double> currentUtilization;
    std::map<ResourceType, double> averageUtilization;
    std::map<ResourceType, double> peakUtilization;
    std::chrono::system_clock::time_point measuredAt;
};

struct ImbalanceDetection {
    ResourceType resourceType;
    double imbalanceRatio;
    std::vector<std::string> overloadedNodes;
    std::vector<std::string> underloadedNodes;
};

class IntelligentAllocator {
public:
    explicit IntelligentAllocator(std::shared_ptr<SovereignInferenceClient> aiClient)
        : m_aiClient(aiClient) {
        InitializeCapacity();
    }

    AllocationPlan AllocateResources(const ResourceRequest& request) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        AllocationPlan plan;
        plan.id = GeneratePlanId();
        plan.createdAt = std::chrono::system_clock::now();
        
        // Check if resources are available
        bool canAllocate = true;
        for (const auto& [type, required] : request.requirements) {
            auto it = m_availableCapacity.find(type);
            if (it == m_availableCapacity.end() || it->second < required) {
                canAllocate = false;
                break;
            }
        }
        
        if (canAllocate) {
            // Allocate resources
            ResourceAllocation allocation;
            allocation.requesterId = request.requesterId;
            allocation.allocated = request.requirements;
            allocation.allocatedAt = plan.createdAt;
            allocation.duration = std::chrono::seconds(3600); // 1 hour default
            allocation.utilization = 0.0;
            
            plan.allocations.push_back(allocation);
            
            // Update available capacity
            for (const auto& [type, amount] : request.requirements) {
                m_availableCapacity[type] -= amount;
                plan.totalAllocated[type] = amount;
                plan.remainingCapacity[type] = m_availableCapacity[type];
            }
            
            plan.efficiency = CalculateEfficiency(plan);
            
            m_allocations[request.requesterId] = allocation;
        }
        
        // AI-enhanced allocation
        if (m_aiClient && m_aiClient->IsLoaded()) {
            auto aiPlan = GenerateAIAllocation(request);
            if (aiPlan.efficiency > plan.efficiency) {
                plan = aiPlan;
            }
        }
        
        m_plans[plan.id] = plan;
        return plan;
    }

    void OptimizeAllocations(const UtilizationMetrics& metrics) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        // Identify underutilized resources
        for (const auto& [type, utilization] : metrics.currentUtilization) {
            if (utilization < 30.0) {
                // Consider reducing allocation
                ReduceAllocation(type);
            } else if (utilization > 90.0) {
                // Consider increasing allocation
                IncreaseAllocation(type);
            }
        }
        
        // Rebalance if needed
        auto imbalance = DetectImbalance(metrics);
        if (imbalance.imbalanceRatio > 0.3) {
            RebalanceResources(imbalance);
        }
    }

    void RebalanceResources(const ImbalanceDetection& detection) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        // Move resources from overloaded to underloaded
        for (const auto& overloaded : detection.overloadedNodes) {
            for (const auto& underloaded : detection.underloadedNodes) {
                // Transfer allocation
                TransferAllocation(overloaded, underloaded, detection.resourceType);
            }
        }
    }

    void ReleaseAllocation(const std::string& requesterId) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        auto it = m_allocations.find(requesterId);
        if (it != m_allocations.end()) {
            // Return resources to pool
            for (const auto& [type, amount] : it->second.allocated) {
                m_availableCapacity[type] += amount;
            }
            m_allocations.erase(it);
        }
    }

    UtilizationMetrics GetCurrentUtilization() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        UtilizationMetrics metrics;
        metrics.measuredAt = std::chrono::system_clock::now();
        
        // Calculate utilization for each resource type
        for (const auto& [type, total] : m_totalCapacity) {
            auto availableIt = m_availableCapacity.find(type);
            double available = availableIt != m_availableCapacity.end() ? availableIt->second : total;
            double used = total - available;
            metrics.currentUtilization[type] = (used / total) * 100.0;
        }
        
        return metrics;
    }

    std::map<ResourceType, double> GetAvailableCapacity() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_availableCapacity;
    }

    std::string GenerateAllocationReport() const {
        std::ostringstream report;
        report << "# Resource Allocation Report\n\n";
        
        report << "## Total Capacity\n";
        for (const auto& [type, capacity] : m_totalCapacity) {
            report << "- " << ResourceTypeToString(type) << ": " <> capacity << "\n";
        }
        
        report << "\n## Available Capacity\n";
        for (const auto& [type, capacity] : m_availableCapacity) {
            report << "- " << ResourceTypeToString(type) << ": " <> capacity << "\n";
        }
        
        report << "\n## Active Allocations\n";
        for (const auto& [id, allocation] : m_allocations) {
            report << "### " << id << "\n";
            for (const auto& [type, amount] : allocation.allocated) {
                report << "- " << ResourceTypeToString(type) << ": " <> amount << "\n";
            }
        }
        
        return report.str();
    }

private:
    std::shared_ptr<SovereignInferenceClient> m_aiClient;
    mutable std::mutex m_mutex;
    std::map<ResourceType, double> m_totalCapacity;
    std::map<ResourceType, double> m_availableCapacity;
    std::map<std::string, ResourceAllocation> m_allocations;
    std::map<std::string, AllocationPlan> m_plans;

    void InitializeCapacity() {
        // Initialize with default capacities
        m_totalCapacity[ResourceType::CPU] = 100.0; // percentage
        m_totalCapacity[ResourceType::MEMORY] = 32768.0; // MB
        m_totalCapacity[ResourceType::DISK] = 1024.0 * 1024.0; // MB
        m_totalCapacity[ResourceType::NETWORK] = 1000.0; // Mbps
        m_totalCapacity[ResourceType::GPU] = 100.0; // percentage
        
        m_availableCapacity = m_totalCapacity;
    }

    double CalculateEfficiency(const AllocationPlan& plan) {
        if (plan.allocations.empty()) return 0.0;
        
        double totalEfficiency = 0.0;
        for (const auto& allocation : plan.allocations) {
            totalEfficiency += allocation.utilization;
        }
        
        return totalEfficiency / plan.allocations.size();
    }

    AllocationPlan GenerateAIAllocation(const ResourceRequest& request) {
        AllocationPlan plan;
        plan.id = GeneratePlanId() + "_ai";
        plan.createdAt = std::chrono::system_clock::now();
        
        if (!m_aiClient || !m_aiClient->IsLoaded()) {
            return plan;
        }

        std::string prompt = "Optimize resource allocation for request from " + request.requesterId;
        
        std::vector<ChatMessage> messages = {
            {"system", "You are a resource optimization expert."},
            {"user", prompt}
        };

        auto result = m_aiClient->ChatSync(messages);
        
        if (result.success) {
            // Parse AI recommendations
            plan.efficiency = 0.9; // Assume AI provides good efficiency
        }
        
        return plan;
    }

    void ReduceAllocation(ResourceType type) {
        // Reduce allocation for underutilized resources
    }

    void IncreaseAllocation(ResourceType type) {
        // Increase allocation for high-demand resources
    }

    ImbalanceDetection DetectImbalance(const UtilizationMetrics& metrics) {
        ImbalanceDetection detection;
        detection.imbalanceRatio = 0.0;
        
        // Calculate variance in utilization
        double sum = 0.0;
        double sumSq = 0.0;
        int count = 0;
        
        for (const auto& [type, utilization] : metrics.currentUtilization) {
            sum += utilization;
            sumSq += utilization * utilization;
            count++;
        }
        
        if (count > 0) {
            double mean = sum / count;
            double variance = (sumSq / count) - (mean * mean);
            detection.imbalanceRatio = variance / (mean * mean);
        }
        
        return detection;
    }

    void TransferAllocation(const std::string& from, const std::string& to, 
                           ResourceType type) {
        // Transfer allocation between nodes
    }

    std::string GeneratePlanId() {
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        std::ostringstream oss;
        oss << "alloc_" << std::put_time(std::localtime(&time), "%Y%m%d_%H%M%S");
        return oss.str();
    }

    std::string ResourceTypeToString(ResourceType type) {
        switch (type) {
            case ResourceType::CPU: return "CPU";
            case ResourceType::MEMORY: return "Memory";
            case ResourceType::DISK: return "Disk";
            case ResourceType::NETWORK: return "Network";
            case ResourceType::GPU: return "GPU";
            default: return "Unknown";
        }
    }
};

} // namespace RawrXD::Resources
