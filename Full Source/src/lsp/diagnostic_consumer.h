// ============================================================================
// diagnostic_consumer.h — LSP Diagnostic Consumer
// ============================================================================
// Aggregates diagnostics from clangd/language servers, provides quick-fix
// suggestions, severity filtering, and agentic auto-fix pipeline integration.
//
// Pattern: PatchResult-style, no exceptions, factory results.
// Rule:    NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <mutex>
#include <chrono>

namespace RawrXD {
namespace LSP {

// ============================================================================
// Diagnostic Severity (matches LSP spec)
// ============================================================================

// Windows headers #define ERROR 0 — must undefine to use as enum value
#ifdef ERROR
#undef ERROR
#endif

enum class DiagnosticSeverity : uint8_t {
    ERROR       = 1,
    WARNING     = 2,
    INFORMATION = 3,
    HINT        = 4
};

const char* severityToString(DiagnosticSeverity sev);

// ============================================================================
// Diagnostic Source
// ============================================================================

enum class DiagnosticSource : uint8_t {
    CLANGD      = 0,
    GGUF_LINT   = 1,  // Our hotpatch validator
    ASM_LINT    = 2,  // MASM/NASM diagnostics
    HOTPATCH    = 3,  // Hotpatch conflict diagnostics
    USER        = 4,  // User-defined rules
    AGENT       = 5   // Agentic failure detector
};

// ============================================================================
// Text Range
// ============================================================================

struct TextRange {
    int startLine;
    int startCol;
    int endLine;
    int endCol;
};

// ============================================================================
// Diagnostic
// ============================================================================

struct Diagnostic {
    std::string        file;
    TextRange          range;
    DiagnosticSeverity severity;
    DiagnosticSource   source;
    std::string        code;       // e.g. "C2664", "E0020", "hotpatch-conflict"
    std::string        message;
    uint64_t           timestampMs;

    // Related diagnostics (e.g., "see declaration of X")
    std::vector<std::pair<std::string, TextRange>> relatedInfo;
};

// ============================================================================
// Quick Fix / Code Action
// ============================================================================

struct QuickFix {
    std::string title;             // "Add #include <string>"
    std::string kind;              // "quickfix", "refactor", "source.organizeImports"
    bool        isPreferred;       // Preferred fix (auto-applicable)

    // Text edits to apply
    struct TextEdit {
        std::string file;
        TextRange   range;
        std::string newText;
    };
    std::vector<TextEdit> edits;

    // Alternative: command to execute
    std::string command;
    std::string commandArgs;       // JSON
};

// ============================================================================
// Diagnostic Stats
// ============================================================================

struct DiagnosticStats {
    int errorCount;
    int warningCount;
    int infoCount;
    int hintCount;
    int totalFiles;
    int fixableCount;              // Diagnostics with available quick fixes
};

// ============================================================================
// Diagnostic Consumer Result
// ============================================================================

struct DiagResult {
    bool        success;
    const char* detail;
    int         errorCode;

    static DiagResult ok(const char* msg = "OK") {
        DiagResult r;
        r.success   = true;
        r.detail    = msg;
        r.errorCode = 0;
        return r;
    }

    static DiagResult error(const char* msg, int code = -1) {
        DiagResult r;
        r.success   = false;
        r.detail    = msg;
        r.errorCode = code;
        return r;
    }
};

// ============================================================================
// Diagnostic Filter
// ============================================================================

struct DiagnosticFilter {
    DiagnosticSeverity  minSeverity = DiagnosticSeverity::HINT;
    DiagnosticSource    source      = DiagnosticSource::CLANGD; // 0xFF = all
    bool                allSources  = true;
    std::string         filePattern;  // Glob pattern, empty = all files
    std::string         codePattern;  // Regex for diagnostic code
};

// ============================================================================
// Diagnostic Change Callback
// ============================================================================

using DiagnosticCallback = void(*)(const std::string& file,
                                    const std::vector<Diagnostic>& diags,
                                    void* userData);

// ============================================================================
// Diagnostic Consumer
// ============================================================================

class DiagnosticConsumer {
public:
    DiagnosticConsumer();
    ~DiagnosticConsumer();

    // Singleton
    static DiagnosticConsumer& Global();

    // ---- Ingestion ----

    // Publish diagnostics for a file (from LSP textDocument/publishDiagnostics)
    DiagResult publishDiagnostics(const std::string& file,
                                   const std::vector<Diagnostic>& diagnostics);

    // Add a single diagnostic
    DiagResult addDiagnostic(const Diagnostic& diag);

    // Clear diagnostics for a file
    void clearFile(const std::string& file);

    // Clear all diagnostics
    void clearAll();

    // ---- Querying ----

    // Get diagnostics for a file
    std::vector<Diagnostic> getDiagnostics(const std::string& file) const;

    // Get diagnostics for a file filtered by severity
    std::vector<Diagnostic> getDiagnostics(const std::string& file,
                                            DiagnosticSeverity minSeverity) const;

    // Get all diagnostics matching a filter
    std::vector<Diagnostic> query(const DiagnosticFilter& filter) const;

    // Get diagnostics at a specific line
    std::vector<Diagnostic> getDiagnosticsAtLine(const std::string& file,
                                                   int line) const;

    // Get diagnostics in a range
    std::vector<Diagnostic> getDiagnosticsInRange(const std::string& file,
                                                    const TextRange& range) const;

    // ---- Statistics ----

    DiagnosticStats getStats() const;
    DiagnosticStats getFileStats(const std::string& file) const;

    // ---- Quick Fixes ----

    // Register quick fix provider for a diagnostic code pattern
    using QuickFixProvider = std::vector<QuickFix>(*)(const Diagnostic& diag,
                                                       void* userData);
    void registerQuickFixProvider(const std::string& codePattern,
                                  QuickFixProvider provider,
                                  void* userData);

    // Get available quick fixes for a diagnostic
    std::vector<QuickFix> getQuickFixes(const Diagnostic& diag) const;

    // Get all fixable diagnostics for a file
    std::vector<std::pair<Diagnostic, std::vector<QuickFix>>>
        getFixableDiagnostics(const std::string& file) const;

    // ---- Agentic Integration ----

    // Build a prompt string describing errors for agentic auto-fix
    std::string buildFixPrompt(const std::string& file,
                                int maxTokens = 2000) const;

    // Classify diagnostic for agentic routing
    // Returns: "syntax", "type", "linker", "include", "hotpatch", "unknown"
    const char* classifyDiagnostic(const Diagnostic& diag) const;

    // ---- Callbacks ----

    void registerCallback(DiagnosticCallback callback, void* userData);
    void removeCallback(DiagnosticCallback callback);

private:
    // File → diagnostics
    std::unordered_map<std::string, std::vector<Diagnostic>> m_diagnostics;

    // Quick fix providers: code pattern → (provider, userData)
    struct FixProviderEntry {
        std::string     codePattern;
        QuickFixProvider provider;
        void*           userData;
    };
    std::vector<FixProviderEntry> m_fixProviders;

    // Change callbacks
    struct CallbackEntry {
        DiagnosticCallback callback;
        void*              userData;
    };
    std::vector<CallbackEntry> m_callbacks;

    mutable std::mutex m_mutex;

    // Notify callbacks
    void notifyCallbacks(const std::string& file) const;
};

} // namespace LSP
} // namespace RawrXD
