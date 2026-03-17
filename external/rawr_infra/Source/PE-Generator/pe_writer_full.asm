; ═══════════════════════════════════════════════════════════════════
; PRODUCTION PE32+ WRITER & MACHINE CODE EMITTER - x64 MASM
; Full-featured executable generator for production builds
; Generates runnable x64 executables with complete import tables
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

; PE signature values
IMAGE_DOS_SIGNATURE     equ 5A4Dh
IMAGE_NT_SIGNATURE      equ 00004550h
IMAGE_FILE_MACHINE_AMD64 equ 8664h
IMAGE_NT_OPTIONAL_HDR64_MAGIC equ 020Bh

; Section characteristics
IMAGE_SCN_CNT_CODE              equ 00000020h
IMAGE_SCN_CNT_INITIALIZED_DATA  equ 00000040h
IMAGE_SCN_MEM_EXECUTE           equ 20000000h
IMAGE_SCN_MEM_READ              equ 40000000h
IMAGE_SCN_MEM_WRITE             equ 80000000h

; File characteristics
IMAGE_FILE_EXECUTABLE_IMAGE     equ 0002h
IMAGE_FILE_LARGE_ADDRESS_AWARE  equ 0020h

; Subsystem
IMAGE_SUBSYSTEM_WINDOWS_CUI     equ 3

; DLL characteristics
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

IMAGE_THUNK_DATA64 STRUCT
    u1 dq ?
IMAGE_THUNK_DATA64 ENDS

IMAGE_IMPORT_BY_NAME STRUCT
    Hint    dw ?
    Name_   db 256 dup(?)
IMAGE_IMPORT_BY_NAME ENDS

; ── Data Section ───────────────────────────────────────────────────
.data
    ; Output file configuration
    szOutputFile    db "generated.exe", 0
    szKernel32      db "kernel32.dll", 0
    szExitProcess   db "ExitProcess", 0
    szTextSection   db ".text", 0, 0, 0
    szIdataSection  db ".idata", 0, 0
    
    ; DOS stub message
    szDosStub       db "This program cannot be run in DOS mode.", 0Dh, 0Dh, 0Ah, 24h
    
    ; Messages
    szSuccess       db "PE file written successfully: generated.exe", 0Dh, 0Ah, 0
    szError         db "Error: Failed to write PE file", 0Dh, 0Ah, 0
    
    ; PE buffer info
    peBuffer        dq 0
    peBufferSize    dd 100000h          ; 1MB
    currentOffset   dd 0

    
    ; File handle
    hFile           dq 0
    bytesWritten    dd 0
    
    ; Machine code for complete main function that calls ExitProcess
    ; sub rsp, 28h         ; Shadow space + alignment
    ; xor ecx, ecx         ; Exit code 0
    ; call [rel IAT]       ; Call ExitProcess via IAT
    ; add rsp, 28h         ; Cleanup (unreachable but for correctness)
    ; ret
    simpleMainCode  db 48h, 83h, 0ECh, 28h              ; sub rsp, 28h
                    db 33h, 0C9h                        ; xor ecx, ecx
                    db 0FFh, 15h, 0Eh, 10h, 00h, 00h    ; call qword ptr [rip+0x100E] -> IAT at RVA 2000h+offset
                    db 48h, 83h, 0C4h, 28h              ; add rsp, 28h
                    db 0C3h                             ; ret
    simpleMainSize  equ $ - simpleMainCode

.code

; ═══════════════════════════════════════════════════════════════════
; AlignUp - Align value to specified boundary
; Input: RCX = value, RDX = alignment
; Output: RAX = aligned value
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

; ═══════════════════════════════════════════════════════════════════
; WriteBuffer - Write bytes to PE buffer at current offset
; Input: RCX = source data, RDX = size
; ═══════════════════════════════════════════════════════════════════
WriteBuffer PROC
    push    rbp
    mov     eax, currentOffset          ; load 32-bit offset (zero-extends into RAX)
    add     rdi, rax                    ; RDI = peBuffer + currentOffset
    push    rsi
    push    rdi
    push    rdx
    
    mov     rsi, rcx                    ; source
    mov     rcx, rdx                    ; count
    mov     rdi, peBuffer
    add     rdi, qword ptr currentOffset
    
    rep     movsb
    
    pop     rdx
    add     currentOffset, edx
    
    pop     rdi
    pop     rsi
    pop     rbp
    ret
