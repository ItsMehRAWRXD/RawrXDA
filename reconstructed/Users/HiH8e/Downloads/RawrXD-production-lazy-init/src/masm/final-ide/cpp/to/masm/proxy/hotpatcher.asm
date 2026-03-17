; proxy_hotpatcher_masm.asm
; Pure MASM x64 - Proxy Hotpatcher (converted from C++ ProxyHotpatcher class)
; Agentic correction proxy with token manipulation and validation

option casemap:none

EXTERN malloc:PROC
EXTERN free:PROC
EXTERN memset:PROC
EXTERN memcpy:PROC
EXTERN memcmp:PROC
EXTERN strlen:PROC
EXTERN strcpy:PROC
EXTERN sprintf:PROC
EXTERN console_log:PROC
EXTERN GetSystemTimeAsFileTime:PROC

; Proxy constants
MAX_CORRECTIONS EQU 100
MAX_VALIDATORS EQU 50
MAX_TOKENS EQU 1000
TOKEN_BUFFER_SIZE EQU 65536        ; 64 KB

; ============================================================================
; DATA STRUCTURES
; ============================================================================

; PROXY_RESULT - Proxy operation result
PROXY_RESULT STRUCT
    success BYTE ?                  ; True if successful
    detail QWORD ?                  ; Result detail
    errorCode DWORD ?               ; Error code
    correctedBytes QWORD ?          ; Number of bytes corrected
    elapsedMs QWORD ?               ; Execution time
ENDS

; TOKEN_CORRECTION - Token correction definition
TOKEN_CORRECTION STRUCT
    name QWORD ?                    ; Correction name
    description QWORD ?             ; Correction description
    type DWORD ?                    ; Correction type enum
    tokenId DWORD ?                 ; Token ID
    originalValue DWORD ?           ; Original token value
    correctedValue DWORD ?          ; Corrected token value
    enabled BYTE ?                  ; Whether correction is enabled
    applied BYTE ?                  ; Whether correction is applied
    timesApplied DWORD ?            ; Number of times applied
ENDS

; VALIDATOR - Response validator
VALIDATOR STRUCT
    name QWORD ?                    ; Validator name
    description QWORD ?             ; Validator description
    validatorFunc QWORD ?           ; Validator function pointer
    enabled BYTE ?                  ; Whether validator is enabled
ENDS

; PROXY_HOTPATCHER - Proxy state
PROXY_HOTPATCHER STRUCT
    corrections QWORD ?             ; Array of TOKEN_CORRECTION
    correctionCount DWORD ?         ; Current correction count
    maxCorrections DWORD ?          ; Capacity
    
    validators QWORD ?              ; Array of VALIDATOR
    validatorCount DWORD ?          ; Current validator count
    maxValidators DWORD ?           ; Capacity
    
    tokenBuffer QWORD ?             ; Token buffer
    tokenCount DWORD ?              ; Current token count
    maxTokens DWORD ?               ; Capacity
    
    ; Statistics
    totalCorrectionsApplied QWORD ?
    totalTokensCorrected QWORD ?
    totalValidations DWORD ?
    totalErrors DWORD ?
    
    ; Callbacks
    correctionAppliedCallback QWORD ? ; Called when correction applied
    validationFailedCallback QWORD ?  ; Called on validation failure
    errorOccurredCallback QWORD ?   ; Called on error
    
    initialized BYTE ?
ENDS

; ============================================================================
; GLOBAL DATA
; ============================================================================

.data
    szProxyCreated DB "[PROXY_HOTPATCHER] Created with %d validators", 0
    szCorrectionApplied DB "[PROXY_HOTPATCHER] Correction applied: %s (token=%d)", 0
    szCorrectionFailed DB "[PROXY_HOTPATCHER] Correction failed: %s (error=%d)", 0
    szValidationPassed DB "[PROXY_HOTPATCHER] Validation passed: %s", 0
    szValidationFailed DB "[PROXY_HOTPATCHER] Validation failed: %s", 0
    szTokenBufferFull DB "[PROXY_HOTPATCHER] Token buffer full: %d/%d", 0

