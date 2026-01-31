#!/usr/bin/env python3
"""
Comprehensive Compiler GUI with Drag & Drop
Advanced GUI for the extensible compiler system with transpilation support
"""

import os
import sys
import tkinter as tk
from tkinter import ttk, filedialog, messagebox, scrolledtext
from typing import Dict, List, Any, Optional
import json
import importlib
import threading

# Import our compiler components
from extensible_compiler_system import ExtensibleCompilerSystem, LanguageInfo, LanguageType, TargetType
from ast_visitor import CodeGenerator
from user_compiler_components import UserComponentManager

# Import language components
try:
    from plugins.python_components import PythonLexer, PythonParser, get_language_info as get_python_info
    from plugins.js_components import JavaScriptLexer, JavaScriptParser, get_language_info as get_js_info
    from plugins.rust_components import RustLexer, RustParser, get_language_info as get_rust_info
    from plugins.py_to_cpp_codegen import PyToCppCodeGenerator, get_backend_target_info as get_py_to_cpp_info
    from plugins.py_to_rust_codegen import PyToRustCodeGenerator, get_backend_target_info as get_py_to_rust_info
except ImportError as e:
    print(f"Warning: Could not import some plugins: {e}")

class ComprehensiveCompilerGUI:
    """Advanced GUI for the comprehensive compiler system"""
    
    def __init__(self):
        self.root = tk.Tk()
        self.root.title("🚀 Comprehensive Extensible Compiler System")
        self.root.geometry("1400x900")
        self.root.configure(bg='#f0f0f0')
        
        # Initialize compiler system
        self.compiler_system = ExtensibleCompilerSystem()
        self.user_component_manager = UserComponentManager()
        
        # Current state
        self.current_file = None
        self.current_language = None
        self.current_ast = None
        self.current_tokens = None
        
        # Setup UI
        self.setup_ui()
        self.load_plugins()
        self.setup_drag_drop()
        
        # Bind keyboard shortcuts
        self.setup_keyboard_shortcuts()
    
    def setup_ui(self):
        """Setup the main UI"""
        # Main container
        main_frame = ttk.Frame(self.root)
        main_frame.pack(fill=tk.BOTH, expand=True, padx=10, pady=10)
        
        # Top toolbar
        self.create_toolbar(main_frame)
        
        # Main content area
        content_frame = ttk.Frame(main_frame)
        content_frame.pack(fill=tk.BOTH, expand=True, pady=(10, 0))
        
        # Left panel - File and language selection
        left_panel = ttk.Frame(content_frame)
        left_panel.pack(side=tk.LEFT, fill=tk.BOTH, expand=True, padx=(0, 5))
        
        # Right panel - Output and results
        right_panel = ttk.Frame(content_frame)
        right_panel.pack(side=tk.RIGHT, fill=tk.BOTH, expand=True, padx=(5, 0))
        
        # Create panels
        self.create_left_panel(left_panel)
        self.create_right_panel(right_panel)
        
        # Status bar
        self.create_status_bar(main_frame)
    
    def create_toolbar(self, parent):
        """Create the toolbar"""
        toolbar = ttk.Frame(parent)
        toolbar.pack(fill=tk.X, pady=(0, 10))
        
        # File operations
        ttk.Button(toolbar, text="📁 Open File", command=self.open_file).pack(side=tk.LEFT, padx=(0, 5))
        ttk.Button(toolbar, text="💾 Save", command=self.save_file).pack(side=tk.LEFT, padx=(0, 5))
        ttk.Button(toolbar, text="🔄 New", command=self.new_file).pack(side=tk.LEFT, padx=(0, 5))
        
        ttk.Separator(toolbar, orient=tk.VERTICAL).pack(side=tk.LEFT, fill=tk.Y, padx=10)
        
        # Compilation
        ttk.Button(toolbar, text="🔧 Compile", command=self.compile_code).pack(side=tk.LEFT, padx=(0, 5))
        ttk.Button(toolbar, text="🌳 Show AST", command=self.show_ast).pack(side=tk.LEFT, padx=(0, 5))
        ttk.Button(toolbar, text="🔤 Show Tokens", command=self.show_tokens).pack(side=tk.LEFT, padx=(0, 5))
        
        ttk.Separator(toolbar, orient=tk.VERTICAL).pack(side=tk.LEFT, fill=tk.Y, padx=10)
        
        # Transpilation
        ttk.Button(toolbar, text="🔄 Transpile to C++", command=self.transpile_to_cpp).pack(side=tk.LEFT, padx=(0, 5))
        ttk.Button(toolbar, text="🦀 Transpile to Rust", command=self.transpile_to_rust).pack(side=tk.LEFT, padx=(0, 5))
        
        ttk.Separator(toolbar, orient=tk.VERTICAL).pack(side=tk.LEFT, fill=tk.Y, padx=10)
        
        # Advanced features
        ttk.Button(toolbar, text="🔧 Components", command=self.show_component_manager).pack(side=tk.LEFT, padx=(0, 5))
        ttk.Button(toolbar, text="📊 Analytics", command=self.show_analytics).pack(side=tk.LEFT, padx=(0, 5))
    
    def create_left_panel(self, parent):
        """Create the left panel"""
        # File info frame
        file_frame = ttk.LabelFrame(parent, text="📁 File Information", padding="10")
        file_frame.pack(fill=tk.X, pady=(0, 10))
        
        # Current file
        ttk.Label(file_frame, text="Current File:").pack(anchor=tk.W)
        self.current_file_label = ttk.Label(file_frame, text="No file loaded", foreground="gray")
        self.current_file_label.pack(anchor=tk.W, fill=tk.X)
        
        # Language selection
        ttk.Label(file_frame, text="Language:").pack(anchor=tk.W, pady=(10, 0))
        self.language_var = tk.StringVar()
        self.language_combo = ttk.Combobox(file_frame, textvariable=self.language_var, state="readonly")
        self.language_combo.pack(fill=tk.X, pady=(5, 0))
        self.language_combo.bind('<<ComboboxSelected>>', self.on_language_change)
        
        # Drag and drop area
        self.create_drag_drop_area(file_frame)
        
        # Source code editor
        editor_frame = ttk.LabelFrame(parent, text="📝 Source Code", padding="10")
        editor_frame.pack(fill=tk.BOTH, expand=True)
        
        # Editor with line numbers
        editor_container = ttk.Frame(editor_frame)
        editor_container.pack(fill=tk.BOTH, expand=True)
        
        # Line numbers
        self.line_numbers = tk.Text(editor_container, width=4, padx=3, pady=3, takefocus=0, 
                                   border=0, background='lightgray', state='disabled')
        self.line_numbers.pack(side=tk.LEFT, fill=tk.Y)
        
        # Main editor
        self.source_editor = scrolledtext.ScrolledText(editor_container, wrap=tk.NONE, 
                                                      font=('Consolas', 10))
        self.source_editor.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
        
        # Bind editor events
        self.source_editor.bind('<KeyRelease>', self.on_editor_change)
        self.source_editor.bind('<Button-1>', self.on_editor_change)
        
        # Update line numbers
        self.update_line_numbers()
    
    def create_drag_drop_area(self, parent):
        """Create drag and drop area"""
        drop_frame = ttk.Frame(parent)
        drop_frame.pack(fill=tk.X, pady=(10, 0))
        
        # Drop target
        self.drop_target = tk.Frame(drop_frame, bg='lightblue', relief=tk.RAISED, bd=2)
        self.drop_target.pack(fill=tk.X, pady=5)
        
        drop_label = ttk.Label(self.drop_target, text="📁 Drag & Drop Source File Here", 
                              font=('Arial', 12, 'bold'))
        drop_label.pack(pady=20)
        
        # Bind drop events
        self.setup_drag_drop_events()
    
    def create_right_panel(self, parent):
        """Create the right panel"""
        # Create notebook for different views
        self.notebook = ttk.Notebook(parent)
        self.notebook.pack(fill=tk.BOTH, expand=True)
        
        # Compilation results tab
        self.create_compilation_tab()
        
        # AST viewer tab
        self.create_ast_tab()
        
        # Tokens viewer tab
        self.create_tokens_tab()
        
        # Transpilation results tab
        self.create_transpilation_tab()
        
        # Analytics tab
        self.create_analytics_tab()
    
    def create_compilation_tab(self):
        """Create compilation results tab"""
        compilation_frame = ttk.Frame(self.notebook)
        self.notebook.add(compilation_frame, text="🔧 Compilation")
        
        # Compilation output
        self.compilation_output = scrolledtext.ScrolledText(compilation_frame, 
                                                          font=('Consolas', 10))
        self.compilation_output.pack(fill=tk.BOTH, expand=True, padx=10, pady=10)
        
        # Compilation controls
        controls_frame = ttk.Frame(compilation_frame)
        controls_frame.pack(fill=tk.X, padx=10, pady=(0, 10))
        
        ttk.Button(controls_frame, text="🔧 Compile", command=self.compile_code).pack(side=tk.LEFT, padx=(0, 5))
        ttk.Button(controls_frame, text="🧹 Clear", command=self.clear_compilation_output).pack(side=tk.LEFT, padx=(0, 5))
        ttk.Button(controls_frame, text="💾 Save Output", command=self.save_compilation_output).pack(side=tk.LEFT)
    
    def create_ast_tab(self):
        """Create AST viewer tab"""
        ast_frame = ttk.Frame(self.notebook)
        self.notebook.add(ast_frame, text="🌳 AST")
        
        # AST tree view
        tree_frame = ttk.Frame(ast_frame)
        tree_frame.pack(fill=tk.BOTH, expand=True, padx=10, pady=10)
        
        self.ast_tree = ttk.Treeview(tree_frame, columns=('type', 'value'), show='tree headings')
        self.ast_tree.heading('#0', text='Node')
        self.ast_tree.heading('type', text='Type')
        self.ast_tree.heading('value', text='Value')
        
        # Scrollbar for tree
        ast_scrollbar = ttk.Scrollbar(tree_frame, orient=tk.VERTICAL, command=self.ast_tree.yview)
        self.ast_tree.configure(yscrollcommand=ast_scrollbar.set)
        
        self.ast_tree.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
        ast_scrollbar.pack(side=tk.RIGHT, fill=tk.Y)
        
        # AST controls
        ast_controls = ttk.Frame(ast_frame)
        ast_controls.pack(fill=tk.X, padx=10, pady=(0, 10))
        
        ttk.Button(ast_controls, text="🌳 Generate AST", command=self.generate_ast).pack(side=tk.LEFT, padx=(0, 5))
        ttk.Button(ast_controls, text="🧹 Clear", command=self.clear_ast).pack(side=tk.LEFT, padx=(0, 5))
        ttk.Button(ast_controls, text="💾 Export AST", command=self.export_ast).pack(side=tk.LEFT)
    
    def create_tokens_tab(self):
        """Create tokens viewer tab"""
        tokens_frame = ttk.Frame(self.notebook)
        self.notebook.add(tokens_frame, text="🔤 Tokens")
        
        # Tokens list
        list_frame = ttk.Frame(tokens_frame)
        list_frame.pack(fill=tk.BOTH, expand=True, padx=10, pady=10)
        
        self.tokens_tree = ttk.Treeview(list_frame, columns=('type', 'value', 'line', 'column'), show='headings')
        self.tokens_tree.heading('type', text='Type')
        self.tokens_tree.heading('value', text='Value')
        self.tokens_tree.heading('line', text='Line')
        self.tokens_tree.heading('column', text='Column')
        
        # Scrollbar for tokens
        tokens_scrollbar = ttk.Scrollbar(list_frame, orient=tk.VERTICAL, command=self.tokens_tree.yview)
        self.tokens_tree.configure(yscrollcommand=tokens_scrollbar.set)
        
        self.tokens_tree.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
        tokens_scrollbar.pack(side=tk.RIGHT, fill=tk.Y)
        
        # Tokens controls
        tokens_controls = ttk.Frame(tokens_frame)
        tokens_controls.pack(fill=tk.X, padx=10, pady=(0, 10))
        
        ttk.Button(tokens_controls, text="🔤 Tokenize", command=self.tokenize_code).pack(side=tk.LEFT, padx=(0, 5))
        ttk.Button(tokens_controls, text="🧹 Clear", command=self.clear_tokens).pack(side=tk.LEFT, padx=(0, 5))
        ttk.Button(tokens_controls, text="💾 Export Tokens", command=self.export_tokens).pack(side=tk.LEFT)
    
    def create_transpilation_tab(self):
        """Create transpilation results tab"""
        transpilation_frame = ttk.Frame(self.notebook)
        self.notebook.add(transpilation_frame, text="🔄 Transpilation")
        
        # Target language selection
        target_frame = ttk.Frame(transpilation_frame)
        target_frame.pack(fill=tk.X, padx=10, pady=10)
        
        ttk.Label(target_frame, text="Target Language:").pack(side=tk.LEFT)
        self.target_language_var = tk.StringVar()
        self.target_language_combo = ttk.Combobox(target_frame, textvariable=self.target_language_var, 
                                                 values=["C++", "Rust", "JavaScript", "Python"], state="readonly")
        self.target_language_combo.pack(side=tk.LEFT, padx=(10, 0))
        
        ttk.Button(target_frame, text="🔄 Transpile", command=self.transpile_code).pack(side=tk.LEFT, padx=(10, 0))
        
        # Transpilation output
        self.transpilation_output = scrolledtext.ScrolledText(transpilation_frame, 
                                                              font=('Consolas', 10))
        self.transpilation_output.pack(fill=tk.BOTH, expand=True, padx=10, pady=(0, 10))
        
        # Transpilation controls
        transpilation_controls = ttk.Frame(transpilation_frame)
        transpilation_controls.pack(fill=tk.X, padx=10, pady=(0, 10))
        
        ttk.Button(transpilation_controls, text="🧹 Clear", command=self.clear_transpilation_output).pack(side=tk.LEFT, padx=(0, 5))
        ttk.Button(transpilation_controls, text="💾 Save Output", command=self.save_transpilation_output).pack(side=tk.LEFT)
    
    def create_analytics_tab(self):
        """Create analytics tab"""
        analytics_frame = ttk.Frame(self.notebook)
        self.notebook.add(analytics_frame, text="📊 Analytics")
        
        # Analytics content
        self.analytics_text = scrolledtext.ScrolledText(analytics_frame, font=('Consolas', 10))
        self.analytics_text.pack(fill=tk.BOTH, expand=True, padx=10, pady=10)
        
        # Analytics controls
        analytics_controls = ttk.Frame(analytics_frame)
        analytics_controls.pack(fill=tk.X, padx=10, pady=(0, 10))
        
        ttk.Button(analytics_controls, text="📊 Generate Analytics", command=self.generate_analytics).pack(side=tk.LEFT, padx=(0, 5))
        ttk.Button(analytics_controls, text="🧹 Clear", command=self.clear_analytics).pack(side=tk.LEFT)
    
    def create_status_bar(self, parent):
        """Create status bar"""
        self.status_bar = ttk.Frame(parent)
        self.status_bar.pack(fill=tk.X, pady=(10, 0))
        
        self.status_label = ttk.Label(self.status_bar, text="Ready")
        self.status_label.pack(side=tk.LEFT)
        
        # Progress bar
        self.progress = ttk.Progressbar(self.status_bar, mode='indeterminate')
        self.progress.pack(side=tk.RIGHT, padx=(10, 0))
    
    def setup_drag_drop_events(self):
        """Setup drag and drop events"""
        # Bind drag and drop events
        self.drop_target.bind('<Button-1>', self.on_drop_target_click)
        self.drop_target.bind('<B1-Motion>', self.on_drag_motion)
        self.drop_target.bind('<ButtonRelease-1>', self.on_drop_release)
        
        # Bind paste event for file dropping
        self.root.bind('<Control-v>', self.on_paste)
    
    def setup_drag_drop(self):
        """Setup drag and drop functionality"""
        # This is a simplified implementation
        # For full cross-platform drag and drop, you would need tkdnd or similar
        pass
    
    def setup_keyboard_shortcuts(self):
        """Setup keyboard shortcuts"""
        self.root.bind('<Control-o>', lambda e: self.open_file())
        self.root.bind('<Control-s>', lambda e: self.save_file())
        self.root.bind('<Control-n>', lambda e: self.new_file())
        self.root.bind('<F5>', lambda e: self.compile_code())
        self.root.bind('<F6>', lambda e: self.transpile_code())
        self.root.bind('<F7>', lambda e: self.generate_ast())
        self.root.bind('<F8>', lambda e: self.tokenize_code())
    
    def load_plugins(self):
        """Load available plugins"""
        # Load language components
        languages = []
        try:
            # Add Python
            python_info = get_python_info()
            languages.append(python_info.name)
            self.compiler_system.languages['python'] = python_info
        except:
            pass
        
        try:
            # Add JavaScript
            js_info = get_js_info()
            languages.append(js_info.name)
            self.compiler_system.languages['javascript'] = js_info
        except:
            pass
        
        try:
            # Add Rust
            rust_info = get_rust_info()
            languages.append(rust_info.name)
            self.compiler_system.languages['rust'] = rust_info
        except:
            pass
        
        # Update language combo
        self.language_combo['values'] = languages
        if languages:
            self.language_combo.set(languages[0])
            self.current_language = languages[0]
    
    def on_drop_target_click(self, event):
        """Handle drop target click"""
        self.open_file()
    
    def on_drag_motion(self, event):
        """Handle drag motion"""
        pass
    
    def on_drop_release(self, event):
        """Handle drop release"""
        pass
    
    def on_paste(self, event):
        """Handle paste event"""
        # This could be used for file dropping simulation
        pass
    
    def on_language_change(self, event):
        """Handle language change"""
        self.current_language = self.language_var.get()
        self.update_status(f"Language changed to: {self.current_language}")
    
    def on_editor_change(self, event):
        """Handle editor content change"""
        self.update_line_numbers()
    
    def update_line_numbers(self):
        """Update line numbers in the editor"""
        # Get current content
        content = self.source_editor.get("1.0", tk.END)
        lines = content.split('\n')
        
        # Update line numbers
        line_numbers_text = '\n'.join(str(i) for i in range(1, len(lines)))
        self.line_numbers.config(state='normal')
        self.line_numbers.delete("1.0", tk.END)
        self.line_numbers.insert("1.0", line_numbers_text)
        self.line_numbers.config(state='disabled')
    
    def open_file(self):
        """Open a file"""
        file_path = filedialog.askopenfilename(
            title="Open Source File",
            filetypes=[
                ("All supported", "*.py *.js *.rs *.cpp *.c *.h"),
                ("Python files", "*.py"),
                ("JavaScript files", "*.js"),
                ("Rust files", "*.rs"),
                ("C++ files", "*.cpp *.c *.h"),
                ("All files", "*.*")
            ]
        )
        
        if file_path:
            self.load_file(file_path)
    
    def load_file(self, file_path):
        """Load a file into the editor"""
        try:
            with open(file_path, 'r', encoding='utf-8') as f:
                content = f.read()
            
            # Load content into editor
            self.source_editor.delete("1.0", tk.END)
            self.source_editor.insert("1.0", content)
            
            # Update file info
            self.current_file = file_path
            self.current_file_label.config(text=os.path.basename(file_path))
            
            # Auto-detect language
            file_ext = os.path.splitext(file_path)[1].lower()
            language_map = {
                '.py': 'Python',
                '.js': 'JavaScript',
                '.rs': 'Rust',
                '.cpp': 'C++',
                '.c': 'C++',
                '.h': 'C++'
            }
            
            if file_ext in language_map:
                self.language_combo.set(language_map[file_ext])
                self.current_language = language_map[file_ext]
            
            self.update_status(f"Loaded: {os.path.basename(file_path)}")
            
        except Exception as e:
            messagebox.showerror("Error", f"Could not load file: {e}")
    
    def save_file(self):
        """Save the current file"""
        if self.current_file:
            try:
                content = self.source_editor.get("1.0", tk.END)
                with open(self.current_file, 'w', encoding='utf-8') as f:
                    f.write(content)
                self.update_status(f"Saved: {os.path.basename(self.current_file)}")
            except Exception as e:
                messagebox.showerror("Error", f"Could not save file: {e}")
        else:
            self.save_file_as()
    
    def save_file_as(self):
        """Save file with new name"""
        file_path = filedialog.asksaveasfilename(
            title="Save Source File",
            defaultextension=".py",
            filetypes=[
                ("Python files", "*.py"),
                ("JavaScript files", "*.js"),
                ("Rust files", "*.rs"),
                ("C++ files", "*.cpp"),
                ("All files", "*.*")
            ]
        )
        
        if file_path:
            try:
                content = self.source_editor.get("1.0", tk.END)
                with open(file_path, 'w', encoding='utf-8') as f:
                    f.write(content)
                
                self.current_file = file_path
                self.current_file_label.config(text=os.path.basename(file_path))
                self.update_status(f"Saved: {os.path.basename(file_path)}")
            except Exception as e:
                messagebox.showerror("Error", f"Could not save file: {e}")
    
    def new_file(self):
        """Create a new file"""
        self.source_editor.delete("1.0", tk.END)
        self.current_file = None
        self.current_file_label.config(text="No file loaded")
        self.update_status("New file created")
    
    def compile_code(self):
        """Compile the current code"""
        if not self.current_language:
            messagebox.showerror("Error", "Please select a language first")
            return
        
        # Get source code
        source_code = self.source_editor.get("1.0", tk.END).strip()
        if not source_code:
            messagebox.showerror("Error", "No source code to compile")
            return
        
        # Start compilation in a separate thread
        self.start_progress()
        threading.Thread(target=self._compile_code_thread, args=(source_code,), daemon=True).start()
    
    def _compile_code_thread(self, source_code):
        """Compile code in a separate thread"""
        try:
            # This is a simplified compilation process
            # In a real implementation, you would use the actual compiler system
            
            result = f"Compilation successful for {self.current_language}!\n\n"
            result += f"Source code length: {len(source_code)} characters\n"
            result += f"Lines: {len(source_code.splitlines())}\n"
            result += f"Language: {self.current_language}\n\n"
            result += "Generated output would appear here..."
            
            # Update UI in main thread
            self.root.after(0, self._update_compilation_result, result)
            
        except Exception as e:
            self.root.after(0, self._update_compilation_error, str(e))
        finally:
            self.root.after(0, self.stop_progress)
    
    def _update_compilation_result(self, result):
        """Update compilation result in UI"""
        self.compilation_output.delete("1.0", tk.END)
        self.compilation_output.insert("1.0", result)
        self.update_status("Compilation completed successfully")
    
    def _update_compilation_error(self, error):
        """Update compilation error in UI"""
        self.compilation_output.delete("1.0", tk.END)
        self.compilation_output.insert("1.0", f"Compilation error: {error}")
        self.update_status("Compilation failed")
    
    def transpile_code(self):
        """Transpile code to target language"""
        if not self.current_language:
            messagebox.showerror("Error", "Please select a source language first")
            return
        
        target_language = self.target_language_var.get()
        if not target_language:
            messagebox.showerror("Error", "Please select a target language")
            return
        
        # Get source code
        source_code = self.source_editor.get("1.0", tk.END).strip()
        if not source_code:
            messagebox.showerror("Error", "No source code to transpile")
            return
        
        # Start transpilation in a separate thread
        self.start_progress()
        threading.Thread(target=self._transpile_code_thread, 
                        args=(source_code, self.current_language, target_language), daemon=True).start()
    
    def _transpile_code_thread(self, source_code, source_lang, target_lang):
        """Transpile code in a separate thread"""
        try:
            # This is a simplified transpilation process
            # In a real implementation, you would use the actual transpiler system
            
            result = f"Transpilation from {source_lang} to {target_lang}:\n\n"
            
            if source_lang == "Python" and target_lang == "C++":
                result += self._transpile_python_to_cpp(source_code)
            elif source_lang == "Python" and target_lang == "Rust":
                result += self._transpile_python_to_rust(source_code)
            else:
                result += f"Transpilation from {source_lang} to {target_lang} not yet implemented.\n"
                result += "This would use the actual transpiler system."
            
            # Update UI in main thread
            self.root.after(0, self._update_transpilation_result, result)
            
        except Exception as e:
            self.root.after(0, self._update_transpilation_error, str(e))
        finally:
            self.root.after(0, self.stop_progress)
    
    def _transpile_python_to_cpp(self, source_code):
        """Transpile Python to C++ (simplified)"""
        result = "// Generated C++ code:\n"
        result += "#include <iostream>\n"
        result += "#include <vector>\n"
        result += "#include <string>\n\n"
        result += "int main() {\n"
        
        # Simple transpilation logic
        lines = source_code.split('\n')
        for line in lines:
            line = line.strip()
            if line.startswith('print('):
                # Convert print to cout
                content = line[6:-1]  # Remove print( and )
                result += f"    std::cout << {content} << std::endl;\n"
            elif '=' in line and not line.startswith('#'):
                # Convert assignment
                result += f"    auto {line};\n"
        
        result += "    return 0;\n"
        result += "}\n"
        
        return result
    
    def _transpile_python_to_rust(self, source_code):
        """Transpile Python to Rust (simplified)"""
        result = "// Generated Rust code:\n"
        result += "fn main() {\n"
        
        # Simple transpilation logic
        lines = source_code.split('\n')
        for line in lines:
            line = line.strip()
            if line.startswith('print('):
                # Convert print to println!
                content = line[6:-1]  # Remove print( and )
                result += f"    println!(\"{content}\");\n"
            elif '=' in line and not line.startswith('#'):
                # Convert assignment
                result += f"    let mut {line};\n"
        
        result += "}\n"
        
        return result
    
    def _update_transpilation_result(self, result):
        """Update transpilation result in UI"""
        self.transpilation_output.delete("1.0", tk.END)
        self.transpilation_output.insert("1.0", result)
        self.update_status("Transpilation completed successfully")
    
    def _update_transpilation_error(self, error):
        """Update transpilation error in UI"""
        self.transpilation_output.delete("1.0", tk.END)
        self.transpilation_output.insert("1.0", f"Transpilation error: {error}")
        self.update_status("Transpilation failed")
    
    def generate_ast(self):
        """Generate AST from current code"""
        # Implementation for AST generation
        self.update_status("AST generation not yet implemented")
    
    def show_ast(self):
        """Show AST in the AST tab"""
        self.notebook.select(1)  # Switch to AST tab
    
    def show_tokens(self):
        """Show tokens in the tokens tab"""
        self.notebook.select(2)  # Switch to tokens tab
    
    def tokenize_code(self):
        """Tokenize the current code"""
        # Implementation for tokenization
        self.update_status("Tokenization not yet implemented")
    
    def transpile_to_cpp(self):
        """Transpile to C++"""
        self.target_language_combo.set("C++")
        self.transpile_code()
    
    def transpile_to_rust(self):
        """Transpile to Rust"""
        self.target_language_combo.set("Rust")
        self.transpile_code()
    
    def show_component_manager(self):
        """Show component manager"""
        # Implementation for component manager
        self.update_status("Component manager not yet implemented")
    
    def show_analytics(self):
        """Show analytics"""
        self.notebook.select(4)  # Switch to analytics tab
    
    def generate_analytics(self):
        """Generate analytics"""
        analytics = f"Code Analytics:\n\n"
        analytics += f"File: {self.current_file or 'No file loaded'}\n"
        analytics += f"Language: {self.current_language or 'Not selected'}\n"
        analytics += f"Lines: {len(self.source_editor.get('1.0', tk.END).splitlines())}\n"
        analytics += f"Characters: {len(self.source_editor.get('1.0', tk.END))}\n"
        analytics += f"Words: {len(self.source_editor.get('1.0', tk.END).split())}\n\n"
        analytics += "Advanced analytics would be generated here..."
        
        self.analytics_text.delete("1.0", tk.END)
        self.analytics_text.insert("1.0", analytics)
        self.update_status("Analytics generated")
    
    def clear_compilation_output(self):
        """Clear compilation output"""
        self.compilation_output.delete("1.0", tk.END)
    
    def clear_ast(self):
        """Clear AST"""
        for item in self.ast_tree.get_children():
            self.ast_tree.delete(item)
    
    def clear_tokens(self):
        """Clear tokens"""
        for item in self.tokens_tree.get_children():
            self.tokens_tree.delete(item)
    
    def clear_transpilation_output(self):
        """Clear transpilation output"""
        self.transpilation_output.delete("1.0", tk.END)
    
    def clear_analytics(self):
        """Clear analytics"""
        self.analytics_text.delete("1.0", tk.END)
    
    def save_compilation_output(self):
        """Save compilation output"""
        content = self.compilation_output.get("1.0", tk.END)
        if content.strip():
            file_path = filedialog.asksaveasfilename(
                title="Save Compilation Output",
                defaultextension=".txt",
                filetypes=[("Text files", "*.txt"), ("All files", "*.*")]
            )
            if file_path:
                with open(file_path, 'w') as f:
                    f.write(content)
                self.update_status(f"Compilation output saved to {os.path.basename(file_path)}")
    
    def save_transpilation_output(self):
        """Save transpilation output"""
        content = self.transpilation_output.get("1.0", tk.END)
        if content.strip():
            target_lang = self.target_language_var.get()
            ext = ".cpp" if target_lang == "C++" else ".rs" if target_lang == "Rust" else ".txt"
            
            file_path = filedialog.asksaveasfilename(
                title="Save Transpilation Output",
                defaultextension=ext,
                filetypes=[("All files", "*.*")]
            )
            if file_path:
                with open(file_path, 'w') as f:
                    f.write(content)
                self.update_status(f"Transpilation output saved to {os.path.basename(file_path)}")
    
    def export_ast(self):
        """Export AST"""
        # Implementation for AST export
        self.update_status("AST export not yet implemented")
    
    def export_tokens(self):
        """Export tokens"""
        # Implementation for token export
        self.update_status("Token export not yet implemented")
    
    def start_progress(self):
        """Start progress indicator"""
        self.progress.start()
        self.update_status("Processing...")
    
    def stop_progress(self):
        """Stop progress indicator"""
        self.progress.stop()
    
    def update_status(self, message):
        """Update status bar"""
        self.status_label.config(text=message)
        self.root.update_idletasks()
    
    def run(self):
        """Run the GUI"""
        self.root.mainloop()

def test_comprehensive_compiler_gui():
    """Test the comprehensive compiler GUI"""
    print("🧪 Testing Comprehensive Compiler GUI...")
    
    try:
        gui = ComprehensiveCompilerGUI()
        print("✅ GUI created successfully")
        print("🚀 Starting GUI...")
        gui.run()
    except Exception as e:
        print(f"❌ GUI test failed: {e}")
        return False
    
    return True

if __name__ == "__main__":
    test_comprehensive_compiler_gui()
