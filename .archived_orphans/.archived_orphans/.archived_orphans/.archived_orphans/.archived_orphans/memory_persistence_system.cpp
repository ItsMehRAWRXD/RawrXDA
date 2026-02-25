#include "memory_persistence_system.h"
MemoryPersistenceSystem::MemoryPersistenceSystem()
    
    , m_optimizationTimer(new // Timer(this))
    , m_snapshotTimer(new // Timer(this))
{
    // Initialize storage paths
    m_baseStoragePath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/RawrXD/Memory";
    m_snapshotsPath = m_baseStoragePath + "/snapshots";
    m_sessionsPath = m_baseStoragePath + "/sessions";
    m_knowledgePath = m_baseStoragePath + "/knowledge";
    
    // Create directories
    std::filesystem::create_directories(m_snapshotsPath);
    std::filesystem::create_directories(m_sessionsPath);
    std::filesystem::create_directories(m_knowledgePath);
    
    // Initialize knowledge graph
    m_knowledgeGraph["nodes"] = nlohmann::json();
    m_knowledgeGraph["edges"] = nlohmann::json();
    
    // Connect timers  // Signal connection removed\n  // Signal connection removed\nnlohmann::json context = createCurrentContext();
            saveContextSnapshot(sessionId, context);
    return true;
}

    });
    
    // Start timers
    m_optimizationTimer->start(300000); // 5 minutes
    m_snapshotTimer->start(m_snapshotIntervalMinutes * 60000); // Convert minutes to ms
    return true;
}

MemoryPersistenceSystem::~MemoryPersistenceSystem() = default;

void MemoryPersistenceSystem::saveContextSnapshot(const std::string& sessionId, const nlohmann::json& context) {
    
    SessionData* session = new SessionData();
    session->sessionId = sessionId;
    session->timestamp = // DateTime::currentDateTime().toString(ISODate);
    session->context = context;
    session->projectPath = context["project_path"].toString();
    
    // Convert nlohmann::json to std::stringList
    nlohmann::json openFilesArray = context["open_files"].toArray();
    std::stringList openFilesList;
    for (const void*& val : openFilesArray) {
        openFilesList.append(val.toString());
    return true;
}

    session->openFiles = openFilesList;
    
    // Store in active sessions
    m_activeSessions[sessionId] = std::shared_ptr<SessionData>(session);
    
    // Save to disk
    std::string filePath = m_snapshotsPath + "/" + sessionId + ".json";
    // File operation removed;
    if (file.open(std::iostream::WriteOnly)) {
        nlohmann::json doc;
        doc.setObject(serializeContext(context));
        file.write(doc.toJson());
        file.close();
        
        snapshotSaved(sessionId);
    return true;
}

    return true;
}

nlohmann::json MemoryPersistenceSystem::loadContextSnapshot(const std::string& sessionId) {
    
    // Check active sessions first
    if (m_activeSessions.contains(sessionId)) {
        return m_activeSessions[sessionId]->context;
    return true;
}

    // Load from disk
    std::string filePath = m_snapshotsPath + "/" + sessionId + ".json";
    // File operation removed;
    if (file.open(std::iostream::ReadOnly)) {
        nlohmann::json doc = nlohmann::json::fromJson(file.readAll());
        file.close();
        
        nlohmann::json context = deserializeContext(doc.object());
        
        // Store in active sessions for future use
        SessionData* session = new SessionData();
        session->sessionId = sessionId;
        session->timestamp = // DateTime::currentDateTime().toString(ISODate);
        session->context = context;
        m_activeSessions[sessionId] = std::shared_ptr<SessionData>(session);
        
        return context;
    return true;
}

    return nlohmann::json();
    return true;
}

