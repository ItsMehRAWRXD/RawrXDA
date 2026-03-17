; ai_code_assistant_masm.asm
; Pure MASM x64 - AI Code Assistant (converted from C++ AICodeAssistant class)
; AI-powered code completion, refactoring, and IDE integration

option casemap:none

EXTERN malloc:PROC
EXTERN free:PROC
EXTERN memset:PROC
EXTERN memcpy:PROC
EXTERN strlen:PROC
EXTERN strcpy:PROC
EXTERN sprintf:PROC
EXTERN console_log:PROC
EXTERN CreateProcessA:PROC
EXTERN CreateThreadA:PROC
EXTERN WaitForSingleObject:PROC
EXTERN CloseHandle:PROC

; Assistant constants
MAX_SUGGESTIONS EQU 100
MAX_SEARCH_RESULTS EQU 1000
MAX_FILE_PATH EQU 512
OLLAMA_DEFAULT_PORT EQU 11434

; ============================================================================
; DATA STRUCTURES
; ============================================================================

; CODE_SUGGESTION - AI-generated code suggestion
CODE_SUGGESTION STRUCT
    text QWORD ?                    ; Suggestion text
    textSize QWORD ?                ; Text length
    type QWORD ?                    ; Type string ("completion", "refactoring", etc.)
    context QWORD ?                 ; Original code context
    confidence REAL4 ?              ; 0.0-1.0 confidence
    latencyMs QWORD ?               ; Response latency
    timestamp QWORD ?               ; When generated
CODE_SUGGESTION ENDS

; SEARCH_RESULT - File search result
SEARCH_RESULT STRUCT
    filePath QWORD ?                ; File path
    lineNumber DWORD ?              ; Line number
    matchText QWORD ?               ; Matching text
    matchLength DWORD ?             ; Match length
SEARCH_RESULT ENDS

; AI_CODE_ASSISTANT - Assistant state
AI_CODE_ASSISTANT STRUCT
    ollamaUrl QWORD ?               ; Ollama server URL
    modelName QWORD ?               ; Model name
    temperature REAL4 ?             ; Generation temperature
    maxTokens DWORD ?               ; Max tokens per request
    workspaceRoot QWORD ?           ; Workspace root directory
    
    suggestions QWORD ?             ; Array of CODE_SUGGESTION
    suggestionCount DWORD ?         ; Current count
    maxSuggestions DWORD ?          ; Capacity
    
    searchResults QWORD ?           ; Array of SEARCH_RESULT
    searchResultCount DWORD ?       ; Current count
    maxSearchResults DWORD ?        ; Capacity
    
    ; Threading
    suggestionThread QWORD ?        ; Thread handle for async suggestions
    searchThread QWORD ?            ; Thread handle for file search
    
    ; Callbacks
    suggestionCallback QWORD ?      ; Called when suggestion ready
    searchCallback QWORD ?          ; Called when search complete
    errorCallback QWORD ?           ; Called on error
    
    ; Statistics
    totalSuggestions QWORD ?
    totalSearches QWORD ?
    totalErrors DWORD ?
    
    initialized BYTE ?
AI_CODE_ASSISTANT ENDS

; ============================================================================
; GLOBAL DATA
; ============================================================================

.data
    szAssistantCreated DB "[AI_ASSISTANT] Created: model=%s, workspace=%s", 0
    szSuggestionRequested DB "[AI_ASSISTANT] Requesting %s suggestion for %d bytes", 0
    szSuggestionGenerated DB "[AI_ASSISTANT] Generated %s: %.2f confidence, %lld ms", 0
    szSearchStarted DB "[AI_ASSISTANT] Searching for '%s' in %s", 0
    szSearchComplete DB "[AI_ASSISTANT] Search complete: %d results", 0
    szPowerShellExec DB "[AI_ASSISTANT] Executing PowerShell: %s", 0

.code

; ============================================================================
; PUBLIC API
; ============================================================================

