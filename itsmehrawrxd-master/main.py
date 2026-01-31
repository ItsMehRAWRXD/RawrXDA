#!/usr/bin/env python3
"""
Main Extensible Compiler System GUI
Complete implementation with all language parsers, IR passes, and code generators
"""

import os
import sys
import json
import importlib
import inspect
from abc import ABC, abstractmethod
from typing import Dict, List, Any, Optional
from dataclasses import dataclass, field
from enum import Enum
import tkinter as tk
from tkinter import ttk, filedialog, messagebox
import collections

# Import our components
from main_compiler_system import ExtensibleCompilerSystem, CompilerException
from ast_nodes import print_ast

class CompilerGUI:
    """Enhanced GUI for the Extensible Compiler System"""
    
    def __init__(self, master, compiler_system):
        self.master = master
        self.compiler = compiler_system
        self.master.title("Extensible Compiler System - Meta-Prompting AST/IR Generator")
        self.master.geometry("1200x800")

        self.create_widgets()
        self.load_components()

    def create_widgets(self):
        """Create the GUI widgets"""
        # Main Frame
        main_frame = ttk.Frame(self.master, padding="10")
        main_frame.grid(row=0, column=0, sticky=(tk.W, tk.E, tk.N, tk.S))
        self.master.columnconfigure(0, weight=1)
        self.master.rowconfigure(0, weight=1)

        # Create notebook for tabs
        self.notebook = ttk.Notebook(main_frame)
        self.notebook.grid(row=0, column=0, columnspan=3, sticky=(tk.W, tk.E, tk.N, tk.S))
        main_frame.rowconfigure(0, weight=1)
        main_frame.columnconfigure(0, weight=1)

        # Compiler Tab
        self.create_compiler_tab()
        
        # AST Viewer Tab
        self.create_ast_viewer_tab()
        
        # Language Support Tab
        self.create_language_support_tab()

    def create_compiler_tab(self):
        """Create the main compiler tab"""
        compiler_frame = ttk.Frame(self.notebook, padding="10")
        self.notebook.add(compiler_frame, text="Compiler")
        
        # Source Code Area
        ttk.Label(compiler_frame, text="Source Code:", font=('Arial', 12, 'bold')).grid(row=0, column=0, sticky=tk.W, pady=(0, 5))
        self.source_code_text = tk.Text(compiler_frame, height=15, width=80, font=('Consolas', 10))
        self.source_code_text.grid(row=1, column=0, columnspan=3, sticky=(tk.W, tk.E), pady=(0, 10))
        
        # Options Frame
        options_frame = ttk.LabelFrame(compiler_frame, text="Compilation Options", padding="10")
        options_frame.grid(row=2, column=0, columnspan=3, sticky=(tk.W, tk.E), pady=10)

        # Language Selection
        ttk.Label(options_frame, text="Language:").grid(row=0, column=0, padx=5, pady=5, sticky=tk.W)
        self.language_var = tk.StringVar()
        self.language_combo = ttk.Combobox(options_frame, textvariable=self.language_var, state='readonly', width=15)
        self.language_combo.grid(row=0, column=1, padx=5, pady=5, sticky=(tk.W, tk.E))
        
        # Passes Selection
        ttk.Label(options_frame, text="IR Passes:").grid(row=1, column=0, padx=5, pady=5, sticky=tk.W)
        self.passes_listbox = tk.Listbox(options_frame, selectmode=tk.MULTIPLE, height=4, width=20)
        self.passes_listbox.grid(row=1, column=1, padx=5, pady=5, sticky=(tk.W, tk.E))

        # Target Selection
        ttk.Label(options_frame, text="Backend Target:").grid(row=2, column=0, padx=5, pady=5, sticky=tk.W)
        self.target_var = tk.StringVar()
        self.target_combo = ttk.Combobox(options_frame, textvariable=self.target_var, state='readonly', width=15)
        self.target_combo.grid(row=2, column=1, padx=5, pady=5, sticky=(tk.W, tk.E))
        
        options_frame.columnconfigure(1, weight=1)

        # Control Buttons
        button_frame = ttk.Frame(compiler_frame)
        button_frame.grid(row=3, column=0, columnspan=3, pady=10)
        ttk.Button(button_frame, text="Load File", command=self.load_file).grid(row=0, column=0, padx=5)
        ttk.Button(button_frame, text="Compile", command=self.compile_code).grid(row=0, column=1, padx=5)
        ttk.Button(button_frame, text="Show AST", command=self.show_ast).grid(row=0, column=2, padx=5)
        ttk.Button(button_frame, text="Clear", command=self.clear_fields).grid(row=0, column=3, padx=5)
        
        # Output Area
        ttk.Label(compiler_frame, text="Compilation Output:", font=('Arial', 12, 'bold')).grid(row=4, column=0, sticky=tk.W, pady=(10, 5))
        self.output_text = tk.Text(compiler_frame, height=10, width=80, state=tk.DISABLED, font=('Consolas', 10))
        self.output_text.grid(row=5, column=0, columnspan=3, sticky=(tk.W, tk.E))

        # Status Bar
        self.status_bar = ttk.Label(compiler_frame, text="Ready.", relief=tk.SUNKEN, anchor=tk.W)
        self.status_bar.grid(row=6, column=0, columnspan=3, sticky=(tk.W, tk.E), pady=5)

    def create_ast_viewer_tab(self):
        """Create the AST viewer tab"""
        ast_frame = ttk.Frame(self.notebook, padding="10")
        self.notebook.add(ast_frame, text="AST Viewer")
        
        # AST Display
        ttk.Label(ast_frame, text="Abstract Syntax Tree:", font=('Arial', 12, 'bold')).grid(row=0, column=0, sticky=tk.W, pady=(0, 5))
        self.ast_text = tk.Text(ast_frame, height=25, width=80, font=('Consolas', 10))
        self.ast_text.grid(row=1, column=0, sticky=(tk.W, tk.E, tk.N, tk.S))
        
        # Scrollbar for AST
        ast_scrollbar = ttk.Scrollbar(ast_frame, orient=tk.VERTICAL, command=self.ast_text.yview)
        ast_scrollbar.grid(row=1, column=1, sticky=(tk.N, tk.S))
        self.ast_text.configure(yscrollcommand=ast_scrollbar.set)
        
        ast_frame.rowconfigure(1, weight=1)
        ast_frame.columnconfigure(0, weight=1)

    def create_language_support_tab(self):
        """Create the language support information tab"""
        support_frame = ttk.Frame(self.notebook, padding="10")
        self.notebook.add(support_frame, text="Language Support")
        
        # Language Information
        ttk.Label(support_frame, text="Supported Languages:", font=('Arial', 12, 'bold')).grid(row=0, column=0, sticky=tk.W, pady=(0, 5))
        
        # Create treeview for language information
        self.language_tree = ttk.Treeview(support_frame, columns=('Type', 'Description'), show='tree headings')
        self.language_tree.heading('#0', text='Language')
        self.language_tree.heading('Type', text='Type')
        self.language_tree.heading('Description', text='Description')
        self.language_tree.grid(row=1, column=0, sticky=(tk.W, tk.E, tk.N, tk.S), pady=(0, 10))
        
        # IR Passes Information
        ttk.Label(support_frame, text="Available IR Passes:", font=('Arial', 12, 'bold')).grid(row=2, column=0, sticky=tk.W, pady=(0, 5))
        
        self.passes_tree = ttk.Treeview(support_frame, columns=('Level', 'Dependencies'), show='tree headings')
        self.passes_tree.heading('#0', text='Pass Name')
        self.passes_tree.heading('Level', text='Level')
        self.passes_tree.heading('Dependencies', text='Dependencies')
        self.passes_tree.grid(row=3, column=0, sticky=(tk.W, tk.E, tk.N, tk.S), pady=(0, 10))
        
        # Backend Targets Information
        ttk.Label(support_frame, text="Backend Targets:", font=('Arial', 12, 'bold')).grid(row=4, column=0, sticky=tk.W, pady=(0, 5))
        
        self.targets_tree = ttk.Treeview(support_frame, columns=('Type', 'Platform'), show='tree headings')
        self.targets_tree.heading('#0', text='Target Name')
        self.targets_tree.heading('Type', text='Type')
        self.targets_tree.heading('Platform', text='Platform')
        self.targets_tree.grid(row=5, column=0, sticky=(tk.W, tk.E, tk.N, tk.S))
        
        support_frame.rowconfigure(1, weight=1)
        support_frame.rowconfigure(3, weight=1)
        support_frame.rowconfigure(5, weight=1)
        support_frame.columnconfigure(0, weight=1)

    def load_components(self):
        """Populate the GUI with available components"""
        # Load languages
        languages = sorted(list(self.compiler.languages.keys()))
        self.language_combo['values'] = languages
        if languages:
            self.language_combo.set(languages[0])
            
        # Load passes
        passes = sorted(list(self.compiler.ir_passes.keys()))
        for p in passes:
            self.passes_listbox.insert(tk.END, p)
            
        # Load targets
        targets = sorted(list(self.compiler.backend_targets.keys()))
        self.target_combo['values'] = targets
        if targets:
            self.target_combo.set(targets[0])
        
        # Populate language support tab
        self.populate_language_support()

    def populate_language_support(self):
        """Populate the language support information"""
        # Clear existing items
        for item in self.language_tree.get_children():
            self.language_tree.delete(item)
        for item in self.passes_tree.get_children():
            self.passes_tree.delete(item)
        for item in self.targets_tree.get_children():
            self.targets_tree.delete(item)
        
        # Add languages
        for lang_name, lang_info in self.compiler.languages.items():
            self.language_tree.insert('', 'end', text=lang_info.name, 
                                    values=(lang_info.language_type.value, lang_info.description))
        
        # Add IR passes
        for pass_name, pass_info in self.compiler.ir_passes.items():
            deps = ', '.join(pass_info.dependencies) if pass_info.dependencies else 'None'
            self.passes_tree.insert('', 'end', text=pass_info.name,
                                  values=(pass_info.optimization_level, deps))
        
        # Add backend targets
        for target_name, target_info in self.compiler.backend_targets.items():
            self.targets_tree.insert('', 'end', text=target_info.name,
                                   values=(target_info.target_type.value, target_info.platform))

    def load_file(self):
        """Load a source file"""
        file_path = filedialog.askopenfilename(
            defaultextension=".txt",
            filetypes=[
                ("Source files", "*.cpp *.py *.js *.rs *.c *.h"),
                ("C++ files", "*.cpp *.hpp *.h"),
                ("Python files", "*.py"),
                ("JavaScript files", "*.js"),
                ("Rust files", "*.rs"),
                ("All files", "*.*")
            ]
        )
        if file_path:
            try:
                with open(file_path, 'r', encoding='utf-8') as f:
                    self.source_code_text.delete(1.0, tk.END)
                    self.source_code_text.insert(tk.END, f.read())
                self.status_bar.config(text=f"Loaded file: {os.path.basename(file_path)}")
            except Exception as e:
                messagebox.showerror("Error", f"Failed to read file: {e}")
                self.status_bar.config(text="File loading failed.")

    def compile_code(self):
        """Compile the source code"""
        self.clear_output()
        self.status_bar.config(text="Compiling...")
        
        source_code = self.source_code_text.get(1.0, tk.END).strip()
        if not source_code:
            messagebox.showwarning("Warning", "Please enter some source code.")
            self.status_bar.config(text="Compilation aborted.")
            return

        language = self.language_var.get()
        selected_passes_indices = self.passes_listbox.curselection()
        selected_passes = [self.passes_listbox.get(i) for i in selected_passes_indices]
        target = self.target_var.get()

        if not language or not target:
            messagebox.showwarning("Warning", "Please select a language and a target.")
            self.status_bar.config(text="Compilation aborted.")
            return

        try:
            output = self.compiler.compile(source_code, language, selected_passes, target)
            self.display_output(output)
            self.status_bar.config(text="Compilation successful!")
        except CompilerException as e:
            messagebox.showerror("Compiler Error", str(e))
            self.status_bar.config(text=f"Compilation failed: {e.stage.upper()}")
        except Exception as e:
            messagebox.showerror("Unknown Error", f"An unexpected error occurred: {e}")
            self.status_bar.config(text="Compilation failed with unexpected error.")
            
    def show_ast(self):
        """Show AST without compilation"""
        source_code = self.source_code_text.get(1.0, tk.END).strip()
        if not source_code:
            messagebox.showwarning("Warning", "Please enter some source code.")
            return

        language = self.language_var.get()
        if not language:
            messagebox.showwarning("Warning", "Please select a language.")
            return

        try:
            # Just parse to AST
            lang_info = self.compiler.languages[language]
            lexer_class = self.compiler.custom_parsers[lang_info.lexer_class]
            lexer = lexer_class()
            tokens = lexer.tokenize(source_code)
            
            parser_class = self.compiler.custom_parsers[lang_info.parser_class]
            parser = parser_class()
            ast = parser.parse(tokens)
            
            # Display AST in both output and AST viewer
            ast_output = print_ast(ast)
            self.display_output(ast_output)
            self.ast_text.delete(1.0, tk.END)
            self.ast_text.insert(tk.END, ast_output)
            self.status_bar.config(text="AST generated successfully!")
            
        except Exception as e:
            messagebox.showerror("Error", f"Failed to generate AST: {e}")
            self.status_bar.config(text="AST generation failed.")
            
    def display_output(self, text):
        """Display text in the output text widget"""
        self.output_text.config(state=tk.NORMAL)
        self.output_text.delete(1.0, tk.END)
        self.output_text.insert(tk.END, text)
        self.output_text.config(state=tk.DISABLED)
        
    def clear_fields(self):
        """Clear all text fields"""
        self.source_code_text.delete(1.0, tk.END)
        self.clear_output()
        self.ast_text.delete(1.0, tk.END)
        self.status_bar.config(text="Ready.")

    def clear_output(self):
        """Clear the output text widget"""
        self.output_text.config(state=tk.NORMAL)
        self.output_text.delete(1.0, tk.END)
        self.output_text.config(state=tk.DISABLED)

