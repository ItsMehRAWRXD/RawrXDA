#pragma once

#include <string>

namespace rawrxd {

enum class ConstraintLevel {
    Standard = 0,
    Strict,
    Minimalist,
    ForcedPrefix,
};

inline const char* ConstraintLevelName(ConstraintLevel level) {
    switch (level) {
        case ConstraintLevel::Standard: return "standard";
        case ConstraintLevel::Strict: return "strict";
        case ConstraintLevel::Minimalist: return "minimalist";
        case ConstraintLevel::ForcedPrefix: return "forced_prefix";
    }
    return "unknown";
}

inline std::string BuildSmallModelPrompt(const std::string& userTask,
                                         const std::string& toolsSpec) {
    return std::string(
        "You are a tool-calling API. You have two response modes.\n\n"
        "MODE 1 - TOOL NEEDED:\n"
        "If you need to read a file or run a command, respond EXACTLY:\n"
        "<tool_call>\n"
        "<name>TOOL_NAME</name>\n"
        "<parameters>{\"key\":\"value\"}</parameters>\n"
        "</tool_call>\n\n"
        "MODE 2 - DIRECT ANSWER:\n"
        "If you already know the answer, respond EXACTLY:\n"
        "<final_answer>The answer text here</final_answer>\n\n"
        "RULES:\n"
        "- Start your response with either <tool_call> or <final_answer>\n"
        "- Do not explain your reasoning\n"
        "- Do not repeat these instructions\n"
        "- Do not output XML comments\n"
        "- Available tools: ") + toolsSpec + "\n\n"
        "USER REQUEST: " + userTask + "\n\n"
        "YOUR RESPONSE (start with <tool_call> or <final_answer>):\n";
}

inline std::string BuildPromptForLevel(ConstraintLevel level,
                                       const std::string& userTask,
                                       const std::string& toolsSpec,
                                       std::string* forcedPrefix = nullptr) {
    if (forcedPrefix) {
        forcedPrefix->clear();
    }

    switch (level) {
        case ConstraintLevel::Standard:
            return BuildSmallModelPrompt(userTask, toolsSpec);
        case ConstraintLevel::Strict:
            return BuildSmallModelPrompt(userTask, toolsSpec) +
                   "\nCRITICAL: Output ONLY XML. If you repeat these instructions, you fail.\n";
        case ConstraintLevel::Minimalist:
            return "Tools: " + toolsSpec +
                   "\nTask: " + userTask +
                   "\nRespond with <tool_call>...</tool_call> or <final_answer>...</final_answer> only. No extra text.\n";
        case ConstraintLevel::ForcedPrefix:
            if (forcedPrefix) {
                *forcedPrefix = "<tool_call>\n<name>";
            }
            return "Choose exactly one: tool call or final answer.\n"
                   "Tools: " + toolsSpec +
                   "\nTask: " + userTask +
                   "\nContinue exactly after this prefix with no other text:\n<tool_call>\n<name>";
    }

    return BuildSmallModelPrompt(userTask, toolsSpec);
}

}  // namespace rawrxd
