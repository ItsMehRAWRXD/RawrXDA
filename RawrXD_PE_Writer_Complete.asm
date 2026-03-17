; =============================================================================
; RawrXD_PE_Writer_Complete.asm — Phase 1-2 (ml64-compatible)
; Pure x64 MASM PE32+ Binary Emitter
; Generates byte-reproducible PE32+ executables from assembly (ZERO deps)
; =============================================================================

OPTION CASEMAP:NONE

; ============================================================================
; PE CONSTANTS
; ============================================================================
IMAGE_DOS_SIGNATURE         EQU 5A4Dh
IMAGE_NT_SIGNATURE          EQU 4550h  
IMAGE_NT_OPTIONAL_HDR64_MAGIC EQU 20Bh

IMAGE_FILE_MACHINE_AMD64    EQU 8664h
IMAGE_FILE_EXECUTABLE_IMAGE EQU 0002h
IMAGE_FILE_LARGE_ADDRESS_AWARE EQU 0020h

IMAGE_SUBSYSTEM_WINDOWS_CUI EQU 3

IMAGE_DLLCHARACTERISTICS_DYNAMIC_BASE EQU 0040h
IMAGE_DLLCHARACTERISTICS_NX_COMPAT    EQU 0100h

PAGE_SIZE                   EQU 1000h
FILE_ALIGNMENT              EQU 200h

IMAGE_DIRECTORY_ENTRY_IMPORT EQU 1

IMAGE_SCN_CNT_CODE            EQU 00000020h
IMAGE_SCN_CNT_INITIALIZED_DATA EQU 00000040h
IMAGE_SCN_MEM_EXECUTE         EQU 20000000h
IMAGE_SCN_MEM_READ            EQU 40000000h
IMAGE_SCN_MEM_WRITE           EQU 80000000h

GENERIC_WRITE         EQU 40000000h
CREATE_ALWAYS         EQU 2
FILE_ATTRIBUTE_NORMAL EQU 80h
INVALID_HANDLE_VALUE  EQU -1

; ============================================================================
; GLOBAL STATE
; ============================================================================

.data
    ALIGN 16

    ; PE Buffer (256KB)
    g_PE_Buffer         BYTE 40000h dup(?)
    
    ; Section tracking
    g_Section_Count     DWORD 0
    g_Output_File       QWORD 0
    g_Total_File_Size   QWORD 0

; ============================================================================
; CODE SECTION
; ============================================================================

.code

EXTERN CreateFileW:PROC
EXTERN WriteFile:PROC
EXTERN CloseHandle:PROC
EXTERN ExitProcess:PROC

; ============================================================================
; BUILD_DOS_HEADER
; ============================================================================
BUILD_DOS_HEADER PROC FRAME
    .pushreg rbp
    push rbp
    mov rbp, rsp
    .endprolog
    
    lea rax, g_PE_Buffer
    
    ; Signature "MZ"
    mov word [rax], IMAGE_DOS_SIGNATURE
    
    ; PE offset at 0x3C
    mov dword [rax + 3Ch], 40h
    
    pop rbp
    ret
BUILD_DOS_HEADER ENDP

