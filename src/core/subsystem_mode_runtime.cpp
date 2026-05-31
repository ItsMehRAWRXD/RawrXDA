// Runtime provider for subsystem-mode exports expected by Win32IDE.
// These symbols stay available when optional ASM modules are not linked.
//
// NOTE:
// - AD_* and SO_* symbols are intentionally defined in the real providers:
//   analyzer_distiller.cpp and streaming_orchestrator.cpp.

#include <atomic>
#include <cstdint>
#include <cstdio>

namespace {
    // Subsystem telemetry counters — track how often each fallback is invoked.
    // This helps diagnose whether an ASM module failed to link.
    struct FallbackCounters {
        std::atomic<uint64_t> modeCalls{0};
        std::atomic<uint64_t> watchdogCalls{0};
        std::atomic<uint64_t> omegaCalls{0};
        std::atomic<uint64_t> meshCalls{0};
        std::atomic<uint64_t> speciatorCalls{0};
        std::atomic<uint64_t> neuralCalls{0};
        std::atomic<uint64_t> hwsynthCalls{0};
        std::atomic<uint64_t> quadbufCalls{0};
        std::atomic<uint64_t> spengineCalls{0};
    };
    static FallbackCounters g_fallback;

    // Per-subsystem init flags so init/shutdown pairs are idempotent.
    struct InitFlags {
        std::atomic<bool> watchdog{false};
        std::atomic<bool> omega{false};
        std::atomic<bool> mesh{false};
        std::atomic<bool> speciator{false};
        std::atomic<bool> neural{false};
        std::atomic<bool> hwsynth{false};
        std::atomic<bool> quadbuf{false};
        std::atomic<bool> spengine{false};
    };
    static InitFlags g_init;

    static void logOnce(const char* name) {
        static std::atomic<uint64_t> logged{0};
        if (logged.fetch_add(1, std::memory_order_relaxed) < 1) {
            std::fprintf(stderr, "[WARN] [SubsystemFallback] %s invoked — optional ASM module not linked.\n", name);
        }
    }
} // namespace

// ============================================================================
// Mode switches (no-op when ASM unavailable)
// ============================================================================
extern "C" void InjectMode(void)       { g_fallback.modeCalls.fetch_add(1, std::memory_order_relaxed); }
extern "C" void DiffCovMode(void)      { g_fallback.modeCalls.fetch_add(1, std::memory_order_relaxed); }
extern "C" void IntelPTMode(void)      { g_fallback.modeCalls.fetch_add(1, std::memory_order_relaxed); }
extern "C" void AgentTraceMode(void)   { g_fallback.modeCalls.fetch_add(1, std::memory_order_relaxed); }
extern "C" void DynTraceMode(void)     { g_fallback.modeCalls.fetch_add(1, std::memory_order_relaxed); }
extern "C" void CovFusionMode(void)   { g_fallback.modeCalls.fetch_add(1, std::memory_order_relaxed); }
extern "C" void SideloadMode(void)     { g_fallback.modeCalls.fetch_add(1, std::memory_order_relaxed); }
extern "C" void PersistenceMode(void)  { g_fallback.modeCalls.fetch_add(1, std::memory_order_relaxed); }
extern "C" void BasicBlockCovMode(void){ g_fallback.modeCalls.fetch_add(1, std::memory_order_relaxed); }
extern "C" void StubGenMode(void)      { g_fallback.modeCalls.fetch_add(1, std::memory_order_relaxed); }
extern "C" void TraceEngineMode(void)  { g_fallback.modeCalls.fetch_add(1, std::memory_order_relaxed); }
extern "C" void CompileMode(void)     { g_fallback.modeCalls.fetch_add(1, std::memory_order_relaxed); }
extern "C" void GapFuzzMode(void)     { g_fallback.modeCalls.fetch_add(1, std::memory_order_relaxed); }
extern "C" void EncryptMode(void)     { g_fallback.modeCalls.fetch_add(1, std::memory_order_relaxed); }
extern "C" void EntropyMode(void)     { g_fallback.modeCalls.fetch_add(1, std::memory_order_relaxed); }
extern "C" void AgenticMode(void)    { g_fallback.modeCalls.fetch_add(1, std::memory_order_relaxed); }
extern "C" void UACBypassMode(void)   { g_fallback.modeCalls.fetch_add(1, std::memory_order_relaxed); }
extern "C" void AVScanMode(void)     { g_fallback.modeCalls.fetch_add(1, std::memory_order_relaxed); }

