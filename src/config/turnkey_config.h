// turnkey_config.h — RawrXD IDE Turnkey Configuration System
// Auto-detection and self-configuration for production deployment
// Part of 14-Day Sprint: Final Integration & Turnkey Validation

#pragma once
#ifndef RAWRXD_TURNKEY_CONFIG_H
#define RAWRXD_TURNKEY_CONFIG_H

#include <windows.h>
#include <string>
#include <vector>
#include <functional>

namespace rawrxd {
namespace config {

// ============================================================================
// Turnkey Mode Enumeration
// ============================================================================
enum class TurnkeyMode {
    Unknown,           // Not yet determined
    FullTurnkey,       // Auto-configured, all features enabled
    Assisted,          // Partial auto-config, user guidance needed
    Manual,            // Full manual configuration required
    Degraded,          // Running with reduced functionality
    Recovery           // Recovery mode after configuration failure
};

// ============================================================================
// Configuration Detection Result
// ============================================================================
struct DetectionResult {
    bool success{false};
    std::string component;
    std::string detectedPath;
    std::string version;
    std::string errorMessage;
    bool isRequired{true};
    bool isFound{false};
};

// ============================================================================
// Turnkey Configuration State
// ============================================================================
struct TurnkeyState {
    TurnkeyMode mode{TurnkeyMode::Unknown};
    bool firstRun{true};
    bool autoConfigured{false};
    std::string configVersion{"1.0.0"};
    std::vector<DetectionResult> detections;
    
    // Feature flags based on detection
    bool hasGit{false};
    bool hasPython{false};
    bool hasNodeJS{false};
    bool hasPowerShell7{false};
    bool hasVSBuildTools{false};
    bool hasCMake{false};
    bool hasNinja{false};
    bool hasVulkanSDK{false};
    bool hasCUDA{false};
    
    // Paths
    std::string gitPath;
    std::string pythonPath;
    std::string nodePath;
    std::string pwshPath;
    std::string msbuildPath;
    std::string cmakePath;
    std::string ninjaPath;
    std::string vulkanSdkPath;
    std::string cudaPath;
    
    // IDE paths
    std::string workspaceRoot;
    std::string configDir;
    std::string cacheDir;
    std::string logsDir;
    std::string extensionsDir;
};

// ============================================================================
// Turnkey Configuration Manager
// ============================================================================
class TurnkeyConfigManager {
public:
    TurnkeyConfigManager();
    ~TurnkeyConfigManager();

    // Initialize and detect environment
    bool Initialize();
    
    // Detection methods
    TurnkeyState DetectEnvironment();
    DetectionResult DetectGit();
    DetectionResult DetectPython();
    DetectionResult DetectNodeJS();
    DetectionResult DetectPowerShell();
    DetectionResult DetectBuildTools();
    DetectionResult DetectCMake();
    DetectionResult DetectNinja();
    DetectionResult DetectVulkanSDK();
    DetectionResult DetectCUDA();
    
    // Configuration methods
    bool ApplyTurnkeyConfiguration();
    bool CreateDefaultConfigFiles();
    bool SetupDirectoryStructure();
    bool ConfigureFeatureFlags();
    
    // Validation
    bool ValidateConfiguration();
    bool RunSmokeTests();
    std::vector<std::string> GetMissingRequirements();
    
    // Mode management
    TurnkeyMode DetermineOptimalMode();
    bool SwitchMode(TurnkeyMode newMode);
    std::string GetModeDescription(TurnkeyMode mode);
    
    // State access
    const TurnkeyState& GetState() const { return m_state; }
    TurnkeyMode GetCurrentMode() const { return m_state.mode; }
    bool IsFirstRun() const { return m_state.firstRun; }
    
    // Persistence
    bool SaveConfiguration();
    bool LoadConfiguration();
    bool ResetToDefaults();
    
    // Callbacks for UI integration
    using ProgressCallback = std::function<void(const std::string& stage, int percent)>;
    using ErrorCallback = std::function<void(const std::string& component, const std::string& error)>;
    
    void SetProgressCallback(ProgressCallback cb) { m_progressCallback = cb; }
    void SetErrorCallback(ErrorCallback cb) { m_errorCallback = cb; }

private:
    TurnkeyState m_state;
    ProgressCallback m_progressCallback;
    ErrorCallback m_errorCallback;
    
    // Helper methods
    std::string DetectInPath(const std::string& executable);
    std::string DetectInRegistry(const std::string& key, const std::string& value);
    std::string DetectInProgramFiles(const std::string& subPath);
    std::string GetVersionFromExecutable(const std::string& path);
    bool CheckFileExists(const std::string& path);
    TurnkeyMode DetermineOptimalModeForState(const TurnkeyState& state) const;
    void ReportProgress(const std::string& stage, int percent);
    void ReportError(const std::string& component, const std::string& error);
};

// ============================================================================
// Global Access
// ============================================================================
TurnkeyConfigManager* GetTurnkeyConfigManager();
void InitializeTurnkeySystem();
void ShutdownTurnkeySystem();

// ============================================================================
// Utility Functions
// ============================================================================
std::string GetTurnkeyConfigPath();
std::string GetTurnkeyCachePath();
bool IsRunningInTurnkeyMode();
bool RequireComponent(const std::string& component);

} // namespace config
} // namespace rawrxd

#endif // RAWRXD_TURNKEY_CONFIG_H
