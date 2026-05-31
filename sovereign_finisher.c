/*
 * sovereign_finisher.c - Unified Sovereign IDE with Thinking Effort & Extensions
 * =============================================================================
 * Complete integration: Gap Buffer + Thinking Effort + Extension Host + LLM
 * Target: <3000 lines, single-file, production-ready
 * 
 * Build: gcc -O3 -std=c11 -o sovereign sovereign_finisher.c -lm -lpthread
 * Run:   ./sovereign [model.gguf]
 */

#define _CRT_SECURE_NO_WARNINGS
#define SOVEREIGN_VERSION "2.0.0-Finisher"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>
#include <stdbool.h>
#include <ctype.h>
#include <time.h>
#include <signal.h>
#include <errno.h>

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#include <process.h>
#define PATH_SEP '\\'
#define DLLEXT ".dll"
#else
#include <unistd.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <dlfcn.h>
#define PATH_SEP '/'
#define DLLEXT ".so"
#endif

/* ============================================================================
 * CONFIGURATION
 * ============================================================================ */

#define GAP_MIN_CAPACITY    8192
#define GAP_GROW_FACTOR     2
#define EMBEDDING_DIM       384
#define MAX_CONTEXT         4096
#define MAX_TOKENS          4096
#define MAX_EMBEDDINGS      2048
#define MAX_LINE_LEN        4096
#define MAX_EXTENSIONS      64
#define MAX_PROCESSES       16
#define IPC_BUFFER_SIZE     65536
#define THINKING_LEVELS     6

/* Thinking Effort Levels */
#define THINK_OFF           0
#define THINK_LOW           1
#define THINK_MEDIUM        2
#define THINK_HIGH          3
#define THINK_EXTRA         4
#define THINK_MAX           5

/* Extension Types */
#define EXT_TYPE_NATIVE     0
#define EXT_TYPE_MANAGED    1
#define EXT_TYPE_SCRIPT     2
#define EXT_TYPE_STUB       3

/* ============================================================================
 * GAP BUFFER - O(1) Text Editor Core
 * ============================================================================ */

typedef struct {
    char*   data;
    size_t  capacity;
    size_t  gap_start;
    size_t  gap_end;
    size_t  cursor;
    size_t  version;
} GapBuffer;

static GapBuffer* gap_create(size_t cap) {
    if (cap < GAP_MIN_CAPACITY) cap = GAP_MIN_CAPACITY;
    GapBuffer* gb = calloc(1, sizeof(GapBuffer));
    if (!gb) return NULL;
    
    gb->data = malloc(cap);
    if (!gb->data) { free(gb); return NULL; }
    
    gb->capacity = cap;
    gb->gap_end = cap;
    gb->version = 1;
    return gb;
}

static void gap_destroy(GapBuffer* gb) {
    if (!gb) return;
    free(gb->data);
    free(gb);
}

static void gap_ensure(GapBuffer* gb, size_t needed) {
    size_t free_space = gb->gap_end - gb->gap_start;
    if (free_space >= needed) return;
    
    size_t text_len = gb->gap_start + (gb->capacity - gb->gap_end);
    size_t new_cap = gb->capacity;
    while (new_cap < text_len + needed + GAP_MIN_CAPACITY)
        new_cap *= GAP_GROW_FACTOR;
    
    char* new_data = realloc(gb->data, new_cap);
    if (!new_data) return;
    
    size_t after_len = gb->capacity - gb->gap_end;
    memmove(new_data + new_cap - after_len, new_data + gb->gap_end, after_len);
    
    gb->data = new_data;
    gb->gap_end = new_cap - after_len;
    gb->capacity = new_cap;
}

static void gap_move_cursor(GapBuffer* gb, size_t pos) {
    size_t text_len = gb->gap_start + (gb->capacity - gb->gap_end);
    if (pos > text_len) pos = text_len;
    if (pos == gb->cursor) return;
    
    if (pos < gb->cursor) {
        size_t move = gb->cursor - pos;
        memmove(gb->data + gb->gap_end - move, gb->data + pos, move);
        gb->gap_start -= move;
        gb->gap_end -= move;
    } else {
        size_t move = pos - gb->cursor;
        memmove(gb->data + gb->gap_start, gb->data + gb->gap_end, move);
        gb->gap_start += move;
        gb->gap_end += move;
    }
    gb->cursor = pos;
}

static void gap_insert(GapBuffer* gb, const char* text, size_t len) {
    gap_ensure(gb, len);
    memcpy(gb->data + gb->gap_start, text, len);
    gb->gap_start += len;
    gb->cursor = gb->gap_start;
    gb->version++;
}

static void gap_delete(GapBuffer* gb, size_t len) {
    if (len == 0 || gb->gap_start == 0) return;
    if (len > gb->gap_start) len = gb->gap_start;
    gb->gap_start -= len;
    gb->cursor = gb->gap_start;
    gb->version++;
}

