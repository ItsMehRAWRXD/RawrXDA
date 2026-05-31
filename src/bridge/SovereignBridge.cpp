/*
 * SovereignBridge.cpp - C/C++ Bridge for C# Integration
 * =====================================================
 * Exports native functions for interop with RawrXD Extensions
 */

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Include the unified finisher
extern "C" {
    #include "../../sovereign_finisher.c"
}

// Bridge handle structure
struct BridgeEditor {
    SovereignIDE* ide;
    char last_result[4096];
    int last_result_len;
};

extern "C" {

// Editor Management
__declspec(dllexport) void* Bridge_CreateEditor() {
    BridgeEditor* bridge = (BridgeEditor*)calloc(1, sizeof(BridgeEditor));
    if (!bridge) return NULL;
    
    bridge->ide = ide_create();
    if (!bridge->ide) {
        free(bridge);
        return NULL;
    }
    
    return bridge;
}

__declspec(dllexport) void Bridge_DestroyEditor(void* handle) {
    if (!handle) return;
    BridgeEditor* bridge = (BridgeEditor*)handle;
    if (bridge->ide) {
        ide_destroy(bridge->ide);
    }
    free(bridge);
}

// Text Operations
__declspec(dllexport) int Bridge_GetEditorText(void* handle, char* buffer, int size) {
    if (!handle || !buffer || size <= 0) return 0;
    
    BridgeEditor* bridge = (BridgeEditor*)handle;
    char* text = gap_extract(bridge->ide->editor);
    if (!text) return 0;
    
    int len = (int)strlen(text);
    if (len > size - 1) len = size - 1;
    
    memcpy(buffer, text, len);
    buffer[len] = '\0';
    
    free(text);
    return len;
}

__declspec(dllexport) void Bridge_SetEditorText(void* handle, const char* text) {
    if (!handle || !text) return;
    
    BridgeEditor* bridge = (BridgeEditor*)handle;
    
    // Clear and insert new text
    gap_destroy(bridge->ide->editor);
    bridge->ide->editor = gap_create(GAP_MIN_CAPACITY);
    gap_insert(bridge->ide->editor, text, strlen(text));
    bridge->ide->dirty = 1;
}

__declspec(dllexport) int Bridge_GetEditorLength(void* handle) {
    if (!handle) return 0;
    BridgeEditor* bridge = (BridgeEditor*)handle;
    return (int)gap_length(bridge->ide->editor);
}

// Cursor Operations
__declspec(dllexport) int Bridge_GetCursorPosition(void* handle) {
    if (!handle) return 0;
    BridgeEditor* bridge = (BridgeEditor*)handle;
    return (int)bridge->ide->editor->cursor;
}

__declspec(dllexport) void Bridge_SetCursorPosition(void* handle, int pos) {
    if (!handle) return;
    BridgeEditor* bridge = (BridgeEditor*)handle;
    gap_move_cursor(bridge->ide->editor, (size_t)pos);
}

// Thinking Effort Integration
__declspec(dllexport) void Bridge_ExecuteThinkingCommand(void* handle, const char* cmd, int level) {
    if (!handle || !cmd) return;
    
    BridgeEditor* bridge = (BridgeEditor*)handle;
    
    // Set thinking level
    if (level >= 0 && level < THINKING_LEVELS) {
        thinking_set_level(bridge->ide->thinking, level);
    }
    
    // Execute with thinking
    ide_smart_command(bridge->ide, cmd);
    
    // Store result for retrieval
    snprintf(bridge->last_result, sizeof(bridge->last_result),
             "Executed: %s\nLevel: %d\nIterations: %zu",
             cmd, level, bridge->ide->thinking->iteration);
    bridge->last_result_len = (int)strlen(bridge->last_result);
}

__declspec(dllexport) int Bridge_GetThinkingResult(void* handle, char* buffer, int size) {
    if (!handle || !buffer || size <= 0) return 0;
    
    BridgeEditor* bridge = (BridgeEditor*)handle;
    
    int len = bridge->last_result_len;
    if (len > size - 1) len = size - 1;
    
    memcpy(buffer, bridge->last_result, len);
    buffer[len] = '\0';
    
    return len;
}

__declspec(dllexport) int Bridge_GetRecommendedThinkingLevel(void* handle, const char* task, double importance) {
    if (!task) return THINK_MEDIUM;
    return thinking_recommend_level(task, importance);
}

// Extension Host Integration
__declspec(dllexport) int Bridge_LoadExtension(void* handle, const char* path) {
    if (!handle || !path) return -1;
    
    BridgeEditor* bridge = (BridgeEditor*)handle;
    Extension* ext = ext_host_load(bridge->ide->ext_host, path, NULL);
    
    return ext ? 0 : -1;
}

__declspec(dllexport) int Bridge_ExecuteExtension(void* handle, const char* ext_name, const char* func) {
    if (!handle || !ext_name || !func) return -1;
    
    BridgeEditor* bridge = (BridgeEditor*)handle;
    return ext_host_execute(bridge->ide->ext_host, ext_name, func, NULL);
}

__declspec(dllexport) int Bridge_GetExtensionCount(void* handle) {
    if (!handle) return 0;
    BridgeEditor* bridge = (BridgeEditor*)handle;
    return (int)bridge->ide->ext_host->count;
}

__declspec(dllexport) int Bridge_GetExtensionName(void* handle, int index, char* buffer, int size) {
    if (!handle || !buffer || size <= 0) return 0;
    
    BridgeEditor* bridge = (BridgeEditor*)handle;
    if (index < 0 || index >= (int)bridge->ide->ext_host->count) return 0;
    
    const char* name = bridge->ide->ext_host->extensions[index].name;
    int len = (int)strlen(name);
    if (len > size - 1) len = size - 1;
    
    memcpy(buffer, name, len);
    buffer[len] = '\0';
    
    return len;
}

// RAG / Vector Store Integration
__declspec(dllexport) void Bridge_IndexFile(void* handle, const char* path) {
    if (!handle || !path) return;
    
    BridgeEditor* bridge = (BridgeEditor*)handle;
    
    // Generate simulated embedding
    float emb[EMBEDDING_DIM];
    for (size_t i = 0; i < EMBEDDING_DIM; i++) {
        emb[i] = ((float)rand() / RAND_MAX) * 2.0f - 1.0f;
    }
    
    vs_add(bridge->ide->index, path, path, 0, 0, emb);
}

__declspec(dllexport) int Bridge_QueryVectorStore(void* handle, const char* query, 
                                                   char* results, int size, int max_results) {
    if (!handle || !query || !results || size <= 0) return 0;
    
    BridgeEditor* bridge = (BridgeEditor*)handle;
    
    // Generate query embedding
    float query_emb[EMBEDDING_DIM];
    for (size_t i = 0; i < EMBEDDING_DIM; i++) {
        query_emb[i] = ((float)rand() / RAND_MAX) * 2.0f - 1.0f;
    }
    
    SearchResult search_results[16];
    size_t count = vs_search(bridge->ide->index, query_emb, max_results, search_results);
    
    // Format results
    int pos = 0;
    for (size_t i = 0; i < count && pos < size - 256; i++) {
        int written = snprintf(results + pos, size - pos, 
                              "%s|%.3f\n", 
                              search_results[i].chunk->name,
                              search_results[i].score);
        if (written > 0) pos += written;
    }
    
    return pos;
}

// Diff Engine Integration
__declspec(dllexport) int Bridge_ApplyDiff(void* handle, const char* diff_text) {
    if (!handle || !diff_text) return -1;
    
    BridgeEditor* bridge = (BridgeEditor*)handle;
    
    Diff* diff = diff_parse(diff_text, strlen(diff_text));
    if (!diff || diff->file_count == 0) {
        diff_destroy(diff);
        return -1;
    }
    
    int result = diff_apply(diff, bridge->ide->editor);
    if (result == 0) {
        bridge->ide->dirty = 1;
    }
    
    diff_destroy(diff);
    return result;
}

// File Operations
__declspec(dllexport) int Bridge_OpenFile(void* handle, const char* path) {
    if (!handle || !path) return -1;
    BridgeEditor* bridge = (BridgeEditor*)handle;
    return ide_open_file(bridge->ide, path);
}

__declspec(dllexport) int Bridge_SaveFile(void* handle) {
    if (!handle) return -1;
    BridgeEditor* bridge = (BridgeEditor*)handle;
    return ide_save_file(bridge->ide);
}

__declspec(dllexport) int Bridge_IsDirty(void* handle) {
    if (!handle) return 0;
    BridgeEditor* bridge = (BridgeEditor*)handle;
    return bridge->ide->dirty;
}

// Version Info
__declspec(dllexport) const char* Bridge_GetVersion() {
    return SOVEREIGN_VERSION;
}

} // extern "C"

// DLL Entry Point
BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID lpReserved) {
    switch (reason) {
        case DLL_PROCESS_ATTACH:
            DisableThreadLibraryCalls(hModule);
            srand((unsigned)time(NULL));
            break;
        case DLL_PROCESS_DETACH:
            break;
    }
    return TRUE;
}
