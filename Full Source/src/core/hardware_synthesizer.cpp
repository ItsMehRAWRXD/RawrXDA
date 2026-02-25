// hardware_synthesizer.cpp — Phase F: Hardware-Software Co-Design Engine
//
// C++20 orchestrator for the MASM64 hardware synthesis kernel. Full design
// flow: profile → spec → synthesize → estimate → generate Verilog → JTAG.
//
// Architecture: C++20 bridge → MASM64 HardwareSynthesizer kernel
// Threading: mutex-protected
// Error model: PatchResult (no exceptions)
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "hardware_synthesizer.hpp"
#include <cstring>
#include <cstdio>

// ---------------------------------------------------------------------------
// Singleton
// ---------------------------------------------------------------------------
HardwareSynthesizer& HardwareSynthesizer::instance() {
    static HardwareSynthesizer s_instance;
    return s_instance;
}

HardwareSynthesizer::HardwareSynthesizer() : m_initialized(false) {}
HardwareSynthesizer::~HardwareSynthesizer() { if (m_initialized) shutdown(); }

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------
PatchResult HardwareSynthesizer::initialize() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_initialized) return PatchResult::ok("HWSynth already initialized");
#ifdef RAWR_HAS_MASM
    int rc = asm_hwsynth_init();
    if (rc != 0) return PatchResult::error("HWSynth ASM init failed", rc);
#endif
    m_initialized = true;
    notifyCallback("hwsynth_initialized", nullptr);
    return PatchResult::ok("Hardware Synthesizer initialized");
}

bool HardwareSynthesizer::isInitialized() const { return m_initialized; }

PatchResult HardwareSynthesizer::shutdown() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_initialized) return PatchResult::ok("HWSynth not initialized");
#ifdef RAWR_HAS_MASM
    asm_hwsynth_shutdown();
#endif
    m_designs.clear();
    m_initialized = false;
    return PatchResult::ok("Hardware Synthesizer shut down");
}

// ---------------------------------------------------------------------------
// Dataflow Profiling
// ---------------------------------------------------------------------------
PatchResult HardwareSynthesizer::profileTensorDataflow(
    const void* tensorBase, uint32_t M, uint32_t N,
    uint32_t K, uint32_t elemBits, DataflowProfile* out) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_initialized) return PatchResult::error("HWSynth not initialized");
    if (!out) return PatchResult::error("Null output");

#ifdef RAWR_HAS_MASM
    int rc = asm_hwsynth_profile_dataflow(tensorBase, M, N, K, elemBits, out);
    if (rc != 0) return PatchResult::error("Dataflow profile failed", rc);
#else
    out->tensorDimM = M;
    out->tensorDimN = N;
    out->tensorDimK = K;
    out->elemSize = elemBits;
    out->accessPattern = 0;
    out->reuseFactor = K;
    out->bytesRead = static_cast<uint64_t>(M) * K * elemBits / 8;
    out->bytesWritten = static_cast<uint64_t>(M) * N * elemBits / 8;
    out->computeOps = 2ULL * M * N * K;
    out->arithmeticIntensity = out->bytesRead ? (out->computeOps << 16) / out->bytesRead : 0;
#endif
    return PatchResult::ok("Dataflow profiled");
}

// ---------------------------------------------------------------------------
// GEMM Unit Design
// ---------------------------------------------------------------------------
PatchResult HardwareSynthesizer::designGEMMUnit(
    const DataflowProfile& profile, FPGAFamily target, GemmSpec* out) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_initialized) return PatchResult::error("HWSynth not initialized");
    if (!out) return PatchResult::error("Null output");

#ifdef RAWR_HAS_MASM
    int rc = asm_hwsynth_gen_gemm_spec(&profile, static_cast<uint32_t>(target), out);
    if (rc != 0) return PatchResult::error("GEMM spec generation failed", rc);
#else
    std::memset(out, 0, sizeof(GemmSpec));
    out->arrayDimM = (profile.tensorDimM > 128) ? 32 : 16;
    out->arrayDimN = out->arrayDimM;
    out->peDataWidth = profile.elemSize;
    out->accumWidth = profile.elemSize * 2;
    out->numUnits = (target == FPGAFamily::XilinxUltraScale) ? 8 : 2;
    out->pipelineDepth = 4;
    out->clockMhz = 200;
    out->throughputGops = 2 * out->arrayDimM * out->arrayDimN * out->numUnits * out->clockMhz / 1000;
    out->dspBlocks = out->arrayDimM * out->arrayDimN * out->numUnits;
    out->lutCount = out->dspBlocks * 200;
    out->bramBlocks = out->arrayDimM * out->arrayDimN * out->accumWidth * out->pipelineDepth / (36 * 1024) + 1;
    out->powerMw = out->dspBlocks * 5;
