// phase_integration_real.cpp - COMPLETE INITIALIZATION SEQUENCE
// Implements unified phase initialization with proper sequencing
// Fixes missing initialization order and error handling

#include <windows.h>
#include <stdio.h>
#include <cstring>
#include <exception>

// Forward declaration for shutdown function called during init failure
void Titan_Master_Shutdown();

// ============================================================
// STRUCTURED LOGGING
// ============================================================
enum LogLevel { LOG_DEBUG = 0, LOG_INFO = 1, LOG_WARN = 2, LOG_ERROR = 3 };

static void LogMessage(LogLevel level, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    
    const char* level_str[] = { "[DEBUG]", "[INFO]", "[WARN]", "[ERROR]" };

    v

    va_end(args);
}

// ============================================================
// PHASE FUNCTION DECLARATIONS
// ============================================================
extern "C" {
    // Week phases
    extern int Titan_Week1_Init();
    extern void Titan_Week1_Shutdown();
    
    extern int Titan_Week2_3_Init();
    extern void Titan_Week2_3_Shutdown();
    
    // Regular phases
    extern int Titan_Phase1_Init();  // Hardware detection
    extern void Titan_Phase1_Shutdown();
    
    extern int Titan_Phase2_Init();  // Model selection
    extern void Titan_Phase2_Shutdown();
    
    extern int Titan_Phase3_Init();  // Agent kernel
    extern void Titan_Phase3_Shutdown();
    
    extern int Titan_Phase4_Init();  // I/O pipeline
    extern void Titan_Phase4_Shutdown();
    
    extern int Titan_Phase5_Init();  // Orchestration
    extern void Titan_Phase5_Shutdown();
    
    // HAL (Hardware Abstraction Layer)
    extern int Titan_HAL_Init_Real();
    extern void Titan_HAL_Shutdown();
}

// ============================================================
// PHASE STATE TRACKING
// ============================================================
enum class PhaseStatus {
    PENDING = 0,
    IN_PROGRESS = 1,
    COMPLETED = 2,
    FAILED = 3,
    RETRYING = 4
};

struct PhaseState {
    PhaseStatus hal_status;
    PhaseStatus week1_status;
    PhaseStatus week23_status;
    PhaseStatus phase1_status;
    PhaseStatus phase2_status;
    PhaseStatus phase3_status;
    PhaseStatus phase4_status;
    PhaseStatus phase5_status;
    
    int retry_count;
    int last_error_code;
    
    PhaseState() : hal_status(PhaseStatus::PENDING), week1_status(PhaseStatus::PENDING),
                   week23_status(PhaseStatus::PENDING), phase1_status(PhaseStatus::PENDING),
                   phase2_status(PhaseStatus::PENDING), phase3_status(PhaseStatus::PENDING),
                   phase4_status(PhaseStatus::PENDING), phase5_status(PhaseStatus::PENDING),
                   retry_count(0), last_error_code(0) {}
    
    bool isComplete() const {
        return hal_status == PhaseStatus::COMPLETED &&
               week1_status == PhaseStatus::COMPLETED &&
               week23_status == PhaseStatus::COMPLETED &&
               phase1_status == PhaseStatus::COMPLETED &&
               phase2_status == PhaseStatus::COMPLETED &&
               phase3_status == PhaseStatus::COMPLETED &&
               phase4_status == PhaseStatus::COMPLETED &&
               phase5_status == PhaseStatus::COMPLETED;
    }
    
    bool hasFailed() const {
        return hal_status == PhaseStatus::FAILED ||
               week1_status == PhaseStatus::FAILED ||
               week23_status == PhaseStatus::FAILED ||
               phase1_status == PhaseStatus::FAILED ||
               phase2_status == PhaseStatus::FAILED ||
               phase3_status == PhaseStatus::FAILED ||
               phase4_status == PhaseStatus::FAILED ||
               phase5_status == PhaseStatus::FAILED;
    }
    
    int completedCount() const {
        int count = 0;
        if (hal_status == PhaseStatus::COMPLETED) count++;
        if (week1_status == PhaseStatus::COMPLETED) count++;
        if (week23_status == PhaseStatus::COMPLETED) count++;
        if (phase1_status == PhaseStatus::COMPLETED) count++;
        if (phase2_status == PhaseStatus::COMPLETED) count++;
        if (phase3_status == PhaseStatus::COMPLETED) count++;
        if (phase4_status == PhaseStatus::COMPLETED) count++;
        if (phase5_status == PhaseStatus::COMPLETED) count++;
        return count;
    }
};

