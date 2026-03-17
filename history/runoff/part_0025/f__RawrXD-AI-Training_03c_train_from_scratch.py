#!/usr/bin/env python3
"""Phase 3c: Training loop with DirectML/CUDA/CPU auto-detect"""
import os, json, math, time, argparse, sys
from pathlib import Path
import torch
import torch.nn.functional as F
from torch.utils.data import Dataset, DataLoader

sys.path.insert(0, str(Path(__file__).parent))
from model_architecture import RawrXDTransformer, RawrXDConfig
from tokenizers import Tokenizer

# Auto-detect device
def get_device():
    if torch.cuda.is_available():
        name = torch.cuda.get_device_name(0)
        mem = torch.cuda.get_device_properties(0).total_mem / (1024**3)
        print(f"[Device] CUDA: {name} ({mem:.1f} GB)")
        return torch.device("cuda")
    try:
        import torch_directml
        print(f"[Device] DirectML (AMD GPU)")
        return torch_directml.device()
    except ImportError:
        pass
    print(f"[Device] CPU (install torch-directml for AMD GPU acceleration)")
    return torch.device("cpu")

DATA_DIR = Path("F:/RawrXD-AI-Training")
TOKENIZER_PATH = DATA_DIR / "tokenizer" / "tokenizer.json"
CORPUS = DATA_DIR / "raw_corpus.jsonl"
INSTRUCTIONS = [DATA_DIR / "instructions.jsonl", DATA_DIR / "instructions_e.jsonl"]

class RawrXDDataset(Dataset):
    def __init__(self, files, tokenizer, max_length=2048):
        self.tokenizer = tokenizer
        self.max_length = max_length
        self.samples = []
        
        for f in files:
            if not f.exists():
                print(f"  [Skip] {f} not found")
                continue
            count = 0
            with open(f, 'r', encoding='utf-8', errors='ignore') as fp:
                for line in fp:
                    try:
                        data = json.loads(line)
                        if 'instruction' in data:
                            text = f"<|user|>{data['instruction']}<|assistant|>{data['output']}<|end|>"
                        elif 'content' in data:
                            text = data['content']
                        else:
                            continue
                        self.samples.append(text)
                        count += 1
                    except: continue
            print(f"  [Data] {f.name}: {count} samples")
        print(f"  [Data] Total: {len(self.samples)} samples")
    
    def __len__(self): return len(self.samples)
    
    def __getitem__(self, idx):
        text = self.samples[idx]
        toks = self.tokenizer.encode(text).ids[:self.max_length]
        if len(toks) < self.max_length:
            toks += [0] * (self.max_length - len(toks))  # pad
        
        x = torch.tensor(toks[:-1], dtype=torch.long)
        y = torch.tensor(toks[1:], dtype=torch.long)
        return x, y

