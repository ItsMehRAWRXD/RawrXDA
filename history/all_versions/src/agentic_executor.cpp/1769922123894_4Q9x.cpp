// AgenticExecutor - Real agentic task execution (not simulated)
#include "agentic_executor.h"
#include "agentic_engine.h"
#include "qtapp/inference_engine.hpp"
#include "model_trainer.h"
#include <fstream>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <regex>
#include <chrono>

using json = nlohmann::json;

AgenticExecutor::AgenticExecutor(void* parent)
    : m_currentWorkingDirectory(std::filesystem::path::currentPath())
{
    m_executionHistory = json::array();
}

AgenticExecutor::~AgenticExecutor()
{
    clearMemory();
}

void AgenticExecutor::initialize(AgenticEngine* engine, InferenceEngine* inference)
{
    m_agenticEngine = engine;
    m_inferenceEngine = inference;
    // Real training requires pointer to ModelTrainer, assumed to be properly initialized later
    m_modelTrainer = std::make_unique<ModelTrainer>(this);
    
    if (m_inferenceEngine) {
        m_modelTrainer->initialize(m_inferenceEngine, m_inferenceEngine->modelPath());
    }
}

// ========== MAIN AGENTIC EXECUTION ==========

json AgenticExecutor::executeUserRequest(const std::string& request)
{
    logMessage("Starting execution: " + request);

    json result;
    result["request"] = request;
    
    auto now = std::chrono::system_clock::now();
    result["timestamp"] = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();

    try {
        // Step 1: Decompose the task using the model
        json steps = decomposeTask(request);
        result["steps"] = steps;
        result["total_steps"] = steps.size();

        // Step 2: Execute each step
        json executionResults = json::array();
        int successCount = 0;

        for (size_t i = 0; i < steps.size(); ++i) {
            json step = steps[i];
            
            std::string desc = step.value("description", "Step " + std::to_string(i+1));
            stepStarted(desc);
            taskProgress(i + 1, steps.size());

            bool success = executeStep(step);
            
            json stepResult;
            stepResult["step_number"] = i + 1;
            stepResult["description"] = desc;
            stepResult["success"] = success;
            stepResult["timestamp"] = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();

            executionResults.push_back(stepResult);

            if (success) {
                successCount++;
                stepCompleted(desc, true);
            } else {
                stepCompleted(desc, false);
                
                // Try to recover from failure
                if (m_currentRetryCount < m_maxRetries) {
                    json retryResult = retryWithCorrection(step);
                    if (retryResult.value("success", false)) {
                        successCount++;
                        stepResult["recovered"] = true;
                    }
                }
            }
        }

        result["execution_results"] = executionResults;
        result["success_count"] = successCount;
        result["success_rate"] = steps.empty() ? 0.0 : (successCount * 100.0) / steps.size();
        result["overall_success"] = steps.empty() ? false : (successCount == steps.size());

        executionComplete(result);
        return result;
    } catch (const std::exception& e) {
        errorOccurred(e.what());
        result["error"] = e.what();
        return result;
    }
}

json AgenticExecutor::decomposeTask(const std::string& goal)
{
    if (!m_agenticEngine) {
        return json::array();
    }
    
    std::string fullPrompt = "You are an expert planner. Decompose this task into actionable JSON steps: " + goal + 
                           "\nReturn ONLY a JSON array of objects with keys: action, description, params.";
                           
    std::string response = m_agenticEngine ? m_agenticEngine->generateResponse(fullPrompt) : "[]";
    
    try {
        // Attempt to find JSON array in response
        size_t start = response.find('[');
        size_t end = response.rfind(']');
        if (start != std::string::npos && end != std::string::npos && end > start) {
            std::string jsonStr = response.substr(start, end - start + 1);
            return json::parse(jsonStr);
        }
        return json::array();
    } catch (...) {
        logMessage("Failed to parse decomposition plan: " + response);
        return json::array();
    }
}
// ========== STEP EXECUTION ==========

