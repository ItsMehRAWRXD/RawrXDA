#pragma once

#include "trust_event.h"

#include <functional>
#include <map>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace rawrxd {

struct StepFileRead {
    std::string path;
    std::string alias;
};

struct StepTerminalExecute {
    std::string command;
    std::string alias;
    int timeoutMs = 5000;
    bool captureOutput = true;
};

struct StepLLMSynthesize {
    std::string promptTemplate;
    std::string alias;
    bool optional = true;
    int maxTokens = 256;
};

struct StepCondition {
    std::string lhs;
    std::string op;
    std::string rhs;
    int trueBranchIndex = 0;
    int falseBranchIndex = 0;
};

using PlanStep = std::variant<StepFileRead, StepTerminalExecute, StepLLMSynthesize, StepCondition>;

class ExecutionPlanIR {
  public:
    std::vector<PlanStep> steps;
    std::string planId;

    bool Validate() const {
        for (const auto& step : steps) {
            bool valid = std::visit([](const auto& concrete) -> bool {
                using T = std::decay_t<decltype(concrete)>;
                if constexpr (std::is_same_v<T, StepFileRead>) {
                    return !concrete.path.empty() && !concrete.alias.empty();
                } else if constexpr (std::is_same_v<T, StepTerminalExecute>) {
                    return !concrete.command.empty() && !concrete.alias.empty();
                } else if constexpr (std::is_same_v<T, StepLLMSynthesize>) {
                    return !concrete.promptTemplate.empty() && !concrete.alias.empty();
                } else if constexpr (std::is_same_v<T, StepCondition>) {
                    return concrete.trueBranchIndex >= 0 && concrete.falseBranchIndex >= 0;
                }
                return false;
            }, step);
            if (!valid) {
                return false;
            }
        }
        return true;
    }
};

class PlanExecutor {
  public:
    struct ExecutionContext {
        std::map<std::string, std::string> variables;
        int programCounter = 0;
        bool halted = false;
        std::string finalOutput;
        TrustTelemetry* telemetry = nullptr;
    };

    using ToolBackend = std::function<std::string(const std::string& toolName,
                                                  const std::map<std::string, std::string>& params)>;
    using LLMBackend = std::function<std::pair<std::string, TrustEvent>(const std::string& prompt)>;

    static bool Execute(const ExecutionPlanIR& plan,
                        const ToolBackend& toolBackend,
                        const LLMBackend& llmBackend,
                        TrustTelemetry& telemetry,
                        std::string* finalOutput = nullptr) {
        ExecutionContext context{{}, 0, false, "", &telemetry};

        while (!context.halted && context.programCounter >= 0 &&
               context.programCounter < static_cast<int>(plan.steps.size())) {
            const auto& step = plan.steps[static_cast<size_t>(context.programCounter)];

            std::visit([&](const auto& concrete) {
                using T = std::decay_t<decltype(concrete)>;

                if constexpr (std::is_same_v<T, StepFileRead>) {
                    const std::map<std::string, std::string> params{{"path", concrete.path}};
                    context.variables[concrete.alias] = toolBackend("file_read", params);
                    context.programCounter++;
                } else if constexpr (std::is_same_v<T, StepTerminalExecute>) {
                    const std::map<std::string, std::string> params{{"command", concrete.command},
                                                                    {"timeout", std::to_string(concrete.timeoutMs)}};
                    context.variables[concrete.alias] = toolBackend("terminal_execute", params);
                    context.programCounter++;
                } else if constexpr (std::is_same_v<T, StepLLMSynthesize>) {
                    const std::string resolvedPrompt = ResolveTemplate(concrete.promptTemplate, context.variables);
                    auto [response, trustEvent] = llmBackend(resolvedPrompt);
                    if (context.telemetry) {
                        context.telemetry->Record(std::move(trustEvent));
                    }

                    const TrustClassification classification = context.telemetry && !context.telemetry->events().empty()
                        ? context.telemetry->events().back().classification
                        : TrustClassification::StructureInvalid;

                    if (classification == TrustClassification::Compliant || concrete.optional) {
                        context.variables[concrete.alias] = response;
                        context.programCounter++;
                    } else {
                        context.halted = true;
                        context.finalOutput = "PLAN_FAILED_LLM_NON_COMPLIANT";
                    }
                } else if constexpr (std::is_same_v<T, StepCondition>) {
                    const std::string lhsValue = ResolveReference(concrete.lhs, context.variables);
                    const std::string rhsValue = ResolveReference(concrete.rhs, context.variables);
                    bool conditionMet = false;
                    if (concrete.op == "eq") {
                        conditionMet = lhsValue == rhsValue;
                    } else if (concrete.op == "neq") {
                        conditionMet = lhsValue != rhsValue;
                    } else if (concrete.op == "contains") {
                        conditionMet = lhsValue.find(rhsValue) != std::string::npos;
                    } else if (concrete.op == "exists") {
                        conditionMet = !lhsValue.empty();
                    }
                    context.programCounter = conditionMet ? concrete.trueBranchIndex : concrete.falseBranchIndex;
                }
            }, step);
        }

        if (!context.halted && context.finalOutput.empty()) {
            if (context.variables.count("result") != 0U) {
                context.finalOutput = context.variables["result"];
            } else if (!context.variables.empty()) {
                context.finalOutput = context.variables.rbegin()->second;
            } else {
                context.finalOutput = "OK";
            }
        }

        if (finalOutput) {
            *finalOutput = context.finalOutput;
        }
        return !context.halted;
    }

  private:
    static std::string ResolveReference(const std::string& reference,
                                        const std::map<std::string, std::string>& variables) {
        const auto it = variables.find(reference);
        if (it != variables.end()) {
            return it->second;
        }
        return reference;
    }

    static std::string ResolveTemplate(std::string promptTemplate,
                                       const std::map<std::string, std::string>& variables) {
        for (const auto& [key, value] : variables) {
            const std::string placeholder = "{{" + key + "}}";
            size_t pos = 0;
            while ((pos = promptTemplate.find(placeholder, pos)) != std::string::npos) {
                promptTemplate.replace(pos, placeholder.size(), value);
                pos += value.size();
            }
        }
        return promptTemplate;
    }
};

}  // namespace rawrxd
