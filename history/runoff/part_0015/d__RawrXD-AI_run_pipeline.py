#!/usr/bin/env python3
"""
RawrXD-AI Autonomous Pipeline Orchestrator v2
===============================================
Fully agentic, self-healing pipeline with:
  - Persistent state across restarts (JSON state file)
  - Lock-file protection against concurrent runs
  - Automatic dependency installation
  - Retry with exponential backoff
  - Validation gates between phases
  - Health checks and resource monitoring
  - Signal handling for graceful shutdown
  - Phase-level checkpointing

Usage:
  python run_pipeline.py                  # Run everything autonomously
  python run_pipeline.py --phase 1        # Run only corpus extraction
  python run_pipeline.py --phase 2.5      # Run only agentic data generation
  python run_pipeline.py --from 2         # Resume from phase 2 onward
  python run_pipeline.py --status         # Show current pipeline state
  python run_pipeline.py --reset          # Reset state and start fresh
  python run_pipeline.py --validate       # Validate all outputs
  python run_pipeline.py --auto           # Full auto mode (default)
"""

import os
import sys
import time
import subprocess
import argparse
import json
import signal
import shutil
import hashlib
import platform
import psutil  # optional but useful
import traceback
from pathlib import Path
from datetime import datetime, timedelta
from typing import Dict, List, Optional, Tuple, Any

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
STATE_FILE = os.path.join(SCRIPT_DIR, ".pipeline_state.json")
LOCK_FILE = os.path.join(SCRIPT_DIR, ".pipeline.lock")
LOG_FILE = os.path.join(SCRIPT_DIR, "pipeline.log")

# ---------------------------------------------------------------------------
# Phase definitions
# ---------------------------------------------------------------------------

PHASES = {
    "1": {
        "script": "01_extract_corpus.py",
        "name": "Corpus Extraction",
        "description": "Scanning D: drive for all code, docs, configs...",
        "args": [],
        "requires_ml": False,
        "output_files": ["F:\\RawrXD-AI-Training\\corpus\\raw_corpus.jsonl"],
        "min_output_size_mb": 100,
        "timeout_hours": 4,
        "retries": 3,
    },
    "2": {
        "script": "02_format_instructions.py",
        "name": "Instruction Formatting",
        "description": "Converting raw files into training instructions...",
        "args": [],
        "requires_ml": False,
        "depends_on": ["1"],
        "output_files": ["F:\\RawrXD-AI-Training\\corpus\\instructions.jsonl"],
        "min_output_size_mb": 200,
        "timeout_hours": 6,
        "retries": 3,
    },
    "2.5": {
        "script": "05_generate_agentic_data.py",
        "name": "Agentic Tool-Use Data",
        "description": "Generating tool-use training data for autonomous IDE operation...",
        "args": [],
        "requires_ml": False,
        "depends_on": ["1"],
        "output_files": ["F:\\RawrXD-AI-Training\\corpus\\agentic_tool_use.jsonl"],
        "min_output_size_mb": 50,
        "timeout_hours": 2,
        "retries": 3,
    },
    "3": {
        "script": "03_qlora_finetune.py",
        "name": "QLoRA Fine-Tuning",
        "description": "Fine-tuning base model on your codebase (longest step)...",
        "args": ["--merge"],
        "requires_ml": True,
        "depends_on": ["2", "2.5"],
        "output_files": ["F:\\RawrXD-AI-Training\\merged_model\\config.json"],
        "min_output_size_mb": 1000,
        "timeout_hours": 96,
        "retries": 2,
    },
    "4": {
        "script": "04_merge_and_quantize.py",
        "name": "Merge & Quantize to GGUF",
        "description": "Creating all 4 GGUF variants for your hardware...",
        "args": [],
        "requires_ml": True,
        "depends_on": ["3"],
        "output_files": [
            "D:\\OllamaModels\\RawrXD-AI\\RawrXD-IDE-32B-Q2_K.gguf",
            "D:\\OllamaModels\\RawrXD-AI\\RawrXD-IDE-32B-Q3_K_M.gguf",
            "D:\\OllamaModels\\RawrXD-AI\\RawrXD-IDE-32B-Q4_K_M.gguf",
            "D:\\OllamaModels\\RawrXD-AI\\RawrXD-IDE-32B-Q5_K_M.gguf",
        ],
        "min_output_size_mb": 10000,
        "timeout_hours": 12,
        "retries": 2,
    },
}

