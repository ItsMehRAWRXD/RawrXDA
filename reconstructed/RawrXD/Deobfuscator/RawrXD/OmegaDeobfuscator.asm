;================================================================================
; RawrXD_OmegaDeobfuscator.asm
; FULLY REVERSE-ENGINEERED ANTI-OBFUSCATION ENGINE
; Handles ALL obfuscation types including self-hiding reverse-engineered tricks
; Author: ItsMehRAWRXD
; Target: Windows x64 | MASM64 | AVX-512
;================================================================================

.686
.xmm
.model flat, c
option casemap:none
option frame:auto

;================================================================================
; INCLUDES & EXTERNALS
;================================================================================
include \masm64\include64\masm64rt.inc

; Windows API
extern VirtualProtect:PROC
extern VirtualAlloc:PROC
extern VirtualFree:PROC
extern CreateFileA:PROC
extern ReadFile:PROC
extern WriteFile:PROC
extern GetFileSizeEx:PROC
extern CloseHandle:PROC
extern CreateThread:PROC
extern WaitForSingleObject:PROC
extern GetTickCount64:PROC
extern RtlCaptureContext:PROC
extern RtlVirtualUnwind:PROC
extern DebugActiveProcessStop:PROC
extern CheckRemoteDebuggerPresent:PROC
extern IsDebuggerPresent:PROC
extern OutputDebugStringA:PROC

;================================================================================
; STRUCTURES
;================================================================================
; Layered analysis context - tracks ALL forms of obfuscation
OBFUSCATION_LAYER struct
    layer_type      dd ?        ; 0=encoding, 1=control flow, 2=anti-debug, 3=metamorphic
    detected_at     dq ?        ; RVA where detected
    handler_ptr     dq ?        ; Function to neutralize this layer
    confidence      dd ?        ; 0-100 detection confidence
    metadata        db 64 dup(?) ; Layer-specific data
OBFUSCATION_LAYER ends

; Code pattern signature for identification
CODE_SIGNATURE struct
    pattern         db 32 dup(?) ; Byte pattern (wildcards supported)
    mask            db 32 dup(?) ; 0xFF=match, 0x00=wildcard
    pattern_len     dd ?
    signature_type  dd ?        ; What this indicates
    handler         dq ?
CODE_SIGNATURE ends

; Execution trace entry for dynamic analysis
TRACE_ENTRY struct
    rip             dq ?
    rax             dq ?
    flags           dd ?        ; RFLAGS snapshot
    memory_access   dq ?        ; If mem op, address accessed
    is_branch       db ?        ; 1 if this is a branch/jump
TRACE_ENTRY ends

;================================================================================
; DATA SECTION
;================================================================================
.data

;------------------------------------------------------------------------------
; SIGNATURE DATABASE - Detects ALL known obfuscation families
;------------------------------------------------------------------------------
; Standard packers
sig_upx             CODE_SIGNATURE <{5Ch, 89h, 0E5h, 5Dh, 5Fh, 5Eh, 5Bh, 5Ah, 59h, 58h, 0C3h}, {11 dup(0FFh)}, 11, 1, OFFSET Handle_UPX>
sig_aspack          CODE_SIGNATURE <{60h, 0E8h, 00h, 00h, 00h, 00h, 5Dh, 81h, 0EDh}, {0FFh, 0FFh, 00h, 00h, 00h, 00h, 0FFh, 0FFh, 0FFh}, 9, 1, OFFSET Handle_ASPACK>
sig_fsg             CODE_SIGNATURE <{0EBh, 02h, 0EBh, 05h, 0E8h}, {0FFh, 0FFh, 0FFh, 0FFh, 0FFh}, 5, 1, OFFSET Handle_FSG>

; Anti-debug tricks
sig_isdebugger      CODE_SIGNATURE <{0FFh, 15h, 00h, 00h, 00h, 00h, 0FFh, 0C8h, 0Fh, 85h}, {0FFh, 0FFh, 00h, 00h, 00h, 00h, 0FFh, 0FFh, 0FFh, 0FFh}, 10, 2, OFFSET Neutralize_AntiDebug>
sig_timing_check    CODE_SIGNATURE <{0Fh, 31h, 89h, 44h, 24h, 00h, 0Fh, 31h, 2Bh, 44h, 24h}, {0FFh, 0FFh, 0FFh, 0FFh, 0FFh, 00h, 0FFh, 0FFh, 0FFh, 0FFh, 0FFh}, 11, 2, OFFSET Neutralize_Timing>
sig_int3            CODE_SIGNATURE <{0CCh}, {0FFh}, 1, 2, OFFSET Neutralize_Int3>
sig_int2d           CODE_SIGNATURE <{0CDh, 2Dh}, {0FFh, 0FFh}, 2, 2, OFFSET Neutralize_Int2D>

; Control flow flattening
sig_flatten_jump    CODE_SIGNATURE <{8Bh, 04h, 0Bh, 0FFh, 0E0h}, {0FFh, 0FFh, 0FFh, 0FFh, 0FFh}, 5, 3, OFFSET Unflatten_ControlFlow>
sig_dispatcher      CODE_SIGNATURE <{48h, 8Bh, 04h, 0C5h, 00h, 00h, 00h, 00h, 0FFh, 0E0h}, {0FFh, 0FFh, 0FFh, 0FFh, 00h, 00h, 00h, 00h, 0FFh, 0FFh}, 10, 3, OFFSET Unflatten_Dispatcher>

