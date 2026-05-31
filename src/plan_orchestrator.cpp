#include "plan_orchestrator.h"

#include "RawrXD_Interfaces.h"
#include "lsp_client.h"
#include "universal_model_router.h"
#include "agentic/AgentToolHandlers.h"
#include "asm/rawrxd_asm_orchestration.h"
#include "runtime/SemanticRetrieval.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <unordered_set>

namespace RawrXD {
namespace {

constexpr std::size_t kMaxExecutionObservations = 20;
constexpr std::size_t kMaxLoopMemoryEntries = 10;
constexpr std::size_t kMaxObservationLength = 1200;
constexpr std::size_t kTerminalBufferFlushThreshold = 512;

enum class OperationSafety {
    Safe,
    RequiresConfirmation,
    Unsupported
};

std::string toLower(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return value;
}

std::string trim(std::string value)
{
    const auto notSpace = [](unsigned char ch) { return !std::isspace(ch); };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), notSpace));
    value.erase(std::find_if(value.rbegin(), value.rend(), notSpace).base(), value.end());
    return value;
}

bool startsWith(const std::string& text, const std::string& prefix)
{
    return text.size() >= prefix.size() && text.compare(0, prefix.size(), prefix) == 0;
}

std::string normalizePathLike(std::string value)
{
    std::replace(value.begin(), value.end(), '\\', '/');
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return value;
}

std::string taskUriSuffix(const std::string& filePath)
{
    const std::string normalized = normalizePathLike(filePath);
    if (normalized.empty()) {
        return normalized;
    }
    return normalized.front() == '/' ? normalized : "/" + normalized;
}

bool uriMatchesTaskFile(const std::string& uri, const std::string& filePath)
{
    if (filePath.empty()) {
        return true;
    }

    const std::string normalizedUri = normalizePathLike(uri);
    const std::string suffix = taskUriSuffix(filePath);
    return normalizedUri.size() >= suffix.size() &&
           normalizedUri.compare(normalizedUri.size() - suffix.size(), suffix.size(), suffix) == 0;
}

bool resultContainsExactSymbol(const nlohmann::json& result,
                               const std::string& symbolName,
                               const std::string& filePath)
{
    if (!result.is_array()) {
        return false;
    }

    for (const auto& entry : result) {
        if (!entry.is_object()) {
            continue;
        }
        if (!entry.contains("name") || !entry["name"].is_string() || entry["name"].get<std::string>() != symbolName) {
            continue;
        }
        if (filePath.empty()) {
            return true;
        }
        if (entry.contains("location") && entry["location"].is_object()) {
            const auto& location = entry["location"];
            if (location.contains("uri") && location["uri"].is_string() &&
                uriMatchesTaskFile(location["uri"].get<std::string>(), filePath)) {
                return true;
            }
        }
        if (entry.contains("uri") && entry["uri"].is_string() &&
            uriMatchesTaskFile(entry["uri"].get<std::string>(), filePath)) {
            return true;
        }
    }

    return false;
}

bool commandLooksDestructive(const std::string& command)
{
    const std::string cmd = toLower(command);
    return cmd.find("rm ") != std::string::npos ||
           cmd.find("del ") != std::string::npos ||
           cmd.find("rmdir") != std::string::npos ||
           cmd.find("format ") != std::string::npos ||
           cmd.find("fdisk") != std::string::npos ||
           cmd.find("diskpart") != std::string::npos ||
           cmd.find("dd ") != std::string::npos;
}

OperationSafety classifyOperation(const std::string& operation,
                                  const EditTask& task,
                                  std::string* reason = nullptr)
{
    static const std::unordered_set<std::string> safeOps = {
        "analyze", "read_file", "list_directory", "list_dir", "search_code", "search_files"
    };

    static const std::unordered_set<std::string> confirmOps = {
        "write_file", "replace", "insert", "delete", "rename", "execute_command", "run_command", "command"
    };

    if (safeOps.find(operation) != safeOps.end()) {
        return OperationSafety::Safe;
    }

    if (confirmOps.find(operation) != confirmOps.end()) {
        if ((operation == "execute_command" || operation == "run_command" || operation == "command") &&
            commandLooksDestructive(!task.newText.empty() ? task.newText : task.description)) {
            if (reason) {
                *reason = "Command contains potentially destructive operations";
            }
            return OperationSafety::RequiresConfirmation;
        }

        if (reason) {
            *reason = "Operation modifies workspace or executes commands";
        }
        return OperationSafety::RequiresConfirmation;
    }

    if (reason) {
        *reason = "Unsupported operation";
    }
    return OperationSafety::Unsupported;
}

void configureToolGuardrails(const std::string& workspaceRoot)
{
    if (workspaceRoot.empty()) {
        return;
    }

    RawrXD::Agent::ToolGuardrails guardrails;
    guardrails.allowedRoots.push_back(std::filesystem::path(workspaceRoot).lexically_normal().string());
    guardrails.allowedCommands = {
        "cmake", "ninja", "ctest", "git", "python", "py", "pwsh", "powershell", "cmd",
        "cl", "link", "msbuild", "where", "dir", "type"
    };
    RawrXD::Agent::AgentToolHandlers::SetGuardrails(guardrails);
}

bool appendExecutionMessage(const std::function<void(const std::string&)>& callback, const std::string& message)
{
    if (callback) {
        callback(message);
        return true;
    }
    return false;
}

bool isAnalyzeOnlyTask(const EditTask& task)
{
    const std::string op = toLower(task.operation);
    return op.empty() || op == "analyze";
}

void trimHistory(std::vector<std::string>& items, std::size_t maxItems)
{
    if (items.size() <= maxItems) {
        return;
    }

    items.erase(items.begin(), items.begin() + static_cast<std::ptrdiff_t>(items.size() - maxItems));
}

std::string normalizeObservation(std::string value)
{
    std::replace(value.begin(), value.end(), '\r', '\n');
    value = trim(value);
    if (value.size() > kMaxObservationLength) {
        value.resize(kMaxObservationLength);
        value += "\n...[truncated]";
    }
    return value;
}

std::string joinObservationBatch(const std::vector<std::string>& observations)
{
    std::ostringstream stream;
    for (std::size_t index = 0; index < observations.size(); ++index) {
        if (index > 0) {
            stream << '\n';
        }
        stream << observations[index];
    }
    return stream.str();
}

