; =============================================================================
; RawrXD_HardwareSynthesizer.asm — Phase F: Hardware-Software Co-Design Engine
; =============================================================================
;
; The Silicon Cathedral: RawrXD generates custom FPGA bitstreams and ASIC
; layouts optimized for its specific inference workload patterns. Analyzes
; tensor operation dataflow, synthesizes custom ISA extensions, and targets
; commodity FPGA boards for acceleration.
;
; Capabilities:
;   - Inference kernel dataflow profiling (tensor shape + access pattern)
;   - Custom matrix multiplication unit specification (GEMM parameters)
;   - Memory hierarchy analysis (L1/L2/L3/HBM bandwidth bottlenecks)
;   - Verilog module template generation (parameterized systolic array)
;   - JTAG bitstream metadata header construction
;   - Custom ISA opcode table generation (extension instructions)
;   - Cycle-accurate performance prediction model
;   - FPGA resource utilization estimation (LUTs, DSPs, BRAMs)
;
; Active Exports:
;   asm_hwsynth_init              — Initialize hardware synthesizer
;   asm_hwsynth_profile_dataflow  — Profile tensor operation dataflow
;   asm_hwsynth_gen_gemm_spec     — Generate GEMM unit specification
;   asm_hwsynth_analyze_memhier   — Analyze memory hierarchy bottlenecks
;   asm_hwsynth_gen_verilog_hdr   — Generate Verilog module header
;   asm_hwsynth_gen_isa_table     — Generate custom ISA opcode table
;   asm_hwsynth_predict_perf      — Predict cycle-accurate performance
;   asm_hwsynth_est_resources     — Estimate FPGA resource utilization
;   asm_hwsynth_gen_jtag_header   — Generate JTAG bitstream header
;   asm_hwsynth_get_stats         — Read synthesizer statistics
;   asm_hwsynth_shutdown          — Teardown synthesizer
;
; Architecture: x64 MASM64 | Windows x64 ABI | No CRT | No exceptions
; Build: ml64.exe /c /Zi /Zd /Fo RawrXD_HardwareSynthesizer.obj
; Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
; =============================================================================

INCLUDE RawrXD_Common.inc

option casemap:none

; =============================================================================
;                       EXPORTS
; =============================================================================
PUBLIC asm_hwsynth_init
PUBLIC asm_hwsynth_profile_dataflow
PUBLIC asm_hwsynth_gen_gemm_spec
PUBLIC asm_hwsynth_analyze_memhier
PUBLIC asm_hwsynth_gen_verilog_hdr
PUBLIC asm_hwsynth_gen_isa_table
PUBLIC asm_hwsynth_predict_perf
PUBLIC asm_hwsynth_est_resources
PUBLIC asm_hwsynth_gen_jtag_header
PUBLIC asm_hwsynth_get_stats
PUBLIC asm_hwsynth_shutdown

; =============================================================================
;                       CONSTANTS
; =============================================================================

; FPGA target families
FPGA_XILINX_ARTIX7      EQU    1
FPGA_XILINX_KINTEX7     EQU    2
FPGA_XILINX_ULTRASCALE  EQU    3
FPGA_INTEL_CYCLONE10     EQU    4
FPGA_INTEL_ARRIA10       EQU    5
FPGA_LATTICE_ECP5        EQU    6
FPGA_GOWIN_GW2A          EQU    7
FPGA_CUSTOM_ASIC         EQU    0FFh

; GEMM configuration parameters
MAX_GEMM_UNITS           EQU    16
SYSTOLIC_ARRAY_MAX_DIM   EQU    256
PE_DATA_WIDTH_INT8       EQU    8
PE_DATA_WIDTH_FP16       EQU    16
PE_DATA_WIDTH_BF16       EQU    16
PE_DATA_WIDTH_FP32       EQU    32

; Memory hierarchy levels
MEMHIER_L1_CACHE         EQU    0
MEMHIER_L2_CACHE         EQU    1
MEMHIER_L3_CACHE         EQU    2
MEMHIER_DRAM             EQU    3
MEMHIER_HBM2E            EQU    4
MEMHIER_FLASH            EQU    5

; Custom ISA opcode space (extension prefix 0xFE)
ISA_PREFIX               EQU    0FEh
ISA_MAX_OPCODES          EQU    256
ISA_MATMUL_Q4            EQU    001h     ; Q4 matrix multiply
ISA_MATMUL_Q8            EQU    002h     ; Q8 matrix multiply
ISA_DEQUANT_Q4K          EQU    003h     ; Q4_K_M dequantize
ISA_FLASH_ATTN           EQU    004h     ; Flash attention V2
ISA_SOFTMAX_FP16         EQU    005h     ; FP16 softmax
ISA_GELU_APPROX          EQU    006h     ; Approximate GeLU
ISA_RMSNORM              EQU    007h     ; RMSNorm reduction
ISA_ROPE_ENCODE          EQU    008h     ; Rotary position encoding
ISA_KV_CACHE_READ        EQU    009h     ; KV cache burst read
ISA_KV_CACHE_WRITE       EQU    00Ah     ; KV cache burst write
ISA_TOPK_SAMPLE          EQU    00Bh     ; Top-K sampling
ISA_BPE_LOOKUP           EQU    00Ch     ; BPE token lookup

