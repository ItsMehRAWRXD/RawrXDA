; src/direct_io/jit_lba_mapper.asm
; ═══════════════════════════════════════════════════════════════════════════════
; RAWRXD v1.2.0 — JIT-LBA MAPPER (Zero-FS Direct Sector Access)
; ═══════════════════════════════════════════════════════════════════════════════
; PURPOSE: Bypass filesystem entirely. Map tensor IDs directly to physical LBAs.
; DEPENDENCIES: NVMe controller MMIO base, pre-generated .jitmap file
; AUTHOR: RawrXD Sovereign Architect
; ═══════════════════════════════════════════════════════════════════════════════

.data

; ─────────────────────────────────────────────────────────────────────────────
; JITMAP HEADER STRUCTURE (loaded from .jitmap file or embedded in GGUF)
; ─────────────────────────────────────────────────────────────────────────────
; Offset  Size    Field
; 0x00    4       Magic ("JLBA")
; 0x04    4       Version (0x00010200 for v1.2.0)
; 0x08    4       EntryCount
; 0x0C    4       Reserved
; 0x10    N*32    Entries (see JitMapEntry below)
;
; JitMapEntry (32 bytes each):
;   0x00  8   TensorUID (hash of tensor name)
;   0x08  1   DriveIndex (0-4 for 5-drive grid)
;   0x09  3   Reserved
;   0x0C  4   SectorCount
;   0x10  8   StartLBA
;   0x18  8   Reserved
; ─────────────────────────────────────────────────────────────────────────────

JITMAP_MAGIC        EQU 'ABLJ'      ; "JLBA" little-endian
JITMAP_VERSION      EQU 00010200h
JITMAP_ENTRY_SIZE   EQU 32
MAX_JITMAP_ENTRIES  EQU 65536       ; 64K tensors max (2MB table)

; NVMe Controller MMIO Bases (populated by calibration)
; These are PCIe BAR0 addresses for each NVMe controller
ALIGN 8
g_NVMeBase          QWORD 5 DUP(0)  ; [Drive0, Drive1, Drive2, Drive3, Drive4]
g_NVMeDoorbellBase  QWORD 5 DUP(0)  ; Submission Queue Tail Doorbell per drive
g_NVMeSQBase        QWORD 5 DUP(0)  ; Submission Queue base address
g_NVMeSQTail        DWORD 5 DUP(0)  ; Current SQ tail index
g_NVMeSQSize        DWORD 5 DUP(0)  ; SQ depth per controller

; JIT Map Table (loaded at init)
ALIGN 8
g_JitMapHeader      BYTE 16 DUP(0)
g_JitMapEntries     BYTE (MAX_JITMAP_ENTRIES * JITMAP_ENTRY_SIZE) DUP(0)
g_JitMapCount       DWORD 0

; Doorbell stride (typically 4 bytes, but can vary)
NVME_DOORBELL_STRIDE EQU 4

.code

; ═══════════════════════════════════════════════════════════════════════════════
; JitLBA_Init — Initialize the JIT-LBA mapper from a .jitmap file
; ═══════════════════════════════════════════════════════════════════════════════
; INPUT:  RCX = pointer to .jitmap data (memory-mapped or loaded)
;         RDX = size of .jitmap data
; OUTPUT: RAX = 0 on success, error code on failure
; ═══════════════════════════════════════════════════════════════════════════════
JitLBA_Init PROC
    push rbx
    push rsi
    push rdi
    
    ; Validate magic
    mov eax, [rcx]
    cmp eax, JITMAP_MAGIC
    jne _init_bad_magic
    
    ; Validate version
    mov eax, [rcx + 4]
    cmp eax, JITMAP_VERSION
    jb _init_old_version
    
    ; Load entry count
    mov eax, [rcx + 8]
    cmp eax, MAX_JITMAP_ENTRIES
    ja _init_too_many
    mov [g_JitMapCount], eax
    
    ; Copy header
    lea rdi, [g_JitMapHeader]
    mov rsi, rcx
    mov ecx, 16
    rep movsb
    
    ; Copy entries
    lea rdi, [g_JitMapEntries]
    ; rsi already points to rcx+16 after header copy
    mov eax, [g_JitMapCount]
    imul eax, JITMAP_ENTRY_SIZE
    mov ecx, eax
    rep movsb
    
    xor rax, rax        ; Success
    jmp _init_done
    
_init_bad_magic:
    mov rax, 1
    jmp _init_done
    
_init_old_version:
    mov rax, 2
    jmp _init_done
    
_init_too_many:
    mov rax, 3
    
_init_done:
    pop rdi
    pop rsi
    pop rbx
    ret
JitLBA_Init ENDP