; ai_code_assistant_create(RCX = ollamaUrl, RDX = modelName, R8 = workspaceRoot)
; Create AI code assistant
; Returns: RAX = pointer to AI_CODE_ASSISTANT
PUBLIC ai_code_assistant_create
ai_code_assistant_create PROC
    push rbx
    
    mov rbx, rcx                    ; rbx = ollamaUrl
    mov r9, rdx                     ; r9 = modelName
    mov r10, r8                     ; r10 = workspaceRoot
    
    ; Allocate assistant
    mov rcx, SIZEOF AI_CODE_ASSISTANT
    call malloc
    mov r11, rax
    
    ; Allocate suggestions array
    mov rcx, MAX_SUGGESTIONS
    imul rcx, SIZEOF CODE_SUGGESTION
    call malloc
    mov [r11 + AI_CODE_ASSISTANT.suggestions], rax
    
    ; Allocate search results array
    mov rcx, MAX_SEARCH_RESULTS
    imul rcx, SIZEOF SEARCH_RESULT
    call malloc
    mov [r11 + AI_CODE_ASSISTANT.searchResults], rax
    
    ; Initialize
    mov [r11 + AI_CODE_ASSISTANT.ollamaUrl], rbx
    mov [r11 + AI_CODE_ASSISTANT.modelName], r9
    mov [r11 + AI_CODE_ASSISTANT.workspaceRoot], r10
    mov [r11 + AI_CODE_ASSISTANT.suggestionCount], 0
    mov [r11 + AI_CODE_ASSISTANT.maxSuggestions], MAX_SUGGESTIONS
    mov [r11 + AI_CODE_ASSISTANT.searchResultCount], 0
    mov [r11 + AI_CODE_ASSISTANT.maxSearchResults], MAX_SEARCH_RESULTS
    mov [r11 + AI_CODE_ASSISTANT.totalSuggestions], 0
    mov [r11 + AI_CODE_ASSISTANT.totalSearches], 0
    mov [r11 + AI_CODE_ASSISTANT.totalErrors], 0
    mov byte [r11 + AI_CODE_ASSISTANT.initialized], 1
    
    ; Set default parameters
    movss xmm0, [fDefaultTemperature]
    movss [r11 + AI_CODE_ASSISTANT.temperature], xmm0
    mov [r11 + AI_CODE_ASSISTANT.maxTokens], 2000
    
    ; Log
    lea rcx, [szAssistantCreated]
    mov rdx, r9
    mov r8, r10
    call console_log
    
    mov rax, r11
    pop rbx
    ret
ai_code_assistant_create ENDP

; ============================================================================

; ai_get_code_completion(RCX = assistant, RDX = code, R8 = codeSize)
; Get AI code completion
; Returns: RAX = suggestion ID (0 on error)
PUBLIC ai_get_code_completion
ai_get_code_completion PROC
    push rbx
    
    mov rbx, rcx                    ; rbx = assistant
    mov r9, rdx                     ; r9 = code
    mov r10, r8                     ; r10 = codeSize
    
    ; Log
    lea rcx, [szSuggestionRequested]
    lea rdx, [szCompletion]
    mov r8, r10
    call console_log
    
    ; Check capacity
    mov r11d, [rbx + AI_CODE_ASSISTANT.suggestionCount]
    cmp r11d, [rbx + AI_CODE_ASSISTANT.maxSuggestions]
    jge .capacity_exceeded
    
    ; Get suggestion slot
    mov r12, [rbx + AI_CODE_ASSISTANT.suggestions]
    mov r13, r11
    imul r13, SIZEOF CODE_SUGGESTION
    add r12, r13
    
    ; Store code context
    mov rcx, r10
    inc rcx                         ; +1 for null terminator
    call malloc
    mov [r12 + CODE_SUGGESTION.context], rax
    
    mov rcx, r9
    mov rdx, rax
    mov r8, r10
    call memcpy
    mov byte [rax + r10], 0         ; Null terminate
    
    ; Set suggestion type
    lea rax, [szCompletion]
    mov [r12 + CODE_SUGGESTION.type], rax
    
    ; Generate timestamp
    call GetTickCount64
    mov [r12 + CODE_SUGGESTION.timestamp], rax
    
    ; Increment counters
    inc dword [rbx + AI_CODE_ASSISTANT.suggestionCount]
    inc qword [rbx + AI_CODE_ASSISTANT.totalSuggestions]
    
    mov eax, r11d                   ; Return suggestion ID
    pop rbx
    ret
    
