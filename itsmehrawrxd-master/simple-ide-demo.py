#!/usr/bin/env python3
"""
Simple Console IDE Demo - Actually Visible!
No GUI dependencies, just pure console interaction
"""

import os
import sys
import json
from pathlib import Path

class SimpleConsoleIDE:
    def __init__(self):
        self.current_file = None
        self.content = ""
        self.project_files = []
        self.running = True
        
    def show_banner(self):
        print("=" * 60)
        print(" n0mn0m Simple Console IDE")
        print("=" * 60)
        print("Commands:")
        print("  new    - Create new file")
        print("  open   - Open file") 
        print("  save   - Save current file")
        print("  list   - List project files")
        print("  edit   - Edit current file (simple)")
        print("  run    - Execute current file")
        print("  help   - Show this help")
        print("  quit   - Exit IDE")
        print("=" * 60)
        
    def run(self):
        self.show_banner()
        
        while self.running:
            try:
                if self.current_file:
                    prompt = f"[{os.path.basename(self.current_file)}]> "
                else:
                    prompt = "[no file]> "
                    
                command = input(prompt).strip().lower()
                
                if command == "quit" or command == "exit":
                    self.running = False
                elif command == "help":
                    self.show_banner()
                elif command == "new":
                    self.new_file()
                elif command == "open":
                    self.open_file()
                elif command == "save":
                    self.save_file()
                elif command == "list":
                    self.list_files()
                elif command == "edit":
                    self.edit_file()
                elif command == "run":
                    self.run_file()
                elif command == "status":
                    self.show_status()
                else:
                    print(f"Unknown command: {command}. Type 'help' for commands.")
                    
            except KeyboardInterrupt:
                print("\n\n IDE interrupted. Goodbye!")
                self.running = False
            except EOFError:
                print("\n IDE session ended. Goodbye!")
                self.running = False
                
    def new_file(self):
        filename = input("Enter filename: ").strip()
        if filename:
            self.current_file = filename
            self.content = ""
            print(f" Created new file: {filename}")
        
    def open_file(self):
        filename = input("Enter filename to open: ").strip()
        try:
            with open(filename, 'r') as f:
                self.content = f.read()
                self.current_file = filename
                print(f" Opened: {filename} ({len(self.content)} chars)")
                print("Preview (first 200 chars):")
                print("-" * 40)
                print(self.content[:200] + ("..." if len(self.content) > 200 else ""))
                print("-" * 40)
        except FileNotFoundError:
            print(f" File not found: {filename}")
        except Exception as e:
            print(f" Error opening file: {e}")
            
    def save_file(self):
        if not self.current_file:
            print(" No file to save. Use 'new' first.")
            return
            
        try:
            with open(self.current_file, 'w') as f:
                f.write(self.content)
            print(f" Saved: {self.current_file}")
        except Exception as e:
            print(f" Error saving file: {e}")
            
    def list_files(self):
        print("\n Current directory files:")
        files = [f for f in os.listdir('.') if os.path.isfile(f)]
        for i, file in enumerate(files[:20], 1):  # Show first 20
            marker = " " if file == os.path.basename(self.current_file or "") else ""
            print(f"  {i:2d}. {file}{marker}")
        if len(files) > 20:
            print(f"     ... and {len(files) - 20} more files")
            
    def edit_file(self):
        if not self.current_file:
            print(" No file to edit. Use 'new' or 'open' first.")
            return
            
        print(f"\n Editing: {self.current_file}")
        print("Current content:")
        print("-" * 40)
        print(self.content)
        print("-" * 40)
        print("Enter new content (press Ctrl+D when done, or Ctrl+C to cancel):")
        
        try:
            lines = []
            while True:
                try:
                    line = input()
                    lines.append(line)
                except EOFError:
                    break
            self.content = '\n'.join(lines)
            print(f" Content updated ({len(self.content)} chars)")
        except KeyboardInterrupt:
            print("\n Edit cancelled")
            
    def run_file(self):
        if not self.current_file:
            print(" No file to run. Use 'open' first.")
            return
            
        print(f" Running: {self.current_file}")
        try:
            if self.current_file.endswith('.py'):
                os.system(f"python {self.current_file}")
            elif self.current_file.endswith('.js'):
                os.system(f"node {self.current_file}")
            elif self.current_file.endswith(('.sh', '.bat')):
                os.system(self.current_file)
            else:
                print(f" File content ({len(self.content)} chars):")
                print("-" * 40)
                print(self.content)
                print("-" * 40)
        except Exception as e:
            print(f" Error running file: {e}")
            
    def show_status(self):
        print(f"\n IDE Status:")
        print(f"  Current file: {self.current_file or 'None'}")
        print(f"  Content size: {len(self.content)} characters")
        print(f"  Working dir:  {os.getcwd()}")
        print(f"  Python ver:   {sys.version}")

if __name__ == "__main__":
    ide = SimpleConsoleIDE()
    ide.run()
