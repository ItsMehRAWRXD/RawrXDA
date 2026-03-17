;=============================================================================
; PE32+ WRITER — Native x64 MASM tool for the RawrXD IDE toolchain
; Builds a runnable PE32+ executable from a flat buffer, writes to disk.
; Invoked by the IDE when native MASM compilation is selected.
;
; The generated output.exe calls ExitProcess(0) — minimal valid PE.
; Extend BuildImage to emit arbitrary payloads.
;
; Build:
;   ml64 /c /nologo pe_emitter.asm
;   link /nologo /subsystem:console /entry:main pe_emitter.obj kernel32.lib
;
; Run:
;   pe_emitter.exe          -> writes output.exe
;   output.exe              -> exits cleanly with code 0
;=============================================================================

option casemap:none

;-----------------------------------------------------------------------------
; Build-time imports (for this tool — not for the generated PE)
;-----------------------------------------------------------------------------
CreateFileA     PROTO :QWORD,:DWORD,:DWORD,:QWORD,:DWORD,:DWORD,:QWORD
WriteFile       PROTO :QWORD,:QWORD,:DWORD,:QWORD,:QWORD
CloseHandle     PROTO :QWORD
ExitProcess     PROTO :DWORD
GetStdHandle    PROTO :DWORD

;-----------------------------------------------------------------------------
; Constants
;-----------------------------------------------------------------------------
GENERIC_WRITE           EQU 40000000h
CREATE_ALWAYS           EQU 2
FILE_ATTRIBUTE_NORMAL   EQU 80h
STD_OUTPUT_HANDLE       EQU -11

; PE layout constants
FILE_ALIGN              EQU 0200h
SECT_ALIGN              EQU 1000h
TEXT_RVA                EQU 1000h
RDATA_RVA               EQU 2000h
TEXT_RAW                EQU 0200h
RDATA_RAW               EQU 0400h
IMAGE_SIZE              EQU 0600h       ; 3 * FILE_ALIGN = 1536 bytes

; Helper macros
StoreWord MACRO base, offs, val
    mov     WORD PTR [base + offs], val
ENDM

StoreDword MACRO base, offs, val
    mov     DWORD PTR [base + offs], val
ENDM

StoreQword MACRO base, offs, val
    mov     rax, val
    mov     QWORD PTR [base + offs], rax
ENDM

StoreByte MACRO base, offs, val
    mov     BYTE PTR [base + offs], val
ENDM

;=============================================================================
; DATA SEGMENT
;=============================================================================
.data

szOutFile   db "output.exe", 0
szOK        db "PE32+ written: output.exe", 13, 10, 0
szOKLen     EQU 27
szErr       db "ERROR: cannot create output.exe", 13, 10, 0
szErrLen    EQU 32
bw          dd 0

ALIGN 16
peImg       db IMAGE_SIZE dup(0)

;=============================================================================
; CODE SEGMENT
;=============================================================================
.code

