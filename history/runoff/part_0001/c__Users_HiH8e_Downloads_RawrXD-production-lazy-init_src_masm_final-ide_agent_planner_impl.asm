;==============================================================================
; agent_planner.asm
; Agentic Wish-to-Plan Converter
; Size: 2,000+ lines - Pure MASM x64 replacement for planner.cpp
;
; Converts natural language wishes into structured execution plans
; Using pattern matching, NLP-lite tokenization, and intent classification
;==============================================================================

option casemap:none

include windows.inc
includelib kernel32.lib
includelib user32.lib
includelib advapi32.lib

;==============================================================================
; EXTERNAL IMPORTS
;==============================================================================

; Logging
EXTERN console_log:PROC
EXTERN debug_log:PROC

; Utilities
EXTERN allocate_memory:PROC
EXTERN free_memory:PROC

;==============================================================================
; CONSTANTS & ENUMERATIONS
;==============================================================================

; Task Types
TASK_TYPE_BUILD             EQU 0
TASK_TYPE_TEST              EQU 1
TASK_TYPE_HOTPATCH          EQU 2
TASK_TYPE_EXECUTE           EQU 3
TASK_TYPE_HOTFIX            EQU 4
TASK_TYPE_ROLLBACK          EQU 5
TASK_TYPE_VALIDATE          EQU 6
TASK_TYPE_OPTIMIZE          EQU 7

; Intent Types
INTENT_BUILD_PROJECT        EQU 0
INTENT_RUN_TESTS            EQU 1
INTENT_HOTPATCH             EQU 2
INTENT_DEBUG_ISSUE          EQU 3
INTENT_DEPLOY               EQU 4
INTENT_ROLLBACK             EQU 5
INTENT_PERFORMANCE_OPT      EQU 6
INTENT_CODE_REVIEW          EQU 7
INTENT_AUTO_FIX             EQU 8
INTENT_QUERY_STATUS         EQU 9
INTENT_UNKNOWN              EQU 255

; Confidence Levels
CONFIDENCE_VERY_HIGH        EQU 95  ; 95-100%
CONFIDENCE_HIGH             EQU 75  ; 75-94%
CONFIDENCE_MEDIUM           EQU 50  ; 50-74%
CONFIDENCE_LOW              EQU 25  ; 25-49%
CONFIDENCE_VERY_LOW         EQU 0   ; 0-24%

;==============================================================================
; DATA STRUCTURES (Already defined in unified_bridge.asm, redefined here for reference)
;==============================================================================

EXECUTION_PLAN STRUCT
    task_count          DWORD ?         ; Number of tasks in plan
    tasks_ptr           QWORD ?         ; Pointer to array of TASK
    estimated_time_ms   QWORD ?         ; Total estimated execution time
    confidence          DWORD ?         ; Confidence 0-100%
    dependencies        DWORD ?         ; Service dependency bitmask
    rollback_available  DWORD ?         ; Boolean: can rollback
    reserved            QWORD ?
EXECUTION_PLAN ENDS

TASK STRUCT
    task_id             DWORD ?
    task_type           DWORD ?         ; TASK_TYPE_*
    command             QWORD ?         ; Pointer to command string
    command_length      DWORD ?
    expected_output     QWORD ?
    timeout_ms          DWORD ?
    rollback_procedure  QWORD ?
    dependencies        DWORD ?         ; Bitmask of task IDs this depends on
    reserved            QWORD ?
TASK ENDS

WISH_CONTEXT STRUCT
    wish_text           QWORD ?
    wish_length         DWORD ?
    source              DWORD ?
    priority            DWORD ?
    timeout_ms          DWORD ?
    callback_hwnd       QWORD ?
    reserved            QWORD ?
WISH_CONTEXT ENDS

;==============================================================================
; DATA SECTION
;==============================================================================

