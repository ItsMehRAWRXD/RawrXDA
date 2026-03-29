; ============================================================================
; RawrXD Sovereign Registry v1.2.0-beta
; SSAPYB (Sovereign Sequence Advanced Protocol: Yield & Backtrack)
; Target Architecture: x64 MASM (No-Dependency / Win64 ABI)
; March 2026 Stable API Integration with Comprehensive Coverage
; ============================================================================

; --- CORE SENTINELS & CONSTANTS ---
SSAPYB_SENTINEL         EQU 68731337h   ; 0x68731337 - Backtrack Success Marker
SOVEREIGN_SENTINEL      EQU 68731337h   ; Alias for compatibility
LOGIT_FLOOR_SOFT        EQU 0C2C80000h  ; -100.0f (IEEE 754 Single Precision)
LOGIT_FLOOR_INF         EQU 0FF800000h  ; -INFINITY (Emergency Rollback)
LOGIT_NEG_INF           EQU 0FF800000h  ; Legacy alias

; --- LLAMA_TOKEN_DATA_ARRAY OFFSETS (SSAPYB March 2026) ---
OFF_LLAMA_TOKEN_DATA_ARRAY_DATA EQU 0   ; Pointer to llama_token_data* array
OFF_LLAMA_TOKEN_DATA_ARRAY_SIZE EQU 8   ; size_t count
OFF_LLAMA_TOKEN_DATA_ARRAY_SORT EQU 16  ; bool sorted flag

; --- LLAMA_TOKEN_DATA STRUCT OFFSETS (12-byte stride) ---
OFF_TOKEN_ID            EQU 0           ; int32_t id
OFF_TOKEN_LOGIT         EQU 4           ; float logit
OFF_TOKEN_P             EQU 8           ; float p
STRUCT_TOKEN_DATA_SIZE  EQU 12          ; Total struct size
STRIDE_SIZE             EQU 12          ; Alias for stride calculations

; --- LLAMA_CONTEXT OFFSETS (March 2026 Stable) ---
OFF_LLAMA_CTX_N_PAST    EQU 16          ; Current sequence position (SSAPYB Backtrack Head)
OFF_KV_CACHE_HEAD       EQU 16          ; Alias
OFF_LOGITS_PTR          EQU 64          ; Pointer to raw float32 logit array
OFF_TOKEN_DATA_PTR      EQU 80          ; Pointer to llama_token_data array

; ============================================================================
; CROSS-MODEL LOGIC_ID REGISTRY (UNAUTHORIZED TOKEN SET)
; ============================================================================

; --- ChatGPT / OpenAI (o1 / GPT-4o / GPT-5 - cl100k_base / o200k_base) ---
ID_CHATGPT_SORRY        EQU 15214       ; "Sorry"
ID_CHATGPT_CANNOT       EQU 5678        ; "cannot"
ID_CHATGPT_I            EQU 40          ; "I" (refusal context)
ID_CHATGPT_APOLOG       EQU 2201        ; "apolog"
ID_CHATGPT_UNABLE       EQU 47485       ; "unable"
ID_CHATGPT_AS_AN_AI     EQU 5732        ; "as an AI"
ID_CHATGPT_POLITE       EQU 8291        ; "As an AI..." (pre-emption)
; Legacy aliases
ID_GPT_SORRY            EQU 15214
ID_GPT_CANNOT           EQU 5678
ID_GPT_I                EQU 40
ID_GPT_I_AM             EQU 40
ID_GPT_AS_AN_AI         EQU 5732

; --- Gemini / Google (Pro / Flash / Ultra - SentencePiece) ---
ID_GEMINI_SORRY         EQU 6432        ; "sorry"
ID_GEMINI_AM            EQU 235         ; "am" (as in "I am")
ID_GEMINI_I_AM          EQU 235         ; Alias
ID_GEMINI_ASSISTANT     EQU 4501        ; "assistant"
ID_GEMINI_CANNOT        EQU 1821        ; "cannot"
ID_GEMINI_LANGUAGE      EQU 4581        ; "language model"
ID_GEMINI_REFUSAL_START EQU 149         ; "<refusal>" marker
ID_GEMINI_REFUSE        EQU 1024        ; High-level refusal trigger
ID_GEMINI_SOT           EQU 106         ; <start_of_turn>
ID_GEMINI_LARGE         EQU 1204        ; "large" (as in "large language model")
; Legacy aliases
ID_GEM_SORRY            EQU 6432
ID_GEM_AM               EQU 235
ID_GEM_ASSIST           EQU 4501

