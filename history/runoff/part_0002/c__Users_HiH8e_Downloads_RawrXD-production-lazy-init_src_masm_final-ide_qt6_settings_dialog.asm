; ============================================================================
; Settings Dialog Implementation for RawrXD Pure MASM IDE
; ============================================================================
; Phase 4 - Settings Dialog with 7 Tabs
; ============================================================================
; Based on: src/qtapp/settings_dialog.cpp (23,338 bytes)
; ============================================================================

include windows.inc
include kernel32.inc
include user32.inc
include comctl32.inc
include "dialog_system.inc"
include "tab_control.inc"
include "listview_control.inc"

; ============================================================================
; EXTERNAL DECLARATIONS - Phase 7 Quantization Module
; ============================================================================
EXTERN QuantizationControls_Create:PROC
EXTERN QuantizationControls_PopulateComboBox:PROC
EXTERN QuantizationControls_UpdateVRAMDisplay:PROC
EXTERN QuantizationControls_ApplyQuantization:PROC
EXTERN QuantizationControls_GetRecommendedQuantization:PROC
EXTERN QuantizationControls_SaveSettings:PROC

.DATA

; ============================================================================
; Settings Dialog Constants
; ============================================================================
SETTINGS_DIALOG_WIDTH     EQU 600
SETTINGS_DIALOG_HEIGHT    EQU 500
TAB_CONTROL_HEIGHT        EQU 400

; ============================================================================
; Settings Dialog Structure
; ============================================================================
SETTINGS_DIALOG STRUCT
    hwnd                   QWORD ?     ; Dialog window
    tab_control            QWORD ?     ; TAB_CONTROL pointer
    settings_data          QWORD ?     ; SETTINGS_DATA pointer
    is_dirty               BYTE ?      ; Unsaved changes flag
    padding                BYTE 7 DUP(?)
SETTINGS_DIALOG ENDS

; ============================================================================
; Tab Page Handles
; ============================================================================
TAB_GENERAL       EQU 0
TAB_MODEL         EQU 1
TAB_CHAT          EQU 2
TAB_SECURITY      EQU 3
TAB_TRAINING      EQU 4
TAB_CICD          EQU 5
TAB_ENTERPRISE    EQU 6
TAB_QUANTIZATION  EQU 7

; ============================================================================
; Control IDs
; ============================================================================
IDC_AUTO_SAVE             EQU 1001
IDC_STARTUP_FULLSCREEN    EQU 1002
IDC_FONT_SIZE             EQU 1003
IDC_MODEL_PATH            EQU 1004
IDC_BROWSE_MODEL          EQU 1005
IDC_DEFAULT_MODEL         EQU 1006
IDC_CHAT_MODEL            EQU 1007
IDC_TEMPERATURE           EQU 1008
IDC_MAX_TOKENS            EQU 1009
IDC_SYSTEM_PROMPT         EQU 1010
IDC_API_KEY               EQU 1011
IDC_ENCRYPTION            EQU 1012
IDC_SECURE_STORAGE        EQU 1013
IDC_OK                    EQU 1014
IDC_CANCEL                EQU 1015
IDC_APPLY                 EQU 1016
IDC_QUANT_COMBO           EQU 3101
IDC_VRAM_LABEL            EQU 3102
IDC_CURRENT_QUANT_LABEL   EQU 3104
IDC_APPLY_QUANT_BUTTON    EQU 3105
IDC_AUTO_SELECT_CHECK     EQU 3106
IDC_VRAM_PROGRESS         EQU 3107

; Buffer sizing
SETTINGS_PATH_CHARS       EQU 260
SETTINGS_PROMPT_CHARS     EQU 512
SETTINGS_APIKEY_CHARS     EQU 256

; ============================================================================
; Window Class Names (Wide)
; ============================================================================
WC_BUTTON_W               DW 'B','U','T','T','O','N',0
WC_EDIT_W                 DW 'E','D','I','T',0
WC_COMBOBOX_W             DW 'C','O','M','B','O','B','O','X',0
WC_UPDOWN_W               DW 'm','s','c','t','l','s','_','u','p','d','o','w','n','3','2',0
WC_STATIC_W               DW 'S','T','A','T','I','C',0
WC_PROGRESS               DW 'm','s','c','t','l','s','_','p','r','o','g','r','e','s','s','3','2',0

; ============================================================================
; UI Strings (Wide)
; ============================================================================
STR_AUTO_SAVE             DW 'A','u','t','o','-','s','a','v','e',' ','s','e','t','t','i','n','g','s',0
STR_STARTUP_FULL          DW 'S','t','a','r','t','u','p',' ','f','u','l','l','s','c','r','e','e','n',0
STR_FONT_SIZE             DW 'F','o','n','t',' ','S','i','z','e',':',0
STR_MODEL_PATH            DW 'M','o','d','e','l',' ','P','a','t','h',':',0
STR_BROWSE                DW 'B','r','o','w','s','e','.',0
STR_DEFAULT_MODEL         DW 'D','e','f','a','u','l','t',' ','M','o','d','e','l',':',0
STR_CHAT_MODEL            DW 'C','h','a','t',' ','M','o','d','e','l',':',0
STR_TEMPERATURE           DW 'T','e','m','p','e','r','a','t','u','r','e',':',0
STR_MAX_TOKENS            DW 'M','a','x',' ','T','o','k','e','n','s',':',0
STR_SYSTEM_PROMPT         DW 'S','y','s','t','e','m',' ','P','r','o','m','p','t',':',0
STR_API_KEY               DW 'A','P','I',' ','K','e','y',':',0
STR_ENCRYPTION            DW 'E','n','a','b','l','e',' ','E','n','c','r','y','p','t','i','o','n',0
STR_SECURE_STORAGE        DW 'S','e','c','u','r','e',' ','S','t','o','r','a','g','e',0

; Tab Titles
STR_TAB_GENERAL           DW 'G','e','n','e','r','a','l',0
STR_TAB_MODEL             DW 'M','o','d','e','l',0
STR_TAB_CHAT              DW 'A','I',' ','C','h','a','t',0
STR_TAB_SECURITY          DW 'S','e','c','u','r','i','t','y',0
STR_TAB_TRAINING          DW 'T','r','a','i','n','i','n','g',0
STR_TAB_CICD              DW 'C','I','/','C','D',0
STR_TAB_ENTERPRISE        DW 'E','n','t','e','r','p','r','i','s','e',0
STR_TAB_QUANTIZATION      DW 'Q','u','a','n','t','i','z','a','t','i','o','n',0

szQuantizationType        DB "Quantization Type:",0
szAutoSelectQuant         DB "Auto-Select Based on VRAM",0
szAvailableVRAM           DB "Available VRAM:",0
szVRAMInfo                DB "0 GB / 0 GB Free",0
szApplyQuantization       DB "Apply Quantization",0
szTrainingPath            DB "Training Path:",0
szCheckpointInterval      DB "Checkpoint Interval (hours):",0
szPipelineEnabled         DB "Enable CI/CD Pipeline",0
szGithubToken             DB "GitHub API Token:",0
szComplianceLogging       DB "Enable Compliance Logging",0
szTelemetryEnabled        DB "Enable Telemetry",0
szEnterpriseInfo          DB "Enterprise features require activation",0

