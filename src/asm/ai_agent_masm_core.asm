OPTION CASEMAP:NONE

EXTERN QueryPerformanceCounter:PROC

CPU_FEATURE_AVX512 EQU 1
CPU_FEATURE_AVX2   EQU 2
CPU_FEATURE_SSE41  EQU 4

CMD_ID_UNKNOWN         EQU 0
CMD_ID_OPEN_FILE       EQU 1
CMD_ID_RUN_COMMAND     EQU 2
CMD_ID_ANALYZE_FILE    EQU 3
CMD_ID_SEARCH_CODE     EQU 4
CMD_ID_GENERATE_TESTS  EQU 5
CMD_ID_REFACTOR_CODE   EQU 6
CMD_ID_SWITCH_PROVIDER EQU 7
CMD_ID_INSTALL_EXT     EQU 8
CMD_ID_CREATE_FILE     EQU 9

.data
szActOpenFile      db "open-file",0
szActRunCommand    db "run-command",0
szActAnalyzeFile   db "analyze-file",0
szActSearchCode    db "search-code",0
szActGenerateTests db "generate-tests",0
szActRefactorCode  db "refactor-code",0
szActSwitchProv    db "switch-provider",0
szActInstallExt    db "install-extension",0
szActCreateFile    db "create-file",0

.code

PUBLIC rawrxd_recovery_stub_ai_agent_masm_core
PUBLIC masm_get_performance_counter
PUBLIC masm_get_cpu_features
PUBLIC masm_memory_scan_pattern_avx512
PUBLIC masm_parse_slash_command

; Compatibility entry point retained for older recovery probes.
rawrxd_recovery_stub_ai_agent_masm_core PROC FRAME
    .endprolog
    mov eax, 1
    ret
rawrxd_recovery_stub_ai_agent_masm_core ENDP

; uint64_t masm_parse_slash_command(const char* input, char* action_out, char* arg_out, uint64_t arg_cap)
; RCX=input, RDX=action_out (recommended >= 32 bytes), R8=arg_out, R9=arg_cap
; Returns command id (CMD_ID_*) or 0 if not a recognized slash command.
masm_parse_slash_command PROC FRAME
    push rbx
    .pushreg rbx
    push rsi
    .pushreg rsi
    push rdi
    .pushreg rdi
    sub rsp, 40h
    .allocstack 40h
    .endprolog

    ; Clear outputs eagerly so callers never read stale buffers.
    test rdx, rdx
    jz psc_clear_arg_only
    mov byte ptr [rdx], 0
psc_clear_arg_only:
    test r8, r8
    jz psc_invalid
    test r9, r9
    jz psc_invalid
    mov byte ptr [r8], 0

    test rcx, rcx
    jz psc_invalid
    mov al, byte ptr [rcx]
    cmp al, '/'
    jne psc_invalid

    lea rsi, [rcx+1]              ; after slash

psc_skip_lead_ws:
    mov al, byte ptr [rsi]
    cmp al, ' '
    je psc_lead_ws_advance
    cmp al, 9
    je psc_lead_ws_advance
    jmp psc_token_begin
psc_lead_ws_advance:
    inc rsi
    jmp psc_skip_lead_ws

psc_token_begin:
    mov rdi, rsp                  ; token buffer base (64 bytes)
    xor ebx, ebx                  ; token length

psc_token_loop:
    mov al, byte ptr [rsi]
    test al, al
    jz psc_token_done
    cmp al, ' '
    je psc_token_done
    cmp al, 9
    je psc_token_done
    cmp bl, 3Fh
    jae psc_token_overflow_consume
    cmp al, 'A'
    jb psc_store_char
    cmp al, 'Z'
    ja psc_store_char
    or al, 20h                    ; to lower
psc_store_char:
    mov byte ptr [rdi+rbx], al
    inc bl
psc_token_overflow_consume:
    inc rsi
    jmp psc_token_loop

psc_token_done:
    mov byte ptr [rdi+rbx], 0

    xor eax, eax                  ; default unknown
    xor r10d, r10d                ; action string pointer

    cmp bl, 4
    jne psc_chk_len3
    cmp dword ptr [rdi], 'nepo'   ; "open"
    jne psc_chk_len3
    mov eax, CMD_ID_OPEN_FILE
    lea r10, szActOpenFile
    jmp psc_dispatch_selected

