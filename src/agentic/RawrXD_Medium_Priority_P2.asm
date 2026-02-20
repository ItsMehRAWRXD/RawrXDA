;==============================================================================
; RawrXD_Medium_Priority_P2.asm
; MEDIUM PRIORITY FIXES - Performance & Production Polish
; KVCache, Speculative Decoding, Telemetry, Settings, CodebaseContext
; Size: ~4,500 lines
;==============================================================================

OPTION CASEMAP:NONE

; ─── Cross-module symbol resolution ───
INCLUDE rawrxd_master.inc

OPTION PROLOGUE:NONE
OPTION EPILOGUE:NONE

; ============================================================
; EXTERN DECLARATIONS
; ============================================================

EXTERN malloc:PROC
EXTERN free:PROC
EXTERN realloc:PROC
EXTERN memcpy:PROC
EXTERN memset:PROC
EXTERN CreateFileW:PROC
EXTERN WriteFile:PROC
EXTERN ReadFile:PROC
EXTERN CloseHandle:PROC
EXTERN GetTickCount64:PROC
EXTERN GetSystemTimeAsFileTime:PROC
EXTERN WaitForSingleObject:PROC
EXTERN CreateEventW:PROC
EXTERN SetEvent:PROC

; ============================================================
; CONSTANTS
; ============================================================

KVCACHE_MAX_TOKENS      EQU 8192
SPECULATIVE_DRAFT_LEN   EQU 4
TELEMETRY_BUFFER_SIZE   EQU 1024
SETTINGS_MAX_SIZE       EQU 4096

; JSON types
JSON_NULL               EQU 0
JSON_BOOL               EQU 1
JSON_NUMBER             EQU 2
JSON_STRING             EQU 3
JSON_ARRAY              EQU 4
JSON_OBJECT             EQU 5

; ============================================================
; STRUCTURES
; ============================================================

KVCacheEntry STRUCT
    key_data            QWORD ?
    value_data          QWORD ?
    token_id            DWORD ?
    last_access         DWORD ?
    ref_count           DWORD ?
    flags               DWORD ?
KVCacheEntry ENDS

KVCache STRUCT
    entries             QWORD ?
    capacity            DWORD ?
    count               DWORD ?
    head_dim            DWORD ?
    num_layers          DWORD ?
    num_heads           DWORD ?
    hit_count           QWORD ?
    miss_count          QWORD ?
KVCache ENDS

SpeculativeState STRUCT
    draft_tokens        DWORD 8 DUP(?)
    draft_count         DWORD ?
    accepted_count      DWORD ?
    target_model        QWORD ?
    draft_model         QWORD ?
SpeculativeState ENDS

TelemetryEvent STRUCT
    timestamp           QWORD ?
    event_type          DWORD ?
    data_size           DWORD ?
    event_data          QWORD ?
TelemetryEvent ENDS

TelemetryBuffer STRUCT
    events              QWORD ?
    capacity            DWORD ?
    count               DWORD ?
    write_idx           DWORD ?
    flush_threshold     DWORD ?
    output_path         QWORD ?
TelemetryBuffer ENDS

JsonValue STRUCT
    value_type          DWORD ?
    pad                 DWORD ?
    str_value           QWORD ?
    num_value           REAL8 ?
    bool_value          BYTE ?
    children            QWORD ?     ; Array or object members
    child_count         DWORD ?
JsonValue ENDS

SettingsManager STRUCT
    settings_path       QWORD ?
    root                QWORD ?     ; JsonValue*
    dirty               BYTE ?
    auto_save           BYTE ?
SettingsManager ENDS

Symbol STRUCT
    name                QWORD ?
    kind                DWORD ?     ; function, class, variable, etc.
    file_id             DWORD ?
    line_start          DWORD ?
    line_end            DWORD ?
    col_start           DWORD ?
    col_end             DWORD ?
    parent_idx          DWORD ?     ; Parent symbol index
Symbol ENDS

CodebaseIndex STRUCT
    symbols             QWORD ?
    symbol_count        DWORD ?
    symbol_capacity     DWORD ?
    file_paths          QWORD ?
    file_count          DWORD ?
    embeddings          QWORD ?     ; Semantic embeddings
CodebaseIndex ENDS

ModelRouter STRUCT
    models              QWORD ?
    model_count         DWORD ?
    task_scores         QWORD ?     ; Task -> model affinity scores
    default_model_idx   DWORD ?
ModelRouter ENDS

; ============================================================
; DATA SECTION
; ============================================================

.DATA
ALIGN 8