; ============================================================================
; BUILD_NT_HEADERS
; ============================================================================
BUILD_NT_HEADERS PROC FRAME
    .pushreg rbp
    push rbp
    mov rbp, rsp
    .pushreg rdi
    push rdi
    .endprolog
    
    ; NT headers @ 0x40
    lea rdi, [g_PE_Buffer + 40h]
    
    ; === SIGNATURE ===
    mov dword [rdi], IMAGE_NT_SIGNATURE
    
    ; === FILE HEADER (@ +4) ===
    mov ax, IMAGE_FILE_MACHINE_AMD64
    mov [rdi + 4], ax
    
    mov ax, 2                   ; NumberOfSections
    mov [rdi + 6], ax
    
    mov dword [rdi + 8], 0      ; TimeDateStamp
    mov dword [rdi + 12], 0     ; PointerToSymbolTable
    mov dword [rdi + 16], 0     ; NumberOfSymbols
    
    mov ax, 240                 ; SizeOfOptionalHeader
    mov [rdi + 20], ax
    
    mov ax, IMAGE_FILE_EXECUTABLE_IMAGE or IMAGE_FILE_LARGE_ADDRESS_AWARE
    mov [rdi + 22], ax          ; Characteristics
    
    ; === OPTIONAL HEADER (@ +24) ===
    mov ax, IMAGE_NT_OPTIONAL_HDR64_MAGIC
    mov [rdi + 24], ax          ; Magic
    
    mov byte [rdi + 26], 14     ; MajorLinkerVersion
    mov byte [rdi + 27], 0      ; MinorLinkerVersion
    
    ;SizeOfCode
    mov dword [rdi + 28], 200h
    ; SizeOfInitializedData
    mov dword [rdi + 32], 200h
    ; SizeOfUninitializedData
    mov dword [rdi + 36], 0
    
    ; AddressOfEntryPoint
    mov dword [rdi + 40], 1000h
    ; BaseOfCode
    mov dword [rdi + 44], 1000h
    
    ; ImageBase (140000000h for ASLR)
    mov qword [rdi + 48], 140000000h
    
    ; SectionAlignment, FileAlignment
    mov dword [rdi + 56], PAGE_SIZE
    mov dword [rdi + 60], FILE_ALIGNMENT
    
    ; OS/Image versions
    mov ax, 6
    mov [rdi + 64], ax          ; MajorOperatingSystemVersion
    mov [rdi + 66], ax          ; MajorSubsystemVersion
    mov ax, 0
    mov [rdi + 68], ax
    mov [rdi + 70], ax
    
    ; SizeOfHeaders, SizeOfImage (placeholder)
    mov dword [rdi + 80], 400h
    mov dword [rdi + 84], 400h
    
    mov dword [rdi + 88], 0     ; CheckSum
    
    mov ax, IMAGE_SUBSYSTEM_WINDOWS_CUI
    mov [rdi + 90], ax
    
    mov ax, IMAGE_DLLCHARACTERISTICS_DYNAMIC_BASE or IMAGE_DLLCHARACTERISTICS_NX_COMPAT
    mov [rdi + 92], ax
    
    ; Stack/Heap
    mov qword [rdi + 96], 100000h
    mov qword [rdi + 104], 1000h
    mov qword [rdi + 112], 100000h
    mov qword [rdi + 120], 1000h
    
    mov dword [rdi + 128], 0    ; LoaderFlags
    mov dword [rdi + 132], 16   ; NumberOfRvaAndSizes
    
    pop rdi
    pop rbp
    ret
BUILD_NT_HEADERS ENDP

; ============================================================================
; ADD_SECTION_HEADER
; RCX = Section name  
; RDX = Virtual size
; R8  = Raw size
; R9  = Characteristics
; ============================================================================
ADD_SECTION_HEADER PROC FRAME
    .pushreg rbp
    push rbp
    mov rbp, rsp
    .pushreg rdi
    push rdi
    .endprolog
    
    ; Section headers start @ 0x140
    lea rdi, [g_PE_Buffer + 140h]
    
    mov eax, g_Section_Count
    mov r10d, 40                ; sizeof IMAGE_SECTION_HEADER
    imul eax, r10d
    add rdi, rax
    
    ; Name (8 bytes)
    mov rax, rcx
    mov [rdi], rax
    
    ; VirtualSize
    mov [rdi + 8], edx
    
    ; VirtualAddress
    mov eax, g_Section_Count
    mov ecx, PAGE_SIZE
    imul eax, ecx
    add eax, 1000h
    mov [rdi + 12], eax
    
    ; SizeOfRawData (aligned)
    mov eax, r8d
    add eax, FILE_ALIGNMENT - 1
    and eax, 0FFFFF800h         ; Align to FILE_ALIGNMENT
    mov [rdi + 16], eax
    
    ; PointerToRawData
    mov eax, g_Section_Count
    mov ecx, FILE_ALIGNMENT
    imul eax, ecx
    add eax, 400h
    mov [rdi + 20], eax
    
    ; Relocations, Linenumbers
    mov dword [rdi + 24], 0
    mov dword [rdi + 28], 0
    mov word [rdi + 32], 0
    mov word [rdi + 34], 0
    
    ; Characteristics
    mov [rdi + 36], r9d
    
    inc g_Section_Count
    
    pop rdi
    pop rbp
    ret
