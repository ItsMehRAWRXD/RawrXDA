#!/usr/bin/env python3
"""
Local AI Model Manager for n0mn0m IDE
Handles downloading, managing, and using local AI models with Ollama
"""

import tkinter as tk
from tkinter import ttk, messagebox, scrolledtext
import requests
import json
import threading
import time
import subprocess
import os
import sys
from typing import Dict, List, Optional, Any
from pathlib import Path

class LocalAIModelManager:
    """Manages local AI models using Ollama"""
    
    def __init__(self, ide_instance):
        self.ide_instance = ide_instance
        self.ollama_url = "http://localhost:11434"
        self.ollama_available = False
        self.available_models = []
        self.running_models = {}
        
        # Popular AI models to download
        self.recommended_models = {
            "llama3.2": {
                "name": "Llama 3.2",
                "size": "3.8GB",
                "description": "Meta's latest Llama model - great for coding and general tasks"
            },
            "llama3.2:3b": {
                "name": "Llama 3.2 3B",
                "size": "2GB", 
                "description": "Smaller Llama model - faster, less memory"
            },
            "codellama": {
                "name": "Code Llama",
                "size": "3.8GB",
                "description": "Specialized for coding tasks"
            },
            "codellama:7b": {
                "name": "Code Llama 7B",
                "size": "3.8GB",
                "description": "Code Llama 7B - good balance of performance and speed"
            },
            "mistral": {
                "name": "Mistral 7B",
                "size": "4.1GB",
                "description": "High-performance model from Mistral AI"
            },
            "phi3": {
                "name": "Phi-3",
                "size": "2.3GB",
                "description": "Microsoft's efficient model"
            },
            "gemma": {
                "name": "Gemma 2B",
                "size": "1.6GB",
                "description": "Google's lightweight model"
            },
            "qwen": {
                "name": "Qwen 2.5",
                "size": "1.5GB",
                "description": "Alibaba's efficient model"
            },
            "deepseek-coder": {
                "name": "DeepSeek Coder",
                "size": "6.7GB",
                "description": "Specialized for programming tasks"
            },
            "starling-lm": {
                "name": "Starling LM",
                "size": "3.8GB",
                "description": "Helpful assistant model"
            }
        }
        
        self.setup_gui()
        self.check_ollama()
    
    def setup_gui(self, parent_frame=None):
        """Setup the local AI model manager GUI"""
        if parent_frame is None:
            parent_frame = ttk.Frame(self.ide_instance.notebook)
            self.ide_instance.notebook.add(parent_frame, text="🤖 Local AI Models")
        
        main_frame = ttk.Frame(parent_frame, padding="10")
        main_frame.pack(fill=tk.BOTH, expand=True)
        
        # Title
        ttk.Label(main_frame, text="🤖 Local AI Model Manager", 
                 font=("Arial", 16, "bold")).pack(pady=10)
        
        # Ollama status
        status_frame = ttk.LabelFrame(main_frame, text="Ollama Status", padding="10")
        status_frame.pack(fill=tk.X, pady=5)
        
        self.status_text = scrolledtext.ScrolledText(status_frame, wrap=tk.WORD, height=4, state=tk.DISABLED)
        self.status_text.pack(fill=tk.X)
        
        # Model management
        model_frame = ttk.LabelFrame(main_frame, text="Model Management", padding="10")
        model_frame.pack(fill=tk.BOTH, expand=True, pady=5)
        
        # Model list
        list_frame = ttk.Frame(model_frame)
        list_frame.pack(fill=tk.BOTH, expand=True)
        
        columns = ('Model', 'Size', 'Status')
        self.model_tree = ttk.Treeview(list_frame, columns=columns, show='headings', height=8)
        
        for col in columns:
            self.model_tree.heading(col, text=col)
            self.model_tree.column(col, width=150)
        
        tree_scroll = ttk.Scrollbar(list_frame, orient=tk.VERTICAL, command=self.model_tree.yview)
        self.model_tree.configure(yscrollcommand=tree_scroll.set)
        
        self.model_tree.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
        tree_scroll.pack(side=tk.RIGHT, fill=tk.Y)
        
        # Action buttons
        button_frame = ttk.Frame(model_frame)
        button_frame.pack(fill=tk.X, pady=10)
        
        ttk.Button(button_frame, text="📥 Pull Model", 
                  command=self.pull_model_dialog).pack(side=tk.LEFT, padx=5)
        ttk.Button(button_frame, text="🗑️ Remove Model", 
                  command=self.remove_model).pack(side=tk.LEFT, padx=5)
        ttk.Button(button_frame, text="🚀 Start Model", 
                  command=self.start_model).pack(side=tk.LEFT, padx=5)
        ttk.Button(button_frame, text="⏹️ Stop Model", 
                  command=self.stop_model).pack(side=tk.LEFT, padx=5)
        ttk.Button(button_frame, text="🔄 Refresh", 
                  command=self.refresh_models).pack(side=tk.RIGHT, padx=5)
        
        # Quick install section
        install_frame = ttk.LabelFrame(main_frame, text="Quick Install Popular Models", padding="10")
        install_frame.pack(fill=tk.X, pady=5)
        
        # Create buttons for recommended models
        quick_buttons_frame = ttk.Frame(install_frame)
        quick_buttons_frame.pack(fill=tk.X)
        
        row = 0
        col = 0
        for model_id, model_info in list(self.recommended_models.items())[:6]:  # Show first 6
            btn_text = f"{model_info['name']}\n({model_info['size']})"
            btn = ttk.Button(quick_buttons_frame, text=btn_text, 
                           command=lambda mid=model_id: self.pull_model(mid))
            btn.grid(row=row, column=col, padx=5, pady=5, sticky='ew')
            col += 1
            if col > 2:  # 3 buttons per row
                col = 0
                row += 1
        
        # Configure grid weights
        for i in range(3):
            quick_buttons_frame.columnconfigure(i, weight=1)
    
    def check_ollama(self):
        """Check if Ollama is installed and running"""
        self._update_status("🔍 Checking Ollama installation...")
        
        thread = threading.Thread(target=self._check_ollama_thread)
        thread.daemon = True
        thread.start()
    
    def _check_ollama_thread(self):
        """Background thread to check Ollama"""
        try:
            # First check if ollama command exists
            result = subprocess.run(['ollama', '--version'], 
                                  capture_output=True, text=True, timeout=5)
            
            if result.returncode == 0:
                self._update_status("✅ Ollama installed")
                
                # Check if Ollama server is running
                response = requests.get(f"{self.ollama_url}/api/tags", timeout=5)
                if response.status_code == 200:
                    self.ollama_available = True
                    self._update_status("✅ Ollama server running")
                    self.refresh_models()
                else:
                    self._update_status("⚠️ Ollama installed but server not running")
                    self._update_status("💡 Start Ollama server: ollama serve")
            else:
                self._update_status("❌ Ollama not installed")
                self._update_status("📥 Install Ollama: https://ollama.ai/download")
                
        except FileNotFoundError:
            self._update_status("❌ Ollama not found in PATH")
            self._update_status("📥 Install Ollama: https://ollama.ai/download")
        except Exception as e:
            self._update_status(f"❌ Error checking Ollama: {str(e)}")
    
    def refresh_models(self):
        """Refresh the list of available models"""
        if not self.ollama_available:
            self._update_status("⚠️ Cannot refresh - Ollama not available")
            return
        
        self._update_status("🔄 Refreshing model list...")
        
        thread = threading.Thread(target=self._refresh_models_thread)
        thread.daemon = True
        thread.start()
    
    def _refresh_models_thread(self):
        """Background thread to refresh models"""
        try:
            response = requests.get(f"{self.ollama_url}/api/tags", timeout=10)
            if response.status_code == 200:
                data = response.json()
                self.available_models = data.get('models', [])
                
                # Clear and repopulate tree
                for item in self.model_tree.get_children():
                    self.model_tree.delete(item)
                
                for model in self.available_models:
                    name = model.get('name', 'Unknown')
                    size = self._format_size(model.get('size', 0))
                    
                    # Check if model is running (simplified check)
                    status = "Available"
                    if name in self.running_models:
                        status = "Running"
                    
                    self.model_tree.insert('', 'end', values=(name, size, status))
                
                self._update_status(f"✅ Found {len(self.available_models)} models")
            else:
                self._update_status("❌ Failed to fetch models")
                
        except Exception as e:
            self._update_status(f"❌ Error refreshing models: {str(e)}")
    
    def pull_model_dialog(self):
        """Show dialog to pull a model"""
        dialog = tk.Toplevel(self.ide_instance.root)
        dialog.title("Pull AI Model")
        dialog.geometry("600x400")
        dialog.transient(self.ide_instance.root)
        dialog.grab_set()
        
        ttk.Label(dialog, text="Select a model to download:", 
                 font=("Arial", 12, "bold")).pack(pady=10)
        
        # Model selection listbox
        listbox_frame = ttk.Frame(dialog)
        listbox_frame.pack(fill=tk.BOTH, expand=True, padx=10, pady=5)
        
        listbox = tk.Listbox(listbox_frame)
        scrollbar = ttk.Scrollbar(listbox_frame, orient=tk.VERTICAL, command=listbox.yview)
        listbox.configure(yscrollcommand=scrollbar.set)
        
        for model_id, model_info in self.recommended_models.items():
            display_text = f"{model_info['name']} ({model_info['size']}) - {model_info['description']}"
            listbox.insert(tk.END, f"{model_id}|{display_text}")
        
        listbox.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
        scrollbar.pack(side=tk.RIGHT, fill=tk.Y)
        
        # Buttons
        button_frame = ttk.Frame(dialog)
        button_frame.pack(fill=tk.X, pady=10)
        
        def pull_selected():
            selection = listbox.curselection()
            if selection:
                model_id = listbox.get(selection[0]).split('|')[0]
                dialog.destroy()
                self.pull_model(model_id)
            else:
                messagebox.showwarning("Warning", "Please select a model")
        
        ttk.Button(button_frame, text="Pull Selected", command=pull_selected).pack(side=tk.LEFT, padx=5)
        ttk.Button(button_frame, text="Cancel", command=dialog.destroy).pack(side=tk.RIGHT, padx=5)
    
    def pull_model(self, model_name: str):
        """Pull/download a model"""
        if not self.ollama_available:
            messagebox.showerror("Error", "Ollama not available. Please install and start Ollama first.")
            return
        
        self._update_status(f"📥 Pulling model: {model_name}")
        
        thread = threading.Thread(target=self._pull_model_thread, args=(model_name,))
        thread.daemon = True
        thread.start()
    
    def _pull_model_thread(self, model_name: str):
        """Background thread to pull model"""
        try:
            # Use ollama command to pull model
            process = subprocess.Popen(['ollama', 'pull', model_name], 
                                     stdout=subprocess.PIPE, 
                                     stderr=subprocess.STDOUT, 
                                     text=True, 
                                     universal_newlines=True)
            
            # Stream output
            while True:
                output = process.stdout.readline()
                if output == '' and process.poll() is not None:
                    break
                if output:
                    self._update_status(output.strip())
            
            if process.returncode == 0:
                self._update_status(f"✅ Successfully pulled {model_name}")
                self.refresh_models()
                messagebox.showinfo("Success", f"Model '{model_name}' downloaded successfully!")
            else:
                self._update_status(f"❌ Failed to pull {model_name}")
                messagebox.showerror("Error", f"Failed to download model '{model_name}'")
                
        except Exception as e:
            self._update_status(f"❌ Error pulling model: {str(e)}")
            messagebox.showerror("Error", f"Error downloading model: {str(e)}")
    
    def remove_model(self):
        """Remove selected model"""
        selection = self.model_tree.selection()
        if not selection:
            messagebox.showwarning("Warning", "Please select a model to remove")
            return
        
        item = self.model_tree.item(selection[0])
        model_name = item['values'][0]
        
        if messagebox.askyesno("Confirm", f"Are you sure you want to remove '{model_name}'?"):
            self._remove_model_thread(model_name)
    
    def _remove_model_thread(self, model_name: str):
        """Background thread to remove model"""
        try:
            self._update_status(f"🗑️ Removing model: {model_name}")
            
            process = subprocess.run(['ollama', 'rm', model_name], 
                                   capture_output=True, text=True, timeout=30)
            
            if process.returncode == 0:
                self._update_status(f"✅ Successfully removed {model_name}")
                self.refresh_models()
                messagebox.showinfo("Success", f"Model '{model_name}' removed successfully!")
            else:
                self._update_status(f"❌ Failed to remove {model_name}: {process.stderr}")
                messagebox.showerror("Error", f"Failed to remove model: {process.stderr}")
                
        except Exception as e:
            self._update_status(f"❌ Error removing model: {str(e)}")
            messagebox.showerror("Error", f"Error removing model: {str(e)}")
    
    def start_model(self):
        """Start selected model"""
        selection = self.model_tree.selection()
        if not selection:
            messagebox.showwarning("Warning", "Please select a model to start")
            return
        
        item = self.model_tree.item(selection[0])
        model_name = item['values'][0]
        
        self._update_status(f"🚀 Starting model: {model_name}")
        
        thread = threading.Thread(target=self._start_model_thread, args=(model_name,))
        thread.daemon = True
        thread.start()
    
    def _start_model_thread(self, model_name: str):
        """Background thread to start model"""
        try:
            # Start model by making a simple request
            response = requests.post(f"{self.ollama_url}/api/generate",
                                   json={
                                       "model": model_name,
                                       "prompt": "Hello",
                                       "stream": False
                                   },
                                   timeout=60)
            
            if response.status_code == 200:
                self.running_models[model_name] = True
                self._update_status(f"✅ Model {model_name} started successfully")
                self.refresh_models()
            else:
                self._update_status(f"❌ Failed to start model {model_name}")
                
        except Exception as e:
            self._update_status(f"❌ Error starting model: {str(e)}")
    
    def stop_model(self):
        """Stop selected model"""
        selection = self.model_tree.selection()
        if not selection:
            messagebox.showwarning("Warning", "Please select a model to stop")
            return
        
        item = self.model_tree.item(selection[0])
        model_name = item['values'][0]
        
        if model_name in self.running_models:
            del self.running_models[model_name]
            self._update_status(f"⏹️ Model {model_name} stopped")
            self.refresh_models()
        else:
            self._update_status(f"⚠️ Model {model_name} was not running")
    
    def _format_size(self, size_bytes: int) -> str:
        """Format size in bytes to human readable format"""
        if size_bytes == 0:
            return "Unknown"
        
        for unit in ['B', 'KB', 'MB', 'GB', 'TB']:
            if size_bytes < 1024.0:
                return f"{size_bytes:.1f} {unit}"
            size_bytes /= 1024.0
        return f"{size_bytes:.1f} PB"
    
    def _update_status(self, message: str):
        """Update status display"""
        if hasattr(self, 'status_text') and self.status_text:
            self.status_text.config(state=tk.NORMAL)
            timestamp = time.strftime("%H:%M:%S")
            self.status_text.insert(tk.END, f"[{timestamp}] {message}\n")
            self.status_text.see(tk.END)
            self.status_text.config(state=tk.DISABLED)

