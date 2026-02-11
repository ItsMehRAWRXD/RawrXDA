// ============================================================================
// pdb_reference_provider.h — Phase 29.2: Multi-Result Reference Provider
// ============================================================================
//
// PURPOSE:
//   "Find All References" and "Go to References" for PDB-resolved symbols.
//   Returns 1, 2, 3, or N results per query — NOT just a single definition.
//
//   Architecture:
//     ReferenceQuery → ReferenceRouter → [Provider₁, Provider₂, ... Providerₙ]
//                                              ↓         ↓            ↓
//                                         results₁  results₂    resultsₙ
//                                              ↓         ↓            ↓
//                                         ← merged + deduplicated + ranked →
//
//   The user picks any subset (1, 2, 3, or all N) from the result set.
//   This is NOT a single-answer Go-to-Definition — it's a full reference
//   enumeration system.
//
// PROVIDERS (pluggable — register via ReferenceRouter::addProvider):
//   1. PDBPublicSymbolProvider — Public symbols from loaded PDBs
//   2. PDBProcedureProvider   — Function boundaries (GPROC32/LPROC32)
//   3. ImportTableProvider    — PE import table cross-references
//   4. LocalIndexProvider     — LSP local workspace index
//   5. InstructionHookProvider — AI/model-driven reference resolution
//
// TOOLS/INSTRUCTIONS HOOK:
//   The InstructionHookProvider reads the user's request, routes through
//   registered "instruction files" (markdown/text rules), and generates
//   synthetic references based on semantic understanding. This bridges
//   the gap between static symbol resolution and agentic comprehension.
//
//   Hook dispatch flow:
//     1. User types "Find references to NtCreateFile"
//     2. Router queries all providers in parallel
//     3. PDB provider returns: ntdll.dll!NtCreateFile at RVA 0x12345
//     4. InstructionHookProvider reads .instructions.md files
//     5. Hook matches "NtCreateFile" → knows it's a syscall wrapper
//     6. Hook adds synthetic refs: ntoskrnl.exe!NtCreateFile (kernel-side),
//        kernelbase.dll!CreateFileW (user-mode wrapper), etc.
//     7. All results merged → user picks 1..N from the list
//
// PATTERN:   PDBResult-compatible, no exceptions, no std::function
// THREADING: Providers are called sequentially on caller thread.
//            Phase 29.3: parallel dispatch via thread pool.
// ============================================================================
#pragma once

#include <cstdint>
#include <cstddef>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace RawrXD {
namespace PDB {

// Forward declarations from pdb_native.h
struct PDBResult;
struct ResolvedSymbol;

// ============================================================================
// ReferenceLocation — A single reference result
// ============================================================================
//
// Each reference has:
//   - moduleName:  "ntdll.dll", "kernelbase.dll", etc.
//   - symbolName:  "NtCreateFile", "CreateFileW", etc.
//   - rva:         Relative virtual address within the module
//   - section/offset: PE section location
//   - kind:        What type of reference this is
//   - confidence:  0.0..1.0 — how confident the provider is
//   - providerTag: Which provider generated this result
//   - detail:      Human-readable explanation
//

enum class ReferenceKind : uint8_t {
    Definition      = 0,    // Primary definition (Go-to-Definition target)
    Declaration     = 1,    // Forward declaration
    Implementation  = 2,    // Implementation body
    Import          = 3,    // Import table entry (IAT/INT)
    Export          = 4,    // Export table entry (EAT)
    CrossReference  = 5,    // Call site or data reference
    SyscallStub     = 6,    // Ntdll syscall stub
    KernelEntry     = 7,    // Kernel-mode entry point
    WrapperFunc     = 8,    // Higher-level wrapper (e.g. CreateFileW → NtCreateFile)
    Synthetic       = 9,    // AI/instruction-generated reference
    TypeReference   = 10,   // Type system reference (TPI)
};

struct ReferenceLocation {
    char        moduleName[64];     // Source module
    char        symbolName[256];    // Symbol name
    uint64_t    rva;                // RVA within module
    uint16_t    section;            // PE section (1-based)
    uint32_t    sectionOffset;      // Offset within section
    uint32_t    size;               // Symbol size (0 if unknown)
    ReferenceKind kind;             // Type of reference
    float       confidence;         // 0.0..1.0
    const char* providerTag;        // "pdb-public", "pdb-proc", "import", "local", "hook"
    char        detail[256];        // Human-readable description
    bool        isFunction;         // true if procedure
};

// ============================================================================
// ReferenceQuery — What the user is searching for
// ============================================================================

struct ReferenceQuery {
    const char* symbolName;         // The symbol to find references for
    uint32_t    symbolNameLen;      // Length (not including null)
    const char* contextModule;      // Optional: limit to this module (nullptr = all)
    uint64_t    contextRVA;         // Optional: RVA context (0 = none)
    uint32_t    maxResults;         // Maximum results to return (0 = unlimited)
    bool        includeDefinitions; // Include definitions?
    bool        includeImports;     // Include import table refs?
    bool        includeExports;     // Include export table refs?
    bool        includeSynthetic;   // Include AI/instruction-generated refs?
    bool        crossModuleSearch;  // Search across all loaded modules?

