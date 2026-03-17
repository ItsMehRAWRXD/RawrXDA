// RawrXD Deobfuscator Test Harness
#include <windows.h>
#include <stdio.h>

// C interface to ASM functions
extern "C" {
    int OmegaDeobf_Initialize(void* base, unsigned __int64 flags);
    int OmegaDeobf_AnalyzeTarget();
    int OmegaDeobf_FullUnpack(const char* outputPath);
    int MetaReverse_Initialize(void* base, unsigned __int64 size);
    int MetaReverse_AnalyzeAuthenticity();
}

int main(int argc, char** argv) {
    printf("========================================\n");
    printf("  RawrXD Omega Deobfuscator Test\n");
    printf("========================================\n\n");
    
    if (argc < 2) {
        printf("Usage: %s <target.exe>\n", argv[0]);
        return 1;
    }
    
    // Load target
    HANDLE hFile = CreateFileA(argv[1], GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        printf("[-] Failed to open: %s\n", argv[1]);
        return 1;
    }
    
    DWORD size = GetFileSize(hFile, NULL);
    void* buffer = VirtualAlloc(NULL, size * 2, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    
    DWORD read;
    ReadFile(hFile, buffer, size, &read, NULL);
    CloseHandle(hFile);
    
    printf("[+] Loaded: %s (%u bytes)\n", argv[1], size);
    
    // Initialize engines
    printf("[*] Initializing OmegaDeobfuscator...\n");
    if (!OmegaDeobf_Initialize(buffer, 0)) {
        printf("[-] Omega init failed\n");
        return 1;
    }
    
    printf("[*] Initializing MetaReverse...\n");
    MetaReverse_Initialize(buffer, size);
    
    // Pre-analysis authenticity check
    printf("\n[*] Pre-analysis authenticity check...\n");
    int auth = MetaReverse_AnalyzeAuthenticity();
    printf("    Score: %d/100\n", auth);
    
    // Full deobfuscation
    printf("\n[*] Running full deobfuscation...\n");
    int layers = OmegaDeobf_AnalyzeTarget();
    printf("    Detected %d obfuscation layers\n", layers);
    
    // Unpack
    char outputPath[MAX_PATH];
    sprintf_s(outputPath, "%s.unpacked.exe", argv[1]);
    
    if (OmegaDeobf_FullUnpack(outputPath)) {
        printf("\n[+] Success! Unpacked to: %s\n", outputPath);
        
        // Post-check
        printf("\n[*] Post-analysis authenticity check...\n");
        auth = MetaReverse_AnalyzeAuthenticity();
        printf("    Score: %d/100\n", auth);
    } else {
        printf("\n[-] Unpack failed\n");
    }
    
    VirtualFree(buffer, 0, MEM_RELEASE);
    return 0;
}
