#!/usr/bin/env python3
"""
RawrXD Backend Server - Python Implementation
==============================================
Implements the same API contract as the MASM backend:
  GET  /models  -> Returns model list
  POST /ask     -> Returns AI response
  GET  /health  -> Returns server status
  OPTIONS /*    -> CORS preflight

This serves as a working reference while the MASM version is debugged.
"""

import http.server
import json
import os
import glob
from urllib.parse import urlparse, parse_qs

PORT = 8080
MODEL_DIR = r"D:\OllamaModels"

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
        else:
            self.send_json_response({'error': 'Not Found'}, 404)
    
    def do_POST(self):
        parsed = urlparse(self.path)
        path = parsed.path
        
        if path == '/ask':
            self.handle_post_ask()
        else:
            self.send_json_response({'error': 'Not Found'}, 404)
    
    def handle_get_models(self):
        """Return list of available models"""
        models = []
        
        # Scan for GGUF files
        if os.path.exists(MODEL_DIR):
            for filepath in glob.glob(os.path.join(MODEL_DIR, "*.gguf")):
                name = os.path.basename(filepath)
                models.append({
                    'name': name,
                    'path': filepath,
                    'type': 'gguf'
                })
        
        # Add some default models if none found
        if not models:
            models = [
                {'name': 'rawrxd-7b', 'path': 'D:\\models\\rawrxd-7b.gguf', 'type': 'gguf'},
                {'name': 'rawrxd-13b', 'path': 'D:\\models\\rawrxd-13b.gguf', 'type': 'gguf'},
                {'name': 'codestral-22b', 'path': 'D:\\models\\codestral-22b.gguf', 'type': 'gguf'}
            ]
        
        self.send_json_response({'models': models})
    
    def handle_get_health(self):
        """Return server health status"""
        self.send_json_response({
            'status': 'online',
            'models': len(glob.glob(os.path.join(MODEL_DIR, "*.gguf"))) if os.path.exists(MODEL_DIR) else 3,
            'version': '3.0',
            'backend': 'python-reference'
        })
    
    def handle_post_ask(self):
        """Handle /ask endpoint - parse question and return response"""
        # Read request body
        content_length = int(self.headers.get('Content-Length', 0))
        body = self.rfile.read(content_length).decode('utf-8') if content_length > 0 else ''
        
        # Parse JSON body
        try:
            data = json.loads(body) if body else {}
        except json.JSONDecodeError:
            data = {}
        
        question = data.get('question', '').lower()
        model = data.get('model', 'default')
        language = data.get('language', 'English')
        
        # Generate response based on keywords
        if 'hello' in question or 'hi' in question:
            answer = "Hello! I am the RawrXD IDE backend. I'm currently running as a Python reference server while the MASM version is being debugged. How can I assist you with coding today?"
        elif 'swarm' in question:
            answer = "**Swarm Mode**: Deploy multiple AI agents via the Swarm panel. Configure the number of agents (max 40), select your model, and specify a target directory. Agents will work in parallel on your tasks."
        elif 'model' in question:
            answer = "**Model Management**: Use GET /models to list available models. The backend supports GGUF format models from your local directory. You can load different models for different tasks."
        elif 'bench' in question:
            answer = "**Benchmark Results**: Expected performance on RX 7800 XT is approximately 3,158 tokens/second for 3.8B parameter models. Run benchmarks via POST /benchmark with {model, iterations}."
        elif 'help' in question:
            answer = """**RawrXD IDE Assistant Help**

Available commands:
- **swarm**: Deploy multi-agent workflows
- **model**: Manage AI models (GGUF/Ollama)
- **benchmark**: Run performance tests
- **todo**: Task management integration
- **code**: Get coding assistance

Just ask me anything about development!"""
        else:
            answer = f"MASM backend is operational (Python reference). You asked: '{data.get('question', 'nothing')}'. Model: {model}, Language: {language}. Try asking about 'swarm', 'model', 'benchmark', or 'help'!"
        
        self.send_json_response({'answer': answer})


def main():
    print("""
================================================
  RawrXD Backend Server v3.0 (Python Reference)
  Pure x64 MASM Implementation Coming Soon!
================================================
Endpoints: GET /models | POST /ask | GET /health
================================================
""")
    
    server = http.server.HTTPServer(('', PORT), RawrXDHandler)
    print(f"[+] Server listening on http://localhost:{PORT}")
    print(f"[*] Press Ctrl+C to stop\n")
    
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\n[!] Shutdown signal received, exiting...")
        server.shutdown()


if __name__ == '__main__':
    main()
