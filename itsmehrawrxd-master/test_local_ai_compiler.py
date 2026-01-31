#!/usr/bin/env python3
"""
Test script for the Local AI Compiler Manager
Tests all AI services and features
"""

import tkinter as tk
from tkinter import ttk
import sys
import os

# Add current directory to path
sys.path.append(os.path.dirname(os.path.abspath(__file__)))

from local_ai_compiler_manager import LocalAICompilerManager

class TestIDE:
    """Mock IDE instance for testing"""
    def __init__(self):
        self.root = tk.Tk()
        self.root.title("Local AI Compiler Manager Test")
        self.root.geometry("1200x800")
        
        # Create notebook
        self.notebook = ttk.Notebook(self.root)
        self.notebook.pack(fill=tk.BOTH, expand=True)
        
        # Mock current file
        self.current_file = __file__

def main():
    """Test the Local AI Compiler Manager"""
    print("🤖 Testing Local AI Compiler Manager...")
    print("=" * 50)
    
    # Create test IDE
    test_ide = TestIDE()
    
    # Initialize AI manager
    try:
        ai_manager = LocalAICompilerManager(test_ide)
        print("✅ Local AI Compiler Manager initialized successfully!")
        
        # Print service status
        print("\n📊 Service Status:")
        for service, status in ai_manager.service_status.items():
            status_icon = "✅" if status else "❌"
            print(f"  {status_icon} {service.upper()}: {'Running' if status else 'Not running'}")
        
        # Print available models
        if ai_manager.available_models:
            print(f"\n📦 Available Models ({len(ai_manager.available_models)}):")
            for model in ai_manager.available_models:
                print(f"  • {model}")
        else:
            print("\n📦 No models available. Try pulling one!")
            print("💡 Quick start: ollama pull tinyllama:1.1b")
        
        print("\n🚀 Starting GUI...")
        print("💡 Use the GUI to:")
        print("  • Pull and test AI models")
        print("  • Test code completion with Tabby")
        print("  • Test context-aware chat")
        print("  • Test code analysis and documentation")
        print("  • Execute generated code with online compilers")
        
        # Start GUI
        test_ide.root.mainloop()
        
    except Exception as e:
        print(f"❌ Error initializing AI manager: {e}")
        import traceback
        traceback.print_exc()

if __name__ == "__main__":
    main()
