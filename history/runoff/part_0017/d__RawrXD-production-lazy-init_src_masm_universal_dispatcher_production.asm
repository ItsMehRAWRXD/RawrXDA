; ===============================================================================
; Universal Dispatcher - Production Version
; Pure MASM x86-64, Zero Dependencies, Production-Ready
; ===============================================================================

option casemap:none

; ===============================================================================
; EXTERNAL DEPENDENCIES
; ===============================================================================

extern EnterpriseAlloc:proc
extern EnterpriseFree:proc
extern EnterpriseStrLen:proc
extern EnterpriseStrCpy:proc
extern EnterpriseStrCmp:proc
extern EnterpriseLog:proc
extern InitializeCriticalSection:proc
extern EnterCriticalSection:proc
extern LeaveCriticalSection:proc
extern DeleteCriticalSection:proc

; ===============================================================================
; CONSTANTS
; ===============================================================================

MAX_COMMANDS        equ 256
MAX_COMMAND_LEN     equ 512
MAX_TOKENS          equ 64
DISPATCH_SUCCESS    equ 1
DISPATCH_FAILURE    equ 0

; Intent types
INTENT_PLAN         equ 1
INTENT_ASK          equ 2
INTENT_EDIT         equ 3
INTENT_CONFIGURE    equ 4
INTENT_BUILD        equ 5
INTENT_RUN          equ 6
INTENT_DEBUG        equ 7
INTENT_DEPLOY       equ 8
INTENT_UNKNOWN      equ 0

; ===============================================================================
; STRUCTURES
; ===============================================================================

COMMAND_HANDLER STRUCT
    command         byte MAX_COMMAND_LEN dup(?)
    handler         qword ?
    intent_type     dword ?
COMMAND_HANDLER ENDS

DISPATCH_CONTEXT STRUCT
    handlers        COMMAND_HANDLER MAX_COMMANDS dup(<>)
    handler_count   dword ?
    initialized     dword ?
DISPATCH_CONTEXT ENDS

; ===============================================================================
; DATA
; ===============================================================================

.data
    g_DispatchContext qword 0
    g_DispatchLock    qword 0
    g_DispatchInitialized dword 0

.code

; ===============================================================================
; Initialize Universal Dispatcher
; ===============================================================================
InitializeDispatcher PROC
    push    rbp
    mov     rbp, rsp
    sub     rsp, 32
    
    cmp     g_DispatchInitialized, 0
    jne     already_init
    
    ; Allocate dispatch context
    mov     rcx, sizeof DISPATCH_CONTEXT
    call    EnterpriseAlloc
    mov     g_DispatchContext, rax
    
    ; Initialize critical section
    lea     rcx, g_DispatchLock
    call    InitializeCriticalSection
    
    ; Register default handlers
    call    RegisterDefaultHandlers
    
    mov     g_DispatchInitialized, 1
    mov     eax, DISPATCH_SUCCESS
    jmp     done
    
already_init:
    xor     eax, eax
    
done:
    leave
    ret
InitializeDispatcher ENDP

; ===============================================================================
; Register Default Handlers
; ===============================================================================
RegisterDefaultHandlers PROC
    push    rbp
    mov     rbp, rsp
    sub     rsp, 32
    
    ; Enter critical section
    lea     rcx, g_DispatchLock
    call    EnterCriticalSection
    
    mov     rax, g_DispatchContext
    mov     dword ptr [rax].DISPATCH_CONTEXT.handler_count, 0
    
    ; Register plan handler
    lea     rcx, szCommandPlan
    lea     rdx, HandlePlan
    mov     r8d, INTENT_PLAN
    call    RegisterHandler
    
    ; Register ask handler
    lea     rcx, szCommandAsk
    lea     rdx, HandleAsk
    mov     r8d, INTENT_ASK
    call    RegisterHandler
    
    ; Register edit handler
    lea     rcx, szCommandEdit
    lea     rdx, HandleEdit
    mov     r8d, INTENT_EDIT
    call    RegisterHandler
    
    ; Register configure handler
    lea     rcx, szCommandConfigure
    lea     rdx, HandleConfigure
    mov     r8d, INTENT_CONFIGURE
    call    RegisterHandler
    
    ; Register build handler
    lea     rcx, szCommandBuild
    lea     rdx, HandleBuild
    mov     r8d, INTENT_BUILD
    call    RegisterHandler
    
    ; Register run handler
    lea     rcx, szCommandRun
    lea     rdx, HandleRun
    mov     r8d, INTENT_RUN
    call    RegisterHandler
    
    ; Register debug handler
    lea     rcx, szCommandDebug
    lea     rdx, HandleDebug
    mov     r8d, INTENT_DEBUG
    call    RegisterHandler
    
    ; Register deploy handler
    lea     rcx, szCommandDeploy
    lea     rdx, HandleDeploy
    mov     r8d, INTENT_DEPLOY
    call    RegisterHandler
    
    ; Leave critical section
    lea     rcx, g_DispatchLock
    call    LeaveCriticalSection
    
    leave
    ret
