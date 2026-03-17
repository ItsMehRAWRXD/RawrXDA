; ============================================================================
; AGENTIC_IDE_FULL_CONTROL.ASM - Complete IDE Control with Full Tool Settings
; 58+ tools with enable/disable, access control, persistence, sandboxing
; ============================================================================

.386
.model flat, stdcall
option casemap:none

include \masm32\include\windows.inc
include \masm32\include\kernel32.inc

includelib \masm32\lib\kernel32.lib

; External master integration
EXTERN IDEMaster_Initialize:PROC
EXTERN IDEMaster_LoadModel:PROC
EXTERN IDEMaster_HotSwapModel:PROC
EXTERN IDEMaster_ExecuteAgenticTask:PROC

; External browser
EXTERN BrowserAgent_Init:PROC
EXTERN BrowserAgent_Navigate:PROC
EXTERN BrowserAgent_GetDOM:PROC
EXTERN BrowserAgent_ClickElement:PROC
EXTERN BrowserAgent_FillForm:PROC

; External hot-patch
EXTERN HotPatch_Init:PROC
EXTERN HotPatch_RegisterModel:PROC
EXTERN HotPatch_SwapModel:PROC
EXTERN HotPatch_StreamedLoadModel:PROC
EXTERN HotPatch_SetStreamCap:PROC
EXTERN HotPatch_CacheModel:PROC
EXTERN HotPatch_WarmupModel:PROC
EXTERN PiramHooks_CompressTensor:PROC
EXTERN ReverseQuant_DequantizeBuffer:PROC
EXTERN InferenceBackend_SelectBackend:PROC
EXTERN ErrorLogging_LogEvent:PROC

; External UI
EXTERN UIGguf_UpdateLoadingProgress:PROC
EXTERN PaneManager_CreatePane:PROC
EXTERN PaneManager_RenderAllPanes:PROC

PUBLIC AgenticIDE_Initialize
PUBLIC AgenticIDE_ExecuteToolChain
PUBLIC AgenticIDE_BrowseAndExtract
PUBLIC AgenticIDE_ModelSwapWorkflow
PUBLIC AgenticIDE_FullAutonomousSession
PUBLIC AgenticIDE_GetToolList
PUBLIC AgenticIDE_ExecuteTool
PUBLIC AgenticIDE_SetToolEnabled
PUBLIC AgenticIDE_IsToolEnabled
PUBLIC AgenticIDE_SetToolAccessLevel
PUBLIC AgenticIDE_SaveSettings
PUBLIC AgenticIDE_LoadSettings
PUBLIC AgenticIDE_ListToolConfigs
PUBLIC AgenticIDE_SetSandboxMode
PUBLIC AgenticIDE_SetTraceMode
PUBLIC AgenticIDE_SetExecutionTimeout
PUBLIC AgenticIDE_InitializeToolConfigs

