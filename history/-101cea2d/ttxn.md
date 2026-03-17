# INTEGRATION BRIDGE QUICK REFERENCE

## API Overview (All Functions)

### Qt Signal Bridge (masm_qt_bridge.asm)

```asm
; Initialize bridge system
masm_qt_bridge_init() -> bool (eax)

; Register signal handler (max 32)
masm_signal_connect(ecx=signal_id, rdx=callback_addr) -> bool (eax)

; Unregister signal handler
masm_signal_disconnect(ecx=signal_id) -> bool (eax)

; Emit signal to all registered handlers
masm_signal_emit(ecx=signal_id, rdx=param) -> bool (eax)

; Invoke callback with parameters
masm_callback_invoke(rcx=callback, rdx=param1, r8=param2) -> result (rax)

; Process pending Qt events (non-blocking)
masm_event_pump() -> bool (eax)

; Thread-safe function call with mutex
masm_thread_safe_call(rcx=func, rdx=param) -> result (rax)
```

### Thread Coordination (masm_thread_coordinator.asm)

```asm
; Create thread pool with min/max threads
thread_coordinator_init(ecx=min_threads, edx=max_threads) -> bool (eax)

; Clean shutdown
thread_coordinator_shutdown() -> void

; Queue work item (max 256, returns immediately)
thread_safe_queue_work(rcx=func, rdx=param, r8d=priority) -> bool (eax)

; Wait for all work to complete
thread_wait_for_completion(ecx=timeout_ms) -> bool (eax)

; Get current thread ID
thread_get_current_id() -> thread_id (eax)

; Spawn new worker thread
thread_create_worker() -> bool (eax)

; Signal thread event (32 max)
thread_signal_event(ecx=event_id) -> bool (eax)

; Wait for thread event with timeout
thread_wait_event(ecx=event_id, edx=timeout_ms) -> bool (eax)
```

### Memory Bridge (masm_memory_bridge.asm)

```asm
; Initialize private heap
memory_bridge_init() -> bool (eax)

; Allocate aligned memory (16-byte alignment, <100MB)
memory_bridge_alloc(rcx=size) -> ptr (rax)

; Free memory
memory_bridge_free(rcx=ptr) -> bool (eax)

; Safe memory copy with validation
memory_bridge_copy(rcx=dest, rdx=src, r8=size) -> bool (eax)

; Validate pointer safety
memory_bridge_validate(rcx=ptr) -> bool (eax)

; Get statistics
memory_bridge_get_stats() -> (rax=total_allocated, rcx=block_count)
```

### I/O Reactor (masm_io_reactor.asm)

```asm
; Initialize I/O reactor
io_reactor_init() -> bool (eax)

; Register handle for monitoring (max 32)
io_reactor_add(rcx=handle, edx=type, r8=callback) -> bool (eax)

; Unregister handle
io_reactor_remove(rcx=handle) -> bool (eax)

; Wait for I/O events
io_reactor_wait(ecx=timeout_ms) -> event_count (eax)

; Process ready I/O and invoke callbacks
io_reactor_process() -> void

; Cancel pending I/O on handle
io_reactor_cancel(rcx=handle) -> bool (eax)

; Shutdown reactor
io_reactor_shutdown() -> void
```

Handle Types:
- 1 = IO_TYPE_FILE
- 2 = IO_TYPE_PIPE
- 3 = IO_TYPE_TIMER
- 4 = IO_TYPE_NETWORK

### Agent Integration (masm_agent_integration.asm)

```asm
; Initialize agent integration
agent_integration_init() -> bool (eax)

; Send message to agent (returns request_id)
agent_chat_send_message(rcx=msg_ptr, edx=mode) -> request_id (eax)

; Receive agent response
agent_chat_receive_response(ecx=req_id) -> response_ptr (rax)

; Apply hotpatch from agent
agent_hotpatch_apply(rcx=patch_data) -> bool (eax)

; Execute multi-file plan
agent_plan_execute(ecx=plan_id) -> bool (eax)

; Push context on stack (max 16)
agent_context_push(rcx=context_ptr) -> bool (eax)

; Pop context from stack
agent_context_pop() -> context_ptr (rax)

; Get confidence score (0-100%)
agent_get_confidence(ecx=req_id) -> percent (eax)
```

Agent Modes:
- 0 = Ask
- 1 = Edit
- 2 = Plan
- 3 = Debug
- 4 = Optimize

---

## Common Usage Patterns

### Pattern 1: Register a Signal Handler

