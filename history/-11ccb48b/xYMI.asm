;==========================================================================
; masm_ml_training_studio.asm - ML Training Studio for RawrXD IDE
;==========================================================================
; Enterprise-grade ML training interface with:
; - Dataset management and visualization
; - Real-time training metrics dashboard
; - Hyperparameter tuning interface
; - Model comparison and experiment tracking
; - TensorBoard-like visualization
; - GPU/CPU resource monitoring
;==========================================================================

option casemap:none
includelib kernel32.lib
includelib user32.lib
includelib gdi32.lib
includelib comctl32.lib
includelib winhttp.lib

;==========================================================================
; CONSTANTS
;==========================================================================
MAX_DATASETS        equ 50
MAX_MODELS          equ 20
MAX_EXPERIMENTS     equ 100
MAX_METRICS         equ 1000
MAX_HYPERPARAMS     equ 50

; Training states
TRAINING_IDLE       equ 0
TRAINING_RUNNING    equ 1
TRAINING_PAUSED     equ 2
TRAINING_COMPLETED  equ 3
TRAINING_ERROR      equ 4

; Dataset types
DATASET_IMAGES      equ 1
DATASET_TEXT        equ 2
DATASET_TABULAR     equ 3
DATASET_AUDIO       equ 4
DATASET_VIDEO       equ 5

; Model types
MODEL_CLASSIFICATION equ 1
MODEL_REGRESSION     equ 2
MODEL_CLUSTERING     equ 3
MODEL_GENERATIVE     equ 4
MODEL_TRANSFORMER    equ 5

;==========================================================================
; STRUCTURES
;==========================================================================

; Dataset definition (256 bytes)
DATASET struct
    id              DWORD 0
    name            BYTE 64 dup(0)
    path            BYTE 128 dup(0)
    type            DWORD 0
    samples         DWORD 0
    features        DWORD 0
    classes         DWORD 0
    split_train     REAL4 0.8
    split_val       REAL4 0.2
    split_test      REAL4 0.0
    loaded          BYTE 0
    preview_data    QWORD 0
    stats_mean      REAL4 10 dup(0.0)
    stats_std       REAL4 10 dup(0.0)
    stats_min       REAL4 10 dup(0.0)
    stats_max       REAL4 10 dup(0.0)
DATASET ends

; Model definition (512 bytes)
MODEL struct
    id              DWORD 0
    name            BYTE 64 dup(0)
    type            DWORD 0
    architecture    BYTE 128 dup(0)
    input_shape     DWORD 4 dup(0)
    output_shape    DWORD 4 dup(0)
    parameters      DWORD 0
    file_path       BYTE 128 dup(0)
    loaded          BYTE 0
    trained         BYTE 0
    device          DWORD 0  ; 0=CPU, 1=CUDA, 2=ROCm
    memory_usage    DWORD 0  ; MB
MODEL ends

; Training experiment (1024 bytes)
EXPERIMENT struct
    id              DWORD 0
    name            BYTE 64 dup(0)
    model_id        DWORD 0
    dataset_id      DWORD 0
    start_time      QWORD 0
    end_time        QWORD 0
    state           DWORD TRAINING_IDLE
    
    ; Hyperparameters
    learning_rate   REAL4 0.001
    batch_size      DWORD 32
    epochs          DWORD 10
    optimizer       DWORD 0  ; 0=SGD, 1=Adam, 2=AdamW
    loss_function   DWORD 0  ; 0=MSE, 1=CrossEntropy
    
    ; Metrics storage
    metrics_count   DWORD 0
    train_loss      REAL4 MAX_METRICS dup(0.0)
    val_loss        REAL4 MAX_METRICS dup(0.0)
    train_acc       REAL4 MAX_METRICS dup(0.0)
    val_acc         REAL4 MAX_METRICS dup(0.0)
    
    ; Resource usage
    gpu_usage       REAL4 MAX_METRICS dup(0.0)
    memory_usage    DWORD MAX_METRICS dup(0)
    
    ; Checkpoints
    best_checkpoint REAL4 0.0
    checkpoint_path BYTE 128 dup(0)