WriteBuffer ENDP

; ═══════════════════════════════════════════════════════════════════
; ZeroBuffer - Zero bytes in PE buffer
; Input: RCX = count
; ═══════════════════════════════════════════════════════════════════
ZeroBuffer PROC
    push    rbp
    mov     rbp, rsp
    push    rdi
    push    rcx
    
    mov     rdi, peBuffer
    add     rdi, qword ptr currentOffset
    xor     rax, rax
    rep     stosb
    
    pop     rcx
    add     currentOffset, ecx
    
    pop     rdi
    pop     rbp
    ret
ZeroBuffer ENDP

; ═══════════════════════════════════════════════════════════════════
; BuildDosHeader - Create DOS header and stub
; ═══════════════════════════════════════════════════════════════════
BuildDosHeader PROC
    push    rbp
    mov     rbp, rsp
    sub     rsp, 40h
    
    ; Allocate space for DOS header on stack
    sub     rsp, sizeof IMAGE_DOS_HEADER
    mov     rdi, rsp
    
    ; Zero initialize
    xor     rax, rax
    mov     rcx, sizeof IMAGE_DOS_HEADER
    rep     stosb
    
    ; Set DOS header fields
    mov     rdi, rsp
    mov     word ptr IMAGE_DOS_HEADER.e_magic[rdi], IMAGE_DOS_SIGNATURE
    mov     word ptr IMAGE_DOS_HEADER.e_cblp[rdi], 90h
    mov     word ptr IMAGE_DOS_HEADER.e_cp[rdi], 3
    mov     word ptr IMAGE_DOS_HEADER.e_cparhdr[rdi], 4
    mov     word ptr IMAGE_DOS_HEADER.e_maxalloc[rdi], 0FFFFh
    mov     word ptr IMAGE_DOS_HEADER.e_sp[rdi], 0B8h
    mov     word ptr IMAGE_DOS_HEADER.e_lfarlc[rdi], 40h
    mov     dword ptr IMAGE_DOS_HEADER.e_lfanew[rdi], 80h  ; NT headers at offset 128
    
    ; Write DOS header
    mov     rcx, rsp
    mov     edx, sizeof IMAGE_DOS_HEADER
    call    WriteBuffer
    
    ; Write DOS stub
    lea     rcx, szDosStub
    mov     edx, sizeof szDosStub
    call    WriteBuffer
    
    ; Pad to e_lfanew (80h = 128 bytes)
    mov     eax, 80h
    sub     eax, currentOffset
    mov     ecx, eax
    call    ZeroBuffer
    
    add     rsp, sizeof IMAGE_DOS_HEADER + 40h
    pop     rbp
    ret
BuildDosHeader ENDP

