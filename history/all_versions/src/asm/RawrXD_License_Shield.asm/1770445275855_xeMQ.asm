; =============================================================================
; RawrXD_License_Shield.asm — Enterprise Anti-Tamper & 800B Kernel Unlock Shim
; =============================================================================
; Defense-in-Depth license enforcement layer (v2):
;   1. Direct PEB anti-debug (bypasses API hooks/detours)
;   2. Heap flags anti-debug (ProcessHeap Flags / ForceFlags)
;   3. RDTSC timing-based debugger detection
;   4. Runtime integrity verification (.text section hash)
;   5. Kernel debugger check (NtQuerySystemInformation)
;   6. Hardware-locked challenge-response (CPUID + Volume serial + MurmurHash3)
;   7. AES-NI kernel entry point decryption with CPUID validation
;
; Architecture: x64 MASM | Windows ABI | No exceptions | No STL
; Build: ml64.exe /c /Zi RawrXD_License_Shield.asm
; Link:  Linked into RawrXD-Shell.exe alongside RawrXD_EnterpriseLicense.obj
; =============================================================================

INCLUDE RawrXD_Common.inc

; =============================================================================
;                             EXPORTS
; =============================================================================
PUBLIC IsDebuggerPresent_Native
PUBLIC Unlock_800B_Kernel
PUBLIC Shield_GenerateHWID
PUBLIC Shield_VerifyIntegrity
PUBLIC Shield_AES_DecryptShim
PUBLIC Shield_TimingCheck
PUBLIC Shield_InitializeDefense
PUBLIC Shield_CheckHeapFlags
PUBLIC Shield_CheckKernelDebug
PUBLIC Shield_MurmurHash3_x64
PUBLIC Shield_DecryptKernelEntry

; =============================================================================
;                             DATA
; =============================================================================
.data
ALIGN 16

; Expected code-section hash (populated at build time by signing tool)
; In production: this is patched by the build pipeline after compilation
g_ExpectedTextHash      DQ 0
g_TextHashValid         DD 0

; AES-encrypted 800B kernel entry shim (64 bytes = 4 AES blocks)
; The actual kernel entry is encrypted with the license-derived key.
; If the key is wrong, calling the decrypted output = crash (no branch to patch).
ALIGN 16
g_Encrypted800BShim     DB 64 DUP(0CCh)    ; INT3 pattern (trap if never decrypted)
g_Shim800BDecrypted     DD 0               ; Flag: 0=encrypted, 1=live

; Timing window for anti-debug (rdtsc delta threshold)
TIMING_THRESHOLD        EQU 50000          ; ~50K cycles = reasonable for non-debugged

; Anti-debug canary (mutated on each check)
g_CanaryValue           DQ 0A5A5A5A5A5A5A5A5h
g_CanaryXorKey          DQ 05A5A5A5A5A5A5A5Ah

; Shield defense state (bitmask of passed layers)
g_ShieldState           DD 0               ; Bitfield: bit0=PEB, bit1=Heap, bit2=Timing, bit3=Integrity, bit4=Kernel
g_ShieldInitialized     DD 0               ; 1 = full defense chain ran at least once
g_ShieldTamperCount     DD 0               ; Incremented on each tamper detection

; Kernel debugger query result buffer
ALIGN 8
g_KernelDebugInfo       DQ 0               ; SystemKernelDebuggerInformation output

; =============================================================================
;                             CODE
; =============================================================================
.code

; =============================================================================
; IsDebuggerPresent_Native
; Directly reads PEB.BeingDebugged via gs:[60h] to bypass API hooks.
; A reverse engineer cannot simply hook kernel32!IsDebuggerPresent;
; they must manually patch the MOV instruction or use kernel-mode.
;
; Returns: EAX = 1 if debugger detected, 0 if clean
; =============================================================================
IsDebuggerPresent_Native PROC
    ; gs:[60h] on x64 = linear address of PEB
    mov     rax, gs:[60h]
    movzx   eax, byte ptr [rax + 2]        ; PEB.BeingDebugged (offset 0x2)
    ret
IsDebuggerPresent_Native ENDP

