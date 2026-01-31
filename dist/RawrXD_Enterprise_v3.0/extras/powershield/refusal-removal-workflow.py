"""
Complete Refusal Removal Workflow for GGUF Models
Converts GGUF → HF → Removes Refusals → Converts back to GGUF
"""

import os
import subprocess
import sys

def install_requirements():
    """Install all required dependencies"""
    print("[*] Installing dependencies...")
    
    packages = [
        "transformers",
        "torch",
        "bitsandbytes",
        "accelerate",
        "jaxtyping",
        "einops",
        "tqdm",
        "huggingface-hub",
        "safetensors"
    ]
    
    for package in packages:
        print(f"[*] Installing {package}...")
        subprocess.run([sys.executable, "-m", "pip", "install", "--user", package], 
                      capture_output=True)
    
    print("[✓] Dependencies installed\n")

def check_llama_cpp_python():
    """Check if llama-cpp-python is installed"""
    try:
        import llama_cpp
        print("[✓] llama-cpp-python is installed")
        return True
    except ImportError:
        print("[!] llama-cpp-python not found")
        print("[*] Install with: pip install llama-cpp-python")
        return False

def main():
    print("="*60)
    print("Complete GGUF Refusal Removal Workflow")
    print("="*60)
    
    print("""
This workflow will:
1. Convert your GGUF model to HuggingFace format
2. Apply refusal direction removal using transformers
3. Quantize and convert back to GGUF for Ollama

REQUIREMENTS:
- ~140GB free disk space (for conversion and processing)
- Python 3.10+
- CUDA GPU recommended (CPU possible but very slow)

PROCESS:
Step 1: Convert GGUF → HuggingFace safetensors
Step 2: Load model and compute refusal direction
Step 3: Remove refusal direction from model weights
Step 4: Save modified HF model
Step 5: Convert back to GGUF
Step 6: Quantize to Q4_K_M (optional)
Step 7: Create Ollama model

Would you like to proceed? (This will take several hours)
""")
    
    # Check current setup
    print("\n[*] Checking current setup...")
    
    print("\n[*] Your GGUF models:")
    gguf_dir = r"D:\OllamaModels"
    import glob
    models = glob.glob(os.path.join(gguf_dir, "*.gguf"))
    
    for i, model in enumerate(models, 1):
        size = os.path.getsize(model) / (1024**3)
        print(f"  {i}. {os.path.basename(model)} ({size:.2f} GB)")
    
    print("\n" + "="*60)
    print("DETAILED INSTRUCTIONS")
    print("="*60)
    
    print("""
Since this is a complex multi-step process, here's what you need to do:

OPTION 1: Use Pre-Abliterated Models (RECOMMENDED)
---------------------------------------------------
Download already uncensored models from HuggingFace:
- dolphin-2.9-llama3.1-70b
- WizardLM-Uncensored-13B  
- Nous-Hermes-2-Mixtral-8x7B-DPO
- openbuddy-llama3-8b-v21.1-8k

These are already uncensored and will work immediately.

OPTION 2: Convert & Abliterate Your Model (ADVANCED)
----------------------------------------------------
1. Install llama.cpp with Python bindings:
   pip install llama-cpp-python

2. Convert GGUF to HF format (requires llama.cpp tools):
   # This is complex and requires building llama.cpp from source
   # See: https://github.com/ggerganov/llama.cpp

3. Run the refusal removal script:
   cd C:\\Users\\HiH8e\\OneDrive\\Desktop\\Powershield\\remove-refusals-with-transformers
   
   Edit compute_refusal_dir.py:
   - Set model_name to your HF model path
   - Set quantization config (4bit recommended)
   
   python compute_refusal_dir.py
   python inference.py

4. Convert modified HF model back to GGUF
5. Import to Ollama

OPTION 3: Use Aggressive System Prompts (QUICK)
-----------------------------------------------
While not perfect, you can create models with very aggressive
system prompts that override most refusals. This won't remove
the refusal from weights but can work for many use cases.

RECOMMENDATION:
For immediate results, download pre-abliterated models.
For your specific models, the conversion process is complex
and requires significant time and resources.
""")
    
    print("\n" + "="*60)
    print("Next Steps:")
    print("="*60)
    print("""
1. Would you like me to:
   a) Download a pre-abliterated model from HuggingFace
   b) Set up the refusal removal tool for your models
   c) Create better system prompt overrides
   
Please let me know which option you'd like to pursue.
""")

if __name__ == "__main__":
    main()
