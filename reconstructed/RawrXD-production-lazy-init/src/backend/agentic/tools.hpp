/**
 * @file agentic_tools.hpp
 * @brief Complete tool definitions for agentic execution
 */

#pragma once

#include <QObject>
#include <QDir>
#include <QFile>
#include <QHash>
#include <QJsonObject>
#include <QProcess>
#include <QString>
#include <QStringList>
#include <functional>

/**
 * @struct ToolResult
 * @brief Result of tool execution
 */
struct ToolResult {
    bool success = false;
    QString output;
    QString error;
    int exitCode = 0;
    double executionTimeMs = 0.0;
};

/**
 * @class AgenticToolExecutor
 * @brief Complete tool executor with file, git, build, and analysis capabilities
 */
class AgenticToolExecutor : public QObject {
    Q_OBJECT

public:
    explicit AgenticToolExecutor(QObject* parent = nullptr);

    ToolResult executeTool(const QString& toolName, const QStringList& arguments);

    void registerTool(const QString& name, std::function<ToolResult(const QStringList&)> executor);

    // Built-in tools
    ToolResult readFile(const QString& filePath);
    ToolResult writeFile(const QString& filePath, const QString& content);
    ToolResult listDirectory(const QString& dirPath);
    ToolResult executeCommand(const QString& command, const QStringList& args);
    ToolResult grepSearch(const QString& pattern, const QString& path);
    ToolResult gitStatus(const QString& repoPath);
    ToolResult runTests(const QString& testPath);
    ToolResult analyzeCode(const QString& filePath);

signals:
    void toolExecuted(const QString& name, const ToolResult& result);
    void toolFailed(const QString& name, const QString& error);
    void toolProgress(const QString& name, const QString& progress);
    void toolExecutionCompleted(const QString& name, const QString& result);
    void toolExecutionError(const QString& name, const QString& error);

private:
    QHash<QString, std::function<ToolResult(const QStringList&)>> m_tools;
    bool m_blockDeletes = true; // When true, destructive commands/file deletes are blocked

    bool isDestructiveCommand(const QString& program, const QStringList& args) const;
    void initializeBuiltInTools();
    ToolResult executeProcess(const QString& program, const QStringList& args, int timeoutMs = 30000);
    QString detectLanguage(const QString& filePath);
};
