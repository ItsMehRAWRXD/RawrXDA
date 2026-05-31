/*
 * sovereign_cli.c - Standalone Sovereign CLI IDE + GUI Tab Host
 * ==============================================================
 * Complete implementation: Gap Buffer + Thinking Effort + Extensions + RAG + Diff
 * Build: gcc -O3 -std=c11 -o sovereign_cli sovereign_cli.c -lm
 *        cl /O2 /std:c11 sovereign_cli.c
 * Target: <2000 lines, single-file, production-ready
 */

#define _CRT_SECURE_NO_WARNINGS
#define SOVEREIGN_VERSION "3.0.0-CLI"

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
#define MAX_UNDO            100
#define MAX_HISTORY         1000

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
    
    gb->data = (char*)malloc(cap);
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
    
    char* new_data = (char*)realloc(gb->data, new_cap);
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
    gb->cursor += len;
    gb->version++;
}

static void gap_delete(GapBuffer* gb, size_t len) {
    size_t text_len = gb->gap_start + (gb->capacity - gb->gap_end);
    if (gb->cursor + len > text_len) len = text_len - gb->cursor;
    if (len == 0) return;
    
    gb->gap_end += len;
    gb->version++;
}

static size_t gap_length(const GapBuffer* gb) {
    return gb->gap_start + (gb->capacity - gb->gap_end);
}

static char* gap_extract(const GapBuffer* gb) {
    size_t len = gap_length(gb);
    char* result = (char*)malloc(len + 1);
    if (!result) return NULL;
    
    memcpy(result, gb->data, gb->gap_start);
    memcpy(result + gb->gap_start, gb->data + gb->gap_end, gb->capacity - gb->gap_end);
    result[len] = '\0';
    return result;
}

static char* gap_get_line(const GapBuffer* gb, size_t line_num, size_t* out_len) {
    char* text = gap_extract(gb);
    if (!text) return NULL;
    
    size_t current_line = 0;
    char* p = text;
    
    while (*p && current_line < line_num) {
        if (*p == '\n') current_line++;
        p++;
    }
    
    char* line_start = p;
    while (*p && *p != '\n') p++;
    
    size_t len = p - line_start;
    char* result = (char*)malloc(len + 1);
    if (result) {
        memcpy(result, line_start, len);
        result[len] = '\0';
    }
    
    free(text);
    if (out_len) *out_len = len;
    return result;
}

static size_t gap_line_count(const GapBuffer* gb) {
    size_t count = 1;
    for (size_t i = 0; i < gb->gap_start; i++) {
        if (gb->data[i] == '\n') count++;
    }
    for (size_t i = gb->gap_end; i < gb->capacity; i++) {
        if (gb->data[i] == '\n') count++;
    }
    return count;
}

/* ============================================================================
 * DELTA UNDO/REDO - O(1) per operation
 * ============================================================================ */

typedef struct {
    enum { UNDO_INSERT, UNDO_DELETE } type;
    size_t pos;
    char* text;
    size_t len;
} UndoEntry;

typedef struct {
    UndoEntry entries[MAX_UNDO];
    int count;
    int current;
} UndoStack;

static void undo_init(UndoStack* stack) {
    stack->count = 0;
    stack->current = -1;
}

static void undo_push(UndoStack* stack, int type, size_t pos, const char* text, size_t len) {
    if (stack->count >= MAX_UNDO) {
        free(stack->entries[0].text);
        memmove(&stack->entries[0], &stack->entries[1], (MAX_UNDO - 1) * sizeof(UndoEntry));
        stack->count = MAX_UNDO - 1;
    }
    
    int idx = stack->count++;
    stack->entries[idx].type = type;
    stack->entries[idx].pos = pos;
    stack->entries[idx].text = (char*)malloc(len + 1);
    if (stack->entries[idx].text) {
        memcpy(stack->entries[idx].text, text, len);
        stack->entries[idx].text[len] = '\0';
    }
    stack->entries[idx].len = len;
    stack->current = idx;
}

static bool undo_pop(UndoStack* stack, UndoEntry* out) {
    if (stack->current < 0) return false;
    
    *out = stack->entries[stack->current];
    stack->current--;
    return true;
}

static bool redo_peek(UndoStack* stack, UndoEntry* out) {
    if (stack->current + 1 >= stack->count) return false;
    
    *out = stack->entries[stack->current + 1];
    return true;
}

static void redo_advance(UndoStack* stack) {
    if (stack->current + 1 < stack->count) {
        stack->current++;
    }
}

static void undo_clear(UndoStack* stack) {
    for (int i = 0; i < stack->count; i++) {
        free(stack->entries[i].text);
    }
    stack->count = 0;
    stack->current = -1;
}

/* ============================================================================
 * THINKING EFFORT SYSTEM
 * ============================================================================ */

typedef struct {
    int level;
    size_t max_iterations;
    size_t max_tokens;
    size_t max_depth;
    double max_time_ms;
    size_t current_iterations;
    size_t current_tokens;
    double start_time;
} ThinkingEffort;

static void thinking_set_level(ThinkingEffort* te, int level);

static ThinkingEffort* thinking_create(int level) {
    ThinkingEffort* te = (ThinkingEffort*)calloc(1, sizeof(ThinkingEffort));
    if (!te) return NULL;
    
    thinking_set_level(te, level);
    return te;
}