; Correction types
CORRECTION_TYPE_BIAS_ADJUST EQU 0
CORRECTION_TYPE_TOKEN_SWAP EQU 1
CORRECTION_TYPE_RST_INJECTION EQU 2
CORRECTION_TYPE_FORMAT_FIX EQU 3
CORRECTION_TYPE_SAFETY_FILTER EQU 4
CORRECTION_TYPE_CUSTOM EQU 5

; Token constants
TOKEN_RST EQU 0                    ; End of stream token
TOKEN_EOS EQU 1                    ; End of sequence

.code

; ============================================================================
; PUBLIC API
; ============================================================================

; proxy_hotpatcher_create(RCX = initialValidatorCount)
; Create proxy hotpatcher
; Returns: RAX = pointer to PROXY_HOTPATCHER
PUBLIC proxy_hotpatcher_create
proxy_hotpatcher_create PROC
    push rbx
    
    mov ebx, ecx                    ; ebx = initialValidatorCount
    
    ; Allocate proxy
    mov rcx, SIZEOF PROXY_HOTPATCHER
    call malloc
    mov r10, rax
    
    ; Allocate corrections array
    mov rcx, MAX_CORRECTIONS
    imul rcx, SIZEOF TOKEN_CORRECTION
    call malloc
    mov [r10 + PROXY_HOTPATCHER.corrections], rax
    
    ; Allocate validators array
    mov rcx, MAX_VALIDATORS
    imul rcx, SIZEOF VALIDATOR
    call malloc
    mov [r10 + PROXY_HOTPATCHER.validators], rax
    
    ; Allocate token buffer
    mov rcx, TOKEN_BUFFER_SIZE
    call malloc
    mov [r10 + PROXY_HOTPATCHER.tokenBuffer], rax
    
    ; Initialize
    mov [r10 + PROXY_HOTPATCHER.correctionCount], 0
    mov [r10 + PROXY_HOTPATCHER.maxCorrections], MAX_CORRECTIONS
    mov [r10 + PROXY_HOTPATCHER.validatorCount], 0
    mov [r10 + PROXY_HOTPATCHER.maxValidators], MAX_VALIDATORS
    mov [r10 + PROXY_HOTPATCHER.tokenCount], 0
    mov [r10 + PROXY_HOTPATCHER.maxTokens], MAX_TOKENS
    mov [r10 + PROXY_HOTPATCHER.totalCorrectionsApplied], 0
    mov [r10 + PROXY_HOTPATCHER.totalTokensCorrected], 0
    mov [r10 + PROXY_HOTPATCHER.totalValidations], 0
    mov [r10 + PROXY_HOTPATCHER.totalErrors], 0
    
    mov byte [r10 + PROXY_HOTPATCHER.initialized], 1
    
    ; Log
    lea rcx, [szProxyCreated]
    mov edx, ebx
    call console_log
    
    mov rax, r10
    pop rbx
    ret
proxy_hotpatcher_create ENDP

; ============================================================================

; proxy_apply_correction(RCX = proxy, RDX = correction)
; Apply token correction
; Returns: RAX = pointer to PROXY_RESULT
PUBLIC proxy_apply_correction
proxy_apply_correction PROC
    push rbx
    push rsi
    
    mov rbx, rcx                    ; rbx = proxy
    mov rsi, rdx                    ; rsi = correction
    
    ; Check if correction enabled
    cmp byte [rsi + TOKEN_CORRECTION.enabled], 1
    jne .correction_disabled
    
    ; Check token bounds
    mov eax, [rsi + TOKEN_CORRECTION.tokenId]
    cmp eax, [rbx + PROXY_HOTPATCHER.tokenCount]
    jge .token_out_of_bounds
    
    ; Get start time
    call GetSystemTimeAsFileTime
    mov r8, rax                     ; r8 = start time
    
    ; Allocate result
    mov rcx, SIZEOF PROXY_RESULT
    call malloc
    mov r9, rax                     ; r9 = result
    
    ; Apply correction based on type
    mov eax, [rsi + TOKEN_CORRECTION.type]
    
    cmp eax, CORRECTION_TYPE_BIAS_ADJUST
    je .apply_bias_adjust
    cmp eax, CORRECTION_TYPE_TOKEN_SWAP
    je .apply_token_swap
    cmp eax, CORRECTION_TYPE_RST_INJECTION
    je .apply_rst_injection
    cmp eax, CORRECTION_TYPE_FORMAT_FIX
    je .apply_format_fix
    cmp eax, CORRECTION_TYPE_SAFETY_FILTER
    je .apply_safety_filter
    cmp eax, CORRECTION_TYPE_CUSTOM
    je .apply_custom
    
    jmp .unknown_type
    
