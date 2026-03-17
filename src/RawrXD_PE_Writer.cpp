// ============================================================================
// RawrXD_PE_Writer.cpp — Complete monolithic PE32+ writer + machine code emitter
//
// Reverse-engineered from x64 MASM to simplest least-dep C++ backend designer.
// Generates runnable executables with import tables.
//
// Usage:
//   PEWriter writer;
//   writer.AddSection(".text", code, codeSize, IMAGE_SCN_MEM_EXECUTE | IMAGE_SCN_MEM_READ);
//   writer.AddImport("kernel32.dll", "ExitProcess");
//   writer.AddImport("user32.dll", "MessageBoxA");
//   writer.EmitExecutable("output.exe");
// ============================================================================

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <vector>
#include <map>
#include <string>
#include <algorithm>

// ── PE Constants (reverse-engineered from MASM includes) ──
#define IMAGE_FILE_MACHINE_AMD64             0x8664
#define IMAGE_NT_OPTIONAL_HDR64_MAGIC        0x20b
#define IMAGE_SUBSYSTEM_WINDOWS_GUI          2
#define IMAGE_SUBSYSTEM_WINDOWS_CUI          3
#define IMAGE_SCN_MEM_EXECUTE                0x20000000
#define IMAGE_SCN_MEM_READ                   0x40000000
#define IMAGE_SCN_MEM_WRITE                  0x80000000
#define IMAGE_SCN_CNT_CODE                   0x00000020
#define IMAGE_SCN_CNT_INITIALIZED_DATA       0x00000040
#define IMAGE_DIRECTORY_ENTRY_IMPORT         1
#define IMAGE_DIRECTORY_ENTRY_BASERELOC      5
#define IMAGE_DIRECTORY_ENTRY_IAT            12
#define IMAGE_REL_BASED_DIR64               10

// ── DOS Header and Stub Generation ──

// Generate a proper DOS stub that displays an error message and exits
std::vector<BYTE> GenerateDOSStub() {
    std::vector<BYTE> stub;

    // DOS executable header (MZ header is handled separately)
    // This stub will print "This program cannot be run in DOS mode." and exit

    // mov dx, msg
    stub.push_back(0xBA); // mov dx, imm16
    stub.push_back(0x0E); // offset of message (after header)
    stub.push_back(0x01);

    // mov ah, 09h (DOS print string)
    stub.push_back(0xB4);
    stub.push_back(0x09);

    // int 21h
    stub.push_back(0xCD);
    stub.push_back(0x21);

    // mov ax, 4C01h (exit with code 1)
    stub.push_back(0xB8);
    stub.push_back(0x01);
    stub.push_back(0x4C);

    // int 21h
    stub.push_back(0xCD);
    stub.push_back(0x21);

    // Message: "This program cannot be run in DOS mode.\r\n$"
    const char* message = "This program cannot be run in DOS mode.\r\n$";
    for (size_t i = 0; message[i]; ++i) {
        stub.push_back((BYTE)message[i]);
    }

    // Pad to minimum DOS stub size (0x40 bytes after MZ header)
    while (stub.size() < 0x3C) {
        stub.push_back(0x00);
    }

    return stub;
}

// ── Machine Code Emitter (reverse-engineered from x64 MASM) ──
class MachineCodeEmitter {
public:
    std::vector<BYTE> code;

    void Emit(BYTE b) { code.push_back(b); }
    void Emit(WORD w) { 
        code.push_back((BYTE)(w & 0xFF)); 
        code.push_back((BYTE)((w >> 8) & 0xFF)); 
    }
    void Emit(DWORD d) { 
        Emit((WORD)d); 
        Emit((WORD)(d >> 16)); 
    }
    void Emit(ULONGLONG q) { 
        Emit((DWORD)q); 
        Emit((DWORD)(q >> 32)); 
    }

    // x64 instruction emitters (reverse-engineered from MASM)
    void RET() { Emit((BYTE)0xC3); }
    void MOV_RAX_IMM(ULONGLONG imm) { 
        Emit((BYTE)0x48); Emit((BYTE)0xB8); Emit(imm); 
    }
    void CALL_RAX() { Emit((BYTE)0xFF); Emit((BYTE)0xD0); }
    // CALL RCX
    void CALL_RCX() { Emit((BYTE)0xFF); Emit((BYTE)0xD1); }
    // CALL RDX
    void CALL_RDX() { Emit((BYTE)0xFF); Emit((BYTE)0xD2); }
    // CALL RBX
    void CALL_RBX() { Emit((BYTE)0xFF); Emit((BYTE)0xD3); }
    void SUB_RSP_IMM8(BYTE imm) { 
        Emit((BYTE)0x48); Emit((BYTE)0x83); Emit((BYTE)0xEC); Emit(imm); 
    }
    void ADD_RSP_IMM8(BYTE imm) { 
        Emit((BYTE)0x48); Emit((BYTE)0x83); Emit((BYTE)0xC4); Emit(imm); 
    }
    void MOV_RCX_IMM(ULONGLONG imm) { 
        Emit((BYTE)0x48); Emit((BYTE)0xB9); Emit(imm); 
    }
    void MOV_RDX_IMM(ULONGLONG imm) { 
        Emit((BYTE)0x48); Emit((BYTE)0xBA); Emit(imm); 
    }
    void MOV_R8_IMM(ULONGLONG imm) { 
        Emit((BYTE)0x49); Emit((BYTE)0xB8); Emit(imm); 
    }
    void MOV_R9_IMM(ULONGLONG imm) { 
        Emit((BYTE)0x49); Emit((BYTE)0xB9); Emit(imm); 
    }