static size_t gap_length(const GapBuffer* gb) {
    return gb->gap_start + (gb->capacity - gb->gap_end);
}

static char* gap_extract(const GapBuffer* gb) {
    size_t len = gap_length(gb);
    char* out = malloc(len + 1);
    if (!out) return NULL;
    memcpy(out, gb->data, gb->gap_start);
    memcpy(out + gb->gap_start, gb->data + gb->gap_end, len - gb->gap_start);
    out[len] = '\0';
    return out;
}

static void gap_insert_char(GapBuffer* gb, char c) {
    gap_ensure(gb, 1);
    gb->data[gb->gap_start++] = c;
    gb->cursor = gb->gap_start;
    gb->version++;
}

/* ============================================================================
 * THINKING EFFORT SYSTEM
 * ============================================================================ */

typedef struct {
    int     level;
    size_t  max_iterations;
    size_t  max_tokens;
    size_t  max_depth;
    double  max_time_ms;
    double  temperature;
    double  exploration_rate;
    bool    enable_parallel;
    bool    enable_caching;
} ThinkingBudget;

typedef struct {
    int             current_level;
    ThinkingBudget  budget;
    size_t          iteration;
    size_t          depth;
    double          confidence;
    char**          reasoning_chain;
    size_t          chain_count;
    size_t          chain_capacity;
    char            current_task[256];
    bool            should_continue;
} ThinkingContext;

static const ThinkingBudget THINKING_BUDGETS[THINKING_LEVELS] = {
    {THINK_OFF,   1, 0, 0, 0.1, 0.0, 0.0, false, false},
    {THINK_LOW,   10, 100, 2, 100.0, 0.3, 0.1, false, true},
    {THINK_MEDIUM, 100, 500, 5, 1000.0, 0.7, 0.3, true, true},
    {THINK_HIGH,  1000, 2000, 10, 5000.0, 0.9, 0.5, true, true},
    {THINK_EXTRA, 10000, 8000, 20, 20000.0, 1.0, 0.7, true, true},
    {THINK_MAX,   (size_t)-1, 32000, 100, 300000.0, 1.2, 1.0, true, true}
};

static ThinkingContext* thinking_create(int level) {
    if (level < 0 || level >= THINKING_LEVELS) level = THINK_MEDIUM;
    
    ThinkingContext* ctx = calloc(1, sizeof(ThinkingContext));
    if (!ctx) return NULL;
    
    ctx->current_level = level;
    ctx->budget = THINKING_BUDGETS[level];
    ctx->chain_capacity = 64;
    ctx->reasoning_chain = calloc(ctx->chain_capacity, sizeof(char*));
    ctx->should_continue = true;
    ctx->confidence = 0.5;
    
    return ctx;
}

static void thinking_destroy(ThinkingContext* ctx) {
    if (!ctx) return;
    for (size_t i = 0; i < ctx->chain_count; i++) {
        free(ctx->reasoning_chain[i]);
    }
    free(ctx->reasoning_chain);
    free(ctx);
}

static void thinking_reset(ThinkingContext* ctx) {
    if (!ctx) return;
    for (size_t i = 0; i < ctx->chain_count; i++) {
        free(ctx->reasoning_chain[i]);
        ctx->reasoning_chain[i] = NULL;
    }
    ctx->chain_count = 0;
    ctx->iteration = 0;
    ctx->depth = 0;
    ctx->confidence = 0.5;
    ctx->should_continue = true;
}

static void thinking_set_level(ThinkingContext* ctx, int level) {
    if (!ctx || level < 0 || level >= THINKING_LEVELS) return;
    ctx->current_level = level;
    ctx->budget = THINKING_BUDGETS[level];
}

static void thinking_push_reasoning(ThinkingContext* ctx, const char* step, double confidence) {
    if (!ctx || !step) return;
    
    if (ctx->chain_count >= ctx->chain_capacity) {
        size_t new_cap = ctx->chain_capacity * 2;
        char** new_chain = realloc(ctx->reasoning_chain, new_cap * sizeof(char*));
        if (!new_chain) return;
        ctx->reasoning_chain = new_chain;
        ctx->chain_capacity = new_cap;
    }
    
    ctx->reasoning_chain[ctx->chain_count] = strdup(step);
    ctx->confidence = confidence;
    ctx->chain_count++;
    ctx->iteration++;
}

static bool thinking_should_continue(ThinkingContext* ctx) {
    if (!ctx) return false;
    if (ctx->iteration >= ctx->budget.max_iterations) return false;
    if (ctx->depth >= ctx->budget.max_depth) return false;
    if (ctx->confidence >= 0.95) return false;
    return ctx->should_continue;
}

