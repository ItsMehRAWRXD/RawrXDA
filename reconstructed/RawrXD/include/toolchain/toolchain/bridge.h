/*==========================================================================
 * RawrXD Toolchain Bridge — IDE ↔ from_scratch toolchain integration
 *
 * Bridges the custom assembler (phase1: x64_encoder) and linker
 * (phase2: coff_reader → section_merge → pe_writer) into the IDE's
 * LSP layer, diagnostics provider, and build task system.
 *
 * Architecture:
 *   ┌─────────────────────────────────────────────────────┐
 *   │  IDE Orchestrator / LSP / Completion Engine         │
 *   └────────┬──────────────────────────────┬─────────────┘
 *            │ C++ API                      │ C ABI (MASM bridge)
 *   ┌────────▼──────────────────────────────▼─────────────┐
 *   │  ToolchainBridge                                    │
 *   │   ├─ assemble()        → COFF .obj in memory       │
 *   │   ├─ link()            → PE .exe on disk            │
 *   │   ├─ getDiagnostics()  → errors/warnings per line   │
 *   │   ├─ getSymbols()      → label/proc symbol list     │
 *   │   ├─ getHoverInfo()    → encoding details at pos    │
 *   │   └─ buildProject()    → full asm→obj→exe pipeline  │
 *   └────────┬──────────────────────────────┬─────────────┘
 *            │                              │
 *   ┌────────▼────────┐          ┌──────────▼─────────────┐
 *   │  x64_encoder.h  │          │  coff_reader.h         │
 *   │  (Phase 1)      │          │  section_merge.h       │
 *   └─────────────────┘          │  pe_writer.h           │
 *                                │  (Phase 2)             │
 *                                └────────────────────────┘
 *
 * Thread Safety:
 *   - Each ToolchainSession owns its own assembler/linker state
 *   - Multiple sessions can run concurrently (per-file builds)
 *   - getDiagnostics() is lock-free for incremental re-parse
 *
 * Performance Targets (per tools.instructions.md):
 *   - Assemble single file:  <50ms for files up to 10K lines
 *   - getDiagnostics():      <100ms (LSP diagnostic latency)
 *   - getHoverInfo():        <10ms
 *   - Full build pipeline:   <500ms for typical projects
 *=========================================================================*/
#ifndef TOOLCHAIN_BRIDGE_H
#define TOOLCHAIN_BRIDGE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =========================================================================
 * Error / Diagnostic types
 * ========================================================================= */

typedef enum {
    TC_SEV_ERROR   = 1,
    TC_SEV_WARNING = 2,
    TC_SEV_INFO    = 3,
    TC_SEV_HINT    = 4
} tc_severity_t;

typedef enum {
    TC_OK                   = 0,
    TC_ERR_FILE_NOT_FOUND   = 1,
    TC_ERR_PARSE            = 2,
    TC_ERR_ENCODE           = 3,
    TC_ERR_LINK             = 4,
    TC_ERR_OUT_OF_MEMORY    = 5,
    TC_ERR_INVALID_SESSION  = 6,
    TC_ERR_INVALID_ARGUMENT = 7,
    TC_ERR_INTERNAL         = 99
} tc_error_t;

/* ---- Diagnostic: a single error/warning at a source location ---- */
typedef struct {
    const char    *file;        /* source file path (interned, do not free) */
    uint32_t       line;        /* 1-based line number */
    uint32_t       col;         /* 1-based column (0 = whole line) */
    uint32_t       end_col;     /* end column for range highlight (0 = EOL) */
    tc_severity_t  severity;
    const char    *code;        /* error code, e.g. "A2008" (interned) */
    const char    *message;     /* human-readable message (interned) */
} tc_diagnostic_t;

/* ---- Symbol: an assembled label, proc, or extern ---- */
typedef enum {
    TC_SYM_LABEL   = 0,
    TC_SYM_PROC    = 1,
    TC_SYM_EXTERN  = 2,
    TC_SYM_DATA    = 3,
    TC_SYM_EQUATE  = 4,
    TC_SYM_MACRO   = 5,
    TC_SYM_STRUCT  = 6,
    TC_SYM_SECTION = 7
} tc_symbol_kind_t;

typedef struct {
    const char       *name;       /* symbol name (interned) */
    tc_symbol_kind_t  kind;
    const char       *file;       /* defining file path */
    uint32_t          line;       /* 1-based definition line */
    uint32_t          col;        /* 1-based column */
    uint32_t          section;    /* section index (0xFFFF = none) */
    uint64_t          value;      /* offset / value / RVA */
    uint32_t          size;       /* symbol size in bytes (0 = unknown) */
    const char       *detail;     /* hover detail string */
} tc_symbol_t;