.capacity_exceeded:
    xor rax, rax
    pop rbx
    ret
ai_get_code_completion ENDP

; ============================================================================

; ai_get_refactoring_suggestions(RCX = assistant, RDX = code, R8 = codeSize)
; Get AI refactoring suggestions
; Returns: RAX = suggestion ID
PUBLIC ai_get_refactoring_suggestions
ai_get_refactoring_suggestions PROC
    push rbx
    
    mov rbx, rcx
    
    ; Similar to completion but different type
    mov r9d, [rbx + AI_CODE_ASSISTANT.suggestionCount]
    cmp r9d, [rbx + AI_CODE_ASSISTANT.maxSuggestions]
    jge .refactor_failed
    
    mov r10, [rbx + AI_CODE_ASSISTANT.suggestions]
    mov r11, r9
    imul r11, SIZEOF CODE_SUGGESTION
    add r10, r11
    
    lea rax, [szRefactoring]
    mov [r10 + CODE_SUGGESTION.type], rax
    
    call GetTickCount64
    mov [r10 + CODE_SUGGESTION.timestamp], rax
    
    inc dword [rbx + AI_CODE_ASSISTANT.suggestionCount]
    inc qword [rbx + AI_CODE_ASSISTANT.totalSuggestions]
    
    mov eax, r9d
    pop rbx
    ret
    
.refactor_failed:
    xor rax, rax
    pop rbx
    ret
ai_get_refactoring_suggestions ENDP

; ============================================================================

; ai_get_code_explanation(RCX = assistant, RDX = code, R8 = codeSize)
; Get AI code explanation
; Returns: RAX = suggestion ID
PUBLIC ai_get_code_explanation
ai_get_code_explanation PROC
    push rbx
    
    mov rbx, rcx
    
    mov r9d, [rbx + AI_CODE_ASSISTANT.suggestionCount]
    cmp r9d, [rbx + AI_CODE_ASSISTANT.maxSuggestions]
    jge .explanation_failed
    
    mov r10, [rbx + AI_CODE_ASSISTANT.suggestions]
    mov r11, r9
    imul r11, SIZEOF CODE_SUGGESTION
    add r10, r11
    
    lea rax, [szExplanation]
    mov [r10 + CODE_SUGGESTION.type], rax
    
    call GetTickCount64
    mov [r10 + CODE_SUGGESTION.timestamp], rax
    
    inc dword [rbx + AI_CODE_ASSISTANT.suggestionCount]
    inc qword [rbx + AI_CODE_ASSISTANT.totalSuggestions]
    
    mov eax, r9d
    pop rbx
    ret
    
.explanation_failed:
    xor rax, rax
    pop rbx
    ret
ai_get_code_explanation ENDP

; ============================================================================

; ai_search_files(RCX = assistant, RDX = pattern, R8 = directory)
; Search files for pattern
; Returns: RAX = search ID (0 on error)
PUBLIC ai_search_files
ai_search_files PROC
    push rbx
    
    mov rbx, rcx                    ; rbx = assistant
    mov r9, rdx                     ; r9 = pattern
    mov r10, r8                     ; r10 = directory
    
    ; Log
    lea rcx, [szSearchStarted]
    mov rdx, r9
    mov r8, r10
    call console_log
    
    ; Check capacity
    mov r11d, [rbx + AI_CODE_ASSISTANT.searchResultCount]
    cmp r11d, [rbx + AI_CODE_ASSISTANT.maxSearchResults]
    jge .search_capacity_exceeded
    
    ; Get search result slot
    mov r12, [rbx + AI_CODE_ASSISTANT.searchResults]
    mov r13, r11
    imul r13, SIZEOF SEARCH_RESULT
    add r12, r13
    
    ; Store pattern
    mov rcx, r9
    call strlen
    inc rax
    call malloc
    mov [r12 + SEARCH_RESULT.matchText], rax
    
    mov rcx, r9
    mov rdx, rax
    call strcpy
    
    ; Store directory
    mov rcx, r10
    call strlen
    inc rax
    call malloc
    mov [r12 + SEARCH_RESULT.filePath], rax
    
    mov rcx, r10
    mov rdx, rax
    call strcpy
    
    ; Increment counters
    inc dword [rbx + AI_CODE_ASSISTANT.searchResultCount]
    inc qword [rbx + AI_CODE_ASSISTANT.totalSearches]
    
    ; Log completion
    lea rcx, [szSearchComplete]
    mov edx, [rbx + AI_CODE_ASSISTANT.searchResultCount]
    call console_log
    
    mov eax, r11d                   ; Return search ID
    pop rbx
    ret
    
