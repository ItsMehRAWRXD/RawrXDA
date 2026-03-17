; RawrXD_LazyTensorPager.asm
; Layer-wise demand pager for 800B-class models on 64GB hosts
; Assemble: ml64 /c /FoLazyTensorPager.obj RawrXD_LazyTensorPager.asm

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
externdef RtlCopyMemory:proc
externdef GetCurrentProcess:proc
externdef SetProcessWorkingSetSize:proc
externdef Sleep:proc

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
FILE_MAP_WRITE       equ 2
FILE_MAP_ALL_ACCESS  equ 0F001Fh
PAGE_READWRITE       equ 4
SEC_RESERVE          equ 04000000h
MEM_COMMIT           equ 1000h
MEM_RESERVE          equ 2000h
MEM_RELEASE          equ 8000h
MEM_DECOMMIT         equ 4000h

; Configuration
MAX_RESIDENT_LAYERS  equ 8          ; ~3.6GB resident max (8*450MB)
AVX512_THRESHOLD     equ 4096       ; Use AVX-512 for copies > 4KB

; Slot size: 64 bytes for cache alignment
; Layer structure offsets (manually computed for 64-byte struct)
SLOT_LAYERID         equ 0
SLOT_HOSTADDR        equ 8
SLOT_FILEOFFSET      equ 16
SLOT_SIZEBYTES       equ 24
SLOT_LASTACCESSTICK  equ 32
SLOT_LOCKCOUNT       equ 40
SLOT_STATUS          equ 44
SLOT_THERMAL         equ 48
SLOT_SIZE            equ 64

; PagerContext structure offsets
CTX_HFILEMAPPING     equ 0
CTX_PFILEBASE        equ 8
CTX_LAYERCOUNT       equ 16
CTX_RESIDENTLAYERS   equ 24    ; 8 slots * 64 bytes = 512 bytes
CTX_THERMALLIMIT     equ 536
CTX_CURRENTTEMP      equ 540
CTX_ACCESSCOUNTER    equ 544
CTX_PAGESIZE         equ 552
CTX_HPROCESS         equ 560
CTX_SIZE             equ 600

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
    sub rsp, 68h
    
    ; Save input params
    mov r12, rcx                      ; hFile
    mov r13, rdx                      ; LayerCount
    mov r14d, r8d                     ; ThermalThreshold
    
    ; Allocate context via VirtualAlloc
    xor ecx, ecx                      ; NULL base
    mov edx, CTX_SIZE                 ; Size
    mov r8d, MEM_COMMIT or MEM_RESERVE
    mov r9d, PAGE_READWRITE
    call VirtualAlloc
    test rax, rax
    jz create_fail
    mov rbx, rax                      ; RBX = ctx
    
    ; Zero memory
    mov rdi, rbx
    mov ecx, CTX_SIZE
    xor eax, eax
    rep stosb
    
    ; Store params
    mov [rbx + CTX_HFILEMAPPING], r12
    mov [rbx + CTX_LAYERCOUNT], r13
    mov [rbx + CTX_THERMALLIMIT], r14d
    
    ; Get system page size  
    lea rcx, [rsp + 30h]              ; SYSTEM_INFO local (48 bytes)
    call GetSystemInfo
    mov eax, dword ptr [rsp + 34h]    ; dwPageSize at offset 4
    mov [rbx + CTX_PAGESIZE], rax
    
    ; Get process handle
    call GetCurrentProcess
    mov [rbx + CTX_HPROCESS], rax
    
    ; Initialize resident slots to empty (-1 = no layer)
    lea rdi, [rbx + CTX_RESIDENTLAYERS]
    mov ecx, MAX_RESIDENT_LAYERS
init_slots:
    mov qword ptr [rdi + SLOT_LAYERID], -1
    mov qword ptr [rdi + SLOT_HOSTADDR], 0
    mov dword ptr [rdi + SLOT_STATUS], 0
    add rdi, SLOT_SIZE
    dec ecx
    jnz init_slots
    
    mov rax, rbx
    