;-----------------------------------------------------------------------------
; BuildImage — constructs the entire PE32+ in peImg
;
; Generated executable layout:
;   Headers [0000-01FF]  DOS + PE + Optional + 2 Section Headers
;   .text   [0200-03FF]  sub rsp,28h / xor ecx,ecx / call [ExitProcess] / epilogue
;   .rdata  [0400-05FF]  Import Dir + ILT + IAT + HintName + DLL name
;
; Import table (single DLL, single function):
;   kernel32.dll -> ExitProcess
;
;   .rdata internal layout (offsets from .rdata start):
;     +0000  IDT[0] kernel32.dll  (20 bytes)
;     +0014  IDT[1] null term     (20 bytes)
;     +0028  ILT:  QWORD -> HN_ExitProcess, QWORD 0
;     +0038  IAT:  QWORD -> HN_ExitProcess, QWORD 0  (loader overwrites)
;     +0048  HN:   WORD hint=0, "ExitProcess\0"
;     +0056  DLL:  "kernel32.dll\0"
;
;   RVAs (add RDATA_RVA = 2000h):
;     ILT  = 2028h
;     IAT  = 2038h
;     HN   = 2048h
;     DLL  = 2056h
;
;   .text code references IAT_ExitProcess at RVA 2038h
;-----------------------------------------------------------------------------
BuildImage PROC
    push    rbx
    push    rsi
    sub     rsp, 28h

    lea     rdi, [peImg]

    ;==================================================================
    ; DOS Header (64 bytes)
    ;==================================================================
    StoreWord rdi, 00h, 5A4Dh              ; e_magic = "MZ"
    StoreDword rdi, 3Ch, 00000080h         ; e_lfanew -> PE sig @ 80h

    ;==================================================================
    ; DOS Stub @ 40h (minimal: INT 21h / RET)
    ;==================================================================
    StoreByte rdi, 40h, 0CDh
    StoreByte rdi, 41h, 21h
    StoreByte rdi, 42h, 0C3h

    ;==================================================================
    ; PE Signature @ 80h
    ;==================================================================
    StoreDword rdi, 80h, 00004550h         ; "PE\0\0"

    ;==================================================================
    ; COFF File Header @ 84h (20 bytes)
    ;==================================================================
    StoreWord  rdi, 84h, 8664h             ; Machine = AMD64
    StoreWord  rdi, 86h, 0002h             ; NumberOfSections = 2
    StoreDword rdi, 88h, 00000000h         ; TimeDateStamp
    StoreDword rdi, 8Ch, 00000000h         ; PointerToSymbolTable
    StoreDword rdi, 90h, 00000000h         ; NumberOfSymbols
    StoreWord  rdi, 94h, 00F0h             ; SizeOfOptionalHeader = 240
    StoreWord  rdi, 96h, 0022h             ; Characteristics = EXEC|LARGE_ADDR

    ;==================================================================
    ; Optional Header (PE32+) @ 98h (240 bytes)
    ;==================================================================
    StoreWord  rdi, 98h, 020Bh             ; Magic = PE32+
    StoreByte  rdi, 9Ah, 14                ; MajorLinkerVersion
    StoreByte  rdi, 9Bh, 0                 ; MinorLinkerVersion
    StoreDword rdi, 9Ch, 00000200h         ; SizeOfCode
    StoreDword rdi, 0A0h, 00000200h        ; SizeOfInitializedData
    StoreDword rdi, 0A4h, 00000000h        ; SizeOfUninitializedData
    StoreDword rdi, 0A8h, 00001000h        ; AddressOfEntryPoint = TEXT_RVA
    StoreDword rdi, 0ACh, 00001000h        ; BaseOfCode

    ; ImageBase = 0x0000000140000000
    StoreDword rdi, 0B0h, 40000000h        ; lo
    StoreDword rdi, 0B4h, 00000001h        ; hi

    StoreDword rdi, 0B8h, 00001000h        ; SectionAlignment
    StoreDword rdi, 0BCh, 00000200h        ; FileAlignment
    StoreWord  rdi, 0C0h, 0006h            ; MajorOSVersion
    StoreWord  rdi, 0C2h, 0000h            ; MinorOSVersion
    StoreWord  rdi, 0C4h, 0000h            ; MajorImageVersion
    StoreWord  rdi, 0C6h, 0000h            ; MinorImageVersion
    StoreWord  rdi, 0C8h, 0006h            ; MajorSubsystemVersion
    StoreWord  rdi, 0CAh, 0000h            ; MinorSubsystemVersion
    StoreDword rdi, 0CCh, 00000000h        ; Win32VersionValue

    ; SizeOfImage = headers(1000h) + .text(1000h) + .rdata(1000h) = 3000h
    StoreDword rdi, 0D0h, 00003000h
    StoreDword rdi, 0D4h, 00000200h        ; SizeOfHeaders
    StoreDword rdi, 0D8h, 00000000h        ; CheckSum
    StoreWord  rdi, 0DCh, 0003h            ; Subsystem = CUI
    StoreWord  rdi, 0DEh, 8160h            ; DllCharacteristics

    ; Stack/Heap reserves & commits
    StoreQword rdi, 0E0h, 0000000000100000h     ; StackReserve
    StoreQword rdi, 0E8h, 0000000000001000h     ; StackCommit
    StoreQword rdi, 0F0h, 0000000000100000h     ; HeapReserve
    StoreQword rdi, 0F8h, 0000000000001000h     ; HeapCommit

    StoreDword rdi, 100h, 00000000h        ; LoaderFlags
    StoreDword rdi, 104h, 00000010h        ; NumberOfRvaAndSizes = 16

    ;=== Data Directories ===
    ; [1] Import Directory: RVA=2000h, Size=28h (2×20=40 bytes, 2 entries incl. null)
    StoreDword rdi, 110h, 00002000h
    StoreDword rdi, 114h, 00000028h

    ; [12] IAT: RVA=2038h, Size=10h (1 QWORD entry + null)
    StoreDword rdi, 168h, 00002038h
    StoreDword rdi, 16Ch, 00000010h

    ;==================================================================
    ; Section Headers (2 × 40 bytes)
    ;==================================================================

    ;--- .text @ 188h ---
    StoreByte  rdi, 188h, '.'
    StoreByte  rdi, 189h, 't'
    StoreByte  rdi, 18Ah, 'e'
    StoreByte  rdi, 18Bh, 'x'
    StoreByte  rdi, 18Ch, 't'
    StoreDword rdi, 190h, 00000200h        ; VirtualSize
    StoreDword rdi, 194h, 00001000h        ; VirtualAddress
    StoreDword rdi, 198h, 00000200h        ; SizeOfRawData
    StoreDword rdi, 19Ch, 00000200h        ; PointerToRawData
    StoreDword rdi, 1ACh, 60000020h        ; CODE|EXECUTE|READ

    ;--- .rdata @ 1B0h ---
    StoreByte  rdi, 1B0h, '.'
    StoreByte  rdi, 1B1h, 'r'
    StoreByte  rdi, 1B2h, 'd'
    StoreByte  rdi, 1B3h, 'a'
    StoreByte  rdi, 1B4h, 't'
    StoreByte  rdi, 1B5h, 'a'
    StoreDword rdi, 1B8h, 00000200h        ; VirtualSize
    StoreDword rdi, 1BCh, 00002000h        ; VirtualAddress
    StoreDword rdi, 1C0h, 00000200h        ; SizeOfRawData
    StoreDword rdi, 1C4h, 00000400h        ; PointerToRawData
    StoreDword rdi, 1D4h, 40000040h        ; INITIALIZED_DATA|READ

    ;==================================================================
    ; Import Table — .rdata section (file offset 400h)
    ;==================================================================
    lea     rsi, [rdi + 400h]

    ;--- IDT[0]: kernel32.dll ---
    ; OriginalFirstThunk (ILT RVA) = 2028h
    StoreDword rsi, 00h, 00002028h
    ; TimeDateStamp
    StoreDword rsi, 04h, 00000000h
    ; ForwarderChain
    StoreDword rsi, 08h, 00000000h
    ; Name RVA = 2056h
    StoreDword rsi, 0Ch, 00002056h
    ; FirstThunk (IAT RVA) = 2038h
    StoreDword rsi, 10h, 00002038h

    ;--- IDT[1]: null terminator (20 zero bytes — already zeroed) ---

    ;--- ILT @ +28h: QWORD -> HN_ExitProcess at RVA 2048h ---
    StoreDword rsi, 28h, 00002048h
    StoreDword rsi, 2Ch, 00000000h
    ; ILT null terminator (QWORD zero — already zeroed)

    ;--- IAT @ +38h: same content as ILT (loader overwrites at load time) ---
    StoreDword rsi, 38h, 00002048h
    StoreDword rsi, 3Ch, 00000000h
    ; IAT null terminator (QWORD zero — already zeroed)

    ;--- Hint/Name: ExitProcess @ +48h ---
    StoreWord  rsi, 48h, 0000h             ; hint = 0
    StoreByte  rsi, 4Ah, 'E'
    StoreByte  rsi, 4Bh, 'x'
    StoreByte  rsi, 4Ch, 'i'
    StoreByte  rsi, 4Dh, 't'
    StoreByte  rsi, 4Eh, 'P'
    StoreByte  rsi, 4Fh, 'r'
    StoreByte  rsi, 50h, 'o'
    StoreByte  rsi, 51h, 'c'
    StoreByte  rsi, 52h, 'e'
    StoreByte  rsi, 53h, 's'
    StoreByte  rsi, 54h, 's'
    ; null terminator at +55h already zero

    ;--- DLL Name: "kernel32.dll" @ +56h ---
    StoreByte  rsi, 56h, 'k'
    StoreByte  rsi, 57h, 'e'
    StoreByte  rsi, 58h, 'r'
    StoreByte  rsi, 59h, 'n'
    StoreByte  rsi, 5Ah, 'e'
    StoreByte  rsi, 5Bh, 'l'
    StoreByte  rsi, 5Ch, '3'
    StoreByte  rsi, 5Dh, '2'
    StoreByte  rsi, 5Eh, '.'
    StoreByte  rsi, 5Fh, 'd'
    StoreByte  rsi, 60h, 'l'
    StoreByte  rsi, 61h, 'l'
    ; null terminator at +62h already zero

    ;==================================================================
    ; .text Machine Code (file offset 200h)
    ;
    ; Generated program:
    ;   sub rsp, 28h           ; prologue (shadow space + alignment)
    ;   xor ecx, ecx           ; exit code = 0
    ;   call [rip+disp32]      ; call ExitProcess via IAT
    ;   add rsp, 28h           ; epilogue (unreachable)
    ;   ret
    ;
    ; IAT_ExitProcess is at RVA 2038h.
    ; RIP at end of call instruction = TEXT_RVA + 0Ch = 100Ch.
    ; disp32 = 2038h - 100Ch = 102Ch.
    ;==================================================================
    lea     rsi, [rdi + 200h]

    ; sub rsp, 28h  (48 83 EC 28)
    StoreByte  rsi, 00h, 48h
    StoreByte  rsi, 01h, 83h
    StoreByte  rsi, 02h, 0ECh
    StoreByte  rsi, 03h, 28h

    ; xor ecx, ecx  (33 C9)
    StoreByte  rsi, 04h, 33h
    StoreByte  rsi, 05h, 0C9h

    ; call [rip+102Ch]  (FF 15 2C100000)
    StoreByte  rsi, 06h, 0FFh
    StoreByte  rsi, 07h, 15h
    StoreDword rsi, 08h, 0000102Ch

    ; add rsp, 28h  (48 83 C4 28) — epilogue, unreachable
    StoreByte  rsi, 0Ch, 48h
    StoreByte  rsi, 0Dh, 83h
    StoreByte  rsi, 0Eh, 0C4h
    StoreByte  rsi, 0Fh, 28h

    ; ret  (C3)
    StoreByte  rsi, 10h, 0C3h

    add     rsp, 28h
    pop     rsi
    pop     rbx
    ret
