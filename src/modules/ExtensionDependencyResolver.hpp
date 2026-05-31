// ============================================================================
// ExtensionDependencyResolver.hpp — Extension Dependency Resolution System
// ============================================================================
//
// Phase 29C: Extension Dependency Management
//
// Purpose:
//   Provides complete dependency resolution for extension installation.
//   Handles extension dependencies, conflicts, and version compatibility.
//   Ensures proper installation order and conflict resolution.
//
// Features:
//   - Dependency Graph Construction & Analysis
//   - Topological Sorting for Installation Order
//   - Version Constraint Resolution (semver)
//   - Circular Dependency Detection
//   - Conflict Resolution Strategies
//   - Optional vs Required Dependencies
//   - Extension Pack Support
//   - Graceful Degradation for Missing Dependencies
//
// Design:
//   - Directed Acyclic Graph (DAG) representation
//   - Constraint satisfaction solver
//   - Backtracking for conflict resolution
//   - Integration with MarketplaceBackend
//   - Compatible with VS Code extension ecosystem
//
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <functional>
#include <algorithm>
#include <regex>
#include <iostream>

// ============================================================================
// Version & Constraint Structures
// ============================================================================

struct SemanticVersion {
    uint32_t major;
    uint32_t minor;
    uint32_t patch;
    std::string preRelease;
    std::string build;
    
    SemanticVersion() : major(0), minor(0), patch(0) {}
    SemanticVersion(uint32_t maj, uint32_t min, uint32_t pat) : major(maj), minor(min), patch(pat) {}
    
    static SemanticVersion parse(const std::string& version);
    std::string toString() const;
    
    bool operator==(const SemanticVersion& other) const;
    bool operator!=(const SemanticVersion& other) const;
    bool operator<(const SemanticVersion& other) const;
    bool operator<=(const SemanticVersion& other) const;
    bool operator>(const SemanticVersion& other) const;
    bool operator>=(const SemanticVersion& other) const;
    
    bool satisfies(const std::string& constraint) const;
};

enum class VersionConstraintType {
    Exact,          // =1.0.0
    GreaterThan,    // >1.0.0
    GreaterEqual,   // >=1.0.0
    LessThan,       // <1.0.0
    LessEqual,      // <=1.0.0
    Compatible,     // ^1.0.0 (compatible release)
    Approximate,    // ~1.0.0 (approximately equivalent)
    Range,          // 1.0.0 - 2.0.0
    Wildcard,       // 1.*, 1.0.*
    Any             // *
};

struct VersionConstraint {
    VersionConstraintType type;
    SemanticVersion version;
    SemanticVersion rangeEnd;  // For range constraints
    
    static VersionConstraint parse(const std::string& constraint);
    bool isSatisfiedBy(const SemanticVersion& version) const;
    std::string toString() const;
};

// ============================================================================
// Dependency Structures
// ============================================================================

enum class DependencyType {
    Required,           // Must be installed
    Optional,           // Nice to have, but not required
    Development,        // Only needed for development
    Peer,              // Must be provided by parent context
    Bundled,           // Included in extension package
    Extension          // Extension pack dependency
};

struct ExtensionDependency {
    std::string extensionId;        // publisher.name
    VersionConstraint constraint;   // Version requirement
    DependencyType type;           // Dependency type
    std::string reason;            // Human-readable reason
    bool isOptional;               // Can be skipped if not available
    
    ExtensionDependency() : type(DependencyType::Required), isOptional(false) {}
    
    static ExtensionDependency parse(const std::string& depString, DependencyType defaultType = DependencyType::Required);
};

struct ExtensionNode {
    std::string extensionId;
    SemanticVersion version;
    std::vector<ExtensionDependency> dependencies;
    std::unordered_set<std::string> conflicts;     // Extensions that conflict with this
    std::vector<std::string> provides;             // Virtual packages provided
    bool isInstalled;
    bool isRequired;                               // Required by user or other dependencies
    int installOrder;                              // Topological sort order (-1 = unassigned)
    
    ExtensionNode() : isInstalled(false), isRequired(false), installOrder(-1) {}
    ExtensionNode(const std::string& id, const SemanticVersion& ver) 
        : extensionId(id), version(ver), isInstalled(false), isRequired(false), installOrder(-1) {}
};

// ============================================================================
// Dependency Resolution Results
// ============================================================================