    static ReferenceQuery forSymbol(const char* name) {
        ReferenceQuery q{};
        q.symbolName = name;
        q.symbolNameLen = name ? static_cast<uint32_t>(strlen(name)) : 0;
        q.maxResults = 64;
        q.includeDefinitions = true;
        q.includeImports = true;
        q.includeExports = true;
        q.includeSynthetic = true;
        q.crossModuleSearch = true;
        return q;
    }
};

// ============================================================================
// ReferenceResult — Collected results from all providers
// ============================================================================

static constexpr uint32_t MAX_REFERENCES = 256;

struct ReferenceResult {
    ReferenceLocation refs[MAX_REFERENCES];
    uint32_t    count;              // Number of valid entries
    uint32_t    totalAvailable;     // Total before maxResults cap
    bool        truncated;          // true if more results exist than MAX_REFERENCES

    void clear() {
        count = 0;
        totalAvailable = 0;
        truncated = false;
    }

    bool addRef(const ReferenceLocation& loc) {
        if (count >= MAX_REFERENCES) {
            truncated = true;
            totalAvailable++;
            return false;
        }
        refs[count++] = loc;
        totalAvailable++;
        return true;
    }
};

// ============================================================================
// IReferenceProvider — Abstract provider interface (C-style vtable)
// ============================================================================
//
// Each provider implements this interface. The router calls findReferences()
// on each registered provider and merges the results.
//
// Why not std::function? Per project rules: no std::function in hot paths.
// Why not virtual? We use C-style function pointers for ABI stability and
// to match the three-layer hotpatching architecture.
//

struct IReferenceProvider {
    // Provider identity
    const char* tag;                // e.g., "pdb-public", "import-table", "hook"
    const char* description;        // Human-readable description

    // Query this provider for references.
    // provider: 'this' pointer (opaque)
    // query:    what to search for
    // result:   output — append to this
    // Returns:  PDBResult indicating success/failure
    PDBResult (*findReferences)(void* provider,
                                 const ReferenceQuery* query,
                                 ReferenceResult* result);

    // Get provider statistics
    uint32_t (*getIndexedCount)(void* provider);

    // Is this provider ready?
    bool (*isAvailable)(void* provider);

    // Opaque provider state (the 'this' for the callbacks)
    void* state;
};

// ============================================================================
// InstructionFile — Registered instruction/tool file for hook provider
// ============================================================================
//
// Instruction files are markdown/text files that contain rules for
// symbol resolution. The InstructionHookProvider reads these files and
// uses pattern matching to generate synthetic references.
//
// Example instruction file entry:
//   ## NtCreateFile
//   - Kernel entry: ntoskrnl.exe!NtCreateFile
//   - User wrapper: kernelbase.dll!CreateFileW
//   - Alt wrapper: kernel32.dll!CreateFileA
//   - Syscall ID: 0x55 (Win10 21H2)
//

static constexpr uint32_t MAX_INSTRUCTION_FILES = 32;
static constexpr uint32_t MAX_INSTRUCTION_RULES = 512;

struct InstructionRule {
    char pattern[128];              // Symbol name pattern (exact or wildcard)
    char targetModule[64];          // Target module for the reference
    char targetSymbol[256];         // Target symbol name
    ReferenceKind kind;             // What kind of reference this generates
    float confidence;               // Default confidence for this rule
    bool  isWildcard;               // true if pattern contains '*' or '?'
};

struct InstructionFile {
    wchar_t path[MAX_PATH];         // Absolute path to the file
    char    tag[64];                // Short identifier
    bool    loaded;                 // Successfully parsed?
    uint32_t ruleCount;             // Number of rules extracted
    uint64_t lastModifiedTime;      // For hot-reload detection
};

// ============================================================================
// ReferenceRouter — Central dispatch for all reference providers
// ============================================================================
//
// Singleton. Manages provider registration, query dispatch, result
// merging, and deduplication. Owns the InstructionHookProvider.
//
// Usage:
//   ReferenceRouter& router = ReferenceRouter::instance();
//   router.addProvider(&myProvider);
//   ReferenceResult result;
//   router.findAllReferences(ReferenceQuery::forSymbol("NtCreateFile"), &result);
//   // result.count == N (user can pick 1..N)
//

class ReferenceRouter {
public:
    static ReferenceRouter& instance();

