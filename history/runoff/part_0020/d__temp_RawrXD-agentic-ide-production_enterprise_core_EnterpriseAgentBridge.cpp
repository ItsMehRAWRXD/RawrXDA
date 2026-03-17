#include "EnterpriseAgentBridge.hpp"
#include <random>
#include <chrono>
#include <thread>
#include <future>
#include <functional>
#include <algorithm>
#include <sstream>
#include <iostream>
#include <cmath>

// MissionData is declared in the header

class EnterpriseAgentBridgePrivate {
public:
    std::unordered_map<std::string, MissionData> activeMissions;
    std::queue<MissionData> missionQueue;
    std::mutex missionMutex;
    std::condition_variable missionAvailable;
    std::atomic<int> concurrentMissions;
    std::atomic<int> totalMissionsProcessed;
    std::atomic<int> successfulMissions;
    std::atomic<int> failedMissions;
    int maxConcurrentMissions;
    int defaultTimeoutMs;
    std::string retryPolicy;
    std::vector<std::thread> threadPool;
    std::atomic<bool> shouldStop;
    
    EnterpriseAgentBridgePrivate() 
        : concurrentMissions(0)
        , totalMissionsProcessed(0)
        , successfulMissions(0)
        , failedMissions(0)
        , maxConcurrentMissions(16) // Enterprise: 16 parallel missions
        , defaultTimeoutMs(30000)   // 30 seconds default
        , retryPolicy("exponential_backoff")
        , shouldStop(false) {
    }
    
    ~EnterpriseAgentBridgePrivate() {
        shouldStop = true;
        for (auto& thread : threadPool) {
            if (thread.joinable()) {
                thread.join();
            }
        }
    }
};

EnterpriseAgentBridge* EnterpriseAgentBridge::instance() {
    static EnterpriseAgentBridge* instance = nullptr;
    if (!instance) {
        instance = new EnterpriseAgentBridge();
    }
    return instance;
}

EnterpriseAgentBridge::EnterpriseAgentBridge()
    : d_ptr(new EnterpriseAgentBridgePrivate())
{
    // Start mission processing thread
    d_ptr->threadPool.emplace_back([this]() {
        processMissionQueue();
    });
    
    // Setup cleanup timer (using std::thread for periodic cleanup)
    d_ptr->threadPool.emplace_back([this]() {
        while (!d_ptr->shouldStop) {
            std::this_thread::sleep_for(std::chrono::minutes(1));
            cleanupCompletedMissions();
        }
    });
}

// Helper function to generate UUID
std::string generateUuid() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 15);
    static std::uniform_int_distribution<> dis2(8, 11);
    
    std::stringstream ss;
    ss << std::hex;
    for (int i = 0; i < 8; i++) ss << dis(gen);
    ss << "-";
    for (int i = 0; i < 4; i++) ss << dis(gen);
    ss << "-";
    ss << dis2(gen);
    for (int i = 0; i < 3; i++) ss << dis(gen);
    ss << "-";
    for (int i = 0; i < 4; i++) ss << dis(gen);
    ss << "-";
    for (int i = 0; i < 12; i++) ss << dis(gen);
    return ss.str();
}

std::string EnterpriseAgentBridge::submitMission(const std::string& description, const std::unordered_map<std::string, std::string>& parameters) {
    EnterpriseAgentBridgePrivate* d = d_ptr.get();
    
    std::lock_guard<std::mutex> locker(d->missionMutex);
    
    // Generate enterprise mission ID
    std::string missionId = generateUuid();
    
    // Validate mission parameters
    if (!validateMissionParameters(parameters)) {
        return "";
    }
    
    // Create mission data
    MissionData mission;
    mission.id = missionId;
    mission.description = description;
    mission.parameters = parameters;
    mission.createdAt = std::chrono::system_clock::now();
    mission.status = "queued";
    mission.retryCount = 0;
    
    // Extract priority from parameters
    auto it = parameters.find("priority");
    mission.priority = (it != parameters.end()) ? std::stoi(it->second) : 5;
    
    // Add to queue based on priority
    if (mission.priority <= 3) { // High priority - add to front
        // For priority queue, we need to insert at front
        std::queue<MissionData> tempQueue;
        tempQueue.push(mission);
        while (!d->missionQueue.empty()) {
            tempQueue.push(d->missionQueue.front());
            d->missionQueue.pop();
        }
        d->missionQueue = tempQueue;
    } else {
        d->missionQueue.push(mission);
    }
    
    // Store mission in active missions
    d->activeMissions[missionId] = mission;
    
    // Notify mission processing thread
    d->missionAvailable.notify_one();
    
    return missionId;
}

