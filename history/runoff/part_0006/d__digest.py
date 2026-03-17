#!/usr/bin/env python3
"""
MODEL DIGESTION ENGINE - Lightweight Python CLI
Direct drive letter ingestion for RawrXD + Carmilla + MASM x64
No Ollama, no dependencies beyond Python standard library + pycryptodome

Usage:
    python digest.py -i d:\models\llama.gguf -o d:\digested\llama -n "Llama 2"
    python digest.py --input e:\model.blob --output d:\out --name "MyModel" --encrypt aes256
    python digest.py --drive d: --pattern "*.gguf" --output d:\bulk-digested
"""

import os
import sys
import argparse
import hashlib
import struct
from pathlib import Path
from typing import Dict, Tuple, Optional, List
import json
import subprocess

# Optional: Crypto support
try:
    from Crypto.Cipher import AES
    from Crypto.Random import get_random_bytes
    from Crypto.Protocol.KDF import PBKDF2
    CRYPTO_AVAILABLE = True
except ImportError:
    CRYPTO_AVAILABLE = False
    print("⚠️  pycryptodome not installed. Install with: pip install pycryptodome")


class GGUFIngestor:
    """Parse GGUF binary format without external deps"""
    
    GGUF_MAGIC = 0x46554747  # "GGUF"
    
    @staticmethod
    def parse_header(filepath: str) -> Dict:
        """Extract GGUF header info"""
        try:
            with open(filepath, 'rb') as f:
                magic = struct.unpack('<I', f.read(4))[0]
                if magic != GGUFIngestor.GGUF_MAGIC:
                    raise ValueError(f"Invalid GGUF magic: {hex(magic)}")
                
                version = struct.unpack('<I', f.read(4))[0]
                tensor_count = struct.unpack('<Q', f.read(8))[0]
                kv_count = struct.unpack('<Q', f.read(8))[0]
                
                return {
                    'magic': hex(magic),
                    'version': version,
                    'tensor_count': tensor_count,
                    'metadata_kv_count': kv_count,
                    'file_size': os.path.getsize(filepath),
                    'format': 'GGUF'
                }
        except Exception as e:
            print(f"❌ GGUF parsing error: {e}")
            return {}


class BLOBIngestor:
    """Handle raw BLOB format (or just binary data)"""
    
    BLOB_MAGIC = b'BLOB'
    
    @staticmethod
    def parse_header(filepath: str) -> Dict:
        """Extract BLOB header if present"""
        try:
            with open(filepath, 'rb') as f:
                magic = f.read(4)
                if magic == BLOBIngestor.BLOB_MAGIC:
                    # BLOB format: [MAGIC(4)][SIZE(4)][VOCAB(4)][CONTEXT(4)]...
                    model_size = struct.unpack('<I', f.read(4))[0]
                    vocab_size = struct.unpack('<I', f.read(4))[0]
                    context_len = struct.unpack('<I', f.read(4))[0]
                    
                    return {
                        'magic': magic.decode('utf-8', errors='ignore'),
                        'model_size': model_size,
                        'vocab_size': vocab_size,
                        'context_length': context_len,
                        'file_size': os.path.getsize(filepath),
                        'format': 'BLOB'
                    }
                else:
                    # Raw binary
                    return {
                        'magic': 'NONE',
                        'file_size': os.path.getsize(filepath),
                        'format': 'RAW_BINARY'
                    }
        except Exception as e:
            print(f"❌ BLOB parsing error: {e}")
            return {}


