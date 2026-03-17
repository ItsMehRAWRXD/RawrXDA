; ============================================================================
; IDE_MASTER_INTEGRATION.ASM - Complete RawrXD IDE Integration Hub
; Wires: UI + Qt Panes + Agentic System + GGUF Loaders + Backends + Hot-Patching
; Full autonomous control with 44 tools + model hot-swap capabilities
; ============================================================================

.386
.model flat, stdcall
option casemap:none

include \masm32\include\windows.inc
include \masm32\include\kernel32.inc
include \masm32\include\user32.inc

includelib \masm32\lib\kernel32.lib
includelib \masm32\lib\user32.lib

; ============================================================================
; EXTERNAL SYSTEMS - UI & Qt Panes
; ============================================================================
EXTERN UIGguf_CreateMenuBar:PROC
EXTERN UIGguf_CreateToolbar:PROC
EXTERN UIGguf_CreateStatusPane:PROC
EXTERN UIGguf_CreateBreadcrumb:PROC
EXTERN UIGguf_EnableQtSkin:PROC
EXTERN UIGguf_UpdateLoadingProgress:PROC

EXTERN IDEPaneSystem_Initialize:PROC
EXTERN IDEPaneSystem_CreateDefaultLayout:PROC
EXTERN PaneManager_CreatePane:PROC
EXTERN PaneManager_RenderAllPanes:PROC
EXTERN ThemeManager_ApplyDarkTheme:PROC
EXTERN LayoutManager_SaveLayout:PROC
EXTERN LayoutManager_LoadLayout:PROC

; ============================================================================
; EXTERNAL SYSTEMS - GGUF Loading (All 4 Methods)
; ============================================================================
EXTERN GgufUnified_Init:PROC
EXTERN GgufUnified_LoadModelAutomatic:PROC
EXTERN GgufUnified_LoadStandard:PROC
EXTERN GgufUnified_LoadStreaming:PROC
EXTERN GgufUnified_LoadChunked:PROC
EXTERN GgufUnified_LoadMMAP:PROC
EXTERN GgufUnified_UnloadModel:PROC

EXTERN GgufChain_Init:PROC
EXTERN GgufChain_LoadWithDialog:PROC
EXTERN GgufChain_SetBackend:PROC
EXTERN GgufChain_SetLoadingMethod:PROC

EXTERN GGUFChainQt_Init:PROC
EXTERN GGUFChainQt_CreateLoaderPane:PROC

; ============================================================================
; EXTERNAL SYSTEMS - Inference Backends
; ============================================================================
EXTERN InferenceBackend_Init:PROC
EXTERN InferenceBackend_DetectAvailable:PROC
EXTERN InferenceBackend_SelectBackend:PROC
EXTERN InferenceBackend_CreateInferenceContext:PROC
EXTERN InferenceBackend_ExecuteLayer:PROC

; ============================================================================
; EXTERNAL SYSTEMS - Autonomous Agents (44 Tools)
; ============================================================================
EXTERN AgentSystem_Init:PROC
EXTERN AgentSystem_Execute:PROC
EXTERN AgentSystem_Cancel:PROC
EXTERN ToolDispatcher_Init:PROC
EXTERN ToolDispatcher_Execute:PROC

; ============================================================================
; EXTERNAL SYSTEMS - Cloud & HTTP
; ============================================================================
EXTERN HttpClient_Init:PROC
EXTERN HttpClient_Get:PROC
EXTERN HttpClient_Post:PROC
EXTERN OllamaClient_Generate:PROC
EXTERN OllamaClient_ListModels:PROC

; ============================================================================
; PUBLIC API
; ============================================================================
PUBLIC IDEMaster_Initialize_Impl
PUBLIC IDEMaster_InitializeWithConfig_Impl
PUBLIC IDEMaster_LoadModel_Impl
PUBLIC IDEMaster_HotSwapModel_Impl
PUBLIC IDEMaster_ExecuteAgenticTask_Impl
PUBLIC IDEMaster_GetSystemStatus_Impl
PUBLIC IDEMaster_EnableAutonomousBrowsing_Impl
PUBLIC IDEMaster_SaveWorkspace_Impl
PUBLIC IDEMaster_LoadWorkspace_Impl

