#include "hot_patcher.h"
#include <vector>
#include <string>

#if defined(_MSC_VER)
#define NOINLINE __declspec(noinline)
#else
#define NOINLINE __attribute__((noinline))
#endif

namespace {

NOINLINE int HotpatchProbe(int x) {
    return x + 1;
}

}

extern "C" __declspec(dllexport)
int RawrXD_RunHotpatchValidation() {
    HotPatcher patcher;

    const int before = HotpatchProbe(5);
    if (before != 6) {
        return -10;
    }

    // x64: mov eax, 1337 ; ret
    const std::vector<unsigned char> patch = {
        0xB8, 0x39, 0x05, 0x00, 0x00, 0xC3
    };

    if (!patcher.ApplyPatch("hotpatch_probe", (void*)(&HotpatchProbe), patch)) {
        return -1;
    }

    const int patched = HotpatchProbe(5);
    if (patched != 1337) {
        patcher.RevertPatch("hotpatch_probe");
        return -2;
    }

    if (!patcher.RevertPatch("hotpatch_probe")) {
        return -3;
    }

    const int restored = HotpatchProbe(5);
    if (restored != 6) {
        return -4;
    }

    return 0;
}

extern "C" __declspec(dllexport)
int RawrXD_ApplyHotpatchBytes(const char* patchName,
                              void* targetAddress,
                              const unsigned char* bytes,
                              unsigned int length) {
    if (!patchName || !targetAddress || !bytes || length == 0) {
        return -1;
    }

    std::vector<unsigned char> patch(bytes, bytes + length);
    HotPatcher patcher;
    return patcher.ApplyPatch(patchName, targetAddress, patch) ? 0 : -2;
}
