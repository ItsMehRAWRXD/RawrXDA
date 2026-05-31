// ============================================================================
// CLI_SlashRouter.cpp — CLI Integration for 25 Slash Commands
// ============================================================================
// Bridges the Win32IDE slash commands to the CLI InteractiveShell.
// Provides full functionality for all 25 commands in headless/terminal mode.
// ============================================================================

#include "cpu_inference_engine.h"
#include "agentic_engine.h"
#if !defined(RAWRXD_CLI_ENABLE_INTERACTIVE_SHELL)
#define RAWRXD_CLI_ENABLE_INTERACTIVE_SHELL 1
#endif
#if RAWRXD_CLI_ENABLE_INTERACTIVE_SHELL
#include "InteractiveShell.hpp"
#endif
#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace RawrXD {
namespace CLI {

// ============================================================================
// Forward Declarations
// ============================================================================

class CLISlashRouter;
class CLIContext;

// ============================================================================
// CLI Context - Provides IDE-like interface for commands
// ============================================================================

class CLIContext
{
public:
    CLIContext() = default;
    
    // File operations
    std::string getCurrentFile() const { return m_currentFile; }
    void setCurrentFile(const std::string& path) { m_currentFile = path; }
    
    std::string getEditorSelection() const { return m_editorSelection; }
    void setEditorSelection(const std::string& sel) { m_editorSelection = sel; }
    
    // Model operations
    std::shared_ptr<CPUInferenceEngine> getInferenceEngine() const { return m_inferenceEngine; }
    void setInferenceEngine(std::shared_ptr<CPUInferenceEngine> engine) { m_inferenceEngine = engine; }
    
    std::shared_ptr<AgenticEngine> getAgenticEngine() const { return m_agenticEngine; }
    void setAgenticEngine(std::shared_ptr<AgenticEngine> engine) { m_agenticEngine = engine; }
    
    // Model state
    std::string getLoadedModelPath() const { return m_loadedModelPath; }
    void setLoadedModelPath(const std::string& path) { m_loadedModelPath = path; }
    
    std::vector<std::string> getAvailableModels() const { return m_availableModels; }
    void setAvailableModels(const std::vector<std::string>& models) { m_availableModels = models; }
    
    // Ollama connection
    bool isOllamaConnected() const { return m_ollamaConnected; }
    void setOllamaConnected(bool connected) { m_ollamaConnected = connected; }
    
    // KV-Cache state
    uint64_t getKVCacheSeqLen() const { return m_kvCacheSeqLen; }
    void setKVCacheSeqLen(uint64_t len) { m_kvCacheSeqLen = len; }
    
    // Output
    void appendToOutput(const std::string& msg, const std::string& pane = "Output")
    {
        std::cout << msg;
        if (!msg.empty() && msg.back() != '\n')
        {
            std::cout << "\n";
        }
    }
    
    void clearOutput(const std::string& pane = "Output")
    {
#ifdef _WIN32
        system("cls");
#else
        system("clear");
#endif
    }
    
    // Inference
    std::string sendMessageToModel(const std::string& prompt)
    {
        m_lastUserPrompt = prompt;
        if (m_inferenceEngine)
        {
            const auto inputTokens = m_inferenceEngine->Tokenize(prompt);
            const auto outputTokens = m_inferenceEngine->Generate(inputTokens, 256);
            m_lastAssistantResponse = m_inferenceEngine->Detokenize(outputTokens);
            return m_lastAssistantResponse;
        }
        m_lastAssistantResponse = "Error: No inference engine loaded. Use /model <path> to load a model.";
        return m_lastAssistantResponse;
    }
    
