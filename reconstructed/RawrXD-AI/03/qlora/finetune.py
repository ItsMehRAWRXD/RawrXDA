#!/usr/bin/env python3
"""
RawrXD QLoRA Fine-Tuning - Phase 3
=====================================
Fine-tunes Qwen2.5-Coder-32B-Instruct on the RawrXD instruction dataset using QLoRA.

Hardware target:
  - AMD Radeon RX 7800 XT (16GB VRAM) via ROCm or CPU fallback
  - 64GB DDR5 RAM
  - AMD Ryzen 7 7800X3D

Strategy:
  - 4-bit quantized base model (NF4) = ~8-9GB VRAM for 32B model
  - LoRA rank 128, alpha 256 = ~400-600MB trainable params
  - Targets: q/k/v/o/gate/up/down_proj + embed_tokens + lm_head
  - Gradient checkpointing = halves activation memory
  - Paged AdamW 8-bit = minimal optimizer VRAM
  - 8192 context window, gradient_accumulation=8, lr=2e-4
  pip install torch transformers peft bitsandbytes datasets accelerate trl
  
  For AMD GPU (ROCm):
    pip install torch --index-url https://download.pytorch.org/whl/rocm6.0
    
  For CPU-only fallback (slower but works):
    pip install torch (default)
"""

import os
import sys
import json
import time
import logging
from pathlib import Path

# ---------------------------------------------------------------------------
# Configuration
# ---------------------------------------------------------------------------

CONFIG_PATH = os.path.join(os.path.dirname(__file__), "pipeline_config.json")
with open(CONFIG_PATH, "r", encoding="utf-8") as f:
    CONFIG = json.load(f)

TRAIN_CFG = CONFIG["training"]
CORPUS_CFG = CONFIG["corpus"]
BASE_CFG = CONFIG["base_model"]

INSTRUCTIONS_JSONL = CORPUS_CFG["instructions_jsonl"]
OUTPUT_DIR = TRAIN_CFG["output_dir"]
MERGED_DIR = TRAIN_CFG["final_merged_dir"]
BASE_MODEL = BASE_CFG["primary_choice"]
DOWNLOAD_DIR = BASE_CFG["download_dir"]

# LoRA config
QLORA = TRAIN_CFG["qlora_config"]
TRAIN_ARGS = TRAIN_CFG["training_args"]

logging.basicConfig(level=logging.INFO, format="%(asctime)s [%(levelname)s] %(message)s")
logger = logging.getLogger("RawrXD-Train")

# ---------------------------------------------------------------------------
# Device Detection
# ---------------------------------------------------------------------------

def detect_device():
    """Detect best available compute device."""
    import torch

    if torch.cuda.is_available():
        gpu_name = torch.cuda.get_device_name(0)
        vram = torch.cuda.get_device_properties(0).total_mem / (1024**3)
        logger.info(f"GPU detected: {gpu_name} ({vram:.1f} GB VRAM)")

        # Check if ROCm (AMD)
        if hasattr(torch.version, 'hip') and torch.version.hip is not None:
            logger.info(f"ROCm version: {torch.version.hip}")
            return "rocm"
        return "cuda"

    logger.warning("No GPU detected. CPU training will be VERY slow but functional.")
    logger.info("For AMD GPU support, install PyTorch with ROCm:")
    logger.info("  pip install torch --index-url https://download.pytorch.org/whl/rocm6.0")
    return "cpu"


# ---------------------------------------------------------------------------
# Dataset Loading
# ---------------------------------------------------------------------------

def load_instruction_dataset(path: str):
    """Load ChatML-formatted JSONL into HuggingFace Dataset."""
    from datasets import Dataset

    records = []
    with open(path, "r", encoding="utf-8") as f:
        for line in f:
            try:
                record = json.loads(line)
                convos = record.get("conversations", [])
                if len(convos) >= 2:
                    records.append(record)
            except json.JSONDecodeError:
                continue

    logger.info(f"Loaded {len(records):,} instruction examples")

    # Convert to text format for training (Qwen2.5 ChatML format)
    formatted = []
    for rec in records:
        convos = rec["conversations"]
        text_parts = []
        for msg in convos:
            role = msg["role"]
            content = msg["content"]
            text_parts.append(f"<|im_start|>{role}\n{content}<|im_end|>")
        text_parts.append("<|im_start|>assistant\n")  # Generation prompt
        formatted.append({"text": "\n".join(text_parts)})

    dataset = Dataset.from_list(formatted)

    # Split 95/5 train/eval
    split = dataset.train_test_split(test_size=0.05, seed=42)
    logger.info(f"Train: {len(split['train']):,} | Eval: {len(split['test']):,}")
    return split


