#ifndef AGENTIC_TOOLS_HPP_INCLUDED
#define AGENTIC_TOOLS_HPP_INCLUDED

#include <QString>
#include <QStringList>
#include <QObject>
#include <QProcess>

// Tool result structure returned by all tool methods
struct ToolResult {
    bool success;
    QString output;
    QString error;
    int exitCode;
    double executionTimeMs;
};

// Agent tool executor with 8 built-in tools
class AgenticToolExecutor : public QObject {
    Q_OBJECT

public:
    AgenticToolExecutor(QObject* parent = nullptr);
    ~AgenticToolExecutor();

    // File operations
    ToolResult readFile(const QString& filePath);
    ToolResult writeFile(const QString& filePath, const QString& content);
    ToolResult listDirectory(const QString& dirPath);

    // Command execution
    ToolResult executeCommand(const QString& program, const QStringList& args);

    // Search and analysis
    ToolResult grepSearch(const QString& pattern, const QString& searchPath);
    ToolResult gitStatus(const QString& repoPath);
    ToolResult runTests(const QString& testPath);
    ToolResult analyzeCode(const QString& filePath);

signals:
    void toolExecutionCompleted(QString toolName, QString result);
    void toolExecutionError(QString toolName, QString error);
    void toolExecuted(QString toolName, ToolResult result);
    void toolFailed(QString toolName, QString error);
    void toolProgress(QString toolName, QString progress);

private:
    QString detectLanguage(const QString& filePath);
};

#endif // AGENTIC_TOOLS_HPP_INCLUDED
