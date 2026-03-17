#include "ai_merge_resolver.hpp"


AIMergeResolver::AIMergeResolver(void* parent)
    : void(parent)
{
    logStructured("INFO", "AIMergeResolver initializing", void*{{"component", "AIMergeResolver"}});
    logStructured("INFO", "AIMergeResolver initialized successfully", void*{{"component", "AIMergeResolver"}});
}

AIMergeResolver::~AIMergeResolver()
{
    logStructured("INFO", "AIMergeResolver shutting down", void*{{"component", "AIMergeResolver"}});
}

void AIMergeResolver::setConfig(const Config& config)
{
    std::lock_guard<std::mutex> locker(&m_configMutex);
    m_config = config;
    logStructured("INFO", "Configuration updated", void*{
        {"enableAutoResolve", config.enableAutoResolve},
        {"minConfidenceThreshold", config.minConfidenceThreshold},
        {"maxConflictSize", config.maxConflictSize}
    });
}

AIMergeResolver::Config AIMergeResolver::getConfig() const
{
    std::lock_guard<std::mutex> locker(&m_configMutex);
    return m_config;
}

std::vector<AIMergeResolver::ConflictBlock> AIMergeResolver::detectConflicts(const std::string& filePath)
{
    auto startTime = std::chrono::steady_clock::now();
    std::vector<ConflictBlock> conflicts;
    
    try {
        std::fstream file(filePath);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            logStructured("ERROR", "Failed to open file for conflict detection", void*{
                {"filePath", filePath},
                {"error", file.errorString()}
            });
            std::lock_guard<std::mutex> metricsLocker(&m_metricsMutex);
            m_metrics.errorCount++;
            errorOccurred(std::string("Failed to open file: %1")));
            return conflicts;
        }
        
        QTextStream in(&file);
        std::string content = in.readAll();
        file.close();
        
        // Detect conflict markers
        std::regex conflictStart("^<<<<<<< (.+)$", std::regex::MultilineOption);
        std::regex conflictMid("^=======\\s*$", std::regex::MultilineOption);
        std::regex conflictEnd("^>>>>>>> (.+)$", std::regex::MultilineOption);
        
        std::vector<std::string> lines = content.split('\n');
        int i = 0;
        
        while (i < lines.size()) {
            std::smatch startMatch = conflictStart.match(lines[i]);
            if (startMatch.hasMatch()) {
                ConflictBlock conflict;
                conflict.file = filePath;
                conflict.startLine = i + 1;
                
                std::string currentBranch = startMatch"";
                std::vector<std::string> currentLines;
                i++;
                
                // Collect current version lines
                while (i < lines.size() && !conflictMid.match(lines[i]).hasMatch()) {
                    currentLines.append(lines[i]);
                    i++;
                }
                
                conflict.currentVersion = currentLines.join("\n");
                
                if (i < lines.size()) {
                    i++; // Skip =======
                }
                
                // Collect incoming version lines
                std::vector<std::string> incomingLines;
                while (i < lines.size() && !conflictEnd.match(lines[i]).hasMatch()) {
                    incomingLines.append(lines[i]);
                    i++;
                }
                
                conflict.incomingVersion = incomingLines.join("\n");
                
                if (i < lines.size()) {
                    std::smatch endMatch = conflictEnd.match(lines[i]);
                    if (endMatch.hasMatch()) {
                        std::string incomingBranch = endMatch"";
                        conflict.endLine = i + 1;
                        
                        // Extract context (5 lines before and after)
                        int contextStart = qMax(0, conflict.startLine - 6);
                        int contextEnd = qMin(lines.size(), conflict.endLine + 5);
                        conflict.context = lines.mid(contextStart, contextEnd - contextStart).join("\n");
                        
                        conflicts.append(conflict);
                    }
                }
            }
            i++;
        }
        
        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
        
        {
            std::lock_guard<std::mutex> metricsLocker(&m_metricsMutex);
            m_metrics.conflictsDetected += conflicts.size();
        }
        
        logStructured("INFO", "Conflicts detected", void*{
            {"filePath", filePath},
            {"conflictCount", conflicts.size()},
            {"latencyMs", duration.count()}
        });
        
        conflictsDetected(conflicts.size());
        
    } catch (const std::exception& e) {
        std::lock_guard<std::mutex> metricsLocker(&m_metricsMutex);
        m_metrics.errorCount++;
        logStructured("ERROR", "Exception during conflict detection", void*{{"error", e.what()}});
        errorOccurred(std::string("Conflict detection failed: %1")));
    }
    
    return conflicts;
}

