#!/usr/bin/env python3
"""
Multi-Chat IDE with Moveable AI Panels
Like Cursor but better - multiple AI chats, toggleable panels
NO EMOJIS - Plain ASCII only
"""

import tkinter as tk
from tkinter import ttk, scrolledtext, messagebox
import json
import requests
import threading
from datetime import datetime
import uuid

class ChatPanel:
    def __init__(self, parent, chat_id, provider="ChatGPT", title="AI Assistant"):
        self.chat_id = chat_id
        self.provider = provider
        self.title = title
        self.messages = []
        
        # Create chat window
        self.frame = ttk.Frame(parent)
        
        # Chat header
        header_frame = ttk.Frame(self.frame)
        header_frame.pack(fill=tk.X, padx=5, pady=2)
        
        self.title_label = ttk.Label(header_frame, text=f"{title} ({provider})", font=("Arial", 10, "bold"))
        self.title_label.pack(side=tk.LEFT)
        
        # Close button
        close_btn = ttk.Button(header_frame, text="X", width=3, command=self.close_chat)
        close_btn.pack(side=tk.RIGHT)
        
        # Move button
        move_btn = ttk.Button(header_frame, text="Move", command=self.toggle_floating)
        move_btn.pack(side=tk.RIGHT, padx=2)
        
        # Chat display
        self.chat_display = scrolledtext.ScrolledText(
            self.frame, 
            height=20, 
            wrap=tk.WORD,
            state=tk.DISABLED,
            bg="#f8f9fa",
            font=("Segoe UI", 10)
        )
        self.chat_display.pack(fill=tk.BOTH, expand=True, padx=5, pady=2)
        
        # Input area
        input_frame = ttk.Frame(self.frame)
        input_frame.pack(fill=tk.X, padx=5, pady=2)
        
        self.input_text = scrolledtext.ScrolledText(input_frame, height=3, wrap=tk.WORD)
        self.input_text.pack(fill=tk.BOTH, expand=True, side=tk.LEFT)
        
        send_btn = ttk.Button(input_frame, text="Send", command=self.send_message)
        send_btn.pack(side=tk.RIGHT, padx=(5,0))
        
        # Bind Enter key
        self.input_text.bind("<Control-Return>", lambda e: self.send_message())
        
        self.floating_window = None
    
    def add_message(self, sender, message, timestamp=None):
        if timestamp is None:
            timestamp = datetime.now().strftime("%H:%M:%S")
        
        self.chat_display.config(state=tk.NORMAL)
        
        # Add message with formatting
        if sender == "You":
            self.chat_display.insert(tk.END, f"[{timestamp}] You: ", "user_tag")
        else:
            self.chat_display.insert(tk.END, f"[{timestamp}] {self.provider}: ", "ai_tag")
        
        self.chat_display.insert(tk.END, f"{message}\n\n")
        
        # Configure tags for styling
        self.chat_display.tag_config("user_tag", foreground="#0066cc", font=("Segoe UI", 10, "bold"))
        self.chat_display.tag_config("ai_tag", foreground="#cc6600", font=("Segoe UI", 10, "bold"))
        
        self.chat_display.config(state=tk.DISABLED)
        self.chat_display.see(tk.END)
        
        # Store message
        self.messages.append({
            "sender": sender,
            "message": message,
            "timestamp": timestamp
        })
    
    def send_message(self):
        message = self.input_text.get(1.0, tk.END).strip()
        if not message:
            return
        
        # Clear input
        self.input_text.delete(1.0, tk.END)
        
        # Add user message
        self.add_message("You", message)
        
        # Send to AI (in background thread)
        threading.Thread(target=self.process_ai_response, args=(message,), daemon=True).start()
    
    def process_ai_response(self, message):
        try:
            # Simulate AI response (replace with actual API calls)
            response = self.call_ai_api(message)
            self.add_message(self.provider, response)
        except Exception as e:
            self.add_message("System", f"Error: {str(e)}")
    
    def call_ai_api(self, message):
        # Placeholder for actual AI API integration
        # Replace with real ChatGPT, Claude, etc. API calls
        return f"This is a simulated response from {self.provider} to: '{message[:50]}...'"
    
    def toggle_floating(self):
        if self.floating_window is None:
            self.make_floating()
        else:
            self.dock_back()
    
    def make_floating(self):
        # Create floating window
        self.floating_window = tk.Toplevel()
        self.floating_window.title(f"{self.title} - Floating")
        self.floating_window.geometry("400x500")
        
        # Move chat to floating window
        self.frame.pack_forget()
        self.frame = ttk.Frame(self.floating_window)
        self.frame.pack(fill=tk.BOTH, expand=True)
        
        # Recreate UI in floating window
        self.rebuild_ui()
    
    def dock_back(self):
        if self.floating_window:
            self.floating_window.destroy()
            self.floating_window = None
            # Would need reference to parent to dock back
    
    def rebuild_ui(self):
        # Rebuild the UI in current frame
        pass  # Implementation would recreate all widgets
    
    def close_chat(self):
        if messagebox.askyesno("Close Chat", f"Close chat with {self.provider}?"):
            if self.floating_window:
                self.floating_window.destroy()
            self.frame.destroy()

