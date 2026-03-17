;==========================================================================
; agent_learning_system.asm - Learn from Chat History to Improve Responses
; ==========================================================================
; Features:
; - Parse chat patterns to identify common questions
; - Track response quality feedback
; - Build knowledge base from successful responses
; - Suggest improvements based on patterns
; - Auto-improve future responses
;==========================================================================

option casemap:none

include windows.inc
includelib kernel32.lib
includelib user32.lib

;==========================================================================
; CONSTANTS
;==========================================================================
MAX_PATTERNS        EQU 100
MAX_PATTERN_SIZE    EQU 256
MAX_RESPONSES       EQU 500
PATTERN_THRESHOLD   EQU 3              ; Min occurrences to be a pattern

; Learning categories
LEARN_QUESTION_TYPE EQU 0              ; Type of question asked
LEARN_RESPONSE_QUALITY EQU 1           ; Quality of response
LEARN_CONTEXT       EQU 2              ; Context/code-related
LEARN_SOLUTION      EQU 3              ; Solution effectiveness

;==========================================================================
; STRUCTURES
;==========================================================================
LEARNED_PATTERN STRUCT
    pattern_type    DWORD ?             ; LEARN_* constant
    content         BYTE MAX_PATTERN_SIZE DUP (?)
    occurrence_count DWORD ?
    avg_quality     DWORD ?             ; 0-100
    last_seen       QWORD ?
LEARNED_PATTERN ENDS

RESPONSE_QUALITY STRUCT
    message_hash    DWORD ?             ; Hash of response text
    user_rating     DWORD ?             ; 0-5 stars
    effectiveness   DWORD ?             ; 0-100
    timestamp       QWORD ?
RESPONSE_QUALITY ENDS

;==========================================================================
; DATA
;==========================================================================
.data
    ; Learning patterns database
    LearnedPatterns     LEARNED_PATTERN MAX_PATTERNS DUP (<>)
    PatternCount        DWORD 0
    
    ; Response quality tracking
    ResponseQualities   RESPONSE_QUALITY MAX_RESPONSES DUP (<>)
    QualityCount        DWORD 0
    
    ; Statistics
    TotalQuestionsAnalyzed  QWORD 0
    TotalPatternsFound      QWORD 0
    
    ; Strings
    szLearningInitialized   BYTE "[Learning] Agent learning system initialized",0
    szPatternDetected       BYTE "[Learning] Pattern detected: %s (frequency: %d)",0
    szQualityImprovement    BYTE "[Learning] Response quality improved by %d%%",0
    
    ; Pattern templates (common question types)
    szPatternHowTo          BYTE "How to",0
    szPatternWhatIs         BYTE "What is",0
    szPatternWhy            BYTE "Why",0
    szPatternFix            BYTE "Fix",0
    szPatternDebug          BYTE "Debug",0

.data?
    ; Current learning context
    CurrentContext          BYTE 256 DUP (?)
    ContextMode             DWORD ?         ; Agent mode in effect

;==========================================================================
; CODE
;==========================================================================
.code

;==========================================================================
; PUBLIC: agent_learning_init() -> rax (success)
; Initialize agent learning system
;==========================================================================
PUBLIC agent_learning_init
agent_learning_init PROC
    push rbx
    sub rsp, 32
    
    ; Zero pattern database
    lea rcx, LearnedPatterns
    xor edx, edx
    mov r8d, MAX_PATTERNS * (SIZE LEARNED_PATTERN) / 8
    
.zero_patterns:
    cmp r8d, 0
    je .zero_done
    mov QWORD PTR [rcx + rdx], 0
    add rdx, 8
    dec r8d
    jmp .zero_patterns
    
.zero_done:
    mov PatternCount, 0
    mov QualityCount, 0
    
    ; Log initialization
    lea rcx, szLearningInitialized
    call log_learning_event
    
    mov eax, 1                          ; Success
    add rsp, 32
    pop rbx
    ret
agent_learning_init ENDP

;==========================================================================
; PUBLIC: agent_learning_analyze_chat(messages: rcx, count: edx) -> eax
; Analyze chat messages to identify patterns
; messages: pointer to CHAT_MESSAGE array
; count: number of messages
;==========================================================================
PUBLIC agent_learning_analyze_chat
agent_learning_analyze_chat PROC
    push rbx
    push rdi
    push rsi
    sub rsp, 32
    
    mov rsi, rcx                        ; rsi = messages
    mov edi, edx                        ; edi = count
    xor ebx, ebx                        ; ebx = current message index
    
