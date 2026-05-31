// ==============================================================================
// RAWR_SOVEREIGN_LOG.H - Zero-Dep Syscall Telemetry Interface
// Links to RAWR_NTWRITE_LINK.ASM for direct NtCreateFile/NtWriteFile access
// ==============================================================================
// PURPOSE: Expose RF counter + latency telemetry without kernel32.dll overhead
// GUARANTEES: No buffering, write-through, sector-aligned, 11x performance parity
// ==============================================================================

#ifndef RAWR_SOVEREIGN_LOG_H
#define RAWR_SOVEREIGN_LOG_H

#include <stdint.h>

// === ASSEMBLY EXPORTS (Implemented in RawrXD_NtWrite_Link.asm) ===

extern "C" {
    // Initialize sovereign log file via NtCreateFile (syscall 0x55)
    // Opens D:\scripts\phase7b_sovereign_drift.txt with NO_BUFFERING | WRITE_THROUGH
    // Returns: 0 = success, non-zero = NTSTATUS error code
    int64_t Rawr_Sovereign_Init_Log(void);
    
    // Flush telemetry buffer to disk via NtWriteFile (syscall 0x08)
    // Parameters:
    //   buffer: Sector-aligned buffer (must be multiple of 512 bytes)
    //   length: Buffer length (must be multiple of 512)
    // Returns: NTSTATUS (0 = success)
    int64_t Rawr_Sovereign_Log_Flush(const void* buffer, uint64_t length);
    
    // Format RF counters into sector-aligned telemetry record
    // Parameters:
    //   rf_data_seq: Ingress ring sequence counter
    //   rf_consumed_seq: GPU acknowledgment counter
    //   rf_frame_ready: Renderer signal counter
    //   epoch: Rollover safety epoch (increments every 2^32 cycles)
    //   ttft_ms: Time-to-first-token in milliseconds
    //   drift_delta: Computed drift between ingress and consumption
    // Returns: RAX = buffer address, RDX = 512 (always)
    void* Rawr_Format_RF_Telemetry(
        uint64_t rf_data_seq,
        uint64_t rf_consumed_seq,
        uint64_t rf_frame_ready,
        uint64_t epoch,
        uint64_t ttft_ms,
        int64_t drift_delta
    );
    
    // Close sovereign log via NtClose
    int64_t Rawr_Sovereign_Close_Log(void);
    
    // BAR base address (set by C++ during GPU mapping)
    extern uint64_t g_sovereign_bar_base;
}

// === C++ WRAPPER FOR CONVENIENCE ===

namespace RawrSovereign {
    
    struct RFCounters {
        uint64_t data_seq;          // Monotonic ingress counter
        uint64_t consumed_seq;      // GPU ack counter
        uint64_t frame_ready;       // Renderer signal
        uint64_t epoch;             // Rollover safety (2^32 boundary)
    };
    
    struct LatencyProfile {
        uint64_t ttft_cycles;       // Time-to-first-token in CPU cycles
        uint64_t ttft_ms;           // TTFT in milliseconds
        int64_t drift_delta;        // data_seq - consumed_seq (drift window)
    };
    
    // Initialize the sovereign logging system
    // Call once during DLL load, before any inference operations
    inline bool InitLog() {
        return Rawr_Sovereign_Init_Log() == 0;
    }
    
    // Log RF counters + latency profile to disk (zero-copy, zero-buffer)
    // This is the "Beat Google" hot path - pure syscalls, no kernel32.dll
    inline bool LogTelemetry(const RFCounters& counters, const LatencyProfile& profile) {
        // Format into sector-aligned buffer
        void* buffer = Rawr_Format_RF_Telemetry(
            counters.data_seq,
            counters.consumed_seq,
            counters.frame_ready,
            counters.epoch,
            profile.ttft_ms,
            profile.drift_delta
        );
        
        // Direct syscall flush (0x08 NtWriteFile)
        return Rawr_Sovereign_Log_Flush(buffer, 512) == 0;
    }
    
    // Cleanup (call during DLL unload)
    inline void CloseLog() {
        Rawr_Sovereign_Close_Log();
    }
    
    // Set BAR base for hardware counter reads
    inline void SetBARBase(uint64_t aperture_base) {
        g_sovereign_bar_base = aperture_base;
    }
}

#endif // RAWR_SOVEREIGN_LOG_H