psc_chk_len3:
    cmp bl, 3
    jne psc_chk_len7
    cmp dword ptr [rdi], 'nur'    ; "run\0" low bytes compare works with token zero-terminated
    jne psc_chk_len7
    mov eax, CMD_ID_RUN_COMMAND
    lea r10, szActRunCommand
    jmp psc_dispatch_selected

psc_chk_len7:
    cmp bl, 7
    jne psc_chk_len6
    mov ecx, dword ptr [rdi]
    cmp ecx, 'alpx'               ; "xpla" little-endian piece for "explain"
    jne psc_chk_search
    cmp dword ptr [rdi+3], 'nial' ; overlaps to reduce compares
    jne psc_chk_search
    cmp byte ptr [rdi+6], 'n'
    jne psc_chk_search
    mov eax, CMD_ID_ANALYZE_FILE
    lea r10, szActAnalyzeFile
    jmp psc_dispatch_selected

psc_chk_search:
    cmp dword ptr [rdi], 'raes'   ; "sear"
    jne psc_chk_install
    cmp dword ptr [rdi+3], 'hcr'  ; overlap for "earch"
    jne psc_chk_install
    mov eax, CMD_ID_SEARCH_CODE
    lea r10, szActSearchCode
    jmp psc_dispatch_selected

psc_chk_install:
    cmp bl, 7
    jne psc_chk_len6
    cmp dword ptr [rdi], 'tsni'   ; "inst"
    jne psc_chk_len6
    cmp dword ptr [rdi+3], 'lla'  ; overlap for "stall"
    jne psc_chk_len6
    cmp byte ptr [rdi+6], 'l'
    jne psc_chk_len6
    mov eax, CMD_ID_INSTALL_EXT
    lea r10, szActInstallExt
    jmp psc_dispatch_selected

psc_chk_len6:
    cmp bl, 6
    jne psc_chk_len5
    cmp dword ptr [rdi], 'tset'   ; "test"
    jne psc_chk_switch
    mov eax, CMD_ID_GENERATE_TESTS
    lea r10, szActGenerateTests
    jmp psc_dispatch_selected

psc_chk_switch:
    cmp dword ptr [rdi], 'iws'    ; "swi"
    jne psc_chk_len5
    cmp dword ptr [rdi+2], 'hct'  ; overlap for "witch"
    jne psc_chk_len5
    mov eax, CMD_ID_SWITCH_PROVIDER
    lea r10, szActSwitchProv
    jmp psc_dispatch_selected

psc_chk_len5:
    cmp bl, 5
    jne psc_chk_len8
    cmp dword ptr [rdi], 'aerc'   ; "crea"
    jne psc_unknown
    cmp byte ptr [rdi+4], 't'
    jne psc_unknown
    cmp byte ptr [rdi+5], 'e'
    jne psc_unknown
    mov eax, CMD_ID_CREATE_FILE
    lea r10, szActCreateFile
    jmp psc_dispatch_selected

psc_chk_len8:
    cmp bl, 8
    jne psc_unknown
    cmp dword ptr [rdi], 'afer'   ; "refa"
    jne psc_unknown
    cmp dword ptr [rdi+4], 'tcor' ; "rcto" overlap for "refactor"
    jne psc_unknown
    mov eax, CMD_ID_REFACTOR_CODE
    lea r10, szActRefactorCode
    jmp psc_dispatch_selected

psc_unknown:
    xor eax, eax
    jmp psc_done

psc_dispatch_selected:
    ; Copy action text if caller provided buffer.
    test rdx, rdx
    jz psc_copy_arg
    mov r11, rdx
psc_copy_action_loop:
    mov dl, byte ptr [r10]
    mov byte ptr [r11], dl
    inc r10
    inc r11
    test dl, dl
    jnz psc_copy_action_loop

psc_copy_arg:
    ; Skip whitespace between command token and argument tail.
psc_skip_arg_ws:
    mov dl, byte ptr [rsi]
    cmp dl, ' '
    je psc_arg_ws_advance
    cmp dl, 9
    je psc_arg_ws_advance
    jmp psc_arg_copy_begin