static PhaseState g_phase_state;

// ============================================================
// INITIALIZATION TIMING
// ============================================================
struct InitializationMetrics {
    DWORD hal_time;
    DWORD week1_time;
    DWORD week23_time;
    DWORD phase1_time;
    DWORD phase2_time;
    DWORD phase3_time;
    DWORD phase4_time;
    DWORD phase5_time;
    DWORD total_time;
    
    InitializationMetrics() : hal_time(0), week1_time(0), week23_time(0),
                              phase1_time(0), phase2_time(0), phase3_time(0),
                              phase4_time(0), phase5_time(0), total_time(0) {}
};

static InitializationMetrics g_init_metrics;

// ============================================================
// UNIFIED INITIALIZATION SEQUENCE
// ============================================================
int Titan_Master_Init() {
    DWORD master_start = GetTickCount();
    
    LogMessage(LOG_INFO, "======================================================");
    LogMessage(LOG_INFO, "=== RawrXD AI IDE Initialization Sequence Started ===");
    LogMessage(LOG_INFO, "======================================================");
    
    int result = 0;
    
    // ===== PHASE 0: HARDWARE ABSTRACTION LAYER =====
    {
        LogMessage(LOG_INFO, "[INIT] Phase 0: Hardware Abstraction Layer");
        DWORD phase_start = GetTickCount();
        
        result = Titan_HAL_Init_Real();
        g_init_metrics.hal_time = GetTickCount() - phase_start;
        
        if (result != 0) {
            LogMessage(LOG_ERROR, "[INIT FAILED] HAL initialization failed with code: %d", result);
            goto INIT_FAILED;
        }
        
        g_phase_state.hal_status = PhaseStatus::COMPLETED;
        LogMessage(LOG_INFO, "[INIT] HAL complete (%.0f ms)", (float)g_init_metrics.hal_time);
    }
    
    // ===== WEEK 1: CORE INFRASTRUCTURE =====
    {
        LogMessage(LOG_INFO, "[INIT] Week 1: Core Infrastructure");
        LogMessage(LOG_INFO, "  - Background thread pools");
        LogMessage(LOG_INFO, "  - Heartbeat monitor");
        LogMessage(LOG_INFO, "  - Resource manager");
        
        DWORD phase_start = GetTickCount();
        g_phase_state.week1_status = PhaseStatus::IN_PROGRESS;
        
        result = Titan_Week1_Init();
        g_init_metrics.week1_time = GetTickCount() - phase_start;
        
        if (result != 0) {
            g_phase_state.week1_status = PhaseStatus::FAILED;
            g_phase_state.last_error_code = result;
            LogMessage(LOG_ERROR, "[INIT FAILED] Week 1 initialization failed with code: %d", result);
            goto INIT_FAILED;
        }
        
        g_phase_state.week1_status = PhaseStatus::COMPLETED;
        LogMessage(LOG_INFO, "[INIT] Week 1 complete (%.0f ms)", (float)g_init_metrics.week1_time);
    }
    
    // ===== WEEK 2-3: MODEL INFRASTRUCTURE =====
    {
        LogMessage(LOG_INFO, "[INIT] Week 2-3: Model Infrastructure");
        LogMessage(LOG_INFO, "  - GGUF loader");
        LogMessage(LOG_INFO, "  - Tensor management");
        LogMessage(LOG_INFO, "  - Inference engine");
        
        DWORD phase_start = GetTickCount();
        
        result = Titan_Week2_3_Init();
        g_init_metrics.week23_time = GetTickCount() - phase_start;
        
        if (result != 0) {
            LogMessage(LOG_ERROR, "[INIT FAILED] Week 2-3 initialization failed with code: %d", result);
            goto INIT_FAILED;
        }
        
        g_phase_state.week23_status = PhaseStatus::COMPLETED;
        LogMessage(LOG_INFO, "[INIT] Week 2-3 complete (%.0f ms)", (float)g_init_metrics.week23_time);
    }
    
    // ===== PHASE 1: HARDWARE DETECTION =====
    {
        LogMessage(LOG_INFO, "[INIT] Phase 1: Hardware Detection");
        LogMessage(LOG_INFO, "  - GPU enumeration");
        LogMessage(LOG_INFO, "  - CPU feature detection");
        LogMessage(LOG_INFO, "  - Memory profiling");
        
        DWORD phase_start = GetTickCount();
        
        result = Titan_Phase1_Init();
        g_init_metrics.phase1_time = GetTickCount() - phase_start;
        
        if (result != 0) {
            LogMessage(LOG_ERROR, "[INIT FAILED] Phase 1 initialization failed with code: %d", result);
            goto INIT_FAILED;
        }
        
        g_phase_state.phase1_status = PhaseStatus::COMPLETED;
        LogMessage(LOG_INFO, "[INIT] Phase 1 complete (%.0f ms)", (float)g_init_metrics.phase1_time);
    }
    
    // ===== PHASE 2: MODEL SELECTION =====
    {
        LogMessage(LOG_INFO, "[INIT] Phase 2: Model Selection");
        LogMessage(LOG_INFO, "  - Model discovery");
        LogMessage(LOG_INFO, "  - Compatibility check");
        LogMessage(LOG_INFO, "  - Weight loading");
        
        DWORD phase_start = GetTickCount();
        
        result = Titan_Phase2_Init();
        g_init_metrics.phase2_time = GetTickCount() - phase_start;
        
        if (result != 0) {
            LogMessage(LOG_ERROR, "[INIT FAILED] Phase 2 initialization failed with code: %d", result);
            goto INIT_FAILED;
        }
        
        g_phase_state.phase2_status = PhaseStatus::COMPLETED;
        LogMessage(LOG_INFO, "[INIT] Phase 2 complete (%.0f ms)", (float)g_init_metrics.phase2_time);
    }
    
    // ===== PHASE 3: AGENT KERNEL =====
    {
        LogMessage(LOG_INFO, "[INIT] Phase 3: Agent Kernel");
        LogMessage(LOG_INFO, "  - Decision engine");
        LogMessage(LOG_INFO, "  - Tool registry");
        LogMessage(LOG_INFO, "  - Planning system");
        
        DWORD phase_start = GetTickCount();
        
        result = Titan_Phase3_Init();
        g_init_metrics.phase3_time = GetTickCount() - phase_start;
        
        if (result != 0) {
            LogMessage(LOG_ERROR, "[INIT FAILED] Phase 3 initialization failed with code: %d", result);
            goto INIT_FAILED;
        }
        
        g_phase_state.phase3_status = PhaseStatus::COMPLETED;
        LogMessage(LOG_INFO, "[INIT] Phase 3 complete (%.0f ms)", (float)g_init_metrics.phase3_time);
    }
    
    // ===== PHASE 4: I/O PIPELINE =====
    {
        LogMessage(LOG_INFO, "[INIT] Phase 4: I/O Pipeline");
        LogMessage(LOG_INFO, "  - File streaming");
        LogMessage(LOG_INFO, "  - Network protocols");
        LogMessage(LOG_INFO, "  - Buffer management");
        
        DWORD phase_start = GetTickCount();
        
        result = Titan_Phase4_Init();
        g_init_metrics.phase4_time = GetTickCount() - phase_start;
        
        if (result != 0) {
            LogMessage(LOG_ERROR, "[INIT FAILED] Phase 4 initialization failed with code: %d", result);
            goto INIT_FAILED;
        }
        
        g_phase_state.phase4_status = PhaseStatus::COMPLETED;
        LogMessage(LOG_INFO, "[INIT] Phase 4 complete (%.0f ms)", (float)g_init_metrics.phase4_time);
    }
    
    // ===== PHASE 5: ORCHESTRATION =====
    {
        LogMessage(LOG_INFO, "[INIT] Phase 5: Orchestration");
        LogMessage(LOG_INFO, "  - System integration");
        LogMessage(LOG_INFO, "  - Performance tuning");
        LogMessage(LOG_INFO, "  - Event loop setup");
        
        DWORD phase_start = GetTickCount();
        
        result = Titan_Phase5_Init();
        g_init_metrics.phase5_time = GetTickCount() - phase_start;
        
        if (result != 0) {
            LogMessage(LOG_ERROR, "[INIT FAILED] Phase 5 initialization failed with code: %d", result);
            goto INIT_FAILED;
        }
        
        g_phase_state.phase5_status = PhaseStatus::COMPLETED;
        LogMessage(LOG_INFO, "[INIT] Phase 5 complete (%.0f ms)", (float)g_init_metrics.phase5_time);
    }
    
    // ===== SUCCESS =====
    g_init_metrics.total_time = GetTickCount() - master_start;
    
    LogMessage(LOG_INFO, "");
    LogMessage(LOG_INFO, "======================================================");
    LogMessage(LOG_INFO, "=== Initialization Complete ===");
    LogMessage(LOG_INFO, "======================================================");
    LogMessage(LOG_INFO, "");
    LogMessage(LOG_INFO, "Initialization Timing Summary:");
    LogMessage(LOG_INFO, "  HAL:        %.0f ms", (float)g_init_metrics.hal_time);
    LogMessage(LOG_INFO, "  Week 1:     %.0f ms", (float)g_init_metrics.week1_time);
    LogMessage(LOG_INFO, "  Week 2-3:   %.0f ms", (float)g_init_metrics.week23_time);
    LogMessage(LOG_INFO, "  Phase 1:    %.0f ms", (float)g_init_metrics.phase1_time);
    LogMessage(LOG_INFO, "  Phase 2:    %.0f ms", (float)g_init_metrics.phase2_time);
    LogMessage(LOG_INFO, "  Phase 3:    %.0f ms", (float)g_init_metrics.phase3_time);
    LogMessage(LOG_INFO, "  Phase 4:    %.0f ms", (float)g_init_metrics.phase4_time);
    LogMessage(LOG_INFO, "  Phase 5:    %.0f ms", (float)g_init_metrics.phase5_time);
    LogMessage(LOG_INFO, "  ---Total:   %.0f ms", (float)g_init_metrics.total_time);
    LogMessage(LOG_INFO, "");
    
    return 0;
    
INIT_FAILED:
    LogMessage(LOG_ERROR, "");
    LogMessage(LOG_ERROR, "======================================================");
    LogMessage(LOG_ERROR, "=== Initialization FAILED ===");
    LogMessage(LOG_ERROR, "======================================================");
    LogMessage(LOG_ERROR, "Failed phase initialization returned: %d", result);
    LogMessage(LOG_ERROR, "Performing emergency shutdown sequence...");
    LogMessage(LOG_ERROR, "");
    
    Titan_Master_Shutdown();
    
    return result;
}