ADD_SECTION_HEADER ENDP

; ============================================================================
; UPDATE_FINAL_SIZES
; ============================================================================
UPDATE_FINAL_SIZES PROC FRAME
    .pushreg rbp
    push rbp
    mov rbp, rsp
    .pushreg rdi
    push rdi
    .endprolog
    
    lea rdi, [g_PE_Buffer + 40h]
    
    ; SizeOfHeaders
    mov eax, 140h
    mov ecx, g_Section_Count
    mov edx, 40
    imul ecx, edx
    add eax, ecx
    add eax, FILE_ALIGNMENT - 1
    and eax, 0FFFFF800h
    mov [rdi + 80], eax         ; @ OptionalHeader.SizeOfHeaders
    
    ; SizeOfImage  
    mov ecx, g_Section_Count
    mov edx, PAGE_SIZE
    imul ecx, edx
    add eax, ecx
    mov [rdi + 84], eax         ; @ OptionalHeader.SizeOfImage
    
    pop rdi
    pop rbp
    ret
UPDATE_FINAL_SIZES ENDP

; ============================================================================
; WRITE_PE_TO_FILE
; RCX = Output filename
; ============================================================================
WRITE_PE_TO_FILE PROC FRAME
    .pushreg rbp
    push rbp
    mov rbp, rsp
    sub rsp, 40h
    .pushreg rdi
    push rdi
    .endprolog
    
    mov rdi, rcx
    
    ; CreateFileW
    mov rcx, rdi
    mov edx, GENERIC_WRITE
    xor r8d, r8d
    xor r9d, r9d
    mov qword [rsp + 20h], CREATE_ALWAYS
    mov qword [rsp + 28h], FILE_ATTRIBUTE_NORMAL
    mov qword [rsp + 30h], 0
    call CreateFileW
    
    mov g_Output_File, rax
    cmp rax, INVALID_HANDLE_VALUE
    je error_create
    
    ; Calculate size
    mov eax, 800h               ; Approximate output size
    mov g_Total_File_Size, rax
    
    ; WriteFile
    mov rcx, g_Output_File
    lea rdx, g_PE_Buffer
    mov r8, g_Total_File_Size
    lea r9, [rsp + 32h]
    mov qword [rsp + 20h], 0
    call WriteFile
    
    test eax, eax
    jz error_write
    
    ; CloseHandle
    mov rcx, g_Output_File
    call CloseHandle
    
    mov rax, g_Total_File_Size
    jmp done
    
error_create:
error_write:
    xor rax, rax
    
done:
    pop rdi
    add rsp, 40h
    pop rbp
    ret
WRITE_PE_TO_FILE ENDP

