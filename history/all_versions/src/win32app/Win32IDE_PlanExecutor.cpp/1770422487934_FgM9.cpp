// ============================================================================
// Win32IDE_PlanExecutor.cpp — Plan → Approve → Execute Agent Workflow
// ============================================================================
// Provides structured agent planning with user trust controls:
//   - Agent generates a structured plan (JSON steps)
//   - User sees plan in a dialog: steps, time estimates, confidence
//   - Approve / Reject / Modify workflow
//   - Step-by-step execution with progress, pause, skip, cancel
//   - Rollback on failure (restores file backups)
//   - Plan history for auditability
//
// This is the feature that builds agent trust — the user is always in control.
// ============================================================================

#include "Win32IDE.h"
#include "IDELogger.h"
#include <richedit.h>
#include <commctrl.h>
#include <algorithm>
#include <sstream>
#include <fstream>
#include <thread>
#include <chrono>

// ============================================================================
// PLAN STEP EXECUTION — runs a single step via the agent
// ============================================================================

static std::string executeSingleStep(AgenticBridge* bridge, const PlanStep& step) {
    if (!bridge || !bridge->IsInitialized()) return "[Error] Agent not initialized";

    std::string prompt;
    switch (step.type) {
        case PlanStepType::CodeEdit:
            prompt = "Execute exactly: " + step.description +
                     (step.targetFile.empty() ? "" : "\nFile: " + step.targetFile);
            break;
        case PlanStepType::FileCreate:
            prompt = "Create file: " + step.targetFile + "\n" + step.description;
            break;
        case PlanStepType::FileDelete:
            prompt = "Delete file: " + step.targetFile;
            break;
        case PlanStepType::ShellCommand:
            prompt = "Execute shell command: " + step.description;
            break;
        case PlanStepType::Analysis:
            prompt = "Analyze: " + step.description;
            break;
        case PlanStepType::Verification:
            prompt = "Verify: " + step.description;
            break;
        default:
            prompt = step.description;
            break;
    }

    AgentResponse response = bridge->ExecuteAgentCommand(prompt);
    return response.content;
}

// ============================================================================
// GENERATE PLAN — ask the agent to create a structured plan
// ============================================================================

void Win32IDE::generateAgentPlan(const std::string& goal) {
    if (!m_agenticBridge || !m_agenticBridge->IsInitialized()) {
        appendToOutput("[Plan] Agent not initialized. Configure a model first.",
                       "General", OutputSeverity::Warning);
        return;
    }

    appendToOutput("[Plan] Generating plan for: " + goal, "General", OutputSeverity::Info);

    // Reset current plan
    m_currentPlan.goal = goal;
    m_currentPlan.steps.clear();
    m_currentPlan.status = PlanStatus::Generating;
    m_currentPlan.currentStepIndex = -1;
    m_currentPlan.overallConfidence = 0.0f;

    // Background thread — ask agent for structured plan
    std::string goalCopy = goal;
    std::thread([this, goalCopy]() {
        std::string planPrompt =
            "Create a detailed step-by-step plan to accomplish the following goal.\n"
            "For each step, provide:\n"
            "  - A short title (one line)\n"
            "  - A description of what to do\n"
            "  - Estimated time (in minutes)\n"
            "  - Confidence level (0.0 to 1.0)\n"
            "  - Risk level (low/medium/high)\n"
            "  - Type: code_edit, file_create, file_delete, shell_command, analysis, or verification\n"
            "  - Target file (if applicable)\n\n"
            "Format each step as:\n"
            "STEP: <title>\n"
            "DESC: <description>\n"
            "TIME: <minutes>\n"
            "CONF: <0.0-1.0>\n"
            "RISK: <low|medium|high>\n"
            "TYPE: <type>\n"
            "FILE: <path or none>\n"
            "---\n\n"
            "Goal: " + goalCopy;

        AgentResponse response = m_agenticBridge->ExecuteAgentCommand(planPrompt);

        // Parse the plan from agent output
        std::vector<PlanStep> steps = parsePlanSteps(response.content);

        // Post to UI thread
        PlanStep* heapSteps = nullptr;
        int stepCount = (int)steps.size();
        if (stepCount > 0) {
            heapSteps = new PlanStep[stepCount];
            for (int i = 0; i < stepCount; i++) {
                heapSteps[i] = steps[i];
            }
        }
        PostMessageA(m_hwndMain, WM_PLAN_READY, (WPARAM)stepCount, (LPARAM)heapSteps);
    }).detach();
}

