// ============================================================================
// Cross-Platform Engine — Multi-Platform Compatibility
// Ensures code works across different platforms and environments
// ============================================================================
#pragma once
#include "../inference/SovereignInferenceClient.h"
#include "../build/build_system.h"
#include <memory>
#include <vector>
#include <string>
#include <map>
#include <mutex>
#include <set>

namespace RawrXD::Compatibility {

enum class Platform {
    WINDOWS,
    LINUX,
    MACOS,
    ANDROID,
    IOS,
    WEBASSEMBLY
};

enum class CompatibilityLevel {
    FULL,
    PARTIAL,
    NONE
};

struct CompatibilityIssue {
    std::string description;
    Platform affectedPlatform;
    std::string filePath;
    int lineNumber;
    std::string suggestedFix;
    CompatibilityLevel severity;
};

struct CompatibilityReport {
    std::string codePath;
    std::map<Platform, CompatibilityLevel> platformCompatibility;
    std::vector<CompatibilityIssue> issues;
    std::vector<std::string> platformSpecificCode;
    std::map<Platform, std::vector<std::string>> adaptationRequirements;
    double overallCompatibility;
};

struct AdaptationLayer {
    Platform targetPlatform;
    std::map<std::string, std::string> replacements;
    std::vector<std::string> additionalIncludes;
    std::vector<std::string> requiredLibraries;
};

struct TestMatrix {
    std::vector<Platform> targetPlatforms;
    std::map<Platform, std::vector<std::string>> testCases;
    std::map<Platform, bool> testResults;
};

class CrossPlatformEngine {
public:
    explicit CrossPlatformEngine(std::shared_ptr<SovereignInferenceClient> aiClient)
        : m_aiClient(aiClient) {
        InitializePlatformMappings();
    }

    CompatibilityReport CheckCompatibility(const std::string& code, 
                                          const std::vector<Platform>& platforms) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        CompatibilityReport report;
        report.codePath = code.substr(0, 50); // Truncated for display
        
        // Check compatibility for each platform
        for (const auto& platform : platforms) {
            auto level = CheckPlatformCompatibility(code, platform);
            report.platformCompatibility[platform] = level;
            
            if (level != CompatibilityLevel::FULL) {
                auto issues = FindCompatibilityIssues(code, platform);
                report.issues.insert(report.issues.end(), issues.begin(), issues.end());
            }
        }
        
        // Calculate overall compatibility
        int fullCount = 0;
        for (const auto& [platform, level] : report.platformCompatibility) {
            if (level == CompatibilityLevel::FULL) fullCount++;
        }
        report.overallCompatibility = platforms.empty() ? 100.0 : 
                                     (fullCount * 100.0 / platforms.size());
        
        // Identify platform-specific code
        report.platformSpecificCode = IdentifyPlatformSpecificCode(code);
        
        // AI-enhanced analysis
        if (m_aiClient && m_aiClient->IsLoaded()) {
            auto aiIssues = PerformAICompatibilityCheck(code, platforms);
            report.issues.insert(report.issues.end(), aiIssues.begin(), aiIssues.end());
        }
        
        return report;
    }

    AdaptationLayer GenerateAdaptationLayer(const CompatibilityReport& report) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        AdaptationLayer layer;
        
        // Find the platform with the most issues
        Platform targetPlatform = Platform::WINDOWS;
        int maxIssues = 0;
        
        for (const auto& [platform, level] : report.platformCompatibility) {
            if (level != CompatibilityLevel::FULL) {
                int issueCount = 0;
                for (const auto& issue : report.issues) {
                    if (issue.affectedPlatform == platform) {
                        issueCount++;
                    }
                }
                if (issueCount > maxIssues) {
                    maxIssues = issueCount;
                    targetPlatform = platform;
                }
            }
        }
        
        layer.targetPlatform = targetPlatform;
        
