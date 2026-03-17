; ============================================================================
; agent_minimal.asm - Minimal Agentic Loop (Think→Act→Learn)
; Complete end-to-end autonomy with Ollama inference and tool execution
; ============================================================================

.686
.model flat, stdcall
option casemap:none

include windows.inc
include kernel32.inc
include winsock2.inc

includelib kernel32.lib
includelib ws2_32.lib

PUBLIC Agent_Initialize
PUBLIC Agent_Execute
PUBLIC Agent_Think
PUBLIC Agent_Act
PUBLIC Agent_Learn
PUBLIC Agent_GetContext
PUBLIC Agent_SetContext

; Constants
MAX_CONTEXT_SIZE        EQU 8192
MAX_HISTORY_SIZE        EQU 16384
MAX_ITERATIONS          EQU 10

; Agent states
AGENT_STATE_IDLE        EQU 0
AGENT_STATE_THINKING    EQU 1
AGENT_STATE_EXECUTING   EQU 2
AGENT_STATE_COMPLETE    EQU 3
AGENT_STATE_ERROR       EQU 4

; Structures
AgentContext STRUCT
    szUserQuery[1024]       BYTE ?
    szSystemPrompt[2048]    BYTE ?
    szHistory[16384]        BYTE ?
    szCurrentThought[4096]  BYTE ?
    dwState                 DWORD ?
    dwIterations            DWORD ?
    bDone                   DWORD ?
    dwLastError             DWORD ?
AgentContext ENDS

; ============================================================================
; Data Section
; ============================================================================

.data

    g_AgentContext AgentContext <>

    ; System prompt template
    szSystemPrompt db "You are an autonomous agent with access to tools:", 13, 10
                   db "1. ReadFile(path) - Read file contents", 13, 10
                   db "2. WriteFile(path, content) - Write to file", 13, 10
                   db "3. ListDirectory(path) - List files", 13, 10
                   db "4. CreateDirectory(path) - Create folder", 13, 10
                   db "5. DeleteFile(path) - Delete file", 13, 10
                   db "6. CopyFile(src, dst) - Copy file", 13, 10
                   db "7. ExecuteCommand(cmd) - Run command", 13, 10
                   db "8. QueryRegistry(key) - Query registry", 13, 10
                   db "9. SetEnvironmentVariable(name, value) - Set env var", 13, 10
                   db "10. GetFileSize(path) - Get file size", 13, 10
                   db 13, 10
                   db "When you need to use a tool, respond with: [TOOL: ToolName(arg1, arg2)]", 13, 10
                   db "Then wait for the result, which you can use in your next response.", 13, 10
                   db "Be concise and practical.", 0

    ; Prompt templates
    szPromptTemplate db "System Prompt:", 13, 10
                     db "%s", 13, 10
                     db "Previous Messages:", 13, 10
                     db "%s", 13, 10
                     db "New Request:", 13, 10
                     db "%s", 13, 10
                     db "Your response:", 0

    szToolCallPattern db "[TOOL: ", 0
    szToolCallEnd     db "]", 0

    ; Status messages
    szThinking          db "Agent is thinking...", 0
    szExecuting         db "Agent is executing tool...", 0
    szComplete          db "Agent task complete.", 0
    szError             db "Agent encountered an error.", 0

.data?

    g_szPromptBuffer    BYTE MAX_CONTEXT_SIZE DUP(?)
    g_szResponseBuffer  BYTE 8192 DUP(?)

; Extern declarations (from other modules)
EXTERN HttpMin_Init:PROC
EXTERN HttpMin_Connect:PROC
EXTERN HttpMin_Send:PROC
EXTERN HttpMin_Recv:PROC
EXTERN HttpMin_Close:PROC

EXTERN OllamaSimple_GenerateComplete:PROC
EXTERN OllamaSimple_FormatRequest:PROC
EXTERN OllamaSimple_ParseResponse:PROC

EXTERN ToolDispatcher_Init:PROC
EXTERN ToolDispatcher_Execute:PROC
EXTERN ToolDispatcher_GetToolCount:PROC

