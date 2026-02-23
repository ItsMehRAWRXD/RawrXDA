; RawrXD_LazyTensorPager.asm
; Layer-wise demand pager for 800B-class models on 64GB hosts
; Assemble: ml64 /c /FoLazyTensorPager.obj RawrXD_LazyTensorPager.asm
; 
; FIXED: Uses per-layer MapViewOfFile instead of mapping entire file
;        This allows 36GB+ models to work without 36GB virtual address reservation

option casemap:none

; Windows API imports
externdef CreateFileMappingA:proc
externdef MapViewOfFile:proc
externdef UnmapViewOfFile:proc
externdef VirtualLock:proc
externdef VirtualUnlock:proc
externdef VirtualAlloc:proc
externdef VirtualFree:proc
externdef GetSystemInfo:proc
externdef QueryPerformanceCounter:proc
externdef GetCurrentProcess:proc
externdef SetProcessWorkingSetSize:proc
externdef Sleep:proc
externdef CloseHandle:proc

; Exports
public LazyPager_Create
public LazyPager_Destroy
public LazyPager_AttachModel
public LazyPager_MapLayer
public LazyPager_ReadTensor
public LazyPager_ThermalThrottle

; Constants
INVALID_HANDLE_VALUE equ -1
FILE_MAP_READ        equ 4
PAGE_READONLY        equ 2

; Configuration
MAX_RESIDENT_LAYERS  equ 8          ; ~3.6GB resident max (8*450MB)

; Slot size: 64 bytes for cache alignment
; Layer structure offsets (manually computed for 64-byte struct)
SLOT_LAYERID         equ 0
SLOT_PVIEW           equ 8          ; MapViewOfFile pointer (per-layer mapping)
SLOT_FILEOFFSET      equ 16
SLOT_SIZEBYTES       equ 24
SLOT_LASTACCESSTICK  equ 32
SLOT_STATUS          equ 40
SLOT_SIZE            equ 64

; PagerContext structure offsets
CTX_HFILE            equ 0          ; Original file handle
CTX_HFILEMAPPING     equ 8          ; File mapping handle (created in AttachModel)
CTX_FILESIZE         equ 16         ; Total file size in bytes
CTX_LAYERCOUNT       equ 24
CTX_RESIDENTLAYERS   equ 32         ; 8 slots * 64 bytes = 512 bytes
CTX_THERMALLIMIT     equ 544
CTX_CURRENTTEMP      equ 548
CTX_ACCESSCOUNTER    equ 552
CTX_PAGESIZE         equ 560
CTX_HPROCESS         equ 568
CTX_SIZE             equ 608

.code

; ----------------------------------------------------------------------
; LazyPager_Create: Initialize pager context
; RCX=hFile (GGUF handle), RDX=LayerCount, R8=ThermalThreshold
; Returns: RAX=Context pointer or 0
; ----------------------------------------------------------------------
LazyPager_Create proc
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    push r14
    sub rsp, 88h
    
    ; Save input params
    mov r12, rcx                      ; hFile
    mov r13, rdx                      ; LayerCount
    mov r14d, r8d                     ; ThermalThreshold
    
    ; Allocate context via VirtualAlloc
    xor ecx, ecx                      ; lpAddress = NULL
    mov edx, CTX_SIZE                 ; dwSize
    mov r8d, 3000h                    ; MEM_COMMIT | MEM_RESERVE
    mov r9d, 4                        ; PAGE_READWRITE
    call VirtualAlloc
    
    test rax, rax
    jz create_fail
    mov rbx, rax                      ; RBX = ctx
    
    ; Zero memory (simple loop)
    mov rdi, rbx
    mov ecx, CTX_SIZE
    xor eax, eax