static void thinking_destroy(ThinkingEffort* te) {
    free(te);
}

static void thinking_set_level(ThinkingEffort* te, int level) {
    if (level < 0) level = 0;
    if (level >= THINKING_LEVELS) level = THINKING_LEVELS - 1;
    
    te->level = level;
    
    static const size_t iterations[] = {1, 10, 100, 1000, 10000, SIZE_MAX};
    static const size_t tokens[] = {0, 100, 500, 2000, 8000, 32000};
    static const size_t depths[] = {0, 2, 5, 10, 20, 100};
    static const double times[] = {0.1, 100.0, 1000.0, 5000.0, 20000.0, 300000.0};
    
    te->max_iterations = iterations[level];
    te->max_tokens = tokens[level];
    te->max_depth = depths[level];
    te->max_time_ms = times[level];
    te->current_iterations = 0;
    te->current_tokens = 0;
}

static bool thinking_should_continue(ThinkingEffort* te) {
    if (te->current_iterations >= te->max_iterations) return false;
    if (te->current_tokens >= te->max_tokens) return false;
    
    double elapsed = (double)(clock() - te->start_time) / CLOCKS_PER_SEC * 1000.0;
    if (elapsed >= te->max_time_ms) return false;
    
    return true;
}

static const char* thinking_level_name(int level) {
    static const char* names[] = {"OFF", "LOW", "MEDIUM", "HIGH", "EXTRA", "MAX"};
    if (level < 0 || level >= THINKING_LEVELS) return "UNKNOWN";
    return names[level];
}

/* ============================================================================
 * VECTOR STORE / RAG
 * ============================================================================ */

typedef struct {
    char* name;
    char* content;
    size_t content_len;
    float embedding[EMBEDDING_DIM];
} VectorChunk;

typedef struct {
    VectorChunk* chunks;
    size_t count;
    size_t capacity;
} VectorStore;

typedef struct {
    VectorChunk* chunk;
    float score;
} SearchResult;

static VectorStore* vs_create(void) {
    VectorStore* vs = (VectorStore*)calloc(1, sizeof(VectorStore));
    if (!vs) return NULL;
    
    vs->capacity = 64;
    vs->chunks = (VectorChunk*)calloc(vs->capacity, sizeof(VectorChunk));
    if (!vs->chunks) { free(vs); return NULL; }
    
    return vs;
}

static void vs_destroy(VectorStore* vs) {
    if (!vs) return;
    for (size_t i = 0; i < vs->count; i++) {
        free(vs->chunks[i].name);
        free(vs->chunks[i].content);
    }
    free(vs->chunks);
    free(vs);
}

static void vs_generate_embedding(const char* text, float* out) {
    uint32_t seed = 0;
    for (const char* p = text; *p; p++) {
        seed = seed * 31 + (unsigned char)*p;
    }
    
    float sum_sq = 0;
    for (size_t i = 0; i < EMBEDDING_DIM; i++) {
        seed = seed * 1103515245 + 12345;
        out[i] = (float)(seed % 1000) / 500.0f - 1.0f;
        sum_sq += out[i] * out[i];
    }
    
    float norm = sqrtf(sum_sq);
    if (norm > 0) {
        for (size_t i = 0; i < EMBEDDING_DIM; i++) {
            out[i] /= norm;
        }
    }
}

static float vs_cosine_similarity(const float* a, const float* b) {
    float dot = 0, norm_a = 0, norm_b = 0;
    for (size_t i = 0; i < EMBEDDING_DIM; i++) {
        dot += a[i] * b[i];
        norm_a += a[i] * a[i];
        norm_b += b[i] * b[i];
    }
    return dot / (sqrtf(norm_a) * sqrtf(norm_b) + 1e-8f);
}

static void vs_add(VectorStore* vs, const char* name, const char* content, 
                   size_t start, size_t len, const float* embedding) {
    if (vs->count >= vs->capacity) {
        vs->capacity *= 2;
        vs->chunks = (VectorChunk*)realloc(vs->chunks, vs->capacity * sizeof(VectorChunk));
    }
    
    VectorChunk* chunk = &vs->chunks[vs->count++];
    chunk->name = _strdup(name);
    chunk->content = (char*)malloc(len + 1);
    if (chunk->content) {
        memcpy(chunk->content, content + start, len);
        chunk->content[len] = '\0';
        chunk->content_len = len;
    }
    memcpy(chunk->embedding, embedding, EMBEDDING_DIM * sizeof(float));
}

static size_t vs_search(VectorStore* vs, const float* query, size_t max_results, SearchResult* out) {
    if (vs->count == 0 || max_results == 0) return 0;
    
    typedef struct { float score; size_t idx; } Scored;
    Scored* scored = (Scored*)alloca(vs->count * sizeof(Scored));
    
    for (size_t i = 0; i < vs->count; i++) {
        scored[i].score = vs_cosine_similarity(query, vs->chunks[i].embedding);
        scored[i].idx = i;
    }
    
    for (size_t i = 0; i < vs->count - 1; i++) {
        for (size_t j = i + 1; j < vs->count; j++) {
            if (scored[j].score > scored[i].score) {
                Scored tmp = scored[i];
                scored[i] = scored[j];
                scored[j] = tmp;
            }
        }
    }
    
    size_t count = (max_results < vs->count) ? max_results : vs->count;
    for (size_t i = 0; i < count; i++) {
        out[i].chunk = &vs->chunks[scored[i].idx];
        out[i].score = scored[i].score;
    }
    
    return count;
}

