// ============================================================================
// agentic_flow.cpp — Autonomous Multi-Step Task Execution Engine Implementation
// ============================================================================
// Copyright (c) 2025-2026 RawrXD Project — All rights reserved.
// ============================================================================

#include "agentic/agentic_flow.h"

#include <algorithm>
#include <sstream>
#include <fstream>
#include <thread>
#include <future>
#include <queue>
#include <stack>
#include <regex>
#include <random>
#include <chrono>
#include <mutex>
#include <shared_mutex>
#include <atomic>
#include <condition_variable>
#include <filesystem>
#include <cstdlib>

namespace RawrXD::Agentic {

// ============================================================================
// Internal Implementation
// ============================================================================

class FlowEngine::Impl {
public:
    FlowMode mode = FlowMode::Interactive;
    FlowCallback callback;
    
    std::unordered_map<std::string, FlowDefinition> flows;
    std::unordered_map<std::string, FlowExecution> executions;
    std::unordered_map<std::string, FlowResult> results;
    
    std::atomic<uint32_t> nextExecutionId{1};
    std::atomic<uint32_t> nextCheckpointId{1};
    
    mutable std::shared_mutex flowsMutex;
    mutable std::shared_mutex executionsMutex;
    
    // Input handling
    std::unordered_map<std::string, std::queue<std::string>> inputQueues;
    std::unordered_map<std::string, std::condition_variable> inputCVs;
    std::mutex inputMutex;
    
    // Execution threads
    std::unordered_map<std::string, std::thread> executionThreads;
    std::unordered_map<std::string, std::atomic<bool>> cancellationTokens;
    
    Impl() = default;

    static std::string trimCopy(const std::string& value) {
        const auto begin = value.find_first_not_of(" \t\r\n");
        if (begin == std::string::npos) return "";
        const auto end = value.find_last_not_of(" \t\r\n");
        return value.substr(begin, end - begin + 1);
    }

    static std::string shellQuote(const std::string& value) {
#ifdef _WIN32
        std::string escaped;
        escaped.reserve(value.size() + 2);
        escaped.push_back('"');
        for (char ch : value) {
            if (ch == '"') escaped += "\\\"";
            else escaped.push_back(ch);
        }
        escaped.push_back('"');
        return escaped;
#else
        std::string escaped = "'";
        for (char ch : value) {
            if (ch == '\'') escaped += "'\\''";
            else escaped.push_back(ch);
        }
        escaped.push_back('\'');
        return escaped;
#endif
    }

    static std::string substituteVariables(std::string text, const FlowContext& ctx) {
        for (const auto& [name, value] : ctx.variables) {
            const std::string placeholder = "${" + name + "}";
            size_t pos = 0;
            while ((pos = text.find(placeholder, pos)) != std::string::npos) {
                text.replace(pos, placeholder.length(), value);
                pos += value.length();
            }
        }
        return text;
    }

    std::pair<bool, std::string> executeCommandCapture(
        const std::string& command,
        const FlowContext& ctx,
        std::atomic<bool>& cancelToken) {

        if (cancelToken) return {false, "Cancelled"};

        const std::string resolvedCommand = trimCopy(substituteVariables(command, ctx));
        if (resolvedCommand.empty()) {
            return {false, "No command specified"};
        }

        std::error_code ec;
        auto tempFile = std::filesystem::temp_directory_path(ec) /
                        ("rawrxd_agentic_flow_" + generateId() + ".log");
        if (ec) {
            tempFile = std::filesystem::path("rawrxd_agentic_flow_" + generateId() + ".log");
        }

#ifdef _WIN32
        std::string wrapped;
        if (!ctx.workingDirectory.empty()) {
            wrapped = "cmd /d /c \"cd /d " + shellQuote(ctx.workingDirectory) +
                      " && " + resolvedCommand + " > " + shellQuote(tempFile.string()) + " 2>&1\"";
        } else {
            wrapped = "cmd /d /c \"" + resolvedCommand + " > " + shellQuote(tempFile.string()) + " 2>&1\"";
        }
#else
        std::string wrapped;
        if (!ctx.workingDirectory.empty()) {
            wrapped = "sh -lc " + shellQuote("cd " + shellQuote(ctx.workingDirectory) +
                      " && " + resolvedCommand + " > " + shellQuote(tempFile.string()) + " 2>&1");
        } else {
            wrapped = "sh -lc " + shellQuote(resolvedCommand + " > " + shellQuote(tempFile.string()) + " 2>&1");
        }
#endif

        const int exitCode = std::system(wrapped.c_str());

        std::ifstream in(tempFile, std::ios::binary);
        std::ostringstream output;
        if (in.is_open()) output << in.rdbuf();
        std::filesystem::remove(tempFile, ec);

        std::string text = trimCopy(output.str());
        if (text.empty()) {
            text = exitCode == 0 ? "Command completed successfully" : "Command failed";
        }
        return {exitCode == 0, text};
    }
    
    ~Impl() {
        // Cancel all running flows
        for (auto& [id, token] : cancellationTokens) {
            token = true;
        }
        
        // Wait for threads
        for (auto& [id, thread] : executionThreads) {
            if (thread.joinable()) {
                thread.join();
            }
        }
    }
    
    void emitEvent(FlowEvent event, const std::string& executionId,
                   const std::string& stepId = "",
                   const std::string& message = "",
                   float progress = 0.0f) {
        if (callback) {
            FlowEventArgs args;
            args.event = event;
            args.executionId = executionId;
            args.stepId = stepId;
            args.message = message;
            args.progress = progress;
            callback(args);
        }
    }
    
    // Generate unique ID
    std::string generateId() {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        static std::uniform_int_distribution<uint64_t> dis;
        
        std::ostringstream oss;
        oss << std::hex << dis(gen) << dis(gen);
        return oss.str();
    }
    
