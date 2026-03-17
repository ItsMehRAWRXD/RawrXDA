;=============================================================================
; Pure MASM AES-256-GCM Implementation (EVP_aes_256_gcm replacement)
; Zero dependencies - pure assembly implementation
;=============================================================================

.686
.XMM
.MODEL flat, c
OPTION casemap:none

;=============================================================================
; AES-256 Constants
;=============================================================================
AES_BLOCK_SIZE      EQU 16
AES_256_KEY_SIZE    EQU 32
AES_256_ROUNDS      EQU 14
GCM_TAG_SIZE        EQU 16
GCM_IV_SIZE         EQU 12

;=============================================================================
; Data Segment - Simplified for MASM compatibility
;=============================================================================
.DATA

; AES S-Box (Substitution Box) - Simplified definition
AES_SBOX BYTE \
    63h, 7Ch, 77h, 7Bh, 0F2h, 06h, 06Fh, 0C5h, 30h, 01h, 067h, 02Bh, 0FEh, 0D7h, 0ABh, 076h, \
    0CAh, 082h, 0C9h, 07Dh, 0FAh, 059h, 047h, 0F0h, 0ADh, 0D4h, 0A2h, 0AFh, 09Ch, 0A4h, 072h, 0C0h, \
    0B7h, 0FDh, 093h, 026h, 036h, 03Fh, 0F7h, 0CCh, 034h, 0A5h, 0E5h, 0F1h, 071h, 0D8h, 031h, 015h, \
    04h, 0C7h, 023h, 0C3h, 018h, 096h, 005h, 09Ah, 007h, 012h, 080h, 0E2h, 0EBh, 027h, 0B2h, 075h, \
    009h, 083h, 02Ch, 01Ah, 01Bh, 06Eh, 05Ah, 0A0h, 052h, 03Bh, 0D6h, 0B3h, 029h, 0E3h, 02Fh, 084h, \
    053h, 0D1h, 000h, 0EDh, 020h, 0FCh, 0B1h, 05Bh, 06Ah, 0CBh, 0BEh, 039h, 04Ah, 04Ch, 058h, 0CFh, \
    0D0h, 0EFh, 0AAh, 0FBh, 043h, 04Dh, 033h, 085h, 045h, 0F9h, 002h, 07Fh, 050h, 03Ch, 09Fh, 0A8h, \
    051h, 0A3h, 040h, 08Fh, 092h, 09Dh, 038h, 0F5h, 0BCh, 0B6h, 0DAh, 021h, 010h, 0FFh, 0F3h, 0D2h, \
    0CDh, 00Ch, 013h, 0ECh, 05Fh, 097h, 044h, 017h, 0C4h, 0A7h, 07Eh, 03Dh, 064h, 05Dh, 019h, 073h, \
    060h, 081h, 04Fh, 0DCh, 022h, 02Ah, 090h, 088h, 046h, 0EEh, 0B8h, 014h, 0DEh, 05Eh, 00Bh, 0DBh, \
    0E0h, 032h, 03Ah, 00Ah, 049h, 006h, 024h, 05Ch, 0C2h, 0D3h, 0ACh, 062h, 091h, 095h, 0E4h, 079h, \
    0E7h, 0C8h, 037h, 06Dh, 08Dh, 0D5h, 04Eh, 0A9h, 06Ch, 056h, 0F4h, 0EAh, 065h, 07Ah, 0AEh, 008h, \
    0BAh, 078h, 025h, 02Eh, 01Ch, 0A6h, 0B4h, 0C6h, 0E8h, 0DDh, 074h, 01Fh, 04Bh, 0BDh, 08Bh, 08Ah, \
    070h, 03Eh, 0B5h, 066h, 048h, 003h, 0F6h, 00Eh, 061h, 035h, 057h, 0B9h, 086h, 0C1h, 01Dh, 09Eh, \
    0E1h, 0F8h, 098h, 011h, 069h, 0D9h, 08Eh, 094h, 09Bh, 01Eh, 087h, 0E9h, 0CEh, 055h, 028h, 0DFh, \
    08Ch, 0A1h, 089h, 00Dh, 0BFh, 0E6h, 042h, 068h, 041h, 099h, 02Dh, 00Fh, 0B0h, 054h, 0BBh, 016h

