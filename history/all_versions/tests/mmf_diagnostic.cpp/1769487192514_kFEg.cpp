// mmf_diagnostic.cpp - Comprehensive MMF Creation Diagnostic Tool
// Tests CreateFileMappingA with various namespaces to isolate failures
// Build: cl.exe mmf_diagnostic.cpp /Fe:mmf_diagnostic.exe advapi32.lib

#include <windows.h>
#include <stdio.h>
#include <sddl.h>

void PrintError(const char* context, DWORD err) {
    char msg[512] = {0};
    FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, 
                   NULL, err, 0, msg, sizeof(msg)-1, NULL);
    // Remove trailing newline
    size_t len = strlen(msg);
    while (len > 0 && (msg[len-1] == '\n' || msg[len-1] == '\r')) msg[--len] = 0;
    
    printf("  [FAILED] %s\n", context);
    printf("  GetLastError = %lu (0x%lX)\n", err, err);
    printf("  Message: %s\n", msg);
    
    // Common error explanations
    switch(err) {
        case 5:   printf("  >>> ACCESS DENIED - Need elevated privileges or DACL\n"); break;
        case 87:  printf("  >>> INVALID PARAMETER - Check size or name format\n"); break;
        case 183: printf("  >>> ALREADY EXISTS - Object exists with different access\n"); break;
        case 1314: printf("  >>> PRIVILEGE NOT HELD - Need SeCreateGlobalPrivilege\n"); break;
        case 2:   printf("  >>> FILE NOT FOUND - Namespace may not exist\n"); break;
    }
}

bool TestMMF(const char* name, SECURITY_ATTRIBUTES* pSA, const char* description) {
    printf("\n--- %s ---\n", description);
    printf("  Name: \"%s\"\n", name);
    printf("  Security: %s\n", pSA ? "Custom DACL" : "NULL (default)");
    
    HANDLE hMap = CreateFileMappingA(
        INVALID_HANDLE_VALUE,   // Pagefile-backed
        pSA,                    // Security attributes
        PAGE_READWRITE,         // Read/Write access
        0,                      // Size high DWORD
        1024,                   // Size low DWORD (1KB)
        name                    // Object name
    );
    
    if (!hMap) {
        DWORD err = GetLastError();
        PrintError("CreateFileMappingA", err);
        return false;
    }
    
    DWORD lastErr = GetLastError();
    if (lastErr == ERROR_ALREADY_EXISTS) {
        printf("  [WARNING] Opened EXISTING mapping (not created new)\n");
    }
    
    printf("  [SUCCESS] Handle = 0x%p\n", hMap);
    
    // Try to map a view
    void* pView = MapViewOfFile(hMap, FILE_MAP_WRITE, 0, 0, 1024);
    if (!pView) {
        DWORD err = GetLastError();
        PrintError("MapViewOfFile", err);
        CloseHandle(hMap);
        return false;
    }
    
    printf("  [SUCCESS] View mapped at 0x%p\n", pView);
    
    // Write test data
    memcpy(pView, "SOVE", 4);
    printf("  [SUCCESS] Wrote test signature 'SOVE'\n");
    
    UnmapViewOfFile(pView);
    CloseHandle(hMap);
    return true;
}

void CheckPrivileges() {
    printf("\n=== PRIVILEGE CHECK ===\n");
    
    HANDLE hToken = NULL;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken)) {
        PrintError("OpenProcessToken", GetLastError());
        return;
    }
    
    // Check SeCreateGlobalPrivilege
    LUID luid;
    if (LookupPrivilegeValueA(NULL, "SeCreateGlobalPrivilege", &luid)) {
        PRIVILEGE_SET ps = {0};
        ps.PrivilegeCount = 1;
        ps.Privilege[0].Luid = luid;
        ps.Privilege[0].Attributes = SE_PRIVILEGE_ENABLED;
        
        BOOL hasPriv = FALSE;
        if (PrivilegeCheck(hToken, &ps, &hasPriv)) {
            printf("  SeCreateGlobalPrivilege: %s\n", hasPriv ? "ENABLED" : "DISABLED");
        }
    }
    
    // Get token user info
    char buffer[256];
    DWORD size = sizeof(buffer);
    if (GetTokenInformation(hToken, TokenUser, buffer, size, &size)) {
        TOKEN_USER* pUser = (TOKEN_USER*)buffer;
        char name[128], domain[128];
        DWORD nameLen = sizeof(name), domainLen = sizeof(domain);
        SID_NAME_USE use;
        if (LookupAccountSidA(NULL, pUser->User.Sid, name, &nameLen, domain, &domainLen, &use)) {
            printf("  Running as: %s\\%s\n", domain, name);
        }
    }
    
    // Check elevation
    TOKEN_ELEVATION elev;
    size = sizeof(elev);
    if (GetTokenInformation(hToken, TokenElevation, &elev, size, &size)) {
        printf("  Elevated: %s\n", elev.TokenIsElevated ? "YES" : "NO");
    }
    
    CloseHandle(hToken);
}