def integrate_local_ai_manager(ide_instance):
    """Integrate local AI model manager into the n0mn0m IDE"""
    local_ai_manager = LocalAIModelManager(ide_instance)
    
    # Add to Tools menu
    if hasattr(ide_instance, 'menubar'):
        tools_menu = None
        for i in range(ide_instance.menubar.index(tk.END)):
            if ide_instance.menubar.entrycget(i, "label") == "Tools":
                tools_menu = ide_instance.menubar.winfo_children()[i]
                break
        
        if tools_menu:
            tools_menu.add_command(label="🤖 Local AI Models", 
                                 command=lambda: ide_instance.notebook.select(local_ai_manager.ide_instance.notebook.children['!frame']))
    
    ide_instance.local_ai_manager = local_ai_manager
    print("🤖 Local AI Model Manager integrated successfully!")

def install_ollama_instructions():
    """Display instructions for installing Ollama"""
    instructions = """
🤖 How to Install and Use Local AI Models with Ollama

📥 Step 1: Install Ollama
• Windows: Download from https://ollama.ai/download
• Mac: brew install ollama
• Linux: curl -fsSL https://ollama.ai/install.sh | sh

🚀 Step 2: Start Ollama Server
• Open terminal/command prompt
• Run: ollama serve
• Keep this running in background

📥 Step 3: Pull AI Models
• Run: ollama pull llama3.2        (3.8GB - best general model)
• Run: ollama pull codellama       (3.8GB - best for coding)
• Run: ollama pull mistral         (4.1GB - high performance)
• Run: ollama pull phi3            (2.3GB - efficient)

💡 Popular Models for Coding:
• codellama - Specialized for programming
• llama3.2 - Great for general coding tasks
• deepseek-coder - Advanced coding model
• mistral - High performance

🔧 Usage:
• Chat: ollama run llama3.2
• Code help: ollama run codellama
• List models: ollama list
• Remove model: ollama rm model_name

🌐 Web Interface:
• Access: http://localhost:11434
• API docs: http://localhost:11434/docs
    """
    print(instructions)
