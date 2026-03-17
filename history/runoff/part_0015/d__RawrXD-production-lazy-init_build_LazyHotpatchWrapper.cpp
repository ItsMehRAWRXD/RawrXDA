#include <windows.h>
#include <stdio.h>

// Forward declare the MASM function
extern "C" int RawrXD_LazyHotpatch(wchar_t* modelPath, HANDLE* outMap, void** outView);

// Export a clean C wrapper for PowerShell P/Invoke
extern "C" __declspec(dllexport) int LazyHotpatchWrapper(wchar_t* modelPath, HANDLE* outMap, void** outView)
{
    if (!modelPath || !outMap || !outView) {
        return 1;
    }
    
    *outMap = NULL;
    *outView = NULL;
    
    // Call the MASM implementation
    return RawrXD_LazyHotpatch(modelPath, outMap, outView);
}