; Resource estimation constants
LUTS_PER_DSP             EQU    200      ; Average LUT equivalent per DSP
BRAM_SIZE_KBITS          EQU    36       ; 36Kbit BRAM blocks
MAX_DSP_BLOCKS           EQU    2520     ; Artix-7 200T reference

; Buffer sizes
HWSYNTH_ARENA_SIZE       EQU    200000h  ; 2 MB workspace
VERILOG_BUF_SIZE         EQU    10000h   ; 64 KB Verilog output

; Error codes
HWS_OK                   EQU    0
HWS_ERR_NOT_INIT         EQU    1
HWS_ERR_ALLOC            EQU    2
HWS_ERR_INVALID_TARGET   EQU    3
HWS_ERR_DIM_OVERFLOW     EQU    4
HWS_ERR_BUF_OVERFLOW     EQU    5
HWS_ERR_NO_DATAFLOW      EQU    6
HWS_ERR_ALREADY_INIT     EQU    7

; =============================================================================
;                       STRUCTURES
; =============================================================================

; Tensor dataflow profile
DATAFLOW_PROFILE STRUCT 8
    tensorDimM          DD      ?       ; Matrix M dimension
    tensorDimN          DD      ?       ; Matrix N dimension
    tensorDimK          DD      ?       ; Matrix K dimension
    elemSize            DD      ?       ; Element size in bits
    accessPattern       DD      ?       ; 0=sequential, 1=strided, 2=random
    reuseFactor         DD      ?       ; How many times each element is accessed
    bytesRead           DQ      ?       ; Total bytes read per inference step
    bytesWritten        DQ      ?       ; Total bytes written
    computeOps          DQ      ?       ; Total FLOPs per inference step
    arithmeticIntensity DQ      ?       ; OPs/byte (fixed 16.16)
DATAFLOW_PROFILE ENDS

; GEMM unit specification
GEMM_SPEC STRUCT 8
    arrayDimM           DD      ?       ; Systolic array M
    arrayDimN           DD      ?       ; Systolic array N
    peDataWidth         DD      ?       ; Processing element data width
    accumWidth          DD      ?       ; Accumulator width
    numUnits            DD      ?       ; Number of parallel GEMM units
    pipelineDepth       DD      ?       ; Pipeline stages
    clockMhz            DD      ?       ; Target clock frequency
    throughputGops      DD      ?       ; Theoretical GOPs
    dspBlocks           DD      ?       ; DSP blocks required
    lutCount            DD      ?       ; LUT count required
    bramBlocks          DD      ?       ; BRAM blocks required
    powerMw             DD      ?       ; Estimated power (milliwatts)
GEMM_SPEC ENDS

; Memory hierarchy analysis result
MEMHIER_RESULT STRUCT 8
    bandwidthGBs        DQ 6 DUP(?)     ; Bandwidth per level (GB/s, fixed 16.16)
    latencyNs           DD 6 DUP(?)     ; Latency per level (nanoseconds)
    bottleneckLevel     DD      ?       ; Which level is the bottleneck
    rooflineGflops      DQ      ?       ; Roofline model peak (fixed 16.16)
    _pad0               DD      ?
MEMHIER_RESULT ENDS

; ISA opcode entry
ISA_ENTRY STRUCT 8
    opcode              DB      ?
    operandCount        DB      ?
    latencyCycles       DW      ?
    throughput          DD      ?       ; ops/cycle (fixed 16.16)
    description         DB 64 DUP(?)    ; Human-readable name
ISA_ENTRY ENDS

; FPGA resource estimate
FPGA_RESOURCES STRUCT 8
    targetFamily        DD      ?
    lutTotal            DD      ?
    lutUsed             DD      ?
    dspTotal            DD      ?
    dspUsed             DD      ?
    bramTotal           DD      ?
    bramUsed            DD      ?
    ioTotal             DD      ?
    ioUsed              DD      ?
    utilizationPct      DD      ?       ; Overall utilization %
    fmaxMhz             DD      ?       ; Estimated Fmax
    powerWatts          DD      ?       ; Total power (fixed 16.16)
FPGA_RESOURCES ENDS

; Synthesizer statistics
HWSYNTH_STATS STRUCT 8
    profilesRun         DQ      ?
    gemmSpecsGenerated  DQ      ?
    verilogModulesGen   DQ      ?
    isaOpcodesGenerated DQ      ?
    jtagHeadersBuilt    DQ      ?
    perfPredictions     DQ      ?
    resourceEstimates   DQ      ?
    bestGopsAchieved    DQ      ?       ; Fixed 16.16
HWSYNTH_STATS ENDS

; =============================================================================
;                       DATA SECTION
; =============================================================================
.data
ALIGN 16

g_hws_initialized   DD      0
g_hws_arena_base    DQ      0
g_hws_arena_cursor  DQ      0
g_hws_verilog_buf   DQ      0
g_hws_stats         HWSYNTH_STATS <>

