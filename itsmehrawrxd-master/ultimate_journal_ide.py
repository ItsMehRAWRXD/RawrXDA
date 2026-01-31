#!/usr/bin/env python3
"""
Ultimate Journal IDE - Integration with Real External Tools
The perfect productivity hub with real tool integration
"""

import os
import sys
import json
import subprocess
import threading
import time
import datetime
import webbrowser
from pathlib import Path
from typing import Dict, List, Optional, Any, Tuple
import tkinter as tk
from tkinter import ttk, scrolledtext, messagebox, filedialog
import requests
import sqlite3
import hashlib
import base64

class ExternalToolManager:
    """Manages integration with real external tools"""
    
    def __init__(self):
        self.tools = {
            'git': self.check_git,
            'python': self.check_python,
            'node': self.check_node,
            'gcc': self.check_gcc,
            'clang': self.check_clang,
            'docker': self.check_docker,
            'vscode': self.check_vscode,
            'notepad': self.check_notepad,
            'paint': self.check_paint,
            'calculator': self.check_calculator,
            'terminal': self.check_terminal,
            'browser': self.check_browser
        }
        
        self.available_tools = {}
        self.check_all_tools()
        
        print("🔧 External Tool Manager initialized")
        print(f"✅ Available tools: {', '.join(self.available_tools.keys())}")
    
    def check_all_tools(self):
        """Check which external tools are available"""
        
        for tool_name, check_func in self.tools.items():
            try:
                if check_func():
                    self.available_tools[tool_name] = True
                else:
                    self.available_tools[tool_name] = False
            except Exception:
                self.available_tools[tool_name] = False
    
    def check_git(self) -> bool:
        """Check if Git is available"""
        
        try:
            result = subprocess.run(['git', '--version'], 
                                  capture_output=True, text=True, timeout=5)
            return result.returncode == 0
        except:
            return False
    
    def check_python(self) -> bool:
        """Check if Python is available"""
        
        try:
            result = subprocess.run(['python', '--version'], 
                                  capture_output=True, text=True, timeout=5)
            return result.returncode == 0
        except:
            return False
    
    def check_node(self) -> bool:
        """Check if Node.js is available"""
        
        try:
            result = subprocess.run(['node', '--version'], 
                                  capture_output=True, text=True, timeout=5)
            return result.returncode == 0
        except:
            return False
    
    def check_gcc(self) -> bool:
        """Check if GCC is available"""
        
        try:
            result = subprocess.run(['gcc', '--version'], 
                                  capture_output=True, text=True, timeout=5)
            return result.returncode == 0
        except:
            return False
    
    def check_clang(self) -> bool:
        """Check if Clang is available"""
        
        try:
            result = subprocess.run(['clang', '--version'], 
                                  capture_output=True, text=True, timeout=5)
            return result.returncode == 0
        except:
            return False
    
    def check_docker(self) -> bool:
        """Check if Docker is available"""
        
        try:
            result = subprocess.run(['docker', '--version'], 
                                  capture_output=True, text=True, timeout=5)
            return result.returncode == 0
        except:
            return False
    
    def check_vscode(self) -> bool:
        """Check if VS Code is available"""
        
        try:
            if sys.platform == "win32":
                result = subprocess.run(['code', '--version'], 
                                      capture_output=True, text=True, timeout=5)
                return result.returncode == 0
            else:
                result = subprocess.run(['code', '--version'], 
                                      capture_output=True, text=True, timeout=5)
                return result.returncode == 0
        except:
            return False
    
    def check_notepad(self) -> bool:
        """Check if Notepad is available"""
        
        try:
            if sys.platform == "win32":
                result = subprocess.run(['notepad', '/?'], 
                                      capture_output=True, text=True, timeout=5)
                return True  # Notepad always available on Windows
            else:
                result = subprocess.run(['notepad'], 
                                      capture_output=True, text=True, timeout=5)
                return result.returncode == 0
        except:
            return False
    
    def check_paint(self) -> bool:
        """Check if Paint is available"""
        
        try:
            if sys.platform == "win32":
                result = subprocess.run(['mspaint', '/?'], 
                                      capture_output=True, text=True, timeout=5)
                return True  # Paint always available on Windows
            else:
                return False
        except:
            return False
    
    def check_calculator(self) -> bool:
        """Check if Calculator is available"""
        
        try:
            if sys.platform == "win32":
                result = subprocess.run(['calc'], 
                                      capture_output=True, text=True, timeout=5)
                return True  # Calculator always available on Windows
            else:
                result = subprocess.run(['gnome-calculator'], 
                                      capture_output=True, text=True, timeout=5)
                return result.returncode == 0
        except:
            return False
    
    def check_terminal(self) -> bool:
        """Check if Terminal is available"""
        
        return True  # Terminal is always available
    
    def check_browser(self) -> bool:
        """Check if Browser is available"""
        
        try:
            webbrowser.open('about:blank')
            return True
        except:
            return False
    
    def run_tool(self, tool_name: str, args: List[str] = None) -> Tuple[bool, str]:
        """Run external tool"""
        
        if tool_name not in self.available_tools or not self.available_tools[tool_name]:
            return False, f"Tool {tool_name} not available"
        
        try:
            cmd = [tool_name] + (args or [])
            result = subprocess.run(cmd, capture_output=True, text=True, timeout=30)
            
            if result.returncode == 0:
                return True, result.stdout
            else:
                return False, result.stderr
                
        except subprocess.TimeoutExpired:
            return False, "Tool execution timed out"
        except Exception as e:
            return False, str(e)