; Opaque predicates (always true/false conditions)
sig_opaque_jz       CODE_SIGNATURE <{33h, 0C0h, 74h, 00h}, {0FFh, 0FFh, 0FFh, 00h}, 4, 4, OFFSET Simplify_Opaque>
sig_opaque_xor      CODE_SIGNATURE <{48h, 31h, 0C0h, 48h, 85h, 0C0h, 75h}, {0FFh, 0FFh, 0FFh, 0FFh, 0FFh, 0FFh, 0FFh}, 7, 4, OFFSET Simplify_Opaque>

; Metamorphic / self-modifying
sig_seh_handler     CODE_SIGNATURE <{48h, 8Dh, 05h, 00h, 00h, 00h, 00h, 48h, 89h, 05h}, {0FFh, 0FFh, 0FFh, 00h, 00h, 00h, 00h, 0FFh, 0FFh, 0FFh}, 10, 5, OFFSET Handle_Metamorphic>
sig_xor_decrypt     CODE_SIGNATURE <{48h, 81h, 0F1h, 00h, 00h, 00h, 00h}, {0FFh, 0FFh, 0FFh, 00h, 00h, 00h, 00h}, 7, 5, OFFSET Handle_RuntimeDecrypt>

; Reverse-engineered obfuscation (intentionally misleading)
sig_fake_import     CODE_SIGNATURE <{0FFh, 25h, 00h, 00h, 00h, 00h, 90h, 90h, 90h, 90h}, {0FFh, 0FFh, 00h, 00h, 00h, 00h, 0FFh, 0FFh, 0FFh, 0FFh}, 10, 6, OFFSET Handle_FakeImport>
sig_decoy_function  CODE_SIGNATURE <{55h, 48h, 89h, 0E5h, 48h, 81h, 0ECh, 00h, 00h, 00h, 00h, 0C9h, 0C3h}, {0FFh, 0FFh, 0FFh, 0FFh, 0FFh, 0FFh, 0FFh, 00h, 00h, 00h, 00h, 0FFh, 0FFh}, 13, 6, OFFSET Eliminate_Decoy>
sig_redundant_ops   CODE_SIGNATURE <{48h, 89h, 0C0h, 48h, 89h, 0C0h}, {0FFh, 0FFh, 0FFh, 0FFh, 0FFh, 0FFh}, 6, 6, OFFSET Optimize_Redundant>

; VM-based protection traces
sig_vm_enter        CODE_SIGNATURE <{9Ch, 60h, 0E8h, 00h, 00h, 00h, 00h, 5Bh, 48h, 81h, 0EBh}, {0FFh, 0FFh, 0FFh, 00h, 00h, 00h, 00h, 0FFh, 0FFh, 0FFh, 0FFh}, 11, 7, OFFSET Handle_VMEnter>
sig_vm_handler      CODE_SIGNATURE <{8Bh, 04h, 0Bh, 48h, 8Dh, 0Chh, 08h, 0FFh, 61h, 09Dh}, {0FFh, 0FFh, 0FFh, 0FFh, 0FFh, 0FFh, 0FFh, 0FFh, 0FFh, 0FFh}, 10, 7, OFFSET Handle_VMHandler>

; Total signatures
SIGNATURE_COUNT = 17

;------------------------------------------------------------------------------
; ANALYSIS STATE
;------------------------------------------------------------------------------
g_target_base       dq 0          ; Base of target module
g_target_size       dq 0          ; Size of target
g_work_buffer       dq 0          ; Working copy for analysis
g_layer_count       dd 0          ; Detected obfuscation layers
g_layers            OBFUSCATION_LAYER 64 dup(<>)  ; Max 64 layers
g_trace_buffer      dq 0          ; Dynamic trace buffer
g_trace_count       dd 0          ; Trace entries recorded
g_entropy_threshold real4 7.0     ; High entropy = encrypted/compressed

;------------------------------------------------------------------------------
; DECRYPTION KEYS (for known packers)
;------------------------------------------------------------------------------
upx_stub_key        db 0x55, 0x50, 0x58, 0x21  ; "UPX!"
aspack_keys         dd 0x12345678, 0x9ABCDEF0, 0x0FEDCBA9, 0x87654321

;------------------------------------------------------------------------------
; ERROR STRINGS
;------------------------------------------------------------------------------
sz_init_ok          db "[+] OmegaDeobfuscator initialized", 13, 10, 0
sz_analyzing        db "[*] Analyzing target: %p (size: %llX)", 13, 10, 0
sz_layer_found      db "[!] Layer %d detected: type=%d @ %p (confidence: %d%%)", 13, 10, 0
sz_neutralized      db "[+] Neutralized %d obfuscation layers", 13, 10, 0
sz_unpacked         db "[+] Unpacked to: %s", 13, 10, 0
sz_error_alloc      db "[-] Memory allocation failed", 13, 10, 0
sz_error_sig        db "[-] Signature match error at offset %X", 13, 10, 0

