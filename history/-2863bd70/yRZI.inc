; ============================================================================
; Settings Dialog Include File
; ============================================================================
; Constants and structures for qt6_settings_dialog.asm
; ============================================================================

; Settings Dialog Functions
EXTERN CreateSettingsDialog:PROC
EXTERN ShowSettingsDialog:PROC
EXTERN HideSettingsDialog:PROC
EXTERN DestroySettingsDialog:PROC
EXTERN LoadSettingsFromRegistry:PROC
EXTERN SaveSettingsToRegistry:PROC
EXTERN OnSettingsInit:PROC
EXTERN OnSettingsCommand:PROC
EXTERN OnSettingsClose:PROC

; Settings Structures
SETTINGS_DATA STRUCT
    ; General Tab
    auto_save_enabled      BYTE ?
    startup_fullscreen     BYTE ?
    font_size              DWORD ?
    
    ; Model Tab
    model_path             QWORD ?
    model_name             QWORD ?
    default_model          QWORD ?
    
    ; AI Chat Tab
    chat_model             QWORD ?
    temperature            DWORD ? ; Using DWORD for fixed-point or integer representation
    max_tokens             DWORD ?
    system_prompt          QWORD ?
    
    ; Security Tab
    api_key                QWORD ?
    encryption_enabled     BYTE ?
    secure_storage         BYTE ?
    
    ; Training Tab (optional)
    training_path          QWORD ?
    checkpoint_interval    DWORD ?
    batch_size             DWORD ?
    
    ; CI/CD Tab (optional)
    pipeline_enabled       BYTE ?
    github_token           QWORD ?
    
    ; Enterprise Tab (optional)
    compliance_logging     BYTE ?
    telemetry_enabled      BYTE ?
    
    padding                BYTE 100 DUP(?)
SETTINGS_DATA ENDS

SETTINGS_DIALOG STRUCT
    hwnd            QWORD ?
    tab_control     QWORD ?
    general_tab     QWORD ?
    model_tab       QWORD ?
    chat_tab        QWORD ?
    security_tab    QWORD ?
    training_tab    QWORD ?
    cicd_tab        QWORD ?
    enterprise_tab  QWORD ?
    registry_path   QWORD ?
    user_data       QWORD ?
SETTINGS_DIALOG ENDS

; Tab Control IDs
TAB_GENERAL        EQU 1000
TAB_MODEL          EQU 1001
TAB_CHAT           EQU 1002
TAB_SECURITY       EQU 1003
TAB_TRAINING       EQU 1004
TAB_CICD           EQU 1005
TAB_ENTERPRISE     EQU 1006

; Control IDs for General Tab
CHK_AUTO_START     EQU 1100
CHK_MINIMIZE_TRAY  EQU 1101
CHK_SAVE_SESSIONS  EQU 1102
DDL_THEME          EQU 1103
SPN_FONT_SIZE      EQU 1104

; Control IDs for Model Tab
EDT_MODEL_PATH     EQU 1200
BTN_BROWSE_MODEL   EQU 1201
LVW_MODEL_LIST     EQU 1202
SPN_MAX_TOKENS     EQU 1203
SPN_TEMPERATURE    EQU 1204

; Control IDs for Chat Tab
CHK_STREAMING      EQU 1300
CHK_AUTO_SCROLL    EQU 1301
SPN_HISTORY_LIMIT  EQU 1302
EDT_PROMPT_PREFIX  EQU 1303
EDT_PROMPT_SUFFIX  EQU 1304

; Registry Paths
REGISTRY_BASE_PATH EQU "Software\\RawrXD-QtShell\\Settings"
REG_GENERAL_PATH   EQU "General"
REG_MODEL_PATH     EQU "Model"
REG_CHAT_PATH      EQU "Chat"
REG_SECURITY_PATH  EQU "Security"
REG_TRAINING_PATH  EQU "Training"
REG_CICD_PATH      EQU "CI_CD"
REG_ENTERPRISE_PATH EQU "Enterprise"