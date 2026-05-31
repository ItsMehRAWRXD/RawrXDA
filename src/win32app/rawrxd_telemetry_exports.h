#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// MASM Telemetry Kernel Exports
// UTC_InitTelemetry — Opens the log file and marks subsystem as ready
// RCX = Pointer to log file path (null-terminated), or NULL for default
uint64_t UTC_InitTelemetry(const char* logPath);

// UTC_ShutdownTelemetry — Flush remaining events, close handle
uint64_t UTC_ShutdownTelemetry();

// UTC_IncrementCounter — Thread-safe atomic increment
// counterPtr = Address of the counter
uint64_t UTC_IncrementCounter(uint64_t* counterPtr);

// UTC_DecrementCounter — Thread-safe atomic decrement
uint64_t UTC_DecrementCounter(uint64_t* counterPtr);

// UTC_ReadCounter — Acquire-fence read of a 64-bit counter
uint64_t UTC_ReadCounter(uint64_t* counterPtr);

// UTC_ResetCounter — Atomically zero a counter and return old value
uint64_t UTC_ResetCounter(uint64_t* counterPtr);

// UTC_LogEvent — Wait-free write to the ring buffer
// message = Pointer to null-terminated message string
uint64_t UTC_LogEvent(const char* message);

// UTC_FlushToDisk — Drain ring buffer to log file
uint64_t UTC_FlushToDisk();

// Shared telemetry counters bridged from the MASM/C++ runtime.
extern uint64_t g_Counter_AgentLoop;
extern uint64_t g_Counter_Errors;

#ifdef __cplusplus
}
#endif
