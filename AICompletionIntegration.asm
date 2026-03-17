; ============================================================================
; AICompletionIntegration.asm - AI Backend Integration Module
; ============================================================================
; Demonstrates how to integrate RawrXD_TextEditorGUI with AI completion
; backends (llama.cpp, OpenAI, etc.)
; ============================================================================

.DATA
    ; HTTP/Network buffers for AI API communication
    ai_request_buffer db 'POST /completion HTTP/1.1', 0Dh, 0Ah
                      db 'Host: localhost:8000', 0Dh, 0Ah
                      db 'Content-Type: application/json', 0Dh, 0Ah
                      db 'Content-Length: 256', 0Dh, 0Ah, 0Dh, 0Ah
    
    ai_json_template db '{"prompt":"', 0
    ai_json_suffix db '","max_tokens":50}', 0
    
    ai_response_buffer db 4096 dup(0)  ; Response from API
    ai_tokens_output db 256 dup(0)     ; Parsed tokens
    
    ; Status messages
    ai_status_thinking db "Thinking...", 0
    ai_status_complete db "Completion ready", 0
    ai_status_error db "AI Error", 0

.CODE

; ============================================================================
; AI_GetCompletion(rcx=editor_window_data, rdx=ai_backend_url)
; Main entry point for AI completion request
; ============================================================================
; PARAMETERS:
;   rcx = editor window_data_ptr (contains buffer_ptr)
;   rdx = AI backend URL (e.g., "http://localhost:8000/v1/completions")
;
; OPERATION:
;   1. Get buffer snapshot (current text state)
;   2. Build JSON request for AI API
;   3. Send HTTP request to backend
;   4. Parse response for tokens
;   5. Insert tokens into buffer
;   6. Update status bar
;
; RETURNS: rax = 1 (success) or 0 (error)
;
; EXAMPLE USAGE (from background thread):
;   lea rcx, [window_data]
;   lea rdx, [ai_backend_url]
;   call AI_GetCompletion
;
; ============================================================================
AI_GetCompletion PROC FRAME
    .PUSHREG rbx
    .PUSHREG r12
    .PUSHREG r13
    .ALLOCSTACK 96
    .ENDPROLOG
    
    push rbx
    push r12
    push r13
    sub rsp, 96
    
    mov rbx, rcx                       ; rbx = window_data_ptr
    mov r12, rdx                       ; r12 = backend_url
    
    ; ====== STEP 1: Update status to "Thinking..." ======
    lea rcx, [rbx]
    lea rdx, [rel ai_status_thinking]
    call EditorWindow_UpdateStatus
    
    ; ====== STEP 2: Get buffer snapshot ======
    mov rcx, [rbx + 32]                ; rcx = buffer_ptr
    lea rdx, [rsp]                     ; rdx = local snapshot buffer
    call AICompletion_GetBufferSnapshot
    
    mov r13, rax                       ; r13 = snapshot size
    
    ; ====== STEP 3: Build JSON request ======
    ; [In real implementation: Build JSON with snapshot text]
    ; JSON structure: {"prompt": "<snapshot_text>", "max_tokens": 50}
    
    ; ====== STEP 4: Send HTTP request ======
    ; [In real implementation: Use WinHTTP or socket API]
    ; mov rcx, r12                      ; backend URL
    ; lea rdx, [request_json]
    ; call HTTP_PostRequest
    
    ; ====== STEP 5: Parse response ======
    ; [In real implementation: Extract tokens from JSON response]
    ; Response format: {"tokens": [49, 51, 52, ...]}
    
    ; ====== STEP 6: Insert tokens into buffer ======
    mov rcx, [rbx + 32]                ; rcx = buffer_ptr
    lea rdx, [rel ai_tokens_output]    ; rdx = parsed tokens
    mov r8d, 5                         ; r8d = token count (example: 5 tokens)
    call AICompletion_InsertTokens
    
    ; ====== STEP 7: Update status ======
    lea rcx, [rbx]
    lea rdx, [rel ai_status_complete]
    call EditorWindow_UpdateStatus
    
    mov rax, 1
    add rsp, 96
    pop r13
    pop r12
    pop rbx
    ret
AI_GetCompletion ENDP


; ============================================================================
; AI_ParseJsonResponse(rcx=response_buffer, rdx=token_output)
; Parse AI backend JSON response to extract tokens
; ============================================================================
; PARAMETERS:
;   rcx = JSON response from AI backend
;   rdx = output buffer for parsed tokens
;
; JSON Example Response:
;   {"choices":[{"text":"def hello():\n    pass"}]}
;
; OPERATION:
;   1. Find "text" field in JSON
;   2. Extract token characters
;   3. Place in output buffer
;   4. Return token count
;
; RETURNS: rax = token count
;
; ============================================================================
AI_ParseJsonResponse PROC FRAME
    .ALLOCSTACK 32
    .ENDPROLOG
    
    mov rbx, rcx                       ; rbx = response_buffer
    mov r12, rdx                       ; r12 = token_output
    
    ; Simple implementation: Find "text": and extract following characters
    xor r8d, r8d                       ; token count
    
    ; [Real implementation would use JSON parser]
    ; For now: placeholder returning 0 tokens
    
    mov rax, r8d
    ret
AI_ParseJsonResponse ENDP


