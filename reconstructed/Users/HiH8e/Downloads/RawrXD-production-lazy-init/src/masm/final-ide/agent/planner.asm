;==========================================================================
; agent_planner.asm - Pure MASM Agentic Task Planner
; ==========================================================================
; Replaces planner.cpp with a high-performance assembly implementation.
; Decomposes human wishes into structured JSON task arrays.
;==========================================================================

option casemap:none

include windows.inc
include json_parser.inc

;==========================================================================
; EXTERNAL DECLARATIONS
;==========================================================================
EXTERN asm_str_create_from_cstr:PROC
EXTERN asm_str_to_lower:PROC
EXTERN asm_str_contains:PROC
EXTERN json_builder_create_array:PROC
EXTERN json_builder_add_object:PROC
EXTERN json_builder_add_string:PROC
EXTERN json_builder_add_int:PROC
EXTERN json_builder_to_string:PROC

;==========================================================================
; DATA SECTION
;==========================================================================
.data
    ; Intent keywords
    szSelf          BYTE "yourself", 0
    szItself        BYTE "itself", 0
    szClone         BYTE "clone", 0
    szReplicate     BYTE "replicate", 0
    
    szFaster        BYTE "faster", 0
    szOptimize      BYTE "optimize", 0
    szQuant         BYTE "quant", 0
    
    szRelease       BYTE "release", 0
    szShip          BYTE "ship", 0
    szDeploy        BYTE "deploy", 0
    
    szWebsite       BYTE "website", 0
    szWebApp        BYTE "web app", 0
    szFrontend      BYTE "frontend", 0
    
    ; Task types
    szTaskAddKernel BYTE "add_kernel", 0
    szTaskAddCpp    BYTE "add_cpp", 0
    szTaskAddMenu   BYTE "add_menu", 0
    szTaskBench     BYTE "bench", 0
    szTaskSelfTest  BYTE "self_test", 0
    szTaskHotReload BYTE "hot_reload", 0

.code

;==========================================================================
; agent_planner_plan(humanWish: rcx) -> rax (json_string_ptr)
;==========================================================================
PUBLIC agent_planner_plan
agent_planner_plan PROC
    push rbx
    push rsi
    push rdi
    sub rsp, 32
    
    mov rsi, rcx        ; rsi = humanWish
    
    ; 1. Normalize wish (to lower case)
    mov rcx, rsi
    call asm_str_to_lower
    mov rbx, rax        ; rbx = lowerWish
    
    ; 2. Route based on intent
    
    ; Check for optimization/quantization
    mov rcx, rbx
    lea rdx, szOptimize
    call asm_str_contains
    test rax, rax
    jnz plan_quant
    
    mov rcx, rbx
    lea rdx, szQuant
    call asm_str_contains
    test rax, rax
    jnz plan_quant
    
    ; Check for release
    mov rcx, rbx
    lea rdx, szRelease
    call asm_str_contains
    test rax, rax
    jnz plan_release
    
    ; Default to generic plan
    jmp plan_generic

plan_quant:
    call agent_planner_plan_quant_kernel
    jmp done

plan_release:
    call agent_planner_plan_release
    jmp done

plan_generic:
    call agent_planner_plan_generic
    jmp done

done:
    add rsp, 32
    pop rdi
    pop rsi
    pop rbx
    ret
agent_planner_plan ENDP

;==========================================================================
; agent_planner_plan_quant_kernel() -> rax (json_string_ptr)
;==========================================================================
agent_planner_plan_quant_kernel PROC
    push rbx
    sub rsp, 32
    
    call json_builder_create_array
    mov rbx, rax        ; rbx = array builder
    
    ; Task 1: Add Kernel
    mov rcx, rbx
    call json_builder_add_object
    mov rdi, rax
    
    mov rcx, rdi
    lea rdx, szTaskAddKernel
    call json_builder_add_string ; type
    
    ; ... add more fields ...
    
    ; Task 2: Add C++ Wrapper
    mov rcx, rbx
    call json_builder_add_object
    ; ...
    
    ; Finalize
    mov rcx, rbx
    call json_builder_to_string
    
    add rsp, 32
    pop rbx
    ret
agent_planner_plan_quant_kernel ENDP

; Stubs for other plan types
agent_planner_plan_release PROC
    xor rax, rax
    ret
agent_planner_plan_release ENDP

agent_planner_plan_generic PROC
    xor rax, rax
    ret
agent_planner_plan_generic ENDP

END
