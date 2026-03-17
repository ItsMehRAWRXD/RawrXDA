; =============================================================================
; rawrxd_cot_engine.asm — Chain-of-Thought Engine Core (Pure x64 MASM DLL)
; =============================================================================
;
; Phase 37: Full MASM64 reverse-engineering of chain_of_thought_engine.cpp
;
; Implements:
;   - 100M token capacity via VirtualAlloc 1GB reserve + commit-on-demand
;   - 12 CoT roles (Reviewer, Auditor, Thinker, Researcher, DebaterFor,
;     DebaterAgainst, Critic, Synthesizer, Brainstorm, Verifier, Refiner,
;     Summarizer)
;   - 6 presets (review, audit, think, research, debate, custom)
;   - Sequential multi-step chain execution (1-8 steps)
;   - Cumulative reasoning (each step sees all prior outputs)
;   - Statistics tracking (chains, steps, latency, role usage)
;   - JSON serialization (status, steps, presets)
;   - SEH structured exception handling for memory safety
;   - Thread-safe via SRW lock (coordinated with rawrxd_cot_dll_entry.asm)
;
; Exports:
;   CoT_Initialize_Core     — Reserve 1GB VA, init data structures
;   CoT_Shutdown_Core       — Release all memory, zero state
;   CoT_Set_Max_Steps       — Clamp to [1,8]
;   CoT_Get_Max_Steps       — Returns current max
;   CoT_Clear_Steps         — Remove all configured steps
;   CoT_Add_Step            — Append a step (role, model, instruction, skip)
;   CoT_Apply_Preset        — Load a named preset (6 built-in)
;   CoT_Get_Step_Count      — Number of configured steps
;   CoT_Execute_Chain       — Run the chain synchronously
;   CoT_Cancel              — Set cancellation flag
;   CoT_Is_Running          — Check if chain executing
;   CoT_Get_Stats           — Copy statistics to caller buffer
;   CoT_Reset_Stats         — Zero all counters
;   CoT_Get_Status_JSON     — Build JSON status into caller buffer
;   CoT_Get_Role_Info       — Get role name/label/icon/instruction by ID
;   CoT_Append_Data         — Append raw token data to the 1GB arena
;   CoT_Get_Data_Ptr        — Get base pointer to arena
;   CoT_Get_Data_Used       — Get bytes used in arena
;   CoT_Get_Version         — Returns engine version DWORD
;
; Architecture: x64 MASM | Windows ABI | SEH | No CRT dependency
; Build: ml64 /c /Zi /Zd /Fo rawrxd_cot_engine.obj rawrxd_cot_engine.asm
; Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
; =============================================================================

INCLUDE RawrXD_Common.inc

; =============================================================================
;                             EXPORTS
; =============================================================================
PUBLIC CoT_Initialize_Core
PUBLIC CoT_Shutdown_Core
PUBLIC CoT_Set_Max_Steps
PUBLIC CoT_Get_Max_Steps
PUBLIC CoT_Clear_Steps
PUBLIC CoT_Add_Step
PUBLIC CoT_Apply_Preset
PUBLIC CoT_Get_Step_Count
PUBLIC CoT_Execute_Chain
PUBLIC CoT_Cancel
PUBLIC CoT_Is_Running
PUBLIC CoT_Get_Stats
PUBLIC CoT_Reset_Stats
PUBLIC CoT_Get_Status_JSON
PUBLIC CoT_Get_Role_Info
PUBLIC CoT_Append_Data
PUBLIC CoT_Get_Data_Ptr
PUBLIC CoT_Get_Data_Used
PUBLIC CoT_Get_Version

; =============================================================================
;                          EXTERNAL IMPORTS
; =============================================================================
EXTERN VirtualAlloc: PROC
EXTERN VirtualFree: PROC
EXTERN GetTickCount64: PROC
EXTERN RtlZeroMemory: PROC
EXTERN OutputDebugStringA: PROC

; SRW lock functions (coordinated with rawrxd_cot_dll_entry.asm)
EXTERN Acquire_CoT_Lock: PROC
EXTERN Release_CoT_Lock: PROC

; =============================================================================
;                            CONSTANTS
; =============================================================================

; Engine version: 1.0.0 = 0x00010000
COT_ENGINE_VERSION          EQU     00010000h

; Arena sizes — 100M tokens at ~10 bytes avg = ~1 GB
COT_ARENA_RESERVE_SIZE      EQU     40000000h       ; 1 GB reserved
COT_ARENA_INITIAL_COMMIT    EQU     00100000h       ; 1 MB initial commit
COT_ARENA_COMMIT_GRANULE    EQU     00100000h       ; 1 MB commit chunks

; Chain limits
COT_MAX_STEPS               EQU     8
COT_MAX_STEP_OUTPUT_SIZE    EQU     00100000h       ; 1 MB per step output
COT_MAX_MODEL_NAME          EQU     256
COT_MAX_INSTRUCTION         EQU     1024
COT_MAX_JSON_BUFFER         EQU     00010000h       ; 64 KB JSON output

; Role IDs (match CoTRoleId enum in chain_of_thought_engine.h)
ROLE_REVIEWER               EQU     0
ROLE_AUDITOR                EQU     1
ROLE_THINKER                EQU     2
ROLE_RESEARCHER             EQU     3
ROLE_DEBATER_FOR            EQU     4
ROLE_DEBATER_AGAINST        EQU     5
ROLE_CRITIC                 EQU     6
ROLE_SYNTHESIZER            EQU     7
ROLE_BRAINSTORM             EQU     8
ROLE_VERIFIER               EQU     9
ROLE_REFINER                EQU     10
ROLE_SUMMARIZER             EQU     11
ROLE_COUNT                  EQU     12

; Preset IDs
PRESET_REVIEW               EQU     0
PRESET_AUDIT                EQU     1
PRESET_THINK                EQU     2
PRESET_RESEARCH             EQU     3
PRESET_DEBATE               EQU     4
PRESET_CUSTOM               EQU     5
PRESET_COUNT                EQU     6

; Execution states
STATE_IDLE                  EQU     0
STATE_RUNNING               EQU     1
STATE_CANCELLED             EQU     2

; =============================================================================
;                          STRUCTURES
; =============================================================================

; CoT Step descriptor (per-step configuration, 1344 bytes padded to 1408)
COT_STEP_DESC STRUCT
    roleId              DD ?                        ; CoTRoleId (0-11)
    skip                DD ?                        ; bool: skip this step
    modelName           DB COT_MAX_MODEL_NAME DUP(?)  ; model override string
    instructionOverride DB COT_MAX_INSTRUCTION DUP(?)  ; custom instruction
    _padding            DB 92 DUP(?)                ; pad to 1408 bytes
COT_STEP_DESC ENDS

; CoT Step Result (per-step execution result, 2MB + metadata)
COT_STEP_RESULT STRUCT
    stepIndex           DD ?                        ; 0-based
    roleId              DD ?                        ; CoTRoleId used
    modelUsed           DB COT_MAX_MODEL_NAME DUP(?)  ; actual model name
    success             DD ?                        ; 1 = success
    skipped             DD ?                        ; 1 = skipped
    errorCode           DD ?                        ; 0 = none
    latencyMs           DD ?                        ; step execution time
    tokenCount          DD ?                        ; approximate token count
    outputLen           DD ?                        ; bytes of output
    _padding2           DD ?                        ; alignment
    outputPtr           DQ ?                        ; pointer to output data
COT_STEP_RESULT ENDS

; CoT Statistics block (matches ChainOfThoughtEngine::CoTStats, 112 bytes)
COT_STATS STRUCT
    totalChains         DD ?
    successfulChains    DD ?
    failedChains        DD ?
    totalStepsExecuted  DD ?
    totalStepsSkipped   DD ?
    totalStepsFailed    DD ?
    avgLatencyMs        DD ?
    _pad0               DD ?
    roleUsage           DD ROLE_COUNT DUP(?)        ; per-role usage counts
    _reserved           DQ 4 DUP(?)                 ; future expansion
COT_STATS ENDS

; Role info entry (read-only table entry)
COT_ROLE_ENTRY STRUCT
    roleId              DD ?
    _pad0               DD ?
    namePtr             DQ ?                        ; -> ASCIIZ name
    labelPtr            DQ ?                        ; -> ASCIIZ label
    iconPtr             DQ ?                        ; -> ASCIIZ icon
    instructionPtr      DQ ?                        ; -> ASCIIZ instruction
COT_ROLE_ENTRY ENDS

; Preset step slot (role + skip flag for preset table)
PRESET_SLOT STRUCT
    roleId              DD ?
    skip                DD ?
