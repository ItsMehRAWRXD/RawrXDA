option casemap:none

.data
newline_char db 10

.code

ndjson_parse_chunk proc
    ; rcx = buffer
    ; rdx = length
    test rcx, rcx
    jz done
    test rdx, rdx
    jz done

    mov r8, rcx          ; line_start
    mov r9, rcx          ; cursor
    mov r10, rdx         ; remaining bytes

scan_loop:
    cmp r10, 0
    je flush_tail

    mov al, byte ptr [r9]
    cmp al, newline_char
    jne advance

    mov rcx, r8
    mov rdx, r9
    sub rdx, r8
    test rdx, rdx
    jz after_emit

after_emit:
    lea r8, [r9+1]

advance:
    inc r9
    dec r10
    jmp scan_loop

flush_tail:
    mov rcx, r8
    mov rdx, r9
    sub rdx, r8
    test rdx, rdx
    jz done

done:
    ret
ndjson_parse_chunk endp

end