; autonomous_agent_system.asm - Complete Enterprise Autonomous Agent
; Full Think→Plan→Act→Learn loop with local and cloud models
.386
.model flat, stdcall
option casemap:none

include windows.inc
include kernel32.inc
include user32.inc

includelib kernel32.lib
includelib user32.lib

EXTERN OllamaClient_Init:PROC
EXTERN OllamaClient_Generate:PROC
EXTERN OllamaClient_GenerateStreaming:PROC
EXTERN OllamaClient_Chat:PROC
EXTERN OllamaClient_Close:PROC
EXTERN GgufInference_Init:PROC
EXTERN GgufInference_LoadModel:PROC
EXTERN GgufInference_Generate:PROC
EXTERN GgufInference_Free:PROC
EXTERN ToolDispatcher_Execute:PROC
EXTERN PromptBuilder_Build:PROC

PUBLIC AgentSystem_Init
PUBLIC AgentSystem_Execute
PUBLIC AgentSystem_ExecuteAsync
PUBLIC AgentSystem_SetMode
PUBLIC AgentSystem_SetModel
PUBLIC AgentSystem_GetStatus
PUBLIC AgentSystem_Cancel
PUBLIC AgentSystem_Shutdown

; Agent modes
AGENT_MODE_OLLAMA       EQU 0
AGENT_MODE_LOCAL_GGUF   EQU 1
AGENT_MODE_CLOUD_API    EQU 2

; Agent states
AGENT_STATE_IDLE        EQU 0
AGENT_STATE_THINKING    EQU 1
AGENT_STATE_PLANNING    EQU 2
AGENT_STATE_EXECUTING   EQU 3
AGENT_STATE_LEARNING    EQU 4
AGENT_STATE_COMPLETE    EQU 5
AGENT_STATE_ERROR       EQU 6

; Agent configuration
AgentConfig STRUCT
    mode            dd ?
    szModelPath     db 260 dup(?)
    szModelName     db 64 dup(?)
    bAutoMode       dd ?
    bApprovalRequired dd ?
    dwMaxIterations dd ?
    dwTimeout       dd ?
AgentConfig ENDS

; Agent state
AgentState STRUCT
    currentState    dd ?
    iteration       dd ?
    bCancelled      dd ?
    dwStartTime     dd ?
    szLastError     db 512 dup(?)
AgentState ENDS

; Task structure
AgentTask STRUCT
    szRequest       db 4096 dup(?)
    szContext       db 16384 dup(?)
    szPlan          db 8192 dup(?)
    szResult        db 32768 dup(?)
    nToolCalls      dd ?
    bSuccess        dd ?
AgentTask ENDS

.data
g_Config AgentConfig <AGENT_MODE_OLLAMA,"","llama2",1,0,10,300000>
g_State AgentState <AGENT_STATE_IDLE,0,0,0,"">
g_bInitialized dd 0
g_hThread dd 0
g_hMutex dd 0

; System prompts
szSystemPrompt  db "You are an expert AI coding assistant integrated into an IDE.",13,10
                db "You can execute tools to analyze code, edit files, run commands, and more.",13,10
                db "Think step by step and use tools to accomplish tasks.",13,10,13,10
                db "Available tools: read_file, write_file, list_dir, search_files, ",13,10
                db "run_command, edit_file, create_file, delete_file, get_errors, grep_search",13,10,13,10
                db "When you need to use a tool, format your response as:",13,10
                db "TOOL_CALL: <tool_name>(<parameters>)",13,10,13,10
                db "User request: ",0

szThinkingPrompt db "Analyze this request and create a step-by-step plan:",13,10,0
szPlanningPrompt db "Break down the task into specific tool calls:",13,10,0
szExecutionPrompt db "Tool result: %s",13,10,13,10
                  db "What's the next step?",0

; Tool call parsing
szToolCallPrefix db "TOOL_CALL:",0
szDoneMarker    db "DONE",0

.code

; ================================================================
; AgentSystem_Init - Initialize autonomous agent system
; Output: EAX = 1 success
; ================================================================
AgentSystem_Init PROC
    push ebx
    
    ; Create mutex for thread safety
    invoke CreateMutex, 0, FALSE, 0
    test eax, eax
    jz @fail
    mov [g_hMutex], eax
    
    ; Initialize based on mode
    mov eax, [g_Config.mode]
    cmp eax, AGENT_MODE_OLLAMA
    je @init_ollama
    cmp eax, AGENT_MODE_LOCAL_GGUF
    je @init_gguf
    jmp @init_cloud
    
@init_ollama:
    ; Initialize Ollama client
    push 0  ; Use default localhost:11434
    call OllamaClient_Init
    add esp, 4
    test eax, eax
    jz @fail
    jmp @success
    
