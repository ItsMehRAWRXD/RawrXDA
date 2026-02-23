#include <windows.h>
#include <string>
#include <cstdio>
#include <cstdint>
#include <iostream>
#include <fstream>

std::string dump_pe(const char* path){
    if (!path || *path == 0) return "Error: Invalid path";
    
    // Add extension if missing for testing convenience
    std::string fullPath = path;
    if (fullPath.find('.') == std::string::npos) fullPath += ".exe";

    HANDLE h=CreateFileA(fullPath.c_str(),GENERIC_READ,FILE_SHARE_READ,0,OPEN_EXISTING,0,0);
    if (h == INVALID_HANDLE_VALUE) return "Failed to open file: " + fullPath;

    HANDLE m=CreateFileMapping(h,0,PAGE_READONLY,0,0,0);
    if (!m) { CloseHandle(h); return "Failed to map file."; }

    uint8_t* b=(uint8_t*)MapViewOfFile(m,FILE_MAP_READ,0,0,0);
    if (!b) { CloseHandle(m); CloseHandle(h); return "Failed to view file."; }

    std::string output;
    char buf[512];

    try {
        IMAGE_DOS_HEADER* dos=(IMAGE_DOS_HEADER*)b;
        if (dos->e_magic != 0x5A4D) { // MZ
             UnmapViewOfFile(b); CloseHandle(m); CloseHandle(h);
             return "Not a valid PE file (Missing MZ signature).";
        }

        IMAGE_NT_HEADERS* nt=(IMAGE_NT_HEADERS*)(b+dos->e_lfanew);
        
        sprintf(buf, "PE Analysis for: %s\n\n", fullPath.c_str());
        output += buf;
        
        sprintf(buf, "Machine:         0x%X (%s)\n", nt->FileHeader.Machine, 
            nt->FileHeader.Machine == 0x8664 ? "x64" : "x86/Other");
        output += buf;
        
        sprintf(buf, "Sections:        %d\n", nt->FileHeader.NumberOfSections);
        output += buf;
        
        sprintf(buf, "Timestamp:       0x%X\n", nt->FileHeader.TimeDateStamp);
        output += buf;
        
        sprintf(buf, "Entry Point:     0x%X\n", nt->OptionalHeader.AddressOfEntryPoint);
        output += buf;
        
        sprintf(buf, "Image Base:      0x%llX\n", (unsigned long long)nt->OptionalHeader.ImageBase);
        output += buf;
        
        output += "\n[Section Headers]\n";
        IMAGE_SECTION_HEADER* sec = IMAGE_FIRST_SECTION(nt);
        for(int i=0; i<nt->FileHeader.NumberOfSections; i++) {
            char name[9] = {0};
            memcpy(name, sec[i].Name, 8);
            sprintf(buf, "  %-8s | RawSize: 0x%-6X | RawAddr: 0x%-6X | VirtAddr: 0x%-6X\n", 
                name, sec[i].SizeOfRawData, sec[i].PointerToRawData, sec[i].VirtualAddress);
            output += buf;
        }

    } catch (...) {
        output = "Exception during analysis.";
    }

    UnmapViewOfFile(b);
    CloseHandle(m);
    CloseHandle(h);

    return output;
}

std::string run_compiler(const char* src){
    // Write source to temp file
    std::string tempCtx = "rawr_temp.asm";
    std::ofstream ofs(tempCtx);
    ofs << src;
    ofs.close();

    // Try MASM (ml64) first; same return shape for parity with custom/non-MASM builds.
    std::string cmd = "ml64.exe /c /nologo " + tempCtx + " > rawr_compile_log.txt 2>&1";
    int ret = system(cmd.c_str());

    std::ifstream logf("rawr_compile_log.txt");
    std::string result((std::istreambuf_iterator<char>(logf)), std::istreambuf_iterator<char>());
    logf.close();

    // NASM fallback: if MASM failed, try NASM so non-MASM builds function the same way
    if (ret != 0) {
        std::string objOut = "rawr_temp.obj";
        cmd = "nasm.exe -f win64 -o " + objOut + " " + tempCtx + " > rawr_compile_log.txt 2>&1";
        ret = system(cmd.c_str());
        logf.open("rawr_compile_log.txt");
        if (logf) {
            result.assign((std::istreambuf_iterator<char>(logf)), std::istreambuf_iterator<char>());
            logf.close();
        }
        remove(objOut.c_str());
    }

    remove(tempCtx.c_str());
    remove("rawr_compile_log.txt");

    if (ret != 0) {
        if (result.empty()) return "Compiler execution failed (ml64.exe and nasm.exe not found or both failed).";
        return "Compilation Errors:\n" + result;
    }

    if (result.empty()) return "Compilation successful (No output).";
    return "Output:\n" + result;
}
