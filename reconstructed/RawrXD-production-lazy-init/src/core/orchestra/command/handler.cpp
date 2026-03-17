#include "orchestra_command_handler.hpp"
#include "orchestra_manager.hpp"
#include <QProcess>
#include <QDir>
#include <QFileInfo>
#include <QDebug>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>

namespace RawrXD {

OrchestraCommandHandler& OrchestraCommandHandler::instance() {
    static OrchestraCommandHandler s_instance;
    return s_instance;
}

// ============================================================
// Project Commands
// ============================================================

void OrchestraCommandHandler::projectOpen(const QString& path, CommandCallback callback) {
    QString validPath = validatePath(path);
    if (validPath.isEmpty()) {
        CommandResult result;
        result.success = false;
        result.error = "Invalid project path: " + path;
        if (callback) callback(result);
        return;
    }

    OrchestraManager::instance().openProject(validPath, [callback](const TaskResult& r) {
        CommandResult result;
        result.success = r.success;
        result.output = r.output;
        result.error = r.error;
        result.exitCode = r.success ? 0 : 1;
        if (callback) callback(result);
    });
}

void OrchestraCommandHandler::projectClose(CommandCallback callback) {
    OrchestraManager::instance().closeProject();
    CommandResult result;
    result.success = true;
    result.output = "Project closed";
    if (callback) callback(result);
}

void OrchestraCommandHandler::projectCreate(const QString& path, const QString& templateName, CommandCallback callback) {
    QString validPath = validatePath(path);
    if (validPath.isEmpty()) {
        CommandResult result;
        result.success = false;
        result.error = "Invalid project path: " + path;
        if (callback) callback(result);
        return;
    }

    OrchestraManager::instance().createProject(validPath, templateName, QVariantMap(),
        [callback](const TaskResult& r) {
            CommandResult result;
            result.success = r.success;
            result.output = r.output;
            result.error = r.error;
            result.exitCode = r.success ? 0 : 1;
            if (callback) callback(result);
        });
}

void OrchestraCommandHandler::projectList(CommandCallback callback) {
    // Get list of recent projects
    QVariantMap data;
    QJsonArray projectsArray;

    QStringList recentProjects = OrchestraManager::instance().getRecentProjects();
    for (const QString& proj : recentProjects) {
        QJsonObject projObj;
        projObj["path"] = proj;
        projObj["name"] = QFileInfo(proj).baseName();
        projectsArray.append(projObj);
    }

    data["projects"] = projectsArray;

    CommandResult result;
    result.success = true;
    result.output = QString::number(recentProjects.size()) + " recent projects";
    result.data = data;
    if (callback) callback(result);
}

// ============================================================
// Build Commands
// ============================================================

void OrchestraCommandHandler::buildProject(const QString& target, const QString& config, CommandCallback callback) {
    if (!isProjectOpen()) {
        CommandResult result;
        result.success = false;
        result.error = "No project open";
        if (callback) callback(result);
        return;
    }

    QString actualTarget = target.isEmpty() ? "all" : target;
    QString actualConfig = config.isEmpty() ? "Release" : config;

    OrchestraManager::instance().buildProject(actualTarget, actualConfig,
        [callback](const TaskResult& r) {
            CommandResult result;
            result.success = r.success;
            result.output = r.output;
            result.error = r.error;
            result.exitCode = r.success ? 0 : 1;
            if (callback) callback(result);
        });
}

void OrchestraCommandHandler::buildClean(CommandCallback callback) {
    if (!isProjectOpen()) {
        CommandResult result;
        result.success = false;
        result.error = "No project open";
        if (callback) callback(result);
        return;
    }

    OrchestraManager::instance().cleanBuild([callback](const TaskResult& r) {
        CommandResult result;
        result.success = r.success;
        result.output = r.output;
        result.error = r.error;
        result.exitCode = r.success ? 0 : 1;
        if (callback) callback(result);
    });
}

void OrchestraCommandHandler::buildRebuild(const QString& target, const QString& config, CommandCallback callback) {
    if (!isProjectOpen()) {
        CommandResult result;
        result.success = false;
        result.error = "No project open";
        if (callback) callback(result);
        return;
    }

    buildClean([this, target, config, callback](const CommandResult& cleanResult) {
        if (!cleanResult.success) {
            if (callback) callback(cleanResult);
            return;
        }
        buildProject(target, config, callback);
    });
}

void OrchestraCommandHandler::buildConfigure(const QString& cmakeArgs, CommandCallback callback) {
    if (!isProjectOpen()) {
        CommandResult result;
        result.success = false;
        result.error = "No project open";
        if (callback) callback(result);
        return;
    }

    OrchestraManager::instance().configureBuild(cmakeArgs, [callback](const TaskResult& r) {
        CommandResult result;
        result.success = r.success;
        result.output = r.output;
        result.error = r.error;
        result.exitCode = r.success ? 0 : 1;
        if (callback) callback(result);
    });
}

// ============================================================
// Git Commands
// ============================================================

void OrchestraCommandHandler::gitStatus(CommandCallback callback) {
    QString gitDir = validateGitRepository();
    if (gitDir.isEmpty()) {
        CommandResult result;
        result.success = false;
        result.error = "Not a git repository";
        if (callback) callback(result);
        return;
    }

    QProcess process;
    process.setWorkingDirectory(gitDir);
    process.start("git", QStringList() << "status" << "--porcelain");

    if (!process.waitForFinished(5000)) {
        CommandResult result;
        result.success = false;
        result.error = "Git command timed out";
        if (callback) callback(result);
        return;
    }

    CommandResult result;
    result.success = process.exitCode() == 0;
    result.output = QString::fromUtf8(process.readAllStandardOutput());
    result.error = QString::fromUtf8(process.readAllStandardError());
    result.exitCode = process.exitCode();
    if (callback) callback(result);
}

void OrchestraCommandHandler::gitAdd(const QStringList& files, CommandCallback callback) {
    QString gitDir = validateGitRepository();
    if (gitDir.isEmpty()) {
        CommandResult result;
        result.success = false;
        result.error = "Not a git repository";
        if (callback) callback(result);
        return;
    }

    QStringList args = QStringList() << "add";
    if (files.isEmpty()) {
        args << ".";
    } else {
        args << files;
    }

    QProcess process;
    process.setWorkingDirectory(gitDir);
    process.start("git", args);

    if (!process.waitForFinished(5000)) {
        CommandResult result;
        result.success = false;
        result.error = "Git command timed out";
        if (callback) callback(result);
        return;
    }

    CommandResult result;
    result.success = process.exitCode() == 0;
    result.output = QString::fromUtf8(process.readAllStandardOutput());
    result.error = QString::fromUtf8(process.readAllStandardError());
    result.exitCode = process.exitCode();
    if (callback) callback(result);
}

void OrchestraCommandHandler::gitCommit(const QString& message, CommandCallback callback) {
    QString gitDir = validateGitRepository();
    if (gitDir.isEmpty()) {
        CommandResult result;
        result.success = false;
        result.error = "Not a git repository";
        if (callback) callback(result);
        return;
    }

    QProcess process;
    process.setWorkingDirectory(gitDir);
    process.start("git", QStringList() << "commit" << "-m" << message);

    if (!process.waitForFinished(5000)) {
        CommandResult result;
        result.success = false;
        result.error = "Git command timed out";
        if (callback) callback(result);
        return;
    }

    CommandResult result;
    result.success = process.exitCode() == 0;
    result.output = QString::fromUtf8(process.readAllStandardOutput());
    result.error = QString::fromUtf8(process.readAllStandardError());
    result.exitCode = process.exitCode();
    if (callback) callback(result);
}

void OrchestraCommandHandler::gitPush(const QString& remote, const QString& branch, CommandCallback callback) {
    QString gitDir = validateGitRepository();
    if (gitDir.isEmpty()) {
        CommandResult result;
        result.success = false;
        result.error = "Not a git repository";
        if (callback) callback(result);
        return;
    }

    QString actualRemote = remote.isEmpty() ? "origin" : remote;
    QString actualBranch = branch.isEmpty() ? "HEAD" : branch;

    QProcess process;
    process.setWorkingDirectory(gitDir);
    process.start("git", QStringList() << "push" << actualRemote << actualBranch);

    if (!process.waitForFinished(10000)) {
        CommandResult result;
        result.success = false;
        result.error = "Git command timed out";
        if (callback) callback(result);
        return;
    }

    CommandResult result;
    result.success = process.exitCode() == 0;
    result.output = QString::fromUtf8(process.readAllStandardOutput());
    result.error = QString::fromUtf8(process.readAllStandardError());
    result.exitCode = process.exitCode();
    if (callback) callback(result);
}

void OrchestraCommandHandler::gitPull(const QString& remote, const QString& branch, CommandCallback callback) {
    QString gitDir = validateGitRepository();
    if (gitDir.isEmpty()) {
        CommandResult result;
        result.success = false;
        result.error = "Not a git repository";
        if (callback) callback(result);
        return;
    }

    QString actualRemote = remote.isEmpty() ? "origin" : remote;
    QString actualBranch = branch.isEmpty() ? "HEAD" : branch;

    QProcess process;
    process.setWorkingDirectory(gitDir);
    process.start("git", QStringList() << "pull" << actualRemote << actualBranch);

    if (!process.waitForFinished(10000)) {
        CommandResult result;
        result.success = false;
        result.error = "Git command timed out";
        if (callback) callback(result);
        return;
    }

    CommandResult result;
    result.success = process.exitCode() == 0;
    result.output = QString::fromUtf8(process.readAllStandardOutput());
    result.error = QString::fromUtf8(process.readAllStandardError());
    result.exitCode = process.exitCode();
    if (callback) callback(result);
}

void OrchestraCommandHandler::gitBranch(const QString& branchName, CommandCallback callback) {
    QString gitDir = validateGitRepository();
    if (gitDir.isEmpty()) {
        CommandResult result;
        result.success = false;
        result.error = "Not a git repository";
        if (callback) callback(result);
        return;
    }

    QStringList args = QStringList() << "branch";
    if (!branchName.isEmpty()) {
        args << branchName;
    }

    QProcess process;
    process.setWorkingDirectory(gitDir);
    process.start("git", args);

    if (!process.waitForFinished(5000)) {
        CommandResult result;
        result.success = false;
        result.error = "Git command timed out";
        if (callback) callback(result);
        return;
    }

    CommandResult result;
    result.success = process.exitCode() == 0;
    result.output = QString::fromUtf8(process.readAllStandardOutput());
    result.error = QString::fromUtf8(process.readAllStandardError());
    result.exitCode = process.exitCode();
    if (callback) callback(result);
}

void OrchestraCommandHandler::gitCheckout(const QString& branchOrCommit, CommandCallback callback) {
    QString gitDir = validateGitRepository();
    if (gitDir.isEmpty()) {
        CommandResult result;
        result.success = false;
        result.error = "Not a git repository";
        if (callback) callback(result);
        return;
    }

    QProcess process;
    process.setWorkingDirectory(gitDir);
    process.start("git", QStringList() << "checkout" << branchOrCommit);

    if (!process.waitForFinished(5000)) {
        CommandResult result;
        result.success = false;
        result.error = "Git command timed out";
        if (callback) callback(result);
        return;
    }

    CommandResult result;
    result.success = process.exitCode() == 0;
    result.output = QString::fromUtf8(process.readAllStandardOutput());
    result.error = QString::fromUtf8(process.readAllStandardError());
    result.exitCode = process.exitCode();
    if (callback) callback(result);
}

void OrchestraCommandHandler::gitLog(int count, CommandCallback callback) {
    QString gitDir = validateGitRepository();
    if (gitDir.isEmpty()) {
        CommandResult result;
        result.success = false;
        result.error = "Not a git repository";
        if (callback) callback(result);
        return;
    }

    QString countStr = (count > 0) ? QString::number(count) : "10";
    QProcess process;
    process.setWorkingDirectory(gitDir);
    process.start("git", QStringList() << "log" << "-n" << countStr << "--oneline");

    if (!process.waitForFinished(5000)) {
        CommandResult result;
        result.success = false;
        result.error = "Git command timed out";
        if (callback) callback(result);
        return;
    }

    CommandResult result;
    result.success = process.exitCode() == 0;
    result.output = QString::fromUtf8(process.readAllStandardOutput());
    result.error = QString::fromUtf8(process.readAllStandardError());
    result.exitCode = process.exitCode();
    if (callback) callback(result);
}

// ============================================================
// File Commands
// ============================================================

void OrchestraCommandHandler::fileRead(const QString& path, CommandCallback callback) {
    QString validPath = validatePath(path);
    if (validPath.isEmpty()) {
        CommandResult result;
        result.success = false;
        result.error = "Invalid file path: " + path;
        if (callback) callback(result);
        return;
    }

    QFile file(validPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        CommandResult result;
        result.success = false;
        result.error = "Cannot open file: " + validPath;
        if (callback) callback(result);
        return;
    }

    CommandResult result;
    result.success = true;
    result.output = QString::fromUtf8(file.readAll());
    file.close();
    if (callback) callback(result);
}

void OrchestraCommandHandler::fileWrite(const QString& path, const QString& content, CommandCallback callback) {
    QString validPath = validatePath(path);
    if (validPath.isEmpty()) {
        CommandResult result;
        result.success = false;
        result.error = "Invalid file path: " + path;
        if (callback) callback(result);
        return;
    }

    QFile file(validPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        CommandResult result;
        result.success = false;
        result.error = "Cannot write to file: " + validPath;
        if (callback) callback(result);
        return;
    }

    file.write(content.toUtf8());
    file.close();

    CommandResult result;
    result.success = true;
    result.output = "File written: " + validPath;
    if (callback) callback(result);
}

void OrchestraCommandHandler::fileFind(const QString& pattern, const QString& searchPath, CommandCallback callback) {
    QString basePath = searchPath.isEmpty() ? 
        (isProjectOpen() ? OrchestraManager::instance().currentProjectPath() : QDir::currentPath()) : 
        searchPath;

    QDir dir(basePath);
    if (!dir.exists()) {
        CommandResult result;
        result.success = false;
        result.error = "Search path does not exist: " + basePath;
        if (callback) callback(result);
        return;
    }

    QStringList filters;
    if (!pattern.isEmpty()) {
        filters << pattern;
    } else {
        filters << "*";
    }

    QStringList files = dir.entryList(filters, QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);

    CommandResult result;
    result.success = true;
    result.output = QString::number(files.size()) + " results found\n";
    for (const QString& file : files) {
        result.output += basePath + "/" + file + "\n";
    }

    QJsonArray filesArray;
    for (const QString& file : files) {
        filesArray.append(basePath + "/" + file);
    }
    result.data["files"] = filesArray;

    if (callback) callback(result);
}

void OrchestraCommandHandler::fileSearch(const QString& query, const QString& pattern, const QString& searchPath, CommandCallback callback) {
    QString basePath = searchPath.isEmpty() ? 
        (isProjectOpen() ? OrchestraManager::instance().currentProjectPath() : QDir::currentPath()) : 
        searchPath;

    QDir dir(basePath);
    if (!dir.exists()) {
        CommandResult result;
        result.success = false;
        result.error = "Search path does not exist: " + basePath;
        if (callback) callback(result);
        return;
    }

    QStringList filters = pattern.isEmpty() ? QStringList("*") : QStringList(pattern);
    QStringList foundFiles;
    int lineCount = 0;

    QDirIterator iterator(basePath, filters, QDir::Files, QDirIterator::Subdirectories);
    while (iterator.hasNext()) {
        iterator.next();
        QFile file(iterator.filePath());
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream stream(&file);
            int lineNum = 0;
            while (!stream.atEnd()) {
                QString line = stream.readLine();
                if (line.contains(query, Qt::CaseInsensitive)) {
                    lineCount++;
                    foundFiles << iterator.filePath() + ":" + QString::number(lineNum);
                }
                lineNum++;
            }
            file.close();
        }
    }