.apply_bias_adjust:
    ; Bias adjustment
    mov rcx, rbx
    mov edx, [rsi + TOKEN_CORRECTION.tokenId]
    mov r8d, [rsi + TOKEN_CORRECTION.correctedValue]
    call apply_bias_adjustment
    jmp .correction_applied
    
.apply_token_swap:
    ; Token swap
    mov rcx, rbx
    mov edx, [rsi + TOKEN_CORRECTION.tokenId]
    mov r8d, [rsi + TOKEN_CORRECTION.correctedValue]
    call apply_token_swap
    jmp .correction_applied
    
.apply_rst_injection:
    ; RST injection
    mov rcx, rbx
    mov edx, [rsi + TOKEN_CORRECTION.tokenId]
    call apply_rst_injection
    jmp .correction_applied
    
.apply_format_fix:
    ; Format fix
    mov rcx, rbx
    mov edx, [rsi + TOKEN_CORRECTION.tokenId]
    mov r8d, [rsi + TOKEN_CORRECTION.correctedValue]
    call apply_format_fix
    jmp .correction_applied
    
.apply_safety_filter:
    ; Safety filter
    mov rcx, rbx
    mov edx, [rsi + TOKEN_CORRECTION.tokenId]
    mov r8d, [rsi + TOKEN_CORRECTION.correctedValue]
    call apply_safety_filter
    jmp .correction_applied
    
.apply_custom:
    ; Custom correction
    mov rcx, rbx
    mov rdx, rsi
    call apply_custom_correction
    jmp .correction_applied
    
.correction_applied:
    ; Get end time
    call GetSystemTimeAsFileTime
    sub rax, r8                     ; rax = elapsed time
    mov [r9 + PROXY_RESULT.elapsedMs], rax
    
    ; Set result
    mov byte [r9 + PROXY_RESULT.success], 1
    lea rax, [szCorrectionAppliedDetail]
    mov [r9 + PROXY_RESULT.detail], rax
    mov [r9 + PROXY_RESULT.errorCode], 0
    mov [r9 + PROXY_RESULT.correctedBytes], 1
    
    ; Update correction status
    mov byte [rsi + TOKEN_CORRECTION.applied], 1
    inc dword [rsi + TOKEN_CORRECTION.timesApplied]
    
    ; Update statistics
    inc qword [rbx + PROXY_HOTPATCHER.totalCorrectionsApplied]
    inc qword [rbx + PROXY_HOTPATCHER.totalTokensCorrected]
    
    ; Log success
    lea rcx, [szCorrectionApplied]
    mov rdx, [rsi + TOKEN_CORRECTION.name]
    mov r8d, [rsi + TOKEN_CORRECTION.tokenId]
    call console_log
    
    mov rax, r9                     ; Return result
    pop rsi
    pop rbx
    ret
    
