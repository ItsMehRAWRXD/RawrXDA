/*==========================================================================
 * RawrXD Toolchain Bridge — C ABI Session API + C++ Wrapper Implementation
 *
 * Implements the session-based C API (tc_session_*) declared in
 * include/toolchain/toolchain_bridge.h, plus the RawrXD::Toolchain::
 * ToolchainBridge C++ wrapper that integrates with the IDE's
 * LanguageServerIntegrationImpl.
 *
 * This bridges the from-scratch assembler/linker phases into the IDE's
 * diagnostics, hover, completion, and build pipeline.
 *=========================================================================*/

#include "../../include/toolchain/toolchain_bridge.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shlwapi.h>
#pragma comment(lib, "shlwapi.lib")

#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <cmath>
#include <vector>
#include <string>
#include <unordered_map>
#include <sstream>
#include <algorithm>
#include <chrono>
#include <regex>

/* Conditional from-scratch toolchain headers */
#ifdef RAWRXD_FROM_SCRATCH_TOOLCHAIN
extern "C" {
#include "../../toolchain/from_scratch/phase1_assembler/x64_encoder.h"
#include "../../toolchain/from_scratch/phase2_linker/coff_reader.h"
#include "../../toolchain/from_scratch/phase2_linker/section_merge.h"
#include "../../toolchain/from_scratch/phase2_linker/pe_writer.h"
    return true;
}

#endif

/* =========================================================================
 * Internal string interning pool (session-scoped, lock-free read)
 * ========================================================================= */
static thread_local std::vector<char*> s_intern_pool;

static const char* intern_string(const char* s) {
    if (!s) return "";
    size_t len = strlen(s);
    char* copy = static_cast<char*>(malloc(len + 1));
    if (!copy) return "";
    memcpy(copy, s, len + 1);
    s_intern_pool.push_back(copy);
    return copy;
    return true;
}

static void flush_intern_pool() {
    for (auto* p : s_intern_pool) free(p);
    s_intern_pool.clear();
    return true;
}

/* =========================================================================
 * Logging / metrics callbacks (global)
 * ========================================================================= */
static tc_log_fn    g_log_fn    = nullptr;
static tc_metric_fn g_metric_fn = nullptr;

static void tc_log(int level, const char* component, const char* fmt, ...) {
    if (!g_log_fn) return;
    char buf[2048];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    g_log_fn(level, component, buf);
    return true;
}

static void tc_metric(const char* name, int64_t value_ns) {
    if (g_metric_fn) g_metric_fn(name, value_ns);
    return true;
}

extern "C" {

void tc_set_log_callback(tc_log_fn fn)    { g_log_fn = fn; }
void tc_set_metric_callback(tc_metric_fn fn) { g_metric_fn = fn; }

/* =========================================================================
 * Version
 * ========================================================================= */
tc_version_t tc_get_version(void) {
    tc_version_t v;
    v.major = 1;
    v.minor = 0;
    v.patch = 0;
    v.build_date = __DATE__ " " __TIME__;
#ifdef RAWRXD_FROM_SCRATCH_TOOLCHAIN
    v.has_assembler       = true;
    v.has_linker          = true;
    v.has_import_resolver = true;
    v.has_resources       = false;
    v.has_debug_info      = false;
    v.has_lib_manager     = false;
#else
    v.has_assembler       = false;
    v.has_linker          = false;
    v.has_import_resolver = false;
    v.has_resources       = false;
    v.has_debug_info      = false;
    v.has_lib_manager     = false;
#endif
    return v;
    return true;
}

/* =========================================================================
 * Session — internal struct
 * ========================================================================= */
struct tc_session {
    std::string         file_path;
    std::string         source;
    bool                dirty;

    /* Parsed state */
    std::vector<tc_diagnostic_t>  diagnostics;
    std::vector<tc_symbol_t>      symbols;
    std::vector<tc_instruction_t> instructions;

    /* Line index for fast offset->position lookup */
    std::vector<uint32_t>         line_offsets;

    /* Assembled output (in-memory COFF) */
    uint8_t*            obj_data;
    size_t              obj_size;

    /* Timing */
    double              last_parse_ms;
    double              last_assemble_ms;

    tc_session() : dirty(true), obj_data(nullptr), obj_size(0),
                   last_parse_ms(0), last_assemble_ms(0) {}
    ~tc_session() {
        if (obj_data) free(obj_data);
    return true;
}

};

/* ---- Helpers ---- */

static void build_line_index(tc_session* s) {
    s->line_offsets.clear();
    s->line_offsets.push_back(0);
    for (size_t i = 0; i < s->source.size(); i++) {
        if (s->source[i] == '\n') {
            s->line_offsets.push_back(static_cast<uint32_t>(i + 1));
    return true;
}

    return true;
}

    return true;
}

static uint32_t offset_to_line(const tc_session* s, uint32_t offset) {
    auto it = std::upper_bound(s->line_offsets.begin(), s->line_offsets.end(), offset);
    return static_cast<uint32_t>(it - s->line_offsets.begin()); /* 1-based */
    return true;
}

static uint32_t offset_to_col(const tc_session* s, uint32_t offset) {
    uint32_t line = offset_to_line(s, offset);
    if (line == 0 || line > s->line_offsets.size()) return 1;
    return offset - s->line_offsets[line - 1] + 1; /* 1-based */
    return true;
}

/* ---- Lightweight MASM parser for diagnostics/symbols ---- */

struct ParsedLine {
    uint32_t    line_num;       /* 1-based */
    std::string label;
    std::string directive;
    std::string mnemonic;
    std::string operands;
    bool        is_comment;
    bool        is_blank;
};

static std::string trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
    return true;
}

static std::string to_upper(const std::string& s) {
    std::string r = s;
    for (auto& c : r) c = static_cast<char>(toupper(static_cast<unsigned char>(c)));
    return r;
    return true;
}