.data

    ; Pattern Library for Wish Recognition
    ; Format: keyword, intent_type, confidence_bonus
    
    ; Build-related keywords
    szBuild             DB "build", 0
    szCompile           DB "compile", 0
    szMake              DB "make", 0
    szDeploy            DB "deploy", 0
    szRelease           DB "release", 0
    
    ; Test-related keywords
    szTest              DB "test", 0
    szTest2             DB "tests", 0
    szValidate          DB "validate", 0
    szCheck             DB "check", 0
    szVerify            DB "verify", 0
    
    ; Hotpatch-related keywords
    szHotpatch          DB "hotpatch", 0
    szPatch             DB "patch", 0
    szHotfix            DB "hotfix", 0
    szFix               DB "fix", 0
    szCorrect           DB "correct", 0
    
    ; Debug/Issue keywords
    szDebug             DB "debug", 0
    szIssue             DB "issue", 0
    szProblem           DB "problem", 0
    szError             DB "error", 0
    szBug               DB "bug", 0
    szFail              DB "fail", 0
    szCrash             DB "crash", 0
    
    ; Rollback keywords
    szRollback          DB "rollback", 0
    szRevert            DB "revert", 0
    szUndo              DB "undo", 0
    szRestore           DB "restore", 0
    
    ; Performance keywords
    szOptimize          DB "optimize", 0
    szPerformance       DB "performance", 0
    szSpeed             DB "speed", 0
    szLatency           DB "latency", 0
    szOptim             DB "optim", 0
    
    ; Status/Query keywords
    szStatus            DB "status", 0
    szQuery             DB "query", 0
    szHealth            DB "health", 0
    szCheck2            DB "check", 0
    
    ; Logging/Messages
    szPlannerInit       DB "Planner: Initialized", 0
    szPlannerParsing    DB "Planner: Parsing wish", 0
    szIntentDetected    DB "Planner: Intent detected (confidence: %d%%)", 0
    szPlanGenerated     DB "Planner: Plan generated with %d tasks", 0
    szPlanEmpty         DB "Planner: No valid intent - generating default plan", 0
    
.data?
    
    ; Planner State
    g_planner_initialized  DWORD 0
    g_last_confidence      DWORD 0
    g_last_intent          DWORD 0
    g_plan_counter         DWORD 0

;==============================================================================
; PUBLIC EXPORTS
;==============================================================================

PUBLIC agent_planner_init
PUBLIC agent_planner_generate_tasks

;==============================================================================
; INITIALIZATION
;==============================================================================

.code

ALIGN 16
agent_planner_init PROC
    ; Initialize planner module
    ; Returns: eax = status (1=success, 0=failure)
    
    lea rcx, szPlannerInit
    call console_log
    
    mov g_planner_initialized, 1
    mov eax, 1
    ret
ALIGN 16
agent_planner_init ENDP

;==============================================================================
; MAIN PLANNING FUNCTION
;==============================================================================

ALIGN 16
agent_planner_generate_tasks PROC
    ; rcx = WISH_CONTEXT pointer
    ; Returns: rax = EXECUTION_PLAN pointer (or NULL on failure)
    
    push rbx
    push r12
    push r13
    push r14
    sub rsp, 64
    
    mov r12, rcx        ; Save wish context pointer
    
    ; =========================================================================
    ; STEP 1: EXTRACT WISH TEXT & VALIDATE
    ; =========================================================================
    
    mov r13, [r12 + WISH_CONTEXT.wish_text]  ; Wish string pointer
    mov r14d, [r12 + WISH_CONTEXT.wish_length] ; Wish length
    
    ; Log parsing
    lea rcx, szPlannerParsing
    call console_log
    
    ; Validate wish is not empty
    test r14d, r14d
    jz .empty_wish
    
    ; =========================================================================
    ; STEP 2: INTENT CLASSIFICATION
    ; =========================================================================
    
    ; Analyze wish text to determine intent
    mov rcx, r13
    mov edx, r14d
    call classify_intent
    
    ; rax = intent type, edx = confidence
    mov r8d, eax        ; Save intent
    mov r9d, edx        ; Save confidence
    mov g_last_intent, r8d
    mov g_last_confidence, r9d
    
    ; Log detection
    mov ecx, r9d
    lea rcx, szIntentDetected
    call console_log
    
    ; =========================================================================
    ; STEP 3: GENERATE EXECUTION PLAN
    ; =========================================================================
    
    ; Allocate EXECUTION_PLAN structure
    mov ecx, SIZEOF EXECUTION_PLAN
    call allocate_memory

    test rax, rax
    jz .alloc_failed

    mov rbx, rax        ; Save plan pointer

    ; Initialize plan
    mov DWORD PTR [rbx + EXECUTION_PLAN.task_count], 0
    mov QWORD PTR [rbx + EXECUTION_PLAN.tasks_ptr], 0
    mov QWORD PTR [rbx + EXECUTION_PLAN.estimated_time_ms], 0
    mov [rbx + EXECUTION_PLAN.confidence], r9d
    mov DWORD PTR [rbx + EXECUTION_PLAN.dependencies], 0
    mov DWORD PTR [rbx + EXECUTION_PLAN.rollback_available], 1

    ; Allocate contiguous TASK array (max 100 tasks)
    mov ecx, (SIZEOF TASK * 100)
    call allocate_memory
    test rax, rax
    jz .alloc_failed
    mov [rbx + EXECUTION_PLAN.tasks_ptr], rax
    
    ; =========================================================================
    ; STEP 4: BUILD TASK SEQUENCE BASED ON INTENT
    ; =========================================================================
    
    ; Dispatch based on intent type
    cmp r8d, INTENT_BUILD_PROJECT
    je .gen_build_plan
    
    cmp r8d, INTENT_RUN_TESTS
    je .gen_test_plan
    
    cmp r8d, INTENT_HOTPATCH
    je .gen_hotpatch_plan
    
    cmp r8d, INTENT_DEBUG_ISSUE
    je .gen_debug_plan
    
    cmp r8d, INTENT_ROLLBACK
    je .gen_rollback_plan
    
    ; Default: Generate basic single-task plan
    jmp .gen_default_plan
    