; ============================================================================
; CONFIGURATION STRUCTURE
; ============================================================================
IDEConfig STRUCT
    ; UI Settings
    bEnableQtSkin       dd ?
    bEnableDarkTheme    dd ?
    bShowToolbar        dd ?
    bShowStatusBar      dd ?
    
    ; GGUF Loader Settings
    dwDefaultLoadMethod dd ?    ; 0=Auto, 1=Standard, 2=Streaming, 3=Chunked, 4=MMAP
    dwDefaultBackend    dd ?    ; 0=CPU, 1=Vulkan, 2=CUDA, 3=ROCm
    dwCompressionLevel  dd ?    ; 0=None, 1=Fast, 2=Balanced, 3=Max
    bAutoQuantize       dd ?
    
    ; Agent Settings
    bEnableAgenticMode  dd ?
    dwMaxAgentThreads   dd ?
    dwToolsetEnabled    dd ?    ; Bitfield for 44 tools
    
    ; Model Hot-Patching
    bEnableHotPatch     dd ?
    dwModelCacheSize    dd ?
    lpActiveModelPath   dd ?
    lpBackupModelPath   dd ?
    
    ; Browser Integration
    bEnableBrowser      dd ?
    dwBrowserEngine     dd ?    ; 0=Edge/Chromium, 1=Custom
    
    dwReserved          dd 16 dup(?)
IDEConfig ENDS

; ============================================================================
; GLOBAL STATE
; ============================================================================
.data
    g_IDEConfig IDEConfig <1, 1, 1, 1, 0, 0, 2, 1, 1, 8, 0FFFFFFFFh, 1, 512, 0, 0, 1, 0>
    g_hMainWindow dd 0
    g_hActiveModel dd 0
    g_hBackupModel dd 0
    g_bInitialized dd 0
    g_dwAgentTaskCount dd 0
    g_hInferenceContext dd 0
    
    szDefaultModelPath db "models\default.gguf", 0
    szStatusReady db "IDE Ready - All Systems Online", 0
    szStatusLoading db "Loading Model...", 0
    szStatusAgentActive db "Autonomous Agent Active", 0
    szStatusHotPatch db "Hot-Patching Model...", 0

.code

; ============================================================================
; IDEMaster_Initialize - Full system initialization with defaults
; Output: EAX = 1 success, 0 failure
; ============================================================================
IDEMaster_Initialize_Impl PROC
    push ebx
    push esi
    push edi
    
    cmp g_bInitialized, 1
    je @AlreadyInit
    
    ; Step 1: Initialize Qt Pane System
    call IDEPaneSystem_Initialize
    test eax, eax
    jz @InitFailed
    
    ; Step 2: Apply theme
    cmp g_IDEConfig.bEnableDarkTheme, 0
    je @SkipTheme
    call ThemeManager_ApplyDarkTheme
@SkipTheme:
    
    ; Step 3: Create default panes
    call IDEPaneSystem_CreateDefaultLayout
    
    ; Step 4: Initialize GGUF loaders
    call GgufUnified_Init
    call GgufChain_Init
    call GGUFChainQt_Init
    
    ; Step 5: Initialize inference backends
    call InferenceBackend_Init
    call InferenceBackend_DetectAvailable
    
    ; Step 6: Initialize autonomous agent system
    call AgentSystem_Init
    call ToolDispatcher_Init
    
    ; Step 7: Initialize HTTP/Cloud clients
    call HttpClient_Init
    
    ; Step 8: Enable Qt skin for GGUF if configured
    cmp g_IDEConfig.bEnableQtSkin, 0
    je @SkipQtSkin
    call UIGguf_EnableQtSkin
@SkipQtSkin:
    
    mov g_bInitialized, 1
    mov eax, 1
    
@AlreadyInit:
    pop edi
    pop esi
    pop ebx
    ret
    
@InitFailed:
    xor eax, eax
    pop edi
    pop esi
    pop ebx
    ret
IDEMaster_Initialize_Impl ENDP

; ============================================================================
; IDEMaster_InitializeWithConfig - Initialize with custom config
; Input:  ECX = pointer to IDEConfig structure
; Output: EAX = 1 success, 0 failure
; ============================================================================
IDEMaster_InitializeWithConfig_Impl PROC
 pConfig:DWORD
    push ebx
    push esi
    push edi
    
    ; Copy config
    mov esi, pConfig
    lea edi, g_IDEConfig
    mov ecx, SIZEOF IDEConfig
    rep movsb
    
    ; Initialize with custom config
    call IDEMaster_Initialize
    
    pop edi
    pop esi
    pop ebx
    ret
IDEMaster_InitializeWithConfig ENDP

; ============================================================================
; IDEMaster_LoadModel - Load GGUF model with auto-detection
; Input:  ECX = model path
;         EDX = load method (0=auto, 1-4=specific)
; Output: EAX = model handle, 0 on failure
; ============================================================================
IDEMaster_LoadModel_Impl PROC
 lpModelPath:DWORD, dwMethod:DWORD
    LOCAL hModel:DWORD
    push ebx
    push esi
    
    ; Update status
    push OFFSET szStatusLoading
    push 25
    call UIGguf_UpdateLoadingProgress
    add esp, 8
    
    ; Select loading method
    mov eax, dwMethod
    test eax, eax
    jz @UseAutomatic
    
    ; Manual method selection  
    push lpModelPath
    call GgufUnified_LoadModelAutomatic
    add esp, 4
    jmp @LoadDone
    
