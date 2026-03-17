.code

PUBLIC matmul_kernel_avx2

; void matmul_kernel_avx2(float* A, float* B, float* C, int N, int M, int K);
matmul_kernel_avx2 PROC

    ; Stub implementation to satisfy MSVC linker. Real work handled in C++ fallback.
    ret

matmul_kernel_avx2 ENDP

END

    // Conceptual C++: for (i=0 to N) { for (j=0 to K) { for (k=0 to M) { C[i][j] += A[i][k] * B[k][j] } } }

    // --- Loop Variables (Conceptual Initialization) ---
    xorq %r10, %r10  // r10 = i (N loop counter)
    xorq %r11, %r11  // r11 = j (K loop counter)

.L_N_Loop:
    cmpq %r10, %rcx  // Compare i (r10) with N (rcx)
    jge .L_End      // If i >= N, exit loop

.L_K_Loop:
    // ... Initialization for C[i][j] = 0.0f
    vmovups .L_zero_vec(%rip), %ymm0 // ymm0 = 8x 0.0f (Accumulator for C[i][j] group)
    
    // Inner loop (M loop counter k)
    movq %r12, %rax  // rax = A_ptr (A[i][k])
    movq %r13, %rbx  // rbx = B_ptr (B[k][j])
    xorq %r9, %r9    // r9 = k loop counter

.L_M_Loop:
    cmpq %r9, %r15   // Compare k (r9) with M (r15)
    jge .L_M_Loop_End // If k >= M, exit inner loop

    // --- AVX2 SIMD Core (8-way Parallel MatMul) ---
    // 1. Load A[i][k] (A_val)
    vmovss (%rax), %xmm1       // Load A[i][k] into xmm1 (scalar)
    vbroadcastss %xmm1, %ymm1  // Broadcast A_val to all 8 lanes of ymm1 (A_val_8x)

    // 2. Load B[k][j] block (B_vals - 8 floats)
    vmovups (%rbx), %ymm2      // Load B[k][j] ... B[k][j+7] into ymm2 (B_vals)

    // 3. Fused Multiply-Add (FMA)
    // ymm0 = (ymm1 * ymm2) + ymm0
    // Multiply 8x A_val_8x by 8x B_vals, and add to the running sum ymm0
    vfmadd231ps %ymm1, %ymm2, %ymm0 // ymm0 += ymm1 * ymm2 (AVX2 FMA is key for performance)

    // --- Increment Pointers and Counters ---
    addq $4, %rax    // A_ptr += sizeof(float)
    addq $(4*8), %rbx // B_ptr += sizeof(float) * 8 (moving to the next 8-float block in B)
    addq $1, %r9     // k++

    jmp .L_M_Loop
.L_M_Loop_End:
    
    // --- Store Result ---
    // Store the 8-float result from accumulator ymm0 into C[i][j]
    // The complexity of C pointer math is abstracted for this conceptual kernel.
    // vmovups %ymm0, (%r14) // Conceptual store C[i][j] ... C[i][j+7]
    
    // ... Advance j (r11) counter and jump back to .L_K_Loop ...

    jmp .L_K_Loop // Conceptual: continue K loop (j)

.L_End:
    // --- CLEANUP ---
    popq %rbx
    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %rbp
    ret

// --- Data Section ---
.section .data
.L_zero_vec:
    .long 0, 0, 0, 0, 0, 0, 0, 0 // 8x 0.0f
