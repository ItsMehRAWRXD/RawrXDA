// ═════════════════════════════════════════════════════════════════════════════
// RawrXD_IntegrationTest.hpp - Comprehensive Test Harness
// Tests all 24 components + hidden logic + 44 tools integration
// Zero Qt, Pure Win32/Native C++20
// ═════════════════════════════════════════════════════════════════════════════

#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <process.h>
#include <psapi.h>
#include <dbghelp.h>
#include <shlwapi.h>

#include <iostream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <vector>
#include <map>
#include <functional>
#include <string>
#include <atomic>
#include <mutex>
#include <thread>
#include <random>
#include <algorithm>
#include <numeric>
#include <fstream>
#include <future>
#include <condition_variable>

#pragma comment(lib, "psapi.lib")
#pragma comment(lib, "dbghelp.lib")
#pragma comment(lib, "shlwapi.lib")

namespace RawrXD {
namespace Testing {

// ═════════════════════════════════════════════════════════════════════════════
// TEST FRAMEWORK CORE
// ═════════════════════════════════════════════════════════════════════════════

enum class TestResult {
    Passed,
    Failed,
    Skipped,
    Timeout
};

struct TestContext {
    std::wstring name;
    std::wstring category;
    std::chrono::steady_clock::time_point startTime;
    std::chrono::milliseconds duration{0};
    TestResult result{TestResult::Passed};
    std::wstring errorMessage;
    std::map<std::wstring, std::wstring> metrics;
    size_t memoryBefore{0};
    size_t memoryAfter{0};
    double cpuUsage{0.0};
};

class TestRegistry {
public:
    using TestFunction = std::function<void(TestContext&)>;
    
    struct TestCase {
        std::wstring id;
        std::wstring name;
        std::wstring category;
        std::wstring description;
        TestFunction func;
        bool critical{false};
        std::chrono::milliseconds timeout{30000};
    };

private:
    std::vector<TestCase> tests_;
    mutable std::mutex mutex_;
    std::map<std::wstring, std::vector<TestCase*>> categories_;

public:
    static TestRegistry& Instance() {
        static TestRegistry instance;
        return instance;
    }
    
    void Register(const TestCase& test) {
        std::lock_guard<std::mutex> lock(mutex_);
        tests_.push_back(test);
        categories_[test.category].push_back(&tests_.back());
    }
    
    const std::vector<TestCase>& GetAllTests() const {
        return tests_;
    }
    
    std::vector<TestCase*> GetTestsByCategory(const std::wstring& category) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = categories_.find(category);
        if (it != categories_.end()) {
            return it->second;
        }
        return {};
    }
};

#define RAWRXD_TEST(category, name, description) \
    static void Test_##category##_##name(RawrXD::Testing::TestContext& ctx); \
    struct TestReg_##category##_##name { \
        TestReg_##category##_##name() { \
            RawrXD::Testing::TestRegistry::Instance().Register({ \
                L#category L"_" L#name, \
                L#name, \
                L#category, \
                L#description, \
                Test_##category##_##name \
            }); \
        } \
    } static testReg_##category##_##name; \
    static void Test_##category##_##name(RawrXD::Testing::TestContext& ctx)

// ═════════════════════════════════════════════════════════════════════════════
// PERFORMANCE MONITORING
// ═════════════════════════════════════════════════════════════════════════════

class PerformanceMonitor {
public:
    struct Snapshot {
        size_t workingSet;
        size_t privateBytes;
        size_t handleCount;
        size_t threadCount;
        double cpuPercent;
        FILETIME cpuTime;
    };

private:
    HANDLE process_;
    FILETIME lastCPU_;
    FILETIME lastSysCPU_;
    FILETIME lastUserCPU_;
    int numProcessors_;

public:
    PerformanceMonitor() : process_(GetCurrentProcess()), numProcessors_(0) {
        SYSTEM_INFO sysInfo;
        GetSystemInfo(&sysInfo);
        numProcessors_ = sysInfo.dwNumberOfProcessors;
        
        FILETIME ftime, fsys, fuser;
        GetProcessTimes(process_, &ftime, &ftime, &fsys, &fuser);
        lastCPU_ = ftime;
        lastSysCPU_ = fsys;
        lastUserCPU_ = fuser;
    }
    