#endif
    return PatchResult::ok("GEMM unit designed");
}

// ---------------------------------------------------------------------------
// Memory Hierarchy Analysis
// ---------------------------------------------------------------------------
PatchResult HardwareSynthesizer::analyzeMemoryHierarchy(
    const DataflowProfile& profile, MemoryHierarchyResult* out) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_initialized) return PatchResult::error("HWSynth not initialized");

#ifdef RAWR_HAS_MASM
    int rc = asm_hwsynth_analyze_memhier(&profile, out);
    if (rc != 0) return PatchResult::error("Memory hierarchy analysis failed", rc);
#else
    const uint64_t FP16 = 65536;
    out->bandwidthGBs[0] = 300 * FP16;  // L1
    out->bandwidthGBs[1] = 100 * FP16;  // L2
    out->bandwidthGBs[2] = 40 * FP16;   // L3
    out->bandwidthGBs[3] = 25 * FP16;   // DRAM
    out->bandwidthGBs[4] = 400 * FP16;  // HBM2e
    out->bandwidthGBs[5] = 2 * FP16;    // Flash
    out->latencyNs[0] = 1;
    out->latencyNs[1] = 4;
    out->latencyNs[2] = 12;
    out->latencyNs[3] = 80;
    out->latencyNs[4] = 100;
    out->latencyNs[5] = 50000;
    out->bottleneckLevel = (profile.arithmeticIntensity < 10 * FP16) ? 3 : 0;
    out->rooflineGflops = 1000 * FP16;
#endif
    return PatchResult::ok("Memory hierarchy analyzed");
}

// ---------------------------------------------------------------------------
// Verilog Generation
// ---------------------------------------------------------------------------
PatchResult HardwareSynthesizer::generateVerilog(
    const GemmSpec& spec, std::string& outVerilog) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_initialized) return PatchResult::error("HWSynth not initialized");

    char buf[8192];
    int len = snprintf(buf, sizeof(buf),
        "// RawrXD Hardware Synthesizer — Auto-Generated Verilog\n"
        "// Target: RawrXD Accelerator Card (GGUF Inference)\n"
        "// Generated by Phase F Silicon Cathedral\n"
        "`timescale 1ns / 1ps\n\n"
        "module rawrxd_matmul_unit #(\n"
        "    parameter M       = %u,\n"
        "    parameter N       = %u,\n"
        "    parameter DATA_W  = %u,\n"
        "    parameter ACCUM_W = %u,\n"
        "    parameter UNITS   = %u,\n"
        "    parameter PIPE    = %u\n"
        ")(\n"
        "    input  wire clk,\n"
        "    input  wire rst_n,\n"
        "    input  wire valid_in,\n"
        "    input  wire [DATA_W*M-1:0] a_row,\n"
        "    input  wire [DATA_W*N-1:0] b_col,\n"
        "    output reg  valid_out,\n"
        "    output reg  [ACCUM_W*N-1:0] c_out\n"
        ");\n\n"
        "    // Systolic array processing elements\n"
        "    genvar i, j;\n"
        "    generate\n"
        "        for (i = 0; i < M; i = i + 1) begin : row\n"
        "            for (j = 0; j < N; j = j + 1) begin : col\n"
        "                reg [ACCUM_W-1:0] acc;\n"
        "                wire [DATA_W-1:0] a_val = a_row[DATA_W*(i+1)-1 : DATA_W*i];\n"
        "                wire [DATA_W-1:0] b_val = b_col[DATA_W*(j+1)-1 : DATA_W*j];\n"
        "                always @(posedge clk or negedge rst_n) begin\n"
        "                    if (!rst_n)\n"
        "                        acc <= 0;\n"
        "                    else if (valid_in)\n"
        "                        acc <= acc + a_val * b_val;\n"
        "                end\n"
        "                assign c_out[ACCUM_W*(j+1)-1 : ACCUM_W*j] = acc;\n"
        "            end\n"
        "        end\n"
        "    endgenerate\n\n"
        "    // Pipeline valid signal\n"
        "    reg [PIPE-1:0] valid_pipe;\n"
        "    always @(posedge clk or negedge rst_n) begin\n"
        "        if (!rst_n)\n"
        "            valid_pipe <= 0;\n"
        "        else\n"
        "            valid_pipe <= {valid_pipe[PIPE-2:0], valid_in};\n"
        "    end\n"
        "    assign valid_out = valid_pipe[PIPE-1];\n\n"
        "endmodule\n",
        spec.arrayDimM, spec.arrayDimN, spec.peDataWidth,
        spec.accumWidth, spec.numUnits, spec.pipelineDepth);

    outVerilog.assign(buf, len);
    return PatchResult::ok("Verilog generated");
}

