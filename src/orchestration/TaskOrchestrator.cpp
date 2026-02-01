#include "TaskOrchestrator.h"
#include "../qtapp/MainWindow.h"
namespace RawrXD {

TaskOrchestrator::TaskOrchestrator(::MainWindow* parent)
    
    , m_mainWindow(parent)
    , m_networkManager(new void*(this))
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
RawrXD::Backend::ToolResult TaskOrchestrator::executeTool(const std::string& toolName, const void*& params)
{
    void* doc(params);
    std::string paramsJson = doc.toJson(void*::Compact);
    return m_toolExecutor.executeTool(toolName, paramsJson);
}

TaskOrchestrator::~TaskOrchestrator()
{
    // Cleanup network requests
    for (auto reply : m_networkManager->findChildren<void**>()) {
        reply->abort();
        reply->deleteLater();
    }
}

void TaskOrchestrator::orchestrateTask(const std::string& naturalLanguageDescription)
{
    
    // Step 1: Enhanced task decomposition with memory awareness
    std::vector<TaskDefinition> tasks = decomposeComplexTask(naturalLanguageDescription);
    
    if (tasks.empty()) {
        errorOccurred("Failed to decompose task description");
        return;
    }
    
    // Step 2: Apply memory constraints and strategies
    for (TaskDefinition& task : tasks) {
        applyMemoryConstraints(task);
    }
    
    taskSplitCompleted(tasks);
    
    // Step 3: Calculate optimal parallelism based on memory and system constraints
    int optimalParallel = calculateOptimalParallelism();
    int actualParallel = qMin(tasks.size(), optimalParallel);
    
    // Step 4: Select models for each task
    std::map<std::string, std::string> modelAssignments;
    for (TaskDefinition& task : tasks) {
        task.model = selectModelForTask(task);
        modelAssignments[task.id] = task.model;
        m_modelWorkloads[task.model]++;
    }
    
    modelSelectionCompleted(modelAssignments);
    
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

std::vector<TaskDefinition> TaskOrchestrator::parseNaturalLanguage(const std::string& description)
{
    std::vector<TaskDefinition> tasks;
    
    // Simple keyword-based parsing for common task patterns
    std::string lowerDesc = description.toLower();
    
    // Check for multiple task indicators
    std::regex multiTaskRegex("(and|then|next|also|plus|,|;)");
    std::regexMatchIterator matches = multiTaskRegex;
    
    if (matchesfalse) {
        // Complex task with multiple components
        std::stringList parts = description.split(multiTaskRegex, SkipEmptyParts);
        
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

std::string TaskOrchestrator::determineTaskType(const std::string& description)
{
    std::string lowerDesc = description.toLower();
    
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

int TaskOrchestrator::estimateTokenCount(const std::string& description)
{
    // Rough estimation: ~4 characters per token
    return qMax(100, description.length() / 4);
}

std::string TaskOrchestrator::selectModelForTask(const TaskDefinition& task)
{
    // First, check which models support this task type
    std::vector<std::string> capableModels;
    for (const auto& model : m_modelCapabilities.keys()) {
        if (m_modelCapabilities[model].contains(task.type)) {
            capableModels.append(model);
        }
    }
    
    if (capableModels.empty()) {
        // Fallback to general models
        capableModels = m_modelCapabilities.keys();
    }
    
    // Select model based on preference and current workload
    std::string bestModel = capableModels.first();
    int bestScore = -1;
    
    for (const std::string& model : capableModels) {
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

void TaskOrchestrator::balanceWorkload(std::vector<TaskDefinition>& tasks)
{
    // Simple balancing: redistribute if one model has significantly more work
    std::map<std::string, int> workloadCount;
    for (const TaskDefinition& task : tasks) {
        workloadCount[task.model]++;
    }
    
    int maxWorkload = 0;
    std::string maxModel;
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
                std::string alternative = selectModelForTask(task);
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
    taskStarted(task.id, task.model);
    
    // Create the prompt for RollarCoaster
    std::string prompt = std::string("Task: %1\nType: %2\nPriority: %3\n\nPlease execute this task:")


        ;
    
    // Send request to RollarCoaster
    void* request = createRollarCoasterRequest(task.model, prompt);
    
    void* networkRequest;
    networkRequest.setUrl(std::string(m_rollarCoasterEndpoint + "/generate"));
    networkRequest.setHeader(void*::ContentTypeHeader, "application/json");
    
    void** reply = m_networkManager->post(networkRequest, void*(request).toJson());
    
    // Connect reply to handler  // Signal connection removed\n});
    
    // Set timeout
    // Timer::singleShot(m_taskTimeout, this, [this, task]() {
        OrchestrationResult result;
        result.taskId = task.id;
        result.model = task.model;
        result.success = false;
        result.error = "Task execution timeout";
        
        m_completedTasks[task.id] = result;
        taskCompleted(result);
        
        // Check if all tasks are completed
        if (m_completedTasks.size() == m_activeTasks.size()) {
            orchestrationCompleted(m_completedTasks.values());
        }
    });
}

void TaskOrchestrator::createExecutionTab(const TaskDefinition& task)
{
    if (!m_mainWindow) return;
    
    // Find the tab widget in MainWindow
    void* tabWidget = m_mainWindow->findChild<void*>("editorTabs_");
    if (!tabWidget) return;
    
    // Create a new tab for this task
    void* taskTab = new // Widget();
    void* layout = new void(taskTab);
    
    // Task header
    void* headerLayout = new void();
    void* taskLabel = new void(std::string("Task: %1"));
    void* modelLabel = new void(std::string("Model: %1"));
    void* progressBar = new void();
    progressBar->setRange(0, 100);
    progressBar->setValue(0);
    
    headerLayout->addWidget(taskLabel);
    headerLayout->addWidget(modelLabel);
    headerLayout->addWidget(progressBar);
    headerLayout->addStretch();
    
    // Task content area
    void* contentArea = new void();
    contentArea->setPlaceholderText("Waiting for task execution...");
    contentArea->setReadOnly(true);
    
    layout->addLayout(headerLayout);
    layout->addWidget(contentArea);
    
    // Add tab to main window
    std::string tabName = std::string("Task-%1"));
    int tabIndex = tabWidget->addTab(taskTab, tabName);
    tabWidget->setCurrentIndex(tabIndex);
    
    tabCreated(tabName, task.model);
    
    // Store reference to update progress and content
    m_activeTasks[task.id].parameters["tabIndex"] = tabIndex;
    m_activeTasks[task.id].parameters["progressBar"] = std::any::fromValue<quintptr>(
        reinterpret_cast<quintptr>(progressBar));
    m_activeTasks[task.id].parameters["contentArea"] = std::any::fromValue<quintptr>(
        reinterpret_cast<quintptr>(contentArea));
}

void TaskOrchestrator::handleModelResponse(void** reply, const std::string& taskId)
{
    if (!m_activeTasks.contains(taskId)) {
        reply->deleteLater();
        return;
    }
    
    OrchestrationResult result;
    result.taskId = taskId;
    result.model = m_activeTasks[taskId].model;
    
    if (reply->error() == void*::NoError) {
        std::vector<uint8_t> responseData = reply->readAll();
        void* doc = void*::fromJson(responseData);
        
        if (!doc.isNull() && doc.isObject()) {
            void* obj = doc.object();
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
    
    result.executionTime = // DateTime::currentDateTime().toMSecsSinceEpoch() - 
                          m_activeTasks[taskId].parameters["startTime"].toLongLong();
    
    // Update the task tab with results
    if (m_activeTasks[taskId].parameters.contains("contentArea")) {
        void* contentArea = reinterpret_cast<void*>(
            m_activeTasks[taskId].parameters["contentArea"].value<quintptr>());
        if (contentArea) {
            contentArea->setPlainText(result.success ? result.result : result.error);
        }
    }
    
    if (m_activeTasks[taskId].parameters.contains("progressBar")) {
        void* progressBar = reinterpret_cast<void*>(
            m_activeTasks[taskId].parameters["progressBar"].value<quintptr>());
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
    
    taskCompleted(result);
    
    // Check if all tasks are completed
    if (m_activeTasks.empty()) {
        orchestrationCompleted(m_completedTasks.values());
    }
    
    reply->deleteLater();
}

void* TaskOrchestrator::createRollarCoasterRequest(const std::string& model, const std::string& prompt) const
{
    void* request;
    request["model"] = model;
    request["prompt"] = prompt;
    request["max_tokens"] = 2000;
    request["temperature"] = 0.7;
    request["top_p"] = 0.9;
    request["stream"] = false;
    
    return request;
}

std::string TaskOrchestrator::generateTaskId() const
{
    return std::string("task_%1_%2")
        .toMSecsSinceEpoch())
    ->bounded(10000));
}

std::vector<std::string> TaskOrchestrator::getAvailableModels() const
{
    return m_modelCapabilities.keys();
}

bool TaskOrchestrator::isModelAvailable(const std::string& model) const
{
    return m_modelCapabilities.contains(model);
}

void TaskOrchestrator::setModelPreferences(const std::map<std::string, int>& preferences)
{
    m_modelPreferences = preferences;
}

std::vector<TaskDefinition> TaskOrchestrator::getCurrentTasks() const
{
    return m_activeTasks.values();
}

OrchestrationResult TaskOrchestrator::getTaskResult(const std::string& taskId) const
{
    return m_completedTasks.value(taskId);
}

void TaskOrchestrator::cancelTask(const std::string& taskId)
{
    if (m_activeTasks.contains(taskId)) {
        // Mark task as cancelled
        Task& task = m_activeTasks[taskId];
        task.status = TaskStatus::FAILED;
        task.error = "Task cancelled by user";
        
        // Decrement model workload
        if (m_modelWorkloads.contains(task.model)) {
            m_modelWorkloads[task.model] = std::max(0, m_modelWorkloads[task.model] - 1);
        }
        
        // Remove from active tasks
        m_activeTasks.remove(taskId);
        
        // Emit task failed signal
        taskFailed(taskId, "Task cancelled");
    }
}

void TaskOrchestrator::setRollarCoasterEndpoint(const std::string& endpoint)
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

void TaskOrchestrator::setMemoryProfile(const std::string& profileName)
{
    if (m_memoryProfiles.contains(profileName)) {
        m_memoryProfile = profileName;
        m_globalMemoryLimit = m_memoryProfiles[profileName]["global_limit"];
    } else {
    }
}

void TaskOrchestrator::setGlobalMemoryLimit(int64_t limitBytes)
{
    m_globalMemoryLimit = limitBytes;
}

void TaskOrchestrator::setTaskMemoryStrategy(const std::string& strategy)
{
    if (strategy == "conservative" || strategy == "balanced" || strategy == "aggressive") {
        m_defaultMemoryStrategy = strategy;
    }
}

int64_t TaskOrchestrator::getAvailableMemory() const
{
    if (m_globalMemoryLimit == 0) return LLONG_MAX; // Unlimited
    return m_globalMemoryLimit - m_totalMemoryAllocated;
}

int64_t TaskOrchestrator::getTotalMemoryUsage() const
{
    return m_totalMemoryAllocated;
}

bool TaskOrchestrator::canAllocateMemory(int64_t requestedBytes) const
{
    if (m_globalMemoryLimit == 0) return true; // Unlimited
    return (m_totalMemoryAllocated + requestedBytes) <= m_globalMemoryLimit;
}

void TaskOrchestrator::allocateTaskMemory(const std::string& taskId, int64_t bytes)
{
    if (bytes > 0 && canAllocateMemory(bytes)) {
        m_taskMemoryUsage[taskId] = bytes;
        m_totalMemoryAllocated += bytes;
    }
}

void TaskOrchestrator::releaseTaskMemory(const std::string& taskId)
{
    if (m_taskMemoryUsage.contains(taskId)) {
        int64_t released = m_taskMemoryUsage[taskId];
        m_totalMemoryAllocated = qMax((int64_t)0, m_totalMemoryAllocated - released);
        m_taskMemoryUsage.remove(taskId);
    }
}

// Enhanced Task Decomposition

std::vector<TaskDefinition> TaskOrchestrator::decomposeComplexTask(const std::string& description)
{
    std::vector<TaskDefinition> tasks;
    
    // Use the original parsing as base
    std::vector<TaskDefinition> baseTasks = parseNaturalLanguage(description);
    
    // Enhance decomposition based on complexity and memory requirements
    for (TaskDefinition baseTask : baseTasks) {
        if (shouldDecomposeFurther(baseTask)) {
            std::vector<TaskDefinition> subtasks = createMemoryAwareSubtasks(baseTask);
            tasks.append(subtasks);
        } else {
            tasks.append(baseTask);
        }
    }
    
    return tasks;
}

std::vector<TaskDefinition> TaskOrchestrator::createMemoryAwareSubtasks(const TaskDefinition& mainTask)
{
    std::vector<TaskDefinition> subtasks;
    
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
            std::stringList chunks = splitDescription(mainTask.description);
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
        int64_t avgMemoryPerTask = m_memoryProfiles[m_memoryProfile]["max_per_task"] / 2;
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

int64_t TaskOrchestrator::calculateMemoryForTask(const TaskDefinition& task) const
{
    // Calculate memory based on task type, complexity, and profile
    int64_t baseMemory = m_memoryProfiles[m_memoryProfile]["min_per_task"];
    int64_t maxMemory = m_memoryProfiles[m_memoryProfile]["max_per_task"];
    
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
    
    int64_t calculated = baseMemory * multiplier * complexityMultiplier * strategyMultiplier;
    return qMin(calculated, maxMemory);
}

void TaskOrchestrator::applyMemoryConstraints(TaskDefinition& task)
{
    task.memoryLimit = calculateMemoryForTask(task);
    task.memoryStrategy = m_defaultMemoryStrategy;
}

void TaskOrchestrator::balanceWorkloadWithMemory(std::vector<TaskDefinition>& tasks)
{
    // Original workload balancing plus memory considerations
    balanceWorkload(tasks);
    
    // Additional memory-aware balancing
    std::map<std::string, int64_t> modelMemoryUsage;
    for (const TaskDefinition& task : tasks) {
        modelMemoryUsage[task.model] += task.memoryLimit;
    }
    
    // Redistribute if memory usage is imbalanced
    int64_t avgMemoryPerModel = 0;
    for (int64_t usage : modelMemoryUsage.values()) {
        avgMemoryPerModel += usage;
    }
    if (!modelMemoryUsage.empty()) {
        avgMemoryPerModel /= modelMemoryUsage.size();
    }
    
    for (TaskDefinition& task : tasks) {
        if (modelMemoryUsage[task.model] > avgMemoryPerModel * 1.5) {
            // Try to move to a less memory-loaded model
            for (const std::string& model : m_modelCapabilities.keys()) {
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

std::stringList TaskOrchestrator::splitDescription(const std::string& description) const
{
    // Split long descriptions into manageable chunks
    std::stringList chunks;
    std::stringList sentences = description.split(std::regex("[.!?]\\s+"));
    
    std::string currentChunk;
    for (const std::string& sentence : sentences) {
        if (currentChunk.length() + sentence.length() > 150) {
            if (!currentChunk.empty()) {
                chunks.append(currentChunk.trimmed());
                currentChunk = sentence;
            } else {
                chunks.append(sentence);
            }
        } else {
            if (!currentChunk.empty()) currentChunk += ". ";
            currentChunk += sentence;
        }
    }
    
    if (!currentChunk.empty()) {
        chunks.append(currentChunk.trimmed());
    }
    
    return chunks;
}

} // namespace RawrXD