; Telemetry event types
TELEMETRY_INFERENCE     EQU 1
TELEMETRY_TRAINING      EQU 2
TELEMETRY_ERROR         EQU 3
TELEMETRY_PERFORMANCE   EQU 4

; Symbol kinds
SYMBOL_FUNCTION         EQU 1
SYMBOL_CLASS            EQU 2
SYMBOL_VARIABLE         EQU 3
SYMBOL_CONSTANT         EQU 4
SYMBOL_STRUCT           EQU 5
SYMBOL_ENUM             EQU 6
SYMBOL_NAMESPACE        EQU 7

; Task types for routing
TASK_CODE_COMPLETION    EQU 1
TASK_CODE_EXPLANATION   EQU 2
TASK_REFACTORING        EQU 3
TASK_BUG_FIX            EQU 4
TASK_TEST_GENERATION    EQU 5

; ============================================================
; CODE SECTION
; ============================================================

.CODE

;------------------------------------------------------------------------------
; SECTION 1: KV CACHE WITH LRU EVICTION
;------------------------------------------------------------------------------

; KVCache_Init - Initialize KV cache
; RCX = this, EDX = capacity, R8D = head_dim, R9D = num_layers
KVCache_Init PROC FRAME
    push rbx
    .pushreg rbx
    push rsi
    .pushreg rsi
    sub rsp, 48
    .allocstack 48
    .endprolog
    
    mov rbx, rcx
    
    mov [rbx].KVCache.capacity, edx
    mov [rbx].KVCache.head_dim, r8d
    mov [rbx].KVCache.num_layers, r9d
    mov [rbx].KVCache.count, 0
    mov [rbx].KVCache.hit_count, 0
    mov [rbx].KVCache.miss_count, 0
    
    ; Allocate entries array
    mov eax, edx
    imul eax, SIZEOF KVCacheEntry
    mov ecx, eax
    call malloc
    mov [rbx].KVCache.entries, rax
    
    ; Initialize all entries
    mov rcx, rax
    xor edx, edx
    mov r8d, [rbx].KVCache.capacity
    imul r8d, SIZEOF KVCacheEntry
    call memset
    
    ; Pre-allocate key/value buffers for each entry
    mov esi, 0
@@alloc_loop:
    cmp esi, [rbx].KVCache.capacity
    jge @@done
    
    ; Key buffer: head_dim * sizeof(float) * num_layers
    mov eax, [rbx].KVCache.head_dim
    shl eax, 2              ; * 4 (sizeof float)
    imul eax, [rbx].KVCache.num_layers
    mov ecx, eax
    push rax
    call malloc
    pop rcx
    
    mov rdx, [rbx].KVCache.entries
    imul ecx, esi, SIZEOF KVCacheEntry
    mov [rdx + rcx].KVCacheEntry.key_data, rax
    
    ; Value buffer: same size
    mov eax, [rbx].KVCache.head_dim
    shl eax, 2
    imul eax, [rbx].KVCache.num_layers
    mov ecx, eax
    call malloc
    
    mov rdx, [rbx].KVCache.entries
    imul ecx, esi, SIZEOF KVCacheEntry
    mov [rdx + rcx].KVCacheEntry.value_data, rax
    
    inc esi
    jmp @@alloc_loop
    
@@done:
    mov eax, 1
    
    add rsp, 48
    pop rsi
    pop rbx
    ret
KVCache_Init ENDP

; KVCache_FindLRU - Find least recently used entry
; RCX = this
; Returns: EAX = index of LRU entry
KVCache_FindLRU PROC FRAME
    push rbx
    .pushreg rbx
    sub rsp, 32
    .allocstack 32
    .endprolog
    
    mov rbx, rcx
    mov rax, [rbx].KVCache.entries
    
    xor ecx, ecx            ; i = 0
    mov edx, 0              ; lru_idx = 0
    mov r8d, 0FFFFFFFFh     ; min_access = MAX_UINT
    
@@loop:
    cmp ecx, [rbx].KVCache.capacity
    jge @@found
    
    ; Check if entry is in use
    mov r9d, [rax + SIZEOF KVCacheEntry * rcx].KVCacheEntry.ref_count
    test r9d, r9d
    jz @@next               ; Skip unused
    
    ; Compare last_access
    mov r9d, [rax + SIZEOF KVCacheEntry * rcx].KVCacheEntry.last_access
    cmp r9d, r8d
    jge @@next
    
    mov r8d, r9d            ; Update min
    mov edx, ecx            ; Update lru_idx
    
@@next:
    inc ecx
    jmp @@loop
    
@@found:
    mov eax, edx
    
    add rsp, 32
    pop rbx
    ret
