#include "intelligent_codebase_engine.h"
#include <iostream>
#include <filesystem>
#include <fstream>
#include <regex>
#include <algorithm>
#include <thread>
#include <future>

namespace fs = std::filesystem;

IntelligentCodebaseEngine::IntelligentCodebaseEngine() {
}

IntelligentCodebaseEngine::~IntelligentCodebaseEngine() {
    stopRealTimeAnalysis();
}

bool IntelligentCodebaseEngine::analyzeEntireCodebase(const std::string& projectPath) {
    projectRoot = projectPath;
    if (!fs::exists(projectPath)) return false;
    
    if (onProgressUpdate) onProgressUpdate("Starting deep codebase analysis...");
    
    // reset data
    fileSymbols.clear();
    callGraph.clear();
    refactoringOpportunities.clear();
    bugReports.clear();
    optimizations.clear();
    
    int filesProcessed = 0;
    try {
        for (const auto& entry : fs::recursive_directory_iterator(projectPath)) {
            if (entry.is_regular_file()) {
                std::string ext = entry.path().extension().string();
                if (ext == ".cpp" || ext == ".h" || ext == ".hpp" || ext == ".c" || ext == ".py" || ext == ".asm") {
                    analyzeFile(entry.path().string());
                    filesProcessed++;
                }
            }
        }
    } catch (...) {
        if (onProgressUpdate) onProgressUpdate("Error scanning directory structure.");
        return false;
    }
    
    performDeepScan();
    buildCallGraph();
    
    if (onProgressUpdate) onProgressUpdate("Analysis complete. Processed " + std::to_string(filesProcessed) + " files.");
    if (onAnalysisComplete) onAnalysisComplete(true);
    return true;
}

bool IntelligentCodebaseEngine::analyzeFile(const std::string& filePath) {
    std::ifstream file(filePath);
    if (!file.is_open()) return false;
    
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    std::vector<SymbolInfo> symbols;

    // Very basic regex-based parser
    // This is a simplified implementation for demonstration purposes
    // A real implementation would use a proper parser (clang-based or similar)
    
    std::string ext = fs::path(filePath).extension().string();
    
    if (ext == ".cpp" || ext == ".h" || ext == ".hpp") {
        std::regex classRegex(R"(class\s+(\w+))");
        auto words_begin = std::sregex_iterator(content.begin(), content.end(), classRegex);
        auto words_end = std::sregex_iterator();
        
        for (std::sregex_iterator i = words_begin; i != words_end; ++i) {
            std::smatch match = *i;
            SymbolInfo sym;
            sym.name = match[1].str();
            sym.type = "class";
            sym.filePath = filePath;
            // Line number would require counting newlines up to match.position()
            sym.lineNumber = 1 + std::count(content.begin(), content.begin() + match.position(), '\n');
            symbols.push_back(sym);
        }
        
        std::regex funcRegex(R"((\w+)\s*\([^)]*\)\s*\{)");
        words_begin = std::sregex_iterator(content.begin(), content.end(), funcRegex);
        
        for (std::sregex_iterator i = words_begin; i != words_end; ++i) {
            std::smatch match = *i;
            SymbolInfo sym;
            sym.name = match[1].str();
            sym.type = "function";
            sym.filePath = filePath;
            sym.lineNumber = 1 + std::count(content.begin(), content.begin() + match.position(), '\n');
            symbols.push_back(sym);
        }
    } else if (ext == ".py") {
        std::regex defRegex(R"(def\s+(\w+))");
        auto words_begin = std::sregex_iterator(content.begin(), content.end(), defRegex);
        auto words_end = std::sregex_iterator();
        
        for (std::sregex_iterator i = words_begin; i != words_end; ++i) {
            std::smatch match = *i;
            SymbolInfo sym;
            sym.name = match[1].str();
            sym.type = "function";
            sym.filePath = filePath;
            sym.lineNumber = 1 + std::count(content.begin(), content.begin() + match.position(), '\n');
            symbols.push_back(sym);
        }
    }

    fileSymbols[filePath] = symbols;
    
    // Complexity Update
    CodeComplexity fileComplexity;
    fileComplexity.linesOfCode = std::count(content.begin(), content.end(), '\n') + 1;
    fileComplexity.numberOfFunctions = symbols.size(); 
    complexityAnalysis.linesOfCode += fileComplexity.linesOfCode;
    complexityAnalysis.numberOfFunctions += fileComplexity.numberOfFunctions;

    if (onAnalysisComplete) {
        // Just for single file update
         // onAnalysisComplete(true); // Don't trigger full complete on single file
    }
    return true;
}


bool IntelligentCodebaseEngine::analyzeFunction(const std::string& functionName, const std::string& filePath) {
    // Stub
    return true;
}

bool IntelligentCodebaseEngine::analyzeSymbol(const std::string& symbolName) {
    // Stub
    return true;
}

