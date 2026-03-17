#!/usr/bin/env python3
"""Phase 3a: Train 32K BPE tokenizer from scratch on the full corpus"""
import json, os
from pathlib import Path
from tokenizers import Tokenizer, models, pre_tokenizers, trainers
from tokenizers.processors import TemplateProcessing

CORPUS = Path("F:/RawrXD-AI-Training/raw_corpus.jsonl")
OUTPUT = Path("F:/RawrXD-AI-Training/tokenizer")
VOCAB_SIZE = 32000

def main():
    OUTPUT.mkdir(exist_ok=True)
    
    # Extract text to temp file (tokenizers lib trains on text files, not JSONL)
    tmp_file = OUTPUT / "_corpus_text.tmp"
    print("[Tokenizer] Extracting corpus text...")
    count = 0
    with open(CORPUS, 'r', encoding='utf-8', errors='ignore') as f_in, \
         open(tmp_file, 'w', encoding='utf-8') as f_out:
        for line in f_in:
            try:
                data = json.loads(line)
                content = data.get('content', '')
                if content and len(content) > 20:
                    f_out.write(content + '\n')
                    count += 1
            except:
                continue
    print(f"[Tokenizer] Extracted {count} documents")
    
    # Initialize BPE tokenizer
    tokenizer = Tokenizer(models.BPE(unk_token="<unk>"))
    tokenizer.pre_tokenizer = pre_tokenizers.ByteLevel(add_prefix_space=False)
    
    trainer = trainers.BpeTrainer(
        vocab_size=VOCAB_SIZE,
        special_tokens=[
            "<pad>", "<unk>", "<s>", "</s>",
            "<|user|>", "<|assistant|>", "<|system|>", "<|end|>",
            "<|fim_prefix|>", "<|fim_middle|>", "<|fim_suffix|>",
            "<|tool_call|>", "<|tool_result|>",
        ],
        min_frequency=2,
        show_progress=True
    )
    
    print("[Tokenizer] Training BPE...")
    tokenizer.train([str(tmp_file)], trainer)
    
    # Add post-processor for chat format
    tokenizer.post_processor = TemplateProcessing(
        single="<s> $A </s>",
        pair="<s> $A </s> $B </s>",
        special_tokens=[
            ("<s>", tokenizer.token_to_id("<s>")),
            ("</s>", tokenizer.token_to_id("</s>")),
        ],
    )
    
    # Save
    tokenizer.save(str(OUTPUT / "tokenizer.json"))
    
    # Save config for GGUF export
    with open(OUTPUT / "config.json", 'w') as f:
        json.dump({
            "vocab_size": VOCAB_SIZE,
            "bos_token_id": tokenizer.token_to_id("<s>"),
            "eos_token_id": tokenizer.token_to_id("</s>"),
            "pad_token_id": tokenizer.token_to_id("<pad>"),
        }, f, indent=2)
    
    # Cleanup
    try:
        os.remove(tmp_file)
    except:
        pass
    
    # Test
    test_strings = [
        "PatchResult apply_memory_patch(void* addr, size_t size, const void* data);",
        "#include <windows.h>",
        "def train_model(corpus_path: str, epochs: int = 10):",
    ]
    print(f"\n[Tokenizer] Complete. Vocab size: {tokenizer.get_vocab_size()}")
    for s in test_strings:
        encoded = tokenizer.encode(s)
        print(f"  {s[:60]:60s} -> {len(encoded.ids)} tokens")

if __name__ == "__main__":
    main()