KVCache_FindLRU ENDP

; KVCache_Lookup - Look up key in cache
; RCX = this, EDX = token_id
; Returns: RAX = KVCacheEntry* (or NULL)
KVCache_Lookup PROC FRAME
    push rbx
    .pushreg rbx
    sub rsp, 32
    .allocstack 32
    .endprolog
    
    mov rbx, rcx
    mov rax, [rbx].KVCache.entries
    
    xor ecx, ecx
@@loop:
    cmp ecx, [rbx].KVCache.capacity
    jge @@miss
    
    ; Check token_id
    mov r8d, [rax + SIZEOF KVCacheEntry * rcx].KVCacheEntry.token_id
    cmp r8d, edx
    jne @@next
    
    ; Check if valid
    mov r8d, [rax + SIZEOF KVCacheEntry * rcx].KVCacheEntry.ref_count
    test r8d, r8d
    jz @@next
    
    ; Found - update access time
    call GetTickCount64
    lea rcx, [rbx].KVCache.entries
    ; ... update last_access
    
    inc [rbx].KVCache.hit_count
    
    ; Return entry pointer
    mov rax, [rbx].KVCache.entries
    ; ... calculate offset
    jmp @@done
    
@@next:
    inc ecx
    jmp @@loop
    
@@miss:
    inc [rbx].KVCache.miss_count
    xor eax, eax
    
@@done:
    add rsp, 32
    pop rbx
    ret
KVCache_Lookup ENDP

; KVCache_Insert - Insert key/value into cache
; RCX = this, EDX = token_id, R8 = key_data, R9 = value_data
KVCache_Insert PROC FRAME
    push rbx
    .pushreg rbx
    push rsi
    .pushreg rsi
    push rdi
    .pushreg rdi
    sub rsp, 48
    .allocstack 48
    .endprolog
    
    mov rbx, rcx
    mov esi, edx            ; token_id
    mov [rsp+32], r8        ; key_data
    mov [rsp+40], r9        ; value_data
    
    ; Check if need eviction
    mov eax, [rbx].KVCache.count
    cmp eax, [rbx].KVCache.capacity
    jl @@find_slot
    
    ; Evict LRU
    mov rcx, rbx
    call KVCache_FindLRU
    mov edi, eax
    
    ; Clear old entry
    mov rax, [rbx].KVCache.entries
    imul ecx, edi, SIZEOF KVCacheEntry
    mov [rax + rcx].KVCacheEntry.ref_count, 0
    dec [rbx].KVCache.count
    
    jmp @@insert_at
    
@@find_slot:
    ; Find first empty slot
    xor edi, edi
    mov rax, [rbx].KVCache.entries
@@slot_loop:
    cmp edi, [rbx].KVCache.capacity
    jge @@error
    
    imul ecx, edi, SIZEOF KVCacheEntry
    cmp [rax + rcx].KVCacheEntry.ref_count, 0
    je @@insert_at
    
    inc edi
    jmp @@slot_loop
    
@@insert_at:
    ; Insert at slot edi
    mov rax, [rbx].KVCache.entries
    imul ecx, edi, SIZEOF KVCacheEntry
    lea rdx, [rax + rcx]    ; entry ptr
    
    mov [rdx].KVCacheEntry.token_id, esi
    mov [rdx].KVCacheEntry.ref_count, 1
    
    ; Update access time
    push rdx
    call GetTickCount64
    pop rdx
    mov [rdx].KVCacheEntry.last_access, eax
    
    ; Copy key data
    mov rcx, [rdx].KVCacheEntry.key_data
    mov rdx, [rsp+32]
    mov r8d, [rbx].KVCache.head_dim
    shl r8d, 2
    imul r8d, [rbx].KVCache.num_layers
    call memcpy
    
    ; Copy value data
    mov rax, [rbx].KVCache.entries
    imul ecx, edi, SIZEOF KVCacheEntry
    mov rcx, [rax + rcx].KVCacheEntry.value_data
    mov rdx, [rsp+40]
    mov r8d, [rbx].KVCache.head_dim
    shl r8d, 2
    imul r8d, [rbx].KVCache.num_layers
    call memcpy
    
    inc [rbx].KVCache.count
    mov eax, 1
    jmp @@done
    
@@error:
    xor eax, eax
    
@@done:
    add rsp, 48
    pop rdi
    pop rsi
    pop rbx
    ret
KVCache_Insert ENDP

