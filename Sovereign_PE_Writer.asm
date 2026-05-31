OPTION CASEMAP:NONE

EXTERN GetStdHandle:PROC
EXTERN WriteFile:PROC
EXTERN CreateFileA:PROC
EXTERN CloseHandle:PROC
EXTERN ExitProcess:PROC

;==============================================================================
; PE Structure Constants
;==============================================================================

; IMAGE_DOS_HEADER offsets
DOS_e_magic         EQU 0
DOS_e_lfanew        EQU 60
SIZEOF_DOS_HEADER   EQU 64

; IMAGE_NT_HEADERS64 offsets
NT_Signature        EQU 0
NT_FileHeader       EQU 4

; IMAGE_FILE_HEADER offsets
FH_Machine              EQU 0
FH_NumberOfSections     EQU 2
FH_TimeDateStamp        EQU 4
FH_PointerToSymbolTable EQU 8
FH_NumberOfSymbols      EQU 12
FH_SizeOfOptionalHeader EQU 16
FH_Characteristics      EQU 18
SIZEOF_FILE_HEADER      EQU 20

; IMAGE_OPTIONAL_HEADER64 offsets
OPT_Magic                       EQU 0
OPT_MajorLinkerVersion          EQU 2
OPT_MinorLinkerVersion          EQU 3
OPT_SizeOfCode                  EQU 4
OPT_SizeOfInitializedData       EQU 8
OPT_SizeOfUninitializedData     EQU 12
OPT_AddressOfEntryPoint         EQU 16
OPT_BaseOfCode                  EQU 20
OPT_ImageBase                   EQU 24
OPT_SectionAlignment            EQU 32
OPT_FileAlignment               EQU 36
OPT_MajorOperatingSystemVersion EQU 40
OPT_MinorOperatingSystemVersion EQU 42
OPT_MajorImageVersion           EQU 44
OPT_MinorImageVersion           EQU 46
OPT_MajorSubsystemVersion       EQU 48
OPT_MinorSubsystemVersion       EQU 50
OPT_Win32VersionValue           EQU 52
OPT_SizeOfImage                 EQU 56
OPT_SizeOfHeaders               EQU 60
OPT_CheckSum                    EQU 64
OPT_Subsystem                   EQU 68
OPT_DllCharacteristics          EQU 70
OPT_SizeOfStackReserve          EQU 72
OPT_SizeOfStackCommit           EQU 80
OPT_SizeOfHeapReserve           EQU 88
OPT_SizeOfHeapCommit            EQU 96
OPT_LoaderFlags                 EQU 104
OPT_NumberOfRvaAndSizes         EQU 108
OPT_DataDirectory               EQU 112
SIZEOF_OPT_HEADER64             EQU 240
NUM_DATA_DIRS                   EQU 16
SIZEOF_DATA_DIR                 EQU 8

; IMAGE_SECTION_HEADER offsets
SEC_Name                    EQU 0
SEC_VirtualSize             EQU 8
SEC_VirtualAddress          EQU 12
SEC_SizeOfRawData           EQU 16
SEC_PointerToRawData        EQU 20
SEC_PointerToRelocations    EQU 24
SEC_PointerToLinenumbers    EQU 28
SEC_NumberOfRelocations     EQU 32
SEC_NumberOfLinenumbers     EQU 34
SEC_Characteristics         EQU 36
SIZEOF_SECTION_HEADER       EQU 40

; Signatures and constants
PE_DOS_SIGNATURE            EQU 5A4Dh
PE_NT_SIGNATURE             EQU 00004550h
PE_OPT_MAGIC64              EQU 20Bh
PE_MACHINE_AMD64            EQU 8664h
PE_SUBSYSTEM_CUI            EQU 3
PE_FILE_ALIGNMENT           EQU 200h
PE_SECTION_ALIGNMENT        EQU 1000h
PE_IMAGE_BASE64             EQU 140000000h

; File header characteristics
PE_FILE_EXECUTABLE_IMAGE    EQU 0002h
PE_FILE_LARGE_ADDRESS_AWARE EQU 0020h

; Section characteristics
PE_SCN_CNT_CODE             EQU 00000020h
PE_SCN_MEM_EXECUTE          EQU 20000000h
PE_SCN_MEM_READ             EQU 40000000h

; Win32 constants
STD_OUTPUT_HANDLE           EQU -11
GENERIC_WRITE               EQU 40000000h
CREATE_ALWAYS               EQU 2
FILE_ATTRIBUTE_NORMAL       EQU 80h
INVALID_HANDLE_VALUE        EQU -1

.DATA
ALIGN 8
g_pe_buf            BYTE 65536 DUP(0)
g_num_buf           BYTE 32 DUP(0)
g_pe_size           QWORD 0
g_stdout            QWORD 0
g_file              QWORD 0