class MultiChatIDE:
    def __init__(self):
        self.root = tk.Tk()
        self.root.title("n0mn0m Multi-Chat IDE")
        self.root.geometry("1400x900")
        
        self.chat_panels = {}
        self.next_chat_id = 1
        
        self.setup_ui()
        self.setup_ai_providers()
    
    def setup_ui(self):
        # Main menu
        self.setup_menu()
        
        # Main paned window (horizontal split)
        self.main_paned = ttk.PanedWindow(self.root, orient=tk.HORIZONTAL)
        self.main_paned.pack(fill=tk.BOTH, expand=True)
        
        # Left side: Code editor and file explorer
        self.setup_code_area()
        
        # Right side: Chat panels
        self.setup_chat_area()
        
        # Bottom status bar
        self.setup_status_bar()
    
    def setup_menu(self):
        menubar = tk.Menu(self.root)
        self.root.config(menu=menubar)
        
        # File menu
        file_menu = tk.Menu(menubar, tearoff=0)
        menubar.add_cascade(label="File", menu=file_menu)
        file_menu.add_command(label="New", command=self.new_file)
        file_menu.add_command(label="Open", command=self.open_file)
        file_menu.add_command(label="Save", command=self.save_file)
        
        # Chat menu
        chat_menu = tk.Menu(menubar, tearoff=0)
        menubar.add_cascade(label="Chat", menu=chat_menu)
        chat_menu.add_command(label="New ChatGPT Chat", command=lambda: self.new_chat("ChatGPT"))
        chat_menu.add_command(label="New Claude Chat", command=lambda: self.new_chat("Claude"))
        chat_menu.add_command(label="New Copilot Chat", command=lambda: self.new_chat("GitHub Copilot"))
        chat_menu.add_separator()
        chat_menu.add_command(label="Toggle Chat Panel", command=self.toggle_chat_panel)
        chat_menu.add_command(label="Tile Chats Vertically", command=self.tile_chats_vertical)
        chat_menu.add_command(label="Tile Chats Horizontally", command=self.tile_chats_horizontal)
        
        # View menu
        view_menu = tk.Menu(menubar, tearoff=0)
        menubar.add_cascade(label="View", menu=view_menu)
        view_menu.add_command(label="Show File Explorer", command=self.toggle_file_explorer)
        view_menu.add_command(label="Show Terminal", command=self.toggle_terminal)
        view_menu.add_command(label="Fullscreen Code", command=self.fullscreen_code)
    
    def setup_code_area(self):
        # Code area frame
        code_frame = ttk.Frame(self.main_paned)
        self.main_paned.add(code_frame, weight=3)
        
        # Code area paned window (vertical split)
        code_paned = ttk.PanedWindow(code_frame, orient=tk.VERTICAL)
        code_paned.pack(fill=tk.BOTH, expand=True)
        
        # Top: Editor
        editor_frame = ttk.Frame(code_paned)
        code_paned.add(editor_frame, weight=3)
        
        # File tabs
        self.editor_notebook = ttk.Notebook(editor_frame)
        self.editor_notebook.pack(fill=tk.BOTH, expand=True)
        
        # Default editor tab
        self.create_editor_tab("untitled.py")
        
        # Bottom: Terminal
        terminal_frame = ttk.Frame(code_paned)
        code_paned.add(terminal_frame, weight=1)
        
        ttk.Label(terminal_frame, text="Terminal", font=("Arial", 10, "bold")).pack(anchor="w")
        self.terminal = scrolledtext.ScrolledText(
            terminal_frame,
            height=8,
            bg="black",
            fg="lime",
            font=("Consolas", 10)
        )
        self.terminal.pack(fill=tk.BOTH, expand=True)
        self.terminal.insert(tk.END, "n0mn0m IDE Terminal Ready\n$ ")
    
    def setup_chat_area(self):
        # Chat area frame
        self.chat_frame = ttk.Frame(self.main_paned)
        self.main_paned.add(self.chat_frame, weight=2)
        
        # Chat notebook for multiple chat tabs
        self.chat_notebook = ttk.Notebook(self.chat_frame)
        self.chat_notebook.pack(fill=tk.BOTH, expand=True)
        
        # Add button for new chats
        control_frame = ttk.Frame(self.chat_frame)
        control_frame.pack(fill=tk.X, pady=2)
        
        ttk.Button(control_frame, text="+ ChatGPT", command=lambda: self.new_chat("ChatGPT")).pack(side=tk.LEFT, padx=2)
        ttk.Button(control_frame, text="+ Claude", command=lambda: self.new_chat("Claude")).pack(side=tk.LEFT, padx=2)
        ttk.Button(control_frame, text="+ Copilot", command=lambda: self.new_chat("GitHub Copilot")).pack(side=tk.LEFT, padx=2)
        
        # Create initial chat
        self.new_chat("ChatGPT")
    
    def setup_status_bar(self):
        self.status_bar = ttk.Label(
            self.root,
            text="Ready | Chats: 0 | Files: 1",
            relief=tk.SUNKEN,
            anchor="w"
        )
        self.status_bar.pack(side=tk.BOTTOM, fill=tk.X)
    
    def setup_ai_providers(self):
        self.ai_providers = {
            "ChatGPT": {
                "api_url": "https://api.openai.com/v1/chat/completions",
                "model": "gpt-3.5-turbo",
                "api_key_env": "OPENAI_API_KEY"
            },
            "Claude": {
                "api_url": "https://api.anthropic.com/v1/messages",
                "model": "claude-3-sonnet-20240229",
                "api_key_env": "ANTHROPIC_API_KEY"
            },
            "GitHub Copilot": {
                "api_url": "https://api.github.com/copilot/completions",
                "model": "copilot",
                "api_key_env": "GITHUB_TOKEN"
            }
        }
    
    def create_editor_tab(self, filename):
        # Create new editor tab
        tab_frame = ttk.Frame(self.editor_notebook)
        
        editor = scrolledtext.ScrolledText(
            tab_frame,
            wrap=tk.NONE,
            undo=True,
            font=("Consolas", 12)
        )
        editor.pack(fill=tk.BOTH, expand=True)
        
        self.editor_notebook.add(tab_frame, text=filename)
        self.editor_notebook.select(tab_frame)
        
        return editor
    
    def new_chat(self, provider):
        chat_id = f"chat_{self.next_chat_id}"
        self.next_chat_id += 1
        
        # Create chat panel
        chat_panel = ChatPanel(self.chat_notebook, chat_id, provider, f"AI Chat {len(self.chat_panels) + 1}")
        
        # Add to notebook
        self.chat_notebook.add(chat_panel.frame, text=f"{provider[:8]}")
        self.chat_notebook.select(chat_panel.frame)
        
        # Store reference
        self.chat_panels[chat_id] = chat_panel
        
        # Send welcome message
        chat_panel.add_message(provider, f"Hello! I'm {provider}. How can I help you with your code?")
        
        self.update_status()
    
    def toggle_chat_panel(self):
        # Toggle visibility of entire chat area
        if self.chat_frame.winfo_viewable():
            self.main_paned.forget(self.chat_frame)
        else:
            self.main_paned.add(self.chat_frame, weight=2)
    
    def tile_chats_vertical(self):
        # Arrange chats in vertical tiles
        # Implementation would rearrange chat windows
        pass
    
    def tile_chats_horizontal(self):
        # Arrange chats in horizontal tiles
        # Implementation would rearrange chat windows
        pass
    
    def toggle_file_explorer(self):
        # Toggle file explorer visibility
        pass
    
    def toggle_terminal(self):
        # Toggle terminal visibility
        pass
    
    def fullscreen_code(self):
        # Hide all panels except code editor
        if self.chat_frame.winfo_viewable():
            self.main_paned.forget(self.chat_frame)
        else:
            self.main_paned.add(self.chat_frame, weight=2)
    
    def update_status(self):
        chat_count = len(self.chat_panels)
        file_count = self.editor_notebook.index("end")
        self.status_bar.config(text=f"Ready | Chats: {chat_count} | Files: {file_count}")
    
    def new_file(self):
        self.create_editor_tab(f"untitled_{self.editor_notebook.index('end')}.py")
        self.update_status()
    
    def open_file(self):
        # File open dialog implementation
        pass
    
    def save_file(self):
        # File save implementation
        pass
    
    def run(self):
        self.root.mainloop()

if __name__ == "__main__":
    ide = MultiChatIDE()
    print("Multi-Chat IDE Starting...")
    print("Features:")
    print("- Multiple AI chat panels (ChatGPT, Claude, Copilot)")
    print("- Moveable and toggleable chat windows")
    print("- Split code/chat layout")
    print("- Resizable panels")
    ide.run()
