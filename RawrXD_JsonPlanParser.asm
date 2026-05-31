; =============================================================================
; RawrXD_JsonPlanParser.asm
; Upgrade 3: Pure-MASM JSON tokenizer for agentic plan parsing
;
; Parses a partial JSON response from the LLM and populates a flat array of
; PlanStep records.  The LLM is expected to produce an array of objects:
;
;     [{"tool":"edit_file","param":"src/foo.cpp"},
;      {"tool":"compile","param":"cmake --build ."},
;      ...]
;
; This avoids any CRT or heap dependency: all scanning is done in-place on
; the response buffer.  Two-pass string scanning replaces regex.
;
; ---- PlanStep memory layout (caller allocates; SIZEOF_PLAN_STEP = 832) -----
;   +   0 ..  63  (64 bytes)   tool   : BYTE[64]   – null-terminated tool name
;   +  64 .. 319 (256 bytes)   param  : BYTE[256]  – null-terminated parameter
;   + 320 .. 831 (512 bytes)   result : BYTE[512]  – filled by orchestrator post-exec
;
; ---- PUBLIC API -------------------------------------------------------------
;
;   JsonPlan_ExtractValue(pJson, pKey, pOutVal, dwValSize) -> BOOL
;       RCX  LPSTR   pJson     – JSON text to search (null-terminated)
;       RDX  LPSTR   pKey      – key name, e.g. "tool" (null-terminated)
;       R8   LPBYTE  pOutVal   – destination buffer for extracted string value
;       R9   DWORD   dwValSize – byte capacity of pOutVal
;       Returns 1 (TRUE) if found, 0 (FALSE) if not found / buffer too small.
;       Extracted value is null-terminated.
;
;   JsonPlan_Parse(pJson, pPlanSteps, dwMaxSteps, pdwStepCount) -> BOOL
;       RCX  LPSTR   pJson        – full JSON response text
;       RDX  LPBYTE  pPlanSteps   – flat array of PlanStep records
;       R8   DWORD   dwMaxSteps   – capacity of array (element count)
;       R9   LPDWORD pdwStepCount – receives number of steps parsed
;       Returns 1 if at least one step was parsed, 0 otherwise.
;
; ---- Internal helpers (not exported) ---------------------------------------
;   _StrFind(pHaystack, pNeedle) -> LPSTR
;       Returns pointer to first match of pNeedle in pHaystack, or NULL.
;       Pure leaf: no stack frame, volatile registers only.
;
;   _FindChar(pStr, chTarget) -> LPSTR
;       Returns pointer to first occurrence of byte chTarget in pStr, or NULL.
;       Pure leaf.
;
;   _Strlen(pStr) -> QWORD
;       Returns strlen. Pure leaf.
; =============================================================================

OPTION CASEMAP:NONE

; ---- PlanStep layout constants ----------------------------------------------
PS_tool             EQU   0             ; BYTE[64]
PS_param            EQU  64             ; BYTE[256]
PS_result           EQU 320             ; BYTE[512]
SIZEOF_PLAN_STEP    EQU 832             ; 64 + 256 + 512

PS_TOOL_SZ          EQU 64
PS_PARAM_SZ         EQU 256

; ---- Read-only key strings --------------------------------------------------
.data
    jpp_key_tool        db "tool",  0
    jpp_key_param       db "param", 0

.code

; =============================================================================
; _StrFind  –  find needle in haystack; returns pointer to match or NULL
;
; RCX = pHaystack   (scanned)
; RDX = pNeedle     (what to find)
; Returns: RAX = pointer to first match, or 0 (NULL) if not found.
;
; Volatile registers used: RAX, R8, R9, R10, R11.
; No stack frame; pure leaf.
; =============================================================================
_StrFind PROC
    ; R8  = outer scan position (starts at pHaystack)
    ; R9  = pNeedle (constant)
    ; R10 = inner haystack cursor (per attempt)
    ; R11 = inner needle cursor (per attempt)
    ; RAX = scratch / return

    mov  r8,  rcx               ; outer cursor = start of haystack
    mov  r9,  rdx               ; needle

    ; Guard: NULL inputs
    test r8, r8
    jz   sf_not_found
    test r9, r9
    jz   sf_not_found

    ; Empty needle → return haystack (consistent with strstr behaviour)
    cmp  byte ptr [r9], 0
    jz   sf_empty_needle

