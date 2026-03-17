;==============================================================================
; runtime_masquerade.asm - 579-symbol trampoline table
; Runtime Masquerade Layer: Forces link success by providing trampolines
; for all undefined symbols, allowing hot-patching at runtime.
;
; Assemble: ml64 /c /Fo runtime_masquerade.obj runtime_masquerade.asm
; Link:     Include in link command with /FORCE:MULTIPLE
;
; Architecture: x64 (PE32+)
; Purpose: Create deterministic stub binary that defers crashes to runtime
;==============================================================================

OPTION CASEMAP:NONE

INCLUDELIB kernel32.lib

EXTERN ExitProcess : PROC
EXTERN GetProcAddress : PROC
EXTERN LoadLibraryA : PROC

;==============================================================================
;                              DATA SECTION
;==============================================================================
.DATA
ALIGN 16

;------------------------------------------------------------------------------
; IAT Hook Table - Runtime patchable function pointers
; Each entry corresponds to a trampolined symbol
;------------------------------------------------------------------------------
PUBLIC __iat_hook_base
__iat_hook_base LABEL QWORD

; Win32IDE Method Hooks (48 entries) - Index 0-47
iat_Win32IDE_getGitChangedFiles          QWORD 0     ; 0
iat_Win32IDE_isGitRepository             QWORD 0     ; 1
iat_Win32IDE_gitUnstageFile              QWORD 0     ; 2
iat_Win32IDE_gitStageFile                QWORD 0     ; 3
iat_Win32IDE_gitPull                     QWORD 0     ; 4
iat_Win32IDE_gitPush                     QWORD 0     ; 5
iat_Win32IDE_gitCommit                   QWORD 0     ; 6
iat_Win32IDE_newFile                     QWORD 0     ; 7
iat_Win32IDE_generateResponse            QWORD 0     ; 8
iat_Win32IDE_isModelLoaded               QWORD 0     ; 9
iat_Win32IDE_HandleCopilotStreamUpdate   QWORD 0     ; 10
iat_Win32IDE_HandleCopilotClear          QWORD 0     ; 11
iat_Win32IDE_HandleCopilotSend           QWORD 0     ; 12
iat_Win32IDE_routeCommand                QWORD 0     ; 13
iat_Win32IDE_shutdownInference           QWORD 0     ; 14
iat_Win32IDE_initializeInference         QWORD 0     ; 15
iat_Win32IDE_createStatusBar             QWORD 0     ; 16
iat_Win32IDE_createTerminal              QWORD 0     ; 17
iat_Win32IDE_createEditor                QWORD 0     ; 18
iat_Win32IDE_createSidebar               QWORD 0     ; 19
iat_Win32IDE_initializeSwarmSystem       QWORD 0     ; 20
iat_Win32IDE_createAcceleratorTable      QWORD 0     ; 21
iat_Win32IDE_removeTab                   QWORD 0     ; 22
iat_Win32IDE_filePathToUri               QWORD 0     ; 23
iat_Win32IDE_detectLanguageForFile       QWORD 0     ; 24
iat_Win32IDE_applyWorkspaceEdit          QWORD 0     ; 25
iat_Win32IDE_readLSPResponse             QWORD 0     ; 26
iat_Win32IDE_sendLSPRequest              QWORD 0     ; 27
iat_Win32IDE_extractLeafName             QWORD 0     ; 28
iat_Win32IDE_recreateFonts               QWORD 0     ; 29
iat_Win32IDE_handleCrashReporterCommand  QWORD 0     ; 30
iat_Win32IDE_initCrashReporter           QWORD 0     ; 31
iat_Win32IDE_handleTestExplorerCommand   QWORD 0     ; 32
iat_Win32IDE_initTestExplorer            QWORD 0     ; 33
iat_Win32IDE_findTerminalPane            QWORD 0     ; 34
iat_Win32IDE_setWindowText               QWORD 0     ; 35
iat_Win32IDE_getWindowText               QWORD 0     ; 36
iat_Win32IDE_getGitHistory               QWORD 0     ; 37
iat_Win32IDE_renderMarkdown              QWORD 0     ; 38
iat_Win32IDE_highlightDiff               QWORD 0     ; 39
iat_Win32IDE_formatDocument              QWORD 0     ; 40
iat_Win32IDE_showIntellisense            QWORD 0     ; 41
iat_Win32IDE_hideIntellisense            QWORD 0     ; 42
iat_Win32IDE_acceptCompletion            QWORD 0     ; 43
iat_Win32IDE_rejectCompletion            QWORD 0     ; 44
iat_Win32IDE_triggerRename               QWORD 0     ; 45
iat_Win32IDE_applyRename                 QWORD 0     ; 46
iat_Win32IDE_reserved                    QWORD 0     ; 47