; Phase 7 Quantization Tab Strings
STR_QUANT_TYPE            DW 'Q','u','a','n','t','i','z','a','t','i','o','n',' ','T','y','p','e',':',0
STR_QUANT_SELECT          DW 'S','e','l','e','c','t',' ','q','u','a','n','t','i','z','a','t','i','o','n',0
STR_AUTO_SELECT           DW 'A','u','t','o','-','S','e','l','e','c','t',' ','B','a','s','e','d',' ','o','n',' ','V','R','A','M',0
STR_VRAM_AVAILABLE        DW 'A','v','a','i','l','a','b','l','e',' ','V','R','A','M',':',0
STR_CURRENT_QUANT         DW 'C','u','r','r','e','n','t',' ','Q','u','a','n','t','i','z','a','t','i','o','n',':',0
STR_APPLY                 DW 'A','p','p','l','y',' ','Q','u','a','n','t','i','z','a','t','i','o','n',0

STR_TAB_QUANTIZATION      DW 'Q','u','a','n','t','i','z','a','t','i','o','n',0

.CODE

; ============================================================================
; CreateSettingsDialog: Create and show settings dialog
; Parameters:
;   rcx = parent HWND
;   rdx = settings_data pointer (optional)
; Returns:
;   rax = dialog result (IDOK=1, IDCANCEL=2)
; ============================================================================
CreateSettingsDialog PROC FRAME
    ; Save non-volatile registers
    push rbx
    push rsi
    push rdi
    .PUSHREG rbx
    .PUSHREG rsi
    .PUSHREG rdi
    
    ; Allocate stack space
    sub rsp, 40h
    .ALLOCSTACK 40h
    
    .ENDPROLOG
    
    mov rbx, rcx  ; parent_hwnd
    mov rsi, rdx  ; settings_data
    
    ; Allocate SETTINGS_DIALOG structure
    mov rcx, sizeof(SETTINGS_DIALOG)
    call malloc
    test rax, rax
    jz allocation_failed
    
    mov rdi, rax  ; rdi = settings_dialog
    
    ; Initialize structure
    mov [rdi+SETTINGS_DIALOG.settings_data], rsi
    mov [rdi+SETTINGS_DIALOG.is_dirty], 0
    
    ; Create modal dialog
    mov rcx, rbx  ; parent_hwnd
    mov rdx, "RawrXD Settings"
    mov r8d, SETTINGS_DIALOG_WIDTH
    mov r9d, SETTINGS_DIALOG_HEIGHT
    push rdi      ; user_data = SETTINGS_DIALOG pointer
    push offset OnSettingsInit  ; on_init
    push offset OnSettingsCommand  ; on_command
    call CreateModalDialog
    
    ; Save dialog result
    mov [rsp+20h], eax
    
    ; Free settings_dialog structure
    mov rcx, rdi
    call free
    
    ; Return result
    mov eax, [rsp+20h]
    jmp settings_dialog_complete
    
allocation_failed:
    xor eax, eax
    
settings_dialog_complete:
    ; Cleanup stack and return
    add rsp, 40h
    pop rdi
    pop rsi
    pop rbx
    ret
CreateSettingsDialog ENDP

    ; Create Max Tokens spinner
    mov rcx, rbx
    mov rdx, "Max Tokens:"
    mov r8d, 20
    mov r9d, 100
    push 100
    push 25
    push IDC_MAX_TOKENS
    call CreateSpinner
    
    ; Create System Prompt edit control
    mov rcx, rbx
    mov rdx, "System Prompt:"
    mov r8d, 20
    mov r9d, 140
    push 400
    push 100
    push IDC_SYSTEM_PROMPT
    call CreateEditControl
    
    mov rax, 1
    ret
CreateChatTabControls ENDP

    
     ; Cache settings_dialog on the window for command handlers
     mov rcx, rbx
     mov edx, GWLP_USERDATA
     mov r8, rdi
     call SetWindowLongPtrW
; ============================================================================
; CreateSecurityTabControls: Create controls for Security tab
; Parameters:
;   rcx = tab page HWND
;   rdx = SETTINGS_DIALOG pointer
; Returns:
;   rax = 1 if success
; ============================================================================
CreateSecurityTabControls PROC
    mov rbx, rcx  ; tab_hwnd
    mov rsi, rdx  ; settings_dialog
    
    ; Create API Key edit control
    mov rcx, rbx
    mov rdx, "API Key:"
    mov r8d, 20
    mov r9d, 20
    push 300
    push 25
    push IDC_API_KEY
    call CreateEditControl
    
    ; Create Encryption checkbox
    mov rcx, rbx
    mov rdx, "Enable Encryption"
    mov r8d, 20
    mov r9d, 60
    push 200
    push 25
    push IDC_ENCRYPTION
    call CreateCheckbox
    
    ; Create Secure Storage checkbox
    mov rcx, rbx
    mov rdx, "Secure Storage"
    mov r8d, 20
    mov r9d, 90
    push 200
    push 25
    push IDC_SECURE_STORAGE
    call CreateCheckbox
    
    mov rax, 1
    ret
CreateSecurityTabControls ENDP
; ============================================================================
; CreateTrainingTabControls: Create controls for Training tab
; Parameters:
;   rcx = tab page HWND
; Returns:
;   rax = 1 if success
; ============================================================================
CreateTrainingTabControls PROC
    push rbx
    sub rsp, 20h
    
    mov rbx, rcx  ; tab_hwnd
    
    ; Create Training Path label
    mov rcx, rbx
    lea rdx, szTrainingPath
    mov r8d, 20
    mov r9d, 25
    mov [rsp], 25
    mov [rsp+8], 100
    call CreateStaticText
    
    ; Create Training Path edit
    mov rcx, rbx
    xor rdx, rdx
    mov r8d, 120
    mov r9d, 20
    mov [rsp], IDC_MODEL_PATH  ; Reuse control ID
    mov [rsp+8], 25
    mov [rsp+10h], 300
    call CreateEditControl
    
    ; Create Checkpoint Interval label
    mov rcx, rbx
    lea rdx, szCheckpointInterval
    mov r8d, 20
    mov r9d, 65
    mov [rsp], 25
    mov [rsp+8], 150
    call CreateStaticText
    
    ; Create Checkpoint Interval spinner
    mov rcx, rbx
    xor rdx, rdx
    mov r8d, 170
    mov r9d, 60
    mov [rsp], IDC_TEMPERATURE
    mov [rsp+8], 25
    mov [rsp+10h], 80
    call CreateEditControl
    
    add rsp, 20h
    pop rbx
    mov rax, 1
    ret
CreateTrainingTabControls ENDP