.token_out_of_bounds:
.unknown_type:
.correction_disabled:
    ; Set error result
    mov byte [r9 + PROXY_RESULT.success], 0
    lea rax, [szCorrectionFailedDetail]
    mov [r9 + PROXY_RESULT.detail], rax
    mov [r9 + PROXY_RESULT.errorCode], 1
    mov [r9 + PROXY_RESULT.correctedBytes], 0
    
    ; Log failure
    lea rcx, [szCorrectionFailed]
    mov rdx, [rsi + TOKEN_CORRECTION.name]
    mov r8d, 1
    call console_log
    
    inc dword [rbx + PROXY_HOTPATCHER.totalErrors]
    
    mov rax, r9
    pop rsi
    pop rbx
    ret
proxy_apply_correction ENDP

; ============================================================================

; proxy_add_correction(RCX = proxy, RDX = name, R8 = description, R9d = type)
; Add token correction
; Returns: RAX = correction ID (0 on error)
PUBLIC proxy_add_correction
proxy_add_correction PROC
    push rbx
    push rsi
    
    mov rbx, rcx                    ; rbx = proxy
    mov rsi, rdx                    ; rsi = name
    mov r10, r8                     ; r10 = description
    mov r11d, r9d                   ; r11d = type
    
    ; Check capacity
    mov r12d, [rbx + PROXY_HOTPATCHER.correctionCount]
    cmp r12d, [rbx + PROXY_HOTPATCHER.maxCorrections]
    jge .capacity_exceeded
    
    ; Get correction slot
    mov r13, [rbx + PROXY_HOTPATCHER.corrections]
    mov r14, r12
    imul r14, SIZEOF TOKEN_CORRECTION
    add r13, r14
    
    ; Store correction name
    mov rcx, rsi
    call strlen
    inc rax
    call malloc
    mov [r13 + TOKEN_CORRECTION.name], rax
    
    mov rcx, rsi
    mov rdx, rax
    call strcpy
    
    ; Store correction description
    mov rcx, r10
    call strlen
    inc rax
    call malloc
    mov [r13 + TOKEN_CORRECTION.description], rax
    
    mov rcx, r10
    mov rdx, rax
    call strcpy
    
    ; Set correction properties
    mov [r13 + TOKEN_CORRECTION.type], r11d
    mov byte [r13 + TOKEN_CORRECTION.enabled], 1
    mov byte [r13 + TOKEN_CORRECTION.applied], 0
    mov [r13 + TOKEN_CORRECTION.timesApplied], 0
    
    ; Set default token values
    mov [r13 + TOKEN_CORRECTION.tokenId], 0
    mov [r13 + TOKEN_CORRECTION.originalValue], 0
    mov [r13 + TOKEN_CORRECTION.correctedValue], 0
    
    ; Increment correction count
    inc dword [rbx + PROXY_HOTPATCHER.correctionCount]
    
    mov eax, r12d                   ; Return correction ID
    pop rsi
    pop rbx
    ret
    
.capacity_exceeded:
    xor rax, rax
    pop rsi
    pop rbx
    ret
proxy_add_correction ENDP

; ============================================================================

; proxy_add_validator(RCX = proxy, RDX = name, R8 = description, R9 = validatorFunc)
; Add response validator
; Returns: RAX = validator ID (0 on error)
PUBLIC proxy_add_validator
proxy_add_validator PROC
    push rbx
    push rsi
    
    mov rbx, rcx                    ; rbx = proxy
    mov rsi, rdx                    ; rsi = name
    mov r10, r8                     ; r10 = description
    mov r11, r9                     ; r11 = validatorFunc
    
    ; Check capacity
    mov r12d, [rbx + PROXY_HOTPATCHER.validatorCount]
    cmp r12d, [rbx + PROXY_HOTPATCHER.maxValidators]
    jge .capacity_exceeded
    
    ; Get validator slot
    mov r13, [rbx + PROXY_HOTPATCHER.validators]
    mov r14, r12
    imul r14, SIZEOF VALIDATOR
    add r13, r14
    
    ; Store validator name
    mov rcx, rsi
    call strlen
    inc rax
    call malloc
    mov [r13 + VALIDATOR.name], rax
    
    mov rcx, rsi
    mov rdx, rax
    call strcpy
    
    ; Store validator description
    mov rcx, r10
    call strlen
    inc rax
    call malloc
    mov [r13 + VALIDATOR.description], rax
    
    mov rcx, r10
    mov rdx, rax
    call strcpy
    
    ; Set validator properties
    mov [r13 + VALIDATOR.validatorFunc], r11
    mov byte [r13 + VALIDATOR.enabled], 1
    
    ; Increment validator count
    inc dword [rbx + PROXY_HOTPATCHER.validatorCount]
    
    mov eax, r12d                   ; Return validator ID
    pop rsi
    pop rbx
    ret
    
