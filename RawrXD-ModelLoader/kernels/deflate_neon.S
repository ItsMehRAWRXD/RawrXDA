/* kernels/deflate_neon.S */
/* ARM64/NEON kernel for brutal gzip (stored blocks + memcpy) */
/* GAS syntax, target: Apple Silicon / Windows-on-ARM */

.section .text
.global deflate_neon
.type deflate_neon, @function

deflate_neon:
    /* Prologue */
    stp     x29, x30, [sp, #-16]!   /* Save FP, LR */
    mov     x29, sp                 /* Set FP */
    stp     x19, x20, [sp, #-16]!   /* Save callee-saved regs */
    stp     x21, x22, [sp, #-16]!

    /* Args: x0=src, x1=len, x2=out_len* */
    mov     x19, x0                 /* src */
    mov     x20, x1                 /* len */
    mov     x21, x2                 /* out_len* */

    /* Calculate block count and allocation size */
    /* block_count = (len + 65534) / 65535 */
    mov     x9, x20
    mov     x10, #65534
    add     x9, x9, x10
    mov     x10, #65535
    udiv    x9, x9, x10             /* x9 = block_count */
    
    /* alloc = len + 18 + block_count * 5 */
    mov     x10, #5
    mul     x9, x9, x10
    add     x9, x9, x20
    add     x0, x9, #18             /* x0 = alloc size */

    /* Allocate memory */
    bl      malloc
    cbz     x0, .fail

    mov     x22, x0                 /* out base */
    mov     x9, x0                  /* p (current out ptr) */

    /* Gzip header (10 bytes) */
    /* 1F 8B 08 00 00 00 00 00 00 03 */
    mov     w10, #0x8B1F
    strh    w10, [x9], #2
    mov     w10, #0x08
    strb    w10, [x9], #1
    mov     w10, #0
    str     w10, [x9], #4           /* MTIME */
    mov     w10, #0
    strb    w10, [x9], #1           /* XFL */
    mov     w10, #0x03
    strb    w10, [x9], #1           /* OS */

    /* Stored blocks */
    mov     x10, x20                /* remaining bytes */
    
.block_loop:
    cmp     x10, #0
    beq     .after_blocks

    /* chunk size = min(remaining, 65535) */
    mov     x11, #65535
    cmp     x10, x11
    csel    x11, x10, x11, lo       /* x11 = chunk size */

    /* BFINAL = (remaining == chunk) ? 1 : 0 */
    cmp     x10, x11
    cset    w12, eq                 /* w12 = BFINAL */
    
    /* Write block header */
    /* BFINAL (bit 0), BTYPE=00 (bits 1-2) -> byte = BFINAL */
    strb    w12, [x9], #1
    
    /* LEN (u16 little endian) */
    strh    w11, [x9], #2
    
    /* NLEN (u16) = ~LEN */
    mvn     w12, w11
    strh    w12, [x9], #2

    /* Copy data using NEON (128-bit vector loads/stores) */
    /* x19 = src, x9 = dst, x11 = count */
    mov     x12, x11                /* copy count */
    
.copy_loop_neon:
    cmp     x12, #64
    blo     .copy_loop_32
    
    /* Unrolled 64-byte copy */
    ldp     q0, q1, [x19], #32
    ldp     q2, q3, [x19], #32
    stp     q0, q1, [x9], #32
    stp     q2, q3, [x9], #32
    sub     x12, x12, #64
    b       .copy_loop_neon

.copy_loop_32:
    cmp     x12, #32
    blo     .copy_tail
    
    ldp     q0, q1, [x19], #32
    stp     q0, q1, [x9], #32
    sub     x12, x12, #32

.copy_tail:
    /* Handle remaining bytes byte-by-byte (simple fallback) */
    cbz     x12, .copy_done
.copy_tail_loop:
    ldrb    w13, [x19], #1
    strb    w13, [x9], #1
    sub     x12, x12, #1
    cbnz    x12, .copy_tail_loop

.copy_done:
    sub     x10, x10, x11           /* remaining -= chunk */
    b       .block_loop

.after_blocks:
    /* Footer: CRC32 (0) + ISIZE (len mod 2^32) */
    mov     w10, #0
    str     w10, [x9], #4
    mov     w10, w20                /* ISIZE */
    str     w10, [x9], #4

    /* Set out_len */
    sub     x10, x9, x22
    str     x10, [x21]

    mov     x0, x22                 /* return buffer */
    b       .exit

.fail:
    mov     x0, #0

.exit:
    ldp     x21, x22, [sp], #16
    ldp     x19, x20, [sp], #16
    ldp     x29, x30, [sp], #16
    ret
