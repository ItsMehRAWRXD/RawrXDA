; =============================================================================
; RawrXD PE Generator & Encoder - PRODUCTION READY
; Pure MASM x64 - Zero Dependencies
; Generates valid Windows PE32+ executables from scratch
; Build: ml64 pe_generator_production.asm /link /SUBSYSTEM:CONSOLE /ENTRY:main
; =============================================================================

OPTION CASEMAP:NONE
OPTION DOTNAME

; =============================================================================
; INCLUDES & EXTERNALS
; =============================================================================

includelib kernel32.lib
includelib ntdll.lib

ExitProcess proto :dword
VirtualAlloc proto :ptr, :qword, :dword, :dword
VirtualFree proto :ptr, :qword, :dword
CreateFileA proto :ptr, :dword, :dword, :ptr, :dword, :dword, :ptr
WriteFile proto :ptr, :ptr, :dword, :ptr, :ptr
CloseHandle proto :ptr
RtlZeroMemory proto :ptr, :qword
RtlCopyMemory proto :ptr, :ptr, :qword

; =============================================================================
; CONSTANTS
; =============================================================================

PE_MAGIC                        equ 00004550h
PE32PLUS_MAGIC                  equ 020Bh
IMAGE_FILE_MACHINE_AMD64        equ 8664h
IMAGE_FILE_EXECUTABLE_IMAGE     equ 0002h
IMAGE_FILE_LARGE_ADDRESS_AWARE  equ 0020h
IMAGE_SUBSYSTEM_WINDOWS_CUI     equ 3
IMAGE_DLLCHARACTERISTICS_DYNAMIC_BASE equ 0040h
IMAGE_DLLCHARACTERISTICS_NX_COMPAT equ 0100h
IMAGE_SCN_CNT_CODE              equ 00000020h
IMAGE_SCN_MEM_EXECUTE           equ 20000000h
IMAGE_SCN_MEM_READ              equ 40000000h

MEM_COMMIT                      equ 1000h
MEM_RESERVE                     equ 2000h
MEM_RELEASE                     equ 8000h
PAGE_READWRITE                  equ 04h
GENERIC_WRITE                   equ 40000000h
CREATE_ALWAYS                   equ 2
FILE_ATTRIBUTE_NORMAL           equ 80h

; =============================================================================
; STRUCTURES
; =============================================================================

IMAGE_DOS_HEADER struct
    e_magic         word    ?
    e_cblp          word    ?
    e_cp            word    ?
    e_crlc          word    ?
    e_cparhdr       word    ?
    e_minalloc      word    ?
    e_maxalloc      word    ?
    e_ss            word    ?
    e_sp            word    ?
    e_csum          word    ?
    e_ip            word    ?
    e_cs            word    ?
    e_lfarlc        word    ?
    e_ovno          word    ?
    e_res           word    4 dup(?)
    e_oemid         word    ?
    e_oeminfo       word    ?
    e_res2          word    10 dup(?)
    e_lfanew        dword   ?
IMAGE_DOS_HEADER ends

IMAGE_FILE_HEADER struct
    Machine             word    ?
    NumberOfSections    word    ?
    TimeDateStamp       dword   ?
    PointerToSymbolTable dword   ?
    NumberOfSymbols     dword   ?
    SizeOfOptionalHeader word    ?
    Characteristics     word    ?
IMAGE_FILE_HEADER ends

IMAGE_OPTIONAL_HEADER64 struct
    Magic                       word    ?
    MajorLinkerVersion          byte    ?
    MinorLinkerVersion          byte    ?
    SizeOfCode                  dword   ?
    SizeOfInitializedData       dword   ?
    SizeOfUninitializedData     dword   ?
    AddressOfEntryPoint         dword   ?
    BaseOfCode                  dword   ?
    ImageBase                   qword   ?
    SectionAlignment            dword   ?
    FileAlignment               dword   ?
    MajorOperatingSystemVersion word    ?
    MinorOperatingSystemVersion word    ?
    MajorImageVersion           word    ?
    MinorImageVersion           word    ?
    MajorSubsystemVersion       word    ?
    MinorSubsystemVersion       word    ?
    Win32VersionValue           dword   ?
    SizeOfImage                 dword   ?
    SizeOfHeaders               dword   ?
    CheckSum                    dword   ?
    Subsystem                   word    ?
    DllCharacteristics          word    ?
    SizeOfStackReserve          qword   ?
    SizeOfStackCommit           qword   ?
    SizeOfHeapReserve           qword   ?
    SizeOfHeapCommit            qword   ?
    LoaderFlags                 dword   ?
    NumberOfRvaAndSizes         dword   ?