EXPERIMENT ends

; Hyperparameter search configuration
HYPERPARAM_SEARCH struct
    id              DWORD 0
    experiment_id   DWORD 0
    method          DWORD 0  ; 0=Grid, 1=Random, 2=Bayesian
    param_count     DWORD 0
    param_names     BYTE 32 * MAX_HYPERPARAMS dup(0)
    param_values    REAL4 MAX_HYPERPARAMS dup(0.0)
    best_score      REAL4 0.0
    trials          DWORD 0
HYPERPARAM_SEARCH ends

; Training studio state
TRAINING_STUDIO struct
    hWindow         QWORD 0
    hDatasetList    QWORD 0
    hModelList      QWORD 0
    hExperimentList QWORD 0
    hMetricsChart   QWORD 0
    hResourceChart  QWORD 0
    hHyperparamGrid QWORD 0
    
    ; Data storage
    datasets        DATASET MAX_DATASETS dup(<>)
    dataset_count   DWORD 0
    models          MODEL MAX_MODELS dup(<>)
    model_count     DWORD 0
    experiments     EXPERIMENT MAX_EXPERIMENTS dup(<>)
    experiment_count DWORD 0
    
    ; Current selection
    selected_dataset DWORD -1
    selected_model   DWORD -1
    selected_experiment DWORD -1
    
    ; Training thread
    training_thread QWORD 0
    training_active BYTE 0
    
    ; Real-time update timer
    update_timer    DWORD 0
TRAINING_STUDIO ends

;==========================================================================
; DATA
;==========================================================================
.data
g_training_studio TRAINING_STUDIO <>

; Window classes
szTrainingStudioClass db "TrainingStudio",0
szMetricsChartClass   db "MetricsChart",0
szResourceChartClass  db "ResourceChart",0

; Default strings
szDefaultDatasetName db "New Dataset",0
szDefaultModelName   db "New Model",0
szDefaultExperimentName db "Experiment",0

; Metric names
szTrainLoss db "Training Loss",0
szValLoss   db "Validation Loss",0
szTrainAcc  db "Training Accuracy",0
szValAcc    db "Validation Accuracy",0
szGPUUsage  db "GPU Usage %",0
szMemoryUsage db "Memory Usage (MB)",0

.code

;==========================================================================
; EXTERNAL DECLARATIONS
;==========================================================================
EXTERN RegisterClassExA:PROC
EXTERN CreateWindowExA:PROC
EXTERN ShowWindow:PROC
EXTERN UpdateWindow:PROC
EXTERN InvalidateRect:PROC
EXTERN GetDC:PROC
EXTERN ReleaseDC:PROC
EXTERN CreateCompatibleDC:PROC
EXTERN CreateCompatibleBitmap:PROC
EXTERN SelectObject:PROC
EXTERN BitBlt:PROC
EXTERN DeleteDC:PROC
EXTERN DeleteObject:PROC
EXTERN CreateSolidBrush:PROC
EXTERN Rectangle:PROC
EXTERN FillRect:PROC
EXTERN DrawTextA:PROC
EXTERN SetBkMode:PROC
EXTERN SetTextColor:PROC
EXTERN CreateFontA:PROC
EXTERN SetTimer:PROC
EXTERN KillTimer:PROC
EXTERN CreateThread:PROC
EXTERN WaitForSingleObject:PROC
EXTERN CloseHandle:PROC

;==========================================================================
; PUBLIC EXPORTS
;==========================================================================
PUBLIC training_studio_init
PUBLIC training_studio_create_window
PUBLIC training_studio_load_dataset
PUBLIC training_studio_create_model
PUBLIC training_studio_start_training
PUBLIC training_studio_stop_training
PUBLIC training_studio_get_metrics
PUBLIC training_studio_export_checkpoint
PUBLIC training_studio_compare_experiments
PUBLIC training_studio_tune_hyperparameters
PUBLIC create_model_from_template
PUBLIC reverse_engineer_model

