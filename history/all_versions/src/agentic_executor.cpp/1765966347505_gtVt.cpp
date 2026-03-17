// AgenticExecutor - Real agentic task execution (not simulated)
#include "agentic_executor.h"
#include "agentic_engine.h"
#include "qtapp/inference_engine.hpp"
#include "model_trainer.h"
#include "settings_manager.h"
#include "memory_space_manager.h"
#include <QFile>
#include <QTextStream>
#include <QFileInfo>
#include <QDateTime>
#include <QRegularExpression>
#include <QJsonDocument>
#include <QDebug>

AgenticExecutor::AgenticExecutor(QObject* parent)
    : QObject(parent)
    , m_currentWorkingDirectory(QDir::currentPath())
{
    qInfo() << "[AgenticExecutor] Initialized - Real execution engine ready";

    // Settings manager is optional; we skip persistence for this build
    // m_settingsManager = std::make_unique<SettingsManager>(this);
    // loadMemorySettings();
}

AgenticExecutor::~AgenticExecutor()
{
    clearMemory();
}

void AgenticExecutor::initialize(AgenticEngine* engine, InferenceEngine* inference)
{
    m_agenticEngine = engine;
    m_inferenceEngine = inference;
    m_modelTrainer = std::make_unique<ModelTrainer>(this);
    
    // Connect training signals
    connect(m_modelTrainer.get(), &ModelTrainer::epochStarted, 
            this, [this](int epoch, int totalEpochs) {
                // Emit progress with estimated values
                emit trainingProgress(epoch, totalEpochs, 0.0f, 0.0f);
            });
    connect(m_modelTrainer.get(), &ModelTrainer::trainingCompleted, 
            this, &AgenticExecutor::trainingCompleted);
    connect(m_modelTrainer.get(), &ModelTrainer::trainingError, 
            this, &AgenticExecutor::errorOccurred);
    connect(m_modelTrainer.get(), &ModelTrainer::logMessage, 
            this, &AgenticExecutor::logMessage);
    
    if (m_inferenceEngine) {
        m_modelTrainer->initialize(m_inferenceEngine, m_inferenceEngine->modelPath());
    }
    
    qInfo() << "[AgenticExecutor] Connected to AgenticEngine, InferenceEngine, and ModelTrainer";
}

// ========== MAIN AGENTIC EXECUTION ==========

