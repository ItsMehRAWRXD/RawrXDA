// ============================================================================
// RawrXD_PE_Writer.h — Complete monolithic PE32+ writer + machine code emitter header
//
// Reverse-engineered from x64 MASM to simplest least-dep C++ backend designer.
// Generates runnable executables with import tables.
//
// Usage:
//   PEWriter writer;
//   writer.AddSection(".text", code, codeSize, IMAGE_SCN_MEM_EXECUTE | IMAGE_SCN_MEM_READ);
//   writer.AddImport("kernel32.dll", "ExitProcess");
//   writer.EmitExecutable("output.exe");
// ============================================================================

#ifndef RAWRXD_PE_WRITER_H
#define RAWRXD_PE_WRITER_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <vector>
#include <map>
#include <string>

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

// ── Machine Code Emitter (reverse-engineered from x64 MASM) ──
class MachineCodeEmitter {
public:
    std::vector<BYTE> code;

    void Emit(BYTE b);
    void Emit(WORD w);
    void Emit(DWORD d);
    void Emit(ULONGLONG q);

    // x64 instruction emitters (reverse-engineered from MASM)
    void RET();
    void MOV_RAX_IMM(ULONGLONG imm);
    void CALL_RAX();
    void CALL_RCX();
    void CALL_RDX();
    void CALL_RBX();
    void SUB_RSP_IMM8(BYTE imm);
    void ADD_RSP_IMM8(BYTE imm);
    void MOV_RCX_IMM(ULONGLONG imm);
    void MOV_RDX_IMM(ULONGLONG imm);
    void MOV_R8_IMM(ULONGLONG imm);
    void MOV_R9_IMM(ULONGLONG imm);

    // PUSH RBP
    void PUSH_RBP();
    // POP RBP
    void POP_RBP();
    // MOV RBP, RSP
    void MOV_RBP_RSP();
    // MOV RSP, RBP
    void MOV_RSP_RBP();
    // SUB RSP, imm32 (for frame sizes > 127)
    void SUB_RSP_IMM32(DWORD imm);
    // ADD RSP, imm32 (for frame sizes > 127)
    void ADD_RSP_IMM32(DWORD imm);

    // ── ALU operations ──
    // XOR RAX, RAX (zero register)
    void XOR_RAX_RAX();
    // XOR RCX, RCX
    void XOR_RCX_RCX();
    // TEST RAX, RAX (set flags based on RAX & RAX)
    void TEST_RAX_RAX();
    // TEST RCX, RCX
    void TEST_RCX_RCX();
    // AND RAX, imm32
    void AND_RAX_IMM32(DWORD imm);
    // OR RAX, RCX
    void OR_RAX_RCX();
    // SHL RAX, CL (shift left)
    void SHL_RAX_CL();
    // SHR RAX, CL (shift right logical)
    void SHR_RAX_CL();
    // SAR RAX, CL (shift right arithmetic)
    void SAR_RAX_CL();
    // CMP RCX, imm32
    void CMP_RCX_IMM32(DWORD imm);

    // ── PUSH/POP operations for all GP registers ──
    void PUSH_RAX();
    void PUSH_RCX();
    void PUSH_RDX();
    void PUSH_RBX();
    void PUSH_RSP();
    void PUSH_RSI();
    void PUSH_RDI();
    void PUSH_R8();
    void PUSH_R9();
    void PUSH_R10();
    void PUSH_R11();

    void POP_RAX();
    void POP_RCX();
    void POP_RDX();
    void POP_RBX();
    void POP_RSP();
    void POP_RSI();
    void POP_RDI();
    void POP_R8();
    void POP_R9();
    void POP_R10();
    void POP_R11();

    // ── Control flow: conditional jumps and LEA ──
    // JMP rel32 (unconditional near jump)
    void JMP_REL32(DWORD targetRVA);
    // JE rel32 (jump if equal / zero flag set)
    void JE_REL32(DWORD targetRVA);
    // JNE rel32 (jump if not equal / zero flag clear)
    void JNE_REL32(DWORD targetRVA);
    // JG rel32 (jump if greater / ZF=0 and SF=OF)
    void JG_REL32(DWORD targetRVA);
    // JL rel32 (jump if less / SF≠OF)
    void JL_REL32(DWORD targetRVA);
    // JGE rel32 (jump if greater or equal / SF=OF)
    void JGE_REL32(DWORD targetRVA);
    // JLE rel32 (jump if less or equal / ZF=1 or SF≠OF)
    void JLE_REL32(DWORD targetRVA);
    // LEA RCX, [RIP+disp32] — load effective address (position-independent addressing)
    void LEA_RCX_RIP_DISP32(DWORD disp32);
    // LEA RAX, [RIP+disp32]
    void LEA_RAX_RIP_DISP32(DWORD disp32);
    // LEA RDX, [RIP+disp32]
    void LEA_RDX_RIP_DISP32(DWORD disp32);
    // LEA RBX, [RIP+disp32]
    void LEA_RBX_RIP_DISP32(DWORD disp32);

    // ── Register-to-register MOV operations ──
    // MOV RAX, RCX
    void MOV_RAX_RCX();
    // MOV RCX, RAX
    void MOV_RCX_RAX();
    // MOV RDX, RAX
    void MOV_RDX_RAX();
    // MOV RAX, RDX
    void MOV_RAX_RDX();

    // Function prologue/epilogue — parameterized frame size
    // localBytes = space for local variables (0 for leaf functions)
    // numArgs    = max arguments passed to any callee (minimum 4 for shadow space)
    // useFramePointer = emit PUSH RBP / MOV RBP, RSP for debuggable frames
    void FunctionPrologue(DWORD localBytes = 0, DWORD numArgs = 4, bool useFramePointer = false);
    void FunctionEpilogue(bool useFramePointer = false);

    // Call imported function by index — FF 15 [rip+disp32] indirect call through IAT
    // This is the correct x64 encoding: the IAT slot holds the real function pointer,
    // and FF 15 dereferences it. E8 rel32 would jump INTO the IAT data, which is wrong.
    void CALL_IMPORT(size_t importIndex);

    // Fix up CALL_REL32 calls with actual RVAs
    void FixupCalls(DWORD currentRVA);

    // Fix up import calls with IAT RVAs (FF 15 [rip+disp32] encoding)
    // disp32 = targetRVA - (instructionRVA + 4), where +4 accounts for the disp32 field itself
    void FixupImportCalls(DWORD currentRVA, const std::vector<DWORD>& iatRVAs);

    size_t Size() const;
    const BYTE* Data() const;

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

    std::vector<BYTE> GenerateDOSStub();
    std::vector<BYTE> BuildImportTable(DWORD baseRVA, std::vector<DWORD>& outIATRVAs);
    void ComputeAndPatchChecksum(const char* filename);
    bool ValidatePEFile(const char* filename);
    DWORD AlignUp(DWORD value, DWORD alignment);
    void PadFile(FILE* f, DWORD targetPos);

public:
    PEWriter();
    void AddSection(const char* name, const BYTE* data, size_t size, DWORD characteristics, MachineCodeEmitter* emitter = nullptr);
    void AddImport(const char* dllName, const char* functionName);
    void SetEntryPoint(DWORD rva);
    void SetSubsystem(WORD subsystem);
    bool EmitExecutable(const char* filename);
};

#endif // RAWRXD_PE_WRITER_H