# ---------------------------------------------------------------------------
# Model Loading with QLoRA
# ---------------------------------------------------------------------------

def load_model_qlora(model_name: str, device_type: str):
    """Load base model in 4-bit with LoRA adapters."""
    import torch
    from transformers import AutoModelForCausalLM, AutoTokenizer, BitsAndBytesConfig
    from peft import LoraConfig, get_peft_model, prepare_model_for_kbit_training

    logger.info(f"Loading base model: {model_name}")
    logger.info(f"Download cache: {DOWNLOAD_DIR}")

    os.makedirs(DOWNLOAD_DIR, exist_ok=True)

    # Tokenizer
    tokenizer = AutoTokenizer.from_pretrained(
        model_name,
        cache_dir=DOWNLOAD_DIR,
        trust_remote_code=True,
    )
    if tokenizer.pad_token is None:
        tokenizer.pad_token = tokenizer.eos_token
        tokenizer.pad_token_id = tokenizer.eos_token_id

    # Quantization config
    if device_type in ("cuda", "rocm"):
        bnb_config = BitsAndBytesConfig(
            load_in_4bit=True,
            bnb_4bit_quant_type="nf4",
            bnb_4bit_compute_dtype=torch.bfloat16,
            bnb_4bit_use_double_quant=True,
        )
        model = AutoModelForCausalLM.from_pretrained(
            model_name,
            quantization_config=bnb_config,
            device_map="auto",
            cache_dir=DOWNLOAD_DIR,
            trust_remote_code=True,
            torch_dtype=torch.bfloat16,
        )
    else:
        # CPU fallback - load in fp32, no quantization
        logger.warning("CPU mode: loading in fp32 (slow but works)")
        model = AutoModelForCausalLM.from_pretrained(
            model_name,
            device_map="cpu",
            cache_dir=DOWNLOAD_DIR,
            trust_remote_code=True,
            torch_dtype=torch.float32,
        )

    model = prepare_model_for_kbit_training(model)

    # LoRA configuration
    # For embed_tokens and lm_head, we use modules_to_save instead of LoRA
    # to train them fully (more effective for vocabulary adaptation)
    lora_target_modules = [m for m in QLORA["target_modules"] 
                           if m not in ("embed_tokens", "lm_head")]
    modules_to_save = [m for m in QLORA["target_modules"]
                       if m in ("embed_tokens", "lm_head")]
    
    lora_config = LoraConfig(
        r=QLORA["lora_r"],
        lora_alpha=QLORA["lora_alpha"],
        lora_dropout=QLORA["lora_dropout"],
        target_modules=lora_target_modules,
        modules_to_save=modules_to_save if modules_to_save else None,
        task_type=QLORA["task_type"],
        bias="none",
    )

    model = get_peft_model(model, lora_config)

    trainable = sum(p.numel() for p in model.parameters() if p.requires_grad)
    total = sum(p.numel() for p in model.parameters())
    pct = 100 * trainable / total

    logger.info(f"Total parameters:     {total:,}")
    logger.info(f"Trainable parameters: {trainable:,} ({pct:.2f}%)")

    return model, tokenizer


# ---------------------------------------------------------------------------
# Training
# ---------------------------------------------------------------------------