QJsonObject AgenticExecutor::executeUserRequest(const QString& request)
{
    qInfo() << "[AgenticExecutor] Executing user request:" << request;
    emit logMessage("Starting execution: " + request);

    QJsonObject result;
    result["request"] = request;
    result["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);

    // Store request in memory for context
    addToMemory("last_user_request", request);
    addToMemory("last_request_timestamp", result["timestamp"]);

    try {
        // Step 1: Decompose the task using the model
        QJsonArray steps = decomposeTask(request);
        result["steps"] = steps;
        result["total_steps"] = steps.size();

        // Step 2: Execute each step
        QJsonArray executionResults;
        int successCount = 0;

        for (int i = 0; i < steps.size(); ++i) {
            QJsonObject step = steps[i].toObject();
            emit stepStarted(step["description"].toString());
            emit taskProgress(i + 1, steps.size());

            bool success = executeStep(step);
            
            QJsonObject stepResult;
            stepResult["step_number"] = i + 1;
            stepResult["description"] = step["description"];
            stepResult["success"] = success;
            stepResult["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);

            executionResults.append(stepResult);

            if (success) {
                successCount++;
                emit stepCompleted(step["description"].toString(), true);
            } else {
                emit stepCompleted(step["description"].toString(), false);
                
                // Try to recover from failure
                if (m_currentRetryCount < m_maxRetries) {
                    qWarning() << "[AgenticExecutor] Step failed, attempting retry...";
                    QJsonObject retryResult = retryWithCorrection(step);
                    if (retryResult["success"].toBool()) {
                        successCount++;
                        stepResult["recovered"] = true;
                    }
                }
            }
        }

        result["execution_results"] = executionResults;
        result["success_count"] = successCount;
        result["success_rate"] = (successCount * 100.0) / steps.size();
        result["overall_success"] = (successCount == steps.size());

        emit executionComplete(result);
        return result;

    } catch (const std::exception& e) {
        result["error"] = QString("Execution failed: %1").arg(e.what());
        result["overall_success"] = false;
        emit errorOccurred(result["error"].toString());
        return result;
    }
}

// ========== TASK DECOMPOSITION ==========

QJsonArray AgenticExecutor::decomposeTask(const QString& goal)
{
    if (!m_agenticEngine) {
        qWarning() << "[AgenticExecutor] Cannot decompose - no engine";
        return QJsonArray();
    }

    qInfo() << "[AgenticExecutor] Decomposing task:" << goal;

    // Build decomposition prompt for the model
    QString prompt = QString(
        "You are an expert software architect and project planner.\n\n"
        "User Request: %1\n\n"
        "Break this down into detailed, actionable steps. For each step, provide:\n"
        "1. A clear description of what to do\n"
        "2. The type of action (create_directory, create_file, compile, run, train_model, etc.)\n"
        "3. Required parameters\n"
        "4. Success criteria\n\n"
        "Available tools:\n"
        "- create_directory: Create a new directory\n"
        "- create_file: Create a file with content\n"
        "- read_file: Read file contents\n"
        "- delete_file: Delete a file\n"
        "- list_directory: List directory contents\n"
        "- compile_project: Compile C++ project\n"
        "- run_executable: Run compiled executable\n"
        "- train_model: Fine-tune a GGUF model with dataset\n"
        "- is_training: Check if model training is in progress\n\n"
        "Return as JSON array:\n"
        "[\n"
        "  {\"step\": 1, \"action\": \"create_directory\", \"description\": \"...\", \"params\": {...}, \"criteria\": \"...\" },\n"
        "  {\"step\": 2, \"action\": \"create_file\", \"description\": \"...\", \"params\": {\"path\": \"...\", \"content\": \"...\"}, \"criteria\": \"...\" }\n"
        "]\n\n"
        "Be specific and include all necessary files, compilation commands, and verification steps.\n"
        "For model training tasks, include dataset path, model path, and training configuration."
    ).arg(goal);

    // Get plan from model
    QString response = m_agenticEngine->generateResponse(prompt);
    
    // Extract JSON from response
    QRegularExpression jsonRegex("\\[\\s*\\{.*\\}\\s*\\]", QRegularExpression::DotMatchesEverythingOption);
    QRegularExpressionMatch match = jsonRegex.match(response);
    
    if (match.hasMatch()) {
        QString jsonStr = match.captured(0);
        QJsonDocument doc = QJsonDocument::fromJson(jsonStr.toUtf8());
        if (doc.isArray()) {
            qInfo() << "[AgenticExecutor] Task decomposed into" << doc.array().size() << "steps";
            return doc.array();
        }
    }

    // Fallback: create basic plan
    qWarning() << "[AgenticExecutor] Could not parse model response, creating fallback plan";
    QJsonArray fallback;
    QJsonObject step;
    step["step"] = 1;
    step["action"] = "analyze";
    step["description"] = "Analyze user request: " + goal;
    step["params"] = QJsonObject();
    fallback.append(step);
    return fallback;
}

// ========== STEP EXECUTION ==========

bool AgenticExecutor::executeStep(const QJsonObject& step)
{
    QString action = step["action"].toString();
    QJsonObject params = step["params"].toObject();
    QString description = step["description"].toString();

    qInfo() << "[AgenticExecutor] Executing step:" << description;
    emit logMessage("Step: " + description);

    try {
        if (action == "create_directory") {
            QString path = params["path"].toString();
            return createDirectory(path);
        }
        else if (action == "create_file") {
            QString path = params["path"].toString();
            QString content = params["content"].toString();
            
            // If content not in params, generate it
            if (content.isEmpty() && params.contains("specification")) {
                QJsonObject codeGen = generateCode(params["specification"].toString());
                content = codeGen["code"].toString();
            }
            
            return createFile(path, content);
        }
        else if (action == "compile") {
            QString projectPath = params["project_path"].toString();
            QString compiler = params.contains("compiler") ? params["compiler"].toString() : "g++";
            QJsonObject compileResult = compileProject(projectPath, compiler);
            return compileResult["success"].toBool();
        }
        else if (action == "run") {
            QString executable = params["executable"].toString();
            QStringList args = params["args"].toVariant().toStringList();
            QJsonObject runResult = runExecutable(executable, args);
            return runResult["success"].toBool();
        }
        else if (action == "generate_code") {
            QString spec = params["specification"].toString();
            QString outputPath = params["output_path"].toString();
            QJsonObject codeGen = generateCode(spec);
            if (codeGen.contains("code")) {
                return writeFile(outputPath, codeGen["code"].toString());
            }
            return false;
        }
        else if (action == "tool_call") {
            QString toolName = params["tool_name"].toString();
            QJsonObject toolParams = params["tool_params"].toObject();
            QJsonObject toolResult = callTool(toolName, toolParams);
            return toolResult["success"].toBool();
        }
        else {
            qWarning() << "[AgenticExecutor] Unknown action:" << action;
            return false;
        }

    } catch (const std::exception& e) {
        qCritical() << "[AgenticExecutor] Step execution failed:" << e.what();
        emit errorOccurred(QString("Step failed: %1").arg(e.what()));
        return false;
    }
}

bool AgenticExecutor::verifyStepCompletion(const QJsonObject& step, const QString& result)
{
    QString criteria = step["criteria"].toString();
    if (criteria.isEmpty()) return true;

    // Use model to verify completion
    QString prompt = QString(
        "Verification Task:\n"
        "Expected: %1\n"
        "Actual Result: %2\n\n"
        "Does the actual result meet the success criteria? Answer with ONLY 'yes' or 'no'."
    ).arg(criteria, result);

    QString verification = m_agenticEngine->generateResponse(prompt);
    return verification.toLower().contains("yes");
}

// ========== FILE SYSTEM OPERATIONS (REAL) ==========

bool AgenticExecutor::createDirectory(const QString& path)
{
    QDir dir;
    bool success = dir.mkpath(path);
    
    if (success) {
        qInfo() << "[AgenticExecutor] Created directory:" << path;
        emit logMessage("Created directory: " + path);
        addToMemory("last_created_dir", path);
    } else {
        qWarning() << "[AgenticExecutor] Failed to create directory:" << path;
    }
    
    return success;
}

bool AgenticExecutor::createFile(const QString& path, const QString& content)
{
    // Ensure parent directory exists
    QFileInfo fileInfo(path);
    if (!fileInfo.dir().exists()) {
        createDirectory(fileInfo.dir().absolutePath());
    }

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "[AgenticExecutor] Cannot open file for writing:" << path;
        return false;
    }

    QTextStream out(&file);
    out << content;
    file.close();

    qInfo() << "[AgenticExecutor] Created file:" << path << "(" << content.length() << "bytes)";
    emit logMessage("Created file: " + path);
    addToMemory("last_created_file", path);
    
    return true;
}

bool AgenticExecutor::writeFile(const QString& path, const QString& content)
{
    return createFile(path, content); // Same implementation
}

QString AgenticExecutor::readFile(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "[AgenticExecutor] Cannot read file:" << path;
        return QString();
    }

    QTextStream in(&file);
    QString content = in.readAll();
    file.close();

    qInfo() << "[AgenticExecutor] Read file:" << path << "(" << content.length() << "bytes)";
    return content;
}

