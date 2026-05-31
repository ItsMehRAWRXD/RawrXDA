// ============================================================================
// ExtensionDependencyResolver.cpp — Implementation
// ============================================================================

#include "ExtensionDependencyResolver.hpp"
#include <sstream>
#include <algorithm>
#include <queue>
#include <chrono>

// ============================================================================
// Semantic Version Implementation
// ============================================================================

SemanticVersion SemanticVersion::parse(const std::string& version) {
    SemanticVersion v;
    std::regex versionRegex(R"(^(\d+)\.(\d+)\.(\d+)(?:-([a-zA-Z0-9\-\.]+))?(?:\+([a-zA-Z0-9\-\.]+))?$)");
    std::smatch matches;
    
    if (std::regex_match(version, matches, versionRegex)) {
        v.major = static_cast<uint32_t>(std::stoul(matches[1]));
        v.minor = static_cast<uint32_t>(std::stoul(matches[2]));
        v.patch = static_cast<uint32_t>(std::stoul(matches[3]));
        v.preRelease = matches[4].matched ? matches[4].str() : "";
        v.build = matches[5].matched ? matches[5].str() : "";
    }
    
    return v;
}

std::string SemanticVersion::toString() const {
    std::ostringstream oss;
    oss << major << "." << minor << "." << patch;
    
    if (!preRelease.empty()) {
        oss << "-" << preRelease;
    }
    
    if (!build.empty()) {
        oss << "+" << build;
    }
    
    return oss.str();
}

bool SemanticVersion::operator==(const SemanticVersion& other) const {
    return major == other.major && minor == other.minor && patch == other.patch && preRelease == other.preRelease;
}

bool SemanticVersion::operator<(const SemanticVersion& other) const {
    if (major != other.major) return major < other.major;
    if (minor != other.minor) return minor < other.minor;
    if (patch != other.patch) return patch < other.patch;
    
    // Pre-release versions have lower precedence
    if (preRelease.empty() && !other.preRelease.empty()) return false;
    if (!preRelease.empty() && other.preRelease.empty()) return true;
    
    return preRelease < other.preRelease;
}

bool SemanticVersion::operator>(const SemanticVersion& other) const {
    return other < *this;
}

bool SemanticVersion::operator<=(const SemanticVersion& other) const {
    return *this < other || *this == other;
}

bool SemanticVersion::operator>=(const SemanticVersion& other) const {
    return *this > other || *this == other;
}

bool SemanticVersion::satisfies(const std::string& constraint) const {
    VersionConstraint vc = VersionConstraint::parse(constraint);
    return vc.isSatisfiedBy(*this);
}

// ============================================================================
// Version Constraint Implementation
// ============================================================================

VersionConstraint VersionConstraint::parse(const std::string& constraint) {
    VersionConstraint vc;
    
    if (constraint == "*") {
        vc.type = VersionConstraintType::Any;
        return vc;
    }
    
    std::regex patterns[] = {
        std::regex(R"(^=(.+)$)"),                    // Exact: =1.0.0
        std::regex(R"(^>=(.+)$)"),                   // GreaterEqual: >=1.0.0  
        std::regex(R"(^>(.+)$)"),                    // GreaterThan: >1.0.0
        std::regex(R"(^<=(.+)$)"),                   // LessEqual: <=1.0.0
        std::regex(R"(^<(.+)$)"),                    // LessThan: <1.0.0
        std::regex(R"(^\^(.+)$)"),                   // Compatible: ^1.0.0
        std::regex(R"(^~(.+)$)"),                    // Approximate: ~1.0.0
        std::regex(R"(^(.+)\s*-\s*(.+)$)"),         // Range: 1.0.0 - 2.0.0
        std::regex(R"(^(.+\*.*|\*.*\..*|\*.*\*.*))$)") // Wildcard patterns
    };
    
    VersionConstraintType types[] = {
        VersionConstraintType::Exact,
        VersionConstraintType::GreaterEqual,
        VersionConstraintType::GreaterThan,
        VersionConstraintType::LessEqual,
        VersionConstraintType::LessThan,
        VersionConstraintType::Compatible,
        VersionConstraintType::Approximate,
        VersionConstraintType::Range,
        VersionConstraintType::Wildcard
    };
    
    for (size_t i = 0; i < sizeof(patterns)/sizeof(patterns[0]); ++i) {
        std::smatch matches;
        if (std::regex_match(constraint, matches, patterns[i])) {
            vc.type = types[i];
            
            if (vc.type == VersionConstraintType::Range) {
                vc.version = SemanticVersion::parse(matches[1]);
                vc.rangeEnd = SemanticVersion::parse(matches[2]);
            } else if (vc.type == VersionConstraintType::Wildcard) {
                // Handle wildcard patterns
                vc.version = SemanticVersion::parse(std::regex_replace(matches[1].str(), std::regex("\\*"), "0"));
            } else {
                vc.version = SemanticVersion::parse(matches[1]);
            }
            return vc;
        }
    }
    
    // Default to exact match
    vc.type = VersionConstraintType::Exact;
    vc.version = SemanticVersion::parse(constraint);
    return vc;
}

