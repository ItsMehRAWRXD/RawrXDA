; byte_level_hotpatcher_masm.asm
; Pure MASM x64 - Byte Level Hotpatcher (converted from C++ ByteLevelHotpatcher class)
; Precision GGUF binary file manipulation with pattern matching

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

; Hotpatcher constants
MAX_PATCHES EQU 100
MAX_PATTERNS EQU 50
MAX_SEARCH_RESULTS EQU 100
BUFFER_SIZE EQU 1048576              ; 1 MB

; ============================================================================
; DATA STRUCTURES
; ============================================================================

; PATCH_RESULT - Patch operation result
PATCH_RESULT STRUCT
    success BYTE ?                  ; True if successful
    detail QWORD ?                  ; Result detail
    errorCode DWORD ?               ; Error code
    elapsedMs QWORD ?               ; Execution time
ENDS

; BYTE_PATCH - Byte patch definition
BYTE_PATCH STRUCT
    name QWORD ?                    ; Patch name
    description QWORD ?             ; Patch description
    type DWORD ?                    ; Patch type enum
    offset QWORD ?                  ; File offset
    size QWORD ?                    ; Patch size
    originalData QWORD ?            ; Original data buffer
    patchData QWORD ?               ; Patch data buffer
    enabled BYTE ?                  ; Whether patch is enabled
    applied BYTE ?                  ; Whether patch is applied
    timesApplied DWORD ?            ; Number of times applied
ENDS

; PATTERN_MATCH - Pattern search result
PATTERN_MATCH STRUCT
    pattern QWORD ?                 ; Pattern that was matched
    offset QWORD ?                  ; File offset of match
    length QWORD ?                  ; Length of match
    confidence DWORD ?              ; Match confidence (0-100)
ENDS

; BYTE_LEVEL_HOTPATCHER - Hotpatcher state
BYTE_LEVEL_HOTPATCHER STRUCT
    patches QWORD ?                 ; Array of BYTE_PATCH
    patchCount DWORD ?              ; Current patch count
    maxPatches DWORD ?              ; Capacity
    
    patterns QWORD ?                ; Array of patterns
    patternCount DWORD ?            ; Current pattern count
    maxPatterns DWORD ?             ; Capacity
    
    modelData QWORD ?               ; Model data buffer
    modelSize QWORD ?               ; Model size
    
    ; Statistics
    totalPatchesApplied QWORD ?
    totalBytesPatched QWORD ?
    totalPatternMatches QWORD ?
    totalErrors DWORD ?
    
    ; Callbacks
    patchAppliedCallback QWORD ?    ; Called when patch applied
    patternFoundCallback QWORD ?    ; Called when pattern found
    errorOccurredCallback QWORD ?   ; Called on error
    
    initialized BYTE ?
ENDS

; ============================================================================
; GLOBAL DATA
; ============================================================================

.data
    szHotpatcherCreated DB "[BYTE_HOTPATCHER] Created for model: %lld bytes", 0
    szPatchApplied DB "[BYTE_HOTPATCHER] Patch applied: %s (offset=%lld, size=%lld)", 0
    szPatchFailed DB "[BYTE_HOTPATCHER] Patch failed: %s (error=%d)", 0
    szPatternFound DB "[BYTE_HOTPATCHER] Pattern found: %s (offset=%lld, confidence=%d)", 0
    szSearchComplete DB "[BYTE_HOTPATCHER] Search complete: %d matches", 0

; Patch types
PATCH_TYPE_REPLACE EQU 0
PATCH_TYPE_BITFLIP EQU 1
PATCH_TYPE_XOR EQU 2
PATCH_TYPE_ROTATE EQU 3
PATCH_TYPE_REVERSE EQU 4
PATCH_TYPE_SWAP EQU 5
PATCH_TYPE_INSERT EQU 6
PATCH_TYPE_DELETE EQU 7

.code

; ============================================================================
; PUBLIC API
; ============================================================================

