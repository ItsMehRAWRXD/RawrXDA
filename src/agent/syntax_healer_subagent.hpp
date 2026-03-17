// ============================================================================
// syntax_healer_subagent.hpp — Autonomous Syntax Error Detection & Auto-Repair
// ============================================================================
// Production subagent that scans source files for syntax errors, classifies
// them by category, generates targeted fixes, and applies them with
// validation. Supports C/C++, MASM x64, and header files.
//
// Architecture: C++20, Win32, no Qt, no exceptions
// Threading:    Mutex-protected. Thread-safe.
// Rule:         NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================
#pragma once

#include "autonomous_subagent.hpp"
#include "agentic_failure_detector.hpp"
#include "agentic_puppeteer.hpp"
#include "../subagent_core.h"

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include <atomic>
#include <functional>
#include <memory>

// ============================================================================
//  SyntaxErrorKind — Classification of syntax errors
// ============================================================================
enum class SyntaxErrorKind : uint8_t {
    Unknown                 = 0,
    // Bracket/delimiter errors
    UnmatchedBrace          = 1,   ///< { without }
    UnmatchedParen          = 2,   ///< ( without )
    UnmatchedBracket        = 3,   ///< [ without ]
    UnmatchedAngle          = 4,   ///< < without > in templates
    // Punctuation errors
    MissingSemicolon        = 5,   ///< Missing ; at end of statement
    ExtraSemicolon          = 6,   ///< Extraneous ;
    MissingComma            = 7,   ///< Missing , in parameter list
    ExtraComma              = 8,   ///< Trailing ,
    // Keyword/construct errors
    UnterminatedString      = 9,   ///< String literal not closed
    UnterminatedComment     = 10,  ///< /* without */
    MalformedPreprocessor   = 11,  ///< Bad #directive
    MissingEndif            = 12,  ///< #if without #endif
    OrphanedElse            = 13,  ///< #else/#endif without #if
    DuplicateDefault        = 14,  ///< Multiple default: in switch
    MissingBreak            = 15,  ///< Fallthrough in switch without comment
    // Type/declaration errors
    MissingReturnType       = 16,  ///< Function without return type
    MissingParamType        = 17,  ///< Parameter without type
    InvalidTypeSpec         = 18,  ///< Unknown type specifier
    // MASM-specific errors
    MasmMissingEndp         = 19,  ///< PROC without ENDP
    MasmMissingEnd          = 20,  ///< Missing END directive
    MasmBadRegister         = 21,  ///< Invalid register name
    MasmBadOperand          = 22,  ///< Invalid operand combination
    MasmMissingFrame        = 23,  ///< PROC FRAME without .ENDPROLOG
    MasmBadDirective        = 24,  ///< Unknown directive
    // Scope/structure errors
    RedundantQualifier      = 25,  ///< Duplicate const/volatile
    DanglingPointer         = 26,  ///< Taking address of temporary
    EmptyBody               = 27,  ///< Empty if/while/for body (warning)
    UnreachableCode         = 28,  ///< Code after return/break
    // Encoding errors
    InvalidUTF8             = 29,  ///< Malformed UTF-8 bytes
    BOMMismatch             = 30,  ///< BOM present but wrong encoding
};