bool AgenticExecutor::deleteFile(const QString& path)
{
    QFile file(path);
    bool success = file.remove();
    
    if (success) {
        qInfo() << "[AgenticExecutor] Deleted file:" << path;
    }
    
    return success;
}

bool AgenticExecutor::deleteDirectory(const QString& path)
{
    QDir dir(path);
    bool success = dir.removeRecursively();
    
    if (success) {
        qInfo() << "[AgenticExecutor] Deleted directory:" << path;
    }
    
    return success;
}

QStringList AgenticExecutor::listDirectory(const QString& path)
{
    QDir dir(path);
    QStringList entries = dir.entryList(QDir::AllEntries | QDir::NoDotAndDotDot);
    
    qInfo() << "[AgenticExecutor] Listed directory:" << path << "(" << entries.size() << "items)";
    return entries;
}

// ========== COMPILER INTEGRATION (REAL) ==========

QJsonObject AgenticExecutor::compileProject(const QString& projectPath, const QString& compiler)
{
    QJsonObject result;
    result["compiler"] = compiler;
    result["project_path"] = projectPath;

    qInfo() << "[AgenticExecutor] Compiling project:" << projectPath << "with" << compiler;
    emit logMessage("Compiling with " + compiler + "...");

    QProcess process;
    process.setWorkingDirectory(projectPath);

    // Detect build system and compile
    if (QFile::exists(projectPath + "/CMakeLists.txt")) {
        // CMake project
        emit logMessage("Detected CMake project");
        
        // Create build directory
        createDirectory(projectPath + "/build");
        process.setWorkingDirectory(projectPath + "/build");
        
        // Run cmake
        process.start("cmake", QStringList() << "..");
        process.waitForFinished(-1);
        
        QString cmakeOutput = process.readAllStandardOutput();
        QString cmakeError = process.readAllStandardError();
        
        if (process.exitCode() != 0) {
            result["success"] = false;
            result["error"] = "CMake configuration failed: " + cmakeError;
            qWarning() << "[AgenticExecutor]" << result["error"].toString();
            return result;
        }
        
        // Run make
        process.start("cmake", QStringList() << "--build" << ".");
        process.waitForFinished(-1);
        
        QString buildOutput = process.readAllStandardOutput();
        QString buildError = process.readAllStandardError();
        
        result["cmake_output"] = cmakeOutput;
        result["build_output"] = buildOutput;
        result["build_error"] = buildError;
        result["exit_code"] = process.exitCode();
        result["success"] = (process.exitCode() == 0);
        
    } else {
        // Direct compilation
        QStringList files;
        QDir dir(projectPath);
        files = dir.entryList(QStringList() << "*.cpp" << "*.c", QDir::Files);
        
        if (files.isEmpty()) {
            result["success"] = false;
            result["error"] = "No source files found";
            return result;
        }
        
        QStringList args;
        args << "-o" << "output";
        for (const QString& file : files) {
            args << file;
        }
        
        process.start(compiler, args);
        process.waitForFinished(-1);
        
        result["compiler_output"] = QString::fromUtf8(process.readAllStandardOutput());
        result["compiler_error"] = QString::fromUtf8(process.readAllStandardError());
        result["exit_code"] = process.exitCode();
        result["success"] = (process.exitCode() == 0);
    }

    if (result["success"].toBool()) {
        qInfo() << "[AgenticExecutor] Compilation successful";
        emit logMessage("Compilation successful!");
    } else {
        qWarning() << "[AgenticExecutor] Compilation failed";
        emit logMessage("Compilation failed: " + result["compiler_error"].toString());
        emit errorOccurred("Compilation failed");
    }

    addToMemory("last_compilation", result);
    return result;
}