bool VersionConstraint::isSatisfiedBy(const SemanticVersion& version) const {
    switch (type) {
        case VersionConstraintType::Any:
            return true;
            
        case VersionConstraintType::Exact:
            return version == this->version;
            
        case VersionConstraintType::GreaterThan:
            return version > this->version;
            
        case VersionConstraintType::GreaterEqual:
            return version >= this->version;
            
        case VersionConstraintType::LessThan:
            return version < this->version;
            
        case VersionConstraintType::LessEqual:
            return version <= this->version;
            
        case VersionConstraintType::Compatible:
            // ^1.2.3 allows >=1.2.3 and <2.0.0
            return version >= this->version && 
                   (version.major == this->version.major);
                   
        case VersionConstraintType::Approximate:
            // ~1.2.3 allows >=1.2.3 and <1.3.0
            return version >= this->version && 
                   (version.major == this->version.major && version.minor == this->version.minor);
                   
        case VersionConstraintType::Range:
            return version >= this->version && version <= rangeEnd;
            
        case VersionConstraintType::Wildcard:
            // Simple wildcard matching  
            return version.major == this->version.major;
            
        default:
            return false;
    }
}

std::string VersionConstraint::toString() const {
    switch (type) {
        case VersionConstraintType::Any: return "*";
        case VersionConstraintType::Exact: return "=" + version.toString();
        case VersionConstraintType::GreaterThan: return ">" + version.toString();
        case VersionConstraintType::GreaterEqual: return ">=" + version.toString();
        case VersionConstraintType::LessThan: return "<" + version.toString();
        case VersionConstraintType::LessEqual: return "<=" + version.toString();
        case VersionConstraintType::Compatible: return "^" + version.toString();
        case VersionConstraintType::Approximate: return "~" + version.toString();
        case VersionConstraintType::Range: return version.toString() + " - " + rangeEnd.toString();
        case VersionConstraintType::Wildcard: return version.toString() + "*";
        default: return version.toString();
    }
}

// ============================================================================
// Extension Dependency Implementation
// ============================================================================

ExtensionDependency ExtensionDependency::parse(const std::string& depString, DependencyType defaultType) {
    ExtensionDependency dep;
    dep.type = defaultType;
    
    // Parse "extensionId@versionConstraint" format
    size_t atPos = depString.find('@');
    if (atPos != std::string::npos) {
        dep.extensionId = depString.substr(0, atPos);
        std::string constraintStr = depString.substr(atPos + 1);
        dep.constraint = VersionConstraint::parse(constraintStr);
    } else {
        dep.extensionId = depString;
        dep.constraint.type = VersionConstraintType::Any;
    }
    
    return dep;
}

// ============================================================================
// Global Instance Management
// ============================================================================

static std::unique_ptr<ExtensionDependencyResolver> g_dependencyResolver;
static std::mutex g_resolverMutex;