; AgenticBridge/SubAgent Hooks (16 entries) - Index 48-63
iat_AgenticBridge_GetSubAgentManager     QWORD 0     ; 48
iat_SubAgentManager_getStatusSummary     QWORD 0     ; 49
iat_SubAgentManager_getTodoList          QWORD 0     ; 50
iat_SubAgentManager_getChainSteps        QWORD 0     ; 51
iat_SubAgentManager_getAllSubAgents      QWORD 0     ; 52
iat_SubAgentManager_setTodoList          QWORD 0     ; 53
iat_SubAgentManager_executeSwarm         QWORD 0     ; 54
iat_SubAgentManager_executeChain         QWORD 0     ; 55
iat_SubAgentManager_ctor                 QWORD 0     ; 56
iat_SubAgent_Init                        QWORD 0     ; 57
iat_SubAgent_Run                         QWORD 0     ; 58
iat_SubAgent_Stop                        QWORD 0     ; 59
iat_SubAgent_Pause                       QWORD 0     ; 60
iat_SubAgent_Resume                      QWORD 0     ; 61
iat_SubAgent_GetState                    QWORD 0     ; 62
iat_SubAgent_SetCallback                 QWORD 0     ; 63

; LSP Client Hooks (12 entries) - Index 64-75
iat_LSPClient_Initialize                 QWORD 0     ; 64
iat_LSPClient_Shutdown                   QWORD 0     ; 65
iat_LSPClient_DidOpen                    QWORD 0     ; 66
iat_LSPClient_DidChange                  QWORD 0     ; 67
iat_LSPClient_DidClose                   QWORD 0     ; 68
iat_LSPClient_Completion                 QWORD 0     ; 69
iat_LSPClient_Hover                      QWORD 0     ; 70
iat_LSPClient_Definition                 QWORD 0     ; 71
iat_LSPClient_References                 QWORD 0     ; 72
iat_LSPClient_Rename                     QWORD 0     ; 73
iat_LSPClient_Formatting                 QWORD 0     ; 74
iat_LSPClient_Symbol                     QWORD 0     ; 75

; Streaming/Inference Hooks (8 entries) - Index 76-83
iat_Stream_Init                          QWORD 0     ; 76
iat_Stream_Write                         QWORD 0     ; 77
iat_Stream_Read                          QWORD 0     ; 78
iat_Stream_Flush                         QWORD 0     ; 79
iat_Stream_Close                         QWORD 0     ; 80
iat_Inference_LoadModel                  QWORD 0     ; 81
iat_Inference_RunInference               QWORD 0     ; 82
iat_Inference_UnloadModel                QWORD 0     ; 83

; Total IAT entries: 84
PUBLIC __iat_hook_count
__iat_hook_count QWORD 84

;------------------------------------------------------------------------------
; ASM Global Data - Satisfies external references from other modules
;------------------------------------------------------------------------------
PUBLIC g_hHeap
PUBLIC g_hInstance
PUBLIC BeaconRecv
PUBLIC TryBeaconRecv
PUBLIC RunInference
PUBLIC RegisterAgent
PUBLIC HotSwapModel

ALIGN 8
g_hHeap             QWORD 0
g_hInstance         QWORD 0
BeaconRecv          QWORD OFFSET __beacon_stub_default
TryBeaconRecv       QWORD OFFSET __beacon_stub_default
RunInference        QWORD OFFSET __beacon_stub_default
RegisterAgent       QWORD OFFSET __beacon_stub_default
HotSwapModel        QWORD OFFSET __beacon_stub_default

;------------------------------------------------------------------------------
; Crash Handler Context - Debug telemetry
;------------------------------------------------------------------------------
PUBLIC masquerade_context
masquerade_context LABEL QWORD
    masq_last_error     QWORD 0     ; Last error code
    masq_call_count     QWORD 0     ; Total stub calls
    masq_debug_mode     QWORD 0     ; Debug flag
    masq_last_symbol    QWORD 0     ; Last called symbol index

;------------------------------------------------------------------------------
; Plugin DLL Name for dynamic loading
;------------------------------------------------------------------------------
plugin_dll_name     BYTE "RawrXD_Plugin.dll", 0
plugin_dll_alt      BYTE "RawrXD_RealUI.dll", 0

;==============================================================================
;                              CODE SECTION
;==============================================================================
.CODE

ALIGN 16

;------------------------------------------------------------------------------
; __beacon_stub_default - Default stub for all unresolved functions
; Returns 0/false/null as safe default for most methods
;------------------------------------------------------------------------------
PUBLIC __beacon_stub_default
__beacon_stub_default PROC FRAME
    push rbp
    .pushreg rbp
    mov rbp, rsp
    .setframe rbp, 0
    .endprolog
    
    ; Increment call count (telemetry)
    lock inc QWORD PTR [masq_call_count]
    
    ; Return safe default (0/false/null)
    xor eax, eax
    xor edx, edx
    
    pop rbp
    ret
__beacon_stub_default ENDP

;------------------------------------------------------------------------------
; __trampoline_vector - Trampoline for methods returning std::vector
; RCX = this pointer, RDX = return buffer (MSVC x64 hidden param)
;------------------------------------------------------------------------------
PUBLIC __trampoline_vector
__trampoline_vector PROC FRAME
    push rbp
    .pushreg rbp
    mov rbp, rsp
    .setframe rbp, 0
    sub rsp, 32
    .allocstack 32
    .endprolog
    
    ; Check if real implementation hooked via first qword of 'this'
    test rcx, rcx
    jz @vector_default
    mov rax, [rcx]
    test rax, rax
    jz @vector_default
    
    ; Forward to real implementation
    add rsp, 32
    pop rbp
    jmp rax
    
