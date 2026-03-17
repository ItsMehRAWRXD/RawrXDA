#include <windows.h>
#include <string>
#include <cstdio>

std::string dump_pe(const char* path){
    HANDLE h=CreateFileA(path,GENERIC_READ,FILE_SHARE_READ,0,OPEN_EXISTING,0,0);
    if (h == INVALID_HANDLE_VALUE) return "Failed to open file.";

    HANDLE m=CreateFileMapping(h,0,PAGE_READONLY,0,0,0);
    if (!m) { CloseHandle(h); return "Failed to map file."; }

    uint8_t* b=(uint8_t*)MapViewOfFile(m,FILE_MAP_READ,0,0,0);
    if (!b) { CloseHandle(m); CloseHandle(h); return "Failed to view file."; }

    IMAGE_DOS_HEADER* dos=(IMAGE_DOS_HEADER*)b;
    if (dos->e_magic != 0x5A4D) { // MZ
         UnmapViewOfFile(b); CloseHandle(m); CloseHandle(h);
         return "Not a PE file.";
    }

    IMAGE_NT_HEADERS* nt=(IMAGE_NT_HEADERS*)(b+dos->e_lfanew);

    char buf[512];
    sprintf(buf,"Machine: %X\nSections:%d\nEntry:%X\n",
        nt->FileHeader.Machine,
        nt->FileHeader.NumberOfSections,
        nt->OptionalHeader.AddressOfEntryPoint);

    UnmapViewOfFile(b);
    CloseHandle(m);
    CloseHandle(h);

    return std::string(buf);
}

std::string run_compiler(const char* src){
    // In a real scenario, we'd write src to a file first
    // This is just a shell exec stub as requested
    system(("rawrxd_compiler_masm64.exe "+std::string(src)).c_str());
    return "Compiled.";
}