    // PUSH RBP
    void PUSH_RBP() {
        Emit((BYTE)0x55);
    }
    // POP RBP
    void POP_RBP() {
        Emit((BYTE)0x5D);
    }
    // MOV RBP, RSP
    void MOV_RBP_RSP() {
        Emit((BYTE)0x48); Emit((BYTE)0x89); Emit((BYTE)0xE5);
    }
    // MOV RSP, RBP
    void MOV_RSP_RBP() {
        Emit((BYTE)0x48); Emit((BYTE)0x89); Emit((BYTE)0xEC);
    }
    // SUB RSP, imm32 (for frame sizes > 127)
    void SUB_RSP_IMM32(DWORD imm) {
        Emit((BYTE)0x48); Emit((BYTE)0x81); Emit((BYTE)0xEC); Emit(imm);
    }
    // ADD RSP, imm32 (for frame sizes > 127)
    void ADD_RSP_IMM32(DWORD imm) {
        Emit((BYTE)0x48); Emit((BYTE)0x81); Emit((BYTE)0xC4); Emit(imm);
    }

    // ── ALU operations ──
    // XOR RAX, RAX (zero register)
    void XOR_RAX_RAX() {
        Emit((BYTE)0x48); Emit((BYTE)0x31); Emit((BYTE)0xC0);
    }
    // XOR RCX, RCX
    void XOR_RCX_RCX() {
        Emit((BYTE)0x48); Emit((BYTE)0x31); Emit((BYTE)0xC9);
    }
    // TEST RAX, RAX (set flags based on RAX & RAX)
    void TEST_RAX_RAX() {
        Emit((BYTE)0x48); Emit((BYTE)0x85); Emit((BYTE)0xC0);
    }
    // TEST RCX, RCX
    void TEST_RCX_RCX() {
        Emit((BYTE)0x48); Emit((BYTE)0x85); Emit((BYTE)0xC9);
    }
    // AND RAX, imm32
    void AND_RAX_IMM32(DWORD imm) {
        Emit((BYTE)0x48); Emit((BYTE)0x25); Emit(imm);
    }
    // OR RAX, RCX
    void OR_RAX_RCX() {
        Emit((BYTE)0x48); Emit((BYTE)0x09); Emit((BYTE)0xC8);
    }
    // SHL RAX, CL (shift left)
    void SHL_RAX_CL() {
        Emit((BYTE)0x48); Emit((BYTE)0xD3); Emit((BYTE)0xE0);
    }
    // SHR RAX, CL (shift right logical)
    void SHR_RAX_CL() {
        Emit((BYTE)0x48); Emit((BYTE)0xD3); Emit((BYTE)0xE8);
    }
    // SAR RAX, CL (shift right arithmetic)
    void SAR_RAX_CL() {
        Emit((BYTE)0x48); Emit((BYTE)0xD3); Emit((BYTE)0xF8);
    }
    // CMP RCX, imm32
    void CMP_RCX_IMM32(DWORD imm) {
        Emit((BYTE)0x48); Emit((BYTE)0x81); Emit((BYTE)0xF9); Emit(imm);
    }

    // ── PUSH/POP operations for all GP registers ──
    void PUSH_RAX() { Emit((BYTE)0x50); }
    void PUSH_RCX() { Emit((BYTE)0x51); }
    void PUSH_RDX() { Emit((BYTE)0x52); }
    void PUSH_RBX() { Emit((BYTE)0x53); }
    void PUSH_RSP() { Emit((BYTE)0x54); }
    void PUSH_RSI() { Emit((BYTE)0x56); }
    void PUSH_RDI() { Emit((BYTE)0x57); }
    void PUSH_R8()  { Emit((BYTE)0x41); Emit((BYTE)0x50); }
    void PUSH_R9()  { Emit((BYTE)0x41); Emit((BYTE)0x51); }
    void PUSH_R10() { Emit((BYTE)0x41); Emit((BYTE)0x52); }
    void PUSH_R11() { Emit((BYTE)0x41); Emit((BYTE)0x53); }

    void POP_RAX() { Emit((BYTE)0x58); }
    void POP_RCX() { Emit((BYTE)0x59); }
    void POP_RDX() { Emit((BYTE)0x5A); }
    void POP_RBX() { Emit((BYTE)0x5B); }
    void POP_RSP() { Emit((BYTE)0x5C); }
    void POP_RSI() { Emit((BYTE)0x5E); }
    void POP_RDI() { Emit((BYTE)0x5F); }
    void POP_R8()  { Emit((BYTE)0x41); Emit((BYTE)0x58); }
    void POP_R9()  { Emit((BYTE)0x41); Emit((BYTE)0x59); }
    void POP_R10() { Emit((BYTE)0x41); Emit((BYTE)0x5A); }
    void POP_R11() { Emit((BYTE)0x41); Emit((BYTE)0x5B); }