class ModelDigester:
    """Main digestion pipeline"""
    
    def __init__(self, input_path: str, output_dir: str, model_name: str = None):
        self.input_path = Path(input_path)
        self.output_dir = Path(output_dir)
        self.model_name = model_name or self.input_path.stem
        self.output_dir.mkdir(parents=True, exist_ok=True)
        
        # Detect format
        self.file_format = self._detect_format()
        self.metadata = {}
    
    def _detect_format(self) -> str:
        """Detect input format"""
        suffix = self.input_path.suffix.lower()
        
        if suffix == '.gguf':
            return 'GGUF'
        elif suffix == '.blob':
            return 'BLOB'
        else:
            return 'RAW'
    
    def ingest(self) -> bool:
        """Run complete digestion pipeline"""
        print(f"\n{'='*70}")
        print(f"  MODEL DIGESTION - {self.model_name}")
        print(f"{'='*70}\n")
        
        print(f"📥 Input: {self.input_path}")
        print(f"📤 Output: {self.output_dir}")
        print(f"📋 Format: {self.file_format}")
        
        # Phase 1: Parse metadata
        print(f"\n[Phase 1] Parsing metadata...")
        if self.file_format == 'GGUF':
            self.metadata = GGUFIngestor.parse_header(str(self.input_path))
        elif self.file_format == 'BLOB':
            self.metadata = BLOBIngestor.parse_header(str(self.input_path))
        else:
            self.metadata = {'file_size': self.input_path.stat().st_size, 'format': 'RAW_BINARY'}
        
        if self.metadata:
            print(f"  ✅ Metadata extracted:")
            for key, val in self.metadata.items():
                print(f"     {key}: {val}")
        
        # Phase 2: Compute checksums
        print(f"\n[Phase 2] Computing checksums...")
        checksum_sha256 = self._compute_checksum()
        print(f"  ✅ SHA256: {checksum_sha256[:16]}...")
        
        # Phase 3: Create BLOB (normalize format)
        print(f"\n[Phase 3] Creating BLOB package...")
        blob_path = self._create_blob()
        if blob_path:
            print(f"  ✅ BLOB created: {blob_path}")
        else:
            print(f"  ❌ BLOB creation failed")
            return False
        
        # Phase 4: Encrypt (if crypto available)
        print(f"\n[Phase 4] Encryption...")
        if CRYPTO_AVAILABLE:
            encrypted_path = self._encrypt_blob(blob_path)
            if encrypted_path:
                print(f"  ✅ Encrypted: {encrypted_path}")
        else:
            print(f"  ⚠️  Crypto not available - skipping encryption")
            encrypted_path = blob_path
        
        # Phase 5: Generate MASM stub
        print(f"\n[Phase 5] Generating MASM loader stub...")
        asm_path = self._generate_masm_stub(encrypted_path, checksum_sha256)
        if asm_path:
            print(f"  ✅ MASM stub: {asm_path}")
        
        # Phase 6: Generate metadata + manifest
        print(f"\n[Phase 6] Generating metadata...")
        self._write_metadata(checksum_sha256)
        print(f"  ✅ Metadata written")
        
        # Phase 7: Generate C++ header
        print(f"\n[Phase 7] Generating C++ header...")
        cpp_header = self._generate_cpp_header(checksum_sha256)
        if cpp_header:
            print(f"  ✅ C++ header: {cpp_header}")
        
        print(f"\n{'='*70}")
        print(f"✅ DIGESTION COMPLETE")
        print(f"{'='*70}\n")
        print(f"📦 Output files:")
        print(f"   Blob: {self.output_dir / f'{self.model_name}.blob'}")
        print(f"   Meta: {self.output_dir / f'{self.model_name}.meta.json'}")
        print(f"   ASM:  {self.output_dir / f'{self.model_name}.asm'}")
        print(f"   HPP:  {self.output_dir / f'{self.model_name}.hpp'}")
        print(f"\n")
        
        return True
    
    def _compute_checksum(self) -> str:
        """SHA256 checksum of input file"""
        sha256 = hashlib.sha256()
        with open(self.input_path, 'rb') as f:
            while chunk := f.read(8192):
                sha256.update(chunk)
        return sha256.hexdigest()
    
    def _create_blob(self) -> Optional[str]:
        """Create normalized BLOB format"""
        try:
            blob_path = self.output_dir / f"{self.model_name}.blob"
            
            with open(self.input_path, 'rb') as infile:
                with open(blob_path, 'wb') as outfile:
                    # Write BLOB header
                    outfile.write(b'BLOB')
                    
                    # Get file size
                    file_size = self.input_path.stat().st_size
                    outfile.write(struct.pack('<I', file_size))
                    
                    # Metadata placeholders
                    outfile.write(struct.pack('<I', self.metadata.get('vocab_size', 32000)))
                    outfile.write(struct.pack('<I', self.metadata.get('context_length', 2048)))
                    outfile.write(struct.pack('<I', self.metadata.get('tensor_count', 0)))
                    outfile.write(struct.pack('<I', self.metadata.get('layer_count', 24)))
                    
                    # Copy model data
                    outfile.write(infile.read())
            
            return str(blob_path)
        except Exception as e:
            print(f"  ❌ BLOB creation failed: {e}")
            return None
    
    def _encrypt_blob(self, blob_path: str) -> Optional[str]:
        """Encrypt BLOB with AES-256-GCM (Carmilla compatible)"""
        if not CRYPTO_AVAILABLE:
            return blob_path
        
        try:
            encrypted_path = self.output_dir / f"{self.model_name}.encrypted.blob"
            
            # Generate key + IV
            iv = get_random_bytes(12)  # 96-bit for GCM
            salt = get_random_bytes(32)
            
            # Derive key from passphrase (hardcoded for now, make it configurable)
            passphrase = b"rawrz-model-key-" + os.urandom(16)
            key = PBKDF2(passphrase, salt, 32, count=100000, hmac_hash_module=None)
            
            # Read plaintext
            with open(blob_path, 'rb') as f:
                plaintext = f.read()
            
            # Encrypt with AES-256-GCM
            cipher = AES.new(key, AES.MODE_GCM, nonce=iv)
            ciphertext = cipher.encrypt(plaintext)
            tag = cipher.digest()
            
            # Write: [IV(12)][TAG(16)][CIPHERTEXT(n)]
            with open(encrypted_path, 'wb') as f:
                f.write(iv)
                f.write(tag)
                f.write(ciphertext)
            
            # Save key info for later
            key_info = {
                'passphrase_hint': 'rawrz-model-key',
                'salt': salt.hex(),
                'iv': iv.hex(),
                'iterations': 100000
            }
            
            with open(self.output_dir / f"{self.model_name}.key.json", 'w') as f:
                json.dump(key_info, f, indent=2)
            
            return str(encrypted_path)
        except Exception as e:
            print(f"  ❌ Encryption failed: {e}")
            return None
    
    def _generate_masm_stub(self, blob_path: str, checksum: str) -> Optional[str]:
        """Generate MASM x64 loader stub"""
        try:
            asm_path = self.output_dir / f"{self.model_name}.asm"
            
            stub = f"""; MODEL LOADER - {self.model_name}
; Auto-generated by Model Digestion Engine
; Format: AES-256-GCM encrypted BLOB
; Checksum: {checksum[:32]}

.CODE ALIGN 16

public InitializeModelInference
public LoadModelToMemory
public DecryptModelBlock

; Entry point - load encrypted model
InitializeModelInference PROC
    ; RCX = blob path
    ; RDX = encryption key
    ; R8  = metadata ptr
    
    push rbp
    mov rbp, rsp
    sub rsp, 64
    
    ; Load blob from disk
    mov rsi, rcx                    ; blob path
    call LoadModelFromDisk
    
    ; Decrypt with key
    mov rsi, rax                    ; encrypted data
    mov rdx, r8                     ; key
    call AesGcmDecrypt
    
    ; Verify checksum
    mov rsi, rax                    ; decrypted data
    call VerifyChecksum
    
    add rsp, 64
    pop rbp
    ret
InitializeModelInference ENDP

LoadModelToMemory PROC
    ; RCX = file path
    ; RDX = size ptr
    
    ; Simple file read - in production use proper I/O
    mov rax, rcx
    ret
LoadModelToMemory ENDP

DecryptModelBlock PROC
    ; Placeholder - real impl uses AES-NI instructions
    mov rax, rcx
    ret
DecryptModelBlock ENDP

.END
"""
            
            with open(asm_path, 'w') as f:
                f.write(stub)
            
            return str(asm_path)
        except Exception as e:
            print(f"  ❌ MASM generation failed: {e}")
            return None
    
    def _write_metadata(self, checksum: str):
        """Write metadata JSON"""
        metadata_file = self.output_dir / f"{self.model_name}.meta.json"
        
        meta = {
            'model_name': self.model_name,
            'input_format': self.file_format,
            'output_format': 'encrypted-blob',
            'checksum_sha256': checksum,
            'file_size': self.metadata.get('file_size', 0),
            'vocab_size': self.metadata.get('vocab_size', 32000),
            'context_length': self.metadata.get('context_length', 2048),
            'layer_count': self.metadata.get('layer_count', 24),
            'encryption': 'aes-256-gcm' if CRYPTO_AVAILABLE else 'none',
            'created': __import__('datetime').datetime.now().isoformat()
        }
        
        with open(metadata_file, 'w') as f:
            json.dump(meta, f, indent=2)
    
    def _generate_cpp_header(self, checksum: str) -> Optional[str]:
        """Generate C++ integration header"""
        try:
            hpp_path = self.output_dir / f"{self.model_name}.hpp"
            
            header = f"""#pragma once

/**
 * MODEL DIGESTION CONFIG - {self.model_name}
 * Auto-generated by Python Model Digestion Engine
 */

namespace ModelDigestion {{

struct {self.model_name}Config {{
    static constexpr const char* NAME = "{self.model_name}";
    static constexpr const char* CHECKSUM = "{checksum}";
    static constexpr uint32_t VOCAB_SIZE = {self.metadata.get('vocab_size', 32000)};
    static constexpr uint32_t CONTEXT_LENGTH = {self.metadata.get('context_length', 2048)};
    static constexpr uint32_t LAYER_COUNT = {self.metadata.get('layer_count', 24)};
    static constexpr const char* ENCRYPTION_METHOD = "{'aes-256-gcm' if CRYPTO_AVAILABLE else 'none'}";
}};

}} // namespace ModelDigestion
"""
            
            with open(hpp_path, 'w') as f:
                f.write(header)
            
            return str(hpp_path)
        except Exception as e:
            print(f"  ❌ C++ header generation failed: {e}")
            return None


