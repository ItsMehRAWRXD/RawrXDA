// ============================================================================
// pdb_reference_provider.cpp — Phase 29.2: Multi-Result Reference Provider
// ============================================================================
//
// PURPOSE:
//   "Find All References" implementation for PDB-resolved symbols.
//   Returns 1..N results per query. User can pick any subset (1, 2, 3, or all N).
//
//   Built-in providers:
//     1. PDBPublicSymbolProvider  — Searches public symbol tables across all PDBs
//     2. PDBProcedureProvider     — Searches procedure symbols (GPROC32/LPROC32)
//     3. ImportTableProvider      — Stub for PE import table cross-references
//     4. InstructionHookProvider  — Reads .instructions.md files and generates
//                                   synthetic references based on rule matching
//
//   Tools/Instructions Hook:
//     The InstructionHookProvider reads markdown/text instruction files at startup,
//     extracts pattern→reference rules, and applies them at query time. This enables
//     the model/agent to "hook" reference resolution: after reading the user's
//     request, the agent can route through instruction rules to add context-aware
//     references that static PDB analysis would miss.
//
//     Example: user queries "NtCreateFile" →
//       PDB provider finds: ntdll.dll!NtCreateFile
//       Instruction hook adds: ntoskrnl.exe!NtCreateFile (kernel-side)
//                              kernelbase.dll!CreateFileW (wrapper)
//       Total: 3 results, user picks 1, 2, or all 3
//
// PATTERN:   PDBResult-compatible, no exceptions, no std::function
// THREADING: Sequential dispatch on caller thread (Phase 29.3: parallel)
// RULE:      NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#include "../../include/pdb_reference_provider.h"
#include "../../include/pdb_native.h"

