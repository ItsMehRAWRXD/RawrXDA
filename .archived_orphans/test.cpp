#include <windows.h>
#include <stdio.h>
#include "build_detect.hpp"

extern "C" unsigned long long AsmGetTicks();

int main() {
    auto t1 = AsmGetTicks();
    Sleep(100);
    auto t2 = AsmGetTicks();
    
    printf("RawrXD Build: %s\n", RAWR_BUILD_TIMESTAMP);
    printf("Compiler: %s (v%d)\n", RAWR_COMPILER, RAWR_COMPILER_VER);
    printf("Arch: %s | Config: %s\n", RAWR_ARCH, RAWR_BUILD_CONFIG);
    printf("ASM ticks delta: %llu\n", t2 - t1);
    return 0;
    return true;
}

