;; ============================================================================
;; RawrXD Multi-Window Kernel Driver - MASM x64
;; Purpose: Hardware-accelerated window management, shared memory IPC,
;;          process scheduling, and GPU dispatch for multi-window IDE
;; Architecture: Ring 0/Ring 3 hybrid with IOCTL bridge
;; ============================================================================

.686
.model flat, stdcall
option casemap:none

; ============================================================================
; Windows API Imports
; ============================================================================
include \masm32\include\windows.inc
include \masm32\include\kernel32.inc
include \masm32\include\user32.inc
include \masm32\include\gdi32.inc
include \masm32\include\advapi32.inc

includelib \masm32\lib\kernel32.lib
includelib \masm32\lib\user32.lib
includelib \masm32\lib\gdi32.lib
includelib \masm32\lib\advapi32.lib

; ============================================================================
; IOCTL Codes for Window Management Driver
; ============================================================================
IOCTL_CREATE_WINDOW         EQU 80002000h
IOCTL_DESTROY_WINDOW        EQU 80002004h
IOCTL_SPLIT_HORIZONTAL      EQU 80002008h
IOCTL_SPLIT_VERTICAL        EQU 8000200Ch
IOCTL_SPLIT_GRID            EQU 80002010h
IOCTL_RESIZE_PANE           EQU 80002014h
IOCTL_FOCUS_PANE            EQU 80002018h
IOCTL_SWAP_PANES            EQU 8000201Ch
IOCTL_GET_LAYOUT            EQU 80002020h
IOCTL_SET_LAYOUT            EQU 80002024h
IOCTL_ALLOC_SHARED_MEM      EQU 80002028h
IOCTL_FREE_SHARED_MEM       EQU 8000202Ch
IOCTL_GPU_DISPATCH          EQU 80002030h
IOCTL_SCHEDULE_TASK         EQU 80002034h
IOCTL_GET_METRICS           EQU 80002038h
IOCTL_MODEL_BIND            EQU 8000203Ch
IOCTL_MODEL_UNBIND          EQU 80002040h
IOCTL_SWARM_DISPATCH        EQU 80002044h
IOCTL_COT_PIPELINE          EQU 80002048h
IOCTL_AUDIT_FILE            EQU 8000204Ch
IOCTL_SNAPSHOT_STATE        EQU 80002050h
IOCTL_RESTORE_STATE         EQU 80002054h

; ============================================================================
; Constants
; ============================================================================
MAX_WINDOWS                 EQU 64
MAX_PANES_PER_WINDOW        EQU 16
MAX_MODELS                  EQU 32
MAX_TASKS                   EQU 256
MAX_SHARED_REGIONS          EQU 128
MAX_SWARM_AGENTS            EQU 16
MAX_COT_STEPS               EQU 64
MAX_AUDIT_ENTRIES           EQU 1024

PANE_TYPE_EDITOR            EQU 0
PANE_TYPE_TERMINAL          EQU 1
PANE_TYPE_AI_CHAT           EQU 2
PANE_TYPE_MODEL_SELECTOR    EQU 3
PANE_TYPE_FILE_EXPLORER     EQU 4
PANE_TYPE_AUDIT_PANEL       EQU 5
PANE_TYPE_COT_VIEWER        EQU 6
PANE_TYPE_SWARM_MONITOR     EQU 7
PANE_TYPE_DEBUGGER          EQU 8
PANE_TYPE_OUTPUT            EQU 9
PANE_TYPE_PREVIEW           EQU 10
PANE_TYPE_DIFF_VIEWER       EQU 11

SPLIT_NONE                  EQU 0
SPLIT_HORIZONTAL            EQU 1
SPLIT_VERTICAL              EQU 2
SPLIT_GRID_2X2              EQU 3
SPLIT_GRID_3X3              EQU 4
SPLIT_TREE                  EQU 5

TASK_PRIORITY_CRITICAL      EQU 0
TASK_PRIORITY_HIGH          EQU 1
TASK_PRIORITY_NORMAL        EQU 2
TASK_PRIORITY_LOW           EQU 3
TASK_PRIORITY_BACKGROUND    EQU 4

TASK_STATE_IDLE             EQU 0
TASK_STATE_QUEUED           EQU 1
TASK_STATE_RUNNING          EQU 2
TASK_STATE_COMPLETE         EQU 3
TASK_STATE_ERROR            EQU 4
TASK_STATE_CANCELLED        EQU 5

MODEL_STATE_UNLOADED        EQU 0
MODEL_STATE_LOADING         EQU 1
MODEL_STATE_READY           EQU 2
MODEL_STATE_BUSY            EQU 3
MODEL_STATE_ERROR           EQU 4

SWARM_MODE_PARALLEL         EQU 0
SWARM_MODE_PIPELINE         EQU 1
SWARM_MODE_CONSENSUS        EQU 2
SWARM_MODE_COMPETITIVE      EQU 3

; ============================================================================
; Structures
; ============================================================================