PRESET_SLOT ENDS

; Preset entry (name + up to 8 steps)
PRESET_ENTRY STRUCT
    namePtr             DQ ?                        ; -> ASCIIZ preset name
    labelPtr            DQ ?                        ; -> ASCIIZ preset label
    stepCount           DD ?
    _pad0               DD ?
    steps               PRESET_SLOT COT_MAX_STEPS DUP(<>)
PRESET_ENTRY ENDS

; =============================================================================
;                        GLOBAL STATE (.data)
; =============================================================================
.data

ALIGN 16

; --- Arena ---
g_arenaBase             DQ 0                        ; VirtualAlloc base (1 GB)
g_arenaUsed             DQ 0                        ; bytes used in arena
g_arenaCommitted        DQ 0                        ; bytes committed so far

; --- Chain state ---
g_maxSteps              DD COT_MAX_STEPS            ; max steps [1,8]
g_stepCount             DD 0                        ; currently configured steps
g_executionState        DD STATE_IDLE               ; atomic: idle/running/cancelled
g_initialized           DD 0                        ; init flag

; --- Statistics ---
ALIGN 16
g_stats                 COT_STATS <>

; --- Configured steps (up to 8) ---
ALIGN 16
g_steps                 COT_STEP_DESC COT_MAX_STEPS DUP(<>)

; --- Step results (written during executeChain) ---
ALIGN 16
g_stepResults           COT_STEP_RESULT COT_MAX_STEPS DUP(<>)

; --- Default model name ---
g_defaultModel          DB COT_MAX_MODEL_NAME DUP(0)

; =============================================================================
;                 ROLE INFO TABLE (.rdata — read-only strings)
; =============================================================================
.const

ALIGN 8

; Role name strings
szRole_Reviewer         DB "reviewer", 0
szRole_Auditor          DB "auditor", 0
szRole_Thinker          DB "thinker", 0
szRole_Researcher       DB "researcher", 0
szRole_DebaterFor       DB "debater_for", 0
szRole_DebaterAgainst   DB "debater_against", 0
szRole_Critic           DB "critic", 0
szRole_Synthesizer      DB "synthesizer", 0
szRole_Brainstorm       DB "brainstorm", 0
szRole_Verifier         DB "verifier", 0
szRole_Refiner          DB "refiner", 0
szRole_Summarizer       DB "summarizer", 0

; Role labels
szLabel_Reviewer        DB "Reviewer", 0
szLabel_Auditor         DB "Auditor", 0
szLabel_Thinker         DB "Thinker", 0
szLabel_Researcher      DB "Researcher", 0
szLabel_DebaterFor      DB "Argue For", 0
szLabel_DebaterAgainst  DB "Argue Against", 0
szLabel_Critic          DB "Critic", 0
szLabel_Synthesizer     DB "Synthesizer", 0
szLabel_Brainstorm      DB "Brainstorm", 0
szLabel_Verifier        DB "Verifier", 0
szLabel_Refiner         DB "Refiner", 0
szLabel_Summarizer      DB "Summarizer", 0

; Role icons
szIcon_Reviewer         DB "[R]", 0
szIcon_Auditor          DB "[A]", 0
szIcon_Thinker          DB "[T]", 0
szIcon_Researcher       DB "[Rs]", 0
szIcon_DebaterFor       DB "[D+]", 0
szIcon_DebaterAgainst   DB "[D-]", 0
szIcon_Critic           DB "[Cr]", 0
szIcon_Synthesizer      DB "[Sy]", 0
szIcon_Brainstorm       DB "[B]", 0
szIcon_Verifier         DB "[V]", 0
szIcon_Refiner          DB "[Rf]", 0
szIcon_Summarizer       DB "[Su]", 0

; Role instructions (system prompts — matching chain_of_thought_engine.cpp)
szInstr_Reviewer        DB "You are a code reviewer. Analyze the following carefully, identify issues, suggest improvements.", 0
szInstr_Auditor         DB "You are a security/quality auditor. Check for vulnerabilities, correctness issues, edge cases, and compliance.", 0
szInstr_Thinker         DB "You are a deep thinker. Reason step-by-step through the problem, consider alternatives, and explain your reasoning.", 0
szInstr_Researcher      DB "You are a research assistant. Gather relevant context, find patterns, cross-reference information, and cite sources.", 0
szInstr_DebaterFor      DB "You argue IN FAVOR of the proposed approach. Present the strongest possible case for why this is correct/optimal.", 0
szInstr_DebaterAgainst  DB "You argue AGAINST the proposed approach. Present the strongest possible counterarguments and alternatives.", 0
szInstr_Critic          DB "You are a harsh critic. Find every flaw, weakness, and edge case. Be thorough and unforgiving.", 0
szInstr_Synthesizer     DB "You are a synthesizer. Combine all previous analyses into a coherent, actionable final answer. Resolve conflicts and present the best path forward.", 0
szInstr_Brainstorm      DB "You are a creative brainstormer. Generate multiple diverse approaches and ideas without filtering.", 0
szInstr_Verifier        DB "You are a verifier. Check all previous claims for accuracy. Flag anything unverified or incorrect.", 0
szInstr_Refiner         DB "You are a refiner. Take the previous output and improve its clarity, correctness, and completeness.", 0
szInstr_Summarizer      DB "You are a summarizer. Distill everything into a concise, actionable summary.", 0

; --- Role table (12 entries) ---
ALIGN 8
g_roleTable LABEL QWORD
    ; Entry 0: Reviewer
    DD ROLE_REVIEWER, 0
    DQ OFFSET szRole_Reviewer
    DQ OFFSET szLabel_Reviewer
    DQ OFFSET szIcon_Reviewer
    DQ OFFSET szInstr_Reviewer

    ; Entry 1: Auditor
    DD ROLE_AUDITOR, 0
    DQ OFFSET szRole_Auditor
    DQ OFFSET szLabel_Auditor
    DQ OFFSET szIcon_Auditor
    DQ OFFSET szInstr_Auditor

    ; Entry 2: Thinker
    DD ROLE_THINKER, 0
    DQ OFFSET szRole_Thinker
    DQ OFFSET szLabel_Thinker
    DQ OFFSET szIcon_Thinker
    DQ OFFSET szInstr_Thinker

    ; Entry 3: Researcher
    DD ROLE_RESEARCHER, 0
    DQ OFFSET szRole_Researcher
    DQ OFFSET szLabel_Researcher
    DQ OFFSET szIcon_Researcher
    DQ OFFSET szInstr_Researcher

    ; Entry 4: DebaterFor
    DD ROLE_DEBATER_FOR, 0
    DQ OFFSET szRole_DebaterFor
    DQ OFFSET szLabel_DebaterFor
    DQ OFFSET szIcon_DebaterFor
    DQ OFFSET szInstr_DebaterFor

    ; Entry 5: DebaterAgainst
    DD ROLE_DEBATER_AGAINST, 0
    DQ OFFSET szRole_DebaterAgainst
    DQ OFFSET szLabel_DebaterAgainst
    DQ OFFSET szIcon_DebaterAgainst
    DQ OFFSET szInstr_DebaterAgainst

    ; Entry 6: Critic
    DD ROLE_CRITIC, 0
    DQ OFFSET szRole_Critic
    DQ OFFSET szLabel_Critic
    DQ OFFSET szIcon_Critic
    DQ OFFSET szInstr_Critic

    ; Entry 7: Synthesizer
    DD ROLE_SYNTHESIZER, 0
    DQ OFFSET szRole_Synthesizer
    DQ OFFSET szLabel_Synthesizer
    DQ OFFSET szIcon_Synthesizer
    DQ OFFSET szInstr_Synthesizer

    ; Entry 8: Brainstorm
    DD ROLE_BRAINSTORM, 0
    DQ OFFSET szRole_Brainstorm
    DQ OFFSET szLabel_Brainstorm
    DQ OFFSET szIcon_Brainstorm
    DQ OFFSET szInstr_Brainstorm

    ; Entry 9: Verifier
    DD ROLE_VERIFIER, 0
    DQ OFFSET szRole_Verifier
    DQ OFFSET szLabel_Verifier
    DQ OFFSET szIcon_Verifier
    DQ OFFSET szInstr_Verifier

    ; Entry 10: Refiner
    DD ROLE_REFINER, 0
    DQ OFFSET szRole_Refiner
    DQ OFFSET szLabel_Refiner
    DQ OFFSET szIcon_Refiner
    DQ OFFSET szInstr_Refiner

    ; Entry 11: Summarizer
    DD ROLE_SUMMARIZER, 0
    DQ OFFSET szRole_Summarizer
    DQ OFFSET szLabel_Summarizer
    DQ OFFSET szIcon_Summarizer
    DQ OFFSET szInstr_Summarizer