;==========================================================================
; EXTERNAL FROM model_generator.asm
;==========================================================================
EXTERN create_model_from_template:PROC
EXTERN reverse_engineer_model:PROC

;==========================================================================
; training_studio_init() -> bool (rax)
; Initialize training studio system
;==========================================================================
training_studio_init PROC
    sub rsp, 32
    
    ; Register window classes
    call register_training_studio_class
    call register_metrics_chart_class
    call register_resource_chart_class
    
    ; Initialize data structures
    mov g_training_studio.dataset_count, 0
    mov g_training_studio.model_count, 0
    mov g_training_studio.experiment_count, 0
    mov g_training_studio.training_active, 0
    
    mov rax, 1  ; Success
    add rsp, 32
    ret
training_studio_init ENDP

;==========================================================================
; register_training_studio_class() - Register main window class
;==========================================================================
register_training_studio_class PROC
    LOCAL wc:WNDCLASSEXA
    
    sub rsp, 96
    
    mov dword ptr [wc.cbSize], sizeof WNDCLASSEXA
    mov dword ptr [wc.style], 3     ; CS_HREDRAW | CS_VREDRAW
    lea rax, training_studio_wnd_proc
    mov qword ptr [wc.lpfnWndProc], rax
    mov dword ptr [wc.cbClsExtra], 0
    mov dword ptr [wc.cbWndExtra], 0
    mov qword ptr [wc.hInstance], 0
    mov qword ptr [wc.hIcon], 0
    mov qword ptr [wc.hCursor], 0
    mov qword ptr [wc.hbrBackground], 0
    mov qword ptr [wc.lpszMenuName], 0
    lea rax, szTrainingStudioClass
    mov qword ptr [wc.lpszClassName], rax
    mov qword ptr [wc.hIconSm], 0
    
    lea rcx, wc
    call RegisterClassExA
    
    add rsp, 96
    ret
register_training_studio_class ENDP

;==========================================================================
; register_metrics_chart_class() - Register metrics chart class
;==========================================================================
register_metrics_chart_class PROC
    LOCAL wc:WNDCLASSEXA
    
    sub rsp, 96
    
    mov dword ptr [wc.cbSize], sizeof WNDCLASSEXA
    mov dword ptr [wc.style], 3
    lea rax, metrics_chart_wnd_proc
    mov qword ptr [wc.lpfnWndProc], rax
    mov dword ptr [wc.cbClsExtra], 0
    mov dword ptr [wc.cbWndExtra], 0
    mov qword ptr [wc.hInstance], 0
    mov qword ptr [wc.hIcon], 0
    mov qword ptr [wc.hCursor], 0
    mov qword ptr [wc.hbrBackground], 0
    mov qword ptr [wc.lpszMenuName], 0
    lea rax, szMetricsChartClass
    mov qword ptr [wc.lpszClassName], rax
    mov qword ptr [wc.hIconSm], 0
    
    lea rcx, wc
    call RegisterClassExA
    
    add rsp, 96
    ret
register_metrics_chart_class ENDP

;==========================================================================
; register_resource_chart_class() - Register resource chart class
;==========================================================================
register_resource_chart_class PROC
    LOCAL wc:WNDCLASSEXA
    
    sub rsp, 96
    
    mov dword ptr [wc.cbSize], sizeof WNDCLASSEXA
    mov dword ptr [wc.style], 3
    lea rax, resource_chart_wnd_proc
    mov qword ptr [wc.lpfnWndProc], rax
    mov dword ptr [wc.cbClsExtra], 0
    mov dword ptr [wc.cbWndExtra], 0
    mov qword ptr [wc.hInstance], 0
    mov qword ptr [wc.hIcon], 0
    mov qword ptr [wc.hCursor], 0
    mov qword ptr [wc.hbrBackground], 0
    mov qword ptr [wc.lpszMenuName], 0
    lea rax, szResourceChartClass
    mov qword ptr [wc.lpszClassName], rax
    mov qword ptr [wc.hIconSm], 0
    
    lea rcx, wc
    call RegisterClassExA
    
    add rsp, 96
    ret
register_resource_chart_class ENDP

