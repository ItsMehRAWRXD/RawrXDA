#include "tool_registry.h"
#include <QFile>
#include <QTextStream>
#include <QProcess>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <memory>

// Note: This is a simplified implementation based on MASM tool_integration.asm
// For production use, integrate with the existing tool_registry.hpp/cpp

// Forward declarations for Logger and Metrics (in case they're used)
class Logger {
public:
    virtual ~Logger() = default;
    virtual void info(const std::string&, const std::string&) {}
};

class Metrics {
public:
    virtual ~Metrics() = default;
};

// Constructor with just QObject parent (default)
ToolRegistry::ToolRegistry(QObject* parent)
    : QObject(parent)
{
}

// Alternative constructor for compatibility with full tool_registry.cpp (takes Logger and Metrics)
// This is called when m_logger and m_metrics are provided
ToolRegistry::ToolRegistry(
    std::shared_ptr<Logger> logger,
    std::shared_ptr<Metrics> metrics
) : QObject(nullptr)
{
    // Accept logger and metrics parameters for API compatibility
    // The simplified version doesn't actually use them, but this allows
    // code expecting the full version to still compile and link
    (void)logger;    // Suppress unused parameter warning
    (void)metrics;   // Suppress unused parameter warning
}

ToolRegistry::~ToolRegistry() = default;

void ToolRegistry::registerTool(const ToolDef& toolDef)
{
    // From MASM ToolRegistry_Register - add tool to global registry
    if (!toolDef.name.isEmpty() && toolDef.function) {
        m_tools[toolDef.name] = toolDef;
    }
}

void ToolRegistry::registerTool(const QString& name, const QString& description,
                                ToolFunction function, const QJsonObject& schema)
{
    ToolDef def;
    def.name = name;
    def.description = description;
    def.function = function;
    def.parameterSchema = schema;
    registerTool(def);
}

QJsonObject ToolRegistry::executeTool(const QString& toolName, const QJsonObject& parameters)
{
    // From MASM ToolExecutor_Call
    if (!m_tools.contains(toolName)) {
        auto error = createErrorResult("Tool not found: " + toolName);
        emit toolError(toolName, "Tool not found");
        return error;
    }

    try {
        const ToolDef& toolDef = m_tools[toolName];
        QJsonObject result = toolDef.function(parameters);
        emit toolExecuted(toolName, result);
        return result;
    }
    catch (const std::exception& e) {
        auto error = createErrorResult(QString("Tool execution failed: %1").arg(e.what()));
        emit toolError(toolName, e.what());
        return error;
    }
}

QJsonObject ToolRegistry::executeToolCall(const QJsonObject& toolCall)
{
    // From MASM ToolExecutor_Call - parse {"name": "tool_name", "parameters": {...}}
    QString toolName = toolCall["name"].toString();
    QJsonObject parameters = toolCall["parameters"].toObject();

    if (toolName.isEmpty()) {
        return createErrorResult("Tool name is required");
    }

    return executeTool(toolName, parameters);
}

bool ToolRegistry::hasTool(const QString& name) const
{
    return m_tools.contains(name);
}

ToolRegistry::ToolDef ToolRegistry::getToolDef(const QString& name) const
{
    return m_tools.value(name);
}

QStringList ToolRegistry::getToolNames() const
{
    return m_tools.keys();
}

QJsonArray ToolRegistry::getToolsAsJsonArray() const
{
    QJsonArray tools;
    for (const auto& toolDef : m_tools) {
        QJsonObject tool;
        tool["name"] = toolDef.name;
        tool["description"] = toolDef.description;
        tool["parameters"] = toolDef.parameterSchema;
        tools.append(tool);
    }
    return tools;
}

// Built-in tool implementations (from MASM tool_integration.asm)

QJsonObject ToolRegistry::Tool_FileRead(const QJsonObject& params)
{
    // From MASM Tool_FileRead
    QString filePath = params["path"].toString();

    if (filePath.isEmpty()) {
        return QJsonObject{{"error", "File path is required"}};
    }

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return QJsonObject{{"error", QString("Failed to open file: %1").arg(file.errorString())}};
    }

    QTextStream in(&file);
    QString content = in.readAll();
    file.close();

    return QJsonObject{{"content", content}, {"path", filePath}};
}

QJsonObject ToolRegistry::Tool_FileWrite(const QJsonObject& params)
{
    // From MASM Tool_FileWrite
    QString filePath = params["path"].toString();
    QString content = params["content"].toString();

    if (filePath.isEmpty()) {
        return QJsonObject{{"error", "File path is required"}};
    }

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return QJsonObject{{"error", QString("Failed to open file: %1").arg(file.errorString())}};
    }

    QTextStream out(&file);
    out << content;
    file.close();

    return QJsonObject{{"success", true}, {"path", filePath}, {"bytes_written", content.toUtf8().size()}};
}