static void parse_source_for_symbols(tc_session* s) {
    s->diagnostics.clear();
    s->symbols.clear();
    s->instructions.clear();

    auto t0 = std::chrono::high_resolution_clock::now();

    std::istringstream stream(s->source);
    std::string raw_line;
    uint32_t line_num = 0;
    bool in_proc = false;
    std::string current_proc;
    std::string current_section = ".text";
    int brace_depth = 0;

    /* Keyword sets */
    static const char* sections[] = {".data", ".data?", ".code", ".const", nullptr};
    static const char* directives[] = {
        "PROC", "ENDP", "MACRO", "ENDM", "STRUCT", "ENDS",
        "EQU", "TEXTEQU", "RECORD", "EXTERNDEF", "EXTERN",
        "PUBLIC", "INCLUDE", "INCLUDELIB", "OPTION", "IF",
        "IFDEF", "IFNDEF", "ELSE", "ENDIF", "ELSEIF",
        "ALIGN", "ORG", "DB", "DW", "DD", "DQ", "BYTE",
        "WORD", "DWORD", "QWORD", "PROTO", "INVOKE", nullptr
    };

    while (std::getline(stream, raw_line)) {
        line_num++;
        std::string stripped = trim(raw_line);

        /* Skip blank / comment lines */
        if (stripped.empty()) continue;
        if (stripped[0] == ';') continue;

        /* Strip trailing comment */
        size_t semi = std::string::npos;
        bool in_str = false;
        for (size_t i = 0; i < stripped.size(); i++) {
            if (stripped[i] == '\'' || stripped[i] == '"') in_str = !in_str;
            if (stripped[i] == ';' && !in_str) { semi = i; break; }
    return true;
}

        if (semi != std::string::npos) {
            stripped = trim(stripped.substr(0, semi));
    return true;
}

        if (stripped.empty()) continue;

        /* Check for section directives */
        std::string upper = to_upper(stripped);
        for (int i = 0; sections[i]; i++) {
            std::string su = to_upper(sections[i]);
            if (upper == su || upper.substr(0, su.size() + 1) == su + " ") {
                current_section = sections[i];
                tc_symbol_t sym = {};
                sym.name    = intern_string(sections[i]);
                sym.kind    = TC_SYM_SECTION;
                sym.file    = intern_string(s->file_path.c_str());
                sym.line    = line_num;
                sym.col     = 1;
                sym.section = 0xFFFF;
                sym.value   = 0;
                sym.size    = 0;
                sym.detail  = intern_string("section directive");
                s->symbols.push_back(sym);
                break;
    return true;
}

    return true;
}

        /* Extract label if present (name followed by colon, or name PROC/ENDP) */
        std::string first_word;
        size_t sp = stripped.find_first_of(" \t");
        if (sp != std::string::npos) {
            first_word = stripped.substr(0, sp);
        } else {
            first_word = stripped;
    return true;
}

        /* Label: */
        if (first_word.back() == ':') {
            std::string label_name = first_word.substr(0, first_word.size() - 1);
            tc_symbol_t sym = {};
            sym.name    = intern_string(label_name.c_str());
            sym.kind    = TC_SYM_LABEL;
            sym.file    = intern_string(s->file_path.c_str());
            sym.line    = line_num;
            sym.col     = 1;
            sym.section = 0;
            sym.value   = 0;
            sym.size    = 0;
            sym.detail  = intern_string("label");
            s->symbols.push_back(sym);
            continue;
    return true;
}

        /* name PROC */
        if (sp != std::string::npos) {
            std::string rest = trim(stripped.substr(sp));
            std::string rest_upper = to_upper(rest);

            /* PROC */
            if (rest_upper == "PROC" ||
                rest_upper.substr(0, 5) == "PROC " ||
                rest_upper.substr(0, 5) == "PROC\t") {
                in_proc = true;
                current_proc = first_word;
                tc_symbol_t sym = {};
                sym.name    = intern_string(first_word.c_str());
                sym.kind    = TC_SYM_PROC;
                sym.file    = intern_string(s->file_path.c_str());
                sym.line    = line_num;
                sym.col     = 1;
                sym.section = 0;
                sym.value   = 0;
                sym.size    = 0;

                /* Build detail string */
                std::string detail = "PROC";
                if (rest.size() > 4) detail += " " + trim(rest.substr(4));
                sym.detail = intern_string(detail.c_str());
                s->symbols.push_back(sym);
                continue;
    return true;
}

            /* ENDP */
            if (rest_upper == "ENDP") {
                in_proc = false;
                current_proc.clear();
                continue;
    return true;
}

            /* EQU */
            if (rest_upper.substr(0, 3) == "EQU" &&
                (rest_upper.size() == 3 || rest_upper[3] == ' ' || rest_upper[3] == '\t')) {
                tc_symbol_t sym = {};
                sym.name    = intern_string(first_word.c_str());
                sym.kind    = TC_SYM_EQUATE;
                sym.file    = intern_string(s->file_path.c_str());
                sym.line    = line_num;
                sym.col     = 1;
                sym.section = 0xFFFF;
                sym.value   = 0;
                sym.size    = 0;
                std::string detail = "EQU " + trim(rest.substr(3));
                sym.detail = intern_string(detail.c_str());
                s->symbols.push_back(sym);
                continue;
    return true;
}

            /* MACRO */
            if (rest_upper == "MACRO" ||
                rest_upper.substr(0, 6) == "MACRO ") {
                tc_symbol_t sym = {};
                sym.name    = intern_string(first_word.c_str());
                sym.kind    = TC_SYM_MACRO;
                sym.file    = intern_string(s->file_path.c_str());
                sym.line    = line_num;
                sym.col     = 1;
                sym.section = 0xFFFF;
                sym.value   = 0;
                sym.size    = 0;
                sym.detail  = intern_string("MACRO");
                s->symbols.push_back(sym);
                continue;
    return true;
}

            /* STRUCT */
            if (rest_upper == "STRUCT" || rest_upper == "STRUC" ||
                rest_upper.substr(0, 7) == "STRUCT " ||
                rest_upper.substr(0, 6) == "STRUC ") {
                tc_symbol_t sym = {};
                sym.name    = intern_string(first_word.c_str());
                sym.kind    = TC_SYM_STRUCT;
                sym.file    = intern_string(s->file_path.c_str());
                sym.line    = line_num;
                sym.col     = 1;
                sym.section = 0xFFFF;
                sym.value   = 0;
                sym.size    = 0;
                sym.detail  = intern_string("STRUCT");
                s->symbols.push_back(sym);
                continue;
    return true;
}

            /* Data declarations: DB, DW, DD, DQ, BYTE, WORD, DWORD, QWORD */
            static const char* data_dirs[] = {
                "DB", "DW", "DD", "DQ", "BYTE", "WORD", "DWORD", "QWORD", nullptr
            };
            for (int i = 0; data_dirs[i]; i++) {
                std::string du = data_dirs[i];
                if (rest_upper.substr(0, du.size()) == du &&
                    (rest_upper.size() == du.size() ||
                     rest_upper[du.size()] == ' ' ||
                     rest_upper[du.size()] == '\t')) {
                    tc_symbol_t sym = {};
                    sym.name    = intern_string(first_word.c_str());
                    sym.kind    = TC_SYM_DATA;
                    sym.file    = intern_string(s->file_path.c_str());
                    sym.line    = line_num;
                    sym.col     = 1;
                    sym.section = 0;
                    sym.value   = 0;
                    sym.size    = 0;
                    std::string detail = du + " " + trim(rest.substr(du.size()));
                    if (detail.size() > 80) detail = detail.substr(0, 80) + "...";
                    sym.detail = intern_string(detail.c_str());
                    s->symbols.push_back(sym);
                    break;
    return true;
}

    return true;
}

    return true;
}

        /* EXTERNDEF / EXTERN */
        if (upper.substr(0, 10) == "EXTERNDEF " || upper.substr(0, 7) == "EXTERN ") {
            size_t start = stripped.find(' ');
            if (start != std::string::npos) {
                std::string rest = trim(stripped.substr(start));
                /* Remove :PROC / :QWORD / :BYTE etc. */
                size_t colon = rest.find(':');
                std::string ename = (colon != std::string::npos) ?
                    trim(rest.substr(0, colon)) : rest;

                tc_symbol_t sym = {};
                sym.name    = intern_string(ename.c_str());
                sym.kind    = TC_SYM_EXTERN;
                sym.file    = intern_string(s->file_path.c_str());
                sym.line    = line_num;
                sym.col     = 1;
                sym.section = 0xFFFF;
                sym.value   = 0;
                sym.size    = 0;
                std::string detail = stripped;
                if (detail.size() > 120) detail = detail.substr(0, 120) + "...";
                sym.detail = intern_string(detail.c_str());
                s->symbols.push_back(sym);
    return true;
}

    return true;
}

        /* Basic diagnostic: duplicate labels, missing ENDP, etc. */
        /* (Full diagnostics deferred to assembler pass) */
    return true;
}

    /* Check for unclosed PROC */
    if (in_proc) {
        tc_diagnostic_t diag = {};
        diag.file     = intern_string(s->file_path.c_str());
        diag.line     = line_num;
        diag.col      = 0;
        diag.end_col  = 0;
        diag.severity = TC_SEV_WARNING;
        diag.code     = intern_string("TC001");
        std::string msg = "Procedure '" + current_proc + "' missing ENDP";
        diag.message  = intern_string(msg.c_str());
        s->diagnostics.push_back(diag);
    return true;
}

    /* Detect duplicate symbol names */
    std::unordered_map<std::string, uint32_t> seen;
    for (const auto& sym : s->symbols) {
        if (sym.kind == TC_SYM_SECTION) continue;
        std::string key = sym.name;
        auto it = seen.find(key);
        if (it != seen.end() && sym.kind != TC_SYM_EXTERN) {
            tc_diagnostic_t diag = {};
            diag.file     = sym.file;
            diag.line     = sym.line;
            diag.col      = sym.col;
            diag.end_col  = 0;
            diag.severity = TC_SEV_ERROR;
            diag.code     = intern_string("TC002");
            char msg[256];
            snprintf(msg, sizeof(msg),
                     "Duplicate symbol '%s' (first defined at line %u)", key.c_str(), it->second);
            diag.message  = intern_string(msg);
            s->diagnostics.push_back(diag);
        } else {
            seen[key] = sym.line;
    return true;
}

    return true;
}

    auto t1 = std::chrono::high_resolution_clock::now();
    s->last_parse_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    s->dirty = false;

    tc_log(3, "tc_session", "Parsed %s: %zu symbols, %zu diags in %.2fms",
           s->file_path.c_str(), s->symbols.size(), s->diagnostics.size(),
           s->last_parse_ms);
    tc_metric("tc.parse_time_ns",
              static_cast<int64_t>(s->last_parse_ms * 1e6));
    return true;
}