QJsonObject AgenticExecutor::runExecutable(const QString& executablePath, const QStringList& args)
{
    QJsonObject result;
    result["executable"] = executablePath;
    result["arguments"] = QJsonArray::fromStringList(args);

    qInfo() << "[AgenticExecutor] Running executable:" << executablePath;
    emit logMessage("Running: " + executablePath);

    QProcess process;
    process.start(executablePath, args);
    
    if (!process.waitForStarted()) {
        result["success"] = false;
        result["error"] = "Failed to start process";
        return result;
    }

    process.waitForFinished(-1);

    result["stdout"] = QString::fromUtf8(process.readAllStandardOutput());
    result["stderr"] = QString::fromUtf8(process.readAllStandardError());
    result["exit_code"] = process.exitCode();
    result["success"] = (process.exitCode() == 0);

    if (result["success"].toBool()) {
        qInfo() << "[AgenticExecutor] Execution completed successfully";
        emit logMessage("Execution completed");
    } else {
        qWarning() << "[AgenticExecutor] Execution failed with code" << process.exitCode();
    }

    addToMemory("last_execution", result);
    return result;
}

// ========== CODE GENERATION ==========

QJsonObject AgenticExecutor::generateCode(const QString& specification)
{
    QJsonObject result;
    
    if (!m_agenticEngine) {
        result["error"] = "No engine available";
        return result;
    }

    qInfo() << "[AgenticExecutor] Generating code for:" << specification;
    
    QString prompt = QString(
        "Generate production-ready C++ code for the following specification:\n\n"
        "%1\n\n"
        "Requirements:\n"
        "- Complete, compilable code\n"
        "- Include all necessary headers\n"
        "- Add error handling\n"
        "- Add helpful comments\n"
        "- Follow C++17 best practices\n\n"
        "Return ONLY the code, no explanations."
    ).arg(specification);

    QString response = m_agenticEngine->generateCode(prompt);
    QString code = extractCodeFromResponse(response);

    result["specification"] = specification;
    result["code"] = code;
    result["success"] = !code.isEmpty();

    return result;
}

