#!/usr/bin/env python3
"""
Visual Studio-Style Tabbed Interface
Tabs positioned under menu bar (File, Edit, View, etc.) with proper integration
"""

import tkinter as tk
from tkinter import ttk, messagebox, filedialog
import os
import sys
from pathlib import Path
from typing import Dict, List, Optional, Set
import threading
import time

class TabManager:
    """Manages tabs in the IDE interface"""
    
    def __init__(self, parent_frame, menu_bar):
        self.parent_frame = parent_frame
        self.menu_bar = menu_bar
        self.tabs = {}
        self.active_tab = None
        self.tab_order = []
        
        # Create tab container
        self.setup_tab_container()
        
        print("📑 Tab Manager initialized")
    
    def setup_tab_container(self):
        """Setup the tab container under the menu bar"""
        
        # Main container frame (goes under menu bar)
        self.tab_container = ttk.Frame(self.parent_frame)
        self.tab_container.pack(fill='x', side='top', padx=0, pady=0)
        
        # Tab bar frame (for tab buttons)
        self.tab_bar = ttk.Frame(self.tab_container, height=30)
        self.tab_bar.pack(fill='x', side='top', padx=5, pady=2)
        self.tab_bar.pack_propagate(False)
        
        # Tab content area (for tab content)
        self.tab_content_area = ttk.Frame(self.tab_container)
        self.tab_content_area.pack(fill='both', expand=True, padx=5, pady=5)
        
        # Create close all button
        self.close_all_button = ttk.Button(self.tab_bar, text="✕", width=3,
                                          command=self.close_all_tabs)
        self.close_all_button.pack(side='right', padx=(5, 0))
        
        # Create new tab button
        self.new_tab_button = ttk.Button(self.tab_bar, text="+", width=3,
                                        command=self.new_tab)
        self.new_tab_button.pack(side='right', padx=(5, 0))
    
    def new_tab(self, title: str = "New Tab", content_type: str = "text", 
                file_path: str = None, content: str = ""):
        """Create a new tab"""
        
        # Generate unique tab ID
        tab_id = f"tab_{len(self.tabs) + 1}_{int(time.time())}"
        
        # Create tab button
        tab_button = ttk.Button(self.tab_bar, text=title[:20] + "..." if len(title) > 20 else title,
                               command=lambda: self.switch_to_tab(tab_id))
        tab_button.pack(side='left', padx=2, pady=2)
        
        # Create tab content
        tab_content = self.create_tab_content(content_type, file_path, content)
        
        # Store tab information
        self.tabs[tab_id] = {
            'title': title,
            'button': tab_button,
            'content': tab_content,
            'content_type': content_type,
            'file_path': file_path,
            'modified': False,
            'created': time.time()
        }
        
        self.tab_order.append(tab_id)
        
        # Switch to new tab
        self.switch_to_tab(tab_id)
        
        print(f"📑 Created new tab: {title}")
        return tab_id
    
    def create_tab_content(self, content_type: str, file_path: str, content: str):
        """Create content for a tab based on type"""
        
        content_frame = ttk.Frame(self.tab_content_area)
        
        if content_type == "text":
            # Text editor
            text_widget = tk.Text(content_frame, wrap=tk.WORD, font=("Consolas", 10))
            text_widget.pack(fill='both', expand=True)
            
            # Add scrollbar
            scrollbar = ttk.Scrollbar(content_frame, orient="vertical", command=text_widget.yview)
            text_widget.configure(yscrollcommand=scrollbar.set)
            scrollbar.pack(side='right', fill='y')
            
            # Insert content
            if content:
                text_widget.insert(1.0, content)
            elif file_path and os.path.exists(file_path):
                try:
                    with open(file_path, 'r', encoding='utf-8') as f:
                        file_content = f.read()
                    text_widget.insert(1.0, file_content)
                except Exception as e:
                    text_widget.insert(1.0, f"Error loading file: {e}")
            
            # Bind modification event
            text_widget.bind('<KeyPress>', lambda e: self.mark_tab_modified(file_path))
            
            return content_frame
            
        elif content_type == "code":
            # Code editor with syntax highlighting
            code_widget = tk.Text(content_frame, wrap=tk.WORD, font=("Consolas", 10),
                                 bg="#1e1e1e", fg="#d4d4d4", insertbackground="white")
            code_widget.pack(fill='both', expand=True)
            
            # Add scrollbar
            scrollbar = ttk.Scrollbar(content_frame, orient="vertical", command=code_widget.yview)
            code_widget.configure(yscrollcommand=scrollbar.set)
            scrollbar.pack(side='right', fill='y')
            
            # Insert content
            if content:
                code_widget.insert(1.0, content)
            elif file_path and os.path.exists(file_path):
                try:
                    with open(file_path, 'r', encoding='utf-8') as f:
                        file_content = f.read()
                    code_widget.insert(1.0, file_content)
                except Exception as e:
                    code_widget.insert(1.0, f"Error loading file: {e}")
            
            return content_frame
            
        elif content_type == "terminal":
            # Terminal/console
            terminal_widget = tk.Text(content_frame, wrap=tk.WORD, font=("Consolas", 9),
                                     bg="#0c0c0c", fg="#cccccc")
            terminal_widget.pack(fill='both', expand=True)
            
            # Add scrollbar
            scrollbar = ttk.Scrollbar(content_frame, orient="vertical", command=terminal_widget.yview)
            terminal_widget.configure(yscrollcommand=scrollbar.set)
            scrollbar.pack(side='right', fill='y')
            
            # Insert welcome message
            terminal_widget.insert(1.0, "Terminal - Ready for commands\n")
            terminal_widget.insert(tk.END, f"Current directory: {os.getcwd()}\n")
            terminal_widget.insert(tk.END, "Type commands below:\n\n")
            
            return content_frame
            
        elif content_type == "file_explorer":
            # File explorer
            explorer_frame = ttk.Frame(content_frame)
            explorer_frame.pack(fill='both', expand=True)
            
            # Tree view for file system
            tree = ttk.Treeview(explorer_frame)
            tree.pack(side='left', fill='both', expand=True)
            
            # Add scrollbar
            tree_scrollbar = ttk.Scrollbar(explorer_frame, orient="vertical", command=tree.yview)
            tree.configure(yscrollcommand=tree_scrollbar.set)
            tree_scrollbar.pack(side='right', fill='y')
            
            # Populate with current directory
            self.populate_file_tree(tree, os.getcwd())
            
            return content_frame
            
        else:
            # Default content
            label = ttk.Label(content_frame, text=f"Content type: {content_type}")
            label.pack(expand=True)
            return content_frame
    
    def populate_file_tree(self, tree, path, parent=""):
        """Populate file tree with directory contents"""
        
        try:
            items = os.listdir(path)
            for item in sorted(items):
                item_path = os.path.join(path, item)
                
                if os.path.isdir(item_path):
                    # Directory
                    node = tree.insert(parent, "end", text=f"📁 {item}", values=[item_path])
                    # Add placeholder for lazy loading
                    tree.insert(node, "end", text="Loading...")
                else:
                    # File
                    ext = os.path.splitext(item)[1]
                    icon = self.get_file_icon(ext)
                    tree.insert(parent, "end", text=f"{icon} {item}", values=[item_path])
        except PermissionError:
            tree.insert(parent, "end", text="❌ Permission denied")
    
    def get_file_icon(self, extension: str) -> str:
        """Get icon for file extension"""
        
        icons = {
            '.py': '🐍', '.js': '🟨', '.html': '🌐', '.css': '🎨',
            '.json': '📋', '.xml': '📄', '.txt': '📝', '.md': '📖',
            '.exe': '⚙️', '.dll': '🔧', '.zip': '📦', '.pdf': '📕',
            '.jpg': '🖼️', '.png': '🖼️', '.gif': '🖼️', '.svg': '🖼️'
        }
        
        return icons.get(extension.lower(), '📄')
    
    def switch_to_tab(self, tab_id: str):
        """Switch to a specific tab"""
        
        if tab_id not in self.tabs:
            return
        
        # Hide all tab contents
        for tid, tab_info in self.tabs.items():
            tab_info['content'].pack_forget()
        
        # Show selected tab content
        self.tabs[tab_id]['content'].pack(fill='both', expand=True)
        
        # Update button states
        for tid, tab_info in self.tabs.items():
            if tid == tab_id:
                tab_info['button'].configure(style="Accent.TButton")
            else:
                tab_info['button'].configure(style="TButton")
        
        self.active_tab = tab_id
        
        print(f"📑 Switched to tab: {self.tabs[tab_id]['title']}")
    
    def close_tab(self, tab_id: str):
        """Close a specific tab"""
        
        if tab_id not in self.tabs:
            return
        
        tab_info = self.tabs[tab_id]
        
        # Check if tab is modified
        if tab_info['modified']:
            result = messagebox.askyesnocancel(
                "Save Changes?",
                f"Tab '{tab_info['title']}' has unsaved changes. Save before closing?"
            )
            
            if result is True:  # Yes - Save
                self.save_tab(tab_id)
            elif result is None:  # Cancel
                return
        
        # Remove tab button
        tab_info['button'].destroy()
        
        # Remove tab content
        tab_info['content'].destroy()
        
        # Remove from tabs dict
        del self.tabs[tab_id]
        
        # Remove from order
        if tab_id in self.tab_order:
            self.tab_order.remove(tab_id)
        
        # Switch to another tab if this was active
        if self.active_tab == tab_id:
            if self.tab_order:
                self.switch_to_tab(self.tab_order[-1])
            else:
                self.active_tab = None
        
        print(f"📑 Closed tab: {tab_info['title']}")
    
    def close_all_tabs(self):
        """Close all tabs"""
        
        if not self.tabs:
            return
        
        # Get list of tab IDs to avoid modification during iteration
        tab_ids = list(self.tabs.keys())
        
        for tab_id in tab_ids:
            self.close_tab(tab_id)
        
        print("📑 Closed all tabs")
    
    def mark_tab_modified(self, tab_id: str):
        """Mark a tab as modified"""
        
        if tab_id in self.tabs:
            self.tabs[tab_id]['modified'] = True
            # Update button text to show modification
            title = self.tabs[tab_id]['title']
            if not title.endswith(' *'):
                self.tabs[tab_id]['button'].configure(text=title + ' *')
    
    def save_tab(self, tab_id: str):
        """Save tab content"""
        
        if tab_id not in self.tabs:
            return
        
        tab_info = self.tabs[tab_id]
        
        # Get content from text widget
        content = ""
        for widget in tab_info['content'].winfo_children():
            if isinstance(widget, tk.Text):
                content = widget.get(1.0, tk.END)
                break
        
        # Save to file if file path exists
        if tab_info['file_path']:
            try:
                with open(tab_info['file_path'], 'w', encoding='utf-8') as f:
                    f.write(content)
                tab_info['modified'] = False
                # Remove * from button text
                title = tab_info['title'].replace(' *', '')
                tab_info['button'].configure(text=title)
                print(f"💾 Saved tab: {title}")
            except Exception as e:
                messagebox.showerror("Save Error", f"Could not save file: {e}")
        else:
            # Save as dialog
            filename = filedialog.asksaveasfilename(
                title="Save As",
                defaultextension=".txt",
                filetypes=[("Text files", "*.txt"), ("All files", "*.*")]
            )
            if filename:
                try:
                    with open(filename, 'w', encoding='utf-8') as f:
                        f.write(content)
                    tab_info['file_path'] = filename
                    tab_info['modified'] = False
                    print(f"💾 Saved tab as: {filename}")
                except Exception as e:
                    messagebox.showerror("Save Error", f"Could not save file: {e}")