#include <cstdio>
#include <cstring>
#include <cctype>
#include <cstdlib>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace RawrXD {
namespace PDB {

// ============================================================================
// Static Instance Storage (Singleton)
// ============================================================================
static ReferenceRouter* s_routerInstance = nullptr;

ReferenceRouter& ReferenceRouter::instance() {
    if (!s_routerInstance) {
        s_routerInstance = new ReferenceRouter();
    }
    return *s_routerInstance;
}

// ============================================================================
// ReferenceRouter Constructor / Destructor
// ============================================================================
ReferenceRouter::ReferenceRouter()
    : m_providerCount(0)
    , m_instrFileCount(0)
    , m_instrRuleCount(0)
    , m_stats{}
    , m_builtinsInitialized(false) {
    memset(m_providers, 0, sizeof(m_providers));
    memset(m_instrFiles, 0, sizeof(m_instrFiles));
    memset(m_instrRules, 0, sizeof(m_instrRules));
}

ReferenceRouter::~ReferenceRouter() {
    // Free built-in providers (they were heap-allocated by factory functions)
    for (uint32_t i = 0; i < m_providerCount; ++i) {
        if (m_providers[i] && m_providers[i]->state) {
            // Provider state was allocated by the factory — free it
            free(m_providers[i]->state);
            m_providers[i]->state = nullptr;
        }
        if (m_providers[i]) {
            free(m_providers[i]);
            m_providers[i] = nullptr;
        }
    }
    m_providerCount = 0;
}

// ============================================================================
// Provider Management
// ============================================================================
PDBResult ReferenceRouter::addProvider(IReferenceProvider* provider) {
    if (!provider) {
        return PDBResult::error("Null provider", PDB_ERR_INVALID_FORMAT);
    }
    if (!provider->tag) {
        return PDBResult::error("Provider has no tag", PDB_ERR_INVALID_FORMAT);
    }
    if (m_providerCount >= MAX_PROVIDERS) {
        return PDBResult::error("Max providers reached", PDB_ERR_ALLOC_FAIL);
    }

    // Check for duplicate tag
    for (uint32_t i = 0; i < m_providerCount; ++i) {
        if (m_providers[i] && m_providers[i]->tag &&
            _stricmp(m_providers[i]->tag, provider->tag) == 0) {
            return PDBResult::error("Provider with this tag already registered", PDB_ERR_INVALID_FORMAT);
        }
    }

    m_providers[m_providerCount++] = provider;
    m_stats.providersActive = m_providerCount;

    char msg[256];
    snprintf(msg, sizeof(msg), "[Phase 29.2] Reference provider registered: '%s' (%s)",
             provider->tag, provider->description ? provider->description : "");
    OutputDebugStringA(msg);

    return PDBResult::ok("Provider registered");
}

void ReferenceRouter::removeProvider(const char* tag) {
    if (!tag) return;

    for (uint32_t i = 0; i < m_providerCount; ++i) {
        if (m_providers[i] && m_providers[i]->tag &&
            _stricmp(m_providers[i]->tag, tag) == 0) {
            // Shift remaining providers down
            for (uint32_t j = i; j + 1 < m_providerCount; ++j) {
                m_providers[j] = m_providers[j + 1];
            }
            m_providers[--m_providerCount] = nullptr;
            m_stats.providersActive = m_providerCount;

            char msg[256];
            snprintf(msg, sizeof(msg), "[Phase 29.2] Reference provider removed: '%s'", tag);
            OutputDebugStringA(msg);
            return;
        }
    }
}

const IReferenceProvider* ReferenceRouter::getProvider(uint32_t index) const {
    if (index >= m_providerCount) return nullptr;
    return m_providers[index];
}

// ============================================================================
// FNV-1a Hash for Reference Deduplication
// ============================================================================
uint64_t ReferenceRouter::hashReference(const ReferenceLocation& loc) {
    uint64_t hash = 14695981039346656037ULL;

    // Hash module name (case-insensitive)
    for (int i = 0; i < 64 && loc.moduleName[i]; ++i) {
        uint8_t c = static_cast<uint8_t>(tolower(static_cast<unsigned char>(loc.moduleName[i])));
        hash ^= c;
        hash *= 1099511628211ULL;
    }

    // Hash symbol name (case-insensitive)
    for (int i = 0; i < 256 && loc.symbolName[i]; ++i) {
        uint8_t c = static_cast<uint8_t>(tolower(static_cast<unsigned char>(loc.symbolName[i])));
        hash ^= c;
        hash *= 1099511628211ULL;
    }

    // Hash RVA
    for (int i = 0; i < 8; ++i) {
        hash ^= static_cast<uint8_t>((loc.rva >> (i * 8)) & 0xFF);
        hash *= 1099511628211ULL;
    }

    return hash;
}

// ============================================================================
// Deduplication — Remove duplicate references (same module + symbol + RVA)
// ============================================================================
void ReferenceRouter::deduplicateResults(ReferenceResult* result) {
    if (!result || result->count <= 1) return;

    // Build hash set (simple open-addressing with MAX_REFERENCES * 2 slots)
    static constexpr uint32_t HASH_TABLE_SIZE = MAX_REFERENCES * 2;
    uint64_t hashTable[HASH_TABLE_SIZE];
    memset(hashTable, 0, sizeof(hashTable));

    uint32_t writeIdx = 0;
    for (uint32_t i = 0; i < result->count; ++i) {
        uint64_t h = hashReference(result->refs[i]);
        if (h == 0) h = 1; // Avoid sentinel collision

        // Probe hash table
        uint32_t slot = static_cast<uint32_t>(h % HASH_TABLE_SIZE);
        bool duplicate = false;
        for (uint32_t probe = 0; probe < HASH_TABLE_SIZE; ++probe) {
            uint32_t idx = (slot + probe) % HASH_TABLE_SIZE;
            if (hashTable[idx] == 0) {
                // Empty slot — not a duplicate
                hashTable[idx] = h;
                break;
            }
            if (hashTable[idx] == h) {
                // Hash collision or duplicate — check full fields
                // For correctness, we'd need to store the full refs; for speed,
                // we accept rare false-positive dedup (hash collision). This is
                // acceptable because: (a) 64-bit FNV-1a has ~negligible collision
                // rate for <256 items, (b) user still gets N-1 of N results.
                duplicate = true;
                break;
            }
        }

        if (!duplicate) {
            if (writeIdx != i) {
                result->refs[writeIdx] = result->refs[i];
            }
            writeIdx++;
        }
    }

    result->count = writeIdx;
}

// ============================================================================
// Sort by Confidence (descending) — simple insertion sort for small N
// ============================================================================
void ReferenceRouter::sortByConfidence(ReferenceResult* result) {
    if (!result || result->count <= 1) return;

    // Insertion sort — O(n²) but n ≤ 256, so this is faster than qsort overhead
    for (uint32_t i = 1; i < result->count; ++i) {
        ReferenceLocation key = result->refs[i];
        uint32_t j = i;
        while (j > 0 && result->refs[j - 1].confidence < key.confidence) {
            result->refs[j] = result->refs[j - 1];
            j--;
        }
        result->refs[j] = key;
    }
}

// ============================================================================
// findAllReferences — Dispatch query to all providers, merge results
// ============================================================================
PDBResult ReferenceRouter::findAllReferences(const ReferenceQuery& query,
                                              ReferenceResult* result) {
    if (!result) {
        return PDBResult::error("Null result pointer", PDB_ERR_INVALID_FORMAT);
    }
    if (!query.symbolName || query.symbolNameLen == 0) {
        return PDBResult::error("Empty symbol name", PDB_ERR_SYMBOL_NOT_FOUND);
    }

    result->clear();
    m_stats.totalQueries++;

    // Dispatch to each registered provider
    uint32_t providerErrors = 0;
    for (uint32_t i = 0; i < m_providerCount; ++i) {
        if (!m_providers[i]) continue;

        // Check if provider is available
        if (m_providers[i]->isAvailable &&
            !m_providers[i]->isAvailable(m_providers[i]->state)) {
            continue;
        }

        // Skip synthetic providers if not requested
        if (!query.includeSynthetic &&
            m_providers[i]->tag &&
            _stricmp(m_providers[i]->tag, "hook") == 0) {
            continue;
        }

        // Query this provider
        PDBResult r = m_providers[i]->findReferences(
            m_providers[i]->state, &query, result);

        if (!r.success) {
            providerErrors++;
            char msg[256];
            snprintf(msg, sizeof(msg), "[Phase 29.2] Provider '%s' failed: %s",
                     m_providers[i]->tag ? m_providers[i]->tag : "?",
                     r.detail ? r.detail : "unknown");
            OutputDebugStringA(msg);
        }

        // Respect maxResults cap
        if (query.maxResults > 0 && result->count >= query.maxResults) {
            result->truncated = true;
            break;
        }
    }

    m_stats.providerErrors += providerErrors;

    // Deduplicate and sort
    deduplicateResults(result);
    sortByConfidence(result);

    // Apply maxResults cap after dedup (may have shrunk)
    if (query.maxResults > 0 && result->count > query.maxResults) {
        result->count = query.maxResults;
        result->truncated = true;
    }

    m_stats.totalResults += result->count;

    char msg[256];
    snprintf(msg, sizeof(msg),
             "[Phase 29.2] findAllReferences('%.*s'): %u results from %u providers "
             "(total available: %u, truncated: %s)",
             query.symbolNameLen > 64 ? 64 : query.symbolNameLen, query.symbolName,
             result->count, m_providerCount, result->totalAvailable,
             result->truncated ? "yes" : "no");
    OutputDebugStringA(msg);

    if (result->count == 0) {
        return PDBResult::error("No references found", PDB_ERR_SYMBOL_NOT_FOUND);
    }

    return PDBResult::ok("References found");
}

// ============================================================================
// findReferencesFrom — Query a single provider by tag
// ============================================================================
PDBResult ReferenceRouter::findReferencesFrom(const char* providerTag,
                                               const ReferenceQuery& query,
                                               ReferenceResult* result) {
    if (!result) {
        return PDBResult::error("Null result pointer", PDB_ERR_INVALID_FORMAT);
    }
    if (!providerTag) {
        return PDBResult::error("Null provider tag", PDB_ERR_INVALID_FORMAT);
    }

    result->clear();

    for (uint32_t i = 0; i < m_providerCount; ++i) {
        if (m_providers[i] && m_providers[i]->tag &&
            _stricmp(m_providers[i]->tag, providerTag) == 0) {
            return m_providers[i]->findReferences(
                m_providers[i]->state, &query, result);
        }
    }

    return PDBResult::error("Provider not found", PDB_ERR_SYMBOL_NOT_FOUND);
}

// ============================================================================
// Statistics
// ============================================================================
ReferenceRouter::Stats ReferenceRouter::getStats() const {
    m_stats.providersActive = m_providerCount;
    m_stats.instructionRulesLoaded = m_instrRuleCount;
    return m_stats;
}

// ============================================================================
// Instruction File Parser
// ============================================================================
//
// Parses markdown-style instruction files. Recognized patterns:
//
//   ## PatternName
//   - Kernel entry: module.dll!SymbolName
//   - User wrapper: module.dll!SymbolName
//   - Alt wrapper: module.dll!SymbolName
//   - Syscall stub: module.dll!SymbolName
//   - Type reference: module.dll!TypeName
//   - Import: module.dll!ImportName
//   - Export: module.dll!ExportName
//
// Lines starting with '#' set the current pattern.
// Lines starting with '- ' add rules under the current pattern.
// The keyword after '- ' determines the ReferenceKind.
//

static ReferenceKind parseRuleKind(const char* keyword) {
    if (!keyword) return ReferenceKind::CrossReference;

    if (_strnicmp(keyword, "kernel entry", 12) == 0) return ReferenceKind::KernelEntry;
    if (_strnicmp(keyword, "user wrapper", 12) == 0) return ReferenceKind::WrapperFunc;
    if (_strnicmp(keyword, "alt wrapper", 11) == 0)  return ReferenceKind::WrapperFunc;
    if (_strnicmp(keyword, "syscall stub", 12) == 0) return ReferenceKind::SyscallStub;
    if (_strnicmp(keyword, "syscall", 7) == 0)       return ReferenceKind::SyscallStub;
    if (_strnicmp(keyword, "type reference", 14) == 0) return ReferenceKind::TypeReference;
    if (_strnicmp(keyword, "type ref", 8) == 0)      return ReferenceKind::TypeReference;
    if (_strnicmp(keyword, "import", 6) == 0)        return ReferenceKind::Import;
    if (_strnicmp(keyword, "export", 6) == 0)        return ReferenceKind::Export;
    if (_strnicmp(keyword, "definition", 10) == 0)   return ReferenceKind::Definition;
    if (_strnicmp(keyword, "declaration", 11) == 0)  return ReferenceKind::Declaration;
    if (_strnicmp(keyword, "implementation", 14) == 0) return ReferenceKind::Implementation;
    if (_strnicmp(keyword, "cross reference", 15) == 0) return ReferenceKind::CrossReference;
    if (_strnicmp(keyword, "xref", 4) == 0)          return ReferenceKind::CrossReference;
    if (_strnicmp(keyword, "synthetic", 9) == 0)     return ReferenceKind::Synthetic;

    return ReferenceKind::CrossReference;
}

// Parse "module.dll!SymbolName" into module and symbol parts
static bool parseModuleSymbol(const char* str, char* moduleOut, uint32_t moduleMax,
                               char* symbolOut, uint32_t symbolMax) {
    if (!str) return false;

    const char* bang = strchr(str, '!');
    if (!bang) {
        // No '!' — treat entire string as symbol, module = ""
        moduleOut[0] = '\0';
        strncpy_s(symbolOut, symbolMax, str, _TRUNCATE);
        return true;
    }

    uint32_t modLen = static_cast<uint32_t>(bang - str);
    if (modLen >= moduleMax) modLen = moduleMax - 1;
    memcpy(moduleOut, str, modLen);
    moduleOut[modLen] = '\0';

    strncpy_s(symbolOut, symbolMax, bang + 1, _TRUNCATE);
    return true;
}

PDBResult ReferenceRouter::parseInstructionFile(const wchar_t* path,
                                                 InstructionFile* fileOut) {
    if (!path || !fileOut) {
        return PDBResult::error("Null path or file output", PDB_ERR_INVALID_FORMAT);
    }

    // Open and read the file
    HANDLE hFile = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ,
                               nullptr, OPEN_EXISTING, 0, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) {
        return PDBResult::error("Cannot open instruction file", PDB_ERR_FILE_OPEN);
    }