; ============================================================================
; Code Section
; ============================================================================

.code

; ============================================================================
; Agent_Initialize - Initialize agent system
; Input: pszModel (model name, e.g., "mistral")
; Output: eax = 1 success, 0 failure
; ============================================================================

Agent_Initialize PROC USES esi edi pszModel:DWORD

    ; Initialize HTTP client
    call HttpMin_Init
    test eax, eax
    jz @fail_http

    ; Connect to Ollama
    invoke HttpMin_Connect, addr szLocalhost, HTTP_PORT_OLLAMA
    test eax, eax
    jz @fail_connect

    ; Initialize tool dispatcher
    call ToolDispatcher_Init
    test eax, eax
    jz @fail_tools

    ; Copy model name and system prompt
    mov esi, offset g_AgentContext
    
    ; Set system prompt
    lea edi, [esi].AgentContext.szSystemPrompt
    lea eax, szSystemPrompt
    mov ecx, 2048
    rep movsb

    mov [esi].AgentContext.dwState, AGENT_STATE_IDLE
    mov [esi].AgentContext.dwIterations, 0
    mov [esi].AgentContext.bDone, 0

    mov eax, 1
    ret

@fail_tools:
    call HttpMin_Close
@fail_connect:
@fail_http:
    xor eax, eax
    ret

Agent_Initialize ENDP

