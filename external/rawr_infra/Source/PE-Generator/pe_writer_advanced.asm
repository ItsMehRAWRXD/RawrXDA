; ═══════════════════════════════════════════════════════════════════
; ADVANCED PE32+ WRITER & MACHINE CODE EMITTER - x64 MASM
; Zero dependencies, backend designer style
; Generates runnable x64 executables with multi-DLL import tables
; Emits machine code to call MessageBoxA and ExitProcess
; ═══════════════════════════════════════════════════════════════════

option casemap:none

; ── Win32 API Imports ──────────────────────────────────────────────
EXTERN GetProcessHeap:PROC
EXTERN HeapAlloc:PROC
EXTERN HeapFree:PROC
EXTERN CreateFileA:PROC
EXTERN WriteFile:PROC
EXTERN CloseHandle:PROC
EXTERN GetStdHandle:PROC
EXTERN ExitProcess:PROC

; ── Constants ──────────────────────────────────────────────────────
STD_OUTPUT_HANDLE       equ -11
HEAP_ZERO_MEMORY        equ 8
GENERIC_WRITE           equ 40000000h
CREATE_ALWAYS           equ 2
FILE_ATTRIBUTE_NORMAL   equ 80h
INVALID_HANDLE_VALUE    equ -1

IMAGE_DOS_SIGNATURE     equ 5A4Dh
IMAGE_NT_SIGNATURE      equ 00004550h
IMAGE_FILE_MACHINE_AMD64 equ 8664h
IMAGE_NT_OPTIONAL_HDR64_MAGIC equ 020Bh

IMAGE_SCN_CNT_CODE              equ 00000020h
IMAGE_SCN_CNT_INITIALIZED_DATA  equ 00000040h
IMAGE_SCN_MEM_EXECUTE           equ 20000000h
IMAGE_SCN_MEM_READ              equ 40000000h
IMAGE_SCN_MEM_WRITE             equ 80000000h

IMAGE_FILE_EXECUTABLE_IMAGE     equ 0002h
IMAGE_FILE_LARGE_ADDRESS_AWARE  equ 0020h
IMAGE_SUBSYSTEM_WINDOWS_CUI     equ 3
IMAGE_DLLCHARACTERISTICS_DYNAMIC_BASE    equ 0040h
IMAGE_DLLCHARACTERISTICS_NX_COMPAT       equ 0100h
IMAGE_DLLCHARACTERISTICS_TERMINAL_SERVER_AWARE equ 8000h

; ── PE Structure Definitions ───────────────────────────────────────
IMAGE_DOS_HEADER STRUCT
    e_magic         dw ?
    e_cblp          dw ?
    e_cp            dw ?
    e_crlc          dw ?
    e_cparhdr       dw ?
    e_minalloc      dw ?
    e_maxalloc      dw ?
    e_ss            dw ?
    e_sp            dw ?
    e_csum          dw ?
    e_ip            dw ?
    e_cs            dw ?
    e_lfarlc        dw ?
    e_ovno          dw ?
    e_res           dw 4 dup(?)
    e_oemid         dw ?
    e_oeminfo       dw ?
    e_res2          dw 10 dup(?)
    e_lfanew        dd ?
IMAGE_DOS_HEADER ENDS

IMAGE_FILE_HEADER STRUCT
    Machine                 dw ?
    NumberOfSections        dw ?
    TimeDateStamp           dd ?
    PointerToSymbolTable    dd ?
    NumberOfSymbols         dd ?
    SizeOfOptionalHeader    dw ?
    Characteristics         dw ?
IMAGE_FILE_HEADER ENDS

IMAGE_DATA_DIRECTORY STRUCT
    VirtualAddress  dd ?
    Size_           dd ?
IMAGE_DATA_DIRECTORY ENDS

