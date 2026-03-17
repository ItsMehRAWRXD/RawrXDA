#include <compare>
#include <string>
#include <fstream> 
#include <filesystem>
#include <iostream>
#include <sstream>
#include <regex>
#include <chrono>

#include "agentic_executor.h"
#include "agentic_engine.h"
#include "cpu_inference_engine.h" 
#include "model_trainer.h"

using json = nlohmann::json;

AgenticExecutor::AgenticExecutor(void* parent)
    : m_currentWorkingDirectory(std::filesystem::current_path().string())
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
    // Real training requires pointer to ModelTrainer
    m_modelTrainer = std::make_unique<ModelTrainer>(); 
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
    
    std::string fullPrompt = "You are an autonomous AI agent. Decompose this task into actionable JSON steps: " + goal + 
                           "\nReturn ONLY a JSON array of objects with keys: action, description, params.";
                           
    std::string response = m_agenticEngine ? m_agenticEngine->processQuery(fullPrompt) : "[]";
    
    // Attempt to parse JSON response
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
    if (!step.contains("action")) return false;
    
    std::string action = step["action"];
    std::string description = step.contains("description") ? step["description"].get<std::string>() : action;

    if (action == "CREATE_FILE") {
        // Special handling for file creation to include content generation
        std::string path = step["params"]["path"];
        std::string content = step["params"].contains("content") ? step["params"]["content"] : "";
        
        // If content not provided, generate it
        if (content.empty() && step["params"].contains("specification")) {
            content = generateCode(step["params"]["specification"]);
        }
        
        return createFile(path, content);
    }

    logMessage("Step: " + description);

    try {
        if (action == "create_directory") {
            std::string path = step["params"]["path"];
            return createDirectory(path);
        }
        else if (action == "create_file") {
            std::string path = step["params"]["path"];
            std::string content = "";
            
            if (step["params"].contains("content")) {
                content = step["params"]["content"];
            }
            
            // If content not in params, generate it
            if (content.empty() && step["params"].contains("specification")) {
                json codeGen = generateCode(step["params"]["specification"]);
                if (codeGen.contains("code")) {
                    content = codeGen["code"];
                }
            }
            
            return createFile(path, content);
        }
        else if (action == "compile") {
            std::string projectPath = step["params"]["project_path"];
            std::string compiler = step["params"].contains("compiler") ? step["params"]["compiler"].get<std::string>() : "g++";
            json compileResult = compileProject(projectPath, compiler);
            return compileResult.contains("success") && compileResult["success"].get<bool>();
        }
        else if (action == "run") {
            std::string executable = step["params"]["executable"];
            std::vector<std::string> args;
            if (step["params"].contains("args") && step["params"]["args"].is_array()) {
                args = step["params"]["args"].get<std::vector<std::string>>();
            }
            json runResult = runExecutable(executable, args);
            return runResult.contains("success") && runResult["success"].get<bool>();
        }
        else if (action == "generate_code") {
            std::string spec = step["params"]["specification"];
            std::string outputPath = step["params"]["output_path"];
            json codeGen = generateCode(spec);
            if (codeGen.contains("code")) {
                return writeFile(outputPath, codeGen["code"]);
            }
            return false;
        }
        else if (action == "tool_call") {
            std::string toolName = step["params"]["tool_name"];
            json toolParams = step["params"]["tool_params"];
            json toolResult = callTool(toolName, toolParams);
            return toolResult.contains("success") && toolResult["success"].get<bool>();
        }
        else {
            return false;
        }

    } catch (const std::exception& e) {
        errorOccurred("Step failed: " + std::string(e.what()));
        return false;
    }
}

bool AgenticExecutor::verifyStepCompletion(const json& step, const std::string& result)
{
    if (!step.contains("criteria")) return true;
    std::string criteria = step["criteria"];
    if (criteria.empty()) return true;

    // Use model to verify completion
    std::ostringstream prompt;
    prompt << "Verification Task:\n"
           << "Expected: " << criteria << "\n"
           << "Actual Result: " << result << "\n\n"
           << "Does the actual result meet the success criteria? Answer with ONLY 'yes' or 'no'.";

    if (m_agenticEngine) {
        std::string verification = m_agenticEngine->processQuery(prompt.str());
        std::transform(verification.begin(), verification.end(), verification.begin(), ::tolower);
        return verification.find("yes") != std::string::npos;
    }
    return true;
}