szOutFile           BYTE "sovereign_pe_test.exe",0
szTextName          BYTE ".text",0,0,0
szOkBuilt           BYTE "[PE-WRITER] PE built: ",0
szBytes             BYTE " bytes",13,10,0
szOkWrote           BYTE "[PE-WRITER] Wrote: sovereign_pe_test.exe",13,10,0
szErrWrite          BYTE "[PE-WRITER] Error: file write failed",13,10,0
szErrOverflow       BYTE "[PE-WRITER] Error: buffer overflow",13,10,0

test_code           BYTE 090h, 0C3h   ; nop; ret
test_code_end       LABEL BYTE

.CODE

align_up PROC FRAME
    .endprolog
    test    rcx, rcx
    jz      align_done
    dec     rcx
    add     rax, rcx
    not     rcx
    and     rax, rcx
align_done:
    ret
align_up ENDP

strlen_s PROC FRAME
    .endprolog
    xor     eax, eax
strlen_loop:
    cmp     BYTE PTR [rcx+rax], 0
    je      strlen_done
    inc     rax
    jmp     strlen_loop
strlen_done:
    ret
strlen_s ENDP

memset_s PROC FRAME
    push    rdi
    .pushreg rdi
    .endprolog
    mov     rdi, rdx
    mov     al, r8b
    rep     stosb
    pop     rdi
    ret
memset_s ENDP

memcpy_s PROC FRAME
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    .endprolog
    mov     rdi, rcx
    mov     rsi, rdx
    mov     rcx, r8
    rep     movsb
    pop     rdi
    pop     rsi
    ret
memcpy_s ENDP

write_con PROC FRAME
    sub     rsp, 40h
    .allocstack 40h
    .endprolog
    mov     r8d, edx
    mov     rdx, rcx
    mov     rcx, [g_stdout]
    lea     r9, [rsp+30h]
    mov     QWORD PTR [rsp+20h], 0
    call    WriteFile
    add     rsp, 40h
    ret
write_con ENDP

itoa_u64 PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    sub     rsp, 20h
    .allocstack 20h
    .endprolog

    mov     rdi, rcx
    mov     rbx, rax
    xor     esi, esi
    mov     ecx, 10

    test    rbx, rbx
    jnz     itoa_loop
    mov     BYTE PTR [rdi], '0'
    mov     BYTE PTR [rdi+1], 0
    mov     rax, rdi
    jmp     itoa_exit

itoa_loop:
    xor     edx, edx
    mov     rax, rbx
    div     rcx
    add     dl, '0'
    mov     BYTE PTR [rsp+rsi], dl
    inc     rsi
    mov     rbx, rax
    test    rbx, rbx
    jnz     itoa_loop

    mov     rax, rdi
itoa_emit:
    dec     rsi
    mov     dl, BYTE PTR [rsp+rsi]
    mov     BYTE PTR [rdi], dl
    inc     rdi
    test    rsi, rsi
    jnz     itoa_emit
    mov     BYTE PTR [rdi], 0

itoa_exit:
    add     rsp, 20h
    pop     rdi
    pop     rsi
    pop     rbx
    ret
itoa_u64 ENDP

pe_write_dos_header PROC FRAME
    .endprolog
    mov     WORD PTR [rcx+DOS_e_magic], PE_DOS_SIGNATURE
    mov     DWORD PTR [rcx+DOS_e_lfanew], 80h
    ret
pe_write_dos_header ENDP