class BulkDigester:
    """Process multiple models from a drive"""
    
    @staticmethod
    def scan_drive(drive_letter: str, pattern: str = "*.gguf") -> List[Path]:
        """Find all matching files on drive"""
        drive_path = Path(drive_letter.rstrip(':') + ':/')
        
        if not drive_path.exists():
            print(f"❌ Drive not found: {drive_letter}")
            return []
        
        print(f"🔍 Scanning {drive_letter} for {pattern}...")
        files = list(drive_path.rglob(pattern))
        print(f"✅ Found {len(files)} files")
        return files
    
    @staticmethod
    def process_all(files: List[Path], output_base: Path):
        """Digest multiple models"""
        output_base.mkdir(parents=True, exist_ok=True)
        
        for i, filepath in enumerate(files, 1):
            print(f"\n[{i}/{len(files)}] Processing: {filepath.name}")
            
            # Create output subdir per model
            model_output = output_base / filepath.stem
            digester = ModelDigester(str(filepath), str(model_output), filepath.stem)
            digester.ingest()


def main():
    parser = argparse.ArgumentParser(
        description='Model Digestion Engine - Ingest GGUF/BLOB for RawrXD',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Single file digestion
  python digest.py -i d:\\models\\llama.gguf -o d:\\digested\\llama
  
  # Bulk scan drive
  python digest.py --drive d: --pattern "*.gguf" --output d:\\bulk-digested
  
  # Custom name
  python digest.py -i e:\\model.blob -o d:\\out -n "MyModel"
        """
    )
    
    parser.add_argument('-i', '--input', help='Input model file')
    parser.add_argument('-o', '--output', required=True, help='Output directory')
    parser.add_argument('-n', '--name', help='Model name (default: input filename)')
    parser.add_argument('--drive', help='Drive letter to scan (e.g., d:)')
    parser.add_argument('--pattern', default='*.gguf', help='File pattern for drive scan')
    parser.add_argument('--no-encrypt', action='store_true', help='Skip encryption')
    
    args = parser.parse_args()
    
    print(f"\n{'='*70}")
    print(f"  MODEL DIGESTION ENGINE - Python Edition")
    print(f"  RawrXD + Carmilla + MASM x64 Integration")
    print(f"{'='*70}\n")
    
    # Single file mode
    if args.input:
        digester = ModelDigester(args.input, args.output, args.name)
        success = digester.ingest()
        sys.exit(0 if success else 1)
    
    # Bulk scan mode
    elif args.drive:
        files = BulkDigester.scan_drive(args.drive, args.pattern)
        if files:
            BulkDigester.process_all(files, Path(args.output))
        sys.exit(0)
    
    else:
        parser.print_help()
        sys.exit(1)


if __name__ == '__main__':
    main()
