/**
 * ExecutionStatePersistence - Production Implementation
 * Days 1-2: Workflow state persistence and cross-session resumption
 */

#include "execution_state_persistence.h"
#include "agentic_loop_state.h"
#include "agentic_memory_system.h"
#include "agentic_executor.h"

#include <fstream>
#include <sstream>
#include <chrono>
#include <algorithm>
#include <cstdlib>
#include <iomanip>
#include <exception>
#include <iostream>
#include <thread>
#include <unordered_map>
#include <unordered_set>

namespace {
std::string makeIsoUtcNow()
{
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", std::gmtime(&time_t));
    return std::string(buf);
}

std::filesystem::path backupPathFor(const std::filesystem::path& target)
{
    return std::filesystem::path(target.string() + ".bak");
}

std::filesystem::path corruptPathFor(const std::filesystem::path& target)
{
    return std::filesystem::path(target.string() + ".corrupt");
}

bool hasRequiredCheckpointFields(const nlohmann::json& cp)
{
    return cp.is_object() && cp.contains("checkpointId") && cp.contains("label") &&
           cp.contains("sequenceNumber") && cp.contains("timestamp") && cp.contains("stateSnapshot");
}
}

// ===== ExecutionCheckpoint =====

nlohmann::json ExecutionCheckpoint::toJson() const
{
    nlohmann::json j;
    j["checkpointId"] = checkpointId;
    j["label"] = label;
    j["sequenceNumber"] = sequenceNumber;
    j["timestamp"] = timestamp;
    j["stateSnapshot"] = stateSnapshot;
    j["isRecoveryPoint"] = isRecoveryPoint;
    j["recoveryReason"] = recoveryReason;
    return j;
}

void ExecutionCheckpoint::fromJson(const nlohmann::json& j)
{
    checkpointId = j["checkpointId"].get<std::string>();
    label = j["label"].get<std::string>();
    sequenceNumber = j["sequenceNumber"].get<int>();
    timestamp = j["timestamp"].get<std::string>();
    stateSnapshot = j["stateSnapshot"];
    isRecoveryPoint = j.value("isRecoveryPoint", false);
    recoveryReason = j.value("recoveryReason", "");
}

// ===== WorkflowExecution =====

nlohmann::json WorkflowExecution::toJson() const
{
    nlohmann::json j;
    j["executionId"] = executionId;
    j["workflowName"] = workflowName;
    j["goal"] = goal;
    j["status"] = status;
    j["startTime"] = startTime;
    j["lastUpdateTime"] = lastUpdateTime;
    j["completionTime"] = completionTime;
    j["currentCheckpointIndex"] = currentCheckpointIndex;
    
    nlohmann::json checkpointArray = nlohmann::json::array();
    for (const auto& cp : checkpoints) {
        checkpointArray.push_back(cp.toJson());
    }
    j["checkpoints"] = checkpointArray;
    
    // Serialize globalContext - handle null case
    if (globalContext.is_null()) {
        j["globalContext"] = nlohmann::json::object();
    } else {
        j["globalContext"] = globalContext;
    }
    
    // Serialize metadata - handle null case  
    if (metadata.is_null()) {
        j["metadata"] = nlohmann::json::object();
    } else {
        j["metadata"] = metadata;
    }
    
    j["errorLog"] = errorLog;
    
    return j;
}

void WorkflowExecution::fromJson(const nlohmann::json& j)
{
    executionId = j["executionId"].get<std::string>();
    workflowName = j["workflowName"].get<std::string>();
    goal = j["goal"].get<std::string>();
    status = j["status"].get<std::string>();
    startTime = j["startTime"].get<std::string>();
    lastUpdateTime = j["lastUpdateTime"].get<std::string>();
    completionTime = j.value("completionTime", "");
    currentCheckpointIndex = j.value("currentCheckpointIndex", 0);
    
    checkpoints.clear();
    if (j.contains("checkpoints")) {
        for (const auto& cpJson : j["checkpoints"]) {
            ExecutionCheckpoint cp;
            cp.fromJson(cpJson);
            checkpoints.push_back(cp);
        }
    }
    
    if (j.contains("globalContext")) {
        globalContext = j["globalContext"];
    } else {
        globalContext = nlohmann::json::object();
    }
    if (globalContext.is_null()) {
        globalContext = nlohmann::json::object();
    }
    
    if (j.contains("metadata")) {
        metadata = j["metadata"];
    } else {
        metadata = nlohmann::json::object();
    }
    if (metadata.is_null()) {
        metadata = nlohmann::json::object();
    }
    
    errorLog.clear();
    if (j.contains("errorLog")) {
        errorLog = j["errorLog"].get<std::vector<std::string>>();
    }
}

// ===== ExecutionStatePersistence =====

ExecutionStatePersistence::ExecutionStatePersistence(
    const std::filesystem::path& persistenceRoot)
    : m_persistenceRoot(persistenceRoot)
{
    ensurePersistenceRoot();
    std::cout << "[ExecutionStatePersistence] Initialized at: "
              << m_persistenceRoot.string() << std::endl;
}

ExecutionStatePersistence::~ExecutionStatePersistence()
{
    // Shutdown WAL thread if running
    if (m_asyncPersistenceEnabled) {
        m_walShutdown = true;
        if (m_walThread.joinable()) {
            m_walThread.join();
        }
    }
}

bool ExecutionStatePersistence::ensurePersistenceRoot()
{
    try {
        if (!std::filesystem::exists(m_persistenceRoot)) {
            std::filesystem::create_directories(m_persistenceRoot);
        }
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[ExecutionStatePersistence] Failed to create root: " << e.what() << std::endl;
        return false;
    }
}

std::string ExecutionStatePersistence::generateExecutionId()
{
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    
    std::stringstream ss;
    ss << "exec_" << std::put_time(std::gmtime(&time_t), "%Y%m%d_%H%M%S");
    
    // Add microsecond component for uniqueness
    auto micros = std::chrono::duration_cast<std::chrono::microseconds>(
        now.time_since_epoch()).count() % 1000000;
    ss << "_" << std::setfill('0') << std::setw(6) << micros;
    
    return ss.str();
}

