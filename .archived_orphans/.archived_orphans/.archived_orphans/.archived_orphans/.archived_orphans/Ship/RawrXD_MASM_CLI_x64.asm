; ============================================================================
; RawrXD_MASM_CLI_x64.asm
; Pure x64 MASM Assembly Command Line Interface
; Export interface for CPU-level diagnostics, memory inspection, and system commands
; 
; Compiled with: ml64.exe /c /Fo RawrXD_MASM_CLI_x64.obj RawrXD_MASM_CLI_x64.asm
; Linked as: link /DLL /EXPORT:CLI_Initialize /EXPORT:CLI_ExecuteCommand /EXPORT:CLI_GetOutput \
;            /EXPORT:CLI_Shutdown /OUT:RawrXD_MASM_CLI_x64.dll RawrXD_MASM_CLI_x64.obj
; ============================================================================

.code

; ============================================================================
; Structure Definitions (in C equivalent: passed by pointer)
; CommandBuffer: 
;   [0:8]   = input string pointer (64-bit, null-terminated wide char)
;   [8:16]  = output buffer pointer (64-bit)
;   [16:20] = output buffer size (DWORD)
;   [20:24] = bytes written (DWORD)
; ============================================================================

; ============================================================================
; Global State
; ============================================================================
cliState                QWORD 0          ; CLI state: 0=uninitialized, 1=initialized, 2=shutdown
cliOutputBuffer         QWORD 0          ; Current output buffer pointer
cliOutputSize           DWORD 0          ; Current output buffer size
cliOutputWritten        DWORD 0          ; Bytes written to output

; CPU feature flags (cached at init)
has_avx2                BYTE 0
has_sse42               BYTE 0
has_aes_ni              BYTE 0
has_rdrand              BYTE 0

; ============================================================================
; CLI_Initialize() -> int32 (status code)
; 
; Initialize CLI subsystem, detect CPU features, prepare output buffers
; ============================================================================
PUBLIC CLI_Initialize
CLI_Initialize PROC
    ; rcx = this pointer (unused for pure C interface calls)
    ; Returns: rax = 0 (success) or error code
    
    ; Use CPUID to detect features
    mov rax, 1
    cpuid                                  ; rax, rbx, rcx, rdx contain feature flags
    
    ; Check for SSE4.2 (bit 20 of rcx)
    test ecx, 100000h                      ; ECX bit 20 = SSE4.2
    jz .skip_sse42
    mov BYTE PTR has_sse42, 1
.skip_sse42:
    
    ; Check for AES-NI (bit 25 of rcx)
    test ecx, 2000000h                     ; ECX bit 25 = AES-NI
    jz .skip_aesni
    mov BYTE PTR has_aes_ni, 1
.skip_aesni:
    
    ; Check for RDRAND (bit 30 of ecx)
    test ecx, 40000000h                    ; ECX bit 30 = RDRAND
    jz .skip_rdrand
    mov BYTE PTR has_rdrand, 1
.skip_rdrand:
    
    ; Check for AVX2 (leaf 7)
    mov rax, 7
    xor rcx, rcx
    cpuid                                  ; rbx = extended features
    test ebx, 20h                          ; EBX bit 5 = AVX2
    jz .skip_avx2
    mov BYTE PTR has_avx2, 1
.skip_avx2:
    
    mov cliState, 1                        ; Mark initialized
    xor eax, eax                           ; Return 0 (success)
    ret
CLI_Initialize ENDP