    // ── Control flow: conditional jumps and LEA ──
    // JMP rel32 (unconditional near jump)
    void JMP_REL32(DWORD targetRVA) {
        callFixups.push_back({(DWORD)code.size() + 1, targetRVA});
        Emit((BYTE)0xE9);  // JMP rel32
        Emit((DWORD)0);    // Placeholder
    }
    // JE rel32 (jump if equal / zero flag set)
    void JE_REL32(DWORD targetRVA) {
        callFixups.push_back({(DWORD)code.size() + 2, targetRVA});
        Emit((BYTE)0x0F); Emit((BYTE)0x84);  // JE rel32
        Emit((DWORD)0);
    }
    // JNE rel32 (jump if not equal / zero flag clear)
    void JNE_REL32(DWORD targetRVA) {
        callFixups.push_back({(DWORD)code.size() + 2, targetRVA});
        Emit((BYTE)0x0F); Emit((BYTE)0x85);  // JNE rel32
        Emit((DWORD)0);
    }
    // JG rel32 (jump if greater / ZF=0 and SF=OF)
    void JG_REL32(DWORD targetRVA) {
        callFixups.push_back({(DWORD)code.size() + 2, targetRVA});
        Emit((BYTE)0x0F); Emit((BYTE)0x8F);  // JG rel32
        Emit((DWORD)0);
    }
    // JL rel32 (jump if less / SF≠OF)
    void JL_REL32(DWORD targetRVA) {
        callFixups.push_back({(DWORD)code.size() + 2, targetRVA});
        Emit((BYTE)0x0F); Emit((BYTE)0x8C);  // JL rel32
        Emit((DWORD)0);
    }
    // JGE rel32 (jump if greater or equal / SF=OF)
    void JGE_REL32(DWORD targetRVA) {
        callFixups.push_back({(DWORD)code.size() + 2, targetRVA});
        Emit((BYTE)0x0F); Emit((BYTE)0x8D);  // JGE rel32
        Emit((DWORD)0);
    }
    // JLE rel32 (jump if less or equal / ZF=1 or SF≠OF)
    void JLE_REL32(DWORD targetRVA) {
        callFixups.push_back({(DWORD)code.size() + 2, targetRVA});
        Emit((BYTE)0x0F); Emit((BYTE)0x8E);  // JLE rel32
        Emit((DWORD)0);
    }
    // LEA RCX, [RIP+disp32] — load effective address (position-independent addressing)
    void LEA_RCX_RIP_DISP32(DWORD disp32) {
        Emit((BYTE)0x48); Emit((BYTE)0x8D); Emit((BYTE)0x0D); Emit(disp32);
    }
    // LEA RAX, [RIP+disp32]
    void LEA_RAX_RIP_DISP32(DWORD disp32) {
        Emit((BYTE)0x48); Emit((BYTE)0x8D); Emit((BYTE)0x05); Emit(disp32);
    }
    // LEA RDX, [RIP+disp32]
    void LEA_RDX_RIP_DISP32(DWORD disp32) {
        Emit((BYTE)0x48); Emit((BYTE)0x8D); Emit((BYTE)0x15); Emit(disp32);
    }
    // LEA RBX, [RIP+disp32]
    void LEA_RBX_RIP_DISP32(DWORD disp32) {
        Emit((BYTE)0x48); Emit((BYTE)0x8D); Emit((BYTE)0x1D); Emit(disp32);
    }

    // ── Register-to-register MOV operations ──
    // MOV RAX, RCX
    void MOV_RAX_RCX() {
        Emit((BYTE)0x48); Emit((BYTE)0x89); Emit((BYTE)0xC8);
    }
    // MOV RCX, RAX
    void MOV_RCX_RAX() {
        Emit((BYTE)0x48); Emit((BYTE)0x89); Emit((BYTE)0xC1);
    }
    // MOV RDX, RAX
    void MOV_RDX_RAX() {
        Emit((BYTE)0x48); Emit((BYTE)0x89); Emit((BYTE)0xC2);
    }
    // MOV RAX, RDX
    void MOV_RAX_RDX() {
        Emit((BYTE)0x48); Emit((BYTE)0x89); Emit((BYTE)0xD0);
    }

    // Function prologue/epilogue — parameterized frame size
    // localBytes = space for local variables (0 for leaf functions)
    // numArgs    = max arguments passed to any callee (minimum 4 for shadow space)
    // useFramePointer = emit PUSH RBP / MOV RBP, RSP for debuggable frames
    void FunctionPrologue(DWORD localBytes = 0, DWORD numArgs = 4, bool useFramePointer = false) {
        if (useFramePointer) {
            PUSH_RBP();
            MOV_RBP_RSP();
        }
        // Frame = max(4,numArgs)*8 for shadow/args + localBytes, aligned to 16
        DWORD shadowSpace = (numArgs < 4 ? 4 : numArgs) * 8;
        DWORD totalFrame = shadowSpace + localBytes;
        // x64 ABI: RSP must be 16-byte aligned BEFORE calling.
        // On entry RSP is 8-mod-16 (return addr). After PUSH RBP (if used), RSP is 0-mod-16.
        // Without frame pointer, RSP is 8-mod-16. We need (RSP - totalFrame) % 16 == 0.
        if (!useFramePointer) {
            totalFrame = (totalFrame + 15) & ~15;  // round up
            if (totalFrame % 16 == 0) totalFrame += 8;  // ensure 8-mod-16 after sub
        } else {
            totalFrame = (totalFrame + 15) & ~15;  // RSP already 0-mod-16 after push rbp
        }
        lastFrameSize = totalFrame;
        if (totalFrame <= 127) {
            SUB_RSP_IMM8((BYTE)totalFrame);
        } else {
            SUB_RSP_IMM32(totalFrame);
        }
    }

    void FunctionEpilogue(bool useFramePointer = false) {
        if (useFramePointer) {
            MOV_RSP_RBP();
            POP_RBP();
        } else {
            if (lastFrameSize <= 127) {
                ADD_RSP_IMM8((BYTE)lastFrameSize);
            } else {
                ADD_RSP_IMM32(lastFrameSize);
            }
        }
        RET();
    }

    // Call imported function by index — FF 15 [rip+disp32] indirect call through IAT
    // This is the correct x64 encoding: the IAT slot holds the real function pointer,
    // and FF 15 dereferences it. E8 rel32 would jump INTO the IAT data, which is wrong.
    void CALL_IMPORT(size_t importIndex) {
        importCalls.push_back({(DWORD)code.size() + 2, importIndex});  // +2 for FF 15 opcode bytes
        Emit((BYTE)0xFF);  // opcode: CALL r/m64
        Emit((BYTE)0x15);  // ModR/M: [RIP + disp32]
        Emit((DWORD)0);    // disp32 placeholder — will be fixed to RIP-relative offset to IAT slot
    }

