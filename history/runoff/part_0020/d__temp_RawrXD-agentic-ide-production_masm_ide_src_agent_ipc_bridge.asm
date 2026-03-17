; ============================================================================
; File 31: agent_ipc_bridge.asm - Named pipe IPC to agentic process
; ============================================================================
; Purpose: Marshalling requests/responses to external agent, hotpatcher integration
; Uses: Named pipes, JSON serialization, async response handling
; Functions: Init, SendRequest, ReceiveResponse, ProcessCompletion
; ============================================================================

.code

; CONSTANTS
; ============================================================================

AGENT_PIPE_NAME         BYTE "\\\\.\\pipe\\rawrxd-agent", 0
PIPE_BUFFER_SIZE        equ 1048576  ; 1MB
IPC_MAX_PENDING         equ 100

; REQUEST/RESPONSE STRUCTURES
; ============================================================================

; AgentRequest {
;   requestId: u32,
;   type: u32,     ; 1=CodeGeneration, 2=Refactoring, 3=Debugging, 4=Explanation
;   operation: str[256],
;   context: str[8192],
;   selectionStart: u64,
;   selectionEnd: u64,
; }

; AgentResponse {
;   requestId: u32,
;   success: bool,
;   result: str[8192],
;   edits: Edit[100],
;   applyHotpatch: bool,
; }

; INITIALIZATION
; ============================================================================

AgentBridge_Init PROC USES rbx rcx rdx rsi rdi
    ; Returns: AgentBridge* in rax
    ; { pipeHandle, connected, requestQueue, responseQueue, requestID, workerThread, mutex }
    
    sub rsp, 40
    call GetProcessHeap
    add rsp, 40
    mov rcx, rax
    
    ; Allocate main struct (128 bytes)
    mov rdx, 0
    mov r8, 128
    sub rsp, 40
    call HeapAlloc
    add rsp, 40
    mov rbx, rax  ; rbx = AgentBridge*
    
    ; Initialize fields
    mov qword ptr [rbx + 0], 0      ; pipeHandle = INVALID_HANDLE_VALUE
    mov qword ptr [rbx + 8], 0      ; connected = false
    mov qword ptr [rbx + 16], 0     ; requestQueue
    mov qword ptr [rbx + 24], 0     ; responseQueue
    mov qword ptr [rbx + 32], 1     ; requestID = 1
    
    ; Initialize CRITICAL_SECTION
    lea rdx, [rbx + 56]
    sub rsp, 40
    mov rcx, rdx
    call InitializeCriticalSection
    add rsp, 40
    
    ; Allocate request/response queues
    mov rcx, rbx
    sub rsp, 40
    call GetProcessHeap
    add rsp, 40
    mov rcx, rax
    mov rdx, 0
    mov r8, 10485760  ; 10MB for request queue
    sub rsp, 40
    call HeapAlloc
    add rsp, 40
    mov [rbx + 16], rax  ; requestQueue
    
    mov rcx, rbx
    sub rsp, 40
    call GetProcessHeap
    add rsp, 40
    mov rcx, rax
    mov rdx, 0
    mov r8, 10485760
    sub rsp, 40
    call HeapAlloc
    add rsp, 40
    mov [rbx + 24], rax  ; responseQueue
    
    mov rax, rbx
    ret
AgentBridge_Init ENDP

; CONNECT TO AGENT PROCESS
; ============================================================================

