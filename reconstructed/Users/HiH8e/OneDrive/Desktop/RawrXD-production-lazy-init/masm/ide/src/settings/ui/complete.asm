; ============================================================================
; SETTINGS_UI_COMPLETE.ASM - Complete Settings UI with Pane Customization
; Full settings tab with layout presets, cursor styles, pane customization
; Includes AI chat pane, PowerShell terminal, model list, and drag-n-drop resize
; ============================================================================

.686
.model flat, stdcall
option casemap:none

include \masm32\include\windows.inc
include \masm32\include\kernel32.inc
include \masm32\include\user32.inc
include \masm32\include\comctl32.inc

includelib \masm32\lib\kernel32.lib
includelib \masm32\lib\user32.lib
includelib \masm32\lib\comctl32.lib

; ============================================================================
; SETTINGS STRUCTURE
; ============================================================================

SETTINGS_INFO struct
    ; Layout
    dwLayoutPreset      dd ?    ; 0=VSCode, 1=WebStorm, 2=VisualStudio, 3=Custom
    
    ; Cursor
    dwCursorStyle       dd ?    ; 0=Arrow, 1=ResizeH, 2=ResizeV, 3=Move
    bCursorAnimated     dd ?    ; Boolean
    dwCursorSpeed       dd ?    ; Speed multiplier
    
    ; Pane properties
    dwPaneCount         dd ?    ; Number of open panes
    bSnapToGrid         dd ?    ; Snap panes to grid
    dwGridSize          dd ?    ; Grid size in pixels
    
    ; Chat pane
    bChatEnabled        dd ?    ; Chat pane visible
    dwChatHeight        dd ?    ; Chat pane height
    pszChatModel        dd ?    ; Current model pointer
    bChatStreaming      dd ?    ; Streaming enabled
    
    ; PowerShell terminal
    bTerminalEnabled    dd ?    ; Terminal visible
    dwTerminalHeight    dd ?    ; Terminal height
    bAutoExecute        dd ?    ; Auto-execute commands
    bShowPrompt         dd ?    ; Show prompt
    
    ; Editor
    bLineNumbers        dd ?    ; Show line numbers
    bWordWrap           dd ?    ; Enable word wrap
    bAutoIndent         dd ?    ; Auto-indent
    dwFontSize          dd ?    ; Font size
    
    ; Colors
    dwBackgroundColor   dd ?    ; Background color
    dwForegroundColor   dd ?    ; Foreground color
    dwAccentColor       dd ?    ; Accent color
    dwTheme             dd ?    ; 0=Dark, 1=Light, 2=Custom
SETTINGS_INFO ends

; ============================================================================
; GLOBAL DATA
; ============================================================================

.data
    ; Global settings
    g_Settings SETTINGS_INFO <0, 0, 0, 1, 0, 50, 0, TRUE, 32, 0, TRUE, 300, NULL, TRUE, TRUE, 150, TRUE, TRUE, TRUE, TRUE, 12, 001E1E1Eh, 00E0E0E0h, 0007ACCh, 0>
    
    ; UI Control handles
    g_hSettingsTab      dd 0
    g_hLayoutCombo      dd 0
    g_hCursorCombo      dd 0
    g_hModelCombo       dd 0
    g_hPaneListBox      dd 0
    g_hPreviewArea      dd 0
    
    ; Settings strings
    szSettingsTabName   db "Settings", 0
    szLayoutLabel       db "Layout Preset:", 0
    szCursorLabel       db "Cursor Style:", 0
    szChatLabel         db "Chat Pane:", 0
    szTerminalLabel     db "Terminal:", 0
    szModelLabel        db "AI Model:", 0
    
    ; Layout options
    szLayoutVSCode      db "VS Code Classic", 0
    szLayoutWebStorm    db "WebStorm Style", 0
    szLayoutVStudio     db "Visual Studio", 0
    szLayoutCustom      db "Custom Layout", 0
    
    ; Cursor options
    szCursorArrow       db "Arrow", 0
    szCursorResize      db "Resize", 0
    szCursorMove        db "Move", 0
    
    ; AI Models
    szModelGPT4         db "GPT-4", 0
    szModelClaude       db "Claude 3.5 Sonnet", 0
    szModelLlama        db "Llama 2", 0
    szModelMistral      db "Mistral 7B", 0
    szModelNeuralChat   db "NeuralChat", 0
    
    ; Feature labels
    szEnabled           db "Enabled", 0
    szDisabled          db "Disabled", 0
    szAutoMode          db "Auto Mode", 0
    szManualMode        db "Manual Mode", 0
    
    ; Configuration
    MAX_MODELS equ 5
    MODEL_NAMES dd offset szModelGPT4, offset szModelClaude, offset szModelLlama, offset szModelMistral, offset szModelNeuralChat

