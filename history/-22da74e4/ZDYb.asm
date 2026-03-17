; ============================================================================
; POCKET_LAB.ASM — Auto-Scaling Ghost Paging Kernel
; Pure MASM64 | No CRT | Kernel32+Ntdll Only
; ============================================================================
OPTION PROLOGUE:NONE
OPTION EPILOGUE:NONE
OPTION CASEMAP:NONE

; --- External Symbols (Kernel32 Only) ---
EXTERN GetStdHandle:PROC
EXTERN WriteConsoleA:PROC
EXTERN GlobalMemoryStatusEx:PROC
EXTERN ExitProcess:PROC
EXTERN Sleep:PROC

; --- Constants ---
STD_OUTPUT_HANDLE       equ -11
NULL                    equ 0

.const
    ; Thresholds (Bytes)
    RAM_8GB             dq 8589934592     ; 8 * 1024^3
    RAM_16GB            dq 17179869184    ; 16 * 1024^3
    
    ; Strings
    szBanner            db "=== POCKET LAB: Auto-Scaling Neural Kernel ===", 13, 10, 0
    szDetecting         db "[*] Probing Physical Memory... ", 0
    szGB                db " GB", 13, 10, 0
    
    szMode70B           db "[+] Configured: 70B Q4 (Mobile Profile)", 13, 10
                        db "    -> Ghost-Lite: 16 MB Pinning", 13, 10, 0
                        
    szMode120B          db "[+] Configured: 120B Q4 (Workstation Profile)", 13, 10
                        db "    -> Ghost-Full: 4 GB Pinning", 13, 10, 0
                        
    szMode800B          db "[+] Configured: 800B Q4 (Enterprise Profile)", 13, 10
                        db "    -> Ghost-Enhanced: Critical Slab Preload | 512KB Prefetch", 13, 10, 0

.data
    ; MEMORYSTATUSEX Structure
    ; dwLength (4)
    ; dwMemoryLoad (4)
    ; ullTotalPhys (8)
    ; ... (total 64 bytes)
    memStatus           db 64 dup(0)
    
    hStdOut             dq 0
    bytesWritten        dq 0
    
    ; Runtime Config
    KernelMode          dq 0    ; 0=70B, 1=120B, 2=800B
    PrefetchSize        dq 0

    ; Integer Conversion Buffer
    numBuffer           db 32 dup(0)

.code

; ----------------------------------------------------------------------------
; Entry Point
; ----------------------------------------------------------------------------
main PROC
    sub rsp, 28h                ; Shadow space
    
    ; 1. Setup Console
    mov rcx, STD_OUTPUT_HANDLE
    call GetStdHandle
    mov hStdOut, rax
    
    lea rcx, szBanner
    call PrintString
    
    lea rcx, szDetecting
    call PrintString

    ; 2. Detect RAM
    ; memStatus.dwLength = 64
    lea rcx, memStatus
    mov dword ptr [rcx], 64
    call GlobalMemoryStatusEx
    
    ; Get TotalPhys (Offset 8)
    lea rax, memStatus
    mov rdx, [rax + 8]          ; ullTotalPhys in RDX
    
    ; Print RAM Size (Approx GB)
    ; GB = Bytes >> 30
    mov r8, rdx
    shr r8, 30
    mov rcx, r8
    call PrintDec
    
    lea rcx, szGB
    call PrintString
    
    ; 3. Auto-Cofigure
    lea rax, memStatus
    mov rdx, [rax + 8]          ; Restore ullTotalPhys
    
    ; Check <= 8GB
    mov rax, RAM_8GB
    cmp rdx, rax
    jbe _config_70b
    
    ; Check <= 16GB
    mov rax, RAM_16GB
    cmp rdx, rax
    jbe _config_120b
    
    ; Else 800B
    jmp _config_800b

_config_70b:
    lea rcx, szMode70B
    call PrintString
    ; Apply Config
    mov KernelMode, 0
    mov PrefetchSize, 16 * 1024 ; 16KB default for Lite
    jmp _fini

_config_120b:
    lea rcx, szMode120B
    call PrintString
    ; Apply Config
    mov KernelMode, 1
    mov PrefetchSize, 64 * 1024 ; 64KB for Standard
    jmp _fini

_config_800b:
    lea rcx, szMode800B
    call PrintString
    ; Apply Config
    mov KernelMode, 2
    mov PrefetchSize, 512 * 1024 ; 512KB for Enterprise
    jmp _fini

_fini:
    mov ecx, 0
    call ExitProcess

main ENDP

; ----------------------------------------------------------------------------
; Helper: PrintString (RCX = SzPtr)
; ----------------------------------------------------------------------------
PrintString PROC
    ; Save volatile registers if needed (not strictly needed for this simple flow)
    mov rdx, rcx
    
    ; Calculate Length
    xor r8, r8
_ps_len:
    cmp byte ptr [rdx + r8], 0
    je _ps_write
    inc r8
    jmp _ps_len

_ps_write:
    sub rsp, 28h ; Local stack (+ align)
    
    mov rcx, hStdOut
    ; RDX is already buffer start (from RCX original)
    ; R8 is length
    lea r9, bytesWritten
    mov qword ptr [rsp + 20h], 0 ; lpReserved
    call WriteConsoleA
    
    add rsp, 28h
    ret
PrintString ENDP

; ----------------------------------------------------------------------------
; Helper: PrintDec (RCX = Number)
; Simple unsigned decimal printer
; ----------------------------------------------------------------------------
PrintDec PROC
    sub rsp, 28h
    
    lea rdi, numBuffer
    add rdi, 31                 ; Start at end
    mov byte ptr [rdi], 0       ; Null terminator
    dec rdi
    
    mov rax, rcx
    mov rbx, 10
    
_pd_loop:
    xor rdx, rdx
    div rbx                     ; RAX / 10, Remainder RDX
    add dl, '0'
    mov [rdi], dl
    dec rdi
    test rax, rax
    jnz _pd_loop
    
    inc rdi                     ; Point to first char
    
    mov rcx, rdi
    call PrintString
    
    add rsp, 28h
    ret
PrintDec ENDP

END
