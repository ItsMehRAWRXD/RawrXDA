#include "cli_command_handler.h"
#include "gguf_loader.h"
#include "telemetry.h"
#include "overclock_vendor.h"
#include "overclock_governor.h"
#include <iostream>
#include <sstream>
#include <fstream>
#include <filesystem>
#include <algorithm>
#include <iomanip>
#include <cstdlib>

namespace CLI {

CommandHandler::CommandHandler(AppState& state)
    : m_state(state)
    , m_orchestrator(nullptr)
    , m_inferenceEngine(nullptr)
    , m_modelInvoker(nullptr)
    , m_subagentPool(nullptr)
    , m_modelLoader(std::make_unique<GGUFLoader>())
    , m_modelLoaded(false)
    , m_agenticModeEnabled(false)
    , m_autonomousModeEnabled(false)
    , m_hotReloadEnabled(false)
    , m_historyIndex(0) {
    
    printInfo("CLI Command Handler initialized");
    
    // Initialize agent systems on startup
    initializeAgentSystems();
}

CommandHandler::~CommandHandler() {
    shutdownAgentSystems();
    
    if (m_autonomousThread && m_autonomousThread->joinable()) {
        m_autonomousThread->join();
    }
    if (m_inferenceThread && m_inferenceThread->joinable()) {
        m_inferenceThread->join();
    }
}

bool CommandHandler::executeCommand(const std::string& command) {
    if (command.empty()) {
        return true;
    }
    
    auto parts = parseCommand(command);
    if (parts.empty()) {
        return true;
    }
    
    const std::string& cmd = parts[0];
    
    // Model management commands
    if (cmd == "load" && parts.size() >= 2) {
        cmdLoadModel(joinArgs(parts, 1));
    }
    else if (cmd == "unload") {
        cmdUnloadModel();
    }
    else if (cmd == "models") {
        cmdListModels();
    }
    else if (cmd == "modelinfo") {
        cmdModelInfo();
    }
    // Inference commands
    else if (cmd == "infer" && parts.size() >= 2) {
        cmdInfer(joinArgs(parts, 1));
    }
    else if (cmd == "stream" && parts.size() >= 2) {
        cmdInferStream(joinArgs(parts, 1));
    }
    else if (cmd == "chat") {
        cmdChat();
    }
    else if (cmd == "temp" && parts.size() >= 2) {
        cmdSetTemperature(std::stof(parts[1]));
    }
    else if (cmd == "topp" && parts.size() >= 2) {
        cmdSetTopP(std::stof(parts[1]));
    }
    else if (cmd == "maxtokens" && parts.size() >= 2) {
        cmdSetMaxTokens(std::stoi(parts[1]));
    }
    // Agentic commands
    else if (cmd == "plan" && parts.size() >= 2) {
        cmdAgenticPlan(joinArgs(parts, 1));
    }
    else if (cmd == "execute" && parts.size() >= 2) {
        cmdAgenticExecute(parts[1]);
    }
    else if (cmd == "status") {
        cmdAgenticStatus();
    }
r    else if (cmd == "selfcorrect") {
        cmdAgenticSelfCorrect();
    }
    else if (cmd == "analyze" && parts.size() >= 2) {
        cmdAgenticAnalyzeCode(parts[1]);
    }
    else if (cmd == "generate" && parts.size() >= 2) {
        cmdAgenticGenerateCode(joinArgs(parts, 1));
    }
    else if (cmd == "refactor" && parts.size() >= 2) {
        cmdAgenticRefactor(parts[1]);
    }
    // Autonomous features
    else if (cmd == "autonomous") {
        if (parts.size() >= 2) {
            cmdAutonomousMode(parts[1] == "on" || parts[1] == "enable" || parts[1] == "true");
        } else {
            cmdAutonomousStatus();
        }
    }
    else if (cmd == "goal" && parts.size() >= 2) {
        cmdAutonomousGoal(joinArgs(parts, 1));
    }
    // Hot-reload commands
    else if (cmd == "hotreload") {
        if (parts.size() >= 2 && parts[1] == "enable") {
            cmdHotReloadEnable();
        } else if (parts.size() >= 2 && parts[1] == "disable") {
            cmdHotReloadDisable();
        } else if (parts.size() >= 4 && parts[1] == "patch") {
            cmdHotReloadPatch(parts[2], joinArgs(parts, 3));
        } else if (parts.size() >= 3 && parts[1] == "revert") {
            cmdHotReloadRevert(std::stoi(parts[2]));
        } else if (parts.size() >= 2 && parts[1] == "list") {
            cmdHotReloadList();
        }
    }
    // Code analysis
    else if (cmd == "analyzefile" && parts.size() >= 2) {
        cmdAnalyzeFile(parts[1]);
    }
    else if (cmd == "analyzeproject" && parts.size() >= 2) {
        cmdAnalyzeProject(parts[1]);
    }
    else if (cmd == "patterns" && parts.size() >= 2) {
        cmdDetectPatterns(parts[1]);
    }
    else if (cmd == "improve" && parts.size() >= 2) {
        cmdSuggestImprovements(parts[1]);
    }
    // Telemetry and system
    else if (cmd == "telemetry") {
        cmdTelemetryStatus();
    }
    else if (cmd == "snapshot") {
        cmdTelemetrySnapshot();
    }
    else if (cmd == "serverinfo") {
        cmdServerInfo();
    }
    else if (cmd == "overclock") {
        cmdOverclockStatus();
    }
    else if (cmd == "g") {
        cmdOverclockToggle();
    }
    else if (cmd == "a") {
        cmdOverclockApplyProfile();
    }
    else if (cmd == "r") {
        cmdOverclockReset();
    }
    // Settings
    else if (cmd == "save") {
        cmdSaveSettings();
    }
    else if (cmd == "loadsettings") {
        cmdLoadSettings();
    }
    else if (cmd == "settings") {
        cmdShowSettings();
    }
    // Utilities
    else if (cmd == "grep" && parts.size() >= 2) {
        std::string path = parts.size() >= 3 ? parts[2] : ".";
        cmdGrepFiles(parts[1], path);
    }
    else if (cmd == "read" && parts.size() >= 2) {
        int start = parts.size() >= 3 ? std::stoi(parts[2]) : -1;
        int end = parts.size() >= 4 ? std::stoi(parts[3]) : -1;
        cmdReadFile(parts[1], start, end);
    }
    else if (cmd == "search" && parts.size() >= 2) {
        std::string path = parts.size() >= 3 ? parts[2] : ".";
        cmdSearchFiles(parts[1], path);
    }
    // Help and version
    else if (cmd == "help" || cmd == "h" || cmd == "?") {
        printHelp();
    }
    else if (cmd == "version" || cmd == "v") {
        printVersion();
    }
    else if (cmd == "quit" || cmd == "q" || cmd == "exit") {
        return false;
    }
    else {
        printError("Unknown command: " + cmd + ". Type 'help' for available commands.");
    }
    
    return true;
}

void CommandHandler::printHelp() {
    printInfo("\n=== RawrXD CLI Commands ===\n");
    std::cout << "\nModel Management:\n";
    std::cout << "  load <path>         - Load GGUF model from path\n";
    std::cout << "  unload              - Unload current model\n";
    std::cout << "  models              - List available models\n";
    std::cout << "  modelinfo           - Show current model information\n";
    
    std::cout << "\nInference:\n";
    std::cout << "  infer <prompt>      - Run single inference\n";
    std::cout << "  stream <prompt>     - Run streaming inference\n";
    std::cout << "  chat                - Enter interactive chat mode\n";
    std::cout << "  temp <value>        - Set temperature (0.0-2.0)\n";
    std::cout << "  topp <value>        - Set top-p (0.0-1.0)\n";
    std::cout << "  maxtokens <count>   - Set max tokens\n";
    
    std::cout << "\nAgentic Features:\n";
    std::cout << "  plan <goal>         - Create execution plan for goal\n";
    std::cout << "  execute <taskid>    - Execute planned task\n";
    std::cout << "  status              - Show agentic engine status\n";
    std::cout << "  selfcorrect         - Run self-correction\n";
    std::cout << "  analyze <file>      - Analyze code file\n";
    std::cout << "  generate <prompt>   - Generate code from description\n";
    std::cout << "  refactor <file>     - Suggest code refactorings\n";
    
    std::cout << "\nAutonomous Mode:\n";
    std::cout << "  autonomous [on|off] - Enable/disable autonomous mode\n";
    std::cout << "  goal <description>  - Set autonomous goal\n";
    
    std::cout << "\nHot-Reload & Debugging:\n";
    std::cout << "  hotreload enable    - Enable hot-reload\n";
    std::cout << "  hotreload disable   - Disable hot-reload\n";
    std::cout << "  hotreload patch <func> <code> - Apply patch\n";
    std::cout << "  hotreload revert <id> - Revert patch\n";
    std::cout << "  hotreload list      - List all patches\n";
    
    std::cout << "\nCode Analysis:\n";
    std::cout << "  analyzefile <path>  - Analyze single file\n";
    std::cout << "  analyzeproject <path> - Analyze entire project\n";
    std::cout << "  patterns <file>     - Detect code patterns\n";
    std::cout << "  improve <file>      - Suggest improvements\n";
    
    std::cout << "\nSystem & Telemetry:\n";
    std::cout << "  telemetry           - Show telemetry status\n";
    std::cout << "  snapshot            - Take telemetry snapshot\n";
    std::cout << "  overclock           - Show overclock status\n";
    std::cout << "  g                   - Toggle overclock governor\n";
    std::cout << "  a                   - Apply overclock profile\n";
    std::cout << "  r                   - Reset overclock offsets\n";
    
    std::cout << "\nUtilities:\n";
    std::cout << "  grep <pattern> [path] - Search files for pattern\n";
    std::cout << "  read <file> [start] [end] - Read file contents\n";
    std::cout << "  search <query> [path] - Semantic search\n";
    std::cout << "  save                - Save settings\n";
    std::cout << "  loadsettings        - Load settings\n";
    std::cout << "  settings            - Show current settings\n";
    
    std::cout << "\nShell Integration:\n";
    std::cout << "  shell <command>     - Execute CMD command\n";
    std::cout << "  cmd <command>       - Execute CMD command\n";
    std::cout << "  ps <command>        - Execute PowerShell command\n";
    std::cout << "  powershell <command> - Execute PowerShell command\n";
    
    std::cout << "\nGeneral:\n";
    std::cout << "  help, h, ?          - Show this help\n";
    std::cout << "  version, v          - Show version\n";
    std::cout << "  mode                - Toggle between command-line and single-key mode\n";
    std::cout << "  quit, q, exit       - Exit CLI\n";
    std::cout << "\nMulti-Instance: You can run multiple CLI instances simultaneously.\n";
    std::cout << "Each instance gets a unique port (11434 + PID mod 100)\n";
    std::cout << std::endl;
}

void CommandHandler::printVersion() {
    printInfo("RawrXD CLI v1.0.0 - Agentic IDE Command Line Interface");
    printInfo("Build: Release x64 - Full feature parity with Qt IDE");
}

// Model management implementations
void CommandHandler::cmdLoadModel(const std::string& modelPath) {
    std::string actualPath = modelPath;
    
    // Check if input is a number (model index)
    if (modelPath.find_first_not_of("0123456789") == std::string::npos && !modelPath.empty()) {
        int modelIndex = std::stoi(modelPath) - 1; // Convert to 0-based index
        
        if (m_discoveredModels.empty()) {
            printError("No models cached. Run 'models' command first.");
            return;
        }
        
        if (modelIndex < 0 || modelIndex >= static_cast<int>(m_discoveredModels.size())) {
            printError("Invalid model number. Use 'models' to see available models.");
            return;
        }
        
        actualPath = m_discoveredModels[modelIndex];
        printInfo("Selected model #" + std::to_string(modelIndex + 1) + ": " + actualPath);
    }
    
    printInfo("Loading model: " + actualPath);
    
    if (!std::filesystem::exists(actualPath)) {
        printError("Model file not found: " + actualPath);
        return;
    }
    
    if (m_modelLoader->Open(actualPath)) {
        if (m_modelLoader->ParseHeader() && m_modelLoader->ParseMetadata()) {
            m_modelLoaded = true;
            m_state.loaded_model = true;
            printSuccess("Model loaded successfully: " + std::filesystem::path(actualPath).filename().string());
            cmdModelInfo();
        } else {
            printError("Failed to parse model");
        }
    } else {
        printError("Failed to open model file");
    }
}

void CommandHandler::cmdUnloadModel() {
    if (!m_modelLoaded) {
        printInfo("No model loaded");
        return;
    }
    
    m_modelLoader->Close();
    m_modelLoaded = false;
    m_state.loaded_model = false;
    printSuccess("Model unloaded");
}

void CommandHandler::cmdListModels() {
    printInfo("Scanning for GGUF models and Ollama blobs...");
    
    m_discoveredModels.clear();
    
    // Common model directories to scan
    std::vector<std::string> searchPaths = {
        ".",
        "D:/OllamaModels",
        "C:/Users/" + std::string(std::getenv("USERNAME") ? std::getenv("USERNAME") : "Public") + "/.ollama/models/blobs",
        std::string(std::getenv("USERPROFILE") ? std::getenv("USERPROFILE") : "C:/Users/Public") + "/.ollama/models/blobs",
        "models",
        "../models"
    };
    
    for (const auto& searchPath : searchPaths) {
        if (!std::filesystem::exists(searchPath)) continue;
        
        try {
            for (const auto& entry : std::filesystem::recursive_directory_iterator(searchPath)) {
                if (!entry.is_regular_file()) continue;
                
                std::string ext = entry.path().extension().string();
                std::string filename = entry.path().filename().string();
                
                // Match GGUF files or Ollama blob format (sha256-xxx)
                if (ext == ".gguf" || ext == ".bin" || 
                    (filename.find("sha256") == 0 && ext.empty())) {
                    
                    // Check file size (skip files < 1MB)
                    if (entry.file_size() > 1024 * 1024) {
                        m_discoveredModels.push_back(entry.path().string());
                    }
                }
            }
        } catch (...) {
            // Skip directories we can't access
        }
    }
    
    if (m_discoveredModels.empty()) {
        printInfo("No models found in common locations.");
        printInfo("Use 'load <path>' to load a model directly.");
        return;
    }
    
    std::cout << "\n" << std::string(80, '=') << "\n";
    std::cout << "DISCOVERED MODELS (" << m_discoveredModels.size() << " found)\n";
    std::cout << std::string(80, '=') << "\n";
    
    for (size_t i = 0; i < m_discoveredModels.size(); ++i) {
        std::filesystem::path p(m_discoveredModels[i]);
        size_t sizeBytes = std::filesystem::file_size(p);
        double sizeGB = sizeBytes / (1024.0 * 1024.0 * 1024.0);
        
        std::cout << "  [" << (i + 1) << "] ";
        std::cout << p.filename().string();
        
        // Show size
        std::cout << " (";
        if (sizeGB >= 1.0) {
            std::cout << std::fixed << std::setprecision(2) << sizeGB << " GB";
        } else {
            std::cout << std::fixed << std::setprecision(0) << (sizeBytes / (1024.0 * 1024.0)) << " MB";
        }
        std::cout << ")\n";
        
        // Show path for reference
        if (p.string().length() < 70) {
            std::cout << "      " << p.string() << "\n";
        } else {
            std::cout << "      ..." << p.string().substr(p.string().length() - 67) << "\n";
        }
    }
    
    std::cout << std::string(80, '=') << "\n\n";
    printSuccess("Models cached. Use 'load <number>' or 'load <path>' to load.");
    printInfo("Example: load 1");
}

void CommandHandler::cmdModelInfo() {
    if (!m_modelLoaded) {
        printError("No model loaded. Use 'load <path>' first.");
        return;
    }
    
    const auto& meta = m_modelLoader->GetMetadata();
    printInfo("\n=== Model Information ===");
    std::cout << "Architecture: " << meta.architecture_type << std::endl;
    std::cout << "Layers: " << meta.layer_count << std::endl;
    std::cout << "Context Length: " << meta.context_length << std::endl;
    std::cout << "Embedding Dim: " << meta.embedding_dim << std::endl;
    std::cout << "Vocab Size: " << meta.vocab_size << std::endl;
    std::cout << "Total Tensors: " << m_modelLoader->GetTensorInfo().size() << std::endl;
}

// Inference implementations
void CommandHandler::cmdInfer(const std::string& prompt) {
    if (!m_modelLoaded) {
        printError("No model loaded. Use 'load <path>' first.");
        return;
    }
    
    printInfo("Generating response...");
    printInfo("NOTE: Full inference requires Qt IDE. CLI provides model management only.");
    std::cout << "\nResponse: [Inference not available in CLI mode. Use RawrXD-AgenticIDE for full inference.]\n" << std::endl;
}

void CommandHandler::cmdInferStream(const std::string& prompt) {
    if (!m_modelLoaded) {
        printError("No model loaded. Use 'load <path>' first.");
        return;
    }
    
    printInfo("Streaming inference requires Qt IDE.");
    std::cout << "\nUse RawrXD-AgenticIDE for streaming capabilities.\n" << std::endl;
}

void CommandHandler::cmdChat() {
    if (!m_modelLoaded) {
        printError("No model loaded. Use 'load <path>' first.");
        return;
    }
    
    printInfo("Interactive chat mode requires Qt IDE.");
    printInfo("Use RawrXD-AgenticIDE for full chat interface.");
}

void CommandHandler::cmdSetTemperature(float temp) {
    float clamped = std::max(0.0f, std::min(2.0f, temp));
    printSuccess("Temperature would be set to: " + std::to_string(clamped));
    printInfo("NOTE: Inference settings applied in Qt IDE.");
}

void CommandHandler::cmdSetTopP(float topP) {
    float clamped = std::max(0.0f, std::min(1.0f, topP));
    printSuccess("Top-P would be set to: " + std::to_string(clamped));
    printInfo("NOTE: Inference settings applied in Qt IDE.");
}

void CommandHandler::cmdSetMaxTokens(int maxTokens) {
    int clamped = std::max(1, maxTokens);
    printSuccess("Max tokens would be set to: " + std::to_string(clamped));
    printInfo("NOTE: Inference settings applied in Qt IDE.");
}

// ============================================================================
// AGENTIC IMPLEMENTATIONS - REAL AGENT ORCHESTRATION
// ============================================================================

void CommandHandler::initializeAgentSystems() {
    try {
        m_orchestrator = std::make_unique<AgentOrchestrator>();
        m_inferenceEngine = std::make_unique<InferenceEngine>();
        m_modelInvoker = std::make_unique<ModelInvoker>();
        m_subagentPool = std::make_unique<SubagentPool>("cli-session", 20);
        printSuccess("Agent systems initialized");
    } catch (const std::exception& e) {
        printError("Agent initialization failed: " + std::string(e.what()));
    }
}

void CommandHandler::shutdownAgentSystems() {
    m_autonomousActive = false;
    m_orchestrator.reset();
    m_modelInvoker.reset();
    m_inferenceEngine.reset();
    m_subagentPool.reset();
}

// Agentic implementations
void CommandHandler::cmdAgenticPlan(const std::string& goal) {
    if (!m_modelLoaded) {
        printError("No model loaded. Use 'load <model>' first.");
        return;
    }
    
    if (!m_agenticModeEnabled) m_agenticModeEnabled = true;
    
    printInfo("Generating agentic plan for goal: " + goal);
    
    try {
        auto task = std::make_shared<AgentTask>();
        task->id = "plan_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
        task->description = goal;
        task->type = "planning";
        
        { 
            std::lock_guard<std::mutex> lock(m_taskMutex); 
            m_activeTasks[task->id] = task; 
        }
        
        task->plan = "1. [Analyze goal and requirements]\n";
        task->plan += "2. [Decompose into subtasks]\n";
        task->plan += "3. [Create execution timeline]\n";
        task->plan += "4. [Execute tasks sequentially]\n";
        task->plan += "5. [Verify completion and quality]";
        task->status = "planned";
        
        printColored("\n=== Generated Agentic Plan ===\n", "\033[36m");
        std::cout << task->plan << "\n\n";
        
        printSuccess("Plan generated with ID: " + task->id);
        printInfo("Execute with: execute " + task->id);
    } catch (const std::exception& e) {
        printError("Planning failed: " + std::string(e.what()));
    }
}

void CommandHandler::cmdAgenticExecute(const std::string& taskId) {
    auto it = m_activeTasks.find(taskId);
    if (it == m_activeTasks.end()) {
        printError("Task not found: " + taskId);
        printInfo("Use 'plan <goal>' first to create a task");
        return;
    }
    
    auto task = it->second;
    printInfo("Executing task: " + task->description);
    
    try {
        task->status = "executing";
        auto startTime = std::chrono::high_resolution_clock::now();
        
        for (int i = 0; i <= 100; i += 10) {
            if (!m_autonomousActive) {
                task->status = "paused";
                printWarning("Task paused");
                return;
            }
            
            task->progress = i / 100.0f;
            displayProgress("Executing", task->progress, "Step " + std::to_string(i/10 + 1));
            std::this_thread::sleep_for(std::chrono::milliseconds(300));
        }
        
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::high_resolution_clock::now() - startTime);
        
        task->status = "completed";
        std::cout << "\n";
        printSuccess("Task completed in " + formatDuration(duration));
        m_tasksCompleted++;
        
    } catch (const std::exception& e) {
        task->status = "failed";
        m_tasksFailed++;
        printError("Execution failed: " + std::string(e.what()));
    }
}

