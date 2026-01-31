#!/usr/bin/env python3
"""
Extensible Compiler System Launcher
Quick launcher for the meta-prompting AST/IR generator
"""

import sys
import os
import subprocess

def check_dependencies():
    """Check if required dependencies are available"""
    try:
        import tkinter
        print("✅ tkinter available")
    except ImportError:
        print("❌ tkinter not available. Please install python3-tk")
        return False
    
    try:
        import dataclasses
        print("✅ dataclasses available")
    except ImportError:
        print("❌ dataclasses not available. Please upgrade Python to 3.7+")
        return False
    
    return True

def run_tests():
    """Run the test suite"""
    print("🧪 Running test suite...")
    try:
        result = subprocess.run([sys.executable, "test_compiler.py"], 
                              capture_output=True, text=True, cwd=os.path.dirname(__file__))
        if result.returncode == 0:
            print("✅ Tests passed!")
            return True
        else:
            print("❌ Tests failed!")
            print(result.stderr)
            return False
    except Exception as e:
        print(f"❌ Error running tests: {e}")
        return False

def run_gui():
    """Run the GUI application"""
    print("🚀 Starting Extensible Compiler System GUI...")
    try:
        subprocess.run([sys.executable, "main.py"], cwd=os.path.dirname(__file__))
    except KeyboardInterrupt:
        print("\n👋 Goodbye!")
    except Exception as e:
        print(f"❌ Error starting GUI: {e}")

def main():
    """Main launcher function"""
    print("🔧 Extensible Compiler System Launcher")
    print("=" * 50)
    
    # Check dependencies
    if not check_dependencies():
        print("\n❌ Missing dependencies. Please install required packages.")
        return
    
    print("\n📋 Available options:")
    print("1. Run GUI application")
    print("2. Run test suite")
    print("3. Run both tests and GUI")
    print("4. Exit")
    
    while True:
        try:
            choice = input("\n🎯 Enter your choice (1-4): ").strip()
            
            if choice == "1":
                run_gui()
                break
            elif choice == "2":
                if run_tests():
                    print("\n✅ All tests passed!")
                break
            elif choice == "3":
                if run_tests():
                    print("\n✅ All tests passed!")
                    run_gui()
                break
            elif choice == "4":
                print("👋 Goodbye!")
                break
            else:
                print("❌ Invalid choice. Please enter 1-4.")
                
        except KeyboardInterrupt:
            print("\n👋 Goodbye!")
            break
        except Exception as e:
            print(f"❌ Error: {e}")
            break

if __name__ == "__main__":
    main()
