#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <windows.h>


namespace SovereignAssembler
{

// Function pointer type for the delimiter scanner
using FindDelimiterFn = const char* (*)(const char* start, const char* end);

// Global pointer – initially points to scalar version
extern FindDelimiterFn g_findNextDelimiter;

// Hot-patch: load AVX2 DLL and replace g_findNextDelimiter
bool HotPatchTokenizer(const wchar_t* dllPath);

// Phase 22: Runtime Kernel Management
// ====================================

// Kernel activation - switch to named kernel at runtime
bool ActivateKernel(const char* kernelName, std::string& errorMsg);

// Get the currently active kernel name
const char* GetActiveKernelName();

// Auto-detect CPU features and select optimal kernel
bool AutoSelectOptimalKernel(std::string& report);

// Load optimized kernel from DLL
bool LoadOptimizedKernel(const wchar_t* dllPath, std::string& errorMsg);

// Performance monitoring
struct PerformanceStats {
    uint64_t callCount;
    uint64_t totalBytesScanned;
    uint64_t cacheHits;
    uint64_t cacheMisses;
};

void ResetPerformanceStats();
PerformanceStats GetPerformanceStats();
void PrintKernelStatus();

// Verify PE optional-header CheckSum field (same algorithm as the loader; scalar, no external DLL)
bool VerifyPEChecksum(const std::vector<uint8_t>& peBinary);

struct AssemblyResult
{
    std::vector<uint8_t> code;        // .text section
    std::vector<uint8_t> data;        // .data section
    uint64_t entryPointRVA = 0x1000;  // default entry
    std::string error;
    bool success = false;
};

// Assemble MASM-like source into raw .text/.data buffers (no PE write; for tests and tooling).
bool AssembleToBuffer(const std::string& source, AssemblyResult& out, std::string& errorMsg);

// Assemble MASM-like source into a PE executable (.text, .data, optional .idata).
//
// Windows API imports (no external linker):
//   import kernel32.dll ExitProcess, GetModuleHandleA
//   ; then:  call ExitProcess   -> FF 15 rel32 to IAT slot; loader fills IAT at runtime
// DLL name is the first token after `import`; remaining comma-separated names are functions.
// Multiple `import` lines merge by DLL. Requires at least one `import` if you use `call` to those names.
bool AssembleAndLink(const std::string& source, const std::wstring& outputExePath, std::string& errorMsg);

// Same language subset: emit AMD64 COFF object (.obj) for MSVC link (IMAGE_REL_AMD64_REL32 for jmp/call labels).
bool AssembleToCOFF(const std::string& source, const std::wstring& outputObjPath, std::string& errorMsg);

// For Tool Registry: C-exported function
extern "C" __declspec(dllexport) int AssembleSovereign(const char* source, const wchar_t* outputPath, char* errorBuf,
                                                       size_t errorBufSize);

extern "C" __declspec(dllexport) int AssembleSovereignCOFF(const char* source, const wchar_t* outputObjPath,
                                                           char* errorBuf, size_t errorBufSize);

}  // namespace SovereignAssembler