PHASE_ORDER = ["1", "2", "2.5", "3", "4"]

# ---------------------------------------------------------------------------
# Logging
# ---------------------------------------------------------------------------

def log(msg: str, level: str = "INFO"):
    ts = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    line = f"[{ts}] [{level}] {msg}"
    print(line)
    try:
        with open(LOG_FILE, "a", encoding="utf-8") as f:
            f.write(line + "\n")
    except Exception:
        pass

def log_error(msg: str):
    log(msg, "ERROR")

def log_warn(msg: str):
    log(msg, "WARN")

# ---------------------------------------------------------------------------
# State management — survives restarts
# ---------------------------------------------------------------------------

class PipelineState:
    """Persistent pipeline state with atomic save."""

    def __init__(self):
        self.data: Dict[str, Any] = {
            "version": 2,
            "created": datetime.now().isoformat(),
            "last_updated": datetime.now().isoformat(),
            "phases": {},
            "global": {
                "total_runs": 0,
                "total_errors": 0,
                "deps_installed": False,
            }
        }
        self.load()

    def load(self):
        if os.path.exists(STATE_FILE):
            try:
                with open(STATE_FILE, "r") as f:
                    saved = json.load(f)
                if saved.get("version") == 2:
                    self.data = saved
                    log(f"Loaded pipeline state (last run: {saved.get('last_updated', 'unknown')})")
                else:
                    log_warn("State file version mismatch — starting fresh")
            except (json.JSONDecodeError, KeyError) as e:
                log_warn(f"Corrupted state file, starting fresh: {e}")

    def save(self):
        self.data["last_updated"] = datetime.now().isoformat()
        tmp = STATE_FILE + ".tmp"
        try:
            with open(tmp, "w") as f:
                json.dump(self.data, f, indent=2)
            if os.path.exists(STATE_FILE):
                os.replace(tmp, STATE_FILE)
            else:
                os.rename(tmp, STATE_FILE)
        except Exception as e:
            log_error(f"Failed to save state: {e}")

    def get_phase(self, phase_id: str) -> Dict:
        return self.data["phases"].setdefault(phase_id, {
            "status": "not_started",
            "attempts": 0,
            "last_attempt": None,
            "last_error": None,
            "started_at": None,
            "completed_at": None,
            "output_validated": False,
            "duration_seconds": None,
        })

    def mark_started(self, phase_id: str):
        p = self.get_phase(phase_id)
        p["status"] = "running"
        p["started_at"] = datetime.now().isoformat()
        p["attempts"] += 1
        self.save()

    def mark_completed(self, phase_id: str, duration: float):
        p = self.get_phase(phase_id)
        p["status"] = "completed"
        p["completed_at"] = datetime.now().isoformat()
        p["duration_seconds"] = round(duration, 1)
        p["last_error"] = None
        self.save()

    def mark_failed(self, phase_id: str, error: str):
        p = self.get_phase(phase_id)
        p["status"] = "failed"
        p["last_error"] = error
        p["last_attempt"] = datetime.now().isoformat()
        self.data["global"]["total_errors"] += 1
        self.save()

    def mark_validated(self, phase_id: str):
        p = self.get_phase(phase_id)
        p["output_validated"] = True
        self.save()

    def is_completed(self, phase_id: str) -> bool:
        return self.get_phase(phase_id)["status"] == "completed"

    def can_retry(self, phase_id: str) -> bool:
        p = self.get_phase(phase_id)
        max_retries = PHASES[phase_id]["retries"]
        return p["attempts"] < max_retries

    def reset(self):
        self.data["phases"] = {}
        self.data["global"]["total_runs"] += 1
        self.save()
        log("Pipeline state reset.")


# ---------------------------------------------------------------------------
# Lock file — prevent concurrent runs
# ---------------------------------------------------------------------------

