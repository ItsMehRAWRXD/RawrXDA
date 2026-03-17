#pragma once

#include <QObject>
#include <QThread>
#include <QString>
#include <QMap>
#include <QProgressBar>
#include <QPlainTextEdit>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
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
class CompilerInterface : public QObject {
    Q_OBJECT

public:
    explicit CompilerInterface(QObject* parent = nullptr);
    virtual ~CompilerInterface();
    
    // ========== CONFIGURATION ==========
    void setCompilationOptions(const Compiler::CompilationOptions& opts);
    const Compiler::CompilationOptions& getCompilationOptions() const { return options_; }
    
    void setTargetArchitecture(Compiler::TargetArchitecture arch);
    void setOptimizationLevel(int level);
    void setDebugSymbols(bool enabled);
    void setVerbose(bool enabled);
    
    // ========== COMPILATION CONTROL ==========
    void compileFile(const QString& source_file, const QString& output_file);
    void compileString(const QString& source, const QString& output_file);
    void compileCurrentEditor(const QString& output_file = "");
    void cancelCompilation();
    
    // ========== METRICS & STATUS ==========
    const Compiler::CompilationMetrics& getMetrics() const { return latest_metrics_; }
    int getErrorCount() const { return error_count_; }
    int getWarningCount() const { return warning_count_; }
    bool isCompiling() const { return is_compiling_; }
    
    // ========== EDITOR INTEGRATION ==========
    void setEditorWidget(QPlainTextEdit* editor) { editor_ = editor; }
    void setOutputPanel(CompilerOutputPanel* panel) { output_panel_ = panel; }
    
    // ========== BUILD CACHE ==========
    void enableBuildCache(bool enabled);
    void clearBuildCache();
    
signals:
    // Compilation lifecycle
    void compilationStarted();
    void compilationFinished(bool success);
    void compilationCancelled();
    
    // Progress reporting
    void stageChanged(int stage, const QString& stage_name);
    void progressUpdated(int percent);
    
    // Metrics reporting
    void metricsUpdated(const Compiler::CompilationMetrics& metrics);
    void timingUpdated(const QString& stage, int milliseconds);
    
    // Error reporting
    void errorOccurred(int line, int column, const QString& message, bool is_warning);
    void allErrorsReported(int error_count, int warning_count);
    
    // Editor integration
    void requestNavigateToError(const QString& file, int line, int column);
    void requestHighlightError(int line, int column, int end_line, int end_column);

private slots:
    void onWorkerFinished(bool success);
    void onWorkerProgress(int percent);
    void onWorkerStageChanged(int stage);
    void onWorkerError(const QString& error);
    void onWorkerMetricsUpdated();

private:
    Compiler::CompilationOptions options_;
    Compiler::CompilationMetrics latest_metrics_;
    
    QPlainTextEdit* editor_ = nullptr;
    CompilerOutputPanel* output_panel_ = nullptr;
    
    std::unique_ptr<CompilerWorker> worker_;
    std::unique_ptr<QThread> worker_thread_;
    
    bool is_compiling_ = false;
    int error_count_ = 0;
    int warning_count_ = 0;
    bool build_cache_enabled_ = true;
    
    std::map<QString, std::chrono::system_clock::time_point> file_cache_;
    std::map<QString, std::vector<Compiler::CompilationError>> error_cache_;
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
class CompilerOutputPanel : public QWidget {
    Q_OBJECT

public:
    explicit CompilerOutputPanel(QWidget* parent = nullptr);
    
    void clearOutput();
    void addError(int line, int column, const QString& file, 
                  const QString& message, bool is_warning = false);
    void addMetrics(const Compiler::CompilationMetrics& metrics);
    void displayCompilationReport(const QString& title, const QString& summary);
    
    void exportReport(const QString& filename);

signals:
    void errorClicked(const QString& file, int line, int column);

private:
    class ErrorItem {
    public:
        int line;
        int column;
        QString file;
        QString message;
        bool is_warning;
    };
    
    QPlainTextEdit* output_text_ = nullptr;
    QWidget* metrics_panel_ = nullptr;
    std::vector<ErrorItem> errors_;
};

// ============================================================================
// COMPILER SETTINGS DIALOG
// ============================================================================

/**
 * @class CompilerSettingsDialog
 * @brief Configuration UI for compiler options
 */
class CompilerSettingsDialog : public QDialog {
    Q_OBJECT

public:
    explicit CompilerSettingsDialog(QWidget* parent = nullptr);
    
    Compiler::CompilationOptions getOptions() const;
    void setOptions(const Compiler::CompilationOptions& opts);

private:
    void setupUI();
    void connectSignals();
    
    QComboBox* arch_combo_ = nullptr;
    QComboBox* os_combo_ = nullptr;
    QComboBox* format_combo_ = nullptr;
    QComboBox* opt_level_combo_ = nullptr;
    
    QPushButton* ok_button_ = nullptr;
    QPushButton* cancel_button_ = nullptr;
};

// ============================================================================
// COMPILER WORKER (BACKGROUND THREAD)
// ============================================================================

class CompilerWorker : public QObject {
    Q_OBJECT

public:
    explicit CompilerWorker(const Compiler::CompilationOptions& opts);
    
    void setSourceFile(const QString& source);
    void setOutputFile(const QString& output);
    void setOptions(const Compiler::CompilationOptions& opts);

public slots:
    void compile();
    void cancel();

signals:
    void finished(bool success);
    void progressUpdated(int percent);
    void stageChanged(int stage);
    void errorOccurred(const QString& error);
    void metricsUpdated(const Compiler::CompilationMetrics& metrics);

private:
    QString source_file_;
    QString output_file_;
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
class CompileToolbar : public QToolBar {
    Q_OBJECT

public:
    explicit CompileToolbar(CompilerInterface* compiler, QWidget* parent = nullptr);
    
    void updateMetrics(const Compiler::CompilationMetrics& metrics);

signals:
    void compilePressed();
    void compileAndRunPressed();
    void compileAndDebugPressed();
    void settingsPressed();

private:
    void setupUI();
    
    CompilerInterface* compiler_ = nullptr;
    QPushButton* compile_btn_ = nullptr;
    QPushButton* compile_run_btn_ = nullptr;
    QPushButton* compile_debug_btn_ = nullptr;
    QPushButton* settings_btn_ = nullptr;
    
    QLabel* status_label_ = nullptr;
    QProgressBar* progress_bar_ = nullptr;
};

// ============================================================================
// ERROR NAVIGATOR
// ============================================================================

/**
 * @class ErrorNavigator
 * @brief Navigate between compilation errors
 */
class ErrorNavigator : public QWidget {
    Q_OBJECT

public:
    explicit ErrorNavigator(QWidget* parent = nullptr);
    
    void setErrors(const std::vector<Compiler::CompilationError>& errors);
    void clear();
    
    int getTotalErrors() const { return total_errors_; }
    int getCurrentError() const { return current_error_; }

signals:
    void errorSelected(int line, int column);
    void errorSelectedInFile(const QString& file, int line, int column);

private slots:
    void onPrevError();
    void onNextError();
    void updateDisplay();

private:
    std::vector<Compiler::CompilationError> errors_;
    int current_error_ = -1;
    int total_errors_ = 0;
    QLabel* status_label_ = nullptr;
};

} // namespace IDE
} // namespace RawrXD

#endif // RAWRXD_QT_COMPILER_INTERFACE_HPP