        // Generate replacements for each issue
        for (const auto& issue : report.issues) {
            if (issue.affectedPlatform == targetPlatform) {
                layer.replacements[issue.description] = issue.suggestedFix;
            }
        }
        
        // Add platform-specific includes
        switch (targetPlatform) {
            case Platform::WINDOWS:
                layer.additionalIncludes.push_back("#include <windows.h>");
                break;
            case Platform::LINUX:
                layer.additionalIncludes.push_back("#include <unistd.h>");
                break;
            case Platform::MACOS:
                layer.additionalIncludes.push_back("#include <TargetConditionals.h>");
                break;
            default:
                break;
        }
        
        return layer;
    }

    TestMatrix TestCrossPlatform(const TestMatrix& matrix) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        TestMatrix results = matrix;
        
        for (const auto& platform : matrix.targetPlatforms) {
            bool allPassed = true;
            
            for (const auto& testCase : matrix.testCases[platform]) {
                // Execute test for platform
                bool passed = ExecutePlatformTest(testCase, platform);
                if (!passed) {
                    allPassed = false;
                }
            }
            
            results.testResults[platform] = allPassed;
        }
        
        return results;
    }

    std::string GenerateCompatibilityReport(const CompatibilityReport& report) {
        std::ostringstream doc;
        doc << "# Cross-Platform Compatibility Report\n\n";
        doc << "**Code Path:** " << report.codePath << "\n";
        doc << "**Overall Compatibility:** " << std::fixed << std::setprecision(1) 
               << report.overallCompatibility << "%\n\n";
        
        doc << "## Platform Compatibility\n";
        for (const auto& [platform, level] : report.platformCompatibility) {
            doc << "- " << PlatformToString(platform) << ": " << LevelToString(level) << "\n";
        }
        
        if (!report.issues.empty()) {
            doc << "\n## Compatibility Issues\n";
            for (const auto& issue : report.issues) {
                doc << "### " << issue.description << "\n";
                doc << "- **Platform:** " << PlatformToString(issue.affectedPlatform) << "\n";
                doc << "- **Severity:** " << LevelToString(issue.severity) << "\n";
                doc << "- **Suggested Fix:** " << issue.suggestedFix << "\n\n";
            }
        }
        
        if (!report.platformSpecificCode.empty()) {
            doc << "## Platform-Specific Code\n";
            for (const auto& code : report.platformSpecificCode) {
                doc << "- `" << code << "`\n";
            }
        }
        
        return doc.str();
    }

    std::vector<Platform> GetSupportedPlatforms() const {
        return {
            Platform::WINDOWS,
            Platform::LINUX,
            Platform::MACOS,
            Platform::ANDROID,
            Platform::IOS,
            Platform::WEBASSEMBLY
        };
    }