bool AgenticExecutor::executeStep(const json& step)
{
    std::string action = step["action"].toString();
    void* params = step["params"].toObject();
    std::string description = step["description"].toString();

    logMessage("Step: " + description);

    try {
        if (action == "create_directory") {
            std::string path = params["path"].toString();
            return createDirectory(path);
        }
        else if (action == "create_file") {
            std::string path = params["path"].toString();
            std::string content = params["content"].toString();
            
            // If content not in params, generate it
            if (content.empty() && params.contains("specification")) {
                void* codeGen = generateCode(params["specification"].toString());
                content = codeGen["code"].toString();
            }
            
            return createFile(path, content);
        }
        else if (action == "compile") {
            std::string projectPath = params["project_path"].toString();
            std::string compiler = params.contains("compiler") ? params["compiler"].toString() : "g++";
            void* compileResult = compileProject(projectPath, compiler);
            return compileResult["success"].toBool();
        }
        else if (action == "run") {
            std::string executable = params["executable"].toString();
            std::vector<std::string> args = params["args"].toVariant().toStringList();
            void* runResult = runExecutable(executable, args);
            return runResult["success"].toBool();
        }
        else if (action == "generate_code") {
            std::string spec = params["specification"].toString();
            std::string outputPath = params["output_path"].toString();
            void* codeGen = generateCode(spec);
            if (codeGen.contains("code")) {
                return writeFile(outputPath, codeGen["code"].toString());
            }
            return false;
        }
        else if (action == "tool_call") {
            std::string toolName = params["tool_name"].toString();
            void* toolParams = params["tool_params"].toObject();
            void* toolResult = callTool(toolName, toolParams);
            return toolResult["success"].toBool();
        }
        else {
            return false;
        }

    } catch (const std::exception& e) {
        errorOccurred(std::string("Step failed: %1")));
        return false;
    }
}

bool AgenticExecutor::verifyStepCompletion(const void*& step, const std::string& result)
{
    std::string criteria = step["criteria"].toString();
    if (criteria.empty()) return true;

    // Use model to verify completion
    std::string prompt = std::string(
        "Verification Task:\n"
        "Expected: %1\n"
        "Actual Result: %2\n\n"
        "Does the actual result meet the success criteria? Answer with ONLY 'yes' or 'no'."
    );

    std::string verification = m_agenticEngine->generateResponse(prompt);
    return verification.toLower().contains("yes");
}

// ========== FILE SYSTEM OPERATIONS (REAL) ==========

bool AgenticExecutor::createDirectory(const std::string& path)
{
    std::filesystem::path dir;
    bool success = dir.mkpath(path);
    
    if (success) {
        logMessage("Created directory: " + path);
        addToMemory("last_created_dir", path);
    } else {
    }
    
    return success;
}

bool AgenticExecutor::createFile(const std::string& path, const std::string& content)
{
    // Ensure parent directory exists
    std::filesystem::path fp(path);
    if (!std::filesystem::exists(fp.parent_path())) {
        std::filesystem::create_directories(fp.parent_path());
    }

    std::ofstream file(path);
    if (!file.is_open()) {
        return false;
    }

    file << content;
    file.close();

    logAction("Created file: " + path);
    return true;
}

bool AgenticExecutor::writeFile(const std::string& path, const std::string& content)
{
    return createFile(path, content);
}