def train(args):
    device = get_device()
    is_cuda = (str(device) == 'cuda')
    
    # Load tokenizer
    if not TOKENIZER_PATH.exists():
        print(f"[Error] Tokenizer not found: {TOKENIZER_PATH}")
        print("        Run 03a_train_tokenizer.py first!")
        sys.exit(1)
    tokenizer = Tokenizer.from_file(str(TOKENIZER_PATH))
    
    # Model
    config_map = {
        'nano': RawrXDConfig.nano(),
        'small': RawrXDConfig.small(),
        'medium': RawrXDConfig.medium(),
        'large': RawrXDConfig.large()
    }
    config = config_map[args.preset]
    config.vocab_size = tokenizer.get_vocab_size()
    model = RawrXDTransformer(config).to(device)
    
    param_count = sum(p.numel() for p in model.parameters())
    print(f"[Model] RawrXD-{args.preset}: {param_count/1e6:.1f}M parameters")
    
    # Dataset
    dataset = RawrXDDataset([CORPUS] + INSTRUCTIONS, tokenizer, args.seq_len)
    loader = DataLoader(dataset, batch_size=args.batch_size, shuffle=True,
                       num_workers=0, pin_memory=is_cuda, drop_last=True)
    
    # Optimizer (AdamW with weight decay)
    optimizer = torch.optim.AdamW(model.parameters(), lr=args.lr,
                                  betas=(0.9, 0.95), weight_decay=0.1)
    scheduler = torch.optim.lr_scheduler.CosineAnnealingLR(
        optimizer, T_max=args.epochs * len(loader))
    
    # Mixed precision (CUDA only — DirectML doesn't support torch.cuda.amp)
    use_amp = is_cuda
    scaler = torch.amp.GradScaler("cuda") if use_amp else None
    
    # Checkpoint dir
    ckpt_dir = DATA_DIR / "checkpoints"
    ckpt_dir.mkdir(exist_ok=True)
    
    print(f"\n{'='*60}")
    print(f"Training: {args.epochs} epochs, batch={args.batch_size}, lr={args.lr}")
    print(f"Steps/epoch: {len(loader)}, Total steps: {args.epochs * len(loader)}")
    print(f"{'='*60}\n")
    
    model.train()
    step = 0
    for epoch in range(args.epochs):
        epoch_loss = 0
        start_time = time.time()
        
        for batch_idx, (x, y) in enumerate(loader):
            x, y = x.to(device), y.to(device)
            
            optimizer.zero_grad()
            
            if use_amp:
                with torch.amp.autocast("cuda"):
                    logits = model(x)
                    loss = F.cross_entropy(logits.view(-1, logits.size(-1)), y.view(-1),
                                          ignore_index=0)  # ignore pad
                scaler.scale(loss).backward()
                scaler.unscale_(optimizer)
                torch.nn.utils.clip_grad_norm_(model.parameters(), 1.0)
                scaler.step(optimizer)
                scaler.update()
            else:
                logits = model(x)
                loss = F.cross_entropy(logits.view(-1, logits.size(-1)), y.view(-1),
                                      ignore_index=0)  # ignore pad
                loss.backward()
                torch.nn.utils.clip_grad_norm_(model.parameters(), 1.0)
                optimizer.step()
            
            scheduler.step()
            epoch_loss += loss.item()
            step += 1
            
            if batch_idx % 100 == 0:
                lr_now = scheduler.get_last_lr()[0]
                print(f"  Epoch {epoch+1}/{args.epochs} | "
                      f"Batch {batch_idx}/{len(loader)} | "
                      f"Loss: {loss.item():.4f} | "
                      f"LR: {lr_now:.2e}")
            
            if step % 1000 == 0:
                ckpt_path = ckpt_dir / f"{args.preset}_step{step}.pt"
                torch.save({
                    'step': step,
                    'epoch': epoch,
                    'model_state_dict': model.state_dict(),
                    'optimizer_state_dict': optimizer.state_dict(),
                    'scheduler_state_dict': scheduler.state_dict(),
                    'loss': loss.item(),
                    'config': vars(config),
                }, ckpt_path)
                size_mb = ckpt_path.stat().st_size / (1024*1024)
                print(f"  [Save] {ckpt_path.name} ({size_mb:.1f} MB)")
        
        avg_loss = epoch_loss / len(loader)
        elapsed = time.time() - start_time
        print(f"\nEpoch {epoch+1} complete. Avg Loss: {avg_loss:.4f} | Time: {elapsed:.1f}s")
        
        # Save epoch checkpoint
        torch.save(model.state_dict(), ckpt_dir / f"{args.preset}_epoch{epoch+1}.pt")
    
    # Final save
    final_path = DATA_DIR / f"model_{args.preset}_final.pt"
    torch.save(model.state_dict(), final_path)
    print(f"\n[Complete] Final model saved: {final_path}")
    print(f"[Complete] Size: {final_path.stat().st_size / (1024**3):.2f} GB")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="RawrXD From-Scratch Training")
    parser.add_argument("--preset", default="small", choices=["nano", "small", "medium", "large"])
    parser.add_argument("--epochs", type=int, default=3)
    parser.add_argument("--batch_size", type=int, default=4)
    parser.add_argument("--lr", type=float, default=6e-4)
    parser.add_argument("--seq_len", type=int, default=2048)
    parser.add_argument("--resume", type=str, default=None, help="Path to checkpoint to resume from")
    args = parser.parse_args()
    train(args)