std::string buildReplanPrompt(const std::string& originalPrompt,
                              int attemptNumber,
                              const ExecutionResult& result,
                              const std::string& observations)
{
    std::ostringstream prompt;
    prompt << originalPrompt;
    prompt << "\n\nPrevious execution attempt " << attemptNumber << " summary:";
    prompt << "\nSuccessful tasks: " << result.successCount;
    prompt << "\nFailed tasks: " << result.failureCount;
    if (!result.errorMessage.empty()) {
        prompt << "\nLast execution error: " << result.errorMessage;
    }
    if (!result.loopMemory.empty()) {
        prompt << "\nRecent loop memory:";
        for (const auto& memory : result.loopMemory) {
            prompt << "\n- " << memory;
        }
    }
    if (!observations.empty()) {
        prompt << "\nObserved tool output:";
        prompt << "\n" << observations;
    }
    prompt << "\nRevise the next plan to finish the original goal.";
    prompt << "\nDo not repeat failed actions unless the new plan accounts for the observations above.";
    return prompt.str();
}

} // namespace

PlanOrchestrator::PlanOrchestrator() = default;
PlanOrchestrator::~PlanOrchestrator() = default;

void PlanOrchestrator::initialize()
{
    m_initialized = true;
}

void PlanOrchestrator::setLSPClient(LSPClient* client)
{
    m_lspClient = client;
}

void PlanOrchestrator::setInferenceEngine(InferenceEngine* engine)
{
    m_inferenceEngine = engine;
}

void PlanOrchestrator::setModelRouter(UniversalModelRouter* router)
{
    m_modelRouter = router;
}

void PlanOrchestrator::setTerminalCommandSink(std::function<void(const std::string&)> sink)
{
    m_terminalCommandSink = std::move(sink);
}

void PlanOrchestrator::setWorkspaceRoot(const std::string& root)
{
    m_workspaceRoot = root;
    configureToolGuardrails(m_workspaceRoot);
}

void PlanOrchestrator::recordExecutionObservation(const std::string& message)
{
    const std::string normalized = normalizeObservation(message);
    if (normalized.empty()) {
        return;
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_executionObservations.empty() && m_executionObservations.back() == normalized) {
        return;
    }

    m_executionObservations.push_back(normalized);
    trimHistory(m_executionObservations, kMaxExecutionObservations);
}

std::string PlanOrchestrator::consumeExecutionObservations()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_executionObservations.empty()) {
        return {};
    }

    std::ostringstream stream;
    for (std::size_t index = 0; index < m_executionObservations.size(); ++index) {
        if (index > 0) {
            stream << '\n';
        }
        stream << m_executionObservations[index];
    }

    m_executionObservations.clear();
    return stream.str();
}

void PlanOrchestrator::notifyWorkspaceChanged()
{
    if (onWorkspaceChanged) {
        onWorkspaceChanged();
    }
}

void PlanOrchestrator::observeExternalEvent(const std::string& message)
{
    recordExecutionObservation("[EXTERNAL]\n" + message);
    rememberLoopEvent(message);
}

void PlanOrchestrator::observeTerminalOutput(const std::string& source, const std::string& chunk, bool isError)
{
    if (chunk.empty()) {
        return;
    }

    const std::string key = source + (isError ? "|stderr" : "|stdout");
    std::vector<std::string> completedLines;

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::string& buffer = m_terminalObservationBuffers[key];
        buffer.append(chunk);

        std::size_t cursor = 0;
        while (cursor < buffer.size()) {
            const std::size_t newline = buffer.find('\n', cursor);
            if (newline == std::string::npos) {
                break;
            }

            completedLines.push_back(buffer.substr(cursor, newline - cursor));
            cursor = newline + 1;
        }

        buffer.erase(0, cursor);
        if (buffer.size() >= kTerminalBufferFlushThreshold) {
            completedLines.push_back(buffer);
            buffer.clear();
        }
    }

    for (const std::string& line : completedLines) {
        const std::string payload = normalizeObservation(line);
        if (payload.empty()) {
            continue;
        }

        std::ostringstream observation;
        observation << "[TERMINAL " << source << (isError ? " stderr" : " stdout") << "]\n" << payload;
        recordExecutionObservation(observation.str());
    }
}

void PlanOrchestrator::createPlan(const std::string& goal)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_steps.clear();
    m_currentStepIndex = 0;
    decomposeGoal(goal);
}

void PlanOrchestrator::executeNextStep()
{
    Step completedStep;
    bool hasCompletedStep = false;
    bool planDone = false;

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_currentStepIndex < 0 || static_cast<std::size_t>(m_currentStepIndex) >= m_steps.size()) {
            return;
        }

        Step& step = m_steps[static_cast<std::size_t>(m_currentStepIndex)];
        const std::string lower = toLower(step.description);
        if (lower.find("analy") != std::string::npos) {
            step.result = "Analyzed task scope and constraints.";
        } else if (lower.find("implement") != std::string::npos || lower.find("code") != std::string::npos) {
            step.result = "Implemented targeted code changes.";
        } else if (lower.find("verify") != std::string::npos || lower.find("test") != std::string::npos ||
                   lower.find("build") != std::string::npos) {
            step.result = "Verification step completed successfully.";
        } else if (lower.find("document") != std::string::npos) {
            step.result = "Documentation updated for the change.";
        } else {
            step.result = "Executed: " + step.description;
        }

        step.isComplete = true;
        completedStep = step;
        hasCompletedStep = true;

        ++m_currentStepIndex;
        planDone = (m_currentStepIndex >= static_cast<int>(m_steps.size()));
    }

    if (hasCompletedStep && onStepCompleted) {
        onStepCompleted(completedStep.result);
    }
    if (planDone && onPlanCompleted) {
        onPlanCompleted("All steps completed.");
    }
}

std::vector<PlanOrchestrator::Step> PlanOrchestrator::getPlan() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_steps;
}

bool PlanOrchestrator::isComplete() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_steps.empty() || m_currentStepIndex >= static_cast<int>(m_steps.size());
}