; byte_level_hotpatcher_create(RCX = modelData, RDX = modelSize)
; Create byte level hotpatcher
; Returns: RAX = pointer to BYTE_LEVEL_HOTPATCHER
PUBLIC byte_level_hotpatcher_create
byte_level_hotpatcher_create PROC
    push rbx
    
    mov rbx, rcx                    ; rbx = modelData
    mov r9, rdx                     ; r9 = modelSize
    
    ; Allocate hotpatcher
    mov rcx, SIZEOF BYTE_LEVEL_HOTPATCHER
    call malloc
    mov r10, rax
    
    ; Allocate patches array
    mov rcx, MAX_PATCHES
    imul rcx, SIZEOF BYTE_PATCH
    call malloc
    mov [r10 + BYTE_LEVEL_HOTPATCHER.patches], rax
    
    ; Allocate patterns array
    mov rcx, MAX_PATTERNS
    imul rcx, 256                   ; Max pattern size
    call malloc
    mov [r10 + BYTE_LEVEL_HOTPATCHER.patterns], rax
    
    ; Initialize
    mov [r10 + BYTE_LEVEL_HOTPATCHER.modelData], rbx
    mov [r10 + BYTE_LEVEL_HOTPATCHER.modelSize], r9
    mov [r10 + BYTE_LEVEL_HOTPATCHER.patchCount], 0
    mov [r10 + BYTE_LEVEL_HOTPATCHER.maxPatches], MAX_PATCHES
    mov [r10 + BYTE_LEVEL_HOTPATCHER.patternCount], 0
    mov [r10 + BYTE_LEVEL_HOTPATCHER.maxPatterns], MAX_PATTERNS
    mov [r10 + BYTE_LEVEL_HOTPATCHER.totalPatchesApplied], 0
    mov [r10 + BYTE_LEVEL_HOTPATCHER.totalBytesPatched], 0
    mov [r10 + BYTE_LEVEL_HOTPATCHER.totalPatternMatches], 0
    mov [r10 + BYTE_LEVEL_HOTPATCHER.totalErrors], 0
    
    mov byte [r10 + BYTE_LEVEL_HOTPATCHER.initialized], 1
    
    ; Log
    lea rcx, [szHotpatcherCreated]
    mov rdx, r9
    call console_log
    
    mov rax, r10
    pop rbx
    ret
byte_level_hotpatcher_create ENDP

; ============================================================================

; byte_hotpatcher_apply_patch(RCX = hotpatcher, RDX = patch)
; Apply byte patch
; Returns: RAX = pointer to PATCH_RESULT
PUBLIC byte_hotpatcher_apply_patch
byte_hotpatcher_apply_patch PROC
    push rbx
    push rsi
    
    mov rbx, rcx                    ; rbx = hotpatcher
    mov rsi, rdx                    ; rsi = patch
    
    ; Check if patch enabled
    cmp byte [rsi + BYTE_PATCH.enabled], 1
    jne .patch_disabled
    
    ; Check bounds
    mov r8, [rsi + BYTE_PATCH.offset]
    mov r9, [rsi + BYTE_PATCH.size]
    mov r10, [rbx + BYTE_LEVEL_HOTPATCHER.modelSize]
    
    add r8, r9
    cmp r8, r10
    jg .out_of_bounds
    
    ; Get start time
    call GetSystemTimeAsFileTime
    mov r8, rax                     ; r8 = start time
    
    ; Allocate result
    mov rcx, SIZEOF PATCH_RESULT
    call malloc
    mov r9, rax                     ; r9 = result
    
    ; Apply patch based on type
    mov eax, [rsi + BYTE_PATCH.type]
    
    cmp eax, PATCH_TYPE_REPLACE
    je .apply_replace
    cmp eax, PATCH_TYPE_BITFLIP
    je .apply_bitflip
    cmp eax, PATCH_TYPE_XOR
    je .apply_xor
    cmp eax, PATCH_TYPE_ROTATE
    je .apply_rotate
    cmp eax, PATCH_TYPE_REVERSE
    je .apply_reverse
    cmp eax, PATCH_TYPE_SWAP
    je .apply_swap
    cmp eax, PATCH_TYPE_INSERT
    je .apply_insert
    cmp eax, PATCH_TYPE_DELETE
    je .apply_delete
    
    jmp .unknown_type
    