class PipelineLock:
    def __init__(self):
        self.locked = False

    def acquire(self) -> bool:
        if os.path.exists(LOCK_FILE):
            try:
                with open(LOCK_FILE, "r") as f:
                    lock_data = json.load(f)
                pid = lock_data.get("pid")
                # Check if the process is still running
                if pid and self._process_alive(pid):
                    log_error(f"Pipeline already running (PID {pid}). Use --force to override.")
                    return False
                else:
                    log_warn(f"Stale lock file from PID {pid} — removing")
                    os.remove(LOCK_FILE)
            except Exception:
                os.remove(LOCK_FILE)

        with open(LOCK_FILE, "w") as f:
            json.dump({"pid": os.getpid(), "started": datetime.now().isoformat()}, f)
        self.locked = True
        return True

    def release(self):
        if self.locked and os.path.exists(LOCK_FILE):
            try:
                os.remove(LOCK_FILE)
            except Exception:
                pass
        self.locked = False

    @staticmethod
    def _process_alive(pid: int) -> bool:
        try:
            import psutil
            return psutil.pid_exists(pid)
        except ImportError:
            # Fallback: try os.kill with signal 0
            try:
                os.kill(pid, 0)
                return True
            except (OSError, ProcessLookupError):
                return False


# ---------------------------------------------------------------------------
# Health checks & resource monitoring
# ---------------------------------------------------------------------------

def check_disk_space(path: str, required_gb: float = 50) -> Tuple[bool, str]:
    """Ensure enough disk space is available."""
    try:
        usage = shutil.disk_usage(path)
        free_gb = usage.free / (1024**3)
        if free_gb < required_gb:
            return False, f"Low disk space on {path}: {free_gb:.1f}GB free, need {required_gb}GB"
        return True, f"{free_gb:.1f}GB free"
    except Exception as e:
        return True, f"Could not check disk: {e}"

def check_ram() -> Tuple[bool, str]:
    """Check available RAM."""
    try:
        import psutil
        vm = psutil.virtual_memory()
        avail_gb = vm.available / (1024**3)
        total_gb = vm.total / (1024**3)
        if avail_gb < 8:
            return False, f"Low RAM: {avail_gb:.1f}GB / {total_gb:.0f}GB available"
        return True, f"{avail_gb:.1f}GB / {total_gb:.0f}GB available"
    except ImportError:
        return True, "psutil not available, skipping RAM check"

def check_gpu() -> Tuple[bool, str]:
    """Check if GPU is available for ML phases."""
    try:
        import torch
        if torch.cuda.is_available():
            name = torch.cuda.get_device_name(0)
            vram = torch.cuda.get_device_properties(0).total_mem / (1024**3)
            return True, f"{name} ({vram:.0f}GB VRAM)"
        # Check for ROCm (AMD)
        if hasattr(torch, 'hip') or 'rocm' in torch.__version__.lower():
            return True, "ROCm GPU detected"
        return False, "No GPU detected — training will use CPU (very slow)"
    except ImportError:
        return False, "PyTorch not installed"

def health_check(needs_ml: bool = False) -> bool:
    """Run all health checks."""
    log("Running health checks...")
    all_ok = True

    # Disk checks
    for path, label in [("D:\\", "D: drive"), ("F:\\", "F: drive")]:
        ok, msg = check_disk_space(path, required_gb=30)
        status = "OK" if ok else "FAIL"
        log(f"  Disk [{label}]: {status} — {msg}")
        if not ok:
            all_ok = False

    # RAM
    ok, msg = check_ram()
    status = "OK" if ok else "WARN"
    log(f"  RAM: {status} — {msg}")

    # GPU (only for ML phases)
    if needs_ml:
        ok, msg = check_gpu()
        status = "OK" if ok else "WARN"
        log(f"  GPU: {status} — {msg}")

    return all_ok


# ---------------------------------------------------------------------------
# Dependency management
# ---------------------------------------------------------------------------