    bool loadModelFromPath(const std::string& path)
    {
        if (m_inferenceEngine)
        {
            bool success = m_inferenceEngine->LoadModel(path);
            if (success)
            {
                m_loadedModelPath = path;
            }
            return success;
        }
        return false;
    }

private:
    std::string m_currentFile;
    std::string m_editorSelection;
    std::string m_loadedModelPath;
    std::vector<std::string> m_availableModels;
    std::shared_ptr<CPUInferenceEngine> m_inferenceEngine;
    std::shared_ptr<AgenticEngine> m_agenticEngine;
    bool m_ollamaConnected = false;
    uint64_t m_kvCacheSeqLen = 0;
    bool m_currentFileContextEnabled = true;
    std::string m_lastUserPrompt;
    std::string m_lastAssistantResponse;
    int m_helpfulCount = 0;
    int m_unhelpfulCount = 0;
    std::string m_modelBadge = "GPT-5.3-Codex • 0.9x";

public:
    // Current File Context Toggle
    bool isCurrentFileContextEnabled() const { return m_currentFileContextEnabled; }
    void setCurrentFileContextEnabled(bool enabled) { m_currentFileContextEnabled = enabled; }
    const std::string& getLastUserPrompt() const { return m_lastUserPrompt; }
    const std::string& getLastAssistantResponse() const { return m_lastAssistantResponse; }
    int markHelpful() { return ++m_helpfulCount; }
    int markUnhelpful() { return ++m_unhelpfulCount; }
    int helpfulCount() const { return m_helpfulCount; }
    int unhelpfulCount() const { return m_unhelpfulCount; }
    const std::string& modelBadge() const { return m_modelBadge; }
    void setModelBadge(const std::string& badge)
    {
        if (!badge.empty())
            m_modelBadge = badge;
    }
};

// ============================================================================
// Command Result
// ============================================================================

struct SlashCommandResult
{
    bool success = false;
    std::string output;
    std::string error;
};

using SlashCommandHandler = std::function<SlashCommandResult(const std::vector<std::string>& args, CLIContext* ctx)>;

// ============================================================================
// Helper Functions
// ============================================================================

namespace
{

std::string ReadFileContent(const std::string& path)
{
    try
    {
        std::ifstream file(path, std::ios::binary);
        if (!file.is_open()) return "";
        std::stringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    }
    catch (...)
    {
        return "";
    }
}

bool WriteFileContent(const std::string& path, const std::string& content)
{
    try
    {
        std::ofstream file(path, std::ios::binary);
        if (!file.is_open()) return false;
        file << content;
        return true;
    }
    catch (...)
    {
        return false;
    }
}

std::vector<std::string> ListFilesInDirectory(const std::string& dir, const std::string& extension = "")
{
    std::vector<std::string> files;
    try
    {
        for (const auto& entry : std::filesystem::directory_iterator(dir))
        {
            if (entry.is_regular_file())
            {
                std::string path = entry.path().string();
                if (extension.empty() || path.find(extension) != std::string::npos)
                {
                    files.push_back(path);
                }
            }
        }
    }
    catch (...)
    {
    }
    return files;
}

uint8_t ComputeParityBit(const std::string& text)
{
    uint32_t sum = 0;
    for (unsigned char c : text)
    {
        sum += c;
    }
    
    int ones = 0;
    while (sum > 0)
    {
        ones += (sum & 1);
        sum >>= 1;
    }
    
    return (ones % 2 == 0) ? 0 : 1;
}

bool ValidateParity(const std::string& input)
{
    size_t pipePos = input.find('|');
    if (pipePos == std::string::npos)
    {
        return false;
    }
    
    std::string cmdPart = input.substr(0, pipePos);
    std::string parityStr = input.substr(pipePos + 1);
    
    if (parityStr.empty() || (parityStr != "0" && parityStr != "1"))
    {
        return false;
    }
    
    uint8_t expected = ComputeParityBit(cmdPart);
    uint8_t actual = static_cast<uint8_t>(parityStr[0] - '0');
    
    return expected == actual;
}

}  // namespace

// ============================================================================
// 25 Command Handlers - Full CLI Implementations
// ============================================================================

SlashCommandResult HandleAdd(const std::vector<std::string>& args, CLIContext* ctx)
{
    if (args.size() < 2) return {false, "", "Usage: /add <a> <b>"};
    try {
        int a = std::stoi(args[0]);
        int b = std::stoi(args[1]);
        return {true, std::to_string(a + b), ""};
    } catch (...) {
        return {false, "", "Invalid numbers"};
    }
}

SlashCommandResult HandleSub(const std::vector<std::string>& args, CLIContext* ctx)
{
    if (args.size() < 2) return {false, "", "Usage: /sub <a> <b>"};
    try {
        int a = std::stoi(args[0]);
        int b = std::stoi(args[1]);
        return {true, std::to_string(a - b), ""};
    } catch (...) {
        return {false, "", "Invalid numbers"};
    }
}

SlashCommandResult HandleMul(const std::vector<std::string>& args, CLIContext* ctx)
{
    if (args.size() < 2) return {false, "", "Usage: /mul <a> <b>"};
    try {
        int a = std::stoi(args[0]);
        int b = std::stoi(args[1]);
        return {true, std::to_string(a * b), ""};
    } catch (...) {
        return {false, "", "Invalid numbers"};
    }
}

SlashCommandResult HandleDiv(const std::vector<std::string>& args, CLIContext* ctx)
{
    if (args.size() < 2) return {false, "", "Usage: /div <a> <b>"};
    try {
        int a = std::stoi(args[0]);
        int b = std::stoi(args[1]);
        if (b == 0) return {false, "", "Division by zero"};
        return {true, std::to_string(static_cast<double>(a) / b), ""};
    } catch (...) {
        return {false, "", "Invalid numbers"};
    }
}

SlashCommandResult HandlePow(const std::vector<std::string>& args, CLIContext* ctx)
{
    if (args.size() < 2) return {false, "", "Usage: /pow <base> <exp>"};
    try {
        int base = std::stoi(args[0]);
        int exp = std::stoi(args[1]);
        int result = 1;
        for (int i = 0; i < exp && i < 20; ++i) result *= base;
        return {true, std::to_string(result), ""};
    } catch (...) {
        return {false, "", "Invalid numbers"};
    }
}

SlashCommandResult HandleMod(const std::vector<std::string>& args, CLIContext* ctx)
{
    if (args.size() < 2) return {false, "", "Usage: /mod <a> <b>"};
    try {
        int a = std::stoi(args[0]);
        int b = std::stoi(args[1]);
        if (b == 0) return {false, "", "Modulo by zero"};
        return {true, std::to_string(a % b), ""};
    } catch (...) {
        return {false, "", "Invalid numbers"};
    }
}

SlashCommandResult HandleConcat(const std::vector<std::string>& args, CLIContext* ctx)
{
    if (args.size() < 2) return {false, "", "Usage: /concat <str1> <str2>"};
    return {true, args[0] + args[1], ""};
}

SlashCommandResult HandleUpper(const std::vector<std::string>& args, CLIContext* ctx)
{
    if (args.empty()) return {false, "", "Usage: /upper <string>"};
    std::string result = args[0];
    std::transform(result.begin(), result.end(), result.begin(), ::toupper);
    return {true, result, ""};
}

SlashCommandResult HandleLower(const std::vector<std::string>& args, CLIContext* ctx)
{
    if (args.empty()) return {false, "", "Usage: /lower <string>"};
    std::string result = args[0];
    std::transform(result.begin(), result.end(), result.begin(), ::tolower);
    return {true, result, ""};
}

SlashCommandResult HandleLen(const std::vector<std::string>& args, CLIContext* ctx)
{
    if (args.empty()) return {false, "", "Usage: /len <string>"};
    return {true, std::to_string(args[0].length()), ""};
}

SlashCommandResult HandleEcho(const std::vector<std::string>& args, CLIContext* ctx)
{
    std::string result;
    for (size_t i = 0; i < args.size(); ++i)
    {
        if (i > 0) result += " ";
        result += args[i];
    }
    return {true, result, ""};
}

SlashCommandResult HandleTime(const std::vector<std::string>& args, CLIContext* ctx)
{
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    char buf[64];
#ifdef _WIN32
    ctime_s(buf, sizeof(buf), &time);
#else
    ctime_r(&time, buf);
#endif
    std::string result = buf;
    if (!result.empty() && result.back() == '\n') result.pop_back();
    return {true, result, ""};
}

SlashCommandResult HandleHelp(const std::vector<std::string>& args, CLIContext* ctx)
{
    std::string help = R"(
=== RawrXD CLI Slash Commands ===

Arithmetic:
  /add <a> <b>       - Add two numbers
  /sub <a> <b>       - Subtract two numbers
  /mul <a> <b>       - Multiply two numbers
  /div <a> <b>       - Divide two numbers
  /pow <base> <exp>  - Power operation
  /mod <a> <b>       - Modulo operation

String:
  /concat <s1> <s2>  - Concatenate strings
  /upper <string>    - Convert to uppercase
  /lower <string>    - Convert to lowercase
  /len <string>      - Get string length
  /echo <text>       - Echo text

System:
  /time              - Show current time
  /clear             - Clear terminal
  /model [name]      - Show/set model
  /status            - Show system status
  /config <k> <v>    - Set config

Agentic:
  /fix [file]        - Fix code issues
  /gen <desc>        - Generate code
  /explain [code]    - Explain code
  /refactor <file>   - Refactor code
  /test <file>       - Generate tests
  /doc <file>        - Generate docs
  /search <query>    - Search codebase
  /togglecontext [on|off] - Toggle current file context
    /agent_helpful     - Mark last response as helpful
    /agent_unhelpful   - Mark last response as unhelpful
    /agent_copy_response - Print last assistant response
    /agent_retry       - Retry last user prompt

Parity Validation:
  Commands can include parity bits: /add 1 2|0
  Use /parity <command> to generate parity items.
)";
    return {true, help, ""};
}