; ═══════════════════════════════════════════════════════════════════
; BuildNTHeaders - Create NT headers (PE signature + COFF + Optional)
; Input: RCX = code size, RDX = idata size
; ═══════════════════════════════════════════════════════════════════
BuildNTHeaders PROC
    push    rbp
    mov     rbp, rsp
    sub     rsp, 300h                   ; Space for NT headers + locals
    
    mov     r14, rcx                    ; Save code size
    mov     r15, rdx                    ; Save idata size
    
    ; Allocate NT headers on stack
    lea     rdi, [rsp + 40h]
    mov     rcx, sizeof IMAGE_NT_HEADERS64
    xor     rax, rax
    rep     stosb
    
    lea     rbx, [rsp + 40h]           ; RBX = NT headers
    
    ; PE Signature
    mov     dword ptr IMAGE_NT_HEADERS64.Signature[rbx], IMAGE_NT_SIGNATURE
    
    ; File Header
    mov     word ptr IMAGE_NT_HEADERS64.FileHeader.Machine[rbx], IMAGE_FILE_MACHINE_AMD64
    mov     word ptr IMAGE_NT_HEADERS64.FileHeader.NumberOfSections[rbx], 2
    mov     dword ptr IMAGE_NT_HEADERS64.FileHeader.TimeDateStamp[rbx], 0
    mov     word ptr IMAGE_NT_HEADERS64.FileHeader.SizeOfOptionalHeader[rbx], sizeof IMAGE_OPTIONAL_HEADER64
    mov     word ptr IMAGE_NT_HEADERS64.FileHeader.Characteristics[rbx], \
        IMAGE_FILE_EXECUTABLE_IMAGE or IMAGE_FILE_LARGE_ADDRESS_AWARE
    
    ; Optional Header
    lea     rdi, [rbx + sizeof IMAGE_NT_HEADERS64.Signature + sizeof IMAGE_FILE_HEADER]
    mov     word ptr IMAGE_OPTIONAL_HEADER64.Magic[rdi], IMAGE_NT_OPTIONAL_HDR64_MAGIC
    mov     byte ptr IMAGE_OPTIONAL_HEADER64.MajorLinkerVersion[rdi], 14
    mov     byte ptr IMAGE_OPTIONAL_HEADER64.MinorLinkerVersion[rdi], 0
    
    ; Align code size to file alignment (200h)
    mov     rcx, r14
    mov     rdx, 200h
    call    AlignUp
    mov     dword ptr IMAGE_OPTIONAL_HEADER64.SizeOfCode[rdi], eax
    
    ; Align idata size
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
    mov     word ptr IMAGE_OPTIONAL_HEADER64.MinorOperatingSystemVersion[rdi], 0
    mov     word ptr IMAGE_OPTIONAL_HEADER64.MajorSubsystemVersion[rdi], 6
    mov     word ptr IMAGE_OPTIONAL_HEADER64.MinorSubsystemVersion[rdi], 0
    
    ; Calculate SizeOfHeaders (aligned to file alignment)
    mov     ecx, 80h                    ; DOS header + stub
    add     ecx, sizeof IMAGE_NT_HEADERS64
    add     ecx, sizeof IMAGE_SECTION_HEADER * 2
    mov     edx, ecx
    and     ecx, 1FFh
    test    ecx, ecx
    jz      @F
    add     edx, 200h
    and     edx, 0FFFFFE00h
@@:
    mov     dword ptr IMAGE_OPTIONAL_HEADER64.SizeOfHeaders[rdi], edx
    
    ; Calculate SizeOfImage (aligned to section alignment)
    mov     eax, edx                    ; Headers
    add     eax, 1000h                  ; .text section
    add     eax, 1000h                  ; .idata section
    mov     dword ptr IMAGE_OPTIONAL_HEADER64.SizeOfImage[rdi], eax
    
    mov     word ptr IMAGE_OPTIONAL_HEADER64.Subsystem[rdi], IMAGE_SUBSYSTEM_WINDOWS_CUI
    mov     word ptr IMAGE_OPTIONAL_HEADER64.DllCharacteristics[rdi], \
        IMAGE_DLLCHARACTERISTICS_DYNAMIC_BASE or IMAGE_DLLCHARACTERISTICS_NX_COMPAT or \
        IMAGE_DLLCHARACTERISTICS_TERMINAL_SERVER_AWARE
    
    mov     qword ptr IMAGE_OPTIONAL_HEADER64.SizeOfStackReserve[rdi], 100000h
    mov     qword ptr IMAGE_OPTIONAL_HEADER64.SizeOfStackCommit[rdi], 1000h
    mov     qword ptr IMAGE_OPTIONAL_HEADER64.SizeOfHeapReserve[rdi], 100000h
    mov     qword ptr IMAGE_OPTIONAL_HEADER64.SizeOfHeapCommit[rdi], 1000h
    mov     dword ptr IMAGE_OPTIONAL_HEADER64.NumberOfRvaAndSizes[rdi], 16
    
    ; Import directory (DataDirectory[1])
    lea     rsi, [rdi + IMAGE_OPTIONAL_HEADER64.DataDirectory]
    add     rsi, sizeof IMAGE_DATA_DIRECTORY  ; Skip [0], go to [1]
    mov     dword ptr IMAGE_DATA_DIRECTORY.VirtualAddress[rsi], 2000h
    mov     dword ptr IMAGE_DATA_DIRECTORY.Size_[rsi], 100h
    
    ; Write NT headers
    lea     rcx, [rbx]
    mov     edx, sizeof IMAGE_NT_HEADERS64
    call    WriteBuffer
    
    add     rsp, 300h
    pop     rbp
    ret
BuildNTHeaders ENDP

