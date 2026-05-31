#pragma once

#include <string>
#include <vector>

namespace rawrxd {

enum class TaskIntent {
    FileIO,
    Terminal,
    Reasoning,
    Hybrid,
    Unknown,
};

enum class ExecutionLane {
    ModelFirst,
    SyntheticFirst,
    SyntheticOnly,
    ModelAssisted,
};

enum class TrustScore {
    ModelOk,
    ModelSuspect,
    ModelInvalid,
    DeterministicOnly,
};

struct ExecutionStep {
    std::string toolName;
    std::string argumentsJson;
    bool deterministic = false;
};

struct ExecutionPlan {
    TaskIntent intent = TaskIntent::Unknown;
    ExecutionLane lane = ExecutionLane::ModelFirst;
    TrustScore trust = TrustScore::ModelSuspect;
    std::string rationale;
    std::vector<ExecutionStep> steps;

    bool permitsModel() const {
        return lane == ExecutionLane::ModelFirst || lane == ExecutionLane::ModelAssisted;
    }

    bool prefersSynthetic() const {
        return lane == ExecutionLane::SyntheticFirst || lane == ExecutionLane::SyntheticOnly;
    }
};

inline const char* ToString(TaskIntent value) {
    switch (value) {
        case TaskIntent::FileIO: return "file_io";
        case TaskIntent::Terminal: return "terminal";
        case TaskIntent::Reasoning: return "reasoning";
        case TaskIntent::Hybrid: return "hybrid";
        case TaskIntent::Unknown: return "unknown";
    }
    return "unknown";
}

inline const char* ToString(ExecutionLane value) {
    switch (value) {
        case ExecutionLane::ModelFirst: return "model_first";
        case ExecutionLane::SyntheticFirst: return "synthetic_first";
        case ExecutionLane::SyntheticOnly: return "synthetic_only";
        case ExecutionLane::ModelAssisted: return "model_assisted";
    }
    return "model_first";
}

inline const char* ToString(TrustScore value) {
    switch (value) {
        case TrustScore::ModelOk: return "model_ok";
        case TrustScore::ModelSuspect: return "model_suspect";
        case TrustScore::ModelInvalid: return "model_invalid";
        case TrustScore::DeterministicOnly: return "deterministic_only";
    }
    return "model_suspect";
}

}  // namespace rawrxd
