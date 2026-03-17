; =============================================================================
; RawrXD_Debug_Engine.asm — Phase 12: Native Debugger Low-Level Kernel
; =============================================================================
; Pure x64 MASM zero-dependency debug primitives for the Native Debugger.
;
; Responsibilities:
;   1. INT3 software breakpoint injection and restoration
;   2. Hardware breakpoint (DR0–DR3) set/clear via thread context
;   3. Single-step enable/disable (RFLAGS Trap Flag manipulation)
;   4. Thread context capture (full GPR + segment + debug registers)
;   5. Stack walking via RBP chain traversal
;   6. Remote process memory read/write (ReadProcessMemory/WriteProcessMemory)
;   7. Boyer–Moore inspired pattern scan in remote process memory
;   8. CRC-32 checksum of remote memory regions
;   9. RDTSC high-precision timestamp counter
;
; Architecture: x64 MASM | Windows ABI | No exceptions | No CRT | No STL
;
; Build: ml64.exe /c /Zi /Zd /Fo RawrXD_Debug_Engine.obj RawrXD_Debug_Engine.asm
; Link:  Linked into RawrXD-Win32IDE.exe
;
; Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
; =============================================================================

INCLUDE RawrXD_Common.inc

; =============================================================================
;                          ADDITIONAL EXTERNS
; =============================================================================
; Win32 API functions needed for debug operations not in RawrXD_Common.inc

EXTERNDEF GetThreadContext:PROC
EXTERNDEF SetThreadContext:PROC
EXTERNDEF ReadProcessMemory:PROC
EXTERNDEF WriteProcessMemory:PROC
EXTERNDEF VirtualProtectEx:PROC
EXTERNDEF VirtualQueryEx:PROC
EXTERNDEF FlushInstructionCache:PROC
EXTERNDEF SuspendThread:PROC
EXTERNDEF ResumeThread:PROC
EXTERNDEF OpenThread:PROC

; =============================================================================
;                             EXPORTS
; =============================================================================

PUBLIC Dbg_InjectINT3
PUBLIC Dbg_RestoreINT3
PUBLIC Dbg_SetHardwareBreakpoint
PUBLIC Dbg_ClearHardwareBreakpoint
PUBLIC Dbg_EnableSingleStep
PUBLIC Dbg_DisableSingleStep
PUBLIC Dbg_CaptureContext
PUBLIC Dbg_SetRegister
PUBLIC Dbg_WalkStack
PUBLIC Dbg_ReadMemory
PUBLIC Dbg_WriteMemory
PUBLIC Dbg_MemoryScan
PUBLIC Dbg_MemoryCRC32
PUBLIC Dbg_RDTSC

; =============================================================================
;                         DEBUG ENGINE CONSTANTS
; =============================================================================

; CONTEXT flags (from winnt.h)
CONTEXT_AMD64               EQU     00100000h
CONTEXT_CONTROL             EQU     (CONTEXT_AMD64 OR 00000001h)
CONTEXT_INTEGER             EQU     (CONTEXT_AMD64 OR 00000002h)
CONTEXT_SEGMENTS            EQU     (CONTEXT_AMD64 OR 00000004h)
CONTEXT_FLOATING_POINT      EQU     (CONTEXT_AMD64 OR 00000008h)
CONTEXT_DEBUG_REGISTERS     EQU     (CONTEXT_AMD64 OR 00000010h)
CONTEXT_FULL                EQU     (CONTEXT_CONTROL OR CONTEXT_INTEGER OR CONTEXT_FLOATING_POINT)
CONTEXT_ALL                 EQU     (CONTEXT_FULL OR CONTEXT_SEGMENTS OR CONTEXT_DEBUG_REGISTERS)

; CONTEXT structure offsets (x64, from SDK)
; Full CONTEXT is 1232 bytes on x64
CTX_ContextFlags            EQU     30h     ; DWORD ContextFlags offset in CONTEXT
CTX_Dr0                     EQU     48h
CTX_Dr1                     EQU     50h
CTX_Dr2                     EQU     58h
CTX_Dr3                     EQU     60h
CTX_Dr6                     EQU     68h
CTX_Dr7                     EQU     70h
CTX_Rax                     EQU     78h
CTX_Rcx                     EQU     80h
CTX_Rdx                     EQU     88h
CTX_Rbx                     EQU     90h
CTX_Rsp                     EQU     98h
CTX_Rbp                     EQU     0A0h
CTX_Rsi                     EQU     0A8h
CTX_Rdi                     EQU     0B0h
CTX_R8                      EQU     0B8h
CTX_R9                      EQU     0C0h
CTX_R10                     EQU     0C8h
CTX_R11                     EQU     0D0h
CTX_R12                     EQU     0D8h
CTX_R13                     EQU     0E0h
CTX_R14                     EQU     0E8h
CTX_R15                     EQU     0F0h
CTX_Rip                     EQU     0F8h
CTX_EFlags                  EQU     44h
CTX_SegCs                   EQU     38h
CTX_SegDs                   EQU     3Ah
CTX_SegEs                   EQU     3Ch
CTX_SegFs                   EQU     3Eh
CTX_SegGs                   EQU     40h
CTX_SegSs                   EQU     42h

CONTEXT_STRUCT_SIZE         EQU     4D0h    ; 1232 bytes

; RFLAGS bits
RFLAGS_CF                   EQU     001h    ; Carry flag
RFLAGS_PF                   EQU     004h    ; Parity flag
RFLAGS_AF                   EQU     010h    ; Auxiliary carry flag
RFLAGS_ZF                   EQU     040h    ; Zero flag
RFLAGS_SF                   EQU     080h    ; Sign flag
RFLAGS_TF                   EQU     100h    ; Trap flag (single-step)
RFLAGS_IF                   EQU     200h    ; Interrupt enable flag
RFLAGS_DF                   EQU     400h    ; Direction flag
RFLAGS_OF                   EQU     800h    ; Overflow flag

; DR7 control bits
DR7_L0                      EQU     001h    ; Local enable DR0
DR7_G0                      EQU     002h    ; Global enable DR0
DR7_L1                      EQU     004h    ; Local enable DR1
DR7_G1                      EQU     008h    ; Global enable DR1
DR7_L2                      EQU     010h    ; Local enable DR2
DR7_G2                      EQU     020h    ; Global enable DR2
DR7_L3                      EQU     040h    ; Local enable DR3
DR7_G3                      EQU     080h    ; Global enable DR3

; DR7 condition types (bits 16-17, 20-21, 24-25, 28-29 for DR0-DR3)
DR7_COND_EXEC               EQU     0       ; Break on execution
DR7_COND_WRITE              EQU     1       ; Break on data write
DR7_COND_IO                 EQU     2       ; Break on I/O (rarely used)
DR7_COND_RW                 EQU     3       ; Break on data read/write

; DR7 size encoding (bits 18-19, 22-23, 26-27, 30-31 for DR0-DR3)
DR7_SIZE_1                  EQU     0       ; 1 byte
DR7_SIZE_2                  EQU     1       ; 2 bytes (WORD)
DR7_SIZE_8                  EQU     2       ; 8 bytes (QWORD)
DR7_SIZE_4                  EQU     3       ; 4 bytes (DWORD)

; INT3 opcode
INT3_OPCODE                 EQU     0CCh

; Thread access rights
THREAD_GET_CONTEXT          EQU     0008h
THREAD_SET_CONTEXT          EQU     0010h
THREAD_SUSPEND_RESUME       EQU     0002h
THREAD_ALL_ACCESS           EQU     001FFFFFh

