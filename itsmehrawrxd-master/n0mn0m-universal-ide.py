#!/usr/bin/env python3
"""
n0mn0m Universal IDE - Simple Cross-Platform Version
Works on: Windows, macOS, Linux, and can be adapted for mobile
"""

import tkinter as tk
from tkinter import ttk, filedialog, messagebox, scrolledtext
import os
import sys
import platform
import subprocess
import threading
import json
from pathlib import Path

class UniversalIDE:
    def __init__(self):
        self.root = tk.Tk()
        self.root.title("n0mn0m Universal IDE")
        self.root.geometry("1200x800")
        
        # Platform detection
        self.platform = platform.system()
        self.is_mobile = self.platform in ["Android", "iOS"]
        
        # Current file info
        self.current_file = None
        self.is_modified = False
        
        # Setup UI
        self.setup_ui()
        self.setup_keybindings()
        
        print(f" n0mn0m IDE started on {self.platform}")
    
    def setup_ui(self):
        """Setup the main UI components"""
        
        # Menu bar
        self.setup_menu()
        
        # Main container with paned windows
        main_paned = ttk.PanedWindow(self.root, orient=tk.HORIZONTAL)
        main_paned.pack(fill=tk.BOTH, expand=True)
        
        # Left panel (file explorer)
        self.setup_file_explorer(main_paned)
        
        # Right panel (editor + output)
        right_paned = ttk.PanedWindow(main_paned, orient=tk.VERTICAL)
        main_paned.add(right_paned)
        
        # Editor area
        self.setup_editor(right_paned)
        
        # Output panel
        self.setup_output_panel(right_paned)
        
        # Status bar
        self.setup_status_bar()
    
    def setup_menu(self):
        """Create menu bar"""
        menubar = tk.Menu(self.root)
        self.root.config(menu=menubar)
        
        # File menu
        file_menu = tk.Menu(menubar, tearoff=0)
        menubar.add_cascade(label="File", menu=file_menu)
        file_menu.add_command(label="New", command=self.new_file, accelerator="Ctrl+N")
        file_menu.add_command(label="Open", command=self.open_file, accelerator="Ctrl+O")
        file_menu.add_command(label="Save", command=self.save_file, accelerator="Ctrl+S")
        file_menu.add_command(label="Save As", command=self.save_as_file)
        file_menu.add_separator()
        file_menu.add_command(label="Exit", command=self.exit_app)
        
        # Edit menu
        edit_menu = tk.Menu(menubar, tearoff=0)
        menubar.add_cascade(label="Edit", menu=edit_menu)
        edit_menu.add_command(label="Undo", command=lambda: self.editor.event_generate("<<Undo>>"))
        edit_menu.add_command(label="Redo", command=lambda: self.editor.event_generate("<<Redo>>"))
        edit_menu.add_separator()
        edit_menu.add_command(label="Cut", command=lambda: self.editor.event_generate("<<Cut>>"))
        edit_menu.add_command(label="Copy", command=lambda: self.editor.event_generate("<<Copy>>"))
        edit_menu.add_command(label="Paste", command=lambda: self.editor.event_generate("<<Paste>>"))
        edit_menu.add_command(label="Find", command=self.find_text, accelerator="Ctrl+F")
        
        # Run menu
        run_menu = tk.Menu(menubar, tearoff=0)
        menubar.add_cascade(label="Run", menu=run_menu)
        run_menu.add_command(label="Run File", command=self.run_file, accelerator="F5")
        run_menu.add_command(label="Run in Terminal", command=self.run_in_terminal)
        
        # Tools menu
        tools_menu = tk.Menu(menubar, tearoff=0)
        menubar.add_cascade(label="Tools", menu=tools_menu)
        tools_menu.add_command(label="Terminal", command=self.open_terminal)
        tools_menu.add_command(label="Git Status", command=self.git_status)
        
        # View menu
        view_menu = tk.Menu(menubar, tearoff=0)
        menubar.add_cascade(label="View", menu=view_menu)
        view_menu.add_command(label="Toggle Dark Mode", command=self.toggle_dark_mode)
        view_menu.add_command(label="Zoom In", command=self.zoom_in, accelerator="Ctrl++")
        view_menu.add_command(label="Zoom Out", command=self.zoom_out, accelerator="Ctrl+-")
    
    def setup_file_explorer(self, parent):
        """Setup file explorer panel"""
        explorer_frame = ttk.Frame(parent)
        parent.add(explorer_frame)
        
        ttk.Label(explorer_frame, text=" Files", font=("Arial", 12, "bold")).pack(pady=5)
        
        # Treeview for files
        self.file_tree = ttk.Treeview(explorer_frame)
        self.file_tree.pack(fill=tk.BOTH, expand=True, padx=5, pady=5)
        
        # Populate with current directory
        self.refresh_file_tree()
        
        # Bind double-click to open file
        self.file_tree.bind("<Double-1>", self.tree_double_click)
    
    def setup_editor(self, parent):
        """Setup code editor"""
        editor_frame = ttk.Frame(parent)
        parent.add(editor_frame)
        
        # Editor with syntax highlighting
        self.editor = scrolledtext.ScrolledText(
            editor_frame,
            font=("Consolas" if self.platform == "Windows" else "Monaco", 12),
            wrap=tk.NONE,
            undo=True,
            maxundo=50
        )
        self.editor.pack(fill=tk.BOTH, expand=True)
        
        # Line numbers (simple implementation)
        self.setup_line_numbers()
        
        # Bind events
        self.editor.bind("<KeyRelease>", self.on_text_change)
        self.editor.bind("<Button-1>", self.on_cursor_change)
    
    def setup_output_panel(self, parent):
        """Setup output/terminal panel"""
        output_frame = ttk.Frame(parent)
        parent.add(output_frame)
        
        ttk.Label(output_frame, text=" Output", font=("Arial", 10, "bold")).pack(anchor="w")
        
        self.output_text = scrolledtext.ScrolledText(
            output_frame,
            height=8,
            font=("Consolas" if self.platform == "Windows" else "Monaco", 10),
            bg="black",
            fg="lime"
        )
        self.output_text.pack(fill=tk.BOTH, expand=True, padx=5, pady=5)
        
        # Welcome message
        self.log_output(f" n0mn0m IDE ready on {self.platform}!")
        self.log_output(f" Python {sys.version}")
        self.log_output(f" Working directory: {os.getcwd()}")
    
    def setup_status_bar(self):
        """Setup status bar"""
        self.status_bar = ttk.Label(
            self.root,
            text="Ready",
            relief=tk.SUNKEN,
            anchor="w"
        )
        self.status_bar.pack(side=tk.BOTTOM, fill=tk.X)
    
    def setup_line_numbers(self):
        """Add simple line numbers to editor"""
        # This is a simple implementation - could be enhanced
        pass
    
    def setup_keybindings(self):
        """Setup keyboard shortcuts"""
        self.root.bind("<Control-n>", lambda e: self.new_file())
        self.root.bind("<Control-o>", lambda e: self.open_file())
        self.root.bind("<Control-s>", lambda e: self.save_file())
        self.root.bind("<Control-f>", lambda e: self.find_text())
        self.root.bind("<F5>", lambda e: self.run_file())
        self.root.bind("<Control-plus>", lambda e: self.zoom_in())
        self.root.bind("<Control-minus>", lambda e: self.zoom_out())
    
    def refresh_file_tree(self):
        """Refresh the file explorer"""
        self.file_tree.delete(*self.file_tree.get_children())
        
        def add_to_tree(parent, path):
            try:
                for item in sorted(os.listdir(path)):
                    if item.startswith('.'):
                        continue
                    
                    full_path = os.path.join(path, item)
                    if os.path.isdir(full_path):
                        folder_id = self.file_tree.insert(parent, "end", text=f" {item}", values=[full_path])
                        # Add a dummy child so we can expand folders
                        self.file_tree.insert(folder_id, "end", text="...")
                    else:
                        # Choose icon based on file extension
                        icon = self.get_file_icon(item)
                        self.file_tree.insert(parent, "end", text=f"{icon} {item}", values=[full_path])
            except PermissionError:
                pass
        
        # Start with current directory
        add_to_tree("", ".")
    
    def get_file_icon(self, filename):
        """Get appropriate icon for file type"""
        ext = Path(filename).suffix.lower()
        icons = {
            '.py': '', '.js': '', '.html': '', '.css': '',
            '.json': '', '.xml': '', '.txt': '', '.md': '',
            '.jpg': '', '.png': '', '.gif': '',
            '.mp3': '', '.mp4': '', '.zip': ''
        }
        return icons.get(ext, '')
    
    def tree_double_click(self, event):
        """Handle double-click on file tree"""
        selection = self.file_tree.selection()
        if selection:
            item = self.file_tree.item(selection[0])
            file_path = item['values'][0] if item['values'] else None
            if file_path and os.path.isfile(file_path):
                self.open_file_path(file_path)
    
    def new_file(self):
        """Create new file"""
        if self.check_unsaved_changes():
            self.editor.delete(1.0, tk.END)
            self.current_file = None
            self.is_modified = False
            self.update_title()
            self.log_output(" New file created")
    
    def open_file(self):
        """Open file dialog"""
        if not self.check_unsaved_changes():
            return
        
        file_path = filedialog.askopenfilename(
            title="Open File",
            filetypes=[
                ("All Files", "*.*"),
                ("Python Files", "*.py"),
                ("JavaScript Files", "*.js"),
                ("HTML Files", "*.html"),
                ("Text Files", "*.txt")
            ]
        )
        
        if file_path:
            self.open_file_path(file_path)
    
    def open_file_path(self, file_path):
        """Open specific file path"""
        try:
            with open(file_path, 'r', encoding='utf-8') as file:
                content = file.read()
                self.editor.delete(1.0, tk.END)
                self.editor.insert(1.0, content)
                self.current_file = file_path
                self.is_modified = False
                self.update_title()
                self.log_output(f" Opened: {file_path}")
                
                # Simple syntax highlighting
                self.apply_syntax_highlighting()
                
        except Exception as e:
            messagebox.showerror("Error", f"Could not open file:\n{e}")
    
    def save_file(self):
        """Save current file"""
        if self.current_file:
            self.save_file_path(self.current_file)
        else:
            self.save_as_file()
    
    def save_as_file(self):
        """Save as dialog"""
        file_path = filedialog.asksaveasfilename(
            title="Save As",
            defaultextension=".txt",
            filetypes=[
                ("Text Files", "*.txt"),
                ("Python Files", "*.py"),
                ("JavaScript Files", "*.js"),
                ("HTML Files", "*.html"),
                ("All Files", "*.*")
            ]
        )
        
        if file_path:
            self.save_file_path(file_path)
    
    def save_file_path(self, file_path):
        """Save to specific file path"""
        try:
            with open(file_path, 'w', encoding='utf-8') as file:
                content = self.editor.get(1.0, tk.END)
                file.write(content)
                self.current_file = file_path
                self.is_modified = False
                self.update_title()
                self.log_output(f" Saved: {file_path}")
                self.refresh_file_tree()
        except Exception as e:
            messagebox.showerror("Error", f"Could not save file:\n{e}")
    
    def find_text(self):
        """Simple find dialog"""
        search_term = tk.simpledialog.askstring("Find", "Enter text to find:")
        if search_term:
            content = self.editor.get(1.0, tk.END)
            start_pos = content.find(search_term)
            if start_pos != -1:
                # Calculate line and character position
                lines_before = content[:start_pos].count('\n')
                char_pos = start_pos - content.rfind('\n', 0, start_pos) - 1
                
                # Select the found text
                start_index = f"{lines_before + 1}.{char_pos}"
                end_index = f"{lines_before + 1}.{char_pos + len(search_term)}"
                
                self.editor.tag_remove(tk.SEL, 1.0, tk.END)
                self.editor.tag_add(tk.SEL, start_index, end_index)
                self.editor.mark_set(tk.INSERT, start_index)
                self.editor.see(start_index)
            else:
                messagebox.showinfo("Find", f"'{search_term}' not found")
    
    def run_file(self):
        """Run current file"""
        if not self.current_file:
            messagebox.showwarning("Warning", "No file to run")
            return
        
        self.log_output(f"\n Running: {self.current_file}")
        
        # Save first if modified
        if self.is_modified:
            self.save_file()
        
        # Run in separate thread to avoid blocking UI
        thread = threading.Thread(target=self._run_file_thread)
        thread.daemon = True
        thread.start()
    
    def _run_file_thread(self):
        """Run file in separate thread"""
        try:
            ext = Path(self.current_file).suffix.lower()
            
            if ext == '.py':
                cmd = [sys.executable, self.current_file]
            elif ext == '.js':
                cmd = ['node', self.current_file]
            elif ext in ['.html', '.htm']:
                # Open in browser
                if self.platform == "Windows":
                    os.startfile(self.current_file)
                elif self.platform == "Darwin":  # macOS
                    subprocess.run(['open', self.current_file])
                else:  # Linux
                    subprocess.run(['xdg-open', self.current_file])
                return
            else:
                self.log_output(f" Don't know how to run {ext} files")
                return
            
            # Run the command
            process = subprocess.run(
                cmd,
                capture_output=True,
                text=True,
                cwd=os.path.dirname(self.current_file)
            )
            
            if process.stdout:
                self.log_output(f" Output:\n{process.stdout}")
            
            if process.stderr:
                self.log_output(f" Errors:\n{process.stderr}")
            
            self.log_output(f" Process finished with code {process.returncode}")
            
        except Exception as e:
            self.log_output(f" Error running file: {e}")
    
    def run_in_terminal(self):
        """Open terminal in current directory"""
        try:
            if self.platform == "Windows":
                subprocess.Popen(['cmd'], cwd=os.getcwd())
            elif self.platform == "Darwin":  # macOS
                subprocess.Popen(['open', '-a', 'Terminal', '.'])
            else:  # Linux
                subprocess.Popen(['gnome-terminal'], cwd=os.getcwd())
            
            self.log_output(" Terminal opened")
        except Exception as e:
            self.log_output(f" Could not open terminal: {e}")
    
    def open_terminal(self):
        """Open integrated terminal (simplified)"""
        self.log_output(" Integrated terminal would go here (future feature)")
    
    def git_status(self):
        """Show git status"""
        try:
            result = subprocess.run(['git', 'status'], capture_output=True, text=True)
            if result.returncode == 0:
                self.log_output(f" Git Status:\n{result.stdout}")
            else:
                self.log_output(f" Git error: {result.stderr}")
        except Exception as e:
            self.log_output(f" Git not available: {e}")
    
    def toggle_dark_mode(self):
        """Toggle between light and dark mode"""
        current_bg = self.editor.cget('bg')
        if current_bg == 'white' or current_bg == 'SystemWindow':
            # Switch to dark mode
            self.editor.config(bg='#2b2b2b', fg='#ffffff', insertbackground='white')
            self.log_output(" Dark mode enabled")
        else:
            # Switch to light mode
            self.editor.config(bg='white', fg='black', insertbackground='black')
            self.log_output(" Light mode enabled")
    
    def zoom_in(self):
        """Increase font size"""
        current_font = self.editor.cget('font')
        if isinstance(current_font, tuple):
            family, size = current_font[0], current_font[1]
            new_size = min(size + 1, 32)
            self.editor.config(font=(family, new_size))
            self.log_output(f" Font size: {new_size}")
    
    def zoom_out(self):
        """Decrease font size"""
        current_font = self.editor.cget('font')
        if isinstance(current_font, tuple):
            family, size = current_font[0], current_font[1]
            new_size = max(size - 1, 8)
            self.editor.config(font=(family, new_size))
            self.log_output(f" Font size: {new_size}")
    
    def apply_syntax_highlighting(self):
        """Simple syntax highlighting (basic implementation)"""
        if not self.current_file:
            return
        
        ext = Path(self.current_file).suffix.lower()
        content = self.editor.get(1.0, tk.END)
        
        # Clear existing tags
        self.editor.tag_delete("keyword", "string", "comment")
        
        if ext == '.py':
            # Python keywords
            keywords = ['def', 'class', 'if', 'elif', 'else', 'for', 'while', 'try', 'except', 'import', 'from', 'return']
            for keyword in keywords:
                start = 1.0
                while True:
                    start = self.editor.search(f'\\b{keyword}\\b', start, tk.END, regexp=True)
                    if not start:
                        break
                    end = f"{start}+{len(keyword)}c"
                    self.editor.tag_add("keyword", start, end)
                    start = end
            
            # Configure colors
            self.editor.tag_config("keyword", foreground="blue", font=("Consolas", 12, "bold"))
    
    def on_text_change(self, event=None):
        """Handle text changes"""
        if not self.is_modified:
            self.is_modified = True
            self.update_title()
    
    def on_cursor_change(self, event=None):
        """Handle cursor position changes"""
        try:
            line, col = self.editor.index(tk.INSERT).split('.')
            self.status_bar.config(text=f"Line {line}, Column {col}")
        except:
            pass
    
    def update_title(self):
        """Update window title"""
        title = "n0mn0m Universal IDE"
        if self.current_file:
            filename = os.path.basename(self.current_file)
            title = f"{filename}{'*' if self.is_modified else ''} - {title}"
        self.root.title(title)
    
    def check_unsaved_changes(self):
        """Check for unsaved changes"""
        if self.is_modified:
            result = messagebox.askyesnocancel(
                "Unsaved Changes",
                "You have unsaved changes. Do you want to save them?"
            )
            if result is True:  # Yes, save
                self.save_file()
                return True
            elif result is False:  # No, don't save
                return True
            else:  # Cancel
                return False
        return True
    
    def log_output(self, message):
        """Log message to output panel"""
        self.output_text.insert(tk.END, message + "\n")
        self.output_text.see(tk.END)
    
    def exit_app(self):
        """Exit application"""
        if self.check_unsaved_changes():
            self.root.quit()
    
    def run(self):
        """Start the IDE"""
        # Handle window close
        self.root.protocol("WM_DELETE_WINDOW", self.exit_app)
        
        # Start main loop
        self.root.mainloop()

if __name__ == "__main__":
    # Import tkinter simpledialog here to avoid issues
    import tkinter.simpledialog
    
    ide = UniversalIDE()
    ide.run()
