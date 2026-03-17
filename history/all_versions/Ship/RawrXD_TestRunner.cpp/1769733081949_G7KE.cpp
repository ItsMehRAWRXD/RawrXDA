// ═════════════════════════════════════════════════════════════════════════════
// RawrXD_TestRunner.cpp - Main Test Executable Entry Point
// ═════════════════════════════════════════════════════════════════════════════

#include "RawrXD_IntegrationTest.hpp"
#include "RawrXD_Test_Components.cpp"

#include <windows.h>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <ctime>
#include <codecvt>
#include <locale>

using namespace RawrXD::Testing;

// Convert wide string to regular string
std::string WideToString(const std::wstring& wide) {
    std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
    return converter.to_bytes(wide);
}

// Convert regular string to wide string
std::wstring StringToWide(const std::string& str) {
    std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
    return converter.from_bytes(str);
}

// Command line options
struct TestOptions {
    bool listTests{false};
    bool runAll{true};
    std::wstring categoryFilter;
    std::wstring outputFile;
    bool xmlOutput{false};
    bool stopOnFailure{false};
    int repeatCount{1};
};

TestOptions ParseCommandLine(int argc, wchar_t* argv[]) {
    TestOptions opts;
    
    for (int i = 1; i < argc; ++i) {
        std::wstring arg = argv[i];
        
        if (arg == L"--list" || arg == L"-l") {
            opts.listTests = true;
            opts.runAll = false;
        }
        else if (arg == L"--category" || arg == L"-c") {
            if (i + 1 < argc) {
                opts.categoryFilter = argv[++i];
            }
        }
        else if (arg == L"--output" || arg == L"-o") {
            if (i + 1 < argc) {
                opts.outputFile = argv[++i];
            }
        }
        else if (arg == L"--xml" || arg == L"-x") {
            opts.xmlOutput = true;
        }
        else if (arg == L"--stop-on-failure" || arg == L"-s") {
            opts.stopOnFailure = true;
        }
        else if (arg == L"--repeat" || arg == L"-r") {
            if (i + 1 < argc) {
                opts.repeatCount = std::stoi(argv[++i]);
            }
        }
        else if (arg == L"--help" || arg == L"-h") {
            std::cout << "RawrXD Integration Test Runner\n";
            std::cout << "Usage: RawrXD_TestRunner [options]\n\n";
            std::cout << "Options:\n";
            std::cout << "  -l, --list              List all tests\n";
            std::cout << "  -c, --category <name>   Run only specified category\n";
            std::cout << "  -o, --output <file>     Write results to file\n";
            std::cout << "  -x, --xml               Output in JUnit XML format\n";
            std::cout << "  -s, --stop-on-failure   Stop on first failure\n";
            std::cout << "  -r, --repeat <n>        Repeat tests n times\n";
            std::cout << "  -h, --help              Show this help\n";
            exit(0);
        }
    }
    
    return opts;
}

void ListAllTests() {
    auto& registry = TestRegistry::Instance();
    auto tests = registry.GetAllTests();
    
    std::cout << "\nAvailable Tests (" << tests.size() << " total):\n";
    std::cout << "═══════════════════════════════════════════════════════════════\n";
    
    std::string currentCategory;
    for (const auto& test : tests) {
        if (test.category != currentCategory) {
            currentCategory = test.category;
            std::cout << "\n[" << currentCategory << "]\n";
        }
        std::cout << "  " << std::left << std::setw(40) << test.name 
                  << " - " << test.description << "\n";
    }
    std::cout << "\n";
}

void WriteJUnitXML(const TestExecutor::Results& results, const std::wstring& filename) {
    std::wofstream file(filename);
    
    file << L"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    file << L"<testsuites name=\"RawrXD_Integration_Tests\" "
         << L"tests=\"" << results.totalTests << L"\" "
         << L"failures=\"" << results.failed << L"\" "
         << L"time=\"" << (results.totalDuration.count() / 1000.0) << L"\">\n";
    
    for (const auto& [category, tests] : results.byCategory) {
        file << L"  <testsuite name=\"" << category << L"\" "
             << L"tests=\"" << tests.size() << L"\">\n";
        
        for (const auto& ctx : tests) {
            file << L"    <testcase name=\"" << ctx.name << L"\" "
                 << L"time=\"" << (ctx.duration.count() / 1000.0) << L"\">\n";
            
            if (ctx.result == TestResult::Failed) {
                file << L"      <failure message=\"" << ctx.errorMessage << L"\"/>\n";
            }
            else if (ctx.result == TestResult::Skipped) {
                file << L"      <skipped/>\n";
            }
            
            if (!ctx.metrics.empty()) {
                file << L"      <properties>\n";
                for (const auto& [key, value] : ctx.metrics) {
                    file << L"        <property name=\"" << key << L"\" value=\"" 
                         << value << L"\"/>\n";
                }
                file << L"      </properties>\n";
            }
            
            file << L"    </testcase>\n";
        }
        
        file << L"  </testsuite>\n";
    }
    
    file << L"</testsuites>\n";
}

