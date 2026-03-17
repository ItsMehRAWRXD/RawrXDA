; PE_Backend_Emitter.asm
; Fully monolithic PE32+ (x64) backend emitter + writer.
; No MASM include files. Only links against kernel32.lib/user32.lib.
;
; Builds a minimal EXE with:
; - DOS header + DOS stub
; - NT headers (PE32+)
; - 2 sections: .text, .idata
; - Import table for kernel32!ExitProcess
; - .text entrypoint: xor ecx,ecx; call [IAT]; ret
;
; Build example:
;   ml64.exe PE_Backend_Emitter.asm /link /subsystem:windows /entry:main kernel32.lib user32.lib

option casemap:none

includelib kernel32.lib
includelib user32.lib

EXTERN  CreateFileA:PROC
EXTERN  WriteFile:PROC
EXTERN  CloseHandle:PROC
EXTERN  ExitProcess:PROC
EXTERN  MessageBoxA:PROC
EXTERN  GetCommandLineA:PROC
EXTERN  GetLastError:PROC

; =============================================================================
; Structs (manual, no windows.inc)
; =============================================================================

IMAGE_DOS_HEADER STRUCT
    e_magic         WORD    ?
    e_cblp          WORD    ?
    e_cp            WORD    ?
    e_crlc          WORD    ?
    e_cparhdr       WORD    ?
    e_minalloc      WORD    ?
    e_maxalloc      WORD    ?
    e_ss            WORD    ?
    e_sp            WORD    ?
    e_csum          WORD    ?
    e_ip            WORD    ?
    e_cs            WORD    ?
    e_lfarlc        WORD    ?
    e_ovno          WORD    ?
    e_res           WORD    4 DUP(?)
    e_oemid         WORD    ?
    e_oeminfo       WORD    ?
    e_res2          WORD    10 DUP(?)
    e_lfanew        DWORD   ?
IMAGE_DOS_HEADER ENDS

IMAGE_FILE_HEADER STRUCT
    Machine                 WORD    ?
    NumberOfSections        WORD    ?
    TimeDateStamp           DWORD   ?
    PointerToSymbolTable    DWORD   ?
    NumberOfSymbols         DWORD   ?
    SizeOfOptionalHeader    WORD    ?
    Characteristics         WORD    ?
IMAGE_FILE_HEADER ENDS

IMAGE_DATA_DIRECTORY STRUCT
    VirtualAddress  DWORD   ?
    _Size           DWORD   ?
IMAGE_DATA_DIRECTORY ENDS

IMAGE_OPTIONAL_HEADER64 STRUCT
    Magic                       WORD    ?
    MajorLinkerVersion          BYTE    ?
    MinorLinkerVersion          BYTE    ?
    SizeOfCode                  DWORD   ?
    SizeOfInitializedData       DWORD   ?
    SizeOfUninitializedData     DWORD   ?
    AddressOfEntryPoint         DWORD   ?
    BaseOfCode                  DWORD   ?
    ImageBase                   QWORD   ?
    SectionAlignment            DWORD   ?
    FileAlignment               DWORD   ?
    MajorOperatingSystemVersion WORD    ?
    MinorOperatingSystemVersion WORD    ?
    MajorImageVersion           WORD    ?
    MinorImageVersion           WORD    ?
    MajorSubsystemVersion       WORD    ?
    MinorSubsystemVersion       WORD    ?
    Win32VersionValue           DWORD   ?
    SizeOfImage                 DWORD   ?
    SizeOfHeaders               DWORD   ?
    CheckSum                    DWORD   ?
    Subsystem                   WORD    ?
    DllCharacteristics          WORD    ?
    SizeOfStackReserve          QWORD   ?
    SizeOfStackCommit           QWORD   ?
    SizeOfHeapReserve           QWORD   ?
    SizeOfHeapCommit            QWORD   ?
    LoaderFlags                 DWORD   ?
    NumberOfRvaAndSizes         DWORD   ?
    DataDirectory               IMAGE_DATA_DIRECTORY 16 DUP(<>)
IMAGE_OPTIONAL_HEADER64 ENDS