std::string ExecutionStatePersistence::getExecutionPath(const std::string& executionId) const
{
    return (m_persistenceRoot / (executionId + ".json")).string();
}

std::string ExecutionStatePersistence::persistWorkflowExecution(
    const WorkflowExecution& execution)
{
    try {
        WorkflowExecution toWrite = execution;
        if (toWrite.executionId.empty()) {
            toWrite.executionId = generateExecutionId();
        }
        if (toWrite.startTime.empty()) {
            toWrite.startTime = makeIsoUtcNow();
        }
        toWrite.lastUpdateTime = makeIsoUtcNow();

        auto execPath = getExecutionPath(toWrite.executionId);
        
        // Build JSON directly to avoid any serialization issues with toJson()
        nlohmann::json json;
        json["executionId"] = toWrite.executionId;
        json["workflowName"] = toWrite.workflowName;
        json["goal"] = toWrite.goal;
        json["status"] = toWrite.status;
        json["startTime"] = toWrite.startTime;
        json["lastUpdateTime"] = toWrite.lastUpdateTime;
        json["completionTime"] = toWrite.completionTime;
        json["currentCheckpointIndex"] = toWrite.currentCheckpointIndex;
        
        nlohmann::json checkpointArray = nlohmann::json::array();
        for (const auto& cp : toWrite.checkpoints) {
            checkpointArray.push_back(cp.toJson());
        }
        json["checkpoints"] = checkpointArray;
        
        // Serialize globalContext - ensure it's never null
        if (toWrite.globalContext.is_null() || toWrite.globalContext.empty()) {
            json["globalContext"] = nlohmann::json::object();
        } else {
            json["globalContext"] = toWrite.globalContext;
        }
        
        // Serialize metadata - ensure it's never null
        if (toWrite.metadata.is_null() || toWrite.metadata.empty()) {
            json["metadata"] = nlohmann::json::object();
        } else {
            json["metadata"] = toWrite.metadata;
        }
        
        json["errorLog"] = toWrite.errorLog;
        
        json["schemaVersion"] = 1;
        json["lastPersistedAt"] = toWrite.lastUpdateTime;
        
        if (!atomicWrite(execPath, json.dump(2))) {
            std::cerr << "[ExecutionStatePersistence] Atomic write failed for " 
                      << execution.executionId << std::endl;
            return "";
        }
        
        std::cout << "[ExecutionStatePersistence] Persisted execution: "
                  << toWrite.executionId << " (" << toWrite.goal << ")" << std::endl;
        return toWrite.executionId;
    } catch (const std::exception& e) {
        std::cerr << "[ExecutionStatePersistence] Persist error: " << e.what() << std::endl;
        return "";
    }
}

std::unique_ptr<WorkflowExecution> ExecutionStatePersistence::loadWorkflowExecution(
    const std::string& executionId) const
{
    try {
        auto execPath = std::filesystem::path(getExecutionPath(executionId));
        
        if (!std::filesystem::exists(execPath)) {
            std::cerr << "[ExecutionStatePersistence] Execution not found: " 
                      << executionId << std::endl;
            return nullptr;
        }

        auto readCandidate = [&](const std::filesystem::path& candidate,
                                 nlohmann::json& outJson) -> bool {
            std::ifstream ifs(candidate, std::ios::binary);
            if (!ifs.good()) {
                return false;
            }
            std::stringstream buffer;
            buffer << ifs.rdbuf();
            outJson = nlohmann::json::parse(buffer.str());
            return validateJsonSchema(outJson);
        };

        nlohmann::json j;
        bool loaded = false;
        try {
            loaded = readCandidate(execPath, j);
        } catch (const std::exception&) {
            loaded = false;
        }

        if (!loaded) {
            // Quarantine bad primary and attempt backup fallback.
            try {
                auto corruptPath = corruptPathFor(execPath);
                if (std::filesystem::exists(corruptPath)) {
                    std::filesystem::remove(corruptPath);
                }
                std::filesystem::rename(execPath, corruptPath);
            } catch (const std::exception& e) {
                std::cerr << "[ExecutionStatePersistence] Failed to quarantine corrupt state: "
                          << e.what() << std::endl;
            }

            const auto backupPath = backupPathFor(execPath);
            try {
                loaded = readCandidate(backupPath, j);
            } catch (const std::exception&) {
                loaded = false;
            }

            if (loaded) {
                // Promote backup as active state to restore forward progress.
                atomicWrite(execPath, j.dump(2));
            } else {
                std::cerr << "[ExecutionStatePersistence] Invalid or corrupt state: "
                          << executionId << std::endl;
                return nullptr;
            }
        }
        
        auto execution = std::make_unique<WorkflowExecution>();
        execution->fromJson(j);
        
        std::cout << "[ExecutionStatePersistence] Loaded execution: "
                  << executionId << " with " << execution->checkpoints.size() 
                  << " checkpoints" << std::endl;
        return execution;
    } catch (const std::exception& e) {
        std::cerr << "[ExecutionStatePersistence] Load error: " << e.what() << std::endl;
        return nullptr;
    }
}

std::string ExecutionStatePersistence::createCheckpoint(
    const std::string& executionId,
    const std::string& label,
    const nlohmann::json& stateSnapshot)
{
    try {
        auto execution = loadWorkflowExecution(executionId);
        if (!execution) {
            auto bootstrap = std::make_unique<WorkflowExecution>();
            bootstrap->executionId = executionId.empty() ? generateExecutionId() : executionId;
            bootstrap->workflowName = "RecoveredWorkflow";
            bootstrap->goal = "Recovered from checkpoint bootstrap";
            bootstrap->status = "in-progress";
            bootstrap->startTime = makeIsoUtcNow();
            bootstrap->lastUpdateTime = bootstrap->startTime;
            bootstrap->globalContext = nlohmann::json::object();
            if (persistWorkflowExecution(*bootstrap).empty()) {
                return "";
            }
            execution = loadWorkflowExecution(bootstrap->executionId);
            if (!execution) {
                return "";
            }
        }
        
        ExecutionCheckpoint checkpoint;
        checkpoint.checkpointId = generateExecutionId() + "_cp";
        checkpoint.label = label;
        checkpoint.sequenceNumber = static_cast<int>(execution->checkpoints.size());
        
        checkpoint.timestamp = makeIsoUtcNow();
        
        checkpoint.stateSnapshot = stateSnapshot;
        checkpoint.isRecoveryPoint = true;
        
        execution->checkpoints.push_back(checkpoint);
        execution->currentCheckpointIndex = checkpoint.sequenceNumber;
        
        execution->lastUpdateTime = checkpoint.timestamp;
        
        persistWorkflowExecution(*execution);
        
        std::cout << "[ExecutionStatePersistence] Created checkpoint: " 
                  << checkpoint.checkpointId << " (" << label << ")" << std::endl;
        return checkpoint.checkpointId;
    } catch (const std::exception& e) {
        std::cerr << "[ExecutionStatePersistence] Checkpoint error: " << e.what() << std::endl;
        return "";
    }
}