@@:
    mov byte ptr [rdi], al
    inc rdi
    dec ecx
    jnz @B
    
    ; Store params
    mov [rbx + CTX_HFILE], r12        ; Store original file handle
    mov qword ptr [rbx + CTX_HFILEMAPPING], 0  ; Will be set in AttachModel
    mov qword ptr [rbx + CTX_FILESIZE], 0     ; Will be set in AttachModel
    mov [rbx + CTX_LAYERCOUNT], r13
    mov [rbx + CTX_THERMALLIMIT], r14d
    
    ; Get system page size (SYSTEM_INFO at [rsp+30h])
    lea rcx, [rsp + 30h]              ; SYSTEM_INFO buffer
    call GetSystemInfo
    mov eax, dword ptr [rsp + 34h]    ; dwPageSize at offset 4 in SYSTEM_INFO
    mov [rbx + CTX_PAGESIZE], rax
    
    ; Get process handle
    call GetCurrentProcess
    mov [rbx + CTX_HPROCESS], rax
    
    ; Initialize resident slots to empty (-1 = no layer)
    lea rdi, [rbx + CTX_RESIDENTLAYERS]
    mov ecx, MAX_RESIDENT_LAYERS
init_slots:
    mov qword ptr [rdi + SLOT_LAYERID], -1
    mov qword ptr [rdi + SLOT_PVIEW], 0
    mov qword ptr [rdi + SLOT_FILEOFFSET], 0
    mov qword ptr [rdi + SLOT_SIZEBYTES], 0
    mov qword ptr [rdi + SLOT_STATUS], 0
    add rdi, SLOT_SIZE
    dec ecx
    jnz init_slots
    
    mov rax, rbx
    
create_exit:
    add rsp, 88h
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
create_fail:
    xor eax, eax
    jmp create_exit
LazyPager_Create endp

; ----------------------------------------------------------------------
; LazyPager_Destroy: Free pager context and all mapped views
; RCX=Context
; ----------------------------------------------------------------------
LazyPager_Destroy proc
    push rbx
    push rsi
    push rdi
    sub rsp, 30h
    
    test rcx, rcx
    jz destroy_exit
    mov rbx, rcx                      ; Context
    
    ; Unmap all resident layer views
    lea rsi, [rbx + CTX_RESIDENTLAYERS]
    mov edi, MAX_RESIDENT_LAYERS
unmap_loop:
    mov rcx, [rsi + SLOT_PVIEW]
    test rcx, rcx
    jz skip_unmap
    call UnmapViewOfFile
    mov qword ptr [rsi + SLOT_PVIEW], 0
skip_unmap:
    add rsi, SLOT_SIZE
    dec edi
    jnz unmap_loop
    
    ; Close file mapping handle (if created)
    mov rcx, [rbx + CTX_HFILEMAPPING]
    test rcx, rcx
    jz skip_close_mapping
    call CloseHandle
skip_close_mapping:
    
    ; Free context memory
    mov rcx, rbx                      ; lpAddress
    xor edx, edx                      ; dwSize = 0 for MEM_RELEASE
    mov r8d, 8000h                    ; MEM_RELEASE
    call VirtualFree
    
destroy_exit:
    add rsp, 30h
    pop rdi
    pop rsi
    pop rbx
    ret
LazyPager_Destroy endp

; ----------------------------------------------------------------------
; LazyPager_AttachModel: Create file mapping handle (NO full view mapping)
; RCX=Context, RDX=FileSize (36GB)
; Returns: 1=success, 0=failure
; ----------------------------------------------------------------------
LazyPager_AttachModel proc
    push rbx
    push rsi
    sub rsp, 48h
    
    mov rbx, rcx                      ; Context
    mov rsi, rdx                      ; FileSize
    
    ; Store file size
    mov [rbx + CTX_FILESIZE], rsi
    
    ; Create file mapping (read-only, DO NOT map entire view yet)
    ; MapViewOfFile will be called per-layer in MapLayer
    mov rcx, [rbx + CTX_HFILE]        ; hFile
    xor edx, edx                      ; Security attrs (NULL)
    mov r8d, PAGE_READONLY            ; flProtect = PAGE_READONLY (2)
    
    ; MaxSize high dword (r9)
    mov r9, rsi
    shr r9, 32                        ; High 32 bits of size
    
    ; Stack args for CreateFileMappingA
    ; [rsp+20h] = MaxSize low dword
    ; [rsp+28h] = lpName (NULL)
    mov rax, rsi
    and rax, 0FFFFFFFFh               ; Clean low 32 bits
    mov qword ptr [rsp + 20h], rax    ; MaxSize low (arg 5)
    mov qword ptr [rsp + 28h], 0      ; lpName (NULL) (arg 6)
    
    call CreateFileMappingA
    test rax, rax
    jz attach_fail
    
    ; Store mapping handle
    mov [rbx + CTX_HFILEMAPPING], rax
    
    ; SUCCESS - Do NOT map entire view here
    ; Individual layers will be mapped on demand in LazyPager_MapLayer
    
    mov eax, 1                        ; Success