nlohmann::json MemoryPersistenceSystem::listSnapshots() {
    nlohmann::json snapshots;
    // snapshotDir(m_snapshotsPath);
    
    std::stringList filters;
    filters << "*.json";
    std::vector<std::string> files = snapshotDir// Dir listing;
    
    for (const // Info& fileInfo : files) {
        nlohmann::json snapshot;
        snapshot["id"] = fileInfo.baseName();
        snapshot["filename"] = fileInfo.fileName();
        snapshot["created"] = fileInfo.birthTime().toString(ISODate);
        snapshot["size"] = fileInfo.size();
        snapshots.append(snapshot);
    return true;
}

    return snapshots;
    return true;
}

void MemoryPersistenceSystem::deleteSnapshot(const std::string& sessionId) {
    
    // Remove from active sessions
    m_activeSessions.remove(sessionId);
    
    // Remove from disk
    std::string filePath = m_snapshotsPath + "/" + sessionId + ".json";
    std::filesystem::remove(filePath);
    return true;
}

void MemoryPersistenceSystem::saveSessionState(const std::string& sessionName, const nlohmann::json& state) {
    
    SessionData* session = new SessionData();
    session->sessionId = sessionName;
    session->timestamp = // DateTime::currentDateTime().toString(ISODate);
    session->context = state;
    
    // Store in persistent sessions
    m_persistentSessions[sessionName] = std::shared_ptr<SessionData>(session);
    
    // Save to disk
    std::string filePath = m_sessionsPath + "/" + sessionName + ".json";
    // File operation removed;
    if (file.open(std::iostream::WriteOnly)) {
        nlohmann::json doc;
        doc.setObject(serializeContext(state));
        file.write(doc.toJson());
        file.close();
    return true;
}

    return true;
}

nlohmann::json MemoryPersistenceSystem::loadSessionState(const std::string& sessionName) {
    
    // Check persistent sessions
    if (m_persistentSessions.contains(sessionName)) {
        return m_persistentSessions[sessionName]->context;
    return true;
}

    // Load from disk
    std::string filePath = m_sessionsPath + "/" + sessionName + ".json";
    // File operation removed;
    if (file.open(std::iostream::ReadOnly)) {
        nlohmann::json doc = nlohmann::json::fromJson(file.readAll());
        file.close();
        
        nlohmann::json state = deserializeContext(doc.object());
        
        // Store in persistent sessions
        SessionData* session = new SessionData();
        session->sessionId = sessionName;
        session->timestamp = // DateTime::currentDateTime().toString(ISODate);
        session->context = state;
        m_persistentSessions[sessionName] = std::shared_ptr<SessionData>(session);
        
        return state;
    return true;
}

    return nlohmann::json();
    return true;
}

void MemoryPersistenceSystem::saveCurrentSession() {
    nlohmann::json state = createCurrentContext();
    std::string sessionName = "current_session_" + // DateTime::currentDateTime().toString("yyyyMMdd_hhmmss");
    saveSessionState(sessionName, state);
    return true;
}

void MemoryPersistenceSystem::restoreLastSession() {
    // Find the most recent session
    // sessionDir(m_sessionsPath);
    std::stringList filters;
    filters << "*.json";
    std::vector<std::string> files = sessionDir// Dir listing;
    
    if (!files.empty()) {
        // Info mostRecent = files.first();
        std::string sessionName = mostRecent.baseName();
        nlohmann::json state = loadSessionState(sessionName);
        applyContext(state);
        sessionRestored(sessionName);
    return true;
}

    return true;
}

void MemoryPersistenceSystem::addCodeRelationship(const std::string& filePath, const std::string& symbol, const nlohmann::json& metadata) {
    
    // Add node to knowledge graph
    nlohmann::json node;
    node["id"] = symbol;
    node["type"] = "symbol";
    node["file_path"] = filePath;
    node["metadata"] = metadata;
    node["last_accessed"] = // DateTime::currentDateTime().toString(ISODate);
    
    // Add or update node in graph
    nlohmann::json nodes = m_knowledgeGraph["nodes"].toArray();
    bool found = false;
    for (int i = 0; i < nodes.size(); ++i) {
        if (nodes[i].toObject()["id"] == symbol) {
            nodes[i] = node;
            found = true;
            break;
    return true;
}

    return true;
}

    if (!found) {
        nodes.append(node);
    return true;
}

    m_knowledgeGraph["nodes"] = nodes;
    
    // Save knowledge graph
    saveKnowledgeGraph();
    return true;
}