    LARGE_INTEGER fileSize;
    if (!GetFileSizeEx(hFile, &fileSize) || fileSize.QuadPart == 0 ||
        fileSize.QuadPart > 1024 * 1024) { // 1MB max
        CloseHandle(hFile);
        return PDBResult::error("Invalid instruction file size", PDB_ERR_INVALID_FORMAT);
    }

    uint32_t size = static_cast<uint32_t>(fileSize.QuadPart);
    char* buffer = static_cast<char*>(malloc(size + 1));
    if (!buffer) {
        CloseHandle(hFile);
        return PDBResult::error("Allocation failed", PDB_ERR_ALLOC_FAIL);
    }

    DWORD bytesRead = 0;
    BOOL ok = ReadFile(hFile, buffer, size, &bytesRead, nullptr);
    CloseHandle(hFile);

    if (!ok || bytesRead == 0) {
        free(buffer);
        return PDBResult::error("Read failed", PDB_ERR_FILE_OPEN);
    }

    buffer[bytesRead] = '\0';

    // Get last modified time for hot-reload detection
    WIN32_FILE_ATTRIBUTE_DATA fad;
    if (GetFileAttributesExW(path, GetFileExInfoStandard, &fad)) {
        ULARGE_INTEGER uli;
        uli.LowPart = fad.ftLastWriteTime.dwLowDateTime;
        uli.HighPart = fad.ftLastWriteTime.dwHighDateTime;
        fileOut->lastModifiedTime = uli.QuadPart;
    }