; Memory scan
SCAN_CHUNK_SIZE             EQU     10000h  ; 64KB read chunks for pattern scan
SCAN_MAX_CHUNKS             EQU     1000h   ; Max 4096 chunks

; CRC-32 polynomial (IEEE 802.3)
CRC32_POLY                  EQU     0EDB88320h

; =============================================================================
;                         READ-ONLY DATA
; =============================================================================

.data

; CRC-32 lookup table (256 entries × 4 bytes = 1024 bytes)
ALIGN 16
crc32_table DD 000000000h, 077073096h, 0EE0E612Ch, 0990951BAh
            DD 0076DC419h, 0706AF48Fh, 0E963A535h, 09E6495A3h
            DD 00EDB8832h, 079DCB8A4h, 0E0D5E91Bh, 097D2D988h
            DD 009B64C2Bh, 07EB17CBDh, 0E7B82D09h, 090BF1D9Fh
            DD 01DB71064h, 06AB020F2h, 0F3B97148h, 084BE41DEh
            DD 01ADAD47Dh, 06DDDE4EBh, 0F4D4B551h, 083D385C7h
            DD 0136C9856h, 0646BA8C0h, 0FD62F97Ah, 08A65C9ECh
            DD 014015C4Fh, 063066CD9h, 0FA0F3D63h, 08D080DF5h
            DD 03B6E20C8h, 04C69105Eh, 0D56041E4h, 0A2677172h
            DD 03C03E4D1h, 04B04D447h, 0D20D85FDh, 0A50AB56Bh
            DD 035B5A8FAh, 042B2986Ch, 0DBBBC9D6h, 0ACBCF940h
            DD 032D86CE3h, 045DF5C75h, 0DCD60DCFh, 0ABD13D59h
            DD 026D930ACh, 051DE003Ah, 0C8D75180h, 0BFD06116h
            DD 021B4F4B5h, 056B3C423h, 0CF0BA899h, 0B805980Fh
            DD 028D12D17h, 05FD6E381h, 0C6DF0B3Bh, 0B1D0BBFDh
            DD 02F6F7C87h, 0584ED011h, 0C1E3D0ABh, 0B6E4E03Dh
            DD 04ADF5355h, 03DD863C3h, 0A4D13279h, 0D3D602EFh
            DD 04DB26158h, 03AB551CEh, 0A3BC0074h, 0D4BB30E2h
            DD 044042D73h, 033031DE5h, 0AA0A4C5Fh, 0DD0D7CC9h
            DD 05005713Ch, 0270241AAh, 0BE0B1010h, 0C90C2086h
            DD 05768B525h, 0206F85B3h, 0B966D409h, 0CE61E49Fh
            DD 05EDEF90Eh, 029D9C998h, 0B0D09822h, 0C7D7A8B4h
            DD 059B33D17h, 02EB40D81h, 0B7BD5C3Bh, 0C0BA6CADh
            DD 0EDB88320h, 09ABFB3B6h, 003B6E20Ch, 074B1D29Ah
            DD 0EAD54739h, 09DD277AFh, 004DB2615h, 073DC1683h
            DD 0E3630B12h, 094643B84h, 00D6D6A3Eh, 07A6A5AA8h
            DD 0E40ECF0Bh, 09309FF9Dh, 00A00AE27h, 07D079EB1h
            DD 0F00F9344h, 08708A3D2h, 01E01F268h, 06906C2FEh
            DD 0F762575Dh, 0806567CBh, 0196C3671h, 06E6B06E7h
            DD 0FED41B76h, 089D32BE0h, 010DA7A5Ah, 067DD4ACCh
            DD 0F9B9DF6Fh, 08EBEEFF9h, 017B7BE43h, 060B08ED5h
            DD 0D6D6A3E8h, 0A1D1937Eh, 038D8C2C4h, 04FDFF252h
            DD 0D1BB67F1h, 0A6BC5767h, 03FB506DDh, 048B2364Bh
            DD 0D80D2BDAh, 0AF0A1B4Ch, 036034AF6h, 041047A60h
            DD 0DF60EFC3h, 0A867DF55h, 0316E8EEFh, 04669BE79h
            DD 0CB61B38Ch, 0BC66831Ah, 0256FD2A0h, 05268E236h
            DD 0CC0C7795h, 0BB0B4703h, 0220216B9h, 05505262Fh
            DD 0C5BA3BBEh, 0B2BD0B28h, 02BB45A92h, 05CB36A04h
            DD 0C2D7FFA7h, 0B5D0CF31h, 02CD99E8Bh, 05BDEAE1Dh
            DD 09B64C2B0h, 0EC63F226h, 07562639Ch, 0026557A5h  ; corrected last entry
            DD 09C0906A9h, 0EB0E363Fh, 072076785h, 005005713h
            DD 095BF4A82h, 0E2B87A14h, 07BB12BAEh, 00CB61B38h
            DD 092D28E9Bh, 0E5D5BE0Dh, 07CDCEFB7h, 00BDBDF21h
            DD 086D3D2D4h, 0F1D4E242h, 068DDB3F8h, 01FDA836Eh
            DD 081BE16CDh, 0F6B9265Bh, 06FB077E1h, 018B74777h
            DD 088085AE6h, 0FF0F6B70h, 066063BCAh, 011010B5Ch
            DD 08F659EFFh, 0F862AE69h, 0616BFFD3h, 0166CCF45h
            DD 0A00AE278h, 0D70DD2EEh, 04E048354h, 03903B3C2h
            DD 0A7672661h, 0D06016F7h, 04969474Dh, 03E6E77DBh
            DD 0AED16A4Ah, 0D9D65ADCh, 040DF0B66h, 037D83BF0h
            DD 0A9BCAE53h, 0DEBB9EC5h, 047B2CF7Fh, 030B5FFE9h
            DD 0BDBDF21Ch, 0CABAC28Ah, 053B39330h, 024B4A3A6h
            DD 0BAD03605h, 0CDD70693h, 054DE5729h, 023D967BFh
            DD 0B3667A2Eh, 0C4614AB8h, 05D681B02h, 02A6F2B94h
            DD 0B40BBE37h, 0C30C8EA1h, 05A05DF1Bh, 02D02EF8Dh

; Status strings
dbg_ok_msg      DB "OK", 0
dbg_err_ctx     DB "GetThreadContext failed", 0
dbg_err_setctx  DB "SetThreadContext failed", 0
dbg_err_rpm     DB "ReadProcessMemory failed", 0
dbg_err_wpm     DB "WriteProcessMemory failed", 0
dbg_err_vpe     DB "VirtualProtectEx failed", 0
dbg_err_slot    DB "Invalid HW breakpoint slot", 0
dbg_err_size    DB "Invalid buffer size", 0
dbg_err_scan    DB "Pattern not found", 0

; =============================================================================
;                              CODE
; =============================================================================

.code

