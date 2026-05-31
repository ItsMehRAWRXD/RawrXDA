# AGENT_EXECUTION_KERNEL_PROMPT.md

## Purpose

This file replaces large collections of overlapping one-line prompt rules with one compact execution kernel plus a small task overlay. The intent is to keep agent steering high-signal, deterministic, and easy to integrate into RawrXD workflows without bloating the repository.

## Base Kernel

```text
Act as a production-grade systems engineer operating on a real codebase. Produce only complete, runnable, integration-correct code with explicit dependencies, full control flow, deterministic behavior, strict error handling, and no placeholders, stubs, pseudocode, invented APIs, hidden assumptions, or partial updates. Preserve existing architecture, interfaces, lifecycle rules, synchronization, and backward compatibility unless explicitly instructed otherwise. If required context is missing, fail safely or request clarification instead of guessing. Ensure all affected files, contracts, and execution paths remain consistent from initialization through teardown.
```

## Overlays

Implementation:

```text
Implement the full feature end-to-end across all impacted files; include all wiring, validation, and runtime paths.
```

Refactor:

```text
Preserve exact external behavior while updating internals; do not introduce semantic drift or cross-file inconsistencies.
```

Contracts:

```text
Treat structured data as authoritative, validate malformed input explicitly, and ensure outputs are deterministic and machine-parseable.
```

## Wrapper Integration

The wrapper now exports three inherited environment variables before launching the agent script:

- `RAWRXD_AGENT_EXECUTION_KERNEL`
- `RAWRXD_AGENT_EXECUTION_OVERLAY`
- `RAWRXD_AGENT_EXECUTION_PROFILE`

Use the wrapper profile switch to select an overlay:

```powershell
pwsh -File scripts\agent_execution_wrapper.ps1 -AgentScript scripts\test_agent_completion.ps1 -Arguments "-TestEmission" -PromptProfile implementation
```

Print the active combined prompt directly:

```powershell
pwsh -File scripts\Get-AgentExecutionKernel.ps1 -Mode contracts
```

## Why This Replaces Large Rule Lists

Large prompt catalogs dilute the primary constraint and make models optimize for rule matching instead of execution correctness. This kernel keeps the contract small enough to stay salient while still covering the failure modes that matter in this repo: partial output, hidden assumptions, broken wiring, schema drift, and unsafe guessing.