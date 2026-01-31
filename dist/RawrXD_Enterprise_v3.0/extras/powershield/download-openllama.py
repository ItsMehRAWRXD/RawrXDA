#!/usr/bin/env python3
"""
Download OpenLLaMA-70B from Hugging Face
Uses snapshot_download to get the full model
"""

import os
from pathlib import Path
from huggingface_hub import snapshot_download

def main():
    print("=" * 70)
    print("OpenLLaMA-70B Downloader")
    print("=" * 70)
    
    # Configuration
    model_id = "openlm-research/open_llama_70b"
    local_dir = r"D:\Franken\OpenLLaMA-70B-HF"
    cache_dir = r"D:\Franken\cache"
    
    print(f"\n[*] Model: {model_id}")
    print(f"[*] Destination: {local_dir}")
    print(f"[*] Cache: {cache_dir}")
    print(f"\n[!] This will download ~140 GB")
    print(f"[!] Ensure you have sufficient disk space\n")
    
    # Create directories
    os.makedirs(local_dir, exist_ok=True)
    os.makedirs(cache_dir, exist_ok=True)
    
    try:
        print("[*] Starting download...")
        print("[*] This may take several hours depending on your connection\n")
        
        snapshot_download(
            repo_id=model_id,
            local_dir=local_dir,
            cache_dir=cache_dir,
            resume_download=True,  # Resume if interrupted
            local_dir_use_symlinks=False  # Copy files instead of symlinks
        )
        
        print("\n" + "=" * 70)
        print("[✓] Download complete!")
        print("=" * 70)
        print(f"\n[*] Model saved to: {local_dir}")
        
        # Verify download
        config_path = os.path.join(local_dir, "config.json")
        if os.path.exists(config_path):
            print(f"[✓] config.json found")
        else:
            print(f"[!] Warning: config.json not found")
        
        print("\n[*] Next step:")
        print(f"    python D:\\BigDaddyG-40GB-Torrent\\llama.cpp\\convert-hf-to-gguf.py {local_dir} --outfile D:\\Franken\\OpenLLaMA-70B-F16.gguf")
        
    except KeyboardInterrupt:
        print("\n\n[!] Download interrupted by user")
        print("[*] You can resume by running this script again")
        return 1
    except Exception as e:
        print(f"\n[!] Error: {e}")
        import traceback
        traceback.print_exc()
        return 1
    
    return 0

if __name__ == "__main__":
    exit(main())
