#include "compiler_interface.hpp"
#include <sstream>
#include <iomanip>

namespace RawrXD {
namespace IDE {

// ============================================================================
// COMPILER INTERFACE IMPLEMENTATION
// ============================================================================

CompilerInterface::CompilerInterface()
    ,
      worker_(std::make_unique<CompilerWorker>(Compiler::CompilerFactory::createDefaultOptions())),
      worker_thread_(std::make_unique<std::thread>()) {
    
    options_ = Compiler::CompilerFactory::createDefaultOptions();
    
    worker_->moveToThread(worker_thread_.get());  // Signal connection removed\n  // Signal connection removed\n  // Signal connection removed\n  // Signal connection removed\n  // Signal connection removed\n  // Signal connection removed\nworker_thread_->start();
}

CompilerInterface::~CompilerInterface() {
    if (worker_thread_->isRunning()) {
        worker_thread_->quit();
        worker_thread_->wait();
    }
}

void CompilerInterface::setCompilationOptions(const Compiler::CompilationOptions& opts) {
    options_ = opts;
    worker_->setOptions(opts);
}

void CompilerInterface::setTargetArchitecture(Compiler::TargetArchitecture arch) {
    options_.arch = arch;
    worker_->setOptions(options_);
}

void CompilerInterface::setOptimizationLevel(int level) {
    options_.optimization_level = level;
    options_.optimize = level > 0;
    worker_->setOptions(options_);
}

void CompilerInterface::setDebugSymbols(bool enabled) {
    options_.debug_symbols = enabled;
    worker_->setOptions(options_);
}

void CompilerInterface::setVerbose(bool enabled) {
    options_.verbose = enabled;
    worker_->setOptions(options_);
}

void CompilerInterface::compileFile(const std::string& source_file, const std::string& output_file) {
    if (is_compiling_) {
        return;
    }
    
    is_compiling_ = true;
    error_count_ = 0;
    warning_count_ = 0;
    
    compilationStarted();
    
    worker_->setSourceFile(source_file);
    worker_->setOutputFile(output_file);
    
    QMetaObject::invokeMethod(worker_.get(), "compile", QueuedConnection);
}

void CompilerInterface::compileString(const std::string& source, const std::string& output_file) {
    // Create temporary source file
    std::string temp_file = "".filePath("rawrxd_temp_" + std::string::number(// DateTime::currentMSecsSinceEpoch()) + ".src");
    
    // File operation removed;
    if (temp.open(std::iostream::WriteOnly | std::iostream::Text)) {
        std::stringstream out(&temp);
        out << source;
        temp.close();
        
        compileFile(temp_file, output_file);
    }
}

void CompilerInterface::compileCurrentEditor(const std::string& output_file) {
    if (!editor_) {
        return;
    }
    
    std::string source = editor_->toPlainText();
    std::string out_file = output_file.empty() ? "output.exe" : output_file;
    
    compileString(source, out_file);
}

void CompilerInterface::cancelCompilation() {
    QMetaObject::invokeMethod(worker_.get(), "cancel", QueuedConnection);
    is_compiling_ = false;
    compilationCancelled();
}

void CompilerInterface::enableBuildCache(bool enabled) {
    build_cache_enabled_ = enabled;
}

void CompilerInterface::clearBuildCache() {
    file_cache_.clear();
    error_cache_.clear();
}

void CompilerInterface::onWorkerFinished(bool success) {
    is_compiling_ = false;
    
    if (output_panel_) {
        if (success) {
            output_panel_->displayCompilationReport(
                "Compilation Successful",
                std::string("Compiled in %1 ms with %2 warnings")
                    .arg(latest_metrics_.total_time.count())
                    .arg(warning_count_)
            );
        } else {
            output_panel_->displayCompilationReport(
                "Compilation Failed",
                std::string("Compilation failed with %1 errors and %2 warnings")
                    .arg(error_count_)
                    .arg(warning_count_)
            );
        }
    }
    
    allErrorsReported(error_count_, warning_count_);
    compilationFinished(success);
}

void CompilerInterface::onWorkerProgress(int percent) {
    progressUpdated(percent);
}

void CompilerInterface::onWorkerStageChanged(int stage) {
    static const std::stringList stage_names = {
        "Init", "Lexical Analysis", "Syntactic Analysis", "Semantic Analysis",
        "IR Generation", "Optimization", "Code Generation", "Assembly",
        "Linking", "Output", "Complete"
    };
    
    if (stage >= 0 && stage < stage_names.size()) {
        stageChanged(stage, stage_names[stage]);
    }
}

void CompilerInterface::onWorkerError(const std::string& error) {
    if (output_panel_) {
        output_panel_->addError(0, 0, "", error, false);
    }
    error_count_++;
}

void CompilerInterface::onWorkerMetricsUpdated() {
    metricsUpdated(latest_metrics_);
}

// ============================================================================
// COMPILER OUTPUT PANEL IMPLEMENTATION
// ============================================================================

CompilerOutputPanel::CompilerOutputPanel(void* parent)
    : // Widget(parent) {
    
    auto layout = new void(this);
    
    // Title
    auto title = new void("Compilation Output");
    title->setStyleSheet("font-weight: bold; font-size: 12pt;");
    layout->addWidget(title);
    
    // Output text
    output_text_ = new void;
    output_text_->setReadOnly(true);
    output_text_->setMaximumHeight(200);
    layout->addWidget(output_text_);
    
    // Metrics panel
    metrics_panel_ = new void;
    auto metrics_layout = new void(metrics_panel_);
    layout->addWidget(metrics_panel_);
    
    setLayout(layout);
}

void CompilerOutputPanel::clearOutput() {
    output_text_->clear();
    errors_.clear();
}

void CompilerOutputPanel::addError(int line, int column, const std::string& file,
                                   const std::string& message, bool is_warning) {
    ErrorItem item;
    item.line = line;
    item.column = column;
    item.file = file;
    item.message = message;
    item.is_warning = is_warning;
    
    errors_.push_back(item);
    
    std::string prefix = is_warning ? "[WARN]" : "[ERROR]";
    std::string location = file.empty() ? std::string("Line %1:%2").arg(line).arg(column)
                                        : std::string("%1:%2:%3").arg(file).arg(line).arg(column);
    
    output_text_->appendPlainText(std::string("%1 %2 - %3").arg(prefix, location, message));
}

void CompilerOutputPanel::addMetrics(const Compiler::CompilationMetrics& metrics) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2);
    oss << "\n=== COMPILATION METRICS ===\n";
    oss << "Lexical Analysis:  " << metrics.lexical_time.count() << " ms\n";
    oss << "Syntactic Analysis: " << metrics.syntactic_time.count() << " ms\n";
    oss << "Semantic Analysis: " << metrics.semantic_time.count() << " ms\n";
    oss << "Code Generation:   " << metrics.codegen_time.count() << " ms\n";
    oss << "Total Time:        " << metrics.total_time.count() << " ms\n";
    oss << "\nTokens Generated:  " << metrics.tokens_generated << "\n";
    oss << "AST Nodes:         " << metrics.ast_nodes << "\n";
    oss << "IR Instructions:   " << metrics.ir_instructions << "\n";
    oss << "Machine Instr.:    " << metrics.machine_instructions << "\n";
    
