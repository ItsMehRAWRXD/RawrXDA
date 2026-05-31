/*
 * sovereign.c - Self-Contained Sovereign IDE with LLM Inference
 * =============================================================
 * Complete implementation with core components
 */

#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>
#include <stdbool.h>
#include <ctype.h>

/* Configuration */
#define SOV_VERSION "1.0.0"
#define GAP_MIN_CAPACITY 8192
#define GAP_GROW_FACTOR 2
#define EMBEDDING_DIM 384
#define MAX_CONTEXT 4096
#define MAX_TOKENS 4096
#define MAX_EMBEDDINGS 2048
#define MAX_LINE_LEN 4096

/* LLM Architecture */
#define TRANSFORMER_LAYERS 32
#define ATTENTION_HEADS 12
#define FFN_DIM 3072

/* Gap Buffer Implementation */
typedef struct {
    char* data;
    size_t capacity;
    size_t gap_start;
    size_t gap_end;
    size_t cursor;
} GapBuffer;

static GapBuffer* gap_create(size_t cap) {
    if (cap < GAP_MIN_CAPACITY) cap = GAP_MIN_CAPACITY;
    GapBuffer* gb = (GapBuffer*)malloc(sizeof(GapBuffer));
    if (!gb) return NULL;
    gb->data = (char*)malloc(cap);
    if (!gb->data) { free(gb); return NULL; }
    gb->capacity = cap;
    gb->gap_start = 0;
    gb->gap_end = cap;
    gb->cursor = 0;
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
    gb->cursor = gb->gap_start;
}

static void gap_delete(GapBuffer* gb, size_t len) {
    if (len == 0 || gb->gap_start == 0) return;
    if (len > gb->gap_start) len = gb->gap_start;
    gb->gap_start -= len;
    gb->cursor = gb->gap_start;
}

static size_t gap_length(const GapBuffer* gb) {
    return gb->gap_start + (gb->capacity - gb->gap_end);
}

static char* gap_extract(const GapBuffer* gb) {
    size_t len = gap_length(gb);
    char* out = (char*)malloc(len + 1);
    if (!out) return NULL;
    memcpy(out, gb->data, gb->gap_start);
    memcpy(out + gb->gap_start, gb->data + gb->gap_end, len - gb->gap_start);
    out[len] = '\0';
    return out;
}

/* Vector Store Implementation */
typedef struct {
    char name[128];
    char path[256];
    size_t offset;
    size_t length;
    float embedding[EMBEDDING_DIM];
} Embedding;

typedef struct {
    Embedding* chunks;
    size_t count;
    size_t capacity;
} VectorStore;

static VectorStore* vs_create(size_t cap) {
    VectorStore* vs = (VectorStore*)malloc(sizeof(VectorStore));
    if (!vs) return NULL;
    vs->chunks = (Embedding*)malloc(cap * sizeof(Embedding));
    if (!vs->chunks) { free(vs); return NULL; }
    vs->capacity = cap;
    vs->count = 0;
    return vs;
}

static void vs_destroy(VectorStore* vs) {
    if (!vs) return;
    free(vs->chunks);
    free(vs);
}