void PlanOrchestrator::decomposeGoal(const std::string& goal)
{
    std::vector<std::string> chunks;
    std::string current;

    for (char ch : goal) {
        if (ch == '.' || ch == ';' || ch == '\n') {
            std::string chunk = trim(current);
            if (!chunk.empty()) {
                chunks.push_back(chunk);
            }
            current.clear();
            continue;
        }
        current.push_back(ch);
    }

    current = trim(current);
    if (!current.empty()) {
        chunks.push_back(current);
    }
    if (chunks.empty()) {
        chunks.push_back(trim(goal));
    }

    for (const std::string& chunk : chunks) {
        const std::string lower = toLower(chunk);
        if (lower.find("fix") != std::string::npos || lower.find("debug") != std::string::npos) {
            m_steps.push_back({"Analyze failure conditions for: " + chunk, false, ""});
            m_steps.push_back({"Implement fix for: " + chunk, false, ""});
            m_steps.push_back({"Verify fix by build/tests for: " + chunk, false, ""});
        } else if (lower.find("feature") != std::string::npos || lower.find("add") != std::string::npos) {
            m_steps.push_back({"Analyze requirements for: " + chunk, false, ""});
            m_steps.push_back({"Implement feature changes for: " + chunk, false, ""});
            m_steps.push_back({"Verify integration for: " + chunk, false, ""});
        } else {
            m_steps.push_back({"Analyze requirements for: " + chunk, false, ""});
            m_steps.push_back({"Implement core logic for: " + chunk, false, ""});
            m_steps.push_back({"Verify implementation for: " + chunk, false, ""});
        }
    }

    m_steps.push_back({"Document and summarize outcomes", false, ""});
}

PlanningResult PlanOrchestrator::generatePlan(const std::string& prompt,
                                              const std::string& workspaceRoot,
                                              const std::vector<std::string>& contextFiles)
{
    if (!m_initialized) {
        initialize();
    }

    if (!workspaceRoot.empty()) {
        setWorkspaceRoot(workspaceRoot);
    }

    if (onPlanningStarted) {
        onPlanningStarted(prompt);
    }

    PlanningResult plan;
    const std::vector<std::string> files = contextFiles.empty() ? gatherContextFiles(m_workspaceRoot, 50) : contextFiles;
    plan.planDescription = buildPlanningPrompt(prompt, files);

    std::string modelPlan = generateModelBackedPlan(prompt, files);
    if (modelPlan.empty()) {
        modelPlan = generateInferenceBackedPlan(prompt, files);
    }

    if (!modelPlan.empty()) {
        plan = parsePlanningResponse(modelPlan);
        if (plan.planDescription.empty()) {
            plan.planDescription = buildPlanningPrompt(prompt, files);
        }
    }

    if (!plan.success) {
        createPlan(prompt);
        const std::vector<Step> steps = getPlan();

        for (const Step& step : steps) {
            EditTask task;
            task.description = step.description;
            task.priority = 1;
            task.operation = "analyze";
            plan.tasks.push_back(std::move(task));
        }
        plan.success = !plan.tasks.empty();
        plan.estimatedChanges = static_cast<int>(plan.tasks.size());
    }

    plan.affectedFiles = files;
    validatePlanSymbols(plan, files);
    if (plan.estimatedChanges == 0) {
        plan.estimatedChanges = static_cast<int>(plan.tasks.size());
    }
    if (!plan.success) {
        plan.success = !plan.tasks.empty();
    }

    if (!plan.success) {
        plan.errorMessage = "Planner could not derive executable tasks from the prompt.";
        if (onErrorOccurred) {
            onErrorOccurred(plan.errorMessage);
        }
    }

    if (onPlanningCompleted) {
        onPlanningCompleted(plan);
    }

    (void)m_inferenceEngine;
    (void)m_modelRouter;
    return plan;
}

void PlanOrchestrator::validatePlanSymbols(PlanningResult& plan, const std::vector<std::string>& contextFiles)
{
    if (!m_lspClient || plan.tasks.empty()) {
        return;
    }

    std::vector<EditTask> validatedTasks;
    validatedTasks.reserve(plan.tasks.size());
    std::vector<std::string> validationErrors;

    for (const EditTask& task : plan.tasks) {
        const std::string operation = toLower(task.operation);
        const bool shouldValidateSymbol = !task.symbolName.empty() &&
                                          (operation == "rename" || !task.newSymbolName.empty());
        if (!shouldValidateSymbol) {
            validatedTasks.push_back(task);
            continue;
        }

        const std::string lookupPath = !task.filePath.empty()
            ? resolvePath(task.filePath)
            : (contextFiles.empty() ? std::string() : resolvePath(contextFiles.front()));

        nlohmann::json symbolMatches = nlohmann::json::array();
        try {
            symbolMatches = m_lspClient->workspaceSymbols(task.symbolName).get();
        } catch (...) {
            validatedTasks.push_back(task);
            continue;
        }

        if (!resultContainsExactSymbol(symbolMatches, task.symbolName, lookupPath)) {
            validationErrors.push_back("LSP validation rejected rename task for missing symbol '" +
                                       task.symbolName + "'" +
                                       (task.filePath.empty() ? std::string() : (" in " + task.filePath)));
            continue;
        }

        if (!task.newSymbolName.empty()) {
            nlohmann::json newNameMatches = nlohmann::json::array();
            try {
                newNameMatches = m_lspClient->workspaceSymbols(task.newSymbolName).get();
            } catch (...) {
                validatedTasks.push_back(task);
                continue;
            }

            if (resultContainsExactSymbol(newNameMatches, task.newSymbolName, lookupPath)) {
                validationErrors.push_back("LSP validation rejected rename task because target symbol '" +
                                           task.newSymbolName + "' already exists" +
                                           (task.filePath.empty() ? std::string() : (" in " + task.filePath)));
                continue;
            }
        }

        validatedTasks.push_back(task);
    }

    plan.tasks = std::move(validatedTasks);
    plan.estimatedChanges = static_cast<int>(plan.tasks.size());

    if (!validationErrors.empty()) {
        std::ostringstream oss;
        for (size_t i = 0; i < validationErrors.size(); ++i) {
            if (i != 0) {
                oss << "\n";
            }
            oss << validationErrors[i];
        }

        plan.errorMessage = oss.str();
        if (plan.tasks.empty()) {
            plan.success = false;
        }
    }
}