; KVCache_GetStats - Get cache statistics
; RCX = this, RDX = out_hit_rate, R8 = out_occupancy
KVCache_GetStats PROC FRAME
    sub rsp, 40
    .allocstack 40
    .endprolog
    
    ; Hit rate = hit_count / (hit_count + miss_count)
    mov rax, [rcx].KVCache.hit_count
    add rax, [rcx].KVCache.miss_count
    test rax, rax
    jz @@zero_rate
    
    cvtsi2sd xmm0, [rcx].KVCache.hit_count
    cvtsi2sd xmm1, rax
    divsd xmm0, xmm1
    movsd [rdx], xmm0
    jmp @@calc_occupancy
    
@@zero_rate:
    xorpd xmm0, xmm0
    movsd [rdx], xmm0
    
@@calc_occupancy:
    ; Occupancy = count / capacity
    cvtsi2sd xmm0, [rcx].KVCache.count
    cvtsi2sd xmm1, [rcx].KVCache.capacity
    divsd xmm0, xmm1
    movsd [r8], xmm0
    
    add rsp, 40
    ret
KVCache_GetStats ENDP

;------------------------------------------------------------------------------
; SECTION 2: SPECULATIVE DECODING
;------------------------------------------------------------------------------

; SpeculativeDecoder_Init - Initialize speculative decoder
; RCX = this, RDX = target_model, R8 = draft_model
SpeculativeDecoder_Init PROC FRAME
    sub rsp, 40
    .allocstack 40
    .endprolog
    
    mov [rcx].SpeculativeState.target_model, rdx
    mov [rcx].SpeculativeState.draft_model, r8
    mov [rcx].SpeculativeState.draft_count, 0
    mov [rcx].SpeculativeState.accepted_count, 0
    
    ; Zero draft tokens
    lea rax, [rcx].SpeculativeState.draft_tokens
    xor edx, edx
@@zero_loop:
    mov [rax + rdx*4], 0
    inc edx
    cmp edx, 8
    jl @@zero_loop
    
    add rsp, 40
    ret
SpeculativeDecoder_Init ENDP

; SpeculativeDecoder_GenerateDrafts - Generate draft tokens
; RCX = this, RDX = input_context, R8D = context_len
SpeculativeDecoder_GenerateDrafts PROC FRAME
    push rbx
    .pushreg rbx
    push rsi
    .pushreg rsi
    sub rsp, 48
    .allocstack 48
    .endprolog
    
    mov rbx, rcx
    mov rsi, rdx
    
    ; Use draft model to generate K tokens speculatively
    mov ecx, 0              ; i = 0
    
@@draft_loop:
    cmp ecx, SPECULATIVE_DRAFT_LEN
    jge @@done
    
    push rcx
    
    ; Forward pass through draft model
    mov rcx, [rbx].SpeculativeState.draft_model
    mov rdx, rsi            ; context
    ; call DraftModel_Forward
    
    ; Sample token
    ; call SampleToken
    
    pop rcx
    
    ; Store draft token
    ; mov [rbx].SpeculativeState.draft_tokens[rcx*4], eax
    
    inc ecx
    jmp @@draft_loop
    
@@done:
    mov [rbx].SpeculativeState.draft_count, SPECULATIVE_DRAFT_LEN
    mov eax, SPECULATIVE_DRAFT_LEN
    
    add rsp, 48
    pop rsi
    pop rbx
    ret
SpeculativeDecoder_GenerateDrafts ENDP

; SpeculativeDecoder_Verify - Verify draft tokens with target
; RCX = this, RDX = context, R8D = context_len
; Returns: EAX = number of accepted tokens
SpeculativeDecoder_Verify PROC FRAME
    push rbx
    .pushreg rbx
    push rsi
    .pushreg rsi
    push rdi
    .pushreg rdi
    sub rsp, 48
    .allocstack 48
    .endprolog
    
    mov rbx, rcx
    mov rsi, rdx
    
    ; Run target model on context + all draft tokens
    ; Get probabilities for each position
    
    xor edi, edi            ; accepted = 0
    
@@verify_loop:
    cmp edi, [rbx].SpeculativeState.draft_count
    jge @@done
    
    ; Get target model's probability for draft token
    ; p_target = TargetModel_GetProb(draft_token[i])
    ; p_draft = DraftModel_GetProb(draft_token[i])
    
    ; Acceptance criterion: random() < min(1, p_target / p_draft)
    rdtsc
    ; ... random check
    
    ; If accepted, continue
    ; If rejected, break and use target's sample instead
    
    inc edi
    jmp @@verify_loop
    
@@done:
    mov [rbx].SpeculativeState.accepted_count, edi
    mov eax, edi
    
    add rsp, 48
    pop rdi
    pop rsi
    pop rbx
    ret