@UseAutomatic:
    ; Automatic method (file size based)
    push lpModelPath
    call GgufUnified_LoadModelAutomatic
    add esp, 4
    
@LoadDone:
    mov hModel, eax
    test eax, eax
    jz @LoadFailed
    
    ; Store as active model
    mov g_hActiveModel, eax
    mov eax, lpModelPath
    mov [g_IDEConfig.lpActiveModelPath], eax
    
    ; Create inference context
    push g_IDEConfig.dwDefaultBackend
    call InferenceBackend_CreateInferenceContext
    add esp, 4
    mov g_hInferenceContext, eax
    
    ; Update progress to complete
    push OFFSET szStatusReady
    push 100
    call UIGguf_UpdateLoadingProgress
    add esp, 8
    
    mov eax, hModel
    pop esi
    pop ebx
    ret
    
@LoadFailed:
    xor eax, eax
    pop esi
    pop ebx
    ret
IDEMaster_LoadModel ENDP

; ============================================================================
; IDEMaster_HotSwapModel - Hot-patch model without IDE restart
; Input:  ECX = new model path
; Output: EAX = 1 success, 0 failure
; ============================================================================
IDEMaster_HotSwapModel_Impl PROC
 lpNewModelPath:DWORD
    LOCAL hNewModel:DWORD
    push ebx
    push esi
    
    cmp g_IDEConfig.bEnableHotPatch, 0
    je @HotPatchDisabled
    
    ; Update status
    push OFFSET szStatusHotPatch
    push 50
    call UIGguf_UpdateLoadingProgress
    add esp, 8
    
    ; Backup current model
    mov eax, g_hActiveModel
    mov g_hBackupModel, eax
    
    ; Load new model (automatic method)
    push lpNewModelPath
    call GgufUnified_LoadModelAutomatic
    add esp, 4
    mov hNewModel, eax
    test eax, eax
    jz @SwapFailed
    
    ; Recreate inference context for new model
    push g_IDEConfig.dwDefaultBackend
    call InferenceBackend_CreateInferenceContext
    add esp, 4
    mov g_hInferenceContext, eax
    
    ; Swap active model
    mov eax, hNewModel
    mov g_hActiveModel, eax
    mov eax, lpNewModelPath
    mov [g_IDEConfig.lpActiveModelPath], eax
    
    ; Unload backup model after delay (async)
    ; (In production, you'd spawn a thread to cleanup old model)
    cmp g_hBackupModel, 0
    je @NoBackup
    push g_hBackupModel
    call GgufUnified_UnloadModel
    add esp, 4
    mov g_hBackupModel, 0
    
@NoBackup:
    ; Update status
    push OFFSET szStatusReady
    push 100
    call UIGguf_UpdateLoadingProgress
    add esp, 8
    
    mov eax, 1
    pop esi
    pop ebx
    ret
    
@SwapFailed:
    ; Restore backup
    mov eax, g_hBackupModel
    mov g_hActiveModel, eax
    xor eax, eax
    pop esi
    pop ebx
    ret
    
@HotPatchDisabled:
    xor eax, eax
    pop esi
    pop ebx
    ret
IDEMaster_HotSwapModel ENDP

; ============================================================================
; IDEMaster_ExecuteAgenticTask - Execute autonomous agent task
; Input:  ECX = task description (natural language)
;         EDX = result buffer
;         ESI = buffer size
; Output: EAX = 1 success, 0 failure
; ============================================================================
IDEMaster_ExecuteAgenticTask_Impl PROC
 lpTask:DWORD, lpResult:DWORD, cbResult:DWORD
    push ebx
    push esi
    
    cmp g_IDEConfig.bEnableAgenticMode, 0
    je @AgenticDisabled
    
    ; Update status
    push OFFSET szStatusAgentActive
    push 0
    call UIGguf_UpdateLoadingProgress
    add esp, 8
    
    ; Increment task counter
    inc g_dwAgentTaskCount
    
    ; Execute via agent system (Think → Plan → Act → Learn)
    push cbResult
    push lpResult
    push lpTask
    call AgentSystem_Execute
    add esp, 12
    
    ; Restore status
    push OFFSET szStatusReady
    push 100
    call UIGguf_UpdateLoadingProgress
    add esp, 8
    
    mov eax, 1
    pop esi
    pop ebx
    ret
    
@AgenticDisabled:
    xor eax, eax
    pop esi
    pop ebx
    ret
IDEMaster_ExecuteAgenticTask ENDP

; ============================================================================
; IDEMaster_GetSystemStatus - Get current IDE status
; Output: EAX = status structure pointer
; ============================================================================
IDEMaster_GetSystemStatus PROC
    lea eax, g_IDEConfig
    ret
IDEMaster_GetSystemStatus ENDP

; ============================================================================
; IDEMaster_EnableAutonomousBrowsing - Enable web browsing for agents
; Output: EAX = 1 success, 0 failure
; ============================================================================
IDEMaster_EnableAutonomousBrowsing PROC
    ; Set browser flag
    mov g_IDEConfig.bEnableBrowser, 1
    
    ; Initialize browser engine
    call BrowserAgent_Init
    test eax, eax
    jz @BrowserFailed
    
    mov eax, 1
    ret
    
@BrowserFailed:
    xor eax, eax
    ret
IDEMaster_EnableAutonomousBrowsing ENDP

; ============================================================================
; IDEMaster_SaveWorkspace - Save entire IDE state (panes + models + config)
; Input:  ECX = file path
; Output: EAX = 1 success, 0 failure
; ============================================================================
IDEMaster_SaveWorkspace PROC lpFilePath:DWORD
    LOCAL hFile:DWORD
    LOCAL dwBytesWritten:DWORD
    LOCAL pLayoutBuffer:DWORD
    LOCAL dwLayoutSize:DWORD
    push ebx
    push esi
    
    ; Create workspace file
    invoke CreateFileA, lpFilePath, GENERIC_WRITE, 0, 0, \
        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0
    cmp eax, INVALID_HANDLE_VALUE
    je @SaveFailed
    mov hFile, eax
    
    ; Write config structure
    invoke WriteFile, hFile, ADDR g_IDEConfig, SIZEOF IDEConfig, \
        ADDR dwBytesWritten, 0
    
    ; Save pane layout
    call LayoutManager_SaveLayout
    test eax, eax
    jz @SaveFailed
    
    ; Write layout data (EAX = layout buffer pointer)
    mov pLayoutBuffer, eax
    
    ; Get layout size (first DWORD in buffer)
    mov ebx, pLayoutBuffer
    mov eax, [ebx]
    mov dwLayoutSize, eax
    
    ; Write layout size
    invoke WriteFile, hFile, ADDR dwLayoutSize, 4, ADDR dwBytesWritten, 0
    
    ; Write layout buffer
    invoke WriteFile, hFile, pLayoutBuffer, dwLayoutSize, ADDR dwBytesWritten, 0
    
    ; Free layout buffer
    invoke GlobalFree, pLayoutBuffer
    
    invoke CloseHandle, hFile
    
    mov eax, 1
    pop esi
    pop ebx
    ret
    
@SaveFailed:
    cmp hFile, INVALID_HANDLE_VALUE
    je @NoClose
    invoke CloseHandle, hFile
@NoClose:
    xor eax, eax
    pop esi
    pop ebx
    ret
IDEMaster_SaveWorkspace ENDP

; ============================================================================
; IDEMaster_LoadWorkspace - Restore IDE state from file
; Input:  ECX = file path
; Output: EAX = 1 success, 0 failure
; ============================================================================
IDEMaster_LoadWorkspace PROC lpFilePath:DWORD
    LOCAL hFile:DWORD
    LOCAL dwBytesRead:DWORD
    LOCAL pLayoutBuffer:DWORD
    LOCAL dwLayoutSize:DWORD
    push ebx
    push esi
    
    ; Open workspace file
    invoke CreateFileA, lpFilePath, GENERIC_READ, FILE_SHARE_READ, 0, \
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0
    cmp eax, INVALID_HANDLE_VALUE
    je @LoadFailed
    mov hFile, eax
    
    ; Read config structure
    invoke ReadFile, hFile, ADDR g_IDEConfig, SIZEOF IDEConfig, \
        ADDR dwBytesRead, 0
    
    ; Restore pane layout
    ; Read layout size
    invoke ReadFile, hFile, ADDR dwLayoutSize, 4, ADDR dwBytesRead, 0
    test eax, eax
    jz @LayoutSkip
    
    ; Allocate buffer for layout
    invoke GlobalAlloc, GMEM_FIXED, dwLayoutSize
    test eax, eax
    jz @LayoutSkip
    mov pLayoutBuffer, eax
    
    ; Read layout data
    invoke ReadFile, hFile, pLayoutBuffer, dwLayoutSize, ADDR dwBytesRead, 0
    
    ; Restore layout
    push pLayoutBuffer
    call LayoutManager_LoadLayout
    add esp, 4
    
    ; Free layout buffer
    invoke GlobalFree, pLayoutBuffer
    
@LayoutSkip:
    
    invoke CloseHandle, hFile
    
    ; Re-initialize with loaded config
    call IDEMaster_Initialize
    
    mov eax, 1
    pop esi
    pop ebx
    ret
    
@LoadFailed:
    xor eax, eax
    pop esi
    pop ebx
    ret
IDEMaster_LoadWorkspace ENDP

END