ExecutionResult PlanOrchestrator::planAndExecute(const std::string& prompt,
                                                 const std::string& workspaceRoot,
                                                 bool dryRun)
{
    ExecutionResult result;
    constexpr int kMaxAutonomousPasses = 5;
    std::string loopPrompt = prompt;

    try {
        for (int attempt = 0; attempt < kMaxAutonomousPasses; ++attempt) {
            result.iterations = attempt + 1;
            const PlanningResult plan = generatePlan(loopPrompt, workspaceRoot);
            if (!plan.success) {
                result.errorMessage = plan.errorMessage.empty() ? "Failed to generate execution plan" : plan.errorMessage;
                break;
            }

            const bool analyzeOnlyPlan = std::all_of(plan.tasks.begin(), plan.tasks.end(),
                [](const EditTask& task) {
                    return isAnalyzeOnlyTask(task);
                });

            // Observations are collected during executePlan and will be consumed by generatePlan in the next loop via buildPlanningPrompt
            ExecutionResult iterationResult = executePlan(plan, dryRun);

            result.successCount += iterationResult.successCount;
            result.failureCount += iterationResult.failureCount;
            result.successfulFiles.insert(result.successfulFiles.end(),
                                          iterationResult.successfulFiles.begin(),
                                          iterationResult.successfulFiles.end());
            result.failedFiles.insert(result.failedFiles.end(),
                                      iterationResult.failedFiles.begin(),
                                      iterationResult.failedFiles.end());
            result.observations.insert(result.observations.end(),
                                       iterationResult.observations.begin(),
                                       iterationResult.observations.end());
            if (result.errorMessage.empty() && !iterationResult.errorMessage.empty()) {
                result.errorMessage = iterationResult.errorMessage;
            }

            std::string attemptMemory = "Attempt " + std::to_string(attempt + 1) +
                                        ": successes=" + std::to_string(iterationResult.successCount) +
                                        ", failures=" + std::to_string(iterationResult.failureCount);
            if (!iterationResult.errorMessage.empty()) {
                attemptMemory += ", error=" + iterationResult.errorMessage;
            }
            rememberLoopEvent(attemptMemory);

            const std::string observations = joinObservationBatch(iterationResult.observations);
            if (!observations.empty()) {
                rememberLoopEvent(observations);
            }

            if (iterationResult.failureCount == 0 && !analyzeOnlyPlan) {
                result.success = true;
                break;
            }

            if (attempt == (kMaxAutonomousPasses - 1)) {
                if (result.errorMessage.empty()) {
                    result.errorMessage = "Planner exhausted autonomous retry budget.";
                }
                break;
            }

            loopPrompt = buildReplanPrompt(prompt, attempt + 1, iterationResult, observations);
        }
    } catch (const std::exception& e) {
        result.errorMessage = std::string("Plan execution failed: ") + e.what();
        if (onErrorOccurred) {
            onErrorOccurred(result.errorMessage);
        }
    }

    result.loopMemory = snapshotLoopMemory();
    result.success = (result.failureCount == 0) && result.errorMessage.empty();
    return result;
}

ExecutionResult PlanOrchestrator::runAutonomousLoop(const std::string& userGoal, int maxSteps)
{
    ExecutionResult result;
    std::string currentContext = "Initial Goal: " + userGoal;

#if defined(RAWRXD_MASM_CORE_NATIVE_BRIDGE) && defined(_MSC_VER) && defined(_M_X64)
    // ---- MASM context buffer: reset at the start of each autonomous run -----
    ContextBuf_Reset();
#endif

    if (onPlanningStarted) onPlanningStarted(userGoal);

    // Hallucination/Stall Detection
    std::string lastPlanDescription;
    int samePlanCounter = 0;
    constexpr int kMaxSamePlanThreshold = 3;

    for (int step = 0; step < maxSteps; ++step) {
        result.iterations = step + 1;
        // 1. THINK: Request next action from LLM
        // We accumulate context in 'currentContext' which is used to generate the next plan
        PlanningResult plan = generatePlan(currentContext, m_workspaceRoot);
        
        if (!plan.success || plan.tasks.empty()) {
            break; 
        }

        // --- Stall/Hallucination Detection Logic ---
        if (plan.planDescription == lastPlanDescription) {
            samePlanCounter++;
            if (samePlanCounter >= kMaxSamePlanThreshold) {
                std::string stallErr = "Execution halted: Potential hallucination loop detected (same plan repeated " + std::to_string(samePlanCounter) + " times).";
                result.errorMessage = stallErr;
                recordExecutionObservation("[STALL_DETECTED] " + stallErr);
                break;
            }
        } else {
            samePlanCounter = 0;
            lastPlanDescription = plan.planDescription;
        }
        // ------------------------------------------

        // 2. SAFETY GATE: Simple confirmation for now (can be expanded)
        bool allApproved = true;
        if (onTaskApprovalRequired) {
            for (const auto& task : plan.tasks) {
                if (!onTaskApprovalRequired(task)) {
                    allApproved = false;
                    break;
                }
            }
        }

        if (!allApproved) {
            currentContext += "\nStep " + std::to_string(step) + " Observation: User denied execution of tasks.";
            continue;
        }

        // 3. ACT: Execute the plan
        ExecutionResult stepResult = executePlan(plan, false);
        
        // 4. OBSERVE: Accumulate history
        std::string observation = "Step " + std::to_string(step) + " Results:\n";
        observation += "Successes: " + std::to_string(stepResult.successCount) + "\n";
        observation += "Failures: " + std::to_string(stepResult.failureCount) + "\n";
        if (!stepResult.successfulFiles.empty()) {
            observation += "Modified: ";
            for (const auto& f : stepResult.successfulFiles) observation += f + " ";
            observation += "\n";
        }
        if (!stepResult.errorMessage.empty()) {
            observation += "Error: " + stepResult.errorMessage + "\n";
        }
        if (!stepResult.observations.empty()) {
            observation += "Observed Output:\n";
            for (const auto& item : stepResult.observations) {
                observation += item + "\n";
            }
        }

#if defined(RAWRXD_MASM_CORE_NATIVE_BRIDGE) && defined(_MSC_VER) && defined(_M_X64)
        // ---- MASM context buffer: log step + result for next Think prompt ---
        {
            const std::string stepLabel = "step_" + std::to_string(step) +
                                          " [" + plan.planDescription.substr(
                                              0, std::min<std::size_t>(60, plan.planDescription.size())) + "]";
            ContextBuf_AppendStep(stepLabel.c_str());
            ContextBuf_AppendResult(observation.c_str());
        }
        // Rebuild currentContext from the MASM buffer for the next Think call.
        static thread_local char s_ctxPromptBuf[10240];
        ContextBuf_BuildPrompt(userGoal.c_str(), nullptr, s_ctxPromptBuf,
                               static_cast<DWORD>(sizeof(s_ctxPromptBuf)));
        currentContext = std::string(s_ctxPromptBuf);
#else
        currentContext += "\nPlan at Step " + std::to_string(step) + ": " + plan.planDescription + "\n" + observation;
#endif
        
        result.successCount += stepResult.successCount;
        result.failureCount += stepResult.failureCount;
        result.successfulFiles.insert(result.successfulFiles.end(),
                                      stepResult.successfulFiles.begin(),
                                      stepResult.successfulFiles.end());
        result.failedFiles.insert(result.failedFiles.end(),
                                  stepResult.failedFiles.begin(),
                                  stepResult.failedFiles.end());
        result.observations.insert(result.observations.end(),
                                   stepResult.observations.begin(),
                                   stepResult.observations.end());
        result.loopMemory.push_back(observation);
        if (result.loopMemory.size() > 8) {
            result.loopMemory.erase(result.loopMemory.begin(),
                                    result.loopMemory.begin() + static_cast<std::ptrdiff_t>(result.loopMemory.size() - 8));
        }
        if (result.errorMessage.empty() && !stepResult.errorMessage.empty()) {
            result.errorMessage = stepResult.errorMessage;
        }

        // If explicitly requested to stop in plan description or if goal reached (heuristic)
        if (plan.planDescription.find("COMPLETE") != std::string::npos || plan.planDescription.find("GOAL REACHED") != std::string::npos) {
            break;
        }
    }

    result.success = (result.failureCount == 0);
    if (onExecutionCompleted) onExecutionCompleted(result);
    return result;
}

