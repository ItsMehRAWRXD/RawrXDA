#pragma once

#include <QObject>
#include <QString>
#include <QJsonObject>
#include <QJsonDocument>
#include <QMap>
#include <functional>
#include <memory>

// Forward declarations
class Logger;
class Metrics;

/**
 * @brief ToolRegistry - Tool calling framework with JSON-based interface
 * 
 * Ported from tool_integration.asm MASM implementation.
 * Implements tool registration and execution with JSON parameters.
 */
class ToolRegistry : public QObject {
    Q_OBJECT

public:
    // Tool function signature: takes JSON parameters, returns JSON result
    using ToolFunction = std::function<QJsonObject(const QJsonObject&)>;

    // Tool definition structure (from MASM ToolDef)
    struct ToolDef {
        QString name;
        QString description;
        ToolFunction function;
        QJsonObject parameterSchema;
    };

    explicit ToolRegistry(QObject* parent = nullptr);
    // Alternative constructor for compatibility with full tool_registry.cpp
    ToolRegistry(std::shared_ptr<Logger> logger, std::shared_ptr<Metrics> metrics);
    ~ToolRegistry();

    // Tool registration (from MASM ToolRegistry_Register)
    void registerTool(const ToolDef& toolDef);
    void registerTool(const QString& name, const QString& description,
                     ToolFunction function, const QJsonObject& schema = QJsonObject());

    // Tool execution (from MASM ToolExecutor_Call)
    QJsonObject executeTool(const QString& toolName, const QJsonObject& parameters);
    QJsonObject executeToolCall(const QJsonObject& toolCall); // Format: {"name": "...", "parameters": {...}}

    // Tool lookup
    bool hasTool(const QString& name) const;
    ToolDef getToolDef(const QString& name) const;
    QStringList getToolNames() const;
    QJsonArray getToolsAsJsonArray() const;

    // Built-in tools (from MASM tool_integration.asm)
    static QJsonObject Tool_FileRead(const QJsonObject& params);
    static QJsonObject Tool_FileWrite(const QJsonObject& params);
    static QJsonObject Tool_GrepSearch(const QJsonObject& params);
    static QJsonObject Tool_ExecuteCommand(const QJsonObject& params);
    static QJsonObject Tool_GitStatus(const QJsonObject& params);
    static QJsonObject Tool_CompileProject(const QJsonObject& params);

    // Register all built-in tools
    void registerBuiltInTools();

signals:
    void toolExecuted(const QString& toolName, const QJsonObject& result);
    void toolError(const QString& toolName, const QString& error);

private:
    QMap<QString, ToolDef> m_tools;

    // Helper methods
    QJsonObject createErrorResult(const QString& error) const;
    QJsonObject createSuccessResult(const QJsonValue& result) const;
};
