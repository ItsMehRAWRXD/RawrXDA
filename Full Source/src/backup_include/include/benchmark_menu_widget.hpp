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
#include <atomic>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

class BenchmarkRunner;

// Local TestResult struct for benchmark results display
struct TestResult {
    std::string testName;
    bool passed = false;
    double avgLatencyMs = 0.0;
    double p95LatencyMs = 0.0;
    double successRate = 0.0;
};

/**
 * @brief Widget for selecting which benchmarks to run
 */
class BenchmarkSelector {
public:
    BenchmarkSelector() = default;

    // Win32: create child controls inside a parent window.
    void create(HWND parent, int x, int y, int w, int h);

    // Get selected tests
    std::vector<std::string> getSelectedTests() const;
    
    // Get configuration
    std::string getModelPath() const;
    void setModelPath(const std::string& path);
    bool isGPUEnabled() const;
    bool isVerbose() const;

    void selectAll();
    void deselectAll();

private:
    void setupUI();

    // Test selection checkboxes (HWND handles)
    std::vector<HWND> testCheckboxes_;
    
    // Configuration widgets
    HWND modelCombo_ = nullptr; // Win32: EDIT control holding model path
    HWND gpuCheckbox_ = nullptr;
    HWND verboseCheckbox_ = nullptr;

    HWND parent_ = nullptr;
    HWND group_ = nullptr;
};

/**
 * @brief Real-time logging output for benchmark execution
 */
class BenchmarkLogOutput {
public:
    BenchmarkLogOutput() = default;

    enum LogLevel {
        DEBUG = 0,
        INFO = 1,
        SUCCESS = 2,
        WARNING = 3,
        LOG_ERROR = 4   // was ERROR; renamed to avoid Windows ERROR macro
    };

    void logMessage(const std::string& message, LogLevel level = INFO);
    void logProgress(int current, int total);
    void logTestResult(const std::string& testName, bool passed, double latencyMs);
    void clear();

    void attach(HWND hwndEdit) { m_hwnd = hwndEdit; }

private:
    void formatLog(const std::string& message, LogLevel level);
    std::string levelToString(LogLevel level);
    uint32_t levelToColor(LogLevel level);

    HWND m_hwnd = nullptr;  // RichEdit or multiline EDIT control
};

/**
 * @brief Progress and results display
 */
class BenchmarkResultsDisplay {
public:
    BenchmarkResultsDisplay() = default;

    // Win32: create child controls inside a parent window.
    void create(HWND parent, int x, int y, int w, int h);

    void setTotalTests(int count);
    void updateProgress(int current);
    void addResult(const std::string& testName, bool passed, 
                   double avgLatencyMs, double p95LatencyMs, double successRate);
    void showSummary(int passed, int total, double executionTimeSec);
    void reset();

private:
    void setupUI();

    HWND progressBar_ = nullptr;
    HWND resultsDisplay_ = nullptr;
    HWND parent_ = nullptr;
    int totalTests_ = 0;
    std::vector<TestResult> results_;
};

/**
 * @brief Main benchmark menu integration for the IDE
 */
class BenchmarkMenu {
public:
    explicit BenchmarkMenu(HWND mainWindow = nullptr);
    ~BenchmarkMenu();

    /** Set main window (call before initialize when constructed without one). */
    void setMainWindow(HWND hwnd) { mainWindow_ = hwnd; }

    // Register the benchmark menu
    void initialize();

    /** Show the benchmark UI (opens dialog). */
    void show() { openBenchmarkDialog(); }

    void openBenchmarkDialog();
    void runSelectedBenchmarks();
    void stopBenchmarks();
    void viewBenchmarkResults();

    // Win32 dialog proc helpers (keeps private UI pointers encapsulated).
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
    HMENU benchmarkMenu_ = nullptr;
    
    // Dialog components
    BenchmarkSelector* selector_ = nullptr;
    BenchmarkLogOutput* logOutput_ = nullptr;
    BenchmarkResultsDisplay* resultsDisplay_ = nullptr;
    
    // Benchmark runner thread
    std::unique_ptr<BenchmarkRunner> runner_;
    std::thread runnerThread_;
    std::atomic<bool> runnerActive_{false};
};

// End of benchmark_menu_widget.hpp
