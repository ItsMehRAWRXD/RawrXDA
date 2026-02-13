# Brutal Gzip (ARM64 AArch64)

- Strategy: Stored-block gzip (no compression), malloc + memcpy
- ABI: AArch64, 16-byte aligned frame, minimal clobber
- Entry: `deflate_brutal_neon` in `kernels/deflate_brutal_neon.S`

Build (native ARM64)
```
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target bench_deflate_brutal_arm64 --config Release
./build/tests/bench_deflate_brutal_arm64
```

Target: Brutal speed win (1.00× ratio, ≤ 5 ms latency on 1 MB incompressible data).
