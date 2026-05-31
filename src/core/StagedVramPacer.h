#pragma once
#include <windows.h>
#include <cstdint>
#include <functional>

namespace RawrXD {
namespace Core {

class StagedVramPacer {
public:
    typedef std::function<void(float progress)> ProgressCallback;

    // Uploads data in chunks, pumping the Win32 message loop to keep UI responsive.
    static bool UploadToVram(void* devicePtr, const void* hostSrc, uint64_t totalBytes, 
                             uint64_t chunkSize, ProgressCallback callback = nullptr);

private:
    static void PumpWin32Messages();
};

} // namespace Core
} // namespace RawrXD
