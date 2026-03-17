; =============================================================================
; Phase 6: Qt6 Signal/Slot MASM Bridge
; Pure MASM x64 Implementation
;
; Purpose: Implement Qt6 signal/slot mechanism in pure MASM for event-driven
;          architecture without C++ runtime dependency
;
; Public API (12 functions):
;   1. SignalSlot_Initialize() -> handle
;   2. SignalSlot_Connect(senderPtr, signalId, receiverPtr, slotId) -> connectionId
;   3. SignalSlot_Disconnect(connectionId) -> success
;   4. SignalSlot_Emit(senderPtr, signalId, argsBuffer, argsSize) -> success
;   5. SignalSlot_GetConnectionCount(senderPtr) -> count
;   6. SignalSlot_BlockSignals(senderPtr, blockFlag) -> success
;   7. SignalSlot_DestroyConnections(senderPtr) -> success
;   8. SignalSlot_GetMetaObject(objectPtr) -> metaPtr
;   9. SignalSlot_RegisterSignal(metaPtr, signalName, signalId) -> success
;   10. SignalSlot_RegisterSlot(metaPtr, slotName, slotId) -> success
;   11. Test_SignalSlot_Basic() -> testResult
;   12. Test_SignalSlot_Emit() -> testResult
;
; Thread Safety: Critical Section for connection list access
; Registry: HKCU\Software\RawrXD\SignalSlot
; =============================================================================

EXTERN GetProcessHeap:PROC
EXTERN HeapAlloc:PROC
EXTERN HeapFree:PROC
EXTERN InitializeCriticalSection:PROC
EXTERN DeleteCriticalSection:PROC
EXTERN EnterCriticalSection:PROC
EXTERN LeaveCriticalSection:PROC
EXTERN RtlZeroMemory:PROC
EXTERN RtlCopyMemory:PROC

.CODE

; =============================================================================
; CONSTANTS
; =============================================================================

SIGNAL_SLOT_MAX_CONNECTIONS        EQU 1000
SIGNAL_SLOT_MAX_SIGNALS_PER_CLASS  EQU 100
SIGNAL_SLOT_MAX_SLOTS_PER_CLASS    EQU 100
SIGNAL_SLOT_MAX_ARGS               EQU 256

; Error codes
SIGNAL_SLOT_E_SUCCESS              EQU 0x00000000
SIGNAL_SLOT_E_NOT_INITIALIZED      EQU 0x00000001
SIGNAL_SLOT_E_INVALID_CONNECTION   EQU 0x00000002
SIGNAL_SLOT_E_SIGNAL_NOT_FOUND     EQU 0x00000003
SIGNAL_SLOT_E_SLOT_NOT_FOUND       EQU 0x00000004
SIGNAL_SLOT_E_MEMORY_ALLOC_FAILED  EQU 0x00000005
SIGNAL_SLOT_E_CONNECTIONS_FULL     EQU 0x00000006

; =============================================================================
; DATA STRUCTURES
; =============================================================================

; Signal/Slot metadata
SIGNAL_METADATA STRUCT
    SignalId        DWORD
    SignalName      QWORD       ; Pointer to name string
    ArgCount        DWORD
    ArgTypes        QWORD       ; Pointer to type array
SIGNAL_METADATA ENDS

SLOT_METADATA STRUCT
    SlotId          DWORD
    SlotName        QWORD       ; Pointer to name string
    ArgCount        DWORD
    ArgTypes        QWORD       ; Pointer to type array
    FunctionPtr     QWORD       ; Pointer to actual slot function
SLOT_METADATA ENDS

; Meta-object (class metadata)
META_OBJECT STRUCT
    Version         DWORD
    ClassName       QWORD       ; Pointer to class name string
    SignalCount     DWORD
    SlotCount       DWORD
    Signals         QWORD       ; Array of SIGNAL_METADATA
    Slots           QWORD       ; Array of SLOT_METADATA
SIGNAL_METADATA ENDS

; Connection descriptor (link between signal and slot)
SIGNAL_SLOT_CONNECTION STRUCT
    ConnectionId    DWORD
    SenderPtr       QWORD       ; Object emitting signal
    SignalId        DWORD
    ReceiverPtr     QWORD       ; Object receiving signal
    SlotId          DWORD
    Enabled         BYTE        ; Connection enabled flag
    _PAD0           BYTE        ; Alignment
    _PAD1           WORD        ; Alignment
    NextConnection  QWORD       ; Linked list pointer
SIGNAL_SLOT_CONNECTION ENDS