// ============================================================================
// Watchdog subsystem fallback
// ============================================================================
extern "C" void asm_watchdog_init(void) {
    g_fallback.watchdogCalls.fetch_add(1, std::memory_order_relaxed);
    g_init.watchdog.store(true, std::memory_order_release);
    logOnce("asm_watchdog_init");
}
extern "C" void asm_watchdog_verify(void) {
    g_fallback.watchdogCalls.fetch_add(1, std::memory_order_relaxed);
}
extern "C" uint64_t asm_watchdog_get_status(void) {
    g_fallback.watchdogCalls.fetch_add(1, std::memory_order_relaxed);
    return g_init.watchdog.load(std::memory_order_acquire) ? 1ULL : 0ULL;
}
extern "C" uint64_t asm_watchdog_get_baseline(void) {
    g_fallback.watchdogCalls.fetch_add(1, std::memory_order_relaxed);
    return 0ULL;
}
extern "C" void asm_watchdog_shutdown(void) {
    g_fallback.watchdogCalls.fetch_add(1, std::memory_order_relaxed);
    g_init.watchdog.store(false, std::memory_order_release);
}

// ============================================================================
// Omega (agentic orchestrator) subsystem fallback
// ============================================================================
extern "C" void asm_omega_init(void) {
    g_fallback.omegaCalls.fetch_add(1, std::memory_order_relaxed);
    g_init.omega.store(true, std::memory_order_release);
    logOnce("asm_omega_init");
}
extern "C" void asm_omega_implement_generate(void) {
    g_fallback.omegaCalls.fetch_add(1, std::memory_order_relaxed);
}
extern "C" void asm_omega_agent_step(void) {
    g_fallback.omegaCalls.fetch_add(1, std::memory_order_relaxed);
}
extern "C" void asm_omega_shutdown(void) {
    g_fallback.omegaCalls.fetch_add(1, std::memory_order_relaxed);
    g_init.omega.store(false, std::memory_order_release);
}
extern "C" void asm_omega_plan_decompose(void) {
    g_fallback.omegaCalls.fetch_add(1, std::memory_order_relaxed);
}
extern "C" void asm_omega_evolve_improve(void) {
    g_fallback.omegaCalls.fetch_add(1, std::memory_order_relaxed);
}
extern "C" uint64_t asm_omega_get_stats(void) {
    g_fallback.omegaCalls.fetch_add(1, std::memory_order_relaxed);
    return g_fallback.omegaCalls.load(std::memory_order_relaxed);
}
extern "C" int asm_omega_verify_test(void) {
    g_fallback.omegaCalls.fetch_add(1, std::memory_order_relaxed);
    return 0; // 0 = no tests available in fallback
}
extern "C" void asm_omega_architect_select(void) {
    g_fallback.omegaCalls.fetch_add(1, std::memory_order_relaxed);
}
extern "C" void asm_omega_agent_spawn(void) {
    g_fallback.omegaCalls.fetch_add(1, std::memory_order_relaxed);
}
extern "C" void asm_omega_observe_monitor(void) {
    g_fallback.omegaCalls.fetch_add(1, std::memory_order_relaxed);
}
extern "C" void asm_omega_deploy_distribute(void) {
    g_fallback.omegaCalls.fetch_add(1, std::memory_order_relaxed);
}
extern "C" void asm_omega_execute_pipeline(void) {
    g_fallback.omegaCalls.fetch_add(1, std::memory_order_relaxed);
}
extern "C" void asm_omega_ingest_requirement(void) {
    g_fallback.omegaCalls.fetch_add(1, std::memory_order_relaxed);
}
extern "C" void asm_omega_world_model_update(void) {
    g_fallback.omegaCalls.fetch_add(1, std::memory_order_relaxed);
}

