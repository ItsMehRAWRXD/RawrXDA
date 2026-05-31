// gpu_masm_bridge.h
// C-callable interface for MASM GPU backend
// Exposes MASM routines for C++ integration

#ifdef __cplusplus
extern "C" {
#endif

// Backend IDs: 0=CPU, 1=Vulkan, 2=CUDA, 3=ROCm
int InitializeGPUBackend(int preferred_backend); // returns 0 on success, -1 on failure
void* AllocateGPUMemory(unsigned long long size); // returns pointer or NULL
void FreeGPUMemory(void* ptr);
int GPU_Detect(); // returns number of GPUs found

// Device info struct (must match MASM layout)
typedef struct {
    unsigned short VendorID;
    unsigned short DeviceID;
    unsigned int ClassCode;
    unsigned char Bus;
    unsigned char Dev;
    unsigned char Func;
    unsigned long long MemorySize;
    unsigned int ComputeCapability;
    char DeviceName[256];
} GPU_DEVICE;

extern GPU_DEVICE GPU_DeviceList[16];
extern int GPU_DeviceCount;

#ifdef __cplusplus
}
#endif