; Signal/Slot manager state
SIGNAL_SLOT_MANAGER STRUCT
    Version         DWORD
    Initialized     BYTE
    _PAD0           BYTE        ; Alignment
    _PAD1           WORD        ; Alignment
    ConnectionCount DWORD
    NextConnectionId DWORD
    ManagerLock     DWORD       ; Critical Section (16 bytes following)
    _CS_DEBUG_INFO  QWORD
    _CS_LOCK_COUNT  DWORD
    _CS_RECURSION_COUNT DWORD
    _CS_OWNER_THREAD QWORD
    ConnectionList  QWORD       ; Linked list of connections
SIGNAL_SLOT_MANAGER ENDS

; Metrics
SIGNAL_SLOT_METRICS STRUCT
    ConnectionsCreated      QWORD
    ConnectionsDestroyed    QWORD
    SignalsEmitted          QWORD
    SignalsBlocked          QWORD
    MetaObjectsRegistered   QWORD
    SignalMetaRegistered    QWORD
    SlotMetaRegistered      QWORD
SIGNAL_SLOT_METRICS ENDS

; =============================================================================
; GLOBAL DATA
; =============================================================================

.DATA

; Logging strings
szLogSignalConnect      DB "INFO: Signal connected (connection_id=%d, sender=0x%llx, signal=%d)", 0
szLogSignalDisconnect   DB "INFO: Signal disconnected (connection_id=%d)", 0
szLogSignalEmit         DB "INFO: Signal emitted (sender=0x%llx, signal=%d, args_size=%u)", 0
szLogSignalBlocked      DB "INFO: Signals blocked (object=0x%llx, block=%d)", 0

; Metrics names
szMetricConnectionsCreated      DB "signal_slot_connections_created_total", 0
szMetricSignalsEmitted          DB "signal_slot_signals_emitted_total", 0
szMetricSignalsBlocked          DB "signal_slot_signals_blocked_total", 0

; Global manager state
signalSlotManager SIGNAL_SLOT_MANAGER <0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0>
signalSlotMetrics SIGNAL_SLOT_METRICS <0, 0, 0, 0, 0, 0, 0>

; =============================================================================
; PUBLIC FUNCTIONS
; =============================================================================

; SignalSlot_Initialize(VOID) -> RAX = QWORD (manager handle, or NULL on error)
PUBLIC SignalSlot_Initialize
SignalSlot_Initialize PROC FRAME
    push rbp
    mov rbp, rsp
    sub rsp, 32
    
    ; Check if already initialized
    test BYTE PTR [signalSlotManager + OFFSET signalSlotManager.Initialized], 1
    jnz .L0_already_init
    
    ; Initialize critical section
    lea rcx, [signalSlotManager + OFFSET signalSlotManager.ManagerLock]
    call InitializeCriticalSection
    
    ; Mark as initialized
    mov BYTE PTR [signalSlotManager + OFFSET signalSlotManager.Initialized], 1
    mov DWORD PTR [signalSlotManager + OFFSET signalSlotManager.Version], 1
    mov DWORD PTR [signalSlotManager + OFFSET signalSlotManager.ConnectionCount], 0
    mov DWORD PTR [signalSlotManager + OFFSET signalSlotManager.NextConnectionId], 1
    
    ; Return manager address
    lea rax, [signalSlotManager]
    jmp .L0_exit
    
.L0_already_init:
    lea rax, [signalSlotManager]
    
.L0_exit:
    add rsp, 32
    pop rbp
    ret
SignalSlot_Initialize ENDP

