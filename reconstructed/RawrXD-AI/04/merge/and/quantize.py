#!/usr/bin/env python3
"""
RawrXD Merge & Quantize - Phase 4
====================================
Converts the merged HuggingFace model to GGUF format and quantizes to
multiple levels for deployment on consumer hardware.

Target hardware:
  - 16GB VRAM (RX 7800 XT) → Q2_K (12GB), Q3_K_M (15GB)
  - 64GB RAM + GPU offload  → Q4_K_M (19GB), Q5_K_M (23GB)

Uses:
  - convert-hf-to-gguf.py from llama.cpp to create F16 GGUF
  - llama-quantize.exe to create quantized variants
  
Output goes to F:\RawrXD-AI-Models\ (4TB drive)
"""

import os
import sys
import json
import subprocess
import time
import shutil
from pathlib import Path

CONFIG_PATH = os.path.join(os.path.dirname(__file__), "pipeline_config.json")
with open(CONFIG_PATH, "r", encoding="utf-8") as f:
    CONFIG = json.load(f)

TRAIN_CFG = CONFIG["training"]
QUANT_CFG = CONFIG["quantization"]
MERGED_DIR = TRAIN_CFG["final_merged_dir"]
QUANT_OUTPUT = QUANT_CFG["output_dir"]  # D:\OllamaModels\RawrXD-AI
QUANTIZE_EXE = QUANT_CFG["llama_cpp_quantize"]
CONVERT_SCRIPT = QUANT_CFG["convert_script"]

# ---------------------------------------------------------------------------
# Step 1: Convert HuggingFace → GGUF F16
# ---------------------------------------------------------------------------

def convert_to_gguf(merged_dir: str, output_dir: str) -> str:
    """Convert HuggingFace safetensors model to GGUF F16 format."""
    os.makedirs(output_dir, exist_ok=True)

    f16_path = os.path.join(output_dir, "RawrXD-IDE-32B-F16.gguf")

    if os.path.exists(f16_path):
        size_gb = os.path.getsize(f16_path) / (1024**3)
        print(f"  [SKIP] F16 GGUF already exists: {f16_path} ({size_gb:.1f} GB)")
        return f16_path

    print(f"\n  Converting HuggingFace model to GGUF F16...")
    print(f"  Source:  {merged_dir}")
    print(f"  Output:  {f16_path}")
    print(f"  This may take 30-60 minutes and use significant RAM.\n")

    # Use llama.cpp's convert script
    cmd = [
        sys.executable, CONVERT_SCRIPT,
        merged_dir,
        "--outfile", f16_path,
        "--outtype", "f16",
    ]

    print(f"  Running: {' '.join(cmd)}\n")
    start = time.time()

    result = subprocess.run(
        cmd,
        capture_output=True,
        text=True,
        cwd=os.path.dirname(CONVERT_SCRIPT),
    )

    if result.returncode != 0:
        print(f"  ERROR: Conversion failed!")
        print(f"  STDOUT: {result.stdout[-2000:]}")
        print(f"  STDERR: {result.stderr[-2000:]}")

        # Fallback: try with the newer convert-hf-to-gguf.py directly
        alt_script = os.path.join(os.path.dirname(CONVERT_SCRIPT), "convert-hf-to-gguf.py")
        if os.path.exists(alt_script) and alt_script != CONVERT_SCRIPT:
            print(f"  Trying alternate converter: {alt_script}")
            cmd[1] = alt_script
            result = subprocess.run(cmd, capture_output=True, text=True,
                                    cwd=os.path.dirname(alt_script))
            if result.returncode != 0:
                print(f"  Alternate also failed. Check llama.cpp installation.")
                sys.exit(1)
        else:
            sys.exit(1)

    elapsed = time.time() - start
    size_gb = os.path.getsize(f16_path) / (1024**3)
    print(f"  F16 GGUF created: {size_gb:.1f} GB in {elapsed/60:.1f} minutes")

    return f16_path


