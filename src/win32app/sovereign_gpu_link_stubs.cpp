// Win32IDE sovereign GPU link stubs.
// Purpose: keep RawrXD-Win32IDE linkable when RAWR_HAS_SOVEREIGN_ENGINES=0.
// These symbols are referenced by Win32IDE_TabManager.cpp but implemented in optional GPU ASM.

#include <cstdint>

#if !defined(RAWR_HAS_SOVEREIGN_ENGINES) || (RAWR_HAS_SOVEREIGN_ENGINES == 0)

extern "C"
{

    uint64_t KFD_Get_Driver_Version()
    {
        return 0;
    }
    void KFD_Ring_Hardware_Doorbell() {}

    void RDNA3_Shadow_Pager_Init() {}
    void RDNA3_Power_Pulse() {}
    void RDNA3_Speculative_Preload() {}
    void Neural_Entropy_Generate() {}

    uint64_t RDNA3_MMIO_Read(uint64_t)
    {
        return 0;
    }
    uint64_t RDNA3_Telemetry_Read()
    {
        return 0;
    }

    uint64_t RDNA3_HugePage_Allocate()
    {
        return 0;
    }
    uint64_t RDNA3_3X_Virtualize(uint64_t)
    {
        return 0;
    }
    uint64_t RDNA3_Elastic_Scale(uint64_t)
    {
        return 0;
    }

    void RDNA3_Sovereign_Deflate(uint8_t*, uint8_t*) {}
    void RDNA3_3x_Expand(uint8_t*, uint8_t*) {}
    void RDNA3_Custom_Inflate(uint8_t*, uint8_t*) {}

    uint64_t Silicon_PUF_Generate()
    {
        return 0;
    }
    bool RDNA3_Silicon_Authenticate(uint64_t)
    {
        return false;
    }

}  // extern "C"

#endif