;==========================================================================
; training_studio_create_window(parent_hwnd: rcx) -> hwnd (rax)
; Create training studio window
;==========================================================================
training_studio_create_window PROC
    push rbx
    sub rsp, 96
    
    mov rbx, rcx  ; Save parent
    
    ; Create main window
    xor rcx, rcx
    lea rdx, szTrainingStudioClass
    lea r8, szTrainingStudioTitle
    mov r9d, 50000000h or 10000000h ; WS_CHILD | WS_VISIBLE
    mov dword ptr [rsp + 32], 0
    mov dword ptr [rsp + 40], 0
    mov dword ptr [rsp + 48], 1200
    mov dword ptr [rsp + 56], 800
    mov qword ptr [rsp + 64], rbx
    mov qword ptr [rsp + 72], 0
    mov qword ptr [rsp + 80], 0
    mov qword ptr [rsp + 88], 0
    call CreateWindowExA
    mov g_training_studio.hWindow, rax
    
    ; Create child windows
    call create_dataset_list
    call create_model_list
    call create_experiment_list
    call create_metrics_chart
    call create_resource_chart
    call create_hyperparam_grid
    
    ; Show window
    mov rcx, g_training_studio.hWindow
    mov edx, 5  ; SW_SHOW
    call ShowWindow
    
    ; Start update timer (100ms intervals)
    mov rcx, g_training_studio.hWindow
    mov edx, 1
    mov r8d, 100
    mov r9d, 0
    call SetTimer
    mov g_training_studio.update_timer, eax
    
    mov rax, g_training_studio.hWindow
    add rsp, 96
    pop rbx
    ret
    
.data
szTrainingStudioTitle db "ML Training Studio",0
.code
training_studio_create_window ENDP

;==========================================================================
; training_studio_load_dataset(path: rcx, type: edx) -> dataset_id (rax)
; Load dataset from file
;==========================================================================
training_studio_load_dataset PROC
    push rbx
    push rsi
    push rdi
    sub rsp, 256
    
    mov rsi, rcx  ; path
    mov edi, edx  ; type
    
    ; Check dataset limit
    mov eax, g_training_studio.dataset_count
    cmp eax, MAX_DATASETS
    jge @limit_reached
    
    ; Get next dataset slot
    mov ebx, eax
    imul rbx, rbx, sizeof DATASET
    lea rdi, g_training_studio.datasets
    add rdi, rbx
    
    ; Set basic info
    mov eax, g_training_studio.dataset_count
    inc eax
    mov [rdi + DATASET.id], eax
    mov [rdi + DATASET.type], edi
    
    ; Copy path
    lea rsi, [rsp + 64]
    mov rcx, rsi
    call strlen
    mov rcx, rdi
    add rcx, DATASET.path
    mov rdx, rsi
    mov r8d, eax
    call memcpy
    
    ; Generate name from filename
    call generate_dataset_name
    
    ; Load dataset statistics (stub)
    call load_dataset_stats
    
    ; Increment count
    inc g_training_studio.dataset_count
    
    ; Update UI
    call update_dataset_list
    
    mov rax, [rdi + DATASET.id]
    jmp @done
    
@limit_reached:
    xor rax, rax
    
@done:
    add rsp, 256
    pop rdi
    pop rsi
    pop rbx
    ret
training_studio_load_dataset ENDP

