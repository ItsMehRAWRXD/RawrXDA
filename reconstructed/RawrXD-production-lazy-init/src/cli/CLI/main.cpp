#include "cli_compiler_engine.hpp"
#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <sstream>
#include <iomanip>
#include <algorithm>

/**
 * @file cli_main.cpp
 * @brief RawrXD CLI Compiler - Production-Ready Command-Line Interface
 * 
 * Complete compiler CLI supporting:
 * - Single-file and batch compilation
 * - Configuration files (rawrxd.toml/yaml)
 * - Watch mode for continuous compilation
 * - REPL for expression testing
 * - Multiple output formats (Text, JSON, XML, CSV)
 * - Project management
 * - Build system integration
 * 
 * Usage Examples:
 *   rawrxd-cli compile input.src output.exe
 *   rawrxd-cli compile -O3 --debug input.src
 *   rawrxd-cli build --config Release
 *   rawrxd-cli watch src/
 *   rawrxd-cli repl
 * 
 * Environment:
 *   RAWRXD_HOME    - Installation directory
 *   RAWRXD_CONFIG  - Configuration file path
 *   RAWRXD_VERBOSE - Enable verbose logging
 *   
 *   file read <path>
 *   file write <path> <content>
 *   file find <pattern> [path]
 *   file search <query> [--pattern glob] [path]
 *   file replace <search> <replace> [--pattern glob] [--dry-run]
 *   
 *   ai load <model-path>
 *   ai unload
 *   ai infer <prompt>
 *   ai complete <context>
 *   ai explain <code>
 *   ai refactor <code> <instruction>
 *   
 *   test discover
 *   test run [test-ids...]
 *   test coverage
 *   
 *   diag run
 *   diag info
 *   diag status
 */

using namespace RawrXD;

// ANSI color codes for terminal output
namespace Colors {
    const char* Reset = "\033[0m";
    const char* Red = "\033[31m";
    const char* Green = "\033[32m";
    const char* Yellow = "\033[33m";
    const char* Blue = "\033[34m";
    const char* Magenta = "\033[35m";
    const char* Cyan = "\033[36m";
    const char* White = "\033[37m";
    const char* Bold = "\033[1m";
}

class CLIApp : public QObject {
    Q_OBJECT

public:
    CLIApp(QObject* parent = nullptr)
        : QObject(parent)
        , m_out(stdout)
        , m_in(stdin)
    {
    }

    int run(int argc, char* argv[]) {
        QCoreApplication app(argc, argv);
        app.setApplicationName("RawrXD-CLI");
        app.setApplicationVersion("1.0.0");

        // Initialize orchestra manager
        if (!OrchestraManager::instance().initialize()) {
            printError("Failed to initialize OrchestraManager");
            return 1;
        }

        // Initialize agentic lazy init model loader
        AutoModelLoader::QtIDEAutoLoader::initialize();
        AutoModelLoader::QtIDEAutoLoader::autoLoadOnStartup();

        // Initialize lazy directory loader
        RawrXD::LazyDirectoryLoader::instance().initialize(100, 100);

        // Connect signals for output
        connectSignals();

        // Parse command line
        QCommandLineParser parser;
        parser.setApplicationDescription("RawrXD IDE Command-Line Interface");
        parser.addHelpOption();
        parser.addVersionOption();

        QCommandLineOption interactiveOption({"i", "interactive"},
            "Enter interactive mode");
        parser.addOption(interactiveOption);

        QCommandLineOption projectOption({"p", "project"},
            "Project directory", "path");
        parser.addOption(projectOption);

        QCommandLineOption verboseOption("verbose",
            "Verbose output");
        parser.addOption(verboseOption);

        QCommandLineOption jsonOption("json", "Output in JSON format");
        parser.addOption(jsonOption);

        // Headless mode options for CI/CD
        QCommandLineOption headlessOption("headless",
            "Run in headless mode (for CI/CD pipelines)");
        parser.addOption(headlessOption);

        QCommandLineOption batchFileOption("batch",
            "Execute commands from a batch file", "file");
        parser.addOption(batchFileOption);

        QCommandLineOption timeoutOption("timeout",
            "Timeout in seconds for headless operations (default: 300)", "seconds");
        parser.addOption(timeoutOption);

        QCommandLineOption failFastOption("fail-fast",
            "Exit on first error in batch mode");
        parser.addOption(failFastOption);

        parser.addPositionalArgument("command", "Command to execute");
        parser.addPositionalArgument("args", "Command arguments", "[args...]");

        parser.process(app);

        m_verbose = parser.isSet(verboseOption);
        m_jsonOutput = parser.isSet(jsonOption);

        // Configure headless mode if enabled
        if (parser.isSet(headlessOption)) {
            HeadlessConfig headlessCfg;
            headlessCfg.enabled = true;
            headlessCfg.outputFormat = m_jsonOutput ? "json" : "plain";
            headlessCfg.failFast = parser.isSet(failFastOption);
            headlessCfg.logLevel = m_verbose ? "DEBUG" : "INFO";
            
            if (parser.isSet(timeoutOption)) {
                headlessCfg.timeoutSeconds = parser.value(timeoutOption).toInt();
            }
            
            OrchestraManager::instance().setHeadlessMode(headlessCfg);
            
            // In headless mode, batch file takes priority
            if (parser.isSet(batchFileOption)) {
                return runBatchFile(parser.value(batchFileOption));
            }
        }

        // Open project if specified
        if (parser.isSet(projectOption)) {
            QString projectPath = parser.value(projectOption);
            OrchestraManager::instance().openProject(projectPath);
        }

        // Headless mode with commands runs batch
        if (parser.isSet(headlessOption) && !parser.positionalArguments().isEmpty()) {
            QStringList commands;
            commands << parser.positionalArguments().join(" ");
            return OrchestraManager::instance().runHeadlessBatch(commands);
        }

        // Interactive mode or single command
        if (parser.isSet(interactiveOption) || parser.positionalArguments().isEmpty()) {
            return runInteractive(app);
        } else {
            return executeCommand(parser.positionalArguments());
        }
    }

private:
    int runBatchFile(const QString& filePath) {
        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            printError(QString("Cannot open batch file: %1").arg(filePath));
            return 1;
        }
        
