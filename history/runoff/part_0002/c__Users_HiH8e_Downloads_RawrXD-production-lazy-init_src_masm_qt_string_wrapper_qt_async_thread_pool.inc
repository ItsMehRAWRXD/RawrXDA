; =============================================================================
; Qt Async Thread Pool - Pure MASM Include Definitions (x64)
; =============================================================================
; Purpose: Thread pool, work queue, and synchronization definitions
; Architecture: Windows + POSIX compatible definitions
; Status: Phase 3 Implementation
; =============================================================================

; =============================================================================
; Thread Pool Configuration Constants
; =============================================================================

; Maximum number of worker threads (configurable per platform)
MAX_WORKER_THREADS          EQU 16
DEFAULT_WORKER_THREADS      EQU 4
MIN_WORKER_THREADS          EQU 1

; Work queue limits
MAX_QUEUE_SIZE              EQU 1024
DEFAULT_QUEUE_SIZE          EQU 256
QUEUE_TIMEOUT_MS            EQU 5000

; Thread priority levels
THREAD_PRIORITY_IDLE        EQU -15
THREAD_PRIORITY_BELOW_NORMAL EQU -2
THREAD_PRIORITY_NORMAL      EQU 0
THREAD_PRIORITY_ABOVE_NORMAL EQU 2
THREAD_PRIORITY_HIGH        EQU 15

; =============================================================================
; Work Item Structure
; =============================================================================

ASYNC_WORK_ITEM struct
    callback_fn             DQ  ?       ; Function pointer to execute
    context_data            DQ  ?       ; User context/data pointer
    file_handle             DQ  ?       ; File handle (for I/O operations)
    buffer_ptr              DQ  ?       ; Buffer pointer
    buffer_size             DQ  ?       ; Buffer size
    offset                  DQ  ?       ; File offset
    priority                DD  ?       ; Work priority (0 = normal)
    operation_type          DD  ?       ; Type of operation (read, write, etc.)
    status                  DD  ?       ; Current status (pending, running, complete)
    result_code             DD  ?       ; Result code after completion
    progress_callback       DQ  ?       ; Progress callback function
    cancel_event            DQ  ?       ; Cancellation event handle
    completion_event        DQ  ?       ; Completion event handle
ASYNC_WORK_ITEM ends

; Operation types for work items
ASYNC_OP_READ               EQU 1
ASYNC_OP_WRITE              EQU 2
ASYNC_OP_COPY               EQU 3
ASYNC_OP_DELETE             EQU 4
ASYNC_OP_CUSTOM             EQU 5

; Work item status codes
ASYNC_STATUS_PENDING        EQU 0
ASYNC_STATUS_QUEUED         EQU 1
ASYNC_STATUS_RUNNING        EQU 2
ASYNC_STATUS_COMPLETE       EQU 3
ASYNC_STATUS_CANCELLED      EQU 4
ASYNC_STATUS_ERROR          EQU 5

; =============================================================================
; Thread Pool State Structure
; =============================================================================

THREAD_POOL struct
    worker_count            DD  ?       ; Number of active worker threads
    queue_size              DD  ?       ; Current queue size
    total_processed         DQ  ?       ; Total work items processed
    total_errors            DQ  ?       ; Total errors occurred
    is_running              DB  ?       ; Pool running state
    reserved1               DB  ?       ; Alignment padding
    reserved2               DW  ?       ; Alignment padding
    shutdown_event          DQ  ?       ; Shutdown event handle
    queue_mutex             DQ  ?       ; Queue synchronization
    worker_semaphore        DQ  ?       ; Worker thread signaling
    queue_front             DQ  ?       ; Work queue front pointer
    queue_back              DQ  ?       ; Work queue back pointer
    statistics_lock         DQ  ?       ; Stats synchronization
THREAD_POOL ends

; =============================================================================
; Progress Tracking Structure
; =============================================================================

ASYNC_PROGRESS struct
    bytes_processed         DQ  ?       ; Total bytes processed
    total_bytes             DQ  ?       ; Total bytes to process
    percentage              DD  ?       ; Progress percentage (0-100)
    current_operation       DD  ?       ; Current operation identifier
    elapsed_ms              DQ  ?       ; Elapsed time in milliseconds
    estimated_remaining_ms  DQ  ?       ; Estimated remaining time
ASYNC_PROGRESS ends

; =============================================================================
; Callback Function Signatures
; =============================================================================

; void async_work_callback(ASYNC_WORK_ITEM* work, void* context)
; Called when work item completes
ASYNC_WORK_CALLBACK typedef PTR PROTO C VARARG

; void async_progress_callback(const ASYNC_PROGRESS* progress, void* context)
; Called periodically to report progress
ASYNC_PROGRESS_CALLBACK typedef PTR PROTO C VARARG

; void async_error_callback(int error_code, const char* error_msg, void* context)
; Called when an error occurs
ASYNC_ERROR_CALLBACK typedef PTR PROTO C VARARG

