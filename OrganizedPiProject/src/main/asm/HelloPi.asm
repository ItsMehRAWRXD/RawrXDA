[BITS 16]
[ORG 0x7C00]

jmp start

start:
    mov ah, 0x0E
    mov al, 'π'
    int 0x10
    mov al, ' '
    int 0x10
    mov al, 'A'
    int 0x10
    mov al, 'S'
    int 0x10
    mov al, 'M'
    int 0x10
    hlt

times 510-($-$$) db 0
dw 0xAA55