.apply_replace:
    ; Simple replacement
    mov rcx, [rbx + BYTE_LEVEL_HOTPATCHER.modelData]
    add rcx, [rsi + BYTE_PATCH.offset]
    mov rdx, [rsi + BYTE_PATCH.patchData]
    mov r8, [rsi + BYTE_PATCH.size]
    call memcpy
    jmp .patch_applied
    
.apply_bitflip:
    ; Bitflip operation
    mov rcx, [rbx + BYTE_LEVEL_HOTPATCHER.modelData]
    add rcx, [rsi + BYTE_PATCH.offset]
    mov rdx, [rsi + BYTE_PATCH.size]
    call apply_bitflip_operation
    jmp .patch_applied
    
.apply_xor:
    ; XOR operation
    mov rcx, [rbx + BYTE_LEVEL_HOTPATCHER.modelData]
    add rcx, [rsi + BYTE_PATCH.offset]
    mov rdx, [rsi + BYTE_PATCH.size]
    mov r8, [rsi + BYTE_PATCH.patchData]
    call apply_xor_operation
    jmp .patch_applied
    
.apply_rotate:
    ; Rotate operation
    mov rcx, [rbx + BYTE_LEVEL_HOTPATCHER.modelData]
    add rcx, [rsi + BYTE_PATCH.offset]
    mov rdx, [rsi + BYTE_PATCH.size]
    mov r8d, 1                      ; Rotate right by 1
    call apply_rotate_operation
    jmp .patch_applied
    
.apply_reverse:
    ; Reverse operation
    mov rcx, [rbx + BYTE_LEVEL_HOTPATCHER.modelData]
    add rcx, [rsi + BYTE_PATCH.offset]
    mov rdx, [rsi + BYTE_PATCH.size]
    call apply_reverse_operation
    jmp .patch_applied
    
.apply_swap:
    ; Swap operation
    mov rcx, [rbx + BYTE_LEVEL_HOTPATCHER.modelData]
    add rcx, [rsi + BYTE_PATCH.offset]
    mov rdx, [rsi + BYTE_PATCH.size]
    call apply_swap_operation
    jmp .patch_applied
    
.apply_insert:
    ; Insert operation (requires buffer expansion)
    mov rcx, rbx
    mov rdx, rsi
    call apply_insert_operation
    jmp .patch_applied
    
.apply_delete:
    ; Delete operation (requires buffer compaction)
    mov rcx, rbx
    mov rdx, rsi
    call apply_delete_operation
    jmp .patch_applied
    
.patch_applied:
    ; Get end time
    call GetSystemTimeAsFileTime
    sub rax, r8                     ; rax = elapsed time
    mov [r9 + PATCH_RESULT.elapsedMs], rax
    
    ; Set result
    mov byte [r9 + PATCH_RESULT.success], 1
    lea rax, [szPatchAppliedDetail]
    mov [r9 + PATCH_RESULT.detail], rax
    mov [r9 + PATCH_RESULT.errorCode], 0
    
    ; Update patch status
    mov byte [rsi + BYTE_PATCH.applied], 1
    inc dword [rsi + BYTE_PATCH.timesApplied]
    
    ; Update statistics
    inc qword [rbx + BYTE_LEVEL_HOTPATCHER.totalPatchesApplied]
    mov rax, [rsi + BYTE_PATCH.size]
    add [rbx + BYTE_LEVEL_HOTPATCHER.totalBytesPatched], rax
    
    ; Log success
    lea rcx, [szPatchApplied]
    mov rdx, [rsi + BYTE_PATCH.name]
    mov r8, [rsi + BYTE_PATCH.offset]
    mov r9, [rsi + BYTE_PATCH.size]
    call console_log
    
    mov rax, r9                     ; Return result
    pop rsi
    pop rbx
    ret
    
