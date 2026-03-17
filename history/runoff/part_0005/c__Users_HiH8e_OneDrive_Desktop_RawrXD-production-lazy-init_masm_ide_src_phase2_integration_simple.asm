; ============================================================================
; RawrXD IDE - Phase 2: Integration Module (Simplified)
; Basic menu integration and keyboard shortcuts
; ============================================================================

.686
.model flat, stdcall
option casemap:none

include \masm32\include\windows.inc
include \masm32\include\user32.inc

includelib \masm32\lib\user32.lib

; ============================================================================
; CONSTANTS
; ============================================================================

; Menu IDs
IDM_FILE_NEW        equ 2001
IDM_FILE_OPEN       equ 2002
IDM_FILE_SAVE       equ 2003
IDM_FILE_SAVEAS     equ 2004

IDM_BUILD_BUILD     equ 2101

; Keyboard shortcuts
VK_CTRL_N           equ 'N'
VK_CTRL_O           equ 'O'
VK_CTRL_S           equ 'S'
VK_F7               equ VK_F7

; ============================================================================
; DATA SECTION
; ============================================================================

.data
    public hBuildMenu
    
    hBuildMenu          dd 0

; ============================================================================
; EXTERNAL DECLARATIONS
; ============================================================================

extrn g_hMainWindow:DWORD
extrn g_hStatusBar:DWORD
extrn g_hMainMenu:DWORD

extrn InitializeFileOperations:PROC
extrn InitializeBuildSystem:PROC
extrn FileNew:PROC
extrn FileOpen:PROC
extrn FileSave:PROC
extrn FileSaveAs:PROC
extrn BuildProject:PROC
extrn CleanupFileOperations:PROC
extrn CleanupBuildSystem:PROC
extrn HandleDropFiles:PROC

; ============================================================================
; CODE SECTION
; ============================================================================

.code

public InitializePhase2Integration
public HandlePhase2Command
public HandlePhase2KeyDown
public HandleDropFilesPhase2
public CleanupPhase2Integration

; ============================================================================
; InitializePhase2Integration - Setup Phase 2 integration
; ============================================================================
InitializePhase2Integration proc
    LOCAL hFileMenu:DWORD
    
    ; Get File menu (first menu)
    invoke GetSubMenu, g_hMainMenu, 0
    mov hFileMenu, eax
    
    ; Add Build menu
    invoke CreatePopupMenu
    mov hBuildMenu, eax
    
    ; Add build menu items
    invoke AppendMenu, hBuildMenu, MF_STRING, IDM_BUILD_BUILD, offset szBuild
    
    ; Insert Build menu into main menu
    invoke InsertMenu, g_hMainMenu, 2, MF_BYPOSITION or MF_POPUP, 
                       hBuildMenu, offset szBuildMenu
    
    ; Initialize file operations and build system
    call InitializeFileOperations
    call InitializeBuildSystem
    
    ret
InitializePhase2Integration endp

; ============================================================================
; HandlePhase2Command - Process Phase 2 menu commands
; ============================================================================
HandlePhase2Command proc wParam:DWORD, lParam:DWORD
    LOCAL commandID:DWORD
    LOCAL result:DWORD
    
    mov eax, wParam
    and eax, 0FFFFh  ; Low word is command ID
    mov commandID, eax
    
    ; Handle File menu commands
    cmp commandID, IDM_FILE_NEW
    jne @CheckOpen
    call FileNew
    mov result, 1
    jmp @CommandHandled
    
    @CheckOpen:
    cmp commandID, IDM_FILE_OPEN
    jne @CheckSave
    call FileOpen, g_hMainWindow
    mov result, 1
    jmp @CommandHandled
    
    @CheckSave:
    cmp commandID, IDM_FILE_SAVE
    jne @CheckSaveAs
    call FileSave
    mov result, 1
    jmp @CommandHandled
    
    @CheckSaveAs:
    cmp commandID, IDM_FILE_SAVEAS
    jne @CheckBuild
    call FileSaveAs
    mov result, 1
    jmp @CommandHandled
    
    @CheckBuild:
    cmp commandID, IDM_BUILD_BUILD
    jne @CommandNotHandled
    call BuildProject
    mov result, 1
    jmp @CommandHandled
    
    @CommandNotHandled:
    mov result, 0
    
    @CommandHandled:
    mov eax, result
    ret
HandlePhase2Command endp

; ============================================================================
; HandlePhase2KeyDown - Process keyboard shortcuts
; ============================================================================
HandlePhase2KeyDown proc wParam:DWORD, lParam:DWORD
    LOCAL virtKey:DWORD
    LOCAL isCtrl:DWORD
    
    ; Get virtual key code
    mov eax, wParam
    mov virtKey, eax
    
    ; Check if Ctrl is pressed
    invoke GetKeyState, VK_CONTROL
    and eax, 8000h
    cmp eax, 0
    je @CheckF7
    mov isCtrl, 1
    jmp @CheckCtrlKeys
    
    @CheckCtrlKeys:
    cmp virtKey, VK_CTRL_N
    jne @CheckCtrlO
    call FileNew
    mov eax, 1
    ret
    
    @CheckCtrlO:
    cmp virtKey, VK_CTRL_O
    jne @CheckCtrlS
    call FileOpen, g_hMainWindow
    mov eax, 1
    ret
    
    @CheckCtrlS:
    cmp virtKey, VK_CTRL_S
    jne @CheckF7
    call FileSave
    mov eax, 1
    ret
    
    @CheckF7:
    cmp virtKey, VK_F7
    jne @NoMatch
    call BuildProject
    mov eax, 1
    ret
    
    @NoMatch:
    mov eax, 0
    ret
HandlePhase2KeyDown endp

; ============================================================================
; HandleDropFilesPhase2 - Handle WM_DROPFILES for Phase 2
; ============================================================================
HandleDropFilesPhase2 proc wParam:DWORD, lParam:DWORD
    LOCAL hDrop:DWORD
    
    mov eax, wParam
    mov hDrop, eax
    
    ; Process dropped files
    call HandleDropFiles, hDrop
    
    mov eax, 1
    ret
HandleDropFilesPhase2 endp

; ============================================================================
; CleanupPhase2Integration - Release Phase 2 resources
; ============================================================================
CleanupPhase2Integration proc
    ; Cleanup file operations
    call CleanupFileOperations
    
    ; Cleanup build system
    call CleanupBuildSystem
    
    ret
CleanupPhase2Integration endp

; ============================================================================
; DATA STRINGS
; ============================================================================

szBuildMenu         db "&Build", 0
szBuild             db "&Build", 0

end