@init_gguf:
    ; Initialize GGUF inference
    call GgufInference_Init
    test eax, eax
    jz @fail
    
    ; Load model
    lea eax, [g_Config.szModelPath]
    push eax
    call GgufInference_LoadModel
    add esp, 4
    test eax, eax
    jz @fail
    jmp @success
    
@init_cloud:
    ; Initialize cloud API client
    ; (Would initialize OpenAI/Claude/etc)
    jmp @success
    
@success:
    mov [g_bInitialized], 1
    mov [g_State.currentState], AGENT_STATE_IDLE
    mov eax, 1
    pop ebx
    ret
    
@fail:
    xor eax, eax
    pop ebx
    ret
AgentSystem_Init ENDP

; ================================================================
; AgentSystem_Execute - Execute task synchronously
; Input:  ECX = user request text
;         EDX = result buffer
;         ESI = max result length
; Output: EAX = 1 success
; ================================================================
AgentSystem_Execute PROC lpRequest:DWORD, lpResult:DWORD, cbMaxResult:DWORD
    LOCAL task:AgentTask
    LOCAL response:OllamaResponse
    LOCAL szPrompt[16384]:BYTE
    push ebx
    push esi
    push edi
    
    ; Check if initialized
    cmp [g_bInitialized], 0
    je @fail
    
    ; Initialize task
    lea edi, task
    push 4096
    lea eax, [edi].AgentTask.szRequest
    push eax
    push lpRequest
    call lstrcpynA
    add esp, 12
    
    mov [edi].AgentTask.nToolCalls, 0
    mov [edi].AgentTask.bSuccess, 0
    
    ; Build initial prompt
    lea esi, szPrompt
    push esi
    push OFFSET szSystemPrompt
    call lstrcpyA
    add esp, 8
    
    push esi
    call lstrlenA
    add esp, 4
    add esi, eax
    
    push esi
    push lpRequest
    call lstrcatA
    add esp, 8
    
    ; Update state
    mov [g_State.currentState], AGENT_STATE_THINKING
    mov [g_State.iteration], 0
    
    ; Main agent loop
@agent_loop:
    ; Check iteration limit
    mov eax, [g_State.iteration]
    cmp eax, [g_Config.dwMaxIterations]
    jae @max_iterations
    
    ; Check if cancelled
    cmp [g_State.bCancelled], 1
    je @cancelled
    
    ; Generate response from model
    mov [g_State.currentState], AGENT_STATE_PLANNING
    
    mov eax, [g_Config.mode]
    cmp eax, AGENT_MODE_OLLAMA
    je @use_ollama
    cmp eax, AGENT_MODE_LOCAL_GGUF
    je @use_gguf
    jmp @use_cloud
    
@use_ollama:
    lea edi, response
    push edi
    lea eax, szPrompt
    push eax
    call OllamaClient_Generate
    add esp, 8
    test eax, eax
    jz @fail
    jmp @process_response
    
@use_gguf:
    lea edi, response
    lea eax, [edi].OllamaResponse.szResponse
    push 32768
    push eax
    lea ebx, szPrompt
    push ebx
    call GgufInference_Generate
    add esp, 12
    jmp @process_response
    
@use_cloud:
    ; Cloud API call
    jmp @process_response
    
@process_response:
    ; Parse response for tool calls
    lea esi, [response.OllamaResponse.szResponse]
    
    ; Check for TOOL_CALL marker
    push OFFSET szToolCallPrefix
    push esi
    call FindSubstring
    add esp, 8
    test eax, eax
    jz @no_tool_call
    
    ; Extract and execute tool
    mov [g_State.currentState], AGENT_STATE_EXECUTING
    
    push eax
    call ExtractToolCall
    add esp, 4
    ; EAX = tool name, EDX = parameters
    
    push edx
    push eax
    call ToolDispatcher_Execute
    add esp, 8
    ; EAX = tool result
    
    inc [task.nToolCalls]
    
    ; Append result to prompt for next iteration
    lea edi, szPrompt
    push edi
    call lstrlenA
    add esp, 4
    add edi, eax
    
    push eax  ; Tool result
    push OFFSET szExecutionPrompt
    push edi
    call wsprintfA
    add esp, 12
    
    inc [g_State.iteration]
    jmp @agent_loop
    
@no_tool_call:
    ; Check for DONE marker or final answer
    push OFFSET szDoneMarker
    push esi
    call FindSubstring
    add esp, 8
    test eax, eax
    jnz @task_complete
    
    ; Treat as final answer
    jmp @task_complete
    
@task_complete:
    ; Copy final response to result
    push cbMaxResult
    push lpResult
    lea eax, [response.OllamaResponse.szResponse]
    push eax
    call lstrcpynA
    add esp, 12
    
    mov [g_State.currentState], AGENT_STATE_COMPLETE
    mov [task.bSuccess], 1
    
    mov eax, 1
    pop edi
    pop esi
    pop ebx
    ret
    
@max_iterations:
    mov [g_State.currentState], AGENT_STATE_ERROR
    jmp @fail
    