inline const char* syntaxErrorKindStr(SyntaxErrorKind k) {
    switch (k) {
        case SyntaxErrorKind::UnmatchedBrace:       return "unmatched-brace";
        case SyntaxErrorKind::UnmatchedParen:       return "unmatched-paren";
        case SyntaxErrorKind::UnmatchedBracket:     return "unmatched-bracket";
        case SyntaxErrorKind::UnmatchedAngle:       return "unmatched-angle";
        case SyntaxErrorKind::MissingSemicolon:      return "missing-semicolon";
        case SyntaxErrorKind::ExtraSemicolon:        return "extra-semicolon";
        case SyntaxErrorKind::MissingComma:          return "missing-comma";
        case SyntaxErrorKind::ExtraComma:            return "extra-comma";
        case SyntaxErrorKind::UnterminatedString:    return "unterminated-string";
        case SyntaxErrorKind::UnterminatedComment:   return "unterminated-comment";
        case SyntaxErrorKind::MalformedPreprocessor: return "malformed-preprocessor";
        case SyntaxErrorKind::MissingEndif:          return "missing-endif";
        case SyntaxErrorKind::OrphanedElse:          return "orphaned-else";
        case SyntaxErrorKind::DuplicateDefault:      return "duplicate-default";
        case SyntaxErrorKind::MissingBreak:          return "missing-break";
        case SyntaxErrorKind::MissingReturnType:     return "missing-return-type";
        case SyntaxErrorKind::MissingParamType:      return "missing-param-type";
        case SyntaxErrorKind::InvalidTypeSpec:       return "invalid-type-spec";
        case SyntaxErrorKind::MasmMissingEndp:       return "masm-missing-endp";
        case SyntaxErrorKind::MasmMissingEnd:        return "masm-missing-end";
        case SyntaxErrorKind::MasmBadRegister:       return "masm-bad-register";
        case SyntaxErrorKind::MasmBadOperand:        return "masm-bad-operand";
        case SyntaxErrorKind::MasmMissingFrame:      return "masm-missing-frame";
        case SyntaxErrorKind::MasmBadDirective:      return "masm-bad-directive";
        case SyntaxErrorKind::RedundantQualifier:    return "redundant-qualifier";
        case SyntaxErrorKind::DanglingPointer:       return "dangling-pointer";
        case SyntaxErrorKind::EmptyBody:             return "empty-body";
        case SyntaxErrorKind::UnreachableCode:       return "unreachable-code";
        case SyntaxErrorKind::InvalidUTF8:           return "invalid-utf8";
        case SyntaxErrorKind::BOMMismatch:           return "bom-mismatch";
        default:                                     return "unknown";
    }
}

// ============================================================================
//  SyntaxSeverity
// ============================================================================
enum class SyntaxSeverity : uint8_t {
    Info    = 0,
    Warning = 1,
    Error   = 2,
    Fatal   = 3,
};

// ============================================================================
//  SyntaxError — One detected syntax error
// ============================================================================
struct SyntaxError {
    SyntaxErrorKind kind = SyntaxErrorKind::Unknown;
    SyntaxSeverity severity = SyntaxSeverity::Error;
    std::string filePath;
    int line = 0;
    int column = 0;
    int endLine = 0;
    int endColumn = 0;
    std::string message;
    std::string context;            ///< Source line(s) around the error
    std::string suggestion;         ///< Auto-generated fix suggestion
    bool autoFixable = false;       ///< Can be auto-repaired

    std::string toJSON() const;
};

// ============================================================================
//  SyntaxFixAction — One repair action
// ============================================================================
struct SyntaxFixAction {
    enum class Type : uint8_t {
        InsertText,                 ///< Insert text at position
        ReplaceText,                ///< Replace text range
        DeleteText,                 ///< Delete text range
        InsertLine,                 ///< Insert a full line
        DeleteLine,                 ///< Delete a full line
        ReplaceLine,                ///< Replace entire line
        WrapBlock,                  ///< Wrap code in a block
        SwapLines,                  ///< Swap two lines
        IndentBlock,                ///< Fix indentation
        ReformatBlock,              ///< Reformat a code block
    } type;

    std::string filePath;
    int line = -1;
    int column = -1;
    int endLine = -1;
    int endColumn = -1;
    std::string oldText;
    std::string newText;
    std::string reason;
    SyntaxErrorKind errorKind = SyntaxErrorKind::Unknown;
    int priority = 0;
    float confidence = 1.0f;        ///< 0.0-1.0 confidence in the fix

    bool operator<(const SyntaxFixAction& o) const { return priority > o.priority; }
};