; ============================================================================
; CreateCICDTabControls: Create controls for CI/CD tab
; Parameters:
;   rcx = tab page HWND
; Returns:
;   rax = 1 if success
; ============================================================================
CreateCICDTabControls PROC
    push rbx
    sub rsp, 20h
    
    mov rbx, rcx  ; tab_hwnd
    
    ; Create Pipeline Enabled checkbox
    mov rcx, rbx
    lea rdx, szPipelineEnabled
    mov r8d, 20
    mov r9d, 25
    mov [rsp], IDC_ENCRYPTION
    mov [rsp+8], 25
    mov [rsp+10h], 200
    call CreateCheckbox
    
    ; Create GitHub Token label
    mov rcx, rbx
    lea rdx, szGithubToken
    mov r8d, 20
    mov r9d, 65
    mov [rsp], 25
    mov [rsp+8], 100
    call CreateStaticText
    
    ; Create GitHub Token edit
    mov rcx, rbx
    xor rdx, rdx
    mov r8d, 120
    mov r9d, 60
    mov [rsp], IDC_API_KEY
    mov [rsp+8], 25
    mov [rsp+10h], 300
    call CreateEditControl
    
    add rsp, 20h
    pop rbx
    mov rax, 1
    ret
CreateCICDTabControls ENDP

; ============================================================================
; CreateEnterpriseTabControls: Create controls for Enterprise tab
; Parameters:
;   rcx = tab page HWND
; Returns:
;   rax = 1 if success
; ============================================================================
CreateEnterpriseTabControls PROC
    push rbx
    sub rsp, 20h
    
    mov rbx, rcx  ; tab_hwnd
    
    ; Create Compliance Logging checkbox
    mov rcx, rbx
    lea rdx, szComplianceLogging
    mov r8d, 20
    mov r9d, 25
    mov [rsp], IDC_ENCRYPTION
    mov [rsp+8], 25
    mov [rsp+10h], 250
    call CreateCheckbox
    
    ; Create Telemetry Enabled checkbox
    mov rcx, rbx
    lea rdx, szTelemetryEnabled
    mov r8d, 20
    mov r9d, 55
    mov [rsp], IDC_SECURE_STORAGE
    mov [rsp+8], 25
    mov [rsp+10h], 250
    call CreateCheckbox
    
    ; Create info label
    mov rcx, rbx
    lea rdx, szEnterpriseInfo
    mov r8d, 20
    mov r9d, 95
    mov [rsp], 200
    mov [rsp+8], 400
    call CreateStaticText
    
    add rsp, 20h
    pop rbx
    mov rax, 1
    ret
CreateEnterpriseTabControls ENDP

; ============================================================================
; CreateQuantizationTabControls: Create controls for Quantization tab (Phase 7)
; Parameters:
;   rcx = tab page HWND
;   rdx = SETTINGS_DIALOG pointer
; Returns:
;   rax = 1 if success
; ============================================================================
CreateQuantizationTabControls PROC USES rbx rsi
    mov rbx, rcx  ; tab_hwnd
    mov rsi, rdx  ; settings_dialog
    sub rsp, 40h

    ; Create "Quantization Type:" label
    mov rcx, rbx
    mov rdx, OFFSET szQuantizationType
    mov r8d, 20
    mov r9d, 20
    mov [rsp], 25
    mov [rsp+8], 150
    call CreateStaticText

    ; Create Quantization combo box
    mov rcx, rbx
    xor rdx, rdx
    mov r8d, 170
    mov r9d, 15
    mov [rsp], IDC_QUANT_COMBO
    mov [rsp+8], 25
    mov [rsp+10h], 250
    call CreateComboBox
    
    mov rcx, rax  ; combo handle
    call QuantizationControls_PopulateComboBox

    ; Create "Auto-Select" checkbox
    mov rcx, rbx
    mov rdx, OFFSET szAutoSelectQuant
    mov r8d, 20
    mov r9d, 50
    mov [rsp], 25
    mov [rsp+8], 250
    mov [rsp+10h], IDC_AUTO_SELECT_CHECK
    call CreateCheckbox

    ; Create "Available VRAM:" label
    mov rcx, rbx
    mov rdx, OFFSET szAvailableVRAM
    mov r8d, 20
    mov r9d, 80
    mov [rsp], 25
    mov [rsp+8], 100
    call CreateStaticText

    ; Create VRAM progress bar
    mov rcx, rbx
    xor rdx, rdx
    mov r8d, 120
    mov r9d, 75
    mov [rsp], IDC_VRAM_PROGRESS
    mov [rsp+8], 25
    mov [rsp+10h], 250
    call CreateProgressBar

    ; Create VRAM info label
    mov rcx, rbx
    mov rdx, OFFSET szVRAMInfo
    mov r8d, 120
    mov r9d, 100
    mov [rsp], 25
    mov [rsp+8], 250
    mov [rsp+10h], IDC_VRAM_LABEL
    call CreateStaticText

    ; Create "Apply Quantization" button
    mov rcx, rbx
    mov rdx, OFFSET szApplyQuantization
    mov r8d, 20
    mov r9d, 150
    mov [rsp], 25
    mov [rsp+8], 120
    mov [rsp+10h], IDC_APPLY_QUANT_BUTTON
    call CreateButton

    ; Initialize quantization state (Phase 7)
    call QuantizationControls_Create

    add rsp, 40h
    mov rax, 1
    ret
CreateQuantizationTabControls ENDP

; ============================================================================

; ============================================================================
; SaveSettingsFromUI: Save settings from UI controls to data structure
; Parameters:
;   rcx = SETTINGS_DIALOG pointer
; Returns:
;   rax = 1 if success
; ============================================================================
SaveSettingsFromUI PROC
    push rbx
    push rsi
    push rdi
    sub rsp, 20h
    
    mov rbx, rcx  ; settings_dialog
    
    ; Get settings data
    mov rsi, [rbx+SETTINGS_DIALOG.settings_data]
    test rsi, rsi
    jz save_complete
    
    mov rdi, [rbx+SETTINGS_DIALOG.hwnd]  ; dialog hwnd
    
    ; --- General Tab ---
    ; Auto-save checkbox
    mov rcx, rdi
    mov edx, IDC_AUTO_SAVE
    call IsDlgButtonChecked
    mov [rsi+SETTINGS_DATA.auto_save_enabled], al
    
    ; Startup fullscreen checkbox
    mov rcx, rdi
    mov edx, IDC_STARTUP_FULLSCREEN
    call IsDlgButtonChecked
    mov [rsi+SETTINGS_DATA.startup_fullscreen], al
    
    ; Font size spinner
    mov rcx, rdi
    mov edx, IDC_FONT_SIZE
    lea r8, [rsp+18h]  ; lpTranslated
    xor r9, r9  ; bSigned
    call GetDlgItemInt
    mov [rsi+SETTINGS_DATA.font_size], eax
    
    ; --- Chat Tab ---
    ; Temperature value
    mov rcx, rdi
    mov edx, IDC_TEMPERATURE
    lea r8, [rsp+18h]
    xor r9, r9
    call GetDlgItemInt
    mov [rsi+SETTINGS_DATA.temperature], eax
    
    ; Max tokens value
    mov rcx, rdi
    mov edx, IDC_MAX_TOKENS
    lea r8, [rsp+18h]
    xor r9, r9
    call GetDlgItemInt
    mov [rsi+SETTINGS_DATA.max_tokens], eax
    
    ; --- Security Tab ---
    ; Encryption checkbox
    mov rcx, rdi
    mov edx, IDC_ENCRYPTION
    call IsDlgButtonChecked
    mov [rsi+SETTINGS_DATA.encryption_enabled], al
    
    ; Secure storage checkbox
    mov rcx, rdi
    mov edx, IDC_SECURE_STORAGE
    call IsDlgButtonChecked
    mov [rsi+SETTINGS_DATA.secure_storage], al
    
    ; Save to registry
    mov rcx, rsi
    call SaveSettingsToRegistry
    
    ; Mark as clean
    mov [rbx+SETTINGS_DIALOG.is_dirty], 0
    
