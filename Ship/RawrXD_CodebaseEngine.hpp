// RawrXD_CodebaseEngine.hpp - Intelligent Code Analysis & Refactoring
// Pure C++20 - No Qt Dependencies
// Pattern detection, complexity metrics, bug analysis

#pragma once

#include "RawrXD_JSON.hpp"
#include "RawrXD_SymbolIndex.hpp"
#include <string>
#include <vector>
#include <algorithm>
#include <regex>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

namespace RawrXD {

struct CodeComplexity {
    double cyclomaticComplexity = 0.0;
    int linesOfCode = 0;
    int numberOfFunctions = 0;
    int numberOfClasses = 0;
};

struct RefactoringOpportunity {
    std::string type; // extract_method, inline_function, rename
    std::string description;
    std::string filePath;
    int startLine = 0;
    double confidence = 0.0;
};

struct BugReport {
    std::string bugType; // null_pointer, memory_leak, infinite_loop
    std::string severity; // low, medium, high, critical
    std::string description;
    std::string filePath;
    int lineNumber = 0;
    double confidence = 0.0;
};

class CodebaseEngine {
public:
    void AnalyzeDirectory(const std::string& rootPath) {
        symbolIndex_.IndexDirectory(rootPath);
        for (const auto& entry : fs::recursive_directory_iterator(rootPath)) {
            if (entry.is_regular_file()) {
                auto ext = entry.path().extension().string();
                if (ext == ".cpp" || ext == ".hpp" || ext == ".c" || ext == ".h") {
                    AnalyzeFile(entry.path().string());
                }
            }
        }
    }

    CodeComplexity GetComplexity(const std::string& filePath) {
        auto it = complexityMap_.find(filePath);
        return (it != complexityMap_.end()) ? it->second : CodeComplexity{};
    }

    std::vector<RefactoringOpportunity> GetRefactoringOpportunities() {
        return refactorings_;
    }

    std::vector<BugReport> GetBugReports() {
        return bugs_;
    }

private:
    SymbolIndex symbolIndex_;
    std::map<std::string, CodeComplexity> complexityMap_;
    std::vector<RefactoringOpportunity> refactorings_;
    std::vector<BugReport> bugs_;

    void AnalyzeFile(const std::string& path) {
        std::ifstream f(path);
        if (!f.is_open()) return;

        CodeComplexity metrics;
        std::string line;
        int lineNum = 0;
        int braceDepth = 0;
        
        while (std::getline(f, line)) {
            lineNum++;
            metrics.linesOfCode++;

            // Cyclomatic complexity (rough estimate)
            if (line.find("if ") != std::string::npos || 
                line.find("while ") != std::string::npos ||
                line.find("for ") != std::string::npos) {
                metrics.cyclomaticComplexity += 1.0;
            }

            // Function count
            std::regex funcRe("\\w+\\s+\\w+\\s*\\(");
            if (std::regex_search(line, funcRe)) {
                metrics.numberOfFunctions++;
            }

            // Class count
            if (line.find("class ") != std::string::npos) {
                metrics.numberOfClasses++;
            }

            // Bug detection: Potential null pointer dereference
            if (line.find("->") != std::string::npos && line.find("nullptr") == std::string::npos) {
                bugs_.push_back({
                    "null_pointer",
                    "medium",
                    "Potential null pointer dereference without check",
                    path,
                    lineNum,
                    0.6
                });
            }

            // Refactoring opportunity: Long function (>100 lines)
            if (metrics.linesOfCode > 100 && metrics.numberOfFunctions == 1) {
                refactorings_.push_back({
                    "extract_method",
                    "Function is too long, consider extracting sub-methods",
                    path,
                    1,
                    0.8
                });
            }
        }

        complexityMap_[path] = metrics;
    }
};

} // namespace RawrXD
