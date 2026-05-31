# GGUF Prefetch Aperture Performance Manifest

Date: 2026-04-03
Status: Promoted
Default aperture: 2MB
Previous aperture: 8MB
Method: Interleaved Four-Gate suite (10 iterations, B/C alternating)

## Run Log

| Run | Type | Timestamp | Map-Init (ms) | FTL (ms) | Cold-Start (s) | GGUF_MAP_STATS |
| --- | --- | --- | ---: | ---: | ---: | --- |
| 01 | B (8MB) | 04:18:02 | 142 | 840 | 1.21 | OK |
| 02 | C (2MB) | 04:19:15 | 118 | 838 | 1.18 | OK |
| 03 | B (8MB) | 04:20:28 | 145 | 845 | 1.24 | OK |
| 04 | C (2MB) | 04:21:40 | 115 | 842 | 1.19 | OK |
| 05 | B (8MB) | 04:22:55 | 141 | 839 | 1.22 | OK |
| 06 | C (2MB) | 04:24:08 | 116 | 841 | 1.20 | OK |
| 07 | B (8MB) | 04:25:20 | 144 | 852 | 1.25 | OK |
| 08 | C (2MB) | 04:26:33 | 117 | 840 | 1.18 | OK |
| 09 | B (8MB) | 04:27:45 | 143 | 844 | 1.23 | OK |
| 10 | C (2MB) | 04:28:58 | 115 | 839 | 1.19 | OK |

## Four-Gate Decision Matrix

| Gate | Metric | Baseline (Mean/p95) | Candidate (Mean/p95) | Delta % (Mean) | Status |
| --- | --- | --- | --- | ---: | --- |
| 1. Efficiency | Map-Init | 143 / 145 ms | 116 / 118 ms | -18.88% | PASS |
| 2. Performance | FTL | 844 / 852 ms | 840 / 842 ms | -0.47% | PASS |
| 2. Performance | Cold-Start | 1.23 / 1.25 s | 1.19 / 1.20 s | -3.25% | PASS |
| 3. Stability | GGUF errors | 0 | 0 | N/A | PASS |
| 4. Consistency | p95(FTL) | 852 ms | 842 ms | -1.17% | PASS |
| 4. Consistency | p95(CS) | 1.25 s | 1.20 s | -4.00% | PASS |

## Gate Evidence

- Gate 1 passed: Map-Init improved by 18.88%, exceeding the 15% threshold.
- Gate 2 passed: FTL and Cold-Start both improved versus baseline.
- Gate 3 passed: all candidate runs reported GGUF_MAP_STATS as OK with zero mapping failures.
- Gate 4 passed: p95 for FTL and Cold-Start both improved.

## Verdict

Promote 2MB as default aperture. All four gates passed with no stability regressions.