// ============================================================================
// Mesh (federated / distributed) subsystem fallback
// ============================================================================
extern "C" void asm_mesh_init(void) {
    g_fallback.meshCalls.fetch_add(1, std::memory_order_relaxed);
    g_init.mesh.store(true, std::memory_order_release);
    logOnce("asm_mesh_init");
}
extern "C" void asm_mesh_shutdown(void) {
    g_fallback.meshCalls.fetch_add(1, std::memory_order_relaxed);
    g_init.mesh.store(false, std::memory_order_release);
}
extern "C" void asm_mesh_crdt_delta(void) {
    g_fallback.meshCalls.fetch_add(1, std::memory_order_relaxed);
}
extern "C" uint64_t asm_mesh_get_stats(void) {
    g_fallback.meshCalls.fetch_add(1, std::memory_order_relaxed);
    return g_fallback.meshCalls.load(std::memory_order_relaxed);
}
extern "C" void asm_mesh_dht_find_closest(void) {
    g_fallback.meshCalls.fetch_add(1, std::memory_order_relaxed);
}
extern "C" void asm_mesh_fedavg_aggregate(void) {
    g_fallback.meshCalls.fetch_add(1, std::memory_order_relaxed);
}
extern "C" void asm_mesh_crdt_merge(void) {
    g_fallback.meshCalls.fetch_add(1, std::memory_order_relaxed);
}
extern "C" void asm_mesh_dht_xor_distance(void) {
    g_fallback.meshCalls.fetch_add(1, std::memory_order_relaxed);
}
extern "C" int asm_mesh_zkp_verify(void) {
    g_fallback.meshCalls.fetch_add(1, std::memory_order_relaxed);
    return 0; // unverified in fallback
}
extern "C" void asm_mesh_shard_hash(void) {
    g_fallback.meshCalls.fetch_add(1, std::memory_order_relaxed);
}
extern "C" void asm_mesh_quorum_vote(void) {
    g_fallback.meshCalls.fetch_add(1, std::memory_order_relaxed);
}
extern "C" void asm_mesh_topology_update(void) {
    g_fallback.meshCalls.fetch_add(1, std::memory_order_relaxed);
}
extern "C" void asm_mesh_zkp_generate(void) {
    g_fallback.meshCalls.fetch_add(1, std::memory_order_relaxed);
}
extern "C" uint32_t asm_mesh_topology_active_count(void) {
    g_fallback.meshCalls.fetch_add(1, std::memory_order_relaxed);
    return 0;
}
extern "C" uint64_t asm_mesh_shard_bitfield(void) {
    g_fallback.meshCalls.fetch_add(1, std::memory_order_relaxed);
    return 0ULL;
}
extern "C" void asm_mesh_gossip_disseminate(void) {
    g_fallback.meshCalls.fetch_add(1, std::memory_order_relaxed);
}

// ============================================================================
// Speciator (evolutionary) subsystem fallback
// ============================================================================
extern "C" void asm_speciator_init(void) {
    g_fallback.speciatorCalls.fetch_add(1, std::memory_order_relaxed);
    g_init.speciator.store(true, std::memory_order_release);
    logOnce("asm_speciator_init");
}
extern "C" void asm_speciator_shutdown(void) {
    g_fallback.speciatorCalls.fetch_add(1, std::memory_order_relaxed);
    g_init.speciator.store(false, std::memory_order_release);
}
extern "C" void asm_speciator_mutate(void) {
    g_fallback.speciatorCalls.fetch_add(1, std::memory_order_relaxed);
}
extern "C" void asm_speciator_gen_variant(void) {
    g_fallback.speciatorCalls.fetch_add(1, std::memory_order_relaxed);
}
extern "C" uint64_t asm_speciator_get_stats(void) {
    g_fallback.speciatorCalls.fetch_add(1, std::memory_order_relaxed);
    return g_fallback.speciatorCalls.load(std::memory_order_relaxed);
}
extern "C" void asm_speciator_create_genome(void) {
    g_fallback.speciatorCalls.fetch_add(1, std::memory_order_relaxed);
}
extern "C" void asm_speciator_crossover(void) {
    g_fallback.speciatorCalls.fetch_add(1, std::memory_order_relaxed);
}
extern "C" void asm_speciator_speciate(void) {
    g_fallback.speciatorCalls.fetch_add(1, std::memory_order_relaxed);
}
extern "C" void asm_speciator_evaluate(void) {
    g_fallback.speciatorCalls.fetch_add(1, std::memory_order_relaxed);
}
extern "C" void asm_speciator_compete(void) {
    g_fallback.speciatorCalls.fetch_add(1, std::memory_order_relaxed);
}
extern "C" void asm_speciator_migrate(void) {
    g_fallback.speciatorCalls.fetch_add(1, std::memory_order_relaxed);
}
extern "C" void asm_speciator_select(void) {
    g_fallback.speciatorCalls.fetch_add(1, std::memory_order_relaxed);
}

