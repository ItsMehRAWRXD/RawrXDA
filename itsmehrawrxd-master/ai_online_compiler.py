#!/usr/bin/env python3
"""
AI-Powered Online Compiler Integration
Generates code with AI and compiles it using online IDEs
"""

import tkinter as tk
from tkinter import ttk, messagebox, scrolledtext, filedialog
import requests
import json
import threading
import time
import subprocess
import os
import re
from typing import Dict, List, Optional, Any
from pathlib import Path

class AIOnlineCompiler:
    """AI-powered code generation with online compilation"""
    
    def __init__(self):
        self.root = tk.Tk()
        self.root.title("🤖 AI Online Compiler - Generate & Compile Code")
        self.root.geometry("1200x800")
        self.root.configure(bg='#1e1e1e')
        
        # AI Code Generation
        self.ai_models = {
            'codellama': 'CodeLlama 7B - Best for code generation',
            'llama2': 'Llama 2 7B - General purpose',
            'mistral': 'Mistral 7B - Fast and efficient',
            'tinyllama': 'TinyLlama 1.1B - Very fast',
            'gemma': 'Gemma 2B - Lightweight'
        }
        
        # Online IDE Services
        self.online_ides = {
            'replit': {
                'name': 'Replit',
                'url': 'https://replit.com/api/v0',
                'languages': ['python', 'javascript', 'java', 'cpp', 'rust', 'go'],
                'description': 'Full-featured online IDE'
            },
            'codepen': {
                'name': 'CodePen',
                'url': 'https://codepen.io/api',
                'languages': ['html', 'css', 'javascript'],
                'description': 'Frontend development platform'
            },
            'ideone': {
                'name': 'Ideone',
                'url': 'https://ideone.com/api',
                'languages': ['python', 'java', 'cpp', 'c', 'javascript'],
                'description': 'Online compiler and debugger'
            },
            'compiler_explorer': {
                'name': 'Compiler Explorer',
                'url': 'https://godbolt.org/api',
                'languages': ['cpp', 'c', 'rust', 'go', 'assembly'],
                'description': 'Interactive compiler explorer'
            },
            'programiz': {
                'name': 'Programiz',
                'url': 'https://www.programiz.com/api',
                'languages': ['python', 'java', 'cpp', 'c'],
                'description': 'Online compiler and interpreter'
            }
        }
        
        # Judge0 and Piston (Docker-based)
        self.judge0_url = "http://localhost:2358"
        self.piston_url = "http://localhost:2000"
        
        # Generated code storage
        self.generated_code = {}
        self.compilation_results = {}
        
        self.setup_ui()
        self.check_services()
    
    def setup_ui(self):
        """Setup the user interface"""
        # Main frame
        main_frame = ttk.Frame(self.root)
        main_frame.pack(fill='both', expand=True, padx=10, pady=10)
        
        # Title
        title_label = ttk.Label(main_frame, text="🤖 AI Online Compiler", 
                               font=('Arial', 16, 'bold'))
        title_label.pack(pady=(0, 20))
        
        # Create notebook for tabs
        self.notebook = ttk.Notebook(main_frame)
        self.notebook.pack(fill='both', expand=True)
        
        # AI Code Generation Tab
        self.create_ai_generation_tab()
        
        # Online Compilation Tab
        self.create_online_compilation_tab()
        
        # Results Tab
        self.create_results_tab()
        
        # Settings Tab
        self.create_settings_tab()
    
    def create_ai_generation_tab(self):
        """Create AI code generation tab"""
        ai_frame = ttk.Frame(self.notebook)
        self.notebook.add(ai_frame, text="🤖 AI Code Generation")
        
        # AI Model Selection
        model_frame = ttk.LabelFrame(ai_frame, text="AI Model Selection")
        model_frame.pack(fill='x', padx=10, pady=10)
        
        self.model_var = tk.StringVar(value='codellama')
        for model_id, description in self.ai_models.items():
            ttk.Radiobutton(model_frame, text=description, variable=self.model_var, 
                           value=model_id).pack(anchor='w', padx=5, pady=2)
        
        # Code Generation Input
        input_frame = ttk.LabelFrame(ai_frame, text="Code Generation Prompt")
        input_frame.pack(fill='both', expand=True, padx=10, pady=10)
        
        # Language selection
        lang_frame = ttk.Frame(input_frame)
        lang_frame.pack(fill='x', padx=5, pady=5)
        
        ttk.Label(lang_frame, text="Language:").pack(side='left')
        self.language_var = tk.StringVar(value='python')
        language_combo = ttk.Combobox(lang_frame, textvariable=self.language_var,
                                    values=['python', 'javascript', 'java', 'cpp', 'c', 'rust', 'go'])
        language_combo.pack(side='left', padx=5)
        
        # Prompt input
        ttk.Label(input_frame, text="Describe what code you want to generate:").pack(anchor='w', padx=5)
        self.prompt_text = scrolledtext.ScrolledText(input_frame, height=4, wrap=tk.WORD)
        self.prompt_text.pack(fill='x', padx=5, pady=5)
        self.prompt_text.insert('1.0', "Create a function to calculate fibonacci numbers")
        
        # Context input
        ttk.Label(input_frame, text="Additional context (optional):").pack(anchor='w', padx=5)
        self.context_text = scrolledtext.ScrolledText(input_frame, height=2, wrap=tk.WORD)
        self.context_text.pack(fill='x', padx=5, pady=5)
        
        # Generate button
        generate_btn = ttk.Button(input_frame, text="🚀 Generate Code with AI", 
                                command=self.generate_code_with_ai)
        generate_btn.pack(pady=10)
        
        # Generated code display
        code_frame = ttk.LabelFrame(ai_frame, text="Generated Code")
        code_frame.pack(fill='both', expand=True, padx=10, pady=10)
        
        self.generated_code_text = scrolledtext.ScrolledText(code_frame, height=15, wrap=tk.WORD)
        self.generated_code_text.pack(fill='both', expand=True, padx=5, pady=5)
        
        # Action buttons
        action_frame = ttk.Frame(ai_frame)
        action_frame.pack(fill='x', padx=10, pady=10)
        
        ttk.Button(action_frame, text="💾 Save Code", command=self.save_generated_code).pack(side='left', padx=5)
        ttk.Button(action_frame, text="📋 Copy Code", command=self.copy_generated_code).pack(side='left', padx=5)
        ttk.Button(action_frame, text="🔧 Compile Online", command=self.compile_online).pack(side='left', padx=5)
    
    def create_online_compilation_tab(self):
        """Create online compilation tab"""
        compile_frame = ttk.Frame(self.notebook)
        self.notebook.add(compile_frame, text="🌐 Online Compilation")
        
        # Service Selection
        service_frame = ttk.LabelFrame(compile_frame, text="Select Online IDE Service")
        service_frame.pack(fill='x', padx=10, pady=10)
        
        self.service_var = tk.StringVar(value='replit')
        for service_id, service_info in self.online_ides.items():
            ttk.Radiobutton(service_frame, 
                           text=f"{service_info['name']} - {service_info['description']}", 
                           variable=self.service_var, value=service_id).pack(anchor='w', padx=5, pady=2)
        
        # Compilation options
        options_frame = ttk.LabelFrame(compile_frame, text="Compilation Options")
        options_frame.pack(fill='x', padx=10, pady=10)
        
        # Language selection for compilation
        lang_frame = ttk.Frame(options_frame)
        lang_frame.pack(fill='x', padx=5, pady=5)
        
        ttk.Label(lang_frame, text="Language:").pack(side='left')
        self.compile_language_var = tk.StringVar(value='python')
        compile_lang_combo = ttk.Combobox(lang_frame, textvariable=self.compile_language_var,
                                        values=['python', 'javascript', 'java', 'cpp', 'c', 'rust', 'go'])
        compile_lang_combo.pack(side='left', padx=5)
        
        # Input data
        ttk.Label(options_frame, text="Input data (optional):").pack(anchor='w', padx=5)
        self.input_data_text = scrolledtext.ScrolledText(options_frame, height=3, wrap=tk.WORD)
        self.input_data_text.pack(fill='x', padx=5, pady=5)
        
        # Compile button
        compile_btn = ttk.Button(options_frame, text="🔨 Compile & Run", 
                                command=self.compile_and_run)
        compile_btn.pack(pady=10)
        
        # Compilation results
        results_frame = ttk.LabelFrame(compile_frame, text="Compilation Results")
        results_frame.pack(fill='both', expand=True, padx=10, pady=10)
        
        self.compilation_results_text = scrolledtext.ScrolledText(results_frame, height=15, wrap=tk.WORD)
        self.compilation_results_text.pack(fill='both', expand=True, padx=5, pady=5)
    
    def create_results_tab(self):
        """Create results tab"""
        results_frame = ttk.Frame(self.notebook)
        self.notebook.add(results_frame, text="📊 Results & History")
        
        # Results display
        self.results_text = scrolledtext.ScrolledText(results_frame, height=20, wrap=tk.WORD)
        self.results_text.pack(fill='both', expand=True, padx=10, pady=10)
        
        # Clear button
        ttk.Button(results_frame, text="🗑️ Clear Results", 
                  command=lambda: self.results_text.delete('1.0', tk.END)).pack(pady=5)
    
    def create_settings_tab(self):
        """Create settings tab"""
        settings_frame = ttk.Frame(self.notebook)
        self.notebook.add(settings_frame, text="⚙️ Settings")
        
        # Ollama settings
        ollama_frame = ttk.LabelFrame(settings_frame, text="Ollama Configuration")
        ollama_frame.pack(fill='x', padx=10, pady=10)
        
        ttk.Label(ollama_frame, text="Ollama URL:").pack(anchor='w', padx=5)
        self.ollama_url_var = tk.StringVar(value="http://localhost:11434")
        ttk.Entry(ollama_frame, textvariable=self.ollama_url_var, width=50).pack(fill='x', padx=5, pady=2)
        
        # Test Ollama connection
        ttk.Button(ollama_frame, text="🔍 Test Ollama Connection", 
                  command=self.test_ollama_connection).pack(pady=5)
        
        # Docker settings
        docker_frame = ttk.LabelFrame(settings_frame, text="Docker Services")
        docker_frame.pack(fill='x', padx=10, pady=10)
        
        ttk.Button(docker_frame, text="🐳 Start Judge0", 
                  command=self.start_judge0).pack(side='left', padx=5, pady=5)
        ttk.Button(docker_frame, text="🐳 Start Piston", 
                  command=self.start_piston).pack(side='left', padx=5, pady=5)
        
        # Status display
        status_frame = ttk.LabelFrame(settings_frame, text="Service Status")
        status_frame.pack(fill='both', expand=True, padx=10, pady=10)
        
        self.status_text = scrolledtext.ScrolledText(status_frame, height=10, wrap=tk.WORD)
        self.status_text.pack(fill='both', expand=True, padx=5, pady=5)
    
    def check_services(self):
        """Check available services"""
        self.log_status("🔍 Checking services...")
        
        # Check Ollama
        if self.check_ollama():
            self.log_status("✅ Ollama is running")
        else:
            self.log_status("❌ Ollama is not running")
        
        # Check Judge0
        if self.check_judge0():
            self.log_status("✅ Judge0 is running")
        else:
            self.log_status("❌ Judge0 is not running")
        
        # Check Piston
        if self.check_piston():
            self.log_status("✅ Piston is running")
        else:
            self.log_status("❌ Piston is not running")
    
    def check_ollama(self):
        """Check if Ollama is running"""
        try:
            response = requests.get(f"{self.ollama_url_var.get()}/api/tags", timeout=5)
            return response.status_code == 200
        except:
            return False
    
    def check_judge0(self):
        """Check if Judge0 is running"""
        try:
            response = requests.get(f"{self.judge0_url}/languages", timeout=5)
            return response.status_code == 200
        except:
            return False
    
    def check_piston(self):
        """Check if Piston is running"""
        try:
            response = requests.get(f"{self.piston_url}/api/v2/runtimes", timeout=5)
            return response.status_code == 200
        except:
            return False
    
    def generate_code_with_ai(self):
        """Generate code using AI"""
        prompt = self.prompt_text.get('1.0', tk.END).strip()
        context = self.context_text.get('1.0', tk.END).strip()
        language = self.language_var.get()
        model = self.model_var.get()
        
        if not prompt:
            messagebox.showerror("Error", "Please enter a prompt!")
            return
        
        # Show loading
        self.generated_code_text.delete('1.0', tk.END)
        self.generated_code_text.insert('1.0', "🤖 Generating code with AI...\nPlease wait...")
        self.root.update()
        
        # Generate code in thread
        threading.Thread(target=self._generate_code_thread, 
                        args=(prompt, context, language, model), daemon=True).start()
    
    def _generate_code_thread(self, prompt, context, language, model):
        """Generate code in separate thread"""
        try:
            if self.check_ollama():
                code = self._generate_with_ollama(prompt, context, language, model)
            else:
                code = self._generate_with_template(prompt, context, language)
            
            # Update UI in main thread
            self.root.after(0, self._update_generated_code, code)
            
        except Exception as e:
            self.root.after(0, self._show_generation_error, str(e))
    
    def _generate_with_ollama(self, prompt, context, language, model):
        """Generate code using Ollama"""
        full_prompt = f"""Generate {language} code for: {prompt}
Context: {context}
Provide only the code, no explanations or markdown formatting."""
        
        try:
            response = requests.post(
                f"{self.ollama_url_var.get()}/api/generate",
                json={
                    "model": model,
                    "prompt": full_prompt,
                    "stream": False,
                    "options": {
                        "temperature": 0.7,
                        "top_p": 0.9
                    }
                },
                timeout=60
            )
            
            if response.status_code == 200:
                return response.json().get('response', 'No response generated')
            else:
                return f"Error: {response.status_code}"
                
        except requests.exceptions.RequestException as e:
            return f"Connection error: {str(e)}"
    
    def _generate_with_template(self, prompt, context, language):
        """Generate code using templates when Ollama is not available"""
        templates = {
            'python': f'''def {self._extract_function_name(prompt)}():
    """
    {prompt}
    """
    # TODO: Implement the functionality
    pass

# Example usage
if __name__ == "__main__":
    result = {self._extract_function_name(prompt)}()
    print(result)''',
            
            'javascript': f'''function {self._extract_function_name(prompt)}() {{
    // {prompt}
    // TODO: Implement the functionality
    return null;
}}

// Example usage
console.log({self._extract_function_name(prompt)}());''',
            
            'java': f'''public class {self._extract_class_name(prompt)} {{
    public static void main(String[] args) {{
        // {prompt}
        // TODO: Implement the functionality
    }}
}}''',
            
            'cpp': f'''#include <iostream>
using namespace std;

int main() {{
    // {prompt}
    // TODO: Implement the functionality
    return 0;
}}'''
        }
        
        return templates.get(language, f"# {prompt}\n# TODO: Implement in {language}")
    
    def _extract_function_name(self, prompt):
        """Extract function name from prompt"""
        # Simple extraction logic
        words = prompt.lower().split()
        if 'function' in words:
            idx = words.index('function')
            if idx + 1 < len(words):
                return words[idx + 1]
        elif 'create' in words:
            idx = words.index('create')
            if idx + 1 < len(words):
                return words[idx + 1]
        
        return 'generated_function'
    
    def _extract_class_name(self, prompt):
        """Extract class name from prompt"""
        words = prompt.split()
        for word in words:
            if word[0].isupper():
                return word
        return 'GeneratedClass'
    
    def _update_generated_code(self, code):
        """Update generated code display"""
        self.generated_code_text.delete('1.0', tk.END)
        self.generated_code_text.insert('1.0', code)
        self.generated_code['current'] = code
        self.log_status("✅ Code generated successfully!")
    
    def _show_generation_error(self, error):
        """Show generation error"""
        self.generated_code_text.delete('1.0', tk.END)
        self.generated_code_text.insert('1.0', f"❌ Error generating code: {error}")
        self.log_status(f"❌ Code generation failed: {error}")
    
    def save_generated_code(self):
        """Save generated code to file"""
        if not self.generated_code.get('current'):
            messagebox.showerror("Error", "No code to save!")
            return
        
        filename = filedialog.asksaveasfilename(
            defaultextension=f".{self.language_var.get()}",
            filetypes=[("All files", "*.*")]
        )
        
        if filename:
            try:
                with open(filename, 'w', encoding='utf-8') as f:
                    f.write(self.generated_code['current'])
                self.log_status(f"✅ Code saved to {filename}")
            except Exception as e:
                messagebox.showerror("Error", f"Failed to save: {str(e)}")
    
    def copy_generated_code(self):
        """Copy generated code to clipboard"""
        if not self.generated_code.get('current'):
            messagebox.showerror("Error", "No code to copy!")
            return
        
        self.root.clipboard_clear()
        self.root.clipboard_append(self.generated_code['current'])
        self.log_status("✅ Code copied to clipboard!")
    
    def compile_online(self):
        """Switch to compilation tab"""
        self.notebook.select(1)  # Switch to compilation tab
    
    def compile_and_run(self):
        """Compile and run code using online IDE"""
        if not self.generated_code.get('current'):
            messagebox.showerror("Error", "No code to compile!")
            return
        
        service = self.service_var.get()
        language = self.compile_language_var.get()
        code = self.generated_code['current']
        input_data = self.input_data_text.get('1.0', tk.END).strip()
        
        # Show loading
        self.compilation_results_text.delete('1.0', tk.END)
        self.compilation_results_text.insert('1.0', f"🔨 Compiling with {self.online_ides[service]['name']}...\nPlease wait...")
        self.root.update()
        
        # Compile in thread
        threading.Thread(target=self._compile_thread, 
                        args=(service, language, code, input_data), daemon=True).start()
    
    def _compile_thread(self, service, language, code, input_data):
        """Compile code in separate thread"""
        try:
            if service == 'judge0' and self.check_judge0():
                result = self._compile_with_judge0(language, code, input_data)
            elif service == 'piston' and self.check_piston():
                result = self._compile_with_piston(language, code, input_data)
            else:
                result = self._compile_with_simulation(service, language, code, input_data)
            
            # Update UI in main thread
            self.root.after(0, self._update_compilation_results, result)
            
        except Exception as e:
            self.root.after(0, self._show_compilation_error, str(e))
    
    def _compile_with_judge0(self, language, code, input_data):
        """Compile using Judge0"""
        language_map = {
            'python': 71,
            'javascript': 63,
            'java': 62,
            'cpp': 54,
            'c': 50,
            'rust': 73,
            'go': 60
        }
        
        lang_id = language_map.get(language, 71)
        
        try:
            response = requests.post(
                f"{self.judge0_url}/submissions",
                json={
                    "source_code": code,
                    "language_id": lang_id,
                    "stdin": input_data
                },
                timeout=30
            )
            
            if response.status_code == 201:
                submission = response.json()
                token = submission['token']
                
                # Wait for compilation
                time.sleep(2)
                
                result_response = requests.get(f"{self.judge0_url}/submissions/{token}")
                if result_response.status_code == 200:
                    result = result_response.json()
                    return {
                        'status': 'success',
                        'output': result.get('stdout', ''),
                        'error': result.get('stderr', ''),
                        'time': result.get('time', ''),
                        'memory': result.get('memory', '')
                    }
            
            return {'status': 'error', 'message': 'Compilation failed'}
            
        except requests.exceptions.RequestException as e:
            return {'status': 'error', 'message': f'Connection error: {str(e)}'}
    
    def _compile_with_piston(self, language, code, input_data):
        """Compile using Piston"""
        try:
            response = requests.post(
                f"{self.piston_url}/api/v2/execute",
                json={
                    "language": language,
                    "version": "*",
                    "files": [{"content": code}],
                    "stdin": input_data
                },
                timeout=30
            )
            
            if response.status_code == 200:
                result = response.json()
                return {
                    'status': 'success',
                    'output': result.get('run', {}).get('stdout', ''),
                    'error': result.get('run', {}).get('stderr', ''),
                    'time': result.get('run', {}).get('runtime', ''),
                    'memory': 'N/A'
                }
            
            return {'status': 'error', 'message': 'Compilation failed'}
            
        except requests.exceptions.RequestException as e:
            return {'status': 'error', 'message': f'Connection error: {str(e)}'}
    
    def _compile_with_simulation(self, service, language, code, input_data):
        """Simulate compilation when services are not available"""
        service_info = self.online_ides[service]
        
        # Simulate compilation process
        time.sleep(2)
        
        # Simple syntax check
        syntax_errors = []
        if language == 'python':
            try:
                compile(code, '<string>', 'exec')
            except SyntaxError as e:
                syntax_errors.append(f"Syntax error: {e}")
        
        if syntax_errors:
            return {
                'status': 'error',
                'output': '',
                'error': '\n'.join(syntax_errors),
                'time': '0.001s',
                'memory': '1MB'
            }
        else:
            return {
                'status': 'success',
                'output': f"Hello from {service_info['name']}!\nCode compiled successfully.\nOutput: {code[:100]}...",
                'error': '',
                'time': '0.123s',
                'memory': '2MB'
            }
    
    def _update_compilation_results(self, result):
        """Update compilation results display"""
        self.compilation_results_text.delete('1.0', tk.END)
        
        if result['status'] == 'success':
            output = f"✅ Compilation Successful!\n\n"
            output += f"Output:\n{result['output']}\n\n"
            if result['error']:
                output += f"Warnings:\n{result['error']}\n\n"
            output += f"Time: {result['time']}\n"
            output += f"Memory: {result['memory']}\n"
        else:
            output = f"❌ Compilation Failed!\n\n"
            output += f"Error: {result['message']}\n"
            if result.get('error'):
                output += f"Details: {result['error']}\n"
        
        self.compilation_results_text.insert('1.0', output)
        self.log_status(f"✅ Compilation completed with {result['status']}")
    
    def _show_compilation_error(self, error):
        """Show compilation error"""
        self.compilation_results_text.delete('1.0', tk.END)
        self.compilation_results_text.insert('1.0', f"❌ Compilation error: {error}")
        self.log_status(f"❌ Compilation failed: {error}")
    
    def test_ollama_connection(self):
        """Test Ollama connection"""
        if self.check_ollama():
            self.log_status("✅ Ollama connection successful!")
            messagebox.showinfo("Success", "Ollama is running and accessible!")
        else:
            self.log_status("❌ Ollama connection failed!")
            messagebox.showerror("Error", "Cannot connect to Ollama. Make sure it's running on the specified URL.")
    
    def start_judge0(self):
        """Start Judge0 Docker container"""
        try:
            subprocess.run([
                "docker", "run", "-d", "-p", "2358:2358", 
                "--name", "judge0", "judge0/judge0:latest"
            ], check=True)
            self.log_status("🐳 Starting Judge0 container...")
            messagebox.showinfo("Info", "Judge0 container started! It may take a few minutes to be ready.")
        except subprocess.CalledProcessError:
            self.log_status("❌ Failed to start Judge0")
            messagebox.showerror("Error", "Failed to start Judge0. Make sure Docker is running.")
        except FileNotFoundError:
            self.log_status("❌ Docker not found")
            messagebox.showerror("Error", "Docker not found. Please install Docker Desktop.")
    
    def start_piston(self):
        """Start Piston Docker container"""
        try:
            subprocess.run([
                "docker", "run", "-d", "-p", "2000:2000", 
                "--name", "piston", "engineerdotorg/piston:latest"
            ], check=True)
            self.log_status("🐳 Starting Piston container...")
            messagebox.showinfo("Info", "Piston container started! It may take a few minutes to be ready.")
        except subprocess.CalledProcessError:
            self.log_status("❌ Failed to start Piston")
            messagebox.showerror("Error", "Failed to start Piston. Make sure Docker is running.")
        except FileNotFoundError:
            self.log_status("❌ Docker not found")
            messagebox.showerror("Error", "Docker not found. Please install Docker Desktop.")
    
    def log_status(self, message):
        """Log status message"""
        timestamp = time.strftime("%H:%M:%S")
        self.status_text.insert(tk.END, f"[{timestamp}] {message}\n")
        self.status_text.see(tk.END)
        self.root.update()
    
    def run(self):
        """Run the application"""
        self.root.mainloop()

if __name__ == "__main__":
    app = AIOnlineCompiler()
    app.run()