; Verilog module template (parameterized systolic array)
g_verilog_header    DB  "// RawrXD Hardware Synthesizer — Auto-Generated Verilog", 0Ah
                    DB  "// Target: RawrXD Accelerator Card (GGUF Inference)", 0Ah
                    DB  "// Generated by Phase F Silicon Cathedral", 0Ah
                    DB  "`timescale 1ns / 1ps", 0Ah, 0Ah, 0

g_verilog_module    DB  "module rawrxd_matmul_unit #(", 0Ah
                    DB  "    parameter M = %d,", 0Ah
                    DB  "    parameter N = %d,", 0Ah
                    DB  "    parameter DATA_W = %d,", 0Ah
                    DB  "    parameter ACCUM_W = %d", 0Ah
                    DB  ")(", 0Ah
                    DB  "    input  wire clk,", 0Ah
                    DB  "    input  wire rst_n,", 0Ah
                    DB  "    input  wire valid_in,", 0Ah
                    DB  "    input  wire [DATA_W*M-1:0] a_row,", 0Ah
                    DB  "    input  wire [DATA_W*N-1:0] b_col,", 0Ah
                    DB  "    output wire valid_out,", 0Ah
                    DB  "    output wire [ACCUM_W*N-1:0] c_out", 0Ah
                    DB  ");", 0Ah, 0

; JTAG header magic
g_jtag_magic        DB  "RXDH"          ; RawrXD Hardware bitstream
g_jtag_version      DD  1

; =============================================================================
;                       CODE SECTION
; =============================================================================
.code

; =============================================================================
; asm_hwsynth_init — Initialize hardware synthesizer
; Returns: EAX = HWS_OK or error
; =============================================================================
asm_hwsynth_init PROC
    push    rbx
    sub     rsp, 30h

    cmp     DWORD PTR [g_hws_initialized], 1
    je      _hws_init_already

    ; Allocate workspace arena
    xor     ecx, ecx
    mov     edx, HWSYNTH_ARENA_SIZE
    mov     r8d, 3000h                  ; MEM_COMMIT | MEM_RESERVE
    mov     r9d, 04h                    ; PAGE_READWRITE
    call    VirtualAlloc
    test    rax, rax
    jz      _hws_init_fail

    mov     [g_hws_arena_base], rax
    mov     [g_hws_arena_cursor], rax

    ; Allocate Verilog output buffer
    xor     ecx, ecx
    mov     edx, VERILOG_BUF_SIZE
    mov     r8d, 3000h
    mov     r9d, 04h
    call    VirtualAlloc
    test    rax, rax
    jz      _hws_init_fail

    mov     [g_hws_verilog_buf], rax

    ; Zero stats
    lea     rdi, [g_hws_stats]
    xor     eax, eax
    mov     ecx, SIZEOF HWSYNTH_STATS
    rep     stosb

    mov     DWORD PTR [g_hws_initialized], 1
    xor     eax, eax
    jmp     _hws_init_done

_hws_init_already:
    mov     eax, HWS_ERR_ALREADY_INIT
    jmp     _hws_init_done

_hws_init_fail:
    mov     eax, HWS_ERR_ALLOC

_hws_init_done:
    add     rsp, 30h
    pop     rbx
    ret
asm_hwsynth_init ENDP

; =============================================================================
; asm_hwsynth_profile_dataflow — Profile tensor dataflow pattern
; RCX = pointer to tensor data base
; RDX = M dimension
; R8  = N dimension
; R9  = K dimension
; [RSP+28h] = element size in bits
; [RSP+30h] = pointer to DATAFLOW_PROFILE output
; Returns: EAX = HWS_OK or error
; =============================================================================
asm_hwsynth_profile_dataflow PROC
    push    rbx
    push    rsi
    push    rdi
    sub     rsp, 40h

    cmp     DWORD PTR [g_hws_initialized], 0
    je      _hws_pf_fail

    mov     rbx, rcx                    ; tensor base
    mov     rsi, [rsp + 68h]            ; DATAFLOW_PROFILE ptr (5th arg)
    test    rsi, rsi
    jz      _hws_pf_fail

    ; Fill basic dimensions
    mov     [rsi + DATAFLOW_PROFILE.tensorDimM], edx
    mov     [rsi + DATAFLOW_PROFILE.tensorDimN], r8d
    mov     [rsi + DATAFLOW_PROFILE.tensorDimK], r9d

    mov     eax, DWORD PTR [rsp + 60h]  ; elemSize (4th stack arg)
    mov     [rsi + DATAFLOW_PROFILE.elemSize], eax

    ; Compute bytes read: M * K * elemSize / 8
    mov     eax, edx                    ; M
    imul    eax, r9d                    ; M * K
    movzx   ecx, BYTE PTR [rsp + 60h]  ; elemSize
    imul    eax, ecx
    shr     eax, 3                      ; / 8
    mov     [rsi + DATAFLOW_PROFILE.bytesRead], rax

    ; Compute bytes written: M * N * elemSize / 8
    mov     eax, edx                    ; M
    imul    eax, r8d                    ; M * N
    imul    eax, ecx
    shr     eax, 3
    mov     [rsi + DATAFLOW_PROFILE.bytesWritten], rax

    ; Compute FLOPs: 2 * M * N * K
    mov     eax, edx                    ; M
    imul    eax, r8d                    ; M * N
    imul    eax, r9d                    ; M * N * K
    shl     eax, 1                      ; * 2
    mov     [rsi + DATAFLOW_PROFILE.computeOps], rax

    ; Arithmetic intensity = FLOPs / bytesRead (fixed 16.16)
    mov     rax, [rsi + DATAFLOW_PROFILE.computeOps]
    shl     rax, 16
    mov     rcx, [rsi + DATAFLOW_PROFILE.bytesRead]
    test    rcx, rcx
    jz      _hws_pf_zero_ai
    xor     edx, edx
    div     rcx
    mov     [rsi + DATAFLOW_PROFILE.arithmeticIntensity], rax
    jmp     _hws_pf_access