def main():
    """Main entry point"""
    print("🚀 Starting Extensible Compiler System...")
    print("=" * 60)
    
    # Initialize the compiler system
    system = ExtensibleCompilerSystem()
    
    # Show system information
    print(f"📋 Loaded {len(system.languages)} languages:")
    for lang_name, lang_info in system.languages.items():
        print(f"  • {lang_info.name} ({lang_info.language_type.value})")
    
    print(f"\n🔧 Loaded {len(system.ir_passes)} IR passes:")
    for pass_name, pass_info in system.ir_passes.items():
        print(f"  • {pass_info.name} (level {pass_info.optimization_level})")
    
    print(f"\n🎯 Loaded {len(system.backend_targets)} backend targets:")
    for target_name, target_info in system.backend_targets.items():
        print(f"  • {target_info.name} ({target_info.target_type.value})")
    
    print("\n✅ Extensible Compiler System ready!")
    print("=" * 60)
    
    # Create and run GUI
    root = tk.Tk()
    app = CompilerGUI(root, system)
    
    # Add some sample code
    sample_code = """int main() {
    int x = 5 + 3;
    int y = x * 2;
    return y;
}"""
    app.source_code_text.insert(tk.END, sample_code)
    app.language_combo.set('cpp')
    app.target_combo.set('print_ast')
    
    root.mainloop()

if __name__ == "__main__":
    main()
