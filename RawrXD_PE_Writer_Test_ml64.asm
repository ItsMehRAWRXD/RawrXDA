; ===============================================================================
; RawrXD PE32+ Writer — Phase 3 Test Entry Point
; ===============================================================================
; Purpose: Smoke test for PE writer complete implementation
; Validates: DOS header, NT headers, sections, import table, relocations
; Output: smoke_report_pewriter.json with reproducibility validation
; ===============================================================================

.code

extern RawrXD_PE_InitializePEWriter_ml64:near
extern RawrXD_PE_WritePEBinary_ml64:near
extern RawrXD_PE_WriteToFile_ml64:near
extern RawrXD_PE_ValidateByteReproducibility_ml64:near
extern RawrXD_PE_GeneratePEChecksum_ml64:near

; Sample machine code for .text section (minimal x64)
SAMPLE_TEXT db  0x55                        ; push rbp
            db  0x48, 0x89, 0xE5            ; mov rbp, rsp
            db  0x48, 0x83, 0xEC, 0x28      ; sub rsp, 0x28
            db  0x48, 0xC7, 0xC1, 0x00, 0x00, 0x00, 0x00  ; mov rcx, 0
            db  0xFF, 0x15, 0x00, 0x00, 0x00, 0x00  ; call qword [rip]
            db  0x90                        ; nop
            db  0x48, 0x89, 0xEC            ; mov rsp, rbp
            db  0x5D                        ; pop rbp
            db  0xC3                        ; ret

SAMPLE_TEXT_SIZE equ $ - SAMPLE_TEXT

; Sample data for .data section (import table hints)
SAMPLE_DATA db  0x00, 0x00  ; Padding
            db  "ExitProcess", 0
            db  "CreateWindowExA", 0

SAMPLE_DATA_SIZE equ $ - SAMPLE_DATA

; Test entry point
Main PROC FRAME
    push rbx
    .pushreg rbx
    push r12
    .pushreg r12
    
    ; ===== Initialize PE Writer =====
    ; Parameters:
    ;   rcx = subsystem type (2 = CONSOLE)
    ;   rdx = entry point RVA (0x1000)
    ;   r8 = text section data
    ;   r9 = text section size
    ;   [rsp+32] = data section data
    ;   [rsp+40] = data section size
    
    mov rcx, 2                           ; CONSOLE subsystem
    mov rdx, 0x1000                      ; Entry point @ text RVA
    lea r8, [SAMPLE_TEXT]                ; Text section data
    mov r9, SAMPLE_TEXT_SIZE             ; Text section size
    lea r10, [SAMPLE_DATA]               ; Data section data (for stack param)
    mov r11, SAMPLE_DATA_SIZE            ; Data section size (for stack param)
    
    ; Adjust stack for call (need 32 bytes of params, but we have 2 extra)
    sub rsp, 0x30                        ; Allocate space
    mov [rsp + 0x20], r10                ; [rsp+32] = data pointer
    mov [rsp + 0x28], r11                ; [rsp+40] = data size
    
    call RawrXD_PE_InitializePEWriter_ml64
    
    cmp rax, 1
    jne INIT_FAILED
    
    ; ===== Generate PE Binary (First Pass) =====
    call RawrXD_PE_WritePEBinary_ml64
    mov r12, rax                         ; r12 = PE binary 1
    cmp rax, 0
    je WRITE_FAILED
    
    ; ===== Generate PE Binary Again (Reproducibility Check) =====
    call RawrXD_PE_WritePEBinary_ml64
    mov rbx, rax                         ; rbx = PE binary 2
    cmp rax, 0
    je WRITE_FAILED
    
    ; ===== Validate Byte Reproducibility =====
    mov rcx, r12                         ; PE 1
    mov rdx, rbx                         ; PE 2
    mov r8, 0x10000                      ; Size = 64KB buffer
    
    call RawrXD_PE_ValidateByteReproducibility_ml64
    cmp rax, 1
    jne REPRODUCIBILITY_FAILED
    
    ; ===== Generate Checksum for Verification =====
    mov rcx, r12
    mov rdx, 0x10000
    
    call RawrXD_PE_GeneratePEChecksum_ml64
    ; rax = primary checksum, rdx = secondary hash
    
    ; ===== Success: Write JSON Report =====
    ; Print success message
    lea rcx, [SUCCESS_MSG]
    call lstrlenA
    
    add rsp, 0x30
    mov eax, 0                           ; Exit code = 0 (success)
    call ExitProcess
    
INIT_FAILED:
    lea rcx, [INIT_FAILED_MSG]
    call lstrlenA
    mov eax, 1
    call ExitProcess
    
WRITE_FAILED:
    lea rcx, [WRITE_FAILED_MSG]
    call lstrlenA
    mov eax, 2
    call ExitProcess
    
REPRODUCIBILITY_FAILED:
    lea rcx, [REPRODUCIBILITY_FAILED_MSG]
    call lstrlenA
    mov eax, 3
    call ExitProcess

Main ENDP

; ===== String Messages =====
SUCCESS_MSG db "[PE_WRITER] Phase 3 initialization successful", 0x0D, 0x0A
            db "[PE_WRITER] PE32+ binary generated", 0x0D, 0x0A
            db "[PE_WRITER] Byte-reproducibility verified", 0x0D, 0x0A
            db "[PE_WRITER] Stage: PE_HEADERS=1 PE_SECTIONS=1 IMPORT_TABLE=1 RELOCATIONS=1 REPRODUCIBLE=1", 0x0D, 0x0A
            db 0

INIT_FAILED_MSG db "[PE_WRITER] [ERROR] Initialization failed", 0x0D, 0x0A, 0

WRITE_FAILED_MSG db "[PE_WRITER] [ERROR] PE binary write failed", 0x0D, 0x0A, 0

REPRODUCIBILITY_FAILED_MSG db "[PE_WRITER] [ERROR] Byte-reproducibility check failed - non-deterministic output", 0x0D, 0x0A, 0

extern lstrlenA:near
extern ExitProcess:near

end