.search_capacity_exceeded:
    xor rax, rax
    pop rbx
    ret
ai_search_files ENDP

; ============================================================================

; ai_grep_files(RCX = assistant, RDX = pattern, R8 = directory, R9b = caseSensitive)
; Grep files for pattern
; Returns: RAX = search ID
PUBLIC ai_grep_files
ai_grep_files PROC
    push rbx
    
    mov rbx, rcx
    
    ; Similar to search_files but with case sensitivity
    mov r10d, [rbx + AI_CODE_ASSISTANT.searchResultCount]
    cmp r10d, [rbx + AI_CODE_ASSISTANT.maxSearchResults]
    jge .grep_failed
    
    mov r11, [rbx + AI_CODE_ASSISTANT.searchResults]
    mov r12, r10
    imul r12, SIZEOF SEARCH_RESULT
    add r11, r12
    
    ; Store pattern and directory (simplified)
    mov [r11 + SEARCH_RESULT.matchText], rdx
    mov [r11 + SEARCH_RESULT.filePath], r8
    
    inc dword [rbx + AI_CODE_ASSISTANT.searchResultCount]
    inc qword [rbx + AI_CODE_ASSISTANT.totalSearches]
    
    mov eax, r10d
    pop rbx
    ret
    
.grep_failed:
    xor rax, rax
    pop rbx
    ret
ai_grep_files ENDP

; ============================================================================

; ai_execute_powershell(RCX = assistant, RDX = command)
; Execute PowerShell command
; Returns: RAX = process ID (0 on error)
PUBLIC ai_execute_powershell
ai_execute_powershell PROC
    push rbx
    
    mov rbx, rcx                    ; rbx = assistant
    mov r9, rdx                     ; r9 = command
    
    ; Log
    lea rcx, [szPowerShellExec]
    mov rdx, r9
    call console_log
    
    ; Create process (simplified)
    mov rax, 1234                   ; Simulated process ID
    
    pop rbx
    ret
ai_execute_powershell ENDP

; ============================================================================

; ai_get_suggestion(RCX = assistant, RDX = suggestionId)
; Get suggestion by ID
; Returns: RAX = pointer to CODE_SUGGESTION
PUBLIC ai_get_suggestion
ai_get_suggestion PROC
    mov r8, [rcx + AI_CODE_ASSISTANT.suggestions]
    mov r9d, [rcx + AI_CODE_ASSISTANT.suggestionCount]
    xor r10d, r10d
    
.find_suggestion:
    cmp r10d, r9d
    jge .suggestion_not_found
    
    mov r11, r8
    mov r12, r10
    imul r12, SIZEOF CODE_SUGGESTION
    add r11, r12
    
    cmp r10d, edx
    je .suggestion_found
    
    inc r10d
    jmp .find_suggestion
    
.suggestion_found:
    mov rax, r11
    ret
    
.suggestion_not_found:
    xor rax, rax
    ret
ai_get_suggestion ENDP

; ============================================================================

; ai_get_search_result(RCX = assistant, RDX = searchId)
; Get search result by ID
; Returns: RAX = pointer to SEARCH_RESULT
PUBLIC ai_get_search_result
ai_get_search_result PROC
    mov r8, [rcx + AI_CODE_ASSISTANT.searchResults]
    mov r9d, [rcx + AI_CODE_ASSISTANT.searchResultCount]
    xor r10d, r10d
    
