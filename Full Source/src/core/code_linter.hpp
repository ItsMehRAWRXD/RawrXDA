// ===========================================================================
// code_linter.hpp — Real-Time IDE Code Linter
// ===========================================================================
// Architecture: C++20, no exceptions, PatchResult pattern
// Purpose: Real-time syntax/semantic linting with incremental analysis
//
// Features:
// - Incremental parsing on file change
// - Multi-threaded analysis (C++, ASM, Python, JavaScript)
// - LSP-compatible diagnostics
// - Inline suggestions (quick fixes, refactoring hints)
// - Performance: <50ms for 10K LOC
// ===========================================================================

#pragma once

#include <cstdint>
#include <cstring>
#include <vector>
#include <string>

// ---------------------------------------------------------------------------
// Diagnostic Severity Levels
// ---------------------------------------------------------------------------

enum class DiagnosticSeverity : uint8_t {
    Error = 0,
    Warning = 1,
    Info = 2,
    Hint = 3
};

// ---------------------------------------------------------------------------
// Diagnostic Category
// ---------------------------------------------------------------------------

enum class DiagnosticCategory : uint8_t {
    Syntax,
    Semantic,
    Style,
    Performance,
    Security,
    Convention,
    Deprecated
};

// ---------------------------------------------------------------------------
// Source Location
// ---------------------------------------------------------------------------

struct SourceRange {
    uint32_t startLine;
    uint32_t startCol;
    uint32_t endLine;
    uint32_t endCol;
};

// ---------------------------------------------------------------------------
// Quick Fix Action
// ---------------------------------------------------------------------------

struct QuickFix {
    const char* title;
    SourceRange range;
    const char* replacement;
};

// ---------------------------------------------------------------------------
// Diagnostic Entry
// ---------------------------------------------------------------------------

struct Diagnostic {
    DiagnosticSeverity severity;
    DiagnosticCategory category;
    SourceRange range;
    const char* message;
    const char* code;           // e.g. "CPP001", "ASM042"
    QuickFix* quickFixes;
    uint32_t quickFixCount;
};

// ---------------------------------------------------------------------------
// Linter Result
// ---------------------------------------------------------------------------

struct LintResult {
    bool success;
    const char* detail;
    Diagnostic* diagnostics;
    uint32_t diagnosticCount;
    uint32_t errorCount;
    uint32_t warningCount;
    uint32_t analysisTimeMs;

    static LintResult ok(const char* msg, Diagnostic* diags, uint32_t count, uint32_t time);
    static LintResult error(const char* msg);
};

// ---------------------------------------------------------------------------
// Language Type
// ---------------------------------------------------------------------------

enum class Language : uint8_t {
    Cpp,
    Asm,
    Python,
    JavaScript,
    TypeScript,
    Rust,
    Unknown
};

// ---------------------------------------------------------------------------
// Linter Configuration
// ---------------------------------------------------------------------------

struct LinterConfig {
    bool enableSemanticAnalysis;
    bool enableStyleChecks;
    bool enablePerformanceHints;
    bool enableSecurityChecks;
    uint32_t maxDiagnostics;
    uint32_t threadCount;
};

// ---------------------------------------------------------------------------
// Code Linter API
// ---------------------------------------------------------------------------

class CodeLinter {
public:
    static CodeLinter& instance();

    // Initialize linter with configuration
    LintResult initialize(const LinterConfig& config);
    
    // Lint a file by path
    LintResult lintFile(const char* filePath);
    
    // Lint in-memory source code
    LintResult lintSource(const char* source, uint32_t length, Language lang, const char* filename);
    
    // Incremental lint (only changed region)
    LintResult lintIncremental(const char* filename, uint32_t startLine, uint32_t endLine);
    
    // Clear diagnostics for a file
    void clearDiagnostics(const char* filename);
    
    // Get diagnostics for a file
    const Diagnostic* getDiagnostics(const char* filename, uint32_t* outCount);
    
    // Apply quick fix
    LintResult applyQuickFix(const char* filename, uint32_t diagnosticIndex, uint32_t fixIndex);
    
    // Statistics
    struct Stats {
        uint64_t totalFilesLinted;
        uint64_t totalDiagnostics;
        uint64_t totalErrors;
        uint64_t totalWarnings;
        uint32_t avgAnalysisTimeMs;
    };
    Stats getStats() const;

private:
    CodeLinter();
    ~CodeLinter();
    CodeLinter(const CodeLinter&) = delete;
    CodeLinter& operator=(const CodeLinter&) = delete;

    struct Impl;
    Impl* m_impl;
};

// ---------------------------------------------------------------------------
// Language-Specific Analyzers
// ---------------------------------------------------------------------------

namespace analyzers {

// C++ Analyzer
LintResult analyzeCpp(const char* source, uint32_t length, std::vector<Diagnostic>& diags);

// ASM Analyzer  
LintResult analyzeAsm(const char* source, uint32_t length, std::vector<Diagnostic>& diags);

// Python Analyzer
LintResult analyzePython(const char* source, uint32_t length, std::vector<Diagnostic>& diags);

// JavaScript/TypeScript Analyzer
LintResult analyzeJavaScript(const char* source, uint32_t length, std::vector<Diagnostic>& diags);

} // namespace analyzers
