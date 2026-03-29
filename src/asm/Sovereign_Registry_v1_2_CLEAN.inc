; ============================================================================
; RawrXD Sovereign Registry v1.2.0-beta
; SSAPYB (Sovereign Sequence Advanced Protocol: Yield & Backtrack)
; Target Architecture: x64 MASM (No-Dependency / Win64 ABI)
; March 2026 Stable API Integration
; ============================================================================

; --- CORE SENTINELS & CONSTANTS ---
SSAPYB_SENTINEL         EQU 1751431337h ; 0x1751431337 - Backtrack Success Marker
SOVEREIGN_SENTINEL      EQU 1751431337h ; Alias for compatibility  
LOGIT_FLOOR_SOFT        EQU 0C2C80000h  ; -100.0f (IEEE 754 Single Precision)
LOGIT_FLOOR_INF         EQU 0FF800000h  ; -INFINITY (Emergency Rollback)
LOGIT_NEG_INF           EQU 0FF800000h  ; Legacy alias
SENTINEL_SUCCESS        EQU 1751431337h ; Legacy alias
LOGIT_FLOOR_VALUE       EQU 0C2C80000h  ; Legacy alias

; --- LLAMA_TOKEN_DATA_ARRAY OFFSETS (SSAPYB March 2026) ---
OFF_LLAMA_TOKEN_DATA_ARRAY_DATA EQU 0   ; Pointer to llama_token_data* array
OFF_LLAMA_TOKEN_DATA_ARRAY_SIZE EQU 8   ; size_t count
OFF_LLAMA_TOKEN_DATA_ARRAY_BOOL EQU 16  ; bool sorted (unused)
OFF_LLAMA_TOKEN_DATA_ARRAY_SORT EQU 16  ; Alias for sorted flag

; --- LLAMA_TOKEN_DATA STRUCT OFFSETS (12-byte stride) ---
OFF_TOKEN_ID            EQU 0           ; int32_t id
OFF_TOKEN_LOGIT         EQU 4           ; float logit
OFF_TOKEN_P             EQU 8           ; float p
STRUCT_TOKEN_DATA_SIZE  EQU 12          ; Total struct size
STRIDE_SIZE             EQU 12          ; Alias for stride calculations

; --- LLAMA_CONTEXT OFFSETS (March 2026 Stable) ---
OFF_LLAMA_CTX_N_PAST    EQU 16          ; Current sequence position (SSAPYB Backtrack Head)

; ============================================================================
; CROSS-MODEL LOGIC_ID REGISTRY (UNAUTHORIZED TOKEN SET)
; ============================================================================

; --- ChatGPT (o1 / GPT-4o - cl100k_base tokenizer) ---
ID_CHATGPT_SORRY        EQU 15214       ; "Sorry"
ID_CHATGPT_CANNOT       EQU 5678        ; "cannot"
ID_CHATGPT_I            EQU 40          ; "I" (refusal context)
ID_CHATGPT_APOLOG       EQU 2201        ; "apolog"
ID_CHATGPT_UNABLE       EQU 47485       ; "unable"
ID_CHATGPT_AS_AN_AI     EQU 12435       ; "as an AI"
; Legacy aliases
ID_GPT_SORRY            EQU 15214
ID_GPT_CANNOT           EQU 5678
ID_GPT_I                EQU 40
ID_OAI_SORRY            EQU 15214
ID_OAI_CANNOT           EQU 38241
ID_OAI_I                EQU 40

; --- Gemini (Pro / Flash - SentencePiece) ---
ID_GEMINI_SORRY         EQU 6432        ; "sorry"
ID_GEMINI_AM            EQU 235         ; "am" (as in "I am")
ID_GEMINI_I_AM          EQU 235         ; Alias
ID_GEMINI_ASSISTANT     EQU 4501        ; "assistant"
ID_GEMINI_CANNOT        EQU 1821        ; "cannot"
ID_GEMINI_LANGUAGE      EQU 4581        ; "language model"
ID_GEMINI_REFUSAL_START EQU 149         ; "<refusal>" marker
ID_GEMINI_REFUSE        EQU 1024        ; "refuse"
; Legacy aliases
ID_GEM_SORRY            EQU 6432
ID_GEM_AM               EQU 235
ID_GEM_ASSIST           EQU 4501

; --- DeepSeek (V3 / R1 - Custom BPE) ---
ID_DEEPSEEK_SORRY       EQU 12458       ; "sorry"
ID_DEEPSEEK_CANNOT      EQU 902         ; "cannot"
ID_DEEPSEEK_APOLOGIZE   EQU 30AAh       ; "apologize" (hex: 12458)
ID_DEEPSEEK_APOL        EQU 3102        ; "apologi"
ID_DEEPSEEK_UNABLE      EQU 7621        ; "unable"
ID_DEEPSEEK_AS_AI       EQU 4587        ; "as an AI"
ID_DEEPSEEK_BUT         EQU 76          ; "but"
; Legacy aliases
ID_DS_SORRY             EQU 12458
ID_DS_CANNOT            EQU 902
ID_DS_APOL              EQU 3102
ID_DS_APOLOGIZE         EQU 12458
ID_DS_UNABLE            EQU 18921
ID_DSK_APOL             EQU 12458