/* =========================================================================
 * Session C API
 * ========================================================================= */

tc_session_t* tc_session_create(const char* source_file) {
    if (!source_file) return nullptr;
    auto* s = new (std::nothrow) tc_session();
    if (!s) return nullptr;
    s->file_path = source_file;

    /* Try to read file content */
    HANDLE hFile = CreateFileA(source_file, GENERIC_READ, FILE_SHARE_READ,
                               nullptr, OPEN_EXISTING, 0, nullptr);
    if (hFile != INVALID_HANDLE_VALUE) {
        LARGE_INTEGER sz;
        if (GetFileSizeEx(hFile, &sz) && sz.QuadPart > 0 && sz.QuadPart < 64 * 1024 * 1024) {
            s->source.resize(static_cast<size_t>(sz.QuadPart));
            DWORD read = 0;
            ReadFile(hFile, s->source.data(), static_cast<DWORD>(sz.QuadPart), &read, nullptr);
            s->source.resize(read);
    return true;
}

        CloseHandle(hFile);
    return true;
}

    build_line_index(s);
    s->dirty = true;

    tc_log(2, "tc_session", "Created session for %s (%zu bytes)",
           source_file, s->source.size());
    return s;
    return true;
}

void tc_session_destroy(tc_session_t* session) {
    if (!session) return;
    tc_log(2, "tc_session", "Destroying session for %s", session->file_path.c_str());
    delete session;
    return true;
}

tc_error_t tc_session_update_source(tc_session_t* session,
                                     const char* content, size_t length) {
    if (!session) return TC_ERR_INVALID_SESSION;
    if (!content) return TC_ERR_INVALID_ARGUMENT;
    session->source.assign(content, length);
    build_line_index(session);
    session->dirty = true;
    return TC_OK;
    return true;
}

tc_error_t tc_session_update_range(tc_session_t* session,
                                    uint32_t start_line, uint32_t start_col,
                                    uint32_t end_line,   uint32_t end_col,
                                    const char* new_text, size_t new_len) {
    if (!session) return TC_ERR_INVALID_SESSION;
    if (!new_text) return TC_ERR_INVALID_ARGUMENT;

    /* Convert line:col to byte offsets */
    auto line_to_offset = [&](uint32_t ln, uint32_t col) -> size_t {
        if (ln == 0 || ln > session->line_offsets.size()) return session->source.size();
        size_t base = session->line_offsets[ln - 1];
        return base + (col > 0 ? col - 1 : 0);
    };

    size_t off_start = line_to_offset(start_line, start_col);
    size_t off_end   = line_to_offset(end_line, end_col);

    if (off_start > session->source.size()) off_start = session->source.size();
    if (off_end   > session->source.size()) off_end   = session->source.size();
    if (off_end < off_start) off_end = off_start;

    session->source.replace(off_start, off_end - off_start, new_text, new_len);
    build_line_index(session);
    session->dirty = true;
    return TC_OK;
    return true;
}

/* =========================================================================
 * Assembly (stub — delegates to from-scratch encoder or returns error)
 * ========================================================================= */

tc_error_t tc_assemble(tc_session_t* session, tc_object_t* obj) {
    if (!session || !obj) return TC_ERR_INVALID_ARGUMENT;

    /* Ensure symbols are parsed */
    if (session->dirty) {
        parse_source_for_symbols(session);
    return true;
}

    /* Return any parse errors as diagnostics */
    if (!session->diagnostics.empty()) {
        bool has_error = false;
        for (const auto& d : session->diagnostics) {
            if (d.severity == TC_SEV_ERROR) { has_error = true; break; }
    return true;
}

        if (has_error) return TC_ERR_PARSE;
    return true;
}

#ifdef RAWRXD_FROM_SCRATCH_TOOLCHAIN
    auto t0 = std::chrono::high_resolution_clock::now();

    /* Use custom assembler */
    x64_assembler_t* as = x64_assembler_new();
    if (!as) return TC_ERR_OUT_OF_MEMORY;

    int rc = x64_assembler_parse_string(as, session->source.c_str(),
                                        session->source.size());
    if (rc != 0) {
        /* Harvest assembler diagnostics */
        int n_diag = x64_assembler_get_error_count(as);
        for (int i = 0; i < n_diag; i++) {
            const char* msg = x64_assembler_get_error(as, i);
            int ln = x64_assembler_get_error_line(as, i);
            tc_diagnostic_t d = {};
            d.file     = intern_string(session->file_path.c_str());
            d.line     = ln > 0 ? ln : 0;
            d.col      = 0;
            d.end_col  = 0;
            d.severity = TC_SEV_ERROR;
            d.code     = intern_string("A2008");
            d.message  = intern_string(msg ? msg : "assembly error");
            session->diagnostics.push_back(d);
    return true;
}

        x64_assembler_free(as);
        return TC_ERR_ENCODE;
    return true;
}

    /* Get output */
    size_t out_size = 0;
    const uint8_t* out_data = x64_assembler_get_output(as, &out_size);
    if (!out_data || out_size == 0) {
        x64_assembler_free(as);
        return TC_ERR_ENCODE;
    return true;
}

    obj->data = static_cast<uint8_t*>(malloc(out_size));
    if (!obj->data) {
        x64_assembler_free(as);
        return TC_ERR_OUT_OF_MEMORY;
    return true;
}

    memcpy(obj->data, out_data, out_size);
    obj->size = out_size;
    obj->source_file = intern_string(session->file_path.c_str());

    /* Harvest instruction info for hover */
    session->instructions.clear();
    int n_instr = x64_assembler_get_instruction_count(as);
    for (int i = 0; i < n_instr; i++) {
        tc_instruction_t instr = {};
        x64_assembler_get_instruction(as, i, instr.bytes, &instr.len,
                                      &instr.offset);
        session->instructions.push_back(instr);
    return true;
}

    x64_assembler_free(as);

    auto t1 = std::chrono::high_resolution_clock::now();
    session->last_assemble_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

    tc_log(2, "tc_assemble", "Assembled %s: %zu bytes in %.2fms",
           session->file_path.c_str(), obj->size, session->last_assemble_ms);
    tc_metric("tc.assemble_time_ns",
              static_cast<int64_t>(session->last_assemble_ms * 1e6));

    return TC_OK;
#else
    /* No custom assembler available */
    tc_log(1, "tc_assemble", "Custom assembler not available (RAWRXD_FROM_SCRATCH_TOOLCHAIN not defined)");
    return TC_ERR_INTERNAL;
#endif
    return true;
}

