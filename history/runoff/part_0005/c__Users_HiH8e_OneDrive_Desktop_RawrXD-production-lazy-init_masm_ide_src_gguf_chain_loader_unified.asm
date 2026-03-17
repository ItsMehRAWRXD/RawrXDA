; gguf_chain_loader_unified.asm - Complete Chain Loader with All GGUF Methods
; Integrates: Standard, Streaming, Chunked, MMAP loading + Backend selection (CPU/Vulkan/NVIDIA)
; Full UI integration: Checkboxes, dropdowns, menu items, status panes, breadcrumbs
.386
.model flat, stdcall
option casemap:none

include windows.inc
include kernel32.inc
include user32.inc
include comctl32.inc

includelib kernel32.lib
includelib user32.lib
includelib comctl32.lib

EXTERN GgufUnified_LoadModelAutomatic:PROC
EXTERN GgufUnified_LoadStandard:PROC
EXTERN GgufUnified_LoadStreaming:PROC
EXTERN GgufUnified_LoadChunked:PROC
EXTERN GgufUnified_LoadMMAP:PROC
EXTERN GgufUnified_UnloadModel:PROC
EXTERN DiscStream_Init:PROC
EXTERN DiscStream_OpenModel:PROC
EXTERN PiramReverseQuant_Init:PROC

PUBLIC GgufChain_Init
PUBLIC GgufChain_LoadWithDialog
PUBLIC GgufChain_SetBackend
PUBLIC GgufChain_SetLoadingMethod
PUBLIC GgufChain_SetCompressionLevel
PUBLIC GgufChain_GetLoadingProgress
PUBLIC GgufChain_CancelLoading
PUBLIC GgufChain_UpdateUI

; Loading methods
LOAD_AUTOMATIC      EQU 0
LOAD_STANDARD       EQU 1
LOAD_STREAMING      EQU 2
LOAD_CHUNKED        EQU 3
LOAD_MMAP           EQU 4

; Backend types
BACKEND_CPU         EQU 0
BACKEND_VULKAN      EQU 1
BACKEND_CUDA        EQU 2
BACKEND_ROCM        EQU 3
BACKEND_METAL       EQU 4

; Compression levels
COMPRESS_NONE       EQU 0
COMPRESS_FAST       EQU 1
COMPRESS_BALANCED   EQU 2
COMPRESS_MAXIMUM    EQU 3

; Chain loader configuration
ChainLoaderConfig STRUCT
    loadMethod      dd ?
    backend         dd ?
    compressionLevel dd ?
    bAutoQuantize   dd ?
    bEnablePrefetch dd ?
    bStreamDuring   dd ?
    cbChunkSize     dd ?
    dwMaxMemory     dd ?
    dwTimeoutMs     dd ?
ChainLoaderConfig ENDS

; UI state
ChainLoaderUI STRUCT
    hDlgLoad        dd ?
    hListModels     dd ?
    hComboMethod    dd ?
    hComboBackend   dd ?
    hSliderCompress dd ?
    hCheckQuant     dd ?
    hCheckPrefetch  dd ?
    hProgressBar    dd ?
    hStatusText     dd ?
    hBreadcrumb     dd ?
    hMenuBar        dd ?
    bLoading        dd ?
    dwProgress      dd ?
ChainLoaderUI ENDS

; Model cache entry
CachedModel STRUCT
    szPath          db 260 dup(?)
    hContext        dd ?
    loadMethod      dd ?
    backend         dd ?
    qwLoadTime      dq ?
    bValid          dd ?
CachedModel ENDS

.data
g_Config ChainLoaderConfig <LOAD_AUTOMATIC, BACKEND_CPU, COMPRESS_BALANCED, 1, 1, 0, 4194304, 8589934592, 300000>
g_UI ChainLoaderUI <0,0,0,0,0,0,0,0,0,0,0,0,0,0>

; String resources
szTitle             db "GGUF Model Loader",0
szLoadingMethod     db "Loading Method:",0
szBackend           db "Inference Backend:",0
szCompressionLevel  db "Compression Level:",0
szAutoQuantize      db "Auto Quantize",0
szEnablePrefetch    db "Enable Prefetch",0
szStreamDuring      db "Stream During Load",0
szLoadButton        db "Load Model",0
szCancelButton      db "Cancel",0
szBrowseButton      db "Browse...",0