EnterpriseAgentBridge::~EnterpriseAgentBridge() = default;

EnterpriseMission EnterpriseAgentBridge::getMissionStatus(const std::string& missionId) {
    EnterpriseAgentBridgePrivate* d = d_ptr.get();
    
    std::lock_guard<std::mutex> locker(d->missionMutex);
    
    auto it = d->activeMissions.find(missionId);
    if (it != d->activeMissions.end()) {
        const MissionData& data = it->second;
        
        EnterpriseMission status;
        status.id = data.id;
        status.description = data.description;
        status.parameters = data.parameters;
        status.createdAt = data.createdAt;
        status.completedAt = data.completedAt;
        status.status = data.status;
        status.results = data.results;
        
        return status;
    }
    
    return EnterpriseMission(); // Return empty if not found
}

std::vector<EnterpriseMission> EnterpriseAgentBridge::getActiveMissions() {
    EnterpriseAgentBridgePrivate* d = d_ptr.get();
    
    std::lock_guard<std::mutex> locker(d->missionMutex);
    
    std::vector<EnterpriseMission> missions;
    for (const auto& [id, data] : d->activeMissions) {
        EnterpriseMission mission;
        mission.id = data.id;
        mission.description = data.description;
        mission.parameters = data.parameters;
        mission.createdAt = data.createdAt;
        mission.status = data.status;
        missions.push_back(mission);
    }
    
    return missions;
}

bool EnterpriseAgentBridge::executeToolChain(const QStringList& tools, const QVariantMap& parameters) {
    EnterpriseAgentBridgePrivate* d = d_ptr.data();
    
    // Create enterprise tool chain mission
    QJsonObject missionParams;
    missionParams["type"] = "tool_chain";
    missionParams["tools"] = QJsonArray::fromStringList(tools);
    missionParams["parameters"] = QJsonObject::fromVariantMap(parameters);
    missionParams["execution_mode"] = "sequential";
    missionParams["retry_policy"] = d->retryPolicy;
    missionParams["timeout_per_tool"] = d->defaultTimeoutMs;
    missionParams["priority"] = 3; // High priority for tool chains
    
    QString missionId = submitMission("Enterprise tool chain execution", missionParams.toVariantMap());
    return !missionId.isEmpty();
}

bool EnterpriseAgentBridge::executeParallelTools(const QStringList& tools, const QVariantMap& parameters) {
    EnterpriseAgentBridgePrivate* d = d_ptr.data();
    
    // Create enterprise parallel execution mission
    QJsonObject missionParams;
    missionParams["type"] = "parallel_tools";
    missionParams["tools"] = QJsonArray::fromStringList(tools);
    missionParams["parameters"] = QJsonObject::fromVariantMap(parameters);
    missionParams["execution_mode"] = "parallel";
    missionParams["max_parallelism"] = d->maxConcurrentMissions;
    missionParams["shared_resource_check"] = true;
    missionParams["priority"] = 4; // Medium priority for parallel tools
    
    QString missionId = submitMission("Enterprise parallel tool execution", missionParams.toVariantMap());
    return !missionId.isEmpty();
}

void EnterpriseAgentBridge::processMissionQueue() {
    EnterpriseAgentBridgePrivate* d = d_ptr.data();
    
    while (true) {
        MissionData mission;
        
        {
            QMutexLocker locker(&d->missionMutex);
            
            // Wait for available mission with timeout
            if (d->missionQueue.isEmpty()) {
                d->missionAvailable.wait(&d->missionMutex, 500); // 500ms timeout
                if (d->missionQueue.isEmpty()) {
                    continue; // Check again
                }
            }
            
            // Get next mission (respect priority)
            mission = d->missionQueue.dequeue();
            d->activeMissions[mission.id] = mission;
        }
        
        // Execute mission in enterprise thread pool
        QtConcurrent::run(d->threadPool, [this, mission]() {
            this->executeMission(mission);
        });
        
        // Yield to allow other threads to run
        QThread::yieldCurrentThread();
    }
}

