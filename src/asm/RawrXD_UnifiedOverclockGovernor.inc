; =============================================================================
; RawrXD_UnifiedOverclockGovernor.inc — Structure layouts for x64 MASM
; Matches unified_overclock_governor.h (no C++ dependency)
; =============================================================================

; HardwareDomain enum
HARDWARE_DOMAIN_CPU      EQU 0
HARDWARE_DOMAIN_GPU      EQU 1
HARDWARE_DOMAIN_MEMORY   EQU 2
HARDWARE_DOMAIN_STORAGE  EQU 3
HARDWARE_DOMAIN_COUNT    EQU 4

; ClockDirection
CLOCK_OVERCLOCK         EQU 0
CLOCK_UNDERCLOCK        EQU 1
CLOCK_STOCK             EQU 2

; AutoTuneStrategy
AUTO_TUNE_DISABLED     EQU 0
AUTO_TUNE_CONSERVATIVE  EQU 1
AUTO_TUNE_BALANCED     EQU 2
AUTO_TUNE_AGGRESSIVE   EQU 3
AUTO_TUNE_ADAPTIVE_ML  EQU 4

; ClockResult (RAX=success 0/1, RDX=detail ptr, R8=errorCode)
; Caller uses: test eax,eax / jz fail; mov rdx,[rdx] for message

; ClockProfile — per-domain settings
CLOCK_PROFILE STRUCT 8
    domain              DWORD   ?
    direction           DWORD   ?
    offsetMhz           SDWORD  ?
    minFreqMhz          SDWORD  ?
    maxFreqMhz          SDWORD  ?
    targetTempC         REAL4   ?
    criticalTempC       REAL4   ?
    hysteresisC         REAL4   ?
    autoTuneEnabled     DWORD   ?   ; BOOL as DWORD
    autoTuneStrategy    DWORD   ?
CLOCK_PROFILE ENDS
CLOCK_PROFILE_SIZE      EQU 40

; PIDState — per-domain PID controller
PID_STATE STRUCT 8
    kp                  REAL4   ?
    ki                  REAL4   ?
    kd                  REAL4   ?
    integral            REAL4   ?
    prevError           REAL4   ?
    output              REAL4   ?
    consecutiveFaults    SDWORD  ?
    totalFaults         SDWORD  ?
    lastFaultTime       QWORD   ?   ; chrono tick
PID_STATE ENDS
PID_STATE_SIZE          EQU 40

; DomainTelemetry (pidSnapshot inlined as 40 bytes)
DOMAIN_TELEMETRY STRUCT 8
    domain              DWORD   ?
    currentTempC        REAL4   ?
    currentFreqMhz      REAL4   ?
    baselineFreqMhz     REAL4   ?
    appliedOffsetMhz    SDWORD  ?
    powerDrawWatts      REAL4   ?
    utilizationPct      REAL4   ?
    efficiencyScore     REAL4   ?
    adjustmentCount     QWORD   ?
    faultCount          QWORD   ?
    autoTuneActive      DWORD   ?
    activeStrategy      DWORD   ?
    pidSnapshot         BYTE PID_STATE_SIZE dup(?)
DOMAIN_TELEMETRY ENDS
DOMAIN_TELEMETRY_SIZE   EQU 96

; Governor singleton state (C++ class layout equivalent)
GOV_PROFILES_OFF        EQU 0
GOV_PID_STATES_OFF      EQU (GOV_PROFILES_OFF + HARDWARE_DOMAIN_COUNT * CLOCK_PROFILE_SIZE)
GOV_TELEMETRY_OFF       EQU (GOV_PID_STATES_OFF + HARDWARE_DOMAIN_COUNT * PID_STATE_SIZE)
GOV_BASELINES_OFF       EQU (GOV_TELEMETRY_OFF + HARDWARE_DOMAIN_COUNT * DOMAIN_TELEMETRY_SIZE)
GOV_RUNNING_OFF         EQU (GOV_BASELINES_OFF + HARDWARE_DOMAIN_COUNT * 4)   ; float[4]
GOV_EMERGENCY_OFF       EQU (GOV_RUNNING_OFF + 4)   ; atomic bool
GOV_MUTEX_OFF           EQU (GOV_EMERGENCY_OFF + 4) ; CRITICAL_SECTION
GOV_CONTROL_THREAD_OFF  EQU (GOV_MUTEX_OFF + 40)    ; HANDLE
GOV_APP_STATE_OFF       EQU (GOV_CONTROL_THREAD_OFF + 8)
GOV_STATE_SIZE          EQU (GOV_APP_STATE_OFF + 8)