IMAGE_OPTIONAL_HEADER64 STRUCT
    Magic                       dw ?
    MajorLinkerVersion          db ?
    MinorLinkerVersion          db ?
    SizeOfCode                  dd ?
    SizeOfInitializedData       dd ?
    SizeOfUninitializedData     dd ?
    AddressOfEntryPoint         dd ?
    BaseOfCode                  dd ?
    ImageBase                   dq ?
    SectionAlignment            dd ?
    FileAlignment               dd ?
    MajorOperatingSystemVersion dw ?
    MinorOperatingSystemVersion dw ?
    MajorImageVersion           dw ?
    MinorImageVersion           dw ?
    MajorSubsystemVersion       dw ?
    MinorSubsystemVersion       dw ?
    Win32VersionValue           dd ?
    SizeOfImage                 dd ?
    SizeOfHeaders               dd ?
    CheckSum                    dd ?
    Subsystem                   dw ?
    DllCharacteristics          dw ?
    SizeOfStackReserve          dq ?
    SizeOfStackCommit           dq ?
    SizeOfHeapReserve           dq ?
    SizeOfHeapCommit            dq ?
    LoaderFlags                 dd ?
    NumberOfRvaAndSizes         dd ?
    DataDirectory               IMAGE_DATA_DIRECTORY 16 dup(<>)
IMAGE_OPTIONAL_HEADER64 ENDS

IMAGE_NT_HEADERS64 STRUCT
    Signature       dd ?
    FileHeader      IMAGE_FILE_HEADER <>
    OptionalHeader  IMAGE_OPTIONAL_HEADER64 <>
IMAGE_NT_HEADERS64 ENDS

IMAGE_SECTION_HEADER STRUCT
    Name_                   db 8 dup(?)
    VirtualSize             dd ?
    VirtualAddress          dd ?
    SizeOfRawData           dd ?
    PointerToRawData        dd ?
    PointerToRelocations    dd ?
    PointerToLinenumbers    dd ?
    NumberOfRelocations     dw ?
    NumberOfLinenumbers     dw ?
    Characteristics         dd ?
IMAGE_SECTION_HEADER ENDS

IMAGE_IMPORT_DESCRIPTOR STRUCT
    OriginalFirstThunk  dd ?
    TimeDateStamp       dd ?
    ForwarderChain      dd ?
    Name_               dd ?
    FirstThunk          dd ?
IMAGE_IMPORT_DESCRIPTOR ENDS

; ── Data Section ───────────────────────────────────────────────────
.data
    szOutputFile    db "generated.exe", 0
    szUser32        db "user32.dll", 0
    szKernel32      db "kernel32.dll", 0
    szMessageBoxA   db "MessageBoxA", 0
    szExitProcess   db "ExitProcess", 0
    szTextSection   db ".text", 0, 0, 0
    szIdataSection  db ".idata", 0, 0
    
    szDosStub       db "This program cannot be run in DOS mode.", 0Dh, 0Dh, 0Ah, 24h
    szSuccess       db "Advanced PE file written successfully: generated.exe", 0Dh, 0Ah, 0
    szError         db "Error: Failed to write PE file", 0Dh, 0Ah, 0
    
    szMsgText       db "Hello from RawrXD PE Emitter!", 0
    szMsgCaption    db "Success", 0

    peBuffer        dq 0
    peBufferSize    dd 100000h          ; 1MB
    currentOffset   dd 0
    
    emitterBuffer   dq 0
    emitterSize     dd 0
    
    hFile           dq 0
    bytesWritten    dd 0

.code

; ═══════════════════════════════════════════════════════════════════
; Utility Functions
; ═══════════════════════════════════════════════════════════════════
AlignUp PROC
    push    rbp
    mov     rbp, rsp
    lea     rax, [rcx + rdx - 1]
    dec     rdx
    not     rdx
    and     rax, rdx
    pop     rbp
    ret
AlignUp ENDP

WriteBuffer PROC
    push    rbp
    mov     rbp, rsp
    push    rsi
    push    rdi
    mov     rsi, rcx
    mov     rcx, rdx
    mov     rdi, peBuffer
    add     rdi, qword ptr currentOffset
    rep     movsb
    add     currentOffset, edx
    pop     rdi
    pop     rsi
    pop     rbp
    ret
WriteBuffer ENDP

ZeroBuffer PROC
    push    rbp
    mov     rbp, rsp
    push    rdi
    mov     rdi, peBuffer
    add     rdi, qword ptr currentOffset
    xor     rax, rax
    rep     stosb
    add     currentOffset, ecx
    pop     rdi
    pop     rbp
    ret
ZeroBuffer ENDP

PadToAlignment PROC
    push    rbp
    mov     rbp, rsp
    mov     eax, currentOffset
    mov     edx, eax
    and     eax, ecx
    sub     ecx, 1
    and     eax, ecx
    test    eax, eax
    jz      @done
    mov     ecx, edx
    add     ecx, 1FFh
    and     ecx, 0FFFFFE00h
    sub     ecx, edx
    call    ZeroBuffer
