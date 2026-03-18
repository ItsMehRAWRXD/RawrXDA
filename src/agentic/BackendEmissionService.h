#pragma once
#include <vector>
#include <cstdint>
#include <string>
#include <functional>

namespace RawrXD::Agentic {
    // Bridges AgenticController -> BareMetal_PE_Writer.asm
    class BackendEmissionService {
    public:
        struct EmissionRequest {
            std::vector<uint8_t> machine_code;  // Raw x64 instructions
            std::string entry_point_symbol;      // "AgentTask_Main"
            uint64_t preferred_base;             // 0x140000000 default
            bool require_relocatable;
        };
        
        struct EmissionResult {
            bool success;
            std::string output_path;             // Generated .exe
            std::string error_message;
            uint32_t entry_point_rva;
        };
        
        // Synchronous emission - returns when .exe is written
        static EmissionResult emit_executable(const EmissionRequest& req);
        
        // Async emission for non-blocking agent workflow
        using CompletionCallback = std::function<void(const EmissionResult&)>;
        static void emit_executable_async(const EmissionRequest& req, CompletionCallback cb);
        
        // Validate that BareMetal_PE_Writer.asm is available
        static bool is_emitter_available();

        // Capability flag (default off) to allow PE emission in locked-down environments
        static void set_emitter_enabled(bool enabled);
        static bool is_emitter_enabled();
    };
}