/* ---- Hover info for a position in source ---- */
typedef struct {
    const char *markdown;       /* Markdown-formatted hover text (allocated) */
    uint32_t    start_line;     /* highlight range */
    uint32_t    start_col;
    uint32_t    end_line;
    uint32_t    end_col;
} tc_hover_t;

/* ---- Encoded instruction info (for instruction-level hover) ---- */
typedef struct {
    uint8_t     bytes[15];      /* encoded machine code */
    uint8_t     len;            /* byte count */
    uint32_t    offset;         /* file offset within section */
    const char *mnemonic;       /* instruction mnemonic */
    const char *encoding_detail;/* human-readable encoding breakdown */
} tc_instruction_t;

/* ---- Completion item from symbol table ---- */
typedef struct {
    const char       *label;        /* display label */
    const char       *insert_text;  /* text to insert */
    const char       *detail;       /* type / signature info */
    const char       *documentation;/* doc string */
    tc_symbol_kind_t  kind;
    float             score;        /* relevance score 0.0–1.0 */
} tc_completion_t;

/* ---- Assembled object result (in-memory COFF) ---- */
typedef struct {
    uint8_t    *data;           /* raw COFF bytes (malloc'd, caller frees) */
    size_t      size;           /* COFF size in bytes */
    const char *source_file;    /* source .asm path */
} tc_object_t;

/* ---- Build result ---- */
typedef struct {
    tc_error_t       status;
    const char      *output_path;       /* path to output .exe/.obj */
    uint64_t         output_size;       /* output file size */
    double           elapsed_ms;        /* build time in milliseconds */
    tc_diagnostic_t *diagnostics;       /* array of diagnostics */
    uint32_t         num_diagnostics;
    uint32_t         num_errors;
    uint32_t         num_warnings;
} tc_build_result_t;

/* ---- Build configuration ---- */
typedef struct {
    const char  *source_file;       /* input .asm file path */
    const char  *output_path;       /* output .exe path (NULL = auto) */
    const char  *entry_point;       /* entry symbol (NULL = "_start") */
    uint16_t     subsystem;         /* PE subsystem (3=CONSOLE, 2=GUI) */
    uint64_t     image_base;        /* PE image base (0 = default) */
    uint64_t     stack_reserve;     /* stack reserve (0 = default 1MB) */
    uint64_t     stack_commit;      /* stack commit (0 = default 4KB) */
    bool         debug_info;        /* emit debug info */
    bool         verbose;           /* verbose output */
    const char **include_paths;     /* NULL-terminated array of include dirs */
    const char **lib_paths;         /* NULL-terminated array of lib directories */
    const char **libraries;         /* NULL-terminated array of .lib names */
} tc_build_config_t;


/* =========================================================================
 * Session API — per-file editing/build context
 * ========================================================================= */

typedef struct tc_session tc_session_t;

/* Create a new toolchain session for a source file */
tc_session_t *tc_session_create(const char *source_file);

/* Destroy session and free all resources */
void tc_session_destroy(tc_session_t *session);

/* Update source content (incremental editing — replaces full buffer) */
tc_error_t tc_session_update_source(tc_session_t *session,
                                     const char *content,
                                     size_t length);

/* Incremental update: replace a range in the source buffer */
tc_error_t tc_session_update_range(tc_session_t *session,
                                    uint32_t start_line, uint32_t start_col,
                                    uint32_t end_line,   uint32_t end_col,
                                    const char *new_text, size_t new_len);

/* =========================================================================
 * Assembly API
 * ========================================================================= */

/* Assemble current source to in-memory COFF object.
 * Returns TC_OK on success. Diagnostics are populated even on failure.
 * Caller must free obj->data when done.  */
tc_error_t tc_assemble(tc_session_t *session, tc_object_t *obj);

/* Assemble from file on disk (convenience wrapper) */
tc_error_t tc_assemble_file(const char *source_path, tc_object_t *obj,
                             tc_diagnostic_t **diags, uint32_t *num_diags);

/* =========================================================================
 * Linker API
 * ========================================================================= */

/* Link one or more COFF objects to PE executable */
tc_error_t tc_link(tc_object_t *objects, uint32_t num_objects,
                    const tc_build_config_t *config,
                    tc_build_result_t *result);

/* =========================================================================
 * Full Build Pipeline — assemble + link in one call
 * ========================================================================= */

tc_error_t tc_build(const tc_build_config_t *config,
                     tc_build_result_t *result);