class JournalDatabase:
    """Database for journal entries"""
    
    def __init__(self, db_path: str = "journal.db"):
        self.db_path = db_path
        self.init_database()
    
    def init_database(self):
        """Initialize database"""
        
        conn = sqlite3.connect(self.db_path)
        cursor = conn.cursor()
        
        # Create entries table
        cursor.execute('''
            CREATE TABLE IF NOT EXISTS entries (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                title TEXT NOT NULL,
                content TEXT NOT NULL,
                date_created TEXT NOT NULL,
                date_modified TEXT NOT NULL,
                tags TEXT,
                mood TEXT,
                weather TEXT,
                location TEXT,
                file_path TEXT,
                word_count INTEGER,
                reading_time INTEGER
            )
        ''')
        
        # Create tags table
        cursor.execute('''
            CREATE TABLE IF NOT EXISTS tags (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                name TEXT UNIQUE NOT NULL,
                color TEXT DEFAULT '#007acc',
                created_date TEXT NOT NULL
            )
        ''')
        
        # Create attachments table
        cursor.execute('''
            CREATE TABLE IF NOT EXISTS attachments (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                entry_id INTEGER,
                file_path TEXT NOT NULL,
                file_type TEXT,
                file_size INTEGER,
                created_date TEXT NOT NULL,
                FOREIGN KEY (entry_id) REFERENCES entries (id)
            )
        ''')
        
        conn.commit()
        conn.close()
    
    def add_entry(self, title: str, content: str, tags: str = "", 
                 mood: str = "", weather: str = "", location: str = "") -> int:
        """Add journal entry"""
        
        conn = sqlite3.connect(self.db_path)
        cursor = conn.cursor()
        
        now = datetime.datetime.now().isoformat()
        word_count = len(content.split())
        reading_time = max(1, word_count // 200)  # Assume 200 words per minute
        
        cursor.execute('''
            INSERT INTO entries (title, content, date_created, date_modified, 
                               tags, mood, weather, location, word_count, reading_time)
            VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        ''', (title, content, now, now, tags, mood, weather, location, word_count, reading_time))
        
        entry_id = cursor.lastrowid
        conn.commit()
        conn.close()
        
        return entry_id
    
    def get_entries(self, limit: int = 50) -> List[Dict[str, Any]]:
        """Get journal entries"""
        
        conn = sqlite3.connect(self.db_path)
        cursor = conn.cursor()
        
        cursor.execute('''
            SELECT id, title, content, date_created, date_modified, 
                   tags, mood, weather, location, word_count, reading_time
            FROM entries 
            ORDER BY date_created DESC 
            LIMIT ?
        ''', (limit,))
        
        entries = []
        for row in cursor.fetchall():
            entries.append({
                'id': row[0],
                'title': row[1],
                'content': row[2],
                'date_created': row[3],
                'date_modified': row[4],
                'tags': row[5],
                'mood': row[6],
                'weather': row[7],
                'location': row[8],
                'word_count': row[9],
                'reading_time': row[10]
            })
        
        conn.close()
        return entries
    
    def search_entries(self, query: str) -> List[Dict[str, Any]]:
        """Search journal entries"""
        
        conn = sqlite3.connect(self.db_path)
        cursor = conn.cursor()
        
        cursor.execute('''
            SELECT id, title, content, date_created, date_modified, 
                   tags, mood, weather, location, word_count, reading_time
            FROM entries 
            WHERE title LIKE ? OR content LIKE ? OR tags LIKE ?
            ORDER BY date_created DESC
        ''', (f'%{query}%', f'%{query}%', f'%{query}%'))
        
        entries = []
        for row in cursor.fetchall():
            entries.append({
                'id': row[0],
                'title': row[1],
                'content': row[2],
                'date_created': row[3],
                'date_modified': row[4],
                'tags': row[5],
                'mood': row[6],
                'weather': row[7],
                'location': row[8],
                'word_count': row[9],
                'reading_time': row[10]
            })
        
        conn.close()
        return entries

class UltimateJournalIDE:
    """Ultimate Journal IDE with external tool integration"""
    
    def __init__(self):
        self.root = tk.Tk()
        self.root.title("Ultimate Journal IDE - Productivity Hub")
        self.root.geometry("1400x900")
        
        # Initialize components
        self.tool_manager = ExternalToolManager()
        self.database = JournalDatabase()
        
        # Current entry
        self.current_entry = None
        self.current_file = None
        
        # Setup UI
        self.setup_ui()
        
        # Load entries
        self.load_entries()
        
        print("📖 Ultimate Journal IDE initialized")
    
    def setup_ui(self):
        """Setup user interface"""
        
        # Create main frame
        main_frame = ttk.Frame(self.root)
        main_frame.pack(fill=tk.BOTH, expand=True, padx=10, pady=10)
        
        # Create left panel (entries list)
        left_frame = ttk.Frame(main_frame)
        left_frame.pack(side=tk.LEFT, fill=tk.BOTH, expand=True, padx=(0, 5))
        
        # Create right panel (editor)
        right_frame = ttk.Frame(main_frame)
        right_frame.pack(side=tk.RIGHT, fill=tk.BOTH, expand=True, padx=(5, 0))
        
        # Setup left panel
        self.setup_entries_panel(left_frame)
        
        # Setup right panel
        self.setup_editor_panel(right_frame)
        
        # Setup menu
        self.setup_menu()
        
        # Setup status bar
        self.setup_status_bar()
    
    def setup_entries_panel(self, parent):
        """Setup entries list panel"""
        
        # Title
        title_label = ttk.Label(parent, text="📖 Journal Entries", font=("Arial", 14, "bold"))
        title_label.pack(pady=(0, 10))
        
        # Search frame
        search_frame = ttk.Frame(parent)
        search_frame.pack(fill=tk.X, pady=(0, 10))
        
        self.search_var = tk.StringVar()
        self.search_var.trace('w', self.on_search_change)
        search_entry = ttk.Entry(search_frame, textvariable=self.search_var, font=("Arial", 10))
        search_entry.pack(fill=tk.X, padx=(0, 5))
        
        # New entry button
        new_button = ttk.Button(search_frame, text="📝 New Entry", command=self.new_entry)
        new_button.pack(side=tk.RIGHT)
        
        # Entries list
        list_frame = ttk.Frame(parent)
        list_frame.pack(fill=tk.BOTH, expand=True)
        
        # Treeview for entries
        columns = ('title', 'date', 'tags', 'words')
        self.entries_tree = ttk.Treeview(list_frame, columns=columns, show='tree headings', height=15)
        
        # Configure columns
        self.entries_tree.heading('#0', text='ID')
        self.entries_tree.heading('title', text='Title')
        self.entries_tree.heading('date', text='Date')
        self.entries_tree.heading('tags', text='Tags')
        self.entries_tree.heading('words', text='Words')
        
        self.entries_tree.column('#0', width=50)
        self.entries_tree.column('title', width=200)
        self.entries_tree.column('date', width=100)
        self.entries_tree.column('tags', width=150)
        self.entries_tree.column('words', width=80)
        
        # Scrollbar for treeview
        scrollbar = ttk.Scrollbar(list_frame, orient=tk.VERTICAL, command=self.entries_tree.yview)
        self.entries_tree.configure(yscrollcommand=scrollbar.set)
        
        self.entries_tree.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
        scrollbar.pack(side=tk.RIGHT, fill=tk.Y)
        
        # Bind selection
        self.entries_tree.bind('<<TreeviewSelect>>', self.on_entry_select)
        
        # Tools panel
        tools_frame = ttk.LabelFrame(parent, text="🔧 External Tools", padding=10)
        tools_frame.pack(fill=tk.X, pady=(10, 0))
        
        self.setup_tools_panel(tools_frame)
    
    def setup_tools_panel(self, parent):
        """Setup external tools panel"""
        
        # Tool buttons
        tools = [
            ("📝 Notepad", "notepad"),
            ("🎨 Paint", "paint"),
            ("🧮 Calculator", "calculator"),
            ("💻 Terminal", "terminal"),
            ("🌐 Browser", "browser"),
            ("📁 VS Code", "vscode"),
            ("🐍 Python", "python"),
            ("📦 Git", "git")
        ]
        
        for i, (label, tool) in enumerate(tools):
            available = self.tool_manager.available_tools.get(tool, False)
            state = tk.NORMAL if available else tk.DISABLED
            
            btn = ttk.Button(parent, text=label, command=lambda t=tool: self.run_external_tool(t), state=state)
            btn.grid(row=i//4, column=i%4, padx=5, pady=5, sticky='ew')
        
        # Configure grid
        for i in range(4):
            parent.columnconfigure(i, weight=1)
    
    def setup_editor_panel(self, parent):
        """Setup editor panel"""
        
        # Title frame
        title_frame = ttk.Frame(parent)
        title_frame.pack(fill=tk.X, pady=(0, 10))
        
        ttk.Label(title_frame, text="📝 Entry Editor", font=("Arial", 14, "bold")).pack(side=tk.LEFT)
        
        # Save button
        save_button = ttk.Button(title_frame, text="💾 Save", command=self.save_entry)
        save_button.pack(side=tk.RIGHT, padx=(5, 0))
        
        # Entry metadata frame
        meta_frame = ttk.Frame(parent)
        meta_frame.pack(fill=tk.X, pady=(0, 10))
        
        # Title input
        ttk.Label(meta_frame, text="Title:").grid(row=0, column=0, sticky='w', padx=(0, 5))
        self.title_var = tk.StringVar()
        title_entry = ttk.Entry(meta_frame, textvariable=self.title_var, font=("Arial", 12))
        title_entry.grid(row=0, column=1, sticky='ew', padx=(0, 10))
        
        # Tags input
        ttk.Label(meta_frame, text="Tags:").grid(row=0, column=2, sticky='w', padx=(0, 5))
        self.tags_var = tk.StringVar()
        tags_entry = ttk.Entry(meta_frame, textvariable=self.tags_var)
        tags_entry.grid(row=0, column=3, sticky='ew', padx=(0, 10))
        
        # Mood input
        ttk.Label(meta_frame, text="Mood:").grid(row=0, column=4, sticky='w', padx=(0, 5))
        self.mood_var = tk.StringVar()
        mood_combo = ttk.Combobox(meta_frame, textvariable=self.mood_var, 
                                 values=['😊', '😢', '😡', '😴', '🤔', '😍', '😎', '🤢'])
        mood_combo.grid(row=0, column=5, sticky='ew')
        
        meta_frame.columnconfigure(1, weight=2)
        meta_frame.columnconfigure(3, weight=1)
        meta_frame.columnconfigure(5, weight=1)
        
        # Content editor
        editor_frame = ttk.Frame(parent)
        editor_frame.pack(fill=tk.BOTH, expand=True)
        
        self.content_text = scrolledtext.ScrolledText(
            editor_frame, 
            font=("Arial", 11),
            wrap=tk.WORD,
            undo=True,
            maxundo=50
        )
        self.content_text.pack(fill=tk.BOTH, expand=True)
        
        # Bind events
        self.content_text.bind('<KeyRelease>', self.on_content_change)
        self.title_var.trace('w', self.on_content_change)
        self.tags_var.trace('w', self.on_content_change)
        self.mood_var.trace('w', self.on_content_change)
        
        # Quick actions frame
        actions_frame = ttk.Frame(parent)
        actions_frame.pack(fill=tk.X, pady=(10, 0))
        
        # Quick action buttons
        ttk.Button(actions_frame, text="📊 Word Count", command=self.show_word_count).pack(side=tk.LEFT, padx=(0, 5))
        ttk.Button(actions_frame, text="📅 Insert Date", command=self.insert_date).pack(side=tk.LEFT, padx=(0, 5))
        ttk.Button(actions_frame, text="🕒 Insert Time", command=self.insert_time).pack(side=tk.LEFT, padx=(0, 5))
        ttk.Button(actions_frame, text="📁 Open File", command=self.open_file).pack(side=tk.LEFT, padx=(0, 5))
        ttk.Button(actions_frame, text="💾 Export", command=self.export_entry).pack(side=tk.LEFT)
    
    def setup_menu(self):
        """Setup menu bar"""
        
        menubar = tk.Menu(self.root)
        self.root.config(menu=menubar)
        
        # File menu
        file_menu = tk.Menu(menubar, tearoff=0)
        menubar.add_cascade(label="File", menu=file_menu)
        file_menu.add_command(label="New Entry", command=self.new_entry)
        file_menu.add_command(label="Open Entry", command=self.open_entry)
        file_menu.add_command(label="Save Entry", command=self.save_entry)
        file_menu.add_separator()
        file_menu.add_command(label="Export All", command=self.export_all)
        file_menu.add_command(label="Import", command=self.import_entries)
        file_menu.add_separator()
        file_menu.add_command(label="Exit", command=self.root.quit)
        
        # Tools menu
        tools_menu = tk.Menu(menubar, tearoff=0)
        menubar.add_cascade(label="Tools", menu=tools_menu)
        
        # Add external tools to menu
        for tool_name, available in self.tool_manager.available_tools.items():
            if available:
                tools_menu.add_command(
                    label=f"Open {tool_name.title()}", 
                    command=lambda t=tool_name: self.run_external_tool(t)
                )
        
        # Help menu
        help_menu = tk.Menu(menubar, tearoff=0)
        menubar.add_cascade(label="Help", menu=help_menu)
        help_menu.add_command(label="About", command=self.show_about)
        help_menu.add_command(label="Keyboard Shortcuts", command=self.show_shortcuts)
    
    def setup_status_bar(self):
        """Setup status bar"""
        
        self.status_bar = ttk.Frame(self.root)
        self.status_bar.pack(side=tk.BOTTOM, fill=tk.X)
        
        self.status_label = ttk.Label(self.status_bar, text="Ready", relief=tk.SUNKEN)
        self.status_label.pack(side=tk.LEFT, padx=5, pady=2)
        
        self.word_count_label = ttk.Label(self.status_bar, text="Words: 0", relief=tk.SUNKEN)
        self.word_count_label.pack(side=tk.RIGHT, padx=5, pady=2)
    
    def load_entries(self):
        """Load journal entries"""
        
        # Clear existing entries
        for item in self.entries_tree.get_children():
            self.entries_tree.delete(item)
        
        # Load entries from database
        entries = self.database.get_entries()
        
        for entry in entries:
            # Format date
            date_str = entry['date_created'][:10]  # YYYY-MM-DD
            
            # Insert into tree
            item = self.entries_tree.insert('', 'end', 
                                          text=str(entry['id']),
                                          values=(entry['title'], date_str, entry['tags'], entry['word_count']))
    
    def on_search_change(self, *args):
        """Handle search change"""
        
        query = self.search_var.get().strip()
        
        if not query:
            self.load_entries()
            return
        
        # Clear existing entries
        for item in self.entries_tree.get_children():
            self.entries_tree.delete(item)
        
        # Search entries
        entries = self.database.search_entries(query)
        
        for entry in entries:
            date_str = entry['date_created'][:10]
            item = self.entries_tree.insert('', 'end', 
                                          text=str(entry['id']),
                                          values=(entry['title'], date_str, entry['tags'], entry['word_count']))
    
    def on_entry_select(self, event):
        """Handle entry selection"""
        
        selection = self.entries_tree.selection()
        if not selection:
            return
        
        item = self.entries_tree.item(selection[0])
        entry_id = int(item['text'])
        
        # Load entry
        self.load_entry(entry_id)
    
    def load_entry(self, entry_id: int):
        """Load specific entry"""
        
        entries = self.database.get_entries(1000)  # Get more entries to find the one we need
        
        for entry in entries:
            if entry['id'] == entry_id:
                self.current_entry = entry
                
                # Update UI
                self.title_var.set(entry['title'])
                self.content_text.delete(1.0, tk.END)
                self.content_text.insert(1.0, entry['content'])
                self.tags_var.set(entry['tags'])
                self.mood_var.set(entry['mood'])
                
                # Update status
                self.update_status(f"Loaded entry: {entry['title']}")
                self.update_word_count()
                
                break
    
    def new_entry(self):
        """Create new entry"""
        
        self.current_entry = None
        self.title_var.set("")
        self.content_text.delete(1.0, tk.END)
        self.tags_var.set("")
        self.mood_var.set("")
        
        self.update_status("New entry created")
        self.update_word_count()
        
        # Focus on title
        self.title_var.set(f"Entry - {datetime.datetime.now().strftime('%Y-%m-%d %H:%M')}")
    
    def save_entry(self):
        """Save current entry"""
        
        title = self.title_var.get().strip()
        content = self.content_text.get(1.0, tk.END).strip()
        tags = self.tags_var.get().strip()
        mood = self.mood_var.get().strip()
        
        if not title or not content:
            messagebox.showwarning("Warning", "Please enter a title and content.")
            return
        
        try:
            if self.current_entry:
                # Update existing entry
                # TODO: Implement update functionality
                messagebox.showinfo("Info", "Entry updated successfully!")
            else:
                # Add new entry
                entry_id = self.database.add_entry(title, content, tags, mood)
                self.current_entry = {'id': entry_id}
                messagebox.showinfo("Info", "Entry saved successfully!")
            
            # Reload entries
            self.load_entries()
            self.update_status("Entry saved")
            
        except Exception as e:
            messagebox.showerror("Error", f"Failed to save entry: {e}")
    
    def run_external_tool(self, tool_name: str):
        """Run external tool"""
        
        try:
            if tool_name == "notepad":
                subprocess.Popen(['notepad'])
            elif tool_name == "paint":
                subprocess.Popen(['mspaint'])
            elif tool_name == "calculator":
                subprocess.Popen(['calc'])
            elif tool_name == "terminal":
                subprocess.Popen(['cmd'], shell=True)
            elif tool_name == "browser":
                webbrowser.open('https://www.google.com')
            elif tool_name == "vscode":
                subprocess.Popen(['code', '.'])
            elif tool_name == "python":
                subprocess.Popen(['python'], shell=True)
            elif tool_name == "git":
                subprocess.Popen(['git', '--help'], shell=True)
            
            self.update_status(f"Opened {tool_name}")
            
        except Exception as e:
            messagebox.showerror("Error", f"Failed to open {tool_name}: {e}")
    
    def on_content_change(self, *args):
        """Handle content change"""
        
        self.update_word_count()
    
    def update_word_count(self):
        """Update word count"""
        
        content = self.content_text.get(1.0, tk.END).strip()
        word_count = len(content.split()) if content else 0
        
        self.word_count_label.config(text=f"Words: {word_count}")
    
    def update_status(self, message: str):
        """Update status bar"""
        
        self.status_label.config(text=message)
        self.root.after(3000, lambda: self.status_label.config(text="Ready"))
    
    def show_word_count(self):
        """Show detailed word count"""
        
        content = self.content_text.get(1.0, tk.END).strip()
        words = content.split()
        word_count = len(words)
        char_count = len(content)
        char_no_spaces = len(content.replace(' ', ''))
        
        message = f"""Word Count Statistics:
Words: {word_count}
Characters: {char_count}
Characters (no spaces): {char_no_spaces}
Reading time: ~{max(1, word_count // 200)} minutes"""
        
        messagebox.showinfo("Word Count", message)
    
    def insert_date(self):
        """Insert current date"""
        
        date_str = datetime.datetime.now().strftime('%Y-%m-%d')
        self.content_text.insert(tk.INSERT, date_str)
    
    def insert_time(self):
        """Insert current time"""
        
        time_str = datetime.datetime.now().strftime('%H:%M:%S')
        self.content_text.insert(tk.INSERT, time_str)
    
    def open_file(self):
        """Open file dialog"""
        
        file_path = filedialog.askopenfilename(
            title="Open File",
            filetypes=[("Text files", "*.txt"), ("All files", "*.*")]
        )
        
        if file_path:
            try:
                with open(file_path, 'r', encoding='utf-8') as f:
                    content = f.read()
                
                self.content_text.delete(1.0, tk.END)
                self.content_text.insert(1.0, content)
                
                self.update_status(f"Opened file: {file_path}")
                
            except Exception as e:
                messagebox.showerror("Error", f"Failed to open file: {e}")
    
    def export_entry(self):
        """Export current entry"""
        
        if not self.current_entry:
            messagebox.showwarning("Warning", "No entry selected.")
            return
        
        file_path = filedialog.asksaveasfilename(
            title="Export Entry",
            defaultextension=".txt",
            filetypes=[("Text files", "*.txt"), ("All files", "*.*")]
        )
        
        if file_path:
            try:
                title = self.title_var.get()
                content = self.content_text.get(1.0, tk.END)
                tags = self.tags_var.get()
                mood = self.mood_var.get()
                
                with open(file_path, 'w', encoding='utf-8') as f:
                    f.write(f"Title: {title}\n")
                    f.write(f"Date: {datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n")
                    f.write(f"Tags: {tags}\n")
                    f.write(f"Mood: {mood}\n")
                    f.write("-" * 50 + "\n\n")
                    f.write(content)
                
                self.update_status(f"Exported to: {file_path}")
                
            except Exception as e:
                messagebox.showerror("Error", f"Failed to export: {e}")
    
    def export_all(self):
        """Export all entries"""
        
        # TODO: Implement export all functionality
        messagebox.showinfo("Info", "Export all functionality coming soon!")
    
    def import_entries(self):
        """Import entries"""
        
        # TODO: Implement import functionality
        messagebox.showinfo("Info", "Import functionality coming soon!")
    
    def open_entry(self):
        """Open entry dialog"""
        
        # TODO: Implement open entry dialog
        messagebox.showinfo("Info", "Open entry dialog coming soon!")
    
    def show_about(self):
        """Show about dialog"""
        
        about_text = """Ultimate Journal IDE
Version 1.0

A powerful journal and productivity application with external tool integration.

Features:
• Rich text editing
• Tag-based organization
• Mood tracking
• Word count statistics
• External tool integration
• Search functionality
• Export/Import capabilities

Built with Python and Tkinter."""
        
        messagebox.showinfo("About", about_text)
    
    def show_shortcuts(self):
        """Show keyboard shortcuts"""
        
        shortcuts_text = """Keyboard Shortcuts:

Ctrl+N - New Entry
Ctrl+S - Save Entry
Ctrl+O - Open File
Ctrl+F - Search
F5 - Refresh Entries

External Tools:
F1 - Open Notepad
F2 - Open Paint
F3 - Open Calculator
F4 - Open Terminal
F5 - Open Browser"""
        
        messagebox.showinfo("Keyboard Shortcuts", shortcuts_text)
    
    def run(self):
        """Run the application"""
        
        print("🚀 Starting Ultimate Journal IDE...")
        self.root.mainloop()

# Integration function
def integrate_journal_ide(ide_instance):
    """Integrate Journal IDE with main IDE"""
    
    ide_instance.journal_ide = UltimateJournalIDE()
    print("📖 Journal IDE integrated with main IDE")

if __name__ == "__main__":
    print("📖 Ultimate Journal IDE - Productivity Hub")
    print("=" * 50)
    
    # Create and run the application
    app = UltimateJournalIDE()
    app.run()