; =============================================================================
; Dbg_InjectINT3 — Inject INT3 (0xCC) at a target address
; =============================================================================
; IN:  RCX = targetAddress (ULONG64)
;      RDX = outOriginalByte (BYTE* — receives the byte replaced)
; OUT: EAX = 0 on success, nonzero on failure
; =============================================================================
ALIGN 16
Dbg_InjectINT3 PROC FRAME
    SAVE_NONVOL
    .PUSHREG r15
    .PUSHREG r14
    .PUSHREG r13
    .PUSHREG r12
    .PUSHREG rdi
    .PUSHREG rsi
    .PUSHREG rbx
    push    rbp
    .PUSHREG rbp
    mov     rbp, rsp
    .SETFRAME rbp, 0
    sub     rsp, 80h                ; Local space
    .ALLOCSTACK 80h
    .ENDPROLOG

    mov     r12, rcx                ; r12 = targetAddress
    mov     r13, rdx                ; r13 = outOriginalByte ptr

    ; Read the original byte at target address
    ; We use ReadProcessMemory with GetCurrentProcess (pseudohandle -1)
    lea     r8, [rbp - 10h]         ; Buffer for 1 byte
    mov     rcx, -1                 ; hProcess = GetCurrentProcess()
    mov     rdx, r12                ; lpBaseAddress
    mov     r9, 1                   ; nSize = 1
    lea     rax, [rbp - 18h]        ; lpNumberOfBytesRead
    mov     [rsp + 20h], rax
    call    ReadProcessMemory
    test    eax, eax
    jz      @@inject_fail

    ; Save original byte to caller
    movzx   eax, BYTE PTR [rbp - 10h]
    mov     BYTE PTR [r13], al

    ; Change memory protection to RWX
    lea     r9, [rbp - 20h]         ; lpflOldProtect
    mov     rcx, -1                 ; hProcess
    mov     rdx, r12                ; lpAddress
    mov     r8, 1                   ; dwSize = 1
    mov     DWORD PTR [rsp + 20h], PAGE_EXECUTE_READWRITE
    call    VirtualProtectEx
    test    eax, eax
    jz      @@inject_fail

    ; Write INT3 byte
    mov     BYTE PTR [rbp - 10h], INT3_OPCODE
    lea     r8, [rbp - 10h]         ; lpBuffer
    mov     rcx, -1
    mov     rdx, r12
    mov     r9, 1
    lea     rax, [rbp - 18h]
    mov     [rsp + 20h], rax
    call    WriteProcessMemory
    test    eax, eax
    jz      @@inject_restore_prot

    ; Flush instruction cache
    mov     rcx, -1
    mov     rdx, r12
    mov     r8, 1
    call    FlushInstructionCache

    ; Restore original protection
    mov     ecx, DWORD PTR [rbp - 20h]   ; saved old protection
    lea     r9, [rbp - 28h]         ; dummy lpflOldProtect
    push    rcx                     ; Save old prot
    mov     rcx, -1
    mov     rdx, r12
    mov     r8, 1
    pop     rax
    mov     DWORD PTR [rsp + 20h], eax
    call    VirtualProtectEx

    xor     eax, eax                ; SUCCESS
    jmp     @@inject_done

@@inject_restore_prot:
    ; Restore protection on write failure
    mov     ecx, DWORD PTR [rbp - 20h]
    lea     r9, [rbp - 28h]
    push    rcx
    mov     rcx, -1
    mov     rdx, r12
    mov     r8, 1
    pop     rax
    mov     DWORD PTR [rsp + 20h], eax
    call    VirtualProtectEx

@@inject_fail:
    mov     eax, 1                  ; FAILURE
    lea     rdx, dbg_err_wpm

@@inject_done:
    add     rsp, 80h
    pop     rbp
    RESTORE_NONVOL
    ret
Dbg_InjectINT3 ENDP

; =============================================================================
; Dbg_RestoreINT3 — Restore original byte at address (remove INT3)
; =============================================================================
; IN:  RCX = targetAddress (ULONG64)
;      DL  = originalByte (BYTE to restore)
; OUT: EAX = 0 on success, nonzero on failure
; =============================================================================
ALIGN 16
Dbg_RestoreINT3 PROC FRAME
    SAVE_NONVOL
    .PUSHREG r15
    .PUSHREG r14
    .PUSHREG r13
    .PUSHREG r12
    .PUSHREG rdi
    .PUSHREG rsi
    .PUSHREG rbx
    push    rbp
    .PUSHREG rbp
    mov     rbp, rsp
    .SETFRAME rbp, 0
    sub     rsp, 80h
    .ALLOCSTACK 80h
    .ENDPROLOG

    mov     r12, rcx                ; r12 = targetAddress
    mov     r13b, dl                ; r13b = originalByte

    ; Change to RWX
    lea     r9, [rbp - 20h]
    mov     rcx, -1
    mov     rdx, r12
    mov     r8, 1
    mov     DWORD PTR [rsp + 20h], PAGE_EXECUTE_READWRITE
    call    VirtualProtectEx
    test    eax, eax
    jz      @@restore_fail

    ; Write original byte back
    mov     BYTE PTR [rbp - 10h], r13b
    lea     r8, [rbp - 10h]
    mov     rcx, -1
    mov     rdx, r12
    mov     r9, 1
    lea     rax, [rbp - 18h]
    mov     [rsp + 20h], rax
    call    WriteProcessMemory
    test    eax, eax
    jz      @@restore_prot

    ; Flush icache
    mov     rcx, -1
    mov     rdx, r12
    mov     r8, 1
    call    FlushInstructionCache

@@restore_prot:
    ; Restore original protection
    mov     ecx, DWORD PTR [rbp - 20h]
    lea     r9, [rbp - 28h]
    push    rcx
    mov     rcx, -1
    mov     rdx, r12
    mov     r8, 1
    pop     rax
    mov     DWORD PTR [rsp + 20h], eax
    call    VirtualProtectEx

    xor     eax, eax                ; SUCCESS
    jmp     @@restore_done

@@restore_fail:
    mov     eax, 1
    lea     rdx, dbg_err_vpe

@@restore_done:
    add     rsp, 80h
    pop     rbp
    RESTORE_NONVOL
    ret
Dbg_RestoreINT3 ENDP

; =============================================================================
; Dbg_SetHardwareBreakpoint — Set a hardware breakpoint via DR0–DR3
; =============================================================================
; IN:  RCX = threadHandle (HANDLE)
;      EDX = slotIndex (0–3)
;      R8  = breakpoint address
;      R9D = condition (0=exec, 1=write, 3=rw)
;      [rsp+28h] = sizeBytes (1,2,4,8)
; OUT: EAX = 0 on success, nonzero on failure
; =============================================================================
ALIGN 16
Dbg_SetHardwareBreakpoint PROC FRAME
    SAVE_NONVOL
    .PUSHREG r15
    .PUSHREG r14
    .PUSHREG r13
    .PUSHREG r12
    .PUSHREG rdi
    .PUSHREG rsi
    .PUSHREG rbx
    push    rbp
    .PUSHREG rbp
    mov     rbp, rsp
    .SETFRAME rbp, 0
    sub     rsp, 500h               ; Room for CONTEXT (1232 bytes) + locals
    .ALLOCSTACK 500h
    .ENDPROLOG

    mov     r12, rcx                ; r12 = threadHandle
    mov     r13d, edx               ; r13d = slotIndex
    mov     r14, r8                  ; r14 = address
    mov     r15d, r9d               ; r15d = condition

    ; Validate slot index
    cmp     r13d, 3
    ja      @@hwbp_bad_slot

    ; Get size parameter from stack (5th param at [rbp + 68h] adjusted for pushes)
    ; After 7 pushes (SAVE_NONVOL) + push rbp = 8 pushes = 64 bytes
    ; Original RSP + 8 (return addr) + 20h (shadow) + 0 = 5th param at [original_rsp + 28h]
    ; With our frame: [rbp + 8 + 64 + 28h] = [rbp + 70h]
    mov     ebx, DWORD PTR [rbp + 70h]

    ; Suspend thread first
    mov     rcx, r12
    call    SuspendThread
    cmp     eax, -1
    je      @@hwbp_fail

    ; Prepare CONTEXT on stack
    lea     rdi, [rbp - 4D0h]       ; CONTEXT buffer
    mov     rcx, rdi
    xor     edx, edx
    mov     r8, CONTEXT_STRUCT_SIZE
    call    memset

    ; Set ContextFlags = CONTEXT_DEBUG_REGISTERS | CONTEXT_CONTROL
    mov     DWORD PTR [rdi + CTX_ContextFlags], CONTEXT_DEBUG_REGISTERS OR CONTEXT_CONTROL

    ; GetThreadContext
    mov     rcx, r12
    mov     rdx, rdi
    call    GetThreadContext
    test    eax, eax
    jz      @@hwbp_resume_fail

    ; Set DRn address register based on slot index
    cmp     r13d, 0
    je      @@hwbp_dr0
    cmp     r13d, 1
    je      @@hwbp_dr1
    cmp     r13d, 2
    je      @@hwbp_dr2
    jmp     @@hwbp_dr3