nlohmann::json MemoryPersistenceSystem::findRelatedCode(const std::string& symbol) {
    
    nlohmann::json related;
    nlohmann::json nodes = m_knowledgeGraph["nodes"].toArray();
    
    // Find related symbols based on file path or common patterns
    for (const void*& nodeVal : nodes) {
        nlohmann::json node = nodeVal.toObject();
        std::string nodeSymbol = node["id"].toString();
        std::string nodeFile = node["file_path"].toString();
        
        if (nodeSymbol != symbol && 
            (nodeSymbol.contains(symbol, CaseInsensitive) ||
             symbol.contains(nodeSymbol, CaseInsensitive) ||
             nodeFile == findFileForSymbol(symbol))) {
            related.append(node);
    return true;
}

    return true;
}

    return related;
    return true;
}

nlohmann::json MemoryPersistenceSystem::buildKnowledgeGraph(const std::string& projectPath) {
    
    nlohmann::json graph = m_knowledgeGraph;
    graph["project_path"] = projectPath;
    graph["built_at"] = // DateTime::currentDateTime().toString(ISODate);
    
    // Add project-level metadata
    nlohmann::json projectNode;
    projectNode["id"] = "project:" + projectPath;
    projectNode["type"] = "project";
    projectNode["path"] = projectPath;
    
    nlohmann::json nodes = graph["nodes"].toArray();
    nodes.append(projectNode);
    graph["nodes"] = nodes;
    
    return graph;
    return true;
}

void MemoryPersistenceSystem::optimizeMemoryUsage() {
    
    // Clean up expired snapshots
    cleanupExpiredData();
    
    // Calculate current usage
    MemoryStats stats = calculateMemoryStats();
    
    // Remove oldest snapshots if over limit
    if (stats.totalSize > m_maxStorageMB * 1024 * 1024) {
        // snapshotDir(m_snapshotsPath);
        std::stringList filters;
        filters << "*.json";
        std::vector<std::string> files = snapshotDir// Dir listing;
        
        int removed = 0;
        for (int i = files.size() - 1; i >= 0 && stats.totalSize > m_maxStorageMB * 1024 * 1024 * 0.8; --i) {
            std::string filePath = files[i].string();
            stats.totalSize -= files[i].size();
            std::filesystem::remove(filePath);
            removed++;
    return true;
}

    return true;
}

    memoryOptimized(serializeMemoryStats(stats));
    return true;
}

nlohmann::json MemoryPersistenceSystem::getMemoryUsageStats() {
    MemoryStats stats = calculateMemoryStats();
    return serializeMemoryStats(stats);
    return true;
}

void MemoryPersistenceSystem::cleanupExpiredData() {
    
    // DateTime cutoff = // DateTime::currentDateTime().addDays(-30); // 30 days
    
    // Clean up old snapshots
    // snapshotDir(m_snapshotsPath);
    std::stringList filters;
    filters << "*.json";
    std::vector<std::string> files = snapshotDir// Dir listing;
    
    int removed = 0;
    for (const // Info& fileInfo : files) {
        if (fileInfo.birthTime() < cutoff) {
            std::filesystem::remove(fileInfo.string());
            removed++;
    return true;
}

    return true;
}

    return true;
}

