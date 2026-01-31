/**
 * CODEX REVERSE ENGINE ULTIMATE v7.0
 * Professional PE Analysis & Source Reconstruction System
 * 
 * C Implementation - GCC Compatible
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <windows.h>

// Version information
#define VER_MAJOR 7
#define VER_MINOR 0
#define VER_PATCH 0

// Constants
#define MAX_PATH 260
#define MAX_BUFFER 65536
#define MAX_SECTIONS 96
#define MAX_EXPORTS 8192
#define MAX_IMPORTS 16384

// Analysis structures
typedef struct {
    char Name[256];
    uint16_t Ordinal;
    uint32_t RVA;
    uint8_t Forwarded;
    char ForwardName[256];
    uint8_t CallingConv;  // 0=stdcall, 1=cdecl, 2=fastcall, 3=thiscall
    char ReturnType[64];
    uint8_t ParamCount;
    char ParamTypes[8][64];
} RECONSTRUCTED_EXPORT;

typedef struct {
    char DLLName[256];
    char FunctionName[256];
    uint16_t Ordinal;
    uint16_t Hint;
    uint8_t IsOrdinal;
    uint32_t IAT_RVA;
} RECONSTRUCTED_IMPORT;

typedef struct {
    char Name[9];
    uint32_t VirtualSize;
    uint32_t RawSize;
    uint32_t VirtualAddress;
    uint32_t Characteristics;
    double Entropy;
    uint8_t IsCode;
    uint8_t IsData;
    uint8_t IsExecutable;
    uint8_t IsWritable;
    uint8_t IsReadable;
} SECTION_ANALYSIS;

// Global variables
char szInputPath[MAX_PATH];
char szOutputPath[MAX_PATH];
char szProjectName[128];
char szTempBuffer[MAX_BUFFER];

void* pFileBase = NULL;
uint64_t qwFileSize = 0;
IMAGE_DOS_HEADER* pDosHeader = NULL;
IMAGE_NT_HEADERS64* pNtHeaders = NULL;
IMAGE_SECTION_HEADER* pSectionHeaders = NULL;
uint32_t dwSectionCount = 0;
uint8_t bIs64Bit = 0;
uint8_t bIsDLL = 0;
uint64_t qwImageBase = 0;

RECONSTRUCTED_EXPORT ExportsArray[MAX_EXPORTS];
RECONSTRUCTED_IMPORT ImportsArray[MAX_IMPORTS];
SECTION_ANALYSIS SectionsArray[MAX_SECTIONS];
uint32_t dwExportCount = 0;
uint32_t dwImportCount = 0;

// Statistics
uint32_t dwFilesProcessed = 0;
uint32_t dwHeadersGenerated = 0;
uint32_t dwTotalExports = 0;
uint32_t dwTotalImports = 0;

// Function prototypes
void PrintBanner();
void PrintMenu();
void ReadLine(char* buffer, size_t size);
int MapTargetFile(const char* lpFileName);
void UnmapTargetFile();
int ParsePEHeaders();
uint32_t RVAToFileOffset(uint32_t dwRVA);
int ParseExportTable();
int ParseImportTable();
int InferCallingConvention(const char* lpName);
int GenerateHeaderFile(const char* lpModuleName);
int GenerateCMakeLists();
int ProcessPEFile(const char* lpFilePath);
int ProcessDirectory(const char* lpPath);

// Utility functions
void PrintBanner() {
    printf("\n");
    printf("╔══════════════════════════════════════════════════════════════╗\n");
    printf("║     CODEX REVERSE ENGINE ULTIMATE v%d.%d.%d                    ║\n", VER_MAJOR, VER_MINOR, VER_PATCH);
    printf("║     Professional PE Analysis & Source Reconstruction        ║\n");
    printf("╚══════════════════════════════════════════════════════════════╝\n");
    printf("\n");
}

void PrintMenu() {
    printf("Main Menu:\n");
    printf("1. Analyze Single PE File\n");
    printf("2. Batch Process Directory\n");
    printf("3. Exit\n");
    printf("\nChoice: ");
}

void ReadLine(char* buffer, size_t size) {
    if (fgets(buffer, size, stdin)) {
        // Remove newline
        size_t len = strlen(buffer);
        if (len > 0 && buffer[len - 1] == '\n') {
            buffer[len - 1] = '\0';
        }
    }
}

int MapTargetFile(const char* lpFileName) {
    HANDLE hFile = CreateFileA(lpFileName, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        return 0;
    }
    
    LARGE_INTEGER fileSize;
    if (!GetFileSizeEx(hFile, &fileSize)) {
        CloseHandle(hFile);
        return 0;
    }
    
    qwFileSize = fileSize.QuadPart;
    if (qwFileSize > MAX_BUFFER) {
        CloseHandle(hFile);
        return 0;
    }
    
    HANDLE hMapping = CreateFileMappingA(hFile, NULL, PAGE_READONLY, 0, 0, NULL);
    if (!hMapping) {
        CloseHandle(hFile);
        return 0;
    }
    
    pFileBase = MapViewOfFile(hMapping, FILE_MAP_READ, 0, 0, 0);
    CloseHandle(hMapping);
    CloseHandle(hFile);
    
    return pFileBase != NULL;
}

void UnmapTargetFile() {
    if (pFileBase) {
        UnmapViewOfFile(pFileBase);
        pFileBase = NULL;
    }
}

int ParsePEHeaders() {
    pDosHeader = (IMAGE_DOS_HEADER*)pFileBase;
    
    // Validate DOS signature
    if (pDosHeader->e_magic != IMAGE_DOS_SIGNATURE) {
        return 0;
    }
    
    // Get NT headers offset
    if (pDosHeader->e_lfanew <= 0 || pDosHeader->e_lfanew > 4096) {
        return 0;
    }
    
    pNtHeaders = (IMAGE_NT_HEADERS64*)((uint8_t*)pFileBase + pDosHeader->e_lfanew);
    
    // Validate NT signature
    if (pNtHeaders->Signature != IMAGE_NT_SIGNATURE) {
        return 0;
    }
    
    // Determine architecture
    uint16_t machine = pNtHeaders->FileHeader.Machine;
    
    if (machine == IMAGE_FILE_MACHINE_AMD64) {
        bIs64Bit = 1;
    } else if (machine == IMAGE_FILE_MACHINE_I386) {
        bIs64Bit = 0;
    } else if (machine == IMAGE_FILE_MACHINE_ARM64) {
        bIs64Bit = 1;
    } else {
        return 0;
    }
    
    // Get section count
    dwSectionCount = pNtHeaders->FileHeader.NumberOfSections;
    if (dwSectionCount > MAX_SECTIONS) {
        return 0;
    }
    
    // Check if DLL
    bIsDLL = (pNtHeaders->FileHeader.Characteristics & IMAGE_FILE_DLL) ? 1 : 0;
    
    // Get image base
    qwImageBase = pNtHeaders->OptionalHeader.ImageBase;
    
    // Get section headers
    pSectionHeaders = (IMAGE_SECTION_HEADER*)((uint8_t*)pNtHeaders + sizeof(IMAGE_NT_HEADERS64));
    
    return 1;
}

uint32_t RVAToFileOffset(uint32_t dwRVA) {
    for (uint32_t i = 0; i < dwSectionCount; i++) {
        uint32_t vaddr = pSectionHeaders[i].VirtualAddress;
        uint32_t vsize = pSectionHeaders[i].SizeOfRawData;
        
        if (dwRVA >= vaddr && dwRVA < vaddr + vsize) {
            return pSectionHeaders[i].PointerToRawData + (dwRVA - vaddr);
        }
    }
    return dwRVA;
}

int ParseExportTable() {
    uint32_t dwExpRVA = pNtHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress;
    
    if (dwExpRVA == 0) {
        dwExportCount = 0;
        return 1;
    }
    
    uint32_t dwExpOffset = RVAToFileOffset(dwExpRVA);
    if (dwExpOffset == 0) {
        dwExportCount = 0;
        return 1;
    }
    
    IMAGE_EXPORT_DIRECTORY* pExpDir = (IMAGE_EXPORT_DIRECTORY*)((uint8_t*)pFileBase + dwExpOffset);
    
    // Get tables
    uint32_t* pAddressTable = (uint32_t*)((uint8_t*)pFileBase + RVAToFileOffset(pExpDir->AddressOfFunctions));
    uint32_t* pNameTable = (uint32_t*)((uint8_t*)pFileBase + RVAToFileOffset(pExpDir->AddressOfNames));
    uint16_t* pOrdinalTable = (uint16_t*)((uint8_t*)pFileBase + RVAToFileOffset(pExpDir->AddressOfNameOrdinals));
    
    uint32_t dwBase = pExpDir->Base;
    uint32_t dwNumberOfNames = pExpDir->NumberOfNames;
    
    if (dwNumberOfNames > MAX_EXPORTS) {
        dwNumberOfNames = MAX_EXPORTS;
    }
    
    dwExportCount = dwNumberOfNames;
    
    for (uint32_t i = 0; i < dwNumberOfNames; i++) {
        // Get name
        uint32_t dwNameRVA = pNameTable[i];
        char* pName = (char*)((uint8_t*)pFileBase + RVAToFileOffset(dwNameRVA));
        strncpy(ExportsArray[i].Name, pName, sizeof(ExportsArray[i].Name) - 1);
        
        // Get ordinal
        ExportsArray[i].Ordinal = pOrdinalTable[i] + dwBase;
        
        // Get function RVA
        ExportsArray[i].RVA = pAddressTable[pOrdinalTable[i]];
        
        // Detect calling convention
        ExportsArray[i].CallingConv = InferCallingConvention(ExportsArray[i].Name);
    }
    
    return 1;
}

int InferCallingConvention(const char* lpName) {
    if (!lpName || strlen(lpName) == 0) {
        return 0; // stdcall default
    }
    
    // Simple heuristic based on name patterns
    if (lpName[0] == '_') {
        return 1; // cdecl
    }
    if (lpName[0] == '?') {
        return 3; // thiscall (C++ mangled)
    }
    if (lpName[0] == '@') {
        return 2; // fastcall
    }
    
    // Default to stdcall for Windows exports
    return 0;
}

int ParseImportTable() {
    uint32_t dwImpRVA = pNtHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress;
    
    if (dwImpRVA == 0) {
        dwImportCount = 0;
        return 1;
    }
    
    uint32_t dwImpOffset = RVAToFileOffset(dwImpRVA);
    if (dwImpOffset == 0) {
        dwImportCount = 0;
        return 1;
    }
    
    IMAGE_IMPORT_DESCRIPTOR* pImpDesc = (IMAGE_IMPORT_DESCRIPTOR*)((uint8_t*)pFileBase + dwImpOffset);
    
    dwImportCount = 0;
    
    for (uint32_t i = 0; pImpDesc[i].Name != 0; i++) {
        if (dwImportCount >= MAX_IMPORTS) {
            break;
        }
        
        // Get DLL name
        char* pDLLName = (char*)((uint8_t*)pFileBase + RVAToFileOffset(pImpDesc[i].Name));
        strncpy(ImportsArray[dwImportCount].DLLName, pDLLName, sizeof(ImportsArray[dwImportCount].DLLName) - 1);
        
        // Process IAT
        uint64_t* pThunk = (uint64_t*)((uint8_t*)pFileBase + RVAToFileOffset(pImpDesc[i].FirstThunk));
        
        for (uint32_t j = 0; pThunk[j] != 0; j++) {
            if (dwImportCount >= MAX_IMPORTS) {
                break;
            }
            
            // Check if ordinal
            if (pThunk[j] & 0x8000000000000000) {
                ImportsArray[dwImportCount].IsOrdinal = 1;
                ImportsArray[dwImportCount].Ordinal = (uint16_t)(pThunk[j] & 0xFFFF);
            } else {
                // Import by name
                uint32_t* pNameRVA = (uint32_t*)((uint8_t*)pFileBase + RVAToFileOffset((uint32_t)pThunk[j]));
                char* pFuncName = (char*)((uint8_t*)pFileBase + RVAToFileOffset(pNameRVA[0]) + 2); // Skip hint
                strncpy(ImportsArray[dwImportCount].FunctionName, pFuncName, sizeof(ImportsArray[dwImportCount].FunctionName) - 1);
                ImportsArray[dwImportCount].Hint = (uint16_t)pNameRVA[0];
            }
            
            ImportsArray[dwImportCount].IAT_RVA = pImpDesc[i].FirstThunk + j * 8;
            dwImportCount++;
        }
    }
    
    return 1;
}

int GenerateHeaderFile(const char* lpModuleName) {
    char szFilePath[MAX_PATH];
    snprintf(szFilePath, sizeof(szFilePath), "%s\\include\\%s.h", szOutputPath, lpModuleName);
    
    // Create include directory if it doesn't exist
    char szIncludeDir[MAX_PATH];
    snprintf(szIncludeDir, sizeof(szIncludeDir), "%s\\include", szOutputPath);
    CreateDirectoryA(szIncludeDir, NULL);
    
    FILE* hFile = fopen(szFilePath, "w");
    if (!hFile) {
        return 0;
    }
    
    // Write header guard
    fprintf(hFile, "/*\n");
    fprintf(hFile, " * Auto-generated reconstruction\n");
    fprintf(hFile, " * Original: %s\n", lpModuleName);
    fprintf(hFile, " * Architecture: %s\n", bIs64Bit ? "x64" : "x86");
    fprintf(hFile, " * Timestamp: %08X\n", pNtHeaders->FileHeader.TimeDateStamp);
    fprintf(hFile, " */\n\n");
    
    fprintf(hFile, "#pragma once\n");
    fprintf(hFile, "#ifndef _RECONSTRUCTED_%s_H_\n", lpModuleName);
    fprintf(hFile, "#define _RECONSTRUCTED_%s_H_\n\n", lpModuleName);
    fprintf(hFile, "#include <windows.h>\n");
    fprintf(hFile, "#include <stdint.h>\n\n");
    fprintf(hFile, "#ifdef __cplusplus\n");
    fprintf(hFile, "extern \"C\" {\n");
    fprintf(hFile, "#endif\n\n");
    
    // Write exports
    for (uint32_t i = 0; i < dwExportCount; i++) {
        const char* pCallingConv = "__stdcall";
        switch (ExportsArray[i].CallingConv) {
            case 1: pCallingConv = "__cdecl"; break;
            case 2: pCallingConv = "__fastcall"; break;
            case 3: pCallingConv = "__thiscall"; break;
        }
        
        fprintf(hFile, "/* Export: %s */\n", ExportsArray[i].Name);
        fprintf(hFile, "/* Ordinal: %d, RVA: %08X */\n", ExportsArray[i].Ordinal, ExportsArray[i].RVA);
        fprintf(hFile, "%s void %s(void);\n\n", pCallingConv, ExportsArray[i].Name);
    }
    
    fprintf(hFile, "#ifdef __cplusplus\n");
    fprintf(hFile, "}\n");
    fprintf(hFile, "#endif\n\n");
    fprintf(hFile, "#endif /* _RECONSTRUCTED_%s_H_ */\n", lpModuleName);
    
    fclose(hFile);
    dwHeadersGenerated++;
    
    return 1;
}

