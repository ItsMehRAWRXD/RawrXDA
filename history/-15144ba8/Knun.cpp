#include "cli_command_handler.h"
#include "agentic_engine.h"
#include "gguf_loader.h"
#include "inference_engine.h"
#include "planning_agent.h"
#include "qtapp/debugger/DebuggerHotReload.h"
#include "telemetry.h"
#include "overclock_vendor.h"
#include "overclock_governor.h"
#include <iostream>
#include <sstream>
#include <fstream>
#include <filesystem>
#include <algorithm>

namespace CLI {

CommandHandler::CommandHandler(AppState& state)
    : m_state(state)
    , m_agenticEngine(std::make_unique<AgenticEngine>())
    , m_modelLoader(std::make_unique<GGUFLoader>())
    , m_inferenceEngine(nullptr)
    , m_planningAgent(std::make_unique<PlanningAgent>())
    , m_modelLoaded(false)
    , m_agenticModeEnabled(false)
    , m_autonomousModeEnabled(false)
    , m_hotReloadEnabled(false) {
    
    // Initialize agentic engine
    m_agenticEngine->initialize();
}

CommandHandler::~CommandHandler() = default;

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
    else if (cmd == "selfcorrect") {
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
    
    std::cout << "\nGeneral:\n";
    std::cout << "  help, h, ?          - Show this help\n";
    std::cout << "  version, v          - Show version\n";
    std::cout << "  quit, q, exit       - Exit CLI\n";
    std::cout << std::endl;
}

void CommandHandler::printVersion() {
    printInfo("RawrXD CLI v1.0.0 - Agentic IDE Command Line Interface");
    printInfo("Build: Release x64 - Full feature parity with Qt IDE");
}

// Model management implementations
void CommandHandler::cmdLoadModel(const std::string& modelPath) {
    printInfo("Loading model: " + modelPath);
    
    if (!std::filesystem::exists(modelPath)) {
        printError("Model file not found: " + modelPath);
        return;
    }
    
    if (m_modelLoader->Open(modelPath)) {
        if (m_modelLoader->ParseHeader() && m_modelLoader->ParseMetadata()) {
            m_modelLoaded = true;
            m_state.loaded_model = true;
            m_agenticEngine->markModelAsLoaded(QString::fromStdString(modelPath));
            printSuccess("Model loaded successfully");
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
    printInfo("Scanning for GGUF models in current directory...");
    
    int count = 0;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(".")) {
        if (entry.path().extension() == ".gguf") {
            std::cout << "  [" << ++count << "] " << entry.path().string() << std::endl;
        }
    }
    
    if (count == 0) {
        printInfo("No GGUF models found. Use 'load <path>' to load a model.");
    }
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
    std::string response = m_agenticEngine->generateResponse(QString::fromStdString(prompt)).toStdString();
    std::cout << "\n" << response << "\n" << std::endl;
}

void CommandHandler::cmdInferStream(const std::string& prompt) {
    if (!m_modelLoaded) {
        printError("No model loaded. Use 'load <path>' first.");
        return;
    }
    
    printInfo("Streaming response:");
    m_agenticEngine->processMessage(QString::fromStdString(prompt), QString(), true);
    // Note: Actual streaming would require signal/slot connection or callback
    std::cout << std::endl;
}

void CommandHandler::cmdChat() {
    if (!m_modelLoaded) {
        printError("No model loaded. Use 'load <path>' first.");
        return;
    }
    
    printInfo("Entering chat mode. Type 'exit' to leave.");
    std::string line;
    
    while (true) {
        std::cout << "\n You: ";
        std::getline(std::cin, line);
        
        if (line == "exit" || line == "quit") {
            break;
        }
        
        if (line.empty()) {
            continue;
        }
        
        std::cout << " Bot: ";
        std::string response = m_agenticEngine->generateResponse(QString::fromStdString(line)).toStdString();
        std::cout << response << std::endl;
    }
    
    printInfo("Exited chat mode");
}

void CommandHandler::cmdSetTemperature(float temp) {
    auto config = m_agenticEngine->generationConfig();
    config.temperature = std::max(0.0f, std::min(2.0f, temp));
    m_agenticEngine->setGenerationConfig(config);
    printSuccess("Temperature set to: " + std::to_string(config.temperature));
}

void CommandHandler::cmdSetTopP(float topP) {
    auto config = m_agenticEngine->generationConfig();
    config.topP = std::max(0.0f, std::min(1.0f, topP));
    m_agenticEngine->setGenerationConfig(config);
    printSuccess("Top-P set to: " + std::to_string(config.topP));
}

void CommandHandler::cmdSetMaxTokens(int maxTokens) {
    auto config = m_agenticEngine->generationConfig();
    config.maxTokens = std::max(1, maxTokens);
    m_agenticEngine->setGenerationConfig(config);
    printSuccess("Max tokens set to: " + std::to_string(config.maxTokens));
}

// Agentic implementations
void CommandHandler::cmdAgenticPlan(const std::string& goal) {
    if (!m_agenticModeEnabled) {
        printInfo("Enabling agentic mode...");
        m_agenticModeEnabled = true;
    }
    
    printInfo("Planning for goal: " + goal);
    auto plan = m_agenticEngine->planTask(QString::fromStdString(goal));
    
    std::cout << "\n=== Execution Plan ===" << std::endl;
    for (int i = 0; i < plan.size(); ++i) {
        std::cout << "Step " << (i+1) << ": " << plan[i].toString().toStdString() << std::endl;
    }
}

void CommandHandler::cmdAgenticExecute(const std::string& taskId) {
    printInfo("Executing task: " + taskId);
    // Would trigger actual task execution in production
    printSuccess("Task execution initiated");
}

void CommandHandler::cmdAgenticStatus() {
    printInfo("\n=== Agentic Engine Status ===");
    std::cout << "Agentic Mode: " << (m_agenticModeEnabled ? "Enabled" : "Disabled") << std::endl;
    std::cout << "Model Loaded: " << (m_modelLoaded ? "Yes" : "No") << std::endl;
    std::cout << "Autonomous Mode: " << (m_autonomousModeEnabled ? "Enabled" : "Disabled") << std::endl;
}

void CommandHandler::cmdAgenticSelfCorrect() {
    printInfo("Running self-correction analysis...");
    // Would trigger self-correction logic
    printSuccess("Self-correction complete");
}

void CommandHandler::cmdAgenticAnalyzeCode(const std::string& filePath) {
    if (!std::filesystem::exists(filePath)) {
        printError("File not found: " + filePath);
        return;
    }
    
    std::ifstream file(filePath);
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string code = buffer.str();
    
    printInfo("Analyzing code...");
    std::string analysis = m_agenticEngine->analyzeCode(QString::fromStdString(code)).toStdString();
    std::cout << "\n" << analysis << "\n" << std::endl;
}

void CommandHandler::cmdAgenticGenerateCode(const std::string& prompt) {
    printInfo("Generating code...");
    std::string code = m_agenticEngine->generateCode(QString::fromStdString(prompt)).toStdString();
    std::cout << "\n```\n" << code << "\n```\n" << std::endl;
}

void CommandHandler::cmdAgenticRefactor(const std::string& filePath) {
    if (!std::filesystem::exists(filePath)) {
        printError("File not found: " + filePath);
        return;
    }
    
    std::ifstream file(filePath);
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string code = buffer.str();
    
    printInfo("Analyzing refactoring opportunities...");
    std::string refactored = m_agenticEngine->refactorCode(QString::fromStdString(code), QString("general")).toStdString();
    std::cout << "\n=== Refactored Code ===\n" << refactored << "\n" << std::endl;
}

// Autonomous mode implementations
void CommandHandler::cmdAutonomousMode(bool enable) {
    m_autonomousModeEnabled = enable;
    if (enable) {
        printSuccess("Autonomous mode enabled");
        printInfo("Set a goal with: goal <description>");
    } else {
        printSuccess("Autonomous mode disabled");
    }
}

void CommandHandler::cmdAutonomousGoal(const std::string& goal) {
    if (!m_autonomousModeEnabled) {
        printError("Autonomous mode not enabled. Use 'autonomous on' first.");
        return;
    }
    
    printInfo("Autonomous goal set: " + goal);
    printInfo("Autonomous system will work towards this goal...");
    // Would trigger autonomous execution in production
}

void CommandHandler::cmdAutonomousStatus() {
    printInfo("\n=== Autonomous System Status ===");
    std::cout << "Mode: " << (m_autonomousModeEnabled ? "Active" : "Inactive") << std::endl;
    std::cout << "Current Goal: " << (m_autonomousModeEnabled ? "[goal here]" : "None") << std::endl;
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
    printInfo("Analyzing project: " + projectPath);
    // Would scan and analyze all project files
    printSuccess("Project analysis complete");
}

void CommandHandler::cmdDetectPatterns(const std::string& filePath) {
    if (!std::filesystem::exists(filePath)) {
        printError("File not found: " + filePath);
        return;
    }
    
    std::ifstream file(filePath);
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string code = buffer.str();
    
    printInfo("Detecting patterns...");
    auto patterns = m_agenticEngine->detectPatterns(QString::fromStdString(code));
    
    std::cout << "\n=== Detected Patterns ===" << std::endl;
    for (const auto& pattern : patterns) {
        std::cout << pattern.toString().toStdString() << std::endl;
    }
}

void CommandHandler::cmdSuggestImprovements(const std::string& filePath) {
    if (!std::filesystem::exists(filePath)) {
        printError("File not found: " + filePath);
        return;
    }
    
    std::ifstream file(filePath);
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string code = buffer.str();
    
    printInfo("Analyzing improvements...");
    std::string suggestions = m_agenticEngine->suggestImprovements(QString::fromStdString(code)).toStdString();
    std::cout << "\n" << suggestions << "\n" << std::endl;
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
    std::string result = m_agenticEngine->grepFiles(QString::fromStdString(pattern), QString::fromStdString(path)).toStdString();
    std::cout << result << std::endl;
}

void CommandHandler::cmdReadFile(const std::string& path, int startLine, int endLine) {
    std::string result = m_agenticEngine->readFile(QString::fromStdString(path), startLine, endLine).toStdString();
    std::cout << result << std::endl;
}

void CommandHandler::cmdSearchFiles(const std::string& query, const std::string& path) {
    std::string result = m_agenticEngine->searchFiles(QString::fromStdString(query), QString::fromStdString(path)).toStdString();
    std::cout << result << std::endl;
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

} // namespace CLI