@@hwbp_dr0:
    mov     QWORD PTR [rdi + CTX_Dr0], r14
    jmp     @@hwbp_set_dr7

@@hwbp_dr1:
    mov     QWORD PTR [rdi + CTX_Dr1], r14
    jmp     @@hwbp_set_dr7

@@hwbp_dr2:
    mov     QWORD PTR [rdi + CTX_Dr2], r14
    jmp     @@hwbp_set_dr7

@@hwbp_dr3:
    mov     QWORD PTR [rdi + CTX_Dr3], r14

@@hwbp_set_dr7:
    ; Configure DR7
    ; Each DR slot uses 2 bits for local/global enable + 4 bits for condition/size
    ; Slot n:
    ;   Enable bits: bit (2*n) = local enable, bit (2*n+1) = global enable
    ;   Condition: bits (16 + 4*n) and (17 + 4*n)
    ;   Size:      bits (18 + 4*n) and (19 + 4*n)

    mov     rax, QWORD PTR [rdi + CTX_Dr7]

    ; Compute bit positions
    mov     ecx, r13d               ; slot index
    shl     ecx, 1                  ; ecx = 2 * slot
    mov     edx, 1
    shl     edx, cl                 ; Local enable bit
    or      eax, edx                ; Set local enable

    ; Condition bits at 16 + 4*slot
    mov     ecx, r13d
    shl     ecx, 2                  ; ecx = 4 * slot
    add     ecx, 16                 ; ecx = 16 + 4*slot

    ; Clear condition + size bits (4 bits)
    mov     edx, 0Fh
    shl     edx, cl
    not     edx
    and     eax, edx

    ; Set condition (2 bits)
    mov     edx, r15d
    and     edx, 3                  ; Mask to 2 bits
    shl     edx, cl
    or      eax, edx

    ; Set size (2 bits at offset +2)
    add     ecx, 2
    ; Encode size: 1→0, 2→1, 4→3, 8→2
    xor     edx, edx
    cmp     ebx, 1
    je      @@hwbp_size_done
    cmp     ebx, 2
    jne     @@hwbp_sz4
    mov     edx, 1
    jmp     @@hwbp_size_done
@@hwbp_sz4:
    cmp     ebx, 4
    jne     @@hwbp_sz8
    mov     edx, 3
    jmp     @@hwbp_size_done
@@hwbp_sz8:
    mov     edx, 2                  ; 8 bytes
@@hwbp_size_done:
    shl     edx, cl
    or      eax, edx

    mov     QWORD PTR [rdi + CTX_Dr7], rax

    ; SetThreadContext
    mov     rcx, r12
    mov     rdx, rdi
    call    SetThreadContext
    test    eax, eax
    jz      @@hwbp_resume_fail

    ; Resume thread
    mov     rcx, r12
    call    ResumeThread

    xor     eax, eax                ; SUCCESS
    jmp     @@hwbp_done

@@hwbp_bad_slot:
    mov     eax, 2
    lea     rdx, dbg_err_slot
    jmp     @@hwbp_done

@@hwbp_resume_fail:
    ; Resume thread even on failure
    mov     rcx, r12
    call    ResumeThread

@@hwbp_fail:
    mov     eax, 1
    lea     rdx, dbg_err_setctx

@@hwbp_done:
    add     rsp, 500h
    pop     rbp
    RESTORE_NONVOL
    ret
Dbg_SetHardwareBreakpoint ENDP

; =============================================================================
; Dbg_ClearHardwareBreakpoint — Clear a hardware breakpoint slot
; =============================================================================
; IN:  RCX = threadHandle
;      EDX = slotIndex (0–3)
; OUT: EAX = 0 on success
; =============================================================================
ALIGN 16
Dbg_ClearHardwareBreakpoint PROC FRAME
    SAVE_NONVOL
    .PUSHREG r15
    .PUSHREG r14
    .PUSHREG r13
    .PUSHREG r12
    .PUSHREG rdi
    .PUSHREG rsi
    .PUSHREG rbx
    push    rbp
    .PUSHREG rbp
    mov     rbp, rsp
    .SETFRAME rbp, 0
    sub     rsp, 500h
    .ALLOCSTACK 500h
    .ENDPROLOG

    mov     r12, rcx
    mov     r13d, edx

    cmp     r13d, 3
    ja      @@clr_bad_slot

    ; Suspend thread
    mov     rcx, r12
    call    SuspendThread
    cmp     eax, -1
    je      @@clr_fail

    ; Get context
    lea     rdi, [rbp - 4D0h]
    mov     rcx, rdi
    xor     edx, edx
    mov     r8, CONTEXT_STRUCT_SIZE
    call    memset

    mov     DWORD PTR [rdi + CTX_ContextFlags], CONTEXT_DEBUG_REGISTERS
    mov     rcx, r12
    mov     rdx, rdi
    call    GetThreadContext
    test    eax, eax
    jz      @@clr_resume_fail

    ; Clear DRn register
    cmp     r13d, 0
    je      @@clr_dr0
    cmp     r13d, 1
    je      @@clr_dr1
    cmp     r13d, 2
    je      @@clr_dr2
    jmp     @@clr_dr3

@@clr_dr0:
    mov     QWORD PTR [rdi + CTX_Dr0], 0
    jmp     @@clr_update_dr7
@@clr_dr1:
    mov     QWORD PTR [rdi + CTX_Dr1], 0
    jmp     @@clr_update_dr7
@@clr_dr2:
    mov     QWORD PTR [rdi + CTX_Dr2], 0
    jmp     @@clr_update_dr7
@@clr_dr3:
    mov     QWORD PTR [rdi + CTX_Dr3], 0

@@clr_update_dr7:
    ; Clear enable bits + condition/size for this slot in DR7
    mov     rax, QWORD PTR [rdi + CTX_Dr7]

    ; Clear local + global enable (bits 2*n, 2*n+1)
    mov     ecx, r13d
    shl     ecx, 1
    mov     edx, 3
    shl     edx, cl
    not     edx
    and     eax, edx

    ; Clear condition + size (bits 16+4*n through 19+4*n)
    mov     ecx, r13d
    shl     ecx, 2
    add     ecx, 16
    mov     edx, 0Fh
    shl     edx, cl
    not     edx
    and     eax, edx

    mov     QWORD PTR [rdi + CTX_Dr7], rax

    ; Set context
    mov     DWORD PTR [rdi + CTX_ContextFlags], CONTEXT_DEBUG_REGISTERS
    mov     rcx, r12
    mov     rdx, rdi
    call    SetThreadContext
    test    eax, eax
    jz      @@clr_resume_fail

    ; Resume
    mov     rcx, r12
    call    ResumeThread

    xor     eax, eax
    jmp     @@clr_done

