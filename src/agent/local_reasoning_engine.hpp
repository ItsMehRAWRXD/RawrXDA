#pragma once
#include <string>
#include <vector>
#include <map>
#include <set>
#include <memory>
#include <atomic>
#include <mutex>
#include <cstdint>

/**
 * @class LocalReasoningEngine
 * @brief API-free local reasoning using static analysis, pattern matching, and reverse engineering heuristics
 * 
 * Architecture: Pure C++/MASM - NO external API dependencies
 * Expertise: x64 kernel debugging, static analysis, PE/ELF parsing, pattern recognition
 * 
 * Features:
 * - Static code analysis (AST-free, pattern-based)
 * - x64 disassembly recognition (MASM syntax aware)
 * - Symbol analysis via DbgHelp
 * - PE/DLL structure analysis
 * - Heuristic-based bug detection
 * - Rule-based reasoning (expert system)
 * - Multi-pass analysis with refinement
 * - Zero external dependencies
 */
class LocalReasoningEngine {
public:
    enum class AnalysisPass {
        SyntaxScan = 0,           // Quick syntax/structure scan
        SymbolResolution = 1,     // Symbol table analysis
        PatternMatching = 2,      // Known pattern detection
        DataFlowAnalysis = 3,     // Trace variable usage
        ControlFlowAnalysis = 4,  // Branch/loop analysis
        SecurityAudit = 5,        // Vulnerability detection
        PerformanceAnalysis = 6,  // Optimization opportunities
        Synthesis = 7,            // Generate recommendations
        Complete = 8
    };

    struct AnalysisContext {
        std::string sourceCode;       // Code to analyze
        std::string language;         // cpp, asm, c, etc.
        std::string projectRoot;
        bool deepAnalysis = false;    // Enable all passes
        bool includeAssembly = false; // Include x64 disassembly hints
        int maxPasses = 5;            // Multi-pass refinement limit
        
        // Reverse engineering context
        bool kernelMode = false;      // Kernel vs usermode analysis
        bool x64Mode = true;          // x64 vs x86 (default x64)
        std::vector<std::string> suspiciousPatterns;  // Custom patterns to detect
    };

    struct CodeIssue {
        std::string issueType;        // "memory-leak", "race-condition", etc.
        std::string severity;         // "critical", "high", "medium", "low"
        std::string description;
        std::string location;         // File:line or address
        std::string recommendation;
        float confidence;             // 0.0-1.0
        std::vector<std::string> evidence;  // Supporting code snippets
    };

    struct AnalysisResult {
        std::string summary;
        std::vector<CodeIssue> issues;
        std::vector<std::string> patterns;      // Detected patterns
        std::vector<std::string> symbols;       // Resolved symbols
        std::vector<std::string> suggestions;   // Optimization/fix suggestions
        float overallConfidence;
        int passesCompleted;
        long long elapsedMicroseconds;
        
        // Disassembly insights (if x64 analysis enabled)
        std::vector<std::string> asmInstructions;
        std::vector<std::string> hotspots;      // Performance bottlenecks
    };

    explicit LocalReasoningEngine();
    ~LocalReasoningEngine();

    // Core API (mirrors AgenticDeepThinkingEngine)
    AnalysisResult analyze(const AnalysisContext& context);
    
    // Streaming analysis
    void startAnalysis(
        const AnalysisContext& context,
        std::function<void(const CodeIssue&)> onIssueFound,
        std::function<void(float)> onProgressUpdate,
        std::function<void(const std::string&)> onError
    );
    void cancelAnalysis();
    bool isAnalyzing() const { return m_analyzing.load(); }

    // Configuration
    void enableVerboseLogging() { m_verbose = true; }
    void disableVerboseLogging() { m_verbose = false; }
    void setMaxAnalysisTime(int milliseconds) { m_maxAnalysisTime = milliseconds; }
    
    // Knowledge base management
    void addCustomPattern(const std::string& name, const std::string& regex);
    void addSecurityRule(const std::string& name, const std::string& description);
    void clearKnowledgeBase();
    