BuildImage ENDP

;-----------------------------------------------------------------------------
; Emit_FunctionPrologue — emit sub rsp, imm8
;   RDI = code buffer pointer, SIL = stack size (imm8)
;   Returns: RAX = bytes emitted (4)
;-----------------------------------------------------------------------------
Emit_FunctionPrologue PROC
    mov     BYTE PTR [rdi+0], 48h          ; REX.W
    mov     BYTE PTR [rdi+1], 83h          ; sub r/m64, imm8
    mov     BYTE PTR [rdi+2], 0ECh         ; ModRM: rsp
    mov     BYTE PTR [rdi+3], sil          ; imm8
    mov     eax, 4
    ret
Emit_FunctionPrologue ENDP

;-----------------------------------------------------------------------------
; Emit_FunctionEpilogue — emit add rsp, imm8 + ret
;   RDI = code buffer pointer, SIL = stack size (imm8)
;   Returns: RAX = bytes emitted (5)
;-----------------------------------------------------------------------------
Emit_FunctionEpilogue PROC
    mov     BYTE PTR [rdi+0], 48h          ; REX.W
    mov     BYTE PTR [rdi+1], 83h          ; add r/m64, imm8
    mov     BYTE PTR [rdi+2], 0C4h         ; ModRM: rsp
    mov     BYTE PTR [rdi+3], sil          ; imm8
    mov     BYTE PTR [rdi+4], 0C3h         ; ret
    mov     eax, 5
    ret
