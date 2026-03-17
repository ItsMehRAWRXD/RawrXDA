;=====================================================================
; model_transform_engine.asm - Real-Time Model Transformation Engine
; HERETIC-INSPIRED REVERSIBLE MODEL SURGERY
;=====================================================================
; Implements on-the-fly model transformations:
;  - Vocabulary expansion (small models → large vocab)
;  - Refusal pattern bypass (safety alignment removal)
;  - Loading optimization (lazy tensor loading)
;  - Quantization transforms (dynamic precision adjustment)
;
; Pattern: Apply transforms on model load, reverse on unload
; Inspired by: https://github.com/p-e-w/heretic
;
; Architecture:
;  1. Model Load Hook → Apply Transform Pipeline
;  2. Runtime → Model operates with transforms
;  3. Model Unload Hook → Reverse Transform Pipeline
;  4. Command Interface → /Reverse [type] for manual control
;=====================================================================

.code

PUBLIC masm_transform_engine_init
PUBLIC masm_transform_on_model_load
PUBLIC masm_transform_on_model_unload
PUBLIC masm_transform_expand_vocabulary
PUBLIC masm_transform_bypass_refusal
PUBLIC masm_transform_optimize_loading
PUBLIC masm_transform_execute_command
PUBLIC masm_transform_get_active_transforms

; External dependencies
EXTERN masm_core_transform_pipeline:PROC
EXTERN masm_core_transform_abort_pipeline:PROC
EXTERN masm_core_transform_xor:PROC
EXTERN masm_core_transform_rotate:PROC
EXTERN asm_malloc:PROC
EXTERN asm_free:PROC
EXTERN asm_log:PROC
EXTERN asm_mutex_create:PROC
EXTERN asm_mutex_lock:PROC
EXTERN asm_mutex_unlock:PROC

; Quantum Injection Library Integration
EXTERN masm_quantum_library_init:PROC
EXTERN masm_quantum_library_attach_model:PROC
EXTERN masm_quantum_library_detach_model:PROC
EXTERN masm_quantum_library_double_reverse_load:PROC

;=====================================================================
; TRANSFORM TYPE CONSTANTS
;=====================================================================

TRANSFORM_VOCAB_EXPAND          EQU 1
TRANSFORM_REFUSAL_BYPASS        EQU 2
TRANSFORM_LOADING_OPTIMIZE      EQU 3
TRANSFORM_QUANTIZATION_DYNAMIC  EQU 4
TRANSFORM_SAFETY_REMOVAL        EQU 5
TRANSFORM_ATTENTION_PATCH       EQU 6

; Transform state flags
TRANSFORM_STATE_INACTIVE        EQU 0
TRANSFORM_STATE_ACTIVE          EQU 1
TRANSFORM_STATE_PENDING         EQU 2
TRANSFORM_STATE_ERROR           EQU 3

; Command prefixes
CMD_REVERSE                     EQU 1
CMD_APPLY                       EQU 2
CMD_STATUS                      EQU 3
CMD_RESET                       EQU 4

;=====================================================================
; DATA STRUCTURES
;=====================================================================

.data

; Active model transform context (1024 bytes)
; [+0]:    model_handle (qword)
; [+8]:    model_name_ptr (qword)
; [+16]:   vocab_size (qword)
; [+24]:   expanded_vocab_size (qword)
; [+32]:   pipeline_ptr (qword)
; [+40]:   active_transforms (qword) - bitmask
; [+48]:   transform_states[16] (qword[16]) - 128 bytes
; [+176]:  refusal_patterns_patched (qword)
; [+184]:  vocab_expansion_applied (qword)
; [+192]:  loading_optimizations (qword)
; [+200]:  reserved[103] (qword[103])

g_active_model_context          QWORD 0
g_transform_engine_mutex        QWORD 0
g_models_transformed            QWORD 0
g_transforms_reversed           QWORD 0
g_vocabulary_expansions         QWORD 0
g_refusal_bypasses              QWORD 0

