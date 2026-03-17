# RawrXD-AI — From-Scratch Training Pipeline

## No base model needed. The model builds itself as it digests both drives.

### Architecture
- **Decoder-only Transformer** (GPT-style) with RoPE, RMSNorm, GQA, SwiGLU
- **BPE tokenizer** trained from scratch on the codebase
- **GGUF export** for deployment with llama.cpp / RawrXD-Shell

### Model Sizes
| Preset   | Params | Layers | Dim  | Heads | VRAM Needed |
|----------|--------|--------|------|-------|-------------|
| nano     | ~125M  | 12     | 768  | 12    | ~2 GB       |
| small    | ~350M  | 24     | 1024 | 16    | ~4 GB       |
| medium   | ~1.3B  | 24     | 2048 | 16    | ~12 GB      |
| large    | ~3B    | 32     | 2560 | 32    | ~24 GB      |

### Data Already Produced
- `corpus/raw_corpus.jsonl` — 574 MB, 22,881 files from D:\
- `corpus/instructions.jsonl` — 1.2 GB, 68,212 instruction pairs
- `corpus/agentic_tool_use.jsonl` — 0.4 MB, 101 agentic examples

### Quick Start

```powershell
# 1. Install dependencies
pip install -r requirements.txt

# 2. For AMD GPU (optional but recommended):
pip install torch-directml

# 3. Run the full pipeline (nano preset for fast iteration):
python run_pipeline.py --preset nano --epochs 1

# 4. Run with small model (recommended):
python run_pipeline.py --preset small --epochs 3

# 5. Production run with medium model:
python run_pipeline.py --preset medium --epochs 3 --batch-size 2 --seq-len 4096
```

### Individual Phase Execution

```powershell
# Phase 1b: Crawl E:\ into corpus
python 01_crawl_e_drive.py

# Phase 2b: Generate instructions for E:\ files
python 02_generate_instructions_e.py

# Phase 3a: Train BPE tokenizer
python 03a_train_tokenizer.py

# Phase 3b: Verify model architecture (smoke test)
python 03b_model_architecture.py

# Phase 3c: Train from scratch
python 03c_train_from_scratch.py --preset small --epochs 3

# Phase 4: Export to GGUF
python 04_export_gguf.py --quant f16 q8_0 q4_0
```

### Resume Training
```powershell
python 03c_train_from_scratch.py --preset small --resume F:\RawrXD-AI-Training\checkpoints\step_5000.pt
```

### File Layout
```
F:\RawrXD-AI-Training\
├── corpus/
│   ├── raw_corpus.jsonl          (D:\ + E:\ source files)
│   ├── instructions.jsonl        (instruction pairs)
│   ├── agentic_tool_use.jsonl    (agentic training data)
│   ├── corpus_metadata.json
│   └── instructions_metadata.json
├── tokenizer/
│   ├── rawrxd_tokenizer.json     (trained BPE tokenizer)
│   └── tokenizer_metadata.json
├── checkpoints/
│   ├── best_model.pt             (best eval loss)
│   ├── final_model.pt            (end of training)
│   └── step_*.pt                 (periodic saves)
├── output/
│   ├── rawrxd-small-f16.gguf
│   ├── rawrxd-small-q8_0.gguf
│   └── rawrxd-small-q4_0.gguf
├── logs/
│   └── training_*.jsonl          (loss curves)
├── 01_crawl_e_drive.py
├── 02_generate_instructions_e.py
├── 03a_train_tokenizer.py
├── 03b_model_architecture.py
├── 03c_train_from_scratch.py
├── 04_export_gguf.py
├── run_pipeline.py               (unified launcher)
├── requirements.txt
└── README.md
```

### GPU Support
| GPU Vendor | Method | Install |
|-----------|--------|---------|
| NVIDIA    | CUDA   | `pip install torch` (default) |
| AMD       | DirectML | `pip install torch-directml` |
| CPU       | Fallback | No extra install needed |

The training loop auto-detects your GPU. For AMD on Windows, DirectML works
natively without WSL2 or ROCm.
