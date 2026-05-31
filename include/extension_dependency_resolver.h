// ============================================================================
// extension_dependency_resolver.h — Extension Dependency Resolution (npm-style)
// ============================================================================
// PURPOSE:
//   Resolves extension dependencies using npm-style version constraints and
//   semantic versioning. Handles:
//   - Version range parsing (^1.0.0, ~1.0.0, >=1.0.0, etc.)
//   - Transitive dependencies
//   - Conflict resolution and version negotiation
//   - Dependency graph analysis
//   - Circular dependency detection
//
// Architecture: C++20 | Win32 | No exceptions | Qt-free
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <cstdint>

namespace RawrXD {
namespace Extensions {

// ============================================================================
// Version Types
// ============================================================================

struct SemanticVersion {
    uint32_t major = 0;
    uint32_t minor = 0;
    uint32_t patch = 0;
    std::string prerelease;              // Alpha, beta, rc versions
    std::string build;                   // Build metadata
    
    explicit SemanticVersion(const std::string& versionString = "0.0.0");
    
    bool operator<(const SemanticVersion& other) const;
    bool operator<=(const SemanticVersion& other) const;
    bool operator>(const SemanticVersion& other) const;
    bool operator>=(const SemanticVersion& other) const;
    bool operator==(const SemanticVersion& other) const;
    
    std::string ToString() const;
    static bool Parse(const std::string& str, SemanticVersion& outVersion);
};

// ============================================================================
// Version Constraint Types
// ============================================================================

enum class ConstraintOperator {
    Exact,                              // 1.0.0
    Caret,                              // ^1.0.0 (allow compatible)
    Tilde,                              // ~1.0.0 (allow patch)
    GreaterEqual,                       // >=1.0.0
    Greater,                            // >1.0.0
    LessEqual,                          // <=1.0.0
    Less,                               // <1.0.0
    Range,                              // 1.0.0 - 2.0.0
    Any,                                // *
};

struct VersionConstraint {
    ConstraintOperator op = ConstraintOperator::Any;
    SemanticVersion minVersion;
    SemanticVersion maxVersion;         // For range constraints
    
    explicit VersionConstraint(const std::string& constraintString = "*");
    
    bool IsSatisfiedBy(const SemanticVersion& version) const;
    std::string ToString() const;
    
    static bool Parse(const std::string& str, VersionConstraint& outConstraint);
};

// ============================================================================
// Dependency Types
// ============================================================================

struct Dependency {
    std::string extensionId;            // "publisher.name"
    VersionConstraint constraint;
    bool optional = false;
    
    explicit Dependency(const std::string& id = "", const std::string& constraint = "*")
        : extensionId(id), constraint(constraint) {}
};

struct ResolvedDependency {
    std::string extensionId;
    SemanticVersion resolvedVersion;
    std::vector<Dependency> transitiveDependencies;
};

// ============================================================================
// Resolution Result
// ============================================================================

struct DependencyResolutionResult {
    bool success = false;
    std::string errorMessage;
    std::vector<ResolvedDependency> resolved;
    std::unordered_set<std::string> unresolvable;
    
    static DependencyResolutionResult Success(
        const std::vector<ResolvedDependency>& deps) {
        DependencyResolutionResult r;
        r.success = true;
        r.resolved = deps;
        return r;
    }
    
    static DependencyResolutionResult Error(
        const std::string& msg,
        const std::unordered_set<std::string>& unresolvableIds) {
        DependencyResolutionResult r;
        r.success = false;
        r.errorMessage = msg;
        r.unresolvable = unresolvableIds;
        return r;
    }
};

// ============================================================================
// Dependency Graph Analysis
// ============================================================================

struct DependencyGraph {
    std::unordered_map<std::string, std::vector<std::string>> edges;  // extId -> depends on
    
    bool HasCycle() const;
    std::vector<std::string> GetTopologicalOrder() const;
    std::vector<std::string> GetTransitiveDependencies(const std::string& extId) const;
};

// ============================================================================
// Extension Dependency Resolver
// ============================================================================

class ExtensionDependencyResolver {
public:
    explicit ExtensionDependencyResolver();
    ~ExtensionDependencyResolver();

    // ── Registration ───────────────────────────────────────────────

    // Register available extension versions
    bool RegisterExtensionVersion(const std::string& extensionId,
                                  const SemanticVersion& version,
                                  const std::vector<Dependency>& dependencies);

    // ── Resolution ─────────────────────────────────────────────────

    // Resolve dependencies for extension at specified version
    DependencyResolutionResult ResolveDependencies(
        const std::string& extensionId,
        const SemanticVersion& version
    );

    // Resolve dependencies from a list of specifications
    DependencyResolutionResult ResolveDependencies(
        const std::vector<Dependency>& directDependencies
    );

    // ── Queries ────────────────────────────────────────────────────

    // Get available versions for extension
    std::vector<SemanticVersion> GetAvailableVersions(
        const std::string& extensionId
    ) const;

    // Get best matching version for constraint
    bool GetBestVersion(const std::string& extensionId,
                        const VersionConstraint& constraint,
                        SemanticVersion& outVersion) const;

    // Get dependency graph for extension
    DependencyGraph BuildDependencyGraph(const std::string& extensionId) const;

    // ── Conflict Analysis ──────────────────────────────────────────

    // Detect conflicting version constraints
    std::vector<std::string> DetectConflicts(
        const std::vector<Dependency>& dependencies
    ) const;

    // Try to find compatible version set
    bool TryResolveConflicts(
        const std::vector<Dependency>& dependencies,
        DependencyResolutionResult& outResult
    );

private:
    struct ExtensionVersionEntry {
        SemanticVersion version;
        std::vector<Dependency> dependencies;
    };

    // Catalog of available extensions and their versions
    std::unordered_map<std::string, std::vector<ExtensionVersionEntry>> m_catalog;

    // Resolution cache
    std::unordered_map<std::string, DependencyResolutionResult> m_resolutionCache;

    // Internal resolution algorithm
    DependencyResolutionResult ResolveRecursive(
        const std::string& extensionId,
        const VersionConstraint& constraint,
        std::unordered_set<std::string>& visited,
        std::unordered_set<std::string>& unresolvable
    );

    // Detect cycles in dependency graph
    bool HasCycleDetection(const std::string& current,
                           const std::unordered_set<std::string>& visiting,
                           std::unordered_set<std::string>& visited,
                           DependencyGraph& graph) const;

    // Version negotiation for conflicts
    bool NegotiateConflict(const DependencyGraph& graph,
                           std::unordered_map<std::string, SemanticVersion>& versions);
};

// ============================================================================
// Global Helper
// ============================================================================

// Get singleton resolver instance
ExtensionDependencyResolver& GetDependencyResolver();

// Convenience functions
bool ResolveExtensionDependencies(
    const std::string& extensionId,
    const std::string& version,
    std::vector<ResolvedDependency>& outResolved
);

}  // namespace Extensions
}  // namespace RawrXD

#endif  // EXTENSION_DEPENDENCY_RESOLVER_H
