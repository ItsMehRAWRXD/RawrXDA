// refactoring_smoke_test.cpp — Minimal smoke test for refactoring engine
#include "refactoring/rawrxd_refactoring.h"
#include <iostream>

struct TestResult {
    bool passed = false;
    std::string name;
    std::string errorMessage;
};

class TestRunner {
private:
    std::vector<TestResult> results_;
    int failedCount_ = 0;

public:
    void runTest(const std::string& name, std::function<TestResult()> testFunc) {
        TestResult result = testFunc();
        result.name = name;
        results_.push_back(result);

        std::cout << name << ": " << (result.passed ? "OK" : "FAIL") << "\n";
        if (!result.passed) {
            failedCount_++;
            std::cout << "  Error: " << result.errorMessage << "\n";
        }
    }

    int summarize() const {
        if (failedCount_ == 0) {
            std::cout << "=== All smoke tests passed ===\n";
            return 0;
        } else {
            std::cout << "=== " << failedCount_ << " smoke test(s) FAILED ===\n";
            std::cout << "Failed tests:\n";
            for (const auto& result : results_) {
                if (!result.passed) {
                    std::cout << "  - " << result.name << "\n";
                }
            }
            return 1;
        }
    }

    int failedCount() const { return failedCount_; }
};

int main() {
    using namespace RawrXD::Refactoring;

    std::cout << "=== RawrXD Refactoring Engine Smoke Test ===\n";
    TestRunner runner;

    // Test RefactoringEngine::rename
    runner.runTest("Rename", []() -> TestResult {
        RefactoringEngine engine;
        RefactoringContext ctx;
        ctx.filePath = "test.cpp";
        ctx.line = 1;
        ctx.column = 8; // Cursor inside "testVar"
        ctx.selectedText = ""; // Exercise cursor-based symbol resolution
        ctx.fileContents["test.cpp"] =
            "int testVar = 42;\n"
            "int z = testVar + 1;\n";

        auto result = engine.rename(ctx, "renamedVar");
        if (!result.success) {
            return {false, "Rename", result.errorMessage};
        }
        // Verify at least one edit was produced
        if (result.edits.empty()) {
            return {false, "Rename", "No edits produced for rename"};
        }
        return {true, "Rename", ""};
    });

    // Test RefactoringEngine::extractVariable
    runner.runTest("Extract Variable", []() -> TestResult {
        RefactoringEngine engine;
        RefactoringContext ctx;
        ctx.filePath = "test.cpp";
        ctx.line = 1;
        ctx.column = 10;
        ctx.selectedText = "42";
        ctx.fileContents["test.cpp"] = "int x = 42;\n";

        auto result = engine.extractVariable(ctx, "newVar");
        if (!result.success) {
            return {false, "Extract Variable", result.errorMessage};
        }
        if (result.edits.empty()) {
            return {false, "Extract Variable", "No edits produced"};
        }
        return {true, "Extract Variable", ""};
    });

    // Test RefactoringEngine::sortIncludes
    runner.runTest("Sort Includes", []() -> TestResult {
        RefactoringEngine engine;
        RefactoringContext ctx;
        ctx.filePath = "test.cpp";
        ctx.fileContents["test.cpp"] =
            "#include \"local.h\"\n"
            "#include <vector>\n"
            "#include <string>\n"
            "int main() { return 0; }\n";

        auto result = engine.sortIncludes(ctx);
        if (!result.success) {
            return {false, "Sort Includes", result.errorMessage};
        }
        if (result.edits.empty()) {
            return {false, "Sort Includes", "No edits produced"};
        }
        return {true, "Sort Includes", ""};
    });

    // Test StaticAnalyzer::analyzeFile
    runner.runTest("Static Analysis", []() -> TestResult {
        StaticAnalyzer analyzer;
        auto result = analyzer.analyzeFile("test.cpp", "int main() { return 0; }");
        // Analysis should complete without crashing; 0 diagnostics is OK for trivial code
        return {true, "Static Analysis", ""};
    });

    // Test StaticAnalyzer::computeMetrics
    runner.runTest("Code Metrics", []() -> TestResult {
        StaticAnalyzer analyzer;
        auto metrics = analyzer.computeMetrics("test.cpp", "int main() { return 0; }");
        if (metrics.linesOfCode == 0) {
            return {false, "Code Metrics", "Expected at least 1 line of code"};
        }
        return {true, "Code Metrics", ""};
    });

    // Test CodeFormatter::format
    runner.runTest("Code Formatting", []() -> TestResult {
        CodeFormatter formatter;
        std::string code = "int main(){return 0;}";
        std::string formatted = formatter.format(code);
        if (formatted.empty()) {
            return {false, "Code Formatting", "Formatter returned empty string"};
        }
        return {true, "Code Formatting", ""};
    });

    // Test CodeNavigator::findDeadCode
    runner.runTest("Code Navigation", []() -> TestResult {
        CodeNavigator navigator;
        auto dead = navigator.findDeadCode();
        // Empty result is OK for empty navigator
        return {true, "Code Navigation", ""};
    });

    // Test MetricsDashboard::getProjectMetrics
    runner.runTest("Metrics Dashboard", []() -> TestResult {
        MetricsDashboard dashboard;
        auto metrics = dashboard.getProjectMetrics();
        // Default-constructed metrics should have 0 files
        return {true, "Metrics Dashboard", ""};
    });

    // Test QuickActionProvider::getActions
    runner.runTest("Quick Actions", []() -> TestResult {
        QuickActionProvider provider;
        RefactoringContext ctx;
        auto actions = provider.getActions(ctx);
        if (actions.empty()) {
            return {false, "Quick Actions", "No quick actions registered"};
        }
        return {true, "Quick Actions", ""};
    });

    // Print summary diagnostics
    std::cout << "\nDiagnostics:\n";
    std::cout << "  Analysis diagnostics: " << StaticAnalyzer().analyzeFile("test.cpp", "int main() { return 0; }").diagnostics.size() << "\n";
    std::cout << "  Lines of code: " << StaticAnalyzer().computeMetrics("test.cpp", "int main() { return 0; }").linesOfCode << "\n";
    std::cout << "  Formatted code length: " << CodeFormatter().format("int main(){return 0;}").size() << "\n";
    std::cout << "  Dead code findings: " << CodeNavigator().findDeadCode().size() << "\n";
    std::cout << "  Project files: " << MetricsDashboard().getProjectMetrics().totalFiles << "\n";
    std::cout << "  Quick actions: " << QuickActionProvider().getActions(RefactoringContext{}).size() << "\n";

    return runner.summarize();
}