pe_write_nt_headers PROC FRAME
    push    rbx
    .pushreg rbx
    .endprolog

    mov     DWORD PTR [rcx+NT_Signature], PE_NT_SIGNATURE

    lea     rbx, [rcx+NT_FileHeader]
    mov     WORD PTR [rbx+FH_Machine], PE_MACHINE_AMD64
    mov     WORD PTR [rbx+FH_NumberOfSections], 1
    mov     DWORD PTR [rbx+FH_TimeDateStamp], 0
    mov     DWORD PTR [rbx+FH_PointerToSymbolTable], 0
    mov     DWORD PTR [rbx+FH_NumberOfSymbols], 0
    mov     WORD PTR [rbx+FH_SizeOfOptionalHeader], SIZEOF_OPT_HEADER64
    mov     WORD PTR [rbx+FH_Characteristics], PE_FILE_EXECUTABLE_IMAGE OR PE_FILE_LARGE_ADDRESS_AWARE

    lea     rbx, [rbx+SIZEOF_FILE_HEADER]
    mov     WORD PTR [rbx+OPT_Magic], PE_OPT_MAGIC64
    mov     BYTE PTR [rbx+OPT_MajorLinkerVersion], 1
    mov     BYTE PTR [rbx+OPT_MinorLinkerVersion], 0
    mov     DWORD PTR [rbx+OPT_SizeOfCode], r8d
    mov     DWORD PTR [rbx+OPT_SizeOfInitializedData], 0
    mov     DWORD PTR [rbx+OPT_SizeOfUninitializedData], 0
    mov     DWORD PTR [rbx+OPT_AddressOfEntryPoint], 1000h
    mov     DWORD PTR [rbx+OPT_BaseOfCode], 1000h
    mov     rax, PE_IMAGE_BASE64
    mov     QWORD PTR [rbx+OPT_ImageBase], rax
    mov     DWORD PTR [rbx+OPT_SectionAlignment], PE_SECTION_ALIGNMENT
    mov     DWORD PTR [rbx+OPT_FileAlignment], PE_FILE_ALIGNMENT
    mov     WORD PTR [rbx+OPT_MajorOperatingSystemVersion], 6
    mov     WORD PTR [rbx+OPT_MinorOperatingSystemVersion], 0
    mov     WORD PTR [rbx+OPT_MajorImageVersion], 0
    mov     WORD PTR [rbx+OPT_MinorImageVersion], 0
    mov     WORD PTR [rbx+OPT_MajorSubsystemVersion], 6
    mov     WORD PTR [rbx+OPT_MinorSubsystemVersion], 0
    mov     DWORD PTR [rbx+OPT_Win32VersionValue], 0
    mov     DWORD PTR [rbx+OPT_SizeOfImage], r9d
    mov     DWORD PTR [rbx+OPT_SizeOfHeaders], edx
    mov     DWORD PTR [rbx+OPT_CheckSum], 0
    mov     WORD PTR [rbx+OPT_Subsystem], PE_SUBSYSTEM_CUI
    mov     WORD PTR [rbx+OPT_DllCharacteristics], 0
    mov     QWORD PTR [rbx+OPT_SizeOfStackReserve], 100000h
    mov     QWORD PTR [rbx+OPT_SizeOfStackCommit], 1000h
    mov     QWORD PTR [rbx+OPT_SizeOfHeapReserve], 100000h
    mov     QWORD PTR [rbx+OPT_SizeOfHeapCommit], 1000h
    mov     DWORD PTR [rbx+OPT_LoaderFlags], 0
    mov     DWORD PTR [rbx+OPT_NumberOfRvaAndSizes], NUM_DATA_DIRS

    lea     rdx, [rbx+OPT_DataDirectory]
    mov     rcx, NUM_DATA_DIRS*SIZEOF_DATA_DIR
    xor     r8d, r8d
    call    memset_s

    pop     rbx
    ret
pe_write_nt_headers ENDP

pe_write_text_section PROC FRAME
    .endprolog
    mov     DWORD PTR [rcx+SEC_Name], '.txe'
    mov     WORD PTR [rcx+SEC_Name+4], 't'
    mov     WORD PTR [rcx+SEC_Name+6], 0
    mov     DWORD PTR [rcx+SEC_VirtualSize], edx
    mov     DWORD PTR [rcx+SEC_VirtualAddress], 1000h
    mov     DWORD PTR [rcx+SEC_SizeOfRawData], r8d
    mov     DWORD PTR [rcx+SEC_PointerToRawData], r9d
    mov     DWORD PTR [rcx+SEC_PointerToRelocations], 0
    mov     DWORD PTR [rcx+SEC_PointerToLinenumbers], 0
    mov     WORD PTR [rcx+SEC_NumberOfRelocations], 0
    mov     WORD PTR [rcx+SEC_NumberOfLinenumbers], 0
    mov     DWORD PTR [rcx+SEC_Characteristics], PE_SCN_CNT_CODE OR PE_SCN_MEM_EXECUTE OR PE_SCN_MEM_READ
    ret
pe_write_text_section ENDP

write_pe_file PROC FRAME
    sub     rsp, 50h
    .allocstack 50h
    .endprolog

    lea     rcx, szOutFile
    mov     edx, GENERIC_WRITE
    xor     r8d, r8d
    xor     r9d, r9d
    mov     QWORD PTR [rsp+20h], CREATE_ALWAYS
    mov     QWORD PTR [rsp+28h], FILE_ATTRIBUTE_NORMAL
    mov     QWORD PTR [rsp+30h], 0
    call    CreateFileA
    cmp     rax, INVALID_HANDLE_VALUE
    je      write_fail
    mov     [g_file], rax

    mov     rcx, [g_file]
    lea     rdx, g_pe_buf
    mov     r8, [g_pe_size]
    lea     r9, [rsp+38h]
    mov     QWORD PTR [rsp+20h], 0
    call    WriteFile
    test    eax, eax
    jz      write_fail_close

    mov     rcx, [g_file]
    call    CloseHandle
    mov     eax, 1
    add     rsp, 50h
    ret

