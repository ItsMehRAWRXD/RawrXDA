#!/usr/bin/env python3
"""Phase 1b: E:\ corpus ingestion with D:\ deduplication"""
import os, json, hashlib, sys
from pathlib import Path
import mmap

E_DRIVE_ROOT = Path("E:/")
OUTPUT = Path("F:/RawrXD-AI-Training/raw_corpus.jsonl")
BLOCKED_EXTS = {'.exe', '.dll', '.bin', '.zip', '.tar', '.gz', '.rar', '.7z', '.jpg', '.png', '.mp4', '.mp3', '.wav', '.ico', '.bmp', '.gif', '.pdb', '.obj', '.o', '.lib', '.a', '.so', '.dylib', '.whl', '.pyc', '.pyo'}
TARGET_EXTS = {'.py', '.cpp', '.hpp', '.h', '.c', '.rs', '.go', '.js', '.ts', '.jsx', '.tsx', '.java', '.kt', '.swift', '.cs', '.vb', '.fs', '.fsx', '.fsi', '.scala', '.sc', '.r', '.m', '.mm', '.asm', '.s', '.inc', '.cmake', '.txt', '.md', '.rst', '.yaml', '.yml', '.json', '.xml', '.toml', '.ini', '.cfg', '.conf', '.sh', '.bash', '.zsh', '.ps1', '.psm1', '.bat', '.cmd'}

def get_existing_hashes():
    """Load existing content hashes from corpus for dedup"""
    hashes = set()
    if OUTPUT.exists():
        with open(OUTPUT, 'r', encoding='utf-8', errors='ignore') as f:
            for line in f:
                try:
                    data = json.loads(line)
                    if data.get('source') == 'E_DRIVE':
                        continue  # Skip prior E: entries
                    content = data.get('content', '')
                    hashes.add(hashlib.blake2b(content.encode(), digest_size=16).hexdigest())
                except: continue
    print(f"[Dedup] Loaded {len(hashes)} existing hashes")
    return hashes

def process_file(filepath, existing_hashes):
    """Process single file, return JSONL line or None if duplicate/skip"""
    fp = Path(filepath)
    if fp.suffix.lower() in BLOCKED_EXTS:
        return None
    if TARGET_EXTS and fp.suffix.lower() not in TARGET_EXTS:
        return None
    
    try:
        if fp.stat().st_size > 50_000_000:  # Skip files >50MB
            return None
        if fp.stat().st_size < 10:
            return None
        
        with open(fp, 'r', encoding='utf-8', errors='ignore') as f:
            content = f.read()
        
        if len(content) < 10:
            return None
            
        h = hashlib.blake2b(content.encode(), digest_size=16).hexdigest()
        if h in existing_hashes:
            return None
        
        existing_hashes.add(h)  # Prevent intra-E: duplicates
            
        return json.dumps({
            'file': str(fp),
            'content': content,
            'size': len(content),
            'hash': h,
            'source': 'E_DRIVE'
        }, ensure_ascii=False)
    except Exception:
        return None

def main():
    existing = get_existing_hashes()
    print(f"[Crawl] Scanning E:\\ for files...")
    files = [p for p in E_DRIVE_ROOT.rglob('*') if p.is_file()]
    print(f"[Crawl] Found {len(files)} files on E:\\")
    
    processed = 0
    scanned = 0
    with open(OUTPUT, 'a', encoding='utf-8') as out_f:
        for fp in files:
            result = process_file(fp, existing)
            if result:
                out_f.write(result + '\n')
                processed += 1
                if processed % 1000 == 0:
                    print(f"[Progress] {processed} new files ingested, {scanned}/{len(files)} scanned...")
            scanned += 1
            if scanned % 50000 == 0:
                print(f"[Scan] {scanned}/{len(files)} files scanned, {processed} ingested...")
    
    print(f"[Complete] Appended {processed} unique files to corpus from {scanned} scanned")

if __name__ == "__main__":
    main()