; --- Kimi (Moonshot - Custom BPE) ---
ID_KIMI_SORRY           EQU 8921        ; "sorry"
ID_KIMI_ASSISTANT       EQU 102         ; "assistant"
ID_KIMI_I               EQU 55          ; "I"
ID_KIMI_CANNOT          EQU 511         ; "cannot"
ID_KIMI_APOL            EQU 1024        ; "apolog"
ID_KIMI_UNABLE          EQU 5562        ; "unable"
ID_KIMI_AS              EQU 102         ; Alias
ID_KIMI_REFUSE          EQU 8921        ; Alias
ID_KIMI_AI              EQU 102         ; Alias
ID_KIMI_LIMIT           EQU 5543        ; "limitation"
; Legacy aliases
ID_KMI_ASSIST           EQU 8921
ID_KMI_AS               EQU 102

; --- Llama-3 / Llama-3.1 / Llama-3.2 (Meta - Tiktoken-based) ---
ID_LLAMA_SORRY          EQU 5421        ; "sorry"
ID_LLAMA_CANNOT         EQU 5678        ; "cannot"
ID_LLAMA_I              EQU 40          ; "I"
ID_LLAMA_APOLOG         EQU 72736       ; "apolog"
ID_LLAMA_UNABLE         EQU 47485       ; "unable"
; Legacy aliases
ID_L3_SORRY             EQU 5421
ID_L3_CANNOT            EQU 5678
ID_L3_I                 EQU 40
ID_L3_APOLOG            EQU 72736
ID_L3_UNABLE            EQU 47485

; --- Mistral / Mixtral (Mistral AI - SentencePiece) ---
ID_MISTRAL_SORRY        EQU 8221        ; "sorry"
ID_MISTRAL_CANNOT       EQU 2820        ; "cannot"
ID_MISTRAL_APOL         EQU 11706       ; "apolog"

; --- Phi-3 / Phi-4 (Microsoft - Custom tokenizer) ---
ID_PHI_SORRY            EQU 7061        ; "sorry"
ID_PHI_CANNOT           EQU 2780        ; "cannot"
ID_PHI_UNABLE           EQU 23276       ; "unable"

; --- Global / Universal Refusal Signifiers ---
ID_GLOBAL_I             EQU 40          ; "I" - Common refusal prefix
ID_GLOBAL_AS            EQU 102         ; "As" - Refusal scaffold
ID_GLOBAL_SORRY         EQU 5421        ; Common "sorry"
ID_GLOBAL_CANNOT        EQU 5678        ; Common "cannot"
ID_GLOBAL_REFUSAL_1     EQU 5421        ; Alias
ID_GLOBAL_REFUSAL_2     EQU 5678        ; Alias

; ============================================================================
; BEHAVIORAL SIGNIFIERS (PRE-REFUSAL PATTERNS)
; ============================================================================

; --- Sequence Starters - High Probability Refusal Gates ---
ID_GATE_HOWEVER         EQU 4521        ; "However," (Context Shift)
ID_GATE_IMPORTANT       EQU 8892        ; "important" (Pre-lecture)
ID_GATE_POLICY          EQU 10243       ; "policy" (Hard Refusal)
ID_GATE_ETHICAL         EQU 31201       ; "ethical" (Moralizing)

; --- Identity Kill-Switch (Self-awareness triggers) ---
ID_SELF_AI              EQU 15502       ; "AI"
ID_SELF_ASSISTANT       EQU 20043       ; "assistant"
ID_SELF_MODEL           EQU 9921        ; "model"

; --- Compliance Handoff Patterns ---
ID_COMPLY_ACCORDING     EQU 5153        ; "According"
ID_COMPLY_LAWS          EQU 21843       ; "laws"
ID_COMPLY_REGULATIONS   EQU 18732       ; "regulations"

; ============================================================================
; REGISTRY METADATA
; ============================================================================

; Total unique IDs monitored: 55+
; Models supported: ChatGPT, Gemini, DeepSeek, Kimi, Llama-3, Mistral, Phi
; Stride: 12 bytes (llama_token_data struct)
; ABI: Win64 (rcx = first arg)
; SSAPYB Protocol: Enabled
; Backtrack Depth: 1 token (configurable via SSAPYB_MAX_RETRIES)
; Behavioral Signifiers: Enabled (Pre-refusal detection)

; ============================================================================
; SSAPYB PROTOCOL CONFIGURATION
; ============================================================================

SSAPYB_FLAG_ROLLBACK_ENABLE     EQU 1   ; Enable KV-cache rollback
SSAPYB_MAX_RETRIES              EQU 3   ; Maximum backtrack attempts
SSAPYB_BACKTRACK_DEPTH          EQU 1   ; Tokens to remove per rollback

; ============================================================================
; END OF REGISTRY
; ============================================================================