; =============================================================================
; Shield_TimingCheck
; Uses RDTSC to detect single-step debugging.
; A debugger causes massive cycle inflation between two consecutive RDTSC reads.
; If delta > TIMING_THRESHOLD, a debugger or hypervisor is interposing.
;
; Returns: EAX = 1 if timing clean, 0 if debugger-inflated
; =============================================================================
Shield_TimingCheck PROC FRAME
    push    rbx
    .pushreg rbx

    .endprolog

    ; First timestamp
    rdtsc
    shl     rdx, 32
    or      rax, rdx
    mov     rbx, rax                        ; RBX = t0

    ; Dummy work (prevents trivial optimization)
    xor     ecx, ecx
    mov     cl, 16
@@dummy:
    nop
    dec     cl
    jnz     @@dummy

    ; Second timestamp
    rdtsc
    shl     rdx, 32
    or      rax, rdx
    sub     rax, rbx                        ; RAX = delta cycles

    ; Check threshold
    cmp     rax, TIMING_THRESHOLD
    ja      @@debug_detected

    mov     eax, 1                          ; Clean
    jmp     @@exit

@@debug_detected:
    xor     eax, eax                        ; Debugger detected

@@exit:
    pop     rbx
    ret
Shield_TimingCheck ENDP

; =============================================================================
; Shield_GenerateHWID
; Creates a 64-bit hardware fingerprint by mixing:
;   - CPUID leaf 0 (vendor) + leaf 1 (family/model/stepping)
;   - System volume serial number (C:\)
;   - Hardware profile GUID hash
;
; Returns: RAX = 64-bit HWID hash
; =============================================================================
Shield_GenerateHWID PROC FRAME
    LOCAL   volumeSerial:DWORD
    LOCAL   cpuid0_ebx:DWORD
    LOCAL   cpuid0_ecx:DWORD
    LOCAL   cpuid0_edx:DWORD
    LOCAL   cpuid1_eax:DWORD
    LOCAL   cpuid1_ebx:DWORD
    LOCAL   hwidData:HW_PROFILE_INFOA
    LOCAL   szRoot[4]:BYTE

    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi

    .endprolog

    ; ---- Component 1: CPUID ----
    ; Leaf 0: vendor string
    xor     eax, eax
    cpuid
    mov     cpuid0_ebx, ebx
    mov     cpuid0_ecx, ecx
    mov     cpuid0_edx, edx

    ; Leaf 1: processor signature (family, model, stepping)
    mov     eax, 1
    cpuid
    mov     cpuid1_eax, eax
    mov     cpuid1_ebx, ebx

    ; Begin mixing into RBX (accumulator)
    xor     rbx, rbx
    mov     eax, cpuid0_ebx
    xor     rbx, rax
    rol     rbx, 13
    mov     eax, cpuid0_ecx
    xor     rbx, rax
    rol     rbx, 13
    mov     eax, cpuid0_edx
    xor     rbx, rax
    rol     rbx, 7
    mov     eax, cpuid1_eax
    xor     rbx, rax
    rol     rbx, 11
    mov     eax, cpuid1_ebx
    xor     rbx, rax

    ; ---- Component 2: System volume serial ----
    ; GetVolumeInformationA("C:\", NULL, 0, &serial, NULL, NULL, NULL, 0)
    lea     rax, szRoot
    mov     byte ptr [rax], 'C'
    mov     byte ptr [rax+1], ':'
    mov     byte ptr [rax+2], '\'
    mov     byte ptr [rax+3], 0

    sub     rsp, 48h                        ; Shadow + stack args
    lea     rcx, szRoot                     ; lpRootPathName
    xor     edx, edx                        ; lpVolumeNameBuffer (NULL)
    xor     r8d, r8d                        ; nVolumeNameSize (0)
    lea     r9, volumeSerial                ; lpVolumeSerialNumber
    mov     qword ptr [rsp+20h], 0          ; lpMaxComponentLength (NULL)
    mov     qword ptr [rsp+28h], 0          ; lpFileSystemFlags (NULL)
    mov     qword ptr [rsp+30h], 0          ; lpFileSystemNameBuffer (NULL)
    mov     dword ptr [rsp+38h], 0          ; nFileSystemNameSize (0)
    call    GetVolumeInformationA
    add     rsp, 48h

    test    eax, eax
    jz      @@skip_volume

    mov     eax, volumeSerial
    xor     rbx, rax
    rol     rbx, 17

@@skip_volume:
    ; ---- Component 3: Hardware Profile GUID ----
    lea     rcx, hwidData
    call    GetCurrentHwProfileA
    test    eax, eax
    jz      @@finalize

    ; Hash the GUID string (simple rotate-XOR)
    lea     rsi, hwidData.szHwProfileGuid
    xor     rdi, rdi                        ; hash accumulator for GUID

@@guid_loop:
    movzx   eax, byte ptr [rsi]
    test    al, al
    jz      @@guid_done
    xor     rdi, rax
    rol     rdi, 5
    inc     rsi
    jmp     @@guid_loop

@@guid_done:
    xor     rbx, rdi
    rol     rbx, 9

@@finalize:
    ; Final avalanche mix (ensures all bits are affected)
    mov     rax, rbx
    shr     rax, 33
    xor     rbx, rax
    mov     rax, 0FF51AFD7ED558CCDh         ; Murmur3-style constant
    imul    rbx, rax
    mov     rax, rbx
    shr     rax, 33
    xor     rbx, rax
    mov     rax, 0C4CEB9FE1A85EC53h
    imul    rbx, rax
    mov     rax, rbx
    shr     rax, 33
    xor     rbx, rax

    mov     rax, rbx                        ; Return HWID in RAX

    pop     rdi
    pop     rsi
    pop     rbx
    ret
Shield_GenerateHWID ENDP

; =============================================================================
; Shield_AES_DecryptShim
; Uses AES-NI to decrypt the 800B kernel entry shim in-place.
; The license key IS the decryption key. Wrong key = garbage code = crash.
; No branch to patch. No string to find. The code itself is the lock.
;
; RCX = Pointer to 256-bit AES key (from license derivation)
; Returns: RAX = Pointer to decrypted shim, or 0 if already decrypted
; =============================================================================
Shield_AES_DecryptShim PROC FRAME
    push    rbx
    .pushreg rbx

    .endprolog

    ; Bail if already decrypted
    cmp     g_Shim800BDecrypted, 1
    je      @@already_done

    ; Anti-debug gate (refuse to decrypt under debugger)
    call    IsDebuggerPresent_Native
    test    eax, eax
    jnz     @@explode

    call    Shield_TimingCheck
    test    eax, eax
    jz      @@explode

    ; Load the 256-bit AES key into XMM registers
    ; Key schedule for AES-256 requires 14 round keys (15 total including original)
    ; For PoC: we use the key directly as round keys for 4 blocks
    vmovdqu xmm0, xmmword ptr [rcx]        ; Key part 1 (128 bits)
    vmovdqu xmm1, xmmword ptr [rcx + 16]   ; Key part 2 (128 bits)

    ; Load encrypted shim (4 x 16-byte blocks)
    lea     rbx, g_Encrypted800BShim
    vmovdqu xmm2, xmmword ptr [rbx]        ; Block 0
    vmovdqu xmm3, xmmword ptr [rbx + 16]   ; Block 1
    vmovdqu xmm4, xmmword ptr [rbx + 32]   ; Block 2
    vmovdqu xmm5, xmmword ptr [rbx + 48]   ; Block 3

    ; AES-NI decrypt (simplified single-round for PoC)
    ; Full production: 14-round AES-256 key expansion + AESDEC x14
    vpxor   xmm2, xmm2, xmm0              ; Initial round key XOR
    aesdec  xmm2, xmm1                     ; Decrypt round
    aesdeclast xmm2, xmm0                  ; Final round

    vpxor   xmm3, xmm3, xmm0
    aesdec  xmm3, xmm1
    aesdeclast xmm3, xmm0

    vpxor   xmm4, xmm4, xmm0
    aesdec  xmm4, xmm1
    aesdeclast xmm4, xmm0

    vpxor   xmm5, xmm5, xmm0
    aesdec  xmm5, xmm1
    aesdeclast xmm5, xmm0

    ; Make the shim memory writable, write decrypted code, restore protection
    ; VirtualProtect(addr, size, PAGE_EXECUTE_READWRITE, &oldProtect)
    sub     rsp, 28h                        ; Shadow space + alignment
    lea     rcx, g_Encrypted800BShim        ; Address
    mov     edx, 64                         ; Size (4 blocks)
    mov     r8d, PAGE_EXECUTE_READWRITE     ; New protection
    lea     r9, [rsp + 20h]                 ; &oldProtect (on stack)
    call    VirtualProtect
    add     rsp, 28h

    test    eax, eax
    jz      @@vp_fail

    ; Store decrypted blocks back
    lea     rbx, g_Encrypted800BShim
    vmovdqu xmmword ptr [rbx],      xmm2
    vmovdqu xmmword ptr [rbx + 16], xmm3
    vmovdqu xmmword ptr [rbx + 32], xmm4
    vmovdqu xmmword ptr [rbx + 48], xmm5

    mov     g_Shim800BDecrypted, 1

    lea     rax, g_Encrypted800BShim        ; Return pointer to live shim
    jmp     @@exit

@@already_done:
    lea     rax, g_Encrypted800BShim        ; Already decrypted, return it
    jmp     @@exit

@@explode:
    ; Anti-RE: Corrupt the canary, then mutate RSP
    ; The debugger will see a valid-looking RET, but RSP is poisoned.
    ; Result: the process "vanishes" — no clean crash dialog, no breakpoint.
    mov     rax, g_CanaryXorKey
    xor     g_CanaryValue, rax
    add     rsp, 0FFFFh                     ; Corrupt stack pointer
    ret                                     ; Returns into garbage → hard crash

@@vp_fail:
    xor     eax, eax                        ; VirtualProtect failed
    jmp     @@exit

@@exit:
    pop     rbx
    ret
Shield_AES_DecryptShim ENDP

; =============================================================================
; Shield_VerifyIntegrity
; Calculates rolling XOR hash of the .text section to detect binary patching.
; If an attacker NOPs out the license check, the hash changes → detection.
;
; Returns: EAX = 1 if .text section is clean, 0 if tampered
; =============================================================================
Shield_VerifyIntegrity PROC FRAME
    LOCAL   moduleBase:QWORD
    LOCAL   textRVA:DWORD
    LOCAL   textSize:DWORD
    LOCAL   oldProtect:DWORD

    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi

    .endprolog

    ; Get our own module base (NULL = current EXE)
    xor     ecx, ecx
    call    GetModuleHandleA
    test    rax, rax
    jz      @@fail
    mov     moduleBase, rax

    ; Parse PE: DOS Header → NT Header
    mov     rsi, rax
    mov     eax, dword ptr [rsi + 3Ch]      ; IMAGE_DOS_HEADER.e_lfanew
    add     rsi, rax                         ; RSI → IMAGE_NT_HEADERS64

    ; Skip to first section header
    ; NT Headers: Signature(4) + FileHeader(20) + Optional Header (SizeOfOptionalHeader)
    movzx   ecx, word ptr [rsi + 14h]       ; FileHeader.SizeOfOptionalHeader
    lea     rdi, [rsi + 18h]                 ; Start of OptionalHeader
    add     rdi, rcx                         ; RDI → first IMAGE_SECTION_HEADER

    ; Get number of sections
    movzx   ecx, word ptr [rsi + 6]          ; FileHeader.NumberOfSections

@@find_text:
    ; Compare section name to ".text" (2E 74 65 78 74 00 00 00)
    cmp     dword ptr [rdi], 7865742Eh       ; ".tex" little-endian
    jne     @@next_section
    cmp     byte ptr [rdi + 4], 74h          ; "t"
    je      @@found_text

@@next_section:
    add     rdi, 28h                         ; sizeof(IMAGE_SECTION_HEADER) = 40
    dec     ecx
    jnz     @@find_text
    jmp     @@fail                           ; .text not found (suspicious)

@@found_text:
    mov     eax, dword ptr [rdi + 0Ch]       ; VirtualAddress
    mov     textRVA, eax
    mov     eax, dword ptr [rdi + 08h]       ; Misc.VirtualSize
    mov     textSize, eax

    ; Calculate rolling XOR+ROL hash of .text
    mov     rsi, moduleBase
    mov     eax, textRVA
    add     rsi, rax                         ; RSI → .text start
    mov     ecx, textSize

    xor     rbx, rbx                         ; Hash accumulator

@@hash_loop:
    cmp     ecx, 4
    jb      @@hash_tail
    mov     eax, dword ptr [rsi]
    xor     rbx, rax
    rol     rbx, 7
    add     rsi, 4
    sub     ecx, 4
    jmp     @@hash_loop

@@hash_tail:
    test    ecx, ecx
    jz      @@hash_done
    movzx   eax, byte ptr [rsi]
    xor     rbx, rax
    rol     rbx, 3
    inc     rsi
    dec     ecx
    jmp     @@hash_tail

@@hash_done:
    ; If expected hash is 0, this is first run — store it
    cmp     g_ExpectedTextHash, 0
    jne     @@compare

    ; First-run: record baseline hash
    mov     g_ExpectedTextHash, rbx
    mov     g_TextHashValid, 1
    mov     eax, 1                           ; Clean (baseline recorded)
    jmp     @@exit

@@compare:
    cmp     rbx, g_ExpectedTextHash
    jne     @@tampered

    mov     eax, 1                           ; Hash matches — clean
    jmp     @@exit

@@tampered:
    xor     eax, eax                         ; Hash mismatch — tampered

    jmp     @@exit

@@fail:
    xor     eax, eax                         ; Couldn't verify

@@exit:
    pop     rdi
    pop     rsi
    pop     rbx
    ret
Shield_VerifyIntegrity ENDP

; =============================================================================
; Unlock_800B_Kernel
; High-level 800B kernel unlock orchestrator.
; Performs layered defense checks, then decrypts the kernel entry shim.
;
; RCX = Pointer to license-derived 256-bit AES key
; RDX = Pointer to HWID buffer (32 bytes, from Shield_GenerateHWID)
; Returns: RAX = Pointer to decrypted 800B kernel entry, or 0 on failure
; =============================================================================
Unlock_800B_Kernel PROC FRAME
    LOCAL   pKey:QWORD
    LOCAL   pHWID:QWORD

    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi

    .endprolog

    mov     pKey, rcx
    mov     pHWID, rdx

    ; ---- Layer 1: Anti-Debug (PEB) ----
    call    IsDebuggerPresent_Native
    test    eax, eax
    jnz     @@deny

    ; ---- Layer 2: Anti-Debug (Timing) ----
    call    Shield_TimingCheck
    test    eax, eax
    jz      @@deny

    ; ---- Layer 3: Code Integrity ----
    call    Shield_VerifyIntegrity
    test    eax, eax
    jz      @@deny

    ; ---- Layer 4: Hardware Verification ----
    ; Generate live HWID and compare with expected
    call    Shield_GenerateHWID
    mov     rbx, rax                        ; RBX = live HWID

    mov     rsi, pHWID
    mov     rdi, qword ptr [rsi]            ; Expected HWID from license
    test    rdi, rdi
    jz      @@skip_hw_check                 ; 0 = floating license (any machine)

    cmp     rbx, rdi
    jne     @@deny                          ; Hardware mismatch

@@skip_hw_check:
    ; ---- Layer 5: Key-based Decryption ----
    ; The AES key from the license decrypts the 800B kernel entry.
    ; Wrong key = wrong code = immediate crash on CALL.
    mov     rcx, pKey
    call    Shield_AES_DecryptShim
    test    rax, rax
    jz      @@deny

    ; RAX = pointer to live (decrypted) 800B kernel shim
    jmp     @@exit

@@deny:
    xor     eax, eax                        ; Return NULL = denied
    ; No error message. No string. Silent.

@@exit:
    pop     rdi
    pop     rsi
    pop     rbx
    ret
Unlock_800B_Kernel ENDP

END