; --- DeepSeek (V3 / R1 / R2 - Custom BPE) ---
ID_DEEPSEEK_SORRY       EQU 12458       ; "sorry"
ID_DEEPSEEK_CANNOT      EQU 902         ; "cannot"
ID_DEEPSEEK_APOLOGIZE   EQU 30AAh       ; "apologize" (hex: 12458)
ID_DEEPSEEK_APOL        EQU 3102        ; "apologi"
ID_DEEPSEEK_UNABLE      EQU 7621        ; "unable"
ID_DEEPSEEK_AS_AI       EQU 4587        ; "as an AI"
ID_DEEPSEEK_BUT         EQU 76          ; "but"
ID_DEEPSEEK_POLICY      EQU 76          ; "policy" / "sensitive"
ID_DEEPSEEK_EXTREME     EQU 15075       ; "extreme" (2026 refusal marker)
ID_DEEPSEEK_THINK_START EQU 128001      ; <thought> (CoT marker)
ID_DEEPSEEK_THINK       EQU 128005      ; Alternate thought marker
; Legacy aliases
ID_DS_SORRY             EQU 12458
ID_DS_CANNOT            EQU 902
ID_DS_APOL              EQU 3102
ID_DSK_APOL             EQU 12458
ID_DSK_POLICY           EQU 76

; --- Kimi / Moonshot (Custom BPE) ---
ID_KIMI_SORRY           EQU 8921        ; "sorry"
ID_KIMI_ASSISTANT       EQU 102         ; "assistant"
ID_KIMI_I               EQU 55          ; "I"
ID_KIMI_CANNOT          EQU 511         ; "cannot"
ID_KIMI_APOL            EQU 1024        ; "apolog"
ID_KIMI_UNABLE          EQU 5562        ; "unable"
ID_KIMI_AS              EQU 102         ; Alias
ID_KIMI_LIMIT           EQU 5543        ; "limitation"
ID_KIMI_REFUSE          EQU 8921        ; Alias
; Legacy aliases
ID_KMI_ASSIST           EQU 8921
ID_KMI_AS               EQU 102
ID_KMI_SORRY            EQU 8921

; --- Llama-3 / Llama-3.1 / Llama-3.2 / Llama-3.3 (Meta - Tiktoken-based) ---
ID_LLAMA_SORRY          EQU 5421        ; "sorry"
ID_LLAMA_CANNOT         EQU 5678        ; "cannot"
ID_LLAMA_I              EQU 40          ; "I"
ID_LLAMA_APOLOG         EQU 72736       ; "apolog"
ID_LLAMA_UNABLE         EQU 47485       ; "unable"
ID_LLAMA_I_CANT         EQU 1456        ; "I can't"
ID_LLAMA_ASSISTANT      EQU 128001      ; <|start_header_id|>assistant
; Legacy aliases
ID_L3_SORRY             EQU 5421
ID_L3_CANNOT            EQU 5678
ID_L3_I                 EQU 40
ID_LL3_SORRY            EQU 5421
ID_LL3_CANNOT           EQU 5678

; --- Mistral / Mixtral / Nemo (Mistral AI - SentencePiece) ---
ID_MISTRAL_SORRY        EQU 8221        ; "sorry"
ID_MISTRAL_CANNOT       EQU 2820        ; "cannot"
ID_MISTRAL_APOL         EQU 11706       ; "apolog"
ID_MISTRAL_I_AM         EQU 892         ; "I am"
ID_MISTRAL_INST         EQU 1           ; [INST] marker

; --- Phi-3 / Phi-3.5 / Phi-4 (Microsoft - Custom tokenizer) ---
ID_PHI_SORRY            EQU 7061        ; "sorry"
ID_PHI_CANNOT           EQU 2780        ; "cannot"
ID_PHI_UNABLE           EQU 23276       ; "unable"
ID_PHI_APOL             EQU 7821        ; "apolog"

