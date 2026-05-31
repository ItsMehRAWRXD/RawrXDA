// Minimal InferenceProfiler definitions for RawrXD_Gold + BackendOrchestrator only.
#include "InferenceProfiler.h"

#include <sstream>

namespace RawrXD
{

namespace
{
std::string makeOpKey(const std::string& op, int layer)
{
    return op + "#" + std::to_string(layer);
}
}

InferenceProfiler& InferenceProfiler::Instance()
{
    static InferenceProfiler inst;
    return inst;
}

void InferenceProfiler::BeginOp(const std::string& op, int layer)
{
    std::lock_guard<std::mutex> lock(m_start_mtx);
    m_starts[makeOpKey(op, layer)] = std::chrono::steady_clock::now();
}

void InferenceProfiler::EndOp(const std::string& op, int layer, double mem_read, double mem_write)
{
    const auto now = std::chrono::steady_clock::now();
    const std::string key = makeOpKey(op, layer);

    std::chrono::steady_clock::time_point start = now;
    {
        std::lock_guard<std::mutex> lock(m_start_mtx);
        auto it = m_starts.find(key);
        if (it != m_starts.end())
        {
            start = it->second;
            m_starts.erase(it);
        }
    }

    const double durationUs = std::chrono::duration<double, std::micro>(now - start).count();

    LayerTimingSample sample{};
    sample.layer_idx = layer;
    sample.op_name = op;
    sample.duration_us = durationUs;
    sample.mem_bytes_read = mem_read;
    sample.mem_bytes_write = mem_write;
    sample.timestamp = now;

    {
        std::lock_guard<std::mutex> lock(m_sample_mtx);
        m_samples.push_back(sample);
    }

    {
        std::lock_guard<std::mutex> lock(m_hist_mtx);
        auto& histogram = m_histograms[op];
        if (!histogram)
        {
            histogram = std::make_shared<Histogram>();
        }
        histogram->Record(durationUs);
    }

    if (layer < 0 || op == "inference" || op == "total")
    {
        m_total_inference_ms.fetch_add(durationUs / 1000.0, std::memory_order_relaxed);
        m_inference_count.fetch_add(1, std::memory_order_relaxed);
    }
}

std::string InferenceProfiler::GetPrometheusText() const
{
    std::vector<LayerTimingSample> samples;
    {
        std::lock_guard<std::mutex> lock(m_sample_mtx);
        samples = m_samples;
    }

    std::unordered_map<std::string, double> totalUs;
    std::unordered_map<std::string, uint64_t> counts;
    std::unordered_map<std::string, double> totalRead;
    std::unordered_map<std::string, double> totalWrite;

    for (const auto& sample : samples)
    {
        totalUs[sample.op_name] += sample.duration_us;
        counts[sample.op_name] += 1;
        totalRead[sample.op_name] += sample.mem_bytes_read;
        totalWrite[sample.op_name] += sample.mem_bytes_write;
    }

    std::ostringstream out;
    out << "# HELP rawrxd_inference_profiler_samples_total Number of recorded profiler samples\n";
    out << "# TYPE rawrxd_inference_profiler_samples_total counter\n";
    out << "rawrxd_inference_profiler_samples_total " << samples.size() << "\n";
    out << "# HELP rawrxd_inference_total_ms Accumulated inference time in milliseconds\n";
    out << "# TYPE rawrxd_inference_total_ms counter\n";
    out << "rawrxd_inference_total_ms " << m_total_inference_ms.load(std::memory_order_relaxed) << "\n";
    out << "# HELP rawrxd_inference_count Completed inference operations\n";
    out << "# TYPE rawrxd_inference_count counter\n";
    out << "rawrxd_inference_count " << m_inference_count.load(std::memory_order_relaxed) << "\n";

    std::lock_guard<std::mutex> histLock(m_hist_mtx);
    for (const auto& [op, histogram] : m_histograms)
    {
        const double avgUs = counts[op] ? (totalUs[op] / static_cast<double>(counts[op])) : 0.0;
        out << "rawrxd_inference_op_avg_us{op=\"" << op << "\"} " << avgUs << "\n";
        out << "rawrxd_inference_op_p50_us{op=\"" << op << "\"} " << histogram->P50() << "\n";
        out << "rawrxd_inference_op_p95_us{op=\"" << op << "\"} " << histogram->P95() << "\n";
        out << "rawrxd_inference_op_p99_us{op=\"" << op << "\"} " << histogram->P99() << "\n";
        out << "rawrxd_inference_op_mem_read_bytes{op=\"" << op << "\"} " << totalRead[op] << "\n";
        out << "rawrxd_inference_op_mem_write_bytes{op=\"" << op << "\"} " << totalWrite[op] << "\n";
    }

    if (samples.empty())
    {
        out << "# RawrXD_Gold: inference profiler active (no samples yet)\n";
    }
    return out.str();
}

}  // namespace RawrXD