enum class ResolutionStatus {
    Success,                    // Resolution successful
    CircularDependency,         // Circular dependency detected
    ConflictingVersions,        // Version conflicts cannot be resolved
    MissingDependency,          // Required dependency not found
    IncompatibleExtensions,     // Extensions declare conflicts
    InsufficientPermissions,    // Permission requirements not met
    NetworkError,               // Unable to fetch dependency information
    UserCancellation           // User cancelled resolution
};

struct DependencyConflict {
    std::string extensionId;
    std::string conflictingExtension;
    std::string conflictType;       // "version", "incompatible", "circular"
    std::string description;
    std::vector<std::string> resolutionOptions;
};

struct ResolutionPlan {
    ResolutionStatus status;
    std::vector<ExtensionNode> installOrder;       // Dependencies in install order
    std::vector<std::string> toUpdate;             // Extensions to update
    std::vector<std::string> toRemove;             // Extensions to remove
    std::vector<DependencyConflict> conflicts;     // Unresolved conflicts
    std::vector<std::string> warnings;             // Non-critical warnings
    std::string errorMessage;                      // Error description if failed
    
    ResolutionPlan() : status(ResolutionStatus::Success) {}
};

// ============================================================================
// Dependency Resolver Configuration
// ============================================================================

struct ResolverConfig {
    bool allowPreRelease;           // Include pre-release versions
    bool preferLatest;              // Use latest compatible version
    bool skipOptionalDeps;          // Skip optional dependencies
    bool autoResolveConflicts;      // Automatically resolve conflicts when possible
    bool allowDowngrades;           // Allow downgrading extensions to resolve conflicts
    bool requireExactVersions;      // Require exact version matches (no range resolution)
    uint32_t maxResolutionDepth;    // Maximum dependency depth
    uint32_t maxResolutionTime;     // Maximum resolution time (seconds)
    
    static ResolverConfig getDefault() {
        ResolverConfig config;
        config.allowPreRelease = false;
        config.preferLatest = true;
        config.skipOptionalDeps = false;
        config.autoResolveConflicts = true;
        config.allowDowngrades = false;
        config.requireExactVersions = false;
        config.maxResolutionDepth = 50;
        config.maxResolutionTime = 60;
        return config;
    }
};

// ============================================================================
// Main Dependency Resolver
// ============================================================================

class ExtensionDependencyResolver {
public:
    ExtensionDependencyResolver(const ResolverConfig& config = ResolverConfig::getDefault());
    ~ExtensionDependencyResolver();
    
    // Core resolution methods
    ResolutionPlan resolveInstallation(const std::vector<std::string>& extensionIds);
    ResolutionPlan resolveUpdate(const std::vector<std::string>& extensionIds);
    ResolutionPlan resolveRemoval(const std::vector<std::string>& extensionIds);
    ResolutionPlan resolveAll();  // Resolve all installed extensions
    
    // Dependency analysis
    std::vector<std::string> getDependents(const std::string& extensionId);
    std::vector<std::string> getDependencies(const std::string& extensionId, bool transitively = false);
    std::vector<DependencyConflict> detectConflicts();
    bool hasCircularDependencies(std::vector<std::string>& cycle);
    
    // Extension information
    void addInstalledExtension(const std::string& extensionId, const SemanticVersion& version, const std::vector<ExtensionDependency>& dependencies);
    void removeInstalledExtension(const std::string& extensionId);
    void updateInstalledExtension(const std::string& extensionId, const SemanticVersion& newVersion);
    bool isInstalled(const std::string& extensionId) const;
    SemanticVersion getInstalledVersion(const std::string& extensionId) const;
    
    // Resolution strategy configuration
    void setConfig(const ResolverConfig& config);
    ResolverConfig getConfig() const;
    
    // External data source integration
    void setExtensionInfoProvider(std::function<ExtensionNode(const std::string&, const std::string&)> provider);
    void setVersionListProvider(std::function<std::vector<SemanticVersion>(const std::string&)> provider);
    void setConflictResolver(std::function<bool(const DependencyConflict&, std::string&)> resolver);
    
    // Progress callbacks
    void setProgressCallback(std::function<void(const std::string&, int, int)> callback);
    void setConflictCallback(std::function<bool(const DependencyConflict&, std::string&)> callback);
    