void WriteTextReport(const TestExecutor::Results& results, const std::wstring& filename) {
    std::wofstream file(filename);
    
    time_t now = time(nullptr);
    struct tm timeinfo;
    localtime_s(&timeinfo, &now);
    wchar_t timestr[64];
    wcsftime(timestr, sizeof(timestr), L"%Y-%m-%d %H:%M:%S", &timeinfo);
    
    file << L"RawrXD Integration Test Report\n";
    file << L"Generated: " << timestr << L"\n";
    file << L"═══════════════════════════════════════════════════════════════\n\n";
    
    file << L"Summary:\n";
    file << L"  Total Tests:  " << results.totalTests << L"\n";
    file << L"  Passed:       " << results.passed << L" (" 
         << (results.totalTests > 0 ? (results.passed * 100 / results.totalTests) : 0) 
         << L"%)\n";
    file << L"  Failed:       " << results.failed << L"\n";
    file << L"  Skipped:      " << results.skipped << L"\n";
    file << L"  Timeouts:     " << results.timeouts << L"\n";
    file << L"  Total Time:   " << results.totalDuration.count() << L"ms\n\n";
    
    for (const auto& [category, tests] : results.byCategory) {
        file << L"Category: " << category << L"\n";
        file << L"─────────────────────────────────────────────────────────────\n";
        
        for (const auto& ctx : tests) {
            const wchar_t* status = L"?";
            switch (ctx.result) {
                case TestResult::Passed: status = L"PASS"; break;
                case TestResult::Failed: status = L"FAIL"; break;
                case TestResult::Skipped: status = L"SKIP"; break;
                case TestResult::Timeout: status = L"TIME"; break;
            }
            
            file << std::left << std::setw(10) << status 
                 << std::setw(40) << ctx.name 
                 << std::right << std::setw(8) << ctx.duration.count() << L"ms\n";
            
            if (!ctx.errorMessage.empty()) {
                file << L"           Error: " << ctx.errorMessage << L"\n";
            }
            
            if (!ctx.metrics.empty()) {
                file << L"           Metrics: ";
                for (const auto& [k, v] : ctx.metrics) {
                    file << k << L"=" << v << L" ";
                }
                file << L"\n";
            }
        }
        file << L"\n";
    }
}

int wmain(int argc, wchar_t* argv[]) {
    try {
        auto opts = ParseCommandLine(argc, argv);
        
        if (opts.listTests) {
            ListAllTests();
            return 0;
        }
        
        std::cout << "\n";
        std::cout << "╔═══════════════════════════════════════════════════════════════╗\n";
        std::cout << "║     RawrXD Integration Test Suite                             ║\n";
        std::cout << "║     Testing 24 Components + Hidden Logic + 44 Tools          ║\n";
        std::cout << "╚═══════════════════════════════════════════════════════════════╝\n\n";
        
        TestExecutor executor;
        executor.SetStopOnFailure(opts.stopOnFailure);
        
        TestExecutor::Results finalResults;
        finalResults.totalTests = 0;
        finalResults.passed = 0;
        finalResults.failed = 0;
        finalResults.skipped = 0;
        finalResults.timeouts = 0;
        
        for (int run = 0; run < opts.repeatCount; ++run) {
            if (opts.repeatCount > 1) {
                std::cout << "\n=== Run " << (run + 1) << "/" << opts.repeatCount << " ===\n\n";
            }
            
            TestExecutor::Results results;
            
            if (!opts.categoryFilter.empty()) {
                results = executor.RunCategory(opts.categoryFilter);
            } else {
                results = executor.RunAllTests();
            }
            
            finalResults.totalTests += results.totalTests;
            finalResults.passed += results.passed;
            finalResults.failed += results.failed;
            finalResults.skipped += results.skipped;
            finalResults.timeouts += results.timeouts;
            finalResults.totalDuration += results.totalDuration;
            
            for (const auto& [cat, tests] : results.byCategory) {
                for (const auto& test : tests) {
                    finalResults.byCategory[cat].push_back(test);
                }
            }
            
            for (const auto& failure : results.failures) {
                finalResults.failures.push_back(failure);
            }
        }
        
        if (!opts.outputFile.empty()) {
            if (opts.xmlOutput) {
                WriteJUnitXML(finalResults, opts.outputFile);
            } else {
                WriteTextReport(finalResults, opts.outputFile);
            }
            std::cout << "\nResults written to: " << opts.outputFile << "\n";
        }
        
        return (finalResults.failed > 0 || finalResults.timeouts > 0) ? 1 : 0;
    }
    catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << "\n";
        return 1;
    }
    catch (...) {
        std::cerr << "Fatal error: unknown exception\n";
        return 1;
    }
}