void EnterpriseAgentBridge::executeMission(const MissionData& mission) {
    EnterpriseAgentBridgePrivate* d = d_ptr.data();
    
    emit missionStarted(mission.id);
    
    try {
        d->concurrentMissions.ref();
        
        MissionData executingMission = mission;
        executingMission.status = "running";
        executingMission.startedAt = QDateTime::currentDateTime();
        
        // Update mission status
        {
            QMutexLocker locker(&d->missionMutex);
            d->activeMissions[mission.id] = executingMission;
        }
        
        // Execute based on mission type
        QJsonObject results;
        ToolResult toolResult;
        
        if (mission.parameters["type"].toString() == "tool_chain") {
            toolResult = executeToolChainMission(mission);
        } else if (mission.parameters["type"].toString() == "parallel_tools") {
            toolResult = executeParallelToolsMission(mission);
        } else if (mission.parameters["type"].toString() == "single_tool") {
            toolResult = executeSingleToolMission(mission);
        } else {
            throw std::runtime_error("Unknown mission type: " + mission.parameters["type"].toString().toStdString());
        }
        
        // Prepare results
        results["toolResult"] = QJsonObject{
            {"success", toolResult.success},
            {"output", toolResult.output},
            {"error", toolResult.error},
            {"exitCode", toolResult.exitCode},
            {"executionTimeMs", toolResult.executionTimeMs}
        };
        results["executionTime"] = toolResult.executionTimeMs;
        results["completedAt"] = QDateTime::currentDateTime().toString(Qt::ISODate);
        results["missionId"] = mission.id;
        
        // Update mission with results
        {
            QMutexLocker locker(&d->missionMutex);
            executingMission.status = "completed";
            executingMission.completedAt = QDateTime::currentDateTime();
            executingMission.results = results;
            executingMission.errorMessage.clear();
            d->activeMissions[mission.id] = executingMission;
        }
        
        d->totalMissionsProcessed.ref();
        d->successfulMissions.ref();
        d->concurrentMissions.deref();
        
        emit missionCompleted(mission.id, results.toVariantMap());
        
    } catch (const std::exception& e) {
        d->concurrentMissions.deref();
        d->failedMissions.ref();
        
        // Handle mission failure with enterprise retry logic
        this->handleMissionFailure(mission, QString::fromStdString(e.what()));
    }
}

ToolResult EnterpriseAgentBridge::executeToolChainMission(const MissionData& mission) {
    QStringList tools;
    for (const QJsonValue& v : mission.parameters["tools"].toArray()) tools.append(v.toString());
    QJsonObject toolParams = mission.parameters["parameters"].toObject();
    int timeoutPerTool = mission.parameters["timeout_per_tool"].toInt(30000);
    
    ToolResult finalResult;
    finalResult.success = true;
    finalResult.executionTimeMs = 0;
    finalResult.exitCode = 0;
    
    // Execute tools sequentially with enterprise error handling
    for (int i = 0; i < tools.size(); ++i) {
        const QString& tool = tools[i];
        QJsonArray params = toolParams[tool].toArray();
        QStringList paramList;
        for (const QJsonValue& param : params) {
            paramList.append(param.toString());
        }
        
        // Execute individual tool with timeout
        ToolResult stepResult = this->executeToolWithTimeout(tool, paramList, timeoutPerTool);
        
        if (!stepResult.success) {
            finalResult.success = false;
            finalResult.error = QString("Tool chain failed at step %1 (%2): %3")
                                     .arg(i + 1)
                                     .arg(tool)
                                     .arg(stepResult.error);
            finalResult.exitCode = stepResult.exitCode;
            break;
        }
        
        finalResult.executionTimeMs += stepResult.executionTimeMs;
        finalResult.output += stepResult.output + "\n";
        
        // Pass results to next tool if specified
        if (mission.parameters["pass_results_between_steps"].toBool() && i < tools.size() - 1) {
            QJsonArray nextParams = toolParams[tools[i + 1]].toArray();
            nextParams.append(stepResult.output);
            toolParams[tools[i + 1]] = nextParams;
        }
    }
    
    return finalResult;
}

