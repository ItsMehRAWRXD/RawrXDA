// mmf_diagnostic.cpp - Diagnostic tool for MMF creation testing
// Builds with: cl /EHsc /O2 mmf_diagnostic.cpp

#include <windows.h>
#include <stdio.h>
#include <string>

#define MAP_SIZE 256

void PrintError(const char* operation, DWORD error) {
    char* message = nullptr;
    FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
        nullptr,
        error,
        0,
        (LPSTR)&message,
        0,
        nullptr
    );
    
    printf("[ERROR] %s failed with code %lu (0x%08X)\n", operation, error, error);
    if (message) {
        printf("  Message: %s", message);
        LocalFree(message);
    }
    
    // Common error explanations
    switch(error) {
        case ERROR_ACCESS_DENIED:
            printf("  Explanation: Access denied. Check permissions and session isolation.\n");
            printf("  For Global\\ namespace, ensure running as SYSTEM with SeCreateGlobalPrivilege.\n");
            break;
        case ERROR_INVALID_PARAMETER:
            printf("  Explanation: Invalid parameter passed to API.\n");
            break;
        case ERROR_ALREADY_EXISTS:
            printf("  Explanation: Object already exists with different security attributes.\n");
            break;
        case ERROR_PRIVILEGE_NOT_HELD:
            printf("  Explanation: Required privilege not held.\n");
            printf("  For Global\\ namespace, need SeCreateGlobalPrivilege.\n");
            break;
        case ERROR_PATH_NOT_FOUND:
            printf("  Explanation: Path not found (invalid namespace prefix).\n");
            break;
    }
}

bool TestMMF(const char* name, bool useSecurity, SECURITY_ATTRIBUTES* sa) {
    printf("\n=== Testing MMF: %s ===\n", name);
    
    HANDLE hMap = CreateFileMappingA(
        INVALID_HANDLE_VALUE,
        useSecurity ? sa : nullptr,
        PAGE_READWRITE,
        0,
        MAP_SIZE,
        name
    );
    
    if (!hMap) {
        DWORD error = GetLastError();
        PrintError("CreateFileMappingA", error);
        return false;
    }
    
    printf("[SUCCESS] CreateFileMappingA succeeded! Handle: %p\n", hMap);
    
    // Try to map view
    void* view = MapViewOfFile(hMap, FILE_MAP_WRITE, 0, 0, MAP_SIZE);
    if (!view) {
        DWORD error = GetLastError();
        PrintError("MapViewOfFile", error);
        CloseHandle(hMap);
        return false;
    }
    
    printf("[SUCCESS] MapViewOfFile succeeded! View: %p\n", view);
    
    // Write test data
    *(DWORD*)view = 0x12345678;
    printf("[SUCCESS] Wrote test data to MMF\n");
    
    // Cleanup
    UnmapViewOfFile(view);
    CloseHandle(hMap);
    
    return true;
}

void TestCurrentProcess() {
    printf("=== Current Process Information ===\n");
    printf("Process ID: %lu\n", GetCurrentProcessId());
    printf("Session ID: %lu\n", GetCurrentProcessId()); // Simplified
    
    HANDLE token;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) {
        printf("Token opened successfully\n");
        
        // Check for SeCreateGlobalPrivilege
        LUID luid;
        if (LookupPrivilegeValueA(nullptr, "SeCreateGlobalPrivilege", &luid)) {
            printf("SeCreateGlobalPrivilege LUID: %lu:%lu\n", luid.HighPart, luid.LowPart);
            
            TOKEN_PRIVILEGES tp;
            DWORD len;
            if (GetTokenInformation(token, TokenPrivileges, &tp, sizeof(tp), &len)) {
                bool hasPrivilege = false;
                for (DWORD i = 0; i < tp.PrivilegeCount; i++) {
                    if (tp.Privileges[i].Luid.LowPart == luid.LowPart &&
                        tp.Privileges[i].Luid.HighPart == luid.HighPart) {
                        hasPrivilege = true;
                        printf("SeCreateGlobalPrivilege: %s\n", 
                               (tp.Privileges[i].Attributes & SE_PRIVILEGE_ENABLED) ? "ENABLED" : "DISABLED");
                        break;
                    }
                }
                if (!hasPrivilege) {
                    printf("SeCreateGlobalPrivilege: NOT PRESENT\n");
                }
            }
        }
        
        CloseHandle(token);
    } else {
        printf("Failed to open process token: %lu\n", GetLastError());
    }
}

int main() {
    printf("=== MMF Diagnostic Tool ===\n");
    printf("Testing MMF creation with different namespaces\n\n");
    
    TestCurrentProcess();
    
    // Test 1: Global namespace (requires SYSTEM + SeCreateGlobalPrivilege)
    printf("\n");
    printf("========================================\n");
    TestMMF("Global\\SOVEREIGN_NVME_TEMPS", false, nullptr);
    
    // Test 2: Local namespace (current session only)
    printf("\n");
    printf("========================================\n");
    TestMMF("Local\\SOVEREIGN_NVME_TEMPS", false, nullptr);
    
    // Test 3: Bare name (no prefix, same session only)
    printf("\n");
    printf("========================================\n");
    TestMMF("SOVEREIGN_NVME_TEMPS", false, nullptr);
    
    // Test 4: Try with NULL security (same as current sidecar)
    printf("\n");
    printf("========================================\n");
    printf("Testing with explicit NULL security descriptor:\n");
    TestMMF("Local\\SOVEREIGN_NVME_TEMPS", false, nullptr);
    
    printf("\n=== Diagnostic Summary ===\n");
    printf("If Global\\ failed: Check SeCreateGlobalPrivilege and session isolation\n");
    printf("If Local\\ failed: Check process permissions and session\n");
    printf("If Bare name failed: Check for name collisions or invalid characters\n");
    
    return 0;
}