attach_exit:
    add rsp, 48h
    pop rsi
    pop rbx
    ret
attach_fail:
    xor eax, eax
    jmp attach_exit
LazyPager_AttachModel endp

; ----------------------------------------------------------------------
; LazyPager_MapLayer: Bring layer into resident set via MapViewOfFile
; RCX=Context, RDX=LayerId, R8=FileOffset, R9=Size
; Returns: RAX=View pointer or NULL
; ----------------------------------------------------------------------
LazyPager_MapLayer proc
    push rbx
    push rbp
    push rsi
    push rdi
    push r12
    push r13
    push r14
    push r15
    sub rsp, 78h
    
    mov rbx, rcx                      ; Context
    mov rbp, rdx                      ; LayerId
    mov r12, r8                       ; FileOffset
    mov r13, r9                       ; Size
    
    ; Check if already resident
    lea rsi, [rbx + CTX_RESIDENTLAYERS]
    mov ecx, MAX_RESIDENT_LAYERS
check_existing:
    cmp qword ptr [rsi + SLOT_LAYERID], rbp
    je found_existing
    add rsi, SLOT_SIZE
    dec ecx
    jnz check_existing
    jmp not_resident
    
found_existing:
    ; Update LRU tick
    inc qword ptr [rbx + CTX_ACCESSCOUNTER]
    mov rax, [rbx + CTX_ACCESSCOUNTER]
    mov [rsi + SLOT_LASTACCESSTICK], rax
    mov rax, [rsi + SLOT_PVIEW]       ; Return existing view pointer
    jmp map_exit

not_resident:
    ; Find empty slot or LRU victim
    lea rdi, [rbx + CTX_RESIDENTLAYERS]
    mov ecx, MAX_RESIDENT_LAYERS
    mov r14, 0                        ; Best victim slot ptr (0 = none found)
    mov r15, 0FFFFFFFFFFFFFFFFh       ; Min tick (for LRU)
    xor esi, esi                      ; Slot index
    
lru_search:
    cmp qword ptr [rdi + SLOT_LAYERID], -1
    je found_empty_slot               ; Empty slot takes priority
    
    ; Compare LRU tick
    mov rax, [rdi + SLOT_LASTACCESSTICK]
    cmp rax, r15
    jae not_older
    mov r15, rax                      ; New minimum
    mov r14, rdi                      ; Best victim
not_older:
    add rdi, SLOT_SIZE
    inc esi
    cmp esi, MAX_RESIDENT_LAYERS
    jb lru_search
    
    ; No empty slot found, must evict victim at R14
    test r14, r14
    jz map_fail                       ; Shouldn't happen
    
    ; Evict victim layer - unmap its view
    mov rdi, r14
    mov rcx, [rdi + SLOT_PVIEW]
    test rcx, rcx
    jz skip_evict
    
    ; UnmapViewOfFile to release the old layer
    mov [rsp + 40h], rdi              ; Save rdi
    call UnmapViewOfFile
    mov rdi, [rsp + 40h]              ; Restore rdi
    
skip_evict:
    mov qword ptr [rdi + SLOT_LAYERID], -1
    mov qword ptr [rdi + SLOT_PVIEW], 0
    jmp allocate_slot

found_empty_slot:
    ; RDI already points to empty slot

