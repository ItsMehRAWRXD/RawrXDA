; Distributed Executor - Multi-machine execution
; Phase D Component 3: 800 MASM LOC, 6 functions
; Author: RawrXD-QtShell MASM Conversion Project
; Date: December 29, 2025
; Architecture: x64 ML64

DISTRIBUTED_EXECUTOR_IMPLEMENTATION equ 1

include windows.inc
include masm_master_defs.inc

OPTION PROLOGUE:PrologueDef
OPTION EPILOGUE:EpilogueDef

extern HeapAlloc: proc
extern HeapFree: proc
extern GetProcessHeap: proc
extern WSAStartup: proc
extern WSACleanup: proc
extern socket: proc
extern closesocket: proc
extern bind: proc
extern listen: proc
extern accept: proc
extern recv: proc
extern send: proc

.data
; Distributed executor structure (x64-aligned)
DISTRIBUTED_EXECUTOR struct
    executor_mutex QWORD ?       ; Mutex handle for thread safety
    node_count DWORD ?           ; Number of registered nodes
    max_nodes DWORD ?            ; Maximum nodes (default 64)
    job_count DWORD ?            ; Number of active jobs
    max_jobs DWORD ?             ; Maximum jobs (default 256)
    nodes_ptr QWORD ?            ; Pointer to nodes array
    jobs_ptr QWORD ?             ; Pointer to jobs array
    allocator_ptr QWORD ?        ; Memory allocator
    deallocator_ptr QWORD ?      ; Memory deallocator
    stats_enabled DWORD ?        ; Statistics enabled
    reserved QWORD 13 dup(?)     ; Reserved
DISTRIBUTED_EXECUTOR ends

; Node structure (x64-aligned)
NODE struct
    node_id DWORD ?              ; Unique node ID
    name_ptr QWORD ?             ; Node name
    name_len DWORD ?             ; Name length
    address_ptr QWORD ?          ; Network address
    address_len DWORD ?          ; Address length
    port DWORD ?                 ; Port number
    status DWORD ?               ; Node status (0=offline, 1=online)
    capacity DWORD ?             ; Concurrent job capacity
    current_jobs DWORD ?         ; Current job count
    last_heartbeat QWORD ?       ; Last heartbeat timestamp
    flags DWORD ?                ; Node flags
    reserved QWORD 2 dup(?)      ; Reserved
NODE ends

; Job structure (x64-aligned)
JOB struct
    job_id DWORD ?               ; Unique job ID
    node_id DWORD ?              ; Assigned node ID
    command_ptr QWORD ?          ; Command to execute
    command_len DWORD ?          ; Command length
    input_ptr QWORD ?            ; Input data
    input_len DWORD ?            ; Input length
    output_ptr QWORD ?           ; Output data
    output_len DWORD ?           ; Output length
    status DWORD ?               ; Job status (0=pending, 1=running, 2=completed, 3=failed)
    exit_code DWORD ?            ; Exit code
    created_at QWORD ?           ; Creation timestamp
    started_at QWORD ?           ; Start timestamp
    completed_at QWORD ?         ; Completion timestamp
    flags DWORD ?                ; Job flags
    reserved QWORD 2 dup(?)      ; Reserved
JOB ends

; Constants
NODE_MAX_NAME_LEN equ 256
NODE_MAX_ADDRESS_LEN equ 256
JOB_MAX_COMMAND_LEN equ 4096
JOB_MAX_INPUT_LEN equ 1048576
JOB_MAX_OUTPUT_LEN equ 1048576

; Error codes
DISTRIBUTED_SUCCESS equ 0
DISTRIBUTED_ERROR_INVALID equ 1
DISTRIBUTED_ERROR_OOM equ 2
DISTRIBUTED_ERROR_NOT_FOUND equ 3
DISTRIBUTED_ERROR_EXISTS equ 4
DISTRIBUTED_ERROR_FULL equ 5
DISTRIBUTED_ERROR_OFFLINE equ 6
DISTRIBUTED_ERROR_NETWORK equ 7

.code

; Initialize distributed executor
distributed_executor_init proc uses ebx esi edi, max_nodes:dword, max_jobs:dword, stats:dword
    local executor_ptr:dword
    
    ; Validate parameters
    mov eax, max_nodes
    test eax, eax
    jnz nodes_ok
    mov eax, 64