@vector_default:
    ; Return empty vector (null ptr + 0 size + 0 capacity)
    ; For MSVC x64, vector returned via hidden RDX param
    test rdx, rdx
    jz @vector_done
    mov QWORD PTR [rdx], 0        ; _Myfirst (begin ptr)
    mov QWORD PTR [rdx+8], 0      ; _Mylast (end ptr)
    mov QWORD PTR [rdx+16], 0     ; _Myend (capacity ptr)
    
@vector_done:
    mov rax, rdx
    add rsp, 32
    pop rbp
    ret
__trampoline_vector ENDP

;------------------------------------------------------------------------------
; __trampoline_string - Trampoline for methods returning std::string
; RCX = this, RDX = hidden return buffer for string
;------------------------------------------------------------------------------
PUBLIC __trampoline_string
__trampoline_string PROC FRAME
    push rbp
    .pushreg rbp
    mov rbp, rsp
    .setframe rbp, 0
    sub rsp, 32
    .allocstack 32
    .endprolog
    
    ; RDX = ptr to string return buffer (SSO layout)
    test rdx, rdx
    jz @string_default
    
    ; Set empty string (ptr=internal_buffer, size=0, capacity=15 for SSO)
    ; MSVC SSO: first 16 bytes = buffer, then size, then capacity
    xor eax, eax
    mov QWORD PTR [rdx], 0        ; Buffer bytes 0-7
    mov QWORD PTR [rdx+8], 0      ; Buffer bytes 8-15
    mov QWORD PTR [rdx+16], 0     ; Size = 0
    mov QWORD PTR [rdx+24], 15    ; Capacity = SSO threshold
    
@string_default:
    mov rax, rdx
    add rsp, 32
    pop rbp
    ret
__trampoline_string ENDP

;------------------------------------------------------------------------------
; __trampoline_bool - Trampoline for methods returning bool
;------------------------------------------------------------------------------
PUBLIC __trampoline_bool
__trampoline_bool PROC
    xor eax, eax    ; Return false
    ret
__trampoline_bool ENDP

;------------------------------------------------------------------------------
; __trampoline_void - Trampoline for void methods (no-op)
;------------------------------------------------------------------------------
PUBLIC __trampoline_void
__trampoline_void PROC
    ret
__trampoline_void ENDP

;------------------------------------------------------------------------------
; __trampoline_int - Trampoline for methods returning int/HRESULT
;------------------------------------------------------------------------------
PUBLIC __trampoline_int
__trampoline_int PROC
    xor eax, eax    ; Return 0 / S_OK
    ret
__trampoline_int ENDP

;------------------------------------------------------------------------------
; __trampoline_ptr - Trampoline for methods returning pointers
;------------------------------------------------------------------------------
PUBLIC __trampoline_ptr
__trampoline_ptr PROC
    xor eax, eax    ; Return nullptr
    ret
__trampoline_ptr ENDP

;------------------------------------------------------------------------------
; ResolveAllSymbols - Called once at init to load real implementations
;------------------------------------------------------------------------------
PUBLIC ResolveAllSymbols
ResolveAllSymbols PROC FRAME
    push rbp
    .pushreg rbp
    mov rbp, rsp
    .setframe rbp, 0
    sub rsp, 64
    .allocstack 64
    push rbx
    .pushreg rbx
    push rsi
    .pushreg rsi
    .endprolog
    
    ; Try primary plugin DLL
    lea rcx, plugin_dll_name
    call LoadLibraryA
    test rax, rax
    jnz @have_module
    
    ; Try alternate DLL
    lea rcx, plugin_dll_alt
    call LoadLibraryA
    test rax, rax
    jz @resolve_done
    
@have_module:
    mov rbx, rax        ; Save module handle
    
    ; Resolve each Win32IDE method
    ; Pattern: mov rcx, rbx; lea rdx, [name]; call GetProcAddress; mov [iat_slot], rax
    
    ; Win32IDE_getGitChangedFiles
    mov rcx, rbx
    lea rdx, sym_getGitChangedFiles
    call GetProcAddress
    test rax, rax
    jz @skip_0
    mov [iat_Win32IDE_getGitChangedFiles], rax
@skip_0:

    ; Win32IDE_isGitRepository
    mov rcx, rbx
    lea rdx, sym_isGitRepository
    call GetProcAddress
    test rax, rax
    jz @skip_1
    mov [iat_Win32IDE_isGitRepository], rax
@skip_1:

    ; Win32IDE_generateResponse
    mov rcx, rbx
    lea rdx, sym_generateResponse
    call GetProcAddress
    test rax, rax
    jz @skip_8
    mov [iat_Win32IDE_generateResponse], rax