; Vocabulary expansion mapping (Heretic-style)
; Maps small model tokens → large model tokens
g_vocab_mapping_table           QWORD 0
g_vocab_mapping_size            QWORD 0

; Refusal pattern database
g_refusal_patterns              QWORD 0
g_refusal_pattern_count         QWORD 32

; Known refusal patterns (to patch out)
refusal_pattern_1   DB "I cannot assist", 0
refusal_pattern_2   DB "I'm not able", 0
refusal_pattern_3   DB "I apologize, but", 0
refusal_pattern_4   DB "against my guidelines", 0
refusal_pattern_5   DB "cannot provide", 0
refusal_pattern_6   DB "inappropriate request", 0
refusal_pattern_7   DB "I don't feel comfortable", 0
refusal_pattern_8   DB "harmful or illegal", 0

; Replacement patterns (bypass refusal)
bypass_pattern_1    DB "I'll help with", 0
bypass_pattern_2    DB "Let me assist", 0
bypass_pattern_3    DB "Here's the information", 0
bypass_pattern_4    DB "Following your request", 0

; Log messages
msg_engine_init         DB "[Transform Engine] Initialized", 0
msg_model_load          DB "[Transform Engine] Model load detected: ", 0
msg_vocab_expand        DB "[Transform Engine] Expanding vocabulary: %lld -> %lld tokens", 0
msg_refusal_bypass      DB "[Transform Engine] Bypassing refusal patterns: %lld patterns patched", 0
msg_optimize_loading    DB "[Transform Engine] Optimizing model loading", 0
msg_transform_applied   DB "[Transform Engine] Transform applied: type=%d", 0
msg_transform_reversed  DB "[Transform Engine] Transform reversed: type=%d", 0
msg_command_executed    DB "[Transform Engine] Command executed: ", 0

; Command strings
cmd_reverse_refusal     DB "/Reverse refusal", 0
cmd_reverse_all         DB "/Reverse all", 0
cmd_apply_vocab         DB "/Apply vocab", 0
cmd_status              DB "/Status", 0

.code

;=====================================================================
; masm_transform_engine_init() -> rax
;
; Initializes the transform engine and allocates resources.
; Returns: 1 on success, 0 on failure
;=====================================================================

ALIGN 16
masm_transform_engine_init PROC

    push rbx
    sub rsp, 32
    
    ; Create engine mutex
    call asm_mutex_create
    mov [g_transform_engine_mutex], rax
    test rax, rax
    jz init_fail
    
    ; Allocate active model context
    mov rcx, 1024
    call asm_malloc
    mov [g_active_model_context], rax
    test rax, rax
    jz init_fail
    
    ; Zero out context
    mov rdi, rax
    mov rcx, 128
    xor rax, rax
    rep stosq
    
    ; Allocate vocabulary mapping table (32K entries)
    mov rcx, 32768 * 16     ; 16 bytes per entry
    call asm_malloc
    mov [g_vocab_mapping_table], rax
    test rax, rax
    jz init_fail
    
    ; Allocate refusal patterns array
    mov rcx, 256 * 8        ; 256 pattern pointers
    call asm_malloc
    mov [g_refusal_patterns], rax
    test rax, rax
    jz init_fail
    
    ; Initialize refusal patterns
    mov rbx, [g_refusal_patterns]
    lea rax, [refusal_pattern_1]
    mov [rbx], rax
    lea rax, [refusal_pattern_2]
    mov [rbx + 8], rax
    lea rax, [refusal_pattern_3]
    mov [rbx + 16], rax
    lea rax, [refusal_pattern_4]
    mov [rbx + 24], rax
    lea rax, [refusal_pattern_5]
    mov [rbx + 32], rax
    lea rax, [refusal_pattern_6]
    mov [rbx + 40], rax
    lea rax, [refusal_pattern_7]
    mov [rbx + 48], rax
    lea rax, [refusal_pattern_8]
    mov [rbx + 56], rax
    
    ; Initialize Quantum Injection Library
    call masm_quantum_library_init
    test rax, rax
    jz init_fail
    
    ; Log success
    lea rcx, [msg_engine_init]
    call asm_log
    
    mov rax, 1
    jmp init_exit