AIMergeResolver::Resolution AIMergeResolver::resolveConflict(const ConflictBlock& conflict)
{
    auto startTime = std::chrono::steady_clock::now();
    Resolution resolution;
    
    try {
        Config config;
        {
            std::lock_guard<std::mutex> configLocker(&m_configMutex);
            config = m_config;
        }
        
        // Validate conflict size
        int conflictSize = conflict.currentVersion.length() + conflict.incomingVersion.length();
        if (conflictSize > config.maxConflictSize) {
            logStructured("WARN", "Conflict exceeds max size, requires manual resolution", void*{
                {"conflictSize", conflictSize},
                {"maxSize", config.maxConflictSize}
            });
            resolution.requiresManualReview = true;
            resolution.explanation = "Conflict too large for automated resolution";
            return resolution;
        }
        
        // Prepare AI request
        void* payload;
        payload["current_version"] = conflict.currentVersion;
        payload["incoming_version"] = conflict.incomingVersion;
        payload["base_version"] = conflict.baseVersion;
        payload["context"] = conflict.context;
        payload["file"] = conflict.file;
        
        logStructured("DEBUG", "Sending conflict resolution request to AI", void*{
            {"file", conflict.file},
            {"conflictSize", conflictSize}
        });
        
        void* response = makeAiRequest(config.aiEndpoint + "/resolve-conflict", payload);
        
        resolution.resolvedContent = response["resolved_content"].toString();
        resolution.confidence = response["confidence"].toDouble();
        resolution.strategy = response["strategy"].toString();
        resolution.explanation = response["explanation"].toString();
        resolution.requiresManualReview = resolution.confidence < config.minConfidenceThreshold;
        
        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
        recordLatency("conflict_resolution", duration);
        
        {
            std::lock_guard<std::mutex> metricsLocker(&m_metricsMutex);
            m_metrics.conflictsResolved++;
            if (!resolution.requiresManualReview) {
                m_metrics.autoResolved++;
            } else {
                m_metrics.manualResolved++;
            }
            
            // Update average confidence
            m_metrics.avgResolutionConfidence = 
                (m_metrics.avgResolutionConfidence * (m_metrics.conflictsResolved - 1) + resolution.confidence) 
                / m_metrics.conflictsResolved;
        }
        
        logStructured("INFO", "Conflict resolved", void*{
            {"file", conflict.file},
            {"confidence", resolution.confidence},
            {"strategy", resolution.strategy},
            {"requiresManualReview", resolution.requiresManualReview},
            {"latencyMs", duration.count()}
        });
        
        // Audit log
        if (config.enableAuditLog) {
            logAudit("conflict_resolved", void*{
                {"file", conflict.file},
                {"startLine", conflict.startLine},
                {"endLine", conflict.endLine},
                {"resolution", resolution.resolvedContent},
                {"confidence", resolution.confidence},
                {"strategy", resolution.strategy}
            });
        }
        
        conflictResolved(resolution);
        
    } catch (const std::exception& e) {
        std::lock_guard<std::mutex> metricsLocker(&m_metricsMutex);
        m_metrics.errorCount++;
        logStructured("ERROR", "Conflict resolution failed", void*{{"error", e.what()}});
        errorOccurred(std::string("Resolution failed: %1")));
        resolution.requiresManualReview = true;
        resolution.explanation = std::string("AI resolution failed: %1"));
    }
    
    return resolution;
}