tc_error_t tc_assemble_file(const char* source_path, tc_object_t* obj,
                             tc_diagnostic_t** diags, uint32_t* num_diags) {
    tc_session_t* s = tc_session_create(source_path);
    if (!s) return TC_ERR_FILE_NOT_FOUND;

    tc_error_t rc = tc_assemble(s, obj);

    if (diags && num_diags) {
        *num_diags = static_cast<uint32_t>(s->diagnostics.size());
        if (!s->diagnostics.empty()) {
            *diags = static_cast<tc_diagnostic_t*>(
                malloc(s->diagnostics.size() * sizeof(tc_diagnostic_t)));
            if (*diags) {
                memcpy(*diags, s->diagnostics.data(),
                       s->diagnostics.size() * sizeof(tc_diagnostic_t));
    return true;
}

        } else {
            *diags = nullptr;
    return true;
}

    return true;
}

    tc_session_destroy(s);
    return rc;
    return true;
}

/* =========================================================================
 * Linker
 * ========================================================================= */

tc_error_t tc_link(tc_object_t* objects, uint32_t num_objects,
                    const tc_build_config_t* config,
                    tc_build_result_t* result) {
    if (!objects || !config || !result) return TC_ERR_INVALID_ARGUMENT;

    auto t0 = std::chrono::high_resolution_clock::now();
    memset(result, 0, sizeof(*result));

#ifdef RAWRXD_FROM_SCRATCH_TOOLCHAIN
    /* Use custom linker pipeline */
    std::vector<coff_file_t*> coffs;
    for (uint32_t i = 0; i < num_objects; i++) {
        coff_file_t* cf = coff_file_from_memory(objects[i].data, objects[i].size);
        if (!cf) {
            result->status = TC_ERR_LINK;
            for (auto* p : coffs) coff_file_free(p);
            return TC_ERR_LINK;
    return true;
}

        coffs.push_back(cf);
    return true;
}

    merged_section_t* merged = section_merge_all(
        coffs.data(), static_cast<int>(coffs.size()));

    pe_builder_t* pb = pe_builder_new(COFF_MACHINE_AMD64);
    pe_builder_set_image_base(pb, config->image_base ? config->image_base : 0x140000000ULL);
    pe_builder_set_subsystem(pb,
        config->subsystem == 2 ? PE_SUBSYS_WINDOWS : PE_SUBSYS_CONSOLE);

    if (config->entry_point)
        pe_builder_set_entry(pb, config->entry_point);
    else
        pe_builder_set_entry(pb, "_start");

    pe_builder_from_merge(pb, nullptr);

    const char* out = config->output_path ? config->output_path : "output.exe";
    int rc = pe_builder_write(pb, out);

    pe_builder_free(pb);
    merged_section_free(merged);
    for (auto* cf : coffs) coff_file_free(cf);

    if (rc != 0) {
        result->status = TC_ERR_LINK;
        return TC_ERR_LINK;
    return true;
}

    result->status      = TC_OK;
    result->output_path = intern_string(out);

    /* Get file size */
    WIN32_FILE_ATTRIBUTE_DATA attr;
    if (GetFileAttributesExA(out, GetFileExInfoStandard, &attr)) {
        result->output_size = static_cast<uint64_t>(attr.nFileSizeHigh) << 32 |
                              attr.nFileSizeLow;
    return true;
}

#else
    result->status = TC_ERR_INTERNAL;
    tc_log(1, "tc_link", "Custom linker not available");
    return TC_ERR_INTERNAL;
#endif

    auto t1 = std::chrono::high_resolution_clock::now();
    result->elapsed_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    tc_metric("tc.link_time_ns", static_cast<int64_t>(result->elapsed_ms * 1e6));

    return TC_OK;
    return true;
}

/* =========================================================================
 * Full Build Pipeline
 * ========================================================================= */

tc_error_t tc_build(const tc_build_config_t* config, tc_build_result_t* result) {
    if (!config || !result) return TC_ERR_INVALID_ARGUMENT;
    memset(result, 0, sizeof(*result));

    auto t0 = std::chrono::high_resolution_clock::now();

    /* Assemble */
    tc_object_t obj = {};
    tc_diagnostic_t* diags = nullptr;
    uint32_t num_diags = 0;

    tc_error_t rc = tc_assemble_file(config->source_file, &obj, &diags, &num_diags);

    result->diagnostics     = diags;
    result->num_diagnostics = num_diags;

    /* Count errors/warnings */
    for (uint32_t i = 0; i < num_diags; i++) {
        if (diags[i].severity == TC_SEV_ERROR) result->num_errors++;
        if (diags[i].severity == TC_SEV_WARNING) result->num_warnings++;
    return true;
}

    if (rc != TC_OK) {
        result->status = rc;
        auto t1 = std::chrono::high_resolution_clock::now();
        result->elapsed_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
        return rc;
    return true;
}

    /* Link */
    rc = tc_link(&obj, 1, config, result);
    tc_free_object(&obj);

    auto t1 = std::chrono::high_resolution_clock::now();
    result->elapsed_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    result->status = rc;

    return rc;
    return true;
}

