#ifndef ORCHESTRA_COMMAND_HANDLER_HPP
#define ORCHESTRA_COMMAND_HANDLER_HPP

#include <QString>
#include <QVariantMap>
#include <functional>
#include <memory>

namespace RawrXD {

/**
 * @brief Result of a command execution
 */
struct CommandResult {
    bool success = false;
    QString output;
    QString error;
    int exitCode = 0;
    QVariantMap data;  // Structured data for JSON output
};

/**
 * @brief Callback for asynchronous command completion
 */
using CommandCallback = std::function<void(const CommandResult&)>;

/**
 * @brief Central command handler for both CLI and GUI
 * 
 * This class provides unified command handling that both the CLI
 * and GUI use. This ensures feature parity between both interfaces.
 */
class OrchestraCommandHandler {
public:
    static OrchestraCommandHandler& instance();

    // Project commands
    void projectOpen(const QString& path, CommandCallback callback);
    void projectClose(CommandCallback callback);
    void projectCreate(const QString& path, const QString& templateName, CommandCallback callback);
    void projectList(CommandCallback callback);

    // Build commands
    void buildProject(const QString& target, const QString& config, CommandCallback callback);
    void buildClean(CommandCallback callback);
    void buildRebuild(const QString& target, const QString& config, CommandCallback callback);
    void buildConfigure(const QString& cmakeArgs, CommandCallback callback);

    // Git commands
    void gitStatus(CommandCallback callback);
    void gitAdd(const QStringList& files, CommandCallback callback);
    void gitCommit(const QString& message, CommandCallback callback);
    void gitPush(const QString& remote, const QString& branch, CommandCallback callback);
    void gitPull(const QString& remote, const QString& branch, CommandCallback callback);
    void gitBranch(const QString& branchName, CommandCallback callback);
    void gitCheckout(const QString& branchOrCommit, CommandCallback callback);
    void gitLog(int count, CommandCallback callback);

    // File commands
    void fileRead(const QString& path, CommandCallback callback);
    void fileWrite(const QString& path, const QString& content, CommandCallback callback);
    void fileFind(const QString& pattern, const QString& searchPath, CommandCallback callback);
    void fileSearch(const QString& query, const QString& pattern, const QString& searchPath, CommandCallback callback);
    void fileReplace(const QString& search, const QString& replace, const QString& pattern, bool dryRun, CommandCallback callback);

    // AI commands
    void aiLoadModel(const QString& modelPath, CommandCallback callback);
    void aiUnloadModel(CommandCallback callback);
    void aiInference(const QString& prompt, CommandCallback callback);
    void aiComplete(const QString& context, CommandCallback callback);
    void aiExplain(const QString& code, CommandCallback callback);
    void aiRefactor(const QString& code, const QString& instruction, CommandCallback callback);
    void aiOptimize(const QString& code, CommandCallback callback);
    void aiTest(const QString& code, CommandCallback callback);

    // Test commands
    void testDiscover(CommandCallback callback);
    void testRun(const QStringList& testIds, CommandCallback callback);
    void testCoverage(CommandCallback callback);
    void testDebug(const QString& testId, CommandCallback callback);

    // Diagnostic commands
    void diagRun(CommandCallback callback);
    void diagInfo(CommandCallback callback);
    void diagStatus(CommandCallback callback);
    void diagSystemInfo(CommandCallback callback);
    void diagModelInfo(CommandCallback callback);

    // Agent commands
    void agentPlan(const QString& task, CommandCallback callback);
    void agentExecute(const QString& plan, CommandCallback callback);
    void agentRefactor(const QString& code, CommandCallback callback);
    void agentAnalyze(const QString& code, CommandCallback callback);

    // Execution commands
    void execCommand(const QString& command, const QStringList& args, CommandCallback callback);
    void shellCommand(const QString& command, CommandCallback callback);

private:
    OrchestraCommandHandler() = default;
    ~OrchestraCommandHandler() = default;

    // Prevent copying
    OrchestraCommandHandler(const OrchestraCommandHandler&) = delete;
    OrchestraCommandHandler& operator=(const OrchestraCommandHandler&) = delete;

    // Helper methods
    QString validatePath(const QString& path);
    QString validateGitRepository();
    bool isProjectOpen();
    void emitProgress(const QString& message);
    void emitError(const QString& error);
};

}  // namespace RawrXD

#endif // ORCHESTRA_COMMAND_HANDLER_HPP