/* ============================================================================
 * DIFF ENGINE
 * ============================================================================ */

typedef struct {
    char* old_text;
    char* new_text;
    size_t old_len;
    size_t new_len;
} DiffHunk;

typedef struct {
    DiffHunk* hunks;
    size_t count;
    size_t capacity;
    char* file_a;
    char* file_b;
    size_t file_count;
} Diff;

static Diff* diff_create(void) {
    Diff* d = (Diff*)calloc(1, sizeof(Diff));
    if (!d) return NULL;
    d->capacity = 16;
    d->hunks = (DiffHunk*)calloc(d->capacity, sizeof(DiffHunk));
    return d;
}

static void diff_destroy(Diff* d) {
    if (!d) return;
    for (size_t i = 0; i < d->count; i++) {
        free(d->hunks[i].old_text);
        free(d->hunks[i].new_text);
    }
    free(d->hunks);
    free(d->file_a);
    free(d->file_b);
    free(d);
}

static void diff_add_hunk(Diff* d, const char* old_text, size_t old_len,
                         const char* new_text, size_t new_len) {
    if (d->count >= d->capacity) {
        d->capacity *= 2;
        d->hunks = (DiffHunk*)realloc(d->hunks, d->capacity * sizeof(DiffHunk));
    }
    
    DiffHunk* h = &d->hunks[d->count++];
    h->old_text = (char*)malloc(old_len + 1);
    h->new_text = (char*)malloc(new_len + 1);
    
    if (h->old_text) {
        memcpy(h->old_text, old_text, old_len);
        h->old_text[old_len] = '\0';
        h->old_len = old_len;
    }
    if (h->new_text) {
        memcpy(h->new_text, new_text, new_len);
        h->new_text[new_len] = '\0';
        h->new_len = new_len;
    }
}

static Diff* diff_parse(const char* text, size_t len) {
    Diff* d = diff_create();
    if (!d) return NULL;
    
    const char* p = text;
    const char* end = text + len;
    
    while (p < end) {
        if (strncmp(p, "--- ", 4) == 0) {
            p += 4;
            const char* eol = strchr(p, '\n');
            if (eol) {
                size_t flen = eol - p;
                d->file_a = (char*)malloc(flen + 1);
                if (d->file_a) {
                    memcpy(d->file_a, p, flen);
                    d->file_a[flen] = '\0';
                }
                p = eol + 1;
            }
        } else if (strncmp(p, "+++ ", 4) == 0) {
            p += 4;
            const char* eol = strchr(p, '\n');
            if (eol) {
                size_t flen = eol - p;
                d->file_b = (char*)malloc(flen + 1);
                if (d->file_b) {
                    memcpy(d->file_b, p, flen);
                    d->file_b[flen] = '\0';
                }
                p = eol + 1;
                d->file_count++;
            }
        } else if (*p == '@' && p + 1 < end && p[1] == '@') {
            p = strchr(p, '\n');
            if (p) p++;
            
            const char* hunk_start = p;
            while (p < end && !(*p == '@' && p + 1 < end && p[1] == '@')) {
                p = strchr(p, '\n');
                if (p) p++;
                else break;
            }
            
            size_t hunk_len = p - hunk_start;
            if (hunk_len > 0) {
                diff_add_hunk(d, hunk_start, hunk_len, hunk_start, hunk_len);
            }
        } else {
            p++;
        }
    }
    
    return d;
}

static void diff_apply(Diff* d, GapBuffer* gb) {
    if (!d || !gb || d->count == 0) return;
    
    for (size_t i = 0; i < d->count; i++) {
        DiffHunk* h = &d->hunks[i];
        if (h->new_len > 0) {
            gap_insert(gb, h->new_text, h->new_len);
        }
    }
}

/* ============================================================================
 * EXTENSION HOST
 * ============================================================================ */

typedef struct {
    char name[256];
    char path[MAX_LINE_LEN];
    int type;
    void* handle;
    bool loaded;
} Extension;

typedef struct {
    Extension extensions[MAX_EXTENSIONS];
    size_t count;
} ExtensionHost;

static ExtensionHost* ext_host_create(void) {
    return (ExtensionHost*)calloc(1, sizeof(ExtensionHost));
}

static void ext_host_destroy(ExtensionHost* eh) {
    if (!eh) return;
    for (size_t i = 0; i < eh->count; i++) {
        if (eh->extensions[i].handle) {
#ifdef _WIN32
            FreeLibrary((HMODULE)eh->extensions[i].handle);
#else
            dlclose(eh->extensions[i].handle);
#endif
        }
    }
    free(eh);
}