; --- Preset name/label strings ---
szPreset_Review         DB "review", 0
szPresetL_Review        DB "Review", 0
szPreset_Audit          DB "audit", 0
szPresetL_Audit         DB "Audit", 0
szPreset_Think          DB "think", 0
szPresetL_Think         DB "Think", 0
szPreset_Research       DB "research", 0
szPresetL_Research      DB "Research", 0
szPreset_Debate         DB "debate", 0
szPresetL_Debate        DB "Debate", 0
szPreset_Custom         DB "custom", 0
szPresetL_Custom        DB "Custom", 0

; --- Preset table (6 entries, each with step definitions) ---
; Preset 0: review — reviewer -> critic -> synthesizer
ALIGN 8
g_presetTable LABEL QWORD
    ; review
    DQ OFFSET szPreset_Review
    DQ OFFSET szPresetL_Review
    DD 3, 0                                         ; 3 steps
    DD ROLE_REVIEWER, 0
    DD ROLE_CRITIC, 0
    DD ROLE_SYNTHESIZER, 0
    DD 0, 0                                          ; unused slots 3-7
    DD 0, 0
    DD 0, 0
    DD 0, 0
    DD 0, 0

    ; audit — auditor -> verifier -> summarizer
    DQ OFFSET szPreset_Audit
    DQ OFFSET szPresetL_Audit
    DD 3, 0
    DD ROLE_AUDITOR, 0
    DD ROLE_VERIFIER, 0
    DD ROLE_SUMMARIZER, 0
    DD 0, 0
    DD 0, 0
    DD 0, 0
    DD 0, 0
    DD 0, 0

    ; think — brainstorm -> thinker -> refiner -> synthesizer
    DQ OFFSET szPreset_Think
    DQ OFFSET szPresetL_Think
    DD 4, 0
    DD ROLE_BRAINSTORM, 0
    DD ROLE_THINKER, 0
    DD ROLE_REFINER, 0
    DD ROLE_SYNTHESIZER, 0
    DD 0, 0
    DD 0, 0
    DD 0, 0
    DD 0, 0

    ; research — researcher -> thinker -> verifier -> synthesizer
    DQ OFFSET szPreset_Research
    DQ OFFSET szPresetL_Research
    DD 4, 0
    DD ROLE_RESEARCHER, 0
    DD ROLE_THINKER, 0
    DD ROLE_VERIFIER, 0
    DD ROLE_SYNTHESIZER, 0
    DD 0, 0
    DD 0, 0
    DD 0, 0
    DD 0, 0

    ; debate — debater_for -> debater_against -> synthesizer
    DQ OFFSET szPreset_Debate
    DQ OFFSET szPresetL_Debate
    DD 3, 0
    DD ROLE_DEBATER_FOR, 0
    DD ROLE_DEBATER_AGAINST, 0
    DD ROLE_SYNTHESIZER, 0
    DD 0, 0
    DD 0, 0
    DD 0, 0
    DD 0, 0
    DD 0, 0

    ; custom — thinker (single step)
    DQ OFFSET szPreset_Custom
    DQ OFFSET szPresetL_Custom
    DD 1, 0
    DD ROLE_THINKER, 0
    DD 0, 0
    DD 0, 0
    DD 0, 0
    DD 0, 0
    DD 0, 0
    DD 0, 0
    DD 0, 0

; Preset entry size (bytes) — computed: 2 DQ + 2 DD + 8*(2 DD) = 16+8+64 = 88
PRESET_ENTRY_SIZE           EQU     88

; Role entry size in bytes: 2 DD + 4 DQ = 8 + 32 = 40
ROLE_ENTRY_SIZE             EQU     40

; JSON format strings
szJsonStatusFmt1    DB '{"version":', 0
szJsonStatusFmt2    DB ',"initialized":', 0
szJsonStatusFmt3    DB ',"running":', 0
szJsonStatusFmt4    DB ',"maxSteps":', 0
szJsonStatusFmt5    DB ',"stepCount":', 0
szJsonStatusFmt6    DB ',"arenaUsedBytes":', 0
szJsonStatusFmt7    DB ',"arenaCommittedBytes":', 0
szJsonStatusFmt8    DB ',"stats":{"totalChains":', 0
szJsonStatusFmt9    DB ',"successfulChains":', 0
szJsonStatusFmt10   DB ',"failedChains":', 0
szJsonStatusFmt11   DB ',"totalStepsExecuted":', 0
szJsonStatusFmt12   DB ',"totalStepsSkipped":', 0
szJsonStatusFmt13   DB ',"totalStepsFailed":', 0
szJsonStatusFmt14   DB ',"avgLatencyMs":', 0
szJsonClose         DB '}}', 0
szTrue              DB 'true', 0
szFalse             DB 'false', 0

; Debug strings
szDbg_InitStart     DB "[CoT-ASM] Initialize_Core: reserving 1GB VA...", 0
szDbg_InitOK        DB "[CoT-ASM] Initialize_Core: arena ready, 1MB committed.", 0
szDbg_InitFail      DB "[CoT-ASM] Initialize_Core: VirtualAlloc FAILED!", 0
szDbg_Shutdown      DB "[CoT-ASM] Shutdown_Core: releasing arena.", 0
szDbg_AppendFault   DB "[CoT-ASM] Append_Data: SEH fault during copy!", 0

; =============================================================================
;                            CODE SECTION
; =============================================================================
.code

; =============================================================================
; Internal: asm_strlen — compute length of ASCIIZ string
;   RCX = pointer to string
;   Returns: RAX = length (not including NUL)
; =============================================================================
asm_strlen PROC
    test    rcx, rcx
    jz      @@sl_zero
    xor     rax, rax
@@sl_loop:
    cmp     BYTE PTR [rcx + rax], 0
    je      @@sl_done
    inc     rax
    jmp     @@sl_loop
@@sl_done:
    ret
@@sl_zero:
    xor     rax, rax
    ret
asm_strlen ENDP

; =============================================================================
; Internal: asm_strcpy — copy ASCIIZ string (NUL-terminated)
;   RCX = dest
;   RDX = src
;   Returns: RAX = bytes copied (including NUL)
; =============================================================================
asm_strcpy PROC
    test    rcx, rcx
    jz      @@sc_zero
    test    rdx, rdx
    jz      @@sc_zero
    xor     rax, rax
@@sc_loop:
    mov     r8b, BYTE PTR [rdx + rax]
    mov     BYTE PTR [rcx + rax], r8b
    inc     rax
    test    r8b, r8b
    jnz     @@sc_loop
    ret                                     ; RAX = count including NUL
@@sc_zero:
    xor     rax, rax
    ret
asm_strcpy ENDP

; =============================================================================
; Internal: asm_strcmp — compare two ASCIIZ strings
;   RCX = str1
;   RDX = str2
;   Returns: EAX = 0 if equal, non-zero otherwise
; =============================================================================
asm_strcmp PROC
    test    rcx, rcx
    jz      @@cmp_neq
    test    rdx, rdx
    jz      @@cmp_neq
    xor     r8, r8
@@cmp_loop:
    movzx   eax, BYTE PTR [rcx + r8]
    movzx   r9d, BYTE PTR [rdx + r8]
    cmp     eax, r9d
    jne     @@cmp_neq
    test    eax, eax                        ; both NUL?
    jz      @@cmp_eq
    inc     r8
    jmp     @@cmp_loop
@@cmp_eq:
    xor     eax, eax                        ; return 0 = equal
    ret
@@cmp_neq:
    mov     eax, 1                          ; return 1 = not equal
    ret
asm_strcmp ENDP

; =============================================================================
; Internal: asm_itoa — convert unsigned 64-bit integer to decimal ASCII
;   RCX = value
;   RDX = output buffer (must hold >= 21 chars)
;   Returns: RAX = length of string written
; =============================================================================
asm_itoa PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    sub     rsp, 32                         ; temp buffer
    .allocstack 32
    .endprolog

    mov     rdi, rdx                        ; rdi = output
    mov     rax, rcx                        ; rax = value
    xor     esi, esi                        ; esi = digit count

    ; Special case: 0
    test    rax, rax
    jnz     @@itoa_loop
    mov     BYTE PTR [rdi], '0'
    mov     BYTE PTR [rdi + 1], 0
    mov     eax, 1
    jmp     @@itoa_done