    Snapshot Capture() {
        Snapshot snap = {};
        
        PROCESS_MEMORY_COUNTERS pmc;
        if (GetProcessMemoryInfo(process_, &pmc, sizeof(pmc))) {
            snap.workingSet = pmc.WorkingSetSize;
            snap.privateBytes = pmc.PagefileUsage;
        }
        
        FILETIME ftime, fsys, fuser;
        GetProcessTimes(process_, &ftime, &ftime, &fsys, &fuser);
        snap.cpuTime = ftime;
        
        ULARGE_INTEGER now, sys, user;
        memcpy(&now, &ftime, sizeof(FILETIME));
        memcpy(&sys, &fsys, sizeof(FILETIME));
        memcpy(&user, &fuser, sizeof(FILETIME));
        
        ULARGE_INTEGER last, lastSys, lastUser;
        memcpy(&last, &lastCPU_, sizeof(FILETIME));
        memcpy(&lastSys, &lastSysCPU_, sizeof(FILETIME));
        memcpy(&lastUser, &lastUserCPU_, sizeof(FILETIME));
        
        double percent = (sys.QuadPart - lastSys.QuadPart) + 
                        (user.QuadPart - lastUser.QuadPart);
        percent /= (now.QuadPart - last.QuadPart);
        percent /= numProcessors_;
        snap.cpuPercent = percent * 100.0;
        
        lastCPU_ = ftime;
        lastSysCPU_ = fsys;
        lastUserCPU_ = fuser;
        
        return snap;
    }
    
    static size_t GetMemoryUsage() {
        PROCESS_MEMORY_COUNTERS pmc;
        if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
            return pmc.WorkingSetSize;
        }
        return 0;
    }
};

// ═════════════════════════════════════════════════════════════════════════════
// TEST EXECUTION ENGINE
// ═════════════════════════════════════════════════════════════════════════════

class TestExecutor {
public:
    struct Results {
        size_t totalTests{0};
        size_t passed{0};
        size_t failed{0};
        size_t skipped{0};
        size_t timeouts{0};
        std::chrono::milliseconds totalDuration{0};
        std::map<std::wstring, std::vector<TestContext>> byCategory;
        std::vector<TestContext> failures;
    };

private:
    Results results_;
    std::mutex resultsMutex_;
    PerformanceMonitor perfMon_;
    bool stopOnFailure_{false};
    std::atomic<bool> running_{false};

public:
    void SetStopOnFailure(bool stop) { stopOnFailure_ = stop; }
    
    Results RunAllTests() {
        results_ = Results();
        running_ = true;
        
        auto& registry = TestRegistry::Instance();
        auto tests = registry.GetAllTests();
        
        std::wcout << L"═══════════════════════════════════════════════════════════════\n";
        std::wcout << L"RawrXD Integration Test Suite\n";
        std::wcout << L"Total Tests: " << tests.size() << L"\n";
        std::wcout << L"═══════════════════════════════════════════════════════════════\n\n";
        
        for (const auto& test : tests) {
            if (!running_) break;
            ExecuteSingleTest(test);
        }
        
        PrintSummary();
        return results_;
    }
    
    Results RunCategory(const std::wstring& category) {
        auto tests = TestRegistry::Instance().GetTestsByCategory(category);
        
        std::wcout << L"Running category: " << category << L" (" << tests.size() << L" tests)\n";
        
        for (auto* test : tests) {
            if (!running_) break;
            ExecuteSingleTest(*test);
        }
        
        return results_;
    }

private:
    void ExecuteSingleTest(const TestRegistry::TestCase& test) {
        TestContext ctx;
        ctx.name = test.name;
        ctx.category = test.category;
        ctx.startTime = std::chrono::steady_clock::now();
        ctx.memoryBefore = PerformanceMonitor::GetMemoryUsage();
        
        std::atomic<bool> completed{false};
        std::wstring errorStr;
        
        std::thread testThread([&]() {
            __try {
                test.func(ctx);
                completed = true;
            } __except(EXCEPTION_EXECUTE_HANDLER) {
                errorStr = L"SEH Exception (Access Violation)";
                ctx.result = TestResult::Failed;
            }
        });
        
        auto future = std::async(std::launch::async, [&]() {
            testThread.join();
        });
        
        if (future.wait_for(test.timeout) == std::future_status::timeout) {
            TerminateThread(testThread.native_handle(), 1);
            ctx.result = TestResult::Timeout;
            ctx.errorMessage = L"Test timeout after " + 
                              std::to_wstring(test.timeout.count()) + L"ms";
        } else {
            testThread.join();
        }
        
        ctx.duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - ctx.startTime);
        ctx.memoryAfter = PerformanceMonitor::GetMemoryUsage();
        