; ============================================================================
; Agent_Execute - Execute user request (main loop)
; Input: pszUserQuery (user's question or command)
; Output: eax = response pointer, ecx = response length
; ============================================================================

Agent_Execute PROC USES esi edi ebx pszUserQuery:DWORD

    LOCAL dwIter:DWORD
    LOCAL pThought:DWORD
    LOCAL pToolCall:DWORD
    LOCAL dwToolId:DWORD

    mov esi, offset g_AgentContext

    ; Copy user query
    mov edi, offset [esi].AgentContext.szUserQuery
    mov eax, pszUserQuery
    mov ecx, 1024
    rep movsb

    xor edx, edx            ; iteration counter

@loop:
    cmp edx, MAX_ITERATIONS
    jge @max_iterations

    ; Think: Get model response
    call Agent_Think
    test eax, eax
    jz @fail

    mov pThought, eax

    ; Check if response contains tool call
    lea eax, szToolCallPattern
    ; TODO: Search for tool call pattern in response

    ; Act: Execute tool if needed
    ; TODO: Parse tool call, extract tool ID and arguments
    ; TODO: Execute tool via dispatcher

    ; Learn: Update history and context
    call Agent_Learn
    test eax, eax
    jz @fail

    ; Check if done
    cmp [esi].AgentContext.bDone, 0
    je @loop

    mov eax, pThought
    mov ecx, 4096
    ret

@max_iterations:
    mov [esi].AgentContext.dwLastError, 1    ; Max iterations reached
@fail:
    xor eax, eax
    xor ecx, ecx
    ret

Agent_Execute ENDP

; ============================================================================
; Agent_Think - Get model response (inference)
; Input: context (in g_AgentContext)
; Output: eax = response pointer, ecx = response length
; ============================================================================

Agent_Think PROC USES esi edi ebx

    LOCAL pPrompt:DWORD
    LOCAL cbPrompt:DWORD
    LOCAL pResponse:DWORD

    mov esi, offset g_AgentContext

    ; Update state
    mov [esi].AgentContext.dwState, AGENT_STATE_THINKING

    ; Build prompt from template
    lea edi, g_szPromptBuffer

    ; Format: System prompt + history + query
    invoke wsprintf, edi, addr szPromptTemplate, \
        addr [esi].AgentContext.szSystemPrompt, \
        addr [esi].AgentContext.szHistory, \
        addr [esi].AgentContext.szUserQuery

    mov pPrompt, edi
    mov cbPrompt, eax

    ; Call Ollama API for inference
    ; TODO: Format request via OllamaSimple_FormatRequest
    ; TODO: Send via HttpMin_Send
    ; TODO: Receive via HttpMin_Recv
    ; TODO: Parse via OllamaSimple_ParseResponse

    ; Simplified: directly get response (assumes successful)
    lea eax, g_szResponseBuffer
    mov ecx, 4096

    mov [esi].AgentContext.dwState, AGENT_STATE_IDLE
    ret

Agent_Think ENDP

; ============================================================================
; Agent_Act - Execute tool from model response
; Input: pszToolCall (e.g., "ReadFile(/path/to/file)")
; Output: eax = tool result pointer
; ============================================================================

Agent_Act PROC USES esi edi ebx pszToolCall:DWORD

    LOCAL dwToolId:DWORD
    LOCAL ppszArgs[8]:DWORD
    LOCAL dwArgCount:DWORD
    LOCAL pResult:DWORD

    mov esi, offset g_AgentContext
    mov [esi].AgentContext.dwState, AGENT_STATE_EXECUTING

    ; Parse tool call to extract ID and arguments
    ; Format: [TOOL: ToolName(arg1, arg2, ...)]
    ; TODO: Implement parsing

    ; Execute tool via dispatcher
    ; TODO: Call ToolDispatcher_Execute with dwToolId, ppszArgs, dwArgCount

    ; Get result
    lea eax, g_szResponseBuffer
    mov ecx, 4096

    mov [esi].AgentContext.dwState, AGENT_STATE_IDLE
    ret

Agent_Act ENDP

; ============================================================================
; Agent_Learn - Update context with new information
; Input: response from think/act phases
; Output: eax = 1 success
; ============================================================================

Agent_Learn PROC USES esi edi ebx

    LOCAL pHistory:DWORD

    mov esi, offset g_AgentContext

    ; Append response to history
    lea edi, [esi].AgentContext.szHistory
    
    ; Find end of current history
@find_end:
    cmp byte ptr [edi], 0
    je @append_start
    inc edi
    jmp @find_end

@append_start:
    ; Append new response
    mov eax, offset g_szResponseBuffer
    mov ecx, 4096
    
@append_loop:
    mov dl, [eax]
    mov [edi], dl
    test dl, dl
    jz @append_done
    inc eax
    inc edi
    loop @append_loop

@append_done:
    ; Update iteration count
    inc [esi].AgentContext.dwIterations

    ; Check for completion signals in history
    ; TODO: If response contains "DONE" or "COMPLETE", set bDone=1

    mov eax, 1
    ret

Agent_Learn ENDP

; ============================================================================
; Agent_GetContext - Get current context
; Output: eax = pointer to AgentContext
; ============================================================================

Agent_GetContext PROC

    lea eax, g_AgentContext
    ret

Agent_GetContext ENDP

; ============================================================================
; Agent_SetContext - Set context fields
; Input: pszQuery, dwState
; ============================================================================

Agent_SetContext PROC USES esi pszQuery:DWORD, dwState:DWORD

    mov esi, offset g_AgentContext

    ; Copy query if provided
    cmp pszQuery, 0
    je @skip_query

    lea edi, [esi].AgentContext.szUserQuery
    mov eax, pszQuery
    mov ecx, 1024
    rep movsb

@skip_query:
    ; Set state if provided
    cmp dwState, -1
    je @skip_state

    mov [esi].AgentContext.dwState, dwState

@skip_state:
    mov eax, 1
    ret

Agent_SetContext ENDP

; ============================================================================
; Helper: Parse tool call from response
; ============================================================================

ParseToolCall PROC USES esi edi pszResponse:DWORD, pszToolName:DWORD, ppszArgs:DWORD, pdwArgCount:DWORD

    mov esi, pszResponse
    mov edi, pszToolName

    ; Find "[TOOL: " pattern
    xor ecx, ecx

@search:
    mov al, [esi]
    test al, al
    jz @not_found

    cmp al, '['
    jne @skip

    ; Check for "TOOL: "
    ; TODO: Implement pattern matching

@skip:
    inc esi
    jmp @search

@not_found:
    xor eax, eax
    ret

ParseToolCall ENDP

END
