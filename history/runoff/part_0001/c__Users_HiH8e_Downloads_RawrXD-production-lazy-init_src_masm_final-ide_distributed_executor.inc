; Distributed Executor Header File
; Phase D Component 3: Multi-machine execution

; Distributed executor error codes
DISTRIBUTED_SUCCESS equ 0
DISTRIBUTED_ERROR_INVALID equ 1
DISTRIBUTED_ERROR_OOM equ 2
DISTRIBUTED_ERROR_NOT_FOUND equ 3
DISTRIBUTED_ERROR_EXISTS equ 4
DISTRIBUTED_ERROR_FULL equ 5
DISTRIBUTED_ERROR_OFFLINE equ 6
DISTRIBUTED_ERROR_NETWORK equ 7

; Distributed executor structure
DISTRIBUTED_EXECUTOR struct
    executor_mutex dword ?
    node_count dword ?
    max_nodes dword ?
    job_count dword ?
    max_jobs dword ?
    nodes_ptr dword ?
    jobs_ptr dword ?
    allocator_ptr dword ?
    deallocator_ptr dword ?
    stats_enabled dword ?
    reserved dword 16 dup(?)
DISTRIBUTED_EXECUTOR ends

; Node structure
NODE struct
    node_id dword ?
    name_ptr dword ?
    name_len dword ?
    address_ptr dword ?
    address_len dword ?
    port dword ?
    status dword ?
    capacity dword ?
    current_jobs dword ?
    last_heartbeat dword ?
    flags dword ?
    reserved dword 4 dup(?)
NODE ends

; Job structure
JOB struct
    job_id dword ?
    node_id dword ?
    command_ptr dword ?
    command_len dword ?
    input_ptr dword ?
    input_len dword ?
    output_ptr dword ?
    output_len dword ?
    status dword ?
    exit_code dword ?
    created_at dword ?
    started_at dword ?
    completed_at dword ?
    flags dword ?
    reserved dword 4 dup(?)
JOB ends

; Constants
NODE_MAX_NAME_LEN equ 256
NODE_MAX_ADDRESS_LEN equ 256
JOB_MAX_COMMAND_LEN equ 4096
JOB_MAX_INPUT_LEN equ 1048576
JOB_MAX_OUTPUT_LEN equ 1048576

; Function declarations
EXTERN distributed_executor_init:proc
EXTERN distributed_executor_shutdown:proc
EXTERN distributed_register_node:proc
EXTERN distributed_submit_job:proc
EXTERN distributed_get_status:proc
EXTERN distributed_cancel_job:proc