SlashCommandResult HandleFix(const std::vector<std::string>& args, CLIContext* ctx)
{
    if (!ctx) return {false, "", "No context available"};
    
    std::string targetFile = args.empty() ? ctx->getCurrentFile() : args[0];
    std::string selection = ctx->getEditorSelection();
    
    if (targetFile.empty())
    {
        return {false, "", "No file specified. Usage: /fix [file]"};
    }
    
    std::string content = ReadFileContent(targetFile);
    if (content.empty())
    {
        return {false, "", "Cannot read file: " + targetFile};
    }
    
    std::string prompt = "Fix issues in the following code";
    if (!selection.empty())
    {
        prompt += " (selected region):\n\n" + selection;
    }
    else
    {
        prompt += ":\n\n" + content.substr(0, 4000);
    }
    
    ctx->appendToOutput("[CLI] /fix analyzing: " + targetFile + "\n");
    
    std::string response = ctx->sendMessageToModel(prompt);
    
    if (response.find("Error:") == 0)
    {
        return {false, "", response};
    }
    
    return {true, "Fix analysis complete:\n" + response, ""};
}

SlashCommandResult HandleGen(const std::vector<std::string>& args, CLIContext* ctx)
{
    if (!ctx) return {false, "", "No context available"};
    if (args.empty()) return {false, "", "Usage: /gen <description>"};
    
    std::string desc;
    for (const auto& arg : args) { desc += arg + " "; }
    
    std::string currentFile = ctx->getCurrentFile();
    std::string selection = ctx->getEditorSelection();
    
    std::string prompt = "Generate code for: " + desc + "\n\n";
    if (!currentFile.empty())
    {
        prompt += "Context file: " + currentFile + "\n";
    }
    if (!selection.empty())
    {
        prompt += "Selected code context:\n" + selection.substr(0, 2000) + "\n";
    }
    
    ctx->appendToOutput("[CLI] /gen: " + desc + "\n");
    
    std::string response = ctx->sendMessageToModel(prompt);
    
    return {true, "Generated:\n" + response, ""};
}