// ============================================================================
// PLAN READY — delivered to UI thread
// ============================================================================

void Win32IDE::onPlanReady(int stepCount, PlanStep* steps) {
    if (stepCount <= 0 || !steps) {
        m_currentPlan.status = PlanStatus::Failed;
        appendToOutput("[Plan] Failed to generate plan — no steps parsed.",
                       "General", OutputSeverity::Error);
        return;
    }

    m_currentPlan.steps.clear();
    float totalConf = 0.0f;
    int totalTime = 0;

    for (int i = 0; i < stepCount; i++) {
        steps[i].id = i + 1;
        steps[i].status = PlanStepStatus::Pending;
        m_currentPlan.steps.push_back(steps[i]);
        totalConf += steps[i].confidence;
        totalTime += steps[i].estimatedMinutes;
    }
    delete[] steps;

    m_currentPlan.overallConfidence = totalConf / stepCount;
    m_currentPlan.status = PlanStatus::AwaitingApproval;

    // Show plan approval dialog
    showPlanApprovalDialog();
}

// ============================================================================
// PLAN APPROVAL DIALOG — user reviews, approves, or rejects
// ============================================================================

void Win32IDE::showPlanApprovalDialog() {
    if (m_currentPlan.steps.empty()) return;

    // Build plan summary for display
    std::ostringstream oss;
    oss << "PLAN: " << m_currentPlan.goal << "\r\n";
    oss << "Steps: " << m_currentPlan.steps.size()
        << " | Confidence: " << (int)(m_currentPlan.overallConfidence * 100) << "%\r\n";
    oss << "========================================\r\n\r\n";

    int totalMinutes = 0;
    for (const auto& step : m_currentPlan.steps) {
        oss << "Step " << step.id << ": " << step.title << "\r\n";
        oss << "  Type: " << planStepTypeString(step.type) << "\r\n";
        oss << "  " << step.description << "\r\n";
        if (!step.targetFile.empty()) {
            oss << "  File: " << step.targetFile << "\r\n";
        }
        oss << "  Time: ~" << step.estimatedMinutes << " min"
            << " | Confidence: " << (int)(step.confidence * 100) << "%"
            << " | Risk: " << step.risk << "\r\n";
        oss << "----------------------------------------\r\n";
        totalMinutes += step.estimatedMinutes;
    }
    oss << "\r\nTotal estimated time: ~" << totalMinutes << " min\r\n";

    std::string planText = oss.str();

    // Display in a modal dialog with Approve / Reject / Modify buttons
    // Using a simple MessageBox for now — upgrade to custom dialog later
    int result = MessageBoxA(m_hwndMain, planText.c_str(),
                             "Agent Plan — Review & Approve",
                             MB_YESNOCANCEL | MB_ICONQUESTION);

    switch (result) {
        case IDYES:
            m_currentPlan.status = PlanStatus::Approved;
            appendToOutput("[Plan] Approved. Starting execution...",
                           "General", OutputSeverity::Info);
            executePlan();
            break;
        case IDNO:
            m_currentPlan.status = PlanStatus::Rejected;
            appendToOutput("[Plan] Rejected by user.",
                           "General", OutputSeverity::Warning);
            break;
        case IDCANCEL:
            m_currentPlan.status = PlanStatus::AwaitingApproval;
            appendToOutput("[Plan] Deferred — plan saved for later review.",
                           "General", OutputSeverity::Info);
            break;
    }
}

// ============================================================================
// EXECUTE PLAN — step-by-step with progress
// ============================================================================