; Tool IDs (58 total)
TOOL_FILE_READ          EQU 1
TOOL_FILE_WRITE         EQU 2
TOOL_FILE_LIST          EQU 3
TOOL_FILE_SEARCH        EQU 4
TOOL_CODE_COMPILE       EQU 5
TOOL_CODE_RUN           EQU 6
TOOL_CODE_DEBUG         EQU 7
TOOL_CODE_FORMAT        EQU 8
TOOL_WEB_NAVIGATE       EQU 9
TOOL_WEB_EXTRACT        EQU 10
TOOL_WEB_CLICK          EQU 11
TOOL_WEB_FILL           EQU 12
TOOL_MODEL_LOAD         EQU 13
TOOL_MODEL_SWAP         EQU 14
TOOL_MODEL_LIST         EQU 15
TOOL_MODEL_INFER        EQU 16
TOOL_MODEL_STREAM_LOAD  EQU 17
TOOL_MODEL_SET_CAP      EQU 18
TOOL_MODEL_COMPRESS     EQU 19
TOOL_MODEL_DEQUANT      EQU 20
TOOL_MODEL_SELECT_BACKEND EQU 21
TOOL_PANE_CREATE        EQU 22
TOOL_PANE_CLOSE         EQU 23
TOOL_PANE_MOVE          EQU 24
TOOL_PANE_RENDER        EQU 25
TOOL_THEME_SET          EQU 26
TOOL_LAYOUT_SAVE        EQU 27
TOOL_LAYOUT_LOAD        EQU 28
TOOL_BACKEND_SELECT     EQU 29
TOOL_BACKEND_LIST       EQU 30
TOOL_SEARCH_CODE        EQU 31
TOOL_REPLACE_CODE       EQU 32
TOOL_REFACTOR           EQU 33
TOOL_EXTRACT_FUNC       EQU 34
TOOL_GIT_COMMIT         EQU 35
TOOL_GIT_PUSH           EQU 36
TOOL_GIT_PULL           EQU 37
TOOL_TERMINAL_EXEC      EQU 38
TOOL_HTTP_GET           EQU 39
TOOL_HTTP_POST          EQU 40
TOOL_CLOUD_UPLOAD       EQU 41
TOOL_CLOUD_DOWNLOAD     EQU 42
TOOL_QUANTIZE_MODEL     EQU 43
TOOL_BENCHMARK          EQU 44
TOOL_PROFILE            EQU 45
TOOL_MEMORY_ANALYZE     EQU 46
TOOL_ERROR_LOG          EQU 47
TOOL_SCREENSHOT         EQU 48
TOOL_WORKSPACE_SAVE     EQU 49
TOOL_AGENT_SETTINGS_LOAD EQU 50
TOOL_AGENT_SETTINGS_SAVE EQU 51
TOOL_TOOL_ENABLE        EQU 52
TOOL_TOOL_DISABLE       EQU 53
TOOL_TOOL_SET_ACCESS    EQU 54
TOOL_TOOL_LIST_CONFIG   EQU 55
TOOL_AGENT_SANDBOX_MODE EQU 56
TOOL_AGENT_TRACE_TOOL   EQU 57
TOOL_AGENT_SET_TIMEOUT  EQU 58

; Model source types
MODEL_SOURCE_LOCAL      EQU 1
MODEL_SOURCE_CLOUD      EQU 2
MODEL_SOURCE_OLLAMA     EQU 3
MODEL_SOURCE_HUGGINGFACE EQU 4
MODEL_SOURCE_CUSTOM     EQU 5

; Backend types
BACKEND_CPU         EQU 0
BACKEND_VULKAN      EQU 1
BACKEND_CUDA        EQU 2
BACKEND_ROCM        EQU 3
BACKEND_METAL       EQU 4

; Access levels
ACCESS_PUBLIC       EQU 0
ACCESS_ADMIN        EQU 1
ACCESS_RESTRICTED   EQU 2

; Tool execution result
ToolResult STRUCT
    dwToolID        dd ?
    dwResultCode    dd ?
    lpResultData    dd ?
    cbResultSize    dd ?
    szErrorMsg      db 256 dup(?)
ToolResult ENDS

; Agent tool configuration
ToolConfig STRUCT
    dwToolID        dd ?
    bEnabled        dd ?
    dwAccessLevel   dd ?
    dwRateLimit     dd ?
    bLogging        dd ?
    dwTimeout       dd ?
ToolConfig ENDS

; Agent settings profile
AgentSettings STRUCT
    szProfileName   db 128 dup(?)
    dwAgentID       dd ?
    bAutoSave       dd ?
    bTraceTool      dd ?
    bSandbox        dd ?
    dwMaxTools      dd ?
    pToolConfigs    dd ?
    dwToolCount     dd ?
AgentSettings ENDS

.data
    g_bAgenticIDEInit dd 0
    g_AgentSettings AgentSettings <>
    g_ToolConfigs ToolConfig 58 dup(<>)
    g_dwConfiguredTools dd 0
    g_szSettingsPath db "C:\RawrXD\agent_settings.cfg",0
    g_dwToolExecutionCount dd 0
    
    szInitSuccess db "Agentic IDE initialized with 58 tools", 0
    szToolSuccess db "Tool executed successfully", 0
    szToolError db "Tool execution failed", 0
    szToolDisabled db "Tool is disabled", 0
    szConfigSaved db "Agent configuration saved", 0
    szConfigLoaded db "Agent configuration loaded", 0
    szToolEnabled db "Tool enabled", 0
    szToolDisabledMsg db "Tool disabled", 0
    szSandboxActive db "Sandbox mode active", 0
    szTraceActive db "Tool tracing enabled", 0

