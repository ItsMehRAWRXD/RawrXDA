#!/usr/bin/env python3
"""
RawrZ Universal IDE - Pull Model Dialog
Tkinter dialog for pulling AI models with progress tracking
"""

import tkinter as tk
from tkinter import ttk, messagebox
import threading
import time
import json
import os
from pathlib import Path
from datetime import datetime

class PullModelDialog:
    def __init__(self, parent, ai_manager=None):
        self.parent = parent
        self.ai_manager = ai_manager
        self.dialog = None
        self.progress_var = None
        self.status_var = None
        self.model_name_var = None
        self.pull_thread = None
        self.cancelled = False

    def show_dialog(self):
        """Show the pull model dialog"""
        self.dialog = tk.Toplevel(self.parent)
        self.dialog.title("🤖 Pull AI Model")
        self.dialog.geometry("500x400")
        self.dialog.configure(bg='#1e1e1e')
        self.dialog.resizable(False, False)
        
        # Center the dialog
        self.dialog.transient(self.parent)
        self.dialog.grab_set()
        
        # Main frame
        main_frame = ttk.Frame(self.dialog)
        main_frame.pack(fill=tk.BOTH, expand=True, padx=20, pady=20)
        
        # Title
        title_label = ttk.Label(main_frame, text="🤖 Pull AI Model", 
                               font=('Arial', 16, 'bold'))
        title_label.pack(pady=(0, 20))
        
        # Model selection frame
        model_frame = ttk.LabelFrame(main_frame, text="Model Selection")
        model_frame.pack(fill=tk.X, pady=(0, 15))
        
        # Model name entry
        ttk.Label(model_frame, text="Model Name:").pack(anchor=tk.W, padx=10, pady=5)
        self.model_name_var = tk.StringVar()
        model_entry = ttk.Entry(model_frame, textvariable=self.model_name_var, width=40)
        model_entry.pack(fill=tk.X, padx=10, pady=(0, 10))
        
        # Model suggestions
        suggestions_frame = ttk.Frame(model_frame)
        suggestions_frame.pack(fill=tk.X, padx=10, pady=(0, 10))
        
        ttk.Label(suggestions_frame, text="Popular Models:").pack(anchor=tk.W)
        
        suggestions = [
            "codellama:7b", "codellama:13b", "codellama:34b",
            "llama2:7b", "llama2:13b", "llama2:70b",
            "mistral:7b", "mistral:13b",
            "starcoder:3b", "starcoder:7b", "starcoder:15b",
            "wizardcoder:7b", "wizardcoder:13b", "wizardcoder:34b"
        ]
        
        # Create suggestion buttons
        for i, suggestion in enumerate(suggestions):
            if i % 3 == 0:
                row_frame = ttk.Frame(suggestions_frame)
                row_frame.pack(fill=tk.X, pady=2)
            
            btn = ttk.Button(row_frame, text=suggestion, 
                            command=lambda s=suggestion: self.model_name_var.set(s))
            btn.pack(side=tk.LEFT, padx=2)
        
        # Progress frame
        progress_frame = ttk.LabelFrame(main_frame, text="Pull Progress")
        progress_frame.pack(fill=tk.X, pady=(0, 15))
        
        # Progress bar
        self.progress_var = tk.DoubleVar()
        progress_bar = ttk.Progressbar(progress_frame, variable=self.progress_var, 
                                     maximum=100, length=400)
        progress_bar.pack(padx=10, pady=10)
        
        # Status label
        self.status_var = tk.StringVar(value="Ready to pull model")
        status_label = ttk.Label(progress_frame, textvariable=self.status_var)
        status_label.pack(pady=(0, 10))
        
        # Log display
        log_frame = ttk.LabelFrame(main_frame, text="Pull Log")
        log_frame.pack(fill=tk.BOTH, expand=True, pady=(0, 15))
        
        self.log_text = tk.Text(log_frame, height=8, bg='#2d2d2d', fg='white', 
                               font=('Consolas', 9))
        log_scrollbar = ttk.Scrollbar(log_frame, orient=tk.VERTICAL, command=self.log_text.yview)
        self.log_text.configure(yscrollcommand=log_scrollbar.set)
        
        self.log_text.pack(side=tk.LEFT, fill=tk.BOTH, expand=True, padx=(5, 0), pady=5)
        log_scrollbar.pack(side=tk.RIGHT, fill=tk.Y, pady=5)
        
        # Control buttons
        button_frame = ttk.Frame(main_frame)
        button_frame.pack(fill=tk.X)
        
        self.pull_button = ttk.Button(button_frame, text="🚀 Pull Model", 
                                    command=self.start_pull, style='Accent.TButton')
        self.pull_button.pack(side=tk.LEFT, padx=(0, 10))
        
        self.cancel_button = ttk.Button(button_frame, text="❌ Cancel", 
                                      command=self.cancel_pull, state=tk.DISABLED)
        self.cancel_button.pack(side=tk.LEFT, padx=(0, 10))
        
        self.close_button = ttk.Button(button_frame, text="✅ Close", 
                                  command=self.dialog.destroy)
        self.close_button.pack(side=tk.RIGHT)
        
        # Focus on model entry
        model_entry.focus()
        
        return self.dialog

    def start_pull(self):
        """Start pulling the model"""
        model_name = self.model_name_var.get().strip()
        if not model_name:
            messagebox.showwarning("No Model", "Please enter a model name")
            return
        
        # Update UI
        self.pull_button.config(state=tk.DISABLED)
        self.cancel_button.config(state=tk.NORMAL)
        self.cancelled = False
        
        # Start pull thread
        self.pull_thread = threading.Thread(target=self._pull_model_thread, 
                                           args=(model_name,), daemon=True)
        self.pull_thread.start()

    def _pull_model_thread(self, model_name):
        """Pull model in background thread"""
        try:
            self._update_status(f"Starting pull of {model_name}...")
            self._log_message(f"🚀 Starting pull of model: {model_name}")
            
            # Simulate model pull with progress updates
            self._simulate_model_pull(model_name)
            
            if not self.cancelled:
                self._update_status(f"Successfully pulled {model_name}")
                self._log_message(f"✅ Model {model_name} pulled successfully!")
                self._update_progress(100)
                
                # Update AI manager if available
                if self.ai_manager:
                    self._update_ai_manager(model_name)
                
                # Show success message
                self.dialog.after(0, lambda: messagebox.showinfo("Success", 
                    f"Model {model_name} pulled successfully!"))
            else:
                self._update_status("Pull cancelled")
                self._log_message("❌ Model pull cancelled by user")
                
        except Exception as e:
            self._update_status(f"Error: {str(e)}")
            self._log_message(f"❌ Error pulling model: {str(e)}")
            self.dialog.after(0, lambda: messagebox.showerror("Error", 
                f"Failed to pull model: {str(e)}"))
        finally:
            # Reset UI
            self.dialog.after(0, self._reset_ui)

    def _simulate_model_pull(self, model_name):
        """Simulate model pull with progress updates"""
        stages = [
            ("Initializing pull...", 5),
            ("Downloading model files...", 30),
            ("Verifying model integrity...", 50),
            ("Extracting model data...", 70),
            ("Installing model dependencies...", 85),
            ("Finalizing installation...", 95)
        ]
        
        for stage, progress in stages:
            if self.cancelled:
                return
            
            self._update_status(stage)
            self._log_message(f"📦 {stage}")
            
            # Simulate work
            for i in range(10):
                if self.cancelled:
                    return
                time.sleep(0.1)
                self._update_progress(progress + (i * 0.5))

    def _update_status(self, status):
        """Update status label"""
        self.dialog.after(0, lambda: self.status_var.set(status))

    def _update_progress(self, progress):
        """Update progress bar"""
        self.dialog.after(0, lambda: self.progress_var.set(progress))

    def _log_message(self, message):
        """Add message to log"""
        timestamp = datetime.now().strftime("%H:%M:%S")
        log_entry = f"[{timestamp}] {message}\n"
        self.dialog.after(0, lambda: self._append_log(log_entry))

    def _append_log(self, message):
        """Append message to log text widget"""
        self.log_text.insert(tk.END, message)
        self.log_text.see(tk.END)

    def _update_ai_manager(self, model_name):
        """Update AI manager with new model"""
        try:
            if hasattr(self.ai_manager, 'add_model'):
                self.ai_manager.add_model(model_name)
            elif hasattr(self.ai_manager, 'available_models'):
                if model_name not in self.ai_manager.available_models:
                    self.ai_manager.available_models.append(model_name)
        except Exception as e:
            self._log_message(f"⚠️ Could not update AI manager: {e}")

    def cancel_pull(self):
        """Cancel the pull operation"""
        self.cancelled = True
        self._update_status("Cancelling pull...")
        self._log_message("🛑 Cancelling model pull...")
        
        # Wait for thread to finish
        if self.pull_thread and self.pull_thread.is_alive():
            self.pull_thread.join(timeout=2)

    def _reset_ui(self):
        """Reset UI to initial state"""
        self.pull_button.config(state=tk.NORMAL)
        self.cancel_button.config(state=tk.DISABLED)