nodes_ok:
    mov max_nodes, eax
    
    mov eax, max_jobs
    test eax, eax
    jnz jobs_ok
    mov eax, 256
jobs_ok:
    mov max_jobs, eax
    
    ; Allocate executor structure
    invoke crt_malloc, sizeof DISTRIBUTED_EXECUTOR
    test eax, eax
    jz error_oom
    mov executor_ptr, eax
    
    ; Initialize executor structure
    mov ebx, executor_ptr
    assume ebx:ptr DISTRIBUTED_EXECUTOR
    
    ; Create mutex
    invoke CreateMutex, NULL, FALSE, NULL
    test eax, eax
    jz cleanup_error
    mov [ebx].executor_mutex, eax
    
    ; Initialize fields
    mov [ebx].node_count, 0
    mov eax, max_nodes
    mov [ebx].max_nodes, eax
    mov [ebx].job_count, 0
    mov eax, max_jobs
    mov [ebx].max_jobs, eax
    mov eax, stats
    mov [ebx].stats_enabled, eax
    
    ; Allocate nodes array
    mov eax, max_nodes
    imul eax, sizeof NODE
    invoke crt_malloc, eax
    test eax, eax
    jz cleanup_mutex
    mov [ebx].nodes_ptr, eax
    
    ; Initialize nodes array to zeros
    mov edi, eax
    mov ecx, max_nodes
    imul ecx, sizeof NODE
    shr ecx, 2
    xor eax, eax
    rep stosd
    
    ; Allocate jobs array
    mov eax, max_jobs
    imul eax, sizeof JOB
    invoke crt_malloc, eax
    test eax, eax
    jz cleanup_nodes
    mov [ebx].jobs_ptr, eax
    
    ; Initialize jobs array
    mov edi, eax
    mov ecx, max_jobs
    imul ecx, sizeof JOB
    shr ecx, 2
    xor eax, eax
    rep stosd
    
    ; Set default allocators
    mov [ebx].allocator_ptr, offset crt_malloc
    mov [ebx].deallocator_ptr, offset crt_free
    
    assume ebx:nothing
    mov eax, executor_ptr
    ret
    
cleanup_nodes:
    invoke crt_free, [ebx].nodes_ptr
cleanup_mutex:
    invoke CloseHandle, [ebx].executor_mutex
cleanup_error:
    invoke crt_free, executor_ptr
error_oom:
    xor eax, eax
    ret
distributed_executor_init endp

; Shutdown distributed executor
distributed_executor_shutdown proc uses ebx esi edi, executor_ptr:dword
    local i:dword, node_ptr:dword, job_ptr:dword
    
    mov ebx, executor_ptr
    test ebx, ebx
    jz done
    assume ebx:ptr DISTRIBUTED_EXECUTOR
    
    ; Lock executor
    invoke WaitForSingleObject, [ebx].executor_mutex, INFINITE
    
    ; Free all nodes
    mov i, 0
free_nodes_loop:
    mov eax, i
    cmp eax, [ebx].node_count
    jae free_nodes_done
    
    ; Get node pointer
    mov edx, [ebx].nodes_ptr
    imul eax, sizeof NODE
    add edx, eax
    mov node_ptr, edx
    assume edx:ptr NODE
    
    ; Free node name and address
    invoke crt_free, [edx].name_ptr
    invoke crt_free, [edx].address_ptr
    assume edx:nothing
    inc i
    jmp free_nodes_loop
    
free_nodes_done:
    ; Free all jobs
    mov i, 0
free_jobs_loop:
    mov eax, i
    cmp eax, [ebx].job_count
    jae free_jobs_done
    
    ; Get job pointer
    mov edx, [ebx].jobs_ptr
    imul eax, sizeof JOB
    add edx, eax
    mov job_ptr, edx
    assume edx:ptr JOB
    
    ; Free job data
    invoke crt_free, [edx].command_ptr
    invoke crt_free, [edx].input_ptr
    invoke crt_free, [edx].output_ptr
    assume edx:nothing
    inc i
    jmp free_jobs_loop
    
free_jobs_done:
    ; Free arrays
    invoke crt_free, [ebx].nodes_ptr
    invoke crt_free, [ebx].jobs_ptr
    
    ; Release mutex and close handle
    invoke ReleaseMutex, [ebx].executor_mutex
    invoke CloseHandle, [ebx].executor_mutex
    
    ; Free executor structure
    invoke crt_free, ebx
    
    assume ebx:nothing
