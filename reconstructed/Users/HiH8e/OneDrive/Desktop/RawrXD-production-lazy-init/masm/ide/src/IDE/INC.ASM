; ============================================================================
; IDE_INC.ASM - Common definitions and utilities for all IDE modules
; ============================================================================
.386
.model flat, stdcall
option casemap:none

include \masm32\include\windows.inc
include \masm32\include\kernel32.inc
include \masm32\include\user32.inc
include \masm32\include\advapi32.inc

includelib \masm32\lib\kernel32.lib
includelib \masm32\lib\user32.lib
includelib \masm32\lib\advapi32.lib

LOG_SIZE        equ 512

.data
LOG_FILE        db "C:\RawrXD\Logs\ide_runtime.log",0
dummy           dd 0

; String constants for logging
szIDEMasterInit         db "IDEMaster_Initialize",0
szIDEMasterLoad         db "IDEMaster_LoadModel",0
szIDEMasterHotSwap      db "IDEMaster_HotSwapModel",0
szIDEMasterExec         db "IDEMaster_ExecuteAgenticTask",0
szBrowserInit           db "BrowserAgent_Init",0
szBrowserNav            db "BrowserAgent_Navigate",0
szBrowserGetDOM         db "BrowserAgent_GetDOM",0
szBrowserClick          db "BrowserAgent_ClickElement",0
szBrowserFill           db "BrowserAgent_FillForm",0
szHotPatchInit          db "HotPatch_Init",0
szHotPatchReg           db "HotPatch_RegisterModel",0
szHotPatchSwap          db "HotPatch_SwapModel",0
szHotPatchStream        db "HotPatch_StreamedLoadModel",0
szHotPatchCap           db "HotPatch_SetStreamCap",0
szHotPatchCache         db "HotPatch_CacheModel",0
szHotPatchWarmup        db "HotPatch_WarmupModel",0
szUIProgress            db "UIGguf_UpdateLoadingProgress",0
szPaneCreate            db "PaneManager_CreatePane",0
szPaneRender            db "PaneManager_RenderAllPanes",0
szHTTPDownload          db "WinHTTP_DownloadToFileBearerA",0
szGgufAuto              db "GgufUnified_LoadModelAutomatic",0
szGgufLoad              db "GgufUnified_LoadModel",0
szDiscOpen              db "DiscStream_OpenModel",0
szDiscRead              db "DiscStream_ReadChunk",0
szPiramCompress         db "PiramHooks_CompressTensor",0
szPiramDecompress       db "PiramHooks_DecompressTensor",0
szQuantDequant          db "ReverseQuant_DequantizeBuffer",0
szInferCreate           db "InferenceBackend_CreateInferenceContext",0
szErrorLog              db "ErrorLogging_LogEvent",0
szSettingsTheme         db "IDESettings_ApplyTheme",0

; ---- LOG macro ----
LOG MACRO pszFunc:REQ, dwResult:REQ
    LOCAL buf, skip_log
    pushad
    sub esp, LOG_SIZE
    mov edi, esp
    invoke wsprintfA, edi, CSTR("%s -> %08lX"), pszFunc, dwResult
    invoke OutputDebugStringA, edi
    ; append to file
    invoke CreateFileA, offset LOG_FILE, \
           GENERIC_WRITE, FILE_SHARE_READ, NULL, \
           OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL
    cmp eax, INVALID_HANDLE_VALUE
    je skip_log
    mov ebx, eax
    invoke SetFilePointer, ebx, 0, NULL, FILE_END
    invoke lstrlenA, edi
    push eax
    invoke WriteFile, ebx, edi, eax, offset dummy, NULL
    pop eax
    invoke CloseHandle, ebx
skip_log:
    add esp, LOG_SIZE
    popad
ENDM

CSTR MACRO text:VARARG
    LOCAL lbl
    .data
    lbl db text, 0
    .code
    EXITM <offset lbl>
ENDM

.code
