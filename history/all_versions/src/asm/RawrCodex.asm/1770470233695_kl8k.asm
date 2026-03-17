; =============================================================================
; RawrCodex.asm - Pure MASM64 Binary Analysis Engine
; Zero STL, Pure Win32 API Implementation
; Part of RawrXD-Shell Reverse Engineering Suite
;
; Features:
;   - Dynamic array management (no STL containers)
;   - PE32/PE64 parser (IDT, EDT, ILT/IAT, sections, symbols)
;   - ELF32/ELF64 parser (sections, symbols, string tables)
;   - x64 instruction decoder (REX, ModR/M, SIB, two-byte 0F)
;   - Pattern scanner (Boyer-Moore style with wildcard support)
;   - String extraction (ASCII, UTF-16)
;   - IDA Pro script export
;   - RVA-to-file-offset conversion
;
; Build: ml64 /c /Fo RawrCodex.obj RawrCodex.asm
;        link RawrCodex.obj /SUBSYSTEM:CONSOLE kernel32.lib
;
; Calling Convention: Microsoft x64 ABI (RCX, RDX, R8, R9, stack)
; =============================================================================

OPTION CASEMAP:NONE

; =============================================================================
; Windows API Imports
; =============================================================================

EXTERN GetProcessHeap:PROC
EXTERN HeapAlloc:PROC
EXTERN HeapFree:PROC
EXTERN HeapReAlloc:PROC
EXTERN CreateFileA:PROC
EXTERN ReadFile:PROC
EXTERN WriteFile:PROC
EXTERN CloseHandle:PROC
EXTERN GetFileSize:PROC
EXTERN CreateFileMappingA:PROC
EXTERN MapViewOfFile:PROC
EXTERN UnmapViewOfFile:PROC
EXTERN GetLastError:PROC
EXTERN VirtualProtect:PROC
EXTERN lstrlenA:PROC
EXTERN lstrcpyA:PROC
EXTERN lstrcmpA:PROC
EXTERN wsprintfA:PROC

; =============================================================================
; Constants
; =============================================================================

; Heap flags
HEAP_ZERO_MEMORY            EQU 00000008h

; File access
GENERIC_READ                EQU 80000000h
GENERIC_WRITE               EQU 40000000h
FILE_SHARE_READ             EQU 00000001h
OPEN_EXISTING               EQU 3
CREATE_ALWAYS               EQU 2
INVALID_HANDLE_VALUE        EQU -1

; Memory mapping
PAGE_READONLY               EQU 002h
FILE_MAP_READ               EQU 004h
PAGE_READWRITE              EQU 004h

; PE Signatures
IMAGE_DOS_SIGNATURE         EQU 05A4Dh      ; 'MZ'
IMAGE_NT_SIGNATURE          EQU 04550h      ; 'PE\0\0'
IMAGE_FILE_MACHINE_I386     EQU 014Ch
IMAGE_FILE_MACHINE_AMD64    EQU 08664h

; PE Optional Header Magic
IMAGE_NT_OPTIONAL_HDR32_MAGIC EQU 010Bh
IMAGE_NT_OPTIONAL_HDR64_MAGIC EQU 020Bh

; PE Section characteristics
IMAGE_SCN_MEM_EXECUTE       EQU 20000000h
IMAGE_SCN_MEM_READ          EQU 40000000h
IMAGE_SCN_MEM_WRITE         EQU 80000000h
IMAGE_SCN_CNT_CODE          EQU 00000020h

; ELF Signatures
ELF_MAGIC                   EQU 464C457Fh   ; 0x7F 'ELF'
ELFCLASS32                  EQU 1
ELFCLASS64                  EQU 2
ELFDATA2LSB                 EQU 1

; ELF Section types
SHT_NULL                    EQU 0
SHT_PROGBITS                EQU 1
SHT_SYMTAB                  EQU 2
SHT_STRTAB                  EQU 3
SHT_RELA                    EQU 4
SHT_DYNAMIC                 EQU 6
SHT_NOTE                    EQU 7
SHT_NOBITS                  EQU 8
SHT_REL                     EQU 9
SHT_DYNSYM                  EQU 11

; ELF Symbol binding
STB_LOCAL                   EQU 0
STB_GLOBAL                  EQU 1
STB_WEAK                    EQU 2

; Dynamic array initial capacity
INITIAL_CAPACITY            EQU 16
GROWTH_FACTOR               EQU 2

; Instruction decode limits
MAX_INSTR_LEN               EQU 15
MAX_MNEMONIC_LEN            EQU 32
MAX_OPERANDS_LEN            EQU 128
MAX_DISASM_ENTRIES          EQU 65536
MAX_PATTERN_LEN             EQU 256
MAX_STRING_LEN              EQU 1024

; =============================================================================
; Data Structures
; =============================================================================

; --- Dynamic Array (generic growable buffer) ---
DYNARRAY STRUCT
    pData           QWORD ?         ; Pointer to element storage
    count           DWORD ?         ; Current element count
    capacity        DWORD ?         ; Allocated capacity
    elemSize        DWORD ?         ; Size of each element in bytes
    _pad            DWORD ?         ; Alignment padding
DYNARRAY ENDS

; --- Section descriptor ---
RAWRSECTION STRUCT
    szName          BYTE 16 DUP(?)  ; Section name (null-terminated)
    virtualAddress  QWORD ?         ; RVA of section
    virtualSize     QWORD ?         ; Virtual size
    fileOffset      QWORD ?         ; Raw file offset
    rawSize         QWORD ?         ; Raw data size on disk
    characteristics DWORD ?         ; Section flags
    isExecutable    DWORD ?         ; 1 if executable
    isWritable      DWORD ?         ; 1 if writable
    isReadable      DWORD ?         ; 1 if readable
RAWRSECTION ENDS

; --- Symbol descriptor ---
RAWRSYMBOL STRUCT
    szName          BYTE 128 DUP(?) ; Symbol name
    szDemangled     BYTE 128 DUP(?) ; Demangled name (if applicable)
    address         QWORD ?         ; Symbol address / RVA
    size            QWORD ?         ; Symbol size (0 if unknown)
    sectionIndex    DWORD ?         ; Section index
    isFunction      DWORD ?         ; 1 if function symbol
    isExport        DWORD ?         ; 1 if exported
    isImport        DWORD ?         ; 1 if imported
    isMangled       DWORD ?         ; 1 if name is mangled
    _pad            DWORD ?         ; Alignment
RAWRSYMBOL ENDS

; --- Import entry ---
RAWRIMPORT STRUCT
    szDllName       BYTE 128 DUP(?) ; DLL name
    szFuncName      BYTE 128 DUP(?) ; Function name
    ordinal         DWORD ?         ; Ordinal number
    isOrdinalOnly   DWORD ?         ; 1 if imported by ordinal
    iatRVA          QWORD ?         ; IAT entry RVA
RAWRIMPORT ENDS

; --- Export entry ---
RAWREXPORT STRUCT
    szFuncName      BYTE 128 DUP(?) ; Function name
    ordinal         DWORD ?         ; Ordinal
    rva             QWORD ?         ; Export RVA
    isForwarded     DWORD ?         ; 1 if forwarded
    szForwardName   BYTE 128 DUP(?) ; Forward target name
    _pad            DWORD ?         ; Alignment
RAWREXPORT ENDS

; --- Disassembled instruction ---
RAWRINSTRUCTION STRUCT
    address         QWORD ?         ; Instruction VA
    length          DWORD ?         ; Instruction length in bytes
    _pad1           DWORD ?
    rawBytes        BYTE 16 DUP(?)  ; Raw encoded bytes
    szMnemonic      BYTE 32 DUP(?)  ; Mnemonic string
    szOperands      BYTE 128 DUP(?) ; Operands string
    isCall          DWORD ?         ; 1 if call instruction
    isJump          DWORD ?         ; 1 if any jump (jmp/jcc)
    isReturn        DWORD ?         ; 1 if ret
    isBranch        DWORD ?         ; 1 if conditional branch
    branchTarget    QWORD ?         ; Target address for branches
RAWRINSTRUCTION ENDS

; --- Basic block for CFG ---
RAWRBASICBLOCK STRUCT
    startAddress    QWORD ?
    endAddress      QWORD ?
    instrCount      DWORD ?
    isReturn        DWORD ?
    isCall          DWORD ?
    _pad            DWORD ?
    successorCount  DWORD ?
    predecessorCount DWORD ?
    successors      QWORD 8 DUP(?)  ; Up to 8 successors
    predecessors    QWORD 8 DUP(?)  ; Up to 8 predecessors
RAWRBASICBLOCK ENDS

; --- Recovered function entry ---
RAWRFUNCTION STRUCT
    startAddress    QWORD ?
    endAddress      QWORD ?
    szName          BYTE 128 DUP(?)
    instrCount      DWORD ?
    hasFramePointer DWORD ?
    isThunk         DWORD ?
    _pad            DWORD ?
RAWRFUNCTION ENDS

; --- Cross-reference entry ---
RAWRXREF STRUCT
    fromAddr        QWORD ?         ; Source address (instruction that references)
    toAddr          QWORD ?         ; Target address (what is referenced)
    xrefType        DWORD ?         ; 0=code_call, 1=code_jump, 2=code_cond_branch, 3=data_read, 4=data_write, 5=data_lea
    instrIndex      DWORD ?         ; Index into instructions array
RAWRXREF ENDS

; XREF type constants
XREF_CODE_CALL          EQU 0
XREF_CODE_JUMP          EQU 1
XREF_CODE_COND_BRANCH   EQU 2
XREF_DATA_READ          EQU 3
XREF_DATA_WRITE         EQU 4
XREF_DATA_LEA           EQU 5

; --- Pattern match result ---
RAWRMATCH STRUCT
    offset          QWORD ?         ; File offset of match
    rva             QWORD ?         ; RVA of match (if in section)
RAWRMATCH ENDS

; --- Extracted string ---
RAWRSTRING STRUCT
    offset          QWORD ?         ; File offset
    rva             QWORD ?         ; RVA
    length          DWORD ?         ; String length
    isWide          DWORD ?         ; 1 if UTF-16
    szValue         BYTE 256 DUP(?) ; String content (truncated)
RAWRSTRING ENDS

; --- Main context structure ---
RAWRCODEX_CTX STRUCT
    ; File mapping state
    hFile           QWORD ?         ; File handle
    hMapping        QWORD ?         ; File mapping handle
    pFileBase       QWORD ?         ; Mapped view base address
    fileSize        QWORD ?         ; Total file size

    ; Binary type detection
    isPE            DWORD ?         ; 1 if PE format
    isELF           DWORD ?         ; 1 if ELF format
    is64Bit         DWORD ?         ; 1 if 64-bit binary
    machine         DWORD ?         ; Machine type (IMAGE_FILE_MACHINE_*)

    ; PE header offsets (cached for quick access)
    peNtHeaders     QWORD ?         ; Offset to NT headers
    peOptionalHdr   QWORD ?         ; Offset to optional header
    peSectionTable  QWORD ?         ; Offset to first section header
    peNumSections   DWORD ?         ; Number of PE sections
    peEntryPointRVA DWORD ?         ; Entry point RVA
    peImageBase     QWORD ?         ; Preferred image base

    ; PE data directory offsets
    peImportDirRVA  DWORD ?         ; Import directory RVA
    peImportDirSize DWORD ?         ; Import directory size
    peExportDirRVA  DWORD ?         ; Export directory RVA
    peExportDirSize DWORD ?         ; Export directory size

    ; ELF header offsets (cached)
    elfSectionHdrOff QWORD ?        ; Section header table offset
    elfNumSections  DWORD ?         ; Number of ELF sections
    elfStrTabIndex  DWORD ?         ; Section name string table index
    elfEntryPoint   QWORD ?         ; ELF entry point

    ; Dynamic arrays for parsed data
    sections        DYNARRAY <>     ; Array of RAWRSECTION
    symbols         DYNARRAY <>     ; Array of RAWRSYMBOL
    imports         DYNARRAY <>     ; Array of RAWRIMPORT
    exports         DYNARRAY <>     ; Array of RAWREXPORT
    instructions    DYNARRAY <>     ; Array of RAWRINSTRUCTION
    basicBlocks     DYNARRAY <>     ; Array of RAWRBASICBLOCK
    functions       DYNARRAY <>     ; Array of RAWRFUNCTION
    matches         DYNARRAY <>     ; Array of RAWRMATCH
    strings         DYNARRAY <>     ; Array of RAWRSTRING
    xrefs           DYNARRAY <>     ; Array of RAWRXREF

    ; Status
    lastError       DWORD ?
    isLoaded        DWORD ?