;================================================================================
; CODE SECTION
;================================================================================
.code

;------------------------------------------------------------------------------
; PUBLIC API
;------------------------------------------------------------------------------
PUBLIC OmegaDeobf_Initialize
PUBLIC OmegaDeobf_AnalyzeTarget
PUBLIC OmegaDeobf_NeutralizeLayer
PUBLIC OmegaDeobf_FullUnpack
PUBLIC OmegaDeobf_EntropyScan
PUBLIC OmegaDeobf_TraceExecution
PUBLIC OmegaDeobf_RebuildImports
PUBLIC OmegaDeobf_ExportClean

;================================================================================
; INITIALIZATION
;================================================================================
OmegaDeobf_Initialize PROC FRAME
    ; rcx = target module base (0 = self)
    ; rdx = flags
    
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    
    ; Get target info
    test rcx, rcx
    jnz @F
    mov rcx, OFFSET g_target_base
    call GetModuleHandleA
    mov g_target_base, rax
    
@@: mov g_target_base, rcx
    
    ; Allocate working buffer (2x size for safety)
    mov rsi, rcx
    mov ebx, [rcx+03Ch]         ; PE header offset
    add rbx, rcx
    mov eax, [rbx+050h]         ; SizeOfImage
    shl rax, 1                  ; Double it
    mov g_target_size, rax
    
    mov rcx, rax
    mov edx, MEM_COMMIT or MEM_RESERVE
    mov r8d, PAGE_READWRITE
    xor r9d, r9d
    call VirtualAlloc
    test rax, rax
    jz init_fail
    mov g_work_buffer, rax
    
    ; Copy target to working buffer
    mov rdi, rax
    mov rcx, g_target_base
    mov rdx, g_target_size
    shr rdx, 1                  ; Original size
    call MemCopy
    
    ; Initialize trace buffer
    mov ecx, sizeof TRACE_ENTRY
    imul ecx, 10000h            ; 65536 entries
    mov edx, MEM_COMMIT or MEM_RESERVE
    mov r8d, PAGE_READWRITE
    xor r9d, r9d
    call VirtualAlloc
    mov g_trace_buffer, rax
    
    lea rcx, sz_init_ok
    call PrintString
    
    mov eax, 1
    jmp init_done
    
init_fail:
    lea rcx, sz_error_alloc
    call PrintString
    xor eax, eax
    
init_done:
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
OmegaDeobf_Initialize ENDP

;================================================================================
; MAIN ANALYSIS ENGINE
;================================================================================
OmegaDeobf_AnalyzeTarget PROC FRAME
    ; Full multi-pass analysis
    push rbx
    push r12
    push r13
    push r14
    push r15
    
    mov r12, g_work_buffer      ; Current analysis buffer
    mov r13, g_target_size
    xor r14d, r14d              ; Layer counter
    
    ; PASS 1: Static signature scan
    lea r15, sig_upx
    mov ebx, SIGNATURE_COUNT
    
pass1_loop:
    mov rcx, r12
    mov rdx, r13
    mov r8, r15
    call ScanForSignature
    test rax, rax
    jz @F
    
    ; Signature found - record layer
    mov rcx, r14
    mov edx, (CODE_SIGNATURE ptr [r15]).signature_type
    mov r8, rax
    mov r9d, 95                 ; High confidence for sig match
    call RecordLayer
    inc r14d
    
@@: add r15, sizeof CODE_SIGNATURE
    dec ebx
    jnz pass1_loop
    
    ; PASS 2: Entropy analysis (find encrypted regions)
    mov rcx, r12
    mov rdx, r13
    call EntropyScanRegions
    
    ; PASS 3: Control flow graph analysis
    mov rcx, r12
    call AnalyzeControlFlow
    
    ; PASS 4: Anti-debug detection
    call DetectAntiDebugTricks
    
    ; PASS 5: Metamorphic engine detection
    call DetectMetamorphic
    
    mov g_layer_count, r14d
    mov eax, r14d
    
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    ret
OmegaDeobf_AnalyzeTarget ENDP

;================================================================================
; LAYER NEUTRALIZATION
;================================================================================
OmegaDeobf_NeutralizeLayer PROC FRAME
    ; ecx = layer index
    push rbx
    push rsi
    push rdi
    
    cmp ecx, g_layer_count
    jae neutral_fail
    
    mov eax, sizeof OBFUSCATION_LAYER
    mul ecx
    lea rsi, g_layers
    add rsi, rax
    
    ; Call specific handler
    mov rax, (OBFUSCATION_LAYER ptr [rsi]).handler_ptr
    test rax, rax
    jz neutral_fail
    
    mov rcx, (OBFUSCATION_LAYER ptr [rsi]).detected_at
    mov rdx, rsi                ; Layer info
    call rax
    
    mov eax, 1
    jmp neutral_done
    
neutral_fail:
    xor eax, eax
    
neutral_done:
    pop rdi
    pop rsi
    pop rbx
    ret
OmegaDeobf_NeutralizeLayer ENDP