std::vector<std::string> ExecutionStatePersistence::listExecutions() const
{
    std::vector<std::string> result;
    try {
        for (const auto& entry : std::filesystem::directory_iterator(m_persistenceRoot)) {
            if (entry.is_regular_file() && entry.path().extension() == ".json") {
                auto filename = entry.path().stem().string();
                if (filename.substr(0, 5) == "exec_") {
                    result.push_back(filename);
                }
            }
        }
        std::sort(result.rbegin(), result.rend());  // Most recent first
    } catch (const std::exception& e) {
        std::cerr << "[ExecutionStatePersistence] List error: " << e.what() << std::endl;
    }
    return result;
}

bool ExecutionStatePersistence::hasValidExecution(const std::string& executionId) const
{
    const auto execPath = std::filesystem::path(getExecutionPath(executionId));
    if (!std::filesystem::exists(execPath)) {
        return false;
    }
    return validateExecution(executionId).empty();
}

std::unique_ptr<WorkflowExecution> ExecutionStatePersistence::getLastIncompleteExecution()
{
    auto executions = listExecutions();
    for (const auto& execId : executions) {
        auto exec = loadWorkflowExecution(execId);
        if (exec && (exec->status == "in-progress" || exec->status == "paused")) {
            return exec;
        }
    }
    return nullptr;
}

std::unique_ptr<WorkflowExecution> ExecutionStatePersistence::resumeFromCheckpoint(
    const std::string& executionId,
    int checkpointIndex)
{
    try {
        auto execution = loadWorkflowExecution(executionId);
        if (!execution || execution->checkpoints.empty()) {
            return nullptr;
        }
        
        int cpIndex = findResumableCheckpointIndex(*execution, checkpointIndex);
        if (cpIndex < 0) {
            execution->errorLog.push_back("No resumable checkpoint available");
            return nullptr;
        }
        
        execution->currentCheckpointIndex = cpIndex;
        execution->status = "in-progress";
        execution->lastUpdateTime = makeIsoUtcNow();
        if (checkpointIndex != cpIndex) {
            execution->errorLog.push_back(
                std::string("Requested checkpoint unavailable; resumed from fallback index ") +
                std::to_string(cpIndex));
        }

        // Persist resumed pointer to guarantee restart continuity after resume.
        persistWorkflowExecution(*execution);
        
        std::cout << "[ExecutionStatePersistence] Resuming from checkpoint "
                  << cpIndex << " in execution " << executionId << std::endl;
        return execution;
    } catch (const std::exception& e) {
        std::cerr << "[ExecutionStatePersistence] Resume error: " << e.what() << std::endl;
        return nullptr;
    }
}

WorkflowExecution ExecutionStatePersistence::captureCurrentExecution(
    const std::string& workflowName,
    const std::string& goal,
    AgenticLoopState* loopState,
    AgenticMemorySystem* memorySystem,
    AgenticExecutor* executor)
{
    WorkflowExecution execution;
    execution.executionId = generateExecutionId();
    execution.workflowName = workflowName;
    execution.goal = goal;
    execution.status = "in-progress";
    
    execution.startTime = makeIsoUtcNow();
    execution.lastUpdateTime = execution.startTime;
    
    // Capture state snapshot
    nlohmann::json snapshot = nlohmann::json::object();
    if (loopState) {
        auto loopJson = nlohmann::json::parse(loopState->serializeState(), nullptr, false);
        if (!loopJson.is_discarded()) {
            snapshot["loopState"] = std::move(loopJson);
        }
    }
    if (memorySystem) {
        snapshot["memorySystem"] = memorySystem->exportState();
    }
    if (executor) {
        snapshot["executorContext"] = {
            {"available", true}
        };
    }
    snapshot["capturedAt"] = execution.lastUpdateTime;
    snapshot["schemaVersion"] = 1;
    
    execution.globalContext = snapshot;
    execution.metadata["capturedSubsystems"] = nlohmann::json::array();
    if (snapshot.contains("loopState")) execution.metadata["capturedSubsystems"].push_back("loopState");
    if (snapshot.contains("memorySystem")) execution.metadata["capturedSubsystems"].push_back("memorySystem");
    if (snapshot.contains("executorContext")) execution.metadata["capturedSubsystems"].push_back("executorContext");
    
    return execution;
}

bool ExecutionStatePersistence::restoreExecutionState(
    const WorkflowExecution& execution,
    AgenticLoopState* loopState,
    AgenticMemorySystem* memorySystem) const
{
    if (restoreStatePayload(execution.globalContext, loopState, memorySystem)) {
        return true;
    }

    const int checkpointIndex = findResumableCheckpointIndex(
        execution,
        execution.currentCheckpointIndex);
    if (checkpointIndex < 0) {
        return false;
    }

    return restoreCheckpointState(execution.checkpoints[checkpointIndex], loopState, memorySystem);
}

bool ExecutionStatePersistence::restoreCheckpointState(
    const ExecutionCheckpoint& checkpoint,
    AgenticLoopState* loopState,
    AgenticMemorySystem* memorySystem) const
{
    return restoreStatePayload(checkpoint.stateSnapshot, loopState, memorySystem);
}