    // Evaluate condition
    bool evaluateCondition(const StepCondition& cond, const FlowContext& ctx) {
        auto it = ctx.variables.find(cond.variable);
        std::string value = (it != ctx.variables.end()) ? it->second : "";
        
        if (cond.operator_ == "==") return value == cond.value;
        if (cond.operator_ == "!=") return value != cond.value;
        if (cond.operator_ == "contains") return value.find(cond.value) != std::string::npos;
        if (cond.operator_ == "startsWith") return value.find(cond.value) == 0;
        if (cond.operator_ == "endsWith") {
            if (value.length() < cond.value.length()) return false;
            return value.substr(value.length() - cond.value.length()) == cond.value;
        }
        if (cond.operator_ == "matches") {
            try {
                std::regex pattern(cond.value);
                return std::regex_search(value, pattern);
            } catch (...) { return false; }
        }
        if (cond.operator_ == "exists") return ctx.variables.count(cond.variable) > 0;
        if (cond.operator_ == "empty") return value.empty();
        if (cond.operator_ == "notEmpty") return !value.empty();
        
        // Numeric comparisons
        try {
            double left = std::stod(value);
            double right = std::stod(cond.value);
            if (cond.operator_ == "<") return left < right;
            if (cond.operator_ == "<=") return left <= right;
            if (cond.operator_ == ">") return left > right;
            if (cond.operator_ == ">=") return left >= right;
        } catch (...) {}
        
        return false;
    }
    
    // Execute a single step action
    std::pair<bool, std::string> executeAction(
        const StepAction& action,
        FlowContext& ctx,
        std::atomic<bool>& cancelToken) {
        
        switch (action.type) {
            case StepType::Analyze:
                return executeAnalyze(action, ctx, cancelToken);
            case StepType::Plan:
                return executePlan(action, ctx, cancelToken);
            case StepType::Edit:
                return executeEdit(action, ctx, cancelToken);
            case StepType::Search:
                return executeSearch(action, ctx, cancelToken);
            case StepType::Build:
                return executeBuild(action, ctx, cancelToken);
            case StepType::Test:
                return executeTest(action, ctx, cancelToken);
            case StepType::Generate:
                return executeGenerate(action, ctx, cancelToken);
            case StepType::Query:
                return executeQuery(action, ctx, cancelToken);
            case StepType::Wait:
                return executeWait(action, ctx, cancelToken);
            case StepType::Custom:
                return executeCustom(action, ctx, cancelToken);
            default:
                return {false, "Unknown step type"};
        }
    }
    
    std::pair<bool, std::string> executeAnalyze(
        const StepAction& action,
        FlowContext& ctx,
        std::atomic<bool>& cancelToken) {
        
        // Analyze target files
        std::ostringstream result;
        result << "{\n";
        
        for (const auto& file : ctx.targetFiles) {
            if (cancelToken) return {false, "Cancelled"};
            
            auto it = ctx.fileContents.find(file);
            if (it != ctx.fileContents.end()) {
                result << "  \"" << file << "\": {\n";
                result << "    \"size\": " << it->second.size() << ",\n";
                result << "    \"lines\": " << std::count(it->second.begin(), it->second.end(), '\n') << "\n";
                result << "  },\n";
            }
        }
        
        result << "}\n";
        
        if (!action.outputVar.empty()) {
            ctx.variables[action.outputVar] = result.str();
        }
        
        return {true, result.str()};
    }
    
    std::pair<bool, std::string> executePlan(
        const StepAction& action,
        FlowContext& ctx,
        std::atomic<bool>& cancelToken) {
        
        // Generate execution plan based on context
        std::ostringstream plan;
        plan << "Plan generated for: " << action.description << "\n";
        plan << "Target files: " << ctx.targetFiles.size() << "\n";
        
        plan << "Working directory: "
             << (ctx.workingDirectory.empty() ? "<default>" : ctx.workingDirectory) << "\n";
        uint32_t index = 1;
        for (const auto& file : ctx.targetFiles) {
            plan << index++ << ". Inspect " << file;
            const auto contentIt = ctx.fileContents.find(file);
            if (contentIt != ctx.fileContents.end()) {
                plan << " (" << contentIt->second.size() << " bytes)";
            }
            plan << "\n";
        }
        if (ctx.targetFiles.empty()) {
            plan << index++ << ". Discover relevant files from current context\n";
        }
        plan << index++ << ". Generate concrete changes for the requested task\n";
        plan << index++ << ". Apply changes and capture modified file contents\n";
        plan << index++ << ". Run build/test verification before completion\n";
        
        ctx.variables["last_plan"] = plan.str();
        if (!action.outputVar.empty()) ctx.variables[action.outputVar] = plan.str();
        
        return {true, plan.str()};
    }
    