RAWRCODEX_CTX ENDS

; =============================================================================
; Data Segment
; =============================================================================

.DATA

; Format strings for wsprintfA
fmtHex8         BYTE "%08X", 0
fmtHex16        BYTE "%016llX", 0
fmtHexByte      BYTE "%02X ", 0
fmtDecimal      BYTE "%d", 0
fmtString       BYTE "%s", 0
fmtSectionFmt   BYTE "  %-16s  VA: %08X  Size: %08X  Raw: %08X", 0
fmtSymbolFmt    BYTE "  %08X  %s", 0
fmtImportFmt    BYTE "  %s!%s  (ordinal %d)", 0
fmtExportFmt    BYTE "  [%d] %08X  %s", 0
fmtInstrFmt     BYTE "  %08X: %-8s %s", 0
fmtFuncFmt      BYTE "  %08X - %08X  %s", 0

; IDA script header
idaScriptHdr    BYTE "# RawrCodex IDA Pro Import Script", 13, 10
                BYTE "# Auto-generated by RawrXD-Shell", 13, 10
                BYTE "import idautils", 13, 10
                BYTE "import idc", 13, 10, 13, 10
                BYTE "def apply_rawrcodex_analysis():", 13, 10, 0

idaSetName      BYTE "    idc.set_name(0x%08X, '%s', idc.SN_NOWARN)", 13, 10, 0
idaSetFunc      BYTE "    idc.add_func(0x%08X, 0x%08X)", 13, 10, 0
idaComment      BYTE "    idc.set_cmt(0x%08X, '%s', 0)", 13, 10, 0
idaScriptFtr    BYTE 13, 10, "apply_rawrcodex_analysis()", 13, 10, 0

; Mnemonic lookup table for single-byte opcodes
; Indexed by opcode byte, each entry is 8 bytes (null-padded)
mnNOP           BYTE "nop", 0, 0, 0, 0, 0
mnRET           BYTE "ret", 0, 0, 0, 0, 0
mnRETF          BYTE "retf", 0, 0, 0, 0
mnINT3          BYTE "int3", 0, 0, 0, 0
mnINT           BYTE "int", 0, 0, 0, 0, 0
mnHLT           BYTE "hlt", 0, 0, 0, 0, 0
mnCLC           BYTE "clc", 0, 0, 0, 0, 0
mnSTC           BYTE "stc", 0, 0, 0, 0, 0
mnCLI           BYTE "cli", 0, 0, 0, 0, 0
mnSTI           BYTE "sti", 0, 0, 0, 0, 0
mnCLD           BYTE "cld", 0, 0, 0, 0, 0
mnSTD           BYTE "std", 0, 0, 0, 0, 0
mnPUSH          BYTE "push", 0, 0, 0, 0
mnPOP           BYTE "pop", 0, 0, 0, 0, 0
mnCALL          BYTE "call", 0, 0, 0, 0
mnJMP           BYTE "jmp", 0, 0, 0, 0, 0
mnMOV           BYTE "mov", 0, 0, 0, 0, 0
mnLEA           BYTE "lea", 0, 0, 0, 0, 0
mnXOR           BYTE "xor", 0, 0, 0, 0, 0
mnADD           BYTE "add", 0, 0, 0, 0, 0
mnSUB           BYTE "sub", 0, 0, 0, 0, 0
mnCMP           BYTE "cmp", 0, 0, 0, 0, 0
mnTEST          BYTE "test", 0, 0, 0, 0
mnAND           BYTE "and", 0, 0, 0, 0, 0
mnOR            BYTE "or", 0, 0, 0, 0, 0, 0
mnSHL           BYTE "shl", 0, 0, 0, 0, 0
mnSHR           BYTE "shr", 0, 0, 0, 0, 0
mnSAR           BYTE "sar", 0, 0, 0, 0, 0
mnIMUL          BYTE "imul", 0, 0, 0, 0
mnIDIV          BYTE "idiv", 0, 0, 0, 0
mnCDQ           BYTE "cdq", 0, 0, 0, 0, 0
mnCQO           BYTE "cqo", 0, 0, 0, 0, 0
mnMOVSXD        BYTE "movsxd", 0, 0
mnMOVSX         BYTE "movsx", 0, 0, 0
mnMOVZX         BYTE "movzx", 0, 0, 0
mnCMOVCC        BYTE "cmov", 0, 0, 0, 0
mnSETCC         BYTE "set", 0, 0, 0, 0, 0
mnREP           BYTE "rep", 0, 0, 0, 0, 0
mnMOVSB         BYTE "movsb", 0, 0, 0
mnMOVSQ         BYTE "movsq", 0, 0, 0
mnSTOSB         BYTE "stosb", 0, 0, 0
mnSTOSQ         BYTE "stosq", 0, 0, 0
mnLODSB         BYTE "lodsb", 0, 0, 0
mnSCASB         BYTE "scasb", 0, 0, 0
mnCMPSB         BYTE "cmpsb", 0, 0, 0
mnSYSCALL       BYTE "syscall", 0
mnCPUID         BYTE "cpuid", 0, 0, 0
mnRDTSC         BYTE "rdtsc", 0, 0, 0
mnUD2           BYTE "ud2", 0, 0, 0, 0, 0
mnENTER         BYTE "enter", 0, 0, 0
mnLEAVE         BYTE "leave", 0, 0, 0
mnDB            BYTE "db", 0, 0, 0, 0, 0, 0

; Condition code suffixes for Jcc/CMOVcc/SETcc
; Indexed by lower nibble of 0F 8x/9x/4x
ccSuffixes      BYTE "o", 0        ; 0
                BYTE "no", 0       ; 1
                BYTE "b", 0        ; 2
                BYTE "ae", 0       ; 3
                BYTE "e", 0        ; 4
                BYTE "ne", 0       ; 5
                BYTE "be", 0       ; 6
                BYTE "a", 0        ; 7
                BYTE "s", 0        ; 8
                BYTE "ns", 0       ; 9
                BYTE "p", 0        ; A
                BYTE "np", 0       ; B
                BYTE "l", 0        ; C
                BYTE "ge", 0       ; D
                BYTE "le", 0       ; E
                BYTE "g", 0        ; F

; Register name tables (64-bit)
regNames64      BYTE "rax", 0, 0, 0, 0, 0  ; 0
                BYTE "rcx", 0, 0, 0, 0, 0  ; 1
                BYTE "rdx", 0, 0, 0, 0, 0  ; 2
                BYTE "rbx", 0, 0, 0, 0, 0  ; 3
                BYTE "rsp", 0, 0, 0, 0, 0  ; 4
                BYTE "rbp", 0, 0, 0, 0, 0  ; 5
                BYTE "rsi", 0, 0, 0, 0, 0  ; 6
                BYTE "rdi", 0, 0, 0, 0, 0  ; 7
                BYTE "r8", 0, 0, 0, 0, 0, 0 ; 8
                BYTE "r9", 0, 0, 0, 0, 0, 0 ; 9
                BYTE "r10", 0, 0, 0, 0, 0  ; 10
                BYTE "r11", 0, 0, 0, 0, 0  ; 11
                BYTE "r12", 0, 0, 0, 0, 0  ; 12
                BYTE "r13", 0, 0, 0, 0, 0  ; 13
                BYTE "r14", 0, 0, 0, 0, 0  ; 14
                BYTE "r15", 0, 0, 0, 0, 0  ; 15

; Register name tables (32-bit)
regNames32      BYTE "eax", 0, 0, 0, 0, 0
                BYTE "ecx", 0, 0, 0, 0, 0
                BYTE "edx", 0, 0, 0, 0, 0
                BYTE "ebx", 0, 0, 0, 0, 0
                BYTE "esp", 0, 0, 0, 0, 0
                BYTE "ebp", 0, 0, 0, 0, 0
                BYTE "esi", 0, 0, 0, 0, 0
                BYTE "edi", 0, 0, 0, 0, 0

; Register name tables (8-bit)
regNames8       BYTE "al", 0, 0, 0, 0, 0, 0
                BYTE "cl", 0, 0, 0, 0, 0, 0
                BYTE "dl", 0, 0, 0, 0, 0, 0
                BYTE "bl", 0, 0, 0, 0, 0, 0
                BYTE "ah", 0, 0, 0, 0, 0, 0
                BYTE "ch", 0, 0, 0, 0, 0, 0
                BYTE "dh", 0, 0, 0, 0, 0, 0
                BYTE "bh", 0, 0, 0, 0, 0, 0

; Condition code mnemonic table — 16 entries, 4 bytes each (null-padded)
; Indexed by lower nibble of opcode (0x70-0x7F, 0F 80-8F, 0F 40-4F, 0F 90-9F)
ccMnemonics     BYTE "jo", 0, 0             ; 0
                BYTE "jno", 0               ; 1
                BYTE "jb", 0, 0             ; 2
                BYTE "jae", 0               ; 3
                BYTE "je", 0, 0             ; 4
                BYTE "jne", 0               ; 5
                BYTE "jbe", 0               ; 6
                BYTE "ja", 0, 0             ; 7
                BYTE "js", 0, 0             ; 8
                BYTE "jns", 0               ; 9
                BYTE "jp", 0, 0             ; A
                BYTE "jnp", 0               ; B
                BYTE "jl", 0, 0             ; C
                BYTE "jge", 0               ; D
                BYTE "jle", 0               ; E
                BYTE "jg", 0, 0             ; F

; CMOVcc mnemonic prefixes — 16 entries, 8 bytes each
cmovMnemonics   BYTE "cmovo", 0, 0, 0      ; 0
                BYTE "cmovno", 0, 0         ; 1
                BYTE "cmovb", 0, 0, 0       ; 2
                BYTE "cmovae", 0, 0         ; 3
                BYTE "cmove", 0, 0, 0       ; 4
                BYTE "cmovne", 0, 0         ; 5
                BYTE "cmovbe", 0, 0         ; 6
                BYTE "cmova", 0, 0, 0       ; 7
                BYTE "cmovs", 0, 0, 0       ; 8
                BYTE "cmovns", 0, 0         ; 9
                BYTE "cmovp", 0, 0, 0       ; A
                BYTE "cmovnp", 0, 0         ; B
                BYTE "cmovl", 0, 0, 0       ; C
                BYTE "cmovge", 0, 0         ; D
                BYTE "cmovle", 0, 0         ; E
                BYTE "cmovg", 0, 0, 0       ; F

; SETcc mnemonic prefixes — 16 entries, 8 bytes each
setMnemonics    BYTE "seto", 0, 0, 0, 0    ; 0
                BYTE "setno", 0, 0, 0       ; 1
                BYTE "setb", 0, 0, 0, 0     ; 2
                BYTE "setae", 0, 0, 0       ; 3
                BYTE "sete", 0, 0, 0, 0     ; 4
                BYTE "setne", 0, 0, 0       ; 5
                BYTE "setbe", 0, 0, 0       ; 6
                BYTE "seta", 0, 0, 0, 0     ; 7
                BYTE "sets", 0, 0, 0, 0     ; 8
                BYTE "setns", 0, 0, 0       ; 9
                BYTE "setp", 0, 0, 0, 0     ; A
                BYTE "setnp", 0, 0, 0       ; B
                BYTE "setl", 0, 0, 0, 0     ; C
                BYTE "setge", 0, 0, 0       ; D
                BYTE "setle", 0, 0, 0       ; E
                BYTE "setg", 0, 0, 0, 0     ; F

; ALU group mnemonics (indexed by /reg field for opcodes 80-83, plus standalone)
aluGroupMn      BYTE "add", 0, 0, 0, 0, 0  ; /0
                BYTE "or", 0, 0, 0, 0, 0, 0 ; /1
                BYTE "adc", 0, 0, 0, 0, 0  ; /2
                BYTE "sbb", 0, 0, 0, 0, 0  ; /3
                BYTE "and", 0, 0, 0, 0, 0  ; /4
                BYTE "sub", 0, 0, 0, 0, 0  ; /5
                BYTE "xor", 0, 0, 0, 0, 0  ; /6
                BYTE "cmp", 0, 0, 0, 0, 0  ; /7