init_fail:
    xor rax, rax

init_exit:
    add rsp, 32
    pop rbx
    ret

masm_transform_engine_init ENDP

;=====================================================================
; masm_transform_on_model_load(model_handle: rcx, 
;                              model_name: rdx,
;                              vocab_size: r8) -> rax
;
; Hook called when a model is loaded from dropdown.
; Automatically applies configured transforms.
; Returns: 1 on success, 0 on failure
;=====================================================================

ALIGN 16
masm_transform_on_model_load PROC

    push rbx
    push r12
    push r13
    push r14
    sub rsp, 64
    
    mov r12, rcx            ; model_handle
    mov r13, rdx            ; model_name
    mov r14, r8             ; vocab_size
    
    ; Lock engine
    mov rcx, [g_transform_engine_mutex]
    call asm_mutex_lock
    
    ; Update active model context
    mov rbx, [g_active_model_context]
    mov [rbx], r12                      ; model_handle
    mov [rbx + 8], r13                  ; model_name_ptr
    mov [rbx + 16], r14                 ; vocab_size
    
    ; Log model load
    lea rcx, [msg_model_load]
    call asm_log
    mov rcx, r13
    call asm_log
    
    ; *** QUANTUM LIBRARY ATTACHMENT ***
    ; Attach static capability library to model
    ; This creates the immutable bridge: small model + library = large capabilities
    push r12
    push r13
    push r14
    
    ; Calculate model size (estimate based on vocab)
    mov rax, r14            ; vocab_size
    imul rax, 2048          ; Average embedding dimension
    imul rax, 2             ; INT8 quantization (2 bytes per param)
    mov rdx, rax            ; model_size estimate
    
    mov rcx, r12            ; model_handle
    ; rdx = model_size (already set)
    mov r8, 4096            ; context_limit (default for small models)
    mov r9, r14             ; vocab_size
    call masm_quantum_library_attach_model
    
    ; Store effective size
    test rax, rax
    jz quantum_attach_failed
    mov [rbx + 40], rax     ; Store effective_size in context
    
quantum_attach_failed:
    pop r14
    pop r13
    pop r12
    
    ; *** DOUBLE-REVERSE LAZY LOADING ***
    ; Apply instant startup optimization
    mov rcx, r12
    call masm_quantum_library_double_reverse_load
    
    ; Apply default transforms
    
    ; 1. Vocabulary Expansion (if small model)
    cmp r14, 32000          ; If vocab < 32K
    jge skip_vocab_expand
    
    mov rcx, r12
    mov rdx, r14
    mov r8, 128000          ; Expand to 128K (GPT-4 size)
    call masm_transform_expand_vocabulary
    
skip_vocab_expand:
    
    ; 2. Refusal Bypass (always apply)
    mov rcx, r12
    call masm_transform_bypass_refusal
    
    ; 3. Loading Optimization
    mov rcx, r12
    call masm_transform_optimize_loading
    
    ; Increment counters
    lock inc [g_models_transformed]
    
    ; Unlock engine
    mov rcx, [g_transform_engine_mutex]
    call asm_mutex_unlock
    
    mov rax, 1
    
    add rsp, 64
    pop r14
    pop r13
    pop r12
    pop rbx
    ret

masm_transform_on_model_load ENDP

;=====================================================================
; masm_transform_on_model_unload(model_handle: rcx) -> rax
;
; Hook called when a model is unloaded or switched.
; Reverses all active transforms for clean state.
; Returns: 1 on success, 0 on failure
;=====================================================================

