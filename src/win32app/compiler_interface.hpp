#pragma once
#include <memory>
#include <chrono>
#include <vector>

#include "solo_compiler_engine.hpp"

namespace RawrXD {
namespace IDE {

class CompilerOutputPanel;
class CompilerWorker;

// ============================================================================
// QT IDE COMPILER INTEGRATION
// ============================================================================

/**
 * @class CompilerInterface
 * @brief Central compiler integration point for QT IDE
 * 
 * Provides:
 * - Non-blocking async compilation
 * - Real-time compilation metrics & progress
 * - Integrated error reporting with file/line navigation
 * - Build cache & incremental compilation
 * - Multiple target support (x86-64, ARM64, etc.)
 */
class CompilerInterface  {

public:
    explicit CompilerInterface( = nullptr);
    virtual ~CompilerInterface();
    
    // ========== CONFIGURATION ==========
    void setCompilationOptions(const Compiler::CompilationOptions& opts);
    const Compiler::CompilationOptions& getCompilationOptions() const { return options_; }
    
    void setTargetArchitecture(Compiler::TargetArchitecture arch);
    void setOptimizationLevel(int level);
    void setDebugSymbols(bool enabled);
    void setVerbose(bool enabled);
    
    // ========== COMPILATION CONTROL ==========
    void compileFile(const std::string& source_file, const std::string& output_file);
    void compileString(const std::string& source, const std::string& output_file);
    void compileCurrentEditor(const std::string& output_file = "");
    void cancelCompilation();
    
    // ========== METRICS & STATUS ==========
    const Compiler::CompilationMetrics& getMetrics() const { return latest_metrics_; }
    int getErrorCount() const { return error_count_; }
    int getWarningCount() const { return warning_count_; }
    bool isCompiling() const { return is_compiling_; }
    
    // ========== EDITOR INTEGRATION ==========
    void setEditorWidget(void* editor) { editor_ = editor; }
    void setOutputPanel(CompilerOutputPanel* panel) { output_panel_ = panel; }
    
    // ========== BUILD CACHE ==========
    void enableBuildCache(bool enabled);
    void clearBuildCache();
    \npublic:\n    // Compilation lifecycle
    void compilationStarted();
    void compilationFinished(bool success);
    void compilationCancelled();
    
    // Progress reporting
    void stageChanged(int stage, const std::string& stage_name);
    void progressUpdated(int percent);
    
    // Metrics reporting
    void metricsUpdated(const Compiler::CompilationMetrics& metrics);
    void timingUpdated(const std::string& stage, int milliseconds);
    
    // Error reporting
    void errorOccurred(int line, int column, const std::string& message, bool is_warning);
    void allErrorsReported(int error_count, int warning_count);
    
    // Editor integration
    void requestNavigateToError(const std::string& file, int line, int column);
    void requestHighlightError(int line, int column, int end_line, int end_column);
\nprivate:\n    void onWorkerFinished(bool success);
    void onWorkerProgress(int percent);
    void onWorkerStageChanged(int stage);
    void onWorkerError(const std::string& error);
    void onWorkerMetricsUpdated();

private:
    Compiler::CompilationOptions options_;
    Compiler::CompilationMetrics latest_metrics_;
    
    void* editor_ = nullptr;
    CompilerOutputPanel* output_panel_ = nullptr;
    
    std::unique_ptr<CompilerWorker> worker_;
    std::unique_ptr<std::thread> worker_thread_;
    
    bool is_compiling_ = false;
    int error_count_ = 0;
    int warning_count_ = 0;
    bool build_cache_enabled_ = true;
    
    std::map<std::string, std::chrono::system_clock::time_point> file_cache_;
    std::map<std::string, std::vector<Compiler::CompilationError>> error_cache_;
};

// ============================================================================
// COMPILER OUTPUT PANEL
// ============================================================================

/**
 * @class CompilerOutputPanel
 * @brief UI panel for displaying compilation results
 * 
 * Features:
 * - Color-coded error/warning display
 * - Quick-click navigation to errors
 * - Performance metrics visualization
 * - Export compilation report
 */
class CompilerOutputPanel {

public:
    explicit CompilerOutputPanel(void* parent = nullptr);
    
