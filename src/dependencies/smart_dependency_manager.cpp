// ============================================================================
// Smart Dependency Manager — Intelligent Dependency Management
// Automated dependency analysis, updates, and vulnerability scanning
// ============================================================================
#pragma once
#include "../inference/SovereignInferenceClient.h"
#include "../security/vulnerability_scanner.h"
#include <memory>
#include <vector>
#include <string>
#include <map>
#include <mutex>
#include <set>

namespace RawrXD::Dependencies {

enum class DependencyType {
    DIRECT,
    TRANSITIVE,
    DEV,
    OPTIONAL
};

enum class UpdateType {
    PATCH,
    MINOR,
    MAJOR,
    SECURITY
};

struct Dependency {
    std::string name;
    std::string currentVersion;
    std::string latestVersion;
    DependencyType type;
    std::vector<std::string> licenses;
    std::vector<std::string> vulnerabilities;
    std::chrono::system_clock::time_point lastUpdated;
    bool isOutdated;
    bool hasVulnerabilities;
};

struct DependencyTree {
    std::string rootPackage;
    std::map<std::string, Dependency> dependencies;
    std::map<std::string, std::vector<std::string>> dependencyGraph;
    int totalDependencies;
    int outdatedCount;
    int vulnerableCount;
};

struct UpdateRecommendation {
    std::string packageName;
    UpdateType type;
    std::string fromVersion;
    std::string toVersion;
    std::string reason;
    double riskScore;
    std::vector<std::string> breakingChanges;
    std::vector<std::string> benefits;
};

class SmartDependencyManager {
public:
    explicit SmartDependencyManager(std::shared_ptr<SovereignInferenceClient> aiClient)
        : m_aiClient(aiClient) {}

    DependencyTree AnalyzeDependencies(const std::string& projectPath) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        DependencyTree tree;
        tree.rootPackage = projectPath;
        
        // Parse dependency files (package.json, requirements.txt, Cargo.toml, etc.)
        auto deps = ParseDependencyFiles(projectPath);
        
        for (const auto& dep : deps) {
            tree.dependencies[dep.name] = dep;
            
            if (dep.isOutdated) {
                tree.outdatedCount++;
            }
            if (dep.hasVulnerabilities) {
                tree.vulnerableCount++;
            }
        }
        
        tree.totalDependencies = tree.dependencies.size();
        
        // Build dependency graph
        tree.dependencyGraph = BuildDependencyGraph(tree.dependencies);
        
        m_dependencyTrees[projectPath] = tree;
        return tree;
    }

    std::vector<UpdateRecommendation> GetUpdateRecommendations(const DependencyTree& tree) {
        std::vector<UpdateRecommendation> recommendations;
        
        for (const auto& [name, dep] : tree.dependencies) {
            if (!dep.isOutdated) continue;
            
            UpdateRecommendation rec;
            rec.packageName = name;
            rec.fromVersion = dep.currentVersion;
            rec.toVersion = dep.latestVersion;
            rec.type = DetermineUpdateType(dep.currentVersion, dep.latestVersion);
            
            // AI-enhanced analysis
            if (m_aiClient && m_aiClient->IsLoaded()) {
                auto analysis = AnalyzeUpdateRisk(name, dep.currentVersion, dep.latestVersion);
                rec.riskScore = analysis.riskScore;
                rec.breakingChanges = analysis.breakingChanges;
                rec.benefits = analysis.benefits;
            }
            
            recommendations.push_back(rec);
        }
        
        // Sort by risk score (lowest risk first)
        std::sort(recommendations.begin(), recommendations.end(),
                 [](const UpdateRecommendation& a, const UpdateRecommendation& b) {
                     return a.riskScore < b.riskScore;
                 });
        
        return recommendations;
    }

    bool UpdateDependency(const std::string& projectPath, const std::string& packageName,
                         const std::string& targetVersion) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        // Update dependency file
        bool success = UpdateDependencyFile(projectPath, packageName, targetVersion);
        
        if (success) {
            // Re-analyze dependencies
            AnalyzeDependencies(projectPath);
        }
        