; ============================================================================
; MAIN
; ============================================================================
main PROC FRAME
    .pushreg rbp
    push rbp
    mov rbp, rsp
    sub rsp, 40h
    .endprolog
    
    ; Build headers
    call BUILD_DOS_HEADER
    call BUILD_NT_HEADERS
    
    ; Add sections
    lea rcx, szTextName
    mov edx, 1000h
    mov r8d, 200h
    mov r9d, IMAGE_SCN_CNT_CODE or IMAGE_SCN_MEM_EXECUTE or IMAGE_SCN_MEM_READ
    call ADD_SECTION_HEADER
    
    lea rcx, szRdataName
    mov edx, 1000h
    mov r8d, 200h
    mov r9d, IMAGE_SCN_CNT_INITIALIZED_DATA or IMAGE_SCN_MEM_READ
    call ADD_SECTION_HEADER
    
    ; Update sizes
    call UPDATE_FINAL_SIZES
    
    ; Write output
    lea rcx, szOutputPath
    call WRITE_PE_TO_FILE
    
    test rax, rax
    jz fail
    
    xor ecx, ecx
    jmp exit_ok
    
fail:
    mov ecx, 1
    
exit_ok:
    add rsp, 40h
    pop rbp
    call ExitProcess
    
main ENDP

; ============================================================================
; PHASE 3: COMPLETE IMPORT TABLE BUILDER
; ============================================================================

; BUILD_IMPORT_DIRECTORY_TABLE
; Constructs Import Directory Entries for kernel32.dll and user32.dll
; Returns RVA in rax, size in rdx
BUILD_IMPORT_DIRECTORY_TABLE PROC FRAME
    push rbx
    push rsi
    .PUSHREG rbx
    .PUSHREG rsi
    .ENDPROLOG
    
    ; Import directory at .rdata + 0x0
    lea rax, [g_PE_Buffer]
    add rax, 2000h                      ; .rdata RVA = 0x2000
    
    lea rcx, [g_PE_Buffer]
    add rcx, 3000h                      ; ILT at 0x3000
    
    ; ─────────────────────────────────────────────────────
    ; Import Directory Entry 0: kernel32.dll
    ; ─────────────────────────────────────────────────────
    
    ; ImportLookupTable RVA (points to ILT_kernel32)
    mov dword [rax + 0], 3000h
    
    ; TimeDateStamp (not bound)
    mov dword [rax + 4], 0
    
    ; ForwarderChain (-1 = no forwarding)
    mov dword [rax + 8], 0FFFFFFFFh
    
    ; Name RVA (string "kernel32.dll" at 0x4000)
    mov dword [rax + 12], 4000h
    
    ; ImportAddressTable RVA (IAT will be at 0x3200)
    mov dword [rax + 16], 3200h
    
    ; ─────────────────────────────────────────────────────
    ; Import Directory Entry 1: user32.dll
    ; ─────────────────────────────────────────────────────
    
    add rax, 20                         ; Next entry
    
    ; ImportLookupTable RVA
    mov dword [rax + 0], 3400h
    
    ; TimeDateStamp
    mov dword [rax + 4], 0
    
    ; ForwarderChain
    mov dword [rax + 8], 0FFFFFFFFh
    
    ; Name RVA ("user32.dll" at 0x4010)
    mov dword [rax + 12], 4010h
    
    ; ImportAddressTable RVA
    mov dword [rax + 16], 3600h
    
    ; ─────────────────────────────────────────────────────
    ; Null terminator
    ; ─────────────────────────────────────────────────────
    
    add rax, 20
    xor edx, edx
    mov qword [rax], rdx
    mov dword [rax + 8], 0
    
    ; Return size of directory (2 entries + null = 60 bytes)
    mov rdx, 60
    
    pop rsi
    pop rbx
    ret
BUILD_IMPORT_DIRECTORY_TABLE ENDP