    CommandResult result;
    result.success = true;
    result.output = QString::number(lineCount) + " matches found\n";
    for (const QString& file : foundFiles) {
        result.output += file + "\n";
    }

    QJsonArray matchesArray;
    for (const QString& file : foundFiles) {
        matchesArray.append(file);
    }
    result.data["matches"] = matchesArray;

    if (callback) callback(result);
}

void OrchestraCommandHandler::fileReplace(const QString& search, const QString& replace, const QString& pattern, bool dryRun, CommandCallback callback) {
    // For now, implement a simple version
    // Full implementation would support regex and multiple file patterns
    
    CommandResult result;
    result.success = false;
    result.error = "fileReplace not yet fully implemented";
    if (callback) callback(result);
}

// ============================================================
// AI Commands
// ============================================================

void OrchestraCommandHandler::aiLoadModel(const QString& modelPath, CommandCallback callback) {
    OrchestraManager::instance().loadAIModel(modelPath, [callback](const TaskResult& r) {
        CommandResult result;
        result.success = r.success;
        result.output = r.output;
        result.error = r.error;
        result.exitCode = r.success ? 0 : 1;
        if (callback) callback(result);
    });
}

void OrchestraCommandHandler::aiUnloadModel(CommandCallback callback) {
    OrchestraManager::instance().unloadAIModel();
    CommandResult result;
    result.success = true;
    result.output = "Model unloaded";
    if (callback) callback(result);
}