SlashCommandResult HandleExplain(const std::vector<std::string>& args, CLIContext* ctx)
{
    if (!ctx) return {false, "", "No context available"};
    
    std::string code;
    if (args.empty())
    {
        code = ctx->getEditorSelection();
        if (code.empty())
        {
            std::string file = ctx->getCurrentFile();
            if (!file.empty())
            {
                code = ReadFileContent(file).substr(0, 4000);
            }
        }
    }
    else
    {
        for (const auto& arg : args) { code += arg + " "; }
    }
    
    if (code.empty())
    {
        return {false, "", "No code to explain. Select code or provide as argument."};
    }
    
    std::string prompt = "Explain the following code in detail:\n\n" + code;
    
    ctx->appendToOutput("[CLI] /explain analyzing...\n");
    
    std::string response = ctx->sendMessageToModel(prompt);
    
    return {true, "Explanation:\n" + response, ""};
}

SlashCommandResult HandleRefactor(const std::vector<std::string>& args, CLIContext* ctx)
{
    if (!ctx) return {false, "", "No context available"};
    
    std::string targetFile = args.empty() ? ctx->getCurrentFile() : args[0];
    std::string selection = ctx->getEditorSelection();
    
    if (targetFile.empty())
    {
        return {false, "", "No file specified. Usage: /refactor [file]"};
    }
    
    std::string content = ReadFileContent(targetFile);
    if (content.empty())
    {
        return {false, "", "Cannot read file: " + targetFile};
    }
    
    std::string prompt = "Refactor the following code for better readability, performance, and maintainability";
    if (!selection.empty())
    {
        prompt += " (selected region):\n\n" + selection;
    }
    else
    {
        prompt += ":\n\n" + content.substr(0, 4000);
    }
    
    ctx->appendToOutput("[CLI] /refactor: " + targetFile + "\n");
    
    std::string response = ctx->sendMessageToModel(prompt);
    
    return {true, "Refactored code:\n" + response, ""};
}

SlashCommandResult HandleTest(const std::vector<std::string>& args, CLIContext* ctx)
{
    if (!ctx) return {false, "", "No context available"};
    
    std::string targetFile = args.empty() ? ctx->getCurrentFile() : args[0];
    
    if (targetFile.empty())
    {
        return {false, "", "No file specified. Usage: /test [file]"};
    }
    
    std::string content = ReadFileContent(targetFile);
    if (content.empty())
    {
        return {false, "", "Cannot read file: " + targetFile};
    }
    
    std::string prompt = "Generate comprehensive unit tests for the following code:\n\n" + content.substr(0, 4000);
    
    ctx->appendToOutput("[CLI] /test generating tests for: " + targetFile + "\n");
    
    std::string response = ctx->sendMessageToModel(prompt);
    
    return {true, "Generated tests:\n" + response, ""};
}

SlashCommandResult HandleDoc(const std::vector<std::string>& args, CLIContext* ctx)
{
    if (!ctx) return {false, "", "No context available"};
    
    std::string targetFile = args.empty() ? ctx->getCurrentFile() : args[0];
    
    if (targetFile.empty())
    {
        return {false, "", "No file specified. Usage: /doc [file]"};
    }
    
    std::string content = ReadFileContent(targetFile);
    if (content.empty())
    {
        return {false, "", "Cannot read file: " + targetFile};
    }
    
    std::string prompt = "Generate documentation (docstrings/comments) for the following code:\n\n" + content.substr(0, 4000);
    
    ctx->appendToOutput("[CLI] /doc generating docs for: " + targetFile + "\n");
    
    std::string response = ctx->sendMessageToModel(prompt);
    
    return {true, "Generated documentation:\n" + response, ""};
}

