#pragma once
#include <cstdint>
#include <string>

namespace RawrXD::Inference {
    struct TitanDiagnostics {
        bool dll_present;
        bool proc_table_valid;
        uint32_t version_major;
        uint32_t version_minor;
        std::string last_error;
        uint64_t detected_cpu_features;  // AVX-512, etc.
        
        static TitanDiagnostics probe();
        static void alert_user_on_fallback(const TitanDiagnostics& diag);
    };
}