void OrchestraCommandHandler::aiInference(const QString& prompt, CommandCallback callback) {
    OrchestraManager::instance().generateAIResponse(prompt, [callback](const TaskResult& r) {
        CommandResult result;
        result.success = r.success;
        result.output = r.output;
        result.error = r.error;
        result.exitCode = r.success ? 0 : 1;
        if (callback) callback(result);
    });
}

void OrchestraCommandHandler::aiComplete(const QString& context, CommandCallback callback) {
    OrchestraManager::instance().completeCode(context, [callback](const TaskResult& r) {
        CommandResult result;
        result.success = r.success;
        result.output = r.output;
        result.error = r.error;
        result.exitCode = r.success ? 0 : 1;
        if (callback) callback(result);
    });
}

void OrchestraCommandHandler::aiExplain(const QString& code, CommandCallback callback) {
    QString prompt = "Explain the following code:\n\n" + code;
    aiInference(prompt, callback);
}

void OrchestraCommandHandler::aiRefactor(const QString& code, const QString& instruction, CommandCallback callback) {
    QString prompt = "Refactor the following code according to: " + instruction + "\n\n" + code;
    aiInference(prompt, callback);
}

void OrchestraCommandHandler::aiOptimize(const QString& code, CommandCallback callback) {
    QString prompt = "Optimize the following code for performance:\n\n" + code;
    aiInference(prompt, callback);
}