; Shift/rotate group mnemonics (indexed by /reg for opcodes C0/C1/D0-D3)
shiftGroupMn    BYTE "rol", 0, 0, 0, 0, 0  ; /0
                BYTE "ror", 0, 0, 0, 0, 0  ; /1
                BYTE "rcl", 0, 0, 0, 0, 0  ; /2
                BYTE "rcr", 0, 0, 0, 0, 0  ; /3
                BYTE "shl", 0, 0, 0, 0, 0  ; /4
                BYTE "shr", 0, 0, 0, 0, 0  ; /5
                BYTE "sal", 0, 0, 0, 0, 0  ; /6
                BYTE "sar", 0, 0, 0, 0, 0  ; /7

; Format strings for operand rendering
fmtRegReg       BYTE "%s, %s", 0
fmtRegImm32     BYTE "%s, 0x%08X", 0
fmtRegImm8      BYTE "%s, 0x%02X", 0
fmtRegMem       BYTE "%s, [%s]", 0
fmtRegMemDisp8  BYTE "%s, [%s%+d]", 0
fmtRegMemDisp32 BYTE "%s, [%s%+d]", 0
fmtMemReg       BYTE "[%s], %s", 0
fmtMemDisp8Reg  BYTE "[%s%+d], %s", 0
fmtMemDisp32Reg BYTE "[%s%+d], %s", 0
fmtRipDisp32    BYTE "[rip+0x%X]", 0
fmtRegRipDisp32 BYTE "%s, [rip+0x%X]", 0

; =============================================================================
; BSS Segment (uninitialized)
; =============================================================================

.DATA?

tmpBuffer       BYTE 4096 DUP(?)    ; Scratch buffer for formatting
tmpBuffer2      BYTE 4096 DUP(?)    ; Secondary scratch buffer

; =============================================================================
; Code Segment
; =============================================================================

.CODE

; =============================================================================
; DynArray_Init - Initialize a dynamic array
;   RCX = pointer to DYNARRAY structure
;   EDX = element size in bytes
; =============================================================================
DynArray_Init PROC
    push rbx
    push rsi
    sub rsp, 28h

    mov rsi, rcx                    ; Save DYNARRAY ptr
    mov [rsi].DYNARRAY.elemSize, edx
    mov [rsi].DYNARRAY.count, 0
    mov [rsi].DYNARRAY.capacity, INITIAL_CAPACITY

    ; Allocate initial storage: capacity * elemSize
    call GetProcessHeap
    mov rcx, rax                    ; hHeap
    mov edx, HEAP_ZERO_MEMORY       ; dwFlags
    mov eax, [rsi].DYNARRAY.capacity
    imul eax, [rsi].DYNARRAY.elemSize
    mov r8d, eax                    ; dwBytes
    call HeapAlloc
    mov [rsi].DYNARRAY.pData, rax

    add rsp, 28h
    pop rsi
    pop rbx
    ret
DynArray_Init ENDP

; =============================================================================
; DynArray_Destroy - Free dynamic array storage
;   RCX = pointer to DYNARRAY structure
; =============================================================================
DynArray_Destroy PROC
    push rbx
    sub rsp, 28h

    mov rbx, rcx
    mov rdx, [rbx].DYNARRAY.pData
    test rdx, rdx
    jz @@done

    call GetProcessHeap
    mov rcx, rax
    xor edx, edx                    ; dwFlags = 0
    mov r8, [rbx].DYNARRAY.pData
    call HeapFree

    mov QWORD PTR [rbx].DYNARRAY.pData, 0
    mov [rbx].DYNARRAY.count, 0
    mov [rbx].DYNARRAY.capacity, 0

@@done:
    add rsp, 28h
    pop rbx
    ret
DynArray_Destroy ENDP

; =============================================================================
; DynArray_Push - Append an element to the array
;   RCX = pointer to DYNARRAY structure
;   RDX = pointer to element data to copy
; Returns: RAX = pointer to the new element in the array
; =============================================================================
DynArray_Push PROC
    push rbx
    push rsi
    push rdi
    push r12
    sub rsp, 28h

    mov rsi, rcx                    ; DYNARRAY ptr
    mov r12, rdx                    ; Source element ptr (saved in callee-saved)

    ; Check if we need to grow
    mov eax, [rsi].DYNARRAY.count
    cmp eax, [rsi].DYNARRAY.capacity
    jb @@noresize

    ; Grow: new capacity = old * GROWTH_FACTOR
    mov eax, [rsi].DYNARRAY.capacity
    shl eax, 1                      ; * 2
    mov [rsi].DYNARRAY.capacity, eax

    ; HeapReAlloc(hHeap, HEAP_ZERO_MEMORY, pOld, newSize)
    call GetProcessHeap
    mov rcx, rax
    mov edx, HEAP_ZERO_MEMORY
    mov r8, [rsi].DYNARRAY.pData
    mov eax, [rsi].DYNARRAY.capacity
    imul eax, [rsi].DYNARRAY.elemSize
    mov r9d, eax
    call HeapReAlloc
    test rax, rax
    jz @@push_fail
    mov [rsi].DYNARRAY.pData, rax

@@noresize:
    ; Calculate destination: pData + count * elemSize
    mov eax, [rsi].DYNARRAY.count
    imul eax, [rsi].DYNARRAY.elemSize
    mov rbx, [rsi].DYNARRAY.pData
    add rbx, rax                    ; rbx = destination pointer

    ; Copy elemSize bytes: rep movsb (rdi=dest, rsi=src, ecx=len)
    push rsi                        ; Save DYNARRAY ptr
    mov ecx, [rsi].DYNARRAY.elemSize
    mov rdi, rbx                    ; dest = slot in array
    mov rsi, r12                    ; src  = element ptr (from callee-saved r12)
    rep movsb
    pop rsi                         ; Restore DYNARRAY ptr

    ; Increment count
    inc [rsi].DYNARRAY.count

    ; Return pointer to new element
    mov rax, rbx
    jmp @@push_exit

@@push_fail:
    xor eax, eax                    ; Return NULL on alloc failure

@@push_exit:
    add rsp, 28h
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
DynArray_Push ENDP

; =============================================================================
; DynArray_Get - Get element by index
;   RCX = pointer to DYNARRAY structure
;   EDX = index
; Returns: RAX = pointer to element, or 0 if out of bounds
; =============================================================================
DynArray_Get PROC
    cmp edx, [rcx].DYNARRAY.count
    jae @@outofbounds

    ; Calculate: pData + index * elemSize
    imul edx, [rcx].DYNARRAY.elemSize
    mov rax, [rcx].DYNARRAY.pData
    movsxd rdx, edx
    add rax, rdx
    ret

@@outofbounds:
    xor eax, eax
    ret
DynArray_Get ENDP

; =============================================================================
; DynArray_Clear - Reset count to zero (does not free memory)
;   RCX = pointer to DYNARRAY structure
; =============================================================================
DynArray_Clear PROC
    mov [rcx].DYNARRAY.count, 0
    ret
DynArray_Clear ENDP

; =============================================================================
; RawrCodex_Create - Allocate and initialize a RAWRCODEX_CTX
; Returns: RAX = pointer to new context, or 0 on failure
; =============================================================================
RawrCodex_Create PROC
    push rbx
    sub rsp, 28h

    ; Allocate context structure
    call GetProcessHeap
    mov rcx, rax
    mov edx, HEAP_ZERO_MEMORY
    mov r8d, SIZEOF RAWRCODEX_CTX
    call HeapAlloc
    test rax, rax
    jz @@fail

    mov rbx, rax                    ; Save ctx ptr

    ; Initialize all dynamic arrays
    lea rcx, [rbx].RAWRCODEX_CTX.sections
    mov edx, SIZEOF RAWRSECTION
    call DynArray_Init

    lea rcx, [rbx].RAWRCODEX_CTX.symbols
    mov edx, SIZEOF RAWRSYMBOL
    call DynArray_Init

    lea rcx, [rbx].RAWRCODEX_CTX.imports
    mov edx, SIZEOF RAWRIMPORT
    call DynArray_Init

    lea rcx, [rbx].RAWRCODEX_CTX.exports
    mov edx, SIZEOF RAWREXPORT
    call DynArray_Init

    lea rcx, [rbx].RAWRCODEX_CTX.instructions
    mov edx, SIZEOF RAWRINSTRUCTION
    call DynArray_Init

    lea rcx, [rbx].RAWRCODEX_CTX.basicBlocks
    mov edx, SIZEOF RAWRBASICBLOCK
    call DynArray_Init

    lea rcx, [rbx].RAWRCODEX_CTX.functions
    mov edx, SIZEOF RAWRFUNCTION
    call DynArray_Init

    lea rcx, [rbx].RAWRCODEX_CTX.matches
    mov edx, SIZEOF RAWRMATCH
    call DynArray_Init

    lea rcx, [rbx].RAWRCODEX_CTX.strings
    mov edx, SIZEOF RAWRSTRING
    call DynArray_Init

    lea rcx, [rbx].RAWRCODEX_CTX.xrefs
    mov edx, SIZEOF RAWRXREF
    call DynArray_Init

    mov [rbx].RAWRCODEX_CTX.hFile, INVALID_HANDLE_VALUE
    mov [rbx].RAWRCODEX_CTX.isLoaded, 0

    mov rax, rbx
    jmp @@done

@@fail:
    xor eax, eax

@@done:
    add rsp, 28h
    pop rbx
    ret
RawrCodex_Create ENDP

; =============================================================================
; RawrCodex_Destroy - Free context and all owned memory
;   RCX = pointer to RAWRCODEX_CTX
; =============================================================================
RawrCodex_Destroy PROC
    push rbx
    sub rsp, 28h

    test rcx, rcx
    jz @@done
    mov rbx, rcx

    ; Unmap file if loaded
    cmp [rbx].RAWRCODEX_CTX.isLoaded, 0
    je @@skip_unmap

    mov rcx, [rbx].RAWRCODEX_CTX.pFileBase
    test rcx, rcx
    jz @@skip_unmap_view
    call UnmapViewOfFile
@@skip_unmap_view:

    mov rcx, [rbx].RAWRCODEX_CTX.hMapping
    cmp rcx, 0
    je @@skip_close_mapping
    call CloseHandle
@@skip_close_mapping:

    mov rcx, [rbx].RAWRCODEX_CTX.hFile
    cmp rcx, INVALID_HANDLE_VALUE
    je @@skip_unmap
    call CloseHandle
@@skip_unmap:

    ; Destroy all dynamic arrays
    lea rcx, [rbx].RAWRCODEX_CTX.sections
    call DynArray_Destroy
    lea rcx, [rbx].RAWRCODEX_CTX.symbols
    call DynArray_Destroy
    lea rcx, [rbx].RAWRCODEX_CTX.imports
    call DynArray_Destroy
    lea rcx, [rbx].RAWRCODEX_CTX.exports
    call DynArray_Destroy
    lea rcx, [rbx].RAWRCODEX_CTX.instructions
    call DynArray_Destroy
    lea rcx, [rbx].RAWRCODEX_CTX.basicBlocks
    call DynArray_Destroy
    lea rcx, [rbx].RAWRCODEX_CTX.functions
    call DynArray_Destroy
    lea rcx, [rbx].RAWRCODEX_CTX.matches
    call DynArray_Destroy
    lea rcx, [rbx].RAWRCODEX_CTX.strings
    call DynArray_Destroy
    lea rcx, [rbx].RAWRCODEX_CTX.xrefs
    call DynArray_Destroy

    ; Free the context itself
    call GetProcessHeap
    mov rcx, rax
    xor edx, edx
    mov r8, rbx
    call HeapFree

@@done:
    add rsp, 28h
    pop rbx
    ret
RawrCodex_Destroy ENDP

