// Dependency Detector Implementation
#include "dependency_detector.h"
#include <QJsonDocument>
#include <QFile>
#include <QDir>
#include <QCoreApplication>

// VersionConstraint implementations
bool VersionConstraint::isVersionSatisfied(const QVersionNumber& version) const {
    switch (type) {
        case ConstraintType::Exact:
            return version == exactVersion;
        case ConstraintType::Minimum:
            return version >= minVersion;
        case ConstraintType::Maximum:
            return version <= maxVersion;
        case ConstraintType::Range:
            return version >= minVersion && version <= maxVersion;
        case ConstraintType::Compatible:
            return version.majorVersion() == exactVersion.majorVersion() && version >= exactVersion;
        case ConstraintType::Latest:
        case ConstraintType::Any:
            return true;
    }
    return true;
}

QString VersionConstraint::toString() const {
    switch (type) {
        case ConstraintType::Exact:
            return QString("=%1").arg(exactVersion.toString());
        case ConstraintType::Minimum:
            return QString(">=%1").arg(minVersion.toString());
        case ConstraintType::Maximum:
            return QString("<=%1").arg(maxVersion.toString());
        case ConstraintType::Range:
            return QString(">=%1 <=%2").arg(minVersion.toString(), maxVersion.toString());
        case ConstraintType::Compatible:
            return QString("^%1").arg(exactVersion.toString());
        case ConstraintType::Latest:
            return "latest";
        case ConstraintType::Any:
            return "*";
    }
    return "*";
}

VersionConstraint VersionConstraint::fromString(const QString& constraint) {
    VersionConstraint vc;
    if (constraint.isEmpty() || constraint == "*") {
        vc.type = ConstraintType::Any;
    } else if (constraint.startsWith("^")) {
        vc.type = ConstraintType::Compatible;
        vc.exactVersion = QVersionNumber::fromString(constraint.mid(1));
    } else if (constraint.startsWith(">=")) {
        vc.type = ConstraintType::Minimum;
        vc.minVersion = QVersionNumber::fromString(constraint.mid(2));
    } else if (constraint.startsWith("<=")) {
        vc.type = ConstraintType::Maximum;
        vc.maxVersion = QVersionNumber::fromString(constraint.mid(2));
    } else if (constraint.startsWith("=")) {
        vc.type = ConstraintType::Exact;
        vc.exactVersion = QVersionNumber::fromString(constraint.mid(1));
    } else if (constraint == "latest") {
        vc.type = ConstraintType::Latest;
    } else {
        vc.type = ConstraintType::Exact;
        vc.exactVersion = QVersionNumber::fromString(constraint);
    }
    return vc;
}

// DependencyDetector implementation
DependencyDetector::DependencyDetector(QObject* parent)
    : QObject(parent)
    , m_processingTimer(new QTimer(this))
    , m_metricsTimer(new QTimer(this))
    , m_maintenanceTimer(new QTimer(this))
{
    setupTimers();
    m_uptimeTimer.start();
}

DependencyDetector::~DependencyDetector() = default;

void DependencyDetector::initialize(AgenticExecutor* executor, AdvancedPlanningEngine* planner, 
                                    ErrorAnalysisSystem* errorSystem) {
    m_agenticExecutor = executor;
    m_planningEngine = planner;
    m_errorSystem = errorSystem;
    m_initialized = true;
    initializeComponents();
    loadBuiltInStrategies();
    connectSignals();
}

void DependencyDetector::setupTimers() {
    connect(m_processingTimer, &QTimer::timeout, this, &DependencyDetector::processResolutionQueue);
    connect(m_metricsTimer, &QTimer::timeout, this, &DependencyDetector::updateMetrics);
    connect(m_maintenanceTimer, &QTimer::timeout, this, &DependencyDetector::performMaintenanceTasks);
    
    m_processingTimer->start(1000);  // Process queue every second
    m_metricsTimer->start(30000);    // Update metrics every 30 seconds
    m_maintenanceTimer->start(300000); // Maintenance every 5 minutes
}

void DependencyDetector::initializeComponents() {
    m_metrics["dependencies_count"] = 0;
    m_metrics["conflicts_count"] = 0;
    m_metrics["resolutions_count"] = 0;
    m_metrics["last_analysis"] = QDateTime::currentDateTime().toString(Qt::ISODate);
}