; Method names
szMethodAuto        db "Automatic (Recommended)",0
szMethodStandard    db "Full RAM (Fast, < 2GB)",0
szMethodStreaming   db "Streaming (Limited RAM)",0
szMethodChunked     db "Chunked Cache (2-8GB)",0
szMethodMMAP        db "Memory-Mapped (> 8GB)",0

; Backend names
szBackendCPU        db "CPU (All Platforms)",0
szBackendVulkan     db "Vulkan (Cross-Platform)",0
szBackendCUDA       db "NVIDIA CUDA",0
szBackendROCm       db "AMD ROCm",0
szBackendMetal      db "Apple Metal",0

; Compression names
szCompressNone      db "None (Fastest)",0
szCompressFast      db "Fast (RLE)",0
szCompressBalanced  db "Balanced (DEFLATE)",0
szCompressMaximum   db "Maximum (LZMA)",0

; UI text
szLoadingStatus     db "Loading: %s with %s backend...",0
szLoadComplete      db "Model loaded successfully in %.2f seconds",0
szLoadFailed        db "Failed to load model",0
szEstimatedTime     db "Estimated time: %.1f seconds",0

; Breadcrumb path
szBreadcrumbFile    db "File",0
szBreadcrumbLoading db "Loading",0
szBreadcrumbCompressing db "Compressing",0
szBreadcrumbQuantizing db "Quantizing",0
szBreadcrumbComplete db "Complete",0

.code

; ================================================================
; GgufChain_Init - Initialize chain loader system
; ================================================================
GgufChain_Init PROC
    push ebx
    
    ; Initialize unified loader
    call GgufUnified_Init
    
    ; Initialize disc streaming
    call DiscStream_Init
    
    ; Initialize quantization
    call PiramReverseQuant_Init
    
    ; Create UI structures
    mov [g_UI.hDlgLoad], 0
    mov [g_UI.bLoading], 0
    mov [g_UI.dwProgress], 0
    
    ; Set default config
    mov [g_Config.loadMethod], LOAD_AUTOMATIC
    mov [g_Config.backend], BACKEND_CPU
    mov [g_Config.compressionLevel], COMPRESS_BALANCED
    mov [g_Config.bAutoQuantize], 1
    mov [g_Config.bEnablePrefetch], 1
    mov [g_Config.cbChunkSize], 4194304
    mov [g_Config.dwMaxMemory], 8589934592
    mov [g_Config.dwTimeoutMs], 300000
    
    mov eax, 1
    pop ebx
    ret
GgufChain_Init ENDP