.out_of_bounds:
.unknown_type:
.patch_disabled:
    ; Set error result
    mov byte [r9 + PATCH_RESULT.success], 0
    lea rax, [szPatchFailedDetail]
    mov [r9 + PATCH_RESULT.detail], rax
    mov [r9 + PATCH_RESULT.errorCode], 1
    
    ; Log failure
    lea rcx, [szPatchFailed]
    mov rdx, [rsi + BYTE_PATCH.name]
    mov r8d, 1
    call console_log
    
    inc dword [rbx + BYTE_LEVEL_HOTPATCHER.totalErrors]
    
    mov rax, r9
    pop rsi
    pop rbx
    ret
byte_hotpatcher_apply_patch ENDP

; ============================================================================

; byte_hotpatcher_add_patch(RCX = hotpatcher, RDX = name, R8 = description, R9d = type)
; Add byte patch
; Returns: RAX = patch ID (0 on error)
PUBLIC byte_hotpatcher_add_patch
byte_hotpatcher_add_patch PROC
    push rbx
    push rsi
    
    mov rbx, rcx                    ; rbx = hotpatcher
    mov rsi, rdx                    ; rsi = name
    mov r10, r8                     ; r10 = description
    mov r11d, r9d                   ; r11d = type
    
    ; Check capacity
    mov r12d, [rbx + BYTE_LEVEL_HOTPATCHER.patchCount]
    cmp r12d, [rbx + BYTE_LEVEL_HOTPATCHER.maxPatches]
    jge .capacity_exceeded
    
    ; Get patch slot
    mov r13, [rbx + BYTE_LEVEL_HOTPATCHER.patches]
    mov r14, r12
    imul r14, SIZEOF BYTE_PATCH
    add r13, r14
    
    ; Store patch name
    mov rcx, rsi
    call strlen
    inc rax
    call malloc
    mov [r13 + BYTE_PATCH.name], rax
    
    mov rcx, rsi
    mov rdx, rax
    call strcpy
    
    ; Store patch description
    mov rcx, r10
    call strlen
    inc rax
    call malloc
    mov [r13 + BYTE_PATCH.description], rax
    
    mov rcx, r10
    mov rdx, rax
    call strcpy
    
    ; Set patch properties
    mov [r13 + BYTE_PATCH.type], r11d
    mov byte [r13 + BYTE_PATCH.enabled], 1
    mov byte [r13 + BYTE_PATCH.applied], 0
    mov [r13 + BYTE_PATCH.timesApplied], 0
    
    ; Set default offset and size
    mov [r13 + BYTE_PATCH.offset], 0
    mov [r13 + BYTE_PATCH.size], 1024
    
    ; Allocate patch data buffer
    mov rcx, 1024
    call malloc
    mov [r13 + BYTE_PATCH.patchData], rax
    
    ; Allocate original data buffer
    mov rcx, 1024
    call malloc
    mov [r13 + BYTE_PATCH.originalData], rax
    
    ; Increment patch count
    inc dword [rbx + BYTE_LEVEL_HOTPATCHER.patchCount]
    
    mov eax, r12d                   ; Return patch ID
    pop rsi
    pop rbx
    ret
    
.capacity_exceeded:
    xor rax, rax
    pop rsi
    pop rbx
    ret
byte_hotpatcher_add_patch ENDP

; ============================================================================