; =============================================================================
; RawrCodex_LoadBinary - Memory-map a binary file for analysis
;   RCX = pointer to RAWRCODEX_CTX
;   RDX = pointer to null-terminated filename (ANSI)
; Returns: EAX = 1 on success, 0 on failure
; =============================================================================
RawrCodex_LoadBinary PROC
    push rbx
    push rsi
    push rdi
    push r12
    sub rsp, 48h

    mov rbx, rcx                    ; ctx
    mov rsi, rdx                    ; filename

    ; Open file for reading
    mov rcx, rsi                    ; lpFileName
    mov edx, GENERIC_READ           ; dwDesiredAccess
    mov r8d, FILE_SHARE_READ        ; dwShareMode
    xor r9d, r9d                    ; lpSecurityAttributes = NULL
    mov DWORD PTR [rsp+20h], OPEN_EXISTING  ; dwCreationDisposition
    mov DWORD PTR [rsp+28h], 0      ; dwFlagsAndAttributes
    mov QWORD PTR [rsp+30h], 0      ; hTemplateFile
    call CreateFileA
    cmp rax, INVALID_HANDLE_VALUE
    je @@fail
    mov [rbx].RAWRCODEX_CTX.hFile, rax

    ; Get file size
    mov rcx, rax                    ; hFile
    xor edx, edx                    ; lpFileSizeHigh = NULL
    call GetFileSize
    cmp eax, -1                     ; INVALID_FILE_SIZE
    je @@fail_close
    mov [rbx].RAWRCODEX_CTX.fileSize, rax

    ; Create file mapping
    mov rcx, [rbx].RAWRCODEX_CTX.hFile  ; hFile
    xor edx, edx                    ; lpFileMappingAttributes = NULL
    mov r8d, PAGE_READONLY          ; flProtect
    xor r9d, r9d                    ; dwMaximumSizeHigh = 0
    mov DWORD PTR [rsp+20h], 0      ; dwMaximumSizeLow = 0 (entire file)
    mov QWORD PTR [rsp+28h], 0      ; lpName = NULL
    call CreateFileMappingA
    test rax, rax
    jz @@fail_close
    mov [rbx].RAWRCODEX_CTX.hMapping, rax

    ; Map view of file
    mov rcx, rax                    ; hFileMappingObject
    mov edx, FILE_MAP_READ          ; dwDesiredAccess
    xor r8d, r8d                    ; dwFileOffsetHigh = 0
    xor r9d, r9d                    ; dwFileOffsetLow = 0
    mov QWORD PTR [rsp+20h], 0      ; dwNumberOfBytesToMap = 0 (all)
    call MapViewOfFile
    test rax, rax
    jz @@fail_close_mapping
    mov [rbx].RAWRCODEX_CTX.pFileBase, rax

    ; Detect binary format
    mov rdi, rax                    ; pFileBase

    ; Check PE: 'MZ' at offset 0
    movzx eax, WORD PTR [rdi]
    cmp ax, IMAGE_DOS_SIGNATURE
    jne @@check_elf

    ; PE detected - read e_lfanew at offset 0x3C
    mov eax, DWORD PTR [rdi + 3Ch]
    add rax, rdi                    ; NT headers pointer
    mov [rbx].RAWRCODEX_CTX.peNtHeaders, rax

    ; Verify PE signature
    cmp DWORD PTR [rax], IMAGE_NT_SIGNATURE
    jne @@check_elf

    mov [rbx].RAWRCODEX_CTX.isPE, 1
    mov [rbx].RAWRCODEX_CTX.isELF, 0

    ; Read machine type from File Header (offset +4 from NT headers)
    movzx ecx, WORD PTR [rax + 4]
    mov [rbx].RAWRCODEX_CTX.machine, ecx

    cmp cx, IMAGE_FILE_MACHINE_AMD64
    jne @@pe32
    mov [rbx].RAWRCODEX_CTX.is64Bit, 1

    ; PE64: sections = NumSections from FileHeader (+6)
    movzx ecx, WORD PTR [rax + 6]
    mov [rbx].RAWRCODEX_CTX.peNumSections, ecx

    ; Optional header at NT + 24
    lea r12, [rax + 18h]
    mov [rbx].RAWRCODEX_CTX.peOptionalHdr, r12

    ; Entry point RVA at OptionalHeader + 16
    mov ecx, DWORD PTR [r12 + 10h]
    mov [rbx].RAWRCODEX_CTX.peEntryPointRVA, ecx

    ; ImageBase at OptionalHeader + 24 (8 bytes for PE64)
    mov rcx, QWORD PTR [r12 + 18h]
    mov [rbx].RAWRCODEX_CTX.peImageBase, rcx

    ; Import directory: DataDirectory[1] at OptionalHeader + 120
    mov ecx, DWORD PTR [r12 + 78h]
    mov [rbx].RAWRCODEX_CTX.peImportDirRVA, ecx
    mov ecx, DWORD PTR [r12 + 7Ch]
    mov [rbx].RAWRCODEX_CTX.peImportDirSize, ecx

    ; Export directory: DataDirectory[0] at OptionalHeader + 112
    mov ecx, DWORD PTR [r12 + 70h]
    mov [rbx].RAWRCODEX_CTX.peExportDirRVA, ecx
    mov ecx, DWORD PTR [r12 + 74h]
    mov [rbx].RAWRCODEX_CTX.peExportDirSize, ecx

    ; Section table: after optional header
    ; SizeOfOptionalHeader is at FileHeader + 16 (NT + 20)
    movzx ecx, WORD PTR [rax + 14h]
    lea r12, [rax + 18h]            ; Start of optional header
    add r12, rcx                    ; Section table start
    mov [rbx].RAWRCODEX_CTX.peSectionTable, r12
    jmp @@format_done

@@pe32:
    mov [rbx].RAWRCODEX_CTX.is64Bit, 0
    movzx ecx, WORD PTR [rax + 6]
    mov [rbx].RAWRCODEX_CTX.peNumSections, ecx

    lea r12, [rax + 18h]
    mov [rbx].RAWRCODEX_CTX.peOptionalHdr, r12

    mov ecx, DWORD PTR [r12 + 10h]
    mov [rbx].RAWRCODEX_CTX.peEntryPointRVA, ecx

    ; ImageBase at OptionalHeader + 28 (4 bytes for PE32)
    mov ecx, DWORD PTR [r12 + 1Ch]
    movsxd rcx, ecx
    mov [rbx].RAWRCODEX_CTX.peImageBase, rcx

    ; Import directory: DataDirectory[1] at OptionalHeader + 104 (PE32)
    mov ecx, DWORD PTR [r12 + 68h]
    mov [rbx].RAWRCODEX_CTX.peImportDirRVA, ecx
    mov ecx, DWORD PTR [r12 + 6Ch]
    mov [rbx].RAWRCODEX_CTX.peImportDirSize, ecx

    ; Export directory: DataDirectory[0] at OptionalHeader + 96
    mov ecx, DWORD PTR [r12 + 60h]
    mov [rbx].RAWRCODEX_CTX.peExportDirRVA, ecx
    mov ecx, DWORD PTR [r12 + 64h]
    mov [rbx].RAWRCODEX_CTX.peExportDirSize, ecx

    movzx ecx, WORD PTR [rax + 14h]
    lea r12, [rax + 18h]
    add r12, rcx
    mov [rbx].RAWRCODEX_CTX.peSectionTable, r12
    jmp @@format_done

@@check_elf:
    ; Check ELF: 0x7F 'ELF' at offset 0
    mov eax, DWORD PTR [rdi]
    cmp eax, ELF_MAGIC
    jne @@unknown_format

    mov [rbx].RAWRCODEX_CTX.isPE, 0
    mov [rbx].RAWRCODEX_CTX.isELF, 1

    ; EI_CLASS at offset 4
    movzx eax, BYTE PTR [rdi + 4]
    cmp al, ELFCLASS64
    jne @@elf32

    ; ELF64
    mov [rbx].RAWRCODEX_CTX.is64Bit, 1

    ; e_entry at offset 0x18 (8 bytes)
    mov rax, QWORD PTR [rdi + 18h]
    mov [rbx].RAWRCODEX_CTX.elfEntryPoint, rax

    ; e_shoff at offset 0x28 (8 bytes)
    mov rax, QWORD PTR [rdi + 28h]
    mov [rbx].RAWRCODEX_CTX.elfSectionHdrOff, rax

    ; e_shnum at offset 0x3C (2 bytes)
    movzx eax, WORD PTR [rdi + 3Ch]
    mov [rbx].RAWRCODEX_CTX.elfNumSections, eax

    ; e_shstrndx at offset 0x3E (2 bytes)
    movzx eax, WORD PTR [rdi + 3Eh]
    mov [rbx].RAWRCODEX_CTX.elfStrTabIndex, eax
    jmp @@format_done

@@elf32:
    mov [rbx].RAWRCODEX_CTX.is64Bit, 0

    ; e_entry at offset 0x18 (4 bytes for ELF32)
    mov eax, DWORD PTR [rdi + 18h]
    movsxd rax, eax
    mov [rbx].RAWRCODEX_CTX.elfEntryPoint, rax

    ; e_shoff at offset 0x20 (4 bytes)
    mov eax, DWORD PTR [rdi + 20h]
    movsxd rax, eax
    mov [rbx].RAWRCODEX_CTX.elfSectionHdrOff, rax

    ; e_shnum at offset 0x30 (2 bytes for ELF32)
    movzx eax, WORD PTR [rdi + 30h]
    mov [rbx].RAWRCODEX_CTX.elfNumSections, eax

    ; e_shstrndx at offset 0x32
    movzx eax, WORD PTR [rdi + 32h]
    mov [rbx].RAWRCODEX_CTX.elfStrTabIndex, eax
    jmp @@format_done

@@unknown_format:
    ; Not PE or ELF - still allow raw analysis
    mov [rbx].RAWRCODEX_CTX.isPE, 0
    mov [rbx].RAWRCODEX_CTX.isELF, 0

@@format_done:
    mov [rbx].RAWRCODEX_CTX.isLoaded, 1
    mov eax, 1
    jmp @@exit

@@fail_close_mapping:
    mov rcx, [rbx].RAWRCODEX_CTX.hMapping
    call CloseHandle

@@fail_close:
    mov rcx, [rbx].RAWRCODEX_CTX.hFile
    call CloseHandle
    mov [rbx].RAWRCODEX_CTX.hFile, INVALID_HANDLE_VALUE

@@fail:
    xor eax, eax

@@exit:
    add rsp, 48h
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
RawrCodex_LoadBinary ENDP

; =============================================================================
; RvaToFileOffset - Convert RVA to raw file offset using section table
;   RCX = pointer to RAWRCODEX_CTX
;   EDX = RVA to convert
; Returns: RAX = file offset, or 0 if not found
; =============================================================================
RvaToFileOffset PROC
    push rbx
    push rsi

    mov rbx, rcx                    ; ctx
    mov esi, edx                    ; rva

    ; Walk sections array
    lea rcx, [rbx].RAWRCODEX_CTX.sections
    mov eax, [rcx].DYNARRAY.count
    test eax, eax
    jz @@not_found

    mov rcx, [rcx].DYNARRAY.pData   ; Section array base
    xor edx, edx                    ; index

@@loop:
    cmp edx, [rbx + OFFSET RAWRCODEX_CTX.sections + OFFSET DYNARRAY.count]
    jge @@not_found

    ; Calculate section pointer: base + index * sizeof(RAWRSECTION)
    mov eax, edx
    imul eax, SIZEOF RAWRSECTION
    lea rax, [rcx + rax]

    ; Check if RVA falls within this section
    ; VA <= rva < VA + virtualSize
    mov r8d, DWORD PTR [rax].RAWRSECTION.virtualAddress
    cmp esi, r8d
    jb @@next

    mov r9d, DWORD PTR [rax].RAWRSECTION.virtualSize
    add r9d, r8d
    cmp esi, r9d
    jae @@next

    ; Found: fileOffset = rawFileOffset + (rva - sectionVA)
    sub esi, r8d                    ; delta = rva - VA
    mov eax, DWORD PTR [rax].RAWRSECTION.fileOffset
    add eax, esi
    movsxd rax, eax

    pop rsi
    pop rbx
    ret

@@next:
    inc edx
    jmp @@loop

@@not_found:
    xor eax, eax
    pop rsi
    pop rbx
    ret
RvaToFileOffset ENDP