void OrchestraCommandHandler::aiTest(const QString& code, CommandCallback callback) {
    QString prompt = "Write comprehensive unit tests for the following code:\n\n" + code;
    aiInference(prompt, callback);
}

// ============================================================
// Test Commands
// ============================================================

void OrchestraCommandHandler::testDiscover(CommandCallback callback) {
    if (!isProjectOpen()) {
        CommandResult result;
        result.success = false;
        result.error = "No project open";
        if (callback) callback(result);
        return;
    }

    OrchestraManager::instance().discoverTests([callback](const TaskResult& r) {
        CommandResult result;
        result.success = r.success;
        result.output = r.output;
        result.error = r.error;
        result.exitCode = r.success ? 0 : 1;
        if (callback) callback(result);
    });
}

void OrchestraCommandHandler::testRun(const QStringList& testIds, CommandCallback callback) {
    if (!isProjectOpen()) {
        CommandResult result;
        result.success = false;
        result.error = "No project open";
        if (callback) callback(result);
        return;
    }

    OrchestraManager::instance().runTests(testIds, [callback](const TaskResult& r) {
        CommandResult result;
        result.success = r.success;
        result.output = r.output;
        result.error = r.error;
        result.exitCode = r.success ? 0 : 1;
        if (callback) callback(result);
    });
}

