#ifndef PROMPT_TEMPLATES_H
#define PROMPT_TEMPLATES_H

// C++20, no Qt. Prompt templates for AI debugger. JSON as std::string.

#include <string>

class PromptTemplates
{
public:
    /** Generate a prompt for the AI model based on debug information (JSON string). */
    static std::string generateDebugPrompt(const std::string& debugInfoJson);

private:
    static std::string formatLocals(const std::string& localsJson);
    static std::string formatStack(const std::string& stackJson);
    static std::string formatRegisters(const std::string& registersJson);
};

#endif // PROMPT_TEMPLATES_H