@done:
    pop     rbp
    ret
PadToAlignment ENDP

; ═══════════════════════════════════════════════════════════════════
; Machine Code Emitter Functions
; ═══════════════════════════════════════════════════════════════════
EmitByte PROC b:BYTE
    mov     rdi, emitterBuffer
    add     rdi, qword ptr emitterSize
    mov     al, b
    mov     byte ptr [rdi], al
    inc     emitterSize
    ret
EmitByte ENDP

EmitDword PROC d:DWORD
    mov     rdi, emitterBuffer
    add     rdi, qword ptr emitterSize
    mov     eax, d
    mov     dword ptr [rdi], eax
    add     emitterSize, 4
    ret
EmitDword ENDP

EmitBytes PROC pSrc:QWORD, cbSize:DWORD
    mov     rsi, pSrc
    mov     rdi, emitterBuffer
    add     rdi, qword ptr emitterSize
    mov     ecx, cbSize
    rep     movsb
    mov     eax, cbSize
    add     emitterSize, eax
    ret
EmitBytes ENDP

GenerateMachineCode PROC
    ; push rbp
    mov     cl, 55h
    call    EmitByte
    
    ; mov rbp, rsp
    mov     cl, 48h
    call    EmitByte
    mov     cl, 89h
    call    EmitByte
    mov     cl, 0E5h
    call    EmitByte
    
    ; sub rsp, 28h
    mov     cl, 48h
    call    EmitByte
    mov     cl, 83h
    call    EmitByte
    mov     cl, 0ECh
    call    EmitByte
    mov     cl, 28h
    call    EmitByte
    
    ; xor ecx, ecx
    mov     cl, 31h
    call    EmitByte
    mov     cl, 0C9h
    call    EmitByte
    
    ; lea rdx, [rip+19h]
    mov     cl, 48h
    call    EmitByte
    mov     cl, 8Dh
    call    EmitByte
    mov     cl, 15h
    call    EmitByte
    mov     ecx, 19h
    call    EmitDword
    
    ; lea r8, [rip+30h]
    mov     cl, 4Ch
    call    EmitByte
    mov     cl, 8Dh
    call    EmitByte
    mov     cl, 05h
    call    EmitByte
    mov     ecx, 30h
    call    EmitDword
    
    ; xor r9d, r9d
    mov     cl, 45h
    call    EmitByte
    mov     cl, 31h
    call    EmitByte
    mov     cl, 0C9h
    call    EmitByte
    
    ; call [MessageBoxA] (RIP+103Bh)
    mov     cl, 0FFh
    call    EmitByte
    mov     cl, 15h
    call    EmitByte
    mov     ecx, 103Bh
    call    EmitDword
    
    ; xor ecx, ecx
    mov     cl, 31h
    call    EmitByte
    mov     cl, 0C9h
    call    EmitByte
    
    ; call [ExitProcess] (RIP+1043h)
    mov     cl, 0FFh
    call    EmitByte
    mov     cl, 15h
    call    EmitByte
    mov     ecx, 1043h
    call    EmitDword
    
    ; int 3
    mov     cl, 0CCh
    call    EmitByte
    
    ; Strings
    lea     rcx, szMsgText
    mov     edx, 30
    call    EmitBytes
    
    lea     rcx, szMsgCaption
    mov     edx, 8
    call    EmitBytes
    
    ret
GenerateMachineCode ENDP

; ═══════════════════════════════════════════════════════════════════
; PE Builders
; ═══════════════════════════════════════════════════════════════════
BuildDosHeader PROC
    push    rbp
    mov     rbp, rsp
    sub     rsp, 40h
    sub     rsp, sizeof IMAGE_DOS_HEADER
    mov     rdi, rsp
    xor     rax, rax
    mov     rcx, sizeof IMAGE_DOS_HEADER
    rep     stosb
    
    mov     rdi, rsp
    mov     word ptr IMAGE_DOS_HEADER.e_magic[rdi], IMAGE_DOS_SIGNATURE
    mov     word ptr IMAGE_DOS_HEADER.e_cblp[rdi], 90h
    mov     word ptr IMAGE_DOS_HEADER.e_cp[rdi], 3
    mov     word ptr IMAGE_DOS_HEADER.e_cparhdr[rdi], 4
    mov     word ptr IMAGE_DOS_HEADER.e_maxalloc[rdi], 0FFFFh
    mov     word ptr IMAGE_DOS_HEADER.e_sp[rdi], 0B8h
    mov     word ptr IMAGE_DOS_HEADER.e_lfarlc[rdi], 40h
    mov     dword ptr IMAGE_DOS_HEADER.e_lfanew[rdi], 80h
    
    mov     rcx, rsp
    mov     edx, sizeof IMAGE_DOS_HEADER
    call    WriteBuffer
    
    lea     rcx, szDosStub
    mov     edx, sizeof szDosStub
    call    WriteBuffer
    
    mov     eax, 80h
    sub     eax, currentOffset
    mov     ecx, eax
    call    ZeroBuffer
    
    add     rsp, sizeof IMAGE_DOS_HEADER + 40h
    pop     rbp
    ret