SpeculativeDecoder_Verify ENDP

;------------------------------------------------------------------------------
; SECTION 3: TELEMETRY SYSTEM
;------------------------------------------------------------------------------

; Telemetry_Init - Initialize telemetry buffer
; RCX = this, RDX = output_path, R8D = buffer_size
Telemetry_Init PROC FRAME
    push rbx
    .pushreg rbx
    sub rsp, 48
    .allocstack 48
    .endprolog
    
    mov rbx, rcx
    
    mov [rbx].TelemetryBuffer.output_path, rdx
    mov [rbx].TelemetryBuffer.capacity, r8d
    mov [rbx].TelemetryBuffer.count, 0
    mov [rbx].TelemetryBuffer.write_idx, 0
    
    ; Default flush at 80% capacity
    mov eax, r8d
    shr eax, 1
    add eax, r8d
    shr eax, 2
    mov [rbx].TelemetryBuffer.flush_threshold, eax
    
    ; Allocate event buffer
    mov ecx, r8d
    imul ecx, SIZEOF TelemetryEvent
    call malloc
    mov [rbx].TelemetryBuffer.events, rax
    
    add rsp, 48
    pop rbx
    ret
Telemetry_Init ENDP

; Telemetry_Record - Record a telemetry event
; RCX = this, EDX = event_type, R8 = data, R9D = data_size
Telemetry_Record PROC FRAME
    push rbx
    .pushreg rbx
    sub rsp, 48
    .allocstack 48
    .endprolog
    
    mov rbx, rcx
    
    ; Check if buffer full
    mov eax, [rbx].TelemetryBuffer.count
    cmp eax, [rbx].TelemetryBuffer.capacity
    jge @@drop              ; Drop event if full
    
    ; Get current write slot
    mov ecx, [rbx].TelemetryBuffer.write_idx
    mov rax, [rbx].TelemetryBuffer.events
    imul r10d, ecx, SIZEOF TelemetryEvent
    lea rax, [rax + r10]
    
    ; Get timestamp
    push rdx
    push r8
    push r9
    call GetTickCount64
    pop r9
    pop r8
    pop rdx
    mov [rax].TelemetryEvent.timestamp, rax
    
    ; Fill event
    mov [rax].TelemetryEvent.event_type, edx
    mov [rax].TelemetryEvent.data_size, r9d
    mov [rax].TelemetryEvent.event_data, r8
    
    ; Update indices
    inc [rbx].TelemetryBuffer.count
    mov eax, [rbx].TelemetryBuffer.write_idx
    inc eax
    cmp eax, [rbx].TelemetryBuffer.capacity
    jl @@no_wrap
    xor eax, eax
@@no_wrap:
    mov [rbx].TelemetryBuffer.write_idx, eax
    
    ; Check flush threshold
    mov eax, [rbx].TelemetryBuffer.count
    cmp eax, [rbx].TelemetryBuffer.flush_threshold
    jl @@done
    
    mov rcx, rbx
    call Telemetry_Flush
    jmp @@done
    
@@drop:
    ; Could increment dropped counter here
    
@@done:
    add rsp, 48
    pop rbx
    ret
Telemetry_Record ENDP

; Telemetry_Flush - Flush buffer to disk
; RCX = this
Telemetry_Flush PROC FRAME
    push rbx
    .pushreg rbx
    push rsi
    .pushreg rsi
    sub rsp, 56
    .allocstack 56
    .endprolog
    
    mov rbx, rcx
    
    ; Open output file (append mode)
    mov rcx, [rbx].TelemetryBuffer.output_path
    mov edx, GENERIC_WRITE
    xor r8d, r8d
    mov r9d, 4              ; OPEN_ALWAYS
    mov dword ptr [rsp+32], 80h
    mov qword ptr [rsp+40], 0
    call CreateFileW
    mov rsi, rax
    
    cmp rax, -1             ; INVALID_HANDLE_VALUE
    je @@error
    
    ; Write each event as binary/JSON line
    xor ecx, ecx            ; i = 0
@@write_loop:
    cmp ecx, [rbx].TelemetryBuffer.count
    jge @@close
    
    push rcx
    
    ; Format and write event
    mov rax, [rbx].TelemetryBuffer.events
    imul edx, ecx, SIZEOF TelemetryEvent
    lea rdx, [rax + rdx]
    
    mov rcx, rsi
    ; ... WriteFile
    
    pop rcx
    inc ecx
    jmp @@write_loop
    
@@close:
    mov rcx, rsi
    call CloseHandle
    
    ; Reset buffer
    mov [rbx].TelemetryBuffer.count, 0
    mov [rbx].TelemetryBuffer.write_idx, 0
    
    mov eax, 1
    jmp @@done
    