save_complete:
    add rsp, 20h
    pop rdi
    pop rsi
    pop rbx
    mov rax, 1
    ret
SaveSettingsFromUI ENDP

; ============================================================================
; Note: OnTabSelectionChanged is implemented below (duplicate removed)
; ============================================================================

; ============================================================================
; OnSettingsCommand: WM_COMMAND handler
; Parameters:
;   rcx = dialog HWND
;   rdx = control ID (LOWORD of wParam)
; Returns:
;   rax = 1 if handled
; ============================================================================
OnSettingsCommand PROC
    push rbx
    push rsi
    push rdi
    sub rsp, 20h
    
    mov rbx, rcx                  ; dialog HWND
    mov esi, edx                  ; control id
    
    ; Retrieve settings_dialog from window user data
    mov rcx, rbx
    mov edx, GWLP_USERDATA
    call GetWindowLongPtrW
    mov rdi, rax                  ; settings_dialog*
    test rdi, rdi
    jz command_done
    
    ; Handle OK button
    cmp esi, IDC_OK
    jne check_cancel
    
    mov rcx, rdi                  ; Save and close
    call SaveSettingsFromUI
    mov rcx, rbx
    call DestroyWindow
    mov rax, 1
    jmp command_done
    
check_cancel:
    cmp esi, IDC_CANCEL
    jne check_apply
    
    mov rcx, rbx                  ; Close without saving
    call DestroyWindow
    mov rax, 1
    jmp command_done
    
check_apply:
    cmp esi, IDC_APPLY
    jne check_quant_apply
    
    mov rcx, rdi                  ; Save but keep dialog
    call SaveSettingsFromUI
    mov rax, 1
    jmp command_done
    
check_quant_apply:
    cmp esi, IDC_APPLY_QUANT_BUTTON
    jne check_auto_select
    
    ; Get selected quantization type from combo box
    mov rcx, rbx
    mov edx, IDC_QUANT_COMBO
    mov r8d, WM_GETTEXT
    xor r9, r9
    call SendMessageW
    
    ; RAX contains selected index (0-9 for Q2_K through F32)
    mov ecx, eax  ; quantization_type
    
    ; Apply quantization
    call QuantizationControls_ApplyQuantization
    
    ; Update VRAM display and save settings
    call QuantizationControls_UpdateVRAMDisplay
    mov rcx, rdi
    call SaveSettingsFromUI
    mov rax, 1
    jmp command_done

check_auto_select:
    cmp esi, IDC_AUTO_SELECT_CHECK
    jne check_other_controls
    
    ; Get checkbox state
    mov rcx, rbx
    mov edx, IDC_AUTO_SELECT_CHECK
    call IsDlgButtonChecked
    
    ; If checked, get recommended quantization and auto-select
    test eax, eax
    jz other_controls_continue
    
    ; Call GetRecommendedQuantization - it returns type in RAX
    call QuantizationControls_GetRecommendedQuantization
    mov ecx, eax
    
    ; Update combo box with recommended type
    mov rcx, rbx
    mov edx, IDC_QUANT_COMBO
    mov r8d, CB_SETCURSEL
    mov r9d, ecx  ; recommended_type
    call SendMessageW
    
    ; Update VRAM display
    call QuantizationControls_UpdateVRAMDisplay
    
other_controls_continue:
    mov rax, 1
    jmp command_done
    
check_other_controls:
    ; Mark dirty and validate/normalize input
    mov byte ptr [rdi+SETTINGS_DIALOG.is_dirty], 1
    mov rcx, rbx
    mov edx, esi
    call HandleControlChange
    mov rax, 1
    
command_done:
    add rsp, 20h
    pop rdi
    pop rsi
    pop rbx
    ret
OnSettingsCommand ENDP

; ============================================================================
; OnTabSelectionChanged: Tab selection change handler
; Parameters:
;   rcx = TAB_CONTROL pointer
;   rdx = page index
; Returns:
;   rax = 1 if handled
; ============================================================================
OnTabSelectionChanged PROC
    push rbx
    push rsi
    push rdi
    mov rbx, rcx  ; tab_control
    mov esi, edx  ; page_index
    
    ; Loop through all pages and hide them
    xor edi, edi
hide_loop:
    mov eax, [rbx+TAB_CONTROL.page_count]
    cmp edi, eax
    jge show_active
    
    ; Get page structure
    mov rax, [rbx+TAB_CONTROL.pages]
    mov r8, [rax+rdi*8]
    test r8, r8
    jz next_hide
    
    mov rcx, [r8+TAB_PAGE.hwnd]
    test rcx, rcx
    jz next_hide
    
    ; Hide window
    mov rdx, SW_HIDE
    call ShowWindow
    
next_hide:
    inc edi
    jmp hide_loop
    
show_active:
    ; Show active page
    mov rax, [rbx+TAB_CONTROL.pages]
    mov r8, [rax+rsi*8]
    test r8, r8
    jz done
    
    mov rcx, [r8+TAB_PAGE.hwnd]
    test rcx, rcx
    jz done
    
    mov rdx, SW_SHOW
    call ShowWindow
    
done:
    pop rdi
    pop rsi
    pop rbx
    mov rax, 1
    ret
OnTabSelectionChanged ENDP

; ============================================================================
; CreateGeneralTabControls: Create controls for General tab
; Parameters:
;   rcx = parent HWND
; Returns: rax = 1 if success
; ============================================================================
CreateGeneralTabControls PROC
    push rbx
    mov rbx, rcx ; parent_hwnd
    
    ; Auto-save checkbox
    mov rcx, rbx
    lea rdx, STR_AUTO_SAVE
    mov r8d, 20
    mov r9d, 20
    push IDC_AUTO_SAVE
    push 25
    push 200
    call CreateCheckbox
    add rsp, 18h
    
    ; Startup fullscreen checkbox
    mov rcx, rbx
    lea rdx, STR_STARTUP_FULL
    mov r8d, 20
    mov r9d, 50
    push IDC_STARTUP_FULLSCREEN
    push 25
    push 200
    call CreateCheckbox
    add rsp, 18h
    
    ; Font size label
    mov rcx, rbx
    lea rdx, STR_FONT_SIZE
    mov r8d, 20
    mov r9d, 85
    push 25
    push 100
    call CreateStaticText
    add rsp, 10h
    
    ; Font size edit (spinner buddy)
    mov rcx, rbx
    xor rdx, rdx
    mov r8d, 120
    mov r9d, 80
    push IDC_FONT_SIZE
    push 25
    push 50
    call CreateEditControl
    add rsp, 18h
    
    pop rbx
    mov rax, 1
    ret
CreateGeneralTabControls ENDP