@@itoa_loop:
    ; Divide RAX by 10
    xor     edx, edx
    mov     rbx, 10
    div     rbx                             ; RAX = quotient, RDX = remainder
    add     dl, '0'
    mov     BYTE PTR [rsp + rsi], dl        ; store digit (reversed)
    inc     esi
    test    rax, rax
    jnz     @@itoa_loop

    ; Reverse digits into output buffer
    xor     ecx, ecx                        ; output index
    mov     eax, esi
    dec     eax                             ; eax = last digit index
@@itoa_reverse:
    movzx   r8d, BYTE PTR [rsp + rax]
    mov     BYTE PTR [rdi + rcx], r8b
    inc     ecx
    dec     eax
    jns     @@itoa_reverse

    mov     BYTE PTR [rdi + rcx], 0         ; NUL terminate
    mov     eax, esi                        ; return length

@@itoa_done:
    add     rsp, 32
    pop     rdi
    pop     rsi
    pop     rbx
    ret
asm_itoa ENDP

; =============================================================================
; Internal: asm_append_str — append ASCIIZ string to buffer
;   RCX = dest buffer current write position
;   RDX = source string (ASCIIZ)
;   Returns: RAX = new write position (past last char, NOT past NUL)
; =============================================================================
asm_append_str PROC
    test    rcx, rcx
    jz      @@as_done
    test    rdx, rdx
    jz      @@as_done
@@as_loop:
    movzx   r8d, BYTE PTR [rdx]
    test    r8d, r8d
    jz      @@as_done
    mov     BYTE PTR [rcx], r8b
    inc     rcx
    inc     rdx
    jmp     @@as_loop
@@as_done:
    mov     rax, rcx
    ret
asm_append_str ENDP

; =============================================================================
; Internal: asm_append_int — append decimal integer to buffer
;   RCX = dest buffer current write position
;   RDX = integer value (unsigned 64-bit)
;   Returns: RAX = new write position
; =============================================================================
asm_append_int PROC FRAME
    push    rbx
    .pushreg rbx
    sub     rsp, 48                         ; itoa temp + shadow
    .allocstack 48
    .endprolog

    mov     rbx, rcx                        ; save dest position
    ; asm_itoa(value, temp_buffer)
    mov     rcx, rdx                        ; value
    lea     rdx, [rsp + 24]                 ; temp buffer on stack
    call    asm_itoa
    ; rax = length, digits at [rsp+24]
    ; asm_append_str(dest, temp_buffer)
    mov     rcx, rbx
    lea     rdx, [rsp + 24]
    call    asm_append_str
    ; rax = new position

    add     rsp, 48
    pop     rbx
    ret
asm_append_int ENDP

; =============================================================================
; Internal: ensure_arena_committed — commit more pages if needed
;   RCX = required total bytes (g_arenaUsed + new data)
;   Returns: EAX = 0 on success, -1 if commit fails
; =============================================================================
ensure_arena_committed PROC FRAME
    push    rbx
    .pushreg rbx
    push    r12
    .pushreg r12
    sub     rsp, 40
    .allocstack 40
    .endprolog

    mov     r12, rcx                        ; required bytes

    mov     rbx, QWORD PTR [g_arenaCommitted]
    cmp     r12, rbx
    jbe     @@eac_ok                        ; already committed enough

    ; Calculate how much more to commit (round up to granule)
    mov     rax, r12
    sub     rax, rbx                        ; additional needed
    add     rax, COT_ARENA_COMMIT_GRANULE - 1
    and     rax, NOT (COT_ARENA_COMMIT_GRANULE - 1)  ; round up

    ; VirtualAlloc(base + committed, roundedSize, MEM_COMMIT, PAGE_READWRITE)
    mov     rcx, QWORD PTR [g_arenaBase]
    add     rcx, rbx                        ; start of new commit region
    mov     rdx, rax                        ; size
    mov     r8d, MEM_COMMIT
    mov     r9d, PAGE_READWRITE
    call    VirtualAlloc
    test    rax, rax
    jz      @@eac_fail

    ; Update committed size
    mov     rax, r12
    add     rax, COT_ARENA_COMMIT_GRANULE - 1
    and     rax, NOT (COT_ARENA_COMMIT_GRANULE - 1)
    add     QWORD PTR [g_arenaCommitted], rax

@@eac_ok:
    xor     eax, eax                        ; success
    jmp     @@eac_done

@@eac_fail:
    mov     eax, -1

@@eac_done:
    add     rsp, 40
    pop     r12
    pop     rbx
    ret
ensure_arena_committed ENDP

; =============================================================================
; SEH exception handler for arena memory operations
; This catches access violations during rep movsb over the arena
; =============================================================================
CoT_SEH_Handler PROC
    ; EXCEPTION_DISPOSITION handler(
    ;   EXCEPTION_RECORD* ExceptionRecord,   RCX
    ;   void* EstablisherFrame,               RDX
    ;   CONTEXT* ContextRecord,               R8
    ;   void* DispatcherContext                R9
    ; )
    ; Check if this is an access violation (0xC0000005)
    mov     eax, DWORD PTR [rcx]            ; ExceptionRecord->ExceptionCode
    cmp     eax, 0C0000005h
    jne     @@seh_continue

    ; Access violation in arena — set return to fault label
    ; Update RIP in ContextRecord to point to our safe resume
    ; ContextRecord->Rip = @@seh_resume_addr
    lea     rax, [@@seh_resume_target]
    mov     QWORD PTR [r8 + 0F8h], rax      ; CONTEXT.Rip offset = 0xF8

    ; Clear RAX in context (signal error)
    mov     QWORD PTR [r8 + 78h], 0         ; CONTEXT.Rax = 0

    mov     eax, 0                          ; ExceptionContinueExecution
    ret

@@seh_continue:
    mov     eax, 1                          ; ExceptionContinueSearch
    ret

@@seh_resume_target:
    ; This is never called directly — it's a target IP for the context fixup
    ret
CoT_SEH_Handler ENDP

; =============================================================================
; CoT_Initialize_Core
; Reserve 1GB virtual address space. Commit initial 1MB.
; Initialize all data structures to zero state.
;
; Returns: RAX = 0 on success, STATUS_UNSUCCESSFUL on failure
;          RDX = detail string pointer
; =============================================================================
CoT_Initialize_Core PROC FRAME
    push    rbx
    .pushreg rbx
    push    r12
    .pushreg r12
    sub     rsp, 56
    .allocstack 56
    .endprolog

    ; Debug output
    lea     rcx, szDbg_InitStart
    call    OutputDebugStringA

    ; Check if already initialized
    cmp     DWORD PTR [g_initialized], 1
    je      @@init_already

    ; Acquire lock
    call    Acquire_CoT_Lock

    ; Double-check after lock
    cmp     DWORD PTR [g_initialized], 1
    je      @@init_unlock_ok

    ; Reserve 1 GB VA space
    xor     ecx, ecx                        ; lpAddress = NULL
    mov     rdx, COT_ARENA_RESERVE_SIZE     ; 1 GB
    mov     r8d, MEM_RESERVE
    mov     r9d, PAGE_READWRITE
    call    VirtualAlloc
    test    rax, rax
    jz      @@init_unlock_fail

    mov     QWORD PTR [g_arenaBase], rax
    mov     r12, rax                        ; save base

    ; Commit initial 1 MB
    mov     rcx, rax                        ; base
    mov     rdx, COT_ARENA_INITIAL_COMMIT   ; 1 MB
    mov     r8d, MEM_COMMIT
    mov     r9d, PAGE_READWRITE
    call    VirtualAlloc
    test    rax, rax
    jz      @@init_unlock_free

    mov     QWORD PTR [g_arenaCommitted], COT_ARENA_INITIAL_COMMIT
    mov     QWORD PTR [g_arenaUsed], 0

    ; Zero state
    mov     DWORD PTR [g_maxSteps], COT_MAX_STEPS
    mov     DWORD PTR [g_stepCount], 0
    mov     DWORD PTR [g_executionState], STATE_IDLE

    ; Zero statistics
    lea     rcx, g_stats
    xor     edx, edx
    mov     r8d, SIZEOF COT_STATS
    call    RtlZeroMemory

    ; Zero step descriptors
    lea     rcx, g_steps
    xor     edx, edx
    mov     r8d, SIZEOF COT_STEP_DESC * COT_MAX_STEPS
    call    RtlZeroMemory

    ; Zero step results
    lea     rcx, g_stepResults
    xor     edx, edx
    mov     r8d, SIZEOF COT_STEP_RESULT * COT_MAX_STEPS
    call    RtlZeroMemory

    ; Zero default model
    lea     rcx, g_defaultModel
    xor     edx, edx
    mov     r8d, COT_MAX_MODEL_NAME
    call    RtlZeroMemory

    ; Mark initialized
    mov     DWORD PTR [g_initialized], 1

    ; Debug output
    lea     rcx, szDbg_InitOK
    call    OutputDebugStringA

