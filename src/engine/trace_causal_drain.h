#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace RawrXD::Trace {

struct CausalDrainStats {
    size_t drainedEvents = 0;
    size_t writtenEvents = 0;
    uint64_t filteredByEpoch = 0;
    bool openedOutput = false;
};

// Drains events from TraceBus and writes JSONL records suitable for DAG tools.
// If epoch_filter is non-zero, only events from that epoch are emitted.
bool DrainTraceBusToJsonl(const std::string& output_path,
                          size_t max_events,
                          uint64_t epoch_filter,
                          CausalDrainStats* stats,
                          std::string* error);

}  // namespace RawrXD::Trace