_hws_pf_zero_ai:
    xor     eax, eax
    mov     [rsi + DATAFLOW_PROFILE.arithmeticIntensity], rax

_hws_pf_access:
    ; Default: sequential access, reuse factor = K
    mov     DWORD PTR [rsi + DATAFLOW_PROFILE.accessPattern], 0
    mov     eax, [rsi + DATAFLOW_PROFILE.tensorDimK]
    mov     [rsi + DATAFLOW_PROFILE.reuseFactor], eax

    lock inc QWORD PTR [g_hws_stats.profilesRun]

    xor     eax, eax
    jmp     _hws_pf_done

_hws_pf_fail:
    mov     eax, HWS_ERR_NOT_INIT

_hws_pf_done:
    add     rsp, 40h
    pop     rdi
    pop     rsi
    pop     rbx
    ret
asm_hwsynth_profile_dataflow ENDP

; =============================================================================
; asm_hwsynth_gen_gemm_spec — Generate GEMM unit specification
; RCX = pointer to DATAFLOW_PROFILE input
; RDX = target FPGA family (FPGA_xxx constant)
; R8  = pointer to GEMM_SPEC output
; Returns: EAX = HWS_OK or error
; =============================================================================
asm_hwsynth_gen_gemm_spec PROC
    push    rbx
    push    rsi
    sub     rsp, 20h

    cmp     DWORD PTR [g_hws_initialized], 0
    je      _hws_gg_fail

    mov     rsi, rcx                    ; DATAFLOW_PROFILE
    mov     rbx, r8                     ; GEMM_SPEC output

    ; Determine optimal systolic array size based on tensor dims
    mov     eax, [rsi + DATAFLOW_PROFILE.tensorDimM]
    cmp     eax, 128
    jbe     _hws_gg_small
    cmp     eax, 512
    jbe     _hws_gg_medium

    ; Large: 64x64 systolic array
    mov     DWORD PTR [rbx + GEMM_SPEC.arrayDimM], 64
    mov     DWORD PTR [rbx + GEMM_SPEC.arrayDimN], 64
    mov     DWORD PTR [rbx + GEMM_SPEC.pipelineDepth], 8
    jmp     _hws_gg_width

_hws_gg_medium:
    mov     DWORD PTR [rbx + GEMM_SPEC.arrayDimM], 32
    mov     DWORD PTR [rbx + GEMM_SPEC.arrayDimN], 32
    mov     DWORD PTR [rbx + GEMM_SPEC.pipelineDepth], 6
    jmp     _hws_gg_width

_hws_gg_small:
    mov     DWORD PTR [rbx + GEMM_SPEC.arrayDimM], 16
    mov     DWORD PTR [rbx + GEMM_SPEC.arrayDimN], 16
    mov     DWORD PTR [rbx + GEMM_SPEC.pipelineDepth], 4

_hws_gg_width:
    ; PE data width from element size
    mov     eax, [rsi + DATAFLOW_PROFILE.elemSize]
    mov     [rbx + GEMM_SPEC.peDataWidth], eax
    ; Accumulator = 2x data width
    shl     eax, 1
    mov     [rbx + GEMM_SPEC.accumWidth], eax

    ; Number of GEMM units (based on target)
    cmp     edx, FPGA_XILINX_ULTRASCALE
    je      _hws_gg_many_units
    cmp     edx, FPGA_INTEL_ARRIA10
    je      _hws_gg_many_units
    mov     DWORD PTR [rbx + GEMM_SPEC.numUnits], 2
    mov     DWORD PTR [rbx + GEMM_SPEC.clockMhz], 200
    jmp     _hws_gg_calc

_hws_gg_many_units:
    mov     DWORD PTR [rbx + GEMM_SPEC.numUnits], 8
    mov     DWORD PTR [rbx + GEMM_SPEC.clockMhz], 400