IMAGE_NT_HEADERS64 STRUCT
    Signature       DWORD   ?
    FileHeader      IMAGE_FILE_HEADER <>
    OptionalHeader  IMAGE_OPTIONAL_HEADER64 <>
IMAGE_NT_HEADERS64 ENDS

IMAGE_SECTION_HEADER STRUCT
    Name1               BYTE    8 DUP(?)
    VirtualSize         DWORD   ?
    VirtualAddress      DWORD   ?
    SizeOfRawData       DWORD   ?
    PointerToRawData    DWORD   ?
    PointerToRelocations DWORD  ?
    PointerToLinenumbers DWORD  ?
    NumberOfRelocations WORD    ?
    NumberOfLinenumbers WORD    ?
    Characteristics     DWORD   ?
IMAGE_SECTION_HEADER ENDS

IMAGE_IMPORT_DESCRIPTOR STRUCT
    OriginalFirstThunk  DWORD   ?
    TimeDateStamp       DWORD   ?
    ForwarderChain      DWORD   ?
    NameRVA             DWORD   ?
    FirstThunk          DWORD   ?
IMAGE_IMPORT_DESCRIPTOR ENDS

; =============================================================================
; Constants
; =============================================================================

DOS_MAGIC                   EQU 05A4Dh              ; "MZ"
PE_MAGIC                    EQU 00004550h           ; "PE\0\0"
OPTIONAL_HDR64_MAGIC        EQU 020Bh
IMAGE_FILE_MACHINE_AMD64    EQU 08664h

IMAGE_FILE_EXECUTABLE_IMAGE EQU 0002h
IMAGE_FILE_LARGE_ADDRESS_AWARE EQU 0020h

IMAGE_SCN_CNT_CODE          EQU 00000020h
IMAGE_SCN_CNT_INITIALIZED_DATA EQU 00000040h
IMAGE_SCN_MEM_EXECUTE       EQU 20000000h
IMAGE_SCN_MEM_READ          EQU 40000000h
IMAGE_SCN_MEM_WRITE         EQU 80000000h

SUBSYSTEM_WINDOWS_GUI       EQU 2

DLLCHAR_NX_COMPAT           EQU 0100h
DLLCHAR_TERMINAL_SERVER_AWARE EQU 8000h

GENERIC_WRITE               EQU 40000000h
CREATE_ALWAYS               EQU 2
FILE_ATTRIBUTE_NORMAL       EQU 80h
INVALID_HANDLE_VALUE        EQU -1

MB_YESNO                    EQU 00000004h
MB_ICONQUESTION             EQU 00000020h
IDYES                       EQU 6

FILE_ALIGNMENT              EQU 200h
SECTION_ALIGNMENT           EQU 1000h
IMAGE_BASE                  EQU 140000000h

; Layout (fixed, minimal)
DOS_OFF                     EQU 0000h
DOS_STUB_OFF                EQU 0040h
NT_OFF                      EQU 0080h
; On-disk section headers start at:
;   NT_OFF + 4 (Signature) + 20 (FileHeader) + 0F0 (OptionalHeader64)
; We hardcode this to avoid any assembler struct packing/tail-padding surprises.
SECT_OFF                    EQU 0188h
HEADERS_SIZE                EQU 0200h

TEXT_RVA                    EQU 1000h
IDATA_RVA                   EQU 2000h
TEXT_RAW                    EQU 0200h
IDATA_RAW                   EQU 0400h
TEXT_RAW_SIZE               EQU 0200h
IDATA_RAW_SIZE              EQU 0200h

; .idata internal offsets (section-relative)
ID_OFF_IMPORT_DESC          EQU 0000h
ID_OFF_ILT                  EQU 0028h
ID_OFF_IAT                  EQU 0038h
ID_OFF_IMPORT_BY_NAME       EQU 0048h
ID_OFF_DLL_NAME             EQU 0060h               ; "offset 0x60" (section-relative)

IMPORT_DIR_RVA              EQU (IDATA_RVA + ID_OFF_IMPORT_DESC)
ILT_RVA                     EQU (IDATA_RVA + ID_OFF_ILT)
IAT_RVA                     EQU (IDATA_RVA + ID_OFF_IAT)
IMPORT_BY_NAME_RVA          EQU (IDATA_RVA + ID_OFF_IMPORT_BY_NAME)
DLL_NAME_RVA                EQU (IDATA_RVA + ID_OFF_DLL_NAME)