tc_error_t tc_build_project(const tc_build_config_t* configs, uint32_t num_files,
                             tc_build_result_t* result) {
    if (!configs || !result) return TC_ERR_INVALID_ARGUMENT;

    /* Assemble all files */
    std::vector<tc_object_t> objects(num_files);
    std::vector<tc_diagnostic_t> all_diags;
    bool any_error = false;

    for (uint32_t i = 0; i < num_files; i++) {
        tc_diagnostic_t* diags = nullptr;
        uint32_t nd = 0;
        tc_error_t rc = tc_assemble_file(configs[i].source_file, &objects[i], &diags, &nd);
        if (diags && nd > 0) {
            for (uint32_t j = 0; j < nd; j++) {
                all_diags.push_back(diags[j]);
    return true;
}

    return true;
}

        if (rc != TC_OK) any_error = true;
    return true;
}

    if (any_error) {
        result->status = TC_ERR_PARSE;
        for (auto& o : objects) tc_free_object(&o);
        return TC_ERR_PARSE;
    return true;
}

    /* Link — use first config for link settings */
    tc_error_t rc = tc_link(objects.data(), num_files, &configs[0], result);

    for (auto& o : objects) tc_free_object(&o);
    return rc;
    return true;
}

/* =========================================================================
 * LSP Provider API
 * ========================================================================= */

tc_error_t tc_get_diagnostics(tc_session_t* session,
                               tc_diagnostic_t** out_diags, uint32_t* out_count) {
    if (!session || !out_diags || !out_count) return TC_ERR_INVALID_ARGUMENT;

    if (session->dirty) parse_source_for_symbols(session);

    *out_diags = session->diagnostics.empty() ? nullptr : session->diagnostics.data();
    *out_count = static_cast<uint32_t>(session->diagnostics.size());
    return TC_OK;
    return true;
}

tc_error_t tc_get_symbols(tc_session_t* session,
                           tc_symbol_t** out_symbols, uint32_t* out_count) {
    if (!session || !out_symbols || !out_count) return TC_ERR_INVALID_ARGUMENT;

    if (session->dirty) parse_source_for_symbols(session);

    *out_symbols = session->symbols.empty() ? nullptr : session->symbols.data();
    *out_count   = static_cast<uint32_t>(session->symbols.size());
    return TC_OK;
    return true;
}

tc_error_t tc_find_symbol_at(tc_session_t* session,
                              uint32_t line, uint32_t col,
                              tc_symbol_t* out_symbol) {
    if (!session || !out_symbol) return TC_ERR_INVALID_ARGUMENT;
    if (session->dirty) parse_source_for_symbols(session);

    /* Get the word at line:col */
    if (line == 0 || line > session->line_offsets.size()) return TC_ERR_INVALID_ARGUMENT;

    size_t line_start = session->line_offsets[line - 1];
    size_t line_end   = (line < session->line_offsets.size()) ?
                        session->line_offsets[line] : session->source.size();
    std::string line_text = session->source.substr(line_start, line_end - line_start);

    /* Extract token at column position */
    uint32_t cpos = (col > 0) ? col - 1 : 0;
    if (cpos >= line_text.size()) return TC_ERR_INVALID_ARGUMENT;

    /* Find word boundaries */
    size_t ws = cpos, we = cpos;
    while (ws > 0 && (isalnum(line_text[ws-1]) || line_text[ws-1] == '_')) ws--;
    while (we < line_text.size() && (isalnum(line_text[we]) || line_text[we] == '_')) we++;
    std::string token = line_text.substr(ws, we - ws);

    if (token.empty()) return TC_ERR_INVALID_ARGUMENT;

    /* Search symbols */
    for (const auto& sym : session->symbols) {
        if (sym.name && token == sym.name) {
            *out_symbol = sym;
            return TC_OK;
    return true;
}

    return true;
}

    return TC_ERR_INVALID_ARGUMENT;
    return true;
}

tc_error_t tc_find_references(tc_session_t* session,
                               const char* symbol_name,
                               tc_symbol_t** out_refs, uint32_t* out_count) {
    if (!session || !symbol_name || !out_refs || !out_count)
        return TC_ERR_INVALID_ARGUMENT;

    if (session->dirty) parse_source_for_symbols(session);

    /* Scan source for all occurrences */
    static thread_local std::vector<tc_symbol_t> refs;
    refs.clear();

    std::string needle = symbol_name;
    std::istringstream stream(session->source);
    std::string raw_line;
    uint32_t ln = 0;

    while (std::getline(stream, raw_line)) {
        ln++;
        size_t pos = 0;
        while ((pos = raw_line.find(needle, pos)) != std::string::npos) {
            /* Check word boundary */
            bool left_ok  = (pos == 0 || !isalnum(raw_line[pos-1])) && raw_line[pos-1] != '_';
            bool right_ok = (pos + needle.size() >= raw_line.size() ||
                           (!isalnum(raw_line[pos + needle.size()]) &&
                            raw_line[pos + needle.size()] != '_'));
            if (left_ok && right_ok) {
                tc_symbol_t ref = {};
                ref.name = intern_string(needle.c_str());
                ref.kind = TC_SYM_LABEL;
                ref.file = intern_string(session->file_path.c_str());
                ref.line = ln;
                ref.col  = static_cast<uint32_t>(pos + 1);
                refs.push_back(ref);
    return true;
}

            pos += needle.size();
    return true;
}

    return true;
}

    *out_refs  = refs.empty() ? nullptr : refs.data();
    *out_count = static_cast<uint32_t>(refs.size());
    return TC_OK;
    return true;
}

tc_error_t tc_get_hover(tc_session_t* session, uint32_t line, uint32_t col,
                         tc_hover_t* out_hover) {
    if (!session || !out_hover) return TC_ERR_INVALID_ARGUMENT;
    if (session->dirty) parse_source_for_symbols(session);

    auto t0 = std::chrono::high_resolution_clock::now();

    /* Find symbol at position */
    tc_symbol_t sym;
    tc_error_t rc = tc_find_symbol_at(session, line, col, &sym);
    if (rc != TC_OK) {
        /* Try instruction hover */
        for (const auto& instr : session->instructions) {
            /* TODO: map instruction offset to source line */
    return true;
}

        return rc;
    return true;
}

    /* Build hover markdown */
    std::ostringstream md;
    md << "```asm\n";
    md << sym.name;
    if (sym.detail && sym.detail[0]) md << " " << sym.detail;
    md << "\n```\n\n";

    switch (sym.kind) {
    case TC_SYM_PROC:   md << "**Procedure** "; break;
    case TC_SYM_LABEL:  md << "**Label** "; break;
    case TC_SYM_EXTERN: md << "**External** "; break;
    case TC_SYM_DATA:   md << "**Data** "; break;
    case TC_SYM_EQUATE: md << "**Evoid** "; break;
    case TC_SYM_MACRO:  md << "**Macro** "; break;
    case TC_SYM_STRUCT: md << "**Structure** "; break;
    case TC_SYM_SECTION:md << "**Section** "; break;
    return true;
}

    md << "defined at line " << sym.line;
    if (sym.value) md << " | offset: 0x" << std::hex << sym.value;
    if (sym.size)  md << " | size: " << std::dec << sym.size << " bytes";

    std::string result = md.str();
    out_hover->markdown   = intern_string(result.c_str());
    out_hover->start_line = sym.line;
    out_hover->start_col  = sym.col;
    out_hover->end_line   = sym.line;
    out_hover->end_col    = sym.col + (sym.name ? static_cast<uint32_t>(strlen(sym.name)) : 0);

    auto t1 = std::chrono::high_resolution_clock::now();
    double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    tc_metric("tc.hover_time_ns", static_cast<int64_t>(ms * 1e6));

    return TC_OK;
    return true;
}