/* Build a multi-file project */
tc_error_t tc_build_project(const tc_build_config_t *configs,
                             uint32_t num_files,
                             tc_build_result_t *result);

/* =========================================================================
 * LSP Provider API — diagnostics, symbols, hover, completion
 * ========================================================================= */

/* Get diagnostics for current session (re-parses if dirty).
 * Returns array valid until next tc_session_update_* or tc_session_destroy. */
tc_error_t tc_get_diagnostics(tc_session_t *session,
                               tc_diagnostic_t **out_diags,
                               uint32_t *out_count);

/* Get all symbols defined in the current session's source */
tc_error_t tc_get_symbols(tc_session_t *session,
                           tc_symbol_t **out_symbols,
                           uint32_t *out_count);

/* Find symbol at a source position (for go-to-definition) */
tc_error_t tc_find_symbol_at(tc_session_t *session,
                              uint32_t line, uint32_t col,
                              tc_symbol_t *out_symbol);

/* Get all references to a symbol name */
tc_error_t tc_find_references(tc_session_t *session,
                               const char *symbol_name,
                               tc_symbol_t **out_refs,
                               uint32_t *out_count);

/* Get hover information at a source position */
tc_error_t tc_get_hover(tc_session_t *session,
                         uint32_t line, uint32_t col,
                         tc_hover_t *out_hover);

/* Get completions at cursor position */
tc_error_t tc_get_completions(tc_session_t *session,
                               uint32_t line, uint32_t col,
                               const char *trigger_char,
                               tc_completion_t **out_items,
                               uint32_t *out_count);

/* Get encoded instruction info at a source line (for disassembly hover) */
tc_error_t tc_get_instruction(tc_session_t *session,
                               uint32_t line,
                               tc_instruction_t *out_instr);

/* =========================================================================
 * Memory management helpers
 * ========================================================================= */

void tc_free_object(tc_object_t *obj);
void tc_free_build_result(tc_build_result_t *result);
void tc_free_hover(tc_hover_t *hover);
void tc_free_completions(tc_completion_t *items, uint32_t count);
void tc_free_symbols(tc_symbol_t *syms, uint32_t count);

/* =========================================================================
 * Logging / metrics callbacks (connect to IDE logging infrastructure)
 * ========================================================================= */

typedef void (*tc_log_fn)(int level, const char *component, const char *message);
typedef void (*tc_metric_fn)(const char *name, int64_t value_ns);

void tc_set_log_callback(tc_log_fn fn);
void tc_set_metric_callback(tc_metric_fn fn);

/* =========================================================================
 * Version / capability query
 * ========================================================================= */

typedef struct {
    uint32_t    major;
    uint32_t    minor;
    uint32_t    patch;
    const char *build_date;
    bool        has_assembler;       /* phase1 available */
    bool        has_linker;          /* phase2 available */
    bool        has_import_resolver; /* phase3 available */
    bool        has_resources;       /* phase4 available */
    bool        has_debug_info;      /* phase5 available */
    bool        has_lib_manager;     /* phase6 available */
} tc_version_t;

tc_version_t tc_get_version(void);

#ifdef __cplusplus
}
#endif

/* ========================================================================= */
/* C++ wrapper (IDE-facing, integrates with LanguageServerIntegrationImpl)    */
/* ========================================================================= */
#ifdef __cplusplus

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <chrono>
#include <mutex>
#include <atomic>
#include <unordered_map>

namespace RawrXD {
namespace Toolchain {

/* ---- C++ diagnostic matching IDE's Diagnostic struct ---- */
struct ToolchainDiagnostic {
    std::string file;
    uint32_t    line;
    uint32_t    col;
    uint32_t    endCol;
    std::string severity;   /* "error", "warning", "info", "hint" */
    std::string code;
    std::string message;
};

/* ---- C++ symbol matching IDE's SymbolInformation struct ---- */
struct ToolchainSymbol {
    std::string name;
    std::string kind;       /* "label", "proc", "extern", "data", "equate" */
    std::string file;
    uint32_t    line;
    uint32_t    col;
    uint64_t    value;
    uint32_t    size;
    std::string detail;
};

/* ---- C++ build result ---- */
struct BuildResult {
    bool        success;
    std::string outputPath;
    uint64_t    outputSize;
    double      elapsedMs;
    std::vector<ToolchainDiagnostic> diagnostics;
    uint32_t    errorCount;
    uint32_t    warningCount;
};

/* ---- C++ build configuration ---- */
struct BuildConfig {
    std::string              sourceFile;
    std::string              outputPath;
    std::string              entryPoint   = "_start";
    uint16_t                 subsystem    = 3; /* CONSOLE */
    uint64_t                 imageBase    = 0;
    bool                     debugInfo    = false;
    bool                     verbose      = false;
    std::vector<std::string> includePaths;
    std::vector<std::string> libPaths;
    std::vector<std::string> libraries    = {"kernel32.lib"};
};

/* =========================================================================
 * ToolchainBridge — main C++ integration class
 *
 * Usage from IDE orchestrator:
 *   auto bridge = std::make_shared<ToolchainBridge>();
 *   bridge->initialize();
 *
 *   // During editing (on content change):
 *   bridge->updateSource("file.asm", content);
 *   auto diags = bridge->getDiagnostics("file.asm");
 *   // diags → feed into LSP publishDiagnostics
 *
 *   // On Ctrl+Shift+B (build):
 *   BuildConfig cfg;
 *   cfg.sourceFile = "file.asm";
 *   auto result = bridge->build(cfg);
 *
 *   // On hover:
 *   auto hover = bridge->getHoverInfo("file.asm", line, col);
 *
 *   // On completion:
 *   auto items = bridge->getCompletions("file.asm", line, col, "");
 * ========================================================================= */
class ToolchainBridge {
public:
    ToolchainBridge();
    ~ToolchainBridge();

