#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <intrin.h>  // RDTSCP

/**
 * TitanTokenTrace: Zero-overhead cycle-accurate token pipeline instrumentation
 * 
 * Captures 6 critical timing boundaries to expose causality of TPS bottlenecks:
 * - T0: token_start        (entry to GenerateFromTokens iteration)
 * - T1: scheduler_dispatch (compute dispatch initiated)
 * - T2: weights_ready      (forward pass begins, weights loaded)
 * - T3: compute_begin      (Transformer forward start)
 * - T4: compute_end        (logits returned, softmax scheduled)
 * - T5: kv_update_done     (KV cache updated, sampler ready)
 * - T6: token_emit         (token selected and pushed to output)
 * 
 * Usage:
 *   TitanTokenTrace trace = {};
 *   trace.record_t0();
 *   // ... token generation ...
 *   trace.record_t1();
 *   // ... forward pass ...
 *   trace.record_t6();
 */

struct TitanTokenTrace
{
    // Cycle timestamps (RDTSCP with serialization)
    uint64_t t0_start;
    uint64_t t1_sched;
    uint64_t t2_weights;
    uint64_t t3_compute_begin;
    uint64_t t4_compute_end;
    uint64_t t5_kv_done;
    uint64_t t6_emit;

    // Token identity
    uint32_t token_id;
    uint16_t batch_id;
    uint16_t flags;  // Context: [0]=prefetch_hit, [1]=cache_miss, [2]=gpu_stall

    // Derived stall attribution (computed post-capture)
    struct Stalls
    {
        uint32_t sched_delay_us;
        uint32_t weight_latency_us;
        uint32_t compute_time_us;
        uint32_t kv_latency_us;
        uint32_t emit_latency_us;
        uint32_t total_time_us;

        uint8_t dominant_cause;  // 0=MIXED, 1=NVME_BOUND, 2=SCHEDULER_STARVATION, 3=KV_CACHE_THRASH, 4=GPU_UNDERUTILIZED
    } stalls;

    // Forward pass metadata
    uint32_t layer_count;
    uint32_t expert_count;
    uint32_t vocab_size;
    uint32_t logits_size;

    // Inline timestamps with RDTSCP serialization barrier
    inline void record_t0()
    {
        unsigned int ui;
        t0_start = __rdtscp(&ui);
    }

    inline void record_t1()
    {
        unsigned int ui;
        t1_sched = __rdtscp(&ui);
    }

    inline void record_t2()
    {
        unsigned int ui;
        t2_weights = __rdtscp(&ui);
    }

    inline void record_t3()
    {
        unsigned int ui;
        t3_compute_begin = __rdtscp(&ui);
    }

    inline void record_t4()
    {
        unsigned int ui;
        t4_compute_end = __rdtscp(&ui);
    }

    inline void record_t5()
    {
        unsigned int ui;
        t5_kv_done = __rdtscp(&ui);
    }

    inline void record_t6()
    {
        unsigned int ui;
        t6_emit = __rdtscp(&ui);
    }

    // Compute stall deltas (microseconds, assuming 3.5 GHz base frequency)
    // Adjust CPP_BASE_FREQ_GHZ in implementation if needed
    inline void compute_stalls(double cpu_ghz = 3.5)
    {
        const double ticks_per_us = cpu_ghz * 1000.0;

        stalls.sched_delay_us = static_cast<uint32_t>((t1_sched - t0_start) / ticks_per_us);
        stalls.weight_latency_us = static_cast<uint32_t>((t2_weights - t1_sched) / ticks_per_us);
        stalls.compute_time_us = static_cast<uint32_t>((t4_compute_end - t3_compute_begin) / ticks_per_us);
        stalls.kv_latency_us = static_cast<uint32_t>((t5_kv_done - t4_compute_end) / ticks_per_us);
        stalls.emit_latency_us = static_cast<uint32_t>((t6_emit - t5_kv_done) / ticks_per_us);
        stalls.total_time_us = static_cast<uint32_t>((t6_emit - t0_start) / ticks_per_us);

        // Classify dominant stall root cause
        classify_stall();
    }

    inline void classify_stall()
    {
        stalls.dominant_cause = 0;  // MIXED default

        // Weight loading dominates if > 50% of compute time
        if (stalls.weight_latency_us > stalls.compute_time_us * 2)
        {
            stalls.dominant_cause = 1;  // NVME_BOUND
            return;
        }

        // Scheduler starvation if delay is significant
        if (stalls.sched_delay_us > stalls.compute_time_us)
        {
            stalls.dominant_cause = 2;  // SCHEDULER_STARVATION
            return;
        }

        // KV cache latency if sustained
        if (stalls.kv_latency_us > stalls.compute_time_us)
        {
            stalls.dominant_cause = 3;  // KV_CACHE_THRASH
            return;
        }

        // GPU underutilization if compute time exceeds expected by 1.5x
        // (assumes normal compute time ~420ms for forward pass)
        if (stalls.compute_time_us > 420000 * 1.5)
        {
            stalls.dominant_cause = 4;  // GPU_UNDERUTILIZED
            return;
        }
    }