    // Statistics
    struct AnalysisStats {
        int totalAnalyses = 0;
        int issuesDetected = 0;
        int criticalIssues = 0;
        float avgAnalysisTime = 0.0f;
        float avgConfidence = 0.0f;
        std::map<std::string, int> issueTypeFrequency;
    };
    AnalysisStats getStats() const;
    void resetStats();

private:
    // Multi-pass analysis engine
    AnalysisResult performAnalysisPasses(const AnalysisContext& context);
    CodeIssue executePass(AnalysisPass pass, const AnalysisContext& context, 
                          const std::vector<CodeIssue>& priorIssues);

    // ════════════════════════════════════════════════════════════════════════
    // SYNTAX & STRUCTURE ANALYSIS
    // ════════════════════════════════════════════════════════════════════════
    std::vector<CodeIssue> scanSyntaxStructure(const std::string& code, const std::string& language);
    bool isValidCppSyntax(const std::string& code);
    bool isValidAsmSyntax(const std::string& code);
    std::vector<std::string> extractFunctions(const std::string& code);
    std::vector<std::string> extractStructures(const std::string& code);
    std::map<std::string, std::string> extractVariables(const std::string& code);

    // ════════════════════════════════════════════════════════════════════════
    // PATTERN RECOGNITION (NO API NEEDED)
    // ════════════════════════════════════════════════════════════════════════
    std::vector<CodeIssue> detectCommonPatterns(const std::string& code);
    
    // Memory issues
    bool detectMemoryLeak(const std::string& code, std::string& evidence);
    bool detectBufferOverflow(const std::string& code, std::string& evidence);
    bool detectUseAfterFree(const std::string& code, std::string& evidence);
    bool detectDoubleFree(const std::string& code, std::string& evidence);
    bool detectNullDereference(const std::string& code, std::string& evidence);
    
    // Threading issues
    bool detectRaceCondition(const std::string& code, std::string& evidence);
    bool detectDeadlock(const std::string& code, std::string& evidence);
    bool detectMissingLock(const std::string& code, std::string& evidence);
    
    // Security vulnerabilities
    bool detectSQLInjection(const std::string& code, std::string& evidence);
    bool detectCommandInjection(const std::string& code, std::string& evidence);
    bool detectXSS(const std::string& code, std::string& evidence);
    bool detectIntegerOverflow(const std::string& code, std::string& evidence);
    bool detectFormatStringVuln(const std::string& code, std::string& evidence);
    
    // Performance anti-patterns
    bool detectExpensiveLoop(const std::string& code, std::string& evidence);
    bool detectUnneededAllocation(const std::string& code, std::string& evidence);
    bool detectMissingInline(const std::string& code, std::string& evidence);
    bool detectVirtualCallHotspot(const std::string& code, std::string& evidence);

    // ════════════════════════════════════════════════════════════════════════
    // x64 ASSEMBLY ANALYSIS (MASM EXPERT)
    // ════════════════════════════════════════════════════════════════════════
    std::vector<CodeIssue> analyzeAssembly(const std::string& asmCode);
    bool detectUnalignedAccess(const std::string& asmCode, std::string& evidence);
    bool detectMissingStackAlignment(const std::string& asmCode, std::string& evidence);
    bool detectLeakedRegisters(const std::string& asmCode, std::string& evidence);
    bool detectMissingVolatile(const std::string& asmCode, std::string& evidence);
    bool detectIneffientInstruction(const std::string& asmCode, std::string& evidence);
    
    // x64 calling conventions
    bool validateCallingConvention(const std::string& asmCode, const std::string& convention);
    bool checkShadowSpace(const std::string& asmCode);  // x64 Win64 ABI
    bool checkRegisterPreservation(const std::string& asmCode, const std::vector<std::string>& nonVolatile);
    
    // SIMD/vectorization
    bool detectMissedVectorization(const std::string& code, std::string& suggestion);
    bool detectSuboptimalSIMD(const std::string& asmCode, std::string& suggestion);

