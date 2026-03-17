#include "advanced_planning_engine.h"
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QDebug>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>

AdvancedPlanningEngine::AdvancedPlanningEngine(QObject* parent)
    : QObject(parent)
{
    // Initialize planning heuristics
    m_complexityWeights["code_generation"] = 3.0;
    m_complexityWeights["refactoring"] = 2.5;
    m_complexityWeights["testing"] = 2.0;
    m_complexityWeights["debugging"] = 4.0;
    m_complexityWeights["optimization"] = 3.5;
    m_complexityWeights["documentation"] = 1.5;
    
    m_resourceLimits["max_parallel_tasks"] = 8;
    m_resourceLimits["max_memory_mb"] = 4096;
    m_resourceLimits["max_cpu_cores"] = 16;
}

AdvancedPlanningEngine::~AdvancedPlanningEngine() {
    // Clean up any active sessions
    for (auto& session : m_activeSessions) {
        if (session && session->progressTimer) {
            session->progressTimer->stop();
            delete session->progressTimer;
        }
    }
}

QJsonObject AdvancedPlanningEngine::createMasterPlan(const QString& goal) {
    qDebug() << "[AdvancedPlanningEngine] Creating master plan for goal:" << goal;
    
    QJsonObject masterPlan;
    masterPlan["goal"] = goal;
    masterPlan["id"] = QUuid::createUuid().toString();
    masterPlan["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    masterPlan["status"] = "planning";
    masterPlan["complexity"] = analyzeComplexity(goal);
    
    // Parse goal into high-level tasks
    QJsonObject parsedTasks = parseGoalToTasks(goal);
    masterPlan["high_level_tasks"] = parsedTasks["tasks"];
    
    // Build task dependency graph
    QJsonArray tasks = parsedTasks["tasks"].toArray();
    QJsonObject taskGraph = buildTaskGraph(tasks);
    masterPlan["task_graph"] = taskGraph;
    
    // Calculate task priorities
    QJsonObject priorities = calculateTaskPriorities(taskGraph);
    masterPlan["task_priorities"] = priorities;
    
    // Generate execution workflow
    QJsonArray workflow = generateExecutionWorkflow(masterPlan);
    masterPlan["execution_workflow"] = workflow;
    
    // Resource estimation
    QJsonObject resourceEstimate = estimateResources(masterPlan);
    masterPlan["resource_estimate"] = resourceEstimate;
    
    // Timeline estimation
    QString timeline = estimateTimeline(masterPlan);
    masterPlan["estimated_timeline"] = timeline;
    
    // Store in history
    m_planningHistory[masterPlan["id"].toString()] = masterPlan;
    
    emit planCreated(masterPlan);
    return masterPlan;
}

QJsonObject AdvancedPlanningEngine::decomposeTaskRecursive(const QString& task, int depth) {
    qDebug() << "[AdvancedPlanningEngine] Decomposing task at depth" << depth << ":" << task;
    
    QJsonObject decomposed;
    decomposed["task"] = task;
    decomposed["depth"] = depth;
    decomposed["id"] = QUuid::createUuid().toString();
    
    if (depth >= m_maxRecursionDepth) {
        decomposed["is_leaf"] = true;
        decomposed["subtasks"] = QJsonArray();
        return decomposed;
    }
    
    // Analyze task complexity to determine decomposition strategy
    QJsonObject complexity = analyzeComplexity(task);
    double complexityScore = complexity["score"].toDouble();
    
    if (complexityScore < 2.0) {
        // Simple task - no further decomposition needed
        decomposed["is_leaf"] = true;
        decomposed["subtasks"] = QJsonArray();
    } else {
        // Complex task - generate subtasks
        QJsonObject subtasks = generateSubtasks(task, depth);
        decomposed["is_leaf"] = false;
        decomposed["subtasks"] = subtasks["items"].toArray();
        
        // Recursively decompose each subtask
        QJsonArray refinedSubtasks;
        for (const QJsonValue& subtaskVal : decomposed["subtasks"].toArray()) {
            QJsonObject subtask = subtaskVal.toObject();
            QString subtaskDesc = subtask["description"].toString();
            QJsonObject refined = decomposeTaskRecursive(subtaskDesc, depth + 1);
            refinedSubtasks.append(refined);
        }
        decomposed["subtasks"] = refinedSubtasks;
    }
    
    // Add execution metadata
    QJsonArray dependencies = detectDependencies(decomposed);
    decomposed["dependencies"] = dependencies;
    decomposed["estimated_effort"] = complexity["effort_hours"];
    
    QJsonArray subtasksArray = decomposed["subtasks"].toArray();
    emit taskDecomposed(task, subtasksArray);
    return decomposed;
}

QJsonArray AdvancedPlanningEngine::generateExecutionWorkflow(const QJsonObject& plan) {
    qDebug() << "[AdvancedPlanningEngine] Generating execution workflow";
    
    QJsonArray workflow;
    
    // Extract task graph from plan
    QJsonObject taskGraph = plan["task_graph"].toObject();
    QJsonObject priorities = plan["task_priorities"].toObject();
    
    // Topological sort based on dependencies and priorities
    QJsonArray sortedTasks;
    
    // Phase 1: Independent tasks that can run in parallel
    QJsonArray independentTasks;
    for (auto it = taskGraph.begin(); it != taskGraph.end(); ++it) {
        QString taskId = it.key();
        QJsonObject task = it.value().toObject();
        if (task["dependencies"].toArray().isEmpty()) {
            independentTasks.append(taskId);
        }
    }
    
    // Sort independent tasks by priority
    QJsonArray sortedIndependent;
    for (const QJsonValue& taskIdVal : independentTasks) {
        QString taskId = taskIdVal.toString();
        int priority = priorities[taskId].toInt(0);
        // Insert in priority order
        int insertIndex = 0;
        for (int i = 0; i < sortedIndependent.size(); ++i) {
            QString existingId = sortedIndependent[i].toString();
            int existingPriority = priorities[existingId].toInt(0);
            if (priority > existingPriority) {
                break;
            }
            insertIndex = i + 1;
        }
        sortedIndependent.insert(insertIndex, taskId);
    }
    
    // Add parallel phase
    QJsonObject parallelPhase;
    parallelPhase["phase"] = "parallel";
    parallelPhase["tasks"] = sortedIndependent;
    parallelPhase["max_parallel"] = m_resourceLimits["max_parallel_tasks"].toInt();
    workflow.append(parallelPhase);
    
    // Phase 2: Dependent tasks (sequential execution)
    QJsonArray dependentTasks;
    for (auto it = taskGraph.begin(); it != taskGraph.end(); ++it) {
        QString taskId = it.key();
        QJsonObject task = it.value().toObject();
        if (!task["dependencies"].toArray().isEmpty()) {
            dependentTasks.append(taskId);
        }
    }
    
    // Sort dependent tasks by dependency order
    QJsonArray sortedDependent;
    QSet<QString> completed;
    bool progress = true;
    
    while (progress && !dependentTasks.isEmpty()) {
        progress = false;
        for (int i = 0; i < dependentTasks.size(); ) {
            QString taskId = dependentTasks[i].toString();
            QJsonObject task = taskGraph[taskId].toObject();
            QJsonArray deps = task["dependencies"].toArray();
            
            bool depsCompleted = true;
            for (const QJsonValue& depVal : deps) {
                QString depId = depVal.toString();
                if (!completed.contains(depId)) {
                    depsCompleted = false;
                    break;
                }
            }
            
            if (depsCompleted) {
                sortedDependent.append(taskId);
                completed.insert(taskId);
                dependentTasks.removeAt(i);
                progress = true;
            } else {
                ++i;
            }
        }
    }
    
    if (!sortedDependent.isEmpty()) {
        QJsonObject sequentialPhase;
        sequentialPhase["phase"] = "sequential";
        sequentialPhase["tasks"] = sortedDependent;
        workflow.append(sequentialPhase);
    }
    
    return workflow;
}

QJsonObject AdvancedPlanningEngine::optimizePlan(const QJsonObject& plan) {
    qDebug() << "[AdvancedPlanningEngine] Optimizing plan";
    
    QJsonObject optimized = plan;
    
    // Optimization 1: Merge similar sequential tasks
    QJsonArray workflow = optimized["execution_workflow"].toArray();
    for (int i = 0; i < workflow.size(); ++i) {
        QJsonObject phase = workflow[i].toObject();
        if (phase["phase"].toString() == "sequential") {
            QJsonArray tasks = phase["tasks"].toArray();
            QJsonArray mergedTasks;
            
            for (int j = 0; j < tasks.size(); ++j) {
                QString taskId = tasks[j].toString();
                QJsonObject taskGraph = optimized["task_graph"].toObject();
                QJsonObject task = taskGraph[taskId].toObject();
                
                // Look for merge opportunities
                if (j < tasks.size() - 1) {
                    QString nextTaskId = tasks[j + 1].toString();
                    QJsonObject nextTask = taskGraph[nextTaskId].toObject();
                    
                    // Check if tasks can be merged (same category, no dependencies)
                    if (task["category"].toString() == nextTask["category"].toString() &&
                        task["dependencies"].toArray().isEmpty() &&
                        nextTask["dependencies"].toArray().isEmpty()) {
                        
                        // Merge tasks
                        QJsonObject merged;
                        merged["id"] = taskId + "_" + nextTaskId;
                        merged["description"] = task["description"].toString() + " + " + nextTask["description"].toString();
                        merged["category"] = task["category"];
                        merged["estimated_effort"] = task["estimated_effort"].toDouble() + nextTask["estimated_effort"].toDouble();
                        merged["dependencies"] = QJsonArray();
                        mergedTasks.append(merged);
                        ++j; // Skip next task
                        continue;
                    }
                }
                
                mergedTasks.append(task);
            }
            
            phase["tasks"] = mergedTasks;
            workflow[i] = phase;
        }
    }
    
    optimized["execution_workflow"] = workflow;
    optimized["optimization_applied"] = true;
    optimized["optimization_timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    
    emit planOptimized(optimized);
    return optimized;
}

QJsonObject AdvancedPlanningEngine::analyzeComplexity(const QString& task) {
    qDebug() << "[AdvancedPlanningEngine] Analyzing complexity for:" << task;
    
    QJsonObject analysis;
    
    // Score based on task characteristics
    double score = 1.0;
    QString category = "general";
    
    if (task.contains("generate", Qt::CaseInsensitive) || task.contains("create", Qt::CaseInsensitive)) {
        score *= m_complexityWeights["code_generation"].toDouble();
        category = "code_generation";
    } else if (task.contains("refactor", Qt::CaseInsensitive) || task.contains("improve", Qt::CaseInsensitive)) {
        score *= m_complexityWeights["refactoring"].toDouble();
        category = "refactoring";
    } else if (task.contains("test", Qt::CaseInsensitive)) {
        score *= m_complexityWeights["testing"].toDouble();
        category = "testing";
    } else if (task.contains("debug", Qt::CaseInsensitive) || task.contains("fix", Qt::CaseInsensitive)) {
        score *= m_complexityWeights["debugging"].toDouble();
        category = "debugging";
    } else if (task.contains("optimize", Qt::CaseInsensitive) || task.contains("performance", Qt::CaseInsensitive)) {
        score *= m_complexityWeights["optimization"].toDouble();
        category = "optimization";
    } else if (task.contains("document", Qt::CaseInsensitive) || task.contains("explain", Qt::CaseInsensitive)) {
        score *= m_complexityWeights["documentation"].toDouble();
        category = "documentation";
    }
    
    // Adjust based on task scope
    if (task.contains("system", Qt::CaseInsensitive) || task.contains("architecture", Qt::CaseInsensitive)) {
        score *= 1.5;
    }
    if (task.contains("entire", Qt::CaseInsensitive) || task.contains("full", Qt::CaseInsensitive)) {
        score *= 1.3;
    }
    
    analysis["score"] = score;
    analysis["category"] = category;
    analysis["effort_hours"] = score * 2.0; // Base effort estimation
    analysis["difficulty"] = score > 3.0 ? "high" : (score > 2.0 ? "medium" : "low");
    
    return analysis;
}

QJsonArray AdvancedPlanningEngine::detectDependencies(const QJsonObject& task) {
    QJsonArray dependencies;
    
    // Analyze task for implicit dependencies
    QString taskDesc = task["task"].toString();
    
    if (taskDesc.contains("compile", Qt::CaseInsensitive) || taskDesc.contains("build", Qt::CaseInsensitive)) {
        dependencies.append("code_generation");
        dependencies.append("setup_environment");
    }
    
    if (taskDesc.contains("test", Qt::CaseInsensitive)) {
        dependencies.append("code_generation");
    }
    
    if (taskDesc.contains("deploy", Qt::CaseInsensitive) || taskDesc.contains("release", Qt::CaseInsensitive)) {
        dependencies.append("testing");
        dependencies.append("optimization");
    }
    
    return dependencies;
}

QJsonObject AdvancedPlanningEngine::estimateResources(const QJsonObject& task) {
    QJsonObject estimate;
    
    // Base resource requirements
    estimate["cpu_cores"] = 1;
    estimate["memory_mb"] = 512;
    estimate["disk_gb"] = 1.0;
    estimate["network_mb"] = 0.0;
    
    // Adjust based on task complexity
    QString complexity = task["complexity"].toObject()["difficulty"].toString();
    if (complexity == "high") {
        estimate["cpu_cores"] = 4;
        estimate["memory_mb"] = 2048;
    } else if (complexity == "medium") {
        estimate["cpu_cores"] = 2;
        estimate["memory_mb"] = 1024;
    }
    
    // Adjust based on task type
    QString taskDesc = task["task"].toString();
    if (taskDesc.contains("compile", Qt::CaseInsensitive)) {
        estimate["cpu_cores"] = estimate["cpu_cores"].toInt() * 2;
    }
    if (taskDesc.contains("model", Qt::CaseInsensitive) || taskDesc.contains("AI", Qt::CaseInsensitive)) {
        estimate["memory_mb"] = estimate["memory_mb"].toInt() * 2;
    }
    
    return estimate;
}

QString AdvancedPlanningEngine::estimateTimeline(const QJsonObject& plan) {
    QJsonObject resourceEstimate = plan["resource_estimate"].toObject();
    QJsonArray workflow = plan["execution_workflow"].toArray();
    
    double totalHours = 0.0;
    
    for (const QJsonValue& phaseVal : workflow) {
        QJsonObject phase = phaseVal.toObject();
        QJsonArray tasks = phase["tasks"].toArray();
        
        if (phase["phase"].toString() == "parallel") {
            // Parallel tasks - take the maximum time
            double maxTime = 0.0;
            for (const QJsonValue& taskIdVal : tasks) {
                QString taskId = taskIdVal.toString();
                QJsonObject taskGraph = plan["task_graph"].toObject();
                QJsonObject task = taskGraph[taskId].toObject();
                double effort = task["estimated_effort"].toDouble();
                maxTime = qMax(maxTime, effort);
            }
            totalHours += maxTime;
        } else {
            // Sequential tasks - sum the times
            for (const QJsonValue& taskIdVal : tasks) {
                QString taskId = taskIdVal.toString();
                QJsonObject taskGraph = plan["task_graph"].toObject();
                QJsonObject task = taskGraph[taskId].toObject();
                double effort = task["estimated_effort"].toDouble();
                totalHours += effort;
            }
        }
    }
    
    // Convert to human-readable format
    if (totalHours < 1.0) {
        return QString("~%1 minutes").arg(int(totalHours * 60));
    } else if (totalHours < 8.0) {
        return QString("~%1 hours").arg(totalHours, 0, 'f', 1);
    } else {
        double days = totalHours / 8.0;
        return QString("~%1 days").arg(days, 0, 'f', 1);
    }
}

void AdvancedPlanningEngine::adaptPlanFromFeedback(const QString& taskId, const QJsonObject& feedback) {
    qDebug() << "[AdvancedPlanningEngine] Adapting plan from feedback for task:" << taskId;
    
    // Update complexity weights based on actual performance
    QString outcome = feedback["outcome"].toString();
    double actualTime = feedback["actual_time_hours"].toDouble();
    double estimatedTime = feedback["estimated_time_hours"].toDouble();
    
    if (actualTime > estimatedTime * 1.5) {
        // Task took longer than expected - increase complexity weights for similar tasks
        QString category = feedback["category"].toString();
        double currentWeight = m_complexityWeights[category].toDouble();
        m_complexityWeights[category] = currentWeight * 1.1;
    } else if (actualTime < estimatedTime * 0.7) {
        // Task completed faster than expected - decrease complexity weights
        QString category = feedback["category"].toString();
        double currentWeight = m_complexityWeights[category].toDouble();
        m_complexityWeights[category] = qMax(0.5, currentWeight * 0.95);
    }
    
    // Update planning history
    if (m_planningHistory.contains(taskId)) {
        QJsonObject historyEntry = m_planningHistory[taskId].toObject();
        historyEntry["feedback_received"] = true;
        historyEntry["feedback"] = feedback;
        m_planningHistory[taskId] = historyEntry;
    }
}

QJsonObject AdvancedPlanningEngine::refinePlan(const QJsonObject& originalPlan, const QJsonObject& executionResults) {
    qDebug() << "[AdvancedPlanningEngine] Refining plan based on execution results";
    
    QJsonObject refined = originalPlan;
    
    // Analyze execution results
    bool success = executionResults["success"].toBool();
    QJsonArray completedTasks = executionResults["completed_tasks"].toArray();
    QJsonArray failedTasks = executionResults["failed_tasks"].toArray();
    
    if (!success) {
        // Plan failed - need to adjust
        refined["status"] = "failed";
        refined["failure_reason"] = executionResults["failure_reason"].toString();
        
        // Generate recovery plan
        QJsonObject recoveryPlan;
        recoveryPlan["type"] = "recovery";
        recoveryPlan["failed_tasks"] = failedTasks;
        recoveryPlan["strategy"] = "retry_with_alternatives";
        refined["recovery_plan"] = recoveryPlan;
    } else {
        // Plan succeeded - optimize for future
        refined["status"] = "completed";
        refined["actual_timeline"] = executionResults["actual_timeline"].toString();
        
        // Update complexity estimates based on actual performance
        for (const QJsonValue& taskVal : completedTasks) {
            QJsonObject task = taskVal.toObject();
            QString taskId = task["id"].toString();
            QString category = task["category"].toString();
            double actualEffort = task["actual_effort_hours"].toDouble();
            double estimatedEffort = task["estimated_effort_hours"].toDouble();
            
            // Adjust complexity weights
            double ratio = actualEffort / estimatedEffort;
            double currentWeight = m_complexityWeights[category].toDouble();
            m_complexityWeights[category] = currentWeight * (0.9 + ratio * 0.1);
        }
    }
    
    refined["refinement_timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    return refined;
}

// Helper method implementations
QJsonObject AdvancedPlanningEngine::parseGoalToTasks(const QString& goal) {
    QJsonObject result;
    QJsonArray tasks;
    
    // Simple task parsing based on keywords
    if (goal.contains("build", Qt::CaseInsensitive)) {
        tasks.append(QJsonObject{{"id", "setup_environment"}, {"description", "Setup build environment"}, {"category", "setup"}});
        tasks.append(QJsonObject{{"id", "configure_project"}, {"description", "Configure project settings"}, {"category", "configuration"}});
        tasks.append(QJsonObject{{"id", "compile_code"}, {"description", "Compile source code"}, {"category", "compilation"}});
    }
    
    if (goal.contains("test", Qt::CaseInsensitive)) {
        tasks.append(QJsonObject{{"id", "run_unit_tests"}, {"description", "Run unit tests"}, {"category", "testing"}});
        tasks.append(QJsonObject{{"id", "generate_coverage"}, {"description", "Generate test coverage report"}, {"category", "testing"}});
    }
    
    if (goal.contains("deploy", Qt::CaseInsensitive)) {
        tasks.append(QJsonObject{{"id", "package_application"}, {"description", "Package application for deployment"}, {"category", "packaging"}});
        tasks.append(QJsonObject{{"id", "deploy_to_server"}, {"description", "Deploy to target server"}, {"category", "deployment"}});
    }
    
    if (tasks.isEmpty()) {
        // Default generic tasks
        tasks.append(QJsonObject{{"id", "analyze_requirements"}, {"description", "Analyze requirements and constraints"}, {"category", "analysis"}});
        tasks.append(QJsonObject{{"id", "design_solution"}, {"description", "Design solution approach"}, {"category", "design"}});
        tasks.append(QJsonObject{{"id", "implement_solution"}, {"description", "Implement the solution"}, {"category", "implementation"}});
        tasks.append(QJsonObject{{"id", "validate_results"}, {"description", "Validate and test results"}, {"category", "validation"}});
    }
    
    result["tasks"] = tasks;
    return result;
}

QJsonObject AdvancedPlanningEngine::buildTaskGraph(const QJsonArray& tasks) {
    QJsonObject graph;
    
    for (const QJsonValue& taskVal : tasks) {
        QJsonObject task = taskVal.toObject();
        QString taskId = task["id"].toString();
        
        // Add basic task info
        QJsonObject taskInfo;
        taskInfo["description"] = task["description"];
        taskInfo["category"] = task["category"];
        taskInfo["dependencies"] = QJsonArray();
        
        // Analyze dependencies
        QString category = task["category"].toString();
        if (category == "compilation") {
            QJsonArray deps;
            deps.append("setup_environment");
            deps.append("configure_project");
            taskInfo["dependencies"] = deps;
        } else if (category == "testing") {
            QJsonArray deps;
            deps.append("compile_code");
            taskInfo["dependencies"] = deps;
        } else if (category == "deployment") {
            QJsonArray deps;
            deps.append("run_unit_tests");
            deps.append("package_application");
            taskInfo["dependencies"] = deps;
        }
        
        graph[taskId] = taskInfo;
    }
    
    return graph;
}

QJsonObject AdvancedPlanningEngine::calculateTaskPriorities(const QJsonObject& taskGraph) {
    QJsonObject priorities;
    
    for (auto it = taskGraph.begin(); it != taskGraph.end(); ++it) {
        QString taskId = it.key();
        QJsonObject task = it.value().toObject();
        QString category = task["category"].toString();
        
        // Assign priority based on category
        int priority = 1;
        if (category == "setup" || category == "configuration") priority = 10;
        else if (category == "analysis" || category == "design") priority = 9;
        else if (category == "implementation") priority = 8;
        else if (category == "compilation" || category == "testing") priority = 7;
        else if (category == "packaging" || category == "deployment") priority = 6;
        else if (category == "validation") priority = 5;
        
        priorities[taskId] = priority;
    }
    
    return priorities;
}

QJsonObject AdvancedPlanningEngine::generateSubtasks(const QString& task, int depth) {
    QJsonObject result;
    QJsonArray subtasks;
    
    QString taskLower = task.toLower();
    
    if (taskLower.contains("generate") || taskLower.contains("create")) {
        subtasks.append(QJsonObject{{"id", "analyze_requirements"}, {"description", "Analyze requirements"}, {"category", "analysis"}});
        subtasks.append(QJsonObject{{"id", "design_structure"}, {"description", "Design structure"}, {"category", "design"}});
        subtasks.append(QJsonObject{{"id", "implement_code"}, {"description", "Implement code"}, {"category", "coding"}});
        subtasks.append(QJsonObject{{"id", "test_implementation"}, {"description", "Test implementation"}, {"category", "testing"}});
    } else if (taskLower.contains("refactor") || taskLower.contains("improve")) {
        subtasks.append(QJsonObject{{"id", "analyze_current_code"}, {"description", "Analyze current code"}, {"category", "analysis"}});
        subtasks.append(QJsonObject{{"id", "identify_issues"}, {"description", "Identify issues"}, {"category", "analysis"}});
        subtasks.append(QJsonObject{{"id", "apply_improvements"}, {"description", "Apply improvements"}, {"category", "refactoring"}});
        subtasks.append(QJsonObject{{"id", "verify_improvements"}, {"description", "Verify improvements"}, {"category", "validation"}});
    } else if (taskLower.contains("debug") || taskLower.contains("fix")) {
        subtasks.append(QJsonObject{{"id", "reproduce_issue"}, {"description", "Reproduce the issue"}, {"category", "debugging"}});
        subtasks.append(QJsonObject{{"id", "analyze_root_cause"}, {"description", "Analyze root cause"}, {"category", "analysis"}});
        subtasks.append(QJsonObject{{"id", "implement_fix"}, {"description", "Implement fix"}, {"category", "fixing"}});
        subtasks.append(QJsonObject{{"id", "verify_fix"}, {"description", "Verify fix"}, {"category", "validation"}});
    }
    
    result["items"] = subtasks;
    return result;
}

void AdvancedPlanningEngine::processPlanningRequest(const QString& goal) {
    QTimer::singleShot(0, this, [this, goal]() {
        QJsonObject plan = createMasterPlan(goal);
        emit planCreated(plan);
    });
}

void AdvancedPlanningEngine::updateExecutionStatus(const QString& taskId, const QString& status, const QJsonObject& metrics) {
    qDebug() << "[AdvancedPlanningEngine] Execution status update:" << taskId << status;
    
    // Find the active session and update progress
    for (auto& session : m_activeSessions) {
        if (session && session->masterPlan["task_graph"].toObject().contains(taskId)) {
            if (status == "completed") {
                session->completedTasks++;
                int progress = int((session->completedTasks * 100.0) / session->totalTasks);
                emit executionProgress(taskId, progress, status);
            }
            break;
        }
    }
}

QJsonObject AdvancedPlanningEngine::planMultiGoal(const QJsonArray& goals) {
    qDebug() << "[AdvancedPlanningEngine] Planning for multiple goals:" << goals.size();
    
    QJsonObject multiGoalPlan;
    multiGoalPlan["goal_count"] = goals.size();
    multiGoalPlan["id"] = QUuid::createUuid().toString();
    multiGoalPlan["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    
    QJsonArray individualPlans;
    for (const auto& goal : goals) {
        QString goalStr = goal.toString();
        QJsonObject plan = createMasterPlan(goalStr);
        individualPlans.append(plan);
    }
    
    multiGoalPlan["individual_plans"] = individualPlans;
    multiGoalPlan["merged_plan"] = mergeMultiGoalPlans(individualPlans);
    
    return multiGoalPlan;
}

QJsonObject AdvancedPlanningEngine::resolveGoalConflicts(const QJsonArray& plans) {
    qDebug() << "[AdvancedPlanningEngine] Resolving conflicts among" << plans.size() << "plans";
    
    QJsonObject conflictResolution;
    conflictResolution["original_plan_count"] = plans.size();
    conflictResolution["conflicts_detected"] = 0;
    
    QJsonArray resolvedPlans;
    QJsonArray conflicts;
    
    // Analyze resource conflicts
    for (int i = 0; i < plans.size(); ++i) {
        for (int j = i + 1; j < plans.size(); ++j) {
            QJsonObject conflict = detectConflict(plans[i].toObject(), plans[j].toObject());
            if (!conflict.isEmpty()) {
                conflicts.append(conflict);
                conflictResolution["conflicts_detected"] = conflicts.size();
            }
        }
    }
    
    conflictResolution["conflicts"] = conflicts;
    conflictResolution["resolution_strategy"] = "priority_based";
    
    return conflictResolution;
}

QString AdvancedPlanningEngine::prioritizeGoals(const QJsonArray& goals) {
    qDebug() << "[AdvancedPlanningEngine] Prioritizing" << goals.size() << "goals";
    
    QJsonArray priorities;
    
    for (const auto& goal : goals) {
        QString goalStr = goal.toString();
        QJsonObject complexity = analyzeComplexity(goalStr);
        
        QJsonObject goalPriority;
        goalPriority["goal"] = goalStr;
        goalPriority["complexity"] = complexity["score"];
        goalPriority["priority"] = (complexity["score"].toDouble() > 3.0) ? "high" : "normal";
        goalPriority["estimated_duration"] = estimateTimeline(goalPriority);
        
        priorities.append(goalPriority);
    }
    
    // Sort by priority (manual sort, no std::sort on QJsonArray)
    bool swapped = true;
    while (swapped) {
        swapped = false;
        for (int i = 0; i < priorities.size() - 1; ++i) {
            QString priorityA = priorities[i].toObject()["priority"].toString();
            QString priorityB = priorities[i + 1].toObject()["priority"].toString();
            if (priorityA < priorityB) {  // "normal" < "high"
                priorities[i] = priorities[i + 1];
                priorities[i + 1] = priorities[i];
                swapped = true;
            }
        }
    }
    
    QJsonDocument doc(priorities);
    return doc.toJson(QJsonDocument::Compact);
}

QJsonObject AdvancedPlanningEngine::mergeMultiGoalPlans(const QJsonArray& plans) {
    QJsonObject merged;
    merged["type"] = "merged_multi_goal_plan";
    merged["merged_plans_count"] = plans.size();
    
    QJsonArray allTasks;
    for (const auto& plan : plans) {
        QJsonObject planObj = plan.toObject();
        if (planObj.contains("high_level_tasks")) {
            QJsonArray tasks = planObj["high_level_tasks"].toArray();
            for (const auto& task : tasks) {
                allTasks.append(task);
            }
        }
    }
    
    merged["all_tasks"] = allTasks;
    merged["total_tasks"] = allTasks.size();
    
    return merged;
}

QJsonObject AdvancedPlanningEngine::detectConflict(const QJsonObject& plan1, const QJsonObject& plan2) {
    QJsonObject conflict;
    
    // Check for resource conflicts
    QJsonObject res1 = plan1.value("resource_estimate").toObject();
    QJsonObject res2 = plan2.value("resource_estimate").toObject();
    
    if (!res1.isEmpty() && !res2.isEmpty()) {
        double total1 = res1["memory_mb"].toDouble() + res1["cpu_cores"].toDouble();
        double total2 = res2["memory_mb"].toDouble() + res2["cpu_cores"].toDouble();
        
        if (total1 + total2 > 8000) {
            conflict["type"] = "resource_conflict";
            conflict["severity"] = "high";
            return conflict;
        }
    }
    
    return QJsonObject(); // No conflict
}

void AdvancedPlanningEngine::initialize(class AgenticExecutor* executor, class InferenceEngine* inference) {
    qDebug() << "[AdvancedPlanningEngine] Initialized with external systems";
}

QJsonObject AdvancedPlanningEngine::getPerformanceMetrics() {
    QJsonObject metrics;
    metrics["planning_sessions"] = m_activeSessions.size();
    metrics["pending_plans"] = m_pendingPlans.size();
    metrics["history_entries"] = m_planningHistory.size();
    return metrics;
}

void AdvancedPlanningEngine::loadConfiguration(const QJsonObject& config) {
    qDebug() << "[AdvancedPlanningEngine] Loading configuration";
    if (config.contains("maxRecursionDepth")) {
        m_maxRecursionDepth = config["maxRecursionDepth"].toInt();
    }
    if (config.contains("enableAdaptivePlanning")) {
        m_enableAdaptivePlanning = config["enableAdaptivePlanning"].toBool();
    }
}

void AdvancedPlanningEngine::executionCompleted(const QJsonObject& result) {
    qDebug() << "[AdvancedPlanningEngine] Execution completed with result:" << result["status"];
    emit executionCompleted(result["plan_id"].toString(), result["success"].toBool(), result);
}

void AdvancedPlanningEngine::bottleneckDetected(const QString& taskId) {
    qWarning() << "[AdvancedPlanningEngine] Bottleneck detected in task:" << taskId;
}