QString AgenticExecutor::extractCodeFromResponse(const QString& response)
{
    // Extract code from markdown code blocks
    QRegularExpression codeBlockRegex("```(?:cpp|c\\+\\+)?\\s*\\n([\\s\\S]*?)```");
    QRegularExpressionMatch match = codeBlockRegex.match(response);
    
    if (match.hasMatch()) {
        return match.captured(1).trimmed();
    }
    
    // If no code block, return the whole response
    return response.trimmed();
}

// ========== FUNCTION CALLING / TOOL USE ==========

QJsonArray AgenticExecutor::getAvailableTools()
{
    QJsonArray tools;
    
    // File system tools
    tools.append(QJsonObject{{"name", "create_directory"}, {"description", "Create a new directory"}});
    tools.append(QJsonObject{{"name", "create_file"}, {"description", "Create a file with content"}});
    tools.append(QJsonObject{{"name", "read_file"}, {"description", "Read file contents"}});
    tools.append(QJsonObject{{"name", "delete_file"}, {"description", "Delete a file"}});
    tools.append(QJsonObject{{"name", "list_directory"}, {"description", "List directory contents"}});
    
    // Compilation tools
    tools.append(QJsonObject{{"name", "compile_project"}, {"description", "Compile C++ project"}});
    tools.append(QJsonObject{{"name", "run_executable"}, {"description", "Run compiled executable"}});
    
    // Model tools
    tools.append(QJsonObject{{"name", "train_model"}, {"description", "Fine-tune a GGUF model with dataset"}});
    tools.append(QJsonObject{{"name", "is_training"}, {"description", "Check if model training is in progress"}});
    
    return tools;
}