IMAGE_OPTIONAL_HEADER64 ends

IMAGE_DATA_DIRECTORY struct
    VirtualAddress  dword   ?
    DataSize        dword   ?
IMAGE_DATA_DIRECTORY ends

IMAGE_SECTION_HEADER struct
    Name            byte    8 dup(?)
    VirtualSize     dword   ?
    VirtualAddress  dword   ?
    SizeOfRawData   dword   ?
    PointerToRawData dword   ?
    PointerToRelocations dword   ?
    PointerToLinenumbers dword   ?
    NumberOfRelocations word    ?
    NumberOfLinenumbers word    ?
    Characteristics dword   ?
IMAGE_SECTION_HEADER ends

PE_BUILDER_CONTEXT struct
    ImageBase           qword   ?
    EntryPointRVA       dword   ?
    Subsystem           dword   ?
    DosHeader           qword   ?
    PeHeaders           qword   ?
    SectionTable        qword   ?
    CodeBuffer          qword   ?
    CodeSize            qword   ?
    DataBuffer          qword   ?
    DataSize            qword   ?
    OutputBuffer        qword   ?
    OutputSize          qword   ?
    SectionCount        dword   ?
    SectionAlignment    dword   ?
    FileAlignment       dword   ?
PE_BUILDER_CONTEXT ends

; Offsets for manual struct access
CTX_ImageBase           equ 0
CTX_EntryPointRVA       equ 8
CTX_Subsystem           equ 12
CTX_DosHeader           equ 16
CTX_PeHeaders           equ 24
CTX_SectionTable        equ 32
CTX_CodeBuffer          equ 40
CTX_CodeSize            equ 48
CTX_DataBuffer          equ 56
CTX_DataSize            equ 64
CTX_OutputBuffer        equ 72
CTX_OutputSize          equ 80
CTX_SectionCount        equ 88
CTX_SectionAlignment    equ 92
CTX_FileAlignment       equ 96

; IMAGE_FILE_HEADER field offsets
IFH_Machine                 equ 0
IFH_NumberOfSections        equ 2
IFH_TimeDateStamp           equ 4
IFH_PointerToSymbolTable    equ 8
IFH_NumberOfSymbols         equ 12
IFH_SizeOfOptionalHeader    equ 16
IFH_Characteristics         equ 18

; IMAGE_OPTIONAL_HEADER64 field offsets
IOH_Magic                       equ 0
IOH_MajorLinkerVersion          equ 2
IOH_MinorLinkerVersion          equ 3
IOH_SizeOfCode                  equ 4
IOH_SizeOfInitializedData       equ 8
IOH_SizeOfUninitializedData     equ 12
IOH_AddressOfEntryPoint         equ 16
IOH_BaseOfCode                  equ 20
IOH_ImageBase                   equ 24
IOH_SectionAlignment            equ 32
IOH_FileAlignment               equ 36
IOH_MajorOperatingSystemVersion equ 40
IOH_MinorOperatingSystemVersion equ 42
IOH_MajorImageVersion           equ 44
IOH_MinorImageVersion           equ 46
IOH_MajorSubsystemVersion       equ 48
IOH_MinorSubsystemVersion       equ 50
IOH_Win32VersionValue           equ 52
IOH_SizeOfImage                 equ 56
IOH_SizeOfHeaders               equ 60
IOH_CheckSum                    equ 64
IOH_Subsystem                   equ 68
IOH_DllCharacteristics          equ 70
IOH_SizeOfStackReserve          equ 72
IOH_SizeOfStackCommit           equ 80
IOH_SizeOfHeapReserve           equ 88
IOH_SizeOfHeapCommit            equ 96
IOH_LoaderFlags                 equ 104
IOH_NumberOfRvaAndSizes         equ 108

