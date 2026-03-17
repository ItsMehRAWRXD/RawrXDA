;  ============================================================================
;  IDE_15_AUTH.asm  –  JWT validation and rate limiting
;  ============================================================================
;  Implements:
;    - AgentPolicy_CheckSafety (JWT + rate-limit gate)
;    - AgentPolicy_Enforce (sandbox / deny list)
;    - Auth_ValidateJWT / RateLimitCheck
;  ============================================================================

include ..\agent_core\IDE_INC.ASM

; ============================================================================
;  EXPORTS
; ============================================================================
PUBLIC AgentPolicy_CheckSafety
PUBLIC AgentPolicy_Enforce
PUBLIC Auth_ValidateJWT
PUBLIC Auth_RateLimitCheck

; ============================================================================
;  CONSTANTS
; ============================================================================
MAX_RATE_BUCKETS  equ 256
RATE_LIMIT_PER_MIN equ 60
TOKEN_BUCKET_SIZE equ 100

; ============================================================================
;  RATE LIMIT BUCKET STRUCTURE
; ============================================================================
RATE_BUCKET STRUCT
    tool_id     DWORD ?
    tokens      DWORD ?
    last_refill QWORD ?
RATE_BUCKET ENDS

; ============================================================================
;  DATA
; ============================================================================
.data
g_rate_buckets  RATE_BUCKET MAX_RATE_BUCKETS dup(<>)

szCheckSafety   db "AgentPolicy_CheckSafety", 0
szEnforce       db "AgentPolicy_Enforce", 0
szValidateJWT   db "Auth_ValidateJWT", 0
szRateLimit     db "Auth_RateLimitCheck", 0

; Deny list (tools that require explicit approval)
deny_list       db "exec_shell", 0
                db "delete_file", 0
                db "modify_registry", 0
                db "network_raw", 0
                db 0, 0

; ============================================================================
;  CODE
; ============================================================================
.code

; ============================================================================
;  Auth_ValidateJWT – Validate JWT token
;  Input:  pToken = JWT token string
;  Output: eax = HRESULT
; ============================================================================
Auth_ValidateJWT PROC uses ebx esi, pToken:DWORD
    LOCAL start:QWORD, dur:QWORD, hr:SDWORD
    
    PERF_INIT offset start
    call _enter_crit
    
    .if pToken == 0
        mov hr, E_INVALIDARG
        jmp done
    .endif
    
    ; TODO: Real JWT validation:
    ; 1. Parse header.payload.signature
    ; 2. Verify signature with public key
    ; 3. Check expiration
    ; 4. Validate issuer/audience
    
    ; For now, just check for non-empty token
    invoke lstrlenA, pToken
    .if eax == 0
        mov hr, E_FAIL
        jmp done
    .endif
    
    mov hr, 0
    
done:
    call _leave_crit
    PERF_DELTA offset start, offset dur, offset dur
    
    LOG offset szValidateJWT, hr, dword ptr [dur]
    
    mov eax, hr
    ret
Auth_ValidateJWT ENDP