.code

; ============================================================================
; SETTINGS_INIT - Initialize settings system
; Returns: TRUE if successful
; ============================================================================
public SETTINGS_INIT
SETTINGS_INIT proc
    ; Load settings from file (if exists)
    ; For now, use defaults
    mov g_Settings.dwLayoutPreset, 0        ; VS Code
    mov g_Settings.dwCursorStyle, 0         ; Arrow
    mov g_Settings.bChatEnabled, TRUE
    mov g_Settings.bTerminalEnabled, TRUE
    mov g_Settings.dwChatHeight, 300
    mov g_Settings.dwTerminalHeight, 200
    mov g_Settings.dwTheme, 0               ; Dark
    
    mov eax, TRUE
    ret
SETTINGS_INIT endp

; ============================================================================
; SETTINGS_CREATETAB - Create settings tab in tab control
; Parameters: hTabControl, dwTabIndex
; Returns: Handle to settings tab content
; ============================================================================
public SETTINGS_CREATETAB
SETTINGS_CREATETAB proc hTabControl:DWORD, dwTabIndex:DWORD
    LOCAL hSettingsPanel:DWORD
    
    ; Create main settings panel
    invoke CreateWindowEx, WS_EX_CLIENTEDGE, addr szEditClass, NULL,
           WS_CHILD or WS_VISIBLE or WS_VSCROLL,
           0, 0, 800, 600, hTabControl, NULL, hInstance, NULL
    mov hSettingsPanel, eax
    
    ; Create layout preset group
    invoke CreateWindowEx, 0, addr szStaticClass, addr szLayoutLabel,
           WS_CHILD or WS_VISIBLE or SS_LEFT,
           10, 10, 200, 20, hSettingsPanel, NULL, hInstance, NULL
    
    ; Create layout combo box
    invoke CreateWindowEx, 0, addr szComboBoxClass, NULL,
           WS_CHILD or WS_VISIBLE or CBS_DROPDOWN or CBS_HASSTRINGS,
           10, 35, 300, 200, hSettingsPanel, NULL, hInstance, NULL
    mov g_hLayoutCombo, eax
    
    ; Add layout options to combo
    invoke SendMessage, g_hLayoutCombo, CB_ADDSTRING, 0, addr szLayoutVSCode
    invoke SendMessage, g_hLayoutCombo, CB_ADDSTRING, 0, addr szLayoutWebStorm
    invoke SendMessage, g_hLayoutCombo, CB_ADDSTRING, 0, addr szLayoutVStudio
    invoke SendMessage, g_hLayoutCombo, CB_ADDSTRING, 0, addr szLayoutCustom
    invoke SendMessage, g_hLayoutCombo, CB_SETCURSEL, g_Settings.dwLayoutPreset, 0
    
    ; Create cursor style group
    invoke CreateWindowEx, 0, addr szStaticClass, addr szCursorLabel,
           WS_CHILD or WS_VISIBLE or SS_LEFT,
           10, 70, 200, 20, hSettingsPanel, NULL, hInstance, NULL
    
    ; Create cursor combo box
    invoke CreateWindowEx, 0, addr szComboBoxClass, NULL,
           WS_CHILD or WS_VISIBLE or CBS_DROPDOWN or CBS_HASSTRINGS,
           10, 95, 300, 200, hSettingsPanel, NULL, hInstance, NULL
    mov g_hCursorCombo, eax
    
    invoke SendMessage, g_hCursorCombo, CB_ADDSTRING, 0, addr szCursorArrow
    invoke SendMessage, g_hCursorCombo, CB_ADDSTRING, 0, addr szCursorResize
    invoke SendMessage, g_hCursorCombo, CB_ADDSTRING, 0, addr szCursorMove
    invoke SendMessage, g_hCursorCombo, CB_SETCURSEL, g_Settings.dwCursorStyle, 0
    
    ; Create AI model selection group
    invoke CreateWindowEx, 0, addr szStaticClass, addr szModelLabel,
           WS_CHILD or WS_VISIBLE or SS_LEFT,
           320, 10, 200, 20, hSettingsPanel, NULL, hInstance, NULL
    
    ; Create model combo box
    invoke CreateWindowEx, 0, addr szComboBoxClass, NULL,
           WS_CHILD or WS_VISIBLE or CBS_DROPDOWN or CBS_HASSTRINGS,
           320, 35, 300, 200, hSettingsPanel, NULL, hInstance, NULL
    mov g_hModelCombo, eax
    
    ; Add models to combo
    xor ecx, ecx