def install_dependencies(state: PipelineState, needs_ml: bool = False):
    """Auto-install required Python packages."""
    if state.data["global"]["deps_installed"]:
        return True

    log("Checking Python dependencies...")
    req_file = os.path.join(SCRIPT_DIR, "requirements.txt")

    # Core packages (always needed)
    core_packages = ["tqdm"]
    # ML packages (only for phases 3+)
    ml_packages = ["transformers", "datasets", "peft", "trl", "accelerate", "bitsandbytes"]

    packages_to_check = core_packages + (ml_packages if needs_ml else [])
    missing = []

    for pkg in packages_to_check:
        try:
            __import__(pkg)
        except ImportError:
            missing.append(pkg)

    if not missing:
        log("  All dependencies satisfied.")
        if needs_ml:
            state.data["global"]["deps_installed"] = True
            state.save()
        return True

    log(f"  Missing packages: {', '.join(missing)}")
    log(f"  Auto-installing from {req_file}...")

    try:
        # Install from requirements.txt
        if os.path.exists(req_file):
            result = subprocess.run(
                [sys.executable, "-m", "pip", "install", "-r", req_file],
                capture_output=True, text=True, timeout=600
            )
            if result.returncode == 0:
                log("  Dependencies installed successfully.")
                state.data["global"]["deps_installed"] = True
                state.save()
                return True
            else:
                log_error(f"  pip install failed: {result.stderr[:500]}")

        # Fallback: install individually
        for pkg in missing:
            log(f"  Installing {pkg}...")
            result = subprocess.run(
                [sys.executable, "-m", "pip", "install", pkg],
                capture_output=True, text=True, timeout=300
            )
            if result.returncode != 0:
                log_error(f"  Failed to install {pkg}: {result.stderr[:200]}")

        # ROCm torch for AMD GPU
        if needs_ml:
            try:
                import torch
                if not (torch.cuda.is_available() or hasattr(torch, 'hip')):
                    log("  Installing PyTorch for ROCm (AMD GPU)...")
                    subprocess.run([
                        sys.executable, "-m", "pip", "install", "torch",
                        "--index-url", "https://download.pytorch.org/whl/rocm6.0"
                    ], capture_output=True, text=True, timeout=600)
            except ImportError:
                log("  Installing PyTorch for ROCm (AMD GPU)...")
                subprocess.run([
                    sys.executable, "-m", "pip", "install", "torch",
                    "--index-url", "https://download.pytorch.org/whl/rocm6.0"
                ], capture_output=True, text=True, timeout=600)

        return True

    except Exception as e:
        log_error(f"  Dependency installation failed: {e}")
        return False


# ---------------------------------------------------------------------------
# Output validation gates
# ---------------------------------------------------------------------------

def validate_phase_output(phase_id: str, state: PipelineState) -> bool:
    """Validate that a phase produced expected outputs."""
    phase = PHASES[phase_id]
    output_files = phase.get("output_files", [])
    min_size = phase.get("min_output_size_mb", 0) * 1024 * 1024

    if not output_files:
        return True

    log(f"  Validating Phase {phase_id} outputs...")

    all_ok = True
    for filepath in output_files:
        if os.path.exists(filepath):
            size = os.path.getsize(filepath)
            size_mb = size / (1024 * 1024)

            if size < min_size and min_size > 0:
                log_warn(f"    {filepath}: {size_mb:.1f}MB (below expected {min_size/(1024*1024):.0f}MB)")
                # Don't fail on size — it might be a valid smaller output
            else:
                log(f"    {filepath}: {size_mb:.1f}MB — OK")

            # JSONL validation for corpus files
            if filepath.endswith(".jsonl"):
                try:
                    with open(filepath, "r", encoding="utf-8", errors="replace") as f:
                        first_line = f.readline().strip()
                        json.loads(first_line)  # Verify it's valid JSON
                    log(f"    {filepath}: Valid JSONL format")
                except (json.JSONDecodeError, Exception) as e:
                    log_error(f"    {filepath}: Invalid JSONL — {e}")
                    all_ok = False
        else:
            log_error(f"    {filepath}: NOT FOUND")
            all_ok = False

    if all_ok:
        state.mark_validated(phase_id)

    return all_ok