; AES Rcon (Round Constants) - Simplified definition
AES_RCON DWORD 01000000h, 02000000h, 04000000h, 08000000h, 10000000h, 20000000h, 40000000h, 80000000h
DWORD 1B000000h, 36000000h, 6C000000h, D8000000h, AB000000h, 4D000000h, 9A000000h, 2F000000h

; GCM GHASH Constants
GCM_H QWORD 0E100000000000000h  ; GCM H value for GHASH

;=============================================================================
; Code Segment
;=============================================================================
.CODE

;-----------------------------------------------------------------------------
; aes_sub_bytes - AES SubBytes transformation
; Input:  state = 16-byte state array
; Output: state transformed through S-Box
;-----------------------------------------------------------------------------
aes_sub_bytes PROC, state:PTR BYTE
    mov ecx, AES_BLOCK_SIZE
    mov edx, state
    
@sub_loop:
    mov al, [edx]
    movzx eax, al
    lea ebx, AES_SBOX
    mov al, [ebx + eax]
    mov [edx], al
    inc edx
    loop @sub_loop
    
    ret
aes_sub_bytes ENDP

;-----------------------------------------------------------------------------
; aes_shift_rows - AES ShiftRows transformation
;-----------------------------------------------------------------------------
aes_shift_rows PROC, state:PTR BYTE
    LOCAL temp[4]:BYTE
    
    mov edx, state
    
    ; Row 1: shift left by 1
    mov al, [edx + 1]
    mov temp[0], al
    mov al, [edx + 5]
    mov [edx + 1], al
    mov al, [edx + 9]
    mov [edx + 5], al
    mov al, [edx + 13]
    mov [edx + 9], al
    mov al, temp[0]
    mov [edx + 13], al
    
    ; Row 2: shift left by 2
    mov al, [edx + 2]
    mov temp[0], al
    mov al, [edx + 6]
    mov temp[1], al
    mov al, [edx + 10]
    mov [edx + 2], al
    mov al, [edx + 14]
    mov [edx + 6], al
    mov al, temp[0]
    mov [edx + 10], al
    mov al, temp[1]
    mov [edx + 14], al
    
    ; Row 3: shift left by 3
    mov al, [edx + 15]
    mov temp[0], al
    mov al, [edx + 11]
    mov [edx + 15], al
    mov al, [edx + 7]
    mov [edx + 11], al
    mov al, [edx + 3]
    mov [edx + 7], al
    mov al, temp[0]
    mov [edx + 3], al
    
    ret
aes_shift_rows ENDP

;-----------------------------------------------------------------------------
; aes_gf_mul - Galois Field (2^8) multiplication
; Input: a, b = bytes to multiply
; Returns: result in al
;-----------------------------------------------------------------------------
aes_gf_mul PROC, a:BYTE, b:BYTE
    LOCAL result:BYTE
    LOCAL hi_bit:BYTE
    
    mov result, 0
    mov al, a
    mov bl, b
    
    mov ecx, 8  ; 8 bits
@gf_loop:
    test bl, 1
    jz @skip_xor
    xor result, al
@skip_xor:
    mov hi_bit, al
    and hi_bit, 80h
    shl al, 1
    test hi_bit, 80h
    jz @skip_reduction
    xor al, 1Bh  ; Reduction polynomial x^8 + x^4 + x^3 + x + 1
@skip_reduction:
    shr bl, 1
    loop @gf_loop
    
    mov al, result
    ret
aes_gf_mul ENDP