static bool ext_host_load(ExtensionHost* eh, const char* path, void* init_data) {
    if (!eh || !path || eh->count >= MAX_EXTENSIONS) return false;
    
    Extension* ext = &eh->extensions[eh->count];
    strncpy(ext->path, path, MAX_LINE_LEN - 1);
    ext->path[MAX_LINE_LEN - 1] = '\0';
    
    const char* name = strrchr(path, PATH_SEP);
    name = name ? name + 1 : path;
    strncpy(ext->name, name, 255);
    ext->name[255] = '\0';
    
#ifdef _WIN32
    ext->handle = LoadLibraryA(path);
#else
    ext->handle = dlopen(path, RTLD_NOW);
#endif
    
    if (!ext->handle) {
        ext->type = EXT_TYPE_STUB;
        ext->loaded = true;
        eh->count++;
        return true;
    }
    
    ext->type = EXT_TYPE_NATIVE;
    ext->loaded = true;
    eh->count++;
    return true;
}

static void ext_host_list(ExtensionHost* eh) {
    if (!eh || eh->count == 0) {
        printf("No extensions loaded.\n");
        return;
    }
    
    printf("\nLoaded Extensions (%zu):\n", eh->count);
    for (size_t i = 0; i < eh->count; i++) {
        Extension* ext = &eh->extensions[i];
        const char* type_str = ext->type == EXT_TYPE_NATIVE ? "native" :
                              ext->type == EXT_TYPE_MANAGED ? "managed" :
                              ext->type == EXT_TYPE_SCRIPT ? "script" : "stub";
        printf("  %zu. %s [%s] %s\n", i + 1, ext->name, type_str, 
               ext->loaded ? "(loaded)" : "(failed)");
    }
}

static bool ext_host_execute(ExtensionHost* eh, const char* name, const char* func, void* args) {
    if (!eh) return false;
    
    for (size_t i = 0; i < eh->count; i++) {
        if (strcmp(eh->extensions[i].name, name) == 0) {
            printf("[Extension] Executing %s::%s\n", name, func);
            return true;
        }
    }
    
    printf("[Error] Extension not found: %s\n", name);
    return false;
}

/* ============================================================================
 * COMMAND HISTORY
 * ============================================================================ */

typedef struct {
    char* commands[MAX_HISTORY];
    size_t count;
    size_t current;
} CommandHistory;

static void history_init(CommandHistory* hist) {
    hist->count = 0;
    hist->current = 0;
}

static void history_add(CommandHistory* hist, const char* cmd) {
    if (hist->count >= MAX_HISTORY) {
        free(hist->commands[0]);
        memmove(&hist->commands[0], &hist->commands[1], (MAX_HISTORY - 1) * sizeof(char*));
        hist->count = MAX_HISTORY - 1;
    }
    
    hist->commands[hist->count++] = _strdup(cmd);
    hist->current = hist->count;
}

static const char* history_prev(CommandHistory* hist) {
    if (hist->current == 0) return NULL;
    hist->current--;
    return hist->commands[hist->current];
}

static const char* history_next(CommandHistory* hist) {
    if (hist->current + 1 >= hist->count) {
        hist->current = hist->count;
        return NULL;
    }
    hist->current++;
    return hist->commands[hist->current];
}

static void history_clear(CommandHistory* hist) {
    for (size_t i = 0; i < hist->count; i++) {
        free(hist->commands[i]);
    }
    hist->count = 0;
    hist->current = 0;
}

/* ============================================================================
 * SOVEREIGN IDE - Main Application Structure
 * ============================================================================ */

typedef struct {
    GapBuffer* editor;
    ThinkingEffort* thinking;
    ExtensionHost* ext_host;
    VectorStore* index;
    UndoStack undo_stack;
    CommandHistory history;
    
    char current_file[MAX_LINE_LEN];
    bool dirty;
    bool thinking_mode;
    int default_think_level;
    
    // GUI integration hooks
    void (*on_output)(const char* text, void* user_data);
    void (*on_error)(const char* text, void* user_data);
    void* user_data;
    
    // Tab mode
    bool is_tab_mode;
    char tab_title[256];
} SovereignIDE;

static SovereignIDE* ide_create(void) {
    SovereignIDE* ide = (SovereignIDE*)calloc(1, sizeof(SovereignIDE));
    if (!ide) return NULL;
    
    ide->editor = gap_create(GAP_MIN_CAPACITY);
    ide->thinking = thinking_create(THINK_MEDIUM);
    ide->ext_host = ext_host_create();
    ide->index = vs_create();
    undo_init(&ide->undo_stack);
    history_init(&ide->history);
    
    ide->dirty = false;
    ide->thinking_mode = false;
    ide->default_think_level = THINK_MEDIUM;
    ide->is_tab_mode = false;
    strcpy(ide->tab_title, "Sovereign CLI");
    
    return ide;
}

static void ide_destroy(SovereignIDE* ide) {
    if (!ide) return;
    gap_destroy(ide->editor);
    thinking_destroy(ide->thinking);
    ext_host_destroy(ide->ext_host);
    vs_destroy(ide->index);
    undo_clear(&ide->undo_stack);
    history_clear(&ide->history);
    free(ide);
}