; .text codegen constants
CODE_CALL_NEXT_RVA          EQU (TEXT_RVA + 0Ch)     ; rip after FF 15 disp32
CALL_DISP32                 EQU (IAT_RVA - CODE_CALL_NEXT_RVA)

; =============================================================================
; Data
; =============================================================================

.data
szOutFile       db "output.exe",0
szTitle         db "PE Backend Emitter",0
szPrompt        db "Build minimal PE32+ output.exe?",0
szOk            db "Wrote output.exe",0
szFail          db "Failed writing output.exe",0

; DOS stub (64 bytes at 0x40); standard minimal "This program cannot be run..."
dos_stub_bytes  db  0Eh,1Fh,0BAh,0Eh,00h,0B4h,09h,0CDh,21h,0B8h,01h,4Ch,0CDh,21h
                db  "This program cannot be run in DOS mode.",0Dh,0Dh,0Ah,'$'
dos_stub_end    label byte

align 16
peBuffer        db  1000h dup(0)     ; 0x600 used, 0x1000 allocated

.code

; =============================================================================
; x64 prologue/epilogue emitters (machine-code backend building blocks)
; =============================================================================

Emit_FunctionPrologue64 PROC
    ; RCX = p, EDX = stackAlloc (imm8 only; we use 0x28)
    ; emits: sub rsp, imm8  => 48 83 EC xx
    mov     BYTE PTR [rcx+0], 048h
    mov     BYTE PTR [rcx+1], 083h
    mov     BYTE PTR [rcx+2], 0ECh
    mov     BYTE PTR [rcx+3], dl
    lea     rax, [rcx+4]
    ret
Emit_FunctionPrologue64 ENDP

Emit_FunctionEpilogue64 PROC
    ; RCX = p, EDX = stackAlloc (imm8 only; we use 0x28)
    ; emits: add rsp, imm8; ret  => 48 83 C4 xx C3
    mov     BYTE PTR [rcx+0], 048h
    mov     BYTE PTR [rcx+1], 083h
    mov     BYTE PTR [rcx+2], 0C4h
    mov     BYTE PTR [rcx+3], dl
    mov     BYTE PTR [rcx+4], 0C3h
    lea     rax, [rcx+5]
    ret
Emit_FunctionEpilogue64 ENDP

; =============================================================================
; PE emitters
; =============================================================================

Emit_DOS PROC USES rsi rdi
    ; RCX = base
    lea     rdx, [rcx+DOS_OFF]

    mov     WORD PTR [rdx].IMAGE_DOS_HEADER.e_magic, DOS_MAGIC
    mov     WORD PTR [rdx].IMAGE_DOS_HEADER.e_cblp, 0090h
    mov     WORD PTR [rdx].IMAGE_DOS_HEADER.e_cp, 0003h
    mov     WORD PTR [rdx].IMAGE_DOS_HEADER.e_cparhdr, 0004h
    mov     WORD PTR [rdx].IMAGE_DOS_HEADER.e_maxalloc, 0FFFFh
    mov     WORD PTR [rdx].IMAGE_DOS_HEADER.e_sp, 00B8h
    mov     WORD PTR [rdx].IMAGE_DOS_HEADER.e_lfarlc, 0040h
    mov     DWORD PTR [rdx].IMAGE_DOS_HEADER.e_lfanew, NT_OFF

    ; Stub bytes at 0x40.. (fits before NT_OFF=0x80)
    lea     rdi, [rcx+DOS_STUB_OFF]
    lea     rsi, dos_stub_bytes
    mov     ecx, (dos_stub_end - dos_stub_bytes)
    rep movsb
    ret
Emit_DOS ENDP