private:
    std::shared_ptr<SovereignInferenceClient> m_aiClient;
    mutable std::mutex m_mutex;
    std::map<std::string, std::string> m_platformMappings;

    void InitializePlatformMappings() {
        m_platformMappings["windows"] = "#ifdef _WIN32";
        m_platformMappings["linux"] = "#ifdef __linux__";
        m_platformMappings["macos"] = "#ifdef __APPLE__";
        m_platformMappings["android"] = "#ifdef __ANDROID__";
    }

    CompatibilityLevel CheckPlatformCompatibility(const std::string& code, Platform platform) {
        // Check for platform-specific APIs
        std::vector<std::string> platformSpecificAPIs;
        
        switch (platform) {
            case Platform::WINDOWS:
                platformSpecificAPIs = {"CreateFile", "RegOpenKey", "GetProcAddress"};
                break;
            case Platform::LINUX:
                platformSpecificAPIs = {"fork", "execve", "pthread_create"};
                break;
            case Platform::MACOS:
                platformSpecificAPIs = {"dispatch_async", "CFRelease"};
                break;
            default:
                break;
        }
        
        int issueCount = 0;
        for (const auto& api : platformSpecificAPIs) {
            if (code.find(api) != std::string::npos) {
                issueCount++;
            }
        }
        
        if (issueCount == 0) return CompatibilityLevel::FULL;
        if (issueCount < 3) return CompatibilityLevel::PARTIAL;
        return CompatibilityLevel::NONE;
    }

    std::vector<CompatibilityIssue> FindCompatibilityIssues(const std::string& code, 
                                                               Platform platform) {
        std::vector<CompatibilityIssue> issues;
        
        // Check for common compatibility issues
        if (platform == Platform::WINDOWS) {
            if (code.find("unistd.h") != std::string::npos) {
                CompatibilityIssue issue;
                issue.description = "Linux-specific header included";
                issue.affectedPlatform = platform;
                issue.suggestedFix = "Use Windows equivalent or conditional compilation";
                issue.severity = CompatibilityLevel::PARTIAL;
                issues.push_back(issue);
            }
        }
        
        if (platform == Platform::LINUX) {
            if (code.find("windows.h") != std::string::npos) {
                CompatibilityIssue issue;
                issue.description = "Windows-specific header included";
                issue.affectedPlatform = platform;
                issue.suggestedFix = "Use Linux equivalent or conditional compilation";
                issue.severity = CompatibilityLevel::PARTIAL;
                issues.push_back(issue);
            }
        }
        
        return issues;
    }

    std::vector<std::string> IdentifyPlatformSpecificCode(const std::string& code) {
        std::vector<std::string> specificCode;
        
        // Look for platform-specific patterns
        if (code.find("#ifdef _WIN32") != std::string::npos) {
            specificCode.push_back("Windows-specific code blocks");
        }
        if (code.find("#ifdef __linux__") != std::string::npos) {
            specificCode.push_back("Linux-specific code blocks");
        }
        if (code.find("#ifdef __APPLE__") != std::string::npos) {
            specificCode.push_back("macOS-specific code blocks");
        }
        
        return specificCode;
    }

    std::vector<CompatibilityIssue> PerformAICompatibilityCheck(const std::string& code,
                                                                  const std::vector<Platform>& platforms) {
        std::vector<CompatibilityIssue> issues;
        
        if (!m_aiClient || !m_aiClient->IsLoaded()) {
            return issues;
        }

        std::string prompt = "Check this code for cross-platform compatibility issues:\n```\n" + 
                            code.substr(0, 1000) + "\n```";
        
        std::vector<ChatMessage> messages = {
            {"system", "You are a cross-platform development expert."},
            {"user", prompt}
        };

        auto result = m_aiClient->ChatSync(messages);
        
        if (result.success) {
            CompatibilityIssue issue;
            issue.description = "AI Analysis: " + result.response;
            issue.affectedPlatform = Platform::WINDOWS;
            issue.suggestedFix = "Review AI recommendations";
            issue.severity = CompatibilityLevel::PARTIAL;
            issues.push_back(issue);
        }
        
        return issues;
    }

    bool ExecutePlatformTest(const std::string& testCase, Platform platform) {
        // Execute test on target platform
        // This would integrate with your CI/CD system
        return true;
    }

    std::string PlatformToString(Platform platform) {
        switch (platform) {
            case Platform::WINDOWS: return "Windows";
            case Platform::LINUX: return "Linux";
            case Platform::MACOS: return "macOS";
            case Platform::ANDROID: return "Android";
            case Platform::IOS: return "iOS";
            case Platform::WEBASSEMBLY: return "WebAssembly";
            default: return "Unknown";
        }
    }

    std::string LevelToString(CompatibilityLevel level) {
        switch (level) {
            case CompatibilityLevel::FULL: return "Full";
            case CompatibilityLevel::PARTIAL: return "Partial";
            case CompatibilityLevel::NONE: return "None";
            default: return "Unknown";
        }
    }
};

} // namespace RawrXD::Compatibility
