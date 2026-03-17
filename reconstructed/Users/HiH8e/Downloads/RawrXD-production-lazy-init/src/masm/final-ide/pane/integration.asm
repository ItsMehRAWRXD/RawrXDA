;==========================================================================
; PANE INTEGRATION SYSTEM - Front-to-Back Linkage with ASM Features
;==========================================================================

option casemap:none

;==========================================================================
; CONSTANTS
;==========================================================================
PANE_EDITOR      equ 1
PANE_DEBUGGER    equ 2
PANE_ASSEMBLER   equ 3
PANE_PERFORMANCE equ 4

;==========================================================================
; EXTERNAL DECLARATIONS
;==========================================================================
EXTERN pane_register_feature:PROC
EXTERN asm_highlight_syntax:PROC
EXTERN asm_validate_syntax:PROC
EXTERN asm_intellisense_handler:PROC
EXTERN asm_register_viewer:PROC
EXTERN asm_memory_inspector:PROC
EXTERN asm_build_system:PROC
EXTERN asm_linker_interface:PROC
EXTERN asm_profiler:PROC
EXTERN asm_optimizer:PROC

;==========================================================================
; CODE SEGMENT
;==========================================================================
.code

;==========================================================================
; Pane Feature Registry - Maps ASM features to panes
;==========================================================================
ALIGN 16
pane_feature_init PROC
    ; Register ASM features with specific panes
    
    ; Editor Pane - ASM syntax highlighting, intellisense
    mov rcx, PANE_EDITOR
    lea rdx, asm_syntax_handler
    call pane_register_feature
    
    mov rcx, PANE_EDITOR  
    lea rdx, asm_intellisense_handler
    call pane_register_feature
    
    ; Debugger Pane - Register viewer, memory inspector
    mov rcx, PANE_DEBUGGER
    lea rdx, asm_register_viewer
    call pane_register_feature
    
    mov rcx, PANE_DEBUGGER
    lea rdx, asm_memory_inspector  
    call pane_register_feature
    
    ; Assembler Pane - Build system, linker
    mov rcx, PANE_ASSEMBLER
    lea rdx, asm_build_system
    call pane_register_feature
    
    mov rcx, PANE_ASSEMBLER
    lea rdx, asm_linker_interface
    call pane_register_feature
    
    ; Performance Pane - Profiler, optimizer
    mov rcx, PANE_PERFORMANCE
    lea rdx, asm_profiler
    call pane_register_feature
    
    mov rcx, PANE_PERFORMANCE
    lea rdx, asm_optimizer
    call pane_register_feature
    
    ret
pane_feature_init ENDP

;==========================================================================
; ASM Feature Handlers - Drop into panes
;==========================================================================

; Editor Pane Features
asm_syntax_handler PROC
    ; rcx = editor handle, rdx = text buffer
    call asm_highlight_syntax
    call asm_validate_syntax
    ret
asm_syntax_handler ENDP

asm_intellisense_handler PROC
    ; rcx = cursor position, rdx = context
    call asm_get_completions
    call asm_show_help_popup
    ret
asm_intellisense_handler ENDP

; Debugger Pane Features  
asm_register_viewer PROC
    ; rcx = register set
    call asm_format_registers
    call pane_update_display
    ret
asm_register_viewer ENDP

asm_memory_inspector PROC
    ; rcx = memory address, rdx = size
    call asm_read_memory
    call asm_format_hex_dump
    call pane_update_display
    ret
asm_memory_inspector ENDP

; Assembler Pane Features
asm_build_system PROC
    ; rcx = source file path
    call asm_invoke_masm
    call asm_parse_errors
    call pane_show_results
    ret
asm_build_system ENDP

asm_linker_interface PROC
    ; rcx = object files, rdx = output path
    call asm_invoke_linker
    call asm_parse_link_map
    call pane_show_results
    ret
asm_linker_interface ENDP

; Performance Pane Features
asm_profiler PROC
    ; rcx = executable path
    call asm_start_profiling
    call asm_collect_metrics
    call pane_show_profile_data
    ret
asm_profiler ENDP

asm_optimizer PROC
    ; rcx = asm code buffer
    call asm_analyze_performance
    call asm_suggest_optimizations
    call pane_show_suggestions
    ret
asm_optimizer ENDP

;==========================================================================
; Pane Communication Bus - Front-to-Back messaging
;==========================================================================
ALIGN 16
pane_message_bus PROC
    ; rcx = source pane, rdx = target pane, r8 = message, r9 = data
    
    ; Route message through central bus
    call validate_pane_ids
    test eax, eax
    jz bus_error
    
    ; Check if target pane has handler
    mov rcx, rdx  ; target pane
    mov rdx, r8   ; message type
    call pane_get_handler
    test rax, rax
    jz bus_no_handler
    
    ; Call handler with data
    mov rcx, r9   ; data
    call rax      ; handler function
    
    mov eax, 1    ; success
    ret
    
bus_error:
bus_no_handler:
    xor eax, eax  ; failure
    ret
pane_message_bus ENDP

;==========================================================================
; Integration with existing UI system
;==========================================================================
ALIGN 16
integrate_with_main PROC
    ; Hook into existing init_application
    call pane_feature_init
    
    ; Connect to existing UI controls
    call connect_editor_pane
    call connect_debugger_pane  
    call connect_assembler_pane
    call connect_performance_pane
    
    ; Link with ML model for AI assistance
    call link_ml_to_panes
    
    ret
integrate_with_main ENDP

connect_editor_pane PROC
    ; Link editor pane to existing ui_create_* functions
    call ui_get_editor_handle
    mov editor_pane_handle, rax
    
    ; Register ASM-specific callbacks
    mov rcx, rax
    lea rdx, asm_on_text_change
    call ui_set_text_change_callback
    
    ret
connect_editor_pane ENDP

connect_debugger_pane PROC
    ; Create debugger pane in existing split pane system
    mov rcx, PANE_TYPE_DEBUGGER
    call ui_create_split_pane
    mov debugger_pane_handle, rax
    
    ; Add register and memory viewers
    mov rcx, rax
    call add_register_viewer
    call add_memory_viewer
    
    ret
connect_debugger_pane ENDP

;==========================================================================
; Data Structures
;==========================================================================
.data
editor_pane_handle      dq 0
debugger_pane_handle    dq 0
assembler_pane_handle   dq 0
performance_pane_handle dq 0

; Pane IDs
PANE_EDITOR      equ 1
PANE_DEBUGGER    equ 2  
PANE_ASSEMBLER   equ 3
PANE_PERFORMANCE equ 4

; Pane Types
PANE_TYPE_EDITOR      equ 100
PANE_TYPE_DEBUGGER    equ 101
PANE_TYPE_ASSEMBLER   equ 102
PANE_TYPE_PERFORMANCE equ 103

END