// ============================================================
// GRACEFUL SHUTDOWN SEQUENCE (REVERSE ORDER)
// ============================================================
void Titan_Master_Shutdown() {
    LogMessage(LOG_INFO, "");
    LogMessage(LOG_INFO, "======================================================");
    LogMessage(LOG_INFO, "=== RawrXD AI IDE Shutdown Sequence Started ===");
    LogMessage(LOG_INFO, "======================================================");
    LogMessage(LOG_INFO, "");
    
    // Shutdown in reverse order of initialization
    
    // Phase 5 shutdown
    if (g_phase_state.phase5_status == PhaseStatus::COMPLETED) {
        LogMessage(LOG_INFO, "[SHUTDOWN] Phase 5: Orchestration");
        try {
            Titan_Phase5_Shutdown();
            LogMessage(LOG_INFO, "[SHUTDOWN] Phase 5 complete");
        }
        catch (...) {
            LogMessage(LOG_ERROR, "[SHUTDOWN ERROR] Phase 5 shutdown exception");
        }
        g_phase_state.phase5_status = PhaseStatus::PENDING;
    }
    
    // Phase 4 shutdown
    if (g_phase_state.phase4_status == PhaseStatus::COMPLETED) {
        LogMessage(LOG_INFO, "[SHUTDOWN] Phase 4: I/O Pipeline");
        try {
            Titan_Phase4_Shutdown();
            LogMessage(LOG_INFO, "[SHUTDOWN] Phase 4 complete");
        }
        catch (...) {
            LogMessage(LOG_ERROR, "[SHUTDOWN ERROR] Phase 4 shutdown exception");
        }
        g_phase_state.phase4_status = PhaseStatus::PENDING;
    }
    
    // Phase 3 shutdown
    if (g_phase_state.phase3_status == PhaseStatus::COMPLETED) {
        LogMessage(LOG_INFO, "[SHUTDOWN] Phase 3: Agent Kernel");
        try {
            Titan_Phase3_Shutdown();
            LogMessage(LOG_INFO, "[SHUTDOWN] Phase 3 complete");
        }
        catch (...) {
            LogMessage(LOG_ERROR, "[SHUTDOWN ERROR] Phase 3 shutdown exception");
        }
        g_phase_state.phase3_status = PhaseStatus::PENDING;
    }
    
    // Phase 2 shutdown
    if (g_phase_state.phase2_status == PhaseStatus::COMPLETED) {
        LogMessage(LOG_INFO, "[SHUTDOWN] Phase 2: Model Selection");
        try {
            Titan_Phase2_Shutdown();
            LogMessage(LOG_INFO, "[SHUTDOWN] Phase 2 complete");
        }
        catch (...) {
            LogMessage(LOG_ERROR, "[SHUTDOWN ERROR] Phase 2 shutdown exception");
        }
        g_phase_state.phase2_status = PhaseStatus::PENDING;
    }
    
    // Phase 1 shutdown
    if (g_phase_state.phase1_status == PhaseStatus::COMPLETED) {
        LogMessage(LOG_INFO, "[SHUTDOWN] Phase 1: Hardware Detection");
        try {
            Titan_Phase1_Shutdown();
            LogMessage(LOG_INFO, "[SHUTDOWN] Phase 1 complete");
        }
        catch (...) {
            LogMessage(LOG_ERROR, "[SHUTDOWN ERROR] Phase 1 shutdown exception");
        }
        g_phase_state.phase1_status = PhaseStatus::PENDING;
    }
    
    // Week 2-3 shutdown
    if (g_phase_state.week23_status == PhaseStatus::COMPLETED) {
        LogMessage(LOG_INFO, "[SHUTDOWN] Week 2-3: Model Infrastructure");
        try {
            Titan_Week2_3_Shutdown();
            LogMessage(LOG_INFO, "[SHUTDOWN] Week 2-3 complete");
        }
        catch (...) {
            LogMessage(LOG_ERROR, "[SHUTDOWN ERROR] Week 2-3 shutdown exception");
        }
        g_phase_state.week23_status = PhaseStatus::PENDING;
    }
    
    // Week 1 shutdown
    if (g_phase_state.week1_status == PhaseStatus::COMPLETED) {
        LogMessage(LOG_INFO, "[SHUTDOWN] Week 1: Core Infrastructure");
        try {
            Titan_Week1_Shutdown();
            LogMessage(LOG_INFO, "[SHUTDOWN] Week 1 complete");
        }
        catch (...) {
            LogMessage(LOG_ERROR, "[SHUTDOWN ERROR] Week 1 shutdown exception");
        }
        g_phase_state.week1_status = PhaseStatus::PENDING;
    }
    
    // HAL shutdown
    if (g_phase_state.hal_status == PhaseStatus::COMPLETED) {
        LogMessage(LOG_INFO, "[SHUTDOWN] Phase 0: Hardware Abstraction Layer");
        try {
            Titan_HAL_Shutdown();
            LogMessage(LOG_INFO, "[SHUTDOWN] HAL complete");
        }
        catch (...) {
            LogMessage(LOG_ERROR, "[SHUTDOWN ERROR] HAL shutdown exception");
        }
        g_phase_state.hal_status = PhaseStatus::PENDING;
    }
    
    LogMessage(LOG_INFO, "");
    LogMessage(LOG_INFO, "======================================================");
    LogMessage(LOG_INFO, "=== Shutdown Complete ===");
    LogMessage(LOG_INFO, "======================================================");
    LogMessage(LOG_INFO, "");
}