    // Parse line-by-line
    char currentPattern[128] = {};
    bool currentIsWildcard = false;
    uint32_t rulesAdded = 0;

    char* line = buffer;
    while (line && *line) {
        // Find end of line
        char* eol = strchr(line, '\n');
        if (eol) *eol = '\0';

        // Trim trailing \r
        size_t lineLen = strlen(line);
        if (lineLen > 0 && line[lineLen - 1] == '\r') {
            line[lineLen - 1] = '\0';
            lineLen--;
        }

        // Skip empty lines
        if (lineLen == 0) {
            line = eol ? eol + 1 : nullptr;
            continue;
        }

        // Pattern header: lines starting with ## (markdown H2)
        if (lineLen >= 3 && line[0] == '#' && line[1] == '#') {
            // Extract pattern name (skip ## and leading whitespace)
            const char* patternStart = line + 2;
            while (*patternStart == ' ' || *patternStart == '#') patternStart++;

            strncpy_s(currentPattern, sizeof(currentPattern), patternStart, _TRUNCATE);

            // Trim trailing whitespace
            size_t plen = strlen(currentPattern);
            while (plen > 0 && (currentPattern[plen - 1] == ' ' ||
                                 currentPattern[plen - 1] == '\t')) {
                currentPattern[--plen] = '\0';
            }

            // Check for wildcards
            currentIsWildcard = (strchr(currentPattern, '*') != nullptr ||
                                 strchr(currentPattern, '?') != nullptr);
        }
        // Rule line: starts with "- "
        else if (lineLen >= 3 && line[0] == '-' && line[1] == ' ' &&
                 currentPattern[0] != '\0') {
            // Parse "- keyword: module!symbol"
            const char* ruleStart = line + 2;

            // Find colon separator
            const char* colon = strchr(ruleStart, ':');
            if (colon && m_instrRuleCount < MAX_INSTRUCTION_RULES) {
                // Keyword is before the colon
                char keyword[64] = {};
                uint32_t kwLen = static_cast<uint32_t>(colon - ruleStart);
                if (kwLen >= sizeof(keyword)) kwLen = sizeof(keyword) - 1;
                memcpy(keyword, ruleStart, kwLen);
                keyword[kwLen] = '\0';

                // Value is after the colon (trim leading whitespace)
                const char* value = colon + 1;
                while (*value == ' ') value++;

                // Parse module!symbol
                InstructionRule& rule = m_instrRules[m_instrRuleCount];
                strncpy_s(rule.pattern, sizeof(rule.pattern), currentPattern, _TRUNCATE);
                rule.kind = parseRuleKind(keyword);
                rule.confidence = 0.7f; // Synthetic references get lower confidence
                rule.isWildcard = currentIsWildcard;

                if (parseModuleSymbol(value,
                                       rule.targetModule, sizeof(rule.targetModule),
                                       rule.targetSymbol, sizeof(rule.targetSymbol))) {
                    m_instrRuleCount++;
                    rulesAdded++;
                }
            }
        }

        line = eol ? eol + 1 : nullptr;
    }