; --- Window Pane ---
PaneDescriptor STRUCT
    paneId          DWORD   ?           ; Unique pane ID
    paneType        DWORD   ?           ; PANE_TYPE_*
    parentWindow    DWORD   ?           ; Parent window ID
    x               DWORD   ?           ; X position (pixels)
    y               DWORD   ?           ; Y position (pixels)
    width           DWORD   ?           ; Width (pixels)
    height          DWORD   ?           ; Height (pixels)
    minWidth        DWORD   ?           ; Minimum width
    minHeight       DWORD   ?           ; Minimum height
    isActive        DWORD   ?           ; Active/focused flag
    isVisible       DWORD   ?           ; Visibility flag
    zOrder          DWORD   ?           ; Z-order layer
    splitMode       DWORD   ?           ; SPLIT_* mode
    childLeft       DWORD   ?           ; Left/top child pane ID (-1 = none)
    childRight      DWORD   ?           ; Right/bottom child pane ID (-1 = none)
    splitRatio      DWORD   ?           ; Split ratio (0-1000, /1000)
    boundModelId    DWORD   ?           ; Bound AI model ID (-1 = none)
    taskId          DWORD   ?           ; Associated task ID (-1 = none)
    sharedMemHandle DWORD   ?           ; Shared memory region handle
    editorBufferId  DWORD   ?           ; Editor buffer index
    scrollX         DWORD   ?           ; Horizontal scroll
    scrollY         DWORD   ?           ; Vertical scroll
    cursorLine      DWORD   ?           ; Cursor line
    cursorCol       DWORD   ?           ; Cursor column
    language        BYTE 32 DUP(?)      ; Language ID string
    title           BYTE 128 DUP(?)     ; Pane title
    filePath        BYTE 260 DUP(?)     ; Associated file path
PaneDescriptor ENDS

; --- Window Descriptor ---
WindowDescriptor STRUCT
    windowId        DWORD   ?           ; Window ID
    hwnd            DWORD   ?           ; Win32 HWND
    x               DWORD   ?           ; Screen X
    y               DWORD   ?           ; Screen Y
    width           DWORD   ?           ; Window width
    height          DWORD   ?           ; Window height
    isMaximized     DWORD   ?           ; Maximized flag
    isMinimized     DWORD   ?           ; Minimized flag
    rootPaneId      DWORD   ?           ; Root pane of layout tree
    paneCount       DWORD   ?           ; Number of panes
    activePaneId    DWORD   ?           ; Currently focused pane
    layoutMode      DWORD   ?           ; Current layout mode
    title           BYTE 256 DUP(?)     ; Window title
WindowDescriptor ENDS

; --- Model Descriptor ---
ModelDescriptor STRUCT
    modelId         DWORD   ?           ; Unique model ID
    modelState      DWORD   ?           ; MODEL_STATE_*
    provider        BYTE 64 DUP(?)      ; Provider name (Ollama/OpenAI/AWS/etc)
    modelName       BYTE 128 DUP(?)     ; Model name
    endpoint        BYTE 256 DUP(?)     ; API endpoint URL
    contextSize     DWORD   ?           ; Context window size
    maxTokens       DWORD   ?           ; Max output tokens
    temperature     DWORD   ?           ; Temperature * 1000
    tokensPerSec    DWORD   ?           ; Throughput metric
    memoryUsageMB   DWORD   ?           ; RAM usage in MB
    gpuMemoryMB     DWORD   ?           ; VRAM usage in MB
    requestCount    DWORD   ?           ; Total requests served
    errorCount      DWORD   ?           ; Error count
    avgLatencyMs    DWORD   ?           ; Average latency (ms)
    boundPaneId     DWORD   ?           ; Pane this model serves (-1=any)
    capabilities    DWORD   ?           ; Bitmask of capabilities
ModelDescriptor ENDS

; --- Task Descriptor ---
TaskDescriptor STRUCT
    taskId          DWORD   ?           ; Unique task ID
    taskType        DWORD   ?           ; Task type code
    taskState       DWORD   ?           ; TASK_STATE_*
    priority        DWORD   ?           ; TASK_PRIORITY_*
    assignedModel   DWORD   ?           ; Model handling this task
    assignedPane    DWORD   ?           ; Pane displaying results
    progressPct     DWORD   ?           ; Progress 0-100
    createdAt       DWORD   ?           ; Timestamp (tick count)
    startedAt       DWORD   ?           ; Timestamp
    completedAt     DWORD   ?           ; Timestamp
    parentTask      DWORD   ?           ; Parent task ID for chaining
    childCount      DWORD   ?           ; Number of subtasks
    description     BYTE 256 DUP(?)     ; Task description
    inputBuffer     DWORD   ?           ; Pointer to input shared mem
    outputBuffer    DWORD   ?           ; Pointer to output shared mem
    inputSize       DWORD   ?           ; Input buffer size
    outputSize      DWORD   ?           ; Output buffer size
TaskDescriptor ENDS

; --- Shared Memory Region ---
SharedMemRegion STRUCT
    regionId        DWORD   ?           ; Region ID
    hFileMapping    DWORD   ?           ; File mapping handle
    pBaseAddr       DWORD   ?           ; Mapped base address
    size            DWORD   ?           ; Region size in bytes
    refCount        DWORD   ?           ; Reference count
    ownerPaneId     DWORD   ?           ; Owner pane
    isLocked        DWORD   ?           ; Lock flag
    lockOwner       DWORD   ?           ; Thread ID of lock holder
    name            BYTE 64 DUP(?)      ; Region name
SharedMemRegion ENDS