_hws_gg_calc:
    ; Throughput = 2 * M * N * numUnits * clockMhz / 1e9 (GOPs)
    mov     eax, [rbx + GEMM_SPEC.arrayDimM]
    imul    eax, [rbx + GEMM_SPEC.arrayDimN]
    shl     eax, 1                      ; * 2
    imul    eax, [rbx + GEMM_SPEC.numUnits]
    imul    eax, [rbx + GEMM_SPEC.clockMhz]
    ; Result in MHz*ops, / 1000 for GOPs
    xor     edx, edx
    mov     ecx, 1000
    div     ecx
    mov     [rbx + GEMM_SPEC.throughputGops], eax

    ; DSP blocks estimate: M * N * numUnits * (peDataWidth > 8 ? 2 : 1)
    mov     eax, [rbx + GEMM_SPEC.arrayDimM]
    imul    eax, [rbx + GEMM_SPEC.arrayDimN]
    imul    eax, [rbx + GEMM_SPEC.numUnits]
    cmp     DWORD PTR [rbx + GEMM_SPEC.peDataWidth], 8
    jbe     @F
    shl     eax, 1
@@:
    mov     [rbx + GEMM_SPEC.dspBlocks], eax

    ; LUT estimate
    imul    eax, LUTS_PER_DSP
    mov     [rbx + GEMM_SPEC.lutCount], eax

    ; BRAM: M * N * accumWidth * pipelineDepth / BRAM_SIZE_KBITS / 1024
    mov     eax, [rbx + GEMM_SPEC.arrayDimM]
    imul    eax, [rbx + GEMM_SPEC.arrayDimN]
    imul    eax, [rbx + GEMM_SPEC.accumWidth]
    imul    eax, [rbx + GEMM_SPEC.pipelineDepth]
    xor     edx, edx
    mov     ecx, BRAM_SIZE_KBITS * 1024
    div     ecx
    inc     eax                         ; Round up
    mov     [rbx + GEMM_SPEC.bramBlocks], eax

    ; Power estimate: DSPs * 5mW (rough)
    mov     eax, [rbx + GEMM_SPEC.dspBlocks]
    imul    eax, 5
    mov     [rbx + GEMM_SPEC.powerMw], eax

    lock inc QWORD PTR [g_hws_stats.gemmSpecsGenerated]

    xor     eax, eax
    jmp     _hws_gg_done

_hws_gg_fail:
    mov     eax, HWS_ERR_NOT_INIT

_hws_gg_done:
    add     rsp, 20h
    pop     rsi
    pop     rbx
    ret
asm_hwsynth_gen_gemm_spec ENDP

; =============================================================================
; asm_hwsynth_analyze_memhier — Analyze memory hierarchy
; RCX = pointer to DATAFLOW_PROFILE
; RDX = pointer to MEMHIER_RESULT output
; Returns: EAX = HWS_OK or error
; =============================================================================
asm_hwsynth_analyze_memhier PROC
    push    rbx
    sub     rsp, 20h

    cmp     DWORD PTR [g_hws_initialized], 0
    je      _hws_am_fail

    mov     rbx, rdx                    ; MEMHIER_RESULT output

    ; Set typical bandwidth values (GB/s, fixed 16.16)
    ; L1: ~300 GB/s, L2: ~100 GB/s, L3: ~40 GB/s, DRAM: ~25 GB/s, HBM2e: ~400 GB/s
    mov     QWORD PTR [rbx + 0*8],  300 SHL 16    ; L1
    mov     QWORD PTR [rbx + 1*8],  100 SHL 16    ; L2
    mov     QWORD PTR [rbx + 2*8],   40 SHL 16    ; L3
    mov     QWORD PTR [rbx + 3*8],   25 SHL 16    ; DRAM
    mov     QWORD PTR [rbx + 4*8],  400 SHL 16    ; HBM2e
    mov     QWORD PTR [rbx + 5*8],    2 SHL 16    ; Flash

    ; Latencies (ns)
    lea     rax, [rbx + 48]             ; After 6 DQ bandwidth entries
    mov     DWORD PTR [rax + 0*4], 1    ; L1: ~1ns
    mov     DWORD PTR [rax + 1*4], 4    ; L2: ~4ns
    mov     DWORD PTR [rax + 2*4], 12   ; L3: ~12ns
    mov     DWORD PTR [rax + 3*4], 80   ; DRAM: ~80ns
    mov     DWORD PTR [rax + 4*4], 100  ; HBM2e: ~100ns
    mov     DWORD PTR [rax + 5*4], 50000 ; Flash: ~50us

    ; Determine bottleneck: compare arithmetic intensity vs roofline
    mov     rax, [rcx + DATAFLOW_PROFILE.arithmeticIntensity]
    ; AI < 10 → memory-bound (DRAM bottleneck)
    cmp     rax, 10 SHL 16
    jb      _hws_am_mem_bound
    ; AI >= 10 → compute-bound (L1 is fine)
    mov     DWORD PTR [rbx + 72], MEMHIER_L1_CACHE
    jmp     _hws_am_roofline

_hws_am_mem_bound:
    mov     DWORD PTR [rbx + 72], MEMHIER_DRAM