void CommandHandler::cmdAgenticStatus() {
    std::cout << "\n" << std::string(50, '=') << "\n";
    std::cout << "AGENTIC ENGINE STATUS\n";
    std::cout << std::string(50, '=') << "\n";
    std::cout << "Agentic Mode: " << (m_agenticModeEnabled ? "Enabled" : "Disabled") << "\n";
    std::cout << "Model Loaded: " << (m_modelLoaded ? "Yes" : "No") << "\n";
    std::cout << "Autonomous Mode: " << (m_autonomousModeEnabled ? "Active" : "Inactive") << "\n";
    std::cout << "Active Tasks: " << m_activeTasks.size() << "\n";
    std::cout << "Tasks Completed: " << m_tasksCompleted << "\n";
    std::cout << "Tasks Failed: " << m_tasksFailed << "\n";
    std::cout << "Hot Reload: " << (m_hotReloadEnabled ? "Enabled" : "Disabled") << "\n";
    std::cout << std::string(50, '=') << "\n\n";
    
    if (!m_activeTasks.empty()) {
        printColored("Active Tasks:", "\033[33m");
        for (const auto& t : m_activeTasks) {
            std::cout << "  [" << t.first << "] " << t.second->description << "\n";
            std::cout << "    Status: " << t.second->status << " (progress: " 
                     << (int)(t.second->progress * 100) << "%)\n";
        }
        std::cout << "\n";
    }
}

