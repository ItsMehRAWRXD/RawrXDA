/**
 * \file plan_orchestrator.cpp
 * \brief AI-driven multi-file edit coordinator implementation
 * \author RawrXD Team
 * \date 2025-12-07
 */

#include "plan_orchestrator.h"
#include "lsp_client.h"
#include <QFile>
#include <QTextStream>
#include <QDir>
#include <QDirIterator>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>
#include <QRegularExpression>
#include <QFileInfo>

namespace RawrXD {

PlanOrchestrator::PlanOrchestrator(QObject* parent)
    : QObject(parent)
{
    // Lightweight constructor - defer initialization
}

void PlanOrchestrator::initialize() {
    if (m_initialized) return;
    
    qDebug() << "[PlanOrchestrator] Initialized";
    m_initialized = true;
}

void PlanOrchestrator::setLSPClient(LSPClient* client) {
    m_lspClient = client;
}

void PlanOrchestrator::setInferenceEngine(InferenceEngine* engine) {
    m_inferenceEngine = engine;
}

void PlanOrchestrator::setWorkspaceRoot(const QString& root) {
    m_workspaceRoot = root;
    qDebug() << "[PlanOrchestrator] Workspace root set to:" << root;
}

PlanningResult PlanOrchestrator::generatePlan(const QString& prompt, 
                                               const QString& workspaceRoot,
                                               const QStringList& contextFiles) {
    emit planningStarted(prompt);
    
    PlanningResult result;
    
    if (!m_inferenceEngine) {
        result.errorMessage = "No inference engine available";
        emit errorOccurred(result.errorMessage);
        return result;
    }
    
    // Gather context files if not provided
    QStringList filesToAnalyze = contextFiles;
    if (filesToAnalyze.isEmpty()) {
        filesToAnalyze = gatherContextFiles(workspaceRoot);
    }
    
    // Build planning prompt with context
    QString planningPrompt = buildPlanningPrompt(prompt, filesToAnalyze);
    
    qDebug() << "[PlanOrchestrator] Generating plan for:" << prompt;
    qDebug() << "[PlanOrchestrator] Context files:" << filesToAnalyze.size();
    
    // Initialize result defaults (overwritten by inference engine below)
    result.success = true;
    result.planDescription = "Generated plan for: " + prompt;
    result.affectedFiles = filesToAnalyze.mid(0, 3);
    result.estimatedChanges = 5;
    
        // === REAL IMPLEMENTATION: Call inference engine for plan generation ===
    
        qInfo() << "[PlanOrchestrator] Calling InferenceEngine for plan generation";
    
        // Prepare structured prompt for the model
        QString structuredPrompt = QString(
            "You are a code refactoring assistant. Generate a structured plan for the following task:\n"
            "\n"
            "TASK: %1\n"
            "\n"
            "AFFECTED FILES:\n%2\n"
            "\n"
            "Return a JSON object with the following structure:\n"
            "{\n"
            "  \"success\": true,\n"
            "  \"planDescription\": \"Human-readable description of the plan\",\n"
            "  \"estimatedChanges\": <number>,\n"
            "  \"tasks\": [\n"
            "    {\n"
            "      \"filePath\": \"path/to/file.cpp\",\n"
            "      \"startLine\": 0,\n"
            "      \"endLine\": 0,\n"
            "      \"operation\": \"insert|replace|delete|rename\",\n"
            "      \"oldText\": \"text to replace (if replace/delete)\",\n"
            "      \"newText\": \"replacement text (if insert/replace)\",\n"
            "      \"description\": \"What this change does\",\n"
            "      \"priority\": 1\n"
            "    }\n"
            "  ]\n"
            "}\n"
        ).arg(prompt, filesToAnalyze.join("\n  - "));
    
        // Call the inference engine to generate the plan
        std::vector<int32_t> promptTokens = m_inferenceEngine->tokenize(structuredPrompt);
        qInfo() << "[PlanOrchestrator] Tokenized prompt:" << promptTokens.size() << "tokens";
    
        // Generate response with reasonable token limit for plan generation
        std::vector<int32_t> generatedTokens = m_inferenceEngine->generate(promptTokens, 1024);
        qInfo() << "[PlanOrchestrator] Generated response:" << generatedTokens.size() << "tokens";
    
        // Detokenize the response back to text
        QString planResponse = m_inferenceEngine->detokenize(generatedTokens);
        qInfo() << "[PlanOrchestrator] Plan response (first 200 chars):" << planResponse.left(200);
    
        // === PARSE THE JSON RESPONSE ===
        QJsonDocument jsonDoc;
    
        // Try to extract JSON from the response (model might include extra text)
        int jsonStart = planResponse.indexOf('{');
        int jsonEnd = planResponse.lastIndexOf('}');
    
        if (jsonStart != -1 && jsonEnd != -1 && jsonEnd > jsonStart) {
            QString jsonStr = planResponse.mid(jsonStart, jsonEnd - jsonStart + 1);
            QJsonParseError parseError;
            jsonDoc = QJsonDocument::fromJson(jsonStr.toUtf8(), &parseError);
        
            if (parseError.error != QJsonParseError::NoError) {
                qWarning() << "[PlanOrchestrator] JSON parse error:" << parseError.errorString();
                qWarning() << "[PlanOrchestrator] JSON attempted parse:" << jsonStr.left(500);
                result.errorMessage = "Failed to parse plan response: " + parseError.errorString();
                result.success = false;
                emit planningCompleted(result);
                return result;
            }
        } else {
            qWarning() << "[PlanOrchestrator] No JSON found in response";
            result.errorMessage = "Model response did not contain valid JSON";
            result.success = false;
            emit planningCompleted(result);
            return result;
        }
    
        // Extract fields from JSON response
        QJsonObject planObj = jsonDoc.object();
    
        result.planDescription = planObj.value("planDescription").toString("Plan generated by AI");
        result.estimatedChanges = planObj.value("estimatedChanges").toInt(0);
        result.success = planObj.value("success").toBool(false);
    
        // Extract and validate tasks array
        QJsonArray tasksArray = planObj.value("tasks").toArray();
    
        if (tasksArray.isEmpty()) {
            qWarning() << "[PlanOrchestrator] No tasks in plan response";
            result.errorMessage = "Plan contained no executable tasks";
            result.success = false;
            emit planningCompleted(result);
            return result;
        }
    
        // Parse each task from JSON
        QSet<QString> processedFiles;
        for (int i = 0; i < tasksArray.size(); ++i) {
            QJsonObject taskObj = tasksArray.at(i).toObject();
        
            EditTask task;
            task.filePath = taskObj.value("filePath").toString();
            task.startLine = taskObj.value("startLine").toInt(0);
            task.endLine = taskObj.value("endLine").toInt(0);
            task.operation = taskObj.value("operation").toString("insert");
            task.oldText = taskObj.value("oldText").toString();
            task.newText = taskObj.value("newText").toString();
            task.symbolName = taskObj.value("symbolName").toString();
            task.newSymbolName = taskObj.value("newSymbolName").toString();
            task.description = taskObj.value("description").toString(QString("Task %1").arg(i));
            task.priority = taskObj.value("priority").toInt(i);
        
            // Validate task has required fields
            if (task.filePath.isEmpty() || task.operation.isEmpty()) {
                qWarning() << "[PlanOrchestrator] Skipping invalid task:" << task.description;
                continue;
            }
        
            // Validate operation type
            QStringList validOps = {"insert", "replace", "delete", "rename"};
            if (!validOps.contains(task.operation)) {
                qWarning() << "[PlanOrchestrator] Invalid operation:" << task.operation;
                task.operation = "insert";  // Fallback
            }
        
            result.tasks.append(task);
            processedFiles.insert(task.filePath);
        
            qInfo() << "[PlanOrchestrator] Task" << i << ":" << task.operation 
                    << "in" << QFileInfo(task.filePath).fileName();
        }
    
        // Update affected files list from actual tasks
        result.affectedFiles = processedFiles.toList();
    
        // Validate plan has tasks
        if (result.tasks.isEmpty()) {
            qWarning() << "[PlanOrchestrator] Plan parsing resulted in zero valid tasks";
            result.errorMessage = "No valid tasks extracted from plan";
            result.success = false;
            emit planningCompleted(result);
            return result;
        }
    
        qInfo() << "[PlanOrchestrator] Successfully generated plan with"
                << result.tasks.size() << "tasks affecting" << result.affectedFiles.size() << "files";
    
    emit planningCompleted(result);
    return result;
}

ExecutionResult PlanOrchestrator::executePlan(const PlanningResult& plan, bool dryRun) {
    emit executionStarted(plan.tasks.size());
    
    ExecutionResult result;
    
    if (!plan.success) {
        result.errorMessage = "Invalid plan: " + plan.errorMessage;
        emit errorOccurred(result.errorMessage);
        return result;
    }
    
    // Backup original file contents for rollback
    m_originalFileContents.clear();
    for (const QString& filePath : plan.affectedFiles) {
        QString content = readFileContent(filePath);
        if (!content.isNull()) {
            m_originalFileContents[filePath] = content;
        }
    }
    
    // Sort tasks by priority (higher first)
    QVector<EditTask> sortedTasks = plan.tasks;
    std::sort(sortedTasks.begin(), sortedTasks.end(), 
              [](const EditTask& a, const EditTask& b) {
                  return a.priority > b.priority;
              });
    
    // Execute tasks
    int taskIndex = 0;
    for (const EditTask& task : sortedTasks) {
        qDebug() << "[PlanOrchestrator] Executing task" << taskIndex << ":" << task.description;
        
        bool success = executeTask(task, dryRun);
        
        if (success) {
            result.successCount++;
            if (!result.successfulFiles.contains(task.filePath)) {
                result.successfulFiles.append(task.filePath);
            }
        } else {
            result.failureCount++;
            if (!result.failedFiles.contains(task.filePath)) {
                result.failedFiles.append(task.filePath);
            }
        }
        
        emit taskExecuted(taskIndex, success, task.description);
        taskIndex++;
    }
    
    // Check overall success
    result.success = (result.failureCount == 0);
    
    // Rollback on failure (unless dry run)
    if (!result.success && !dryRun) {
        qWarning() << "[PlanOrchestrator] Execution failed, rolling back...";
        for (auto it = m_originalFileContents.begin(); it != m_originalFileContents.end(); ++it) {
            writeFileContent(it.key(), it.value());
        }
        result.errorMessage = QString("Execution failed with %1 errors, changes rolled back")
                                  .arg(result.failureCount);
    }
    
    emit executionCompleted(result);
    return result;
}

ExecutionResult PlanOrchestrator::planAndExecute(const QString& prompt,
                                                  const QString& workspaceRoot,
                                                  bool dryRun) {
    PlanningResult plan = generatePlan(prompt, workspaceRoot);
    
    if (!plan.success) {
        ExecutionResult result;
        result.errorMessage = "Planning failed: " + plan.errorMessage;
        return result;
    }
    
    return executePlan(plan, dryRun);
}

QString PlanOrchestrator::buildPlanningPrompt(const QString& userPrompt, 
                                               const QStringList& contextFiles) {
    QString prompt = "You are an AI code refactoring assistant. ";
    prompt += "Analyze the following codebase and generate a detailed edit plan ";
    prompt += "to accomplish this task:\n\n";
    prompt += "TASK: " + userPrompt + "\n\n";
    
    prompt += "CONTEXT FILES:\n";
    for (const QString& filePath : contextFiles) {
        QString content = readFileContent(filePath);
        if (!content.isEmpty()) {
            prompt += "\n=== " + filePath + " ===\n";
            prompt += content.left(2000);  // Limit to 2000 chars per file
            if (content.length() > 2000) {
                prompt += "\n... (truncated) ...\n";
            }
        }
    }
    
    prompt += "\n\nGenerate a JSON array of edit tasks with this structure:\n";
    prompt += "[\n";
    prompt += "  {\n";
    prompt += "    \"filePath\": \"absolute/path/to/file.cpp\",\n";
    prompt += "    \"startLine\": 0,\n";
    prompt += "    \"endLine\": 0,\n";
    prompt += "    \"operation\": \"replace|insert|delete|rename\",\n";
    prompt += "    \"oldText\": \"text to replace\",\n";
    prompt += "    \"newText\": \"replacement text\",\n";
    prompt += "    \"description\": \"Human-readable description\",\n";
    prompt += "    \"priority\": 1\n";
    prompt += "  }\n";
    prompt += "]\n";
    
    return prompt;
}

PlanningResult PlanOrchestrator::parsePlanningResponse(const QString& response) {
    PlanningResult result;
    
    // Try to parse JSON response
    QJsonDocument doc = QJsonDocument::fromJson(response.toUtf8());
    
    if (!doc.isArray()) {
        result.errorMessage = "Invalid JSON response (expected array)";
        return result;
    }
    
    QJsonArray tasksArray = doc.array();
    
    for (const QJsonValue& val : tasksArray) {
        if (!val.isObject()) continue;
        
        QJsonObject taskObj = val.toObject();
        
        EditTask task;
        task.filePath = taskObj["filePath"].toString();
        task.startLine = taskObj["startLine"].toInt();
        task.endLine = taskObj["endLine"].toInt();
        task.operation = taskObj["operation"].toString();
        task.oldText = taskObj["oldText"].toString();
        task.newText = taskObj["newText"].toString();
        task.symbolName = taskObj["symbolName"].toString();
        task.newSymbolName = taskObj["newSymbolName"].toString();
        task.description = taskObj["description"].toString();
        task.priority = taskObj["priority"].toInt(0);
        
        result.tasks.append(task);
        
        if (!result.affectedFiles.contains(task.filePath)) {
            result.affectedFiles.append(task.filePath);
        }
    }
    
    result.estimatedChanges = result.tasks.size();
    result.success = !result.tasks.isEmpty();
    
    return result;
}

bool PlanOrchestrator::executeTask(const EditTask& task, bool dryRun) {
    if (task.operation == "replace") {
        return applyReplace(task, dryRun);
    } else if (task.operation == "insert") {
        return applyInsert(task, dryRun);
    } else if (task.operation == "delete") {
        return applyDelete(task, dryRun);
    } else if (task.operation == "rename") {
        return applyRename(task, dryRun);
    } else {
        qWarning() << "[PlanOrchestrator] Unknown operation:" << task.operation;
        return false;
    }
}

bool PlanOrchestrator::applyReplace(const EditTask& task, bool dryRun) {
    qDebug() << "[PlanOrchestrator] Replace in" << task.filePath 
             << "lines" << task.startLine << "-" << task.endLine;
    
    if (dryRun) {
        qDebug() << "[PlanOrchestrator] DRY RUN: Would replace:" << task.oldText 
                 << "with:" << task.newText;
        return true;
    }
    
    QString content = readFileContent(task.filePath);
    if (content.isNull()) return false;
    
    // Simple replace (TODO: use line-based replacement)
    QString newContent = content.replace(task.oldText, task.newText);
    
    return writeFileContent(task.filePath, newContent);
}

bool PlanOrchestrator::applyInsert(const EditTask& task, bool dryRun) {
    qDebug() << "[PlanOrchestrator] Insert in" << task.filePath 
             << "at line" << task.startLine;
    
    if (dryRun) {
        qDebug() << "[PlanOrchestrator] DRY RUN: Would insert:" << task.newText;
        return true;
    }
    
    QString content = readFileContent(task.filePath);
    if (content.isNull()) return false;
    
    QStringList lines = content.split('\n');
    
    if (task.startLine < 0 || task.startLine > lines.size()) {
        qWarning() << "[PlanOrchestrator] Invalid line number:" << task.startLine;
        return false;
    }
    
    lines.insert(task.startLine, task.newText);
    
    return writeFileContent(task.filePath, lines.join('\n'));
}

bool PlanOrchestrator::applyDelete(const EditTask& task, bool dryRun) {
    qDebug() << "[PlanOrchestrator] Delete in" << task.filePath 
             << "lines" << task.startLine << "-" << task.endLine;
    
    if (dryRun) {
        qDebug() << "[PlanOrchestrator] DRY RUN: Would delete lines" 
                 << task.startLine << "-" << task.endLine;
        return true;
    }
    
    QString content = readFileContent(task.filePath);
    if (content.isNull()) return false;
    
    QStringList lines = content.split('\n');
    
    if (task.startLine < 0 || task.endLine >= lines.size()) {
        qWarning() << "[PlanOrchestrator] Invalid line range";
        return false;
    }
    
    // Remove lines from startLine to endLine (inclusive)
    for (int i = task.endLine; i >= task.startLine; --i) {
        lines.removeAt(i);
    }
    
    return writeFileContent(task.filePath, lines.join('\n'));
}

bool PlanOrchestrator::applyRename(const EditTask& task, bool dryRun) {
    qDebug() << "[PlanOrchestrator] Rename" << task.symbolName 
             << "to" << task.newSymbolName;
    
    if (task.symbolName.isEmpty() || task.newSymbolName.isEmpty()) {
        qWarning() << "[PlanOrchestrator] Rename requires both symbolName and newSymbolName";
        return false;
    }
    
    if (dryRun) {
        qDebug() << "[PlanOrchestrator] DRY RUN: Would rename" 
                 << task.symbolName << "to" << task.newSymbolName;
        return true;
    }
    
    // Attempt LSP-powered rename first (semantically aware)
    if (m_lspClient && m_lspClient->isRunning()) {
        qDebug() << "[PlanOrchestrator] Attempting LSP rename for" << task.symbolName;
        
        bool lspSuccess = m_lspClient->requestRename(
            task.filePath, task.startLine, 0, task.newSymbolName);
        
        if (lspSuccess) {
            qInfo() << "[PlanOrchestrator] LSP rename succeeded for" << task.symbolName;
            return true;
        }
        
        qWarning() << "[PlanOrchestrator] LSP rename failed, falling back to text-based rename";
    }
    
    // Fallback: Word-boundary-aware text replacement
    // Ensures we don't rename substrings (e.g., "foo" inside "fooBar")
    QString content = readFileContent(task.filePath);
    if (content.isNull()) return false;
    
    // Build a regex that matches the symbol at word boundaries
    QString escapedSymbol = QRegularExpression::escape(task.symbolName);
    QRegularExpression wordBoundaryRegex(
        QString("\\b%1\\b").arg(escapedSymbol));
    
    QString newContent = content;
    int replacements = 0;
    
    // Iterate matches in reverse to preserve positions
    QRegularExpressionMatchIterator it = wordBoundaryRegex.globalMatch(content);
    QVector<QPair<int, int>> matches; // offset, length
    while (it.hasNext()) {
        QRegularExpressionMatch match = it.next();
        matches.append({match.capturedStart(), match.capturedLength()});
    }
    
    // Apply replacements from end to start (preserves offsets)
    for (int i = matches.size() - 1; i >= 0; --i) {
        int offset = matches[i].first;
        int length = matches[i].second;
        newContent.replace(offset, length, task.newSymbolName);
        replacements++;
    }
    
    if (replacements == 0) {
        qWarning() << "[PlanOrchestrator] Symbol not found:" << task.symbolName;
        return false;
    }
    
    qInfo() << "[PlanOrchestrator] Renamed" << replacements << "occurrences of" 
            << task.symbolName << "to" << task.newSymbolName;
    
    return writeFileContent(task.filePath, newContent);
}

QStringList PlanOrchestrator::gatherContextFiles(const QString& workspaceRoot, int maxFiles) {
    QStringList files;
    
    QStringList nameFilters;
    nameFilters << "*.cpp" << "*.h" << "*.hpp" << "*.cc" << "*.cxx"
                << "*.py" << "*.js" << "*.ts" << "*.java" << "*.cs";
    
    QDirIterator it(workspaceRoot, nameFilters, QDir::Files, 
                    QDirIterator::Subdirectories);
    
    while (it.hasNext() && files.size() < maxFiles) {
        QString filePath = it.next();
        
        // Skip build directories
        if (filePath.contains("/build/") || filePath.contains("\\build\\")) {
            continue;
        }
        
        files.append(filePath);
    }
    
    qDebug() << "[PlanOrchestrator] Gathered" << files.size() << "context files";
    return files;
}

QString PlanOrchestrator::readFileContent(const QString& filePath) {
    QFile file(filePath);
    
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "[PlanOrchestrator] Failed to open file:" << filePath;
        return QString();
    }
    
    QTextStream in(&file);
    QString content = in.readAll();
    file.close();
    
    return content;
}

bool PlanOrchestrator::writeFileContent(const QString& filePath, const QString& content) {
    QFile file(filePath);
    
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "[PlanOrchestrator] Failed to write file:" << filePath;
        return false;
    }
    
    QTextStream out(&file);
    out << content;
    file.close();
    
    qDebug() << "[PlanOrchestrator] Wrote file:" << filePath;
    return true;
}

} // namespace RawrXD