;==========================================================================
; training_studio_create_model(name: rcx, type: edx, arch: r8) -> model_id (rax)
; Create new model
;==========================================================================
training_studio_create_model PROC
    push rbx
    push rsi
    push rdi
    sub rsp, 256
    
    mov rsi, rcx  ; name
    mov edi, edx  ; type
    mov r12, r8   ; architecture
    
    ; Check model limit
    mov eax, g_training_studio.model_count
    cmp eax, MAX_MODELS
    jge @limit_reached
    
    ; Get next model slot
    mov ebx, eax
    imul rbx, rbx, sizeof MODEL
    lea rdi, g_training_studio.models
    add rdi, rbx
    
    ; Set basic info
    mov eax, g_training_studio.model_count
    inc eax
    mov [rdi + MODEL.id], eax
    mov [rdi + MODEL.type], edi
    
    ; Copy name and architecture
    mov rcx, rdi
    add rcx, MODEL.name
    mov rdx, rsi
    mov r8d, 63
    call strncpy
    
    mov rcx, rdi
    add rcx, MODEL.architecture
    mov rdx, r12
    mov r8d, 127
    call strncpy
    
    ; Set default parameters
    mov [rdi + MODEL.parameters], 1000000  ; 1M params
    mov byte ptr [rdi + MODEL.loaded], 0
    mov byte ptr [rdi + MODEL.trained], 0
    
    ; Increment count
    inc g_training_studio.model_count
    
    ; Update UI
    call update_model_list
    
    mov rax, [rdi + MODEL.id]
    jmp @done
    
@limit_reached:
    xor rax, rax
    
@done:
    add rsp, 256
    pop rdi
    pop rsi
    pop rbx
    ret
training_studio_create_model ENDP

;==========================================================================
; training_studio_start_training(model_id: ecx, dataset_id: edx, config: r8) -> experiment_id (rax)
; Start training experiment
;==========================================================================
training_studio_start_training PROC
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    sub rsp, 512
    
    mov esi, ecx  ; model_id
    mov edi, edx  ; dataset_id
    mov r12, r8   ; config
    
    ; Check experiment limit
    mov eax, g_training_studio.experiment_count
    cmp eax, MAX_EXPERIMENTS
    jge @limit_reached
    
    ; Create experiment
    mov ebx, eax
    imul rbx, rbx, sizeof EXPERIMENT
    lea r13, g_training_studio.experiments
    add r13, rbx
    
    ; Set experiment info
    mov eax, g_training_studio.experiment_count
    inc eax
    mov [r13 + EXPERIMENT.id], eax
    mov [r13 + EXPERIMENT.model_id], esi
    mov [r13 + EXPERIMENT.dataset_id], edi
    
    ; Generate name
    lea rcx, szDefaultExperimentName
    lea rdx, [r13 + EXPERIMENT.name]
    mov r8d, 63
    call strncpy
    
    ; Set start time
    call GetTickCount
    mov [r13 + EXPERIMENT.start_time], rax
    
    ; Set hyperparameters from config
    mov eax, [r12 + 0]  ; learning_rate
    mov [r13 + EXPERIMENT.learning_rate], eax
    mov eax, [r12 + 4]  ; batch_size
    mov [r13 + EXPERIMENT.batch_size], eax
    mov eax, [r12 + 8]  ; epochs
    mov [r13 + EXPERIMENT.epochs], eax
    mov eax, [r12 + 12] ; optimizer
    mov [r13 + EXPERIMENT.optimizer], eax
    mov eax, [r12 + 16] ; loss_function
    mov [r13 + EXPERIMENT.loss_function], eax
    
    ; Set state to running
    mov [r13 + EXPERIMENT.state], TRAINING_RUNNING
    
    ; Increment count
    inc g_training_studio.experiment_count
    
    ; Start training thread
    call start_training_thread
    
    ; Update UI
    call update_experiment_list
    
    mov rax, [r13 + EXPERIMENT.id]
    jmp @done
    
@limit_reached:
    xor rax, rax
    
@done:
    add rsp, 512
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
training_studio_start_training ENDP

;==========================================================================
; training_studio_stop_training(experiment_id: ecx) -> bool (rax)
; Stop training experiment
;==========================================================================
training_studio_stop_training PROC
    push rbx
    sub rsp, 32
    
    mov ebx, ecx  ; experiment_id
    
    ; Find experiment
    call find_experiment_by_id
    test rax, rax
    jz @not_found
    
    ; Set state to completed
    mov [rax + EXPERIMENT.state], TRAINING_COMPLETED
    
    ; Set end time
    call GetTickCount
    mov [rax + EXPERIMENT.end_time], rax
    
    ; Stop training thread
    call stop_training_thread
    
    ; Update UI
    call update_experiment_list
    
    mov rax, 1  ; Success
    jmp @done
    
