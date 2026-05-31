#include "trace_causal_drain.h"

#include "stream_trace_bus.h"

#include <cstdlib>
#include <iostream>
#include <string>

namespace {

size_t ParseSize(const char* text, size_t fallback) {
    if (!text || text[0] == '\0') {
        return fallback;
    }
    const unsigned long long value = std::strtoull(text, nullptr, 10);
    return value == 0 ? fallback : static_cast<size_t>(value);
}

uint64_t ParseEpoch(const char* text) {
    if (!text || text[0] == '\0') {
        return 0;
    }
    return static_cast<uint64_t>(std::strtoull(text, nullptr, 10));
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: RawrXD-CausalDrain <output.jsonl> [max_events] [epoch_filter]\n";
        return 2;
    }

    const std::string output_path = argv[1];
    const size_t max_events = ParseSize(argc > 2 ? argv[2] : nullptr, 100000);
    const uint64_t epoch_filter = ParseEpoch(argc > 3 ? argv[3] : nullptr);

    RAWRXD_TRACE_INIT();

    RawrXD::Trace::CausalDrainStats stats;
    std::string error;
    const bool ok = RawrXD::Trace::DrainTraceBusToJsonl(
        output_path, max_events, epoch_filter, &stats, &error);

    if (!ok) {
        std::cerr << "Causal drain failed: " << error << "\n";
        return 1;
    }

    std::cout << "Causal drain complete"
              << " drained=" << stats.drainedEvents
              << " written=" << stats.writtenEvents
              << " filtered_epoch=" << stats.filteredByEpoch
              << " output=\"" << output_path << "\"\n";
    return 0;
}