def train(model, tokenizer, dataset, device_type: str):
    """Run QLoRA fine-tuning with SFTTrainer."""
    from transformers import TrainingArguments
    from trl import SFTTrainer

    os.makedirs(OUTPUT_DIR, exist_ok=True)

    training_args = TrainingArguments(
        output_dir=OUTPUT_DIR,
        num_train_epochs=TRAIN_ARGS["num_train_epochs"],
        per_device_train_batch_size=TRAIN_ARGS["per_device_train_batch_size"],
        per_device_eval_batch_size=1,
        gradient_accumulation_steps=TRAIN_ARGS["gradient_accumulation_steps"],
        learning_rate=TRAIN_ARGS["learning_rate"],
        warmup_ratio=TRAIN_ARGS["warmup_ratio"],
        lr_scheduler_type=TRAIN_ARGS["lr_scheduler_type"],
        fp16=TRAIN_ARGS["fp16"],
        bf16=TRAIN_ARGS["bf16"],
        gradient_checkpointing=TRAIN_ARGS["gradient_checkpointing"],
        optim=TRAIN_ARGS["optim"],
        logging_steps=TRAIN_ARGS["logging_steps"],
        save_steps=TRAIN_ARGS["save_steps"],
        save_total_limit=TRAIN_ARGS["save_total_limit"],
        eval_strategy="steps",
        eval_steps=TRAIN_ARGS["save_steps"],
        dataloader_num_workers=TRAIN_ARGS["dataloader_num_workers"],
        group_by_length=TRAIN_ARGS["group_by_length"],
        report_to="none",
        remove_unused_columns=False,
        max_grad_norm=0.3,
        weight_decay=0.01,
    )

    trainer = SFTTrainer(
        model=model,
        tokenizer=tokenizer,
        train_dataset=dataset["train"],
        eval_dataset=dataset["test"],
        args=training_args,
        max_seq_length=TRAIN_ARGS["max_seq_length"],
    )

    logger.info("=" * 60)
    logger.info("  Starting QLoRA fine-tuning...")
    logger.info(f"  Epochs: {TRAIN_ARGS['num_train_epochs']}")
    logger.info(f"  Effective batch: {TRAIN_ARGS['per_device_train_batch_size'] * TRAIN_ARGS['gradient_accumulation_steps']}")
    logger.info(f"  Learning rate: {TRAIN_ARGS['learning_rate']}")
    logger.info(f"  Max seq length: {TRAIN_ARGS['max_seq_length']}")
    logger.info("=" * 60)

    start = time.time()
    train_result = trainer.train()
    elapsed = time.time() - start

    logger.info(f"Training complete in {elapsed/3600:.1f} hours")
    logger.info(f"Final loss: {train_result.training_loss:.4f}")

    # Save final adapter
    adapter_dir = os.path.join(OUTPUT_DIR, "final_adapter")
    model.save_pretrained(adapter_dir)
    tokenizer.save_pretrained(adapter_dir)
    logger.info(f"Adapter saved to: {adapter_dir}")

    return adapter_dir


# ---------------------------------------------------------------------------
# Merge LoRA into base model
# ---------------------------------------------------------------------------

def merge_adapter(base_model_name: str, adapter_dir: str):
    """Merge LoRA adapter back into base model for GGUF conversion."""
    import torch
    from transformers import AutoModelForCausalLM, AutoTokenizer
    from peft import PeftModel

    logger.info("Merging LoRA adapter into base model...")

    os.makedirs(MERGED_DIR, exist_ok=True)

    # Load base model in fp16
    base_model = AutoModelForCausalLM.from_pretrained(
        base_model_name,
        cache_dir=DOWNLOAD_DIR,
        trust_remote_code=True,
        torch_dtype=torch.float16,
        device_map="cpu",  # Merge on CPU to avoid VRAM issues
        low_cpu_mem_usage=True,
    )

    tokenizer = AutoTokenizer.from_pretrained(adapter_dir)

    # Load and merge adapter
    model = PeftModel.from_pretrained(base_model, adapter_dir)
    model = model.merge_and_unload()

    # Save merged model
    model.save_pretrained(MERGED_DIR, safe_serialization=True)
    tokenizer.save_pretrained(MERGED_DIR)

    logger.info(f"Merged model saved to: {MERGED_DIR}")
    return MERGED_DIR


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main():
    print("=" * 72)
    print("  RawrXD-AI QLoRA Fine-Tuning Pipeline")
    print("  Building your custom IDE assistant...")
    print("=" * 72)

    # Validate input
    if not os.path.exists(INSTRUCTIONS_JSONL):
        logger.error(f"Instructions file not found: {INSTRUCTIONS_JSONL}")
        logger.error("Run 01_extract_corpus.py and 02_format_instructions.py first!")
        sys.exit(1)

    # Detect hardware
    device = detect_device()

    # Load dataset
    logger.info("Loading instruction dataset...")
    dataset = load_instruction_dataset(INSTRUCTIONS_JSONL)

    # Load model with QLoRA
    model, tokenizer = load_model_qlora(BASE_MODEL, device)

    # Train
    adapter_dir = train(model, tokenizer, dataset, device)

    # Ask whether to merge now
    logger.info("")
    logger.info("=" * 60)
    logger.info("  Training complete! Next steps:")
    logger.info(f"  1. Adapter saved at: {adapter_dir}")
    logger.info(f"  2. Run 04_merge_and_quantize.py to create GGUF")
    logger.info("=" * 60)

    # Auto-merge if running as full pipeline
    if "--merge" in sys.argv:
        merge_adapter(BASE_MODEL, adapter_dir)


if __name__ == "__main__":
    main()