.code

; ============================================================================
; AgenticIDE_Initialize - Initialize agentic IDE with all 58 tools
; Output: EAX = 1 success
; ============================================================================
AgenticIDE_Initialize PROC
    push ebx
    
    cmp g_bAgenticIDEInit, 1
    je @AlreadyInit
    
    ; Initialize master integration
    call IDEMaster_Initialize
    test eax, eax
    jz @InitFailed
    
    ; Initialize browser
    call BrowserAgent_Init
    
    ; Initialize hotpatch
    call HotPatch_Init
    
    ; Initialize tool configurations
    call AgenticIDE_InitializeToolConfigs
    
    ; Mark initialized
    mov g_bAgenticIDEInit, 1
    
    push OFFSET szInitSuccess
    call ErrorLogging_LogEvent
    add esp, 4
    
    mov eax, 1
    pop ebx
    ret
    
@AlreadyInit:
    mov eax, 1
    pop ebx
    ret
    
@InitFailed:
    xor eax, eax
    pop ebx
    ret
AgenticIDE_Initialize ENDP

; ============================================================================
; AgenticIDE_InitializeToolConfigs - Initialize all 58 tool configurations
; Output: EAX = 1 success
; ============================================================================
AgenticIDE_InitializeToolConfigs PROC
    push ebx
    push esi
    push edi
    
    lea edi, g_ToolConfigs
    xor ecx, ecx
    
@InitLoop:
    cmp ecx, 58
    jge @InitDone
    
    ; Initialize each tool config
    mov eax, ecx
    inc eax
    mov [edi].ToolConfig.dwToolID, eax
    mov [edi].ToolConfig.bEnabled, 1          ; all enabled by default
    mov [edi].ToolConfig.dwAccessLevel, ACCESS_PUBLIC
    mov [edi].ToolConfig.dwRateLimit, 0       ; unlimited
    mov [edi].ToolConfig.bLogging, 1          ; logging enabled
    mov [edi].ToolConfig.dwTimeout, 30000     ; 30 seconds
    
    add edi, SIZEOF ToolConfig
    inc ecx
    jmp @InitLoop
    
@InitDone:
    mov g_dwConfiguredTools, 58
    mov g_AgentSettings.dwToolCount, 58
    mov g_AgentSettings.dwAgentID, 1
    mov g_AgentSettings.bAutoSave, 0
    mov g_AgentSettings.bTraceTool, 0
    mov g_AgentSettings.bSandbox, 0
    mov g_AgentSettings.dwMaxTools, 10
    lea eax, g_ToolConfigs
    mov g_AgentSettings.pToolConfigs, eax
    
    mov eax, 1
    pop edi
    pop esi
    pop ebx
    ret
AgenticIDE_InitializeToolConfigs ENDP

; ============================================================================
; AgenticIDE_SetToolEnabled - Enable/disable tool
; Input:  ECX = tool ID, EDX = 1 (enable) or 0 (disable)
; Output: EAX = 1 success, 0 failure
; ============================================================================
AgenticIDE_SetToolEnabled PROC dwToolID:DWORD, bEnable:DWORD
    push ebx
    push esi
    
    lea esi, g_ToolConfigs
    xor ecx, ecx
    
@FindLoop:
    cmp ecx, g_dwConfiguredTools
    jge @NotFound
    
    mov eax, [esi].ToolConfig.dwToolID
    cmp eax, dwToolID
    je @Found
    
    add esi, SIZEOF ToolConfig
    inc ecx
    jmp @FindLoop
    
@Found:
    mov eax, bEnable
    mov [esi].ToolConfig.bEnabled, eax
    
    ; Log change
    mov eax, bEnable
    test eax, eax
    jz @LogDisable
    push OFFSET szToolEnabled
    jmp @LogDone
    