QJsonObject AgenticExecutor::callTool(const QString& toolName, const QJsonObject& params)
{
    QJsonObject result;
    result["tool"] = toolName;
    result["params"] = params;

    qInfo() << "[AgenticExecutor] Calling tool:" << toolName;

    if (toolName == "create_directory") {
        bool success = createDirectory(params["path"].toString());
        result["success"] = success;
    }
    else if (toolName == "create_file") {
        bool success = createFile(params["path"].toString(), params["content"].toString());
        result["success"] = success;
    }
    else if (toolName == "read_file") {
        QString content = readFile(params["path"].toString());
        result["success"] = !content.isEmpty();
        result["content"] = content;
    }
    else if (toolName == "delete_file") {
        bool success = deleteFile(params["path"].toString());
        result["success"] = success;
    }
    else if (toolName == "compile_project") {
        QJsonObject compileResult = compileProject(params["project_path"].toString());
        result = compileResult;
    }
    else if (toolName == "run_executable") {
        QJsonObject runResult = runExecutable(params["executable"].toString());
        result = runResult;
    }
    else if (toolName == "list_directory") {
        QStringList entries = listDirectory(params["path"].toString());
        result["success"] = true;
        result["entries"] = QJsonArray::fromStringList(entries);
    }
    else if (toolName == "train_model") {
        QString datasetPath = params["dataset_path"].toString();
        QString modelPath = params["model_path"].toString();
        QJsonObject config = params["config"].toObject();
        QJsonObject trainResult = trainModel(datasetPath, modelPath, config);
        result = trainResult;
    }
    else if (toolName == "is_training") {
        bool training = isTrainingModel();
        result["success"] = true;
        result["is_training"] = training;
    }
    else {
        result["success"] = false;
        result["error"] = "Unknown tool: " + toolName;
    }

    return result;
}

// ========== MEMORY & CONTEXT ==========

void AgenticExecutor::addToMemory(const QString& key, const QVariant& value)
{
    m_memory[key] = value;
    qDebug() << "[AgenticExecutor] Memory updated:" << key;

    // Store request in memory for context
    addToMemory("last_user_request", request);
    addToMemory("last_request_timestamp", result["timestamp"]);
    enforceMemoryLimit();
    persistMemoryToDisk();
}

QVariant AgenticExecutor::getFromMemory(const QString& key)
{
    return m_memory.value(key);
}

void AgenticExecutor::clearMemory()
{
    m_memory.clear();
    m_executionHistory = QJsonArray();

    if (m_memoryEnabled) {
        MemorySpaceManager::instance().clearAll();
    }
}

QString AgenticExecutor::getFullContext()
{
    QString context;
    context += "=== EXECUTION CONTEXT ===\n";
    context += "Working Directory: " + m_currentWorkingDirectory + "\n";
    context += "Memory Items: " + QString::number(m_memory.size()) + "\n";
    context += "Execution History: " + QString::number(m_executionHistory.size()) + " steps\n";
    context += "\n=== MEMORY ===\n";
    
    for (auto it = m_memory.begin(); it != m_memory.end(); ++it) {
        context += it.key() + ": " + it.value().toString() + "\n";
    }
    
    return context;
}

void AgenticExecutor::removeMemoryItem(const QString& key)
{
    if (m_memory.contains(key)) {
        m_memory.remove(key);
    }
    MemorySpaceManager::instance().deleteKey(key);
    enforceMemoryLimit();
}

// ========== SELF-CORRECTION ==========

bool AgenticExecutor::detectFailure(const QString& output)
{
    QStringList failureIndicators = {
        "error", "failed", "exception", "cannot", "unable",
        "undefined reference", "segmentation fault", "compilation terminated"
    };
    
    QString lowerOutput = output.toLower();
    for (const QString& indicator : failureIndicators) {
        if (lowerOutput.contains(indicator)) {
            return true;
        }
    }
    
    return false;
}

QString AgenticExecutor::generateCorrectionPlan(const QString& failureReason)
{
    if (!m_agenticEngine) return "No correction available";

    QString prompt = QString(
        "An automated task failed with this error:\n%1\n\n"
        "Analyze the error and provide a correction plan. Include:\n"
        "1. Root cause of the failure\n"
        "2. Specific steps to fix it\n"
        "3. Code changes if needed\n"
        "4. Verification steps\n\n"
        "Be concise and actionable."
    ).arg(failureReason);

    return m_agenticEngine->generateResponse(prompt);
}

