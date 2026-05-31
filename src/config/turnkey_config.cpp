// turnkey_config.cpp — RawrXD IDE Turnkey Configuration Implementation
// Auto-detection and self-configuration for production deployment
// Part of 14-Day Sprint: Final Integration & Turnkey Validation

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shlwapi.h>
#include <shlobj.h>
#include <string>
#include <vector>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <json/json.h>

#include "turnkey_config.h"

#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "shell32.lib")

namespace rawrxd {
namespace config {

namespace fs = std::filesystem;

// ============================================================================
// Global Instance
// ============================================================================
static TurnkeyConfigManager* g_turnkeyManager = nullptr;

TurnkeyConfigManager* GetTurnkeyConfigManager() {
    if (!g_turnkeyManager) {
        g_turnkeyManager = new TurnkeyConfigManager();
    }
    return g_turnkeyManager;
}

void InitializeTurnkeySystem() {
    if (!g_turnkeyManager) {
        g_turnkeyManager = new TurnkeyConfigManager();
        g_turnkeyManager->Initialize();
    }
}

void ShutdownTurnkeySystem() {
    if (g_turnkeyManager) {
        g_turnkeyManager->SaveConfiguration();
        delete g_turnkeyManager;
        g_turnkeyManager = nullptr;
    }
}

// ============================================================================
// Constructor/Destructor
// ============================================================================
TurnkeyConfigManager::TurnkeyConfigManager() = default;
TurnkeyConfigManager::~TurnkeyConfigManager() = default;

// ============================================================================
// Initialization
// ============================================================================
bool TurnkeyConfigManager::Initialize() {
    ReportProgress("Initializing Turnkey Configuration System", 0);
    
    // Load existing configuration if available
    if (!LoadConfiguration()) {
        // First run - detect environment
        m_state.firstRun = true;
        m_state = DetectEnvironment();
        
        // Apply turnkey configuration
        if (!ApplyTurnkeyConfiguration()) {
            ReportError("Initialize", "Failed to apply turnkey configuration");
            return false;
        }
        
        // Save configuration
        SaveConfiguration();
    } else {
        m_state.firstRun = false;
    }
    
    ReportProgress("Initialization Complete", 100);
    return true;
}

// ============================================================================
// Environment Detection
// ============================================================================
TurnkeyState TurnkeyConfigManager::DetectEnvironment() {
    ReportProgress("Detecting Environment", 10);
    
    TurnkeyState state;
    state.mode = TurnkeyMode::Unknown;
    
    // Detect all required and optional components
    auto gitResult = DetectGit();
    state.detections.push_back(gitResult);
    state.hasGit = gitResult.isFound;
    state.gitPath = gitResult.detectedPath;
    
    ReportProgress("Detecting Python", 20);
    auto pythonResult = DetectPython();
    state.detections.push_back(pythonResult);
    state.hasPython = pythonResult.isFound;
    state.pythonPath = pythonResult.detectedPath;
    
    ReportProgress("Detecting Node.js", 30);
    auto nodeResult = DetectNodeJS();
    state.detections.push_back(nodeResult);
    state.hasNodeJS = nodeResult.isFound;
    state.nodePath = nodeResult.detectedPath;
    
    ReportProgress("Detecting PowerShell", 40);
    auto pwshResult = DetectPowerShell();
    state.detections.push_back(pwshResult);
    state.hasPowerShell7 = pwshResult.isFound;
    state.pwshPath = pwshResult.detectedPath;
    
    ReportProgress("Detecting Build Tools", 50);
    auto buildResult = DetectBuildTools();
    state.detections.push_back(buildResult);
    state.hasVSBuildTools = buildResult.isFound;
    state.msbuildPath = buildResult.detectedPath;
    
    ReportProgress("Detecting CMake", 60);
    auto cmakeResult = DetectCMake();
    state.detections.push_back(cmakeResult);
    state.hasCMake = cmakeResult.isFound;
    state.cmakePath = cmakeResult.detectedPath;
    
    ReportProgress("Detecting Ninja", 70);
    auto ninjaResult = DetectNinja();
    state.detections.push_back(ninjaResult);
    state.hasNinja = ninjaResult.isFound;
    state.ninjaPath = ninjaResult.detectedPath;
    
    ReportProgress("Detecting Vulkan SDK", 80);
    auto vulkanResult = DetectVulkanSDK();
    state.detections.push_back(vulkanResult);
    state.hasVulkanSDK = vulkanResult.isFound;
    state.vulkanSdkPath = vulkanResult.detectedPath;
    
    ReportProgress("Detecting CUDA", 90);
    auto cudaResult = DetectCUDA();
    state.detections.push_back(cudaResult);
    state.hasCUDA = cudaResult.isFound;
    state.cudaPath = cudaResult.detectedPath;
    
    // Determine optimal mode based on the freshly detected snapshot.
    state.mode = DetermineOptimalModeForState(state);
    
    return state;
}

DetectionResult TurnkeyConfigManager::DetectGit() {
    DetectionResult result;
    result.component = "Git";
    result.isRequired = true;
    
    // Check PATH
    std::string gitPath = DetectInPath("git.exe");
    if (!gitPath.empty()) {
        result.detectedPath = gitPath;
        result.version = GetVersionFromExecutable(gitPath);
        result.isFound = true;
        result.success = true;
        return result;
    }
    
    // Check common install locations
    std::vector<std::string> commonPaths = {
        "C:\\Program Files\\Git\\bin\\git.exe",
        "C:\\Program Files (x86)\\Git\\bin\\git.exe",
        "C:\\Git\\bin\\git.exe"
    };
    
    for (const auto& path : commonPaths) {
        if (CheckFileExists(path)) {
            result.detectedPath = path;
            result.version = GetVersionFromExecutable(path);
            result.isFound = true;
            result.success = true;
            return result;
        }
    }
    
    result.errorMessage = "Git not found in PATH or common locations";
    return result;
}

DetectionResult TurnkeyConfigManager::DetectPython() {
    DetectionResult result;
    result.component = "Python";
    result.isRequired = false; // Optional but recommended
    
    // Check for Python 3.x in PATH
    std::vector<std::string> pythonNames = {
        "python.exe", "python3.exe", "py.exe"
    };
    
    for (const auto& name : pythonNames) {
        std::string path = DetectInPath(name);
        if (!path.empty()) {
            result.detectedPath = path;
            result.version = GetVersionFromExecutable(path);
            result.isFound = true;
            result.success = true;
            return result;
        }
    }
    
    // Check Windows Store Python location
    std::string winStorePath = "C:\\Users\\" + std::string(getenv("USERNAME")) + 
                               "\\AppData\\Local\\Microsoft\\WindowsApps\\python.exe";
    if (CheckFileExists(winStorePath)) {
        result.detectedPath = winStorePath;
        result.version = GetVersionFromExecutable(winStorePath);
        result.isFound = true;
        result.success = true;
        return result;
    }
    
    result.errorMessage = "Python not found";
    return result;
}

DetectionResult TurnkeyConfigManager::DetectNodeJS() {
    DetectionResult result;
    result.component = "Node.js";
    result.isRequired = false;
    
    std::string nodePath = DetectInPath("node.exe");
    if (!nodePath.empty()) {
        result.detectedPath = nodePath;
        result.version = GetVersionFromExecutable(nodePath);
        result.isFound = true;
        result.success = true;
        return result;
    }
    
    // Check common install locations
    std::vector<std::string> commonPaths = {
        "C:\\Program Files\\nodejs\\node.exe",
        "C:\\Program Files (x86)\\nodejs\\node.exe"
    };
    
    for (const auto& path : commonPaths) {
        if (CheckFileExists(path)) {
            result.detectedPath = path;
            result.version = GetVersionFromExecutable(path);
            result.isFound = true;
            result.success = true;
            return result;
        }
    }
    
    result.errorMessage = "Node.js not found";
    return result;
}

DetectionResult TurnkeyConfigManager::DetectPowerShell() {
    DetectionResult result;
    result.component = "PowerShell 7+";
    result.isRequired = false;
    
    // Check for pwsh (PowerShell 7+)
    std::string pwshPath = DetectInPath("pwsh.exe");
    if (!pwshPath.empty()) {
        result.detectedPath = pwshPath;
        result.version = GetVersionFromExecutable(pwshPath);
        result.isFound = true;
        result.success = true;
        return result;
    }
    
    // Check standard PowerShell 7 location
    std::string ps7Path = "C:\\Program Files\\PowerShell\\7\\pwsh.exe";
    if (CheckFileExists(ps7Path)) {
        result.detectedPath = ps7Path;
        result.version = GetVersionFromExecutable(ps7Path);
        result.isFound = true;
        result.success = true;
        return result;
    }
    
    result.errorMessage = "PowerShell 7+ not found (Windows PowerShell 5.1 will be used)";
    return result;
}

DetectionResult TurnkeyConfigManager::DetectBuildTools() {
    DetectionResult result;
    result.component = "Visual Studio Build Tools";
    result.isRequired = true;
    
    // Check for MSBuild
    std::string msbuildPath = DetectInPath("MSBuild.exe");
    if (!msbuildPath.empty()) {
        result.detectedPath = msbuildPath;
        result.version = GetVersionFromExecutable(msbuildPath);
        result.isFound = true;
        result.success = true;
        return result;
    }
    
    // Check VS 2022 locations
    std::vector<std::string> vsPaths = {
        "C:\\VS2022Enterprise\\MSBuild\\Current\\Bin\\MSBuild.exe",
        "C:\\Program Files\\Microsoft Visual Studio\\2022\\Enterprise\\MSBuild\\Current\\Bin\\MSBuild.exe",
        "C:\\Program Files\\Microsoft Visual Studio\\2022\\Professional\\MSBuild\\Current\\Bin\\MSBuild.exe",
        "C:\\Program Files\\Microsoft Visual Studio\\2022\\Community\\MSBuild\\Current\\Bin\\MSBuild.exe"
    };
    
    for (const auto& path : vsPaths) {
        if (CheckFileExists(path)) {
            result.detectedPath = path;
            result.version = GetVersionFromExecutable(path);
            result.isFound = true;
            result.success = true;
            return result;
        }
    }
    
    result.errorMessage = "Visual Studio Build Tools not found";
    return result;
}

DetectionResult TurnkeyConfigManager::DetectCMake() {
    DetectionResult result;
    result.component = "CMake";
    result.isRequired = true;
    
    std::string cmakePath = DetectInPath("cmake.exe");
    if (!cmakePath.empty()) {
        result.detectedPath = cmakePath;
        result.version = GetVersionFromExecutable(cmakePath);
        result.isFound = true;
        result.success = true;
        return result;
    }
    
    // Check CMake install location
    std::string cmakeInstallPath = "C:\\Program Files\\CMake\\bin\\cmake.exe";
    if (CheckFileExists(cmakeInstallPath)) {
        result.detectedPath = cmakeInstallPath;
        result.version = GetVersionFromExecutable(cmakeInstallPath);
        result.isFound = true;
        result.success = true;
        return result;
    }
    
    result.errorMessage = "CMake not found";
    return result;
}

DetectionResult TurnkeyConfigManager::DetectNinja() {
    DetectionResult result;
    result.component = "Ninja";
    result.isRequired = false;
    
    std::string ninjaPath = DetectInPath("ninja.exe");
    if (!ninjaPath.empty()) {
        result.detectedPath = ninjaPath;
        result.version = GetVersionFromExecutable(ninjaPath);
        result.isFound = true;
        result.success = true;
        return result;
    }
    
    result.errorMessage = "Ninja not found (will use MSBuild)";
    return result;
}

DetectionResult TurnkeyConfigManager::DetectVulkanSDK() {
    DetectionResult result;
    result.component = "Vulkan SDK";
    result.isRequired = false;
    
    // Check VULKAN_SDK environment variable
    const char* vulkanSdk = getenv("VULKAN_SDK");
    if (vulkanSdk) {
        std::string vulkanPath = std::string(vulkanSdk) + "\\Bin\\vulkaninfo.exe";
        if (CheckFileExists(vulkanPath)) {
            result.detectedPath = vulkanSdk;
            result.version = GetVersionFromExecutable(vulkanPath);
            result.isFound = true;
            result.success = true;
            return result;
        }
    }
    
    // Check common Vulkan SDK locations
    std::vector<std::string> vulkanPaths = {
        "C:\\VulkanSDK\\1.4.328.1",
        "C:\\VulkanSDK\\1.3.290.0",
        "C:\\VulkanSDK\\1.3.268.0"
    };
    
    for (const auto& path : vulkanPaths) {
        std::string vulkanInfoPath = path + "\\Bin\\vulkaninfo.exe";
        if (CheckFileExists(vulkanInfoPath)) {
            result.detectedPath = path;
            result.version = GetVersionFromExecutable(vulkanInfoPath);
            result.isFound = true;
            result.success = true;
            return result;
        }
    }
    
    result.errorMessage = "Vulkan SDK not found (GPU acceleration disabled)";
    return result;
}

DetectionResult TurnkeyConfigManager::DetectCUDA() {
    DetectionResult result;
    result.component = "CUDA Toolkit";
    result.isRequired = false;
    
    // Check CUDA_PATH environment variable
    const char* cudaPath = getenv("CUDA_PATH");
    if (cudaPath) {
        std::string nvccPath = std::string(cudaPath) + "\\bin\\nvcc.exe";
        if (CheckFileExists(nvccPath)) {
            result.detectedPath = cudaPath;
            result.version = GetVersionFromExecutable(nvccPath);
            result.isFound = true;
            result.success = true;
            return result;
        }
    }
    
    // Check common CUDA locations
    std::vector<std::string> cudaPaths = {
        "C:\\Program Files\\NVIDIA GPU Computing Toolkit\\CUDA\\v12.8",
        "C:\\Program Files\\NVIDIA GPU Computing Toolkit\\CUDA\\v12.6",
        "C:\\Program Files\\NVIDIA GPU Computing Toolkit\\CUDA\\v12.4",
        "C:\\Program Files\\NVIDIA GPU Computing Toolkit\\CUDA\\v11.8"
    };
    
    for (const auto& path : cudaPaths) {
        std::string nvccPath = path + "\\bin\\nvcc.exe";
        if (CheckFileExists(nvccPath)) {
            result.detectedPath = path;
            result.version = GetVersionFromExecutable(nvccPath);
            result.isFound = true;
            result.success = true;
            return result;
        }
    }
    
    result.errorMessage = "CUDA Toolkit not found (CUDA acceleration disabled)";
    return result;
}

// ============================================================================
// Configuration Application
// ============================================================================
bool TurnkeyConfigManager::ApplyTurnkeyConfiguration() {
    ReportProgress("Applying Turnkey Configuration", 0);
    
    // Create directory structure
    if (!SetupDirectoryStructure()) {
        ReportError("ApplyTurnkeyConfiguration", "Failed to setup directory structure");
        return false;
    }
    
    ReportProgress("Creating Default Config Files", 50);
    
    // Create default configuration files
    if (!CreateDefaultConfigFiles()) {
        ReportError("ApplyTurnkeyConfiguration", "Failed to create default config files");
        return false;
    }
    
    ReportProgress("Configuring Feature Flags", 75);
    
    // Configure feature flags based on detection
    if (!ConfigureFeatureFlags()) {
        ReportError("ApplyTurnkeyConfiguration", "Failed to configure feature flags");
        return false;
    }
    
    m_state.autoConfigured = true;
    ReportProgress("Turnkey Configuration Applied", 100);
    
    return true;
}

bool TurnkeyConfigManager::CreateDefaultConfigFiles() {
    try {
        // Create main configuration JSON
        Json::Value config;
        config["version"] = m_state.configVersion;
        config["mode"] = static_cast<int>(m_state.mode);
        config["firstRun"] = m_state.firstRun;
        config["autoConfigured"] = m_state.autoConfigured;
        
        // Component paths
        Json::Value components;
        components["git"]["path"] = m_state.gitPath;
        components["git"]["enabled"] = m_state.hasGit;
        components["python"]["path"] = m_state.pythonPath;
        components["python"]["enabled"] = m_state.hasPython;
        components["nodejs"]["path"] = m_state.nodePath;
        components["nodejs"]["enabled"] = m_state.hasNodeJS;
        components["powershell"]["path"] = m_state.pwshPath;
        components["powershell"]["enabled"] = m_state.hasPowerShell7;
        components["msbuild"]["path"] = m_state.msbuildPath;
        components["msbuild"]["enabled"] = m_state.hasVSBuildTools;
        components["cmake"]["path"] = m_state.cmakePath;
        components["cmake"]["enabled"] = m_state.hasCMake;
        components["ninja"]["path"] = m_state.ninjaPath;
        components["ninja"]["enabled"] = m_state.hasNinja;
        components["vulkan"]["path"] = m_state.vulkanSdkPath;
        components["vulkan"]["enabled"] = m_state.hasVulkanSDK;
        components["cuda"]["path"] = m_state.cudaPath;
        components["cuda"]["enabled"] = m_state.hasCUDA;
        
        config["components"] = components;
        
        // Feature flags based on available components
        Json::Value features;
        features["gitIntegration"] = m_state.hasGit;
        features["pythonExtensions"] = m_state.hasPython;
        features["nodejsExtensions"] = m_state.hasNodeJS;
        features["powershellScripts"] = m_state.hasPowerShell7 || true; // Fallback to 5.1
        features["gpuAcceleration"] = m_state.hasVulkanSDK;
        features["cudaAcceleration"] = m_state.hasCUDA;
        features["fastBuild"] = m_state.hasNinja;
        
        config["features"] = features;
        
        // Write configuration file
        std::string configPath = GetTurnkeyConfigPath();
        fs::create_directories(fs::path(configPath).parent_path());
        
        std::ofstream file(configPath);
        if (!file.is_open()) {
            return false;
        }
        
        Json::StreamWriterBuilder builder;
        builder["indentation"] = "  ";
        std::unique_ptr<Json::StreamWriter> writer(builder.newStreamWriter());
        writer->write(config, &file);
        file.close();
        
        return true;
    } catch (const std::exception& e) {
        ReportError("CreateDefaultConfigFiles", e.what());
        return false;
    }
}

bool TurnkeyConfigManager::SetupDirectoryStructure() {
    try {
        // Get AppData path
        char appDataPath[MAX_PATH];
        if (SHGetFolderPathA(nullptr, CSIDL_LOCAL_APPDATA, nullptr, 0, appDataPath) != S_OK) {
            return false;
        }
        
        std::string basePath = std::string(appDataPath) + "\\RawrXD";
        m_state.configDir = basePath + "\\Config";
        m_state.cacheDir = basePath + "\\Cache";
        m_state.logsDir = basePath + "\\Logs";
        m_state.extensionsDir = basePath + "\\Extensions";
        
        // Create directories
        fs::create_directories(m_state.configDir);
        fs::create_directories(m_state.cacheDir);
        fs::create_directories(m_state.logsDir);
        fs::create_directories(m_state.extensionsDir);
        
        // Create subdirectories
        fs::create_directories(m_state.cacheDir + "\\Build");
        fs::create_directories(m_state.cacheDir + "\\Index");
        fs::create_directories(m_state.cacheDir + "\\Model");
        fs::create_directories(m_state.logsDir + "\\Crash");
        fs::create_directories(m_state.logsDir + "\\Session");
        
        return true;
    } catch (const std::exception& e) {
        ReportError("SetupDirectoryStructure", e.what());
        return false;
    }
}

bool TurnkeyConfigManager::ConfigureFeatureFlags() {
    // Feature flags are already set in CreateDefaultConfigFiles
    // This method can be extended for additional runtime configuration
    return true;
}

// ============================================================================
// Mode Management
// ============================================================================
TurnkeyMode TurnkeyConfigManager::DetermineOptimalModeForState(const TurnkeyState& state) const {
    // Count required components
    int requiredFound = 0;
    int requiredTotal = 0;
    
    for (const auto& detection : state.detections) {
        if (detection.isRequired) {
            requiredTotal++;
            if (detection.isFound) {
                requiredFound++;
            }
        }
    }

    // Fallback to direct required flags if no detection rows were loaded.
    if (requiredTotal == 0) {
        requiredTotal = 3; // Git, CMake, and Visual Studio Build Tools.
        if (state.hasGit) {
            requiredFound++;
        }
        if (state.hasCMake) {
            requiredFound++;
        }
        if (state.hasVSBuildTools) {
            requiredFound++;
        }
    }
    
    // Determine mode based on component availability
    if (requiredFound == requiredTotal) {
        // All required components found
        return TurnkeyMode::FullTurnkey;
    } else if (requiredFound >= requiredTotal / 2) {
        // Most required components found
        return TurnkeyMode::Assisted;
    } else if (requiredFound > 0) {
        // Some components found
        return TurnkeyMode::Manual;
    } else {
        // No required components found
        return TurnkeyMode::Degraded;
    }
}

TurnkeyMode TurnkeyConfigManager::DetermineOptimalMode() {
    return DetermineOptimalModeForState(m_state);
}

std::string TurnkeyConfigManager::GetModeDescription(TurnkeyMode mode) {
    switch (mode) {
        case TurnkeyMode::FullTurnkey:
            return "Full Turnkey - All features auto-configured and enabled";
        case TurnkeyMode::Assisted:
            return "Assisted - Most features available, some manual setup required";
        case TurnkeyMode::Manual:
            return "Manual - Significant manual configuration required";
        case TurnkeyMode::Degraded:
            return "Degraded - Running with reduced functionality";
        case TurnkeyMode::Recovery:
            return "Recovery - Recovery mode after configuration failure";
        default:
            return "Unknown - Configuration not yet determined";
    }
}

bool TurnkeyConfigManager::SwitchMode(TurnkeyMode newMode) {
    m_state.mode = newMode;
    return SaveConfiguration();
}

// ============================================================================
// Validation
// ============================================================================
bool TurnkeyConfigManager::ValidateConfiguration() {
    // Re-run detection to verify current state
    TurnkeyState currentState = DetectEnvironment();
    
    // Compare with stored state
    bool valid = true;
    
    for (size_t i = 0; i < m_state.detections.size() && i < currentState.detections.size(); ++i) {
        if (m_state.detections[i].isFound != currentState.detections[i].isFound) {
            valid = false;
            ReportError("ValidateConfiguration", 
                "Component " + m_state.detections[i].component + " state changed");
        }
    }
    
    return valid;
}

std::vector<std::string> TurnkeyConfigManager::GetMissingRequirements() {
    std::vector<std::string> missing;

    // Prefer deterministic required checks so persisted configurations without
    // detection rows still enforce required components.
    if (!m_state.hasGit) {
        missing.push_back("Git: not detected");
    }
    if (!m_state.hasCMake) {
        missing.push_back("CMake: not detected");
    }
    if (!m_state.hasVSBuildTools) {
        missing.push_back("Visual Studio Build Tools: not detected");
    }

    if (!missing.empty()) {
        return missing;
    }
    
    for (const auto& detection : m_state.detections) {
        if (detection.isRequired && !detection.isFound) {
            missing.push_back(detection.component + ": " + detection.errorMessage);
        }
    }
    
    return missing;
}

bool TurnkeyConfigManager::RunSmokeTests() {
    ReportProgress("Running Smoke Tests", 0);
    
    // Test 1: Configuration file exists and is valid
    ReportProgress("Testing Configuration File", 20);
    std::string configPath = GetTurnkeyConfigPath();
    if (!CheckFileExists(configPath)) {
        ReportError("SmokeTest", "Configuration file not found");
        return false;
    }
    
    // Test 2: Required components are accessible
    ReportProgress("Testing Component Accessibility", 40);
    for (const auto& detection : m_state.detections) {
        if (detection.isRequired && detection.isFound) {
            if (!CheckFileExists(detection.detectedPath)) {
                ReportError("SmokeTest", 
                    "Required component not accessible: " + detection.component);
                return false;
            }
        }
    }
    
    // Test 3: Directory structure is valid
    ReportProgress("Testing Directory Structure", 60);
    if (!fs::exists(m_state.configDir) ||
        !fs::exists(m_state.cacheDir) ||
        !fs::exists(m_state.logsDir)) {
        ReportError("SmokeTest", "Directory structure incomplete");
        return false;
    }
    
    // Test 4: Can write to directories
    ReportProgress("Testing Write Access", 80);
    try {
        std::string testFile = m_state.cacheDir + "\\.test_write";
        std::ofstream file(testFile);
        file << "test";
        file.close();
        fs::remove(testFile);
    } catch (const std::exception& e) {
        ReportError("SmokeTest", "Cannot write to cache directory");
        return false;
    }
    
    ReportProgress("Smoke Tests Complete", 100);
    return true;
}

// ============================================================================
// Persistence
// ============================================================================
bool TurnkeyConfigManager::SaveConfiguration() {
    return CreateDefaultConfigFiles();
}

bool TurnkeyConfigManager::LoadConfiguration() {
    try {
        std::string configPath = GetTurnkeyConfigPath();
        
        if (!CheckFileExists(configPath)) {
            return false;
        }
        
        std::ifstream file(configPath);
        if (!file.is_open()) {
            return false;
        }
        
        Json::Value config;
        file >> config;
        file.close();
        
        // Load state from JSON
        m_state.configVersion = config.get("version", "1.0.0").asString();
        m_state.mode = static_cast<TurnkeyMode>(config.get("mode", 0).asInt());
        m_state.firstRun = config.get("firstRun", false).asBool();
        m_state.autoConfigured = config.get("autoConfigured", false).asBool();
        
        // Load component paths
        const Json::Value& components = config["components"];
        m_state.gitPath = components["git"]["path"].asString();
        m_state.hasGit = components["git"]["enabled"].asBool();
        m_state.pythonPath = components["python"]["path"].asString();
        m_state.hasPython = components["python"]["enabled"].asBool();
        m_state.nodePath = components["nodejs"]["path"].asString();
        m_state.hasNodeJS = components["nodejs"]["enabled"].asBool();
        m_state.pwshPath = components["powershell"]["path"].asString();
        m_state.hasPowerShell7 = components["powershell"]["enabled"].asBool();
        m_state.msbuildPath = components["msbuild"]["path"].asString();
        m_state.hasVSBuildTools = components["msbuild"]["enabled"].asBool();
        m_state.cmakePath = components["cmake"]["path"].asString();
        m_state.hasCMake = components["cmake"]["enabled"].asBool();
        m_state.ninjaPath = components["ninja"]["path"].asString();
        m_state.hasNinja = components["ninja"]["enabled"].asBool();
        m_state.vulkanSdkPath = components["vulkan"]["path"].asString();
        m_state.hasVulkanSDK = components["vulkan"]["enabled"].asBool();
        m_state.cudaPath = components["cuda"]["path"].asString();
        m_state.hasCUDA = components["cuda"]["enabled"].asBool();

        // Recreate canonical detection rows used by validation routines.
        m_state.detections.clear();
        auto addDetection = [this](
            const std::string& component,
            const std::string& path,
            bool enabled,
            bool required,
            const std::string& errorMessage) {
            DetectionResult d;
            d.component = component;
            d.detectedPath = path;
            d.isFound = enabled;
            d.success = enabled;
            d.isRequired = required;
            d.errorMessage = enabled ? "" : errorMessage;
            m_state.detections.push_back(d);
        };

        addDetection("Git", m_state.gitPath, m_state.hasGit, true, "Git not found");
        addDetection("Python", m_state.pythonPath, m_state.hasPython, false, "Python not found");
        addDetection("Node.js", m_state.nodePath, m_state.hasNodeJS, false, "Node.js not found");
        addDetection("PowerShell 7+", m_state.pwshPath, m_state.hasPowerShell7, false, "PowerShell 7+ not found");
        addDetection("Visual Studio Build Tools", m_state.msbuildPath, m_state.hasVSBuildTools, true, "Visual Studio Build Tools not found");
        addDetection("CMake", m_state.cmakePath, m_state.hasCMake, true, "CMake not found");
        addDetection("Ninja", m_state.ninjaPath, m_state.hasNinja, false, "Ninja not found");
        addDetection("Vulkan SDK", m_state.vulkanSdkPath, m_state.hasVulkanSDK, false, "Vulkan SDK not found");
        addDetection("CUDA Toolkit", m_state.cudaPath, m_state.hasCUDA, false, "CUDA Toolkit not found");

        // Rehydrate directory paths used by smoke/validation checks.
        SetupDirectoryStructure();
        
        return true;
    } catch (const std::exception& e) {
        ReportError("LoadConfiguration", e.what());
        return false;
    }
}

bool TurnkeyConfigManager::ResetToDefaults() {
    m_state = TurnkeyState();
    m_state.firstRun = true;
    m_state.mode = TurnkeyMode::Unknown;
    
    return ApplyTurnkeyConfiguration();
}

// ============================================================================
// Helper Methods
// ============================================================================
std::string TurnkeyConfigManager::DetectInPath(const std::string& executable) {
    char pathBuffer[MAX_PATH];
    if (SearchPathA(nullptr, executable.c_str(), nullptr, MAX_PATH, pathBuffer, nullptr)) {
        return std::string(pathBuffer);
    }
    return "";
}

std::string TurnkeyConfigManager::DetectInRegistry(const std::string& key, const std::string& value) {
    HKEY hKey;
    char buffer[MAX_PATH];
    DWORD bufferSize = MAX_PATH;
    
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, key.c_str(), 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        if (RegQueryValueExA(hKey, value.c_str(), nullptr, nullptr, 
                             reinterpret_cast<LPBYTE>(buffer), &bufferSize) == ERROR_SUCCESS) {
            RegCloseKey(hKey);
            return std::string(buffer);
        }
        RegCloseKey(hKey);
    }
    