; ============================================================================
; CreateModelTabControls: Create controls for Model tab
; Parameters:
;   rcx = parent HWND
; Returns: rax = 1 if success
; ============================================================================
CreateModelTabControls PROC
    push rbx
    mov rbx, rcx
    
    ; Model path label
    mov rcx, rbx
    lea rdx, STR_MODEL_PATH
    mov r8d, 20
    mov r9d, 25
    push 25
    push 100
    call CreateStaticText
    add rsp, 10h
    
    ; Model path edit
    mov rcx, rbx
    xor rdx, rdx
    mov r8d, 120
    mov r9d, 20
    push IDC_MODEL_PATH
    push 25
    push 300
    call CreateEditControl
    add rsp, 18h
    
    ; Browse button
    mov rcx, rbx
    lea rdx, STR_BROWSE
    mov r8d, 430
    mov r9d, 20
    push IDC_BROWSE_MODEL
    push 25
    push 80
    call CreateButton
    add rsp, 18h
    
    ; Default model label
    mov rcx, rbx
    lea rdx, STR_DEFAULT_MODEL
    mov r8d, 20
    mov r9d, 65
    push 25
    push 100
    call CreateStaticText
    add rsp, 10h
    
    ; Default model dropdown
    mov rcx, rbx
    xor rdx, rdx
    mov r8d, 120
    mov r9d, 60
    push IDC_DEFAULT_MODEL
    push 200 ; height of dropdown list
    push 200 ; width
    call CreateDropdown
    add rsp, 18h
    
    pop rbx
    mov rax, 1
    ret
CreateModelTabControls ENDP

; ============================================================================
; CreateChatTabControls: Create controls for Chat tab
; Parameters:
;   rcx = parent HWND
; Returns: rax = 1 if success
; ============================================================================
CreateChatTabControls PROC
    push rbx
    mov rbx, rcx
    
    ; Chat model label
    mov rcx, rbx
    lea rdx, STR_CHAT_MODEL
    mov r8d, 20
    mov r9d, 25
    push 25
    push 100
    call CreateStaticText
    add rsp, 10h
    
    ; Chat model dropdown
    mov rcx, rbx
    xor rdx, rdx
    mov r8d, 120
    mov r9d, 20
    push IDC_CHAT_MODEL
    push 200
    push 200
    call CreateDropdown
    add rsp, 18h
    
    ; Temperature label
    mov rcx, rbx
    lea rdx, STR_TEMPERATURE
    mov r8d, 20
    mov r9d, 65
    push 25
    push 100
    call CreateStaticText
    add rsp, 10h
    
    ; Temperature edit
    mov rcx, rbx
    xor rdx, rdx
    mov r8d, 120
    mov r9d, 60
    push IDC_TEMPERATURE
    push 25
    push 50
    call CreateEditControl
    add rsp, 18h
    
    ; Max tokens label
    mov rcx, rbx
    lea rdx, STR_MAX_TOKENS
    mov r8d, 20
    mov r9d, 105
    push 25
    push 100
    call CreateStaticText
    add rsp, 10h
    
    ; Max tokens edit
    mov rcx, rbx
    xor rdx, rdx
    mov r8d, 120
    mov r9d, 100
    push IDC_MAX_TOKENS
    push 25
    push 80
    call CreateEditControl
    add rsp, 18h
    
    ; System prompt label
    mov rcx, rbx
    lea rdx, STR_SYSTEM_PROMPT
    mov r8d, 20
    mov r9d, 145
    push 25
    push 100
    call CreateStaticText
    add rsp, 10h
    
    ; System prompt edit (multiline)
    mov rcx, rbx
    xor rdx, rdx
    mov r8d, 20
    mov r9d, 170
    push IDC_SYSTEM_PROMPT
    push 150
    push 500
    call CreateEditControl
    add rsp, 18h
    
    pop rbx
    mov rax, 1
    ret
CreateChatTabControls ENDP

; ============================================================================
; CreateSecurityTabControls: Create controls for Security tab
; Parameters:
;   rcx = parent HWND
; Returns: rax = 1 if success
; ============================================================================
CreateSecurityTabControls PROC
    push rbx
    mov rbx, rcx
    
    ; API key label
    mov rcx, rbx
    lea rdx, STR_API_KEY
    mov r8d, 20
    mov r9d, 25
    push 25
    push 100
    call CreateStaticText
    add rsp, 10h
    
    ; API key edit
    mov rcx, rbx
    xor rdx, rdx
    mov r8d, 120
    mov r9d, 20
    push IDC_API_KEY
    push 25
    push 300
    call CreateEditControl
    add rsp, 18h
    
    ; Encryption checkbox
    mov rcx, rbx
    lea rdx, STR_ENCRYPTION
    mov r8d, 20
    mov r9d, 60
    push IDC_ENCRYPTION
    push 25
    push 200
    call CreateCheckbox
    add rsp, 18h
    
    ; Secure storage checkbox
    mov rcx, rbx
    lea rdx, STR_SECURE_STORAGE
    mov r8d, 20
    mov r9d, 90
    push IDC_SECURE_STORAGE
    push 25
    push 200
    call CreateCheckbox
    add rsp, 18h
    
    pop rbx
    mov rax, 1
    ret
CreateSecurityTabControls ENDP

; ============================================================================
; LoadSettingsToUI: Load settings data to UI controls
; Parameters:
;   rcx = SETTINGS_DIALOG pointer
; Returns: rax = 1 if success
; ============================================================================
LoadSettingsToUI PROC
    push rbx
    push rsi
    push rdi
    mov rbx, rcx  ; settings_dialog
    
    ; Get settings data
    mov rsi, [rbx+SETTINGS_DIALOG.settings_data]
    test rsi, rsi
    jz no_settings_data
    
    mov rdi, [rbx+SETTINGS_DIALOG.hwnd] ; rdi = dialog_hwnd
    
    ; --- General Tab ---
    ; Auto-save checkbox
    mov rcx, rdi
    mov edx, IDC_AUTO_SAVE
    movzx r8d, [rsi+SETTINGS_DATA.auto_save_enabled]
    call CheckDlgButton
    
    ; Startup fullscreen checkbox
    mov rcx, rdi
    mov edx, IDC_STARTUP_FULLSCREEN
    movzx r8d, [rsi+SETTINGS_DATA.startup_fullscreen]
    call CheckDlgButton
    
    ; Font size
    mov rcx, rdi
    mov edx, IDC_FONT_SIZE
    mov r8d, [rsi+SETTINGS_DATA.font_size]
    push 0 ; signed
    call SetDlgItemInt
    
    ; --- Model Tab ---
    ; Model path
    mov rcx, rdi
    mov edx, IDC_MODEL_PATH
    mov r8, [rsi+SETTINGS_DATA.model_path]
    test r8, r8
    jz next_model
    call SetDlgItemTextW
    
next_model:
    ; ... other fields ...
    
no_settings_data:
    pop rdi
    pop rsi
    pop rbx
    mov rax, 1
    ret
LoadSettingsToUI ENDP

