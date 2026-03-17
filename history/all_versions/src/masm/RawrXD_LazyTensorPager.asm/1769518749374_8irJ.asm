; RawrXD_LazyTensorPager.asm
; Layer-wise demand pager for 800B-class models on 64GB hosts
; Assemble: ml64 /c /FoLazyTensorPager.obj RawrXD_LazyTensorPager.asm

option casemap:none

; Windows API imports
extern CreateFileMappingA:proc
extern MapViewOfFile:proc
extern UnmapViewOfFile:proc
extern VirtualLock:proc
extern VirtualUnlock:proc
extern VirtualAlloc:proc
extern VirtualFree:proc
extern GetSystemInfo:proc
extern QueryPerformanceCounter:proc
extern RtlCopyMemory:proc
extern GetCurrentProcess:proc
extern SetProcessWorkingSetSize:proc
extern Sleep:proc

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
LAYER_MAGIC          equ 52585744h ; 'RXDT'

; Configuration
MAX_RESIDENT_LAYERS  equ 8          ; ~3.6GB resident max (8*450MB)
LAYER_SLOT_SIZE      equ 512*1024*1024 ; 512MB per slot (overprovision)
AVX512_THRESHOLD     equ 4096       ; Use AVX-512 for copies > 4KB

; Structure: LayerSlot (64 bytes, cache line aligned)
LayerSlot struct
    LayerId        dq ?             ; 0-79, -1=empty
    HostAddr       dq ?             ; Virtual address in process space
    FileOffset     dq ?             ; Offset in GGUF file
    SizeBytes      dq ?
    LastAccessTick dq ?             ; For LRU
    LockCount      dd ?             ; Ref counting
    Status         dd ?             ; 0=empty, 1=loading, 2=resident, 3=dirty
    ThermalBackpressure db ?        ; 0=normal, 1=throttled
    Padding        db 7 dup(?)      ; Align to 64 bytes
LayerSlot ends

; Structure: PagerContext
PagerContext struct
    hFileMapping   dq ?             ; Handle to GGUF file mapping
    pFileBase      dq ?             ; Base of mapped file (sparse)
    LayerCount     dq ?             ; 80
    ResidentLayers LayerSlot MAX_RESIDENT_LAYERS dup(<>)
    ThermalLimit   dd ?             ; 0-100 (temp threshold)
    CurrentTemp    dd ?
    AccessCounter  dq ?
    PageSize       dq ?
    hProcess       dq ?
    CriticalSection dq 5 dup(?)     ; SRWLOCK equivalent (simplified)
PagerContext ends

.code

; AVX-512 Non-temporal memcpy (64-byte streaming stores)
; RCX=dst, RDX=src, R8=len (must be 64-byte aligned)
AVX512_NTCopy proc private
    push rbx
    push rsi
    push rdi
    
    mov rsi, rdx
    mov rdi, rcx
    mov rbx, r8
    shr rbx, 6                      ; /64 bytes
    
    test rbx, rbx
    jz avx_done
    
avx_loop:
    vmovdqu64 zmm0, zmmword ptr [rsi]
    vmovntdq zmmword ptr [rdi], zmm0
    add rsi, 64
    add rdi, 64
    dec rbx
    jnz avx_loop
    
avx_done:
    sfence
    vzeroupper
    
    pop rdi
    pop rsi
    pop rbx
    ret
AVX512_NTCopy endp