    free(buffer);

    fileOut->ruleCount = rulesAdded;
    fileOut->loaded = true;

    char msg[256];
    snprintf(msg, sizeof(msg), "[Phase 29.2] Parsed instruction file: %u rules extracted", rulesAdded);
    OutputDebugStringA(msg);

    return PDBResult::ok("Instruction file parsed");
}

// ============================================================================
// loadInstructionFile — Register and parse an instruction file
// ============================================================================
PDBResult ReferenceRouter::loadInstructionFile(const wchar_t* path, const char* tag) {
    if (!path || !tag) {
        return PDBResult::error("Null path or tag", PDB_ERR_INVALID_FORMAT);
    }
    if (m_instrFileCount >= MAX_INSTRUCTION_FILES) {
        return PDBResult::error("Max instruction files reached", PDB_ERR_ALLOC_FAIL);
    }

    InstructionFile& f = m_instrFiles[m_instrFileCount];
    wcsncpy_s(f.path, MAX_PATH, path, _TRUNCATE);
    strncpy_s(f.tag, sizeof(f.tag), tag, _TRUNCATE);
    f.loaded = false;
    f.ruleCount = 0;

    PDBResult r = parseInstructionFile(path, &f);
    if (r.success) {
        m_instrFileCount++;
    }

    return r;
}

// ============================================================================
// reloadInstructionFiles — Re-parse all registered instruction files
// ============================================================================
PDBResult ReferenceRouter::reloadInstructionFiles() {
    // Clear existing rules
    m_instrRuleCount = 0;
    memset(m_instrRules, 0, sizeof(m_instrRules));

    uint32_t reloaded = 0;
    uint32_t errors = 0;

    for (uint32_t i = 0; i < m_instrFileCount; ++i) {
        InstructionFile& f = m_instrFiles[i];
        f.ruleCount = 0;
        f.loaded = false;

        PDBResult r = parseInstructionFile(f.path, &f);
        if (r.success) {
            reloaded++;
        } else {
            errors++;
        }
    }

    char msg[256];
    snprintf(msg, sizeof(msg),
             "[Phase 29.2] Reloaded %u/%u instruction files (%u total rules)",
             reloaded, m_instrFileCount, m_instrRuleCount);
    OutputDebugStringA(msg);

    if (errors > 0 && reloaded == 0) {
        return PDBResult::error("All instruction files failed to reload", PDB_ERR_FILE_OPEN);
    }

    return PDBResult::ok("Instruction files reloaded");
}

// ============================================================================
// Pattern Matching — Exact or wildcard (simple '*' and '?' support)
// ============================================================================
bool ReferenceRouter::matchPattern(const char* pattern, const char* name,
                                    bool isWildcard) const {
    if (!pattern || !name) return false;

    if (!isWildcard) {
        // Exact case-insensitive match
        return _stricmp(pattern, name) == 0;
    }

    // Simple wildcard matching:
    //   '*' matches zero or more characters
    //   '?' matches exactly one character
    const char* p = pattern;
    const char* n = name;
    const char* starP = nullptr;
    const char* starN = nullptr;

    while (*n) {
        if (*p == '?' || tolower(static_cast<unsigned char>(*p)) ==
                         tolower(static_cast<unsigned char>(*n))) {
            p++;
            n++;
        } else if (*p == '*') {
            starP = p++;
            starN = n;
        } else if (starP) {
            p = starP + 1;
            n = ++starN;
        } else {
            return false;
        }
    }

    while (*p == '*') p++;
    return *p == '\0';
}

// ============================================================================
// ============================================================================
//
//   BUILT-IN PROVIDER IMPLEMENTATIONS
//
// ============================================================================
// ============================================================================

// ============================================================================
// PDB Public Symbol Provider
// ============================================================================
//
// Walks all loaded PDBs via PDBManager, enumerates public symbols,
// and matches against the query symbol name.
//

struct PDBPublicProviderState {
    // Context passed to the enumeration visitor
    const ReferenceQuery*   query;
    ReferenceResult*        result;
    const char*             moduleName;
};