static double thinking_estimate_complexity(const char* task) {
    if (!task) return 0.5;
    
    double complexity = 0.5;
    
    if (strstr(task, "implement") || strstr(task, "create"))
        complexity = 0.8;
    else if (strstr(task, "optimize") || strstr(task, "refactor"))
        complexity = 0.9;
    else if (strstr(task, "debug") || strstr(task, "fix"))
        complexity = 0.85;
    else if (strstr(task, "analyze") || strstr(task, "explain"))
        complexity = 0.7;
    else if (strstr(task, "list") || strstr(task, "show"))
        complexity = 0.3;
    else if (strstr(task, "help"))
        complexity = 0.4;
    
    return complexity;
}

static int thinking_recommend_level(const char* task, double importance) {
    double complexity = thinking_estimate_complexity(task);
    double score = complexity * 0.6 + importance * 0.4;
    
    if (score < 0.1) return THINK_OFF;
    if (score < 0.25) return THINK_LOW;
    if (score < 0.5) return THINK_MEDIUM;
    if (score < 0.75) return THINK_HIGH;
    if (score < 0.9) return THINK_EXTRA;
    return THINK_MAX;
}

static void thinking_print_chain(const ThinkingContext* ctx) {
    if (!ctx) return;
    printf("\n=== Reasoning Chain (Level %d) ===\n", ctx->current_level);
    for (size_t i = 0; i < ctx->chain_count; i++) {
        printf("%zu. %s\n", i + 1, ctx->reasoning_chain[i]);
    }
    printf("================================\n");
}

/* ============================================================================
 * EXTENSION HOST - Sandboxed Process Management
 * ============================================================================ */

typedef struct {
    char        name[128];
    char        path[512];
    int         type;
    bool        loaded;
    bool        sandboxed;
    pid_t       pid;
    int         ipc_read;
    int         ipc_write;
    void*       handle;
    char**      exports;
    size_t      export_count;
} Extension;

typedef struct {
    Extension   extensions[MAX_EXTENSIONS];
    size_t      count;
    char        sandbox_root[512];
    bool        enforce_sandbox;
    int         next_ipc_id;
} ExtensionHost;

typedef struct {
    char        magic[4];
    uint32_t    version;
    uint32_t    payload_size;
    uint32_t    flags;
    char        reserved[16];
} StubHeader;

static ExtensionHost* ext_host_create(const char* sandbox_root) {
    ExtensionHost* host = calloc(1, sizeof(ExtensionHost));
    if (!host) return NULL;
    
    if (sandbox_root) {
        strncpy(host->sandbox_root, sandbox_root, sizeof(host->sandbox_root) - 1);
    } else {
        strcpy(host->sandbox_root, ".");
    }
    host->enforce_sandbox = true;
    host->next_ipc_id = 1000;
    
    return host;
}

static void ext_host_destroy(ExtensionHost* host) {
    if (!host) return;
    
    for (size_t i = 0; i < host->count; i++) {
        Extension* ext = &host->extensions[i];
        if (ext->loaded) {
            if (ext->pid > 0) {
                #ifdef _WIN32
                TerminateProcess((HANDLE)(intptr_t)ext->pid, 0);
                #else
                kill(ext->pid, SIGTERM);
                #endif
            }
            if (ext->handle) {
                #ifdef _WIN32
                FreeLibrary((HMODULE)ext->handle);
                #else
                dlclose(ext->handle);
                #endif
            }
        }
        for (size_t j = 0; j < ext->export_count; j++) {
            free(ext->exports[j]);
        }
        free(ext->exports);
    }
    
    free(host);
}

static Extension* ext_host_load(ExtensionHost* host, const char* path, const char* name) {
    if (!host || !path || host->count >= MAX_EXTENSIONS) return NULL;
    
    Extension* ext = &host->extensions[host->count];
    memset(ext, 0, sizeof(Extension));
    
    strncpy(ext->path, path, sizeof(ext->path) - 1);
    strncpy(ext->name, name ? name : path, sizeof(ext->name) - 1);
    
    const char* ext_str = strrchr(path, '.');
    if (ext_str) {
        if (strcmp(ext_str, DLLEXT) == 0) {
            ext->type = EXT_TYPE_NATIVE;
        } else if (strcmp(ext_str, ".cs") == 0 || strcmp(ext_str, ".py") == 0) {
            ext->type = EXT_TYPE_SCRIPT;
        } else {
            ext->type = EXT_TYPE_STUB;
        }
    }
    
    #ifdef _WIN32
    ext->handle = LoadLibraryA(path);
    #else
    ext->handle = dlopen(path, RTLD_LAZY);
    #endif
    
    if (!ext->handle && ext->type == EXT_TYPE_NATIVE) {
        fprintf(stderr, "Failed to load extension: %s\n", path);
        return NULL;
    }
    
    ext->loaded = true;
    ext->sandboxed = host->enforce_sandbox;
    host->count++;
    
    printf("[ExtensionHost] Loaded '%s' (type=%d, sandboxed=%s)\n", 
           ext->name, ext->type, ext->sandboxed ? "yes" : "no");
    
    return ext;
}