BuildDosHeader ENDP

BuildNTHeaders PROC
    push    rbp
    mov     rbp, rsp
    sub     rsp, 300h
    
    mov     r14, rcx                    ; code size
    mov     r15, rdx                    ; idata size
    
    lea     rdi, [rsp + 40h]
    mov     rcx, sizeof IMAGE_NT_HEADERS64
    xor     rax, rax
    rep     stosb
    
    lea     rbx, [rsp + 40h]
    mov     dword ptr IMAGE_NT_HEADERS64.Signature[rbx], IMAGE_NT_SIGNATURE
    mov     word ptr IMAGE_NT_HEADERS64.FileHeader.Machine[rbx], IMAGE_FILE_MACHINE_AMD64
    mov     word ptr IMAGE_NT_HEADERS64.FileHeader.NumberOfSections[rbx], 2
    mov     word ptr IMAGE_NT_HEADERS64.FileHeader.SizeOfOptionalHeader[rbx], sizeof IMAGE_OPTIONAL_HEADER64
    mov     word ptr IMAGE_NT_HEADERS64.FileHeader.Characteristics[rbx], \
        IMAGE_FILE_EXECUTABLE_IMAGE OR IMAGE_FILE_LARGE_ADDRESS_AWARE
    
    lea     rdi, [rbx + 4 + sizeof IMAGE_FILE_HEADER]
    mov     word ptr IMAGE_OPTIONAL_HEADER64.Magic[rdi], IMAGE_NT_OPTIONAL_HDR64_MAGIC
    mov     byte ptr IMAGE_OPTIONAL_HEADER64.MajorLinkerVersion[rdi], 14
    
    mov     rcx, r14
    mov     rdx, 200h
    call    AlignUp
    mov     dword ptr IMAGE_OPTIONAL_HEADER64.SizeOfCode[rdi], eax
    
    mov     rcx, r15
    mov     rdx, 200h
    call    AlignUp
    mov     dword ptr IMAGE_OPTIONAL_HEADER64.SizeOfInitializedData[rdi], eax
    
    mov     dword ptr IMAGE_OPTIONAL_HEADER64.AddressOfEntryPoint[rdi], 1000h
    mov     dword ptr IMAGE_OPTIONAL_HEADER64.BaseOfCode[rdi], 1000h
    mov     qword ptr IMAGE_OPTIONAL_HEADER64.ImageBase[rdi], 400000h
    mov     dword ptr IMAGE_OPTIONAL_HEADER64.SectionAlignment[rdi], 1000h
    mov     dword ptr IMAGE_OPTIONAL_HEADER64.FileAlignment[rdi], 200h
    mov     word ptr IMAGE_OPTIONAL_HEADER64.MajorOperatingSystemVersion[rdi], 6
    mov     word ptr IMAGE_OPTIONAL_HEADER64.MajorSubsystemVersion[rdi], 6
    
    mov     ecx, 80h + sizeof IMAGE_NT_HEADERS64 + sizeof IMAGE_SECTION_HEADER * 2
    mov     edx, ecx
    and     ecx, 1FFh
    test    ecx, ecx
    jz      @F
    add     edx, 200h
    and     edx, 0FFFFFE00h
