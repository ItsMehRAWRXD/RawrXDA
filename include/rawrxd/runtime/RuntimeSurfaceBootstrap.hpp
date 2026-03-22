#pragma once

namespace RawrXD::Runtime {

/// One-shot init for runtime surface (four-lane policy, quant legend, ROCm probe, compressed pool defaults).
/// **No** directory scan — callers register shards explicitly later.
void bootstrapRuntimeSurface();

}  // namespace RawrXD::Runtime