    /* ---- Lifecycle ---- */
    bool initialize();
    void shutdown();
    bool isInitialized() const { return m_initialized.load(); }

    /* ---- Source management ---- */
    void openFile(const std::string& filePath, const std::string& content);
    void updateSource(const std::string& filePath, const std::string& content);
    void updateRange(const std::string& filePath,
                     uint32_t startLine, uint32_t startCol,
                     uint32_t endLine,   uint32_t endCol,
                     const std::string& newText);
    void closeFile(const std::string& filePath);

    /* ---- LSP Providers ---- */
    std::vector<ToolchainDiagnostic> getDiagnostics(const std::string& filePath);
    std::vector<ToolchainSymbol>     getSymbols(const std::string& filePath);
    ToolchainSymbol                  findDefinition(const std::string& filePath,
                                                     uint32_t line, uint32_t col);
    std::vector<ToolchainSymbol>     findReferences(const std::string& filePath,
                                                     const std::string& symbolName);
    std::string                      getHoverInfo(const std::string& filePath,
                                                   uint32_t line, uint32_t col);
    std::vector<tc_completion_t>     getCompletions(const std::string& filePath,
                                                     uint32_t line, uint32_t col,
                                                     const std::string& trigger);

    /* ---- Build ---- */
    BuildResult build(const BuildConfig& config);
    BuildResult buildProject(const std::vector<BuildConfig>& configs);

    /* ---- Instruction-level introspection ---- */
    tc_instruction_t getInstruction(const std::string& filePath, uint32_t line);

    /* ---- Metrics ---- */
    struct Metrics {
        uint64_t totalAssemblies;
        uint64_t totalLinks;
        uint64_t totalDiagnosticQueries;
        uint64_t totalHoverQueries;
        uint64_t totalCompletionQueries;
        double   avgAssembleTimeMs;
        double   avgDiagnosticTimeMs;
        double   avgHoverTimeMs;
    };
    Metrics getMetrics() const;

    /* ---- Callbacks ---- */
    using DiagnosticCallback = std::function<void(const std::string& file,
                                                   const std::vector<ToolchainDiagnostic>&)>;
    void setDiagnosticCallback(DiagnosticCallback cb);

    using BuildCallback = std::function<void(const BuildResult&)>;
    void setBuildCallback(BuildCallback cb);

private:
    /* Per-file session storage */
    struct FileSession {
        tc_session_t   *session   = nullptr;
        std::string     filePath;
        std::string     content;
        bool            dirty     = true;
        std::chrono::steady_clock::time_point lastUpdate;
    };

    std::unordered_map<std::string, std::unique_ptr<FileSession>> m_sessions;
    mutable std::mutex    m_sessionMutex;
    std::atomic<bool>     m_initialized{false};

    /* Metrics tracking */
    mutable std::mutex    m_metricsMutex;
    Metrics               m_metrics{};

    /* Callbacks */
    DiagnosticCallback    m_diagCallback;
    BuildCallback         m_buildCallback;

    /* Internal helpers */
    FileSession* getOrCreateSession(const std::string& filePath);
    void ensureParsed(FileSession* fs);
    static std::string severityToString(tc_severity_t sev);
    static std::string symbolKindToString(tc_symbol_kind_t kind);
    void recordMetric(const char* name, double ms);
};

} // namespace Toolchain
} // namespace RawrXD

#endif /* __cplusplus */
#endif /* TOOLCHAIN_BRIDGE_H */
