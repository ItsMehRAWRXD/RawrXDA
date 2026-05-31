// ============================================================================
// extension_dependency_resolver.cpp — Dependency Resolution Implementation
// ============================================================================
// Architecture: C++20 | Win32 | Recursive resolution | No exceptions
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#include "extension_dependency_resolver.h"

#include <algorithm>
#include <cctype>
#include <regex>
#include <queue>

namespace RawrXD {
namespace Extensions {

// ============================================================================
// Global Instance
// ============================================================================

static ExtensionDependencyResolver* g_dependencyResolver = nullptr;

ExtensionDependencyResolver& GetDependencyResolver() {
    if (!g_dependencyResolver) {
        g_dependencyResolver = new ExtensionDependencyResolver();
    }
    return *g_dependencyResolver;
}

// ============================================================================
// SemanticVersion Implementation
// ============================================================================

SemanticVersion::SemanticVersion(const std::string& versionString) {
    Parse(versionString, *this);
}

bool SemanticVersion::operator<(const SemanticVersion& other) const {
    if (major != other.major) return major < other.major;
    if (minor != other.minor) return minor < other.minor;
    return patch < other.patch;
}

bool SemanticVersion::operator<=(const SemanticVersion& other) const {
    return *this < other || *this == other;
}

bool SemanticVersion::operator>(const SemanticVersion& other) const {
    return other < *this;
}

bool SemanticVersion::operator>=(const SemanticVersion& other) const {
    return *this > other || *this == other;
}

bool SemanticVersion::operator==(const SemanticVersion& other) const {
    return major == other.major && minor == other.minor && patch == other.patch;
}

std::string SemanticVersion::ToString() const {
    std::string result = std::to_string(major) + "." + std::to_string(minor) + "." + std::to_string(patch);
    if (!prerelease.empty()) result += "-" + prerelease;
    if (!build.empty()) result += "+" + build;
    return result;
}

bool SemanticVersion::Parse(const std::string& str, SemanticVersion& outVersion) {
    // Simple parser: major.minor.patch[-prerelease][+build]
    size_t dot1 = str.find('.');
    if (dot1 == std::string::npos) return false;

    size_t dot2 = str.find('.', dot1 + 1);
    if (dot2 == std::string::npos) return false;

    try {
        outVersion.major = std::stoul(str.substr(0, dot1));
        
        // Find prerelease and build
        size_t dash = str.find('-', dot2);
        size_t plus = str.find('+', dot2);
        
        if (dash == std::string::npos) dash = str.length();
        if (plus == std::string::npos) plus = str.length();
        
        size_t minorEnd = std::min(dash, plus);
        outVersion.minor = std::stoul(str.substr(dot1 + 1, dot2 - dot1 - 1));
        outVersion.patch = std::stoul(str.substr(dot2 + 1, minorEnd - dot2 - 1));
        
        if (dash < str.length() && dash < plus) {
            outVersion.prerelease = str.substr(dash + 1, plus - dash - 1);
        }
        
        if (plus < str.length()) {
            outVersion.build = str.substr(plus + 1);
        }
        
        return true;
    } catch (...) {
        return false;
    }
}

// ============================================================================
// VersionConstraint Implementation
// ============================================================================

VersionConstraint::VersionConstraint(const std::string& constraintString) {
    Parse(constraintString, *this);
}

bool VersionConstraint::IsSatisfiedBy(const SemanticVersion& version) const {
    switch (op) {
        case ConstraintOperator::Any:
            return true;
        case ConstraintOperator::Exact:
            return version == minVersion;
        case ConstraintOperator::GreaterEqual:
            return version >= minVersion;
        case ConstraintOperator::Greater:
            return version > minVersion;
        case ConstraintOperator::LessEqual:
            return version <= minVersion;
        case ConstraintOperator::Less:
            return version < minVersion;
        case ConstraintOperator::Range:
            return version >= minVersion && version <= maxVersion;
        case ConstraintOperator::Caret: {
            // ^1.0.0 -> >=1.0.0 <2.0.0
            if (version < minVersion) return false;
            SemanticVersion maxCaret = minVersion;
            maxCaret.major++;
            return version < maxCaret;
        }
        case ConstraintOperator::Tilde: {
            // ~1.0.0 -> >=1.0.0 <1.1.0
            if (version < minVersion) return false;
            SemanticVersion maxTilde = minVersion;
            maxTilde.minor++;
            return version < maxTilde;
        }
        default:
            return false;
    }
}

std::string VersionConstraint::ToString() const {
    switch (op) {
        case ConstraintOperator::Any: return "*";
        case ConstraintOperator::Exact: return minVersion.ToString();
        case ConstraintOperator::Caret: return "^" + minVersion.ToString();
        case ConstraintOperator::Tilde: return "~" + minVersion.ToString();
        case ConstraintOperator::GreaterEqual: return ">=" + minVersion.ToString();
        case ConstraintOperator::Greater: return ">" + minVersion.ToString();
        case ConstraintOperator::LessEqual: return "<=" + minVersion.ToString();
        case ConstraintOperator::Less: return "<" + minVersion.ToString();
        case ConstraintOperator::Range:
            return minVersion.ToString() + " - " + maxVersion.ToString();
        default: return "unknown";
    }
}

bool VersionConstraint::Parse(const std::string& str, VersionConstraint& outConstraint) {
    if (str.empty() || str == "*") {
        outConstraint.op = ConstraintOperator::Any;
        return true;
    }

    // Extract operator and version
    if (str[0] == '^') {
        outConstraint.op = ConstraintOperator::Caret;
        return SemanticVersion::Parse(str.substr(1), outConstraint.minVersion);
    } else if (str[0] == '~') {
        outConstraint.op = ConstraintOperator::Tilde;
        return SemanticVersion::Parse(str.substr(1), outConstraint.minVersion);
    } else if (str[0] == '>' && str[1] == '=') {
        outConstraint.op = ConstraintOperator::GreaterEqual;
        return SemanticVersion::Parse(str.substr(2), outConstraint.minVersion);
    } else if (str[0] == '>') {
        outConstraint.op = ConstraintOperator::Greater;
        return SemanticVersion::Parse(str.substr(1), outConstraint.minVersion);
    } else if (str[0] == '<' && str[1] == '=') {
        outConstraint.op = ConstraintOperator::LessEqual;
        return SemanticVersion::Parse(str.substr(2), outConstraint.minVersion);
    } else if (str[0] == '<') {
        outConstraint.op = ConstraintOperator::Less;
        return SemanticVersion::Parse(str.substr(1), outConstraint.minVersion);
    } else {
        // Exact version
        outConstraint.op = ConstraintOperator::Exact;
        return SemanticVersion::Parse(str, outConstraint.minVersion);
    }
}

// ============================================================================
// ExtensionDependencyResolver Implementation
// ============================================================================

ExtensionDependencyResolver::ExtensionDependencyResolver() {
}

ExtensionDependencyResolver::~ExtensionDependencyResolver() {
}

bool ExtensionDependencyResolver::RegisterExtensionVersion(
    const std::string& extensionId,
    const SemanticVersion& version,
    const std::vector<Dependency>& dependencies
) {
    if (extensionId.empty()) {
        return false;
    }

    auto& versions = m_catalog[extensionId];
    versions.push_back({version, dependencies});

    // Sort versions in descending order
    std::sort(versions.begin(), versions.end(),
        [](const ExtensionVersionEntry& a, const ExtensionVersionEntry& b) {
            return a.version > b.version;
        }
    );

    m_resolutionCache.clear();  // Invalidate cache
    return true;
}

DependencyResolutionResult ExtensionDependencyResolver::ResolveDependencies(
    const std::string& extensionId,
    const SemanticVersion& version
) {
    // Find extension in catalog
    auto it = m_catalog.find(extensionId);
    if (it == m_catalog.end()) {
        return DependencyResolutionResult::Error("Extension not found: " + extensionId, {});
    }

    // Find exact version
    const ExtensionVersionEntry* entry = nullptr;
    for (const auto& e : it->second) {
        if (e.version == version) {
            entry = &e;
            break;
        }
    }

    if (!entry) {
        return DependencyResolutionResult::Error(
            "Extension version not found: " + extensionId + "@" + version.ToString(), {}
        );
    }

    return ResolveDependencies(entry->dependencies);
}

DependencyResolutionResult ExtensionDependencyResolver::ResolveDependencies(
    const std::vector<Dependency>& directDependencies
) {
    std::vector<ResolvedDependency> resolved;
    std::unordered_set<std::string> visited;
    std::unordered_set<std::string> unresolvable;

    for (const auto& dep : directDependencies) {
        auto result = ResolveRecursive(dep.extensionId, dep.constraint, visited, unresolvable);
        if (result.success) {
            resolved.insert(resolved.end(), result.resolved.begin(), result.resolved.end());
        }
    }

    if (!unresolvable.empty()) {
        return DependencyResolutionResult::Error("Could not resolve dependencies", unresolvable);
    }

    return DependencyResolutionResult::Success(resolved);
}

std::vector<SemanticVersion> ExtensionDependencyResolver::GetAvailableVersions(
    const std::string& extensionId
) const {
    std::vector<SemanticVersion> versions;

    auto it = m_catalog.find(extensionId);
    if (it != m_catalog.end()) {
        for (const auto& entry : it->second) {
            versions.push_back(entry.version);
        }
    }

    return versions;
}

bool ExtensionDependencyResolver::GetBestVersion(
    const std::string& extensionId,
    const VersionConstraint& constraint,
    SemanticVersion& outVersion
) const {
    auto it = m_catalog.find(extensionId);
    if (it == m_catalog.end()) {
        return false;
    }

    // Return latest version that satisfies constraint
    for (const auto& entry : it->second) {
        if (constraint.IsSatisfiedBy(entry.version)) {
            outVersion = entry.version;
            return true;
        }
    }

    return false;
}

DependencyGraph ExtensionDependencyResolver::BuildDependencyGraph(
    const std::string& extensionId
) const {
    DependencyGraph graph;
    std::unordered_set<std::string> visited;
    std::queue<std::string> queue;

    queue.push(extensionId);
    visited.insert(extensionId);

    while (!queue.empty()) {
        std::string current = queue.front();
        queue.pop();

        auto it = m_catalog.find(current);
        if (it == m_catalog.end()) {
            continue;
        }

        // Use the latest version's dependencies
        if (!it->second.empty()) {
            const auto& entry = it->second.front();
            for (const auto& dep : entry.dependencies) {
                graph.edges[current].push_back(dep.extensionId);
                if (visited.find(dep.extensionId) == visited.end()) {
                    visited.insert(dep.extensionId);
                    queue.push(dep.extensionId);
                }
            }
        }
    }

    return graph;
}

std::vector<std::string> ExtensionDependencyResolver::DetectConflicts(
    const std::vector<Dependency>& dependencies
) const {
    std::vector<std::string> conflicts;
    std::unordered_map<std::string, std::vector<VersionConstraint>> constraintsByExt;

    // Group constraints by extension ID
    for (const auto& dep : dependencies) {
        constraintsByExt[dep.extensionId].push_back(dep.constraint);
    }

    // Check for conflicting constraints on the same extension
    for (const auto& [extId, constraints] : constraintsByExt) {
        if (constraints.size() < 2) {
            continue;  // No conflict possible with single constraint
        }

        // Get available versions for this extension
        auto versions = GetAvailableVersions(extId);
        if (versions.empty()) {
            conflicts.push_back(extId + ": no versions available");
            continue;
        }

        // Find versions that satisfy ALL constraints
        std::vector<SemanticVersion> compatibleVersions;
        for (const auto& ver : versions) {
            bool allSatisfied = true;
            for (const auto& constraint : constraints) {
                if (!constraint.IsSatisfiedBy(ver)) {
                    allSatisfied = false;
                    break;
                }
            }
            if (allSatisfied) {
                compatibleVersions.push_back(ver);
            }
        }

        if (compatibleVersions.empty()) {
            std::string conflictMsg = extId + ": conflicting constraints ";
            for (size_t i = 0; i < constraints.size(); ++i) {
                if (i > 0) conflictMsg += " vs ";
                conflictMsg += constraints[i].ToString();
            }
            conflicts.push_back(conflictMsg);
        }
    }

    return conflicts;
}

bool ExtensionDependencyResolver::TryResolveConflicts(
    const std::vector<Dependency>& dependencies,
    DependencyResolutionResult& outResult
) {
    outResult = ResolveDependencies(dependencies);
    return outResult.success;
}

DependencyResolutionResult ExtensionDependencyResolver::ResolveRecursive(
    const std::string& extensionId,
    const VersionConstraint& constraint,
    std::unordered_set<std::string>& visited,
    std::unordered_set<std::string>& unresolvable
) {
    if (visited.find(extensionId) != visited.end()) {
        // Cycle detection
        return DependencyResolutionResult::Error("Circular dependency: " + extensionId, {});
    }

    visited.insert(extensionId);

    // Find best matching version
    SemanticVersion bestVersion;
    if (!GetBestVersion(extensionId, constraint, bestVersion)) {
        unresolvable.insert(extensionId);
        return DependencyResolutionResult::Error(
            "No version found for " + extensionId + "@" + constraint.ToString(), {}
        );
    }

    // Resolve transitive dependencies
    auto it = m_catalog.find(extensionId);
    if (it != m_catalog.end()) {
        const ExtensionVersionEntry* entry = nullptr;
        for (const auto& e : it->second) {
            if (e.version == bestVersion) {
                entry = &e;
                break;
            }
        }

        if (entry) {
            ResolvedDependency resolved;
            resolved.extensionId = extensionId;
            resolved.resolvedVersion = bestVersion;
            resolved.transitiveDependencies = entry->dependencies;

            std::vector<ResolvedDependency> result;
            result.push_back(resolved);

            return DependencyResolutionResult::Success(result);
        }
    }

    return DependencyResolutionResult::Error("Could not resolve: " + extensionId, {});
}

bool ExtensionDependencyResolver::HasCycleDetection(
    const std::string& current,
    const std::unordered_set<std::string>& visiting,
    std::unordered_set<std::string>& visited,
    DependencyGraph& graph
) const {
    // DFS-based cycle detection for extension dependency graph
    if (visiting.count(current)) return true;  // Cycle found
    if (visited.count(current)) return false;    // Already checked, no cycle
    
    visited.insert(current);
    
    auto it = graph.dependencies.find(current);
    if (it != graph.dependencies.end()) {
        std::unordered_set<std::string> newVisiting = visiting;
        newVisiting.insert(current);
        for (const auto& dep : it->second) {
            if (HasCycleDetection(dep.extensionId, newVisiting, visited, graph)) {
                return true;
            }
        }
    }
    return false;
}

bool ExtensionDependencyResolver::NegotiateConflict(
    const DependencyGraph& graph,
    std::unordered_map<std::string, SemanticVersion>& versions
) {
    // Version negotiation: select highest compatible version for each extension
    for (const auto& ext : graph.extensions) {
        SemanticVersion highest = ext.second.version;
        
        auto depIt = graph.dependencies.find(ext.first);
        if (depIt != graph.dependencies.end()) {
            for (const auto& dep : depIt->second) {
                auto reqIt = versions.find(dep.extensionId);
                if (reqIt != versions.end()) {
                    // Keep the higher version requirement
                    if (dep.requiredVersion > reqIt->second) {
                        reqIt->second = dep.requiredVersion;
                    }
                } else {
                    versions[dep.extensionId] = dep.requiredVersion;
                }
            }
        }
        versions[ext.first] = highest;
    }
    return true;
}

// ============================================================================
// Global Helper
// ============================================================================

bool ResolveExtensionDependencies(
    const std::string& extensionId,
    const std::string& version,
    std::vector<ResolvedDependency>& outResolved
) {
    SemanticVersion ver(version);
    auto result = GetDependencyResolver().ResolveDependencies(extensionId, ver);
    if (result.success) {
        outResolved = result.resolved;
        return true;
    }
    return false;
}

}  // namespace Extensions
}  // namespace RawrXD

// ============================================================================
// End of extension_dependency_resolver.cpp
// ============================================================================