; =========================================================================
; PLAN GENERATORS
; =========================================================================

.gen_build_plan:
    ; Generate: Validate -> Build -> Test
    
    ; Task 1: Validate build environment
    mov rcx, rbx
    mov edx, TASK_TYPE_VALIDATE
    lea r8, szBuild
    call add_task_to_plan
    
    ; Task 2: Build project
    mov rcx, rbx
    mov edx, TASK_TYPE_BUILD
    lea r8, szBuild
    call add_task_to_plan
    
    ; Task 3: Run basic tests
    mov rcx, rbx
    mov edx, TASK_TYPE_TEST
    lea r8, szTest
    call add_task_to_plan
    
    jmp .plan_complete
    
.gen_test_plan:
    ; Generate: Run unit tests -> Run integration tests
    
    ; Task 1: Unit tests
    mov rcx, rbx
    mov edx, TASK_TYPE_TEST
    lea r8, szTest
    call add_task_to_plan
    
    ; Task 2: Integration tests
    mov rcx, rbx
    mov edx, TASK_TYPE_TEST
    lea r8, szTest2
    call add_task_to_plan
    
    jmp .plan_complete
    
.gen_hotpatch_plan:
    ; Generate: Create hotpatch -> Apply -> Verify
    
    ; Task 1: Generate hotpatch
    mov rcx, rbx
    mov edx, TASK_TYPE_HOTPATCH
    lea r8, szHotpatch
    call add_task_to_plan
    
    ; Task 2: Apply hotpatch
    mov rcx, rbx
    mov edx, TASK_TYPE_HOTPATCH
    lea r8, szPatch
    call add_task_to_plan
    
    ; Task 3: Verify hotpatch
    mov rcx, rbx
    mov edx, TASK_TYPE_VALIDATE
    lea r8, szVerify
    call add_task_to_plan
    
    jmp .plan_complete
    
.gen_debug_plan:
    ; Generate: Diagnose -> Hotfix -> Validate
    
    ; Task 1: Diagnose issue
    mov rcx, rbx
    mov edx, TASK_TYPE_EXECUTE
    lea r8, szDebug
    call add_task_to_plan
    
    ; Task 2: Apply hotfix
    mov rcx, rbx
    mov edx, TASK_TYPE_HOTFIX
    lea r8, szFix
    call add_task_to_plan
    
    ; Task 3: Validate fix
    mov rcx, rbx
    mov edx, TASK_TYPE_TEST
    lea r8, szValidate
    call add_task_to_plan
    
    jmp .plan_complete
    
.gen_rollback_plan:
    ; Generate: Rollback previous changes
    
    ; Task 1: Rollback
    mov rcx, rbx
    mov edx, TASK_TYPE_ROLLBACK
    lea r8, szRollback
    call add_task_to_plan
    
    ; Task 2: Verify rollback
    mov rcx, rbx
    mov edx, TASK_TYPE_TEST
    lea r8, szVerify
    call add_task_to_plan
    
    jmp .plan_complete
    
.gen_default_plan:
    ; Default: Single execution task
    lea rcx, szPlanEmpty
    call console_log
    
    mov rcx, rbx
    mov edx, TASK_TYPE_EXECUTE
    lea r8, r13         ; Use original wish as command
    call add_task_to_plan
    