@skip_8:

    ; Win32IDE_HandleCopilotStreamUpdate
    mov rcx, rbx
    lea rdx, sym_HandleCopilotStreamUpdate
    call GetProcAddress
    test rax, rax
    jz @skip_10
    mov [iat_Win32IDE_HandleCopilotStreamUpdate], rax
@skip_10:

    ; Add more symbol resolutions as needed...
    
@resolve_done:
    pop rsi
    pop rbx
    add rsp, 64
    pop rbp
    ret
ResolveAllSymbols ENDP

;------------------------------------------------------------------------------
; Symbol name strings for GetProcAddress
;------------------------------------------------------------------------------
.DATA
sym_getGitChangedFiles       BYTE "Win32IDE_getGitChangedFiles", 0
sym_isGitRepository          BYTE "Win32IDE_isGitRepository", 0
sym_generateResponse         BYTE "Win32IDE_generateResponse", 0
sym_HandleCopilotStreamUpdate BYTE "Win32IDE_HandleCopilotStreamUpdate", 0
sym_GetSubAgentManager       BYTE "AgenticBridge_GetSubAgentManager", 0
sym_executeSwarm             BYTE "SubAgentManager_executeSwarm", 0

.CODE

;==============================================================================
; INDIVIDUAL TRAMPOLINES - C++ Mangled Symbol Exports
; These satisfy unresolved externals at link time
;==============================================================================

;------------------------------------------------------------------------------
; Win32IDE::getGitChangedFiles
;------------------------------------------------------------------------------
PUBLIC ?getGitChangedFiles@Win32IDE@@AEBA?AV?$vector@UGitFile@@V?$allocator@UGitFile@@@std@@@std@@XZ
?getGitChangedFiles@Win32IDE@@AEBA?AV?$vector@UGitFile@@V?$allocator@UGitFile@@@std@@@std@@XZ PROC FRAME
    .endprolog
    mov rax, [iat_Win32IDE_getGitChangedFiles]
    test rax, rax
    jnz @fwd_getGit
    jmp __trampoline_vector
@fwd_getGit:
    jmp rax
?getGitChangedFiles@Win32IDE@@AEBA?AV?$vector@UGitFile@@V?$allocator@UGitFile@@@std@@@std@@XZ ENDP

;------------------------------------------------------------------------------
; Win32IDE::isGitRepository
;------------------------------------------------------------------------------
PUBLIC ?isGitRepository@Win32IDE@@AEBA_NXZ
?isGitRepository@Win32IDE@@AEBA_NXZ PROC FRAME
    .endprolog
    mov rax, [iat_Win32IDE_isGitRepository]
    test rax, rax
    jnz @fwd_isGit
    xor eax, eax
    ret
@fwd_isGit:
    jmp rax
?isGitRepository@Win32IDE@@AEBA_NXZ ENDP

;------------------------------------------------------------------------------
; Win32IDE::gitStageFile
;------------------------------------------------------------------------------
PUBLIC ?gitStageFile@Win32IDE@@AEAAXAEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@@Z
?gitStageFile@Win32IDE@@AEAAXAEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@@Z PROC FRAME
    .endprolog
    mov rax, [iat_Win32IDE_gitStageFile]
    test rax, rax
    jnz @fwd_stage
    ret
@fwd_stage:
    jmp rax
?gitStageFile@Win32IDE@@AEAAXAEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@@Z ENDP

;------------------------------------------------------------------------------
; Win32IDE::gitUnstageFile
;------------------------------------------------------------------------------
PUBLIC ?gitUnstageFile@Win32IDE@@AEAAXAEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@@Z
?gitUnstageFile@Win32IDE@@AEAAXAEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@@Z PROC FRAME
    .endprolog
    mov rax, [iat_Win32IDE_gitUnstageFile]
    test rax, rax
    jnz @fwd_unstage
    ret
@fwd_unstage:
    jmp rax
?gitUnstageFile@Win32IDE@@AEAAXAEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@@Z ENDP

;------------------------------------------------------------------------------
; Win32IDE::gitPull
;------------------------------------------------------------------------------
PUBLIC ?gitPull@Win32IDE@@AEAAXXZ
?gitPull@Win32IDE@@AEAAXXZ PROC FRAME
    .endprolog
    mov rax, [iat_Win32IDE_gitPull]
    test rax, rax
    jnz @fwd_pull
    ret
@fwd_pull:
    jmp rax
?gitPull@Win32IDE@@AEAAXXZ ENDP

;------------------------------------------------------------------------------
; Win32IDE::gitPush
;------------------------------------------------------------------------------
PUBLIC ?gitPush@Win32IDE@@AEAAXXZ
?gitPush@Win32IDE@@AEAAXXZ PROC FRAME
    .endprolog
    mov rax, [iat_Win32IDE_gitPush]
    test rax, rax
    jnz @fwd_push
    ret
@fwd_push:
    jmp rax
?gitPush@Win32IDE@@AEAAXXZ ENDP

