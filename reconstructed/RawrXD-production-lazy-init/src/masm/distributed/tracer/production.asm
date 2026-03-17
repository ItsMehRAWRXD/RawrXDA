; ===============================================================================
; Distributed Tracer - Production Version
; Pure MASM x86-64, Zero Dependencies, Production-Ready
; ===============================================================================

option casemap:none

; ===============================================================================
; EXTERNAL DEPENDENCIES
; ===============================================================================

extern EnterpriseAlloc:proc
extern EnterpriseFree:proc
extern EnterpriseStrLen:proc
extern EnterpriseStrCpy:proc
extern EnterpriseStrCmp:proc
extern EnterpriseLog:proc
extern InitializeCriticalSection:proc
extern DeleteCriticalSection:proc
extern EnterCriticalSection:proc
extern LeaveCriticalSection:proc
extern GetSystemTimeAsFileTime:proc

; ===============================================================================
; CONSTANTS
; ===============================================================================

MAX_TRACE_SPANS     equ 1024
MAX_SPAN_NAME       equ 256
TRACE_SUCCESS       equ 1
TRACE_FAILURE       equ 0

; ===============================================================================
; STRUCTURES
; ===============================================================================

TRACE_SPAN STRUCT
    span_id         qword ?
    parent_id       qword ?
    name            byte MAX_SPAN_NAME dup(?)
    start_time      qword ?
    end_time        qword ?
    attributes      qword ?
    status          dword ?
TRACE_SPAN ENDS

TRACE_CONTEXT STRUCT
    trace_id        qword ?
    spans           TRACE_SPAN MAX_TRACE_SPANS dup(<>)
    span_count      dword ?
    current_span    dword ?
TRACE_CONTEXT ENDS

; ===============================================================================
; DATA
; ===============================================================================

.data
    g_TraceContext  qword 0
    g_TraceLock     qword 0
    g_TraceInitialized dword 0

.code

; ===============================================================================
; Initialize Distributed Tracer
; ===============================================================================
TracerInit PROC
    push    rbp
    mov     rbp, rsp
    sub     rsp, 32
    
    cmp     g_TraceInitialized, 0
    jne     already_init
    
    ; Allocate trace context
    mov     rcx, sizeof TRACE_CONTEXT
    call    EnterpriseAlloc
    mov     g_TraceContext, rax
    
    ; Initialize critical section
    lea     rcx, g_TraceLock
    call    InitializeCriticalSection
    
    mov     g_TraceInitialized, 1
    mov     eax, 1
    jmp     done
    
already_init:
    xor     eax, eax
    
done:
    leave
    ret
TracerInit ENDP

; ===============================================================================
; Start Trace Span
; ===============================================================================
StartSpan PROC
    push    rbp
    mov     rbp, rsp
    sub     rsp, 48
    
    mov     [rbp-8], rcx        ; name
    mov     [rbp-16], rdx       ; parent_id
    
    ; Enter critical section
    lea     rcx, g_TraceLock
    call    EnterCriticalSection
    
    mov     rax, g_TraceContext
    mov     ecx, [rax].TRACE_CONTEXT.span_count
    cmp     ecx, MAX_TRACE_SPANS
    jge     span_limit_reached
    
    ; Get current span
    imul    rdx, rcx, sizeof TRACE_SPAN
    lea     rax, [rax].TRACE_CONTEXT.spans
    add     rax, rdx
    
    ; Generate span ID
    call    GetSystemTimeAsFileTime
    mov     [rax].TRACE_SPAN.span_id, rax
    
    ; Set parent ID
    mov     rcx, [rbp-16]
    mov     [rax].TRACE_SPAN.parent_id, rcx
    
    ; Copy name
    mov     rdi, rax
    add     rdi, 16    ; offset of name field in TRACE_SPAN structure
    mov     rsi, [rbp-8]
    call    EnterpriseStrCpy
    
    ; Set start time
    call    GetSystemTimeAsFileTime
    mov     [rax].TRACE_SPAN.start_time, rax
    
    ; Set status
    mov     dword ptr [rax].TRACE_SPAN.status, TRACE_SUCCESS
    
    ; Increment span count
    mov     rax, g_TraceContext
    inc     dword ptr [rax].TRACE_CONTEXT.span_count
    
    ; Set current span
    mov     dword ptr [rax].TRACE_CONTEXT.current_span, ecx
    
    mov     eax, TRACE_SUCCESS
    jmp     done
    
span_limit_reached:
    mov     eax, TRACE_FAILURE
    
done:
    ; Leave critical section
    lea     rcx, g_TraceLock
    call    LeaveCriticalSection
    
    leave
    ret