psc_arg_ws_advance:
    inc rsi
    jmp psc_skip_arg_ws

psc_arg_copy_begin:
    test r9, r9
    jz psc_done
    mov rdi, r8
    mov r11, r9
    dec r11                        ; room for null terminator
    js psc_done
psc_arg_copy_loop:
    cmp r11, 0
    je psc_arg_terminate
    mov dl, byte ptr [rsi]
    test dl, dl
    jz psc_arg_terminate
    mov byte ptr [rdi], dl
    inc rdi
    inc rsi
    dec r11
    jmp psc_arg_copy_loop

psc_arg_terminate:
    mov byte ptr [rdi], 0
    jmp psc_done

psc_invalid:
    xor eax, eax

psc_done:
    add rsp, 40h
    pop rdi
    pop rsi
    pop rbx
    ret
masm_parse_slash_command ENDP

; uint64_t masm_get_performance_counter(void)
masm_get_performance_counter PROC FRAME
    sub rsp, 28h
    .allocstack 28h
    .endprolog

    lea rcx, [rsp+20h]
    call QueryPerformanceCounter
    test eax, eax
    jz perf_counter_failed

    mov rax, qword ptr [rsp+20h]
    add rsp, 28h
    ret

perf_counter_failed:
    xor eax, eax
    add rsp, 28h
    ret
masm_get_performance_counter ENDP

; uint64_t masm_get_cpu_features(void)
masm_get_cpu_features PROC FRAME
    push rbx
    .pushreg rbx
    .endprolog

    xor r10d, r10d

    mov eax, 0
    cpuid
    mov r11d, eax

    cmp r11d, 1
    jb cpu_features_done

    mov eax, 1
    xor ecx, ecx
    cpuid
    bt ecx, 19
    jnc check_leaf7
    or r10d, CPU_FEATURE_SSE41

check_leaf7:
    cmp r11d, 7
    jb cpu_features_done

    mov eax, 7
    xor ecx, ecx
    cpuid
    bt ebx, 5
    jnc check_avx512
    or r10d, CPU_FEATURE_AVX2

check_avx512:
    bt ebx, 16
    jnc cpu_features_done
    or r10d, CPU_FEATURE_AVX512

cpu_features_done:
    mov eax, r10d
    pop rbx
    ret
masm_get_cpu_features ENDP

; uint64_t masm_memory_scan_pattern_avx512(const void* buffer, size_t buffer_size, const MasmBytePattern* pattern)
; Scalar implementation for now; exported from MASM so the live bridge no longer depends on the C++ scaffold.
; RCX=buffer, RDX=buffer_size, R8=pattern
masm_memory_scan_pattern_avx512 PROC FRAME
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
    push r15
    .pushreg r15
    .endprolog

    xor eax, eax
    test rcx, rcx
    jz scan_return
    test r8, r8
    jz scan_return

    mov r11, qword ptr [r8+8]      ; pattern_length
    test r11, r11
    jz zero_matches
    cmp rdx, r11
    jb zero_matches

    mov r9, qword ptr [r8]         ; pattern bytes
    test r9, r9
    jz zero_matches

    mov r10, qword ptr [r8+18h]    ; mask pointer
    mov r12, rcx                   ; buffer base
    mov r13, rdx                   ; buffer size
    sub r13, r11
    inc r13                        ; scan limit count
    xor r14, r14                   ; match count
    xor r15, r15                   ; outer index

outer_loop:
    cmp r15, r13
    jae scan_done

    xor rbx, rbx                   ; inner index

inner_loop:
    cmp rbx, r11
    jae matched_pattern

    test r10, r10
    jz compare_byte

    mov al, byte ptr [r10+rbx]
    cmp al, '?'
    je next_byte

compare_byte:
    lea rsi, [r12+r15]
     mov al, byte ptr [rsi+rbx]
    cmp al, byte ptr [r9+rbx]
    jne no_match

next_byte:
    inc rbx
    jmp inner_loop

matched_pattern:
    inc r14

no_match:
    inc r15
    jmp outer_loop

zero_matches:
    xor r14, r14

scan_done:
    mov qword ptr [r8+20h], r14    ; pattern->matches_found
    mov rax, r14

scan_return:
    pop r15
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
masm_memory_scan_pattern_avx512 ENDP

END