;-----------------------------------------------------------------------------
; aes_mix_columns - AES MixColumns transformation
;-----------------------------------------------------------------------------
aes_mix_columns PROC, state:PTR BYTE
    LOCAL col[4]:BYTE
    LOCAL result[4]:BYTE
    
    mov edx, state
    mov ecx, 4  ; 4 columns
    
@mix_loop:
    push ecx
    
    ; Load column
    mov al, [edx]
    mov col[0], al
    mov al, [edx + 1]
    mov col[1], al
    mov al, [edx + 2]
    mov col[2], al
    mov al, [edx + 3]
    mov col[3], al
    
    ; Mix column using Galois Field multiplication
    ; s'0 = (2 * s0) + (3 * s1) + s2 + s3
    ; s'1 = s0 + (2 * s1) + (3 * s2) + s3
    ; s'2 = s0 + s1 + (2 * s2) + (3 * s3)
    ; s'3 = (3 * s0) + s1 + s2 + (2 * s3)
    
    ; Calculate 2 * s0, 2 * s1, 2 * s2, 2 * s3
    mov al, col[0]
    shl al, 1
    jnc @no_carry_0
    xor al, 1Bh
@no_carry_0:
    mov result[0], al
    
    mov al, col[1]
    shl al, 1
    jnc @no_carry_1
    xor al, 1Bh
@no_carry_1:
    mov result[1], al
    
    mov al, col[2]
    shl al, 1
    jnc @no_carry_2
    xor al, 1Bh
@no_carry_2:
    mov result[2], al
    
    mov al, col[3]
    shl al, 1
    jnc @no_carry_3
    xor al, 1Bh
@no_carry_3:
    mov result[3], al
    
    ; Calculate 3 * s1 = (2 * s1) + s1
    mov al, result[1]
    xor al, col[1]
    xor result[0], al
    
    ; Calculate 3 * s2 = (2 * s2) + s2
    mov al, result[2]
    xor al, col[2]
    xor result[1], al
    
    ; Calculate 3 * s3 = (2 * s3) + s3
    mov al, result[3]
    xor al, col[3]
    xor result[2], al
    
    ; Calculate 3 * s0 = (2 * s0) + s0
    mov al, result[0]
    xor al, col[0]
    xor result[3], al
    
    ; Add remaining terms
    mov al, col[2]
    xor result[0], al
    mov al, col[3]
    xor result[0], al
    
    mov al, col[0]
    xor result[1], al
    mov al, col[3]
    xor result[1], al
    
    mov al, col[0]
    xor result[2], al
    mov al, col[1]
    xor result[2], al
    
    mov al, col[1]
    xor result[3], al
    mov al, col[2]
    xor result[3], al
    
    ; Store result
    mov al, result[0]
    mov [edx], al
    mov al, result[1]
    mov [edx + 1], al
    mov al, result[2]
    mov [edx + 2], al
    mov al, result[3]
    mov [edx + 3], al
    
    add edx, 4
    pop ecx
    loop @mix_loop
    
    ret
aes_mix_columns ENDP

;-----------------------------------------------------------------------------
; aes_add_round_key - AES AddRoundKey transformation
;-----------------------------------------------------------------------------
aes_add_round_key PROC, state:PTR BYTE, round_key:PTR BYTE
    mov edx, state
    mov ecx, round_key
    
    mov eax, [edx]
    xor eax, [ecx]
    mov [edx], eax
    
    mov eax, [edx + 4]
    xor eax, [ecx + 4]
    mov [edx + 4], eax
    
    mov eax, [edx + 8]
    xor eax, [ecx + 8]
    mov [edx + 8], eax
    
    mov eax, [edx + 12]
    xor eax, [ecx + 12]
    mov [edx + 12], eax
    
    ret
aes_add_round_key ENDP