        QStringList commands;
        QTextStream in(&file);
        while (!in.atEnd()) {
            QString line = in.readLine().trimmed();
            // Skip empty lines and comments
            if (!line.isEmpty() && !line.startsWith('#') && !line.startsWith("//")) {
                commands << line;
            }
        }
        file.close();
        
        if (commands.isEmpty()) {
            printError("Batch file is empty or contains only comments");
            return 1;
        }
        
        if (m_verbose) {
            m_out << Colors::Cyan << "Running batch file: " << filePath << Colors::Reset << Qt::endl;
            m_out << Colors::Cyan << "Commands: " << commands.size() << Colors::Reset << Qt::endl;
        }
        
        int exitCode = OrchestraManager::instance().runHeadlessBatch(commands, 
            [this](const TaskResult& result) {
                if (m_jsonOutput) {
                    QJsonDocument doc(result.data);
                    m_out << doc.toJson() << Qt::endl;
                } else {
                    if (result.success) {
                        printSuccess(result.message);
                    } else {
                        printError(result.message);
                    }
                    if (m_verbose) {
                        m_out << "Duration: " << result.durationMs << " ms" << Qt::endl;
                    }
                }
            });
        
        return exitCode;
    }

    void connectSignals() {
        auto& om = OrchestraManager::instance();

        connect(&om, &OrchestraManager::buildOutput, this, [this](const QString& line, bool isError) {
            if (isError) {
                printError(line);
            } else {
                m_out << line << Qt::endl;
            }
        });

        connect(&om, &OrchestraManager::buildFinished, this, [this](bool success, const QString& msg) {
            if (success) {
                printSuccess(msg);
            } else {
                printError(msg);
            }
        });

        connect(&om, &OrchestraManager::aiInferenceToken, this, [this](const QString& token) {
            m_out << token;
            m_out.flush();
        });

        connect(&om, &OrchestraManager::terminalOutput, this, [this](int, const QString& output) {
            m_out << output;
            m_out.flush();
        });

        connect(&om, &OrchestraManager::info, this, [this](const QString& component, const QString& msg) {
            if (m_verbose) {
                m_out << Colors::Cyan << "[" << component << "] " << Colors::Reset << msg << Qt::endl;
            }
        });

        connect(&om, &OrchestraManager::error, this, [this](const QString& component, const QString& msg) {
            printError(QString("[%1] %2").arg(component, msg));
        });
    }

    int runInteractive(QCoreApplication& app) {
        printBanner();

        m_running = true;

        // Use a timer to read input without blocking
        QTimer inputTimer;
        connect(&inputTimer, &QTimer::timeout, this, [this, &app]() {
            if (!m_running) {
                app.quit();
                return;
            }

            // Print prompt
            m_out << Colors::Green << "rawrxd" << Colors::Reset;
            if (OrchestraManager::instance().hasOpenProject()) {
                QString projectName = QDir(OrchestraManager::instance().currentProjectPath()).dirName();
                m_out << Colors::Blue << " [" << projectName << "]" << Colors::Reset;
            }
            m_out << "> ";
            m_out.flush();

            // Read line
            QString line = m_in.readLine();
            if (line.isNull()) {
                m_running = false;
                return;
            }

            line = line.trimmed();
            if (line.isEmpty()) {
                return;
            }

            // Parse and execute
            QStringList args = parseCommandLine(line);
            if (!args.isEmpty()) {
                if (args[0] == "exit" || args[0] == "quit" || args[0] == "q") {
                    m_running = false;
                    return;
                }
                executeCommand(args);
            }
        });

        inputTimer.start(100);

        return app.exec();
    }

    QStringList parseCommandLine(const QString& line) {
        QStringList args;
        QString current;
        bool inQuotes = false;
        QChar quoteChar;

        for (int i = 0; i < line.length(); ++i) {
            QChar c = line[i];

            if (inQuotes) {
                if (c == quoteChar) {
                    inQuotes = false;
                } else {
                    current += c;
                }
            } else {
                if (c == '"' || c == '\'') {
                    inQuotes = true;
                    quoteChar = c;
                } else if (c.isSpace()) {
                    if (!current.isEmpty()) {
                        args << current;
                        current.clear();
                    }
                } else {
                    current += c;
                }
            }
        }

        if (!current.isEmpty()) {
            args << current;
        }

        return args;
    }

    int executeCommand(const QStringList& args) {
        if (args.isEmpty()) return 0;

        QString cmd = args[0].toLower();
        QStringList cmdArgs = args.mid(1);

        // Project commands
        if (cmd == "project") {
            return handleProjectCommand(cmdArgs);
        }
        // Build commands
        else if (cmd == "build") {
            return handleBuildCommand(cmdArgs);
        }
        else if (cmd == "clean") {
            return handleCleanCommand();
        }
        else if (cmd == "rebuild") {
            return handleRebuildCommand(cmdArgs);
        }
        else if (cmd == "configure") {
            return handleConfigureCommand(cmdArgs);
        }
        // Git commands
        else if (cmd == "git") {
            return handleGitCommand(cmdArgs);
        }
        // Execution commands
        else if (cmd == "exec") {
            return handleExecCommand(cmdArgs);
        }
        else if (cmd == "shell" || cmd == "sh") {
            return handleShellCommand(cmdArgs);
        }
        // File commands
        else if (cmd == "file") {
            return handleFileCommand(cmdArgs);
        }
        // AI commands
        else if (cmd == "ai") {
            return handleAICommand(cmdArgs);
        }
        // Test commands
        else if (cmd == "test") {
            return handleTestCommand(cmdArgs);
        }
        // Diagnostic commands
        else if (cmd == "diag") {
            return handleDiagCommand(cmdArgs);
        }
        // Log commands
        else if (cmd == "log") {
            return handleLogCommand(cmdArgs);
        }
        // Help
        else if (cmd == "help" || cmd == "?") {
            printHelp();
            return 0;
        }
        // Clear screen
        else if (cmd == "clear" || cmd == "cls") {
            m_out << "\033[2J\033[H";
            return 0;
        }
        // PWD
        else if (cmd == "pwd") {
            if (OrchestraManager::instance().hasOpenProject()) {
                m_out << OrchestraManager::instance().currentProjectPath() << Qt::endl;
            } else {
                m_out << QDir::currentPath() << Qt::endl;
            }
            return 0;
        }
        // LS
        else if (cmd == "ls" || cmd == "dir") {
            return handleLsCommand(cmdArgs);
        }
        else {
            printError(QString("Unknown command: %1").arg(cmd));
            m_out << "Type 'help' for a list of commands." << Qt::endl;
            return 1;
        }
    }

    // ================================================================
    // Command Handlers
    // ================================================================

    int handleProjectCommand(const QStringList& args) {
        if (args.isEmpty()) {
            if (OrchestraManager::instance().hasOpenProject()) {
                m_out << "Current project: " << OrchestraManager::instance().currentProjectPath() << Qt::endl;
            } else {
                m_out << "No project open" << Qt::endl;
            }
            return 0;
        }

        QString subcmd = args[0].toLower();

        if (subcmd == "open" && args.size() > 1) {
            bool done = false;
            int result = 0;
            OrchestraManager::instance().openProject(args[1], [this, &done, &result](const TaskResult& r) {
                done = true;
                result = r.success ? 0 : 1;
                printResult(r);
            });
            waitForCompletion(done);
            return result;
        }
        else if (subcmd == "close") {
            OrchestraManager::instance().closeProject();
            printSuccess("Project closed");
            return 0;
        }
        else if (subcmd == "create" && args.size() > 2) {
            QString path = args[1];
            QString templateName = args[2];
            bool done = false;
            int result = 0;
            OrchestraManager::instance().createProject(path, templateName, QVariantMap(),
                [this, &done, &result](const TaskResult& r) {
                    done = true;
                    result = r.success ? 0 : 1;
                    printResult(r);
                });
            waitForCompletion(done);
            return result;
        }
        else {
            m_out << "Usage: project [open <path>|close|create <path> <template>]" << Qt::endl;
            m_out << "Templates: cpp, python, rust" << Qt::endl;
            return 1;
        }
    }

    int handleBuildCommand(const QStringList& args) {
        QString target;
        QString config = "Release";

        for (int i = 0; i < args.size(); ++i) {
            if (args[i] == "--config" && i + 1 < args.size()) {
                config = args[++i];
            } else if (!args[i].startsWith("--")) {
                target = args[i];
            }
        }

        bool done = false;
        int result = 0;

        OrchestraManager::instance().build(target, config,
            [this](const QString& line, bool isError) {
                if (isError) {
                    m_out << Colors::Red << line << Colors::Reset << Qt::endl;
                } else {
                    m_out << line << Qt::endl;
                }
            },
            [this, &done, &result](const TaskResult& r) {
                done = true;
                result = r.success ? 0 : 1;
                printResult(r);
            });

        waitForCompletion(done);
        return result;
    }

    int handleCleanCommand() {
        bool done = false;
        int result = 0;
        OrchestraManager::instance().clean([this, &done, &result](const TaskResult& r) {
            done = true;
            result = r.success ? 0 : 1;
            printResult(r);
        });
        waitForCompletion(done);
        return result;
    }

    int handleRebuildCommand(const QStringList& args) {
        QString target;
        QString config = "Release";

        for (int i = 0; i < args.size(); ++i) {
            if (args[i] == "--config" && i + 1 < args.size()) {
                config = args[++i];
            } else if (!args[i].startsWith("--")) {
                target = args[i];
            }
        }

        bool done = false;
        int result = 0;
        OrchestraManager::instance().rebuild(target, config, nullptr,
            [this, &done, &result](const TaskResult& r) {
                done = true;
                result = r.success ? 0 : 1;
                printResult(r);
            });
        waitForCompletion(done);
        return result;
    }

    int handleConfigureCommand(const QStringList& args) {
        QVariantMap options;
        for (int i = 0; i < args.size(); ++i) {
            if (args[i] == "-G" && i + 1 < args.size()) {
                options["generator"] = args[++i];
            }
        }

        bool done = false;
        int result = 0;
        OrchestraManager::instance().configure(options,
            [this](const QString& line, bool) {
                m_out << line << Qt::endl;
            },
            [this, &done, &result](const TaskResult& r) {
                done = true;
                result = r.success ? 0 : 1;
                printResult(r);
            });
        waitForCompletion(done);
        return result;
    }

    int handleGitCommand(const QStringList& args) {
        if (args.isEmpty()) {
            m_out << "Usage: git [status|add|commit|push|pull|branch|checkout|log]" << Qt::endl;
            return 1;
        }

        QString subcmd = args[0].toLower();
        bool done = false;
        int result = 0;

        if (subcmd == "status") {
            OrchestraManager::instance().vcsStatus([this, &done, &result](const TaskResult& r) {
                done = true;
                result = r.success ? 0 : 1;
                if (r.success) {
                    QJsonArray files = r.data["files"].toArray();
                    if (files.isEmpty()) {
                        m_out << "Nothing to commit, working tree clean" << Qt::endl;
                    } else {
                        for (const QJsonValue& v : files) {
                            QJsonObject f = v.toObject();
                            QString status = f["status"].toString();
                            QString path = f["path"].toString();

                            if (status == "M" || status == " M") {
                                m_out << Colors::Yellow << "  modified:   " << path << Colors::Reset << Qt::endl;
                            } else if (status == "A" || status == "??" ) {
                                m_out << Colors::Green << "  new file:   " << path << Colors::Reset << Qt::endl;
                            } else if (status == "D") {
                                m_out << Colors::Red << "  deleted:    " << path << Colors::Reset << Qt::endl;
                            } else {
                                m_out << "  " << status << " " << path << Qt::endl;
                            }
                        }
                    }
                } else {
                    printError(r.message);
                }
            });
        }
        else if (subcmd == "add") {
            QStringList files = args.mid(1);
            OrchestraManager::instance().vcsStage(files, [this, &done, &result](const TaskResult& r) {
                done = true;
                result = r.success ? 0 : 1;
                printResult(r);
            });
        }
        else if (subcmd == "commit" && args.size() > 1) {
            QString message = args.mid(1).join(' ');
            OrchestraManager::instance().vcsCommit(message, [this, &done, &result](const TaskResult& r) {
                done = true;
                result = r.success ? 0 : 1;
                printResult(r);
            });
        }
        else if (subcmd == "push") {
            QString remote = args.size() > 1 ? args[1] : "origin";
            QString branch = args.size() > 2 ? args[2] : QString();
            OrchestraManager::instance().vcsPush(remote, branch, [this, &done, &result](const TaskResult& r) {
                done = true;
                result = r.success ? 0 : 1;
                printResult(r);
            });
        }
        else if (subcmd == "pull") {
            QString remote = args.size() > 1 ? args[1] : "origin";
            QString branch = args.size() > 2 ? args[2] : QString();
            OrchestraManager::instance().vcsPull(remote, branch, [this, &done, &result](const TaskResult& r) {
                done = true;
                result = r.success ? 0 : 1;
                printResult(r);
            });
        }
        else if (subcmd == "branch") {
            if (args.size() > 1) {
                OrchestraManager::instance().vcsCreateBranch(args[1], true, [this, &done, &result](const TaskResult& r) {
                    done = true;
                    result = r.success ? 0 : 1;
                    printResult(r);
                });
            } else {
                QStringList branches = OrchestraManager::instance().vcsListBranches(false);
                for (const QString& b : branches) {
                    m_out << "  " << b << Qt::endl;
                }
                done = true;
                result = 0;
            }
        }
        else if (subcmd == "checkout" && args.size() > 1) {
            OrchestraManager::instance().vcsCheckout(args[1], [this, &done, &result](const TaskResult& r) {
                done = true;
                result = r.success ? 0 : 1;
                printResult(r);
            });
        }
        else if (subcmd == "log") {
            int count = 10;
            for (int i = 1; i < args.size(); ++i) {
                if (args[i] == "--count" && i + 1 < args.size()) {
                    count = args[++i].toInt();
                }
            }
            OrchestraManager::instance().vcsLog(count, [this, &done, &result](const TaskResult& r) {
                done = true;
                result = r.success ? 0 : 1;
                if (r.success) {
                    QJsonArray commits = r.data["commits"].toArray();
                    for (const QJsonValue& v : commits) {
                        QJsonObject c = v.toObject();
                        m_out << Colors::Yellow << "commit " << c["hash"].toString().left(8) << Colors::Reset << Qt::endl;
                        m_out << "Author: " << c["author"].toString() << Qt::endl;
                        m_out << "Date:   " << c["date"].toString() << Qt::endl;
                        m_out << Qt::endl;
                        m_out << "    " << c["message"].toString() << Qt::endl;
                        m_out << Qt::endl;
                    }
                }
            });
        }
        else {
            m_out << "Unknown git command: " << subcmd << Qt::endl;
            done = true;
            result = 1;
        }

        waitForCompletion(done);
        return result;
    }

    int handleExecCommand(const QStringList& args) {
        if (args.isEmpty()) {
            m_out << "Usage: exec <command> [args...]" << Qt::endl;
            return 1;
        }

        QString command = args[0];
        QStringList cmdArgs = args.mid(1);

        bool done = false;
        int result = 0;

        OrchestraManager::instance().exec(command, cmdArgs, QString(),
            [this](const QString& line, bool isError) {
                if (isError) {
                    m_out << Colors::Red << line << Colors::Reset << Qt::endl;
                } else {
                    m_out << line << Qt::endl;
                }
            },
            [this, &done, &result](const TaskResult& r) {
                done = true;
                result = r.exitCode;
            });

        waitForCompletion(done);
        return result;
    }

    int handleShellCommand(const QStringList& args) {
        if (args.isEmpty()) {
            m_out << "Usage: shell <command>" << Qt::endl;
            return 1;
        }

        QString command = args.join(' ');
        bool done = false;
        int result = 0;

        OrchestraManager::instance().shell(command,
            [this](const QString& line, bool isError) {
                if (isError) {
                    m_out << Colors::Red << line << Colors::Reset << Qt::endl;
                } else {
                    m_out << line << Qt::endl;
                }
            },
            [this, &done, &result](const TaskResult& r) {
                done = true;
                result = r.exitCode;
            });

        waitForCompletion(done);
        return result;
    }

    int handleFileCommand(const QStringList& args) {
        if (args.isEmpty()) {
            m_out << "Usage: file [read|write|find|search|replace] ..." << Qt::endl;
            return 1;
        }

        QString subcmd = args[0].toLower();

        if (subcmd == "read" && args.size() > 1) {
            QString content = OrchestraManager::instance().readFile(args[1]);
            if (content.isNull()) {
                printError("Failed to read file: " + args[1]);
                return 1;
            }
            m_out << content;
            if (!content.endsWith('\n')) m_out << Qt::endl;
            return 0;
        }
        else if (subcmd == "write" && args.size() > 2) {
            QString content = args.mid(2).join(' ');
            if (OrchestraManager::instance().writeFile(args[1], content)) {
                printSuccess("File written: " + args[1]);
                return 0;
            } else {
                printError("Failed to write file: " + args[1]);
                return 1;
            }
        }
        else if (subcmd == "find" && args.size() > 1) {
            QString pattern = args[1];
            QString path = args.size() > 2 ? args[2] : QString();
            QStringList files = OrchestraManager::instance().findFiles(pattern, path);
            for (const QString& f : files) {
                m_out << f << Qt::endl;
            }
            m_out << Colors::Cyan << files.size() << " file(s) found" << Colors::Reset << Qt::endl;
            return 0;
        }
        else if (subcmd == "search" && args.size() > 1) {
            QString query = args[1];
            QString pattern = "*";
            QString path;

            for (int i = 2; i < args.size(); ++i) {
                if (args[i] == "--pattern" && i + 1 < args.size()) {
                    pattern = args[++i];
                } else {
                    path = args[i];
                }
            }

            bool done = false;
            int result = 0;
            OrchestraManager::instance().searchInFiles(query, pattern, path,
                [this, &done, &result](const TaskResult& r) {
                    done = true;
                    result = r.success ? 0 : 1;
                    if (r.success) {
                        QJsonArray matches = r.data["matches"].toArray();
                        for (const QJsonValue& v : matches) {
                            QJsonObject m = v.toObject();
                            m_out << Colors::Magenta << m["file"].toString()
                                  << Colors::Reset << ":"
                                  << Colors::Green << m["line"].toInt()
                                  << Colors::Reset << ": "
                                  << m["text"].toString() << Qt::endl;
                        }
                        m_out << Qt::endl << r.message << Qt::endl;
                    }
                });
            waitForCompletion(done);
            return result;
        }
        else if (subcmd == "replace" && args.size() > 2) {
            QString search = args[1];
            QString replace = args[2];
            QString pattern = "*";
            QString path;
            bool dryRun = false;

            for (int i = 3; i < args.size(); ++i) {
                if (args[i] == "--pattern" && i + 1 < args.size()) {
                    pattern = args[++i];
                } else if (args[i] == "--dry-run") {
                    dryRun = true;
                } else {
                    path = args[i];
                }
            }

            bool done = false;
            int result = 0;
            OrchestraManager::instance().replaceInFiles(search, replace, pattern, path, dryRun,
                [this, &done, &result](const TaskResult& r) {
                    done = true;
                    result = r.success ? 0 : 1;
                    printResult(r);
                });
            waitForCompletion(done);
            return result;
        }
        else {
            m_out << "Unknown file command: " << subcmd << Qt::endl;
            return 1;
        }
    }

    int handleAICommand(const QStringList& args) {
        if (args.isEmpty()) {
            m_out << "Usage: ai [load|unload|infer|complete|explain|refactor|discover|list-models|model-info|model-load] ..." << Qt::endl;
            return 1;
        }

        QString subcmd = args[0].toLower();
        bool done = false;
        int result = 0;

        if (subcmd == "load" && args.size() > 1) {
            OrchestraManager::instance().aiLoadModel(args[1],
                [this](int percent, const QString& status) {
                    m_out << "\rLoading: " << percent << "% " << status;
                    m_out.flush();
                },
                [this, &done, &result](const TaskResult& r) {
                    done = true;
                    result = r.success ? 0 : 1;
                    m_out << Qt::endl;
                    printResult(r);
                });
        }
        else if (subcmd == "unload") {
            OrchestraManager::instance().aiUnloadModel();
            printSuccess("Model unloaded");
            return 0;
        }
        else if (subcmd == "infer" && args.size() > 1) {
            QString prompt = args.mid(1).join(' ');
            OrchestraManager::instance().aiInfer(prompt, QVariantMap(),
                [this](const QString& token, bool) {
                    m_out << token;
                    m_out.flush();
                },
                [this, &done, &result](const TaskResult& r) {
                    done = true;
                    result = r.success ? 0 : 1;
                    m_out << Qt::endl;
                });
        }
        else if (subcmd == "status") {
            if (OrchestraManager::instance().aiIsModelLoaded()) {
                m_out << "Model loaded: " << OrchestraManager::instance().aiModelName() << Qt::endl;
            } else {
                m_out << "No model loaded" << Qt::endl;
            }
            return 0;
        }
        // Model discovery commands
        else if (subcmd == "discover") {
            QStringList searchPaths;
            if (args.size() > 1) {
                searchPaths << args[1];
            }
            
            QList<RawrXD::ModelInfo> models = OrchestraManager::instance().discoverModels(searchPaths, true);
            
            m_out << "Discovered " << models.size() << " models:" << Qt::endl;
            for (const auto& model : models) {
                m_out << "  " << model.name << " (" << model.path << ")" << Qt::endl;
            }
            return 0;
        }
        else if (subcmd == "list-models") {
            QList<RawrXD::ModelInfo> models = OrchestraManager::instance().getDiscoveredModels();
            m_out << "Discovered models (" << models.size() << "):" << Qt::endl;
            for (const auto& model : models) {
                m_out << "  " << model.name << " (" << model.path << ")" << Qt::endl;
                m_out << "    Framework: " << model.framework << Qt::endl;
                m_out << "    Size: " << model.size << " bytes" << Qt::endl;
                m_out << "    Format: " << model.format << Qt::endl;
                if (model.cached) {
                    m_out << "    Cached: Yes" << Qt::endl;
                }
                m_out << Qt::endl;
            }
            return 0;
        }
        else if (subcmd == "model-info" && args.size() > 1) {
            QString modelId = args[1];
            RawrXD::ModelInfo info = OrchestraManager::instance().getModelInfo(modelId);
            if (info.id.isEmpty()) {
                printError("Model not found: " + modelId);
                return 1;
            }
            m_out << "Model Information:" << Qt::endl;
            m_out << "  ID: " << info.id << Qt::endl;
            m_out << "  Name: " << info.name << Qt::endl;
            m_out << "  Path: " << info.path << Qt::endl;
            m_out << "  Size: " << info.size << " bytes" << Qt::endl;
            m_out << "  Format: " << info.format << Qt::endl;
            m_out << "  Framework: " << info.framework << Qt::endl;
            m_out << "  Parameters: " << info.parameters << Qt::endl;
            m_out << "  Quantization: " << info.quantization << Qt::endl;
            m_out << "  Cached: " << (info.cached ? "Yes" : "No") << Qt::endl;
            m_out << "  Checksum: " << info.checksum << Qt::endl;
            return 0;
        }
        else if (subcmd == "model-load" && args.size() > 1) {
            QString modelId = args[1];
            bool success = OrchestraManager::instance().loadModel(modelId, 
                [this](int progress) {
                    m_out << "\rLoading model: " << progress << "%";
                    m_out.flush();
                });
            
            m_out << Qt::endl;
            if (success) {
                printSuccess("Model loaded: " + modelId);
                return 0;
            } else {
                printError("Failed to load model: " + modelId);
                return 1;
            }
        }
        else {
            m_out << "Unknown AI command: " << subcmd << Qt::endl;
            done = true;
            result = 1;
        }

        waitForCompletion(done);
        return result;
    }

    int handleTestCommand(const QStringList& args) {
        if (args.isEmpty()) {
            m_out << "Usage: test [discover|run|coverage] ..." << Qt::endl;
            return 1;
        }

        QString subcmd = args[0].toLower();
        bool done = false;
        int result = 0;

        if (subcmd == "discover") {
            OrchestraManager::instance().testDiscover([this, &done, &result](const TaskResult& r) {
                done = true;
                result = r.success ? 0 : 1;
                if (r.success) {
                    QJsonArray tests = r.data["tests"].toArray();
                    if (tests.isEmpty()) {
                        m_out << "No tests found" << Qt::endl;
                    } else {
                        for (const QJsonValue& v : tests) {
                            QJsonObject t = v.toObject();
                            m_out << "  " << t["id"].toString() << ": " << t["name"].toString() << Qt::endl;
                        }
                    }
                }
            });
        }
        else if (subcmd == "run") {
            QStringList testIds = args.mid(1);
            auto outputCb = [this](const QString& line, bool isError) {
                if (isError) {
                    m_out << Colors::Red << line << Colors::Reset << Qt::endl;
                } else {
                    m_out << line << Qt::endl;
                }
            };

            if (testIds.isEmpty()) {
                OrchestraManager::instance().testRunAll(outputCb, [this, &done, &result](const TaskResult& r) {
                    done = true;
                    result = r.success ? 0 : 1;
                    printResult(r);
                });
            } else {
                OrchestraManager::instance().testRun(testIds, outputCb, [this, &done, &result](const TaskResult& r) {
                    done = true;
                    result = r.success ? 0 : 1;
                    printResult(r);
                });
            }
        }
        else if (subcmd == "coverage") {
            OrchestraManager::instance().testCoverage([this, &done, &result](const TaskResult& r) {
                done = true;
                result = r.success ? 0 : 1;
                printResult(r);
            });
        }
        else {
            m_out << "Unknown test command: " << subcmd << Qt::endl;
            done = true;
            result = 1;
        }

        waitForCompletion(done);
        return result;
    }

    int handleDiagCommand(const QStringList& args) {
        if (args.isEmpty()) {
            m_out << "Usage: diag [run|info|status]" << Qt::endl;
            return 1;
        }

        QString subcmd = args[0].toLower();

        if (subcmd == "run") {
            bool done = false;
            int result = 0;
            OrchestraManager::instance().runDiagnostics([this, &done, &result](const TaskResult& r) {
                done = true;
                result = r.success ? 0 : 1;
                if (m_jsonOutput) {
                    m_out << QJsonDocument(r.data).toJson() << Qt::endl;
                } else {
                    m_out << Colors::Bold << "Diagnostics Report" << Colors::Reset << Qt::endl;
                    m_out << "==================" << Qt::endl;
                    m_out << Qt::endl;
                    
                    // Health score
                    int healthScore = r.data["healthScore"].toInt(100);
                    int criticalIssues = r.data["criticalIssues"].toInt(0);
                    int warnings = r.data["warnings"].toInt(0);
                    
                    QString scoreColor = (healthScore >= 80) ? Colors::Green : 
                                        (healthScore >= 50) ? Colors::Yellow : Colors::Red;
                    m_out << Colors::Bold << "Health Score: " << Colors::Reset 
                          << scoreColor << healthScore << "/100" << Colors::Reset << Qt::endl;
                    if (criticalIssues > 0) {
                        m_out << Colors::Red << "  Critical Issues: " << criticalIssues << Colors::Reset << Qt::endl;
                    }
                    if (warnings > 0) {
                        m_out << Colors::Yellow << "  Warnings: " << warnings << Colors::Reset << Qt::endl;
                    }
                    m_out << Qt::endl;
                    
                    // Toolchain
                    m_out << Colors::Bold << "Toolchain:" << Colors::Reset << Qt::endl;
                    auto printTool = [this, &r](const QString& name, const QString& versionKey) {
                        bool available = r.data[name].toBool();
                        QString status = available ? Colors::Green + QString("✓") : Colors::Red + QString("✗");
                        m_out << "  " << status << Colors::Reset << " " << name;
                        if (available && r.data.contains(versionKey)) {
                            m_out << ": " << r.data[versionKey].toString();
                        }
                        m_out << Qt::endl;
                    };
                    printTool("git", "gitVersion");
                    printTool("cmake", "cmakeVersion");
                    printTool("msvc", "msvcVersion");
                    printTool("gcc", "gccVersion");
                    printTool("clang", "clangVersion");
                    printTool("python", "pythonVersion");
                    printTool("nodejs", "nodeVersion");
                    printTool("rust", "cargoVersion");
                    m_out << Qt::endl;
                    
                    // System
                    m_out << Colors::Bold << "System:" << Colors::Reset << Qt::endl;
                    m_out << "  Platform: " << r.data["platform"].toString() << Qt::endl;
                    m_out << "  Architecture: " << r.data["architecture"].toString() << Qt::endl;
                    m_out << "  Qt Version: " << r.data["qtVersion"].toString() << Qt::endl;
                    m_out << "  Processors: " << r.data["processorCount"].toInt() << Qt::endl;
                    m_out << Qt::endl;
                    
                    // Memory
                    if (r.data.contains("memory")) {
                        QJsonObject mem = r.data["memory"].toObject();
                        m_out << Colors::Bold << "Memory:" << Colors::Reset << Qt::endl;
                        m_out << "  Total: " << mem["totalPhysicalMB"].toInt() << " MB" << Qt::endl;
                        m_out << "  Available: " << mem["availablePhysicalMB"].toInt() << " MB" << Qt::endl;
                        m_out << "  Load: " << mem["memoryLoadPercent"].toInt() << "%" << Qt::endl;
                        m_out << Qt::endl;
                    }
                    
                    // Subsystems
                    if (r.data.contains("subsystems")) {
                        QJsonObject subs = r.data["subsystems"].toObject();
                        m_out << Colors::Bold << "Subsystems:" << Colors::Reset << Qt::endl;
                        for (auto it = subs.begin(); it != subs.end(); ++it) {
                            bool ok = it.value().toBool();
                            QString status = ok ? Colors::Green + QString("✓") : Colors::Red + QString("✗");
                            m_out << "  " << status << Colors::Reset << " " << it.key() << Qt::endl;
                        }
                    }
                }
            });
            waitForCompletion(done);
            return result;
        }
        else if (subcmd == "info") {
            QJsonObject info = OrchestraManager::instance().getSystemInfo();
            if (m_jsonOutput) {
                m_out << QJsonDocument(info).toJson() << Qt::endl;
            } else {
                m_out << Colors::Bold << "System Information" << Colors::Reset << Qt::endl;
                m_out << "==================" << Qt::endl;
                for (auto it = info.begin(); it != info.end(); ++it) {
                    m_out << it.key() << ": " << it.value().toVariant().toString() << Qt::endl;
                }
            }
            return 0;
        }
        else if (subcmd == "status") {
            QJsonObject status = OrchestraManager::instance().getSubsystemStatus();
            if (m_jsonOutput) {
                m_out << QJsonDocument(status).toJson() << Qt::endl;
            } else {
                m_out << Colors::Bold << "Subsystem Status" << Colors::Reset << Qt::endl;
                m_out << "================" << Qt::endl;
                for (auto it = status.begin(); it != status.end(); ++it) {
                    m_out << it.key() << ": " << it.value().toVariant().toString() << Qt::endl;
                }
            }
            return 0;
        }
        else {
            m_out << "Unknown diag command: " << subcmd << Qt::endl;
            return 1;
        }
    }

    int handleLsCommand(const QStringList& args) {
        QString path = args.isEmpty() ? "." : args[0];
        if (!QDir::isAbsolutePath(path) && OrchestraManager::instance().hasOpenProject()) {
            path = OrchestraManager::instance().currentProjectPath() + "/" + path;
        }

        QDir dir(path);
        if (!dir.exists()) {
            printError("Directory not found: " + path);
            return 1;
        }

        QFileInfoList entries = dir.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot, QDir::DirsFirst | QDir::Name);
        for (const QFileInfo& fi : entries) {
            if (fi.isDir()) {
                m_out << Colors::Blue << fi.fileName() << "/" << Colors::Reset << Qt::endl;
            } else {
                m_out << fi.fileName() << Qt::endl;
            }
        }
        return 0;
    }

    // ================================================================
    // Helper Methods
    // ================================================================

    void waitForCompletion(bool& done) {
        while (!done) {
            QCoreApplication::processEvents(QEventLoop::WaitForMoreEvents, 100);
        }
    }

    void printResult(const TaskResult& r) {
        if (m_jsonOutput) {
            QJsonObject obj;
            obj["success"] = r.success;
            obj["message"] = r.message;
            obj["exitCode"] = r.exitCode;
            obj["durationMs"] = r.durationMs;
            obj["data"] = r.data;
            m_out << QJsonDocument(obj).toJson() << Qt::endl;
        } else {
            if (r.success) {
                printSuccess(r.message);
            } else {
                printError(r.message);
            }
            if (m_verbose && r.durationMs > 0) {
                m_out << Colors::Cyan << "Duration: " << r.durationMs << " ms" << Colors::Reset << Qt::endl;
            }
        }
    }

    void printSuccess(const QString& msg) {
        m_out << Colors::Green << "✓ " << msg << Colors::Reset << Qt::endl;
    }

    void printError(const QString& msg) {
        m_out << Colors::Red << "✗ " << msg << Colors::Reset << Qt::endl;
    }

    int handleLogCommand(const QStringList& args) {
        if (args.isEmpty()) {
            m_out << "Usage: log <command> [args...]" << Qt::endl;
            m_out << "Commands:" << Qt::endl;
            m_out << "  level [TRACE|DEBUG|INFO|WARN|ERROR|FATAL] - Get/set log level" << Qt::endl;
            m_out << "  info <message>   - Log an info message" << Qt::endl;
            m_out << "  warn <message>   - Log a warning message" << Qt::endl;
            m_out << "  error <message>  - Log an error message" << Qt::endl;
            m_out << "  debug <message>  - Log a debug message" << Qt::endl;
            m_out << "  metric <name> <value> - Record a metric" << Qt::endl;
            m_out << "  counter <name> [value] - Increment a counter" << Qt::endl;
            m_out << "  span-start <name> - Start a tracing span" << Qt::endl;
            m_out << "  span-end <id>     - End a tracing span" << Qt::endl;
            return 1;
        }

        QString subcmd = args[0].toLower();
        using namespace RawrXD;

        if (subcmd == "level") {
            if (args.size() > 1) {
                // Set log level
                QString levelStr = args[1].toUpper();
                LogLevel level = LogLevel::INFO;
                if (levelStr == "TRACE") level = LogLevel::TRACE;
                else if (levelStr == "DEBUG") level = LogLevel::DEBUG;
                else if (levelStr == "INFO") level = LogLevel::INFO;
                else if (levelStr == "WARN") level = LogLevel::WARN;
                else if (levelStr == "ERROR") level = LogLevel::ERROR;
                else if (levelStr == "FATAL") level = LogLevel::FATAL;
                else {
                    printError("Unknown log level: " + levelStr);
                    return 1;
                }
                StructuredLogger::instance().initialize(QString(), level);
                printSuccess("Log level set to: " + levelStr);
            } else {
                m_out << "Current log level displayed in logs/rawrxd-production.log" << Qt::endl;
            }
            return 0;
        }
        else if (subcmd == "info") {
            if (args.size() < 2) {
                printError("Usage: log info <message>");
                return 1;
            }
            QString message = args.mid(1).join(" ");
            LOG_INFO(message, QJsonObject{{"source", "cli"}});
            printSuccess("Logged info: " + message);
            return 0;
        }
        else if (subcmd == "warn") {
            if (args.size() < 2) {
                printError("Usage: log warn <message>");
                return 1;
            }
            QString message = args.mid(1).join(" ");
            LOG_WARN(message, QJsonObject{{"source", "cli"}});
            printSuccess("Logged warning: " + message);
            return 0;
        }
        else if (subcmd == "error") {
            if (args.size() < 2) {
                printError("Usage: log error <message>");
                return 1;
            }
            QString message = args.mid(1).join(" ");
            LOG_ERROR(message, QJsonObject{{"source", "cli"}});
            printSuccess("Logged error: " + message);
            return 0;
        }
        else if (subcmd == "debug") {
            if (args.size() < 2) {
                printError("Usage: log debug <message>");
                return 1;
            }
            QString message = args.mid(1).join(" ");
            LOG_DEBUG(message, QJsonObject{{"source", "cli"}});
            printSuccess("Logged debug: " + message);
            return 0;
        }
        else if (subcmd == "metric") {
            if (args.size() < 3) {
                printError("Usage: log metric <name> <value>");
                return 1;
            }
            QString name = args[1];
            bool ok;
            double value = args[2].toDouble(&ok);
            if (!ok) {
                printError("Invalid metric value: " + args[2]);
                return 1;
            }
            LOG_METRIC(name, value, QJsonObject{{"source", "cli"}});
            printSuccess(QString("Recorded metric %1 = %2").arg(name).arg(value));
            return 0;
        }
        else if (subcmd == "counter") {
            if (args.size() < 2) {
                printError("Usage: log counter <name> [value]");
                return 1;
            }
            QString name = args[1];
            int value = (args.size() > 2) ? args[2].toInt() : 1;
            LOG_COUNTER(name, value, QJsonObject{{"source", "cli"}});
            printSuccess(QString("Incremented counter %1 by %2").arg(name).arg(value));
            return 0;
        }
        else if (subcmd == "span-start") {
            if (args.size() < 2) {
                printError("Usage: log span-start <name>");
                return 1;
            }
            QString spanName = args[1];
            START_SPAN(spanName);
            printSuccess("Started span: " + spanName);
            return 0;
        }
        else if (subcmd == "span-end") {
            if (args.size() < 2) {
                printError("Usage: log span-end <id>");
                return 1;
            }
            QString spanId = args[1];
            END_SPAN(spanId);
            printSuccess("Ended span: " + spanId);
            return 0;
        }
        else {
            printError("Unknown log command: " + subcmd);
            return 1;
        }
    }

 */

using namespace RawrXD::CLI;

int main(int argc, const char* argv[]) {
    try {
        CLICompilerEngine compiler;
        
        // Enable colored output by default on Unix-like systems
        #ifndef _WIN32
        compiler.setColorOutput(true);
        #endif
        
        return compiler.run(argc, argv);
    }
    catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << "\n";
        return 1;
    }
    catch (...) {
        std::cerr << "Unknown error occurred\n";
        return 1;
    }
}
