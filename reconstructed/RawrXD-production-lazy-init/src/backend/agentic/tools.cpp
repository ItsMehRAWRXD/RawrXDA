/**
 * @file agentic_tools.cpp
 * @brief Complete implementation of AgenticToolExecutor
 */

#include "agentic_tools.hpp"

#include <QDebug>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QTextStream>
#include <algorithm>

AgenticToolExecutor::AgenticToolExecutor(QObject* parent) : QObject(parent) {
    initializeBuiltInTools();
}

void AgenticToolExecutor::initializeBuiltInTools() {
    registerTool("readFile", [this](const QStringList& args) {
        if (args.isEmpty()) return ToolResult{false, "", "Missing file path argument", 1, 0.0};
        return readFile(args[0]);
    });

    registerTool("writeFile", [this](const QStringList& args) {
        if (args.size() < 2) return ToolResult{false, "", "Missing file path or content argument", 1, 0.0};
        return writeFile(args[0], args[1]);
    });

    registerTool("listDirectory", [this](const QStringList& args) {
        if (args.isEmpty()) return ToolResult{false, "", "Missing directory path argument", 1, 0.0};
        return listDirectory(args[0]);
    });

    registerTool("deleteFile", [this](const QStringList& args) {
        if (args.isEmpty()) return ToolResult{false, "", "Missing file path argument", 1, 0.0};
        if (m_blockDeletes) return ToolResult{false, "", "File deletion is disabled by policy", 1, 0.0};
        QString path = args[0];
        QFile f(path);
        if (!f.exists()) return ToolResult{false, "", "File not found: " + path, 1, 0.0};
        if (!f.remove()) return ToolResult{false, "", "Failed to delete file: " + path, 1, 0.0};
        return ToolResult{true, "Deleted: " + path, "", 0, 0.0};
    });

    registerTool("executeCommand", [this](const QStringList& args) {
        if (args.isEmpty()) return ToolResult{false, "", "Missing command argument", 1, 0.0};
        QStringList cmdArgs = args.mid(1);
        return executeCommand(args[0], cmdArgs);
    });

    registerTool("grepSearch", [this](const QStringList& args) {
        if (args.size() < 2) return ToolResult{false, "", "Missing pattern or path argument", 1, 0.0};
        return grepSearch(args[0], args[1]);
    });

    registerTool("gitStatus", [this](const QStringList& args) {
        QString repoPath = args.isEmpty() ? "." : args[0];
        return gitStatus(repoPath);
    });

    registerTool("runTests", [this](const QStringList& args) {
        QString testPath = args.isEmpty() ? "." : args[0];
        return runTests(testPath);
    });

    registerTool("analyzeCode", [this](const QStringList& args) {
        if (args.isEmpty()) return ToolResult{false, "", "Missing file path argument", 1, 0.0};
        return analyzeCode(args[0]);
    });

    registerTool("toggleBlockDeletes", [this](const QStringList& args) {
        if (args.isEmpty()) return ToolResult{false, "", "Missing enable/disable argument (true/false)", 1, 0.0};
        QString val = args[0].toLower();
        m_blockDeletes = (val == "true" || val == "1" || val == "on");
        return ToolResult{true, QString("block_deletes=%1").arg(m_blockDeletes ? "true" : "false"), "", 0, 0.0};
    });

    registerTool("isBlockDeletesEnabled", [this](const QStringList& /*args*/) {
        return ToolResult{true, m_blockDeletes ? "true" : "false", "", 0, 0.0};
    });
}

void AgenticToolExecutor::registerTool(const QString& name, std::function<ToolResult(const QStringList&)> executor) {
    m_tools[name] = executor;
}

ToolResult AgenticToolExecutor::executeTool(const QString& toolName, const QStringList& arguments) {
    QElapsedTimer timer;
    timer.start();

    auto it = m_tools.find(toolName);
    if (it == m_tools.end()) {
        ToolResult result{false, "", "Tool not found: " + toolName, 1, 0.0};
        emit toolExecutionError(toolName, result.error);
        return result;
    }

    ToolResult result = it.value()(arguments);
    result.executionTimeMs = timer.elapsed();

    if (result.success) {
        emit toolExecutionCompleted(toolName, result.output);
        emit toolExecuted(toolName, result);
    } else {
        emit toolExecutionError(toolName, result.error);
        emit toolFailed(toolName, result.error);
    }

    return result;
}