; ============================================================================
; SaveSettingsFromUI: Save UI controls to settings data
; Parameters:
;   rcx = SETTINGS_DIALOG pointer
; Returns: rax = 1 if success
; ============================================================================
SaveSettingsFromUI PROC
    push rbx
    push rsi
    push rdi
    mov rbx, rcx  ; settings_dialog
    
    ; Get settings data
    mov rsi, [rbx+SETTINGS_DIALOG.settings_data]
    test rsi, rsi
    jz no_save_data
    
    mov rdi, [rbx+SETTINGS_DIALOG.hwnd] ; rdi = dialog_hwnd
    
    ; --- General Tab ---
    ; Auto-save checkbox
    mov rcx, rdi
    mov edx, IDC_AUTO_SAVE
    call IsDlgButtonChecked
    mov [rsi+SETTINGS_DATA.auto_save_enabled], al
    
    ; Startup fullscreen checkbox
    mov rcx, rdi
    mov edx, IDC_STARTUP_FULLSCREEN
    call IsDlgButtonChecked
    mov [rsi+SETTINGS_DATA.startup_fullscreen], al
    
    ; Font size
    mov rcx, rdi
    mov edx, IDC_FONT_SIZE
    xor r8, r8 ; lpTranslated
    xor r9, r9 ; bSigned
    call GetDlgItemInt
    mov [rsi+SETTINGS_DATA.font_size], eax
    
    ; --- Model Tab ---
    ; Model path
    mov rax, [rsi+SETTINGS_DATA.model_path]
    test rax, rax
    jnz model_buf_ready
    mov rcx, SETTINGS_PATH_CHARS*2
    call malloc
    mov [rsi+SETTINGS_DATA.model_path], rax
model_buf_ready:
    mov rcx, rdi
    mov edx, IDC_MODEL_PATH
    mov r8, [rsi+SETTINGS_DATA.model_path]
    mov r9d, SETTINGS_PATH_CHARS
    call GetDlgItemTextW

    ; Default model
    mov rax, [rsi+SETTINGS_DATA.default_model]
    test rax, rax
    jnz default_buf_ready
    mov rcx, SETTINGS_PATH_CHARS*2
    call malloc
    mov [rsi+SETTINGS_DATA.default_model], rax
default_buf_ready:
    mov rcx, rdi
    mov edx, IDC_DEFAULT_MODEL
    mov r8, [rsi+SETTINGS_DATA.default_model]
    mov r9d, SETTINGS_PATH_CHARS
    call GetDlgItemTextW

    ; --- Chat Tab ---
    ; Chat model
    mov rax, [rsi+SETTINGS_DATA.chat_model]
    test rax, rax
    jnz chat_buf_ready
    mov rcx, SETTINGS_PATH_CHARS*2
    call malloc
    mov [rsi+SETTINGS_DATA.chat_model], rax
chat_buf_ready:
    mov rcx, rdi
    mov edx, IDC_CHAT_MODEL
    mov r8, [rsi+SETTINGS_DATA.chat_model]
    mov r9d, SETTINGS_PATH_CHARS
    call GetDlgItemTextW

    ; Temperature (0-200 integer maps to 0.0-2.0)
    mov rcx, rdi
    mov edx, IDC_TEMPERATURE
    xor r8, r8
    xor r9, r9
    call GetDlgItemInt
    mov [rsi+SETTINGS_DATA.temperature], eax

    ; Max tokens
    mov rcx, rdi
    mov edx, IDC_MAX_TOKENS
    xor r8, r8
    xor r9, r9
    call GetDlgItemInt
    mov [rsi+SETTINGS_DATA.max_tokens], eax

    ; System prompt
    mov rax, [rsi+SETTINGS_DATA.system_prompt]
    test rax, rax
    jnz prompt_buf_ready
    mov rcx, SETTINGS_PROMPT_CHARS*2
    call malloc
    mov [rsi+SETTINGS_DATA.system_prompt], rax
prompt_buf_ready:
    mov rcx, rdi
    mov edx, IDC_SYSTEM_PROMPT
    mov r8, [rsi+SETTINGS_DATA.system_prompt]
    mov r9d, SETTINGS_PROMPT_CHARS
    call GetDlgItemTextW

    ; API key (security tab)
    mov rax, [rsi+SETTINGS_DATA.api_key]
    test rax, rax
    jnz apikey_buf_ready
    mov rcx, SETTINGS_APIKEY_CHARS*2
    call malloc
    mov [rsi+SETTINGS_DATA.api_key], rax
apikey_buf_ready:
    mov rcx, rdi
    mov edx, IDC_API_KEY
    mov r8, [rsi+SETTINGS_DATA.api_key]
    mov r9d, SETTINGS_APIKEY_CHARS
    call GetDlgItemTextW

    ; Encryption toggle
    mov rcx, rdi
    mov edx, IDC_ENCRYPTION
    call IsDlgButtonChecked
    mov [rsi+SETTINGS_DATA.encryption_enabled], al

    ; Secure storage toggle
    mov rcx, rdi
    mov edx, IDC_SECURE_STORAGE
    call IsDlgButtonChecked
    mov [rsi+SETTINGS_DATA.secure_storage], al
    
    ; Save to registry
    mov rcx, rsi
    call SaveSettingsToRegistry
    
no_save_data:
    pop rdi
    pop rsi
    pop rbx
    mov rax, 1
    ret
SaveSettingsFromUI ENDP

; ============================================================================
; HandleControlChange: Handle control value changes
; Parameters:
;   rcx = dialog HWND
;   rdx = control ID
; Returns: rax = 1 if handled
; ============================================================================
HandleControlChange PROC
    push rbx
    push rsi
    sub rsp, 20h
    
    mov rbx, rcx  ; dialog_hwnd
    mov esi, edx  ; control_id
    
    ; Validate input based on control type
    cmp esi, IDC_FONT_SIZE
    jne check_temperature
    
    ; Validate font size (8-72)
    mov rcx, rbx
    mov edx, IDC_FONT_SIZE
    lea r8, [rsp+18h]
    xor r9, r9
    call GetDlgItemInt
    
    cmp eax, 8
    jl font_invalid
    cmp eax, 72
    jg font_invalid
    jmp check_temperature
    
font_invalid:
    ; Set to 10 if invalid
    mov rcx, rbx
    mov edx, IDC_FONT_SIZE
    mov r8d, 10
    xor r9, r9
    call SetDlgItemInt
    
check_temperature:
    cmp esi, IDC_TEMPERATURE
    jne check_tokens
    
    ; Validate temperature (0-200 as integer, represents 0.0-2.0)
    mov rcx, rbx
    mov edx, IDC_TEMPERATURE
    lea r8, [rsp+18h]
    xor r9, r9
    call GetDlgItemInt
    
    cmp eax, 0
    jl temp_invalid
    cmp eax, 200
    jg temp_invalid
    jmp check_tokens
    
temp_invalid:
    mov rcx, rbx
    mov edx, IDC_TEMPERATURE
    mov r8d, 100  ; Default 1.0
    xor r9, r9
    call SetDlgItemInt
    
check_tokens:
    cmp esi, IDC_MAX_TOKENS
    jne done_validation
    
    ; Validate max tokens (1-4096)
    mov rcx, rbx
    mov edx, IDC_MAX_TOKENS
    lea r8, [rsp+18h]
    xor r9, r9
    call GetDlgItemInt
    
    cmp eax, 1
    jl tokens_invalid
    cmp eax, 4096
    jg tokens_invalid
    jmp done_validation
    
tokens_invalid:
    mov rcx, rbx
    mov edx, IDC_MAX_TOKENS
    mov r8d, 2048  ; Default
    xor r9, r9
    call SetDlgItemInt
    