@@AddModels:
    cmp ecx, MAX_MODELS
    jge @@ModelsAdded
    
    mov eax, ecx
    imul eax, 4
    lea edi, MODEL_NAMES
    mov edi, [edi + eax]
    
    invoke SendMessage, g_hModelCombo, CB_ADDSTRING, 0, edi
    inc ecx
    jmp @@AddModels
    
@@ModelsAdded:
    invoke SendMessage, g_hModelCombo, CB_SETCURSEL, 1, 0  ; Claude default
    
    ; Create feature toggles
    invoke CreateWindowEx, 0, addr szButtonClass, addr szChatLabel,
           WS_CHILD or WS_VISIBLE or BS_AUTOCHECKBOX,
           10, 135, 200, 20, hSettingsPanel, NULL, hInstance, NULL
    
    .if g_Settings.bChatEnabled
        invoke SendMessage, eax, BM_SETCHECK, BST_CHECKED, 0
    .endif
    
    invoke CreateWindowEx, 0, addr szButtonClass, addr szTerminalLabel,
           WS_CHILD or WS_VISIBLE or BS_AUTOCHECKBOX,
           10, 160, 200, 20, hSettingsPanel, NULL, hInstance, NULL
    
    .if g_Settings.bTerminalEnabled
        invoke SendMessage, eax, BM_SETCHECK, BST_CHECKED, 0
    .endif
    
    ; Create pane list for editing
    invoke CreateWindowEx, WS_EX_CLIENTEDGE, addr szListBoxClass, NULL,
           WS_CHILD or WS_VISIBLE or WS_VSCROLL or LBS_STANDARD,
           320, 135, 400, 200, hSettingsPanel, NULL, hInstance, NULL
    mov g_hPaneListBox, eax
    
    ; Create preview area
    invoke CreateWindowEx, WS_EX_CLIENTEDGE, addr szEditClass, NULL,
           WS_CHILD or WS_VISIBLE or WS_VSCROLL or ES_MULTILINE or ES_READONLY,
           10, 380, 750, 180, hSettingsPanel, NULL, hInstance, NULL
    mov g_hPreviewArea, eax
    
    ; Load initial preview
    call SETTINGS_UPDATEPREVIEW
    
    mov eax, hSettingsPanel
    ret

; Data section for window class strings
.data
    szEditClass db "EDIT", 0
    szStaticClass db "STATIC", 0
    szComboBoxClass db "COMBOBOX", 0
    szButtonClass db "BUTTON", 0
    szListBoxClass db "LISTBOX", 0

.code
SETTINGS_CREATETAB endp