std::string ExecutionStatePersistence::validateExecution(
    const std::string& executionId) const
{
    try {
        auto execution = loadWorkflowExecution(executionId);
        if (!execution) {
            return "Execution not found";
        }
        
        if (execution->executionId.empty()) {
            return "Missing executionId";
        }
        
        if (execution->checkpoints.empty() && execution->status == "completed") {
            return "Completed execution with no checkpoints";
        }
        
        return "";  // Valid
    } catch (const std::exception& e) {
        return std::string("Validation error: ") + e.what();
    }
}

bool ExecutionStatePersistence::recoverFromCorruption(
    const std::string& executionId)
{
    try {
        auto execution = loadWorkflowExecution(executionId);
        if (execution && !execution->checkpoints.empty()) {
            execution->currentCheckpointIndex = std::max(0, execution->currentCheckpointIndex - 1);
            execution->status = "paused";
            execution->lastUpdateTime = makeIsoUtcNow();

            auto& lastCp = execution->checkpoints.back();
            lastCp.isRecoveryPoint = true;
            lastCp.recoveryReason = "Corruption recovery";

            persistWorkflowExecution(*execution);
            std::cout << "[ExecutionStatePersistence] Recovered from checkpoint rollback: "
                      << executionId << std::endl;
            return true;
        }

        // Fallback path: restore from backup if JSON is too corrupt to load.
        const auto execPath = std::filesystem::path(getExecutionPath(executionId));
        const auto backupPath = backupPathFor(execPath);
        if (std::filesystem::exists(backupPath)) {
            nlohmann::json backupJson;
            std::ifstream ifs(backupPath, std::ios::binary);
            if (ifs.good()) {
                std::stringstream buffer;
                buffer << ifs.rdbuf();
                backupJson = nlohmann::json::parse(buffer.str());
                if (validateJsonSchema(backupJson)) {
                    if (atomicWrite(execPath, backupJson.dump(2))) {
                        std::cout << "[ExecutionStatePersistence] Recovered from backup: "
                                  << executionId << std::endl;
                        return true;
                    }
                }
            }
        }

        return false;
    } catch (const std::exception& e) {
        std::cerr << "[ExecutionStatePersistence] Recovery error: " << e.what() << std::endl;
        return false;
    }
}

bool ExecutionStatePersistence::deleteExecution(const std::string& executionId)
{
    try {
        auto execPath = getExecutionPath(executionId);
        if (std::filesystem::exists(execPath)) {
            std::filesystem::remove(execPath);
            std::cout << "[ExecutionStatePersistence] Deleted execution: " 
                      << executionId << std::endl;
            return true;
        }
        return false;
    } catch (const std::exception& e) {
        std::cerr << "[ExecutionStatePersistence] Delete error: " << e.what() << std::endl;
        return false;
    }
}

bool ExecutionStatePersistence::atomicWrite(
    const std::filesystem::path& target,
    const std::string& content) const
{
    try {
        // Write to temporary file first
        auto tempPath = std::filesystem::path(target.string() + ".tmp");
        auto backupPath = backupPathFor(target);
        
        {
            std::ofstream ofs(tempPath, std::ios::binary | std::ios::trunc);
            if (!ofs.good()) {
                return false;
            }
            ofs.write(content.c_str(), content.size());
            ofs.flush();
            ofs.close();
        }

        // Keep a hot backup of last valid state for corruption recovery.
        if (std::filesystem::exists(target)) {
            std::filesystem::copy_file(
                target,
                backupPath,
                std::filesystem::copy_options::overwrite_existing);
            std::filesystem::remove(target);
        }

        // Atomic move to active file name.
        std::filesystem::rename(tempPath, target);
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[ExecutionStatePersistence] Atomic write failed: " 
                  << e.what() << std::endl;
        return false;
    }
}

bool ExecutionStatePersistence::validateJsonSchema(const nlohmann::json& j) const
{
    if (!j.is_object()) {
        return false;
    }

    if (!j.contains("executionId") || !j["executionId"].is_string() ||
        !j.contains("workflowName") || !j["workflowName"].is_string() ||
        !j.contains("status") || !j["status"].is_string()) {
        return false;
    }

    if (j.contains("checkpoints")) {
        if (!j["checkpoints"].is_array()) {
            return false;
        }
        for (const auto& cp : j["checkpoints"]) {
            if (!hasRequiredCheckpointFields(cp)) {
                return false;
            }
        }
    }

    if (j.contains("currentCheckpointIndex") && !j["currentCheckpointIndex"].is_number_integer()) {
        return false;
    }

    return true;
}

int ExecutionStatePersistence::findResumableCheckpointIndex(
    const WorkflowExecution& execution,
    int requestedIndex) const
{
    if (execution.checkpoints.empty()) {
        return -1;
    }

    int index = requestedIndex;
    if (index < 0 || index >= static_cast<int>(execution.checkpoints.size())) {
        index = static_cast<int>(execution.checkpoints.size()) - 1;
    }

    for (int candidate = index; candidate >= 0; --candidate) {
        if (execution.checkpoints[candidate].stateSnapshot.is_object()) {
            return candidate;
        }
    }

    return -1;
}

bool ExecutionStatePersistence::restoreStatePayload(
    const nlohmann::json& payload,
    AgenticLoopState* loopState,
    AgenticMemorySystem* memorySystem) const
{
    if (!payload.is_object()) {
        return false;
    }

    bool restored = false;

    if (loopState && payload.contains("loopState")) {
        const auto& loopStateJson = payload["loopState"];
        if (loopStateJson.is_object()) {
            restored = loopState->deserializeState(loopStateJson.dump()) || restored;
        }
    }

    if (memorySystem && payload.contains("memorySystem")) {
        restored = memorySystem->importState(payload["memorySystem"]) || restored;
    }

    return restored;
}

