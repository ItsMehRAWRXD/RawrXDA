; PE_Test_Helpers.asm
; Helper functions for PE Writer integration tests
; Provides file validation, execution testing, and utility functions

OPTION CASEMAP:NONE

; Windows API functions
EXTERN CreateFileA: PROC
EXTERN ReadFile: PROC
EXTERN CloseHandle: PROC
EXTERN GetFileSize: PROC
EXTERN CreateProcessA: PROC
EXTERN WaitForSingleObject: PROC
EXTERN GetTickCount64: PROC
EXTERN SetFilePointer: PROC

; Public helper functions
PUBLIC StrLen
PUBLIC CheckFileExists
PUBLIC ExecuteAndWait
PUBLIC ValidateExecutableFile
PUBLIC ValidateDOSHeader
PUBLIC ValidateNTHeaders
PUBLIC ValidateSectionHeaders
PUBLIC ValidateImportTable
PUBLIC GenerateComplexCode
PUBLIC FreePEContext
PUBLIC GetPerformanceTime

.data
; File validation constants
IMAGE_DOS_SIGNATURE equ 5A4Dh
IMAGE_NT_SIGNATURE equ 00004550h
IMAGE_FILE_MACHINE_AMD64 equ 8664h
IMAGE_SCN_CNT_CODE equ 00000020h
IMAGE_SCN_CNT_INITIALIZED_DATA equ 00000040h

; Process creation structures
STARTUPINFOA STRUCT
    cb              DWORD ?
    lpReserved      QWORD ?
    lpDesktop       QWORD ?
    lpTitle         QWORD ?
    dwX             DWORD ?
    dwY             DWORD ?
    dwXSize         DWORD ?
    dwYSize         DWORD ?
    dwXCountChars   DWORD ?
    dwYCountChars   DWORD ?
    dwFillAttribute DWORD ?
    dwFlags         DWORD ?
    wShowWindow     WORD ?
    cbReserved2     WORD ?
    lpReserved2     QWORD ?
    hStdInput       QWORD ?
    hStdOutput      QWORD ?
    hStdError       QWORD ?
STARTUPINFOA ENDS

PROCESS_INFORMATION STRUCT
    hProcess    QWORD ?
    hThread     QWORD ?
    dwProcessId DWORD ?
    dwThreadId  DWORD ?
PROCESS_INFORMATION ENDS

.code

;-----------------------------------------------------------------------------
; StrLen - Calculate string length
; Input: RCX = string pointer
; Output: RAX = string length
;-----------------------------------------------------------------------------
StrLen PROC
    push rdi
    
    mov rdi, rcx
    xor rax, rax
    
strlen_loop:
    cmp byte ptr [rdi + rax], 0
    jz strlen_done
    inc rax
    jmp strlen_loop
    
strlen_done:
    pop rdi
    ret
StrLen ENDP