// ============================================================================
// Neural (BCI / signal) subsystem fallback
// ============================================================================
extern "C" void asm_neural_init(void) {
    g_fallback.neuralCalls.fetch_add(1, std::memory_order_relaxed);
    g_init.neural.store(true, std::memory_order_release);
    logOnce("asm_neural_init");
}
extern "C" void asm_neural_shutdown(void) {
    g_fallback.neuralCalls.fetch_add(1, std::memory_order_relaxed);
    g_init.neural.store(false, std::memory_order_release);
}
extern "C" void asm_neural_classify_intent(void) {
    g_fallback.neuralCalls.fetch_add(1, std::memory_order_relaxed);
}
extern "C" void asm_neural_haptic_pulse(void) {
    g_fallback.neuralCalls.fetch_add(1, std::memory_order_relaxed);
}
extern "C" void asm_neural_encode_command(void) {
    g_fallback.neuralCalls.fetch_add(1, std::memory_order_relaxed);
}
extern "C" void asm_neural_acquire_eeg(void) {
    g_fallback.neuralCalls.fetch_add(1, std::memory_order_relaxed);
}
extern "C" void asm_neural_adapt(void) {
    g_fallback.neuralCalls.fetch_add(1, std::memory_order_relaxed);
}
extern "C" void asm_neural_fft_decompose(void) {
    g_fallback.neuralCalls.fetch_add(1, std::memory_order_relaxed);
}
extern "C" void asm_neural_calibrate(void) {
    g_fallback.neuralCalls.fetch_add(1, std::memory_order_relaxed);
}
extern "C" int asm_neural_detect_event(void) {
    g_fallback.neuralCalls.fetch_add(1, std::memory_order_relaxed);
    return 0; // no event detected in fallback
}
extern "C" void asm_neural_gen_phosphene(void) {
    g_fallback.neuralCalls.fetch_add(1, std::memory_order_relaxed);
}
extern "C" void asm_neural_extract_csp(void) {
    g_fallback.neuralCalls.fetch_add(1, std::memory_order_relaxed);
}
extern "C" uint64_t asm_neural_get_stats(void) {
    g_fallback.neuralCalls.fetch_add(1, std::memory_order_relaxed);
    return g_fallback.neuralCalls.load(std::memory_order_relaxed);
}

