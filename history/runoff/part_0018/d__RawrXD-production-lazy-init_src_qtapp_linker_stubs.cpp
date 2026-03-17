//=====================================================================
// linker_stubs.cpp - Stub implementations for unresolved externals
// Provides fallback implementations for symbols referenced but not implemented
//=====================================================================

#include <QString>
#include <QJsonObject>
#include <QList>
#include <QMap>
#include <windows.h>
#include <string>

//=====================================================================
// ModelInterface Stubs
//=====================================================================

class ModelConfig {};
struct GenerationOptions {};
struct GenerationResult {
    QString result;
    double latency = 0.0;
    bool success = false;
};

class ModelInterface {
public:
    ModelInterface(class QObject *) {}
    ~ModelInterface() {}
    bool loadConfig(const QString &) { return true; }
    QJsonObject getUsageStatistics() const { return QJsonObject(); }
    QList<QString> getAvailableModels() const { return QList<QString>(); }
    QString selectBestModel(const QString &, const QString &, bool) { return "default"; }
    QString selectCostOptimalModel(const QString &, double) { return "default"; }
    QString selectFastestModel(const QString &) { return "default"; }
    double getAverageLatency(const QString &) const { return 0.0; }
    double getTotalCost() const { return 0.0; }
    int getSuccessRate(const QString &) const { return 100; }
    GenerationResult generate(const QString &, const QString &, const GenerationOptions &) {
        return GenerationResult{"", 0.0, true};
    }
    void registerModel(const QString &, const ModelConfig &) {}
};

//=====================================================================
// rawr_xd::CompleteModelLoaderSystem Stubs
//=====================================================================

namespace rawr_xd {
class CompleteModelLoaderSystem {
public:
    struct GenerationResult {
        std::string text;
        double latency = 0.0;
        bool success = false;
    };
    
    GenerationResult generateAutonomous(const std::string &, int, const std::string &) {
        return GenerationResult{"", 0.0, true};
    }
};
}

//=====================================================================
// MASM/ASM Deflate/Inflate Stubs
//=====================================================================

extern "C" {
    void AsmDeflate() {}
    void AsmInflate() {}
    void Bridge_InflateBrutal() {}
}

//=====================================================================
// Authentication Stubs
//=====================================================================

extern "C" {
    void auth_init() {}
    void auth_shutdown() {}
    int auth_authenticate(const char *, const char *) { return 1; }
    int auth_authorize(const char *) { return 1; }
}

//=====================================================================
// Distributed Executor Stubs
//=====================================================================

extern "C" {
    void distributed_executor_init() {}
    void distributed_executor_shutdown() {}
    void distributed_submit_job(const char *) {}
    void distributed_register_node(const char *, int) {}
    void distributed_get_status() {}
    void distributed_cancel_job(const char *) {}
}

//=====================================================================
// MASM Byte Patch Stubs
//=====================================================================

extern "C" {
    void masm_byte_patch_apply(const char *, unsigned char *, size_t) {}
    void masm_byte_patch_close() {}
    int masm_byte_patch_find_pattern(const unsigned char *, size_t, const unsigned char *, size_t) { return -1; }
    void masm_byte_patch_open(const char *) {}
    void masm_byte_patch_read(unsigned char *, size_t) {}
    void masm_byte_patch_write(const unsigned char *, size_t) {}
    void masm_memory_protect(void *, size_t, int) {}
    void masm_memset_fast(void *, int, size_t) {}
    void masm_trap_install(int, void (*)(int)) {}
}

//=====================================================================
// Inference Manager Stubs
//=====================================================================

extern "C" {
    void inference_manager_init() {}
    void inference_manager_shutdown() {}
    void inference_manager_queue_request(const char *) {}
    void inference_manager_wait_result(char *, size_t) {}
}

//=====================================================================
// UI/GUI Stubs
//=====================================================================

extern "C" {
    void gui_create_complete_ide() {}
    void gui_create_component(const char *) {}
    void gui_load_pane_layout(const char *) {}
    void gui_save_pane_layout(const char *) {}
    void ui_create_layout_shell() {}
    void ui_register_components() {}
    void ui_on_hotpatch_byte() {}
    void wnd_proc_main() {}
    void load_layout_json(const char *) {}
    void save_layout_json(const char *) {}
}

//=====================================================================
// Menu/UI Handler Stubs
//=====================================================================

extern "C" {
    void MenuBar_Create() {}
    void MenuBar_EnableMenuItem() {}
    void FileBrowser_Create() {}
    void FileBrowser_Destroy() {}
    HMENU CreateMenuA() { return NULL; }
    BOOL EnableMenuItemA(HMENU, unsigned int, unsigned int) { return FALSE; }
    HANDLE CreateMutex(LPSECURITY_ATTRIBUTES, BOOL, LPCSTR) { return NULL; }
    HANDLE CreatePipeEx() { return NULL; }
    HANDLE CreateThreadEx(LPSECURITY_ATTRIBUTES, SIZE_T, LPTHREAD_START_ROUTINE, LPVOID, DWORD, LPDWORD) { return NULL; }
    HRESULT ImageList_Create(int, int, unsigned int, int, int) { return 0; }
    BOOL ImageList_Destroy(HIMAGELIST) { return FALSE; }
}

//=====================================================================
// Main UI Event Handler Stubs
//=====================================================================

extern "C" {
    void main_on_open() {}
    void main_on_open_file() {}
    void main_on_save_file() {}
    void main_on_save_file_as() {}
    void main_on_send() {}
    void keyboard_shortcuts_process() {}
}

//=====================================================================
// Agent Chat Stubs
//=====================================================================

extern "C" {
    void agent_chat_enhanced_init() {}
    void agentic_bridge_initialize() {}
}

//=====================================================================
// Default Model Variable
//=====================================================================

extern "C" {
    const char *default_model = "default";
}