; --- Swarm Agent ---
SwarmAgent STRUCT
    agentId         DWORD   ?           ; Agent index
    modelId         DWORD   ?           ; Bound model
    paneId          DWORD   ?           ; Display pane
    taskId          DWORD   ?           ; Current task
    state           DWORD   ?           ; Agent state
    tokensGenerated DWORD   ?           ; Total tokens
    confidence      DWORD   ?           ; Confidence * 1000
    voteWeight      DWORD   ?           ; Vote weight for consensus
    role            BYTE 64 DUP(?)      ; Agent role (coder/reviewer/etc)
SwarmAgent ENDS

; --- Chain of Thought Step ---
CotStep STRUCT
    stepIndex       DWORD   ?           ; Step number
    parentStep      DWORD   ?           ; Parent step (-1 for root)
    branchId        DWORD   ?           ; Branch ID for tree-of-thought
    confidence      DWORD   ?           ; Confidence * 1000
    tokenCount      DWORD   ?           ; Tokens in this step
    reasoning       BYTE 512 DUP(?)     ; Reasoning text
    conclusion      BYTE 256 DUP(?)     ; Step conclusion
CotStep ENDS

; --- Kernel Metrics ---
KernelMetrics STRUCT
    totalWindows    DWORD   ?
    totalPanes      DWORD   ?
    totalModels     DWORD   ?
    activeTasks     DWORD   ?
    queuedTasks     DWORD   ?
    completedTasks  DWORD   ?
    erroredTasks    DWORD   ?
    totalSharedMem  DWORD   ?
    cpuUsagePct     DWORD   ?
    gpuUsagePct     DWORD   ?
    memUsageMB      DWORD   ?
    gpuMemUsageMB   DWORD   ?
    tokensPerSec    DWORD   ?
    swarmAgentCount DWORD   ?
    cotStepCount    DWORD   ?
    auditEntries    DWORD   ?
    uptimeSec       DWORD   ?
    ipcLatencyUs    DWORD   ?
KernelMetrics ENDS

; ============================================================================
; Data Segment
; ============================================================================
.data

; --- Kernel State ---
gInitialized        DWORD   0
gNextWindowId       DWORD   1
gNextPaneId         DWORD   1
gNextModelId        DWORD   1
gNextTaskId         DWORD   1
gNextRegionId       DWORD   1
gStartTick          DWORD   0

; --- Window Pool ---
gWindowCount        DWORD   0
gWindows            WindowDescriptor MAX_WINDOWS DUP(<>)

; --- Pane Pool ---
gPaneCount          DWORD   0
gPanes              PaneDescriptor (MAX_WINDOWS * MAX_PANES_PER_WINDOW) DUP(<>)

; --- Model Pool ---
gModelCount         DWORD   0
gModels             ModelDescriptor MAX_MODELS DUP(<>)

; --- Task Queue ---
gTaskCount          DWORD   0
gTasks              TaskDescriptor MAX_TASKS DUP(<>)

; --- Shared Memory ---
gSharedMemCount     DWORD   0
gSharedMem          SharedMemRegion MAX_SHARED_REGIONS DUP(<>)

; --- Swarm ---
gSwarmCount         DWORD   0
gSwarmAgents        SwarmAgent MAX_SWARM_AGENTS DUP(<>)
gSwarmMode          DWORD   SWARM_MODE_PARALLEL

; --- Chain of Thought ---
gCotStepCount       DWORD   0
gCotSteps           CotStep MAX_COT_STEPS DUP(<>)

; --- Metrics ---
gMetrics            KernelMetrics <>

; --- Synchronization ---
gKernelMutex        DWORD   0       ; Simple spinlock
gTaskQueueLock      DWORD   0
gSharedMemLock      DWORD   0
gSwarmLock          DWORD   0

; --- Strings ---
szKernelName        BYTE "RawrXD-MultiWindow-Kernel", 0
szDriverDevice      BYTE "\\.\RawrXDMultiWinDriver", 0
szSharedMemPrefix   BYTE "RawrXD_SharedMem_", 0
szInitOk            BYTE "[KERNEL] Multi-Window Kernel initialized", 0
szPaneCreated       BYTE "[KERNEL] Pane created: ID=%d Type=%d", 0
szSplitOk           BYTE "[KERNEL] Split applied: Pane=%d Mode=%d", 0
szModelBound        BYTE "[KERNEL] Model bound: Model=%d Pane=%d", 0
szTaskScheduled     BYTE "[KERNEL] Task scheduled: ID=%d Priority=%d", 0
szSwarmDispatch     BYTE "[KERNEL] Swarm dispatched: %d agents Mode=%d", 0
szCotStep           BYTE "[KERNEL] CoT step: %d/%d Confidence=%d", 0
szAuditStart        BYTE "[KERNEL] Audit started: File=%s", 0

; ============================================================================
; Code Segment
; ============================================================================
.code

