;==========================================================================
; agentic_masm.asm - Clean, Single-Version Agentic Tool System
; ==========================================================================
; Implements the core agentic tool registry and execution loop for RawrXD.
; Features:
; - Tool registration and lookup
; - Command processing with pattern matching
; - Integration with GUI and Rawr1024 engines
; - Zero C++ dependencies
;==========================================================================

option casemap:none

include windows.inc
includelib kernel32.lib
includelib user32.lib

; ============================================================================
; Integration with Zero-Day Agentic Engine (master include)
; Provides access to:
;   - All ZeroDayAgenticEngine_* functions
;   - All mission state constants (MISSION_STATE_*)
;   - All signal types (SIGNAL_TYPE_*)
;   - Structured logging framework (LOG_LEVEL_*)
;   - Complexity levels (COMPLEXITY_*)
; ============================================================================
include d:\RawrXD-production-lazy-init\masm\masm_master_include.asm

; Win32 APIs
EXTERN CreateFileA:PROC
EXTERN ReadFile:PROC
EXTERN CloseHandle:PROC
EXTERN CreateProcessA:PROC
EXTERN WaitForSingleObject:PROC
EXTERN GetTickCount64:PROC
EXTERN LoadLibraryA:PROC

; Constants
INFINITE            EQU 0FFFFFFFFh
INVALID_HANDLE_VALUE EQU -1
GENERIC_READ        EQU 80000000h
GENERIC_WRITE       EQU 40000000h
FILE_SHARE_READ     EQU 00000001h
OPEN_EXISTING       EQU 3
FILE_ATTRIBUTE_NORMAL EQU 80h

;==========================================================================
; EXTERNAL DECLARATIONS
;==========================================================================
EXTERN masm_detect_failure:PROC
EXTERN ml_helpers_log_append:PROC ; From ml_helpers.asm

; Alias for logging
file_log_append EQU ml_helpers_log_append

;==========================================================================
; PUBLIC: agent_execute_tool(tool_name: rcx, params: rdx) -> rax
; Executes a tool by name
;==========================================================================
PUBLIC agent_execute_tool
agent_execute_tool PROC
    push rbx
    push rsi
    push rdi
    sub rsp, 32
    
    ; ============================================================================
    ; Log tool execution start
    ; ============================================================================
    LEA rcx, [rel szExecutingTool]
    MOV rdx, LOG_LEVEL_INFO
    CALL Logger_LogStructured
    
    ; Measure execution time
    CALL QueryPerformanceCounter
    MOV [BenchValue], rax  ; Store start time
    
    mov rsi, rdx ; params
    call agent_get_tool
    test rax, rax
    jz tool_not_found
    
    mov rbx, rax ; AgentTool ptr
    mov rcx, rsi ; params
    call QWORD PTR [rbx + AgentTool.handler]
    
    ; ============================================================================
    ; Log tool execution completion
    ; ============================================================================
    LEA rcx, [rel szToolExecuted]
    MOV rdx, LOG_LEVEL_INFO
    CALL Logger_LogStructured
    
    ; Measure execution time
    CALL QueryPerformanceCounter
    SUB rax, [BenchValue]  ; Calculate elapsed time
    MOV [BenchValue], rax  ; Store elapsed time
    
    jmp done
    
tool_not_found:
    ; ============================================================================
    ; Log tool not found error
    ; ============================================================================
    LEA rcx, [rel szToolNotFound]
    MOV rdx, LOG_LEVEL_ERROR
    CALL Logger_LogStructured
    
    lea rax, szToolNotFound
    add rsp, 32
    pop rdi
    pop rsi
    pop rbx
    ret
agent_execute_tool ENDP

; Alias for string length
asm_str_length EQU strlen_masm

;==========================================================================
; agent_execute_with_retry(tool_name: rcx, params: rdx, max_retries: r8d) -> rax
; Executes a tool with automatic failure detection and retry logic
;==========================================================================
agent_execute_with_retry PROC
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    sub rsp, 512 ; Space for FailureDetectionResult

    ; ============================================================================
    ; Log retry execution start
    ; ============================================================================
    LEA rcx, [rel szRetryStart]
    MOV rdx, LOG_LEVEL_INFO
    CALL Logger_LogStructured

    mov rsi, rcx ; tool_name
    mov rdi, rdx ; params
    mov r12d, r8d ; max_retries
    xor r13d, r13d ; current_retry

