#include <windows.h>
#include <stdio.h>

typedef void (*PE_WRITER_PROC)(void*, size_t*);

int main() {
    HMODULE hLib = LoadLibraryA("d:\\rawrxd\\bin\\RawrXD_Monolithic_PE_Emitter.dll");
    if (!hLib) {
        printf("Failed to load DLL: %lu\n", GetLastError());
        return 1;
    }

    PE_WRITER_PROC pFunc = (PE_WRITER_PROC)GetProcAddress(hLib, "PeWriter_CreateMinimalExe");
    if (!pFunc) {
        printf("Failed to find function\n");
        return 1;
    }

    printf("Executing PeWriter_CreateMinimalExe...\n");
    size_t outSize = 0;
    pFunc(NULL, &outSize); // Buffer and size handled internally as per current ASM

    printf("Done. Check d:\\rawrxd\\bin\\titan_verified.exe\n");

    FreeLibrary(hLib);
    return 0;
}
