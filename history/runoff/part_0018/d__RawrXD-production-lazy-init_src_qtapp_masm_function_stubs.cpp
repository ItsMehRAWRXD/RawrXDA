#include <windows.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

extern "C" {
    // UI Callbacks
    void main_on_send() {}
    void main_on_open() {}
    void main_on_open_file() {}
    void main_on_save_file() {}
    void gui_create_component(const char* name) {}

    // Compression/Decompression stubs (match real fallback signature)
    size_t AsmDeflate(const void* in, size_t in_size, void* out, size_t out_size) { (void)in; (void)in_size; (void)out; (void)out_size; return 0; }
    size_t AsmInflate(const void* in, size_t in_size, void* out, size_t out_size) { (void)in; (void)in_size; (void)out; (void)out_size; return 0; }

    void masm_hotpatch_apply_patch(const char* p, size_t s) {}
    void masm_hotpatch_validate_integrity(void* a, size_t s) {}
    int masm_hotpatch_rollback_last() { return 0; }
    void masm_hotpatch_list_patches() {}
    void masm_hotpatch_get_patch_status(const char* p) {}
    void masm_hotpatch_find_pattern(const char* p) {}
    void masm_hotpatch_protect_memory(void* a, size_t s) {}
    void masm_hotpatch_unprotect_memory(void* a, size_t s) {}


    void* stream_create(const char* name) { return NULL; }
    void stream_processor_init() {}
    void stream_processor_shutdown() {}
    void stream_subscribe(void* s, const char* topic) {}
    void stream_publish(void* s, const char* topic, const void* data, size_t size) {}
    void stream_consume(void* s, const char* topic) {}
    void stream_ack(void* s, const char* id) {}
    void stream_nack(void* s, const char* id) {}
    void* stream_create_buffer(size_t s) { return malloc(s); }
    void stream_destroy_buffer(void* b) { free(b); }
    size_t stream_write_data(void* s, const void* d, size_t z) { return z; }
    size_t stream_read_data(void* s, void* b, size_t z) { return 0; }
    void stream_flush(void* s) {}
    long long stream_get_offset(void* s) { return 0; }
    void stream_seek(void* s, long long o, int origin) {}
    void stream_stats(void* s, void* st) {}
    void stream_list(void* s) {}

    int distributed_init_node(const char* id) { return 1; }
    int distributed_connect_peer(const char* id, const char* addr) { return 1; }
    void distributed_send_message(const char* id, const void* m, size_t s) {}
    void* distributed_receive_message(const char* id) { return NULL; }
    void distributed_shutdown() {}
    void distributed_executor_init() {}
    void distributed_executor_shutdown() {}
    void distributed_register_node(const char* n) {}
    void distributed_submit_job(const char* j) {}
    void distributed_get_status(const char* j) {}
    void distributed_cancel_job(const char* j) {}

    int auth_verify_signature(const void* d, size_t s, const void* g, size_t z) { return 1; }
    void* auth_generate_key_pair() { return malloc(256); }
    int auth_authenticate_user(const char* u, const char* p) { return 1; }
    void auth_revoke_token(const char* t) {}
    void auth_init() {}
    void auth_shutdown() {}
    void auth_authenticate(const char* u, const char* p) {}
    void auth_authorize(const char* t) {}

    void agent_chat_enhanced_init() {}

    void* ml_masm_create_model() { return malloc(1024); }
    void ml_masm_destroy_model(void* m) { free(m); }
    float ml_masm_predict(void* m, const float* f, size_t c) { return 0.5f; }
    void ml_masm_train_epoch(void* m, const float* t, size_t s) {}

    void ml_masm_init() {}
    void ml_masm_inference() {}

    void gui_draw_window(int x, int y, int w, int h) {}
    void gui_update_display() {}
    void gui_handle_input(int i) {}
    void gui_render_text(const char* t, int x, int y) {}
    void gui_create_complete_ide() {}
    void gui_save_pane_layout(const char* p) {}
    void gui_load_pane_layout(const char* p) {}

    void main_on_save_file_as() {}
    void ui_file_open_dialog() {}
    void ui_file_save() {}
    void RecalculateLayout() {}
    void keyboard_shortcuts_process() {}
    void session_trigger_autosave() {}
    void session_manager_init() {}
    const char* default_model = "default";

    void masm_initialize_runtime() {}
    void masm_shutdown_runtime() {}
    int masm_get_runtime_info(char* b, size_t s) { return 0; }
    void masm_mainwindow_init() {}

    void masm_orchestration_install() {}
    void masm_orchestration_shutdown() {}
    void masm_orchestration_poll() {}
    void masm_orchestration_set_handles(void** h, size_t c) {}
    void masm_orchestration_schedule_task(const char* n, void* d) {}

    void masm_byte_patch_open_file(const char* f) {}
    void masm_byte_patch_find_pattern(const char* p) {}
    void masm_byte_patch_apply(void* d) {}
    void masm_byte_patch_close() {}
    void masm_byte_patch_get_stats(void* s) {}

    /* masm_server_hotpatch_* stubs removed to avoid duplicate symbol; production asm provides implementations */
    void masm_proxy_hotpatch_add(const char* p) {}
    void masm_proxy_apply_logit_bias(float b) {}
    void masm_proxy_inject_rst() {}
    void masm_proxy_transform_response(const char* r) {}
    void masm_proxy_hotpatch_get_stats(void* s) {}

    // MASM Qt bridge fallback stubs - ensure linker can resolve bridge symbols
    // These are lightweight no-op implementations used when MASM integration
    // is not available or the MASM runtime is not linked into this build.
    bool masm_qt_bridge_init() { return true; }
    bool masm_signal_connect(unsigned int signalId, unsigned long long callbackAddr) { (void)signalId; (void)callbackAddr; return true; }
    bool masm_signal_disconnect(unsigned int signalId) { (void)signalId; return true; }
    bool masm_signal_emit(unsigned int signalId, unsigned int paramCount, const void* params) { (void)signalId; (void)paramCount; (void)params; return true; }
    unsigned int masm_event_pump() { return 0; }


    int masm_strcmp(const char* a, const char* b) { return strcmp(a, b); }
    size_t masm_strlen(const char* s) { return strlen(s); }
    void* masm_memcpy(void* d, const void* s, size_t c) { return memcpy(d, s, c); }
    void* masm_memset(void* d, int v, size_t c) { return memset(d, v, c); }
    void masm_debug_print(const char* m) { OutputDebugStringA(m); }
    void masm_debug_assert(int c, const char* m) {}
    unsigned long long masm_get_tick_count() { return GetTickCount64(); }
    void masm_record_performance_metric(const char* n, double v) {}
    void* masm_file_open(const char* f, const char* m) { return NULL; }
    void masm_file_close(void* h) {}
    size_t masm_file_read(void* h, void* b, size_t s, size_t c) { return 0; }
    size_t masm_file_write(void* h, const void* b, size_t s, size_t c) { return 0; }

# Removed duplicate thread/pipe wrappers to avoid LNK2005 conflicts; use core implementations
#undef CreateMutex
    HANDLE CreateMutex(LPSECURITY_ATTRIBUTES a, BOOL o, LPCSTR n) {
        return CreateMutexA(a, o, n);
    }
    HMENU CreateMenuA() {
        return CreateMenu();
    }
    BOOL EnableMenuItemA(HMENU h, UINT i, UINT m) {
        return EnableMenuItem(h, i, m);
    }

    // Wrapper functions that provide CreateThreadEx and CreatePipeEx implementations
    int __stdcall CreateThreadEx(void** hThread, unsigned long (__stdcall* threadFunc)(void*), void* arg, unsigned int flags) {
        // Direct Windows API call to avoid conflicts
        if (!hThread || !threadFunc) {
            return -1;
        }

        HANDLE h = ::CreateThread(
            nullptr,                       // Security attributes
            0,                             // Stack size (default)
            (LPTHREAD_START_ROUTINE)threadFunc,
            arg,
            (flags & 0x04) ? CREATE_SUSPENDED : 0,
            nullptr);

        if (h == nullptr) {
            *hThread = nullptr;
            return static_cast<int>(::GetLastError());
        }

        *hThread = h;
        return 0;
    }

    int __stdcall CreatePipeEx(void** hReadPipe, void** hWritePipe, void* pipeAttributes, unsigned int size) {
        // Direct Windows API call to avoid conflicts
        if (!hReadPipe || !hWritePipe) {
            return -1;
        }

        HANDLE hRead = nullptr;
        HANDLE hWrite = nullptr;

        if (!::CreatePipe(&hRead, &hWrite, (LPSECURITY_ATTRIBUTES)pipeAttributes, size)) {
            *hReadPipe = nullptr;
            *hWritePipe = nullptr;
            return static_cast<int>(::GetLastError());
        }

        *hReadPipe = hRead;
        *hWritePipe = hWrite;
        return 0;
    }
}

// Lightweight metrics stubs to satisfy linker when full metrics
// implementations are not present in this build configuration.
namespace RawrXD {
    struct LLMMetrics {
        struct Request; // forward declaration of nested Request type
        static void recordRequest(const Request&) {}
    };

    struct CircuitBreakerMetrics {
        struct Event; // forward declaration of nested Event type
        static void recordEvent(const Event&) {}
    };
}
