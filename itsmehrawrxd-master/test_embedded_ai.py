#!/usr/bin/env python3
"""
Test script for embedded AI functionality
Run this to verify the embedded AI engine is working correctly
"""

import sys
import os
sys.path.insert(0, os.path.dirname(__file__))

from safe_hybrid_ide import EmbeddedAIEngine, EmbeddedAIServer
import time
import requests
import json

def test_embedded_ai_engine():
    """Test the embedded AI engine directly"""
    print("🧪 Testing Embedded AI Engine...")
    
    engine = EmbeddedAIEngine()
    
    # Test Python code analysis
    python_code = '''
def hello_world():
    print("Hello World!")
    return "success"

# Missing docstring, print statement
if __name__ == "__main__":
    hello_world()
'''
    
    print("\n📝 Analyzing Python code:")
    print(python_code)
    
    result = engine.analyze_code(python_code, "python")
    
    if result['success']:
        print(f"\n✅ Analysis successful! Found {len(result['suggestions'])} suggestions:")
        for i, suggestion in enumerate(result['suggestions'], 1):
            print(f"\n{i}. {suggestion['title']}")
            print(f"   Priority: {suggestion['priority']}")
            print(f"   Description: {suggestion['description']}")
            print(f"   Fix: {suggestion['fix']}")
    else:
        print(f"\n❌ Analysis failed: {result.get('error', 'Unknown error')}")
    
    return result['success']

def test_embedded_ai_server():
    """Test the embedded AI server HTTP interface"""
    print("\n🌐 Testing Embedded AI Server...")
    
    server = EmbeddedAIServer(port=11436)  # Different port for testing
    
    if not server.start_server():
        print("❌ Failed to start server")
        return False
    
    time.sleep(1)  # Give server time to start
    
    try:
        # Test /api/tags endpoint
        print("\n🏷️ Testing /api/tags endpoint...")
        response = requests.get("http://localhost:11436/api/tags", timeout=5)
        if response.status_code == 200:
            data = response.json()
            print(f"✅ Tags endpoint working: {data}")
        else:
            print(f"❌ Tags endpoint failed: {response.status_code}")
            return False
        
        # Test /api/generate endpoint
        print("\n🔄 Testing /api/generate endpoint...")
        test_prompt = '''Review this Python code and suggest improvements:

def calculate(a, b):
    return a + b

print(calculate(5, 3))
'''
        
        payload = {
            "model": "embedded-ai:latest",
            "prompt": test_prompt,
            "stream": False
        }
        
        response = requests.post(
            "http://localhost:11436/api/generate",
            json=payload,
            timeout=10
        )
        
        if response.status_code == 200:
            data = response.json()
            print("✅ Generate endpoint working!")
            print(f"📤 Model: {data['model']}")
            print(f"📝 Response length: {len(data['response'])} characters")
            print(f"🎯 Sample response: {data['response'][:200]}...")
        else:
            print(f"❌ Generate endpoint failed: {response.status_code}")
            print(f"Response: {response.text}")
            return False
        
        return True
    
    except requests.RequestException as e:
        print(f"❌ Server test failed: {e}")
        return False
    
    finally:
        server.stop_server()

def test_javascript_analysis():
    """Test JavaScript code analysis"""
    print("\n🟨 Testing JavaScript Analysis...")
    
    engine = EmbeddedAIEngine()
    
    js_code = '''
var userName = "John";
function greetUser(name) {
    if (name == undefined) {
        console.log("Hello, Guest!");
    } else {
        console.log("Hello, " + name + "!");
    }
}

greetUser(userName);
'''
    
    print("📝 Analyzing JavaScript code:")
    print(js_code)
    
    result = engine.analyze_code(js_code, "javascript")
    
    if result['success']:
        print(f"\n✅ JavaScript analysis successful! Found {len(result['suggestions'])} suggestions:")
        for i, suggestion in enumerate(result['suggestions'], 1):
            priority_emoji = {'high': '🔴', 'medium': '🟡', 'low': '🟢'}.get(suggestion['priority'], '🟡')
            print(f"\n{i}. {priority_emoji} {suggestion['title']}")
            print(f"   {suggestion['description']}")
    
    return result['success']

def test_cpp_analysis():
    """Test C++ code analysis"""
    print("\n🔷 Testing C++ Analysis...")
    
    engine = EmbeddedAIEngine()
    
    cpp_code = '''
#include <iostream>
using namespace std;

int main() {
    int* ptr = new int(42);
    cout << *ptr << endl;
    return 0;
}
'''
    
    print("📝 Analyzing C++ code:")
    print(cpp_code)
    
    result = engine.analyze_code(cpp_code, "cpp")
    
    if result['success']:
        print(f"\n✅ C++ analysis successful! Found {len(result['suggestions'])} suggestions:")
        for suggestion in result['suggestions']:
            if suggestion['priority'] == 'high':
                print(f"🔴 {suggestion['title']}: {suggestion['description']}")
    
    return result['success']

if __name__ == "__main__":
    print("🤖 Embedded AI Test Suite")
    print("=" * 50)
    
    tests_passed = 0
    total_tests = 4
    
    # Run tests
    if test_embedded_ai_engine():
        tests_passed += 1
    
    if test_embedded_ai_server():
        tests_passed += 1
    
    if test_javascript_analysis():
        tests_passed += 1
    
    if test_cpp_analysis():
        tests_passed += 1
    
    # Results
    print("\n" + "=" * 50)
    print(f"🎯 Test Results: {tests_passed}/{total_tests} tests passed")
    
    if tests_passed == total_tests:
        print("🎉 All tests passed! Embedded AI is working correctly.")
        print("\n✅ Ready to use in the IDE:")
        print("   - Embedded AI engine: ✅ Working")
        print("   - HTTP server interface: ✅ Working") 
        print("   - Multi-language support: ✅ Working")
        print("   - Code analysis patterns: ✅ Working")
        print("\n🚀 Launch safe_hybrid_ide.py to use the AI-enhanced IDE!")
    else:
        print("❌ Some tests failed. Please check the implementation.")
    
    print("\nPress Enter to exit...")
    input()