bool AIMergeResolver::applyResolution(const std::string& filePath, const Resolution& resolution, int lineStart, int lineEnd)
{
    auto startTime = std::chrono::steady_clock::now();
    
    try {
        std::fstream file(filePath);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            logStructured("ERROR", "Failed to open file for applying resolution", void*{
                {"filePath", filePath},
                {"error", file.errorString()}
            });
            std::lock_guard<std::mutex> metricsLocker(&m_metricsMutex);
            m_metrics.errorCount++;
            errorOccurred(std::string("Failed to open file: %1")));
            return false;
        }
        
        QTextStream in(&file);
        std::vector<std::string> lines;
        while (!in.atEnd()) {
            lines.append(in.readLine());
        }
        file.close();
        
        // Replace conflict block with resolution
        if (lineStart < 1 || lineEnd > lines.size() || lineStart > lineEnd) {
            logStructured("ERROR", "Invalid line range for resolution", void*{
                {"lineStart", lineStart},
                {"lineEnd", lineEnd},
                {"fileLines", lines.size()}
            });
            std::lock_guard<std::mutex> metricsLocker(&m_metricsMutex);
            m_metrics.errorCount++;
            return false;
        }
        
        // Remove conflict lines
        lines.erase(lines.begin() + lineStart - 1, lines.begin() + lineEnd);
        
        // Insert resolved content
        std::vector<std::string> resolvedLines = resolution.resolvedContent.split('\n');
        for (int i = resolvedLines.size() - 1; i >= 0; i--) {
            lines.insert(lineStart - 1, resolvedLines[i]);
        }
        
        // Write back to file
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
            logStructured("ERROR", "Failed to open file for writing resolution", void*{
                {"filePath", filePath},
                {"error", file.errorString()}
            });
            std::lock_guard<std::mutex> metricsLocker(&m_metricsMutex);
            m_metrics.errorCount++;
            errorOccurred(std::string("Failed to write file: %1")));
            return false;
        }
        
        QTextStream out(&file);
        for (const std::string& line : lines) {
            out << line << "\n";
        }
        file.close();
        
        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
        
        logStructured("INFO", "Resolution applied successfully", void*{
            {"filePath", filePath},
            {"lineStart", lineStart},
            {"lineEnd", lineEnd},
            {"latencyMs", duration.count()}
        });
        
        Config config;
        {
            std::lock_guard<std::mutex> configLocker(&m_configMutex);
            config = m_config;
        }
        
        if (config.enableAuditLog) {
            logAudit("resolution_applied", void*{
                {"file", filePath},
                {"lineStart", lineStart},
                {"lineEnd", lineEnd},
                {"resolution", resolution.resolvedContent}
            });
        }
        
        return true;
        
    } catch (const std::exception& e) {
        std::lock_guard<std::mutex> metricsLocker(&m_metricsMutex);
        m_metrics.errorCount++;
        logStructured("ERROR", "Exception applying resolution", void*{{"error", e.what()}});
        errorOccurred(std::string("Apply resolution failed: %1")));
        return false;
    }
}

void* AIMergeResolver::analyzeSemanticMerge(const std::string& base, const std::string& current, const std::string& incoming)
{
    auto startTime = std::chrono::steady_clock::now();
    void* analysis;
    
    try {
        Config config;
        {
            std::lock_guard<std::mutex> configLocker(&m_configMutex);
            config = m_config;
        }
        
        void* payload;
        payload["base"] = base;
        payload["current"] = current;
        payload["incoming"] = incoming;
        payload["analysis_type"] = "semantic";
        
        logStructured("DEBUG", "Requesting semantic merge analysis", void*{
            {"baseSize", base.length()},
            {"currentSize", current.length()},
            {"incomingSize", incoming.length()}
        });
        
        analysis = makeAiRequest(config.aiEndpoint + "/analyze-semantic-merge", payload);
        
        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
        
        logStructured("INFO", "Semantic analysis completed", void*{
            {"hasConflicts", analysis["has_conflicts"].toBool()},
            {"semanticChanges", analysis["semantic_changes"].toArray().size()},
            {"latencyMs", duration.count()}
        });
        
    } catch (const std::exception& e) {
        std::lock_guard<std::mutex> metricsLocker(&m_metricsMutex);
        m_metrics.errorCount++;
        logStructured("ERROR", "Semantic analysis failed", void*{{"error", e.what()}});
        errorOccurred(std::string("Semantic analysis failed: %1")));
    }
    
    return analysis;
}

std::vector<std::string> AIMergeResolver::detectBreakingChanges(const std::string& diff)
{
    auto startTime = std::chrono::steady_clock::now();
    std::vector<std::string> breakingChanges;
    
    try {
        Config config;
        {
            std::lock_guard<std::mutex> configLocker(&m_configMutex);
            config = m_config;
        }
        
        void* payload;
        payload["diff"] = diff;
        payload["detection_mode"] = "breaking_changes";
        
        logStructured("DEBUG", "Detecting breaking changes", void*{{"diffSize", diff.length()}});
        
        void* response = makeAiRequest(config.aiEndpoint + "/detect-breaking-changes", payload);
        
        void* changesArray = response["breaking_changes"].toArray();
        for (const void*& value : changesArray) {
            breakingChanges.append(value.toString());
        }
        
        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
        
        {
            std::lock_guard<std::mutex> metricsLocker(&m_metricsMutex);
            m_metrics.breakingChangesDetected += breakingChanges.size();
        }
        
        logStructured("INFO", "Breaking changes detected", void*{
            {"count", breakingChanges.size()},
            {"latencyMs", duration.count()}
        });
        
        for (const std::string& change : breakingChanges) {
            breakingChangeDetected(change);
        }
        
    } catch (const std::exception& e) {
        std::lock_guard<std::mutex> metricsLocker(&m_metricsMutex);
        m_metrics.errorCount++;
        logStructured("ERROR", "Breaking change detection failed", void*{{"error", e.what()}});
        errorOccurred(std::string("Breaking change detection failed: %1")));
    }
    
    return breakingChanges;
}