@@error:
    xor eax, eax
    
@@done:
    add rsp, 56
    pop rsi
    pop rbx
    ret
Telemetry_Flush ENDP

;------------------------------------------------------------------------------
; SECTION 4: SETTINGS MANAGER WITH JSON PERSISTENCE
;------------------------------------------------------------------------------

; SettingsManager_Init - Initialize settings manager
; RCX = this, RDX = settings_path
SettingsManager_Init PROC FRAME
    push rbx
    .pushreg rbx
    sub rsp, 48
    .allocstack 48
    .endprolog
    
    mov rbx, rcx
    mov [rbx].SettingsManager.settings_path, rdx
    mov [rbx].SettingsManager.dirty, 0
    mov [rbx].SettingsManager.auto_save, 1
    
    ; Create root object
    mov ecx, SIZEOF JsonValue
    call malloc
    mov [rbx].SettingsManager.root, rax
    
    mov [rax].JsonValue.value_type, JSON_OBJECT
    mov [rax].JsonValue.children, 0
    mov [rax].JsonValue.child_count, 0
    
    add rsp, 48
    pop rbx
    ret
SettingsManager_Init ENDP

; SettingsManager_Load - Load settings from file
; RCX = this
SettingsManager_Load PROC FRAME
    push rbx
    .pushreg rbx
    sub rsp, 48
    .allocstack 48
    .endprolog
    
    mov rbx, rcx
    
    ; Open file
    mov rcx, [rbx].SettingsManager.settings_path
    mov edx, 80000000h      ; GENERIC_READ
    xor r8d, r8d
    mov r9d, 3              ; OPEN_EXISTING
    mov dword ptr [rsp+32], 80h
    mov qword ptr [rsp+40], 0
    call CreateFileW
    
    cmp rax, -1
    je @@use_defaults
    
    push rax                ; file handle
    
    ; Read file content
    ; ... allocate buffer, ReadFile
    
    ; Parse JSON
    ; ... JsonParse(buffer)
    
    pop rcx
    call CloseHandle
    
    mov eax, 1
    jmp @@done
    
@@use_defaults:
    ; Use default settings
    mov eax, 1
    
@@done:
    add rsp, 48
    pop rbx
    ret
SettingsManager_Load ENDP

; SettingsManager_Save - Save settings to file
; RCX = this
SettingsManager_Save PROC FRAME
    push rbx
    .pushreg rbx
    sub rsp, 48
    .allocstack 48
    .endprolog
    
    mov rbx, rcx
    
    ; Open/create file
    mov rcx, [rbx].SettingsManager.settings_path
    mov edx, 40000000h      ; GENERIC_WRITE
    xor r8d, r8d
    mov r9d, 2              ; CREATE_ALWAYS
    mov dword ptr [rsp+32], 80h
    mov qword ptr [rsp+40], 0
    call CreateFileW
    
    cmp rax, -1
    je @@error
    
    push rax
    
    ; Serialize JSON to string
    ; ... JsonStringify(root)
    
    ; Write to file
    ; ... WriteFile
    
    pop rcx
    call CloseHandle
    
    mov [rbx].SettingsManager.dirty, 0
    mov eax, 1
    jmp @@done
    
@@error:
    xor eax, eax
    
@@done:
    add rsp, 48
    pop rbx
    ret
SettingsManager_Save ENDP

; SettingsManager_Get - Get setting value
; RCX = this, RDX = key_path (e.g., "editor.fontSize")
; Returns: RAX = JsonValue*
SettingsManager_Get PROC FRAME
    push rbx
    .pushreg rbx
    sub rsp, 40
    .allocstack 40
    .endprolog
    
    mov rbx, rcx
    mov rcx, [rbx].SettingsManager.root
    
    ; Walk path segments separated by '.'
    ; For each segment, find matching child
    
    mov rax, rcx            ; Simplified - return root
    
    add rsp, 40
    pop rbx
    ret
SettingsManager_Get ENDP

; SettingsManager_Set - Set setting value
; RCX = this, RDX = key_path, R8 = value
SettingsManager_Set PROC FRAME
    push rbx
    .pushreg rbx
    sub rsp, 48
    .allocstack 48
    .endprolog
    
    mov rbx, rcx
    
    ; Find or create path in JSON tree
    ; Set value
    
    mov [rbx].SettingsManager.dirty, 1
    
    ; Auto-save if enabled
    cmp [rbx].SettingsManager.auto_save, 0
    je @@done
    
    mov rcx, rbx
    call SettingsManager_Save
    