Emit_NT PROC USES rbx rdi
    ; RCX = base
    lea     rdi, [rcx+NT_OFF]

    mov     DWORD PTR [rdi].IMAGE_NT_HEADERS64.Signature, PE_MAGIC

    ; FileHeader
    mov     WORD PTR [rdi].IMAGE_NT_HEADERS64.FileHeader.Machine, IMAGE_FILE_MACHINE_AMD64
    mov     WORD PTR [rdi].IMAGE_NT_HEADERS64.FileHeader.NumberOfSections, 2
    mov     WORD PTR [rdi].IMAGE_NT_HEADERS64.FileHeader.SizeOfOptionalHeader, (SIZEOF IMAGE_OPTIONAL_HEADER64)
    mov     ax, IMAGE_FILE_EXECUTABLE_IMAGE or IMAGE_FILE_LARGE_ADDRESS_AWARE
    mov     WORD PTR [rdi].IMAGE_NT_HEADERS64.FileHeader.Characteristics, ax

    ; OptionalHeader
    mov     WORD PTR [rdi].IMAGE_NT_HEADERS64.OptionalHeader.Magic, OPTIONAL_HDR64_MAGIC
    mov     DWORD PTR [rdi].IMAGE_NT_HEADERS64.OptionalHeader.SizeOfCode, TEXT_RAW_SIZE
    mov     DWORD PTR [rdi].IMAGE_NT_HEADERS64.OptionalHeader.SizeOfInitializedData, IDATA_RAW_SIZE
    mov     DWORD PTR [rdi].IMAGE_NT_HEADERS64.OptionalHeader.AddressOfEntryPoint, TEXT_RVA
    mov     DWORD PTR [rdi].IMAGE_NT_HEADERS64.OptionalHeader.BaseOfCode, TEXT_RVA
    ; mov [mem64], imm64 only supports sign-extended imm32; IMAGE_BASE doesn't fit.
    mov     rax, IMAGE_BASE
    mov     QWORD PTR [rdi].IMAGE_NT_HEADERS64.OptionalHeader.ImageBase, rax
    mov     DWORD PTR [rdi].IMAGE_NT_HEADERS64.OptionalHeader.SectionAlignment, SECTION_ALIGNMENT
    mov     DWORD PTR [rdi].IMAGE_NT_HEADERS64.OptionalHeader.FileAlignment, FILE_ALIGNMENT
    mov     WORD PTR [rdi].IMAGE_NT_HEADERS64.OptionalHeader.MajorOperatingSystemVersion, 6
    mov     WORD PTR [rdi].IMAGE_NT_HEADERS64.OptionalHeader.MajorSubsystemVersion, 6
    mov     DWORD PTR [rdi].IMAGE_NT_HEADERS64.OptionalHeader.SizeOfImage, 3000h
    mov     DWORD PTR [rdi].IMAGE_NT_HEADERS64.OptionalHeader.SizeOfHeaders, HEADERS_SIZE
    mov     WORD PTR [rdi].IMAGE_NT_HEADERS64.OptionalHeader.Subsystem, SUBSYSTEM_WINDOWS_GUI
    mov     ax, DLLCHAR_NX_COMPAT or DLLCHAR_TERMINAL_SERVER_AWARE
    mov     WORD PTR [rdi].IMAGE_NT_HEADERS64.OptionalHeader.DllCharacteristics, ax
    mov     QWORD PTR [rdi].IMAGE_NT_HEADERS64.OptionalHeader.SizeOfStackReserve, 100000h
    mov     QWORD PTR [rdi].IMAGE_NT_HEADERS64.OptionalHeader.SizeOfStackCommit, 1000h
    mov     QWORD PTR [rdi].IMAGE_NT_HEADERS64.OptionalHeader.SizeOfHeapReserve, 100000h
    mov     QWORD PTR [rdi].IMAGE_NT_HEADERS64.OptionalHeader.SizeOfHeapCommit, 1000h
    mov     DWORD PTR [rdi].IMAGE_NT_HEADERS64.OptionalHeader.NumberOfRvaAndSizes, 16

    ; DataDirectory[IMPORT] = index 1
    lea     rbx, [rdi].IMAGE_NT_HEADERS64.OptionalHeader.DataDirectory
    add     rbx, 8                            ; 1 * sizeof(IMAGE_DATA_DIRECTORY)
    mov     DWORD PTR [rbx].IMAGE_DATA_DIRECTORY.VirtualAddress, IMPORT_DIR_RVA
    mov     DWORD PTR [rbx].IMAGE_DATA_DIRECTORY._Size, 28h    ; 2 descriptors (0x14 each)

    ret
