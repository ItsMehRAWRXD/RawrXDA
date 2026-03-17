#!/usr/bin/env python3
"""
RawrXD Backend Server - Python Reference Implementation
========================================================
Implements the same API contract as the MASM middleware:
  GET  /models  -> Returns dynamically scanned model list
  POST /ask     -> Returns AI response
  GET  /health  -> Returns server status
  POST /scan    -> Force rescan of model directories
  GET  /config  -> View configuration
  OPTIONS /*    -> CORS preflight

Zero Ollama dependency - scans directories for GGUF files directly.
"""

import http.server
import json
import os
import glob
import time
from urllib.parse import urlparse, parse_qs

PORT = 8080

# Model directories to scan (same as MASM middleware)
MODEL_DIRS = [
    r"D:\OllamaModels",
    r"D:\models", 
    r"C:\models",
    r"E:\models",
]

# Model cache
cached_models = []
models_last_scan = 0
MODEL_CACHE_TTL = 30  # seconds

def format_file_size(size_bytes):
    """Format bytes to human readable string"""
    if size_bytes >= 1073741824:
        return f"{size_bytes // 1073741824} GB"
    elif size_bytes >= 1048576:
        return f"{size_bytes // 1048576} MB"
    else:
        return f"{size_bytes // 1024} KB"

def load_ollama_manifests(base_dir):
    """Load Ollama manifest files to map blob hashes to model names"""
    manifest_map = {}
    manifests_dir = os.path.join(base_dir, "manifests", "registry.ollama.ai")
    
    if os.path.exists(manifests_dir):
        for root, dirs, files in os.walk(manifests_dir):
            for filename in files:
                manifest_path = os.path.join(root, filename)
                try:
                    with open(manifest_path, 'r', encoding='utf-8') as f:
                        manifest = json.load(f)
                        
                        if not isinstance(manifest, dict):
                            continue
                        
                        # Extract model name from path (namespace/model/tag)
                        rel_path = os.path.relpath(manifest_path, manifests_dir)
                        path_parts = rel_path.replace('\\', '/').split('/')
                        
                        if len(path_parts) >= 2:
                            namespace = path_parts[0]
                            model_name = path_parts[1]
                            tag = path_parts[2] if len(path_parts) > 2 else 'latest'
                            display_name = f"{namespace}/{model_name}:{tag}"
                            
                            # Map all layers to this model name
                            if 'layers' in manifest and isinstance(manifest['layers'], list):
                                for layer in manifest['layers']:
                                    if isinstance(layer, dict) and 'digest' in layer:
                                        blob_hash = layer['digest'].replace('sha256:', 'sha256-')
                                        manifest_map[blob_hash] = display_name
                            
                            # Also map config digest
                            if 'config' in manifest and isinstance(manifest['config'], dict):
                                if 'digest' in manifest['config']:
                                    blob_hash = manifest['config']['digest'].replace('sha256:', 'sha256-')
                                    manifest_map[blob_hash] = display_name
                                
                except (json.JSONDecodeError, OSError, KeyError, TypeError, AttributeError):
                    pass
    
    return manifest_map

def scan_model_directories():
    """Scan all model directories recursively for GGUF, BLOB, BIN, SafeTensors, and Ollama blobs"""
    global cached_models, models_last_scan
    
    models = []
    file_patterns = ["*.gguf", "*.blob", "*.bin", "*.safetensors"]
    
    # Load Ollama manifests first
    ollama_manifests = {}
    for dir_path in MODEL_DIRS:
        if os.path.exists(dir_path):
            manifests = load_ollama_manifests(dir_path)
            ollama_manifests.update(manifests)
    
    for dir_path in MODEL_DIRS:
        if os.path.exists(dir_path):
            # Scan recursively using os.walk
            for root, dirs, files in os.walk(dir_path):
                for filename in files:
                    filepath = os.path.join(root, filename)
                    
                    # Check if matches pattern or is an Ollama blob (sha256-* files)
                    ext = filename.split('.')[-1].lower() if '.' in filename else ''
                    is_ollama_blob = filename.startswith('sha256-') and 'blobs' in root.lower()
                    
                    if any(filename.lower().endswith(p.replace('*', '')) for p in file_patterns) or is_ollama_blob:
                        try:
                            size = os.path.getsize(filepath)
                            
                            # Determine type and display name
                            if is_ollama_blob:
                                model_type = 'ollama'
                                # Look up friendly name from manifest
                                display_name = ollama_manifests.get(filename, filename[:24] + '...')
                            else:
                                model_type = ext
                                display_name = filename
                            
                            models.append({
                                'name': display_name,
                                'path': filepath,
                                'type': model_type,
                                'size': format_file_size(size)
                            })
                        except OSError:
                            pass
    
    cached_models = models
    models_last_scan = time.time()
    print(f"[Scan] Found {len(models)} models (GGUF, BLOB, BIN, SafeTensors, Ollama) across {len(MODEL_DIRS)} directories (recursive)")
    return models

def get_models():
    """Get models from cache or scan if expired"""
    global cached_models, models_last_scan
    
    if time.time() - models_last_scan > MODEL_CACHE_TTL:
        return scan_model_directories()
    return cached_models


