# RawrXD Overclock Governor

## Overview
The RawrXD Model Loader includes an adaptive overclock governor designed for *observability-first* performance tuning. It never applies unsafe voltage changes directly; instead it adjusts frequency offsets through vendor tooling (currently simulated) while continuously monitoring telemetry.

## Components
- Telemetry: `telemetry.*` gathers CPU/GPU temps and usage (WMI, PDH, vendor CLI parsing).
- Vendor Interface: `overclock_vendor.*` detects Ryzen Master / AMD CLI tools (simulation layer now; real hooks can replace stubs).
- Governor: `overclock_governor.*` adaptive loop adjusting `applied_core_offset_mhz`, enforcing thermal / safety rollback.
- Stress Harness: `rawrxd_stress` executes dense MatMul loops to surface thermal behavior.
- Bench Integration: `model_loader_bench` emits thermal headroom and applied offset.
- Logging: `oc-session.log` captures lifecycle events, boost steps, thermal faults, rollbacks.

## Configuration (`settings/overclock.conf`)
| Key | Description |
|-----|-------------|
| enable_overclock_governor | true = start adaptive loop at launch |
| target_all_core_mhz | Desired all-core ceiling (0 = auto baseline) |
| boost_step_mhz | Increment applied on stable interval |
| max_cpu_temp_c | Thermal cap triggering step-down / rollback |
| max_gpu_hotspot_c | GPU thermal cap (headroom only for now) |
| max_core_voltage | Guardrail (not actively set) |

## Governor Logic
1. Detect vendor tools and set status.
2. Establish baseline frequency (`target_all_core_mhz` or auto). Applied offset starts at 0.
3. Each second:
   - Poll telemetry
   - Check thermal caps
   - On cap breach: decrement offset; count fault; log `thermal_fault`
   - After 3 consecutive faults: rollback offset to 0 (log `rollback`).
   - If stable for 10s: attempt `boost_step` (capped by target).
4. Status strings: `running`, `boost-step`, `stable`, `thermal-throttle`, `rollback`, `stopped`.

## Logging Format (`oc-session.log`)
```
HH:MM:SS <event> offset=<mhz> cpu_temp=<C> gpu_temp=<C> status=<state>
```
Events: `start`, `boost_step`, `thermal_fault`, `rollback`, `stop`.

## Safe Operation Principles
- No direct voltage manipulation (voltage key is an upper reference only).
- Rollback after repeated thermal infractions prevents oscillation.
- Telemetry validity checks ignore implausible readings (< -50C or > 130C).

## Extending Vendor Control
Replace stubs in `overclock_vendor.cpp`:
- Use Ryzen Master SDK / CLI to apply profiles.
- Implement GPU offset adjustments via ADL / AMD API.
- Handle error codes; populate `g_lastError` and surface in `governor_last_fault`.

## Stress Testing
Run controlled thermal load:
```
rawrxd_stress --cpu-max 85 --gpu-max 95 --seconds 120 --size 512
```
Observe offset evolution and `oc-session.log` entries.

## Benchmark Metrics Added
`model_loader_bench` now reports:
- `overclock_governor_enabled`
- `cpu_temp_c`, `gpu_hotspot_c`
- `cpu_temp_headroom_c`, `gpu_temp_headroom_c`
- `applied_core_offset_mhz`
Use `scripts/bench-summary.ps1` to aggregate performance vs thermal headroom.

## Future Enhancements
- PID-style control tolerances (adaptive interval scaling).
- Separate per-core telemetry analysis for heterogenous architectures.
- GPU clock stepping and VRAM temperature guard.
- Persistent baseline profiling (store initial offset viability matrix).
- Real vendor integration and fault classification taxonomy.

## Disclaimer
Use at your own risk. Thermal and frequency control can reduce hardware lifespan if misconfigured. Keep conservative limits and monitor `oc-session.log` for repeated rollback events signaling instability.