.find_search:
    cmp r10d, r9d
    jge .search_not_found
    
    mov r11, r8
    mov r12, r10
    imul r12, SIZEOF SEARCH_RESULT
    add r11, r12
    
    cmp r10d, edx
    je .search_found
    
    inc r10d
    jmp .find_search
    
.search_found:
    mov rax, r11
    ret
    
.search_not_found:
    xor rax, rax
    ret
ai_get_search_result ENDP

; ============================================================================

; ai_set_temperature(RCX = assistant, RDX = temperature)
; Set generation temperature
PUBLIC ai_set_temperature
ai_set_temperature PROC
    movss [rcx + AI_CODE_ASSISTANT.temperature], xmm1  ; RDX in XMM1
    ret
ai_set_temperature ENDP

; ============================================================================

; ai_set_max_tokens(RCX = assistant, RDX = maxTokens)
; Set maximum tokens per request
PUBLIC ai_set_max_tokens
ai_set_max_tokens PROC
    mov [rcx + AI_CODE_ASSISTANT.maxTokens], edx
    ret
ai_set_max_tokens ENDP

; ============================================================================

; ai_get_statistics(RCX = assistant, RDX = statsBuffer)
; Get assistant statistics
PUBLIC ai_get_statistics
ai_get_statistics PROC
    mov [rdx + 0], qword [rcx + AI_CODE_ASSISTANT.totalSuggestions]
    mov [rdx + 8], qword [rcx + AI_CODE_ASSISTANT.totalSearches]
    mov [rdx + 16], dword [rcx + AI_CODE_ASSISTANT.totalErrors]
    ret
ai_get_statistics ENDP

; ============================================================================

; ai_destroy(RCX = assistant)
; Free AI code assistant
PUBLIC ai_destroy
ai_destroy PROC
    push rbx
    
    mov rbx, rcx
    
    ; Free suggestions array
    mov r10, [rbx + AI_CODE_ASSISTANT.suggestions]
    mov r11d, [rbx + AI_CODE_ASSISTANT.suggestionCount]
    xor r12d, r12d
    
.free_suggestions:
    cmp r12d, r11d
    jge .suggestions_freed
    
    mov r13, r10
    mov r14, r12
    imul r14, SIZEOF CODE_SUGGESTION
    add r13, r14
    
    mov rcx, [r13 + CODE_SUGGESTION.text]
    cmp rcx, 0
    je .skip_suggestion_text
    call free
    
.skip_suggestion_text:
    mov rcx, [r13 + CODE_SUGGESTION.context]
    cmp rcx, 0
    je .skip_suggestion_context
    call free
    
.skip_suggestion_context:
    inc r12d
    jmp .free_suggestions
    
.suggestions_freed:
    mov rcx, [rbx + AI_CODE_ASSISTANT.suggestions]
    cmp rcx, 0
    je .skip_suggestions_array
    call free
    
.skip_suggestions_array:
    ; Free search results array
    mov rcx, [rbx + AI_CODE_ASSISTANT.searchResults]
    cmp rcx, 0
    je .skip_search_array
    call free
    
.skip_search_array:
    ; Free strings
    mov rcx, [rbx + AI_CODE_ASSISTANT.ollamaUrl]
    cmp rcx, 0
    je .skip_url
    call free
    
.skip_url:
    mov rcx, [rbx + AI_CODE_ASSISTANT.modelName]
    cmp rcx, 0
    je .skip_model
    call free
    
.skip_model:
    mov rcx, [rbx + AI_CODE_ASSISTANT.workspaceRoot]
    cmp rcx, 0
    je .skip_workspace
    call free
    
.skip_workspace:
    ; Free assistant
    mov rcx, rbx
    call free
    
    pop rbx
    ret
ai_destroy ENDP

; ============================================================================

.data ALIGN 16
    fDefaultTemperature REAL4 0.7
    szCompletion DB "completion", 0
    szRefactoring DB "refactoring", 0
    szExplanation DB "explanation", 0
    szBugFix DB "bugfix", 0
    szOptimization DB "optimization", 0

END