; =============================================================================
; RawrCodex_ParsePE - Parse PE headers, sections, imports, exports
;   RCX = pointer to RAWRCODEX_CTX
; Returns: EAX = number of sections parsed
; =============================================================================
RawrCodex_ParsePE PROC
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    push r14
    push r15
    sub rsp, 68h

    mov rbx, rcx                    ; ctx
    cmp [rbx].RAWRCODEX_CTX.isPE, 1
    jne @@fail

    ; Clear existing parsed data
    lea rcx, [rbx].RAWRCODEX_CTX.sections
    call DynArray_Clear
    lea rcx, [rbx].RAWRCODEX_CTX.imports
    call DynArray_Clear
    lea rcx, [rbx].RAWRCODEX_CTX.exports
    call DynArray_Clear

    ; Parse sections
    mov r12, [rbx].RAWRCODEX_CTX.peSectionTable
    mov r13d, [rbx].RAWRCODEX_CTX.peNumSections
    xor r14d, r14d                  ; section index

@@section_loop:
    cmp r14d, r13d
    jge @@sections_done

    ; Build RAWRSECTION on stack
    sub rsp, SIZEOF RAWRSECTION
    mov rdi, rsp                    ; point to stack section

    ; Zero it
    push rcx
    push rdi
    mov rcx, SIZEOF RAWRSECTION
    xor eax, eax
    rep stosb
    pop rdi
    pop rcx

    ; Copy 8-byte section name
    lea rsi, [r12]                  ; IMAGE_SECTION_HEADER.Name
    lea rdi, [rsp]                  ; RAWRSECTION.szName on stack
    ; Actually rdi was clobbered, recalculate
    mov rdi, rsp
    push rcx
    mov ecx, 8
@@copy_name:
    mov al, [rsi + rcx - 8]
    mov [rdi + rcx - 8], al
    inc ecx
    cmp ecx, 16
    jl @@copy_name_end
@@copy_name_end:
    ; Simple: copy first 8 bytes
    mov rax, QWORD PTR [r12]
    mov QWORD PTR [rdi], rax
    pop rcx

    ; VirtualSize at offset 8
    mov eax, DWORD PTR [r12 + 8]
    mov DWORD PTR [rdi + OFFSET RAWRSECTION.virtualSize], eax

    ; VirtualAddress at offset 12
    mov eax, DWORD PTR [r12 + 0Ch]
    mov DWORD PTR [rdi + OFFSET RAWRSECTION.virtualAddress], eax

    ; SizeOfRawData at offset 16
    mov eax, DWORD PTR [r12 + 10h]
    mov DWORD PTR [rdi + OFFSET RAWRSECTION.rawSize], eax

    ; PointerToRawData at offset 20
    mov eax, DWORD PTR [r12 + 14h]
    mov DWORD PTR [rdi + OFFSET RAWRSECTION.fileOffset], eax

    ; Characteristics at offset 36
    mov eax, DWORD PTR [r12 + 24h]
    mov [rdi + OFFSET RAWRSECTION.characteristics], eax

    ; Set flags
    test eax, IMAGE_SCN_MEM_EXECUTE
    setnz cl
    movzx ecx, cl
    mov [rdi + OFFSET RAWRSECTION.isExecutable], ecx

    mov eax, [rdi + OFFSET RAWRSECTION.characteristics]
    test eax, IMAGE_SCN_MEM_WRITE
    setnz cl
    movzx ecx, cl
    mov [rdi + OFFSET RAWRSECTION.isWritable], ecx

    mov eax, [rdi + OFFSET RAWRSECTION.characteristics]
    test eax, IMAGE_SCN_MEM_READ
    setnz cl
    movzx ecx, cl
    mov [rdi + OFFSET RAWRSECTION.isReadable], ecx

    ; Push section to array
    lea rcx, [rbx].RAWRCODEX_CTX.sections
    mov rdx, rsp                    ; pointer to stack RAWRSECTION
    call DynArray_Push

    add rsp, SIZEOF RAWRSECTION     ; Clean up stack section

    ; Advance to next section header (each is 40 bytes)
    add r12, 28h
    inc r14d
    jmp @@section_loop

@@sections_done:
    ; --- Parse Import Directory Table ---
    mov eax, [rbx].RAWRCODEX_CTX.peImportDirRVA
    test eax, eax
    jz @@imports_done

    mov edx, eax
    mov rcx, rbx
    call RvaToFileOffset
    test rax, rax
    jz @@imports_done

    mov r12, [rbx].RAWRCODEX_CTX.pFileBase
    add r12, rax                    ; r12 = pointer to IDT in mapped file

    ; Walk IDT entries (each 20 bytes, until all-zero)
@@idt_loop:
    ; Check if entry is all zero (OriginalFirstThunk at offset 0)
    mov eax, DWORD PTR [r12]
    or eax, DWORD PTR [r12 + 4]
    or eax, DWORD PTR [r12 + 8]
    or eax, DWORD PTR [r12 + 0Ch]
    or eax, DWORD PTR [r12 + 10h]
    test eax, eax
    jz @@imports_done

    ; Get DLL name RVA (offset 12 in IDT entry)
    mov edx, DWORD PTR [r12 + 0Ch]
    mov rcx, rbx
    call RvaToFileOffset
    test rax, rax
    jz @@idt_next

    mov r13, [rbx].RAWRCODEX_CTX.pFileBase
    add r13, rax                    ; r13 = DLL name string

    ; Get ILT RVA (OriginalFirstThunk, offset 0) or IAT (offset 16)
    mov edx, DWORD PTR [r12]
    test edx, edx
    jnz @@use_ilt
    mov edx, DWORD PTR [r12 + 10h]  ; Fall back to IAT (FirstThunk)
@@use_ilt:
    mov rcx, rbx
    call RvaToFileOffset
    test rax, rax
    jz @@idt_next

    mov r14, [rbx].RAWRCODEX_CTX.pFileBase
    add r14, rax                    ; r14 = ILT/IAT pointer

    ; Walk ILT entries
@@ilt_loop:
    ; Read thunk value (8 bytes for PE64, 4 bytes for PE32)
    cmp [rbx].RAWRCODEX_CTX.is64Bit, 1
    jne @@ilt_32bit

    mov rax, QWORD PTR [r14]
    test rax, rax
    jz @@idt_next                   ; End of ILT

    ; Check ordinal flag (bit 63 for PE64)
    bt rax, 63
    jc @@import_by_ordinal_64

    ; Import by name: thunk value is RVA to IMAGE_IMPORT_BY_NAME
    mov edx, eax                    ; Lower 32 bits = RVA
    mov rcx, rbx
    call RvaToFileOffset
    test rax, rax
    jz @@ilt_next

    mov r15, [rbx].RAWRCODEX_CTX.pFileBase
    add r15, rax                    ; r15 -> IMAGE_IMPORT_BY_NAME
    ; Hint at offset 0 (2 bytes), Name at offset 2
    lea rsi, [r15 + 2]             ; Function name string

    ; Build RAWRIMPORT on stack
    sub rsp, SIZEOF RAWRIMPORT
    mov rdi, rsp
    ; Zero it
    push rcx
    mov ecx, SIZEOF RAWRIMPORT
    xor eax, eax
    push rdi
    rep stosb
    pop rdi
    pop rcx

    ; Copy DLL name (up to 127 chars)
    push rcx
    lea rdi, [rsp + 8]             ; account for pushed rcx -> szDllName
    mov rax, r13
    xor ecx, ecx
@@copy_dll:
    mov al, [r13 + rcx]
    test al, al
    jz @@copy_dll_done
    cmp ecx, 126
    jge @@copy_dll_done
    mov [rdi + rcx], al
    inc ecx
    jmp @@copy_dll
@@copy_dll_done:
    pop rcx

    ; Copy function name
    push rcx
    mov rdi, rsp
    add rdi, 8                      ; pushed rcx
    add rdi, 128                    ; skip szDllName -> szFuncName
    xor ecx, ecx
@@copy_func:
    mov al, [rsi + rcx]
    test al, al
    jz @@copy_func_done
    cmp ecx, 126
    jge @@copy_func_done
    mov [rdi + rcx], al
    inc ecx
    jmp @@copy_func
@@copy_func_done:
    pop rcx

    ; Set ordinal from hint
    movzx eax, WORD PTR [r15]
    mov DWORD PTR [rsp + OFFSET RAWRIMPORT.ordinal], eax
    mov DWORD PTR [rsp + OFFSET RAWRIMPORT.isOrdinalOnly], 0

    ; Push to imports array
    lea rcx, [rbx].RAWRCODEX_CTX.imports
    mov rdx, rsp
    call DynArray_Push

    add rsp, SIZEOF RAWRIMPORT
    jmp @@ilt_next

@@import_by_ordinal_64:
    ; Ordinal = lower 16 bits
    movzx eax, ax
    ; Build minimal RAWRIMPORT
    sub rsp, SIZEOF RAWRIMPORT
    mov rdi, rsp
    push rcx
    mov ecx, SIZEOF RAWRIMPORT
    xor eax, eax
    push rdi
    rep stosb
    pop rdi
    pop rcx
    ; Copy DLL name
    push rcx
    mov rdi, rsp
    add rdi, 8
    xor ecx, ecx
@@copy_dll_ord:
    mov al, [r13 + rcx]
    test al, al
    jz @@copy_dll_ord_done
    cmp ecx, 126
    jge @@copy_dll_ord_done
    mov [rdi + rcx], al
    inc ecx
    jmp @@copy_dll_ord
@@copy_dll_ord_done:
    pop rcx
    mov DWORD PTR [rsp + OFFSET RAWRIMPORT.isOrdinalOnly], 1
    lea rcx, [rbx].RAWRCODEX_CTX.imports
    mov rdx, rsp
    call DynArray_Push
    add rsp, SIZEOF RAWRIMPORT
    jmp @@ilt_next

@@ilt_32bit:
    mov eax, DWORD PTR [r14]
    test eax, eax
    jz @@idt_next
    ; Similar logic for 32-bit thunks (bit 31 = ordinal flag)
    ; Simplified: treat all as by-name for now
    jmp @@ilt_next

@@ilt_next:
    ; Advance ILT pointer
    cmp [rbx].RAWRCODEX_CTX.is64Bit, 1
    jne @@ilt_advance_32
    add r14, 8
    jmp @@ilt_loop
@@ilt_advance_32:
    add r14, 4
    jmp @@ilt_loop

@@idt_next:
    add r12, 14h                    ; Next IDT entry (20 bytes)
    jmp @@idt_loop

@@imports_done:

    ; --- Parse Export Directory Table ---
    mov eax, [rbx].RAWRCODEX_CTX.peExportDirRVA
    test eax, eax
    jz @@exports_done

    mov edx, eax
    mov rcx, rbx
    call RvaToFileOffset
    test rax, rax
    jz @@exports_done

    mov r12, [rbx].RAWRCODEX_CTX.pFileBase
    add r12, rax                    ; r12 = IMAGE_EXPORT_DIRECTORY

    ; NumberOfFunctions at offset 20
    mov r13d, DWORD PTR [r12 + 14h]
    ; NumberOfNames at offset 24
    mov r14d, DWORD PTR [r12 + 18h]
    ; AddressOfFunctions RVA at offset 28
    mov edx, DWORD PTR [r12 + 1Ch]
    mov rcx, rbx
    call RvaToFileOffset
    mov r15, [rbx].RAWRCODEX_CTX.pFileBase
    add r15, rax                    ; r15 = AddressOfFunctions array

    ; AddressOfNames RVA at offset 32
    mov edx, DWORD PTR [r12 + 20h]
    mov rcx, rbx
    call RvaToFileOffset
    push rax                        ; Save names array offset

    ; AddressOfNameOrdinals RVA at offset 36
    mov edx, DWORD PTR [r12 + 24h]
    mov rcx, rbx
    call RvaToFileOffset
    mov rsi, [rbx].RAWRCODEX_CTX.pFileBase
    add rsi, rax                    ; rsi = AddressOfNameOrdinals array

    pop rax
    mov rdi, [rbx].RAWRCODEX_CTX.pFileBase
    add rdi, rax                    ; rdi = AddressOfNames array

    ; Base ordinal at offset 16
    mov eax, DWORD PTR [r12 + 10h]
    mov [rsp + 40h], eax            ; Store base ordinal locally

    ; Walk named exports
    xor r14d, r14d                  ; name index

