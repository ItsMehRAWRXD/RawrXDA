<<<<<<< HEAD
#pragma once

=======
/**
 * \file benchmark_menu_widget.hpp
 * \brief Benchmark menu and test selector — Qt-free C++20/Win32
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

#include <string>
#include <vector>
#include <memory>
#include <cstdint>
#include <thread>
>>>>>>> origin/main
#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
using HWND = void*;
#endif

class BenchmarkRunner;

struct TestResult {
    std::string testName;
    bool passed = false;
    double avgLatencyMs = 0.0;
    double p95LatencyMs = 0.0;
    double successRate = 0.0;
};

<<<<<<< HEAD
class BenchmarkSelector {
public:
    void create(HWND parent, int x, int y, int w, int h);
    std::vector<std::string> getSelectedTests() const;
=======
/**
 * @brief Widget for selecting which benchmarks to run
 */
class BenchmarkSelector {
public:
    BenchmarkSelector() = default;

    // Get selected tests
    std::vector<std::string> getSelectedTests() const;
    
    // Get configuration
>>>>>>> origin/main
    std::string getModelPath() const;
    void setModelPath(const std::string& path);
    bool isGPUEnabled() const;
    bool isVerbose() const;
<<<<<<< HEAD
=======

>>>>>>> origin/main
    void selectAll();
    void deselectAll();

private:
    void setupUI();

<<<<<<< HEAD
    HWND parent_ = nullptr;
    HWND modelCombo_ = nullptr;
    HWND gpuCheckbox_ = nullptr;
    HWND verboseCheckbox_ = nullptr;
    std::vector<HWND> testCheckboxes_;
};

class BenchmarkLogOutput {
public:
=======
    // Test selection checkboxes (HWND handles)
    std::vector<HWND> testCheckboxes_;
    
    // Configuration widgets
    HWND modelCombo_ = nullptr;
    HWND gpuCheckbox_ = nullptr;
    HWND verboseCheckbox_ = nullptr;
};

/**
 * @brief Real-time logging output for benchmark execution
 */
class BenchmarkLogOutput {
public:
    BenchmarkLogOutput() = default;

>>>>>>> origin/main
    enum LogLevel {
        DEBUG = 0,
        INFO = 1,
        SUCCESS = 2,
        WARNING = 3,
<<<<<<< HEAD
        LOG_ERROR = 4
=======
        LOG_ERROR = 4   // was ERROR; renamed to avoid Windows ERROR macro
>>>>>>> origin/main
    };

    void attach(HWND hwnd);
    void logMessage(const std::string& message, LogLevel level = INFO);
    void logProgress(int current, int total);
    void logTestResult(const std::string& testName, bool passed, double latencyMs);
    void clear();

private:
    void formatLog(const std::string& message, LogLevel level);
    std::string levelToString(LogLevel level);
    uint32_t levelToColor(LogLevel level);

<<<<<<< HEAD
    HWND m_hwnd = nullptr;
};

=======
    HWND m_hwnd = nullptr;  // RichEdit or multiline EDIT control
};

/**
 * @brief Progress and results display
 */
>>>>>>> origin/main
class BenchmarkResultsDisplay {
public:
    void create(HWND parent, int x, int y, int w, int h);
    void setTotalTests(int count);
    void updateProgress(int current);
<<<<<<< HEAD
    void addResult(const std::string& testName, bool passed, double avgLatencyMs, double p95LatencyMs, double successRate);
=======
    void addResult(const std::string& testName, bool passed, 
                   double avgLatencyMs, double p95LatencyMs, double successRate);
>>>>>>> origin/main
    void showSummary(int passed, int total, double executionTimeSec);
    void reset();

private:
    void setupUI();

<<<<<<< HEAD
    HWND parent_ = nullptr;
=======
>>>>>>> origin/main
    HWND progressBar_ = nullptr;
    HWND resultsDisplay_ = nullptr;
    int totalTests_ = 0;
    std::vector<TestResult> results_;
};

<<<<<<< HEAD
=======
/**
 * @brief Main benchmark menu integration for the IDE
 */
>>>>>>> origin/main
class BenchmarkMenu {
public:
    explicit BenchmarkMenu(HWND mainWindow = nullptr);
    ~BenchmarkMenu();

    void setMainWindow(HWND mainWindow) { mainWindow_ = mainWindow; }
    void initialize();
<<<<<<< HEAD
    void show();
=======

    /** Show the benchmark UI (opens dialog). */
    void show() { openBenchmarkDialog(); }

>>>>>>> origin/main
    void openBenchmarkDialog();
    void runSelectedBenchmarks();
    void stopBenchmarks();
    void viewBenchmarkResults();

    BenchmarkSelector* ensureSelectorAttached(HWND parent);
    BenchmarkLogOutput* ensureLogAttached(HWND logEdit);
    BenchmarkResultsDisplay* ensureResultsAttached(HWND parent);
    void notifyFinished();

private:
    void createMenu();
    void createDialog();
    void connectHandlers();

    HWND mainWindow_ = nullptr;
    HWND dialogHwnd_ = nullptr;
    BenchmarkSelector* selector_ = nullptr;
    BenchmarkLogOutput* logOutput_ = nullptr;
    BenchmarkResultsDisplay* resultsDisplay_ = nullptr;
    std::unique_ptr<BenchmarkRunner> runner_;
    std::thread runnerThread_;
    std::atomic<bool> runnerActive_{false};
};
