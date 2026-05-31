#pragma once

#include "execution_plan_ir.h"
#include "parrot_detector.h"
#include "response_validator.h"

#include <algorithm>
#include <chrono>
#include <functional>
#include <sstream>
#include <string>
#include <utility>

namespace rawrxd {

class PromotedAgentController {
  public:
    using ToolBackend = PlanExecutor::ToolBackend;
    using LLMBackend = PlanExecutor::LLMBackend;

    PromotedAgentController(ToolBackend toolBackend, LLMBackend llmBackend)
        : toolBackend_(std::move(toolBackend)), llmBackend_(std::move(llmBackend)) {}

    bool ExecuteUserIntent(const std::string& userIntent, std::string* finalOutput = nullptr) {
        auto [plan, trustEvent] = GeneratePlanWithFallback(userIntent);
        telemetry_.Record(std::move(trustEvent));

        if (!plan.Validate()) {
            return false;
        }

        std::string output;
        const bool success = PlanExecutor::Execute(plan, toolBackend_,
                                                   [&](const std::string& prompt) { return SafeLLMCall(prompt); },
                                                   telemetry_, &output);
        if (finalOutput) {
            *finalOutput = output;
        }
        return success;
    }

    const TrustTelemetry& telemetry() const {
        return telemetry_;
    }

  private:
    using PlanResult = std::pair<ExecutionPlanIR, TrustEvent>;

    PlanResult GeneratePlanWithFallback(const std::string& userIntent) {
        const std::string planPrompt =
            "Generate execution plan for: " + userIntent + "\nAvailable tools: file_read, terminal_execute";

        auto [llmResponse, trustEvent] = llmBackend_(planPrompt);
        if (trustEvent.classification == TrustClassification::Compliant) {
            ExecutionPlanIR plan = ParsePlanFromLLM(llmResponse, userIntent);
            if (plan.Validate()) {
                return {std::move(plan), std::move(trustEvent)};
            }
            trustEvent.classification = TrustClassification::StructureInvalid;
            trustEvent.resolutionAction = "fallback_heuristic_plan_due_to_invalid_llm_plan";
        }

        ExecutionPlanIR fallbackPlan = GenerateHeuristicPlan(userIntent);
        TrustEvent fallbackEvent;
        fallbackEvent.classification = TrustClassification::DeterministicFallback;
        fallbackEvent.retryLevelAttempted = -1;
        fallbackEvent.resolutionAction = "heuristic_plan_generation";
        fallbackEvent.rawPreview = userIntent.substr(0, userIntent.size() < 100 ? userIntent.size() : 100);
        fallbackEvent.inputTokens = planPrompt.size() / 4;
        fallbackEvent.outputTokens = 0;
        return {std::move(fallbackPlan), std::move(fallbackEvent)};
    }

    ExecutionPlanIR GenerateHeuristicPlan(const std::string& userIntent) const {
        ExecutionPlanIR plan;
        plan.planId = "heuristic_" + std::to_string(std::hash<std::string>{}(userIntent));

        std::string lower = userIntent;
        std::transform(lower.begin(), lower.end(), lower.begin(),
                       [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });

        if (lower.find("read") != std::string::npos && lower.find("file") != std::string::npos) {
            const std::string path = ExtractLastPath(userIntent);
            plan.steps.push_back(StepFileRead{path, "file_content"});
            plan.steps.push_back(StepLLMSynthesize{"Summarize this file content: {{file_content}}", "result", true, 256});
        } else if (lower.find("run") != std::string::npos || lower.find("execute") != std::string::npos ||
                   lower.find("terminal") != std::string::npos) {
            plan.steps.push_back(StepTerminalExecute{"cmd /c dir", "terminal_result", 5000, true});
            plan.steps.push_back(StepLLMSynthesize{"Summarize command output: {{terminal_result}}", "result", true, 256});
        } else {
            plan.steps.push_back(StepLLMSynthesize{"Answer this user question: " + userIntent, "result", true, 512});
        }

        return plan;
    }

    std::pair<std::string, TrustEvent> SafeLLMCall(const std::string& prompt) const {
        const auto start = std::chrono::steady_clock::now();
        auto [response, event] = llmBackend_(prompt);
        event.timestamp = std::chrono::steady_clock::now();
        event.inferenceLatencyMs = std::chrono::duration<double, std::milli>(event.timestamp - start).count();
        event.rawPreview = response.substr(0, response.size() < 100 ? response.size() : 100);
        event.inputTokens = prompt.size() / 4;
        event.outputTokens = response.size() / 4;

        if (ParrotDetector::IsParroting(response, prompt)) {
            event.classification = TrustClassification::ParrotPrefixEcho;
        } else if (ResponseValidator::ContainsPromptLeakage(response)) {
            event.classification = TrustClassification::PromptLeakage;
        } else if (ParrotDetector::IsDegenerate(response) || response.empty()) {
            event.classification = TrustClassification::DegenerateOutput;
        } else {
            event.classification = TrustClassification::Compliant;
        }

        return {response, std::move(event)};
    }

    static std::string ExtractLastPath(const std::string& text) {
        const size_t lastQuote = text.rfind('"');
        if (lastQuote != std::string::npos && lastQuote > 0) {
            const size_t prevQuote = text.rfind('"', lastQuote - 1);
            if (prevQuote != std::string::npos) {
                return text.substr(prevQuote + 1, lastQuote - prevQuote - 1);
            }
        }

        const size_t lastSeparator = text.find_last_of(" /");
        if (lastSeparator != std::string::npos && lastSeparator + 1 < text.size()) {
            return text.substr(lastSeparator + 1);
        }
        return "default.txt";
    }

    static ExecutionPlanIR ParsePlanFromLLM(const std::string& response, const std::string& userIntent) {
        ExecutionPlanIR plan;
        plan.planId = "llm_" + std::to_string(std::hash<std::string>{}(response + userIntent));

        std::string lower = response;
        std::transform(lower.begin(), lower.end(), lower.begin(),
                       [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });

        if (lower.find("file_read") != std::string::npos) {
            plan.steps.push_back(StepFileRead{ExtractLastPath(userIntent), "file_content"});
            plan.steps.push_back(StepLLMSynthesize{"Summarize this file content: {{file_content}}", "result", true, 256});
        } else if (lower.find("terminal_execute") != std::string::npos || lower.find("command") != std::string::npos) {
            plan.steps.push_back(StepTerminalExecute{"cmd /c dir", "terminal_result", 5000, true});
            plan.steps.push_back(StepLLMSynthesize{"Summarize command output: {{terminal_result}}", "result", true, 256});
        } else {
            plan.steps.push_back(StepLLMSynthesize{"Answer this user question: " + userIntent, "result", true, 512});
        }

        return plan;
    }

    ToolBackend toolBackend_;
    LLMBackend llmBackend_;
    TrustTelemetry telemetry_;
};

}  // namespace rawrxd