QJsonObject ToolRegistry::Tool_GrepSearch(const QJsonObject& params)
{
    // From MASM Tool_GrepSearch - ripgrep-style search
    QString pattern = params["pattern"].toString();
    QString path = params["path"].toString(".");

    if (pattern.isEmpty()) {
        return QJsonObject{{"error", "Search pattern is required"}};
    }

    // Use QProcess to run grep or ripgrep if available
    QProcess process;
    QStringList args;

    // Try to use ripgrep if available, otherwise fall back to grep
    QString command = "rg";
    args << "--json" << pattern << path;

    process.start(command, args);
    if (!process.waitForStarted()) {
        // Fall back to grep
        command = "grep";
        args.clear();
        args << "-rn" << pattern << path;
        process.start(command, args);
        if (!process.waitForStarted()) {
            return QJsonObject{{"error", "No search tool available (tried rg, grep)"}};
        }
    }

    process.waitForFinished();
    QString output = process.readAllStandardOutput();

    return QJsonObject{{"output", output}, {"pattern", pattern}, {"path", path}};
}

QJsonObject ToolRegistry::Tool_ExecuteCommand(const QJsonObject& params)
{
    // From MASM Tool_ExecuteCommand - shell commands with sandboxing
    QString command = params["command"].toString();
    QString workingDir = params["working_dir"].toString();

    if (command.isEmpty()) {
        return QJsonObject{{"error", "Command is required"}};
    }

    QProcess process;
    if (!workingDir.isEmpty()) {
        process.setWorkingDirectory(workingDir);
    }

    process.start(command);
    if (!process.waitForStarted()) {
        return QJsonObject{{"error", QString("Failed to start command: %1").arg(process.errorString())}};
    }

    process.waitForFinished();

    QString stdout_text = process.readAllStandardOutput();
    QString stderr_text = process.readAllStandardError();
    int exitCode = process.exitCode();

    return QJsonObject{
        {"stdout", stdout_text},
        {"stderr", stderr_text},
        {"exit_code", exitCode},
        {"command", command}
    };
}

QJsonObject ToolRegistry::Tool_GitStatus(const QJsonObject& params)
{
    // From MASM Tool_GitStatus - git status/diff
    QString path = params["path"].toString(".");

    QProcess process;
    process.setWorkingDirectory(path);
    process.start("git", QStringList{"status", "--porcelain"});

    if (!process.waitForStarted()) {
        return QJsonObject{{"error", "Git not available"}};
    }

    process.waitForFinished();
    QString output = process.readAllStandardOutput();

    // Also get diff
    QProcess diffProcess;
    diffProcess.setWorkingDirectory(path);
    diffProcess.start("git", QStringList{"diff"});
    diffProcess.waitForFinished();
    QString diff = diffProcess.readAllStandardOutput();

    return QJsonObject{
        {"status", output},
        {"diff", diff},
        {"path", path}
    };
}

QJsonObject ToolRegistry::Tool_CompileProject(const QJsonObject& params)
{
    // From MASM Tool_CompileProject
    QString buildCommand = params["command"].toString("cmake --build .");
    QString workingDir = params["working_dir"].toString();

    if (workingDir.isEmpty()) {
        return QJsonObject{{"error", "Working directory is required"}};
    }

    QProcess process;
    process.setWorkingDirectory(workingDir);
    process.start(buildCommand);

    if (!process.waitForStarted()) {
        return QJsonObject{{"error", "Failed to start build"}};
    }

    process.waitForFinished(300000); // 5 minute timeout

    QString stdout_text = process.readAllStandardOutput();
    QString stderr_text = process.readAllStandardError();
    int exitCode = process.exitCode();

    return QJsonObject{
        {"success", exitCode == 0},
        {"stdout", stdout_text},
        {"stderr", stderr_text},
        {"exit_code", exitCode}
    };
}

void ToolRegistry::registerBuiltInTools()
{
    // Register all built-in tools from MASM tool_integration.asm
    registerTool("file_read", "Read contents of a file",
                Tool_FileRead,
                QJsonObject{{"path", QJsonObject{{"type", "string"}, {"required", true}}}});

    registerTool("file_write", "Write content to a file",
                Tool_FileWrite,
                QJsonObject{
                    {"path", QJsonObject{{"type", "string"}, {"required", true}}},
                    {"content", QJsonObject{{"type", "string"}, {"required", true}}}
                });

    registerTool("grep_search", "Search for pattern in files (ripgrep-style)",
                Tool_GrepSearch,
                QJsonObject{
                    {"pattern", QJsonObject{{"type", "string"}, {"required", true}}},
                    {"path", QJsonObject{{"type", "string"}, {"default", "."}}}
                });

    registerTool("execute_command", "Execute shell command",
                Tool_ExecuteCommand,
                QJsonObject{
                    {"command", QJsonObject{{"type", "string"}, {"required", true}}},
                    {"working_dir", QJsonObject{{"type", "string"}}}
                });

    registerTool("git_status", "Get git status and diff",
                Tool_GitStatus,
                QJsonObject{
                    {"path", QJsonObject{{"type", "string"}, {"default", "."}}}
                });

    registerTool("compile_project", "Compile/build project",
                Tool_CompileProject,
                QJsonObject{
                    {"command", QJsonObject{{"type", "string"}, {"default", "cmake --build ."}}},
                    {"working_dir", QJsonObject{{"type", "string"}, {"required", true}}}
                });
}

// Private helper methods

QJsonObject ToolRegistry::createErrorResult(const QString& error) const
{
    // From MASM ERR_TOOL_NOT_FOUND
    return QJsonObject{{"error", error}};
}

QJsonObject ToolRegistry::createSuccessResult(const QJsonValue& result) const
{
    return QJsonObject{{"result", result}, {"success", true}};
}