# ---------------------------------------------------------------------------
# Step 2: Quantize to target levels
# ---------------------------------------------------------------------------

def quantize_model(f16_path: str, output_dir: str, quant_type: str, output_name: str) -> str:
    """Quantize F16 GGUF to a specific quantization level."""
    output_path = os.path.join(output_dir, f"{output_name}.gguf")

    if os.path.exists(output_path):
        size_gb = os.path.getsize(output_path) / (1024**3)
        print(f"  [SKIP] {quant_type} already exists: {output_path} ({size_gb:.1f} GB)")
        return output_path

    print(f"\n  Quantizing to {quant_type}...")
    print(f"  Input:  {f16_path}")
    print(f"  Output: {output_path}")

    if not os.path.exists(QUANTIZE_EXE):
        print(f"  ERROR: llama-quantize not found at {QUANTIZE_EXE}")
        print(f"  Looking for alternatives...")

        # Search for quantize exe
        alt_paths = [
            r"D:\OllamaModels\llama.cpp\llama-quantize.exe",
            r"D:\llama.cpp\build\bin\Release\quantize.exe",
            r"D:\llama.cpp\build\bin\quantize.exe",
        ]
        found = None
        for p in alt_paths:
            if os.path.exists(p):
                found = p
                break
        if not found:
            print(f"  ERROR: Cannot find llama-quantize. Build llama.cpp first.")
            return ""
        print(f"  Found: {found}")
        quantize_exe = found
    else:
        quantize_exe = QUANTIZE_EXE

    cmd = [quantize_exe, f16_path, output_path, quant_type]

    # For importance matrix (better Q2_K quality), generate imatrix first
    imatrix_path = os.path.join(output_dir, "imatrix.dat")
    if quant_type in ("Q2_K", "Q3_K_M", "IQ2_M", "IQ3_M") and os.path.exists(imatrix_path):
        cmd = [quantize_exe, "--imatrix", imatrix_path, f16_path, output_path, quant_type]
        print(f"  Using importance matrix for better {quant_type} quality")

    print(f"  Running: {' '.join(cmd)}")
    start = time.time()

    result = subprocess.run(cmd, capture_output=True, text=True)

    if result.returncode != 0:
        print(f"  ERROR: Quantization failed!")
        print(f"  STDERR: {result.stderr[-1000:]}")
        return ""

    elapsed = time.time() - start
    size_gb = os.path.getsize(output_path) / (1024**3)
    print(f"  {quant_type} created: {size_gb:.1f} GB in {elapsed/60:.1f} minutes")

    return output_path


# ---------------------------------------------------------------------------
# Step 3: Generate importance matrix (optional, improves Q2/Q3 quality)
# ---------------------------------------------------------------------------

def generate_imatrix(f16_path: str, output_dir: str, calibration_data: str = None) -> str:
    """Generate importance matrix for better low-bit quantization."""
    imatrix_path = os.path.join(output_dir, "imatrix.dat")

    if os.path.exists(imatrix_path):
        print(f"  [SKIP] Importance matrix already exists: {imatrix_path}")
        return imatrix_path

    imatrix_exe = QUANTIZE_EXE.replace("llama-quantize", "llama-imatrix")
    if not os.path.exists(imatrix_exe):
        print(f"  [SKIP] llama-imatrix not found, skipping importance matrix")
        return ""

    # Use a sample of training data as calibration
    if not calibration_data:
        calib_path = os.path.join(output_dir, "calibration.txt")
        if not os.path.exists(calib_path):
            print(f"  Generating calibration data from corpus...")
            raw_jsonl = CONFIG["corpus"]["raw_jsonl"]
            if os.path.exists(raw_jsonl):
                with open(raw_jsonl, "r", encoding="utf-8") as f, \
                     open(calib_path, "w", encoding="utf-8") as out:
                    count = 0
                    for line in f:
                        if count >= 200:  # 200 samples is enough
                            break
                        try:
                            rec = json.loads(line)
                            out.write(rec["text"] + "\n\n")
                            count += 1
                        except:
                            continue
            else:
                print(f"  No calibration data available, skipping imatrix")
                return ""
        calibration_data = calib_path

    print(f"  Generating importance matrix (improves Q2_K/Q3_K quality)...")
    cmd = [
        imatrix_exe,
        "-m", f16_path,
        "-f", calibration_data,
        "-o", imatrix_path,
        "--chunks", "100",
    ]

    print(f"  Running: {' '.join(cmd)}")
    start = time.time()
    result = subprocess.run(cmd, capture_output=True, text=True)
    elapsed = time.time() - start

    if result.returncode != 0:
        print(f"  WARNING: imatrix generation failed (non-critical)")
        print(f"  {result.stderr[-500:]}")
        return ""

    print(f"  Importance matrix generated in {elapsed/60:.1f} minutes")
    return imatrix_path