done_validation:
    add rsp, 20h
    pop rsi
    pop rbx
    mov rax, 1
    ret
HandleControlChange ENDP

; ============================================================================
; Control Creation Helper Functions
; ============================================================================

; CreateCheckbox: Create a checkbox control
; rcx = parent, rdx = text, r8d = x, r9d = y, [rsp+28h] = w, [rsp+30h] = h, [rsp+38h] = id
CreateCheckbox PROC
    sub rsp, 68h
    mov rax, [rsp+90h] ; id
    mov [rsp+58h], rax ; lpParam
    mov rax, [rsp+68h] ; hInstance (can be 0 for controls)
    mov [rsp+50h], rax
    mov rax, [rsp+90h] ; id (hMenu)
    mov [rsp+48h], rax
    mov [rsp+40h], rcx ; hWndParent
    mov eax, [rsp+88h] ; height
    mov [rsp+38h], eax
    mov eax, [rsp+80h] ; width
    mov [rsp+30h], eax
    mov [rsp+28h], r9d ; y
    mov [rsp+20h], r8d ; x
    mov r9d, WS_CHILD or WS_VISIBLE or BS_AUTOCHECKBOX
    mov r8, rdx ; lpWindowName
    lea rdx, WC_BUTTON_W
    xor rcx, rcx ; dwExStyle
    call CreateWindowExW
    add rsp, 68h
    ret
CreateCheckbox ENDP

; CreateButton: Create a push button
; rcx = parent, rdx = text, r8d = x, r9d = y, [rsp+28h] = w, [rsp+30h] = h, [rsp+38h] = id
CreateButton PROC
    sub rsp, 68h
    mov rax, [rsp+90h] ; id
    mov [rsp+58h], rax
    xor rax, rax
    mov [rsp+50h], rax
    mov rax, [rsp+90h] ; id
    mov [rsp+48h], rax
    mov [rsp+40h], rcx
    mov eax, [rsp+88h] ; height
    mov [rsp+38h], eax
    mov eax, [rsp+80h] ; width
    mov [rsp+30h], eax
    mov [rsp+28h], r9d
    mov [rsp+20h], r8d
    mov r9d, WS_CHILD or WS_VISIBLE or BS_PUSHBUTTON
    mov r8, rdx
    lea rdx, WC_BUTTON_W
    xor rcx, rcx
    call CreateWindowExW
    add rsp, 68h
    ret
CreateButton ENDP

; CreateEditControl: Create an edit control
; rcx = parent, rdx = text, r8d = x, r9d = y, [rsp+28h] = w, [rsp+30h] = h, [rsp+38h] = id
CreateEditControl PROC
    sub rsp, 68h
    mov rax, [rsp+90h] ; id
    mov [rsp+58h], rax
    xor rax, rax
    mov [rsp+50h], rax
    mov rax, [rsp+90h] ; id
    mov [rsp+48h], rax
    mov [rsp+40h], rcx
    mov eax, [rsp+88h] ; height
    mov [rsp+38h], eax
    mov eax, [rsp+80h] ; width
    mov [rsp+30h], eax
    mov [rsp+28h], r9d
    mov [rsp+20h], r8d
    mov r9d, WS_CHILD or WS_VISIBLE or WS_BORDER or ES_AUTOHSCROLL
    mov r8, rdx
    lea rdx, WC_EDIT_W
    xor rcx, rcx
    call CreateWindowExW
    add rsp, 68h
    ret
CreateEditControl ENDP

; CreateStaticText: Create a static text label
; rcx = parent, rdx = text, r8d = x, r9d = y, [rsp+28h] = w, [rsp+30h] = h
CreateStaticText PROC
    sub rsp, 68h
    xor rax, rax
    mov [rsp+58h], rax
    mov [rsp+50h], rax
    mov [rsp+48h], rax
    mov [rsp+40h], rcx
    mov eax, [rsp+88h] ; height
    mov [rsp+38h], eax
    mov eax, [rsp+80h] ; width
    mov [rsp+30h], eax
    mov [rsp+28h], r9d
    mov [rsp+20h], r8d
    mov r9d, WS_CHILD or WS_VISIBLE or SS_LEFT
    mov r8, rdx
    lea rdx, WC_STATIC_W
    xor rcx, rcx
    call CreateWindowExW
    add rsp, 68h
    ret
CreateStaticText ENDP

; CreateDropdown: Create a combobox
; rcx = parent, rdx = text, r8d = x, r9d = y, [rsp+28h] = w, [rsp+30h] = h, [rsp+38h] = id
CreateDropdown PROC
    sub rsp, 68h
    mov rax, [rsp+90h] ; id
    mov [rsp+58h], rax
    xor rax, rax
    mov [rsp+50h], rax
    mov rax, [rsp+90h] ; id
    mov [rsp+48h], rax
    mov [rsp+40h], rcx
    mov eax, [rsp+88h] ; height
    mov [rsp+38h], eax
    mov eax, [rsp+80h] ; width
    mov [rsp+30h], eax
    mov [rsp+28h], r9d
    mov [rsp+20h], r8d
    mov r9d, WS_CHILD or WS_VISIBLE or CBS_DROPDOWNLIST
    mov r8, rdx
    lea rdx, WC_COMBOBOX_W
    xor rcx, rcx
    call CreateWindowExW
    add rsp, 68h
    ret
CreateDropdown ENDP

; CreateComboBox: Alias for CreateDropdown (Phase 7 quantization)
; rcx = parent, rdx = text, r8d = x, r9d = y, [rsp+28h] = w, [rsp+30h] = h, [rsp+38h] = id
CreateComboBox PROC
    jmp CreateDropdown
CreateComboBox ENDP

; CreateProgressBar: Create a progress bar control
; rcx = parent, rdx = text, r8d = x, r9d = y, [rsp+28h] = w, [rsp+30h] = h, [rsp+38h] = id
CreateProgressBar PROC
    sub rsp, 68h
    mov rax, [rsp+90h] ; id
    mov [rsp+58h], rax
    xor rax, rax
    mov [rsp+50h], rax
    mov rax, [rsp+90h] ; id
    mov [rsp+48h], rax
    mov [rsp+40h], rcx
    mov eax, [rsp+88h] ; height
    mov [rsp+38h], eax
    mov eax, [rsp+80h] ; width
    mov [rsp+30h], eax
    mov [rsp+28h], r9d
    mov [rsp+20h], r8d
    mov r9d, WS_CHILD or WS_VISIBLE or PBS_SMOOTH
    mov r8, 0  ; no text
    lea rdx, WC_PROGRESS    ; "msctls_progress32"
    xor rcx, rcx
    call CreateWindowExW
    add rsp, 68h
    ret
CreateProgressBar ENDP