```asm
; Define callback
my_signal_handler PROC uses rbx
    ; rcx = signal parameter
    mov rax, rcx
    ; ... process signal ...
    ret
my_signal_handler ENDP

; Initialize bridge (once on startup)
call masm_qt_bridge_init
test eax, eax
jz .error

; Connect to signal
mov ecx, 1001h              ; SIG_CHAT_MESSAGE_RECEIVED
lea rdx, my_signal_handler
call masm_signal_connect
test eax, eax
jz .connect_error
```

### Pattern 2: Emit Signal

```asm
; Emit signal to all registered handlers
mov ecx, 1001h              ; SIG_CHAT_MESSAGE_RECEIVED
lea rdx, message_data
call masm_signal_emit
test eax, eax
jz .emit_error
```

### Pattern 3: Queue Work Item

```asm
; Define work function
work_function PROC uses rbx
    ; rcx = work parameter
    ; ... do work ...
    ret
work_function ENDP

; Initialize thread pool
mov ecx, 2                  ; min 2 threads
mov edx, 8                  ; max 8 threads
call thread_coordinator_init
test eax, eax
jz .init_error

; Queue work
mov rcx, OFFSET work_function
mov rdx, param_value
mov r8d, 0                  ; priority
call thread_safe_queue_work
test eax, eax
jz .queue_error

; Wait for completion
mov ecx, 5000               ; 5 second timeout
call thread_wait_for_completion
test eax, eax
jz .timeout_error
```

### Pattern 4: Allocate Shared Memory

```asm
; Initialize memory bridge
call memory_bridge_init
test eax, eax
jz .init_error

; Allocate buffer
mov rcx, 4096               ; 4KB buffer
call memory_bridge_alloc
test rax, rax
jz .alloc_error
mov buffer_ptr, rax

; Validate before use
mov rcx, buffer_ptr
call memory_bridge_validate
test eax, eax
jz .invalid_ptr

; Use buffer...

; Free when done
mov rcx, buffer_ptr
call memory_bridge_free
test eax, eax
jz .free_error
```

### Pattern 5: Register I/O Handle

```asm
; Define I/O callback
io_callback PROC uses rbx
    ; rcx = context pointer
    ; ... handle I/O event ...
    ret
io_callback ENDP

; Initialize reactor
call io_reactor_init
test eax, eax
jz .init_error

; Register file handle
mov rcx, hFile              ; File handle
mov edx, 1                  ; IO_TYPE_FILE
lea r8, io_callback
call io_reactor_add
test eax, eax
jz .add_error

; Wait for I/O
mov ecx, 1000               ; 1 second timeout
call io_reactor_wait
test eax, eax
jz .timeout

; Process events
call io_reactor_process
```

### Pattern 6: Send Agent Message

```asm
; Initialize agent
call agent_integration_init
test eax, eax
jz .init_error

; Send message
mov rcx, OFFSET user_message
mov edx, 0                  ; AGENT_MODE_ASK
call agent_chat_send_message
mov request_id, eax

; Receive response
mov ecx, request_id
call agent_chat_receive_response
mov response_ptr, rax

; Get confidence
mov ecx, request_id
call agent_get_confidence
mov confidence_percent, eax
```

---

## Error Handling

All functions return status in EAX:
- **1 (True)** = Success
- **0 (False)** = Failure

For functions returning pointers:
- **Non-zero** = Valid pointer
- **0 (NULL)** = Allocation failed

### Safe Pattern

```asm
; All operations should check return value
mov rcx, parameter
call some_function
test eax, eax               ; eax=0 means error
jz .error_handler

; Continue on success...
jmp .exit

.error_handler:
; Log error and cleanup
mov rcx, OFFSET szErrorMsg
call log_error
; ... cleanup ...

.exit:
ret
```

---

## Thread Safety Guarantees

### Already Protected (No Additional Locking Needed)
✅ All public functions in bridges  
✅ Signal registration/emission  
✅ Work queue operations  
✅ Memory allocation/freeing  
✅ I/O handle management  

### Caller Responsibility
- Don't call same callback simultaneously from multiple threads
- Don't access work result while work is queued
- Don't free memory still being accessed by another thread
- Don't call shutdown while operations are pending

---

## Performance Tips

### Optimization 1: Batch Signal Emits
```asm
; Bad: emit 100 times
mov ecx, 100
.loop:
    call masm_signal_emit      ; 32+ context switches
    loop .loop

; Good: emit once with aggregated data
call masm_signal_emit           ; Single emit
```