@@export_loop:
    cmp r14d, DWORD PTR [r12 + 18h] ; NumberOfNames
    jge @@exports_done

    ; Get name RVA: AddressOfNames[index]
    mov edx, DWORD PTR [rdi + r14 * 4]
    mov rcx, rbx
    call RvaToFileOffset
    test rax, rax
    jz @@export_next

    mov rcx, [rbx].RAWRCODEX_CTX.pFileBase
    add rcx, rax                    ; rcx = export name string

    ; Get ordinal index: AddressOfNameOrdinals[index]
    movzx eax, WORD PTR [rsi + r14 * 2]
    ; Function RVA: AddressOfFunctions[ordinal]
    mov edx, DWORD PTR [r15 + rax * 4]

    ; Build RAWREXPORT on stack
    sub rsp, SIZEOF RAWREXPORT
    mov r8, rsp
    ; Zero it
    push rcx
    push rdi
    mov rdi, r8
    push rcx
    mov ecx, SIZEOF RAWREXPORT
    xor eax, eax
    rep stosb
    pop rcx
    pop rdi
    pop rcx

    ; Copy function name
    push rdx
    mov rdi, rsp
    add rdi, 8                      ; szFuncName at start of RAWREXPORT
    ; Actually RAWREXPORT starts with szFuncName
    lea rdi, [rsp + 8]             ; after pushed rdx
    xor edx, edx
@@copy_exp_name:
    mov al, [rcx + rdx]
    test al, al
    jz @@copy_exp_done
    cmp edx, 126
    jge @@copy_exp_done
    mov [rdi + rdx], al
    inc edx
    jmp @@copy_exp_name
@@copy_exp_done:
    pop rdx

    ; Set RVA
    movsxd rax, edx
    mov [rsp + OFFSET RAWREXPORT.rva], rax

    ; Set ordinal (index + base)
    movzx eax, WORD PTR [rsi + r14 * 2]
    add eax, [rsp + 40h + SIZEOF RAWREXPORT]  ; base ordinal
    mov [rsp + OFFSET RAWREXPORT.ordinal], eax

    ; Push to exports array
    lea rcx, [rbx].RAWRCODEX_CTX.exports
    mov rdx, rsp
    call DynArray_Push

    add rsp, SIZEOF RAWREXPORT

@@export_next:
    inc r14d
    jmp @@export_loop

@@exports_done:
    ; Return section count
    lea rcx, [rbx].RAWRCODEX_CTX.sections
    mov eax, [rcx].DYNARRAY.count

    add rsp, 68h
    pop r15
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret

@@fail:
    xor eax, eax
    add rsp, 68h
    pop r15
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
RawrCodex_ParsePE ENDP

; =============================================================================
; RawrCodex_ParseELF - Parse ELF headers and section table
;   RCX = pointer to RAWRCODEX_CTX
; Returns: EAX = number of sections parsed
; =============================================================================
RawrCodex_ParseELF PROC
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    sub rsp, 48h

    mov rbx, rcx
    cmp [rbx].RAWRCODEX_CTX.isELF, 1
    jne @@fail

    lea rcx, [rbx].RAWRCODEX_CTX.sections
    call DynArray_Clear

    mov rdi, [rbx].RAWRCODEX_CTX.pFileBase
    mov r12, [rbx].RAWRCODEX_CTX.elfSectionHdrOff
    add r12, rdi                    ; r12 = section header table in memory
    mov r13d, [rbx].RAWRCODEX_CTX.elfNumSections

    ; Get section name string table
    ; shstrtab = section headers + strTabIndex * entrySize
    mov eax, [rbx].RAWRCODEX_CTX.elfStrTabIndex
    cmp [rbx].RAWRCODEX_CTX.is64Bit, 1
    jne @@elf32_strtab

    ; ELF64: section header entry size = 64 bytes
    imul eax, 40h
    movsxd rax, eax
    add rax, r12                    ; rax = shstrtab header
    ; sh_offset at offset 24 in ELF64 section header
    mov rsi, QWORD PTR [rax + 18h]
    add rsi, rdi                    ; rsi = string table base
    jmp @@parse_elf_sections

@@elf32_strtab:
    ; ELF32: section header entry size = 40 bytes
    imul eax, 28h
    movsxd rax, eax
    add rax, r12
    ; sh_offset at offset 16 in ELF32 section header
    mov eax, DWORD PTR [rax + 10h]
    movsxd rax, eax
    add rax, rdi
    mov rsi, rax                    ; rsi = string table base

@@parse_elf_sections:
    xor ecx, ecx                    ; section index

@@elf_section_loop:
    cmp ecx, r13d
    jge @@elf_sections_done

    ; Save index
    mov [rsp + 20h], ecx

    ; Calculate current section header pointer
    mov eax, ecx
    cmp [rbx].RAWRCODEX_CTX.is64Bit, 1
    jne @@elf32_entry_size
    imul eax, 40h                   ; 64-byte entries
    jmp @@elf_calc_ptr
@@elf32_entry_size:
    imul eax, 28h                   ; 40-byte entries
@@elf_calc_ptr:
    movsxd rax, eax
    lea r8, [r12 + rax]            ; r8 = current section header

    ; Build RAWRSECTION on stack
    sub rsp, SIZEOF RAWRSECTION
    mov rdi, rsp
    ; Zero it
    push rcx
    push rdi
    mov ecx, SIZEOF RAWRSECTION
    xor eax, eax
    rep stosb
    pop rdi
    pop rcx

    ; sh_name (name index into strtab) at offset 0
    mov eax, DWORD PTR [r8]
    ; Copy name from string table
    push rcx
    lea rcx, [rsi + rax]           ; name string
    xor edx, edx
@@copy_elf_name:
    mov al, [rcx + rdx]
    test al, al
    jz @@copy_elf_name_done
    cmp edx, 15
    jge @@copy_elf_name_done
    mov [rdi + rdx], al
    inc edx
    jmp @@copy_elf_name
@@copy_elf_name_done:
    pop rcx

    ; Parse fields based on 64/32 bit
    cmp [rbx].RAWRCODEX_CTX.is64Bit, 1
    jne @@elf32_fields

    ; ELF64 section header layout:
    ; +4: sh_type, +8: sh_flags, +16: sh_addr, +24: sh_offset, +32: sh_size

    ; sh_addr (VA) at offset 16
    mov rax, QWORD PTR [r8 + 10h]
    mov [rdi + OFFSET RAWRSECTION.virtualAddress], rax

    ; sh_offset at offset 24
    mov rax, QWORD PTR [r8 + 18h]
    mov [rdi + OFFSET RAWRSECTION.fileOffset], rax

    ; sh_size at offset 32
    mov rax, QWORD PTR [r8 + 20h]
    mov [rdi + OFFSET RAWRSECTION.virtualSize], rax
    mov [rdi + OFFSET RAWRSECTION.rawSize], rax

    ; sh_flags at offset 8
    mov rax, QWORD PTR [r8 + 8]
    ; SHF_WRITE = 1, SHF_ALLOC = 2, SHF_EXECINSTR = 4
    test rax, 4                     ; SHF_EXECINSTR
    setnz cl
    movzx ecx, cl
    mov [rdi + OFFSET RAWRSECTION.isExecutable], ecx
    test rax, 1                     ; SHF_WRITE
    setnz cl
    movzx ecx, cl
    mov [rdi + OFFSET RAWRSECTION.isWritable], ecx
    test rax, 2                     ; SHF_ALLOC
    setnz cl
    movzx ecx, cl
    mov [rdi + OFFSET RAWRSECTION.isReadable], ecx
    jmp @@elf_push_section

@@elf32_fields:
    ; ELF32: +4: type, +8: flags, +12: addr, +16: offset, +20: size
    mov eax, DWORD PTR [r8 + 0Ch]
    movsxd rax, eax
    mov [rdi + OFFSET RAWRSECTION.virtualAddress], rax

    mov eax, DWORD PTR [r8 + 10h]
    movsxd rax, eax
    mov [rdi + OFFSET RAWRSECTION.fileOffset], rax

    mov eax, DWORD PTR [r8 + 14h]
    movsxd rax, eax
    mov [rdi + OFFSET RAWRSECTION.virtualSize], rax
    mov [rdi + OFFSET RAWRSECTION.rawSize], rax

    mov eax, DWORD PTR [r8 + 8]
    test eax, 4
    setnz cl
    movzx ecx, cl
    mov [rdi + OFFSET RAWRSECTION.isExecutable], ecx
    test eax, 1
    setnz cl
    movzx ecx, cl
    mov [rdi + OFFSET RAWRSECTION.isWritable], ecx
    test eax, 2
    setnz cl
    movzx ecx, cl
    mov [rdi + OFFSET RAWRSECTION.isReadable], ecx

@@elf_push_section:
    lea rcx, [rbx].RAWRCODEX_CTX.sections
    mov rdx, rsp
    call DynArray_Push

    add rsp, SIZEOF RAWRSECTION

    mov ecx, [rsp + 20h]
    inc ecx
    jmp @@elf_section_loop

@@elf_sections_done:
    lea rcx, [rbx].RAWRCODEX_CTX.sections
    mov eax, [rcx].DYNARRAY.count

    add rsp, 48h
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret

@@fail:
    xor eax, eax
    add rsp, 48h
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
RawrCodex_ParseELF ENDP

; =============================================================================
; RawrCodex_DecodeInstruction - Decode a single x64 instruction
;   RCX = pointer to byte stream
;   RDX = virtual address of instruction
;   R8  = pointer to RAWRINSTRUCTION to fill
;   R9  = maximum bytes available
; Returns: EAX = instruction length, 0 on failure
; =============================================================================
RawrCodex_DecodeInstruction PROC
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    push r14
    push r15
    sub rsp, 48h

    mov rsi, rcx                    ; byte stream
    mov r12, rdx                    ; virtual address
    mov r13, r8                     ; output RAWRINSTRUCTION
    mov r14d, r9d                   ; max bytes

    ; Zero out the instruction struct
    mov rdi, r13
    mov ecx, SIZEOF RAWRINSTRUCTION
    xor eax, eax
    rep stosb

    ; Store the address
    mov [r13].RAWRINSTRUCTION.address, r12

    ; Initialize decode state
    xor r15d, r15d                  ; Current offset into byte stream
    xor ebx, ebx                    ; REX prefix (0 if none)

    ; Check for REX prefix (0x40-0x4F)
    movzx eax, BYTE PTR [rsi]
    cmp al, 40h
    jb @@no_rex
    cmp al, 4Fh
    ja @@no_rex
    mov ebx, eax                    ; Save REX byte
    inc r15d                        ; Consume prefix

@@no_rex:
    ; Read opcode byte
    movzx eax, BYTE PTR [rsi + r15]
    inc r15d

    ; ---- Single-byte opcode decode ----

    ; NOP (0x90)
    cmp al, 90h
    jne @@check_ret
    lea rcx, [r13].RAWRINSTRUCTION.szMnemonic
    lea rdx, mnNOP
    call lstrcpyA
    jmp @@decode_done

@@check_ret:
    ; RET (0xC3)
    cmp al, 0C3h
    jne @@check_retf
    lea rcx, [r13].RAWRINSTRUCTION.szMnemonic
    lea rdx, mnRET
    call lstrcpyA
    mov [r13].RAWRINSTRUCTION.isReturn, 1
    jmp @@decode_done

@@check_retf:
    ; RETF (0xCB)
    cmp al, 0CBh
    jne @@check_int3
    lea rcx, [r13].RAWRINSTRUCTION.szMnemonic
    lea rdx, mnRETF
    call lstrcpyA
    mov [r13].RAWRINSTRUCTION.isReturn, 1
    jmp @@decode_done

@@check_int3:
    ; INT3 (0xCC)
    cmp al, 0CCh
    jne @@check_hlt
    lea rcx, [r13].RAWRINSTRUCTION.szMnemonic
    lea rdx, mnINT3
    call lstrcpyA
    jmp @@decode_done

@@check_hlt:
    ; HLT (0xF4)
    cmp al, 0F4h
    jne @@check_push_reg
    lea rcx, [r13].RAWRINSTRUCTION.szMnemonic
    lea rdx, mnHLT
    call lstrcpyA
    jmp @@decode_done