; ═══════════════════════════════════════════════════════════════════════════════
; JitLBA_Lookup — Find LBA mapping for a tensor UID
; ═══════════════════════════════════════════════════════════════════════════════
; INPUT:  RCX = TensorUID (8-byte hash)
; OUTPUT: RAX = pointer to JitMapEntry, or 0 if not found
;         On success: [RAX+8]=DriveIndex, [RAX+16]=StartLBA, [RAX+12]=SectorCount
; ═══════════════════════════════════════════════════════════════════════════════
JitLBA_Lookup PROC
    push rbx
    push rsi
    
    mov rsi, rcx                    ; TensorUID to find
    lea rbx, [g_JitMapEntries]      ; Entry table base
    mov ecx, [g_JitMapCount]        ; Entry count
    test ecx, ecx
    jz _lookup_notfound
    
_lookup_loop:
    cmp rsi, [rbx]                  ; Compare TensorUID
    je _lookup_found
    add rbx, JITMAP_ENTRY_SIZE
    dec ecx
    jnz _lookup_loop
    
_lookup_notfound:
    xor rax, rax
    jmp _lookup_done
    
_lookup_found:
    mov rax, rbx
    
_lookup_done:
    pop rsi
    pop rbx
    ret
JitLBA_Lookup ENDP

