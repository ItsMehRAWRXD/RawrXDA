// turnkey_validation.h — RawrXD IDE Turnkey/Un-Turnkey Validation System
// Final integration test suite for 14-Day Sprint

#pragma once
#ifndef RAWRXD_TURNKEY_VALIDATION_H
#define RAWRXD_TURNKEY_VALIDATION_H

#include <windows.h>
#include <string>
#include <vector>
#include <functional>
#include <chrono>

namespace rawrxd {
namespace validation {

// ============================================================================
// Test Result Enumeration
// ============================================================================
enum class TestStatus {
    NotRun,
    Running,
    Passed,
    Failed,
    Skipped,
    Warning
};

// ============================================================================
// Validation Test Result
// ============================================================================
struct ValidationResult {
    std::string testName;
    std::string category;
    TestStatus status{TestStatus::NotRun};
    std::chrono::milliseconds duration{0};
    std::string message;
    std::string details;
    bool isCritical{false};
    
    // Constructor helpers
    static ValidationResult Pass(const std::string& name, const std::string& msg = "");
    static ValidationResult Fail(const std::string& name, const std::string& msg);
    static ValidationResult Skip(const std::string& name, const std::string& reason);
    static ValidationResult Warn(const std::string& name, const std::string& msg);
};

// ============================================================================
// Validation Suite Configuration
// ============================================================================
struct ValidationConfig {
    bool runTurnkeyTests{true};
    bool runUnturnkeyTests{true};
    bool runIntegrationTests{true};
    bool runPerformanceTests{true};
    bool runSecurityTests{true};
    bool stopOnCriticalFailure{true};
    int timeoutSeconds{300};
    std::string outputFormat{"json"}; // json, xml, console
    std::string outputPath;
};

// ============================================================================
// Turnkey Validation Suite
// ============================================================================
class TurnkeyValidationSuite {
public:
    TurnkeyValidationSuite();
    ~TurnkeyValidationSuite();

    // Configuration
    void SetConfig(const ValidationConfig& config);
    const ValidationConfig& GetConfig() const { return m_config; }
    
    // Test Execution
    bool RunAllTests();
    bool RunTurnkeyTests();
    bool RunUnturnkeyTests();
    bool RunIntegrationTests();
    bool RunPerformanceTests();
    bool RunSecurityTests();
    
    // Individual Test Categories
    std::vector<ValidationResult> TestTurnkeyConfiguration();
    std::vector<ValidationResult> TestComponentDetection();
    std::vector<ValidationResult> TestFeatureAvailability();
    std::vector<ValidationResult> TestGracefulDegradation();
    std::vector<ValidationResult> TestFirstRunExperience();
    std::vector<ValidationResult> TestConfigurationPersistence();
    std::vector<ValidationResult> TestModeSwitching();
    std::vector<ValidationResult> TestIntegrationWithSubsystems();
    std::vector<ValidationResult> TestPerformanceBenchmarks();
    std::vector<ValidationResult> TestSecurityBoundaries();
    
    // Results
    const std::vector<ValidationResult>& GetResults() const { return m_results; }
    int GetPassCount() const;
    int GetFailCount() const;
    int GetSkipCount() const;
    int GetWarningCount() const;
    bool HasCriticalFailures() const;
    
    // Reporting
    bool GenerateReport();
    std::string GetConsoleReport() const;
    std::string GetJsonReport() const;
    std::string GetXmlReport() const;
    
    // Callbacks
    using TestStartCallback = std::function<void(const std::string& testName)>;
    using TestCompleteCallback = std::function<void(const ValidationResult& result)>;
    using ProgressCallback = std::function<void(int current, int total, const std::string& status)>;
    
    void SetTestStartCallback(TestStartCallback cb) { m_testStartCallback = cb; }
    void SetTestCompleteCallback(TestCompleteCallback cb) { m_testCompleteCallback = cb; }
    void SetProgressCallback(ProgressCallback cb) { m_progressCallback = cb; }

private:
    ValidationConfig m_config;
    std::vector<ValidationResult> m_results;
    
    TestStartCallback m_testStartCallback;
    TestCompleteCallback m_testCompleteCallback;
    ProgressCallback m_progressCallback;
    
    // Helper methods
    void RecordResult(const ValidationResult& result);
    void ReportProgress(int current, int total, const std::string& status);
    void ReportTestStart(const std::string& testName);
    void ReportTestComplete(const ValidationResult& result);
    
    // Individual tests
    ValidationResult TestTurnkeyModeDetection();
    ValidationResult TestComponentPathResolution();
    ValidationResult TestRequiredComponentsPresent();
    ValidationResult TestOptionalComponentsHandled();
    ValidationResult TestConfigurationFileCreation();
    ValidationResult TestDirectoryStructureCreation();
    ValidationResult TestFeatureFlagConfiguration();
    ValidationResult TestMissingComponentFallback();
    ValidationResult TestDegradedModeOperation();
    ValidationResult TestRecoveryModeOperation();
    ValidationResult TestConfigurationReload();
    ValidationResult TestConfigurationMigration();
    ValidationResult TestAgentIntegration();
    ValidationResult TestExtensionHostIntegration();
    ValidationResult TestLSPIntegration();
    ValidationResult TestBuildSystemIntegration();
    ValidationResult TestStartupTime();
    ValidationResult TestConfigurationLoadTime();
    ValidationResult TestComponentDetectionTime();
    ValidationResult TestMemoryUsage();
    ValidationResult TestSandboxEnforcement();
    ValidationResult TestPathValidation();
    ValidationResult TestExecutableVerification();
};

// ============================================================================
// Turnkey vs Un-Turnkey Comparison
// ============================================================================
struct ModeComparison {
    std::string feature;
    bool turnkeyAvailable;
    bool unturnkeyAvailable;
    std::string turnkeyNotes;
    std::string unturnkeyNotes;
    bool parityAchieved;
};

class TurnkeyComparisonAnalyzer {
public:
    std::vector<ModeComparison> AnalyzeFeatureParity();
    std::vector<ModeComparison> GetCriticalDifferences();
    bool IsFeatureParityAchieved() const;
    std::string GenerateComparisonReport() const;
};

// ============================================================================
// Global Access
// ============================================================================
TurnkeyValidationSuite* GetValidationSuite();
bool RunTurnkeyValidation();
bool RunQuickSmokeTest();

} // namespace validation
} // namespace rawrxd

#endif // RAWRXD_TURNKEY_VALIDATION_H
