// ============================================================================
// crt_free_entry.cpp — CRT-free entry point for -1 dependencies
// Replaces MSVCRT startup code with direct Win32 kernel calls
// ============================================================================

#include <windows.h>
#include "crt_free_memory.h"

namespace RawrXD {

// Forward declarations
int MainNoCRuntime(int argc, char** argv);

// ============================================================================
// ParseCommandLine — Parse Windows command line into argc/argv
// ============================================================================
void ParseCommandLine(int& argc, char**& argv) {
    const char* cmdLine = GetCommandLineA();
    if (!cmdLine) {
        argc = 0;
        argv = nullptr;
        return;
    }
    
    // First pass: count arguments
    int count = 0;
    const char* p = cmdLine;
    bool inQuotes = false;
    bool inArg = false;
    
    while (*p) {
        if (*p == '"') {
            inQuotes = !inQuotes;
        } else if (*p == ' ' && !inQuotes) {
            if (inArg) {
                inArg = false;
                count++;
            }
        } else if (!inArg) {
            inArg = true;
        }
        p++;
    }
    if (inArg) count++;
    
    // Allocate argv array
    argc = count;
    argv = (char**)CRTFreeMemory::Allocate((count + 1) * sizeof(char*));
    if (!argv) return;
    
    // Second pass: extract arguments
    p = cmdLine;
    int argIdx = 0;
    inQuotes = false;
    inArg = false;
    const char* argStart = nullptr;
    
    while (*p && argIdx < count) {
        if (*p == '"') {
            if (!inQuotes && !inArg) {
                argStart = p + 1;
                inArg = true;
            }
            inQuotes = !inQuotes;
        } else if (*p == ' ' && !inQuotes) {
            if (inArg) {
                size_t len = p - argStart;
                char* arg = (char*)CRTFreeMemory::Allocate(len + 1);
                if (arg) {
                    CopyMemory(arg, argStart, len);
                    arg[len] = '\0';
                    argv[argIdx++] = arg;
                }
                inArg = false;
            }
        } else if (!inArg) {
            argStart = p;
            inArg = true;
        }
        p++;
    }
    
    if (inArg && argIdx < count) {
        size_t len = p - argStart;
        char* arg = (char*)CRTFreeMemory::Allocate(len + 1);
        if (arg) {
            CopyMemory(arg, argStart, len);
            arg[len] = '\0';
            argv[argIdx++] = arg;
        }
    }
    
    argv[count] = nullptr;  // Null terminator
}

}  // namespace RawrXD

// ============================================================================
// WinMainCRTStartup / mainCRTStartup replacement
// This is called directly by the kernel when /NODEFAULTLIB is set
// ============================================================================
extern "C" int mainCRTStartup() {
    int argc = 0;
    char** argv = nullptr;
    
    RawrXD::ParseCommandLine(argc, argv);
    
    int exitCode = RawrXD::MainNoCRuntime(argc, argv);
    
    // Cleanup argv
    if (argv) {
        for (int i = 0; i < argc; i++) {
            if (argv[i]) {
                RawrXD::CRTFreeMemory::Deallocate(argv[i]);
            }
        }
        RawrXD::CRTFreeMemory::Deallocate(argv);
    }
    
    // Exit directly without CRT cleanup
    ExitProcess(exitCode);
    return exitCode;
}

// GUI application entry point equivalent
extern "C" int WinMainCRTStartup() {
    return mainCRTStartup();
}

// Emergency fallback in case things go terribly wrong
extern "C" void __cdecl abort() {
    ExitProcess(1);
}

// Allocator stubs that some code might reference
extern "C" void* __cdecl malloc(size_t size) {
    return RawrXD::CRTFreeMemory::Allocate(size);
}

extern "C" void __cdecl free(void* ptr) {
    RawrXD::CRTFreeMemory::Deallocate(ptr);
}

extern "C" void* __cdecl calloc(size_t count, size_t size) {
    size_t total = count * size;
    void* ptr = RawrXD::CRTFreeMemory::Allocate(total);
    if (ptr) {
        ZeroMemory(ptr, total);
    }
    return ptr;
}

extern "C" void* __cdecl realloc(void* ptr, size_t size) {
    // Simplified realloc: allocate new, copy old, free old
    if (!ptr) return malloc(size);
    
    size_t oldSize = RawrXD::CRTFreeMemory::GetAllocationSize(ptr);
    void* newPtr = RawrXD::CRTFreeMemory::Allocate(size);
    if (newPtr) {
        CopyMemory(newPtr, ptr, (oldSize < size) ? oldSize : size);
        RawrXD::CRTFreeMemory::Deallocate(ptr);
    }
    return newPtr;
}
