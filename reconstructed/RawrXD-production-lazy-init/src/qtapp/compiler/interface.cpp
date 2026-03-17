#include "compiler_interface.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QCheckBox>
#include <QSpinBox>
#include <QGroupBox>
#include <QTableWidget>
#include <QHeaderView>
#include <QProgressBar>
#include <QScrollArea>
#include <QToolBar>
#include <QAction>
#include <QMessageBox>
#include <QFileDialog>
#include <QDateTime>
#include <QClipboard>
#include <QApplication>
#include <sstream>
#include <iomanip>

namespace RawrXD {
namespace IDE {

// ============================================================================
// COMPILER INTERFACE IMPLEMENTATION
// ============================================================================

CompilerInterface::CompilerInterface(QObject* parent)
    : QObject(parent),
      worker_(std::make_unique<CompilerWorker>(Compiler::CompilerFactory::createDefaultOptions())),
      worker_thread_(std::make_unique<QThread>()) {
    
    options_ = Compiler::CompilerFactory::createDefaultOptions();
    
    worker_->moveToThread(worker_thread_.get());
    
    connect(worker_.get(), &CompilerWorker::finished, this, &CompilerInterface::onWorkerFinished);
    connect(worker_.get(), &CompilerWorker::progressUpdated, this, &CompilerInterface::onWorkerProgress);
    connect(worker_.get(), &CompilerWorker::stageChanged, this, &CompilerInterface::onWorkerStageChanged);
    connect(worker_.get(), &CompilerWorker::errorOccurred, this, &CompilerInterface::onWorkerError);
    connect(worker_.get(), &CompilerWorker::metricsUpdated, this, &CompilerInterface::onWorkerMetricsUpdated);
    
    connect(worker_thread_.get(), &QThread::finished, worker_.get(), &QObject::deleteLater);
    
    worker_thread_->start();
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

void CompilerInterface::compileFile(const QString& source_file, const QString& output_file) {
    if (is_compiling_) {
        return;
    }
    
    is_compiling_ = true;
    error_count_ = 0;
    warning_count_ = 0;
    
    emit compilationStarted();
    
    worker_->setSourceFile(source_file);
    worker_->setOutputFile(output_file);
    
    QMetaObject::invokeMethod(worker_.get(), "compile", Qt::QueuedConnection);
}

void CompilerInterface::compileString(const QString& source, const QString& output_file) {
    // Create temporary source file
    QString temp_file = QDir::temp().filePath("rawrxd_temp_" + QString::number(QDateTime::currentMSecsSinceEpoch()) + ".src");
    
    QFile temp(temp_file);
    if (temp.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&temp);
        out << source;
        temp.close();
        
        compileFile(temp_file, output_file);
    }
}

void CompilerInterface::compileCurrentEditor(const QString& output_file) {
    if (!editor_) {
        return;
    }
    
    QString source = editor_->toPlainText();
    QString out_file = output_file.isEmpty() ? "output.exe" : output_file;
    
    compileString(source, out_file);
}

void CompilerInterface::cancelCompilation() {
    QMetaObject::invokeMethod(worker_.get(), "cancel", Qt::QueuedConnection);
    is_compiling_ = false;
    emit compilationCancelled();
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
                QString("Compiled in %1 ms with %2 warnings")
                    .arg(latest_metrics_.total_time.count())
                    .arg(warning_count_)
            );
        } else {
            output_panel_->displayCompilationReport(
                "Compilation Failed",
                QString("Compilation failed with %1 errors and %2 warnings")
                    .arg(error_count_)
                    .arg(warning_count_)
            );
        }
    }
    
    emit allErrorsReported(error_count_, warning_count_);
    emit compilationFinished(success);
}

void CompilerInterface::onWorkerProgress(int percent) {
    emit progressUpdated(percent);
}

void CompilerInterface::onWorkerStageChanged(int stage) {
    static const QStringList stage_names = {
        "Init", "Lexical Analysis", "Syntactic Analysis", "Semantic Analysis",
        "IR Generation", "Optimization", "Code Generation", "Assembly",
        "Linking", "Output", "Complete"
    };
    
    if (stage >= 0 && stage < stage_names.size()) {
        emit stageChanged(stage, stage_names[stage]);
    }
}

