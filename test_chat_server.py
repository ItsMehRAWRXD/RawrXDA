#!/usr/bin/env python3
"""
Test the RawrXD-QtShell GGUF Server chat functionality
Communicates with the running Qt application via the GGUF server API
"""

import requests
import json
import sys
import time

# GGUF Server endpoint
SERVER_URL = "http://localhost:11434"

def test_model_status():
    """Check if the model is loaded"""
    try:
        response = requests.get(f"{SERVER_URL}/api/status", timeout=2)
        if response.status_code == 200:
            data = response.json()
            print(f"✓ Server is running")
            print(f"  Model loaded: {data.get('model_loaded', False)}")
            print(f"  Model path: {data.get('model_path', 'N/A')}")
            return data.get('model_loaded', False)
        else:
            print(f"✗ Server returned status {response.status_code}")
            return False
    except requests.exceptions.ConnectionError:
        print(f"✗ Cannot connect to server at {SERVER_URL}")
        print(f"  Make sure RawrXD-QtShell is running")
        return False
    except Exception as e:
        print(f"✗ Error checking status: {e}")
        return False

def test_chat_message(prompt, model_required=False):
    """Send a chat message to the inference engine"""
    print(f"\nSending chat prompt: '{prompt}'")
    
    payload = {
        "prompt": prompt,
        "max_tokens": 128,
        "temperature": 0.7,
        "top_p": 0.95
    }
    
    try:
        response = requests.post(
            f"{SERVER_URL}/api/generate",
            json=payload,
            timeout=10
        )
        
        if response.status_code == 200:
            data = response.json()
            result = data.get('response', '')
            print(f"✓ Response received:")
            print(f"  {result}")
            return True
        elif response.status_code == 503:
            print(f"✗ No model loaded on server")
            return False
        else:
            print(f"✗ Server returned status {response.status_code}")
            print(f"  Response: {response.text}")
            return False
            
    except requests.exceptions.Timeout:
        print(f"✗ Request timed out (model might be processing or not loaded)")
        return False
    except requests.exceptions.ConnectionError:
        print(f"✗ Cannot connect to server at {SERVER_URL}")
        return False
    except Exception as e:
        print(f"✗ Error sending message: {e}")
        return False

def test_tokenization():
    """Test tokenization"""
    print(f"\nTesting tokenization...")
    
    text = "Hello world"
    payload = {"text": text}
    
    try:
        response = requests.post(
            f"{SERVER_URL}/api/tokenize",
            json=payload,
            timeout=5
        )
        
        if response.status_code == 200:
            data = response.json()
            tokens = data.get('tokens', [])
            print(f"✓ Tokenization successful")
            print(f"  Text: '{text}'")
            print(f"  Token count: {len(tokens)}")
            print(f"  Tokens: {tokens[:10]}")  # Show first 10
            return True
        else:
            print(f"✗ Server returned status {response.status_code}")
            return False
            
    except Exception as e:
        print(f"✗ Error during tokenization: {e}")
        return False

def main():
    print("=" * 60)
    print("RawrXD-QtShell Chat Integration Test")
    print("=" * 60)
    
    # Test 1: Server connectivity
    print("\n[Test 1] Checking server connectivity...")
    model_loaded = test_model_status()
    
    # Test 2: Tokenization
    print("\n[Test 2] Testing tokenization...")
    test_tokenization()
    
    # Test 3: Chat message (only if model is loaded)
    if model_loaded:
        print("\n[Test 3] Testing chat message...")
        success = test_chat_message("What is machine learning?")
        if success:
            print("\n✓ Chat integration test PASSED")
        else:
            print("\n✗ Chat integration test FAILED")
    else:
        print("\n[Test 3] Skipped (model not loaded)")
        print("  Load a model in the Qt app first, then run this test")
    
    print("\n" + "=" * 60)
    print("Test Summary:")
    print(f"  Server: {'✓ Connected' if model_loaded else '✗ Not responding or no model'}")
    print("=" * 60)

if __name__ == "__main__":
    main()