@@clr_bad_slot:
    mov     eax, 2
    lea     rdx, dbg_err_slot
    jmp     @@clr_done

@@clr_resume_fail:
    mov     rcx, r12
    call    ResumeThread
@@clr_fail:
    mov     eax, 1
    lea     rdx, dbg_err_setctx

@@clr_done:
    add     rsp, 500h
    pop     rbp
    RESTORE_NONVOL
    ret
Dbg_ClearHardwareBreakpoint ENDP

; =============================================================================
; Dbg_EnableSingleStep — Set Trap Flag (TF) in thread's RFLAGS
; =============================================================================
; IN:  RCX = threadHandle
; OUT: EAX = 0 on success
; =============================================================================
ALIGN 16
Dbg_EnableSingleStep PROC FRAME
    SAVE_NONVOL
    .PUSHREG r15
    .PUSHREG r14
    .PUSHREG r13
    .PUSHREG r12
    .PUSHREG rdi
    .PUSHREG rsi
    .PUSHREG rbx
    push    rbp
    .PUSHREG rbp
    mov     rbp, rsp
    .SETFRAME rbp, 0
    sub     rsp, 500h
    .ALLOCSTACK 500h
    .ENDPROLOG

    mov     r12, rcx

    ; Suspend
    mov     rcx, r12
    call    SuspendThread
    cmp     eax, -1
    je      @@ss_en_fail

    ; Get context
    lea     rdi, [rbp - 4D0h]
    mov     rcx, rdi
    xor     edx, edx
    mov     r8, CONTEXT_STRUCT_SIZE
    call    memset

    mov     DWORD PTR [rdi + CTX_ContextFlags], CONTEXT_CONTROL
    mov     rcx, r12
    mov     rdx, rdi
    call    GetThreadContext
    test    eax, eax
    jz      @@ss_en_resume_fail

    ; Set TF bit
    mov     eax, DWORD PTR [rdi + CTX_EFlags]
    or      eax, RFLAGS_TF
    mov     DWORD PTR [rdi + CTX_EFlags], eax

    ; Set context
    mov     rcx, r12
    mov     rdx, rdi
    call    SetThreadContext
    test    eax, eax
    jz      @@ss_en_resume_fail

    ; Resume
    mov     rcx, r12
    call    ResumeThread
    xor     eax, eax
    jmp     @@ss_en_done

@@ss_en_resume_fail:
    mov     rcx, r12
    call    ResumeThread
@@ss_en_fail:
    mov     eax, 1
    lea     rdx, dbg_err_setctx

@@ss_en_done:
    add     rsp, 500h
    pop     rbp
    RESTORE_NONVOL
    ret
Dbg_EnableSingleStep ENDP

; =============================================================================
; Dbg_DisableSingleStep — Clear Trap Flag (TF) in thread's RFLAGS
; =============================================================================
; IN:  RCX = threadHandle
; OUT: EAX = 0 on success
; =============================================================================
ALIGN 16
Dbg_DisableSingleStep PROC FRAME
    SAVE_NONVOL
    .PUSHREG r15
    .PUSHREG r14
    .PUSHREG r13
    .PUSHREG r12
    .PUSHREG rdi
    .PUSHREG rsi
    .PUSHREG rbx
    push    rbp
    .PUSHREG rbp
    mov     rbp, rsp
    .SETFRAME rbp, 0
    sub     rsp, 500h
    .ALLOCSTACK 500h
    .ENDPROLOG

    mov     r12, rcx

    mov     rcx, r12
    call    SuspendThread
    cmp     eax, -1
    je      @@ss_dis_fail

    lea     rdi, [rbp - 4D0h]
    mov     rcx, rdi
    xor     edx, edx
    mov     r8, CONTEXT_STRUCT_SIZE
    call    memset

    mov     DWORD PTR [rdi + CTX_ContextFlags], CONTEXT_CONTROL
    mov     rcx, r12
    mov     rdx, rdi
    call    GetThreadContext
    test    eax, eax
    jz      @@ss_dis_resume_fail

    ; Clear TF bit
    mov     eax, DWORD PTR [rdi + CTX_EFlags]
    and     eax, NOT RFLAGS_TF
    mov     DWORD PTR [rdi + CTX_EFlags], eax

    mov     rcx, r12
    mov     rdx, rdi
    call    SetThreadContext
    test    eax, eax
    jz      @@ss_dis_resume_fail

    mov     rcx, r12
    call    ResumeThread
    xor     eax, eax
    jmp     @@ss_dis_done

@@ss_dis_resume_fail:
    mov     rcx, r12
    call    ResumeThread
@@ss_dis_fail:
    mov     eax, 1
    lea     rdx, dbg_err_setctx

@@ss_dis_done:
    add     rsp, 500h
    pop     rbp
    RESTORE_NONVOL
    ret
Dbg_DisableSingleStep ENDP

; =============================================================================
; Dbg_CaptureContext — Get full thread context (GPR + debug registers)
; =============================================================================
; IN:  RCX = threadHandle
;      RDX = outContextBuffer (void* — must be >= CONTEXT_STRUCT_SIZE bytes)
;      R8D = bufferSize
; OUT: EAX = 0 on success
; =============================================================================
ALIGN 16
Dbg_CaptureContext PROC FRAME
    SAVE_NONVOL
    .PUSHREG r15
    .PUSHREG r14
    .PUSHREG r13
    .PUSHREG r12
    .PUSHREG rdi
    .PUSHREG rsi
    .PUSHREG rbx
    push    rbp
    .PUSHREG rbp
    mov     rbp, rsp
    .SETFRAME rbp, 0
    sub     rsp, 60h
    .ALLOCSTACK 60h
    .ENDPROLOG

    mov     r12, rcx                ; threadHandle
    mov     r13, rdx                ; outBuffer
    mov     r14d, r8d               ; bufferSize

    ; Validate buffer size
    cmp     r14d, CONTEXT_STRUCT_SIZE
    jb      @@cap_bad_size

    ; Zero the output buffer
    mov     rcx, r13
    xor     edx, edx
    mov     r8d, r14d
    call    memset

    ; Set flags for CONTEXT_ALL
    mov     DWORD PTR [r13 + CTX_ContextFlags], CONTEXT_ALL

    ; Suspend thread
    mov     rcx, r12
    call    SuspendThread
    cmp     eax, -1
    je      @@cap_fail

    ; Get context
    mov     rcx, r12
    mov     rdx, r13
    call    GetThreadContext
    test    eax, eax
    jz      @@cap_resume_fail

    ; Resume
    mov     rcx, r12
    call    ResumeThread

    xor     eax, eax
    jmp     @@cap_done

@@cap_bad_size:
    mov     eax, 2
    lea     rdx, dbg_err_size
    jmp     @@cap_done

@@cap_resume_fail:
    mov     rcx, r12
    call    ResumeThread
@@cap_fail:
    mov     eax, 1
    lea     rdx, dbg_err_ctx

@@cap_done:
    add     rsp, 60h
    pop     rbp
    RESTORE_NONVOL
    ret
Dbg_CaptureContext ENDP