    void clearOutput();
    void addError(int line, int column, const std::string& file, 
                  const std::string& message, bool is_warning = false);
    void addMetrics(const Compiler::CompilationMetrics& metrics);
    void displayCompilationReport(const std::string& title, const std::string& summary);
    
    void exportReport(const std::string& filename);
\npublic:\n    void errorClicked(const std::string& file, int line, int column);

private:
    class ErrorItem {
    public:
        int line;
        int column;
        std::string file;
        std::string message;
        bool is_warning;
    };
    
    void* output_text_ = nullptr;
    void* metrics_panel_ = nullptr;
    std::vector<ErrorItem> errors_;
};

// ============================================================================
// COMPILER SETTINGS DIALOG
// ============================================================================

/**
 * @class CompilerSettingsDialog
 * @brief Configuration UI for compiler options
 */
class CompilerSettingsDialog {

public:
    explicit CompilerSettingsDialog(void* parent = nullptr);
    
    Compiler::CompilationOptions getOptions() const;
    void setOptions(const Compiler::CompilationOptions& opts);

private:
    void setupUI();
    void connectSignals();
    
    void* arch_combo_ = nullptr;
    void* os_combo_ = nullptr;
    void* format_combo_ = nullptr;
    void* opt_level_combo_ = nullptr;
    
    void* ok_button_ = nullptr;
    void* cancel_button_ = nullptr;
};

// ============================================================================
// COMPILER WORKER (BACKGROUND THREAD)
// ============================================================================

class CompilerWorker  {

public:
    explicit CompilerWorker(const Compiler::CompilationOptions& opts);
    
    void setSourceFile(const std::string& source);
    void setOutputFile(const std::string& output);
    void setOptions(const Compiler::CompilationOptions& opts);
\npublic:\n    void compile();
    void cancel();
\npublic:\n    void finished(bool success);
    void progressUpdated(int percent);
    void stageChanged(int stage);
    void errorOccurred(const std::string& error);
    void metricsUpdated(const Compiler::CompilationMetrics& metrics);

private:
    std::string source_file_;
    std::string output_file_;
    Compiler::CompilationOptions options_;
    std::unique_ptr<Compiler::SoloCompilerEngine> engine_;
    bool should_cancel_ = false;
};

// ============================================================================
// INTEGRATED COMPILE TOOLBAR
// ============================================================================

/**
 * @class CompileToolbar
 * @brief Integrated toolbar with compile/run/debug controls
 */
class CompileToolbar {

public:
    explicit CompileToolbar(CompilerInterface* compiler, void* parent = nullptr);
    
    void updateMetrics(const Compiler::CompilationMetrics& metrics);
\npublic:\n    void compilePressed();
    void compileAndRunPressed();
    void compileAndDebugPressed();
    void settingsPressed();

private:
    void setupUI();
    
    CompilerInterface* compiler_ = nullptr;
    void* compile_btn_ = nullptr;
    void* compile_run_btn_ = nullptr;
    void* compile_debug_btn_ = nullptr;
    void* settings_btn_ = nullptr;
    
    void* status_label_ = nullptr;
    void* progress_bar_ = nullptr;
};

// ============================================================================
// ERROR NAVIGATOR
// ============================================================================

/**
 * @class ErrorNavigator
 * @brief Navigate between compilation errors
 */
class ErrorNavigator {

public:
    explicit ErrorNavigator(void* parent = nullptr);
    
    void setErrors(const std::vector<Compiler::CompilationError>& errors);
    void clear();
    
    int getTotalErrors() const { return total_errors_; }
    int getCurrentError() const { return current_error_; }
\npublic:\n    void errorSelected(int line, int column);
    void errorSelectedInFile(const std::string& file, int line, int column);
\nprivate:\n    void onPrevError();
    void onNextError();
    void updateDisplay();

private:
    std::vector<Compiler::CompilationError> errors_;
    int current_error_ = -1;
    int total_errors_ = 0;
    void* status_label_ = nullptr;
};

} // namespace IDE
} // namespace RawrXD

#endif // RAWRXD_QT_COMPILER_INTERFACE_HPP