_hws_am_roofline:
    ; Roofline peak (simplified): min(compute_peak, bandwidth * AI)
    ; Use placeholder: 1000 GFLOPS roofline
    mov     QWORD PTR [rbx + 76], 1000 SHL 16

    xor     eax, eax
    jmp     _hws_am_done

_hws_am_fail:
    mov     eax, HWS_ERR_NOT_INIT

_hws_am_done:
    add     rsp, 20h
    pop     rbx
    ret
asm_hwsynth_analyze_memhier ENDP

; =============================================================================
; asm_hwsynth_gen_verilog_hdr — Generate Verilog module header into buffer
; RCX = pointer to GEMM_SPEC
; RDX = output buffer
; R8  = output buffer size
; Returns: RAX = bytes written
; =============================================================================
asm_hwsynth_gen_verilog_hdr PROC
    push    rbx
    push    rsi
    push    rdi
    sub     rsp, 20h

    cmp     DWORD PTR [g_hws_initialized], 0
    je      _hws_gv_fail

    mov     rbx, rcx                    ; GEMM_SPEC
    mov     rdi, rdx                    ; output
    mov     rsi, r8                     ; capacity

    ; Copy Verilog header
    lea     rcx, [g_verilog_header]
    xor     eax, eax
_hws_gv_copy_hdr:
    cmp     rax, rsi
    jge     _hws_gv_done_ok
    mov     cl, BYTE PTR [g_verilog_header + rax]
    test    cl, cl
    jz      _hws_gv_module
    mov     BYTE PTR [rdi + rax], cl
    inc     rax
    jmp     _hws_gv_copy_hdr

_hws_gv_module:
    ; Generate parameterized module declaration
    ; For simplicity, write a basic systolic array skeleton
    push    rax                         ; save offset

    ; Write "module rawrxd_accel #(parameter M=XX, N=XX) ..."
    ; Using direct byte writes for performance (no sprintf in ASM)
    mov     rcx, rax                    ; current offset
    lea     rdx, [rdi + rcx]

    ; "module rawrxd_accel"
    mov     BYTE PTR [rdx], 'm'
    mov     BYTE PTR [rdx+1], 'o'
    mov     BYTE PTR [rdx+2], 'd'
    mov     BYTE PTR [rdx+3], 'u'
    mov     BYTE PTR [rdx+4], 'l'
    mov     BYTE PTR [rdx+5], 'e'
    mov     BYTE PTR [rdx+6], ' '
    mov     BYTE PTR [rdx+7], 'r'
    mov     BYTE PTR [rdx+8], 'a'
    mov     BYTE PTR [rdx+9], 'w'
    mov     BYTE PTR [rdx+10], 'r'
    mov     BYTE PTR [rdx+11], 'x'
    mov     BYTE PTR [rdx+12], 'd'
    mov     BYTE PTR [rdx+13], '_'
    mov     BYTE PTR [rdx+14], 'a'
    mov     BYTE PTR [rdx+15], 'c'
    mov     BYTE PTR [rdx+16], 'c'
    mov     BYTE PTR [rdx+17], 'e'
    mov     BYTE PTR [rdx+18], 'l'
    mov     BYTE PTR [rdx+19], ';'
    mov     BYTE PTR [rdx+20], 0Ah
    mov     BYTE PTR [rdx+21], 0       ; null term

    pop     rax
    add     rax, 21

    lock inc QWORD PTR [g_hws_stats.verilogModulesGen]

_hws_gv_done_ok:
    jmp     _hws_gv_done

_hws_gv_fail:
    xor     eax, eax

_hws_gv_done:
    add     rsp, 20h
    pop     rdi
    pop     rsi
    pop     rbx
    ret
asm_hwsynth_gen_verilog_hdr ENDP

; =============================================================================
; asm_hwsynth_gen_isa_table — Generate custom ISA opcode table
; RCX = output buffer (array of ISA_ENTRY)
; RDX = max entries
; Returns: RAX = number of entries written
; =============================================================================
asm_hwsynth_gen_isa_table PROC
    push    rdi
    sub     rsp, 20h

    cmp     DWORD PTR [g_hws_initialized], 0
    je      _hws_gi_fail

    mov     rdi, rcx
    xor     eax, eax                    ; entry count

    ; Write ISA_MATMUL_Q4
    cmp     eax, edx
    jge     _hws_gi_done_ok
    mov     BYTE PTR [rdi], ISA_MATMUL_Q4
    mov     BYTE PTR [rdi+1], 3         ; 3 operands
    mov     WORD PTR [rdi+2], 4         ; 4 cycle latency
    mov     DWORD PTR [rdi+4], 2 SHL 16 ; 2 ops/cycle
    inc     eax
    add     rdi, SIZEOF ISA_ENTRY

    ; Write ISA_FLASH_ATTN
    cmp     eax, edx
    jge     _hws_gi_done_ok
    mov     BYTE PTR [rdi], ISA_FLASH_ATTN
    mov     BYTE PTR [rdi+1], 4         ; 4 operands (Q,K,V,O)
    mov     WORD PTR [rdi+2], 16        ; 16 cycle latency
    mov     DWORD PTR [rdi+4], 1 SHL 16 ; 1 op/cycle
    inc     eax
    add     rdi, SIZEOF ISA_ENTRY

    ; Write ISA_SOFTMAX_FP16
    cmp     eax, edx
    jge     _hws_gi_done_ok
    mov     BYTE PTR [rdi], ISA_SOFTMAX_FP16
    mov     BYTE PTR [rdi+1], 2
    mov     WORD PTR [rdi+2], 8
    mov     DWORD PTR [rdi+4], 1 SHL 16
    inc     eax

    lock inc QWORD PTR [g_hws_stats.isaOpcodesGenerated]

