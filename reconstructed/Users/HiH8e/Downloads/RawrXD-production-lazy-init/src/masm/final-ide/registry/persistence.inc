; ============================================================================
; Registry Persistence Include File
; ============================================================================
; Constants and functions for registry_persistence.asm
; ============================================================================

; Registry Functions
EXTERN RegistryOpenKey:PROC
EXTERN RegistryCloseKey:PROC
EXTERN RegistrySetDWORD:PROC
EXTERN RegistryGetDWORD:PROC
EXTERN RegistrySetString:PROC
EXTERN RegistryGetString:PROC
EXTERN LoadSettingsFromRegistry:PROC
EXTERN SaveSettingsToRegistry:PROC

; Registry Paths
REGISTRY_BASE_PATH     EQU "Software\\RawrXD-QtShell\\Settings"
REG_GENERAL_PATH       EQU "General"
REG_MODEL_PATH         EQU "Model"
REG_CHAT_PATH          EQU "Chat"
REG_SECURITY_PATH      EQU "Security"
REG_TRAINING_PATH      EQU "Training"
REG_CICD_PATH          EQU "CI_CD"
REG_ENTERPRISE_PATH    EQU "Enterprise"

; Registry Value Names
REG_AUTO_SAVE          EQU "AutoSave"
REG_STARTUP_FULLSCREEN EQU "StartupFullscreen"
REG_FONT_SIZE          EQU "FontSize"
REG_MODEL_PATH_VAL     EQU "ModelPath"
REG_DEFAULT_MODEL      EQU "DefaultModel"
REG_CHAT_MODEL         EQU "ChatModel"
REG_TEMPERATURE        EQU "Temperature"
REG_MAX_TOKENS         EQU "MaxTokens"
REG_SYSTEM_PROMPT      EQU "SystemPrompt"
REG_API_KEY            EQU "ApiKey"
REG_ENCRYPTION         EQU "Encryption"
REG_SECURE_STORAGE     EQU "SecureStorage"