; byte_hotpatcher_add_pattern(RCX = hotpatcher, RDX = pattern, R8 = patternSize)
; Add search pattern
; Returns: RAX = pattern ID (0 on error)
PUBLIC byte_hotpatcher_add_pattern
byte_hotpatcher_add_pattern PROC
    push rbx
    push rsi
    
    mov rbx, rcx                    ; rbx = hotpatcher
    mov rsi, rdx                    ; rsi = pattern
    mov r9, r8                      ; r9 = patternSize
    
    ; Check capacity
    mov r10d, [rbx + BYTE_LEVEL_HOTPATCHER.patternCount]
    cmp r10d, [rbx + BYTE_LEVEL_HOTPATCHER.maxPatterns]
    jge .capacity_exceeded
    
    ; Get pattern slot
    mov r11, [rbx + BYTE_LEVEL_HOTPATCHER.patterns]
    mov r12, r10
    imul r12, 256                   ; Each pattern max 256 bytes
    add r11, r12
    
    ; Copy pattern
    mov rcx, r11
    mov rdx, rsi
    mov r8, r9
    call memcpy
    
    ; Increment pattern count
    inc dword [rbx + BYTE_LEVEL_HOTPATCHER.patternCount]
    
    mov eax, r10d                   ; Return pattern ID
    pop rsi
    pop rbx
    ret
    
.capacity_exceeded:
    xor rax, rax
    pop rsi
    pop rbx
    ret
byte_hotpatcher_add_pattern ENDP

; ============================================================================

; byte_hotpatcher_search_patterns(RCX = hotpatcher, RDX = resultsBuffer)
; Search for all patterns
; Returns: RAX = number of matches found
PUBLIC byte_hotpatcher_search_patterns
byte_hotpatcher_search_patterns PROC
    push rbx
    push rsi
    push rdi
    
    mov rbx, rcx                    ; rbx = hotpatcher
    mov rsi, rdx                    ; rsi = resultsBuffer
    
    xor rdi, rdi                    ; rdi = match count
    
    ; Get model data
    mov r8, [rbx + BYTE_LEVEL_HOTPATCHER.modelData]
    mov r9, [rbx + BYTE_LEVEL_HOTPATCHER.modelSize]
    
    ; Get patterns
    mov r10, [rbx + BYTE_LEVEL_HOTPATCHER.patterns]
    mov r11d, [rbx + BYTE_LEVEL_HOTPATCHER.patternCount]
    xor r12d, r12d                  ; r12d = pattern index
    
.search_loop:
    cmp r12d, r11d
    jge .search_complete
    
    ; Get current pattern
    mov r13, r10
    mov r14, r12
    imul r14, 256
    add r13, r14
    
    ; Search for this pattern
    mov rcx, r8                     ; modelData
    mov rdx, r9                     ; modelSize
    mov r15, r13                    ; pattern
    mov r14, 256                    ; max pattern size
    
    call boyer_moore_search
    
    cmp rax, -1
    je .pattern_not_found
    
    ; Store match result
    mov r15, rsi
    mov r14, rdi
    imul r14, SIZEOF PATTERN_MATCH
    add r15, r14
    
    mov [r15 + PATTERN_MATCH.pattern], r13
    mov [r15 + PATTERN_MATCH.offset], rax
    mov [r15 + PATTERN_MATCH.length], 256
    mov [r15 + PATTERN_MATCH.confidence], 100
    
    inc rdi
    
    ; Log match
    lea rcx, [szPatternFound]
    mov rdx, r13
    mov r8, rax
    mov r9d, 100
    call console_log
    
.pattern_not_found:
    inc r12d
    jmp .search_loop
    
.search_complete:
    ; Update statistics
    add [rbx + BYTE_LEVEL_HOTPATCHER.totalPatternMatches], rdi
    
    ; Log completion
    lea rcx, [szSearchComplete]
    mov rdx, rdi
    call console_log
    
    mov rax, rdi                    ; Return match count
    pop rdi
    pop rsi
    pop rbx
    ret
byte_hotpatcher_search_patterns ENDP

; ============================================================================

; byte_hotpatcher_get_patch(RCX = hotpatcher, RDX = patchId)
; Get patch by ID
; Returns: RAX = pointer to BYTE_PATCH
PUBLIC byte_hotpatcher_get_patch
byte_hotpatcher_get_patch PROC
    mov r8, [rcx + BYTE_LEVEL_HOTPATCHER.patches]
    mov r9d, [rcx + BYTE_LEVEL_HOTPATCHER.patchCount]
    xor r10d, r10d
    