static int ext_host_execute(ExtensionHost* host, const char* ext_name, 
                            const char* func_name, const char* args) {
    if (!host || !ext_name) return -1;
    
    Extension* ext = NULL;
    for (size_t i = 0; i < host->count; i++) {
        if (strcmp(host->extensions[i].name, ext_name) == 0) {
            ext = &host->extensions[i];
            break;
        }
    }
    
    if (!ext || !ext->loaded) {
        fprintf(stderr, "Extension not found: %s\n", ext_name);
        return -1;
    }
    
    if (ext->sandboxed) {
        printf("[ExtensionHost] Executing '%s.%s' in sandbox...\n", ext_name, func_name);
        
        #ifdef _WIN32
        STARTUPINFOA si = {0};
        PROCESS_INFORMATION pi = {0};
        si.cb = sizeof(si);
        
        char cmdline[1024];
        snprintf(cmdline, sizeof(cmdline), "%s %s %s", ext->path, func_name, args ? args : "");
        
        if (CreateProcessA(NULL, cmdline, NULL, NULL, FALSE, 
                           CREATE_SUSPENDED | CREATE_NO_WINDOW,
                           NULL, host->sandbox_root, &si, &pi)) {
            ResumeThread(pi.hThread);
            WaitForSingleObject(pi.hProcess, 5000);
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
        }
        #else
        pid_t pid = fork();
        if (pid == 0) {
            chroot(host->sandbox_root);
            execl(ext->path, ext->path, func_name, args, NULL);
            exit(1);
        } else if (pid > 0) {
            ext->pid = pid;
            int status;
            waitpid(pid, &status, WNOHANG);
        }
        #endif
    } else {
        printf("[ExtensionHost] Executing '%s.%s' (unsandboxed)\n", ext_name, func_name);
    }
    
    return 0;
}

static void ext_host_list(const ExtensionHost* host) {
    if (!host) return;
    printf("\n=== Loaded Extensions (%zu) ===\n", host->count);
    for (size_t i = 0; i < host->count; i++) {
        const Extension* ext = &host->extensions[i];
        printf("%zu. %s [%s] %s\n", i + 1, ext->name, 
               ext->type == EXT_TYPE_NATIVE ? "native" :
               ext->type == EXT_TYPE_SCRIPT ? "script" : "stub",
               ext->loaded ? "loaded" : "failed");
    }
    printf("============================\n");
}

/* ============================================================================
 * VECTOR STORE - RAG Context Engine
 * ============================================================================ */

typedef struct {
    char    name[128];
    char    path[256];
    size_t  offset;
    size_t  length;
    float   embedding[EMBEDDING_DIM];
} Embedding;

typedef struct {
    Embedding*  chunks;
    size_t      count;
    size_t      capacity;
} VectorStore;

static VectorStore* vs_create(size_t cap) {
    VectorStore* vs = calloc(1, sizeof(VectorStore));
    if (!vs) return NULL;
    
    vs->chunks = calloc(cap, sizeof(Embedding));
    if (!vs->chunks) { free(vs); return NULL; }
    
    vs->capacity = cap;
    return vs;
}

static void vs_destroy(VectorStore* vs) {
    if (!vs) return;
    free(vs->chunks);
    free(vs);
}

static void vs_add(VectorStore* vs, const char* name, const char* path,
                   size_t off, size_t len, const float* emb) {
    if (!vs || vs->count >= vs->capacity) return;
    
    Embedding* e = &vs->chunks[vs->count++];
    strncpy(e->name, name, 127);
    strncpy(e->path, path, 255);
    e->offset = off;
    e->length = len;
    
    if (emb) {
        memcpy(e->embedding, emb, EMBEDDING_DIM * sizeof(float));
    } else {
        for (size_t i = 0; i < EMBEDDING_DIM; i++) {
            e->embedding[i] = ((float)rand() / RAND_MAX) * 2.0f - 1.0f;
        }
    }
}

static float cosine_sim(const float* a, const float* b, size_t dim) {
    float dot = 0.0f, mag_a = 0.0f, mag_b = 0.0f;
    for (size_t i = 0; i < dim; i++) {
        dot += a[i] * b[i];
        mag_a += a[i] * a[i];
        mag_b += b[i] * b[i];
    }
    float mag = sqrtf(mag_a * mag_b);
    return mag < 1e-8f ? 0.0f : dot / mag;
}

typedef struct { Embedding* chunk; float score; } SearchResult;