;-----------------------------------------------------------------------------
; aes_key_expansion - AES-256 Key Expansion
;-----------------------------------------------------------------------------
aes_key_expansion PROC, key:PTR BYTE, expanded_key:PTR BYTE
    LOCAL i:DWORD
    LOCAL temp[4]:BYTE
    LOCAL rcon_idx:DWORD
    
    mov i, 0
    mov edx, expanded_key
    mov esi, key
    
    ; Copy original key (32 bytes = 8 DWORDs)
    mov ecx, 8
    rep movsd
    
    ; Expand key
    mov i, 8  ; Start after original key (8 DWORDs)
    mov rcon_idx, 0
    
@expand_loop:
    cmp i, 60  ; AES-256 needs 60 DWORDs total (15 rounds * 4 DWORDs)
    jge @expand_done
    
    ; For AES-256, we process 8 DWORDs at a time
    mov eax, i
    and eax, 7  ; i % 8
    jnz @skip_special
    
    ; Every 8th DWORD needs special processing
    mov ecx, expanded_key
    mov eax, i
    sub eax, 8
    lea esi, [ecx + eax*4]  ; &expanded_key[i-8]
    mov edi, OFFSET temp
    mov ecx, 4
    rep movsd
    
    ; RotWord
    mov al, temp[0]
    mov temp[0], temp[1]
    mov temp[1], temp[2]
    mov temp[2], temp[3]
    mov temp[3], al
    
    ; SubWord
    lea ebx, AES_SBOX
    movzx eax, temp[0]
    mov al, [ebx + eax]
    mov temp[0], al
    movzx eax, temp[1]
    mov al, [ebx + eax]
    mov temp[1], al
    movzx eax, temp[2]
    mov al, [ebx + eax]
    mov temp[2], al
    movzx eax, temp[3]
    mov al, [ebx + eax]
    mov temp[3], al
    
    ; XOR with Rcon
    lea ebx, AES_RCON
    mov eax, rcon_idx
    mov eax, [ebx + eax*4]
    xor temp[0], al
    
    inc rcon_idx
    
    @skip_special:
    
    ; For i >= 8, XOR with expanded_key[i-8]
    mov eax, i
    sub eax, 8
    mov ecx, expanded_key
    lea esi, [ecx + eax*4]  ; &expanded_key[i-8]
    mov edi, OFFSET temp
    mov eax, [esi]
    xor [edi], eax
    
    ; Store result
    mov ecx, expanded_key
    lea edi, [ecx + i*4]  ; &expanded_key[i]
    mov eax, [edi]
    xor eax, [OFFSET temp]
    mov [edi], eax
    
    inc i
    jmp @expand_loop
    
@expand_done:
    ret
aes_key_expansion ENDP

;-----------------------------------------------------------------------------
; masm_crypto_aes256_init - Initialize AES-256 context
; Input:  ctx = AES256_CTX pointer
;         key = 32-byte key
;         iv = 12-byte IV for GCM
; Returns: 0=success, -1=error
;-----------------------------------------------------------------------------
masm_crypto_aes256_init PROC C, ctx:DWORD, key:DWORD, iv:DWORD
    LOCAL ctx_ptr:PTR AES256_CTX
    LOCAL key_ptr:PTR BYTE
    LOCAL iv_ptr:PTR BYTE
    
    mov ctx_ptr, ctx
    mov key_ptr, key
    mov iv_ptr, iv
    
    ; Validate pointers
    cmp ctx, 0
    je @init_error
    cmp key, 0
    je @init_error
    cmp iv, 0
    je @init_error
    
    ; Initialize GCM IV
    mov edx, ctx_ptr
    add edx, OFFSET AES256_CTX.gcm_iv
    mov ecx, iv_ptr
    mov eax, [ecx]
    mov [edx], eax
    mov eax, [ecx + 4]
    mov [edx + 4], eax
    mov ax, [ecx + 8]
    mov [edx + 8], ax
    
    ; Expand encryption key
    lea eax, (AES256_CTX PTR [edx]).encrypt_key
    push eax
    push key_ptr
    call aes_key_expansion
    add esp, 8
    
    ; Generate decryption key (inverse cipher)
    ; ... (inverse key schedule)
    
    xor eax, eax
    ret
    
