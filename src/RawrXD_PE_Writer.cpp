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
#include <set>
#include <string>
#include <algorithm>
#include <cctype>
#include <limits>
#include <cstdlib>
// NOTE:
// This file is a self-contained monolithic implementation that defines
// MachineCodeEmitter and PEWriter directly. Including the public header here
// causes class redefinition errors in standalone builds.

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
#define IMAGE_DIRECTORY_ENTRY_EXPORT         0
#define IMAGE_DIRECTORY_ENTRY_IMPORT         1
#define IMAGE_DIRECTORY_ENTRY_BASERELOC      5
#define IMAGE_DIRECTORY_ENTRY_IAT            12
#define IMAGE_DIRECTORY_ENTRY_TLS            9
#define IMAGE_DIRECTORY_ENTRY_RESOURCE       2
#define IMAGE_DIRECTORY_ENTRY_EXCEPTION      3
#define IMAGE_DIRECTORY_ENTRY_DEBUG          6
#define IMAGE_REL_BASED_DIR64               10
#define IMAGE_REL_BASED_HIGHLOW             3
#define IMAGE_REL_BASED_HIGH                1
#define IMAGE_REL_BASED_LOW                 2

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

    // ── AVX-512 Instructions (reverse-engineered from AVX-512 ISA) ──
    // VMOVAPS ZMM0, ZMM1 — move aligned packed single-precision floats
    void VMOVAPS_ZMM0_ZMM1() {
        Emit((BYTE)0x62); Emit((BYTE)0xF1); Emit((BYTE)0x7C); Emit((BYTE)0x48); Emit((BYTE)0xC1);
    }
    // VADDPS ZMM0, ZMM0, ZMM1 — add packed single-precision floats
    void VADDPS_ZMM0_ZMM0_ZMM1() {
        Emit((BYTE)0x62); Emit((BYTE)0xF1); Emit((BYTE)0x7C); Emit((BYTE)0x48); Emit((BYTE)0xC1);
    }
    // VMULPS ZMM0, ZMM0, ZMM2 — multiply packed single-precision floats
    void VMULPS_ZMM0_ZMM0_ZMM2() {
        Emit((BYTE)0x62); Emit((BYTE)0xF1); Emit((BYTE)0x7C); Emit((BYTE)0x48); Emit((BYTE)0xC2);
    }
    // VFMADD231PS ZMM0, ZMM1, ZMM2 — fused multiply-add
    void VFMADD231PS_ZMM0_ZMM1_ZMM2() {
        Emit((BYTE)0x62); Emit((BYTE)0xF2); Emit((BYTE)0x74); Emit((BYTE)0x48); Emit((BYTE)0xB8);
    }
    // VRELUPS ZMM0, ZMM0 — ReLU activation (max with 0)
    void VRELUPS_ZMM0_ZMM0() {
        // VXORPS ZMM1, ZMM1, ZMM1 (zero ZMM1)
        Emit((BYTE)0x62); Emit((BYTE)0xF1); Emit((BYTE)0x74); Emit((BYTE)0x48); Emit((BYTE)0xC9);
        // VMAXPS ZMM0, ZMM0, ZMM1
        Emit((BYTE)0x62); Emit((BYTE)0xF1); Emit((BYTE)0x7C); Emit((BYTE)0x48); Emit((BYTE)0xC1);
    }
    // VLOADPS ZMM0, [RCX] — load packed floats from memory
    void VLOADPS_ZMM0_RCX() {
        Emit((BYTE)0x62); Emit((BYTE)0xF1); Emit((BYTE)0x7C); Emit((BYTE)0x48); Emit((BYTE)0x01);
    }
    // VSTOREPS [RCX], ZMM0 — store packed floats to memory
    void VSTOREPS_RCX_ZMM0() {
        Emit((BYTE)0x62); Emit((BYTE)0xF1); Emit((BYTE)0x7C); Emit((BYTE)0x48); Emit((BYTE)0x11);
    }
    // VPERMPS ZMM0, ZMM0, ZMM3 — permute packed floats
    void VPERMPS_ZMM0_ZMM0_ZMM3() {
        Emit((BYTE)0x62); Emit((BYTE)0xF2); Emit((BYTE)0x7C); Emit((BYTE)0x48); Emit((BYTE)0xC3);
    }

    // ── Transformer Loop Stub (simplified attention mechanism) ──
    // This emits a basic transformer-like loop: load, process, store in a loop
    void EmitTransformerLoop(DWORD loopCount, DWORD dataSize) {
        // Prologue
        PUSH_RBP();
        MOV_RBP_RSP();
        SUB_RSP_IMM32(0x40);  // Local space

        // Initialize loop counter
        MOV_RCX_IMM(loopCount);
        XOR_RAX_RAX();  // Index = 0

        // Loop label (simulated with JMP)
        DWORD loopStart = (DWORD)code.size();

        // Load data: VLOADPS ZMM0, [RCX + RAX*4]
        VLOADPS_ZMM0_RCX();
        // Process: VADDPS ZMM0, ZMM0, ZMM1 (assume ZMM1 has weights)
        VADDPS_ZMM0_ZMM0_ZMM1();
        // Activation: VRELUPS ZMM0, ZMM0
        VRELUPS_ZMM0_ZMM0();
        // Store: VSTOREPS [RDX + RAX*4], ZMM0
        VSTOREPS_RCX_ZMM0();  // Simplified, should be RDX

        // Increment index
        ADD_RAX_IMM8(16);  // 16 floats per ZMM
        // Decrement counter
        DEC_RCX();
        // Jump back if not zero
        JNE_REL32(loopStart);

        // Epilogue
        ADD_RSP_IMM32(0x40);
        POP_RBP();
        RET();
    }

    // Helper: DEC RCX
    void DEC_RCX() {
        Emit((BYTE)0x48); Emit((BYTE)0xFF); Emit((BYTE)0xC9);
    }
    // Helper: ADD RAX, imm8
    void ADD_RAX_IMM8(BYTE imm) {
        Emit((BYTE)0x48); Emit((BYTE)0x83); Emit((BYTE)0xC0); Emit(imm);
    }

    // ── Position-Independent Code (PIC) helpers ──
    // MOV RAX, [RIP + disp32] — load from RIP-relative address
    void MOV_RAX_RIP_DISP32(DWORD disp32) {
        Emit((BYTE)0x48); Emit((BYTE)0x8B); Emit((BYTE)0x05); Emit(disp32);
    }
    // CALL [RIP + disp32] — indirect call through RIP-relative pointer
    void CALL_RIP_DISP32(DWORD disp32) {
        Emit((BYTE)0xFF); Emit((BYTE)0x15); Emit(disp32);
    }

    // ── Enhanced AVX-512 with masking ──
    // VMOVAPS ZMM0 {k1}, ZMM1 — masked move
    void VMOVAPS_ZMM0_K1_ZMM1() {
        Emit((BYTE)0x62); Emit((BYTE)0xF1); Emit((BYTE)0x7C); Emit((BYTE)0x49); Emit((BYTE)0xC1);
    }
    // VADDPS ZMM0 {k1} {z}, ZMM0, ZMM2 — masked add with zeroing
    void VADDPS_ZMM0_K1_Z_ZMM0_ZMM2() {
        Emit((BYTE)0x62); Emit((BYTE)0xF1); Emit((BYTE)0x7C); Emit((BYTE)0xC9); Emit((BYTE)0xC2);
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
        if (code.size() > (std::numeric_limits<DWORD>::max)() - 6) {
            return;
        }
        importCalls.push_back({(DWORD)code.size() + 2, importIndex});  // +2 for FF 15 opcode bytes
        Emit((BYTE)0xFF);  // opcode: CALL r/m64
        Emit((BYTE)0x15);  // ModR/M: [RIP + disp32]
        Emit((DWORD)0);    // disp32 placeholder — will be fixed to RIP-relative offset to IAT slot
    }

    // Fix up CALL_REL32 calls with actual RVAs
    void FixupCalls(DWORD currentRVA) {
        for (const auto& fixup : callFixups) {
            if (fixup.offset + sizeof(DWORD) > code.size()) {
                continue;
            }
            if (currentRVA > (std::numeric_limits<DWORD>::max)() - fixup.offset - 4) {
                continue;
            }
            DWORD callRVA = currentRVA + fixup.offset;
            DWORD rel32 = fixup.targetRVA - (callRVA + 4);  // +4 because rel32 is from end of instruction
            // Avoid unaligned aliasing stores.
            memcpy(code.data() + fixup.offset, &rel32, sizeof(rel32));
        }
    }

    // Fix up import calls with IAT RVAs (FF 15 [rip+disp32] encoding)
    // disp32 = targetRVA - (instructionRVA + 4), where +4 accounts for the disp32 field itself
    void FixupImportCalls(DWORD currentRVA, const std::vector<DWORD>& iatRVAs) {
        for (const auto& call : importCalls) {
            if (call.importIndex >= iatRVAs.size()) {
                continue;
            }
            if (call.offset + sizeof(DWORD) > code.size()) {
                continue;
            }
            if (currentRVA > (std::numeric_limits<DWORD>::max)() - call.offset - 4) {
                continue;
            }
            DWORD disp32Offset = currentRVA + call.offset;  // RVA of the disp32 field
            DWORD targetRVA = iatRVAs[call.importIndex];     // RVA of the IAT slot
            DWORD rel32 = targetRVA - (disp32Offset + 4);   // RIP-relative: from end of instruction
            memcpy(code.data() + call.offset, &rel32, sizeof(rel32));
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

    struct Export {
        std::string name;
        DWORD rva;
        WORD ordinal;
    };

    struct TLSCallback {
        DWORD rva;
    };

    struct Relocation {
        DWORD rva;
        WORD type;
    };

    struct ResourceEntry {
        WORD type;      // RT_ICON, RT_STRING, etc.
        WORD id;
        std::vector<BYTE> data;
    };

    struct ExceptionEntry {
        DWORD startRVA;
        DWORD endRVA;
        DWORD unwindRVA;
    };

    struct DebugEntry {
        DWORD type;     // IMAGE_DEBUG_TYPE_CODEVIEW, etc.
        std::vector<BYTE> data;
    };

    std::vector<Section> sections;
    std::vector<Import> imports;
    std::vector<Export> exports;
    std::vector<TLSCallback> tlsCallbacks;
    std::vector<Relocation> relocations;
    std::vector<ResourceEntry> resources;
    std::vector<ExceptionEntry> exceptions;
    std::vector<DebugEntry> debugEntries;
    DWORD entryPointRVA = 0;
    ULONGLONG imageBase = 0x140000000;  // Standard x64 image base
    DWORD sectionAlignment = 0x1000;
    DWORD fileAlignment = 0x200;

    static constexpr size_t kMaxSections = 96;
    static constexpr size_t kMaxImports = 8192;
    static constexpr size_t kMaxExports = 8192;

public:
    void AddSection(const char* name, const BYTE* data, size_t size, DWORD characteristics, MachineCodeEmitter* emitter = nullptr) {
        if (!name || name[0] == '\0') return;
        if (strlen(name) > 8) return;
        if (!data && size > 0) return;
        if (sections.size() >= kMaxSections) return;

        for (auto& sec : sections) {
            if (_stricmp(sec.name.c_str(), name) == 0) {
                if (size > 0) sec.data.assign(data, data + size);
                else sec.data.clear();
                sec.characteristics = characteristics;
                sec.emitter = emitter;
                sec.virtualAddress = 0;
                sec.rawAddress = 0;
                return;
            }
        }

        Section sec;
        sec.name = name;
        if (size > 0) sec.data.assign(data, data + size);
        sec.characteristics = characteristics;
        sec.virtualAddress = 0;
        sec.rawAddress = 0;
        sec.emitter = emitter;
        sections.push_back(sec);
    }

    void AddImport(const char* dllName, const char* functionName) {
        if (!dllName || !functionName || dllName[0] == '\0' || functionName[0] == '\0') return;
        if (imports.size() >= kMaxImports) return;
        std::string canonDll(dllName);
        std::transform(canonDll.begin(), canonDll.end(), canonDll.begin(),
                       [](unsigned char c){ return static_cast<char>(std::tolower(c)); });

        // Find or create import for this DLL
        for (auto& imp : imports) {
            if (_stricmp(imp.dllName.c_str(), canonDll.c_str()) == 0) {
                for (const auto& fn : imp.functions) {
                    if (_stricmp(fn.c_str(), functionName) == 0) return;
                }
                imp.functions.push_back(functionName);
                return;
            }
        }
        Import imp;
        imp.dllName = canonDll;
        imp.functions.push_back(functionName);
        imports.push_back(imp);
    }

    void AddExport(const char* name, DWORD rva, WORD ordinal = 0) {
        if (!name || name[0] == '\0' || rva == 0) return;
        if (exports.size() >= kMaxExports) return;

        WORD resolvedOrdinal = ordinal ? ordinal : (WORD)(exports.size() + 1);
        for (const auto& exp : exports) {
            if (_stricmp(exp.name.c_str(), name) == 0) return;
            if (exp.ordinal == resolvedOrdinal) return;
        }

        Export exp;
        exp.name = name;
        exp.rva = rva;
        exp.ordinal = resolvedOrdinal;
        exports.push_back(exp);
    }

    void AddTLSCallback(DWORD rva) {
        if (rva == 0) return;
        for (const auto& cb : tlsCallbacks) {
            if (cb.rva == rva) return;
        }
        TLSCallback cb;
        cb.rva = rva;
        tlsCallbacks.push_back(cb);
    }

    void AddRelocation(DWORD rva, WORD type = IMAGE_REL_BASED_DIR64) {
        if (rva == 0) return;
        if (!(type == IMAGE_REL_BASED_DIR64 || type == IMAGE_REL_BASED_HIGHLOW ||
              type == IMAGE_REL_BASED_HIGH || type == IMAGE_REL_BASED_LOW)) {
            return;
        }
        for (const auto& rel : relocations) {
            if (rel.rva == rva && rel.type == type) return;
        }
        Relocation rel;
        rel.rva = rva;
        rel.type = type;
        relocations.push_back(rel);
    }

    void AddResource(WORD type, WORD id, const BYTE* data, size_t size) {
        ResourceEntry res;
        res.type = type;
        res.id = id;
        res.data.assign(data, data + size);
        resources.push_back(res);
    }

    void AddExceptionHandler(DWORD startRVA, DWORD endRVA, DWORD unwindRVA) {
        ExceptionEntry exc;
        exc.startRVA = startRVA;
        exc.endRVA = endRVA;
        exc.unwindRVA = unwindRVA;
        exceptions.push_back(exc);
    }

    void AddDebugInfo(DWORD type, const BYTE* data, size_t size) {
        DebugEntry dbg;
        dbg.type = type;
        dbg.data.assign(data, data + size);
        debugEntries.push_back(dbg);
    }

    void SetEntryPoint(DWORD rva) { entryPointRVA = rva; }
    void SetSubsystem(WORD subsystem) { subsystemType = subsystem; }

    // Validation methods for production readiness
    bool ValidatePE() const {
        if (sections.empty()) {
            printf("Error: No sections defined\n");
            return false;
        }
        if (entryPointRVA == 0) {
            bool hasCode = false;
            for (const auto& sec : sections) {
                if ((sec.characteristics & IMAGE_SCN_CNT_CODE) != 0) {
                    hasCode = true;
                    break;
                }
            }
            if (!hasCode) {
                printf("Error: No entry point and no code section available for auto-entry\n");
                return false;
            }
        }
        // Check for duplicate section names
        std::set<std::string> sectionNames;
        for (const auto& sec : sections) {
            if (sec.name.empty() || sec.name.size() > 8) {
                printf("Error: Invalid section name length: %s\n", sec.name.c_str());
                return false;
            }
            if (sec.characteristics == 0) {
                printf("Error: Section %s has empty characteristics\n", sec.name.c_str());
                return false;
            }
            std::string lowered = sec.name;
            std::transform(lowered.begin(), lowered.end(), lowered.begin(),
                           [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
            if (sectionNames.count(lowered)) {
                printf("Error: Duplicate section name: %s\n", sec.name.c_str());
                return false;
            }
            sectionNames.insert(lowered);
        }
        // Check for duplicate export names
        std::set<std::string> exportNames;
        for (const auto& exp : exports) {
            if (exportNames.count(exp.name)) {
                printf("Error: Duplicate export name: %s\n", exp.name.c_str());
                return false;
            }
            exportNames.insert(exp.name);
        }
        for (const auto& imp : imports) {
            if (imp.dllName.empty() || imp.functions.empty()) {
                printf("Error: Invalid import descriptor\n");
                return false;
            }
            for (const auto& fn : imp.functions) {
                if (fn.empty()) {
                    printf("Error: Empty imported function name in %s\n", imp.dllName.c_str());
                    return false;
                }
            }
        }
        if (sections.size() > kMaxSections) {
            printf("Error: Too many sections: %zu\n", sections.size());
            return false;
        }
        printf("PE validation passed\n");
        return true;
    }

    bool EmitExecutable(const char* filename) {
        if (!filename || filename[0] == '\0') {
            printf("Error: Output filename is empty\n");
            return false;
        }
        if (!ValidatePE()) {
            return false;
        }

        if (sections.empty()) {
            return false;
        }
        if (fileAlignment == 0 || (fileAlignment & (fileAlignment - 1)) != 0) {
            printf("Error: fileAlignment must be power-of-two\n");
            return false;
        }
        if (sectionAlignment == 0 || (sectionAlignment & (sectionAlignment - 1)) != 0) {
            printf("Error: sectionAlignment must be power-of-two\n");
            return false;
        }
        if (sectionAlignment < fileAlignment) {
            printf("Error: sectionAlignment(%u) < fileAlignment(%u)\n", sectionAlignment, fileAlignment);
            return false;
        }

        bool autoEntryPoint = (entryPointRVA == 0);

        auto ensureSection = [&](const char* name, DWORD characteristics) -> Section* {
            for (auto& sec : sections) {
                if (_stricmp(sec.name.c_str(), name) == 0) {
                    sec.characteristics = characteristics;
                    return &sec;
                }
            }
            Section sec;
            sec.name = name;
            sec.characteristics = characteristics;
            sec.virtualAddress = 0;
            sec.rawAddress = 0;
            sec.emitter = nullptr;
            sections.push_back(sec);
            return &sections.back();
        };

        bool hasImports = !imports.empty();
        bool hasExports = !exports.empty();
        bool hasTLS = !tlsCallbacks.empty();
        bool hasResources = !resources.empty();
        bool hasExceptions = !exceptions.empty();
        bool hasDebug = !debugEntries.empty();
        if (hasImports) ensureSection(".idata", IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_WRITE | IMAGE_SCN_CNT_INITIALIZED_DATA);
        if (hasExports) ensureSection(".edata", IMAGE_SCN_MEM_READ | IMAGE_SCN_CNT_INITIALIZED_DATA);
        if (hasTLS) ensureSection(".tls", IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_WRITE | IMAGE_SCN_CNT_INITIALIZED_DATA);
        if (hasResources) ensureSection(".rsrc", IMAGE_SCN_MEM_READ | IMAGE_SCN_CNT_INITIALIZED_DATA);
        if (hasExceptions) ensureSection(".pdata", IMAGE_SCN_MEM_READ | IMAGE_SCN_CNT_INITIALIZED_DATA);
        if (hasDebug) ensureSection(".debug", IMAGE_SCN_MEM_READ | IMAGE_SCN_CNT_INITIALIZED_DATA);
        Section* relocSec = ensureSection(".reloc", IMAGE_SCN_MEM_READ | IMAGE_SCN_CNT_INITIALIZED_DATA);
        relocSec->data = BuildRelocTable();

        // Deterministic ordering of import/export metadata for reproducible builds.
        std::sort(imports.begin(), imports.end(), [](const Import& a, const Import& b) {
            return _stricmp(a.dllName.c_str(), b.dllName.c_str()) < 0;
        });
        for (auto& imp : imports) {
            std::sort(imp.functions.begin(), imp.functions.end(), [](const std::string& a, const std::string& b) {
                return _stricmp(a.c_str(), b.c_str()) < 0;
            });
        }
        std::sort(exports.begin(), exports.end(), [](const Export& a, const Export& b) {
            if (a.ordinal != b.ordinal) return a.ordinal < b.ordinal;
            return _stricmp(a.name.c_str(), b.name.c_str()) < 0;
        });

        // Generate DOS stub
        std::vector<BYTE> dosStub = GenerateDOSStub();

        // ── Calculate layout ──
        DWORD dosStubSize = (DWORD)dosStub.size();
        DWORD ntHeaderSize = sizeof(DWORD) + sizeof(IMAGE_FILE_HEADER) + sizeof(IMAGE_OPTIONAL_HEADER64);
        if (sections.size() > (std::numeric_limits<DWORD>::max)() / sizeof(IMAGE_SECTION_HEADER)) {
            printf("Error: Section header size overflow\n");
            return false;
        }
        DWORD sectionHeadersSize = static_cast<DWORD>(sections.size() * sizeof(IMAGE_SECTION_HEADER));
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
                if (rva > (std::numeric_limits<DWORD>::max)() - virtualSize ||
                    raw > (std::numeric_limits<DWORD>::max)() - rawSize) {
                    printf("Error: Section layout overflow for %s\n", sec.name.c_str());
                    return (std::numeric_limits<DWORD>::max)();
                }
                rva += virtualSize;
                raw += rawSize;
                printf("Section %s: RVA %u, raw %u, size %zu\n", sec.name.c_str(), sec.virtualAddress, sec.rawAddress, sec.data.size());
            }
            return rva;
        };

        printf("First section RVA: %u\n", AlignUp(alignedHeadersSize, sectionAlignment));
        DWORD currentRVA = layoutSections();
        if (currentRVA == (std::numeric_limits<DWORD>::max)()) {
            return false;
        }

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
            for (auto& sec : sections) {
                if (sec.name == ".idata") {
                    importTableRVA = sec.virtualAddress;
                    DWORD estimatedImportSize = CalculateImportTableSize();
                    std::vector<BYTE> importTableData = BuildImportTable(importTableRVA, iatRVAs);
                    if (importTableData.empty() && estimatedImportSize != sizeof(IMAGE_IMPORT_DESCRIPTOR)) {
                        printf("Error: BuildImportTable returned empty unexpectedly\n");
                        return false;
                    }
                    if (importTableData.size() > estimatedImportSize) {
                        printf("Error: Import table exceeds estimated size (%zu > %u)\n",
                               importTableData.size(), estimatedImportSize);
                        return false;
                    }
                    importTableSize = (DWORD)importTableData.size();
                    sec.data = importTableData;
                    currentRVA = layoutSections();
                    printf("Import table RVA: %u, size: %u\n", importTableRVA, importTableSize);
                    break;
                }
            }
        }

        // Build export table into .edata and refresh layout
        DWORD exportTableRVA = 0;
        DWORD exportTableSize = 0;
        if (!exports.empty()) {
            // Find .edata section
            for (auto& sec : sections) {
                if (sec.name == ".edata") {
                    exportTableRVA = sec.virtualAddress;
                    std::vector<BYTE> exportTableData = BuildExportTable(exportTableRVA);
                    exportTableSize = (DWORD)exportTableData.size();
                    sec.data = exportTableData;
                    currentRVA = layoutSections();
                    printf("Export table RVA: %u, size: %u\n", exportTableRVA, exportTableSize);
                    break;
                }
            }
        }

        // Build TLS table into .tls and refresh layout
        DWORD tlsTableRVA = 0;
        DWORD tlsTableSize = 0;
        if (!tlsCallbacks.empty()) {
            // Find .tls section
            for (auto& sec : sections) {
                if (sec.name == ".tls") {
                    tlsTableRVA = sec.virtualAddress;
                    std::vector<BYTE> tlsTableData = BuildTLSTable(tlsTableRVA);
                    tlsTableSize = (DWORD)tlsTableData.size();
                    sec.data = tlsTableData;
                    currentRVA = layoutSections();
                    printf("TLS table RVA: %u, size: %u\n", tlsTableRVA, tlsTableSize);
                    break;
                }
            }
        }

        // Build resource table into .rsrc and refresh layout
        DWORD resourceTableRVA = 0;
        DWORD resourceTableSize = 0;
        if (!resources.empty()) {
            // Find .rsrc section
            for (auto& sec : sections) {
                if (sec.name == ".rsrc") {
                    resourceTableRVA = sec.virtualAddress;
                    std::vector<BYTE> resourceTableData = BuildResourceTable(resourceTableRVA);
                    resourceTableSize = (DWORD)resourceTableData.size();
                    sec.data = resourceTableData;
                    currentRVA = layoutSections();
                    printf("Resource table RVA: %u, size: %u\n", resourceTableRVA, resourceTableSize);
                    break;
                }
            }
        }

        // Build exception table into .pdata and refresh layout
        DWORD exceptionTableRVA = 0;
        DWORD exceptionTableSize = 0;
        if (!exceptions.empty()) {
            // Find .pdata section
            for (auto& sec : sections) {
                if (sec.name == ".pdata") {
                    exceptionTableRVA = sec.virtualAddress;
                    std::vector<BYTE> exceptionTableData = BuildExceptionTable();
                    exceptionTableSize = (DWORD)exceptionTableData.size();
                    sec.data = exceptionTableData;
                    currentRVA = layoutSections();
                    printf("Exception table RVA: %u, size: %u\n", exceptionTableRVA, exceptionTableSize);
                    break;
                }
            }
        }

        // Build debug table into .debug and refresh layout
        DWORD debugTableRVA = 0;
        DWORD debugTableSize = 0;
        if (!debugEntries.empty()) {
            // Find .debug section
            for (auto& sec : sections) {
                if (sec.name == ".debug") {
                    debugTableRVA = sec.virtualAddress;
                    std::vector<BYTE> debugTableData = BuildDebugTable(debugTableRVA);
                    debugTableSize = (DWORD)debugTableData.size();
                    sec.data = debugTableData;
                    currentRVA = layoutSections();
                    printf("Debug table RVA: %u, size: %u\n", debugTableRVA, debugTableSize);
                    break;
                }
            }
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
        {
            char* sde = nullptr;
            size_t sdeLen = 0;
            errno_t envErr = _dupenv_s(&sde, &sdeLen, "SOURCE_DATE_EPOCH");
            if (envErr == 0 && sde && sde[0] != '\0') {
                unsigned long long epoch = _strtoui64(sde, nullptr, 10);
                fileHeader.TimeDateStamp = static_cast<DWORD>(epoch & 0xFFFFFFFFull);
            } else {
                fileHeader.TimeDateStamp = (DWORD)time(nullptr);
            }
            if (sde) free(sde);
        }
        fileHeader.SizeOfOptionalHeader = sizeof(IMAGE_OPTIONAL_HEADER64);
        fileHeader.Characteristics = IMAGE_FILE_EXECUTABLE_IMAGE | IMAGE_FILE_LARGE_ADDRESS_AWARE;

        IMAGE_OPTIONAL_HEADER64 optHeader = {};
        optHeader.Magic = IMAGE_NT_OPTIONAL_HDR64_MAGIC;
        optHeader.MajorLinkerVersion = 14;
        optHeader.MinorLinkerVersion = 0;
        optHeader.SizeOfCode = 0; // Will calculate
        optHeader.SizeOfInitializedData = 0; // Will calculate
        optHeader.AddressOfEntryPoint = entryPointRVA;
        optHeader.BaseOfCode = 0;
        for (const auto& sec : sections) {
            if ((sec.characteristics & IMAGE_SCN_CNT_CODE) != 0) {
                optHeader.BaseOfCode = sec.virtualAddress;
                break;
            }
        }
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
        if (exportTableRVA) {
            optHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress = exportTableRVA;
            optHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].Size = exportTableSize;
        }
        if (tlsTableRVA) {
            optHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_TLS].VirtualAddress = tlsTableRVA;
            optHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_TLS].Size = tlsTableSize;
        }
        if (resourceTableRVA) {
            optHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_RESOURCE].VirtualAddress = resourceTableRVA;
            optHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_RESOURCE].Size = resourceTableSize;
        }
        if (exceptionTableRVA) {
            optHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXCEPTION].VirtualAddress = exceptionTableRVA;
            optHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXCEPTION].Size = exceptionTableSize;
        }
        if (debugTableRVA) {
            optHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_DEBUG].VirtualAddress = debugTableRVA;
            optHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_DEBUG].Size = debugTableSize;
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
        if (fopen_s(&f, filename, "wb") != 0) {
            printf("Failed to open output file for write: %s\n", filename);
            return false;
        }

        // DOS header + stub
        if (fwrite(&dosHeader, sizeof(dosHeader), 1, f) != 1) { fclose(f); return false; }
        if (!dosStub.empty() && fwrite(dosStub.data(), dosStub.size(), 1, f) != 1) { fclose(f); return false; }

        // NT headers
        DWORD ntSignature = IMAGE_NT_SIGNATURE;
        if (fwrite(&ntSignature, sizeof(ntSignature), 1, f) != 1) { fclose(f); return false; }
        if (fwrite(&fileHeader, sizeof(fileHeader), 1, f) != 1) { fclose(f); return false; }
        if (fwrite(&optHeader, sizeof(optHeader), 1, f) != 1) { fclose(f); return false; }

        // Section headers
        for (const auto& sh : sectionHeaders) {
            if (fwrite(&sh, sizeof(sh), 1, f) != 1) { fclose(f); return false; }
        }

        // Pad to file alignment
        if (!PadFile(f, alignedHeadersSize)) {
            fclose(f);
            return false;
        }

        // Section data
        printf("Writing %zu sections\n", sections.size());
        for (const auto& sec : sections) {
            printf("Writing section %s, size %zu\n", sec.name.c_str(), sec.data.size());
            if (!sec.data.empty() && fwrite(sec.data.data(), sec.data.size(), 1, f) != 1) {
                fclose(f);
                return false;
            }
            DWORD nextPos = sec.rawAddress + AlignUp((DWORD)sec.data.size(), fileAlignment);
            printf("Padding to %u\n", nextPos);
            if (!PadFile(f, nextPos)) {
                fclose(f);
                return false;
            }
        }

        long finalPos = ftell(f);
        if (finalPos < 0) {
            fclose(f);
            return false;
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
        if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0 || (size_t)dos->e_lfanew >= fileData.size()) {
            fclose(f);
            return;
        }
        DWORD checksumOffset = dos->e_lfanew + 4 + sizeof(IMAGE_FILE_HEADER) + 64;
        if ((size_t)checksumOffset + sizeof(DWORD) > fileData.size()) {
            fclose(f);
            return;
        }

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
        if (fwrite(&checksum, sizeof(checksum), 1, f) != 1) {
            fclose(f);
            return;
        }
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

        fseek(f, 0, SEEK_END);
        long fileSize = ftell(f);
        if (fileSize <= 0) {
            printf("Invalid file size\n");
            fclose(f);
            return false;
        }
        fseek(f, 0, SEEK_SET);

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
        if (dosHeader.e_lfanew <= 0 || dosHeader.e_lfanew > fileSize - (long)sizeof(DWORD)) {
            printf("Invalid e_lfanew: %ld\n", (long)dosHeader.e_lfanew);
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
        if (fileHeader.NumberOfSections == 0 || fileHeader.NumberOfSections > kMaxSections) {
            printf("Invalid section count: %u\n", fileHeader.NumberOfSections);
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
        if (optHeader.NumberOfRvaAndSizes < 16) {
            printf("Insufficient NumberOfRvaAndSizes: %u\n", optHeader.NumberOfRvaAndSizes);
            fclose(f);
            return false;
        }
        if (optHeader.FileAlignment == 0 || (optHeader.FileAlignment & (optHeader.FileAlignment - 1)) != 0 ||
            optHeader.SectionAlignment == 0 || (optHeader.SectionAlignment & (optHeader.SectionAlignment - 1)) != 0) {
            printf("Invalid non-power-of-two alignment values\n");
            fclose(f);
            return false;
        }
        if (optHeader.SizeOfHeaders < fileHeader.SizeOfOptionalHeader + sizeof(IMAGE_FILE_HEADER) + sizeof(DWORD)) {
            printf("Invalid SizeOfHeaders: %u\n", optHeader.SizeOfHeaders);
            fclose(f);
            return false;
        }
        if (optHeader.SizeOfImage == 0 || (optHeader.SizeOfImage % optHeader.SectionAlignment) != 0) {
            printf("Invalid SizeOfImage alignment/value: %u\n", optHeader.SizeOfImage);
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
        for (const auto& sh : sectionHeaders) {
            if ((sh.VirtualAddress % optHeader.SectionAlignment) != 0) {
                printf("Section virtual address alignment violation\n");
                fclose(f);
                return false;
            }
            if (sh.SizeOfRawData > 0 && (sh.PointerToRawData % optHeader.FileAlignment) != 0) {
                printf("Section raw pointer alignment violation\n");
                fclose(f);
                return false;
            }
            if (sh.SizeOfRawData > 0 && (sh.SizeOfRawData % optHeader.FileAlignment) != 0) {
                printf("Section raw size alignment violation\n");
                fclose(f);
                return false;
            }
            if (sh.Misc.VirtualSize > 0 && sh.VirtualAddress > optHeader.SizeOfImage) {
                printf("Section virtual address exceeds SizeOfImage\n");
                fclose(f);
                return false;
            }
            DWORD vsize = sh.Misc.VirtualSize ? sh.Misc.VirtualSize : sh.SizeOfRawData;
            if (vsize > 0 && sh.VirtualAddress > (std::numeric_limits<DWORD>::max)() - vsize) {
                printf("Section virtual range overflow\n");
                fclose(f);
                return false;
            }
            if (vsize > 0 && (sh.VirtualAddress + vsize) > optHeader.SizeOfImage) {
                printf("Section virtual range exceeds SizeOfImage\n");
                fclose(f);
                return false;
            }
            bool nameAllZero = true;
            for (unsigned char c : sh.Name) {
                if (c != 0) { nameAllZero = false; break; }
            }
            if (nameAllZero) {
                printf("Section name is empty\n");
                fclose(f);
                return false;
            }
            DWORD rawSize = sh.SizeOfRawData;
            DWORD rawPtr = sh.PointerToRawData;
            if (rawSize > 0) {
                if (rawPtr < optHeader.SizeOfHeaders) {
                    printf("Section raw pointer overlaps headers\n");
                    fclose(f);
                    return false;
                }
                if ((ULONGLONG)rawPtr + (ULONGLONG)rawSize > (ULONGLONG)fileSize) {
                    printf("Section exceeds file bounds\n");
                    fclose(f);
                    return false;
                }
            }
        }
        std::vector<std::pair<DWORD, DWORD>> rawRanges;
        rawRanges.reserve(sectionHeaders.size());
        for (const auto& sh : sectionHeaders) {
            if (sh.SizeOfRawData == 0) continue;
            rawRanges.push_back({sh.PointerToRawData, sh.PointerToRawData + sh.SizeOfRawData});
        }
        std::sort(rawRanges.begin(), rawRanges.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
        for (size_t i = 1; i < rawRanges.size(); ++i) {
            if (rawRanges[i].first < rawRanges[i - 1].second) {
                printf("Overlapping section raw ranges detected\n");
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
        bool baseOfCodeValid = false;
        for (const auto& sh : sectionHeaders) {
            DWORD vsize = sh.Misc.VirtualSize ? sh.Misc.VirtualSize : sh.SizeOfRawData;
            if (optHeader.BaseOfCode >= sh.VirtualAddress && optHeader.BaseOfCode < sh.VirtualAddress + vsize) {
                baseOfCodeValid = (sh.Characteristics & IMAGE_SCN_CNT_CODE) != 0;
                break;
            }
        }
        if (!baseOfCodeValid) {
            printf("BaseOfCode does not map to a code section\n");
            fclose(f);
            return false;
        }

        auto rvaMapsToSection = [&](DWORD rva) -> bool {
            for (const auto& sh : sectionHeaders) {
                DWORD vsize = sh.Misc.VirtualSize ? sh.Misc.VirtualSize : sh.SizeOfRawData;
                if (rva >= sh.VirtualAddress && rva < sh.VirtualAddress + vsize) return true;
            }
            return false;
        };
        if (optHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress &&
            !rvaMapsToSection(optHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress)) {
            printf("Import directory RVA is outside all sections\n");
            fclose(f);
            return false;
        }
        if (optHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].Size > 0 &&
            optHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress == 0) {
            printf("Import directory size set but RVA is zero\n");
            fclose(f);
            return false;
        }
        if (optHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].VirtualAddress &&
            !rvaMapsToSection(optHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].VirtualAddress)) {
            printf("Reloc directory RVA is outside all sections\n");
            fclose(f);
            return false;
        }
        if (optHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].Size > 0 &&
            optHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].VirtualAddress == 0) {
            printf("Reloc directory size set but RVA is zero\n");
            fclose(f);
            return false;
        }

        printf("PE validation passed\n");
        fclose(f);
        return true;
    }

    DWORD AlignUp(DWORD value, DWORD alignment) {
        if (alignment == 0) return value;
        if ((alignment & (alignment - 1)) != 0) {
            DWORD rem = value % alignment;
            return rem ? (value + (alignment - rem)) : value;
        }
        if (value > (std::numeric_limits<DWORD>::max)() - (alignment - 1)) {
            return (std::numeric_limits<DWORD>::max)();
        }
        return (value + alignment - 1) & ~(alignment - 1);
    }

    bool PadFile(FILE* f, DWORD targetOffset) {
        long current = ftell(f);
        if (current < 0) return false;
        while (current < (long)targetOffset) {
            if (fputc(0, f) == EOF) return false;
            current++;
        }
        return true;
    }

    DWORD CalculateImportTableSize() {
        size_t activeImportCount = 0;
        for (const auto& imp : imports) {
            if (!imp.functions.empty()) ++activeImportCount;
        }
        DWORD size = (DWORD)((activeImportCount + 1) * sizeof(IMAGE_IMPORT_DESCRIPTOR));
        for (const auto& imp : imports) {
            if (imp.functions.empty()) continue;
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

    std::vector<BYTE> BuildExportTable(DWORD baseRVA) {
        std::vector<BYTE> data;
        if (exports.empty()) return data;

        // IMAGE_EXPORT_DIRECTORY
        IMAGE_EXPORT_DIRECTORY exportDir = {};
        exportDir.NumberOfFunctions = (DWORD)exports.size();
        exportDir.NumberOfNames = (DWORD)exports.size();
        exportDir.Base = 1;  // Ordinal base

        DWORD currentRVA = baseRVA + sizeof(IMAGE_EXPORT_DIRECTORY);

        // AddressOfFunctions
        exportDir.AddressOfFunctions = currentRVA;
        currentRVA += (DWORD)exports.size() * sizeof(DWORD);

        // AddressOfNames
        exportDir.AddressOfNames = currentRVA;
        currentRVA += (DWORD)exports.size() * sizeof(DWORD);

        // AddressOfNameOrdinals
        exportDir.AddressOfNameOrdinals = currentRVA;
        currentRVA += (DWORD)exports.size() * sizeof(WORD);

        // Name RVA (point to a dummy name)
        exportDir.Name = currentRVA;
        const char* dllName = "RawrXD.dll";
        currentRVA += (DWORD)strlen(dllName) + 1;

        // Export address table
        for (const auto& exp : exports) {
            DWORD rva = exp.rva;
            data.insert(data.end(), (BYTE*)&rva, (BYTE*)&rva + sizeof(DWORD));
        }

        // Name pointers
        DWORD nameRVA = currentRVA;
        for (const auto& exp : exports) {
            data.insert(data.end(), (BYTE*)&nameRVA, (BYTE*)&nameRVA + sizeof(DWORD));
            nameRVA += (DWORD)exp.name.size() + 1;
        }

        // Name ordinals
        for (size_t i = 0; i < exports.size(); ++i) {
            WORD ordinal = (WORD)i;
            data.insert(data.end(), (BYTE*)&ordinal, (BYTE*)&ordinal + sizeof(WORD));
        }

        // DLL name
        data.insert(data.end(), dllName, dllName + strlen(dllName) + 1);

        // Export names
        for (const auto& exp : exports) {
            data.insert(data.end(), exp.name.begin(), exp.name.end());
            data.push_back(0);
        }

        // Prepend export directory
        std::vector<BYTE> result;
        result.insert(result.end(), (BYTE*)&exportDir, (BYTE*)&exportDir + sizeof(exportDir));
        result.insert(result.end(), data.begin(), data.end());

        return result;
    }

    std::vector<BYTE> BuildTLSTable(DWORD baseRVA) {
        std::vector<BYTE> data;
        if (tlsCallbacks.empty()) return data;

        // IMAGE_TLS_DIRECTORY64
        IMAGE_TLS_DIRECTORY64 tlsDir = {};
        tlsDir.AddressOfCallBacks = imageBase + baseRVA + sizeof(IMAGE_TLS_DIRECTORY64);

        // Callbacks array
        for (const auto& cb : tlsCallbacks) {
            ULONGLONG callbackRVA = cb.rva;
            data.insert(data.end(), (BYTE*)&callbackRVA, (BYTE*)&callbackRVA + sizeof(ULONGLONG));
        }
        // Null terminator
        ULONGLONG nullCallback = 0;
        data.insert(data.end(), (BYTE*)&nullCallback, (BYTE*)&nullCallback + sizeof(ULONGLONG));

        // Prepend TLS directory
        std::vector<BYTE> result;
        result.insert(result.end(), (BYTE*)&tlsDir, (BYTE*)&tlsDir + sizeof(tlsDir));
        result.insert(result.end(), data.begin(), data.end());

        return result;
    }

    std::vector<BYTE> BuildResourceTable(DWORD baseRVA) {
        std::vector<BYTE> data;
        if (resources.empty()) return data;

        // IMAGE_RESOURCE_DIRECTORY root
        IMAGE_RESOURCE_DIRECTORY rootDir = {};
        rootDir.NumberOfIdEntries = (WORD)resources.size();

        DWORD currentRVA = baseRVA + sizeof(IMAGE_RESOURCE_DIRECTORY);

        // Resource directory entries
        std::vector<IMAGE_RESOURCE_DIRECTORY_ENTRY> entries;
        for (size_t i = 0; i < resources.size(); ++i) {
            IMAGE_RESOURCE_DIRECTORY_ENTRY entry = {};
            entry.Id = resources[i].type;
            entry.OffsetToData = currentRVA - baseRVA;
            entries.push_back(entry);

            // Resource data entry
            IMAGE_RESOURCE_DATA_ENTRY dataEntry = {};
            dataEntry.OffsetToData = currentRVA + sizeof(IMAGE_RESOURCE_DATA_ENTRY);
            dataEntry.Size = (DWORD)resources[i].data.size();
            data.insert(data.end(), (BYTE*)&dataEntry, (BYTE*)&dataEntry + sizeof(dataEntry));

            // Resource data
            data.insert(data.end(), resources[i].data.begin(), resources[i].data.end());

            currentRVA += sizeof(IMAGE_RESOURCE_DATA_ENTRY) + (DWORD)resources[i].data.size();
        }

        // Prepend root directory and entries
        std::vector<BYTE> result;
        result.insert(result.end(), (BYTE*)&rootDir, (BYTE*)&rootDir + sizeof(rootDir));
        for (const auto& entry : entries) {
            result.insert(result.end(), (BYTE*)&entry, (BYTE*)&entry + sizeof(entry));
        }
        result.insert(result.end(), data.begin(), data.end());

        return result;
    }

    std::vector<BYTE> BuildExceptionTable() {
        std::vector<BYTE> data;
        if (exceptions.empty()) return data;

        // RUNTIME_FUNCTION entries
        for (const auto& exc : exceptions) {
            RUNTIME_FUNCTION rf = {};
            rf.BeginAddress = exc.startRVA;
            rf.EndAddress = exc.endRVA;
            rf.UnwindData = exc.unwindRVA;
            data.insert(data.end(), (BYTE*)&rf, (BYTE*)&rf + sizeof(rf));
        }

        return data;
    }

    std::vector<BYTE> BuildDebugTable(DWORD baseRVA) {
        std::vector<BYTE> data;
        if (debugEntries.empty()) return data;

        // IMAGE_DEBUG_DIRECTORY entries
        for (const auto& dbg : debugEntries) {
            IMAGE_DEBUG_DIRECTORY debugDir = {};
            debugDir.Type = dbg.type;
            debugDir.SizeOfData = (DWORD)dbg.data.size();
            debugDir.AddressOfRawData = baseRVA + sizeof(IMAGE_DEBUG_DIRECTORY) * (DWORD)debugEntries.size();
            debugDir.PointerToRawData = debugDir.AddressOfRawData;

            data.insert(data.end(), (BYTE*)&debugDir, (BYTE*)&debugDir + sizeof(debugDir));
        }

        // Debug data
        for (const auto& dbg : debugEntries) {
            data.insert(data.end(), dbg.data.begin(), dbg.data.end());
        }

        return data;
    }

    std::vector<BYTE> BuildRelocTable() {
        std::vector<BYTE> data;

        // Group relocations by page (4KB pages)
        std::map<DWORD, std::vector<Relocation>> pageRelocs;
        for (const auto& rel : relocations) {
            DWORD page = rel.rva & ~0xFFF;  // 4KB page alignment
            pageRelocs[page].push_back(rel);
        }

        // Build IMAGE_BASE_RELOCATION blocks
        for (const auto& pr : pageRelocs) {
            DWORD pageRVA = pr.first;
            const auto& rels = pr.second;
            std::vector<WORD> entries;
            for (const auto& rel : rels) {
                WORD entry = (rel.rva & 0xFFF) | (rel.type << 12);
                entries.push_back(entry);
            }
            // IMAGE_BASE_RELOCATION blocks must be DWORD-aligned.
            if (entries.size() & 1) {
                entries.push_back(0);  // IMAGE_REL_BASED_ABSOLUTE padding
            }
            DWORD blockSize = static_cast<DWORD>(sizeof(DWORD) + sizeof(DWORD) + entries.size() * sizeof(WORD));
            data.insert(data.end(), (BYTE*)&pageRVA, (BYTE*)&pageRVA + sizeof(DWORD));
            data.insert(data.end(), (BYTE*)&blockSize, (BYTE*)&blockSize + sizeof(DWORD));
            for (WORD entry : entries) {
                data.insert(data.end(), (BYTE*)&entry, (BYTE*)&entry + sizeof(WORD));
            }
        }

        // If no custom relocations, add a dummy one for image base
        if (data.empty()) {
            DWORD pageRVA = 0;
            DWORD blockSize = 12;
            data.insert(data.end(), (BYTE*)&pageRVA, (BYTE*)&pageRVA + 4);
            data.insert(data.end(), (BYTE*)&blockSize, (BYTE*)&blockSize + 4);
            WORD relocEntry = 0 | (IMAGE_REL_BASED_DIR64 << 12);
            data.insert(data.end(), (BYTE*)&relocEntry, (BYTE*)&relocEntry + 2);
            WORD padEntry = 0;  // ABSOLUTE padding to satisfy declared blockSize.
            data.insert(data.end(), (BYTE*)&padEntry, (BYTE*)&padEntry + 2);
        }

        return data;
    }

    std::vector<BYTE> BuildImportTable(DWORD baseRVA, std::vector<DWORD>& outIATRVAs) {
        std::vector<BYTE> data;
        outIATRVAs.clear();

        size_t activeImportCount = 0;
        size_t totalFunctionCount = 0;
        for (const auto& imp : imports) {
            if (!imp.functions.empty()) {
                ++activeImportCount;
                totalFunctionCount += imp.functions.size();
            }
        }
        if (activeImportCount == 0) {
            IMAGE_IMPORT_DESCRIPTOR nullDesc = {};
            data.insert(data.end(), (BYTE*)&nullDesc, (BYTE*)&nullDesc + sizeof(nullDesc));
            return data;
        }
        outIATRVAs.reserve(totalFunctionCount);

        auto addRvaChecked = [](DWORD& dst, DWORD add) -> bool {
            if (dst > (std::numeric_limits<DWORD>::max)() - add) {
                return false;
            }
            dst += add;
            return true;
        };
        DWORD currentRVA = baseRVA + (DWORD)((activeImportCount + 1) * sizeof(IMAGE_IMPORT_DESCRIPTOR));

        // Calculate RVAs for each part
        std::map<std::string, DWORD> dllNameRVAs;
        std::map<std::string, std::vector<DWORD>> iltRVAs, iatRVAs, hintNameRVAs;

        for (const auto& imp : imports) {
            if (imp.functions.empty()) continue;
            dllNameRVAs[imp.dllName] = currentRVA;
            DWORD dllSpan = AlignUp((DWORD)imp.dllName.size() + 1, 2);
            if (!addRvaChecked(currentRVA, dllSpan)) return {};

            iltRVAs[imp.dllName].resize(imp.functions.size());
            iatRVAs[imp.dllName].resize(imp.functions.size());
            hintNameRVAs[imp.dllName].resize(imp.functions.size());

            for (size_t i = 0; i < imp.functions.size(); ++i) {
                iltRVAs[imp.dllName][i] = currentRVA;
                if (!addRvaChecked(currentRVA, sizeof(IMAGE_THUNK_DATA64))) return {};
            }
            if (!addRvaChecked(currentRVA, sizeof(IMAGE_THUNK_DATA64))) return {}; // ILT null terminator

            for (size_t i = 0; i < imp.functions.size(); ++i) {
                iatRVAs[imp.dllName][i] = currentRVA;
                outIATRVAs.push_back(currentRVA);
                if (!addRvaChecked(currentRVA, sizeof(IMAGE_THUNK_DATA64))) return {};
            }
            if (!addRvaChecked(currentRVA, sizeof(IMAGE_THUNK_DATA64))) return {}; // IAT null terminator

            for (size_t i = 0; i < imp.functions.size(); ++i) {
                hintNameRVAs[imp.dllName][i] = currentRVA;
                DWORD hintNameSize = sizeof(WORD) + (DWORD)imp.functions[i].size() + 1;
                if (!addRvaChecked(currentRVA, AlignUp(hintNameSize, 2))) return {};
            }
        }

        // Build descriptors
        for (const auto& imp : imports) {
            if (imp.functions.empty()) continue;
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
            if (imp.functions.empty()) continue;
            data.insert(data.end(), imp.dllName.begin(), imp.dllName.end());
            data.push_back(0);
            if (data.size() % 2) data.push_back(0);  // Align to WORD
        }

        // ILT
        for (const auto& imp : imports) {
            if (imp.functions.empty()) continue;
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
            if (imp.functions.empty()) continue;
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
            if (imp.functions.empty()) continue;
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


#ifndef RAWRXD_PE_WRITER_NO_MAIN
int main() {
    PEWriter writer;
    MachineCodeEmitter emitter;
    emitter.FunctionPrologue(0, 4, false);
    emitter.MOV_RCX_IMM(0);
    emitter.CALL_IMPORT(0);
    emitter.RET();

    writer.AddSection(".text", emitter.Data(), emitter.Size(),
                      IMAGE_SCN_MEM_EXECUTE | IMAGE_SCN_MEM_READ | IMAGE_SCN_CNT_CODE, &emitter);
    writer.AddImport("kernel32.dll", "ExitProcess");
    writer.SetSubsystem(IMAGE_SUBSYSTEM_WINDOWS_CUI);

    char outPath[MAX_PATH] = {};
    if (GetModuleFileNameA(nullptr, outPath, MAX_PATH) == 0) return 1;
    char* slash = strrchr(outPath, '\\');
    if (!slash) slash = strrchr(outPath, '/');
    if (!slash) return 1;
    slash[1] = '\0';
    strcat_s(outPath, "test.exe");
    return writer.EmitExecutable(outPath) ? 0 : 1;
}
#endif