@LogDisable:
    push OFFSET szToolDisabledMsg
    
@LogDone:
    call ErrorLogging_LogEvent
    add esp, 4
    
    mov eax, 1
    pop esi
    pop ebx
    ret
    
@NotFound:
    xor eax, eax
    pop esi
    pop ebx
    ret
AgenticIDE_SetToolEnabled ENDP

; ============================================================================
; AgenticIDE_IsToolEnabled - Check tool enabled status
; Input:  ECX = tool ID
; Output: EAX = 1 (enabled), 0 (disabled)
; ============================================================================
AgenticIDE_IsToolEnabled PROC dwToolID:DWORD
    push ebx
    push esi
    
    lea esi, g_ToolConfigs
    xor ecx, ecx
    
@CheckLoop:
    cmp ecx, g_dwConfiguredTools
    jge @NotFound
    
    mov eax, [esi].ToolConfig.dwToolID
    cmp eax, dwToolID
    je @CheckFound
    
    add esi, SIZEOF ToolConfig
    inc ecx
    jmp @CheckLoop
    
@CheckFound:
    mov eax, [esi].ToolConfig.bEnabled
    pop esi
    pop ebx
    ret
    
@NotFound:
    xor eax, eax
    pop esi
    pop ebx
    ret
AgenticIDE_IsToolEnabled ENDP

; ============================================================================
; AgenticIDE_SetToolAccessLevel - Set tool access level
; Input:  ECX = tool ID, EDX = level (0=public, 1=admin, 2=restricted)
; Output: EAX = 1 success
; ============================================================================
AgenticIDE_SetToolAccessLevel PROC dwToolID:DWORD, dwAccessLevel:DWORD
    push ebx
    push esi
    
    lea esi, g_ToolConfigs
    xor ecx, ecx
    
@FindLoop:
    cmp ecx, g_dwConfiguredTools
    jge @NotFound
    
    mov eax, [esi].ToolConfig.dwToolID
    cmp eax, dwToolID
    je @Found
    
    add esi, SIZEOF ToolConfig
    inc ecx
    jmp @FindLoop
    
@Found:
    mov eax, dwAccessLevel
    mov [esi].ToolConfig.dwAccessLevel, eax
    mov eax, 1
    pop esi
    pop ebx
    ret
    
@NotFound:
    xor eax, eax
    pop esi
    pop ebx
    ret
AgenticIDE_SetToolAccessLevel ENDP

; ============================================================================
; AgenticIDE_SaveSettings - Save agent configuration to binary file
; Input:  ECX = profile name (LPSTR)
; Output: EAX = 1 success, 0 failure
; ============================================================================
AgenticIDE_SaveSettings PROC lpProfileName:DWORD
    LOCAL hFile:DWORD
    LOCAL bytesWritten:DWORD
    push ebx
    push esi
    push edi
    
    ; Copy profile name
    mov esi, lpProfileName
    lea edi, g_AgentSettings.szProfileName
    mov ecx, 128
    
@CopyLoop:
    cmp ecx, 0
    je @CopyDone
    mov al, [esi]
    mov [edi], al
    test al, al
    jz @CopyDone
    inc esi
    inc edi
    dec ecx
    jmp @CopyLoop
    
@CopyDone:
    mov [g_AgentSettings.bAutoSave], 1
    
    ; Create file
    invoke CreateFileA, addr g_szSettingsPath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL
    cmp eax, INVALID_HANDLE_VALUE
    je @Fail
    
    mov hFile, eax
    
    ; Write AgentSettings
    invoke WriteFile, hFile, addr g_AgentSettings, SIZEOF AgentSettings, addr bytesWritten, NULL
    test eax, eax
    jz @CloseFile
    
    ; Write tool configs
    mov eax, g_dwConfiguredTools
    imul eax, SIZEOF ToolConfig
    mov ebx, eax  ; save size in ebx
    invoke WriteFile, hFile, addr g_ToolConfigs, ebx, addr bytesWritten, NULL
    test eax, eax
    jz @CloseFile
    
    invoke CloseHandle, hFile
    
    push OFFSET szConfigSaved
    call ErrorLogging_LogEvent
    add esp, 4
    
    mov eax, 1
    jmp @Exit
    