    return "";
}

std::string TurnkeyConfigManager::DetectInProgramFiles(const std::string& subPath) {
    char programFiles[MAX_PATH];
    if (SHGetFolderPathA(nullptr, CSIDL_PROGRAM_FILES, nullptr, 0, programFiles) == S_OK) {
        std::string fullPath = std::string(programFiles) + "\\" + subPath;
        if (CheckFileExists(fullPath)) {
            return fullPath;
        }
    }
    return "";
}

std::string TurnkeyConfigManager::GetVersionFromExecutable(const std::string& path) {
    DWORD handle = 0;
    DWORD size = GetFileVersionInfoSizeA(path.c_str(), &handle);
    
    if (size == 0) {
        return "unknown";
    }
    
    std::vector<BYTE> versionInfo(size);
    if (!GetFileVersionInfoA(path.c_str(), handle, size, versionInfo.data())) {
        return "unknown";
    }
    
    VS_FIXEDFILEINFO* fileInfo = nullptr;
    UINT fileInfoSize = 0;
    
    if (VerQueryValueA(versionInfo.data(), "\\", reinterpret_cast<LPVOID*>(&fileInfo), &fileInfoSize)) {
        if (fileInfoSize >= sizeof(VS_FIXEDFILEINFO)) {
            std::stringstream version;
            version << HIWORD(fileInfo->dwFileVersionMS) << "."
                   << LOWORD(fileInfo->dwFileVersionMS) << "."
                   << HIWORD(fileInfo->dwFileVersionLS) << "."
                   << LOWORD(fileInfo->dwFileVersionLS);
            return version.str();
        }
    }
    
    return "unknown";
}

bool TurnkeyConfigManager::CheckFileExists(const std::string& path) {
    return fs::exists(path) && fs::is_regular_file(path);
}

void TurnkeyConfigManager::ReportProgress(const std::string& stage, int percent) {
    if (m_progressCallback) {
        m_progressCallback(stage, percent);
    }
}

void TurnkeyConfigManager::ReportError(const std::string& component, const std::string& error) {
    if (m_errorCallback) {
        m_errorCallback(component, error);
    }
}

// ============================================================================
// Utility Functions
// ============================================================================
std::string GetTurnkeyConfigPath() {
    char appDataPath[MAX_PATH];
    if (SHGetFolderPathA(nullptr, CSIDL_LOCAL_APPDATA, nullptr, 0, appDataPath) == S_OK) {
        return std::string(appDataPath) + "\\RawrXD\\Config\\turnkey.json";
    }
    return "";
}

std::string GetTurnkeyCachePath() {
    char appDataPath[MAX_PATH];
    if (SHGetFolderPathA(nullptr, CSIDL_LOCAL_APPDATA, nullptr, 0, appDataPath) == S_OK) {
        return std::string(appDataPath) + "\\RawrXD\\Cache";
    }
    return "";
}

bool IsRunningInTurnkeyMode() {
    TurnkeyConfigManager* mgr = GetTurnkeyConfigManager();
    if (mgr) {
        TurnkeyMode mode = mgr->GetCurrentMode();
        return mode == TurnkeyMode::FullTurnkey || mode == TurnkeyMode::Assisted;
    }
    return false;
}

bool RequireComponent(const std::string& component) {
    TurnkeyConfigManager* mgr = GetTurnkeyConfigManager();
    if (!mgr) {
        return false;
    }
    
    const TurnkeyState& state = mgr->GetState();
    
    if (component == "git") return state.hasGit;
    if (component == "python") return state.hasPython;
    if (component == "nodejs") return state.hasNodeJS;
    if (component == "powershell7") return state.hasPowerShell7;
    if (component == "msbuild") return state.hasVSBuildTools;
    if (component == "cmake") return state.hasCMake;
    if (component == "ninja") return state.hasNinja;
    if (component == "vulkan") return state.hasVulkanSDK;
    if (component == "cuda") return state.hasCUDA;
    
    return false;
}

} // namespace config
} // namespace rawrxd