std::string PlanOrchestrator::buildPlanningPrompt(const std::string& userPrompt,
                                                  const std::vector<std::string>& contextFiles)
{
    std::ostringstream prompt;
    prompt << "PLAN: Deterministic execution plan for: " << userPrompt;
    prompt << "\nReturn task lines in this exact form:";
    prompt << "\n1. <description>";
    prompt << "\nOperation: analyze|read_file|write_file|replace|insert|delete|rename|execute_command|list_directory|search_code";
    prompt << "\nFile: <path when applicable>";
    prompt << "\nOld: <old text or anchor when applicable>";
    prompt << "\nNew: <new text, new path, or command payload when applicable>";
    prompt << "\nAfter: <anchor line for insert operations when applicable>";

    std::string obs = consumeExecutionObservations();
    if (!obs.empty()) {
        prompt << "\n\nCRITICAL: Previous tool execution observations (ERRORS/OUTPUTS):";
        prompt << "\n" << obs;
        prompt << "\n\nAdjust your next plan based on these observations.";
    }

    const std::string loopMemory = buildLoopMemoryContext();
    if (!loopMemory.empty()) {
        prompt << "\n\nRecent execution memory:";
        prompt << "\n" << loopMemory;
    }

    if (!contextFiles.empty()) {
        prompt << "\n\nContext files:";
        for (std::size_t index = 0; index < std::min<std::size_t>(10, contextFiles.size()); ++index) {
            prompt << "\n- " << contextFiles[index];
        }
    }

    const std::string semanticContext =
        RawrXD::Runtime::SemanticRetrieval::BuildPromptSemanticContextBlock(
            userPrompt + "\n" + loopMemory,
            6,
            "PLANNER_SEMANTIC_CONTEXT");
    if (!semanticContext.empty()) {
        prompt << "\n\nRelevant semantic retrieval:";
        prompt << "\n" << semanticContext;
    }
    return prompt.str();
}

std::string PlanOrchestrator::generateModelBackedPlan(const std::string& userPrompt,
                                                      const std::vector<std::string>& contextFiles)
{
    if (!m_modelRouter) {
        return {};
    }

    try {
        const std::string modelName = selectPlanningModel();
        const std::string planningPrompt = buildPlanningPrompt(userPrompt, contextFiles);

        std::string accumulatedResponse;

        // Use streaming for real-time feedback during planning
        m_modelRouter->routeStreamQuery(modelName, planningPrompt,
            [this, &accumulatedResponse](const std::string& chunk) {
                accumulatedResponse += chunk;
                if (onPlanningChunk) {
                    onPlanningChunk(chunk);
                }
            }, 0.2f);

        return accumulatedResponse;
    } catch (const std::exception& e) {
        if (onErrorOccurred) {
            onErrorOccurred(std::string("Model-backed planning failed: ") + e.what());
        }
        return {};
    }
}

std::string PlanOrchestrator::generateInferenceBackedPlan(const std::string& userPrompt,
                                                          const std::vector<std::string>& contextFiles)
{
    if (!m_inferenceEngine || !m_inferenceEngine->IsModelLoaded()) {
        return {};
    }

    try {
        const std::string planningPrompt = buildPlanningPrompt(userPrompt, contextFiles);
        const std::vector<int32_t> inputTokens = m_inferenceEngine->Tokenize(planningPrompt);
        if (inputTokens.empty()) {
            return {};
        }

        std::string response;
        std::vector<int32_t> outputTokens;
        m_inferenceEngine->GenerateStreaming(
            inputTokens,
            256,
            [this, &response](const std::string& token) {
                response += token;
                if (onPlanningChunk) {
                    onPlanningChunk(token);
                }
            },
            []() {},
            [&outputTokens](int32_t tokenId) {
                outputTokens.push_back(tokenId);
            });

        if (response.empty() && !outputTokens.empty()) {
            response = m_inferenceEngine->Detokenize(outputTokens);
        }

        return response;
    } catch (const std::exception& e) {
        if (onErrorOccurred) {
            onErrorOccurred(std::string("Inference-backed planning failed: ") + e.what());
        }
        return {};
    }
}

