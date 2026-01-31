#!/usr/bin/env python3
"""
Launch Comprehensive Extensible Compiler System
Main entry point for the complete compiler system
"""

import os
import sys
import subprocess
from pathlib import Path

def print_banner():
    """Print system banner"""
    print("🚀" + "=" * 60 + "🚀")
    print("🚀" + " " * 20 + "COMPREHENSIVE EXTENSIBLE COMPILER SYSTEM" + " " * 20 + "🚀")
    print("🚀" + "=" * 60 + "🚀")
    print()
    print("🌟 Features:")
    print("   • Multi-language support (Python, JavaScript, Rust, C++)")
    print("   • Real transpilation (Python → C++, Python → Rust)")
    print("   • Drag & drop interface")
    print("   • Extensible plugin system")
    print("   • User-defined language parsers")
    print("   • Custom IR passes")
    print("   • Backend target generation")
    print("   • AST visualization")
    print("   • Code analytics")
    print()

def check_dependencies():
    """Check if required dependencies are available"""
    print("🔍 Checking dependencies...")
    
    required_modules = [
        'tkinter',
        'json',
        'threading',
        'pathlib'
    ]
    
    missing_modules = []
    for module in required_modules:
        try:
            __import__(module)
            print(f"   ✅ {module}")
        except ImportError:
            missing_modules.append(module)
            print(f"   ❌ {module}")
    
    if missing_modules:
        print(f"\n❌ Missing required modules: {', '.join(missing_modules)}")
        print("Please install the missing dependencies and try again.")
        return False
    
    print("✅ All dependencies available!")
    return True

def run_tests():
    """Run comprehensive tests"""
    print("\n🧪 Running comprehensive tests...")
    
    try:
        result = subprocess.run([sys.executable, 'test_comprehensive_system.py'], 
                              capture_output=True, text=True, cwd=os.getcwd())
        
        if result.returncode == 0:
            print("✅ All tests passed!")
            return True
        else:
            print("❌ Some tests failed:")
            print(result.stdout)
            print(result.stderr)
            return False
            
    except Exception as e:
        print(f"❌ Error running tests: {e}")
        return False

def launch_gui():
    """Launch the comprehensive GUI"""
    print("\n🚀 Launching Comprehensive Compiler GUI...")
    
    try:
        from comprehensive_compiler_gui import ComprehensiveCompilerGUI
        
        gui = ComprehensiveCompilerGUI()
        print("✅ GUI launched successfully!")
        print("📝 Instructions:")
        print("   • Use File → Open to load source files")
        print("   • Select language from dropdown")
        print("   • Click Compile to compile code")
        print("   • Use Transpile tabs for language conversion")
        print("   • Drag & drop files onto the drop area")
        print()
        
        gui.run()
        
    except Exception as e:
        print(f"❌ Error launching GUI: {e}")
        print("Please check that all components are properly installed.")
        return False
    
    return True

def show_help():
    """Show help information"""
    print("📖 Comprehensive Extensible Compiler System Help")
    print("=" * 50)
    print()
    print("🔧 Available Commands:")
    print("   python launch_comprehensive_compiler.py gui     - Launch GUI")
    print("   python launch_comprehensive_compiler.py test  - Run tests")
    print("   python launch_comprehensive_compiler.py help  - Show this help")
    print()
    print("📁 Project Structure:")
    print("   plugins/                    - Language components")
    print("   ├── python_components.py   - Python lexer/parser")
    print("   ├── js_components.py       - JavaScript lexer/parser")
    print("   ├── rust_components.py     - Rust lexer/parser")
    print("   ├── py_to_cpp_codegen.py   - Python→C++ transpiler")
    print("   └── py_to_rust_codegen.py  - Python→Rust transpiler")
    print()
    print("   ast_visitor.py              - AST visitor pattern")
    print("   user_compiler_components.py - User component manager")
    print("   comprehensive_compiler_gui.py - Main GUI")
    print("   test_comprehensive_system.py - Test suite")
    print()
    print("🌟 Features:")
    print("   • Multi-language compilation")
    print("   • Real transpilation between languages")
    print("   • Extensible plugin architecture")
    print("   • User-defined language parsers")
    print("   • Custom IR optimization passes")
    print("   • Backend target generation")
    print("   • Drag & drop file interface")
    print("   • AST visualization")
    print("   • Code analytics")
    print()

def main():
    """Main entry point"""
    print_banner()
    
    # Check if we're in the right directory
    if not os.path.exists('comprehensive_compiler_gui.py'):
        print("❌ Error: Please run this script from the project root directory")
        print("   (The directory containing comprehensive_compiler_gui.py)")
        return False
    
    # Parse command line arguments
    if len(sys.argv) > 1:
        command = sys.argv[1].lower()
        
        if command == 'test':
            if not check_dependencies():
                return False
            return run_tests()
        
        elif command == 'gui':
            if not check_dependencies():
                return False
            return launch_gui()
        
        elif command == 'help':
            show_help()
            return True
        
        else:
            print(f"❌ Unknown command: {command}")
            show_help()
            return False
    
    # Default: interactive mode
    print("🎯 Interactive Mode")
    print("Choose an option:")
    print("1. 🚀 Launch GUI")
    print("2. 🧪 Run Tests")
    print("3. 📖 Show Help")
    print("4. ❌ Exit")
    print()
    
    while True:
        try:
            choice = input("Enter your choice (1-4): ").strip()
            
            if choice == '1':
                if not check_dependencies():
                    return False
                return launch_gui()
            
            elif choice == '2':
                if not check_dependencies():
                    return False
                return run_tests()
            
            elif choice == '3':
                show_help()
                continue
            
            elif choice == '4':
                print("👋 Goodbye!")
                return True
            
            else:
                print("❌ Invalid choice. Please enter 1, 2, 3, or 4.")
                continue
                
        except KeyboardInterrupt:
            print("\n👋 Goodbye!")
            return True
        except Exception as e:
            print(f"❌ Error: {e}")
            return False

if __name__ == "__main__":
    success = main()
    sys.exit(0 if success else 1)