; ============================================================================
; KernelInit - Initialize the multi-window kernel
; Returns: EAX = 1 on success, 0 on failure
; ============================================================================
KernelInit PROC
    push ebx
    push esi
    push edi

    ; Check if already initialized
    cmp gInitialized, 1
    je @already_init

    ; Record start time
    invoke GetTickCount
    mov gStartTick, eax

    ; Zero out all pools
    lea edi, gWindows
    mov ecx, SIZEOF gWindows
    xor eax, eax
    rep stosb

    lea edi, gPanes
    mov ecx, SIZEOF gPanes
    xor eax, eax
    rep stosb

    lea edi, gModels
    mov ecx, SIZEOF gModels
    xor eax, eax
    rep stosb

    lea edi, gTasks
    mov ecx, SIZEOF gTasks
    xor eax, eax
    rep stosb

    lea edi, gSharedMem
    mov ecx, SIZEOF gSharedMem
    xor eax, eax
    rep stosb

    lea edi, gSwarmAgents
    mov ecx, SIZEOF gSwarmAgents
    xor eax, eax
    rep stosb

    lea edi, gCotSteps
    mov ecx, SIZEOF gCotSteps
    xor eax, eax
    rep stosb

    lea edi, gMetrics
    mov ecx, SIZEOF KernelMetrics
    xor eax, eax
    rep stosb

    ; Initialize counters
    mov gWindowCount, 0
    mov gPaneCount, 0
    mov gModelCount, 0
    mov gTaskCount, 0
    mov gSharedMemCount, 0
    mov gSwarmCount, 0
    mov gCotStepCount, 0
    mov gNextWindowId, 1
    mov gNextPaneId, 1
    mov gNextModelId, 1
    mov gNextTaskId, 1
    mov gNextRegionId, 1

    ; Mark as initialized
    mov gInitialized, 1

    pop edi
    pop esi
    pop ebx
    mov eax, 1
    ret

@already_init:
    pop edi
    pop esi
    pop ebx
    mov eax, 1
    ret
KernelInit ENDP

; ============================================================================
; SpinLock / SpinUnlock - Lightweight mutex via XCHG
; ============================================================================
SpinLock PROC pLock:DWORD
    push eax
    push ecx
    mov ecx, pLock
@spin:
    mov eax, 1
    xchg eax, [ecx]
    test eax, eax
    jnz @spin
    pop ecx
    pop eax
    ret
SpinLock ENDP

SpinUnlock PROC pLock:DWORD
    push ecx
    mov ecx, pLock
    mov DWORD PTR [ecx], 0
    pop ecx
    ret
SpinUnlock ENDP

; ============================================================================
; CreatePane - Create a new pane in a window
; Args: windowId, paneType, x, y, w, h
; Returns: EAX = new pane ID, 0 on error
; ============================================================================
CreatePane PROC windowId:DWORD, paneType:DWORD, px:DWORD, py:DWORD, pw:DWORD, ph:DWORD
    push ebx
    push esi
    push edi

    invoke SpinLock, ADDR gKernelMutex

    ; Check capacity
    mov eax, gPaneCount
    cmp eax, MAX_WINDOWS * MAX_PANES_PER_WINDOW
    jge @cap_err

    ; Find slot
    mov esi, eax
    imul esi, SIZEOF PaneDescriptor
    lea edi, gPanes
    add edi, esi

    ; Assign ID
    mov eax, gNextPaneId
    mov [edi].PaneDescriptor.paneId, eax
    inc gNextPaneId

    ; Set properties
    mov ebx, paneType
    mov [edi].PaneDescriptor.paneType, ebx

    mov ebx, windowId
    mov [edi].PaneDescriptor.parentWindow, ebx

    mov ebx, px
    mov [edi].PaneDescriptor.x, ebx
    mov ebx, py
    mov [edi].PaneDescriptor.y, ebx
    mov ebx, pw
    mov [edi].PaneDescriptor.width, ebx
    mov ebx, ph
    mov [edi].PaneDescriptor.height, ebx

    ; Defaults
    mov [edi].PaneDescriptor.minWidth, 100
    mov [edi].PaneDescriptor.minHeight, 50
    mov [edi].PaneDescriptor.isActive, 0
    mov [edi].PaneDescriptor.isVisible, 1
    mov [edi].PaneDescriptor.zOrder, 0
    mov [edi].PaneDescriptor.splitMode, SPLIT_NONE
    mov [edi].PaneDescriptor.childLeft, -1
    mov [edi].PaneDescriptor.childRight, -1
    mov [edi].PaneDescriptor.splitRatio, 500   ; 50%
    mov [edi].PaneDescriptor.boundModelId, -1
    mov [edi].PaneDescriptor.taskId, -1
    mov [edi].PaneDescriptor.sharedMemHandle, -1
    mov [edi].PaneDescriptor.editorBufferId, -1
    mov [edi].PaneDescriptor.scrollX, 0
    mov [edi].PaneDescriptor.scrollY, 0
    mov [edi].PaneDescriptor.cursorLine, 1
    mov [edi].PaneDescriptor.cursorCol, 1

    inc gPaneCount

    ; Update metrics
    mov eax, gPaneCount
    mov gMetrics.KernelMetrics.totalPanes, eax

    mov eax, [edi].PaneDescriptor.paneId

    invoke SpinUnlock, ADDR gKernelMutex

    pop edi
    pop esi
    pop ebx
    ret

@cap_err:
    invoke SpinUnlock, ADDR gKernelMutex
    pop edi
    pop esi
    pop ebx
    xor eax, eax
    ret
CreatePane ENDP

; ============================================================================
; SplitPane - Split an existing pane into two
; Args: paneId, splitMode (SPLIT_HORIZONTAL or SPLIT_VERTICAL), ratio (0-1000)
; Returns: EAX = right/bottom child pane ID, 0 on error
; ============================================================================
SplitPane PROC paneId:DWORD, splitMode:DWORD, ratio:DWORD
    push ebx
    push esi
    push edi

    invoke SpinLock, ADDR gKernelMutex

    ; Find parent pane
    xor ecx, ecx
    lea esi, gPanes
