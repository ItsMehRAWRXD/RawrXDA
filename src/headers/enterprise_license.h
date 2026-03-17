#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <memory>

// ============================================================================
// CATEGORY 1: Enterprise/License Infrastructure (~25 symbols)
// ============================================================================

// Forward declarations
namespace RawrXD {
namespace License {
enum class FeatureID : unsigned char;
}
}

// ============================================================================
// Enumerations
// ============================================================================

enum class LicenseTier : unsigned int {
    Tier_Free = 0,
    Tier_Pro = 1,
    Tier_Enterprise = 2,
    Tier_Enterprise_Plus = 3
};

enum class FeatureID : unsigned char {
    Feature_BasicInference = 0,
    Feature_MultiGPU = 1,
    Feature_AdvancedOptimization = 2,
    Feature_AIThinking = 3,
    Feature_DeepResearch = 4,
    Feature_HotPatching = 5,
    Feature_CustomModels = 6,
    Feature_RealTimeMonitoring = 7,
    Feature_DistributedCompute = 8,
    Feature_EnterpriseSupport = 9,
    Feature_Max = 10
};

// ============================================================================
// RawrXD::License::EnterpriseLicenseV2
// ============================================================================

namespace RawrXD {
namespace License {

struct LicenseResult {
    bool success;
    std::string error_message;
    uint64_t timestamp;
};

class EnterpriseLicenseV2 {
public:
    EnterpriseLicenseV2(const EnterpriseLicenseV2&) = delete;
    EnterpriseLicenseV2& operator=(const EnterpriseLicenseV2&) = delete;

    static EnterpriseLicenseV2& Instance();
    
    bool gate(FeatureID feature_id, const char* context) const;
    LicenseResult initialize();

private:
    EnterpriseLicenseV2() = default;
    ~EnterpriseLicenseV2() = default;
};

}  // namespace License
}  // namespace RawrXD

// ============================================================================
// RawrXD::Enforce::LicenseEnforcer
// ============================================================================

namespace RawrXD {
namespace Enforce {

enum class SubsystemID : unsigned char {
    Subsystem_GPU = 0,
    Subsystem_CPU = 1,
    Subsystem_Memory = 2,
    Subsystem_Network = 3,
    Subsystem_Storage = 4
};

class LicenseEnforcer {
public:
    LicenseEnforcer(const LicenseEnforcer&) = delete;
    LicenseEnforcer& operator=(const LicenseEnforcer&) = delete;

    static LicenseEnforcer& Instance();
    
    bool allow(License::FeatureID feature_id, const char* context) const;
    bool allow(SubsystemID subsystem_id, License::FeatureID feature_id, const char* context) const;
    bool initialize();
    void shutdown();

private:
    LicenseEnforcer() = default;
    ~LicenseEnforcer() = default;
};

}  // namespace Enforce
}  // namespace RawrXD

// ============================================================================
// EnterpriseFeatureManager
// ============================================================================

struct EnterpriseFeatureStatus {
    std::string feature_name;
    bool is_enabled;
    LicenseTier required_tier;
    uint32_t usage_count;
    std::string last_used;
};

struct FeatureAuditEntry {
    std::string feature_id;
    std::string action;
    std::string timestamp;
    std::string user_context;
    bool success;
};

class EnterpriseFeatureManager {
public:
    EnterpriseFeatureManager(const EnterpriseFeatureManager&) = delete;
    EnterpriseFeatureManager& operator=(const EnterpriseFeatureManager&) = delete;

    static EnterpriseFeatureManager& Instance();
    
    bool Initialize();
    void Shutdown();
    bool DevUnlock();
    bool InstallLicenseFromFile(const std::string& license_file_path);
    
    std::vector<EnterpriseFeatureStatus> GetFeatureStatuses() const;
    std::string GetHWIDString() const;
    LicenseTier GetCurrentTier() const;
    const char* GetEditionName() const;
    
    std::vector<FeatureAuditEntry> RunFullAudit() const;
    std::string GenerateAuditReport() const;
    std::string GenerateDashboard() const;
    
    static const char* GetTierName(LicenseTier tier);

private:
    EnterpriseFeatureManager() = default;
    ~EnterpriseFeatureManager() = default;
};

// ============================================================================
// RawrXD::Enterprise::MultiGPUManager
// ============================================================================