AIMergeResolver::Metrics AIMergeResolver::getMetrics() const
{
    std::lock_guard<std::mutex> locker(&m_metricsMutex);
    return m_metrics;
}

void AIMergeResolver::resetMetrics()
{
    std::lock_guard<std::mutex> locker(&m_metricsMutex);
    m_metrics = Metrics();
    logStructured("INFO", "Metrics reset", void*{});
}

void AIMergeResolver::logStructured(const std::string& level, const std::string& message, const void*& context)
{
    void* logEntry;
    logEntry["timestamp"] = std::chrono::system_clock::time_point::currentDateTime().toString(//ISODate);
    logEntry["level"] = level;
    logEntry["component"] = "AIMergeResolver";
    logEntry["message"] = message;
    logEntry["context"] = context;
    
    void* doc(logEntry);
}

void AIMergeResolver::recordLatency(const std::string& operation, const std::chrono::milliseconds& duration)
{
    std::lock_guard<std::mutex> locker(&m_metricsMutex);
    
    if (operation == "conflict_resolution") {
        m_metrics.avgResolutionLatencyMs = 
            (m_metrics.avgResolutionLatencyMs * (m_metrics.conflictsResolved - 1) + duration.count()) 
            / m_metrics.conflictsResolved;
    }
    
    Config config;
    {
        std::lock_guard<std::mutex> configLocker(&m_configMutex);
        config = m_config;
    }
    
    if (config.enableMetrics) {
        metricsUpdated(m_metrics);
    }
}

void AIMergeResolver::logAudit(const std::string& action, const void*& details)
{
    Config config;
    {
        std::lock_guard<std::mutex> configLocker(&m_configMutex);
        config = m_config;
    }
    
    if (!config.enableAuditLog || config.auditLogPath.empty()) {
        return;
    }
    
    void* auditEntry;
    auditEntry["timestamp"] = std::chrono::system_clock::time_point::currentDateTime().toString(//ISODate);
    auditEntry["action"] = action;
    auditEntry["details"] = details;
    
    std::fstream auditFile(config.auditLogPath);
    if (auditFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        QTextStream out(&auditFile);
        void* doc(auditEntry);
        out << doc.toJson(void*::Compact) << "\n";
        auditFile.close();
    } else {
        logStructured("ERROR", "Failed to write audit log", void*{
            {"path", config.auditLogPath},
            {"error", auditFile.errorString()}
        });
    }
}

void* AIMergeResolver::makeAiRequest(const std::string& endpoint, const void*& payload)
{
    void* manager;
    void* request(endpoint);
    
    Config config;
    {
        std::lock_guard<std::mutex> configLocker(&m_configMutex);
        config = m_config;
    }
    
    request.setHeader(void*::ContentTypeHeader, "application/json");
    if (!config.apiKey.empty()) {
        request.setRawHeader("Authorization", std::string("Bearer %1").toUtf8());
    }
    
    void* doc(payload);
    std::vector<uint8_t> data = doc.toJson(void*::Compact);
    
    void* loop;
    void** reply = manager.post(request, data);
// Qt connect removed
    loop.exec();
    
    void* response;
    if (reply->error() == void*::NoError) {
        std::vector<uint8_t> responseData = reply->readAll();
        void* responseDoc = void*::fromJson(responseData);
        response = responseDoc.object();
    } else {
        logStructured("ERROR", "AI API request failed", void*{
            {"endpoint", endpoint},
            {"error", reply->errorString()}
        });
        throw std::runtime_error(reply->errorString().toStdString());
    }
    
    reply->deleteLater();
    return response;
}

bool AIMergeResolver::validateResolution(const Resolution& resolution, const ConflictBlock& conflict)
{
    if (resolution.resolvedContent.empty()) {
        logStructured("WARN", "Empty resolution content", void*{});
        return false;
    }
    
    Config config;
    {
        std::lock_guard<std::mutex> configLocker(&m_configMutex);
        config = m_config;
    }
    
    if (resolution.confidence < config.minConfidenceThreshold) {
        logStructured("WARN", "Resolution confidence below threshold", void*{
            {"confidence", resolution.confidence},
            {"threshold", config.minConfidenceThreshold}
        });
        return false;
    }
    
    return true;
}