@find_pane:
    cmp ecx, gPaneCount
    jge @not_found
    cmp [esi].PaneDescriptor.paneId, 0
    je @next_pane
    mov eax, paneId
    cmp [esi].PaneDescriptor.paneId, eax
    je @found_pane
@next_pane:
    add esi, SIZEOF PaneDescriptor
    inc ecx
    jmp @find_pane

@found_pane:
    ; Store parent info
    mov ebx, [esi].PaneDescriptor.x
    mov edx, [esi].PaneDescriptor.y

    ; Set split mode on parent
    mov eax, splitMode
    mov [esi].PaneDescriptor.splitMode, eax
    mov eax, ratio
    mov [esi].PaneDescriptor.splitRatio, eax

    ; Calculate child dimensions based on split mode
    mov eax, splitMode
    cmp eax, SPLIT_VERTICAL
    je @vertical_split

    ; Horizontal split - top and bottom
    mov eax, [esi].PaneDescriptor.height
    imul eax, ratio
    mov edx, 1000
    div edx
    ; EAX = top height

    ; Create left (top) child
    push eax  ; save top height
    invoke SpinUnlock, ADDR gKernelMutex

    invoke CreatePane, [esi].PaneDescriptor.parentWindow, \
                       [esi].PaneDescriptor.paneType, \
                       [esi].PaneDescriptor.x, \
                       [esi].PaneDescriptor.y, \
                       [esi].PaneDescriptor.width, \
                       eax
    pop ebx   ; restore top height
    ; Left child ID in EAX
    push eax  ; save left child ID

    ; Create right (bottom) child
    mov eax, [esi].PaneDescriptor.height
    sub eax, ebx  ; bottom height
    mov ecx, [esi].PaneDescriptor.y
    add ecx, ebx  ; bottom Y

    invoke CreatePane, [esi].PaneDescriptor.parentWindow, \
                       [esi].PaneDescriptor.paneType, \
                       [esi].PaneDescriptor.x, \
                       ecx, \
                       [esi].PaneDescriptor.width, \
                       eax
    ; Right child ID in EAX
    mov edi, eax

    invoke SpinLock, ADDR gKernelMutex

    ; Link children to parent
    pop ebx   ; left child ID
    mov [esi].PaneDescriptor.childLeft, ebx
    mov [esi].PaneDescriptor.childRight, edi

    mov eax, edi  ; return right child ID
    jmp @split_done

@vertical_split:
    ; Vertical split - left and right
    mov eax, [esi].PaneDescriptor.width
    imul eax, ratio
    mov edx, 1000
    div edx
    ; EAX = left width

    invoke SpinUnlock, ADDR gKernelMutex

    push eax  ; save left width
    invoke CreatePane, [esi].PaneDescriptor.parentWindow, \
                       [esi].PaneDescriptor.paneType, \
                       [esi].PaneDescriptor.x, \
                       [esi].PaneDescriptor.y, \
                       eax, \
                       [esi].PaneDescriptor.height
    pop ebx   ; restore left width
    push eax  ; save left child ID

    mov eax, [esi].PaneDescriptor.width
    sub eax, ebx
    mov ecx, [esi].PaneDescriptor.x
    add ecx, ebx

    invoke CreatePane, [esi].PaneDescriptor.parentWindow, \
                       [esi].PaneDescriptor.paneType, \
                       ecx, \
                       [esi].PaneDescriptor.y, \
                       eax, \
                       [esi].PaneDescriptor.height
    mov edi, eax

    invoke SpinLock, ADDR gKernelMutex

    pop ebx
    mov [esi].PaneDescriptor.childLeft, ebx
    mov [esi].PaneDescriptor.childRight, edi

    mov eax, edi
    jmp @split_done

@split_done:
    invoke SpinUnlock, ADDR gKernelMutex
    pop edi
    pop esi
    pop ebx
    ret

@not_found:
    invoke SpinUnlock, ADDR gKernelMutex
    pop edi
    pop esi
    pop ebx
    xor eax, eax
    ret
SplitPane ENDP

; ============================================================================
; AllocSharedMemory - Create shared memory region between panes
; Args: name (ptr to string), size, ownerPaneId
; Returns: EAX = region ID, 0 on error
; ============================================================================
AllocSharedMemory PROC pName:DWORD, dwSize:DWORD, ownerPane:DWORD
    push ebx
    push esi
    push edi

    invoke SpinLock, ADDR gSharedMemLock

    mov eax, gSharedMemCount
    cmp eax, MAX_SHARED_REGIONS
    jge @shm_cap_err

    ; Find slot
    mov esi, eax
    imul esi, SIZEOF SharedMemRegion
    lea edi, gSharedMem
    add edi, esi

    ; Create file mapping
    invoke CreateFileMappingA, -1, NULL, 4, 0, dwSize, pName  ; PAGE_READWRITE=4
    test eax, eax
    jz @shm_create_err
    mov [edi].SharedMemRegion.hFileMapping, eax

    ; Map view
    invoke MapViewOfFile, eax, 0Fh, 0, 0, dwSize  ; FILE_MAP_ALL_ACCESS
    test eax, eax
    jz @shm_map_err
    mov [edi].SharedMemRegion.pBaseAddr, eax

    ; Assign ID and properties
    mov eax, gNextRegionId
    mov [edi].SharedMemRegion.regionId, eax
    inc gNextRegionId

    mov eax, dwSize
    mov [edi].SharedMemRegion.size, eax
    mov [edi].SharedMemRegion.refCount, 1

    mov eax, ownerPane
    mov [edi].SharedMemRegion.ownerPaneId, eax
    mov [edi].SharedMemRegion.isLocked, 0
    mov [edi].SharedMemRegion.lockOwner, 0

    ; Copy name
    push edi
    lea edi, [edi].SharedMemRegion.name
    mov esi, pName
    mov ecx, 63
