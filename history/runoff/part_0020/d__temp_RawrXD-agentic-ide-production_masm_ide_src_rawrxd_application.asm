;======================================================================
; RawrXD IDE - Application Core Management
; Manages application state, initialization, and cleanup
;======================================================================
INCLUDE rawrxd_includes.inc

.CONST
APP_STATE_INITIALIZING  EQU 1
APP_STATE_RUNNING       EQU 2
APP_STATE_SHUTTING_DOWN EQU 3

.DATA
g_appState              DD APP_STATE_INITIALIZING
g_appStartTime          DQ ?
g_sessionHandle         DQ ?
g_configData            DQ ?

.CODE

;----------------------------------------------------------------------
; RawrXD_Application_Create - Initialize application
;----------------------------------------------------------------------
RawrXD_Application_Create PROC
    LOCAL pApp:QWORD
    
    ; Allocate application context
    INVOKE HeapAlloc, GetProcessHeap(), HEAP_ZERO_MEMORY, 256
    mov pApp, rax
    test rax, rax
    jz @@fail
    
    ; Record startup time
    INVOKE GetTickCount64
    mov g_appStartTime, rax
    
    ; Initialize text engine (GapBuffer, Tokenizer, etc.)
    INVOKE GapBuffer_Init
    INVOKE Tokenizer_Init
    INVOKE UndoStack_Init
    INVOKE Search_Init
    INVOKE Renderer_Init
    INVOKE Theme_Init
    
    ; Initialize session manager
    INVOKE Session_Init
    
    ; Initialize LSP client
    INVOKE LSPClient_Init
    
    ; Initialize agent bridge
    INVOKE AgentBridge_Init
    
    ; Set application state to running
    mov g_appState, APP_STATE_RUNNING
    
    mov rax, pApp
    ret
    
@@fail:
    xor rax, rax
    ret
    
RawrXD_Application_Create ENDP

;----------------------------------------------------------------------
; RawrXD_Application_Destroy - Clean up application
;----------------------------------------------------------------------
RawrXD_Application_Destroy PROC pApp:QWORD
    
    mov g_appState, APP_STATE_SHUTTING_DOWN
    
    ; Close all editor windows
    test g_hEditorWnd, g_hEditorWnd
    jz @@skip_editor
    INVOKE RawrXD_Editor_Destroy, g_hEditorWnd
    
@@skip_editor:
    ; Destroy menu system
    INVOKE RawrXD_Menu_Destroy
    
    ; Free application context
    test pApp, pApp
    jz @@skip_free
    INVOKE HeapFree, GetProcessHeap(), 0, pApp
    
@@skip_free:
    ret
    
RawrXD_Application_Destroy ENDP

;----------------------------------------------------------------------
; RawrXD_Application_IsRunning - Check if app is running
;----------------------------------------------------------------------
RawrXD_Application_IsRunning PROC
    mov eax, g_appState
    cmp eax, APP_STATE_RUNNING
    sete al
    movzx rax, al
    ret
RawrXD_Application_IsRunning ENDP

;----------------------------------------------------------------------
; RawrXD_Application_GetUptime - Get application uptime in ms
;----------------------------------------------------------------------
RawrXD_Application_GetUptime PROC
    INVOKE GetTickCount64
    sub rax, g_appStartTime
    ret
RawrXD_Application_GetUptime ENDP

END