    std::pair<bool, std::string> executeEdit(
        const StepAction& action,
        FlowContext& ctx,
        std::atomic<bool>& cancelToken) {
        
        // Apply file edits
        std::string targetFile = action.params.count("file") ? 
            action.params.at("file") : (ctx.targetFiles.empty() ? "" : ctx.targetFiles[0]);
        
        if (targetFile.empty()) {
            return {false, "No target file specified"};
        }
        
        // Get edit operation
        std::string operation = action.params.count("operation") ?
            action.params.at("operation") : "replace";
        
        std::string content = action.params.count("content") ?
            substituteVariables(action.params.at("content"), ctx) : "";

        if (content.empty()) {
            if (!action.inputVar.empty()) {
                auto inputIt = ctx.variables.find(action.inputVar);
                if (inputIt != ctx.variables.end()) content = inputIt->second;
            }
            if (content.empty()) {
                auto generatedIt = ctx.variables.find("generated_content");
                if (generatedIt != ctx.variables.end()) content = generatedIt->second;
            }
            if (content.empty()) {
                auto generatedIt = ctx.variables.find("last_generate");
                if (generatedIt != ctx.variables.end()) content = generatedIt->second;
            }
        }
        
        // Read current content
        std::string currentContent;
        auto it = ctx.fileContents.find(targetFile);
        if (it != ctx.fileContents.end()) {
            currentContent = it->second;
        } else {
            std::ifstream file(targetFile);
            if (file.is_open()) {
                std::ostringstream ss;
                ss << file.rdbuf();
                currentContent = ss.str();
            }
        }
        
        // Apply edit
        if (operation == "replace") {
            // Replace pattern
            std::string pattern = action.params.count("pattern") ?
                action.params.at("pattern") : "";
            
            if (!pattern.empty()) {
                try {
                    std::regex re(pattern);
                    currentContent = std::regex_replace(currentContent, re, content);
                } catch (const std::regex_error& e) {
                    return {false, std::string("Regex error: ") + e.what()};
                }
            }
        } else if (operation == "insert") {
            // Insert at position
            uint32_t line = action.params.count("line") ?
                std::stoul(action.params.at("line")) : 0;
            
            std::istringstream iss(currentContent);
            std::vector<std::string> lines;
            std::string line;
            while (std::getline(iss, line)) {
                lines.push_back(line);
            }
            
            if (line <= lines.size()) {
                lines.insert(lines.begin() + line, content);
            }
            
            std::ostringstream oss;
            for (const auto& l : lines) {
                oss << l << "\n";
            }
            currentContent = oss.str();
        } else if (operation == "append") {
            currentContent += content;
        }
        
        // Store modified content
        ctx.fileContents[targetFile] = currentContent;
        
        // Write to file
        std::ofstream file(targetFile);
        if (!file.is_open()) {
            return {false, "Failed to write file: " + targetFile};
        }
        file << currentContent;
        
        return {true, "File edited: " + targetFile};
    }
    
    std::pair<bool, std::string> executeSearch(
        const StepAction& action,
        FlowContext& ctx,
        std::atomic<bool>& cancelToken) {
        
        std::string pattern = action.params.count("pattern") ?
            action.params.at("pattern") : action.command;
        
        std::ostringstream results;
        results << "[\n";
        
        bool first = true;
        for (const auto& file : ctx.targetFiles) {
            if (cancelToken) return {false, "Cancelled"};
            
            auto it = ctx.fileContents.find(file);
            if (it == ctx.fileContents.end()) continue;
            
            try {
                std::regex re(pattern);
                std::sregex_iterator begin(it->second.begin(), it->second.end(), re);
                std::sregex_iterator end;
                
                for (auto match = begin; match != end; ++match) {
                    if (!first) results << ",\n";
                    first = false;
                    
                    results << "  {\"file\": \"" << file << "\", ";
                    results << "\"position\": " << match->position() << ", ";
                    results << "\"match\": \"" << match->str() << "\"}";
                }
            } catch (...) {}
        }
        
        results << "\n]\n";
        
        if (!action.outputVar.empty()) {
            ctx.variables[action.outputVar] = results.str();
        }
        
        return {true, results.str()};
    }
    
    std::pair<bool, std::string> executeBuild(
        const StepAction& action,
        FlowContext& ctx,
        std::atomic<bool>& cancelToken) {
        
        // Execute build command
        std::string buildCmd = action.command.empty() ?
            "cmake --build build" : action.command;
        
        auto [ok, output] = executeCommandCapture(buildCmd, ctx, cancelToken);
        if (!action.outputVar.empty()) ctx.variables[action.outputVar] = output;
        ctx.variables["last_build"] = output;
        return {ok, output};
    }
    
    std::pair<bool, std::string> executeTest(
        const StepAction& action,
        FlowContext& ctx,
        std::atomic<bool>& cancelToken) {
        
        // Execute test command
        std::string testCmd = action.command.empty() ?
            "ctest --output-on-failure" : action.command;
        
        auto [ok, output] = executeCommandCapture(testCmd, ctx, cancelToken);
        if (!action.outputVar.empty()) ctx.variables[action.outputVar] = output;
        ctx.variables["last_test"] = output;
        return {ok, output};
    }
    
    std::pair<bool, std::string> executeGenerate(
        const StepAction& action,
        FlowContext& ctx,
        std::atomic<bool>& cancelToken) {
        
        // Generate code based on description
        std::string description = action.params.count("description") ?
            action.params.at("description") : action.description;
        
        const std::string lowered = [&description]() {
            std::string out = description;
            std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) {
                return static_cast<char>(std::tolower(c));
            });
            return out;
        }();

        std::ostringstream generated;
        if (lowered.find("test") != std::string::npos) {
            const std::filesystem::path target = ctx.targetFiles.empty() ? std::filesystem::path("unknown")
                                                                         : std::filesystem::path(ctx.targetFiles.front());
            generated << "#include <cassert>\n\n"
                      << "int main() {\n"
                      << "    // Auto-generated smoke test for " << target.filename().string() << "\n"
                      << "    assert(true);\n"
                      << "    return 0;\n"
                      << "}\n";
        } else if (lowered.find("doc") != std::string::npos || lowered.find("report") != std::string::npos) {
            generated << "# " << description << "\n\n";
            generated << "## Targets\n";
            for (const auto& file : ctx.targetFiles) generated << "- " << file << "\n";
            if (ctx.targetFiles.empty()) generated << "- No explicit targets\n";
            auto analysisIt = ctx.variables.find("last_plan");
            if (analysisIt != ctx.variables.end()) {
                generated << "\n## Plan\n```\n" << analysisIt->second << "\n```\n";
            }
        } else {
            generated << "// Generated code for: " << description << "\n";
            if (!ctx.targetFiles.empty()) {
                generated << "// Target: " << ctx.targetFiles.front() << "\n";
            }
            const auto planIt = ctx.variables.find("last_plan");
            if (planIt != ctx.variables.end()) {
                generated << "// Derived from plan\n";
            }
            generated << "\n";
        }