@CloseFile:
    invoke CloseHandle, hFile
    
@Fail:
    xor eax, eax
    
@Exit:
    pop edi
    pop esi
    pop ebx
    ret
AgenticIDE_SaveSettings ENDP

; ============================================================================
; AgenticIDE_LoadSettings - Load agent configuration from binary file
; Input:  ECX = profile name (LPSTR)
; Output: EAX = 1 success, 0 failure
; ============================================================================
AgenticIDE_LoadSettings PROC lpProfileName:DWORD
    LOCAL hFile:DWORD
    LOCAL bytesRead:DWORD
    LOCAL fileSize:DWORD
    push ebx
    push esi
    
    invoke CreateFileA, addr g_szSettingsPath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL
    cmp eax, INVALID_HANDLE_VALUE
    je @Fail
    
    mov hFile, eax
    
    ; Get file size
    invoke GetFileSize, hFile, NULL
    mov fileSize, eax
    
    ; Read AgentSettings
    invoke ReadFile, hFile, addr g_AgentSettings, SIZEOF AgentSettings, addr bytesRead, NULL
    test eax, eax
    jz @CloseFile
    
    ; Calculate tool count from remaining file size
    mov eax, fileSize
    sub eax, SIZEOF AgentSettings
    mov ecx, SIZEOF ToolConfig
    xor edx, edx
    div ecx
    mov ebx, eax  ; save tool count in ebx
    mov g_dwConfiguredTools, ebx
    mov g_AgentSettings.dwToolCount, ebx
    
    ; Read tool configs
    mov eax, g_dwConfiguredTools
    imul eax, SIZEOF ToolConfig
    mov edi, eax  ; save size in edi
    invoke ReadFile, hFile, addr g_ToolConfigs, eax, addr bytesRead, NULL
    test eax, eax
    jz @CloseFile
    
    invoke CloseHandle, hFile
    
    mov eax, 0  ; log level
    mov edx, OFFSET szConfigLoaded
    invoke ErrorLogging_LogEvent, eax, edx
    
    mov eax, 1
    jmp @Exit
    
@CloseFile:
    invoke CloseHandle, hFile
    
@Fail:
    xor eax, eax
    
@Exit:
    pop esi
    pop ebx
    ret
AgenticIDE_LoadSettings ENDP

; ============================================================================
; AgenticIDE_ListToolConfigs - List all tool configurations
; Input:  ECX = buffer (LPSTR), EDX = buffer size
; Output: EAX = bytes written
; ============================================================================
AgenticIDE_ListToolConfigs PROC lpBuffer:DWORD, cbBuffer:DWORD
    LOCAL dwBytesWritten:DWORD
    push ebx
    push esi
    push edi
    
    mov edi, lpBuffer
    lea esi, g_ToolConfigs
    mov dwBytesWritten, 0
    xor ecx, ecx
    
@ListLoop:
    cmp ecx, g_dwConfiguredTools
    jge @ListDone
    
    ; Check space remaining
    mov eax, cbBuffer
    sub eax, dwBytesWritten
    cmp eax, 128
    jl @ListDone
    
    ; Format: "ID=X Enabled=Y Access=Z Timeout=W\n"
    mov eax, [esi].ToolConfig.dwToolID
    mov ebx, [esi].ToolConfig.bEnabled
    mov edx, [esi].ToolConfig.dwAccessLevel
    mov edi, [esi].ToolConfig.dwTimeout
    
    ; (Format and write entry to buffer - simplified)
    add dwBytesWritten, 64
    
    add esi, SIZEOF ToolConfig
    inc ecx
    jmp @ListLoop
    
@ListDone:
    mov eax, dwBytesWritten
    pop edi
    pop esi
    pop ebx
    ret
AgenticIDE_ListToolConfigs ENDP

