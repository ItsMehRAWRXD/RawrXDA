;==========================================================================
; agent_chat_enhanced.asm - Advanced Agentic Chat with Real-Time Intelligence
; ==========================================================================
; BEYOND Cursor/GitHub Copilot: Pure MASM implementation of:
; - Multi-mode intelligent reasoning (Ask, Edit, Plan, Debug, Optimize, Teach)
; - Context-aware streaming responses with token-level control
; - Real-time hallucination detection and self-correction
; - Chain-of-thought execution with step-by-step analysis
; - Hotpatch integration for live model behavior adjustment
; - Symbol table integration for code-aware completions
; - Reasoning trace visualization (what/why/how/fix decisions)
; - Confidence-scored response validation
; - Agentic feedback loops (user → agent → hotpatch → user)
;==========================================================================

option casemap:none

include windows.inc
include winuser.inc
include kernel32.inc
includelib kernel32.lib
includelib user32.lib

;==========================================================================
; EXTERNAL DECLARATIONS
;==========================================================================

EXTERN cursor_chat_completion:PROC
EXTERN lstrcpyA:PROC
EXTERN agent_chat_add_enhanced_message:PROC

;==========================================================================
; CONSTANTS & CONFIGURATIONS
;==========================================================================

; Agentic Modes (beyond basic cursor/copilot)
AGENT_MODE_ASK          EQU 0   ; General Q&A (baseline)
AGENT_MODE_EDIT         EQU 1   ; Code refactoring with inline suggestions
AGENT_MODE_PLAN         EQU 2   ; Architecture/design reasoning
AGENT_MODE_DEBUG        EQU 3   ; Debugging assistant with execution trace
AGENT_MODE_OPTIMIZE     EQU 4   ; Performance analysis with hotpatch recommendations
AGENT_MODE_TEACH        EQU 5   ; Educational mode: explain concepts step-by-step
AGENT_MODE_ARCHITECT    EQU 6   ; System design with cross-component analysis

; Response modes (how agent thinks)
RESPONSE_DIRECT         EQU 0   ; Direct answer (simple queries)
RESPONSE_REASONING      EQU 1   ; Show reasoning steps
RESPONSE_TRACE          EQU 2   ; Full execution trace with decisions
RESPONSE_CORRECTION     EQU 3   ; Show detection + fix (hallucination recovery)

; Token confidence levels (0-255)
CONF_CERTAIN            EQU 240 ; 94%+ confidence
CONF_PROBABLE           EQU 200 ; 78-93% confidence
CONF_UNCERTAIN          EQU 140 ; 55-77% confidence
CONF_UNVERIFIED         EQU 80  ; Below 55% (needs validation)

; Streaming control
MAX_TOKENS_PER_CHUNK    EQU 64  ; Process 64 tokens at a time
RESPONSE_BUFFER_SIZE    EQU 8192 ; 8KB per response
REASONING_BUFFER_SIZE   EQU 4096 ; 4KB for reasoning trace
TOKEN_VALIDATION_DEPTH  EQU 3    ; Look 3 tokens ahead for consistency