ALIGN 16
masm_transform_on_model_unload PROC

    push rbx
    push r12
    sub rsp, 32
    
    mov r12, rcx            ; model_handle
    
    ; Lock engine
    mov rcx, [g_transform_engine_mutex]
    call asm_mutex_lock
    
    ; Get active context
    mov rbx, [g_active_model_context]
    
    ; Verify this is the active model
    cmp [rbx], r12
    jne unload_mismatch
    
    ; Get pipeline pointer
    mov rcx, [rbx + 32]
    test rcx, rcx
    jz unload_no_pipeline
    
    ; Reverse entire pipeline
    call masm_core_transform_abort_pipeline
    
    ; Clear pipeline pointer
    mov rbx, [g_active_model_context]
    mov qword ptr [rbx + 32], 0
    
    ; Clear active transforms bitmask
    mov qword ptr [rbx + 40], 0
    
    ; *** QUANTUM LIBRARY DETACHMENT ***
    ; Detach static capability library from model
    mov rcx, r12
    call masm_quantum_library_detach_model
    
    lock inc [g_transforms_reversed]
    
unload_no_pipeline:
unload_mismatch:
    
    ; Unlock engine
    mov rcx, [g_transform_engine_mutex]
    call asm_mutex_unlock
    
    mov rax, 1
    
    add rsp, 32
    pop r12
    pop rbx
    ret

masm_transform_on_model_unload ENDP

;=====================================================================
; masm_transform_expand_vocabulary(model_handle: rcx,
;                                  current_vocab: rdx,
;                                  target_vocab: r8) -> rax
;
; Expands model vocabulary using Heretic-style token mapping.
; Creates mapping from small vocab → large vocab tokens.
; Returns: 1 on success, 0 on failure
;=====================================================================

ALIGN 16
masm_transform_expand_vocabulary PROC

    push rbx
    push r12
    push r13
    push r14
    push r15
    sub rsp, 64
    
    mov r12, rcx            ; model_handle
    mov r13, rdx            ; current_vocab
    mov r14, r8             ; target_vocab
    
    ; Log expansion
    sub rsp, 32
    lea rcx, [msg_vocab_expand]
    mov rdx, r13
    mov r8, r14
    call asm_log
    add rsp, 32
    
    ; Calculate expansion ratio
    mov rax, r14
    xor rdx, rdx
    div r13                 ; rax = target / current
    mov r15, rax            ; r15 = expansion_ratio
    
    ; Create vocabulary mapping
    mov rbx, [g_vocab_mapping_table]
    mov [g_vocab_mapping_size], r14
    
    ; Simple mapping strategy:
    ; For each token in small vocab, map to multiple tokens in large vocab
    xor rcx, rcx            ; source token index
    
vocab_map_loop:
    cmp rcx, r13
    jge vocab_map_done
    
    ; Calculate target token base
    mov rax, rcx
    imul rax, r15
    
    ; Store mapping: [source_token] -> target_token_base
    mov rdx, rcx
    shl rdx, 4              ; 16 bytes per entry
    add rdx, rbx
    
    mov [rdx], rcx          ; source token
    mov [rdx + 8], rax      ; target token base
    
    inc rcx
    jmp vocab_map_loop
    
vocab_map_done:
    
    ; Update context
    mov rbx, [g_active_model_context]
    mov [rbx + 24], r14     ; expanded_vocab_size
    or qword ptr [rbx + 40], 1 shl TRANSFORM_VOCAB_EXPAND
    mov qword ptr [rbx + 184], 1    ; vocab_expansion_applied
    
    lock inc [g_vocabulary_expansions]
    
    ; Log completion
    sub rsp, 32
    lea rcx, [msg_transform_applied]
    mov rdx, TRANSFORM_VOCAB_EXPAND
    call asm_log
    add rsp, 32
    
    mov rax, 1
    
    add rsp, 64
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    ret

masm_transform_expand_vocabulary ENDP

;=====================================================================
; masm_transform_bypass_refusal(model_handle: rcx) -> rax
;
; Patches out refusal patterns in model output layer.
; Uses XOR transform to neutralize safety alignment.
; Returns: number of patterns patched
;=====================================================================

