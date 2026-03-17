// AgentKernelBridge.cpp
// Integration layer bridging C++ kernel, MASM producers, and hotpatch engine

#include "CommandRegistry.hpp"
#include "RawrXD_PredictiveCommandKernel.hpp"
#include "IDELogger.hpp"
#include <thread>
#include <atomic>
#include <queue>
#include <mutex>

namespace RawrXD::Agentic::Bridge {

// External MASM functions (declared with C linkage)
extern "C" {
    int MmfProducer_Initialize(HANDLE mmfHandle, void* baseAddr, void* controlBlock);
    int MmfProducer_SubmitTool(void* producerCtx, uint32_t toolId, 
                              uint32_t flags, void* payload);
    int MmfProducer_FlushBatch(void* producerCtx);
    int MmfProducer_DetectBackpressure(void* producerCtx);
    
    int HotpatchEngine_Initialize(void* reserved);
    int HotpatchEngine_InstallDetour(void* targetAddr, void* replacementAddr, 
                                     uint32_t patchToken);
    int HotpatchEngine_RollbackDetour(void* targetAddr);
    int HotpatchEngine_Shutdown();
}

class AgentKernelBridge {
public:
    static AgentKernelBridge& instance() {
        static AgentKernelBridge _instance;
        return _instance;
    }
    
    bool initialize(HWND hwnd, CommandRegistry* registry, 
                   const std::filesystem::path& mmfPath) {
        if (initialized_) return true;
        
        hwnd_ = hwnd;
        registry_ = registry;
        
        // Initialize hotpatch engine
        if (!HotpatchEngine_Initialize(nullptr)) {
            IDELogger::error("HotpatchEngine initialization failed");
            return false;
        }
        
        // Open or create MMF
        HANDLE hFile = CreateFileA(
            mmfPath.string().c_str(),
            GENERIC_READ | GENERIC_WRITE,
            0,
            nullptr,
            OPEN_ALWAYS,
            FILE_ATTRIBUTE_NORMAL,
            nullptr
        );
        
        if (hFile == INVALID_HANDLE_VALUE) {
            IDELogger::error("Failed to open MMF path: " + mmfPath.string());
            return false;
        }
        
        // Map MMF
        HANDLE hMapping = CreateFileMapping(
            hFile,
            nullptr,
            PAGE_READWRITE,
            0,
            0x100000,  // 1MB
            L"RawrXD_CommandMMF"
        );
        
        if (!hMapping) {
            CloseHandle(hFile);
            IDELogger::error("MMF creation failed");
            return false;
        }
        
        void* baseAddr = MapViewOfFile(hMapping, FILE_MAP_ALL_ACCESS, 0, 0, 0x100000);
        if (!baseAddr) {
            CloseHandle(hMapping);
            CloseHandle(hFile);
            IDELogger::error("MapViewOfFile failed");
            return false;
        }
        
        mmfFile_ = hFile;
        mmfMapping_ = hMapping;
        mmfBase_ = baseAddr;
        
        // Initialize MMF producer in MASM
        void* controlBlock = baseAddr;  // First 64 bytes reserved for control
        if (!MmfProducer_Initialize(hMapping, baseAddr, controlBlock)) {
            IDELogger::error("MmfProducer_Initialize failed");
            return false;
        }
        
        // Start kernel thread
        kernelThread_ = std::thread([this]() { kernelLoop(); });
        SetThreadDescription(
            reinterpret_cast<HANDLE>(kernelThread_.native_handle()),
            L"RawrXD-AgentKernelBridge"
        );
        
        initialized_ = true;
        IDELogger::info("AgentKernelBridge initialized successfully");
        return true;
    }
    
    void shutdown() {
        if (!initialized_) return;
        
        running_ = false;
        if (kernelThread_.joinable()) {
            kernelThread_.join();
        }
        
        HotpatchEngine_Shutdown();
        
        if (mmfBase_) UnmapViewOfFile(mmfBase_);
        if (mmfMapping_) CloseHandle(mmfMapping_);
        if (mmfFile_) CloseHandle(mmfFile_);
        
        initialized_ = false;
    }
    
    // Submit command for potential pre-execution via speculative engine
    bool submitForSpeculation(uint32_t commandId, WPARAM wParam, LPARAM lParam) {
        if (!initialized_) return false;
        
        // Route through intelligent router
        auto& router = IntelligentCommandRouter::instance();
        return router.route(hwnd_, commandId, wParam, lParam);
    }
    
    // Directly dispatch tool via MMF producer (from speculative engine)
    bool dispatchToolThroughMMF(uint32_t toolId, uint32_t priority, 
                               const void* payload, size_t payloadLen) {
        if (!mmfBase_) return false;
        
        // Build payload buffer
        struct ToolPayload {
            uint32_t toolId;
            uint32_t priority;
            uint32_t length;
            uint8_t data[24];
        } tp = {toolId, priority, static_cast<uint32_t>(payloadLen)};
        
        if (payloadLen > 24) payloadLen = 24;
        memcpy(tp.data, payload, payloadLen);
        
        // Submit via MASM producer
        int result = MmfProducer_SubmitTool(
            mmfBase_, toolId, priority, &tp.data
        );
        
        return result != 0;
    }
    
    // Apply thermal-aware hotpatch
    bool applyThermalPatch(void* targetAddr, void* replacementAddr) {
        if (!initialized_) return false;
        
        static uint32_t patchToken = 0;
        int result = HotpatchEngine_InstallDetour(
            targetAddr, replacementAddr, patchToken++
        );
        
        return result != 0;
    }
    
    // Get backpressure feedback
    int getMMFBackpressure() const {
        if (!mmfBase_) return 0;
        return MmfProducer_DetectBackpressure(mmfBase_);
    }
    
    struct Metrics {
        uint64_t commandsProcessed;
        uint64_t predictionsHit;
        uint64_t speculationHits;
        uint64_t thermalThrottles;
        uint64_t mmfTokensFlushed;
    };
    
    Metrics getMetrics() const {
        return metrics_;
    }
    
private:
    AgentKernelBridge() : initialized_(false), running_(false) {}
    
    void kernelLoop() {
        while (running_) {
            // Flush MMF batch periodically
            if (mmfBase_) {
                MmfProducer_FlushBatch(mmfBase_);
            }
            
            // Get backpressure feedback
            int pressure = getMMFBackpressure();
            if (pressure > 80) {
                // Thermal or flow control adjustment
                metrics_.thermalThrottles++;
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }
    }
    
    HWND hwnd_;
    CommandRegistry* registry_;
    bool initialized_;
    std::atomic<bool> running_;
    std::thread kernelThread_;
    
    HANDLE mmfFile_;
    HANDLE mmfMapping_;
    void* mmfBase_;
    
    Metrics metrics_;
};

// Global bridge instance
AgentKernelBridge& GetAgentKernelBridge() {
    return AgentKernelBridge::instance();
}

} // namespace RawrXD::Agentic::Bridge