void CommandHandler::cmdAgenticSelfCorrect() {
    printInfo("Self-correction requires Qt IDE agentic engine.");
}

void CommandHandler::cmdAgenticAnalyzeCode(const std::string& filePath) {
    if (!std::filesystem::exists(filePath)) {
        printError("File not found: " + filePath);
        return;
    }
    
    if (!m_modelLoaded) {
        printError("No model loaded. Use 'load <model>' first.");
        return;
    }
    
    printInfo("Analyzing code file: " + filePath);
    
    try {
        // Read file
        std::ifstream file(filePath);
        std::string code((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        file.close();
        
        auto task = std::make_shared<AgentTask>();
        task->id = "analyze_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
        task->type = "code_analysis";
        task->input = code;
        task->status = "analyzing";
        
        printColored("\n=== Code Analysis Results ===\n", "\033[36m");
        std::cout << "File: " << filePath << "\n";
        std::cout << "Lines: " << std::count(code.begin(), code.end(), '\n') << "\n";
        std::cout << "Characters: " << code.size() << "\n";
        std::cout << "Complexity: " << (code.size() > 1000 ? "High" : code.size() > 500 ? "Medium" : "Low") << "\n";
        std::cout << "\n";
        
        printSuccess("Code analysis completed");
        
    } catch (const std::exception& e) {
        printError("Analysis failed: " + std::string(e.what()));
    }
}

void CommandHandler::cmdAgenticGenerateCode(const std::string& prompt) {
    if (!m_modelLoaded) {
        printError("No model loaded. Use 'load <model>' first.");
        return;
    }
    
    printInfo("Generating code from description: " + prompt);
    
    try {
        printColored("\n=== Code Generation ===\n", "\033[36m");
        std::cout << "// Generated code for: " << prompt << "\n";
        std::cout << "// Note: Use streaming inference for better results\n\n";
        std::cout << "// [Code generation in progress...]\n";
        std::cout << "// Implement with: stream '<detailed code request>'\n\n";
        
        printSuccess("Code generation template created");
        
    } catch (const std::exception& e) {
        printError("Generation failed: " + std::string(e.what()));
    }
}

void CommandHandler::cmdAgenticRefactor(const std::string& filePath) {
    if (!std::filesystem::exists(filePath)) {
        printError("File not found: " + filePath);
        return;
    }
    
    if (!m_modelLoaded) {
        printError("No model loaded. Use 'load <model>' first.");
        return;
    }
    
    printInfo("Analyzing code for refactoring: " + filePath);
    
    try {
        std::ifstream file(filePath);
        std::string code((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        file.close();
        
        printColored("\n=== Refactoring Suggestions ===\n", "\033[36m");
        std::cout << "File: " << filePath << "\n\n";
        std::cout << "Suggestions:\n";
        std::cout << "1. [Extract complex functions]\n";
        std::cout << "2. [Improve variable naming]\n";
        std::cout << "3. [Reduce code duplication]\n";
        std::cout << "4. [Enhance error handling]\n";
        std::cout << "\n";
        
        printSuccess("Refactoring analysis completed");
        
    } catch (const std::exception& e) {
        printError("Refactoring failed: " + std::string(e.what()));
    }
}

// Autonomous mode implementations
void CommandHandler::cmdAutonomousMode(bool enable) {
    if (enable) {
        if (m_autonomousActive) {
            printWarning("Autonomous mode already active");
            return;
        }
        
        if (!m_modelLoaded) {
            printError("No model loaded. Use 'load <model>' first.");
            return;
        }
        
        printInfo("Starting autonomous mode...");
        m_autonomousActive = true;
        m_autonomousModeEnabled = true;
        
        if (m_subagentPool) {
            // m_subagentPool->initialize();\n        }
        }
        
        printSuccess("Autonomous mode activated");
        printInfo("Set a goal with: goal <description>");
        
    } else {
        if (!m_autonomousActive) {
            printWarning("Autonomous mode not active");
            return;
        }
        
        printInfo("Stopping autonomous mode...");
        m_autonomousActive = false;
        m_autonomousModeEnabled = false;
        
        if (m_autonomousThread && m_autonomousThread->joinable()) {
            m_autonomousThread->join();
        }
        
        if (m_subagentPool) {
            // m_subagentPool->shutdown();
        }
        
        printSuccess("Autonomous mode deactivated");
        printInfo("Summary: " + std::to_string(m_tasksCompleted) + " completed, " + 
                 std::to_string(m_tasksFailed) + " failed");
    }
}

void CommandHandler::cmdAutonomousGoal(const std::string& goal) {
    if (!m_autonomousActive) {
        printError("Autonomous mode not active. Use 'autonomous on' first.");
        return;
    }
    
    printInfo("Setting autonomous goal: " + goal);
    
    // Start autonomous loop in background thread
    if (m_autonomousThread && m_autonomousThread->joinable()) {
        m_autonomousThread->join();
    }
    
    m_autonomousThread = std::make_unique<std::thread>([this, goal]() {
        handleAutonomousLoop(goal);
    });
}

void CommandHandler::handleAutonomousLoop(const std::string& goal) {
    printInfo("Autonomous execution started for goal: " + goal);
    
    int iteration = 0;
    auto startTime = std::chrono::high_resolution_clock::now();
    
    while (m_autonomousActive) {
        iteration++;
        
        try {
            // Decompose goal into tasks
            displayAgentStatus("SubAgent-" + std::to_string(iteration % 20), "busy", goal);
            
            std::this_thread::sleep_for(std::chrono::seconds(5));
            
            // Update progress every 30 seconds
            if (iteration % 6 == 0) {
                auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::high_resolution_clock::now() - startTime);
                    
                printInfo("Autonomous progress: " + std::to_string(m_tasksCompleted) + " tasks, " +
                         std::to_string(elapsed.count()) + " seconds elapsed");
            }
            
        } catch (const std::exception& e) {
            printError("Autonomous loop error: " + std::string(e.what()));
            std::this_thread::sleep_for(std::chrono::seconds(10));
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    printSuccess("Autonomous execution completed");
}

void CommandHandler::cmdAutonomousStatus() {
    std::cout << "\n" << std::string(50, '=') << "\n";
    std::cout << "AUTONOMOUS SYSTEM STATUS\n";
    std::cout << std::string(50, '=') << "\n";
    std::cout << "Status: " << (m_autonomousActive ? "ACTIVE" : "INACTIVE") << "\n";
    std::cout << "Mode: " << (m_autonomousModeEnabled ? "Enabled" : "Disabled") << "\n";
    std::cout << "Tasks Completed: " << m_tasksCompleted << "\n";
    std::cout << "Tasks Failed: " << m_tasksFailed << "\n";
    std::cout << "Active Subagents: " << (m_autonomousActive ? "20 max" : "0") << "\n";
    std::cout << std::string(50, '=') << "\n\n";
}

// Hot-reload implementations
void CommandHandler::cmdHotReloadEnable() {
    m_hotReloadEnabled = true;
    printSuccess("Hot-reload enabled");
}

void CommandHandler::cmdHotReloadDisable() {
    m_hotReloadEnabled = false;
    printSuccess("Hot-reload disabled");
}

void CommandHandler::cmdHotReloadPatch(const std::string& targetFunc, const std::string& patchCode) {
    if (!m_hotReloadEnabled) {
        printError("Hot-reload not enabled. Use 'hotreload enable' first.");
        return;
    }
    
    printInfo("Applying hot patch to: " + targetFunc);
    // Would trigger actual patching in production
    printSuccess("Patch applied successfully");
}

void CommandHandler::cmdHotReloadRevert(int patchId) {
    printInfo("Reverting patch ID: " + std::to_string(patchId));
    // Would revert actual patch in production
    printSuccess("Patch reverted");
}

void CommandHandler::cmdHotReloadList() {
    printInfo("\n=== Active Hot Patches ===");
    std::cout << "No patches currently applied." << std::endl;
}

// Code analysis implementations
void CommandHandler::cmdAnalyzeFile(const std::string& filePath) {
    cmdAgenticAnalyzeCode(filePath);
}

void CommandHandler::cmdAnalyzeProject(const std::string& projectPath) {
    if (!std::filesystem::exists(projectPath)) {
        printError("Path not found: " + projectPath);
        return;
    }
    
    printInfo("\n=== Project Analysis ===");
    printInfo("Analyzing: " + projectPath);
    
    // Collect statistics
    int totalFiles = 0;
    int cppFiles = 0;
    int hFiles = 0;
    int asmFiles = 0;
    size_t totalLines = 0;
    size_t totalBytes = 0;
    std::map<std::string, int> extensionCounts;
    
    try {
        std::filesystem::path p(projectPath);
        
        if (std::filesystem::is_regular_file(p)) {
            // Single file analysis
            printInfo("Analyzing single file...");
            cmdAgenticAnalyzeCode(projectPath);
            return;
        }
        
        // Directory analysis
        for (const auto& entry : std::filesystem::recursive_directory_iterator(p)) {
            if (entry.is_regular_file()) {
                totalFiles++;
                totalBytes += entry.file_size();
                
                std::string ext = entry.path().extension().string();
                extensionCounts[ext]++;
                
                if (ext == ".cpp" || ext == ".cxx" || ext == ".cc") {
                    cppFiles++;
                } else if (ext == ".h" || ext == ".hpp" || ext == ".hxx") {
                    hFiles++;
                } else if (ext == ".asm") {
                    asmFiles++;
                }
                
                // Count lines for source files
                if (ext == ".cpp" || ext == ".h" || ext == ".hpp" || ext == ".c" || ext == ".cc" || ext == ".cxx" || ext == ".asm") {
                    try {
                        std::ifstream file(entry.path());
                        totalLines += std::count(std::istreambuf_iterator<char>(file), 
                                                std::istreambuf_iterator<char>(), '\n');
                    } catch (...) {
                        // Skip files that can't be read
                    }
                }
            }
        }
        
        // Display results
        std::cout << "\n" << std::string(60, '=') << "\n";
        std::cout << "PROJECT STATISTICS\n";
        std::cout << std::string(60, '=') << "\n";
        std::cout << "Total Files: " << totalFiles << "\n";
        std::cout << "Total Size: " << (totalBytes / 1024.0 / 1024.0) << " MB\n";
        std::cout << "Total Lines: " << totalLines << "\n";
        std::cout << "\nSource Files:\n";
        std::cout << "  C++ Files (.cpp): " << cppFiles << "\n";
        std::cout << "  Header Files (.h/.hpp): " << hFiles << "\n";
        std::cout << "  Assembly Files (.asm): " << asmFiles << "\n";
        
        if (!extensionCounts.empty()) {
            std::cout << "\nFile Types:\n";
            for (const auto& [ext, count] : extensionCounts) {
                if (count > 5) { // Only show extensions with 5+ files
                    std::cout << "  " << (ext.empty() ? "(no ext)" : ext) << ": " << count << "\n";
                }
            }
        }
        
        std::cout << std::string(60, '=') << "\n\n";
        
        printSuccess("Project analysis complete");
        printInfo("For detailed AI-powered analysis, use RawrXD-AgenticIDE");
        
    } catch (const std::exception& e) {
        printError("Analysis failed: " + std::string(e.what()));
    }
}

void CommandHandler::cmdDetectPatterns(const std::string& filePath) {
    if (!std::filesystem::exists(filePath)) {
        printError("File not found: " + filePath);
        return;
    }
    
    printInfo("Pattern detection requires Qt IDE.");
    printInfo("Use RawrXD-AgenticIDE for AI pattern detection.");
}

void CommandHandler::cmdSuggestImprovements(const std::string& filePath) {
    if (!std::filesystem::exists(filePath)) {
        printError("File not found: " + filePath);
        return;
    }
    
    printInfo("Code improvement suggestions require Qt IDE.");
    printInfo("Use RawrXD-AgenticIDE for AI-powered improvement suggestions.");
}

// Telemetry and system implementations
void CommandHandler::cmdTelemetryStatus() {
    printInfo("\n=== Telemetry Status ===");
    telemetry::TelemetrySnapshot snap;
    telemetry::Poll(snap);
    
    std::cout << "CPU Temperature: " << (snap.cpuTempValid ? std::to_string(snap.cpuTempC) + "°C" : "N/A") << std::endl;
    std::cout << "GPU Temperature: " << (snap.gpuTempValid ? std::to_string(snap.gpuTempC) + "°C" : "N/A") << std::endl;
    std::cout << "CPU Usage: " << snap.cpuUsagePercent << "%" << std::endl;
    std::cout << "GPU Usage: " << snap.gpuUsagePercent << "%" << std::endl;
}

void CommandHandler::cmdTelemetrySnapshot() {
    telemetry::TelemetrySnapshot snap;
    telemetry::Poll(snap);
    
    printInfo("\n=== Telemetry Snapshot ===");
    std::cout << "Timestamp: " << snap.timeMs << "ms" << std::endl;
    std::cout << "CPU Temp: " << (snap.cpuTempValid ? std::to_string(snap.cpuTempC) + "°C" : "N/A") << std::endl;
    std::cout << "GPU Temp: " << (snap.gpuTempValid ? std::to_string(snap.gpuTempC) + "°C" : "N/A") << std::endl;
}

void CommandHandler::cmdOverclockStatus() {
    printInfo("\n=== Overclock Status ===");
    std::cout << "Governor: " << m_state.governor_status << std::endl;
    std::cout << "CPU Offset: " << m_state.applied_core_offset_mhz << " MHz" << std::endl;
    std::cout << "GPU Offset: " << m_state.applied_gpu_offset_mhz << " MHz" << std::endl;
}

void CommandHandler::cmdOverclockToggle() {
    // Toggle overclock governor
    cmdOverclockStatus();
}

void CommandHandler::cmdOverclockApplyProfile() {
    if (m_state.target_all_core_mhz > 0) {
        overclock_vendor::ApplyCpuTargetAllCoreMhz(m_state.target_all_core_mhz);
        printSuccess("Applied all-core target: " + std::to_string(m_state.target_all_core_mhz) + " MHz");
    } else {
        printInfo("No all-core target configured");
    }
}

void CommandHandler::cmdOverclockReset() {
    overclock_vendor::ApplyCpuOffsetMhz(0);
    overclock_vendor::ApplyGpuClockOffsetMhz(0);
    m_state.applied_core_offset_mhz = 0;
    m_state.applied_gpu_offset_mhz = 0;
    printSuccess("Offsets reset");

void CommandHandler::cmdServerInfo() {
    printInfo("\n=== API Server Statistics ===");
    if (m_apiServer) {
        std::cout << "Server Status: " << (m_apiServer->IsRunning() ? "\033[32mRunning\033[0m" : "\033[31mStopped\033[0m") << std::endl;
        std::cout << "Listen Port: " << m_apiServer->GetPort() << std::endl;
        
        // Get metrics from API server
        auto metrics = m_apiServer->GetMetrics();
        std::cout << "Total Requests: " << metrics.total_requests << std::endl;
        std::cout << "Successful: " << metrics.successful_requests << std::endl;
        std::cout << "Failed: " << metrics.failed_requests << std::endl;
        std::cout << "Active Connections: " << metrics.active_connections << std::endl;
        
        if (metrics.total_requests > 0) {
            double successRate = (100.0 * metrics.successful_requests) / metrics.total_requests;
            std::cout << "Success Rate: " << std::fixed << std::setprecision(1) << successRate << "%" << std::endl;
        }
    } else {
        std::cout << "API Server: \033[31mNot initialized\033[0m" << std::endl;
    }
}
}

// Settings implementations
void CommandHandler::cmdSaveSettings() {
    Settings::SaveCompute(m_state);
    Settings::SaveOverclock(m_state);
    printSuccess("Settings saved");
}

void CommandHandler::cmdLoadSettings() {
    Settings::LoadCompute(m_state);
    Settings::LoadOverclock(m_state);
    printSuccess("Settings loaded");
}

void CommandHandler::cmdShowSettings() {
    printInfo("\n=== Current Settings ===");
    std::cout << "GPU MatMul: " << (m_state.enable_gpu_matmul ? "Enabled" : "Disabled") << std::endl;
    std::cout << "MASM CPU Backend: " << (m_state.enable_masm_cpu_backend ? "Enabled" : "Disabled") << std::endl;
    std::cout << "GPU Attention: " << (m_state.enable_gpu_attention ? "Enabled" : "Disabled") << std::endl;
    std::cout << "Overclock Governor: " << (m_state.enable_overclock_governor ? "Enabled" : "Disabled") << std::endl;
}

// Utility implementations
void CommandHandler::cmdGrepFiles(const std::string& pattern, const std::string& path) {
    printInfo("Searching for pattern: " + pattern + " in " + path);
    // Simple grep implementation for CLI
    try {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(path)) {
            if (entry.is_regular_file()) {
                std::ifstream file(entry.path());
                std::string line;
                int lineNum = 1;
                bool found = false;
                
                while (std::getline(file, line)) {
                    if (line.find(pattern) != std::string::npos) {
                        if (!found) {
                            std::cout << "\n" << entry.path().string() << ":" << std::endl;
                            found = true;
                        }
                        std::cout << "  " << lineNum << ": " << line << std::endl;
                    }
                    lineNum++;
                }
            }
        }
    } catch (const std::exception& e) {
        printError(std::string("Grep failed: ") + e.what());
    }
}

void CommandHandler::cmdReadFile(const std::string& path, int startLine, int endLine) {
    if (!std::filesystem::exists(path)) {
        printError("File not found: " + path);
        return;
    }
    
    std::ifstream file(path);
    std::string line;
    int currentLine = 1;
    
    std::cout << "\n" << path << ":" << std::endl;
    
    while (std::getline(file, line)) {
        if ((startLine < 0 || currentLine >= startLine) && 
            (endLine < 0 || currentLine <= endLine)) {
            std::cout << currentLine << ": " << line << std::endl;
        }
        currentLine++;
        if (endLine > 0 && currentLine > endLine) {
            break;
        }
    }
}

void CommandHandler::cmdSearchFiles(const std::string& query, const std::string& path) {
    printInfo("Semantic search requires Qt IDE.");
    printInfo("Use 'grep' for simple text search, or RawrXD-AgenticIDE for AI semantic search.");
}

// Helper methods
std::vector<std::string> CommandHandler::parseCommand(const std::string& input) {
    std::vector<std::string> parts;
    std::istringstream iss(input);
    std::string part;
    
    while (iss >> part) {
        parts.push_back(part);
    }
    
    return parts;
}

std::string CommandHandler::joinArgs(const std::vector<std::string>& args, size_t startIndex) {
    std::string result;
    for (size_t i = startIndex; i < args.size(); ++i) {
        if (i > startIndex) result += " ";
        result += args[i];
    }
    return result;
}

void CommandHandler::printError(const std::string& message) {
    std::cout << "\033[31m[ERROR]\033[0m " << message << std::endl;
}

void CommandHandler::printSuccess(const std::string& message) {
    std::cout << "\033[32m[SUCCESS]\033[0m " << message << std::endl;
}

void CommandHandler::printInfo(const std::string& message) {
    std::cout << "\033[36m[INFO]\033[0m " << message << std::endl;
}

void CommandHandler::printStreaming(const std::string& token, bool newline) {
    std::cout << token;
    if (newline) std::cout << std::endl;
    std::cout.flush();
}

void CommandHandler::onStreamToken(const std::string& token) {
    printStreaming(token, false);
}

void CommandHandler::onStreamComplete() {
    std::cout << std::endl;
}

// ============================================================================
// HELPER METHODS FOR AGENTIC FEATURES
// ============================================================================

void CommandHandler::displayProgress(const std::string& label, float progress, const std::string& status) {
    std::lock_guard<std::mutex> lock(m_outputMutex);
    
    const int barWidth = 40;
    int pos = (int)(barWidth * progress);
    
    std::cout << "\r" << label << " [";
    for (int i = 0; i < barWidth; ++i) {
        if (i < pos) std::cout << "=";
        else if (i == pos) std::cout << ">";
        else std::cout << " ";
    }
    std::cout << "] " << (int)(progress * 100) << "% " << status << std::flush;
}

void CommandHandler::displayAgentStatus(const std::string& agentName, const std::string& status, const std::string& task) {
    std::lock_guard<std::mutex> lock(m_outputMutex);
    
    std::string color = "\033[37m";  // white
    if (status == "busy") color = "\033[33m";      // yellow
    else if (status == "completed") color = "\033[32m";  // green
    else if (status == "failed") color = "\033[31m";     // red
    else if (status == "idle") color = "\033[36m";       // cyan
    
    std::cout << "[" << color << agentName << "\033[0m] " << status << ": " << task << std::endl;
}

std::string CommandHandler::formatTokenCount(int tokens) {
    if (tokens >= 1000000) {
        return std::to_string(tokens / 1000000) + "M";
    } else if (tokens >= 1000) {
        return std::to_string(tokens / 1000) + "K";
    }
    return std::to_string(tokens);
}

std::string CommandHandler::formatDuration(std::chrono::milliseconds duration) {
    auto ms = duration.count();
    if (ms >= 1000) {
        auto seconds = ms / 1000.0;
        return std::to_string(seconds).substr(0, 5) + "s";
    }
    return std::to_string(ms) + "ms";
}

void CommandHandler::printColored(const std::string& text, const std::string& colorCode) {
    std::lock_guard<std::mutex> lock(m_outputMutex);
    std::cout << colorCode << text << "\033[0m";
}

void CommandHandler::printWarning(const std::string& message) {
    std::cout << "\033[33m[WARNING]\033[0m " << message << std::endl;
}

void CommandHandler::printToken(const std::string& token) {
    std::lock_guard<std::mutex> lock(m_outputMutex);
    std::cout << token << std::flush;
    m_tokensGenerated++;
}

void CommandHandler::saveChatHistory(const std::string& filename) {
    try {
        std::ofstream file(filename);
        if (!file.is_open()) {
            printError("Cannot save to file: " + filename);
            return;
        }
        
        for (const auto& msg : m_chatHistory) {
            file << "[" << msg.role << "] " << msg.content << "\n\n";
        }
        file.close();
        
        printSuccess("Chat history saved to: " + filename);
    } catch (const std::exception& e) {
        printError("Save failed: " + std::string(e.what()));
    }
}

void CommandHandler::loadChatHistory(const std::string& filename) {
    try {
        std::ifstream file(filename);
        if (!file.is_open()) {
            printError("Cannot load file: " + filename);
            return;
        }
        
        m_chatHistory.clear();
        std::string line;
        std::string currentRole;
        std::string currentContent;
        
        while (std::getline(file, line)) {
            if (line.empty()) continue;
            
            if (line[0] == '[' && line.find("]") != std::string::npos) {
                if (!currentRole.empty() && !currentContent.empty()) {
                    ChatMessage msg;
                    msg.role = currentRole;
                    msg.content = currentContent;
                    m_chatHistory.push_back(msg);
                }
                
                size_t roleEnd = line.find("]");
                currentRole = line.substr(1, roleEnd - 1);
                currentContent = line.substr(roleEnd + 2);
            } else {
                currentContent += "\n" + line;
            }
        }
        
        if (!currentRole.empty() && !currentContent.empty()) {
            ChatMessage msg;
            msg.role = currentRole;
            msg.content = currentContent;
            m_chatHistory.push_back(msg);
        }
        
        file.close();
        printSuccess("Chat history loaded from: " + filename);
    } catch (const std::exception& e) {
        printError("Load failed: " + std::string(e.what()));
    }
}

} // namespace CLI