// ============================================================================
// Hardware synthesis subsystem fallback
// ============================================================================
extern "C" void asm_hwsynth_init(void) {
    g_fallback.hwsynthCalls.fetch_add(1, std::memory_order_relaxed);
    g_init.hwsynth.store(true, std::memory_order_release);
    logOnce("asm_hwsynth_init");
}
extern "C" void asm_hwsynth_shutdown(void) {
    g_fallback.hwsynthCalls.fetch_add(1, std::memory_order_relaxed);
    g_init.hwsynth.store(false, std::memory_order_release);
}
extern "C" void asm_hwsynth_est_resources(void) {
    g_fallback.hwsynthCalls.fetch_add(1, std::memory_order_relaxed);
}
extern "C" void asm_hwsynth_predict_perf(void) {
    g_fallback.hwsynthCalls.fetch_add(1, std::memory_order_relaxed);
}
extern "C" uint64_t asm_hwsynth_get_stats(void) {
    g_fallback.hwsynthCalls.fetch_add(1, std::memory_order_relaxed);
    return g_fallback.hwsynthCalls.load(std::memory_order_relaxed);
}
extern "C" void asm_hwsynth_gen_gemm_spec(void) {
    g_fallback.hwsynthCalls.fetch_add(1, std::memory_order_relaxed);
}
extern "C" void asm_hwsynth_gen_jtag_header(void) {
    g_fallback.hwsynthCalls.fetch_add(1, std::memory_order_relaxed);
}
extern "C" void asm_hwsynth_analyze_memhier(void) {
    g_fallback.hwsynthCalls.fetch_add(1, std::memory_order_relaxed);
}
extern "C" void asm_hwsynth_profile_dataflow(void) {
    g_fallback.hwsynthCalls.fetch_add(1, std::memory_order_relaxed);
}

// ============================================================================
// Quad-buffer (render / token display) subsystem fallback
// ============================================================================
extern "C" void asm_quadbuf_init(void) {
    g_fallback.quadbufCalls.fetch_add(1, std::memory_order_relaxed);
    g_init.quadbuf.store(true, std::memory_order_release);
    logOnce("asm_quadbuf_init");
}
extern "C" void asm_quadbuf_shutdown(void) {
    g_fallback.quadbufCalls.fetch_add(1, std::memory_order_relaxed);
    g_init.quadbuf.store(false, std::memory_order_release);
}
extern "C" void asm_quadbuf_push_token(void) {
    g_fallback.quadbufCalls.fetch_add(1, std::memory_order_relaxed);
}
extern "C" void asm_quadbuf_render_frame(void) {
    g_fallback.quadbufCalls.fetch_add(1, std::memory_order_relaxed);
}
extern "C" void asm_quadbuf_set_flags(void) {
    g_fallback.quadbufCalls.fetch_add(1, std::memory_order_relaxed);
}
extern "C" void asm_quadbuf_resize(void) {
    g_fallback.quadbufCalls.fetch_add(1, std::memory_order_relaxed);
}
extern "C" uint64_t asm_quadbuf_get_stats(void) {
    g_fallback.quadbufCalls.fetch_add(1, std::memory_order_relaxed);
    return g_fallback.quadbufCalls.load(std::memory_order_relaxed);
}

// ============================================================================
// Sparse engine (quantization / inference) subsystem fallback
// ============================================================================
extern "C" void asm_spengine_init(void) {
    g_fallback.spengineCalls.fetch_add(1, std::memory_order_relaxed);
    g_init.spengine.store(true, std::memory_order_release);
    logOnce("asm_spengine_init");
}
extern "C" void asm_spengine_shutdown(void) {
    g_fallback.spengineCalls.fetch_add(1, std::memory_order_relaxed);
    g_init.spengine.store(false, std::memory_order_release);
}
extern "C" void asm_spengine_quant_switch_adaptive(void) {
    g_fallback.spengineCalls.fetch_add(1, std::memory_order_relaxed);
}
extern "C" void asm_spengine_rollback(void) {
    g_fallback.spengineCalls.fetch_add(1, std::memory_order_relaxed);
}
extern "C" void asm_spengine_register(void) {
    g_fallback.spengineCalls.fetch_add(1, std::memory_order_relaxed);
}
extern "C" uint64_t asm_spengine_get_stats(void) {
    g_fallback.spengineCalls.fetch_add(1, std::memory_order_relaxed);
    return g_fallback.spengineCalls.load(std::memory_order_relaxed);
}
extern "C" void asm_spengine_apply(void) {
    g_fallback.spengineCalls.fetch_add(1, std::memory_order_relaxed);
}
extern "C" void asm_spengine_quant_switch(void) {
    g_fallback.spengineCalls.fetch_add(1, std::memory_order_relaxed);
}
extern "C" void asm_spengine_cpu_optimize(void) {
    g_fallback.spengineCalls.fetch_add(1, std::memory_order_relaxed);
}