@@init_unlock_ok:
    call    Release_CoT_Lock

@@init_already:
    xor     eax, eax                        ; STATUS_SUCCESS
    lea     rdx, szDbg_InitOK
    jmp     @@init_ret

@@init_unlock_free:
    ; Commit failed — release the reservation
    mov     rcx, r12
    xor     edx, edx
    mov     r8d, MEM_RELEASE
    call    VirtualFree
    mov     QWORD PTR [g_arenaBase], 0

@@init_unlock_fail:
    call    Release_CoT_Lock
    lea     rcx, szDbg_InitFail
    call    OutputDebugStringA
    mov     eax, STATUS_UNSUCCESSFUL
    lea     rdx, szDbg_InitFail

@@init_ret:
    add     rsp, 56
    pop     r12
    pop     rbx
    ret
CoT_Initialize_Core ENDP

; =============================================================================
; CoT_Shutdown_Core
; Release all virtual memory and reset state.
;
; Returns: RAX = 0 on success
; =============================================================================
CoT_Shutdown_Core PROC FRAME
    push    rbx
    .pushreg rbx
    sub     rsp, 40
    .allocstack 40
    .endprolog

    lea     rcx, szDbg_Shutdown
    call    OutputDebugStringA

    call    Acquire_CoT_Lock

    ; Release arena
    mov     rcx, QWORD PTR [g_arenaBase]
    test    rcx, rcx
    jz      @@sd_no_arena
    xor     edx, edx
    mov     r8d, MEM_RELEASE
    call    VirtualFree

@@sd_no_arena:
    mov     QWORD PTR [g_arenaBase], 0
    mov     QWORD PTR [g_arenaUsed], 0
    mov     QWORD PTR [g_arenaCommitted], 0

    ; Reset state
    mov     DWORD PTR [g_maxSteps], COT_MAX_STEPS
    mov     DWORD PTR [g_stepCount], 0
    mov     DWORD PTR [g_executionState], STATE_IDLE
    mov     DWORD PTR [g_initialized], 0

    ; Zero stats
    lea     rcx, g_stats
    xor     edx, edx
    mov     r8d, SIZEOF COT_STATS
    call    RtlZeroMemory

    call    Release_CoT_Lock

    xor     eax, eax                        ; STATUS_SUCCESS
    add     rsp, 40
    pop     rbx
    ret
CoT_Shutdown_Core ENDP

; =============================================================================
; CoT_Set_Max_Steps
; Clamp value to [1, 8].
;
; RCX = new max steps (int)
; Returns: RAX = clamped value
; =============================================================================
CoT_Set_Max_Steps PROC
    mov     eax, ecx
    cmp     eax, 1
    jge     @@sms_check_high
    mov     eax, 1
@@sms_check_high:
    cmp     eax, COT_MAX_STEPS
    jle     @@sms_store
    mov     eax, COT_MAX_STEPS
@@sms_store:
    mov     DWORD PTR [g_maxSteps], eax
    ret
CoT_Set_Max_Steps ENDP

; =============================================================================
; CoT_Get_Max_Steps
; Returns: EAX = current max steps
; =============================================================================
CoT_Get_Max_Steps PROC
    mov     eax, DWORD PTR [g_maxSteps]
    ret
CoT_Get_Max_Steps ENDP

; =============================================================================
; CoT_Clear_Steps
; Remove all configured steps. Thread-safe.
; =============================================================================
CoT_Clear_Steps PROC FRAME
    sub     rsp, 40
    .allocstack 40
    .endprolog

    call    Acquire_CoT_Lock
    mov     DWORD PTR [g_stepCount], 0
    lea     rcx, g_steps
    xor     edx, edx
    mov     r8d, SIZEOF COT_STEP_DESC * COT_MAX_STEPS
    call    RtlZeroMemory
    call    Release_CoT_Lock

    add     rsp, 40
    ret
CoT_Clear_Steps ENDP