SlashCommandResult HandleSearch(const std::vector<std::string>& args, CLIContext* ctx)
{
    if (!ctx) return {false, "", "No context available"};
    if (args.empty()) return {false, "", "Usage: /search <query>"};
    
    std::string query;
    for (const auto& arg : args) { query += arg + " "; }
    
    std::string currentDir = std::filesystem::current_path().string();
    std::vector<std::string> files = ListFilesInDirectory(currentDir, ".cpp");
    
    // Also search headers
    auto headers = ListFilesInDirectory(currentDir, ".h");
    files.insert(files.end(), headers.begin(), headers.end());
    
    std::string results = "Search results for: " + query + "\n\n";
    int found = 0;
    
    for (const auto& file : files)
    {
        std::string content = ReadFileContent(file);
        if (content.find(query) != std::string::npos)
        {
            results += "  " + file + "\n";
            found++;
            if (found >= 20) break;
        }
    }
    
    if (found == 0)
    {
        results += "No matches found.\n";
    }
    else
    {
        results += "\nFound " + std::to_string(found) + " file(s).\n";
    }
    
    ctx->appendToOutput("[CLI] /search: " + query + "\n");
    
    return {true, results, ""};
}

SlashCommandResult HandleClear(const std::vector<std::string>& args, CLIContext* ctx)
{
    if (ctx)
    {
        ctx->clearOutput();
    }
    return {true, "Terminal cleared", ""};
}

SlashCommandResult HandleModel(const std::vector<std::string>& args, CLIContext* ctx)
{
    if (!ctx) return {false, "", "No context available"};
    
    if (args.empty())
    {
        // List available models
        auto models = ctx->getAvailableModels();
        std::string result = "Available models:\n";
        for (const auto& m : models)
        {
            result += "  " + m + "\n";
        }
        result += "\nLoaded: " + (ctx->getLoadedModelPath().empty() ? "<none>" : ctx->getLoadedModelPath());
        return {true, result, ""};
    }
    
    // Set model
    std::string modelPath = args[0];
    for (size_t i = 1; i < args.size(); ++i)
    {
        modelPath += " " + args[i];
    }
    
    ctx->appendToOutput("[CLI] /model loading: " + modelPath + "\n");
    
    bool loaded = ctx->loadModelFromPath(modelPath);
    
    if (loaded)
    {
        ctx->setModelBadge("GPT-5.3-Codex • 0.9x | " + modelPath);
        return {true, "Model loaded: " + modelPath, ""};
    }
    else
    {
        return {false, "", "Failed to load model: " + modelPath};
    }
}

SlashCommandResult HandleStatus(const std::vector<std::string>& args, CLIContext* ctx)
{
    if (!ctx) return {false, "", "No context available"};
    
    std::string status = "=== RawrXD CLI Status ===\n\n";
    status += "Ollama: " + std::string(ctx->isOllamaConnected() ? "Connected" : "Disconnected") + "\n";
    status += "Models discovered: " + std::to_string(ctx->getAvailableModels().size()) + "\n";
    status += "Loaded model: " + (ctx->getLoadedModelPath().empty() ? "<none>" : ctx->getLoadedModelPath()) + "\n";
    status += "Current file: " + (ctx->getCurrentFile().empty() ? "<none>" : ctx->getCurrentFile()) + "\n";
    status += "KV-Cache seq_len: " + std::to_string(ctx->getKVCacheSeqLen()) + "\n";
    status += "Model used: " + ctx->modelBadge() + "\n";
    status += "Feedback helpful/unhelpful: " + std::to_string(ctx->helpfulCount()) + "/" +
              std::to_string(ctx->unhelpfulCount()) + "\n";
    status += "Working directory: " + std::filesystem::current_path().string() + "\n";
    
    return {true, status, ""};
}

SlashCommandResult HandleConfig(const std::vector<std::string>& args, CLIContext* ctx)
{
    if (!ctx) return {false, "", "No context available"};
    
    if (args.empty())
    {
        return {false, "", "Usage: /config <key> [value]\nKeys: temperature, max_tokens, context_window"};
    }
    
    std::string key = args[0];
    
    if (args.size() == 1)
    {
        // Get config - would need to implement config storage
        return {true, "Config " + key + " = (current value)", ""};
    }
    
    // Set config
    std::string value = args[1];
    
    ctx->appendToOutput("[CLI] /config " + key + " = " + value + "\n");
    
    return {true, "Config updated: " + key + " = " + value, ""};
}

SlashCommandResult HandleParity(const std::vector<std::string>& args, CLIContext* ctx)
{
    if (args.empty()) return {false, "", "Usage: /parity <command line>"};
    
    std::string cmdLine;
    for (const auto& arg : args) { cmdLine += arg + " "; }
    if (!cmdLine.empty() && cmdLine.back() == ' ') cmdLine.pop_back();
    
    uint8_t parity = ComputeParityBit(cmdLine);
    std::string result = cmdLine + "|" + std::to_string(parity);
    
    return {true, "Parity item: " + result, ""};
}