; ═══════════════════════════════════════════════════════════════════
; BuildSectionHeaders - Create .text and .idata section headers
; Input: RCX = code size, RDX = idata size
; ═══════════════════════════════════════════════════════════════════
BuildSectionHeaders PROC
    push    rbp
    mov     rbp, rsp
    sub     rsp, 200h
    
    mov     r14, rcx                    ; code size
    mov     r15, rdx                    ; idata size
    
    ; ── .text section ──
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
    
    ; Align raw size to file alignment
    mov     rcx, r14
    mov     rdx, 200h
    call    AlignUp
    mov     dword ptr IMAGE_SECTION_HEADER.SizeOfRawData[rbx], eax
    
    ; Calculate PointerToRawData (after headers)
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
        IMAGE_SCN_CNT_CODE or IMAGE_SCN_MEM_EXECUTE or IMAGE_SCN_MEM_READ
    
    ; Save .text section data for later use
    lea     rdi, [rsp + 100h]
    lea     rsi, [rbx]
    mov     rcx, sizeof IMAGE_SECTION_HEADER
    rep     movsb
    
    lea     rcx, [rbx]
    mov     edx, sizeof IMAGE_SECTION_HEADER
    call    WriteBuffer
    
    ; ── .idata section ──
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
    
    ; Align raw size
    mov     rcx, r15
    mov     rdx, 200h
    call    AlignUp
    mov     dword ptr IMAGE_SECTION_HEADER.SizeOfRawData[rbx], eax
    
    ; PointerToRawData = previous section's PointerToRawData + SizeOfRawData
    mov     eax, dword ptr [rsp + 100h + IMAGE_SECTION_HEADER.PointerToRawData]
    add     eax, dword ptr [rsp + 100h + IMAGE_SECTION_HEADER.SizeOfRawData]
    mov     dword ptr IMAGE_SECTION_HEADER.PointerToRawData[rbx], eax
    
    mov     dword ptr IMAGE_SECTION_HEADER.Characteristics[rbx], \
        IMAGE_SCN_CNT_INITIALIZED_DATA or IMAGE_SCN_MEM_READ or IMAGE_SCN_MEM_WRITE
    
    lea     rcx, [rbx]
    mov     edx, sizeof IMAGE_SECTION_HEADER
    call    WriteBuffer
    
    add     rsp, 200h
    pop     rbp
    ret
BuildSectionHeaders ENDP
    ; currentOffset in EAX, preserve in EDX
    mov     eax, currentOffset
    mov     edx, eax
    
    ; Compute mask = alignment - 1 in R8D, leaving ECX (alignment) intact
    mov     r8d, ecx            ; r8d = alignment
    dec     r8d                 ; r8d = alignment - 1 (mask)
    
    ; If (currentOffset & mask) == 0, already aligned
    test    eax, r8d
    jz      @done
    
    ; Calculate padding needed:
    ; low     = currentOffset & mask
    ; padding = alignment - low
    mov     eax, edx            ; eax = currentOffset
    and     eax, r8d            ; eax = low = currentOffset & mask
    mov     edx, ecx            ; edx = alignment
    sub     edx, eax            ; edx = padding
    
    mov     ecx, edx            ; ZeroBuffer(size = padding)
    and     eax, ecx
    test    eax, eax
    jz      @done
    
    ; Calculate padding needed
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
; BuildCodeSection - Write .text section data
; ═══════════════════════════════════════════════════════════════════
BuildCodeSection PROC
    push    rbp
    mov     rbp, rsp
    
    ; Pad to section start (200h aligned)
    mov     ecx, 1FFh
    call    PadToAlignment
    
    ; Write simple main function code
    lea     rcx, simpleMainCode
    mov     edx, simpleMainSize
    call    WriteBuffer
    
    ; Pad to 200h boundary
    mov     ecx, 1FFh
    call    PadToAlignment
    
    pop     rbp
    ret
BuildCodeSection ENDP

; ═══════════════════════════════════════════════════════════════════

