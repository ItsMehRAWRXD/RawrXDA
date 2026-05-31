#include "trace_causal_drain.h"

#include "stream_trace_bus.h"

#include <fstream>
#include <sstream>

namespace RawrXD::Trace {
namespace {

const char* TraceTypeToString(TraceType type) {
    switch (type) {
        case TraceType::Phase:
            return "phase";
        case TraceType::TensorOp:
            return "tensor_op";
        case TraceType::KernelDispatch:
            return "kernel_dispatch";
        case TraceType::MemoryAlloc:
            return "memory_alloc";
        case TraceType::MemoryFree:
            return "memory_free";
        case TraceType::AttentionStep:
            return "attention_step";
        case TraceType::Error:
            return "error";
        default:
            return "unknown";
    }
}

void AppendEscapedJsonString(std::ostringstream& out, const char* text) {
    for (const char* p = text; *p != '\0'; ++p) {
        const char c = *p;
        if (c == '"' || c == '\\') {
            out << '\\' << c;
        } else if (c == '\n') {
            out << "\\n";
        } else if (c == '\r') {
            out << "\\r";
        } else if (c == '\t') {
            out << "\\t";
        } else {
            out << c;
        }
    }
}

std::string EventToJsonl(const TraceEvent& e) {
    std::ostringstream out;
    out << '{';
    out << "\"epoch_id\":" << e.epoch_id << ',';
    out << "\"causal_parent\":" << e.causal_parent << ',';
    out << "\"logical_tick\":" << e.logical_tick << ',';
    out << "\"thread_id\":" << e.thread_id << ',';
    out << "\"type\":\"";
    AppendEscapedJsonString(out, TraceTypeToString(e.type));
    out << "\",";
    out << "\"type_id\":" << static_cast<uint32_t>(e.type) << ',';
    out << "\"node_id\":" << e.node_id << ',';
    out << "\"op_id\":" << e.op_id << ',';
    out << "\"payload_a\":" << e.payload_a << ',';
    out << "\"payload_b\":" << e.payload_b << ',';
    out << "\"hash\":" << e.hash;
    out << '}';
    return out.str();
}

}  // namespace

bool DrainTraceBusToJsonl(const std::string& output_path,
                          size_t max_events,
                          uint64_t epoch_filter,
                          CausalDrainStats* stats,
                          std::string* error) {
    CausalDrainStats local_stats;

    std::ofstream out(output_path, std::ios::out | std::ios::app);
    if (!out.is_open()) {
        if (error) {
            *error = "failed to open output file: " + output_path;
        }
        if (stats) {
            *stats = local_stats;
        }
        return false;
    }
    local_stats.openedOutput = true;

    TraceEvent event;
    while (local_stats.drainedEvents < max_events && TraceBus::Pop(event)) {
        ++local_stats.drainedEvents;
        if (epoch_filter != 0 && event.epoch_id != epoch_filter) {
            ++local_stats.filteredByEpoch;
            continue;
        }
        out << EventToJsonl(event) << '\n';
        ++local_stats.writtenEvents;
    }

    out.flush();
    if (!out.good()) {
        if (error) {
            *error = "failed to write trace output to: " + output_path;
        }
        if (stats) {
            *stats = local_stats;
        }
        return false;
    }

    if (stats) {
        *stats = local_stats;
    }
    if (error) {
        error->clear();
    }
    return true;
}

}  // namespace RawrXD::Trace