    // Fix up CALL_REL32 calls with actual RVAs
    void FixupCalls(DWORD currentRVA) {
        for (const auto& fixup : callFixups) {
            DWORD callRVA = currentRVA + fixup.offset;
            DWORD rel32 = fixup.targetRVA - (callRVA + 4);  // +4 because rel32 is from end of instruction
            // Write the rel32 value at the fixup location
            *(DWORD*)&code[fixup.offset] = rel32;
        }
    }

    // Fix up import calls with IAT RVAs (FF 15 [rip+disp32] encoding)
    // disp32 = targetRVA - (instructionRVA + 4), where +4 accounts for the disp32 field itself
    void FixupImportCalls(DWORD currentRVA, const std::vector<DWORD>& iatRVAs) {
        for (const auto& call : importCalls) {
            DWORD disp32Offset = currentRVA + call.offset;  // RVA of the disp32 field
            DWORD targetRVA = iatRVAs[call.importIndex];     // RVA of the IAT slot
            DWORD rel32 = targetRVA - (disp32Offset + 4);   // RIP-relative: from end of instruction
            *(DWORD*)&code[call.offset] = rel32;
        }
    }

    size_t Size() const { return code.size(); }
    const BYTE* Data() const { return code.data(); }

private:
    struct CallFixup {
        DWORD offset;  // Offset in code where rel32 should be written
        DWORD targetRVA;  // Target RVA to call
    };
    struct ImportCall {
        DWORD offset;  // Offset in code where rel32 should be written
        size_t importIndex;  // Index of import in global import list
    };
    DWORD lastFrameSize = 0x28;  // Tracks last prologue frame size for epilogue
    std::vector<CallFixup> callFixups;
    std::vector<ImportCall> importCalls;
};

// ── PE Writer Class ──
class PEWriter {
private:
    WORD subsystemType;

public:
    PEWriter() { subsystemType = IMAGE_SUBSYSTEM_WINDOWS_CUI; }
    struct Section {
        std::string name;
        std::vector<BYTE> data;
        DWORD characteristics;
        DWORD virtualAddress;
        DWORD rawAddress;
        MachineCodeEmitter* emitter;  // Store emitter for fixup
    };

    struct Import {
        std::string dllName;
        std::vector<std::string> functions;
    };

    std::vector<Section> sections;
    std::vector<Import> imports;
    DWORD entryPointRVA = 0;
    ULONGLONG imageBase = 0x140000000;  // Standard x64 image base
    DWORD sectionAlignment = 0x1000;
    DWORD fileAlignment = 0x200;

public:
    void AddSection(const char* name, const BYTE* data, size_t size, DWORD characteristics, MachineCodeEmitter* emitter = nullptr) {
        Section sec;
        sec.name = name;
        sec.data.assign(data, data + size);
        sec.characteristics = characteristics;
        sec.emitter = emitter;
        sections.push_back(sec);
    }

    void AddImport(const char* dllName, const char* functionName) {
        // Find or create import for this DLL
        for (auto& imp : imports) {
            if (_stricmp(imp.dllName.c_str(), dllName) == 0) {
                imp.functions.push_back(functionName);
                return;
            }
        }
        Import imp;
        imp.dllName = dllName;
        imp.functions.push_back(functionName);
        imports.push_back(imp);
    }

    void SetEntryPoint(DWORD rva) { entryPointRVA = rva; }
    void SetSubsystem(WORD subsystem) { subsystemType = subsystem; }