static bool publicSymbolVisitor(const ResolvedSymbol* sym, void* userData) {
    auto* ctx = static_cast<PDBPublicProviderState*>(userData);
    if (!sym || !sym->name || !ctx->query || !ctx->result) return true;

    // Case-insensitive substring match against the query
    // For exact match: use _stricmp
    // For substring: use stristr pattern
    bool match = false;

    // Try exact match first
    if (_stricmp(sym->name, ctx->query->symbolName) == 0) {
        match = true;
    }
    // Then try substring match (for partial queries like "CreateFile" matching "CreateFileW")
    else {
        // Case-insensitive substring search
        const char* haystack = sym->name;
        const char* needle = ctx->query->symbolName;
        uint32_t needleLen = ctx->query->symbolNameLen;

        for (const char* p = haystack; *p; ++p) {
            if (_strnicmp(p, needle, needleLen) == 0) {
                match = true;
                break;
            }
        }
    }

    if (!match) return true; // Continue iteration

    // Build a ReferenceLocation
    ReferenceLocation loc{};
    if (ctx->moduleName) {
        strncpy_s(loc.moduleName, sizeof(loc.moduleName), ctx->moduleName, _TRUNCATE);
    }
    strncpy_s(loc.symbolName, sizeof(loc.symbolName), sym->name, _TRUNCATE);
    loc.rva = sym->rva;
    loc.section = sym->section;
    loc.sectionOffset = sym->sectionOffset;
    loc.size = sym->size;
    loc.isFunction = sym->isFunction;
    loc.providerTag = "pdb-public";

    // Set kind based on symbol type
    if (sym->isPublic) {
        loc.kind = ReferenceKind::Export;
    } else {
        loc.kind = ReferenceKind::Definition;
    }

    // Confidence: exact match = 1.0, substring = 0.8
    if (_stricmp(sym->name, ctx->query->symbolName) == 0) {
        loc.confidence = 1.0f;
    } else {
        loc.confidence = 0.8f;
    }

    snprintf(loc.detail, sizeof(loc.detail), "Public symbol in %s (RVA 0x%llX)",
             ctx->moduleName ? ctx->moduleName : "?",
             static_cast<unsigned long long>(sym->rva));

    ctx->result->addRef(loc);
    return true; // Continue iteration
}

static PDBResult pdbPublicFindReferences(void* provider,
                                          const ReferenceQuery* query,
                                          ReferenceResult* result) {
    (void)provider; // State not used — we go through PDBManager singleton

    if (!query || !result) {
        return PDBResult::error("Null query or result", PDB_ERR_INVALID_FORMAT);
    }

    PDBManager& pdb = PDBManager::instance();
    uint32_t moduleCount = pdb.getLoadedModuleCount();

    if (moduleCount == 0) {
        return PDBResult::ok("No PDBs loaded");
    }

    for (uint32_t i = 0; i < moduleCount; ++i) {
        const char* moduleName = pdb.getLoadedModuleName(i);
        if (!moduleName) continue;

        // If query restricts to a specific module, skip others
        if (query->contextModule && _stricmp(query->contextModule, moduleName) != 0) {
            continue;
        }

        const NativePDBParser* parser = pdb.getParser(moduleName);
        if (!parser) continue;

        PDBPublicProviderState ctx;
        ctx.query = query;
        ctx.result = result;
        ctx.moduleName = moduleName;

        parser->enumeratePublicSymbols(publicSymbolVisitor, &ctx);
    }

    return PDBResult::ok("Public symbol search complete");
}

static uint32_t pdbPublicGetIndexedCount(void* provider) {
    (void)provider;
    PDBManager& pdb = PDBManager::instance();
    PDBManager::Stats stats = pdb.getStats();
    return static_cast<uint32_t>(stats.symbolsIndexed);
}

static bool pdbPublicIsAvailable(void* provider) {
    (void)provider;
    PDBManager& pdb = PDBManager::instance();
    return pdb.getLoadedModuleCount() > 0;
}

IReferenceProvider* createPDBPublicProvider() {
    auto* p = static_cast<IReferenceProvider*>(calloc(1, sizeof(IReferenceProvider)));
    if (!p) return nullptr;

    p->tag = "pdb-public";
    p->description = "PDB public symbols across all loaded modules";
    p->findReferences = pdbPublicFindReferences;
    p->getIndexedCount = pdbPublicGetIndexedCount;
    p->isAvailable = pdbPublicIsAvailable;
    p->state = nullptr; // Uses PDBManager singleton directly

    return p;
}

// ============================================================================
// PDB Procedure Provider
// ============================================================================
//
// Similar to public provider but enumerates GPROC32/LPROC32 records.
// These give function boundaries (start + length), which public symbols lack.
//