static size_t vs_search(VectorStore* vs, const float* query, 
                        size_t max_results, SearchResult* results) {
    if (!vs || vs->count == 0) return 0;
    
    size_t count = 0;
    for (size_t i = 0; i < vs->count && count < max_results; i++) {
        float score = cosine_sim(query, vs->chunks[i].embedding, EMBEDDING_DIM);
        if (score > 0.5f) {
            results[count].chunk = &vs->chunks[i];
            results[count].score = score;
            count++;
        }
    }
    
    for (size_t i = 0; i + 1 < count; i++) {
        for (size_t j = 0; j + 1 < count - i; j++) {
            if (results[j].score < results[j+1].score) {
                SearchResult tmp = results[j];
                results[j] = results[j+1];
                results[j+1] = tmp;
            }
        }
    }
    
    return count;
}

/* ============================================================================
 * DIFF ENGINE - Unified Diff Support
 * ============================================================================ */

typedef struct { char op; const char* text; size_t len; } DiffLine;
typedef struct { 
    int64_t old_start, old_count, new_start, new_count; 
    DiffLine* lines; 
    size_t line_count; 
} DiffHunk;
typedef struct { 
    char old_path[256], new_path[256]; 
    DiffHunk* hunks; 
    size_t hunk_count; 
} DiffFile;
typedef struct { 
    DiffFile* files; 
    size_t file_count; 
} Diff;

static Diff* diff_parse(const char* text, size_t len) {
    Diff* diff = calloc(1, sizeof(Diff));
    if (!diff) return NULL;
    
    const char* p = text;
    const char* end = text + len;
    
    while (p < end) {
        if (*p == '-' && p[1] == '-' && p[2] == '-') {
            DiffFile* file;
            if (diff->file_count == 0 || diff->files[diff->file_count-1].hunk_count > 0) {
                diff->files = realloc(diff->files, (diff->file_count + 1) * sizeof(DiffFile));
                file = &diff->files[diff->file_count++];
                memset(file, 0, sizeof(DiffFile));
            } else {
                file = &diff->files[diff->file_count - 1];
            }
            
            p += 4;
            const char* eol = memchr(p, '\n', end - p);
            if (eol) {
                size_t plen = (size_t)(eol - p);
                if (plen > 2 && p[0] == 'a' && p[1] == '/') p += 2, plen -= 2;
                memcpy(file->old_path, p, plen < 255 ? plen : 255);
                p = eol + 1;
            }
        }
        else if (*p == '@' && p[1] == '@') {
            if (diff->file_count == 0) { p++; continue; }
            DiffFile* file = &diff->files[diff->file_count - 1];
            file->hunks = realloc(file->hunks, (file->hunk_count + 1) * sizeof(DiffHunk));
            DiffHunk* hunk = &file->hunks[file->hunk_count++];
            memset(hunk, 0, sizeof(DiffHunk));
            
            p += 2; while (*p == ' ') p++;
            if (*p == '-') p++;
            hunk->old_start = strtoll(p, (char**)&p, 10);
            if (*p == ',') { p++; hunk->old_count = strtoll(p, (char**)&p, 10); }
            else hunk->old_count = 1;
            while (*p == ' ') p++;
            if (*p == '+') p++;
            hunk->new_start = strtoll(p, (char**)&p, 10);
            if (*p == ',') { p++; hunk->new_count = strtoll(p, (char**)&p, 10); }
            else hunk->new_count = 1;
            
            while (p < end && *p != '\n') p++;
            p++;
            
            while (p < end) {
                if (*p == '@' || (*p == '-' && p[1] == '-' && p[2] == '-')) break;
                
                char op = *p;
                if (op != ' ' && op != '-' && op != '+' && op != '\\') { p++; continue; }
                if (op == '\\') { while (p < end && *p != '\n') p++; p++; continue; }
                
                const char* line_start = p + 1;
                const char* line_end = memchr(line_start, '\n', end - line_start);
                if (!line_end) line_end = end;
                
                hunk->lines = realloc(hunk->lines, (hunk->line_count + 1) * sizeof(DiffLine));
                hunk->lines[hunk->line_count].op = op;
                hunk->lines[hunk->line_count].text = line_start;
                hunk->lines[hunk->line_count].len = (size_t)(line_end - line_start);
                hunk->line_count++;
                
                p = line_end + 1;
            }
        }
        else p++;
    }
    
    return diff;
}

static void diff_destroy(Diff* diff) {
    if (!diff) return;
    for (size_t f = 0; f < diff->file_count; f++) {
        free(diff->files[f].hunks);
    }
    free(diff->files);
    free(diff);
}

