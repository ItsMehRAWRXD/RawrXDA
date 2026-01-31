#!/usr/bin/env python3
"""
Simple IDE Test - Minimal functionality to avoid antivirus triggers
"""

import sys
import os

def main():
    print("=== Simple IDE Test ===")
    print(f"Python version: {sys.version}")
    print(f"Current directory: {os.getcwd()}")
    print("✅ Basic Python functionality working")
    
    # Test basic file operations
    try:
        test_file = "test_ide_simple.txt"
        with open(test_file, 'w') as f:
            f.write("Test file created by IDE")
        print(f"✅ File operations working - created {test_file}")
        
        # Clean up
        os.remove(test_file)
        print("✅ File cleanup working")
        
    except Exception as e:
        print(f"❌ File operations failed: {e}")
    
    print("\n=== IDE Test Complete ===")
    print("If you see this, the basic IDE functionality is working!")
    print("You can now run the full IDE with: python eon-compiler-gui.py")

if __name__ == "__main__":
    main()