    // ════════════════════════════════════════════════════════════════════════
    // SYMBOL & BINARY ANALYSIS (DbgHelp integration)
    // ════════════════════════════════════════════════════════════════════════
    std::vector<std::string> resolveSymbols(const std::string& binaryPath);
    std::map<std::string, uintptr_t> extractExports(const std::string& dllPath);
    std::map<std::string, uintptr_t> extractImports(const std::string& exePath);
    bool analyzePEStructure(const std::string& filePath, std::string& summary);
    
    // ════════════════════════════════════════════════════════════════════════
    // DATA FLOW ANALYSIS (Taint tracking, def-use chains)
    // ════════════════════════════════════════════════════════════════════════
    std::vector<CodeIssue> traceDataFlow(const std::string& code, const std::string& variableName);
    std::vector<std::string> findDefinitions(const std::string& code, const std::string& varName);
    std::vector<std::string> findUses(const std::string& code, const std::string& varName);
    bool isUninitializedUse(const std::string& code, const std::string& varName);
    
    // ════════════════════════════════════════════════════════════════════════
    // CONTROL FLOW ANALYSIS (CFG construction, dominators)
    // ════════════════════════════════════════════════════════════════════════
    struct BasicBlock {
        int id;
        std::vector<std::string> instructions;
        std::vector<int> successors;
        std::vector<int> predecessors;
    };
    std::vector<BasicBlock> buildControlFlowGraph(const std::string& code);
    std::vector<std::vector<int>> findLoops(const std::vector<BasicBlock>& cfg);
    bool detectInfiniteLoop(const std::vector<BasicBlock>& cfg);
    bool detectUnreachableCode(const std::vector<BasicBlock>& cfg);
    
    // ════════════════════════════════════════════════════════════════════════
    // RULE-BASED EXPERT SYSTEM (Hardcoded knowledge)
    // ════════════════════════════════════════════════════════════════════════
    struct Rule {
        std::string name;
        std::string pattern;         // Regex or plain text
        std::string issueType;
        std::string severity;
        std::string recommendation;
        float confidence;
    };
    std::vector<Rule> loadDefaultRules();
    std::vector<CodeIssue> applyRules(const std::string& code, const std::vector<Rule>& rules);
    
    // Hardcoded expert knowledge
    std::vector<Rule> getMemorySafetyRules();
    std::vector<Rule> getThreadSafetyRules();
    std::vector<Rule> getSecurityRules();
    std::vector<Rule> getPerformanceRules();
    std::vector<Rule> getKernelModeRules();
    std::vector<Rule> getx64OptimizationRules();

    // ════════════════════════════════════════════════════════════════════════
    // HEURISTIC SCORING & CONFIDENCE
    // ════════════════════════════════════════════════════════════════════════
    float scoreIssueConfidence(const CodeIssue& issue, const std::string& code);
    float calculateOverallConfidence(const std::vector<CodeIssue>& issues);
    std::string generateSummary(const std::vector<CodeIssue>& issues);
    
    // ════════════════════════════════════════════════════════════════════════
    // UTILITIES
    // ════════════════════════════════════════════════════════════════════════
    std::vector<std::string> tokenize(const std::string& code);
    std::vector<std::string> extractCodeBlocks(const std::string& code, const std::string& blockType);
    bool matchesPattern(const std::string& code, const std::string& pattern);
    std::string extractFunctionBody(const std::string& code, const std::string& funcName);
    int countOccurrences(const std::string& code, const std::string& substring);
    
    // State management
    std::atomic<bool> m_analyzing{false};
    std::thread m_analysisThread;
    std::mutex m_analysisMutex;
    
    // Configuration
    bool m_verbose = false;
    int m_maxAnalysisTime = 10000;  // 10 seconds max
    
    // Knowledge base
    std::vector<Rule> m_customRules;
    std::map<std::string, std::string> m_customPatterns;
    
    // Statistics
    mutable std::mutex m_statsMutex;
    AnalysisStats m_stats;
};
