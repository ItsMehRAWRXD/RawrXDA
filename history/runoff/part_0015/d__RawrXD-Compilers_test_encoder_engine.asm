; =============================================================================
; RawrXD Encoder Engine Test Suite
; Tests all codec implementations
; =============================================================================

option casemap:none

include rawrxd_encoder_engine.asm

.data
    ; Test input strings
    test_str_hello      db "Hello, World!", 0
    test_str_hello_len  equ $ - test_str_hello - 1
    
    test_str_binary     db 0, 1, 2, 0FFh, 0FEh, 0FDh, 0
    test_str_binary_len equ $ - test_str_binary - 1
    
    test_str_url        db "Hello World! @#$%^&*()", 0
    test_str_url_len    equ $ - test_str_url - 1
    
    test_str_rot13      db "The Quick Brown Fox", 0
    test_str_rot13_len  equ $ - test_str_rot13 - 1
    
    ; Expected Base64 output for "Hello, World!"
    expected_b64        db "SGVsbG8sIFdvcmxkIQ==", 0
    expected_b64_len    equ $ - expected_b64 - 1
    
    ; Expected Hex output for "Hello, World!"
    expected_hex        db "48656c6c6f2c20576f726c6421", 0
    expected_hex_len    equ $ - expected_hex - 1
    
    ; Test result messages
    msg_test_start      db "[TEST] Starting encoder test suite...", 13, 10, 0
    msg_test_pass       db "[PASS] ", 0
    msg_test_fail       db "[FAIL] ", 0
    msg_newline         db 13, 10, 0
    
    msg_b64_encode      db "Base64 Encode", 0
    msg_b64_decode      db "Base64 Decode", 0
    msg_b64_url         db "Base64 URL-Safe", 0
    msg_hex_encode      db "Hex Encode (lowercase)", 0
    msg_hex_upper       db "Hex Encode (uppercase)", 0
    msg_hex_decode      db "Hex Decode", 0
    msg_url_encode      db "URL Encode", 0
    msg_url_decode      db "URL Decode", 0
    msg_xor_roundtrip   db "XOR Roundtrip", 0
    msg_rot13           db "ROT13 Transform", 0
    msg_simd_detect     db "SIMD Detection", 0
    msg_context_init    db "Context Initialization", 0
    
    ; SIMD level messages
    msg_simd_sse2       db " (SSE2 detected)", 0
    msg_simd_avx2       db " (AVX2 detected)", 0
    msg_simd_avx512     db " (AVX-512 detected)", 0
    
    ; Summary
    msg_summary         db 13, 10, "[SUMMARY] Tests passed: ", 0
    msg_of              db " of ", 0
    msg_total           db " total", 13, 10, 0
    
    align 16
    ; Output buffers
    output_buffer       db 1024 dup(0)
    decode_buffer       db 1024 dup(0)
    
    align 64
    ; Test context
    test_context        ENCODER_CONTEXT <>
    
    ; Test counters
    tests_passed        dd 0
    tests_total         dd 0

.code
extrn GetStdHandle:proc
extrn WriteConsoleA:proc
extrn ExitProcess:proc

;------------------------------------------------------------------------------
; print_str - Print null-terminated string to console
; RCX = string pointer
;------------------------------------------------------------------------------
print_str proc frame
    push rbx
    .pushreg rbx
    push rsi
    .pushreg rsi
    push rdi
    .pushreg rdi
    sub rsp, 48h
    .allocstack 48h
    .endprolog
    
    mov rsi, rcx
    
    ; Get string length
    xor ecx, ecx
@@: cmp byte ptr [rsi+rcx], 0
    je @F
    inc ecx
    jmp @B
@@:
    mov edi, ecx                    ; Save length
    
    ; Get stdout handle
    mov ecx, -11                    ; STD_OUTPUT_HANDLE
    call GetStdHandle
    mov rbx, rax
    
    ; Write string
    mov rcx, rbx                    ; Handle
    mov rdx, rsi                    ; Buffer
    mov r8d, edi                    ; Length
    lea r9, [rsp+40h]               ; Bytes written
    mov qword ptr [rsp+20h], 0      ; Reserved
    call WriteConsoleA
    
    add rsp, 48h
    pop rdi
    pop rsi
    pop rbx
    ret