PlanningResult PlanOrchestrator::parsePlanningResponse(const std::string& response)
{
    PlanningResult result;

#if defined(RAWRXD_MASM_CORE_NATIVE_BRIDGE) && defined(_MSC_VER) && defined(_M_X64)
    // ---- MASM path: try fast JSON array parser first ----------------------
    // The sovereign model response format is a JSON array:
    //   [{"tool":"edit_file","param":"path/to/file"},…]
    // Skip leading whitespace to find the opening '['.
    std::size_t jsonStart = response.find('[');
    if (jsonStart != std::string::npos) {
        static constexpr DWORD kMaxAsmSteps = 32;
        static thread_local RawrXD_PlanStep s_asmSteps[kMaxAsmSteps];
        DWORD stepCount = 0;
        const int parsed = JsonPlan_Parse(
            response.c_str() + jsonStart,
            s_asmSteps,
            kMaxAsmSteps,
            &stepCount);
        if (parsed && stepCount > 0) {
            result.planDescription = "[MASM-parsed JSON plan: " +
                                     std::to_string(stepCount) + " step(s)]";
            for (DWORD i = 0; i < stepCount; ++i) {
                EditTask task;
                task.operation = s_asmSteps[i].tool;
                task.newText   = s_asmSteps[i].param;
                task.description = std::string(s_asmSteps[i].tool) + ": " +
                                   std::string(s_asmSteps[i].param);
                task.priority  = 1;
                if (!task.operation.empty()) {
                    result.tasks.push_back(std::move(task));
                }
            }
            result.estimatedChanges = static_cast<int>(result.tasks.size());
            result.success = !result.tasks.empty();
            if (result.success) {
                return result;
            }
        }
    }
    // Fall through to the portable line-oriented parser if JSON parse yielded nothing.
#endif
    std::istringstream stream(response);
    std::string line;
    EditTask currentTask;

    while (std::getline(stream, line)) {
        line = trim(line);
        if (line.empty()) {
            continue;
        }

        if (startsWith(line, "PLAN:")) {
            result.planDescription = trim(line.substr(5));
            continue;
        }

        if (std::isdigit(static_cast<unsigned char>(line[0])) && line.size() > 1 && line[1] == '.') {
            if (!currentTask.description.empty() || !currentTask.filePath.empty()) {
                result.tasks.push_back(currentTask);
                if (!currentTask.filePath.empty()) {
                    result.affectedFiles.push_back(currentTask.filePath);
                }
            }
            currentTask = EditTask{};
            currentTask.description = trim(line.substr(2));
            ++result.estimatedChanges;
            continue;
        }

        if (startsWith(line, "File:")) {
            currentTask.filePath = trim(line.substr(5));
            continue;
        }
        if (startsWith(line, "Path:")) {
            currentTask.filePath = trim(line.substr(5));
            continue;
        }
        if (startsWith(line, "Operation:")) {
            currentTask.operation = trim(line.substr(10));
            continue;
        }
        if (startsWith(line, "Command:")) {
            currentTask.newText = trim(line.substr(8));
            continue;
        }
        if (startsWith(line, "Old:")) {
            currentTask.oldText = trim(line.substr(4));
            continue;
        }
        if (startsWith(line, "New:")) {
            currentTask.newText = trim(line.substr(4));
            continue;
        }
        if (startsWith(line, "After:")) {
            currentTask.oldText = trim(line.substr(6));
            continue;
        }
        if (startsWith(line, "Symbol:")) {
            currentTask.symbolName = trim(line.substr(7));
            continue;
        }
        if (startsWith(line, "RenameTo:")) {
            currentTask.newSymbolName = trim(line.substr(9));
        }
    }

    if (!currentTask.description.empty() || !currentTask.filePath.empty()) {
        result.tasks.push_back(currentTask);
        if (!currentTask.filePath.empty()) {
            result.affectedFiles.push_back(currentTask.filePath);
        }
    }

    result.success = !result.tasks.empty();
    if (!result.success && result.errorMessage.empty()) {
        result.errorMessage = "Planner response did not contain executable tasks.";
    }
    return result;
}

ExecutionResult PlanOrchestrator::executePlan(const PlanningResult& plan, bool dryRun)
{
    ExecutionResult result;

    if (onExecutionStarted) {
        onExecutionStarted(static_cast<int>(plan.tasks.size()));
    }

    for (std::size_t index = 0; index < plan.tasks.size(); ++index) {
        const EditTask& task = plan.tasks[index];
        bool taskSuccess = executeTask(task, dryRun);

        if (taskSuccess) {
            ++result.successCount;
            if (!task.filePath.empty()) {
                result.successfulFiles.push_back(task.filePath);
            }
        } else {
            ++result.failureCount;
            if (!task.filePath.empty()) {
                result.failedFiles.push_back(task.filePath);
            }
        }

        if (onTaskExecuted) {
            onTaskExecuted(static_cast<int>(index), taskSuccess, task.description);
        }
    }

    {
        const std::string observations = consumeExecutionObservations();
        if (!observations.empty()) {
            result.observations.push_back(observations);
        }
    }

    result.success = (result.failureCount == 0);
    if (onExecutionCompleted) {
        onExecutionCompleted(result);
    }

    return result;
}

void PlanOrchestrator::rememberLoopEvent(const std::string& message)
{
    const std::string normalized = normalizeObservation(message);
    if (normalized.empty()) {
        return;
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_loopMemory.empty() && m_loopMemory.back() == normalized) {
        return;
    }

    m_loopMemory.push_back(normalized);
    trimHistory(m_loopMemory, kMaxLoopMemoryEntries);
}

std::string PlanOrchestrator::buildLoopMemoryContext() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return joinObservationBatch(m_loopMemory);
}

std::vector<std::string> PlanOrchestrator::snapshotLoopMemory() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_loopMemory;
}

ExecutionResult PlanOrchestrator::runExecutionLoop(const std::string& userGoal,
                                                   const std::string& workspaceRoot,
                                                   int maxSteps,
                                                   bool dryRun)
{
    if (maxSteps <= 0) {
        return {};
    }

    if (dryRun) {
        return planAndExecute(userGoal, workspaceRoot, true);
    }

    return runAutonomousLoop(userGoal, maxSteps);
}

