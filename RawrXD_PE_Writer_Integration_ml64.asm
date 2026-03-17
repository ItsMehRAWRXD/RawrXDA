; ===============================================================================
; RawrXD PE32+ Writer — Integration & Orchestration Layer
; ===============================================================================
; Purpose: Orchestrates complete PE32+ binary generation without link.exe
; Provides high-level API for Amphibious build system integration
; Handles compilation of object sections → final PE32+ executable
; ===============================================================================

; Win32 API imports for PE writer operations
EXTERN GetProcessHeap:PROC
EXTERN HeapAlloc:PROC
EXTERN HeapFree:PROC
EXTERN CreateFileA:PROC
EXTERN WriteFile:PROC
EXTERN CloseHandle:PROC
EXTERN FlushFileBuffers:PROC

; Core PE writer module references
EXTERN RawrXD_PE_WriteDOSHeader_ml64:PROC
EXTERN RawrXD_PE_WriteNTHeaders_ml64:PROC
EXTERN RawrXD_PE_WriteSectionHeaders_ml64:PROC
EXTERN RawrXD_PE_GenerateImportTable_ml64:PROC
EXTERN RawrXD_PE_GenerateRelocations_ml64:PROC
EXTERN RawrXD_PE_ValidateReproduce_ml64:PROC

.data?

; Globals for PE writer orchestration
g_TextSectionData    qword ?     ; Pointer to .text machine code
g_DataSectionData    qword ?     ; Pointer to .data constants/globals
g_TextSectionSize    qword ?     ; Size of .text section
g_DataSectionSize    qword ?     ; Size of .data section
g_MainEntryPointRVA  qword ?     ; RVA of main entry point
g_SubsystemValue     qword ?     ; 2 = CONSOLE, 3 = GUI
g_OutputFilePath     qword ?     ; Path to output PE binary
g_LastPEChecksum     qword ?     ; Last generated PE checksum

.code

; ===============================================================================
; PROCEDURE: RawrXD_PE_InitializePEWriter_ml64
; ===============================================================================
; Purpose: Initialize PE writer with section configuration
; Input:
;   rcx = subsystem type (2 = CONSOLE, 3 = GUI)
;   rdx = main entry point RVA
;   r8 = text section machine code pointer
;   r9 = text section size
;   [rsp+32] = data section pointer
;   [rsp+40] = data section size
; Output:
;   rax = 1 on success, 0 on invalid parameters
; Stack: 32 bytes local storage
; ===============================================================================
RawrXD_PE_InitializePEWriter_ml64 PROC FRAME
    sub rsp, 20h
    .allocstack 20h
    .endprolog
    
    ; Validate subsystem value (2 or 3)
    cmp rcx, 2
    je VALID_SUBSYSTEM_CUI
    cmp rcx, 3
    je VALID_SUBSYSTEM_GUI
    xor eax, eax                    ; Return 0 (failure)
    add rsp, 20h
    ret
    
VALID_SUBSYSTEM_CUI:
VALID_SUBSYSTEM_GUI:
    ; Store configuration in globals
    mov g_SubsystemValue, rcx       ; Subsystem
    mov g_MainEntryPointRVA, rdx    ; Entry point RVA
    mov g_TextSectionData, r8       ; .text pointer
    mov g_TextSectionSize, r9       ; .text size
    
    ; Load data section parameters from stack
    mov rax, [rsp + 38h]            ; Data pointer at [rsp+32]
    mov g_DataSectionData, rax
    
    mov rax, [rsp + 40h]            ; Data size at [rsp+40]
    mov g_DataSectionSize, rax
    
    mov eax, 1                      ; Return 1 (success)
    add rsp, 20h
    ret
RawrXD_PE_InitializePEWriter_ml64 ENDP

; ===============================================================================
; PROCEDURE: RawrXD_PE_WritePEBinary_ml64
; ===============================================================================
; Purpose: Generate complete PE32+ binary from initialized sections
; Output:
;   rcx = output PE binary buffer pointer (allocated internally)
;   rdx = output PE binary size
; Output:
;   rax = pointer to PE binary (NULL on failure)
; ===============================================================================
RawrXD_PE_WritePEBinary_ml64 PROC FRAME
    sub rsp, 60h
    .allocstack 60h
    .endprolog
    
    push rbx
    push r12
    push r13
    push r14
    push r15
    
    ; Allocate output buffer (64KB should be sufficient for Amphibious binaries)
    mov r12, 10000h                 ; 64KB buffer size
    lea r13, [g_TextSectionSize]    ; r13 = text size
    add r13, [g_DataSectionSize]    ; r13 = text + data size
    
    ; Check buffer is large enough
    cmp r13, r12
    jle BUFFER_SIZE_OK
    xor eax, eax                    ; Return NULL (failure)
    jmp WRITE_PE_DONE
    