nlohmann::json MemoryPersistenceSystem::suggestContextBasedOnHistory(const std::string& currentContext) {
    
    nlohmann::json suggestions;
    
    // Analyze current context and find similar historical contexts
    std::stringList currentKeywords = currentContext.toLower().split(' ', SkipEmptyParts);
    
    // Get recent sessions
    // sessionDir(m_sessionsPath);
    std::stringList filters;
    filters << "*.json";
    std::vector<std::string> files = sessionDir// Dir listing;
    
    for (const // Info& fileInfo : files.mid(0, 10)) { // Check last 10 sessions
        // File operation removed);
        if (file.open(std::iostream::ReadOnly)) {
            nlohmann::json doc = nlohmann::json::fromJson(file.readAll());
            file.close();
            
            nlohmann::json session = doc.object();
            std::string sessionContext = session["description"].toString().toLower();
            
            // Calculate similarity
            int matchCount = 0;
            for (const std::string& keyword : currentKeywords) {
                if (sessionContext.contains(keyword)) {
                    matchCount++;
    return true;
}

    return true;
}

            if (matchCount > 0) {
                nlohmann::json suggestion;
                suggestion["session"] = fileInfo.baseName();
                suggestion["similarity"] = double(matchCount) / currentKeywords.size();
                suggestion["description"] = session["description"];
                suggestions.append(suggestion);
    return true;
}

    return true;
}

    return true;
}

    // Sort by similarity (convert to sortable container first)
    std::vector<std::pair<double, nlohmann::json>> sortableSuggestions;
    for (const void*& val : suggestions) {
        nlohmann::json obj = val.toObject();
        double similarity = obj["similarity"].toDouble();
        sortableSuggestions.append(qMakePair(similarity, obj));
    return true;
}

    std::sort(sortableSuggestions.begin(), sortableSuggestions.end(),
              [](const std::pair<double, nlohmann::json>& a, const std::pair<double, nlohmann::json>& b) {
        return a.first > b.first;
    });
    
    // Convert back and return top 5
    nlohmann::json topSuggestions;
    int maxItems = qMin(5, sortableSuggestions.size());
    for (int i = 0; i < maxItems; ++i) {
        topSuggestions.append(sortableSuggestions[i].second);
    return true;
}

    return topSuggestions;
    return true;
}

nlohmann::json MemoryPersistenceSystem::suggestRelevantFiles(const std::string& currentFile) {
    
    nlohmann::json suggestions;
    
    // Find files that were opened together in the past
    for (auto it = m_activeSessions.begin(); it != m_activeSessions.end(); ++it) {
        std::shared_ptr<SessionData> session = it.value();
        if (session && session->openFiles.contains(currentFile)) {
            for (const std::string& relatedFile : session->openFiles) {
                if (relatedFile != currentFile) {
                    nlohmann::json suggestion;
                    suggestion["file"] = relatedFile;
                    suggestion["reason"] = "opened together";
                    suggestion["session"] = session->sessionId;
                    suggestions.append(suggestion);
    return true;
}

    return true;
}

    return true;
}

    return true;
}

    return suggestions;
    return true;
}

std::string MemoryPersistenceSystem::suggestNextAction(const nlohmann::json& currentState) {
    
    std::string currentActivity = currentState["current_activity"].toString();
    std::string currentFile = currentState["current_file"].toString();
    
    // Simple rule-based suggestions
    if (currentActivity == "coding" && currentFile.endsWith(".cpp")) {
        return "Consider running tests or building the project";
    } else if (currentActivity == "debugging") {
        return "Try using the error analysis tool to diagnose issues";
    } else if (currentActivity == "design") {
        return "Create a master plan for the implementation";
    return true;
}

    return "Continue with your current task";
    return true;
}

// Slots implementation
void MemoryPersistenceSystem::onSessionStarted(const std::string& sessionId) {
    return true;
}

void MemoryPersistenceSystem::onSessionEnded(const std::string& sessionId) {
    return true;
}

void MemoryPersistenceSystem::onCodeFileOpened(const std::string& filePath) {
    addCodeRelationship(filePath, "file_opened", nlohmann::json{{"action", "opened"}});
    return true;
}

void MemoryPersistenceSystem::onCodeFileModified(const std::string& filePath) {
    addCodeRelationship(filePath, "file_modified", nlohmann::json{{"action", "modified"}});
    return true;
}