void OrchestraCommandHandler::testCoverage(CommandCallback callback) {
    if (!isProjectOpen()) {
        CommandResult result;
        result.success = false;
        result.error = "No project open";
        if (callback) callback(result);
        return;
    }

    CommandResult result;
    result.success = false;
    result.error = "testCoverage not yet implemented";
    if (callback) callback(result);
}

void OrchestraCommandHandler::testDebug(const QString& testId, CommandCallback callback) {
    if (!isProjectOpen()) {
        CommandResult result;
        result.success = false;
        result.error = "No project open";
        if (callback) callback(result);
        return;
    }

    CommandResult result;
    result.success = false;
    result.error = "testDebug not yet implemented";
    if (callback) callback(result);
}

// ============================================================
// Diagnostic Commands
// ============================================================

void OrchestraCommandHandler::diagRun(CommandCallback callback) {
    OrchestraManager::instance().runDiagnostics([callback](const TaskResult& r) {
        CommandResult result;
        result.success = r.success;
        result.output = r.output;
        result.error = r.error;
        result.exitCode = r.success ? 0 : 1;
        if (callback) callback(result);
    });
}

void OrchestraCommandHandler::diagInfo(CommandCallback callback) {
    CommandResult result;
    result.success = true;
    result.output = OrchestraManager::instance().getDiagnosticInfo();
    if (callback) callback(result);
}

void OrchestraCommandHandler::diagStatus(CommandCallback callback) {
    CommandResult result;
    result.success = true;
    result.output = OrchestraManager::instance().getSystemStatus();
    if (callback) callback(result);
}

