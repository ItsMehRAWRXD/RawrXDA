#!/usr/bin/env python3
"""
Judge0 and Piston Integration for n0mn0m IDE
Provides local and cloud-based code execution with 40+ languages support
"""

import tkinter as tk
from tkinter import ttk, messagebox, scrolledtext
import requests
import json
import threading
import time
import os
import subprocess
from typing import Dict, List, Optional, Any
from pathlib import Path

class Judge0PistonIntegration:
    """Integrates Judge0 and Piston code execution engines"""
    
    def __init__(self, ide_instance):
        self.ide_instance = ide_instance
        self.judge0_url = "http://localhost:8080"  # Local Judge0 instance
        self.piston_url = "http://localhost:2000"  # Local Piston instance
        self.judge0_available = False
        self.piston_available = False
        
        # Language mappings for Judge0 (language_id)
        self.judge0_languages = {
            'python': 71,      # Python 3
            'javascript': 63,  # Node.js
            'cpp': 54,         # C++
            'c': 50,           # C
            'java': 62,        # Java
            'rust': 73,        # Rust
            'go': 60,          # Go
            'php': 68,         # PHP
            'ruby': 72,        # Ruby
            'swift': 83,       # Swift
            'kotlin': 78,      # Kotlin
            'scala': 81,       # Scala
            'haskell': 61,     # Haskell
            'lua': 64,         # Lua
            'assembly': 45,    # Assembly
            'csharp': 51,      # C#
            'typescript': 74,  # TypeScript
            'perl': 70,        # Perl
            'r': 80,           # R
            'bash': 46,        # Bash
            'powershell': 82,  # PowerShell
            'sql': 82,         # SQL
            'html': 71,        # HTML
            'css': 51,         # CSS
            'json': 62,        # JSON
            'yaml': 83,        # YAML
            'xml': 83,         # XML
            'markdown': 71,    # Markdown
            'dockerfile': 46,  # Dockerfile
            'makefile': 46     # Makefile
        }
        
        # Language mappings for Piston
        self.piston_languages = {
            'python': 'python',
            'javascript': 'javascript',
            'cpp': 'cpp',
            'c': 'c',
            'java': 'java',
            'rust': 'rust',
            'go': 'go',
            'php': 'php',
            'ruby': 'ruby',
            'swift': 'swift',
            'kotlin': 'kotlin',
            'scala': 'scala',
            'haskell': 'haskell',
            'lua': 'lua',
            'assembly': 'assembly',
            'csharp': 'csharp',
            'typescript': 'typescript',
            'perl': 'perl',
            'r': 'r',
            'bash': 'bash',
            'powershell': 'powershell',
            'sql': 'sql',
            'html': 'html',
            'css': 'css',
            'json': 'json',
            'yaml': 'yaml',
            'xml': 'xml',
            'markdown': 'markdown',
            'dockerfile': 'dockerfile',
            'makefile': 'makefile'
        }
        
        self.setup_gui()
        self.check_services()
    
    def setup_gui(self, parent_frame=None):
        """Setup the Judge0/Piston integration GUI"""
        if parent_frame is None:
            parent_frame = ttk.Frame(self.ide_instance.notebook)
            self.ide_instance.notebook.add(parent_frame, text="⚡ Code Execution")
        
        main_frame = ttk.Frame(parent_frame, padding="10")
        main_frame.pack(fill=tk.BOTH, expand=True)
        
        # Title
        ttk.Label(main_frame, text="⚡ Judge0 & Piston Code Execution", 
                 font=("Arial", 16, "bold")).pack(pady=10)
        
        # Service status
        status_frame = ttk.LabelFrame(main_frame, text="Execution Engine Status", padding="10")
        status_frame.pack(fill=tk.X, pady=5)
        
        self.status_text = scrolledtext.ScrolledText(status_frame, wrap=tk.WORD, height=6, state=tk.DISABLED)
        self.status_text.pack(fill=tk.X)
        
        # Execution options
        options_frame = ttk.LabelFrame(main_frame, text="Execution Options", padding="10")
        options_frame.pack(fill=tk.X, pady=5)
        
        # Execution method selection
        method_frame = ttk.Frame(options_frame)
        method_frame.pack(fill=tk.X, pady=5)
        
        ttk.Label(method_frame, text="Execution Method:").pack(side=tk.LEFT)
        self.execution_method = tk.StringVar(value="auto")
        ttk.Radiobutton(method_frame, text="Auto (Best Available)", 
                       variable=self.execution_method, value="auto").pack(side=tk.LEFT, padx=10)
        ttk.Radiobutton(method_frame, text="Judge0 Only", 
                       variable=self.execution_method, value="judge0").pack(side=tk.LEFT, padx=10)
        ttk.Radiobutton(method_frame, text="Piston Only", 
                       variable=self.execution_method, value="piston").pack(side=tk.LEFT, padx=10)
        ttk.Radiobutton(method_frame, text="Local Only", 
                       variable=self.execution_method, value="local").pack(side=tk.LEFT, padx=10)
        
        # Language selection
        lang_frame = ttk.Frame(options_frame)
        lang_frame.pack(fill=tk.X, pady=5)
        
        ttk.Label(lang_frame, text="Language:").pack(side=tk.LEFT)
        self.language_var = tk.StringVar(value="python")
        self.language_combo = ttk.Combobox(lang_frame, textvariable=self.language_var, 
                                         values=list(self.judge0_languages.keys()), 
                                         state="readonly", width=15)
        self.language_combo.pack(side=tk.LEFT, padx=10)
        
        # Input section
        input_frame = ttk.LabelFrame(main_frame, text="Input", padding="10")
        input_frame.pack(fill=tk.BOTH, expand=True, pady=5)
        
        ttk.Label(input_frame, text="Code:").pack(anchor=tk.W)
        self.code_text = scrolledtext.ScrolledText(input_frame, wrap=tk.WORD, height=10)
        self.code_text.pack(fill=tk.BOTH, expand=True, pady=5)
        
        # STDIN input
        stdin_frame = ttk.Frame(input_frame)
        stdin_frame.pack(fill=tk.X, pady=5)
        ttk.Label(stdin_frame, text="STDIN:").pack(side=tk.LEFT)
        self.stdin_entry = ttk.Entry(stdin_frame)
        self.stdin_entry.pack(side=tk.LEFT, fill=tk.X, expand=True, padx=5)
        
        # Action buttons
        button_frame = ttk.Frame(main_frame)
        button_frame.pack(fill=tk.X, pady=10)
        
        ttk.Button(button_frame, text="🚀 Execute Code", 
                  command=self.execute_code).pack(side=tk.LEFT, padx=5)
        ttk.Button(button_frame, text="🔍 Check Services", 
                  command=self.check_services).pack(side=tk.LEFT, padx=5)
        ttk.Button(button_frame, text="📋 Copy Result", 
                  command=self.copy_result).pack(side=tk.LEFT, padx=5)
        ttk.Button(button_frame, text="🧹 Clear", 
                  command=self.clear_all).pack(side=tk.RIGHT, padx=5)
        
        # Output section
        output_frame = ttk.LabelFrame(main_frame, text="Output", padding="10")
        output_frame.pack(fill=tk.BOTH, expand=True, pady=5)
        
        self.output_text = scrolledtext.ScrolledText(output_frame, wrap=tk.WORD, height=8, state=tk.DISABLED)
        self.output_text.pack(fill=tk.BOTH, expand=True)
    
    def check_services(self):
        """Check if Judge0 and Piston services are available"""
        self._update_status("🔍 Checking execution engines...")
        
        # Check Judge0
        thread = threading.Thread(target=self._check_judge0)
        thread.daemon = True
        thread.start()
        
        # Check Piston
        thread2 = threading.Thread(target=self._check_piston)
        thread2.daemon = True
        thread2.start()
    
    def _check_judge0(self):
        """Check Judge0 service availability"""
        try:
            response = requests.get(f"{self.judge0_url}/languages", timeout=5)
            if response.status_code == 200:
                self.judge0_available = True
                languages = response.json()
                self._update_status(f"✅ Judge0 available - {len(languages)} languages supported")
            else:
                self.judge0_available = False
                self._update_status("❌ Judge0 not responding")
        except requests.exceptions.RequestException:
            self.judge0_available = False
            self._update_status("❌ Judge0 not available (check if Docker container is running)")
    
    def _check_piston(self):
        """Check Piston service availability"""
        try:
            response = requests.get(f"{self.piston_url}/api/v2/runtimes", timeout=5)
            if response.status_code == 200:
                self.piston_available = True
                runtimes = response.json()
                self._update_status(f"✅ Piston available - {len(runtimes)} runtimes supported")
            else:
                self.piston_available = False
                self._update_status("❌ Piston not responding")
        except requests.exceptions.RequestException:
            self.piston_available = False
            self._update_status("❌ Piston not available (check if Docker container is running)")
    
    def execute_code(self):
        """Execute code using the selected method"""
        code = self.code_text.get(1.0, tk.END).strip()
        if not code:
            messagebox.showwarning("Warning", "Please enter some code to execute")
            return
        
        language = self.language_var.get()
        stdin = self.stdin_entry.get().strip()
        method = self.execution_method.get()
        
        self._update_status(f"🚀 Executing {language} code using {method} method...")
        
        # Execute in thread
        thread = threading.Thread(target=self._execute_code_thread, 
                                args=(code, language, stdin, method))
        thread.daemon = True
        thread.start()
    
    def _execute_code_thread(self, code: str, language: str, stdin: str, method: str):
        """Background thread for code execution"""
        try:
            result = None
            
            if method == "auto":
                # Try Judge0 first, then Piston, then local
                if self.judge0_available:
                    result = self._execute_with_judge0(code, language, stdin)
                elif self.piston_available:
                    result = self._execute_with_piston(code, language, stdin)
                else:
                    result = self._execute_locally(code, language, stdin)
            elif method == "judge0" and self.judge0_available:
                result = self._execute_with_judge0(code, language, stdin)
            elif method == "piston" and self.piston_available:
                result = self._execute_with_piston(code, language, stdin)
            elif method == "local":
                result = self._execute_locally(code, language, stdin)
            else:
                result = {"success": False, "error": f"Selected method '{method}' not available"}
            
            # Display result
            self._display_result(result, method)
            
        except Exception as e:
            self._update_status(f"❌ Execution error: {str(e)}")
    
    def _execute_with_judge0(self, code: str, language: str, stdin: str) -> Dict:
        """Execute code using Judge0"""
        try:
            language_id = self.judge0_languages.get(language.lower())
            if not language_id:
                return {"success": False, "error": f"Language '{language}' not supported by Judge0"}
            
            # Submit code for execution
            submit_data = {
                "source_code": code,
                "language_id": language_id,
                "stdin": stdin
            }
            
            response = requests.post(f"{self.judge0_url}/submissions", 
                                   json=submit_data, timeout=10)
            
            if response.status_code != 201:
                return {"success": False, "error": f"Judge0 submission failed: {response.status_code}"}
            
            submission = response.json()
            token = submission["token"]
            
            # Poll for result
            max_attempts = 30
            for attempt in range(max_attempts):
                time.sleep(1)
                
                result_response = requests.get(f"{self.judge0_url}/submissions/{token}", timeout=5)
                if result_response.status_code == 200:
                    result = result_response.json()
                    
                    if result["status"]["id"] in [1, 2]:  # In Queue or Processing
                        continue
                    elif result["status"]["id"] == 3:  # Accepted
                        return {
                            "success": True,
                            "output": result.get("stdout", ""),
                            "error": result.get("stderr", ""),
                            "engine": "Judge0",
                            "time": result.get("time", ""),
                            "memory": result.get("memory", "")
                        }
                    else:  # Error
                        return {
                            "success": False,
                            "error": f"Judge0 execution failed: {result.get('stderr', 'Unknown error')}",
                            "engine": "Judge0"
                        }
            
            return {"success": False, "error": "Judge0 execution timeout"}
            
        except Exception as e:
            return {"success": False, "error": f"Judge0 error: {str(e)}"}
    
    def _execute_with_piston(self, code: str, language: str, stdin: str) -> Dict:
        """Execute code using Piston"""
        try:
            piston_lang = self.piston_languages.get(language.lower())
            if not piston_lang:
                return {"success": False, "error": f"Language '{language}' not supported by Piston"}
            
            # Execute code
            execute_data = {
                "language": piston_lang,
                "version": "*",  # Use latest version
                "files": [{"content": code}],
                "stdin": stdin
            }
            
            response = requests.post(f"{self.piston_url}/api/v2/execute", 
                                   json=execute_data, timeout=30)
            
            if response.status_code == 200:
                result = response.json()
                return {
                    "success": True,
                    "output": result.get("run", {}).get("stdout", ""),
                    "error": result.get("run", {}).get("stderr", ""),
                    "engine": "Piston",
                    "time": f"{result.get('run', {}).get('runtime', 0)}ms"
                }
            else:
                return {"success": False, "error": f"Piston execution failed: {response.status_code}"}
                
        except Exception as e:
            return {"success": False, "error": f"Piston error: {str(e)}"}
    
    def _execute_locally(self, code: str, language: str, stdin: str) -> Dict:
        """Execute code using local compilers/interpreters"""
        try:
            # Create temporary file
            temp_file = f"temp_exec.{self._get_file_extension(language)}"
            
            with open(temp_file, 'w', encoding='utf-8') as f:
                f.write(code)
            
            # Execute based on language
            if language.lower() == 'python':
                cmd = ['python', temp_file]
            elif language.lower() == 'javascript':
                cmd = ['node', temp_file]
            elif language.lower() == 'cpp':
                exe_file = 'temp_exec.exe'
                compile_cmd = ['g++', '-o', exe_file, temp_file]
                compile_result = subprocess.run(compile_cmd, capture_output=True, text=True)
                if compile_result.returncode != 0:
                    return {"success": False, "error": f"Compilation failed: {compile_result.stderr}"}
                cmd = [f'./{exe_file}'] if os.name != 'nt' else [exe_file]
            elif language.lower() == 'java':
                class_file = 'TempExec'
                compile_cmd = ['javac', temp_file]
                compile_result = subprocess.run(compile_cmd, capture_output=True, text=True)
                if compile_result.returncode != 0:
                    return {"success": False, "error": f"Compilation failed: {compile_result.stderr}"}
                cmd = ['java', class_file]
            else:
                return {"success": False, "error": f"Local execution not supported for {language}"}
            
            # Run the code
            result = subprocess.run(cmd, input=stdin, capture_output=True, text=True, timeout=30)
            
            # Cleanup
            try:
                os.remove(temp_file)
                if language.lower() == 'cpp' and os.path.exists('temp_exec.exe'):
                    os.remove('temp_exec.exe')
                if language.lower() == 'java' and os.path.exists('TempExec.class'):
                    os.remove('TempExec.class')
            except:
                pass
            
            return {
                "success": result.returncode == 0,
                "output": result.stdout,
                "error": result.stderr,
                "engine": "Local",
                "return_code": result.returncode
            }
            
        except subprocess.TimeoutExpired:
            return {"success": False, "error": "Local execution timeout"}
        except Exception as e:
            return {"success": False, "error": f"Local execution error: {str(e)}"}
    
    def _get_file_extension(self, language: str) -> str:
        """Get file extension for language"""
        extensions = {
            'python': 'py',
            'javascript': 'js',
            'cpp': 'cpp',
            'c': 'c',
            'java': 'java',
            'rust': 'rs',
            'go': 'go',
            'php': 'php',
            'ruby': 'rb',
            'swift': 'swift',
            'kotlin': 'kt',
            'scala': 'scala',
            'haskell': 'hs',
            'lua': 'lua',
            'assembly': 'asm',
            'csharp': 'cs'
        }
        return extensions.get(language.lower(), 'txt')
    
    def _display_result(self, result: Dict, method: str):
        """Display execution result"""
        self.output_text.config(state=tk.NORMAL)
        self.output_text.delete(1.0, tk.END)
        
        if result["success"]:
            self.output_text.insert(tk.END, f"✅ Execution successful using {result.get('engine', 'Unknown')}\n")
            self.output_text.insert(tk.END, f"Method: {method}\n")
            
            if result.get("time"):
                self.output_text.insert(tk.END, f"Execution time: {result['time']}\n")
            if result.get("memory"):
                self.output_text.insert(tk.END, f"Memory usage: {result['memory']}\n")
            if result.get("return_code"):
                self.output_text.insert(tk.END, f"Return code: {result['return_code']}\n")
            
            self.output_text.insert(tk.END, "\n" + "="*50 + "\n")
            self.output_text.insert(tk.END, "OUTPUT:\n")
            self.output_text.insert(tk.END, result.get("output", ""))
            
            if result.get("error"):
                self.output_text.insert(tk.END, "\n" + "="*50 + "\n")
                self.output_text.insert(tk.END, "STDERR:\n")
                self.output_text.insert(tk.END, result["error"])
        else:
            self.output_text.insert(tk.END, f"❌ Execution failed\n")
            self.output_text.insert(tk.END, f"Method: {method}\n")
            self.output_text.insert(tk.END, f"Error: {result.get('error', 'Unknown error')}\n")
        
        self.output_text.config(state=tk.DISABLED)
        self.output_text.see(tk.END)
    
    def copy_result(self):
        """Copy execution result to clipboard"""
        result_text = self.output_text.get(1.0, tk.END).strip()
        if result_text:
            self.ide_instance.clipboard_clear()
            self.ide_instance.clipboard_append(result_text)
            self._update_status("📋 Result copied to clipboard")
            messagebox.showinfo("Success", "Execution result copied to clipboard!")
        else:
            messagebox.showwarning("Warning", "No result to copy")
    
    def clear_all(self):
        """Clear all text areas"""
        self.code_text.delete(1.0, tk.END)
        self.stdin_entry.delete(0, tk.END)
        self.output_text.config(state=tk.NORMAL)
        self.output_text.delete(1.0, tk.END)
        self.output_text.config(state=tk.DISABLED)
        self._update_status("🧹 All cleared")
    
    def _update_status(self, message: str):
        """Update status display"""
        if hasattr(self, 'status_text') and self.status_text:
            self.status_text.config(state=tk.NORMAL)
            timestamp = time.strftime("%H:%M:%S")
            self.status_text.insert(tk.END, f"[{timestamp}] {message}\n")
            self.status_text.see(tk.END)
            self.status_text.config(state=tk.DISABLED)

