#!/usr/bin/env python3
"""
RawrXD File Audit Payload Generator
Reads a large file and creates a proper JSON payload for the backend
"""

import json
import sys
import urllib.request
import urllib.error

def create_and_send_audit_payload(filepath, backend_url="http://127.0.0.1:8080/"):
    """Read file and send audit payload to backend"""
    
    try:
        # Read the file
        print(f"[+] Reading file: {filepath}")
        with open(filepath, 'r', encoding='utf-8', errors='ignore') as f:
            content = f.read()
        
        print(f"[+] File size: {len(content)} bytes")
        
        # Create payload
        payload = {
            "question": f"Audit this {filepath.split('/')[-1]} for security risks, obfuscation, shellcode, AMSI bypass, dynamic invoke, and hidden functionality. Provide JSON output with findings.",
            "model": "dolphin3:latest",
            "files": [
                {
                    "name": filepath.split('/')[-1],
                    "content": content
                }
            ]
        }
        
        json_data = json.dumps(payload)
        print(f"[+] JSON payload size: {len(json_data)} bytes")
        
        # Send to backend
        print(f"[+] Sending to backend: {backend_url}")
        req = urllib.request.Request(
            backend_url,
            data=json_data.encode('utf-8'),
            headers={'Content-Type': 'application/json'},
            method='POST'
        )
        
        with urllib.request.urlopen(req, timeout=600) as response:
            result = response.read().decode('utf-8')
            print(f"\n[✓] Response received ({len(result)} bytes):\n")
            
            # Parse and pretty-print response
            try:
                result_json = json.loads(result)
                print(json.dumps(result_json, indent=2))
            except:
                print(result[:2000])  # Print first 2000 chars if not JSON
                
    except FileNotFoundError:
        print(f"[!] File not found: {filepath}")
        sys.exit(1)
    except urllib.error.URLError as e:
        print(f"[!] Connection error: {e.reason}")
        sys.exit(1)
    except Exception as e:
        print(f"[!] Error: {e}")
        sys.exit(1)

if __name__ == "__main__":
    filepath = sys.argv[1] if len(sys.argv) > 1 else r"D:\rawrxd\RawrXD.ps1"
    backend_url = sys.argv[2] if len(sys.argv) > 2 else "http://127.0.0.1:8080/"
    create_and_send_audit_payload(filepath, backend_url)