void DependencyDetector::connectSignals() {
    // Connect internal signals
}

void DependencyDetector::loadBuiltInStrategies() {
    // Load default resolution strategies
    ResolutionStrategy upgradeStrategy;
    upgradeStrategy.strategyId = "upgrade_to_latest";
    upgradeStrategy.name = "Upgrade to Latest";
    upgradeStrategy.description = "Upgrade conflicting dependency to latest version";
    upgradeStrategy.priorityScore = 1.0;
    m_strategies[upgradeStrategy.strategyId] = upgradeStrategy;
    
    ResolutionStrategy downgradeStrategy;
    downgradeStrategy.strategyId = "downgrade_compatible";
    downgradeStrategy.name = "Downgrade to Compatible";
    downgradeStrategy.description = "Downgrade to a compatible version";
    downgradeStrategy.priorityScore = 0.8;
    m_strategies[downgradeStrategy.strategyId] = downgradeStrategy;
}

// Slot implementations
void DependencyDetector::processResolutionQueue() {
    QMutexLocker locker(&m_queueMutex);
    if (m_resolutionQueue.isEmpty()) return;
    
    QString conflictId = m_resolutionQueue.dequeue();
    if (!m_activeResolutions.contains(conflictId)) {
        m_activeResolutions.insert(conflictId);
        locker.unlock();
        resolveConflict(conflictId);
        locker.relock();
        m_activeResolutions.remove(conflictId);
    }
}

void DependencyDetector::updateMetrics() {
    QWriteLocker locker(&m_dataLock);
    m_metrics["dependencies_count"] = static_cast<int>(m_dependencies.size());
    m_metrics["conflicts_count"] = static_cast<int>(m_conflicts.size());
    m_metrics["resolutions_count"] = static_cast<int>(m_resolutionResults.size());
    m_metrics["uptime_seconds"] = m_uptimeTimer.elapsed() / 1000;
    m_metrics["last_update"] = QDateTime::currentDateTime().toString(Qt::ISODate);
}

void DependencyDetector::performMaintenanceTasks() {
    cleanupCompletedResolutions();
    updateCacheIfNeeded();
}

void DependencyDetector::updateDependencyDatabase() {
    // Update the dependency database from remote sources
    QWriteLocker locker(&m_dataLock);
    m_lastCacheUpdate = QDateTime::currentDateTime();
    emit dependencyUpdated("database");
}

void DependencyDetector::onDependencyAdded(const QString& dependencyId) {
    emit dependencyRegistered(dependencyId);
}

void DependencyDetector::onDependencyRemoved(const QString& dependencyId) {
    emit dependencyRemoved(dependencyId);
}

void DependencyDetector::onConflictDetected(const QString& conflictId) {
    if (m_autoResolutionEnabled) {
        QMutexLocker locker(&m_queueMutex);
        m_resolutionQueue.enqueue(conflictId);
    }
}

void DependencyDetector::performPeriodicAnalysis() {
    detectConflicts();
    m_lastAnalysis = QDateTime::currentDateTime();
}

// Dependency management implementations
bool DependencyDetector::registerDependency(const DependencySpec& spec) {
    QWriteLocker locker(&m_dataLock);
    m_dependencies[spec.id] = spec;
    emit dependencyRegistered(spec.id);
    return true;
}

bool DependencyDetector::removeDependency(const QString& dependencyId) {
    QWriteLocker locker(&m_dataLock);
    if (m_dependencies.find(dependencyId) != m_dependencies.end()) {
        m_dependencies.erase(dependencyId);
        emit dependencyRemoved(dependencyId);
        return true;
    }
    return false;
}

DependencySpec DependencyDetector::getDependency(const QString& dependencyId) const {
    QReadLocker locker(&m_dataLock);
    auto it = m_dependencies.find(dependencyId);
    if (it != m_dependencies.end()) {
        return it->second;
    }
    return DependencySpec();
}

QStringList DependencyDetector::getAllDependencies() const {
    QReadLocker locker(&m_dataLock);
    QStringList result;
    for (const auto& [id, spec] : m_dependencies) {
        result.append(id);
    }
    return result;
}