    bool EmitExecutable(const char* filename) {
        if (sections.empty()) {
            return false;
        }

        bool autoEntryPoint = (entryPointRVA == 0);

        bool hasImports = !imports.empty();
        if (hasImports) {
            Section idataSec;
            idataSec.name = ".idata";
            idataSec.characteristics = IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_WRITE | IMAGE_SCN_CNT_INITIALIZED_DATA;
            idataSec.emitter = nullptr;
            sections.push_back(idataSec);
        }

        // Add .reloc section for ASLR support (DYNAMIC_BASE)
        Section relocSec;
        relocSec.name = ".reloc";
        relocSec.characteristics = IMAGE_SCN_MEM_READ | IMAGE_SCN_CNT_INITIALIZED_DATA;
        relocSec.emitter = nullptr;
        // Basic relocation table: one entry for the image base
        std::vector<BYTE> relocData;
        // IMAGE_BASE_RELOCATION structure
        DWORD pageRVA = 0;  // Page RVA (0 for image base relocations)
        DWORD blockSize = 12;  // Size of this block including header
        relocData.insert(relocData.end(), (BYTE*)&pageRVA, (BYTE*)&pageRVA + 4);
        relocData.insert(relocData.end(), (BYTE*)&blockSize, (BYTE*)&blockSize + 4);
        // Relocation entry: offset 0, type IMAGE_REL_BASED_DIR64
        WORD relocEntry = 0 | (IMAGE_REL_BASED_DIR64 << 12);
        relocData.insert(relocData.end(), (BYTE*)&relocEntry, (BYTE*)&relocEntry + 2);
        relocSec.data = relocData;
        sections.push_back(relocSec);

        // Generate DOS stub
        std::vector<BYTE> dosStub = GenerateDOSStub();

        // ── Calculate layout ──
        DWORD dosStubSize = (DWORD)dosStub.size();
        DWORD ntHeaderSize = sizeof(DWORD) + sizeof(IMAGE_FILE_HEADER) + sizeof(IMAGE_OPTIONAL_HEADER64);
        DWORD sectionHeadersSize = sections.size() * sizeof(IMAGE_SECTION_HEADER);
        DWORD headersSize = sizeof(IMAGE_DOS_HEADER) + dosStubSize + ntHeaderSize + sectionHeadersSize;

        // Align headers
        DWORD alignedHeadersSize = AlignUp(headersSize, fileAlignment);

        printf("Headers size: %u, aligned: %u\n", headersSize, alignedHeadersSize);

        auto layoutSections = [&]() {
            DWORD rva = AlignUp(alignedHeadersSize, sectionAlignment);
            DWORD raw = alignedHeadersSize;
            for (auto& sec : sections) {
                sec.virtualAddress = rva;
                sec.rawAddress = raw;
                DWORD virtualSize = AlignUp((DWORD)sec.data.size(), sectionAlignment);
                DWORD rawSize = AlignUp((DWORD)sec.data.size(), fileAlignment);
                rva += virtualSize;
                raw += rawSize;
                printf("Section %s: RVA %u, raw %u, size %zu\n", sec.name.c_str(), sec.virtualAddress, sec.rawAddress, sec.data.size());
            }
            return rva;
        };

        printf("First section RVA: %u\n", AlignUp(alignedHeadersSize, sectionAlignment));
        DWORD currentRVA = layoutSections();

        if (autoEntryPoint) {
            for (const auto& sec : sections) {
                if (sec.characteristics & IMAGE_SCN_CNT_CODE) {
                    entryPointRVA = sec.virtualAddress;
                    break;
                }
            }
            if (entryPointRVA == 0) {
                return false;
            }
        }

        printf("Entry point RVA: %u\n", entryPointRVA);

        // Build import table into .idata and refresh layout
        DWORD importTableRVA = 0;
        DWORD importTableSize = 0;
        std::vector<DWORD> iatRVAs;
        if (!imports.empty()) {
            importTableRVA = sections.back().virtualAddress;
            std::vector<BYTE> importTableData = BuildImportTable(importTableRVA, iatRVAs);
            importTableSize = (DWORD)importTableData.size();
            sections.back().data = importTableData;
            currentRVA = layoutSections();
            printf("Import table RVA: %u, size: %u\n", importTableRVA, importTableSize);
        }

        printf("Final SizeOfImage: %u\n", currentRVA);

        // ── Build DOS header ──
        IMAGE_DOS_HEADER dosHeader = {};
        dosHeader.e_magic = IMAGE_DOS_SIGNATURE;
        dosHeader.e_cblp = 0x90;
        dosHeader.e_cp = 3;
        dosHeader.e_cparhdr = 4;
        dosHeader.e_maxalloc = 0xFFFF;
        dosHeader.e_sp = 0xB8;
        dosHeader.e_lfarlc = 0x40;
        dosHeader.e_lfanew = sizeof(IMAGE_DOS_HEADER) + dosStubSize;

        // ── Build NT headers ──
        IMAGE_FILE_HEADER fileHeader = {};
        fileHeader.Machine = IMAGE_FILE_MACHINE_AMD64;
        fileHeader.NumberOfSections = (WORD)sections.size();
        fileHeader.TimeDateStamp = (DWORD)time(nullptr);  // Real build timestamp
        fileHeader.SizeOfOptionalHeader = sizeof(IMAGE_OPTIONAL_HEADER64);
        fileHeader.Characteristics = IMAGE_FILE_EXECUTABLE_IMAGE | IMAGE_FILE_LARGE_ADDRESS_AWARE;

        IMAGE_OPTIONAL_HEADER64 optHeader = {};
        optHeader.Magic = IMAGE_NT_OPTIONAL_HDR64_MAGIC;
        optHeader.MajorLinkerVersion = 14;
        optHeader.MinorLinkerVersion = 0;
        optHeader.SizeOfCode = 0; // Will calculate
        optHeader.SizeOfInitializedData = 0; // Will calculate
        optHeader.AddressOfEntryPoint = entryPointRVA;
        optHeader.BaseOfCode = sections.empty() ? 0 : sections[0].virtualAddress;
        optHeader.ImageBase = imageBase;
        optHeader.SectionAlignment = sectionAlignment;
        optHeader.FileAlignment = fileAlignment;
        optHeader.MajorOperatingSystemVersion = 6;
        optHeader.MinorOperatingSystemVersion = 0;
        optHeader.MajorImageVersion = 0;
        optHeader.MinorImageVersion = 0;
        optHeader.MajorSubsystemVersion = 6;
        optHeader.MinorSubsystemVersion = 0;
        optHeader.SizeOfImage = currentRVA;  // Moved here
        optHeader.SizeOfHeaders = alignedHeadersSize;
        optHeader.Subsystem = subsystemType;
        // Enable ASLR: DYNAMIC_BASE requires .reloc section (basic stub below)
        optHeader.DllCharacteristics = IMAGE_DLLCHARACTERISTICS_NX_COMPAT
                                     | IMAGE_DLLCHARACTERISTICS_TERMINAL_SERVER_AWARE
                                     | IMAGE_DLLCHARACTERISTICS_DYNAMIC_BASE;
        optHeader.SizeOfStackReserve = 0x100000;
        optHeader.SizeOfStackCommit = 0x1000;
        optHeader.SizeOfHeapReserve = 0x100000;
        optHeader.SizeOfHeapCommit = 0x1000;
        optHeader.NumberOfRvaAndSizes = 16;
        if (importTableRVA) {
            optHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress = importTableRVA;
            optHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].Size = importTableSize;
            // IAT DataDirectory: points to first IAT thunk (loader overwrites these with real addresses)
            if (!iatRVAs.empty()) {
                optHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IAT].VirtualAddress = iatRVAs[0];
                optHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IAT].Size = (DWORD)(iatRVAs.size() * sizeof(IMAGE_THUNK_DATA64));
            }
        }

        // Set up .reloc section data directory for ASLR
        for (size_t i = 0; i < sections.size(); ++i) {
            if (sections[i].name == ".reloc") {
                optHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].VirtualAddress = sections[i].virtualAddress;
                optHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].Size = (DWORD)sections[i].data.size();
                break;
            }
        }

        // Calculate sizes
        for (const auto& sec : sections) {
            if (sec.characteristics & IMAGE_SCN_CNT_CODE) {
                optHeader.SizeOfCode += AlignUp((DWORD)sec.data.size(), sectionAlignment);
            } else if (sec.characteristics & IMAGE_SCN_CNT_INITIALIZED_DATA) {
                optHeader.SizeOfInitializedData += AlignUp((DWORD)sec.data.size(), sectionAlignment);
            }
        }

        // ── Build section headers ──
        std::vector<IMAGE_SECTION_HEADER> sectionHeaders;
        for (const auto& sec : sections) {
            IMAGE_SECTION_HEADER sh = {};
            size_t nameLen = sec.name.size() < sizeof(sh.Name) ? sec.name.size() : sizeof(sh.Name);
            memcpy(sh.Name, sec.name.c_str(), nameLen);
            sh.Misc.VirtualSize = (DWORD)sec.data.size();
            sh.VirtualAddress = sec.virtualAddress;
            sh.SizeOfRawData = AlignUp((DWORD)sec.data.size(), fileAlignment);
            sh.PointerToRawData = sec.rawAddress;
            sh.Characteristics = sec.characteristics;
            sectionHeaders.push_back(sh);
        }

        // Fix up CALL_REL32 calls now that we know the RVAs
        if (!iatRVAs.empty()) {
            printf("IAT RVAs: ");
            for (DWORD rva : iatRVAs) printf("%u ", rva);
            printf("\n");
        }
        for (auto& sec : sections) {
            if (sec.emitter) {
                sec.emitter->FixupCalls(sec.virtualAddress);
                sec.emitter->FixupImportCalls(sec.virtualAddress, iatRVAs);
                // Update section data with fixed up code
                sec.data.assign(sec.emitter->Data(), sec.emitter->Data() + sec.emitter->Size());
                printf("Fixed up code: ");
                for (size_t i = 0; i < sec.emitter->Size(); ++i) {
                    printf("%02X ", sec.emitter->Data()[i]);
                }
                printf("\n");
            }
        }

        // ── Write file ──
        FILE* f = nullptr;
        if (fopen_s(&f, filename, "wb") != 0) return false;

        // DOS header + stub
        fwrite(&dosHeader, sizeof(dosHeader), 1, f);
        fwrite(dosStub.data(), dosStub.size(), 1, f);

        // NT headers
        DWORD ntSignature = IMAGE_NT_SIGNATURE;
        fwrite(&ntSignature, sizeof(ntSignature), 1, f);
        fwrite(&fileHeader, sizeof(fileHeader), 1, f);
        fwrite(&optHeader, sizeof(optHeader), 1, f);

        // Section headers
        for (const auto& sh : sectionHeaders) {
            fwrite(&sh, sizeof(sh), 1, f);
        }

        // Pad to file alignment
        PadFile(f, alignedHeadersSize);

        // Section data
        printf("Writing %zu sections\n", sections.size());
        for (const auto& sec : sections) {
            printf("Writing section %s, size %zu\n", sec.name.c_str(), sec.data.size());
            fwrite(sec.data.data(), sec.data.size(), 1, f);
            DWORD nextPos = sec.rawAddress + AlignUp((DWORD)sec.data.size(), fileAlignment);
            printf("Padding to %u\n", nextPos);
            PadFile(f, nextPos);
        }

        fclose(f);

        // ── Compute and patch PE CheckSum ──
        ComputeAndPatchChecksum(filename);

        printf("File written, validating...\n");
        return ValidatePEFile(filename);
    }

    // Compute PE checksum using the standard fold-add-WORD algorithm
    // (same as MapFileAndCheckSum / MSVC /RELEASE linker flag)
    void ComputeAndPatchChecksum(const char* filename) {
        FILE* f = nullptr;
        if (fopen_s(&f, filename, "r+b") != 0) return;

        // Get file size
        fseek(f, 0, SEEK_END);
        long fileSize = ftell(f);
        fseek(f, 0, SEEK_SET);

        // Read entire file
        std::vector<BYTE> fileData((size_t)fileSize);
        if (fread(fileData.data(), 1, fileSize, f) != (size_t)fileSize) {
            fclose(f);
            return;
        }

        // Find checksum field offset: e_lfanew + 4 (sig) + sizeof(FILE_HEADER) + 64 (offset of CheckSum in OPT64)
        IMAGE_DOS_HEADER* dos = (IMAGE_DOS_HEADER*)fileData.data();
        DWORD checksumOffset = dos->e_lfanew + 4 + sizeof(IMAGE_FILE_HEADER) + 64;

        // Zero checksum field before computing
        *(DWORD*)&fileData[checksumOffset] = 0;

        // Fold-add all WORDs
        ULONGLONG sum = 0;
        DWORD numWords = (DWORD)(fileSize / 2);
        WORD* words = (WORD*)fileData.data();
        for (DWORD i = 0; i < numWords; ++i) {
            sum += words[i];
            sum = (sum & 0xFFFF) + (sum >> 16);  // Fold carry
        }
        // Handle odd trailing byte
        if (fileSize & 1) {
            sum += fileData[fileSize - 1];
            sum = (sum & 0xFFFF) + (sum >> 16);
        }
        // Final fold
        sum = (sum & 0xFFFF) + (sum >> 16);
        DWORD checksum = (DWORD)(sum + fileSize);

        // Patch it back
        fseek(f, (long)checksumOffset, SEEK_SET);
        fwrite(&checksum, sizeof(checksum), 1, f);
        fclose(f);

        printf("PE CheckSum computed and patched: 0x%08X\n", checksum);
    }