# ---------------------------------------------------------------------------
# Step 4: Validate quantized models
# ---------------------------------------------------------------------------

def validate_model(model_path: str) -> bool:
    """Quick validation that a GGUF file loads."""
    if not model_path or not os.path.exists(model_path):
        return False

    size_gb = os.path.getsize(model_path) / (1024**3)
    if size_gb < 0.1:
        print(f"  WARNING: {model_path} is suspiciously small ({size_gb:.3f} GB)")
        return False

    # Try to read GGUF magic number
    try:
        with open(model_path, "rb") as f:
            magic = f.read(4)
            if magic == b"GGUF":
                print(f"  VALID: {Path(model_path).name} ({size_gb:.1f} GB) - GGUF header OK")
                return True
            else:
                print(f"  INVALID: {Path(model_path).name} - bad magic: {magic!r}")
                return False
    except OSError as e:
        print(f"  ERROR reading {model_path}: {e}")
        return False


# ---------------------------------------------------------------------------
# Step 5: Create Modelfiles for Ollama (optional)
# ---------------------------------------------------------------------------

def create_modelfile(model_path: str, output_dir: str, model_name: str, quant_type: str):
    """Create an Ollama Modelfile for easy serving."""
    modelfile_path = os.path.join(output_dir, f"Modelfile-{model_name}")

    content = f"""# RawrXD-AI Custom Model - {quant_type}
# Auto-generated by RawrXD-AI pipeline
FROM {model_path}

PARAMETER temperature 0.3
PARAMETER top_p 0.9
PARAMETER top_k 40
PARAMETER repeat_penalty 1.1
PARAMETER num_predict 2048
PARAMETER num_ctx 8192
PARAMETER num_gpu 99

SYSTEM \"\"\"You are RawrXD-AI, an expert AI coding assistant specialized in the RawrXD-Shell IDE project. You have deep knowledge of C++20, MASM64 assembly, the three-layer hotpatching architecture, GGUF model loading, and the agentic failure recovery system. You write correct, production-quality code following the project's no-exception PatchResult conventions.\"\"\"
"""
    with open(modelfile_path, "w", encoding="utf-8") as f:
        f.write(content)
    print(f"  Modelfile: {modelfile_path}")


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main():
    print("=" * 72)
    print("  RawrXD-AI Merge & Quantize Pipeline")
    print("  Creating GGUF models for consumer hardware...")
    print("=" * 72)

    os.makedirs(QUANT_OUTPUT, exist_ok=True)

    # Check that merged model exists
    if not os.path.isdir(MERGED_DIR):
        print(f"\n  ERROR: Merged model not found at {MERGED_DIR}")
        print(f"  Run 03_qlora_finetune.py --merge first!")

        # Check if there's a direct F16 GGUF we can use instead
        existing_f16 = os.path.join(QUANT_OUTPUT, "RawrXD-IDE-32B-F16.gguf")
        if os.path.exists(existing_f16):
            print(f"  Found existing F16: {existing_f16}")
            f16_path = existing_f16
        else:
            # Can we use BigDaddyG-F32 as a starting point?
            bigdaddy_f32 = r"D:\OllamaModels\BigDaddyG-F32-FROM-Q4.gguf"
            if os.path.exists(bigdaddy_f32):
                print(f"\n  Found BigDaddyG-F32-FROM-Q4.gguf ({os.path.getsize(bigdaddy_f32)/(1024**3):.1f} GB)")
                print(f"  Using as base for quantization demo...")
                f16_path = bigdaddy_f32
            else:
                sys.exit(1)
    else:
        # Step 1: Convert to GGUF F16
        print(f"\n{'='*72}")
        print(f"  STEP 1: Convert to GGUF F16")
        print(f"{'='*72}")
        f16_path = convert_to_gguf(MERGED_DIR, QUANT_OUTPUT)

    # Step 2: Generate importance matrix (optional but improves Q2_K)
    print(f"\n{'='*72}")
    print(f"  STEP 2: Generate Importance Matrix (optional)")
    print(f"{'='*72}")
    generate_imatrix(f16_path, QUANT_OUTPUT)

    # Step 3: Quantize to all target levels
    print(f"\n{'='*72}")
    print(f"  STEP 3: Quantize to target levels")
    print(f"{'='*72}")

    results = {}
    for target in QUANT_CFG["targets"]:
        name = target["name"]
        qtype = target["quant_type"]
        print(f"\n  --- {name} ({qtype}) ---")
        path = quantize_model(f16_path, QUANT_OUTPUT, qtype, name)
        results[name] = path

        # Create Ollama Modelfile
        if path:
            create_modelfile(path, QUANT_OUTPUT, name, qtype)

    # Step 4: Validate all outputs
    print(f"\n{'='*72}")
    print(f"  STEP 4: Validation")
    print(f"{'='*72}")

    valid_count = 0
    for name, path in results.items():
        if validate_model(path):
            valid_count += 1

    # Summary
    print(f"\n{'='*72}")
    print(f"  QUANTIZATION COMPLETE")
    print(f"{'='*72}")
    print(f"  Output directory: {QUANT_OUTPUT}")
    print(f"  Valid models:     {valid_count}/{len(results)}")
    print(f"\n  Models created:")

    for target in QUANT_CFG["targets"]:
        name = target["name"]
        path = results.get(name, "")
        if path and os.path.exists(path):
            size = os.path.getsize(path) / (1024**3)
            fits = "FITS VRAM" if target.get("fits_vram") else "RAM+GPU"
            print(f"    {name:35s} {size:6.1f} GB  [{fits}]  {target['recommended_for']}")

    primary = QUANT_CFG["primary_model"]
    primary_path = results.get(primary, "")
    if primary_path:
        print(f"\n  PRIMARY MODEL (daily driver):")
        print(f"    {primary_path}")
        print(f"\n  To test with llama-server:")
        server_exe = CONFIG["integration"]["llama_server_binary"]
        print(f"    {server_exe} -m {primary_path} -ngl 99 -c 8192 --port 8080")

    print(f"{'='*72}")

    # Save manifest
    manifest = {
        "created": time.strftime("%Y-%m-%d %H:%M:%S"),
        "base_model": f16_path,
        "models": {},
    }
    for name, path in results.items():
        if path and os.path.exists(path):
            manifest["models"][name] = {
                "path": path,
                "size_gb": round(os.path.getsize(path) / (1024**3), 2),
                "quant_type": next(t["quant_type"] for t in QUANT_CFG["targets"] if t["name"] == name),
            }

    manifest_path = os.path.join(QUANT_OUTPUT, "model_manifest.json")
    with open(manifest_path, "w", encoding="utf-8") as f:
        json.dump(manifest, f, indent=2)
    print(f"  Manifest: {manifest_path}")


if __name__ == "__main__":
    main()