QStringList DependencyDetector::getDependenciesByType(DependencyRelationType type) const {
    QReadLocker locker(&m_dataLock);
    QStringList result;
    for (const auto& [id, spec] : m_dependencies) {
        if (spec.type == type) {
            result.append(id);
        }
    }
    return result;
}

bool DependencyDetector::updateDependency(const QString& dependencyId, const DependencySpec& spec) {
    QWriteLocker locker(&m_dataLock);
    if (m_dependencies.find(dependencyId) != m_dependencies.end()) {
        m_dependencies[dependencyId] = spec;
        emit dependencyUpdated(dependencyId);
        return true;
    }
    return false;
}

// Discovery and analysis
QStringList DependencyDetector::discoverDependencies(const QString& projectPath) {
    QStringList discovered;
    QDir projectDir(projectPath);
    
    // Check for package.json
    if (projectDir.exists("package.json")) {
        QFile file(projectDir.filePath("package.json"));
        if (file.open(QIODevice::ReadOnly)) {
            QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
            QJsonObject deps = doc.object()["dependencies"].toObject();
            for (auto it = deps.begin(); it != deps.end(); ++it) {
                discovered.append(it.key());
            }
        }
    }
    
    // Check for CMakeLists.txt
    if (projectDir.exists("CMakeLists.txt")) {
        // Parse CMake find_package calls
    }
    
    return discovered;
}

QStringList DependencyDetector::analyzeDependencyTree(const QString& rootDependency) {
    QStringList tree;
    QReadLocker locker(&m_dataLock);
    
    auto it = m_dependencyGraph.find(rootDependency);
    if (it != m_dependencyGraph.end()) {
        // BFS traversal
        tree.append(rootDependency);
        for (const auto& childId : it->second.dependencies) {
            tree.append(analyzeDependencyTree(childId));
        }
    }
    
    return tree;
}

QJsonObject DependencyDetector::generateDependencyGraph() {
    QReadLocker locker(&m_dataLock);
    QJsonObject graph;
    QJsonArray nodes, edges;
    
    for (const auto& [id, node] : m_dependencyGraph) {
        QJsonObject nodeObj;
        nodeObj["id"] = id;
        nodeObj["name"] = node.spec.name;
        nodeObj["version"] = node.spec.currentVersion;
        nodes.append(nodeObj);
        
        for (const auto& childId : node.dependencies) {
            QJsonObject edge;
            edge["source"] = id;
            edge["target"] = childId;
            edges.append(edge);
        }
    }
    
    graph["nodes"] = nodes;
    graph["edges"] = edges;
    return graph;
}

QJsonObject DependencyDetector::analyzeDependencyMetrics() {
    QReadLocker locker(&m_dataLock);
    return m_metrics;
}

QStringList DependencyDetector::findOrphanedDependencies() {
    QReadLocker locker(&m_dataLock);
    QStringList orphaned;
    
    for (const auto& [id, node] : m_dependencyGraph) {
        if (node.dependents.isEmpty() && node.importance < 0.1) {
            orphaned.append(id);
        }
    }
    
    return orphaned;
}

QStringList DependencyDetector::findUnusedDependencies() {
    return findOrphanedDependencies();  // Same logic for now
}

// Conflict detection
QStringList DependencyDetector::detectConflicts() {
    QStringList conflicts;
    conflicts.append(detectVersionConflicts());
    conflicts.append(detectCircularDependencies());
    conflicts.append(detectPlatformConflicts());
    return conflicts;
}

QStringList DependencyDetector::detectCircularDependencies() {
    return detectCycles();
}

QStringList DependencyDetector::detectVersionConflicts() {
    return detectVersionConflictsInternal();
}

QStringList DependencyDetector::detectPlatformConflicts() {
    return detectPlatformConflictsInternal();
}

DependencyConflict DependencyDetector::getConflictDetails(const QString& conflictId) const {
    QReadLocker locker(&m_dataLock);
    auto it = m_conflicts.find(conflictId);
    if (it != m_conflicts.end()) {
        return it->second;
    }
    return DependencyConflict();
}

