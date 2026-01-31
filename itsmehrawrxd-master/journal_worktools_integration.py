#!/usr/bin/env python3
"""
Journal IDE Worktools Integration
Connects external worktools to the Journal IDE
"""

import tkinter as tk
from tkinter import ttk, messagebox, filedialog
import os
import sys
import threading
import queue
from typing import Dict, List, Optional, Any
from external_worktools import ExternalWorktools

class JournalWorktoolsIntegration:
    """Integration between Journal IDE and external worktools"""
    
    def __init__(self, journal_ide):
        self.journal_ide = journal_ide
        self.worktools = ExternalWorktools(journal_ide)
        self.output_queue = queue.Queue()
        self.is_running = False
        
    def create_worktools_menu(self, parent_menu):
        """Create worktools menu for Journal IDE"""
        worktools_menu = tk.Menu(parent_menu, tearoff=0)
        parent_menu.add_cascade(label="🔧 Worktools", menu=worktools_menu)
        
        # Compilation submenu
        compile_menu = tk.Menu(worktools_menu, tearoff=0)
        worktools_menu.add_cascade(label="📦 Compile", menu=compile_menu)
        compile_menu.add_command(label="C++ File", command=self.compile_cpp_file)
        compile_menu.add_command(label="Java File", command=self.compile_java_file)
        compile_menu.add_separator()
        compile_menu.add_command(label="Detect Compilers", command=self.detect_compilers)
        
        # Execution submenu
        run_menu = tk.Menu(worktools_menu, tearoff=0)
        worktools_menu.add_cascade(label="▶️ Run", menu=run_menu)
        run_menu.add_command(label="Python Script", command=self.run_python_script)
        run_menu.add_command(label="Node.js Script", command=self.run_nodejs_script)
        run_menu.add_command(label="Java Class", command=self.run_java_class)
        run_menu.add_command(label="Executable", command=self.run_executable)
        
        # Build submenu
        build_menu = tk.Menu(worktools_menu, tearoff=0)
        worktools_menu.add_cascade(label="🏗️ Build", menu=build_menu)
        build_menu.add_command(label="Create Build Script", command=self.create_build_script)
        build_menu.add_command(label="Run Build Script", command=self.run_build_script)
        build_menu.add_separator()
        build_menu.add_command(label="Project Settings", command=self.show_project_settings)
        
        # Tools submenu
        tools_menu = tk.Menu(worktools_menu, tearoff=0)
        worktools_menu.add_cascade(label="🛠️ Tools", menu=tools_menu)
        tools_menu.add_command(label="System Info", command=self.show_system_info)
        tools_menu.add_command(label="Terminal", command=self.open_terminal)
        tools_menu.add_command(label="File Explorer", command=self.open_file_explorer)
        
        worktools_menu.add_separator()
        worktools_menu.add_command(label="About Worktools", command=self.show_about)
        
        return worktools_menu
    
    def create_worktools_toolbar(self, parent_frame):
        """Create worktools toolbar"""
        toolbar = ttk.Frame(parent_frame)
        
        # Compile buttons
        ttk.Button(toolbar, text="🔨 C++", command=self.compile_cpp_file).pack(side=tk.LEFT, padx=2)
        ttk.Button(toolbar, text="☕ Java", command=self.compile_java_file).pack(side=tk.LEFT, padx=2)
        ttk.Button(toolbar, text="🐍 Python", command=self.run_python_script).pack(side=tk.LEFT, padx=2)
        ttk.Button(toolbar, text="🟢 Node.js", command=self.run_nodejs_script).pack(side=tk.LEFT, padx=2)
        
        # Separator
        ttk.Separator(toolbar, orient=tk.VERTICAL).pack(side=tk.LEFT, fill=tk.Y, padx=5)
        
        # Build buttons
        ttk.Button(toolbar, text="🏗️ Build", command=self.create_build_script).pack(side=tk.LEFT, padx=2)
        ttk.Button(toolbar, text="▶️ Run", command=self.run_executable).pack(side=tk.LEFT, padx=2)
        ttk.Button(toolbar, text="🔍 Info", command=self.show_system_info).pack(side=tk.LEFT, padx=2)
        
        return toolbar
    
    def compile_cpp_file(self):
        """Compile C++ file"""
        file_path = filedialog.askopenfilename(
            title="Select C++ file to compile",
            filetypes=[("C++ files", "*.cpp *.cc *.cxx"), ("All files", "*.*")]
        )
        
        if not file_path:
            return
        
        # Show compilation dialog
        self.show_compilation_dialog(file_path, "cpp")
    
    def compile_java_file(self):
        """Compile Java file"""
        file_path = filedialog.askopenfilename(
            title="Select Java file to compile",
            filetypes=[("Java files", "*.java"), ("All files", "*.*")]
        )
        
        if not file_path:
            return
        
        # Show compilation dialog
        self.show_compilation_dialog(file_path, "java")
    
    def run_python_script(self):
        """Run Python script"""
        file_path = filedialog.askopenfilename(
            title="Select Python script to run",
            filetypes=[("Python files", "*.py"), ("All files", "*.*")]
        )
        
        if not file_path:
            return
        
        self.run_script_async(file_path, "python")
    
    def run_nodejs_script(self):
        """Run Node.js script"""
        file_path = filedialog.askopenfilename(
            title="Select Node.js script to run",
            filetypes=[("JavaScript files", "*.js"), ("All files", "*.*")]
        )
        
        if not file_path:
            return
        
        self.run_script_async(file_path, "nodejs")
    
    def run_java_class(self):
        """Run Java class"""
        file_path = filedialog.askopenfilename(
            title="Select Java class file to run",
            filetypes=[("Java class files", "*.class"), ("All files", "*.*")]
        )
        
        if not file_path:
            return
        
        self.run_script_async(file_path, "java")
    
    def run_executable(self):
        """Run executable file"""
        file_path = filedialog.askopenfilename(
            title="Select executable to run",
            filetypes=[("Executable files", "*.exe *.out"), ("All files", "*.*")]
        )
        
        if not file_path:
            return
        
        self.run_script_async(file_path, "executable")
    
    def show_compilation_dialog(self, file_path: str, file_type: str):
        """Show compilation options dialog"""
        dialog = tk.Toplevel(self.journal_ide.root)
        dialog.title(f"Compile {file_type.upper()} File")
        dialog.geometry("500x400")
        dialog.transient(self.journal_ide.root)
        dialog.grab_set()
        
        # Main frame
        main_frame = ttk.Frame(dialog)
        main_frame.pack(fill=tk.BOTH, expand=True, padx=10, pady=10)
        
        # File info
        ttk.Label(main_frame, text=f"File: {os.path.basename(file_path)}", font=("Arial", 10, "bold")).pack(anchor=tk.W)
        ttk.Label(main_frame, text=f"Path: {file_path}", font=("Arial", 8)).pack(anchor=tk.W)
        
        # Compiler selection
        compiler_frame = ttk.LabelFrame(main_frame, text="Compiler Options")
        compiler_frame.pack(fill=tk.X, pady=(10, 0))
        
        ttk.Label(compiler_frame, text="Compiler:").grid(row=0, column=0, sticky=tk.W, padx=5, pady=5)
        compiler_var = tk.StringVar(value="auto")
        compiler_combo = ttk.Combobox(compiler_frame, textvariable=compiler_var, state="readonly")
        compiler_combo.grid(row=0, column=1, sticky=tk.W+tk.E, padx=5, pady=5)
        
        # Get available compilers
        compilers = self.worktools.detect_compilers()
        available_compilers = ["auto"] + [name for name, info in compilers.items() if info['available']]
        compiler_combo['values'] = available_compilers
        
        # Optimization selection
        ttk.Label(compiler_frame, text="Optimization:").grid(row=1, column=0, sticky=tk.W, padx=5, pady=5)
        opt_var = tk.StringVar(value="O2")
        opt_combo = ttk.Combobox(compiler_frame, textvariable=opt_var, state="readonly")
        opt_combo['values'] = ["O0", "O1", "O2", "O3", "Os"]
        opt_combo.grid(row=1, column=1, sticky=tk.W+tk.E, padx=5, pady=5)
        
        # Output file
        ttk.Label(compiler_frame, text="Output file:").grid(row=2, column=0, sticky=tk.W, padx=5, pady=5)
        output_var = tk.StringVar()
        output_entry = ttk.Entry(compiler_frame, textvariable=output_var)
        output_entry.grid(row=2, column=1, sticky=tk.W+tk.E, padx=5, pady=5)
        
        # Set default output file
        if file_type == "cpp":
            output_var.set(os.path.splitext(file_path)[0] + (".exe" if os.name == "nt" else ""))
        elif file_type == "java":
            output_var.set(os.path.splitext(file_path)[0] + ".class")
        
        # Buttons
        button_frame = ttk.Frame(main_frame)
        button_frame.pack(fill=tk.X, pady=(10, 0))
        
        def compile_file():
            compiler = compiler_var.get()
            optimization = opt_var.get()
            output_file = output_var.get() if output_var.get() else None
            
            # Start compilation in background
            def compile_thread():
                if file_type == "cpp":
                    result = self.worktools.compile_cpp(file_path, output_file, compiler, optimization)
                elif file_type == "java":
                    result = self.worktools.compile_java(file_path, output_file)
                else:
                    result = {"success": False, "error": f"Unsupported file type: {file_type}"}
                
                # Show result
                dialog.after(0, lambda: self.show_compilation_result(result, dialog))
            
            threading.Thread(target=compile_thread, daemon=True).start()
        
        ttk.Button(button_frame, text="Compile", command=compile_file).pack(side=tk.LEFT, padx=5)
        ttk.Button(button_frame, text="Cancel", command=dialog.destroy).pack(side=tk.LEFT, padx=5)
        
        # Progress bar
        progress = ttk.Progressbar(main_frame, mode='indeterminate')
        progress.pack(fill=tk.X, pady=(10, 0))
        progress.start()
        
        dialog.protocol("WM_DELETE_WINDOW", dialog.destroy)
    
    def show_compilation_result(self, result: Dict[str, Any], dialog):
        """Show compilation result"""
        dialog.destroy()
        
        if result['success']:
            messagebox.showinfo("Compilation Successful", 
                              f"File compiled successfully!\n\nOutput: {result['output_file']}\n"
                              f"Compiler: {result.get('compiler', 'Unknown')}")
        else:
            messagebox.showerror("Compilation Failed", 
                               f"Compilation failed!\n\nError: {result['error']}")
    
    def run_script_async(self, file_path: str, script_type: str):
        """Run script asynchronously"""
        def run_thread():
            if script_type == "python":
                result = self.worktools.run_python(file_path)
            elif script_type == "nodejs":
                result = self.worktools.run_nodejs(file_path)
            elif script_type == "java":
                result = self.worktools.run_java(file_path)
            elif script_type == "executable":
                result = self.worktools.execute_command(f'"{file_path}"')
            else:
                result = {"success": False, "error": f"Unsupported script type: {script_type}"}
            
            # Show result in output window
            self.journal_ide.after(0, lambda: self.show_script_result(result, script_type))
        
        threading.Thread(target=run_thread, daemon=True).start()
    
    def show_script_result(self, result: Dict[str, Any], script_type: str):
        """Show script execution result"""
        if result['success']:
            output = result.get('output', '')
            if output:
                # Create output window
                output_window = tk.Toplevel(self.journal_ide.root)
                output_window.title(f"{script_type.upper()} Output")
                output_window.geometry("600x400")
                
                # Output text
                text_widget = tk.Text(output_window, wrap=tk.WORD)
                text_widget.pack(fill=tk.BOTH, expand=True, padx=10, pady=10)
                text_widget.insert(tk.END, output)
                text_widget.config(state=tk.DISABLED)
                
                # Scrollbar
                scrollbar = ttk.Scrollbar(output_window, orient=tk.VERTICAL, command=text_widget.yview)
                scrollbar.pack(side=tk.RIGHT, fill=tk.Y)
                text_widget.config(yscrollcommand=scrollbar.set)
        else:
            messagebox.showerror(f"{script_type.upper()} Execution Failed", 
                               f"Execution failed!\n\nError: {result['error']}")
    
    def detect_compilers(self):
        """Detect available compilers"""
        compilers = self.worktools.detect_compilers()
        
        # Create info window
        info_window = tk.Toplevel(self.journal_ide.root)
        info_window.title("Available Compilers")
        info_window.geometry("500x300")
        
        # Create treeview
        tree = ttk.Treeview(info_window, columns=("name", "version", "type"), show="tree headings")
        tree.heading("#0", text="Compiler")
        tree.heading("name", text="Name")
        tree.heading("version", text="Version")
        tree.heading("type", text="Type")
        
        for name, info in compilers.items():
            if info['available']:
                tree.insert("", "end", text=name, values=(
                    info['name'],
                    info['version'],
                    info.get('type', 'Unknown')
                ))
        
        tree.pack(fill=tk.BOTH, expand=True, padx=10, pady=10)
        
        # Scrollbar
        scrollbar = ttk.Scrollbar(info_window, orient=tk.VERTICAL, command=tree.yview)
        scrollbar.pack(side=tk.RIGHT, fill=tk.Y)
        tree.config(yscrollcommand=scrollbar.set)
    
    def create_build_script(self):
        """Create build script for current project"""
        project_dir = filedialog.askdirectory(title="Select project directory")
        if not project_dir:
            return
        
        # Show build configuration dialog
        self.show_build_config_dialog(project_dir)
    
    def show_build_config_dialog(self, project_dir: str):
        """Show build configuration dialog"""
        dialog = tk.Toplevel(self.journal_ide.root)
        dialog.title("Build Configuration")
        dialog.geometry("600x500")
        dialog.transient(self.journal_ide.root)
        dialog.grab_set()
        
        # Main frame
        main_frame = ttk.Frame(dialog)
        main_frame.pack(fill=tk.BOTH, expand=True, padx=10, pady=10)
        
        # Project info
        ttk.Label(main_frame, text=f"Project: {os.path.basename(project_dir)}", 
                 font=("Arial", 10, "bold")).pack(anchor=tk.W)
        ttk.Label(main_frame, text=f"Path: {project_dir}", font=("Arial", 8)).pack(anchor=tk.W)
        
        # Build configuration
        config_frame = ttk.LabelFrame(main_frame, text="Build Configuration")
        config_frame.pack(fill=tk.BOTH, expand=True, pady=(10, 0))
        
        # C++ files
        cpp_frame = ttk.Frame(config_frame)
        cpp_frame.pack(fill=tk.X, padx=5, pady=5)
        
        ttk.Label(cpp_frame, text="C++ Files:").pack(anchor=tk.W)
        cpp_listbox = tk.Listbox(cpp_frame, height=3)
        cpp_listbox.pack(fill=tk.X, pady=(5, 0))
        
        def add_cpp_file():
            file_path = filedialog.askopenfilename(
                title="Select C++ file",
                filetypes=[("C++ files", "*.cpp *.cc *.cxx"), ("All files", "*.*")]
            )
            if file_path:
                cpp_listbox.insert(tk.END, file_path)
        
        ttk.Button(cpp_frame, text="Add C++ File", command=add_cpp_file).pack(anchor=tk.W, pady=(5, 0))
        
        # Python files
        py_frame = ttk.Frame(config_frame)
        py_frame.pack(fill=tk.X, padx=5, pady=5)
        
        ttk.Label(py_frame, text="Python Files:").pack(anchor=tk.W)
        py_listbox = tk.Listbox(py_frame, height=3)
        py_listbox.pack(fill=tk.X, pady=(5, 0))
        
        def add_py_file():
            file_path = filedialog.askopenfilename(
                title="Select Python file",
                filetypes=[("Python files", "*.py"), ("All files", "*.*")]
            )
            if file_path:
                py_listbox.insert(tk.END, file_path)
        
        ttk.Button(py_frame, text="Add Python File", command=add_py_file).pack(anchor=tk.W, pady=(5, 0))
        
        # Buttons
        button_frame = ttk.Frame(main_frame)
        button_frame.pack(fill=tk.X, pady=(10, 0))
        
        def create_script():
            # Get configuration
            cpp_files = list(cpp_listbox.get(0, tk.END))
            py_files = list(py_listbox.get(0, tk.END))
            
            build_config = {
                'cpp_files': cpp_files,
                'python_files': py_files
            }
            
            # Create build script
            script_path = self.worktools.create_build_script(project_dir, build_config)
            
            messagebox.showinfo("Build Script Created", 
                              f"Build script created successfully!\n\nPath: {script_path}")
            dialog.destroy()
        
        ttk.Button(button_frame, text="Create Build Script", command=create_script).pack(side=tk.LEFT, padx=5)
        ttk.Button(button_frame, text="Cancel", command=dialog.destroy).pack(side=tk.LEFT, padx=5)
    
    def run_build_script(self):
        """Run build script"""
        script_path = filedialog.askopenfilename(
            title="Select build script to run",
            filetypes=[("Python files", "*.py"), ("All files", "*.*")]
        )
        
        if not script_path:
            return
        
        self.run_script_async(script_path, "python")
    
    def show_project_settings(self):
        """Show project settings dialog"""
        messagebox.showinfo("Project Settings", "Project settings dialog not implemented yet.")
    
    def show_system_info(self):
        """Show system information"""
        info = self.worktools.get_system_info()
        
        # Create info window
        info_window = tk.Toplevel(self.journal_ide.root)
        info_window.title("System Information")
        info_window.geometry("600x400")
        
        # Create text widget
        text_widget = tk.Text(info_window, wrap=tk.WORD)
        text_widget.pack(fill=tk.BOTH, expand=True, padx=10, pady=10)
        
        # Add system info
        text_widget.insert(tk.END, "🖥️ System Information\n")
        text_widget.insert(tk.END, "=" * 50 + "\n\n")
        text_widget.insert(tk.END, f"Platform: {info['platform']}\n")
        text_widget.insert(tk.END, f"Python: {info['python_version']}\n")
        text_widget.insert(tk.END, f"Working Directory: {info['working_directory']}\n")
        text_widget.insert(tk.END, f"Available Compilers: {len(info['available_compilers'])}\n\n")
        
        text_widget.insert(tk.END, "🔧 Available Compilers\n")
        text_widget.insert(tk.END, "=" * 50 + "\n")
        for name, details in info['available_compilers'].items():
            text_widget.insert(tk.END, f"{name}: {details['name']} - {details['version']}\n")
        
        text_widget.config(state=tk.DISABLED)
    
    def open_terminal(self):
        """Open terminal/command prompt"""
        if os.name == 'nt':
            os.system('start cmd')
        else:
            os.system('gnome-terminal &')
    
    def open_file_explorer(self):
        """Open file explorer"""
        if os.name == 'nt':
            os.system('explorer .')
        else:
            os.system('nautilus .')
    
    def show_about(self):
        """Show about dialog"""
        messagebox.showinfo("About Worktools", 
                          "External Worktools for Journal IDE\n\n"
                          "Provides compilation and execution capabilities for:\n"
                          "• C++ (GCC, Clang, MSVC)\n"
                          "• Java (javac, java)\n"
                          "• Python (python)\n"
                          "• Node.js (node)\n"
                          "• General command execution\n\n"
                          "Built for compatibility with Journal IDE")
    
    def setup_journal_interface(self, parent_frame):
        """Setup journal interface for n0mn0m IDE integration"""
        # Create journal components in the provided frame
        journal_label = ttk.Label(parent_frame, text="📖 Journal System", font=("Arial", 14, "bold"))
        journal_label.pack(pady=10)
        
        # Create entry list
        list_frame = ttk.Frame(parent_frame)
        list_frame.pack(fill=tk.BOTH, expand=True, padx=10, pady=5)
        
        self.entries_listbox = tk.Listbox(list_frame, height=10)
        self.entries_listbox.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
        
        # Scrollbar for listbox
        scrollbar = ttk.Scrollbar(list_frame, orient=tk.VERTICAL, command=self.entries_listbox.yview)
        scrollbar.pack(side=tk.RIGHT, fill=tk.Y)
        self.entries_listbox.config(yscrollcommand=scrollbar.set)
        
        # Add some sample entries
        sample_entries = [
            "📝 Daily Journal Entry",
            "💭 Thoughts and Ideas",
            "📚 Learning Notes",
            "🎯 Goals and Plans",
            "📊 Progress Report"
        ]
        
        for entry in sample_entries:
            self.entries_listbox.insert(tk.END, entry)
        
        # Buttons frame
        buttons_frame = ttk.Frame(parent_frame)
        buttons_frame.pack(fill=tk.X, padx=10, pady=5)
        
        ttk.Button(buttons_frame, text="📝 New Entry", command=self.new_entry).pack(side=tk.LEFT, padx=5)
        ttk.Button(buttons_frame, text="🔍 Search", command=self.search_entries).pack(side=tk.LEFT, padx=5)
        ttk.Button(buttons_frame, text="💾 Export", command=self.export_journal).pack(side=tk.LEFT, padx=5)
        ttk.Button(buttons_frame, text="📥 Import", command=self.import_journal).pack(side=tk.LEFT, padx=5)
        
        print("📖 Journal interface setup complete")
    
    def new_entry(self):
        """Create new journal entry"""
        messagebox.showinfo("New Entry", "📝 New journal entry created!\n\nThis will open the journal editor.")
    
    def search_entries(self):
        """Search journal entries"""
        messagebox.showinfo("Search Entries", "🔍 Search functionality will be implemented here.")
    
    def export_journal(self):
        """Export journal"""
        messagebox.showinfo("Export Journal", "💾 Journal export functionality will be implemented here.")
    
    def import_journal(self):
        """Import journal"""
        messagebox.showinfo("Import Journal", "📥 Journal import functionality will be implemented here.")

