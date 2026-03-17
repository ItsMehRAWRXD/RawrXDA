#include <cstdint>
#include <cstdio>

extern "C" void* PEWriter_CreateExecutable(uint64_t imageBase, uint32_t entryRva);
extern "C" uint64_t PEWriter_AddImport(void* ctx, const char* dllName, const char* fnName);
extern "C" uint64_t PEWriter_AddCode(void* ctx, const void* code, uint32_t codeSize);
extern "C" uint64_t PEWriter_WriteFile(void* ctx, const char* outPath);

int main() {
    void* ctx = PEWriter_CreateExecutable(0x140000000ull, 0x1000u);
    if (!ctx) {
        std::printf("create failed\n");
        return 1;
    }

    struct ImportPair { const char* dll; const char* fn; } imports[] = {
        {"kernel32.dll", "ExitProcess"},
        {"kernel32.dll", "GetStdHandle"},
        {"kernel32.dll", "WriteFile"},
        {"user32.dll", "MessageBoxA"},
        {"ntdll.dll", "RtlInitUnicodeString"},
    };

    for (const auto& imp : imports) {
        if (!PEWriter_AddImport(ctx, imp.dll, imp.fn)) {
            std::printf("add import failed: %s!%s\n", imp.dll, imp.fn);
            return 2;
        }
    }

    // x64: xor ecx,ecx; call ExitProcess (rel32 placeholder); ret
    unsigned char code[] = { 0x31,0xC9, 0xE8,0,0,0,0, 0xC3 };
    if (!PEWriter_AddCode(ctx, code, (uint32_t)sizeof(code))) {
        std::printf("add code failed\n");
        return 3;
    }

    if (!PEWriter_WriteFile(ctx, "D:\\RawrXD\\test_output_imports.exe")) {
        std::printf("write failed\n");
        return 4;
    }

    std::printf("ok\n");
    return 0;
}