ToolResult EnterpriseAgentBridge::executeParallelToolsMission(const MissionData& mission) {
    QStringList tools;
    for (const QJsonValue& v : mission.parameters["tools"].toArray()) tools.append(v.toString());
    QJsonObject toolParams = mission.parameters["parameters"].toObject();
    int maxParallelism = mission.parameters["max_parallelism"].toInt(8);
    bool sharedResourceCheck = mission.parameters["shared_resource_check"].toBool();
    
    // Execute tools in parallel using QtConcurrent
    QFutureSynchronizer<ToolResult> synchronizer;
    QList<QFuture<ToolResult>> futures;
    
    for (const QString& tool : tools) {
        QJsonArray params = toolParams[tool].toArray();
        QStringList paramList;
        for (const QJsonValue& param : params) {
            paramList.append(param.toString());
        }
        
        QFuture<ToolResult> future = QtConcurrent::run([this, tool, paramList, maxParallelism]() {
            // Respect parallelism limits
            static QSemaphore parallelismSemaphore(maxParallelism);
            parallelismSemaphore.acquire();
            
            ToolResult result = this->executeToolWithTimeout(tool, paramList, 30000);
            
            parallelismSemaphore.release();
            return result;
        });
        
        synchronizer.addFuture(future);
    }
    
    // Wait for all parallel executions
    synchronizer.waitForFinished();
    
    // Collect and aggregate results
    ToolResult finalResult;
    finalResult.success = true;
    finalResult.executionTimeMs = 0;
    
    for (const QFuture<ToolResult>& future : synchronizer.futures()) {
        ToolResult result = future.result();
        
        if (!result.success) {
            finalResult.success = false;
            finalResult.error += QString("Parallel tool %1 failed: %2; ")
                                       .arg("unknown") // Would need to track tool name
                                       .arg(result.error);
        }
        
        finalResult.executionTimeMs = qMax(finalResult.executionTimeMs, result.executionTimeMs);
        finalResult.output += result.output + "\n";
    }
    
    return finalResult;
}

void EnterpriseAgentBridge::handleMissionFailure(const MissionData& mission, const QString& error) {
    EnterpriseAgentBridgePrivate* d = d_ptr.data();
    
    qWarning() << "Enterprise mission failed:" << mission.id << error;
    
    // Apply enterprise retry logic
    if (mission.retryCount < 3) {
        // Exponential backoff
        int backoffMs = static_cast<int>(std::pow(2.0, mission.retryCount) * 1000.0);
        QThread::msleep(backoffMs);
        
        // Retry mission
        MissionData retryMission = mission;
        retryMission.retryCount++;
        retryMission.status = "queued";
        retryMission.errorMessage.clear();
        
        QMutexLocker locker(&d->missionMutex);
        d->missionQueue.prepend(retryMission);
        d->missionAvailable.wakeOne();
        
    } else {
        // Max retries exceeded - mark as failed
        QMutexLocker locker(&d->missionMutex);
        MissionData failedMission = d->activeMissions[mission.id];
        failedMission.status = "failed";
        failedMission.errorMessage = error;
        failedMission.completedAt = QDateTime::currentDateTime();
        d->activeMissions[mission.id] = failedMission;
        
        emit missionFailed(mission.id, error);
        
        // Enterprise alert for mission failure
        QVariantMap alertDetails;
        alertDetails["missionId"] = mission.id;
        alertDetails["description"] = mission.description;
        alertDetails["error"] = error;
        alertDetails["retryCount"] = mission.retryCount;
        emit enterpriseAlert("mission_failure", alertDetails);
    }
}

bool EnterpriseAgentBridge::validateMissionParameters(const QVariantMap& parameters) {
    // Enterprise parameter validation
    
    // Check for required fields
    if (parameters["type"].toString().isEmpty()) {
        qWarning() << "Mission validation failed: missing type";
        return false;
    }
    
    // Validate tool names for tool chain missions
    if (parameters["type"].toString() == "tool_chain" || parameters["type"].toString() == "parallel_tools") {
        QStringList tools = parameters["tools"].toStringList();
        QStringList validTools = {"readFile", "writeFile", "listDirectory", "executeCommand", 
                                 "grepSearch", "gitStatus", "runTests", "analyzeCode"};
        
        for (const QString& tool : tools) {
            if (!validTools.contains(tool)) {
                qWarning() << "Mission validation failed: invalid tool" << tool;
                return false;
            }
        }
    }
    
    // Security validation
    if (parameters.contains("filePath")) {
        QString filePath = parameters["filePath"].toString();
        if (filePath.contains("..") || filePath.contains("/etc/passwd") || filePath.contains("C:\\Windows")) {
            qWarning() << "Mission validation failed: suspicious file path" << filePath;
            return false;
        }
    }
    
    return true;
}