; ----------------------------------------------------------------------
; LazyPager_Create: Initialize pager context
; RCX=hFile (GGUF handle), RDX=LayerCount, R8=ThermalThreshold
; Returns: RAX=Context pointer or 0
; ----------------------------------------------------------------------
LazyPager_Create proc export frame
    push rbx
    .pushreg rbx
    push rsi
    .pushreg rsi
    push rdi
    .pushreg rdi
    push r12
    .pushreg r12
    push r13
    .pushreg r13
    push r14
    .pushreg r14
    sub rsp, 60h
    .allocstack 60h
    .endprolog
    
    ; Save input params
    mov r12, rcx                      ; hFile
    mov r13, rdx                      ; LayerCount
    mov r14d, r8d                     ; ThermalThreshold
    
    ; Allocate context via VirtualAlloc
    xor rcx, rcx                      ; NULL base
    mov rdx, sizeof PagerContext      ; Size
    mov r8d, MEM_COMMIT or MEM_RESERVE
    mov r9d, PAGE_READWRITE
    call VirtualAlloc
    test rax, rax
    jz create_fail
    mov rbx, rax                      ; RBX = ctx
    
    ; Zero memory
    mov rdi, rbx
    mov rcx, sizeof PagerContext
    xor eax, eax
    rep stosb
    
    ; Store params
    mov [rbx].PagerContext.hFileMapping, r12
    mov [rbx].PagerContext.LayerCount, r13
    mov [rbx].PagerContext.ThermalLimit, r14d
    
    ; Get system page size
    lea rcx, [rsp+30h]                ; SYSTEM_INFO local (48 bytes)
    call GetSystemInfo
    mov eax, dword ptr [rsp+34h]      ; dwPageSize at offset 4
    mov [rbx].PagerContext.PageSize, rax
    
    ; Get process handle
    call GetCurrentProcess
    mov [rbx].PagerContext.hProcess, rax
    
    ; Initialize resident slots to empty (-1 = no layer)
    lea rdi, [rbx].PagerContext.ResidentLayers
    mov rcx, MAX_RESIDENT_LAYERS
init_slots:
    mov qword ptr [rdi].LayerSlot.LayerId, -1
    mov qword ptr [rdi].LayerSlot.HostAddr, 0
    mov dword ptr [rdi].LayerSlot.Status, 0
    add rdi, sizeof LayerSlot
    dec rcx
    jnz init_slots
    
    mov rax, rbx
    
create_exit:
    add rsp, 60h
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
create_fail:
    xor rax, rax
    jmp create_exit
LazyPager_Create endp

; ----------------------------------------------------------------------
; LazyPager_AttachModel: Create sparse mapping for GGUF
; RCX=Context, RDX=FileSize (36GB)
; ----------------------------------------------------------------------
LazyPager_AttachModel proc export frame
    push rbx
    .pushreg rbx
    push rsi
    .pushreg rsi
    sub rsp, 38h
    .allocstack 38h
    .endprolog
    
    mov rbx, rcx                      ; Context
    mov rsi, rdx                      ; FileSize
    
    ; Create file mapping (sparse)
    mov rcx, [rbx].PagerContext.hFileMapping ; hFile
    xor rdx, rdx                      ; Security attrs (NULL)
    mov r8d, PAGE_READWRITE           ; flProtect
    
    ; MaxSize high dword
    mov r9, rsi
    shr r9, 32                        ; High 32 bits of size
    
    ; Stack args for CreateFileMappingA
    mov qword ptr [rsp+20h], rsi      ; MaxSize low (full filesize)
    and qword ptr [rsp+20h], 0FFFFFFFFh
    mov qword ptr [rsp+28h], 0        ; lpName (NULL)
    
    call CreateFileMappingA
    test rax, rax
    jz attach_fail
    
    ; Store mapping handle (separate from file handle)
    mov [rbx].PagerContext.hFileMapping, rax
    
    ; Map view of entire file (read-only sparse view)
    mov rcx, rax                      ; hFileMappingObject
    mov edx, FILE_MAP_READ            ; dwDesiredAccess
    xor r8d, r8d                      ; dwFileOffsetHigh
    xor r9d, r9d                      ; dwFileOffsetLow
    mov qword ptr [rsp+20h], 0        ; dwNumberOfBytesToMap (0 = entire)
    call MapViewOfFile
    test rax, rax
    jz attach_fail
    
    mov [rbx].PagerContext.pFileBase, rax
    
    mov eax, 1                        ; Success
attach_exit:
    add rsp, 38h
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
LazyPager_MapLayer proc export frame
    push rbx
    .pushreg rbx
    push rbp
    .pushreg rbp
    push rsi
    .pushreg rsi
    push rdi
    .pushreg rdi
    push r12
    .pushreg r12
    push r13
    .pushreg r13
    push r14
    .pushreg r14
    push r15
    .pushreg r15
    sub rsp, 58h
    .allocstack 58h
    .endprolog
    
    mov rbx, rcx                      ; Context
    mov rbp, rdx                      ; LayerId
    mov r12, r8                       ; FileOffset
    mov r13, r9                       ; Size
    
    ; Check if already resident
    lea rsi, [rbx].PagerContext.ResidentLayers
    mov rcx, MAX_RESIDENT_LAYERS