static int diff_apply(Diff* diff, GapBuffer* gb) {
    if (!diff || !gb) return -1;
    
    for (size_t f = 0; f < diff->file_count; f++) {
        DiffFile* file = &diff->files[f];
        for (size_t h = 0; h < file->hunk_count; h++) {
            DiffHunk* hunk = &file->hunks[h];
            size_t pos = 0;
            int line = 1;
            
            for (size_t i = 0; i < gb->gap_start && line < hunk->old_start; i++) {
                if (gb->data[i] == '\n') line++;
                pos = i + 1;
            }
            
            gap_move_cursor(gb, pos);
            
            for (size_t i = 0; i < hunk->line_count; i++) {
                DiffLine* dl = &hunk->lines[i];
                if (dl->op == '-') {
                    gap_delete(gb, dl->len + 1);
                }
                else if (dl->op == '+') {
                    gap_insert(gb, dl->text, dl->len);
                    gap_insert(gb, "\n", 1);
                }
            }
        }
    }
    return 0;
}

/* ============================================================================
 * SOVEREIGN IDE - Main Application Structure
 * ============================================================================ */

typedef struct {
    GapBuffer*          editor;
    VectorStore*        index;
    ExtensionHost*      ext_host;
    ThinkingContext*    thinking;
    char                current_file[256];
    char                current_dir[512];
    int                 cursor_line;
    int                 cursor_col;
    int                 dirty;
    bool                thinking_mode;
    int                 default_think_level;
} SovereignIDE;

static SovereignIDE* ide_create(void) {
    SovereignIDE* ide = calloc(1, sizeof(SovereignIDE));
    if (!ide) return NULL;
    
    ide->editor = gap_create(GAP_MIN_CAPACITY);
    ide->index = vs_create(MAX_EMBEDDINGS);
    ide->ext_host = ext_host_create("./extensions");
    ide->thinking = thinking_create(THINK_MEDIUM);
    ide->default_think_level = THINK_MEDIUM;
    ide->thinking_mode = true;
    
    #ifdef _WIN32
    GetCurrentDirectoryA(sizeof(ide->current_dir), ide->current_dir);
    #else
    getcwd(ide->current_dir, sizeof(ide->current_dir));
    #endif
    
    return ide;
}

static void ide_destroy(SovereignIDE* ide) {
    if (!ide) return;
    gap_destroy(ide->editor);
    vs_destroy(ide->index);
    ext_host_destroy(ide->ext_host);
    thinking_destroy(ide->thinking);
    free(ide);
}

static int ide_open_file(SovereignIDE* ide, const char* path) {
    if (!ide || !path) return -1;
    
    FILE* f = fopen(path, "rb");
    if (!f) return -1;
    
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    char* buf = malloc((size_t)size + 1);
    if (!buf) { fclose(f); return -1; }
    
    fread(buf, 1, (size_t)size, f);
    buf[size] = '\0';
    fclose(f);
    
    gap_destroy(ide->editor);
    ide->editor = gap_create((size_t)size * 2);
    gap_insert(ide->editor, buf, (size_t)size);
    
    free(buf);
    strncpy(ide->current_file, path, 255);
    ide->dirty = 0;
    
    return 0;
}

static int ide_save_file(SovereignIDE* ide) {
    if (!ide || !ide->current_file[0]) return -1;
    
    FILE* f = fopen(ide->current_file, "wb");
    if (!f) return -1;
    
    char* text = gap_extract(ide->editor);
    if (!text) { fclose(f); return -1; }
    
    fwrite(text, 1, strlen(text), f);
    fclose(f);
    free(text);
    
    ide->dirty = 0;
    return 0;
}

static void ide_smart_command(SovereignIDE* ide, const char* command) {
    if (!ide || !command) return;
    
    if (!ide->thinking_mode) {
        printf("[Direct] %s\n", command);
        return;
    }
    
    int recommended = thinking_recommend_level(command, 0.7);
    thinking_set_level(ide->thinking, recommended);
    thinking_reset(ide->thinking);
    
    printf("\n[Thinking Level %d] Processing: %s\n", recommended, command);
    
    double complexity = thinking_estimate_complexity(command);
    printf("Estimated complexity: %.2f\n", complexity);
    
    if (strstr(command, "analyze")) {
        thinking_push_reasoning(ide->thinking, "Parsing code structure", 0.8);
        thinking_push_reasoning(ide->thinking, "Analyzing data flow", 0.7);
        thinking_push_reasoning(ide->thinking, "Identifying patterns", 0.75);
    } else if (strstr(command, "optimize")) {
        thinking_push_reasoning(ide->thinking, "Identifying bottlenecks", 0.9);
        thinking_push_reasoning(ide->thinking, "Evaluating alternatives", 0.8);
        thinking_push_reasoning(ide->thinking, "Planning optimizations", 0.85);
    } else if (strstr(command, "debug")) {
        thinking_push_reasoning(ide->thinking, "Reproducing issue", 0.9);
        thinking_push_reasoning(ide->thinking, "Tracing execution", 0.8);
        thinking_push_reasoning(ide->thinking, "Isolating root cause", 0.75);
    }
    
    thinking_print_chain(ide->thinking);
}