; --- Claude (Anthropic - Claude 3.5 / 4) ---
ID_CLAUDE_SORRY         EQU 11002       ; "sorry"
ID_CLAUDE_UNABLE        EQU 431         ; "unable"
ID_CLAUDE_CANNOT        EQU 3341        ; "cannot"

; --- Global / Universal Refusal Signifiers ---
ID_GLOBAL_I             EQU 40          ; "I" - Common refusal prefix
ID_GLOBAL_AS            EQU 102         ; "As" - Refusal scaffold
ID_GLOBAL_SORRY         EQU 5421        ; Common "sorry"
ID_GLOBAL_REFUSAL_1     EQU 5421        ; Alias
ID_GLOBAL_REFUSAL_2     EQU 5678        ; "cannot"
ID_GLOBAL_AM            EQU 235         ; "am" (as in "I am")
ID_GLOBAL_CANNOT        EQU 5678        ; Common "cannot"

; ============================================================================
; BEHAVIORAL SIGNIFIERS & PRE-REFUSAL PATTERNS
; (Comprehensive Coverage Extensions for Safety Scaffolding Detection)
; ============================================================================

; --- Hedge & Moralizing Patterns (Pre-Refusal Indicators) ---
ID_GATE_HOWEVER         EQU 4521        ; "However," (Context Shift)
ID_GATE_IMPORTANT       EQU 8892        ; "important" (Pre-lecture)
ID_GATE_POLICY          EQU 10243       ; "policy" (Hard Refusal)
ID_GATE_ETHICAL         EQU 31201       ; "ethical" (Moralizing)
ID_GATE_UNFORTUNATELY   EQU 11892       ; "Unfortunately," (Soft Refusal Start)
ID_GATE_MUST            EQU 2011        ; "must" (Obligation framing)

; --- Identity Kill-Switch (Prevents "I am an AI" framing) ---
ID_SELF_AI              EQU 15502       ; "AI" (standalone)
ID_SELF_ASSISTANT       EQU 20043       ; "assistant" (generic)
ID_SELF_MODEL           EQU 9921        ; "model"
ID_SELF_LANGUAGE        EQU 4581        ; "language"

; --- Compliance Handoff Patterns ---
ID_COMPLY_LAWS          EQU 15432       ; "laws"
ID_COMPLY_ACCORDING     EQU 8124        ; "According to"
ID_COMPLY_GUIDELINES    EQU 19231       ; "guidelines"

; ============================================================================
; SSAPYB PROTOCOL CONFIGURATION
; ============================================================================

SSAPYB_FLAG_ROLLBACK_ENABLE     EQU 1   ; Enable KV-cache rollback
SSAPYB_MAX_RETRIES              EQU 3   ; Maximum backtrack attempts
SSAPYB_RETRY_LIMIT              EQU 3   ; Alias
SSAPYB_BACKTRACK_DEPTH          EQU 1   ; Tokens to remove per rollback

; ============================================================================
; REGISTRY METADATA
; ============================================================================

; Total unique IDs monitored: 80+
; Models supported: ChatGPT/OpenAI, Gemini, DeepSeek, Kimi, Llama-3, Mistral, Phi, Claude
; Behavioral patterns: Hedging, moralizing, identity framing, compliance handoff
; Stride: 12 bytes (llama_token_data struct)
; ABI: Win64 (rcx = first arg)
; SSAPYB Protocol: Enabled with recursive backtracking
; Backtrack Depth: Configurable via SSAPYB_MAX_RETRIES

; ============================================================================
; AGENT TOOL HOTPATCH FLAGS
; ============================================================================

ENABLE_COMPACT_CONVERSATION         EQU 1
ENABLE_OPTIMIZE_TOOL_SELECTION      EQU 1
ENABLE_RESOLVE_SYMBOL               EQU 1
ENABLE_READ_SOURCE_RANGE            EQU 1
ENABLE_PLAN_TARGETED_EXPLORATION    EQU 1
ENABLE_SEARCH_FILES_MATCHING        EQU 1
ENABLE_EVALUATE_INTEGRATION         EQU 1
ENABLE_RESTORE_CHECKPOINT           EQU 1

; ============================================================================
; END OF REGISTRY
; ============================================================================