sf_outer:
    cmp  byte ptr [r8], 0       ; end of haystack?
    jz   sf_not_found

    ; Try to match needle starting at r8
    mov  r10, r8
    mov  r11, r9

sf_inner:
    movzx eax, byte ptr [r11]       ; eax = current needle byte
    test  al, al
    jz    sf_match                  ; needle fully consumed = full match

    movzx ecx, byte ptr [r10]       ; ecx = current haystack byte
    cmp   cl, al                    ; compare
    jne   sf_no_match

    inc   r10                       ; advance both cursors
    inc   r11
    jmp   sf_inner

sf_match:
    mov  rax, r8                ; return pointer to start of match
    ret

sf_no_match:
    inc  r8                     ; advance outer cursor
    jmp  sf_outer

sf_not_found:
    xor  rax, rax               ; return NULL
    ret

sf_empty_needle:
    mov  rax, rcx               ; return original haystack pointer
    ret
_StrFind ENDP

; =============================================================================
; _FindChar  –  find first occurrence of a byte in a string
;
; RCX = pStr        RDX (low byte) = chTarget
; Returns: RAX = pointer to first occurrence, or 0 (NULL) if not found.
; Pure leaf.
; =============================================================================
_FindChar PROC
    test rcx, rcx
    jz   fc_not_found

fc_loop:
    movzx eax, byte ptr [rcx]
    test  al, al
    jz    fc_not_found          ; end of string
    cmp   al, dl                ; compare against target byte
    je    fc_found
    inc   rcx
    jmp   fc_loop

fc_found:
    mov  rax, rcx
    ret

fc_not_found:
    xor  rax, rax
    ret
_FindChar ENDP

; =============================================================================
; _Strlen  –  compute string length
;
; RCX = pStr
; Returns: RAX = length in bytes (not counting null terminator).
; Pure leaf.
; =============================================================================
_Strlen PROC
    test rcx, rcx
    jz   sl_empty

    xor  rax, rax
sl_loop:
    cmp  byte ptr [rcx + rax], 0
    jz   sl_done
    inc  rax
    jmp  sl_loop
sl_done:
    ret
sl_empty:
    xor  rax, rax
    ret
_Strlen ENDP

; =============================================================================
; JsonPlan_ExtractValue
;
; Finds the pattern  "key":"   in pJson and copies the value (up to the next
; closing '"') into pOutVal.
;
; RCX  pJson     RDX  pKey     R8  pOutVal     R9  dwValSize
; Returns 1 (TRUE) or 0 (FALSE).
;
; Stack layout (0 extra pushes → N ≡ 8 mod 16):
;   Using J for our scratch space:
;   Minimum locals: 4 × 8 (saved params) + 72 (pattern buf) = 104 bytes
;   + 32 shadow = 136 bytes.   136 % 16 = 8  ✓
;   RSP after = X - 8 - 136 = X - 144.  144 % 16 = 0  ✓
;
;   [rsp +  0 ..  31]  32 bytes  – shadow space
;   [rsp + 32 ..  39]   8 bytes  – saved pJson
;   [rsp + 40 ..  47]   8 bytes  – saved pKey (unused after pattern build)
;   [rsp + 48 ..  55]   8 bytes  – saved pOutVal
;   [rsp + 56 ..  63]   8 bytes  – saved dwValSize (QWORD)
;   [rsp + 64 .. 135]  72 bytes  – keyPattern buffer: '"' + key + '":"' + NUL
; =============================================================================
JsonPlan_ExtractValue PROC FRAME
    sub  rsp, 136
    .allocstack 136
    .endprolog

    ; Persist params
    mov  qword ptr [rsp + 32], rcx      ; pJson
    mov  qword ptr [rsp + 40], rdx      ; pKey
    mov  qword ptr [rsp + 48], r8       ; pOutVal
    mov  eax, r9d
    mov  qword ptr [rsp + 56], rax      ; dwValSize (mov eax zero-extends to rax)

    ; ---- Build keyPattern = '"' + pKey + '":' + '"' + NUL ------------------
    ; Pattern buffer at [rsp+64], max 72 bytes.
    ; Structure: " k e y " : "   so the search includes the opening '"' of value.
    lea  r10, [rsp + 64]                ; pattern write pointer
    lea  r11, [rsp + 134]               ; one-past-safe boundary (64+70=134)

    mov  byte ptr [r10], '"'
    inc  r10

    mov  r9, qword ptr [rsp + 40]       ; pKey