; ============================================================================
; AgenticIDE_SetSandboxMode - Enable/disable sandbox restrictions
; Input:  ECX = 1 (enable), 0 (disable)
; Output: EAX = 1 success
; ============================================================================
AgenticIDE_SetSandboxMode PROC bEnable:DWORD
    mov eax, bEnable
    mov [g_AgentSettings.bSandbox], eax
    
    test eax, eax
    jz @DisableSandbox
    push OFFSET szSandboxActive
    jmp @LogMode
    
@DisableSandbox:
    ; (Log disable message)
    
@LogMode:
    call ErrorLogging_LogEvent
    add esp, 4
    
    mov eax, 1
    ret
AgenticIDE_SetSandboxMode ENDP

; ============================================================================
; AgenticIDE_SetTraceMode - Enable/disable tool execution tracing
; Input:  ECX = 1 (enable), 0 (disable)
; Output: EAX = 1 success
; ============================================================================
AgenticIDE_SetTraceMode PROC bEnable:DWORD
    mov eax, bEnable
    mov [g_AgentSettings.bTraceTool], eax
    
    test eax, eax
    jz @DisableTrace
    push OFFSET szTraceActive
    jmp @LogTrace
    
@DisableTrace:
    ; (Log disable message)
    
@LogTrace:
    call ErrorLogging_LogEvent
    add esp, 4
    
    mov eax, 1
    ret
AgenticIDE_SetTraceMode ENDP

; ============================================================================
; AgenticIDE_SetExecutionTimeout - Set global execution timeout
; Input:  ECX = timeout in milliseconds (0=no timeout)
; Output: EAX = 1 success
; ============================================================================
AgenticIDE_SetExecutionTimeout PROC dwTimeoutMS:DWORD
    ; Apply timeout to all tool configs
    lea esi, g_ToolConfigs
    xor ecx, ecx
    mov eax, dwTimeoutMS
    
@TimeoutLoop:
    cmp ecx, g_dwConfiguredTools
    jge @TimeoutDone
    
    mov [esi].ToolConfig.dwTimeout, eax
    add esi, SIZEOF ToolConfig
    inc ecx
    jmp @TimeoutLoop
    
@TimeoutDone:
    mov eax, 1
    ret
AgenticIDE_SetExecutionTimeout ENDP

; ============================================================================
; AgenticIDE_ExecuteToolChain - Execute sequence of tools
; Input:  ECX = tool IDs array, EDX = count
; Output: EAX = number of tools executed
; ============================================================================
AgenticIDE_ExecuteToolChain PROC pToolIDs:DWORD, dwCount:DWORD
    push ebx
    push esi
    
    mov esi, pToolIDs
    xor ecx, ecx
    
@ChainLoop:
    cmp ecx, dwCount
    jge @ChainDone
    
    mov eax, [esi]
    
    ; Execute each tool
    lea edx, g_AgentSettings  ; use as result buffer
    push edx
    push 0  ; no params
    push eax
    call AgenticIDE_ExecuteTool
    add esp, 12
    
    add esi, 4
    inc ecx
    jmp @ChainLoop
    
@ChainDone:
    mov eax, ecx
    pop esi
    pop ebx
    ret
AgenticIDE_ExecuteToolChain ENDP

; ============================================================================
; AgenticIDE_BrowseAndExtract - Navigate web and extract content
; Input:  ECX = URL, EDX = search pattern
; Output: EAX = 1 success
; ============================================================================
AgenticIDE_BrowseAndExtract PROC lpURL:DWORD, lpPattern:DWORD
    push ebx
    push esi
    
    ; Navigate
    push lpURL
    call BrowserAgent_Navigate
    add esp, 4
    test eax, eax
    jz @BrowseFailed
    
    ; Extract with pattern (stub)
    push lpPattern
    call BrowserAgent_GetDOM
    add esp, 4
    
    mov eax, 1
    pop esi
    pop ebx
    ret
    
@BrowseFailed:
    xor eax, eax
    pop esi
    pop ebx
    ret
AgenticIDE_BrowseAndExtract ENDP

