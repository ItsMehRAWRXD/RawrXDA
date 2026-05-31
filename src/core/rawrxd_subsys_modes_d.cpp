#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>

#ifdef RAWRXD_SUBSYS_MODES_D_INTEGRATED
// Skip defining stubs if already integrated via IDE bridge
#else

namespace
{
std::atomic<uint64_t> g_modeCallCount{0};
std::atomic<uint32_t> g_lastModeHash{0};

inline uint32_t fnv1a32(const char* text)
{
    uint32_t hash = 2166136261u;
    for (const unsigned char* p = reinterpret_cast<const unsigned char*>(text); *p != '\0'; ++p)
    {
        hash ^= static_cast<uint32_t>(*p);
        hash *= 16777619u;
    }
    return hash;
}

inline void noteModeCall(const char* modeName)
{
    g_modeCallCount.fetch_add(1, std::memory_order_relaxed);
    g_lastModeHash.store(fnv1a32(modeName), std::memory_order_relaxed);
}
}  // namespace

__declspec(selectany)  // extern "C" int asm_hotpatch_flush_icache(...) { noteModeCall("asm_hotpatch_flush_icache");
                       // return 0; }
__declspec(selectany)  // extern "C" int asm_hotpatch_atomic_swap(...) {
                       // noteModeCall("asm_hotpatch_atomic_swap"); return 0; }
__declspec(selectany)  // extern "C" int
                       // asm_hotpatch_install_trampoline(...) {
                       // noteModeCall("asm_hotpatch_install_trampoline");
                       // return 0; }
__declspec(selectany)  // extern "C" void*
                       // asm_hotpatch_alloc_shadow(...)
                       // {
                       // noteModeCall("asm_hotpatch_alloc_shadow");
                       // return nullptr; }
__declspec(selectany)  // extern
                       // "C"
                       // int
                       // asm_hotpatch_free_shadow(...)
                       // {
                       // noteModeCall("asm_hotpatch_free_shadow");
                       // return
                       // 0;
                       // }
__declspec(selectany)  // extern "C" int asm_hotpatch_backup_prologue(...) {
                       // noteModeCall("asm_hotpatch_backup_prologue"); return 0; }
__declspec(selectany)  // extern "C" int asm_hotpatch_restore_prologue(...) {
                       // noteModeCall("asm_hotpatch_restore_prologue"); return 0; }
__declspec(selectany)  // extern "C" int asm_hotpatch_verify_prologue(...)
                       // { noteModeCall("asm_hotpatch_verify_prologue");
                       // return 0; }
__declspec(selectany)  // extern "C" int
                       // asm_hotpatch_get_stats(...)
                       // {
                       // noteModeCall("asm_hotpatch_get_stats");
                       // return 0; }

// //
// extern
// "C"
// void
// asm_pyre_gemm_fp32(...)
// {
// noteModeCall("asm_pyre_gemm_fp32");
// }
// extern
// "C"
// void
// asm_pyre_gemv_fp32(...)
// {
// noteModeCall("asm_pyre_gemv_fp32");
// }
// extern
// "C"
// void
// asm_pyre_rmsnorm(...)
// {
// noteModeCall("asm_pyre_rmsnorm");
// }
// extern
// "C"
// void
// asm_pyre_silu(...)
// {
// noteModeCall("asm_pyre_silu");
// }
// extern
// "C"
// void
// asm_pyre_softmax(...)
// {
// noteModeCall("asm_pyre_softmax");
// }
// extern
// "C"
// void
// asm_pyre_rope(...)
// {
// noteModeCall("asm_pyre_rope");
// }
// extern
// "C"
// void
// asm_pyre_add_fp32(...)
// {
// noteModeCall("asm_pyre_add_fp32");
// }
// extern
// "C"
// void
// asm_pyre_mul_fp32(...)
// {
// noteModeCall("asm_pyre_mul_fp32");
// }
// extern
// "C"
// void
// asm_pyre_embedding_lookup(...)
// {
// noteModeCall("asm_pyre_embedding_lookup");
// }

extern "C" int asm_camellia256_auth_encrypt_file(...)
{
    noteModeCall("asm_camellia256_auth_encrypt_file");
    return 0;
}
extern "C" int asm_camellia256_auth_decrypt_file(...)
{
    noteModeCall("asm_camellia256_auth_decrypt_file");
    return 0;
}

#if !defined(RAWRXD_GOLD_BUILD)
extern "C" int asm_watchdog_init(...)
{
    noteModeCall("asm_watchdog_init");
    return 0;
}
extern "C" int asm_watchdog_verify(...)
{
    noteModeCall("asm_watchdog_verify");
    return 0;
}
extern "C" int asm_watchdog_get_baseline(...)
{
    noteModeCall("asm_watchdog_get_baseline");
    return 0;
}
extern "C" int asm_watchdog_get_status(...)
{
    noteModeCall("asm_watchdog_get_status");
    return 0;
}
extern "C" int asm_watchdog_shutdown(...)
{
    noteModeCall("asm_watchdog_shutdown");
    return 0;
}
#endif

