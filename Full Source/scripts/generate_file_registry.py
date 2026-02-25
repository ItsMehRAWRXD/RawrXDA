#!/usr/bin/env python3
"""Generate Win32 file registry from all source files"""
import os
from pathlib import Path

def main():
    # Scan all source files
    extensions = {'.cpp', '.h', '.hpp', '.asm', '.py', '.ps1', '.sh', '.c', '.cc'}
    files = []
    
    for root, dirs, filenames in os.walk('.'):
        # Skip build/git directories
        dirs[:] = [d for d in dirs if d not in {'.git', 'build', 'node_modules', '.vs'}]
        
        for filename in filenames:
            if any(filename.endswith(ext) for ext in extensions):
                filepath = os.path.join(root, filename)
                files.append(filepath.replace('\\', '/'))
    
    files.sort()
    print(f"// Found {len(files)} source files")
    
    # Generate C++ registration code
    print('// Auto-generated file registry')
    print('void FileRegistry::registerAllFiles() {')
    
    for filepath in files:
        safe_path = filepath.replace('\\', '\\\\').replace('"', '\\"')
        print(f'    registerFile("{safe_path}");')
    
    print(f'    s_logger.info("Registered {{}} files", {len(files)});')
    print('}')
    
    return len(files)

if __name__ == '__main__':
    count = main()