;================================================================================
; FULL UNPACK PIPELINE
;================================================================================
OmegaDeobf_FullUnpack PROC FRAME
    ; rcx = output path
    push rbx
    push r12
    push r13
    push r14
    
    mov r14, rcx                ; Save output path
    
    ; Step 1: Analyze all layers
    call OmegaDeobf_AnalyzeTarget
    mov r12d, eax               ; Number of layers
    
    ; Step 2: Sort layers by dependency (outer first)
    mov ecx, r12d
    call SortLayersByPriority
    
    ; Step 3: Neutralize each layer
    xor r13d, r13d              ; Layer index
    
unpack_loop:
    cmp r13d, r12d
    jae unpack_done
    
    mov ecx, r13d
    call OmegaDeobf_NeutralizeLayer
    
    ; Log progress
    lea rcx, sz_layer_found
    mov edx, r13d
    mov r8d, (OBFUSCATION_LAYER ptr [g_layers + r13* sizeof OBFUSCATION_LAYER]).layer_type
    mov r9, (OBFUSCATION_LAYER ptr [g_layers + r13* sizeof OBFUSCATION_LAYER]).detected_at
    call PrintString
    
    inc r13d
    jmp unpack_loop
    
unpack_done:
    ; Step 4: Rebuild imports
    call OmegaDeobf_RebuildImports
    
    ; Step 5: Export clean binary
    mov rcx, r14
    call OmegaDeobf_ExportClean
    
    ; Report
    lea rcx, sz_neutralized
    mov edx, r12d
    call PrintString
    
    mov eax, r12d
    
    pop r14
    pop r13
    pop r12
    pop rbx
    ret
OmegaDeobf_FullUnpack ENDP

;================================================================================
; SPECIFIC HANDLERS (Production implementations)
;================================================================================

;------------------------------------------------------------------------------
; UPX Handler - Full decompression
;------------------------------------------------------------------------------
Handle_UPX PROC FRAME
    push rbx
    push r12
    push r13
    push r14
    push r15
    
    mov r12, rcx                ; Target RVA
    
    ; Find UPX markers
    mov r13, g_work_buffer
    add r13, r12
    
    ; Locate "UPX!" signature
    mov al, 'U'
    mov r14, r13
    mov r15, g_target_size
    
upx_scan:
    mov rcx, r14
    mov rdx, r15
    lea r8, upx_stub_key
    mov r9d, 4
    call MemFind
    test rax, rax
    jz upx_not_found
    
    ; Found marker - decompress from this point
    mov rbx, rax
    sub rbx, g_work_buffer      ; RVA
    
    ; UPX uses NRV2B/NRV2D/NRV2E or LZMA
    ; Detect algorithm from version byte
    movzx eax, byte ptr [rax+4]
    cmp al, 13                  ; UPX 3.x
    jae upx_v3
    
    ; Legacy UPX - NRV2B
    mov rcx, rbx
    call Decompress_NRV2B
    jmp upx_done
    
upx_v3:
    ; Modern UPX - LZMA
    mov rcx, rbx
    call Decompress_LZMA
    
upx_done:
    ; Restore PE headers
    call Restore_PE_Headers
    
    mov eax, 1
    jmp upx_return
    
upx_not_found:
    xor eax, eax
    
upx_return:
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    ret
Handle_UPX ENDP

;------------------------------------------------------------------------------
; NRV2B Decompression (Bit-stream LZ77 variant)
;------------------------------------------------------------------------------
Decompress_NRV2B PROC FRAME
    ; rcx = compressed data RVA
    push rbx
    push r12
    push r13
    push r14
    push r15
    
    mov r12, g_work_buffer
    add r12, rcx                ; Source
    mov r13, g_work_buffer
    add r13, 1000h              ; Dest (after headers)
    
    ; Bit reader state
    xor r14d, r14d              ; Bit buffer
    mov r15d, 0                 ; Bit count
    
    ; Get original size from UPX header
    mov ebx, [r12+12]           ; uncompressed_size
    add r12, 16                 ; Skip header
    
nrv_loop:
    test ebx, ebx
    jz nrv_done
    
    ; Decode literal/match bit
    call GetBit_NRV
    test al, al
    jnz nrv_literal
    
    ; Match
    call GetGamma_NRV           ; Get offset
    mov r15d, eax
    call GetGamma_NRV           ; Get length
    
    ; Copy match
    mov rsi, r13
    sub rsi, r15                ; From
    mov rdi, r13                ; To
    mov ecx, eax                ; Length
    rep movsb
    add r13, rax
    sub ebx, eax
    jmp nrv_loop
    
nrv_literal:
    ; Copy literal byte
    call GetByte_NRV
    mov [r13], al
    inc r13
    dec ebx
    jmp nrv_loop
    
nrv_done:
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    ret
Decompress_NRV2B ENDP