@@:
    mov     dword ptr IMAGE_OPTIONAL_HEADER64.SizeOfHeaders[rdi], edx
    
    mov     eax, edx
    add     eax, 1000h                  ; .text
    add     eax, 1000h                  ; .idata
    mov     dword ptr IMAGE_OPTIONAL_HEADER64.SizeOfImage[rdi], eax
    
    mov     word ptr IMAGE_OPTIONAL_HEADER64.Subsystem[rdi], IMAGE_SUBSYSTEM_WINDOWS_CUI
    mov     word ptr IMAGE_OPTIONAL_HEADER64.DllCharacteristics[rdi], \
        IMAGE_DLLCHARACTERISTICS_DYNAMIC_BASE OR IMAGE_DLLCHARACTERISTICS_NX_COMPAT OR \
        IMAGE_DLLCHARACTERISTICS_TERMINAL_SERVER_AWARE
    
    mov     qword ptr IMAGE_OPTIONAL_HEADER64.SizeOfStackReserve[rdi], 100000h
    mov     qword ptr IMAGE_OPTIONAL_HEADER64.SizeOfStackCommit[rdi], 1000h
    mov     qword ptr IMAGE_OPTIONAL_HEADER64.SizeOfHeapReserve[rdi], 100000h
    mov     qword ptr IMAGE_OPTIONAL_HEADER64.SizeOfHeapCommit[rdi], 1000h
    mov     dword ptr IMAGE_OPTIONAL_HEADER64.NumberOfRvaAndSizes[rdi], 16
    
    lea     rsi, [rdi + IMAGE_OPTIONAL_HEADER64.DataDirectory]
    add     rsi, sizeof IMAGE_DATA_DIRECTORY
    mov     dword ptr IMAGE_DATA_DIRECTORY.VirtualAddress[rsi], 2000h
    mov     dword ptr IMAGE_DATA_DIRECTORY.Size_[rsi], 176
    
    lea     rcx, [rbx]
    mov     edx, sizeof IMAGE_NT_HEADERS64
    call    WriteBuffer
    
    add     rsp, 300h
    pop     rbp
    ret
BuildNTHeaders ENDP

BuildSectionHeaders PROC
    push    rbp
    mov     rbp, rsp
    sub     rsp, 100h
    
    mov     r14, rcx
    mov     r15, rdx
    
    ; .text
    lea     rdi, [rsp + 40h]
    mov     rcx, sizeof IMAGE_SECTION_HEADER
    xor     rax, rax
    rep     stosb
    
    lea     rbx, [rsp + 40h]
    lea     rsi, szTextSection
    lea     rdi, IMAGE_SECTION_HEADER.Name_[rbx]
    mov     rcx, 6
    rep     movsb
    
    mov     dword ptr IMAGE_SECTION_HEADER.VirtualSize[rbx], r14d
    mov     dword ptr IMAGE_SECTION_HEADER.VirtualAddress[rbx], 1000h
    
    mov     rcx, r14
    mov     rdx, 200h
    call    AlignUp
    mov     dword ptr IMAGE_SECTION_HEADER.SizeOfRawData[rbx], eax
    
    mov     ecx, 80h + sizeof IMAGE_NT_HEADERS64 + 2 * sizeof IMAGE_SECTION_HEADER
    mov     edx, ecx
    and     ecx, 1FFh
    test    ecx, ecx
    jz      @F
    add     edx, 200h
    and     edx, 0FFFFFE00h
@@:
    mov     dword ptr IMAGE_SECTION_HEADER.PointerToRawData[rbx], edx
    mov     dword ptr IMAGE_SECTION_HEADER.Characteristics[rbx], \
        IMAGE_SCN_CNT_CODE OR IMAGE_SCN_MEM_EXECUTE OR IMAGE_SCN_MEM_READ
    
    lea     rcx, [rbx]
    mov     edx, sizeof IMAGE_SECTION_HEADER
    call    WriteBuffer
    
    ; .idata
    lea     rdi, [rsp + 40h]
    mov     rcx, sizeof IMAGE_SECTION_HEADER
    xor     rax, rax
    rep     stosb
    
    lea     rbx, [rsp + 40h]
    lea     rsi, szIdataSection
    lea     rdi, IMAGE_SECTION_HEADER.Name_[rbx]
    mov     rcx, 7
    rep     movsb
    
    mov     dword ptr IMAGE_SECTION_HEADER.VirtualSize[rbx], r15d
    mov     dword ptr IMAGE_SECTION_HEADER.VirtualAddress[rbx], 2000h
    
    mov     rcx, r15
    mov     rdx, 200h
    call    AlignUp
    mov     dword ptr IMAGE_SECTION_HEADER.SizeOfRawData[rbx], eax
    
    mov     eax, dword ptr [rsp + 40h + IMAGE_SECTION_HEADER.PointerToRawData]
    add     eax, dword ptr [rsp + 40h + IMAGE_SECTION_HEADER.SizeOfRawData]
    mov     dword ptr IMAGE_SECTION_HEADER.PointerToRawData[rbx], eax
    
    mov     dword ptr IMAGE_SECTION_HEADER.Characteristics[rbx], \
        IMAGE_SCN_CNT_INITIALIZED_DATA OR IMAGE_SCN_MEM_READ OR IMAGE_SCN_MEM_WRITE
    
    lea     rcx, [rbx]
    mov     edx, sizeof IMAGE_SECTION_HEADER
    call    WriteBuffer
    
    add     rsp, 100h
    pop     rbp
    ret