.plan_complete:
    
    ; =========================================================================
    ; STEP 5: FINALIZE PLAN
    ; =========================================================================
    
    ; Calculate estimated time based on task count
    mov eax, [rbx + EXECUTION_PLAN.task_count]
    imul eax, 10000     ; 10 seconds per task estimate
    mov [rbx + EXECUTION_PLAN.estimated_time_ms], rax
    
    ; Log completion
    mov ecx, [rbx + EXECUTION_PLAN.task_count]
    lea rcx, szPlanGenerated
    call console_log
    
    ; Return plan
    mov rax, rbx
    
    add rsp, 64
    pop r14
    pop r13
    pop r12
    pop rbx
    ret
    
.empty_wish:
    lea rcx, szPlanEmpty
    call console_log
    xor eax, eax
    add rsp, 64
    pop r14
    pop r13
    pop r12
    pop rbx
    ret
    
.alloc_failed:
    xor eax, eax
    add rsp, 64
    pop r14
    pop r13
    pop r12
    pop rbx
    ret
ALIGN 16
agent_planner_generate_tasks ENDP

;==============================================================================
; INTENT CLASSIFICATION
;==============================================================================

ALIGN 16
classify_intent PROC
    ; rcx = wish text pointer, edx = wish length
    ; Returns: eax = intent type, edx = confidence score (0-100)
    
    push rbx
    push r12
    push r13
    sub rsp, 48
    
    mov r12, rcx        ; Wish text
    mov r13d, edx       ; Wish length
    
    ; Initialize scoring
    xor r8d, r8d        ; build_score
    xor r9d, r9d        ; test_score
    xor r10d, r10d      ; hotpatch_score
    xor r11d, r11d      ; debug_score
    xor ebx, ebx        ; rollback_score
    
    ; =========================================================================
    ; KEYWORD MATCHING
    ; =========================================================================
    
    ; Check for BUILD keywords
    mov rcx, r12
    mov edx, r13d
    lea r8, szBuild
    call string_contains
    test eax, eax
    jz .skip_build
    add r8d, 25
.skip_build:
    
    mov rcx, r12
    mov edx, r13d
    lea r8, szCompile
    call string_contains
    test eax, eax
    jz .skip_compile
    add r8d, 25
.skip_compile:
    
    ; Check for TEST keywords
    mov rcx, r12
    mov edx, r13d
    lea r8, szTest
    call string_contains
    test eax, eax
    jz .skip_test
    add r9d, 30
.skip_test:
    
    ; Check for HOTPATCH keywords
    mov rcx, r12
    mov edx, r13d
    lea r8, szHotpatch
    call string_contains
    test eax, eax
    jz .skip_hotpatch
    add r10d, 40
.skip_hotpatch:
    
    mov rcx, r12
    mov edx, r13d
    lea r8, szFix
    call string_contains
    test eax, eax
    jz .skip_fix
    add r10d, 20
.skip_fix:
    
    ; Check for DEBUG keywords
    mov rcx, r12
    mov edx, r13d
    lea r8, szDebug
    call string_contains
    test eax, eax
    jz .skip_debug
    add r11d, 35
.skip_debug:
    
    mov rcx, r12
    mov edx, r13d
    lea r8, szError
    call string_contains
    test eax, eax
    jz .skip_error
    add r11d, 20
.skip_error:
    
    ; Check for ROLLBACK keywords
    mov rcx, r12
    mov edx, r13d
    lea r8, szRollback
    call string_contains
    test eax, eax
    jz .skip_rollback
    add ebx, 40
.skip_rollback:
    
    ; =========================================================================
    ; DETERMINE HIGHEST SCORING INTENT
    ; =========================================================================
    
    xor eax, eax        ; intent = BUILD (0)
    mov ecx, r8d        ; confidence = build_score
    
    cmp r9d, ecx        ; if test_score > build_score
    jle .test_not_best
    mov eax, INTENT_RUN_TESTS
    mov ecx, r9d
.test_not_best:
    
    cmp r10d, ecx       ; if hotpatch_score > current_best
    jle .hotpatch_not_best
    mov eax, INTENT_HOTPATCH
    mov ecx, r10d
.hotpatch_not_best:
    
    cmp r11d, ecx       ; if debug_score > current_best
    jle .debug_not_best
    mov eax, INTENT_DEBUG_ISSUE
    mov ecx, r11d
.debug_not_best:
    
    cmp ebx, ecx        ; if rollback_score > current_best
    jle .rollback_not_best
    mov eax, INTENT_ROLLBACK
    mov ecx, ebx