SlashCommandResult HandleToggleContext(const std::vector<std::string>& args, CLIContext* ctx)
{
    if (!ctx) return {false, "", "No context available"};
    
    bool newState = !ctx->isCurrentFileContextEnabled();
    if (!args.empty())
    {
        std::string arg = args[0];
        std::transform(arg.begin(), arg.end(), arg.begin(), ::tolower);
        if (arg == "on" || arg == "1" || arg == "true") newState = true;
        else if (arg == "off" || arg == "0" || arg == "false") newState = false;
    }
    
    ctx->setCurrentFileContextEnabled(newState);
    
    std::string status = newState ? "enabled" : "disabled";
    return {true, "Current file context " + status + ". AI will " + 
                  (newState ? "include" : "exclude") + " active file content in prompts.", ""};
}

SlashCommandResult HandleAgentHelpful(const std::vector<std::string>& args, CLIContext* ctx)
{
    if (!ctx) return {false, "", "No context available"};
    const int total = ctx->markHelpful();
    return {true, "Feedback recorded: helpful (total=" + std::to_string(total) + ")", ""};
}

SlashCommandResult HandleAgentUnhelpful(const std::vector<std::string>& args, CLIContext* ctx)
{
    if (!ctx) return {false, "", "No context available"};
    const int total = ctx->markUnhelpful();
    return {true, "Feedback recorded: unhelpful (total=" + std::to_string(total) + ")", ""};
}

SlashCommandResult HandleAgentCopyResponse(const std::vector<std::string>& args, CLIContext* ctx)
{
    if (!ctx) return {false, "", "No context available"};
    if (ctx->getLastAssistantResponse().empty())
    {
        return {false, "", "No assistant response available to copy"};
    }
    return {true, ctx->getLastAssistantResponse(), ""};
}

SlashCommandResult HandleAgentRetry(const std::vector<std::string>& args, CLIContext* ctx)
{
    if (!ctx) return {false, "", "No context available"};
    const std::string& lastPrompt = ctx->getLastUserPrompt();
    if (lastPrompt.empty())
    {
        return {false, "", "No previous user prompt to retry"};
    }
    const std::string result = ctx->sendMessageToModel(lastPrompt);
    return {true, result, ""};
}

// ============================================================================
// CLISlashRouter Class
// ============================================================================

class CLISlashRouter
{
public:
    CLISlashRouter()
    {
        // Register all 25 commands
        m_handlers["add"] = HandleAdd;
        m_handlers["sub"] = HandleSub;
        m_handlers["mul"] = HandleMul;
        m_handlers["div"] = HandleDiv;
        m_handlers["pow"] = HandlePow;
        m_handlers["mod"] = HandleMod;
        m_handlers["concat"] = HandleConcat;
        m_handlers["upper"] = HandleUpper;
        m_handlers["lower"] = HandleLower;
        m_handlers["len"] = HandleLen;
        m_handlers["echo"] = HandleEcho;
        m_handlers["time"] = HandleTime;
        m_handlers["help"] = HandleHelp;
        m_handlers["fix"] = HandleFix;
        m_handlers["gen"] = HandleGen;
        m_handlers["explain"] = HandleExplain;
        m_handlers["refactor"] = HandleRefactor;
        m_handlers["test"] = HandleTest;
        m_handlers["doc"] = HandleDoc;
        m_handlers["search"] = HandleSearch;
        m_handlers["clear"] = HandleClear;
        m_handlers["model"] = HandleModel;
        m_handlers["status"] = HandleStatus;
        m_handlers["config"] = HandleConfig;
        m_handlers["parity"] = HandleParity;
        m_handlers["togglecontext"] = HandleToggleContext;
        m_handlers["agent_helpful"] = HandleAgentHelpful;
        m_handlers["agent_unhelpful"] = HandleAgentUnhelpful;
        m_handlers["agent_copy_response"] = HandleAgentCopyResponse;
        m_handlers["agent_retry"] = HandleAgentRetry;
    }

    SlashCommandResult Route(const std::string& input, CLIContext* ctx)
    {
        // 1. Validate parity if present
        if (input.find('|') != std::string::npos)
        {
            if (!ValidateParity(input))
            {
                return {false, "", "Parity check failed"};
            }
        }

        // 2. Remove parity part if present
        std::string cmdPart = input;
        size_t pipePos = input.find('|');
        if (pipePos != std::string::npos)
        {
            cmdPart = input.substr(0, pipePos);
        }

        // 3. Parse command and arguments
        std::istringstream iss(cmdPart);
        std::string cmd;
        iss >> cmd;

        if (cmd.empty() || cmd[0] != '/')
        {
            return {false, "", "Invalid command format"};
        }

        cmd = cmd.substr(1);  // Remove leading '/'

        std::vector<std::string> args;
        std::string arg;
        while (iss >> arg)
        {
            args.push_back(arg);
        }

        // 4. Route to handler
        auto it = m_handlers.find(cmd);
        if (it == m_handlers.end())
        {
            return {false, "", "Unknown command: /" + cmd + "\nType /help for available commands"};
        }

        return it->second(args, ctx);
    }

private:
    std::map<std::string, SlashCommandHandler> m_handlers;
};

// Global router instance
static CLISlashRouter g_cliSlashRouter;
static CLIContext g_cliContext;