    // Human-readable cause strings
    static inline const char* cause_name(uint8_t cause)
    {
        switch (cause)
        {
            case 0: return "MIXED";
            case 1: return "NVME_BOUND";
            case 2: return "SCHEDULER_STARVATION";
            case 3: return "KV_CACHE_THRASH";
            case 4: return "GPU_UNDERUTILIZED";
            default: return "UNKNOWN";
        }
    }

    // Log single trace entry (CSV format)
    inline void log_csv(FILE* fp) const
    {
        fprintf(fp, "%u,%u,%u,%u,%u,%u,%u,%u,%s\n",
            token_id,
            stalls.sched_delay_us,
            stalls.weight_latency_us,
            stalls.compute_time_us,
            stalls.kv_latency_us,
            stalls.emit_latency_us,
            stalls.total_time_us,
            vocab_size,
            cause_name(stalls.dominant_cause));
    }
};

/**
 * TitanTokenTraceBuffer: Circular ring buffer for zero-allocation logging
 * 
 * Stores up to N recent traces for aggregation and analysis.
 */
template <size_t RING_SIZE = 256>
class TitanTokenTraceBuffer
{
private:
    TitanTokenTrace buffer_[RING_SIZE];
    size_t write_index_ = 0;
    size_t entry_count_ = 0;

public:
    // Append trace entry (overwrites oldest if buffer full)
    inline void push(const TitanTokenTrace& trace)
    {
        buffer_[write_index_] = trace;
        write_index_ = (write_index_ + 1) % RING_SIZE;
        entry_count_ = std::min(entry_count_ + 1, RING_SIZE);
    }

    // Get entry at index (0 = oldest)
    inline const TitanTokenTrace* get(size_t idx) const
    {
        if (idx >= entry_count_)
            return nullptr;
        size_t buf_idx = (write_index_ - entry_count_ + idx) % RING_SIZE;
        return &buffer_[buf_idx];
    }

    // Compute aggregate statistics over last N entries
    inline void compute_aggregates(size_t count, TitanTokenTrace::Stalls& out_avg, uint8_t& out_dominant_cause) const
    {
        count = std::min(count, entry_count_);
        if (count == 0)
            return;

        memset(&out_avg, 0, sizeof(out_avg));

        uint32_t cause_histogram[5] = {};

        for (size_t i = 0; i < count; i++)
        {
            const TitanTokenTrace* trace = get(entry_count_ - count + i);
            if (trace)
            {
                out_avg.sched_delay_us += trace->stalls.sched_delay_us;
                out_avg.weight_latency_us += trace->stalls.weight_latency_us;
                out_avg.compute_time_us += trace->stalls.compute_time_us;
                out_avg.kv_latency_us += trace->stalls.kv_latency_us;
                out_avg.emit_latency_us += trace->stalls.emit_latency_us;
                out_avg.total_time_us += trace->stalls.total_time_us;

                if (trace->stalls.dominant_cause < 5)
                    cause_histogram[trace->stalls.dominant_cause]++;
            }
        }

        // Divide by count for averages
        out_avg.sched_delay_us /= count;
        out_avg.weight_latency_us /= count;
        out_avg.compute_time_us /= count;
        out_avg.kv_latency_us /= count;
        out_avg.emit_latency_us /= count;
        out_avg.total_time_us /= count;

        // Find most common cause
        uint8_t max_count = 0;
        for (uint8_t i = 0; i < 5; i++)
        {
            if (cause_histogram[i] > max_count)
            {
                max_count = cause_histogram[i];
                out_dominant_cause = i;
            }
        }
    }

    // Dump all traces to CSV file
    inline void dump_csv(const char* filepath) const
    {
        FILE* fp = fopen(filepath, "w");
        if (!fp)
            return;

        fprintf(fp, "token_id,sched_delay_us,weight_latency_us,compute_time_us,kv_latency_us,emit_latency_us,total_time_us,vocab_size,cause\n");

        for (size_t i = 0; i < entry_count_; i++)
        {
            const TitanTokenTrace* trace = get(i);
            if (trace)
                trace->log_csv(fp);
        }

        fclose(fp);
    }

    // Clear all entries
    inline void clear()
    {
        write_index_ = 0;
        entry_count_ = 0;
        memset(buffer_, 0, sizeof(buffer_));
    }

    inline size_t size() const { return entry_count_; }
};