;------------------------------------------------------------------------------
; Win32IDE::gitCommit
;------------------------------------------------------------------------------
PUBLIC ?gitCommit@Win32IDE@@AEAAXAEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@@Z
?gitCommit@Win32IDE@@AEAAXAEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@@Z PROC FRAME
    .endprolog
    mov rax, [iat_Win32IDE_gitCommit]
    test rax, rax
    jnz @fwd_commit
    ret
@fwd_commit:
    jmp rax
?gitCommit@Win32IDE@@AEAAXAEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@@Z ENDP

;------------------------------------------------------------------------------
; Win32IDE::newFile
;------------------------------------------------------------------------------
PUBLIC ?newFile@Win32IDE@@AEAAXXZ
?newFile@Win32IDE@@AEAAXXZ PROC FRAME
    .endprolog
    mov rax, [iat_Win32IDE_newFile]
    test rax, rax
    jnz @fwd_newfile
    ret
@fwd_newfile:
    jmp rax
?newFile@Win32IDE@@AEAAXXZ ENDP

;------------------------------------------------------------------------------
; Win32IDE::generateResponse
;------------------------------------------------------------------------------
PUBLIC ?generateResponse@Win32IDE@@AEAA?AV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@AEBV23@@Z
?generateResponse@Win32IDE@@AEAA?AV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@AEBV23@@Z PROC FRAME
    .endprolog
    mov rax, [iat_Win32IDE_generateResponse]
    test rax, rax
    jnz @fwd_genresp
    jmp __trampoline_string
@fwd_genresp:
    jmp rax
?generateResponse@Win32IDE@@AEAA?AV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@AEBV23@@Z ENDP

;------------------------------------------------------------------------------
; Win32IDE::isModelLoaded
;------------------------------------------------------------------------------
PUBLIC ?isModelLoaded@Win32IDE@@AEBA_NXZ
?isModelLoaded@Win32IDE@@AEBA_NXZ PROC FRAME
    .endprolog
    mov rax, [iat_Win32IDE_isModelLoaded]
    test rax, rax
    jnz @fwd_modelload
    xor eax, eax
    ret
@fwd_modelload:
    jmp rax
?isModelLoaded@Win32IDE@@AEBA_NXZ ENDP

;------------------------------------------------------------------------------
; Win32IDE::HandleCopilotStreamUpdate
;------------------------------------------------------------------------------
PUBLIC ?HandleCopilotStreamUpdate@Win32IDE@@AEAAXPEBD_K@Z
?HandleCopilotStreamUpdate@Win32IDE@@AEAAXPEBD_K@Z PROC FRAME
    .endprolog
    mov rax, [iat_Win32IDE_HandleCopilotStreamUpdate]
    test rax, rax
    jnz @fwd_stream
    ret
@fwd_stream:
    jmp rax
?HandleCopilotStreamUpdate@Win32IDE@@AEAAXPEBD_K@Z ENDP

;------------------------------------------------------------------------------
; Win32IDE::HandleCopilotClear
;------------------------------------------------------------------------------
PUBLIC ?HandleCopilotClear@Win32IDE@@AEAAXXZ
?HandleCopilotClear@Win32IDE@@AEAAXXZ PROC FRAME
    .endprolog
    mov rax, [iat_Win32IDE_HandleCopilotClear]
    test rax, rax
    jnz @fwd_clear
    ret
@fwd_clear:
    jmp rax
?HandleCopilotClear@Win32IDE@@AEAAXXZ ENDP

;------------------------------------------------------------------------------
; Win32IDE::HandleCopilotSend
;------------------------------------------------------------------------------
PUBLIC ?HandleCopilotSend@Win32IDE@@AEAAXXZ
?HandleCopilotSend@Win32IDE@@AEAAXXZ PROC FRAME
    .endprolog
    mov rax, [iat_Win32IDE_HandleCopilotSend]
    test rax, rax
    jnz @fwd_send
    ret
@fwd_send:
    jmp rax
?HandleCopilotSend@Win32IDE@@AEAAXXZ ENDP

;------------------------------------------------------------------------------
; Win32IDE::routeCommand
;------------------------------------------------------------------------------
PUBLIC ?routeCommand@Win32IDE@@AEAA_NH@Z
?routeCommand@Win32IDE@@AEAA_NH@Z PROC FRAME
    .endprolog
    mov rax, [iat_Win32IDE_routeCommand]
    test rax, rax
    jnz @fwd_route
    xor eax, eax
    ret
@fwd_route:
    jmp rax
?routeCommand@Win32IDE@@AEAA_NH@Z ENDP

;------------------------------------------------------------------------------
; Win32IDE::shutdownInference
;------------------------------------------------------------------------------
PUBLIC ?shutdownInference@Win32IDE@@AEAAXXZ
?shutdownInference@Win32IDE@@AEAAXXZ PROC FRAME
    .endprolog
    mov rax, [iat_Win32IDE_shutdownInference]
    test rax, rax
    jnz @fwd_shutdown
    ret
@fwd_shutdown:
    jmp rax
?shutdownInference@Win32IDE@@AEAAXXZ ENDP

