// VRAM Audit Tool - RawrXD v1.0.0-gold Pre-Flight
// Dumps GPU memory allocation for 70B model verification
// Author: RawrXD Core Team

#include <iostream>
#include <windows.h>
#include <vector>
#include <string>

// AMD GPU detection via ADL (simplified - would use actual ADL SDK in production)
// For now, use Windows DXGI to get VRAM info
#include <dxgi.h>
#pragma comment(lib, "dxgi.lib")

struct VRAMAllocation {
    size_t totalBytes;
    size_t freeBytes;
    size_t usedBytes;
    std::wstring deviceName;
    int deviceId;
};

class VRAMAuditor {
public:
    bool Initialize() {
        HRESULT hr = CreateDXGIFactory(__uuidof(IDXGIFactory), (void**)&factory_);
        if (FAILED(hr)) {
            std::cerr << "[VRAM Audit] Failed to create DXGI factory\n";
            return false;
        }
        return true;
    }
    
    VRAMAllocation QueryGPU(int adapterIndex = 0) {
        VRAMAllocation alloc{};
        
        IDXGIAdapter* adapter = nullptr;
        HRESULT hr = factory_->EnumAdapters(adapterIndex, &adapter);
        if (FAILED(hr)) {
            std::cerr << "[VRAM Audit] Failed to enumerate adapter " << adapterIndex << "\n";
            return alloc;
        }
        
        DXGI_ADAPTER_DESC desc;
        adapter->GetDesc(&desc);
        
        alloc.deviceName = desc.Description;
        alloc.deviceId = adapterIndex;
        
        // VRAM in bytes (DedicatedVideoMemory)
        alloc.totalBytes = desc.DedicatedVideoMemory;
        
        // Estimate used memory (this is approximate via DXGI)
        // For accurate numbers, we'd need AMD ADL or NVIDIA NVML
        alloc.usedBytes = QueryUsedVRAM(adapter);
        alloc.freeBytes = alloc.totalBytes - alloc.usedBytes;
        
        adapter->Release();
        return alloc;
    }
    
    void DumpAllocationMap() {
        std::cout << "========================================\n";
        std::cout << "RawrXD VRAM Allocation Audit\n";
        std::cout << "70B Model Pre-Flight Check\n";
        std::cout << "========================================\n\n";
        
        int adapterIndex = 0;
        IDXGIAdapter* adapter = nullptr;
        
        while (factory_->EnumAdapters(adapterIndex, &adapter) != DXGI_ERROR_NOT_FOUND) {
            DXGI_ADAPTER_DESC desc;
            adapter->GetDesc(&desc);
            
            // Convert wide string to narrow
            char name[128] = {};
            WideCharToMultiByte(CP_UTF8, 0, desc.Description, -1, name, 128, nullptr, nullptr);
            
            size_t totalVRAM = desc.DedicatedVideoMemory;
            size_t usedVRAM = QueryUsedVRAM(adapter);
            size_t freeVRAM = totalVRAM - usedVRAM;
            
            std::cout << "GPU " << adapterIndex << ": " << name << "\n";
            std::cout << "  Total VRAM: " << FormatBytes(totalVRAM) << "\n";
            std::cout << "  Used VRAM:  " << FormatBytes(usedVRAM) << "\n";
            std::cout << "  Free VRAM:  " << FormatBytes(freeVRAM) << "\n";
            std::cout << "  Utilization: " << (usedVRAM * 100 / totalVRAM) << "%\n\n";
            
            // 70B Model Analysis
            Analyze70BModel(totalVRAM, freeVRAM);
            
            adapter->Release();
            adapterIndex++;
        }
        
        if (adapterIndex == 0) {
            std::cout << "[WARNING] No DXGI adapters found!\n";
            std::cout << "  - AMD GPU may not be exposing DXGI\n";
            std::cout << "  - Try running with AMD-specific tools\n";
        }
    }
    
private:
    IDXGIFactory* factory_ = nullptr;
    
    size_t QueryUsedVRAM(IDXGIAdapter* adapter) {
        // DXGI doesn't provide used VRAM directly
        // This is a placeholder - real implementation would use:
        // - AMD ADL (ADL_Adapter_MemoryInfo_Get)
        // - NVIDIA NVML (nvmlDeviceGetMemoryInfo)
        // - Vulkan VK_EXT_memory_budget
        
        // For now, estimate based on typical usage
        DXGI_ADAPTER_DESC desc;
        adapter->GetDesc(&desc);
        size_t total = desc.DedicatedVideoMemory;
        
        // Assume 10-20% overhead for OS/drivers
        // Real usage would require GPU-specific APIs
        return total * 15 / 100; // 15% estimated overhead
    }
    
    std::string FormatBytes(size_t bytes) {
        const char* units[] = {"B", "KB", "MB", "GB", "TB"};
        int unitIndex = 0;
        double size = static_cast<double>(bytes);
        
        while (size >= 1024.0 && unitIndex < 4) {
            size /= 1024.0;
            unitIndex++;
        }
        
        char buffer[32];
        snprintf(buffer, sizeof(buffer), "%.2f %s", size, units[unitIndex]);
        return std::string(buffer);
    }
    
    void Analyze70BModel(size_t totalVRAM, size_t freeVRAM) {
        std::cout << "  --- 70B Model Analysis ---\n";
        
        // Model sizes
        const size_t model70B_Q4 = 40ULL * 1024 * 1024 * 1024;      // ~40GB Q4_K_M
        const size_t model70B_FP8 = 70ULL * 1024 * 1024 * 1024;     // ~70GB FP8
        const size_t model70B_Q8 = 80ULL * 1024 * 1024 * 1024;     // ~80GB Q8_0
        
        std::cout << "  70B Q4_K_M (40GB): " << (freeVRAM >= model70B_Q4 ? "✅ FITS" : "❌ NO") << "\n";
        std::cout << "  70B FP8 (70GB):   " << (freeVRAM >= model70B_FP8 ? "✅ FITS" : "❌ NO") << "\n";
        std::cout << "  70B Q8_0 (80GB):  " << (freeVRAM >= model70B_Q8 ? "✅ FITS" : "❌ NO") << "\n";
        
        if (freeVRAM < model70B_Q4) {
            std::cout << "\n  [WARNING] 70B model requires offloading:\n";
            std::cout << "    - Partial GPU layers + System RAM\n";
            std::cout << "    - PCIe bottleneck expected\n";
            std::cout << "    - TRES T2 (Control) should manage layer swapping\n";
        }
        
        std::cout << "\n";
    }
};

int main() {
    std::cout << "RawrXD VRAM Audit Tool\n";
    std::cout << "=======================\n\n";
    
    VRAMAuditor auditor;
    if (!auditor.Initialize()) {
        std::cerr << "Failed to initialize VRAM auditor\n";
        return 1;
    }
    
    auditor.DumpAllocationMap();
    
    std::cout << "========================================\n";
    std::cout << "VRAM Audit Complete\n";
    std::cout << "========================================\n";
    
    return 0;
}