; =============================================================================
; Windows-Specific Threading Definitions
; =============================================================================

IFDEF _WIN64

    ; Windows handle types
    HANDLE                  TYPEDEF QWORD
    DWORD                   TYPEDEF DWORD
    
    ; Create thread function (Windows API)
    ; HANDLE CreateThread(
    ;     LPSECURITY_ATTRIBUTES lpThreadAttributes,
    ;     SIZE_T dwStackSize,
    ;     LPTHREAD_START_ROUTINE lpStartAddress,
    ;     LPVOID lpParameter,
    ;     DWORD dwCreationFlags,
    ;     LPDWORD lpThreadId
    ; )
    
    ; Create event function (Windows API)
    ; HANDLE CreateEventA(
    ;     LPSECURITY_ATTRIBUTES lpEventAttributes,
    ;     BOOL bManualReset,
    ;     BOOL bInitialState,
    ;     LPCSTR lpName
    ; )
    
    ; Wait function (Windows API)
    ; DWORD WaitForSingleObject(HANDLE hHandle, DWORD dwMilliseconds)
    
    ; Interlocked operations (Windows API)
    ; LONG InterlockedIncrement(LONG volatile *Addend)
    ; LONG InterlockedDecrement(LONG volatile *Addend)
    ; BOOL InterlockedCompareExchange(LPVOID Destination, LPVOID Exchange, LPVOID Comperand)
    
    ; CreateThread flags
    CREATE_SUSPENDED        EQU 0x00000004
    THREAD_TERMINATE        EQU 0x00000001
    
    ; Event flags
    EVENT_ALL_ACCESS        EQU 0x1F0003
    EVENT_MODIFY_STATE      EQU 0x000002
    
    ; Wait codes
    WAIT_OBJECT_0           EQU 0
    WAIT_TIMEOUT            EQU 258
    WAIT_FAILED             EQU 0xFFFFFFFF

ELSE

    ; POSIX threading definitions
    ; pthread_t is typically a pointer or unsigned long
    pthread_t               TYPEDEF QWORD
    
    ; pthread_mutex_t structure (Linux x64)
    ; Size varies by glibc, typically 40 bytes
    PTHREAD_MUTEX_SIZE      EQU 40
    
    ; pthread_cond_t structure (Linux x64)
    ; Size varies by glibc, typically 48 bytes
    PTHREAD_COND_SIZE       EQU 48
    
    ; Mutex attribute constants
    PTHREAD_MUTEX_NORMAL    EQU 0
    PTHREAD_MUTEX_RECURSIVE EQU 1
    PTHREAD_MUTEX_ERRORCHECK EQU 2
    
    ; Mutex initialization macro value
    PTHREAD_MUTEX_INITIALIZER EQU 0
    
    ; Pthread flags
    PTHREAD_CREATE_JOINABLE EQU 0
    PTHREAD_CREATE_DETACHED EQU 1

ENDIF

; =============================================================================
; Synchronization Primitives (Cross-Platform Abstraction)
; =============================================================================

; Lock structure (abstracts platform differences)
ASYNC_LOCK struct
    is_locked               DB  ?       ; Current lock state
    owner_thread            DQ  ?       ; Thread that owns lock
    recursion_count         DD  ?       ; For recursive locks
    reserved                DD  ?       ; Alignment padding
    platform_handle         DQ  ?       ; Platform-specific handle
ASYNC_LOCK ends

; Event structure (abstracts platform differences)
ASYNC_EVENT struct
    is_signaled             DB  ?       ; Current signal state
    auto_reset              DB  ?       ; Auto-reset on wait
    reserved                DW  ?       ; Alignment padding
    platform_handle         DQ  ?       ; Platform-specific handle
    wait_count              DD  ?       ; Threads waiting on event
ASYNC_EVENT ends

; Semaphore structure (abstracts platform differences)
ASYNC_SEMAPHORE struct
    current_count           DD  ?       ; Current semaphore count
    max_count               DD  ?       ; Maximum count
    platform_handle         DQ  ?       ; Platform-specific handle
ASYNC_SEMAPHORE ends

; =============================================================================
; Error Codes
; =============================================================================

ASYNC_ERROR_NONE                EQU 0
ASYNC_ERROR_THREAD_CREATE       EQU 1
ASYNC_ERROR_THREAD_JOIN         EQU 2
ASYNC_ERROR_LOCK_ACQUIRE        EQU 3
ASYNC_ERROR_LOCK_RELEASE        EQU 4
ASYNC_ERROR_EVENT_CREATE        EQU 5
ASYNC_ERROR_EVENT_SIGNAL        EQU 6
ASYNC_ERROR_QUEUE_FULL          EQU 7
ASYNC_ERROR_QUEUE_EMPTY         EQU 8
ASYNC_ERROR_POOL_NOT_RUNNING    EQU 9
ASYNC_ERROR_INVALID_WORK_ITEM   EQU 10
ASYNC_ERROR_FILE_OPEN           EQU 11
ASYNC_ERROR_FILE_READ           EQU 12
ASYNC_ERROR_FILE_WRITE          EQU 13
ASYNC_ERROR_BUFFER_TOO_SMALL    EQU 14
ASYNC_ERROR_TIMEOUT             EQU 15
ASYNC_ERROR_CANCELLED           EQU 16
ASYNC_ERROR_OUT_OF_MEMORY       EQU 17
ASYNC_ERROR_INVALID_PARAMETER   EQU 18
ASYNC_ERROR_UNKNOWN             EQU 99