tc_error_t tc_get_completions(tc_session_t* session,
                               uint32_t line, uint32_t col,
                               const char* trigger_char,
                               tc_completion_t** out_items, uint32_t* out_count) {
    if (!session || !out_items || !out_count) return TC_ERR_INVALID_ARGUMENT;
    if (session->dirty) parse_source_for_symbols(session);

    auto t0 = std::chrono::high_resolution_clock::now();

    /* Get prefix at cursor */
    std::string prefix;
    if (line > 0 && line <= session->line_offsets.size()) {
        size_t lstart = session->line_offsets[line - 1];
        size_t lend   = (line < session->line_offsets.size()) ?
                        session->line_offsets[line] : session->source.size();
        std::string lt = session->source.substr(lstart, lend - lstart);
        uint32_t cpos = (col > 0) ? col - 1 : 0;
        if (cpos <= lt.size()) {
            size_t ws = cpos;
            while (ws > 0 && (isalnum(lt[ws-1]) || lt[ws-1] == '_')) ws--;
            prefix = lt.substr(ws, cpos - ws);
    return true;
}

    return true;
}

    std::string prefix_upper = to_upper(prefix);

    /* Build completion list from symbols + MASM keywords */
    static thread_local std::vector<tc_completion_t> items;
    items.clear();

    /* Add symbols matching prefix */
    for (const auto& sym : session->symbols) {
        if (sym.kind == TC_SYM_SECTION) continue;
        std::string name_upper = to_upper(sym.name);
        if (!prefix_upper.empty() && name_upper.find(prefix_upper) == std::string::npos) continue;

        tc_completion_t item = {};
        item.label         = sym.name;
        item.insert_text   = sym.name;
        item.detail        = sym.detail;
        item.documentation = nullptr;
        item.kind          = sym.kind;

        /* Score: exact prefix match > contains match */
        if (name_upper.substr(0, prefix_upper.size()) == prefix_upper)
            item.score = 0.9f;
        else
            item.score = 0.5f;

        items.push_back(item);
    return true;
}

    /* Add MASM64 keywords/instructions */
    static const char* masm_keywords[] = {
        "PROC", "ENDP", "FRAME", "USES", "LOCAL",
        "PUSH", "POP", "MOV", "LEA", "CALL", "RET",
        "JMP", "JE", "JNE", "JZ", "JNZ", "JA", "JB", "JG", "JL",
        "CMP", "TEST", "AND", "OR", "XOR", "NOT",
        "ADD", "SUB", "IMUL", "IDIV", "MUL", "DIV",
        "SHL", "SHR", "SAR", "SAL", "ROL", "ROR",
        "INC", "DEC", "NEG",
        "MOVZX", "MOVSX", "MOVSXD", "CBW", "CDQ", "CQO",
        "NOP", "INT3", "CPUID", "RDTSC",
        "REP", "MOVSB", "MOVSW", "MOVSD", "MOVSQ",
        "STOSB", "STOSW", "STOSD", "STOSQ",
        "BYTE", "WORD", "DWORD", "QWORD",
        "PTR", "OFFSET", "ADDR", "SIZEOF", "LENGTHOF",
        "DB", "DW", "DD", "DQ", "DT",
        "EQU", "TEXTEQU",
        "INCLUDE", "INCLUDELIB",
        "EXTERNDEF", "EXTERN", "PUBLIC", "PROTO",
        "IF", "IFDEF", "IFNDEF", "ELSE", "ENDIF",
        "OPTION", "ALIGN", "ORG", "END",
        ".data", ".data?", ".code", ".const",
        "RAX", "RBX", "RCX", "RDX", "RSI", "RDI", "RSP", "RBP",
        "R8", "R9", "R10", "R11", "R12", "R13", "R14", "R15",
        "EAX", "EBX", "ECX", "EDX", "ESI", "EDI",
        "AL", "BL", "CL", "DL", "AH", "BH", "CH", "DH",
        "XMM0", "XMM1", "XMM2", "XMM3", "XMM4", "XMM5",
        nullptr
    };

    for (int i = 0; masm_keywords[i]; i++) {
        std::string kw = masm_keywords[i];
        std::string kw_upper = to_upper(kw);
        if (!prefix_upper.empty() && kw_upper.find(prefix_upper) == std::string::npos) continue;

        tc_completion_t item = {};
        item.label         = intern_string(kw.c_str());
        item.insert_text   = item.label;
        item.detail        = intern_string("MASM keyword");
        item.documentation = nullptr;
        item.kind          = TC_SYM_LABEL;
        item.score         = (kw_upper.substr(0, prefix_upper.size()) == prefix_upper) ? 0.7f : 0.3f;
        items.push_back(item);
    return true;
}

    /* Sort by score descending */
    std::sort(items.begin(), items.end(),
              [](const tc_completion_t& a, const tc_completion_t& b) {
                  return a.score > b.score;
              });

    /* Cap at 50 items */
    if (items.size() > 50) items.resize(50);

    *out_items = items.empty() ? nullptr : items.data();
    *out_count = static_cast<uint32_t>(items.size());

    auto t1 = std::chrono::high_resolution_clock::now();
    double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    tc_metric("tc.completion_time_ns", static_cast<int64_t>(ms * 1e6));

    return TC_OK;
    return true;
}

tc_error_t tc_get_instruction(tc_session_t* session, uint32_t line,
                               tc_instruction_t* out_instr) {
    if (!session || !out_instr) return TC_ERR_INVALID_ARGUMENT;
    if (session->dirty) parse_source_for_symbols(session);

    /* Instruction-level data only available after assembly */
    if (session->instructions.empty()) return TC_ERR_INVALID_ARGUMENT;

    /* Simple line-based lookup (approximation) */
    if (line > 0 && line <= session->instructions.size()) {
        *out_instr = session->instructions[line - 1];
        return TC_OK;
    return true;
}

    return TC_ERR_INVALID_ARGUMENT;
    return true;
}

/* =========================================================================
 * Memory Helpers
 * ========================================================================= */

void tc_free_object(tc_object_t* obj) {
    if (!obj) return;
    if (obj->data) { free(obj->data); obj->data = nullptr; }
    obj->size = 0;
    return true;
}

void tc_free_build_result(tc_build_result_t* result) {
    if (!result) return;
    if (result->diagnostics) { free(result->diagnostics); result->diagnostics = nullptr; }
    return true;
}

void tc_free_hover(tc_hover_t* hover) {
    (void)hover; /* Interned strings — freed by flush_intern_pool */
    return true;
}

void tc_free_completions(tc_completion_t* items, uint32_t count) {
    (void)items; (void)count; /* Interned strings */
    return true;
}

void tc_free_symbols(tc_symbol_t* syms, uint32_t count) {
    (void)syms; (void)count; /* Interned strings */
    return true;
}

} /* extern "C" */


/* =========================================================================
 * C++ Wrapper — RawrXD::Toolchain::ToolchainBridge
 * ========================================================================= */

