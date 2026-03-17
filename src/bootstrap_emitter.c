#include <windows.h>
#include <stdio.h>
#include <stdint.h>

// ----------------------------------------------------------------------------
// RAW PE EMITTER (C Fallback for Self-Hosting Bootstrap)
// ----------------------------------------------------------------------------
// This tool mimics the logic of BareMetal_PE_Writer.asm but ensures
// compatibility with the available GCC toolchain.
// ----------------------------------------------------------------------------

#pragma pack(push, 1)
typedef struct {
    uint16_t MZSignature;
    uint16_t UsedBytesInLastPage;
    uint16_t FilePages;
    uint16_t NumberOfRelocationItems;
    uint16_t HeaderSizeInParagraphs;
    uint16_t MinimumExtraParagraphs;
    uint16_t MaximumExtraParagraphs;
    uint16_t InitialRelativeSS;
    uint16_t InitialSP;
    uint16_t Checksum;
    uint16_t InitialIP;
    uint16_t InitialRelativeCS;
    uint16_t AddressOfRelocationTable;
    uint16_t OverlayNumber;
    uint16_t Reserved[4];
    uint16_t OEMid;
    uint16_t OEMinfo;
    uint16_t Reserved2[10];
    uint32_t AddressOfNewExeHeader;
} IMAGE_DOS_HEADER_RAW;

typedef struct {
    uint32_t Signature;
    IMAGE_FILE_HEADER FileHeader;
    IMAGE_OPTIONAL_HEADER64 OptionalHeader;
} IMAGE_NT_HEADERS64_RAW;
#pragma pack(pop)

int main() {
    const char* filename = "RawrXD_Bootstrap_Core.exe";
    printf("[Bootstrap] Emitting %s...\n", filename);

    FILE* f = fopen(filename, "wb");
    if (!f) return 1;

    // 1. DOS Header
    IMAGE_DOS_HEADER_RAW dos = {0};
    dos.MZSignature = 0x5A4D; // 'MZ'
    dos.AddressOfNewExeHeader = 0x80;
    fwrite(&dos, sizeof(dos), 1, f);

    // Padding to 0x80
    uint8_t pad[0x80 - sizeof(dos)] = {0};
    fwrite(pad, sizeof(pad), 1, f);

    // 2. PE Signature
    uint32_t peSig = 0x00004550; // 'PE\0\0'
    fwrite(&peSig, sizeof(peSig), 1, f);

    // 3. File Header
    IMAGE_FILE_HEADER fileHeader = {0};
    fileHeader.Machine = IMAGE_FILE_MACHINE_AMD64;
    fileHeader.NumberOfSections = 1;
    fileHeader.SizeOfOptionalHeader = sizeof(IMAGE_OPTIONAL_HEADER64);
    fileHeader.Characteristics = IMAGE_FILE_EXECUTABLE_IMAGE | IMAGE_FILE_LARGE_ADDRESS_AWARE;
    fwrite(&fileHeader, sizeof(fileHeader), 1, f);

    // 4. Optional Header
    IMAGE_OPTIONAL_HEADER64 opt = {0};
    opt.Magic = IMAGE_NT_OPTIONAL_HDR64_MAGIC;
    opt.AddressOfEntryPoint = 0x1000;
    opt.ImageBase = 0x140000000;
    opt.SectionAlignment = 0x1000;
    opt.FileAlignment = 0x200;
    opt.MajorOperatingSystemVersion = 6;
    opt.MinorOperatingSystemVersion = 0;
    opt.MajorSubsystemVersion = 6;
    opt.MinorSubsystemVersion = 0;
    opt.SizeOfImage = 0x3000;
    opt.SizeOfHeaders = 0x200;
    opt.Subsystem = IMAGE_SUBSYSTEM_WINDOWS_CUI;
    opt.NumberOfRvaAndSizes = IMAGE_NUMBEROF_DIRECTORY_ENTRIES;
    fwrite(&opt, sizeof(opt), 1, f);

    // 5. Section Header (.text)
    IMAGE_SECTION_HEADER text = {0};
    memcpy(text.Name, ".text", 5);
    text.Misc.VirtualSize = 0x1000;
    text.VirtualAddress = 0x1000;
    text.SizeOfRawData = 0x200;
    text.PointerToRawData = 0x200;
    text.Characteristics = IMAGE_SCN_CNT_CODE | IMAGE_SCN_MEM_EXECUTE | IMAGE_SCN_MEM_READ;
    fwrite(&text, sizeof(text), 1, f);

    // Padding to 0x200
    uint8_t sectionPad[0x200 - (0x80 + 4 + sizeof(fileHeader) + sizeof(opt) + sizeof(text))] = {0};
    fwrite(sectionPad, sizeof(sectionPad), 1, f);

    // 6. Machine Code (x64)
    // mov eax, 42
    // ret
    uint8_t code[0x200] = {0};
    code[0] = 0xB8; code[1] = 0x2A; code[2] = 0x00; code[3] = 0x00; code[4] = 0x00; // mov eax, 42
    code[5] = 0xC3; // ret
    fwrite(code, sizeof(code), 1, f);

    fclose(f);
    printf("[Bootstrap] DONE. Run %s to verify (expect exit code 42).\n", filename);
    return 0;
}
