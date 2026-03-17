#pragma once

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <functional>
#include <nlohmann/json.hpp>

// Enterprise-grade symbol information
struct SymbolInfo {
    std::string name;
    std::string type; // "function", "class", "variable", "namespace"
    std::string filePath;
    int lineNumber;
    std::string signature;
    std::string returnType;
    std::vector<std::string> parameters;
    bool isConst = false;
    bool isStatic = false;
    std::vector<std::string> baseClasses;
    nlohmann::json metadata;
};

// Dependency relationship
struct DependencyInfo {
    std::string fromSymbol;
    std::string toSymbol;
    std::string dependencyType; // "import", "inheritance", "usage"
    std::string filePath;
    int lineNumber;
    double confidence;
};

// Architecture pattern detection
struct ArchitecturePattern {
    std::string patternType; // "MVC", "Microservice", "Layered", "Monolith"
    double confidenceScore;
    nlohmann::json evidence;
    std::vector<std::string> affectedFiles;
    nlohmann::json characteristics;
};

// Code complexity metrics
struct CodeComplexity {
    double cyclomaticComplexity = 0.0;
    double cognitiveComplexity = 0.0;
    int linesOfCode = 0;
    int numberOfFunctions = 0;
    int numberOfClasses = 0;
    int numberOfDependencies = 0;
    double duplicationPercentage = 0.0;
    nlohmann::json complexityHotspots;
};

// Refactoring opportunity
struct RefactoringOpportunity {
    std::string type;
    std::string description;
    std::string filePath;
    int startLine;
    int endLine;
    double potentialImprovement;
};

// Bug report
struct BugReport {
    std::string type;
    std::string severity;
    std::string description;
    std::string filePath;
    int lineNumber;
};

// Optimization
struct Optimization {
    std::string type;
    std::string description;
    std::string targetSymbol;
    std::string filePath; // Added
    double estimatedSpeedup;
};

class IntelligentCodebaseEngine {
private:
    std::string projectRoot;
    std::unordered_map<std::string, std::vector<SymbolInfo>> fileSymbols;
    std::unordered_map<std::string, std::vector<std::string>> callGraph;
    
    std::vector<RefactoringOpportunity> refactoringOpportunities;
    std::vector<BugReport> bugReports;
    std::vector<Optimization> optimizations;
    CodeComplexity complexityAnalysis;
    
    // Remote watcher - simplified for now
    std::function<void(const std::string&)> onFileChanged;
    
    // Configuration
    bool enableRealTimeIndexing = true;
    bool enableDeepAnalysis = true;
    bool enablePatternRecognition = true;
    int maxAnalysisDepth = 3;
    double minimumConfidence = 0.7;
    int batchSize = 100;
    
public:
    IntelligentCodebaseEngine();
    ~IntelligentCodebaseEngine();
    
    // Callbacks
    std::function<void(bool)> onAnalysisComplete;
    std::function<void(const std::string&)> onProgressUpdate;
    std::function<void(const BugReport&)> onBugDetected;

    // Core analysis
    bool analyzeEntireCodebase(const std::string& projectPath);
    bool analyzeFile(const std::string& filePath);
    bool analyzeFunction(const std::string& functionName, const std::string& filePath);
    bool analyzeSymbol(const std::string& symbolName);
    
    // Intelligent recommendations
    std::vector<RefactoringOpportunity> discoverRefactoringOpportunities();
    std::vector<BugReport> detectBugs();
    std::vector<Optimization> suggestOptimizations();
    ArchitecturePattern detectArchitecturePattern();
    CodeComplexity analyzeComplexity();
    
    // Symbol intelligence
    SymbolInfo getSymbolInfo(const std::string& symbolName);
    std::vector<SymbolInfo> getSymbolsInFile(const std::string& filePath);
    std::vector<DependencyInfo> getSymbolDependencies(const std::string& symbolName);
    
    // Dependency intelligence
    nlohmann::json getFileDependencies(const std::string& filePath);
    nlohmann::json getDependencyGraph();
    std::vector<std::string> getCircularDependencies();
    
    // Real-time analysis
    bool startRealTimeAnalysis(const std::string& projectPath);
    bool stopRealTimeAnalysis();
    bool updateAnalysis(const std::string& filePath);
    
    // Quality metrics
    double getCodeQualityScore();
    double getTestCoverage(); // Estimated

    // Getters
    const std::vector<RefactoringOpportunity>& getRefactoringOpportunities() const { return refactoringOpportunities; }
    const std::vector<BugReport>& getBugReports() const { return bugReports; }
    const std::vector<Optimization>& getOptimizations() const { return optimizations; }
    
private:
    void performDeepScan();
    void buildCallGraph();
};