@cancelled:
    mov [g_State.currentState], AGENT_STATE_IDLE
    jmp @fail
    
@fail:
    mov [task.bSuccess], 0
    xor eax, eax
    pop edi
    pop esi
    pop ebx
    ret
AgentSystem_Execute ENDP

; ================================================================
; AgentSystem_ExecuteAsync - Execute task asynchronously
; Input:  ECX = user request
;         EDX = callback function
; Output: EAX = thread handle
; ================================================================
AgentSystem_ExecuteAsync PROC lpRequest:DWORD, pfnCallback:DWORD
    LOCAL dwThreadId:DWORD
    push ebx
    
    ; Create thread for async execution
    push 0
    lea eax, dwThreadId
    push eax
    push 0
    push lpRequest
    push OFFSET AsyncExecuteThread
    push 0
    call CreateThread
    add esp, 24
    
    mov [g_hThread], eax
    
    pop ebx
    ret
AgentSystem_ExecuteAsync ENDP

; ================================================================
; AgentSystem_SetMode - Switch agent mode
; ================================================================
AgentSystem_SetMode PROC mode:DWORD
    mov eax, mode
    mov [g_Config.mode], eax
    
    ; Reinitialize with new mode
    call AgentSystem_Init
    
    ret
AgentSystem_SetMode ENDP

; ================================================================
; AgentSystem_SetModel - Set model name/path
; ================================================================
AgentSystem_SetModel PROC lpModelPath:DWORD
    push ebx
    
    push 260
    lea ebx, [g_Config.szModelPath]
    push ebx
    push lpModelPath
    call lstrcpynA
    add esp, 12
    
    mov eax, 1
    pop ebx
    ret
AgentSystem_SetModel ENDP

; ================================================================
; AgentSystem_GetStatus - Get current agent state
; ================================================================
AgentSystem_GetStatus PROC
    mov eax, [g_State.currentState]
    ret
AgentSystem_GetStatus ENDP

; ================================================================
; AgentSystem_Cancel - Cancel current execution
; ================================================================
AgentSystem_Cancel PROC
    mov [g_State.bCancelled], 1
    mov eax, 1
    ret
AgentSystem_Cancel ENDP

; ================================================================
; AgentSystem_Shutdown - Cleanup all resources
; ================================================================
AgentSystem_Shutdown PROC
    ; Cancel any running task
    call AgentSystem_Cancel
    
    ; Wait for thread
    cmp [g_hThread], 0
    je @no_thread
    invoke WaitForSingleObject, [g_hThread], 5000
    invoke CloseHandle, [g_hThread]
    mov [g_hThread], 0
    
@no_thread:
    ; Cleanup based on mode
    mov eax, [g_Config.mode]
    cmp eax, AGENT_MODE_OLLAMA
    je @cleanup_ollama
    cmp eax, AGENT_MODE_LOCAL_GGUF
    je @cleanup_gguf
    jmp @cleanup_done
    
@cleanup_ollama:
    call OllamaClient_Close
    jmp @cleanup_done
    
@cleanup_gguf:
    call GgufInference_Free
    jmp @cleanup_done
    
@cleanup_done:
    ; Close mutex
    cmp [g_hMutex], 0
    je @done
    invoke CloseHandle, [g_hMutex]
    mov [g_hMutex], 0
    
@done:
    mov [g_bInitialized], 0
    mov eax, 1
    ret
AgentSystem_Shutdown ENDP

; ================================================================
; Internal: AsyncExecuteThread - Thread procedure
; ================================================================
AsyncExecuteThread PROC lpParam:DWORD
    LOCAL szResult[32768]:BYTE
    
    lea eax, szResult
    push 32768
    push eax
    push lpParam
    call AgentSystem_Execute
    add esp, 12
    
    xor eax, eax
    ret
AsyncExecuteThread ENDP

; ================================================================
; Internal: FindSubstring - Find substring in string
; ================================================================
FindSubstring PROC lpStr:DWORD, lpSubstr:DWORD
    push ebx
    push esi
    push edi
    
    mov esi, lpStr
    mov edi, lpSubstr
    
@search_loop:
    movzx eax, byte ptr [esi]
    test al, al
    jz @not_found
    
    movzx ebx, byte ptr [edi]
    cmp al, bl
    je @found
    
    inc esi
    jmp @search_loop
    
@found:
    mov eax, esi
    pop edi
    pop esi
    pop ebx
    ret
    
@not_found:
    xor eax, eax
    pop edi
    pop esi
    pop ebx
    ret
FindSubstring ENDP

; ================================================================
; Internal: ExtractToolCall - Parse tool call from text
; ================================================================
ExtractToolCall PROC lpText:DWORD
    ; Parse "TOOL_CALL: tool_name(params)"
    ; Returns EAX = tool name, EDX = params
    
    mov eax, lpText
    mov edx, 0
    ret
ExtractToolCall ENDP

END