void Win32IDE::executePlan() {
    if (m_currentPlan.status != PlanStatus::Approved) return;
    if (m_currentPlan.steps.empty()) return;

    m_currentPlan.status = PlanStatus::Executing;
    m_planExecutionCancelled.store(false);
    m_planExecutionPaused.store(false);

    // Show progress
    showModelProgressBar("Executing plan: " + m_currentPlan.goal);

    std::thread([this]() {
        int totalSteps = (int)m_currentPlan.steps.size();

        for (int i = 0; i < totalSteps; i++) {
            // Check for cancellation
            if (m_planExecutionCancelled.load()) {
                m_currentPlan.steps[i].status = PlanStepStatus::Skipped;
                for (int j = i + 1; j < totalSteps; j++) {
                    m_currentPlan.steps[j].status = PlanStepStatus::Skipped;
                }
                PostMessageA(m_hwndMain, WM_PLAN_STEP_DONE, i, (LPARAM)2); // 2 = cancelled
                break;
            }

            // Check for pause
            while (m_planExecutionPaused.load() && !m_planExecutionCancelled.load()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }

            m_currentPlan.currentStepIndex = i;
            m_currentPlan.steps[i].status = PlanStepStatus::Running;

            // Update progress on UI thread
            float percent = ((float)i / totalSteps) * 100.0f;
            updateModelProgress(percent,
                "Step " + std::to_string(i + 1) + "/" + std::to_string(totalSteps) +
                ": " + m_currentPlan.steps[i].title);

            // Create file backup before code edits (for rollback)
            std::string backupContent;
            if (m_currentPlan.steps[i].type == PlanStepType::CodeEdit &&
                !m_currentPlan.steps[i].targetFile.empty()) {
                std::ifstream f(m_currentPlan.steps[i].targetFile);
                if (f) {
                    std::ostringstream buf;
                    buf << f.rdbuf();
                    backupContent = buf.str();
                }
            }

            // Execute the step
            std::string result = executeSingleStep(m_agenticBridge.get(), m_currentPlan.steps[i]);

            if (result.find("[Error]") != std::string::npos ||
                result.find("error") != std::string::npos) {
                m_currentPlan.steps[i].status = PlanStepStatus::Failed;
                m_currentPlan.steps[i].output = result;

                // Rollback file if we backed it up
                if (!backupContent.empty() && !m_currentPlan.steps[i].targetFile.empty()) {
                    std::ofstream restore(m_currentPlan.steps[i].targetFile);
                    if (restore) {
                        restore << backupContent;
                        restore.close();
                    }
                }

                PostMessageA(m_hwndMain, WM_PLAN_STEP_DONE, i, (LPARAM)0); // 0 = failed
                // Don't continue on failure — ask user
                break;
            } else {
                m_currentPlan.steps[i].status = PlanStepStatus::Completed;
                m_currentPlan.steps[i].output = result;
                PostMessageA(m_hwndMain, WM_PLAN_STEP_DONE, i, (LPARAM)1); // 1 = success
            }
        }

        // Check if all steps completed
        bool allDone = true;
        for (const auto& step : m_currentPlan.steps) {
            if (step.status != PlanStepStatus::Completed) {
                allDone = false;
                break;
            }
        }

        PostMessageA(m_hwndMain, WM_PLAN_COMPLETE, allDone ? 1 : 0, 0);
    }).detach();
}

// ============================================================================
// PLAN STEP DONE — UI thread handler
// ============================================================================

void Win32IDE::onPlanStepDone(int stepIndex, int result) {
    if (stepIndex < 0 || stepIndex >= (int)m_currentPlan.steps.size()) return;

    const auto& step = m_currentPlan.steps[stepIndex];

    if (result == 1) {
        appendToOutput("[Plan] ✓ Step " + std::to_string(stepIndex + 1) + ": " + step.title,
                       "General", OutputSeverity::Info);
    } else if (result == 0) {
        appendToOutput("[Plan] ✗ Step " + std::to_string(stepIndex + 1) + " failed: " + step.title,
                       "General", OutputSeverity::Error);
        appendToOutput("[Plan] Output: " + step.output, "General", OutputSeverity::Error);

        // Ask user whether to continue or abort
        int choice = MessageBoxA(m_hwndMain,
            ("Step " + std::to_string(stepIndex + 1) + " failed:\n" + step.title +
             "\n\nContinue with remaining steps?").c_str(),
            "Plan Step Failed", MB_YESNO | MB_ICONWARNING);

        if (choice == IDYES) {
            // Mark failed step and continue
            std::thread([this, stepIndex]() {
                // Resume from next step
                int totalSteps = (int)m_currentPlan.steps.size();
                for (int i = stepIndex + 1; i < totalSteps; i++) {
                    if (m_planExecutionCancelled.load()) break;
                    m_currentPlan.currentStepIndex = i;
                    m_currentPlan.steps[i].status = PlanStepStatus::Running;

                    float percent = ((float)i / totalSteps) * 100.0f;
                    updateModelProgress(percent,
                        "Step " + std::to_string(i + 1) + "/" + std::to_string(totalSteps) +
                        ": " + m_currentPlan.steps[i].title);

                    std::string result = executeSingleStep(m_agenticBridge.get(), m_currentPlan.steps[i]);
                    m_currentPlan.steps[i].status = PlanStepStatus::Completed;
                    m_currentPlan.steps[i].output = result;
                    PostMessageA(m_hwndMain, WM_PLAN_STEP_DONE, i, (LPARAM)1);
                }
                PostMessageA(m_hwndMain, WM_PLAN_COMPLETE, 1, 0);
            }).detach();
        } else {
            cancelPlan();
        }
    } else {
        appendToOutput("[Plan] Cancelled at step " + std::to_string(stepIndex + 1),
                       "General", OutputSeverity::Warning);
    }
}