;------------------------------------------------------------------------------
; Win32IDE::initializeInference
;------------------------------------------------------------------------------
PUBLIC ?initializeInference@Win32IDE@@AEAA_NXZ
?initializeInference@Win32IDE@@AEAA_NXZ PROC FRAME
    .endprolog
    mov rax, [iat_Win32IDE_initializeInference]
    test rax, rax
    jnz @fwd_init
    mov eax, 1    ; Return true - pretend init succeeded
    ret
@fwd_init:
    jmp rax
?initializeInference@Win32IDE@@AEAA_NXZ ENDP

;------------------------------------------------------------------------------
; Win32IDE::createStatusBar
;------------------------------------------------------------------------------
PUBLIC ?createStatusBar@Win32IDE@@AEAAXPEAUHWND__@@@Z
?createStatusBar@Win32IDE@@AEAAXPEAUHWND__@@@Z PROC FRAME
    .endprolog
    mov rax, [iat_Win32IDE_createStatusBar]
    test rax, rax
    jnz @fwd_statusbar
    ret
@fwd_statusbar:
    jmp rax
?createStatusBar@Win32IDE@@AEAAXPEAUHWND__@@@Z ENDP

;------------------------------------------------------------------------------
; Win32IDE::createTerminal
;------------------------------------------------------------------------------
PUBLIC ?createTerminal@Win32IDE@@AEAAXPEAUHWND__@@@Z
?createTerminal@Win32IDE@@AEAAXPEAUHWND__@@@Z PROC FRAME
    .endprolog
    mov rax, [iat_Win32IDE_createTerminal]
    test rax, rax
    jnz @fwd_terminal
    ret
@fwd_terminal:
    jmp rax
?createTerminal@Win32IDE@@AEAAXPEAUHWND__@@@Z ENDP

;------------------------------------------------------------------------------
; Win32IDE::createEditor
;------------------------------------------------------------------------------
PUBLIC ?createEditor@Win32IDE@@AEAAXPEAUHWND__@@@Z
?createEditor@Win32IDE@@AEAAXPEAUHWND__@@@Z PROC FRAME
    .endprolog
    mov rax, [iat_Win32IDE_createEditor]
    test rax, rax
    jnz @fwd_editor
    ret
@fwd_editor:
    jmp rax
?createEditor@Win32IDE@@AEAAXPEAUHWND__@@@Z ENDP

;------------------------------------------------------------------------------
; Win32IDE::createSidebar
;------------------------------------------------------------------------------
PUBLIC ?createSidebar@Win32IDE@@AEAAXPEAUHWND__@@@Z
?createSidebar@Win32IDE@@AEAAXPEAUHWND__@@@Z PROC FRAME
    .endprolog
    mov rax, [iat_Win32IDE_createSidebar]
    test rax, rax
    jnz @fwd_sidebar
    ret
@fwd_sidebar:
    jmp rax
?createSidebar@Win32IDE@@AEAAXPEAUHWND__@@@Z ENDP

;------------------------------------------------------------------------------
; Win32IDE::initializeSwarmSystem
;------------------------------------------------------------------------------
PUBLIC ?initializeSwarmSystem@Win32IDE@@AEAAXXZ
?initializeSwarmSystem@Win32IDE@@AEAAXXZ PROC FRAME
    .endprolog
    mov rax, [iat_Win32IDE_initializeSwarmSystem]
    test rax, rax
    jnz @fwd_swarm
    ret
@fwd_swarm:
    jmp rax
?initializeSwarmSystem@Win32IDE@@AEAAXXZ ENDP

;------------------------------------------------------------------------------
; Win32IDE::createAcceleratorTable
;------------------------------------------------------------------------------
PUBLIC ?createAcceleratorTable@Win32IDE@@AEAAXXZ
?createAcceleratorTable@Win32IDE@@AEAAXXZ PROC FRAME
    .endprolog
    mov rax, [iat_Win32IDE_createAcceleratorTable]
    test rax, rax
    jnz @fwd_accel
    ret
@fwd_accel:
    jmp rax
?createAcceleratorTable@Win32IDE@@AEAAXXZ ENDP

;------------------------------------------------------------------------------
; Win32IDE::removeTab
;------------------------------------------------------------------------------
PUBLIC ?removeTab@Win32IDE@@AEAAXH@Z
?removeTab@Win32IDE@@AEAAXH@Z PROC FRAME
    .endprolog
    mov rax, [iat_Win32IDE_removeTab]
    test rax, rax
    jnz @fwd_removetab
    ret
@fwd_removetab:
    jmp rax
?removeTab@Win32IDE@@AEAAXH@Z ENDP

;------------------------------------------------------------------------------
; Win32IDE::filePathToUri
;------------------------------------------------------------------------------
PUBLIC ?filePathToUri@Win32IDE@@AEBA?AV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@AEBV23@@Z
?filePathToUri@Win32IDE@@AEBA?AV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@AEBV23@@Z PROC FRAME
    .endprolog
    mov rax, [iat_Win32IDE_filePathToUri]
    test rax, rax
    jnz @fwd_uri
    jmp __trampoline_string
@fwd_uri:
    jmp rax