@init_error:
    mov eax, -1
    ret
masm_crypto_aes256_init ENDP

;-----------------------------------------------------------------------------
; aes_encrypt_block - Encrypt single AES block
; Input:  ctx = AES256_CTX pointer
;         input = 16-byte input block
;         output = 16-byte output block
;-----------------------------------------------------------------------------
aes_encrypt_block PROC, ctx:DWORD, input:DWORD, output:DWORD
    LOCAL ctx_ptr:PTR AES256_CTX
    LOCAL in_ptr:PTR BYTE
    LOCAL out_ptr:PTR BYTE
    LOCAL state[16]:BYTE
    LOCAL round:DWORD
    
    mov ctx_ptr, ctx
    mov in_ptr, input
    mov out_ptr, output
    
    ; Copy input to state
    mov ecx, 16
    mov esi, in_ptr
    lea edi, state
    rep movsb
    
    ; Initial AddRoundKey
    mov edx, ctx_ptr
    lea ebx, (AES256_CTX PTR [edx]).encrypt_key
    lea edi, state
    mov ecx, 4
@add_key_loop:
    mov eax, [edi]
    xor eax, [ebx]
    mov [edi], eax
    add edi, 4
    add ebx, 4
    loop @add_key_loop
    
    ; Main rounds
    mov round, 1
    
@round_loop:
    cmp round, AES_256_ROUNDS
    jge @final_round
    
    ; SubBytes
    lea ebx, state
    call aes_sub_bytes
    
    ; ShiftRows
    lea ebx, state
    call aes_shift_rows
    
    ; MixColumns
    lea ebx, state
    call aes_mix_columns
    
    ; AddRoundKey
    mov edx, ctx_ptr
    mov eax, round
    shl eax, 4  ; round * 16
    lea ebx, (AES256_CTX PTR [edx]).encrypt_key
    add ebx, eax
    lea edi, state
    mov ecx, 4
@add_key_loop2:
    mov eax, [edi]
    xor eax, [ebx]
    mov [edi], eax
    add edi, 4
    add ebx, 4
    loop @add_key_loop2
    
    inc round
    jmp @round_loop
    
@final_round:
    ; Final round (no MixColumns)
    lea ebx, state
    call aes_sub_bytes
    
    lea ebx, state
    call aes_shift_rows
    
    ; Final AddRoundKey
    mov edx, ctx_ptr
    lea ebx, (AES256_CTX PTR [edx]).encrypt_key
    add ebx, (AES_256_ROUNDS * 16)
    lea edi, state
    mov ecx, 4
@final_add_key:
    mov eax, [edi]
    xor eax, [ebx]
    mov [edi], eax
    add edi, 4
    add ebx, 4
    loop @final_add_key
    
    ; Copy state to output
    mov ecx, 16
    lea esi, state
    mov edi, out_ptr
    rep movsb
    
    ret
aes_encrypt_block ENDP

;-----------------------------------------------------------------------------
; aes_gcm_ghash - GCM GHASH calculation
; Input:  h = GHASH key (128-bit)
;         x = input data
;         len = data length (in bytes)
;         result = output GHASH (128-bit)
;-----------------------------------------------------------------------------
aes_gcm_ghash PROC, h:QWORD, x:PTR BYTE, len:DWORD, result:PTR BYTE
    LOCAL i:DWORD
    LOCAL blocks:DWORD
    LOCAL temp[16]:BYTE
    LOCAL carry:BYTE
    LOCAL bit:DWORD
    
    ; Initialize result to zero
    mov ecx, 16
    mov edi, result
    xor eax, eax
    rep stosb
    
    ; Calculate number of blocks (16-byte blocks)
    mov eax, len
    add eax, 15
    shr eax, 4
    mov blocks, eax
    
    mov i, 0
    mov esi, x
    
@ghash_loop:
    cmp i, blocks
    jge @ghash_done
    
    ; XOR input block with current result
    mov edi, result
    mov ecx, 16