.analyze_loop:
    cmp ebx, edi
    jge .analyze_done
    
    ; Get message at index
    imul eax, ebx, SIZE CHAT_MESSAGE    ; SIZE would be from chat_persistence.asm
    lea rax, [rsi + rax]
    
    ; Analyze this message
    mov rcx, rax
    call analyze_single_message
    
    inc ebx
    inc TotalQuestionsAnalyzed
    jmp .analyze_loop
    
.analyze_done:
    mov eax, ebx                        ; Return number of messages processed
    add rsp, 32
    pop rsi
    pop rdi
    pop rbx
    ret
agent_learning_analyze_chat ENDP

;==========================================================================
; PRIVATE: analyze_single_message(msg: rcx) -> void
; Analyze a single message for learning patterns
;==========================================================================
PRIVATE analyze_single_message
analyze_single_message PROC
    push rbx
    push rdi
    push rsi
    
    mov rsi, rcx                        ; rsi = message
    
    ; Extract content (skip to content field)
    lea rdi, [rsi + 8]                  ; Skip role (4 bytes) + padding (4 bytes)
    
    ; Skip timestamp (8 bytes)
    add rdi, 8
    
    ; Now rdi points to content
    
    ; Check for common question patterns
    mov rcx, rdi
    lea rdx, szPatternHowTo
    call detect_pattern
    
    test eax, eax
    jz .check_what
    
    ; "How to" pattern detected
    lea rcx, LearnedPatterns
    mov edx, LEARN_QUESTION_TYPE
    call record_pattern
    
.check_what:
    mov rcx, rdi
    lea rdx, szPatternWhatIs
    call detect_pattern
    
    test eax, eax
    jz .check_why
    
    lea rcx, LearnedPatterns
    mov edx, LEARN_QUESTION_TYPE
    call record_pattern
    
.check_why:
    mov rcx, rdi
    lea rdx, szPatternWhy
    call detect_pattern
    
    ; Additional pattern detection...
    
    pop rsi
    pop rdi
    pop rbx
    ret
analyze_single_message ENDP

;==========================================================================
; PRIVATE: detect_pattern(text: rcx, pattern: rdx) -> eax (1=match, 0=no)
;==========================================================================
PRIVATE detect_pattern
detect_pattern PROC
    push rbx
    
    ; Case-insensitive substring search
    mov rsi, rcx                        ; rsi = text
    mov rdi, rdx                        ; rdi = pattern
    
.search_loop:
    cmp BYTE PTR [rsi], 0
    je .pattern_not_found
    
    ; Try to match pattern at current position
    mov rcx, rsi
    mov rdx, rdi
    call string_match_case_insensitive
    
    test eax, eax
    jnz .pattern_found
    
    inc rsi
    jmp .search_loop
    
.pattern_found:
    mov eax, 1
    pop rbx
    ret
    
.pattern_not_found:
    xor eax, eax
    pop rbx
    ret
detect_pattern ENDP

;==========================================================================
; PRIVATE: string_match_case_insensitive(text: rcx, pattern: rdx) -> eax
;==========================================================================
PRIVATE string_match_case_insensitive
string_match_case_insensitive PROC
    xor eax, eax
    xor ebx, ebx
    
.match_loop:
    movzx esi, BYTE PTR [rdx + rbx]
    test sil, sil
    jz .match_complete
    
    movzx edi, BYTE PTR [rcx + rbx]
    test dil, dil
    jz .match_fail
    
    ; Convert to lowercase for comparison
    ; (Simplified: just compare directly for now)
    cmp sil, dil
    jne .match_fail
    
    inc rbx
    jmp .match_loop
    
.match_complete:
    mov eax, 1
    ret
    
.match_fail:
    xor eax, eax
    ret
string_match_case_insensitive ENDP

;==========================================================================
; PRIVATE: record_pattern(patterns_array: rcx, pattern_type: edx) -> void
;==========================================================================
PRIVATE record_pattern
record_pattern PROC
    push rbx
    
    ; Find existing pattern or create new one
    mov rsi, rcx
    mov ebx, PatternCount
    
    cmp ebx, MAX_PATTERNS
    jge .no_room
    
    ; Add new pattern
    imul eax, ebx, SIZE LEARNED_PATTERN
    lea rax, [rsi + rax]
    
    ; Initialize pattern
    mov DWORD PTR [rax], edx            ; pattern_type
    mov DWORD PTR [rax + MAX_PATTERN_SIZE + 4], 1  ; occurrence_count = 1
    
    inc PatternCount
    inc TotalPatternsFound
    