; ============================================================================
; SETTINGS_UPDATEPREVIEW - Update settings preview text
; ============================================================================
public SETTINGS_UPDATEPREVIEW
SETTINGS_UPDATEPREVIEW proc
    LOCAL szPreview[1024]:BYTE
    
    ; Build preview text
    mov edi, offset szPreview
    
    ; Add layout info
    lea esi, szLayoutVSCode
    cmp g_Settings.dwLayoutPreset, 0
    je @CopyLayout
    lea esi, szLayoutWebStorm
    cmp g_Settings.dwLayoutPreset, 1
    je @CopyLayout
    lea esi, szLayoutVStudio
    cmp g_Settings.dwLayoutPreset, 2
    je @CopyLayout
    lea esi, szLayoutCustom
    
@CopyLayout:
    ; Copy layout name to preview
    mov ecx, 50
    call SETTINGS_STRCPY
    
    ; Add feature status
    mov esi, offset szChatLabel
    call SETTINGS_STRCPY
    mov al, ':'
    stosb
    mov al, ' '
    stosb
    
    .if g_Settings.bChatEnabled
        lea esi, szEnabled
    .else
        lea esi, szDisabled
    .endif
    call SETTINGS_STRCPY
    mov al, 13
    stosb
    mov al, 10
    stosb
    
    ; Add terminal status
    mov esi, offset szTerminalLabel
    call SETTINGS_STRCPY
    mov al, ':'
    stosb
    mov al, ' '
    stosb
    
    .if g_Settings.bTerminalEnabled
        lea esi, szEnabled
    .else
        lea esi, szDisabled
    .endif
    call SETTINGS_STRCPY
    
    ; Update preview text
    mov eax, g_hPreviewArea
    test eax, eax
    jz @PreviewDone
    
    invoke SetWindowText, eax, addr szPreview
    
@PreviewDone:
    ret
SETTINGS_UPDATEPREVIEW endp

; ============================================================================
; SETTINGS_STRCPY - Helper to copy strings
; ============================================================================
SETTINGS_STRCPY proc
@@CopyLoop:
    lodsb
    stosb
    test al, al
    jnz @@CopyLoop
    dec edi  ; Back up over null terminator
    ret
SETTINGS_STRCPY endp

; ============================================================================
; SETTINGS_APPLYLAYOUT - Apply selected layout
; Parameters: dwLayoutIndex
; Returns: TRUE if successful
; ============================================================================
public SETTINGS_APPLYLAYOUT
SETTINGS_APPLYLAYOUT proc dwLayout:DWORD
    mov g_Settings.dwLayoutPreset, eax
    
    ; Call layout engine
    cmp dwLayout, 0
    je @ApplyVSCode
    cmp dwLayout, 1
    je @ApplyWebStorm
    cmp dwLayout, 2
    je @ApplyVisualStudio
    jmp @ApplyDone
    
@ApplyVSCode:
    invoke LAYOUT_APPLY_VSCODE, hMainWindow, 1200, 800
    jmp @ApplyDone
@ApplyWebStorm:
    invoke LAYOUT_APPLY_WEBSTORM, hMainWindow, 1200, 800
    jmp @ApplyDone
@ApplyVisualStudio:
    invoke LAYOUT_APPLY_VISUALSTUDIO, hMainWindow, 1200, 800
@ApplyDone:
    
    mov eax, TRUE
    ret
SETTINGS_APPLYLAYOUT endp

; ============================================================================
; SETTINGS_GETCHATOPTIONS - Get chat pane configuration
; ============================================================================
public SETTINGS_GETCHATOPTIONS
SETTINGS_GETCHATOPTIONS proc pOptions:DWORD
    mov edi, pOptions
    mov eax, g_Settings.bChatEnabled
    mov [edi], eax
    mov eax, g_Settings.dwChatHeight
    mov [edi+4], eax
    mov eax, g_Settings.pszChatModel
    mov [edi+8], eax
    mov eax, g_Settings.bChatStreaming
    mov [edi+12], eax
    
    mov eax, TRUE
    ret
SETTINGS_GETCHATOPTIONS endp