class RawrXDHandler(http.server.BaseHTTPRequestHandler):
    
    def log_message(self, format, *args):
        print(f"[{self.log_date_time_string()}] {format % args}")
    
    def send_cors_headers(self):
        self.send_header('Access-Control-Allow-Origin', '*')
        self.send_header('Access-Control-Allow-Methods', 'GET, POST, OPTIONS')
        self.send_header('Access-Control-Allow-Headers', 'Content-Type, Accept')
    
    def send_json_response(self, data, status=200):
        response = json.dumps(data).encode('utf-8')
        self.send_response(status)
        self.send_header('Content-Type', 'application/json')
        self.send_header('Content-Length', len(response))
        self.send_cors_headers()
        self.send_header('Connection', 'close')
        self.end_headers()
        self.wfile.write(response)
    
    def do_OPTIONS(self):
        """Handle CORS preflight"""
        self.send_response(200)
        self.send_cors_headers()
        self.send_header('Access-Control-Max-Age', '86400')
        self.send_header('Content-Length', '0')
        self.end_headers()
    
    def do_GET(self):
        parsed = urlparse(self.path)
        path = parsed.path
        
        if path == '/models':
            self.handle_get_models()
        elif path == '/health':
            self.handle_get_health()
        elif path == '/config':
            self.handle_get_config()
        else:
            self.send_json_response({'error': 'Not Found'}, 404)
    
    def do_POST(self):
        parsed = urlparse(self.path)
        path = parsed.path
        
        if path == '/ask':
            self.handle_post_ask()
        elif path == '/scan':
            self.handle_post_scan()
        else:
            self.send_json_response({'error': 'Not Found'}, 404)
    
    def handle_get_models(self):
        """Return list of dynamically scanned models"""
        models = get_models()
        self.send_json_response({'models': models})
    
    def handle_get_health(self):
        """Return server health status"""
        models = get_models()
        self.send_json_response({
            'status': 'online',
            'port': PORT,
            'models': len(models),
            'version': '3.0',
            'backend': 'python-reference'
        })
    
    def handle_get_config(self):
        """Return configuration"""
        self.send_json_response({
            'model_dirs': MODEL_DIRS,
            'port': PORT,
            'max_models': 512,
            'cache_ttl': MODEL_CACHE_TTL
        })
    
    def handle_post_scan(self):
        """Force rescan of model directories"""
        models = scan_model_directories()
        self.send_json_response({'models': models})
    
    def handle_post_ask(self):
        """Handle /ask endpoint - parse question and return response"""
        content_length = int(self.headers.get('Content-Length', 0))
        body = self.rfile.read(content_length).decode('utf-8') if content_length > 0 else ''
        
        try:
            data = json.loads(body) if body else {}
        except json.JSONDecodeError:
            data = {}
        
        question = data.get('question', '').lower()
        model = data.get('model', 'default')
        language = data.get('language', 'English')
        
        # Generate response based on keywords
        models = get_models()
        
        if 'hello' in question or 'hi' in question:
            answer = "Hello! I'm the RawrXD Middleware - a pure assembly x64 service (Python reference). I connect your IDE to local AI models. No cloud, no Ollama required!"
        elif 'swarm' in question:
            answer = "**Swarm Mode**: Deploy up to 40 parallel AI agents. Configure via the GUI Swarm Panel or POST to /swarm with {agents: N, model: 'name', task: '...'}."
        elif 'model' in question:
            answer = f"**Model Management**: I dynamically scan your model directories for GGUF files. Currently tracking {len(models)} models. Use GET /models or POST /scan to refresh."
        elif 'bench' in question:
            answer = "**Benchmarks**: RX 7800 XT achieves ~3,158 tokens/sec on 3.8B models. POST to /benchmark with {model, iterations} for custom tests."
        elif 'help' in question:
            answer = """**RawrXD Middleware Help**

- GET /models - List all GGUF models (dynamically scanned)
- POST /ask - Send a question
- GET /health - Service status
- POST /scan - Force rescan model directories
- GET /config - View configuration

Model directories: """ + ', '.join(MODEL_DIRS)
        else:
            answer = f"RawrXD Middleware operational. {len(models)} models available. Ask about 'swarm', 'model', 'benchmark', or 'help'!"
        
        self.send_json_response({'answer': answer})


class ThreadedHTTPServer(http.server.ThreadingHTTPServer):
    """Threaded HTTP server to handle concurrent requests"""
    allow_reuse_address = True
    daemon_threads = True


def main():
    print("""
================================================
  RawrXD Middleware v3.0 (Python Reference)
  Zero Dependencies - Pure Local Models
  [THREADED MODE ENABLED]
================================================
""")
    
    # Initial scan
    scan_model_directories()
    
    print(f"\nModel directories:")
    for d in MODEL_DIRS:
        exists = "✓" if os.path.exists(d) else "✗"
        print(f"  [{exists}] {d}")
    
    print(f"\nEndpoints:")
    print(f"  GET  /models  - List scanned GGUF models")
    print(f"  POST /ask     - Send a question")
    print(f"  GET  /health  - Service status")
    print(f"  POST /scan    - Rescan directories")
    print(f"  GET  /config  - View configuration")
    print(f"\n================================================")
    
    server = ThreadedHTTPServer(('', PORT), RawrXDHandler)
    print(f"[+] Server listening on http://localhost:{PORT}")
    print(f"[*] Press Ctrl+C to stop\n")
    
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\n[!] Shutdown signal received, exiting...")
        server.shutdown()


if __name__ == '__main__':
    main()