int GenerateCMakeLists() {
    char szFilePath[MAX_PATH];
    snprintf(szFilePath, sizeof(szFilePath), "%s\\CMakeLists.txt", szOutputPath);
    
    FILE* hFile = fopen(szFilePath, "w");
    if (!hFile) {
        return 0;
    }
    
    fprintf(hFile, "cmake_minimum_required(VERSION 3.20)\n");
    fprintf(hFile, "project(%s VERSION 1.0.0 LANGUAGES C CXX)\n\n", szProjectName);
    fprintf(hFile, "set(CMAKE_C_STANDARD 11)\n");
    fprintf(hFile, "set(CMAKE_CXX_STANDARD 17)\n\n");
    fprintf(hFile, "if(MSVC)\n");
    fprintf(hFile, "    add_compile_options(/W4 /permissive-)\n");
    fprintf(hFile, "else()\n");
    fprintf(hFile, "    add_compile_options(-Wall -Wextra -Wpedantic)\n");
    fprintf(hFile, "endif()\n\n");
    fprintf(hFile, "include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)\n\n");
    
    // Sources
    fprintf(hFile, "set(SOURCES\n");
    for (uint32_t i = 0; i < dwExportCount; i++) {
        fprintf(hFile, "    src/%s.c\n", ExportsArray[i].Name);
    }
    fprintf(hFile, ")\n\n");
    
    // Target
    fprintf(hFile, "add_executable(${PROJECT_NAME} ${SOURCES})\n\n");
    fprintf(hFile, "target_link_libraries(${PROJECT_NAME} PRIVATE\n");
    
    // Libraries (from imports)
    for (uint32_t i = 0; i < dwImportCount; i++) {
        // Extract DLL name without extension
        char szLibName[256];
        strncpy(szLibName, ImportsArray[i].DLLName, sizeof(szLibName) - 1);
        char* pDot = strrchr(szLibName, '.');
        if (pDot) *pDot = '\0';
        
        fprintf(hFile, "    %s\n", szLibName);
    }
    fprintf(hFile, ")\n");
    
    fclose(hFile);
    
    return 1;
}