// ---------------------------------------------------------------------------
// Custom ISA
// ---------------------------------------------------------------------------
PatchResult HardwareSynthesizer::generateCustomISA(std::vector<ISAEntry>& outEntries) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_initialized) return PatchResult::error("HWSynth not initialized");

    outEntries.clear();
    outEntries.reserve(12);

    auto addEntry = [&](uint8_t op, uint8_t operands, uint16_t lat,
                        uint32_t throughput, const char* desc) {
        ISAEntry e{};
        e.opcode = op;
        e.operandCount = operands;
        e.latencyCycles = lat;
        e.throughput = throughput;
        snprintf(e.description, sizeof(e.description), "%s", desc);
        outEntries.push_back(e);
    };

    addEntry(0x01, 3, 4,  2 << 16, "MATMUL_Q4 — Q4 matrix multiply");
    addEntry(0x02, 3, 4,  2 << 16, "MATMUL_Q8 — Q8 matrix multiply");
    addEntry(0x03, 2, 2,  4 << 16, "DEQUANT_Q4K — Q4_K_M dequantize");
    addEntry(0x04, 4, 16, 1 << 16, "FLASH_ATTN — Flash Attention V2");
    addEntry(0x05, 2, 8,  1 << 16, "SOFTMAX_FP16 — FP16 softmax");
    addEntry(0x06, 2, 4,  2 << 16, "GELU_APPROX — Approximate GeLU");
    addEntry(0x07, 2, 4,  2 << 16, "RMSNORM — RMSNorm reduction");
    addEntry(0x08, 3, 4,  2 << 16, "ROPE_ENCODE — Rotary position encoding");
    addEntry(0x09, 2, 2,  8 << 16, "KV_CACHE_READ — KV cache burst read");
    addEntry(0x0A, 2, 2,  8 << 16, "KV_CACHE_WRITE — KV cache burst write");
    addEntry(0x0B, 2, 8,  1 << 16, "TOPK_SAMPLE — Top-K sampling");
    addEntry(0x0C, 2, 2,  4 << 16, "BPE_LOOKUP — BPE token lookup");

    return PatchResult::ok("Custom ISA generated (12 opcodes)");
}

// ---------------------------------------------------------------------------
// Performance Prediction
// ---------------------------------------------------------------------------
uint64_t HardwareSynthesizer::predictTokensPerSecond(
    const GemmSpec& spec, const DataflowProfile& profile) {
#ifdef RAWR_HAS_MASM
    return asm_hwsynth_predict_perf(&spec, &profile);
#else
    if (profile.computeOps == 0) return 0;
    uint64_t opsPerSec = static_cast<uint64_t>(spec.throughputGops) * 1000000000ULL;
    return opsPerSec / profile.computeOps;
#endif
}

// ---------------------------------------------------------------------------
// Resource Estimation
// ---------------------------------------------------------------------------
PatchResult HardwareSynthesizer::estimateResources(
    const GemmSpec& spec, FPGAFamily target, FPGAResources* out) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_initialized) return PatchResult::error("HWSynth not initialized");

#ifdef RAWR_HAS_MASM
    int rc = asm_hwsynth_est_resources(&spec, static_cast<uint32_t>(target), out);
    if (rc != 0) return PatchResult::error("Resource estimation failed", rc);
#else
    std::memset(out, 0, sizeof(FPGAResources));
    out->targetFamily = static_cast<uint32_t>(target);
    out->dspUsed = spec.dspBlocks;
    out->lutUsed = spec.lutCount;
    out->bramUsed = spec.bramBlocks;
    out->dspTotal = 2520;
    out->lutTotal = 500000;
    out->bramTotal = 912;
    out->ioTotal = 600;
    out->ioUsed = spec.numUnits * 32;
    out->utilizationPct = (out->dspTotal > 0) ? (out->dspUsed * 100 / out->dspTotal) : 0;
    out->fmaxMhz = spec.clockMhz;
    out->powerWatts = out->dspUsed * 3;
#endif
    return PatchResult::ok("Resources estimated");
}