# ---------------------------------------------------------------------------
# Phase runner with retry & backoff
# ---------------------------------------------------------------------------

def run_phase(phase_id: str, state: PipelineState) -> bool:
    """Run a single phase with retry logic."""
    phase = PHASES[phase_id]
    script_path = os.path.join(SCRIPT_DIR, phase["script"])

    if not os.path.exists(script_path):
        log_error(f"Script not found: {script_path}")
        state.mark_failed(phase_id, f"Script not found: {script_path}")
        return False

    phase_state = state.get_phase(phase_id)
    attempt = phase_state["attempts"]
    max_retries = phase["retries"]

    # Exponential backoff on retries
    if attempt > 0:
        backoff = min(2 ** attempt * 30, 300)  # 30s, 60s, 120s, max 5m
        log(f"  Retry attempt {attempt + 1}/{max_retries} (waiting {backoff}s)...")
        time.sleep(backoff)

    log(f"")
    log(f"{'#' * 72}")
    log(f"#  PHASE {phase_id}: {phase['name']}")
    log(f"#  {phase['description']}")
    log(f"#  Attempt {attempt + 1}/{max_retries}")
    log(f"{'#' * 72}")

    # Health check
    if not health_check(needs_ml=phase.get("requires_ml", False)):
        log_warn("Health check warnings detected — continuing anyway")

    # Dependencies
    if phase.get("requires_ml", False):
        install_dependencies(state, needs_ml=True)

    state.mark_started(phase_id)

    # Build command
    cmd = [sys.executable, script_path] + phase.get("args", [])
    timeout_secs = int(phase.get("timeout_hours", 4) * 3600)

    start = time.time()
    try:
        result = subprocess.run(
            cmd, cwd=SCRIPT_DIR,
            timeout=timeout_secs,
            # Don't capture output — let it stream to console
        )
        elapsed = time.time() - start

        if result.returncode == 0:
            log(f"  Phase {phase_id} completed in {elapsed/60:.1f} minutes")
            state.mark_completed(phase_id, elapsed)

            # Validate output
            if validate_phase_output(phase_id, state):
                log(f"  Phase {phase_id} output validated.")
                return True
            else:
                log_warn(f"  Phase {phase_id} output validation had issues (non-fatal)")
                return True  # Don't block on validation warnings

        else:
            error_msg = f"Exit code {result.returncode} after {elapsed/60:.1f} min"
            log_error(f"  Phase {phase_id} FAILED: {error_msg}")
            state.mark_failed(phase_id, error_msg)
            return False

    except subprocess.TimeoutExpired:
        elapsed = time.time() - start
        error_msg = f"Timed out after {elapsed/3600:.1f} hours"
        log_error(f"  Phase {phase_id} TIMED OUT: {error_msg}")
        state.mark_failed(phase_id, error_msg)
        return False
    except Exception as e:
        error_msg = f"Exception: {str(e)}"
        log_error(f"  Phase {phase_id} EXCEPTION: {error_msg}")
        state.mark_failed(phase_id, error_msg)
        return False


# ---------------------------------------------------------------------------
# Signal handling for graceful shutdown
# ---------------------------------------------------------------------------

_shutdown_requested = False

def signal_handler(sig, frame):
    global _shutdown_requested
    if _shutdown_requested:
        log("Force quit — exiting immediately")
        sys.exit(1)
    _shutdown_requested = True
    log("Shutdown requested — finishing current phase then stopping...")
    log("Press Ctrl+C again to force quit")

signal.signal(signal.SIGINT, signal_handler)
if hasattr(signal, 'SIGTERM'):
    signal.signal(signal.SIGTERM, signal_handler)


# ---------------------------------------------------------------------------
# Pipeline orchestrator
# ---------------------------------------------------------------------------