// ============================================================================
// PLAN COMPLETE — UI thread handler
// ============================================================================

void Win32IDE::onPlanComplete(bool success) {
    hideModelProgressBar();

    if (success) {
        m_currentPlan.status = PlanStatus::Completed;
        appendToOutput("[Plan] ✓ All steps completed successfully!",
                       "General", OutputSeverity::Info);
    } else {
        m_currentPlan.status = PlanStatus::Failed;
        appendToOutput("[Plan] Plan completed with failures.",
                       "General", OutputSeverity::Warning);
    }

    // Store in plan history
    m_planHistory.push_back(m_currentPlan);
    if (m_planHistory.size() > MAX_PLAN_HISTORY) {
        m_planHistory.erase(m_planHistory.begin());
    }
}

// ============================================================================
// CANCEL / PAUSE / RESUME
// ============================================================================

void Win32IDE::cancelPlan() {
    m_planExecutionCancelled.store(true);
    m_currentPlan.status = PlanStatus::Failed;
    hideModelProgressBar();
    appendToOutput("[Plan] Execution cancelled by user.", "General", OutputSeverity::Warning);
}

void Win32IDE::pausePlan() {
    m_planExecutionPaused.store(true);
    appendToOutput("[Plan] Execution paused.", "General", OutputSeverity::Info);
}

void Win32IDE::resumePlan() {
    m_planExecutionPaused.store(false);
    appendToOutput("[Plan] Execution resumed.", "General", OutputSeverity::Info);
}

// ============================================================================
// PLAN STATUS STRING
// ============================================================================

std::string Win32IDE::getPlanStatusString() const {
    std::ostringstream oss;
    oss << "Plan: " << m_currentPlan.goal << "\r\n";
    oss << "Status: ";
    switch (m_currentPlan.status) {
        case PlanStatus::None:              oss << "No plan"; break;
        case PlanStatus::Generating:        oss << "Generating..."; break;
        case PlanStatus::AwaitingApproval:  oss << "Awaiting approval"; break;
        case PlanStatus::Approved:          oss << "Approved"; break;
        case PlanStatus::Rejected:          oss << "Rejected"; break;
        case PlanStatus::Executing:         oss << "Executing..."; break;
        case PlanStatus::Completed:         oss << "Completed"; break;
        case PlanStatus::Failed:            oss << "Failed"; break;
    }
    oss << "\r\nSteps: " << m_currentPlan.steps.size()
        << " | Confidence: " << (int)(m_currentPlan.overallConfidence * 100) << "%\r\n";

    for (const auto& step : m_currentPlan.steps) {
        oss << "  [" << step.id << "] ";
        switch (step.status) {
            case PlanStepStatus::Pending:   oss << "⬜ "; break;
            case PlanStepStatus::Running:   oss << "🔄 "; break;
            case PlanStepStatus::Completed: oss << "✅ "; break;
            case PlanStepStatus::Failed:    oss << "❌ "; break;
            case PlanStepStatus::Skipped:   oss << "⏭️ "; break;
        }
        oss << step.title << "\r\n";
    }

    return oss.str();
}

// ============================================================================
// PARSE PLAN STEPS — from agent output
// ============================================================================

