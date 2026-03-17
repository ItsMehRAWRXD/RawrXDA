/**
 * @file powershell_compiler_manager.h
 * @brief PowerShell Compiler Manager for integrating 30+ language compilers into Agentic IDE
 * 
 * This module provides:
 * - Integration of Pure PowerShell Compilers repository (30+ languages)
 * - Zero-dependency compilation using PowerShell scripts only
 * - Consistent lexer→parser→AST→codegen→output pipeline
 * - Language-specific compiler selection and configuration
 * - Compiler execution with timeout and error handling
 * - Compiler output display in IDE terminal/console
 * - Performance metrics and observability
 * 
 * Supported Languages:
 * - C++, Java, Python, Rust, Go, JavaScript, TypeScript, C#, F#, VB.NET
 * - Swift, Kotlin, Scala, Haskell, OCaml, Erlang, Elixir, Lua, Ruby
 * - PHP, Perl, R, MATLAB, Fortran, Assembly, SQL, Shell scripts
 * 
 * @author RawrXD Agent Team
 * @version 1.0.0
 * @date 2025-12-12
 */

#pragma once

#include <QObject>
#include <QString>
#include <QMap>
#include <QProcess>
#include <QTimer>
#include <QJsonObject>
#include <QJsonArray>
#include <QDir>
#include <QFileInfo>
#include <memory>
#include <functional>

/**
 * @struct CompilerInfo
 * @brief Information about a PowerShell compiler
 */
struct CompilerInfo {
    QString name;           ///< Compiler name (e.g., "C++ Compiler")
    QString language;       ///< Programming language (e.g., "cpp")
    QString scriptPath;     ///< Path to PowerShell script
    QString description;    ///< Compiler description
    QString version;        ///< Compiler version
    QString outputType;     ///< Output type (exe, dll, bytecode, etc.)
    bool enabled;           ///< Whether compiler is enabled
    int timeoutMs;          ///< Execution timeout in milliseconds
    QStringList extensions; ///< Supported file extensions
};

/**
 * @struct CompilationResult
 * @brief Result of compilation operation
 */
struct CompilationResult {
    bool success;           ///< Whether compilation succeeded
    QString output;         ///< Compilation output (stdout)
    QString error;          ///< Compilation error (stderr)
    QString outputFile;     ///< Path to generated output file
    int exitCode;          ///< Process exit code
    qint64 durationMs;     ///< Compilation duration in milliseconds
    QString compilerName;   ///< Name of compiler used
};

/**
 * @class PowerShellCompilerManager
 * @brief Manages 30+ PowerShell compilers for the Agentic IDE
 */
class PowerShellCompilerManager : public QObject
{
    Q_OBJECT

public:
    explicit PowerShellCompilerManager(QObject *parent = nullptr);
    ~PowerShellCompilerManager();

    // Compiler Management
    bool loadCompilers(const QString &compilersPath);
    QList<CompilerInfo> getAvailableCompilers() const;
    CompilerInfo getCompilerInfo(const QString &language) const;
    bool isCompilerAvailable(const QString &language) const;

    // Compilation Operations
    CompilationResult compile(const QString &language, const QString &sourceCode, 
                             const QString &outputPath = "", const QJsonObject &options = QJsonObject());
    CompilationResult compileFile(const QString &language, const QString &sourceFilePath, 
                                 const QString &outputPath = "", const QJsonObject &options = QJsonObject());

    // Configuration
    void setDefaultTimeout(int timeoutMs);
    void setCompilerEnabled(const QString &language, bool enabled);
    void setCompilerPath(const QString &language, const QString &scriptPath);

    // Status and Metrics
    int getCompilerCount() const;
    QMap<QString, int> getCompilationStats() const;
    void resetStats();

signals:
    void compilationStarted(const QString &language, const QString &sourceFile);
    void compilationFinished(const CompilationResult &result);
    void compilationError(const QString &language, const QString &error);
    void compilerLoaded(const QString &language, const CompilerInfo &info);
    void compilerUnloaded(const QString &language);

private slots:
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onProcessError(QProcess::ProcessError error);
    void onTimeout();

private:
    // Compiler Management
    QMap<QString, CompilerInfo> m_compilers;
    QString m_compilersBasePath;
    
    // Current Compilation
    QProcess *m_currentProcess;
    QTimer *m_timeoutTimer;
    QString m_currentLanguage;
    QString m_currentSource;
    QString m_currentOutputPath;
    QJsonObject m_currentOptions;
    qint64 m_startTime;
    
    // Statistics
    QMap<QString, int> m_compilationStats;
    int m_totalCompilations;
    int m_successfulCompilations;
    int m_failedCompilations;
    
    // Configuration
    int m_defaultTimeoutMs;
    
    // Internal Methods
    void initialize();
    bool validateCompilerScript(const QString &scriptPath);
    CompilerInfo parseCompilerInfo(const QString &scriptPath);
    QString buildPowerShellCommand(const CompilerInfo &compiler, const QString &source, 
                                  const QString &outputPath, const QJsonObject &options);
    void cleanupProcess();
    void updateStats(bool success);
};

/**
 * @brief Global instance accessor for PowerShellCompilerManager
 * @return Singleton instance
 */
PowerShellCompilerManager* getPowerShellCompilerManager();