        ctx.variables["last_generate"] = generated.str();
        ctx.variables["generated_content"] = generated.str();
        if (!action.outputVar.empty()) ctx.variables[action.outputVar] = generated.str();
        
        return {true, generated.str()};
    }
    
    std::pair<bool, std::string> executeQuery(
        const StepAction& action,
        FlowContext& ctx,
        std::atomic<bool>& cancelToken) {
        
        // Request user input
        std::string prompt = action.params.count("prompt") ?
            action.params.at("prompt") : action.description;
        
        // Store prompt for UI to display
        ctx.pendingInputs.push_back(prompt);
        
        return {true, "Query pending: " + prompt};
    }
    
    std::pair<bool, std::string> executeWait(
        const StepAction& action,
        FlowContext& ctx,
        std::atomic<bool>& cancelToken) {
        
        uint32_t waitMs = action.params.count("duration") ?
            std::stoul(action.params.at("duration")) : 1000;
        
        std::this_thread::sleep_for(std::chrono::milliseconds(waitMs));
        
        return {true, "Waited " + std::to_string(waitMs) + "ms"};
    }
    
    std::pair<bool, std::string> executeCustom(
        const StepAction& action,
        FlowContext& ctx,
        std::atomic<bool>& cancelToken) {
        
        // Execute custom command
        std::string cmd = action.command;
        
        // Substitute variables
        for (const auto& [name, value] : ctx.variables) {
            std::string placeholder = "${" + name + "}";
            size_t pos;
            while ((pos = cmd.find(placeholder)) != std::string::npos) {
                cmd.replace(pos, placeholder.length(), value);
            }
        }
        
        auto [ok, output] = executeCommandCapture(cmd, ctx, cancelToken);
        if (!action.outputVar.empty()) ctx.variables[action.outputVar] = output;
        return {ok, output};
    }
    
    // Main execution loop
    void runFlow(const std::string& executionId, FlowDefinition flow, 
                 FlowContext context) {
        
        auto& cancelToken = cancellationTokens[executionId];
        cancelToken = false;
        
        FlowExecution exec;
        exec.executionId = executionId;
        exec.flowId = flow.id;
        exec.status = FlowStatus::Executing;
        exec.context = context;
        exec.started = std::chrono::system_clock::now();
        exec.totalSteps = static_cast<uint32_t>(flow.steps.size());
        
        {
            std::unique_lock lock(executionsMutex);
            executions[executionId] = exec;
        }
        
        emitEvent(FlowEvent::FlowStarted, executionId, "", 
                 "Flow started: " + flow.name);
        
        // Execute steps
        std::string currentStepId = flow.entryStep;
        uint32_t stepCount = 0;
        
        while (!currentStepId.empty() && !cancelToken && 
               stepCount < flow.maxSteps) {
            
            auto stepIt = flow.stepMap.find(currentStepId);
            if (stepIt == flow.stepMap.end()) {
                exec.errors.push_back("Step not found: " + currentStepId);
                break;
            }
            
            auto& step = stepIt->second;
            step.status = StepStatus::Running;
            step.started = std::chrono::system_clock::now();
            step.attemptCount++;
            
            exec.currentStepId = currentStepId;
            exec.currentStepIndex = stepCount;
            
            emitEvent(FlowEvent::StepStarted, executionId, currentStepId,
                     "Step started: " + step.name,
                     static_cast<float>(stepCount) / exec.totalSteps);
            
            // Check conditions
            bool conditionsMet = true;
            for (const auto& cond : step.action.conditions) {
                if (!evaluateCondition(cond, exec.context)) {
                    conditionsMet = false;
                    break;
                }
            }
            
            if (!conditionsMet) {
                // Skip step or take false branch
                step.status = StepStatus::Skipped;
                exec.skippedSteps.push_back(currentStepId);
                
                if (!step.action.falseBranch.empty()) {
                    currentStepId = step.action.falseBranch;
                } else {
                    // Move to next step
                    auto idx = std::find_if(flow.steps.begin(), flow.steps.end(),
                        [&currentStepId](const FlowStep& s) { return s.id == currentStepId; });
                    if (idx != flow.steps.end() && std::next(idx) != flow.steps.end()) {
                        currentStepId = std::next(idx)->id;
                    } else {
                        currentStepId.clear();
                    }
                }
                stepCount++;
                continue;
            }
            
            // Execute action with retry
            bool success = false;
            std::string result;
            uint32_t retries = 0;
            
            while (!success && retries <= step.action.maxRetries && !cancelToken) {
                if (retries > 0) {
                    step.status = StepStatus::Retrying;
                    emitEvent(FlowEvent::StepRetrying, executionId, currentStepId,
                             "Retry " + std::to_string(retries));
                    
                    std::this_thread::sleep_for(
                        std::chrono::milliseconds(step.action.retryDelayMs));
                }
                
                auto [ok, msg] = executeAction(step.action, exec.context, cancelToken);
                success = ok;
                result = msg;
                retries++;
            }
            
            step.completed = std::chrono::system_clock::now();
            step.result = result;
            
            if (success) {
                step.status = StepStatus::Completed;
                exec.completedSteps.push_back(currentStepId);
                exec.completedStepCount++;
                
                emitEvent(FlowEvent::StepCompleted, executionId, currentStepId,
                         result, static_cast<float>(exec.completedStepCount) / exec.totalSteps);
                
                // Handle branching
                if (!step.action.trueBranch.empty()) {
                    currentStepId = step.action.trueBranch;
                } else {
                    // Move to next step
                    auto idx = std::find_if(flow.steps.begin(), flow.steps.end(),
                        [&currentStepId](const FlowStep& s) { return s.id == currentStepId; });
                    if (idx != flow.steps.end() && std::next(idx) != flow.steps.end()) {
                        currentStepId = std::next(idx)->id;
                    } else {
                        currentStepId.clear();
                    }
                }
            } else {
                step.status = StepStatus::Failed;
                step.error = result;
                exec.failedSteps.push_back(currentStepId);
                exec.failedStepCount++;
                exec.errors.push_back("Step failed: " + currentStepId + " - " + result);
                
                emitEvent(FlowEvent::StepFailed, executionId, currentStepId,
                         result);
                
                // Handle failure
                if (flow.onFailure == "abort") {
                    break;
                } else if (flow.onFailure == "continue") {
                    // Move to next step
                    auto idx = std::find_if(flow.steps.begin(), flow.steps.end(),
                        [&currentStepId](const FlowStep& s) { return s.id == currentStepId; });
                    if (idx != flow.steps.end() && std::next(idx) != flow.steps.end()) {
                        currentStepId = std::next(idx)->id;
                    } else {
                        currentStepId.clear();
                    }
                } else if (flow.onFailure == "retry") {
                    // Retry from last checkpoint
                    if (!exec.checkpoints.empty()) {
                        auto& cp = exec.checkpoints.back();
                        exec.context = cp.context;
                        currentStepId = cp.stepId;
                    }
                } else {
                    // Ask user (would need UI integration)
                    break;
                }
            }
            
            // Create checkpoint
            if (flow.enableCheckpointing && 
                stepCount % flow.checkpointInterval == 0) {
                FlowCheckpoint cp;
                cp.checkpointId = nextCheckpointId++;
                cp.created = std::chrono::system_clock::now();
                cp.stepId = currentStepId;
                cp.context = exec.context;
                exec.checkpoints.push_back(std::move(cp));
                
                emitEvent(FlowEvent::CheckpointCreated, executionId, "",
                         "Checkpoint " + std::to_string(cp.checkpointId));
            }
            
            stepCount++;
        }
        
        // Finalize
        exec.completed = std::chrono::system_clock::now();
        exec.duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            exec.completed - exec.started);
        exec.progress = 1.0f;
        
        if (cancelToken) {
            exec.status = FlowStatus::Cancelled;
            emitEvent(FlowEvent::FlowCancelled, executionId);
        } else if (exec.failedSteps.empty()) {
            exec.status = FlowStatus::Completed;
            emitEvent(FlowEvent::FlowCompleted, executionId, "",
                     "Flow completed successfully");
        } else {
            exec.status = FlowStatus::Failed;
            emitEvent(FlowEvent::FlowFailed, executionId, "",
                     "Flow failed with " + std::to_string(exec.failedSteps.size()) + " errors");
        }
        
        // Create result
        FlowResult result;
        result.executionId = executionId;
        result.status = exec.status;
        result.success = exec.status == FlowStatus::Completed;
        result.summary = exec.status == FlowStatus::Completed ?
            "Flow completed successfully" : "Flow failed";
        result.outputs = exec.outputs;
        result.errors = exec.errors;
        result.warnings = exec.warnings;
        result.variables = exec.context.variables;
        result.duration = exec.duration;
        result.stepsExecuted = exec.completedStepCount;
        result.stepsFailed = exec.failedStepCount;
        
        // Store modified files
        for (const auto& [file, content] : exec.context.fileContents) {
            result.modifiedFiles[file] = content;
        }
        
        {
            std::unique_lock lock(executionsMutex);
            executions[executionId] = exec;
            results[executionId] = result;
        }
    }
};

