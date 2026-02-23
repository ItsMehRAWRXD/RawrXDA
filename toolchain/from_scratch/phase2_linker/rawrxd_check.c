/*
 * rawrxd_check.c - PE Validator (diagnoses "invalid application" errors)
 * Compile standalone: gcc rawrxd_check.c -o rawrxd_check.exe
 * Usage: rawrxd_check.exe <file.exe>
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#pragma pack(push, 1)
typedef struct {
    uint16_t e_magic;
    uint8_t  pad[58];
    uint32_t e_lfanew;
} DOS_HDR;

typedef struct {
    uint16_t Machine;
    uint16_t NumberOfSections;
    uint32_t TimeDateStamp;
    uint32_t PointerToSymbolTable;
    uint32_t NumberOfSymbols;
    uint16_t SizeOfOptionalHeader;
    uint16_t Characteristics;
} COFF_HDR;

typedef struct {
    uint32_t VirtualAddress;
    uint32_t Size;
} DATA_DIR;

typedef struct {
    uint16_t Magic;
    uint8_t  MajorLinker, MinorLinker;
    uint32_t SizeOfCode;
    uint32_t SizeOfInitializedData;
    uint32_t SizeOfUninitializedData;
    uint32_t AddressOfEntryPoint;
    uint32_t BaseOfCode;
    uint64_t ImageBase;
    uint32_t SectionAlignment;
    uint32_t FileAlignment;
    uint16_t MajorOS, MinorOS;
    uint16_t MajorImage, MinorImage;
    uint16_t MajorSubsys, MinorSubsys;
    uint32_t Win32Version;
    uint32_t SizeOfImage;
    uint32_t SizeOfHeaders;
    uint32_t CheckSum;
    uint16_t Subsystem;
    uint16_t DllCharacteristics;
    uint64_t SizeOfStackReserve;
    uint64_t SizeOfStackCommit;
    uint64_t SizeOfHeapReserve;
    uint64_t SizeOfHeapCommit;
    uint32_t LoaderFlags;
    uint32_t NumberOfRvaAndSizes;
    DATA_DIR DataDirectory[16];
} OPT_HDR64;

typedef struct {
    char Name[8];
    uint32_t VirtualSize;
    uint32_t VirtualAddress;
    uint32_t SizeOfRawData;
    uint32_t PointerToRawData;
    uint32_t PointerToRelocations;
    uint32_t PointerToLinenumbers;
    uint16_t NumberOfRelocations;
    uint16_t NumberOfLinenumbers;
    uint32_t Characteristics;
} SEC_HDR;
#pragma pack(pop)

static void check_imports(FILE* f, uint32_t idata_rva, uint32_t idata_raw) {
    printf("\n=== Import Table Check ===\n");

    fseek(f, idata_raw, SEEK_SET);

    uint32_t ilt_rva, timestamp, forwarder, name_rva, iat_rva;
    fread(&ilt_rva, 4, 1, f);
    fread(&timestamp, 4, 1, f);
    fread(&forwarder, 4, 1, f);
    fread(&name_rva, 4, 1, f);
    fread(&iat_rva, 4, 1, f);

    printf("IDT Entry:\n");
    printf("  ILT RVA:  0x%08X (expected 0x%08X)\n", ilt_rva, idata_rva + 40);
    printf("  Name RVA: 0x%08X (expected 0x%08X)\n", name_rva, idata_rva + 88);
    printf("  IAT RVA:  0x%08X (expected 0x%08X)\n", iat_rva, idata_rva + 56);

    if (ilt_rva != 0) {
        uint32_t hint_name_rva = idata_rva + 72;
        uint32_t ilt_offset = idata_raw + 40;
        fseek(f, ilt_offset, SEEK_SET);
        uint64_t ilt_entry;
        fread(&ilt_entry, 8, 1, f);
        printf("  ILT[0]:   0x%016llX (expected hint/name RVA 0x%08X)\n",
               (unsigned long long)ilt_entry, hint_name_rva);
    }

    {
        uint32_t iat_offset = idata_raw + 56;
        fseek(f, iat_offset, SEEK_SET);
        uint64_t iat_entry;
        fread(&iat_entry, 8, 1, f);
        printf("  IAT[0]:   0x%016llX (pre-load; loader fills)\n", (unsigned long long)iat_entry);
    }

    {
        uint32_t hint_offset = idata_raw + 72;
        fseek(f, hint_offset, SEEK_SET);
        uint16_t hint;
        fread(&hint, 2, 1, f);
        char name[64];
        size_t n = fread(name, 1, 63, f);
        name[n] = '\0';
        printf("  Hint/Name: Hint=%u, Name='%s'\n", (unsigned)hint, name);
    }

    {
        uint32_t dll_offset = idata_raw + 88;
        fseek(f, dll_offset, SEEK_SET);
        char dll_name[64];
        size_t n = fread(dll_name, 1, 63, f);
        dll_name[n] = '\0';
        printf("  DLL Name: '%s'\n", dll_name);
    }
}

int check_pe(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) { printf("FAIL: Cannot open file\n"); return 1; }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    printf("File size: %ld bytes\n", size);

    /* DOS Header */
    DOS_HDR dos;
    fread(&dos, sizeof(dos), 1, f);
    if (dos.e_magic != 0x5A4D) {
        printf("FAIL: DOS magic != MZ (got 0x%04X)\n", dos.e_magic);
        return 1;
    }
    printf("[OK] DOS Magic: MZ\n");
    printf("[OK] PE offset: 0x%X\n", dos.e_lfanew);

    if ((long)dos.e_lfanew > size - 4) {
        printf("FAIL: PE offset beyond file\n");
        return 1;
    }

    /* PE Signature */
    fseek(f, dos.e_lfanew, SEEK_SET);
    uint32_t sig;
    fread(&sig, 4, 1, f);
    if (sig != 0x00004550) {
        printf("FAIL: PE signature invalid (got 0x%08X)\n", sig);
        return 1;
    }
    printf("[OK] PE Signature: PE\\0\\0\n");

    /* COFF */
    COFF_HDR coff;
    fread(&coff, sizeof(coff), 1, f);
    printf("\nCOFF Header:\n");
    printf("  Machine: 0x%04X (%s)\n", coff.Machine,
           coff.Machine == 0x8664 ? "AMD64" :
           coff.Machine == 0x14C ? "I386" : "UNKNOWN");
    if (coff.Machine != 0x8664) printf("  WARN: Expected 0x8664 for x64\n");

    printf("  Sections: %d\n", coff.NumberOfSections);
    if (coff.NumberOfSections == 0) printf("  FAIL: No sections!\n");

    printf("  Opt Header Size: %d\n", coff.SizeOfOptionalHeader);
    if (coff.SizeOfOptionalHeader != 240)
        printf("  WARN: Expected 240 for PE32+\n");

    /* Optional Header */
    long opt_pos = ftell(f);
    uint16_t magic;
    fread(&magic, 2, 1, f);
    fseek(f, opt_pos, SEEK_SET);

    if (magic != 0x20B) {
        printf("\nFAIL: Optional header magic != 0x20B (PE32+) (got 0x%03X)\n", magic);
        return 1;
    }
    printf("\n[OK] Optional Header Magic: PE32+ (0x20B)\n");

    OPT_HDR64 opt;
    fread(&opt, sizeof(opt), 1, f);

    printf("  Entry Point RVA: 0x%08X\n", opt.AddressOfEntryPoint);
    if (opt.AddressOfEntryPoint == 0) printf("  FAIL: Entry point is 0\n");
    printf("  Image Base: 0x%llX\n", (unsigned long long)opt.ImageBase);
    if (opt.ImageBase != 0x140000000ULL)
        printf("  WARN: Non-standard image base (expected 0x140000000)\n");

    printf("  Section Align: 0x%X\n", opt.SectionAlignment);
    printf("  File Align: 0x%X\n", opt.FileAlignment);
    if (opt.SectionAlignment < opt.FileAlignment)
        printf("  FAIL: SectionAlignment < FileAlignment\n");

    printf("  Subsystem: %d (%s)\n", opt.Subsystem,
           opt.Subsystem == 3 ? "CONSOLE" :
           opt.Subsystem == 2 ? "GUI" :
           opt.Subsystem == 1 ? "NATIVE" : "UNKNOWN");
    if (opt.Subsystem == 0) printf("  FAIL: Subsystem is 0\n");
    printf("  Subsystem Version: %u.%u\n", opt.MajorSubsys, opt.MinorSubsys);
    if (opt.MajorSubsys < 5) printf("  WARN: Subsystem version should be >= 5.1 for Win64\n");

    printf("  SizeOfImage: 0x%X\n", opt.SizeOfImage);
    printf("  SizeOfHeaders: 0x%X\n", opt.SizeOfHeaders);
    printf("  SizeOfStackReserve: 0x%llX  SizeOfStackCommit: 0x%llX\n",
           (unsigned long long)opt.SizeOfStackReserve, (unsigned long long)opt.SizeOfStackCommit);
    if (opt.SizeOfStackReserve == 0)
        printf("  FAIL: SizeOfStackReserve is 0 (causes 0xC00000FD stack overflow)\n");

    /* Data Directories */
    printf("\nData Directories:\n");
    const char* names[] = {"Export","Import","Resource","Exception","Cert","Reloc","Debug",
                          "Arch","GlobalPtr","TLS","Config","BoundImport","IAT",
                          "DelayImport","CLR","Reserved"};
    for (int i = 0; i < 16; i++) {
        if (opt.DataDirectory[i].VirtualAddress != 0) {
            printf("  [%d] %s: RVA 0x%08X, Size 0x%X\n", i, names[i],
                   opt.DataDirectory[i].VirtualAddress, opt.DataDirectory[i].Size);
        }
    }

    /* Sections */
    printf("\nSections:\n");
    fseek(f, opt_pos + coff.SizeOfOptionalHeader, SEEK_SET);
    uint32_t idata_rva = 0, idata_raw = 0;
    for (int i = 0; i < coff.NumberOfSections; i++) {
        SEC_HDR sec;
        fread(&sec, sizeof(sec), 1, f);
        char name[9];
        memcpy(name, sec.Name, 8);
        name[8] = 0;
        printf("  [%d] %-8s RVA 0x%08X Size 0x%08X Raw 0x%08X Attr 0x%08X\n",
               i, name, sec.VirtualAddress, sec.VirtualSize,
               sec.PointerToRawData, sec.Characteristics);

        if (opt.AddressOfEntryPoint >= sec.VirtualAddress &&
            opt.AddressOfEntryPoint < sec.VirtualAddress + sec.VirtualSize) {
            printf("       ^ Entry point is in this section\n");
        }
        if (strncmp(sec.Name, ".idata", 6) == 0) {
            idata_rva = sec.VirtualAddress;
            idata_raw = sec.PointerToRawData;
        }
    }

    if (opt.DataDirectory[1].VirtualAddress != 0 && idata_rva != 0) {
        check_imports(f, idata_rva, idata_raw);
    }

    printf("  COFF Characteristics: 0x%04X (0x22 = EXECUTABLE|LARGE_ADDRESS_AWARE)\n", coff.Characteristics);
    printf("  DllCharacteristics: 0x%04X\n", opt.DllCharacteristics);

    fclose(f);
    printf("\n[VALIDATION COMPLETE]\n");
    return 0;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        printf("RawrXD PE Validator - Diagnoses 'invalid application' errors\n");
        printf("Usage: %s <file.exe>\n", argv[0]);
        return 1;
    }
    return check_pe(argv[1]);
}