Emit_NT ENDP

Emit_SectionHeaders PROC USES rdi
    ; RCX = base
    lea     rdi, [rcx+SECT_OFF]

    ; .text
    ; Name is 8 bytes: ".text\0\0\0"
    mov     DWORD PTR [rdi].IMAGE_SECTION_HEADER.Name1, 'xet.'   ; ".tex"
    mov     DWORD PTR [rdi].IMAGE_SECTION_HEADER.Name1+4, 't'    ; "t\0\0\0"
    mov     DWORD PTR [rdi].IMAGE_SECTION_HEADER.VirtualSize, 020h
    mov     DWORD PTR [rdi].IMAGE_SECTION_HEADER.VirtualAddress, TEXT_RVA
    mov     DWORD PTR [rdi].IMAGE_SECTION_HEADER.SizeOfRawData, TEXT_RAW_SIZE
    mov     DWORD PTR [rdi].IMAGE_SECTION_HEADER.PointerToRawData, TEXT_RAW
    mov     eax, IMAGE_SCN_CNT_CODE or IMAGE_SCN_MEM_EXECUTE or IMAGE_SCN_MEM_READ
    mov     DWORD PTR [rdi].IMAGE_SECTION_HEADER.Characteristics, eax

    add     rdi, SIZEOF IMAGE_SECTION_HEADER

    ; .idata
    ; Name is 8 bytes: ".idata\0\0"
    mov     DWORD PTR [rdi].IMAGE_SECTION_HEADER.Name1, 'adi.'   ; ".ida"
    mov     DWORD PTR [rdi].IMAGE_SECTION_HEADER.Name1+4, 'at'   ; "ta\0\0"
    mov     DWORD PTR [rdi].IMAGE_SECTION_HEADER.VirtualSize, 0100h
    mov     DWORD PTR [rdi].IMAGE_SECTION_HEADER.VirtualAddress, IDATA_RVA
    mov     DWORD PTR [rdi].IMAGE_SECTION_HEADER.SizeOfRawData, IDATA_RAW_SIZE
    mov     DWORD PTR [rdi].IMAGE_SECTION_HEADER.PointerToRawData, IDATA_RAW
    mov     eax, IMAGE_SCN_CNT_INITIALIZED_DATA or IMAGE_SCN_MEM_READ or IMAGE_SCN_MEM_WRITE
    mov     DWORD PTR [rdi].IMAGE_SECTION_HEADER.Characteristics, eax

    ret
Emit_SectionHeaders ENDP

Emit_Idata PROC USES rbx rsi rdi
    ; RCX = base
    lea     rdi, [rcx+IDATA_RAW]

    ; Import descriptor (kernel32)
    mov     DWORD PTR [rdi+ID_OFF_IMPORT_DESC].IMAGE_IMPORT_DESCRIPTOR.OriginalFirstThunk, ILT_RVA
    mov     DWORD PTR [rdi+ID_OFF_IMPORT_DESC].IMAGE_IMPORT_DESCRIPTOR.NameRVA, DLL_NAME_RVA
    mov     DWORD PTR [rdi+ID_OFF_IMPORT_DESC].IMAGE_IMPORT_DESCRIPTOR.FirstThunk, IAT_RVA
    ; Null descriptor already zero (buffer pre-zeroed)

    ; ILT (2 qwords)
    mov     QWORD PTR [rdi+ID_OFF_ILT+0], IMPORT_BY_NAME_RVA
    mov     QWORD PTR [rdi+ID_OFF_ILT+8], 0

    ; IAT (2 qwords)
    mov     QWORD PTR [rdi+ID_OFF_IAT+0], IMPORT_BY_NAME_RVA
    mov     QWORD PTR [rdi+ID_OFF_IAT+8], 0

    ; IMAGE_IMPORT_BY_NAME { WORD Hint; CHAR Name[]; }
    mov     WORD PTR [rdi+ID_OFF_IMPORT_BY_NAME+0], 0
    lea     rsi, sz_ExitProcess
    lea     rbx, [rdi+ID_OFF_IMPORT_BY_NAME+2]