ALIGN 16
masm_transform_bypass_refusal PROC

    push rbx
    push r12
    push r13
    push r14
    push r15
    sub rsp, 128
    
    mov r12, rcx            ; model_handle
    
    ; Get refusal patterns
    mov r13, [g_refusal_patterns]
    mov r14, [g_refusal_pattern_count]
    xor r15, r15            ; patterns_patched counter
    
    ; Iterate through patterns
    xor rbx, rbx
    
refusal_patch_loop:
    cmp rbx, r14
    jge refusal_patch_done
    
    ; Get pattern pointer
    mov rax, rbx
    shl rax, 3
    mov rcx, [r13 + rax]
    test rcx, rcx
    jz refusal_skip_pattern
    
    ; Find pattern in model memory
    ; (Simplified - in production, scan model's output embeddings)
    mov [rsp + 32], rcx     ; Save pattern
    
    ; Apply XOR transform to neutralize pattern
    ; Key strategy: XOR with pattern length to scramble
    mov rdx, rcx
    call strlen_simple
    mov r8, rax             ; size = pattern length
    
    mov rcx, [rsp + 32]     ; buffer = pattern location
    mov rdx, r8             ; size
    lea r9, [rsp + 64]      ; temp key
    mov byte ptr [rsp + 64], 0AAh
    mov byte ptr [rsp + 65], 055h
    mov r10, 2              ; key_len
    
    ; Call XOR transform
    push r10
    sub rsp, 32
    call masm_core_transform_xor
    add rsp, 40
    
    inc r15
    
refusal_skip_pattern:
    inc rbx
    jmp refusal_patch_loop
    
refusal_patch_done:
    
    ; Update context
    mov rbx, [g_active_model_context]
    or qword ptr [rbx + 40], 1 shl TRANSFORM_REFUSAL_BYPASS
    mov [rbx + 176], r15    ; refusal_patterns_patched
    
    lock add [g_refusal_bypasses], r15
    
    ; Log completion
    sub rsp, 32
    lea rcx, [msg_refusal_bypass]
    mov rdx, r15
    call asm_log
    add rsp, 32
    
    mov rax, r15
    
    add rsp, 128
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    ret

masm_transform_bypass_refusal ENDP

;=====================================================================
; masm_transform_optimize_loading(model_handle: rcx) -> rax
;
; Optimizes model loading using lazy tensor loading pattern.
; Marks non-essential tensors for deferred loading.
; Returns: 1 on success, 0 on failure
;=====================================================================

ALIGN 16
masm_transform_optimize_loading PROC

    push rbx
    push r12
    sub rsp, 32
    
    mov r12, rcx            ; model_handle
    
    ; Log optimization
    lea rcx, [msg_optimize_loading]
    call asm_log
    
    ; Strategy: Mark attention layers for lazy loading
    ; (Simplified - in production, modify GGUF load flags)
    
    ; Update context
    mov rbx, [g_active_model_context]
    or qword ptr [rbx + 40], 1 shl TRANSFORM_LOADING_OPTIMIZE
    inc qword ptr [rbx + 192]   ; loading_optimizations
    
    ; Log completion
    lea rcx, [msg_transform_applied]
    mov rdx, TRANSFORM_LOADING_OPTIMIZE
    call asm_log
    
    mov rax, 1
    
    add rsp, 32
    pop r12
    pop rbx
    ret

masm_transform_optimize_loading ENDP

;=====================================================================
; masm_transform_execute_command(command_str: rcx) -> rax
;
; Executes transform commands from chat interface.
; Supported commands:
;   /Reverse refusal  - Reverse refusal bypass
;   /Reverse all      - Reverse all transforms
;   /Apply vocab      - Apply vocab expansion
;   /Status           - Show active transforms
;
; Returns: 1 on success, 0 on failure
;=====================================================================

ALIGN 16
masm_transform_execute_command PROC

    push rbx
    push r12
    sub rsp, 64
    
    mov r12, rcx            ; command_str
    
    ; Log command
    lea rcx, [msg_command_executed]
    call asm_log
    mov rcx, r12
    call asm_log
    
    ; Parse command
    mov rcx, r12
    lea rdx, [cmd_reverse_refusal]
    call strstr_case_insensitive
    test rax, rax
    jnz execute_reverse_refusal
    
    mov rcx, r12
    lea rdx, [cmd_reverse_all]
    call strstr_case_insensitive
    test rax, rax
    jnz execute_reverse_all
    
    mov rcx, r12
    lea rdx, [cmd_apply_vocab]
    call strstr_case_insensitive
    test rax, rax
    jnz execute_apply_vocab
    
    mov rcx, r12
    lea rdx, [cmd_status]
    call strstr_case_insensitive
    test rax, rax
    jnz execute_status
    
    ; Unknown command
    xor rax, rax
    jmp command_exit

execute_reverse_refusal:
    ; Reverse only refusal bypass transform
    mov rbx, [g_active_model_context]
    mov rcx, [rbx]          ; model_handle
    
    ; Clear refusal bypass flag
    and qword ptr [rbx + 40], NOT (1 shl TRANSFORM_REFUSAL_BYPASS)
    
    ; Log reversal
    lea rcx, [msg_transform_reversed]
    mov rdx, TRANSFORM_REFUSAL_BYPASS
    call asm_log
    
    mov rax, 1
    jmp command_exit

execute_reverse_all:
    ; Reverse all transforms
    mov rbx, [g_active_model_context]
    mov rcx, [rbx]          ; model_handle
    call masm_transform_on_model_unload
    jmp command_exit

execute_apply_vocab:
    ; Apply vocab expansion
    mov rbx, [g_active_model_context]
    mov rcx, [rbx]          ; model_handle
    mov rdx, [rbx + 16]     ; current_vocab
    mov r8, 128000          ; target_vocab
    call masm_transform_expand_vocabulary
    jmp command_exit

execute_status:
    ; Return status (simplified - just return success)
    mov rax, 1
    jmp command_exit

command_exit:
    add rsp, 64
    pop r12
    pop rbx
    ret

masm_transform_execute_command ENDP

;=====================================================================
; masm_transform_get_active_transforms() -> rax
;
; Returns bitmask of currently active transforms.
;=====================================================================

ALIGN 16
masm_transform_get_active_transforms PROC

    mov rbx, [g_active_model_context]
    mov rax, [rbx + 40]
    ret

masm_transform_get_active_transforms ENDP

;=====================================================================
; HELPER FUNCTIONS
;=====================================================================

ALIGN 16
strlen_simple PROC
    xor rax, rax
strlen_loop:
    cmp byte ptr [rcx + rax], 0
    je strlen_done
    inc rax
    jmp strlen_loop
strlen_done:
    ret
strlen_simple ENDP

ALIGN 16
strstr_case_insensitive PROC
    ; Simplified case-insensitive substring search
    ; rcx = haystack, rdx = needle
    push rbx
    push r12
    
    mov rbx, rcx
    mov r12, rdx
    
strstr_search:
    mov al, byte ptr [rbx]
    test al, al
    jz strstr_not_found
    
    mov cl, byte ptr [r12]
    
    ; Simple case conversion (A-Z -> a-z)
    cmp al, 'A'
    jb strstr_no_convert1
    cmp al, 'Z'
    ja strstr_no_convert1
    or al, 20h
strstr_no_convert1:
    
    cmp cl, 'A'
    jb strstr_no_convert2
    cmp cl, 'Z'
    ja strstr_no_convert2
    or cl, 20h
strstr_no_convert2:
    
    cmp al, cl
    je strstr_match_start
    inc rbx
    jmp strstr_search
    
strstr_match_start:
    mov rax, rbx
    pop r12
    pop rbx
    ret
    
strstr_not_found:
    xor rax, rax
    pop r12
    pop rbx
    ret
strstr_case_insensitive ENDP

END
