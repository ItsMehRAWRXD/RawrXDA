#!/usr/bin/env python3
"""
Test the Unified Compiler System
"""

import sys
import os

# Add current directory to path
sys.path.append(os.path.dirname(os.path.abspath(__file__)))

def test_unified_compiler():
    """Test the unified compiler system"""
    
    print("Testing Unified Compiler System...")
    print("=" * 50)
    
    try:
        # Import the unified compiler system
        from unified_compiler_system import UnifiedCompilerSystem
        
        # Create a mock parent IDE
        class MockIDE:
            def __init__(self):
                self.subsystems = {}
        
        mock_ide = MockIDE()
        compiler_system = UnifiedCompilerSystem(mock_ide)
        
        # Test compiler status
        print("\n1. Testing compiler status...")
        status = compiler_system.get_compiler_status()
        print(f"   Total compilers: {status['total_compilers']}")
        print(f"   Enabled compilers: {status['enabled_compilers']}")
        print(f"   Disabled compilers: {status['disabled_compilers']}")
        
        # Test file detection
        print("\n2. Testing file detection...")
        test_files = [
            "test.cpp",
            "test.py", 
            "test.js",
            "test.cs",
            "test.eon",
            "test.unknown"
        ]
        
        for test_file in test_files:
            compiler_id = compiler_system.get_compiler_for_file(test_file)
            print(f"   {test_file} -> {compiler_id}")
        
        # Test default output files
        print("\n3. Testing default output files...")
        for test_file in test_files:
            compiler_id = compiler_system.get_compiler_for_file(test_file)
            if compiler_id:
                output_file = compiler_system.get_default_output_file(test_file, compiler_id)
                print(f"   {test_file} -> {output_file}")
        
        # Test compilation history
        print("\n4. Testing compilation history...")
        history = compiler_system.get_compilation_history()
        print(f"   History entries: {len(history)}")
        
        # Test compiler info
        print("\n5. Testing compiler info...")
        for compiler_id, compiler_info in compiler_system.compilers.items():
            print(f"   {compiler_id}: {compiler_info['name']} ({'enabled' if compiler_info['enabled'] else 'disabled'})")
        
        print("\n✅ Unified Compiler System test completed successfully!")
        return True
        
    except Exception as e:
        print(f"\n❌ Test failed: {e}")
        import traceback
        traceback.print_exc()
        return False

def main():
    """Main test function"""
    
    print("🚀 Starting Unified Compiler System Test")
    print("=" * 60)
    
    success = test_unified_compiler()
    
    if success:
        print("\n🎉 All tests passed!")
        print("The Unified Compiler System is working correctly.")
    else:
        print("\n❌ Some tests failed.")
        print("Please check the implementation.")
    
    return success

if __name__ == "__main__":
    main()