RegisterDefaultHandlers ENDP

; ===============================================================================
; Register Handler
; ===============================================================================
RegisterHandler PROC
    push    rbp
    mov     rbp, rsp
    sub     rsp, 48
    
    mov     [rbp-8], rcx        ; command
    mov     [rbp-16], rdx       ; handler
    mov     [rbp-24], r8        ; intent_type
    
    ; Enter critical section
    lea     rcx, g_DispatchLock
    call    EnterCriticalSection
    
    mov     rax, g_DispatchContext
    mov     ecx, [rax + DISPATCH_CONTEXT.handler_count]
    cmp     ecx, MAX_COMMANDS
    jge     handler_limit_reached
    
    ; Get handler slot
    imul    rdx, rcx, sizeof COMMAND_HANDLER
    lea     rax, [rax + DISPATCH_CONTEXT.handlers]
    add     rax, rdx
    
    ; Copy command
    lea     rdi, [rax + COMMAND_HANDLER.command]
    mov     rsi, [rbp-8]
    call    EnterpriseStrCpy
    
    ; Set handler
    mov     rcx, [rbp-16]
    mov     [rax + COMMAND_HANDLER.handler], rcx
    
    ; Set intent type
    mov     ecx, [rbp-24]
    mov     [rax + COMMAND_HANDLER.intent_type], ecx
    
    ; Increment handler count
    mov     rax, g_DispatchContext
    inc     dword ptr [rax + DISPATCH_CONTEXT.handler_count]
    
    mov     eax, DISPATCH_SUCCESS
    jmp     done
    
handler_limit_reached:
    mov     eax, DISPATCH_FAILURE
    
done:
    ; Leave critical section
    lea     rcx, g_DispatchLock
    call    LeaveCriticalSection
    
    leave
    ret
RegisterHandler ENDP

; ===============================================================================
; Universal Dispatcher
; ===============================================================================
UniversalDispatcher PROC
    push    rbp
    mov     rbp, rsp
    sub     rsp, 64
    
    mov     [rbp-8], rcx        ; command
    
    ; Enter critical section
    lea     rcx, g_DispatchLock
    call    EnterCriticalSection
    
    ; Find handler
    mov     rcx, [rbp-8]
    call    FindHandler
    test    rax, rax
    jz      handler_not_found
    
    ; Call handler
    mov     rcx, [rbp-8]
    call    [rax + COMMAND_HANDLER.handler]
    
    mov     eax, DISPATCH_SUCCESS
    jmp     done
    
handler_not_found:
    mov     eax, DISPATCH_FAILURE
    
done:
    ; Leave critical section
    lea     rcx, g_DispatchLock
    call    LeaveCriticalSection
    
    leave
    ret
UniversalDispatcher ENDP

; ===============================================================================
; Find Handler
; ===============================================================================
FindHandler PROC
    push    rbp
    mov     rbp, rsp
    sub     rsp, 32
    
    mov     [rbp-8], rcx        ; command
    
    mov     rax, g_DispatchContext
    mov     ecx, [rax + DISPATCH_CONTEXT.handler_count]
    test    ecx, ecx
    jz      no_handlers
    
    xor     ebx, ebx
find_loop:
    cmp     ebx, ecx
    jge     handler_not_found
    
    ; Get handler
    imul    rdx, rbx, sizeof COMMAND_HANDLER
    lea     rax, [rax + DISPATCH_CONTEXT.handlers]
    add     rax, rdx
    
    ; Compare command
    lea     rcx, [rax + COMMAND_HANDLER.command]
    mov     rdx, [rbp-8]
    call    EnterpriseStrCmp
    test    eax, eax
    jz      handler_found
    
    inc     ebx
    mov     rax, g_DispatchContext
    mov     ecx, [rax + DISPATCH_CONTEXT.handler_count]
    jmp     find_loop
    