write_fail_close:
    mov     rcx, [g_file]
    call    CloseHandle
write_fail:
    xor     eax, eax
    add     rsp, 50h
    ret
write_pe_file ENDP

pe_build PROC FRAME
    push    rbp
    .pushreg rbp
    mov     rbp, rsp
    .setframe rbp, 0
    sub     rsp, 40h
    .allocstack 40h
    push    rbx
    .pushreg rbx
    push    r12
    .pushreg r12
    push    r13
    .pushreg r13
    .endprolog

    mov     r12, rcx                ; code ptr
    mov     r13, rdx                ; code size

    ; headers_size = align_up(80h + 4 + file + opt + section, 200h)
    mov     rax, 80h + 4 + SIZEOF_FILE_HEADER + SIZEOF_OPT_HEADER64 + SIZEOF_SECTION_HEADER
    mov     rcx, PE_FILE_ALIGNMENT
    call    align_up
    mov     rbx, rax                ; headers size

    ; raw_size = align_up(code_size, 200h)
    mov     rax, r13
    mov     rcx, PE_FILE_ALIGNMENT
    call    align_up
    mov     [rbp-8], rax            ; raw size

    ; virt_size_aligned = align_up(code_size, 1000h)
    mov     rax, r13
    mov     rcx, PE_SECTION_ALIGNMENT
    call    align_up
    mov     [rbp-10h], rax

    ; size_of_image = 1000h + virt_size_aligned
    mov     rax, 1000h
    add     rax, [rbp-10h]
    mov     [rbp-18h], rax

    ; total file size = headers_size + raw_size
    mov     rax, rbx
    add     rax, [rbp-8]
    mov     [g_pe_size], rax
    cmp     rax, LENGTHOF g_pe_buf
    ja      pe_overflow

    ; zero entire output range
    lea     rdx, g_pe_buf
    mov     rcx, [g_pe_size]
    xor     r8d, r8d
    call    memset_s

    ; DOS header at 0
    lea     rcx, g_pe_buf
    call    pe_write_dos_header

    ; NT headers at 80h
    lea     rcx, g_pe_buf+80h
    mov     edx, ebx                ; size of headers
    mov     r8d, DWORD PTR [rbp-8]  ; size of code/raw
    mov     r9d, DWORD PTR [rbp-18h] ; size of image
    call    pe_write_nt_headers

    ; section header after optional header
    lea     rcx, g_pe_buf+80h+4+SIZEOF_FILE_HEADER+SIZEOF_OPT_HEADER64
    mov     edx, r13d
    mov     r8d, DWORD PTR [rbp-8]
    mov     r9d, ebx
    call    pe_write_text_section

    ; copy code to first section raw data
    lea     rcx, g_pe_buf
    add     rcx, rbx
    mov     rdx, r12
    mov     r8, r13
    call    memcpy_s

    mov     rax, 1
    jmp     pe_done

pe_overflow:
    lea     rcx, szErrOverflow
    mov     edx, SIZEOF szErrOverflow - 1
    call    write_con
    xor     eax, eax

pe_done:
    pop     r13
    pop     r12
    pop     rbx
    leave
    ret
pe_build ENDP

main PROC FRAME
    sub     rsp, 40h
    .allocstack 40h
    .endprolog

    mov     ecx, STD_OUTPUT_HANDLE
    call    GetStdHandle
    mov     [g_stdout], rax

    lea     rcx, test_code
    mov     rdx, OFFSET test_code_end - OFFSET test_code
    call    pe_build
    test    eax, eax
    jz      main_fail

    call    write_pe_file
    test    eax, eax
    jz      main_write_fail

    lea     rcx, szOkBuilt
    mov     edx, SIZEOF szOkBuilt - 1
    call    write_con

    mov     rax, [g_pe_size]
    lea     rcx, g_num_buf
    call    itoa_u64
    mov     rcx, rax
    call    strlen_s
    mov     edx, eax
    lea     rcx, g_num_buf
    call    write_con

    lea     rcx, szBytes
    mov     edx, SIZEOF szBytes - 1
    call    write_con

    lea     rcx, szOkWrote
    mov     edx, SIZEOF szOkWrote - 1
    call    write_con

    xor     ecx, ecx
    call    ExitProcess

main_write_fail:
    lea     rcx, szErrWrite
    mov     edx, SIZEOF szErrWrite - 1
    call    write_con
main_fail:
    mov     ecx, 1
    call    ExitProcess
main ENDP

END