; ═══════════════════════════════════════════════════════════════════
BuildImportSection PROC
    push    rbp
    mov     rbp, rsp
    sub     rsp, 200h
    
    ; Calculate offsets for import structures
    mov     r12d, currentOffset         ; Base of .idata section
    mov     r13d, 2000h                 ; RVA of .idata
    
    ; ── Import Descriptor for kernel32.dll ──
    lea     rdi, [rsp + 40h]
    mov     rcx, sizeof IMAGE_IMPORT_DESCRIPTOR
    xor     rax, rax
    rep     stosb
    
    lea     rbx, [rsp + 40h]
    
    ; Calculate RVAs
    mov     eax, r13d
    add     eax, 2 * sizeof IMAGE_IMPORT_DESCRIPTOR  ; After descriptors
    mov     dword ptr IMAGE_IMPORT_DESCRIPTOR.OriginalFirstThunk[rbx], eax  ; ILT RVA
    
    add     eax, 2 * sizeof IMAGE_THUNK_DATA64       ; After ILT
    mov     dword ptr IMAGE_IMPORT_DESCRIPTOR.Name_[rbx], eax                ; DLL name RVA
    
    mov     eax, r13d
    add     eax, 2 * sizeof IMAGE_IMPORT_DESCRIPTOR
    add     eax, 2 * sizeof IMAGE_THUNK_DATA64
    add     eax, 20                                   ; After DLL name (rounded)
    mov     dword ptr IMAGE_IMPORT_DESCRIPTOR.FirstThunk[rbx], eax           ; IAT RVA
    
    ; Write import descriptor
    lea     rcx, [rbx]
    mov     edx, sizeof IMAGE_IMPORT_DESCRIPTOR
    call    WriteBuffer
    
    ; ── Null Import Descriptor (terminator) ──
    lea     rdi, [rsp + 40h]
    mov     rcx, sizeof IMAGE_IMPORT_DESCRIPTOR
    xor     rax, rax
    rep     stosb
    
    lea     rcx, [rsp + 40h]
    mov     edx, sizeof IMAGE_IMPORT_DESCRIPTOR
    call    WriteBuffer
    
    ; ── Import Lookup Table (ILT) ──
    lea     rdi, [rsp + 40h]
    mov     rcx, 2 * sizeof IMAGE_THUNK_DATA64
    xor     rax, rax
    rep     stosb
    
    ; Thunk for ExitProcess
    mov     eax, r13d
    add     eax, 2 * sizeof IMAGE_IMPORT_DESCRIPTOR
    add     eax, 4 * sizeof IMAGE_THUNK_DATA64
    add     eax, 20                                   ; RVA to hint/name
    mov     qword ptr [rsp + 40h], rax
    
    ; Write ILT
    lea     rcx, [rsp + 40h]
    mov     edx, 2 * sizeof IMAGE_THUNK_DATA64
    call    WriteBuffer
    
    ; ── DLL Name (kernel32.dll) ──
    lea     rcx, szKernel32
    mov     edx, 13
    call    WriteBuffer
    
    ; Align to 8 bytes
    mov     ecx, 7
    call    ZeroBuffer
    
    ; ── Import Address Table (IAT) ──
    lea     rdi, [rsp + 40h]
    mov     rcx, 2 * sizeof IMAGE_THUNK_DATA64
    xor     rax, rax
    rep     stosb
    
    ; Same thunk as ILT
    mov     eax, r13d
    add     eax, 2 * sizeof IMAGE_IMPORT_DESCRIPTOR
    add     eax, 4 * sizeof IMAGE_THUNK_DATA64
    add     eax, 20
    mov     qword ptr [rsp + 40h], rax
    
    lea     rcx, [rsp + 40h]
    mov     edx, 2 * sizeof IMAGE_THUNK_DATA64
    call    WriteBuffer
    
    ; ── Hint/Name Table ──
    ; Hint (word) + Name (null-terminated)
    lea     rdi, [rsp + 40h]
    mov     rcx, 20
    xor     rax, rax
    rep     stosb
    
    mov     word ptr [rsp + 40h], 0     ; Hint = 0
    lea     rsi, szExitProcess
    lea     rdi, [rsp + 40h + 2]
    mov     rcx, 12
    rep     movsb
    
    lea     rcx, [rsp + 40h]
    mov     edx, 20
    call    WriteBuffer
    
    ; Pad to 200h boundary
    mov     ecx, 1FFh
    call    PadToAlignment
    
    add     rsp, 200h
    pop     rbp
    ret
BuildImportSection ENDP

