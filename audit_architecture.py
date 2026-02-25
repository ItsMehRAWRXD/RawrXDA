import os
import re
import json
from collections import defaultdict

def analyze_directory(root_dir):
    ipc_patterns = {
        'Named Pipes': r'CreateNamedPipe|ConnectNamedPipe|WaitNamedPipe|\\\\\\.\\pipe\\',
        'Sockets': r'socket\(|bind\(|listen\(|accept\(|connect\(|WSAStartup',
        'Shared Memory': r'CreateFileMapping|MapViewOfFile|OpenFileMapping',
        'COM / OLE': r'CoCreateInstance|CoInitialize|IOleClientSite|IDocHostUIHandler',
        'Window Messages': r'SendMessage|PostMessage|RegisterWindowMessage',
        'LSP / JSON-RPC': r'Content-Length:|jsonrpc|textDocument/'
    }
    
    arch_patterns = {
        'Plugin System': r'LoadLibrary|GetProcAddress|dlopen|dlsym|RegisterPlugin',
        'Event Driven': r'addEventListener|EventEmitter|SetEvent|WaitForSingleObject',
        'Client-Server': r'listen\(|accept\(|http_server|REST|GraphQL',
        'Multithreading': r'CreateThread|std::thread|pthread_create|_beginthreadex'
    }

    results = {
        'ipc': defaultdict(list),
        'arch': defaultdict(list),
        'components': defaultdict(int)
    }

    skip_dirs = {'.git', '.vscode', 'build', 'node_modules', '3rdparty', 'tests', 'examples', '.archived_orphans', 'history', 'out', 'dist', 'bin', 'obj', '.ultra_archived', 'build_output', 'build_temp', 'build_ide', 'build_native', 'build_qt_free', 'build_vs', 'build_ninja', 'build_bruteforce', 'build_clean', 'build_clean2', 'build_fix', 'build_fresh', 'build_ghost', 'build_gmake', 'build_gold', 'build_gui_3', 'build_ide_native', 'build_ide_ninja', 'build_inhouse', 'build_manual', 'build_mingw', 'build_msvc', 'build_nasm', 'build_new', 'build_prod', 'build_pure_cpp', 'build_test_parse', 'build_trash', 'build_universal', 'build_verify', 'build_win32_gui_test', 'build-arm64', 'build2', 'build2_clean'}
    valid_exts = {'.cpp', '.h', '.c', '.asm', '.js', '.ts', '.py', '.cs'}

    for dirpath, dirnames, filenames in os.walk(root_dir):
        dirnames[:] = [d for d in dirnames if d not in skip_dirs]
        
        for filename in filenames:
            ext = os.path.splitext(filename)[1].lower()
            if ext not in valid_exts:
                continue
                
            filepath = os.path.join(dirpath, filename)
            rel_path = os.path.relpath(filepath, root_dir)
            
            # Component counting
            component = rel_path.split(os.sep)[0]
            if component == 'src' and len(rel_path.split(os.sep)) > 1:
                component = 'src/' + rel_path.split(os.sep)[1]
            results['components'][component] += 1

            try:
                with open(filepath, 'r', encoding='utf-8', errors='ignore') as f:
                    content = f.read()
                    
                    for category, pattern in ipc_patterns.items():
                        if re.search(pattern, content, re.IGNORECASE):
                            results['ipc'][category].append(rel_path)
                            
                    for category, pattern in arch_patterns.items():
                        if re.search(pattern, content, re.IGNORECASE):
                            results['arch'][category].append(rel_path)
            except Exception as e:
                pass

    return results

if __name__ == '__main__':
    print("Starting architecture audit of D:\\RawrXD...")
    results = analyze_directory('D:\\RawrXD')
    
    print("\n=== COMPONENT SIZE (File Count) ===")
    for comp, count in sorted(results['components'].items(), key=lambda x: x[1], reverse=True)[:15]:
        print(f"{comp}: {count} files")
        
    print("\n=== IPC MECHANISMS ===")
    for category, files in results['ipc'].items():
        print(f"{category}: Found in {len(files)} files")
        for f in files[:3]:
            print(f"  - {f}")
        if len(files) > 3:
            print(f"  - ... and {len(files)-3} more")
            
    print("\n=== ARCHITECTURAL PATTERNS ===")
    for category, files in results['arch'].items():
        print(f"{category}: Found in {len(files)} files")
        for f in files[:3]:
            print(f"  - {f}")
        if len(files) > 3:
            print(f"  - ... and {len(files)-3} more")