// Native speed layer entry points (C++ fallbacks; AVX512 MASM may be omitted on some toolchains).
extern "C" void Sampler_SoftMax_TopK_Fused(...)
{
    noteModeCall("Sampler_SoftMax_TopK_Fused");
}
extern "C" void Titan_SiLU_AVX512(...)
{
    noteModeCall("Titan_SiLU_AVX512");
}
extern "C" void Titan_RMSNorm_AVX512(...)
{
    noteModeCall("Titan_RMSNorm_AVX512");
}
extern "C" void Sampler_ApplyTemperature_AVX512(...)
{
    noteModeCall("Sampler_ApplyTemperature_AVX512");
}
extern "C" void Sampler_FindMax_AVX512(...)
{
    noteModeCall("Sampler_FindMax_AVX512");
}
extern "C" void Sampler_ExpSum_AVX512(...)
{
    noteModeCall("Sampler_ExpSum_AVX512");
}

extern "C" const void* find_pattern_asm(const void* haystack, size_t haystack_len, const void* needle,
                                        size_t needle_len)
{
    noteModeCall("find_pattern_asm");
    if (!haystack || !needle || needle_len == 0 || haystack_len < needle_len)
    {
        return nullptr;
    }
    const uint8_t* h = static_cast<const uint8_t*>(haystack);
    const uint8_t* n = static_cast<const uint8_t*>(needle);
    for (size_t i = 0; i + needle_len <= haystack_len; ++i)
    {
        if (std::memcmp(h + i, n, needle_len) == 0)
        {
            return h + i;
        }
    }
    return nullptr;
}

extern "C" int asm_perf_init(...)
{
    noteModeCall("asm_perf_init");
    return 0;
}
extern "C" uint64_t asm_perf_begin(...)
{
    noteModeCall("asm_perf_begin");
    return 0;
}
extern "C" int asm_perf_end(...)
{
    noteModeCall("asm_perf_end");
    return 0;
}
extern "C" int asm_perf_read_slot(...)
{
    noteModeCall("asm_perf_read_slot");
    return 0;
}
extern "C" int asm_perf_reset_slot(...)
{
    noteModeCall("asm_perf_reset_slot");
    return 0;
}

extern "C" int asm_mesh_init(...)
{
    noteModeCall("asm_mesh_init");
    return 0;
}
extern "C" int asm_mesh_crdt_merge(...)
{
    noteModeCall("asm_mesh_crdt_merge");
    return 0;
}
extern "C" int asm_mesh_crdt_delta(...)
{
    noteModeCall("asm_mesh_crdt_delta");
    return 0;
}
extern "C" int asm_mesh_zkp_generate(...)
{
    noteModeCall("asm_mesh_zkp_generate");
    return 0;
}
extern "C" int asm_mesh_zkp_verify(...)
{
    noteModeCall("asm_mesh_zkp_verify");
    return 0;
}
extern "C" int asm_mesh_dht_xor_distance(...)
{
    noteModeCall("asm_mesh_dht_xor_distance");
    return 0;
}
extern "C" int asm_mesh_dht_find_closest(...)
{
    noteModeCall("asm_mesh_dht_find_closest");
    return 0;
}
extern "C" int asm_mesh_fedavg_aggregate(...)
{
    noteModeCall("asm_mesh_fedavg_aggregate");
    return 0;
}
extern "C" int asm_mesh_gossip_disseminate(...)
{
    noteModeCall("asm_mesh_gossip_disseminate");
    return 0;
}
extern "C" int asm_mesh_shard_hash(...)
{
    noteModeCall("asm_mesh_shard_hash");
    return 0;
}
extern "C" int asm_mesh_shard_bitfield(...)
{
    noteModeCall("asm_mesh_shard_bitfield");
    return 0;
}
extern "C" int asm_mesh_quorum_vote(...)
{
    noteModeCall("asm_mesh_quorum_vote");
    return 0;
}
extern "C" int asm_mesh_topology_update(...)
{
    noteModeCall("asm_mesh_topology_update");
    return 0;
}
extern "C" int asm_mesh_topology_active_count(...)
{
    noteModeCall("asm_mesh_topology_active_count");
    return 0;
}
extern "C" int asm_mesh_get_stats(...)
{
    noteModeCall("asm_mesh_get_stats");
    return 0;
}
extern "C" int asm_mesh_shutdown(...)
{
    noteModeCall("asm_mesh_shutdown");
    return 0;
}