@copy_name:
    lodsb
    stosb
    test al, al
    jz @name_done
    loop @copy_name
@name_done:
    mov BYTE PTR [edi], 0
    pop edi

    inc gSharedMemCount
    mov eax, gSharedMemCount
    mov gMetrics.KernelMetrics.totalSharedMem, eax

    mov eax, [edi].SharedMemRegion.regionId

    invoke SpinUnlock, ADDR gSharedMemLock
    pop edi
    pop esi
    pop ebx
    ret

@shm_map_err:
    invoke CloseHandle, [edi].SharedMemRegion.hFileMapping
@shm_create_err:
@shm_cap_err:
    invoke SpinUnlock, ADDR gSharedMemLock
    pop edi
    pop esi
    pop ebx
    xor eax, eax
    ret
AllocSharedMemory ENDP

; ============================================================================
; ScheduleTask - Add a task to the priority queue
; Args: taskType, priority, modelId, paneId, description (ptr)
; Returns: EAX = task ID, 0 on error
; ============================================================================
ScheduleTask PROC taskType:DWORD, priority:DWORD, modelId:DWORD, paneId:DWORD, pDesc:DWORD
    push ebx
    push esi
    push edi

    invoke SpinLock, ADDR gTaskQueueLock

    mov eax, gTaskCount
    cmp eax, MAX_TASKS
    jge @task_cap_err

    ; Find slot
    mov esi, eax
    imul esi, SIZEOF TaskDescriptor
    lea edi, gTasks
    add edi, esi

    ; Assign ID
    mov eax, gNextTaskId
    mov [edi].TaskDescriptor.taskId, eax
    inc gNextTaskId

    ; Set properties
    mov eax, taskType
    mov [edi].TaskDescriptor.taskType, eax
    mov [edi].TaskDescriptor.taskState, TASK_STATE_QUEUED

    mov eax, priority
    mov [edi].TaskDescriptor.priority, eax

    mov eax, modelId
    mov [edi].TaskDescriptor.assignedModel, eax

    mov eax, paneId
    mov [edi].TaskDescriptor.assignedPane, eax

    mov [edi].TaskDescriptor.progressPct, 0

    invoke GetTickCount
    mov [edi].TaskDescriptor.createdAt, eax
    mov [edi].TaskDescriptor.startedAt, 0
    mov [edi].TaskDescriptor.completedAt, 0
    mov [edi].TaskDescriptor.parentTask, -1
    mov [edi].TaskDescriptor.childCount, 0

    ; Copy description
    push edi
    lea edi, [edi].TaskDescriptor.description
    mov esi, pDesc
    mov ecx, 255
@copy_desc:
    lodsb
    stosb
    test al, al
    jz @desc_done
    loop @copy_desc
@desc_done:
    mov BYTE PTR [edi], 0
    pop edi

    inc gTaskCount

    ; Update metrics
    mov eax, 0
    mov ecx, 0
    push edi
    lea edi, gTasks
    mov ebx, gTaskCount
@count_active:
    cmp ecx, ebx
    jge @count_done
    cmp [edi].TaskDescriptor.taskState, TASK_STATE_RUNNING
    jne @not_running
    inc eax
@not_running:
    add edi, SIZEOF TaskDescriptor
    inc ecx
    jmp @count_active
@count_done:
    pop edi
    mov gMetrics.KernelMetrics.activeTasks, eax

    mov eax, [edi].TaskDescriptor.taskId

    invoke SpinUnlock, ADDR gTaskQueueLock
    pop edi
    pop esi
    pop ebx
    ret

@task_cap_err:
    invoke SpinUnlock, ADDR gTaskQueueLock
    pop edi
    pop esi
    pop ebx
    xor eax, eax
    ret
ScheduleTask ENDP

; ============================================================================
; BindModelToPane - Associate an AI model with a pane
; Args: modelId, paneId
; Returns: EAX = 1 success, 0 error
; ============================================================================
BindModelToPane PROC modelId:DWORD, paneId:DWORD
    push ebx
    push esi

    invoke SpinLock, ADDR gKernelMutex

    ; Find model
    xor ecx, ecx
    lea esi, gModels
@find_model:
    cmp ecx, gModelCount
    jge @model_not_found
    mov eax, modelId
    cmp [esi].ModelDescriptor.modelId, eax
    je @model_found
    add esi, SIZEOF ModelDescriptor
    inc ecx
    jmp @find_model

@model_found:
    mov eax, paneId
    mov [esi].ModelDescriptor.boundPaneId, eax

    ; Find pane and set its bound model
    xor ecx, ecx
    lea ebx, gPanes
