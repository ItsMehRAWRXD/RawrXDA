; ============================================================================
; RawrXD_ModuleEngine.asm - PEB/Ldr Module Enumeration (Phase 2.2)
; ============================================================================
.code

; Structure Offsets (Windows x64)
PEB_LDR_DATA_OFFSET         EQU 018h    ; PEB->Ldr
LDR_IN_MEMORY_ORDER_OFFSET  EQU 020h    ; PEB_LDR_DATA->InMemoryOrderModuleList (LIST_ENTRY)
LDR_MODULE_BASE_OFFSET      EQU 030h    ; LDR_DATA_TABLE_ENTRY->DllBase (relative to InLoadOrder)
                                        ; Note: InMemoryOrder links are at offset 0x10 in LDR_DATA_TABLE_ENTRY
                                        ; So DllBase is at offset 0x30 from the start of the struct, 
                                        ; which is +0x20 from the InMemoryOrder link.

; ----------------------------------------------------------------------------
; rawrxd_enumerate_modules_peb
; RCX = Callback function: void callback(u64 base, u32 size, u16 name_len, u16* name_ptr, void* context)
; RDX = Context
; ----------------------------------------------------------------------------
rawrxd_enumerate_modules_peb PROC
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    
    mov r12, rcx                ; Save callback in r12
    mov r13, rdx                ; Save context in r13
    
    ; Get PEB via GS:[0x60]
    mov rax, gs:[60h]
    mov rax, [rax + 18h]        ; PEB->Ldr
    mov rsi, [rax + 20h]        ; Ldr->InMemoryOrderModuleList.Flink
    mov rbx, rsi                ; Save start of list
    
L_module_loop:
    ; rsi points to LDR_DATA_TABLE_ENTRY.InMemoryOrderLinks
    ; Offset 0x20 from InMemoryOrderLinks (0x30 from struct start) is DllBase
    mov rcx, [rsi + 20h]        ; rcx = DllBase
    
    ; Offset 0x30 from InMemoryOrderLinks (0x40 from struct start) is SizeOfImage
    mov edx, [rsi + 30h]        ; edx = SizeOfImage (u32)
    
    ; Name is at offset 0x48 from InMemoryOrderLinks (0x58 from struct start)
    ; struct UNICODE_STRING { u16 Length, u16 MaxLength, u64 Buffer }
    mov r8w, word ptr [rsi + 48h]       ; r8w = Length (bytes)
    mov r9,  qword ptr [rsi + 50h]      ; r9 = Buffer (ptr)
    
    ; Prepare callback:
    ; RCX = DllBase (u64)
    ; RDX = SizeOfImage (u32)
    ; R8  = NameLength (u16)
    ; R9  = NameBuffer (ptr)
    ; STACK/RBP-offset = Context (not needed for simple callback, let's use R10/R11 or just pass via stack)
    ; Actually, the 5th parameter for x64 ABI is via STACK at [rsp+32]
    
    sub rsp, 48                 ; 32 (shadow) + 8 (context) + 8 (align)
    mov [rsp + 32], r13         ; 5th param: Context
    call r12                    ; Invoke callback
    add rsp, 48
    
    mov rsi, [rsi]              ; rsi = Next link (Flink)
    cmp rsi, rbx                ; Back at start?
    jne L_module_loop

L_done:
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
rawrxd_enumerate_modules_peb ENDP

END