; BUILD_IMPORT_LOOKUP_TABLE
; Creates ILT with hint/name RVAs for kernel32.dll
BUILD_IMPORT_LOOKUP_TABLE PROC FRAME
    .ENDPROLOG
    
    ; ILT for kernel32 at 0x3000
    lea rax, [g_PE_Buffer]
    add rax, 3000h
    
    ; ILT Entry 0: ExitProcess (hint/name at 0x4100)
    mov qword [rax + 0], 4100h
    
    ; ILT Entry 1: WriteFile (hint/name at 0x4110)
    mov qword [rax + 8], 4110h
    
    ; ILT Entry 2: GetModuleHandleA (hint/name at 0x4120)
    mov qword [rax + 16], 4120h
    
    ; Null terminator
    mov qword [rax + 24], 0
    
    ; ILT for user32 at 0x3400
    add rax, 400h
    
    ; ILT Entry 0: MessageBoxA (hint/name at 0x4200)
    mov qword [rax + 0], 4200h
    
    ; ILT Entry 1: CreateWindowExA (hint/name at 0x4210)
    mov qword [rax + 8], 4210h
    
    ; Null terminator
    mov qword [rax + 16], 0
    
    ret
BUILD_IMPORT_LOOKUP_TABLE ENDP

; BUILD_IMPORT_ADDRESS_TABLE
; Creates IAT (parallel to ILT, will be filled by loader at runtime)
BUILD_IMPORT_ADDRESS_TABLE PROC FRAME
    .ENDPROLOG
    
    ; IAT for kernel32 at 0x3200
    lea rax, [g_PE_Buffer]
    add rax, 3200h
    
    ; Copy ILT entries to IAT (loader will fixup)
    lea rdx, [g_PE_Buffer]
    add rdx, 3000h
    
    ; Copy 4 entries (3 functions + null term)
    mov ecx, 4
.copy_kernel32:
    mov r8, qword [rdx]
    mov qword [rax], r8
    add rdx, 8
    add rax, 8
    dec ecx
    jnz .copy_kernel32
    
    ; IAT for user32 at 0x3600
    lea rax, [g_PE_Buffer]
    add rax, 3600h
    
    lea rdx, [g_PE_Buffer]
    add rdx, 3400h
    
    ; Copy 3 entries (2 functions + null term)
    mov ecx, 3
.copy_user32:
    mov r8, qword [rdx]
    mov qword [rax], r8
    add rdx, 8
    add rax, 8
    dec ecx
    jnz .copy_user32
    
    ret
BUILD_IMPORT_ADDRESS_TABLE ENDP

; BUILD_HINT_NAME_TABLE
; Creates hint/name pairs for all imported functions
BUILD_HINT_NAME_TABLE PROC FRAME
    push rsi
    .PUSHREG rsi
    .ENDPROLOG
    
    ; Hint/Name table at 0x4100
    lea rax, [g_PE_Buffer]
    add rax, 4100h
    
    ; ─────────────────────────────────────────────────────
    ; kernel32.dll functions
    ; ─────────────────────────────────────────────────────
    
    ; Entry 0: ExitProcess (Hint=0)
    mov word [rax + 0], 0
    lea rcx, [szExitProcess]
    call Copy_Function_Name_To_Buffer     ; rax += incremented
    
    ; Entry 1: WriteFile (Hint=1)
    mov word [rax + 0], 1
    lea rcx, [szWriteFile]
    call Copy_Function_Name_To_Buffer
    
    ; Entry 2: GetModuleHandleA (Hint=2)
    mov word [rax + 0], 2
    lea rcx, [szGetModuleHandleA]
    call Copy_Function_Name_To_Buffer
    
    ; ─────────────────────────────────────────────────────
    ; user32.dll functions
    ; ─────────────────────────────────────────────────────
    
    ; Adjust for user32 section at 0x4200
    lea rax, [g_PE_Buffer]
    add rax, 4200h
    
    ; Entry 0: MessageBoxA (Hint=3)
    mov word [rax + 0], 3
    lea rcx, [szMessageBoxA]
    call Copy_Function_Name_To_Buffer
    
    ; Entry 1: CreateWindowExA (Hint=4)
    mov word [rax + 0], 4
    lea rcx, [szCreateWindowExA]
    call Copy_Function_Name_To_Buffer
    
    pop rsi
    ret
BUILD_HINT_NAME_TABLE ENDP