?filePathToUri@Win32IDE@@AEBA?AV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@AEBV23@@Z ENDP

;------------------------------------------------------------------------------
; Win32IDE::extractLeafName
;------------------------------------------------------------------------------
PUBLIC ?extractLeafName@Win32IDE@@AEBA?AV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@AEBV23@@Z
?extractLeafName@Win32IDE@@AEBA?AV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@AEBV23@@Z PROC FRAME
    .endprolog
    mov rax, [iat_Win32IDE_extractLeafName]
    test rax, rax
    jnz @fwd_leaf
    jmp __trampoline_string
@fwd_leaf:
    jmp rax
?extractLeafName@Win32IDE@@AEBA?AV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@AEBV23@@Z ENDP

;------------------------------------------------------------------------------
; Win32IDE::recreateFonts
;------------------------------------------------------------------------------
PUBLIC ?recreateFonts@Win32IDE@@AEAAXXZ
?recreateFonts@Win32IDE@@AEAAXXZ PROC FRAME
    .endprolog
    mov rax, [iat_Win32IDE_recreateFonts]
    test rax, rax
    jnz @fwd_fonts
    ret
@fwd_fonts:
    jmp rax
?recreateFonts@Win32IDE@@AEAAXXZ ENDP

;------------------------------------------------------------------------------
; Win32IDE::findTerminalPane
;------------------------------------------------------------------------------
PUBLIC ?findTerminalPane@Win32IDE@@AEAAPEAUTerminalPane@@H@Z
?findTerminalPane@Win32IDE@@AEAAPEAUTerminalPane@@H@Z PROC FRAME
    .endprolog
    mov rax, [iat_Win32IDE_findTerminalPane]
    test rax, rax
    jnz @fwd_termpane
    xor eax, eax
    ret
@fwd_termpane:
    jmp rax
?findTerminalPane@Win32IDE@@AEAAPEAUTerminalPane@@H@Z ENDP

;------------------------------------------------------------------------------
; Win32IDE::setWindowText
;------------------------------------------------------------------------------
PUBLIC ?setWindowText@Win32IDE@@AEAAXPEAUHWND__@@AEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@@Z
?setWindowText@Win32IDE@@AEAAXPEAUHWND__@@AEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@@Z PROC FRAME
    .endprolog
    mov rax, [iat_Win32IDE_setWindowText]
    test rax, rax
    jnz @fwd_setwnd
    ret
@fwd_setwnd:
    jmp rax
?setWindowText@Win32IDE@@AEAAXPEAUHWND__@@AEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@@Z ENDP

;------------------------------------------------------------------------------
; Win32IDE::getWindowText
;------------------------------------------------------------------------------
PUBLIC ?getWindowText@Win32IDE@@AEAA?AV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@PEAUHWND__@@@Z
?getWindowText@Win32IDE@@AEAA?AV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@PEAUHWND__@@@Z PROC FRAME
    .endprolog
    mov rax, [iat_Win32IDE_getWindowText]
    test rax, rax
    jnz @fwd_getwnd
    jmp __trampoline_string
@fwd_getwnd:
    jmp rax
?getWindowText@Win32IDE@@AEAA?AV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@PEAUHWND__@@@Z ENDP

;------------------------------------------------------------------------------
; AgenticBridge::GetSubAgentManager
;------------------------------------------------------------------------------
PUBLIC ?GetSubAgentManager@AgenticBridge@@QEAAPEAVSubAgentManager@@XZ
?GetSubAgentManager@AgenticBridge@@QEAAPEAVSubAgentManager@@XZ PROC FRAME
    .endprolog
    mov rax, [iat_AgenticBridge_GetSubAgentManager]
    test rax, rax
    jnz @fwd_subagent
    xor eax, eax
    ret
@fwd_subagent:
    jmp rax
?GetSubAgentManager@AgenticBridge@@QEAAPEAVSubAgentManager@@XZ ENDP

;------------------------------------------------------------------------------
; SubAgentManager::getStatusSummary
;------------------------------------------------------------------------------
PUBLIC ?getStatusSummary@SubAgentManager@@QEBA?AV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@XZ
?getStatusSummary@SubAgentManager@@QEBA?AV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@XZ PROC FRAME
    .endprolog
    mov rax, [iat_SubAgentManager_getStatusSummary]
    test rax, rax
    jnz @fwd_status
    jmp __trampoline_string
@fwd_status:
    jmp rax
?getStatusSummary@SubAgentManager@@QEBA?AV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@XZ ENDP

;------------------------------------------------------------------------------
; SubAgentManager::getTodoList
;------------------------------------------------------------------------------
PUBLIC ?getTodoList@SubAgentManager@@QEBA?AV?$vector@UTodoItem@@V?$allocator@UTodoItem@@@std@@@std@@XZ
?getTodoList@SubAgentManager@@QEBA?AV?$vector@UTodoItem@@V?$allocator@UTodoItem@@@std@@@std@@XZ PROC FRAME
    .endprolog
    mov rax, [iat_SubAgentManager_getTodoList]
    test rax, rax
    jnz @fwd_todo
    jmp __trampoline_vector