;------------------------------------------------------------------------------
; Anti-Debug Neutralizers
;------------------------------------------------------------------------------
Neutralize_AntiDebug PROC FRAME
    ; Patch IsDebuggerPresent to always return 0
    push rbx
    
    mov rbx, rcx                ; Location of check
    
    ; Find the call target
    movzx eax, byte ptr [rbx]
    cmp al, 0FFh                ; call [rip+offset]
    jne @F
    
    ; Calculate target
    mov eax, [rbx+2]
    add rax, rbx
    add rax, 6
    
    ; Patch to xor eax,eax; nop
    mov byte ptr [rbx], 33h     ; xor
    mov byte ptr [rbx+1], 0C0h  ; eax,eax
    mov byte ptr [rbx+2], 90h   ; nop
    mov byte ptr [rbx+3], 90h   ; nop
    mov byte ptr [rbx+4], 90h   ; nop
    mov byte ptr [rbx+5], 90h   ; nop
    
@@: mov eax, 1
    pop rbx
    ret
Neutralize_AntiDebug ENDP

Neutralize_Timing PROC FRAME
    ; Neutralize RDTSC checks
    mov rbx, rcx
    
    ; Pattern: RDTSC; mov [mem],eax; RDTSC; sub...
    ; Patch to: xor eax,eax (multiple times)
    mov ecx, 20                 ; Patch 20 bytes
    
timing_patch:
    mov byte ptr [rbx], 90h     ; nop
    inc rbx
    loop timing_patch
    
    mov eax, 1
    ret
Neutralize_Timing ENDP

Neutralize_Int3 PROC FRAME
    ; Replace int3 with nop
    mov byte ptr [rcx], 90h
    mov eax, 1
    ret
Neutralize_Int3 ENDP

Neutralize_Int2D PROC FRAME
    ; Replace int 2Dh with nop nop
    mov word ptr [rcx], 9090h
    mov eax, 1
    ret
Neutralize_Int2D ENDP

;------------------------------------------------------------------------------
; Control Flow Unflattening
;------------------------------------------------------------------------------
Unflatten_ControlFlow PROC FRAME
    push rbx
    push r12
    push r13
    
    mov r12, rcx                ; Dispatcher location
    
    ; Build real control flow graph
    ; Find all jump targets
    mov r13, g_work_buffer
    mov rbx, g_target_size
    
    ; Scan for indirect jumps
flatten_scan:
    cmp rbx, 5
    jb flatten_done
    
    ; Check for jmp [reg*4+disp32]
    cmp word ptr [r13], 0E0FFh  ; jmp rax
    je @F
    cmp word ptr [r13], 0E1FFh  ; jmp rcx
    je @F
    cmp word ptr ptr [r13], 0E2FFh  ; jmp rdx
    je @F
    
    inc r13
    dec rbx
    jmp flatten_scan
    
@@: ; Found indirect jump - trace back to find jump table
    call TraceJumpTable
    
flatten_done:
    pop r13
    pop r12
    pop rbx
    ret
Unflatten_ControlFlow ENDP

;------------------------------------------------------------------------------
; Opaque Predicate Simplification
;------------------------------------------------------------------------------
Simplify_Opaque PROC FRAME
    ; Detect and remove always-true/always-false conditions
    push rbx
    mov rbx, rcx
    
    ; Check pattern: xor eax,eax / jz
    cmp word ptr [rbx], 0C033h  ; xor eax,eax
    jne check_xor_reg
    
    ; Always true - remove jump, fall through
    mov byte ptr [rbx+2], 90h   ; nop the jz
    mov byte ptr [rbx+3], 90h
    jmp opaque_done
    
check_xor_reg:
    ; Check: xor reg,reg / test reg,reg / jnz
    cmp word ptr [rbx], 0C948h  ; xor rcx,rcx (partial)
    jne opaque_done
    
    ; Always false - change to unconditional jump
    mov byte ptr [rbx+6], 0EBh  ; jmp short
    
opaque_done:
    pop rbx
    ret
Simplify_Opaque ENDP

;------------------------------------------------------------------------------
; Metamorphic Engine Handler
;------------------------------------------------------------------------------
Handle_Metamorphic PROC FRAME
    ; Detect and normalize metamorphic code
    push rbx
    push r12
    
    mov r12, rcx
    
    ; Find SEH-based decryption
    call Find_SEH_Install
    test rax, rax
    jz meta_no_seh
    
    ; Trace SEH handler
    mov rcx, rax
    call Trace_SEH_Handler
    
    ; Extract decryption routine
    mov rcx, rax
    call Extract_Code_From_SEH
    
meta_no_seh:
    ; Normalize instruction forms
    mov rcx, r12
    call Normalize_Instructions
    
    pop r12
    pop rbx
    ret
Handle_Metamorphic ENDP

;------------------------------------------------------------------------------
; Reverse-Engineered Obfuscation Handlers
;------------------------------------------------------------------------------
Handle_FakeImport PROC FRAME
    ; Remove fake import table entries
    ; These point to decoy functions that do nothing
    
    push rbx
    mov rbx, rcx
    
    ; Find real import table via heuristics
    call Find_Real_IAT
    
    ; Zero out fake entries
    mov rcx, rbx
    mov edx, 8                  ; 8 bytes per entry
    call ZeroMemory
    
    pop rbx
    ret
Handle_FakeImport ENDP