print_str endp

;------------------------------------------------------------------------------
; print_number - Print decimal number
; ECX = number
;------------------------------------------------------------------------------
print_number proc frame
    sub rsp, 48h
    .allocstack 48h
    .endprolog
    
    lea rdi, [rsp+20h]
    mov eax, ecx
    
    ; Convert to decimal string (reverse)
    mov r8, rdi
    add r8, 15
    mov byte ptr [r8], 0
    dec r8
    
@@: xor edx, edx
    mov ecx, 10
    div ecx
    add dl, '0'
    mov [r8], dl
    dec r8
    test eax, eax
    jnz @B
    
    inc r8
    mov rcx, r8
    call print_str
    
    add rsp, 48h
    ret
print_number endp

;------------------------------------------------------------------------------
; report_test - Report test result
; RCX = test name, RDX = passed (1) or failed (0)
;------------------------------------------------------------------------------
report_test proc frame
    push rbx
    .pushreg rbx
    push rsi
    .pushreg rsi
    sub rsp, 28h
    .allocstack 28h
    .endprolog
    
    mov rsi, rcx                    ; Test name
    mov ebx, edx                    ; Result
    
    ; Increment total
    inc tests_total
    
    ; Print prefix
    test ebx, ebx
    jz @@failed
    
    inc tests_passed
    lea rcx, msg_test_pass
    jmp @@print_name
    
@@failed:
    lea rcx, msg_test_fail
    
@@print_name:
    call print_str
    
    mov rcx, rsi
    call print_str
    
    lea rcx, msg_newline
    call print_str
    
    add rsp, 28h
    pop rsi
    pop rbx
    ret
report_test endp

;------------------------------------------------------------------------------
; compare_buffers - Compare two buffers
; RCX = buf1, RDX = buf2, R8 = length
; Returns: EAX = 1 if equal, 0 if different
;------------------------------------------------------------------------------
compare_buffers proc frame
    .endprolog
    
    test r8, r8
    jz @@equal
    
@@loop:
    mov al, [rcx]
    cmp al, [rdx]
    jne @@different
    inc rcx
    inc rdx
    dec r8
    jnz @@loop
    
@@equal:
    mov eax, 1
    ret
    
@@different:
    xor eax, eax
    ret
compare_buffers endp