void CompilerInterface::onWorkerError(const QString& error) {
    if (output_panel_) {
        output_panel_->addError(0, 0, "", error, false);
    }
    error_count_++;
}

void CompilerInterface::onWorkerMetricsUpdated() {
    emit metricsUpdated(latest_metrics_);
}

// ============================================================================
// COMPILER OUTPUT PANEL IMPLEMENTATION
// ============================================================================

CompilerOutputPanel::CompilerOutputPanel(QWidget* parent)
    : QWidget(parent) {
    
    auto layout = new QVBoxLayout(this);
    
    // Title
    auto title = new QLabel("Compilation Output");
    title->setStyleSheet("font-weight: bold; font-size: 12pt;");
    layout->addWidget(title);
    
    // Output text
    output_text_ = new QPlainTextEdit;
    output_text_->setReadOnly(true);
    output_text_->setMaximumHeight(200);
    layout->addWidget(output_text_);
    
    // Metrics panel
    metrics_panel_ = new QWidget;
    auto metrics_layout = new QVBoxLayout(metrics_panel_);
    layout->addWidget(metrics_panel_);
    
    setLayout(layout);
}

void CompilerOutputPanel::clearOutput() {
    output_text_->clear();
    errors_.clear();
}

void CompilerOutputPanel::addError(int line, int column, const QString& file,
                                   const QString& message, bool is_warning) {
    ErrorItem item;
    item.line = line;
    item.column = column;
    item.file = file;
    item.message = message;
    item.is_warning = is_warning;
    
    errors_.push_back(item);
    
    QString prefix = is_warning ? "[WARN]" : "[ERROR]";
    QString location = file.isEmpty() ? QString("Line %1:%2").arg(line).arg(column)
                                        : QString("%1:%2:%3").arg(file).arg(line).arg(column);
    
    output_text_->appendPlainText(QString("%1 %2 - %3").arg(prefix, location, message));
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
    
    output_text_->appendPlainText(QString::fromStdString(oss.str()));
}

void CompilerOutputPanel::displayCompilationReport(const QString& title, const QString& summary) {
    output_text_->appendPlainText("\n" + title + "\n" + summary + "\n");
}

void CompilerOutputPanel::exportReport(const QString& filename) {
    QFile file(filename);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&file);
        out << output_text_->toPlainText();
        file.close();
    }
}

// ============================================================================
// COMPILER SETTINGS DIALOG IMPLEMENTATION
// ============================================================================

CompilerSettingsDialog::CompilerSettingsDialog(QWidget* parent)
    : QDialog(parent) {
    
    setWindowTitle("Compiler Settings");
    setMinimumWidth(400);
    
    setupUI();
    connectSignals();
}

void CompilerSettingsDialog::setupUI() {
    auto layout = new QVBoxLayout(this);
    
    // Architecture
    auto arch_group = new QGroupBox("Target Architecture");
    auto arch_layout = new QHBoxLayout;
    arch_combo_ = new QComboBox;
    arch_combo_->addItem("x86-64", QVariant::fromValue(Compiler::TargetArchitecture::x86_64));
    arch_combo_->addItem("x86-32", QVariant::fromValue(Compiler::TargetArchitecture::x86_32));
    arch_combo_->addItem("ARM64", QVariant::fromValue(Compiler::TargetArchitecture::ARM64));
    arch_combo_->addItem("ARM32", QVariant::fromValue(Compiler::TargetArchitecture::ARM32));
    arch_layout->addWidget(arch_combo_);
    arch_group->setLayout(arch_layout);
    layout->addWidget(arch_group);
    
    // OS
    auto os_group = new QGroupBox("Target OS");
    auto os_layout = new QHBoxLayout;
    os_combo_ = new QComboBox;
    os_combo_->addItem("Windows", QVariant::fromValue(Compiler::TargetOS::Windows));
    os_combo_->addItem("Linux", QVariant::fromValue(Compiler::TargetOS::Linux));
    os_combo_->addItem("macOS", QVariant::fromValue(Compiler::TargetOS::MacOS));
    os_layout->addWidget(os_combo_);
    os_group->setLayout(os_layout);
    layout->addWidget(os_group);
    
    // Optimization
    auto opt_group = new QGroupBox("Optimization Level");
    auto opt_layout = new QHBoxLayout;
    opt_level_combo_ = new QComboBox;
    opt_level_combo_->addItem("None (0)");
    opt_level_combo_->addItem("Default (1)");
    opt_level_combo_->addItem("Optimize (2)");
    opt_level_combo_->addItem("Aggressive (3)");
    opt_layout->addWidget(opt_level_combo_);
    opt_group->setLayout(opt_layout);
    layout->addWidget(opt_group);
    
    // Buttons
    auto button_layout = new QHBoxLayout;
    ok_button_ = new QPushButton("OK");
    cancel_button_ = new QPushButton("Cancel");
    button_layout->addStretch();
    button_layout->addWidget(ok_button_);
    button_layout->addWidget(cancel_button_);
    
    layout->addLayout(button_layout);
    
    connect(ok_button_, &QPushButton::clicked, this, &QDialog::accept);
    connect(cancel_button_, &QPushButton::clicked, this, &QDialog::reject);
}

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
        emit stageChanged(static_cast<int>(stage));
        emit progressUpdated(percent);
    });
}

