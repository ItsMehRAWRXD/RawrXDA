#include <windows.h>
#include <iostream>
#include <memory>
#include <string>
#include <cstring>

namespace RawrXD {

class GGUFHotpatch {
public:
    static bool apply70BGgufHotpatch() {
        HMODULE hModule = GetModuleHandleA("RawrXD-Win32IDE.exe");
        if (!hModule) {
            hModule = GetModuleHandleA(nullptr); // fallback: current exe
            if (!hModule) {
                std::cerr << "[GGUFHotpatch] Failed to get module handle" << std::endl;
                return false;
            }
        }

        // Get PE image info for scanning bounds
        auto* dosHeader = reinterpret_cast<IMAGE_DOS_HEADER*>(hModule);
        if (dosHeader->e_magic != IMAGE_DOS_SIGNATURE) {
            std::cerr << "[GGUFHotpatch] Invalid DOS signature" << std::endl;
            return false;
        }
        auto* ntHeaders = reinterpret_cast<IMAGE_NT_HEADERS*>(
            reinterpret_cast<BYTE*>(hModule) + dosHeader->e_lfanew);
        if (ntHeaders->Signature != IMAGE_NT_SIGNATURE) {
            std::cerr << "[GGUFHotpatch] Invalid NT signature" << std::endl;
            return false;
        }

        BYTE* imageBase = reinterpret_cast<BYTE*>(hModule);
        DWORD imageSize = ntHeaders->OptionalHeader.SizeOfImage;

        // Scan for the 4GB model size limit constant (0x100000000 = 4294967296)
        // Used in GGUF loader as max file size gate. Patch to 140GB (0x2200000000).
        // The pattern is the 64-bit immediate load: mov reg, 0x0000000100000000
        // x64 encoding: 48 B8 00 00 00 00 01 00 00 00  (REX.W + mov rax, imm64)
        // We also check for 48 B9/BA/BB variants (different registers)
        static const uint8_t sizeLimit[8] = { 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00 }; // 4GB LE
        static const uint8_t newLimit[8]  = { 0x00, 0x00, 0x00, 0x00, 0x22, 0x00, 0x00, 0x00 }; // 140GB LE

        bool patched = false;
        for (DWORD offset = 0; offset + 10 <= imageSize; ++offset) {
            BYTE* ptr = imageBase + offset;
            // Look for REX.W mov reg, imm64 (48 B8-BF followed by our size limit)
            if (ptr[0] == 0x48 && (ptr[1] >= 0xB8 && ptr[1] <= 0xBF)) {
                if (memcmp(ptr + 2, sizeLimit, 8) == 0) {
                    // Found the 4GB limit — patch it
                    DWORD oldProtect = 0;
                    if (VirtualProtect(ptr + 2, 8, PAGE_EXECUTE_READWRITE, &oldProtect)) {
                        memcpy(ptr + 2, newLimit, 8);
                        VirtualProtect(ptr + 2, 8, oldProtect, &oldProtect);
                        patched = true;
                        std::cout << "[GGUFHotpatch] Patched 4GB limit -> 140GB at offset 0x"
                                  << std::hex << offset << std::dec << std::endl;
                    }
                }
            }
        }

        if (!patched) {
            // Limit constant not found — may already be patched or not present in this build
            std::cout << "[GGUFHotpatch] No 4GB limit pattern found (may already be patched)" << std::endl;
        }

        return true;
    }
};

} // namespace RawrXD