BUFFER_SIZE_OK:
    ; Allocate buffer via HeapAlloc
    call GetProcessHeap
    test rax, rax
    jz WRITE_PE_FAIL_NO_BUFFER
    mov rcx, rax                    ; hHeap
    mov rdx, 8                      ; HEAP_ZERO_MEMORY
    mov r8, r12                     ; Buffer size (64KB)
    call HeapAlloc
    test rax, rax
    jz WRITE_PE_FAIL_NO_BUFFER
    mov rbx, rax                    ; rbx = allocated PE buffer
    mov qword ptr [rsp + 50h], rax  ; Save buffer ptr for cleanup on failure
    
    ; === Write DOS Header (64 bytes) ===
    mov rcx, rbx                    ; Output buffer
    mov rdx, 40h                    ; PE offset = 64 bytes (standard location)
    call RawrXD_PE_WriteDOSHeader_ml64
    mov r14, rax                    ; r14 = bytes written (40h)
    
    ; === Write NT Headers (264 bytes) ===
    lea rcx, [rbx + r14]            ; rcx = buffer after DOS header
    mov rdx, [g_MainEntryPointRVA]  ; rdx = entry point RVA
    mov r8, [g_SubsystemValue]      ; r8 = subsystem type
    mov r9, r12                     ; r9 = image size
    mov qword ptr [rsp + 28h], 4    ; Section count = 4 (push to stack)
    mov qword ptr [rsp + 30h], r13  ; Code size (push to stack)
    
    call RawrXD_PE_WriteNTHeaders_ml64
    add r14, rax                    ; r14 += bytes written
    
    ; === Write Section Headers (4 sections × 40 bytes = 160 bytes) ===
    lea rcx, [rbx + r14]            ; rcx = buffer after NT headers
    mov rdx, 4                      ; rdx = section count
    mov r8, 0                       ; r8 = section config (simplified - use defaults)
    
    call RawrXD_PE_WriteSectionHeaders_ml64
    add r14, rax                    ; r14 += bytes written
    
    ; === Write Section Data (.text, .data) ===
    ; .text section
    mov rcx, [g_TextSectionData]    ; rcx = source .text data
    lea rdx, [rbx + 400h]           ; rdx = destination (.text @ file offset 400h)
    mov r8, [g_TextSectionSize]     ; r8 = size to copy
    
    xor r9, r9                      ; r9 = offset
COPY_TEXT_LOOP:
    cmp r9, r8
    jge COPY_TEXT_DONE
    mov al, byte ptr [rcx + r9]
    mov byte ptr [rdx + r9], al
    inc r9
    jmp COPY_TEXT_LOOP
    
COPY_TEXT_DONE:
    mov r10, [g_TextSectionSize]    ; r10 = text size
    
    ; .data section
    mov rcx, [g_DataSectionData]    ; rcx = source .data data
    lea rdx, [rbx + 1400h]          ; rdx = destination (.data @ file offset 1400h)
    mov r8, [g_DataSectionSize]     ; r8 = size to copy
    
    xor r9, r9                      ; r9 = offset
COPY_DATA_LOOP:
    cmp r9, r8
    jge COPY_DATA_DONE
    mov al, byte ptr [rcx + r9]
    mov byte ptr [rdx + r9], al
    inc r9
    jmp COPY_DATA_LOOP
    
COPY_DATA_DONE:
    mov r11, [g_DataSectionSize]    ; r11 = data size
    
    ; === Generate Import Table (.idata section @ 2600h) ===
    lea rcx, [rbx + 2600h]          ; rcx = .idata section buffer
    mov rdx, 5000h                  ; rdx = .idata RVA
    
    call RawrXD_PE_GenerateImportTable_ml64
    mov r15, rax                    ; r15 = import table size
    
    ; === Generate Base Relocations (.reloc section @ 2400h) ===
    lea rcx, [rbx + 2400h]          ; rcx = .reloc section buffer
    mov rdx, 10                     ; rdx = relocation count estimate
    
    call RawrXD_PE_GenerateRelocations_ml64
    ; rax = relocation table size
    
    ; === Validate PE Binary ===
    mov rcx, rbx                    ; rcx = PE binary buffer
    mov rdx, r14                    ; rdx = binary size
    add rdx, r10                    ; Add text section size
    add rdx, r11                    ; Add data section size
    
    call RawrXD_PE_ValidateReproduce_ml64
    mov g_LastPEChecksum, rax       ; Store checksum for reproducibility check
    
    ; === Return PE binary pointer ===
    mov rax, rbx                    ; Return buffer pointer
    jmp WRITE_PE_DONE

