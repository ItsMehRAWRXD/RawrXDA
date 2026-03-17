#include "TaskOrchestrator.h"
#include "../qtapp/MainWindow.h"
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QNetworkRequest>
#include <QTimer>
#include <QTabWidget>
#include <QTextEdit>
#include <QLabel>
#include <QProgressBar>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QApplication>
#include <QDateTime>
#include <QRegularExpression>

namespace RawrXD {

TaskOrchestrator::TaskOrchestrator(MainWindow* parent)
    : QObject(parent)
    , m_mainWindow(parent)
    , m_networkManager(new QNetworkAccessManager(this))
    , m_rollarCoasterEndpoint("http://localhost:11438")
    , m_maxParallelTasks(4)
    , m_taskTimeout(30000)
    , m_memoryProfile("standard")
    , m_globalMemoryLimit(0) // 0 = no limit
    , m_defaultMemoryStrategy("balanced")
    , m_totalMemoryAllocated(0)
{
    // Initialize model capabilities
    m_modelCapabilities["codellama"] = {"code_generation", "completion", "refactoring"};
    m_modelCapabilities["deepseek-coder"] = {"code_generation", "explanation", "analysis", "optimization"};
    m_modelCapabilities["your-custom-model"] = {"code_generation", "explanation", "refactoring", "analysis", "optimization", "creative"};
    m_modelCapabilities["mock-model"] = {"testing", "debugging", "validation"};
    
    // Initialize model workloads
    m_modelWorkloads["codellama"] = 0;
    m_modelWorkloads["deepseek-coder"] = 0;
    m_modelWorkloads["your-custom-model"] = 0;
    m_modelWorkloads["mock-model"] = 0;
    
    // Default model preferences (higher = more preferred)
    m_modelPreferences["your-custom-model"] = 10;
    m_modelPreferences["deepseek-coder"] = 8;
    m_modelPreferences["codellama"] = 6;
    m_modelPreferences["mock-model"] = 1;
    
    // Initialize memory profiles (values in bytes)
    // Minimal: 1KB - 64KB per task
    m_memoryProfiles["minimal"]["min_per_task"] = 1024;        // 1KB
    m_memoryProfiles["minimal"]["max_per_task"] = 65536;       // 64KB
    m_memoryProfiles["minimal"]["global_limit"] = 1048576;     // 1MB total
    
    // Standard: 64KB - 16MB per task
    m_memoryProfiles["standard"]["min_per_task"] = 65536;      // 64KB
    m_memoryProfiles["standard"]["max_per_task"] = 16777216;   // 16MB
    m_memoryProfiles["standard"]["global_limit"] = 1073741824; // 1GB total
    
    // Large: 1MB - 512MB per task
    m_memoryProfiles["large"]["min_per_task"] = 1048576;       // 1MB
    m_memoryProfiles["large"]["max_per_task"] = 536870912;     // 512MB
    m_memoryProfiles["large"]["global_limit"] = 4294967296;    // 4GB total
    
    // Unlimited: No memory limits
    m_memoryProfiles["unlimited"]["min_per_task"] = 0;
    m_memoryProfiles["unlimited"]["max_per_task"] = 0;          // 0 = unlimited
    m_memoryProfiles["unlimited"]["global_limit"] = 0;          // 0 = unlimited
}

// ---------------------------------------------------------------------------
// Agentic tool execution (autonomous)
// ---------------------------------------------------------------------------
RawrXD::Backend::ToolResult TaskOrchestrator::executeTool(const QString& toolName, const QJsonObject& params)
{
    // Convert QJsonObject to std::unordered_map<std::string,std::string>
    std::unordered_map<std::string, std::string> paramMap;
    for (auto it = params.constBegin(); it != params.constEnd(); ++it) {
        paramMap[it.key().toStdString()] = it.value().toString().toStdString();
    }

    // Use the backend executor (which already respects safety toggles)
    return m_toolExecutor.executeTool(toolName.toStdString(), paramMap);
}

TaskOrchestrator::~TaskOrchestrator()
{
    // Cleanup network requests
    for (auto reply : m_networkManager->findChildren<QNetworkReply*>()) {
        reply->abort();
        reply->deleteLater();
    }
}

void TaskOrchestrator::orchestrateTask(const QString& naturalLanguageDescription)
{
    qDebug() << "Orchestrating task:" << naturalLanguageDescription;
    
    // Step 1: Enhanced task decomposition with memory awareness
    QList<TaskDefinition> tasks = decomposeComplexTask(naturalLanguageDescription);
    
    if (tasks.isEmpty()) {
        emit errorOccurred("Failed to decompose task description");
        return;
    }
    
    // Step 2: Apply memory constraints and strategies
    for (TaskDefinition& task : tasks) {
        applyMemoryConstraints(task);
    }
    
    emit taskSplitCompleted(tasks);
    
    // Step 3: Calculate optimal parallelism based on memory and system constraints
    int optimalParallel = calculateOptimalParallelism();
    int actualParallel = qMin(tasks.size(), optimalParallel);
    
    // Step 4: Select models for each task
    QHash<QString, QString> modelAssignments;
    for (TaskDefinition& task : tasks) {
        task.model = selectModelForTask(task);
        modelAssignments[task.id] = task.model;
        m_modelWorkloads[task.model]++;
    }
    
    emit modelSelectionCompleted(modelAssignments);
    
    // Step 5: Balance workload considering memory constraints
    balanceWorkloadWithMemory(tasks);
    
    // Step 6: Execute tasks in parallel (memory-aware)
    int executed = 0;
    for (const TaskDefinition& task : tasks) {
        if (executed >= actualParallel) break;
        
        if (canExecuteTask(task)) {
            m_activeTasks[task.id] = task;
            allocateTaskMemory(task.id, task.memoryLimit);
            createExecutionTab(task);
            executeTask(task);
            executed++;
        }
    }
}

QList<TaskDefinition> TaskOrchestrator::parseNaturalLanguage(const QString& description)
{
    QList<TaskDefinition> tasks;
    
    // Simple keyword-based parsing for common task patterns
    QString lowerDesc = description.toLower();
    
    // Check for multiple task indicators
    QRegularExpression multiTaskRegex("(and|then|next|also|plus|,|;)");
    QRegularExpressionMatchIterator matches = multiTaskRegex.globalMatch(lowerDesc);
    
    if (matches.hasNext()) {
        // Complex task with multiple components
        QStringList parts = description.split(multiTaskRegex, Qt::SkipEmptyParts);
        
        for (int i = 0; i < parts.size(); ++i) {
            TaskDefinition task;
            task.id = generateTaskId();
            task.description = parts[i].trimmed();
            task.type = determineTaskType(task.description);
            task.priority = 5 + (parts.size() - i); // Later tasks get higher priority
            task.estimatedTokens = estimateTokenCount(task.description);
            
            tasks.append(task);
        }
    } else {
        // Single task - try to break it down
        TaskDefinition mainTask;
        mainTask.id = generateTaskId();
        mainTask.description = description;
        mainTask.type = determineTaskType(description);
        mainTask.priority = 5;
        mainTask.estimatedTokens = estimateTokenCount(description);
        
        // For complex single tasks, create subtasks based on task type
        if (mainTask.type == "code_generation" && description.length() > 100) {
            // Break into implementation and testing
            TaskDefinition implTask = mainTask;
            implTask.id = generateTaskId();
            implTask.description = "Implement: " + description;
            implTask.type = "code_generation";
            
            TaskDefinition testTask = mainTask;
            testTask.id = generateTaskId();
            testTask.description = "Test: " + description;
            testTask.type = "validation";
            testTask.priority = 6; // Testing after implementation
            
            tasks = {implTask, testTask};
        } else {
            tasks = {mainTask};
        }
    }
    
    return tasks;
}

QString TaskOrchestrator::determineTaskType(const QString& description)
{
    QString lowerDesc = description.toLower();
    
    if (lowerDesc.contains("implement") || lowerDesc.contains("write") || 
        lowerDesc.contains("create") || lowerDesc.contains("generate")) {
        return "code_generation";
    } else if (lowerDesc.contains("explain") || lowerDesc.contains("describe") ||
               lowerDesc.contains("what is") || lowerDesc.contains("how does")) {
        return "explanation";
    } else if (lowerDesc.contains("refactor") || lowerDesc.contains("optimize") ||
               lowerDesc.contains("improve") || lowerDesc.contains("clean up")) {
        return "refactoring";
    } else if (lowerDesc.contains("analyze") || lowerDesc.contains("review") ||
               lowerDesc.contains("check") || lowerDesc.contains("validate")) {
        return "analysis";
    } else if (lowerDesc.contains("test") || lowerDesc.contains("debug") ||
               lowerDesc.contains("fix") || lowerDesc.contains("correct")) {
        return "validation";
    } else if (lowerDesc.contains("design") || lowerDesc.contains("plan") ||
               lowerDesc.contains("architecture")) {
        return "planning";
    }
    
    return "general";
}

int TaskOrchestrator::estimateTokenCount(const QString& description)
{
    // Rough estimation: ~4 characters per token
    return qMax(100, description.length() / 4);
}

QString TaskOrchestrator::selectModelForTask(const TaskDefinition& task)
{
    // First, check which models support this task type
    QList<QString> capableModels;
    for (const auto& model : m_modelCapabilities.keys()) {
        if (m_modelCapabilities[model].contains(task.type)) {
            capableModels.append(model);
        }
    }
    
    if (capableModels.isEmpty()) {
        // Fallback to general models
        capableModels = m_modelCapabilities.keys();
    }
    
    // Select model based on preference and current workload
    QString bestModel = capableModels.first();
    int bestScore = -1;
    
    for (const QString& model : capableModels) {
        int preference = m_modelPreferences.value(model, 5);
        int workload = m_modelWorkloads.value(model, 0);
        
        // Score = preference - workload (higher is better)
        int score = preference - workload;
        
        if (score > bestScore) {
            bestScore = score;
            bestModel = model;
        }
    }
    
    return bestModel;
}

void TaskOrchestrator::balanceWorkload(QList<TaskDefinition>& tasks)
{
    // Simple balancing: redistribute if one model has significantly more work
    QHash<QString, int> workloadCount;
    for (const TaskDefinition& task : tasks) {
        workloadCount[task.model]++;
    }
    
    int maxWorkload = 0;
    QString maxModel;
    for (const auto& model : workloadCount.keys()) {
        if (workloadCount[model] > maxWorkload) {
            maxWorkload = workloadCount[model];
            maxModel = model;
        }
    }
    
    // If one model has more than 2x the average, redistribute
    int totalTasks = tasks.size();
    if (maxWorkload > totalTasks / 2) {
        for (TaskDefinition& task : tasks) {
            if (task.model == maxModel) {
                // Try to find a less loaded model that can handle this task
                QString alternative = selectModelForTask(task);
                if (alternative != maxModel) {
                    task.model = alternative;
                    workloadCount[maxModel]--;
                    workloadCount[alternative]++;
                    
                    // Recheck if we've balanced enough
                    if (workloadCount[maxModel] <= totalTasks / 2) {
                        break;
                    }
                }
            }
        }
    }
}

void TaskOrchestrator::executeTask(const TaskDefinition& task)
{
    emit taskStarted(task.id, task.model);
    
    // Create the prompt for RollarCoaster
    QString prompt = QString("Task: %1\nType: %2\nPriority: %3\n\nPlease execute this task:")
        .arg(task.description)
        .arg(task.type)
        .arg(task.priority);
    
    // Send request to RollarCoaster
    QJsonObject request = createRollarCoasterRequest(task.model, prompt);
    
    QNetworkRequest networkRequest;
    networkRequest.setUrl(QUrl(m_rollarCoasterEndpoint + "/generate"));
    networkRequest.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    
    QNetworkReply* reply = m_networkManager->post(networkRequest, QJsonDocument(request).toJson());
    
    // Connect reply to handler
    connect(reply, &QNetworkReply::finished, this, [this, reply, task]() {
        handleModelResponse(reply, task.id);
    });
    
    // Set timeout
    QTimer::singleShot(m_taskTimeout, this, [this, task]() {
        OrchestrationResult result;
        result.taskId = task.id;
        result.model = task.model;
        result.success = false;
        result.error = "Task execution timeout";
        
        m_completedTasks[task.id] = result;
        emit taskCompleted(result);
        
        // Check if all tasks are completed
        if (m_completedTasks.size() == m_activeTasks.size()) {
            emit orchestrationCompleted(m_completedTasks.values());
        }
    });
}

void TaskOrchestrator::createExecutionTab(const TaskDefinition& task)
{
    if (!m_mainWindow) return;
    
    // Find the tab widget in MainWindow
    QTabWidget* tabWidget = m_mainWindow->findChild<QTabWidget*>("editorTabs_");
    if (!tabWidget) return;
    
    // Create a new tab for this task
    QWidget* taskTab = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(taskTab);
    
    // Task header
    QHBoxLayout* headerLayout = new QHBoxLayout();
    QLabel* taskLabel = new QLabel(QString("Task: %1").arg(task.description));
    QLabel* modelLabel = new QLabel(QString("Model: %1").arg(task.model));
    QProgressBar* progressBar = new QProgressBar();
    progressBar->setRange(0, 100);
    progressBar->setValue(0);
    
    headerLayout->addWidget(taskLabel);
    headerLayout->addWidget(modelLabel);
    headerLayout->addWidget(progressBar);
    headerLayout->addStretch();
    
    // Task content area
    QTextEdit* contentArea = new QTextEdit();
    contentArea->setPlaceholderText("Waiting for task execution...");
    contentArea->setReadOnly(true);
    
    layout->addLayout(headerLayout);
    layout->addWidget(contentArea);
    
    // Add tab to main window
    QString tabName = QString("Task-%1").arg(task.id.left(8));
    int tabIndex = tabWidget->addTab(taskTab, tabName);
    tabWidget->setCurrentIndex(tabIndex);
    
    emit tabCreated(tabName, task.model);
    
    // Store reference to update progress and content
    m_activeTasks[task.id].parameters["tabIndex"] = tabIndex;
    m_activeTasks[task.id].parameters["progressBar"] = reinterpret_cast<qint64>(progressBar);
    m_activeTasks[task.id].parameters["contentArea"] = reinterpret_cast<qint64>(contentArea);
}

void TaskOrchestrator::handleModelResponse(QNetworkReply* reply, const QString& taskId)
{
    if (!m_activeTasks.contains(taskId)) {
        reply->deleteLater();
        return;
    }
    
    OrchestrationResult result;
    result.taskId = taskId;
    result.model = m_activeTasks[taskId].model;
    
    if (reply->error() == QNetworkReply::NoError) {
        QByteArray responseData = reply->readAll();
        QJsonDocument doc = QJsonDocument::fromJson(responseData);
        
        if (!doc.isNull() && doc.isObject()) {
            QJsonObject obj = doc.object();
            result.result = obj.value("response").toString();
            result.success = true;
        } else {
            result.success = false;
            result.error = "Invalid response format";
        }
    } else {
        result.success = false;
        result.error = reply->errorString();
    }
    
    result.executionTime = QDateTime::currentDateTime().toMSecsSinceEpoch() - 
                          m_activeTasks[taskId].parameters["startTime"].toInt();
    
    // Update the task tab with results
    if (m_activeTasks[taskId].parameters.contains("contentArea")) {
        QTextEdit* contentArea = reinterpret_cast<QTextEdit*>(
            m_activeTasks[taskId].parameters["contentArea"].toInt());
        if (contentArea) {
            contentArea->setPlainText(result.success ? result.result : result.error);
        }
    }
    
    if (m_activeTasks[taskId].parameters.contains("progressBar")) {
        QProgressBar* progressBar = reinterpret_cast<QProgressBar*>(
            m_activeTasks[taskId].parameters["progressBar"].toInt());
        if (progressBar) {
            progressBar->setValue(100);
        }
    }
    
    // Move task from active to completed
    m_completedTasks[taskId] = result;
    m_activeTasks.remove(taskId);
    
    // Release memory allocated for this task
    releaseTaskMemory(taskId);
    
    // Update model workload
    m_modelWorkloads[result.model] = qMax(0, m_modelWorkloads[result.model] - 1);
    
    emit taskCompleted(result);
    
    // Check if all tasks are completed
    if (m_activeTasks.isEmpty()) {
        emit orchestrationCompleted(m_completedTasks.values());
    }
    
    reply->deleteLater();
}

QJsonObject TaskOrchestrator::createRollarCoasterRequest(const QString& model, const QString& prompt) const
{
    QJsonObject request;
    request["model"] = model;
    request["prompt"] = prompt;
    request["max_tokens"] = 2000;
    request["temperature"] = 0.7;
    request["top_p"] = 0.9;
    request["stream"] = false;
    
    return request;
}

QString TaskOrchestrator::generateTaskId() const
{
    return QString("task_%1_%2")
        .arg(QDateTime::currentDateTime().toMSecsSinceEpoch())
        .arg(qrand() % 10000);
}

QList<QString> TaskOrchestrator::getAvailableModels() const
{
    return m_modelCapabilities.keys();
}

bool TaskOrchestrator::isModelAvailable(const QString& model) const
{
    return m_modelCapabilities.contains(model);
}

void TaskOrchestrator::setModelPreferences(const QHash<QString, int>& preferences)
{
    m_modelPreferences = preferences;
}

QList<TaskDefinition> TaskOrchestrator::getCurrentTasks() const
{
    return m_activeTasks.values();
}

OrchestrationResult TaskOrchestrator::getTaskResult(const QString& taskId) const
{
    return m_completedTasks.value(taskId);
}

void TaskOrchestrator::cancelTask(const QString& taskId)
{
    if (m_activeTasks.contains(taskId)) {
        // Decrement model workload before removing the task entry
        const TaskDefinition& task = m_activeTasks[taskId];
        if (m_modelWorkloads.contains(task.model)) {
            m_modelWorkloads[task.model] = qMax(0, m_modelWorkloads[task.model] - 1);
        }
        
        // Record cancellation result
        OrchestrationResult result;
        result.taskId = taskId;
        result.success = false;
        result.error = "Cancelled by user";
        m_completedTasks[taskId] = result;
        
        m_activeTasks.remove(taskId);
        
        emit taskCancelled(taskId);
    }
}

void TaskOrchestrator::setRollarCoasterEndpoint(const QString& endpoint)
{
    m_rollarCoasterEndpoint = endpoint;
}

void TaskOrchestrator::setMaxParallelTasks(int maxTasks)
{
    m_maxParallelTasks = qMax(1, maxTasks);
}

void TaskOrchestrator::setTaskTimeout(int timeoutMs)
{
    m_taskTimeout = qMax(1000, timeoutMs);
}

// Memory Management Implementation

void TaskOrchestrator::setMemoryProfile(const QString& profileName)
{
    if (m_memoryProfiles.contains(profileName)) {
        m_memoryProfile = profileName;
        m_globalMemoryLimit = m_memoryProfiles[profileName]["global_limit"];
        qDebug() << "Memory profile set to:" << profileName;
    } else {
        qWarning() << "Unknown memory profile:" << profileName;
    }
}

void TaskOrchestrator::setGlobalMemoryLimit(qint64 limitBytes)
{
    m_globalMemoryLimit = limitBytes;
}

void TaskOrchestrator::setTaskMemoryStrategy(const QString& strategy)
{
    if (strategy == "conservative" || strategy == "balanced" || strategy == "aggressive") {
        m_defaultMemoryStrategy = strategy;
    }
}

qint64 TaskOrchestrator::getAvailableMemory() const
{
    if (m_globalMemoryLimit == 0) return LLONG_MAX; // Unlimited
    return m_globalMemoryLimit - m_totalMemoryAllocated;
}

qint64 TaskOrchestrator::getTotalMemoryUsage() const
{
    return m_totalMemoryAllocated;
}

bool TaskOrchestrator::canAllocateMemory(qint64 requestedBytes) const
{
    if (m_globalMemoryLimit == 0) return true; // Unlimited
    return (m_totalMemoryAllocated + requestedBytes) <= m_globalMemoryLimit;
}

void TaskOrchestrator::allocateTaskMemory(const QString& taskId, qint64 bytes)
{
    if (bytes > 0 && canAllocateMemory(bytes)) {
        m_taskMemoryUsage[taskId] = bytes;
        m_totalMemoryAllocated += bytes;
        qDebug() << "Allocated" << bytes << "bytes for task" << taskId;
    }
}

void TaskOrchestrator::releaseTaskMemory(const QString& taskId)
{
    if (m_taskMemoryUsage.contains(taskId)) {
        qint64 released = m_taskMemoryUsage[taskId];
        m_totalMemoryAllocated = qMax((qint64)0, m_totalMemoryAllocated - released);
        m_taskMemoryUsage.remove(taskId);
        qDebug() << "Released" << released << "bytes for task" << taskId;
    }
}

// Enhanced Task Decomposition

QList<TaskDefinition> TaskOrchestrator::decomposeComplexTask(const QString& description)
{
    QList<TaskDefinition> tasks;
    
    // Use the original parsing as base
    QList<TaskDefinition> baseTasks = parseNaturalLanguage(description);
    
    // Enhance decomposition based on complexity and memory requirements
    for (TaskDefinition baseTask : baseTasks) {
        if (shouldDecomposeFurther(baseTask)) {
            QList<TaskDefinition> subtasks = createMemoryAwareSubtasks(baseTask);
            tasks.append(subtasks);
        } else {
            tasks.append(baseTask);
        }
    }
    
    return tasks;
}

QList<TaskDefinition> TaskOrchestrator::createMemoryAwareSubtasks(const TaskDefinition& mainTask)
{
    QList<TaskDefinition> subtasks;
    
    // Break down complex tasks into smaller, memory-efficient subtasks
    if (mainTask.type == "code_generation") {
        // Split into: planning, implementation, testing
        TaskDefinition planTask = mainTask;
        planTask.id = generateTaskId();
        planTask.description = "Plan: " + mainTask.description;
        planTask.type = "planning";
        planTask.priority = mainTask.priority + 1;
        planTask.memoryLimit = calculateMemoryForTask(planTask);
        
        TaskDefinition implTask = mainTask;
        implTask.id = generateTaskId();
        implTask.description = "Implement: " + mainTask.description;
        implTask.type = "code_generation";
        implTask.priority = mainTask.priority;
        implTask.memoryLimit = calculateMemoryForTask(implTask);
        
        TaskDefinition testTask = mainTask;
        testTask.id = generateTaskId();
        testTask.description = "Test: " + mainTask.description;
        testTask.type = "validation";
        testTask.priority = mainTask.priority - 1;
        testTask.memoryLimit = calculateMemoryForTask(testTask);
        
        subtasks = {planTask, implTask, testTask};
    } else if (mainTask.type == "analysis") {
        // Split into: data collection, analysis, reporting
        TaskDefinition collectTask = mainTask;
        collectTask.id = generateTaskId();
        collectTask.description = "Collect data for: " + mainTask.description;
        collectTask.type = "analysis";
        collectTask.memoryLimit = calculateMemoryForTask(collectTask);
        
        TaskDefinition analyzeTask = mainTask;
        analyzeTask.id = generateTaskId();
        analyzeTask.description = "Analyze: " + mainTask.description;
        analyzeTask.type = "analysis";
        analyzeTask.memoryLimit = calculateMemoryForTask(analyzeTask);
        
        TaskDefinition reportTask = mainTask;
        reportTask.id = generateTaskId();
        reportTask.description = "Report on: " + mainTask.description;
        reportTask.type = "explanation";
        reportTask.memoryLimit = calculateMemoryForTask(reportTask);
        
        subtasks = {collectTask, analyzeTask, reportTask};
    } else {
        // For other types, create smaller chunks if description is too long
        if (mainTask.description.length() > 200) {
            QStringList chunks = splitDescription(mainTask.description);
            for (int i = 0; i < chunks.size(); ++i) {
                TaskDefinition chunkTask = mainTask;
                chunkTask.id = generateTaskId();
                chunkTask.description = chunks[i];
                chunkTask.priority = mainTask.priority - i; // Later chunks lower priority
                chunkTask.memoryLimit = calculateMemoryForTask(chunkTask);
                subtasks.append(chunkTask);
            }
        } else {
            TaskDefinition task = mainTask;
            task.memoryLimit = calculateMemoryForTask(task);
            subtasks.append(task);
        }
    }
    
    return subtasks;
}

int TaskOrchestrator::calculateOptimalParallelism() const
{
    // Base parallelism on memory availability and active tasks
    int memoryBasedParallel = m_maxParallelTasks;
    
    if (m_globalMemoryLimit > 0) {
        qint64 avgMemoryPerTask = m_memoryProfiles[m_memoryProfile]["max_per_task"] / 2;
        if (avgMemoryPerTask > 0) {
            memoryBasedParallel = qMin(memoryBasedParallel, (int)(getAvailableMemory() / avgMemoryPerTask));
        }
    }
    
    return qMax(1, memoryBasedParallel);
}

bool TaskOrchestrator::shouldDecomposeFurther(const TaskDefinition& task) const
{
    // Decompose if task is complex or memory-intensive
    bool isComplex = task.description.length() > 150 || 
                    task.estimatedTokens > 2000 ||
                    task.type == "code_generation" ||
                    task.type == "analysis";
    
    bool memoryIntensive = m_memoryProfile != "unlimited" && 
                          calculateMemoryForTask(task) > m_memoryProfiles[m_memoryProfile]["max_per_task"] / 2;
    
    return isComplex || memoryIntensive;
}

qint64 TaskOrchestrator::calculateMemoryForTask(const TaskDefinition& task) const
{
    // Calculate memory based on task type, complexity, and profile
    qint64 baseMemory = m_memoryProfiles[m_memoryProfile]["min_per_task"];
    qint64 maxMemory = m_memoryProfiles[m_memoryProfile]["max_per_task"];
    
    if (maxMemory == 0) return 0; // Unlimited profile
    
    // Adjust based on task type
    double multiplier = 1.0;
    if (task.type == "code_generation") multiplier = 1.5;
    else if (task.type == "analysis") multiplier = 2.0;
    else if (task.type == "refactoring") multiplier = 1.2;
    else if (task.type == "validation") multiplier = 0.8;
    
    // Adjust based on description length (complexity)
    double complexityMultiplier = 1.0 + (task.description.length() / 500.0);
    
    // Adjust based on memory strategy
    double strategyMultiplier = 1.0;
    if (task.memoryStrategy == "conservative") strategyMultiplier = 0.7;
    else if (task.memoryStrategy == "aggressive") strategyMultiplier = 1.3;
    
    qint64 calculated = baseMemory * multiplier * complexityMultiplier * strategyMultiplier;
    return qMin(calculated, maxMemory);
}

void TaskOrchestrator::applyMemoryConstraints(TaskDefinition& task)
{
    task.memoryLimit = calculateMemoryForTask(task);
    task.memoryStrategy = m_defaultMemoryStrategy;
}

void TaskOrchestrator::balanceWorkloadWithMemory(QList<TaskDefinition>& tasks)
{
    // Original workload balancing plus memory considerations
    balanceWorkload(tasks);
    
    // Additional memory-aware balancing
    QHash<QString, qint64> modelMemoryUsage;
    for (const TaskDefinition& task : tasks) {
        modelMemoryUsage[task.model] += task.memoryLimit;
    }
    
    // Redistribute if memory usage is imbalanced
    qint64 avgMemoryPerModel = 0;
    for (qint64 usage : modelMemoryUsage.values()) {
        avgMemoryPerModel += usage;
    }
    if (!modelMemoryUsage.isEmpty()) {
        avgMemoryPerModel /= modelMemoryUsage.size();
    }
    
    for (TaskDefinition& task : tasks) {
        if (modelMemoryUsage[task.model] > avgMemoryPerModel * 1.5) {
            // Try to move to a less memory-loaded model
            for (const QString& model : m_modelCapabilities.keys()) {
                if (model != task.model && 
                    m_modelCapabilities[model].contains(task.type) &&
                    modelMemoryUsage[model] < avgMemoryPerModel) {
                    modelMemoryUsage[task.model] -= task.memoryLimit;
                    task.model = model;
                    modelMemoryUsage[model] += task.memoryLimit;
                    m_modelWorkloads[task.model]++;
                    break;
                }
            }
        }
    }
}

bool TaskOrchestrator::canExecuteTask(const TaskDefinition& task) const
{
    return canAllocateMemory(task.memoryLimit);
}

QStringList TaskOrchestrator::splitDescription(const QString& description) const
{
    // Split long descriptions into manageable chunks
    QStringList chunks;
    QStringList sentences = description.split(QRegularExpression("[.!?]\\s+"));
    
    QString currentChunk;
    for (const QString& sentence : sentences) {
        if (currentChunk.length() + sentence.length() > 150) {
            if (!currentChunk.isEmpty()) {
                chunks.append(currentChunk.trimmed());
                currentChunk = sentence;
            } else {
                chunks.append(sentence);
            }
        } else {
            if (!currentChunk.isEmpty()) currentChunk += ". ";
            currentChunk += sentence;
        }
    }
    
    if (!currentChunk.isEmpty()) {
        chunks.append(currentChunk.trimmed());
    }
    
    return chunks;
}

} // namespace RawrXD