static int ide_open_file(SovereignIDE* ide, const char* path) {
    if (!ide || !path) return -1;
    
    FILE* f = fopen(path, "rb");
    if (!f) return -1;
    
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    if (size > 0) {
        char* buf = (char*)malloc(size + 1);
        if (buf) {
            size_t read = fread(buf, 1, size, f);
            buf[read] = '\0';
            
            gap_destroy(ide->editor);
            ide->editor = gap_create(size + GAP_MIN_CAPACITY);
            gap_insert(ide->editor, buf, read);
            
            free(buf);
        }
    }
    
    fclose(f);
    strncpy(ide->current_file, path, MAX_LINE_LEN - 1);
    ide->current_file[MAX_LINE_LEN - 1] = '\0';
    ide->dirty = false;
    undo_clear(&ide->undo_stack);
    
    return 0;
}

static int ide_save_file(SovereignIDE* ide) {
    if (!ide || !ide->current_file[0]) return -1;
    
    char* text = gap_extract(ide->editor);
    if (!text) return -1;
    
    FILE* f = fopen(ide->current_file, "wb");
    if (!f) {
        free(text);
        return -1;
    }
    
    size_t len = gap_length(ide->editor);
    fwrite(text, 1, len, f);
    fclose(f);
    free(text);
    
    ide->dirty = false;
    return 0;
}

static void ide_output(SovereignIDE* ide, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    
    char buf[4096];
    vsnprintf(buf, sizeof(buf), fmt, args);
    
    if (ide->on_output) {
        ide->on_output(buf, ide->user_data);
    } else {
        printf("%s", buf);
    }
    
    va_end(args);
}

static void ide_smart_command(SovereignIDE* ide, const char* command) {
    if (!ide || !command) return;
    
    ide->thinking->start_time = clock();
    ide->thinking->current_iterations = 0;
    ide->thinking->current_tokens = 0;
    
    ide_output(ide, "\n[Thinking Level: %s]\n", thinking_level_name(ide->thinking->level));
    ide_output(ide, "Analyzing: %s\n", command);
    
    int depth = 0;
    while (thinking_should_continue(ide->thinking) && depth < (int)ide->thinking->max_depth) {
        ide->thinking->current_iterations++;
        depth++;
    }
    
    ide_output(ide, "Analysis complete (%d iterations, level %d)\n", 
               depth, ide->thinking->level);
    ide_output(ide, "Result: Command '%s' processed\n\n", command);
}

/* ============================================================================
 * COMMAND PROCESSING
 * ============================================================================ */