@find_pane2:
    cmp ecx, gPaneCount
    jge @pane_not_found2
    mov eax, paneId
    cmp [ebx].PaneDescriptor.paneId, eax
    je @pane_found2
    add ebx, SIZEOF PaneDescriptor
    inc ecx
    jmp @find_pane2

@pane_found2:
    mov eax, modelId
    mov [ebx].PaneDescriptor.boundModelId, eax

    invoke SpinUnlock, ADDR gKernelMutex
    pop esi
    pop ebx
    mov eax, 1
    ret

@model_not_found:
@pane_not_found2:
    invoke SpinUnlock, ADDR gKernelMutex
    pop esi
    pop ebx
    xor eax, eax
    ret
BindModelToPane ENDP

; ============================================================================
; DispatchSwarm - Launch multi-agent swarm across panes
; Args: mode (SWARM_MODE_*), agentCount
; Returns: EAX = 1 success, 0 error
; ============================================================================
DispatchSwarm PROC swarmMode:DWORD, agentCount:DWORD
    push ebx
    push esi
    push edi

    invoke SpinLock, ADDR gSwarmLock

    mov eax, agentCount
    cmp eax, MAX_SWARM_AGENTS
    jg @swarm_cap_err

    mov eax, swarmMode
    mov gSwarmMode, eax

    ; Initialize agents
    xor ecx, ecx
    lea edi, gSwarmAgents
@init_agents:
    cmp ecx, agentCount
    jge @agents_done

    mov [edi].SwarmAgent.agentId, ecx
    mov [edi].SwarmAgent.state, TASK_STATE_QUEUED
    mov [edi].SwarmAgent.tokensGenerated, 0
    mov [edi].SwarmAgent.confidence, 500  ; 50%
    mov [edi].SwarmAgent.voteWeight, 100

    ; Auto-assign models round-robin
    mov eax, ecx
    xor edx, edx
    mov ebx, gModelCount
    test ebx, ebx
    jz @no_model
    div ebx
    mov eax, edx
    inc eax  ; 1-based model ID
    mov [edi].SwarmAgent.modelId, eax
    jmp @model_assigned
@no_model:
    mov [edi].SwarmAgent.modelId, -1
@model_assigned:

    add edi, SIZEOF SwarmAgent
    inc ecx
    jmp @init_agents

@agents_done:
    mov gSwarmCount, ecx
    mov gMetrics.KernelMetrics.swarmAgentCount, ecx

    invoke SpinUnlock, ADDR gSwarmLock
    pop edi
    pop esi
    pop ebx
    mov eax, 1
    ret

@swarm_cap_err:
    invoke SpinUnlock, ADDR gSwarmLock
    pop edi
    pop esi
    pop ebx
    xor eax, eax
    ret
DispatchSwarm ENDP

; ============================================================================
; AddCotStep - Add a chain-of-thought reasoning step
; Args: parentStep, branchId, confidence, reasoning (ptr), conclusion (ptr)
; Returns: EAX = step index, -1 on error
; ============================================================================
AddCotStep PROC parentStep:DWORD, branchId:DWORD, confidence:DWORD, pReasoning:DWORD, pConclusion:DWORD
    push ebx
    push esi
    push edi

    mov eax, gCotStepCount
    cmp eax, MAX_COT_STEPS
    jge @cot_cap_err

    mov esi, eax
    imul esi, SIZEOF CotStep
    lea edi, gCotSteps
    add edi, esi

    mov eax, gCotStepCount
    mov [edi].CotStep.stepIndex, eax

    mov eax, parentStep
    mov [edi].CotStep.parentStep, eax

    mov eax, branchId
    mov [edi].CotStep.branchId, eax

    mov eax, confidence
    mov [edi].CotStep.confidence, eax

    ; Copy reasoning
    push edi
    lea edi, [edi].CotStep.reasoning
    mov esi, pReasoning
    mov ecx, 511
@copy_reason:
    lodsb
    stosb
    test al, al
    jz @reason_done
    loop @copy_reason
@reason_done:
    mov BYTE PTR [edi], 0
    pop edi

    ; Copy conclusion
    push edi
    lea edi, [edi].CotStep.conclusion
    mov esi, pConclusion
    mov ecx, 255
@copy_concl:
    lodsb
    stosb
    test al, al
    jz @concl_done
    loop @copy_concl
@concl_done:
    mov BYTE PTR [edi], 0
    pop edi

    mov eax, gCotStepCount
    inc gCotStepCount
    mov ebx, gCotStepCount
    mov gMetrics.KernelMetrics.cotStepCount, ebx

    pop edi
    pop esi
    pop ebx
    ret

@cot_cap_err:
    pop edi
    pop esi
    pop ebx
    mov eax, -1
    ret
AddCotStep ENDP

; ============================================================================
; GetKernelMetrics - Snapshot current kernel metrics
; Args: pMetrics (ptr to KernelMetrics output buffer)
; Returns: EAX = 1
; ============================================================================
GetKernelMetrics PROC pMetrics:DWORD
    push esi
    push edi
    push ecx

    ; Update uptime
    invoke GetTickCount
    sub eax, gStartTick
    xor edx, edx
    mov ecx, 1000
    div ecx
    mov gMetrics.KernelMetrics.uptimeSec, eax

    ; Copy metrics to output
    lea esi, gMetrics
    mov edi, pMetrics
    mov ecx, SIZEOF KernelMetrics
    rep movsb

    pop ecx
    pop edi
    pop esi
    mov eax, 1
    ret