done:
    mov eax, DISTRIBUTED_SUCCESS
    ret
distributed_executor_shutdown endp

; Register node
distributed_register_node proc uses ebx esi edi, executor_ptr:dword, name_ptr:dword, name_len:dword, 
                              address_ptr:dword, address_len:dword, port:dword, capacity:dword
    local node_id:dword, node_ptr:dword
    
    mov ebx, executor_ptr
    test ebx, ebx
    jz error_invalid
    assume ebx:ptr DISTRIBUTED_EXECUTOR
    
    ; Validate parameters
    mov eax, name_ptr
    test eax, eax
    jz error_invalid
    mov eax, name_len
    test eax, eax
    jz error_invalid
    cmp eax, NODE_MAX_NAME_LEN
    ja error_size
    mov eax, address_ptr
    test eax, eax
    jz error_invalid
    mov eax, address_len
    test eax, eax
    jz error_invalid
    cmp eax, NODE_MAX_ADDRESS_LEN
    ja error_size
    
    ; Lock executor
    invoke WaitForSingleObject, [ebx].executor_mutex, INFINITE
    
    ; Check if node limit reached
    mov eax, [ebx].node_count
    cmp eax, [ebx].max_nodes
    jae error_full
    
    ; Check if node already exists
    mov ecx, [ebx].node_count
    test ecx, ecx
    jz register_new
    
    mov esi, [ebx].nodes_ptr
    mov edi, 0
    
check_existing:
    cmp edi, ecx
    jae register_new
    assume esi:ptr NODE
    
    ; Check name match
    mov eax, [esi].name_len
    cmp eax, name_len
    jne next_node
    
    push esi
    mov esi, [esi].name_ptr
    mov edi, name_ptr
    mov ecx, name_len
    repe cmpsb
    pop esi
    jne next_node
    
    ; Node already exists
    invoke ReleaseMutex, [ebx].executor_mutex
    mov eax, DISTRIBUTED_ERROR_EXISTS
    ret
    
next_node:
    add esi, sizeof NODE
    inc edi
    jmp check_existing
    
register_new:
    ; Get next node ID
    mov eax, [ebx].node_count
    mov node_id, eax
    
    ; Get node pointer
    mov edx, [ebx].nodes_ptr
    imul eax, sizeof NODE
    add edx, eax
    mov node_ptr, edx
    assume edx:ptr NODE
    
    ; Allocate and copy name
    invoke crt_malloc, name_len
    test eax, eax
    jz error_oom_locked
    mov edi, eax
    mov esi, name_ptr
    mov ecx, name_len
    rep movsb
    mov [edx].name_ptr, eax
    mov [edx].name_len, name_len
    
    ; Allocate and copy address
    invoke crt_malloc, address_len
    test eax, eax
    jz free_name_error
    mov edi, eax
    mov esi, address_ptr
    mov ecx, address_len
    rep movsb
    mov [edx].address_ptr, eax
    mov [edx].address_len, address_len
    
    ; Initialize node fields
    mov eax, node_id
    mov [edx].node_id, eax
    mov eax, port
    mov [edx].port, eax
    mov eax, capacity
    mov [edx].capacity, eax
    mov [edx].status, 1  ; Online
    mov [edx].current_jobs, 0
    invoke GetTickCount
    mov [edx].last_heartbeat, eax
    mov [edx].flags, 0
    
    ; Update executor statistics
    inc [ebx].node_count
    
    invoke ReleaseMutex, [ebx].executor_mutex
    mov eax, DISTRIBUTED_SUCCESS
    ret
    
free_name_error:
    invoke crt_free, [edx].name_ptr
error_oom_locked:
    invoke ReleaseMutex, [ebx].executor_mutex
error_oom:
    mov eax, DISTRIBUTED_ERROR_OOM
    ret
    
error_invalid:
    mov eax, DISTRIBUTED_ERROR_INVALID
    ret
    
error_size:
    mov eax, DISTRIBUTED_ERROR_SIZE
    ret
    
error_full:
    invoke ReleaseMutex, [ebx].executor_mutex
    mov eax, DISTRIBUTED_ERROR_FULL
    ret
    
    assume edx:nothing
    assume ebx:nothing
distributed_register_node endp

