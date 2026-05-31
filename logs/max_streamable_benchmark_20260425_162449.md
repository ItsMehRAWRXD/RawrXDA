# Max Streamable Benchmark

- Generated UTC: 2026-04-25T20:25:02.3617691Z
- Scanned models: 7
- Successful stream probes: 6
- One addition: mapped window view (64 MB default)
- One-addition sweep windows MB: 16, 32, 64, 96, 128, 192, 256, 384, 512, 768, 1024
- Probe mode: all-zones (full)

## Largest Actual Streamed
- File: D:\rawrxd\llama3.2-3b-Q3_K_S.gguf
- Size: 1.437 GiB
- Largest zone: 0.298 GiB (layers_0)
- Observed peak resident: 0.395 GiB

## Estimated Ceiling
- Current whole-zone loader ceiling: 155.329 GiB
- Based on model: D:\rawrxd\phi3-mini-Q2_K.gguf
- One-addition mapped-window ceiling: 738.929 GiB
- Delta: 583.716 GiB
- Best one-addition ceiling across sweep: 2955.717 GiB
- Best one-addition window: 16 MB
- Best one-addition delta: 2800.504 GiB

## Top Results

| Model | Size GiB | Streamable | Largest Zone GiB | Peak GiB | Estimated Current GiB | Estimated One-Addition GiB | Best One-Addition GiB | Best Window MB | TPS |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| llama3.2-3b-Q3_K_S.gguf | 1.437 | True | 0.298 | 0.395 | 155.213 | 738.929 | 2955.717 | 16 |  |
| phi3-mini-Q2_K.gguf | 1.405 | True | 0.292 | 0.389 | 155.329 | 724.921 | 2899.686 | 16 |  |
| llama3.2-3b-Q2_K.gguf | 1.27 | True | 0.275 | 0.373 | 149.324 | 657.52 | 2630.078 | 16 |  |
| gemma3-1b-Q2_K.gguf | 0.651 | True | 0.299 | 0.396 | 70.642 | 337.757 | 1351.027 | 16 |  |
| bench_frag.gguf | 0.252 | True | 0 | 0.098 | 68527593.656 | 68527593.656 | 68527593.656 | 16 |  |
| bench_min.gguf | 0.002 | True | 0 | 0.098 | 4249993.75 | 4249993.75 | 4249993.75 | 16 |  |
| 70b_simulation.gguf | 0.001 | False | 0 | 0 | 0 | 0 | 0 | 16 |  |