void OrchestraCommandHandler::diagSystemInfo(CommandCallback callback) {
    CommandResult result;
    result.success = true;
    result.output = OrchestraManager::instance().getSystemInfo();
    if (callback) callback(result);
}

void OrchestraCommandHandler::diagModelInfo(CommandCallback callback) {
    CommandResult result;
    result.success = true;
    result.output = OrchestraManager::instance().getModelInfo();
    if (callback) callback(result);
}

// ============================================================
// Agent Commands
// ============================================================

void OrchestraCommandHandler::agentPlan(const QString& task, CommandCallback callback) {
    OrchestraManager::instance().agentCreatePlan(task, [callback](const TaskResult& r) {
        CommandResult result;
        result.success = r.success;
        result.output = r.output;
        result.error = r.error;
        result.exitCode = r.success ? 0 : 1;
        if (callback) callback(result);
    });
}

void OrchestraCommandHandler::agentExecute(const QString& plan, CommandCallback callback) {
    OrchestraManager::instance().agentExecutePlan(plan, [callback](const TaskResult& r) {
        CommandResult result;
        result.success = r.success;
        result.output = r.output;
        result.error = r.error;
        result.exitCode = r.success ? 0 : 1;
        if (callback) callback(result);
    });
}

void OrchestraCommandHandler::agentRefactor(const QString& code, CommandCallback callback) {
    OrchestraManager::instance().agentRefactorCode(code, [callback](const TaskResult& r) {
        CommandResult result;
        result.success = r.success;
        result.output = r.output;
        result.error = r.error;
        result.exitCode = r.success ? 0 : 1;
        if (callback) callback(result);
    });
}

void OrchestraCommandHandler::agentAnalyze(const QString& code, CommandCallback callback) {
    OrchestraManager::instance().agentAnalyzeCode(code, [callback](const TaskResult& r) {
        CommandResult result;
        result.success = r.success;
        result.output = r.output;
        result.error = r.error;
        result.exitCode = r.success ? 0 : 1;
        if (callback) callback(result);
    });
}

// ============================================================
// Execution Commands
// ============================================================

void OrchestraCommandHandler::execCommand(const QString& command, const QStringList& args, CommandCallback callback) {
    QProcess process;
    if (isProjectOpen()) {
        process.setWorkingDirectory(OrchestraManager::instance().currentProjectPath());
    }

    process.start(command, args);

    if (!process.waitForFinished(30000)) {
        CommandResult result;
        result.success = false;
        result.error = "Command execution timed out";
        if (callback) callback(result);
        return;
    }

    CommandResult result;
    result.success = process.exitCode() == 0;
    result.output = QString::fromUtf8(process.readAllStandardOutput());
    result.error = QString::fromUtf8(process.readAllStandardError());
    result.exitCode = process.exitCode();
    if (callback) callback(result);
}

void OrchestraCommandHandler::shellCommand(const QString& command, CommandCallback callback) {
    #ifdef Q_OS_WIN
        execCommand("cmd", QStringList() << "/c" << command, callback);
    #else
        execCommand("/bin/sh", QStringList() << "-c" << command, callback);
    #endif
}

// ============================================================
// Helper Methods
// ============================================================

QString OrchestraCommandHandler::validatePath(const QString& path) {
    QString expanded = path;
    
    // Handle tilde expansion for home directory
    if (expanded.startsWith("~")) {
        expanded.replace(0, 1, QDir::homePath());
    }

    // Handle relative paths
    if (!QFileInfo(expanded).isAbsolute()) {
        if (isProjectOpen()) {
            expanded = OrchestraManager::instance().currentProjectPath() + "/" + expanded;
        } else {
            expanded = QDir::currentPath() + "/" + expanded;
        }
    }

    QFileInfo fileInfo(expanded);
    return fileInfo.absoluteFilePath();
}

QString OrchestraCommandHandler::validateGitRepository() {
    QString basePath = isProjectOpen() ? 
        OrchestraManager::instance().currentProjectPath() : 
        QDir::currentPath();

    // Check if .git directory exists
    if (QFileInfo(basePath + "/.git").exists()) {
        return basePath;
    }

    return QString();
}

bool OrchestraCommandHandler::isProjectOpen() {
    return OrchestraManager::instance().hasOpenProject();
}

void OrchestraCommandHandler::emitProgress(const QString& message) {
    qDebug() << "Progress:" << message;
}

void OrchestraCommandHandler::emitError(const QString& error) {
    qDebug() << "Error:" << error;
}

}  // namespace RawrXD