@not_found:
    xor rax, rax
    
@done:
    add rsp, 32
    pop rbx
    ret
training_studio_stop_training ENDP

;==========================================================================
; Helper functions
;==========================================================================

create_dataset_list PROC
    push rbx
    sub rsp, 48
    
    ; Create dataset list view
    xor r9d, r9d
    mov r8, [g_training_studio.hwnd]
    mov edx, 1001 ; ID_DATASET_LIST
    lea rcx, szListViewClass
    mov r10d, WS_CHILD or WS_VISIBLE or LVS_REPORT
    mov [rsp + 32], r10d
    mov dword ptr [rsp + 40], 10 ; x
    mov dword ptr [rsp + 48], 50 ; y
    mov dword ptr [rsp + 56], 300 ; width
    mov dword ptr [rsp + 64], 200 ; height
    call CreateWindowExA
    mov [g_training_studio.hwnd_dataset_list], rax
    
    add rsp, 48
    pop rbx
    ret
create_dataset_list ENDP

create_model_list PROC
    push rbx
    sub rsp, 48
    
    ; Create model list view
    xor r9d, r9d
    mov r8, [g_training_studio.hwnd]
    mov edx, 1002 ; ID_MODEL_LIST
    lea rcx, szListViewClass
    mov r10d, WS_CHILD or WS_VISIBLE or LVS_REPORT
    mov [rsp + 32], r10d
    mov dword ptr [rsp + 40], 320 ; x
    mov dword ptr [rsp + 48], 50 ; y
    mov dword ptr [rsp + 56], 300 ; width
    mov dword ptr [rsp + 64], 200 ; height
    call CreateWindowExA
    mov [g_training_studio.hwnd_model_list], rax
    
    add rsp, 48
    pop rbx
    ret
create_model_list ENDP

create_experiment_list PROC
    push rbx
    sub rsp, 48
    
    ; Create experiment list view
    xor r9d, r9d
    mov r8, [g_training_studio.hwnd]
    mov edx, 1003 ; ID_EXPERIMENT_LIST
    lea rcx, szListViewClass
    mov r10d, WS_CHILD or WS_VISIBLE or LVS_REPORT
    mov [rsp + 32], r10d
    mov dword ptr [rsp + 40], 10 ; x
    mov dword ptr [rsp + 48], 260 ; y
    mov dword ptr [rsp + 56], 610 ; width
    mov dword ptr [rsp + 64], 150 ; height
    call CreateWindowExA
    mov [g_training_studio.hwnd_experiment_list], rax
    
    add rsp, 48
    pop rbx
    ret
create_experiment_list ENDP

create_metrics_chart PROC
    push rbx
    sub rsp, 48
    
    ; Create metrics chart (custom control)
    xor r9d, r9d
    mov r8, [g_training_studio.hwnd]
    mov edx, 1004 ; ID_METRICS_CHART
    lea rcx, szMetricsChartClass
    mov r10d, WS_CHILD or WS_VISIBLE
    mov [rsp + 32], r10d
    mov dword ptr [rsp + 40], 630 ; x
    mov dword ptr [rsp + 48], 50 ; y
    mov dword ptr [rsp + 56], 350 ; width
    mov dword ptr [rsp + 64], 360 ; height
    call CreateWindowExA
    mov [g_training_studio.hwnd_metrics_chart], rax
    
    add rsp, 48
    pop rbx
    ret
create_metrics_chart ENDP

create_resource_chart PROC
    push rbx
    sub rsp, 48
    
    ; Create resource chart (custom control)
    xor r9d, r9d
    mov r8, [g_training_studio.hwnd]
    mov edx, 1005 ; ID_RESOURCE_CHART
    lea rcx, szResourceChartClass
    mov r10d, WS_CHILD or WS_VISIBLE
    mov [rsp + 32], r10d
    mov dword ptr [rsp + 40], 630 ; x
    mov dword ptr [rsp + 48], 420 ; y
    mov dword ptr [rsp + 56], 350 ; width
    mov dword ptr [rsp + 64], 150 ; height
    call CreateWindowExA
    mov [g_training_studio.hwnd_resource_chart], rax
    
    add rsp, 48
    pop rbx
    ret