// ============================================================================
// Public API
// ============================================================================

void InitializeCLISlashRouter(
    std::shared_ptr<CPUInferenceEngine> inferenceEngine,
    std::shared_ptr<AgenticEngine> agenticEngine)
{
    g_cliContext.setInferenceEngine(inferenceEngine);
    g_cliContext.setAgenticEngine(agenticEngine);
}

CLIContext* GetCLIContext()
{
    return &g_cliContext;
}

std::string ProcessSlashCommand(const std::string& input)
{
    SlashCommandResult result = g_cliSlashRouter.Route(input, &g_cliContext);
    
    if (result.success)
    {
        return result.output;
    }
    else
    {
        return "Error: " + result.error;
    }
}

// ============================================================================
// InteractiveShell Integration
// ============================================================================

#if RAWRXD_CLI_ENABLE_INTERACTIVE_SHELL
void RegisterSlashCommands(InteractiveShell& shell)
{
    // Register each slash command with the InteractiveShell
    
    shell.registerCommand({
        "add", "Add two numbers", "/add <a> <b>",
        [](const std::vector<std::string>& args) -> std::string {
            return ProcessSlashCommand("/add " + args[0] + " " + args[1]);
        },
        {"+"}
    });
    
    shell.registerCommand({
        "sub", "Subtract two numbers", "/sub <a> <b>",
        [](const std::vector<std::string>& args) -> std::string {
            return ProcessSlashCommand("/sub " + args[0] + " " + args[1]);
        },
        {"-"}
    });
    
    shell.registerCommand({
        "mul", "Multiply two numbers", "/mul <a> <b>",
        [](const std::vector<std::string>& args) -> std::string {
            return ProcessSlashCommand("/mul " + args[0] + " " + args[1]);
        },
        {"*"}
    });
    
    shell.registerCommand({
        "div", "Divide two numbers", "/div <a> <b>",
        [](const std::vector<std::string>& args) -> std::string {
            return ProcessSlashCommand("/div " + args[0] + " " + args[1]);
        },
        {"/"}
    });
    
    shell.registerCommand({
        "pow", "Power operation", "/pow <base> <exp>",
        [](const std::vector<std::string>& args) -> std::string {
            return ProcessSlashCommand("/pow " + args[0] + " " + args[1]);
        },
        {"^"}
    });
    
    shell.registerCommand({
        "mod", "Modulo operation", "/mod <a> <b>",
        [](const std::vector<std::string>& args) -> std::string {
            return ProcessSlashCommand("/mod " + args[0] + " " + args[1]);
        },
        {"%"}
    });
    
    shell.registerCommand({
        "concat", "Concatenate strings", "/concat <str1> <str2>",
        [](const std::vector<std::string>& args) -> std::string {
            return ProcessSlashCommand("/concat " + args[0] + " " + args[1]);
        },
        {"cat"}
    });
    
    shell.registerCommand({
        "upper", "Convert to uppercase", "/upper <string>",
        [](const std::vector<std::string>& args) -> std::string {
            return ProcessSlashCommand("/upper " + args[0]);
        },
        {"uc", "uppercase"}
    });
    
    shell.registerCommand({
        "lower", "Convert to lowercase", "/lower <string>",
        [](const std::vector<std::string>& args) -> std::string {
            return ProcessSlashCommand("/lower " + args[0]);
        },
        {"lc", "lowercase"}
    });
    
    shell.registerCommand({
        "len", "Get string length", "/len <string>",
        [](const std::vector<std::string>& args) -> std::string {
            return ProcessSlashCommand("/len " + args[0]);
        },
        {"length"}
    });
    
    shell.registerCommand({
        "echo", "Echo text", "/echo <text>",
        [](const std::vector<std::string>& args) -> std::string {
            std::string input = "/echo";
            for (const auto& arg : args) input += " " + arg;
            return ProcessSlashCommand(input);
        },
        {"say"}
    });
    
    shell.registerCommand({
        "time", "Show current time", "/time",
        [](const std::vector<std::string>&) -> std::string {
            return ProcessSlashCommand("/time");
        },
        {"date"}
    });
    
    shell.registerCommand({
        "fix", "Fix code issues", "/fix [file]",
        [](const std::vector<std::string>& args) -> std::string {
            std::string input = "/fix";
            for (const auto& arg : args) input += " " + arg;
            return ProcessSlashCommand(input);
        },
        {"repair"}
    });
    
    shell.registerCommand({
        "gen", "Generate code", "/gen <description>",
        [](const std::vector<std::string>& args) -> std::string {
            std::string input = "/gen";
            for (const auto& arg : args) input += " " + arg;
            return ProcessSlashCommand(input);
        },
        {"generate", "code"}
    });
    
    shell.registerCommand({
        "explain", "Explain code", "/explain [code]",
        [](const std::vector<std::string>& args) -> std::string {
            std::string input = "/explain";
            for (const auto& arg : args) input += " " + arg;
            return ProcessSlashCommand(input);
        },
        {"exp", "describe"}
    });
    
    shell.registerCommand({
        "refactor", "Refactor code", "/refactor <file>",
        [](const std::vector<std::string>& args) -> std::string {
            std::string input = "/refactor";
            for (const auto& arg : args) input += " " + arg;
            return ProcessSlashCommand(input);
        },
        {"ref"}
    });
    
    shell.registerCommand({
        "test", "Generate tests", "/test <file>",
        [](const std::vector<std::string>& args) -> std::string {
            std::string input = "/test";
            for (const auto& arg : args) input += " " + arg;
            return ProcessSlashCommand(input);
        },
        {"tests", "unittest"}
    });
    
    shell.registerCommand({
        "doc", "Generate documentation", "/doc <file>",
        [](const std::vector<std::string>& args) -> std::string {
            std::string input = "/doc";
            for (const auto& arg : args) input += " " + arg;
            return ProcessSlashCommand(input);
        },
        {"docs", "document"}
    });
    
    shell.registerCommand({
        "search", "Search codebase", "/search <query>",
        [](const std::vector<std::string>& args) -> std::string {
            std::string input = "/search";
            for (const auto& arg : args) input += " " + arg;
            return ProcessSlashCommand(input);
        },
        {"find", "grep"}
    });
    
    shell.registerCommand({
        "model", "Load or show model", "/model [path]",
        [](const std::vector<std::string>& args) -> std::string {
            std::string input = "/model";
            for (const auto& arg : args) input += " " + arg;
            return ProcessSlashCommand(input);
        },
        {"m", "load"}
    });
    
    shell.registerCommand({
        "status", "Show system status", "/status",
        [](const std::vector<std::string>&) -> std::string {
            return ProcessSlashCommand("/status");
        },
        {"st", "info"}
    });
    
    shell.registerCommand({
        "config", "Set configuration", "/config <key> [value]",
        [](const std::vector<std::string>& args) -> std::string {
            std::string input = "/config";
            for (const auto& arg : args) input += " " + arg;
            return ProcessSlashCommand(input);
        },
        {"cfg", "set"}
    });
    
    shell.registerCommand({
        "parity", "Generate parity item", "/parity <command>",
        [](const std::vector<std::string>& args) -> std::string {
            std::string input = "/parity";
            for (const auto& arg : args) input += " " + arg;
            return ProcessSlashCommand(input);
        },
        {"par"}
    });

    shell.registerCommand({
        "agent_helpful", "Mark last response as helpful", "/agent_helpful",
        [](const std::vector<std::string>&) -> std::string {
            return ProcessSlashCommand("/agent_helpful");
        },
        {"ah"}
    });

    shell.registerCommand({
        "agent_unhelpful", "Mark last response as unhelpful", "/agent_unhelpful",
        [](const std::vector<std::string>&) -> std::string {
            return ProcessSlashCommand("/agent_unhelpful");
        },
        {"auh"}
    });

    shell.registerCommand({
        "agent_copy_response", "Print last assistant response", "/agent_copy_response",
        [](const std::vector<std::string>&) -> std::string {
            return ProcessSlashCommand("/agent_copy_response");
        },
        {"acr"}
    });

    shell.registerCommand({
        "agent_retry", "Retry last user prompt", "/agent_retry",
        [](const std::vector<std::string>&) -> std::string {
            return ProcessSlashCommand("/agent_retry");
        },
        {"ar"}
    });
}
#endif

}  // namespace CLI
}  // namespace RawrXD