int main() {
    printf("╔══════════════════════════════════════════════════════════════╗\n");
    printf("║     SOVEREIGN MMF DIAGNOSTIC TOOL v1.0                       ║\n");
    printf("║     Tests CreateFileMappingA with various configurations     ║\n");
    printf("╚══════════════════════════════════════════════════════════════╝\n");
    
    CheckPrivileges();
    
    printf("\n=== MMF CREATION TESTS ===\n");
    
    // Test 1: Bare name (no namespace prefix)
    bool bareOK = TestMMF("SOVEREIGN_NVME_TEMPS_TEST", NULL, "TEST 1: Bare Name (No Prefix)");
    
    // Test 2: Local namespace
    bool localOK = TestMMF("Local\\SOVEREIGN_NVME_TEMPS_TEST", NULL, "TEST 2: Local Namespace");
    
    // Test 3: Global namespace (requires privilege)
    bool globalOK = TestMMF("Global\\SOVEREIGN_NVME_TEMPS_TEST", NULL, "TEST 3: Global Namespace");
    
    // Test 4: Global with explicit DACL (Everyone full access)
    PSECURITY_DESCRIPTOR pSD = NULL;
    SECURITY_ATTRIBUTES sa = {0};
    bool globalDaclOK = false;
    
    if (ConvertStringSecurityDescriptorToSecurityDescriptorA(
            "D:(A;;GA;;;WD)",  // Grant All to Everyone (World)
            SDDL_REVISION_1, 
            &pSD, 
            NULL)) {
        sa.nLength = sizeof(sa);
        sa.lpSecurityDescriptor = pSD;
        sa.bInheritHandle = FALSE;
        
        globalDaclOK = TestMMF("Global\\SOVEREIGN_NVME_TEMPS_DACL", &sa, "TEST 4: Global + DACL (Everyone Access)");
        LocalFree(pSD);
    } else {
        printf("\n--- TEST 4: Global + DACL ---\n");
        PrintError("ConvertStringSecurityDescriptorToSecurityDescriptorA", GetLastError());
    }
    
    // Summary
    printf("\n╔══════════════════════════════════════════════════════════════╗\n");
    printf("║                        SUMMARY                               ║\n");
    printf("╠══════════════════════════════════════════════════════════════╣\n");
    printf("║  Bare Name:      %s                                       ║\n", bareOK ? "PASS" : "FAIL");
    printf("║  Local\\:         %s                                       ║\n", localOK ? "PASS" : "FAIL");
    printf("║  Global\\:        %s                                       ║\n", globalOK ? "PASS" : "FAIL");
    printf("║  Global + DACL:  %s                                       ║\n", globalDaclOK ? "PASS" : "FAIL");
    printf("╚══════════════════════════════════════════════════════════════╝\n");
    
    // Recommendation
    printf("\n>>> RECOMMENDATION:\n");
    if (globalOK || globalDaclOK) {
        printf("    Global\\ namespace works. MASM sidecar should succeed.\n");
        printf("    If sidecar still fails, check for MASM calling convention bugs.\n");
    } else if (localOK) {
        printf("    Use Local\\ namespace in sidecar and reader.\n");
        printf("    Change mapName to: \"Local\\\\SOVEREIGN_NVME_TEMPS\"\n");
    } else if (bareOK) {
        printf("    Use bare name without any prefix.\n");
        printf("    Change mapName to: \"SOVEREIGN_NVME_TEMPS\"\n");
    } else {
        printf("    All tests failed. Check if security software is blocking MMF creation.\n");
    }
    
    printf("\nPress Enter to exit...\n");
    getchar();
    return 0;
}
