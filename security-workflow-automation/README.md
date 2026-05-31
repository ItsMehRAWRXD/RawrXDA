# SecFlow Automation

A from-scratch, expert-oriented cybersecurity workflow automation framework for complex defensive operations.

## What It Is

SecFlow Automation is a policy-aware workflow engine for SOC and incident-response teams. It executes high-confidence, auditable playbooks for:

- Threat triage and enrichment
- Endpoint containment orchestration
- Identity and account response
- Evidence handling and forensic snapshots
- Notification and ticketing flows

This project is defensive by design:

- No exploit generation
- No offensive payload logic
- No unauthorized remote execution

## Core Capabilities

- DAG-based workflow execution with dependency control
- Step retries and conditional branching
- Approval-gated high-impact actions
- Dry-run mode for safe validation and tabletop exercises
- Structured execution audit trail and machine-readable report
- Multi-dimensional strategy automation for complex expansion scenarios
- Hardware-aware TPS benchmarking and backend recommendation (AMD/CUDA/CPU)
- Reverse-engineering subject discovery with commercial-priority clustering
- Local model loading lane planner for IDE-hosted inference
- Stale-session handling via fresh-session hot swap without retry loops
- Shared IDE controller used by both CLI and GUI paths
- Secure swarm configuration with hard bounds: 1..64 agents
- Per-agent model selection (`idx:model`) with input sanitization
- Per-role model assignment (`ask|plan|agent|support`) for parallel swarm lanes
- Per-pane model assignment with shared or distinct model sessions
- Per-project Drow state persistence with blocker tracking
- Dual source-selectable screen share sessions (`ide` and `desktop`)

## Project Structure

- `secflow_automation/models.py`: data models
- `secflow_automation/actions.py`: defensive action handlers
- `secflow_automation/policy.py`: guardrails and approval policy engine
- `secflow_automation/engine.py`: workflow orchestrator
- `secflow_automation/cli.py`: command line entrypoint
- `secflow_automation/workflows/`: expert workflow templates
- `tests/test_engine.py`: baseline unit tests

## Quick Start

1. Prepare an incident JSON file.
2. Run a workflow in dry-run mode.
3. Review structured output and audit records.
4. Re-run with explicit approvals for high-impact steps.

Example:

```powershell
python -m secflow_automation.cli \
  --workflow secflow_automation/workflows/ransomware_containment.json \
  --incident incident.json \
  --dry-run \
  --operator soc-tier3 \
  --swarm-size 16 \
  --default-model phi3-mini-q4.gguf \
  --swarm-model 2:codestral22b.gguf \
  --swarm-model 3:gptoss20b_link.gguf
```

Then execute with approvals:

```powershell
python -m secflow_automation.cli \
  --workflow secflow_automation/workflows/ransomware_containment.json \
  --incident incident.json \
  --approve isolate_host \
  --approve disable_account \
  --operator ir-lead
```

4D reverse-engineering expansion scenario:

```powershell
python -m secflow_automation.cli \
  --workflow secflow_automation/workflows/reverse_engineering_expansion_4d.json \
  --incident incident.sample.json \
  --dry-run \
  --operator re-strategy-lead \
  --pretty
```

Local model session optimization scenario (unstale session, no retry):

```powershell
python -m secflow_automation.cli \
  --workflow secflow_automation/workflows/local_model_unstale_session.json \
  --incident incident.sample.json \
  --dry-run \
  --operator local-ide-runtime \
  --drow project-alpha \
  --role-model ask:phi3-mini-q4.gguf \
  --role-model plan:codestral22b.gguf \
  --role-model agent:phi3-mini-q4.gguf \
  --role-model support:codestral22b.gguf \
  --pane-model left:phi3-mini-q4.gguf \
  --pane-model right:codestral22b.gguf \
  --share-source ide \
  --share-source desktop \
  --pretty
```

This workflow demonstrates:

- Choosing a local loading lane (persistent mmap vs hot-swap)
- Opening a local session with explicit staleness policy
- Auto-swapping to a fresh session when stale is detected
- No retry loop: a single fresh-session transition is used

GUI launch:

```powershell
python -m secflow_automation.gui --repo-root .
```

GUI smoke mode (headless wiring check):

```powershell
python -m secflow_automation.gui --repo-root . --smoke
```

Security hardening in this project:

- Workflow file path must remain inside repository root
- Swarm size strictly bounded to 64 max
- Model name/path traversal checks (`..`, absolute path blocks)
- Role and pane model maps are validated and sanitized
- Drow naming is constrained to safe normalized identifiers
- Share source selection is constrained to `ide` or `desktop`, max 2
- Shared secure request validation used by both CLI and GUI

## Workflow Schema

Top-level fields:

- `name`
- `description`
- `risk_tier`
- `required_approvals` (step ids)
- `steps` (list)

Each step supports:

- `id`
- `action`
- `params`
- `depends_on`
- `condition` (simple rule, optional)
- `retries`

Advanced chaining behavior:

- Each completed step persists output to `vars.steps.<step_id>` and `vars.<step_id>`.
- This allows later conditions like:
  - `vars.steps.hardware_benchmark.best_backend == amd_rocm`

Supported `condition` operators:

- `==`, `!=`, `>=`, `<=`, `>`, `<`

Example condition:

- `incident.severity >= 8`

## Security Notes

- Policy engine blocks high-impact actions without explicit approval
- Actions are simulated connectors, suitable for integration hardening
- Integrate with real EDR/SIEM/ITSM APIs by replacing handler internals in `actions.py`

## Intended Audience

- SOC engineers
- Incident responders
- Detection and response architects
- Security automation engineers