; ============================================================================
; AI_OnCompletionComplete(rcx=editor_window_data, rdx=result)
; Called when AI completion finishes (success or error)
; ============================================================================
AI_OnCompletionComplete PROC FRAME
    .ALLOCSTACK 32
    .ENDPROLOG
    
    ; rdx = 1 (success) or 0 (error)
    
    test edx, edx
    jnz .CompletionSuccess
    
    ; Error handling
    lea rcx, [rcx]
    lea rdx, [rel ai_status_error]
    call EditorWindow_UpdateStatus
    ret
    
.CompletionSuccess:
    ; Success - status already updated by AI_GetCompletion
    ret
AI_OnCompletionComplete ENDP


; ============================================================================
; BACKGROUND THREAD WORKER - AI Completion
; ============================================================================
;
; In a multi-threaded implementation, spawn a thread that:
;
;   1. Waits for user to stop typing (delay ~500ms)
;   2. Calls AI_GetCompletion() when user pauses
;   3. AI backend request happens asynchronously
;   4. When complete, UI thread inserts tokens
;
; Pseudo-code:
;
;   Thread_AIWorker():
;       LOOP:
;           IF user_stopped_typing_for_500ms:
;               call AI_GetCompletion(window_data, backend_url)
;               IF error:
;                   show error toast
;               ELSE:
;                   show "Press Tab to accept" in status bar
;           SLEEP 100
;       END LOOP
;
; ============================================================================

; ============================================================================
; INTEGRATION EXAMPLE: Llama.cpp Backend
; ============================================================================
;
; Backend URL: http://localhost:8000/v1/completions
;
; Request:
;   POST /v1/completions HTTP/1.1
;   Content-Type: application/json
;   
;   {
;     "prompt": "def hello():\n    pass\n",
;     "max_tokens": 50,
;     "temperature": 0.7
;   }
;
; Response:
;   {
;     "choices": [
;       {
;         "text": "def hello():\n    ",
;         "finish_reason": "length"
;       }
;     ]
;   }
;
; Implementation steps:
;   1. AICompletion_GetBufferSnapshot() → get "def hello():\n    pass\n"
;   2. Build JSON request with above structure
;   3. Use WinHTTP API to POST to localhost:8000
;   4. Parse JSON response
;   5. AICompletion_InsertTokens() → add returned text
;
; ============================================================================

; ============================================================================
; INTEGRATION EXAMPLE: OpenAI API
; ============================================================================
;
; Backend URL: https://api.openai.com/v1/chat/completions
;
; Request:
;   POST /v1/chat/completions HTTP/1.1
;   Authorization: Bearer sk-...
;   Content-Type: application/json
;   
;   {
;     "model": "gpt-3.5-turbo",
;     "messages": [
;       {"role": "user", "content": "def hello():\n    pass"}
;     ],
;     "max_tokens": 50
;   }
;
; Response:
;   {
;     "choices": [
;       {
;         "message": {
;           "content": ":\n    print('Hello')"
;         }
;       }
;     ]
;   }
;
; Implementation steps:
;   1. AICompletion_GetBufferSnapshot() → export current text
;   2. Build chat message JSON
;   3. Add Authorization header with API key
;   4. POST to api.openai.com over HTTPS
;   5. Parse response message
;   6. AICompletion_InsertTokens() → insert completion
;
; ============================================================================

; ============================================================================
; PSEUDO-CODE: Full Integration Flow
; ============================================================================
;
; /* Main Application */
; WinMain():
;     window_data = malloc(96)
;     hwnd = IDE_CreateMainWindow("RawrXD", window_data)
;     hAccel = IDE_SetupAccelerators(hwnd)
;     
;     /* Spawn AI worker thread */
;     hAIThread = CreateThread(AI_Worker, window_data)
;     
;     /* Main message loop (blocking) */
;     IDE_MessageLoop(hwnd, hAccel)
;     
;     CloseHandle(hAIThread)
;     free(window_data)
; END
;
; /* Background AI thread */
; AI_Worker(window_data):
;     backend_url = "http://localhost:8000/v1/completions"
;     
;     LOOP:
;         IF AI completion requested by user:
;             /* User typed something and paused */
;             AI_GetCompletion(window_data, backend_url)
;             /* This function:
;                - Gets current buffer via GetBufferSnapshot
;                - Sends to AI backend
;                - Parses response
;                - Inserts tokens via InsertTokens */
;         
;         Sleep(500)  /* Check every 500ms */
;     END LOOP
; END
;
; ============================================================================

; ============================================================================
; ERROR HANDLING IN AI INTEGRATION
; ============================================================================
;
; Network errors:
;   - No connection to backend → Show "Offline" in status bar
;   - Timeout → Show "Timeout" and cancel request
;   - HTTP error (500) → Show "Server Error"
;
; Parsing errors:
;   - Invalid JSON response → Ignore, leave text unchanged
;   - Empty tokens → No-op
;
; Buffer errors:
;   - Buffer full (InsertTokens returns 0) → Show "Buffer Full"
;
; ============================================================================

; ============================================================================
; EXAMPLE: Token Insertion with User Confirmation
; ============================================================================
;
; /* After AI provides completion */
; 
; 1. InsertTokens() adds text at cursor
; 2. Highlight inserted text (e.g., light blue background)
; 3. Show status: "Press Esc to reject, Tab to accept"
; 4. Wait for user input:
;    - Esc → Undo insertion (Ctrl+Z)
;    - Tab → Keep insertion, move cursor after
;    - Other key → Normal editing
;
; This provides non-destructive completion experience
;
; ============================================================================

END