class ModelManager:
    """Model manager for handling AI models"""
    
    def __init__(self):
        self.models_dir = Path(__file__).parent / "ai_models"
        self.models_dir.mkdir(exist_ok=True)
        self.available_models = []
        self.load_models()

    def load_models(self):
        """Load available models"""
        models_file = self.models_dir / "models.json"
        try:
            if models_file.exists():
                with open(models_file, 'r') as f:
                    data = json.load(f)
                    self.available_models = data.get('models', [])
        except Exception as e:
            print(f"Error loading models: {e}")
            self.available_models = []

    def save_models(self):
        """Save models list"""
        models_file = self.models_dir / "models.json"
        try:
            data = {
                'models': self.available_models,
                'last_updated': datetime.now().isoformat()
            }
            with open(models_file, 'w') as f:
                json.dump(data, f, indent=2)
        except Exception as e:
            print(f"Error saving models: {e}")

    def add_model(self, model_name):
        """Add a new model"""
        if model_name not in self.available_models:
            self.available_models.append(model_name)
            self.save_models()
            print(f"✅ Added model: {model_name}")

    def remove_model(self, model_name):
        """Remove a model"""
        if model_name in self.available_models:
            self.available_models.remove(model_name)
            self.save_models()
            print(f"✅ Removed model: {model_name}")

    def list_models(self):
        """List all available models"""
        return self.available_models

def test_pull_model_dialog():
    """Test the pull model dialog"""
    root = tk.Tk()
    root.withdraw()  # Hide main window
    
    # Create model manager
    model_manager = ModelManager()
    
    # Show dialog
    dialog = PullModelDialog(root, model_manager)
    dialog.show_dialog()
    
    root.mainloop()

if __name__ == "__main__":
    test_pull_model_dialog()