def integrate_with_journal_ide(journal_ide):
    """Integrate worktools with Journal IDE"""
    integration = JournalWorktoolsIntegration(journal_ide)
    
    # Add worktools menu
    if hasattr(journal_ide, 'menu_bar'):
        integration.create_worktools_menu(journal_ide.menu_bar)
    
    # Add worktools toolbar
    if hasattr(journal_ide, 'toolbar_frame'):
        worktools_toolbar = integration.create_worktools_toolbar(journal_ide.toolbar_frame)
        worktools_toolbar.pack(fill=tk.X, pady=2)
    
    return integration

def integrate_journal_with_ide(ide_instance):
    """Integrate journal functionality with n0mn0m IDE"""
    
    # Add journal tab to the IDE
    journal_frame = ttk.Frame(ide_instance.notebook)
    ide_instance.notebook.add(journal_frame, text="📖 Journal")
    
    # Create journal components
    journal_integration = JournalWorktoolsIntegration(ide_instance)
    journal_integration.setup_journal_interface(journal_frame)
    
    # Add journal menu
    journal_menu = tk.Menu(ide_instance.menubar, tearoff=0)
    ide_instance.menubar.add_cascade(label="📖 Journal", menu=journal_menu)
    
    journal_menu.add_command(label="New Entry", command=lambda: journal_integration.new_entry())
    journal_menu.add_command(label="Search Entries", command=lambda: journal_integration.search_entries())
    journal_menu.add_separator()
    journal_menu.add_command(label="Export Journal", command=lambda: journal_integration.export_journal())
    journal_menu.add_command(label="Import Journal", command=lambda: journal_integration.import_journal())
    
    # Store reference in IDE
    ide_instance.journal_integration = journal_integration
    
    print("📖 Journal system integrated with n0mn0m IDE!")