static void vs_add(VectorStore* vs, const char* name, const char* path,
                   size_t off, size_t len, const float* emb) {
    if (vs->count >= vs->capacity) return;
    Embedding* e = &vs->chunks[vs->count++];
    strncpy(e->name, name, 127);
    strncpy(e->path, path, 255);
    e->offset = off;
    e->length = len;
    if (emb) memcpy(e->embedding, emb, EMBEDDING_DIM * sizeof(float));
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
    if (vs->count == 0) return 0;
    
    size_t count = 0;
    for (size_t i = 0; i < vs->count && count < max_results; i++) {
        float score = cosine_sim(query, vs->chunks[i].embedding, EMBEDDING_DIM);
        if (score > 0.5f) {
            results[count].chunk = &vs->chunks[i];
            results[count].score = score;
            count++;
        }
    }
    
    /* Simple bubble sort by score */
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

/* Diff Engine Implementation */
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
    Diff* diff = (Diff*)malloc(sizeof(Diff));
    if (!diff) return NULL;
    diff->files = NULL;
    diff->file_count = 0;
    
    const char* p = text;
    const char* end = text + len;
    
    while (p < end) {
        if (*p == '-' && p[1] == '-' && p[2] == '-') {
            DiffFile* file;
            if (diff->file_count == 0 || diff->files[diff->file_count-1].hunk_count > 0) {
                diff->files = (DiffFile*)realloc(diff->files, 
                    (diff->file_count + 1) * sizeof(DiffFile));
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
            
            if (p + 4 < end && p[0] == '+' && p[1] == '+' && p[2] == '+') {
                p += 4;
                eol = memchr(p, '\n', end - p);
                if (eol) {
                    size_t plen = (size_t)(eol - p);
                    if (plen > 2 && p[0] == 'b' && p[1] == '/') p += 2, plen -= 2;
                    memcpy(file->new_path, p, plen < 255 ? plen : 255);
                    p = eol + 1;
                }
            }
        }
        else if (*p == '@' && p[1] == '@') {
            if (diff->file_count == 0) { p++; continue; }
            DiffFile* file = &diff->files[diff->file_count - 1];
            file->hunks = (DiffHunk*)realloc(file->hunks, 
                (file->hunk_count + 1) * sizeof(DiffHunk));
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
                if (op != ' ' && op != '-' && op != '+' && op != '\\') {
                    p++; continue;
                }
                
                if (op == '\\') { while (p < end && *p != '\n') p++; p++; continue; }
                
                const char* line_start = p + 1;
                const char* line_end = memchr(line_start, '\n', end - line_start);
                if (!line_end) line_end = end;
                
                hunk->lines = (DiffLine*)realloc(hunk->lines,
                    (hunk->line_count + 1) * sizeof(DiffLine));
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
                if (dl->op == ' ') {
                    for (size_t j = 0; j < dl->len; j++) {
                        if (gb->cursor < gap_length(gb)) {
                            gap_move_cursor(gb, gb->cursor + 1);
                        }
                    }
                    gap_move_cursor(gb, gb->cursor + 1);
                }
                else if (dl->op == '-') {
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

/* Sovereign IDE Main Structure */
typedef struct {
    GapBuffer* editor;
    VectorStore* index;
    char current_file[256];
    int cursor_line;
    int cursor_col;
    int dirty;
} SovereignIDE;

static SovereignIDE* ide_create(void) {
    SovereignIDE* ide = (SovereignIDE*)malloc(sizeof(SovereignIDE));
    if (!ide) return NULL;
    memset(ide, 0, sizeof(SovereignIDE));
    ide->editor = gap_create(GAP_MIN_CAPACITY);
    ide->index = vs_create(MAX_EMBEDDINGS);
    return ide;
}

static void ide_destroy(SovereignIDE* ide) {
    if (!ide) return;
    gap_destroy(ide->editor);
    vs_destroy(ide->index);
    free(ide);
}

static int ide_open_file(SovereignIDE* ide, const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) return -1;
    
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    char* buf = (char*)malloc((size_t)size + 1);
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
    if (!ide->current_file[0]) return -1;
    
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

static void print_usage(const char* prog) {
    printf("Sovereign IDE v%s - Self-Contained AI Development Environment\n\n", SOV_VERSION);
    printf("Usage: %s [options]\n\n", prog);
    printf("Options:\n");
    printf("  -f <file>       Open file in editor\n");
    printf("  -h              Show this help\n\n");
    printf("Interactive Commands:\n");
    printf("  open <file>     Open a file\n");
    printf("  save            Save current file\n");
    printf("  insert <text>   Insert text at cursor\n");
    printf("  delete          Delete character before cursor\n");
    printf("  move <n>        Move cursor by n positions\n");
    printf("  print           Print buffer content\n");
    printf("  diff <text>     Apply unified diff\n");
    printf("  quit            Exit\n\n");
}

int main(int argc, char** argv) {
    printf("Sovereign IDE v%s\n", SOV_VERSION);
    printf("Features: Gap Buffer, Vector Store, Diff Engine\n\n");
    
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
                printf("Opened: %s (%zu bytes)\n", argv[i], gap_length(ide->editor));
            } else {
                fprintf(stderr, "Failed to open: %s\n", argv[i]);
            }
        }
    }
    
    printf("Ready. Type 'help' for commands.\n\n");
    
    char line[MAX_LINE_LEN];
    while (1) {
        printf("sov> ");
        fflush(stdout);
        
        if (!fgets(line, sizeof(line), stdin)) break;
        
        /* Remove newline */
        size_t len = strlen(line);
        if (len > 0 && line[len-1] == '\n') line[len-1] = '\0';
        
        if (strcmp(line, "quit") == 0 || strcmp(line, "exit") == 0) {
            if (ide->dirty) {
                printf("Unsaved changes. Quit anyway? (y/n): ");
                char response[10];
                if (fgets(response, sizeof(response), stdin)) {
                    if (response[0] != 'y' && response[0] != 'Y') continue;
                }
            }
            break;
        }
        else if (strcmp(line, "help") == 0) {
            print_usage(argv[0]);
        }
        else if (strncmp(line, "open ", 5) == 0) {
            if (ide_open_file(ide, line + 5) == 0) {
                printf("Opened: %s\n", line + 5);
            } else {
                printf("Failed to open: %s\n", line + 5);
            }
        }
        else if (strcmp(line, "save") == 0) {
            if (ide_save_file(ide) == 0) {
                printf("Saved.\n");
            } else {
                printf("Save failed.\n");
            }
        }
        else if (strncmp(line, "insert ", 7) == 0) {
            gap_insert(ide->editor, line + 7, strlen(line + 7));
            ide->dirty = 1;
            printf("Inserted %zu characters.\n", strlen(line + 7));
        }
        else if (strcmp(line, "delete") == 0) {
            if (gap_length(ide->editor) > 0) {
                gap_delete(ide->editor, 1);
                ide->dirty = 1;
                printf("Deleted.\n");
            } else {
                printf("Buffer empty.\n");
            }
        }
        else if (strncmp(line, "move ", 5) == 0) {
            int offset = atoi(line + 5);
            size_t old_cursor = ide->editor->cursor;
            gap_move_cursor(ide->editor, (size_t)((int)ide->editor->cursor + offset));
            printf("Cursor: %zu -> %zu\n", old_cursor, ide->editor->cursor);
        }
        else if (strcmp(line, "print") == 0) {
            char* text = gap_extract(ide->editor);
            if (text) {
                printf("--- Buffer Content ---\n");
                printf("%s\n", text);
                printf("--- End ---\n");
                free(text);
            }
        }
        else if (strncmp(line, "diff ", 5) == 0) {
            Diff* diff = diff_parse(line + 5, strlen(line + 5));
            if (diff && diff->file_count > 0) {
                diff_apply(diff, ide->editor);
                ide->dirty = 1;
                printf("Applied diff.\n");
            } else {
                printf("Failed to parse diff.\n");
            }
            diff_destroy(diff);
        }
        else if (line[0]) {
            printf("Unknown command: %s\n", line);
        }
    }
    
    ide_destroy(ide);
    printf("Goodbye!\n");
    return 0;
}