        {
            std::lock_guard<std::mutex> lock(resultsMutex_);
            results_.totalTests++;
            results_.totalDuration += ctx.duration;
            results_.byCategory[test.category].push_back(ctx);
            
            switch (ctx.result) {
                case TestResult::Passed: results_.passed++; break;
                case TestResult::Failed: 
                    results_.failed++; 
                    results_.failures.push_back(ctx);
                    break;
                case TestResult::Skipped: results_.skipped++; break;
                case TestResult::Timeout: results_.timeouts++; break;
            }
        }
        
        PrintTestResult(test, ctx);
        
        if (test.critical && ctx.result == TestResult::Failed && stopOnFailure_) {
            running_ = false;
        }
    }
    
    void PrintTestResult(const TestRegistry::TestCase& test, const TestContext& ctx) {
        const wchar_t* status = L"[?]";
        WORD color = 7;
        
        switch (ctx.result) {
            case TestResult::Passed: 
                status = L"[PASS]"; 
                color = 10;
                break;
            case TestResult::Failed: 
                status = L"[FAIL]"; 
                color = 12;
                break;
            case TestResult::Skipped: 
                status = L"[SKIP]"; 
                color = 14;
                break;
            case TestResult::Timeout: 
                status = L"[TIME]"; 
                color = 13;
                break;
        }
        
        HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
        SetConsoleTextAttribute(hConsole, color);
        
        std::wcout << status << L" " << std::left << std::setw(40) << test.name 
                  << L" " << std::right << std::setw(6) << ctx.duration.count() << L"ms"
                  << L" | Mem: " << std::setw(6) << (ctx.memoryAfter - ctx.memoryBefore) / 1024 << L"KB"
                  << L" | CPU: " << std::fixed << std::setprecision(1) << ctx.cpuUsage << L"%";
        
        if (!ctx.errorMessage.empty()) {
            std::wcout << L"\n      --> " << ctx.errorMessage;
        }
        
        std::wcout << L"\n";
        
        SetConsoleTextAttribute(hConsole, 7);
    }
    
    void PrintSummary() {
        std::wcout << L"\n═══════════════════════════════════════════════════════════════\n";
        std::wcout << L"Test Summary\n";
        std::wcout << L"═══════════════════════════════════════════════════════════════\n";
        std::wcout << L"Total:  " << results_.totalTests << L"\n";
        std::wcout << L"Passed: " << results_.passed << L" (" 
                  << (results_.totalTests > 0 ? (results_.passed * 100 / results_.totalTests) : 0) 
                  << L"%)\n";
        std::wcout << L"Failed: " << results_.failed << L"\n";
        std::wcout << L"Skipped: " << results_.skipped << L"\n";
        std::wcout << L"Timeout: " << results_.timeouts << L"\n";
        std::wcout << L"Time:   " << results_.totalDuration.count() << L"ms\n";
        std::wcout << L"═══════════════════════════════════════════════════════════════\n";
        
        if (!results_.failures.empty()) {
            std::wcout << L"\nFailed Tests:\n";
            for (const auto& ctx : results_.failures) {
                std::wcout << L"  - " << ctx.category << L"/" << ctx.name 
                          << L": " << ctx.errorMessage << L"\n";
            }
        }
    }
};

// ═════════════════════════════════════════════════════════════════════════════
// ASSERTION HELPERS
// ═════════════════════════════════════════════════════════════════════════════

#define ASSERT_TRUE(expr) \
    if (!(expr)) { \
        ctx.result = RawrXD::Testing::TestResult::Failed; \
        ctx.errorMessage = L"Assertion failed: " L#expr; \
        return; \
    }

#define ASSERT_FALSE(expr) ASSERT_TRUE(!(expr))

#define ASSERT_EQ(expected, actual) \
    if ((expected) != (actual)) { \
        ctx.result = RawrXD::Testing::TestResult::Failed; \
        ctx.errorMessage = L"Assertion failed: expected != actual"; \
        return; \
    }

#define ASSERT_NE(expected, actual) \
    if ((expected) == (actual)) { \
        ctx.result = RawrXD::Testing::TestResult::Failed; \
        ctx.errorMessage = L"Assertion failed: values are equal"; \
        return; \
    }

#define SKIP_TEST(reason) \
    { \
        ctx.result = RawrXD::Testing::TestResult::Skipped; \
        ctx.errorMessage = reason; \
        return; \
    }

} // namespace Testing
} // namespace RawrXD