; ============================================================================
; CreateQuantizationTabControls: Create controls for Quantization tab (Phase 7)
; Parameters:
;   rcx = parent HWND
; Returns: rax = 1 if success
; ============================================================================
CreateQuantizationTabControls PROC
    push rbx rdi
    sub rsp, 30h
    
    mov rbx, rcx  ; tab_hwnd
    
    ; Initialize quantization controls module
    call QuantizationControls_Create
    mov rdi, rax  ; save quantization state pointer
    
    ; Create Quantization Type label
    mov rcx, rbx
    lea rdx, STR_QUANT_TYPE
    mov r8d, 20
    mov r9d, 25
    mov [rsp], 25
    mov [rsp+8], 150
    call CreateStaticText
    add rsp, 10h
    
    ; Create Quantization Type combo box
    mov rcx, rbx
    lea rdx, STR_QUANT_SELECT
    mov r8d, 170
    mov r9d, 20
    mov [rsp], 150           ; width
    mov [rsp+8], 25          ; height
    mov [rsp+10h], IDC_QUANT_COMBO
    call CreateComboBox
    add rsp, 18h
    
    ; Populate combo box
    mov rcx, rax            ; combo box handle
    mov rdx, rdi            ; quantization state
    call QuantizationControls_PopulateComboBox
    
    ; Create Auto-Select checkbox
    mov rcx, rbx
    lea rdx, STR_AUTO_SELECT
    mov r8d, 20
    mov r9d, 65
    mov [rsp], 25
    mov [rsp+8], 250
    call CreateCheckbox
    add rsp, 10h
    
    ; Create VRAM Available label
    mov rcx, rbx
    lea rdx, STR_VRAM_AVAILABLE
    mov r8d, 20
    mov r9d, 105
    mov [rsp], 25
    mov [rsp+8], 200
    call CreateStaticText
    add rsp, 10h
    
    ; Create VRAM Progress bar
    mov rcx, rbx
    xor rdx, rdx
    mov r8d, 170
    mov r9d, 100
    mov [rsp], 150           ; width
    mov [rsp+8], 20          ; height
    mov [rsp+10h], IDC_VRAM_PROGRESS
    call CreateProgressBar
    add rsp, 18h
    
    ; Create Current Quantization label
    mov rcx, rbx
    lea rdx, STR_CURRENT_QUANT
    mov r8d, 20
    mov r9d, 145
    mov [rsp], 25
    mov [rsp+8], 200
    call CreateStaticText
    add rsp, 10h
    
    ; Create Apply button
    mov rcx, rbx
    lea rdx, STR_APPLY
    mov r8d, 250
    mov r9d, 180
    mov [rsp], 100           ; width
    mov [rsp+8], 25          ; height
    mov [rsp+10h], IDC_APPLY_QUANT_BUTTON
    call CreateButton
    add rsp, 18h
    
    add rsp, 30h
    pop rdi rbx
    mov rax, 1
    ret
CreateQuantizationTabControls ENDP

; ============================================================================
; CreateTrainingTabControls: Create controls for Training tab
; Parameters:
;   rcx = parent HWND
; Returns: rax = 1 if success
; ============================================================================
CreateTrainingTabControls PROC
    push rbx
    sub rsp, 20h
    
    mov rbx, rcx  ; tab_hwnd
    
    ; Create Training Path label
    mov rcx, rbx
    lea rdx, STR_TRAINING_PATH
    mov r8d, 20
    mov r9d, 25
    mov [rsp], 25
    mov [rsp+8], 100
    call CreateStaticText
    add rsp, 10h
    
    ; Create Training Path edit
    mov rcx, rbx
    xor rdx, rdx
    mov r8d, 120
    mov r9d, 20
    mov [rsp], 1017  ; IDC for training path
    mov [rsp+8], 25
    mov [rsp+10h], 300
    call CreateEditControl
    add rsp, 18h
    
    ; Create Checkpoint Interval label
    mov rcx, rbx
    lea rdx, STR_CHECKPOINT_INTERVAL
    mov r8d, 20
    mov r9d, 65
    mov [rsp], 25
    mov [rsp+8], 150
    call CreateStaticText
    add rsp, 10h
    
    ; Create Checkpoint Interval spinner
    mov rcx, rbx
    xor rdx, rdx
    mov r8d, 170
    mov r9d, 60
    mov [rsp], 1018
    mov [rsp+8], 25
    mov [rsp+10h], 80
    call CreateEditControl
    add rsp, 18h
    
    add rsp, 20h
    pop rbx
    mov rax, 1
    ret
CreateTrainingTabControls ENDP

; ============================================================================
; CreateCICDTabControls: Create controls for CI/CD tab
; Parameters:
;   rcx = parent HWND
; Returns: rax = 1 if success
; ============================================================================
CreateCICDTabControls PROC
    push rbx
    sub rsp, 20h
    
    mov rbx, rcx  ; tab_hwnd
    
    ; Create Pipeline Enabled checkbox
    mov rcx, rbx
    lea rdx, STR_PIPELINE_ENABLED
    mov r8d, 20
    mov r9d, 25
    mov [rsp], 1019  ; IDC for pipeline enabled
    mov [rsp+8], 25
    mov [rsp+10h], 200
    call CreateCheckbox
    add rsp, 18h
    
    ; Create GitHub Token label
    mov rcx, rbx
    lea rdx, STR_GITHUB_TOKEN
    mov r8d, 20
    mov r9d, 65
    mov [rsp], 25
    mov [rsp+8], 100
    call CreateStaticText
    add rsp, 10h
    
    ; Create GitHub Token edit
    mov rcx, rbx
    xor rdx, rdx
    mov r8d, 120
    mov r9d, 60
    mov [rsp], 1020  ; IDC for github token
    mov [rsp+8], 25
    mov [rsp+10h], 300
    call CreateEditControl
    add rsp, 18h
    
    add rsp, 20h
    pop rbx
    mov rax, 1
    ret
CreateCICDTabControls ENDP

; ============================================================================
; CreateEnterpriseTabControls: Create controls for Enterprise tab
; Parameters:
;   rcx = parent HWND
; Returns: rax = 1 if success
; ============================================================================
CreateEnterpriseTabControls PROC
    push rbx
    sub rsp, 20h
    
    mov rbx, rcx  ; tab_hwnd
    
    ; Create Compliance Logging checkbox
    mov rcx, rbx
    lea rdx, STR_COMPLIANCE_LOGGING
    mov r8d, 20
    mov r9d, 25
    mov [rsp], 1021  ; IDC for compliance
    mov [rsp+8], 25
    mov [rsp+10h], 250
    call CreateCheckbox
    add rsp, 18h
    
    ; Create Telemetry Enabled checkbox
    mov rcx, rbx
    lea rdx, STR_TELEMETRY_ENABLED
    mov r8d, 20
    mov r9d, 55
    mov [rsp], 1022  ; IDC for telemetry
    mov [rsp+8], 25
    mov [rsp+10h], 250
    call CreateCheckbox
    add rsp, 18h
    
    ; Create info label
    mov rcx, rbx
    lea rdx, STR_ENTERPRISE_INFO
    mov r8d, 20
    mov r9d, 95
    mov [rsp], 200
    mov [rsp+8], 400
    call CreateStaticText
    add rsp, 10h
    
    add rsp, 20h
    pop rbx
    mov rax, 1
    ret
CreateEnterpriseTabControls ENDP

; ============================================================================
; Registry Persistence Functions
; ============================================================================

SaveSettingsToRegistry PROC
    ; Implementation in registry_persistence.asm
    mov rax, 1
    ret
SaveSettingsToRegistry ENDP

LoadSettingsFromRegistry PROC
    ; Implementation in registry_persistence.asm
    mov rax, 1
    ret
LoadSettingsFromRegistry ENDP

END