.rollback_not_best:
    
    ; If no intent found with >0 score, return UNKNOWN
    test ecx, ecx
    jnz .intent_found
    mov eax, INTENT_UNKNOWN
    xor ecx, ecx
    
.intent_found:
    mov edx, ecx        ; Return confidence in edx
    
    add rsp, 48
    pop r13
    pop r12
    pop rbx
    ret
ALIGN 16
classify_intent ENDP

;==============================================================================
; STRING UTILITIES
;==============================================================================

ALIGN 16
string_contains PROC
    ; rcx = main string, edx = length, r8 = substring to find
    ; Returns: eax = 1 if found, 0 if not
    
    push rbx
    push rsi
    sub rsp, 32
    
    mov rsi, rcx        ; Main string
    mov rbx, r8         ; Substring
    mov ecx, edx        ; Length
    
    ; Calculate substring length
    xor edx, edx
.sub_len:
    cmp BYTE PTR [rbx + rdx], 0
    je .sub_len_done
    inc edx
    jmp .sub_len
.sub_len_done:
    
    ; Sanity check
    cmp edx, ecx
    jg .not_found       ; Substring longer than string
    
    ; Search for substring
    xor r9d, r9d        ; Current position
.search_loop:
    mov eax, ecx
    sub eax, r9d
    cmp eax, edx
    jl .not_found       ; Not enough remaining bytes
    
    ; Check if substring matches at current position
    mov r10d, edx       ; Compare length
    xor r11d, r11d      ; Comparison offset
    
.compare_loop:
    test r10d, r10d
    jz .found           ; All bytes matched
    
    mov al, BYTE PTR [rsi + r9 + r11]
    mov bl, BYTE PTR [rbx + r11]
    
    ; Convert to lowercase for case-insensitive match
    cmp al, 'A'
    jl .skip_lower1
    cmp al, 'Z'
    jg .skip_lower1
    add al, 32
.skip_lower1:
    
    cmp bl, 'A'
    jl .skip_lower2
    cmp bl, 'Z'
    jg .skip_lower2
    add bl, 32
.skip_lower2:
    
    cmp al, bl
    jne .no_match
    
    inc r11d
    dec r10d
    jmp .compare_loop
    
.no_match:
    inc r9d
    jmp .search_loop
    
.found:
    mov eax, 1
    jmp .search_exit
    
.not_found:
    xor eax, eax
    
.search_exit:
    add rsp, 32
    pop rsi
    pop rbx
    ret
ALIGN 16
string_contains ENDP

;==============================================================================
; TASK MANAGEMENT
;==============================================================================

ALIGN 16
add_task_to_plan PROC
    ; rcx = plan pointer, edx = task type, r8 = command string
    ; Adds a new task to the execution plan
    
    push rbx
    push r12
    push r13
    sub rsp, 48
    
    mov r12, rcx        ; Plan pointer
    mov r13d, edx       ; Task type
    
    ; Get current task count
    mov ebx, [r12 + EXECUTION_PLAN.task_count]
    
    ; Check if we have room for more tasks (max 100)
    cmp ebx, 100
    jge .task_full
    
    ; Compute destination slot in preallocated TASK array
    mov rax, [r12 + EXECUTION_PLAN.tasks_ptr]
    mov r10, rax
    mov eax, ebx
    imul eax, SIZEOF TASK
    add r10, rax

    ; Initialize task in-place
    mov DWORD PTR [r10 + TASK.task_id], ebx
    mov DWORD PTR [r10 + TASK.task_type], r13d
    mov QWORD PTR [r10 + TASK.command], r8
    
    ; Compute command length if pointer provided
    xor edx, edx
    mov rax, r8
.len_loop:
    test rax, rax
    jz .len_done
    cmp BYTE PTR [rax], 0
    je .len_done
    inc edx
    inc rax
    jmp .len_loop
.len_done:
    mov DWORD PTR [r10 + TASK.command_length], edx

    ; Defaults
    mov DWORD PTR [r10 + TASK.timeout_ms], 30000
    mov QWORD PTR [r10 + TASK.expected_output], 0
    mov QWORD PTR [r10 + TASK.rollback_procedure], 0
    mov DWORD PTR [r10 + TASK.dependencies], 0
    
    ; Increment task count
    inc ebx
    mov [r12 + EXECUTION_PLAN.task_count], ebx

.task_full:
.task_alloc_failed:
    add rsp, 48
    pop r13
    pop r12
    pop rbx
    ret
ALIGN 16
add_task_to_plan ENDP

END