ToolResult AgenticToolExecutor::readFile(const QString& filePath) {
    QElapsedTimer timer;
    timer.start();

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return ToolResult{false, "", "Cannot open file: " + filePath, 1, static_cast<double>(timer.elapsed())};
    }

    QTextStream in(&file);
    QString content = in.readAll();
    file.close();

    return ToolResult{true, content, "", 0, static_cast<double>(timer.elapsed())};
}

ToolResult AgenticToolExecutor::writeFile(const QString& filePath, const QString& content) {
    QElapsedTimer timer;
    timer.start();

    QFileInfo fileInfo(filePath);
    QDir parentDir = fileInfo.absoluteDir();
    if (!parentDir.exists()) {
        if (!parentDir.mkpath(".")) {
            return ToolResult{false, "", "Cannot create directory: " + parentDir.absolutePath(), 1, static_cast<double>(timer.elapsed())};
        }
    }

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return ToolResult{false, "", "Cannot open file for writing: " + filePath, 1, static_cast<double>(timer.elapsed())};
    }

    QTextStream out(&file);
    out << content;
    file.close();

    return ToolResult{true, "File written successfully: " + filePath, "", 0, static_cast<double>(timer.elapsed())};
}

ToolResult AgenticToolExecutor::listDirectory(const QString& dirPath) {
    QDir dir(dirPath);
    if (!dir.exists()) return ToolResult{false, "", "Directory not found: " + dirPath, 1, 0.0};

    QStringList entries = dir.entryList(QDir::AllEntries | QDir::NoDotAndDotDot);
    if (entries.isEmpty()) return ToolResult{true, "No files in directory", "", 0, 0.0};

    QString output = "Directory contents of " + dirPath + ":\n";
    for (const auto& entry : entries) {
        QFileInfo info(dir.absoluteFilePath(entry));
        output += (info.isDir() ? "[DIR]  " : "[FILE] ") + entry + "\n";
    }

    return ToolResult{true, output, "", 0, 0.0};
}

ToolResult AgenticToolExecutor::executeCommand(const QString& program, const QStringList& args) {
    if (m_blockDeletes && isDestructiveCommand(program, args)) {
        return ToolResult{false, "", "Blocked destructive command by policy", 1, 0.0};
    }

    QProcess process;
    process.start(program, args);

    if (!process.waitForStarted()) {
        return ToolResult{false, "", "Cannot find or execute program: " + program, 1, 0.0};
    }

    if (!process.waitForFinished(30000)) {
        process.kill();
        process.waitForFinished();
        return ToolResult{false, "", "Command timed out after 30 seconds", 1, 0.0};
    }

    QString output = QString::fromUtf8(process.readAllStandardOutput());
    QString error = QString::fromUtf8(process.readAllStandardError());
    int exitCode = process.exitCode();

    if (exitCode == 0) {
        return ToolResult{true, output, "", exitCode, 0.0};
    }

    return ToolResult{false, output, error.isEmpty() ? "Exit code: " + QString::number(exitCode) : error, exitCode, 0.0};
}

bool AgenticToolExecutor::isDestructiveCommand(const QString& program, const QStringList& args) const {
    QString prog = QFileInfo(program).fileName().toLower();
    if (prog == "rm" || prog == "del" || prog == "rmdir") return true;

    if (prog == "bash" || prog == "sh" || prog == "pwsh" || prog == "powershell" || prog == "cmd.exe") {
        QString joined = args.join(" ").toLower();
        if (joined.contains("\\brm\\b") || joined.contains("\\bdel\\b") || joined.contains("remove-item") || joined.contains("rmdir")) return true;
    }

    if (prog.contains("rm") && prog.size() <= 3) return true;
    return false;
}

ToolResult AgenticToolExecutor::grepSearch(const QString& pattern, const QString& path) {
    QDir searchDir(path);
    if (!searchDir.exists()) return ToolResult{false, "", "Path not found: " + path, 1, 0.0};

    QStringList results;
    QRegularExpression regex(pattern);
    if (!regex.isValid()) return ToolResult{false, "", "Invalid regex pattern: " + regex.errorString(), 1, 0.0};

    searchDir.setFilter(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
    QStringList entries = searchDir.entryList();

    for (const auto& entry : entries) {
        QString fullPath = searchDir.absoluteFilePath(entry);
        QFileInfo info(fullPath);

        if (info.isDir()) {
            ToolResult subResult = grepSearch(pattern, fullPath);
            if (subResult.success && !subResult.output.isEmpty()) results.append(subResult.output);
        } else if (info.isFile()) {
            QFile file(fullPath);
            if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                QTextStream in(&file);
                int lineNum = 0;
                while (!in.atEnd()) {
                    QString line = in.readLine();
                    lineNum++;
                    if (regex.match(line).hasMatch()) results.append(QString("%1:%2: %3").arg(fullPath).arg(lineNum).arg(line));
                }
                file.close();
            }
        }
    }

    if (results.isEmpty()) return ToolResult{true, "No matches found", "", 0, 0.0};
    return ToolResult{true, results.join("\n"), "", 0, 0.0};
}

