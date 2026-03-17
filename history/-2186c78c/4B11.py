#!/usr/bin/env python3
"""
Direct test of InferenceEngine via curl to the GGUF server
Uses simple HTTP requests to test the API
"""

import subprocess
import json
import time
import sys

def test_health():
    """Test if server is healthy"""
    print("[Test] Checking server health...")
    try:
        result = subprocess.run([
            'curl', '-s', '-w', '\n%{http_code}',
            'http://localhost:11434/health'
        ], capture_output=True, text=True, timeout=5)
        
        lines = result.stdout.strip().split('\n')
        if len(lines) >= 2:
            http_code = lines[-1]
            body = '\n'.join(lines[:-1])
            
            if http_code == '200':
                print(f"✓ Server is healthy (HTTP {http_code})")
                return True
            else:
                print(f"✗ Server returned HTTP {http_code}")
                if body:
                    print(f"  Response: {body[:100]}")
                return False
        else:
            print(f"✗ No response from server")
            return False
    except Exception as e:
        print(f"✗ Error: {e}")
        return False

def test_tags():
    """Test model tags endpoint"""
    print("\n[Test] Checking available models...")
    try:
        result = subprocess.run([
            'curl', '-s',
            'http://localhost:11434/api/tags'
        ], capture_output=True, text=True, timeout=5)
        
        if result.stdout:
            try:
                data = json.loads(result.stdout)
                models = data.get('models', [])
                if models:
                    print(f"✓ Found {len(models)} model(s)")
                    for model in models[:3]:
                        print(f"  - {model.get('name', 'Unknown')}")
                    return True
                else:
                    print(f"✗ No models loaded")
                    return False
            except json.JSONDecodeError:
                print(f"✗ Invalid JSON response")
                return False
        else:
            print(f"✗ No response")
            return False
    except Exception as e:
        print(f"✗ Error: {e}")
        return False

def test_generate():
    """Test generate endpoint"""
    print("\n[Test] Testing inference with simple prompt...")
    
    payload = {
        "model": "local",
        "prompt": "Hello, how are you?",
        "stream": False,
        "temperature": 0.7,
        "top_p": 0.95
    }
    
    try:
        result = subprocess.run([
            'curl', '-s', '-X', 'POST',
            '-H', 'Content-Type: application/json',
            '-d', json.dumps(payload),
            'http://localhost:11434/api/generate'
        ], capture_output=True, text=True, timeout=15)
        
        if result.stdout:
            try:
                data = json.loads(result.stdout)
                response = data.get('response', '')
                if response:
                    print(f"✓ Got response from model:")
                    print(f"  {response[:100]}...")
                    return True
                else:
                    print(f"✗ Empty response from model")
                    print(f"  Full response: {json.dumps(data, indent=2)[:200]}")
                    return False
            except json.JSONDecodeError as e:
                print(f"✗ Invalid JSON response: {e}")
                print(f"  Response: {result.stdout[:200]}")
                return False
        else:
            print(f"✗ No response from server")
            if result.stderr:
                print(f"  Error: {result.stderr}")
            return False
    except subprocess.TimeoutExpired:
        print(f"✗ Request timed out (model might be generating)")
        return False
    except Exception as e:
        print(f"✗ Error: {e}")
        return False

def main():
    print("=" * 70)
    print("RawrXD GGUF Server Chat Integration Test")
    print("=" * 70)
    
    # Test 1: Health check
    if not test_health():
        print("\n" + "=" * 70)
        print("ERROR: Server not responding!")
        print("Make sure RawrXD-QtShell is running with a model loaded")
        print("=" * 70)
        return False
    
    # Test 2: List models
    has_models = test_tags()
    
    # Test 3: Generate response
    if has_models:
        test_generate()
    
    print("\n" + "=" * 70)
    print("Test Complete")
    print("=" * 70)
    return True

if __name__ == "__main__":
    success = main()
    sys.exit(0 if success else 1)