@@copy_fn:
    mov     al, BYTE PTR [rsi]
    mov     BYTE PTR [rbx], al
    inc     rsi
    inc     rbx
    test    al, al
    jnz     @@copy_fn

    ; DLL name at offset 0x60 (section-relative)
    lea     rsi, sz_kernel32
    lea     rbx, [rdi+ID_OFF_DLL_NAME]
@@copy_dll:
    mov     al, BYTE PTR [rsi]
    mov     BYTE PTR [rbx], al
    inc     rsi
    inc     rbx
    test    al, al
    jnz     @@copy_dll

    ret
Emit_Idata ENDP

Emit_Text PROC USES rdi
    ; RCX = base
    lea     rdi, [rcx+TEXT_RAW]

    ; prologue: sub rsp, 28h
    mov     rcx, rdi
    mov     edx, 28h
    call    Emit_FunctionPrologue64
    mov     rdi, rax

    ; xor ecx, ecx  => 33 C9
    mov     BYTE PTR [rdi+0], 033h
    mov     BYTE PTR [rdi+1], 0C9h
    add     rdi, 2

    ; call qword ptr [rip+disp32] => FF 15 xx xx xx xx
    mov     BYTE PTR [rdi+0], 0FFh
    mov     BYTE PTR [rdi+1], 015h
    mov     DWORD PTR [rdi+2], CALL_DISP32
    add     rdi, 6

    ; epilogue: add rsp, 28h; ret
    mov     rcx, rdi
    mov     edx, 28h
    call    Emit_FunctionEpilogue64
    ret
Emit_Text ENDP

Build_PE PROC
    ; RCX = outBuffer
    ; returns RAX = total file size
    push    rbx

    ; RCX is volatile and is clobbered by helpers (e.g., rep movsb uses RCX as a counter).
    ; Keep the base pointer in RBX and reload RCX for each emitter call.
    mov     rbx, rcx

    mov     rcx, rbx
    call    Emit_DOS
    mov     rcx, rbx
    call    Emit_NT
    mov     rcx, rbx
    call    Emit_SectionHeaders
    mov     rcx, rbx
    call    Emit_Idata
    mov     rcx, rbx
    call    Emit_Text

    mov     eax, (HEADERS_SIZE + TEXT_RAW_SIZE + IDATA_RAW_SIZE)
    pop     rbx
    ret
Build_PE ENDP

; =============================================================================
; Entry
; =============================================================================

main PROC
    ; Win64 ABI: keep stack 16-byte aligned for calls, and reserve shadow space.
    sub     rsp, 68h

    ; If any arguments exist after argv[0], skip the interactive menu (headless-friendly).
    ; autoMode flag stored at [rsp+40h] (byte)
    mov     BYTE PTR [rsp+40h], 0
    call    GetCommandLineA                 ; RAX = pszCmdLine
    mov     rsi, rax

    ; Skip program name (quoted or unquoted), then spaces.
    mov     al, BYTE PTR [rsi]
    cmp     al, '"'
    jne     @@skip_unquoted

    inc     rsi
@@skip_quoted_loop:
    mov     al, BYTE PTR [rsi]
    test    al, al
    jz      @@show_menu
    cmp     al, '"'
    je      @@after_prog
    inc     rsi
    jmp     @@skip_quoted_loop

@@skip_unquoted:
@@skip_unquoted_loop:
    mov     al, BYTE PTR [rsi]
    test    al, al
    jz      @@show_menu
    cmp     al, ' '
    je      @@after_prog
    inc     rsi
    jmp     @@skip_unquoted_loop

@@after_prog:
    ; skip closing quote or the space delimiter
    cmp     BYTE PTR [rsi], 0
    je      @@show_menu
    inc     rsi
@@skip_spaces:
    mov     al, BYTE PTR [rsi]
    cmp     al, ' '
    jne     @@have_args_check
    inc     rsi
    jmp     @@skip_spaces

@@have_args_check:
    test    al, al
    jz      @@show_menu
    jmp     @@auto_yes

@@show_menu:
    ; "menu": yes/no
    xor     ecx, ecx
    lea     rdx, szPrompt
    lea     r8,  szTitle
    mov     r9d, MB_YESNO or MB_ICONQUESTION
    call    MessageBoxA
    test    eax, eax
    jz      @@auto_yes                       ; headless/no-UI: treat as auto-build
    cmp     eax, IDYES
    jne     @@exit

    mov     BYTE PTR [rsp+40h], 0
    jmp     @@do_build