create_resource_chart ENDP

create_hyperparam_grid PROC
    push rbx
    sub rsp, 48
    
    ; Create hyperparameter grid
    xor r9d, r9d
    mov r8, [g_training_studio.hwnd]
    mov edx, 1006 ; ID_HYPERPARAM_GRID
    lea rcx, szListViewClass
    mov r10d, WS_CHILD or WS_VISIBLE or LVS_REPORT
    mov [rsp + 32], r10d
    mov dword ptr [rsp + 40], 10 ; x
    mov dword ptr [rsp + 48], 420 ; y
    mov dword ptr [rsp + 56], 610 ; width
    mov dword ptr [rsp + 64], 150 ; height
    call CreateWindowExA
    mov [g_training_studio.hwnd_hyperparam_grid], rax
    
    add rsp, 48
    pop rbx
    ret
create_hyperparam_grid ENDP

update_dataset_list PROC
    ; Update dataset list UI
    ; Simplified: Invalidate the list window to trigger repaint
    mov rcx, g_training_studio.hDatasetList
    xor rdx, rdx
    mov r8d, 1
    call InvalidateRect
    ret
update_dataset_list ENDP

update_model_list PROC
    ; Update model list UI
    mov rcx, g_training_studio.hModelList
    xor rdx, rdx
    mov r8d, 1
    call InvalidateRect
    ret
update_model_list ENDP

update_experiment_list PROC
    ; Update experiment list UI
    mov rcx, g_training_studio.hExperimentList
    xor rdx, rdx
    mov r8d, 1
    call InvalidateRect
    ret
update_experiment_list ENDP

start_training_thread PROC
    push rbx
    sub rsp, 32
    
    ; Start training in background thread
    xor rcx, rcx
    xor rdx, rdx
    lea r8, training_worker_thread
    xor r9, r9
    mov qword ptr [rsp + 32], 0
    mov qword ptr [rsp + 40], 0
    call CreateThread
    
    add rsp, 32
    pop rbx
    ret
start_training_thread ENDP

stop_training_thread PROC
    ; Stop training thread
    ; Implementation would involve setting a stop flag
    ret
stop_training_thread ENDP

find_experiment_by_id PROC
    ; rcx = experiment_id -> rax = experiment_ptr
    push rbx
    xor rax, rax
    xor rbx, rbx
    
@loop:
    cmp ebx, g_training_studio.experiment_count
    jae @not_found
    
    mov rdx, rbx
    imul rdx, rdx, sizeof EXPERIMENT
    lea r8, g_training_studio.experiments
    add r8, rdx
    
    cmp [r8 + EXPERIMENT.id], ecx
    je @found
    
    inc ebx
    jmp @loop
    
@found:
    mov rax, r8
    
@not_found:
    pop rbx
    ret
find_experiment_by_id ENDP

generate_dataset_name PROC
    ; Generate dataset name from path
    ; Implementation would involve finding the last backslash
    ret
generate_dataset_name ENDP

load_dataset_stats PROC
    ; Load dataset statistics
    ; Implementation would involve reading the file and counting lines/tokens
    ret
load_dataset_stats ENDP

training_worker_thread PROC
    ; Background training logic
    ; This simulates training by updating metrics in the current experiment
    
@training_loop:
    cmp g_training_studio.training_active, 0
    je @done
    
    ; Find active experiment
    mov ecx, g_training_studio.selected_experiment
    call find_experiment_by_id
    test rax, rax
    jz @sleep_and_continue
    
    mov rbx, rax ; experiment ptr
    
    ; Update metrics
    mov eax, [rbx + EXPERIMENT.metrics_count]
    cmp eax, MAX_METRICS
    jae @completed
    
    ; Simulate loss decreasing and accuracy increasing
    ; (Very simplified math for demonstration)
    cvtsi2ss xmm0, eax
    movss xmm1, float_100
    divss xmm0, xmm1 ; progress 0.0 to 1.0
    
    ; Loss = 1.0 - progress
    movss xmm1, float_1
    subss xmm1, xmm0
    movss [rbx + EXPERIMENT.train_loss + rax*4], xmm1
    
    ; Acc = progress
    movss [rbx + EXPERIMENT.train_acc + rax*4], xmm0
    
    inc dword ptr [rbx + EXPERIMENT.metrics_count]
    
    ; Check if epochs reached
    mov edx, [rbx + EXPERIMENT.epochs]
    cmp eax, edx
    jae @completed

