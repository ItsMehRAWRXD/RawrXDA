/**
 * \file benchmark_menu_widget.hpp
 * \brief Benchmark menu and test selector widget for the IDE
 * \author RawrXD Team
 * \date 2025-12-13
 *
 * Provides:
 * - Menu dropdown for benchmark operations
 * - Test selection checkboxes for individual benchmark selection
 * - Real-time logging output with structured formatting
 * - Progress tracking and result display
 */

#pragma once

#include <QString>
#include <QWidget>
#include <QMainWindow>
#include <QMenu>
#include <QAction>
#include <QCheckBox>
#include <QTextEdit>
#include <QPushButton>
#include <QProgressBar>
#include <QThread>
#include <QComboBox>
#include <memory>
#include <vector>
#include <string>

class BenchmarkRunner;

// Local TestResult struct for benchmark results display
struct TestResult {
    QString testName;
    bool passed;
    double avgLatencyMs;
    double p95LatencyMs;
    double successRate;
};

/**
 * @brief Widget for selecting which benchmarks to run
 */
class BenchmarkSelector : public QWidget {
    Q_OBJECT

public:
    explicit BenchmarkSelector(QWidget* parent = nullptr);

    // Get selected tests
    std::vector<std::string> getSelectedTests() const;
    
    // Get configuration
    QString getModelPath() const;
    bool isGPUEnabled() const;
    bool isVerbose() const;

public slots:
    void selectAll();
    void deselectAll();

private:
    void setupUI();

    // Test selection checkboxes
    std::vector<QCheckBox*> testCheckboxes_;
    
    // Configuration widgets
    QComboBox* modelCombo_;
    QCheckBox* gpuCheckbox_;
    QCheckBox* verboseCheckbox_;
};

/**
 * @brief Real-time logging output for benchmark execution
 */
class BenchmarkLogOutput : public QTextEdit {
    Q_OBJECT

public:
    explicit BenchmarkLogOutput(QWidget* parent = nullptr);

    enum LogLevel {
        DEBUG = 0,
        INFO = 1,
        SUCCESS = 2,
        WARNING = 3,
        ERROR = 4
    };

public slots:
    void logMessage(const QString& message, LogLevel level = INFO);
    void logProgress(int current, int total);
    void logTestResult(const QString& testName, bool passed, double latencyMs);
    void clear();

private:
    void formatLog(const QString& message, LogLevel level);
    QString levelToString(LogLevel level);
    QString levelToColor(LogLevel level);
};

/**
 * @brief Progress and results display
 */
class BenchmarkResultsDisplay : public QWidget {
    Q_OBJECT

public:
    explicit BenchmarkResultsDisplay(QWidget* parent = nullptr);

public slots:
    void setTotalTests(int count);
    void updateProgress(int current);
    void addResult(const QString& testName, bool passed, 
                   double avgLatencyMs, double p95LatencyMs, double successRate);
    void showSummary(int passed, int total, double executionTimeSec);
    void reset();

private:
    void setupUI();

    QProgressBar* progressBar_;
    QTextEdit* resultsDisplay_;
    int totalTests_;
    std::vector<TestResult> results_;
};

/**
 * @brief Main benchmark menu integration for the IDE
 */
class BenchmarkMenu : public QObject {
    Q_OBJECT

public:
    explicit BenchmarkMenu(QMainWindow* mainWindow);
    ~BenchmarkMenu();

    // Register the benchmark menu
    void initialize();

public slots:
    void openBenchmarkDialog();
    void runSelectedBenchmarks();
    void stopBenchmarks();
    void viewBenchmarkResults();

private:
    void createMenu();
    void createDialog();
    void connectSignals();

    QMainWindow* mainWindow_;
    QMenu* benchmarkMenu_;
    
    // Dialog components
    BenchmarkSelector* selector_;
    BenchmarkLogOutput* logOutput_;
    BenchmarkResultsDisplay* resultsDisplay_;
    
    // Benchmark runner thread
    std::unique_ptr<BenchmarkRunner> runner_;
    QThread* runnerThread_;
};

// End of benchmark_menu_widget.hpp