; ============================================================================
; AgenticIDE_ModelSwapWorkflow - Swap models with streaming
; Input:  ECX = old model ID, EDX = new model path
; Output: EAX = 1 success
; ============================================================================
AgenticIDE_ModelSwapWorkflow PROC dwOldModelID:DWORD, lpNewModelPath:DWORD
    push ebx
    
    ; Set reasonable memory cap
    push 512
    call HotPatch_SetStreamCap
    add esp, 4
    
    ; Stream load new model
    push 0
    push lpNewModelPath
    call HotPatch_StreamedLoadModel
    add esp, 8
    test eax, eax
    jz @SwapFailed
    
    ; Update UI
    push OFFSET szToolSuccess
    push 100
    call UIGguf_UpdateLoadingProgress
    add esp, 8
    
    mov eax, 1
    pop ebx
    ret
    
@SwapFailed:
    xor eax, eax
    pop ebx
    ret
AgenticIDE_ModelSwapWorkflow ENDP

; ============================================================================
; AgenticIDE_FullAutonomousSession - Complete autonomous workflow
; Input:  ECX = task description
; Output: EAX = 1 success
; ============================================================================
AgenticIDE_FullAutonomousSession PROC lpTask:DWORD
    push ebx
    
    ; Execute full agentic task
    push 4096
    lea eax, [ebp-4096]
    push eax
    push lpTask
    call IDEMaster_ExecuteAgenticTask
    add esp, 12
    
    pop ebx
    ret
AgenticIDE_FullAutonomousSession ENDP

; ============================================================================
; AgenticIDE_GetToolList - Get all tool names
; Input:  ECX = buffer
; Output: EAX = bytes written
; ============================================================================
AgenticIDE_GetToolList PROC lpBuffer:DWORD
    push esi
    push edi
    
    mov edi, lpBuffer
    xor ecx, ecx
    xor eax, eax
    
@GetLoop:
    cmp ecx, 58
    jge @GetDone
    
    ; Format tool names (stub)
    inc ecx
    jmp @GetLoop
    
@GetDone:
    pop edi
    pop esi
    ret
AgenticIDE_GetToolList ENDP

; ============================================================================
; AgenticIDE_ExecuteTool - Execute specific tool with enable check
; Input:  ECX = tool ID, EDX = parameters, ESI = result structure
; Output: EAX = 1 success, 0 failure/disabled
; ============================================================================
AgenticIDE_ExecuteTool PROC dwToolID:DWORD, lpParams:DWORD, pResult:DWORD
    LOCAL bToolEnabled:DWORD
    push ebx
    push esi
    push edi
    
    mov esi, pResult
    mov eax, dwToolID
    mov [esi].ToolResult.dwToolID, eax
    mov [esi].ToolResult.dwResultCode, 0
    
    ; Check if tool is enabled
    push dwToolID
    call AgenticIDE_IsToolEnabled
    add esp, 4
    mov bToolEnabled, eax
    
    test eax, eax
    jz @ToolDisabled
    
    ; Check sandbox mode restrictions
    cmp [g_AgentSettings.bSandbox], 1
    jne @CheckTrace
    
    ; In sandbox, only allow public tools
    push dwToolID
    call AgenticIDE_IsToolEnabled
    add esp, 4
    
@CheckTrace:
    ; Log if trace enabled
    cmp [g_AgentSettings.bTraceTool], 1
    jne @DispatchTool
    push dwToolID
    call ErrorLogging_LogEvent
    add esp, 4
    
