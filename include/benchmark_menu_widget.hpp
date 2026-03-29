#pragma once

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

class BenchmarkSelector {
public:
    void create(HWND parent, int x, int y, int w, int h);
    std::vector<std::string> getSelectedTests() const;
    std::string getModelPath() const;
    void setModelPath(const std::string& path);
    bool isGPUEnabled() const;
    bool isVerbose() const;
    void selectAll();
    void deselectAll();

private:
    void setupUI();

    HWND parent_ = nullptr;
    HWND modelCombo_ = nullptr;
    HWND gpuCheckbox_ = nullptr;
    HWND verboseCheckbox_ = nullptr;
    std::vector<HWND> testCheckboxes_;
};

class BenchmarkLogOutput {
public:
    enum LogLevel {
        DEBUG = 0,
        INFO = 1,
        SUCCESS = 2,
        WARNING = 3,
        LOG_ERROR = 4
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

    HWND m_hwnd = nullptr;
};

class BenchmarkResultsDisplay {
public:
    void create(HWND parent, int x, int y, int w, int h);
    void setTotalTests(int count);
    void updateProgress(int current);
    void addResult(const std::string& testName, bool passed, double avgLatencyMs, double p95LatencyMs, double successRate);
    void showSummary(int passed, int total, double executionTimeSec);
    void reset();

private:
    void setupUI();

    HWND parent_ = nullptr;
    HWND progressBar_ = nullptr;
    HWND resultsDisplay_ = nullptr;
    int totalTests_ = 0;
    std::vector<TestResult> results_;
};

class BenchmarkMenu {
public:
    explicit BenchmarkMenu(HWND mainWindow = nullptr);
    ~BenchmarkMenu();

    void setMainWindow(HWND mainWindow) { mainWindow_ = mainWindow; }
    void initialize();
    void show();
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