int ProcessPEFile(const char* lpFilePath) {
    char szFileName[MAX_PATH];
    
    // Get filename only
    const char* pFileName = strrchr(lpFilePath, '\\');
    if (!pFileName) {
        pFileName = strrchr(lpFilePath, '/');
    }
    if (!pFileName) {
        pFileName = lpFilePath;
    } else {
        pFileName++; // Skip the backslash
    }
    
    strncpy(szFileName, pFileName, sizeof(szFileName) - 1);
    
    printf("[*] Analyzing: %s\n", lpFilePath);
    
    // Map file
    if (!MapTargetFile(lpFilePath)) {
        printf("[-] Failed to map file\n");
        return 0;
    }
    
    // Parse headers
    if (!ParsePEHeaders()) {
        printf("[-] Failed to parse PE headers\n");
        UnmapTargetFile();
        return 0;
    }
    
    // Parse tables
    ParseExportTable();
    ParseImportTable();
    
    // Generate outputs
    GenerateHeaderFile(szFileName);
    GenerateCMakeLists();
    
    printf("[+] Analysis complete!\n");
    
    UnmapTargetFile();
    dwFilesProcessed++;
    
    return 1;
}

int ProcessDirectory(const char* lpPath) {
    char szSearch[MAX_PATH];
    snprintf(szSearch, sizeof(szSearch), "%s\\*", lpPath);
    
    WIN32_FIND_DATAA findData;
    HANDLE hFind = FindFirstFileA(szSearch, &findData);
    
    if (hFind == INVALID_HANDLE_VALUE) {
        return 0;
    }
    
    do {
        if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            continue;
        }
        
        // Check if PE file
        char* pExt = strrchr(findData.cFileName, '.');
        if (!pExt) continue;
        
        if (_stricmp(pExt, ".dll") == 0 || _stricmp(pExt, ".exe") == 0) {
            char szFullPath[MAX_PATH];
            snprintf(szFullPath, sizeof(szFullPath), "%s\\%s", lpPath, findData.cFileName);
            ProcessPEFile(szFullPath);
        }
    } while (FindNextFileA(hFind, &findData));
    
    FindClose(hFind);
    
    return 1;
}