// ========== FILE SYSTEM OPERATIONS (REAL) ==========

bool AgenticExecutor::createDirectory(const std::string& path)
{
    try {
        std::filesystem::create_directories(path);
        logMessage("Created directory: " + path);
        addToMemory("last_created_dir", path);
        return true;
    } catch (const std::exception& e) {
        logMessage("Failed to create directory: " + std::string(e.what()));
        return false;
    }
}

bool AgenticExecutor::createFile(const std::string& path, const std::string& content)
{
    // Ensure parent directory exists
    std::filesystem::path fp(path);
    if (fp.has_parent_path() && !std::filesystem::exists(fp.parent_path())) {
         try {
            std::filesystem::create_directories(fp.parent_path());
         } catch(...) { return false; }
    }

    std::ofstream file(path);
    if (!file.is_open()) {
        return false;
    }

    file << content;
    file.close();

    logMessage("Created file: " + path);
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
    std::vector<std::string> entries;
    try {
        if (std::filesystem::exists(path) && std::filesystem::is_directory(path)) {
            for (const auto& entry : std::filesystem::directory_iterator(path)) {
                entries.push_back(entry.path().filename().string());
            }
        }
    } catch (...) {
        return {};
    }
    return entries;
}

// Helper function for shell execution
static std::pair<std::string, int> runShellCommand(const std::string& cmd, const std::string& directory = "") {
    std::string command = cmd;
    if (!directory.empty()) {
        command = "cd /d \"" + directory + "\" && " + cmd;
    }
    command += " 2>&1";
    
    std::string output;
    FILE* pipe = _popen(command.c_str(), "r");
    if (!pipe) return {"Failed to spawn shell", 1};
    
    char buffer[128];
    while (fgets(buffer, sizeof(buffer), pipe) != NULL) {
        output += buffer;
    }
    
    int result = _pclose(pipe);
    return {output, result};
}

// ========== COMPILER INTEGRATION (REAL) ==========

json AgenticExecutor::compileProject(const std::string& projectPath, const std::string& compiler)
{
    json result;
    result["compiler"] = compiler;
    result["project_path"] = projectPath;

    logMessage("Compiling with " + compiler + "...");

    // Detect build system and compile
    bool isCMake = std::filesystem::exists(projectPath + "/CMakeLists.txt");
    if (isCMake) {
        logMessage("Detected CMake project");
        
        // Create build directory
        std::string buildDir = projectPath + "/build";
        createDirectory(buildDir);
        
        // Run cmake
        auto cmakeRes = runShellCommand("cmake ..", buildDir);
        if (cmakeRes.second != 0) {
            result["success"] = false;
            result["error"] = "CMake configuration failed: " + cmakeRes.first;
            return result;
        }
        
        // Run cmake --build
        auto buildRes = runShellCommand("cmake --build .", buildDir);
        
        result["cmake_output"] = cmakeRes.first;
        result["build_output"] = buildRes.first;
        result["exit_code"] = buildRes.second;
        result["success"] = (buildRes.second == 0);
        
    } else {
        // Direct compilation
        std::vector<std::string> files;
        try {
            for (const auto& entry : std::filesystem::directory_iterator(projectPath)) {
                if (entry.path().extension() == ".cpp" || entry.path().extension() == ".c") {
                    files.push_back(entry.path().filename().string());
                }
            }
        } catch(...) {}
        
        if (files.empty()) {
            result["success"] = false;
            result["error"] = "No source files found";
            return result;
        }
        
        std::string cmd = compiler + " -o output";
        for (const std::string& file : files) {
            cmd += " " + file;
        }
        
        auto compileRes = runShellCommand(cmd, projectPath);
        
        result["compiler_output"] = compileRes.first;
        result["exit_code"] = compileRes.second;
        result["success"] = (compileRes.second == 0);
    }

    if (result["success"].get<bool>()) {
        logMessage("Compilation successful!");
    } else {
        std::string err = result.contains("build_output") ? result["build_output"].get<std::string>() : 
                         (result.contains("compiler_output") ? result["compiler_output"].get<std::string>() : "Unknown error");
        logMessage("Compilation failed: " + err);
        errorOccurred("Compilation failed");
    }

    addToMemory("last_compilation", result);
    return result;
}