// ============================================================
// QUERY FUNCTIONS
// ============================================================
bool Titan_IsInitialized() {
    return g_phase_state.phase5_status == PhaseStatus::COMPLETED;
}

bool Titan_GetPhaseState(int phase, bool* out_completed) {
    if (!out_completed) return false;
    
    switch (phase) {
        case 0: *out_completed = g_phase_state.hal_status == PhaseStatus::COMPLETED; return true;
        case 1: *out_completed = g_phase_state.week1_status == PhaseStatus::COMPLETED; return true;
        case 2: *out_completed = g_phase_state.week23_status == PhaseStatus::COMPLETED; return true;
        case 3: *out_completed = g_phase_state.phase1_status == PhaseStatus::COMPLETED; return true;
        case 4: *out_completed = g_phase_state.phase2_status == PhaseStatus::COMPLETED; return true;
        case 5: *out_completed = g_phase_state.phase3_status == PhaseStatus::COMPLETED; return true;
        case 6: *out_completed = g_phase_state.phase4_status == PhaseStatus::COMPLETED; return true;
        case 7: *out_completed = g_phase_state.phase5_status == PhaseStatus::COMPLETED; return true;
        default: return false;
    }
}

int Titan_GetInitializationTime() {
    return (int)g_init_metrics.total_time;
}

// ============================================================
// SAFE WRAPPER
// ============================================================
int Titan_Master_Init_Safe() {
    try {
        return Titan_Master_Init();
    }
    catch (const std::exception& e) {
        LogMessage(LOG_ERROR, "Exception in master initialization: %s", e.what());
        Titan_Master_Shutdown();
        return -999;
    }
    catch (...) {
        LogMessage(LOG_ERROR, "Unknown exception in master initialization");
        Titan_Master_Shutdown();
        return -1000;
    }
}