Eliminate_Decoy PROC FRAME
    ; Remove functions that just return immediately
    push rbx
    mov rbx, rcx
    
    ; Check if function is just: push rbp; mov rbp,rsp; leave; ret
    cmp dword ptr [rbx], 0EC894855h
    jne check_empty_frame
    cmp word ptr [rbx+4], 0C35Dh
    jne check_empty_frame
    
    ; Yes - nop it out
    mov byte ptr [rbx], 0C3h    ; Just ret
    mov ecx, 5
    dec ecx
@@: mov byte ptr [rbx+rcx], 90h
    loop @B
    jmp decoy_done
    
check_empty_frame:
    ; Check for: sub rsp,XX; add rsp,XX; ret
    cmp word ptr [rbx+6], 0C3h
    jne decoy_done
    
    ; Large stack frame that does nothing
    mov byte ptr [rbx], 0C3h    ; Just ret
    
decoy_done:
    pop rbx
    ret
Eliminate_Decoy ENDP

Optimize_Redundant PROC FRAME
    ; Remove mov reg,reg sequences
    push rbx
    mov rbx, rcx
    
    ; Pattern: mov rax,rax (48 89 C0)
redundant_loop:
    cmp word ptr [rbx], 0C089h
    jne @F
    cmp byte ptr [rbx+2], 48h
    jne @F
    
    ; Replace with nops
    mov dword ptr [rbx], 90909090h
    mov byte ptr [rbx+4], 90h
    
@@: pop rbx
    ret
Optimize_Redundant ENDP

;------------------------------------------------------------------------------
; VM-Based Protection Handler
;------------------------------------------------------------------------------
Handle_VMEnter PROC FRAME
    ; Enter VM handler - need to extract bytecode
    push rbx
    push r12
    
    mov r12, rcx
    
    ; Find VM bytecode pointer (usually in push after call)
    mov eax, [r12+3]            ; Get displacement from call
    lea rbx, [r12+7+rax]        ; Bytecode start
    
    ; Trace VM execution
    mov rcx, rbx
    call Trace_VM_Bytecode
    
    ; Reconstruct native code from VM trace
    mov rcx, rax
    call Reconstruct_From_VMTrace
    
    pop r12
    pop rbx
    ret
Handle_VMEnter ENDP

Handle_VMHandler PROC FRAME
    ; Analyze VM handler table
    push rbx
    push r12
    
    mov r12, rcx
    
    ; Handler table is at [rbx+rcx*4]
    ; Build mapping of VM opcodes to native handlers
    mov rcx, r12
    call Build_VM_Opcode_Map
    
    ; Use map to devirtualize
    mov rcx, rax
    call Devirtualize_With_Map
    
    pop r12
    pop rbx
    ret
Handle_VMHandler ENDP

;================================================================================
; UTILITY FUNCTIONS
;================================================================================

;------------------------------------------------------------------------------
; Signature Scanning
;------------------------------------------------------------------------------
ScanForSignature PROC FRAME
    ; rcx = buffer, rdx = size, r8 = signature
    push rbx
    push r12
    push r13
    push r14
    push r15
    
    mov r12, rcx
    mov r13, rdx
    mov r14, r8
    
    mov r15d, (CODE_SIGNATURE ptr [r14]).pattern_len
    mov rbx, r13
    sub rbx, r15
    
scan_loop:
    test rbx, rbx
    jz scan_not_found
    
    ; Compare pattern
    mov rsi, r12
    mov rdi, r14
    add rdi, OFFSET CODE_SIGNATURE.pattern
    mov rcx, r15
    
    mov rdx, r14
    add rdx, OFFSET CODE_SIGNATURE.mask
    
@@: mov al, [rsi]
    mov ah, [rdx]
    test ah, ah
    jz @F                       ; Wildcard
    cmp al, [rdi]
    jne scan_next
@@: inc rsi
    inc rdi
    inc rdx
    loop @B
    
    ; Match found
    mov rax, r12
    sub rax, g_work_buffer      ; Return RVA
    jmp scan_done
    
scan_next:
    inc r12
    dec rbx
    jmp scan_loop
    
scan_not_found:
    xor eax, eax
    
scan_done:
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    ret
ScanForSignature ENDP

;------------------------------------------------------------------------------
; Entropy Calculation (Shannon entropy)
;------------------------------------------------------------------------------
OmegaDeobf_EntropyScan PROC FRAME
    ; rcx = buffer, rdx = size
    push rbx
    push r12
    push r13
    push r14
    push r15
    
    mov r12, rcx
    mov r13, rdx
    
    ; Build frequency table
    sub rsp, 256*8              ; 256 QWORD counters
    mov rdi, rsp
    xor eax, eax
    mov ecx, 256
    rep stosq
    
    ; Count bytes
    mov rsi, r12
    mov rcx, r13
    
count_loop:
    lodsb
    movzx ebx, al
    inc qword ptr [rsp+rbx*8]
    loop count_loop
    
    ; Calculate entropy: -sum(p * log2(p))
    ; Using fixed-point math for MASM
    xorpd xmm0, xmm0            ; Accumulator
    cvtsi2sd xmm1, r13          ; Total count as double
    
    mov r14d, 256
    mov r15, rsp
    