AgentBridge_Connect PROC USES rbx rcx rdx rsi rdi bridge:PTR DWORD
    ; Connect to agent via named pipe
    ; Returns: 1 if connected, 0 if failed (waits for agent startup)
    
    mov rcx, bridge
    lea rdx, [rcx + 56]
    sub rsp, 40
    mov rcx, rdx
    call EnterCriticalSection
    add rsp, 40
    
    mov rcx, bridge
    
    ; Try to open named pipe (wait up to 5 seconds)
    lea rdx, [rel AGENT_PIPE_NAME]
    mov r8, 0x40000000  ; GENERIC_WRITE
    mov r9, 0           ; no sharing
    
    sub rsp, 40
    mov rcx, rdx
    mov rdx, r8
    mov r8, r9
    mov r9, 0           ; flags
    call CreateFileA
    add rsp, 40
    
    cmp rax, -1
    je @connect_failed
    
    mov rsi, rax  ; rsi = pipe handle
    
    ; Set pipe mode to message mode
    mov r8, 2  ; PIPE_READMODE_MESSAGE
    sub rsp, 40
    mov rcx, rsi
    mov rdx, offset mode_ptr
    mov r8, 2
    call SetNamedPipeHandleState
    add rsp, 40
    
    ; Store handle and mark connected
    mov rcx, bridge
    mov [rcx + 0], rsi       ; pipeHandle
    mov qword ptr [rcx + 8], 1  ; connected = true
    
    mov rcx, bridge
    lea rdx, [rcx + 56]
    sub rsp, 40
    mov rcx, rdx
    call LeaveCriticalSection
    add rsp, 40
    
    mov rax, 1
    ret
    
@connect_failed:
    mov rcx, bridge
    lea rdx, [rcx + 56]
    sub rsp, 40
    mov rcx, rdx
    call LeaveCriticalSection
    add rsp, 40
    
    mov rax, 0
    ret
AgentBridge_Connect ENDP

; SEND REQUEST TO AGENT
; ============================================================================

AgentBridge_SendRequest PROC USES rbx rcx rdx rsi rdi r8 r9 bridge:PTR DWORD, requestType:DWORD, operation:PTR BYTE, context:PTR BYTE
    ; bridge = AgentBridge*
    ; requestType = 1=CodeGen, 2=Refactoring, etc.
    ; operation = "complete_function", "rename_variable", etc.
    ; context = surrounding code/selection
    ; Returns: request ID in rax
    
    mov rcx, bridge
    lea rdx, [rcx + 56]
    sub rsp, 40
    mov rcx, rdx
    call EnterCriticalSection
    add rsp, 40
    
    mov rcx, bridge
    mov r8d, [requestType]
    mov rsi, operation
    mov rdi, context
    
    ; Check if connected
    mov rax, [rcx + 8]
    cmp rax, 0
    je @send_not_connected
    
    ; Get next request ID
    mov rax, [rcx + 32]
    mov r9, rax
    inc rax
    mov [rcx + 32], rax  ; increment for next
    
    ; Build JSON request
    ; {
    ;   "jsonrpc": "2.0",
    ;   "id": N,
    ;   "method": "agent/request",
    ;   "params": {
    ;     "type": 1,
    ;     "operation": "complete_function",
    ;     "context": "...",
    ;     "selectionStart": 0,
    ;     "selectionEnd": 0
    ;   }
    ; }
    
    sub rsp, 40
    call GetProcessHeap
    add rsp, 40
    mov rbx, rax
    mov rcx, rbx
    mov rdx, 0
    mov r8, 16384  ; 16KB for request
    sub rsp, 40
    call HeapAlloc
    add rsp, 40
    mov r10, rax  ; r10 = request buffer
    
    ; Serialize request (wsprintf-style)
    mov rcx, r10
    lea rdx, [rel request_template]
    mov r8, r9         ; request ID
    mov r11d, r8d      ; request type
    
    ; TODO: Implement JSON serialization with wsprintf
    
    ; Send via named pipe
    mov rcx, [bridge + 0]  ; pipeHandle
    mov rdx, r10
    mov r8, 16384
    mov r9, 0  ; bytes written output
    
    sub rsp, 40
    mov rcx, rcx
    mov rdx, rdx
    mov r8, 8
    lea r9, [rsp + 32]
    mov [rsp + 32], r9
    call WriteFile
    add rsp, 40
    
    mov rax, r9  ; return request ID
    
    mov rcx, bridge
    lea rdx, [rcx + 56]
    sub rsp, 40
    mov rcx, rdx
    call LeaveCriticalSection
    add rsp, 40
    
    ret
    