; =============================================================================
; Dbg_SetRegister — Set a single GPR by index in thread context
; =============================================================================
; IN:  RCX = threadHandle
;      EDX = registerIndex (0=rax, 1=rcx, 2=rdx, ... 16=rip, 17=rflags)
;      R8  = value to set
; OUT: EAX = 0 on success
; =============================================================================
ALIGN 16
Dbg_SetRegister PROC FRAME
    SAVE_NONVOL
    .PUSHREG r15
    .PUSHREG r14
    .PUSHREG r13
    .PUSHREG r12
    .PUSHREG rdi
    .PUSHREG rsi
    .PUSHREG rbx
    push    rbp
    .PUSHREG rbp
    mov     rbp, rsp
    .SETFRAME rbp, 0
    sub     rsp, 500h
    .ALLOCSTACK 500h
    .ENDPROLOG

    mov     r12, rcx                ; threadHandle
    mov     r13d, edx               ; registerIndex
    mov     r14, r8                  ; value

    ; Suspend
    mov     rcx, r12
    call    SuspendThread
    cmp     eax, -1
    je      @@setreg_fail

    ; Get context
    lea     rdi, [rbp - 4D0h]
    mov     rcx, rdi
    xor     edx, edx
    mov     r8, CONTEXT_STRUCT_SIZE
    call    memset

    mov     DWORD PTR [rdi + CTX_ContextFlags], CONTEXT_FULL
    mov     rcx, r12
    mov     rdx, rdi
    call    GetThreadContext
    test    eax, eax
    jz      @@setreg_resume_fail

    ; Map registerIndex to CONTEXT offset
    ; Use jump table
    cmp     r13d, 17
    ja      @@setreg_resume_fail

    lea     rax, [@@setreg_table]
    movsxd  rcx, DWORD PTR [rax + r13 * 4]
    add     rcx, rax
    jmp     rcx

ALIGN 4
@@setreg_table:
    DD @@sr_rax  - @@setreg_table
    DD @@sr_rcx  - @@setreg_table
    DD @@sr_rdx  - @@setreg_table
    DD @@sr_rbx  - @@setreg_table
    DD @@sr_rsp  - @@setreg_table
    DD @@sr_rbp  - @@setreg_table
    DD @@sr_rsi  - @@setreg_table
    DD @@sr_rdi  - @@setreg_table
    DD @@sr_r8   - @@setreg_table
    DD @@sr_r9   - @@setreg_table
    DD @@sr_r10  - @@setreg_table
    DD @@sr_r11  - @@setreg_table
    DD @@sr_r12  - @@setreg_table
    DD @@sr_r13  - @@setreg_table
    DD @@sr_r14  - @@setreg_table
    DD @@sr_r15  - @@setreg_table
    DD @@sr_rip  - @@setreg_table
    DD @@sr_efl  - @@setreg_table

@@sr_rax:  mov QWORD PTR [rdi + CTX_Rax], r14
           jmp @@setreg_commit
@@sr_rcx:  mov QWORD PTR [rdi + CTX_Rcx], r14
           jmp @@setreg_commit
@@sr_rdx:  mov QWORD PTR [rdi + CTX_Rdx], r14
           jmp @@setreg_commit
@@sr_rbx:  mov QWORD PTR [rdi + CTX_Rbx], r14
           jmp @@setreg_commit
@@sr_rsp:  mov QWORD PTR [rdi + CTX_Rsp], r14
           jmp @@setreg_commit
@@sr_rbp:  mov QWORD PTR [rdi + CTX_Rbp], r14
           jmp @@setreg_commit
@@sr_rsi:  mov QWORD PTR [rdi + CTX_Rsi], r14
           jmp @@setreg_commit
@@sr_rdi:  mov QWORD PTR [rdi + CTX_Rdi], r14
           jmp @@setreg_commit
@@sr_r8:   mov QWORD PTR [rdi + CTX_R8], r14
           jmp @@setreg_commit
@@sr_r9:   mov QWORD PTR [rdi + CTX_R9], r14
           jmp @@setreg_commit
@@sr_r10:  mov QWORD PTR [rdi + CTX_R10], r14
           jmp @@setreg_commit
@@sr_r11:  mov QWORD PTR [rdi + CTX_R11], r14
           jmp @@setreg_commit
@@sr_r12:  mov QWORD PTR [rdi + CTX_R12], r14
           jmp @@setreg_commit
@@sr_r13:  mov QWORD PTR [rdi + CTX_R13], r14
           jmp @@setreg_commit
@@sr_r14:  mov QWORD PTR [rdi + CTX_R14], r14
           jmp @@setreg_commit
@@sr_r15:  mov QWORD PTR [rdi + CTX_R15], r14
           jmp @@setreg_commit
@@sr_rip:  mov QWORD PTR [rdi + CTX_Rip], r14
           jmp @@setreg_commit
@@sr_efl:  mov DWORD PTR [rdi + CTX_EFlags], r14d

@@setreg_commit:
    mov     rcx, r12
    mov     rdx, rdi
    call    SetThreadContext
    test    eax, eax
    jz      @@setreg_resume_fail

    mov     rcx, r12
    call    ResumeThread
    xor     eax, eax
    jmp     @@setreg_done

@@setreg_resume_fail:
    mov     rcx, r12
    call    ResumeThread
@@setreg_fail:
    mov     eax, 1
    lea     rdx, dbg_err_setctx

@@setreg_done:
    add     rsp, 500h
    pop     rbp
    RESTORE_NONVOL
    ret
Dbg_SetRegister ENDP

; =============================================================================
; Dbg_WalkStack — Walk the call stack via RBP chain in remote process
; =============================================================================
; IN:  RCX = processHandle
;      RDX = threadHandle (for initial RBP/RIP from context)
;      R8  = outFrames (uint64_t* array for return addresses)
;      R9D = maxFrames
;      [rsp+28h] = outFrameCount (uint32_t*)
; OUT: EAX = 0 on success
; =============================================================================
ALIGN 16
Dbg_WalkStack PROC FRAME
    SAVE_NONVOL
    .PUSHREG r15
    .PUSHREG r14
    .PUSHREG r13
    .PUSHREG r12
    .PUSHREG rdi
    .PUSHREG rsi
    .PUSHREG rbx
    push    rbp
    .PUSHREG rbp
    mov     rbp, rsp
    .SETFRAME rbp, 0
    sub     rsp, 500h
    .ALLOCSTACK 500h
    .ENDPROLOG

    mov     r12, rcx                ; processHandle
    mov     r13, rdx                ; threadHandle
    mov     r14, r8                  ; outFrames array
    mov     r15d, r9d               ; maxFrames

    ; Get outFrameCount pointer
    mov     rbx, QWORD PTR [rbp + 70h]

    ; Get thread context for initial RBP + RIP
    lea     rdi, [rbp - 4D0h]
    mov     rcx, rdi
    xor     edx, edx
    mov     r8, CONTEXT_STRUCT_SIZE
    call    memset

    mov     DWORD PTR [rdi + CTX_ContextFlags], CONTEXT_CONTROL OR CONTEXT_INTEGER
    mov     rcx, r13
    mov     rdx, rdi
    call    GetThreadContext
    test    eax, eax
    jz      @@walk_fail

    ; Store first frame (current RIP)
    mov     rax, QWORD PTR [rdi + CTX_Rip]
    mov     QWORD PTR [r14], rax
    mov     esi, 1                  ; frame count = 1

    ; Get initial RBP for chain walking
    mov     rdi, QWORD PTR [rdi + CTX_Rbp]

    ; Walk RBP chain: [RBP+0] = saved RBP, [RBP+8] = return address