@@auto_yes:
    mov     BYTE PTR [rsp+40h], 1
    jmp     @@do_build

@@do_build:
    lea     rcx, peBuffer
    call    Build_PE
    mov     ebx, eax                         ; file size (<= 4GB)

    ; hFile = CreateFileA(out, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0)
    lea     rcx, szOutFile
    mov     edx, GENERIC_WRITE
    xor     r8d, r8d
    xor     r9d, r9d
    mov     QWORD PTR [rsp+20h], CREATE_ALWAYS
    mov     QWORD PTR [rsp+28h], FILE_ATTRIBUTE_NORMAL
    mov     QWORD PTR [rsp+30h], 0
    call    CreateFileA
    cmp     rax, INVALID_HANDLE_VALUE
    je      @@fail_create
    mov     r12, rax

    ; WriteFile(hFile, peBuffer, fileSize, &bytesWritten, NULL)
    mov     rcx, r12
    lea     rdx, peBuffer
    mov     r8d, ebx
    lea     r9,  [rsp+38h]                   ; bytesWritten (DWORD)
    mov     QWORD PTR [rsp+20h], 0           ; lpOverlapped
    call    WriteFile
    test    eax, eax
    jz      @@fail_write

    mov     rcx, r12
    call    CloseHandle

    cmp     BYTE PTR [rsp+40h], 1
    je      @@exit_ok

    xor     ecx, ecx
    lea     rdx, szOk
    lea     r8,  szTitle
    xor     r9d, r9d
    call    MessageBoxA
    jmp     @@exit

@@fail_create:
    call    GetLastError
    mov     ebx, eax
    cmp     BYTE PTR [rsp+40h], 1
    je      @@exit_err

    xor     ecx, ecx
    lea     rdx, szFail
    lea     r8,  szTitle
    xor     r9d, r9d
    call    MessageBoxA
    jmp     @@exit

@@fail_write:
    call    GetLastError
    mov     ebx, eax
    cmp     BYTE PTR [rsp+40h], 1
    je      @@exit_err

    xor     ecx, ecx
    lea     rdx, szFail
    lea     r8,  szTitle
    xor     r9d, r9d
    call    MessageBoxA
    jmp     @@exit

@@exit_ok:
    xor     ecx, ecx
    call    ExitProcess

@@exit_err:
    mov     ecx, ebx
    call    ExitProcess

@@exit:
    xor     ecx, ecx
    call    ExitProcess
    ; not reached
    add     rsp, 68h
    ret
main ENDP

; Headless/test entrypoint: no UI, always emits + writes output.exe.
; Link with: /entry:main_auto
main_auto PROC
    sub     rsp, 68h

    lea     rcx, peBuffer
    call    Build_PE
    mov     ebx, eax

    lea     rcx, szOutFile
    mov     edx, GENERIC_WRITE
    xor     r8d, r8d
    xor     r9d, r9d
    mov     QWORD PTR [rsp+20h], CREATE_ALWAYS
    mov     QWORD PTR [rsp+28h], FILE_ATTRIBUTE_NORMAL
    mov     QWORD PTR [rsp+30h], 0
    call    CreateFileA
    cmp     rax, INVALID_HANDLE_VALUE
    je      @@fail_create
    mov     r12, rax

    mov     rcx, r12
    lea     rdx, peBuffer
    mov     r8d, ebx
    lea     r9,  [rsp+38h]
    mov     QWORD PTR [rsp+20h], 0
    call    WriteFile
    test    eax, eax
    jz      @@fail_write

    mov     rcx, r12
    call    CloseHandle

    xor     ecx, ecx
    call    ExitProcess

@@fail_create:
    call    GetLastError
    mov     ecx, eax
    call    ExitProcess

@@fail_write:
    call    GetLastError
    mov     ecx, eax
    call    ExitProcess
main_auto ENDP

; Strings referenced by Emit_Idata
sz_kernel32     db "kernel32.dll",0
sz_ExitProcess  db "ExitProcess",0

END