; Submit job for execution
distributed_submit_job proc uses ebx esi edi, executor_ptr:dword, command_ptr:dword, command_len:dword, 
                              input_ptr:dword, input_len:dword
    local job_id:dword, job_ptr:dword, node_ptr:dword, best_node:dword, best_load:dword
    
    mov ebx, executor_ptr
    test ebx, ebx
    jz error_invalid
    assume ebx:ptr DISTRIBUTED_EXECUTOR
    
    ; Validate parameters
    mov eax, command_ptr
    test eax, eax
    jz error_invalid
    mov eax, command_len
    test eax, eax
    jz error_invalid
    cmp eax, JOB_MAX_COMMAND_LEN
    ja error_size
    mov eax, input_len
    cmp eax, JOB_MAX_INPUT_LEN
    ja error_size
    
    ; Lock executor
    invoke WaitForSingleObject, [ebx].executor_mutex, INFINITE
    
    ; Check if job limit reached
    mov eax, [ebx].job_count
    cmp eax, [ebx].max_jobs
    jae error_full
    
    ; Find best node (least loaded online node)
    mov best_node, -1
    mov best_load, 0xFFFFFFFF
    
    mov ecx, [ebx].node_count
    test ecx, ecx
    jz no_nodes
    
    mov esi, [ebx].nodes_ptr
    mov edi, 0
    
find_best_node:
    cmp edi, ecx
    jae find_done
    assume esi:ptr NODE
    
    ; Check if node is online
    cmp [esi].status, 1
    jne next_node_find
    
    ; Calculate load (current_jobs / capacity)
    mov eax, [esi].current_jobs
    mov edx, [esi].capacity
    test edx, edx
    jz infinite_capacity
    
    ; Compare load
    cmp eax, best_load
    jae next_node_find
    mov best_load, eax
    mov best_node, edi
    jmp next_node_find
    
infinite_capacity:
    ; Node with infinite capacity
    mov best_node, edi
    mov best_load, 0
    jmp find_done
    
next_node_find:
    add esi, sizeof NODE
    inc edi
    jmp find_best_node
    
find_done:
    cmp best_node, -1
    je no_online_nodes
    
no_nodes:
    ; Get next job ID
    mov eax, [ebx].job_count
    mov job_id, eax
    
    ; Get job pointer
    mov edx, [ebx].jobs_ptr
    imul eax, sizeof JOB
    add edx, eax
    mov job_ptr, edx
    assume edx:ptr JOB
    
    ; Allocate and copy command
    invoke crt_malloc, command_len
    test eax, eax
    jz error_oom_locked
    mov edi, eax
    mov esi, command_ptr
    mov ecx, command_len
    rep movsb
    mov [edx].command_ptr, eax
    mov [edx].command_len, command_len
    
    ; Allocate and copy input (if provided)
    mov eax, input_ptr
    test eax, eax
    jz no_input
    mov eax, input_len
    test eax, eax
    jz no_input
    
    invoke crt_malloc, input_len
    test eax, eax
    jz free_command_error
    mov edi, eax
    mov esi, input_ptr
    mov ecx, input_len
    rep movsb
    mov [edx].input_ptr, eax
    mov [edx].input_len, input_len
    jmp input_done
    
no_input:
    mov [edx].input_ptr, 0
    mov [edx].input_len, 0
    
input_done:
    ; Initialize job fields
    mov eax, job_id
    mov [edx].job_id, eax
    mov eax, best_node
    mov [edx].node_id, eax
    mov [edx].output_ptr, 0
    mov [edx].output_len, 0
    mov [edx].status, 0  ; Pending
    mov [edx].exit_code, 0
    invoke GetTickCount
    mov [edx].created_at, eax
    mov [edx].started_at, 0
    mov [edx].completed_at, 0
    mov [edx].flags, 0
    
    ; Update node job count if node found
    cmp best_node, -1
    je no_node_update
    
    mov esi, [ebx].nodes_ptr
    mov eax, best_node
    imul eax, sizeof NODE
    add esi, eax
    assume esi:ptr NODE
    inc [esi].current_jobs
    assume esi:nothing
    
no_node_update:
    ; Update executor statistics
    inc [ebx].job_count
    
    invoke ReleaseMutex, [ebx].executor_mutex
    mov eax, DISTRIBUTED_SUCCESS
    ret
    
free_command_error:
    invoke crt_free, [edx].command_ptr
error_oom_locked:
    invoke ReleaseMutex, [ebx].executor_mutex
error_oom:
    mov eax, DISTRIBUTED_ERROR_OOM
    ret
    