create_exit:
    add rsp, 68h
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
; LazyPager_AttachModel: Create sparse mapping for GGUF
; RCX=Context, RDX=FileSize (36GB)
; ----------------------------------------------------------------------
LazyPager_AttachModel proc
    push rbx
    push rsi
    sub rsp, 48h
    
    mov rbx, rcx                      ; Context
    mov rsi, rdx                      ; FileSize
    
    ; Create file mapping (sparse)
    mov rcx, [rbx + CTX_HFILEMAPPING] ; hFile
    xor edx, edx                      ; Security attrs (NULL)
    mov r8d, PAGE_READWRITE           ; flProtect
    
    ; MaxSize high dword
    mov r9, rsi
    shr r9, 32                        ; High 32 bits of size
    
    ; Stack args for CreateFileMappingA
    mov rax, rsi
    and eax, 0FFFFFFFFh               ; Low 32 bits
    mov qword ptr [rsp + 20h], rax    ; MaxSize low
    mov qword ptr [rsp + 28h], 0      ; lpName (NULL)
    
    call CreateFileMappingA
    test rax, rax
    jz attach_fail
    
    ; Store mapping handle (separate from file handle)
    mov [rbx + CTX_HFILEMAPPING], rax
    
    ; Map view of entire file (read-only sparse view)
    mov rcx, rax                      ; hFileMappingObject
    mov edx, FILE_MAP_READ            ; dwDesiredAccess
    xor r8d, r8d                      ; dwFileOffsetHigh
    xor r9d, r9d                      ; dwFileOffsetLow
    mov qword ptr [rsp + 20h], 0      ; dwNumberOfBytesToMap (0 = entire)
    call MapViewOfFile
    test rax, rax
    jz attach_fail
    
    mov [rbx + CTX_PFILEBASE], rax
    
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
; LazyPager_MapLayer: Bring layer into resident set (blocking)
; RCX=Context, RDX=LayerId, R8=FileOffset, R9=Size
; Returns: RAX=HostAddr or NULL
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
    sub rsp, 68h
    
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
    mov rax, [rsi + SLOT_HOSTADDR]
    jmp map_exit

not_resident:
    ; Find empty slot or LRU victim
    lea rdi, [rbx + CTX_RESIDENTLAYERS]
    mov ecx, MAX_RESIDENT_LAYERS
    mov r14, -1                       ; Best victim slot ptr
    mov r15, 0FFFFFFFFFFFFFFFFh       ; Min tick (for LRU)
    
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
    dec ecx
    jnz lru_search
    
    ; No empty slot found, evict victim at R14
    cmp r14, -1
    je map_fail                       ; Shouldn't happen
    
    ; Evict victim layer
    mov rdi, r14
    mov rcx, [rdi + SLOT_HOSTADDR]
    test rcx, rcx
    jz skip_evict
    
    ; VirtualUnlock + VirtualFree
    mov [rsp + 30h], rdi              ; Save rdi
    mov rdx, [rdi + SLOT_SIZEBYTES]
    call VirtualUnlock
    mov rdi, [rsp + 30h]              ; Restore rdi
    
    mov rcx, [rdi + SLOT_HOSTADDR]
    xor edx, edx                      ; dwSize = 0 for MEM_RELEASE
    mov r8d, MEM_RELEASE
    mov [rsp + 30h], rdi              ; Save rdi
    call VirtualFree
    mov rdi, [rsp + 30h]              ; Restore rdi
    
skip_evict:
    mov qword ptr [rdi + SLOT_LAYERID], -1
    mov qword ptr [rdi + SLOT_HOSTADDR], 0
    jmp allocate_slot

found_empty_slot:
    ; RDI already points to empty slot

allocate_slot:
    ; Align size to 64KB boundary
    mov rax, r13
    add rax, 0FFFFh
    and rax, 0FFFFFFFFFFFF0000h
    mov [rdi + SLOT_SIZEBYTES], rax
    mov r14, rax                      ; Save aligned size
    
    ; VirtualAlloc for new layer
    xor ecx, ecx                      ; lpAddress = NULL
    mov rdx, r14                      ; dwSize
    mov r8d, MEM_COMMIT or MEM_RESERVE
    mov r9d, PAGE_READWRITE
    mov [rsp + 30h], rdi              ; Save rdi
    call VirtualAlloc
    mov rdi, [rsp + 30h]              ; Restore rdi
    test rax, rax
    jz map_fail
    
    mov [rdi + SLOT_HOSTADDR], rax
    mov r15, rax                      ; Save host addr
    
    ; Copy from file mapping to resident memory
    ; Source = pFileBase + FileOffset
    mov rsi, [rbx + CTX_PFILEBASE]
    add rsi, r12                      ; Add file offset
    
    ; RtlCopyMemory(dest, src, size)
    mov rcx, r15                      ; Dest
    mov rdx, rsi                      ; Source
    mov r8, r13                       ; Size
    mov [rsp + 30h], rdi              ; Save rdi
    call RtlCopyMemory
    mov rdi, [rsp + 30h]              ; Restore rdi
    
    ; Lock pages in physical RAM (optional)
    mov rcx, r15
    mov rdx, r14
    mov [rsp + 30h], rdi              ; Save rdi
    call VirtualLock
    mov rdi, [rsp + 30h]              ; Restore rdi
    ; Ignore failure (non-critical)
    
    ; Populate slot metadata
    mov [rdi + SLOT_LAYERID], rbp
    mov [rdi + SLOT_FILEOFFSET], r12
    inc qword ptr [rbx + CTX_ACCESSCOUNTER]
    mov rax, [rbx + CTX_ACCESSCOUNTER]
    mov [rdi + SLOT_LASTACCESSTICK], rax
    mov dword ptr [rdi + SLOT_STATUS], 2   ; Resident
    mov byte ptr [rdi + SLOT_THERMAL], 0
    
    mov rax, r15                      ; Return host address
    