static bool procedureVisitor(const ResolvedSymbol* sym, void* userData) {
    auto* ctx = static_cast<PDBPublicProviderState*>(userData);
    if (!sym || !sym->name || !ctx->query || !ctx->result) return true;

    // Only match functions
    if (!sym->isFunction) return true;

    // Same matching logic as public provider
    bool match = (_stricmp(sym->name, ctx->query->symbolName) == 0);
    if (!match) {
        const char* needle = ctx->query->symbolName;
        uint32_t needleLen = ctx->query->symbolNameLen;
        for (const char* p = sym->name; *p; ++p) {
            if (_strnicmp(p, needle, needleLen) == 0) {
                match = true;
                break;
            }
        }
    }

    if (!match) return true;

    ReferenceLocation loc{};
    if (ctx->moduleName) {
        strncpy_s(loc.moduleName, sizeof(loc.moduleName), ctx->moduleName, _TRUNCATE);
    }
    strncpy_s(loc.symbolName, sizeof(loc.symbolName), sym->name, _TRUNCATE);
    loc.rva = sym->rva;
    loc.section = sym->section;
    loc.sectionOffset = sym->sectionOffset;
    loc.size = sym->size;
    loc.isFunction = true;
    loc.providerTag = "pdb-proc";
    loc.kind = ReferenceKind::Implementation;

    // Exact match confidence
    if (_stricmp(sym->name, ctx->query->symbolName) == 0) {
        loc.confidence = 0.95f; // Slightly below public (to prefer exported names)
    } else {
        loc.confidence = 0.75f;
    }

    snprintf(loc.detail, sizeof(loc.detail), "Procedure in %s (%u bytes at RVA 0x%llX)",
             ctx->moduleName ? ctx->moduleName : "?",
             sym->size, static_cast<unsigned long long>(sym->rva));

    ctx->result->addRef(loc);
    return true;
}

static PDBResult pdbProcFindReferences(void* provider,
                                        const ReferenceQuery* query,
                                        ReferenceResult* result) {
    (void)provider;

    if (!query || !result) {
        return PDBResult::error("Null query or result", PDB_ERR_INVALID_FORMAT);
    }

    PDBManager& pdb = PDBManager::instance();
    uint32_t moduleCount = pdb.getLoadedModuleCount();

    for (uint32_t i = 0; i < moduleCount; ++i) {
        const char* moduleName = pdb.getLoadedModuleName(i);
        if (!moduleName) continue;

        if (query->contextModule && _stricmp(query->contextModule, moduleName) != 0) {
            continue;
        }

        const NativePDBParser* parser = pdb.getParser(moduleName);
        if (!parser) continue;

        PDBPublicProviderState ctx;
        ctx.query = query;
        ctx.result = result;
        ctx.moduleName = moduleName;

        parser->enumerateProcedures(procedureVisitor, &ctx);
    }

    return PDBResult::ok("Procedure search complete");
}

static uint32_t pdbProcGetIndexedCount(void* provider) {
    (void)provider;
    return 0; // TODO: track procedure count separately
}

static bool pdbProcIsAvailable(void* provider) {
    (void)provider;
    PDBManager& pdb = PDBManager::instance();
    return pdb.getLoadedModuleCount() > 0;
}

IReferenceProvider* createPDBProcedureProvider() {
    auto* p = static_cast<IReferenceProvider*>(calloc(1, sizeof(IReferenceProvider)));
    if (!p) return nullptr;

    p->tag = "pdb-proc";
    p->description = "PDB procedure symbols (GPROC32/LPROC32) with function boundaries";
    p->findReferences = pdbProcFindReferences;
    p->getIndexedCount = pdbProcGetIndexedCount;
    p->isAvailable = pdbProcIsAvailable;
    p->state = nullptr;

    return p;
}

// ============================================================================
// Import Table Provider
// ============================================================================
//
// Stub: In Phase 29.3, this will walk PE import tables to find all modules
// that import a given symbol. For now, returns no results but is registered
// so the provider slot is reserved and the architecture is proven.
//

struct ImportTableProviderState {
    bool initialized;
};

static PDBResult importTableFindReferences(void* provider,
                                            const ReferenceQuery* query,
                                            ReferenceResult* result) {
    auto* state = static_cast<ImportTableProviderState*>(provider);
    (void)state;
    (void)query;
    (void)result;

    // Phase 29.3: Walk loaded PE import tables and match against query.symbolName
    // For now, this is a stub that always succeeds with zero results.

    return PDBResult::ok("Import table search complete (stub)");
}

static uint32_t importTableGetIndexedCount(void* provider) {
    (void)provider;
    return 0;
}

static bool importTableIsAvailable(void* provider) {
    auto* state = static_cast<ImportTableProviderState*>(provider);
    return state && state->initialized;
}

IReferenceProvider* createImportTableProvider() {
    auto* p = static_cast<IReferenceProvider*>(calloc(1, sizeof(IReferenceProvider)));
    if (!p) return nullptr;

    auto* state = static_cast<ImportTableProviderState*>(calloc(1, sizeof(ImportTableProviderState)));
    if (!state) {
        free(p);
        return nullptr;
    }
    state->initialized = true;

    p->tag = "import";
    p->description = "PE import table cross-references (Phase 29.3)";
    p->findReferences = importTableFindReferences;
    p->getIndexedCount = importTableGetIndexedCount;
    p->isAvailable = importTableIsAvailable;
    p->state = state;

    return p;
}

// ============================================================================
// Instruction Hook Provider
// ============================================================================
//
// This is the "tools/instructions" hook. It reads the instruction rules
// that were parsed from .instructions.md files and applies them at query time.
//
// Flow:
//   1. Query comes in: "NtCreateFile"
//   2. Walk all rules, match rule.pattern against query.symbolName
//   3. For each match: create a synthetic ReferenceLocation
//   4. These synthetic references augment the PDB-derived results
//
// This enables the agent/model to "hook" reference resolution by defining
// rules in instruction files. The user loads instruction files once, and
// every subsequent reference query benefits from the rules.
//

