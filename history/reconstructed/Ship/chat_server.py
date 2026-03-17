#!/usr/bin/env python3
"""
RawrXD Chat Server - HTTP API for AI Chat Integration
Auto-starts when chat panel is opened in RawrXD IDE
"""

import http.server
import json
import threading
import socketserver
import sys
import os
from pathlib import Path

# Global server instance
g_server = None
g_server_thread = None

class ChatRequestHandler(http.server.SimpleHTTPRequestHandler):
    """Custom HTTP handler for chat API endpoints"""
    
    def do_GET(self):
        """Handle GET requests"""
        if self.path == '/health':
            self.send_response(200)
            self.send_header('Content-type', 'application/json')
            self.end_headers()
            response = {'status': 'healthy', 'service': 'RawrXD Chat Server'}
            self.wfile.write(json.dumps(response).encode())
        elif self.path == '/api/status':
            self.send_response(200)
            self.send_header('Content-type', 'application/json')
            self.end_headers()
            response = {
                'server_running': True,
                'model_loaded': False,  # Will be updated when model integration is added
                'model_path': None
            }
            self.wfile.write(json.dumps(response).encode())
        else:
            self.send_response(404)
            self.end_headers()
    
    def do_POST(self):
        """Handle POST requests"""
        if self.path == '/api/generate':
            content_length = int(self.headers['Content-Length'])
            post_data = self.rfile.read(content_length)
            
            try:
                request_data = json.loads(post_data.decode('utf-8'))
                prompt = request_data.get('prompt', '')
                max_tokens = request_data.get('max_tokens', 128)
                temperature = request_data.get('temperature', 0.7)
                
                # For now, return a mock response
                # In production, this would call the actual inference engine
                response = {
                    'response': f'AI Response to: {prompt}',
                    'tokens_generated': min(len(prompt.split()), max_tokens),
                    'status': 'success'
                }
                
                self.send_response(200)
                self.send_header('Content-type', 'application/json')
                self.end_headers()
                self.wfile.write(json.dumps(response).encode())
                
            except Exception as e:
                self.send_response(500)
                self.send_header('Content-type', 'application/json')
                self.end_headers()
                error_response = {'error': str(e), 'status': 'error'}
                self.wfile.write(json.dumps(error_response).encode())
        
        elif self.path == '/api/chat':
            content_length = int(self.headers['Content-Length'])
            post_data = self.rfile.read(content_length)
            
            try:
                request_data = json.loads(post_data.decode('utf-8'))
                message = request_data.get('message', '')
                
                # Mock chat response
                response = {
                    'reply': f'Chat response to: {message}',
                    'status': 'success'
                }
                
                self.send_response(200)
                self.send_header('Content-type', 'application/json')
                self.end_headers()
                self.wfile.write(json.dumps(response).encode())
                
            except Exception as e:
                self.send_response(500)
                self.send_header('Content-type', 'application/json')
                self.end_headers()
                error_response = {'error': str(e), 'status': 'error'}
                self.wfile.write(json.dumps(error_response).encode())
        
        else:
            self.send_response(404)
            self.end_headers()
    
    def log_message(self, format, *args):
        """Override to customize logging"""
        print(f"[Chat Server] {format % args}")


def start_server(port=23959):
    """Start the HTTP server in a separate thread"""
    global g_server, g_server_thread
    
    try:
        # Create server
        handler = ChatRequestHandler
        g_server = socketserver.TCPServer(("", port), handler)
        
        # Start server in a separate thread
        g_server_thread = threading.Thread(target=g_server.serve_forever, daemon=True)
        g_server_thread.start()
        
        print(f"[Chat Server] Started on port {port}")
        print(f"[Chat Server] Health check: http://localhost:{port}/health")
        print(f"[Chat Server] API status: http://localhost:{port}/api/status")
        return True
        
    except Exception as e:
        print(f"[Chat Server] Failed to start: {e}")
        return False


def stop_server():
    """Stop the HTTP server"""
    global g_server, g_server_thread
    
    if g_server:
        g_server.shutdown()
        g_server.server_close()
        g_server = None
        g_server_thread = None
        print("[Chat Server] Stopped")


def is_server_running():
    """Check if server is running"""
    return g_server is not None


if __name__ == '__main__':
    import argparse
    
    parser = argparse.ArgumentParser(description='RawrXD Chat Server')
    parser.add_argument('--port', type=int, default=23959, help='Port to listen on')
    parser.add_argument('--stop', action='store_true', help='Stop running server')
    
    args = parser.parse_args()
    
    if args.stop:
        # This is a simple implementation - in production you'd use a PID file
        print("[Chat Server] Stop command received (not implemented for standalone mode)")
    else:
        print("[Chat Server] Starting...")
        if start_server(args.port):
            print("[Chat Server] Press Ctrl+C to stop")
            try:
                while True:
                    pass
            except KeyboardInterrupt:
                print("\n[Chat Server] Shutting down...")
                stop_server()
        else:
            sys.exit(1)