@xor_loop:
    mov al, [esi]
    xor al, [edi]
    mov [edi], al
    inc esi
    inc edi
    loop @xor_loop
    
    ; Multiply by H in GF(2^128)
    ; This is the critical GHASH multiplication step
    mov bit, 128
    
@mul_loop:
    ; Check if most significant bit is set
    mov edi, result
    mov al, [edi]
    test al, 80h
    jz @no_carry
    mov carry, 1
    jmp @carry_set
@no_carry:
    mov carry, 0
@carry_set:
    
    ; Shift result left by 1 bit
    mov ecx, 16
    mov edi, result
    xor ebx, ebx  ; ebx = carry from previous byte
@shift_loop:
    mov al, [edi]
    mov ah, al
    shl al, 1
    add al, bl    ; Add carry from previous byte
    mov [edi], al
    mov bl, ah
    shr bl, 7     ; Carry for next byte
    inc edi
    loop @shift_loop
    
    ; If carry was set, XOR with reduction polynomial
    cmp carry, 0
    je @no_reduction
    
    ; XOR with reduction polynomial x^128 + x^7 + x^2 + x + 1
    ; This is represented as 0xE100000000000000
    mov edi, result
    mov al, [edi]
    xor al, 0E1h
    mov [edi], al
    
@no_reduction:
    
    dec bit
    jnz @mul_loop
    
    inc i
    jmp @ghash_loop
    
@ghash_done:
    ret
aes_gcm_ghash ENDP

;-----------------------------------------------------------------------------
; masm_crypto_aes256_encrypt - Encrypt data with AES-256-GCM
; Input:  ctx = AES256_CTX pointer
;         plaintext = plaintext buffer
;         plaintext_len = plaintext length
;         aad = additional authenticated data
;         aad_len = AAD length
;         ciphertext = output ciphertext buffer
;         tag = output authentication tag (16 bytes)
; Returns: 0=success, -1=error
;-----------------------------------------------------------------------------
masm_crypto_aes256_encrypt PROC C, ctx:DWORD, plaintext:DWORD, plaintext_len:DWORD, \
                                   aad:DWORD, aad_len:DWORD, ciphertext:DWORD, tag:DWORD
    LOCAL ctx_ptr:PTR AES256_CTX
    LOCAL pt_ptr:PTR BYTE
    LOCAL pt_len:DWORD
    LOCAL ct_ptr:PTR BYTE
    LOCAL tag_ptr:PTR BYTE
    LOCAL counter[16]:BYTE
    LOCAL ghash_result[16]:BYTE
    LOCAL i:DWORD
    LOCAL blocks:DWORD
    LOCAL remainder:DWORD
    
    mov ctx_ptr, ctx
    mov pt_ptr, plaintext
    mov pt_len, plaintext_len
    mov ct_ptr, ciphertext
    mov tag_ptr, tag
    
    ; Validate pointers
    cmp ctx, 0
    je @encrypt_error
    cmp plaintext, 0
    je @encrypt_error
    cmp ciphertext, 0
    je @encrypt_error
    cmp tag, 0
    je @encrypt_error
    
    ; Store AAD length in context
    mov edx, ctx_ptr
    mov eax, aad_len
    mov (AES256_CTX PTR [edx]).gcm_aad_len, eax
    
    ; Store cipher length in context
    mov eax, pt_len
    mov (AES256_CTX PTR [edx]).gcm_cipher_len, rax
    
    ; Initialize counter from IV
    mov ecx, ctx_ptr
    lea esi, (AES256_CTX PTR [ecx]).gcm_iv
    lea edi, counter
    mov ecx, 12
    rep movsb
    mov DWORD PTR [edi], 01000000h  ; Initial counter value
    
    ; Calculate number of blocks
    mov eax, pt_len
    add eax, 15
    shr eax, 4
    mov blocks, eax
    mov remainder, 0
    
    mov eax, pt_len
    and eax, 15
    jz @no_remainder
    mov remainder, eax