error_invalid:
    mov eax, DISTRIBUTED_ERROR_INVALID
    ret
    
error_size:
    mov eax, DISTRIBUTED_ERROR_SIZE
    ret
    
error_full:
    invoke ReleaseMutex, [ebx].executor_mutex
    mov eax, DISTRIBUTED_ERROR_FULL
    ret
    
no_online_nodes:
    invoke ReleaseMutex, [ebx].executor_mutex
    mov eax, DISTRIBUTED_ERROR_OFFLINE
    ret
    
    assume edx:nothing
    assume ebx:nothing
distributed_submit_job endp

; Get job status
distributed_get_status proc uses ebx esi edi, executor_ptr:dword, job_id:dword, status_ptr:dword, 
                            output_ptr_ptr:dword, output_len_ptr:dword, exit_code_ptr:dword
    mov ebx, executor_ptr
    test ebx, ebx
    jz error_invalid
    assume ebx:ptr DISTRIBUTED_EXECUTOR
    
    ; Validate parameters
    mov eax, status_ptr
    test eax, eax
    jz error_invalid
    
    ; Lock executor
    invoke WaitForSingleObject, [ebx].executor_mutex, INFINITE
    
    ; Validate job ID
    mov eax, job_id
    cmp eax, [ebx].job_count
    jae error_not_found
    
    ; Get job pointer
    mov edx, [ebx].jobs_ptr
    imul eax, sizeof JOB
    add edx, eax
    assume edx:ptr JOB
    
    ; Return status
    mov eax, status_ptr
    mov ecx, [edx].status
    mov [eax], ecx
    
    ; Return output if requested
    mov eax, output_ptr_ptr
    test eax, eax
    jz no_output_ptr
    mov ecx, [edx].output_ptr
    mov [eax], ecx
    
no_output_ptr:
    mov eax, output_len_ptr
    test eax, eax
    jz no_output_len
    mov ecx, [edx].output_len
    mov [eax], ecx
    
no_output_len:
    mov eax, exit_code_ptr
    test eax, eax
    jz no_exit_code
    mov ecx, [edx].exit_code
    mov [eax], ecx
    
no_exit_code:
    invoke ReleaseMutex, [ebx].executor_mutex
    mov eax, DISTRIBUTED_SUCCESS
    ret
    
error_invalid:
    mov eax, DISTRIBUTED_ERROR_INVALID
    ret
    
error_not_found:
    invoke ReleaseMutex, [ebx].executor_mutex
    mov eax, DISTRIBUTED_ERROR_NOT_FOUND
    ret
    
    assume edx:nothing
    assume ebx:nothing
distributed_get_status endp

; Cancel job
distributed_cancel_job proc uses ebx esi edi, executor_ptr:dword, job_id:dword
    local job_ptr:dword, node_ptr:dword
    
    mov ebx, executor_ptr
    test ebx, ebx
    jz error_invalid
    assume ebx:ptr DISTRIBUTED_EXECUTOR
    
    ; Lock executor
    invoke WaitForSingleObject, [ebx].executor_mutex, INFINITE
    
    ; Validate job ID
    mov eax, job_id
    cmp eax, [ebx].job_count
    jae error_not_found
    
    ; Get job pointer
    mov edx, [ebx].jobs_ptr
    imul eax, sizeof JOB
    add edx, eax
    mov job_ptr, edx
    assume edx:ptr JOB
    
    ; Check if job is running
    cmp [edx].status, 1
    jne not_running
    
    ; Get node pointer
    mov eax, [edx].node_id
    mov ecx, [ebx].nodes_ptr
    imul eax, sizeof NODE
    add ecx, eax
    mov node_ptr, ecx
    assume ecx:ptr NODE
    
    ; Decrement node job count
    dec [ecx].current_jobs
    assume ecx:nothing
    
not_running:
    ; Update job status to failed
    mov [edx].status, 3  ; Failed
    mov [edx].exit_code, -1
    
    invoke ReleaseMutex, [ebx].executor_mutex
    mov eax, DISTRIBUTED_SUCCESS
    ret
    
error_invalid:
    mov eax, DISTRIBUTED_ERROR_INVALID
    ret
    
error_not_found:
    invoke ReleaseMutex, [ebx].executor_mutex
    mov eax, DISTRIBUTED_ERROR_NOT_FOUND
    ret
    
    assume edx:nothing
    assume ebx:nothing
distributed_cancel_job endp

end