// ---------------------------------------------------------------------------
// Full Design Flow
// ---------------------------------------------------------------------------
PatchResult HardwareSynthesizer::synthesizeAccelerator(
    const char* name, FPGAFamily target,
    uint32_t M, uint32_t N, uint32_t K, uint32_t elemBits) {
    AcceleratorDesign design;
    design.name = name;
    design.target = target;

    PatchResult r = profileTensorDataflow(nullptr, M, N, K, elemBits, &design.dataflow);
    if (!r.success) return r;

    r = designGEMMUnit(design.dataflow, target, &design.gemmSpec);
    if (!r.success) return r;

    r = analyzeMemoryHierarchy(design.dataflow, &design.memHierarchy);
    if (!r.success) return r;

    r = estimateResources(design.gemmSpec, target, &design.resources);
    if (!r.success) return r;

    r = generateCustomISA(design.customISA);
    if (!r.success) return r;

    r = generateVerilog(design.gemmSpec, design.verilogSource);
    if (!r.success) return r;

    design.predictedTokensSec = predictTokensPerSecond(design.gemmSpec, design.dataflow);

    LARGE_INTEGER ts;
    QueryPerformanceCounter(&ts);
    design.timestamp = static_cast<uint64_t>(ts.QuadPart);

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_designs.push_back(std::move(design));
        notifyCallback("accelerator_synthesized", &m_designs.back());
    }

    return PatchResult::ok("Accelerator synthesized");
}

// ---------------------------------------------------------------------------
// JTAG Bitstream
// ---------------------------------------------------------------------------
PatchResult HardwareSynthesizer::generateJTAGHeader(
    FPGAFamily target, const GemmSpec& spec, std::vector<uint8_t>& outHeader) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_initialized) return PatchResult::error("HWSynth not initialized");

    outHeader.resize(64);
#ifdef RAWR_HAS_MASM
    uint64_t written = asm_hwsynth_gen_jtag_header(outHeader.data(), 64,
        static_cast<uint32_t>(target), &spec);
    outHeader.resize(static_cast<size_t>(written));
#else
    std::memset(outHeader.data(), 0, 64);
    std::memcpy(outHeader.data(), "RXDH", 4);
    *reinterpret_cast<uint32_t*>(outHeader.data() + 4) = 1;
    *reinterpret_cast<uint32_t*>(outHeader.data() + 8) = static_cast<uint32_t>(target);
    *reinterpret_cast<uint32_t*>(outHeader.data() + 12) = spec.arrayDimM;
    *reinterpret_cast<uint32_t*>(outHeader.data() + 16) = spec.arrayDimN;
    *reinterpret_cast<uint32_t*>(outHeader.data() + 20) = spec.peDataWidth;
#endif
    return PatchResult::ok("JTAG header generated");
}

// ---------------------------------------------------------------------------
// Results & Stats
// ---------------------------------------------------------------------------
const std::vector<AcceleratorDesign>& HardwareSynthesizer::getDesigns() const {
    return m_designs;
}

HardwareSynthStats HardwareSynthesizer::getStats() const {
    HardwareSynthStats stats{};
#ifdef RAWR_HAS_MASM
    void* p = asm_hwsynth_get_stats();
    if (p) std::memcpy(&stats, p, sizeof(HardwareSynthStats));
#endif
    return stats;
}

void HardwareSynthesizer::registerCallback(HWSynthCallback cb, void* userData) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_callbacks.push_back({cb, userData});
}

void HardwareSynthesizer::notifyCallback(const char* event, const AcceleratorDesign* design) {
    for (auto& cb : m_callbacks) {
        if (cb.fn) cb.fn(event, design, cb.userData);
    }
}

size_t HardwareSynthesizer::dumpDiagnostics(char* buffer, size_t bufferSize) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!buffer || bufferSize == 0) return 0;
    auto stats = getStats();
    int n = snprintf(buffer, bufferSize,
        "=== Hardware Synthesizer Diagnostics ===\n"
        "Initialized: %s\n"
        "Designs: %zu\n"
        "Profiles Run: %llu\n"
        "GEMM Specs: %llu\n"
        "Verilog Modules: %llu\n"
        "ISA Opcodes: %llu\n"
        "JTAG Headers: %llu\n",
        m_initialized ? "yes" : "no",
        m_designs.size(),
        (unsigned long long)stats.profilesRun,
        (unsigned long long)stats.gemmSpecsGenerated,
        (unsigned long long)stats.verilogModulesGen,
        (unsigned long long)stats.isaOpcodesGenerated,
        (unsigned long long)stats.jtagHeadersBuilt);
    return (n > 0) ? static_cast<size_t>(n) : 0;
}