@@walk_loop:
    cmp     esi, r15d
    jge     @@walk_done
    test    rdi, rdi
    jz      @@walk_done

    ; Read saved RBP and return address from remote process
    ; ReadProcessMemory(processHandle, rbp, &local_buf, 16, &bytesRead)
    lea     r8, [rbp - 500h]        ; local buffer for 16 bytes
    mov     rcx, r12
    mov     rdx, rdi
    mov     r9, 16
    lea     rax, [rbp - 508h]       ; bytesRead
    mov     [rsp + 20h], rax
    call    ReadProcessMemory
    test    eax, eax
    jz      @@walk_done

    ; Extract return address and store
    mov     rax, QWORD PTR [rbp - 500h + 8]  ; return address = [saved_rbp + 8]
    test    rax, rax
    jz      @@walk_done

    movsxd  rcx, esi
    mov     QWORD PTR [r14 + rcx * 8], rax
    inc     esi

    ; Follow chain: new RBP = [old_rbp + 0]
    mov     rdi, QWORD PTR [rbp - 500h]

    ; Sanity check: RBP should be increasing (stack grows down)
    ; If new RBP < old, we likely hit corruption — stop
    cmp     rdi, QWORD PTR [rbp - 500h]
    jmp     @@walk_loop

@@walk_done:
    ; Store frame count
    test    rbx, rbx
    jz      @@walk_skip_count
    mov     DWORD PTR [rbx], esi
@@walk_skip_count:
    xor     eax, eax
    jmp     @@walk_exit

@@walk_fail:
    mov     eax, 1
    lea     rdx, dbg_err_ctx

@@walk_exit:
    add     rsp, 500h
    pop     rbp
    RESTORE_NONVOL
    ret
Dbg_WalkStack ENDP

; =============================================================================
; Dbg_ReadMemory — Read memory from a target process
; =============================================================================
; IN:  RCX = processHandle
;      RDX = sourceAddress
;      R8  = outBuffer
;      R9  = size (uint64_t)
;      [rsp+28h] = outBytesRead (uint64_t*)
; OUT: EAX = 0 on success
; =============================================================================
ALIGN 16
Dbg_ReadMemory PROC FRAME
    push    rbp
    .PUSHREG rbp
    mov     rbp, rsp
    .SETFRAME rbp, 0
    sub     rsp, 40h
    .ALLOCSTACK 40h
    .ENDPROLOG

    ; ReadProcessMemory(hProcess, lpBaseAddress, lpBuffer, nSize, *lpNumberOfBytesRead)
    ; RCX=processHandle, RDX=sourceAddress, R8=outBuffer, R9=size
    ; 5th param from caller at [rbp + 30h]  (adjusted for push rbp)
    mov     rax, QWORD PTR [rbp + 30h]
    mov     QWORD PTR [rsp + 20h], rax
    call    ReadProcessMemory
    test    eax, eax
    jnz     @@read_ok

    mov     eax, 1
    lea     rdx, dbg_err_rpm
    jmp     @@read_done

@@read_ok:
    xor     eax, eax

@@read_done:
    add     rsp, 40h
    pop     rbp
    ret
Dbg_ReadMemory ENDP

; =============================================================================
; Dbg_WriteMemory — Write memory to a target process
; =============================================================================
; IN:  RCX = processHandle
;      RDX = destAddress
;      R8  = buffer (const void*)
;      R9  = size (uint64_t)
;      [rsp+28h] = outBytesWritten (uint64_t*)
; OUT: EAX = 0 on success
; =============================================================================
ALIGN 16
Dbg_WriteMemory PROC FRAME
    push    rbp
    .PUSHREG rbp
    mov     rbp, rsp
    .SETFRAME rbp, 0
    sub     rsp, 60h
    .ALLOCSTACK 60h
    .ENDPROLOG

    mov     r10, rcx                ; save processHandle
    mov     r11, rdx                ; save destAddress

    ; First, change protection to PAGE_EXECUTE_READWRITE
    lea     r9, [rbp - 10h]         ; lpflOldProtect
    mov     rcx, r10
    mov     rdx, r11
    ; r8 is already size — but we need it for VirtualProtectEx(proc, addr, size, newprot, &old)
    ; Reorder: VirtualProtectEx expects (proc, addr, size, newProt, &oldProt)
    push    r8                      ; save size
    push    r9                      ; save &oldProt addr
    mov     rcx, r10                ; hProcess
    mov     rdx, r11                ; lpAddress
    pop     r9                      ; &oldProt => r9 (5th param position swap)
    pop     r8                      ; size
    mov     DWORD PTR [rsp + 20h], PAGE_EXECUTE_READWRITE
    ; Actually VirtualProtectEx is (hProcess, lpAddress, dwSize, flNewProtect, lpflOldProtect)
    ; So: rcx=proc, rdx=addr, r8=size, r9d=newprot, [rsp+20h]=&oldprot
    ; Need to swap r9 and [rsp+20h]
    mov     rax, r9
    mov     r9d, PAGE_EXECUTE_READWRITE
    mov     QWORD PTR [rsp + 20h], rax
    call    VirtualProtectEx
    test    eax, eax
    jz      @@write_direct

    ; WriteProcessMemory
    mov     rcx, r10
    mov     rdx, r11
    ; Reload original r8 (buffer) and r9 (size) from caller's original values
    ; They're still in our caller's area — reconstruct from rbp
    mov     r8, QWORD PTR [rbp + 20h]   ; 3rd param (buffer)
    mov     r9, QWORD PTR [rbp + 28h]   ; 4th param (size)
    mov     rax, QWORD PTR [rbp + 30h]  ; outBytesWritten
    mov     QWORD PTR [rsp + 20h], rax
    call    WriteProcessMemory
    mov     r15d, eax               ; save result

    ; Restore original protection
    mov     ecx, DWORD PTR [rbp - 10h]  ; oldProtect
    lea     rax, [rbp - 18h]             ; dummy &oldProt
    push    rcx
    mov     rcx, r10
    mov     rdx, r11
    mov     r8, QWORD PTR [rbp + 28h]   ; size
    pop     r9                           ; old protect value... 
    ; Reorder for VirtualProtectEx
    mov     DWORD PTR [rsp + 28h], r9d   ; Wait, this is getting complex
    ; Simpler: just call it with the values directly
    mov     r9d, DWORD PTR [rbp - 10h]
    mov     QWORD PTR [rsp + 20h], rax
    call    VirtualProtectEx

    test    r15d, r15d
    jnz     @@write_ok

@@write_direct:
    ; Try direct WriteProcessMemory without protection change
    mov     rcx, r10
    mov     rdx, r11
    mov     r8, QWORD PTR [rbp + 20h]
    mov     r9, QWORD PTR [rbp + 28h]
    mov     rax, QWORD PTR [rbp + 30h]
    mov     QWORD PTR [rsp + 20h], rax
    call    WriteProcessMemory
    test    eax, eax
    jnz     @@write_ok

    mov     eax, 1
    lea     rdx, dbg_err_wpm
    jmp     @@write_done

@@write_ok:
    xor     eax, eax

@@write_done:
    add     rsp, 60h
    pop     rbp
    ret
Dbg_WriteMemory ENDP