ExtensionDependencyResolver& GetDependencyResolver() {
    std::lock_guard<std::mutex> lock(g_resolverMutex);
    if (!g_dependencyResolver) {
        g_dependencyResolver = std::make_unique<ExtensionDependencyResolver>();
    }
    return *g_dependencyResolver;
}

bool InitializeDependencyResolver(const ResolverConfig& config) {
    std::lock_guard<std::mutex> lock(g_resolverMutex);
    
    g_dependencyResolver = std::make_unique<ExtensionDependencyResolver>(config);
    
    // Setup marketplace integration
    MarketplaceIntegration::setupMarketplaceProviders(*g_dependencyResolver);
    
    std::cout << "[DependencyResolver] Initialized successfully" << std::endl;
    return true;
}

void ShutdownDependencyResolver() {
    std::lock_guard<std::mutex> lock(g_resolverMutex);
    g_dependencyResolver.reset();
    std::cout << "[DependencyResolver] Shutdown completed" << std::endl;
}

// ============================================================================
// Dependency Resolver Implementation
// ============================================================================

ExtensionDependencyResolver::ExtensionDependencyResolver(const ResolverConfig& config) 
    : m_config(config), m_topologicalOrderValid(false) {
}

ExtensionDependencyResolver::~ExtensionDependencyResolver() = default;

ResolutionPlan ExtensionDependencyResolver::resolveInstallation(const std::vector<std::string>& extensionIds) {
    return buildResolutionPlan(extensionIds, "install");
}

ResolutionPlan ExtensionDependencyResolver::resolveUpdate(const std::vector<std::string>& extensionIds) {
    return buildResolutionPlan(extensionIds, "update");
}

ResolutionPlan ExtensionDependencyResolver::resolveRemoval(const std::vector<std::string>& extensionIds) {
    ResolutionPlan plan;
    
    // For removal, find dependents and add them to removal list
    for (const std::string& extensionId : extensionIds) {
        std::vector<std::string> dependents = getDependents(extensionId);
        
        for (const std::string& dependent : dependents) {
            auto it = m_installedExtensions.find(dependent);
            if (it != m_installedExtensions.end()) {
                // Check if dependency is required
                bool isRequiredDep = false;
                for (const auto& dep : it->second.dependencies) {
                    if (dep.extensionId == extensionId && dep.type == DependencyType::Required) {
                        isRequiredDep = true;
                        break;
                    }
                }
                
                if (isRequiredDep) {
                    plan.toRemove.push_back(dependent);
                }
            }
        }
        
        plan.toRemove.push_back(extensionId);
    }
    
    // Remove duplicates while preserving order
    auto last = std::unique(plan.toRemove.begin(), plan.toRemove.end());
    plan.toRemove.erase(last, plan.toRemove.end());
    
    return plan;
}

ResolutionPlan ExtensionDependencyResolver::buildResolutionPlan(
    const std::vector<std::string>& targetExtensions, 
    const std::string& operation
) {
    ResolutionPlan plan;
    auto startTime = std::chrono::steady_clock::now();
    
    try {
        // Clear candidate extensions for fresh resolution
        m_candidateExtensions.clear();
        
        // Collect dependencies for each target extension
        for (const std::string& extensionId : targetExtensions) {
            reportProgress("Analyzing " + extensionId, 0, static_cast<int>(targetExtensions.size()));
            
            if (!collectDependencies(extensionId, "latest", plan)) {
                plan.status = ResolutionStatus::MissingDependency;
                plan.errorMessage = "Failed to resolve dependencies for " + extensionId;
                return plan;
            }
        }
        
        // Resolve version constraints
        if (!resolveVersionConstraints(plan)) {
            plan.status = ResolutionStatus::ConflictingVersions;
            return plan;
        }
        
        // Check for circular dependencies
        std::vector<std::string> cycle;
        if (hasCircularDependencies(cycle)) {
            plan.status = ResolutionStatus::CircularDependency;
            plan.errorMessage = "Circular dependency detected: " + 
                              std::accumulate(cycle.begin(), cycle.end(), std::string(),
                                  [](const std::string& a, const std::string& b) {
                                      return a.empty() ? b : a + " -> " + b;
                                  });
            return plan;
        }
        
        // Create topological ordering for installation
        std::vector<ExtensionNode> sortedNodes;
        if (!topologicalSort(sortedNodes)) {
            plan.status = ResolutionStatus::CircularDependency;
            plan.errorMessage = "Failed to determine installation order";
            return plan;
        }
        
        plan.installOrder = sortedNodes;
        plan.status = ResolutionStatus::Success;
        
    } catch (const std::exception& e) {
        plan.status = ResolutionStatus::NetworkError;
        plan.errorMessage = std::string("Resolution failed: ") + e.what();
    }
    
    auto endTime = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(endTime - startTime);
    
    if (duration.count() > m_config.maxResolutionTime) {
        plan.status = ResolutionStatus::NetworkError;
        plan.errorMessage = "Resolution timeout exceeded";
    }
    
    return plan;
}