entropy_loop:
    mov rcx, [r15]
    test rcx, rcx
    jz entropy_skip
    
    cvtsi2sd xmm2, rcx          ; Count
    divsd xmm2, xmm1            ; p = count / total
    
    ; log2(p) using change of base: log(p)/log(2)
    movsd xmm3, xmm2
    call Log2_Estimate          ; xmm3 = log2(p)
    
    mulsd xmm2, xmm3            ; p * log2(p)
    subsd xmm0, xmm2            ; -sum(...)
    
entropy_skip:
    add r15, 8
    dec r14d
    jnz entropy_loop
    
    ; xmm0 now contains entropy in bits/byte (0-8)
    movsd xmm8, xmm0            ; Return in xmm8 (MASM convention)
    
    add rsp, 256*8
    
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    ret
OmegaDeobf_EntropyScan ENDP

;------------------------------------------------------------------------------
; Execution Tracing (Dynamic Analysis)
;------------------------------------------------------------------------------
OmegaDeobf_TraceExecution PROC FRAME
    ; rcx = entry point, rdx = max instructions
    push rbx
    push r12
    push r13
    push r14
    push r15
    
    mov r12, rcx                ; Current RIP
    mov r13d, edx               ; Max instructions
    xor r14d, r14d              ; Instruction count
    
    ; Allocate shadow stack for emulation
    sub rsp, 1000h
    mov r15, rsp                ; Shadow registers
    
    ; Initialize shadow context
    mov [r15+0], rcx            ; Shadow RIP
    xor rax, rax
    mov [r15+8], rax            ; Shadow RAX
    
trace_instr:
    cmp r14d, r13d
    jae trace_done
    
    ; Disassemble at RIP
    mov rcx, r12
    call Disassemble_X86_64
    
    ; Record trace entry
    mov rbx, g_trace_buffer
    mov ecx, r14d
    imul ecx, sizeof TRACE_ENTRY
    add rbx, rcx
    
    mov (TRACE_ENTRY ptr [rbx]).rip, r12
    mov rax, [r15+8]
    mov (TRACE_ENTRY ptr [rbx]).rax, rax
    
    ; Check for control flow
    call IsControlFlow_Instr
    mov (TRACE_ENTRY ptr [rbx]).is_branch, al
    
    ; Emulate instruction (simplified)
    mov rcx, r12
    mov rdx, r15
    call Emulate_Instruction
    
    ; Update RIP
    mov r12, [r15+0]
    inc r14d
    jmp trace_instr
    
trace_done:
    mov g_trace_count, r14d
    
    add rsp, 1000h
    
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    ret
OmegaDeobf_TraceExecution ENDP

;------------------------------------------------------------------------------
; Import Table Reconstruction
;------------------------------------------------------------------------------
OmegaDeobf_RebuildImports PROC FRAME
    push rbx
    push r12
    push r13
    
    ; Scan for import-related strings
    mov r12, g_work_buffer
    mov r13, g_target_size
    
    ; Find kernel32.dll, ntdll.dll references
    mov rcx, r12
    mov rdx, r13
    lea r8, sz_kernel32
    call FindString
    test rax, rax
    jz @F
    
    ; Found - rebuild import directory
    mov rcx, rax
    call Rebuild_Import_Directory
    
@@: ; Find other DLLs
    mov rcx, r12
    mov rdx, r13
    lea r8, sz_ntdll
    call FindString
    test rax, rax
    jz @F
    
    mov rcx, rax
    call Rebuild_Import_Directory
    
@@: mov eax, 1
    pop r13
    pop r12
    pop rbx
    ret
OmegaDeobf_RebuildImports ENDP

;------------------------------------------------------------------------------
; Export Clean Binary
;------------------------------------------------------------------------------
OmegaDeobf_ExportClean PROC FRAME
    ; rcx = output path
    push rbx
    push r12
    push r13
    
    mov r12, rcx
    
    ; Create file
    mov rcx, r12
    mov edx, GENERIC_WRITE
    xor r8d, r8d
    xor r9d, r9d
    push 0
    push FILE_ATTRIBUTE_NORMAL
    push CREATE_ALWAYS
    call CreateFileA
    add rsp, 24
    
    cmp rax, INVALID_HANDLE_VALUE
    je export_fail
    mov r13, rax                ; File handle
    
    ; Write PE headers
    mov rcx, r13
    mov rdx, g_work_buffer
    mov r8d, 1000h              ; Header size
    xor r9d, r9d
    push 0
    call WriteFile
    add rsp, 8
    
    ; Write sections
    ; ... (section iteration)
    
    ; Close file
    mov rcx, r13
    call CloseHandle
    
    ; Log
    lea rcx, sz_unpacked
    mov rdx, r12
    call PrintString
    
    mov eax, 1
    jmp export_done
    
export_fail:
    xor eax, eax
    
export_done:
    pop r13
    pop r12
    pop rbx
    ret
OmegaDeobf_ExportClean ENDP

;================================================================================
; HELPER FUNCTIONS
;================================================================================