WRITE_PE_FAIL:
    ; Free the allocated buffer on failure
    mov r12, qword ptr [rsp + 50h]  ; Retrieve saved buffer ptr
    test r12, r12
    jz WRITE_PE_FAIL_NO_BUFFER
    call GetProcessHeap
    test rax, rax
    jz WRITE_PE_FAIL_NO_BUFFER
    mov rcx, rax                    ; hHeap
    xor edx, edx                    ; dwFlags = 0
    mov r8, r12                     ; lpMem = saved buffer
    call HeapFree

WRITE_PE_FAIL_NO_BUFFER:
    xor eax, eax                    ; Return NULL (failure)

WRITE_PE_DONE:
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    add rsp, 60h
    ret
RawrXD_PE_WritePEBinary_ml64 ENDP

; ===============================================================================
; PROCEDURE: RawrXD_PE_WriteToFile_ml64
; ===============================================================================
; Purpose: Write generated PE binary to file on disk
; Input:
;   rcx = output file path (null-terminated string)
;   rdx = PE binary buffer pointer
;   r8 = PE binary size
; Output:
;   rax = 1 on success, 0 on failure
; ===============================================================================
RawrXD_PE_WriteToFile_ml64 PROC FRAME
    sub rsp, 40h
    .allocstack 40h
    .endprolog
    
    push rbx
    push r12
    push r13
    push r14
    
    mov rbx, rcx                    ; rbx = output path
    mov r12, rdx                    ; r12 = PE buffer
    mov r13, r8                     ; r13 = PE size
    
    ; Validate parameters
    test rbx, rbx
    jz WRITE_FILE_FAIL
    test r12, r12
    jz WRITE_FILE_FAIL
    test r13, r13
    jz WRITE_FILE_FAIL
    
    ; ----- Create output file -----
    ; CreateFileA(lpFileName, dwDesiredAccess, dwShareMode, lpSecurity,
    ;             dwCreationDisposition, dwFlags, hTemplate)
    mov rcx, rbx                    ; File path
    mov edx, 40000000h              ; GENERIC_WRITE
    xor r8d, r8d                    ; No sharing
    xor r9d, r9d                    ; No security attributes
    mov qword ptr [rsp + 20h], 2    ; CREATE_ALWAYS
    mov qword ptr [rsp + 28h], 80h  ; FILE_ATTRIBUTE_NORMAL
    mov qword ptr [rsp + 30h], 0    ; No template file
    call CreateFileA
    cmp rax, -1                     ; INVALID_HANDLE_VALUE
    je WRITE_FILE_FAIL
    mov r14, rax                    ; r14 = file handle
    
    ; ----- Write PE binary data -----
    ; Write in chunks to handle large binaries reliably
    xor ebx, ebx                    ; Total bytes written
    
WRITE_LOOP:
    mov rax, r13
    sub rax, rbx                    ; Remaining bytes
    jle WRITE_COMPLETE              ; All written
    
    ; Prepare WriteFile call
    mov rcx, r14                    ; hFile
    lea rdx, [r12 + rbx]            ; lpBuffer = PE buffer + offset
    mov r8, rax                     ; nNumberOfBytesToWrite = remaining
    ; Clamp to 32KB chunks for reliability
    cmp r8, 8000h
    jle WRITE_CHUNK_OK
    mov r8, 8000h
WRITE_CHUNK_OK:
    lea r9, [rsp + 38h]             ; lpNumberOfBytesWritten
    mov qword ptr [rsp + 20h], 0    ; lpOverlapped = NULL
    call WriteFile
    test eax, eax
    jz WRITE_CLOSE_FAIL             ; WriteFile failed
    
    ; Accumulate bytes written
    mov eax, dword ptr [rsp + 38h]
    add ebx, eax
    
    ; Continue writing if more data remains
    cmp rbx, r13
    jl WRITE_LOOP
    
