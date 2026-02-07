#!/usr/bin/env python3
"""
Headless test server - verifies GGUF server API without running GUI
Tests basic server health and request handling
"""

import subprocess
import time
import socket
import sys
import os
import json

def check_port_available(port):
    """Check if port is listening"""
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(1)
        result = sock.connect_ex(('127.0.0.1', port))
        sock.close()
        return result == 0
    except:
        return False

def test_health_endpoint():
    """Test /health endpoint"""
    try:
        import urllib.request
        response = urllib.request.urlopen('http://localhost:11434/health', timeout=2)
        data = response.read()
        print(f"✓ Health endpoint responsive: {data.decode()[:50]}")
        return True
    except Exception as e:
        print(f"✗ Health endpoint failed: {e}")
        return False

def test_api_generate():
    """Test /api/generate endpoint"""
    try:
        import urllib.request
        import json
        
        payload = json.dumps({
            "prompt": "Hello world",
            "stream": False
        }).encode('utf-8')
        
        req = urllib.request.Request(
            'http://localhost:11434/api/generate',
            data=payload,
            headers={'Content-Type': 'application/json'},
            method='POST'
        )
        
        response = urllib.request.urlopen(req, timeout=5)
        data = json.loads(response.read().decode())
        print(f"✓ API /generate endpoint working")
        return True
    except urllib.error.HTTPError as e:
        if e.code == 404:
            print(f"✗ API /generate returned 404 - endpoint not implemented")
            return False
        else:
            print(f"✗ API /generate failed with HTTP {e.code}")
            return False
    except Exception as e:
        print(f"✗ API /generate failed: {e}")
        return False

def main():
    print("=" * 60)
    print("RawrXD Server Headless Test")
    print("=" * 60)
    
    # Give app time to start (app is already running in background)
    print("\n[1] Checking if server is listening...")
    
    for attempt in range(10):
        if check_port_available(11434):
            print(f"✓ Server listening on port 11434")
            break
        print(f"  Attempt {attempt+1}/10: Waiting for server...")
        time.sleep(1)
    else:
        print("✗ Server not listening after 10 seconds")
        return False
    
    print("\n[2] Testing API endpoints...")
    
    results = {
        "health": test_health_endpoint(),
        "generate": test_api_generate(),
    }
    
    print("\n" + "=" * 60)
    print("Test Results:")
    print("=" * 60)
    for test_name, result in results.items():
        status = "PASS" if result else "FAIL"
        print(f"{test_name:20} {status}")
    
    if all(results.values()):
        print("\n✓ All tests passed!")
        return True
    else:
        print("\n✗ Some tests failed")
        return False

if __name__ == "__main__":
    success = main()
    sys.exit(0 if success else 1)