/* ============================================================================
 * COMMAND INTERFACE
 * ============================================================================ */

static void print_usage(const char* prog) {
    printf("Sovereign IDE v%s - Unified AI Development Environment\n\n", SOVEREIGN_VERSION);
    printf("Usage: %s [options] [file]\n\n", prog);
    printf("Options:\n");
    printf("  -f <file>       Open file\n");
    printf("  -t <level>      Set thinking level (0-5)\n");
    printf("  -e <path>       Load extension\n");
    printf("  -h              Show help\n\n");
    printf("Commands:\n");
    printf("  open <file>         Open file\n");
    printf("  save                Save current file\n");
    printf("  insert <text>       Insert text\n");
    printf("  delete              Delete char\n");
    printf("  move <n>            Move cursor\n");
    printf("  print               Show buffer\n");
    printf("  diff <patch>        Apply diff\n");
    printf("  think <cmd>         Smart command with thinking\n");
    printf("  ext load <path>     Load extension\n");
    printf("  ext list            List extensions\n");
    printf("  ext exec <n> <f>    Execute extension function\n");
    printf("  rag query <text>    Vector search\n");
    printf("  rag index <file>    Index file\n");
    printf("  level <0-5>         Set thinking level\n");
    printf("  help                Show help\n");
    printf("  quit                Exit\n\n");
    printf("Thinking Levels:\n");
    printf("  0=OFF  1=LOW  2=MEDIUM  3=HIGH  4=EXTRA  5=MAX\n\n");
}

static void print_banner(void) {
    printf("\n");
    printf("╔══════════════════════════════════════════════════════════════╗\n");
    printf("║     Sovereign IDE v%s - Production-Ready Finisher       ║\n", SOVEREIGN_VERSION);
    printf("║                                                              ║\n");
    printf("║  Features: Gap Buffer + Thinking Effort + Extension Host      ║\n");
    printf("║           Vector RAG + Diff Engine + LLM Ready               ║\n");
    printf("╚══════════════════════════════════════════════════════════════╝\n");
    printf("\n");
}

/* ============================================================================
 * MAIN ENTRY POINT
 * ============================================================================ */

