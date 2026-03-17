#!/usr/bin/env python3
"""
RawrXD-AI — Unified Training Pipeline Launcher

Single entry point that runs the full from-scratch pipeline:
  Phase 1b: Crawl E:\ → append to corpus
  Phase 2b: Generate instructions for E:\ data
  Phase 3a: Train BPE tokenizer from scratch
  Phase 3b: Verify model architecture
  Phase 3c: Train model from scratch (no base model)
  Phase 4:  Export to GGUF

Usage:
  python run_pipeline.py                      # Run everything
  python run_pipeline.py --start-phase 3      # Skip corpus, start at training
  python run_pipeline.py --preset medium      # Use medium model (1.3B)
  python run_pipeline.py --preset nano --epochs 1  # Quick test run
"""

import os
import sys
import json
import time
import subprocess
from pathlib import Path
from datetime import datetime

TRAINING_DIR = Path(r"F:\RawrXD-AI-Training")
SCRIPTS = {
    "1b": TRAINING_DIR / "01_crawl_e_drive.py",
    "2b": TRAINING_DIR / "02_generate_instructions_e.py",
    "3a": TRAINING_DIR / "03a_train_tokenizer.py",
    "3b": TRAINING_DIR / "03b_model_architecture.py",
    "3c": TRAINING_DIR / "03c_train_from_scratch.py",
    "4":  TRAINING_DIR / "04_export_gguf.py",
}

PHASE_ORDER = ["1b", "2b", "3a", "3b", "3c", "4"]
PHASE_NAMES = {
    "1b": "Corpus Crawl (E:\\)",
    "2b": "Instruction Generation (E:\\)",
    "3a": "BPE Tokenizer Training",
    "3b": "Model Architecture Verify",
    "3c": "From-Scratch Model Training",
    "4":  "GGUF Export",
}


def run_phase(phase_id: str, extra_args: list = None):
    """Run a pipeline phase as a subprocess."""
    script = SCRIPTS[phase_id]
    if not script.exists():
        print(f"  [ERROR] Script not found: {script}")
        return False

    cmd = [sys.executable, str(script)]
    if extra_args:
        cmd.extend(extra_args)

    print(f"\n{'='*72}")
    print(f"  Phase {phase_id}: {PHASE_NAMES[phase_id]}")
    print(f"  Script: {script}")
    print(f"{'='*72}\n")

    t0 = time.time()
    result = subprocess.run(cmd, cwd=str(TRAINING_DIR))
    elapsed = time.time() - t0

    status = "OK" if result.returncode == 0 else f"FAILED (exit {result.returncode})"
    print(f"\n  Phase {phase_id} {status} [{elapsed:.1f}s]")
    return result.returncode == 0


def check_prerequisites():
    """Check that required packages are installed."""
    required = {
        "torch": "PyTorch (pip install torch)",
        "tokenizers": "HuggingFace Tokenizers (pip install tokenizers)",
        "numpy": "NumPy (pip install numpy)",
    }
    optional = {
        "torch_directml": "DirectML for AMD GPU (pip install torch-directml)",
    }

    print("[CHECK] Verifying dependencies...")
    missing = []
    for pkg, desc in required.items():
        try:
            __import__(pkg)
            print(f"  ✓ {pkg}")
        except ImportError:
            print(f"  ✗ {pkg} — {desc}")
            missing.append(desc)

    for pkg, desc in optional.items():
        try:
            __import__(pkg)
            print(f"  ✓ {pkg} (optional)")
        except ImportError:
            print(f"  ○ {pkg} — {desc} (optional, will use CPU)")

    if missing:
        print(f"\n[ERROR] Missing required packages:")
        for m in missing:
            print(f"  pip install {m.split('(')[1].rstrip(')')}")
        return False

    return True


def main():
    import argparse
    parser = argparse.ArgumentParser(description="RawrXD-AI Training Pipeline")
    parser.add_argument("--start-phase", type=str, default="1b",
        choices=PHASE_ORDER, help="Phase to start from")
    parser.add_argument("--end-phase", type=str, default="4",
        choices=PHASE_ORDER, help="Phase to end at")
    parser.add_argument("--preset", type=str, default="small",
        choices=["nano", "small", "medium", "large"],
        help="Model size preset")
    parser.add_argument("--epochs", type=int, default=3)
    parser.add_argument("--batch-size", type=int, default=4)
    parser.add_argument("--lr", type=float, default=3e-4)
    parser.add_argument("--seq-len", type=int, default=2048)
    parser.add_argument("--max-steps", type=int, default=-1,
        help="Max training steps (-1 = full epochs)")
    parser.add_argument("--quant", nargs="+", default=["f16", "q8_0", "q4_0"],
        help="GGUF quantization types")
    parser.add_argument("--skip-check", action="store_true",
        help="Skip dependency check")
    args = parser.parse_args()

    print("=" * 72)
    print("  RawrXD-AI — From-Scratch Training Pipeline")
    print(f"  Model: RawrXD-{args.preset.capitalize()}")
    print(f"  No base model — the model builds itself from digested code")
    print(f"  Start: Phase {args.start_phase} | End: Phase {args.end_phase}")
    print("=" * 72)

    # Dependency check
    if not args.skip_check:
        if not check_prerequisites():
            sys.exit(1)

    # Determine which phases to run
    start_idx = PHASE_ORDER.index(args.start_phase)
    end_idx = PHASE_ORDER.index(args.end_phase)
    phases_to_run = PHASE_ORDER[start_idx:end_idx + 1]

    pipeline_t0 = time.time()
    results = {}

    for phase_id in phases_to_run:
        extra_args = []

        # Phase-specific arguments
        if phase_id == "3c":
            extra_args = [
                "--preset", args.preset,
                "--epochs", str(args.epochs),
                "--batch-size", str(args.batch_size),
                "--lr", str(args.lr),
                "--seq-len", str(args.seq_len),
            ]
            if args.max_steps > 0:
                extra_args.extend(["--max-steps", str(args.max_steps)])

        elif phase_id == "4":
            extra_args = ["--quant"] + args.quant

        success = run_phase(phase_id, extra_args)
        results[phase_id] = success

        if not success and phase_id in ("3a", "3c"):
            print(f"\n[ABORT] Critical phase {phase_id} failed. Stopping pipeline.")
            break

    # Summary
    pipeline_elapsed = time.time() - pipeline_t0
    print(f"\n\n{'='*72}")
    print(f"  Pipeline Summary")
    print(f"{'='*72}")
    for phase_id, success in results.items():
        status = "PASS" if success else "FAIL"
        print(f"  Phase {phase_id} ({PHASE_NAMES[phase_id]}): {status}")
    print(f"\n  Total elapsed: {pipeline_elapsed / 3600:.1f} hours")
    print(f"  Output: {TRAINING_DIR}")
    print(f"{'='*72}")

    # Write pipeline status
    status_file = TRAINING_DIR / "pipeline_status.json"
    with open(status_file, "w") as f:
        json.dump({
            "completed": datetime.now().isoformat(),
            "elapsed_hours": round(pipeline_elapsed / 3600, 2),
            "model_preset": args.preset,
            "phases": {pid: "pass" if ok else "fail" for pid, ok in results.items()},
            "all_passed": all(results.values()),
        }, f, indent=2)


if __name__ == "__main__":
    main()