double DependencyDetector::assessConflictSeverity(const QString& conflictId) const {
    QReadLocker locker(&m_dataLock);
    auto it = m_conflicts.find(conflictId);
    if (it != m_conflicts.end()) {
        return it->second.severityScore;
    }
    return 0.0;
}

// Resolution
bool DependencyDetector::addResolutionStrategy(const ResolutionStrategy& strategy) {
    QWriteLocker locker(&m_dataLock);
    m_strategies[strategy.strategyId] = strategy;
    return true;
}

bool DependencyDetector::removeResolutionStrategy(const QString& strategyId) {
    QWriteLocker locker(&m_dataLock);
    return m_strategies.erase(strategyId) > 0;
}

QStringList DependencyDetector::getAvailableStrategies() const {
    QReadLocker locker(&m_dataLock);
    QStringList result;
    for (const auto& [id, strategy] : m_strategies) {
        result.append(id);
    }
    return result;
}

QStringList DependencyDetector::getApplicableStrategies(const QString& conflictId) const {
    QReadLocker locker(&m_dataLock);
    QStringList applicable;
    auto it = m_conflicts.find(conflictId);
    if (it != m_conflicts.end()) {
        for (const auto& strategyId : it->second.potentialResolutions) {
            applicable.append(strategyId);
        }
    }
    return applicable;
}

ResolutionStrategy DependencyDetector::getBestStrategy(const QString& conflictId) const {
    QReadLocker locker(&m_dataLock);
    auto it = m_conflicts.find(conflictId);
    if (it != m_conflicts.end() && !it->second.potentialResolutions.isEmpty()) {
        QString bestId = it->second.potentialResolutions.first();
        auto strategyIt = m_strategies.find(bestId);
        if (strategyIt != m_strategies.end()) {
            return strategyIt->second;
        }
    }
    return ResolutionStrategy();
}

QString DependencyDetector::resolveAllConflicts() {
    QString sessionId = generateResolutionId();
    for (const auto& [id, conflict] : m_conflicts) {
        resolveConflict(id);
    }
    return sessionId;
}

QString DependencyDetector::resolveConflict(const QString& conflictId) {
    ResolutionStrategy strategy = getBestStrategy(conflictId);
    return resolveConflictWithStrategy(conflictId, strategy.strategyId);
}

QString DependencyDetector::resolveConflictWithStrategy(const QString& conflictId, const QString& strategyId) {
    QString resolutionId = generateResolutionId();
    emit resolutionStarted(resolutionId);
    
    QWriteLocker locker(&m_dataLock);
    auto strategyIt = m_strategies.find(strategyId);
    if (strategyIt != m_strategies.end()) {
        ResolutionResult result;
        result.resolutionId = resolutionId;
        result.success = true;
        result.completedAt = QDateTime::currentDateTime();
        m_resolutionResults[resolutionId] = result;
        
        m_conflicts.erase(conflictId);
        emit conflictResolved(conflictId, strategyId);
    }
    
    emit resolutionCompleted(resolutionId, true);
    return resolutionId;
}

ResolutionResult DependencyDetector::getResolutionResult(const QString& resolutionId) const {
    QReadLocker locker(&m_dataLock);
    auto it = m_resolutionResults.find(resolutionId);
    if (it != m_resolutionResults.end()) {
        return it->second;
    }
    return ResolutionResult();
}

bool DependencyDetector::isResolutionComplete(const QString& resolutionId) const {
    QReadLocker locker(&m_dataLock);
    auto it = m_resolutionResults.find(resolutionId);
    return it != m_resolutionResults.end() && it->second.success;
}

// Installation
QString DependencyDetector::installDependency(const QString& dependencyId) {
    QString resolutionId = generateResolutionId();
    emit dependencyInstalled(dependencyId, "latest");
    return resolutionId;
}

QString DependencyDetector::updateDependency(const QString& dependencyId, const QString& targetVersion) {
    QString resolutionId = generateResolutionId();
    emit dependencyUpdated(dependencyId);
    return resolutionId;
}

QString DependencyDetector::removeDependencyChain(const QString& dependencyId) {
    QString resolutionId = generateResolutionId();
    removeDependency(dependencyId);
    return resolutionId;
}