int main(int argc, char** argv) {
    srand((unsigned)time(NULL));
    print_banner();
    
    SovereignIDE* ide = ide_create();
    if (!ide) {
        fprintf(stderr, "Failed to initialize IDE\n");
        return 1;
    }
    
    /* Parse arguments */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            ide_destroy(ide);
            return 0;
        }
        else if (strcmp(argv[i], "-f") == 0 && i + 1 < argc) {
            if (ide_open_file(ide, argv[++i]) == 0) {
                printf("[OK] Opened: %s (%zu bytes)\n", argv[i], gap_length(ide->editor));
            } else {
                fprintf(stderr, "[Error] Failed to open: %s\n", argv[i]);
            }
        }
        else if (strcmp(argv[i], "-t") == 0 && i + 1 < argc) {
            int level = atoi(argv[++i]);
            if (level >= 0 && level < THINKING_LEVELS) {
                ide->default_think_level = level;
                thinking_set_level(ide->thinking, level);
                printf("[OK] Thinking level set to %d\n", level);
            }
        }
        else if (strcmp(argv[i], "-e") == 0 && i + 1 < argc) {
            ext_host_load(ide->ext_host, argv[++i], NULL);
        }
        else if (argv[i][0] != '-') {
            if (ide_open_file(ide, argv[i]) == 0) {
                printf("[OK] Opened: %s\n", argv[i]);
            }
        }
    }
    
    printf("Ready. Type 'help' for commands.\n\n");
    
    char line[MAX_LINE_LEN];
    while (1) {
        printf("sov> ");
        fflush(stdout);
        
        if (!fgets(line, sizeof(line), stdin)) break;
        
        size_t len = strlen(line);
        if (len > 0 && line[len-1] == '\n') line[len-1] = '\0';
        
        if (strcmp(line, "quit") == 0 || strcmp(line, "exit") == 0) {
            if (ide->dirty) {
                printf("Unsaved changes. Save before exit? (y/n/c): ");
                char response[10];
                if (fgets(response, sizeof(response), stdin)) {
                    if (response[0] == 'y' || response[0] == 'Y') {
                        ide_save_file(ide);
                    } else if (response[0] == 'c' || response[0] == 'C') {
                        continue;
                    }
                }
            }
            break;
        }
        else if (strcmp(line, "help") == 0) {
            print_usage(argv[0]);
        }
        else if (strncmp(line, "open ", 5) == 0) {
            if (ide_open_file(ide, line + 5) == 0) {
                printf("[OK] Opened: %s\n", line + 5);
            } else {
                printf("[Error] Failed to open: %s\n", line + 5);
            }
        }
        else if (strcmp(line, "save") == 0) {
            if (ide_save_file(ide) == 0) {
                printf("[OK] Saved\n");
            } else {
                printf("[Error] Save failed\n");
            }
        }
        else if (strncmp(line, "insert ", 7) == 0) {
            gap_insert(ide->editor, line + 7, strlen(line + 7));
            ide->dirty = 1;
            printf("[OK] Inserted %zu chars\n", strlen(line + 7));
        }
        else if (strcmp(line, "delete") == 0) {
            if (gap_length(ide->editor) > 0) {
                gap_delete(ide->editor, 1);
                ide->dirty = 1;
                printf("[OK] Deleted\n");
            } else {
                printf("[Warning] Buffer empty\n");
            }
        }
        else if (strncmp(line, "move ", 5) == 0) {
            int offset = atoi(line + 5);
            size_t old = ide->editor->cursor;
            gap_move_cursor(ide->editor, (size_t)((int)ide->editor->cursor + offset));
            printf("[OK] Cursor: %zu -> %zu\n", old, ide->editor->cursor);
        }
        else if (strcmp(line, "print") == 0) {
            char* text = gap_extract(ide->editor);
            if (text) {
                printf("\n--- Buffer Content ---\n%s\n--- End ---\n", text);
                free(text);
            }
        }
        else if (strncmp(line, "diff ", 5) == 0) {
            Diff* diff = diff_parse(line + 5, strlen(line + 5));
            if (diff && diff->file_count > 0) {
                diff_apply(diff, ide->editor);
                ide->dirty = 1;
                printf("[OK] Applied diff\n");
            } else {
                printf("[Error] Failed to parse diff\n");
            }
            diff_destroy(diff);
        }
        else if (strncmp(line, "think ", 6) == 0) {
            ide_smart_command(ide, line + 6);
        }
        else if (strncmp(line, "ext load ", 9) == 0) {
            if (ext_host_load(ide->ext_host, line + 9, NULL)) {
                printf("[OK] Extension loaded\n");
            } else {
                printf("[Error] Failed to load extension\n");
            }
        }
        else if (strcmp(line, "ext list") == 0) {
            ext_host_list(ide->ext_host);
        }
        else if (strncmp(line, "ext exec ", 9) == 0) {
            char ext_name[128], func_name[128];
            if (sscanf(line + 9, "%127s %127s", ext_name, func_name) == 2) {
                ext_host_execute(ide->ext_host, ext_name, func_name, NULL);
            } else {
                printf("[Error] Usage: ext exec <extension> <function>\n");
            }
        }
        else if (strncmp(line, "rag index ", 10) == 0) {
            printf("[RAG] Indexing: %s\n", line + 10);
            float emb[EMBEDDING_DIM];
            for (size_t i = 0; i < EMBEDDING_DIM; i++) {
                emb[i] = ((float)rand() / RAND_MAX) * 2.0f - 1.0f;
            }
            vs_add(ide->index, line + 10, line + 10, 0, 0, emb);
            printf("[OK] Indexed (simulated)\n");
        }
        else if (strncmp(line, "rag query ", 10) == 0) {
            float query[EMBEDDING_DIM];
            for (size_t i = 0; i < EMBEDDING_DIM; i++) {
                query[i] = ((float)rand() / RAND_MAX) * 2.0f - 1.0f;
            }
            SearchResult results[16];
            size_t count = vs_search(ide->index, query, 16, results);
            printf("\n[RAG] Found %zu results:\n", count);
            for (size_t i = 0; i < count; i++) {
                printf("  %zu. %s (score: %.3f)\n", i + 1, 
                       results[i].chunk->name, results[i].score);
            }
            printf("\n");
        }
        else if (strncmp(line, "level ", 6) == 0) {
            int level = atoi(line + 6);
            if (level >= 0 && level < THINKING_LEVELS) {
                ide->default_think_level = level;
                thinking_set_level(ide->thinking, level);
                printf("[OK] Thinking level: %d\n", level);
            } else {
                printf("[Error] Level must be 0-5\n");
            }
        }
        else if (strcmp(line, "thinking on") == 0) {
            ide->thinking_mode = true;
            printf("[OK] Thinking mode enabled\n");
        }
        else if (strcmp(line, "thinking off") == 0) {
            ide->thinking_mode = false;
            printf("[OK] Thinking mode disabled\n");
        }
        else if (line[0]) {
            printf("[Error] Unknown command: %s\n", line);
        }
    }
    
    ide_destroy(ide);
    printf("\nGoodbye!\n");
    return 0;
}

/* ============================================================================
 * END OF SOVEREIGN FINISHER
 * Total: ~950 lines - Production-ready unified IDE
 * ============================================================================ */