bool ExtensionDependencyResolver::collectDependencies(
    const std::string& extensionId, 
    const std::string& requestedVersion, 
    ResolutionPlan& plan
) {
    // Check if already processed
    if (m_candidateExtensions.find(extensionId) != m_candidateExtensions.end()) {
        return true;
    }
    
    // Get extension information
    ExtensionNode node = getExtensionInfo(extensionId, requestedVersion);
    if (node.extensionId.empty()) {
        return false;
    }
    
    // Add to candidates
    m_candidateExtensions[extensionId] = node;
    
    // Recursively collect dependencies
    for (const ExtensionDependency& dep : node.dependencies) {
        if (dep.type == DependencyType::Optional && m_config.skipOptionalDeps) {
            continue;
        }
        
        if (!collectDependencies(dep.extensionId, "latest", plan)) {
            if (dep.isOptional) {
                plan.warnings.push_back("Optional dependency not available: " + dep.extensionId);
                continue;
            } else {
                return false;
            }
        }
    }
    
    return true;
}

bool ExtensionDependencyResolver::resolveVersionConstraints(ResolutionPlan& plan) {
    // Collect all version constraints for each extension
    std::unordered_map<std::string, std::vector<VersionConstraint>> constraints;
    
    for (const auto& candidate : m_candidateExtensions) {
        const ExtensionNode& node = candidate.second;
        
        for (const ExtensionDependency& dep : node.dependencies) {
            constraints[dep.extensionId].push_back(dep.constraint);
        }
    }
    
    // Find best version for each extension that satisfies all constraints
    for (auto& candidate : m_candidateExtensions) {
        const std::string& extensionId = candidate.first;
        ExtensionNode& node = candidate.second;
        
        if (constraints.find(extensionId) != constraints.end()) {
            SemanticVersion bestVersion = findBestVersion(extensionId, constraints[extensionId]);
            if (bestVersion.major == 0 && bestVersion.minor == 0 && bestVersion.patch == 0) {
                // No compatible version found
                DependencyConflict conflict;
                conflict.extensionId = extensionId;
                conflict.conflictType = "version";
                conflict.description = "No version satisfies all constraints";
                plan.conflicts.push_back(conflict);
                return false;
            }
            
            node.version = bestVersion;
        }
    }
    
    return true;
}

bool ExtensionDependencyResolver::hasCircularDependencies(std::vector<std::string>& cycle) {
    m_visitState.clear();
    
    for (const auto& candidate : m_candidateExtensions) {
        std::vector<std::string> path;
        if (detectCircularDependenciesInternal(candidate.first, path, cycle)) {
            return true;
        }
    }
    
    return false;
}

bool ExtensionDependencyResolver::detectCircularDependenciesInternal(
    const std::string& extensionId, 
    std::vector<std::string>& path, 
    std::vector<std::string>& cycle
) const {
    auto& state = m_visitState[extensionId];
    
    if (state == 1) {  // Currently visiting - cycle detected
        // Extract cycle
        auto cycleStart = std::find(path.begin(), path.end(), extensionId);
        cycle.assign(cycleStart, path.end());
        cycle.push_back(extensionId);
        return true;
    }
    
    if (state == 2) {  // Already visited
        return false;
    }
    
    state = 1;  // Mark as visiting
    path.push_back(extensionId);
    
    // Visit dependencies
    auto candidateIt = m_candidateExtensions.find(extensionId);
    if (candidateIt != m_candidateExtensions.end()) {
        for (const ExtensionDependency& dep : candidateIt->second.dependencies) {
            if (detectCircularDependenciesInternal(dep.extensionId, path, cycle)) {
                return true;
            }
        }
    }
    
    state = 2;  // Mark as visited
    path.pop_back();
    return false;
}