map_exit:
    add rsp, 68h
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
; Returns: RAX=Ptr or NULL (triggers fault-in if layer not resident)
; ----------------------------------------------------------------------
LazyPager_ReadTensor proc
    push rbx
    push rsi
    sub rsp, 28h
    
    mov rbx, rcx                      ; Context
    mov rsi, rdx                      ; LayerId
    
    ; Check thermal throttle
    mov eax, [rbx + CTX_CURRENTTEMP]
    cmp eax, 85
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
    
    ; Layer not resident - return NULL (caller should MapLayer first)
    xor eax, eax
    jmp read_exit
    
layer_found:
    ; Update LRU
    inc qword ptr [rbx + CTX_ACCESSCOUNTER]
    mov rcx, [rbx + CTX_ACCESSCOUNTER]
    mov [rax + SLOT_LASTACCESSTICK], rcx
    
    ; Return base + tensor offset
    mov rax, [rax + SLOT_HOSTADDR]
    add rax, r8                       ; Add tensor offset within layer
    
read_exit:
    add rsp, 28h
    pop rsi
    pop rbx
    ret
LazyPager_ReadTensor endp

; ----------------------------------------------------------------------
; LazyPager_ThermalThrottle: Update current temp (from sidecar)
; RCX=Context, RDX=TempCelsius
; ----------------------------------------------------------------------
LazyPager_ThermalThrottle proc
    mov [rcx + CTX_CURRENTTEMP], edx
    
    ; If temp > threshold, mark all slots as throttled
    cmp edx, [rcx + CTX_THERMALLIMIT]
    jb thermal_ok
    
    ; Set throttle flag on all slots
    lea rax, [rcx + CTX_RESIDENTLAYERS]
    mov r8d, MAX_RESIDENT_LAYERS
throttle_loop:
    mov byte ptr [rax + SLOT_THERMAL], 1
    add rax, SLOT_SIZE
    dec r8d
    jnz throttle_loop
    
thermal_ok:
    ret
LazyPager_ThermalThrottle endp

; ----------------------------------------------------------------------
; LazyPager_Destroy: Cleanup all resources
; RCX=Context
; ----------------------------------------------------------------------
LazyPager_Destroy proc
    push rbx
    push rsi
    push rdi
    sub rsp, 28h
    
    mov rbx, rcx
    test rbx, rbx
    jz destroy_done
    
    ; Unmap all resident layers
    lea rsi, [rbx + CTX_RESIDENTLAYERS]
    mov edi, MAX_RESIDENT_LAYERS
    
destroy_loop:
    mov rax, [rsi + SLOT_HOSTADDR]
    test rax, rax
    jz destroy_next
    
    ; VirtualUnlock
    mov rcx, rax
    mov rdx, [rsi + SLOT_SIZEBYTES]
    mov [rsp + 20h], rsi              ; Save rsi
    mov [rsp + 28h], rdi              ; Save rdi
    call VirtualUnlock
    mov rsi, [rsp + 20h]              ; Restore rsi
    mov edi, [rsp + 28h]              ; Restore rdi
    
    ; VirtualFree
    mov rcx, [rsi + SLOT_HOSTADDR]
    xor edx, edx
    mov r8d, MEM_RELEASE
    mov [rsp + 20h], rsi              ; Save rsi
    mov [rsp + 28h], rdi              ; Save rdi
    call VirtualFree
    mov rsi, [rsp + 20h]              ; Restore rsi
    mov edi, [rsp + 28h]              ; Restore rdi
    
destroy_next:
    add rsi, SLOT_SIZE
    dec edi
    jnz destroy_loop
    
    ; Unmap file view
    mov rcx, [rbx + CTX_PFILEBASE]
    test rcx, rcx
    jz skip_unmap
    call UnmapViewOfFile
skip_unmap:
    
    ; Free context structure
    mov rcx, rbx
    xor edx, edx
    mov r8d, MEM_RELEASE
    call VirtualFree
    
destroy_done:
    add rsp, 28h
    pop rdi
    pop rsi
    pop rbx
    ret
LazyPager_Destroy endp

end