class VisualStudioTabbedInterface:
    """
    Visual Studio-style tabbed interface with tabs under menu bar
    """
    
    def __init__(self, root):
        self.root = root
        self.setup_main_interface()
        self.setup_menu_bar()
        self.setup_tab_manager()
        self.setup_status_bar()
        
        print("🎯 Visual Studio tabbed interface initialized")
    
    def setup_main_interface(self):
        """Setup main interface structure"""
        
        # Main container
        self.main_container = ttk.Frame(self.root)
        self.main_container.pack(fill='both', expand=True)
        
        # Configure root window
        self.root.title("Visual Studio-Style IDE")
        self.root.geometry("1200x800")
        
        # Configure grid weights
        self.root.columnconfigure(0, weight=1)
        self.root.rowconfigure(0, weight=1)
        self.main_container.columnconfigure(0, weight=1)
        self.main_container.rowconfigure(1, weight=1)
    
    def setup_menu_bar(self):
        """Setup menu bar at the top"""
        
        self.menu_bar = tk.Menu(self.root)
        self.root.config(menu=self.menu_bar)
        
        # File menu
        self.file_menu = tk.Menu(self.menu_bar, tearoff=0)
        self.menu_bar.add_cascade(label="File", menu=self.file_menu)
        
        self.file_menu.add_command(label="New Tab", command=self.new_tab, accelerator="Ctrl+T")
        self.file_menu.add_command(label="Open File...", command=self.open_file, accelerator="Ctrl+O")
        self.file_menu.add_command(label="Save", command=self.save_current_tab, accelerator="Ctrl+S")
        self.file_menu.add_command(label="Save As...", command=self.save_as_current_tab, accelerator="Ctrl+Shift+S")
        self.file_menu.add_separator()
        self.file_menu.add_command(label="Close Tab", command=self.close_current_tab, accelerator="Ctrl+W")
        self.file_menu.add_command(label="Close All Tabs", command=self.close_all_tabs, accelerator="Ctrl+Shift+W")
        self.file_menu.add_separator()
        self.file_menu.add_command(label="Exit", command=self.root.quit, accelerator="Alt+F4")
        
        # Edit menu
        self.edit_menu = tk.Menu(self.menu_bar, tearoff=0)
        self.menu_bar.add_cascade(label="Edit", menu=self.edit_menu)
        
        self.edit_menu.add_command(label="Undo", command=self.undo, accelerator="Ctrl+Z")
        self.edit_menu.add_command(label="Redo", command=self.redo, accelerator="Ctrl+Y")
        self.edit_menu.add_separator()
        self.edit_menu.add_command(label="Cut", command=self.cut, accelerator="Ctrl+X")
        self.edit_menu.add_command(label="Copy", command=self.copy, accelerator="Ctrl+C")
        self.edit_menu.add_command(label="Paste", command=self.paste, accelerator="Ctrl+V")
        self.edit_menu.add_separator()
        self.edit_menu.add_command(label="Find...", command=self.find, accelerator="Ctrl+F")
        self.edit_menu.add_command(label="Replace...", command=self.replace, accelerator="Ctrl+H")
        
        # View menu
        self.view_menu = tk.Menu(self.menu_bar, tearoff=0)
        self.menu_bar.add_cascade(label="View", menu=self.view_menu)
        
        self.view_menu.add_command(label="New Terminal Tab", command=self.new_terminal_tab)
        self.view_menu.add_command(label="New File Explorer Tab", command=self.new_file_explorer_tab)
        self.view_menu.add_separator()
        self.view_menu.add_command(label="Toggle Tab Bar", command=self.toggle_tab_bar)
        
        # Tools menu
        self.tools_menu = tk.Menu(self.menu_bar, tearoff=0)
        self.menu_bar.add_cascade(label="Tools", menu=self.tools_menu)
        
        self.tools_menu.add_command(label="Language Selector", command=self.show_language_selector)
        self.tools_menu.add_command(label="Distribution Warnings", command=self.show_distribution_warnings)
        
        # Help menu
        self.help_menu = tk.Menu(self.menu_bar, tearoff=0)
        self.menu_bar.add_cascade(label="Help", menu=self.help_menu)
        
        self.help_menu.add_command(label="About", command=self.show_about)
        
        # Bind keyboard shortcuts
        self.root.bind('<Control-t>', lambda e: self.new_tab())
        self.root.bind('<Control-o>', lambda e: self.open_file())
        self.root.bind('<Control-s>', lambda e: self.save_current_tab())
        self.root.bind('<Control-Shift-S>', lambda e: self.save_as_current_tab())
        self.root.bind('<Control-w>', lambda e: self.close_current_tab())
        self.root.bind('<Control-Shift-W>', lambda e: self.close_all_tabs())
    
    def setup_tab_manager(self):
        """Setup tab manager under menu bar"""
        
        self.tab_manager = TabManager(self.main_container, self.menu_bar)
        
        # Create initial tab
        self.tab_manager.new_tab("Welcome", "text", None, 
                                "Welcome to Visual Studio-Style IDE!\n\n"
                                "Tabs are positioned under the menu bar (File, Edit, View, etc.)\n"
                                "Use Ctrl+T to create new tabs\n"
                                "Use Ctrl+O to open files\n"
                                "Use Ctrl+W to close current tab\n\n"
                                "Enjoy coding!")
    
    def setup_status_bar(self):
        """Setup status bar at bottom"""
        
        self.status_bar = ttk.Frame(self.main_container)
        self.status_bar.pack(fill='x', side='bottom', pady=2)
        
        # Status label
        self.status_label = ttk.Label(self.status_bar, text="Ready", relief=tk.SUNKEN, anchor='w')
        self.status_label.pack(side='left', fill='x', expand=True, padx=5)
        
        # Tab count label
        self.tab_count_label = ttk.Label(self.status_bar, text="Tabs: 1", relief=tk.SUNKEN)
        self.tab_count_label.pack(side='right', padx=5)
    
    def update_status(self, message: str):
        """Update status bar message"""
        
        self.status_label.config(text=message)
        self.tab_count_label.config(text=f"Tabs: {len(self.tab_manager.tabs)}")
    
    def new_tab(self):
        """Create new tab"""
        
        tab_id = self.tab_manager.new_tab("New Tab", "text")
        self.update_status("New tab created")
    
    def open_file(self):
        """Open file in new tab"""
        
        filename = filedialog.askopenfilename(
            title="Open File",
            filetypes=[
                ("All files", "*.*"),
                ("Text files", "*.txt"),
                ("Python files", "*.py"),
                ("JavaScript files", "*.js"),
                ("HTML files", "*.html"),
                ("CSS files", "*.css")
            ]
        )
        
        if filename:
            file_name = os.path.basename(filename)
            # Determine content type based on extension
            ext = os.path.splitext(filename)[1].lower()
            content_type = "code" if ext in ['.py', '.js', '.html', '.css', '.cpp', '.c', '.java'] else "text"
            
            self.tab_manager.new_tab(file_name, content_type, filename)
            self.update_status(f"Opened: {file_name}")
    
    def save_current_tab(self):
        """Save current tab"""
        
        if self.tab_manager.active_tab:
            self.tab_manager.save_tab(self.tab_manager.active_tab)
            self.update_status("Tab saved")
    
    def save_as_current_tab(self):
        """Save current tab as new file"""
        
        if self.tab_manager.active_tab:
            tab_info = self.tab_manager.tabs[self.tab_manager.active_tab]
            
            # Get content
            content = ""
            for widget in tab_info['content'].winfo_children():
                if isinstance(widget, tk.Text):
                    content = widget.get(1.0, tk.END)
                    break
            
            # Save as dialog
            filename = filedialog.asksaveasfilename(
                title="Save As",
                defaultextension=".txt",
                filetypes=[("Text files", "*.txt"), ("All files", "*.*")]
            )
            
            if filename:
                try:
                    with open(filename, 'w', encoding='utf-8') as f:
                        f.write(content)
                    
                    # Update tab info
                    tab_info['file_path'] = filename
                    tab_info['modified'] = False
                    tab_info['title'] = os.path.basename(filename)
                    tab_info['button'].configure(text=tab_info['title'])
                    
                    self.update_status(f"Saved as: {filename}")
                except Exception as e:
                    messagebox.showerror("Save Error", f"Could not save file: {e}")
    
    def close_current_tab(self):
        """Close current tab"""
        
        if self.tab_manager.active_tab:
            self.tab_manager.close_tab(self.tab_manager.active_tab)
            self.update_status("Tab closed")
    
    def close_all_tabs(self):
        """Close all tabs"""
        
        self.tab_manager.close_all_tabs()
        self.update_status("All tabs closed")
    
    def new_terminal_tab(self):
        """Create new terminal tab"""
        
        self.tab_manager.new_tab("Terminal", "terminal")
        self.update_status("Terminal tab created")
    
    def new_file_explorer_tab(self):
        """Create new file explorer tab"""
        
        self.tab_manager.new_tab("File Explorer", "file_explorer")
        self.update_status("File explorer tab created")
    
    def toggle_tab_bar(self):
        """Toggle tab bar visibility"""
        
        if self.tab_manager.tab_bar.winfo_viewable():
            self.tab_manager.tab_bar.pack_forget()
        else:
            self.tab_manager.tab_bar.pack(fill='x', side='top', padx=5, pady=2)
        
        self.update_status("Tab bar toggled")
    
    def show_language_selector(self):
        """Show language selector dialog"""
        
        try:
            from visual_studio_language_selector import VisualStudioLanguageSelector
            selector = VisualStudioLanguageSelector(self)
            selector.show_language_selection_dialog()
        except ImportError:
            messagebox.showinfo("Language Selector", "Language selector not available")
    
    def show_distribution_warnings(self):
        """Show distribution warnings"""
        
        try:
            from distribution_warning_system import DistributionWarningSystem
            warning_system = DistributionWarningSystem(self)
            warning_system.show_startup_warning()
        except ImportError:
            messagebox.showinfo("Distribution Warnings", "Distribution warning system not available")
    
    def show_about(self):
        """Show about dialog"""
        
        messagebox.showinfo("About", 
                          "Visual Studio-Style IDE\n\n"
                          "A modern IDE with tabs positioned under the menu bar\n"
                          "Supports multiple languages and file types\n"
                          "Version 1.0")
    
    # Edit menu functions (stubs)
    def undo(self):
        """Undo action"""
        self.update_status("Undo")
    
    def redo(self):
        """Redo action"""
        self.update_status("Redo")
    
    def cut(self):
        """Cut action"""
        self.update_status("Cut")
    
    def copy(self):
        """Copy action"""
        self.update_status("Copy")
    
    def paste(self):
        """Paste action"""
        self.update_status("Paste")
    
    def find(self):
        """Find action"""
        self.update_status("Find")
    
    def replace(self):
        """Replace action"""
        self.update_status("Replace")

# Integration function
def integrate_visual_studio_interface(ide_instance):
    """Integrate Visual Studio-style interface with existing IDE"""
    
    # Create the interface
    vs_interface = VisualStudioTabbedInterface(ide_instance.root)
    
    # Add to IDE instance
    ide_instance.vs_interface = vs_interface
    
    print("🎯 Visual Studio-style tabbed interface integrated")

if __name__ == "__main__":
    print("🎯 Visual Studio-Style Tabbed Interface")
    print("=" * 50)
    print("Tabs positioned under menu bar (File, Edit, View, etc.)")
    
    # Create main window
    root = tk.Tk()
    
    # Create interface
    interface = VisualStudioTabbedInterface(root)
    
    print("✅ Visual Studio-style interface ready!")
    print("📑 Tabs are positioned under the menu bar")
    print("🎯 Use File menu or Ctrl+T to create new tabs")
    
    # Start the application
    root.mainloop()
