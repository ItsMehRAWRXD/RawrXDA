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

// Command line options
struct TestOptions {
    bool listTests{false};
    bool runAll{true};
    std::string categoryFilter;
    std::string outputFile;
    bool xmlOutput{false};
    bool stopOnFailure{false};
    int repeatCount{1};
};

TestOptions ParseCommandLine(int argc, wchar_t* argv[]) {
    TestOptions opts;
    
    for (int i = 1; i < argc; ++i) {
        std::string arg = WideToString(argv[i]);
        
        if (arg == "--list" || arg == "-l") {
            opts.listTests = true;
            opts.runAll = false;
        }
        else if (arg == "--category" || arg == "-c") {
            if (i + 1 < argc) {
                opts.categoryFilter = WideToString(argv[++i]);
            }
        }
        else if (arg == "--output" || arg == "-o") {
            if (i + 1 < argc) {
                opts.outputFile = WideToString(argv[++i]);
            }
        }
        else if (arg == "--xml" || arg == "-x") {
            opts.xmlOutput = true;
        }
        else if (arg == "--stop-on-failure" || arg == "-s") {
            opts.stopOnFailure = true;
        }
        else if (arg == "--repeat" || arg == "-r") {
            if (i + 1 < argc) {
                opts.repeatCount = std::stoi(WideToString(argv[++i]));
            }
        }
        else if (arg == "--help" || arg == "-h") {
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

void WriteJUnitXML(const TestExecutor::Results& results, const std::string& filename) {
    std::ofstream file(filename);
    
    file << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    file << "<testsuites name=\"RawrXD_Integration_Tests\" "
         << "tests=\"" << results.totalTests << "\" "
         << "failures=\"" << results.failed << "\" "
         << "time=\"" << (results.totalDuration.count() / 1000.0) << "\">\n";
    
    for (const auto& [category, tests] : results.byCategory) {
        file << "  <testsuite name=\"" << category << "\" "
             << "tests=\"" << tests.size() << "\">\n";
        
        for (const auto& ctx : tests) {
            file << "    <testcase name=\"" << ctx.name << "\" "
                 << "time=\"" << (ctx.duration.count() / 1000.0) << "\">\n";
            
            if (ctx.result == TestResult::Failed) {
                file << "      <failure message=\"" << ctx.errorMessage << "\"/>\n";
            }
            else if (ctx.result == TestResult::Skipped) {
                file << "      <skipped/>\n";
            }
            
            if (!ctx.metrics.empty()) {
                file << "      <properties>\n";
                for (const auto& [key, value] : ctx.metrics) {
                    file << "        <property name=\"" << key << "\" value=\"" 
                         << value << "\"/>\n";
                }
                file << "      </properties>\n";
            }
            
            file << "    </testcase>\n";
        }
        
        file << "  </testsuite>\n";
    }
    
    file << "</testsuites>\n";
}

void WriteTextReport(const TestExecutor::Results& results, const std::string& filename) {
    std::ofstream file(filename);
    
    time_t now = time(nullptr);
    struct tm timeinfo;
    localtime_s(&timeinfo, &now);
    char timestr[64];
    strftime(timestr, sizeof(timestr), "%Y-%m-%d %H:%M:%S", &timeinfo);
    
    file << "RawrXD Integration Test Report\n";
    file << "Generated: " << timestr << "\n";
    file << "═══════════════════════════════════════════════════════════════\n\n";
    
    file << "Summary:\n";
    file << "  Total Tests:  " << results.totalTests << "\n";
    file << "  Passed:       " << results.passed << " (" 
         << (results.totalTests > 0 ? (results.passed * 100 / results.totalTests) : 0) 
         << "%)\n";
    file << "  Failed:       " << results.failed << "\n";
    file << "  Skipped:      " << results.skipped << "\n";
    file << "  Timeouts:     " << results.timeouts << "\n";
    file << "  Total Time:   " << results.totalDuration.count() << "ms\n\n";
    
    for (const auto& [category, tests] : results.byCategory) {
        file << "Category: " << category << "\n";
        file << "─────────────────────────────────────────────────────────────\n";
        
        for (const auto& ctx : tests) {
            const char* status = "?";
            switch (ctx.result) {
                case TestResult::Passed: status = "PASS"; break;
                case TestResult::Failed: status = "FAIL"; break;
                case TestResult::Skipped: status = "SKIP"; break;
                case TestResult::Timeout: status = "TIME"; break;
            }
            
            file << std::left << std::setw(10) << status 
                 << std::setw(40) << ctx.name 
                 << std::right << std::setw(8) << ctx.duration.count() << "ms\n";
            
            if (!ctx.errorMessage.empty()) {
                file << "           Error: " << ctx.errorMessage << "\n";
            }
            
            if (!ctx.metrics.empty()) {
                file << "           Metrics: ";
                for (const auto& [k, v] : ctx.metrics) {
                    file << k << "=" << v << " ";
                }
                file << "\n";
            }
        }
        file << "\n";
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