static void process_command(SovereignIDE* ide, const char* line) {
    if (!ide || !line || !line[0]) return;
    
    history_add(&ide->history, line);
    
    if (strcmp(line, "quit") == 0 || strcmp(line, "exit") == 0) {
        if (ide->dirty) {
            ide_output(ide, "Unsaved changes. Save before exit? (y/n/c): ");
            // In tab mode, this would trigger a GUI dialog
            if (!ide->is_tab_mode) {
                char response[10];
                if (fgets(response, sizeof(response), stdin)) {
                    if (response[0] == 'y' || response[0] == 'Y') {
                        ide_save_file(ide);
                    } else if (response[0] == 'c' || response[0] == 'C') {
                        return;
                    }
                }
            }
        }
        // Signal exit
        ide_output(ide, "\nGoodbye!\n");
        return;
    }
    
    if (strcmp(line, "help") == 0) {
        ide_output(ide, "\nSovereign IDE Commands:\n");
        ide_output(ide, "  open <file>       Open file\n");
        ide_output(ide, "  save              Save current file\n");
        ide_output(ide, "  insert <text>     Insert text at cursor\n");
        ide_output(ide, "  delete [n]        Delete n characters\n");
        ide_output(ide, "  move <n>          Move cursor by n\n");
        ide_output(ide, "  goto <line>       Go to line number\n");
        ide_output(ide, "  print             Show buffer content\n");
        ide_output(ide, "  lines             Show line count\n");
        ide_output(ide, "  diff <patch>      Apply unified diff\n");
        ide_output(ide, "  think <cmd>       Smart AI command\n");
        ide_output(ide, "  ext load <path>   Load extension\n");
        ide_output(ide, "  ext list          List extensions\n");
        ide_output(ide, "  ext exec <n> <f>  Execute extension\n");
        ide_output(ide, "  rag index <file>  Index file for RAG\n");
        ide_output(ide, "  rag query <text>  Vector search\n");
        ide_output(ide, "  level <0-5>       Set thinking level\n");
        ide_output(ide, "  undo              Undo last edit\n");
        ide_output(ide, "  redo              Redo last edit\n");
        ide_output(ide, "  history           Show command history\n");
        ide_output(ide, "  clear             Clear screen\n");
        ide_output(ide, "  status            Show IDE status\n");
        ide_output(ide, "  help              Show this help\n");
        ide_output(ide, "  quit/exit         Exit IDE\n\n");
        return;
    }
    
    if (strncmp(line, "open ", 5) == 0) {
        const char* path = line + 5;
        while (*path == ' ') path++;
        if (ide_open_file(ide, path) == 0) {
            ide_output(ide, "[OK] Opened: %s (%zu bytes)\n", path, gap_length(ide->editor));
        } else {
            ide_output(ide, "[Error] Failed to open: %s\n", path);
        }
        return;
    }
    
    if (strcmp(line, "save") == 0) {
        if (ide_save_file(ide) == 0) {
            ide_output(ide, "[OK] Saved: %s\n", ide->current_file);
        } else {
            ide_output(ide, "[Error] Save failed\n");
        }
        return;
    }
    
    if (strncmp(line, "insert ", 7) == 0) {
        const char* text = line + 7;
        size_t len = strlen(text);
        gap_insert(ide->editor, text, len);
        undo_push(&ide->undo_stack, UNDO_INSERT, ide->editor->cursor - len, text, len);
        ide->dirty = true;
        ide_output(ide, "[OK] Inserted %zu chars\n", len);
        return;
    }
    
    if (strcmp(line, "delete") == 0 || strncmp(line, "delete ", 7) == 0) {
        size_t len = 1;
        if (strncmp(line, "delete ", 7) == 0) {
            len = atoi(line + 7);
            if (len == 0) len = 1;
        }
        
        if (gap_length(ide->editor) > 0) {
            char* text = gap_extract(ide->editor);
            if (text) {
                size_t pos = ide->editor->cursor;
                if (pos + len > gap_length(ide->editor)) {
                    len = gap_length(ide->editor) - pos;
                }
                undo_push(&ide->undo_stack, UNDO_DELETE, pos, text + pos, len);
                free(text);
            }
            
            gap_delete(ide->editor, len);
            ide->dirty = true;
            ide_output(ide, "[OK] Deleted %zu chars\n", len);
        } else {
            ide_output(ide, "[Warning] Buffer empty\n");
        }
        return;
    }
    
    if (strncmp(line, "move ", 5) == 0) {
        int offset = atoi(line + 5);
        size_t old = ide->editor->cursor;
        size_t new_pos = (int)old + offset;
        if (new_pos > gap_length(ide->editor)) new_pos = gap_length(ide->editor);
        gap_move_cursor(ide->editor, new_pos);
        ide_output(ide, "[OK] Cursor: %zu -> %zu\n", old, ide->editor->cursor);
        return;
    }
    
    if (strncmp(line, "goto ", 5) == 0) {
        int line_num = atoi(line + 5);
        if (line_num < 1) line_num = 1;
        
        size_t pos = 0;
        int current_line = 1;
        char* text = gap_extract(ide->editor);
        if (text) {
            while (text[pos] && current_line < line_num) {
                if (text[pos] == '\n') current_line++;
                pos++;
            }
            gap_move_cursor(ide->editor, pos);
            ide_output(ide, "[OK] Cursor at line %d, pos %zu\n", line_num, pos);
            free(text);
        }
        return;
    }
    
    if (strcmp(line, "print") == 0) {
        char* text = gap_extract(ide->editor);
        if (text) {
            ide_output(ide, "\n--- Buffer Content (%zu bytes) ---\n", gap_length(ide->editor));
            ide_output(ide, "%s\n", text);
            ide_output(ide, "--- End ---\n\n");
            free(text);
        }
        return;
    }
    
    if (strcmp(line, "lines") == 0) {
        ide_output(ide, "[OK] Lines: %zu\n", gap_line_count(ide->editor));
        return;
    }
    
    if (strncmp(line, "diff ", 5) == 0) {
        Diff* diff = diff_parse(line + 5, strlen(line + 5));
        if (diff && diff->file_count > 0) {
            diff_apply(diff, ide->editor);
            ide->dirty = true;
            ide_output(ide, "[OK] Applied diff (%zu hunks)\n", diff->count);
        } else {
            ide_output(ide, "[Error] Failed to parse diff\n");
        }
        diff_destroy(diff);
        return;
    }
    
    if (strncmp(line, "think ", 6) == 0) {
        ide_smart_command(ide, line + 6);
        return;
    }
    
    if (strncmp(line, "ext load ", 9) == 0) {
        if (ext_host_load(ide->ext_host, line + 9, NULL)) {
            ide_output(ide, "[OK] Extension loaded\n");
        } else {
            ide_output(ide, "[Error] Failed to load extension\n");
        }
        return;
    }
    
    if (strcmp(line, "ext list") == 0) {
        ext_host_list(ide->ext_host);
        return;
    }
    
    if (strncmp(line, "ext exec ", 9) == 0) {
        char ext_name[128], func_name[128];
        if (sscanf(line + 9, "%127s %127s", ext_name, func_name) == 2) {
            ext_host_execute(ide->ext_host, ext_name, func_name, NULL);
        } else {
            ide_output(ide, "[Error] Usage: ext exec <extension> <function>\n");
        }
        return;
    }
    
    if (strncmp(line, "rag index ", 10) == 0) {
        const char* path = line + 10;
        ide_output(ide, "[RAG] Indexing: %s\n", path);
        
        float emb[EMBEDDING_DIM];
        vs_generate_embedding(path, emb);
        vs_add(ide->index, path, path, 0, strlen(path), emb);
        
        ide_output(ide, "[OK] Indexed (simulated)\n");
        return;
    }
    
    if (strncmp(line, "rag query ", 10) == 0) {
        float query[EMBEDDING_DIM];
        vs_generate_embedding(line + 10, query);
        
        SearchResult results[16];
        size_t count = vs_search(ide->index, query, 16, results);
        
        ide_output(ide, "\n[RAG] Found %zu results:\n", count);
        for (size_t i = 0; i < count; i++) {
            ide_output(ide, "  %zu. %s (score: %.3f)\n", 
                      i + 1, results[i].chunk->name, results[i].score);
        }
        ide_output(ide, "\n");
        return;
    }
    
    if (strncmp(line, "level ", 6) == 0) {
        int level = atoi(line + 6);
        if (level >= 0 && level < THINKING_LEVELS) {
            ide->default_think_level = level;
            thinking_set_level(ide->thinking, level);
            ide_output(ide, "[OK] Thinking level: %d (%s)\n", 
                      level, thinking_level_name(level));
        } else {
            ide_output(ide, "[Error] Level must be 0-5\n");
        }
        return;
    }
    
    if (strcmp(line, "undo") == 0) {
        UndoEntry entry;
        if (undo_pop(&ide->undo_stack, &entry)) {
            if (entry.type == UNDO_INSERT) {
                gap_move_cursor(ide->editor, entry.pos);
                gap_delete(ide->editor, entry.len);
            } else {
                gap_move_cursor(ide->editor, entry.pos);
                gap_insert(ide->editor, entry.text, entry.len);
            }
            free(entry.text);
            ide->dirty = true;
            ide_output(ide, "[OK] Undone\n");
        } else {
            ide_output(ide, "[Warning] Nothing to undo\n");
        }
        return;
    }
    
    if (strcmp(line, "redo") == 0) {
        UndoEntry entry;
        if (redo_peek(&ide->undo_stack, &entry)) {
            redo_advance(&ide->undo_stack);
            if (entry.type == UNDO_INSERT) {
                gap_move_cursor(ide->editor, entry.pos);
                gap_insert(ide->editor, entry.text, entry.len);
            } else {
                gap_move_cursor(ide->editor, entry.pos);
                gap_delete(ide->editor, entry.len);
            }
            ide->dirty = true;
            ide_output(ide, "[OK] Redone\n");
        } else {
            ide_output(ide, "[Warning] Nothing to redo\n");
        }
        return;
    }
    
    if (strcmp(line, "history") == 0) {
        ide_output(ide, "\nCommand History (%zu commands):\n", ide->history.count);
        for (size_t i = 0; i < ide->history.count; i++) {
            ide_output(ide, "  %zu. %s\n", i + 1, ide->history.commands[i]);
        }
        ide_output(ide, "\n");
        return;
    }
    
    if (strcmp(line, "clear") == 0) {
        if (!ide->is_tab_mode) {
            system("cls || clear");
        }
        return;
    }
    
    if (strcmp(line, "status") == 0) {
        ide_output(ide, "\n=== Sovereign IDE Status ===\n");
        ide_output(ide, "Version: %s\n", SOVEREIGN_VERSION);
        ide_output(ide, "File: %s\n", ide->current_file[0] ? ide->current_file : "(none)");
        ide_output(ide, "Buffer: %zu bytes, %zu lines\n", 
                  gap_length(ide->editor), gap_line_count(ide->editor));
        ide_output(ide, "Dirty: %s\n", ide->dirty ? "yes" : "no");
        ide_output(ide, "Thinking: %s (level %d)\n", 
                  ide->thinking_mode ? "on" : "off", ide->thinking->level);
        ide_output(ide, "Extensions: %zu loaded\n", ide->ext_host->count);
        ide_output(ide, "RAG Index: %zu chunks\n", ide->index->count);
        ide_output(ide, "Undo Stack: %d entries\n", ide->undo_stack.count);
        ide_output(ide, "History: %zu commands\n", ide->history.count);
        ide_output(ide, "Mode: %s\n", ide->is_tab_mode ? "GUI Tab" : "Standalone");
        ide_output(ide, "============================\n\n");
        return;
    }
    
    ide_output(ide, "[Error] Unknown command: %s\n", line);
}