json AgenticExecutor::runExecutable(const std::string& executablePath, const std::vector<std::string>& args)
{
    json result;
    result["executable"] = executablePath;
    result["arguments"] = args;

    logMessage("Running: " + executablePath);

    std::string cmd = "\"" + executablePath + "\"";
    for(const auto& arg : args) {
        cmd += " \"" + arg + "\"";
    }

    std::filesystem::path exePath(executablePath);
    std::string cwd = exePath.has_parent_path() ? exePath.parent_path().string() : "";

    auto runRes = runShellCommand(cmd, cwd);

    result["stdout"] = runRes.first;
    result["exit_code"] = runRes.second;
    result["success"] = (runRes.second == 0);

    if (result["success"].get<bool>()) {
        logMessage("Execution completed");
    }

    addToMemory("last_execution", result);
    return result;
}

// ========== CODE GENERATION ==========

json AgenticExecutor::generateCode(const std::string& specification)
{
    json result;
    
    if (!m_agenticEngine) {
        result["error"] = "No engine available";
        result["success"] = false;
        return result;
    }

    std::ostringstream prompt;
    prompt << "Generate production-ready C++ code for the following specification:\n\n"
           << specification << "\n\n"
           << "Requirements:\n"
           << "- Complete, compilable code\n"
           << "- Include all necessary headers\n"
           << "- Add error handling\n"
           << "- Add helpful comments\n"
           << "- Follow C++17 best practices\n\n"
           << "Return ONLY the code, no explanations.";

    std::string response = m_agenticEngine->processQuery(prompt.str()); 
    std::string code = extractCodeFromResponse(response);

    result["specification"] = specification;
    result["code"] = code;
    result["success"] = !code.empty();

    return result;
}

std::string AgenticExecutor::extractCodeFromResponse(const std::string& response) {
    std::regex codeBlockRegex("```(?:cpp|c\\+\\+)?\\s*\\n([\\s\\S]*?)```");
    std::smatch match;
    if (std::regex_search(response, match, codeBlockRegex) && match.size() > 1) {
        return match[1].str();
    }
    return response;
}

// ========== FUNCTION CALLING / TOOL USE ==========

json AgenticExecutor::getAvailableTools()
{
    json tools = json::array();
    
    tools.push_back({{"name", "create_directory"}, {"description", "Create a new directory"}});
    tools.push_back({{"name", "create_file"}, {"description", "Create a file with content"}});
    tools.push_back({{"name", "read_file"}, {"description", "Read file contents"}});
    tools.push_back({{"name", "delete_file"}, {"description", "Delete a file"}});
    tools.push_back({{"name", "list_directory"}, {"description", "List directory contents"}});
    
    // Compilation tools
    tools.push_back({{"name", "compile_project"}, {"description", "Compile C++ project"}});
    tools.push_back({{"name", "run_executable"}, {"description", "Run compiled executable"}});
    
    // Model tools
    tools.push_back({{"name", "train_model"}, {"description", "Fine-tune a GGUF model with dataset"}});
    tools.push_back({{"name", "is_training"}, {"description", "Check if model training is in progress"}});
    
    return tools;
}