; COPY_FUNCTION_NAME_TO_BUFFER (helper)
; rax = buffer position (hint already written at [rax])
; rcx = function name string
; Updates rax to next hint/name position
Copy_Function_Name_To_Buffer PROC FRAME
    .ENDPROLOG
    
    add rax, 2                          ; Skip hint word
    mov rsi, rax
    
    ; Copy null-terminated string
.copy_loop:
    mov r9b, [rcx]
    mov [rsi], r9b
    test r9b, r9b
    jz .copy_done
    inc rcx
    inc rsi
    jmp .copy_loop
    
.copy_done:
    ; Align to next 8-byte boundary for next hint/name
    mov rax, rsi
    add rax, 1                          ; Account for null terminator
    add rax, 7
    and rax, 0FFFFFFF8h                 ; Align down (next multiple of 8)
    ret
Copy_Function_Name_To_Buffer ENDP

; BUILD_DLL_NAME_STRINGS
; Places DLL name strings at 0x4000 and 0x4010
BUILD_DLL_NAME_STRINGS PROC FRAME
    .ENDPROLOG
    
    lea rax, [g_PE_Buffer]
    add rax, 4000h                      ; kernel32.dll at 0x4000
    
    ; Copy "kernel32.dll"
    lea rcx, [szKernel32DLL]
    xor edx, edx
.copy_k32:
    mov r9b, [rcx + rdx]
    mov [rax + rdx], r9b
    test r9b, r9b
    jz .copy_k32_done
    inc edx
    jmp .copy_k32
    
.copy_k32_done:
    ; Copy "user32.dll" at 0x4010
    add rax, 10h
    lea rcx, [szUser32DLL]
    xor edx, edx
    
.copy_u32:
    mov r9b, [rcx + rdx]
    mov [rax + rdx], r9b
    test r9b, r9b
    jz .copy_u32_done
    inc edx
    jmp .copy_u32
    
.copy_u32_done:
    ret
BUILD_DLL_NAME_STRINGS ENDP

; BUILD_COMPLETE_IMPORT_SYSTEM
; Orchestrates all import table construction
BUILD_COMPLETE_IMPORT_SYSTEM PROC FRAME
    push rbx
    .PUSHREG rbx
    .ENDPROLOG
    
    ; Step 1: Build DLL name strings
    call BUILD_DLL_NAME_STRINGS
    
    ; Step 2: Build import directory
    call BUILD_IMPORT_DIRECTORY_TABLE
    
    ; Step 3: Build ILTs
    call BUILD_IMPORT_LOOKUP_TABLE
    
    ; Step 4: Build IATs
    call BUILD_IMPORT_ADDRESS_TABLE
    
    ; Step 5: Build hint/name tables
    call BUILD_HINT_NAME_TABLE
    
    pop rbx
    ret
BUILD_COMPLETE_IMPORT_SYSTEM ENDP

; ============================================================================
; DATA
; ============================================================================

.data
    
    szTextName      DB ".text",0,0,0
    szRdataName     DB ".rdata",0,0
    szOutputPath    DB "D:\",0,"r",0,"a",0,"w",0,"r",0,"x",0,"d",0,"\",0,"o",0,"u",0,"t",0,"p",0,"u",0,"t",0,"\",0,"t",0,"e",0,"s",0,"t",0,"_",0,"m",0,"i",0,"n",0,"i",0,"m",0,"a",0,"l",0,"_",0,"p",0,"e",0,".",0,"e",0,"x",0,"e",0,0,0
    
    ; Import strings
    szKernel32DLL       DB "kernel32.dll", 0
    szUser32DLL         DB "user32.dll", 0
    
    ; Function names
    szExitProcess       DB "ExitProcess", 0
    szWriteFile         DB "WriteFile", 0
    szGetModuleHandleA  DB "GetModuleHandleA", 0
    szMessageBoxA       DB "MessageBoxA", 0
    szCreateWindowExA   DB "CreateWindowExA", 0

END main