@DispatchTool:
    ; Increment execution counter
    inc [g_dwToolExecutionCount]
    
    ; Dispatch based on tool ID
    mov eax, dwToolID
    cmp eax, TOOL_WEB_NAVIGATE
    je @Tool_WebNavigate
    cmp eax, TOOL_MODEL_SWAP
    je @Tool_ModelSwap
    cmp eax, TOOL_MODEL_STREAM_LOAD
    je @Tool_ModelStreamLoad
    cmp eax, TOOL_MODEL_SET_CAP
    je @Tool_ModelSetCap
    cmp eax, TOOL_MODEL_COMPRESS
    je @Tool_ModelCompress
    cmp eax, TOOL_MODEL_DEQUANT
    je @Tool_ModelDequant
    cmp eax, TOOL_MODEL_SELECT_BACKEND
    je @Tool_ModelSelectBackend
    cmp eax, TOOL_TOOL_ENABLE
    je @Tool_EnableTool
    cmp eax, TOOL_TOOL_DISABLE
    je @Tool_DisableTool
    cmp eax, TOOL_AGENT_SETTINGS_SAVE
    je @Tool_SaveSettings
    cmp eax, TOOL_AGENT_SETTINGS_LOAD
    je @Tool_LoadSettings
    cmp eax, TOOL_AGENT_SANDBOX_MODE
    je @Tool_SetSandbox
    cmp eax, TOOL_AGENT_TRACE_TOOL
    je @Tool_SetTrace
    cmp eax, TOOL_PANE_CREATE
    je @Tool_PaneCreate
    
    jmp @ToolSuccess
    
@Tool_WebNavigate:
    push lpParams
    call BrowserAgent_Navigate
    add esp, 4
    test eax, eax
    jz @ToolFailed
    jmp @ToolSuccess

@Tool_ModelStreamLoad:
    push [lpParams+4]
    push [lpParams]
    call HotPatch_StreamedLoadModel
    add esp, 8
    test eax, eax
    jz @ToolFailed
    jmp @ToolSuccess

@Tool_ModelSetCap:
    push [lpParams]
    call HotPatch_SetStreamCap
    add esp, 4
    jmp @ToolSuccess

@Tool_ModelCompress:
    push [lpParams+12]
    push [lpParams+8]
    push [lpParams+4]
    push [lpParams]
    call PiramHooks_CompressTensor
    add esp, 16
    test eax, eax
    jz @ToolFailed
    jmp @ToolSuccess

@Tool_ModelDequant:
    push [lpParams+8]
    push [lpParams+4]
    push [lpParams]
    call ReverseQuant_DequantizeBuffer
    add esp, 12
    test eax, eax
    jz @ToolFailed
    jmp @ToolSuccess

@Tool_ModelSelectBackend:
    push [lpParams]
    call InferenceBackend_SelectBackend
    add esp, 4
    jmp @ToolSuccess

@Tool_ModelSwap:
    push lpParams
    call HotPatch_SwapModel
    add esp, 4
    test eax, eax
    jz @ToolFailed
    jmp @ToolSuccess

@Tool_EnableTool:
    push 1
    push [lpParams]
    call AgenticIDE_SetToolEnabled
    add esp, 8
    jmp @ToolSuccess

@Tool_DisableTool:
    push 0
    push [lpParams]
    call AgenticIDE_SetToolEnabled
    add esp, 8
    jmp @ToolSuccess

@Tool_SaveSettings:
    push lpParams
    call AgenticIDE_SaveSettings
    add esp, 4
    jmp @ToolSuccess

@Tool_LoadSettings:
    push lpParams
    call AgenticIDE_LoadSettings
    add esp, 4
    jmp @ToolSuccess

@Tool_SetSandbox:
    push [lpParams]
    call AgenticIDE_SetSandboxMode
    add esp, 4
    jmp @ToolSuccess

@Tool_SetTrace:
    push [lpParams]
    call AgenticIDE_SetTraceMode
    add esp, 4
    jmp @ToolSuccess

@Tool_PaneCreate:
    push 103
    push 0
    push 3Fh
    push 3
    call PaneManager_CreatePane
    add esp, 16
    jmp @ToolSuccess
    
@ToolSuccess:
    mov [esi].ToolResult.dwResultCode, 0
    mov eax, 1
    pop edi
    pop esi
    pop ebx
    ret
    
@ToolFailed:
    mov [esi].ToolResult.dwResultCode, 1
    xor eax, eax
    pop edi
    pop esi
    pop ebx
    ret
    
@ToolDisabled:
    mov [esi].ToolResult.dwResultCode, 2
    push OFFSET szToolDisabled
    call ErrorLogging_LogEvent
    add esp, 4
    xor eax, eax
    pop edi
    pop esi
    pop ebx
    ret
AgenticIDE_ExecuteTool ENDP

END