private:
    bool ValidatePEFile(const char* filename) {
        printf("Validating PE file: %s\n", filename);
        FILE* f = nullptr;
        if (fopen_s(&f, filename, "rb") != 0) {
            printf("Failed to open file for validation\n");
            return false;
        }

        // Read DOS header
        IMAGE_DOS_HEADER dosHeader;
        if (fread(&dosHeader, sizeof(dosHeader), 1, f) != 1) {
            printf("Failed to read DOS header\n");
            fclose(f);
            return false;
        }

        // Check DOS signature
        if (dosHeader.e_magic != IMAGE_DOS_SIGNATURE) {
            printf("Invalid DOS signature: %x\n", dosHeader.e_magic);
            fclose(f);
            return false;
        }

        // Seek to NT headers
        if (fseek(f, dosHeader.e_lfanew, SEEK_SET) != 0) {
            printf("Failed to seek to NT headers\n");
            fclose(f);
            return false;
        }

        // Read NT signature
        DWORD ntSignature;
        if (fread(&ntSignature, sizeof(ntSignature), 1, f) != 1) {
            printf("Failed to read NT signature\n");
            fclose(f);
            return false;
        }

        if (ntSignature != IMAGE_NT_SIGNATURE) {
            printf("Invalid NT signature: %x\n", ntSignature);
            fclose(f);
            return false;
        }

        // Read file header
        IMAGE_FILE_HEADER fileHeader;
        if (fread(&fileHeader, sizeof(fileHeader), 1, f) != 1) {
            printf("Failed to read file header\n");
            fclose(f);
            return false;
        }

        // Check machine type
        if (fileHeader.Machine != IMAGE_FILE_MACHINE_AMD64) {
            printf("Invalid machine type: %x\n", fileHeader.Machine);
            fclose(f);
            return false;
        }

        IMAGE_OPTIONAL_HEADER64 optHeader;
        if (fread(&optHeader, sizeof(optHeader), 1, f) != 1) {
            printf("Failed to read optional header\n");
            fclose(f);
            return false;
        }

        if (optHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC) {
            printf("Invalid optional header magic: %x\n", optHeader.Magic);
            fclose(f);
            return false;
        }

        if (optHeader.SectionAlignment < optHeader.FileAlignment) {
            printf("Invalid alignment: section=%u file=%u\n", optHeader.SectionAlignment, optHeader.FileAlignment);
            fclose(f);
            return false;
        }

        std::vector<IMAGE_SECTION_HEADER> sectionHeaders(fileHeader.NumberOfSections);
        if (!sectionHeaders.empty()) {
            if (fread(sectionHeaders.data(), sizeof(IMAGE_SECTION_HEADER), sectionHeaders.size(), f) != sectionHeaders.size()) {
                printf("Failed to read section headers\n");
                fclose(f);
                return false;
            }
        }

        bool entryPointValid = false;
        for (const auto& sh : sectionHeaders) {
            DWORD start = sh.VirtualAddress;
            DWORD end = sh.VirtualAddress + (sh.Misc.VirtualSize ? sh.Misc.VirtualSize : sh.SizeOfRawData);
            if (optHeader.AddressOfEntryPoint >= start && optHeader.AddressOfEntryPoint < end) {
                entryPointValid = (sh.Characteristics & IMAGE_SCN_MEM_EXECUTE) != 0;
                break;
            }
        }

        if (!entryPointValid) {
            printf("Entry point does not map to an executable section\n");
            fclose(f);
            return false;
        }

        printf("PE validation passed\n");
        fclose(f);
        return true;
    }

    DWORD AlignUp(DWORD value, DWORD alignment) {
        return (value + alignment - 1) & ~(alignment - 1);
    }

    void PadFile(FILE* f, DWORD targetOffset) {
        long current = ftell(f);
        while (current < (long)targetOffset) {
            fputc(0, f);
            current++;
        }
    }

    DWORD CalculateImportTableSize() {
        DWORD size = (DWORD)((imports.size() + 1) * sizeof(IMAGE_IMPORT_DESCRIPTOR));
        for (const auto& imp : imports) {
            size += AlignUp((DWORD)imp.dllName.size() + 1, 2);  // DLL name
            size += (DWORD)(imp.functions.size() + 1) * sizeof(IMAGE_THUNK_DATA64);  // ILT + null
            size += (DWORD)(imp.functions.size() + 1) * sizeof(IMAGE_THUNK_DATA64);  // IAT + null
            for (const auto& func : imp.functions) {
                size += sizeof(WORD) + (DWORD)func.size() + 1;  // Hint + name
                size = AlignUp(size, 2);
            }
        }
        return size;
    }

    std::vector<BYTE> BuildImportTable(DWORD baseRVA, std::vector<DWORD>& outIATRVAs) {
        std::vector<BYTE> data;
        outIATRVAs.clear();

        DWORD currentRVA = baseRVA + (DWORD)((imports.size() + 1) * sizeof(IMAGE_IMPORT_DESCRIPTOR));

        // Calculate RVAs for each part
        std::map<std::string, DWORD> dllNameRVAs;
        std::map<std::string, std::vector<DWORD>> iltRVAs, iatRVAs, hintNameRVAs;

        for (const auto& imp : imports) {
            dllNameRVAs[imp.dllName] = currentRVA;
            currentRVA += AlignUp((DWORD)imp.dllName.size() + 1, 2);

            iltRVAs[imp.dllName].resize(imp.functions.size());
            iatRVAs[imp.dllName].resize(imp.functions.size());
            hintNameRVAs[imp.dllName].resize(imp.functions.size());

            for (size_t i = 0; i < imp.functions.size(); ++i) {
                iltRVAs[imp.dllName][i] = currentRVA;
                currentRVA += sizeof(IMAGE_THUNK_DATA64);
            }
            currentRVA += sizeof(IMAGE_THUNK_DATA64); // ILT null terminator

            for (size_t i = 0; i < imp.functions.size(); ++i) {
                iatRVAs[imp.dllName][i] = currentRVA;
                outIATRVAs.push_back(currentRVA);
                currentRVA += sizeof(IMAGE_THUNK_DATA64);
            }
            currentRVA += sizeof(IMAGE_THUNK_DATA64); // IAT null terminator

            for (size_t i = 0; i < imp.functions.size(); ++i) {
                hintNameRVAs[imp.dllName][i] = currentRVA;
                DWORD hintNameSize = sizeof(WORD) + (DWORD)imp.functions[i].size() + 1;
                currentRVA += AlignUp(hintNameSize, 2);
            }
        }

        // Build descriptors
        for (const auto& imp : imports) {
            IMAGE_IMPORT_DESCRIPTOR desc = {};
            desc.Name = dllNameRVAs[imp.dllName];
            desc.FirstThunk = iatRVAs[imp.dllName][0];
            desc.OriginalFirstThunk = iltRVAs[imp.dllName][0];
            desc.TimeDateStamp = 0;
            desc.ForwarderChain = 0;
            data.insert(data.end(), (BYTE*)&desc, (BYTE*)&desc + sizeof(desc));
        }
        // Null descriptor
        IMAGE_IMPORT_DESCRIPTOR nullDesc = {};
        data.insert(data.end(), (BYTE*)&nullDesc, (BYTE*)&nullDesc + sizeof(nullDesc));

        // DLL names
        for (const auto& imp : imports) {
            data.insert(data.end(), imp.dllName.begin(), imp.dllName.end());
            data.push_back(0);
            if (data.size() % 2) data.push_back(0);  // Align to WORD
        }

        // ILT
        for (const auto& imp : imports) {
            for (size_t i = 0; i < imp.functions.size(); ++i) {
                IMAGE_THUNK_DATA64 thunk = {};
                thunk.u1.AddressOfData = hintNameRVAs[imp.dllName][i];
                data.insert(data.end(), (BYTE*)&thunk, (BYTE*)&thunk + sizeof(thunk));
            }
            IMAGE_THUNK_DATA64 nullThunk = {};
            data.insert(data.end(), (BYTE*)&nullThunk, (BYTE*)&nullThunk + sizeof(nullThunk));
        }

        // IAT (same as ILT for now)
        for (const auto& imp : imports) {
            for (size_t i = 0; i < imp.functions.size(); ++i) {
                IMAGE_THUNK_DATA64 thunk = {};
                thunk.u1.AddressOfData = hintNameRVAs[imp.dllName][i];
                data.insert(data.end(), (BYTE*)&thunk, (BYTE*)&thunk + sizeof(thunk));
            }
            IMAGE_THUNK_DATA64 nullThunk = {};
            data.insert(data.end(), (BYTE*)&nullThunk, (BYTE*)&nullThunk + sizeof(nullThunk));
        }

        // Hint/name table
        for (const auto& imp : imports) {
            for (const auto& func : imp.functions) {
                IMAGE_IMPORT_BY_NAME ibn = {};
                ibn.Hint = 0;  // No hint
                data.insert(data.end(), (BYTE*)&ibn, (BYTE*)&ibn + sizeof(WORD));
                data.insert(data.end(), func.begin(), func.end());
                data.push_back(0);
                if (data.size() % 2) data.push_back(0);
            }
        }

        return data;
    }
};