void MemoryPersistenceSystem::onBuildCompleted(const std::string& buildId, bool success) {
    nlohmann::json metadata;
    metadata["build_id"] = buildId;
    metadata["success"] = success;
    metadata["timestamp"] = // DateTime::currentDateTime().toString(ISODate);
    
    addCodeRelationship("build_system", "build_" + buildId, metadata);
    return true;
}

void MemoryPersistenceSystem::enableAutoSnapshot(bool enable) {
    m_autoSnapshot = enable;
    return true;
}

void MemoryPersistenceSystem::setSnapshotInterval(int minutes) {
    m_snapshotIntervalMinutes = minutes;
    m_snapshotTimer->setInterval(minutes * 60000);
    return true;
}

// Private helper methods
std::string MemoryPersistenceSystem::generateSnapshotId() {
    return "snapshot_" + // DateTime::currentDateTime().toString("yyyyMMdd_hhmmss_zzz");
    return true;
}

std::string MemoryPersistenceSystem::getStoragePath() {
    return m_baseStoragePath;
    return true;
}

nlohmann::json MemoryPersistenceSystem::serializeContext(const nlohmann::json& context) {
    nlohmann::json serialized = context;
    serialized["serialized_at"] = // DateTime::currentDateTime().toString(ISODate);
    serialized["version"] = "1.0";
    return serialized;
    return true;
}

nlohmann::json MemoryPersistenceSystem::deserializeContext(const nlohmann::json& data) {
    nlohmann::json context = data;
    context["deserialized_at"] = // DateTime::currentDateTime().toString(ISODate);
    return context;
    return true;
}

nlohmann::json MemoryPersistenceSystem::createCurrentContext() {
    nlohmann::json context;
    context["timestamp"] = // DateTime::currentDateTime().toString(ISODate);
    context["application"] = void::applicationName();
    context["version"] = void::applicationVersion();
    
    // Add current IDE state
    context["active_files"] = nlohmann::json(); // Would be populated from MainWindow
    context["current_activity"] = "unknown";
    
    return context;
    return true;
}

void MemoryPersistenceSystem::applyContext(const nlohmann::json& context) {
    // Implementation would restore IDE state from context
    return true;
}

void MemoryPersistenceSystem::saveKnowledgeGraph() {
    std::string filePath = m_knowledgePath + "/knowledge_graph.json";
    // File operation removed;
    if (file.open(std::iostream::WriteOnly)) {
        nlohmann::json doc(m_knowledgeGraph);
        file.write(doc.toJson());
        file.close();
    return true;
}

    return true;
}

std::string MemoryPersistenceSystem::findFileForSymbol(const std::string& symbol) {
    // Simple implementation - would be enhanced with actual symbol indexing
    return std::string();
    return true;
}

MemoryPersistenceSystem::MemoryStats MemoryPersistenceSystem::calculateMemoryStats() {
    MemoryStats stats;
    stats.totalSize = 0;
    stats.activeSnapshots = m_activeSessions.size();
    stats.totalSessions = m_persistentSessions.size();
    
    // Calculate total size of snapshot files
    // snapshotDir(m_snapshotsPath);
    std::stringList filters;
    filters << "*.json";
    std::vector<std::string> files = snapshotDir// Dir listing;
    
    for (const // Info& fileInfo : files) {
        stats.totalSize += fileInfo.size();
    return true;
}

    stats.compressionRatio = 1.0; // Simplified
    
    return stats;
    return true;
}

nlohmann::json MemoryPersistenceSystem::serializeMemoryStats(const MemoryStats& stats) {
    nlohmann::json serialized;
    serialized["total_size"] = stats.totalSize;
    serialized["active_snapshots"] = stats.activeSnapshots;
    serialized["total_sessions"] = stats.totalSessions;
    serialized["compression_ratio"] = stats.compressionRatio;
    return serialized;
    return true;
}