.find_patch:
    cmp r10d, r9d
    jge .patch_not_found
    
    mov r11, r8
    mov r12, r10
    imul r12, SIZEOF BYTE_PATCH
    add r11, r12
    
    cmp r10d, edx
    je .patch_found
    
    inc r10d
    jmp .find_patch
    
.patch_found:
    mov rax, r11
    ret
    
.patch_not_found:
    xor rax, rax
    ret
byte_hotpatcher_get_patch ENDP

; ============================================================================

; byte_hotpatcher_get_statistics(RCX = hotpatcher, RDX = statsBuffer)
; Get hotpatcher statistics
PUBLIC byte_hotpatcher_get_statistics
byte_hotpatcher_get_statistics PROC
    mov [rdx + 0], qword [rcx + BYTE_LEVEL_HOTPATCHER.totalPatchesApplied]
    mov [rdx + 8], qword [rcx + BYTE_LEVEL_HOTPATCHER.totalBytesPatched]
    mov [rdx + 16], qword [rcx + BYTE_LEVEL_HOTPATCHER.totalPatternMatches]
    mov [rdx + 24], dword [rcx + BYTE_LEVEL_HOTPATCHER.totalErrors]
    ret
byte_hotpatcher_get_statistics ENDP

; ============================================================================

; byte_hotpatcher_destroy(RCX = hotpatcher)
; Free byte level hotpatcher
PUBLIC byte_hotpatcher_destroy
byte_hotpatcher_destroy PROC
    push rbx
    
    mov rbx, rcx
    
    ; Free patches array
    mov r10, [rbx + BYTE_LEVEL_HOTPATCHER.patches]
    mov r11d, [rbx + BYTE_LEVEL_HOTPATCHER.patchCount]
    xor r12d, r12d
    
.free_patches:
    cmp r12d, r11d
    jge .patches_freed
    
    mov r13, r10
    mov r14, r12
    imul r14, SIZEOF BYTE_PATCH
    add r13, r14
    
    mov rcx, [r13 + BYTE_PATCH.name]
    cmp rcx, 0
    je .skip_patch_name
    call free
    
.skip_patch_name:
    mov rcx, [r13 + BYTE_PATCH.description]
    cmp rcx, 0
    je .skip_patch_desc
    call free
    
.skip_patch_desc:
    mov rcx, [r13 + BYTE_PATCH.originalData]
    cmp rcx, 0
    je .skip_original
    call free
    
.skip_original:
    mov rcx, [r13 + BYTE_PATCH.patchData]
    cmp rcx, 0
    je .skip_patch
    call free
    
.skip_patch:
    inc r12d
    jmp .free_patches
    
.patches_freed:
    mov rcx, [rbx + BYTE_LEVEL_HOTPATCHER.patches]
    cmp rcx, 0
    je .skip_patches_array
    call free
    
.skip_patches_array:
    ; Free patterns array
    mov rcx, [rbx + BYTE_LEVEL_HOTPATCHER.patterns]
    cmp rcx, 0
    je .skip_patterns
    call free
    
.skip_patterns:
    ; Free hotpatcher
    mov rcx, rbx
    call free
    
    pop rbx
    ret
byte_hotpatcher_destroy ENDP

; ============================================================================
; HELPER FUNCTIONS
; ============================================================================

; Boyer-Moore search algorithm
boyer_moore_search PROC
    ; RCX = haystack, RDX = haystackSize, R8 = needle, R9 = needleSize
    push rbx
    push rsi
    push rdi
    
    mov rsi, rcx                    ; rsi = haystack
    mov rdi, r8                     ; rdi = needle
    mov r10, rdx                    ; r10 = haystackSize
    mov r11, r9                     ; r11 = needleSize
    
    ; Simple linear search (simplified version)
    xor rbx, rbx                    ; rbx = haystack index
    