; ============================================================================
;  Auth_RateLimitCheck – Token bucket rate limiter
;  Input:  toolId = tool being invoked
;  Output: eax = HRESULT (0 = allowed, E_FAIL = rate limited)
; ============================================================================
Auth_RateLimitCheck PROC uses ebx esi edi, toolId:DWORD
    LOCAL start:QWORD, dur:QWORD, hr:SDWORD
    LOCAL pBucket:PTR RATE_BUCKET
    LOCAL now:QWORD
    LOCAL elapsed:DWORD
    
    PERF_INIT offset start
    call _enter_crit
    
    ; Get current time
    invoke GetTickCount
    mov dword ptr now, eax
    xor eax, eax
    mov dword ptr now+4, eax
    
    ; Find or create bucket for this tool
    mov ecx, toolId
    and ecx, 0FFh          ; Mod 256
    imul ecx, sizeof RATE_BUCKET
    lea esi, g_rate_buckets
    add esi, ecx
    mov pBucket, esi
    
    ; Initialize if new
    .if (RATE_BUCKET PTR [esi]).tool_id == 0
        mov eax, toolId
        mov (RATE_BUCKET PTR [esi]).tool_id, eax
        mov (RATE_BUCKET PTR [esi]).tokens, TOKEN_BUCKET_SIZE
        mov eax, dword ptr now
        mov dword ptr (RATE_BUCKET PTR [esi]).last_refill, eax
    .endif
    
    ; Refill tokens based on time elapsed
    mov eax, dword ptr now
    mov edx, dword ptr (RATE_BUCKET PTR [esi]).last_refill
    sub eax, edx
    mov elapsed, eax
    
    ; Refill: 1 token per second
    mov eax, elapsed
    mov ecx, 1000
    xor edx, edx
    div ecx
    
    ; Add tokens (capped at bucket size)
    mov ecx, (RATE_BUCKET PTR [esi]).tokens
    add ecx, eax
    .if ecx > TOKEN_BUCKET_SIZE
        mov ecx, TOKEN_BUCKET_SIZE
    .endif
    mov (RATE_BUCKET PTR [esi]).tokens, ecx
    
    ; Update refill time
    mov eax, dword ptr now
    mov dword ptr (RATE_BUCKET PTR [esi]).last_refill, eax
    
    ; Check if we have tokens available
    .if (RATE_BUCKET PTR [esi]).tokens == 0
        mov hr, E_FAIL        ; Rate limited
        jmp done
    .endif
    
    ; Consume one token
    dec (RATE_BUCKET PTR [esi]).tokens
    mov hr, 0
    
done:
    call _leave_crit
    PERF_DELTA offset start, offset dur, offset dur
    
    LOG offset szRateLimit, hr, dword ptr [dur]
    
    mov eax, hr
    ret
Auth_RateLimitCheck ENDP

; ============================================================================
;  AgentPolicy_CheckSafety – Combined safety gate
;  Input:  toolId = tool to check
;          pToken = JWT token (can be NULL)
;  Output: eax = HRESULT
; ============================================================================
AgentPolicy_CheckSafety PROC uses ebx, toolId:DWORD, pToken:DWORD
    LOCAL start:QWORD, dur:QWORD, hr:SDWORD
    
    PERF_INIT offset start
    
    ; 1. Check rate limit
    invoke Auth_RateLimitCheck, toolId
    .if eax < 0
        mov hr, eax
        jmp done
    .endif
    
    ; 2. Validate JWT if provided
    .if pToken != 0
        invoke Auth_ValidateJWT, pToken
        .if eax < 0
            mov hr, eax
            jmp done
        .endif
    .endif
    
    mov hr, 0
    
done:
    PERF_DELTA offset start, offset dur, offset dur
    LOG offset szCheckSafety, hr, dword ptr [dur]
    
    mov eax, hr
    ret
AgentPolicy_CheckSafety ENDP

; ============================================================================
;  AgentPolicy_Enforce – Check deny list
;  Input:  pToolName = tool name to check
;  Output: eax = HRESULT (0 = allowed, E_FAIL = denied)
; ============================================================================
AgentPolicy_Enforce PROC uses ebx esi edi, pToolName:DWORD
    LOCAL start:QWORD, dur:QWORD, hr:SDWORD
    
    PERF_INIT offset start
    call _enter_crit
    
    .if pToolName == 0
        mov hr, E_INVALIDARG
        jmp done
    .endif
    
    ; Check deny list
    mov esi, offset deny_list
    
check_loop:
    ; Check for end of list
    movzx eax, byte ptr [esi]
    .if al == 0
        movzx eax, byte ptr [esi+1]
        .if al == 0
            ; End of list, tool is allowed
            mov hr, 0
            jmp done
        .endif
    .endif
    
    ; Compare with tool name
    invoke lstrcmpA, esi, pToolName
    .if eax == 0
        ; Found in deny list
        mov hr, E_FAIL
        jmp done
    .endif
    
    ; Move to next entry
    invoke lstrlenA, esi
    lea esi, [esi + eax + 1]
    jmp check_loop
    
done:
    call _leave_crit
    PERF_DELTA offset start, offset dur, offset dur
    
    LOG offset szEnforce, hr, dword ptr [dur]
    
    mov eax, hr
    ret
AgentPolicy_Enforce ENDP

END