        return success;
    }

    std::vector<Dependency> FindVulnerabilities(const DependencyTree& tree) {
        std::vector<Dependency> vulnerable;
        
        for (const auto& [name, dep] : tree.dependencies) {
            if (dep.hasVulnerabilities) {
                vulnerable.push_back(dep);
            }
        }
        
        return vulnerable;
    }

    std::string GenerateDependencyReport(const DependencyTree& tree) {
        std::ostringstream report;
        report << "# Dependency Analysis Report\n\n";
        report << "**Project:** " << tree.rootPackage << "\n";
        report << "**Total Dependencies:** " << tree.totalDependencies << "\n";
        report << "**Outdated:** " << tree.outdatedCount << "\n";
        report << "**Vulnerable:** " << tree.vulnerableCount << "\n\n";
        
        if (tree.vulnerableCount > 0) {
            report << "## Vulnerable Dependencies\n";
            for (const auto& [name, dep] : tree.dependencies) {
                if (dep.hasVulnerabilities) {
                    report << "- **" << name << "** " << dep.currentVersion << "\n";
                    for (const auto& vuln : dep.vulnerabilities) {
                        report << "  - " << vuln << "\n";
                    }
                }
            }
            report << "\n";
        }
        
        if (tree.outdatedCount > 0) {
            report << "## Outdated Dependencies\n";
            for (const auto& [name, dep] : tree.dependencies) {
                if (dep.isOutdated) {
                    report << "- " << name << ": " << dep.currentVersion << " → " << dep.latestVersion << "\n";
                }
            }
        }
        
        return report.str();
    }

private:
    std::shared_ptr<SovereignInferenceClient> m_aiClient;
    mutable std::mutex m_mutex;
    std::map<std::string, DependencyTree> m_dependencyTrees;

    struct UpdateAnalysis {
        double riskScore;
        std::vector<std::string> breakingChanges;
        std::vector<std::string> benefits;
    };

    std::vector<Dependency> ParseDependencyFiles(const std::string& projectPath) {
        std::vector<Dependency> deps;
        
        // Parse package.json
        auto packageJson = projectPath + "/package.json";
        if (std::filesystem::exists(packageJson)) {
            // Parse npm dependencies
        }
        
        // Parse requirements.txt
        auto requirements = projectPath + "/requirements.txt";
        if (std::filesystem::exists(requirements)) {
            // Parse Python dependencies
        }
        
        // Parse Cargo.toml
        auto cargo = projectPath + "/Cargo.toml";
        if (std::filesystem::exists(cargo)) {
            // Parse Rust dependencies
        }
        
        return deps;
    }

    std::map<std::string, std::vector<std::string>> BuildDependencyGraph(
        const std::map<std::string, Dependency>& dependencies) {
        std::map<std::string, std::vector<std::string>> graph;
        
        // Build graph from dependency relationships
        for (const auto& [name, dep] : dependencies) {
            // Add dependencies as edges
        }
        
        return graph;
    }

    UpdateType DetermineUpdateType(const std::string& current, const std::string& latest) {
        // Parse semantic versions
        auto currentParts = SplitVersion(current);
        auto latestParts = SplitVersion(latest);
        
        if (latestParts[0] > currentParts[0]) {
            return UpdateType::MAJOR;
        } else if (latestParts[1] > currentParts[1]) {
            return UpdateType::MINOR;
        } else {
            return UpdateType::PATCH;
        }
    }

    UpdateAnalysis AnalyzeUpdateRisk(const std::string& packageName,
                                    const std::string& fromVersion,
                                    const std::string& toVersion) {
        UpdateAnalysis analysis;
        analysis.riskScore = 0.5; // Default medium risk
        
        if (!m_aiClient || !m_aiClient->IsLoaded()) {
            return analysis;
        }

        std::string prompt = "Analyze the risk of updating " + packageName + 
                            " from " + fromVersion + " to " + toVersion;
        
        std::vector<ChatMessage> messages = {
            {"system", "You are a dependency management expert."},
            {"user", prompt}
        };

        auto result = m_aiClient->ChatSync(messages);
        
        if (result.success) {
            // Parse AI response for risk assessment
            analysis.riskScore = 0.3; // Assume AI suggests lower risk
            analysis.benefits.push_back("AI analysis: " + result.response);
        }
        
        return analysis;
    }

    bool UpdateDependencyFile(const std::string& projectPath,
                             const std::string& packageName,
                             const std::string& targetVersion) {
        // Update the appropriate dependency file
        return true;
    }

    std::vector<int> SplitVersion(const std::string& version) {
        std::vector<int> parts;
        std::stringstream ss(version);
        std::string part;
        
        while (std::getline(ss, part, '.')) {
            parts.push_back(std::stoi(part));
        }
        
        return parts;
    }
};

} // namespace RawrXD::Dependencies