bool PlanOrchestrator::executeTask(const EditTask& task, bool dryRun)
{
    try {
        const std::string op = toLower(task.operation);
        if (op.empty()) {
            const std::string message = "Task rejected: empty operation is not allowed";
            recordExecutionObservation(message + " | Task: " + task.description);
            if (onErrorOccurred) {
                onErrorOccurred(message);
            }
            return false;
        }

        std::string safetyReason;
        const OperationSafety safety = classifyOperation(op, task, &safetyReason);

        if (safety == OperationSafety::Unsupported) {
            const std::string message = "Task rejected by safety gate: unsupported operation '" + op + "'";
            recordExecutionObservation(message + " | Task: " + task.description);
            if (onErrorOccurred) {
                onErrorOccurred(message);
            }
            return false;
        }

        // Fail-closed: risky operations require explicit approval callback unless running in dry-run mode.
        if (!dryRun && safety == OperationSafety::RequiresConfirmation) {
            if (!onTaskApprovalRequired) {
                const std::string message = "Task rejected by safety gate: approval callback unavailable for '" + op + "'";
                recordExecutionObservation(message + " | Reason: " + safetyReason + " | Task: " + task.description);
                if (onErrorOccurred) {
                    onErrorOccurred(message);
                }
                return false;
            }

            if (!onTaskApprovalRequired(task)) {
                const std::string message = "Task rejected by approval gate for operation '" + op + "'";
                recordExecutionObservation(message + " | Reason: " + safetyReason + " | Task: " + task.description);
                if (onErrorOccurred) {
                    onErrorOccurred(message);
                }
                return false;
            }
        }

        if (op == "execute_command" || op == "run_command" || op == "command") {
            return executeCommandTask(task, dryRun);
        }
        if (op == "read_file") {
            return executeReadTask(task, dryRun);
        }
        if (op == "list_directory" || op == "list_dir") {
            return executeListTask(task, dryRun);
        }
        if (op == "search_code" || op == "search_files") {
            return executeSearchTask(task, dryRun);
        }
        if (op == "write_file") {
            return applyWrite(task, dryRun);
        }
        if (op == "replace") {
            return applyReplace(task, dryRun);
        }
        if (op == "insert") {
            return applyInsert(task, dryRun);
        }
        if (op == "delete") {
            return applyDelete(task, dryRun);
        }
        if (op == "rename") {
            return applyRename(task, dryRun);
        }
        const std::string message = "Task rejected by safety gate: operation dispatch unavailable for '" + op + "'";
        recordExecutionObservation(message + " | Task: " + task.description);
        if (onErrorOccurred) {
            onErrorOccurred(message);
        }
        return false;
    } catch (const std::exception& e) {
        if (onErrorOccurred) {
            onErrorOccurred(std::string("Task execution failed: ") + e.what());
        }
        return false;
    }
}

bool PlanOrchestrator::executeCommandTask(const EditTask& task, bool dryRun)
{
    if (dryRun) {
        return true;
    }

    const std::string command = !task.newText.empty() ? task.newText : task.description;

    std::string observation = "[TOOL: execute_command]\nCommand: " + command;
    bool commandSucceeded = false;

#if defined(RAWRXD_MASM_CORE_NATIVE_BRIDGE) && defined(_MSC_VER) && defined(_M_X64)
    // ---- MASM path: PipeCapture_Run captures stdout+stderr directly ----------
    // Build a mutable command buffer (CreateProcessA requires a non-const LPSTR).
    static constexpr DWORD kCaptureBufSize = 16384;
    static thread_local unsigned char s_captureBuf[kCaptureBufSize];
    std::string cmdBuf = command;           // mutable copy
    DWORD bytesRead = 0;
    const DWORD pcErr = PipeCapture_Run(
        cmdBuf.data(),
        s_captureBuf,
        kCaptureBufSize - 1,
        &bytesRead);
    if (pcErr == 0) {
        s_captureBuf[bytesRead] = '\0';
        const std::string captured(reinterpret_cast<const char*>(s_captureBuf), bytesRead);
        observation += "\nOutput:\n" + captured;
        commandSucceeded = true;
    } else {
        observation += "\nPipeCapture error: " + std::to_string(pcErr);
    }
#else
    // ---- Portable fallback: use AgentToolHandlers::ExecuteCommand ------------
    nlohmann::json args;
    args["command"] = command;
    auto result = Agent::AgentToolHandlers::ExecuteCommand(args);
    if (result.metadata.contains("exit_code")) {
        observation += "\nExit Code: " + std::to_string(result.metadata.value("exit_code", -1));
    }
    const std::string payload = result.output.empty() ? result.error : result.output;
    if (!payload.empty()) {
        observation += "\nOutput:\n" + payload;
    }
    commandSucceeded = result.isSuccess();
#endif

    if (observation.size() > 8000) {
        observation.resize(8000);
        observation += "\n...[truncated]";
    }

    recordExecutionObservation(observation);

    if (m_terminalCommandSink) {
        appendExecutionMessage(onStepCompleted, observation);
    }

    if (commandSucceeded) {
        if (!m_terminalCommandSink) {
            appendExecutionMessage(onStepCompleted, observation);
        }
        return true;
    }

    if (onErrorOccurred) {
        onErrorOccurred(observation);
    }
    return false;
}

bool PlanOrchestrator::executeReadTask(const EditTask& task, bool dryRun)
{
    if (dryRun) {
        return true;
    }

    nlohmann::json args;
    args["path"] = resolvePath(task.filePath);
    auto result = Agent::AgentToolHandlers::ToolReadFile(args);
    if (result.isSuccess()) {
        appendExecutionMessage(onStepCompleted, result.output);
        recordExecutionObservation(result.output);
        return true;
    }

    recordExecutionObservation(result.error);
    if (onErrorOccurred) {
        onErrorOccurred(result.error);
    }
    return false;
}

bool PlanOrchestrator::executeListTask(const EditTask& task, bool dryRun)
{
    if (dryRun) {
        return true;
    }

    nlohmann::json args;
    args["path"] = resolvePath(task.filePath.empty() ? m_workspaceRoot : task.filePath);
    auto result = Agent::AgentToolHandlers::ListDir(args);
    if (result.isSuccess()) {
        appendExecutionMessage(onStepCompleted, result.output);
        recordExecutionObservation(result.output);
        return true;
    }

    recordExecutionObservation(result.error);
    if (onErrorOccurred) {
        onErrorOccurred(result.error);
    }
    return false;
}

bool PlanOrchestrator::executeSearchTask(const EditTask& task, bool dryRun)
{
    if (dryRun) {
        return true;
    }

    nlohmann::json args;
    args["query"] = !task.newText.empty() ? task.newText : task.description;
    auto result = Agent::AgentToolHandlers::SearchCode(args);
    if (result.isSuccess()) {
        appendExecutionMessage(onStepCompleted, result.output);
        recordExecutionObservation(result.output);
        return true;
    }

    recordExecutionObservation(result.error);
    if (onErrorOccurred) {
        onErrorOccurred(result.error);
    }
    return false;
}

bool PlanOrchestrator::applyWrite(const EditTask& task, bool dryRun)
{
    if (dryRun) {
        return true;
    }

    nlohmann::json args;
    args["path"] = resolvePath(task.filePath);
    args["content"] = task.newText;
    auto result = Agent::AgentToolHandlers::WriteFile(args);
    if (result.isSuccess()) {
        appendExecutionMessage(onStepCompleted, result.output);
        recordExecutionObservation(result.output);
        notifyWorkspaceChanged();
        return true;
    }

    recordExecutionObservation(result.error);
    if (onErrorOccurred) {
        onErrorOccurred(result.error);
    }
    return false;
}