// ============================================================================
//  SyntaxHealerConfig
// ============================================================================
struct SyntaxHealerConfig {
    std::string projectRoot;
    std::vector<std::string> sourceExtensions = {
        ".cpp", ".c", ".cc", ".cxx", ".h", ".hpp", ".hxx", ".inc", ".asm"
    };
    int maxFilesPerScan = 4096;
    int maxErrorsPerFile = 256;
    float minFixConfidence = 0.7f;  ///< Only apply fixes with confidence >= this
    bool fixBraces = true;
    bool fixSemicolons = true;
    bool fixStrings = true;
    bool fixComments = true;
    bool fixPreprocessor = true;
    bool fixMasm = true;
    bool fixEncoding = true;
    bool fixIndentation = false;    ///< Disabled by default (cosmetic)
    bool dryRun = false;            ///< Don't actually apply fixes
    int maxRetries = 3;
    int perFileTimeoutMs = 30000;

    /// MASM register whitelist for validation
    std::unordered_set<std::string> validMasmRegisters;
};

// ============================================================================
//  SyntaxHealerResult
// ============================================================================
struct SyntaxHealerResult {
    bool success = false;
    std::string scanId;
    int filesScanned = 0;
    int errorsFound = 0;
    int warningsFound = 0;
    int errorsFixed = 0;
    int errorsFailed = 0;
    int errorsSkipped = 0;
    std::vector<SyntaxError> remainingErrors;
    std::vector<SyntaxFixAction> appliedFixes;
    std::string error;
    int elapsedMs = 0;

    static SyntaxHealerResult ok(const std::string& id) {
        SyntaxHealerResult r;
        r.success = true;
        r.scanId = id;
        return r;
    }
    static SyntaxHealerResult fail(const std::string& id, const std::string& msg) {
        SyntaxHealerResult r;
        r.success = false;
        r.scanId = id;
        r.error = msg;
        return r;
    }

    std::string summary() const;
};

// ============================================================================
//  Callbacks
// ============================================================================
using SyntaxHealerProgressCb = std::function<void(const std::string& scanId,
                                                    int filesProcessed, int totalFiles)>;
using SyntaxHealerErrorCb    = std::function<void(const std::string& scanId,
                                                    const SyntaxError& error)>;
using SyntaxHealerFixCb      = std::function<void(const std::string& scanId,
                                                    const SyntaxFixAction& fix, bool applied)>;
using SyntaxHealerCompleteCb = std::function<void(const SyntaxHealerResult& result)>;

// ============================================================================
//  SyntaxHealerSubAgent — Production syntax error detector & auto-repairer
// ============================================================================
class SyntaxHealerSubAgent {
public:
    explicit SyntaxHealerSubAgent(SubAgentManager* manager,
                                   AgenticEngine* engine,
                                   AgenticFailureDetector* detector = nullptr,
                                   AgenticPuppeteer* puppeteer = nullptr);
    ~SyntaxHealerSubAgent();

    // ---- Configuration ----
    void setConfig(const SyntaxHealerConfig& config);
    const SyntaxHealerConfig& config() const { return m_config; }

    /// Initialize default MASM register set
    void initDefaultMasmRegisters();

    // ---- Single-File Analysis ----

    /// Detect all syntax errors in a C/C++ file
    std::vector<SyntaxError> analyzeCpp(const std::string& filePath) const;

    /// Detect all syntax errors in a MASM file
    std::vector<SyntaxError> analyzeMasm(const std::string& filePath) const;

    /// Detect syntax errors in any supported file
    std::vector<SyntaxError> analyze(const std::string& filePath) const;

    /// Generate fixes for detected errors
    std::vector<SyntaxFixAction> generateFixes(
        const std::vector<SyntaxError>& errors) const;

    // ---- Bulk Scan & Fix ----

    /// Scan files for syntax errors (synchronous)
    SyntaxHealerResult scan(const std::string& parentId,
                             const std::vector<std::string>& filePaths);

    /// Scan + auto-fix (synchronous)
    SyntaxHealerResult scanAndFix(const std::string& parentId,
                                   const std::vector<std::string>& filePaths);

    /// Async variant
    std::string scanAndFixAsync(const std::string& parentId,
                                 const std::vector<std::string>& filePaths,
                                 SyntaxHealerCompleteCb onComplete = nullptr);

    // ---- Apply Fixes ----

    /// Apply fixes to files
    int applyFixes(std::vector<SyntaxFixAction>& fixes);

    /// Validate that a fix didn't introduce new errors
    bool validateFix(const std::string& filePath,
                     const SyntaxFixAction& fix) const;