allocate_slot:
    ; Map view for this specific layer using MapViewOfFile
    ; MapViewOfFile(hFileMappingObject, dwDesiredAccess, dwFileOffsetHigh, dwFileOffsetLow, dwNumberOfBytesToMap)
    
    mov rcx, [rbx + CTX_HFILEMAPPING] ; hFileMappingObject
    mov edx, FILE_MAP_READ            ; dwDesiredAccess = FILE_MAP_READ
    
    ; File offset high 32 bits
    mov r8, r12                       ; FileOffset
    shr r8, 32                        ; High 32 bits
    
    ; File offset low 32 bits
    mov r9d, r12d                     ; Low 32 bits (truncated to 32-bit)
    
    ; dwNumberOfBytesToMap on stack
    mov qword ptr [rsp + 20h], r13    ; Size to map
    
    mov [rsp + 40h], rdi              ; Save slot pointer
    call MapViewOfFile
    mov rdi, [rsp + 40h]              ; Restore slot pointer
    
    test rax, rax
    jz map_fail                       ; MapViewOfFile failed
    
    ; Store the view pointer in slot
    mov [rdi + SLOT_PVIEW], rax
    mov r15, rax                      ; Save for return
    
    ; Populate slot metadata
    mov [rdi + SLOT_LAYERID], rbp     ; LayerId
    mov [rdi + SLOT_FILEOFFSET], r12  ; FileOffset
    mov [rdi + SLOT_SIZEBYTES], r13   ; Size
    inc qword ptr [rbx + CTX_ACCESSCOUNTER]
    mov rax, [rbx + CTX_ACCESSCOUNTER]
    mov [rdi + SLOT_LASTACCESSTICK], rax
    mov qword ptr [rdi + SLOT_STATUS], 1   ; Resident
    
    mov rax, r15                      ; Return view address
    
map_exit:
    add rsp, 78h
    pop r15
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbp
    pop rbx
    ret
map_fail:
    xor eax, eax
    jmp map_exit
LazyPager_MapLayer endp

; ----------------------------------------------------------------------
; LazyPager_ReadTensor: Get resident pointer to tensor in layer
; RCX=Context, RDX=LayerId, R8=TensorOffsetInLayer, R9=Size
; Returns: RAX=Ptr or NULL
; ----------------------------------------------------------------------
LazyPager_ReadTensor proc
    push rbx
    push rsi
    sub rsp, 28h
    
    mov rbx, rcx                      ; Context
    mov rsi, rdx                      ; LayerId
    
    ; Check thermal throttle
    mov eax, [rbx + CTX_CURRENTTEMP]
    cmp eax, [rbx + CTX_THERMALLIMIT]
    jb temp_ok
    
    ; Throttle: brief sleep
    mov ecx, 1                        ; 1ms
    call Sleep
    
temp_ok:
    ; Find layer in resident set
    lea rax, [rbx + CTX_RESIDENTLAYERS]
    mov ecx, MAX_RESIDENT_LAYERS
find_layer:
    cmp qword ptr [rax + SLOT_LAYERID], rsi
    je layer_found
    add rax, SLOT_SIZE
    dec ecx
    jnz find_layer
    
    ; Layer not resident
    xor eax, eax
    jmp read_exit

layer_found:
    ; Return view_base + offset_in_layer
    mov rax, [rax + SLOT_PVIEW]
    test rax, rax
    jz read_exit                      ; No view mapped
    add rax, r8                       ; Add tensor offset within layer
    
read_exit:
    add rsp, 28h
    pop rsi
    pop rbx
    ret
LazyPager_ReadTensor endp

; ----------------------------------------------------------------------
; LazyPager_ThermalThrottle: Update current temperature for backpressure
; RCX=Context, RDX=CurrentTempCelsius
; ----------------------------------------------------------------------
LazyPager_ThermalThrottle proc
    test rcx, rcx
    jz throttle_exit
    mov [rcx + CTX_CURRENTTEMP], edx
throttle_exit:
    ret
LazyPager_ThermalThrottle endp

END