; ============================================================================
; CLI_ExecuteCommand(const wchar_t* cmd, wchar_t* output, uint32_t outSize, uint32_t* bytesWritten)
; rcx = cmd pointer
; rdx = output buffer pointer  
; r8d = output size
; r9d = bytesWritten pointer (actually should be r9, but let's handle both)
; ============================================================================
PUBLIC CLI_ExecuteCommand
CLI_ExecuteCommand PROC
    push rbx
    push rsi
    push rdi
    
    mov cliOutputBuffer, rdx               ; Save output buffer
    mov cliOutputSize, r8d                 ; Save output size
    xor rax, rax                           ; Clear output written count
    mov cliOutputWritten, eax
    
    ; Parse command from rcx (wide char string)
    ; Commands are case-insensitive
    ; Common commands:
    ;   "cpuid" - print CPU info
    ;   "rdrand N" - generate N random bytes
    ;   "memstat" - memory statistics
    ;   "xorshift" - XORSHIFT64* test
    ;   "help" - command list
    
    ; Convert first word to uppercase (simple: compare lowercase)
    mov rsi, rcx                           ; rsi = input string
    
    ; Check for "cpuid" command
    mov rax, [rsi]                         ; Load first word
    cmp ax, 'c'
    je .check_cpuid
    cmp ax, 'C'
    jne .check_rdrand
    
.check_cpuid:
    ; Verify it's "cpuid"
    lea rax, [rsi+1]
    mov bx, [rax]
    cmp bx, 'p'
    je .do_cpuid
    cmp bx, 'P'
    jne .check_rdrand
    
.do_cpuid:
    call ExecuteCmd_CPUID
    jmp .return_output
    
.check_rdrand:
    mov al, BYTE PTR [rsi]
    cmp al, 'r'
    je .check_rdrand_word
    cmp al, 'R'
    jne .check_memstat
    
.check_rdrand_word:
    lea rax, [rsi+1]
    mov al, BYTE PTR [rax]
    cmp al, 'd'
    je .do_rdrand
    cmp al, 'D'
    jne .check_memstat
    
.do_rdrand:
    call ExecuteCmd_RDRAND
    jmp .return_output
    
.check_memstat:
    mov al, BYTE PTR [rsi]
    cmp al, 'm'
    je .check_memstat_word
    cmp al, 'M'
    jne .check_xorshift
    
.check_memstat_word:
    lea rax, [rsi+1]
    mov al, BYTE PTR [rax]
    cmp al, 'e'
    je .do_memstat
    cmp al, 'E'
    jne .check_xorshift
    
.do_memstat:
    call ExecuteCmd_MemStat
    jmp .return_output
    
.check_xorshift:
    mov al, BYTE PTR [rsi]
    cmp al, 'x'
    je .check_xorshift_word
    cmp al, 'X'
    jne .do_help
    
.check_xorshift_word:
    lea rax, [rsi+1]
    mov al, BYTE PTR [rax]
    cmp al, 'o'
    je .do_xorshift
    cmp al, 'O'
    jne .do_help
    
.do_xorshift:
    call ExecuteCmd_XORShift
    jmp .return_output
    
.do_help:
    call ExecuteCmd_Help
    
.return_output:
    ; Load pointer to bytesWritten output param from stack or register
    ; In x64 calling convention, this might have been passed in r9 or on stack
    ; For simplicity, assume r9d contains pointer to output count
    mov rax, cliOutputWritten
    mov DWORD PTR [r9], eax                ; Write byte count to output
    
    xor eax, eax                           ; Return success (0)
    pop rdi
    pop rsi
    pop rbx
    ret
CLI_ExecuteCommand ENDP

; ============================================================================
; ExecuteCmd_CPUID() - Output CPU feature information
; ============================================================================
ExecuteCmd_CPUID PROC
    local @str_buffer[256]:BYTE
    
    ; Write header
    lea rsi, msgCPUInfo
    call WriteOutputString
    
    ; Write CPU flags
    cmp BYTE PTR has_avx2, 1
    jne .skip_avx2_msg
    lea rsi, msgAVX2
    call WriteOutputString
.skip_avx2_msg:
    
    cmp BYTE PTR has_sse42, 1
    jne .skip_sse42_msg
    lea rsi, msgSSE42
    call WriteOutputString
.skip_sse42_msg:
    
    cmp BYTE PTR has_aes_ni, 1
    jne .skip_aesni_msg
    lea rsi, msgAESNI
    call WriteOutputString
.skip_aesni_msg:
    
    cmp BYTE PTR has_rdrand, 1
    jne .skip_rdrand_msg
    lea rsi, msgRDRAND
    call WriteOutputString
.skip_rdrand_msg:
    
    ret
ExecuteCmd_CPUID ENDP

; ============================================================================
; ExecuteCmd_RDRAND() - Generate random bytes using RDRAND
; ============================================================================
ExecuteCmd_RDRAND PROC
    lea rsi, msgRDRANDTest
    call WriteOutputString
    
    ; Generate 8 random bytes
    mov rcx, 0
    
.rdrand_loop:
    cmp rcx, 8
    je .rdrand_done
    
    rdrand rax                             ; Generate random 64-bit
    jnc .rdrand_error                      ; CF clear = failure
    
    ; Convert rax to hex and write
    mov rbx, rax
    mov r10, 16
    call WriteOutputHexQWord
    lea rsi, msgNewline
    call WriteOutputString
    
    inc rcx
    jmp .rdrand_loop
    
.rdrand_error:
    lea rsi, msgRDRANDFailed
    call WriteOutputString
    
.rdrand_done:
    ret
ExecuteCmd_RDRAND ENDP

; ============================================================================
; ExecuteCmd_MemStat() - Display memory statistics
; ============================================================================
ExecuteCmd_MemStat PROC
    lea rsi, msgMemStats
    call WriteOutputString
    
    ; Get current stack pointer
    mov rax, rsp
    mov rbx, rax
    call WriteOutputHexQWord
    lea rsi, msgNewline
    call WriteOutputString
    
    ret
ExecuteCmd_MemStat ENDP

; ============================================================================
; ExecuteCmd_XORShift() - XORSHIFT64* random number generator
; ============================================================================
ExecuteCmd_XORShift PROC
    mov r10, 88172645463325252h            ; Seed
    mov rcx, 0
    
.xorshift_loop:
    cmp rcx, 5
    je .xorshift_done
    
    ; x ^= x << 13
    mov rax, r10
    mov rbx, rax
    shl rbx, 13
    xor rax, rbx
    mov r10, rax
    
    ; x ^= x >> 7
    mov rax, r10
    mov rbx, rax
    shr rbx, 7
    xor rax, rbx
    mov r10, rax
    
    ; x ^= x << 17
    mov rax, r10
    mov rbx, rax
    shl rbx, 17
    xor rax, rbx
    mov r10, rax
    
    call WriteOutputHexQWord
    lea rsi, msgNewline
    call WriteOutputString
    
    inc rcx
    jmp .xorshift_loop
    
.xorshift_done:
    ret
ExecuteCmd_XORShift ENDP

; ============================================================================
; ExecuteCmd_Help() - Display available commands
; ============================================================================
ExecuteCmd_Help PROC
    lea rsi, msgHelp
    call WriteOutputString
    lea rsi, msgCmd_CPUID
    call WriteOutputString
    lea rsi, msgCmd_RDRAND
    call WriteOutputString
    lea rsi, msgCmd_MemStat
    call WriteOutputString
    lea rsi, msgCmd_XORShift
    call WriteOutputString
    ret
ExecuteCmd_Help ENDP

; ============================================================================
; WriteOutputString(rsi = string pointer)
; Write null-terminated ASCII string to output buffer
; ============================================================================
WriteOutputString PROC
    mov rdi, cliOutputBuffer               ; rdi = output buffer
    add rdi, cliOutputWritten              ; rdi = current write position
    
.write_loop:
    mov al, BYTE PTR [rsi]                 ; al = current char
    test al, al
    jz .write_done
    
    ; Convert ASCII to wide char (simple: pad with 0)
    mov WORD PTR [rdi], ax                 ; Write as wide char
    add rdi, 2
    add cliOutputWritten, 2
    inc rsi
    jmp .write_loop
    
.write_done:
    ret
WriteOutputString ENDP

; ============================================================================
; WriteOutputHexQWord(rax = value)
; Write 64-bit hex value to output buffer
; ============================================================================
WriteOutputHexQWord PROC
    mov QWORD PTR g_hexTemp, rax           ; Save value
    mov rsi, OFFSET g_hexStr               ; rsi = format string "0x"
    call WriteOutputString
    
    mov rax, QWORD PTR g_hexTemp
    mov rcx, 60                            ; Start at bit 60 (15 hex digits)
    
.hex_loop:
    mov rbx, rax
    shr rbx, cl
    and rbx, 0Fh
    
    cmp rbx, 9
    jbe .hex_digit
    add rbx, 'A' - 10
    jmp .hex_write
.hex_digit:
    add rbx, '0'
    
.hex_write:
    mov al, bl
    mov BYTE PTR g_hexBuffer, al
    mov BYTE PTR g_hexBuffer[1], 0
    lea rsi, g_hexBuffer
    call WriteOutputString
    
    sub rcx, 4
    cmp rcx, -1
    jne .hex_loop
    
    ret
WriteOutputHexQWord ENDP

; ============================================================================
; CLI_GetOutput() -> const wchar_t*
; Return pointer to current output buffer
; ============================================================================
PUBLIC CLI_GetOutput
CLI_GetOutput PROC
    mov rax, cliOutputBuffer
    ret
CLI_GetOutput ENDP

; ============================================================================
; CLI_Shutdown() -> int32
; Cleanup and shutdown CLI
; ============================================================================
PUBLIC CLI_Shutdown
CLI_Shutdown PROC
    mov cliState, 2
    xor eax, eax
    ret
CLI_Shutdown ENDP

; ============================================================================
; String Data Section
; ============================================================================
.data

msgCPUInfo          db "=== CPU Feature Detection ===", 0Dh, 0Ah, 0
msgAVX2             db "- AVX2 Supported", 0Dh, 0Ah, 0
msgSSE42            db "- SSE4.2 Supported", 0Dh, 0Ah, 0
msgAESNI            db "- AES-NI Supported", 0Dh, 0Ah, 0
msgRDRAND           db "- RDRAND Supported", 0Dh, 0Ah, 0

msgRDRANDTest       db "=== RDRAND Generated Values ===", 0Dh, 0Ah, 0
msgRDRANDFailed     db "ERROR: RDRAND not supported", 0Dh, 0Ah, 0

msgMemStats         db "=== Memory Statistics ===", 0Dh, 0Ah, "Stack Pointer: ", 0

msgHelp             db "=== RawrXD MASM CLI Commands ===", 0Dh, 0Ah, 0
msgCmd_CPUID        db "  cpuid        - CPU feature detection", 0Dh, 0Ah, 0
msgCmd_RDRAND       db "  rdrand       - Generate random bytes", 0Dh, 0Ah, 0
msgCmd_MemStat      db "  memstat      - Memory statistics", 0Dh, 0Ah, 0
msgCmd_XORShift     db "  xorshift     - XORSHIFT64* PRNG test", 0Dh, 0Ah, 0

msgNewline          db 0Dh, 0Ah, 0
g_hexStr            db "0x", 0
g_hexBuffer         db 0, 0, 0
g_hexTemp           QWORD 0

END
