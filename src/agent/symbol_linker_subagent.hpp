// ============================================================================
// symbol_linker_subagent.hpp — Autonomous Symbol Resolution & Cross-TU Linker
// ============================================================================
// Production subagent that resolves unresolved external symbols across
// translation units, detects duplicate definitions, maps symbol references,
// and auto-generates missing stubs/forward declarations.
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
//  SymbolKind — Classification of a symbol
// ============================================================================
enum class SymbolKind : uint8_t {
    Unknown         = 0,
    Function        = 1,
    Variable        = 2,
    Class           = 3,
    Struct          = 4,
    Enum            = 5,
    EnumValue       = 6,
    Typedef         = 7,
    Macro           = 8,
    Namespace       = 9,
    Template        = 10,
    ExternC         = 11,
    StaticVar       = 12,
    StaticFunc      = 13,
    VirtualFunc     = 14,
    OperatorOverload= 15,
    Constructor     = 16,
    Destructor      = 17,
    Label           = 18,    // ASM label
    MasmProc        = 19,    // MASM PROC
    MasmExtern      = 20,    // MASM EXTERN / EXTERNDEF
};

inline const char* symbolKindStr(SymbolKind k) {
    switch (k) {
        case SymbolKind::Function:        return "function";
        case SymbolKind::Variable:        return "variable";
        case SymbolKind::Class:           return "class";
        case SymbolKind::Struct:          return "struct";
        case SymbolKind::Enum:            return "enum";
        case SymbolKind::EnumValue:       return "enum-value";
        case SymbolKind::Typedef:         return "typedef";
        case SymbolKind::Macro:           return "macro";
        case SymbolKind::Namespace:       return "namespace";
        case SymbolKind::Template:        return "template";
        case SymbolKind::ExternC:         return "extern-c";
        case SymbolKind::StaticVar:       return "static-var";
        case SymbolKind::StaticFunc:      return "static-func";
        case SymbolKind::VirtualFunc:     return "virtual-func";
        case SymbolKind::OperatorOverload:return "operator";
        case SymbolKind::Constructor:     return "constructor";
        case SymbolKind::Destructor:      return "destructor";
        case SymbolKind::Label:           return "asm-label";
        case SymbolKind::MasmProc:        return "masm-proc";
        case SymbolKind::MasmExtern:      return "masm-extern";
        default:                          return "unknown";
    }
}

// ============================================================================
//  SymbolLinkage — Visibility/linkage class
// ============================================================================
enum class SymbolLinkage : uint8_t {
    Unknown     = 0,
    External    = 1,    // Visible across TUs
    Internal    = 2,    // static / anonymous namespace
    Inline      = 3,    // inline function/variable
    WeakRef     = 4,    // __attribute__((weak))
    DllExport   = 5,    // __declspec(dllexport)
    DllImport   = 6,    // __declspec(dllimport)
    AsmPublic   = 7,    // MASM PUBLIC
    AsmExtrn    = 8,    // MASM EXTRN
};

// ============================================================================
//  SymbolEntry — One symbol definition or reference
// ============================================================================
struct SymbolEntry {
    std::string name;                   ///< Mangled or unmangled name
    std::string demangledName;          ///< Demangled name (if C++)
    std::string qualifiedName;          ///< Fully qualified (namespace::class::func)
    SymbolKind kind = SymbolKind::Unknown;
    SymbolLinkage linkage = SymbolLinkage::Unknown;

    std::string filePath;               ///< Source file
    int line = 0;                       ///< Line number
    int column = 0;                     ///< Column number

    std::string returnType;             ///< For functions
    std::string signature;              ///< Full signature including params
    std::vector<std::string> paramTypes;///< Parameter types
    std::string parentScope;            ///< Enclosing namespace/class

    bool isDefined = false;             ///< true = definition, false = reference
    bool isForwardDecl = false;         ///< Forward declaration only
    bool isExternC = false;             ///< extern "C" linkage

    uint64_t hash = 0;                  ///< Symbol hash for fast comparison

    /// Compute a deterministic hash
    void computeHash();

    std::string toJSON() const;
};

// ============================================================================
//  SymbolConflict — Duplicate or conflicting symbol
// ============================================================================
struct SymbolConflict {
    enum class Type : uint8_t {
        DuplicateDefinition,            ///< Same symbol defined in multiple TUs
        TypeMismatch,                   ///< Same name, different types
        LinkageMismatch,                ///< Same symbol, different linkage
        ODRViolation,                   ///< One Definition Rule violation
        AmbiguousOverload,              ///< Multiple matching overloads
    } type;

    std::string symbolName;
    std::vector<SymbolEntry> conflicting;
    std::string description;
    int severity = 0;                   ///< 0=info, 1=warning, 2=error

    std::string toJSON() const;
};