namespace RawrXD {
namespace Enterprise {

enum class DispatchStrategy : unsigned char {
    Strategy_Balanced = 0,
    Strategy_MaxThroughput = 1,
    Strategy_MinLatency = 2,
    Strategy_EnergyEfficient = 3
};

struct LayerAssignment {
    uint32_t layer_id;
    uint32_t device_id;
    uint64_t estimated_vram;
};

struct MultiGPUResult {
    bool success;
    std::string status_message;
    uint64_t timestamp;
};

class MultiGPUManager {
public:
    MultiGPUManager(const MultiGPUManager&) = delete;
    MultiGPUManager& operator=(const MultiGPUManager&) = delete;

    static MultiGPUManager& Instance();
    
    uint32_t GetDeviceCount() const;
    uint64_t GetTotalVRAM() const;
    uint64_t GetFreeVRAM() const;
    bool AllDevicesHealthy() const;
    
    DispatchStrategy GetStrategy() const;
    const char* GetStrategyName(DispatchStrategy strategy) const;
    
    MultiGPUResult Initialize();
    MultiGPUResult DispatchBatch(uint32_t layer_id, uint32_t batch_size, 
                                  uint64_t required_memory, DispatchStrategy strategy);
    MultiGPUResult BuildLayerAssignments(uint32_t num_layers, uint64_t total_memory, 
                                          DispatchStrategy strategy);
    
    const std::vector<LayerAssignment>& GetLayerAssignments() const;
    std::string GenerateStatusReport() const;
    std::string GenerateTopologyReport() const;

private:
    MultiGPUManager() = default;
    ~MultiGPUManager() = default;
};

}  // namespace Enterprise
}  // namespace RawrXD

// ============================================================================
// RawrXD::Enterprise::SupportTierManager
// ============================================================================

namespace RawrXD {
namespace Enterprise {

class SupportTierManager {
public:
    SupportTierManager(const SupportTierManager&) = delete;
    SupportTierManager& operator=(const SupportTierManager&) = delete;

    static SupportTierManager& Instance();
    
    std::string GenerateStatusReport() const;

private:
    SupportTierManager() = default;
    ~SupportTierManager() = default;
};

}  // namespace Enterprise
}  // namespace RawrXD

// ============================================================================
// RawrXD::Flags::FeatureFlagsRuntime
// ============================================================================

namespace RawrXD {
namespace Flags {

class FeatureFlagsRuntime {
public:
    FeatureFlagsRuntime(const FeatureFlagsRuntime&) = delete;
    FeatureFlagsRuntime& operator=(const FeatureFlagsRuntime&) = delete;

    static FeatureFlagsRuntime& Instance();
    
    bool isEnabled(License::FeatureID feature_id) const;
    void refreshFromLicense();

private:
    FeatureFlagsRuntime() = default;
    ~FeatureFlagsRuntime() = default;
};

}  // namespace Flags
}  // namespace RawrXD

// ============================================================================
// RawrXD::AgenticAutonomousConfig
// ============================================================================

namespace RawrXD {

enum class AgenticOperationMode : unsigned char {
    Mode_Disabled = 0,
    Mode_BasicChat = 1,
    Mode_Autonomous = 2,
    Mode_DeepThinking = 3,
    Mode_ResearchMode = 4
};

enum class ModelSelectionMode : unsigned char {
    Mode_SingleModel = 0,
    Mode_MultiModel = 1,
    Mode_AdaptiveSelection = 2
};

class AgenticAutonomousConfig {
public:
    AgenticAutonomousConfig(const AgenticAutonomousConfig&) = delete;
    AgenticAutonomousConfig& operator=(const AgenticAutonomousConfig&) = delete;

    static AgenticAutonomousConfig& instance();
    
    AgenticOperationMode getOperationMode() const;
    bool setOperationModeFromString(const std::string& mode_name);
    
    ModelSelectionMode getModelSelectionMode() const;
    bool setModelSelectionModeFromString(const std::string& mode_name);
    
    int getCycleAgentCounter() const;
    void setCycleAgentCounter(int count);
    
    int getMaxModelsInParallel() const;
    void setMaxModelsInParallel(int count);
    
    int getPerModelInstanceCount() const;
    void setPerModelInstanceCount(int count);
    
    void setInstanceCountForModel(const std::string& model_name, int count);
    void clearModelInstanceOverrides();
    
    int effectiveMaxParallel(int requested_count) const;
    
    std::string toJson() const;
    std::string toDisplayString() const;

private:
    AgenticAutonomousConfig() = default;
    ~AgenticAutonomousConfig() = default;
};

}  // namespace RawrXD
