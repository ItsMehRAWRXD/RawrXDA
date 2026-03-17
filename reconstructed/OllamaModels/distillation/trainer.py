py import argparse
import json
from pathlib import Path
from typing import List, Dict

import torch
from torch.utils.data import Dataset
from transformers import (
    AutoTokenizer,
    AutoModelForCausalLM,
    Trainer,
    TrainingArguments,
    default_data_collator,
)


class TeacherDataset(Dataset):
    """Simple dataset of prompt + teacher response pairs."""

    def __init__(self, records: List[Dict[str, str]], tokenizer: AutoTokenizer, max_length: int):
        self.tokenizer = tokenizer
        self.max_length = max_length
        self.samples = []
        for rec in records:
            prompt = rec.get("prompt", "").strip()
            response = rec.get("response", "").strip()
            if not prompt or not response:
                continue
            self.samples.append((prompt, response))

    def __len__(self):
        return len(self.samples)

    def __getitem__(self, idx: int) -> Dict[str, torch.Tensor]:
        prompt, response = self.samples[idx]
        prompt_ids = self.tokenizer.encode(prompt, add_special_tokens=False)
        response_ids = self.tokenizer.encode(response, add_special_tokens=False)
        eos_id = self.tokenizer.eos_token_id or self.tokenizer.sep_token_id
        sequence = prompt_ids + response_ids + ([eos_id] if eos_id is not None else [])
        if len(sequence) > self.max_length:
            # Keep the last `max_length` tokens (response-heavy) to preserve reply quality
            sequence = sequence[-self.max_length :]
            prompt_cutoff = len(prompt_ids) - (len(prompt_ids) - self.max_length)
            prompt_ids = prompt_ids[prompt_cutoff:]
        labels = [-100] * len(prompt_ids) + response_ids
        if eos_id is not None:
            labels.append(eos_id)
        if len(sequence) != len(labels):
            labels = labels[: len(sequence)]
        attention_mask = [1] * len(sequence)
        return {
            "input_ids": torch.tensor(sequence, dtype=torch.long),
            "labels": torch.tensor(labels, dtype=torch.long),
            "attention_mask": torch.tensor(attention_mask, dtype=torch.long),
        }


def load_teacher_records(path: Path) -> List[Dict[str, str]]:
    data = []
    with path.open("r", encoding="utf-8") as reader:
        for line in reader:
            line = line.strip()
            if not line:
                continue
            data.append(json.loads(line))
    return data


def main() -> None:
    parser = argparse.ArgumentParser(description="Distill a student model from teacher JSONL data.")
    parser.add_argument("--teacher-data", type=Path, required=True, help="Path to prompt/response JSONL produced by the F32 teacher.")
    parser.add_argument(
        "--student",
        type=str,
        required=True,
        help="Token or path for the student (cheetah-stealth-agentic) checkpoint or model directory.",
    )
    parser.add_argument(
        "--tokenizer",
        type=str,
        required=True,
        help="Tokenizer that matches the student model.",
    )
    parser.add_argument("--output-dir", type=Path, required=True, help="Where to save the distilled checkpoint.")
    parser.add_argument("--max-length", type=int, default=4096, help="Maximum sequence length for prompt+response pair.")
    parser.add_argument("--per-device-batch-size", type=int, default=1, help="Batch size for training.")
    parser.add_argument("--learning-rate", type=float, default=1e-5, help="Learning rate for AdamW optimizer.")
    parser.add_argument("--epochs", type=int, default=3, help="Number of epochs to train.")
    parser.add_argument("--save-steps", type=int, default=200, help="Checkpoint frequency.")
    args = parser.parse_args()

    args.output_dir.mkdir(parents=True, exist_ok=True)
    records = load_teacher_records(args.teacher_data)
    tokenizer = AutoTokenizer.from_pretrained(args.tokenizer, use_fast=False)
    model = AutoModelForCausalLM.from_pretrained(args.student, torch_dtype=torch.float16)

    dataset = TeacherDataset(records, tokenizer, args.max_length)
    training_args = TrainingArguments(
        output_dir=str(args.output_dir),
        per_device_train_batch_size=args.per_device_batch_size,
        gradient_accumulation_steps=1,
        learning_rate=args.learning_rate,
        num_train_epochs=args.epochs,
        save_steps=args.save_steps,
        save_total_limit=3,
        logging_dir=str(args.output_dir / "logs"),
        report_to="none",
        fp16=True,
        evaluation_strategy="no",
    )

    trainer = Trainer(
        model=model,
        args=training_args,
        train_dataset=dataset,
        tokenizer=tokenizer,
        data_collator=default_data_collator,
    )

    trainer.train()
    trainer.save_model(str(args.output_dir))
    tokenizer.save_pretrained(str(args.output_dir))


if __name__ == "__main__":
    main()