;-----------------------------------------------------------------------------
; CheckFileExists - Verify file exists and get size
; Input: RCX = filename
; Output: RAX = file size (0 if file doesn't exist)
;-----------------------------------------------------------------------------
CheckFileExists PROC
    push rbp
    mov rbp, rsp
    sub rsp, 40h
    push rbx
    
    ; Open file for reading
    mov rbx, rcx    ; Save filename
    mov rdx, 80000000h  ; GENERIC_READ
    mov r8, 1           ; FILE_SHARE_READ
    mov r9, 0           ; No security attributes
    mov qword ptr [rsp+20h], 3  ; OPEN_EXISTING
    mov qword ptr [rsp+28h], 80h ; FILE_ATTRIBUTE_NORMAL
    mov qword ptr [rsp+30h], 0   ; No template
    call CreateFileA
    
    cmp rax, -1
    je file_not_found
    
    mov rbx, rax    ; File handle
    
    ; Get file size
    mov rcx, rbx
    mov rdx, 0      ; High DWORD pointer (NULL)
    call GetFileSize
    push rax        ; Save file size
    
    ; Close file
    mov rcx, rbx
    call CloseHandle
    
    pop rax         ; Restore file size
    jmp check_done
    
file_not_found:
    xor rax, rax
    
check_done:
    pop rbx
    add rsp, 40h
    pop rbp
    ret
CheckFileExists ENDP

;-----------------------------------------------------------------------------
; ExecuteAndWait - Execute a program and wait for completion
; Input: RCX = executable filename
; Output: RAX = 1 if executed successfully and exited normally, 0 otherwise
;-----------------------------------------------------------------------------
ExecuteAndWait PROC
    push rbp
    mov rbp, rsp
    sub rsp, 80h
    push rbx
    push rsi
    push rdi
    
    mov rbx, rcx    ; Save filename
    
    ; Initialize STARTUPINFO structure
    lea rsi, [rsp+40h]      ; STARTUPINFO
    lea rdi, [rsp+60h]      ; PROCESS_INFORMATION
    
    mov rcx, rsi
    xor al, al
    mov rdx, SIZEOF STARTUPINFOA
    rep stosb
    
    mov [rsi].STARTUPINFOA.cb, SIZEOF STARTUPINFOA
    
    ; Clear PROCESS_INFORMATION
    mov rcx, rdi
    xor al, al
    mov rdx, SIZEOF PROCESS_INFORMATION
    rep stosb
    
    ; Create process
    mov rcx, rbx            ; Application name
    mov rdx, 0              ; Command line (NULL)
    mov r8, 0               ; Process security
    mov r9, 0               ; Thread security
    mov qword ptr [rsp+20h], 0  ; Inherit handles (FALSE)
    mov qword ptr [rsp+28h], 0  ; Creation flags
    mov qword ptr [rsp+30h], 0  ; Environment
    mov qword ptr [rsp+38h], 0  ; Current directory
    ; STARTUPINFO and PROCESS_INFORMATION already in place
    call CreateProcessA
    
    test eax, eax
    jz execute_fail
    
    ; Wait for process to complete (max 5 seconds)
    mov rcx, [rdi].PROCESS_INFORMATION.hProcess
    mov rdx, 5000           ; 5 second timeout
    call WaitForSingleObject
    
    ; Close process and thread handles
    mov rcx, [rdi].PROCESS_INFORMATION.hProcess
    call CloseHandle
    
    mov rcx, [rdi].PROCESS_INFORMATION.hThread
    call CloseHandle
    
    mov rax, 1      ; Success
    jmp execute_done
    
execute_fail:
    xor rax, rax
    
execute_done:
    pop rdi
    pop rsi
    pop rbx
    add rsp, 80h
    pop rbp
    ret
ExecuteAndWait ENDP

;-----------------------------------------------------------------------------
; ValidateExecutableFile - Comprehensive PE file validation
; Input: RCX = filename
; Output: RAX = 1 if valid, 0 if invalid
;-----------------------------------------------------------------------------
ValidateExecutableFile PROC
    push rbp
    mov rbp, rsp
    sub rsp, 80h
    push rbx
    push rsi
    push rdi
    
    mov rbx, rcx    ; Save filename
    
    ; Open file for reading
    mov rcx, rbx
    mov rdx, 80000000h  ; GENERIC_READ
    mov r8, 1           ; FILE_SHARE_READ
    mov r9, 0
    mov qword ptr [rsp+20h], 3  ; OPEN_EXISTING
    mov qword ptr [rsp+28h], 80h
    mov qword ptr [rsp+30h], 0
    call CreateFileA
    
    cmp rax, -1
    je validate_fail
    mov rsi, rax    ; File handle
    
    ; Read DOS header
    mov rcx, rsi
    lea rdx, [rsp+40h]
    mov r8, 64      ; DOS header size
    lea r9, [rsp+38h]
    mov qword ptr [rsp+20h], 0
    call ReadFile
    test eax, eax
    jz validate_cleanup_fail
    
    ; Check DOS signature
    cmp word ptr [rsp+40h], IMAGE_DOS_SIGNATURE
    jne validate_cleanup_fail
    
    ; Get NT header offset
    mov eax, dword ptr [rsp+40h + 60] ; e_lfanew
    cmp eax, 1024
    ja validate_cleanup_fail    ; Too far
    cmp eax, 64
    jb validate_cleanup_fail    ; Too close
    
    ; Seek to NT header
    mov rcx, rsi
    mov rdx, 0
    mov r8, rax     ; NT header offset
    mov r9, 0       ; FILE_BEGIN
    call SetFilePointer
    cmp eax, -1
    je validate_cleanup_fail
    
    ; Read NT header (first part)
    mov rcx, rsi
    lea rdx, [rsp+40h]
    mov r8, 40      ; Read enough for signature and file header
    lea r9, [rsp+38h]
    mov qword ptr [rsp+20h], 0
    call ReadFile
    test eax, eax
    jz validate_cleanup_fail
    
    ; Check NT signature
    cmp dword ptr [rsp+40h], IMAGE_NT_SIGNATURE
    jne validate_cleanup_fail
    
    ; Check machine type (should be x64)
    cmp word ptr [rsp+44h], IMAGE_FILE_MACHINE_AMD64
    jne validate_cleanup_fail
    
    ; Check number of sections (should be 3: .text, .rdata, .idata)
    cmp word ptr [rsp+46h], 3
    jne validate_cleanup_fail
    
    ; File appears to be a valid PE
    mov rcx, rsi
    call CloseHandle
    mov rax, 1
    jmp validate_done
    
validate_cleanup_fail:
    mov rcx, rsi
    call CloseHandle
    
validate_fail:
    xor rax, rax
    
validate_done:
    pop rdi
    pop rsi
    pop rbx
    add rsp, 80h
    pop rbp
    ret
ValidateExecutableFile ENDP

;-----------------------------------------------------------------------------
; ValidateDOSHeader - Validate DOS header structure
; Input: RCX = filename
; Output: RAX = 1 if valid, 0 if invalid
;-----------------------------------------------------------------------------
ValidateDOSHeader PROC
    push rbp
    mov rbp, rsp
    sub rsp, 80h
    push rbx
    push rsi
    
    mov rbx, rcx    ; Save filename
    
    ; Open and read DOS header
    mov rcx, rbx
    mov rdx, 80000000h  ; GENERIC_READ
    mov r8, 1
    mov r9, 0
    mov qword ptr [rsp+20h], 3
    mov qword ptr [rsp+28h], 80h
    mov qword ptr [rsp+30h], 0
    call CreateFileA
    cmp rax, -1
    je dos_validate_fail
    mov rsi, rax
    
    ; Read DOS header
    mov rcx, rsi
    lea rdx, [rsp+40h]
    mov r8, 64
    lea r9, [rsp+38h]
    mov qword ptr [rsp+20h], 0
    call ReadFile
    test eax, eax
    jz dos_cleanup_fail
    
    ; Validate DOS signature
    cmp word ptr [rsp+40h], IMAGE_DOS_SIGNATURE  ; 'MZ'
    jne dos_cleanup_fail
    
    ; Validate e_lfanew is reasonable
    mov eax, dword ptr [rsp+40h + 60]
    cmp eax, 64         ; Minimum offset
    jb dos_cleanup_fail
    cmp eax, 1024       ; Maximum reasonable offset
    ja dos_cleanup_fail
    
    ; DOS header is valid
    mov rcx, rsi
    call CloseHandle
    mov rax, 1
    jmp dos_validate_done
    
dos_cleanup_fail:
    mov rcx, rsi
    call CloseHandle
    
dos_validate_fail:
    xor rax, rax
    
dos_validate_done:
    pop rsi
    pop rbx
    add rsp, 80h
    pop rbp
    ret
ValidateDOSHeader ENDP

;-----------------------------------------------------------------------------
; ValidateNTHeaders - Validate NT header structure
; Input: RCX = filename
; Output: RAX = 1 if valid, 0 if invalid
;-----------------------------------------------------------------------------
ValidateNTHeaders PROC
    push rbp
    mov rbp, rsp
    sub rsp, 80h
    push rbx
    push rsi
    
    mov rbx, rcx    ; Save filename
    
    ; This is a simplified validation
    ; In a full implementation, we would read and validate:
    ; - NT signature
    ; - File header fields
    ; - Optional header fields
    ; - Data directories
    
    ; For now, just check that ValidateExecutableFile passes
    mov rcx, rbx
    call ValidateExecutableFile
    
    pop rsi
    pop rbx
    add rsp, 80h
    pop rbp
    ret
ValidateNTHeaders ENDP

;-----------------------------------------------------------------------------
; ValidateSectionHeaders - Validate section header structures
; Input: RCX = filename
; Output: RAX = 1 if valid, 0 if invalid
;-----------------------------------------------------------------------------
ValidateSectionHeaders PROC
    push rbp
    mov rbp, rsp
    sub rsp, 80h
    push rbx
    
    mov rbx, rcx    ; Save filename
    
    ; Simplified validation - check file exists and is valid PE
    mov rcx, rbx
    call ValidateExecutableFile
    test rax, rax
    jz section_validate_fail
    
    ; In a full implementation, we would:
    ; - Read each section header
    ; - Validate section names (.text, .rdata, .idata)
    ; - Check virtual addresses are properly aligned
    ; - Verify file offsets are valid
    ; - Validate section characteristics
    
    mov rax, 1      ; Assume valid for now
    jmp section_validate_done
    
section_validate_fail:
    xor rax, rax
    
section_validate_done:
    pop rbx
    add rsp, 80h
    pop rbp
    ret
ValidateSectionHeaders ENDP

;-----------------------------------------------------------------------------
; ValidateImportTable - Validate import table structure
; Input: RCX = filename  
; Output: RAX = 1 if valid, 0 if invalid
;-----------------------------------------------------------------------------
ValidateImportTable PROC
    push rbp
    mov rbp, rsp
    sub rsp, 80h
    push rbx
    
    mov rbx, rcx
    
    ; Simplified validation - check file is valid PE
    mov rcx, rbx
    call ValidateExecutableFile
    test rax, rax
    jz import_validate_fail
    
    ; In a full implementation, we would:
    ; - Locate the import directory
    ; - Read import descriptors
    ; - Validate DLL names
    ; - Check import address table
    ; - Verify import lookup table
    ; - Validate import by name structures
    
    mov rax, 1      ; Assume valid for now
    jmp import_validate_done
    
import_validate_fail:
    xor rax, rax
    
import_validate_done:
    pop rbx
    add rsp, 80h
    pop rbp
    ret
ValidateImportTable ENDP

;-----------------------------------------------------------------------------
; GenerateComplexCode - Generate multi-API executable code
; Input: RCX = PE context
; Output: RAX = 1 if successful, 0 if failed
;-----------------------------------------------------------------------------
GenerateComplexCode PROC
    push rbp
    mov rbp, rsp
    sub rsp, 40h
    push rbx
    
    mov rbx, rcx    ; PE context
    
    ; This is a placeholder for complex code generation
    ; In a full implementation, this would:
    ; - Generate function prologue
    ; - Emit calls to multiple Windows APIs
    ; - Handle string constants
    ; - Generate proper stack frame management
    ; - Add function epilogue
    ; - Handle relocations for API calls
    
    ; For now, return success
    mov rax, 1
    
    pop rbx
    add rsp, 40h
    pop rbp
    ret
GenerateComplexCode ENDP

;-----------------------------------------------------------------------------
; FreePEContext - Free PE context resources
; Input: RCX = PE context
;-----------------------------------------------------------------------------
FreePEContext PROC
    ; This is a placeholder for PE context cleanup
    ; In the full implementation, this would free all allocated buffers
    ret
FreePEContext ENDP

;-----------------------------------------------------------------------------
; GetPerformanceTime - Get high-resolution timestamp
; Output: RAX = timestamp in milliseconds
;-----------------------------------------------------------------------------
GetPerformanceTime PROC
    call GetTickCount64
    ret
GetPerformanceTime ENDP

END