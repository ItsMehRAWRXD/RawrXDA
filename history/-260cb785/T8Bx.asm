; ============================================================================
; RawrXD Agentic IDE - UI Layout Manager (Pure MASM)
; Initializes and manages main UI components
; ============================================================================

.686
.model flat, stdcall
option casemap:none

include \masm32\include\windows.inc
include \masm32\include\kernel32.inc
include \masm32\include\user32.inc
; include commctrl.inc  ; Use standard Windows constants instead

includelib \masm32\lib\kernel32.lib
includelib \masm32\lib\user32.lib
includelib \masm32\lib\comctl32.lib

; External declarations
extern g_hMainWindow:DWORD
extern g_hMainFont:DWORD
extern g_hInstance:DWORD

; Forward declarations
CreateTabControl proto
CreateFileExplorer proto
RefreshFileExplorer proto
CreateOrchestraPanel proto

; ============================================================================
; Data Section
; ============================================================================

.data
include constants.inc
include structures.inc
include macros.inc

    ; Handles for UI components
    g_hTabControl      dd 0
    g_hFileTree        dd 0
    g_hOrchestra       dd 0
    g_hStatusBar       dd 0

public g_hFileTree

; ============================================================================
; Procedures
; ============================================================================

.code

public InitializeUILayout

; ============================================================================
; InitializeUILayout - Initialize all UI components
; Returns: Success (1) or failure (0) in eax
; ============================================================================
InitializeUILayout proc
    
    ; Create tab control (main multi-document interface)
    call CreateTabControl
    mov g_hTabControl, eax
    test eax, eax
    jz @LayoutFailed
    
    ; Create file explorer (left panel)
    call CreateFileExplorer
    mov g_hFileTree, eax
    ; File tree is optional, doesn't fail if not created
    
    ; Create orchestra panel (bottom panel)
    call CreateOrchestraPanel
    mov g_hOrchestra, eax
    ; Orchestra is important but we continue if it fails
    
    ; All critical components initialized
    mov eax, 1  ; Success
    ret
    
@LayoutFailed:
    xor eax, eax  ; Failure
    ret
InitializeUILayout endp

; ============================================================================
; ShowAllComponents - Show all UI components
; ============================================================================
ShowAllComponents proc
    
    .if g_hTabControl
        invoke ShowWindow, g_hTabControl, SW_SHOW
        invoke UpdateWindow, g_hTabControl
    .endif
    
    .if g_hFileTree
        invoke ShowWindow, g_hFileTree, SW_SHOW
        invoke UpdateWindow, g_hFileTree
    .endif
    
    .if g_hOrchestra
        invoke ShowWindow, g_hOrchestra, SW_SHOW
        invoke UpdateWindow, g_hOrchestra
    .endif
    
    ret
ShowAllComponents endp

; ============================================================================
; HideAllComponents - Hide all UI components
; ============================================================================
HideAllComponents proc
    
    .if g_hTabControl
        invoke ShowWindow, g_hTabControl, SW_HIDE
    .endif
    
    .if g_hFileTree
        invoke ShowWindow, g_hFileTree, SW_HIDE
    .endif
    
    .if g_hOrchestra
        invoke ShowWindow, g_hOrchestra, SW_HIDE
    .endif
    
    ret
HideAllComponents endp

; ============================================================================
; GetTabControl - Get tab control handle
; Returns: Tab handle in eax
; ============================================================================
GetTabControl proc
    mov eax, g_hTabControl
    ret
GetTabControl endp

; ============================================================================
; GetFileTree - Get file tree handle
; Returns: File tree handle in eax
; ============================================================================
GetFileTree proc
    mov eax, g_hFileTree
    ret
GetFileTree endp

; ============================================================================
; GetOrchestraPanel - Get orchestra panel handle
; Returns: Orchestra handle in eax
; ============================================================================
GetOrchestraPanel proc
    mov eax, g_hOrchestra
    ret
GetOrchestraPanel endp

; ============================================================================
; RefreshFileTree - forwarder to explorer refresher
; ============================================================================
public RefreshFileTree
RefreshFileTree proc
    call RefreshFileExplorer
    ret
RefreshFileTree endp

; Tab control constants (defined here to avoid conflicts)
TCM_INSERTITEM          equ 4871  ; 0x1300 + 7

end InitializeUILayout
