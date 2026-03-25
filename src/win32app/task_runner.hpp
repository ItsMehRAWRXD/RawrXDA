/**
 * @file task_runner.hpp
 * @brief Abstract task runner for build systems and external tools
 *
 * Provides unified interface for running CMake, MSBuild, GGUF inference,
 * and other long-running tasks with output streaming to UI panels.
 */

#pragma once
#include <functional>
#include <memory>

/**
 * @brief Abstract base class for executable tasks
 *
 * Manages process lifecycle, output capturing, and error handling.
 * Streams stdout/stderr to Output panel and parses errors to Problems panel.
 */
class TaskRunner  {

public:
    /**
     * @brief Task execution status enumeration
     */
    enum class Status {
        Idle,           ///< Not running
        Preparing,      ///< Initializing
        Running,        ///< Process executing
        Paused,         ///< Suspended (resumable)
        Completed,      ///< Finished successfully
        Failed,         ///< Failed with error
        Cancelled,      ///< User-cancelled
        Timeout         ///< Exceeded time limit
    };

    /**
     * @brief Task output chunk
     */
    struct OutputLine {
        std::string text;
        std::string level;  // "info", "warning", "error", "debug"
        int64_t timestamp = 0;
        bool isStderr = false;
    };

    /**
     * @brief Parse result containing diagnostic issues
     */
    struct ParseResult {
        std::string file;
        int line = 0;
        int column = 0;
        std::string severity;       // "error", "warning", "info"
        std::string code;           // Error code (e.g., "ML2005")
        std::string message;
        std::string source;         // "MASM", "CMAKE", "CLANG", etc.
    };

    /**
     * @brief Constructor
     * @param name Task display name
     * @param parent Qt parent
     */
    explicit TaskRunner(const std::string& name);

    /**
     * @brief Destructor
     */
    virtual ~TaskRunner();

    /**
     * @brief Get task name
     */
    std::string name() const { return m_name; }

    /**
     * @brief Get current execution status
     */
    Status status() const { return m_status; }

    /**
     * @brief Get elapsed time in milliseconds
     */
    int64_t elapsedMs() const;

    /**
     * @brief Start task execution
     * @param workingDir Working directory for process
     * @param args Command-line arguments
     * @return true if process started successfully
     */
    virtual bool start(const std::string& workingDir, const std::stringList& args);

    /**
     * @brief Stop/cancel running task
     */
    virtual void stop();

    /**
     * @brief Pause running task (if supported)
     * @return true if pause successful
     */
    virtual bool pause() { return false; }

    /**
     * @brief Resume paused task (if supported)
     * @return true if resume successful
     */
    virtual bool resume() { return false; }

    /**
     * @brief Get command line that was executed
     */
    std::string commandLine() const { return m_commandLine; }

    /**
     * @brief Get all captured output
     */
    std::vector<OutputLine> output() const { return m_output; }

    /**
     * @brief Get exit code (if completed)
     */
    int exitCode() const { return m_exitCode; }

    /**
     * @brief Parse task output for errors/warnings
     * @return Vector of diagnostic issues found
     */
    virtual std::vector<ParseResult> parseOutput();

    /**
     * @brief Get task result as JSON
     * @return JSON with status, output, metrics
     */
    nlohmann::json toJSON() const;
\npublic:\n    /**
     * @brief Emitted when status changes
     */
    void statusChanged(Status newStatus, Status oldStatus);

    /**
     * @brief Emitted when output line received
     */
    void outputLineReceived(const OutputLine& line);

    /**
     * @brief Emitted when diagnostic issue found
     */
    void issueFound(const ParseResult& issue);

    /**
     * @brief Emitted when task completed
     * @param success true if exit code 0
     */
    void finished(bool success);

    /**
     * @brief Emitted periodically with progress
     * @param message Status message
     */
    void progress(const std::string& message);

protected s:
    /**
     * @brief Internal: handle process stdout
     */
    void onProcessStdout();

    /**
     * @brief Internal: handle process stderr
     */
    void onProcessStderr();

    /**
     * @brief Internal: handle process finished
     * @param exitCode Process exit code
     */
    void onProcessFinished(int exitCode);

    /**
     * @brief Internal: handle process error
     * @param error Process error
     */
    void onProcessError(void*::ProcessError error);

protected:
    /**
     * @brief Get executable path (override in subclasses)
     * @return Full path to executable
     */
    virtual std::string executable() const = 0;

    /**
     * @brief Parse output for errors (override in subclasses)
     * @param line Single output line
     * @return ParseResult if this line contains an error, empty result otherwise
     */
    virtual ParseResult parseOutputLine(const std::string& line);

    /**
     * @brief Set status and signal
     */
    void setStatus(Status newStatus);

    /**
     * @brief Add output line
     */
    void addOutput(const std::string& text, const std::string& level, bool isStderr);

    // Member variables
    std::string m_name;
    Status m_status = Status::Idle;
    std::unique_ptr<void*> m_process;
    std::string m_commandLine;
    std::vector<OutputLine> m_output;
    int m_exitCode = -1;
    int64_t m_startTimeMs = 0;
};

/**
 * @brief CMake task runner
 */
class CMakeTaskRunner : public TaskRunner {

public:
    /**
     * @brief Configure and/or build with CMake
     * @param configure true to run cmake configure phase
     * @param build true to run cmake build phase
     */
    CMakeTaskRunner(bool configure = true, bool build = true);

protected:
    std::string executable() const override;
    ParseResult parseOutputLine(const std::string& line) override;

private:
    bool m_configure;
    bool m_build;
};

/**
 * @brief MSBuild (Visual Studio) task runner
 */
class MSBuildTaskRunner : public TaskRunner {

public:
    /**
     * @brief Build with MSBuild
     * @param solutionPath Path to .sln file
     */
    MSBuildTaskRunner(const std::string& solutionPath = std::string());

protected:
    std::string executable() const override;
    ParseResult parseOutputLine(const std::string& line) override;

private:
    std::string m_solutionPath;
};

/**
 * @brief MASM assembler task runner
 */
class MASMTaskRunner : public TaskRunner {

public:
    /**
     * @brief Assemble with ML64/ML
     * @param sourceFile MASM source file (.asm)
     */
    MASMTaskRunner(const std::string& sourceFile = std::string());

protected:
    std::string executable() const override;
    ParseResult parseOutputLine(const std::string& line) override;

private:
    std::string m_sourceFile;
};

/**
 * @brief GGUF inference task runner
 */
class GGUFTaskRunner : public TaskRunner {

public:
    /**
     * @brief Run inference with GGUF model
     * @param modelPath Path to GGUF model file
     * @param prompt Input prompt for inference
     */
    GGUFTaskRunner(const std::string& modelPath = std::string(),
                  const std::string& prompt = std::string());

protected:
    std::string executable() const override;
    ParseResult parseOutputLine(const std::string& line) override;

private:
    std::string m_modelPath;
    std::string m_prompt;
};

/**
 * @brief Custom shell command task runner
 */
class CustomTaskRunner : public TaskRunner {

public:
    /**
     * @brief Execute arbitrary shell command
     * @param executable Command to run (e.g., "powershell.exe")
     * @param args Command arguments
     */
    CustomTaskRunner(const std::string& executable, const std::string& name = "Custom Task");

protected:
    std::string executable() const override;

private:
    std::string m_executable;
};