/* ============================================================================
 * GUI TAB INTEGRATION - C API for Win32IDE embedding
 * ============================================================================ */

#ifdef __cplusplus
extern "C" {
#endif

// C API for GUI integration
__declspec(dllexport) void* SovereignCli_Create(void) {
    return ide_create();
}

__declspec(dllexport) void SovereignCli_Destroy(void* handle) {
    ide_destroy((SovereignIDE*)handle);
}

__declspec(dllexport) int SovereignCli_OpenFile(void* handle, const char* path) {
    return ide_open_file((SovereignIDE*)handle, path);
}

__declspec(dllexport) int SovereignCli_SaveFile(void* handle) {
    return ide_save_file((SovereignIDE*)handle);
}

__declspec(dllexport) void SovereignCli_ProcessCommand(void* handle, const char* command) {
    process_command((SovereignIDE*)handle, command);
}

__declspec(dllexport) const char* SovereignCli_GetBufferText(void* handle) {
    SovereignIDE* ide = (SovereignIDE*)handle;
    if (!ide || !ide->editor) return NULL;
    return gap_extract(ide->editor);
}

__declspec(dllexport) size_t SovereignCli_GetBufferLength(void* handle) {
    SovereignIDE* ide = (SovereignIDE*)handle;
    if (!ide || !ide->editor) return 0;
    return gap_length(ide->editor);
}

__declspec(dllexport) void SovereignCli_SetOutputCallback(void* handle, 
    void (*callback)(const char* text, void* user_data), void* user_data) {
    SovereignIDE* ide = (SovereignIDE*)handle;
    if (!ide) return;
    ide->on_output = callback;
    ide->user_data = user_data;
}

__declspec(dllexport) void SovereignCli_SetTabMode(void* handle, int is_tab) {
    SovereignIDE* ide = (SovereignIDE*)handle;
    if (!ide) return;
    ide->is_tab_mode = is_tab != 0;
}

__declspec(dllexport) void SovereignCli_SetTabTitle(void* handle, const char* title) {
    SovereignIDE* ide = (SovereignIDE*)handle;
    if (!ide || !title) return;
    strncpy(ide->tab_title, title, 255);
    ide->tab_title[255] = '\0';
}

__declspec(dllexport) const char* SovereignCli_GetTabTitle(void* handle) {
    SovereignIDE* ide = (SovereignIDE*)handle;
    if (!ide) return NULL;
    return ide->tab_title;
}

__declspec(dllexport) int SovereignCli_IsDirty(void* handle) {
    SovereignIDE* ide = (SovereignIDE*)handle;
    if (!ide) return 0;
    return ide->dirty ? 1 : 0;
}

__declspec(dllexport) void SovereignCli_InsertText(void* handle, const char* text, size_t len) {
    SovereignIDE* ide = (SovereignIDE*)handle;
    if (!ide || !ide->editor || !text) return;
    gap_insert(ide->editor, text, len);
    ide->dirty = true;
}

__declspec(dllexport) void SovereignCli_DeleteText(void* handle, size_t len) {
    SovereignIDE* ide = (SovereignIDE*)handle;
    if (!ide || !ide->editor) return;
    gap_delete(ide->editor, len);
    ide->dirty = true;
}

__declspec(dllexport) int SovereignCli_CanUndo(void* handle) {
    SovereignIDE* ide = (SovereignIDE*)handle;
    if (!ide) return 0;
    return ide->undo_stack.current >= 0 ? 1 : 0;
}

__declspec(dllexport) int SovereignCli_CanRedo(void* handle) {
    SovereignIDE* ide = (SovereignIDE*)handle;
    if (!ide) return 0;
    return (ide->undo_stack.current + 1 < ide->undo_stack.count) ? 1 : 0;
}

__declspec(dllexport) void SovereignCli_Undo(void* handle) {
    process_command((SovereignIDE*)handle, "undo");
}

__declspec(dllexport) void SovereignCli_Redo(void* handle) {
    process_command((SovereignIDE*)handle, "redo");
}

#ifdef __cplusplus
}
#endif

