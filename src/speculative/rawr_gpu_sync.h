// rawr_gpu_sync.h
// GPU synchronization and real PCIe bandwidth measurement

#pragma once
#include <windows.h>
#include <stdint.h>

// Forward declarations for GPU sync
struct ID3D12Device;
struct ID3D12CommandQueue;
struct ID3D12Fence;

namespace rawr {

// GPU synchronization wrapper
class GPUSync {
private:
    ID3D12Device* device_ = nullptr;
    ID3D12CommandQueue* queue_ = nullptr;
    ID3D12Fence* fence_ = nullptr;
    HANDLE fence_event_ = nullptr;
    uint64_t fence_value_ = 0;
    bool initialized_ = false;
    
public:
    GPUSync();
    ~GPUSync();
    
    // Initialize D3D12 for GPU sync
    bool Initialize();
    
    // Record a GPU command that reads from aperture address
    bool RecordReadCommand(void* aperture_addr, size_t size);
    
    // Execute and wait for completion (measures real PCIe time)
    double ExecuteAndWait(); // Returns milliseconds
    
    // Check if initialized
    bool IsInitialized() const { return initialized_; }
    
    // Cleanup
    void Shutdown();
};

// Simple GPU memory copy benchmark
class GPUBandwidthTest {
public:
    // Measure actual GPU→VRAM copy bandwidth
    static double MeasureDeviceToHost(void* gpu_addr, void* host_addr, size_t size);
    static double MeasureHostToDevice(void* host_addr, void* gpu_addr, size_t size);
    
    // Measure aperture→GPU bandwidth (the real metric)
    static double MeasureApertureToGPU(void* aperture_addr, size_t size);
};

// NVMe fallback for PANIC tier
class NVMeFallback {
private:
    HANDLE swap_file_ = INVALID_HANDLE_VALUE;
    size_t swap_size_ = 0;
    void* swap_buffer_ = nullptr;
    
public:
    NVMeFallback();
    ~NVMeFallback();
    
    // Initialize swap file
    bool Initialize(const wchar_t* path, size_t size_gb);
    
    // Write tensor to NVMe
    bool WriteToSwap(void* data, size_t offset, size_t size);
    
    // Read tensor from NVMe back to aperture
    bool ReadFromSwap(void* dest, size_t offset, size_t size);
    
    // Check if initialized
    bool IsInitialized() const { return swap_file_ != INVALID_HANDLE_VALUE; }
    
    // Cleanup
    void Shutdown();
};

} // namespace rawr