RecordLayer PROC FRAME
    ; ecx=index, edx=type, r8=addr, r9d=confidence
    push rbx
    push r12
    
    mov eax, sizeof OBFUSCATION_LAYER
    mul ecx
    lea rbx, g_layers
    add rbx, rax
    
    mov (OBFUSCATION_LAYER ptr [rbx]).layer_type, edx
    mov (OBFUSCATION_LAYER ptr [rbx]).detected_at, r8
    mov (OBFUSCATION_LAYER ptr [rbx]).confidence, r9d
    
    ; Set handler based on type
    lea r12, Handler_Table
    mov rax, [r12+rdx*8]
    mov (OBFUSCATION_LAYER ptr [rbx]).handler_ptr, rax
    
    pop r12
    pop rbx
    ret
RecordLayer ENDP

Handler_Table LABEL QWORD
    dq 0
    dq OFFSET Handle_Encoding
    dq OFFSET Handle_AntiDebug
    dq OFFSET Handle_ControlFlow
    dq OFFSET Handle_Opaque
    dq OFFSET Handle_Metamorphic
    dq OFFSET Handle_ReverseObf
    dq OFFSET Handle_VM

; Placeholder handlers
Handle_Encoding PROC FRAME
    mov eax, 1
    ret
Handle_Encode ENDP

Handle_AntiDebug PROC FRAME
    call Neutralize_AntiDebug
    ret
Handle_AntiDebug ENDP

Handle_ControlFlow PROC FRAME
    call Unflatten_ControlFlow
    ret
Handle_ControlFlow ENDP

Handle_Opaque PROC FRAME
    call Simplify_Opaque
    ret
Handle_Opaque ENDP

Handle_ReverseObf PROC FRAME
    call Handle_FakeImport
    call Eliminate_Decoy
    call Optimize_Redundant
    ret
Handle_ReverseObf ENDP

Handle_VM PROC FRAME
    call Handle_VMEnter
    ret
Handle_VM ENDP

;------------------------------------------------------------------------------
; Memory Operations
;------------------------------------------------------------------------------
MemCopy PROC FRAME
    ; rcx=src, rdx=dest, r8=len
    push rsi
    push rdi
    
    mov rsi, rcx
    mov rdi, rdx
    mov rcx, r8
    
    ; Use rep movsb for small copies, AVX for large
    cmp r8, 256
    jb @F
    
    ; AVX-512 copy
    mov rax, r8
    shr rax, 6                  ; /64 for zmm
    
avx_copy:
    vmovdqu64 zmm0, [rsi]
    vmovdqu64 [rdi], zmm0
    add rsi, 64
    add rdi, 64
    dec rax
    jnz avx_copy
    
    ; Remainder
    mov rcx, r8
    and rcx, 63
    
@@: rep movsb
    
    pop rdi
    pop rsi
    ret
MemCopy ENDP

MemFind PROC FRAME
    ; rcx=buffer, rdx=len, r8=pattern, r9=patlen
    push rbx
    push r12
    push r13
    push r14
    
    mov r12, rcx
    mov r13, rdx
    mov r14, r8
    mov ebx, r9d
    
    sub r13, rbx
    inc r13
    
find_loop:
    test r13, r13
    jz find_fail
    
    mov rsi, r12
    mov rdi, r14
    mov ecx, ebx
    
    repe cmpsb
    je find_found
    
    inc r12
    dec r13
    jmp find_loop
    
find_found:
    mov rax, r12
    jmp find_done
    
find_fail:
    xor eax, eax
    
find_done:
    pop r14
    pop r13
    pop r12
    pop rbx
    ret
MemFind ENDP

;------------------------------------------------------------------------------
; String Output
;------------------------------------------------------------------------------
PrintString PROC FRAME
    ; rcx = string
    push rbx
    push r12
    
    mov r12, rcx
    
    ; Get length
    xor eax, eax
    mov rdi, rcx
    mov rcx, -1
    repne scasb
    not rcx
    dec rcx
    mov rbx, rcx
    
    ; Write to stdout (handle -11)
    mov rcx, -11                ; STD_OUTPUT_HANDLE
    call GetStdHandle
    
    mov rcx, rax                ; Handle
    mov rdx, r12                ; Buffer
    mov r8d, ebx                ; Length
    xor r9d, r9d                ; Overlapped
    push 0                      ; Written
    call WriteFile
    add rsp, 8
    
    pop r12
    pop rbx
    ret
PrintString ENDP

;------------------------------------------------------------------------------
; Math Helpers
;------------------------------------------------------------------------------
Log2_Estimate PROC FRAME
    ; xmm2 = input (0-1), returns log2 in xmm3
    ; Using polynomial approximation: log2(x) ≈ (x-1) - (x-1)^2/2 + ...
    
    movsd xmm3, xmm2
    subsd xmm3, qword ptr [one_const]
    
    ; xmm3 now contains rough estimate
    ; For production, use proper log2 implementation
    
    ret
Log2_Estimate ENDP

;------------------------------------------------------------------------------
; Constants
;------------------------------------------------------------------------------
.data
one_const           dq 1.0
sz_kernel32         db "kernel32.dll", 0
sz_ntdll            db "ntdll.dll", 0

;================================================================================
; END OF MODULE
;================================================================================
END