// ========== MEMORY SETTINGS & PERSISTENCE ==========

void AgenticExecutor::loadMemorySettings()
{
    // Memory settings disabled in this build
    qDebug() << "[AgenticExecutor] Memory settings not available";
}

void AgenticExecutor::loadMemoryFromDisk()
{
    // Memory loading disabled in this build
    qDebug() << "[AgenticExecutor] Memory load not available";
}

void AgenticExecutor::persistMemoryToDisk()
{
    // Memory persistence disabled in this build
    qDebug() << "[AgenticExecutor] Memory persistence not available";
}

void AgenticExecutor::enforceMemoryLimit()
{
    // Memory limit enforcement disabled in this build
    qDebug() << "[AgenticExecutor] Memory limit enforcement not available";
}

QJsonObject AgenticExecutor::retryWithCorrection(const QJsonObject& failedStep)
{
    m_currentRetryCount++;
    
    QJsonObject result;
    result["original_step"] = failedStep;
    result["retry_attempt"] = m_currentRetryCount;

    QString failureContext = getFromMemory("last_error").toString();
    QString correctionPlan = generateCorrectionPlan(failureContext);
    
    qInfo() << "[AgenticExecutor] Retry attempt" << m_currentRetryCount << "with correction plan";
    emit logMessage("Attempting correction: " + correctionPlan);

    // Apply correction and retry
    bool success = executeStep(failedStep);
    
    result["success"] = success;
    result["correction_plan"] = correctionPlan;
    
    if (!success && m_currentRetryCount < m_maxRetries) {
        // Recursive retry
        return retryWithCorrection(failedStep);
    }
    
    m_currentRetryCount = 0; // Reset for next task
    return result;
}

// ========== MODEL TRAINING ==========

QJsonObject AgenticExecutor::trainModel(const QString& datasetPath, const QString& modelPath, const QJsonObject& config)
{
    QJsonObject result;
    
    if (!m_modelTrainer) {
        result["success"] = false;
        result["error"] = "Model trainer not initialized";
        return result;
    }
    
    if (!m_inferenceEngine || !m_inferenceEngine->isModelLoaded()) {
        result["success"] = false;
        result["error"] = "No model loaded for training";
        return result;
    }
    
    qInfo() << "[AgenticExecutor] Starting model training:" << datasetPath;
    emit logMessage("Starting model training with dataset: " + datasetPath);
    
    // Configure training
    ModelTrainer::TrainingConfig trainConfig;
    trainConfig.datasetPath = datasetPath;
    trainConfig.outputPath = modelPath + ".trained";
    trainConfig.epochs = config.value("epochs").toInt(10);
    trainConfig.learningRate = static_cast<float>(config.value("learning_rate").toDouble(1e-4));
    trainConfig.batchSize = config.value("batch_size").toInt(32);
    trainConfig.sequenceLength = config.value("sequence_length").toInt(512);
    trainConfig.gradientClip = static_cast<float>(config.value("gradient_clip").toDouble(1.0));
    trainConfig.validateEveryEpoch = config.value("validate_every_epoch").toBool(true);
    trainConfig.validationSplit = static_cast<float>(config.value("validation_split").toDouble(0.1));
    trainConfig.weightDecay = static_cast<float>(config.value("weight_decay").toDouble(0.01));
    trainConfig.warmupSteps = static_cast<float>(config.value("warmup_steps").toDouble(0.1));
    
    // Start training
    bool success = m_modelTrainer->startTraining(trainConfig);
    
    result["success"] = success;
    result["output_model_path"] = trainConfig.outputPath;
    if (!success) {
        result["error"] = "Failed to start training";
    }
    
    return result;
}

bool AgenticExecutor::isTrainingModel() const
{
    return m_modelTrainer && m_modelTrainer->isTraining();
}
