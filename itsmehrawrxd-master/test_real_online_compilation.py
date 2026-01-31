#!/usr/bin/env python3
"""
Test Real Online Compilation
Tests if our online IDE backend actually calls real online IDEs or is simulated
"""

import requests
import json
import time
from pathlib import Path

def test_real_online_apis():
    """Test if we can actually call real online IDE APIs"""
    print("Testing Real Online IDE APIs")
    print("=" * 50)
    
    # Test 1: Replit API
    print("Test 1: Replit API")
    try:
        # Try to access Replit API (this will likely fail without proper auth)
        response = requests.get("https://replit.com/api/v0/repls", timeout=5)
        print(f"  Replit API Status: {response.status_code}")
        if response.status_code == 200:
            print("  ✅ Replit API: ACCESSIBLE")
        else:
            print("  ❌ Replit API: NOT ACCESSIBLE (Auth required)")
    except Exception as e:
        print(f"  ❌ Replit API: ERROR - {str(e)[:50]}...")
    
    # Test 2: CodePen API
    print("\nTest 2: CodePen API")
    try:
        response = requests.get("https://codepen.io/api/v1/pens", timeout=5)
        print(f"  CodePen API Status: {response.status_code}")
        if response.status_code == 200:
            print("  ✅ CodePen API: ACCESSIBLE")
        else:
            print("  ❌ CodePen API: NOT ACCESSIBLE (Auth required)")
    except Exception as e:
        print(f"  ❌ CodePen API: ERROR - {str(e)[:50]}...")
    
    # Test 3: Ideone API
    print("\nTest 3: Ideone API")
    try:
        response = requests.get("https://ideone.com/api/v1/submissions", timeout=5)
        print(f"  Ideone API Status: {response.status_code}")
        if response.status_code == 200:
            print("  ✅ Ideone API: ACCESSIBLE")
        else:
            print("  ❌ Ideone API: NOT ACCESSIBLE (Auth required)")
    except Exception as e:
        print(f"  ❌ Ideone API: ERROR - {str(e)[:50]}...")
    
    # Test 4: Compiler Explorer API
    print("\nTest 4: Compiler Explorer API")
    try:
        response = requests.get("https://godbolt.org/api/compiler", timeout=5)
        print(f"  Compiler Explorer API Status: {response.status_code}")
        if response.status_code == 200:
            print("  ✅ Compiler Explorer API: ACCESSIBLE")
        else:
            print("  ❌ Compiler Explorer API: NOT ACCESSIBLE")
    except Exception as e:
        print(f"  ❌ Compiler Explorer API: ERROR - {str(e)[:50]}...")

def test_our_online_backend():
    """Test our online backend implementation"""
    print("\nTesting Our Online Backend Implementation")
    print("=" * 50)
    
    try:
        from online_ide_backend import OnlineIDEBackend
        
        backend = OnlineIDEBackend()
        
        # Test compilation
        python_code = '''
print("Testing real online compilation")
print("This should show if we're actually calling online APIs")
'''
        
        print("Testing Python compilation...")
        result = backend.compile_with_online_ide(python_code, 'python', 'test_real.py')
        
        if result:
            print(f"  Compilation result: {result['status']}")
            print(f"  IDE used: {result['ide']}")
            print(f"  Output: {result['output']}")
            print(f"  Executable: {result['executable']}")
            
            # Check if it's actually calling online APIs
            if "simulate" in result['logs'].lower() or "mock" in result['logs'].lower():
                print("  ❌ SIMULATED: Not calling real online APIs")
            else:
                print("  ✅ REAL: Actually calling online APIs")
        else:
            print("  ❌ Compilation failed")
            
    except Exception as e:
        print(f"  ❌ Backend test failed: {e}")

def analyze_our_implementation():
    """Analyze our online backend implementation"""
    print("\nAnalyzing Our Online Backend Implementation")
    print("=" * 50)
    
    # Read the online_ide_backend.py file
    backend_file = Path("online_ide_backend.py")
    if backend_file.exists():
        with open(backend_file, 'r', encoding='utf-8') as f:
            content = f.read()
        
        # Look for real API calls vs simulated ones
        real_api_indicators = [
            'requests.get',
            'requests.post',
            'api_key',
            'authentication',
            'real_compilation',
            'actual_api'
        ]
        
        simulated_indicators = [
            'simulate',
            'mock',
            'fake',
            'time.sleep',
            'return result',
            'compilation successful'
        ]
        
        real_count = sum(1 for indicator in real_api_indicators if indicator in content.lower())
        simulated_count = sum(1 for indicator in simulated_indicators if indicator in content.lower())
        
        print(f"  Real API indicators found: {real_count}")
        print(f"  Simulated indicators found: {simulated_count}")
        
        if simulated_count > real_count:
            print("  ❌ CONCLUSION: Our backend is SIMULATED")
            print("  📝 It creates fake results without calling real APIs")
        else:
            print("  ✅ CONCLUSION: Our backend calls REAL APIs")
            print("  📝 It actually communicates with online IDEs")

def test_workspace_files():
    """Test what files are actually created in workspace"""
    print("\nTesting Workspace Files")
    print("=" * 50)
    
    workspace_dir = Path("online_ide_workspace")
    if workspace_dir.exists():
        print(f"  Workspace directory: {workspace_dir}")
        
        # Check output files
        output_dir = workspace_dir / "output"
        if output_dir.exists():
            result_files = list(output_dir.glob("*_result.json"))
            print(f"  Result files found: {len(result_files)}")
            
            if result_files:
                # Read a result file
                with open(result_files[0], 'r') as f:
                    result = json.load(f)
                
                print(f"  Sample result:")
                print(f"    IDE: {result.get('ide', 'unknown')}")
                print(f"    Status: {result.get('status', 'unknown')}")
                print(f"    Output: {result.get('output', 'unknown')}")
                
                # Check if it's real or simulated
                if "successful" in result.get('output', '').lower():
                    print("  ❌ SIMULATED: Generic success messages")
                else:
                    print("  ✅ REAL: Specific compilation results")
        
        # Check executable files
        executable_files = list(output_dir.glob("*.exe"))
        print(f"  Executable files: {len(executable_files)}")
        
        if executable_files:
            # Check if they're real executables or placeholders
            sample_exe = executable_files[0]
            with open(sample_exe, 'r', encoding='utf-8', errors='ignore') as f:
                content = f.read()
            
            if "compiled with" in content.lower():
                print("  ❌ SIMULATED: Placeholder executable files")
            else:
                print("  ✅ REAL: Actual executable files")

def main():
    """Main function"""
    print("Testing Real Online Compilation")
    print("Do our online compilers actually work?")
    print("=" * 60)
    
    test_real_online_apis()
    test_our_online_backend()
    analyze_our_implementation()
    test_workspace_files()
    
    print("\n" + "=" * 60)
    print("FINAL CONCLUSION:")
    print("=" * 60)
    print("❌ Our online IDE backend is SIMULATED")
    print("📝 It creates fake results without calling real APIs")
    print("🔧 It provides the structure for real API integration")
    print("🚀 Ready to be extended with actual API calls")
    print("\n🎯 WHAT WE HAVE:")
    print("✅ Complete workspace management")
    print("✅ File I/O and result storage")
    print("✅ Compiler selection logic")
    print("✅ Error handling framework")
    print("✅ Configuration system")
    print("\n🎯 WHAT WE'RE MISSING:")
    print("❌ Real API authentication")
    print("❌ Actual HTTP requests to online IDEs")
    print("❌ Real compilation results")
    print("❌ Actual executable generation")

if __name__ == "__main__":
    main()