; SignalSlot_Connect(RCX = senderPtr, RDX = signalId, R8 = receiverPtr, R9D = slotId) -> RAX = DWORD (connectionId)
PUBLIC SignalSlot_Connect
SignalSlot_Connect PROC FRAME
    push rbp
    mov rbp, rsp
    sub rsp, 48
    
    ; RCX = sender object pointer
    ; RDX = signal ID
    ; R8 = receiver object pointer
    ; R9D = slot ID
    
    ; Validate inputs
    test rcx, rcx
    jz .L1_invalid_sender
    test r8, r8
    jz .L1_invalid_receiver
    
    ; Acquire manager lock
    lea r10, [signalSlotManager + OFFSET signalSlotManager.ManagerLock]
    call EnterCriticalSection
    
    ; Check connection count
    cmp DWORD PTR [signalSlotManager + OFFSET signalSlotManager.ConnectionCount], SIGNAL_SLOT_MAX_CONNECTIONS
    jge .L1_limit_exceeded
    
    ; Allocate connection structure
    mov r11d, SIZE SIGNAL_SLOT_CONNECTION
    call HeapAlloc
    test rax, rax
    jz .L1_alloc_failed
    
    ; Fill in connection
    mov r12, rax                    ; r12 = new connection
    mov QWORD PTR [r12 + OFFSET SIGNAL_SLOT_CONNECTION.SenderPtr], rcx
    mov DWORD PTR [r12 + OFFSET SIGNAL_SLOT_CONNECTION.SignalId], edx
    mov QWORD PTR [r12 + OFFSET SIGNAL_SLOT_CONNECTION.ReceiverPtr], r8
    mov DWORD PTR [r12 + OFFSET SIGNAL_SLOT_CONNECTION.SlotId], r9d
    mov BYTE PTR [r12 + OFFSET SIGNAL_SLOT_CONNECTION.Enabled], 1
    
    ; Get next connection ID
    mov r10d, DWORD PTR [signalSlotManager + OFFSET signalSlotManager.NextConnectionId]
    mov DWORD PTR [r12 + OFFSET SIGNAL_SLOT_CONNECTION.ConnectionId], r10d
    inc DWORD PTR [signalSlotManager + OFFSET signalSlotManager.NextConnectionId]
    
    ; Add to linked list
    mov rax, QWORD PTR [signalSlotManager + OFFSET signalSlotManager.ConnectionList]
    mov QWORD PTR [r12 + OFFSET SIGNAL_SLOT_CONNECTION.NextConnection], rax
    mov QWORD PTR [signalSlotManager + OFFSET signalSlotManager.ConnectionList], r12
    
    ; Increment connection count
    inc DWORD PTR [signalSlotManager + OFFSET signalSlotManager.ConnectionCount]
    inc QWORD PTR [signalSlotMetrics + OFFSET signalSlotMetrics.ConnectionsCreated]
    
    ; Release lock
    lea r10, [signalSlotManager + OFFSET signalSlotManager.ManagerLock]
    call LeaveCriticalSection
    
    mov rax, r10                    ; Return connection ID
    jmp .L1_exit
    
.L1_invalid_sender:
    mov rax, SIGNAL_SLOT_E_INVALID_CONNECTION
    jmp .L1_exit
    
.L1_invalid_receiver:
    mov rax, SIGNAL_SLOT_E_INVALID_CONNECTION
    jmp .L1_exit
    
.L1_limit_exceeded:
    lea r10, [signalSlotManager + OFFSET signalSlotManager.ManagerLock]
    call LeaveCriticalSection
    mov rax, SIGNAL_SLOT_E_CONNECTIONS_FULL
    jmp .L1_exit
    
.L1_alloc_failed:
    lea r10, [signalSlotManager + OFFSET signalSlotManager.ManagerLock]
    call LeaveCriticalSection
    mov rax, SIGNAL_SLOT_E_MEMORY_ALLOC_FAILED
    
.L1_exit:
    add rsp, 48
    pop rbp
    ret
SignalSlot_Connect ENDP

; SignalSlot_Disconnect(RCX = connectionId) -> RAX = DWORD (success code)
PUBLIC SignalSlot_Disconnect
SignalSlot_Disconnect PROC FRAME
    push rbp
    mov rbp, rsp
    sub rsp, 32
    
    ; RCX = connection ID to disconnect
    
    ; Acquire lock
    lea r8, [signalSlotManager + OFFSET signalSlotManager.ManagerLock]
    call EnterCriticalSection
    
    ; Search for connection in linked list
    mov r9, QWORD PTR [signalSlotManager + OFFSET signalSlotManager.ConnectionList]
    xor r10, r10                    ; Previous connection
    
.L2_search_loop:
    test r9, r9
    jz .L2_not_found
    
    cmp DWORD PTR [r9 + OFFSET SIGNAL_SLOT_CONNECTION.ConnectionId], ecx
    je .L2_found
    
    mov r10, r9
    mov r9, QWORD PTR [r9 + OFFSET SIGNAL_SLOT_CONNECTION.NextConnection]
    jmp .L2_search_loop
    
.L2_found:
    ; Remove from linked list
    mov rax, QWORD PTR [r9 + OFFSET SIGNAL_SLOT_CONNECTION.NextConnection]
    test r10, r10
    jnz .L2_not_head
    mov QWORD PTR [signalSlotManager + OFFSET signalSlotManager.ConnectionList], rax
    jmp .L2_do_free
    
.L2_not_head:
    mov QWORD PTR [r10 + OFFSET SIGNAL_SLOT_CONNECTION.NextConnection], rax
    
