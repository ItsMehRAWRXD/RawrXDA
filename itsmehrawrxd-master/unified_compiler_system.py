#!/usr/bin/env python3
"""
Unified Compiler System
Integrates all compilers as plugins in the n0mn0m IDE
"""

import tkinter as tk
from tkinter import ttk, filedialog, messagebox, scrolledtext
import os
import sys
import subprocess
import threading
import time
from pathlib import Path
from typing import Dict, List, Optional, Any
import json

class UnifiedCompilerSystem:
    """Unified compiler system that manages all compilers as plugins"""
    
    def __init__(self, parent_ide):
        self.parent_ide = parent_ide
        self.compilers = {}
        self.current_compiler = None
        self.compilation_history = []
        
        # Initialize all available compilers
        self.initialize_compilers()
        
        print("Unified Compiler System initialized")
    
    def initialize_compilers(self):
        """Initialize all available compilers"""
        
        # Core compilers
        self.compilers['eon'] = {
            'name': 'EON Compiler',
            'description': 'Native EON ASM compiler',
            'extensions': ['.eon'],
            'module': 'eon_compiler',
            'enabled': True
        }
        
        self.compilers['cpp'] = {
            'name': 'C++ Compiler',
            'description': 'Real C++ compiler with PE generation',
            'extensions': ['.cpp', '.cxx', '.cc', '.c'],
            'module': 'real_cpp_compiler',
            'enabled': True
        }
        
        self.compilers['python'] = {
            'name': 'Python Compiler',
            'description': 'Python to executable compiler',
            'extensions': ['.py'],
            'module': 'real_python_compiler',
            'enabled': True
        }
        
        self.compilers['javascript'] = {
            'name': 'JavaScript Transpiler',
            'description': 'JavaScript to native code transpiler',
            'extensions': ['.js', '.ts'],
            'module': 'real_javascript_transpiler',
            'enabled': True
        }
        
        self.compilers['roslyn'] = {
            'name': 'Roslyn C# Compiler',
            'description': 'Microsoft Roslyn C# compiler',
            'extensions': ['.cs'],
            'module': 'roslyn_cpp_compiler',
            'enabled': True
        }
        
        self.compilers['exe'] = {
            'name': 'EXE Generator',
            'description': 'Direct PE executable generator',
            'extensions': ['.exe', '.bin'],
            'module': 'proper_exe_compiler',
            'enabled': True
        }
        
        # Advanced compilers
        self.compilers['extensible'] = {
            'name': 'Extensible Compiler',
            'description': 'Plugin-based extensible compiler',
            'extensions': ['.ext'],
            'module': 'extensible_compiler_system',
            'enabled': True
        }
        
        print(f"Initialized {len(self.compilers)} compilers")
    
    def get_compiler_for_file(self, file_path: str) -> Optional[str]:
        """Get the appropriate compiler for a file"""
        
        file_ext = Path(file_path).suffix.lower()
        
        for compiler_id, compiler_info in self.compilers.items():
            if compiler_info['enabled'] and file_ext in compiler_info['extensions']:
                return compiler_id
        
        return None
    
    def compile_file(self, source_file: str, output_file: str = None, compiler_id: str = None) -> bool:
        """Compile a file using the appropriate compiler"""
        
        try:
            # Determine compiler if not specified
            if not compiler_id:
                compiler_id = self.get_compiler_for_file(source_file)
            
            if not compiler_id:
                print(f"No compiler found for file: {source_file}")
                return False
            
            # Get compiler info
            compiler_info = self.compilers.get(compiler_id)
            if not compiler_info or not compiler_info['enabled']:
                print(f"Compiler {compiler_id} not available")
                return False
            
            # Set default output file if not provided
            if not output_file:
                output_file = self.get_default_output_file(source_file, compiler_id)
            
            # Compile using the appropriate compiler
            success = self.compile_with_compiler(compiler_id, source_file, output_file)
            
            if success:
                # Record compilation
                self.compilation_history.append({
                    'timestamp': time.time(),
                    'compiler': compiler_id,
                    'source': source_file,
                    'output': output_file,
                    'success': True
                })
                print(f"Compilation successful: {source_file} -> {output_file}")
            else:
                print(f"Compilation failed: {source_file}")
            
            return success
            
        except Exception as e:
            print(f"Compilation error: {e}")
            return False
    
    def compile_with_compiler(self, compiler_id: str, source_file: str, output_file: str) -> bool:
        """Compile using a specific compiler"""
        
        try:
            if compiler_id == 'eon':
                return self.compile_with_eon(source_file, output_file)
            elif compiler_id == 'cpp':
                return self.compile_with_cpp(source_file, output_file)
            elif compiler_id == 'python':
                return self.compile_with_python(source_file, output_file)
            elif compiler_id == 'javascript':
                return self.compile_with_javascript(source_file, output_file)
            elif compiler_id == 'roslyn':
                return self.compile_with_roslyn(source_file, output_file)
            elif compiler_id == 'exe':
                return self.compile_with_exe(source_file, output_file)
            elif compiler_id == 'extensible':
                return self.compile_with_extensible(source_file, output_file)
            else:
                print(f"Unknown compiler: {compiler_id}")
                return False
                
        except Exception as e:
            print(f"Error compiling with {compiler_id}: {e}")
            return False
    
    def compile_with_eon(self, source_file: str, output_file: str) -> bool:
        """Compile using EON compiler"""
        
        try:
            # Use the EON compiler from the parent IDE
            if hasattr(self.parent_ide, 'subsystems') and 'eon_compiler' in self.parent_ide.subsystems:
                eon_compiler = self.parent_ide.subsystems['eon_compiler']
                return eon_compiler.compile_file(source_file, output_file)
            else:
                print("EON compiler not available")
                return False
        except Exception as e:
            print(f"EON compilation error: {e}")
            return False
    
    def compile_with_cpp(self, source_file: str, output_file: str) -> bool:
        """Compile using C++ compiler"""
        
        try:
            # Import and use the real C++ compiler
            from real_cpp_compiler import RealCppCompiler
            compiler = RealCppCompiler()
            return compiler.compile_file(source_file, output_file)
        except Exception as e:
            print(f"C++ compilation error: {e}")
            return False
    
    def compile_with_python(self, source_file: str, output_file: str) -> bool:
        """Compile using Python compiler"""
        
        try:
            # Import and use the real Python compiler
            from real_python_compiler import RealPythonCompiler
            compiler = RealPythonCompiler()
            return compiler.compile_file(source_file, output_file)
        except Exception as e:
            print(f"Python compilation error: {e}")
            return False
    
    def compile_with_javascript(self, source_file: str, output_file: str) -> bool:
        """Compile using JavaScript transpiler"""
        
        try:
            # Import and use the JavaScript transpiler
            from real_javascript_transpiler import RealJavaScriptTranspiler
            transpiler = RealJavaScriptTranspiler()
            return transpiler.transpile_file(source_file, output_file)
        except Exception as e:
            print(f"JavaScript transpilation error: {e}")
            return False
    
    def compile_with_roslyn(self, source_file: str, output_file: str) -> bool:
        """Compile using Roslyn C# compiler"""
        
        try:
            # Import and use the Roslyn compiler
            from roslyn_cpp_compiler import RoslynCppCompiler
            compiler = RoslynCppCompiler()
            return compiler.compile_file(source_file, output_file)
        except Exception as e:
            print(f"Roslyn compilation error: {e}")
            return False
    
    def compile_with_exe(self, source_file: str, output_file: str) -> bool:
        """Compile using EXE generator"""
        
        try:
            # Import and use the EXE compiler
            from proper_exe_compiler import ProperEXECompiler
            compiler = ProperEXECompiler()
            return compiler.compile_to_exe(source_file, output_file)
        except Exception as e:
            print(f"EXE generation error: {e}")
            return False
    
    def compile_with_extensible(self, source_file: str, output_file: str) -> bool:
        """Compile using extensible compiler system"""
        
        try:
            # Import and use the extensible compiler
            from extensible_compiler_system import ExtensibleCompilerSystem
            compiler = ExtensibleCompilerSystem()
            return compiler.compile_file(source_file, output_file)
        except Exception as e:
            print(f"Extensible compilation error: {e}")
            return False
    
    def get_default_output_file(self, source_file: str, compiler_id: str) -> str:
        """Get default output file name"""
        
        source_path = Path(source_file)
        base_name = source_path.stem
        
        if compiler_id == 'eon':
            return str(source_path.parent / f"{base_name}.exe")
        elif compiler_id == 'cpp':
            return str(source_path.parent / f"{base_name}.exe")
        elif compiler_id == 'python':
            return str(source_path.parent / f"{base_name}.exe")
        elif compiler_id == 'javascript':
            return str(source_path.parent / f"{base_name}.exe")
        elif compiler_id == 'roslyn':
            return str(source_path.parent / f"{base_name}.exe")
        elif compiler_id == 'exe':
            return str(source_path.parent / f"{base_name}.exe")
        else:
            return str(source_path.parent / f"{base_name}_compiled.exe")
    
    def show_compiler_manager(self):
        """Show compiler management interface"""
        
        manager_window = tk.Toplevel(self.parent_ide.root)
        manager_window.title("Unified Compiler System")
        manager_window.geometry("800x600")
        manager_window.configure(bg='#1e1e1e')
        
        # Header
        header_frame = ttk.Frame(manager_window)
        header_frame.pack(fill=tk.X, padx=10, pady=10)
        
        ttk.Label(header_frame, text="Unified Compiler System", 
                 font=('Segoe UI', 16, 'bold')).pack()
        ttk.Label(header_frame, text="Manage all compilers as plugins", 
                 font=('Segoe UI', 10)).pack()
        
        # Compiler list
        list_frame = ttk.LabelFrame(manager_window, text="Available Compilers")
        list_frame.pack(fill=tk.BOTH, expand=True, padx=10, pady=5)
        
        # Create treeview for compilers
        columns = ('Name', 'Description', 'Extensions', 'Status')
        tree = ttk.Treeview(list_frame, columns=columns, show='headings', height=10)
        
        for col in columns:
            tree.heading(col, text=col)
            tree.column(col, width=150)
        
        # Add compilers to tree
        for compiler_id, compiler_info in self.compilers.items():
            status = "Enabled" if compiler_info['enabled'] else "Disabled"
            extensions = ", ".join(compiler_info['extensions'])
            
            tree.insert('', 'end', values=(
                compiler_info['name'],
                compiler_info['description'],
                extensions,
                status
            ))
        
        tree.pack(fill=tk.BOTH, expand=True, padx=5, pady=5)
        
        # Control buttons
        control_frame = ttk.Frame(manager_window)
        control_frame.pack(fill=tk.X, padx=10, pady=10)
        
        def toggle_compiler():
            selection = tree.selection()
            if selection:
                item = tree.item(selection[0])
                compiler_name = item['values'][0]
                
                # Find compiler by name
                for compiler_id, compiler_info in self.compilers.items():
                    if compiler_info['name'] == compiler_name:
                        compiler_info['enabled'] = not compiler_info['enabled']
                        status = "Enabled" if compiler_info['enabled'] else "Disabled"
                        tree.item(selection[0], values=(
                            compiler_info['name'],
                            compiler_info['description'],
                            ", ".join(compiler_info['extensions']),
                            status
                        ))
                        break
        
        def compile_file():
            source_file = filedialog.askopenfilename(
                title="Select Source File",
                filetypes=[
                    ("All Files", "*.*"),
                    ("C++ Files", "*.cpp *.cxx *.cc *.c"),
                    ("Python Files", "*.py"),
                    ("JavaScript Files", "*.js *.ts"),
                    ("C# Files", "*.cs"),
                    ("EON Files", "*.eon")
                ]
            )
            
            if source_file:
                output_file = filedialog.asksaveasfilename(
                    title="Save Output As",
                    defaultextension=".exe",
                    filetypes=[("Executable Files", "*.exe"), ("All Files", "*.*")]
                )
                
                if output_file:
                    # Compile in background thread
                    def compile_thread():
                        success = self.compile_file(source_file, output_file)
                        if success:
                            messagebox.showinfo("Success", f"Compilation successful!\nOutput: {output_file}")
                        else:
                            messagebox.showerror("Error", "Compilation failed!")
                    
                    threading.Thread(target=compile_thread, daemon=True).start()
        
        ttk.Button(control_frame, text="Toggle Compiler", 
                  command=toggle_compiler).pack(side=tk.LEFT, padx=5)
        ttk.Button(control_frame, text="Compile File", 
                  command=compile_file).pack(side=tk.LEFT, padx=5)
        ttk.Button(control_frame, text="Close", 
                  command=manager_window.destroy).pack(side=tk.RIGHT, padx=5)
    
    def get_compilation_history(self) -> List[Dict]:
        """Get compilation history"""
        return self.compilation_history
    
    def clear_history(self):
        """Clear compilation history"""
        self.compilation_history.clear()
        print("Compilation history cleared")
    
    def get_compiler_status(self) -> Dict[str, Any]:
        """Get status of all compilers"""
        
        status = {
            'total_compilers': len(self.compilers),
            'enabled_compilers': sum(1 for c in self.compilers.values() if c['enabled']),
            'disabled_compilers': sum(1 for c in self.compilers.values() if not c['enabled']),
            'compilation_history_count': len(self.compilation_history),
            'compilers': {}
        }
        
        for compiler_id, compiler_info in self.compilers.items():
            status['compilers'][compiler_id] = {
                'name': compiler_info['name'],
                'enabled': compiler_info['enabled'],
                'extensions': compiler_info['extensions']
            }
        
        return status

def main():
    """Test the unified compiler system"""
    
    print("Testing Unified Compiler System...")
    
    # Create a mock parent IDE
    class MockIDE:
        def __init__(self):
            self.root = tk.Tk()
            self.subsystems = {}
    
    mock_ide = MockIDE()
    compiler_system = UnifiedCompilerSystem(mock_ide)
    
    # Test compiler status
    status = compiler_system.get_compiler_status()
    print(f"Compiler Status: {status}")
    
    # Test file detection
    test_files = [
        "test.cpp",
        "test.py", 
        "test.js",
        "test.cs",
        "test.eon"
    ]
    
    for test_file in test_files:
        compiler_id = compiler_system.get_compiler_for_file(test_file)
        print(f"File: {test_file} -> Compiler: {compiler_id}")
    
    print("Unified Compiler System test complete")

if __name__ == "__main__":
    main()
