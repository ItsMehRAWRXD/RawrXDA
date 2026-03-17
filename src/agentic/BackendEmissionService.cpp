#include "BackendEmissionService.h"
#include <windows.h>
#include <fstream>
#include <thread>
#include <future>

// Forward to BareMetal_PE_Writer.asm via C wrapper
extern "C" {
    typedef struct {
        const uint8_t* code;
        size_t code_size;
        const char* entry_symbol;
        uint64_t base_addr;
        int relocatable;
        char* out_path;      // Buffer to receive path
        size_t out_path_len;
        char* error;         // Buffer to receive error
        size_t error_len;
        uint32_t* entry_rva;
    } BmpeEmitRequest;
    
    // This is expected to be exported by the DLL or linked from the ASM object
    int BmpeEmitExecutable(const BmpeEmitRequest* req);
}

using namespace RawrXD::Agentic;

bool BackendEmissionService::is_emitter_available() {
    // Check if the symbol is available in the current process or RawrXD_Titan.dll
    HMODULE hMod = GetModuleHandleA(NULL); // Check main exe first
    if (GetProcAddress(hMod, "BmpeEmitExecutable")) return true;
    
    hMod = GetModuleHandleA("RawrXD_Titan.dll");
    if (!hMod) hMod = LoadLibraryA("RawrXD_Titan.dll");
    if (!hMod) return false;
    return GetProcAddress(hMod, "BmpeEmitExecutable") != nullptr;
}

BackendEmissionService::EmissionResult BackendEmissionService::emit_executable(const EmissionRequest& req) {
    EmissionResult result{};
    
    if (!is_emitter_available()) {
        result.error_message = "BareMetal_PE_Writer not available - RawrXD_Titan.dll missing or outdated";
        return result;
    }
    
    // Prepare buffers
    char path_buf[MAX_PATH] = {0};
    char error_buf[512] = {0};
    uint32_t entry_rva = 0;
    
    BmpeEmitRequest c_req{};
    c_req.code = req.machine_code.data();
    c_req.code_size = req.machine_code.size();
    c_req.entry_symbol = req.entry_point_symbol.c_str();
    c_req.base_addr = req.preferred_base;
    c_req.relocatable = req.require_relocatable ? 1 : 0;
    c_req.out_path = path_buf;
    c_req.out_path_len = sizeof(path_buf);
    c_req.error = error_buf;
    c_req.error_len = sizeof(error_buf);
    c_req.entry_rva = &entry_rva;
    
    int status = BmpeEmitExecutable(&c_req);
    
    if (status == 0) {
        result.success = true;
        result.output_path = path_buf;
        result.entry_point_rva = entry_rva;
    } else {
        result.error_message = error_buf;
    }
    
    return result;
}

void BackendEmissionService::emit_executable_async(const EmissionRequest& req, CompletionCallback cb) {
    std::thread([req, cb]() {
        auto result = emit_executable(req);
        if (cb) cb(result);
    }).detach();
}