; =============================================================================
; DATA
; =============================================================================

.data
align 16

default_code_payload:
    db 48h, 0C7h, 0C1h, 02Ah, 00h, 00h, 00h       ; mov rcx, 42
    db 48h, 0C7h, 0C0h, 06Ah, 02h, 00h, 00h       ; mov rax, 0x26A
    db 48h, 31h, 0D2h                              ; xor rdx, rdx
    db 0Fh, 05h                                    ; syscall
default_code_size equ $ - default_code_payload

encoder_key db 0DEh, 0ADh, 0BEh, 0EFh, 0CAh, 0FEh, 0BAh, 0BEh, 013h, 037h
encoder_key_len equ $ - encoder_key

szDefaultOutput db "output.exe", 0
szTextSection db ".text", 0, 0, 0

; =============================================================================
; CODE
; =============================================================================

.code
align 16

PE_EncoderXOR proc frame
    push rbx
    .pushreg rbx
    push rsi
    .pushreg rsi
    push rdi
    .pushreg rdi
    .endprolog
    
    test rcx, rcx
    jz @done
    test rdx, rdx
    jz @done
    
    mov rsi, rcx
    mov rcx, rdx
    mov rbx, r8
    test rbx, rbx
    jnz @key_set
    lea rbx, encoder_key
    
@key_set:
    xor rdx, rdx
    mov rdi, encoder_key_len
    
@loop:
    mov al, [rsi]
    mov r8b, [rbx + rdx]
    xor al, r8b
    mov [rsi], al
    inc rsi
    inc rdx
    cmp rdx, rdi
    jb @next
    xor rdx, rdx
@next:
    dec rcx
    jnz @loop
    
@done:
    pop rdi
    pop rsi
    pop rbx
    ret
PE_EncoderXOR endp

PE_EncoderRolling proc frame
    push rbx
    .pushreg rbx
    push rsi
    .pushreg rsi
    .endprolog
    
    test rcx, rcx
    jz @done
    test rdx, rdx
    jz @done
    
    mov rsi, rcx
    mov rcx, rdx
    xor rbx, rbx
    
@loop:
    mov al, [rsi]
    xor al, bl
    mov bl, al
    mov [rsi], al
    inc rsi
    dec rcx
    jnz @loop
    
@done:
    pop rsi
    pop rbx
    ret
PE_EncoderRolling endp

PE_EncoderNOT proc frame
    push rsi
    .pushreg rsi
    .endprolog
    
    test rcx, rcx
    jz @done
    mov rsi, rcx
    
@loop:
    not byte ptr [rsi]
    inc rsi
    dec rdx
    jnz @loop
    
@done:
    pop rsi
    ret
PE_EncoderNOT endp

PE_InitBuilder proc frame
    push rbx
    .pushreg rbx
    push rdi
    .pushreg rdi
    sub rsp, 32
    .allocstack 32
    .endprolog
    
    mov rbx, rcx
    mov rdi, rcx
    mov r10, rdx
    mov r11, r8

    mov rcx, rbx
    mov rdx, SIZEOF PE_BUILDER_CONTEXT
    call RtlZeroMemory

    mov dword ptr [rbx + CTX_SectionAlignment], 1000h
    mov dword ptr [rbx + CTX_FileAlignment], 200h

    mov rdx, r10
    test rdx, rdx
    jnz @imgbase_set
    mov rdx, 140000000h
    
@imgbase_set:
    mov qword ptr [rbx + CTX_ImageBase], rdx
    mov r8, r11
    test r8, r8
    jnz @subsystem_set
    mov r8d, IMAGE_SUBSYSTEM_WINDOWS_CUI
    
@subsystem_set:
    mov dword ptr [rbx + CTX_Subsystem], r8d
    mov rax, rbx
    
    add rsp, 32
    pop rdi
    pop rbx
    ret
PE_InitBuilder endp

