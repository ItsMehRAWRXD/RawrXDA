// ============================================================================
// moe_telemetry.cpp - Flight Recorder Implementation
// ============================================================================
#include "moe_telemetry.h"
#include <sstream>
#include <iomanip>

namespace moe {

FlightRecorder g_flight_recorder;

std::string FlightRecorder::export_json() const {
    std::ostringstream json;
    json << "{\n";
    json << "  \"session\": {\n";
    json << "    \"start_time_ns\": " << session.start_time_ns << ",\n";
    json << "    \"frames_recorded\": " << session.frames_recorded << ",\n";
    json << "    \"avg_roi\": " << std::fixed << std::setprecision(2) << session.avg_roi << ",\n";
    json << "    \"best_speedup\": " << session.best_speedup << ",\n";
    json << "    \"worst_speedup\": " << session.worst_speedup << "\n";
    json << "  },\n";
    
    json << "  \"global\": {\n";
    json << "    \"acceptance_rate\": " << global_acceptance_rate.load() << ",\n";
    json << "    \"total_tokens\": " << total_tokens_generated.load() << ",\n";
    json << "    \"total_time_ms\": " << total_inference_time_ms.load() << "\n";
    json << "  },\n";
    
    json << "  \"experts\": [\n";
    for (int i = 0; i < MAX_EXPERTS; i++) {
        const auto& m = expert_metrics[i];
        int calls = m.total_calls.load();
        if (calls == 0) continue;
        
        json << "    {\n";
        json << "      \"id\": " << i << ",\n";
        json << "      \"calls\": " << calls << ",\n";
        json << "      \"accepted\": " << m.accepted_completions.load() << ",\n";
        json << "      \"rejected\": " << m.rejected_completions.load() << ",\n";
        json << "      \"acceptance_rate\": " << m.cumulative_acceptance_rate.load() << ",\n";
        json << "      \"avg_latency_ms\": " << m.avg_latency_ms.load() << ",\n";
        json << "      \"current_score\": " << m.current_score.load() << ",\n";
        json << "      \"category_scores\": [";
        for (int c = 0; c < 6; c++) {
            json << m.category_scores[c];
            if (c < 5) json << ", ";
        }
        json << "]\n";
        json << "    }";
        if (i < MAX_EXPERTS - 1) json << ",";
        json << "\n";
    }
    json << "  ]\n";
    json << "}\n";
    
    return json.str();
}

std::string FlightRecorder::get_status_summary() const {
    std::ostringstream summary;
    
    float acceptance = global_acceptance_rate.load();
    float roi = session.avg_roi;
    int tokens = total_tokens_generated.load();
    
    summary << "α=" << std::fixed << std::setprecision(1) << (acceptance * 100) << "%";
    summary << " | ROI=";
    
    if (roi < 0) {
        summary << "TAX " << std::setprecision(0) << roi << "%";
    } else {
        summary << "+" << std::setprecision(0) << roi << "%";
    }
    
    summary << " | " << tokens << "tok";
    
    // Find best expert
    int best_expert = -1;
    float best_score = 0.0f;
    for (int i = 0; i < MAX_EXPERTS; i++) {
        float score = expert_metrics[i].current_score.load();
        if (score > best_score) {
            best_score = score;
            best_expert = i;
        }
    }
    
    if (best_expert >= 0) {
        summary << " | E" << best_expert << "=" << std::setprecision(2) << best_score;
    }
    
    return summary.str();
}

} // namespace moe