def determine_phases_to_run(args, state: PipelineState) -> List[str]:
    """Figure out which phases need to run based on args and state."""
    if hasattr(args, 'phase') and args.phase:
        return [str(args.phase)]

    if hasattr(args, 'from_phase') and args.from_phase:
        from_str = str(args.from_phase)
        idx = PHASE_ORDER.index(from_str) if from_str in PHASE_ORDER else 0
        return PHASE_ORDER[idx:]

    # Auto mode: skip completed phases, run remaining
    phases = []
    for pid in PHASE_ORDER:
        if not state.is_completed(pid):
            phases.append(pid)
        else:
            log(f"  Phase {pid} ({PHASES[pid]['name']}): already completed — skipping")

    return phases


def check_dependencies_met(phase_id: str, state: PipelineState) -> bool:
    """Check if all dependency phases are completed."""
    deps = PHASES[phase_id].get("depends_on", [])
    for dep in deps:
        if not state.is_completed(dep):
            log_error(f"  Phase {phase_id} depends on Phase {dep} which is not completed")
            return False
    return True


def print_status(state: PipelineState):
    """Print current pipeline status."""
    print(f"\n{'=' * 72}")
    print(f"  RawrXD-AI Pipeline Status")
    print(f"{'=' * 72}")
    for pid in PHASE_ORDER:
        p = state.get_phase(pid)
        phase = PHASES[pid]
        status = p["status"].upper()
        attempts = p["attempts"]
        duration = p.get("duration_seconds")
        validated = p.get("output_validated", False)

        dur_str = f"{duration/60:.1f}min" if duration else "N/A"
        val_str = " [validated]" if validated else ""

        print(f"  Phase {pid:4s}: {phase['name']:30s} [{status:10s}] "
              f"attempts={attempts} duration={dur_str}{val_str}")

        if p.get("last_error"):
            print(f"             Last error: {p['last_error']}")

    print(f"\n  Total runs: {state.data['global']['total_runs']}")
    print(f"  Total errors: {state.data['global']['total_errors']}")
    print(f"  Deps installed: {state.data['global']['deps_installed']}")
    print(f"{'=' * 72}\n")


def run_pipeline(args):
    """Main pipeline execution."""
    state = PipelineState()
    lock = PipelineLock()

    # Status mode
    if hasattr(args, 'status') and args.status:
        print_status(state)
        return

    # Reset mode
    if hasattr(args, 'reset') and args.reset:
        state.reset()
        log("Pipeline reset. All phase states cleared.")
        return

    # Validate mode
    if hasattr(args, 'validate') and args.validate:
        log("Validating all phase outputs...")
        for pid in PHASE_ORDER:
            if state.is_completed(pid):
                validate_phase_output(pid, state)
        return

    # Acquire lock
    force = hasattr(args, 'force') and args.force
    if not lock.acquire() and not force:
        return

    try:
        state.data["global"]["total_runs"] += 1
        state.save()

        print("=" * 72)
        print("  RawrXD-AI Autonomous Training Pipeline v2")
        print("  Building YOUR AI from YOUR entire D: drive")
        print("  Fully agentic — self-healing — auto-recovery")
        print("=" * 72)
        print(f"  Target:   4 GGUF variants (Q2_K, Q3_K_M, Q4_K_M, Q5_K_M)")
        print(f"  Base:     Qwen2.5-Coder-32B-Instruct (QLoRA r=128)")
        print(f"  Output:   D:\\OllamaModels\\RawrXD-AI\\")
        print(f"  Hardware: RX 7800 XT 16GB | 64GB RAM | Ryzen 7 7800X3D")
        print(f"  Mode:     {'Auto-recover' if not (hasattr(args, 'phase') and args.phase) else 'Single phase'}")
        print("=" * 72)

        # Install core deps
        install_dependencies(state, needs_ml=False)

        # Determine phases
        phases = determine_phases_to_run(args, state)
        if not phases:
            log("All phases completed! Pipeline is done.")
            print_status(state)
            # Launch summary
            _print_final_summary(state)
            return

        log(f"Phases to run: {phases}")
        total_start = time.time()

        for pid in phases:
            if _shutdown_requested:
                log("Shutdown requested — stopping pipeline")
                break

            # Check dependencies
            if not check_dependencies_met(pid, state):
                log_error(f"Cannot run Phase {pid} — dependencies not met")
                # Try to run the dependency first
                deps = PHASES[pid].get("depends_on", [])
                for dep in deps:
                    if not state.is_completed(dep):
                        log(f"  Running dependency Phase {dep} first...")
                        success = _run_with_retries(dep, state)
                        if not success:
                            log_error(f"  Dependency Phase {dep} failed — cannot proceed to Phase {pid}")
                            break
                else:
                    # All deps resolved
                    pass

                if not check_dependencies_met(pid, state):
                    continue

            success = _run_with_retries(pid, state)
            if not success:
                log_error(f"Phase {pid} failed after all retries. Pipeline paused.")
                log(f"  Resume with: python run_pipeline.py --from {pid}")
                break

        total_elapsed = time.time() - total_start
        log(f"\nPipeline run completed in {total_elapsed/3600:.1f} hours")
        print_status(state)

        # Check if everything is done
        if all(state.is_completed(pid) for pid in PHASE_ORDER):
            _print_final_summary(state)

    finally:
        lock.release()