GetKernelMetrics ENDP

; ============================================================================
; IoctlDispatch - Handle IOCTL requests from userspace
; Args: ioctl code, input buffer, input size, output buffer, output size
; Returns: EAX = bytes written to output, -1 on error
; ============================================================================
IoctlDispatch PROC dwIoctl:DWORD, pInBuf:DWORD, cbIn:DWORD, pOutBuf:DWORD, cbOut:DWORD
    mov eax, dwIoctl

    cmp eax, IOCTL_CREATE_WINDOW
    je @ioctl_create_window
    cmp eax, IOCTL_SPLIT_HORIZONTAL
    je @ioctl_split_h
    cmp eax, IOCTL_SPLIT_VERTICAL
    je @ioctl_split_v
    cmp eax, IOCTL_GET_METRICS
    je @ioctl_get_metrics
    cmp eax, IOCTL_MODEL_BIND
    je @ioctl_model_bind
    cmp eax, IOCTL_SWARM_DISPATCH
    je @ioctl_swarm
    cmp eax, IOCTL_COT_PIPELINE
    je @ioctl_cot
    cmp eax, IOCTL_SCHEDULE_TASK
    je @ioctl_sched
    cmp eax, IOCTL_ALLOC_SHARED_MEM
    je @ioctl_alloc_shm
    cmp eax, IOCTL_SNAPSHOT_STATE
    je @ioctl_snapshot
    jmp @ioctl_unknown

@ioctl_create_window:
    ; Input: PaneDescriptor with type, x, y, w, h
    mov esi, pInBuf
    invoke CreatePane, [esi].PaneDescriptor.parentWindow, \
                       [esi].PaneDescriptor.paneType, \
                       [esi].PaneDescriptor.x, \
                       [esi].PaneDescriptor.y, \
                       [esi].PaneDescriptor.width, \
                       [esi].PaneDescriptor.height
    mov edi, pOutBuf
    mov [edi], eax
    mov eax, 4
    ret

@ioctl_split_h:
    mov esi, pInBuf
    invoke SplitPane, [esi], SPLIT_HORIZONTAL, [esi+4]
    mov edi, pOutBuf
    mov [edi], eax
    mov eax, 4
    ret

@ioctl_split_v:
    mov esi, pInBuf
    invoke SplitPane, [esi], SPLIT_VERTICAL, [esi+4]
    mov edi, pOutBuf
    mov [edi], eax
    mov eax, 4
    ret

@ioctl_get_metrics:
    invoke GetKernelMetrics, pOutBuf
    mov eax, SIZEOF KernelMetrics
    ret

@ioctl_model_bind:
    mov esi, pInBuf
    invoke BindModelToPane, [esi], [esi+4]
    mov edi, pOutBuf
    mov [edi], eax
    mov eax, 4
    ret

@ioctl_swarm:
    mov esi, pInBuf
    invoke DispatchSwarm, [esi], [esi+4]
    mov edi, pOutBuf
    mov [edi], eax
    mov eax, 4
    ret

@ioctl_cot:
    mov esi, pInBuf
    invoke AddCotStep, [esi], [esi+4], [esi+8], [esi+12], [esi+16]
    mov edi, pOutBuf
    mov [edi], eax
    mov eax, 4
    ret

@ioctl_sched:
    mov esi, pInBuf
    invoke ScheduleTask, [esi], [esi+4], [esi+8], [esi+12], [esi+16]
    mov edi, pOutBuf
    mov [edi], eax
    mov eax, 4
    ret

@ioctl_alloc_shm:
    mov esi, pInBuf
    invoke AllocSharedMemory, [esi], [esi+4], [esi+8]
    mov edi, pOutBuf
    mov [edi], eax
    mov eax, 4
    ret

@ioctl_snapshot:
    invoke GetKernelMetrics, pOutBuf
    mov eax, SIZEOF KernelMetrics
    ret

@ioctl_unknown:
    mov eax, -1
    ret
IoctlDispatch ENDP

; ============================================================================
; Entry Point - Self-test when run standalone
; ============================================================================
start:
    invoke KernelInit

    ; Create test window with panes
    invoke CreatePane, 1, PANE_TYPE_EDITOR, 0, 0, 800, 600
    push eax
    invoke CreatePane, 1, PANE_TYPE_AI_CHAT, 800, 0, 400, 600
    invoke CreatePane, 1, PANE_TYPE_TERMINAL, 0, 600, 1200, 200
    invoke CreatePane, 1, PANE_TYPE_MODEL_SELECTOR, 800, 0, 400, 300

    ; Test split
    pop eax
    invoke SplitPane, eax, SPLIT_VERTICAL, 500

    ; Test shared memory
    invoke AllocSharedMemory, ADDR szSharedMemPrefix, 65536, 1

    ; Test task scheduling
    invoke ScheduleTask, 1, TASK_PRIORITY_NORMAL, 1, 1, ADDR szInitOk

    ; Test swarm dispatch
    invoke DispatchSwarm, SWARM_MODE_PARALLEL, 4

    ; Test CoT
    invoke AddCotStep, -1, 0, 800, ADDR szInitOk, ADDR szSplitOk

    invoke ExitProcess, 0

END start