    // ---- Callbacks ----
    void setOnProgress(SyntaxHealerProgressCb cb) { m_onProgress = cb; }
    void setOnError(SyntaxHealerErrorCb cb)       { m_onError = cb; }
    void setOnFix(SyntaxHealerFixCb cb)           { m_onFix = cb; }
    void setOnComplete(SyntaxHealerCompleteCb cb)  { m_onComplete = cb; }

    // ---- Status ----
    bool isRunning() const { return m_running.load(); }
    void cancel();

    struct Stats {
        int64_t totalScans = 0;
        int64_t totalFilesProcessed = 0;
        int64_t totalErrorsFound = 0;
        int64_t totalErrorsFixed = 0;
        int64_t totalFixesFailed = 0;
        int64_t totalLLMAssists = 0;
    };
    Stats getStats() const;
    void resetStats();

private:
    // ---- C/C++ Analysis Helpers ----
    void checkBraces(const std::string& content, const std::string& filePath,
                     std::vector<SyntaxError>& errors) const;
    void checkSemicolons(const std::string& content, const std::string& filePath,
                         std::vector<SyntaxError>& errors) const;
    void checkStrings(const std::string& content, const std::string& filePath,
                      std::vector<SyntaxError>& errors) const;
    void checkComments(const std::string& content, const std::string& filePath,
                       std::vector<SyntaxError>& errors) const;
    void checkPreprocessor(const std::string& content, const std::string& filePath,
                           std::vector<SyntaxError>& errors) const;
    void checkEncoding(const std::string& content, const std::string& filePath,
                       std::vector<SyntaxError>& errors) const;

    // ---- MASM Analysis Helpers ----
    void checkMasmProcEndp(const std::string& content, const std::string& filePath,
                            std::vector<SyntaxError>& errors) const;
    void checkMasmRegisters(const std::string& content, const std::string& filePath,
                             std::vector<SyntaxError>& errors) const;
    void checkMasmDirectives(const std::string& content, const std::string& filePath,
                              std::vector<SyntaxError>& errors) const;
    void checkMasmFrame(const std::string& content, const std::string& filePath,
                         std::vector<SyntaxError>& errors) const;

    // ---- Fix Helpers ----
    SyntaxFixAction generateBraceFix(const SyntaxError& error) const;
    SyntaxFixAction generateSemicolonFix(const SyntaxError& error) const;
    SyntaxFixAction generateStringFix(const SyntaxError& error) const;
    SyntaxFixAction generateCommentFix(const SyntaxError& error) const;
    SyntaxFixAction generatePreprocessorFix(const SyntaxError& error) const;
    SyntaxFixAction generateMasmFix(const SyntaxError& error) const;

    /// Apply a single fix
    bool applySingleFix(const SyntaxFixAction& fix);

    /// Self-heal complex error via LLM
    std::string selfHealComplex(const SyntaxError& error, const std::string& context);

    /// Read file with caching
    std::string readFile(const std::string& path) const;

    /// Get line content by number
    std::string getLine(const std::string& content, int line) const;

    /// Count lines in content
    int countLines(const std::string& content) const;

    /// Get context lines around a position
    std::string getContext(const std::string& content, int line, int radius = 2) const;

    /// Generate UUID
    std::string generateId() const;

    // ---- Members ----
    SubAgentManager*         m_manager   = nullptr;
    AgenticEngine*           m_engine    = nullptr;
    AgenticFailureDetector*  m_detector  = nullptr;
    AgenticPuppeteer*        m_puppeteer = nullptr;

    SyntaxHealerConfig       m_config;
    mutable std::mutex       m_mutex;
    std::atomic<bool>        m_running{false};
    std::atomic<bool>        m_cancelled{false};

    mutable std::mutex       m_cacheMutex;
    mutable std::unordered_map<std::string, std::string> m_fileCache;

    Stats                    m_stats;

    SyntaxHealerProgressCb   m_onProgress;
    SyntaxHealerErrorCb      m_onError;
    SyntaxHealerFixCb        m_onFix;
    SyntaxHealerCompleteCb   m_onComplete;
};