@send_not_connected:
    mov rcx, bridge
    lea rdx, [rcx + 56]
    sub rsp, 40
    mov rcx, rdx
    call LeaveCriticalSection
    add rsp, 40
    
    mov rax, -1
    ret
AgentBridge_SendRequest ENDP

; RECEIVE RESPONSE FROM AGENT
; ============================================================================

AgentBridge_ReceiveResponse PROC USES rbx rcx rdx rsi rdi r8 r9 bridge:PTR DWORD, requestID:DWORD
    ; Blocking wait for agent response to specific request
    ; Returns: response JSON* in rax (or NULL if timeout)
    
    mov rcx, bridge
    mov edx, [requestID]
    
    ; Loop until response arrives
    mov r8, 0  ; timeout counter
    
@recv_loop:
    cmp r8, 1000  ; timeout after ~10 seconds
    jge @recv_timeout
    
    ; Read from named pipe
    mov rcx, [bridge + 0]  ; pipeHandle
    mov rdx, [bridge + 24]  ; responseQueue buffer
    mov r8, PIPE_BUFFER_SIZE
    mov r9, 0  ; bytes read output
    
    sub rsp, 40
    mov rcx, rcx
    mov rdx, rdx
    mov r8, 65536
    lea r9, [rsp + 32]
    mov [rsp + 32], r9
    call ReadFile
    add rsp, 40
    
    ; Check if response is for our request ID
    ; Parse JSON, extract "id" field
    mov rsi, [bridge + 24]  ; response buffer
    
    ; Look for "id": N pattern
    ; (TODO: implement JSON parsing)
    
    ; If match, return response
    mov rax, rsi
    ret
    
    ; Sleep before retry
    mov rcx, 10  ; 10ms
    sub rsp, 40
    call Sleep
    add rsp, 40
    
    inc r8
    jmp @recv_loop
    
@recv_timeout:
    mov rax, 0
    ret
AgentBridge_ReceiveResponse ENDP

; PROCESS COMPLETION
; ============================================================================

AgentBridge_ProcessCompletion PROC USES rbx rcx rdx rsi rdi r8 r9 bridge:PTR DWORD, response:PTR BYTE, editor:PTR DWORD
    ; Apply agent response to editor
    ; Returns: 1 if applied successfully
    
    mov rcx, bridge
    mov rsi, response
    mov rdi, editor
    
    ; Parse response JSON:
    ; {
    ;   "id": 123,
    ;   "result": "...",
    ;   "edits": [
    ;     {"range": {"start": 0, "end": 10}, "text": "new text"}
    ;   ],
    ;   "applyHotpatch": true
    ; }
    
    ; Step 1: Extract result string
    mov r8, 0
    
@parse_result:
    mov al, byte ptr [rsi]
    cmp al, 0
    je @parse_done
    
    ; Look for "result": "..." pattern
    ; (TODO: implement JSON field extraction)
    
    inc rsi
    jmp @parse_result
    
@parse_done:
    ; Step 2: Extract edits array
    ; (TODO: parse edits array)
    
    ; Step 3: Apply edits to editor
    ; For each edit: DeleteRange(start, end), InsertText(text)
    
    ; Step 4: Check applyHotpatch flag
    ; If true, call ProxyHotpatcher to apply transformation
    
    ; Step 5: Trigger rendering update
    
    mov rax, 1
    ret
AgentBridge_ProcessCompletion ENDP

; REQUEST TEMPLATE
; ============================================================================

request_template BYTE '{"jsonrpc":"2.0","id":%d,"method":"agent/request","params":{"type":%d,"operation":"%s","context":"%s"}}', 0

; HELPER: Mode pointer for SetNamedPipeHandleState
; ============================================================================

mode_ptr DWORD 2

end