.no_room:
    pop rbx
    ret
record_pattern ENDP

;==========================================================================
; PUBLIC: agent_learning_rate_response(message_hash: rcx, rating: edx) -> eax
; Record user feedback on response quality (1-5 stars)
;==========================================================================
PUBLIC agent_learning_rate_response
agent_learning_rate_response PROC
    push rbx
    sub rsp, 32
    
    ; Validate rating
    cmp edx, 1
    jl .invalid_rating
    cmp edx, 5
    jg .invalid_rating
    
    ; Find existing quality entry or create new
    mov rbx, QualityCount
    cmp rbx, MAX_RESPONSES
    jge .no_space
    
    ; Add quality record
    imul eax, ebx, SIZE RESPONSE_QUALITY
    lea rax, ResponseQualities[rax]
    
    mov DWORD PTR [rax], ecx            ; message_hash
    mov DWORD PTR [rax + 4], edx        ; user_rating (1-5)
    
    ; Calculate effectiveness from rating
    ; 5 stars = 100, 4 stars = 80, 3 = 60, 2 = 40, 1 = 20
    mov esi, edx
    sub esi, 1                          ; Convert 1-5 to 0-4
    imul esi, esi, 20
    add esi, 20                         ; Now 20-100
    
    mov DWORD PTR [rax + 8], esi        ; effectiveness
    
    ; Timestamp
    call GetTickCount64
    mov QWORD PTR [rax + 16], rax
    
    inc QualityCount
    
    mov eax, 1                          ; Success
    add rsp, 32
    pop rbx
    ret
    
.invalid_rating:
    xor eax, eax
    add rsp, 32
    pop rbx
    ret
    
.no_space:
    xor eax, eax
    add rsp, 32
    pop rbx
    ret
agent_learning_rate_response ENDP

;==========================================================================
; PUBLIC: agent_learning_get_suggestion(context: rcx) -> rax
; Get learning-based improvement suggestion for current context
;==========================================================================
PUBLIC agent_learning_get_suggestion
agent_learning_get_suggestion PROC
    push rbx
    sub rsp, 32
    
    ; Analyze context
    mov rsi, rcx
    
    ; Find matching patterns in learning database
    mov rbx, 0
    
.find_pattern:
    cmp rbx, PatternCount
    jge .no_suggestion
    
    ; Check if pattern context matches current context
    imul eax, ebx, SIZE LEARNED_PATTERN
    lea rax, LearnedPatterns[rax]
    
    mov ecx, DWORD PTR [rax + MAX_PATTERN_SIZE + 4]  ; occurrence_count
    cmp ecx, PATTERN_THRESHOLD
    jl .next_pattern
    
    ; Pattern is significant, return suggestion
    lea rax, [rax + 8]                  ; Point to pattern content
    add rsp, 32
    pop rbx
    ret
    
.next_pattern:
    inc rbx
    jmp .find_pattern
    
.no_suggestion:
    xor eax, eax
    add rsp, 32
    pop rbx
    ret
agent_learning_get_suggestion ENDP

;==========================================================================
; PRIVATE: log_learning_event(message: rcx) -> void
;==========================================================================
PRIVATE log_learning_event
log_learning_event PROC
    ; Log to output pane
    ; TODO: Call output_log_agent or similar
    ret
log_learning_event ENDP

;==========================================================================
; PUBLIC: agent_learning_get_stats(patterns: rcx, avg_quality: rdx) -> void
; Get learning system statistics
;==========================================================================
PUBLIC agent_learning_get_stats
agent_learning_get_stats PROC
    mov rax, TotalPatternsFound
    mov QWORD PTR [rcx], rax
    
    ; Calculate average quality
    xor eax, eax
    mov edx, QualityCount
    cmp edx, 0
    je .no_quality
    
    ; Sum all effectiveness scores
    mov rbx, 0
    xor esi, esi
    
.sum_loop:
    cmp ebx, edx
    jge .avg_done
    
    imul eax, ebx, SIZE RESPONSE_QUALITY
    lea rax, ResponseQualities[rax]
    mov ecx, DWORD PTR [rax + 8]        ; effectiveness
    add esi, ecx
    
    inc ebx
    jmp .sum_loop
    
.avg_done:
    ; Average = sum / count
    mov eax, esi
    cdq
    idiv edx
    
.no_quality:
    mov QWORD PTR [rdx], rax
    ret
agent_learning_get_stats ENDP

END