Emit_FunctionEpilogue ENDP

;-----------------------------------------------------------------------------
; Emit_FunctionPrologueFP — push rbp / mov rbp,rsp / sub rsp,N
;   RDI = code buffer, SIL = local space (imm8)
;   Returns: RAX = 8
;-----------------------------------------------------------------------------
Emit_FunctionPrologueFP PROC
    mov     BYTE PTR [rdi+0], 55h          ; push rbp
    mov     BYTE PTR [rdi+1], 48h          ; REX.W
    mov     BYTE PTR [rdi+2], 89h          ; mov
    mov     BYTE PTR [rdi+3], 0E5h         ; ModRM: rbp, rsp
    mov     BYTE PTR [rdi+4], 48h          ; REX.W
    mov     BYTE PTR [rdi+5], 83h          ; sub r/m64, imm8
    mov     BYTE PTR [rdi+6], 0ECh         ; ModRM: rsp
    mov     BYTE PTR [rdi+7], sil          ; imm8
    mov     eax, 8
    ret
Emit_FunctionPrologueFP ENDP

;-----------------------------------------------------------------------------
; Emit_FunctionEpilogueFP — mov rsp,rbp / pop rbp / ret
;   RDI = code buffer
;   Returns: RAX = 5
;-----------------------------------------------------------------------------
Emit_FunctionEpilogueFP PROC
    mov     BYTE PTR [rdi+0], 48h          ; REX.W
    mov     BYTE PTR [rdi+1], 89h          ; mov
    mov     BYTE PTR [rdi+2], 0ECh         ; ModRM: rsp, rbp
    mov     BYTE PTR [rdi+3], 5Dh          ; pop rbp
    mov     BYTE PTR [rdi+4], 0C3h         ; ret
    mov     eax, 5
    ret