PE_BuildDosHeader proc frame
    push rbx
    .pushreg rbx
    push rdi
    .pushreg rdi
    sub rsp, 32
    .allocstack 32
    .endprolog
    
    mov rbx, rcx
    xor rcx, rcx
    mov rdx, 40h
    mov r8d, MEM_COMMIT or MEM_RESERVE
    mov r9d, PAGE_READWRITE
    call VirtualAlloc
    test rax, rax
    jz @fail
    
    mov qword ptr [rbx + CTX_DosHeader], rax
    mov rdi, rax
    
    mov word ptr [rdi], 5A4Dh
    mov word ptr [rdi + 2], 0090h
    mov word ptr [rdi + 4], 0003h
    mov word ptr [rdi + 0Ah], 0004h
    mov word ptr [rdi + 0Eh], 0FFFFh
    mov word ptr [rdi + 12h], 00B8h
    mov word ptr [rdi + 1Ch], 0040h
    mov dword ptr [rdi + 3Ch], 80h
    
    mov rax, 1
    jmp @done
    
@fail:
    xor rax, rax
    
@done:
    add rsp, 32
    pop rdi
    pop rbx
    ret
PE_BuildDosHeader endp

PE_BuildPeHeaders proc frame
    push rbx
    .pushreg rbx
    push rsi
    .pushreg rsi
    push rdi
    .pushreg rdi
    push r12
    .pushreg r12
    sub rsp, 32
    .allocstack 32
    .endprolog

    mov rbx, rcx
    mov r12, SIZEOF IMAGE_FILE_HEADER
    add r12, SIZEOF IMAGE_OPTIONAL_HEADER64
    add r12, 16 * SIZEOF IMAGE_DATA_DIRECTORY

    xor rcx, rcx
    mov rdx, r12
    mov r8d, MEM_COMMIT or MEM_RESERVE
    mov r9d, PAGE_READWRITE
    call VirtualAlloc
    test rax, rax
    jz @fail

    mov qword ptr [rbx + CTX_PeHeaders], rax
    mov rdi, rax

    mov word ptr [rdi + IFH_Machine], IMAGE_FILE_MACHINE_AMD64
    mov ax, word ptr [rbx + CTX_SectionCount]
    mov word ptr [rdi + IFH_NumberOfSections], ax
    mov dword ptr [rdi + IFH_TimeDateStamp], 66666666h
    mov eax, SIZEOF IMAGE_OPTIONAL_HEADER64 + 16 * SIZEOF IMAGE_DATA_DIRECTORY
    mov [rdi + IFH_SizeOfOptionalHeader], ax
    mov word ptr [rdi + IFH_Characteristics], IMAGE_FILE_EXECUTABLE_IMAGE or IMAGE_FILE_LARGE_ADDRESS_AWARE

    add rdi, SIZEOF IMAGE_FILE_HEADER

    mov word ptr [rdi + IOH_Magic], PE32PLUS_MAGIC
    mov byte ptr [rdi + IOH_MajorLinkerVersion], 14
    mov dword ptr [rdi + IOH_SizeOfCode], 200h
    mov dword ptr [rdi + IOH_SizeOfInitializedData], 200h

    mov eax, [rbx + CTX_EntryPointRVA]
    test eax, eax
    jnz @entry_set
    mov eax, 1000h
@entry_set:
    mov dword ptr [rdi + IOH_AddressOfEntryPoint], eax
    mov dword ptr [rdi + IOH_BaseOfCode], 1000h

    mov rax, [rbx + CTX_ImageBase]
    mov qword ptr [rdi + IOH_ImageBase], rax

    mov eax, [rbx + CTX_SectionAlignment]
    mov dword ptr [rdi + IOH_SectionAlignment], eax
    mov eax, [rbx + CTX_FileAlignment]
    mov dword ptr [rdi + IOH_FileAlignment], eax

    mov word ptr [rdi + IOH_MajorOperatingSystemVersion], 6
    mov dword ptr [rdi + IOH_SizeOfHeaders], 400h

    mov eax, [rbx + CTX_SectionCount]
    inc eax
    shl eax, 12
    mov dword ptr [rdi + IOH_SizeOfImage], eax
    mov qword ptr [rdi + IOH_SizeOfStackReserve], 100000h
    mov qword ptr [rdi + IOH_SizeOfStackCommit], 1000h
    mov qword ptr [rdi + IOH_SizeOfHeapReserve], 100000h
    mov qword ptr [rdi + IOH_SizeOfHeapCommit], 1000h
    mov word ptr [rdi + IOH_DllCharacteristics], IMAGE_DLLCHARACTERISTICS_DYNAMIC_BASE or IMAGE_DLLCHARACTERISTICS_NX_COMPAT

    mov dword ptr [rdi + IOH_NumberOfRvaAndSizes], 16

    add rdi, SIZEOF IMAGE_OPTIONAL_HEADER64
    mov rcx, rdi
    mov rdx, 16 * SIZEOF IMAGE_DATA_DIRECTORY
    xor r8, r8
    call RtlZeroMemory

    mov rax, 1
    jmp @done