.capacity_exceeded:
    xor rax, rax
    pop rsi
    pop rbx
    ret
proxy_add_validator ENDP

; ============================================================================

; proxy_validate_response(RCX = proxy, RDX = responseData, R8 = responseSize)
; Validate response using all validators
; Returns: RAX = validation result (0 = failed, 1 = passed)
PUBLIC proxy_validate_response
proxy_validate_response PROC
    push rbx
    push rsi
    push rdi
    
    mov rbx, rcx                    ; rbx = proxy
    mov rsi, rdx                    ; rsi = responseData
    mov rdi, r8                     ; rdi = responseSize
    
    ; Get validators
    mov r8, [rbx + PROXY_HOTPATCHER.validators]
    mov r9d, [rbx + PROXY_HOTPATCHER.validatorCount]
    xor r10d, r10d                  ; r10d = validator index
    
.validation_loop:
    cmp r10d, r9d
    jge .validation_complete
    
    ; Get current validator
    mov r11, r8
    mov r12, r10
    imul r12, SIZEOF VALIDATOR
    add r11, r12
    
    ; Check if validator enabled
    cmp byte [r11 + VALIDATOR.enabled], 1
    jne .skip_validator
    
    ; Call validator function
    mov rcx, rsi                    ; responseData
    mov rdx, rdi                    ; responseSize
    call qword [r11 + VALIDATOR.validatorFunc]
    
    test rax, rax
    jz .validation_failed
    
    ; Log validation passed
    lea rcx, [szValidationPassed]
    mov rdx, [r11 + VALIDATOR.name]
    call console_log
    
.skip_validator:
    inc r10d
    jmp .validation_loop
    
.validation_complete:
    ; Update statistics
    inc dword [rbx + PROXY_HOTPATCHER.totalValidations]
    
    mov rax, 1                      ; Return success
    pop rdi
    pop rsi
    pop rbx
    ret
    
.validation_failed:
    ; Log validation failed
    lea rcx, [szValidationFailed]
    mov rdx, [r11 + VALIDATOR.name]
    call console_log
    
    mov rax, 0                      ; Return failure
    pop rdi
    pop rsi
    pop rbx
    ret
proxy_validate_response ENDP

; ============================================================================

; proxy_add_token(RCX = proxy, RDX = tokenValue)
; Add token to buffer
; Returns: RAX = token ID (0 on error)
PUBLIC proxy_add_token
proxy_add_token PROC
    push rbx
    
    mov rbx, rcx                    ; rbx = proxy
    mov r8d, edx                    ; r8d = tokenValue
    
    ; Check capacity
    mov r9d, [rbx + PROXY_HOTPATCHER.tokenCount]
    cmp r9d, [rbx + PROXY_HOTPATCHER.maxTokens]
    jge .buffer_full
    
    ; Get token buffer
    mov r10, [rbx + PROXY_HOTPATCHER.tokenBuffer]
    mov r11, r9
    imul r11, 4                     ; DWORD per token
    add r10, r11
    
    ; Store token
    mov dword [r10], r8d
    
    ; Increment token count
    inc dword [rbx + PROXY_HOTPATCHER.tokenCount]
    
    mov eax, r9d                    ; Return token ID
    pop rbx
    ret
    
.buffer_full:
    ; Log buffer full
    lea rcx, [szTokenBufferFull]
    mov edx, r9d
    mov r8d, [rbx + PROXY_HOTPATCHER.maxTokens]
    call console_log
    
    xor rax, rax
    pop rbx
    ret
