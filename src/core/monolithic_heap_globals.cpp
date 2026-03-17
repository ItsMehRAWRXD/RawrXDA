#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

// Monolithic MASM modules (e.g., src/asm/monolithic/beacon.asm) expect a global
// heap handle named `g_hHeap`. In the full monolithic bootstrap this is set via
// HeapCreate; for the Win32IDE build we use the process heap as a safe default.
extern "C" void* g_hHeap = ::GetProcessHeap();
#endif