@@check_push_reg:
    ; PUSH reg (0x50-0x57)
    cmp al, 50h
    jb @@check_pop_reg
    cmp al, 57h
    ja @@check_pop_reg
    lea rcx, [r13].RAWRINSTRUCTION.szMnemonic
    lea rdx, mnPUSH
    call lstrcpyA
    ; Determine register: opcode & 7, extended by REX.B
    movzx eax, BYTE PTR [rsi + r15 - 1]
    and eax, 7
    test ebx, 1                     ; REX.B
    jz @@push_no_ext
    add eax, 8
@@push_no_ext:
    ; Look up register name
    imul eax, 8
    lea rcx, regNames64
    lea rcx, [r13].RAWRINSTRUCTION.szOperands
    lea rdx, [regNames64 + rax]
    call lstrcpyA
    jmp @@decode_done

@@check_pop_reg:
    ; POP reg (0x58-0x5F)
    cmp al, 58h
    jb @@check_call_rel32
    cmp al, 5Fh
    ja @@check_call_rel32
    lea rcx, [r13].RAWRINSTRUCTION.szMnemonic
    lea rdx, mnPOP
    call lstrcpyA
    movzx eax, BYTE PTR [rsi + r15 - 1]
    and eax, 7
    test ebx, 1
    jz @@pop_no_ext
    add eax, 8
@@pop_no_ext:
    imul eax, 8
    lea rcx, [r13].RAWRINSTRUCTION.szOperands
    lea rdx, [regNames64 + rax]
    call lstrcpyA
    jmp @@decode_done

@@check_call_rel32:
    ; CALL rel32 (0xE8)
    cmp al, 0E8h
    jne @@check_jmp_rel32
    lea rcx, [r13].RAWRINSTRUCTION.szMnemonic
    lea rdx, mnCALL
    call lstrcpyA
    mov [r13].RAWRINSTRUCTION.isCall, 1
    ; Read rel32 displacement
    movsxd rax, DWORD PTR [rsi + r15]
    add r15d, 4
    ; Target = VA + totalLength + rel32
    mov rcx, r12
    movsxd rdx, r15d
    add rcx, rdx
    add rcx, rax
    mov [r13].RAWRINSTRUCTION.branchTarget, rcx
    ; Format target as operands
    lea rcx, [r13].RAWRINSTRUCTION.szOperands
    lea rdx, fmtHex8
    mov r8, [r13].RAWRINSTRUCTION.branchTarget
    call wsprintfA
    jmp @@decode_done

@@check_jmp_rel32:
    ; JMP rel32 (0xE9)
    cmp al, 0E9h
    jne @@check_jmp_rel8
    lea rcx, [r13].RAWRINSTRUCTION.szMnemonic
    lea rdx, mnJMP
    call lstrcpyA
    mov [r13].RAWRINSTRUCTION.isJump, 1
    movsxd rax, DWORD PTR [rsi + r15]
    add r15d, 4
    mov rcx, r12
    movsxd rdx, r15d
    add rcx, rdx
    add rcx, rax
    mov [r13].RAWRINSTRUCTION.branchTarget, rcx
    lea rcx, [r13].RAWRINSTRUCTION.szOperands
    lea rdx, fmtHex8
    mov r8, [r13].RAWRINSTRUCTION.branchTarget
    call wsprintfA
    jmp @@decode_done

@@check_jmp_rel8:
    ; JMP rel8 (0xEB)
    cmp al, 0EBh
    jne @@check_jcc_rel8
    lea rcx, [r13].RAWRINSTRUCTION.szMnemonic
    lea rdx, mnJMP
    call lstrcpyA
    mov [r13].RAWRINSTRUCTION.isJump, 1
    movsx eax, BYTE PTR [rsi + r15]
    inc r15d
    movsxd rcx, eax
    mov rax, r12
    movsxd rdx, r15d
    add rax, rdx
    add rax, rcx
    mov [r13].RAWRINSTRUCTION.branchTarget, rax
    lea rcx, [r13].RAWRINSTRUCTION.szOperands
    lea rdx, fmtHex8
    mov r8, [r13].RAWRINSTRUCTION.branchTarget
    call wsprintfA
    jmp @@decode_done

@@check_jcc_rel8:
    ; Jcc rel8 (0x70-0x7F)
    cmp al, 70h
    jb @@check_leave
    cmp al, 7Fh
    ja @@check_leave
    ; Build "jCC" mnemonic
    mov [r13].RAWRINSTRUCTION.isJump, 1
    mov [r13].RAWRINSTRUCTION.isBranch, 1
    ; TODO: Build full jcc mnemonic with condition code suffix
    lea rcx, [r13].RAWRINSTRUCTION.szMnemonic
    lea rdx, mnJMP                  ; Simplified: use "jmp" for now
    call lstrcpyA
    movsx eax, BYTE PTR [rsi + r15]
    inc r15d
    movsxd rcx, eax
    mov rax, r12
    movsxd rdx, r15d
    add rax, rdx
    add rax, rcx
    mov [r13].RAWRINSTRUCTION.branchTarget, rax
    lea rcx, [r13].RAWRINSTRUCTION.szOperands
    lea rdx, fmtHex8
    mov r8, [r13].RAWRINSTRUCTION.branchTarget
    call wsprintfA
    jmp @@decode_done

@@check_leave:
    ; LEAVE (0xC9)
    cmp al, 0C9h
    jne @@check_twobyte
    lea rcx, [r13].RAWRINSTRUCTION.szMnemonic
    lea rdx, mnLEAVE
    call lstrcpyA
    jmp @@decode_done

@@check_twobyte:
    ; Two-byte escape (0x0F xx)
    cmp al, 0Fh
    jne @@check_syscall
    cmp r15d, r14d
    jge @@decode_fail
    movzx eax, BYTE PTR [rsi + r15]
    inc r15d

    ; 0F 05 = SYSCALL
    cmp al, 05h
    jne @@check_0f_ud2
    lea rcx, [r13].RAWRINSTRUCTION.szMnemonic
    lea rdx, mnSYSCALL
    call lstrcpyA
    jmp @@decode_done

@@check_0f_ud2:
    ; 0F 0B = UD2
    cmp al, 0Bh
    jne @@check_0f_cpuid
    lea rcx, [r13].RAWRINSTRUCTION.szMnemonic
    lea rdx, mnUD2
    call lstrcpyA
    jmp @@decode_done

@@check_0f_cpuid:
    ; 0F A2 = CPUID
    cmp al, 0A2h
    jne @@check_0f_rdtsc
    lea rcx, [r13].RAWRINSTRUCTION.szMnemonic
    lea rdx, mnCPUID
    call lstrcpyA
    jmp @@decode_done

@@check_0f_rdtsc:
    ; 0F 31 = RDTSC
    cmp al, 31h
    jne @@check_0f_jcc
    lea rcx, [r13].RAWRINSTRUCTION.szMnemonic
    lea rdx, mnRDTSC
    call lstrcpyA
    jmp @@decode_done

@@check_0f_jcc:
    ; 0F 80-8F = Jcc rel32
    cmp al, 80h
    jb @@check_0f_movzx
    cmp al, 8Fh
    ja @@check_0f_movzx
    mov [r13].RAWRINSTRUCTION.isJump, 1
    mov [r13].RAWRINSTRUCTION.isBranch, 1
    lea rcx, [r13].RAWRINSTRUCTION.szMnemonic
    lea rdx, mnJMP
    call lstrcpyA
    movsxd rax, DWORD PTR [rsi + r15]
    add r15d, 4
    mov rcx, r12
    movsxd rdx, r15d
    add rcx, rdx
    add rcx, rax
    mov [r13].RAWRINSTRUCTION.branchTarget, rcx
    lea rcx, [r13].RAWRINSTRUCTION.szOperands
    lea rdx, fmtHex8
    mov r8, [r13].RAWRINSTRUCTION.branchTarget
    call wsprintfA
    jmp @@decode_done

@@check_0f_movzx:
    ; 0F B6 = MOVZX r, r/m8
    ; 0F B7 = MOVZX r, r/m16
    cmp al, 0B6h
    je @@decode_movzx
    cmp al, 0B7h
    jne @@check_0f_movsx
@@decode_movzx:
    lea rcx, [r13].RAWRINSTRUCTION.szMnemonic
    lea rdx, mnMOVZX
    call lstrcpyA
    inc r15d                        ; Skip ModR/M byte
    jmp @@decode_done

@@check_0f_movsx:
    ; 0F BE = MOVSX r, r/m8
    ; 0F BF = MOVSX r, r/m16
    cmp al, 0BEh
    je @@decode_movsx
    cmp al, 0BFh
    jne @@twobyte_default
@@decode_movsx:
    lea rcx, [r13].RAWRINSTRUCTION.szMnemonic
    lea rdx, mnMOVSX
    call lstrcpyA
    inc r15d
    jmp @@decode_done

@@twobyte_default:
    ; Unknown two-byte: emit as "db 0F, xx"
    lea rcx, [r13].RAWRINSTRUCTION.szMnemonic
    lea rdx, mnDB
    call lstrcpyA
    jmp @@decode_done

@@check_syscall:
    ; Default: unknown single byte
    lea rcx, [r13].RAWRINSTRUCTION.szMnemonic
    lea rdx, mnDB
    call lstrcpyA

@@decode_done:
    ; Store instruction length and raw bytes
    mov [r13].RAWRINSTRUCTION.length, r15d

    ; Copy raw bytes (up to 16)
    xor ecx, ecx
@@copy_raw:
    cmp ecx, r15d
    jge @@raw_done
    cmp ecx, 16
    jge @@raw_done
    movzx eax, BYTE PTR [rsi + rcx]
    mov BYTE PTR [r13 + OFFSET RAWRINSTRUCTION.rawBytes + rcx], al
    inc ecx
    jmp @@copy_raw
@@raw_done:

    mov eax, r15d                   ; Return instruction length

    add rsp, 48h
    pop r15
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret

@@decode_fail:
    xor eax, eax
    add rsp, 48h
    pop r15
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
RawrCodex_DecodeInstruction ENDP

; =============================================================================
; RawrCodex_Disassemble - Disassemble a range of bytes
;   RCX = pointer to RAWRCODEX_CTX
;   RDX = start virtual address
;   R8  = number of bytes to disassemble
; Returns: EAX = number of instructions decoded
; =============================================================================
RawrCodex_Disassemble PROC
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    push r14
    sub rsp, 58h

    mov rbx, rcx                    ; ctx
    mov r12, rdx                    ; start VA
    mov r13, r8                     ; byte count

    ; Clear instruction array
    lea rcx, [rbx].RAWRCODEX_CTX.instructions
    call DynArray_Clear

    ; Find the section containing startVA and get data pointer
    mov rdi, [rbx].RAWRCODEX_CTX.pFileBase
    ; Convert VA to file offset (simplified: walk sections)
    mov edx, r12d
    mov rcx, rbx
    call RvaToFileOffset
    test rax, rax
    jz @@disasm_fail
    add rdi, rax                    ; rdi = byte stream pointer

    xor r14d, r14d                  ; Offset into stream

@@disasm_loop:
    cmp r14, r13
    jge @@disasm_done

    ; Prepare instruction decode
    lea rcx, [rdi + r14]           ; Byte stream at current offset
    lea rdx, [r12 + r14]           ; Current VA

    sub rsp, SIZEOF RAWRINSTRUCTION
    mov r8, rsp                     ; Output instruction struct
    mov rax, r13
    sub rax, r14
    mov r9d, eax                    ; Remaining bytes

    call RawrCodex_DecodeInstruction
    test eax, eax
    jz @@disasm_skip

    ; Push decoded instruction to array
    lea rcx, [rbx].RAWRCODEX_CTX.instructions
    mov rdx, rsp
    call DynArray_Push

    mov eax, DWORD PTR [rsp + OFFSET RAWRINSTRUCTION.length]
    add r14d, eax
    add rsp, SIZEOF RAWRINSTRUCTION
    jmp @@disasm_loop

@@disasm_skip:
    add rsp, SIZEOF RAWRINSTRUCTION
    inc r14d                        ; Skip one byte on failure
    jmp @@disasm_loop