int main() {
    PrintBanner();
    
    while (1) {
        PrintMenu();
        
        char choice[10];
        ReadLine(choice, sizeof(choice));
        
        switch (choice[0]) {
            case '1': {
                printf("Enter PE file path: ");
                ReadLine(szInputPath, sizeof(szInputPath));
                ProcessPEFile(szInputPath);
                break;
            }
            case '2': {
                printf("Enter input directory: ");
                ReadLine(szInputPath, sizeof(szInputPath));
                
                printf("Enter output directory: ");
                ReadLine(szOutputPath, sizeof(szOutputPath));
                CreateDirectoryA(szOutputPath, NULL);
                
                printf("Enter project name: ");
                ReadLine(szProjectName, sizeof(szProjectName));
                
                // Create include directory
                char szIncludeDir[MAX_PATH];
                snprintf(szIncludeDir, sizeof(szIncludeDir), "%s\\include", szOutputPath);
                CreateDirectoryA(szIncludeDir, NULL);
                
                ProcessDirectory(szInputPath);
                
                printf("\n[+] Batch processing complete!\n");
                printf("    Files processed: %d\n", dwFilesProcessed);
                printf("    Headers generated: %d\n", dwHeadersGenerated);
                printf("    Total exports: %d\n", dwExportCount);
                break;
            }
            case '3': {
                printf("Exiting...\n");
                return 0;
            }
            default: {
                printf("Invalid choice. Please try again.\n");
                break;
            }
        }
        
        printf("\n");
    }
    
    return 0;
}