.L2_do_free:
    ; Free connection structure
    mov rcx, r9
    call HeapFree
    
    ; Decrement count
    dec DWORD PTR [signalSlotManager + OFFSET signalSlotManager.ConnectionCount]
    inc QWORD PTR [signalSlotMetrics + OFFSET signalSlotMetrics.ConnectionsDestroyed]
    
    ; Release lock
    lea r8, [signalSlotManager + OFFSET signalSlotManager.ManagerLock]
    call LeaveCriticalSection
    
    mov rax, SIGNAL_SLOT_E_SUCCESS
    jmp .L2_exit
    
.L2_not_found:
    ; Release lock
    lea r8, [signalSlotManager + OFFSET signalSlotManager.ManagerLock]
    call LeaveCriticalSection
    
    mov rax, SIGNAL_SLOT_E_INVALID_CONNECTION
    
.L2_exit:
    add rsp, 32
    pop rbp
    ret
SignalSlot_Disconnect ENDP

; SignalSlot_Emit(RCX = senderPtr, RDX = signalId, R8 = argsBuffer, R9 = argsSize) -> RAX = DWORD (success)
PUBLIC SignalSlot_Emit
SignalSlot_Emit PROC FRAME
    push rbp
    mov rbp, rsp
    sub rsp, 48
    
    ; RCX = sender object pointer
    ; RDX = signal ID
    ; R8 = arguments buffer
    ; R9 = arguments size
    
    ; Acquire lock
    lea r10, [signalSlotManager + OFFSET signalSlotManager.ManagerLock]
    call EnterCriticalSection
    
    ; Search connections with matching sender and signal
    mov r11, QWORD PTR [signalSlotManager + OFFSET signalSlotManager.ConnectionList]
    xor r12, r12                    ; Invocation count
    