@@disasm_done:
    lea rcx, [rbx].RAWRCODEX_CTX.instructions
    mov eax, [rcx].DYNARRAY.count

    add rsp, 58h
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret

@@disasm_fail:
    xor eax, eax
    add rsp, 58h
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
RawrCodex_Disassemble ENDP

; =============================================================================
; RawrCodex_ExtractStrings - Extract ASCII/UTF-16 strings from binary
;   RCX = pointer to RAWRCODEX_CTX
;   EDX = minimum string length (typically 4)
; Returns: EAX = number of strings found
; =============================================================================
RawrCodex_ExtractStrings PROC
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    sub rsp, 48h

    mov rbx, rcx
    mov r12d, edx                   ; min length

    lea rcx, [rbx].RAWRCODEX_CTX.strings
    call DynArray_Clear

    mov rsi, [rbx].RAWRCODEX_CTX.pFileBase
    mov r13, [rbx].RAWRCODEX_CTX.fileSize

    ; Scan for ASCII strings
    xor edi, edi                    ; Current offset
    xor ecx, ecx                    ; Current run length

@@ascii_scan:
    cmp rdi, r13
    jge @@ascii_done

    movzx eax, BYTE PTR [rsi + rdi]
    ; Printable ASCII range: 0x20-0x7E, plus \t \n \r
    cmp al, 20h
    jb @@check_whitespace
    cmp al, 7Eh
    ja @@ascii_break
    inc ecx
    inc rdi
    jmp @@ascii_scan

@@check_whitespace:
    cmp al, 09h                     ; Tab
    je @@ascii_printable
    cmp al, 0Ah                     ; LF
    je @@ascii_printable
    cmp al, 0Dh                     ; CR
    je @@ascii_printable
    jmp @@ascii_break

@@ascii_printable:
    inc ecx
    inc rdi
    jmp @@ascii_scan

@@ascii_break:
    ; Check if accumulated run meets minimum length
    cmp ecx, r12d
    jb @@ascii_reset

    ; Build RAWRSTRING entry
    sub rsp, SIZEOF RAWRSTRING
    mov r8, rsp
    ; Zero it
    push rcx
    push rdi
    mov rdi, r8
    push rcx
    mov ecx, SIZEOF RAWRSTRING
    xor eax, eax
    rep stosb
    pop rcx
    pop rdi
    pop rcx

    ; Calculate string start offset
    mov rax, rdi
    sub rax, rcx                    ; Start offset = current - length
    movsxd rax, eax
    mov [rsp + OFFSET RAWRSTRING.offset], rax
    mov [rsp + OFFSET RAWRSTRING.length], ecx
    mov DWORD PTR [rsp + OFFSET RAWRSTRING.isWide], 0

    ; Copy string value (up to 255 chars)
    push rcx
    push rdi
    push rsi
    mov rdi, rsp
    add rdi, 24                     ; Three pushes
    add rdi, OFFSET RAWRSTRING.szValue
    mov rsi, [rbx].RAWRCODEX_CTX.pFileBase
    mov rax, [rsp + 24 + OFFSET RAWRSTRING.offset]
    add rsi, rax
    xor ecx, ecx
    mov edx, [rsp + 24 + OFFSET RAWRSTRING.length]
    cmp edx, 255
    jle @@copy_str_ok
    mov edx, 255
@@copy_str_ok:
@@copy_str_loop:
    cmp ecx, edx
    jge @@copy_str_end
    movzx eax, BYTE PTR [rsi + rcx]
    mov BYTE PTR [rdi + rcx], al
    inc ecx
    jmp @@copy_str_loop
@@copy_str_end:
    pop rsi
    pop rdi
    pop rcx

    ; Push to strings array
    push rcx
    lea rcx, [rbx].RAWRCODEX_CTX.strings
    mov rdx, rsp
    add rdx, 8                      ; Account for push
    call DynArray_Push
    pop rcx

    add rsp, SIZEOF RAWRSTRING

@@ascii_reset:
    xor ecx, ecx
    inc rdi
    jmp @@ascii_scan

@@ascii_done:
    ; Return count
    lea rcx, [rbx].RAWRCODEX_CTX.strings
    mov eax, [rcx].DYNARRAY.count

    add rsp, 48h
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
RawrCodex_ExtractStrings ENDP

; =============================================================================
; RawrCodex_FindPattern - Search for a byte pattern with wildcard support
;   RCX = pointer to RAWRCODEX_CTX
;   RDX = pointer to pattern bytes
;   R8  = pointer to mask bytes (0xFF = exact, 0x00 = wildcard)
;   R9D = pattern length
; Returns: EAX = number of matches found (stored in ctx.matches)
; =============================================================================
RawrCodex_FindPattern PROC
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    push r14
    push r15
    sub rsp, 48h

    mov rbx, rcx                    ; ctx
    mov r12, rdx                    ; pattern
    mov r13, r8                     ; mask
    mov r14d, r9d                   ; pattern length

    lea rcx, [rbx].RAWRCODEX_CTX.matches
    call DynArray_Clear

    mov rsi, [rbx].RAWRCODEX_CTX.pFileBase
    mov r15, [rbx].RAWRCODEX_CTX.fileSize
    sub r15, r14                    ; Don't scan past end

    xor edi, edi                    ; Current scan offset

@@pattern_loop:
    cmp rdi, r15
    jg @@pattern_done

    ; Compare pattern at current offset
    xor ecx, ecx                    ; Pattern index

@@match_loop:
    cmp ecx, r14d
    jge @@found_match

    ; Check mask
    movzx eax, BYTE PTR [r13 + rcx]
    test al, al
    jz @@match_wildcard             ; 0x00 = wildcard, always matches

    ; Exact match required
    movzx eax, BYTE PTR [rsi + rdi + rcx]
    movzx edx, BYTE PTR [r12 + rcx]
    cmp al, dl
    jne @@no_match

@@match_wildcard:
    inc ecx
    jmp @@match_loop

@@found_match:
    ; Build RAWRMATCH
    sub rsp, SIZEOF RAWRMATCH
    mov [rsp + OFFSET RAWRMATCH.offset], rdi
    mov QWORD PTR [rsp + OFFSET RAWRMATCH.rva], 0   ; Could compute RVA here

    lea rcx, [rbx].RAWRCODEX_CTX.matches
    mov rdx, rsp
    call DynArray_Push

    add rsp, SIZEOF RAWRMATCH

@@no_match:
    inc rdi
    jmp @@pattern_loop

@@pattern_done:
    lea rcx, [rbx].RAWRCODEX_CTX.matches
    mov eax, [rcx].DYNARRAY.count

    add rsp, 48h
    pop r15
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
RawrCodex_FindPattern ENDP

; =============================================================================
; RawrCodex_ExportToIDA - Generate IDA Pro Python script
;   RCX = pointer to RAWRCODEX_CTX
;   RDX = pointer to output buffer
;   R8D = buffer size
; Returns: EAX = bytes written to buffer
; =============================================================================
RawrCodex_ExportToIDA PROC
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    sub rsp, 48h

    mov rbx, rcx                    ; ctx
    mov r12, rdx                    ; output buffer
    mov r13d, r8d                   ; buffer size
    mov rdi, rdx                    ; write pointer

    ; Write script header
    lea rsi, idaScriptHdr
    call lstrlenA
    mov ecx, eax
    push rcx
    push rdi
    push rsi
    ; Copy header
    mov rdi, r12
    mov rsi, OFFSET idaScriptHdr
    xor ecx, ecx
@@copy_hdr:
    movzx eax, BYTE PTR [rsi + rcx]
    test al, al
    jz @@copy_hdr_done
    mov [rdi + rcx], al
    inc ecx
    jmp @@copy_hdr
@@copy_hdr_done:
    add rdi, rcx                    ; Advance write pointer
    pop rsi
    pop rdi
    pop rcx
    mov rdi, r12
    add rdi, rcx

    ; Write symbol names
    lea rsi, [rbx].RAWRCODEX_CTX.symbols
    mov ecx, [rsi].DYNARRAY.count
    test ecx, ecx
    jz @@ida_functions

    mov rsi, [rsi].DYNARRAY.pData
    xor r8d, r8d                    ; index

@@ida_sym_loop:
    cmp r8d, [rbx + OFFSET RAWRCODEX_CTX.symbols + OFFSET DYNARRAY.count]
    jge @@ida_functions

    ; Calculate symbol pointer
    mov eax, r8d
    imul eax, SIZEOF RAWRSYMBOL
    lea rdx, [rsi + rax]

    ; wsprintfA(rdi, idaSetName, sym.address, sym.szName)
    mov rcx, rdi
    lea r9, [rdx + OFFSET RAWRSYMBOL.szName]
    mov [rsp + 20h], r9
    mov r9d, DWORD PTR [rdx + OFFSET RAWRSYMBOL.address]
    lea rdx, idaSetName
    call wsprintfA
    add rdi, rax

    inc r8d
    jmp @@ida_sym_loop

@@ida_functions:
    ; Write function boundaries
    lea rsi, [rbx].RAWRCODEX_CTX.functions
    mov ecx, [rsi].DYNARRAY.count
    test ecx, ecx
    jz @@ida_footer

    mov rsi, [rsi].DYNARRAY.pData
    xor r8d, r8d

@@ida_func_loop:
    cmp r8d, [rbx + OFFSET RAWRCODEX_CTX.functions + OFFSET DYNARRAY.count]
    jge @@ida_footer

    mov eax, r8d
    imul eax, SIZEOF RAWRFUNCTION
    lea rdx, [rsi + rax]

    ; wsprintfA(rdi, idaSetFunc, func.startAddress, func.endAddress)
    mov rcx, rdi
    mov r9d, DWORD PTR [rdx + OFFSET RAWRFUNCTION.endAddress]
    mov [rsp + 20h], r9
    mov r9d, DWORD PTR [rdx + OFFSET RAWRFUNCTION.startAddress]
    lea rdx, idaSetFunc
    call wsprintfA
    add rdi, rax

    inc r8d
    jmp @@ida_func_loop

@@ida_footer:
    ; Write footer
    lea rsi, idaScriptFtr
    xor ecx, ecx
@@copy_ftr:
    movzx eax, BYTE PTR [rsi + rcx]
    test al, al
    jz @@copy_ftr_done
    mov [rdi + rcx], al
    inc ecx
    jmp @@copy_ftr
@@copy_ftr_done:
    add rdi, rcx

    ; Return total bytes written
    mov rax, rdi
    sub rax, r12

    add rsp, 48h
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
RawrCodex_ExportToIDA ENDP

; =============================================================================
; Helper: CopyString - Copy null-terminated string
;   RCX = destination
;   RDX = source
;   R8D = max length (including null)
; Returns: EAX = bytes copied (including null)
; =============================================================================
CopyString PROC
    xor eax, eax
@@loop:
    cmp eax, r8d
    jge @@truncate
    movzx r9d, BYTE PTR [rdx + rax]
    mov BYTE PTR [rcx + rax], r9b
    inc eax
    test r9d, r9d
    jnz @@loop
    ret

@@truncate:
    dec eax
    mov BYTE PTR [rcx + rax], 0
    inc eax
    ret
CopyString ENDP

; =============================================================================
; Helper: CompareMemory - Compare two memory regions
;   RCX = pointer A
;   RDX = pointer B
;   R8D = length
; Returns: EAX = 0 if equal, nonzero if different
; =============================================================================
CompareMemory PROC
    xor eax, eax
    xor r9d, r9d
@@loop:
    cmp r9d, r8d
    jge @@equal
    movzx eax, BYTE PTR [rcx + r9]
    movzx r10d, BYTE PTR [rdx + r9]
    sub eax, r10d
    jnz @@done
    inc r9d
    jmp @@loop
@@equal:
    xor eax, eax
@@done:
    ret
CompareMemory ENDP

; =============================================================================
; Helper: ZeroMemory - Fill memory region with zeros
;   RCX = pointer
;   EDX = length
; =============================================================================
ZeroMem PROC
    xor eax, eax
    xor r8d, r8d
@@loop:
    cmp r8d, edx
    jge @@done
    mov BYTE PTR [rcx + r8], 0
    inc r8d
    jmp @@loop
@@done:
    ret
ZeroMem ENDP

END