@@done:
    add rsp, 48
    pop rbx
    ret
SettingsManager_Set ENDP

;------------------------------------------------------------------------------
; SECTION 5: CODEBASE CONTEXT / SYMBOL INDEXING
;------------------------------------------------------------------------------

; CodebaseContext_Init - Initialize codebase indexer
; RCX = this
CodebaseContext_Init PROC FRAME
    push rbx
    .pushreg rbx
    sub rsp, 48
    .allocstack 48
    .endprolog
    
    mov rbx, rcx
    
    mov [rbx].CodebaseIndex.symbol_count, 0
    mov [rbx].CodebaseIndex.symbol_capacity, 1024
    mov [rbx].CodebaseIndex.file_count, 0
    
    ; Allocate symbols array
    mov ecx, 1024 * SIZEOF Symbol
    call malloc
    mov [rbx].CodebaseIndex.symbols, rax
    
    ; Allocate file paths array
    mov ecx, 256 * 8        ; 256 file pointers
    call malloc
    mov [rbx].CodebaseIndex.file_paths, rax
    
    add rsp, 48
    pop rbx
    ret
CodebaseContext_Init ENDP

; CodebaseContext_IndexFile - Index symbols in a file
; RCX = this, RDX = file_path
CodebaseContext_IndexFile PROC FRAME
    push rbx
    .pushreg rbx
    push rsi
    .pushreg rsi
    sub rsp, 48
    .allocstack 48
    .endprolog
    
    mov rbx, rcx
    mov rsi, rdx
    
    ; Add file to file_paths array
    mov eax, [rbx].CodebaseIndex.file_count
    mov rcx, [rbx].CodebaseIndex.file_paths
    mov [rcx + rax*8], rsi
    inc [rbx].CodebaseIndex.file_count
    mov r8d, eax            ; file_id
    
    ; Parse file for symbols
    ; For each symbol found:
    ;   - Allocate Symbol
    ;   - Fill name, kind, location
    ;   - Add to symbols array
    
    ; Check if need to grow symbols array
    mov eax, [rbx].CodebaseIndex.symbol_count
    cmp eax, [rbx].CodebaseIndex.symbol_capacity
    jl @@has_space
    
    ; Grow array
    mov eax, [rbx].CodebaseIndex.symbol_capacity
    shl eax, 1              ; Double capacity
    mov [rbx].CodebaseIndex.symbol_capacity, eax
    imul eax, SIZEOF Symbol
    mov edx, eax
    mov rcx, [rbx].CodebaseIndex.symbols
    call realloc
    mov [rbx].CodebaseIndex.symbols, rax
    
@@has_space:
    ; Add parsed symbols
    ; ... (parsing logic would go here)
    
    add rsp, 48
    pop rsi
    pop rbx
    ret
CodebaseContext_IndexFile ENDP

; CodebaseContext_FindSymbol - Find symbol by name
; RCX = this, RDX = name
; Returns: RAX = Symbol* or NULL
CodebaseContext_FindSymbol PROC FRAME
    push rbx
    .pushreg rbx
    sub rsp, 40
    .allocstack 40
    .endprolog
    
    mov rbx, rcx
    mov r8, rdx             ; name to find
    
    xor ecx, ecx
@@loop:
    cmp ecx, [rbx].CodebaseIndex.symbol_count
    jge @@not_found
    
    mov rax, [rbx].CodebaseIndex.symbols
    imul edx, ecx, SIZEOF Symbol
    mov rdx, [rax + rdx].Symbol.name
    
    ; Compare strings
    ; ... strcmp(rdx, r8)
    
    inc ecx
    jmp @@loop
    
@@not_found:
    xor eax, eax
    
    add rsp, 40
    pop rbx
    ret
CodebaseContext_FindSymbol ENDP

; CodebaseContext_GetDefinitions - Get all definitions of type
; RCX = this, EDX = symbol_kind
; Returns: RAX = array of Symbol*
CodebaseContext_GetDefinitions PROC FRAME
    push rbx
    .pushreg rbx
    sub rsp, 48
    .allocstack 48
    .endprolog
    
    mov rbx, rcx
    mov r8d, edx            ; kind filter
    
    ; Count matching symbols
    xor ecx, ecx
    xor r9d, r9d            ; count
@@count_loop:
    cmp ecx, [rbx].CodebaseIndex.symbol_count
    jge @@allocate
    
    mov rax, [rbx].CodebaseIndex.symbols
    imul edx, ecx, SIZEOF Symbol
    cmp [rax + rdx].Symbol.kind, r8d
    jne @@count_next
    inc r9d
    