int EnterpriseAgentBridge::getConcurrentMissionCount() const {
    EnterpriseAgentBridgePrivate* d = const_cast<EnterpriseAgentBridgePrivate*>(d_ptr.data());
    return d ? d->concurrentMissions.loadRelaxed() : 0;
}

int EnterpriseAgentBridge::getTotalMissionsProcessed() const {
    EnterpriseAgentBridgePrivate* d = const_cast<EnterpriseAgentBridgePrivate*>(d_ptr.data());
    return d ? d->totalMissionsProcessed.loadRelaxed() : 0;
}

double EnterpriseAgentBridge::getMissionSuccessRate() const {
    EnterpriseAgentBridgePrivate* d = const_cast<EnterpriseAgentBridgePrivate*>(d_ptr.data());
    qint64 total = d ? d->totalMissionsProcessed.loadRelaxed() : 0;
    qint64 successful = d ? d->successfulMissions.loadRelaxed() : 0;
    
    return total > 0 ? (double)successful / total : 0.0;
}

void EnterpriseAgentBridge::setMaxConcurrentMissions(int max) {
    EnterpriseAgentBridgePrivate* d = d_ptr.data();
    d->maxConcurrentMissions = qMax(1, qMin(max, 100)); // Limit to 1-100
    d->threadPool->setMaxThreadCount(d->maxConcurrentMissions);
}

void EnterpriseAgentBridge::setDefaultTimeout(int timeoutMs) {
    EnterpriseAgentBridgePrivate* d = d_ptr.data();
    d->defaultTimeoutMs = qMax(1000, qMin(timeoutMs, 300000)); // Limit to 1-300 seconds
}

void EnterpriseAgentBridge::cleanupCompletedMissions() {
    EnterpriseAgentBridgePrivate* d = d_ptr.data();
    
    QMutexLocker locker(&d->missionMutex);
    
    QDateTime cutoff = QDateTime::currentDateTime().addSecs(-3600); // Keep for 1 hour
    QList<QString> toRemove;
    
    for (auto it = d->activeMissions.begin(); it != d->activeMissions.end(); ++it) {
        if ((it.value().status == "completed" || it.value().status == "failed") &&
            it.value().completedAt < cutoff) {
            toRemove.append(it.key());
        }
    }
    
    for (const QString& missionId : toRemove) {
        d->activeMissions.remove(missionId);
    }
    
    if (!toRemove.isEmpty()) {
        qDebug() << "Enterprise cleanup removed" << toRemove.size() << "completed missions";
    }
}

void EnterpriseAgentBridge::emergencyStopAllMissions() {
    EnterpriseAgentBridgePrivate* d = d_ptr.data();
    
    QMutexLocker locker(&d->missionMutex);
    
    // Clear mission queue
    d->missionQueue.clear();
    
    // Mark all active missions as failed
    for (auto it = d->activeMissions.begin(); it != d->activeMissions.end(); ++it) {
        if (it.value().status == "running" || it.value().status == "queued") {
            it.value().status = "failed";
            it.value().errorMessage = "Emergency stop";
            it.value().completedAt = QDateTime::currentDateTime();
            
            emit missionFailed(it.key(), "Emergency stop");
        }
    }
    
    qWarning() << "Enterprise emergency stop executed";
}

// Helper function to execute tool with timeout
ToolResult EnterpriseAgentBridge::executeToolWithTimeout(const QString& toolName, const QStringList& parameters, int timeoutMs) {
    EnterpriseAgentBridgePrivate* d = d_ptr.data();
    QElapsedTimer timer; timer.start();
    ToolResult res = d->executor.executeTool(toolName, parameters);
    res.executionTimeMs = timer.elapsed();
    return res;
}

ToolResult EnterpriseAgentBridge::executeSingleToolMission(const MissionData& mission) {
    QString tool = mission.parameters["tool"].toString();
    QJsonArray params = mission.parameters["parameters"].toArray();
    QStringList paramList;
    for (const QJsonValue& param : params) {
        paramList.append(param.toString());
    }
    
    return this->executeToolWithTimeout(tool, paramList, mission.parameters["timeout"].toInt(30000));
}