BuildSectionHeaders ENDP

BuildCodeSection PROC
    push    rbp
    mov     rbp, rsp
    
    mov     ecx, 1FFh
    call    PadToAlignment
    
    mov     rcx, emitterBuffer
    mov     edx, emitterSize
    call    WriteBuffer
    
    mov     ecx, 1FFh
    call    PadToAlignment
    
    pop     rbp
    ret
BuildCodeSection ENDP

BuildImportSection PROC
    push    rbp
    mov     rbp, rsp
    sub     rsp, 200h
    
    ; 1. Descriptor User32
    lea     rdi, [rsp+40h]
    mov     rcx, sizeof IMAGE_IMPORT_DESCRIPTOR
    xor     rax, rax
    rep     stosb
    lea     rbx, [rsp+40h]
    mov     dword ptr [rbx+0], 203Ch    ; OriginalFirstThunk
    mov     dword ptr [rbx+12], 207Ch   ; Name
    mov     dword ptr [rbx+16], 205Ch   ; FirstThunk
    lea     rcx, [rbx]
    mov     edx, 20
    call    WriteBuffer
    
    ; 2. Descriptor Kernel32
    lea     rdi, [rsp+40h]
    mov     rcx, 20
    xor     rax, rax
    rep     stosb
    lea     rbx, [rsp+40h]
    mov     dword ptr [rbx+0], 204Ch    ; OriginalFirstThunk
    mov     dword ptr [rbx+12], 2087h   ; Name
    mov     dword ptr [rbx+16], 206Ch   ; FirstThunk
    lea     rcx, [rbx]
    mov     edx, 20
    call    WriteBuffer
    
    ; 3. Null Descriptor
    lea     rdi, [rsp+40h]
    mov     rcx, 20
    xor     rax, rax
    rep     stosb
    lea     rcx, [rsp+40h]
    mov     edx, 20
    call    WriteBuffer
    
    ; 4. ILT User32
    lea     rdi, [rsp+40h]
    mov     rcx, 16
    xor     rax, rax
    rep     stosb
    mov     qword ptr [rsp+40h], 2094h  ; Thunk -> MessageBoxA
    lea     rcx, [rsp+40h]
    mov     edx, 16
    call    WriteBuffer
    
    ; 5. ILT Kernel32
    lea     rdi, [rsp+40h]
    mov     rcx, 16
    xor     rax, rax
    rep     stosb
    mov     qword ptr [rsp+40h], 20A2h  ; Thunk -> ExitProcess
    lea     rcx, [rsp+40h]
    mov     edx, 16
    call    WriteBuffer
    
    ; 6. IAT User32
    lea     rdi, [rsp+40h]
    mov     rcx, 16
    xor     rax, rax
    rep     stosb
    mov     qword ptr [rsp+40h], 2094h  ; Thunk -> MessageBoxA
    lea     rcx, [rsp+40h]
    mov     edx, 16
    call    WriteBuffer
    
    ; 7. IAT Kernel32
    lea     rdi, [rsp+40h]
    mov     rcx, 16
    xor     rax, rax
    rep     stosb
    mov     qword ptr [rsp+40h], 20A2h  ; Thunk -> ExitProcess
    lea     rcx, [rsp+40h]
    mov     edx, 16
    call    WriteBuffer
    
    ; 8. DLL Name User32
    lea     rcx, szUser32
    mov     edx, 11
    call    WriteBuffer
    
    ; 9. DLL Name Kernel32
    lea     rcx, szKernel32
    mov     edx, 13
    call    WriteBuffer
    
    ; 10. Hint/Name MessageBoxA
    lea     rdi, [rsp+40h]
    mov     rcx, 14
    xor     rax, rax
    rep     stosb
    lea     rsi, szMessageBoxA
    lea     rdi, [rsp+42h]
    mov     rcx, 12
    rep     movsb
    lea     rcx, [rsp+40h]
    mov     edx, 14
    call    WriteBuffer
    
    ; 11. Hint/Name ExitProcess
    lea     rdi, [rsp+40h]
    mov     rcx, 14
    xor     rax, rax
    rep     stosb
    lea     rsi, szExitProcess
    lea     rdi, [rsp+42h]
    mov     rcx, 12
    rep     movsb
    lea     rcx, [rsp+40h]
    mov     edx, 14
    call    WriteBuffer
    
    ; Pad to 200h
    mov     ecx, 1FFh
    call    PadToAlignment
    
    add     rsp, 200h
    pop     rbp
    ret