// ── Example usage (uncommented for testing) ──
int main() {
    printf("Starting PE writer test\n");
    PEWriter writer;

    // Generate machine code that calls ExitProcess(0)
    MachineCodeEmitter emitter;
    emitter.FunctionPrologue(0, 4, false);  // Standard frame: 0 locals, 4 args, no frame pointer
    emitter.MOV_RCX_IMM(0);  // Exit code 0
    emitter.CALL_IMPORT(0);  // Call first import (ExitProcess) via FF 15 [rip+disp32]

    printf("Code size: %zu bytes\n", emitter.Size());
    printf("Code: ");
    for (size_t i = 0; i < emitter.Size(); ++i) {
        printf("%02X ", emitter.Data()[i]);
    }
    printf("\n");

    writer.AddSection(".text", emitter.Data(), emitter.Size(),
                      IMAGE_SCN_MEM_EXECUTE | IMAGE_SCN_MEM_READ | IMAGE_SCN_CNT_CODE, &emitter);
    writer.AddImport("kernel32.dll", "ExitProcess");
    writer.SetEntryPoint(0x1000);  // RVA of .text
    writer.SetSubsystem(IMAGE_SUBSYSTEM_WINDOWS_GUI);

    printf("Calling EmitExecutable\n");
    bool result = writer.EmitExecutable("test.exe");
    printf("EmitExecutable returned %d\n", result);
    
    // Check if file exists
    FILE* checkF = nullptr;
    if (fopen_s(&checkF, "test.exe", "rb") == 0) {
        printf("File exists\n");
        fseek(checkF, 0, SEEK_END);
        long size = ftell(checkF);
        printf("File size: %ld bytes\n", size);
        fclose(checkF);
    } else {
        printf("File does not exist\n");
    }
    
    if (result) {
        printf("Successfully created test.exe\n");
        // Check if file exists
        FILE* f = nullptr;
        if (fopen_s(&f, "test.exe", "rb") == 0) {
            printf("File exists and is readable\n");
            fseek(f, 0, SEEK_END);
            long size = ftell(f);
            printf("File size: %ld bytes\n", size);
            fclose(f);
        } else {
            printf("File does not exist or is not readable\n");
        }
        return 0;
    } else {
        printf("Failed to create executable\n");
        return 1;
    }
}