    // Visualization & debugging
    std::string generateDependencyGraphViz() const;
    std::vector<std::string> getTopologicalOrder() const;
    void printResolutionPlan(const ResolutionPlan& plan) const;
    
private:
    ResolverConfig m_config;
    std::unordered_map<std::string, ExtensionNode> m_installedExtensions;
    std::unordered_map<std::string, ExtensionNode> m_candidateExtensions;
    
    // External providers
    std::function<ExtensionNode(const std::string&, const std::string&)> m_extensionInfoProvider;
    std::function<std::vector<SemanticVersion>(const std::string&)> m_versionListProvider;
    std::function<bool(const DependencyConflict&, std::string&)> m_conflictResolver;
    std::function<void(const std::string&, int, int)> m_progressCallback;
    std::function<bool(const DependencyConflict&, std::string&)> m_conflictCallback;
    
    // Resolution state
    mutable std::unordered_map<std::string, int> m_visitState;  // For cycle detection: 0=unvisited, 1=visiting, 2=visited
    mutable std::vector<std::string> m_topologicalOrder;
    mutable bool m_topologicalOrderValid;
    
    // Core resolution algorithms
    ResolutionPlan buildResolutionPlan(const std::vector<std::string>& targetExtensions, const std::string& operation);
    bool collectDependencies(const std::string& extensionId, const std::string& requestedVersion, ResolutionPlan& plan);
    bool resolveVersionConstraints(ResolutionPlan& plan);
    bool detectCircularDependenciesInternal(const std::string& extensionId, std::vector<std::string>& path, std::vector<std::string>& cycle) const;
    bool topologicalSort(std::vector<ExtensionNode>& sortedNodes) const;
    
    // Version resolution
    SemanticVersion findBestVersion(const std::string& extensionId, const std::vector<VersionConstraint>& constraints);
    std::vector<SemanticVersion> getAvailableVersions(const std::string& extensionId);
    bool areVersionsCompatible(const SemanticVersion& v1, const SemanticVersion& v2, const VersionConstraint& constraint);
    
    // Conflict resolution
    bool attemptConflictResolution(DependencyConflict& conflict, ResolutionPlan& plan);
    bool canResolveByDowngrade(const DependencyConflict& conflict);
    bool canResolveByAlternative(const DependencyConflict& conflict);
    std::vector<std::string> findAlternativeExtensions(const std::string& extensionId);
    
    // Graph utilities
    void invalidateTopologicalOrder();
    ExtensionNode getExtensionInfo(const std::string& extensionId, const std::string& version = "latest");
    bool addToResolutionPlan(const ExtensionNode& node, ResolutionPlan& plan);
    
    // Helper methods
    std::string formatConflictMessage(const DependencyConflict& conflict) const;
    bool isOptionalDependencySatisfied(const ExtensionDependency& dep) const;
    void reportProgress(const std::string& operation, int current, int total);
};

// ============================================================================
// Utility Functions
// ============================================================================

// Predefined version constraint patterns
class VersionPatterns {
public:
    static const std::regex SEMVER_REGEX;
    static const std::regex CONSTRAINT_REGEX;
    static const std::regex RANGE_REGEX;
    
    static bool isValidVersion(const std::string& version);
    static bool isValidConstraint(const std::string& constraint);
    static std::vector<std::string> parseVersionRange(const std::string& range);
};

// Extension manifest parsing helpers
namespace ExtensionManifestParser {
    std::vector<ExtensionDependency> parseDependencies(const std::string& manifestJson);
    std::vector<std::string> parseConflicts(const std::string& manifestJson);
    std::vector<std::string> parseProvides(const std::string& manifestJson);
    std::string parseEngineRequirement(const std::string& manifestJson);
}

// Integration with marketplace backend
namespace MarketplaceIntegration {
    void setupMarketplaceProviders(ExtensionDependencyResolver& resolver);
    ExtensionNode fetchExtensionInfo(const std::string& extensionId, const std::string& version);
    std::vector<SemanticVersion> fetchAvailableVersions(const std::string& extensionId);
    bool isExtensionAvailable(const std::string& extensionId);
}

// Global resolver instance
ExtensionDependencyResolver& GetDependencyResolver();

// Initialize dependency resolver with marketplace integration
bool InitializeDependencyResolver(const ResolverConfig& config = ResolverConfig::getDefault());

// Shutdown dependency resolver
void ShutdownDependencyResolver();