void CompilerWorker::setSourceFile(const QString& source) {
    source_file_ = source;
}

void CompilerWorker::setOutputFile(const QString& output) {
    output_file_ = output;
}

void CompilerWorker::setOptions(const Compiler::CompilationOptions& opts) {
    options_ = opts;
    engine_ = Compiler::CompilerFactory::createSoloCompiler(opts);
}

void CompilerWorker::compile() {
    if (should_cancel_) {
        emit finished(false);
        return;
    }
    
    bool success = engine_->compile(source_file_.toStdString(), output_file_.toStdString());
    
    emit metricsUpdated(engine_->getMetrics());
    emit finished(success);
}

void CompilerWorker::cancel() {
    should_cancel_ = true;
}

// ============================================================================
// COMPILE TOOLBAR IMPLEMENTATION
// ============================================================================

CompileToolbar::CompileToolbar(CompilerInterface* compiler, QWidget* parent)
    : QToolBar(parent), compiler_(compiler) {
    
    setupUI();
}

void CompileToolbar::setupUI() {
    compile_btn_ = addAction("Compile")->widget() ? nullptr : nullptr;
    compile_run_btn_ = addAction("Compile & Run")->widget() ? nullptr : nullptr;
    compile_debug_btn_ = addAction("Compile & Debug")->widget() ? nullptr : nullptr;
    addSeparator();
    settings_btn_ = addAction("Settings")->widget() ? nullptr : nullptr;
    
    addSeparator();
    status_label_ = new QLabel("Ready");
    addWidget(status_label_);
    
    addSeparator();
    progress_bar_ = new QProgressBar;
    progress_bar_->setMaximumWidth(200);
    progress_bar_->setRange(0, 100);
    addWidget(progress_bar_);
}

void CompileToolbar::updateMetrics(const Compiler::CompilationMetrics& metrics) {
    status_label_->setText(
        QString("Compiled in %1ms").arg(metrics.total_time.count())
    );
}

// ============================================================================
// ERROR NAVIGATOR IMPLEMENTATION
// ============================================================================

ErrorNavigator::ErrorNavigator(QWidget* parent)
    : QWidget(parent) {
    
    auto layout = new QHBoxLayout(this);
    
    auto prev_btn = new QPushButton("< Previous");
    auto next_btn = new QPushButton("Next >");
    status_label_ = new QLabel("No errors");
    
    layout->addWidget(prev_btn);
    layout->addWidget(next_btn);
    layout->addStretch();
    layout->addWidget(status_label_);
    
    connect(prev_btn, &QPushButton::clicked, this, &ErrorNavigator::onPrevError);
    connect(next_btn, &QPushButton::clicked, this, &ErrorNavigator::onNextError);
}

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
            QString("Error %1/%2: Line %3, Col %4")
                .arg(current_error_ + 1)
                .arg(total_errors_)
                .arg(err.line)
                .arg(err.column)
        );
        
        emit errorSelected(err.line, err.column);
    }
}

} // namespace IDE
} // namespace RawrXD