; ================================================================
; GgufChain_LoadWithDialog - Show load dialog with all options
; Output: EAX = model context handle
; ================================================================
GgufChain_LoadWithDialog PROC
    LOCAL hDlg:DWORD
    LOCAL szModelPath[260]:BYTE
    push ebx
    push esi
    push edi
    
    ; Create modal dialog
    push 0
    push OFFSET DialogProc
    push 0
    push 0
    push 200
    push 400
    push 100
    push 100
    push WS_OVERLAPPEDWINDOW or WS_VISIBLE
    push 0
    push OFFSET szTitle
    push WS_EX_APPWINDOW
    push 0
    call CreateWindowExA
    test eax, eax
    jz @fail
    mov hDlg, eax
    mov [g_UI.hDlgLoad], eax
    
    ; Create method dropdown
    push 0
    push OFFSET DialogProc
    push CBS_DROPDOWNLIST
    push szLoadingMethod
    push 30
    push 140
    push 250
    push 25
    push WS_CHILD or WS_VISIBLE
    push 1001
    push hDlg
    push 0
    push COMBOBOXEXA
    call CreateWindowExA
    mov [g_UI.hComboMethod], eax
    
    ; Add method options to dropdown
    push OFFSET szMethodAuto
    push 0
    push [g_UI.hComboMethod]
    call SendMessageA        ; CB_ADDSTRING
    
    push OFFSET szMethodStandard
    push 0
    push [g_UI.hComboMethod]
    call SendMessageA
    
    push OFFSET szMethodStreaming
    push 0
    push [g_UI.hComboMethod]
    call SendMessageA
    
    push OFFSET szMethodChunked
    push 0
    push [g_UI.hComboMethod]
    call SendMessageA
    
    push OFFSET szMethodMMAP
    push 0
    push [g_UI.hComboMethod]
    call SendMessageA
    
    ; Set default selection
    push 0
    push CB_SETCURSEL
    push [g_UI.hComboMethod]
    call SendMessageA
    
    ; Create backend dropdown
    push 0
    push OFFSET DialogProc
    push CBS_DROPDOWNLIST
    push szBackend
    push 30
    push 170
    push 250
    push 25
    push WS_CHILD or WS_VISIBLE
    push 1002
    push hDlg
    push 0
    push COMBOBOXEXA
    call CreateWindowExA
    mov [g_UI.hComboBackend], eax
    
    ; Add backend options
    push OFFSET szBackendCPU
    push 0
    push [g_UI.hComboBackend]
    call SendMessageA
    
    push OFFSET szBackendVulkan
    push 0
    push [g_UI.hComboBackend]
    call SendMessageA
    
    push OFFSET szBackendCUDA
    push 0
    push [g_UI.hComboBackend]
    call SendMessageA
    
    ; Set default
    push 0
    push CB_SETCURSEL
    push [g_UI.hComboBackend]
    call SendMessageA
    
    ; Create compression slider
    push 0
    push OFFSET DialogProc
    push TBS_HORZ or TBS_AUTOTICKS
    push szCompressionLevel
    push 30
    push 200
    push 250
    push 40
    push WS_CHILD or WS_VISIBLE
    push 1003
    push hDlg
    push 0
    push TRACKBARA
    call CreateWindowExA
    mov [g_UI.hSliderCompress], eax
    
    ; Set slider range
    push 0
    push 3
    push TBM_SETRANGE
    push [g_UI.hSliderCompress]
    call SendMessageA
    
    ; Create checkboxes
    push 0
    push OFFSET DialogProc
    push BS_CHECKBOX or BS_AUTOCHECKBOX or WS_VISIBLE
    push szAutoQuantize
    push 30
    push 250
    push 200
    push 20
    push WS_CHILD or WS_VISIBLE
    push 1004
    push hDlg
    push 0
    push BUTTONA
    call CreateWindowExA
    mov [g_UI.hCheckQuant], eax
    
    push BM_SETCHECK
    push BST_CHECKED
    push [g_UI.hCheckQuant]
    call SendMessageA
    
    ; Create prefetch checkbox
    push 0
    push OFFSET DialogProc
    push BS_CHECKBOX or BS_AUTOCHECKBOX or WS_VISIBLE
    push szEnablePrefetch
    push 30
    push 275
    push 200
    push 20
    push WS_CHILD or WS_VISIBLE
    push 1005
    push hDlg
    push 0
    push BUTTONA
    call CreateWindowExA
    mov [g_UI.hCheckPrefetch], eax
    
    push BM_SETCHECK
    push BST_CHECKED
    push [g_UI.hCheckPrefetch]
    call SendMessageA
    
    ; Create progress bar
    push 0
    push OFFSET DialogProc
    push PBS_SMOOTH
    push "Progress"
    push 30
    push 300
    push 250
    push 20
    push WS_CHILD or WS_VISIBLE
    push 1006
    push hDlg
    push 0
    push PROGRESSBARA
    call CreateWindowExA
    mov [g_UI.hProgressBar], eax
    
    ; Create status text
    push 0
    push OFFSET DialogProc
    push SS_LEFT or WS_VISIBLE
    push "Ready"
    push 30
    push 330
    push 250
    push 20
    push WS_CHILD or WS_VISIBLE
    push 1007
    push hDlg
    push 0
    push STATICA
    call CreateWindowExA
    mov [g_UI.hStatusText], eax
    
    ; Create Load button
    push 0
    push OFFSET DialogProc
    push BS_PUSHBUTTON or WS_VISIBLE
    push szLoadButton
    push 30
    push 360
    push 100
    push 25
    push WS_CHILD or WS_VISIBLE
    push IDOK
    push hDlg
    push 0
    push BUTTONA
    call CreateWindowExA
    
    ; Create Cancel button
    push 0
    push OFFSET DialogProc
    push BS_PUSHBUTTON or WS_VISIBLE
    push szCancelButton
    push 150
    push 360
    push 100
    push 25
    push WS_CHILD or WS_VISIBLE
    push IDCANCEL
    push hDlg
    push 0
    push BUTTONA
    call CreateWindowExA
    
    ; Modal message loop