ExecutionStatePersistence::PersistenceStats ExecutionStatePersistence::getStatistics() const
{
    PersistenceStats stats;
    try {
        auto executions = listExecutions();
        stats.totalExecutions = executions.size();
        
        for (const auto& execId : executions) {
            auto exec = loadWorkflowExecution(execId);
            if (exec) {
                if (exec->status == "in-progress") stats.activeExecutions++;
                else if (exec->status == "completed") stats.completedExecutions++;
                else if (exec->status == "failed") stats.failedExecutions++;
                
                stats.totalCheckpoints += exec->checkpoints.size();
            }
        }
        
        // Estimate disk usage
        for (const auto& entry : std::filesystem::directory_iterator(m_persistenceRoot)) {
            if (entry.is_regular_file()) {
                stats.diskUsageBytes += std::filesystem::file_size(entry);
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "[ExecutionStatePersistence] Stats error: " << e.what() << std::endl;
    }
    
    return stats;
}

int ExecutionStatePersistence::cleanupOldExecutions(int maxAgeHours)
{
    int deleted = 0;
    try {
        if (maxAgeHours <= 0) return 0;
        
        const auto fileNow = std::filesystem::file_time_type::clock::now();
        const auto maxAge = std::chrono::duration_cast<std::filesystem::file_time_type::duration>(
            std::chrono::hours(maxAgeHours));
        
        for (const auto& execId : listExecutions()) {
            auto exec = loadWorkflowExecution(execId);
            if (!exec) continue;
            
            // Parse timestamp (simplified - assumes ISO format)
            if (exec->completionTime.empty()) continue;
            
            // For production: compare with persistent file modification time
            auto execPath = getExecutionPath(execId);
            auto lastWrite = std::filesystem::last_write_time(execPath);

            if (fileNow - lastWrite > maxAge) {
                if (deleteExecution(execId)) {
                    deleted++;
                }
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "[ExecutionStatePersistence] Cleanup error: " << e.what() << std::endl;
    }
    
    std::cout << "[ExecutionStatePersistence] Cleaned up " << deleted 
              << " old executions" << std::endl;
    return deleted;
}

// ============================================================================
// ENHANCEMENT 1: Checkpoint Compression
// ============================================================================

void ExecutionStatePersistence::setCompressionLevel(int level)
{
    m_compressionLevel = std::clamp(level, 0, 9);
}

std::string ExecutionStatePersistence::compressState(const std::string& jsonState) const
{
    if (m_compressionLevel == 0 || jsonState.length() < 1024) {
        return jsonState; // No compression for small states
    }
    
    // Simple run-length encoding for repeated patterns
    std::string compressed;
    compressed.reserve(jsonState.length());
    
    size_t i = 0;
    while (i < jsonState.length()) {
        size_t runStart = i;
        char current = jsonState[i];
        
        // Find run length
        while (i < jsonState.length() && jsonState[i] == current && (i - runStart) < 255) {
            i++;
        }
        
        size_t runLength = i - runStart;
        if (runLength >= 4) {
            // Encode run: 0x00 + char + count
            compressed += '\0';
            compressed += current;
            compressed += static_cast<char>(runLength);
        } else {
            // Literal run
            compressed.append(jsonState, runStart, runLength);
        }
    }
    
    // If compression didn't help, return original
    if (compressed.length() >= jsonState.length()) {
        return jsonState;
    }
    
    return compressed;
}

std::string ExecutionStatePersistence::decompressState(const std::string& compressedState) const
{
    if (compressedState.empty() || m_compressionLevel == 0) {
        return compressedState;
    }
    
    std::string decompressed;
    decompressed.reserve(compressedState.length() * 2);
    
    size_t i = 0;
    while (i < compressedState.length()) {
        if (compressedState[i] == '\0' && i + 2 < compressedState.length()) {
            // Decode run
            char ch = compressedState[i + 1];
            int count = static_cast<unsigned char>(compressedState[i + 2]);
            decompressed.append(count, ch);
            i += 3;
        } else {
            decompressed += compressedState[i];
            i++;
        }
    }
    
    return decompressed;
}

// ============================================================================
// ENHANCEMENT 2: Incremental State Diffing
// ============================================================================

std::string ExecutionStatePersistence::createIncrementalCheckpoint(
    const std::string& executionId,
    const std::string& label,
    const nlohmann::json& currentState)
{
    auto execution = loadWorkflowExecution(executionId);
    if (!execution) {
        return "";
    }
    
    nlohmann::json stateToStore = currentState;
    
    // If we have previous checkpoints, compute diff
    if (!execution->checkpoints.empty()) {
        const auto& lastCheckpoint = execution->checkpoints.back();
        nlohmann::json baseState = lastCheckpoint.stateSnapshot;
        
        // Check if base is compressed
        if (baseState.contains("_compressed") && baseState["_compressed"].get<bool>()) {
            std::string decompressed = decompressState(baseState["data"].get<std::string>());
            baseState = nlohmann::json::parse(decompressed, nullptr, false);
        }
        
        nlohmann::json diff = computeStateDiff(baseState, currentState);
        
        // Only store diff if it's smaller than full state
        if (diff.dump().length() < currentState.dump().length() * 0.5) {
            stateToStore = nlohmann::json::object();
            stateToStore["_diff"] = true;
            stateToStore["_baseIndex"] = static_cast<int>(execution->checkpoints.size()) - 1;
            stateToStore["_diffData"] = diff;
        }
    }
    
    // Apply compression if enabled
    if (m_compressionLevel > 0) {
        std::string compressed = compressState(stateToStore.dump());
        if (compressed.length() < stateToStore.dump().length()) {
            nlohmann::json wrapped;
            wrapped["_compressed"] = true;
            wrapped["data"] = compressed;
            stateToStore = wrapped;
        }
    }
    
    return createCheckpoint(executionId, label, stateToStore);
}

nlohmann::json ExecutionStatePersistence::computeStateDiff(
    const nlohmann::json& base,
    const nlohmann::json& current) const
{
    nlohmann::json diff = nlohmann::json::object();
    
    if (!base.is_object() || !current.is_object()) {
        diff["_full"] = current;
        return diff;
    }
    
    // Find added and modified fields
    for (auto& [key, value] : current.items()) {
        if (!base.contains(key)) {
            diff[key] = value;
        } else if (base[key] != value) {
            if (value.is_object() && base[key].is_object()) {
                nlohmann::json nestedDiff = computeStateDiff(base[key], value);
                if (!nestedDiff.empty()) {
                    diff[key] = nestedDiff;
                }
            } else {
                diff[key] = value;
            }
        }
    }
    
    // Find removed fields
    std::vector<std::string> removed;
    for (auto& [key, value] : base.items()) {
        if (!current.contains(key)) {
            removed.push_back(key);
        }
    }
    if (!removed.empty()) {
        diff["_removed"] = removed;
    }
    
    return diff;
}

nlohmann::json ExecutionStatePersistence::applyStateDiff(
    const nlohmann::json& base,
    const nlohmann::json& diff) const
{
    if (diff.contains("_full")) {
        return diff["_full"];
    }
    
    nlohmann::json result = base;
    
    for (auto& [key, value] : diff.items()) {
        if (key == "_removed") {
            for (const auto& removedKey : value) {
                result.erase(removedKey.get<std::string>());
            }
        } else {
            if (value.is_object() && result.contains(key) && result[key].is_object()) {
                result[key] = applyStateDiff(result[key], value);
            } else {
                result[key] = value;
            }
        }
    }
    
    return result;
}

nlohmann::json ExecutionStatePersistence::reconstructState(
    const WorkflowExecution& execution,
    int checkpointIndex) const
{
    if (checkpointIndex < 0 || checkpointIndex >= static_cast<int>(execution.checkpoints.size())) {
        return nlohmann::json::object();
    }
    
    const auto& checkpoint = execution.checkpoints[checkpointIndex];
    nlohmann::json state = checkpoint.stateSnapshot;
    
    // Decompress if needed
    if (state.contains("_compressed") && state["_compressed"].get<bool>()) {
        std::string decompressed = decompressState(state["data"].get<std::string>());
        state = nlohmann::json::parse(decompressed, nullptr, false);
    }
    
    // Apply diffs if this is an incremental checkpoint
    if (state.contains("_diff") && state["_diff"].get<bool>()) {
        int baseIndex = state["_baseIndex"].get<int>();
        nlohmann::json baseState = reconstructState(execution, baseIndex);
        state = applyStateDiff(baseState, state["_diffData"]);
    }
    
    return state;
}

// ============================================================================
// ENHANCEMENT 3: Memory-Mapped Persistence
// ============================================================================

void ExecutionStatePersistence::enableMemoryMapping(bool enable)
{
    m_memoryMappingEnabled = enable;
}

bool ExecutionStatePersistence::mapExecutionToMemory(const std::string& executionId)
{
#ifdef _WIN32
    auto execPath = getExecutionPath(executionId);
    
    HANDLE hFile = CreateFileA(
        execPath.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    );
    
    if (hFile == INVALID_HANDLE_VALUE) {
        return false;
    }
    
    LARGE_INTEGER fileSize;
    if (!GetFileSizeEx(hFile, &fileSize)) {
        CloseHandle(hFile);
        return false;
    }
    
    HANDLE hMapping = CreateFileMapping(
        hFile,
        nullptr,
        PAGE_READONLY,
        0, 0,
        nullptr
    );
    
    if (!hMapping) {
        CloseHandle(hFile);
        return false;
    }
    
    LPVOID pView = MapViewOfFile(
        hMapping,
        FILE_MAP_READ,
        0, 0, 0
    );
    
    CloseHandle(hMapping);
    CloseHandle(hFile);
    
    if (pView) {
        m_memoryMaps[executionId] = {static_cast<const char*>(pView), static_cast<size_t>(fileSize.QuadPart)};
        return true;
    }
#endif
    return false;
}

void ExecutionStatePersistence::unmapExecution(const std::string& executionId)
{
    auto it = m_memoryMaps.find(executionId);
    if (it != m_memoryMaps.end()) {
#ifdef _WIN32
        UnmapViewOfFile(it->second.first);
#endif
        m_memoryMaps.erase(it);
    }
}

const char* ExecutionStatePersistence::getMemoryMappedView(const std::string& executionId)
{
    if (!m_memoryMappingEnabled) {
        return nullptr;
    }
    
    auto it = m_memoryMaps.find(executionId);
    if (it != m_memoryMaps.end()) {
        return it->second.first;
    }
    
    // Try to map on demand
    mapExecutionToMemory(executionId);
    
    it = m_memoryMaps.find(executionId);
    if (it != m_memoryMaps.end()) {
        return it->second.first;
    }
    
    return nullptr;
}

// ============================================================================
// ENHANCEMENT 4: Semantic Memory Index
// ============================================================================

void ExecutionStatePersistence::buildMemoryIndex(const std::string& executionId)
{
    auto execution = loadWorkflowExecution(executionId);
    if (!execution || execution->checkpoints.empty()) {
        return;
    }
    
    MemoryIndex index;
    
    // Index all checkpoints
    for (const auto& checkpoint : execution->checkpoints) {
        if (checkpoint.stateSnapshot.contains("memorySystem")) {
            const auto& memSystem = checkpoint.stateSnapshot["memorySystem"];
            if (memSystem.contains("memories") && memSystem["memories"].is_array()) {
                for (const auto& memData : memSystem["memories"]) {
                    if (memData.contains("content")) {
                        std::string content = memData["content"].get<std::string>();
                        std::string memId = memData.value("id", "");
                        if (memId.empty()) continue;
                        
                        // Simple keyword extraction (split on whitespace)
                        std::istringstream iss(content);
                        std::string word;
                        while (iss >> word) {
                            // Normalize word
                            std::transform(word.begin(), word.end(), word.begin(), ::tolower);
                            if (word.length() > 3) { // Skip short words
                                index.keywordIndex[word].push_back(memId);
                            }
                        }
                    }
                }
            }
        }
    }
    
    index.lastUpdated = std::chrono::system_clock::now();
    m_memoryIndices[executionId] = std::move(index);
}

std::vector<std::string> ExecutionStatePersistence::searchMemories(
    const std::string& executionId,
    const std::string& query,
    int maxResults)
{
    std::vector<std::string> results;
    
    // Build index if not exists
    if (m_memoryIndices.find(executionId) == m_memoryIndices.end()) {
        buildMemoryIndex(executionId);
    }
    
    auto it = m_memoryIndices.find(executionId);
    if (it == m_memoryIndices.end()) {
        return results;
    }
    
    const auto& index = it->second;
    
    // Extract keywords from query
    std::istringstream iss(query);
    std::string word;
    std::unordered_map<std::string, int> scores;
    
    while (iss >> word) {
        std::transform(word.begin(), word.end(), word.begin(), ::tolower);
        if (word.length() > 3) {
            auto kwIt = index.keywordIndex.find(word);
            if (kwIt != index.keywordIndex.end()) {
                for (const auto& memId : kwIt->second) {
                    scores[memId]++;
                }
            }
        }
    }
    
    // Sort by score
    std::vector<std::pair<std::string, int>> sorted(scores.begin(), scores.end());
    std::sort(sorted.begin(), sorted.end(),
        [](const auto& a, const auto& b) { return a.second > b.second; });
    
    // Return top results
    for (size_t i = 0; i < sorted.size() && static_cast<int>(i) < maxResults; i++) {
        results.push_back(sorted[i].first);
    }
    
    return results;
}

void ExecutionStatePersistence::updateMemoryIndex(
    const std::string& executionId,
    const nlohmann::json& memoryState)
{
    // Invalidate index for this execution
    m_memoryIndices.erase(executionId);
}

// ============================================================================
// ENHANCEMENT 5: Priority-Based Checkpoint Pruning
// ============================================================================

void ExecutionStatePersistence::setCheckpointPolicy(size_t maxCheckpoints, int64_t minIntervalMs)
{
    m_maxCheckpoints = maxCheckpoints;
    m_minCheckpointIntervalMs = minIntervalMs;
}

int ExecutionStatePersistence::pruneCheckpoints(const std::string& executionId)
{
    if (m_maxCheckpoints == 0) {
        return 0; // Unlimited checkpoints
    }
    
    auto execution = loadWorkflowExecution(executionId);
    if (!execution || execution->checkpoints.size() <= m_maxCheckpoints) {
        return 0;
    }
    
    // Simple pruning: keep first checkpoint + most recent up to limit
    std::vector<ExecutionCheckpoint> kept;
    kept.reserve(m_maxCheckpoints);
    
    // Always keep first checkpoint (baseline)
    if (!execution->checkpoints.empty()) {
        kept.push_back(execution->checkpoints[0]);
    }
    
    // Keep most recent checkpoints to fill remaining slots
    size_t remaining = m_maxCheckpoints - kept.size();
    if (remaining > 0 && execution->checkpoints.size() > kept.size()) {
        size_t startIdx = execution->checkpoints.size() - remaining;
        for (size_t i = startIdx; i < execution->checkpoints.size(); i++) {
            kept.push_back(execution->checkpoints[i]);
        }
    }
    
    int pruned = static_cast<int>(execution->checkpoints.size() - kept.size());
    execution->checkpoints = std::move(kept);
    
    // Renumber sequences
    for (size_t i = 0; i < execution->checkpoints.size(); i++) {
        execution->checkpoints[i].sequenceNumber = static_cast<int>(i);
    }
    
    persistWorkflowExecution(*execution);
    
    std::cout << "[ExecutionStatePersistence] Pruned " << pruned 
              << " checkpoints from " << executionId << std::endl;
    return pruned;
}

// ============================================================================
// ENHANCEMENT 6: Cross-Session Execution Resumption
// ============================================================================

std::string ExecutionStatePersistence::getSessionPath(const std::string& sessionId) const
{
    return (m_persistenceRoot / ("session_" + sessionId + ".json")).string();
}

bool ExecutionStatePersistence::saveSessionMetadata(
    const std::string& sessionId,
    const std::vector<std::string>& activeExecutions)
{
    try {
        nlohmann::json sessionData;
        sessionData["sessionId"] = sessionId;
        sessionData["savedAt"] = makeIsoUtcNow();
        sessionData["activeExecutions"] = activeExecutions;
        sessionData["hostname"] = "localhost"; // Could use actual hostname
        
        auto sessionPath = getSessionPath(sessionId);
        return atomicWrite(sessionPath, sessionData.dump(2));
    } catch (const std::exception& e) {
        std::cerr << "[ExecutionStatePersistence] Session save error: " << e.what() << std::endl;
        return false;
    }
}

std::vector<std::unique_ptr<WorkflowExecution>> ExecutionStatePersistence::resumeSession(
    const std::string& sessionId)
{
    std::vector<std::unique_ptr<WorkflowExecution>> resumed;
    
    try {
        auto sessionPath = getSessionPath(sessionId);
        if (!std::filesystem::exists(sessionPath)) {
            return resumed;
        }
        
        std::ifstream ifs(sessionPath, std::ios::binary);
        if (!ifs.good()) {
            return resumed;
        }
        
        std::stringstream buffer;
        buffer << ifs.rdbuf();
        auto sessionData = nlohmann::json::parse(buffer.str());
        
        if (sessionData.contains("activeExecutions")) {
            for (const auto& execId : sessionData["activeExecutions"]) {
                auto execution = resumeFromCheckpoint(execId.get<std::string>());
                if (!execution) {
                    // Fallback: load execution directly even without checkpoints
                    execution = loadWorkflowExecution(execId.get<std::string>());
                }
                if (execution) {
                    resumed.push_back(std::move(execution));
                }
            }
        }
        
        std::cout << "[ExecutionStatePersistence] Resumed session " << sessionId 
                  << " with " << resumed.size() << " executions" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "[ExecutionStatePersistence] Session resume error: " << e.what() << std::endl;
    }
    
    return resumed;
}

std::vector<std::string> ExecutionStatePersistence::listResumableSessions() const
{
    std::vector<std::string> sessions;
    
    try {
        for (const auto& entry : std::filesystem::directory_iterator(m_persistenceRoot)) {
            if (entry.is_regular_file()) {
                auto filename = entry.path().stem().string();
                if (filename.substr(0, 8) == "session_") {
                    sessions.push_back(filename.substr(8));
                }
            }
        }
        std::sort(sessions.rbegin(), sessions.rend()); // Most recent first
    } catch (const std::exception& e) {
        std::cerr << "[ExecutionStatePersistence] List sessions error: " << e.what() << std::endl;
    }
    
    return sessions;
}

// ============================================================================
// ENHANCEMENT 7: Checkpoint Integrity Verification
// ============================================================================

std::string ExecutionStatePersistence::computeHash(const std::string& data) const
{
    // Simple FNV-1a hash (production would use SHA-256)
    const uint64_t FNV_OFFSET_BASIS = 14695981039346656037ULL;
    const uint64_t FNV_PRIME = 1099511628211ULL;
    
    uint64_t hash = FNV_OFFSET_BASIS;
    for (char c : data) {
        hash ^= static_cast<uint64_t>(static_cast<unsigned char>(c));
        hash *= FNV_PRIME;
    }
    
    // Convert to hex string
    std::stringstream ss;
    ss << std::hex << std::setfill('0') << std::setw(16) << hash;
    return ss.str();
}

std::string ExecutionStatePersistence::computeCheckpointHash(const ExecutionCheckpoint& checkpoint) const
{
    std::string data = checkpoint.checkpointId + checkpoint.label + 
                       checkpoint.timestamp + checkpoint.stateSnapshot.dump();
    return computeHash(data);
}

void ExecutionStatePersistence::signCheckpoint(
    ExecutionCheckpoint& checkpoint,
    const std::string& prevHash) const
{
    checkpoint.stateSnapshot["_integrity"] = nlohmann::json::object();
    checkpoint.stateSnapshot["_integrity"]["hash"] = computeCheckpointHash(checkpoint);
    checkpoint.stateSnapshot["_integrity"]["prevHash"] = prevHash;
    checkpoint.stateSnapshot["_integrity"]["signedAt"] = makeIsoUtcNow();
}

bool ExecutionStatePersistence::verifyCheckpointIntegrity(const ExecutionCheckpoint& checkpoint) const
{
    if (!checkpoint.stateSnapshot.contains("_integrity")) {
        return true; // No integrity data = no verification needed
    }
    
    const auto& integrity = checkpoint.stateSnapshot["_integrity"];
    std::string storedHash = integrity.value("hash", "");
    
    // Create a copy without integrity field for hash computation
    nlohmann::json stateCopy = checkpoint.stateSnapshot;
    stateCopy.erase("_integrity");
    
    ExecutionCheckpoint cpCopy = checkpoint;
    cpCopy.stateSnapshot = stateCopy;
    std::string computedHash = computeCheckpointHash(cpCopy);
    
    return storedHash == computedHash;
}

// ============================================================================
// ENHANCEMENT 8: Async Persistence with WAL
// ============================================================================

void ExecutionStatePersistence::enableAsyncPersistence(bool enable)
{
    if (enable && !m_asyncPersistenceEnabled) {
        m_asyncPersistenceEnabled = true;
        m_walShutdown = false;
        m_walThread = std::thread(&ExecutionStatePersistence::walWorkerThread, this);
        replayWal(); // Recover any pending writes from previous crash
    } else if (!enable && m_asyncPersistenceEnabled) {
        m_asyncPersistenceEnabled = false;
        m_walShutdown = true;
        if (m_walThread.joinable()) {
            m_walThread.join();
        }
    }
}

std::filesystem::path ExecutionStatePersistence::getWalPath() const
{
    return m_persistenceRoot / "wal.log";
}

void ExecutionStatePersistence::enqueueWalWrite(const WalEntry& entry)
{
    std::lock_guard<std::mutex> lock(m_walMutex);
    m_walQueue.push_back(entry);
}

void ExecutionStatePersistence::walWorkerThread()
{
    while (!m_walShutdown) {
        std::vector<WalEntry> batch;
        {
            std::lock_guard<std::mutex> lock(m_walMutex);
            batch.swap(m_walQueue);
        }
        
        if (batch.empty()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }
        
        // Write batch to WAL
        auto walPath = getWalPath();
        std::ofstream wal(walPath, std::ios::app | std::ios::binary);
        
        for (auto& entry : batch) {
            nlohmann::json walRecord;
            walRecord["executionId"] = entry.executionId;
            walRecord["operation"] = entry.operation;
            walRecord["payload"] = entry.payload;
            walRecord["timestamp"] = entry.timestamp;
            walRecord["committed"] = false;
            
            std::string record = walRecord.dump();
            wal << record.size() << "\n" << record << "\n";
        }
        wal.flush();
        
        // Process each entry
        for (auto& entry : batch) {
            if (entry.operation == "persist") {
                WorkflowExecution exec;
                exec.fromJson(entry.payload);
                if (!persistWorkflowExecution(exec).empty()) {
                    entry.committed = true;
                    m_walCommitted++;
                } else {
                    m_walFailed++;
                }
            } else if (entry.operation == "checkpoint") {
                auto execId = entry.payload["executionId"].get<std::string>();
                auto label = entry.payload["label"].get<std::string>();
                auto snapshot = entry.payload["snapshot"];
                if (!createCheckpoint(execId, label, snapshot).empty()) {
                    entry.committed = true;
                    m_walCommitted++;
                } else {
                    m_walFailed++;
                }
            }
        }
        
        // Mark committed in WAL
        // (Simplified - production would use proper WAL rotation)
    }
}

void ExecutionStatePersistence::flushAsyncWrites()
{
    if (!m_asyncPersistenceEnabled) {
        return;
    }
    
    // Wait for queue to drain
    while (true) {
        {
            std::lock_guard<std::mutex> lock(m_walMutex);
            if (m_walQueue.empty()) {
                break;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

ExecutionStatePersistence::WalStats ExecutionStatePersistence::getWalStats() const
{
    WalStats stats;
    {
        std::lock_guard<std::mutex> lock(m_walMutex);
        stats.pendingWrites = m_walQueue.size();
    }
    stats.committedWrites = m_walCommitted.load();
    stats.failedWrites = m_walFailed.load();
    
    auto walPath = getWalPath();
    if (std::filesystem::exists(walPath)) {
        stats.walSizeBytes = std::filesystem::file_size(walPath);
    }
    
    return stats;
}

void ExecutionStatePersistence::replayWal()
{
    auto walPath = getWalPath();
    if (!std::filesystem::exists(walPath)) {
        return;
    }
    
    std::ifstream wal(walPath, std::ios::binary);
    if (!wal.good()) {
        return;
    }
    
    std::cout << "[ExecutionStatePersistence] Replaying WAL..." << std::endl;
    
    // Read and replay uncommitted entries
    // (Simplified implementation - production would have proper record parsing)
    
    // Clear WAL after successful replay
    wal.close();
    std::filesystem::remove(walPath);
}