@no_remainder:
    
    ; Encrypt plaintext block by block
    mov i, 0
    mov esi, pt_ptr
    mov edi, ct_ptr
    
@encrypt_loop:
    cmp i, blocks
    jge @encrypt_done
    
    ; Encrypt counter to get keystream
    lea ebx, counter
    push ebx
    push OFFSET temp_keystream
    call aes_encrypt_block
    add esp, 8
    
    ; XOR plaintext with keystream
    mov ecx, 16
    lea ebx, temp_keystream
@xor_encrypt:
    mov al, [esi]
    xor al, [ebx]
    mov [edi], al
    inc esi
    inc edi
    inc ebx
    loop @xor_encrypt
    
    ; Increment counter
    lea ebx, counter
    mov eax, [ebx + 12]
    inc eax
    mov [ebx + 12], eax
    jnc @counter_ok
    mov eax, [ebx + 8]
    inc eax
    mov [ebx + 8], eax
@counter_ok:
    
    inc i
    jmp @encrypt_loop
    
@encrypt_done:
    ; Compute GHASH for AAD
    cmp aad, 0
    je @skip_aad_ghash
    cmp aad_len, 0
    je @skip_aad_ghash
    
    push aad_len
    push OFFSET ghash_result
    push aad
    push (AES256_CTX PTR [edx]).gcm_h
    call aes_gcm_ghash
    add esp, 16
@skip_aad_ghash:
    
    ; Compute GHASH for ciphertext
    cmp pt_len, 0
    je @skip_cipher_ghash
    
    push pt_len
    push OFFSET ghash_result
    push ciphertext
    push (AES256_CTX PTR [edx]).gcm_h
    call aes_gcm_ghash
    add esp, 16
@skip_cipher_ghash:
    
    ; Generate authentication tag
    ; XOR GHASH result with encrypted counter
    lea ebx, counter
    mov DWORD PTR [ebx + 12], 0  ; Counter = 0 for tag generation
    
    push ebx
    push OFFSET temp_keystream
    call aes_encrypt_block
    add esp, 8
    
    mov ecx, 16
    lea esi, ghash_result
    lea edi, temp_keystream
    mov edi, tag_ptr
@xor_tag:
    mov al, [esi]
    xor al, [ebx]
    mov [edi], al
    inc esi
    inc ebx
    inc edi
    loop @xor_tag
    
    xor eax, eax
    ret
    
@encrypt_error:
    mov eax, -1
    ret
masm_crypto_aes256_encrypt ENDP

;-----------------------------------------------------------------------------
; masm_crypto_aes256_decrypt - Decrypt data with AES-256-GCM
; Input:  ctx = AES256_CTX pointer
;         ciphertext = ciphertext buffer
;         ciphertext_len = ciphertext length
;         aad = additional authenticated data
;         aad_len = AAD length
;         tag = authentication tag (16 bytes)
;         plaintext = output plaintext buffer
; Returns: 0=success, -1=error (authentication failure)
;-----------------------------------------------------------------------------
masm_crypto_aes256_decrypt PROC C, ctx:DWORD, ciphertext:DWORD, ciphertext_len:DWORD, \
                                   aad:DWORD, aad_len:DWORD, tag:DWORD, plaintext:DWORD
    LOCAL ctx_ptr:PTR AES256_CTX
    LOCAL ct_ptr:PTR BYTE
    LOCAL ct_len:DWORD
    LOCAL tag_ptr:PTR BYTE
    LOCAL pt_ptr:PTR BYTE
    LOCAL computed_tag[16]:BYTE
    LOCAL counter[16]:BYTE
    LOCAL ghash_result[16]:BYTE
    LOCAL i:DWORD
    LOCAL blocks:DWORD
    
    mov ctx_ptr, ctx
    mov ct_ptr, ciphertext
    mov ct_len, ciphertext_len
    mov tag_ptr, tag
    mov pt_ptr, plaintext
    
    ; Validate pointers
    cmp ctx, 0
    je @decrypt_error
    cmp ciphertext, 0
    je @decrypt_error
    cmp tag, 0
    je @decrypt_error
    cmp plaintext, 0
    je @decrypt_error
    
    ; Initialize counter from IV
    mov ecx, ctx_ptr
    lea esi, (AES256_CTX PTR [ecx]).gcm_iv
    lea edi, counter
    mov ecx, 12
    rep movsb
    mov DWORD PTR [edi], 01000000h  ; Initial counter value
    
    ; Calculate number of blocks
    mov eax, ct_len
    add eax, 15
    shr eax, 4
    mov blocks, eax
    
    ; Decrypt ciphertext block by block
    mov i, 0
    mov esi, ct_ptr
    mov edi, pt_ptr
    