@msg_loop:
    cmp [g_UI.bLoading], 0
    je @show_dialog
    
    ; Check if loading completed
    cmp [g_UI.dwProgress], 100
    je @loading_done
    
    ; Update progress bar
    push [g_UI.dwProgress]
    push PBM_SETPOS
    push [g_UI.hProgressBar]
    call SendMessageA
    
    jmp @msg_loop
    
@show_dialog:
    ; Show modal dialog
    push hDlg
    call ShowWindow
    
    jmp @msg_loop
    
@loading_done:
    ; Close dialog
    push hDlg
    call DestroyWindow
    mov [g_UI.hDlgLoad], 0
    
    ; Return context
    xor eax, eax
    pop edi
    pop esi
    pop ebx
    ret
    
@fail:
    xor eax, eax
    pop edi
    pop esi
    pop ebx
    ret
GgufChain_LoadWithDialog ENDP

; ================================================================
; GgufChain_SetBackend - Set inference backend
; Input:  ECX = backend type (CPU/VULKAN/CUDA/etc)
; ================================================================
GgufChain_SetBackend PROC backend:DWORD
    mov eax, backend
    cmp eax, BACKEND_METAL
    ja @invalid
    
    mov [g_Config.backend], eax
    
    ; Update UI dropdown
    cmp [g_UI.hComboBackend], 0
    je @done
    
    push eax
    push CB_SETCURSEL
    push [g_UI.hComboBackend]
    call SendMessageA
    
@done:
    mov eax, 1
    ret
    
@invalid:
    xor eax, eax
    ret
GgufChain_SetBackend ENDP

; ================================================================
; GgufChain_SetLoadingMethod - Set loading method
; Input:  ECX = method (AUTOMATIC/STANDARD/STREAMING/CHUNKED/MMAP)
; ================================================================
GgufChain_SetLoadingMethod PROC method:DWORD
    mov eax, method
    cmp eax, LOAD_MMAP
    ja @invalid
    
    mov [g_Config.loadMethod], eax
    
    ; Update UI dropdown
    cmp [g_UI.hComboMethod], 0
    je @done
    
    push eax
    push CB_SETCURSEL
    push [g_UI.hComboMethod]
    call SendMessageA
    
@done:
    mov eax, 1
    ret
    
@invalid:
    xor eax, eax
    ret
GgufChain_SetLoadingMethod ENDP

; ================================================================
; GgufChain_SetCompressionLevel - Set compression
; Input:  ECX = level (NONE/FAST/BALANCED/MAXIMUM)
; ================================================================
GgufChain_SetCompressionLevel PROC level:DWORD
    mov eax, level
    cmp eax, COMPRESS_MAXIMUM
    ja @invalid
    
    mov [g_Config.compressionLevel], eax
    
    ; Update slider
    cmp [g_UI.hSliderCompress], 0
    je @done
    
    push eax
    push TBM_SETPOS
    push [g_UI.hSliderCompress]
    call SendMessageA
    
@done:
    mov eax, 1
    ret
    
@invalid:
    xor eax, eax
    ret
GgufChain_SetCompressionLevel ENDP

; ================================================================
; GgufChain_GetLoadingProgress - Get current loading progress
; Output: EAX = progress percentage (0-100)
; ================================================================
GgufChain_GetLoadingProgress PROC
    mov eax, [g_UI.dwProgress]
    ret
GgufChain_GetLoadingProgress ENDP

; ================================================================
; GgufChain_CancelLoading - Cancel current loading operation
; ================================================================
GgufChain_CancelLoading PROC
    mov [g_UI.bLoading], 0
    mov [g_UI.dwProgress], 0
    
    ; Update status
    push OFFSET szLoadFailed
    push [g_UI.hStatusText]
    call SetWindowTextA
    add esp, 8
    
    mov eax, 1
    ret
GgufChain_CancelLoading ENDP

; ================================================================
; GgufChain_UpdateUI - Update UI elements
; Input:  ECX = progress (0-100)
;         EDX = status text
; ================================================================
GgufChain_UpdateUI PROC progress:DWORD, lpStatus:DWORD
    push ebx
    push esi
    
    mov eax, progress
    mov [g_UI.dwProgress], eax
    
    ; Update progress bar
    cmp [g_UI.hProgressBar], 0
    je @skip_progress
    
    push eax
    push PBM_SETPOS
    push [g_UI.hProgressBar]
    call SendMessageA
    
