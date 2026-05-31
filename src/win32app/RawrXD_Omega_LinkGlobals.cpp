#include <cstdint>

extern "C" {

// Assembly-side Omega telemetry/fence globals for Win32IDE link lane.
volatile long g_rawrxd_completion_fence = 0;
volatile unsigned long long g_rawrxd_last_doorbell_addr = 0;
volatile unsigned long long g_rawrxd_last_doorbell_value = 0;
volatile unsigned long long g_rawrxd_last_doorbell_emit_seq = 0;
volatile long g_rawrxd_omega_probe_early_return = 1;

}