// Intelligent recommendations
std::vector<RefactoringOpportunity> IntelligentCodebaseEngine::discoverRefactoringOpportunities() {
    // Generate some mock opportunities based on complexity
    if (refactoringOpportunities.empty() && complexityAnalysis.linesOfCode > 0) {
        for (const auto& [file, symbols] : fileSymbols) {
            if (symbols.size() > 20) {
                 RefactoringOpportunity opp;
                 opp.type = "Extract Class";
                 opp.description = "File has too many responsibilities";
                 opp.filePath = file;
                 opp.potentialImprovement = 0.8;
                 refactoringOpportunities.push_back(opp);
            }
        }
    }
    return refactoringOpportunities;
}

std::vector<BugReport> IntelligentCodebaseEngine::detectBugs() {
    return bugReports;
}

std::vector<Optimization> IntelligentCodebaseEngine::suggestOptimizations() {
    return optimizations;
}

ArchitecturePattern IntelligentCodebaseEngine::detectArchitecturePattern() {
    ArchitecturePattern pattern;
    pattern.patternType = "Layered";
    pattern.confidenceScore = 0.6;
    return pattern;
}

CodeComplexity IntelligentCodebaseEngine::analyzeComplexity() {
    return complexityAnalysis;
}

// Symbol intelligence
SymbolInfo IntelligentCodebaseEngine::getSymbolInfo(const std::string& symbolName) {
    for (const auto& pair : fileSymbols) {
        for (const auto& sym : pair.second) {
            if (sym.name == symbolName) return sym;
        }
    }
    return SymbolInfo();
}

std::vector<SymbolInfo> IntelligentCodebaseEngine::getSymbolsInFile(const std::string& filePath) {
    if (fileSymbols.count(filePath)) {
        return fileSymbols[filePath];
    }
    return {};
}

std::vector<DependencyInfo> IntelligentCodebaseEngine::getSymbolDependencies(const std::string& symbolName) {
    return {};
}

// Dependency intelligence
nlohmann::json IntelligentCodebaseEngine::getFileDependencies(const std::string& filePath) {
    return nlohmann::json::array();
}

nlohmann::json IntelligentCodebaseEngine::getDependencyGraph() {
    return nlohmann::json::object();
}

std::vector<std::string> IntelligentCodebaseEngine::getCircularDependencies() {
    return {};
}

// Real-time analysis
bool IntelligentCodebaseEngine::startRealTimeAnalysis(const std::string& projectPath) {
    enableRealTimeIndexing = true;
    // In a real app, this would set up a file watcher
    return true;
}

bool IntelligentCodebaseEngine::stopRealTimeAnalysis() {
    enableRealTimeIndexing = false;
    return true;
}

bool IntelligentCodebaseEngine::updateAnalysis(const std::string& filePath) {
    return analyzeFile(filePath);
}

// Quality metrics
double IntelligentCodebaseEngine::getCodeQualityScore() {
    return 85.0; // Mock
}

double IntelligentCodebaseEngine::getTestCoverage() {
    return 0.0; // Mock
}

void IntelligentCodebaseEngine::performDeepScan() {
    if (onProgressUpdate) onProgressUpdate("Performing deep code analysis...");
    
    // Cross-reference symbols
    for (auto& [file, symbols] : fileSymbols) {
        for (const auto& sym : symbols) {
            // Naive duplicate check or similar scanning logic
            if (sym.name.length() < 3) {
                 CodeQualityMetrics metric; // Implicit usage
                 RefactoringOpportunity opp;
                 opp.type = "Naming";
                 opp.description = "Symbol '" + sym.name + "' is too short.";
                 opp.filePath = file;
                 opp.startLine = sym.lineNumber;
                 opp.potentialImprovement = 0.8;
                 refactoringOpportunities.push_back(opp);
            }
        }
    }
    
    // Complexity Analysis Logic
    complexityAnalysis.linesOfCode = 0;
    complexityAnalysis.numberOfFunctions = 0;
    for (const auto& [file, symbols] : fileSymbols) {
       complexityAnalysis.numberOfFunctions += (int)symbols.size();
       // Would need to read file again to count lines normally, assuming 10 LOC per symbol avg for stub
       complexityAnalysis.linesOfCode += (int)symbols.size() * 10;
    }
}

void IntelligentCodebaseEngine::buildCallGraph() {
    if (onProgressUpdate) onProgressUpdate("Building call graph...");
    
    // Simplified: Link functions calling other functions based on name occurrence
    for (auto& [file, symbols] : fileSymbols) {
        std::ifstream f(file);
        if (!f.is_open()) continue;
        std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        
        for (const auto& sym : symbols) {
            if (sym.type == "function") {
                // Check if this function calls others
                for (const auto& [targetFile, targetSymbols] : fileSymbols) {
                    for (const auto& targetSym : targetSymbols) {
                        if (targetSym.type == "function" && targetSym.name != sym.name) {
                            if (content.find(targetSym.name + "(") != std::string::npos) {
                                callGraph[sym.name].push_back(targetSym.name);
                            }
                        }
                    }
                }
            }
        }
    }
}