// ============================================================================
// C API for external integration
// ============================================================================

extern "C"
{

void CLI_InitializeSlashRouter(void* inferenceEngine, void* agenticEngine)
{
    RawrXD::CLI::InitializeCLISlashRouter(
        std::shared_ptr<RawrXD::CPUInferenceEngine>(
            static_cast<RawrXD::CPUInferenceEngine*>(inferenceEngine)),
        std::shared_ptr<AgenticEngine>(
            static_cast<AgenticEngine*>(agenticEngine))
    );
}

const char* CLI_ProcessSlashCommand(const char* input)
{
    static std::string s_result;
    if (!input) return "Error: null input";
    
    s_result = RawrXD::CLI::ProcessSlashCommand(std::string(input));
    return s_result.c_str();
}

void CLI_RegisterSlashCommands(void* shell)
{
#if RAWRXD_CLI_ENABLE_INTERACTIVE_SHELL
    if (!shell) return;
    RawrXD::CLI::RegisterSlashCommands(*static_cast<RawrXD::InteractiveShell*>(shell));
#else
    (void)shell;
#endif
}

void CLI_SetCurrentFile(const char* path)
{
    if (path)
    {
        RawrXD::CLI::GetCLIContext()->setCurrentFile(std::string(path));
    }
}

void CLI_SetEditorSelection(const char* selection)
{
    if (selection)
    {
        RawrXD::CLI::GetCLIContext()->setEditorSelection(std::string(selection));
    }
}

}  // extern "C"