json AgenticExecutor::callTool(const std::string& toolName, const json& params)
{
    json result;
    result["tool"] = toolName;
    result["params"] = params;

    if (toolName == "create_directory") {
        bool success = createDirectory(params["path"]);
        result["success"] = success;
    }
    else if (toolName == "create_file") {
        bool success = createFile(params["path"], params["content"]);
        result["success"] = success;
    }
    else if (toolName == "read_file") {
        std::string content = readFile(params["path"]);
        result["success"] = !content.empty();
        result["content"] = content;
    }
    else if (toolName == "delete_file") {
        bool success = deleteFile(params["path"]);
        result["success"] = success;
    }
    else if (toolName == "list_directory") {
        std::vector<std::string> entries = listDirectory(params["path"]);
        result["success"] = true;
        result["entries"] = entries;
    }
    else if (toolName == "compile_project") {
        return compileProject(params["project_path"]);
    }
    else if (toolName == "run_executable") {
        std::string exe = params["executable"];
        std::vector<std::string> args; 
        if(params.contains("args")) args = params["args"].get<std::vector<std::string>>();
        return runExecutable(exe, args);
    }
    else if (toolName == "train_model") {
        return trainModel(params["dataset_path"], params["model_path"], params["config"]);
    }
    else if (toolName == "is_training") {
        result["success"] = true;
        result["is_training"] = isTrainingModel();
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
    auto it = m_memory.find(key);
    if (it != m_memory.end()) return it->second;
    return std::any();
}

void AgenticExecutor::clearMemory()
{
    m_memory.clear();
    m_executionHistory = json::array();
}

std::string AgenticExecutor::getFullContext()
{
    std::ostringstream context;
    context << "=== EXECUTION CONTEXT ===\n";
    context << "Working Directory: " << m_currentWorkingDirectory << "\n";
    context << "Memory Items: " << m_memory.size() << "\n";
    context << "Execution History: " << m_executionHistory.size() << " steps\n";
    context << "\n=== MEMORY ===\n";
    
    for (const auto& kv : m_memory) {
        context << kv.first << ": [Stored Value]\n";
    }
    
    return context.str();
}

// ========== SELF-CORRECTION ==========

bool AgenticExecutor::detectFailure(const std::string& output)
{
    std::vector<std::string> failureIndicators = {
        "error", "failed", "exception", "cannot", "unable",
        "undefined reference", "segmentation fault", "compilation terminated"
    };
    
    std::string lowerOutput = output;
    std::transform(lowerOutput.begin(), lowerOutput.end(), lowerOutput.begin(), ::tolower);
    
    for (const std::string& indicator : failureIndicators) {
        if (lowerOutput.find(indicator) != std::string::npos) {
            return true;
        }
    }
    
    return false;
}

std::string AgenticExecutor::generateCorrectionPlan(const std::string& failureReason)
{
    if (!m_agenticEngine) return "No correction available";

    std::ostringstream prompt;
    prompt << "An automated task failed with this error:\n" << failureReason << "\n\n"
           << "Analyze the error and provide a correction plan. Include:\n"
           << "1. Root cause of the failure\n"
           << "2. Specific steps to fix it\n"
           << "3. Code changes if needed\n"
           << "4. Verification steps\n\n"
           << "Be concise and actionable.";

    return m_agenticEngine->processQuery(prompt.str());
}

json AgenticExecutor::retryWithCorrection(const json& failedStep)
{
    m_currentRetryCount++;
    
    json result;
    result["original_step"] = failedStep;
    result["retry_attempt"] = m_currentRetryCount;

    std::string failureContext = "Unknown Error";
    try {
        auto anyVal = getFromMemory("last_error");
        if(anyVal.has_value()) {
             try { failureContext = std::any_cast<std::string>(anyVal); } catch(...) {}
        }
    } catch(...) {}

    std::string correctionPlan = generateCorrectionPlan(failureContext);
    
    logMessage("Attempting correction: " + correctionPlan);

    // Naive retry
    bool success = executeStep(failedStep);
    
    result["success"] = success;
    result["correction_plan"] = correctionPlan;
    
    if (!success && m_currentRetryCount < m_maxRetries) {
        return retryWithCorrection(failedStep);
    }
    
    m_currentRetryCount = 0;
    return result;
}

// ========== MODEL TRAINING ==========

json AgenticExecutor::trainModel(const std::string& datasetPath, const std::string& modelPath, const json& config)
{
    json result;
    
    if (!m_modelTrainer) {
        result["success"] = false;
        result["error"] = "Model trainer not initialized";
        return result;
    }

    if (m_inferenceEngine == nullptr) {
        result["success"] = false;
        result["error"] = "Inference engine not available";
        return result;
    }
    
    if (!m_modelTrainer->initialize(m_inferenceEngine, modelPath)) {
        result["success"] = false;
        result["error"] = "Failed to initialize model trainer";
        return result;
    }
    
    logMessage("Starting model training with dataset: " + datasetPath);
    
    // Configure training
    ModelTrainer::TrainingConfig trainConfig;
    trainConfig.datasetPath = datasetPath;
    trainConfig.outputPath = modelPath + ".trained";
    
    trainConfig.epochs = config.value("epochs", 10);
    trainConfig.learningRate = config.value("learning_rate", 1e-4);
    trainConfig.batchSize = config.value("batch_size", 32);
    trainConfig.sequenceLength = config.value("sequence_length", 512);
    trainConfig.gradientClip = config.value("gradient_clip", 1.0);
    trainConfig.validateEveryEpoch = config.value("validate_every_epoch", true);
    trainConfig.validationSplit = config.value("validation_split", 0.1);
    trainConfig.weightDecay = config.value("weight_decay", 0.01);
    trainConfig.warmupSteps = config.value("warmup_steps", 0.1);
    
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