; ═══════════════════════════════════════════════════════════════════
; WritePEToFile - Write PE buffer to disk
; ═══════════════════════════════════════════════════════════════════
WritePEToFile PROC
    push    rbp
    mov     rbp, rsp
    sub     rsp, 40h
    
    ; Create file
    lea     rcx, szOutputFile
    mov     edx, GENERIC_WRITE
    xor     r8d, r8d                    ; No sharing
    xor     r9d, r9d                    ; No security
    mov     qword ptr [rsp+20h], CREATE_ALWAYS
    mov     qword ptr [rsp+28h], FILE_ATTRIBUTE_NORMAL
    xor     rax, rax
    mov     qword ptr [rsp+30h], rax    ; No template
    call    CreateFileA
    
    cmp     rax, INVALID_HANDLE_VALUE
    je      @error
    
    mov     hFile, rax
    
    ; Write buffer to file
    mov     rcx, rax                    ; hFile
    mov     rdx, peBuffer               ; lpBuffer
    mov     r8d, currentOffset          ; nNumberOfBytesToWrite
    lea     r9, bytesWritten            ; lpNumberOfBytesWritten
    mov     qword ptr [rsp+20h], 0      ; lpOverlapped
    call    WriteFile
    
    test    eax, eax
    jz      @error
    
    ; Close file
    mov     rcx, hFile
    call    CloseHandle
    
    mov     eax, 1                      ; Success
    jmp     @done
    
@error:
    xor     eax, eax
    
@done:
    add     rsp, 40h
    pop     rbp
    ret
WritePEToFile ENDP

; ═══════════════════════════════════════════════════════════════════
; PrintString - Output string to console
; Input: RCX = string pointer
; ═══════════════════════════════════════════════════════════════════
PrintString PROC
    push    rbp
    mov     rbp, rsp
    sub     rsp, 40h
    
    mov     rbx, rcx
    
    ; Get string length
    xor     rax, rax
@@: cmp     byte ptr [rcx], 0
    je      @F
    inc     rcx
    inc     rax
    jmp     @B
@@: mov     r14, rax
    
    ; Get stdout handle
    mov     ecx, STD_OUTPUT_HANDLE
    call    GetStdHandle
    
    ; Write to console
    mov     rcx, rax                    ; hConsoleOutput
    mov     rdx, rbx                    ; lpBuffer
    mov     r8, r14                     ; nNumberOfCharsToWrite
    lea     r9, [rsp+30h]              ; lpNumberOfCharsWritten
    mov     qword ptr [rsp+20h], 0      ; lpReserved
    call    WriteFile
    
    add     rsp, 40h
    pop     rbp
    ret
PrintString ENDP

; ═══════════════════════════════════════════════════════════════════
; main - Entry point
; ═══════════════════════════════════════════════════════════════════
main PROC
    push    rbp
    mov     rbp, rsp
    sub     rsp, 40h
    
    ; Allocate PE buffer
    call    GetProcessHeap
    mov     rcx, rax                    ; hHeap
    mov     edx, HEAP_ZERO_MEMORY       ; dwFlags
    mov     r8d, peBufferSize           ; dwBytes
    call    HeapAlloc
    
    test    rax, rax
    jz      @error
    mov     peBuffer, rax
    
    ; Reset offset
    mov     currentOffset, 0
    
    ; Build PE components
    call    BuildDosHeader
    
    mov     ecx, simpleMainSize         ; code size
    mov     edx, 100h                   ; idata size (estimate)
    call    BuildNTHeaders
    
    mov     ecx, simpleMainSize
    mov     edx, 100h
    call    BuildSectionHeaders
    
    call    BuildCodeSection
    call    BuildImportSection
    
    ; Write to file
    call    WritePEToFile
    test    eax, eax
    jz      @error
    
    ; Success message
    lea     rcx, szSuccess
    call    PrintString
    
    xor     ecx, ecx                    ; Exit code 0
    jmp     @exit
    
@error:
    lea     rcx, szError
    call    PrintString
    mov     ecx, 1                      ; Exit code 1
    
@exit:
    ; Free PE buffer
    call    GetProcessHeap
    mov     rcx, rax
    xor     edx, edx
    mov     r8, peBuffer
    call    HeapFree
    
    call    ExitProcess
main ENDP

END