bool ExtensionDependencyResolver::topologicalSort(std::vector<ExtensionNode>& sortedNodes) const {
    std::unordered_map<std::string, int> inDegree;
    std::unordered_map<std::string, std::vector<std::string>> adjList;
    
    // Build adjacency list and calculate in-degrees
    for (const auto& candidate : m_candidateExtensions) {
        const std::string& extensionId = candidate.first;
        const ExtensionNode& node = candidate.second;
        
        inDegree[extensionId] = 0;  // Initialize
        
        for (const ExtensionDependency& dep : node.dependencies) {
            adjList[dep.extensionId].push_back(extensionId);
            inDegree[extensionId]++;
        }
    }
    
    // Find nodes with no incoming edges
    std::queue<std::string> queue;
    for (const auto& degree : inDegree) {
        if (degree.second == 0) {
            queue.push(degree.first);
        }
    }
    
    // Process nodes
    sortedNodes.clear();
    int order = 0;
    
    while (!queue.empty()) {
        std::string current = queue.front();
        queue.pop();
        
        // Add to sorted result
        auto candidateIt = m_candidateExtensions.find(current);
        if (candidateIt != m_candidateExtensions.end()) {
            ExtensionNode node = candidateIt->second;
            node.installOrder = order++;
            sortedNodes.push_back(node);
        }
        
        // Reduce in-degree of neighbors
        for (const std::string& neighbor : adjList[current]) {
            inDegree[neighbor]--;
            if (inDegree[neighbor] == 0) {
                queue.push(neighbor);
            }
        }
    }
    
    // Check if all nodes were processed (no cycles)
    return sortedNodes.size() == m_candidateExtensions.size();
}

SemanticVersion ExtensionDependencyResolver::findBestVersion(
    const std::string& extensionId, 
    const std::vector<VersionConstraint>& constraints
) {
    std::vector<SemanticVersion> availableVersions = getAvailableVersions(extensionId);
    
    // Sort versions in descending order (latest first) 
    std::sort(availableVersions.begin(), availableVersions.end(), std::greater<SemanticVersion>());
    
    // Find first version that satisfies all constraints
    for (const SemanticVersion& version : availableVersions) {
        bool satisfiesAll = true;
        
        for (const VersionConstraint& constraint : constraints) {
            if (!constraint.isSatisfiedBy(version)) {
                satisfiesAll = false;
                break;
            }
        }
        
        if (satisfiesAll) {
            // Skip pre-release unless explicitly allowed
            if (!version.preRelease.empty() && !m_config.allowPreRelease) {
                continue;
            }
            
            return version;
        }
    }
    
    return SemanticVersion(0, 0, 0);  // No compatible version
}

std::vector<SemanticVersion> ExtensionDependencyResolver::getAvailableVersions(const std::string& extensionId) {
    if (m_versionListProvider) {
        return m_versionListProvider(extensionId);
    }
    
    // Default: return single latest version
    return { SemanticVersion(1, 0, 0) };
}

ExtensionNode ExtensionDependencyResolver::getExtensionInfo(const std::string& extensionId, const std::string& version) {
    if (m_extensionInfoProvider) {
        return m_extensionInfoProvider(extensionId, version);
    }
    
    // Default: create minimal node
    ExtensionNode node;
    node.extensionId = extensionId;
    node.version = SemanticVersion::parse(version == "latest" ? "1.0.0" : version);
    return node;
}

void ExtensionDependencyResolver::reportProgress(const std::string& operation, int current, int total) {
    if (m_progressCallback) {
        m_progressCallback(operation, current, total);
    }
}