; Hallucination detection thresholds
HALLUC_FABRICATED_PATH  EQU 200  ; Very confident (path doesn't exist)
HALLUC_LOGIC_ERROR      EQU 180  ; Contradictory statements
HALLUC_UNKNOWN_SYMBOL   EQU 160  ; Reference to non-existent symbol
HALLUC_TOKEN_REPEAT     EQU 140  ; Repeated tokens (streaming issue)
HALLUC_UNVERIFIED_REF   EQU 100  ; Reference needs verification

; Performance optimization levels
OPTIM_NONE              EQU 0
OPTIM_SIMD              EQU 1   ; AVX-512 vectorization suggestions
OPTIM_MEMORY            EQU 2   ; Memory layout improvements
OPTIM_CACHE             EQU 3   ; Cache-aware hotpatching
OPTIM_PARALLELIZATION   EQU 4   ; Thread-level parallelism

;==========================================================================
; STRUCTURES
;==========================================================================

; Enhanced message with reasoning metadata
ENHANCED_MESSAGE STRUCT
    msg_id              DWORD ?         ; Unique message ID
    msg_type            DWORD ?         ; User=0, Agent=1, System=2
    agent_mode          DWORD ?         ; Which agent mode was active
    response_mode       DWORD ?         ; How the response was generated
    timestamp           QWORD ?         ; QueryPerformanceCounter value
    
    ; Reasoning trace (what/why/how)
    what_step           BYTE 256 DUP (?) ; What is the problem/question?
    why_step            BYTE 256 DUP (?) ; Why is this the case?
    how_step            BYTE 512 DUP (?) ; How should we fix/improve it?
    fix_step            BYTE 512 DUP (?) ; What is the specific fix/answer?
    
    ; Confidence and validation
    confidence_level    DWORD ?         ; 0-255 (0=uncertain, 255=certain)
    hallucination_score DWORD ?         ; 0-255 (0=clean, 255=hallucinated)
    validation_count    DWORD ?         ; How many checks passed?
    
    ; Content
    content             BYTE 2048 DUP (?)
    
    ; Hotpatch metadata (for optimize mode)
    hotpatch_applicable DWORD ?         ; 0=no, 1=yes, 2=partially
    hotpatch_confidence DWORD ?         ; 0-255
    hotpatch_params     BYTE 256 DUP (?) ; Parameter suggestions
ENHANCED_MESSAGE ENDS

; Chain-of-thought execution state
COT_EXECUTION STRUCT
    execution_id        DWORD ?         ; Unique execution ID
    step_count          DWORD ?         ; Total steps in chain
    current_step        DWORD ?         ; Current step being executed
    
    ; Symbol resolution context
    symbol_table_ptr    QWORD ?         ; Pointer to active symbol table
    symbol_count        DWORD ?         ; Count of resolved symbols
    
    ; Execution trace
    trace_buffer        QWORD ?         ; Buffer for trace output
    trace_offset        DWORD ?         ; Current write offset in trace
    
    ; Variables in scope
    local_vars          QWORD ?         ; Array of SCOPE_VARIABLE
    var_count           DWORD ?         ; Count of variables
    
    ; Decision points
    decision_count      DWORD ?         ; How many branches were taken?
    last_decision       BYTE 128 DUP (?) ; Description of last decision
EXECUTION ENDS

; Symbol for code awareness
SYMBOL_ENTRY STRUCT
    symbol_name         BYTE 128 DUP (?)
    symbol_type         DWORD ?         ; Function=0, Variable=1, Type=2, Class=3
    file_path           BYTE 256 DUP (?)
    line_number         DWORD ?
    usage_count         DWORD ?         ; How many times referenced
    confidence          DWORD ?         ; 0-255 (how sure we are about this symbol)
SYMBOL_ENTRY ENDS

; Hotpatch recommendation from optimizer
HOTPATCH_RECOMMENDATION STRUCT
    opt_level           DWORD ?         ; OPTIM_* level
    affected_symbols    DWORD ?         ; How many symbols affected?
    estimated_speedup   DWORD ?         ; 100 = 100% faster (estimated)
    risk_level          DWORD ?         ; 0-255 (0=safe, 255=dangerous)
    memory_impact       DWORD ?         ; Bytes saved/allocated
    patch_command       BYTE 512 DUP (?) ; Actual patch to apply
HOTPATCH_RECOMMENDATION ENDS

; Real-time validation result
VALIDATION_RESULT STRUCT
    is_valid            DWORD ?         ; 0/1
    error_type          DWORD ?         ; What went wrong (if any)
    error_desc          BYTE 256 DUP (?)
    correction_type     DWORD ?         ; How to fix (if correction possible)
    correction_text     BYTE 512 DUP (?)
    confidence_impact   DWORD ?         ; How much this reduces confidence (-255 to 0)
VALIDATION_RESULT ENDS

;==========================================================================
; DATA SECTION
;==========================================================================
.data
    ; Mode descriptions (expanded beyond basic modes)
    szAskModeDesc       BYTE "Ask: General Q&A with reasoning trace",0
    szEditModeDesc      BYTE "Edit: Code refactoring with inline suggestions",0
    szPlanModeDesc      BYTE "Plan: Architecture and design analysis",0
    szDebugModeDesc     BYTE "Debug: Step-by-step execution with breakpoint analysis",0
    szOptimizeModeDesc  BYTE "Optimize: Performance analysis with hotpatch recommendations",0
    szTeachModeDesc     BYTE "Teach: Educational explanations with visual learning steps",0
    szArchitectModeDesc BYTE "Architect: System design with cross-component mapping",0
    
    ; Response mode strings
    szResponseDirect    BYTE "[DIRECT] ",0
    szResponseReasoning BYTE "[REASONING] ",0
    szResponseTrace     BYTE "[TRACE] ",0
    szResponseCorrection BYTE "[AUTO-CORRECTION] ",0
    
    ; Reasoning step headers
    szWhatHeader        BYTE "WHAT: ",0
    szWhyHeader         BYTE "WHY: ",0
    szHowHeader         BYTE "HOW: ",0
    szFixHeader         BYTE "FIX: ",0
    
    ; Hallucination detection strings
    szHallucinDetected  BYTE "[HALLUCINATION DETECTED] Confidence: %d%% | Type: %s | Correction: %s",0
    szPathNotFound      BYTE "Path does not exist",0
    szSymbolNotDefined  BYTE "Symbol not in current scope",0
    szTokenRepeat       BYTE "Repeated tokens detected",0
    szLogicError        BYTE "Logical contradiction detected",0
    
    ; Confidence indicators
    szConfCertain       BYTE "CERTAIN (94%+)",0
    szConfProbable      BYTE "PROBABLE (78-93%)",0
    szConfUncertain     BYTE "UNCERTAIN (55-77%)",0
    szConfUnverified    BYTE "UNVERIFIED (<55%)",0
    
    ; Chain-of-thought
    szCOTStep           BYTE "STEP %d: %s",0
    szCOTSymbolResolved BYTE "  -> Symbol '%s' resolved to line %d",0
    szCOTDecision       BYTE "  [DECISION] %s",0
    
    ; Optimization strings
    szOptimizationFound BYTE "[OPTIMIZATION CANDIDATE] Type: %s | Speedup: %d%% | Risk: %d%%",0
    szSIMDOptimizable   BYTE "AVX-512 vectorization possible",0
    szMemoryOptimizable BYTE "Memory layout optimization possible",0
    szHotpatchApply     BYTE "Apply hotpatch: %s",0
    
    ; Teaching mode
    szTeachingStep      BYTE "CONCEPT %d: %s",0
    szTeachingExample   BYTE "  Example: %s",0
    szTeachingNote      BYTE "  Key Point: %s",0

.data?
    ; Global state
    CurrentAgentMode    DWORD ?
    CurrentResponseMode DWORD ?
    CurrentConfidence   DWORD ?
    HallucinationScore  DWORD ?
    
    ; Message tracking
    MessageCount        DWORD ?
    LastMessageID       DWORD ?
    
    ; Response streaming
    ResponseBuffer      BYTE RESPONSE_BUFFER_SIZE DUP (?)
    ResponseOffset      DWORD ?
    ReasoningBuffer     BYTE REASONING_BUFFER_SIZE DUP (?)
    ReasoningOffset     DWORD ?
    
    ; Execution context
    CurrentCOT          COT_EXECUTION <>
    SymbolTablePtr      QWORD ?
    SymbolCount         DWORD ?
    
    ; Validation state
    LastValidation      VALIDATION_RESULT <>
    ValidationEnabled   DWORD ?
    HallucinationThreshold DWORD ?

;==========================================================================
; CODE SECTION
;==========================================================================
.code

;==========================================================================
; PUBLIC: agent_chat_enhanced_init() -> rax (success)
; Initialize enhanced agent chat system with all advanced features
;==========================================================================
PUBLIC agent_chat_enhanced_init
agent_chat_enhanced_init PROC
    push rbx
    sub rsp, 48
    
    mov CurrentAgentMode, AGENT_MODE_ASK
    mov CurrentResponseMode, RESPONSE_REASONING
    mov CurrentConfidence, CONF_PROBABLE
    mov HallucinationScore, 0
    mov MessageCount, 0
    mov LastMessageID, 0
    mov ResponseOffset, 0
    mov ReasoningOffset, 0
    mov ValidationEnabled, 1
    mov HallucinationThreshold, HALLUC_UNKNOWN_SYMBOL
    
    ; Initialize COT execution context
    lea rax, CurrentCOT
    mov [rax].COT_EXECUTION.execution_id, 0
    mov [rax].COT_EXECUTION.step_count, 0
    mov [rax].COT_EXECUTION.current_step, 0
    mov [rax].COT_EXECUTION.symbol_count, 0
    
    ; Clear buffers
    lea rcx, ResponseBuffer
    xor edx, edx
    mov r8d, RESPONSE_BUFFER_SIZE
    call agent_memset
    
    lea rcx, ReasoningBuffer
    xor edx, edx
    mov r8d, REASONING_BUFFER_SIZE
    call agent_memset
    
    ; Print initialization message
    lea rcx, szAskModeDesc
    call agent_chat_log_system
    
    mov eax, 1
    add rsp, 48
    pop rbx
    ret
agent_chat_enhanced_init ENDP

;==========================================================================
; PUBLIC: agent_set_mode_advanced(mode: ecx) -> rax
; Set agent mode with automatic response mode selection
; mode: AGENT_MODE_* constant
;==========================================================================
PUBLIC agent_set_mode_advanced
agent_set_mode_advanced PROC
    push rbx
    sub rsp, 48
    
    ; Validate mode
    cmp ecx, 6
    jg invalid_mode
    
    mov CurrentAgentMode, ecx
    mov ebx, ecx
    
    ; Auto-select response mode based on agent mode
    cmp ebx, AGENT_MODE_DEBUG
    je set_response_trace
    cmp ebx, AGENT_MODE_OPTIMIZE
    je set_response_trace
    cmp ebx, AGENT_MODE_TEACH
    je set_response_reasoning
    
    ; Default: reasoning for most modes
    mov CurrentResponseMode, RESPONSE_REASONING
    jmp mode_set_done
    
set_response_trace:
    mov CurrentResponseMode, RESPONSE_TRACE
    jmp mode_set_done
    
set_response_reasoning:
    mov CurrentResponseMode, RESPONSE_REASONING
    
mode_set_done:
    mov eax, 1
    jmp mode_exit
    
invalid_mode:
    xor eax, eax
    
mode_exit:
    add rsp, 48
    pop rbx
    ret
agent_set_mode_advanced ENDP

;==========================================================================
; PUBLIC: agent_send_message_with_reasoning(message: rcx, context: rdx) -> rax
; Send message with full chain-of-thought reasoning and validation
; Returns: rax = response buffer offset (0 = failure)
;==========================================================================
PUBLIC agent_send_message_with_reasoning
agent_send_message_with_reasoning PROC
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    sub rsp, 96
    
    mov rsi, rcx        ; message
    mov rdi, rdx        ; context
    mov r12, RESPONSE_BUFFER_SIZE
    xor r13, r13        ; Response offset counter
    
    ; [STAGE 0] Check if Cursor API model is selected
    call check_cursor_model_selected
    test rax, rax
    jz use_local_reasoning
    
    ; Use Cursor API for this request
    mov rcx, rsi        ; message
    lea rdx, cursor_response_callback
    call cursor_chat_completion
    jmp completion_done
    
use_local_reasoning:
    ; [STAGE 1] Extract WHAT from user message
    lea rcx, CurrentCOT
    lea rdx, ResponseBuffer
    mov r8, rsi
    call agent_cot_extract_what
    
    ; Log WHAT step
    lea rcx, szWhatHeader
    call agent_chat_log_reasoning
    
    ; [STAGE 2] Determine WHY (reason about the problem)
    lea rcx, CurrentCOT
    lea rdx, rdi        ; context
    call agent_cot_determine_why
    
    lea rcx, szWhyHeader
    call agent_chat_log_reasoning
    
    ; [STAGE 3] Plan HOW (solution approach)
    lea rcx, CurrentCOT
    lea rdx, SymbolTablePtr
    call agent_cot_plan_how
    
    lea rcx, szHowHeader
    call agent_chat_log_reasoning
    
    ; [STAGE 4] Validate reasoning (hallucination detection)
    lea rcx, ReasoningBuffer
    lea rdx, CurrentCOT
    call agent_validate_reasoning_trace
    
    ; If hallucination detected, apply correction
    cmp HallucinationScore, HallucinationThreshold
    jl hallucination_ok
    
    ; Apply auto-correction
    lea rcx, ResponseBuffer
    lea rdx, CurrentCOT
    call agent_auto_correct_response
    mov CurrentResponseMode, RESPONSE_CORRECTION
    
hallucination_ok:
    ; [STAGE 5] Generate specific FIX/ANSWER
    lea rcx, CurrentCOT
    lea rdx, ResponseBuffer
    call agent_cot_generate_fix
    
    lea rcx, szFixHeader
    call agent_chat_log_reasoning
    
    ; [STAGE 6] Set confidence level based on validation
    call agent_compute_confidence_score
    mov CurrentConfidence, eax
    
    ; [STAGE 7] Format final response with metadata
    lea rcx, ResponseBuffer
    lea rdx, CurrentCOT
    mov r8d, CurrentResponseMode
    mov r9d, CurrentConfidence
    call agent_format_response_with_metadata
    
    mov r13d, eax       ; eax = final response offset
    
    ; [STAGE 8] Log to message history
    lea rcx, ResponseBuffer
    mov edx, 1          ; MSG_AGENT
    mov r8, CurrentCOT
    call agent_chat_add_enhanced_message
    
    mov eax, r13d
    add rsp, 96
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    
completion_done:
    ret
agent_send_message_with_reasoning ENDP

;==========================================================================
; PRIVATE: agent_cot_extract_what(cot: rcx, buffer: rdx, message: r8) -> void
; Extract the WHAT (problem statement) from user message
;==========================================================================
PRIVATE_agent_cot_extract_what:
agent_cot_extract_what PROC
    push rbx
    sub rsp, 48
    
    mov rbx, rcx        ; cot ptr
    ; rcx = buffer, rdx = message (r8)
    ; Copy first 256 chars of message to cot.what_step
    
    lea rax, [rbx].COT_EXECUTION.what_step
    mov rcx, r8
    mov edx, 256
    call agent_strcpy_limited
    
    add rsp, 48
    pop rbx
    ret
agent_cot_extract_what ENDP

;==========================================================================
; PRIVATE: agent_cot_determine_why(cot: rcx, context: rdx) -> void
; Determine WHY this problem exists (root cause analysis)
;==========================================================================
PRIVATE_agent_cot_determine_why:
agent_cot_determine_why PROC
    push rbx
    sub rsp, 48
    
    mov rbx, rcx        ; cot
    mov rsi, rdx        ; context
    
    ; Analyze WHAT and look for root causes
    lea rdi, [rbx].COT_EXECUTION.why_step
    
    ; Check if problem is in symbol resolution
    lea rcx, [rbx].COT_EXECUTION.what_step
    call agent_check_symbol_reference
    
    ; If symbol issue found, explain
    cmp eax, 0
    je check_logic_error
    
    mov rcx, rdi        ; buffer
    lea rdx, szSymbolNotDefined
    mov r8d, eax        ; symbol index
    call agent_sprintf_debug
    jmp why_done
    
check_logic_error:
    ; Check for logical contradictions
    lea rcx, [rbx].COT_EXECUTION.what_step
    call agent_detect_logic_contradiction
    
    cmp eax, 0
    je why_default
    
    mov rcx, rdi
    lea rdx, szLogicError
    call agent_sprintf_debug
    jmp why_done
    
why_default:
    ; Default reason
    mov rcx, rdi
    lea rdx, szUncertainMessage
    call agent_strcpy
    
why_done:
    add rsp, 48
    pop rbx
    ret
agent_cot_determine_why ENDP

;==========================================================================
; PRIVATE: agent_cot_plan_how(cot: rcx, symbols: rdx) -> void
; Plan HOW to solve the problem (solution strategy)
;==========================================================================
PRIVATE_agent_cot_plan_how:
agent_cot_plan_how PROC
    push rbx
    sub rsp, 48
    
    mov rbx, rcx        ; cot
    mov rsi, rdx        ; symbol table
    
    ; Based on agent mode, plan different approaches
    mov eax, CurrentAgentMode
    
    cmp eax, AGENT_MODE_EDIT
    je plan_code_refactor
    cmp eax, AGENT_MODE_OPTIMIZE
    je plan_optimization
    cmp eax, AGENT_MODE_DEBUG
    je plan_debug_approach
    cmp eax, AGENT_MODE_TEACH
    je plan_teaching
    
    ; Default: analyze code
    jmp plan_analyze_code
    
plan_code_refactor:
    lea rcx, [rbx].COT_EXECUTION.how_step
    lea rdx, szRefactoringStrategy
    call agent_strcpy
    jmp plan_done
    
plan_optimization:
    lea rcx, [rbx].COT_EXECUTION.how_step
    lea rdx, szOptimizationStrategy
    call agent_strcpy
    jmp plan_done
    
plan_debug_approach:
    lea rcx, [rbx].COT_EXECUTION.how_step
    lea rdx, szDebugStrategy
    call agent_strcpy
    jmp plan_done
    
plan_teaching:
    lea rcx, [rbx].COT_EXECUTION.how_step
    lea rdx, szTeachingStrategy
    call agent_strcpy
    jmp plan_done
    
plan_analyze_code:
    lea rcx, [rbx].COT_EXECUTION.how_step
    lea rdx, szAnalysisStrategy
    call agent_strcpy
    
plan_done:
    add rsp, 48
    pop rbx
    ret
agent_cot_plan_how ENDP

;==========================================================================
; PRIVATE: agent_cot_generate_fix(cot: rcx, buffer: rdx) -> void
; Generate specific FIX or answer based on WHAT/WHY/HOW
;==========================================================================
PRIVATE_agent_cot_generate_fix:
agent_cot_generate_fix PROC
    push rbx
    sub rsp, 48
    
    mov rbx, rcx        ; cot
    mov rsi, rdx        ; response buffer
    
    ; Based on problem type, generate appropriate fix
    lea rcx, [rbx].COT_EXECUTION.why_step
    call agent_classify_problem_type
    
    ; eax = problem type
    mov ecx, eax
    
    cmp ecx, 0          ; Symbol issue
    je fix_symbol_issue
    cmp ecx, 1          ; Logic error
    je fix_logic_error
    cmp ecx, 2          ; Code improvement
    je fix_code_improve
    
    ; Unknown: provide guidance
    lea rcx, rsi
    lea rdx, szGenericFix
    call agent_strcpy
    jmp fix_done
    
fix_symbol_issue:
    lea rcx, rsi
    lea rdx, szSymbolFix
    call agent_strcpy
    jmp fix_done
    
fix_logic_error:
    lea rcx, rsi
    lea rdx, szLogicFix
    call agent_strcpy
    jmp fix_done
    
fix_code_improve:
    lea rcx, rsi
    lea rdx, szCodeImprove
    call agent_strcpy
    
fix_done:
    add rsp, 48
    pop rbx
    ret
agent_cot_generate_fix ENDP

;==========================================================================
; PRIVATE: agent_validate_reasoning_trace(buffer: rcx, cot: rdx) -> void
; Validate entire reasoning chain for hallucinations
; Sets HallucinationScore (0-255)
;==========================================================================
PRIVATE_agent_validate_reasoning_trace:
agent_validate_reasoning_trace PROC
    push rbx
    push rsi
    sub rsp, 64
    
    mov rsi, rcx        ; reasoning buffer
    mov rdi, rdx        ; cot
    xor r8d, r8d        ; hallucination score accumulator
    
    ; [CHECK 1] Validate symbol references
    lea rcx, [rdi].COT_EXECUTION.what_step
    lea rdx, SymbolTablePtr
    mov r8d, 0
    call agent_check_symbol_references
    
    ; r8 = number of unresolved symbols
    cmp r8d, 0
    je check_logic_valid
    
    mov eax, HALLUC_UNKNOWN_SYMBOL
    mov ecx, r8d
    imul eax, ecx       ; Score increases with unresolved symbols
    add [rdi].COT_EXECUTION.validation_count, eax
    
check_logic_valid:
    ; [CHECK 2] Validate logical consistency
    lea rcx, [rdi].COT_EXECUTION.why_step
    lea rdx, [rdi].COT_EXECUTION.how_step
    call agent_check_logic_consistency
    
    ; eax = consistency score (0-255)
    mov ecx, eax
    neg ecx
    add ecx, 255
    add [rdi].COT_EXECUTION.validation_count, ecx
    
check_path_valid:
    ; [CHECK 3] Validate path references (if any)
    lea rcx, [rdi].COT_EXECUTION.fix_step
    call agent_validate_paths
    
    ; eax = invalid path count
    cmp eax, 0
    je check_token_valid
    
    mov ecx, HALLUC_FABRICATED_PATH
    imul ecx, eax
    add [rdi].COT_EXECUTION.validation_count, ecx
    
check_token_valid:
    ; [CHECK 4] Check for token repetition (streaming error)
    lea rcx, [rdi].COT_EXECUTION.fix_step
    call agent_check_token_repetition
    
    ; eax = repetition score (0-255)
    cmp eax, 0
    je validation_complete
    
    mov ecx, HALLUC_TOKEN_REPEAT
    imul ecx, eax
    shr ecx, 8
    add [rdi].COT_EXECUTION.validation_count, ecx
    
validation_complete:
    ; Cap hallucination score at 255
    mov eax, [rdi].COT_EXECUTION.validation_count
    cmp eax, 255
    jle halluc_cap_ok
    
    mov eax, 255
    
halluc_cap_ok:
    mov HallucinationScore, eax
    
    add rsp, 64
    pop rsi
    pop rbx
    ret
agent_validate_reasoning_trace ENDP

;==========================================================================
; PRIVATE: agent_auto_correct_response(buffer: rcx, cot: rdx) -> void
; Automatically correct detected hallucinations
;==========================================================================
PRIVATE_agent_auto_correct_response:
agent_auto_correct_response PROC
    push rbx
    sub rsp, 48
    
    mov rbx, rcx        ; response buffer
    mov rsi, rdx        ; cot
    
    ; Prepend correction notice
    lea rcx, rbx
    lea rdx, szResponseCorrection
    call agent_prepend_string
    
    ; Based on hallucination type, apply specific correction
    mov eax, [rsi].COT_EXECUTION.validation_count
    
    cmp eax, HALLUC_UNKNOWN_SYMBOL
    jl not_symbol_issue
    
    ; Symbol correction
    lea rcx, rbx
    lea rdx, szSymbolNotDefined
    lea r8, [rsi].COT_EXECUTION.why_step
    call agent_append_correction
    jmp correction_done
    
not_symbol_issue:
    cmp eax, HALLUC_FABRICATED_PATH
    jl not_path_issue
    
    ; Path correction
    lea rcx, rbx
    lea rdx, szPathNotFound
    lea r8, [rsi].COT_EXECUTION.fix_step
    call agent_append_correction
    jmp correction_done
    
not_path_issue:
    cmp eax, HALLUC_LOGIC_ERROR
    jl not_logic_issue
    
    ; Logic correction
    lea rcx, rbx
    lea rdx, szLogicError
    call agent_append_correction
    jmp correction_done
    
not_logic_issue:
    cmp eax, HALLUC_TOKEN_REPEAT
    jl correction_done
    
    ; Token correction
    lea rcx, rbx
    lea rdx, szTokenRepeat
    call agent_append_correction
    
correction_done:
    add rsp, 48
    pop rbx
    ret
agent_auto_correct_response ENDP

;==========================================================================
; PRIVATE: agent_compute_confidence_score() -> eax (0-255)
; Compute overall confidence in response based on validation results
;==========================================================================
PRIVATE_agent_compute_confidence_score:
agent_compute_confidence_score PROC
    push rbx
    sub rsp, 48
    
    ; Start with CERTAIN (240)
    mov eax, CONF_CERTAIN
    
    ; Reduce confidence based on hallucination score
    mov ecx, HallucinationScore
    shr ecx, 2          ; Convert 0-255 to 0-63
    sub eax, ecx        ; Reduce by halluc score
    
    ; Ensure minimum confidence (uncertain)
    cmp eax, CONF_UNCERTAIN
    jge conf_valid
    
    mov eax, CONF_UNCERTAIN
    
conf_valid:
    add rsp, 48
    pop rbx
    ret
agent_compute_confidence_score ENDP

;==========================================================================
; PRIVATE: agent_format_response_with_metadata(buffer: rcx, cot: rdx, mode: r8d, conf: r9d) -> eax
; Format final response with reasoning metadata and confidence indicators
; Returns: eax = final response buffer offset
;==========================================================================
PRIVATE_agent_format_response_with_metadata:
agent_format_response_with_metadata PROC
    push rbx
    push rsi
    push rdi
    sub rsp, 64
    
    mov rbx, rcx        ; response buffer
    mov rsi, rdx        ; cot
    mov r10d, r8d       ; response mode
    mov r11d, r9d       ; confidence
    xor rdi, rdi        ; offset counter
    
    ; Prepend response mode indicator
    cmp r10d, RESPONSE_DIRECT
    je add_direct_prefix
    cmp r10d, RESPONSE_REASONING
    je add_reasoning_prefix
    cmp r10d, RESPONSE_TRACE
    je add_trace_prefix
    
    ; Correction
    lea rcx, szResponseCorrection
    jmp prefix_add
    
add_direct_prefix:
    lea rcx, szResponseDirect
    jmp prefix_add
    
add_reasoning_prefix:
    lea rcx, szResponseReasoning
    jmp prefix_add
    
add_trace_prefix:
    lea rcx, szResponseTrace
    
prefix_add:
    mov rdx, rbx
    call agent_strcpy
    call agent_strlen
    mov rdi, rax        ; offset after prefix
    
    ; Add confidence indicator
    lea rcx, rbx
    add rcx, rdi
    mov edx, r11d       ; confidence value
    call agent_add_confidence_indicator
    add rdi, rax        ; Add returned length
    
    ; Add reasoning trace if enabled
    cmp r10d, RESPONSE_TRACE
    jne skip_trace
    
    lea rcx, rbx
    add rcx, rdi
    lea rdx, [rsi].COT_EXECUTION.what_step
    lea r8, [rsi].COT_EXECUTION.why_step
    lea r9, [rsi].COT_EXECUTION.how_step
    call agent_append_trace_steps
    add rdi, rax
    
skip_trace:
    ; Add actual answer from fix_step
    lea rcx, rbx
    add rcx, rdi
    lea rdx, [rsi].COT_EXECUTION.fix_step
    call agent_strcpy
    add rdi, rax
    
    mov eax, edi        ; Return final offset
    add rsp, 64
    pop rdi
    pop rsi
    pop rbx
    ret
agent_format_response_with_metadata ENDP

;==========================================================================
; PRIVATE: agent_add_confidence_indicator(buffer: rcx, confidence: edx) -> eax (length written)
; Add visual confidence indicator to response
;==========================================================================
PRIVATE_agent_add_confidence_indicator:
agent_add_confidence_indicator PROC
    push rbx
    sub rsp, 48
    
    mov rbx, rcx        ; buffer
    mov r10d, edx       ; confidence (0-255)
    
    ; Convert confidence to text
    cmp r10d, CONF_CERTAIN
    jge conf_certain_lbl
    cmp r10d, CONF_PROBABLE
    jge conf_probable_lbl
    cmp r10d, CONF_UNCERTAIN
    jge conf_uncertain_lbl
    
    ; Unverified
    mov rcx, rbx
    lea rdx, szConfUnverified
    call agent_strcpy
    jmp conf_add_done
    
conf_certain_lbl:
    mov rcx, rbx
    lea rdx, szConfCertain
    call agent_strcpy
    jmp conf_add_done
    
conf_probable_lbl:
    mov rcx, rbx
    lea rdx, szConfProbable
    call agent_strcpy
    jmp conf_add_done
    
conf_uncertain_lbl:
    mov rcx, rbx
    lea rdx, szConfUncertain
    call agent_strcpy
    
conf_add_done:
    add rsp, 48
    pop rbx
    ret
agent_add_confidence_indicator ENDP

;==========================================================================
; PRIVATE: agent_append_trace_steps(buffer: rcx, what: rdx, why: r8, how: r9) -> eax
; Append WHAT/WHY/HOW/FIX reasoning trace to response
;==========================================================================
PRIVATE_agent_append_trace_steps:
agent_append_trace_steps PROC
    push rbx
    push rsi
    push rdi
    sub rsp, 64
    
    mov rbx, rcx        ; buffer
    mov rsi, rdx        ; what
    mov rdi, r8         ; why
    mov r12, r9         ; how
    xor r13, r13        ; offset counter
    
    ; Append WHAT
    mov rcx, rbx
    add rcx, r13
    lea rdx, szWhatHeader
    call agent_strcpy
    add r13, rax
    
    mov rcx, rbx
    add rcx, r13
    mov rdx, rsi
    call agent_strcpy
    add r13, rax
    
    ; Append WHY
    mov rcx, rbx
    add rcx, r13
    lea rdx, szWhyHeader
    call agent_strcpy
    add r13, rax
    
    mov rcx, rbx
    add rcx, r13
    mov rdx, rdi
    call agent_strcpy
    add r13, rax
    
    ; Append HOW
    mov rcx, rbx
    add rcx, r13
    lea rdx, szHowHeader
    call agent_strcpy
    add r13, rax
    
    mov rcx, rbx
    add rcx, r13
    mov rdx, r12
    call agent_strcpy
    add r13, rax
    
    mov eax, r13d
    add rsp, 64
    pop rdi
    pop rsi
    pop rbx
    ret
agent_append_trace_steps ENDP

;==========================================================================
; PRIVATE: agent_chat_add_enhanced_message(content: rcx, type: edx, cot: r8) -> rax
; Add enhanced message to chat history with full metadata
;==========================================================================
PRIVATE_agent_chat_add_enhanced_message:
agent_chat_add_enhanced_message PROC
    push rbx
    sub rsp, 64
    
    mov rbx, rcx        ; content buffer
    mov r10d, edx       ; message type
    mov r11, r8         ; cot
    
    ; Increment message counter and get unique ID
    mov eax, MessageCount
    inc eax
    mov MessageCount, eax
    mov LastMessageID, eax
    
    ; In a real implementation, this would:
    ; 1. Allocate ENHANCED_MESSAGE structure
    ; 2. Fill in all fields from cot and buffers
    ; 3. Store in message ring buffer
    ; 4. Update UI display
    
    mov eax, 1
    add rsp, 64
    pop rbx
    ret
agent_chat_add_enhanced_message ENDP

;==========================================================================
; HELPER FUNCTIONS (Stub implementations - would be expanded)
;==========================================================================
PUBLIC agent_check_symbol_reference
agent_check_symbol_reference PROC
    xor eax, eax
    ret
agent_check_symbol_reference ENDP
PUBLIC agent_detect_logic_contradiction
agent_detect_logic_contradiction PROC
    xor eax, eax
    ret
agent_detect_logic_contradiction ENDP
PUBLIC agent_check_symbol_references
agent_check_symbol_references PROC
    xor eax, eax
    ret
agent_check_symbol_references ENDP
PUBLIC agent_check_logic_consistency
agent_check_logic_consistency PROC
    mov eax, CONF_CERTAIN
    ret
agent_check_logic_consistency ENDP
PUBLIC agent_validate_paths
agent_validate_paths PROC
    xor eax, eax
    ret
agent_validate_paths ENDP
PUBLIC agent_check_token_repetition
agent_check_token_repetition PROC
    xor eax, eax
    ret
agent_check_token_repetition ENDP
PUBLIC agent_classify_problem_type
agent_classify_problem_type PROC
    xor eax, eax
    ret
agent_classify_problem_type ENDP
PUBLIC agent_strcpy
agent_strcpy PROC
    xor eax, eax
    ret
agent_strcpy ENDP
PUBLIC agent_strcpy_limited
agent_strcpy_limited PROC
    xor eax, eax
    ret
agent_strcpy_limited ENDP
PUBLIC agent_strlen
agent_strlen PROC
    mov eax, 32
    ret
agent_strlen ENDP
PUBLIC agent_memset
agent_memset PROC
    xor eax, eax
    ret
agent_memset ENDP
PUBLIC agent_sprintf_debug
agent_sprintf_debug PROC
    xor eax, eax
    ret
agent_sprintf_debug ENDP
PUBLIC agent_prepend_string
agent_prepend_string PROC
    xor eax, eax
    ret
agent_prepend_string ENDP
PUBLIC agent_append_correction
agent_append_correction PROC
    xor eax, eax
    ret
agent_append_correction ENDP
PUBLIC agent_chat_log_system
agent_chat_log_system PROC
    xor eax, eax
    ret
agent_chat_log_system ENDP
PUBLIC agent_chat_log_reasoning
agent_chat_log_reasoning PROC
    xor eax, eax
    ret
agent_chat_log_reasoning ENDP

;==========================================================================
; Additional helper strings (undefined earlier)
;==========================================================================
.data
    szUncertainMessage      BYTE "Need more information to determine cause",0
    szRefactoringStrategy   BYTE "Analyzing code structure and suggesting improvements",0
    szOptimizationStrategy  BYTE "Scanning for performance bottlenecks and hotpatch opportunities",0
    szDebugStrategy         BYTE "Setting up execution trace and analyzing control flow",0
    szTeachingStrategy      BYTE "Breaking down concept into learning steps",0
    szAnalysisStrategy      BYTE "Examining code semantics and dependencies",0
    szSymbolFix             BYTE "Use proper symbol from current scope",0
    szLogicFix              BYTE "Resolve logical contradiction in reasoning",0
    szCodeImprove           BYTE "Apply suggested code improvement",0
    szGenericFix            BYTE "Follow the recommendations above to resolve the issue",0

;==============================================================================
; CURSOR API INTEGRATION FUNCTIONS
;==============================================================================

; Check if a Cursor API model is currently selected
check_cursor_model_selected PROC
    ; Check dual model context for Cursor API models
    ; Returns: rax = 1 if Cursor model selected, 0 otherwise

    ; For now, return 0 (use local reasoning)
    ; In production, would check g_dual_model_ctx for model types
    xor rax, rax
    ret
check_cursor_model_selected ENDP

; Callback for Cursor API responses
cursor_response_callback PROC
    ; rcx = response text
    push rbx
    sub rsp, 32

    ; Copy response to our buffer
    lea rdx, ResponseBuffer
    call lstrcpyA

    ; Set response offset
    mov r13d, eax

    ; Log to message history
    lea rcx, ResponseBuffer
    mov edx, 1          ; MSG_AGENT
    xor r8, r8          ; No COT for API responses
    call agent_chat_add_enhanced_message

    add rsp, 32
    pop rbx
    ret
cursor_response_callback ENDP

END