check_existing:
    cmp [rsi].LayerSlot.LayerId, rbp
    je found_existing
    add rsi, sizeof LayerSlot
    dec rcx
    jnz check_existing
    jmp not_resident
    
found_existing:
    ; Update LRU tick
    inc qword ptr [rbx].PagerContext.AccessCounter
    mov rax, [rbx].PagerContext.AccessCounter
    mov [rsi].LayerSlot.LastAccessTick, rax
    mov rax, [rsi].LayerSlot.HostAddr
    jmp map_exit

not_resident:
    ; Find empty slot or LRU victim
    lea rdi, [rbx].PagerContext.ResidentLayers
    mov rcx, MAX_RESIDENT_LAYERS
    mov r14, -1                       ; Best victim slot ptr
    mov r15, 0FFFFFFFFFFFFFFFFh       ; Min tick (for LRU)
    
lru_search:
    cmp qword ptr [rdi].LayerSlot.LayerId, -1
    je found_empty_slot               ; Empty slot takes priority
    
    ; Compare LRU tick
    mov rax, [rdi].LayerSlot.LastAccessTick
    cmp rax, r15
    jae not_older
    mov r15, rax                      ; New minimum
    mov r14, rdi                      ; Best victim
not_older:
    add rdi, sizeof LayerSlot
    dec rcx
    jnz lru_search
    
    ; No empty slot found, evict victim at R14
    cmp r14, -1
    je map_fail                       ; Shouldn't happen
    
    ; Evict victim layer
    mov rdi, r14
    mov rcx, [rdi].LayerSlot.HostAddr
    test rcx, rcx
    jz skip_evict
    
    ; VirtualUnlock + VirtualFree
    push rdi
    mov rdx, [rdi].LayerSlot.SizeBytes
    call VirtualUnlock
    pop rdi
    
    mov rcx, [rdi].LayerSlot.HostAddr
    xor edx, edx                      ; dwSize = 0 for MEM_RELEASE
    mov r8d, MEM_RELEASE
    call VirtualFree
    
skip_evict:
    mov qword ptr [rdi].LayerSlot.LayerId, -1
    mov qword ptr [rdi].LayerSlot.HostAddr, 0
    jmp allocate_slot

found_empty_slot:
    mov rdi, rdi                      ; RDI already points to empty slot

allocate_slot:
    ; Align size to 64KB boundary
    mov rax, r13
    add rax, 0FFFFh
    and rax, 0FFFFFFFFFFFF0000h
    mov [rdi].LayerSlot.SizeBytes, rax
    mov r14, rax                      ; Save aligned size
    
    ; VirtualAlloc for new layer
    xor rcx, rcx                      ; lpAddress = NULL
    mov rdx, r14                      ; dwSize
    mov r8d, MEM_COMMIT or MEM_RESERVE
    mov r9d, PAGE_READWRITE
    call VirtualAlloc
    test rax, rax
    jz map_fail
    
    mov [rdi].LayerSlot.HostAddr, rax
    mov r15, rax                      ; Save host addr
    
    ; Copy from file mapping to resident memory
    ; Source = pFileBase + FileOffset
    mov rsi, [rbx].PagerContext.pFileBase
    add rsi, r12                      ; Add file offset
    
    ; Decide copy method
    cmp r13, AVX512_THRESHOLD
    jb use_rtlcopy
    
    ; AVX-512 copy (aligned portion)
    mov rcx, r15                      ; Dest
    mov rdx, rsi                      ; Source
    mov r8, r13
    and r8, 0FFFFFFFFFFFFFFC0h        ; Align to 64
    call AVX512_NTCopy
    jmp copy_done
    
use_rtlcopy:
    mov rcx, r15                      ; Dest
    mov rdx, rsi                      ; Source
    mov r8, r13                       ; Size
    call RtlCopyMemory
    
copy_done:
    ; Lock pages in physical RAM
    mov rcx, r15
    mov rdx, r14
    call VirtualLock
    ; Ignore failure (non-critical)
    
    ; Populate slot metadata
    mov [rdi].LayerSlot.LayerId, rbp
    mov [rdi].LayerSlot.FileOffset, r12
    inc qword ptr [rbx].PagerContext.AccessCounter
    mov rax, [rbx].PagerContext.AccessCounter
    mov [rdi].LayerSlot.LastAccessTick, rax
    mov dword ptr [rdi].LayerSlot.Status, 2   ; Resident
    mov byte ptr [rdi].LayerSlot.ThermalBackpressure, 0
    
    mov rax, r15                      ; Return host address
    