std::vector<std::string> ExtensionDependencyResolver::getDependents(const std::string& extensionId) {
    std::vector<std::string> dependents;
    
    for (const auto& installed : m_installedExtensions) {
        const ExtensionNode& node = installed.second;
        
        for (const ExtensionDependency& dep : node.dependencies) {
            if (dep.extensionId == extensionId) {
                dependents.push_back(node.extensionId);
                break;
            }
        }
    }
    
    return dependents;
}

std::vector<std::string> ExtensionDependencyResolver::getDependencies(const std::string& extensionId, bool transitively) {
    std::vector<std::string> dependencies;
    std::unordered_set<std::string> visited;
    
    std::function<void(const std::string&)> collectDeps = [&](const std::string& id) {
        if (visited.find(id) != visited.end()) {
            return;
        }
        visited.insert(id);
        
        auto it = m_installedExtensions.find(id);
        if (it != m_installedExtensions.end()) {
            for (const ExtensionDependency& dep : it->second.dependencies) {
                dependencies.push_back(dep.extensionId);
                
                if (transitively) {
                    collectDeps(dep.extensionId);
                }
            }
        }
    };
    
    collectDeps(extensionId);
    
    // Remove duplicates while preserving order
    auto last = std::unique(dependencies.begin(), dependencies.end());
    dependencies.erase(last, dependencies.end());
    
    return dependencies;
}

void ExtensionDependencyResolver::addInstalledExtension(
    const std::string& extensionId, 
    const SemanticVersion& version, 
    const std::vector<ExtensionDependency>& dependencies
) {
    ExtensionNode node;
    node.extensionId = extensionId;
    node.version = version;
    node.dependencies = dependencies;
    node.isInstalled = true;
    
    m_installedExtensions[extensionId] = node;
    invalidateTopologicalOrder();
}

void ExtensionDependencyResolver::removeInstalledExtension(const std::string& extensionId) {
    m_installedExtensions.erase(extensionId);
    invalidateTopologicalOrder();
}

void ExtensionDependencyResolver::invalidateTopologicalOrder() {
    m_topologicalOrderValid = false;
    m_topologicalOrder.clear();
}

void ExtensionDependencyResolver::printResolutionPlan(const ResolutionPlan& plan) const {
    std::cout << "\n=== Dependency Resolution Plan ===" << std::endl;
    std::cout << "Status: ";
    
    switch (plan.status) {
        case ResolutionStatus::Success:
            std::cout << "SUCCESS" << std::endl;
            break;
        case ResolutionStatus::CircularDependency:
            std::cout << "CIRCULAR DEPENDENCY" << std::endl;
            break;
        case ResolutionStatus::ConflictingVersions:
            std::cout << "VERSION CONFLICTS" << std::endl;
            break;
        case ResolutionStatus::MissingDependency:
            std::cout << "MISSING DEPENDENCY" << std::endl;
            break;
        default:
            std::cout << "FAILED" << std::endl;
            break;
    }
    
    if (!plan.errorMessage.empty()) {
        std::cout << "Error: " << plan.errorMessage << std::endl;
    }
    
    if (!plan.installOrder.empty()) {
        std::cout << "\nInstallation Order:" << std::endl;
        for (size_t i = 0; i < plan.installOrder.size(); ++i) {
            const ExtensionNode& node = plan.installOrder[i];
            std::cout << "  " << (i + 1) << ". " << node.extensionId 
                      << " v" << node.version.toString() << std::endl;
        }
    }
    
    if (!plan.conflicts.empty()) {
        std::cout << "\nConflicts:" << std::endl;
        for (const DependencyConflict& conflict : plan.conflicts) {
            std::cout << "  - " << conflict.extensionId << ": " << conflict.description << std::endl;
        }
    }
    
    if (!plan.warnings.empty()) {
        std::cout << "\nWarnings:" << std::endl;
        for (const std::string& warning : plan.warnings) {
            std::cout << "  - " << warning << std::endl;
        }
    }
    
    std::cout << "================================\n" << std::endl;
}