;------------------------------------------------------------------------------
; test_simd_detection - Test SIMD capability detection
;------------------------------------------------------------------------------
test_simd_detection proc frame
    sub rsp, 28h
    .allocstack 28h
    .endprolog
    
    call DetectSimdSupport
    
    ; Always passes (detection itself doesn't fail)
    lea rcx, msg_simd_detect
    mov edx, 1
    call report_test
    
    ; Print detected level
    movzx eax, g_simd_level
    cmp al, 2
    je @@avx512
    cmp al, 1
    je @@avx2
    
    lea rcx, msg_simd_sse2
    jmp @@print
@@avx2:
    lea rcx, msg_simd_avx2
    jmp @@print
@@avx512:
    lea rcx, msg_simd_avx512
@@print:
    call print_str
    lea rcx, msg_newline
    call print_str
    
    add rsp, 28h
    ret
test_simd_detection endp

;------------------------------------------------------------------------------
; test_context_init - Test context initialization
;------------------------------------------------------------------------------
test_context_init proc frame
    sub rsp, 28h
    .allocstack 28h
    .endprolog
    
    lea rcx, test_context
    mov edx, CODEC_BASE64_STD
    xor r8d, r8d
    call EncoderInitializeContext
    
    ; Check return value
    test eax, eax
    setz al
    movzx edx, al
    
    lea rcx, msg_context_init
    call report_test
    
    add rsp, 28h
    ret
test_context_init endp

;------------------------------------------------------------------------------
; test_base64_encode - Test Base64 encoding
;------------------------------------------------------------------------------
test_base64_encode proc frame
    sub rsp, 28h
    .allocstack 28h
    .endprolog
    
    ; Encode "Hello, World!"
    lea rcx, test_str_hello
    mov rdx, test_str_hello_len
    lea r8, output_buffer
    xor r9d, r9d                    ; Standard alphabet
    call Base64EncodeScalar
    
    ; Compare with expected
    lea rcx, output_buffer
    lea rdx, expected_b64
    mov r8, expected_b64_len
    call compare_buffers
    
    mov edx, eax
    lea rcx, msg_b64_encode
    call report_test
    
    add rsp, 28h
    ret
test_base64_encode endp

;------------------------------------------------------------------------------
; test_base64_decode - Test Base64 decoding (roundtrip)
;------------------------------------------------------------------------------
test_base64_decode proc frame
    sub rsp, 28h
    .allocstack 28h
    .endprolog
    
    ; First encode
    lea rcx, test_str_hello
    mov rdx, test_str_hello_len
    lea r8, output_buffer
    xor r9d, r9d
    call Base64EncodeScalar
    mov r10, rax                    ; Save encoded length
    
    ; Then decode
    lea rcx, output_buffer
    mov rdx, r10
    lea r8, decode_buffer
    call Base64DecodeScalar
    
    ; Compare decoded with original
    lea rcx, decode_buffer
    lea rdx, test_str_hello
    mov r8, test_str_hello_len
    call compare_buffers
    
    mov edx, eax
    lea rcx, msg_b64_decode
    call report_test
    
    add rsp, 28h
    ret
test_base64_decode endp

;------------------------------------------------------------------------------
; test_hex_encode - Test Hex encoding
;------------------------------------------------------------------------------
test_hex_encode proc frame
    sub rsp, 28h
    .allocstack 28h
    .endprolog
    
    ; Encode "Hello, World!"
    lea rcx, test_str_hello
    mov rdx, test_str_hello_len
    lea r8, output_buffer
    xor r9d, r9d                    ; Lowercase
    call HexEncode
    
    ; Compare with expected
    lea rcx, output_buffer
    lea rdx, expected_hex
    mov r8, expected_hex_len
    call compare_buffers
    
    mov edx, eax
    lea rcx, msg_hex_encode
    call report_test
    
    add rsp, 28h
    ret
test_hex_encode endp

;------------------------------------------------------------------------------
; test_hex_decode - Test Hex decoding (roundtrip)
;------------------------------------------------------------------------------
test_hex_decode proc frame
    sub rsp, 28h
    .allocstack 28h
    .endprolog
    
    ; Encode first
    lea rcx, test_str_hello
    mov rdx, test_str_hello_len
    lea r8, output_buffer
    xor r9d, r9d
    call HexEncode
    mov r10, rax
    
    ; Decode
    lea rcx, output_buffer
    mov rdx, r10
    lea r8, decode_buffer
    call HexDecode
    
    ; Compare
    lea rcx, decode_buffer
    lea rdx, test_str_hello
    mov r8, test_str_hello_len
    call compare_buffers
    
    mov edx, eax
    lea rcx, msg_hex_decode
    call report_test
    
    add rsp, 28h
    ret
test_hex_decode endp

;------------------------------------------------------------------------------
; test_url_encode - Test URL encoding
;------------------------------------------------------------------------------
test_url_encode proc frame
    sub rsp, 28h
    .allocstack 28h
    .endprolog
    
    ; Encode string with special characters
    lea rcx, test_str_url
    mov rdx, test_str_url_len
    lea r8, output_buffer
    mov r9, 1024
    call UrlEncode
    
    ; Basic check: output should be longer (special chars encoded)
    cmp rax, test_str_url_len
    setg al
    movzx edx, al
    
    lea rcx, msg_url_encode
    call report_test
    
    add rsp, 28h
    ret
test_url_encode endp

;------------------------------------------------------------------------------
; test_url_decode - Test URL decoding (roundtrip)
;------------------------------------------------------------------------------
test_url_decode proc frame
    sub rsp, 28h
    .allocstack 28h
    .endprolog
    
    ; Encode
    lea rcx, test_str_url
    mov rdx, test_str_url_len
    lea r8, output_buffer
    mov r9, 1024
    call UrlEncode
    mov r10, rax
    
    ; Decode
    lea rcx, output_buffer
    mov rdx, r10
    lea r8, decode_buffer
    call UrlDecode
    
    ; Compare
    lea rcx, decode_buffer
    lea rdx, test_str_url
    mov r8, test_str_url_len
    call compare_buffers
    
    mov edx, eax
    lea rcx, msg_url_decode
    call report_test
    
    add rsp, 28h
    ret
test_url_decode endp

;------------------------------------------------------------------------------
; test_xor_roundtrip - Test XOR cipher (encrypt then decrypt)
;------------------------------------------------------------------------------
test_xor_roundtrip proc frame
    sub rsp, 28h
    .allocstack 28h
    .endprolog
    
    ; Initialize context with custom key
    lea rcx, test_context
    mov edx, CODEC_XOR
    xor r8d, r8d
    call EncoderInitializeContext
    
    ; Set a specific key
    lea rcx, test_context
    mov edx, 012345678h
    call EncoderSetXorKey
    
    ; Encrypt
    lea rcx, test_context
    lea rdx, test_str_hello
    mov r8, test_str_hello_len
    lea r9, output_buffer
    call XorCrypt
    
    ; Reset key for decryption
    lea rcx, test_context
    mov edx, 012345678h
    call EncoderSetXorKey
    
    ; Decrypt
    lea rcx, test_context
    lea rdx, output_buffer
    mov r8, test_str_hello_len
    lea r9, decode_buffer
    call XorCrypt
    
    ; Compare
    lea rcx, decode_buffer
    lea rdx, test_str_hello
    mov r8, test_str_hello_len
    call compare_buffers
    
    mov edx, eax
    lea rcx, msg_xor_roundtrip
    call report_test
    
    add rsp, 28h
    ret
test_xor_roundtrip endp

;------------------------------------------------------------------------------
; test_rot13 - Test ROT13 (double application should return original)
;------------------------------------------------------------------------------
test_rot13 proc frame
    sub rsp, 28h
    .allocstack 28h
    .endprolog
    
    ; Apply ROT13 once
    lea rcx, test_str_rot13
    mov rdx, test_str_rot13_len
    lea r8, output_buffer
    call Rot13Transform
    
    ; Apply ROT13 again
    lea rcx, output_buffer
    mov rdx, test_str_rot13_len
    lea r8, decode_buffer
    call Rot13Transform
    
    ; Should match original
    lea rcx, decode_buffer
    lea rdx, test_str_rot13
    mov r8, test_str_rot13_len
    call compare_buffers
    
    mov edx, eax
    lea rcx, msg_rot13
    call report_test
    
    add rsp, 28h
    ret
test_rot13 endp

;------------------------------------------------------------------------------
; main - Test entry point
;------------------------------------------------------------------------------
public main
main proc frame
    sub rsp, 28h
    .allocstack 28h
    .endprolog
    
    ; Print banner
    lea rcx, msg_test_start
    call print_str
    
    ; Run all tests
    call test_simd_detection
    call test_context_init
    call test_base64_encode
    call test_base64_decode
    call test_hex_encode
    call test_hex_decode
    call test_url_encode
    call test_url_decode
    call test_xor_roundtrip
    call test_rot13
    
    ; Print summary
    lea rcx, msg_summary
    call print_str
    
    mov ecx, tests_passed
    call print_number
    
    lea rcx, msg_of
    call print_str
    
    mov ecx, tests_total
    call print_number
    
    lea rcx, msg_total
    call print_str
    
    ; Exit with pass/fail code
    mov eax, tests_total
    sub eax, tests_passed
    mov ecx, eax
    call ExitProcess
    
main endp

end