map_exit:
    add rsp, 58h
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
    xor rax, rax
    jmp map_exit
LazyPager_MapLayer endp

; ----------------------------------------------------------------------
; LazyPager_ReadTensor: Get resident pointer to tensor in layer
; RCX=Context, RDX=LayerId, R8=TensorOffsetInLayer, R9=Size
; Returns: RAX=Ptr or NULL (triggers fault-in if layer not resident)
; ----------------------------------------------------------------------
LazyPager_ReadTensor proc export frame
    push rbx
    .pushreg rbx
    push rsi
    .pushreg rsi
    sub rsp, 28h
    .allocstack 28h
    .endprolog
    
    mov rbx, rcx                      ; Context
    mov rsi, rdx                      ; LayerId
    
    ; Check thermal throttle
    mov eax, [rbx].PagerContext.CurrentTemp
    cmp eax, 85
    jb temp_ok
    
    ; Throttle: brief sleep
    mov ecx, 1                        ; 1ms
    call Sleep
    
temp_ok:
    ; Find layer in resident set
    lea rax, [rbx].PagerContext.ResidentLayers
    mov rcx, MAX_RESIDENT_LAYERS
find_layer:
    cmp [rax].LayerSlot.LayerId, rsi
    je layer_found
    add rax, sizeof LayerSlot
    dec rcx
    jnz find_layer
    
    ; Layer not resident - return NULL (caller should MapLayer first)
    xor rax, rax
    jmp read_exit
    
layer_found:
    ; Update LRU
    inc qword ptr [rbx].PagerContext.AccessCounter
    mov rcx, [rbx].PagerContext.AccessCounter
    mov [rax].LayerSlot.LastAccessTick, rcx
    
    ; Return base + tensor offset
    mov rax, [rax].LayerSlot.HostAddr
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
LazyPager_ThermalThrottle proc export frame
    .endprolog
    
    mov [rcx].PagerContext.CurrentTemp, edx
    
    ; If temp > threshold, mark all slots as throttled
    cmp edx, [rcx].PagerContext.ThermalLimit
    jb thermal_ok
    
    ; Set throttle flag on all slots
    lea rax, [rcx].PagerContext.ResidentLayers
    mov r8d, MAX_RESIDENT_LAYERS
throttle_loop:
    mov byte ptr [rax].LayerSlot.ThermalBackpressure, 1
    add rax, sizeof LayerSlot
    dec r8d
    jnz throttle_loop
    
thermal_ok:
    ret
LazyPager_ThermalThrottle endp

; ----------------------------------------------------------------------
; LazyPager_Destroy: Cleanup all resources
; RCX=Context
; ----------------------------------------------------------------------
LazyPager_Destroy proc export frame
    push rbx
    .pushreg rbx
    push rsi
    .pushreg rsi
    push rdi
    .pushreg rdi
    sub rsp, 28h
    .allocstack 28h
    .endprolog
    
    mov rbx, rcx
    test rbx, rbx
    jz destroy_done
    
    ; Unmap all resident layers
    lea rsi, [rbx].PagerContext.ResidentLayers
    mov rdi, MAX_RESIDENT_LAYERS
    
destroy_loop:
    mov rax, [rsi].LayerSlot.HostAddr
    test rax, rax
    jz destroy_next
    
    ; VirtualUnlock
    mov rcx, rax
    mov rdx, [rsi].LayerSlot.SizeBytes
    push rsi
    push rdi
    call VirtualUnlock
    pop rdi
    pop rsi
    
    ; VirtualFree
    mov rcx, [rsi].LayerSlot.HostAddr
    xor edx, edx
    mov r8d, MEM_RELEASE
    push rsi
    push rdi
    call VirtualFree
    pop rdi
    pop rsi
    
destroy_next:
    add rsi, sizeof LayerSlot
    dec rdi
    jnz destroy_loop
    
    ; Unmap file view
    mov rcx, [rbx].PagerContext.pFileBase
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