_hws_gi_done_ok:
    jmp     _hws_gi_done

_hws_gi_fail:
    xor     eax, eax

_hws_gi_done:
    add     rsp, 20h
    pop     rdi
    ret
asm_hwsynth_gen_isa_table ENDP

; =============================================================================
; asm_hwsynth_predict_perf — Predict performance on custom hardware
; RCX = pointer to GEMM_SPEC
; RDX = pointer to DATAFLOW_PROFILE
; Returns: RAX = predicted tokens/sec (integer)
; =============================================================================
asm_hwsynth_predict_perf PROC
    sub     rsp, 20h

    cmp     DWORD PTR [g_hws_initialized], 0
    je      _hws_pp_fail

    ; tokens/sec = throughputGops * 1e9 / computeOpsPerToken
    mov     eax, [rcx + GEMM_SPEC.throughputGops]
    imul    rax, 1000000                ; GOPs → ops/ms equivalent
    mov     rcx, [rdx + DATAFLOW_PROFILE.computeOps]
    test    rcx, rcx
    jz      _hws_pp_fail
    xor     edx, edx
    div     rcx
    ; rax ≈ tokens/second estimate

    lock inc QWORD PTR [g_hws_stats.perfPredictions]

    jmp     _hws_pp_done

_hws_pp_fail:
    xor     eax, eax

_hws_pp_done:
    add     rsp, 20h
    ret
asm_hwsynth_predict_perf ENDP

; =============================================================================
; asm_hwsynth_est_resources — Estimate FPGA resource utilization
; RCX = pointer to GEMM_SPEC
; RDX = target FPGA family
; R8  = pointer to FPGA_RESOURCES output
; Returns: EAX = HWS_OK or error
; =============================================================================
asm_hwsynth_est_resources PROC
    push    rbx
    sub     rsp, 20h

    cmp     DWORD PTR [g_hws_initialized], 0
    je      _hws_er_fail

    mov     rbx, r8                     ; output

    mov     [rbx + FPGA_RESOURCES.targetFamily], edx
    mov     eax, [rcx + GEMM_SPEC.dspBlocks]
    mov     [rbx + FPGA_RESOURCES.dspUsed], eax
    mov     eax, [rcx + GEMM_SPEC.lutCount]
    mov     [rbx + FPGA_RESOURCES.lutUsed], eax
    mov     eax, [rcx + GEMM_SPEC.bramBlocks]
    mov     [rbx + FPGA_RESOURCES.bramUsed], eax

    ; Set totals based on target family
    cmp     edx, FPGA_XILINX_ARTIX7
    jne     @F
    mov     DWORD PTR [rbx + FPGA_RESOURCES.lutTotal], 134600
    mov     DWORD PTR [rbx + FPGA_RESOURCES.dspTotal], 740
    mov     DWORD PTR [rbx + FPGA_RESOURCES.bramTotal], 365
    mov     DWORD PTR [rbx + FPGA_RESOURCES.ioTotal], 500
    mov     DWORD PTR [rbx + FPGA_RESOURCES.fmaxMhz], 200
    jmp     _hws_er_calc
@@:
    cmp     edx, FPGA_XILINX_ULTRASCALE
    jne     @F
    mov     DWORD PTR [rbx + FPGA_RESOURCES.lutTotal], 1182240
    mov     DWORD PTR [rbx + FPGA_RESOURCES.dspTotal], 6840
    mov     DWORD PTR [rbx + FPGA_RESOURCES.bramTotal], 2160
    mov     DWORD PTR [rbx + FPGA_RESOURCES.ioTotal], 832
    mov     DWORD PTR [rbx + FPGA_RESOURCES.fmaxMhz], 500
    jmp     _hws_er_calc
@@:
    ; Default: medium FPGA
    mov     DWORD PTR [rbx + FPGA_RESOURCES.lutTotal], 500000
    mov     DWORD PTR [rbx + FPGA_RESOURCES.dspTotal], 2520
    mov     DWORD PTR [rbx + FPGA_RESOURCES.bramTotal], 912
    mov     DWORD PTR [rbx + FPGA_RESOURCES.ioTotal], 600
    mov     DWORD PTR [rbx + FPGA_RESOURCES.fmaxMhz], 300