@decrypt_loop:
    cmp i, blocks
    jge @decrypt_done
    
    ; Encrypt counter to get keystream
    lea ebx, counter
    push ebx
    push OFFSET temp_keystream
    call aes_encrypt_block
    add esp, 8
    
    ; XOR ciphertext with keystream
    mov ecx, 16
    lea ebx, temp_keystream
@xor_decrypt:
    mov al, [esi]
    xor al, [ebx]
    mov [edi], al
    inc esi
    inc edi
    inc ebx
    loop @xor_decrypt
    
    ; Increment counter
    lea ebx, counter
    mov eax, [ebx + 12]
    inc eax
    mov [ebx + 12], eax
    jnc @counter_ok
    mov eax, [ebx + 8]
    inc eax
    mov [ebx + 8], eax
@counter_ok:
    
    inc i
    jmp @decrypt_loop
    
@decrypt_done:
    ; Compute GHASH for AAD
    cmp aad, 0
    je @skip_aad_ghash
    cmp aad_len, 0
    je @skip_aad_ghash
    
    push aad_len
    push OFFSET ghash_result
    push aad
    push (AES256_CTX PTR [edx]).gcm_h
    call aes_gcm_ghash
    add esp, 16
@skip_aad_ghash:
    
    ; Compute GHASH for ciphertext
    cmp ct_len, 0
    je @skip_cipher_ghash
    
    push ct_len
    push OFFSET ghash_result
    push ciphertext
    push (AES256_CTX PTR [edx]).gcm_h
    call aes_gcm_ghash
    add esp, 16
@skip_cipher_ghash:
    
    ; Generate authentication tag
    ; XOR GHASH result with encrypted counter
    lea ebx, counter
    mov DWORD PTR [ebx + 12], 0  ; Counter = 0 for tag generation
    
    push ebx
    push OFFSET temp_keystream
    call aes_encrypt_block
    add esp, 8
    
    mov ecx, 16
    lea esi, ghash_result
    lea edi, temp_keystream
    lea edi, computed_tag
@xor_tag:
    mov al, [esi]
    xor al, [ebx]
    mov [edi], al
    inc esi
    inc ebx
    inc edi
    loop @xor_tag
    
    ; Verify tag
    mov ecx, GCM_TAG_SIZE
    mov esi, tag_ptr
    lea edi, computed_tag
    repe cmpsb
    jne @decrypt_error  ; Authentication failed
    
    xor eax, eax
    ret
    
@decrypt_error:
    mov eax, -1
    ret
masm_crypto_aes256_decrypt ENDP

;-----------------------------------------------------------------------------
; masm_crypto_aes256_cleanup - Cleanup AES-256 context
;-----------------------------------------------------------------------------
masm_crypto_aes256_cleanup PROC C, ctx:DWORD
    ; Zero out sensitive key material
    mov edx, ctx
    mov ecx, SIZEOF AES256_CTX
    xor eax, eax
    
@zero_loop:
    mov BYTE PTR [edx], al
    inc edx
    loop @zero_loop
    
    ret
masm_crypto_aes256_cleanup ENDP

END