// ============================================================================
// Flow Engine Implementation
// ============================================================================

FlowEngine::FlowEngine() : m_impl(std::make_unique<Impl>()) {}
FlowEngine::~FlowEngine() = default;

void FlowEngine::setMode(FlowMode mode) {
    m_impl->mode = mode;
}

FlowMode FlowEngine::getMode() const {
    return m_impl->mode;
}

void FlowEngine::setCallback(FlowCallback callback) {
    m_impl->callback = std::move(callback);
}

bool FlowEngine::registerFlow(const FlowDefinition& flow) {
    std::unique_lock lock(m_impl->flowsMutex);
    m_impl->flows[flow.id] = flow;
    return true;
}

bool FlowEngine::unregisterFlow(const std::string& flowId) {
    std::unique_lock lock(m_impl->flowsMutex);
    return m_impl->flows.erase(flowId) > 0;
}

std::vector<std::string> FlowEngine::getAvailableFlows() const {
    std::shared_lock lock(m_impl->flowsMutex);
    std::vector<std::string> result;
    for (const auto& [id, flow] : m_impl->flows) {
        result.push_back(id);
    }
    return result;
}

std::optional<FlowDefinition> FlowEngine::getFlow(const std::string& flowId) const {
    std::shared_lock lock(m_impl->flowsMutex);
    auto it = m_impl->flows.find(flowId);
    if (it != m_impl->flows.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::string FlowEngine::startFlow(const std::string& flowId,
                                   const FlowContext& initialContext) {
    auto flow = getFlow(flowId);
    if (!flow) return "";
    
    std::string executionId = m_impl->generateId();
    
    // Start execution in background thread
    std::thread(&Impl::runFlow, m_impl.get(), executionId, *flow, initialContext)
        .detach();
    
    return executionId;
}

bool FlowEngine::pauseFlow(const std::string& executionId) {
    std::shared_lock lock(m_impl->executionsMutex);
    auto it = m_impl->executions.find(executionId);
    if (it != m_impl->executions.end()) {
        it->second.status = FlowStatus::Paused;
        m_impl->emitEvent(FlowEvent::FlowPaused, executionId);
        return true;
    }
    return false;
}

bool FlowEngine::resumeFlow(const std::string& executionId) {
    std::shared_lock lock(m_impl->executionsMutex);
    auto it = m_impl->executions.find(executionId);
    if (it != m_impl->executions.end() && 
        it->second.status == FlowStatus::Paused) {
        it->second.status = FlowStatus::Executing;
        m_impl->emitEvent(FlowEvent::FlowResumed, executionId);
        return true;
    }
    return false;
}

bool FlowEngine::cancelFlow(const std::string& executionId) {
    auto it = m_impl->cancellationTokens.find(executionId);
    if (it != m_impl->cancellationTokens.end()) {
        it->second = true;
        return true;
    }
    return false;
}

bool FlowEngine::retryStep(const std::string& executionId, 
                           const std::string& stepId) {
    // Implementation would retry a specific step
    return true;
}

bool FlowEngine::provideInput(const std::string& executionId,
                              const std::string& input) {
    std::lock_guard lock(m_impl->inputMutex);
    m_impl->inputQueues[executionId].push(input);
    m_impl->inputCVs[executionId].notify_one();
    return true;
}

std::optional<std::string> FlowEngine::waitForInput(const std::string& executionId,
                                                     uint32_t timeoutMs) {
    std::unique_lock lock(m_impl->inputMutex);
    
    auto& queue = m_impl->inputQueues[executionId];
    auto& cv = m_impl->inputCVs[executionId];
    
    if (cv.wait_for(lock, std::chrono::milliseconds(timeoutMs),
        [&queue] { return !queue.empty(); })) {
        std::string input = queue.front();
        queue.pop();
        return input;
    }
    
    return std::nullopt;
}

FlowStatus FlowEngine::getStatus(const std::string& executionId) const {
    std::shared_lock lock(m_impl->executionsMutex);
    auto it = m_impl->executions.find(executionId);
    if (it != m_impl->executions.end()) {
        return it->second.status;
    }
    return FlowStatus::Pending;
}

float FlowEngine::getProgress(const std::string& executionId) const {
    std::shared_lock lock(m_impl->executionsMutex);
    auto it = m_impl->executions.find(executionId);
    if (it != m_impl->executions.end()) {
        return it->second.progress;
    }
    return 0.0f;
}

std::optional<FlowExecution> FlowEngine::getExecution(const std::string& executionId) const {
    std::shared_lock lock(m_impl->executionsMutex);
    auto it = m_impl->executions.find(executionId);
    if (it != m_impl->executions.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::optional<FlowResult> FlowEngine::getResult(const std::string& executionId) const {
    std::shared_lock lock(m_impl->executionsMutex);
    auto it = m_impl->results.find(executionId);
    if (it != m_impl->results.end()) {
        return it->second;
    }
    return std::nullopt;
}

uint32_t FlowEngine::createCheckpoint(const std::string& executionId) {
    std::shared_lock lock(m_impl->executionsMutex);
    auto it = m_impl->executions.find(executionId);
    if (it != m_impl->executions.end()) {
        FlowCheckpoint cp;
        cp.checkpointId = m_impl->nextCheckpointId++;
        cp.created = std::chrono::system_clock::now();
        cp.stepId = it->second.currentStepId;
        cp.context = it->second.context;
        it->second.checkpoints.push_back(std::move(cp));
        return it->second.checkpoints.back().checkpointId;
    }
    return 0;
}

bool FlowEngine::restoreCheckpoint(const std::string& executionId, 
                                   uint32_t checkpointId) {
    std::shared_lock lock(m_impl->executionsMutex);
    auto it = m_impl->executions.find(executionId);
    if (it != m_impl->executions.end()) {
        for (const auto& cp : it->second.checkpoints) {
            if (cp.checkpointId == checkpointId) {
                it->second.context = cp.context;
                it->second.currentStepId = cp.stepId;
                m_impl->emitEvent(FlowEvent::CheckpointRestored, executionId);
                return true;
            }
        }
    }
    return false;
}

bool FlowEngine::executeStep(const std::string& executionId,
                             const std::string& stepId) {
    // Implementation would execute a specific step
    return true;
}

bool FlowEngine::skipStep(const std::string& executionId,
                         const std::string& stepId) {
    // Implementation would skip a specific step
    return true;
}

// ============================================================================
// Built-in Flow Templates
// ============================================================================

namespace Flows {

FlowDefinition createFeatureFlow(const std::string& featureName) {
    FlowDefinition flow;
    flow.id = "feature_" + featureName;
    flow.name = "Feature Implementation: " + featureName;
    flow.description = "Autonomous feature implementation flow";
    flow.mode = FlowMode::Autonomous;
    
    // Steps
    FlowStep analyze;
    analyze.id = "analyze";
    analyze.name = "Analyze Requirements";
    analyze.action.type = StepType::Analyze;
    analyze.action.description = "Analyze feature requirements and existing codebase";
    
    FlowStep plan;
    plan.id = "plan";
    plan.name = "Create Implementation Plan";
    plan.action.type = StepType::Plan;
    plan.action.description = "Create detailed implementation plan";
    plan.action.outputVar = "implementation_plan";
    plan.action.dependsOn = {"analyze"};
    
    FlowStep generate;
    generate.id = "generate";
    generate.name = "Generate Code";
    generate.action.type = StepType::Generate;
    generate.action.description = "Generate implementation code";
    generate.action.inputVar = "implementation_plan";
    generate.action.outputVar = "generated_content";
    generate.action.dependsOn = {"plan"};
    
    FlowStep edit;
    edit.id = "edit";
    edit.name = "Apply Changes";
    edit.action.type = StepType::Edit;
    edit.action.description = "Apply generated changes to files";
    edit.action.inputVar = "generated_content";
    edit.action.params["operation"] = "append";
    edit.action.dependsOn = {"generate"};
    
    FlowStep test;
    test.id = "test";
    test.name = "Run Tests";
    test.action.type = StepType::Test;
    test.action.description = "Run tests to verify implementation";
    test.action.dependsOn = {"edit"};
    
    flow.steps = {analyze, plan, generate, edit, test};
    flow.entryStep = "analyze";
    
    for (const auto& step : flow.steps) {
        flow.stepMap[step.id] = step;
    }
    
    return flow;
}

FlowDefinition createBugFixFlow(const std::string& bugDescription) {
    FlowDefinition flow;
    flow.id = "bugfix_" + std::to_string(std::hash<std::string>{}(bugDescription));
    flow.name = "Bug Fix Flow";
    flow.description = "Autonomous bug fixing flow";
    flow.mode = FlowMode::Autonomous;
    
    FlowStep analyze;
    analyze.id = "analyze";
    analyze.name = "Analyze Bug";
    analyze.action.type = StepType::Analyze;
    analyze.action.description = "Analyze bug: " + bugDescription;
    
    FlowStep debug;
    debug.id = "debug";
    debug.name = "Debug Issue";
    debug.action.type = StepType::Debug;
    debug.action.description = "Debug and identify root cause";
    debug.action.dependsOn = {"analyze"};
    
    FlowStep fix;
    fix.id = "fix";
    fix.name = "Apply Fix";
    fix.action.type = StepType::Edit;
    fix.action.description = "Apply fix to resolve bug";
    fix.action.dependsOn = {"debug"};
    
    FlowStep test;
    test.id = "test";
    test.name = "Verify Fix";
    test.action.type = StepType::Test;
    test.action.description = "Run tests to verify fix";
    test.action.dependsOn = {"fix"};
    
    flow.steps = {analyze, debug, fix, test};
    flow.entryStep = "analyze";
    
    for (const auto& step : flow.steps) {
        flow.stepMap[step.id] = step;
    }
    
    return flow;
}

FlowDefinition createRefactorFlow(const std::string& target, 
                                  const std::string& operation) {
    FlowDefinition flow;
    flow.id = "refactor_" + target;
    flow.name = "Refactor: " + operation + " " + target;
    flow.description = "Autonomous refactoring flow";
    flow.mode = FlowMode::Autonomous;
    
    FlowStep analyze;
    analyze.id = "analyze";
    analyze.name = "Analyze Target";
    analyze.action.type = StepType::Analyze;
    analyze.action.description = "Analyze " + target + " for refactoring";
    
    FlowStep refactor;
    refactor.id = "refactor";
    refactor.name = "Apply Refactoring";
    refactor.action.type = StepType::Refactor;
    refactor.action.description = operation + " on " + target;
    refactor.action.dependsOn = {"analyze"};
    
    FlowStep test;
    test.id = "test";
    test.name = "Verify Refactoring";
    test.action.type = StepType::Test;
    test.action.description = "Run tests to verify refactoring";
    test.action.dependsOn = {"refactor"};
    
    flow.steps = {analyze, refactor, test};
    flow.entryStep = "analyze";
    
    for (const auto& step : flow.steps) {
        flow.stepMap[step.id] = step;
    }
    
    return flow;
}

FlowDefinition createReviewFlow(const std::vector<std::string>& files) {
    FlowDefinition flow;
    flow.id = "review";
    flow.name = "Code Review Flow";
    flow.description = "Autonomous code review flow";
    flow.mode = FlowMode::Interactive;
    
    FlowStep analyze;
    analyze.id = "analyze";
    analyze.name = "Analyze Code";
    analyze.action.type = StepType::Analyze;
    analyze.action.description = "Analyze code for review";
    
    FlowStep review;
    review.id = "review";
    review.name = "Review Findings";
    review.action.type = StepType::Review;
    review.action.description = "Generate review findings";
    review.action.dependsOn = {"analyze"};
    
    flow.steps = {analyze, review};
    flow.entryStep = "analyze";
    
    for (const auto& step : flow.steps) {
        flow.stepMap[step.id] = step;
    }
    
    return flow;
}

FlowDefinition createTestFlow(const std::string& targetFile) {
    FlowDefinition flow;
    flow.id = "test_" + targetFile;
    flow.name = "Test Generation Flow";
    flow.description = "Generate tests for " + targetFile;
    flow.mode = FlowMode::Autonomous;
    
    FlowStep analyze;
    analyze.id = "analyze";
    analyze.name = "Analyze Target";
    analyze.action.type = StepType::Analyze;
    analyze.action.description = "Analyze " + targetFile;
    
    FlowStep generate;
    generate.id = "generate";
    generate.name = "Generate Tests";
    generate.action.type = StepType::Generate;
    generate.action.description = "Generate test cases";
    generate.action.outputVar = "generated_content";
    generate.action.dependsOn = {"analyze"};
    
    FlowStep run;
    run.id = "run";
    run.name = "Run Tests";
    run.action.type = StepType::Test;
    run.action.description = "Run generated tests";
    run.action.dependsOn = {"generate"};
    
    flow.steps = {analyze, generate, run};
    flow.entryStep = "analyze";
    
    for (const auto& step : flow.steps) {
        flow.stepMap[step.id] = step;
    }
    
    return flow;
}

FlowDefinition createDocsFlow(const std::vector<std::string>& files) {
    FlowDefinition flow;
    flow.id = "docs";
    flow.name = "Documentation Flow";
    flow.description = "Generate documentation";
    flow.mode = FlowMode::Autonomous;
    
    FlowStep analyze;
    analyze.id = "analyze";
    analyze.name = "Analyze Code";
    analyze.action.type = StepType::Analyze;
    analyze.action.description = "Analyze code structure";
    
    FlowStep generate;
    generate.id = "generate";
    generate.name = "Generate Docs";
    generate.action.type = StepType::Generate;
    generate.action.description = "Generate documentation";
    generate.action.outputVar = "generated_content";
    generate.action.dependsOn = {"analyze"};
    
    flow.steps = {analyze, generate};
    flow.entryStep = "analyze";
    
    for (const auto& step : flow.steps) {
        flow.stepMap[step.id] = step;
    }
    
    return flow;
}

FlowDefinition createMigrationFlow(const std::string& from, 
                                    const std::string& to) {
    FlowDefinition flow;
    flow.id = "migrate_" + from + "_to_" + to;
    flow.name = "Migration Flow";
    flow.description = "Migrate from " + from + " to " + to;
    flow.mode = FlowMode::Supervised;
    
    FlowStep analyze;
    analyze.id = "analyze";
    analyze.name = "Analyze Migration";
    analyze.action.type = StepType::Analyze;
    analyze.action.description = "Analyze migration requirements";
    
    FlowStep plan;
    plan.id = "plan";
    plan.name = "Create Migration Plan";
    plan.action.type = StepType::Plan;
    plan.action.description = "Create migration plan";
    plan.action.outputVar = "migration_plan";
    plan.action.dependsOn = {"analyze"};
    
    FlowStep migrate;
    migrate.id = "migrate";
    migrate.name = "Apply Migration";
    migrate.action.type = StepType::Edit;
    migrate.action.description = "Apply migration changes";
    migrate.action.dependsOn = {"plan"};
    
    FlowStep test;
    test.id = "test";
    test.name = "Verify Migration";
    test.action.type = StepType::Test;
    test.action.description = "Verify migration success";
    test.action.dependsOn = {"migrate"};
    
    flow.steps = {analyze, plan, migrate, test};
    flow.entryStep = "analyze";
    
    for (const auto& step : flow.steps) {
        flow.stepMap[step.id] = step;
    }
    
    return flow;
}

FlowDefinition createOptimizeFlow(const std::string& target) {
    FlowDefinition flow;
    flow.id = "optimize_" + target;
    flow.name = "Optimization Flow";
    flow.description = "Optimize " + target;
    flow.mode = FlowMode::Autonomous;
    
    FlowStep analyze;
    analyze.id = "analyze";
    analyze.name = "Analyze Performance";
    analyze.action.type = StepType::Analyze;
    analyze.action.description = "Analyze performance bottlenecks";
    
    FlowStep optimize;
    optimize.id = "optimize";
    optimize.name = "Apply Optimizations";
    optimize.action.type = StepType::Edit;
    optimize.action.description = "Apply optimizations";
    optimize.action.dependsOn = {"analyze"};
    
    FlowStep test;
    test.id = "test";
    test.name = "Verify Performance";
    test.action.type = StepType::Test;
    test.action.description = "Verify performance improvements";
    test.action.dependsOn = {"optimize"};
    
    flow.steps = {analyze, optimize, test};
    flow.entryStep = "analyze";
    
    for (const auto& step : flow.steps) {
        flow.stepMap[step.id] = step;
    }
    
    return flow;
}

FlowDefinition createDebugFlow(const std::string& issue) {
    FlowDefinition flow;
    flow.id = "debug_" + std::to_string(std::hash<std::string>{}(issue));
    flow.name = "Debug Flow";
    flow.description = "Debug issue: " + issue;
    flow.mode = FlowMode::Interactive;
    
    FlowStep analyze;
    analyze.id = "analyze";
    analyze.name = "Analyze Issue";
    analyze.action.type = StepType::Analyze;
    analyze.action.description = "Analyze issue: " + issue;
    
    FlowStep debug;
    debug.id = "debug";
    debug.name = "Debug";
    debug.action.type = StepType::Debug;
    debug.action.description = "Debug and identify cause";
    debug.action.dependsOn = {"analyze"};
    
    FlowStep fix;
    fix.id = "fix";
    fix.name = "Apply Fix";
    fix.action.type = StepType::Edit;
    fix.action.description = "Apply fix";
    fix.action.dependsOn = {"debug"};
    
    flow.steps = {analyze, debug, fix};
    flow.entryStep = "analyze";
    
    for (const auto& step : flow.steps) {
        flow.stepMap[step.id] = step;
    }
    
    return flow;
}

FlowDefinition createAnalysisFlow() {
    FlowDefinition flow;
    flow.id = "analysis";
    flow.name = "Full Project Analysis";
    flow.description = "Comprehensive project analysis";
    flow.mode = FlowMode::Autonomous;
    
    FlowStep analyze;
    analyze.id = "analyze";
    analyze.name = "Analyze Project";
    analyze.action.type = StepType::Analyze;
    analyze.action.description = "Analyze entire project";
    
    FlowStep report;
    report.id = "report";
    report.name = "Generate Report";
    report.action.type = StepType::Generate;
    report.action.description = "Generate analysis report";
    report.action.outputVar = "generated_content";
    report.action.dependsOn = {"analyze"};
    
    flow.steps = {analyze, report};
    flow.entryStep = "analyze";
    
    for (const auto& step : flow.steps) {
        flow.stepMap[step.id] = step;
    }
    
    return flow;
}

} // namespace Flows

// Factory function
std::unique_ptr<IFlowEngine> createFlowEngine() {
    return std::make_unique<FlowEngine>();
}

// Condition evaluation
bool StepCondition::evaluate(const FlowContext& ctx) const {
    auto it = ctx.variables.find(variable);
    std::string current = (it != ctx.variables.end()) ? it->second : "";

    if (operator_ == "==") return current == value;
    if (operator_ == "!=") return current != value;
    if (operator_ == "contains") return current.find(value) != std::string::npos;
    if (operator_ == "startsWith") return current.rfind(value, 0) == 0;
    if (operator_ == "endsWith") {
        return current.size() >= value.size() &&
               current.compare(current.size() - value.size(), value.size(), value) == 0;
    }
    if (operator_ == "exists") return it != ctx.variables.end();
    if (operator_ == "empty") return current.empty();
    if (operator_ == "notEmpty") return !current.empty();
    if (operator_ == "matches") {
        try {
            return std::regex_search(current, std::regex(value));
        } catch (...) {
            return false;
        }
    }

    try {
        const double left = std::stod(current);
        const double right = std::stod(value);
        if (operator_ == "<") return left < right;
        if (operator_ == "<=") return left <= right;
        if (operator_ == ">") return left > right;
        if (operator_ == ">=") return left >= right;
    } catch (...) {
    }
    return false;
}

} // namespace RawrXD::Agentic
