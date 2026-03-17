#pragma once

#include <windows.h>
#include <cstdint>
#include <string>
#include <mutex>
#include <cstdlib>
#include <cstring>

namespace rawrxd::gguf::masm {

#pragma pack(push, 1)
struct MetadataEntry {
    const char* key_ptr;
    uint64_t key_len;
    const void* value_ptr;
    uint64_t value_len;
    uint32_t type;
    uint8_t skipped;
    uint8_t reserved[3];
};
#pragma pack(pop)

using FnInitContext = void* (__cdecl*)(HANDLE, uint64_t, uint32_t);
using FnValidateHeader = bool (__cdecl*)(void*);
using FnParseMetadata = uint64_t (__cdecl*)(void*, void(*)(const MetadataEntry*, void*), void*);

class Bridge {
public:
    static Bridge& Instance() {
        static Bridge instance;
        return instance;
    }

    bool EnsureLoaded() {
        std::call_once(init_flag_, [this]() {
            LoadInternal();
        });
        return ready_;
    }

    bool Available() const { return ready_; }

    uint64_t ParseMetadata(HANDLE hFile, uint64_t maxStringLen, uint32_t flags,
                           uint64_t metadataCountOffset,
                           void(*callback)(const MetadataEntry*, void*), void* userData) {
        if (!EnsureLoaded() || !pInit_ || !pParse_) return 0;
        if (hFile == INVALID_HANDLE_VALUE) return 0;

        LARGE_INTEGER original{};
        if (!SetFilePointerEx(hFile, {}, &original, FILE_CURRENT)) return 0;

        LARGE_INTEGER target{};
        target.QuadPart = static_cast<LONGLONG>(metadataCountOffset);
        if (!SetFilePointerEx(hFile, target, nullptr, FILE_BEGIN)) {
            SetFilePointerEx(hFile, original, nullptr, FILE_BEGIN);
            return 0;
        }

        void* ctx = pInit_(hFile, maxStringLen, flags);
        if (!ctx) {
            SetFilePointerEx(hFile, original, nullptr, FILE_BEGIN);
            return 0;
        }

        if (pValidate_) {
            pValidate_(ctx);
        }

        uint64_t parsed = pParse_(ctx, callback, userData);
        SetFilePointerEx(hFile, original, nullptr, FILE_BEGIN);
        return parsed;
    }

    void Unload() {
        if (module_) {
            FreeLibrary(module_);
            module_ = nullptr;
        }
        ready_ = false;
        pInit_ = nullptr;
        pValidate_ = nullptr;
        pParse_ = nullptr;
    }

private:
    Bridge() = default;
    ~Bridge() = default;
    Bridge(const Bridge&) = delete;
    Bridge& operator=(const Bridge&) = delete;

    void LoadInternal() {
        if (module_) return;

        char dllPath[MAX_PATH] = {};
        const char* overridePath = std::getenv("RAWRXD_GGUF_MASM_DLL");
        if (overridePath && *overridePath) {
            strcpy_s(dllPath, overridePath);
        } else {
            if (GetModuleFileNameA(nullptr, dllPath, MAX_PATH)) {
                for (int i = static_cast<int>(strlen(dllPath)) - 1; i >= 0; --i) {
                    if (dllPath[i] == '\\' || dllPath[i] == '/') { dllPath[i + 1] = '\0'; break; }
                }
                strcat_s(dllPath, "RawrXD_GGUF_RobustTools.dll");
            }
        }

        module_ = LoadLibraryA(dllPath);
        if (!module_) {
            module_ = LoadLibraryA("RawrXD_GGUF_RobustTools.dll");
        }
        if (!module_) return;

        pInit_ = reinterpret_cast<FnInitContext>(GetProcAddress(module_, "GGUF_InitContext"));
        pValidate_ = reinterpret_cast<FnValidateHeader>(GetProcAddress(module_, "GGUF_ValidateHeader"));
        pParse_ = reinterpret_cast<FnParseMetadata>(GetProcAddress(module_, "GGUF_ParseMetadataSafe"));

        ready_ = (pInit_ && pParse_);
    }

    std::once_flag init_flag_;
    HMODULE module_ = nullptr;
    bool ready_ = false;
    FnInitContext pInit_ = nullptr;
    FnValidateHeader pValidate_ = nullptr;
    FnParseMetadata pParse_ = nullptr;
};

} // namespace rawrxd::gguf::masm