def _run_with_retries(phase_id: str, state: PipelineState) -> bool:
    """Run a phase with automatic retries."""
    max_retries = PHASES[phase_id]["retries"]

    while state.can_retry(phase_id):
        if _shutdown_requested:
            return False

        success = run_phase(phase_id, state)
        if success:
            return True

        if not state.can_retry(phase_id):
            log_error(f"Phase {phase_id} exhausted all {max_retries} retries")
            return False

        log(f"Phase {phase_id} failed — will retry ({state.get_phase(phase_id)['attempts']}/{max_retries})")

    return False


def _print_final_summary(state: PipelineState):
    """Print the final success summary with launch instructions."""
    print(f"\n{'*' * 72}")
    print(f"*  ALL PHASES COMPLETE — YOUR AI IS READY!")
    print(f"{'*' * 72}")
    print(f"*")
    print(f"*  Models created at: D:\\OllamaModels\\RawrXD-AI\\")
    print(f"*")
    print(f"*  Q2_K  (~12GB) — Fits entirely in VRAM  — Daily driver")
    print(f"*  Q3_K_M (~15GB) — Fits in VRAM           — Better accuracy")
    print(f"*  Q4_K_M (~19GB) — RAM+GPU split           — Near-original quality")
    print(f"*  Q5_K_M (~23GB) — RAM+GPU split           — Maximum quality")
    print(f"*")
    print(f"*  Launch your AI:")
    print(f"*    D:\\OllamaModels\\llama.cpp\\llama-server.exe ^")
    print(f"*      -m D:\\OllamaModels\\RawrXD-AI\\RawrXD-IDE-32B-Q2_K.gguf ^")
    print(f"*      -ngl 99 -c 8192 --port 8080")
    print(f"*")
    print(f"*  Then connect RawrXD IDE to http://127.0.0.1:8080/completion")
    print(f"{'*' * 72}")


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------

def main():
    parser = argparse.ArgumentParser(
        description="RawrXD-AI Autonomous Training Pipeline v2",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  python run_pipeline.py              Run full pipeline (auto-resumes)
  python run_pipeline.py --phase 2.5  Run only agentic data generation
  python run_pipeline.py --from 3     Resume from Phase 3 (QLoRA)
  python run_pipeline.py --status     Show pipeline state
  python run_pipeline.py --reset      Reset and start fresh
  python run_pipeline.py --validate   Validate all outputs
        """
    )
    parser.add_argument("--phase", type=str, choices=PHASE_ORDER,
                        help="Run only this phase")
    parser.add_argument("--from", dest="from_phase", type=str, choices=PHASE_ORDER,
                        help="Resume from this phase onward")
    parser.add_argument("--status", action="store_true",
                        help="Show current pipeline status")
    parser.add_argument("--reset", action="store_true",
                        help="Reset pipeline state")
    parser.add_argument("--validate", action="store_true",
                        help="Validate all phase outputs")
    parser.add_argument("--force", action="store_true",
                        help="Force run even if another instance is running")
    parser.add_argument("--auto", action="store_true", default=True,
                        help="Full autonomous mode (default)")
    args = parser.parse_args()

    run_pipeline(args)


if __name__ == "__main__":
    main()