std::vector<PlanStep> Win32IDE::parsePlanSteps(const std::string& agentOutput) {
    std::vector<PlanStep> steps;
    PlanStep current;
    bool inStep = false;
    int stepId = 0;

    std::istringstream stream(agentOutput);
    std::string line;

    while (std::getline(stream, line)) {
        // Trim whitespace
        size_t start = line.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) continue;
        line = line.substr(start);

        if (line.substr(0, 5) == "STEP:" || line.substr(0, 5) == "Step:") {
            if (inStep && !current.title.empty()) {
                current.id = ++stepId;
                steps.push_back(current);
            }
            current = PlanStep();
            current.title = line.substr(5);
            // Trim title
            size_t ts = current.title.find_first_not_of(" \t");
            if (ts != std::string::npos) current.title = current.title.substr(ts);
            inStep = true;
        } else if (inStep && (line.substr(0, 5) == "DESC:" || line.substr(0, 5) == "Desc:")) {
            current.description = line.substr(5);
            size_t ts = current.description.find_first_not_of(" \t");
            if (ts != std::string::npos) current.description = current.description.substr(ts);
        } else if (inStep && (line.substr(0, 5) == "TIME:" || line.substr(0, 5) == "Time:")) {
            std::string val = line.substr(5);
            try { current.estimatedMinutes = std::stoi(val); } catch (...) { current.estimatedMinutes = 1; }
        } else if (inStep && (line.substr(0, 5) == "CONF:" || line.substr(0, 5) == "Conf:")) {
            std::string val = line.substr(5);
            try { current.confidence = std::stof(val); } catch (...) { current.confidence = 0.5f; }
        } else if (inStep && (line.substr(0, 5) == "RISK:" || line.substr(0, 5) == "Risk:")) {
            current.risk = line.substr(5);
            size_t ts = current.risk.find_first_not_of(" \t");
            if (ts != std::string::npos) current.risk = current.risk.substr(ts);
        } else if (inStep && (line.substr(0, 5) == "TYPE:" || line.substr(0, 5) == "Type:")) {
            std::string val = line.substr(5);
            size_t ts = val.find_first_not_of(" \t");
            if (ts != std::string::npos) val = val.substr(ts);
            if (val.find("code_edit") != std::string::npos) current.type = PlanStepType::CodeEdit;
            else if (val.find("file_create") != std::string::npos) current.type = PlanStepType::FileCreate;
            else if (val.find("file_delete") != std::string::npos) current.type = PlanStepType::FileDelete;
            else if (val.find("shell") != std::string::npos) current.type = PlanStepType::ShellCommand;
            else if (val.find("analysis") != std::string::npos) current.type = PlanStepType::Analysis;
            else if (val.find("verif") != std::string::npos) current.type = PlanStepType::Verification;
        } else if (inStep && (line.substr(0, 5) == "FILE:" || line.substr(0, 5) == "File:")) {
            current.targetFile = line.substr(5);
            size_t ts = current.targetFile.find_first_not_of(" \t");
            if (ts != std::string::npos) current.targetFile = current.targetFile.substr(ts);
            if (current.targetFile == "none" || current.targetFile == "N/A") current.targetFile.clear();
        } else if (line.find("---") == 0) {
            if (inStep && !current.title.empty()) {
                current.id = ++stepId;
                steps.push_back(current);
                current = PlanStep();
                inStep = false;
            }
        }
    }

    // Push last step if not terminated by ---
    if (inStep && !current.title.empty()) {
        current.id = ++stepId;
        steps.push_back(current);
    }

    LOG_INFO("Parsed " + std::to_string(steps.size()) + " plan steps from agent output");
    return steps;
}

// ============================================================================
// PLAN STEP TYPE STRING
// ============================================================================

std::string Win32IDE::planStepTypeString(PlanStepType type) const {
    switch (type) {
        case PlanStepType::CodeEdit:     return "Code Edit";
        case PlanStepType::FileCreate:   return "File Create";
        case PlanStepType::FileDelete:   return "File Delete";
        case PlanStepType::ShellCommand: return "Shell Command";
        case PlanStepType::Analysis:     return "Analysis";
        case PlanStepType::Verification: return "Verification";
        default:                         return "General";
    }
}