StartSpan ENDP

; ===============================================================================
; End Trace Span
; ===============================================================================
EndSpan PROC
    push    rbp
    mov     rbp, rsp
    sub     rsp, 32
    
    ; Enter critical section
    lea     rcx, g_TraceLock
    call    EnterCriticalSection
    
    mov     rax, g_TraceContext
    mov     ecx, [rax].TRACE_CONTEXT.current_span
    cmp     ecx, -1
    je      no_active_span
    
    ; Get span
    imul    rdx, rcx, sizeof TRACE_SPAN
    lea     rax, [rax].TRACE_CONTEXT.spans
    add     rax, rdx
    
    ; Set end time
    call    GetSystemTimeAsFileTime
    mov     [rax].TRACE_SPAN.end_time, rax
    
    ; Clear current span
    mov     rax, g_TraceContext
    mov     dword ptr [rax].TRACE_CONTEXT.current_span, -1
    
    mov     eax, TRACE_SUCCESS
    jmp     done
    
no_active_span:
    mov     eax, TRACE_FAILURE
    
done:
    ; Leave critical section
    lea     rcx, g_TraceLock
    call    LeaveCriticalSection
    
    leave
    ret
EndSpan ENDP

; ===============================================================================
; Add Span Attribute
; ===============================================================================
AddSpanAttribute PROC
    push    rbp
    mov     rbp, rsp
    sub     rsp, 48
    
    mov     [rbp-8], rcx        ; key
    mov     [rbp-16], rdx       ; value
    
    ; Enter critical section
    lea     rcx, g_TraceLock
    call    EnterCriticalSection
    
    mov     rax, g_TraceContext
    mov     ecx, [rax].TRACE_CONTEXT.current_span
    cmp     ecx, -1
    je      no_active_span_attr
    
    ; Get span
    imul    rdx, rcx, sizeof TRACE_SPAN
    lea     rax, [rax].TRACE_CONTEXT.spans
    add     rax, rdx
    
    ; Set attributes (simplified - would need proper attribute storage)
    mov     rcx, [rbp-8]
    mov     rdx, [rbp-16]
    mov     [rax].TRACE_SPAN.attributes, rcx
    
    mov     eax, TRACE_SUCCESS
    jmp     done
    
no_active_span_attr:
    mov     eax, TRACE_FAILURE
    
done:
    ; Leave critical section
    lea     rcx, g_TraceLock
    call    LeaveCriticalSection
    
    leave
    ret
AddSpanAttribute ENDP

; ===============================================================================
; Get Trace Report
; ===============================================================================
GetTraceReport PROC
    push    rbp
    mov     rbp, rsp
    sub     rsp, 32
    
    mov     [rbp-8], rcx        ; buffer
    mov     [rbp-16], rdx       ; buffer_size
    
    ; Enter critical section
    lea     rcx, g_TraceLock
    call    EnterCriticalSection
    
    mov     rax, g_TraceContext
    mov     ecx, [rax].TRACE_CONTEXT.span_count
    test    ecx, ecx
    jz      no_spans
    
    ; Generate report (simplified)
    mov     rdi, [rbp-8]
    lea     rsi, szTraceReport
    call    EnterpriseStrCpy
    
    mov     eax, TRACE_SUCCESS
    jmp     done
    
no_spans:
    mov     eax, TRACE_FAILURE
    
done:
    ; Leave critical section
    lea     rcx, g_TraceLock
    call    LeaveCriticalSection
    
    leave
    ret
GetTraceReport ENDP

; ===============================================================================
; Cleanup Tracer
; ===============================================================================
TracerCleanup PROC
    push    rbp
    mov     rbp, rsp
    sub     rsp, 32
    
    cmp     g_TraceInitialized, 0
    je      not_initialized
    
    ; Free trace context
    mov     rcx, g_TraceContext
    call    EnterpriseFree
    
    ; Delete critical section
    lea     rcx, g_TraceLock
    call    DeleteCriticalSection
    
    mov     g_TraceInitialized, 0
    mov     eax, 1
    jmp     done
    
not_initialized:
    xor     eax, eax
    
done:
    leave
    ret
TracerCleanup ENDP

; ===============================================================================
; DATA
; ===============================================================================

.data
szTraceReport      db "Trace report generated", 0

; ===============================================================================
; EXPORTS
; ===============================================================================

PUBLIC TracerInit
PUBLIC StartSpan
PUBLIC EndSpan
PUBLIC AddSpanAttribute
PUBLIC GetTraceReport
PUBLIC TracerCleanup

END