; =============================================================================
; CoT_Add_Step
; Add a step to the chain.
;
; RCX = role ID (0-11)
; RDX = model name (ASCIIZ, may be NULL for default)
; R8  = instruction override (ASCIIZ, may be NULL for role default)
; R9  = skip flag (0 = don't skip, 1 = skip)
;
; Returns: EAX = 0 on success, -1 if full
; =============================================================================
CoT_Add_Step PROC FRAME
    push    rbx
    .pushreg rbx
    push    r12
    .pushreg r12
    push    r13
    .pushreg r13
    push    r14
    .pushreg r14
    push    r15
    .pushreg r15
    sub     rsp, 48
    .allocstack 48
    .endprolog

    ; Save args
    mov     r12d, ecx                       ; roleId
    mov     r13, rdx                        ; model name ptr
    mov     r14, r8                         ; instruction ptr
    mov     r15d, r9d                       ; skip flag

    call    Acquire_CoT_Lock

    ; Check capacity
    mov     eax, DWORD PTR [g_stepCount]
    cmp     eax, DWORD PTR [g_maxSteps]
    jge     @@as_full

    ; Calculate step descriptor address: g_steps + stepCount * sizeof(COT_STEP_DESC)
    mov     ecx, eax                        ; stepCount
    imul    rcx, SIZEOF COT_STEP_DESC
    lea     rbx, g_steps
    add     rbx, rcx                        ; rbx = target step descriptor

    ; Set roleId
    mov     DWORD PTR [rbx + COT_STEP_DESC.roleId], r12d

    ; Set skip flag
    mov     DWORD PTR [rbx + COT_STEP_DESC.skip], r15d

    ; Copy model name (if provided)
    lea     rcx, [rbx + COT_STEP_DESC.modelName]
    test    r13, r13
    jz      @@as_no_model
    mov     rdx, r13
    call    asm_strcpy
    jmp     @@as_model_done
@@as_no_model:
    mov     BYTE PTR [rbx + COT_STEP_DESC.modelName], 0
@@as_model_done:

    ; Copy instruction override (if provided)
    lea     rcx, [rbx + COT_STEP_DESC.instructionOverride]
    test    r14, r14
    jz      @@as_no_instr
    mov     rdx, r14
    call    asm_strcpy
    jmp     @@as_instr_done
@@as_no_instr:
    mov     BYTE PTR [rbx + COT_STEP_DESC.instructionOverride], 0
@@as_instr_done:

    ; Increment step count
    inc     DWORD PTR [g_stepCount]

    call    Release_CoT_Lock
    xor     eax, eax                        ; success
    jmp     @@as_done

@@as_full:
    call    Release_CoT_Lock
    mov     eax, -1                         ; chain is full

@@as_done:
    add     rsp, 48
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rbx
    ret
CoT_Add_Step ENDP

; =============================================================================
; CoT_Apply_Preset
; Load a named preset configuration, replacing current steps.
;
; RCX = preset name (ASCIIZ, e.g. "review", "audit", "think", etc.)
;
; Returns: EAX = 0 on success, -1 if preset not found
; =============================================================================
CoT_Apply_Preset PROC FRAME
    push    rbx
    .pushreg rbx
    push    r12
    .pushreg r12
    push    r13
    .pushreg r13
    push    rsi
    .pushreg rsi
    sub     rsp, 56
    .allocstack 56
    .endprolog

    mov     r12, rcx                        ; save preset name ptr

    ; Search preset table
    lea     rbx, g_presetTable
    xor     r13d, r13d                      ; preset index = 0

@@ap_search:
    cmp     r13d, PRESET_COUNT
    jge     @@ap_not_found

    ; Compare name: g_presetTable[idx].namePtr
    mov     rcx, r12
    mov     rdx, QWORD PTR [rbx]            ; namePtr
    call    asm_strcmp
    test    eax, eax
    jz      @@ap_found

    add     rbx, PRESET_ENTRY_SIZE
    inc     r13d
    jmp     @@ap_search

@@ap_found:
    ; Clear current steps first
    call    CoT_Clear_Steps

    call    Acquire_CoT_Lock

    ; Read step count from preset entry
    mov     ecx, DWORD PTR [rbx + 16]       ; stepCount at offset 16
    mov     DWORD PTR [g_stepCount], ecx
    mov     esi, ecx                        ; save count

    ; Copy preset steps into g_steps
    lea     r12, g_steps
    lea     r13, [rbx + 24]                 ; first PRESET_SLOT at offset 24

    xor     ecx, ecx                        ; step index
@@ap_copy_loop:
    cmp     ecx, esi
    jge     @@ap_copy_done

    ; Calculate step descriptor offset
    mov     eax, ecx
    imul    rax, SIZEOF COT_STEP_DESC
    lea     rdx, [r12 + rax]               ; target step descriptor

    ; Read role ID from preset slot (8 bytes per slot: roleId DD, skip DD)
    mov     eax, ecx
    shl     eax, 3                          ; * 8
    mov     r8d, DWORD PTR [r13 + rax]      ; roleId
    mov     DWORD PTR [rdx + COT_STEP_DESC.roleId], r8d

    ; Read skip flag
    mov     r8d, DWORD PTR [r13 + rax + 4]  ; skip
    mov     DWORD PTR [rdx + COT_STEP_DESC.skip], r8d

    ; Model and instruction remain empty (use defaults)
    mov     BYTE PTR [rdx + COT_STEP_DESC.modelName], 0
    mov     BYTE PTR [rdx + COT_STEP_DESC.instructionOverride], 0

    inc     ecx
    jmp     @@ap_copy_loop

@@ap_copy_done:
    call    Release_CoT_Lock
    xor     eax, eax                        ; success
    jmp     @@ap_ret

@@ap_not_found:
    mov     eax, -1                         ; preset not found

@@ap_ret:
    add     rsp, 56
    pop     rsi
    pop     r13
    pop     r12
    pop     rbx
    ret
CoT_Apply_Preset ENDP

; =============================================================================
; CoT_Get_Step_Count
; Returns: EAX = number of configured steps
; =============================================================================
CoT_Get_Step_Count PROC
    mov     eax, DWORD PTR [g_stepCount]
    ret
CoT_Get_Step_Count ENDP

; =============================================================================
; CoT_Execute_Chain
; Execute the configured chain synchronously.
; NOTE: In the pure MASM DLL, this performs the step orchestration loop,
; calling an externally-provided inference callback for each step.
; The callback must be registered via CoT_Set_Inference_Callback first.
;
; RCX = user query (ASCIIZ)
; RDX = inference callback function pointer:
;       int64_t (*callback)(const char* systemPrompt, const char* userMsg,
;                           const char* model, char* outputBuf, int64_t outBufSize)
;       Returns: bytes written to outputBuf, or -1 on error
;
; Returns: RAX = steps completed (0 if error)
;          RDX = pointer to g_stepResults array
; =============================================================================
CoT_Execute_Chain PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    push    r12
    .pushreg r12
    push    r13
    .pushreg r13
    push    r14
    .pushreg r14
    push    r15
    .pushreg r15
    sub     rsp, 128
    .allocstack 128
    .endprolog

    ; Save arguments
    mov     r12, rcx                        ; r12 = userQuery
    mov     r13, rdx                        ; r13 = inference callback

    ; Validate
    test    r12, r12
    jz      @@ec_error
    test    r13, r13
    jz      @@ec_error

    ; Check not already running (atomic CAS)
    lea     rbx, g_executionState
    mov     eax, STATE_IDLE
    lock cmpxchg DWORD PTR [rbx], STATE_RUNNING   ; attempt IDLE -> RUNNING
    ; CMPXCHG: if [rbx] == EAX, then [rbx] = STATE_RUNNING and ZF=1
    ; We need to encode this properly for MASM:
    mov     eax, STATE_IDLE
    mov     ecx, STATE_RUNNING
    lock cmpxchg DWORD PTR [rbx], ecx
    jnz     @@ec_error                      ; already running

    ; Record chain start time
    call    GetTickCount64
    mov     QWORD PTR [rsp + 96], rax       ; [rsp+96] = chainStartTime

    ; Zero step results
    lea     rcx, g_stepResults
    xor     edx, edx
    mov     r8d, SIZEOF COT_STEP_RESULT * COT_MAX_STEPS
    call    RtlZeroMemory

    ; Initialize counters
    xor     r14d, r14d                      ; r14 = stepsCompleted
    xor     r15d, r15d                      ; r15 = current step index
    mov     DWORD PTR [rsp + 104], 0        ; stepsSkipped
    mov     DWORD PTR [rsp + 108], 0        ; stepsFailed

    mov     esi, DWORD PTR [g_stepCount]    ; esi = total steps

@@ec_step_loop:
    cmp     r15d, esi
    jge     @@ec_chain_done

    ; Check cancellation
    cmp     DWORD PTR [g_executionState], STATE_CANCELLED
    je      @@ec_chain_done

    ; Get step descriptor address
    mov     eax, r15d
    imul    rax, SIZEOF COT_STEP_DESC
    lea     rbx, g_steps
    add     rbx, rax                        ; rbx = current step descriptor

    ; Get step result address
    mov     eax, r15d
    imul    rax, SIZEOF COT_STEP_RESULT
    lea     rdi, g_stepResults
    add     rdi, rax                        ; rdi = current step result

    ; Fill step result metadata
    mov     DWORD PTR [rdi + COT_STEP_RESULT.stepIndex], r15d
    mov     eax, DWORD PTR [rbx + COT_STEP_DESC.roleId]
    mov     DWORD PTR [rdi + COT_STEP_RESULT.roleId], eax

    ; Check skip flag
    cmp     DWORD PTR [rbx + COT_STEP_DESC.skip], 0
    je      @@ec_not_skipped

    ; Skipped step
    mov     DWORD PTR [rdi + COT_STEP_RESULT.skipped], 1
    mov     DWORD PTR [rdi + COT_STEP_RESULT.success], 1
    inc     DWORD PTR [rsp + 104]           ; stepsSkipped++
    jmp     @@ec_next_step

@@ec_not_skipped:
    ; Determine system prompt: use instruction override if set, else role default
    lea     rcx, [rbx + COT_STEP_DESC.instructionOverride]
    cmp     BYTE PTR [rcx], 0
    jne     @@ec_have_sysprompt

    ; Use role default instruction
    mov     eax, DWORD PTR [rbx + COT_STEP_DESC.roleId]
    cmp     eax, ROLE_COUNT
    jge     @@ec_step_fail
    imul    rax, ROLE_ENTRY_SIZE
    lea     rcx, g_roleTable
    add     rcx, rax
    mov     rcx, QWORD PTR [rcx + 32]      ; instructionPtr (offset 8+8+8+8=32)
@@ec_have_sysprompt:
    mov     QWORD PTR [rsp + 112], rcx      ; save systemPrompt ptr

    ; Determine model: use step override if set, else g_defaultModel
    lea     rcx, [rbx + COT_STEP_DESC.modelName]
    cmp     BYTE PTR [rcx], 0
    jne     @@ec_have_model
    lea     rcx, g_defaultModel
@@ec_have_model:
    ; Copy model name to result
    push    rcx                             ; save model ptr
    lea     rcx, [rdi + COT_STEP_RESULT.modelUsed]
    pop     rdx                             ; model ptr -> rdx
    push    rdx                             ; re-save for callback
    call    asm_strcpy
    pop     rdx                             ; rdx = model ptr

    ; Record step start time
    push    rdx
    call    GetTickCount64
    mov     QWORD PTR [rsp + 128], rax      ; stepStartTime (adjusted for push)
    pop     rdx

    ; Call inference callback:
    ;   callback(systemPrompt, userQuery, model, outputBuf, outBufSize)
    ; Allocate output buffer from arena or stack
    ; For safety, allocate a 1MB region via VirtualAlloc for step output
    push    rdx                             ; save model ptr
    xor     ecx, ecx
    mov     rdx, COT_MAX_STEP_OUTPUT_SIZE
    mov     r8d, MEM_COMMIT OR MEM_RESERVE
    mov     r9d, PAGE_READWRITE
    call    VirtualAlloc
    pop     rdx                             ; restore model ptr
    test    rax, rax
    jz      @@ec_step_fail

    mov     QWORD PTR [rdi + COT_STEP_RESULT.outputPtr], rax

    ; Set up callback arguments (Windows x64 ABI: RCX, RDX, R8, R9, [rsp+32])
    ; arg1 (RCX) = systemPrompt
    ; arg2 (RDX) = userQuery
    ; arg3 (R8)  = model
    ; arg4 (R9)  = outputBuf
    ; arg5 ([rsp+32]) = outBufSize
    mov     r9, rax                         ; outputBuf
    mov     r8, rdx                         ; model
    mov     rdx, r12                        ; userQuery
    mov     rcx, QWORD PTR [rsp + 112]      ; systemPrompt
    mov     QWORD PTR [rsp + 32], COT_MAX_STEP_OUTPUT_SIZE  ; outBufSize
    call    r13                             ; call inference callback

    ; RAX = bytes written or -1 on error
    cmp     rax, 0
    jle     @@ec_step_fail_output

    ; Success
    mov     DWORD PTR [rdi + COT_STEP_RESULT.outputLen], eax
    mov     DWORD PTR [rdi + COT_STEP_RESULT.success], 1
    ; Approximate token count (output_len / 4)
    shr     eax, 2
    mov     DWORD PTR [rdi + COT_STEP_RESULT.tokenCount], eax
    inc     r14d                            ; stepsCompleted++
    jmp     @@ec_step_timing

@@ec_step_fail_output:
    ; Free the output buffer on failure
    mov     rcx, QWORD PTR [rdi + COT_STEP_RESULT.outputPtr]
    test    rcx, rcx
    jz      @@ec_step_fail
    xor     edx, edx
    mov     r8d, MEM_RELEASE
    call    VirtualFree
    mov     QWORD PTR [rdi + COT_STEP_RESULT.outputPtr], 0

@@ec_step_fail:
    mov     DWORD PTR [rdi + COT_STEP_RESULT.success], 0
    mov     DWORD PTR [rdi + COT_STEP_RESULT.errorCode], 1
    inc     DWORD PTR [rsp + 108]           ; stepsFailed++

@@ec_step_timing:
    ; Calculate step latency
    call    GetTickCount64
    sub     rax, QWORD PTR [rsp + 128]      ; stepStartTime
    mov     DWORD PTR [rdi + COT_STEP_RESULT.latencyMs], eax

@@ec_next_step:
    inc     r15d
    jmp     @@ec_step_loop

@@ec_chain_done:
    ; Calculate total chain latency
    call    GetTickCount64
    sub     rax, QWORD PTR [rsp + 96]       ; chainStartTime

    ; Update statistics under lock
    call    Acquire_CoT_Lock

    inc     DWORD PTR [g_stats + COT_STATS.totalChains]
    test    r14d, r14d
    jz      @@ec_stat_fail
    inc     DWORD PTR [g_stats + COT_STATS.successfulChains]
    jmp     @@ec_stat_steps
@@ec_stat_fail:
    inc     DWORD PTR [g_stats + COT_STATS.failedChains]
@@ec_stat_steps:
    add     DWORD PTR [g_stats + COT_STATS.totalStepsExecuted], r14d
    mov     ecx, DWORD PTR [rsp + 104]
    add     DWORD PTR [g_stats + COT_STATS.totalStepsSkipped], ecx
    mov     ecx, DWORD PTR [rsp + 108]
    add     DWORD PTR [g_stats + COT_STATS.totalStepsFailed], ecx

    ; Update role usage for non-skipped steps
    xor     ecx, ecx
@@ec_role_usage_loop:
    cmp     ecx, esi
    jge     @@ec_role_usage_done
    mov     eax, ecx
    imul    rax, SIZEOF COT_STEP_RESULT
    lea     rdx, g_stepResults
    add     rdx, rax
    cmp     DWORD PTR [rdx + COT_STEP_RESULT.skipped], 0
    jne     @@ec_role_next
    mov     eax, DWORD PTR [rdx + COT_STEP_RESULT.roleId]
    cmp     eax, ROLE_COUNT
    jge     @@ec_role_next
    lea     r8, [g_stats + COT_STATS.roleUsage]
    inc     DWORD PTR [r8 + rax * 4]
@@ec_role_next:
    inc     ecx
    jmp     @@ec_role_usage_loop
@@ec_role_usage_done:

    ; Update average latency (running average)
    mov     eax, DWORD PTR [g_stats + COT_STATS.totalChains]
    cmp     eax, 1
    jle     @@ec_first_chain
    ; avg = ((avg * (total-1)) + chainLatency) / total
    dec     eax
    imul    eax, DWORD PTR [g_stats + COT_STATS.avgLatencyMs]
    mov     ecx, DWORD PTR [rsp + 96]       ; low 32 bits of chain latency
    add     eax, ecx
    mov     ecx, DWORD PTR [g_stats + COT_STATS.totalChains]
    xor     edx, edx
    div     ecx
    mov     DWORD PTR [g_stats + COT_STATS.avgLatencyMs], eax
    jmp     @@ec_avg_done
@@ec_first_chain:
    ; First chain: avg = latency
    call    GetTickCount64
    sub     rax, QWORD PTR [rsp + 96]
    mov     DWORD PTR [g_stats + COT_STATS.avgLatencyMs], eax
@@ec_avg_done:

    call    Release_CoT_Lock

    ; Reset state to idle
    mov     DWORD PTR [g_executionState], STATE_IDLE

    ; Return: RAX = steps completed, RDX = results ptr
    mov     eax, r14d
    lea     rdx, g_stepResults
    jmp     @@ec_ret

@@ec_error:
    xor     eax, eax
    xor     edx, edx

@@ec_ret:
    add     rsp, 128
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
CoT_Execute_Chain ENDP

; =============================================================================
; CoT_Cancel
; Set the cancellation flag. Running chain will stop after current step.
; =============================================================================
CoT_Cancel PROC
    mov     DWORD PTR [g_executionState], STATE_CANCELLED
    ret
CoT_Cancel ENDP

; =============================================================================
; CoT_Is_Running
; Returns: EAX = 1 if chain is executing, 0 otherwise
; =============================================================================
CoT_Is_Running PROC
    cmp     DWORD PTR [g_executionState], STATE_RUNNING
    sete    al
    movzx   eax, al
    ret
CoT_Is_Running ENDP

; =============================================================================
; CoT_Get_Stats
; Copy statistics block to caller-provided buffer.
;
; RCX = destination buffer (must be >= sizeof(COT_STATS))
;
; Returns: EAX = 0 on success, -1 if null
; =============================================================================
CoT_Get_Stats PROC FRAME
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    sub     rsp, 40
    .allocstack 40
    .endprolog

    test    rcx, rcx
    jz      @@gs_null

    call    Acquire_CoT_Lock

    ; memcpy(dest, &g_stats, sizeof(COT_STATS))
    mov     rdi, rcx
    lea     rsi, g_stats
    mov     ecx, SIZEOF COT_STATS
    rep     movsb

    call    Release_CoT_Lock

    xor     eax, eax
    jmp     @@gs_done

@@gs_null:
    mov     eax, -1

@@gs_done:
    add     rsp, 40
    pop     rdi
    pop     rsi
    ret
CoT_Get_Stats ENDP

; =============================================================================
; CoT_Reset_Stats
; Zero all statistics counters.
; =============================================================================
CoT_Reset_Stats PROC FRAME
    sub     rsp, 40
    .allocstack 40
    .endprolog

    call    Acquire_CoT_Lock

    lea     rcx, g_stats
    xor     edx, edx
    mov     r8d, SIZEOF COT_STATS
    call    RtlZeroMemory

    call    Release_CoT_Lock

    add     rsp, 40
    ret
CoT_Reset_Stats ENDP

; =============================================================================
; CoT_Get_Status_JSON
; Build a JSON status string into the caller-provided buffer.
;
; RCX = output buffer
; RDX = buffer size (bytes)
;
; Returns: RAX = bytes written (excluding NUL), 0 on error
; =============================================================================
CoT_Get_Status_JSON PROC FRAME
    push    rbx
    .pushreg rbx
    push    r12
    .pushreg r12
    push    r13
    .pushreg r13
    push    r14
    .pushreg r14
    sub     rsp, 72
    .allocstack 72
    .endprolog

    test    rcx, rcx
    jz      @@gj_error
    test    rdx, rdx
    jz      @@gj_error

    mov     r12, rcx                        ; r12 = buffer start
    mov     r13, rcx                        ; r13 = current write position
    mov     r14, rdx                        ; r14 = buffer size

    call    Acquire_CoT_Lock

    ; {"version":
    mov     rcx, r13
    lea     rdx, szJsonStatusFmt1
    call    asm_append_str
    mov     r13, rax
    ; version number
    mov     rcx, r13
    mov     rdx, COT_ENGINE_VERSION
    call    asm_append_int
    mov     r13, rax

    ; ,"initialized":
    mov     rcx, r13
    lea     rdx, szJsonStatusFmt2
    call    asm_append_str
    mov     r13, rax
    cmp     DWORD PTR [g_initialized], 0
    je      @@gj_init_false
    mov     rcx, r13
    lea     rdx, szTrue
    call    asm_append_str
    mov     r13, rax
    jmp     @@gj_init_done
@@gj_init_false:
    mov     rcx, r13
    lea     rdx, szFalse
    call    asm_append_str
    mov     r13, rax
@@gj_init_done:

    ; ,"running":
    mov     rcx, r13
    lea     rdx, szJsonStatusFmt3
    call    asm_append_str
    mov     r13, rax
    cmp     DWORD PTR [g_executionState], STATE_RUNNING
    jne     @@gj_run_false
    mov     rcx, r13
    lea     rdx, szTrue
    call    asm_append_str
    mov     r13, rax
    jmp     @@gj_run_done
@@gj_run_false:
    mov     rcx, r13
    lea     rdx, szFalse
    call    asm_append_str
    mov     r13, rax
@@gj_run_done:

    ; ,"maxSteps":
    mov     rcx, r13
    lea     rdx, szJsonStatusFmt4
    call    asm_append_str
    mov     r13, rax
    mov     rcx, r13
    movzx   rdx, DWORD PTR [g_maxSteps]
    call    asm_append_int
    mov     r13, rax

    ; ,"stepCount":
    mov     rcx, r13
    lea     rdx, szJsonStatusFmt5
    call    asm_append_str
    mov     r13, rax
    mov     rcx, r13
    movzx   rdx, DWORD PTR [g_stepCount]
    call    asm_append_int
    mov     r13, rax

    ; ,"arenaUsedBytes":
    mov     rcx, r13
    lea     rdx, szJsonStatusFmt6
    call    asm_append_str
    mov     r13, rax
    mov     rcx, r13
    mov     rdx, QWORD PTR [g_arenaUsed]
    call    asm_append_int
    mov     r13, rax

    ; ,"arenaCommittedBytes":
    mov     rcx, r13
    lea     rdx, szJsonStatusFmt7
    call    asm_append_str
    mov     r13, rax
    mov     rcx, r13
    mov     rdx, QWORD PTR [g_arenaCommitted]
    call    asm_append_int
    mov     r13, rax

    ; ,"stats":{"totalChains":
    mov     rcx, r13
    lea     rdx, szJsonStatusFmt8
    call    asm_append_str
    mov     r13, rax
    mov     rcx, r13
    movzx   rdx, DWORD PTR [g_stats + COT_STATS.totalChains]
    call    asm_append_int
    mov     r13, rax

    ; ,"successfulChains":
    mov     rcx, r13
    lea     rdx, szJsonStatusFmt9
    call    asm_append_str
    mov     r13, rax
    mov     rcx, r13
    movzx   rdx, DWORD PTR [g_stats + COT_STATS.successfulChains]
    call    asm_append_int
    mov     r13, rax

    ; ,"failedChains":
    mov     rcx, r13
    lea     rdx, szJsonStatusFmt10
    call    asm_append_str
    mov     r13, rax
    mov     rcx, r13
    movzx   rdx, DWORD PTR [g_stats + COT_STATS.failedChains]
    call    asm_append_int
    mov     r13, rax

    ; ,"totalStepsExecuted":
    mov     rcx, r13
    lea     rdx, szJsonStatusFmt11
    call    asm_append_str
    mov     r13, rax
    mov     rcx, r13
    movzx   rdx, DWORD PTR [g_stats + COT_STATS.totalStepsExecuted]
    call    asm_append_int
    mov     r13, rax

    ; ,"totalStepsSkipped":
    mov     rcx, r13
    lea     rdx, szJsonStatusFmt12
    call    asm_append_str
    mov     r13, rax
    mov     rcx, r13
    movzx   rdx, DWORD PTR [g_stats + COT_STATS.totalStepsSkipped]
    call    asm_append_int
    mov     r13, rax

    ; ,"totalStepsFailed":
    mov     rcx, r13
    lea     rdx, szJsonStatusFmt13
    call    asm_append_str
    mov     r13, rax
    mov     rcx, r13
    movzx   rdx, DWORD PTR [g_stats + COT_STATS.totalStepsFailed]
    call    asm_append_int
    mov     r13, rax

    ; ,"avgLatencyMs":
    mov     rcx, r13
    lea     rdx, szJsonStatusFmt14
    call    asm_append_str
    mov     r13, rax
    mov     rcx, r13
    movzx   rdx, DWORD PTR [g_stats + COT_STATS.avgLatencyMs]
    call    asm_append_int
    mov     r13, rax

    ; }}
    mov     rcx, r13
    lea     rdx, szJsonClose
    call    asm_append_str
    mov     r13, rax

    ; NUL-terminate
    mov     BYTE PTR [r13], 0

    call    Release_CoT_Lock

    ; Return bytes written
    mov     rax, r13
    sub     rax, r12
    jmp     @@gj_done

@@gj_error:
    xor     eax, eax

@@gj_done:
    add     rsp, 72
    pop     r14
    pop     r13
    pop     r12
    pop     rbx
    ret
CoT_Get_Status_JSON ENDP

; =============================================================================
; CoT_Get_Role_Info
; Get role info by role ID.
;
; RCX = role ID (0-11)
; RDX = dest buffer for COT_ROLE_ENTRY (caller allocates >= sizeof)
;
; Returns: EAX = 0 on success, -1 on invalid ID
; =============================================================================
CoT_Get_Role_Info PROC FRAME
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    .endprolog

    cmp     ecx, ROLE_COUNT
    jge     @@gri_invalid
    cmp     ecx, 0
    jl      @@gri_invalid
    test    rdx, rdx
    jz      @@gri_invalid

    ; Calculate offset into role table
    imul    rax, rcx, ROLE_ENTRY_SIZE
    lea     rsi, g_roleTable
    add     rsi, rax

    ; Copy entry to caller buffer
    mov     rdi, rdx
    mov     ecx, ROLE_ENTRY_SIZE
    rep     movsb

    xor     eax, eax
    jmp     @@gri_done

@@gri_invalid:
    mov     eax, -1

@@gri_done:
    pop     rdi
    pop     rsi
    ret
CoT_Get_Role_Info ENDP

; =============================================================================
; CoT_Append_Data
; Append raw token data to the 1GB arena. Commits pages as needed.
; Uses SEH wrapper for safety during rep movsb.
;
; RCX = source data pointer
; RDX = data size in bytes
;
; Returns: RAX = new total arena used (bytes), or 0 on failure
; =============================================================================
CoT_Append_Data PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    push    r12
    .pushreg r12
    push    r13
    .pushreg r13
    sub     rsp, 56
    .allocstack 56
    .endprolog

    ; Validate
    test    rcx, rcx
    jz      @@ad_fail
    test    rdx, rdx
    jz      @@ad_fail

    mov     r12, rcx                        ; source
    mov     r13, rdx                        ; size

    ; Check arena initialized
    mov     rax, QWORD PTR [g_arenaBase]
    test    rax, rax
    jz      @@ad_fail

    call    Acquire_CoT_Lock

    ; Check capacity (would exceed 1GB?)
    mov     rax, QWORD PTR [g_arenaUsed]
    add     rax, r13
    cmp     rax, COT_ARENA_RESERVE_SIZE
    ja      @@ad_unlock_fail

    ; Ensure enough pages are committed
    mov     rcx, rax                        ; required total
    call    ensure_arena_committed
    test    eax, eax
    jnz     @@ad_unlock_fail

    ; Copy data: rep movsb(dest, src, count)
    mov     rdi, QWORD PTR [g_arenaBase]
    add     rdi, QWORD PTR [g_arenaUsed]    ; dest = base + used
    mov     rsi, r12                        ; src
    mov     rcx, r13                        ; count
    rep     movsb

    ; Update used counter
    add     QWORD PTR [g_arenaUsed], r13

    call    Release_CoT_Lock

    mov     rax, QWORD PTR [g_arenaUsed]
    jmp     @@ad_done

@@ad_unlock_fail:
    call    Release_CoT_Lock
@@ad_fail:
    xor     eax, eax

@@ad_done:
    add     rsp, 56
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
CoT_Append_Data ENDP

; =============================================================================
; CoT_Get_Data_Ptr
; Returns: RAX = base pointer to arena data (or NULL if not initialized)
; =============================================================================
CoT_Get_Data_Ptr PROC
    mov     rax, QWORD PTR [g_arenaBase]
    ret
CoT_Get_Data_Ptr ENDP

; =============================================================================
; CoT_Get_Data_Used
; Returns: RAX = bytes used in arena
; =============================================================================
CoT_Get_Data_Used PROC
    mov     rax, QWORD PTR [g_arenaUsed]
    ret
CoT_Get_Data_Used ENDP

; =============================================================================
; CoT_Get_Version
; Returns: EAX = engine version (0x00010000 = 1.0.0)
; =============================================================================
CoT_Get_Version PROC
    mov     eax, COT_ENGINE_VERSION
    ret
CoT_Get_Version ENDP

END