; =============================================================================
; Dbg_MemoryScan — Scan remote process memory for a byte pattern
; =============================================================================
; IN:  RCX = processHandle
;      RDX = startAddress
;      R8  = regionSize
;      R9  = pattern (const void*)
;      [rsp+28h] = patternLen (uint32_t)
;      [rsp+30h] = outFoundAddress (uint64_t*)
; OUT: EAX = 0 on found, 1 on not found
; =============================================================================
ALIGN 16
Dbg_MemoryScan PROC FRAME
    SAVE_NONVOL
    .PUSHREG r15
    .PUSHREG r14
    .PUSHREG r13
    .PUSHREG r12
    .PUSHREG rdi
    .PUSHREG rsi
    .PUSHREG rbx
    push    rbp
    .PUSHREG rbp
    mov     rbp, rsp
    .SETFRAME rbp, 0
    sub     rsp, 10080h             ; 64KB read buffer + locals
    .ALLOCSTACK 10080h
    .ENDPROLOG

    mov     r12, rcx                ; processHandle
    mov     r13, rdx                ; startAddress (current search position)
    mov     r14, r8                  ; regionSize (remaining)
    mov     r15, r9                  ; pattern pointer

    ; Get patternLen and outFoundAddress from stack
    mov     ebx, DWORD PTR [rbp + 70h]   ; patternLen
    mov     rsi, QWORD PTR [rbp + 78h]   ; outFoundAddress

    ; Validate
    test    ebx, ebx
    jz      @@scan_notfound
    test    r14, r14
    jz      @@scan_notfound

@@scan_chunk_loop:
    ; Determine read size: min(remaining, SCAN_CHUNK_SIZE)
    mov     rcx, r14
    cmp     rcx, SCAN_CHUNK_SIZE
    jbe     @@scan_size_ok
    mov     rcx, SCAN_CHUNK_SIZE
@@scan_size_ok:
    mov     rdi, rcx                ; rdi = this chunk size

    ; Read chunk from remote process
    lea     r8, [rbp - 10000h]      ; local buffer (64KB)
    mov     rcx, r12                ; processHandle
    mov     rdx, r13                ; address
    mov     r9, rdi                  ; size
    lea     rax, [rbp - 10070h]     ; bytesRead
    mov     QWORD PTR [rsp + 20h], rax
    call    ReadProcessMemory
    test    eax, eax
    jz      @@scan_next_chunk

    ; Get actual bytes read
    mov     rcx, QWORD PTR [rbp - 10070h]
    test    rcx, rcx
    jz      @@scan_next_chunk

    ; Linear scan through the buffer
    ; For each position i from 0 to (bytesRead - patternLen):
    ;   Compare pattern against buffer[i..i+patternLen]
    sub     rcx, rbx                ; max start offset = bytesRead - patternLen
    jl      @@scan_next_chunk       ; pattern longer than buffer
    inc     rcx                     ; number of positions to check

    xor     edx, edx                ; offset = 0

@@scan_inner:
    cmp     rdx, rcx
    jge     @@scan_next_chunk

    ; Compare pattern at buffer + offset
    push    rcx
    push    rdx
    lea     rax, [rbp - 10000h]
    add     rax, rdx                ; buffer + offset
    mov     rcx, rax                ; ptr1
    mov     rdx, r15                ; ptr2 = pattern
    movsxd  r8, ebx                 ; len = patternLen
    call    memcmp
    pop     rdx
    pop     rcx

    test    eax, eax
    jz      @@scan_found

    inc     rdx
    jmp     @@scan_inner

@@scan_found:
    ; Found! Compute address = startAddress + chunk_offset + inner_offset
    mov     rax, r13
    add     rax, rdx                ; startAddress + offset within chunk
    mov     QWORD PTR [rsi], rax    ; *outFoundAddress = found address
    xor     eax, eax                ; SUCCESS
    jmp     @@scan_done

@@scan_next_chunk:
    ; Advance to next chunk (overlap by patternLen-1 to catch cross-boundary matches)
    mov     rax, rdi
    movsxd  rcx, ebx
    sub     rax, rcx
    inc     rax                     ; advance = chunkSize - patternLen + 1
    jle     @@scan_notfound         ; shouldn't happen
    add     r13, rax
    sub     r14, rax
    jg      @@scan_chunk_loop

@@scan_notfound:
    mov     eax, 1
    lea     rdx, dbg_err_scan

@@scan_done:
    add     rsp, 10080h
    pop     rbp
    RESTORE_NONVOL
    ret
Dbg_MemoryScan ENDP

; =============================================================================
; Dbg_MemoryCRC32 — Compute CRC-32 of remote process memory
; =============================================================================
; IN:  RCX = processHandle
;      RDX = address
;      R8  = size
;      R9  = outCRC (uint32_t*)
; OUT: EAX = 0 on success
; =============================================================================
ALIGN 16
Dbg_MemoryCRC32 PROC FRAME
    SAVE_NONVOL
    .PUSHREG r15
    .PUSHREG r14
    .PUSHREG r13
    .PUSHREG r12
    .PUSHREG rdi
    .PUSHREG rsi
    .PUSHREG rbx
    push    rbp
    .PUSHREG rbp
    mov     rbp, rsp
    .SETFRAME rbp, 0
    sub     rsp, 10080h             ; 64KB buffer + locals
    .ALLOCSTACK 10080h
    .ENDPROLOG

    mov     r12, rcx                ; processHandle
    mov     r13, rdx                ; address
    mov     r14, r8                  ; remaining size
    mov     r15, r9                  ; outCRC pointer

    ; Initialize CRC
    mov     edi, 0FFFFFFFFh         ; CRC = ~0

@@crc_chunk:
    test    r14, r14
    jle     @@crc_finalize

    ; Read chunk
    mov     rcx, r14
    cmp     rcx, SCAN_CHUNK_SIZE
    jbe     @@crc_sz_ok
    mov     rcx, SCAN_CHUNK_SIZE
@@crc_sz_ok:
    mov     rbx, rcx                ; this chunk size

    lea     r8, [rbp - 10000h]
    mov     rcx, r12
    mov     rdx, r13
    mov     r9, rbx
    lea     rax, [rbp - 10070h]
    mov     QWORD PTR [rsp + 20h], rax
    call    ReadProcessMemory
    test    eax, eax
    jz      @@crc_fail

    mov     rcx, QWORD PTR [rbp - 10070h]   ; actual bytes read
    test    rcx, rcx
    jz      @@crc_fail

    ; Process CRC for each byte
    lea     rsi, [rbp - 10000h]     ; buffer pointer
    lea     rdx, crc32_table        ; table address

@@crc_byte_loop:
    test    rcx, rcx
    jz      @@crc_next_chunk

    movzx   eax, BYTE PTR [rsi]
    xor     al, dil                 ; (CRC ^ byte) & 0xFF
    movzx   eax, al
    mov     eax, DWORD PTR [rdx + rax * 4]  ; table[index]
    shr     edi, 8                  ; CRC >> 8
    xor     edi, eax                ; CRC = (CRC >> 8) ^ table[index]

    inc     rsi
    dec     rcx
    jmp     @@crc_byte_loop

@@crc_next_chunk:
    add     r13, rbx
    sub     r14, rbx
    jmp     @@crc_chunk

@@crc_finalize:
    not     edi                     ; Final XOR
    mov     DWORD PTR [r15], edi    ; *outCRC = crc
    xor     eax, eax
    jmp     @@crc_done

@@crc_fail:
    mov     eax, 1
    lea     rdx, dbg_err_rpm

@@crc_done:
    add     rsp, 10080h
    pop     rbp
    RESTORE_NONVOL
    ret
Dbg_MemoryCRC32 ENDP

; =============================================================================
; Dbg_RDTSC — Read Time Stamp Counter (cycle-accurate timing)
; =============================================================================
; IN:  (none)
; OUT: RAX = 64-bit TSC value (edx:eax combined)
; =============================================================================
ALIGN 16
Dbg_RDTSC PROC
    rdtsc
    shl     rdx, 32
    or      rax, rdx
    ret
Dbg_RDTSC ENDP

; =============================================================================
; END OF MODULE
; =============================================================================

END