bool DependencyDetector::verifyDependencyIntegrity(const QString& dependencyId) {
    QReadLocker locker(&m_dataLock);
    return m_dependencies.find(dependencyId) != m_dependencies.end();
}

QStringList DependencyDetector::getInstallationOrder(const QStringList& dependencyIds) {
    return topologicalSort();
}

// Version management
QStringList DependencyDetector::getAvailableVersions(const QString& dependencyId) {
    return QStringList{"1.0.0", "1.1.0", "2.0.0"};  // Placeholder
}

QString DependencyDetector::getLatestVersion(const QString& dependencyId) {
    return "2.0.0";  // Placeholder
}

QString DependencyDetector::getBestCompatibleVersion(const QString& dependencyId, const VersionConstraint& constraint) {
    QStringList versions = getAvailableVersions(dependencyId);
    for (const QString& v : versions) {
        if (constraint.isVersionSatisfied(QVersionNumber::fromString(v))) {
            return v;
        }
    }
    return QString();
}

bool DependencyDetector::isVersionCompatible(const QString& dependencyId, const QString& version) {
    return true;  // Placeholder
}

QJsonObject DependencyDetector::compareVersions(const QString& version1, const QString& version2) {
    QJsonObject result;
    QVersionNumber v1 = QVersionNumber::fromString(version1);
    QVersionNumber v2 = QVersionNumber::fromString(version2);
    result["comparison"] = QVersionNumber::compare(v1, v2);
    return result;
}

// Platform compatibility
bool DependencyDetector::isPlatformCompatible(const QString& dependencyId) const {
    return true;  // Placeholder
}

QJsonObject DependencyDetector::getCurrentPlatformInfo() const {
    return createPlatformInfo();
}

QStringList DependencyDetector::getCompatiblePlatforms(const QString& dependencyId) const {
    return QStringList{"Windows", "Linux", "macOS"};
}

bool DependencyDetector::checkPlatformRequirements(const QString& dependencyId) const {
    return true;
}

// Advanced analysis
QStringList DependencyDetector::findUpdateCandidates() {
    QStringList candidates;
    QReadLocker locker(&m_dataLock);
    for (const auto& [id, node] : m_dependencyGraph) {
        // Find nodes that could potentially be updated
        if (!node.dependents.isEmpty()) {
            candidates.append(id);
        }
    }
    return candidates;
}

QStringList DependencyDetector::findSecurityVulnerabilities() {
    return QStringList();  // Placeholder
}

QJsonObject DependencyDetector::generateCompatibilityMatrix() {
    return QJsonObject();  // Placeholder
}

QStringList DependencyDetector::suggestOptimizations() {
    return findOrphanedDependencies();
}

double DependencyDetector::calculateDependencyHealth() const {
    QReadLocker locker(&m_dataLock);
    if (m_dependencies.empty()) return 1.0;
    
    int conflicts = m_conflicts.size();
    int total = m_dependencies.size();
    return 1.0 - (static_cast<double>(conflicts) / total);
}

// Caching
void DependencyDetector::clearCache() {
    QWriteLocker locker(&m_dataLock);
    m_dependencyCache = QJsonObject();
    m_versionCache = QJsonObject();
}

void DependencyDetector::optimizeDependencyTree() {
    // Optimize the dependency tree
}

QJsonObject DependencyDetector::getCacheStatistics() const {
    QReadLocker locker(&m_dataLock);
    QJsonObject stats;
    stats["dependency_cache_size"] = m_dependencyCache.size();
    stats["version_cache_size"] = m_versionCache.size();
    stats["last_update"] = m_lastCacheUpdate.toString(Qt::ISODate);
    return stats;
}

// Configuration
void DependencyDetector::loadConfiguration(const QJsonObject& config) {
    m_resolutionTimeoutMs = config["resolution_timeout_ms"].toInt(300000);
    m_autoResolutionEnabled = config["auto_resolution_enabled"].toBool(true);
    m_cachingEnabled = config["caching_enabled"].toBool(true);
}

QJsonObject DependencyDetector::saveConfiguration() const {
    QJsonObject config;
    config["resolution_timeout_ms"] = m_resolutionTimeoutMs;
    config["auto_resolution_enabled"] = m_autoResolutionEnabled;
    config["caching_enabled"] = m_cachingEnabled;
    return config;
}