    // ---- Provider Management ----
    PDBResult addProvider(IReferenceProvider* provider);
    void      removeProvider(const char* tag);
    uint32_t  getProviderCount() const { return m_providerCount; }
    const IReferenceProvider* getProvider(uint32_t index) const;

    // ---- Query ----
    // Find all references across all providers. Results are merged,
    // deduplicated (by module+symbol+RVA), and sorted by confidence.
    PDBResult findAllReferences(const ReferenceQuery& query,
                                 ReferenceResult* result);

    // Find references from a single provider only.
    PDBResult findReferencesFrom(const char* providerTag,
                                  const ReferenceQuery& query,
                                  ReferenceResult* result);

    // ---- Instruction Hook Management ----
    PDBResult loadInstructionFile(const wchar_t* path, const char* tag);
    PDBResult reloadInstructionFiles();
    uint32_t  getInstructionFileCount() const { return m_instrFileCount; }
    uint32_t  getInstructionRuleCount() const { return m_instrRuleCount; }

    // ---- Pattern Matching (used by instruction hook callbacks) ----
    bool matchPattern(const char* pattern, const char* name, bool isWildcard) const;

    // ---- Built-in Providers (auto-registered) ----
    // These are created and owned by the router.
    void initBuiltinProviders();

    // ---- Statistics ----
    struct Stats {
        uint64_t totalQueries;
        uint64_t totalResults;
        uint64_t cacheHits;
        uint64_t providerErrors;
        uint32_t providersActive;
        uint32_t instructionRulesLoaded;
    };
    Stats getStats() const;

private:
    ReferenceRouter();
    ~ReferenceRouter();

    // No copy
    ReferenceRouter(const ReferenceRouter&) = delete;
    ReferenceRouter& operator=(const ReferenceRouter&) = delete;

    // ---- Provider Storage ----
    static constexpr uint32_t MAX_PROVIDERS = 16;
    IReferenceProvider* m_providers[MAX_PROVIDERS];
    uint32_t m_providerCount;

    // ---- Instruction System ----
    InstructionFile m_instrFiles[MAX_INSTRUCTION_FILES];
    uint32_t m_instrFileCount;
    InstructionRule m_instrRules[MAX_INSTRUCTION_RULES];
    uint32_t m_instrRuleCount;

    // ---- Deduplication ----
    // Hash: FNV-1a of (module + symbol + rva) → used for dedup
    static uint64_t hashReference(const ReferenceLocation& loc);
    void deduplicateResults(ReferenceResult* result);
    void sortByConfidence(ReferenceResult* result);

    // ---- Instruction File Parser ----
    PDBResult parseInstructionFile(const wchar_t* path, InstructionFile* fileOut);

    // ---- Stats ----
    mutable Stats m_stats;
    bool m_builtinsInitialized;
};

// ============================================================================
// Built-in Provider Factories
// ============================================================================
//
// These create provider instances backed by PDBManager, import tables, etc.
// Called by ReferenceRouter::initBuiltinProviders().
//

// Creates a provider that searches all loaded PDBs for public symbols.
IReferenceProvider* createPDBPublicProvider();

// Creates a provider that searches all loaded PDBs for procedures.
IReferenceProvider* createPDBProcedureProvider();

// Creates a provider that searches PE import tables.
IReferenceProvider* createImportTableProvider();

// Creates the instruction hook provider (reads .instructions.md files).
IReferenceProvider* createInstructionHookProvider(
    InstructionRule* rules, uint32_t* ruleCount, uint32_t maxRules);

} // namespace PDB
} // namespace RawrXD