; ═══════════════════════════════════════════════════════════════════════════════
; JitLBA_SubmitRead — Submit NVMe Read command directly to controller
; ═══════════════════════════════════════════════════════════════════════════════
; INPUT:  RCX = DriveIndex (0-4)
;         RDX = StartLBA
;         R8  = SectorCount
;         R9  = DMA destination address (physical or IOMMU-mapped)
; OUTPUT: RAX = Command ID (for completion tracking), or -1 on failure
; ═══════════════════════════════════════════════════════════════════════════════
JitLBA_SubmitRead PROC
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    push r14
    push r15
    
    ; Validate drive index
    cmp rcx, 5
    jae _submit_bad_drive
    
    mov r12, rcx            ; DriveIndex
    mov r13, rdx            ; StartLBA
    mov r14, r8             ; SectorCount
    mov r15, r9             ; DMA address
    
    ; Get SQ base and tail for this drive
    lea rax, [g_NVMeSQBase]
    mov rsi, [rax + r12*8]  ; SQ base address
    test rsi, rsi
    jz _submit_not_init
    
    lea rax, [g_NVMeSQTail]
    mov ebx, [rax + r12*4]  ; Current tail
    
    ; Calculate SQE address: SQ_base + (tail * 64)
    ; NVMe SQE is 64 bytes
    mov rax, rbx
    shl rax, 6              ; * 64
    add rsi, rax            ; RSI = SQE address
    
    ; Build NVMe Read Command (Opcode 0x02)
    ; ───────────────────────────────────────
    ; DW0: Opcode=0x02, FUSE=0, PSDT=0, CID=tail
    mov eax, 00000002h      ; Opcode 0x02 = Read
    or eax, ebx             ; CID = tail index (bits 16-31... actually let's simplify)
    shl ebx, 16
    or eax, ebx
    mov [rsi], eax          ; CDW0
    
    ; DW1: NSID = 1 (namespace 1)
    mov DWORD PTR [rsi + 4], 1
    
    ; DW2-3: Reserved
    mov QWORD PTR [rsi + 8], 0
    
    ; DW4-5: MPTR (metadata pointer) = 0
    mov QWORD PTR [rsi + 16], 0
    
    ; DW6-9: PRP1/PRP2 (Data Pointer)
    ; PRP1 = DMA destination
    mov [rsi + 24], r15     ; PRP1
    
    ; PRP2 = 0 for single-page reads, or PRP list for larger
    ; For simplicity, assume single contiguous buffer
    xor rax, rax
    mov [rsi + 32], rax     ; PRP2
    
    ; DW10-11: Starting LBA
    mov [rsi + 40], r13     ; SLBA (64-bit)
    
    ; DW12: NLB (Number of Logical Blocks - 1), etc.
    mov eax, r14d
    dec eax                 ; NLB is 0-based
    mov [rsi + 48], eax     ; CDW12: NLB
    
    ; DW13-15: Optional flags (zeroed)
    mov DWORD PTR [rsi + 52], 0
    mov DWORD PTR [rsi + 56], 0
    mov DWORD PTR [rsi + 60], 0
    
    ; Increment SQ tail (with wrap)
    lea rax, [g_NVMeSQTail]
    mov ebx, [rax + r12*4]
    inc ebx
    lea rcx, [g_NVMeSQSize]
    mov ecx, [rcx + r12*4]
    cmp ebx, ecx
    jb _submit_no_wrap
    xor ebx, ebx            ; Wrap to 0
_submit_no_wrap:
    mov [rax + r12*4], ebx
    
    ; Ring the doorbell!
    ; Doorbell address = DoorbellBase + (QID * 2 * stride)
    ; For SQ0, QID=0, so just DoorbellBase
    lea rax, [g_NVMeDoorbellBase]
    mov rdi, [rax + r12*8]
    mov [rdi], ebx          ; Write new tail to doorbell
    
    ; Return command ID
    lea rax, [g_NVMeSQTail]
    mov eax, [rax + r12*4]
    dec eax                 ; CID was the old tail
    jmp _submit_done
    
_submit_bad_drive:
    mov rax, -1
    jmp _submit_done
    
_submit_not_init:
    mov rax, -2
    
_submit_done:
    pop r15
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
JitLBA_SubmitRead ENDP

; ═══════════════════════════════════════════════════════════════════════════════
; JitLBA_BurstTensor — High-level: lookup + submit read for a tensor
; ═══════════════════════════════════════════════════════════════════════════════
; INPUT:  RCX = TensorUID
;         RDX = DMA destination buffer
; OUTPUT: RAX = Command ID, or negative on error
; ═══════════════════════════════════════════════════════════════════════════════
JitLBA_BurstTensor PROC
    push rbx
    push r12
    
    mov r12, rdx            ; Save destination
    
    ; Lookup tensor
    call JitLBA_Lookup
    test rax, rax
    jz _burst_not_found
    
    ; Extract mapping
    mov rbx, rax            ; Entry pointer
    movzx ecx, BYTE PTR [rbx + 8]   ; DriveIndex
    mov rdx, [rbx + 16]             ; StartLBA
    mov r8d, [rbx + 12]             ; SectorCount
    mov r9, r12                     ; DMA dest
    
    ; Submit the read
    call JitLBA_SubmitRead
    jmp _burst_done
    
_burst_not_found:
    mov rax, -100           ; Tensor not in map
    
_burst_done:
    pop r12
    pop rbx
    ret
JitLBA_BurstTensor ENDP

; ═══════════════════════════════════════════════════════════════════════════════
; JitLBA_SetDriveMMIO — Configure NVMe controller addresses (called from C++)
; ═══════════════════════════════════════════════════════════════════════════════
; INPUT:  RCX = DriveIndex (0-4)
;         RDX = BAR0 base address
;         R8  = Doorbell base offset
;         R9  = SQ base (virtual address of submission queue)
; STACK:  [rsp+40] = SQ size (depth)
; OUTPUT: RAX = 0 on success
; ═══════════════════════════════════════════════════════════════════════════════
JitLBA_SetDriveMMIO PROC
    cmp rcx, 5
    jae _setdrive_bad
    
    lea rax, [g_NVMeBase]
    mov [rax + rcx*8], rdx
    
    ; Doorbell = BAR0 + 0x1000 + doorbell_offset
    add rdx, 1000h
    add rdx, r8
    lea rax, [g_NVMeDoorbellBase]
    mov [rax + rcx*8], rdx
    
    lea rax, [g_NVMeSQBase]
    mov [rax + rcx*8], r9
    
    ; SQ size from stack
    mov eax, [rsp + 40]
    lea rdx, [g_NVMeSQSize]
    mov [rdx + rcx*4], eax
    
    ; Initialize tail to 0
    lea rax, [g_NVMeSQTail]
    mov DWORD PTR [rax + rcx*4], 0
    
    xor rax, rax
    ret
    
_setdrive_bad:
    mov rax, 1
    ret
JitLBA_SetDriveMMIO ENDP

; ═══════════════════════════════════════════════════════════════════════════════
; JitLBA_HashTensorName — Generate TensorUID from name string
; ═══════════════════════════════════════════════════════════════════════════════
; INPUT:  RCX = pointer to null-terminated tensor name
; OUTPUT: RAX = 64-bit hash (TensorUID)
; Uses FNV-1a for speed
; ═══════════════════════════════════════════════════════════════════════════════
FNV_OFFSET_BASIS EQU 0CBF29CE484222325h
FNV_PRIME        EQU 100000001B3h

JitLBA_HashTensorName PROC
    push rbx
    
    mov rax, FNV_OFFSET_BASIS
    mov rbx, FNV_PRIME
    
_hash_loop:
    movzx edx, BYTE PTR [rcx]
    test dl, dl
    jz _hash_done
    
    xor al, dl              ; XOR with byte
    imul rax, rbx           ; Multiply by prime
    inc rcx
    jmp _hash_loop
    
_hash_done:
    pop rbx
    ret
JitLBA_HashTensorName ENDP

END