; =============================================================================
; External MASM Function Declarations
; =============================================================================

extern wrapper_thread_pool_create : PROTO C :DWORD
extern wrapper_thread_pool_destroy : PROTO C :QWORD
extern wrapper_thread_pool_queue_work : PROTO C :QWORD, :QWORD
extern wrapper_thread_pool_queue_file_read : PROTO C :QWORD, :QWORD, :QWORD, :QWORD, :QWORD
extern wrapper_thread_pool_queue_file_write : PROTO C :QWORD, :QWORD, :QWORD, :QWORD, :QWORD
extern wrapper_thread_pool_get_status : PROTO C :QWORD, :QWORD
extern wrapper_thread_pool_cancel_work : PROTO C :QWORD, :QWORD
extern wrapper_thread_pool_shutdown : PROTO C :QWORD
extern wrapper_lock_acquire : PROTO C :QWORD
extern wrapper_lock_release : PROTO C :QWORD
extern wrapper_event_create : PROTO C :DB, :DB
extern wrapper_event_signal : PROTO C :QWORD
extern wrapper_event_wait : PROTO C :QWORD, :DWORD
extern wrapper_semaphore_create : PROTO C :DWORD, :DWORD
extern wrapper_semaphore_increment : PROTO C :QWORD
extern wrapper_semaphore_decrement : PROTO C :QWORD, :DWORD

; =============================================================================
; Helper Macros
; =============================================================================

; Check if work item is valid
is_valid_work_item MACRO work_ptr
    cmp work_ptr, 0
    jne @valid_work
    mov eax, ASYNC_ERROR_INVALID_WORK_ITEM
    jmp @invalid_work
@valid_work:
ENDM

; Check pool is running
is_pool_running MACRO pool_ptr
    movzx eax, byte ptr [pool_ptr + OFFSET THREAD_POOL.is_running]
    cmp al, 1
    je @pool_active
    mov eax, ASYNC_ERROR_POOL_NOT_RUNNING
@pool_active:
ENDM

; Acquire synchronization lock
acquire_lock MACRO lock_ptr
    mov rcx, lock_ptr
    call wrapper_lock_acquire
ENDM

; Release synchronization lock
release_lock MACRO lock_ptr
    mov rcx, lock_ptr
    call wrapper_lock_release
ENDM

; Signal event
signal_event MACRO event_ptr
    mov rcx, event_ptr
    call wrapper_event_signal
ENDM

; Wait for event with timeout
wait_event MACRO event_ptr, timeout_ms
    mov rcx, event_ptr
    mov edx, timeout_ms
    call wrapper_event_wait
ENDM

; =============================================================================
; Structure Sizes and Offsets (for assembly reference)
; =============================================================================

WORK_ITEM_SIZE              EQU SIZE ASYNC_WORK_ITEM
WORK_ITEM_CALLBACK          EQU 0
WORK_ITEM_CONTEXT           EQU 8
WORK_ITEM_FILE_HANDLE       EQU 16
WORK_ITEM_BUFFER            EQU 24
WORK_ITEM_BUFFER_SIZE       EQU 32
WORK_ITEM_OFFSET            EQU 40
WORK_ITEM_PRIORITY          EQU 48
WORK_ITEM_OP_TYPE           EQU 52
WORK_ITEM_STATUS            EQU 56
WORK_ITEM_RESULT            EQU 60
WORK_ITEM_PROGRESS_CB       EQU 64
WORK_ITEM_CANCEL_EVENT      EQU 72
WORK_ITEM_COMPLETE_EVENT    EQU 80

POOL_SIZE                   EQU SIZE THREAD_POOL
POOL_WORKER_COUNT           EQU 0
POOL_QUEUE_SIZE             EQU 4
POOL_TOTAL_PROCESSED        EQU 8
POOL_TOTAL_ERRORS           EQU 16
POOL_IS_RUNNING             EQU 24
POOL_SHUTDOWN_EVENT         EQU 32
POOL_QUEUE_MUTEX            EQU 40
POOL_WORKER_SEMAPHORE       EQU 48
POOL_QUEUE_FRONT            EQU 56
POOL_QUEUE_BACK             EQU 64
POOL_STATS_LOCK             EQU 72

; =============================================================================
; End of Include File
; =============================================================================