.search_loop:
    mov rax, r10
    sub rax, r11
    cmp rbx, rax
    jg .not_found
    
    mov rcx, rsi
    add rcx, rbx
    mov rdx, rdi
    mov r8, r11
    call memcmp
    
    test rax, rax
    jz .found
    
    inc rbx
    jmp .search_loop
    
.found:
    mov rax, rbx
    pop rdi
    pop rsi
    pop rbx
    ret
    
.not_found:
    mov rax, -1
    pop rdi
    pop rsi
    pop rbx
    ret
boyer_moore_search ENDP

; Bitflip operation
apply_bitflip_operation PROC
    ; RCX = data, RDX = size
    push rbx
    
    mov rbx, rcx
    xor r8, r8
    
.bitflip_loop:
    cmp r8, rdx
    jge .bitflip_done
    
    mov al, byte [rbx + r8]
    not al
    mov byte [rbx + r8], al
    
    inc r8
    jmp .bitflip_loop
    
.bitflip_done:
    pop rbx
    ret
apply_bitflip_operation ENDP

; XOR operation
apply_xor_operation PROC
    ; RCX = data, RDX = size, R8 = key
    push rbx
    
    mov rbx, rcx
    xor r9, r9
    
.xor_loop:
    cmp r9, rdx
    jge .xor_done
    
    mov al, byte [rbx + r9]
    xor al, byte [r8 + r9]
    mov byte [rbx + r9], al
    
    inc r9
    jmp .xor_loop
    
.xor_done:
    pop rbx
    ret
apply_xor_operation ENDP

; Rotate operation
apply_rotate_operation PROC
    ; RCX = data, RDX = size, R8d = rotate amount
    push rbx
    
    mov rbx, rcx
    xor r9, r9
    
.rotate_loop:
    cmp r9, rdx
    jge .rotate_done
    
    mov al, byte [rbx + r9]
    ror al, r8b
    mov byte [rbx + r9], al
    
    inc r9
    jmp .rotate_loop
    
.rotate_done:
    pop rbx
    ret
apply_rotate_operation ENDP

; Reverse operation
apply_reverse_operation PROC
    ; RCX = data, RDX = size
    push rbx
    push rsi
    
    mov rbx, rcx
    mov rsi, rdx
    dec rsi
    xor r8, r8
    
.reverse_loop:
    cmp r8, rsi
    jge .reverse_done
    
    mov al, byte [rbx + r8]
    mov ah, byte [rbx + rsi]
    mov byte [rbx + r8], ah
    mov byte [rbx + rsi], al
    
    inc r8
    dec rsi
    cmp r8, rsi
    jl .reverse_loop
    
.reverse_done:
    pop rsi
    pop rbx
    ret
apply_reverse_operation ENDP

; Swap operation
apply_swap_operation PROC
    ; RCX = data, RDX = size
    push rbx
    
    mov rbx, rcx
    xor r8, r8
    
.swap_loop:
    cmp r8, rdx
    jge .swap_done
    
    mov al, byte [rbx + r8]
    mov ah, byte [rbx + r8 + 1]
    mov byte [rbx + r8], ah
    mov byte [rbx + r8 + 1], al
    
    add r8, 2
    jmp .swap_loop
    
.swap_done:
    pop rbx
    ret
apply_swap_operation ENDP

; Insert operation (simplified)
apply_insert_operation PROC
    ; RCX = hotpatcher, RDX = patch
    ; Not implemented - requires buffer reallocation
    ret
apply_insert_operation ENDP

; Delete operation (simplified)
apply_delete_operation PROC
    ; RCX = hotpatcher, RDX = patch
    ; Not implemented - requires buffer compaction
    ret
apply_delete_operation ENDP

; ============================================================================

.data
    szPatchAppliedDetail DB "Patch applied successfully", 0
    szPatchFailedDetail DB "Patch application failed", 0

END
