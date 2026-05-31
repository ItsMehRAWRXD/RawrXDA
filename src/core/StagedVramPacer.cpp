#include "StagedVramPacer.h"
#include "amd_gpu_accelerator.h"
#include <iostream>

namespace RawrXD {
namespace Core {

void StagedVramPacer::PumpWin32Messages() {
    MSG msg;
    while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

bool StagedVramPacer::UploadToVram(void* devicePtr, const void* hostSrc, uint64_t totalBytes, 
                                 uint64_t chunkSize, ProgressCallback callback) {
    auto& accel = AMDGPUAccelerator::instance();
    uint64_t uploadedBytes = 0;
    const uint8_t* src = static_cast<const uint8_t*>(hostSrc);
    uint8_t* dst = static_cast<uint8_t*>(devicePtr);

    while (uploadedBytes < totalBytes) {
        uint64_t remaining = totalBytes - uploadedBytes;
        uint64_t currentChunkSize = (remaining < chunkSize) ? remaining : chunkSize;

        // Perform chunked copy
        // In real Vulkan, this would involves vkCmdCopyBuffer with a staging buffer
        // but for RawrXD's direct VRAM MASM logic, it's a kernel dispatch.
        GPUBuffer buffer;
        buffer.devicePtr = dst + uploadedBytes;
        buffer.sizeBytes = currentChunkSize;

        AccelResult res = accel.copyToGPU(buffer, src + uploadedBytes, currentChunkSize);
        if (!res.success) {
            std::cerr << "[StagedPacer] VRAM Upload failed at " << uploadedBytes << " bytes: " << res.detail << std::endl;
            return false;
        }

        uploadedBytes += currentChunkSize;

        // Update progress callback
        if (callback) {
            callback(static_cast<float>(uploadedBytes) / totalBytes);
        }

        // P0: UI Breathing - Keep Win32 loop alive during massive upload
        PumpWin32Messages();
    }

    return true;
}

} // namespace Core
} // namespace RawrXD