std::string AgenticExecutor::readFile(const std::string& path)
{
    std::ifstream file(path);
    if (!file.is_open()) {
        return "";
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

bool AgenticExecutor::deleteFile(const std::string& path)
{
    try {
        return std::filesystem::remove(path);
    } catch (...) {
        return false;
    }
}

bool AgenticExecutor::deleteDirectory(const std::string& path)
{
    try {
        return std::filesystem::remove_all(path) > 0;
    } catch (...) {
        return false;
    }
}

std::vector<std::string> AgenticExecutor::listDirectory(const std::string& path)
{
    std::filesystem::path dir(path);
    std::vector<std::string> entries = dir.entryList(std::filesystem::path::AllEntries | std::filesystem::path::NoDotAndDotDot);
    
    return entries;
}

// ========== COMPILER INTEGRATION (REAL) ==========

void* AgenticExecutor::compileProject(const std::string& projectPath, const std::string& compiler)
{
    void* result;
    result["compiler"] = compiler;
    result["project_path"] = projectPath;

    logMessage("Compiling with " + compiler + "...");

    void* process;
    process.setWorkingDirectory(projectPath);

    // Detect build system and compile
    if (std::fstream::exists(projectPath + "/CMakeLists.txt")) {
        // CMake project
        logMessage("Detected CMake project");
        
        // Create build directory
        createDirectory(projectPath + "/build");
        process.setWorkingDirectory(projectPath + "/build");
        
        // Run cmake
        process.start("cmake", std::vector<std::string>() << "..");
        process.waitForFinished(-1);
        
        std::string cmakeOutput = process.readAllStandardOutput();
        std::string cmakeError = process.readAllStandardError();
        
        if (process.exitCode() != 0) {
            result["success"] = false;
            result["error"] = "CMake configuration failed: " + cmakeError;
            return result;
        }
        
        // Run make
        process.start("cmake", std::vector<std::string>() << "--build" << ".");
        process.waitForFinished(-1);
        
        std::string buildOutput = process.readAllStandardOutput();
        std::string buildError = process.readAllStandardError();
        
        result["cmake_output"] = cmakeOutput;
        result["build_output"] = buildOutput;
        result["build_error"] = buildError;
        result["exit_code"] = process.exitCode();
        result["success"] = (process.exitCode() == 0);
        
    } else {
        // Direct compilation
        std::vector<std::string> files;
        std::filesystem::path dir(projectPath);
        files = dir.entryList(std::vector<std::string>() << "*.cpp" << "*.c", std::filesystem::path::Files);
        
        if (files.empty()) {
            result["success"] = false;
            result["error"] = "No source files found";
            return result;
        }
        
        std::vector<std::string> args;
        args << "-o" << "output";
        for (const std::string& file : files) {
            args << file;
        }
        
        process.start(compiler, args);
        process.waitForFinished(-1);
        
        result["compiler_output"] = std::string::fromUtf8(process.readAllStandardOutput());
        result["compiler_error"] = std::string::fromUtf8(process.readAllStandardError());
        result["exit_code"] = process.exitCode();
        result["success"] = (process.exitCode() == 0);
    }

    if (result["success"].toBool()) {
        logMessage("Compilation successful!");
    } else {
        logMessage("Compilation failed: " + result["compiler_error"].toString());
        errorOccurred("Compilation failed");
    }

    addToMemory("last_compilation", result);
    return result;
}

void* AgenticExecutor::runExecutable(const std::string& executablePath, const std::vector<std::string>& args)
{
    void* result;
    result["executable"] = executablePath;
    result["arguments"] = void*::fromStringList(args);

    logMessage("Running: " + executablePath);

    void* process;
    process.start(executablePath, args);
    
    if (!process.waitForStarted()) {
        result["success"] = false;
        result["error"] = "Failed to start process";
        return result;
    }

    process.waitForFinished(-1);

    result["stdout"] = std::string::fromUtf8(process.readAllStandardOutput());
    result["stderr"] = std::string::fromUtf8(process.readAllStandardError());
    result["exit_code"] = process.exitCode();
    result["success"] = (process.exitCode() == 0);

    if (result["success"].toBool()) {
        logMessage("Execution completed");
    } else {
    }

    addToMemory("last_execution", result);
    return result;
}

// ========== CODE GENERATION ==========

void* AgenticExecutor::generateCode(const std::string& specification)
{
    void* result;
    
    if (!m_agenticEngine) {
        result["error"] = "No engine available";
        return result;
    }


    std::string prompt = std::string(
        "Generate production-ready C++ code for the following specification:\n\n"
        "%1\n\n"
        "Requirements:\n"
        "- Complete, compilable code\n"
        "- Include all necessary headers\n"
        "- Add error handling\n"
        "- Add helpful comments\n"
        "- Follow C++17 best practices\n\n"
        "Return ONLY the code, no explanations."
    );

    std::string response = m_agenticEngine->generateCode(prompt);
    std::string code = extractCodeFromResponse(response);

    result["specification"] = specification;
    result["code"] = code;
    result["success"] = !code.empty();

    return result;
}

std::string AgenticExecutor::extractCodeFromResponse(const std::string& response)
{
    // Extract code from markdown code blocks
    std::regex codeBlockRegex("```(?:cpp|c\\+\\+)?\\s*\\n([\\s\\S]*?)```");
    std::smatch match = codeBlockRegex.match(response);
    
    if (match.hasMatch()) {
        return match"".trimmed();
    }
    
    // If no code block, return the whole response
    return response.trimmed();
}

// ========== FUNCTION CALLING / TOOL USE ==========

void* AgenticExecutor::getAvailableTools()
{
    void* tools;
    
    // File system tools
    tools.append(void*{{"name", "create_directory"}, {"description", "Create a new directory"}});
    tools.append(void*{{"name", "create_file"}, {"description", "Create a file with content"}});
    tools.append(void*{{"name", "read_file"}, {"description", "Read file contents"}});
    tools.append(void*{{"name", "delete_file"}, {"description", "Delete a file"}});
    tools.append(void*{{"name", "list_directory"}, {"description", "List directory contents"}});
    
    // Compilation tools
    tools.append(void*{{"name", "compile_project"}, {"description", "Compile C++ project"}});
    tools.append(void*{{"name", "run_executable"}, {"description", "Run compiled executable"}});
    
    // Model tools
    tools.append(void*{{"name", "train_model"}, {"description", "Fine-tune a GGUF model with dataset"}});
    tools.append(void*{{"name", "is_training"}, {"description", "Check if model training is in progress"}});
    
    return tools;
}

void* AgenticExecutor::callTool(const std::string& toolName, const void*& params)
{
    void* result;
    result["tool"] = toolName;
    result["params"] = params;


    if (toolName == "create_directory") {
        bool success = createDirectory(params["path"].toString());
        result["success"] = success;
    }
    else if (toolName == "create_file") {
        bool success = createFile(params["path"].toString(), params["content"].toString());
        result["success"] = success;
    }
    else if (toolName == "read_file") {
        std::string content = readFile(params["path"].toString());
        result["success"] = !content.empty();
        result["content"] = content;
    }
    else if (toolName == "delete_file") {
        bool success = deleteFile(params["path"].toString());
        result["success"] = success;
    }
    else if (toolName == "compile_project") {
        void* compileResult = compileProject(params["project_path"].toString());
        result = compileResult;
    }
    else if (toolName == "run_executable") {
        void* runResult = runExecutable(params["executable"].toString());
        result = runResult;
    }
    else if (toolName == "list_directory") {
        std::vector<std::string> entries = listDirectory(params["path"].toString());
        result["success"] = true;
        result["entries"] = void*::fromStringList(entries);
    }
    else if (toolName == "train_model") {
        std::string datasetPath = params["dataset_path"].toString();
        std::string modelPath = params["model_path"].toString();
        void* config = params["config"].toObject();
        void* trainResult = trainModel(datasetPath, modelPath, config);
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

void AgenticExecutor::addToMemory(const std::string& key, const std::any& value)
{
    m_memory[key] = value;
}

std::any AgenticExecutor::getFromMemory(const std::string& key)
{
    return m_memory.value(key);
}

void AgenticExecutor::clearMemory()
{
    m_memory.clear();
    m_executionHistory = void*();
}

std::string AgenticExecutor::getFullContext()
{
    std::string context;
    context += "=== EXECUTION CONTEXT ===\n";
    context += "Working Directory: " + m_currentWorkingDirectory + "\n";
    context += "Memory Items: " + std::string::number(m_memory.size()) + "\n";
    context += "Execution History: " + std::string::number(m_executionHistory.size()) + " steps\n";
    context += "\n=== MEMORY ===\n";
    
    for (auto it = m_memory.begin(); it != m_memory.end(); ++it) {
        context += it.key() + ": " + it.value().toString() + "\n";
    }
    
    return context;
}

// ========== SELF-CORRECTION ==========

bool AgenticExecutor::detectFailure(const std::string& output)
{
    std::vector<std::string> failureIndicators = {
        "error", "failed", "exception", "cannot", "unable",
        "undefined reference", "segmentation fault", "compilation terminated"
    };
    
    std::string lowerOutput = output.toLower();
    for (const std::string& indicator : failureIndicators) {
        if (lowerOutput.contains(indicator)) {
            return true;
        }
    }
    
    return false;
}

std::string AgenticExecutor::generateCorrectionPlan(const std::string& failureReason)
{
    if (!m_agenticEngine) return "No correction available";

    std::string prompt = std::string(
        "An automated task failed with this error:\n%1\n\n"
        "Analyze the error and provide a correction plan. Include:\n"
        "1. Root cause of the failure\n"
        "2. Specific steps to fix it\n"
        "3. Code changes if needed\n"
        "4. Verification steps\n\n"
        "Be concise and actionable."
    );

    return m_agenticEngine->generateResponse(prompt);
}

void* AgenticExecutor::retryWithCorrection(const void*& failedStep)
{
    m_currentRetryCount++;
    
    void* result;
    result["original_step"] = failedStep;
    result["retry_attempt"] = m_currentRetryCount;

    std::string failureContext = getFromMemory("last_error").toString();
    std::string correctionPlan = generateCorrectionPlan(failureContext);
    
    logMessage("Attempting correction: " + correctionPlan);

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

void* AgenticExecutor::trainModel(const std::string& datasetPath, const std::string& modelPath, const void*& config)
{
    void* result;
    
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
    
    logMessage("Starting model training with dataset: " + datasetPath);
    
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