// ============================================================================
//  SymbolFixAction — One fix to resolve a symbol issue
// ============================================================================
struct SymbolFixAction {
    enum class Type : uint8_t {
        GenerateStub,                   ///< Create a stub implementation
        AddForwardDecl,                 ///< Add forward declaration
        AddExternDecl,                  ///< Add extern declaration
        AddExternC,                     ///< Wrap with extern "C"
        AddInclude,                     ///< Add #include for the defining header
        CreateHeader,                   ///< Create new header for the symbol
        RemoveDuplicate,                ///< Remove duplicate definition
        AddLinkDirective,               ///< Add link pragma/lib reference
        FixMangling,                    ///< Fix name mangling mismatch
        AddMasmExterndef,               ///< Add EXTERNDEF for ASM
        GenerateDefFile,                ///< Generate .def file entry
    } type;

    std::string filePath;               ///< File to modify
    std::string symbolName;             ///< Symbol being fixed
    int insertLine = -1;                ///< Line to insert at (-1 = auto)
    std::string oldText;                ///< Text to replace (empty for insert)
    std::string newText;                ///< Replacement / new text
    std::string reason;                 ///< Human-readable reason
    int priority = 0;                   ///< Higher = more important

    bool operator<(const SymbolFixAction& o) const { return priority > o.priority; }
};

// ============================================================================
//  SymbolLinkerConfig
// ============================================================================
struct SymbolLinkerConfig {
    std::string projectRoot;
    std::vector<std::string> sourceExtensions = {
        ".cpp", ".c", ".cc", ".cxx", ".h", ".hpp", ".hxx", ".inc", ".asm", ".def"
    };
    int maxFilesPerScan = 4096;
    int maxSymbolsPerFile = 16384;
    bool resolveMangled = true;         ///< Attempt C++ demangling
    bool detectDuplicates = true;       ///< Detect duplicate definitions
    bool detectODR = true;              ///< Detect ODR violations
    bool generateStubs = true;          ///< Auto-generate stub implementations
    bool generateExternDecls = true;    ///< Auto-generate extern declarations
    bool supportMasm = true;            ///< Parse MASM PROC/EXTERNDEF
    bool supportDefFiles = true;        ///< Parse .def export files
    int maxRetries = 3;
    int perFileTimeoutMs = 30000;

    /// Known library symbols to skip
    std::unordered_set<std::string> librarySymbols;
};

// ============================================================================
//  SymbolLinkerResult
// ============================================================================
struct SymbolLinkerResult {
    bool success = false;
    std::string scanId;
    int filesScanned = 0;
    int symbolsDefined = 0;
    int symbolsReferenced = 0;
    int symbolsResolved = 0;
    int symbolsUnresolved = 0;
    int conflictsFound = 0;
    int fixesApplied = 0;
    int fixesFailed = 0;
    int stubsGenerated = 0;
    std::vector<SymbolFixAction> appliedFixes;
    std::vector<SymbolConflict> conflicts;
    std::vector<std::string> unresolvedNames;
    std::string error;
    int elapsedMs = 0;