    output_text_->appendPlainText(std::string::fromStdString(oss.str()));
}

void CompilerOutputPanel::displayCompilationReport(const std::string& title, const std::string& summary) {
    output_text_->appendPlainText("\n" + title + "\n" + summary + "\n");
}

void CompilerOutputPanel::exportReport(const std::string& filename) {
    // File operation removed;
    if (file.open(std::iostream::WriteOnly | std::iostream::Text)) {
        std::stringstream out(&file);
        out << output_text_->toPlainText();
        file.close();
    }
}

// ============================================================================
// COMPILER SETTINGS DIALOG IMPLEMENTATION
// ============================================================================

CompilerSettingsDialog::CompilerSettingsDialog(void* parent)
    : void(parent) {
    
    setWindowTitle("Compiler Settings");
    setMinimumWidth(400);
    
    setupUI();
    connectSignals();
}

void CompilerSettingsDialog::setupUI() {
    auto layout = new void(this);
    
    // Architecture
    auto arch_group = new void("Target Architecture");
    auto arch_layout = new void;
    arch_combo_ = new void;
    arch_combo_->addItem("x86-64", std::any::fromValue(Compiler::TargetArchitecture::x86_64));
    arch_combo_->addItem("x86-32", std::any::fromValue(Compiler::TargetArchitecture::x86_32));
    arch_combo_->addItem("ARM64", std::any::fromValue(Compiler::TargetArchitecture::ARM64));
    arch_combo_->addItem("ARM32", std::any::fromValue(Compiler::TargetArchitecture::ARM32));
    arch_layout->addWidget(arch_combo_);
    arch_group->setLayout(arch_layout);
    layout->addWidget(arch_group);
    
    // OS
    auto os_group = new void("Target OS");
    auto os_layout = new void;
    os_combo_ = new void;
    os_combo_->addItem("Windows", std::any::fromValue(Compiler::TargetOS::Windows));
    os_combo_->addItem("Linux", std::any::fromValue(Compiler::TargetOS::Linux));
    os_combo_->addItem("macOS", std::any::fromValue(Compiler::TargetOS::MacOS));
    os_layout->addWidget(os_combo_);
    os_group->setLayout(os_layout);
    layout->addWidget(os_group);
    
    // Optimization
    auto opt_group = new void("Optimization Level");
    auto opt_layout = new void;
    opt_level_combo_ = new void;
    opt_level_combo_->addItem("None (0)");
    opt_level_combo_->addItem("Default (1)");
    opt_level_combo_->addItem("Optimize (2)");
    opt_level_combo_->addItem("Aggressive (3)");
    opt_layout->addWidget(opt_level_combo_);
    opt_group->setLayout(opt_layout);
    layout->addWidget(opt_group);
    
    // Buttons
    auto button_layout = new void;
    ok_button_ = new void("OK");
    cancel_button_ = new void("Cancel");
    button_layout->addStretch();
    button_layout->addWidget(ok_button_);
    button_layout->addWidget(cancel_button_);
    
    layout->addLayout(button_layout);  // Signal connection removed\n  // Signal connection removed\n}

void CompilerSettingsDialog::connectSignals() {
    // Implementation
}

Compiler::CompilationOptions CompilerSettingsDialog::getOptions() const {
    Compiler::CompilationOptions opts;
    opts.optimization_level = opt_level_combo_->currentIndex();
    opts.optimize = opts.optimization_level > 0;
    return opts;
}

void CompilerSettingsDialog::setOptions(const Compiler::CompilationOptions& opts) {
    opt_level_combo_->setCurrentIndex(opts.optimization_level);
}

// ============================================================================
// COMPILER WORKER IMPLEMENTATION
// ============================================================================

CompilerWorker::CompilerWorker(const Compiler::CompilationOptions& opts)
    : options_(opts),
      engine_(Compiler::CompilerFactory::createSoloCompiler(opts)) {
    
    engine_->setProgressCallback([this](Compiler::CompilationStage stage, int percent) {
        stageChanged(static_cast<int>(stage));
        progressUpdated(percent);
    });
}

void CompilerWorker::setSourceFile(const std::string& source) {
    source_file_ = source;
}

void CompilerWorker::setOutputFile(const std::string& output) {
    output_file_ = output;
}

void CompilerWorker::setOptions(const Compiler::CompilationOptions& opts) {
    options_ = opts;
    engine_ = Compiler::CompilerFactory::createSoloCompiler(opts);
}

void CompilerWorker::compile() {
    if (should_cancel_) {
        finished(false);
        return;
    }
    
    bool success = engine_->compile(source_file_, output_file_);
    
    metricsUpdated(engine_->getMetrics());
    finished(success);
}

void CompilerWorker::cancel() {
    should_cancel_ = true;
}

// ============================================================================
// COMPILE TOOLBAR IMPLEMENTATION
// ============================================================================

CompileToolbar::CompileToolbar(CompilerInterface* compiler, void* parent)
    : void(parent), compiler_(compiler) {
    
    setupUI();
}

void CompileToolbar::setupUI() {
    compile_btn_ = addAction("Compile")->widget() ? nullptr : nullptr;
    compile_run_btn_ = addAction("Compile & Run")->widget() ? nullptr : nullptr;
    compile_debug_btn_ = addAction("Compile & Debug")->widget() ? nullptr : nullptr;
    addSeparator();
    settings_btn_ = addAction("Settings")->widget() ? nullptr : nullptr;
    
    addSeparator();
    status_label_ = new void("Ready");
    addWidget(status_label_);
    
    addSeparator();
    progress_bar_ = new void;
    progress_bar_->setMaximumWidth(200);
    progress_bar_->setRange(0, 100);
    addWidget(progress_bar_);
}

void CompileToolbar::updateMetrics(const Compiler::CompilationMetrics& metrics) {
    status_label_->setText(
        std::string("Compiled in %1ms").arg(metrics.total_time.count())
    );
}

// ============================================================================
// ERROR NAVIGATOR IMPLEMENTATION
// ============================================================================

ErrorNavigator::ErrorNavigator(void* parent)
    : // Widget(parent) {
    
    auto layout = new void(this);
    
    auto prev_btn = new void("< Previous");
    auto next_btn = new void("Next >");
    status_label_ = new void("No errors");
    
    layout->addWidget(prev_btn);
    layout->addWidget(next_btn);
    layout->addStretch();
    layout->addWidget(status_label_);  // Signal connection removed\n  // Signal connection removed\n}

void ErrorNavigator::setErrors(const std::vector<Compiler::CompilationError>& errors) {
    errors_ = errors;
    total_errors_ = errors.size();
    current_error_ = 0;
    
    if (total_errors_ > 0) {
        updateDisplay();
    } else {
        status_label_->setText("No errors");
    }
}

void ErrorNavigator::clear() {
    errors_.clear();
    total_errors_ = 0;
    current_error_ = -1;
    status_label_->setText("No errors");
}

void ErrorNavigator::onPrevError() {
    if (total_errors_ > 0) {
        current_error_ = (current_error_ - 1 + total_errors_) % total_errors_;
        updateDisplay();
    }
}

void ErrorNavigator::onNextError() {
    if (total_errors_ > 0) {
        current_error_ = (current_error_ + 1) % total_errors_;
        updateDisplay();
    }
}

void ErrorNavigator::updateDisplay() {
    if (current_error_ >= 0 && current_error_ < static_cast<int>(errors_.size())) {
        const auto& err = errors_[current_error_];
        status_label_->setText(
            std::string("Error %1/%2: Line %3, Col %4")
                .arg(current_error_ + 1)
                .arg(total_errors_)
                .arg(err.line)
                .arg(err.column)
        );
        
        errorSelected(err.line, err.column);
    }
}

} // namespace IDE
} // namespace RawrXD