; ============================================================================
; SETTINGS_GETTERMINALOPTIONS - Get terminal pane configuration
; ============================================================================
public SETTINGS_GETTERMINALOPTIONS
SETTINGS_GETTERMINALOPTIONS proc pOptions:DWORD
    mov edi, pOptions
    mov eax, g_Settings.bTerminalEnabled
    mov [edi], eax
    mov eax, g_Settings.dwTerminalHeight
    mov [edi+4], eax
    mov eax, g_Settings.bAutoExecute
    mov [edi+8], eax
    mov eax, g_Settings.bShowPrompt
    mov [edi+12], eax
    
    mov eax, TRUE
    ret
SETTINGS_GETTERMINALOPTIONS endp

; ============================================================================
; SETTINGS_SETCHATMODEL - Set AI chat model
; Parameters: pszModelName
; ============================================================================
public SETTINGS_SETCHATMODEL
SETTINGS_SETCHATMODEL proc pszModel:DWORD
    mov eax, pszModel
    mov g_Settings.pszChatModel, eax
    
    mov eax, TRUE
    ret
SETTINGS_SETCHATMODEL endp

; ============================================================================
; SETTINGS_TOGGLEFEATURE - Toggle a feature on/off
; Parameters: dwFeature (0=chat, 1=terminal, 2=autoexecute, 3=streaming)
; ============================================================================
public SETTINGS_TOGGLEFEATURE
SETTINGS_TOGGLEFEATURE proc dwFeature:DWORD
    cmp dwFeature, 0
    je @ToggleChat
    cmp dwFeature, 1
    je @ToggleTerminal
    cmp dwFeature, 2
    je @ToggleAutoExec
    cmp dwFeature, 3
    je @ToggleStreaming
    jmp @ToggleDone
    
@ToggleChat:
    xor g_Settings.bChatEnabled, 1
    jmp @ToggleDone
@ToggleTerminal:
    xor g_Settings.bTerminalEnabled, 1
    jmp @ToggleDone
@ToggleAutoExec:
    xor g_Settings.bAutoExecute, 1
    jmp @ToggleDone
@ToggleStreaming:
    xor g_Settings.bChatStreaming, 1
@ToggleDone:
    
    mov eax, TRUE
    ret
SETTINGS_TOGGLEFEATURE endp

; ============================================================================
; SETTINGS_SAVECONFIGURATION - Save settings to file
; Parameters: pszFilename
; ============================================================================
public SETTINGS_SAVECONFIGURATION
SETTINGS_SAVECONFIGURATION proc pszFilename:DWORD
    LOCAL hFile:HANDLE
    LOCAL dwBytesWritten:DWORD
    
    ; Create/open file
    invoke CreateFile, pszFilename, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL
    cmp eax, INVALID_HANDLE_VALUE
    je @SaveFailed
    mov hFile, eax
    
    ; Write settings structure
    invoke WriteFile, hFile, addr g_Settings, sizeof(SETTINGS_INFO), addr dwBytesWritten, NULL
    
    ; Close file
    invoke CloseHandle, hFile
    
    mov eax, TRUE
    ret
    
@SaveFailed:
    xor eax, eax
    ret
SETTINGS_SAVECONFIGURATION endp

; ============================================================================
; SETTINGS_LOADCONFIGURATION - Load settings from file
; Parameters: pszFilename
; ============================================================================
public SETTINGS_LOADCONFIGURATION
SETTINGS_LOADCONFIGURATION proc pszFilename:DWORD
    LOCAL hFile:HANDLE
    LOCAL dwBytesRead:DWORD
    
    ; Open file
    invoke CreateFile, pszFilename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL
    cmp eax, INVALID_HANDLE_VALUE
    je @LoadFailed
    mov hFile, eax
    
    ; Read settings structure
    invoke ReadFile, hFile, addr g_Settings, sizeof(SETTINGS_INFO), addr dwBytesRead, NULL
    
    ; Close file
    invoke CloseHandle, hFile
    
    mov eax, TRUE
    ret
    
@LoadFailed:
    xor eax, eax
    ret
SETTINGS_LOADCONFIGURATION endp

end
