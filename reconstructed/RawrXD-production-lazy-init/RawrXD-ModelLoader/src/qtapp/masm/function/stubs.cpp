#include <windows.h>
#include <string.h>
#include <malloc.h>

extern "C" {
    // Redundant stubs removed (strstr_masm, file_search_recursive, extract_sentence)
    // to avoid LNK2005 as they are already provided by MASM object libraries.

    void agentic_hotpatch_init() {}
    void agentic_hotpatch_apply(int id) {}
    void agentic_streaming_init() {}
    void agentic_streaming_push(const char* token) {}
    
    // Win32 Wrappers for MASM compatibility
    void* masm_malloc(size_t size) { return malloc(size); }
    void masm_free(void* ptr) { free(ptr); }
    
    // Helper to avoid LNK2019 for symbols used in model_interface.cpp
    // but not yet present in the MASM objects (or missing signatures)
    int agentic_orchestrator_status() { return 1; }
    void agentic_orchestrator_execute(const char* prompt) {}
}
