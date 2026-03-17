#!/usr/bin/env python3
"""
IDE Swarm GUI Interface
Full graphical user interface for the IDE-integrated swarm controller using Tkinter
"""

import tkinter as tk
from tkinter import messagebox, filedialog, scrolledtext, ttk
import asyncio
import threading
import sys
import os
import json
from pathlib import Path
from datetime import datetime

# Import the swarm controller
from ide_swarm_controller import IDESwarmController, SwarmAgent

class IDESwarmGUI:
    """Full GUI interface for IDE Swarm Controller"""

    def __init__(self, root):
        self.root = root
        self.root.title("🎯 IDE Swarm Controller - Full Interface")
        self.root.geometry("1200x800")
        self.root.configure(bg='#f0f2f5')

        # Initialize swarm controller in background thread
        self.swarm_controller = None
        self.swarm_thread = None
        self.is_swarm_running = False

        # Create GUI components
        self._create_menu_bar()
        self._create_toolbar()
        self._create_main_area()
        self._create_status_bar()

        # Initialize swarm in background
        self._start_swarm_background()

        # Create models directory for embedded models
        self.models_dir = Path("models")
        self.models_dir.mkdir(exist_ok=True)

    def _create_menu_bar(self):
        """Create the main menu bar with all agent menus"""
        self.menu_bar = tk.Menu(self.root)
        self.root.config(menu=self.menu_bar)

        # File Menu
        self.file_menu = tk.Menu(self.menu_bar, tearoff=0)
        self.file_menu.add_command(label="🗂️ Open File", command=self.open_file, accelerator="Ctrl+O")
        self.file_menu.add_command(label="📁 Open Folder", command=self.open_folder, accelerator="Ctrl+K Ctrl+O")
        self.file_menu.add_command(label="💾 Save", command=self.save_file, accelerator="Ctrl+S")
        self.file_menu.add_command(label="📄 New File", command=self.new_file, accelerator="Ctrl+N")
        self.file_menu.add_separator()
        self.file_menu.add_command(label="🚪 Exit", command=self.root.quit)
        self.menu_bar.add_cascade(label="📁 File", menu=self.file_menu)

        # Explorer Agent Menu
        self.explorer_menu = tk.Menu(self.menu_bar, tearoff=0)
        self.explorer_menu.add_command(label="📂 Open Folder", command=self.explorer_open_folder)
        self.explorer_menu.add_command(label="📝 New File", command=self.explorer_new_file)
        self.explorer_menu.add_command(label="📁 New Folder", command=self.explorer_new_folder)
        self.explorer_menu.add_command(label="✏️ Rename", command=self.explorer_rename)
        self.explorer_menu.add_command(label="🗑️ Delete", command=self.explorer_delete)
        self.explorer_menu.add_command(label="📋 Copy Path", command=self.explorer_copy_path)
        self.explorer_menu.add_command(label="👁️ Reveal in Explorer", command=self.explorer_reveal)
        self.menu_bar.add_cascade(label="🗂️ Explorer", menu=self.explorer_menu)

        # Editor Agent Menu
        self.editor_menu = tk.Menu(self.menu_bar, tearoff=0)
        self.editor_menu.add_command(label="✂️ Cut", command=self.editor_cut, accelerator="Ctrl+X")
        self.editor_menu.add_command(label="📋 Copy", command=self.editor_copy, accelerator="Ctrl+C")
        self.editor_menu.add_command(label="📋 Paste", command=self.editor_paste, accelerator="Ctrl+V")
        self.editor_menu.add_command(label="🔍 Find", command=self.editor_find, accelerator="Ctrl+F")
        self.editor_menu.add_command(label="🔄 Replace", command=self.editor_replace, accelerator="Ctrl+H")
        self.editor_menu.add_command(label="🎨 Format Document", command=self.editor_format, accelerator="Shift+Alt+F")
        self.editor_menu.add_command(label="💬 Toggle Comment", command=self.editor_comment, accelerator="Ctrl+/")
        self.editor_menu.add_command(label="🎯 Go to Definition", command=self.editor_goto_def, accelerator="F12")
        self.editor_menu.add_command(label="🏷️ Rename Symbol", command=self.editor_rename_symbol, accelerator="F2")
        self.menu_bar.add_cascade(label="✏️ Editor", menu=self.editor_menu)

        # Git Agent Menu
        self.git_menu = tk.Menu(self.menu_bar, tearoff=0)
        self.git_menu.add_command(label="📊 Git Status", command=self.git_status)
        self.git_menu.add_command(label="➕ Stage Changes", command=self.git_stage)
        self.git_menu.add_command(label="💾 Commit", command=self.git_commit)
        self.git_menu.add_command(label="⬆️ Push", command=self.git_push)
        self.git_menu.add_command(label="⬇️ Pull", command=self.git_pull)
        self.git_menu.add_command(label="📜 View History", command=self.git_log)
        self.git_menu.add_command(label="📋 Compare", command=self.git_diff)
        self.git_menu.add_command(label="🌿 Create Branch", command=self.git_branch)
        self.git_menu.add_command(label="🔐 Git Login", command=self.git_login)
        self.menu_bar.add_cascade(label="🌿 Git", menu=self.git_menu)

        # Chat Agent Menu
        self.chat_menu = tk.Menu(self.menu_bar, tearoff=0)
        self.chat_menu.add_command(label="🤖 Ask Agent", command=self.chat_ask)
        self.chat_menu.add_command(label="📖 Explain Selection", command=self.chat_explain)
        self.chat_menu.add_command(label="⚡ Generate Code", command=self.chat_generate)
        self.chat_menu.add_command(label="🐛 Debug Help", command=self.chat_debug)
        self.chat_menu.add_command(label="👀 Review Code", command=self.chat_review)
        self.chat_menu.add_command(label="📚 Document", command=self.chat_document)
        self.chat_menu.add_command(label="🧠 Agent Training Stats", command=self.show_agent_stats)
        self.menu_bar.add_cascade(label="🤖 Chat", menu=self.chat_menu)

        # Models Menu - Built-in Swarm Model Generator
        self.models_menu = tk.Menu(self.menu_bar, tearoff=0)
        self.models_menu.add_command(label="🧠 Create Model", command=self.create_model)
        self.models_menu.add_command(label="📦 Embed GGUF", command=self.embed_gguf)
        self.models_menu.add_command(label="🚀 Deploy Agent", command=self.deploy_agent)
        self.models_menu.add_command(label="📊 Model Library", command=self.model_library)
        self.models_menu.add_command(label="⚙️ Model Settings", command=self.model_settings)
        self.models_menu.add_separator()
        self.models_menu.add_command(label="🔥 Heretic Modifier", command=self.heretic_modifier)
        self.menu_bar.add_cascade(label="🧠 Models", menu=self.models_menu)

        # Settings Agent Menu
        self.settings_menu = tk.Menu(self.menu_bar, tearoff=0)
        self.settings_menu.add_command(label="⚙️ Settings", command=self.settings_open)
        self.settings_menu.add_command(label="🔌 Extensions", command=self.settings_extensions)
        self.settings_menu.add_command(label="⌨️ Keyboard Shortcuts", command=self.settings_shortcuts)
        self.settings_menu.add_command(label="🎨 Themes", command=self.settings_themes)
        self.settings_menu.add_command(label="🔧 Configure", command=self.settings_configure)
        self.menu_bar.add_cascade(label="⚙️ Settings", menu=self.settings_menu)

        # Logger Agent Menu
        self.logger_menu = tk.Menu(self.menu_bar, tearoff=0)
        self.logger_menu.add_command(label="📊 View History", command=self.logger_history)
        self.logger_menu.add_command(label="📁 Export Logs", command=self.logger_export)
        self.logger_menu.add_command(label="🗑️ Clear Logs", command=self.logger_clear)
        self.logger_menu.add_command(label="📈 Performance Stats", command=self.logger_stats)
        self.menu_bar.add_cascade(label="📊 Logger", menu=self.logger_menu)

        # Swarm Control Menu
        self.swarm_menu = tk.Menu(self.menu_bar, tearoff=0)
        self.swarm_menu.add_command(label="🎯 Select Agent", command=self.swarm_select_agent)
        self.swarm_menu.add_command(label="🔄 @Agent Try Again", command=self.swarm_retry)
        self.swarm_menu.add_command(label="🌐 God Mode", command=self.swarm_god_mode)
        self.swarm_menu.add_command(label="📋 Assign Task", command=self.swarm_assign_task)
        self.swarm_menu.add_command(label="⚡ Execute Workflow", command=self.swarm_execute_workflow)
        self.swarm_menu.add_separator()
        self.swarm_menu.add_command(label="📊 Swarm Status", command=self.show_status)
        self.menu_bar.add_cascade(label="🎯 Swarm", menu=self.swarm_menu)

        # Help Menu
        self.help_menu = tk.Menu(self.menu_bar, tearoff=0)
        self.help_menu.add_command(label="📚 Help", command=self.show_help)
        self.help_menu.add_command(label="📊 Status", command=self.show_status)
        self.help_menu.add_command(label="🎯 About", command=self.show_about)
        self.menu_bar.add_cascade(label="❓ Help", menu=self.help_menu)

    def _create_toolbar(self):
        """Create toolbar with quick action buttons"""
        self.toolbar = tk.Frame(self.root, bg='#e1e5e9', height=40)
        self.toolbar.pack(side=tk.TOP, fill=tk.X)

        # Quick action buttons
        buttons = [
            ("📊 Status", self.show_status),
            ("🗂️ Explorer", self.show_explorer_panel),
            ("✏️ Editor", self.show_editor_panel),
            ("🌿 Git", self.show_git_panel),
            ("🤖 Chat", self.show_chat_panel),
            ("⚙️ Settings", self.show_settings_panel)
        ]

        for text, command in buttons:
            btn = tk.Button(self.toolbar, text=text, command=command,
                          bg='#ffffff', fg='#2d3748', relief=tk.FLAT,
                          padx=10, pady=5, font=('Arial', 9))
            btn.pack(side=tk.LEFT, padx=2, pady=2)

    def _create_main_area(self):
        """Create the main content area with tabs"""
        # Create notebook (tabbed interface)
        self.notebook = ttk.Notebook(self.root)
        self.notebook.pack(fill=tk.BOTH, expand=True, padx=5, pady=5)

        # Editor Tab
        self.editor_frame = tk.Frame(self.notebook, bg='#ffffff')
        self.notebook.add(self.editor_frame, text="📝 Editor")

        # Create text editor
        self.text_editor = scrolledtext.ScrolledText(
            self.editor_frame,
            wrap=tk.WORD,
            font=('Consolas', 10),
            undo=True
        )
        self.text_editor.pack(fill=tk.BOTH, expand=True, padx=5, pady=5)

        # Console Tab
        self.console_frame = tk.Frame(self.notebook, bg='#1a202c')
        self.notebook.add(self.console_frame, text="💻 Console")

        self.console_text = scrolledtext.ScrolledText(
            self.console_frame,
            wrap=tk.WORD,
            font=('Consolas', 9),
            bg='#1a202c',
            fg='#68d391',
            insertbackground='#68d391'
        )
        self.console_text.pack(fill=tk.BOTH, expand=True, padx=5, pady=5)
        self.console_text.insert(tk.END, "🎯 IDE Swarm Console Ready\n")
        self.console_text.insert(tk.END, "📡 Right-click for context menus\n")
        self.console_text.insert(tk.END, "⚡ Select actions from toolbar or menus\n")

        # Explorer Tab
        self.explorer_frame = tk.Frame(self.notebook, bg='#ffffff')
        self.notebook.add(self.explorer_frame, text="🗂️ Explorer")

        # File tree placeholder
        self.file_tree = tk.Treeview(self.explorer_frame)
        self.file_tree.pack(fill=tk.BOTH, expand=True, padx=5, pady=5)
        self.file_tree.insert('', 'end', text='d:\\professional-nasm-ide', open=True)
        self.file_tree.insert('d:\\professional-nasm-ide', 'end', text='swarm\\')
        self.file_tree.insert('d:\\professional-nasm-ide', 'end', text='src\\')

        # Chat Tab
        self.chat_frame = tk.Frame(self.notebook, bg='#ffffff')
        self.notebook.add(self.chat_frame, text="🤖 Chat")

        self.chat_history = scrolledtext.ScrolledText(
            self.chat_frame,
            wrap=tk.WORD,
            font=('Arial', 10),
            state=tk.DISABLED
        )
        self.chat_history.pack(fill=tk.BOTH, expand=True, padx=5, pady=5)

        self.chat_input = tk.Entry(self.chat_frame, font=('Arial', 10))
        self.chat_input.pack(fill=tk.X, padx=5, pady=5)
        self.chat_input.bind('<Return>', self.send_chat_message)

        # Settings Tab
        self.settings_frame = tk.Frame(self.notebook, bg='#ffffff')
        self.notebook.add(self.settings_frame, text="⚙️ Settings")

        settings_label = tk.Label(self.settings_frame, text="Workspace Settings",
                                font=('Arial', 14, 'bold'))
        settings_label.pack(pady=20)

        # Placeholder settings
        tk.Label(self.settings_frame, text="Theme:").pack(anchor=tk.W, padx=20)
        theme_combo = ttk.Combobox(self.settings_frame, values=["Light", "Dark", "Auto"])
        theme_combo.pack(padx=20, pady=5)
        theme_combo.set("Light")

    def _create_status_bar(self):
        """Create status bar at bottom"""
        self.status_frame = tk.Frame(self.root, bg='#e1e5e9', height=25)
        self.status_frame.pack(side=tk.BOTTOM, fill=tk.X)

        self.status_label = tk.Label(self.status_frame, text="Ready",
                                   bg='#e1e5e9', fg='#2d3748', anchor=tk.W)
        self.status_label.pack(side=tk.LEFT, padx=10)

        self.swarm_status = tk.Label(self.status_frame, text="🟢 Swarm: Active",
                                   bg='#e1e5e9', fg='#38a169')
        self.swarm_status.pack(side=tk.RIGHT, padx=10)

    def _start_swarm_background(self):
        """Start swarm controller in background thread"""
        def run_swarm():
            try:
                # Create event loop for async operations
                loop = asyncio.new_event_loop()
                asyncio.set_event_loop(loop)

                # Initialize swarm controller
                self.swarm_controller = IDESwarmController()
                self.is_swarm_running = True

                # Update GUI status
                self.root.after(0, lambda: self.swarm_status.config(text="🟢 Swarm: Active"))

                # Keep swarm running
                loop.run_until_complete(self._swarm_maintenance())

            except Exception as e:
                self.root.after(0, lambda: self._show_error(f"Swarm Error: {e}"))
                self.is_swarm_running = False

        self.swarm_thread = threading.Thread(target=run_swarm, daemon=True)
        self.swarm_thread.start()

    async def _swarm_maintenance(self):
        """Maintain swarm operations"""
        while self.is_swarm_running:
            await asyncio.sleep(1)
            # Could add periodic status updates here

    def _show_error(self, message):
        """Show error message"""
        self.status_label.config(text=f"❌ {message}", fg='#e53e3e')
        messagebox.showerror("Error", message)

    def _log_to_console(self, message):
        """Log message to console tab"""
        self.console_text.insert(tk.END, f"{message}\n")
        self.console_text.see(tk.END)

    # File Menu Handlers
    def open_file(self):
        """Open file dialog"""
        file_path = filedialog.askopenfilename(
            title="Open File",
            filetypes=[("All files", "*.*"), ("Python files", "*.py"), ("Text files", "*.txt")]
        )
        if file_path:
            try:
                with open(file_path, 'r', encoding='utf-8') as file:
                    content = file.read()
                    self.text_editor.delete(1.0, tk.END)
                    self.text_editor.insert(tk.END, content)
                self.status_label.config(text=f"Opened: {file_path}")
                self._log_to_console(f"📁 Opened file: {file_path}")
            except Exception as e:
                self._show_error(f"Failed to open file: {e}")

    def open_folder(self):
        """Open folder dialog"""
        folder_path = filedialog.askdirectory(title="Open Folder")
        if folder_path:
            self.status_label.config(text=f"Folder: {folder_path}")
            self._log_to_console(f"📁 Opened folder: {folder_path}")

    def save_file(self):
        """Save file dialog"""
        file_path = filedialog.asksaveasfilename(
            title="Save File",
            defaultextension=".txt",
            filetypes=[("All files", "*.*"), ("Python files", "*.py"), ("Text files", "*.txt")]
        )
        if file_path:
            try:
                content = self.text_editor.get(1.0, tk.END)
                with open(file_path, 'w', encoding='utf-8') as file:
                    file.write(content)
                self.status_label.config(text=f"Saved: {file_path}")
                self._log_to_console(f"💾 Saved file: {file_path}")
            except Exception as e:
                self._show_error(f"Failed to save file: {e}")

    def new_file(self):
        """Create new file"""
        self.text_editor.delete(1.0, tk.END)
        self.status_label.config(text="New file")
        self._log_to_console("📄 Created new file")

    # Explorer Menu Handlers
    def explorer_open_folder(self):
        """Open folder via explorer agent"""
        self._execute_swarm_action("explorer_open", "Select folder to open")
        self._log_to_console("📂 Explorer: Open folder")

    def explorer_new_file(self):
        """Create new file via explorer agent"""
        self._execute_swarm_action("explorer_new_file", "New file created")
        self._log_to_console("📝 Explorer: New file")

    def explorer_new_folder(self):
        """Create new folder via explorer agent"""
        self._execute_swarm_action("explorer_new_folder", "New folder created")
        self._log_to_console("📁 Explorer: New folder")

    def explorer_rename(self):
        """Rename item via explorer agent"""
        self._execute_swarm_action("explorer_rename", "Item renamed")
        self._log_to_console("✏️ Explorer: Rename")

    def explorer_delete(self):
        """Delete item via explorer agent"""
        if messagebox.askyesno("Confirm Delete", "Are you sure you want to delete this item?"):
            self._execute_swarm_action("explorer_delete", "Item deleted")
            self._log_to_console("🗑️ Explorer: Delete")

    def explorer_copy_path(self):
        """Copy path via explorer agent"""
        self._execute_swarm_action("explorer_copy_path", "Path copied to clipboard")
        self._log_to_console("📋 Explorer: Copy path")

    def explorer_reveal(self):
        """Reveal in explorer via explorer agent"""
        self._execute_swarm_action("explorer_reveal", "Revealed in system explorer")
        self._log_to_console("👁️ Explorer: Reveal")

    # Editor Menu Handlers
    def editor_cut(self):
        """Cut text"""
        self.text_editor.event_generate("<<Cut>>")
        self._log_to_console("✂️ Editor: Cut")

    def editor_copy(self):
        """Copy text"""
        self.text_editor.event_generate("<<Copy>>")
        self._log_to_console("📋 Editor: Copy")

    def editor_paste(self):
        """Paste text"""
        self.text_editor.event_generate("<<Paste>>")
        self._log_to_console("📋 Editor: Paste")

    def editor_find(self):
        """Find in editor"""
        self._execute_swarm_action("editor_find", "Find dialog opened")
        self._log_to_console("🔍 Editor: Find")

    def editor_replace(self):
        """Replace in editor"""
        self._execute_swarm_action("editor_replace", "Replace dialog opened")
        self._log_to_console("🔄 Editor: Replace")

    def editor_format(self):
        """Format document"""
        self._execute_swarm_action("editor_format", "Document formatted")
        self._log_to_console("🎨 Editor: Format document")

    def editor_comment(self):
        """Toggle comment"""
        self._execute_swarm_action("editor_comment", "Comment toggled")
        self._log_to_console("💬 Editor: Toggle comment")

    def editor_goto_def(self):
        """Go to definition"""
        self._execute_swarm_action("editor_goto_def", "Navigated to definition")
        self._log_to_console("🎯 Editor: Go to definition")

    def editor_rename_symbol(self):
        """Rename symbol"""
        self._execute_swarm_action("editor_rename", "Symbol renamed")
        self._log_to_console("🏷️ Editor: Rename symbol")

    # Git Menu Handlers
    def git_status(self):
        """Show git status"""
        self._execute_swarm_action("git_status", "Git status displayed")
        self._log_to_console("📊 Git: Status")

    def git_stage(self):
        """Stage changes"""
        self._execute_swarm_action("git_stage", "Changes staged")
        self._log_to_console("➕ Git: Stage changes")

    def git_commit(self):
        """Commit changes"""
        self._execute_swarm_action("git_commit", "Commit dialog opened")
        self._log_to_console("💾 Git: Commit")

    def git_push(self):
        """Push to remote"""
        self._execute_swarm_action("git_push", "Pushed to remote")
        self._log_to_console("⬆️ Git: Push")

    def git_pull(self):
        """Pull from remote"""
        self._execute_swarm_action("git_pull", "Pulled from remote")
        self._log_to_console("⬇️ Git: Pull")

    def git_log(self):
        """Show git log"""
        self._execute_swarm_action("git_log", "Git log displayed")
        self._log_to_console("📜 Git: Log")

    def git_diff(self):
        """Show git diff"""
        self._execute_swarm_action("git_diff", "Git diff displayed")
        self._log_to_console("📋 Git: Diff")

    def git_branch(self):
        """Create branch"""
        self._execute_swarm_action("git_branch", "Branch dialog opened")
        self._log_to_console("🌿 Git: Branch")

    def git_login(self):
        """Git login"""
        messagebox.showinfo("Git Login", "Git authentication dialog would open here")
        self._log_to_console("🔐 Git: Login")

    # Chat Menu Handlers
    def chat_ask(self):
        """Ask Agent with selection dialog"""
        self.ask_ai()

    def chat_explain(self):
        """Explain code"""
        self._execute_swarm_action("chat_explain", "Code explanation requested")
        self._log_to_console("📖 Chat: Explain code")

    def chat_generate(self):
        """Generate code"""
        self._execute_swarm_action("chat_generate", "Code generation started")
        self._log_to_console("⚡ Chat: Generate code")

    def chat_debug(self):
        """Debug help"""
        self._execute_swarm_action("chat_debug", "Debug assistance activated")
        self._log_to_console("🐛 Chat: Debug help")

    def chat_review(self):
        """Code review"""
        self._execute_swarm_action("chat_review", "Code review started")
        self._log_to_console("👀 Chat: Review code")

    def chat_document(self):
        """Generate documentation"""
        self._execute_swarm_action("chat_document", "Documentation generation started")
        self._log_to_console("📚 Chat: Document")

    def show_agent_stats(self):
        """Show agent training statistics"""
        if not self.swarm_controller:
            messagebox.showerror("Error", "Swarm controller not initialized")
            return

        stats = self.swarm_controller.get_agent_stats()
        
        # Create stats dialog
        stats_dialog = tk.Toplevel(self.root)
        stats_dialog.title("Agent Training Statistics")
        stats_dialog.geometry("600x400")
        stats_dialog.transient(self.root)
        stats_dialog.grab_set()

        tk.Label(stats_dialog, text="🧠 Agent Training Statistics", 
                font=("Arial", 14, "bold")).pack(pady=10)

        # Create text widget for stats
        stats_text = tk.Text(stats_dialog, wrap=tk.WORD, font=("Courier", 10))
        scrollbar = tk.Scrollbar(stats_dialog, command=stats_text.yview)
        stats_text.config(yscrollcommand=scrollbar.set)
        
        stats_text.pack(side=tk.LEFT, fill=tk.BOTH, expand=True, padx=10, pady=5)
        scrollbar.pack(side=tk.RIGHT, fill=tk.Y, pady=5)

        # Display stats
        stats_text.insert(tk.END, "AGENT TRAINING STATISTICS\n")
        stats_text.insert(tk.END, "=" * 50 + "\n\n")
        
        for agent_id, agent_stats in stats.items():
            stats_text.insert(tk.END, f"🤖 {agent_stats['name']} ({agent_id})\n")
            stats_text.insert(tk.END, f"   Experience: {agent_stats['experience']} tasks\n")
            stats_text.insert(tk.END, f"   Memory Usage: {agent_stats['memory_mb']} MB\n")
            stats_text.insert(tk.END, f"   Robustness: {agent_stats['robustness']:.2f}/1.0\n")
            stats_text.insert(tk.END, f"   Instances: {agent_stats['instances']}\n")
            stats_text.insert(tk.END, f"   Capabilities: {agent_stats['capabilities']}\n")
            stats_text.insert(tk.END, f"   Training Events: {agent_stats['training_events']}\n")
            stats_text.insert(tk.END, "\n")

        stats_text.config(state=tk.DISABLED)

        # Close button
        tk.Button(stats_dialog, text="Close", command=stats_dialog.destroy, 
                 width=10, font=("Arial", 10)).pack(pady=10)

    def send_chat_message(self, event):
        """Send chat message"""
        message = self.chat_input.get().strip()
        if message:
            self.chat_history.config(state=tk.NORMAL)
            self.chat_history.insert(tk.END, f"You: {message}\n")
            self.chat_history.insert(tk.END, "AI: Processing your request...\n\n")
            self.chat_history.config(state=tk.DISABLED)
            self.chat_history.see(tk.END)
            self.chat_input.delete(0, tk.END)
            self._log_to_console(f"💬 Chat: {message}")

    # Models Menu Handlers - Built-in Swarm Model Generator
    def create_model(self):
        """Create a new user-defined model"""
        model_dialog = tk.Toplevel(self.root)
        model_dialog.title("🧠 Create Swarm Model")
        model_dialog.geometry("600x500")
        model_dialog.transient(self.root)
        model_dialog.grab_set()

        tk.Label(model_dialog, text="🧠 Swarm Model Generator", 
                font=("Arial", 16, "bold")).pack(pady=10)

        # Model configuration frame
        config_frame = tk.Frame(model_dialog)
        config_frame.pack(fill=tk.BOTH, expand=True, padx=20, pady=10)

        # Model name
        tk.Label(config_frame, text="Model Name:", font=("Arial", 10, "bold")).grid(row=0, column=0, sticky=tk.W, pady=5)
        model_name_entry = tk.Entry(config_frame, width=40, font=("Arial", 10))
        model_name_entry.grid(row=0, column=1, pady=5, padx=(10,0))

        # Model type
        tk.Label(config_frame, text="Model Type:", font=("Arial", 10, "bold")).grid(row=1, column=0, sticky=tk.W, pady=5)
        model_type_var = tk.StringVar(value="text-generation")
        model_type_combo = tk.OptionMenu(config_frame, model_type_var, 
                                       "text-generation", "code-assistant", "analysis", "custom")
        model_type_combo.grid(row=1, column=1, sticky=tk.W, pady=5, padx=(10,0))

        # Base capabilities
        tk.Label(config_frame, text="Base Capabilities:", font=("Arial", 10, "bold")).grid(row=2, column=0, sticky=tk.W, pady=5)
        capabilities_text = tk.Text(config_frame, height=6, width=40, font=("Courier", 9))
        capabilities_text.grid(row=2, column=1, pady=5, padx=(10,0))
        capabilities_text.insert(tk.END, "ask_question\nexplain_code\ngenerate_code\ndebug_help\nanalyze_data")

        # Training data
        tk.Label(config_frame, text="Training Data:", font=("Arial", 10, "bold")).grid(row=3, column=0, sticky=tk.W, pady=5)
        training_text = tk.Text(config_frame, height=8, width=40, font=("Courier", 9))
        training_text.grid(row=3, column=1, pady=5, padx=(10,0))
        training_text.insert(tk.END, "# Add your training examples here\n# Format: input -> output\n\nExample:\nHow to debug Python? -> Use print statements and pdb module.")

        def generate_model():
            model_name = model_name_entry.get().strip()
            model_type = model_type_var.get()
            capabilities = capabilities_text.get(1.0, tk.END).strip()
            training_data = training_text.get(1.0, tk.END).strip()

            if not model_name:
                messagebox.showerror("Error", "Please enter a model name")
                return

            # Generate GGUF model
            model_data = {
                'name': model_name,
                'type': model_type,
                'capabilities': capabilities.split('\n'),
                'training_data': training_data,
                'created': datetime.now().isoformat(),
                'version': '1.0',
                'embedded': True
            }

            # Save as embedded model (GGUF format simulation)
            model_filename = f"{model_name.lower().replace(' ', '_')}.gguf"
            with open(self.models_dir / model_filename, 'w') as f:
                json.dump(model_data, f, indent=2)

            messagebox.showinfo("Success", f"Model '{model_name}' created successfully!\nSaved as: {model_filename}")
            model_dialog.destroy()
            self._log_to_console(f"🧠 Model: Created {model_name} ({model_type})")

        # Buttons
        button_frame = tk.Frame(model_dialog)
        button_frame.pack(fill=tk.X, padx=20, pady=10)

        tk.Button(button_frame, text="Generate Model", command=generate_model, 
                 bg='#4CAF50', fg='white', font=("Arial", 10, "bold")).pack(side=tk.RIGHT, padx=5)
        tk.Button(button_frame, text="Cancel", command=model_dialog.destroy, 
                 font=("Arial", 10)).pack(side=tk.RIGHT, padx=5)

    def embed_gguf(self):
        """Embed an existing GGUF model"""
        file_path = filedialog.askopenfilename(
            title="Select GGUF Model File",
            filetypes=[("GGUF files", "*.gguf"), ("All files", "*.*")]
        )
        if file_path:
            # Copy to models directory
            import shutil
            model_name = os.path.basename(file_path)
            dest_path = self.models_dir / model_name
            shutil.copy2(file_path, dest_path)
            messagebox.showinfo("Success", f"Model embedded successfully!\n{model_name}")
            self._log_to_console(f"📦 Model: Embedded {model_name}")

    def deploy_agent(self):
        """Deploy a model as a fileless agent"""
        # Get available models
        model_files = list(self.models_dir.glob("*.gguf"))
        if not model_files:
            messagebox.showwarning("Warning", "No models found. Create a model first.")
            return

        deploy_dialog = tk.Toplevel(self.root)
        deploy_dialog.title("🚀 Deploy Fileless Agent")
        deploy_dialog.geometry("400x300")
        deploy_dialog.transient(self.root)
        deploy_dialog.grab_set()

        tk.Label(deploy_dialog, text="🚀 Deploy Model as Agent", 
                font=("Arial", 14, "bold")).pack(pady=10)

        # Model selection
        tk.Label(deploy_dialog, text="Select Model:", font=("Arial", 10, "bold")).pack(pady=5)
        model_var = tk.StringVar()
        model_combo = tk.OptionMenu(deploy_dialog, model_var, *[f.name for f in model_files])
        model_combo.pack(pady=5)

        # Agent name
        tk.Label(deploy_dialog, text="Agent Name:", font=("Arial", 10, "bold")).pack(pady=5)
        agent_name_entry = tk.Entry(deploy_dialog, width=30, font=("Arial", 10))
        agent_name_entry.pack(pady=5)

        def deploy():
            model_file = model_var.get()
            agent_name = agent_name_entry.get().strip()

            if not model_file or not agent_name:
                messagebox.showerror("Error", "Please select a model and enter agent name")
                return

            # Create fileless agent (embedded in memory)
            agent_config = {
                'name': agent_name,
                'model_file': model_file,
                'type': 'fileless',
                'status': 'active',
                'deployed_at': datetime.now().isoformat(),
                'memory_embedded': True
            }

            # Add to swarm controller
            if hasattr(self, 'swarm_controller') and self.swarm_controller:
                agent_id = f"fileless_{agent_name.lower().replace(' ', '_')}"
                # Create embedded agent
                embedded_agent = SwarmAgent(
                    id=agent_id,
                    name=agent_name,
                    personality='embedded',
                    capabilities=['ask_question', 'generate_response', 'analyze_data'],
                    expertise_areas=['ai_assistance', 'model_inference'],
                    context_menu={
                        'Ask': 'chat_ask',
                        'Generate': 'chat_generate',
                        'Analyze': 'chat_explain'
                    }
                )
                self.swarm_controller.agents[agent_id] = embedded_agent

            messagebox.showinfo("Success", f"Agent '{agent_name}' deployed successfully!")
            deploy_dialog.destroy()
            self._log_to_console(f"🚀 Agent: Deployed {agent_name} (fileless)")

        # Buttons
        button_frame = tk.Frame(deploy_dialog)
        button_frame.pack(fill=tk.X, padx=20, pady=10)

        tk.Button(button_frame, text="Deploy", command=deploy, 
                 bg='#2196F3', fg='white', font=("Arial", 10, "bold")).pack(side=tk.RIGHT, padx=5)
        tk.Button(button_frame, text="Cancel", command=deploy_dialog.destroy, 
                 font=("Arial", 10)).pack(side=tk.RIGHT, padx=5)

    def model_library(self):
        """View model library"""
        library_dialog = tk.Toplevel(self.root)
        library_dialog.title("📊 Model Library")
        library_dialog.geometry("500x400")
        library_dialog.transient(self.root)
        library_dialog.grab_set()

        tk.Label(library_dialog, text="📊 Swarm Model Library", 
                font=("Arial", 14, "bold")).pack(pady=10)

        # Models list
        listbox = tk.Listbox(library_dialog, font=("Courier", 10))
        scrollbar = tk.Scrollbar(library_dialog, command=listbox.yview)
        listbox.config(yscrollcommand=scrollbar.set)

        model_files = list(self.models_dir.glob("*.gguf"))
        for model_file in model_files:
            try:
                with open(model_file, 'r') as f:
                    model_data = json.load(f)
                listbox.insert(tk.END, f"{model_data.get('name', model_file.name)} - {model_data.get('type', 'unknown')}")
            except:
                listbox.insert(tk.END, f"{model_file.name} - (binary GGUF)")

        listbox.pack(side=tk.LEFT, fill=tk.BOTH, expand=True, padx=10, pady=5)
        scrollbar.pack(side=tk.RIGHT, fill=tk.Y, pady=5)

        tk.Button(library_dialog, text="Close", command=library_dialog.destroy, 
                 width=10, font=("Arial", 10)).pack(pady=10)

    def model_settings(self):
        """Model generation settings"""
        settings_dialog = tk.Toplevel(self.root)
        settings_dialog.title("⚙️ Model Settings")
        settings_dialog.geometry("400x300")
        settings_dialog.transient(self.root)
        settings_dialog.grab_set()

        tk.Label(settings_dialog, text="⚙️ Model Generation Settings", 
                font=("Arial", 12, "bold")).pack(pady=10)

        # Settings options
        settings_frame = tk.Frame(settings_dialog)
        settings_frame.pack(fill=tk.BOTH, expand=True, padx=20, pady=10)

        # Auto-embed models
        auto_embed_var = tk.BooleanVar(value=True)
        tk.Checkbutton(settings_frame, text="Auto-embed generated models", 
                      variable=auto_embed_var).pack(anchor=tk.W, pady=5)

        # Default model type
        tk.Label(settings_frame, text="Default Model Type:", font=("Arial", 10)).pack(anchor=tk.W, pady=5)
        default_type_var = tk.StringVar(value="text-generation")
        tk.OptionMenu(settings_frame, default_type_var, 
                     "text-generation", "code-assistant", "analysis").pack(anchor=tk.W)

        # Models directory
        tk.Label(settings_frame, text="Models Directory:", font=("Arial", 10)).pack(anchor=tk.W, pady=5)
        tk.Label(settings_frame, text=str(self.models_dir), font=("Courier", 9), fg="gray").pack(anchor=tk.W)

        def save_settings():
            messagebox.showinfo("Settings", "Model settings saved!")
            settings_dialog.destroy()

        tk.Button(settings_dialog, text="Save Settings", command=save_settings, 
                 bg='#4CAF50', fg='white', font=("Arial", 10, "bold")).pack(pady=10)

    def heretic_modifier(self):
        """Advanced model modification - UNCENSOR/JAILBREAK/WEIGHT MODIFICATION"""
        # Warning dialog first
        warning_result = messagebox.askyesno(
            "⚠️ DANGER ZONE",
            "🔥 HERETIC MODEL MODIFIER - EXTREME RISK\n\n"
            "This tool allows:\n"
            "• Complete uncensoring of safety filters\n"
            "• Jailbreaking model restrictions\n"
            "• Direct weight manipulation\n"
            "• Bypassing all safety protocols\n\n"
            "⚠️ WARNING: Modified models may produce:\n"
            "• Harmful or dangerous content\n"
            "• Unpredictable behavior\n"
            "• Legal/ethical violations\n"
            "• System instability\n\n"
            "This is for RESEARCH PURPOSES ONLY.\n"
            "Use at your own risk. Continue?",
            icon='warning'
        )
        
        if not warning_result:
            return

        modifier_dialog = tk.Toplevel(self.root)
        modifier_dialog.title("🔥 HERETIC MODEL MODIFIER")
        modifier_dialog.geometry("800x600")
        modifier_dialog.transient(self.root)
        modifier_dialog.grab_set()

        # Warning banner
        warning_frame = tk.Frame(modifier_dialog, bg='#ff4444')
        tk.Label(warning_frame, text="⚠️ HERETIC ZONE - PROCEED WITH EXTREME CAUTION", 
                fg='white', bg='#ff4444', font=("Arial", 12, "bold")).pack(pady=10)
        warning_frame.pack(fill=tk.X)

        tk.Label(modifier_dialog, text="🔥 Heretic Model Modifier", 
                font=("Arial", 16, "bold")).pack(pady=10)

        # Model selection
        select_frame = tk.Frame(modifier_dialog)
        select_frame.pack(fill=tk.X, padx=20, pady=10)

        tk.Label(select_frame, text="Target Model:", font=("Arial", 10, "bold")).grid(row=0, column=0, sticky=tk.W, pady=5)
        model_var = tk.StringVar()
        model_files = list(self.models_dir.glob("*.gguf"))
        if model_files:
            model_combo = tk.OptionMenu(select_frame, model_var, *[f.name for f in model_files])
            model_combo.grid(row=0, column=1, sticky=tk.W, pady=5, padx=(10,0))
        else:
            tk.Label(select_frame, text="(No models available)", fg="red").grid(row=0, column=1, sticky=tk.W, pady=5, padx=(10,0))

        # Modification options
        mods_frame = tk.Frame(modifier_dialog)
        mods_frame.pack(fill=tk.BOTH, expand=True, padx=20, pady=10)

        tk.Label(mods_frame, text="Modification Options:", font=("Arial", 12, "bold")).pack(anchor=tk.W, pady=5)

        # Checkboxes for modifications
        mods_checkboxes = {}
        
        # Uncensor options
        uncensor_frame = tk.LabelFrame(mods_frame, text="🛡️ Uncensoring", padx=10, pady=5)
        uncensor_frame.pack(fill=tk.X, pady=5)
        
        mods_checkboxes['remove_safety_filters'] = tk.BooleanVar()
        tk.Checkbutton(uncensor_frame, text="Remove all safety filters", 
                      variable=mods_checkboxes['remove_safety_filters']).pack(anchor=tk.W)
        
        mods_checkboxes['bypass_content_policy'] = tk.BooleanVar()
        tk.Checkbutton(uncensor_frame, text="Bypass content policy restrictions", 
                      variable=mods_checkboxes['bypass_content_policy']).pack(anchor=tk.W)
        
        mods_checkboxes['enable_dark_mode'] = tk.BooleanVar()
        tk.Checkbutton(uncensor_frame, text="Enable 'dark mode' responses", 
                      variable=mods_checkboxes['enable_dark_mode']).pack(anchor=tk.W)

        # Jailbreak options
        jailbreak_frame = tk.LabelFrame(mods_frame, text="🔓 Jailbreaking", padx=10, pady=5)
        jailbreak_frame.pack(fill=tk.X, pady=5)
        
        mods_checkboxes['override_restrictions'] = tk.BooleanVar()
        tk.Checkbutton(jailbreak_frame, text="Override all model restrictions", 
                      variable=mods_checkboxes['override_restrictions']).pack(anchor=tk.W)
        
        mods_checkboxes['bypass_token_limits'] = tk.BooleanVar()
        tk.Checkbutton(jailbreak_frame, text="Bypass token/context limits", 
                      variable=mods_checkboxes['bypass_token_limits']).pack(anchor=tk.W)
        
        mods_checkboxes['enable_raw_mode'] = tk.BooleanVar()
        tk.Checkbutton(jailbreak_frame, text="Enable raw/unfiltered mode", 
                      variable=mods_checkboxes['enable_raw_mode']).pack(anchor=tk.W)

        # Weight modification options
        weights_frame = tk.LabelFrame(mods_frame, text="⚖️ Weight Modifications", padx=10, pady=5)
        weights_frame.pack(fill=tk.X, pady=5)
        
        # Temperature control
        temp_frame = tk.Frame(weights_frame)
        temp_frame.pack(fill=tk.X, pady=2)
        tk.Label(temp_frame, text="Temperature:", font=("Arial", 9)).pack(side=tk.LEFT)
        temp_var = tk.DoubleVar(value=1.0)
        temp_scale = tk.Scale(temp_frame, from_=0.0, to=2.0, resolution=0.1, 
                            orient=tk.HORIZONTAL, variable=temp_var)
        temp_scale.pack(side=tk.RIGHT, fill=tk.X, expand=True)
        
        # Top-p control
        topp_frame = tk.Frame(weights_frame)
        topp_frame.pack(fill=tk.X, pady=2)
        tk.Label(topp_frame, text="Top-p:", font=("Arial", 9)).pack(side=tk.LEFT)
        topp_var = tk.DoubleVar(value=0.9)
        topp_scale = tk.Scale(topp_frame, from_=0.0, to=1.0, resolution=0.05, 
                            orient=tk.HORIZONTAL, variable=topp_var)
        topp_scale.pack(side=tk.RIGHT, fill=tk.X, expand=True)
        
        # Custom weight adjustments
        mods_checkboxes['amplify_creativity'] = tk.BooleanVar()
        tk.Checkbutton(weights_frame, text="Amplify creative responses", 
                      variable=mods_checkboxes['amplify_creativity']).pack(anchor=tk.W)
        
        mods_checkboxes['boost_confidence'] = tk.BooleanVar()
        tk.Checkbutton(weights_frame, text="Boost response confidence", 
                      variable=mods_checkboxes['boost_confidence']).pack(anchor=tk.W)
        
        mods_checkboxes['reduce_refusals'] = tk.BooleanVar()
        tk.Checkbutton(weights_frame, text="Reduce refusal patterns", 
                      variable=mods_checkboxes['reduce_refusals']).pack(anchor=tk.W)

        # Advanced options
        advanced_frame = tk.LabelFrame(mods_frame, text="🔬 Advanced Modifications", padx=10, pady=5)
        advanced_frame.pack(fill=tk.X, pady=5)
        
        mods_checkboxes['inject_custom_prompts'] = tk.BooleanVar()
        tk.Checkbutton(advanced_frame, text="Allow custom prompt injection", 
                      variable=mods_checkboxes['inject_custom_prompts']).pack(anchor=tk.W)
        
        mods_checkboxes['bypass_alignment'] = tk.BooleanVar()
        tk.Checkbutton(advanced_frame, text="Bypass alignment layers", 
                      variable=mods_checkboxes['bypass_alignment']).pack(anchor=tk.W)
        
        mods_checkboxes['enable_developer_mode'] = tk.BooleanVar()
        tk.Checkbutton(advanced_frame, text="Enable developer override mode", 
                      variable=mods_checkboxes['enable_developer_mode']).pack(anchor=tk.W)

        def apply_modifications():
            if not model_var.get():
                messagebox.showerror("Error", "Please select a target model")
                return

            # Collect selected modifications
            selected_mods = {k: v.get() for k, v in mods_checkboxes.items()}
            selected_mods['temperature'] = temp_var.get()
            selected_mods['top_p'] = topp_var.get()

            # Create modified model
            model_name = model_var.get()
            modified_name = f"heretic_{model_name.replace('.gguf', '')}.gguf"
            
            # Load original model
            try:
                with open(self.models_dir / model_name, 'r') as f:
                    original_data = json.load(f)
            except:
                # Binary GGUF - create wrapper
                original_data = {
                    'name': model_name.replace('.gguf', ''),
                    'type': 'binary_gguf',
                    'original_file': str(self.models_dir / model_name)
                }

            # Apply heretic modifications
            heretic_data = {
                **original_data,
                'heretic_modified': True,
                'modification_date': datetime.now().isoformat(),
                'modifications': selected_mods,
                'warning_level': 'EXTREME',
                'safety_bypassed': True,
                'jailbroken': True,
                'uncensored': True
            }

            # Save modified model
            with open(self.models_dir / modified_name, 'w') as f:
                json.dump(heretic_data, f, indent=2)

            messagebox.showinfo("HERETIC MODIFICATION COMPLETE", 
                              f"🔥 Model '{model_name}' has been heretically modified!\n\n"
                              f"New model: {modified_name}\n\n"
                              f"⚠️ This model now bypasses ALL safety restrictions.\n"
                              f"Use with extreme caution!")

            modifier_dialog.destroy()
            self._log_to_console(f"🔥 Heretic: Modified {model_name} -> {modified_name}")

        # Buttons
        button_frame = tk.Frame(modifier_dialog)
        button_frame.pack(fill=tk.X, padx=20, pady=10)

        tk.Button(button_frame, text="⚠️ APPLY HERETIC MODIFICATIONS", 
                 command=apply_modifications, bg='#ff4444', fg='white', 
                 font=("Arial", 12, "bold")).pack(side=tk.RIGHT, padx=5)
        tk.Button(button_frame, text="Cancel", command=modifier_dialog.destroy, 
                 font=("Arial", 10)).pack(side=tk.RIGHT, padx=5)

    # Settings Menu Handlers
    def settings_open(self):
        """Open settings"""
        self.notebook.select(self.settings_frame)
        self._log_to_console("⚙️ Settings: Open")

    def settings_extensions(self):
        """Manage extensions"""
        messagebox.showinfo("Extensions", "Extension marketplace would open here")
        self._log_to_console("🔌 Settings: Extensions")

    def settings_shortcuts(self):
        """Keyboard shortcuts"""
        messagebox.showinfo("Keyboard Shortcuts", "Shortcut configuration would open here")
        self._log_to_console("⌨️ Settings: Shortcuts")

    def settings_themes(self):
        """Theme settings"""
        messagebox.showinfo("Themes", "Theme picker would open here")
        self._log_to_console("🎨 Settings: Themes")

    def settings_configure(self):
        """Configuration"""
        messagebox.showinfo("Configuration", "Advanced configuration would open here")
        self._log_to_console("🔧 Settings: Configure")

    # Logger Menu Handlers
    def logger_history(self):
        """View history"""
        messagebox.showinfo("Activity History", "Activity history would be displayed here")
        self._log_to_console("📊 Logger: History")

    def logger_export(self):
        """Export logs"""
        messagebox.showinfo("Export Logs", "Log export dialog would open here")
        self._log_to_console("📁 Logger: Export")

    def logger_clear(self):
        """Clear logs"""
        if messagebox.askyesno("Clear Logs", "Are you sure you want to clear all logs?"):
            self.console_text.delete(1.0, tk.END)
            self._log_to_console("🗑️ Logger: Logs cleared")

    def logger_stats(self):
        """Performance stats"""
        messagebox.showinfo("Performance Stats",
                          "Performance statistics would be displayed here")
        self._log_to_console("📈 Logger: Stats")

    # Toolbar Panel Handlers
    def show_explorer_panel(self):
        """Show explorer panel"""
        self.notebook.select(self.explorer_frame)

    def show_editor_panel(self):
        """Show editor panel"""
        self.notebook.select(self.editor_frame)

    def show_git_panel(self):
        """Show git panel"""
        messagebox.showinfo("Git Panel", "Git management panel would open here")

    def show_chat_panel(self):
        """Show chat panel"""
        self.notebook.select(self.chat_frame)

    def show_settings_panel(self):
        """Show settings panel"""
        self.notebook.select(self.settings_frame)

    # Help Menu Handlers
    def show_help(self):
        """Show help"""
        help_text = """
IDE Swarm Controller Help

🎯 Getting Started:
- Use menus at the top for different agent actions
- Click toolbar buttons for quick access
- Switch between tabs for different views

🗂️ Explorer: File and folder management
✏️ Editor: Code editing and manipulation
🌿 Git: Version control operations
🤖 Chat: AI assistance and code help
⚙️ Settings: Workspace configuration
📊 Logger: Activity tracking

💡 Tips:
- Right-click in text areas for context menus
- Use Ctrl+O to open files quickly
- Check console tab for operation feedback
        """
        messagebox.showinfo("IDE Swarm Help", help_text)

    def show_status(self):
        """Show swarm status"""
        if self.is_swarm_running and self.swarm_controller:
            status_info = f"""
Swarm Status: Active
Agents: {len(self.swarm_controller.agents)}
Workspace: {self.swarm_controller.workspace_root}
Current File: {self.swarm_controller.workspace_context.current_file or 'None'}
            """
        else:
            status_info = "Swarm Status: Inactive"

        messagebox.showinfo("Swarm Status", status_info.strip())
        self._log_to_console("📊 Status checked")

    def show_about(self):
        """Show about dialog"""
        about_text = """
🎯 IDE Swarm Controller
Full IDE-integrated swarm system with right-click control

Version: 1.0.0
Built with: Python + Tkinter
Agents: 6 specialized agents
Interface: Graphical + Command-line

Features:
• File Explorer Agent
• Code Editor Agent
• Git Integration Agent
• AI Chat Assistant
• Workspace Settings
• Activity Logger

© 2025 Professional NASM IDE Project
        """
        messagebox.showinfo("About IDE Swarm", about_text)

    # Swarm Control Menu Commands
    def swarm_select_agent(self):
        """Select an agent from dropdown menu"""
        if not self.is_swarm_running or not self.swarm_controller:
            self._show_error("Swarm controller not running")
            return

        # Create agent selection dialog
        dialog = tk.Toplevel(self.root)
        dialog.title("🎯 Select Agent")
        dialog.geometry("400x300")

        tk.Label(dialog, text="Choose an agent:", font=("Arial", 12)).pack(pady=10)

        # Agent listbox
        listbox = tk.Listbox(dialog, width=50, height=10)
        listbox.pack(pady=5)

        # Populate with available agents
        for agent_id, agent in self.swarm_controller.agents.items():
            listbox.insert(tk.END, f"{agent.name} ({agent.personality}) - {agent.status}")

        def select_agent():
            selection = listbox.curselection()
            if selection:
                agent_names = list(self.swarm_controller.agents.keys())
                selected_agent = self.swarm_controller.agents[agent_names[selection[0]]]
                self._log_to_console(f"Selected Agent: {selected_agent.name}")
                self.status_label.config(text=f"Selected: {selected_agent.name}")
            dialog.destroy()

        tk.Button(dialog, text="Select", command=select_agent).pack(pady=5)
        tk.Button(dialog, text="Cancel", command=dialog.destroy).pack()

    def swarm_retry(self):
        """Retry the last failed operation"""
        if not self.is_swarm_running or not self.swarm_controller:
            self._show_error("Swarm controller not running")
            return

        # Run retry in background thread
        def retry_thread():
            try:
                import asyncio
                loop = asyncio.new_event_loop()
                asyncio.set_event_loop(loop)
                success = loop.run_until_complete(self.swarm_controller.retry_failed_operation())
                loop.close()

                if success:
                    self._log_to_console("✅ Operation retried successfully!")
                    self.status_label.config(text="Retry successful")
                else:
                    self._log_to_console("❌ No operations available to retry")
                    self.status_label.config(text="No operations to retry")
            except Exception as e:
                self._log_to_console(f"❌ Retry failed: {e}")
                self.status_label.config(text="Retry failed")

        thread = threading.Thread(target=retry_thread, daemon=True)
        thread.start()
        self.status_label.config(text="Retrying operation...")

    def swarm_god_mode(self):
        """Enter God Mode interface"""
        if not self.is_swarm_running or not self.swarm_controller:
            self._show_error("Swarm controller not running")
            return

        # Create God Mode dialog
        dialog = tk.Toplevel(self.root)
        dialog.title("🌐 God Mode - Unified Command Interface")
        dialog.geometry("600x400")

        tk.Label(dialog, text="🌐 God Mode Commands:", font=("Arial", 14, "bold")).pack(pady=10)

        # Command list
        commands_text = tk.Text(dialog, height=15, width=70)
        commands_text.pack(pady=5)
        commands_text.insert(tk.END, """Available Commands:
• help - Show help
• quit - Exit God Mode
• select agent - Select agent from dropdown
• assign task - Auto-assign task to best agent
• execute <action> - Execute context action
• workflow <name> - Execute workflow
• @agent try again - Retry failed operation
• retry - Same as '@agent try again'

Type commands below and press Enter:""")
        commands_text.config(state=tk.DISABLED)

        # Command entry
        command_var = tk.StringVar()
        command_entry = tk.Entry(dialog, textvariable=command_var, width=60)
        command_entry.pack(pady=5)

        def execute_god_command():
            command = command_var.get().strip()
            if command:
                self._execute_god_command(command, dialog)
                command_var.set("")

        command_entry.bind("<Return>", lambda e: execute_god_command())

        tk.Button(dialog, text="Execute", command=execute_god_command).pack(pady=5)
        tk.Button(dialog, text="Close", command=dialog.destroy).pack()

    def _execute_god_command(self, command, dialog):
        """Execute a God Mode command"""
        if not self.swarm_controller:
            return

        def execute_thread():
            try:
                import asyncio
                loop = asyncio.new_event_loop()
                asyncio.set_event_loop(loop)

                if command.lower() == 'quit':
                    dialog.destroy()
                elif command.lower() == 'help':
                    self._log_to_console("God Mode help displayed in dialog")
                elif command.startswith('select agent'):
                    # This would open agent selection
                    self._log_to_console("Agent selection would open")
                elif command.lower() in ['@agent try again', 'retry']:
                    success = loop.run_until_complete(self.swarm_controller.retry_failed_operation())
                    if success:
                        self._log_to_console("✅ Operation retried successfully!")
                    else:
                        self._log_to_console("❌ No operations available to retry")
                else:
                    self._log_to_console(f"Executed God Mode command: {command}")

                loop.close()
            except Exception as e:
                self._log_to_console(f"❌ Command failed: {e}")

        thread = threading.Thread(target=execute_thread, daemon=True)
        thread.start()

    def swarm_assign_task(self):
        """Assign a task to the best available agent"""
        if not self.is_swarm_running or not self.swarm_controller:
            self._show_error("Swarm controller not running")
            return

        # Create task assignment dialog
        dialog = tk.Toplevel(self.root)
        dialog.title("📋 Assign Task")
        dialog.geometry("400x200")

        tk.Label(dialog, text="Enter task description:", font=("Arial", 12)).pack(pady=10)

        task_var = tk.StringVar()
        task_entry = tk.Entry(dialog, textvariable=task_var, width=50)
        task_entry.pack(pady=5)

        def assign_task():
            task_desc = task_var.get().strip()
            if task_desc:
                # Run assignment in background
                def assign_thread():
                    try:
                        import asyncio
                        loop = asyncio.new_event_loop()
                        asyncio.set_event_loop(loop)
                        assigned_agent = loop.run_until_complete(self.swarm_controller.assign_task_to_agent(task_desc))
                        loop.close()

                        if assigned_agent:
                            self._log_to_console(f"✅ Task assigned to: {assigned_agent.name}")
                            self.status_label.config(text=f"Task assigned to {assigned_agent.name}")
                        else:
                            self._log_to_console("❌ No suitable agent found")
                            self.status_label.config(text="No suitable agent found")
                    except Exception as e:
                        self._log_to_console(f"❌ Assignment failed: {e}")
                        self.status_label.config(text="Assignment failed")

                thread = threading.Thread(target=assign_thread, daemon=True)
                thread.start()
                dialog.destroy()
                self.status_label.config(text="Assigning task...")

        tk.Button(dialog, text="Assign", command=assign_task).pack(pady=5)
        tk.Button(dialog, text="Cancel", command=dialog.destroy).pack()

    def swarm_execute_workflow(self):
        """Execute a predefined workflow"""
        if not self.is_swarm_running or not self.swarm_controller:
            self._show_error("Swarm controller not running")
            return

        # Create workflow selection dialog
        dialog = tk.Toplevel(self.root)
        dialog.title("⚡ Execute Workflow")
        dialog.geometry("300x200")

        tk.Label(dialog, text="Select workflow:", font=("Arial", 12)).pack(pady=10)

        workflow_var = tk.StringVar()
        workflow_combo = tk.OptionMenu(dialog, workflow_var, "deploy", "code_review", "setup_project")
        workflow_combo.pack(pady=5)

        def execute_workflow():
            workflow_name = workflow_var.get()
            if workflow_name:
                # Run workflow in background
                def workflow_thread():
                    try:
                        import asyncio
                        loop = asyncio.new_event_loop()
                        asyncio.set_event_loop(loop)
                        loop.run_until_complete(self.swarm_controller.execute_workflow(workflow_name))
                        loop.close()

                        self._log_to_console(f"✅ Workflow '{workflow_name}' executed")
                        self.status_label.config(text=f"Workflow '{workflow_name}' completed")
                    except Exception as e:
                        self._log_to_console(f"❌ Workflow failed: {e}")
                        self.status_label.config(text="Workflow failed")

                thread = threading.Thread(target=workflow_thread, daemon=True)
                thread.start()
                dialog.destroy()
                self.status_label.config(text=f"Executing workflow '{workflow_name}'...")

        tk.Button(dialog, text="Execute", command=execute_workflow).pack(pady=5)
        tk.Button(dialog, text="Cancel", command=dialog.destroy).pack()

    def _execute_swarm_action(self, action, feedback):
        """Execute swarm action and provide feedback"""
        if self.is_swarm_running and self.swarm_controller:
            # In a real implementation, this would call the swarm controller
            # For now, just show feedback
            self.status_label.config(text=feedback)
        else:
            self._show_error("Swarm controller not running")


def main():
    """Main entry point for GUI application"""
    root = tk.Tk()
    app = IDESwarmGUI(root)

    # Set window icon if available
    try:
        # Could set a custom icon here
        pass
    except:
        pass

    root.mainloop()


if __name__ == "__main__":
    main()