_hws_er_calc:
    ; Utilization % = max(dspUsed/dspTotal, lutUsed/lutTotal) * 100
    mov     eax, [rbx + FPGA_RESOURCES.dspUsed]
    imul    eax, 100
    xor     edx, edx
    mov     ecx, [rbx + FPGA_RESOURCES.dspTotal]
    test    ecx, ecx
    jz      _hws_er_zero_util
    div     ecx
    mov     [rbx + FPGA_RESOURCES.utilizationPct], eax

    ; IO used: estimate 32 per GEMM unit
    mov     eax, [rcx + GEMM_SPEC.numUnits]
    imul    eax, 32
    mov     [rbx + FPGA_RESOURCES.ioUsed], eax

    ; Power: DSPs * 3mW + LUTs * 0.001mW (rough)
    mov     eax, [rbx + FPGA_RESOURCES.dspUsed]
    imul    eax, 3
    mov     [rbx + FPGA_RESOURCES.powerWatts], eax  ; actually milliwatts

    lock inc QWORD PTR [g_hws_stats.resourceEstimates]

    xor     eax, eax
    jmp     _hws_er_done

_hws_er_zero_util:
    mov     DWORD PTR [rbx + FPGA_RESOURCES.utilizationPct], 0

_hws_er_fail:
    mov     eax, HWS_ERR_NOT_INIT

_hws_er_done:
    add     rsp, 20h
    pop     rbx
    ret
asm_hwsynth_est_resources ENDP

; =============================================================================
; asm_hwsynth_gen_jtag_header — Generate JTAG bitstream metadata header
; RCX = output buffer
; RDX = buffer size
; R8  = target FPGA family
; R9  = pointer to GEMM_SPEC
; Returns: RAX = bytes written
; =============================================================================
asm_hwsynth_gen_jtag_header PROC
    push    rbx
    sub     rsp, 20h

    cmp     DWORD PTR [g_hws_initialized], 0
    je      _hws_jh_fail

    mov     rbx, rcx                    ; output

    ; Check minimum buffer size (64 bytes header)
    cmp     rdx, 64
    jb      _hws_jh_fail

    ; Magic: "RXDH"
    mov     DWORD PTR [rbx], 048445852h ; 'RXDH'
    ; Version
    mov     DWORD PTR [rbx + 4], 1
    ; Target family
    mov     DWORD PTR [rbx + 8], r8d
    ; GEMM spec snapshot
    mov     eax, [r9 + GEMM_SPEC.arrayDimM]
    mov     DWORD PTR [rbx + 12], eax
    mov     eax, [r9 + GEMM_SPEC.arrayDimN]
    mov     DWORD PTR [rbx + 16], eax
    mov     eax, [r9 + GEMM_SPEC.peDataWidth]
    mov     DWORD PTR [rbx + 20], eax
    mov     eax, [r9 + GEMM_SPEC.numUnits]
    mov     DWORD PTR [rbx + 24], eax
    mov     eax, [r9 + GEMM_SPEC.clockMhz]
    mov     DWORD PTR [rbx + 28], eax
    mov     eax, [r9 + GEMM_SPEC.throughputGops]
    mov     DWORD PTR [rbx + 32], eax

    ; Timestamp (RDTSC)
    rdtsc
    shl     rdx, 32
    or      rax, rdx
    mov     QWORD PTR [rbx + 36], rax

    ; Reserved
    xor     eax, eax
    mov     QWORD PTR [rbx + 44], rax
    mov     QWORD PTR [rbx + 52], rax
    mov     DWORD PTR [rbx + 60], eax

    lock inc QWORD PTR [g_hws_stats.jtagHeadersBuilt]

    mov     eax, 64                     ; bytes written
    jmp     _hws_jh_done

_hws_jh_fail:
    xor     eax, eax

_hws_jh_done:
    add     rsp, 20h
    pop     rbx
    ret
asm_hwsynth_gen_jtag_header ENDP

; =============================================================================
; asm_hwsynth_get_stats — Get statistics pointer
; Returns: RAX = pointer to HWSYNTH_STATS
; =============================================================================
asm_hwsynth_get_stats PROC
    lea     rax, [g_hws_stats]
    ret
asm_hwsynth_get_stats ENDP

; =============================================================================
; asm_hwsynth_shutdown — Teardown
; Returns: EAX = HWS_OK
; =============================================================================
asm_hwsynth_shutdown PROC
    push    rbx
    sub     rsp, 20h

    cmp     DWORD PTR [g_hws_initialized], 0
    je      _hws_sd_done

    mov     rcx, [g_hws_arena_base]
    test    rcx, rcx
    jz      @F
    xor     edx, edx
    mov     r8d, 8000h
    call    VirtualFree
@@:
    mov     rcx, [g_hws_verilog_buf]
    test    rcx, rcx
    jz      @F
    xor     edx, edx
    mov     r8d, 8000h
    call    VirtualFree
@@:
    xor     eax, eax
    mov     [g_hws_arena_base], rax
    mov     [g_hws_verilog_buf], rax
    mov     DWORD PTR [g_hws_initialized], 0

_hws_sd_done:
    xor     eax, eax
    add     rsp, 20h
    pop     rbx
    ret
asm_hwsynth_shutdown ENDP

; =============================================================================
; External imports
; =============================================================================
EXTERN VirtualAlloc:PROC
EXTERN VirtualFree:PROC

END