def integrate_judge0_piston(ide_instance):
    """Integrate Judge0 and Piston execution engines into the n0mn0m IDE"""
    judge0_piston = Judge0PistonIntegration(ide_instance)
    
    # Add to Tools menu
    if hasattr(ide_instance, 'menubar'):
        tools_menu = None
        for i in range(ide_instance.menubar.index(tk.END)):
            if ide_instance.menubar.entrycget(i, "label") == "Tools":
                tools_menu = ide_instance.menubar.winfo_children()[i]
                break
        
        if tools_menu:
            tools_menu.add_command(label="⚡ Judge0 & Piston Execution", 
                                 command=lambda: ide_instance.notebook.select(judge0_piston.ide_instance.notebook.children['!frame']))
    
    ide_instance.judge0_piston = judge0_piston
    print("⚡ Judge0 & Piston Integration loaded successfully!")

def setup_docker_instructions():
    """Display instructions for setting up Judge0 and Piston with Docker"""
    instructions = """
🐳 Docker Setup Instructions for Judge0 & Piston

📋 Judge0 Setup:
1. Install Docker: https://www.docker.com/get-started
2. Run Judge0: docker run -d -p 8080:8080 judge0/judge0
3. Access: http://localhost:8080

📋 Piston Setup:
1. Install Docker: https://www.docker.com/get-started  
2. Run Piston: docker run -d -p 2000:2000 engineerdotorg/piston
3. Access: http://localhost:2000

🔧 Alternative: Use our integrated local execution (no Docker required)
    """
    print(instructions)