struct InstructionHookState {
    InstructionRule*    rules;
    uint32_t*           ruleCount;
    uint32_t            maxRules;
};

static PDBResult instructionHookFindReferences(void* provider,
                                                const ReferenceQuery* query,
                                                ReferenceResult* result) {
    auto* state = static_cast<InstructionHookState*>(provider);
    if (!state || !state->rules || !state->ruleCount) {
        return PDBResult::ok("Instruction hook not initialized");
    }

    if (!query || !result) {
        return PDBResult::error("Null query or result", PDB_ERR_INVALID_FORMAT);
    }

    // Skip if synthetic references not requested
    if (!query->includeSynthetic) {
        return PDBResult::ok("Synthetic references disabled");
    }

    uint32_t count = *state->ruleCount;
    const ReferenceRouter& router = ReferenceRouter::instance();

    for (uint32_t i = 0; i < count; ++i) {
        const InstructionRule& rule = state->rules[i];

        // Match the rule pattern against the query symbol name
        if (!router.matchPattern(rule.pattern, query->symbolName, rule.isWildcard)) {
            continue;
        }

        // Build a synthetic reference
        ReferenceLocation loc{};
        strncpy_s(loc.moduleName, sizeof(loc.moduleName), rule.targetModule, _TRUNCATE);
        strncpy_s(loc.symbolName, sizeof(loc.symbolName), rule.targetSymbol, _TRUNCATE);
        loc.rva = 0; // Unknown for synthetic references
        loc.section = 0;
        loc.sectionOffset = 0;
        loc.size = 0;
        loc.isFunction = (rule.kind == ReferenceKind::KernelEntry ||
                          rule.kind == ReferenceKind::WrapperFunc ||
                          rule.kind == ReferenceKind::SyscallStub ||
                          rule.kind == ReferenceKind::Implementation);
        loc.kind = rule.kind;
        loc.confidence = rule.confidence;
        loc.providerTag = "hook";

        snprintf(loc.detail, sizeof(loc.detail), "Instruction rule: %s → %s!%s",
                 rule.pattern, rule.targetModule, rule.targetSymbol);

        result->addRef(loc);
    }

    return PDBResult::ok("Instruction hook search complete");
}

static uint32_t instructionHookGetIndexedCount(void* provider) {
    auto* state = static_cast<InstructionHookState*>(provider);
    if (!state || !state->ruleCount) return 0;
    return *state->ruleCount;
}

static bool instructionHookIsAvailable(void* provider) {
    auto* state = static_cast<InstructionHookState*>(provider);
    return state && state->ruleCount && *state->ruleCount > 0;
}

IReferenceProvider* createInstructionHookProvider(
    InstructionRule* rules, uint32_t* ruleCount, uint32_t maxRules) {
    auto* p = static_cast<IReferenceProvider*>(calloc(1, sizeof(IReferenceProvider)));
    if (!p) return nullptr;

    auto* state = static_cast<InstructionHookState*>(calloc(1, sizeof(InstructionHookState)));
    if (!state) {
        free(p);
        return nullptr;
    }
    state->rules = rules;
    state->ruleCount = ruleCount;
    state->maxRules = maxRules;

    p->tag = "hook";
    p->description = "AI/model instruction hook — generates synthetic references from .instructions.md rules";
    p->findReferences = instructionHookFindReferences;
    p->getIndexedCount = instructionHookGetIndexedCount;
    p->isAvailable = instructionHookIsAvailable;
    p->state = state;

    return p;
}

// ============================================================================
// initBuiltinProviders — Create and register all built-in providers
// ============================================================================
void ReferenceRouter::initBuiltinProviders() {
    if (m_builtinsInitialized) return;

    OutputDebugStringA("[Phase 29.2] Initializing built-in reference providers...");

    // 1. PDB Public Symbols
    IReferenceProvider* pubProvider = createPDBPublicProvider();
    if (pubProvider) {
        addProvider(pubProvider);
    }

    // 2. PDB Procedures
    IReferenceProvider* procProvider = createPDBProcedureProvider();
    if (procProvider) {
        addProvider(procProvider);
    }

    // 3. Import Table (stub for Phase 29.3)
    IReferenceProvider* importProvider = createImportTableProvider();
    if (importProvider) {
        addProvider(importProvider);
    }

    // 4. Instruction Hook (uses router's own rule storage)
    IReferenceProvider* hookProvider = createInstructionHookProvider(
        m_instrRules, &m_instrRuleCount, MAX_INSTRUCTION_RULES);
    if (hookProvider) {
        addProvider(hookProvider);
    }

    m_builtinsInitialized = true;

    char msg[256];
    snprintf(msg, sizeof(msg),
             "[Phase 29.2] Built-in providers initialized: %u providers active",
             m_providerCount);
    OutputDebugStringA(msg);
}

} // namespace PDB
} // namespace RawrXD