BuildImportSection ENDP

WritePEToFile PROC
    push    rbp
    mov     rbp, rsp
    sub     rsp, 40h
    
    lea     rcx, szOutputFile
    mov     edx, GENERIC_WRITE
    xor     r8d, r8d
    xor     r9d, r9d
    mov     qword ptr [rsp+20h], CREATE_ALWAYS
    mov     qword ptr [rsp+28h], FILE_ATTRIBUTE_NORMAL
    xor     rax, rax
    mov     qword ptr [rsp+30h], rax
    call    CreateFileA
    
    cmp     rax, INVALID_HANDLE_VALUE
    je      @error
    
    mov     hFile, rax
    
    mov     rcx, rax
    mov     rdx, peBuffer
    mov     r8d, currentOffset
    lea     r9, bytesWritten
    mov     qword ptr [rsp+20h], 0
    call    WriteFile
    
    mov     rcx, hFile
    call    CloseHandle
    
    mov     eax, 1
    jmp     @done
    
@error:
    xor     eax, eax
@done:
    add     rsp, 40h
    pop     rbp
    ret
WritePEToFile ENDP

PrintString PROC
    push    rbp
    mov     rbp, rsp
    sub     rsp, 40h
    mov     rbx, rcx
    xor     rax, rax
@@: cmp     byte ptr [rcx], 0
    je      @F
    inc     rcx
    inc     rax
    jmp     @B
@@: mov     r14, rax
    mov     ecx, STD_OUTPUT_HANDLE
    call    GetStdHandle
    mov     rcx, rax
    mov     rdx, rbx
    mov     r8, r14
    lea     r9, [rsp+30h]
    mov     qword ptr [rsp+20h], 0
    call    WriteFile
    add     rsp, 40h
    pop     rbp
    ret
PrintString ENDP

main PROC
    push    rbp
    mov     rbp, rsp
    sub     rsp, 40h
    
    ; Allocate PE buffer
    call    GetProcessHeap
    mov     rcx, rax
    mov     edx, HEAP_ZERO_MEMORY
    mov     r8d, peBufferSize
    call    HeapAlloc
    test    rax, rax
    jz      @error
    mov     peBuffer, rax
    
    ; Allocate Emitter buffer
    call    GetProcessHeap
    mov     rcx, rax
    mov     edx, HEAP_ZERO_MEMORY
    mov     r8d, 1000h
    call    HeapAlloc
    test    rax, rax
    jz      @error
    mov     emitterBuffer, rax
    
    mov     currentOffset, 0
    mov     emitterSize, 0
    
    ; 1. Generate Machine Code
    call    GenerateMachineCode
    
    ; 2. Build PE
    call    BuildDosHeader
    
    mov     rcx, qword ptr emitterSize
    mov     edx, 176                    ; Exact size of idata
    call    BuildNTHeaders
    
    mov     rcx, qword ptr emitterSize
    mov     edx, 176
    call    BuildSectionHeaders
    
    call    BuildCodeSection
    call    BuildImportSection
    
    ; 3. Write to disk
    call    WritePEToFile
    test    eax, eax
    jz      @error
    
    lea     rcx, szSuccess
    call    PrintString
    xor     ecx, ecx
    jmp     @exit
    
@error:
    lea     rcx, szError
    call    PrintString
    mov     ecx, 1
    
@exit:
    call    ExitProcess
main ENDP

END