Emit_FunctionEpilogueFP ENDP

;-----------------------------------------------------------------------------
; Emit_CallIndirectRIP — emit FF 15 disp32 (call [rip+disp32])
;   RDI = code buffer, ESI = disp32
;   Returns: RAX = 6
;-----------------------------------------------------------------------------
Emit_CallIndirectRIP PROC
    mov     BYTE PTR [rdi+0], 0FFh
    mov     BYTE PTR [rdi+1], 15h
    mov     DWORD PTR [rdi+2], esi
    mov     eax, 6
    ret
Emit_CallIndirectRIP ENDP

;-----------------------------------------------------------------------------
; Emit_LeaRIPReg — emit lea reg, [rip+disp32]
;   ECX = register (0-15), ESI = disp32
;   Returns: RAX = 7
;-----------------------------------------------------------------------------
Emit_LeaRIPReg PROC
    cmp     ecx, 8
    jge     @F
    ; Registers 0-7: REX.W = 48h
    mov     BYTE PTR [rdi+0], 48h
    mov     eax, ecx
    shl     eax, 3
    or      eax, 05h
    jmp     @store
@@:
    ; Registers 8-15: REX.WR = 4Ch
    mov     BYTE PTR [rdi+0], 4Ch
    mov     eax, ecx
    sub     eax, 8
    shl     eax, 3
    or      eax, 05h
@store:
    mov     BYTE PTR [rdi+1], 8Dh          ; LEA opcode
    mov     BYTE PTR [rdi+2], al           ; ModRM
    mov     DWORD PTR [rdi+3], esi         ; disp32
    mov     eax, 7
    ret
Emit_LeaRIPReg ENDP

;-----------------------------------------------------------------------------
; Emit_XorReg32 — emit xor r32, r32 (zero a register)
;   ECX = register (0-7)
;   Returns: RAX = 2
;-----------------------------------------------------------------------------
Emit_XorReg32 PROC
    mov     BYTE PTR [rdi+0], 33h
    mov     eax, ecx
    shl     eax, 3
    or      eax, ecx
    or      eax, 0C0h
    mov     BYTE PTR [rdi+1], al
    mov     eax, 2
    ret
Emit_XorReg32 ENDP

;-----------------------------------------------------------------------------
; Emit_MovRegImm64 — emit movabs reg, imm64
;   ECX = register (0-15), RDX = imm64 value
;   Returns: RAX = 10
;-----------------------------------------------------------------------------
Emit_MovRegImm64 PROC
    cmp     ecx, 8
    jge     @F
    mov     BYTE PTR [rdi+0], 48h          ; REX.W
    lea     eax, [ecx + 0B8h]              ; opcode B8+reg
    mov     BYTE PTR [rdi+1], al
    mov     QWORD PTR [rdi+2], rdx
    mov     eax, 10
    ret
