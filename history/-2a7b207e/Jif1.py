#!/usr/bin/env python3
"""Unified pipeline launcher — runs all phases sequentially"""
import subprocess, sys, argparse, time, json
from pathlib import Path
from datetime import datetime

DATA_DIR = Path("F:/RawrXD-AI-Training")

SCRIPTS = [
    ("01_crawl_e_drive.py",          "Phase 1b: E:\\ Corpus Ingestion"),
    ("02_generate_instructions_e.py", "Phase 2b: Instruction Generation"),
    ("03a_train_tokenizer.py",        "Phase 3a: BPE Tokenizer Training"),
    ("03b_model_architecture.py",     "Phase 3b: Architecture Verify"),
    ("03c_train_from_scratch.py",     "Phase 3c: Model Training"),
    ("04_export_gguf.py",             "Phase 4:  GGUF Export"),
]

def run_script(script, desc, args):
    print(f"\n{'='*60}")
    print(f"  {desc}")
    print(f"  Script: {script}")
    print(f"{'='*60}")
    
    cmd = [sys.executable, script]
    
    if 'train_from_scratch' in script:
        cmd.extend(['--preset', args.preset, '--epochs', str(args.epochs),
                     '--batch_size', str(args.batch_size), '--lr', str(args.lr),
                     '--seq_len', str(args.seq_len)])
    elif 'export_gguf' in script:
        model_path = str(DATA_DIR / f"model_{args.preset}_final.pt")
        cmd.extend(['--preset', args.preset, '--quant', args.quant,
                     '--model_path', model_path])
    
    start = time.time()
    result = subprocess.run(cmd, cwd=str(DATA_DIR))
    elapsed = time.time() - start
    
    status = "PASS" if result.returncode == 0 else "FAIL"
    print(f"  [{status}] Completed in {elapsed/60:.1f} minutes")
    
    if result.returncode != 0:
        print(f"  [ERROR] {script} failed with exit code {result.returncode}")
        return False
    return True

def main():
    parser = argparse.ArgumentParser(description="RawrXD-AI Training Pipeline")
    parser.add_argument("--preset", default="small", choices=["nano", "small", "medium", "large"])
    parser.add_argument("--epochs", type=int, default=3)
    parser.add_argument("--batch_size", type=int, default=4)
    parser.add_argument("--lr", type=float, default=6e-4)
    parser.add_argument("--seq_len", type=int, default=2048)
    parser.add_argument("--quant", default="q8_0", choices=['f32', 'f16', 'q8_0', 'q4_0'])
    parser.add_argument("--skip-crawl", action="store_true", help="Skip E:\\ corpus crawl")
    parser.add_argument("--skip-train", action="store_true", help="Skip training (export only)")
    parser.add_argument("--start-at", type=int, default=0, help="Start at phase index (0-5)")
    args = parser.parse_args()
    
    print("=" * 60)
    print("  RawrXD-AI — From-Scratch Training Pipeline")
    print(f"  Model: RawrXD-{args.preset.capitalize()}")
    print(f"  No base model — builds itself from digested code")
    print("=" * 60)
    
    pipeline_start = time.time()
    results = {}
    
    for i, (script, desc) in enumerate(SCRIPTS):
        if i < args.start_at:
            continue
        
        # Skip flags
        if args.skip_crawl and ('crawl' in script or 'instructions_e' in script):
            print(f"\n  [SKIP] {desc}")
            continue
        if args.skip_train and 'train_from_scratch' in script:
            print(f"\n  [SKIP] {desc}")
            continue
        
        ok = run_script(script, desc, args)
        results[script] = ok
        
        if not ok and ('tokenizer' in script or 'train_from_scratch' in script):
            print(f"\n[ABORT] Critical phase failed. Stopping pipeline.")
            break
    
    elapsed = time.time() - pipeline_start
    
    print(f"\n\n{'='*60}")
    print(f"  PIPELINE SUMMARY")
    print(f"{'='*60}")
    for script, ok in results.items():
        print(f"  {'PASS' if ok else 'FAIL'} | {script}")
    print(f"\n  Total time: {elapsed/3600:.1f} hours ({elapsed/60:.0f} min)")
    print(f"  Output: {DATA_DIR / 'output' / f'RawrXD-{args.preset}-{args.quant}.gguf'}")
    print(f"{'='*60}")
    
    # Write status
    with open(DATA_DIR / "pipeline_status.json", 'w') as f:
        json.dump({
            "completed": datetime.now().isoformat(),
            "elapsed_minutes": round(elapsed / 60, 1),
            "preset": args.preset,
            "phases": {k: "pass" if v else "fail" for k, v in results.items()},
            "all_passed": all(results.values()),
        }, f, indent=2)

if __name__ == "__main__":
    main()
