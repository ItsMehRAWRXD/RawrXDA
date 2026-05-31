# RawrXD SWE-bench Evaluation Harness

`src/eval/swe_bench_harness.cpp` — standalone C++20 evaluation binary that
drives a live Ollama model through SWE-bench-style code-repair tasks and
records structured telemetry.

---

## Prompt Contract

The harness builds each prompt inside `build_patch_only_prompt()`.  Every
prompt ends with a strict unified diff anchor enforcing:

| Rule | Requirement |
|------|-------------|
| 1 | Output **only** a unified diff — no prose, no explanations |
| 2 | The diff **must** start with `--- a/<file>` on its very first line |
| 3 | Every hunk header in `@@ -L,S +L,S @@` form |
| 4 | No markdown fences (`` ``` `` or `~~~`) |
| 5 | No `NO_PATCH` unless the problem truly needs no change |
| 6 | Single-file target → exactly one `--- a/` header |
| 7 | Multi-file target → one contiguous diff block, files listed in order |
| 8 | Per-file `--- a/` / `+++ b/` headers required even for multi-file output |
| 9 | **CRITICAL**: single-file edits must emit exactly one `--- a/` header |

### Compliance Flags (emitted per sample in JSONL)

| Field | Type | Meaning |
|-------|------|---------|
| `has_header` | bool | Response contains ≥1 `--- a/` line |
| `has_hunks` | bool | Response contains ≥1 `@@ … @@` hunk |
| `is_fenced` | bool | Response wrapped in markdown fences (bad) |
| `prose_detected` | bool | Non-diff prose detected before first header (bad) |
| `starts_with_header` | bool | First non-blank line is `--- a/` |
| `contains_no_patch` | bool | Model emitted `NO_PATCH` token |
| `no_patch_exact` | bool | Response is exactly `NO_PATCH` |
| `is_multifile` | bool | ≥2 `--- a/` headers detected |
| `header_count` | int | Total `--- a/` headers counted |
| `hunk_count` | int | Total `@@ … @@` hunks counted |
| `gold_hunk_count` | int | Hunks in the gold reference patch |
| `target_file_count` | int | Files referenced in gold patch |
| `single_file_lock` | bool | Gold patch touches exactly 1 file |
| `fuzzy_patch_score` | float | Levenshtein similarity to gold patch [0, 1] |

### Phase 3 Telemetry Fields

| Field | Type | Meaning |
|-------|------|---------|
| `model_alias` | str | Friendly model label (`--model-alias`) |
| `prompt_byte_count` | int | Raw prompt size in bytes |
| `fuzzy_patch_score` | float | Edit-distance ratio vs gold [0, 1] |

---

## CLI Flags Quick Reference

```
RawrXD-SWEBench.exe --real-agent --model <name> --json <out.json> --jsonl <out.jsonl>
    [--host <host>]                    # default: 127.0.0.1
    [--port <port>]                    # default: 11434
    [--max-tasks <N>]                  # limit instances processed
    [--max-output-tokens <N>]          # cap generated tokens
    [--seed <N>]                       # reproducibility seed (-1 = random)
    [--model-alias <label>]            # friendly name in JSONL
    [--retry <N>]                      # extra retry attempts for empty/non-compliant responses
    [--strict-mode]                    # reject fenced output even if extractable
    [--fail-fast]                      # stop run on first failed task
    [--resume-checkpoint <path>]       # resume interrupted sweep
    [--resume-jsonl <path>]            # recover completed sample_ids from prior JSONL
    [--context-max-bytes <N>]          # max bytes per injected file (default: 2500)
    [--context-max-total-bytes <N>]    # max total injected bytes (default: 5000)
    [--context-max-files <N>]          # max injected files (default: 2)
    [--no-context]                     # disable context injection
    [--dump-raw-responses <dir>]       # write raw model responses per task
    [--dump-prompts <dir>]             # write built prompts per task
    [--jsonl-summary <path>]           # compact summary JSON
    [--timeout-ms <ms>]                # HTTP receive timeout (default: 240000)
    [--debug-http]                     # verbose HTTP/budget logs
    [--verbose]                        # alias for --debug-http
    [--dataset <path>]                 # load instances from JSONL/JSON file
    [--run-tests]                      # run test_cmds after patching
```

### Resumable Sweeps

Use `--resume-checkpoint <path>` to enable resume:

```powershell
# First run (interrupted)
.\RawrXD-SWEBench.exe --real-agent --model codestral:22b `
    --json reports\run.json --jsonl reports\run.jsonl `
    --resume-checkpoint reports\run.checkpoint

# Resume (skips already-completed tasks)
.\RawrXD-SWEBench.exe --real-agent --model codestral:22b `
    --json reports\run.json --jsonl reports\run.jsonl `
    --resume-checkpoint reports\run.checkpoint

# Resume using prior telemetry JSONL when checkpoint file is unavailable
.\RawrXD-SWEBench.exe --real-agent --model codestral:22b `
    --json reports\run.json --jsonl reports\run.jsonl `
    --resume-jsonl reports\run.jsonl
```

### File Lock Mitigation

When the build fails with linker error LNK1104 on `RawrXD-SWEBench.exe`, use:

```powershell
.\scripts\Unlock-SWEBenchBinary.ps1 -BuildDir d:\rawrxd\build-ninja-ctx2 -Force
```

or run the unlock+build wrapper:

```powershell
.\scripts\Build-SWEBench-Unlocked.ps1 -BuildDir d:\rawrxd\build-ninja-ctx2 -Jobs 8 -ForceKill
```

### Comparison Script

```powershell
.\baseline_compare.ps1 -Left reports\run_a.json -Right reports\run_b.json
```

### Deterministic Regression Gate

Run a deterministic pass/fail gate against the canonical baseline in
`reports/BENCHMARK_CONFIG.json`.

Compare-only mode (uses an existing candidate report):

```powershell
.\scripts\Run-SWEBench-DeterministicGate.ps1 `
    -CandidateJson reports\swe_codestral4_multifile_cap1024.json
```

Run-and-gate mode (executes harness first, then enforces thresholds):

```powershell
.\scripts\Run-SWEBench-DeterministicGate.ps1 -Run `
    -BuildDir d:\rawrxd\build `
    -OutputJson reports\swe_deterministic_candidate.json `
    -OutputJsonl reports\swe_deterministic_candidate.jsonl
```

The script exits non-zero on regression and is safe to use as a CI gate.

---

## Report Schema

JSON report (`--json`):
```json
{
  "total": 4, "completed": 4, "patch_correct": 1, "tests_passed": 0,
  "task_completion_rate": 1.0, "patch_correctness": 0.25,
  "test_pass_rate": 0.25, "overall_score": 0.4286,
  "results": [ ... per-task objects ... ]
}
```

JSONL telemetry (`--jsonl`): one JSON object per line, one per task.

---

## Canonical Baseline

See `reports/BENCHMARK_CONFIG.json` for the pinned model, token cap, and
task set used as the Phase 2 reference baseline.