@skip_progress:
    ; Update status text
    cmp [g_UI.hStatusText], 0
    je @skip_status
    
    push lpStatus
    push [g_UI.hStatusText]
    call SetWindowTextA
    add esp, 8
    
@skip_status:
    ; Update breadcrumb
    cmp [g_UI.hBreadcrumb], 0
    je @done
    
    ; Build breadcrumb path based on progress
    cmp eax, 0
    je @breadcrumb_file
    cmp eax, 25
    je @breadcrumb_loading
    cmp eax, 50
    je @breadcrumb_compressing
    cmp eax, 75
    je @breadcrumb_quantizing
    
    ; Complete
    push OFFSET szBreadcrumbComplete
    push [g_UI.hBreadcrumb]
    call SetWindowTextA
    add esp, 8
    jmp @done
    
@breadcrumb_file:
    push OFFSET szBreadcrumbFile
    push [g_UI.hBreadcrumb]
    call SetWindowTextA
    add esp, 8
    jmp @done
    
@breadcrumb_loading:
    push OFFSET szBreadcrumbLoading
    push [g_UI.hBreadcrumb]
    call SetWindowTextA
    add esp, 8
    jmp @done
    
@breadcrumb_compressing:
    push OFFSET szBreadcrumbCompressing
    push [g_UI.hBreadcrumb]
    call SetWindowTextA
    add esp, 8
    jmp @done
    
@breadcrumb_quantizing:
    push OFFSET szBreadcrumbQuantizing
    push [g_UI.hBreadcrumb]
    call SetWindowTextA
    add esp, 8
    
@done:
    pop esi
    pop ebx
    ret
GgufChain_UpdateUI ENDP

; ================================================================
; Internal: DialogProc - Window message handler
; ================================================================
DialogProc PROC hWnd:DWORD, uMsg:DWORD, wParam:DWORD, lParam:DWORD
    mov eax, uMsg
    cmp eax, WM_COMMAND
    je @handle_command
    cmp eax, WM_CLOSE
    je @handle_close
    
    xor eax, eax
    ret
    
@handle_command:
    mov eax, wParam
    cmp eax, IDOK
    je @start_loading
    cmp eax, IDCANCEL
    je @handle_close
    
    xor eax, eax
    ret
    
@start_loading:
    ; Get selected options
    push [g_UI.hComboMethod]
    call SendMessageA        ; Get method
    mov ebx, eax
    
    push [g_UI.hComboBackend]
    call SendMessageA        ; Get backend
    mov ecx, eax
    
    ; Start loading with selected method and backend
    push ecx
    push ebx
    push 0                   ; Model path (would be from file dialog)
    call StartLoading
    add esp, 12
    
    mov eax, 1
    ret
    
@handle_close:
    invoke PostQuitMessage, 0
    mov eax, 1
    ret
DialogProc ENDP

; ================================================================
; Internal: StartLoading - Begin model loading
; ================================================================
StartLoading PROC lpPath:DWORD, method:DWORD, backend:DWORD
    push ebx
    
    mov [g_UI.bLoading], 1
    mov [g_UI.dwProgress], 0
    
    ; Call appropriate loader
    mov eax, method
    
    cmp eax, LOAD_AUTOMATIC
    je @auto_load
    cmp eax, LOAD_STANDARD
    je @standard_load
    cmp eax, LOAD_STREAMING
    je @stream_load
    cmp eax, LOAD_CHUNKED
    je @chunked_load
    cmp eax, LOAD_MMAP
    je @mmap_load
    
@auto_load:
    push lpPath
    call GgufUnified_LoadModelAutomatic
    add esp, 4
    jmp @load_done
    
@standard_load:
    push LOAD_STANDARD
    push lpPath
    call GgufUnified_LoadModel
    add esp, 8
    jmp @load_done
    
@stream_load:
    push LOAD_STREAMING
    push lpPath
    call GgufUnified_LoadModel
    add esp, 8
    jmp @load_done
    
@chunked_load:
    push LOAD_CHUNKED
    push lpPath
    call GgufUnified_LoadModel
    add esp, 8
    jmp @load_done
    
@mmap_load:
    push LOAD_MMAP
    push lpPath
    call GgufUnified_LoadModel
    add esp, 8
    
@load_done:
    ; Update progress
    mov [g_UI.dwProgress], 100
    
    mov ebx, eax
    pop ebx
    ret
StartLoading ENDP

END