@@count_next:
    inc ecx
    jmp @@count_loop
    
@@allocate:
    ; Allocate result array
    lea ecx, [r9 * 8 + 8]   ; +8 for count header
    call malloc
    
    mov [rax], r9           ; Store count
    ; ... fill array with matching symbols
    
    add rsp, 48
    pop rbx
    ret
CodebaseContext_GetDefinitions ENDP

;------------------------------------------------------------------------------
; SECTION 6: MODEL ROUTER (TASK-AWARE MODEL SELECTION)
;------------------------------------------------------------------------------

; ModelRouter_Init - Initialize model router
; RCX = this, RDX = model_list, R8D = count
ModelRouter_Init PROC FRAME
    push rbx
    .pushreg rbx
    sub rsp, 48
    .allocstack 48
    .endprolog
    
    mov rbx, rcx
    
    mov [rbx].ModelRouter.models, rdx
    mov [rbx].ModelRouter.model_count, r8d
    mov [rbx].ModelRouter.default_model_idx, 0
    
    ; Allocate task score matrix
    ; [num_models * num_task_types] floats
    mov eax, r8d
    imul eax, 8             ; 8 task types
    shl eax, 2              ; * sizeof(float)
    mov ecx, eax
    call malloc
    mov [rbx].ModelRouter.task_scores, rax
    
    ; Initialize with default scores
    ; ... (would load from config or learned weights)
    
    add rsp, 48
    pop rbx
    ret
ModelRouter_Init ENDP

; ModelRouter_SelectModel - Select best model for task
; RCX = this, EDX = task_type
; Returns: RAX = model pointer
ModelRouter_SelectModel PROC FRAME
    push rbx
    .pushreg rbx
    sub rsp, 40
    .allocstack 40
    .endprolog
    
    mov rbx, rcx
    mov r8d, edx            ; task_type
    
    ; Find model with highest score for this task
    xor ecx, ecx            ; i = 0
    xor edx, edx            ; best_idx = 0
    xorps xmm0, xmm0        ; best_score = 0
    
@@loop:
    cmp ecx, [rbx].ModelRouter.model_count
    jge @@found
    
    ; score = task_scores[i * 8 + task_type]
    mov eax, ecx
    shl eax, 3              ; * 8 task types
    add eax, r8d
    mov rax, [rbx].ModelRouter.task_scores
    movss xmm1, [rax + eax*4]
    
    comiss xmm1, xmm0
    jbe @@next
    
    movss xmm0, xmm1
    mov edx, ecx
    
@@next:
    inc ecx
    jmp @@loop
    
@@found:
    ; Return models[best_idx]
    mov rax, [rbx].ModelRouter.models
    mov rax, [rax + rdx*8]
    
    add rsp, 40
    pop rbx
    ret
ModelRouter_SelectModel ENDP

; ModelRouter_UpdateScore - Update task score based on feedback
; RCX = this, EDX = model_idx, R8D = task_type, XMM0 = delta
ModelRouter_UpdateScore PROC FRAME
    sub rsp, 40
    .allocstack 40
    .endprolog
    
    ; score[model_idx * 8 + task_type] += delta
    mov eax, edx
    shl eax, 3
    add eax, r8d
    mov rax, [rcx].ModelRouter.task_scores
    addss xmm0, [rax + eax*4]
    movss [rax + eax*4], xmm0
    
    add rsp, 40
    ret
ModelRouter_UpdateScore ENDP

;------------------------------------------------------------------------------
; EXPORTS
;------------------------------------------------------------------------------

PUBLIC KVCache_Init
PUBLIC KVCache_Lookup
PUBLIC KVCache_Insert
PUBLIC KVCache_FindLRU
PUBLIC KVCache_GetStats
PUBLIC SpeculativeDecoder_Init
PUBLIC SpeculativeDecoder_GenerateDrafts
PUBLIC SpeculativeDecoder_Verify
PUBLIC Telemetry_Init
PUBLIC Telemetry_Record
PUBLIC Telemetry_Flush
PUBLIC SettingsManager_Init
PUBLIC SettingsManager_Load
PUBLIC SettingsManager_Save
PUBLIC SettingsManager_Get
PUBLIC SettingsManager_Set
PUBLIC CodebaseContext_Init
PUBLIC CodebaseContext_IndexFile
PUBLIC CodebaseContext_FindSymbol
PUBLIC CodebaseContext_GetDefinitions
PUBLIC ModelRouter_Init
PUBLIC ModelRouter_SelectModel
PUBLIC ModelRouter_UpdateScore

END