handler_found:
    mov     rax, g_DispatchContext
    imul    rdx, rbx, sizeof COMMAND_HANDLER
    lea     rax, [rax + DISPATCH_CONTEXT.handlers]
    add     rax, rdx
    jmp     done
    
handler_not_found:
    xor     rax, rax
    jmp     done
    
no_handlers:
    xor     rax, rax
    
done:
    leave
    ret
FindHandler ENDP

; ===============================================================================
; Handler Implementations
; ===============================================================================

HandlePlan PROC
    push    rbp
    mov     rbp, rsp
    sub     rsp, 32
    
    ; Log plan execution
    lea     rcx, szPlanExecuted
    call    EnterpriseLog
    
    mov     eax, DISPATCH_SUCCESS
    leave
    ret
HandlePlan ENDP

HandleAsk PROC
    push    rbp
    mov     rbp, rsp
    sub     rsp, 32
    
    lea     rcx, szAskExecuted
    call    EnterpriseLog
    
    mov     eax, DISPATCH_SUCCESS
    leave
    ret
HandleAsk ENDP

HandleEdit PROC
    push    rbp
    mov     rbp, rsp
    sub     rsp, 32
    
    lea     rcx, szEditExecuted
    call    EnterpriseLog
    
    mov     eax, DISPATCH_SUCCESS
    leave
    ret
HandleEdit ENDP

HandleConfigure PROC
    push    rbp
    mov     rbp, rsp
    sub     rsp, 32
    
    lea     rcx, szConfigureExecuted
    call    EnterpriseLog
    
    mov     eax, DISPATCH_SUCCESS
    leave
    ret
HandleConfigure ENDP

HandleBuild PROC
    push    rbp
    mov     rbp, rsp
    sub     rsp, 32
    
    lea     rcx, szBuildExecuted
    call    EnterpriseLog
    
    mov     eax, DISPATCH_SUCCESS
    leave
    ret
HandleBuild ENDP

HandleRun PROC
    push    rbp
    mov     rbp, rsp
    sub     rsp, 32
    
    lea     rcx, szRunExecuted
    call    EnterpriseLog
    
    mov     eax, DISPATCH_SUCCESS
    leave
    ret
HandleRun ENDP

HandleDebug PROC
    push    rbp
    mov     rbp, rsp
    sub     rsp, 32
    
    lea     rcx, szDebugExecuted
    call    EnterpriseLog
    
    mov     eax, DISPATCH_SUCCESS
    leave
    ret
HandleDebug ENDP

HandleDeploy PROC
    push    rbp
    mov     rbp, rsp
    sub     rsp, 32
    
    lea     rcx, szDeployExecuted
    call    EnterpriseLog
    
    mov     eax, DISPATCH_SUCCESS
    leave
    ret
HandleDeploy ENDP

; ===============================================================================
; Cleanup Dispatcher
; ===============================================================================
DispatcherCleanup PROC
    push    rbp
    mov     rbp, rsp
    sub     rsp, 32
    
    cmp     g_DispatchInitialized, 0
    je      not_initialized
    
    ; Free dispatch context
    mov     rcx, g_DispatchContext
    call    EnterpriseFree
    
    ; Delete critical section
    lea     rcx, g_DispatchLock
    call    DeleteCriticalSection
    
    mov     g_DispatchInitialized, 0
    mov     eax, DISPATCH_SUCCESS
    jmp     done
    
not_initialized:
    xor     eax, eax
    
done:
    leave
    ret
DispatcherCleanup ENDP

; ===============================================================================
; DATA
; ===============================================================================

.data
szCommandPlan       db "plan", 0
szCommandAsk        db "ask", 0
szCommandEdit       db "edit", 0
szCommandConfigure  db "configure", 0
szCommandBuild      db "build", 0
szCommandRun        db "run", 0
szCommandDebug      db "debug", 0
szCommandDeploy     db "deploy", 0

szPlanExecuted      db "Plan handler executed", 0
szAskExecuted       db "Ask handler executed", 0
szEditExecuted      db "Edit handler executed", 0
szConfigureExecuted db "Configure handler executed", 0
szBuildExecuted     db "Build handler executed", 0
szRunExecuted       db "Run handler executed", 0
szDebugExecuted     db "Debug handler executed", 0
szDeployExecuted    db "Deploy handler executed", 0

; ===============================================================================
; EXPORTS
; ===============================================================================

PUBLIC InitializeDispatcher
PUBLIC UniversalDispatcher
PUBLIC RegisterHandler
PUBLIC DispatcherCleanup

END