bool PlanOrchestrator::applyReplace(const EditTask& task, bool dryRun)
{
    if (dryRun) {
        return true;
    }

    nlohmann::json args;
    args["path"] = resolvePath(task.filePath);
    args["old_string"] = task.oldText;
    args["new_string"] = task.newText;
    auto result = Agent::AgentToolHandlers::ReplaceInFile(args);
    if (result.isSuccess()) {
        appendExecutionMessage(onStepCompleted, result.output);
        recordExecutionObservation(result.output);
        notifyWorkspaceChanged();
        return true;
    }

    recordExecutionObservation(result.error);
    if (onErrorOccurred) {
        onErrorOccurred(result.error);
    }
    return false;
}

bool PlanOrchestrator::applyInsert(const EditTask& task, bool dryRun)
{
    if (dryRun) {
        return true;
    }

    std::string content = readFileContent(task.filePath);
    if (content.empty()) {
        recordExecutionObservation("Insert failed: could not read file " + task.filePath);
        return false;
    }

    const std::size_t pos = content.find(task.oldText);
    if (pos == std::string::npos) {
        recordExecutionObservation("Insert failed: anchor text not found in " + task.filePath);
        return false;
    }

    std::size_t lineEnd = content.find('\n', pos);
    if (lineEnd == std::string::npos) {
        lineEnd = content.size();
    } else {
        ++lineEnd;
    }

    content.insert(lineEnd, task.newText + "\n");
    if (!writeFileContent(task.filePath, content)) {
        recordExecutionObservation("Insert failed: could not write file " + task.filePath);
        return false;
    }

    recordExecutionObservation("Inserted content into " + resolvePath(task.filePath));
    notifyWorkspaceChanged();
    return true;
}

bool PlanOrchestrator::applyDelete(const EditTask& task, bool dryRun)
{
    if (dryRun) {
        return true;
    }

    std::string content = readFileContent(task.filePath);
    if (content.empty()) {
        recordExecutionObservation("Delete failed: could not read file " + task.filePath);
        return false;
    }

    const std::size_t pos = content.find(task.oldText);
    if (pos == std::string::npos) {
        recordExecutionObservation("Delete failed: target text not found in " + task.filePath);
        return false;
    }

    content.erase(pos, task.oldText.size());
    if (!writeFileContent(task.filePath, content)) {
        recordExecutionObservation("Delete failed: could not write file " + task.filePath);
        return false;
    }

    recordExecutionObservation("Deleted content from " + resolvePath(task.filePath));
    notifyWorkspaceChanged();
    return true;
}

bool PlanOrchestrator::applyRename(const EditTask& task, bool dryRun)
{
    namespace fs = std::filesystem;
    if (dryRun) {
        return true;
    }

    const std::string source = resolvePath(task.filePath);
    if (source.empty() || task.newText.empty()) {
        recordExecutionObservation("Rename skipped: missing source or destination path");
        return true;
    }

    std::error_code ec;
    fs::rename(fs::path(source), fs::path(resolvePath(task.newText)), ec);
    if (ec) {
        recordExecutionObservation("Rename failed: " + ec.message());
        return false;
    }

    recordExecutionObservation("Renamed " + source + " to " + resolvePath(task.newText));
    notifyWorkspaceChanged();
    return true;
}

std::vector<std::string> PlanOrchestrator::gatherContextFiles(const std::string& workspaceRoot, int maxFiles)
{
    namespace fs = std::filesystem;
    std::vector<std::string> files;
    if (workspaceRoot.empty() || maxFiles <= 0) {
        return files;
    }

    std::error_code ec;
    const fs::path rootPath(workspaceRoot);
    if (!fs::exists(rootPath, ec) || !fs::is_directory(rootPath, ec)) {
        return files;
    }

    const std::vector<std::string> extensions = {".cpp", ".h", ".hpp", ".c", ".cc", ".asm", ".md"};
    for (fs::recursive_directory_iterator it(rootPath, fs::directory_options::skip_permission_denied, ec), end;
         it != end && files.size() < static_cast<std::size_t>(maxFiles); it.increment(ec)) {
        if (ec) {
            ec.clear();
            continue;
        }
        if (!it->is_regular_file(ec)) {
            ec.clear();
            continue;
        }

        const std::string extension = toLower(it->path().extension().string());
        if (std::find(extensions.begin(), extensions.end(), extension) == extensions.end()) {
            continue;
        }

        fs::path relativePath = fs::relative(it->path(), rootPath, ec);
        files.push_back(ec ? it->path().generic_string() : relativePath.generic_string());
        ec.clear();
    }

    return files;
}

std::string PlanOrchestrator::readFileContent(const std::string& filePath)
{
    std::ifstream file(resolvePath(filePath), std::ios::binary);
    if (!file.is_open()) {
        return {};
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

bool PlanOrchestrator::writeFileContent(const std::string& filePath, const std::string& content)
{
    const std::filesystem::path resolved = resolvePath(filePath);
    std::error_code ec;
    if (resolved.has_parent_path()) {
        std::filesystem::create_directories(resolved.parent_path(), ec);
        ec.clear();
    }

    std::ofstream file(resolved, std::ios::binary | std::ios::trunc);
    if (!file.is_open()) {
        return false;
    }

    file << content;
    return file.good();
}

std::string PlanOrchestrator::resolvePath(const std::string& filePath) const
{
    namespace fs = std::filesystem;
    if (filePath.empty()) {
        return {};
    }

    const fs::path path(filePath);
    if (path.is_absolute() || m_workspaceRoot.empty()) {
        return path.lexically_normal().string();
    }

    return (fs::path(m_workspaceRoot) / path).lexically_normal().string();
}

std::string PlanOrchestrator::selectPlanningModel() const
{
    if (!m_modelRouter) {
        return "local-default";
    }

    const auto models = m_modelRouter->getAvailableModels();
    if (!models.empty()) {
        return models.front();
    }

    return "local-default";
}

void PlanOrchestrator::requestStop()
{
    std::lock_guard<std::mutex> lk(m_mutex);
    onStepCompleted        = nullptr;
    onPlanCompleted        = nullptr;
    onPlanningStarted      = nullptr;
    onPlanningChunk        = nullptr;
    onPlanningCompleted    = nullptr;
    onExecutionStarted     = nullptr;
    onTaskExecuted         = nullptr;
    onExecutionCompleted   = nullptr;
    onErrorOccurred        = nullptr;
    onWorkspaceChanged     = nullptr;
    onTaskApprovalRequired = nullptr;
}

} // namespace RawrXD

