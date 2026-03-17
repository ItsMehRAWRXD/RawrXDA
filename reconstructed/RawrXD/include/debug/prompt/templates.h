#ifndef PROMPT_TEMPLATES_H
#define PROMPT_TEMPLATES_H

#include <QString>
#include <QJsonObject>
#include <QJsonDocument>

// Prompt templates for AI debugger
class PromptTemplates
{
public:
    // Generate a prompt for the AI model based on debug information
    static QString generateDebugPrompt(const QJsonObject &debugInfo);

private:
    // Helper functions to format different parts of the debug info
    static QString formatLocals(const QJsonArray &locals);
    static QString formatStack(const QJsonArray &stack);
    static QString formatRegisters(const QJsonArray ®isters);
};

#endif // PROMPT_TEMPLATES_H