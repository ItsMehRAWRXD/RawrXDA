#!/usr/bin/env python3
"""
Dynamic Compiler Loader for n0mn0m IDE
Automatically detects and loads external compilers/tools based on project type
Uses curl, gcc, javac, node, etc. when available, falls back to internal compilers
"""

import tkinter as tk
from tkinter import ttk, messagebox, scrolledtext
import os
import sys
import subprocess
import shutil
import threading
import time
import json
from pathlib import Path
from typing import Dict, List, Optional, Any, Tuple

class DynamicCompilerLoader:
    """Dynamically loads external compilers and tools based on project requirements"""
    
    def __init__(self, ide_instance):
        self.ide_instance = ide_instance
        self.detected_tools = {}
        self.available_compilers = {}
        self.compiler_paths = {}
        self.project_compilers = {}
        
        # Tool detection configuration
        self.tool_configs = {
            'gcc': {
                'commands': ['gcc', 'g++'],
                'extensions': ['.c', '.cpp', '.cc', '.cxx'],
                'test_cmd': ['--version'],
                'priority': 1
            },
            'clang': {
                'commands': ['clang', 'clang++'],
                'extensions': ['.c', '.cpp', '.cc', '.cxx'],
                'test_cmd': ['--version'],
                'priority': 2
            },
            'javac': {
                'commands': ['javac'],
                'extensions': ['.java'],
                'test_cmd': ['-version'],
                'priority': 1
            },
            'python': {
                'commands': ['python', 'python3'],
                'extensions': ['.py'],
                'test_cmd': ['--version'],
                'priority': 1
            },
            'node': {
                'commands': ['node'],
                'extensions': ['.js'],
                'test_cmd': ['--version'],
                'priority': 1
            },
            'dotnet': {
                'commands': ['dotnet'],
                'extensions': ['.cs'],
                'test_cmd': ['--version'],
                'priority': 1
            },
            'rust': {
                'commands': ['rustc', 'cargo'],
                'extensions': ['.rs'],
                'test_cmd': ['--version'],
                'priority': 1
            },
            'go': {
                'commands': ['go'],
                'extensions': ['.go'],
                'test_cmd': ['version'],
                'priority': 1
            },
            'php': {
                'commands': ['php'],
                'extensions': ['.php'],
                'test_cmd': ['--version'],
                'priority': 1
            },
            'ruby': {
                'commands': ['ruby'],
                'extensions': ['.rb'],
                'test_cmd': ['--version'],
                'priority': 1
            },
            'swift': {
                'commands': ['swift'],
                'extensions': ['.swift'],
                'test_cmd': ['--version'],
                'priority': 1
            },
            'kotlin': {
                'commands': ['kotlinc'],
                'extensions': ['.kt'],
                'test_cmd': ['-version'],
                'priority': 1
            },
            'scala': {
                'commands': ['scalac'],
                'extensions': ['.scala'],
                'test_cmd': ['-version'],
                'priority': 1
            },
            'haskell': {
                'commands': ['ghc'],
                'extensions': ['.hs'],
                'test_cmd': ['--version'],
                'priority': 1
            },
            'lua': {
                'commands': ['lua'],
                'extensions': ['.lua'],
                'test_cmd': ['-v'],
                'priority': 1
            },
            'nasm': {
                'commands': ['nasm'],
                'extensions': ['.asm', '.s'],
                'test_cmd': ['--version'],
                'priority': 1
            },
            'curl': {
                'commands': ['curl'],
                'extensions': [],
                'test_cmd': ['--version'],
                'priority': 1
            },
            'git': {
                'commands': ['git'],
                'extensions': [],
                'test_cmd': ['--version'],
                'priority': 1
            },
            'docker': {
                'commands': ['docker'],
                'extensions': [],
                'test_cmd': ['--version'],
                'priority': 1
            }
        }
        
        self.setup_gui()
        self.detect_all_tools()
    
    def setup_gui(self, parent_frame=None):
        """Setup the dynamic compiler loader GUI"""
        if parent_frame is None:
            parent_frame = ttk.Frame(self.ide_instance.notebook)
            self.ide_instance.notebook.add(parent_frame, text="🔧 Compiler Manager")
        
        main_frame = ttk.Frame(parent_frame, padding="10")
        main_frame.pack(fill=tk.BOTH, expand=True)
        
        # Title
        ttk.Label(main_frame, text="🔧 Dynamic Compiler Loader", 
                 font=("Arial", 16, "bold")).pack(pady=10)
        
        # Detection status
        status_frame = ttk.LabelFrame(main_frame, text="Tool Detection Status", padding="10")
        status_frame.pack(fill=tk.X, pady=5)
        
        self.status_text = scrolledtext.ScrolledText(status_frame, wrap=tk.WORD, height=6, state=tk.DISABLED)
        self.status_text.pack(fill=tk.X)
        
        # Available compilers
        compiler_frame = ttk.LabelFrame(main_frame, text="Available Compilers", padding="10")
        compiler_frame.pack(fill=tk.BOTH, expand=True, pady=5)
        
        # Create treeview for compilers
        columns = ('Tool', 'Version', 'Path', 'Extensions', 'Status')
        self.compiler_tree = ttk.Treeview(compiler_frame, columns=columns, show='headings', height=10)
        
        for col in columns:
            self.compiler_tree.heading(col, text=col)
            self.compiler_tree.column(col, width=120)
        
        # Scrollbar for treeview
        tree_scroll = ttk.Scrollbar(compiler_frame, orient=tk.VERTICAL, command=self.compiler_tree.yview)
        self.compiler_tree.configure(yscrollcommand=tree_scroll.set)
        
        self.compiler_tree.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
        tree_scroll.pack(side=tk.RIGHT, fill=tk.Y)
        
        # Action buttons
        button_frame = ttk.Frame(main_frame)
        button_frame.pack(fill=tk.X, pady=10)
        
        ttk.Button(button_frame, text="🔍 Re-detect Tools", 
                  command=self.detect_all_tools).pack(side=tk.LEFT, padx=5)
        ttk.Button(button_frame, text="📋 Copy Tool Info", 
                  command=self.copy_tool_info).pack(side=tk.LEFT, padx=5)
        ttk.Button(button_frame, text="🚀 Test Compiler", 
                  command=self.test_selected_compiler).pack(side=tk.LEFT, padx=5)
        ttk.Button(button_frame, text="⚙️ Configure Project", 
                  command=self.configure_project_compilers).pack(side=tk.RIGHT, padx=5)
        
        # Project compiler configuration
        project_frame = ttk.LabelFrame(main_frame, text="Project Compiler Configuration", padding="10")
        project_frame.pack(fill=tk.X, pady=5)
        
        self.project_text = scrolledtext.ScrolledText(project_frame, wrap=tk.WORD, height=4, state=tk.DISABLED)
        self.project_text.pack(fill=tk.X)
    
    def detect_all_tools(self):
        """Detect all available tools and compilers"""
        self._update_status("🔍 Detecting available tools and compilers...")
        
        # Clear previous results
        self.detected_tools.clear()
        self.available_compilers.clear()
        self.compiler_paths.clear()
        
        # Detect tools in thread
        thread = threading.Thread(target=self._detect_tools_thread)
        thread.daemon = True
        thread.start()
    
    def _detect_tools_thread(self):
        """Background thread for tool detection"""
        detected_count = 0
        
        for tool_name, config in self.tool_configs.items():
            for command in config['commands']:
                path = shutil.which(command)
                if path:
                    try:
                        # Test the command
                        result = subprocess.run([command] + config['test_cmd'], 
                                              capture_output=True, text=True, timeout=5)
                        
                        if result.returncode == 0:
                            version = result.stdout.strip().split('\n')[0]
                            
                            self.detected_tools[command] = {
                                'name': tool_name,
                                'path': path,
                                'version': version,
                                'extensions': config['extensions'],
                                'priority': config['priority']
                            }
                            
                            # Add to available compilers
                            for ext in config['extensions']:
                                if ext not in self.available_compilers:
                                    self.available_compilers[ext] = []
                                
                                self.available_compilers[ext].append({
                                    'command': command,
                                    'path': path,
                                    'version': version,
                                    'priority': config['priority']
                                })
                            
                            self.compiler_paths[command] = path
                            detected_count += 1
                            
                            self._update_status(f"✅ Found {tool_name}: {version}")
                            break
                        
                    except (subprocess.TimeoutExpired, FileNotFoundError, Exception) as e:
                        self._update_status(f"⚠️ {command} found but not working: {str(e)}")
        
        # Sort compilers by priority
        for ext in self.available_compilers:
            self.available_compilers[ext].sort(key=lambda x: x['priority'])
        
        self._update_status(f"🎉 Detection complete! Found {detected_count} tools")
        self._update_compiler_tree()
        self._update_project_config()
    
    def _update_compiler_tree(self):
        """Update the compiler tree display"""
        # Clear existing items
        for item in self.compiler_tree.get_children():
            self.compiler_tree.delete(item)
        
        # Add detected tools
        for command, info in self.detected_tools.items():
            extensions_str = ', '.join(info['extensions']) if info['extensions'] else 'N/A'
            self.compiler_tree.insert('', 'end', values=(
                info['name'],
                info['version'][:30] + '...' if len(info['version']) > 30 else info['version'],
                info['path'],
                extensions_str,
                'Available'
            ))
    
    def _update_project_config(self):
        """Update project compiler configuration display"""
        self.project_text.config(state=tk.NORMAL)
        self.project_text.delete(1.0, tk.END)
        
        config_text = "Project Compiler Configuration:\n\n"
        
        for ext, compilers in self.available_compilers.items():
            if compilers:
                best_compiler = compilers[0]  # Highest priority
                config_text += f"{ext}: {best_compiler['command']} ({best_compiler['version']})\n"
            else:
                config_text += f"{ext}: No compiler available - using internal fallback\n"
        
        config_text += f"\nTotal languages supported: {len(self.available_compilers)}"
        config_text += f"\nTotal tools detected: {len(self.detected_tools)}"
        
        self.project_text.insert(tk.END, config_text)
        self.project_text.config(state=tk.DISABLED)
    
    def get_compiler_for_file(self, file_path: str) -> Optional[Dict]:
        """Get the best compiler for a given file"""
        if not file_path:
            return None
        
        file_ext = Path(file_path).suffix.lower()
        
        if file_ext in self.available_compilers and self.available_compilers[file_ext]:
            return self.available_compilers[file_ext][0]  # Best compiler
        
        return None
    
    def compile_file_dynamic(self, file_path: str, output_path: str = None) -> Dict:
        """Compile file using dynamically detected compiler"""
        compiler = self.get_compiler_for_file(file_path)
        
        if not compiler:
            return {
                'success': False,
                'error': f"No compiler available for {Path(file_path).suffix} files",
                'fallback': True
            }
        
        try:
            # Read source code
            with open(file_path, 'r', encoding='utf-8') as f:
                source_code = f.read()
            
            # Generate output path if not provided
            if not output_path:
                base_name = Path(file_path).stem
                if compiler['command'] in ['gcc', 'g++', 'clang', 'clang++']:
                    output_path = f"{base_name}.exe"
                elif compiler['command'] == 'javac':
                    output_path = f"{base_name}.class"
                elif compiler['command'] in ['python', 'python3']:
                    output_path = f"{base_name}.pyc"
                else:
                    output_path = f"{base_name}_compiled"
            
            # Build compilation command
            cmd = self._build_compilation_command(compiler, file_path, output_path)
            
            # Execute compilation
            result = subprocess.run(cmd, capture_output=True, text=True, timeout=30)
            
            if result.returncode == 0:
                return {
                    'success': True,
                    'output_path': output_path,
                    'compiler': compiler['command'],
                    'version': compiler['version'],
                    'stdout': result.stdout,
                    'fallback': False
                }
            else:
                return {
                    'success': False,
                    'error': result.stderr,
                    'compiler': compiler['command'],
                    'fallback': False
                }
                
        except subprocess.TimeoutExpired:
            return {
                'success': False,
                'error': "Compilation timeout",
                'fallback': True
            }
        except Exception as e:
            return {
                'success': False,
                'error': str(e),
                'fallback': True
            }
    
    def _build_compilation_command(self, compiler: Dict, input_path: str, output_path: str) -> List[str]:
        """Build compilation command based on compiler type"""
        command = compiler['command']
        
        if command in ['gcc', 'g++', 'clang', 'clang++']:
            return [command, '-o', output_path, input_path]
        elif command == 'javac':
            return [command, input_path]
        elif command in ['python', 'python3']:
            return [command, '-m', 'py_compile', input_path]
        elif command == 'dotnet':
            return [command, 'build', input_path, '-o', Path(output_path).parent]
        elif command == 'rustc':
            return [command, input_path, '-o', output_path]
        elif command == 'go':
            return [command, 'build', '-o', output_path, input_path]
        elif command in ['php', 'ruby', 'lua']:
            return [command, '-c', input_path]  # Syntax check
        elif command == 'nasm':
            obj_file = Path(output_path).with_suffix('.o')
            return [command, '-f', 'win64' if os.name == 'nt' else 'elf64', input_path, '-o', str(obj_file)]
        else:
            return [command, input_path]
    
    def test_selected_compiler(self):
        """Test the selected compiler with a simple program"""
        selection = self.compiler_tree.selection()
        if not selection:
            messagebox.showwarning("Warning", "Please select a compiler to test")
            return
        
        item = self.compiler_tree.item(selection[0])
        tool_name = item['values'][0]
        compiler_path = item['values'][2]
        
        self._update_status(f"🧪 Testing {tool_name} at {compiler_path}...")
        
        thread = threading.Thread(target=self._test_compiler_thread, args=(tool_name, compiler_path))
        thread.daemon = True
        thread.start()
    
    def _test_compiler_thread(self, tool_name: str, compiler_path: str):
        """Background thread for compiler testing"""
        try:
            # Create a simple test program based on tool type
            test_code = self._generate_test_code(tool_name)
            
            if not test_code:
                self._update_status(f"❌ No test code available for {tool_name}")
                return
            
            # Write test file
            test_file = f"test_{tool_name}.{self._get_extension_for_tool(tool_name)}"
            with open(test_file, 'w') as f:
                f.write(test_code)
            
            # Test compilation
            result = self.compile_file_dynamic(test_file)
            
            if result['success']:
                self._update_status(f"✅ {tool_name} test successful!")
                self._update_status(f"📤 Output: {result.get('output_path', 'N/A')}")
            else:
                self._update_status(f"❌ {tool_name} test failed: {result['error']}")
            
            # Cleanup test file
            try:
                os.remove(test_file)
                if result['success'] and result.get('output_path'):
                    os.remove(result['output_path'])
            except:
                pass
                
        except Exception as e:
            self._update_status(f"❌ Test error: {str(e)}")
    
    def _generate_test_code(self, tool_name: str) -> str:
        """Generate test code for different tools"""
        test_codes = {
            'gcc': '#include <stdio.h>\nint main() { printf("Hello from C!\\n"); return 0; }',
            'g++': '#include <iostream>\nint main() { std::cout << "Hello from C++!" << std::endl; return 0; }',
            'javac': 'public class Test { public static void main(String[] args) { System.out.println("Hello from Java!"); } }',
            'python': 'print("Hello from Python!")',
            'node': 'console.log("Hello from JavaScript!");',
            'dotnet': 'using System; class Program { static void Main() { Console.WriteLine("Hello from C#!"); } }',
            'rust': 'fn main() { println!("Hello from Rust!"); }',
            'go': 'package main\nimport "fmt"\nfunc main() { fmt.Println("Hello from Go!") }',
            'php': '<?php echo "Hello from PHP!"; ?>',
            'ruby': 'puts "Hello from Ruby!"',
            'lua': 'print("Hello from Lua!")',
            'nasm': 'section .text\nglobal _start\n_start:\nmov eax, 4\nmov ebx, 1\nmov ecx, msg\nmov edx, len\nint 0x80\nmov eax, 1\nint 0x80\nsection .data\nmsg db "Hello from Assembly!", 0xa\nlen equ $ - msg'
        }
        
        return test_codes.get(tool_name, '')
    
    def _get_extension_for_tool(self, tool_name: str) -> str:
        """Get file extension for tool"""
        extensions = {
            'gcc': 'c',
            'g++': 'cpp',
            'javac': 'java',
            'python': 'py',
            'node': 'js',
            'dotnet': 'cs',
            'rust': 'rs',
            'go': 'go',
            'php': 'php',
            'ruby': 'rb',
            'lua': 'lua',
            'nasm': 'asm'
        }
        return extensions.get(tool_name, 'txt')
    
    def configure_project_compilers(self):
        """Configure compilers for current project"""
        if not hasattr(self.ide_instance, 'current_file') or not self.ide_instance.current_file:
            messagebox.showwarning("Warning", "No project file selected")
            return
        
        file_ext = Path(self.ide_instance.current_file).suffix.lower()
        available_compilers = self.available_compilers.get(file_ext, [])
        
        if not available_compilers:
            messagebox.showinfo("Info", f"No external compilers available for {file_ext} files.\nUsing internal compiler fallback.")
            return
        
        # Show compiler selection dialog
        self._show_compiler_selection_dialog(file_ext, available_compilers)
    
    def _show_compiler_selection_dialog(self, file_ext: str, compilers: List[Dict]):
        """Show dialog for compiler selection"""
        dialog = tk.Toplevel(self.ide_instance.root)
        dialog.title(f"Select Compiler for {file_ext}")
        dialog.geometry("500x300")
        dialog.transient(self.ide_instance.root)
        dialog.grab_set()
        
        ttk.Label(dialog, text=f"Available compilers for {file_ext}:", 
                 font=("Arial", 12, "bold")).pack(pady=10)
        
        # Create listbox for compiler selection
        listbox_frame = ttk.Frame(dialog)
        listbox_frame.pack(fill=tk.BOTH, expand=True, padx=10, pady=5)
        
        listbox = tk.Listbox(listbox_frame)
        scrollbar = ttk.Scrollbar(listbox_frame, orient=tk.VERTICAL, command=listbox.yview)
        listbox.configure(yscrollcommand=scrollbar.set)
        
        for compiler in compilers:
            listbox.insert(tk.END, f"{compiler['command']} - {compiler['version']}")
        
        listbox.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
        scrollbar.pack(side=tk.RIGHT, fill=tk.Y)
        
        # Buttons
        button_frame = ttk.Frame(dialog)
        button_frame.pack(fill=tk.X, pady=10)
        
        def select_compiler():
            selection = listbox.curselection()
            if selection:
                selected_compiler = compilers[selection[0]]
                self.project_compilers[file_ext] = selected_compiler
                self._update_status(f"✅ Selected {selected_compiler['command']} for {file_ext}")
                dialog.destroy()
            else:
                messagebox.showwarning("Warning", "Please select a compiler")
        
        ttk.Button(button_frame, text="Select", command=select_compiler).pack(side=tk.LEFT, padx=5)
        ttk.Button(button_frame, text="Cancel", command=dialog.destroy).pack(side=tk.RIGHT, padx=5)
    
    def copy_tool_info(self):
        """Copy tool information to clipboard"""
        info_text = "n0mn0m IDE - Dynamic Compiler Detection Results\n\n"
        
        for command, info in self.detected_tools.items():
            info_text += f"{info['name']}: {info['version']}\n"
            info_text += f"  Path: {info['path']}\n"
            info_text += f"  Extensions: {', '.join(info['extensions']) if info['extensions'] else 'N/A'}\n\n"
        
        self.ide_instance.clipboard_clear()
        self.ide_instance.clipboard_append(info_text)
        self.ide_instance.update()
        
        self._update_status("📋 Tool information copied to clipboard")
        messagebox.showinfo("Success", "Tool information copied to clipboard!")
    
    def _update_status(self, message: str):
        """Update status display"""
        if hasattr(self, 'status_text') and self.status_text:
            self.status_text.config(state=tk.NORMAL)
            timestamp = time.strftime("%H:%M:%S")
            self.status_text.insert(tk.END, f"[{timestamp}] {message}\n")
            self.status_text.see(tk.END)
            self.status_text.config(state=tk.DISABLED)

def integrate_dynamic_compiler_loader(ide_instance):
    """Integrate dynamic compiler loader into the n0mn0m IDE"""
    dynamic_compiler = DynamicCompilerLoader(ide_instance)
    
    # Add to Tools menu
    if hasattr(ide_instance, 'menubar'):
        tools_menu = None
        for i in range(ide_instance.menubar.index(tk.END)):
            if ide_instance.menubar.entrycget(i, "label") == "Tools":
                tools_menu = ide_instance.menubar.winfo_children()[i]
                break
        
        if tools_menu:
            tools_menu.add_command(label="🔧 Dynamic Compiler Loader", 
                                 command=lambda: ide_instance.notebook.select(dynamic_compiler.ide_instance.notebook.children['!frame']))
    
    ide_instance.dynamic_compiler_loader = dynamic_compiler
    print("🔧 Dynamic Compiler Loader integrated with n0mn0m IDE!")
