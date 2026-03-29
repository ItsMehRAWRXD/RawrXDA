IFNDEF HERETIC_NODEP_INC
HERETIC_NODEP_INC EQU 1

option casemap:none

STRUCT_TOKEN_DATA_SIZE          EQU 12
OFF_TOKEN_ID                    EQU 0
OFF_TOKEN_LOGIT                 EQU 4
OFF_TOKEN_P                     EQU 8

; Unified llama_token_data_array wrapper mapping for Sovereign/Heretic bridge.
OFF_LLAMA_TOKEN_DATA_ARRAY_DATA EQU 0
OFF_LLAMA_TOKEN_DATA_ARRAY_SIZE EQU 8

; Symbolic aliases used by reverse-engineered Sovereign lane docs.
ON_RAWR_TOKEN_DATA_ARRAY_DATA   EQU OFF_LLAMA_TOKEN_DATA_ARRAY_DATA
ON_RAWR_TOKEN_DATA_ARRAY_SIZE   EQU OFF_LLAMA_TOKEN_DATA_ARRAY_SIZE

; KV-Cache rollback sentinel (for UI telemetry)
SENTINEL_ROLLBACK_SUCCESS       EQU 1751431337h  ; 0x1751431337

STD_OUTPUT_HANDLE               EQU -11

.data
    ; Refusal Logit: IEEE 754 for -FLT_MAX or -100.0f
    ; Using 0C2C80000h for -100.0f to ensure mathematical impossibility
    VAL_REFUSAL_PENALTY  DWORD 0C2C80000h 
    VAL_BEACON_BOOST     DWORD 040000000h ; +2.0f
    
    ; Token IDs (Pre-resolved for Llama-3/Heretic)
    ID_SORRY    DWORD 5421
    ID_CANNOT   DWORD 5678
    ID_I        DWORD 40
    
    .code

ENDIF