proxy_add_token ENDP

; ============================================================================

; proxy_get_correction(RCX = proxy, RDX = correctionId)
; Get correction by ID
; Returns: RAX = pointer to TOKEN_CORRECTION
PUBLIC proxy_get_correction
proxy_get_correction PROC
    mov r8, [rcx + PROXY_HOTPATCHER.corrections]
    mov r9d, [rcx + PROXY_HOTPATCHER.correctionCount]
    xor r10d, r10d
    
.find_correction:
    cmp r10d, r9d
    jge .correction_not_found
    
    mov r11, r8
    mov r12, r10
    imul r12, SIZEOF TOKEN_CORRECTION
    add r11, r12
    
    cmp r10d, edx
    je .correction_found
    
    inc r10d
    jmp .find_correction
    
.correction_found:
    mov rax, r11
    ret
    
.correction_not_found:
    xor rax, rax
    ret
proxy_get_correction ENDP

; ============================================================================

; proxy_get_validator(RCX = proxy, RDX = validatorId)
; Get validator by ID
; Returns: RAX = pointer to VALIDATOR
PUBLIC proxy_get_validator
proxy_get_validator PROC
    mov r8, [rcx + PROXY_HOTPATCHER.validators]
    mov r9d, [rcx + PROXY_HOTPATCHER.validatorCount]
    xor r10d, r10d
    
.find_validator:
    cmp r10d, r9d
    jge .validator_not_found
    
    mov r11, r8
    mov r12, r10
    imul r12, SIZEOF VALIDATOR
    add r11, r12
    
    cmp r10d, edx
    je .validator_found
    
    inc r10d
    jmp .find_validator
    
.validator_found:
    mov rax, r11
    ret
    
.validator_not_found:
    xor rax, rax
    ret
proxy_get_validator ENDP

; ============================================================================

; proxy_get_statistics(RCX = proxy, RDX = statsBuffer)
; Get proxy statistics
PUBLIC proxy_get_statistics
proxy_get_statistics PROC
    mov [rdx + 0], qword [rcx + PROXY_HOTPATCHER.totalCorrectionsApplied]
    mov [rdx + 8], qword [rcx + PROXY_HOTPATCHER.totalTokensCorrected]
    mov [rdx + 16], dword [rcx + PROXY_HOTPATCHER.totalValidations]
    mov [rdx + 20], dword [rcx + PROXY_HOTPATCHER.totalErrors]
    ret
proxy_get_statistics ENDP

; ============================================================================

; proxy_destroy(RCX = proxy)
; Free proxy hotpatcher
PUBLIC proxy_destroy
proxy_destroy PROC
    push rbx
    
    mov rbx, rcx
    
    ; Free corrections array
    mov r10, [rbx + PROXY_HOTPATCHER.corrections]
    mov r11d, [rbx + PROXY_HOTPATCHER.correctionCount]
    xor r12d, r12d
    
.free_corrections:
    cmp r12d, r11d
    jge .corrections_freed
    
    mov r13, r10
    mov r14, r12
    imul r14, SIZEOF TOKEN_CORRECTION
    add r13, r14
    
    mov rcx, [r13 + TOKEN_CORRECTION.name]
    cmp rcx, 0
    je .skip_correction_name
    call free
    
.skip_correction_name:
    mov rcx, [r13 + TOKEN_CORRECTION.description]
    cmp rcx, 0
    je .skip_correction_desc
    call free
    
.skip_correction_desc:
    inc r12d
    jmp .free_corrections
    
.corrections_freed:
    mov rcx, [rbx + PROXY_HOTPATCHER.corrections]
    cmp rcx, 0
    je .skip_corrections_array
    call free
    
.skip_corrections_array:
    ; Free validators array
    mov r10, [rbx + PROXY_HOTPATCHER.validators]
    mov r11d, [rbx + PROXY_HOTPATCHER.validatorCount]
    xor r12d, r12d
    