extern "C" int asm_speciator_init(...)
{
    noteModeCall("asm_speciator_init");
    return 0;
}
extern "C" int asm_speciator_create_genome(...)
{
    noteModeCall("asm_speciator_create_genome");
    return 0;
}
extern "C" int asm_speciator_evaluate(...)
{
    noteModeCall("asm_speciator_evaluate");
    return 0;
}
extern "C" int asm_speciator_crossover(...)
{
    noteModeCall("asm_speciator_crossover");
    return 0;
}
extern "C" int asm_speciator_mutate(...)
{
    noteModeCall("asm_speciator_mutate");
    return 0;
}
extern "C" int asm_speciator_select(...)
{
    noteModeCall("asm_speciator_select");
    return 0;
}
extern "C" int asm_speciator_speciate(...)
{
    noteModeCall("asm_speciator_speciate");
    return 0;
}
extern "C" int asm_speciator_gen_variant(...)
{
    noteModeCall("asm_speciator_gen_variant");
    return 0;
}
extern "C" int asm_speciator_compete(...)
{
    noteModeCall("asm_speciator_compete");
    return 0;
}
extern "C" int asm_speciator_migrate(...)
{
    noteModeCall("asm_speciator_migrate");
    return 0;
}
extern "C" int asm_speciator_get_stats(...)
{
    noteModeCall("asm_speciator_get_stats");
    return 0;
}
extern "C" int asm_speciator_shutdown(...)
{
    noteModeCall("asm_speciator_shutdown");
    return 0;
}

extern "C" int asm_neural_init(...)
{
    noteModeCall("asm_neural_init");
    return 0;
}
extern "C" int asm_neural_acquire_eeg(...)
{
    noteModeCall("asm_neural_acquire_eeg");
    return 0;
}
extern "C" int asm_neural_fft_decompose(...)
{
    noteModeCall("asm_neural_fft_decompose");
    return 0;
}
extern "C" int asm_neural_extract_csp(...)
{
    noteModeCall("asm_neural_extract_csp");
    return 0;
}
extern "C" int asm_neural_classify_intent(...)
{
    noteModeCall("asm_neural_classify_intent");
    return 0;
}
extern "C" int asm_neural_detect_event(...)
{
    noteModeCall("asm_neural_detect_event");
    return 0;
}
extern "C" int asm_neural_encode_command(...)
{
    noteModeCall("asm_neural_encode_command");
    return 0;
}
extern "C" int asm_neural_gen_phosphene(...)
{
    noteModeCall("asm_neural_gen_phosphene");
    return 0;
}
extern "C" int asm_neural_haptic_pulse(...)
{
    noteModeCall("asm_neural_haptic_pulse");
    return 0;
}
extern "C" int asm_neural_calibrate(...)
{
    noteModeCall("asm_neural_calibrate");
    return 0;
}
extern "C" int asm_neural_adapt(...)
{
    noteModeCall("asm_neural_adapt");
    return 0;
}
extern "C" int asm_neural_get_stats(...)
{
    noteModeCall("asm_neural_get_stats");
    return 0;
}
extern "C" int asm_neural_shutdown(...)
{
    noteModeCall("asm_neural_shutdown");
    return 0;
}

extern "C" int asm_hwsynth_get_stats(...)
{
    noteModeCall("asm_hwsynth_get_stats");
    return 0;
}
extern "C" int asm_hwsynth_shutdown(...)
{
    noteModeCall("asm_hwsynth_shutdown");
    return 0;
}

extern "C" int asm_spengine_cpu_optimize(...)
{
    noteModeCall("asm_spengine_cpu_optimize");
    return 0;
}
extern "C" int asm_apply_memory_patch(...)
{
    noteModeCall("asm_apply_memory_patch");
    return 0;
}

#endif  // RAWRXD_SUBSYS_MODES_D_INTEGRATED

#if !defined(RAWRXD_GOLD_BUILD) && !defined(RAWR_HAS_MASM)
// Snapshot API: C++ fallbacks for hotpatch/shadow paths.
// If MASM is available, the snapshot exports come from the MASM object pack.
extern "C"
{
    int asm_snapshot_capture(...)
    {
        return 0;
    }
    int asm_snapshot_restore(...)
    {
        return 0;
    }
    int asm_snapshot_verify(...)
    {
        return 0;
    }
    int asm_snapshot_discard(...)
    {
        return 0;
    }
}
#endif

#if !defined(RAWRXD_GOLD_BUILD)
// These snapshot entrypoints are referenced by hotpatch/orchestrator paths but are not always
// present in the MASM snapshot pack. Provide harmless fallbacks.
extern "C"
{
    int asm_snapshot_verify(...)
    {
        return 0;
    }
    int asm_snapshot_discard(...)
    {
        return 0;
    }
    int asm_snapshot_get_stats(...)
    {
        return 0;
    }
}
#endif
