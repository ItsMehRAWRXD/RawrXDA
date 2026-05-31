# Max Streamable Benchmark

- Generated UTC: 2026-04-19T05:33:29.7045681Z
- Scanned models: 1
- Successful stream probes: 1
- One addition: mapped window view (64 MB default)
- One-addition sweep windows MB: 16
- Probe mode: first-zone-only (fast)

## Largest Actual Streamed
- File: D:\gptoss20b.gguf
- Size: 12.846 GiB
- Largest zone: 23.785 GiB (layers_0)
- Observed peak resident: 0.216 GiB

## Estimated Ceiling
- Current whole-zone loader ceiling: 19.409 GiB
- Based on model: D:\gptoss20b.gguf
- One-addition mapped-window ceiling: 7386.414 GiB
- Delta: 7367.005 GiB
- Best one-addition ceiling across sweep: 29545.656 GiB
- Best one-addition window: 16 MB
- Best one-addition delta: 29526.246 GiB

## Top Results

| Model | Size GiB | Streamable | Largest Zone GiB | Peak GiB | Estimated Current GiB | Estimated One-Addition GiB | Best One-Addition GiB | Best Window MB | TPS |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| gptoss20b.gguf | 12.846 | True | 23.785 | 0.216 | 19.409 | 7386.414 | 29545.656 | 16 |  |