    static SymbolLinkerResult ok(const std::string& id) {
        SymbolLinkerResult r;
        r.success = true;
        r.scanId = id;
        return r;
    }
    static SymbolLinkerResult fail(const std::string& id, const std::string& msg) {
        SymbolLinkerResult r;
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
using SymbolLinkerProgressCb  = std::function<void(const std::string& scanId,
                                                    int filesProcessed, int totalFiles)>;
using SymbolLinkerFixCb       = std::function<void(const std::string& scanId,
                                                    const SymbolFixAction& fix, bool applied)>;
using SymbolLinkerCompleteCb  = std::function<void(const SymbolLinkerResult& result)>;

// ============================================================================
//  SymbolLinkerSubAgent — Production cross-TU symbol resolver
// ============================================================================
class SymbolLinkerSubAgent {
public:
    explicit SymbolLinkerSubAgent(SubAgentManager* manager,
                                  AgenticEngine* engine,
                                  AgenticFailureDetector* detector = nullptr,
                                  AgenticPuppeteer* puppeteer = nullptr);
    ~SymbolLinkerSubAgent();

    // ---- Configuration ----
    void setConfig(const SymbolLinkerConfig& config);
    const SymbolLinkerConfig& config() const { return m_config; }

    // ---- Single-File Symbol Extraction ----

    /// Extract all symbols (definitions + references) from a C/C++ source file
    std::vector<SymbolEntry> extractSymbolsCpp(const std::string& filePath) const;

    /// Extract symbols from a MASM assembly file
    std::vector<SymbolEntry> extractSymbolsMasm(const std::string& filePath) const;

    /// Extract exports from a .def file
    std::vector<SymbolEntry> extractSymbolsDef(const std::string& filePath) const;

    /// Extract symbols from any supported file type
    std::vector<SymbolEntry> extractSymbols(const std::string& filePath) const;

    // ---- Name Mangling ----

    /// Attempt to demangle a C++ mangled name
    std::string demangle(const std::string& mangledName) const;

    /// Check if a name appears mangled
    bool isMangled(const std::string& name) const;

    // ---- Cross-TU Resolution ----

    /// Build a complete symbol table from multiple files
    void buildSymbolTable(const std::vector<std::string>& filePaths);

    /// Resolve all references against definitions
    void resolveReferences();

    /// Get all unresolved external references
    std::vector<SymbolEntry> getUnresolved() const;

    /// Get all defined symbols
    std::vector<SymbolEntry> getDefined() const;

    /// Find the definition(s) for a symbol name
    std::vector<SymbolEntry> findDefinitions(const std::string& name) const;

    /// Find all references to a symbol
    std::vector<SymbolEntry> findReferences(const std::string& name) const;

    // ---- Conflict Detection ----

    /// Detect duplicate definitions and ODR violations
    std::vector<SymbolConflict> detectConflicts() const;

    // ---- Bulk Scan & Fix ----

    /// Scan files and resolve symbols (synchronous)
    SymbolLinkerResult scan(const std::string& parentId,
                            const std::vector<std::string>& filePaths);

    /// Scan + auto-fix all found issues (synchronous)
    SymbolLinkerResult scanAndFix(const std::string& parentId,
                                  const std::vector<std::string>& filePaths);

    /// Async variant
    std::string scanAndFixAsync(const std::string& parentId,
                                 const std::vector<std::string>& filePaths,
                                 SymbolLinkerCompleteCb onComplete = nullptr);

    // ---- Fix Generation ----

    /// Generate fixes for unresolved symbols
    std::vector<SymbolFixAction> generateFixes() const;

    /// Generate a stub implementation for a function symbol
    std::string generateStub(const SymbolEntry& symbol) const;

    /// Generate an extern declaration for a symbol
    std::string generateExternDecl(const SymbolEntry& symbol) const;

    /// Generate a MASM EXTERNDEF for a symbol
    std::string generateMasmExterndef(const SymbolEntry& symbol) const;

    /// Generate .def file entries for exported symbols
    std::string generateDefEntries(const std::vector<SymbolEntry>& symbols) const;

    /// Apply a batch of fixes
    int applyFixes(std::vector<SymbolFixAction>& fixes);

    // ---- Callbacks ----
    void setOnProgress(SymbolLinkerProgressCb cb) { m_onProgress = cb; }
    void setOnFix(SymbolLinkerFixCb cb)           { m_onFix = cb; }
    void setOnComplete(SymbolLinkerCompleteCb cb)  { m_onComplete = cb; }

    // ---- Status ----
    bool isRunning() const { return m_running.load(); }
    void cancel();

    struct Stats {
        int64_t totalScans = 0;
        int64_t totalFilesProcessed = 0;
        int64_t totalSymbolsExtracted = 0;
        int64_t totalRefsResolved = 0;
        int64_t totalStubsGenerated = 0;
        int64_t totalFixesApplied = 0;
        int64_t totalConflictsFound = 0;
    };
    Stats getStats() const;
    void resetStats();

private:
    /// Read file contents (cached)
    std::string readFile(const std::string& path) const;

    /// Normalize a path
    std::string normalizePath(const std::string& path) const;

    /// Check if a symbol is a known library symbol
    bool isLibrarySymbol(const std::string& name) const;

    /// Classify a C/C++ line to detect definitions vs references
    SymbolKind classifyLine(const std::string& line, const std::string& trimmed,
                             bool& isDef, bool& isExternC) const;

    /// Parse a function signature
    void parseFunctionSignature(const std::string& line, SymbolEntry& entry) const;

    /// Apply a single fix
    bool applySingleFix(const SymbolFixAction& fix);

    /// Self-heal via LLM when static analysis fails
    std::string selfHealResolution(const SymbolEntry& unresolved);

    /// Generate UUID
    std::string generateId() const;

    // ---- Members ----
    SubAgentManager*         m_manager   = nullptr;
    AgenticEngine*           m_engine    = nullptr;
    AgenticFailureDetector*  m_detector  = nullptr;
    AgenticPuppeteer*        m_puppeteer = nullptr;

    SymbolLinkerConfig       m_config;
    mutable std::mutex       m_mutex;
    std::atomic<bool>        m_running{false};
    std::atomic<bool>        m_cancelled{false};

    // Symbol tables
    struct SymbolTable {
        std::unordered_map<std::string, std::vector<SymbolEntry>> definitions;
        std::unordered_map<std::string, std::vector<SymbolEntry>> references;
        std::unordered_set<std::string> resolvedRefs;
        std::unordered_set<std::string> unresolvedRefs;
    };
    SymbolTable              m_symtab;
    mutable std::mutex       m_symMutex;

    // File cache
    mutable std::mutex       m_cacheMutex;
    mutable std::unordered_map<std::string, std::string> m_fileCache;

    Stats                    m_stats;

    SymbolLinkerProgressCb   m_onProgress;
    SymbolLinkerFixCb        m_onFix;
    SymbolLinkerCompleteCb   m_onComplete;
};