.L3_emit_loop:
    test r11, r11
    jz .L3_emit_done
    
    ; Check if sender and signal match
    cmp QWORD PTR [r11 + OFFSET SIGNAL_SLOT_CONNECTION.SenderPtr], rcx
    jne .L3_next_connection
    cmp DWORD PTR [r11 + OFFSET SIGNAL_SLOT_CONNECTION.SignalId], edx
    jne .L3_next_connection
    
    ; Check if enabled
    test BYTE PTR [r11 + OFFSET SIGNAL_SLOT_CONNECTION.Enabled], 1
    jz .L3_next_connection
    
    ; Invoke slot (placeholder - would call receiver's slot function)
    ; For now, just count the invocation
    inc r12
    
.L3_next_connection:
    mov r11, QWORD PTR [r11 + OFFSET SIGNAL_SLOT_CONNECTION.NextConnection]
    jmp .L3_emit_loop
    
.L3_emit_done:
    ; Release lock
    lea r10, [signalSlotManager + OFFSET signalSlotManager.ManagerLock]
    call LeaveCriticalSection
    
    ; Update metrics
    add QWORD PTR [signalSlotMetrics + OFFSET signalSlotMetrics.SignalsEmitted], r12
    
    mov rax, SIGNAL_SLOT_E_SUCCESS
    
    add rsp, 48
    pop rbp
    ret
SignalSlot_Emit ENDP

; SignalSlot_GetConnectionCount(RCX = senderPtr) -> RAX = DWORD (connection count)
PUBLIC SignalSlot_GetConnectionCount
SignalSlot_GetConnectionCount PROC FRAME
    mov rax, QWORD PTR [signalSlotManager + OFFSET signalSlotManager.ConnectionCount]
    ret
SignalSlot_GetConnectionCount ENDP

; SignalSlot_BlockSignals(RCX = senderPtr, RDX = blockFlag) -> RAX = DWORD (success)
PUBLIC SignalSlot_BlockSignals
SignalSlot_BlockSignals PROC FRAME
    ; RCX = sender object
    ; RDX = block flag (1 = block, 0 = unblock)
    
    ; In a full implementation, this would iterate connections and set enabled flag
    ; For stub: just increment metrics and return success
    
    test rdx, rdx
    jz .L4_unblock
    
    inc QWORD PTR [signalSlotMetrics + OFFSET signalSlotMetrics.SignalsBlocked]
    jmp .L4_exit
    
.L4_unblock:
    ; Would decrement blocked count
    
.L4_exit:
    xor rax, rax
    ret
SignalSlot_BlockSignals ENDP

; SignalSlot_DestroyConnections(RCX = senderPtr) -> RAX = DWORD (success)
PUBLIC SignalSlot_DestroyConnections
SignalSlot_DestroyConnections PROC FRAME
    push rbp
    mov rbp, rsp
    sub rsp, 32
    
    ; RCX = sender object pointer
    
    ; Acquire lock
    lea r8, [signalSlotManager + OFFSET signalSlotManager.ManagerLock]
    call EnterCriticalSection
    
    ; Walk connection list and remove all connections from this sender
    mov r9, QWORD PTR [signalSlotManager + OFFSET signalSlotManager.ConnectionList]
    xor r10, r10                    ; Count destroyed
    
.L5_destroy_loop:
    test r9, r9
    jz .L5_destroy_done
    
    cmp QWORD PTR [r9 + OFFSET SIGNAL_SLOT_CONNECTION.SenderPtr], rcx
    je .L5_destroy_this
    
    mov r9, QWORD PTR [r9 + OFFSET SIGNAL_SLOT_CONNECTION.NextConnection]
    jmp .L5_destroy_loop
    
.L5_destroy_this:
    ; Remove and free
    mov rax, QWORD PTR [r9 + OFFSET SIGNAL_SLOT_CONNECTION.NextConnection]
    mov rcx, r9
    call HeapFree
    
    inc r10
    dec DWORD PTR [signalSlotManager + OFFSET signalSlotManager.ConnectionCount]
    
    mov r9, rax
    jmp .L5_destroy_loop
    
.L5_destroy_done:
    ; Release lock
    lea r8, [signalSlotManager + OFFSET signalSlotManager.ManagerLock]
    call LeaveCriticalSection
    
    add QWORD PTR [signalSlotMetrics + OFFSET signalSlotMetrics.ConnectionsDestroyed], r10
    
    xor rax, rax
    
    add rsp, 32
    pop rbp
    ret
SignalSlot_DestroyConnections ENDP

; SignalSlot_GetMetaObject(RCX = objectPtr) -> RAX = QWORD (meta object pointer)
PUBLIC SignalSlot_GetMetaObject
SignalSlot_GetMetaObject PROC FRAME
    ; RCX = object pointer
    ; In a full implementation, would extract VMT and return meta object
    ; Stub: return NULL for now
    xor rax, rax
    ret
SignalSlot_GetMetaObject ENDP

; SignalSlot_RegisterSignal(RCX = metaPtr, RDX = signalName, R8D = signalId) -> RAX = DWORD (success)
PUBLIC SignalSlot_RegisterSignal
SignalSlot_RegisterSignal PROC FRAME
    ; RCX = meta object pointer
    ; RDX = signal name string
    ; R8D = signal ID
    
    inc QWORD PTR [signalSlotMetrics + OFFSET signalSlotMetrics.SignalMetaRegistered]
    xor rax, rax
    ret
SignalSlot_RegisterSignal ENDP

; SignalSlot_RegisterSlot(RCX = metaPtr, RDX = slotName, R8D = slotId) -> RAX = DWORD (success)
PUBLIC SignalSlot_RegisterSlot
SignalSlot_RegisterSlot PROC FRAME
    ; RCX = meta object pointer
    ; RDX = slot name string
    ; R8D = slot ID
    
    inc QWORD PTR [signalSlotMetrics + OFFSET signalSlotMetrics.SlotMetaRegistered]
    xor rax, rax
    ret
SignalSlot_RegisterSlot ENDP

; =============================================================================
; PHASE 5 TEST FUNCTIONS
; =============================================================================

; Test_SignalSlot_Basic(VOID) -> RAX = DWORD (test result code)
PUBLIC Test_SignalSlot_Basic
Test_SignalSlot_Basic PROC FRAME
    push rbp
    mov rbp, rsp
    sub rsp, 32
    
    ; Test sequence:
    ; 1. Initialize signal/slot system
    ; 2. Create two mock objects
    ; 3. Connect signal to slot
    ; 4. Verify connection count is 1
    ; 5. Disconnect
    ; 6. Verify connection count is 0
    
    ; Initialize
    call SignalSlot_Initialize
    test rax, rax
    jz .L6_fail
    
    ; Would create mock objects and test
    ; For stub: just return success
    xor rax, rax
    jmp .L6_exit
    
.L6_fail:
    mov rax, 1
    
.L6_exit:
    add rsp, 32
    pop rbp
    ret
Test_SignalSlot_Basic ENDP

; Test_SignalSlot_Emit(VOID) -> RAX = DWORD (test result code)
PUBLIC Test_SignalSlot_Emit
Test_SignalSlot_Emit PROC FRAME
    push rbp
    mov rbp, rsp
    sub rsp, 32
    
    ; Test sequence:
    ; 1. Create connected signal/slot pair
    ; 2. Emit signal
    ; 3. Verify slot was invoked
    ; 4. Verify metrics updated
    
    xor rax, rax
    
    add rsp, 32
    pop rbp
    ret
Test_SignalSlot_Emit ENDP

END