### Optimization 2: Reuse Work Queue
```asm
; Bad: reinit pool for each batch
call thread_coordinator_init
call thread_safe_queue_work
call thread_coordinator_shutdown

; Good: init once, queue many
call thread_coordinator_init    ; Once
.loop:
    call thread_safe_queue_work ; Many times
call thread_coordinator_shutdown ; Once
```

### Optimization 3: Use Memory Pool
```asm
; Pre-allocate buffers
call memory_bridge_alloc        ; Allocate once
mov buffer1, rax
call memory_bridge_alloc        ; Reuse for multiple ops
mov buffer2, rax

; ... use buffers ...

; Free when truly done
call memory_bridge_free
call memory_bridge_free
```

### Optimization 4: Bulk I/O Processing
```asm
; Wait once for multiple handles
call io_reactor_wait            ; Wait for any
cmp eax, 0
je .no_events

; Process all ready at once
call io_reactor_process         ; Invoke all callbacks
```

---

## Common Pitfalls

### ❌ Mistake 1: Not Checking Return Values
```asm
; WRONG: No error checking
call masm_signal_connect
call masm_signal_emit

; RIGHT: Check all returns
call masm_signal_connect
test eax, eax
jz .connect_error

call masm_signal_emit
test eax, eax
jz .emit_error
```

### ❌ Mistake 2: Incorrect Parameter Order
```asm
; WRONG: Parameters in wrong registers
mov rdx, signal_id
mov ecx, callback
call masm_signal_connect

; RIGHT: First parameter in ecx
mov ecx, signal_id
mov rdx, callback
call masm_signal_connect
```

### ❌ Mistake 3: Not Waiting for Completion
```asm
; WRONG: Queue work and exit immediately
call thread_safe_queue_work
; ... exits without waiting ...

; RIGHT: Wait for completion
call thread_safe_queue_work
mov ecx, 5000
call thread_wait_for_completion
```

### ❌ Mistake 4: Double-Free Memory
```asm
; WRONG: Freeing same buffer twice
call memory_bridge_free
call memory_bridge_free    ; ERROR!

; RIGHT: Free once
call memory_bridge_free
; Don't free again
```

### ❌ Mistake 5: Mixing Handles
```asm
; WRONG: Using file handle as pipe
mov rcx, hFile
mov edx, 2                 ; IO_TYPE_PIPE (WRONG!)
call io_reactor_add

; RIGHT: Match handle type
mov rcx, hPipe
mov edx, 2                 ; IO_TYPE_PIPE
call io_reactor_add
```

---

## Debugging Tips

### Enable Logging
```asm
; Most functions log errors to debug output
; Check debug console for messages like:
; "Signal 1001h connected to callback"
; "Memory allocated: 4096 bytes"
; "I/O event fired"
```

### Verify Signal Delivery
```asm
; Register test handler
lea rdx, debug_handler
mov ecx, TEST_SIGNAL
call masm_signal_connect

; Emit and observe
mov ecx, TEST_SIGNAL
lea rdx, test_param
call masm_signal_emit
; Should see "Signal routed to 1 handler" in debug output
```

### Check Thread Count
```asm
; Inspect g_thread_pool.thread_count
; Should show active thread count after init
mov eax, g_thread_pool.thread_count
; Expected: 2-8 depending on config
```

### Memory Leak Detection
```asm
; Call at shutdown:
call memory_bridge_get_stats
; rax = total_allocated (should be 0 after all frees)
; rcx = block_count (should be 0)
```

---

## Compilation Checklist

Before building:

✅ All .asm files present:
```
□ masm_qt_bridge.asm
□ masm_thread_coordinator.asm
□ masm_memory_bridge.asm
□ masm_io_reactor.asm
□ masm_agent_integration.asm
□ agent_chat_modes.asm
□ masm_terminal_integration.asm
□ ide_components.asm
□ ide_pane_system.asm
□ main_window_masm.asm
```

✅ Build command:
```powershell
ml64 /c masm_*.asm agent_*.asm ide_*.asm main_*.asm
link /OUT:RawrXD-QtShell.exe *.obj Qt6*.lib kernel32.lib user32.lib gdi32.lib
```

✅ Verify no linker errors:
```powershell
# Should see "0 error(s)" at end of link
link ... 2>&1 | Select-String "error"
```

---

**Last Updated**: December 27, 2025  
**Status**: ✅ PRODUCTION READY