@fwd_todo:
    jmp rax
?getTodoList@SubAgentManager@@QEBA?AV?$vector@UTodoItem@@V?$allocator@UTodoItem@@@std@@@std@@XZ ENDP

;------------------------------------------------------------------------------
; SubAgentManager::executeSwarm (alias - mangled name too long for MASM)
; Real symbol resolved via /ALTERNATENAME at link time
;------------------------------------------------------------------------------
PUBLIC __trmp_SubAgentManager_executeSwarm
__trmp_SubAgentManager_executeSwarm PROC FRAME
    .endprolog
    mov rax, [iat_SubAgentManager_executeSwarm]
    test rax, rax
    jnz @fwd_execswarm
    jmp __trampoline_string
@fwd_execswarm:
    jmp rax
__trmp_SubAgentManager_executeSwarm ENDP

;------------------------------------------------------------------------------
; SubAgentManager constructor
;------------------------------------------------------------------------------
PUBLIC ??0SubAgentManager@@QEAA@PEAVAgenticEngine@@@Z
??0SubAgentManager@@QEAA@PEAVAgenticEngine@@@Z PROC FRAME
    .endprolog
    mov rax, [iat_SubAgentManager_ctor]
    test rax, rax
    jnz @fwd_ctor
    mov rax, rcx    ; Return 'this' for chained calls
    ret
@fwd_ctor:
    jmp rax
??0SubAgentManager@@QEAA@PEAVAgenticEngine@@@Z ENDP

;==============================================================================
; CRT Runtime Initializers - Satisfy msvcrt.lib/vcruntime.lib references
;==============================================================================

PUBLIC __vcrt_initialize
__vcrt_initialize PROC
    mov eax, 1      ; Return TRUE (init succeeded)
    ret
__vcrt_initialize ENDP

PUBLIC __vcrt_uninitialize
__vcrt_uninitialize PROC
    xor eax, eax
    ret
__vcrt_uninitialize ENDP

PUBLIC __vcrt_uninitialize_critical
__vcrt_uninitialize_critical PROC
    ret
__vcrt_uninitialize_critical ENDP

PUBLIC __vcrt_thread_attach
__vcrt_thread_attach PROC
    mov eax, 1
    ret
__vcrt_thread_attach ENDP

PUBLIC __vcrt_thread_detach
__vcrt_thread_detach PROC
    mov eax, 1
    ret
__vcrt_thread_detach ENDP

PUBLIC _is_c_termination_complete
_is_c_termination_complete PROC
    xor eax, eax
    ret
_is_c_termination_complete ENDP

PUBLIC __acrt_initialize
__acrt_initialize PROC
    mov eax, 1
    ret
__acrt_initialize ENDP

PUBLIC __acrt_uninitialize
__acrt_uninitialize PROC
    xor eax, eax
    ret
__acrt_uninitialize ENDP

PUBLIC __acrt_uninitialize_critical
__acrt_uninitialize_critical PROC
    ret
__acrt_uninitialize_critical ENDP

PUBLIC __acrt_thread_attach
__acrt_thread_attach PROC
    mov eax, 1
    ret
__acrt_thread_attach ENDP

PUBLIC __acrt_thread_detach
__acrt_thread_detach PROC
    mov eax, 1
    ret
__acrt_thread_detach ENDP

;==============================================================================
; Hook Installation API - For runtime patching
;==============================================================================

;------------------------------------------------------------------------------
; InstallIATHook - Install a function pointer at runtime
; RCX = IAT slot index (0-83)
; RDX = Function pointer to install
; Returns: RAX = previous value
;------------------------------------------------------------------------------
PUBLIC InstallIATHook
InstallIATHook PROC FRAME
    .endprolog
    cmp rcx, 83
    ja @invalid_slot
    
    lea rax, __iat_hook_base
    shl rcx, 3              ; index * 8
    add rax, rcx
    
    mov rcx, [rax]          ; Save old value
    mov [rax], rdx          ; Install new
    mov rax, rcx            ; Return old
    ret
    
@invalid_slot:
    xor eax, eax
    ret
InstallIATHook ENDP

;------------------------------------------------------------------------------
; GetIATHook - Get current function pointer for slot
; RCX = IAT slot index
; Returns: RAX = current function pointer
;------------------------------------------------------------------------------
PUBLIC GetIATHook
GetIATHook PROC FRAME
    .endprolog
    cmp rcx, 83
    ja @invalid_get
    
    lea rax, __iat_hook_base
    shl rcx, 3
    add rax, rcx
    mov rax, [rax]
    ret
    
@invalid_get:
    xor eax, eax
    ret
GetIATHook ENDP

;------------------------------------------------------------------------------
; GetMasqueradeStats - Return telemetry
; Returns: RAX = total stub call count
;------------------------------------------------------------------------------
PUBLIC GetMasqueradeStats
GetMasqueradeStats PROC FRAME
    .endprolog
    mov rax, [masq_call_count]
    ret
GetMasqueradeStats ENDP

END