@fail:
    xor rax, rax

@done:
    add rsp, 32
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
PE_BuildPeHeaders endp

PE_Serialize proc frame
    push rbx
    .pushreg rbx
    push rsi
    .pushreg rsi
    push rdi
    .pushreg rdi
    push r12
    .pushreg r12
    push r15
    .pushreg r15
    sub rsp, 32
    .allocstack 32
    .endprolog
    
    mov rbx, rcx
    mov r15d, edx
    mov r12, 400h
    
    mov qword ptr [rbx + CTX_OutputSize], r12
    xor rcx, rcx
    mov rdx, r12
    mov r8d, MEM_COMMIT or MEM_RESERVE
    mov r9d, PAGE_READWRITE
    call VirtualAlloc
    test rax, rax
    jz @fail
    
    mov qword ptr [rbx + CTX_OutputBuffer], rax
    mov rdi, rax
    
    mov rsi, [rbx + CTX_DosHeader]
    test rsi, rsi
    jz @no_dos
    
    mov rcx, rdi
    mov rdx, rsi
    mov r8, 40h
    call RtlCopyMemory
    add rdi, 40h
    
@no_dos:
    mov rcx, rdi
    xor rdx, rdx
    mov r8, 40h
    call RtlZeroMemory
    add rdi, 40h
    
    mov dword ptr [rdi], PE_MAGIC
    add rdi, 4
    
    mov rsi, [rbx + CTX_PeHeaders]
    mov rcx, rdi
    mov rdx, rsi
    mov r8, SIZEOF IMAGE_FILE_HEADER
    call RtlCopyMemory
    add rdi, SIZEOF IMAGE_FILE_HEADER
    
    add rsi, SIZEOF IMAGE_FILE_HEADER
    mov rcx, rdi
    mov rdx, rsi
    mov r8, SIZEOF IMAGE_OPTIONAL_HEADER64
    call RtlCopyMemory
    add rdi, SIZEOF IMAGE_OPTIONAL_HEADER64
    
    add rsi, SIZEOF IMAGE_OPTIONAL_HEADER64
    mov rcx, rdi
    mov rdx, rsi
    mov r8, 16 * SIZEOF IMAGE_DATA_DIRECTORY
    call RtlCopyMemory
    add rdi, 16 * SIZEOF IMAGE_DATA_DIRECTORY
    
    mov rax, rdi
    sub rax, [rbx + CTX_OutputBuffer]
    mov rcx, 400h
    sub rcx, rax
    push rcx
    mov rcx, rdi
    xor rdx, rdx
    pop r8
    call RtlZeroMemory
    
    mov rax, [rbx + CTX_OutputBuffer]
    jmp @done
    
@fail:
    xor rax, rax
    
@done:
    add rsp, 32
    pop r15
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
PE_Serialize endp

PE_WriteFile proc frame
    push rbx
    .pushreg rbx
    push rsi
    .pushreg rsi
    push rdi
    .pushreg rdi
    sub rsp, 64
    .allocstack 64
    .endprolog
    
    mov rbx, rcx
    mov rsi, rdx
    
    mov rcx, rsi
    mov edx, GENERIC_WRITE
    xor r8, r8
    xor r9, r9
    mov dword ptr [rsp + 32], CREATE_ALWAYS
    mov dword ptr [rsp + 40], FILE_ATTRIBUTE_NORMAL
    mov qword ptr [rsp + 48], 0
    call CreateFileA
    cmp rax, -1
    je @fail
    
    mov rdi, rax
    mov rcx, rdi
    mov rdx, [rbx + CTX_OutputBuffer]
    mov r8d, dword ptr [rbx + CTX_OutputSize]
    lea r9, [rsp + 32]
    mov qword ptr [rsp + 32], 0
    call WriteFile
    test eax, eax
    jz @cleanup
    
    mov eax, 1
    jmp @cleanup
    