@@:
    mov     BYTE PTR [rdi+0], 49h          ; REX.WB
    mov     eax, ecx
    sub     eax, 8
    add     eax, 0B8h
    mov     BYTE PTR [rdi+1], al
    mov     QWORD PTR [rdi+2], rdx
    mov     eax, 10
    ret
Emit_MovRegImm64 ENDP

;-----------------------------------------------------------------------------
; Emit_PushReg — emit push r64
;   ECX = register (0-15)
;   Returns: RAX = 1 or 2
;-----------------------------------------------------------------------------
Emit_PushReg PROC
    cmp     ecx, 8
    jge     @F
    lea     eax, [ecx + 50h]
    mov     BYTE PTR [rdi+0], al
    mov     eax, 1
    ret
@@:
    mov     BYTE PTR [rdi+0], 41h
    mov     eax, ecx
    sub     eax, 8
    add     eax, 50h
    mov     BYTE PTR [rdi+1], al
    mov     eax, 2
    ret
Emit_PushReg ENDP

;-----------------------------------------------------------------------------
; Emit_PopReg — emit pop r64
;   ECX = register (0-15)
;   Returns: RAX = 1 or 2
;-----------------------------------------------------------------------------
Emit_PopReg PROC
    cmp     ecx, 8
    jge     @F
    lea     eax, [ecx + 58h]
    mov     BYTE PTR [rdi+0], al
    mov     eax, 1
    ret
@@:
    mov     BYTE PTR [rdi+0], 41h
    mov     eax, ecx
    sub     eax, 8
    add     eax, 58h
    mov     BYTE PTR [rdi+1], al
    mov     eax, 2
    ret
Emit_PopReg ENDP

;=============================================================================
; WritePE — writes peImg to disk as "output.exe"
;=============================================================================
WritePE PROC
    push    rbx
    sub     rsp, 30h

    ; CreateFileA("output.exe", GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, NORMAL, NULL)
    lea     rcx, [szOutFile]
    mov     edx, GENERIC_WRITE
    xor     r8d, r8d
    xor     r9, r9
    mov     DWORD PTR [rsp+20h], CREATE_ALWAYS
    mov     DWORD PTR [rsp+28h], FILE_ATTRIBUTE_NORMAL
    mov     QWORD PTR [rsp+30h], 0
    call    CreateFileA

    cmp     rax, -1
    je      @wfail

    mov     rbx, rax                       ; save handle

    ; WriteFile(hFile, peImg, IMAGE_SIZE, &bw, NULL)
    mov     rcx, rbx
    lea     rdx, [peImg]
    mov     r8d, IMAGE_SIZE
    lea     r9, [bw]
    mov     QWORD PTR [rsp+20h], 0
    call    WriteFile

    ; CloseHandle
    mov     rcx, rbx
    call    CloseHandle

    ; Print success
    mov     ecx, STD_OUTPUT_HANDLE
    call    GetStdHandle
    mov     rcx, rax
    lea     rdx, [szOK]
    mov     r8d, szOKLen
    lea     r9, [bw]
    mov     QWORD PTR [rsp+20h], 0
    call    WriteFile

    xor     eax, eax                       ; return 0 = success
    add     rsp, 30h
    pop     rbx
    ret

@wfail:
    ; Print error
    mov     ecx, STD_OUTPUT_HANDLE
    call    GetStdHandle
    mov     rcx, rax
    lea     rdx, [szErr]
    mov     r8d, szErrLen
    lea     r9, [bw]
    mov     QWORD PTR [rsp+20h], 0
    call    WriteFile

    mov     eax, 1                         ; return 1 = failure
    add     rsp, 30h
    pop     rbx
    ret
WritePE ENDP

;=============================================================================
; main — entry point
;=============================================================================
main PROC
    sub     rsp, 28h

    call    BuildImage
    call    WritePE

    ; Exit with WritePE return code
    mov     ecx, eax
    call    ExitProcess

    ; Unreachable epilogue
    add     rsp, 28h
    ret
main ENDP

END