// Export/Import
QString DependencyDetector::exportDependencyGraph(const QString& format) const {
    QJsonObject graph = const_cast<DependencyDetector*>(this)->generateDependencyGraph();
    return QString::fromUtf8(QJsonDocument(graph).toJson());
}

QString DependencyDetector::exportResolutionReport(const QString& resolutionId) const {
    ResolutionResult result = getResolutionResult(resolutionId);
    QJsonObject report;
    report["resolution_id"] = result.resolutionId;
    report["success"] = result.success;
    report["error"] = result.errorMessage;
    return QString::fromUtf8(QJsonDocument(report).toJson());
}

bool DependencyDetector::importDependencies(const QString& filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) return false;
    
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    // Import dependencies from JSON
    return true;
}

QString DependencyDetector::generateDependencyReport() const {
    return exportDependencyGraph("json");
}

// Private methods
QStringList DependencyDetector::topologicalSort() {
    QStringList sorted;
    QSet<QString> visited;
    
    std::function<void(const QString&)> visit = [&](const QString& nodeId) {
        if (visited.contains(nodeId)) return;
        visited.insert(nodeId);
        
        auto it = m_dependencyGraph.find(nodeId);
        if (it != m_dependencyGraph.end()) {
            for (const auto& childId : it->second.dependencies) {
                visit(childId);
            }
        }
        sorted.prepend(nodeId);
    };
    
    for (const auto& [id, node] : m_dependencyGraph) {
        visit(id);
    }
    
    return sorted;
}

QStringList DependencyDetector::detectCycles() {
    QStringList cycles;
    // Implement cycle detection using DFS
    return cycles;
}

void DependencyDetector::calculateNodeImportance() {
    // Calculate importance based on dependency count
}

QStringList DependencyDetector::findCriticalPath(const QString& startNode, const QString& endNode) {
    return QStringList();  // Placeholder
}

QStringList DependencyDetector::detectVersionConflictsInternal() {
    return QStringList();  // Placeholder
}

QStringList DependencyDetector::detectCircularDependenciesInternal() {
    return detectCycles();
}

QStringList DependencyDetector::detectPlatformConflictsInternal() {
    return QStringList();  // Placeholder
}

QString DependencyDetector::executeResolutionStrategy(const QString& conflictId, const ResolutionStrategy& strategy) {
    return generateResolutionId();
}

bool DependencyDetector::validateResolutionPlan(const QJsonObject& plan) {
    return true;
}

QString DependencyDetector::applyResolutionPlan(const QJsonObject& plan) {
    return generateResolutionId();
}

QString DependencyDetector::generateResolutionId() {
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

QString DependencyDetector::generateConflictId() {
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

bool DependencyDetector::isVersionSatisfied(const QString& version, const VersionConstraint& constraint) const {
    return constraint.isVersionSatisfied(QVersionNumber::fromString(version));
}

QJsonObject DependencyDetector::createPlatformInfo() const {
    QJsonObject info;
#ifdef Q_OS_WIN
    info["os"] = "Windows";
#elif defined(Q_OS_LINUX)
    info["os"] = "Linux";
#elif defined(Q_OS_MACOS)
    info["os"] = "macOS";
#endif

#ifdef Q_PROCESSOR_X86_64
    info["arch"] = "x86_64";
#elif defined(Q_PROCESSOR_ARM)
    info["arch"] = "arm64";
#endif
    
    return info;
}

void DependencyDetector::updateCacheIfNeeded() {
    QDateTime now = QDateTime::currentDateTime();
    if (m_lastCacheUpdate.isNull() || m_lastCacheUpdate.secsTo(now) > m_cacheExpiryMinutes * 60) {
        updateDependencyDatabase();
    }
}

void DependencyDetector::cleanupCompletedResolutions() {
    QWriteLocker locker(&m_dataLock);
    QDateTime cutoff = QDateTime::currentDateTime().addDays(-7);
    
    for (auto it = m_resolutionResults.begin(); it != m_resolutionResults.end(); ) {
        if (it->second.completedAt < cutoff) {
            it = m_resolutionResults.erase(it);
        } else {
            ++it;
        }
    }
}