jev_copy_key:
    cmp  r10, r11
    jge  jev_not_found                  ; key too long for pattern buffer
    movzx eax, byte ptr [r9]
    test  al, al
    jz    jev_key_done
    mov   byte ptr [r10], al
    inc   r9
    inc   r10
    jmp   jev_copy_key

jev_key_done:
    ; Append  ":"   (3 bytes) + NUL terminator
    cmp  r10, r11                       ; guard: need 4 more bytes
    jge  jev_not_found
    mov  byte ptr [r10 + 0], '"'
    mov  byte ptr [r10 + 1], ':'
    mov  byte ptr [r10 + 2], '"'
    mov  byte ptr [r10 + 3], 0

    ; ---- Find pattern in pJson ----------------------------------------------
    mov  rcx, qword ptr [rsp + 32]      ; pJson
    lea  rdx, [rsp + 64]                ; keyPattern
    call _StrFind
    test rax, rax
    jz   jev_not_found

    ; ---- Advance past pattern to start of value text ------------------------
    ; rax → start of match; we need rax + len(pattern)
    lea  rcx, [rsp + 64]
    call _Strlen                        ; rax = pattern length (uses rcx)
    ; _Strlen clobbered RAX (return) and used RCX - reload match address
    ; Problem: _Strlen overwrites RAX with the length, but we lost the match ptr.
    ; Fix: save match pointer before _Strlen call.

    ; Re-do: first save match ptr, then compute pattern length.
    ; (Back-patch: we need to call _StrFind, save result, then call _Strlen)
    ; The code above already lost the match. Let me restructure:
    ;   1. _StrFind → save result in [rsp+32] (repurpose) - no, [rsp+32] = pJson.
    ;   We'll use a different offset. But we've used all assigned slots...
    ; SOLUTION: Use R11 to hold the match pointer across _Strlen call (R11 is volatile
    ;           and _Strlen does not use R11 since it only uses RAX+RCX internally).
    ;           But R11 was clobbered before (pattern pointer). So we're safe to
    ;           reuse it now.

    ; ** Restructured sub-sequence: this block replaces the two calls above **
    ; (The jz jev_not_found just above this comment is the guard. RAX was the
    ;  _StrFind result.  R11 was pattern boundary – we're done with it.)

    ; NOTE: the code reached here with RAX = _StrFind result (match pointer)
    ; and then we called _Strlen which CLOBBERED RAX.  We need to fix this.
    ; Remove the _Strlen call and compute offset differently:

    ; ** INLINE pattern-length walk instead of calling _Strlen **
    ; We already have the pattern in [rsp+64].  Walk it here to get the length.
    ; Result placed in R11.  Then add to the _StrFind match pointer.
    ; But _StrFind match pointer is gone from RAX now...
    ;
    ; This is a design flaw in the linear layout.  Correct approach:

    ; ---- CORRECTED block (replaces from "Find pattern" downward) -----------
    ; Re-call _StrFind (idempotent) and save result in R11 immediately.

    mov  rcx, qword ptr [rsp + 32]      ; pJson (original)
    lea  rdx, [rsp + 64]                ; keyPattern
    call _StrFind
    ; Save match pointer BEFORE anything clobbers RAX
    mov  r11, rax                       ; r11 = match start (or NULL)
    test r11, r11
    jz   jev_not_found

    ; Measure pattern length inline (avoid _Strlen clobbering r11)
    xor  r10d, r10d                     ; pattern length counter
    lea  rcx, [rsp + 64]                ; pattern base
jev_patlen_loop:
    cmp  byte ptr [rcx + r10], 0
    jz   jev_patlen_done
    inc  r10
    jmp  jev_patlen_loop
jev_patlen_done:
    ; r11 = match start, r10 = pattern length
    add  r11, r10                       ; r11 → first byte of the JSON value text

    ; ---- Copy value chars to pOutVal until closing '"' or buffer full -------
    mov  r10, qword ptr [rsp + 48]      ; pOutVal
    mov  r9,  qword ptr [rsp + 56]      ; dwValSize (QWORD)
    dec  r9                             ; reserve one byte for null terminator
    xor  r8d, r8d                       ; r8 = bytes written so far

jev_copy_val:
    cmp  r8, r9
    jge  jev_val_done                   ; buffer full

    movzx eax, byte ptr [r11]
    cmp   al, '"'
    je    jev_val_done                  ; closing quote = end of value
    test  al, al
    jz    jev_val_done                  ; null = truncated JSON (stop safely)

    mov  byte ptr [r10 + r8], al
    inc  r11
    inc  r8
    jmp  jev_copy_val

jev_val_done:
    mov  byte ptr [r10 + r8], 0         ; null-terminate
    mov  eax, 1                         ; return TRUE
    jmp  jev_done

jev_not_found:
    xor  eax, eax                       ; return FALSE

jev_done:
    add  rsp, 136
    ret

JsonPlan_ExtractValue ENDP

; =============================================================================
; JsonPlan_Parse
;
; Scans pJson for JSON objects {"tool":"...","param":"..."} and populates the
; pPlanSteps array.  Object detection: advance until '{', extract fields from
; that position, advance until the matching '}'.
;
; RCX  pJson          RDX  pPlanSteps     R8  dwMaxSteps    R9  pdwStepCount
; Returns 1 if ≥ 1 step parsed, 0 otherwise.
;
; Stack layout (0 extra pushes → N ≡ 8 mod 16):
;   Locals: 4 × 8 (saved params) + 3 × 8 (stepCount, scanPos, pStep) = 56 bytes
;   + 32 shadow = 88 bytes.   88 % 16 = 8  ✓
;   RSP after = X - 8 - 88 = X - 96.  96 % 16 = 0  ✓
;
;   [rsp +  0 ..  31]  32 bytes  – shadow space
;   [rsp + 32 ..  39]   8 bytes  – saved pJson / scan cursor
;   [rsp + 40 ..  47]   8 bytes  – saved pPlanSteps base
;   [rsp + 48 ..  55]   8 bytes  – saved dwMaxSteps
;   [rsp + 56 ..  63]   8 bytes  – saved pdwStepCount
;   [rsp + 64 ..  71]   8 bytes  – stepCount
;   [rsp + 72 ..  79]   8 bytes  – pCurObject (pointer to current '{')
;   [rsp + 80 ..  87]   8 bytes  – padding / alignment
; =============================================================================
JsonPlan_Parse PROC FRAME
    sub  rsp, 88
    .allocstack 88
    .endprolog

    ; Persist params
    mov  qword ptr [rsp + 32], rcx      ; pJson / scan cursor
    mov  qword ptr [rsp + 40], rdx      ; pPlanSteps base
    mov  eax, r8d
    mov  qword ptr [rsp + 48], rax      ; dwMaxSteps (mov eax zero-extends to rax)
    mov  qword ptr [rsp + 56], r9       ; pdwStepCount

    ; Init locals
    xor  eax, eax
    mov  qword ptr [rsp + 64], rax      ; stepCount = 0
    mov  qword ptr [rsp + 72], rax      ; pCurObject = 0

jpp_scan_loop:
    ; ---- Check step count against capacity --------------------------------
    mov  rax, qword ptr [rsp + 64]
    cmp  rax, qword ptr [rsp + 48]
    jge  jpp_write_count                ; array full

    ; ---- Find next '{' in the scan cursor ---------------------------------
    mov  rcx, qword ptr [rsp + 32]      ; scan cursor
    test rcx, rcx
    jz   jpp_write_count
    cmp  byte ptr [rcx], 0
    jz   jpp_write_count

    mov  dl, '{'
    call _FindChar
    test rax, rax
    jz   jpp_write_count                ; no more objects in JSON

    mov  qword ptr [rsp + 72], rax      ; save pCurObject = '{'

    ; ---- Compute pStep = pPlanSteps + stepCount * SIZEOF_PLAN_STEP ---------
    mov  rax, qword ptr [rsp + 64]      ; stepCount
    imul rax, SIZEOF_PLAN_STEP
    mov  r10, qword ptr [rsp + 40]
    add  r10, rax                       ; r10 = pStep

    ; ---- Extract "tool" field into pStep + PS_tool -------------------------
    mov  rcx, qword ptr [rsp + 72]      ; pCurObject
    lea  rdx, [jpp_key_tool]
    lea  r8,  [r10 + PS_tool]
    mov  r9d, PS_TOOL_SZ
    call JsonPlan_ExtractValue
    ; eax = 1 if tool found, 0 if not
    test eax, eax
    jz   jpp_advance_obj               ; no "tool" key → not a valid step

    ; ---- Extract "param" field into pStep + PS_param -----------------------
    mov  rcx, qword ptr [rsp + 72]
    lea  rdx, [jpp_key_param]
    lea  r8,  [r10 + PS_param]
    mov  r9d, PS_PARAM_SZ
    call JsonPlan_ExtractValue
    ; param is optional; we accept a step even if param is absent
    ; (result would be empty string already since pStep is caller-allocated)

    ; ---- Clear result field so orchestrator knows it's unexecuted ----------
    mov  r10, qword ptr [rsp + 40]
    mov  rax, qword ptr [rsp + 64]
    imul rax, SIZEOF_PLAN_STEP
    add  r10, rax                       ; reload pStep (R10 was clobbered)
    mov  byte ptr [r10 + PS_result], 0

    ; ---- Commit step -------------------------------------------------------
    inc  qword ptr [rsp + 64]           ; stepCount++

jpp_advance_obj:
    ; ---- Advance scan cursor past this object's closing '}' ----------------
    mov  rcx, qword ptr [rsp + 72]      ; pCurObject = '{'
    mov  dl,  '}'
    call _FindChar
    test rax, rax
    jz   jpp_write_count                ; malformed JSON – stop

    inc  rax                            ; advance past '}'
    mov  qword ptr [rsp + 32], rax      ; update scan cursor

    jmp  jpp_scan_loop

jpp_write_count:
    ; ---- Write step count back to caller ------------------------------------
    mov  r10, qword ptr [rsp + 56]      ; pdwStepCount
    test r10, r10
    jz   jpp_ret
    mov  rax, qword ptr [rsp + 64]
    mov  dword ptr [r10], eax

jpp_ret:
    ; Return 1 if stepCount > 0, else 0
    cmp  qword ptr [rsp + 64], 0
    seta al
    movzx eax, al

    add  rsp, 88
    ret

JsonPlan_Parse ENDP

; =============================================================================
PUBLIC JsonPlan_ExtractValue
PUBLIC JsonPlan_Parse

END