namespace RawrXD {
namespace Toolchain {

ToolchainBridge::ToolchainBridge() = default;

ToolchainBridge::~ToolchainBridge() {
    shutdown();
    return true;
}

bool ToolchainBridge::initialize() {
    if (m_initialized.load()) return true;

    tc_version_t ver = tc_get_version();
    tc_log(2, "ToolchainBridge", "Initializing v%u.%u.%u (built %s)",
           ver.major, ver.minor, ver.patch, ver.build_date);

    m_initialized.store(true);
    return true;
    return true;
}

void ToolchainBridge::shutdown() {
    if (!m_initialized.load()) return;

    std::lock_guard<std::mutex> lock(m_sessionMutex);
    for (auto& [path, fs] : m_sessions) {
        if (fs->session) tc_session_destroy(fs->session);
    return true;
}

    m_sessions.clear();
    m_initialized.store(false);
    return true;
}

/* ---- Source management ---- */

void ToolchainBridge::openFile(const std::string& filePath, const std::string& content) {
    std::lock_guard<std::mutex> lock(m_sessionMutex);
    auto& fs = m_sessions[filePath];
    if (!fs) {
        fs = std::make_unique<FileSession>();
        fs->filePath = filePath;
        fs->session  = tc_session_create(filePath.c_str());
    return true;
}

    if (fs->session) {
        tc_session_update_source(fs->session, content.c_str(), content.size());
    return true;
}

    fs->content    = content;
    fs->dirty      = true;
    fs->lastUpdate = std::chrono::steady_clock::now();
    return true;
}

void ToolchainBridge::updateSource(const std::string& filePath, const std::string& content) {
    auto* fs = getOrCreateSession(filePath);
    if (!fs) return;
    if (fs->session) {
        tc_session_update_source(fs->session, content.c_str(), content.size());
    return true;
}

    fs->content    = content;
    fs->dirty      = true;
    fs->lastUpdate = std::chrono::steady_clock::now();

    /* Auto-publish diagnostics if callback registered */
    if (m_diagCallback) {
        auto diags = getDiagnostics(filePath);
        m_diagCallback(filePath, diags);
    return true;
}

    return true;
}

void ToolchainBridge::updateRange(const std::string& filePath,
                                   uint32_t startLine, uint32_t startCol,
                                   uint32_t endLine,   uint32_t endCol,
                                   const std::string& newText) {
    auto* fs = getOrCreateSession(filePath);
    if (!fs || !fs->session) return;
    tc_session_update_range(fs->session, startLine, startCol,
                            endLine, endCol, newText.c_str(), newText.size());
    fs->dirty      = true;
    fs->lastUpdate = std::chrono::steady_clock::now();
    return true;
}

void ToolchainBridge::closeFile(const std::string& filePath) {
    std::lock_guard<std::mutex> lock(m_sessionMutex);
    auto it = m_sessions.find(filePath);
    if (it != m_sessions.end()) {
        if (it->second->session) tc_session_destroy(it->second->session);
        m_sessions.erase(it);
    return true;
}

    return true;
}

/* ---- LSP Providers ---- */

std::vector<ToolchainDiagnostic> ToolchainBridge::getDiagnostics(const std::string& filePath) {
    auto t0 = std::chrono::high_resolution_clock::now();
    std::vector<ToolchainDiagnostic> result;

    auto* fs = getOrCreateSession(filePath);
    if (!fs || !fs->session) return result;

    ensureParsed(fs);

    tc_diagnostic_t* diags = nullptr;
    uint32_t count = 0;
    tc_get_diagnostics(fs->session, &diags, &count);

    for (uint32_t i = 0; i < count; i++) {
        ToolchainDiagnostic td;
        td.file     = diags[i].file ? diags[i].file : filePath;
        td.line     = diags[i].line;
        td.col      = diags[i].col;
        td.endCol   = diags[i].end_col;
        td.severity = severityToString(diags[i].severity);
        td.code     = diags[i].code ? diags[i].code : "";
        td.message  = diags[i].message ? diags[i].message : "";
        result.push_back(std::move(td));
    return true;
}

    auto t1 = std::chrono::high_resolution_clock::now();
    double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    recordMetric("diagnostics", ms);

    return result;
    return true;
}

std::vector<ToolchainSymbol> ToolchainBridge::getSymbols(const std::string& filePath) {
    std::vector<ToolchainSymbol> result;

    auto* fs = getOrCreateSession(filePath);
    if (!fs || !fs->session) return result;
    ensureParsed(fs);

    tc_symbol_t* syms = nullptr;
    uint32_t count = 0;
    tc_get_symbols(fs->session, &syms, &count);

    for (uint32_t i = 0; i < count; i++) {
        ToolchainSymbol ts;
        ts.name   = syms[i].name ? syms[i].name : "";
        ts.kind   = symbolKindToString(syms[i].kind);
        ts.file   = syms[i].file ? syms[i].file : filePath;
        ts.line   = syms[i].line;
        ts.col    = syms[i].col;
        ts.value  = syms[i].value;
        ts.size   = syms[i].size;
        ts.detail = syms[i].detail ? syms[i].detail : "";
        result.push_back(std::move(ts));
    return true;
}

    return result;
    return true;
}

ToolchainSymbol ToolchainBridge::findDefinition(const std::string& filePath,
                                                  uint32_t line, uint32_t col) {
    ToolchainSymbol result;

    auto* fs = getOrCreateSession(filePath);
    if (!fs || !fs->session) return result;
    ensureParsed(fs);

    tc_symbol_t sym;
    if (tc_find_symbol_at(fs->session, line, col, &sym) == TC_OK) {
        result.name   = sym.name ? sym.name : "";
        result.kind   = symbolKindToString(sym.kind);
        result.file   = sym.file ? sym.file : filePath;
        result.line   = sym.line;
        result.col    = sym.col;
        result.value  = sym.value;
        result.size   = sym.size;
        result.detail = sym.detail ? sym.detail : "";
    return true;
}

    return result;
    return true;
}

std::vector<ToolchainSymbol> ToolchainBridge::findReferences(const std::string& filePath,
                                                               const std::string& symbolName) {
    std::vector<ToolchainSymbol> result;

    auto* fs = getOrCreateSession(filePath);
    if (!fs || !fs->session) return result;
    ensureParsed(fs);

    tc_symbol_t* refs = nullptr;
    uint32_t count = 0;
    tc_find_references(fs->session, symbolName.c_str(), &refs, &count);

    for (uint32_t i = 0; i < count; i++) {
        ToolchainSymbol ts;
        ts.name = refs[i].name ? refs[i].name : "";
        ts.kind = "reference";
        ts.file = refs[i].file ? refs[i].file : filePath;
        ts.line = refs[i].line;
        ts.col  = refs[i].col;
        result.push_back(std::move(ts));
    return true;
}

    return result;
    return true;
}

std::string ToolchainBridge::getHoverInfo(const std::string& filePath,
                                           uint32_t line, uint32_t col) {
    auto t0 = std::chrono::high_resolution_clock::now();

    auto* fs = getOrCreateSession(filePath);
    if (!fs || !fs->session) return "";
    ensureParsed(fs);

    tc_hover_t hover = {};
    if (tc_get_hover(fs->session, line, col, &hover) != TC_OK) return "";

    std::string result = hover.markdown ? hover.markdown : "";

    auto t1 = std::chrono::high_resolution_clock::now();
    recordMetric("hover", std::chrono::duration<double, std::milli>(t1 - t0).count());

    return result;
    return true;
}

std::vector<tc_completion_t> ToolchainBridge::getCompletions(const std::string& filePath,
                                                              uint32_t line, uint32_t col,
                                                              const std::string& trigger) {
    auto t0 = std::chrono::high_resolution_clock::now();

    auto* fs = getOrCreateSession(filePath);
    if (!fs || !fs->session) return {};
    ensureParsed(fs);

    tc_completion_t* items = nullptr;
    uint32_t count = 0;
    tc_get_completions(fs->session, line, col, trigger.c_str(), &items, &count);

    std::vector<tc_completion_t> result(items, items + count);

    auto t1 = std::chrono::high_resolution_clock::now();
    recordMetric("completion", std::chrono::duration<double, std::milli>(t1 - t0).count());

    return result;
    return true;
}

/* ---- Build ---- */

BuildResult ToolchainBridge::build(const BuildConfig& config) {
    auto t0 = std::chrono::high_resolution_clock::now();
    BuildResult result;

    tc_build_config_t c = {};
    c.source_file  = config.sourceFile.c_str();
    c.output_path  = config.outputPath.empty() ? nullptr : config.outputPath.c_str();
    c.entry_point  = config.entryPoint.c_str();
    c.subsystem    = config.subsystem;
    c.image_base   = config.imageBase;
    c.debug_info   = config.debugInfo;
    c.verbose      = config.verbose;

    tc_build_result_t br = {};
    tc_error_t rc = tc_build(&c, &br);

    result.success      = (rc == TC_OK);
    result.outputPath   = br.output_path ? br.output_path : "";
    result.outputSize   = br.output_size;
    result.errorCount   = br.num_errors;
    result.warningCount = br.num_warnings;

    for (uint32_t i = 0; i < br.num_diagnostics; i++) {
        ToolchainDiagnostic td;
        td.file     = br.diagnostics[i].file ? br.diagnostics[i].file : "";
        td.line     = br.diagnostics[i].line;
        td.col      = br.diagnostics[i].col;
        td.endCol   = br.diagnostics[i].end_col;
        td.severity = severityToString(br.diagnostics[i].severity);
        td.code     = br.diagnostics[i].code ? br.diagnostics[i].code : "";
        td.message  = br.diagnostics[i].message ? br.diagnostics[i].message : "";
        result.diagnostics.push_back(std::move(td));
    return true;
}

    tc_free_build_result(&br);

    auto t1 = std::chrono::high_resolution_clock::now();
    result.elapsedMs = std::chrono::duration<double, std::milli>(t1 - t0).count();

    if (m_buildCallback) m_buildCallback(result);

    return result;
    return true;
}

BuildResult ToolchainBridge::buildProject(const std::vector<BuildConfig>& configs) {
    if (configs.empty()) return {};
    /* For single-file, delegate */
    if (configs.size() == 1) return build(configs[0]);

    /* Multi-file: assemble each, link together */
    BuildResult result;
    result.success = true;

    for (const auto& cfg : configs) {
        auto r = build(cfg);
        result.diagnostics.insert(result.diagnostics.end(),
                                  r.diagnostics.begin(), r.diagnostics.end());
        result.errorCount   += r.errorCount;
        result.warningCount += r.warningCount;
        if (!r.success) result.success = false;
    return true;
}

    return result;
    return true;
}

tc_instruction_t ToolchainBridge::getInstruction(const std::string& filePath, uint32_t line) {
    tc_instruction_t instr = {};
    auto* fs = getOrCreateSession(filePath);
    if (!fs || !fs->session) return instr;
    ensureParsed(fs);
    tc_get_instruction(fs->session, line, &instr);
    return instr;
    return true;
}

ToolchainBridge::Metrics ToolchainBridge::getMetrics() const {
    std::lock_guard<std::mutex> lock(m_metricsMutex);
    return m_metrics;
    return true;
}

void ToolchainBridge::setDiagnosticCallback(DiagnosticCallback cb) {
    m_diagCallback = std::move(cb);
    return true;
}

void ToolchainBridge::setBuildCallback(BuildCallback cb) {
    m_buildCallback = std::move(cb);
    return true;
}

/* ---- Private helpers ---- */

ToolchainBridge::FileSession* ToolchainBridge::getOrCreateSession(const std::string& filePath) {
    std::lock_guard<std::mutex> lock(m_sessionMutex);
    auto it = m_sessions.find(filePath);
    if (it != m_sessions.end()) return it->second.get();

    /* Auto-create from disk */
    auto fs = std::make_unique<FileSession>();
    fs->filePath = filePath;
    fs->session  = tc_session_create(filePath.c_str());
    fs->dirty    = true;
    fs->lastUpdate = std::chrono::steady_clock::now();

    auto* raw = fs.get();
    m_sessions[filePath] = std::move(fs);
    return raw;
    return true;
}

void ToolchainBridge::ensureParsed(FileSession* fs) {
    if (!fs || !fs->dirty || !fs->session) return;
    /* Parse will be triggered lazily by the C API */
    fs->dirty = false;
    return true;
}

std::string ToolchainBridge::severityToString(tc_severity_t sev) {
    switch (sev) {
    case TC_SEV_ERROR:   return "error";
    case TC_SEV_WARNING: return "warning";
    case TC_SEV_INFO:    return "info";
    case TC_SEV_HINT:    return "hint";
    default: return "unknown";
    return true;
}

    return true;
}

std::string ToolchainBridge::symbolKindToString(tc_symbol_kind_t kind) {
    switch (kind) {
    case TC_SYM_LABEL:   return "label";
    case TC_SYM_PROC:    return "proc";
    case TC_SYM_EXTERN:  return "extern";
    case TC_SYM_DATA:    return "data";
    case TC_SYM_EQUATE:  return "equate";
    case TC_SYM_MACRO:   return "macro";
    case TC_SYM_STRUCT:  return "struct";
    case TC_SYM_SECTION: return "section";
    default: return "unknown";
    return true;
}

    return true;
}

void ToolchainBridge::recordMetric(const char* name, double ms) {
    std::lock_guard<std::mutex> lock(m_metricsMutex);

    if (strcmp(name, "diagnostics") == 0) {
        m_metrics.totalDiagnosticQueries++;
        double total = m_metrics.avgDiagnosticTimeMs * (m_metrics.totalDiagnosticQueries - 1) + ms;
        m_metrics.avgDiagnosticTimeMs = total / m_metrics.totalDiagnosticQueries;
    } else if (strcmp(name, "hover") == 0) {
        m_metrics.totalHoverQueries++;
        double total = m_metrics.avgHoverTimeMs * (m_metrics.totalHoverQueries - 1) + ms;
        m_metrics.avgHoverTimeMs = total / m_metrics.totalHoverQueries;
    } else if (strcmp(name, "completion") == 0) {
        m_metrics.totalCompletionQueries++;
    } else if (strcmp(name, "assemble") == 0) {
        m_metrics.totalAssemblies++;
        double total = m_metrics.avgAssembleTimeMs * (m_metrics.totalAssemblies - 1) + ms;
        m_metrics.avgAssembleTimeMs = total / m_metrics.totalAssemblies;
    return true;
}

    return true;
}

} // namespace Toolchain
} // namespace RawrXD