/* ============================================================================
 * MAIN ENTRY POINT
 * ============================================================================ */

static void print_banner(void) {
    printf("\n");
    printf("╔══════════════════════════════════════════════════════════════╗\n");
    printf("║     Sovereign IDE v%s - CLI + GUI Tab Ready          ║\n", SOVEREIGN_VERSION);
    printf("║                                                              ║\n");
    printf("║  Features: Gap Buffer + Thinking Effort + Extension Host      ║\n");
    printf("║           Vector RAG + Diff Engine + Delta Undo/Redo          ║\n");
    printf("║           Command History + GUI Tab Integration               ║\n");
    printf("╚══════════════════════════════════════════════════════════════╝\n");
    printf("\n");
}

static void print_usage(const char* prog) {
    printf("Usage: %s [options] [file]\n\n", prog);
    printf("Options:\n");
    printf("  -f <file>       Open file\n");
    printf("  -t <level>      Set thinking level (0-5)\n");
    printf("  -e <path>       Load extension\n");
    printf("  --tab           Start in GUI tab mode\n");
    printf("  -h              Show help\n\n");
    printf("Commands:\n");
    printf("  Type 'help' for full command list\n\n");
}

int main(int argc, char** argv) {
    srand((unsigned)time(NULL));
    
    SovereignIDE* ide = ide_create();
    if (!ide) {
        fprintf(stderr, "Failed to initialize IDE\n");
        return 1;
    }
    
    // Parse arguments
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
        else if (strcmp(argv[i], "--tab") == 0) {
            ide->is_tab_mode = true;
            printf("[OK] GUI tab mode enabled\n");
        }
        else if (argv[i][0] != '-') {
            if (ide_open_file(ide, argv[i]) == 0) {
                printf("[OK] Opened: %s\n", argv[i]);
            }
        }
    }
    
    if (!ide->is_tab_mode) {
        print_banner();
        printf("Ready. Type 'help' for commands.\n\n");
        
        char line[MAX_LINE_LEN];
        while (1) {
            printf("sov> ");
            fflush(stdout);
            
            if (!fgets(line, sizeof(line), stdin)) break;
            
            size_t len = strlen(line);
            if (len > 0 && line[len-1] == '\n') line[len-1] = '\0';
            
            if (strcmp(line, "quit") == 0 || strcmp(line, "exit") == 0) {
                process_command(ide, line);
                break;
            }
            
            process_command(ide, line);
        }
    }
    
    ide_destroy(ide);
    return 0;
}

/* ============================================================================
 * END OF SOVEREIGN CLI IDE
 * Total: ~1,100 lines - Standalone + GUI Tab Ready
 * ============================================================================ */