@retry_loop:
    ; 1. Execute tool
    mov rcx, rsi
    mov rdx, rdi
    call agent_execute_tool
    mov rbx, rax ; response_ptr (assume it returns a string ptr)
    
    test rbx, rbx
    jz @failure_detected
    
    ; 2. Detect failure in response
    mov rcx, rbx
    call asm_str_length
    mov rdx, rax ; response_len
    mov rcx, rbx ; response_ptr
    lea r8, [rsp + 32] ; result_ptr (FailureDetectionResult)
    call masm_detect_failure
    
    test eax, eax
    jz @success ; No failure detected
    
@failure_detected:
    inc r13d
    cmp r13d, r12d
    jge @max_retries_reached
    
    ; 3. Self-healing logic: Modify parameters based on failure type
    ; (Simplified: just retry for now, in production we'd adjust params)
    lea rcx, szRetryingMsg
    call file_log_append
    
    ; ============================================================================
    ; Log retry attempt
    ; ============================================================================
    LEA rcx, [rel szRetryAttempt]
    MOV rdx, LOG_LEVEL_WARNING
    CALL Logger_LogStructured
    
    jmp @retry_loop

@success:
    ; ============================================================================
    ; Log retry success
    ; ============================================================================
    LEA rcx, [rel szRetrySuccess]
    MOV rdx, LOG_LEVEL_SUCCESS
    CALL Logger_LogStructured
    
    mov rax, rbx
    jmp @done

@max_retries_reached:
    ; ============================================================================
    ; Log max retries reached
    ; ============================================================================
    LEA rcx, [rel szMaxRetries]
    MOV rdx, LOG_LEVEL_ERROR
    CALL Logger_LogStructured
    
    lea rcx, szMaxRetriesMsg
    call file_log_append
    xor rax, rax

@done:
    add rsp, 512
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret

.data
szRetryingMsg db "Agentic failure detected. Retrying...",0
szMaxRetriesMsg db "Max retries reached. Agentic recovery failed.",0
.code
agent_execute_with_retry ENDP

; GUI Tools (from gui_designer_complete.asm)
EXTERN gui_agent_inspect:PROC
EXTERN gui_agent_modify:PROC
EXTERN gui_create_complete_ide:PROC

; Rawr1024 Tools (from rawr1024_dual_engine.asm)
EXTERN rawr1024_build_model:PROC
EXTERN rawr1024_quantize_model:PROC
EXTERN rawr1024_encrypt_model:PROC
EXTERN rawr1024_direct_load:PROC
EXTERN rawr1024_beacon_sync:PROC

; Process Management (from process_manager.asm)
EXTERN CreateRedirectedProcess:PROC
EXTERN ReadProcessOutput:PROC

;==========================================================================
; STRUCTURES
;==========================================================================
PROCESS_INFO_EX STRUCT
    hProcess            QWORD ?
    hThread             QWORD ?
    hStdInWrite         QWORD ?
    hStdOutRead         QWORD ?
    hStdErrRead         QWORD ?
    dwProcessId         DWORD ?
    dwThreadId          DWORD ?
PROCESS_INFO_EX ENDS

AgentTool STRUCT
    tool_name       QWORD ?           ; Pointer to name string
    description     QWORD ?           ; Pointer to description string
    handler         QWORD ?           ; Pointer to handler function
AgentTool ENDS

;==========================================================================
; CONSTANTS
;==========================================================================
MAX_TOOLS           EQU 64
MAX_OUTPUT_SIZE     EQU 65536

;==========================================================================
; DATA SEGMENT
;==========================================================================
.data
    ; Tool Names
    szReadFile      BYTE "read_file", 0
    szExecCmd       BYTE "execute_command", 0
    szTpsBench      BYTE "tps_benchmark", 0
    szGuiInspect    BYTE "gui_design_inspect", 0
    szGuiModify     BYTE "gui_modify_component", 0
    szGuiCreate     BYTE "gui_create_complete_ide", 0
    szRawrBuild     BYTE "rawr1024_build_model", 0
    szRawrQuant     BYTE "rawr1024_quantize_model", 0
    szRawrEncrypt   BYTE "rawr1024_encrypt_model", 0
    szRawrLoad      BYTE "rawr1024_direct_load", 0
    szRawrSync      BYTE "rawr1024_beacon_sync", 0

    ; Descriptions
    descReadFile    BYTE "Reads the contents of a file from disk.", 0
    descExecCmd     BYTE "Executes a shell command and returns output.", 0
    descTpsBench    BYTE "Performs a real-time TPS benchmark on the loaded model.", 0
    descGuiInspect  BYTE "Inspects the current GUI component tree.", 0
    descGuiModify   BYTE "Modifies a GUI component's properties.", 0
    descGuiCreate   BYTE "Creates a complete production-ready IDE.", 0
    descRawrBuild   BYTE "Builds a new Rawr1024 model from source.", 0
    descRawrQuant   BYTE "Quantizes a model using RawrQ/RawrZ/RawrX.", 0
    descRawrEncrypt BYTE "Applies quantum-resistant encryption to a model.", 0
    descRawrLoad    BYTE "Directly loads a model into memory with zero-copy.", 0
    descRawrSync    BYTE "Synchronizes model state via Beaconism protocol.", 0

    szToolNotFound  BYTE "Error: Tool not found.", 0
    szSuccess       BYTE "Operation completed successfully.", 0
    szExecutingTool BYTE "Agentic tool execution starting.", 0
    szToolExecuted  BYTE "Agentic tool execution completed.", 0
    szListHeader    BYTE "Available Agent Tools:", 13, 10, 0
    szStrcmpPrefix  BYTE "[strcmp] s1=",0
    szStrcmpMid     BYTE " s2=",0
    szStrcmpNewline BYTE 13,10,0
    szBenchResult   BYTE "TPS Benchmark Result: %d tokens/sec", 0
    szFileError     BYTE "Error: Could not read file.", 0
    szCmdError      BYTE "Error: Command execution failed.", 0
    szRetryStart    BYTE "Agentic tool retry execution starting.", 0
    szRetryAttempt  BYTE "Agentic tool retry attempt.", 0
    szRetrySuccess  BYTE "Agentic tool retry execution succeeded.", 0
    szMaxRetries    BYTE "Agentic tool max retries reached.", 0
    szProcessingCommand BYTE "Agentic command processing starting.", 0
    szToolFound     BYTE "Agentic tool found and executing.", 0

.data?
    ToolRegistry    AgentTool MAX_TOOLS DUP (<>)
    ToolCount       DWORD ?
    OutputBuffer    BYTE MAX_OUTPUT_SIZE DUP (?)
    BenchValue      QWORD ?

;==========================================================================
; CODE SEGMENT
;==========================================================================
.code

;==========================================================================
; PUBLIC: agent_init_tools()
; Registers all core tools in the registry.
;==========================================================================
PUBLIC agent_init_tools
agent_init_tools PROC
    push rbx
    
    ; ============================================================================
    ; Log tool initialization start
    ; ============================================================================
    LEA rcx, [rel szSuccess]
    MOV rdx, LOG_LEVEL_INFO
    CALL Logger_LogStructured
    
    ; Measure initialization time
    CALL QueryPerformanceCounter
    MOV [BenchValue], rax  ; Store start time
    
    mov ToolCount, 0
    
    ; Register Core Tools
    lea rcx, szReadFile
    lea rdx, descReadFile
    lea r8, read_file_tool
    call register_tool
    
    lea rcx, szExecCmd
    lea rdx, descExecCmd
    lea r8, execute_command_tool
    call register_tool
    
    lea rcx, szTpsBench
    lea rdx, descTpsBench
    lea r8, tps_benchmark_tool
    call register_tool
    
    ; Register GUI Tools
    lea rcx, szGuiInspect
    lea rdx, descGuiInspect
    lea r8, gui_agent_inspect
    call register_tool
    
    lea rcx, szGuiModify
    lea rdx, descGuiModify
    lea r8, gui_agent_modify
    call register_tool
    
    lea rcx, szGuiCreate
    lea rdx, descGuiCreate
    lea r8, gui_create_complete_ide
    call register_tool
    
    ; Register Rawr1024 Tools
    lea rcx, szRawrBuild
    lea rdx, descRawrBuild
    lea r8, rawr1024_build_model
    call register_tool
    
    lea rcx, szRawrQuant
    lea rdx, descRawrQuant
    lea r8, rawr1024_quantize_model
    call register_tool
    
    lea rcx, szRawrEncrypt
    lea rdx, descRawrEncrypt
    lea r8, rawr1024_encrypt_model
    call register_tool
    
    lea rcx, szRawrLoad
    lea rdx, descRawrLoad
    lea r8, rawr1024_direct_load
    call register_tool
    
    lea rcx, szRawrSync
    lea rdx, descRawrSync
    lea r8, rawr1024_beacon_sync
    call register_tool
    
    pop rbx
    ret
agent_init_tools ENDP

;==========================================================================
; INTERNAL: register_tool(name, desc, handler)
;==========================================================================
register_tool PROC
    mov eax, ToolCount
    cmp eax, MAX_TOOLS
    jae reg_done
    
    mov r10d, SIZE AgentTool
    imul eax, r10d
    lea r11, ToolRegistry
    add r11, rax
    
    mov [r11 + AgentTool.tool_name], rcx
    mov [r11 + AgentTool.description], rdx
    mov [r11 + AgentTool.handler], r8
    
    inc ToolCount
reg_done:
    ret
register_tool ENDP

;==========================================================================
; PUBLIC: agent_process_command(cmd_string) -> rax (output string)
; Processes a command string by finding the tool name and calling its handler.
;==========================================================================
PUBLIC agent_process_command
agent_process_command PROC
    push rbx
    push rsi
    push rdi
    sub rsp, 32
    
    ; ============================================================================
    ; Log command processing start
    ; ============================================================================
    LEA rcx, [rel szProcessingCommand]
    MOV rdx, LOG_LEVEL_INFO
    CALL Logger_LogStructured
    
    mov rsi, rcx            ; rsi = command string
    
    ; Skip leading whitespace
skip_ws:
    mov al, [rsi]
    test al, al
    jz tool_not_found
    cmp al, ' '
    jne find_tool
    inc rsi
    jmp skip_ws

find_tool:
    ; Iterate through tools to find a match at the START of the string
    xor ebx, ebx            ; index
process_loop:
    cmp ebx, ToolCount
    jae tool_not_found
    
    mov eax, ebx
    mov ecx, SIZE AgentTool
    imul eax, ecx
    lea rdi, ToolRegistry
    add rdi, rax
    
    ; Check if cmd_string starts with tool_name
    mov rcx, rsi            ; haystack
    mov rdx, [rdi + AgentTool.tool_name] ; needle
    call str_starts_with
    
    test eax, eax
    jnz found_tool
    
    inc ebx
    jmp process_loop
    
found_tool:
    ; ============================================================================
    ; Log tool found
    ; ============================================================================
    LEA rcx, [rel szToolFound]
    MOV rdx, LOG_LEVEL_INFO
    CALL Logger_LogStructured
    
    ; Extract arguments (everything after tool_name)
    mov rcx, [rdi + AgentTool.tool_name]
    call strlen_masm
    add rsi, rax            ; Skip tool name
    
    ; Skip whitespace before arguments
skip_arg_ws:
    mov al, [rsi]
    test al, al
    jz call_handler
    cmp al, ' '
    jne call_handler
    inc rsi
    jmp skip_arg_ws

call_handler:
    ; Call the handler
    mov rax, [rdi + AgentTool.handler]
    mov rcx, rsi            ; Pass arguments string as context
    call rax
    jmp process_done
    
tool_not_found:
    ; ============================================================================
    ; Log tool not found
    ; ============================================================================
    LEA rcx, [rel szToolNotFound]
    MOV rdx, LOG_LEVEL_ERROR
    CALL Logger_LogStructured
    
    lea rax, szToolNotFound
    
process_done:
    add rsp, 32
    pop rdi
    pop rsi
    pop rbx
    ret
agent_process_command ENDP

; Helper: str_starts_with(str, prefix) -> eax (1 if true)
str_starts_with PROC
    push rsi
    push rdi
ssw_loop:
    mov al, [rdx]
    test al, al
    jz ssw_match
    mov cl, [rcx]
    cmp al, cl
    jne ssw_fail
    inc rcx
    inc rdx
    jmp ssw_loop
ssw_match:
    mov eax, 1
    jmp ssw_done
ssw_fail:
    xor eax, eax
ssw_done:
    pop rdi
    pop rsi
    ret
str_starts_with ENDP

; Helper: strlen_masm(str) -> rax
strlen_masm PROC
    xor rax, rax
sl_loop:
    cmp byte ptr [rcx + rax], 0
    je sl_done
    inc rax
    jmp sl_loop
sl_done:
    ret
strlen_masm ENDP

;==========================================================================
; TOOL HANDLER: read_file_tool(args)
;==========================================================================
read_file_tool PROC
    push rbx
    push rsi
    push rdi
    sub rsp, 64
    
    mov rsi, rcx            ; rsi = filename
    
    ; Open file
    mov rcx, rsi
    mov rdx, GENERIC_READ
    mov r8, FILE_SHARE_READ
    xor r9, r9
    mov QWORD PTR [rsp + 32], OPEN_EXISTING
    mov QWORD PTR [rsp + 40], FILE_ATTRIBUTE_NORMAL
    mov QWORD PTR [rsp + 48], 0
    call CreateFileA
    
    cmp rax, INVALID_HANDLE_VALUE
    je rf_fail
    
    mov rbx, rax            ; rbx = hFile
    
    ; Read file (up to MAX_OUTPUT_SIZE - 1)
    mov rcx, rbx
    lea rdx, OutputBuffer
    mov r8, MAX_OUTPUT_SIZE - 1
    lea r9, [rsp + 56]      ; bytesRead
    mov QWORD PTR [rsp + 32], 0
    call ReadFile
    
    test eax, eax
    jz rf_close_fail
    
    ; Null terminate
    mov rax, [rsp + 56]
    lea rcx, OutputBuffer
    mov byte ptr [rcx + rax], 0
    
    mov rcx, rbx
    call CloseHandle
    
    lea rax, OutputBuffer
    jmp rf_done
    
rf_close_fail:
    mov rcx, rbx
    call CloseHandle
rf_fail:
    lea rax, szFileError
rf_done:
    add rsp, 64
    pop rdi
    pop rsi
    pop rbx
    ret
read_file_tool ENDP

;==========================================================================
; TOOL HANDLER: execute_command_tool(args)
;==========================================================================
execute_command_tool PROC
    push rbx
    push rsi
    push rdi
    sub rsp, 700            ; Space for PROCESS_INFO_EX
    
    mov rsi, rcx            ; rsi = command line
    lea rdi, [rsp + 32]     ; rdi = pInfo
    
    mov rcx, rsi
    mov rdx, rdi
    call CreateRedirectedProcess
    
    test eax, eax
    jz exec_fail
    
    ; Read output
    mov rcx, [rdi + PROCESS_INFO_EX.hStdOutRead]
    lea rdx, OutputBuffer
    mov r8, MAX_OUTPUT_SIZE - 1
    lea r9, [rsp + 640]     ; bytesRead
    mov QWORD PTR [rsp + 32], 0
    call ReadFile
    
    ; Null terminate
    mov rax, [rsp + 640]
    lea rcx, OutputBuffer
    mov byte ptr [rcx + rax], 0
    
    ; Cleanup handles
    mov rcx, [rdi + PROCESS_INFO_EX.hProcess]
    call CloseHandle
    mov rcx, [rdi + PROCESS_INFO_EX.hThread]
    call CloseHandle
    mov rcx, [rdi + PROCESS_INFO_EX.hStdOutRead]
    call CloseHandle
    mov rcx, [rdi + PROCESS_INFO_EX.hStdInWrite]
    call CloseHandle
    
    lea rax, OutputBuffer
    jmp exec_done
    
exec_fail:
    lea rax, szCmdError
exec_done:
    add rsp, 700
    pop rdi
    pop rsi
    pop rbx
    ret
execute_command_tool ENDP

;==========================================================================
; TOOL HANDLER: tps_benchmark_tool(args)
;==========================================================================
tps_benchmark_tool PROC
    ; Placeholder for actual benchmark logic
    lea rax, szBenchResult
    ret
tps_benchmark_tool ENDP

;==========================================================================
; PUBLIC: agent_list_tools() -> rax (ptr to list string)
;==========================================================================
PUBLIC agent_list_tools
agent_list_tools PROC
    lea rax, szListHeader
    ret
agent_list_tools ENDP

;==========================================================================
; PUBLIC: agent_get_tool(name) -> rax (ptr to AgentTool or NULL)
;==========================================================================
PUBLIC agent_get_tool
agent_get_tool PROC
    push rbx
    push rsi
    push rdi
    
    mov rsi, rcx            ; tool name to find
    xor ebx, ebx            ; index
agt_loop:
    cmp ebx, ToolCount
    jae agt_not_found
    
    mov eax, ebx
    mov ecx, SIZE AgentTool
    imul eax, ecx
    lea rdi, ToolRegistry
    add rdi, rax
    
    ; Compare tool names
    mov rcx, rsi
    mov rdx, [rdi + AgentTool.tool_name]
    call strcmp_masm
    
    test eax, eax
    jz agt_found
    
    inc ebx
    jmp agt_loop
    
agt_found:
    mov rax, rdi
    jmp agt_done
    
agt_not_found:
    xor rax, rax
    
agt_done:
    pop rdi
    pop rsi
    pop rbx
    ret
agent_get_tool ENDP

;==========================================================================
; Helper: strcmp_masm(str1, str2) -> eax (0 if equal)
;==========================================================================
strcmp_masm PROC
    push rbx
    push rsi
    push rdi
    
    mov rsi, rcx
    mov rdi, rdx

    ; Emit lightweight log to run.log to capture call sites early
    lea rcx, szStrcmpPrefix
    call file_log_append
    mov rcx, rsi
    call file_log_append
    lea rcx, szStrcmpMid
    call file_log_append
    mov rcx, rdi
    call file_log_append
    lea rcx, szStrcmpNewline
    call file_log_append
    
scm_loop:
    mov al, [rsi]
    mov bl, [rdi]
    cmp al, bl
    jne scm_not_equal
    
    test al, al
    jz scm_equal
    
    inc rsi
    inc rdi
    jmp scm_loop
    
scm_equal:
    xor eax, eax
    jmp scm_done
    
scm_not_equal:
    mov eax, 1
    
scm_done:
    pop rdi
    pop rsi
    pop rbx
    ret
strcmp_masm ENDP

;==========================================================================
; Helper: strstr_masm(haystack, needle) -> rax (ptr to match or NULL)
;==========================================================================
strstr_masm PROC
    push rbx
    push rsi
    push rdi
    push r12
    push r10
    push r11
    
    mov rsi, rcx            ; haystack
    mov rdi, rdx            ; needle
    
    ; Get needle length
    xor r12, r12
ssm_len_loop:
    cmp byte ptr [rdi + r12], 0
    je ssm_len_done
    inc r12
    jmp ssm_len_loop
    
ssm_len_done:
    test r12, r12
    jz ssm_found            ; Empty needle matches immediately
    
ssm_haystack_loop:
    cmp byte ptr [rsi], 0
    je ssm_not_found
    
    ; Compare needle at current haystack position
    mov r8, r12             ; Use r8 instead of rcx
    mov r10, rsi            ; Use r10 instead of rbx
    mov r11, rdi            ; Use r11 instead of r9
    
ssm_compare:
    mov al, [r10]
    mov dl, [r11]          ; Use dl register
    cmp al, dl
    jne ssm_no_match
    
    inc r10
    inc r11
    dec r8                  ; Use r8 instead of r12
    test r8, r8
    jnz ssm_compare
    
    ; Match found
    mov rax, rsi
    jmp ssm_done
    
ssm_no_match:
    ; Restore r12
    xor r12, r12
ssm_restore_len:
    cmp byte ptr [rdi + r12], 0
    je ssm_restored
    inc r12
    jmp ssm_restore_len
    
ssm_restored:
    inc rsi
    jmp ssm_haystack_loop
    
ssm_found:
    mov rax, rsi
    jmp ssm_done
    
ssm_not_found:
    xor rax, rax
    
ssm_done:
    pop r11
    pop r10
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
strstr_masm ENDP

END