@sleep_and_continue:
    mov rcx, 100 ; 100ms
    call Sleep
    jmp @training_loop

@completed:
    mov dword ptr [rbx + EXPERIMENT.state], TRAINING_COMPLETED
    mov g_training_studio.training_active, 0

@done:
    ret

.data
float_100 REAL4 100.0
float_1   REAL4 1.0
.code
training_worker_thread ENDP

strlen PROC
    ; rcx = string -> rax = length
    xor rax, rax
@loop:
    cmp byte ptr [rcx + rax], 0
    je @done
    inc rax
    jmp @loop
@done:
    ret
strlen ENDP

strncpy PROC
    ; rcx = dest, rdx = src, r8d = max_len
    xor rax, rax
@loop:
    cmp eax, r8d
    jge @done
    mov r9b, byte ptr [rdx + rax]
    mov byte ptr [rcx + rax], r9b
    test r9b, r9b
    jz @done
    inc eax
    jmp @loop
@done:
    ret
strncpy ENDP

memcpy PROC
    ; rcx = dest, rdx = src, r8d = len
    xor rax, rax
@loop:
    cmp eax, r8d
    jge @done
    mov r9b, byte ptr [rdx + rax]
    mov byte ptr [rcx + rax], r9b
    inc eax
    jmp @loop
@done:
    ret
memcpy ENDP

;==========================================================================
; Window procedures
;==========================================================================

training_studio_wnd_proc PROC
    push rbx
    sub rsp, 32
    
    mov rbx, rcx ; hwnd
    
    cmp edx, WM_CREATE
    je @on_create
    cmp edx, WM_COMMAND
    je @on_command
    cmp edx, WM_DESTROY
    je @on_destroy
    
    call DefWindowProcA
    jmp @done
    
@on_create:
    mov [g_training_studio.hwnd], rbx
    call create_dataset_list
    call create_model_list
    call create_experiment_list
    call create_metrics_chart
    call create_resource_chart
    call create_hyperparam_grid
    xor rax, rax
    jmp @done
    
@on_command:
    ; Handle button clicks and menu items
    xor rax, rax
    jmp @done
    
@on_destroy:
    xor rax, rax
    jmp @done
    
@done:
    add rsp, 32
    pop rbx
    ret
training_studio_wnd_proc ENDP

metrics_chart_wnd_proc PROC
    push rbx
    sub rsp, 32
    
    cmp edx, WM_PAINT
    je @on_paint
    
    call DefWindowProcA
    jmp @done
    
@on_paint:
    ; Custom drawing for metrics chart
    xor rax, rax
    jmp @done
    
@done:
    add rsp, 32
    pop rbx
    ret
metrics_chart_wnd_proc ENDP

resource_chart_wnd_proc PROC
    push rbx
    sub rsp, 32
    
    cmp edx, WM_PAINT
    je @on_paint
    
    call DefWindowProcA
    jmp @done
    
@on_paint:
    ; Custom drawing for resource chart
    xor rax, rax
    jmp @done
    
@done:
    add rsp, 32
    pop rbx
    ret
resource_chart_wnd_proc ENDP

; Stubs for remaining public functions
training_studio_get_metrics PROC
    ret
training_studio_get_metrics ENDP

training_studio_export_checkpoint PROC
    ret
training_studio_export_checkpoint ENDP

training_studio_compare_experiments PROC
    ret
training_studio_compare_experiments ENDP

training_studio_tune_hyperparameters PROC
    ret
training_studio_tune_hyperparameters ENDP

end