WRITE_COMPLETE:
    ; ----- Flush and close file -----
    mov rcx, r14                    ; hFile
    call FlushFileBuffers
    
    mov rcx, r14                    ; hFile
    call CloseHandle
    
    mov rax, 1                      ; Return success
    jmp WRITE_FILE_DONE
    
WRITE_CLOSE_FAIL:
    ; Close handle on failure
    mov rcx, r14
    call CloseHandle
    
WRITE_FILE_FAIL:
    xor eax, eax                    ; Return 0 (failure)
    
WRITE_FILE_DONE:
    pop r14
    pop r13
    pop r12
    pop rbx
    add rsp, 40h
    ret
RawrXD_PE_WriteToFile_ml64 ENDP

; ===============================================================================
; PROCEDURE: RawrXD_PE_ValidateByteReproducibility_ml64
; ===============================================================================
; Purpose: Validate two PE binaries produce identical bytes (reproducibility check)
; Input:
;   rcx = first PE binary buffer
;   rdx = second PE binary buffer
;   r8 = binary size (must be same)
; Output:
;   rax = 1 if identical, 0 if different
; ===============================================================================
RawrXD_PE_ValidateByteReproducibility_ml64 PROC FRAME
    sub rsp, 20h
    .allocstack 20h
    .endprolog
    
    push rbx
    push r12
    
    mov rbx, rcx                    ; rbx = buffer 1
    mov r12, rdx                    ; r12 = buffer 2
    mov r9, r8                      ; r9 = size
    xor r10, r10                    ; r10 = offset
    
BYTE_COMPARE_LOOP:
    cmp r10, r9
    jge BYTE_COMPARE_DONE
    
    movzx eax, byte ptr [rbx + r10]
    movzx edx, byte ptr [r12 + r10]
    cmp eax, edx
    jne BYTE_COMPARE_MISMATCH
    
    inc r10
    jmp BYTE_COMPARE_LOOP
    
BYTE_COMPARE_MISMATCH:
    xor eax, eax                    ; Return 0 (not identical)
    jmp BYTE_COMPARE_EXIT
    
BYTE_COMPARE_DONE:
    mov eax, 1                      ; Return 1 (identical)
    
BYTE_COMPARE_EXIT:
    pop r12
    pop rbx
    add rsp, 20h
    ret
RawrXD_PE_ValidateByteReproducibility_ml64 ENDP

; ===============================================================================
; PROCEDURE: RawrXD_PE_GeneratePEChecksum_ml64
; ===============================================================================
; Purpose: Generate deterministic checksum for PE binary
; Input:
;   rcx = PE binary buffer
;   rdx = binary size
; Output:
;   rax = 64-bit checksum (XOR of all bytes)
;   rdx = secondary hash (for verification)
; ===============================================================================
RawrXD_PE_GeneratePEChecksum_ml64 PROC FRAME
    sub rsp, 20h
    .allocstack 20h
    .endprolog
    
    push rbx
    push r12
    push r13
    
    mov rbx, rcx                    ; rbx = PE buffer
    mov r12, rdx                    ; r12 = binary size
    xor r9, r9                      ; r9 = offset
    xor r10, r10                    ; r10 = XOR accumulator
    xor r13, r13                    ; r13 = secondary hash
    
CHECKSUM_LOOP:
    cmp r9, r12
    jge CHECKSUM_DONE
    
    ; Primary: simple XOR
    movzx eax, byte ptr [rbx + r9]
    xor r10, rax
    
    ; Secondary: rolling hash
    mov eax, r13d
    ror eax, 1
    xor eax, edx
    mov r13d, eax
    
    inc r9
    jmp CHECKSUM_LOOP
    
CHECKSUM_DONE:
    mov rax, r10                    ; Primary checksum
    mov rdx, r13                    ; Secondary hash
    
    pop r13
    pop r12
    pop rbx
    add rsp, 20h
    ret
RawrXD_PE_GeneratePEChecksum_ml64 ENDP

; ===============================================================================
; EXPORTS: High-level PE Writer API for Amphibious Integration
; ===============================================================================

public RawrXD_PE_InitializePEWriter_ml64
public RawrXD_PE_WritePEBinary_ml64
public RawrXD_PE_WriteToFile_ml64
public RawrXD_PE_ValidateByteReproducibility_ml64
public RawrXD_PE_GeneratePEChecksum_ml64

public g_TextSectionData
public g_DataSectionData
public g_TextSectionSize
public g_DataSectionSize
public g_MainEntryPointRVA
public g_SubsystemValue
public g_LastPEChecksum

END