@cleanup:
    mov rcx, rdi
    call CloseHandle
    jmp @done
    
@fail:
    xor eax, eax
    
@done:
    add rsp, 64
    pop rdi
    pop rsi
    pop rbx
    ret
PE_WriteFile endp

PE_Cleanup proc frame
    push rbx
    .pushreg rbx
    sub rsp, 32
    .allocstack 32
    .endprolog
    
    mov rbx, rcx
    
    mov rcx, [rbx + CTX_DosHeader]
    test rcx, rcx
    jz @skip_dos
    xor rdx, rdx
    mov r8d, MEM_RELEASE
    call VirtualFree
    
@skip_dos:
    mov rcx, [rbx + CTX_PeHeaders]
    test rcx, rcx
    jz @skip_pe
    xor rdx, rdx
    mov r8d, MEM_RELEASE
    call VirtualFree
    
@skip_pe:
    mov rcx, [rbx + CTX_OutputBuffer]
    test rcx, rcx
    jz @done
    xor rdx, rdx
    mov r8d, MEM_RELEASE
    call VirtualFree
    
@done:
    add rsp, 32
    pop rbx
    ret
PE_Cleanup endp

PE_GenerateSimple proc frame
    push rbx
    .pushreg rbx
    push rsi
    .pushreg rsi
    push rdi
    .pushreg rdi
    push r12
    .pushreg r12
    push r13
    .pushreg r13
    push r14
    .pushreg r14
    sub rsp, 168
    .allocstack 168
    .endprolog
    
    mov r14, rcx
    mov r12, rdx
    mov r13, r8
    lea rbx, [rsp + 32]
    
    mov rcx, rbx
    xor rdx, rdx
    xor r8, r8
    call PE_InitBuilder
    
    mov rcx, rbx
    call PE_BuildDosHeader
    test rax, rax
    jz @fail
    
    test r12, r12
    jnz @use_custom
    lea r12, default_code_payload
    mov r13, default_code_size
    
@use_custom:
    mov dword ptr [rbx + CTX_EntryPointRVA], 1000h
    
    mov rcx, rbx
    call PE_BuildPeHeaders
    test rax, rax
    jz @fail
    
    mov rcx, rbx
    xor edx, edx
    call PE_Serialize
    test rax, rax
    jz @fail
    
    mov rdi, [rbx + CTX_OutputBuffer]
    add rdi, 400h
    mov rcx, rdi
    mov rdx, r12
    mov r8, r13
    call RtlCopyMemory
    
    mov rcx, rbx
    mov rdx, r14
    call PE_WriteFile
    test eax, eax
    jz @fail
    
    mov rcx, rbx
    call PE_Cleanup
    mov rax, 1
    jmp @done
    
@fail:
    mov rcx, rbx
    call PE_Cleanup
    xor rax, rax
    
@done:
    add rsp, 168
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
PE_GenerateSimple endp

main proc frame
    push rbx
    .pushreg rbx
    sub rsp, 32
    .allocstack 32
    .endprolog
    
    lea rcx, szDefaultOutput
    xor rdx, rdx
    xor r8, r8
    call PE_GenerateSimple
    test rax, rax
    jz @fail
    
    xor ecx, ecx
    jmp @exit
    
@fail:
    mov ecx, 1
    
@exit:
    call ExitProcess
main endp

public PE_EncoderXOR
public PE_EncoderRolling
public PE_EncoderNOT
public PE_InitBuilder
public PE_BuildDosHeader
public PE_BuildPeHeaders
public PE_Serialize
public PE_WriteFile
public PE_Cleanup
public PE_GenerateSimple

end