ToolResult AgenticToolExecutor::gitStatus(const QString& repoPath) {
    return executeCommand("git", QStringList() << "-C" << repoPath << "status" << "--short");
}

ToolResult AgenticToolExecutor::runTests(const QString& testPath) {
    QDir dir(testPath);
    if (!dir.exists()) return ToolResult{false, "", "Test path not found: " + testPath, 1, 0.0};

    if (dir.exists("CMakeLists.txt")) return executeCommand("ctest", QStringList() << "--output-on-failure");

    if (dir.exists("pytest.ini") || dir.exists("conftest.py")) return executeCommand("pytest", QStringList());

    return ToolResult{false, "", "No test framework detected in: " + testPath + " (not found)", 1, 0.0};
}

ToolResult AgenticToolExecutor::analyzeCode(const QString& filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return ToolResult{false, "", "Cannot open file: " + filePath, 1, 0.0};

    QTextStream in(&file);
    QString content = in.readAll();
    file.close();

    QString language = detectLanguage(filePath);
    int lines = content.count('\n');
    int functions = 0;
    int classes = 0;

    if (language == "cpp" || language == "c") {
        QRegularExpression funcPattern("\\b[a-zA-Z_][a-zA-Z0-9_]*\\s*\\([^)]*\\)\\s*[{;]");
        auto funcMatches = funcPattern.globalMatch(content);
        while (funcMatches.hasNext()) {
            funcMatches.next();
            functions++;
        }

        QRegularExpression classPattern("\\b(class|struct)\\s+[a-zA-Z_]");
        auto classMatches = classPattern.globalMatch(content);
        while (classMatches.hasNext()) {
            classMatches.next();
            classes++;
        }
    } else if (language == "python") {
        QRegularExpression funcPattern("^\\s*def\\s+[a-zA-Z_]", QRegularExpression::MultilineOption);
        auto funcMatches = funcPattern.globalMatch(content);
        while (funcMatches.hasNext()) {
            funcMatches.next();
            functions++;
        }

        QRegularExpression classPattern("^\\s*class\\s+[a-zA-Z_]", QRegularExpression::MultilineOption);
        auto classMatches = classPattern.globalMatch(content);
        while (classMatches.hasNext()) {
            classMatches.next();
            classes++;
        }
    }

    QString analysis = QString(
        "Code Analysis for %1:\n"
        "Language: %2\n"
        "Total Lines: %3\n"
        "Functions: %4\n"
        "Classes: %5\n"
        "Average Function Length: %6 lines")
                               .arg(filePath, language)
                               .arg(lines)
                               .arg(functions)
                               .arg(classes)
                               .arg(functions > 0 ? lines / functions : 0);

    return ToolResult{true, analysis, "", 0, 0.0};
}

QString AgenticToolExecutor::detectLanguage(const QString& filePath) {
    if (filePath.endsWith(".cpp") || filePath.endsWith(".cc") || filePath.endsWith(".cxx")) return "cpp";
    if (filePath.endsWith(".c")) return "c";
    if (filePath.endsWith(".h") || filePath.endsWith(".hpp")) return "cpp";
    if (filePath.endsWith(".py")) return "python";
    if (filePath.endsWith(".js")) return "javascript";
    if (filePath.endsWith(".ts")) return "typescript";
    if (filePath.endsWith(".java")) return "java";
    if (filePath.endsWith(".cs")) return "csharp";
    if (filePath.endsWith(".go")) return "go";
    if (filePath.endsWith(".rs")) return "rust";
    return "unknown";
}

ToolResult AgenticToolExecutor::executeProcess(const QString& program, const QStringList& args, int timeoutMs) {
    QProcess process;
    process.start(program, args);

    if (!process.waitForFinished(timeoutMs)) {
        process.kill();
        return ToolResult{false, "", "Process timeout", 1, 0.0};
    }

    return ToolResult{process.exitCode() == 0,
                      QString::fromUtf8(process.readAllStandardOutput()),
                      QString::fromUtf8(process.readAllStandardError()),
                      process.exitCode(),
                      0.0};
}