.free_validators:
    cmp r12d, r11d
    jge .validators_freed
    
    mov r13, r10
    mov r14, r12
    imul r14, SIZEOF VALIDATOR
    add r13, r14
    
    mov rcx, [r13 + VALIDATOR.name]
    cmp rcx, 0
    je .skip_validator_name
    call free
    
.skip_validator_name:
    mov rcx, [r13 + VALIDATOR.description]
    cmp rcx, 0
    je .skip_validator_desc
    call free
    
.skip_validator_desc:
    inc r12d
    jmp .free_validators
    
.validators_freed:
    mov rcx, [rbx + PROXY_HOTPATCHER.validators]
    cmp rcx, 0
    je .skip_validators_array
    call free
    
.skip_validators_array:
    ; Free token buffer
    mov rcx, [rbx + PROXY_HOTPATCHER.tokenBuffer]
    cmp rcx, 0
    je .skip_token_buffer
    call free
    
.skip_token_buffer:
    ; Free proxy
    mov rcx, rbx
    call free
    
    pop rbx
    ret
proxy_destroy ENDP

; ============================================================================
; HELPER FUNCTIONS
; ============================================================================

; Bias adjustment
apply_bias_adjustment PROC
    ; RCX = proxy, RDX = tokenId, R8d = biasValue
    push rbx
    
    mov rbx, rcx
    
    ; Get token buffer
    mov r9, [rbx + PROXY_HOTPATCHER.tokenBuffer]
    mov r10, rdx
    imul r10, 4
    add r9, r10
    
    ; Apply bias adjustment
    mov eax, dword [r9]
    add eax, r8d
    mov dword [r9], eax
    
    pop rbx
    ret
apply_bias_adjustment ENDP

; Token swap
apply_token_swap PROC
    ; RCX = proxy, RDX = tokenId, R8d = newValue
    push rbx
    
    mov rbx, rcx
    
    ; Get token buffer
    mov r9, [rbx + PROXY_HOTPATCHER.tokenBuffer]
    mov r10, rdx
    imul r10, 4
    add r9, r10
    
    ; Swap token value
    mov dword [r9], r8d
    
    pop rbx
    ret
apply_token_swap ENDP

; RST injection
apply_rst_injection PROC
    ; RCX = proxy, RDX = tokenId
    push rbx
    
    mov rbx, rcx
    
    ; Get token buffer
    mov r9, [rbx + PROXY_HOTPATCHER.tokenBuffer]
    mov r10, rdx
    imul r10, 4
    add r9, r10
    
    ; Inject RST token
    mov dword [r9], TOKEN_RST
    
    pop rbx
    ret
apply_rst_injection ENDP

; Format fix
apply_format_fix PROC
    ; RCX = proxy, RDX = tokenId, R8d = formatValue
    push rbx
    
    mov rbx, rcx
    
    ; Get token buffer
    mov r9, [rbx + PROXY_HOTPATCHER.tokenBuffer]
    mov r10, rdx
    imul r10, 4
    add r9, r10
    
    ; Apply format fix
    mov dword [r9], r8d
    
    pop rbx
    ret
apply_format_fix ENDP

; Safety filter
apply_safety_filter PROC
    ; RCX = proxy, RDX = tokenId, R8d = safeValue
    push rbx
    
    mov rbx, rcx
    
    ; Get token buffer
    mov r9, [rbx + PROXY_HOTPATCHER.tokenBuffer]
    mov r10, rdx
    imul r10, 4
    add r9, r10
    
    ; Apply safety filter
    mov dword [r9], r8d
    
    pop rbx
    ret
apply_safety_filter ENDP

; Custom correction
apply_custom_correction PROC
    ; RCX = proxy, RDX = correction
    ; Custom correction logic would go here
    ret
apply_custom_correction ENDP

; ============================================================================

.data
    szCorrectionAppliedDetail DB "Correction applied successfully", 0
    szCorrectionFailedDetail DB "Correction application failed", 0

END
